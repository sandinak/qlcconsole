/*
  Q Light Controller Plus - qlcconsole
  mcpserver.h

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

#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QJsonObject>
#include <QJsonArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QObject>
#include <QHash>

#include "doc.h"

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * The MCP server (docs/design/AI_ASSIST.md section 8 P3): the Operation
 * Registry's third front end, after the command line and the in-app
 * assistant. AI tools on this computer (Claude Code, Claude Desktop, a
 * local agent) see the registry's operations as MCP tools, with the same
 * names, descriptions and JSON schemas the in-app model gets, and run
 * them against the show that is open.
 *
 * - Off by default: Settings ▸ AI Assist ▸ "Let AI tools on this computer
 *   use the show" (AiSettings::mcpEnabled), with AI Assist on.
 * - This computer only: a local socket (QLocalServer; a named pipe on
 *   Windows), never a network port, readable by this user only. Clients
 *   reach it through `qlcconsole --mcp` (McpBridge), the stdio command an
 *   MCP client starts.
 * - Design only: entering Operate closes the socket and every connection
 *   (telling each why first); the Operate audit (AiResources) sees them
 *   gone. A request that reached it in Operate anyway is refused.
 * - Read-only operations run when called. Change operations are listed only
 *   with "...and propose changes" on (AiSettings::mcpChanges), and even
 *   then never run here: each call becomes a step of a proposal in the
 *   Show Tree's command line (AiAssist::proposeExternal()), which the
 *   operator applies -- one undo step -- or discards. The raw `command`
 *   (a whole grammar line) is never offered.
 *
 * decided: transport = stdio through `qlcconsole --mcp` to a local socket
 * of the running app (can change).
 *
 * Owned by App, beside AiAssist; all on the GUI thread.
 */
class McpServer : public QObject
{
    Q_OBJECT

public:
    explicit McpServer(Doc *doc, QObject *parent = nullptr);
    ~McpServer();

    static McpServer *instance();

    /** The socket it listens on; McpBridge::defaultServerName() unless set
     *  (before it starts). */
    void setServerName(const QString &name);
    QString serverName() const { return m_name; }

    /** Should it listen now: AI Assist and the MCP server on, in Design. */
    bool shouldListen() const;
    bool isListening() const;
    /** Why it isn't listening although it should (another qlcconsole has
     *  the socket...); empty: no problem. */
    QString problem() const { return m_problem; }
    /** A sentence for the settings page: listening, off, Operate, a problem. */
    QString statusText() const;
    int connectionCount() const;

    /** One JSON-RPC message from a client -> its answer (an empty object
     *  for a notification). */
    QJsonObject handle(const QJsonObject &message);

    /** The tools/list entries: the read-only operations, then (with
     *  @a changes) the change operations; never `command`. */
    static QJsonArray tools(bool changes);

    /** The MCP protocol versions this server speaks, newest last. */
    static QStringList protocolVersions();

    /** What an MCP client is set up with: the stdio command. */
    static QString clientCommand();

public slots:
    /** Start or stop to match the settings and the mode. */
    void refresh();

signals:
    /** Listening, stopped, a client came or went. */
    void stateChanged();

private slots:
    void slotModeChanged(Doc::Mode mode);
    void slotNewConnection();

private:
    void start();
    /** Close the socket and every connection, telling each @a why. */
    void stop(const QString &why);
    void readClient(QLocalSocket *socket);
    QJsonObject callTool(const QJsonValue &id, const QJsonObject &params);

    Doc *m_doc;
    QString m_name;
    /** A QPointer: the Operate audit may destroy it (AiResources). */
    QPointer<QLocalServer> m_server;
    QList<QPointer<QLocalSocket>> m_clients;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    QString m_problem;
    /** The last client's name (initialize's clientInfo), for the proposal. */
    QString m_clientName;
};

/** @} */

#endif
