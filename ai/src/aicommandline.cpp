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
#include <QPainter>
#include <QPixmap>
#include <QAction>
#include <QTimer>
#include <QLabel>
#include <QIcon>
#include <QUrl>

#include "operationregistry.h"
#include "aicommandline.h"

namespace
{
/** The line whose question is in flight: it alone shows the busy line and
 *  the answer (Build's bar and the Show Tree's share the assistant). */
QPointer<AiCommandLine> s_asker;
/** The spinner turns this often (ms). */
const int kSpinMs = 100;
/** The seconds since asking show after this long (ms). */
const int kShowSecondsMs = 2000;
}

AiCommandLine::AiCommandLine(QLineEdit *line, QTextBrowser *pane, QLabel *message, QObject *parent)
    : QObject(parent)
    , m_line(line)
    , m_pane(pane)
    , m_message(message)
    , m_placeholder(line->placeholderText())
{
    // The busy indicator: a spinner in the line while a question is out.
    m_spinner = new QAction(this);
    m_spinner->setObjectName(QStringLiteral("aiBusySpinner"));
    m_spinner->setToolTip(tr("AI Assist is working on it: Esc (or a click here) cancels"));
    connect(m_spinner, &QAction::triggered, this, [this]() { cancel(); });
    m_spin = new QTimer(this);
    m_spin->setInterval(kSpinMs);
    connect(m_spin, &QTimer::timeout, this, &AiCommandLine::tick);
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
        connect(ai, &AiAssist::busyChanged, this, &AiCommandLine::busyChanged);
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

void AiCommandLine::setBasePlaceholder(const QString &text)
{
    m_placeholder = text;
    update();
}

void AiCommandLine::setPlainWords(bool on)
{
    m_plainWords = on;
    update();
}

bool AiCommandLine::isAsking() const
{
    return s_asker == this && !m_ai.isNull() && m_ai->isBusy();
}

bool AiCommandLine::isBusyShown() const
{
    return m_spin->isActive();
}

int AiCommandLine::elapsedSeconds() const
{
    return isAsking() && m_since.isValid() ? int(m_since.elapsed() / 1000) : -1;
}

bool AiCommandLine::askNow(const QString &question, const ShowTreeCommand::Context &selection, QString *why,
                           const QString &parserError)
{
    AiAssist *ai = assist();
    if (ai == nullptr)
    {
        *why = tr("AI Assist isn't running");
        return false;
    }
    // One in flight already: the newest line is what the operator wants.
    if (ai->isBusy() && ai->isAvailable())
    {
        m_replacing = true;
        ai->cancel();
        m_replacing = false;
    }
    const QPointer<AiCommandLine> before = s_asker;
    s_asker = this;
    if (!ai->ask(question, selection, why, parserError))
    {
        s_asker = before;
        return false;
    }
    return true;
}

QString AiCommandLine::busyText() const
{
    const qint64 ms = m_since.isValid() ? m_since.elapsed() : 0;
    if (ms < kShowSecondsMs)
        return tr("Asking AI Assist (%1)… Esc cancels").arg(AiSettings::load().model);
    return tr("Asking AI Assist (%1)… %2 s · Esc cancels").arg(AiSettings::load().model).arg(ms / 1000);
}

void AiCommandLine::busyChanged(bool busy)
{
    if (busy && s_asker == this)
    {
        m_since.start();
        m_frame = 0;
        m_busyShown.clear();
        if (!m_line.isNull() && !m_line->actions().contains(m_spinner))
            m_line->addAction(m_spinner, QLineEdit::TrailingPosition);
        tick();
        m_spin->start();
        return;
    }
    if (busy)
        return;
    m_spin->stop();
    m_since.invalidate();
    if (!m_line.isNull())
        m_line->removeAction(m_spinner);
    m_busyShown.clear();
}

void AiCommandLine::tick()
{
    if (m_line.isNull())
        return;
    // A 270° arc, a step round each tick.
    const qreal dpr = m_line->devicePixelRatioF();
    const int side = 16;
    QPixmap pm(int(side * dpr), int(side * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(m_line->palette().color(QPalette::Highlight), 2.2);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawArc(QRectF(2.5, 2.5, side - 5, side - 5), -(m_frame * 30) * 16, 270 * 16);
    }
    m_spinner->setIcon(QIcon(pm));
    ++m_frame;
    // The message, unless something else (a command typed meanwhile) has
    // said its piece there since.
    if (!m_message.isNull() && (m_busyShown.isEmpty() || m_message->text() == m_busyShown))
    {
        m_busyShown = busyText();
        setMessage(m_busyShown, false);
    }
}

bool AiCommandLine::fallback(const QString &line, const QString &parserError,
                             const ShowTreeCommand::Context &selection, QString *note, const QString &fix)
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
    if (!askNow(line.trimmed(), selection, &why, parserError))
    {
        if (note != nullptr)
            *note = why;
        return false;
    }
    m_fallbackError = parserError;
    m_fallbackFix = fix;
    if (!m_pane.isNull())
    {
        // What was sent, and the spell-check's fix to take instead.
        QString html = QStringLiteral("<p><b>? %1</b></p><p>%2</p>")
                           .arg(line.trimmed().toHtmlEscaped(),
                                tr("Not a command: asked AI Assist. Esc cancels.").toHtmlEscaped());
        m_pane->setHtml(html + fixLink(fix));
        m_pane->show();
    }
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
    if (askNow(question, selection, &why, QString()))
    {
        m_fallbackError.clear();
        m_fallbackFix.clear();
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
    if (m_lineRunner)
    {
        // Build: each step runs as a line of the bar (onto the scene).
        if (!ai->isAvailable())
        {
            setMessage(tr("Not applied: AI Assist is unavailable in Operate mode"), true);
            return false;
        }
        QStringList commands;
        for (int i = 0; i < m_included.count(); ++i)
            if (m_included.at(i) && i < ai->proposal().count())
                commands << ai->proposal().at(i).command;
        if (commands.isEmpty())
        {
            setMessage(tr("No step is included: tick one, or Esc to discard"), true);
            return false;
        }
        ai->discard();
        int done = 0;
        for (const QString &c : commands)
        {
            if (!m_lineRunner(c))
            {
                if (!m_message.isNull())
                    setMessage(tr("Stopped at “%1” (%2); %n step(s) before it applied", "", done)
                                   .arg(c, m_message->text()), true);
                return false;
            }
            ++done;
        }
        setMessage(tr("AI Assist applied %n line(s); undo takes each back", "", done), false);
        ShowTreeCommand::Result r;
        r.ok = true;
        emit applied(r);
        return true;
    }
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
                     (m_lineRunner ? tr("Enter applies the ticked steps (each its own undo step); Esc discards.")
                                   : tr("Enter applies the ticked steps as one undo step; Esc discards."))
                         .toHtmlEscaped());
    html += fixLink(m_answerFix);
    m_pane->setHtml(html);
    m_pane->show();
}

QString AiCommandLine::fixLink(const QString &fix)
{
    if (fix.isEmpty())
        return QString();
    return QStringLiteral("<p>%1 <a href=\"ex:%2\"><code>%3</code></a></p>")
        .arg(tr("Spell-check: did you mean").toHtmlEscaped(), QString::fromLatin1(QUrl::toPercentEncoding(fix)),
             fix.toHtmlEscaped());
}

void AiCommandLine::showAnswer(const AiAssist::Answer &answer)
{
    // A question replaced by a newer one: its "Cancelled" isn't news. An
    // answer to the other line's question is that line's.
    if (m_replacing)
        return;
    AiAssist *ai = assist();
    if (!s_asker.isNull() && s_asker != this && !(ai != nullptr && ai->isExternalProposal()))
        return;
    const QString fallbackError = m_fallbackError;
    m_fallbackError.clear();
    m_answerFix = m_fallbackFix;
    m_fallbackFix.clear();
    if (!answer.proposal.isEmpty())
    {
        m_answer = answer;
        m_included = QList<bool>(answer.proposal.count(), true);
        // One step: in the line too, as the operator could have typed it --
        // unless they are typing something else there meanwhile.
        m_lineProposal.clear();
        if (answer.proposal.count() == 1 && !m_line.isNull() && m_line->text().trimmed().isEmpty())
        {
            m_lineProposal = answer.proposal.first().command;
            m_line->setText(m_lineProposal);
            m_line->end(false);
        }
        showProposal();
        const bool typing = !m_line.isNull() && !m_line->text().trimmed().isEmpty()
                            && m_line->text().trimmed() != m_lineProposal;
        if (typing)
            setMessage(tr("AI Assist proposes %n change(s): Apply in the pane above, Esc discards",
                          "", answer.proposal.count()), false);
        else if (m_lineRunner)
            setMessage(tr("AI Assist proposes %n change(s): Enter applies them, Esc discards",
                          "", answer.proposal.count()), false);
        else
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
    html += fixLink(m_answerFix);
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
