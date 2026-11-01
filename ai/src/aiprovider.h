/*
  Q Light Controller Plus - qlcconsole
  aiprovider.h

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

#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QNetworkRequest>
#include <QJsonObject>
#include <QStringList>
#include <QByteArray>
#include <QString>
#include <QList>
#include <memory>

#include "aisettings.h"

/** @addtogroup ai AI Assist
 * @{
 */

/** A tool call the model asked for: an operation of the registry. */
struct AiToolCall
{
    QString id;
    QString name;
    QJsonObject args;
};

/** One message of a conversation, the same for every provider. */
struct AiMessage
{
    enum Role { User, Assistant, Tool };
    Role role = User;
    QString text;
    QList<AiToolCall> calls;        //!< Assistant: what it asked for
    QString toolCallId;             //!< Tool: the call it answers
    QString toolName;
};

/** What the model said back. */
struct AiTurn
{
    bool ok = false;
    QString error;                  //!< !ok: why (the server's message)
    QString text;
    QList<AiToolCall> calls;
};

/** A tool offered to the model (an operation's name, what it does, its
 *  JSON schema). */
struct AiTool
{
    QString name;
    QString description;
    QJsonObject schema;
};

/**
 * A provider adapter (docs/design/AI_ASSIST.md section 6): how a
 * conversation and the tools become a request, and how the reply becomes
 * a turn. Pure: no network here (AiAssist sends), so each adapter is
 * tested on recorded bodies.
 *
 * Two adapters: OpenAI-compatible chat completions with tools (Ollama --
 * its /v1 endpoint -- and most local servers) and Anthropic Messages.
 */
class AiProvider
{
public:
    virtual ~AiProvider() = default;

    /** The adapter for @a settings, with @a apiKey (cloud providers). */
    static std::unique_ptr<AiProvider> create(const AiSettings &settings, const QString &apiKey);

    virtual QNetworkRequest chatRequest() const = 0;
    virtual QByteArray chatBody(const QString &system, const QList<AiMessage> &messages,
                                const QList<AiTool> &tools) const = 0;
    virtual AiTurn parseChat(const QByteArray &reply) const = 0;

    /** "Test connection": the request that lists the models, and its
     *  parse (names; empty and @a error on failure). */
    virtual QNetworkRequest modelsRequest() const = 0;
    virtual QStringList parseModels(const QByteArray &reply, QString *error) const = 0;

    /** @a path under the endpoint ("/v1/chat/completions"), without a
     *  doubled "/v1". */
    QUrl url(const QString &path) const;

protected:
    AiProvider(const AiSettings &settings, const QString &apiKey);
    QNetworkRequest jsonRequest(const QUrl &url) const;

    AiSettings m_settings;
    QString m_apiKey;
};

class AiOpenAiProvider : public AiProvider
{
public:
    AiOpenAiProvider(const AiSettings &settings, const QString &apiKey);
    QNetworkRequest chatRequest() const override;
    QByteArray chatBody(const QString &system, const QList<AiMessage> &messages,
                        const QList<AiTool> &tools) const override;
    AiTurn parseChat(const QByteArray &reply) const override;
    QNetworkRequest modelsRequest() const override;
    QStringList parseModels(const QByteArray &reply, QString *error) const override;
};

class AiAnthropicProvider : public AiProvider
{
public:
    AiAnthropicProvider(const AiSettings &settings, const QString &apiKey);
    QNetworkRequest chatRequest() const override;
    QByteArray chatBody(const QString &system, const QList<AiMessage> &messages,
                        const QList<AiTool> &tools) const override;
    AiTurn parseChat(const QByteArray &reply) const override;
    QNetworkRequest modelsRequest() const override;
    QStringList parseModels(const QByteArray &reply, QString *error) const override;
};

/** @} */

#endif
