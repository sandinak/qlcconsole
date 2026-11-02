/*
  Q Light Controller Plus - qlcconsole
  aicommandline.cpp

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

#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <QUrl>

#include "operationregistry.h"
#include "aicommandline.h"

AiCommandLine::AiCommandLine(QLineEdit *line, QTextBrowser *pane, QLabel *message, QObject *parent)
    : QObject(parent)
    , m_line(line)
    , m_pane(pane)
    , m_message(message)
    , m_placeholder(line->placeholderText())
{
    // The proposal's links (the view's own handler only takes "ex:").
    if (!m_pane.isNull())
        connect(m_pane.data(), &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            const QString s = url.toString();
            if (s == QLatin1String("ai:apply"))
                applyProposal();
            else if (s == QLatin1String("ai:discard"))
                cancel();
            else if (s.startsWith(QLatin1String("ai:toggle:")))
                toggleStep(s.mid(10).toInt());
        });
    update();
}

AiAssist *AiCommandLine::assist()
{
    AiAssist *ai = AiAssist::instance();
    if (ai != nullptr && ai != m_ai)
    {
        m_ai = ai;
        connect(ai, &AiAssist::availabilityChanged, this, &AiCommandLine::update);
        connect(ai, &AiAssist::busyChanged, this, &AiCommandLine::update);
        connect(ai, &AiAssist::answered, this, &AiCommandLine::showAnswer);
        connect(ai, &AiAssist::proposalChanged, this, &AiCommandLine::proposalChanged);
        if (m_selectionProvider)
            ai->setSelectionProvider(this, m_selectionProvider);
    }
    return ai;
}

void AiCommandLine::setSelectionProvider(const std::function<ShowTreeCommand::Context()> &provider)
{
    m_selectionProvider = provider;
    if (AiAssist *ai = assist())
        ai->setSelectionProvider(this, provider);
}

void AiCommandLine::update()
{
    if (m_line.isNull())
        return;
    AiAssist *ai = assist();
    const bool on = ai != nullptr && ai->isAvailable();
    // The indicator: the line says it takes plain words too.
    m_line->setPlaceholderText(!on ? m_placeholder
                               : m_plainWords ? tr("%1 · or ask in plain words (AI Assist)").arg(m_placeholder)
                                              : tr("%1 · ? asks AI Assist").arg(m_placeholder));
}

void AiCommandLine::setPlainWords(bool on)
{
    m_plainWords = on;
    update();
}

bool AiCommandLine::fallback(const QString &line, const QString &parserError,
                             const ShowTreeCommand::Context &selection, QString *note)
{
    if (!AiAssist::isEnabled())
    {
        // Plain words: only worth a hint when it looks like some.
        if (note != nullptr && line.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts).count() >= 2)
            *note = tr("To type in plain words, turn on Settings ▸ AI Assist.");
        return false;
    }
    AiAssist *ai = assist();
    if (ai == nullptr || !ai->isAvailable())
        return false;
    if (ai->hasProposal())
        ai->discard();
    QString why;
    if (!ai->ask(line.trimmed(), selection, &why, parserError))
    {
        if (note != nullptr)
            *note = why;
        return false;
    }
    m_fallbackError = parserError;
    setMessage(tr("Not a command (%1). Asking %2… (Esc cancels)").arg(parserError, AiSettings::load().model),
               false);
    update();
    return true;
}

void AiCommandLine::setMessage(const QString &text, bool error)
{
    if (m_message.isNull())
        return;
    m_message->setText(text);
    QPalette pal = m_message->palette();
    pal.setColor(QPalette::WindowText, error ? QColor(Qt::red) : m_message->parentWidget()->palette().color(QPalette::WindowText));
    m_message->setPalette(pal);
}

bool AiCommandLine::handle(const QString &line, const ShowTreeCommand::Context &selection)
{
    AiAssist *ai = assist();
    if (ai != nullptr && ai->hasProposal())
    {
        // Enter on the proposal (an empty line, or the step it put there)
        // applies it; anything else typed drops it and runs as typed.
        const QString t = line.trimmed();
        if (t.isEmpty() || (!m_lineProposal.isEmpty() && t == m_lineProposal))
        {
            applyProposal();
            return true;
        }
        ai->discard();
    }
    if (!AiAssist::isAssistantLine(line))
        return false;
    const QString question = line.trimmed().mid(1).trimmed();
    QString why;
    if (ai == nullptr)
        why = tr("AI Assist isn't running");
    else if (ai->ask(question, selection, &why))
    {
        setMessage(tr("Asking %1… (Esc cancels)").arg(AiSettings::load().model), false);
        update();
        return true;
    }
    setMessage(why, true);
    return true;
}

bool AiCommandLine::cancel()
{
    AiAssist *ai = assist();
    if (ai == nullptr)
        return false;
    if (ai->isBusy())
    {
        ai->cancel();
        return true;
    }
    if (!ai->hasProposal())
        return false;
    ai->discard();
    setMessage(tr("Discarded: nothing in the show changed"), false);
    return true;
}

void AiCommandLine::toggleStep(int index)
{
    if (index < 0 || index >= m_included.count())
        return;
    m_included[index] = !m_included[index];
    showProposal();
}

bool AiCommandLine::applyProposal()
{
    AiAssist *ai = assist();
    if (ai == nullptr || !ai->hasProposal())
        return false;
    QList<int> steps;
    for (int i = 0; i < m_included.count(); ++i)
        if (m_included.at(i))
            steps << i;
    if (steps.isEmpty())
    {
        setMessage(tr("No step is included: tick one, or Esc to discard"), true);
        return false;
    }
    const int count = steps.count();
    ShowTreeCommand::Result r;
    if (!ai->apply(steps, m_registry, &r))
    {
        // Nothing changed; the proposal stays to look at (Esc drops it).
        setMessage(tr("Not applied: %1").arg(r.message), true);
        return false;
    }
    if (!m_line.isNull() && !m_lineProposal.isEmpty() && m_line->text().trimmed() == m_lineProposal)
        m_line->clear();
    m_lineProposal.clear();
    if (!m_pane.isNull())
    {
        m_pane->setHtml(QStringLiteral("<p><b>? %1</b></p><p>%2</p>")
                            .arg(m_answer.question.toHtmlEscaped(),
                                 tr("Applied %n change(s) as one undo step: undo (⌘Z, or type undo) "
                                    "takes them all back.", "", count).toHtmlEscaped()));
        m_pane->show();
    }
    setMessage(tr("AI Assist applied %n change(s) (one undo step)", "", count), false);
    emit applied(r);
    return true;
}

void AiCommandLine::proposalChanged()
{
    AiAssist *ai = assist();
    if (ai != nullptr && ai->hasProposal())
        return;                     // showAnswer() draws a new one
    // Dropped (Esc, Operate, a new question, AI Assist off): the line and
    // the checklist go with it.
    if (!m_lineProposal.isEmpty() && !m_line.isNull() && m_line->text().trimmed() == m_lineProposal)
        m_line->clear();
    const bool wasShown = !m_included.isEmpty();
    m_lineProposal.clear();
    m_included.clear();
    if (wasShown && !m_pane.isNull() && m_pane->toHtml().contains(QLatin1String("ai:apply")))
        m_pane->hide();
}

void AiCommandLine::showProposal()
{
    if (m_pane.isNull())
        return;
    const QList<AiAssist::Step> steps = m_answer.proposal;
    QString html = QStringLiteral("<p><b>? %1</b></p>").arg(m_answer.question.toHtmlEscaped());
    QString body = m_answer.text.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    html += QStringLiteral("<p>%1</p>").arg(body);
    html += QStringLiteral("<p><b>%1</b></p><table>").arg(tr("Proposed changes (nothing has changed yet):"));
    for (int i = 0; i < steps.count(); ++i)
    {
        const bool in = m_included.value(i, true);
        html += QStringLiteral("<tr><td><a href=\"ai:toggle:%1\">%2</a></td><td><code>%3</code></td><td>%4</td></tr>")
                    .arg(i)
                    .arg(in ? QStringLiteral("☑") : QStringLiteral("☐"))
                    .arg(in ? steps.at(i).command.toHtmlEscaped()
                            : QStringLiteral("<s>%1</s>").arg(steps.at(i).command.toHtmlEscaped()),
                         steps.at(i).description.toHtmlEscaped());
    }
    html += QStringLiteral("</table><p><a href=\"ai:apply\"><b>%1</b></a> · <a href=\"ai:discard\">%2</a> "
                           "&nbsp; %3</p>")
                .arg(tr("Apply"), tr("Discard"),
                     tr("Enter applies the ticked steps as one undo step; Esc discards.").toHtmlEscaped());
    m_pane->setHtml(html);
    m_pane->show();
}

void AiCommandLine::showAnswer(const AiAssist::Answer &answer)
{
    const QString fallbackError = m_fallbackError;
    m_fallbackError.clear();
    if (!answer.proposal.isEmpty())
    {
        m_answer = answer;
        m_included = QList<bool>(answer.proposal.count(), true);
        // One step: in the line too, as the operator could have typed it.
        m_lineProposal.clear();
        if (answer.proposal.count() == 1 && !m_line.isNull())
        {
            m_lineProposal = answer.proposal.first().command;
            m_line->setText(m_lineProposal);
            m_line->end(false);
        }
        showProposal();
        setMessage(tr("AI Assist proposes %n change(s): Enter applies them (one undo step), Esc discards",
                      "", answer.proposal.count()), false);
        update();
        return;
    }
    QString html = QStringLiteral("<p><b>? %1</b></p>").arg(answer.question.toHtmlEscaped());
    QString body = answer.text.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    html += QStringLiteral("<p>%1</p>").arg(body);
    if (!answer.commands.isEmpty())
    {
        QStringList links;
        for (const QString &c : answer.commands)
            links << QStringLiteral("<a href=\"ex:%1\"><code>%2</code></a>")
                         .arg(QString::fromLatin1(QUrl::toPercentEncoding(c)), c.toHtmlEscaped());
        html += QStringLiteral("<p>%1 %2</p>").arg(tr("Asked the console (type these yourself next time):"),
                                                   links.join(QStringLiteral(" · ")));
    }
    if (!answer.rejected.isEmpty())
        html += QStringLiteral("<p>%1 %2</p>").arg(tr("Refused (not a read-only question of the console):"),
                                                   answer.rejected.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!m_pane.isNull())
    {
        m_pane->setHtml(html);
        m_pane->show();
    }
    QString failure = tr("AI Assist: %1").arg(answer.text.left(120));
    if (!answer.ok && !fallbackError.isEmpty())
        failure = tr("%1 (AI Assist could not help: %2)").arg(fallbackError, answer.text.left(120));
    setMessage(answer.ok ? tr("AI Assist answered; nothing in the show changed") : failure, !answer.ok);
    update();
}
