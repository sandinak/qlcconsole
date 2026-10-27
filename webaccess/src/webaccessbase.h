/*
  Q Light Controller Plus
  webaccessbase.h

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

#ifndef WEBACCESSBASE_H
#define WEBACCESSBASE_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

#include "webaccessauth.h"

#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
class WebAccessNetwork;
#endif

class VirtualConsole;
class SimpleDesk;
class Doc;

class QHttpServer;
class QHttpRequest;
class QHttpResponse;
class QHttpConnection;

class WebAccessBase : public QObject
{
    Q_OBJECT
public:
    virtual ~WebAccessBase();

    /** The address web access listens on when not told otherwise: this
     *  computer only (RC8; upstream listened on every interface). */
    static QHostAddress defaultBindAddress();

    /** Where it listens (--web-bind), and on which port; a null address and
     *  port 0 when it could not listen. */
    QHostAddress listenAddress() const;
    quint16 listenPort() const;

    /** The URL to open, e.g. http://127.0.0.1:9999/ */
    QString url() const;

    /** Text for an HTML page: <, >, & and quotes escaped */
    static QString htmlEscaped(const QString &text);
    /** A JavaScript string literal, quotes included, safe inside a <script>
     *  and inside an HTML attribute */
    static QString jsString(const QString &text);
    /** Only the file name of an uploaded fixture definition, and only a
     *  .qxf: an empty string when the name is not one (../, a folder,
     *  another kind of file) */
    static QString sanitizedFixtureFileName(const QString &name);

protected:
    enum class CommonRequestResult
    {
        NotHandled,
        Handled,
        ContentReady
    };

    explicit WebAccessBase(Doc *doc, VirtualConsole *vcInstance, SimpleDesk *sdInstance,
                           int portNumber, bool enableAuth, const QString &passwdFile,
                           const QHostAddress &bindAddress, QObject *parent = nullptr);

    /** Who may do what (RC8):
     *  - with -wa: the account's level; until an admin account exists, nobody,
     *    except a visitor on this computer, who may create the first admin;
     *  - with -w only: a visitor on this computer may do everything, anybody
     *    else may look but change nothing.
     *  @a user is filled in; false (and the response sent) when refused. */

    bool sendFile(QHttpResponse *response, QString filename, QString contentType) const;
    void sendWebSocketMessage(const QString &message);
    virtual QString webFilePath(const QString &relativePath) const;
    bool servePng(QHttpResponse *resp, const QString &reqUrl) const;
    bool serveWebFile(QHttpResponse *resp, const QString &reqUrl, const QString &contentType) const;
    /** True when @a path (as the page asks for it) is a picture the show
     *  puts on the page: only those are served from outside the web folder */
    virtual bool isShowImage(const QString &path) const;
    bool authenticateRequest(QHttpRequest *req, QHttpResponse *resp, WebAccessUser &user);
    bool acceptWebSocket(QHttpResponse *resp, const WebAccessUser &user);
    QByteArray extractProjectXml(const QHttpRequest *req) const;
    void sendProjectLoadingResponse(QHttpResponse *resp) const;
    void sendNotFound(QHttpResponse *resp) const;
    void sendHtmlResponse(QHttpResponse *resp, const QString &content) const;
    bool requireAuthLevel(QHttpResponse *resp, const WebAccessUser &user, WebAccessUserLevel level) const;
    /** True when @a user may do what needs @a level */
    bool permits(const WebAccessUser *user, WebAccessUserLevel level) const;
    /** The request comes from this computer, by a loopback name (not a name
     *  some other site resolves to 127.0.0.1: DNS rebinding) */
    bool isLocalRequest(const QHttpRequest *req) const;
    /** A browser page on another site may not drive the console: the
     *  Origin, when there is one, is this console or this computer */
    bool originAllowed(const QHttpRequest *req) const;
    void sendForbidden(QHttpResponse *resp, const QString &why) const;
    bool handleCommonWebSocketCommand(QHttpConnection *conn, WebAccessUser *user,
                                      const QStringList &cmdList, const QString &logTag,
                                      bool logWarning);
    CommonRequestResult handleCommonHTTPRequest(QHttpRequest *req, QHttpResponse *resp,
                                                const WebAccessUser &user, const QString &reqUrl,
                                                QString &content);
    virtual void handleAutostartProject(const QString &path);
    virtual void handleProjectLoad(const QByteArray &projectXml) = 0;
    virtual bool storeFixtureDefinition(const QString &fxName, const QByteArray &fixtureXML) = 0;

protected slots:
    virtual void slotHandleHTTPRequest(QHttpRequest *req, QHttpResponse *resp) = 0;
    virtual void slotHandleWebSocketRequest(QHttpConnection *conn, QString data) = 0;
    virtual void slotHandleWebSocketClose(QHttpConnection *conn) = 0;
    virtual void slotFunctionStarted(quint32 fid) = 0;
    virtual void slotFunctionStopped(quint32 fid) = 0;

protected:
    Doc *m_doc;
    VirtualConsole *m_vc;
    SimpleDesk *m_sd;
    WebAccessAuth *m_auth;
#if defined(Q_WS_X11) || defined(Q_OS_LINUX)
    WebAccessNetwork *m_netConfig;
#endif
    QHttpServer *m_httpServer;
    QList<QHttpConnection *> m_webSocketsList;
    bool m_pendingProjectLoaded;
    /** A loopback visitor is this computer's operator. The tests turn it off
     *  to stand in for a visitor from the network, which loopback can't. */
    bool m_trustLoopback = true;
};

#endif // WEBACCESSBASE_H
