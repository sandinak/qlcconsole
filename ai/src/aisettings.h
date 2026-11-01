/*
  Q Light Controller Plus - qlcconsole
  aisettings.h

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

#ifndef AISETTINGS_H
#define AISETTINGS_H

#include <QCoreApplication>
#include <QString>
#include <QUrl>
#include <functional>

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * AI Assist's settings (docs/design/AI_ASSIST.md section 5): this
 * computer's, in QSettings group "ai/". Never an API key: those go in the
 * keychain (AiKeychain).
 */
struct AiSettings
{
    Q_DECLARE_TR_FUNCTIONS(AiSettings)

public:
    bool enabled = false;
    QString provider = QStringLiteral("ollama");     //!< ollama | openai-compatible | anthropic
    QString endpoint = QStringLiteral("http://localhost:11434");
    QString model = QStringLiteral("qwen3.6:35b-a3b-mtp-q4_K_M");
    QString sendShowContext = QStringLiteral("summary");    //!< none | summary
    int timeoutSec = 60;

    static AiSettings load();
    void save() const;

    static QStringList providers();
    /** "Ollama (this computer or the network)" */
    static QString providerName(const QString &provider);
    /** The endpoint a provider starts with. */
    static QString defaultEndpoint(const QString &provider);

    /** A provider whose model runs somewhere else (the show context leaves
     *  this computer): anthropic, or any endpoint not on this computer. */
    bool sendsOffMachine() const;
    /** Ollama on this computer (localhost, 127.x, ::1): what Operate can
     *  ask to unload its model (section 5a step 3). */
    bool isLocalOllama() const;
    /** Whether @a url's host is this computer. */
    static bool isLoopback(const QUrl &url);

    bool operator==(const AiSettings &o) const;
    bool operator!=(const AiSettings &o) const { return !(*this == o); }
};

/**
 * API keys for the cloud providers, in the OS keychain -- never QSettings
 * or a plist. macOS: the login keychain (service "qlcconsole AI Assist",
 * one account per provider). Elsewhere (no keychain wired up yet): the
 * environment (QLC_AI_API_KEY, else ANTHROPIC_API_KEY / OPENAI_API_KEY),
 * never saved. A key is never logged.
 */
class AiKeychain
{
public:
    static QString read(const QString &provider);
    /** Store @a key for @a provider (empty: remove it). False when this
     *  platform can't store one. */
    static bool write(const QString &provider, const QString &key);
    /** Whether keys can be stored here (macOS). */
    static bool canStore();

    /** For the tests: keys go to @a read / @a write instead of the real
     *  keychain. Unset both to go back. */
    typedef std::function<QString(const QString &)> Reader;
    typedef std::function<bool(const QString &, const QString &)> Writer;
    static void setBackend(const Reader &read, const Writer &write);
};

/** @} */

#endif
