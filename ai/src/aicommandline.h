/*
  Q Light Controller Plus - qlcconsole
  aicommandline.h

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

#ifndef AICOMMANDLINE_H
#define AICOMMANDLINE_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <functional>

#include "showtreecommand.h"
#include "aiassist.h"

class OperationRegistry;
class QAction;
class QTimer;
class QTextBrowser;
class QLineEdit;
class QLabel;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * AI Assist in the Show Tree's command line and Build's command bar
 * (docs/design/AI_ASSIST.md section 7): one line, no button. A line that
 * is a command runs as typed; one that isn't and reads as plain words
 * (ShowTreeCommand::classifyFailure(); a typo gets the local spell-check
 * instead) goes to the assistant with the parser's error (fallback())
 * while AI Assist is on and the console is in Design; a line starting "? "
 * is always a question. The answer shows in the line's help pane with the
 * commands the assistant ran, each a link that puts it in the line (the
 * operator learns the syntax). The line's placeholder says when the
 * fallback is on. Esc cancels a question in flight.
 *
 * A change the assistant proposes (P2) shows in the pane as a checklist --
 * each step its command-line text and a sentence, a link to leave it out
 * -- with Apply and Discard; a single step also goes in the line. Enter
 * (on that line, or an empty one) applies it, Esc discards it, typing
 * anything else drops it. Applied, it is one undo step; the selection
 * goes where the plan left it (applied()).
 *
 * While a question is in flight the line shows it: a turning spinner in
 * the line (a click on it cancels) and, in the message, "Asking AI
 * Assist… Esc cancels" with the seconds since it was sent once that is
 * over two. The line stays usable meanwhile: a command typed runs as
 * usual (its message replaces the busy text; the spinner keeps turning),
 * and another line for the assistant replaces the one in flight -- the
 * newest is what the operator wants. The answer, an error, the timeout or
 * Esc stops it. Only the line that asked shows the answer (Build's bar
 * and the Show Tree's share one assistant).
 *
 * The view only creates it and hands it each line first; everything else
 * is here, so the module stays separable (QLC_AI_ASSIST).
 */
class AiCommandLine : public QObject
{
    Q_OBJECT

public:
    AiCommandLine(QLineEdit *line, QTextBrowser *pane, QLabel *message, QObject *parent);

    /** @a line is a question: ask it (or say why not) and return true.
     *  False: not a question, the command line runs it. */
    bool handle(const QString &line, const ShowTreeCommand::Context &selection);
    /** @a line wasn't a command (@a parserError says why) and reads as
     *  plain words: ask the assistant, with the error, the syntax
     *  reference and @a selection; @a fix, the spell-check's guess, waits
     *  in the pane as a link. One in flight is replaced. True: asked (answered() follows;
     *  the answer is a corrected command as a proposal, or words). False
     *  and @a note: it wasn't asked; @a note, when set, is a sentence for
     *  the line's message (AI Assist is off: where to turn it on). The
     *  view calls this in Design only. */
    bool fallback(const QString &line, const QString &parserError, const ShowTreeCommand::Context &selection,
                  QString *note = nullptr, const QString &fix = QString());
    /** In flight: a question this line asked. */
    bool isAsking() const;
    /** The busy indicator: the spinner in the line is showing. */
    bool isBusyShown() const;
    /** Seconds since the question in flight was sent (-1: none). */
    int elapsedSeconds() const;
    /** Esc: cancel a question in flight, else discard a proposal. False
     *  when there was neither. */
    bool cancel();

    /** The command line's registry: a proposal runs through it (its
     *  grammar, with the view's choosers). Unset: AI Assist's own. */
    void setOperations(OperationRegistry *registry) { m_registry = registry; }

    /** Where the view's selection is now: what an MCP client's calls run
     *  with (AiAssist::currentSelection()). */
    void setSelectionProvider(const std::function<ShowTreeCommand::Context()> &provider);

    /** Apply the proposal's included steps (Apply, Enter). */
    bool applyProposal();
    /** Leave step @a index out (or back in). */
    void toggleStep(int index);
    /** Which steps are included. */
    QList<bool> included() const { return m_included; }

    /** Build's bar: a proposal is applied by running each included step's
     *  command through @a runner (the bar's own line, which puts it on the
     *  scene), in order, stopping at the first that fails. Unset: one plan
     *  through the registry, one undo step (the Show Tree). */
    void setLineRunner(const std::function<bool(const QString &command)> &runner) { m_lineRunner = runner; }

signals:
    /** A proposal was applied: where it leaves the selection. */
    void applied(const ShowTreeCommand::Result &result);

public:
    /** The line takes plain words (fallback()): its placeholder says so; else
     *  only "? asks AI Assist" (Build's command bar). */
    void setPlainWords(bool on);
    /** Redraw the line's placeholder (AI fallback on or off). */
    void update();
    /** The view's own placeholder changed (the selected layer): AI
     *  Assist's hint goes on its end. */
    void setBasePlaceholder(const QString &text);

private:
    /** AiAssist::instance(), its signals connected once. */
    AiAssist *assist();
    void showAnswer(const AiAssist::Answer &answer);
    void showProposal();
    void proposalChanged();
    void setMessage(const QString &text, bool error);
    /** Ask @a question (a line for the assistant), the one in flight
     *  replaced. */
    bool askNow(const QString &question, const ShowTreeCommand::Context &selection, QString *why,
                const QString &parserError);
    void busyChanged(bool busy);
    void tick();
    QString busyText() const;
    /** "Spell-check: did you mean <fix>" as a link that puts it in the line. */
    static QString fixLink(const QString &fix);

    QPointer<QLineEdit> m_line;
    QPointer<QTextBrowser> m_pane;
    QPointer<QLabel> m_message;
    QString m_placeholder;
    bool m_plainWords = false;
    /** The parser's error of a line sent to the assistant: said if the
     *  answer is a failure. */
    QString m_fallbackError;
    QPointer<AiAssist> m_ai;
    OperationRegistry *m_registry = nullptr;
    std::function<ShowTreeCommand::Context()> m_selectionProvider;
    /** The answer the proposal came with (its text heads the checklist). */
    AiAssist::Answer m_answer;
    QList<bool> m_included;
    /** The single step's text put in the line: Enter on it applies. */
    QString m_lineProposal;
    /** The spell-check's fix of the line sent: a link with the answer. */
    QString m_fallbackFix;
    /** The fix shown with the answer (and its proposal). */
    QString m_answerFix;
    std::function<bool(const QString &)> m_lineRunner;
    /** The busy indicator: the spinner's action in the line, its timer,
     *  the time since asking, the text last put in the message. */
    QAction *m_spinner = nullptr;
    QTimer *m_spin = nullptr;
    QElapsedTimer m_since;
    int m_frame = 0;
    QString m_busyShown;
    /** Asking anew: the answer "Cancelled" of the one replaced is not shown. */
    bool m_replacing = false;
};

/** @} */

#endif
