/*
  Q Light Controller Plus - qlcconsole
  mcpbridge.cpp

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
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QDir>

#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

#include "mcpbridge.h"

namespace
{
/** A line longer than this from the server is dropped with the connection. */
const int kMaxLine = 16 * 1024 * 1024;

QString userName()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = qEnvironmentVariable("USERNAME");
    QString out;
    for (const QChar c : user)
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out += c;
    return out.isEmpty() ? QStringLiteral("user") : out;
}

/** A JSON-RPC id as it is written ("7", "\"a\""). */
QByteArray idText(const QJsonValue &id)
{
    return QJsonDocument(QJsonObject { { QStringLiteral("id"), id } }).toJson(QJsonDocument::Compact)
        .mid(6).chopped(1);
}
}

McpBridge::McpBridge(const QString &serverName, QObject *parent)
    : QObject(parent)
    , m_name(serverName)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::readyRead, this, &McpBridge::readServer);
    connect(m_socket, &QLocalSocket::disconnected, this, &McpBridge::serverGone);
}

McpBridge::~McpBridge()
{
    m_socket->disconnect(this);
}

QString McpBridge::defaultServerName()
{
    const QString env = qEnvironmentVariable("QLC_MCP_SOCKET");
    if (!env.isEmpty())
        return env;
#if defined(Q_OS_WIN)
    // A named pipe: Qt's QLocalServer refuses clients from other computers.
    return QStringLiteral("qlcconsole-mcp-%1").arg(userName());
#else
    // A folder only this user can write to, so nobody else can put a socket
    // of theirs where the bridge looks (macOS: $TMPDIR is per user; Linux:
    // $XDG_RUNTIME_DIR, 0700). A Unix socket path is short (~104 bytes).
# if defined(Q_OS_MACOS)
    QString dir = QDir::tempPath();
# else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
# endif
    QString path = QDir(dir).filePath(QStringLiteral("qlcconsole-mcp.sock"));
    if (dir.isEmpty() || path.toLocal8Bit().size() > 100)
        path = QDir(QDir::tempPath()).filePath(QStringLiteral("qlcconsole-mcp-%1.sock").arg(userName()));
    return path;
#endif
}

bool McpBridge::wanted(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], "--mcp") == 0)
            return true;
    return false;
}

bool McpBridge::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState;
}

bool McpBridge::connectNow()
{
    if (isConnected())
        return true;
    m_socket->abort();
    m_buffer.clear();
    m_socket->connectToServer(m_name);
    if (!m_socket->waitForConnected(2000))
    {
        m_socket->abort();
        return false;
    }
    m_reason.clear();
    return true;
}

QString McpBridge::unavailable() const
{
    if (!m_reason.isEmpty())
        return m_reason;
    return tr("qlcconsole isn't taking MCP requests: start it with the show open, in Design mode, with "
              "Settings ▸ AI Assist ▸ \"Let AI tools on this computer use the show\" on");
}

void McpBridge::answerError(const QByteArray &id, int code, const QString &message)
{
    const QByteArray line = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"error\":"
                            + QJsonDocument(QJsonObject { { QStringLiteral("code"), code },
                                                          { QStringLiteral("message"), message } })
                                  .toJson(QJsonDocument::Compact)
                            + "}";
    emit output(line);
}

void McpBridge::send(const QByteArray &raw)
{
    QByteArray line = raw.trimmed();
    if (line.isEmpty())
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        answerError("null", -32700, tr("Not a JSON-RPC message"));
        return;
    }
    const QJsonObject msg = doc.object();
    const bool request = msg.contains(QStringLiteral("method")) && msg.contains(QStringLiteral("id"));
    const QByteArray id = request ? idText(msg.value(QStringLiteral("id"))) : QByteArray();
    if (!connectNow())
    {
        if (!request)
            return;                         // a notification: nobody to tell
        if (msg.value(QStringLiteral("method")).toString() == QLatin1String("ping"))
            emit output("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":{}}");
        else
            answerError(id, -32000, unavailable());
        return;
    }
    if (request)
        m_pending << id;
    // One message a line: re-encoded compact, so an embedded newline can't
    // split it.
    m_socket->write(doc.toJson(QJsonDocument::Compact) + '\n');
    m_socket->flush();
}

void McpBridge::readServer()
{
    m_buffer += m_socket->readAll();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0)
    {
        const QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer.remove(0, nl + 1);
        if (line.isEmpty())
            continue;
        const QJsonObject msg = QJsonDocument::fromJson(line).object();
        if (msg.contains(QStringLiteral("id")) && !msg.contains(QStringLiteral("method")))
        {
            m_pending.removeOne(idText(msg.value(QStringLiteral("id"))));
        }
        // The server's word on why it is going: what later refusals say.
        if (msg.value(QStringLiteral("method")).toString() == QLatin1String("notifications/message"))
        {
            const QJsonObject p = msg.value(QStringLiteral("params")).toObject();
            if (p.value(QStringLiteral("logger")).toString() == QLatin1String("qlcconsole"))
                m_reason = p.value(QStringLiteral("data")).toString();
        }
        emit output(line);
    }
    if (m_buffer.size() > kMaxLine)
    {
        m_buffer.clear();
        m_socket->abort();
    }
}

void McpBridge::serverGone()
{
    m_buffer.clear();
    const QList<QByteArray> pending = m_pending;
    m_pending.clear();
    for (const QByteArray &id : pending)
        answerError(id, -32000, unavailable());
}

int McpBridge::cliMain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString name = defaultServerName();
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--mcp-socket") == 0)
            name = QString::fromLocal8Bit(argv[i + 1]);

#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    McpBridge bridge(name);
    QObject::connect(&bridge, &McpBridge::output, &app, [](const QByteArray &line) {
        std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    });

    // stdin is read on a thread of its own (a blocking read works the same
    // on every platform); each line goes to the bridge on this one.
    std::thread reader([&bridge, &app]() {
        std::string line;
        while (std::getline(std::cin, line))
        {
            const QByteArray bytes = QByteArray::fromStdString(line);
            QMetaObject::invokeMethod(&bridge, [&bridge, bytes]() { bridge.send(bytes); }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    const int rc = app.exec();
    reader.join();
    return rc;
}
