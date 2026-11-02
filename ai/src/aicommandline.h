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

#include <QObject>
#include <QPointer>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <functional>

#include "showtreecommand.h"
#include "aiassist.h"

class OperationRegistry;
class QTextBrowser;
class QLineEdit;
class QLabel;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * AI Assist in the Show Tree's command line (docs/design/AI_ASSIST.md
 * section 7): one line, no button. A line that is a command runs as
 * typed; one that isn't (and that the local spell-check, suggest(), can't
 * put right) goes to the assistant with the parser's error (fallback())
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
    /** @a line wasn't a command (@a parserError says why) and the local
     *  spell-check had nothing: ask the assistant, with the error, the
     *  syntax reference and @a selection. True: asked (answered() follows;
     *  the answer is a corrected command as a proposal, or words). False
     *  and @a note: it wasn't asked; @a note, when set, is a sentence for
     *  the line's message (AI Assist is off: where to turn it on). The
     *  view calls this in Design only. */
    bool fallback(const QString &line, const QString &parserError, const ShowTreeCommand::Context &selection,
                  QString *note = nullptr);
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

signals:
    /** A proposal was applied: where it leaves the selection. */
    void applied(const ShowTreeCommand::Result &result);

public:
    /** The line takes plain words (fallback()): its placeholder says so; else
     *  only "? asks AI Assist" (Build's command bar). */
    void setPlainWords(bool on);
    /** Redraw the line's placeholder (AI fallback on or off). */
    void update();

private:
    /** AiAssist::instance(), its signals connected once. */
    AiAssist *assist();
    void showAnswer(const AiAssist::Answer &answer);
    void showProposal();
    void proposalChanged();
    void setMessage(const QString &text, bool error);

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
};

/** @} */

#endif
