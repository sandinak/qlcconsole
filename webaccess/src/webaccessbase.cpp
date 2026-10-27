/*
  Q Light Controller Plus
  webaccessbase.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QSettings>
#include <QTcpServer>
#include <QFileInfo>
#include <QUrl>
#include <QRegularExpression>

#include "webaccessbase.h"

#include "webaccessconfiguration.h"
#include "webaccesssimpledesk.h"
#include "webaccessnetwork.h"
#include "commonjscss.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "doc.h"
#include "inputpatch.h"
#include "audiocapture.h"
#include "audiorenderer.h"
#include "qhttpserver.h"
#include "qhttprequest.h"
#include "qhttpresponse.h"
#include "qhttpconnection.h"

#define DEFAULT_PORT_NUMBER    9999
#define AUTOSTART_PROJECT_NAME "autostart.qxw"

WebAccessBase::WebAccessBase(Doc *doc, VirtualConsole *vcInstance, SimpleDesk *sdInstance,
                             int portNumber, bool enableAuth, const QString &passwdFile,
                             const QHostAddress &bindAddress, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_vc(vcInstance)
    , m_sd(sdInstance)
    , m_auth(nullptr)
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    , m_netConfig(nullptr)
#endif
    , m_httpServer(nullptr)
    , m_pendingProjectLoaded(false)
{
    Q_ASSERT(m_doc != nullptr);
    Q_ASSERT(m_vc != nullptr);

    if (enableAuth)
    {
        m_auth = new WebAccessAuth(QString("QLC+ web access"));
        m_auth->loadPasswordsFile(passwdFile);
    }

    m_httpServer = new QHttpServer(this);
    connect(m_httpServer, SIGNAL(newRequest(QHttpRequest*, QHttpResponse*)),
            this, SLOT(slotHandleHTTPRequest(QHttpRequest*, QHttpResponse*)));
    connect(m_httpServer, SIGNAL(webSocketDataReady(QHttpConnection*,QString)),
            this, SLOT(slotHandleWebSocketRequest(QHttpConnection*,QString)));
    connect(m_httpServer, SIGNAL(webSocketConnectionClose(QHttpConnection*)),
            this, SLOT(slotHandleWebSocketClose(QHttpConnection*)));

    const QHostAddress address = bindAddress.isNull() ? defaultBindAddress() : bindAddress;
    if (m_httpServer->listen(address, portNumber ? portNumber : DEFAULT_PORT_NUMBER) == false)
    {
        qWarning().noquote() << "Web access could not listen on"
                             << address.toString() << "port" << (portNumber ? portNumber : DEFAULT_PORT_NUMBER);
    }
    else
    {
        qInfo().noquote() << "Web access:" << url();
        if (listenAddress().isLoopback() == false)
            qWarning().noquote() << "Web access listens on" << listenAddress().toString()
                                 << "-- reachable from the network."
                                 << (enableAuth ? "Users must log in (-wa)."
                                                : "Without -wa, visitors from the network can look but change nothing.");
    }

#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    m_netConfig = new WebAccessNetwork();
#endif

    connect(m_doc->masterTimer(), SIGNAL(functionStarted(quint32)),
            this, SLOT(slotFunctionStarted(quint32)));
    connect(m_doc->masterTimer(), SIGNAL(functionStopped(quint32)),
            this, SLOT(slotFunctionStopped(quint32)));
}

WebAccessBase::~WebAccessBase()
{
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    delete m_netConfig;
#endif
    /* Deleting a connection closes its socket, whose disconnected() reached
       slotHandleWebSocketClose -- which deleteLater()'d the same connection
       and edited this list mid-loop: a crash on every quit with a browser
       open. Take the list first and cut each connection off from us. */
    const QList<QHttpConnection *> conns = m_webSocketsList;
    m_webSocketsList.clear();
    for (QHttpConnection *conn : conns)
    {
        conn->disconnect();         // its close no longer reaches us
        delete static_cast<WebAccessUser *>(conn->userData);
        conn->userData = nullptr;
        delete conn;
    }

    delete m_auth;
}

QHostAddress WebAccessBase::defaultBindAddress()
{
    return QHostAddress(QHostAddress::LocalHost);
}

QHostAddress WebAccessBase::listenAddress() const
{
    QTcpServer *srv = m_httpServer != nullptr ? m_httpServer->findChild<QTcpServer *>() : nullptr;
    return srv != nullptr && srv->isListening() ? srv->serverAddress() : QHostAddress();
}

quint16 WebAccessBase::listenPort() const
{
    QTcpServer *srv = m_httpServer != nullptr ? m_httpServer->findChild<QTcpServer *>() : nullptr;
    return srv != nullptr && srv->isListening() ? srv->serverPort() : 0;
}

QString WebAccessBase::url() const
{
    QHostAddress addr = listenAddress();
    QString host;
    if (addr == QHostAddress(QHostAddress::AnyIPv4) || addr == QHostAddress(QHostAddress::Any)
        || addr == QHostAddress(QHostAddress::AnyIPv6))
        host = QStringLiteral("<this computer's address>");
    else if (addr.protocol() == QAbstractSocket::IPv6Protocol)
        host = QStringLiteral("[%1]").arg(addr.toString());
    else
        host = addr.toString();
    return QStringLiteral("http://%1:%2/").arg(host).arg(listenPort());
}

QString WebAccessBase::htmlEscaped(const QString &text)
{
    return QString(text).toHtmlEscaped().replace(QLatin1Char('\''), QStringLiteral("&#39;"));
}

QString WebAccessBase::jsString(const QString &text)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : text)
    {
        const ushort u = c.unicode();
        if (u == '\\')
            out += QStringLiteral("\\\\");
        else if (u < 0x20 || u == '"' || u == '\'' || u == '<' || u == '>' || u == '&'
                 || u == 0x2028 || u == 0x2029)
            out += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
        else
            out += c;
    }
    out += QLatin1Char('"');
    return out;
}

QString WebAccessBase::sanitizedFixtureFileName(const QString &name)
{
    // Only what follows the last separator of either kind: a name never
    // picks the folder it lands in.
    QString base = name;
    const int cut = qMax(base.lastIndexOf(QLatin1Char('/')), base.lastIndexOf(QLatin1Char('\\')));
    if (cut >= 0)
        base = base.mid(cut + 1);
    base = base.trimmed();
    if (base.isEmpty() || base.startsWith(QLatin1Char('.')) || base.contains(QLatin1Char(':'))
        || base.contains(QChar(0)) || base.endsWith(QLatin1String(".qxf"), Qt::CaseInsensitive) == false
        || base.size() > 200)
        return QString();
    return base;
}

bool WebAccessBase::sendFile(QHttpResponse *response, QString filename, QString contentType) const
{
    QFile resFile(filename);
#if defined(WIN32) || defined(Q_OS_WIN)
    if (!resFile.exists())
    {
        filename.remove(0, 1);
        filename.replace("//", ":\\");
        filename.replace('/', '\\');
        resFile.setFileName(filename);
    }
#endif
    if (resFile.open(QIODevice::ReadOnly))
    {
        QByteArray resContent = resFile.readAll();
        resFile.close();

        response->setHeader("Content-Type", contentType);
        response->setHeader("Content-Length", QString::number(resContent.size()));
        response->writeHead(200);
        response->end(resContent);

        return true;
    }
    else
        qDebug() << "Failed to open file:" << filename;

    return false;
}

void WebAccessBase::sendWebSocketMessage(const QString &message)
{
    foreach (QHttpConnection *conn, m_webSocketsList)
        conn->webSocketWrite(message);
}

QString WebAccessBase::webFilePath(const QString &relativePath) const
{
    QString basePath = QLCFile::systemDirectory(WEBFILESDIR).path();
    return QString("%1%2%3").arg(basePath).arg(QDir::separator()).arg(relativePath);
}

bool WebAccessBase::servePng(QHttpResponse *resp, const QString &reqUrl) const
{
    if (reqUrl.contains(QLatin1String("..")))
        return false;
    QString localFilePath = QString(":%1").arg(reqUrl);
    QFile resFile(localFilePath);
    if (!resFile.exists())
    {
        // A picture of the show (a widget's background, a button's icon), by
        // its path on this computer -- only those, not any file on the disk.
        localFilePath = reqUrl;
        resFile.setFileName(localFilePath);
        if (!resFile.exists() || !isShowImage(reqUrl))
            localFilePath = webFilePath(reqUrl.mid(1));
    }

    return sendFile(resp, localFilePath, "image/png");
}

bool WebAccessBase::serveWebFile(QHttpResponse *resp, const QString &reqUrl, const QString &contentType) const
{
    // The web folder's own files only: no way out of it with ../
    const QString rel = reqUrl.mid(1);
    if (rel.contains(QLatin1String("..")) || rel.contains(QLatin1Char('\\')) || QFileInfo(rel).isAbsolute())
        return false;
    return sendFile(resp, webFilePath(rel), contentType);
}

bool WebAccessBase::isShowImage(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

bool WebAccessBase::authenticateRequest(QHttpRequest *req, QHttpResponse *resp, WebAccessUser &user)
{
    const bool local = isLocalRequest(req);

    if (!m_auth)
    {
        // -w alone: this computer's operator does everything; a visitor
        // from the network may look, not touch (start with -wa for that).
        user = WebAccessUser(local ? SUPER_ADMIN_LEVEL : LOGGED_IN_LEVEL);
        return true;
    }

    if (!m_auth->hasAtLeastOneAdmin())
    {
        // -wa before anyone has an account: only this computer, and only
        // to create the first admin (Configuration > Users).
        if (local)
        {
            user = WebAccessUser(LOGGED_IN_LEVEL);
            user.setupOnly = true;
            return true;
        }
        m_auth->sendUnauthorizedResponse(resp);
        return false;
    }

    user = m_auth->authenticateRequest(req, resp);
    if (user.level < LOGGED_IN_LEVEL)
    {
        m_auth->sendUnauthorizedResponse(resp);
        return false;
    }

    return true;
}

static bool isLoopbackHostName(QString host)
{
    host = host.trimmed().toLower();
    if (host.startsWith(QLatin1Char('[')))
    {
        const int end = host.indexOf(QLatin1Char(']'));
        host = host.mid(1, end > 0 ? end - 1 : -1);
    }
    else if (host.count(QLatin1Char(':')) == 1)
    {
        host = host.left(host.indexOf(QLatin1Char(':')));      // host:port
    }
    if (host == QLatin1String("localhost") || host.endsWith(QLatin1String(".localhost")))
        return true;
    QHostAddress addr;
    return addr.setAddress(host) && addr.isLoopback();
}

bool WebAccessBase::isLocalRequest(const QHttpRequest *req) const
{
    if (req == nullptr || m_trustLoopback == false)
        return false;
    QHostAddress peer(req->remoteAddress());
    bool mapped = false;
    const quint32 v4 = peer.toIPv4Address(&mapped);
    if (mapped)
        peer = QHostAddress(v4);
    if (peer.isLoopback() == false)
        return false;
    // A page on some other site whose name resolves to 127.0.0.1 (DNS
    // rebinding) comes from loopback too, but under that site's name.
    const QString host = req->header(QStringLiteral("host"));
    return host.isEmpty() || isLoopbackHostName(host);
}

bool WebAccessBase::originAllowed(const QHttpRequest *req) const
{
    if (req == nullptr)
        return false;
    const QString origin = req->header(QStringLiteral("origin")).trimmed();
    // No Origin: not a browser page on another site (a script, an app).
    if (origin.isEmpty())
        return true;
    const QUrl o(origin);
    if (o.isValid() == false || o.host().isEmpty())
        return false;           // "null" (a sandboxed page, a file://) included
    if (isLoopbackHostName(o.host()))
        return true;
    // Same origin: the page was served by this console, under the name the
    // browser asked for.
    const QString host = req->header(QStringLiteral("host")).trimmed().toLower();
    const int defaultPort = o.scheme() == QLatin1String("https") ? 443 : 80;
    const QString originHostPort = o.port(defaultPort) == defaultPort
            ? o.host().toLower() : QStringLiteral("%1:%2").arg(o.host().toLower()).arg(o.port());
    const QString originHostPortExplicit = QStringLiteral("%1:%2").arg(o.host().toLower()).arg(o.port(defaultPort));
    return host == originHostPort || host == originHostPortExplicit;
}

void WebAccessBase::sendForbidden(QHttpResponse *resp, const QString &why) const
{
    if (resp == nullptr)
        return;
    const QByteArray text = QStringLiteral("403 Forbidden\n%1\n").arg(why).toUtf8();
    resp->setHeader("Content-Type", "text/plain; charset=utf-8");
    resp->setHeader("Content-Length", QString::number(text.size()));
    resp->writeHead(403);
    resp->end(text);
}

bool WebAccessBase::permits(const WebAccessUser *user, WebAccessUserLevel level) const
{
    if (user == nullptr)
        return false;
    if (user->setupOnly)
        return level <= LOGGED_IN_LEVEL;
    return user->level >= level;
}

bool WebAccessBase::acceptWebSocket(QHttpResponse *resp, const WebAccessUser &user)
{
    if (!resp)
        return false;

    QHttpConnection *conn = resp->enableWebSocket();
    if (conn != nullptr)
    {
        conn->userData = new WebAccessUser(user);
        m_webSocketsList.append(conn);
        return true;
    }

    return false;
}

QByteArray WebAccessBase::extractProjectXml(const QHttpRequest *req) const
{
    if (req == nullptr)
        return QByteArray();

    QByteArray projectXML = req->body();
    projectXML.remove(0, projectXML.indexOf("\n\r\n") + 3);
    projectXML.truncate(projectXML.lastIndexOf("\n\r\n"));
    return projectXML;
}

void WebAccessBase::sendProjectLoadingResponse(QHttpResponse *resp) const
{
    if (resp == nullptr)
        return;

    QByteArray postReply =
            QString("<html><head>\n<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\n"
            "<script>\n" PROJECT_LOADED_JS
            "</script></head><body style=\"background-color: #45484d;\">"
            "<div style=\"position: absolute; width: 100%; height: 30px; top: 50%; background-color: #888888;"
            "text-align: center; font:bold 24px/1.2em sans-serif;\">"
            + tr("Loading project...") +
            "</div></body></html>").toUtf8();

    resp->setHeader("Content-Type", "text/html");
    resp->setHeader("Content-Length", QString::number(postReply.size()));
    resp->writeHead(200);
    resp->end(postReply);
}

void WebAccessBase::sendNotFound(QHttpResponse *resp) const
{
    if (resp == nullptr)
        return;

    resp->writeHead(404);
    resp->setHeader("Content-Type", "text/plain");
    resp->setHeader("Content-Length", "14");
    resp->end(QByteArray("404 Not found"));
}

void WebAccessBase::sendHtmlResponse(QHttpResponse *resp, const QString &content) const
{
    if (resp == nullptr)
        return;

    QByteArray contentArray = content.toUtf8();
    resp->setHeader("Content-Type", "text/html");
    resp->setHeader("Content-Length", QString::number(contentArray.size()));
    resp->writeHead(200);
    resp->end(contentArray);
}

bool WebAccessBase::requireAuthLevel(QHttpResponse *resp, const WebAccessUser &user, WebAccessUserLevel level) const
{
    if (permits(&user, level))
        return true;
    if (m_auth)
        m_auth->sendUnauthorizedResponse(resp);
    else
        sendForbidden(resp, tr("Web access lets this computer change the show. To change it "
                               "from another device, start qlcconsole with -wa and log in."));
    return false;
}

WebAccessBase::CommonRequestResult WebAccessBase::handleCommonHTTPRequest(QHttpRequest *req, QHttpResponse *resp,
                                                                          const WebAccessUser &user,
                                                                          const QString &reqUrl,
                                                                          QString &content)
{
    // A page on another site may not drive the console (CSWSH, CSRF)
    if ((reqUrl == "/qlcplusWS" || req->method() == QHttpRequest::HTTP_POST) && !originAllowed(req))
    {
        qWarning().noquote() << "Web access: refused a request from the page at"
                             << req->header(QStringLiteral("origin"));
        sendForbidden(resp, tr("A page on another site may not control this console."));
        return CommonRequestResult::Handled;
    }

    if (reqUrl == "/qlcplusWS")
    {
        if (acceptWebSocket(resp, user))
            return CommonRequestResult::Handled;
        return CommonRequestResult::ContentReady;
    }
    else if (reqUrl == "/loadProject")
    {
        if (!requireAuthLevel(resp, user, SUPER_ADMIN_LEVEL))
            return CommonRequestResult::Handled;
        QByteArray projectXML = extractProjectXml(req);

        qDebug() << "Workspace XML received. Content-Length:" << req->headers().value("content-length") << projectXML.size();
        sendProjectLoadingResponse(resp);

        m_pendingProjectLoaded = false;

        handleProjectLoad(projectXML);

        return CommonRequestResult::Handled;
    }
    else if (reqUrl == "/loadFixture")
    {
        if (!requireAuthLevel(resp, user, SUPER_ADMIN_LEVEL))
            return CommonRequestResult::Handled;
        QByteArray fixtureXML = req->body();
        int fnamePos = fixtureXML.indexOf("filename=") + 10;
        // the name the browser sent, reduced to a plain .qxf file name: it
        // never chooses the folder (../escaped.qxf wrote outside it)
        const QString fxName = sanitizedFixtureFileName(QString::fromUtf8(
                fixtureXML.mid(fnamePos, fixtureXML.indexOf("\"", fnamePos) - fnamePos)));
        if (fxName.isEmpty())
        {
            sendForbidden(resp, tr("A fixture definition is a .qxf file."));
            return CommonRequestResult::Handled;
        }

        fixtureXML.remove(0, fixtureXML.indexOf("\n\r\n") + 3);
        fixtureXML.truncate(fixtureXML.lastIndexOf("\n\r\n"));

        if (!storeFixtureDefinition(fxName, fixtureXML))
            return CommonRequestResult::Handled;

        QByteArray postReply =
                QString("<html><head>\n<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\n"
                        "<script>\n"
                        " alert(\"" + tr("Fixture stored and loaded") + "\");"
                        " window.location = \"/config\"\n"
                        "</script></head></html>").toUtf8();

        resp->setHeader("Content-Type", "text/html");
        resp->setHeader("Content-Length", QString::number(postReply.size()));
        resp->writeHead(200);
        resp->end(postReply);

        return CommonRequestResult::Handled;
    }
    else if (reqUrl == "/config")
    {
        // the page where this computer creates the first admin (-wa, no
        // account yet): shown to it; everything else on it stays refused
        if (!user.setupOnly && !requireAuthLevel(resp, user, SUPER_ADMIN_LEVEL))
            return CommonRequestResult::Handled;
        content = WebAccessConfiguration::getHTML(m_doc, m_auth);
        return CommonRequestResult::ContentReady;
    }
    else if (reqUrl == "/simpleDesk")
    {
        if (!requireAuthLevel(resp, user, SIMPLE_DESK_AND_VC_LEVEL))
            return CommonRequestResult::Handled;
        content = WebAccessSimpleDesk::getHTML(m_doc, m_sd);
        return CommonRequestResult::ContentReady;
    }
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    else if (reqUrl == "/system")
    {
        if (!requireAuthLevel(resp, user, SUPER_ADMIN_LEVEL))
            return CommonRequestResult::Handled;
        content = m_netConfig->getHTML();
        return CommonRequestResult::ContentReady;
    }
#endif
    else if (reqUrl.endsWith(".png"))
    {
        if (servePng(resp, reqUrl))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".jpg") || reqUrl.endsWith(".jpeg"))
    {
        if (isShowImage(reqUrl) && sendFile(resp, reqUrl, "image/jpg"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".bmp"))
    {
        if (isShowImage(reqUrl) && sendFile(resp, reqUrl, "image/bmp"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".svg"))
    {
        if (isShowImage(reqUrl) && sendFile(resp, reqUrl, "image/svg+xml"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".ico"))
    {
        if (serveWebFile(resp, reqUrl, "image/x-icon"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".css"))
    {
        if (serveWebFile(resp, reqUrl, "text/css"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".js"))
    {
        if (serveWebFile(resp, reqUrl, "text/javascript"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl.endsWith(".html"))
    {
        if (serveWebFile(resp, reqUrl, "text/html"))
            return CommonRequestResult::Handled;
    }
    else if (reqUrl != "/")
    {
        sendNotFound(resp);
        return CommonRequestResult::Handled;
    }

    // A file asked for and not served (missing, or not ours to serve) is a
    // 404, not the console page under its name.
    if (reqUrl != "/")
    {
        sendNotFound(resp);
        return CommonRequestResult::Handled;
    }

    return CommonRequestResult::NotHandled;
}

bool WebAccessBase::handleCommonWebSocketCommand(QHttpConnection *conn, WebAccessUser *user,
                                                 const QStringList &cmdList, const QString &logTag,
                                                 bool logWarning)
{
    if (cmdList.isEmpty())
        return false;

    auto logUnsupported = [&](const QString &cmd) {
        if (logWarning)
            qWarning() << logTag << "Command" << cmd << "not supported!";
        else
            qDebug() << logTag << "Command" << cmd << "not supported!";
    };

    if (cmdList[0] == "QLC+IO")
    {
        if (!permits(user, SUPER_ADMIN_LEVEL))
            return true;

        if (cmdList.count() < 3)
            return true;
        // the patch commands carry a plugin and a line, PROFILE and
        // PASSTHROUGH one more value
        if ((cmdList[1] == "INPUT" || cmdList[1] == "OUTPUT" || cmdList[1] == "FB") && cmdList.count() < 5)
            return true;
        if ((cmdList[1] == "PROFILE" || cmdList[1] == "PASSTHROUGH") && cmdList.count() < 4)
            return true;

        int universe = cmdList[2].toInt();

        if (cmdList[1] == "INPUT")
        {
            m_doc->inputOutputMap()->setInputPatch(universe, cmdList[3], "", cmdList[4].toUInt());
            m_doc->inputOutputMap()->saveDefaults();
        }
        else if (cmdList[1] == "OUTPUT")
        {
            m_doc->inputOutputMap()->setOutputPatch(universe, cmdList[3], "", cmdList[4].toUInt(), false);
            m_doc->inputOutputMap()->saveDefaults();
        }
        else if (cmdList[1] == "FB")
        {
            m_doc->inputOutputMap()->setOutputPatch(universe, cmdList[3], "", cmdList[4].toUInt(), true);
            m_doc->inputOutputMap()->saveDefaults();
        }
        else if (cmdList[1] == "PROFILE")
        {
            InputPatch *inPatch = m_doc->inputOutputMap()->inputPatch(universe);
            if (inPatch != nullptr)
            {
                m_doc->inputOutputMap()->setInputPatch(universe, inPatch->pluginName(), "", inPatch->input(), cmdList[3]);
                m_doc->inputOutputMap()->saveDefaults();
            }
        }
        else if (cmdList[1] == "PASSTHROUGH")
        {
            quint32 uniIdx = cmdList[2].toUInt();
            m_doc->inputOutputMap()->setUniversePassthrough(uniIdx, cmdList[3] == "true");
            m_doc->inputOutputMap()->saveDefaults();
        }
        else if (cmdList[1] == "AUDIOIN")
        {
            QSettings settings;
            if (cmdList[2] == "__qlcplusdefault__")
                settings.remove(SETTINGS_AUDIO_INPUT_DEVICE);
            else
            {
                settings.setValue(SETTINGS_AUDIO_INPUT_DEVICE, cmdList[2]);
                m_doc->destroyAudioCapture();
            }
        }
        else if (cmdList[1] == "AUDIOOUT")
        {
            QSettings settings;
            if (cmdList[2] == "__qlcplusdefault__")
                settings.remove(SETTINGS_AUDIO_OUTPUT_DEVICE);
            else
                settings.setValue(SETTINGS_AUDIO_OUTPUT_DEVICE, cmdList[2]);
        }
        else
        {
            logUnsupported(cmdList[1]);
        }

        return true;
    }
    else if (cmdList[0] == "QLC+AUTH")
    {
        if (!m_auth)
            return true;

        if (cmdList.count() < 2)
            return true;

        // An admin manages the accounts. Before there is one, this
        // computer's operator may create it -- the first admin, nothing else.
        const bool firstAdmin = user != nullptr && user->setupOnly && !m_auth->hasAtLeastOneAdmin()
                && cmdList.at(1) == "ADD_USER" && cmdList.count() >= 5
                && cmdList.at(4).toInt() >= SUPER_ADMIN_LEVEL;
        if (!permits(user, SUPER_ADMIN_LEVEL) && !firstAdmin)
            return true;

        if (cmdList.at(1) == "ADD_USER")
        {
            if (cmdList.count() < 5)
                return true;
            QString username = cmdList.at(2);
            QString password = cmdList.at(3);
            int level = cmdList.at(4).toInt();
            // a name is shown in the page and written to the password file
            // (name:hash:level:...): no markup, no separator
            static const QRegularExpression okName(QStringLiteral("^[A-Za-z0-9._@-]{1,64}$"));
            if (!username.isEmpty() && !okName.match(username).hasMatch())
            {
                if (conn)
                    conn->webSocketWrite(QString("ALERT|" + tr("A username is letters, digits and . _ @ - only.")));
                return true;
            }
            if (username.isEmpty() || password.isEmpty())
            {
                QString wsMessage = QString("ALERT|" + tr("Username and password are required fields."));
                if (conn)
                    conn->webSocketWrite(wsMessage);
                return true;
            }
            if (level <= 0)
            {
                QString wsMessage = QString("ALERT|" + tr("User level has to be a positive integer."));
                if (conn)
                    conn->webSocketWrite(wsMessage);
                return true;
            }

            m_auth->addUser(username, password, (WebAccessUserLevel)level);
        }
        else if (cmdList.at(1) == "DEL_USER")
        {
            if (cmdList.count() < 3)
                return true;
            QString username = cmdList.at(2);
            if (!username.isEmpty())
                m_auth->deleteUser(username);
        }
        else if (cmdList.at(1) == "SET_USER_LEVEL")
        {
            if (cmdList.count() < 4)
                return true;
            QString username = cmdList.at(2);
            int level = cmdList.at(3).toInt();
            if (username.isEmpty())
            {
                QString wsMessage = QString("ALERT|" + tr("Username is required."));
                if (conn)
                    conn->webSocketWrite(wsMessage);
                return true;
            }
            if (level <= 0)
            {
                QString wsMessage = QString("ALERT|" + tr("User level has to be a positive integer."));
                if (conn)
                    conn->webSocketWrite(wsMessage);
                return true;
            }

            m_auth->setUserLevel(username, (WebAccessUserLevel)level);
        }
        else
        {
            logUnsupported(cmdList[1]);
        }

        if (!m_auth->savePasswordsFile())
        {
            QString wsMessage = QString("ALERT|" + tr("Error while saving passwords file."));
            if (conn)
                conn->webSocketWrite(wsMessage);
            return true;
        }

        return true;
    }
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    else if (cmdList[0] == "QLC+SYS")
    {
        // Reboot, halt, the network, the hotspot: the computer's admin only
        if (!permits(user, SUPER_ADMIN_LEVEL))
        {
            qWarning() << logTag << "refused" << cmdList.value(1) << "from a client without admin access";
            return true;
        }

        if (cmdList.count() < 2)
            return true;

        if (cmdList.at(1) == "NETWORK")
        {
            // interface, method, address, mask, gateway, SSID, password
            if (cmdList.count() < 9)
                return true;
            QString wsMessage;
            if (m_netConfig->updateNetworkSettings(cmdList))
                wsMessage = QString("ALERT|" + tr("Network configuration changed. Reboot to apply the changes."));
            else
                wsMessage = QString("ALERT|" + tr("An error occurred while updating the network configuration."));

            if (conn)
                conn->webSocketWrite(wsMessage);
            return true;
        }
        else if (cmdList.at(1) == "HOTSPOT")
        {
            QString wsMessage;
            if (cmdList.count() < 5)
                return true;

            bool enable = cmdList.at(2).toInt();

            if (enable)
            {
                if (m_netConfig->createWiFiHotspot(cmdList.at(3), cmdList.at(4)))
                    wsMessage = QString("ALERT|" + tr("Wi-Fi hotspot successfully activated."));
                else
                    wsMessage = QString("ALERT|" + tr("An error occurred while creating a Wi-Fi hotspot."));
            }
            else
            {
                m_netConfig->deleteWiFiHotspot();
                wsMessage = QString("ALERT|" + tr("Wi-Fi hotspot successfully deactivated."));
            }

            if (conn)
                conn->webSocketWrite(wsMessage);
            return true;
        }
        else if (cmdList.at(1) == "AUTOSTART")
        {
            if (cmdList.count() < 3)
                return true;

            QString asName = QString("%1/%2/%3").arg(getenv("HOME")).arg(USERQLCPLUSDIR).arg(AUTOSTART_PROJECT_NAME);
            if (cmdList.at(2) == "none")
                QFile::remove(asName);
            else
                handleAutostartProject(asName);
            QString wsMessage = QString("ALERT|" + tr("Autostart configuration changed"));
            if (conn)
                conn->webSocketWrite(wsMessage);
            return true;
        }
        else if (cmdList.at(1) == "REBOOT")
        {
            QProcess *rebootProcess = new QProcess();
            rebootProcess->start("sudo", QStringList() << "shutdown" << "-r" << "now");
            return true;
        }
        else if (cmdList.at(1) == "HALT")
        {
            QProcess *haltProcess = new QProcess();
            haltProcess->start("sudo", QStringList() << "shutdown" << "-h" << "now");
            return true;
        }
    }
#endif

    return false;
}

void WebAccessBase::handleAutostartProject(const QString &path)
{
    Q_UNUSED(path)
}
