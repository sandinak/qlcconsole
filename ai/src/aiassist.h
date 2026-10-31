/*
  Q Light Controller Plus - qlcconsole
  aiassist.h

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

#ifndef AIASSIST_H
#define AIASSIST_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QList>

#include "showtreecommand.h"
#include "aiprovider.h"
#include "aisettings.h"
#include "doc.h"

class QNetworkAccessManager;
class QNetworkReply;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * Every object AI Assist makes that could hold a socket, a thread or a
 * timer is tracked here, so the teardown audit on entering Operate
 * (docs/design/AI_ASSIST.md section 5a step 4) can tell whether anything
 * is still alive.
 */
class AiResources
{
public:
    /** Track @a obj under @a label ("network", "reply") until it dies. */
    static void track(QObject *obj, const QString &label);
    /** What is still alive, "label" each. */
    static QStringList live();
    /** Delete everything still alive (the audit's force-terminate). */
    static void destroyAll();
};

/**
 * AI Assist (docs/design/AI_ASSIST.md): a Design-mode helper that answers
 * questions about the show by calling the Operation Registry's read-only
 * operations (P1). The model proposes, the console executes; in P1 there
 * is nothing to execute but reads.
 *
 * - Off by default (AiSettings::enabled): off, nothing is made and nothing
 *   is sent.
 * - Lazy: the network manager is made on the first question.
 * - All I/O asynchronous on the GUI thread; never on MasterTimer or a
 *   universe thread (the guard checks both).
 * - Operate (section 5a): entering it aborts what is in flight, destroys
 *   the network manager, asks a local Ollama to unload the model, then
 *   audits; anything left is destroyed and reported (resourceError()),
 *   never holding the switch up. A question in Operate gets an error and
 *   sends nothing.
 *
 * Owned by App (one per Doc); instance() is it, for the command line and
 * the settings page.
 */
class AiAssist : public QObject
{
    Q_OBJECT

public:
    explicit AiAssist(Doc *doc, QObject *parent = nullptr);
    ~AiAssist();

    static AiAssist *instance();

    /** What a question came to. */
    struct Answer
    {
        bool ok = false;
        QString question;
        QString text;               //!< the model's answer (or why it failed)
        /** The operations it ran, as command-line text ("conflicts"):
         *  each could be typed instead. */
        QStringList commands;
        /** A tool call that isn't a read-only operation of the registry:
         *  refused and shown as plain text. */
        QStringList rejected;
    };

    /** AiSettings::enabled, from QSettings (read each time). */
    static bool isEnabled();
    /** Whether a question can be asked now: enabled, in Design. */
    bool isAvailable() const;
    /** The provider didn't answer last time (greyed, with why()). */
    bool isUnreachable() const { return !m_unreachable.isEmpty(); }
    QString unreachableReason() const { return m_unreachable; }
    bool isBusy() const { return m_reply != nullptr; }

    /** Is @a line ("? find address conflicts") a question for the
     *  assistant? Only with AI Assist on; "?" alone and "? <word>" of the
     *  grammar stay the command line's help. */
    static bool isAssistantLine(const QString &line);

    /** Ask @a question. Returns false (and @a error) when it can't be
     *  asked: off, Operate, already asking. Otherwise answered() follows. */
    bool ask(const QString &question, const ShowTreeCommand::Context &selection, QString *error = nullptr);
    /** Esc: drop the question in flight (answered() with "Cancelled"). */
    void cancel();

    /** Settings ▸ AI Assist ▸ Test connection, with @a settings as typed:
     *  connectionTested() follows. False when it can't be tried now. */
    bool testConnection(const AiSettings &settings, const QString &apiKey);

    /** The resource error raised by the last Operate audit (empty: none). */
    QString resourceErrorText() const { return m_resourceError; }

    /** The runtime guard (section 5a): false -- and a logged error, and in
     *  a debug build an assert -- when AI code runs in Operate or off the
     *  GUI thread. */
    bool checkGuard(const char *where) const;
    static int guardViolations();
    /** The tests turn the debug assert off to see the guard fire. */
    static void setAssertOnViolation(bool on);

    /** The system prompt (and, with "summary", the show context). */
    QString systemPrompt(const ShowTreeCommand::Context &selection) const;
    /** The tools offered: the registry's read-only operations. */
    static QList<AiTool> tools();

    /** At most this many rounds of tool calls per question. */
    static const int MaxRounds = 5;

signals:
    void answered(const AiAssist::Answer &answer);
    void busyChanged(bool busy);
    /** Enabled, Design/Operate, reachable: the entry points redraw. */
    void availabilityChanged();
    void connectionTested(bool ok, const QStringList &models, const QString &message);
    /** The Operate audit found something still alive (section 5a). */
    void resourceError(const QString &message);
    void resourceErrorCleared();
    /** The Operate teardown finished (audit done) -- for the tests. */
    void teardownFinished(bool clean);

public slots:
    /** The settings changed (the settings page's OK). */
    void settingsChanged();

private slots:
    void slotModeChanged(Doc::Mode mode);

private:
    QNetworkAccessManager *network();
    void sendRound();
    void handleReply();
    void finish(bool ok, const QString &text);
    void abortAll();
    void teardown();
    void audit(bool modelResident, const QString &extra);

    Doc *m_doc;
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_testReply;
    std::unique_ptr<AiProvider> m_provider;
    AiSettings m_settings;
    QList<AiMessage> m_messages;
    QString m_system;
    int m_round = 0;
    Answer m_answer;
    ShowTreeCommand::Context m_selection;
    QString m_unreachable;
    QString m_resourceError;
    /** A question went to the model this Design session: Operate asks a
     *  local Ollama to unload it. */
    bool m_usedModel = false;
    bool m_operate = false;
};

Q_DECLARE_METATYPE(AiAssist::Answer)

/** @} */

#endif
