/*
  Q Light Controller Plus - qlcconsole
  mcpserver.cpp

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

#include <QCoreApplication>
#include <QJsonDocument>
#include <QDebug>

#include "operationregistry.h"
#include "mcpserver.h"
#include "mcpbridge.h"
#include "aisettings.h"
#include "aiassist.h"

namespace
{
McpServer *s_instance = nullptr;

/** A client line longer than this drops the connection. */
const int kMaxLine = 4 * 1024 * 1024;

QJsonObject rpcResult(const QJsonValue &id, const QJsonObject &result)
{
    return QJsonObject { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                         { QStringLiteral("id"), id },
                         { QStringLiteral("result"), result } };
}

QJsonObject rpcError(const QJsonValue &id, int code, const QString &message)
{
    return QJsonObject { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                         { QStringLiteral("id"), id },
                         { QStringLiteral("error"), QJsonObject { { QStringLiteral("code"), code },
                                                                  { QStringLiteral("message"), message } } } };
}

QJsonObject toolResult(const QString &text, bool isError)
{
    return QJsonObject { { QStringLiteral("content"),
                           QJsonArray { QJsonObject { { QStringLiteral("type"), QStringLiteral("text") },
                                                      { QStringLiteral("text"), text } } } },
                         { QStringLiteral("isError"), isError } };
}

/** The operations a client may see: read-only, then (with @a changes) the
 *  change operations. Never `command`. */
QList<const OperationRegistry::Operation *> offered(bool changes)
{
    QList<const OperationRegistry::Operation *> ops = OperationRegistry::readOnlyOperations();
    if (changes)
        ops += OperationRegistry::changeOperations();
    for (int i = ops.count() - 1; i >= 0; --i)
        if (ops.at(i)->name == QLatin1String("command"))
            ops.removeAt(i);
    return ops;
}
}

McpServer::McpServer(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_name(McpBridge::defaultServerName())
{
    connect(doc, &Doc::modeChanged, this, &McpServer::slotModeChanged);
    if (AiAssist *ai = AiAssist::instance())
        connect(ai, &AiAssist::availabilityChanged, this, &McpServer::refresh);
    s_instance = this;
    refresh();
}

McpServer::~McpServer()
{
    stop(tr("qlcconsole is closing"));
    if (s_instance == this)
        s_instance = nullptr;
}

McpServer *McpServer::instance()
{
    return s_instance;
}

void McpServer::setServerName(const QString &name)
{
    if (name == m_name)
        return;
    const bool was = m_server != nullptr;
    if (was)
        stop(tr("qlcconsole's MCP server moved"));
    m_name = name;
    refresh();
}

bool McpServer::shouldListen() const
{
    const AiSettings s = AiSettings::load();
    return s.enabled && s.mcpEnabled && m_doc->mode() == Doc::Design;
}

bool McpServer::isListening() const
{
    return m_server != nullptr && m_server->isListening();
}

int McpServer::connectionCount() const
{
    int n = 0;
    for (const QPointer<QLocalSocket> &s : m_clients)
        if (!s.isNull())
            ++n;
    return n;
}

QString McpServer::statusText() const
{
    const AiSettings s = AiSettings::load();
    if (!s.enabled || !s.mcpEnabled)
        return tr("Off: no AI tool outside qlcconsole can reach the show.");
    if (m_doc->mode() == Doc::Operate)
        return tr("Closed while in Operate mode; it opens again in Design.");
    if (!m_problem.isEmpty())
        return m_problem;
    if (isListening())
        return tr("Listening on this computer only (%n client(s) connected).", "", connectionCount());
    return tr("Not listening.");
}

QString McpServer::clientCommand()
{
    QString path = QCoreApplication::applicationFilePath();
    if (path.isEmpty())
        path = QStringLiteral("qlcconsole");
    if (path.contains(QLatin1Char(' ')))
        path = QLatin1Char('"') + path + QLatin1Char('"');
    return path + QStringLiteral(" --mcp");
}

QStringList McpServer::protocolVersions()
{
    return { QStringLiteral("2024-11-05"), QStringLiteral("2025-03-26"), QStringLiteral("2025-06-18") };
}

QJsonArray McpServer::tools(bool changes)
{
    QJsonArray out;
    for (const OperationRegistry::Operation *op : offered(changes))
    {
        QString description = op->description;
        if (!op->readOnly)
            description += QStringLiteral(" (A proposal: nothing changes until the operator applies it in "
                                          "qlcconsole; further calls add steps to the same proposal.)");
        out.append(QJsonObject {
            { QStringLiteral("name"), op->name },
            { QStringLiteral("title"), op->title },
            { QStringLiteral("description"), description },
            { QStringLiteral("inputSchema"), op->schema },
            { QStringLiteral("annotations"),
              QJsonObject { { QStringLiteral("title"), op->title },
                            { QStringLiteral("readOnlyHint"), op->readOnly },
                            { QStringLiteral("destructiveHint"), false },
                            { QStringLiteral("openWorldHint"), false } } } });
    }
    return out;
}

void McpServer::refresh()
{
    if (shouldListen())
    {
        if (m_server == nullptr)
            start();
    }
    else if (m_server != nullptr)
    {
        stop(m_doc->mode() == Doc::Operate
                 ? AiAssist::tr("AI Assist is unavailable in Operate mode")
                       + tr(": qlcconsole takes MCP requests again once it is back in Design")
                 : tr("qlcconsole's MCP server was turned off (Settings ▸ AI Assist)"));
    }
    else if (!m_problem.isEmpty())
    {
        m_problem.clear();
        emit stateChanged();
    }
}

void McpServer::slotModeChanged(Doc::Mode mode)
{
    Q_UNUSED(mode)
    // Synchronous: the socket is gone before the Operate audit looks.
    refresh();
}

void McpServer::start()
{
    m_problem.clear();
    // Another qlcconsole already serving this user? Leave it be.
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(200))
        {
            probe.abort();
            m_problem = tr("Another qlcconsole on this computer already has the MCP server; quit it, or turn "
                           "its server off, then turn this one off and on.");
            qWarning().noquote() << "MCP:" << m_problem;
            emit stateChanged();
            return;
        }
    }
    // A socket file left by a qlcconsole that crashed.
    QLocalServer::removeServer(m_name);
    m_server = new QLocalServer(this);
    // This user only.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    AiResources::track(m_server, QStringLiteral("MCP server"));
    connect(m_server, &QLocalServer::newConnection, this, &McpServer::slotNewConnection);
    if (!m_server->listen(m_name))
    {
        m_problem = tr("The MCP server couldn't open its socket: %1").arg(m_server->errorString());
        qWarning().noquote() << "MCP:" << m_problem;
        delete m_server.data();
        m_server = nullptr;
    }
    emit stateChanged();
}

void McpServer::stop(const QString &why)
{
    // Every connection hears why first (an MCP log message), so the bridge
    // can tell the client; then it goes, now.
    const QByteArray note = QJsonDocument(QJsonObject {
                                              { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                                              { QStringLiteral("method"), QStringLiteral("notifications/message") },
                                              { QStringLiteral("params"),
                                                QJsonObject { { QStringLiteral("level"), QStringLiteral("warning") },
                                                              { QStringLiteral("logger"), QStringLiteral("qlcconsole") },
                                                              { QStringLiteral("data"), why } } } })
                                .toJson(QJsonDocument::Compact)
                            + '\n';
    const QList<QPointer<QLocalSocket>> clients = m_clients;
    m_clients.clear();
    m_buffers.clear();
    for (const QPointer<QLocalSocket> &s : clients)
    {
        if (s.isNull())
            continue;
        s->disconnect(this);
        if (s->state() == QLocalSocket::ConnectedState)
        {
            s->write(note);
            s->flush();
            if (s->bytesToWrite() > 0)
                s->waitForBytesWritten(50);
        }
        s->abort();
        delete s.data();
    }
    const bool had = m_server != nullptr;
    if (m_server != nullptr)
    {
        m_server->close();
        delete m_server.data();
        m_server = nullptr;
    }
    if (had)
        emit stateChanged();
}

void McpServer::slotNewConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections())
    {
        QLocalSocket *s = m_server->nextPendingConnection();
        if (s == nullptr)
            break;
        s->setParent(this);
        AiResources::track(s, QStringLiteral("MCP connection"));
        m_clients << QPointer<QLocalSocket>(s);
        connect(s, &QLocalSocket::readyRead, this, [this, s]() { readClient(s); });
        connect(s, &QLocalSocket::disconnected, this, [this, s]() {
            m_buffers.remove(s);
            for (int i = m_clients.count() - 1; i >= 0; --i)
                if (m_clients.at(i).isNull() || m_clients.at(i).data() == s)
                    m_clients.removeAt(i);
            s->deleteLater();
            emit stateChanged();
        });
    }
    emit stateChanged();
}

void McpServer::readClient(QLocalSocket *socket)
{
    QPointer<QLocalSocket> s(socket);
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaxLine && !buffer.contains('\n'))
    {
        m_buffers.remove(socket);
        socket->abort();
        return;
    }
    QList<QByteArray> lines;
    int nl;
    while ((nl = buffer.indexOf('\n')) >= 0)
    {
        lines << buffer.left(nl).trimmed();
        buffer.remove(0, nl + 1);
    }
    for (const QByteArray &line : lines)
    {
        if (line.isEmpty())
            continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        QJsonObject answer;
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            answer = rpcError(QJsonValue(), -32700, tr("Not a JSON-RPC message"));
        else
            answer = handle(doc.object());
        // handle() can't end the connection, but the server may have
        // stopped meanwhile (it never runs anything that switches mode).
        if (s.isNull() || m_server == nullptr)
            return;
        if (!answer.isEmpty())
            s->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
    }
    if (!s.isNull())
        s->flush();
}

QJsonObject McpServer::handle(const QJsonObject &message)
{
    const QString method = message.value(QStringLiteral("method")).toString();
    if (method.isEmpty())
        return QJsonObject();               // a client's answer to us: none asked
    const bool notification = !message.contains(QStringLiteral("id"));
    const QJsonValue id = message.value(QStringLiteral("id"));
    if (notification)
        return QJsonObject();               // notifications/initialized, cancelled...

    // Never in Operate (section 5a), never with it off: nothing listens
    // then, and anything that got here anyway is refused, running nothing.
    if (m_doc->mode() == Doc::Operate)
        return rpcError(id, -32000, AiAssist::tr("AI Assist is unavailable in Operate mode"));
    const AiSettings settings = AiSettings::load();
    if (!settings.enabled || !settings.mcpEnabled)
        return rpcError(id, -32000, tr("qlcconsole's MCP server is off (Settings ▸ AI Assist)"));

    const QJsonObject params = message.value(QStringLiteral("params")).toObject();
    if (method == QLatin1String("initialize"))
    {
        const QString asked = params.value(QStringLiteral("protocolVersion")).toString();
        const QStringList known = protocolVersions();
        m_clientName = params.value(QStringLiteral("clientInfo")).toObject().value(QStringLiteral("name")).toString();
        return rpcResult(id, QJsonObject {
            { QStringLiteral("protocolVersion"), known.contains(asked) ? asked : known.last() },
            { QStringLiteral("capabilities"),
              QJsonObject { { QStringLiteral("tools"), QJsonObject { { QStringLiteral("listChanged"), false } } },
                            { QStringLiteral("logging"), QJsonObject() } } },
            { QStringLiteral("serverInfo"),
              QJsonObject { { QStringLiteral("name"), QStringLiteral("qlcconsole") },
                            { QStringLiteral("title"), QStringLiteral("qlcconsole") },
                            { QStringLiteral("version"), QCoreApplication::applicationVersion() } } },
            { QStringLiteral("instructions"),
              tr("Tools over the lighting show open in qlcconsole (fixtures, groups, functions, the Show Tree's "
                 "cues). The read-only tools answer at once. The change tools, when the operator allows them, "
                 "only propose: nothing changes until the operator applies the proposal in qlcconsole, every "
                 "step together as one undo step. Use only names the show has; ask the read-only tools first. "
                 "Nothing works while qlcconsole is in Operate mode.") } });
    }
    if (method == QLatin1String("ping"))
        return rpcResult(id, QJsonObject());
    if (method == QLatin1String("tools/list"))
        return rpcResult(id, QJsonObject { { QStringLiteral("tools"), tools(settings.mcpChanges) } });
    if (method == QLatin1String("tools/call"))
        return callTool(id, params);
    if (method == QLatin1String("logging/setLevel"))
        return rpcResult(id, QJsonObject());
    if (method == QLatin1String("resources/list"))
        return rpcResult(id, QJsonObject { { QStringLiteral("resources"), QJsonArray() } });
    if (method == QLatin1String("prompts/list"))
        return rpcResult(id, QJsonObject { { QStringLiteral("prompts"), QJsonArray() } });
    return rpcError(id, -32601, tr("Unknown method: %1").arg(method));
}

QJsonObject McpServer::callTool(const QJsonValue &id, const QJsonObject &params)
{
    const AiSettings settings = AiSettings::load();
    const QString name = params.value(QStringLiteral("name")).toString();
    const QJsonValue a = params.value(QStringLiteral("arguments"));
    if (!a.isUndefined() && !a.isNull() && !a.isObject())
        return rpcError(id, -32602, tr("arguments must be an object"));
    const QJsonObject args = a.toObject();

    const OperationRegistry::Operation *op = nullptr;
    for (const OperationRegistry::Operation *o : offered(settings.mcpChanges))
        if (o->name == name)
            op = o;
    if (op == nullptr)
        return rpcError(id, -32602, tr("Unknown tool: %1").arg(name));

    if (op->readOnly)
    {
        OperationRegistry reg(m_doc);
        const ShowTreeCommand::Context selection = AiAssist::instance() != nullptr
                                                       ? AiAssist::instance()->currentSelection()
                                                       : ShowTreeCommand::Context();
        const OperationRegistry::Result r = reg.run(op->name, args, selection);
        if (!r.ok)
            return rpcResult(id, toolResult(r.message, true));
        QString text = r.message;
        if (!r.text.isEmpty())
            text += (text.isEmpty() ? QString() : QStringLiteral("\n")) + r.text;
        return rpcResult(id, toolResult(text, false));
    }

    // A change: a proposal the operator sees in qlcconsole, never run here.
    AiAssist *ai = AiAssist::instance();
    if (ai == nullptr)
        return rpcResult(id, toolResult(tr("AI Assist isn't running in qlcconsole"), true));
    QString why;
    const int step = ai->proposeExternal(op->name, args, m_clientName, &why);
    if (step == 0)
        return rpcResult(id, toolResult(tr("Not proposed: %1").arg(why), true));
    const AiAssist::Step st = ai->proposal().at(step - 1);   // proposal() is by value: copy, don't bind
    return rpcResult(id, toolResult(
        tr("Proposed as step %1: %2 (%3). Nothing has changed yet: the operator applies every proposed step "
           "together, as one undo step, in qlcconsole's Show Tree command line (Enter applies, Esc "
           "discards). Send any further steps the same way; check the result afterwards with the "
           "read-only tools.").arg(step).arg(st.command, st.description),
        false));
}
