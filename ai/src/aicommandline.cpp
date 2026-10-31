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
#include <QToolButton>
#include <QBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QUrl>

#include "aicommandline.h"

AiCommandLine::AiCommandLine(QLineEdit *line, QTextBrowser *pane, QLabel *message, QBoxLayout *row,
                             QObject *parent)
    : QObject(parent)
    , m_line(line)
    , m_pane(pane)
    , m_message(message)
{
    m_button = new QToolButton(line->parentWidget());
    m_button->setObjectName("stAiButton");
    m_button->setText(tr("AI"));
    m_button->setAutoRaise(true);
    row->addWidget(m_button);
    connect(m_button, &QToolButton::clicked, this, [this]() {
        if (m_line.isNull())
            return;
        if (!m_line->text().trimmed().startsWith(QLatin1Char('?')))
            m_line->setText(QStringLiteral("? "));
        m_line->setFocus(Qt::OtherFocusReason);
        m_line->end(false);
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
    }
    return ai;
}

void AiCommandLine::update()
{
    AiAssist *ai = assist();
    const bool shown = ai != nullptr && ai->isAvailable();
    m_button->setVisible(shown);
    if (!shown)
        return;
    m_button->setEnabled(!ai->isUnreachable());
    if (ai->isUnreachable())
        m_button->setToolTip(tr("AI Assist: %1. Settings ▸ AI Assist ▸ Test connection; or ask again "
                                "(? and your question).").arg(ai->unreachableReason()));
    else if (ai->isBusy())
        m_button->setToolTip(tr("AI Assist is answering (Esc in the line cancels)."));
    else
        m_button->setToolTip(tr("Ask AI Assist about the show: type ? and a question (\"? find address "
                                "conflicts\"). It answers from read-only questions to the console and "
                                "changes nothing."));
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
    if (!AiAssist::isAssistantLine(line))
        return false;
    AiAssist *ai = assist();
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
    if (ai == nullptr || !ai->isBusy())
        return false;
    ai->cancel();
    return true;
}

void AiCommandLine::showAnswer(const AiAssist::Answer &answer)
{
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
    setMessage(answer.ok ? tr("AI Assist answered; nothing in the show changed") : tr("AI Assist: %1").arg(answer.text.left(120)),
               !answer.ok);
    update();
}
