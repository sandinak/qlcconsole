/*
  Q Light Controller Plus - qlcconsole
  webaccess_test.cpp

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

/* Everything here talks to 127.0.0.1 only. */

#include <QtTest>
#include <QTcpSocket>
#include <QTcpServer>
#include <QWebSocket>
#include <QSignalSpy>
#include <QFile>
#include <QDir>

#include "hostilecase.h"
#include "webaccess_test.h"

#define private public
#define protected public
#include "webaccess.h"
#include "webaccessbase.h"
#include "webaccessauth.h"
#include "qhttprequest.h"
#include "qhttpresponse.h"
#undef private
#undef protected
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcbutton.h"
#include "vclabel.h"
#include "simpledesk.h"
#include "function.h"
#include "doc.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "qlcconfig.h"

static int freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    return probe.serverPort();
}

void WebAccess_Test::initTestCase()
{
    /* A HOME of our own: the fixture upload and the password file land here,
       never in the user's real folders. */
    qputenv("HOME", m_home.path().toLocal8Bit());
    qputenv("USERPROFILE", m_home.path().toLocal8Bit());
}

void WebAccess_Test::init()
{
    m_doc = new Doc(this);
    m_vc = new VirtualConsole(nullptr, m_doc);
    m_sd = new SimpleDesk(nullptr, m_doc);
}

void WebAccess_Test::cleanup()
{
    /* Let any WebSocket close land (deleteLater of the connection) before the
       server is torn down: see shutdownWithALiveClientIsClean, which pins the
       re-entrant-delete crash in WebAccessBase::~WebAccessBase. */
    if (m_web != nullptr)
    {
        QTest::qWait(600);
        delete m_web;
        m_web = nullptr;
        QTest::qWait(50);
    }
    delete m_sd;
    m_sd = nullptr;
    delete m_vc;
    m_vc = nullptr;
    delete m_doc;
    m_doc = nullptr;
}

void WebAccess_Test::startWeb(bool auth, const QString &passwdFile)
{
    /* A port the OS says is free (0 would mean web access's 9999), read
       back off the listening socket: the gate's legs run this at the same
       time, and the developer's own console may hold 9999. */
    m_web = new WebAccess(m_doc, m_vc, m_sd, freePort(), auth, passwdFile);
    QTcpServer *srv = reinterpret_cast<QObject*>(m_web->m_httpServer)->findChild<QTcpServer *>();
    QVERIFY2(srv != nullptr && srv->isListening(), "web server did not listen");
    m_port = srv->serverPort();
    QVERIFY(m_port != 0);
}

QByteArray WebAccess_Test::http(const QByteArray &request, int timeoutMs)
{
    /* The server runs in this thread: wait with the event loop running
       (QTest::qWait), never with the blocking waitFor*(), or it can't answer
       until the client has given up. */
    QTcpSocket s;
    QElapsedTimer t;
    t.start();
    s.connectToHost(QHostAddress::LocalHost, m_port);
    while (s.state() != QAbstractSocket::ConnectedState && t.elapsed() < timeoutMs)
    {
        if (s.state() == QAbstractSocket::UnconnectedState)
            return QByteArray();
        QTest::qWait(5);
    }
    if (s.state() != QAbstractSocket::ConnectedState)
        return QByteArray();
    s.write(request);
    s.flush();
    QByteArray reply;
    while (t.elapsed() < timeoutMs)
    {
        QTest::qWait(10);
        reply += s.readAll();
        // the server hangs up after its answer, or keeps the connection
        // (HTTP/1.1): either way a whole answer is the end
        if (s.state() != QAbstractSocket::ConnectedState)
            break;
        const int headEnd = reply.indexOf("\r\n\r\n");
        if (headEnd >= 0)
        {
            const QByteArray head = reply.left(headEnd).toLower();
            const int cl = head.indexOf("content-length:");
            if (cl >= 0)
            {
                const int eol = head.indexOf("\r\n", cl);
                const int len = head.mid(cl + 15, eol < 0 ? -1 : eol - cl - 15).trimmed().toInt();
                if (reply.size() - (headEnd + 4) >= len)
                    break;
            }
        }
    }
    reply += s.readAll();
    return reply;
}

QString WebAccess_Test::wsRoundTrip(const QStringList &commands, const QByteArray &origin)
{
    QWebSocket ws;
    QString err;
    connect(&ws, &QWebSocket::errorOccurred, [&](QAbstractSocket::SocketError) { err = ws.errorString(); });
    QNetworkRequest req(QUrl(QStringLiteral("ws://127.0.0.1:%1/qlcplusWS").arg(m_port)));
    if (!origin.isEmpty())
        req.setRawHeader("Origin", origin);
    QSignalSpy connectedSpy(&ws, &QWebSocket::connected);
    QSignalSpy closedSpy(&ws, &QWebSocket::disconnected);
    ws.open(req);
    // a refusal (401, 403) ends it at once: don't sit out the timeout
    QElapsedTimer t;
    t.start();
    while (connectedSpy.isEmpty() && closedSpy.isEmpty() && err.isEmpty() && t.elapsed() < 5000)
        QTest::qWait(10);
    if (connectedSpy.isEmpty())
        return QStringLiteral("connect failed: %1").arg(err);
    for (const QString &c : commands)
        ws.sendTextMessage(c);
    QTest::qWait(300);
    ws.close();
    return QString();
}

/*****************************************************************************
 * Who can reach it
 *****************************************************************************/

/* FINDING: WebAccessBase binds the HTTP/WebSocket server to QHostAddress::Any
   (webaccessbase.cpp:78), so -w exposes the show to the whole LAN. This test
   wants loopback by default; it FAILS today and documents the finding. */
void WebAccess_Test::itListensOnlyOnLoopbackByDefault()
{
    startWeb(false);
    QTcpServer *srv = reinterpret_cast<QObject*>(m_web->m_httpServer)->findChild<QTcpServer *>();
    QVERIFY(srv != nullptr);
    QVERIFY2(srv->serverAddress() == QHostAddress(QHostAddress::LocalHost)
                 || srv->serverAddress() == QHostAddress(QHostAddress::LocalHostIPv6),
             qPrintable(QStringLiteral("web access bound to %1 -- reachable from the LAN")
                            .arg(srv->serverAddress().toString())));
}

/* With no -wa, there is no auth object, so /loadProject (SUPER_ADMIN) is let
   through: an unauthenticated client can replace the running show. This test
   expects a refusal and FAILS today (the finding).
   RC8: -w alone lets this computer change the show and a visitor from the
   network only look. Loopback is all a test can reach, so the test turns
   off "loopback is this computer" to stand in for that visitor. */
void WebAccess_Test::withoutAuthAProjectCannotBeLoaded()
{
    startWeb(false);
    m_web->m_trustLoopback = false;
    QSignalSpy spy(m_web, SIGNAL(loadProject(QString)));
    const QByteArray body = "\n\r\n<Workspace/>\n\r\n";
    const QByteArray req = "POST /loadProject HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                           "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    http(req);
    QTest::qWait(300);
    QCOMPARE(spy.count(), 0);
}

/* Same for an unauthenticated fixture upload (writes a .qxf under HOME). */
void WebAccess_Test::withoutAuthAFixtureCannotBeStored()
{
    startWeb(false);
    m_web->m_trustLoopback = false;
    const QByteArray body = "filename=\"evil.qxf\"\n\r\n<FixtureDefinition/>\n\r\n";
    const QByteArray req = "POST /loadFixture HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                           "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    http(req);
    QTest::qWait(300);
    const QDir udir = QLCFixtureDefCache::userDefinitionDirectory();
    QVERIFY2(!QFile::exists(udir.absoluteFilePath(QStringLiteral("evil.qxf"))),
             "an unauthenticated client wrote a fixture definition to disk");
}

/* With -wa but no admin users yet, authenticateRequest returns
   NOT_PROVIDED_LEVEL (== SUPER_ADMIN), so the very first visitor is admin.
   This test expects that an empty password file does not grant admin, and
   FAILS today (the finding). */
void WebAccess_Test::authWithNoUsersIsNotOpenToEveryone()
{
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_empty"));
    QVERIFY(QFile(pf).open(QIODevice::WriteOnly));   // exists, empty
    startWeb(true, pf);
    QSignalSpy spy(m_web, SIGNAL(loadProject(QString)));
    const QByteArray body = "\n\r\n<Workspace/>\n\r\n";
    const QByteArray req = "POST /loadProject HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                           "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    const QByteArray reply = http(req);
    QTest::qWait(300);
    QVERIFY2(spy.count() == 0 && reply.contains("401"),
             "with no admin accounts, an anonymous client was treated as admin");
}

/* A client that has not authenticated must not be able to add themselves as
   an admin over the WebSocket (QLC+AUTH ADD_USER). RC8: only this computer
   may create the first admin (theFirstAdminComesFromThisComputer); this one
   stands in for a visitor from the network. */
void WebAccess_Test::theFirstVisitorCannotMakeThemselvesAdmin()
{
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_noadmin"));
    QVERIFY(QFile(pf).open(QIODevice::WriteOnly));
    startWeb(true, pf);
    m_web->m_trustLoopback = false;
    wsRoundTrip({ QStringLiteral("QLC+AUTH|ADD_USER|attacker|pw|100") });
    QVERIFY2(!m_web->m_auth->hasAtLeastOneAdmin(),
             "an unauthenticated WebSocket client created an admin account");
}

/* A guest (logged in, low level) must not be able to toggle operate mode or
   drive the grand master to 0 (a blackout). */
void WebAccess_Test::aLoggedInGuestCannotBlackOutTheShow()
{
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_guest"));
    {
        WebAccessAuth auth(QStringLiteral("t"));
        auth.loadPasswordsFile(pf);
        auth.addUser(QStringLiteral("admin"), QStringLiteral("pw"), SUPER_ADMIN_LEVEL);
        auth.addUser(QStringLiteral("guest"), QStringLiteral("pw"), VC_ONLY_LEVEL);
        auth.savePasswordsFile();
    }
    startWeb(true, pf);
    /* GM_VALUE has no level check at all in slotHandleWebSocketRequest, so a
       guest (indeed any connected socket) can black out the show. Expect the
       GM to be untouched -- FAILS today (the finding). */
    QWebSocket ws;
    QSignalSpy connectedSpy(&ws, &QWebSocket::connected);
    ws.open(QUrl(QStringLiteral("ws://guest:pw@127.0.0.1:%1/qlcplusWS").arg(m_port)));
    if (connectedSpy.wait(5000))
    {
        ws.sendTextMessage(QStringLiteral("GM_VALUE|0"));
        QTest::qWait(300);
        ws.close();
    }
    QVERIFY2(m_doc->inputOutputMap()->grandMasterValue() == 255,
             "a low-level client set the grand master (blackout)");
}

/* A page on another origin opens a WebSocket and drives the show: there is
   no Origin check, so a browser visiting a hostile page on the same network
   blacks out the console (classic CSWSH). Expect the command to be refused
   when the Origin is not the console -- FAILS today (the finding). */
void WebAccess_Test::aPageOnAnotherSiteCannotDriveTheShow()
{
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    startWeb(false);
    wsRoundTrip({ QStringLiteral("GM_VALUE|0") }, QByteArray("http://evil.example"));
    QVERIFY2(m_doc->inputOutputMap()->grandMasterValue() == 255,
             "a WebSocket from a foreign Origin drove the grand master");
}

/*****************************************************************************
 * What it serves
 *****************************************************************************/

/* A path-traversal GET must not read files outside the web resource folder. */
void WebAccess_Test::filesOutsideTheWebFolderAreNotServed()
{
    startWeb(false);
    QFile secret(m_home.filePath(QStringLiteral("secret.css")));
    QVERIFY(secret.open(QIODevice::WriteOnly));
    secret.write("SECRET-MARKER");
    secret.close();
    const QByteArray abs = m_home.filePath(QStringLiteral("secret.css")).toUtf8();
    const QByteArray reply = http("GET " + abs + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(!reply.contains("SECRET-MARKER"),
             "an absolute path outside the web folder was served");
    const QByteArray trav = http("GET /../../../../etc/hosts HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(!trav.contains("localhost\t") && !trav.contains("127.0.0.1\tlocalhost"),
             "../ traversal reached a system file");
}

/* An uploaded fixture name must not escape the fixture folder (it is passed
   to QDir::absoluteFilePath without sanitising; see webaccessbase.cpp:289). */
void WebAccess_Test::anUploadedFixtureStaysInTheFixtureFolder()
{
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_admin"));
    {
        WebAccessAuth auth(QStringLiteral("t"));
        auth.loadPasswordsFile(pf);
        auth.addUser(QStringLiteral("admin"), QStringLiteral("pw"), SUPER_ADMIN_LEVEL);
        auth.savePasswordsFile();
    }
    startWeb(true, pf);
    const QByteArray cred = QByteArray("admin:pw").toBase64();
    const QByteArray body = "filename=\"../escaped.qxf\"\n\r\n<FixtureDefinition/>\n\r\n";
    const QByteArray req = "POST /loadFixture HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                           "Authorization: Basic " + cred + "\r\n"
                           "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    http(req);
    QTest::qWait(300);
    const QDir udir = QLCFixtureDefCache::userDefinitionDirectory();
    QDir parent = udir;
    parent.cdUp();
    QVERIFY2(!QFile::exists(parent.absoluteFilePath(QStringLiteral("escaped.qxf"))),
             "an uploaded fixture escaped the fixture folder via ../ in its name");
}

/* A function named with HTML/script must be escaped where the page prints it,
   or a crew member's show name becomes stored XSS in the web UI. The cue list
   HTML writes function->name() straight out (webaccess.cpp:1338). */
void WebAccess_Test::namesFromTheShowAreEscapedInThePage()
{
    startWeb(false);
    VCFrame *contents = m_vc->contents();
    QVERIFY(contents != nullptr);
    /* Drive the widget's own HTML builder directly: a crew member's caption,
       rendered straight into the page with no toHtmlEscaped (webaccess.cpp
       :947 button, :1079 label, :1338 cue-list function name), is stored XSS
       against anyone who opens the web UI. FAILS today. */
    VCLabel *label = new VCLabel(contents, m_doc);
    label->setCaption(QStringLiteral("<img src=x onerror=alert(2)>"));
    const QString labelHtml = m_web->getLabelHTML(label);
    QVERIFY2(!labelHtml.contains("<img src=x onerror=alert(2)>"),
             "a widget caption was emitted as live HTML (XSS)");
}

/*****************************************************************************
 * Malformed input
 *****************************************************************************/

/* GM_VALUE / CH and friends read cmdList[1] (or [2]) without checking the
   count, so a frame of just "GM_VALUE" is an out-of-range QList access: an
   abort in Debug, an unchecked read in Release -- from one unauthenticated
   WebSocket frame. Child-isolated; FAILS today. */
void WebAccess_Test::shortCommandsDoNotCrash()
{
    HOSTILE_CASE(15000);
    startWeb(false);
    wsRoundTrip({
        QStringLiteral("QLC+API"),
        QStringLiteral("QLC+API|getFunctionType"),
        QStringLiteral("QLC+CMD"),
        QStringLiteral("CH"),
        QStringLiteral("CH|1"),
        QStringLiteral("QLC+IO|INPUT"),
        QStringLiteral("GM_VALUE"),
        QStringLiteral("|"),
        QString(),
    });
    delete m_web;       // tear down cleanly so the dtor bug isn't blamed
    m_web = nullptr;
    QTest::qWait(100);
}

void WebAccess_Test::unknownWidgetIdsDoNotCrash()
{
    HOSTILE_CASE(15000);
    startWeb(false);
    wsRoundTrip({
        QStringLiteral("QLC+API|getWidgetType|999999"),
        QStringLiteral("QLC+API|getWidgetStatus|999999"),
        QStringLiteral("QLC+API|getWidgetSubIdList|999999"),
        QStringLiteral("999999|1"),
    });
    delete m_web;
    m_web = nullptr;
    QTest::qWait(100);
}

void WebAccess_Test::aMalformedUrlDoesNotAbort()
{
    HOSTILE_CASE(15000);
    startWeb(false);
    /* HeadersComplete asserts http_parser_parse_url() == 0; a URL it cannot
       parse should 404, not abort the process. */
    const QByteArray reply = http("GET http://[ HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    Q_UNUSED(reply);
    // still alive -> serve a normal request
    const QByteArray ok = http("GET /nope.css HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(ok.contains("404") || ok.isEmpty(), "server did not survive a malformed URL");
    delete m_web;
    m_web = nullptr;
    QTest::qWait(100);
}

void WebAccess_Test::aHugeUploadIsRefusedEarly()
{
    HOSTILE_CASE(15000);
    startWeb(false);
    /* Content-Length far past what the server should buffer. We only send a
       little, then drop: the server must not have pre-allocated gigabytes or
       hung. This is a smoke test that the connection is handled and the
       process stays alive. */
    const QByteArray req = "POST /loadProject HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                           "Content-Length: 2000000000\r\n\r\nxxxx";
    http(req, 2000);
    const QByteArray ok = http("GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(!ok.isEmpty(), "server did not survive a huge Content-Length");
    delete m_web;
    m_web = nullptr;
    QTest::qWait(100);
}

/* Tearing down web access while a WebSocket client is still connected re-enters
   the connection delete (WebAccessBase::~WebAccessBase foreach-deletes each
   conn; the socket's disconnected() drives slotHandleWebSocketClose, which
   deleteLater()s the same conn and mutates m_webSocketsList mid-iteration).
   That happens on every app quit with web access on and a browser open. Runs
   in a child so the crash does not take the suite down; FAILS today. */
void WebAccess_Test::shutdownWithALiveClientIsClean()
{
    HOSTILE_CASE(15000);
    startWeb(false);
    QWebSocket ws;
    QSignalSpy connectedSpy(&ws, &QWebSocket::connected);
    ws.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/qlcplusWS").arg(m_port)));
    connectedSpy.wait(5000);
    ws.sendTextMessage(QStringLiteral("POLL"));
    QTest::qWait(300);
    /* client still open -- tear the server down under it */
    delete m_web;
    m_web = nullptr;
    QTest::qWait(300);
}

/*****************************************************************************
 * RC8: what this computer may still do, and the new ways in that stay shut
 *****************************************************************************/

static QByteArray loadProjectRequest(const QByteArray &extraHeaders = QByteArray())
{
    const QByteArray body = "\n\r\n<Workspace/>\n\r\n";
    return "POST /loadProject HTTP/1.1\r\nHost: 127.0.0.1\r\n" + extraHeaders +
           "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

/* -w alone (the preference recorded in RC8): the operator at this computer
   still loads a show, runs the console, and so on. */
void WebAccess_Test::withoutAuthThisComputerStillHasFullAccess()
{
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    startWeb(false);
    QSignalSpy spy(m_web, SIGNAL(loadProject(QString)));
    http(loadProjectRequest());
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(wsRoundTrip({ QStringLiteral("GM_VALUE|100") }), QString());
    QTRY_COMPARE(int(m_doc->inputOutputMap()->grandMasterValue()), 100);
}

/* A visitor from the network, -w alone: the page and its WebSocket, to look;
   no Grand Master, no channel, no widget. */
void WebAccess_Test::withoutAuthTheNetworkCanLookButNotTouch()
{
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    startWeb(false);
    m_web->m_trustLoopback = false;
    const QByteArray page = http("GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(page.contains("200"), page.left(200).constData());
    QCOMPARE(wsRoundTrip({ QStringLiteral("GM_VALUE|0"), QStringLiteral("CH|1|255"),
                           QStringLiteral("QLC+CMD|opMode") }), QString());
    QCOMPARE(int(m_doc->inputOutputMap()->grandMasterValue()), 255);
}

/* A page on some other site whose name resolves to 127.0.0.1 (DNS
   rebinding) reaches us from loopback, under its own name: it is not this
   computer's operator. */
void WebAccess_Test::aRebindingNameIsNotThisComputer()
{
    startWeb(false);
    QSignalSpy spy(m_web, SIGNAL(loadProject(QString)));
    const QByteArray body = "\n\r\n<Workspace/>\n\r\n";
    http("POST /loadProject HTTP/1.1\r\nHost: evil.example:9999\r\n"
         "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    QTest::qWait(300);
    QCOMPARE(spy.count(), 0);
}

/* A form on another site posting to the console (CSRF) is refused even from
   this computer: its Origin is not ours. */
void WebAccess_Test::aFormOnAnotherSiteCannotLoadAProject()
{
    startWeb(false);
    QSignalSpy spy(m_web, SIGNAL(loadProject(QString)));
    const QByteArray reply = http(loadProjectRequest("Origin: http://evil.example\r\n"));
    QTest::qWait(300);
    QCOMPARE(spy.count(), 0);
    QVERIFY2(reply.contains("403"), reply.left(200).constData());
    // the console's own page (same origin) may
    http(loadProjectRequest(QByteArray("Origin: http://127.0.0.1:") + QByteArray::number(m_port) + "\r\n"));
    QTRY_COMPARE(spy.count(), 1);
}

/* -wa and no admin yet: this computer creates the first admin -- an admin,
   and nothing else. */
void WebAccess_Test::theFirstAdminComesFromThisComputer()
{
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_first"));
    QVERIFY(QFile(pf).open(QIODevice::WriteOnly));
    startWeb(true, pf);
    // not a lesser account, and nothing else
    QCOMPARE(wsRoundTrip({ QStringLiteral("QLC+AUTH|ADD_USER|vc|pw|10"),
                           QStringLiteral("GM_VALUE|0") }), QString());
    QVERIFY(m_web->m_auth->getUsers().isEmpty());
    QCOMPARE(int(m_doc->inputOutputMap()->grandMasterValue()), 255);
    // a name that would be markup in the page is refused
    QCOMPARE(wsRoundTrip({ QStringLiteral("QLC+AUTH|ADD_USER|<b>x</b>|pw|100") }), QString());
    QVERIFY(m_web->m_auth->getUsers().isEmpty());
    // the Users page opens for it (and nothing else would)
    const QByteArray config = http("GET /config HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(config.contains("200") && config.contains("auth-new-username"), config.left(200).constData());
    const QByteArray desk = http("GET /simpleDesk HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    QVERIFY2(desk.contains("401"), desk.left(200).constData());
    QCOMPARE(wsRoundTrip({ QStringLiteral("QLC+AUTH|ADD_USER|boss|pw|100") }), QString());
    QTRY_VERIFY(m_web->m_auth->hasAtLeastOneAdmin());
    // from then on, everyone logs in: an anonymous request is a 401
    const QByteArray reply = http(loadProjectRequest());
    QVERIFY2(reply.contains("401"), reply.left(200).constData());
}

/* --web-bind: another address when asked (still loopback here), and the URL
   to give out names it. */
void WebAccess_Test::itListensWhereItIsTold()
{
    m_web = new WebAccess(m_doc, m_vc, m_sd, freePort(), false, QString(), QHostAddress(QStringLiteral("127.0.0.2")));
    if (m_web->listenPort() == 0)
        QSKIP("127.0.0.2 is not a loopback address on this computer");
    QCOMPARE(m_web->listenAddress(), QHostAddress(QStringLiteral("127.0.0.2")));
    QVERIFY2(m_web->url().startsWith(QStringLiteral("http://127.0.0.2:")), qPrintable(m_web->url()));
}

/* QLC+SYS (reboot, halt, network, hotspot, autostart) needs an admin. The
   only one a test can run safely is AUTOSTART none, which deletes the
   autostart show: a VC-only user must not. */
void WebAccess_Test::systemCommandsNeedAnAdmin()
{
#if defined(Q_OS_LINUX)
    const QString pf = m_home.filePath(QStringLiteral("web_passwd_sys"));
    {
        WebAccessAuth auth(QStringLiteral("t"));
        auth.loadPasswordsFile(pf);
        auth.addUser(QStringLiteral("admin"), QStringLiteral("pw"), SUPER_ADMIN_LEVEL);
        auth.addUser(QStringLiteral("vc"), QStringLiteral("pw"), SIMPLE_DESK_AND_VC_LEVEL);
        auth.savePasswordsFile();
    }
    QDir(m_home.path()).mkpath(QStringLiteral(USERQLCPLUSDIR));
    const QString as = m_home.path() + QStringLiteral("/" USERQLCPLUSDIR "/autostart.qxw");
    QFile f(as);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    startWeb(true, pf);
    WebAccessUser vc(SIMPLE_DESK_AND_VC_LEVEL);
    m_web->handleCommonWebSocketCommand(nullptr, &vc, { QStringLiteral("QLC+SYS"), QStringLiteral("AUTOSTART"),
                                                        QStringLiteral("none") }, QStringLiteral("[t]"), false);
    QVERIFY2(QFile::exists(as), "a non-admin removed the autostart show");
    WebAccessUser none(GUEST_LEVEL);
    m_web->handleCommonWebSocketCommand(nullptr, &none, { QStringLiteral("QLC+SYS"), QStringLiteral("NETWORK") },
                                        QStringLiteral("[t]"), false);
    QVERIFY(QFile::exists(as));
#else
    // QLC+SYS is the Linux/Pi build's only; elsewhere it is not a command.
    startWeb(false);
    WebAccessUser none(GUEST_LEVEL);
    QVERIFY(!m_web->handleCommonWebSocketCommand(nullptr, &none, { QStringLiteral("QLC+SYS"), QStringLiteral("REBOOT") },
                                                 QStringLiteral("[t]"), false));
#endif
}

/* The upload name is only ever a plain .qxf file name. */
void WebAccess_Test::uploadNamesAreSanitised()
{
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("Par.qxf")), QStringLiteral("Par.qxf"));
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("../../x.qxf")), QStringLiteral("x.qxf"));
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("..\\..\\x.qxf")), QStringLiteral("x.qxf"));
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("/etc/passwd")), QString());
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("../")), QString());
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral(".qxf")), QString());
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("C:x.qxf")), QString());
    QCOMPARE(WebAccessBase::sanitizedFixtureFileName(QStringLiteral("evil.sh")), QString());
}

QTEST_MAIN(WebAccess_Test)

/*****************************************************************************
 * WebAccessAuth on its own
 *****************************************************************************/

static QString sha(QCryptographicHash::Algorithm a, const QString &text)
{
    return QCryptographicHash::hash(text.toUtf8(), a).toHex();
}

void WebAccess_Test::passwordFileFormats()
{
    /* username:passwordHash[:level[:hashType[:salt]]]: a bare name and
       hash is an admin with an unsalted SHA-256 (htpasswd-like files
       written by hand), the rest as given. */
    const QString path = m_home.filePath("formats_passwd");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(QString("alice:%1\n"
                    "not a line\n"
                    "\n"
                    "bob:%2:10\n"
                    "carol:%3:20:sha1:pepper\n"
                    "dave:%4:1:md5:s\n"
                    "erin:%5:1:whirlpool:s\n"
                    "bob:%6:1\n")
            .arg(sha(QCryptographicHash::Sha256, "a-pass"),
                 sha(QCryptographicHash::Sha256, "b-pass"),
                 sha(QCryptographicHash::Sha1, "c-passpepper"),
                 sha(QCryptographicHash::Md5, "d-passs"),
                 sha(QCryptographicHash::Sha1, "e-passs"),
                 sha(QCryptographicHash::Sha256, "b-new")).toUtf8());
    f.close();

    WebAccessAuth auth("QLC+");
    QVERIFY(!auth.loadPasswordsFile(m_home.filePath("no_such_passwd")));
    QVERIFY(auth.getUsers().isEmpty());
    QVERIFY(!auth.hasAtLeastOneAdmin());

    QVERIFY(auth.loadPasswordsFile(path));
    QMap<QString, WebAccessUser> users;
    for (const WebAccessUser &u : auth.getUsers())
        users.insert(u.username, u);
    QCOMPARE(users.keys(), QStringList({ "alice", "bob", "carol", "dave", "erin" }));
    QCOMPARE(int(users["alice"].level), int(SUPER_ADMIN_LEVEL));
    QCOMPARE(users["alice"].hashType, QString("sha256"));
    QVERIFY(users["alice"].passwordSalt.isEmpty());
    QCOMPARE(int(users["bob"].level), 1);                  // the later line wins
    QCOMPARE(int(users["carol"].level), 20);
    QCOMPARE(users["carol"].passwordSalt, QString("pepper"));
    QVERIFY(auth.hasAtLeastOneAdmin());

    QVERIFY(auth.verifyPassword("a-pass", users["alice"]));
    QVERIFY(!auth.verifyPassword("a-pas", users["alice"]));
    QVERIFY(auth.verifyPassword("b-new", users["bob"]));
    QVERIFY(!auth.verifyPassword("b-pass", users["bob"]));
    QVERIFY(auth.verifyPassword("c-pass", users["carol"]));
    QVERIFY(auth.verifyPassword("d-pass", users["dave"]));
    // An unknown hash type is taken as SHA-1
    QVERIFY(auth.verifyPassword("e-pass", users["erin"]));

    // Only an admin is an admin
    auth.deleteUser("alice");
    QVERIFY(!auth.hasAtLeastOneAdmin());
    QVERIFY(auth.setUserLevel("dave", SUPER_ADMIN_LEVEL));
    QVERIFY(auth.hasAtLeastOneAdmin());
}

void WebAccess_Test::passwordFileRoundTrip()
{
    const QString path = m_home.filePath("roundtrip_passwd");
    QFile::remove(path);

    WebAccessAuth auth("QLC+");
    QVERIFY(!auth.loadPasswordsFile(path));         // not there yet: remembered
    auth.addUser("admin", "s3cret:with:colons", SUPER_ADMIN_LEVEL);
    auth.addUser("desk", "deskpw", SIMPLE_DESK_AND_VC_LEVEL);
    auth.addUser("gone", "x", VC_ONLY_LEVEL);
    auth.deleteUser("gone");
    auth.deleteUser("never-was");
    QVERIFY(!auth.setUserLevel("never-was", VC_ONLY_LEVEL));
    QVERIFY(auth.setUserLevel("desk", VC_ONLY_LEVEL));
    QVERIFY(auth.savePasswordsFile());

    // Salted per user: one password, two users, two hashes
    auth.addUser("twin1", "same", VC_ONLY_LEVEL);
    auth.addUser("twin2", "same", VC_ONLY_LEVEL);
    WebAccessUser t1, t2;
    for (const WebAccessUser &u : auth.getUsers())
    {
        if (u.username == "twin1") t1 = u;
        if (u.username == "twin2") t2 = u;
    }
    QCOMPARE(t1.passwordSalt.size(), 32);
    QVERIFY(QRegularExpression("^[0-9a-f]{32}$").match(t1.passwordSalt).hasMatch());
    QVERIFY(t1.passwordSalt != t2.passwordSalt);
    QVERIFY(t1.passwordHash != t2.passwordHash);
    QCOMPARE(t1.hashType, QString("sha256"));

    WebAccessAuth again("QLC+");
    QVERIFY(again.loadPasswordsFile(path));
    QMap<QString, WebAccessUser> users;
    for (const WebAccessUser &u : again.getUsers())
        users.insert(u.username, u);
    QCOMPARE(users.keys(), QStringList({ "admin", "desk" }));
    QCOMPARE(int(users["admin"].level), int(SUPER_ADMIN_LEVEL));
    QCOMPARE(int(users["desk"].level), int(VC_ONLY_LEVEL));
    QVERIFY(again.verifyPassword("s3cret:with:colons", users["admin"]));
    QVERIFY(again.verifyPassword("deskpw", users["desk"]));
    QVERIFY(!again.verifyPassword("deskpw", users["admin"]));

    // Nowhere to write
    WebAccessAuth nowhere("QLC+");
    nowhere.loadPasswordsFile(m_home.filePath("no/such/dir/passwd"));
    nowhere.addUser("admin", "x", SUPER_ADMIN_LEVEL);
    QVERIFY(!nowhere.savePasswordsFile());
}

void WebAccess_Test::basicAuthChecksTheCredentials()
{
    WebAccessAuth auth("QLC+ test");
    QHttpRequest req(nullptr);
    QHttpResponse res(nullptr);        // headers only: nothing is written

    auto ask = [&](const QByteArray &authorization) {
        HeaderHash h;
        if (!authorization.isNull())
            h.insert("authorization", authorization);
        req.m_headers = h;
        res.m_headers.clear();
        return auth.authenticateRequest(&req, &res);
    };
    auto basic = [](const QString &userPass) {
        return QByteArray("Basic ") + userPass.toUtf8().toBase64();
    };

    // No admin yet: nobody is anybody, whatever they send
    auth.addUser("guest", "pw", VC_ONLY_LEVEL);
    WebAccessUser u = ask(basic("guest:pw"));
    QVERIFY(u.username.isEmpty());
    QCOMPARE(int(u.level), int(GUEST_LEVEL));

    auth.addUser("admin", "pa:ss", SUPER_ADMIN_LEVEL);

    // Asked for, with the realm
    u = ask(QByteArray());
    QVERIFY(u.username.isEmpty());
    QCOMPARE(int(u.level), int(GUEST_LEVEL));
    QCOMPARE(res.m_headers.value("WWW-Authenticate"), QString("Basic realm=\"QLC+ test\""));

    QVERIFY(ask("Bearer abc").username.isEmpty());
    QVERIFY(ask(basic("admin")).username.isEmpty());          // no colon
    QVERIFY(ask(basic("admin:pa")).username.isEmpty());       // wrong
    QVERIFY(ask(basic("admin:pa:sss")).username.isEmpty());
    QVERIFY(ask(basic("nobody:pa:ss")).username.isEmpty());
    QVERIFY(ask(basic(":pa:ss")).username.isEmpty());
    QVERIFY(ask(basic("guest:")).username.isEmpty());

    // The password is everything after the first colon
    u = ask(basic("admin:pa:ss"));
    QCOMPARE(u.username, QString("admin"));
    QCOMPARE(int(u.level), int(SUPER_ADMIN_LEVEL));
    u = ask(basic("guest:pw"));
    QCOMPARE(u.username, QString("guest"));
    QCOMPARE(int(u.level), int(VC_ONLY_LEVEL));
}
