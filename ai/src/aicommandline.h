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
class QToolButton;
class QBoxLayout;
class QLineEdit;
class QLabel;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * AI Assist in the Show Tree's command line (docs/design/AI_ASSIST.md
 * section 7): a line starting "? " is a question; the answer shows in the
 * line's help pane with the commands the assistant ran, each a link that
 * puts it in the line (the operator learns the syntax). An "AI" button
 * beside the syntax button starts a question: shown only while AI Assist
 * is on and the console is in Design, greyed with the reason while the
 * provider can't be reached. Esc cancels a question in flight.
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
    AiCommandLine(QLineEdit *line, QTextBrowser *pane, QLabel *message, QBoxLayout *row, QObject *parent);

    /** @a line is a question: ask it (or say why not) and return true.
     *  False: not a question, the command line runs it. */
    bool handle(const QString &line, const ShowTreeCommand::Context &selection);
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

    QToolButton *button() const { return m_button; }

    /** Redraw the AI button (shown, greyed, its tooltip). */
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
    QToolButton *m_button;
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
