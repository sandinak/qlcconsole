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

#include "showtreecommand.h"
#include "aiassist.h"

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
    /** Esc: cancel a question in flight. False when none was. */
    bool cancel();

    QToolButton *button() const { return m_button; }

    /** Redraw the AI button (shown, greyed, its tooltip). */
    void update();

private:
    /** AiAssist::instance(), its signals connected once. */
    AiAssist *assist();
    void showAnswer(const AiAssist::Answer &answer);
    void setMessage(const QString &text, bool error);

    QPointer<QLineEdit> m_line;
    QPointer<QTextBrowser> m_pane;
    QPointer<QLabel> m_message;
    QToolButton *m_button;
    QPointer<AiAssist> m_ai;
};

/** @} */

#endif
