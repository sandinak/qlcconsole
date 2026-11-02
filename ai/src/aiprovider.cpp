/*
  Q Light Controller Plus - qlcconsole
  aiprovider.cpp

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

#include <QJsonDocument>
#include <QJsonArray>

#include "aiprovider.h"

namespace
{
QString serverError(const QJsonObject &root)
{
    const QJsonValue e = root.value(QStringLiteral("error"));
    if (e.isString())
        return e.toString();
    if (e.isObject())
        return e.toObject().value(QStringLiteral("message")).toString();
    return QString();
}

bool parseObject(const QByteArray &reply, QJsonObject *root, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        *error = AiSettings::tr("The server's answer isn't JSON");
        return false;
    }
    *root = doc.object();
    const QString err = serverError(*root);
    if (!err.isEmpty())
    {
        *error = err;
        return false;
    }
    return true;
}
}

std::unique_ptr<AiProvider> AiProvider::create(const AiSettings &settings, const QString &apiKey)
{
    if (settings.provider == QLatin1String("anthropic"))
        return std::unique_ptr<AiProvider>(new AiAnthropicProvider(settings, apiKey));
    return std::unique_ptr<AiProvider>(new AiOpenAiProvider(settings, apiKey));
}

AiProvider::AiProvider(const AiSettings &settings, const QString &apiKey)
    : m_settings(settings)
    , m_apiKey(apiKey)
{
}

QUrl AiProvider::url(const QString &path) const
{
    QString base = m_settings.endpoint.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    QString p = path;
    if (base.endsWith(QLatin1String("/v1")) && p.startsWith(QLatin1String("/v1/")))
        p = p.mid(3);
    QUrl u(base + p);
    // "localhost" may resolve to ::1 first, where Ollama (127.0.0.1 only)
    // refuses or Qt stalls before falling back: say what is meant.
    if (u.host().compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0)
        u.setHost(QStringLiteral("127.0.0.1"));
    return u;
}

QNetworkRequest AiProvider::jsonRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(m_settings.timeoutSec * 1000);
    // No redirects: a key must not follow one somewhere else.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return req;
}

/****************************************************************************
 * OpenAI-compatible (and Ollama)
 ****************************************************************************/

AiOpenAiProvider::AiOpenAiProvider(const AiSettings &settings, const QString &apiKey)
    : AiProvider(settings, apiKey)
{
}

QNetworkRequest AiOpenAiProvider::chatRequest() const
{
    QNetworkRequest req = jsonRequest(url(QStringLiteral("/v1/chat/completions")));
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    return req;
}

QByteArray AiOpenAiProvider::chatBody(const QString &system, const QList<AiMessage> &messages,
                                      const QList<AiTool> &tools) const
{
    QJsonArray msgs;
    if (!system.isEmpty())
        msgs.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("system") },
                                  { QStringLiteral("content"), system } });
    for (const AiMessage &m : messages)
    {
        QJsonObject o;
        switch (m.role)
        {
            case AiMessage::User:
                o.insert(QStringLiteral("role"), QStringLiteral("user"));
                o.insert(QStringLiteral("content"), m.text);
            break;
            case AiMessage::Assistant:
            {
                o.insert(QStringLiteral("role"), QStringLiteral("assistant"));
                o.insert(QStringLiteral("content"), m.text);
                QJsonArray calls;
                for (const AiToolCall &c : m.calls)
                    calls.append(QJsonObject {
                        { QStringLiteral("id"), c.id },
                        { QStringLiteral("type"), QStringLiteral("function") },
                        { QStringLiteral("function"),
                          QJsonObject { { QStringLiteral("name"), c.name },
                                        { QStringLiteral("arguments"),
                                          QString::fromUtf8(QJsonDocument(c.args).toJson(QJsonDocument::Compact)) } } } });
                if (!calls.isEmpty())
                    o.insert(QStringLiteral("tool_calls"), calls);
            }
            break;
            case AiMessage::Tool:
                o.insert(QStringLiteral("role"), QStringLiteral("tool"));
                o.insert(QStringLiteral("tool_call_id"), m.toolCallId);
                o.insert(QStringLiteral("name"), m.toolName);
                o.insert(QStringLiteral("content"), m.text);
            break;
        }
        msgs.append(o);
    }
    QJsonArray toolList;
    for (const AiTool &t : tools)
        toolList.append(QJsonObject {
            { QStringLiteral("type"), QStringLiteral("function") },
            { QStringLiteral("function"), QJsonObject { { QStringLiteral("name"), t.name },
                                                        { QStringLiteral("description"), t.description },
                                                        { QStringLiteral("parameters"), t.schema } } } });
    QJsonObject body {
        { QStringLiteral("model"), m_settings.model },
        { QStringLiteral("messages"), msgs },
        { QStringLiteral("stream"), false },
    };
    if (!toolList.isEmpty())
        body.insert(QStringLiteral("tools"), toolList);
    // Ollama: no thinking pass (an answer in seconds, not minutes).
    if (m_settings.provider == QLatin1String("ollama"))
        body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("none"));
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

AiTurn AiOpenAiProvider::parseChat(const QByteArray &reply) const
{
    AiTurn turn;
    QJsonObject root;
    if (!parseObject(reply, &root, &turn.error))
        return turn;
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty())
    {
        turn.error = AiSettings::tr("The server sent no answer");
        return turn;
    }
    const QJsonObject msg = choices.first().toObject().value(QStringLiteral("message")).toObject();
    turn.text = msg.value(QStringLiteral("content")).toString().trimmed();
    for (const QJsonValue &v : msg.value(QStringLiteral("tool_calls")).toArray())
    {
        const QJsonObject c = v.toObject();
        const QJsonObject fn = c.value(QStringLiteral("function")).toObject();
        AiToolCall call;
        call.id = c.value(QStringLiteral("id")).toString();
        call.name = fn.value(QStringLiteral("name")).toString();
        const QJsonValue args = fn.value(QStringLiteral("arguments"));
        if (args.isObject())
            call.args = args.toObject();
        else
            call.args = QJsonDocument::fromJson(args.toString().toUtf8()).object();
        if (call.id.isEmpty())
            call.id = QStringLiteral("call_%1").arg(turn.calls.count());
        turn.calls << call;
    }
    turn.ok = true;
    return turn;
}

QNetworkRequest AiOpenAiProvider::modelsRequest() const
{
    if (m_settings.provider == QLatin1String("ollama"))
        return jsonRequest(url(QStringLiteral("/api/tags")));
    QNetworkRequest req = jsonRequest(url(QStringLiteral("/v1/models")));
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    return req;
}

QStringList AiOpenAiProvider::parseModels(const QByteArray &reply, QString *error) const
{
    QJsonObject root;
    if (!parseObject(reply, &root, error))
        return QStringList();
    QStringList out;
    // Ollama: {"models":[{"name":…}]}; OpenAI: {"data":[{"id":…}]}
    for (const QJsonValue &v : root.value(QStringLiteral("models")).toArray())
        out << v.toObject().value(QStringLiteral("name")).toString();
    for (const QJsonValue &v : root.value(QStringLiteral("data")).toArray())
        out << v.toObject().value(QStringLiteral("id")).toString();
    out.removeAll(QString());
    return out;
}

/****************************************************************************
 * Anthropic Messages
 ****************************************************************************/

AiAnthropicProvider::AiAnthropicProvider(const AiSettings &settings, const QString &apiKey)
    : AiProvider(settings, apiKey)
{
}

QNetworkRequest AiAnthropicProvider::chatRequest() const
{
    QNetworkRequest req = jsonRequest(url(QStringLiteral("/v1/messages")));
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    return req;
}

QByteArray AiAnthropicProvider::chatBody(const QString &system, const QList<AiMessage> &messages,
                                         const QList<AiTool> &tools) const
{
    // Tool results go back as a user message of tool_result blocks; the
    // results of one turn's calls share that one message.
    QJsonArray msgs;
    QJsonArray pendingResults;
    auto flushResults = [&]() {
        if (pendingResults.isEmpty())
            return;
        msgs.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("user") },
                                  { QStringLiteral("content"), pendingResults } });
        pendingResults = QJsonArray();
    };
    for (const AiMessage &m : messages)
    {
        if (m.role == AiMessage::Tool)
        {
            pendingResults.append(QJsonObject { { QStringLiteral("type"), QStringLiteral("tool_result") },
                                                { QStringLiteral("tool_use_id"), m.toolCallId },
                                                { QStringLiteral("content"), m.text } });
            continue;
        }
        flushResults();
        if (m.role == AiMessage::User)
        {
            msgs.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("user") },
                                      { QStringLiteral("content"), m.text } });
            continue;
        }
        QJsonArray blocks;
        if (!m.text.isEmpty())
            blocks.append(QJsonObject { { QStringLiteral("type"), QStringLiteral("text") },
                                        { QStringLiteral("text"), m.text } });
        for (const AiToolCall &c : m.calls)
            blocks.append(QJsonObject { { QStringLiteral("type"), QStringLiteral("tool_use") },
                                        { QStringLiteral("id"), c.id },
                                        { QStringLiteral("name"), c.name },
                                        { QStringLiteral("input"), c.args } });
        msgs.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("assistant") },
                                  { QStringLiteral("content"), blocks } });
    }
    flushResults();

    QJsonArray toolList;
    for (const AiTool &t : tools)
        toolList.append(QJsonObject { { QStringLiteral("name"), t.name },
                                      { QStringLiteral("description"), t.description },
                                      { QStringLiteral("input_schema"), t.schema } });
    QJsonObject body {
        { QStringLiteral("model"), m_settings.model },
        { QStringLiteral("max_tokens"), 2048 },
        { QStringLiteral("messages"), msgs },
    };
    if (!system.isEmpty())
        body.insert(QStringLiteral("system"), system);
    if (!toolList.isEmpty())
        body.insert(QStringLiteral("tools"), toolList);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

AiTurn AiAnthropicProvider::parseChat(const QByteArray &reply) const
{
    AiTurn turn;
    QJsonObject root;
    if (!parseObject(reply, &root, &turn.error))
        return turn;
    QStringList texts;
    for (const QJsonValue &v : root.value(QStringLiteral("content")).toArray())
    {
        const QJsonObject b = v.toObject();
        const QString type = b.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("text"))
            texts << b.value(QStringLiteral("text")).toString();
        else if (type == QLatin1String("tool_use"))
        {
            AiToolCall call;
            call.id = b.value(QStringLiteral("id")).toString();
            call.name = b.value(QStringLiteral("name")).toString();
            call.args = b.value(QStringLiteral("input")).toObject();
            turn.calls << call;
        }
    }
    turn.text = texts.join(QLatin1Char('\n')).trimmed();
    turn.ok = true;
    return turn;
}

QNetworkRequest AiAnthropicProvider::modelsRequest() const
{
    QNetworkRequest req = jsonRequest(url(QStringLiteral("/v1/models")));
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    return req;
}

QStringList AiAnthropicProvider::parseModels(const QByteArray &reply, QString *error) const
{
    QJsonObject root;
    if (!parseObject(reply, &root, error))
        return QStringList();
    QStringList out;
    for (const QJsonValue &v : root.value(QStringLiteral("data")).toArray())
        out << v.toObject().value(QStringLiteral("id")).toString();
    out.removeAll(QString());
    return out;
}
