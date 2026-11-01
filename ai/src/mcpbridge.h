/*
  Q Light Controller Plus - qlcconsole
  mcpbridge.h

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

#ifndef MCPBRIDGE_H
#define MCPBRIDGE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QList>

class QLocalSocket;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * `qlcconsole --mcp` (docs/design/AI_ASSIST.md section 8 P3): what an MCP
 * client (Claude Code, Claude Desktop, a local agent) starts. It opens no
 * window and holds no show: it carries the client's JSON-RPC lines from
 * stdin to the running qlcconsole's MCP server (McpServer, a local socket:
 * no network port) and the answers back to stdout, one JSON message per
 * line (the MCP stdio transport).
 *
 * When nothing is listening (qlcconsole isn't running, is in Operate, or
 * its MCP server is off) it answers each request itself with an error
 * that says so, and tries again on the next one: the client never hangs.
 * A request in flight when the server goes away is answered the same way.
 *
 * Linked into the app's executable alone (main/), so it uses nothing else
 * of AI Assist.
 */
class McpBridge : public QObject
{
    Q_OBJECT

public:
    explicit McpBridge(const QString &serverName, QObject *parent = nullptr);
    ~McpBridge();

    /** One line from the client (a JSON-RPC message, no newline). */
    void send(const QByteArray &line);

    /** Connected to the app's server now. */
    bool isConnected() const;

    /** The local socket's name: QLC_MCP_SOCKET when set (the tests), else
     *  this user's (macOS: in the per-user temp folder; Linux: the user's
     *  runtime folder; Windows: a named pipe). */
    static QString defaultServerName();

    /** Whether @a argv asks for the bridge (--mcp). */
    static bool wanted(int argc, char **argv);
    /** Run the bridge on stdin/stdout until stdin closes. Not called main:
     *  Qt renames main to qMain on Windows. */
    static int cliMain(int argc, char **argv);

signals:
    /** One line for the client (no newline). */
    void output(const QByteArray &line);

private:
    bool connectNow();
    void readServer();
    void serverGone();
    /** An error answer to request @a id, as the client reads it. */
    void answerError(const QByteArray &id, int code, const QString &message);
    QString unavailable() const;

    QString m_name;
    QLocalSocket *m_socket;
    QByteArray m_buffer;
    /** The ids (compact JSON) of the requests the server hasn't answered. */
    QList<QByteArray> m_pending;
    /** Why the server last said it was going (Operate, turned off). */
    QString m_reason;
};

/** @} */

#endif
