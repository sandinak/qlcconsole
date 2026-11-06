/*
  Q Light Controller Plus
  workbenchcommand.cpp

  Copyright (C) Branson Matheson

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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QDataStream>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QSettings>
#include <QPalette>
#include <QLabel>
#include <QUrl>

#include "operationregistry.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "showtreecontroller.h"
#include "showtreeview.h"
#include "workbenchcommand.h"
#include "qlcpalette.h"
#include "function.h"
#include "doc.h"
#ifdef QLC_AI_ASSIST
#include "aicommandline.h"
#endif

BuildCommandBar::BuildCommandBar(Doc *doc, const Host &host, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_host(host)
{
    setObjectName(QStringLiteral("buildCommandBar"));
    m_commander = new ShowTreeCommand(m_doc, m_doc->showTree());
    // Every line goes through the Operation Registry, as the Show Tree's do.
    m_operations = new OperationRegistry(m_doc, m_commander);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    m_help = new QTextBrowser(this);
    m_help->setObjectName(QStringLiteral("buildCommandHelp"));
    m_help->setOpenLinks(false);
    m_help->setMaximumHeight(170);
    m_help->hide();
    connect(m_help, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const QString s = url.toString();
        if (!s.startsWith(QLatin1String("ex:")))
            return;
        const QString cmd = QUrl::fromPercentEncoding(s.mid(3).toUtf8());
        if (cmd.startsWith(QLatin1String("help")))
        {
            runLine(cmd);
            return;
        }
        m_line->setText(cmd);
        m_line->setFocus(Qt::OtherFocusReason);
    });
    v->addWidget(m_help);

    QHBoxLayout *h = new QHBoxLayout();
    h->addWidget(new QLabel(tr("Command"), this));
    m_line = new QLineEdit(this);
    m_line->setObjectName(QStringLiteral("buildCommand"));
    m_line->setPlaceholderText(tr("Type a command (: from the scenes list) — e.g. Towers colour Blue dimmer 45"));
    m_line->setToolTip(tr("Type what you'd say, for the scene loaded in the Workbench. Tab completes every "
                          "word, Enter runs, ↑ recalls, Esc closes the help. Every line is one undo step. "
                          "The Show Tree's cue words (cue, list, record…) answer there only."));
    m_line->setFocusPolicy(Qt::ClickFocus);
    QFont mono(QStringLiteral("Menlo"));
    mono.setStyleHint(QFont::Monospace);
    m_line->setFont(mono);
    m_line->installEventFilter(this);
    h->addWidget(m_line, 1);
    m_syntaxButton = new QToolButton(this);
    m_syntaxButton->setObjectName(QStringLiteral("buildCommandSyntaxButton"));
    m_syntaxButton->setText(QStringLiteral("?"));
    m_syntaxButton->setCheckable(true);
    m_syntaxButton->setAutoRaise(true);
    m_syntaxButton->setToolTip(tr("Show or hide the command line's syntax (also ? typed first, or F1 in "
                                  "the line). Type help for every word, with examples."));
    h->addWidget(m_syntaxButton);
    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("buildCommandMessage"));
    h->addWidget(m_message);
#ifdef QLC_AI_ASSIST
    // "? question", and a line in plain words: AI Assist's, as in the
    // tree. What it proposes runs as lines of this bar, onto the scene.
    m_ai = new AiCommandLine(m_line, m_help, m_message, this);
    m_ai->setOperations(m_operations);
    m_ai->setPlainWords(true);
    m_ai->setSelectionProvider([]() { return ShowTreeCommand::Context(); });
    m_ai->setLineRunner([this](const QString &command) { return runLine(command); });
#endif
    v->addLayout(h);
    m_syntax = new QLabel(grammar(), this);
    m_syntax->setObjectName(QStringLiteral("buildCommandHint"));
    m_syntax->setWordWrap(true);
    m_syntax->setEnabled(false);
    v->addWidget(m_syntax);
    const bool syntax = QSettings().value(QStringLiteral("build/commandSyntax"), false).toBool();
    m_syntax->setVisible(syntax);
    m_syntaxButton->setChecked(syntax);
    connect(m_syntaxButton, &QToolButton::toggled, this, &BuildCommandBar::setSyntaxShown);
    connect(m_line, &QLineEdit::returnPressed, this, [this]() {
        if (runLine(m_line->text()))
            m_line->clear();
    });
}

BuildCommandBar::~BuildCommandBar()
{
    delete m_operations;
    delete m_commander;
}

QString BuildCommandBar::grammar()
{
    return tr("Towers colour Blue dimmer 45 · Towers, Steps colour Red · dimmer 50 (the scene) · "
              "Steps effect Sinelon · Towers formation \"Pairs\" · palette colour \"Amber\" #ffb000 · "
              "undo · redo · help — Tab completes every word, ↑ recalls. cue, list, record, GO… are the Show "
              "Tree's.");
}

QString BuildCommandBar::message() const
{
    return m_message->text();
}

QString BuildCommandBar::helpHtml() const
{
    return m_help->isVisible() ? m_help->toHtml() : QString();
}

bool BuildCommandBar::isHelpShown() const
{
    return m_help->isVisible();
}

bool BuildCommandBar::isSyntaxShown() const
{
    return !m_syntax->isHidden();
}

void BuildCommandBar::setSyntaxShown(bool shown)
{
    m_syntax->setVisible(shown);
    {
        QSignalBlocker block(m_syntaxButton);
        m_syntaxButton->setChecked(shown);
    }
    QSettings().setValue(QStringLiteral("build/commandSyntax"), shown);
}

void BuildCommandBar::focusLine()
{
    m_line->setFocus(Qt::ShortcutFocusReason);
    m_line->selectAll();
}

void BuildCommandBar::setMessage(const QString &text, bool ok)
{
    m_message->setText(text);
    QPalette pal = m_message->palette();
    pal.setColor(QPalette::WindowText, ok ? palette().color(QPalette::WindowText) : QColor(Qt::red));
    m_message->setPalette(pal);
}

QMimeData *BuildCommandBar::mimeFor(const ShowTreeCommand::BuildEdit &edit)
{
    QByteArray groups, fixtures, sets, palettes;
    QDataStream gs(&groups, QIODevice::WriteOnly), fs(&fixtures, QIODevice::WriteOnly),
                ss(&sets, QIODevice::WriteOnly), ps(&palettes, QIODevice::WriteOnly);
    for (const ShowTree::Target &t : edit.targets)
    {
        for (quint32 id : t.groups)
            gs << id;
        for (quint32 id : t.fixtures)
            fs << id;
        for (quint32 id : t.sets)
            ss << id;
    }
    for (quint32 id : edit.palettes)
        ps << id;
    if (groups.isEmpty() && fixtures.isEmpty() && sets.isEmpty() && palettes.isEmpty())
        return nullptr;
    QMimeData *mime = new QMimeData;
    if (!groups.isEmpty())
        mime->setData(QLatin1String(FixtureGroupSource::fixtureGroupMimeType()), groups);
    if (!fixtures.isEmpty())
        mime->setData(QLatin1String(FixtureGroupSource::fixtureMimeType()), fixtures);
    if (!sets.isEmpty())
        mime->setData(QLatin1String(ShowTreeView::setMimeType()), sets);
    if (!palettes.isEmpty())
        mime->setData(QLatin1String(FunctionsTreeWidget::paletteDragMimeType()), palettes);
    return mime;
}

bool BuildCommandBar::runLine(const QString &line)
{
    if (m_host.workbenchLent && m_host.workbenchLent())
    {
        setMessage(tr("The Workbench is in the Show Tree: ↩ there brings it back."), false);
        return false;
    }
    ShowTreeCommand::BuildEdit edit;
    ShowTreeCommand::Context ctx;
    ctx.build = &edit;
    if (m_host.recentPalettes)
        ctx.recentPalettes = m_host.recentPalettes();

#ifdef QLC_AI_ASSIST
    // What AI Assist is asked with: no cue, and not this line's edit (it
    // outlives the line; a proposal runs as lines of the bar).
    ShowTreeCommand::Context aiCtx = ctx;
    aiCtx.build = nullptr;
    // "? question": AI Assist's (it answers in the help pane, later).
    if (m_ai != nullptr && m_ai->handle(line, aiCtx))
    {
        if (m_history.isEmpty() || m_history.last() != line)
            m_history << line;
        m_historyAt = -1;
        return true;
    }
#endif
    ShowTreeCommand::Result r = m_operations->runLine(line, ctx);
    m_suggestedFrom.clear();
    // One line, as in the tree: a line that isn't a command is a typo (the
    // fix in the line) or plain words (AI Assist, in Design).
    if (!r.ok && r.help.isEmpty())
    {
        const ShowTreeCommand::Failure f = m_commander->classifyFailure(line, ctx, r.unparsed);
        auto offerFix = [this, &line, &f]() {
            m_suggestedFrom = line;
            m_suggestedLine = f.fix;
            m_line->setText(f.fix);
            m_line->end(false);
            setMessage(tr("Did you mean “%1”? Enter runs it, Esc puts back what you typed.").arg(f.fix), true);
            return false;                       // the fix stays in the line
        };
        if (f.kind == ShowTreeCommand::Failure::Typo)
        {
            if (!f.fix.isEmpty())
                return offerFix();
        }
#ifdef QLC_AI_ASSIST
        else if (m_ai != nullptr)
        {
            if (m_doc->mode() == Doc::Design)
            {
                QString note;
                if (m_ai->fallback(line, r.message, aiCtx, &note,
                                  f.kind == ShowTreeCommand::Failure::Unsure ? f.fix : QString()))
                {
                    if (m_history.isEmpty() || m_history.last() != line)
                        m_history << line;
                    m_historyAt = -1;
                    return true;
                }
                if (f.kind == ShowTreeCommand::Failure::Unsure && !f.fix.isEmpty())
                    return offerFix();
                if (!note.isEmpty())
                    r.message += QLatin1Char(' ') + note;
            }
            else if (f.kind == ShowTreeCommand::Failure::Unsure && !f.fix.isEmpty())
                return offerFix();
            else if (f.kind == ShowTreeCommand::Failure::Language)
                r.message += QLatin1Char(' ') + (AiAssist::isEnabled()
                                                     ? tr("AI Assist takes plain words in Design mode only.")
                                                     : tr("AI Assist takes plain words in Design mode; turn it on in Settings ▸ AI Assist."));
        }
#endif
        else if (f.kind == ShowTreeCommand::Failure::Unsure && !f.fix.isEmpty())
            return offerFix();
    }
    QString message = r.message;
    bool ok = r.ok;
    if (!r.help.isEmpty())
    {
        m_help->setHtml(r.help);
        m_help->show();
    }
    // A looks line: what it meant goes onto the scene, as a drop would.
    if (ok && (!edit.targets.isEmpty() || !edit.palettes.isEmpty()))
    {
        const quint32 scene = m_host.sceneId ? m_host.sceneId() : Function::invalidId();
        QScopedPointer<QMimeData> mime(mimeFor(edit));
        if (scene == Function::invalidId() || mime.isNull() || !m_host.addToScene)
        {
            message = tr("Load a scene first (click one in the Scenes list): there is nothing here to put it on.");
            ok = false;
        }
        else if (!m_host.addToScene(mime.data(), scene))
        {
            message = tr("Nothing to change: the scene has those already.");
            ok = false;
        }
        else if (m_host.noteRecent)
            m_host.noteRecent(mime.data());
    }
    setMessage(message, ok);
    if (!ok)
        return false;
    if (r.editPalette != QLCPalette::invalidId() && m_host.showPalette && m_doc->palette(r.editPalette) != nullptr)
        m_host.showPalette(r.editPalette);
    if (m_history.isEmpty() || m_history.last() != line)
        m_history << line;
    m_historyAt = -1;
    return true;
}

bool BuildCommandBar::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_line && ev->type() == QEvent::ShortcutOverride
        && static_cast<QKeyEvent *>(ev)->key() == Qt::Key_F1)
    {
        ev->accept();
        return true;
    }
    if (obj == m_line && ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_F1 && ke->modifiers() == Qt::NoModifier)
        {
            setSyntaxShown(!isSyntaxShown());
            return true;
        }
        if (ke->text() == QLatin1String("?") && m_line->text().trimmed().isEmpty())
            setSyntaxShown(true);
        if (ke->key() == Qt::Key_Tab)
        {
            ShowTreeCommand::Context ctx;
            QStringList options;
            int current = -1;
            m_line->setText(m_commander->tab(m_line->text(), ctx, &options, &current));
            QStringList shown;
            const int from = current < 10 ? 0 : current - 9;
            for (int i = from; i < options.count() && shown.count() < 10; ++i)
                shown << (i == current ? QStringLiteral("[%1]").arg(options.at(i)) : options.at(i));
            if (options.count() > from + shown.count())
                shown << tr("+%1 more").arg(options.count() - from - shown.count());
            setMessage(options.count() > 1 ? shown.join(QStringLiteral(" · ")) : QString(), true);
            return true;
        }
        if (ke->key() == Qt::Key_Escape)
        {
            // A suggestion in the line: what was typed comes back.
            if (!m_suggestedFrom.isNull() && m_line->text() == m_suggestedLine)
            {
                m_line->setText(m_suggestedFrom);
                m_suggestedFrom.clear();
                setMessage(QString(), true);
                return true;
            }
#ifdef QLC_AI_ASSIST
            if (m_ai != nullptr && m_ai->cancel())
                return true;
#endif
            if (m_help->isVisible())
                m_help->hide();
            else if (parentWidget() != nullptr)
                parentWidget()->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        if ((ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) && !m_history.isEmpty())
        {
            if (ke->key() == Qt::Key_Up)
                m_historyAt = m_historyAt < 0 ? int(m_history.count()) - 1 : qMax(0, m_historyAt - 1);
            else if (m_historyAt >= 0)
                m_historyAt = m_historyAt + 1 < m_history.count() ? m_historyAt + 1 : -1;
            m_line->setText(m_historyAt < 0 ? QString() : m_history.at(m_historyAt));
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}
