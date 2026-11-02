/*
  Q Light Controller Plus
  whyonpanel.cpp

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

#include <QGuiApplication>
#include <QSettings>
#include <QPointer>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>
#include <QScreen>
#include <QCursor>
#include <QTimer>
#include <QLabel>

#include "whyonpanel.h"
#include "virtualconsole.h"
#include "showtreecontroller.h"
#include "teachingsettings.h"
#include "showtreeview.h"
#include "simpledesk.h"
#include "fixturemanager.h"
#include "programmercontroller.h"
#include "qlcchannel.h"
#include "vcwidget.h"
#include "monitor.h"
#include "functionparent.h"
#include "function.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QPointer<WhyOnPanel> s_panel;
WhyOnPanel::Jump s_jumpOverride;
QList<quint32> s_selection;
const int kRowRole = Qt::UserRole + 1;

/** Its tab forward, and its window */
void bringForward(QWidget *w)
{
    if (w == nullptr)
        return;
    for (QWidget *p = w->parentWidget(); p != nullptr; p = p->parentWidget())
        if (QTabWidget *tabs = qobject_cast<QTabWidget *>(p))
        {
            for (int i = 0; i < tabs->count(); ++i)
                if (tabs->widget(i) == w || tabs->widget(i)->isAncestorOf(w))
                    tabs->setCurrentIndex(i);
            break;
        }
    if (w->window() != nullptr && w->window()->isVisible())
    {
        w->window()->raise();
        w->window()->activateWindow();
    }
}

/** "VC slider “Wash”": the engine knows only the widget id */
QString widgetName(quint32 id)
{
    VirtualConsole *vc = VirtualConsole::instance();
    VCWidget *w = vc != nullptr ? vc->widget(id) : nullptr;
    if (w == nullptr)
        return QString();
    return WhyOnPanel::tr("VC %1 “%2”").arg(VCWidget::typeToString(w->type()).toLower(), w->caption());
}
}

QString WhyOnPanel::actionText()
{
    return tr("Why is this on?");
}

WhyOnPanel *WhyOnPanel::current()
{
    return s_panel.data();
}

void WhyOnPanel::setJumpOverride(const Jump &jump)
{
    s_jumpOverride = jump;
}

void WhyOnPanel::noteSelection(const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return;
    s_selection = fixtureIds;
    // Follow selection: a window that is open goes to the new light.
    WhyOnPanel *p = s_panel.data();
    if (p == nullptr || !p->isVisible() || !p->m_follow->isChecked())
        return;
    for (quint32 fid : fixtureIds)
        if (p->m_doc->fixture(fid) != nullptr)
        {
            if (fid != p->m_fixtureId)
                p->setTarget(fid, WhyOn::defaultChannel(p->m_doc, fid), Universe::invalid(), -1);
            return;
        }
}

bool WhyOnPanel::followSelection()
{
    return QSettings().value(QStringLiteral("whyon/followSelection"), false).toBool();
}

void WhyOnPanel::setFollowSelection(bool follow)
{
    QSettings().setValue(QStringLiteral("whyon/followSelection"), follow);
}

QKeySequence WhyOnPanel::shortcut()
{
    return QKeySequence(Qt::CTRL | Qt::Key_I);
}

WhyOnPanel *WhyOnPanel::showForSelection(Doc *doc, QWidget *parent)
{
    if (doc == nullptr)
        return nullptr;
    for (quint32 fid : std::as_const(s_selection))
        if (doc->fixture(fid) != nullptr)
            return showFor(doc, fid, 0xFFFFFFFF, parent, QCursor::pos());
    // Nothing selected: the window says how.
    if (s_panel.isNull() || s_panel->m_doc != doc)
    {
        delete s_panel.data();
        s_panel = new WhyOnPanel(doc, parent);
    }
    s_panel->setTarget(Fixture::invalidId(), QLCChannel::invalid(), Universe::invalid(), -1);
    s_panel->show();
    s_panel->raise();
    return s_panel.data();
}

WhyOnPanel::Action WhyOnPanel::actionFor(const WhyOn::Line &line)
{
    switch (line.kind)
    {
        case WhyOn::Kind::Park:
            return Action::Unpark;
        case WhyOn::Kind::Mark:
            return Action::LetGoMark;
        case WhyOn::Kind::SimpleDesk:
            return line.absAddress != 0xFFFFFFFF ? Action::Release : Action::NoAction;
        case WhyOn::Kind::Function:
            return line.functionId != 0xFFFFFFFF ? Action::StopFunction : Action::NoAction;
        default:
            return Action::NoAction;
    }
}

QString WhyOnPanel::actionName(Action action)
{
    switch (action)
    {
        case Action::Unpark: return tr("Unpark");
        case Action::Release: return tr("Release the desk channel");
        case Action::LetGoMark: return tr("Let go of the move in black");
        case Action::StopFunction: return tr("Stop the function");
        case Action::NoAction:
        default: return QString();
    }
}

const WhyOn::Line *WhyOnPanel::lineAt(int row) const
{
    QListWidgetItem *item = m_list->item(row);
    if (item == nullptr)
        return nullptr;
    const QVariant v = item->data(kRowRole);
    const int i = v.isValid() ? v.toInt() : -1;
    return i >= 0 && i < m_rows.count() ? &m_rows.at(i) : nullptr;
}

bool WhyOnPanel::runAction(int row)
{
    const WhyOn::Line *line = lineAt(row);
    if (line == nullptr || m_doc->mode() == Doc::Operate)
        return false;
    const Action action = actionFor(*line);
    ProgrammerController *pc = m_doc->programmer();
    switch (action)
    {
        case Action::Unpark:
            if (pc == nullptr || m_fixtureId == Fixture::invalidId())
                return false;
            pc->unparkFixtures({ m_fixtureId });
            break;
        case Action::LetGoMark:
            if (pc == nullptr || m_fixtureId == Fixture::invalidId())
                return false;
            pc->unmarkFixtures({ m_fixtureId });
            break;
        case Action::Release:
            if (SimpleDesk::instance() == nullptr)
                return false;
            SimpleDesk::instance()->resetChannel(line->absAddress);
            break;
        case Action::StopFunction:
        {
            Function *f = m_doc->function(line->functionId);
            if (f == nullptr || !f->isRunning())
                return false;
            f->stop(FunctionParent::master());
            break;
        }
        case Action::NoAction:
        default:
            return false;
    }
    refresh();
    return true;
}

void WhyOnPanel::updateAction()
{
    const WhyOn::Line *line = lineAt(m_list->currentRow());
    const Action action = line != nullptr ? actionFor(*line) : Action::NoAction;
    m_action->setVisible(action != Action::NoAction);
    m_action->setText(actionName(action));
    const bool operate = m_doc->mode() == Doc::Operate;
    m_action->setEnabled(action != Action::NoAction && !operate);
    m_action->setToolTip(operate ? tr("Operate mode: the window only explains (switch to Design to act)")
                                 : tr("Do it now, from here"));
}

WhyOnPanel *WhyOnPanel::showFor(Doc *doc, quint32 fixtureId, quint32 channel, QWidget *parent,
                                const QPoint &globalPos)
{
    if (doc == nullptr || doc->fixture(fixtureId) == nullptr)
        return nullptr;
    if (s_panel.isNull() || s_panel->m_doc != doc)
    {
        delete s_panel.data();
        s_panel = new WhyOnPanel(doc, parent);
    }
    s_panel->setTarget(fixtureId, channel == QLCChannel::invalid() ? WhyOn::defaultChannel(doc, fixtureId) : channel,
                       Universe::invalid(), -1);
    s_panel->place(globalPos);
    s_panel->show();
    s_panel->raise();
    return s_panel.data();
}

WhyOnPanel *WhyOnPanel::showForAddress(Doc *doc, quint32 universe, int address, QWidget *parent,
                                       const QPoint &globalPos)
{
    if (doc == nullptr || address < 0)
        return nullptr;
    // A fixture there: ask about it, so its channels can be picked.
    const quint32 fid = doc->fixtureForAddress((universe << 9) | quint32(address));
    if (Fixture *fxi = doc->fixture(fid))
        return showFor(doc, fid, quint32(address) - fxi->address(), parent, globalPos);
    if (s_panel.isNull() || s_panel->m_doc != doc)
    {
        delete s_panel.data();
        s_panel = new WhyOnPanel(doc, parent);
    }
    s_panel->setTarget(Fixture::invalidId(), QLCChannel::invalid(), universe, address);
    s_panel->place(globalPos);
    s_panel->show();
    s_panel->raise();
    return s_panel.data();
}

WhyOnPanel::WhyOnPanel(Doc *doc, QWidget *parent)
    : QFrame(parent, Qt::Tool)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("whyOnPanel"));
    setWindowTitle(actionText());
    setAttribute(Qt::WA_DeleteOnClose);
    setProperty("helpTopic", QStringLiteral("troubleshooting.md#why-is-this-light-on"));
    setFrameShape(QFrame::StyledPanel);
    resize(460, 280);

    WhyOn::setWidgetNamer(widgetName);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    QHBoxLayout *top = new QHBoxLayout;
    m_subject = new QLabel(this);
    m_subject->setObjectName(QStringLiteral("whyOnSubject"));
    QFont bold = m_subject->font();
    bold.setBold(true);
    m_subject->setFont(bold);
    top->addWidget(m_subject, 1);
    m_channels = new QComboBox(this);
    m_channels->setObjectName(QStringLiteral("whyOnChannel"));
    m_channels->setToolTip(tr("Which of the fixture's channels to explain"));
    top->addWidget(m_channels);
    root->addLayout(top);

    m_headline = new QLabel(this);
    m_headline->setObjectName(QStringLiteral("whyOnHeadline"));
    m_headline->setWordWrap(true);
    root->addWidget(m_headline);

    m_story = new QLabel(this);
    m_story->setObjectName(QStringLiteral("whyOnStory"));
    m_story->setWordWrap(true);
    m_story->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_story->setToolTip(tr("The same in plain sentences, per group. The lines below are the detail."));
    m_story->hide();
    root->addWidget(m_story);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("whyOnLines"));
    m_list->setWordWrap(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setToolTip(tr("What sets this light now, top-down: the highest layer first. Click a line to go there."));
    root->addWidget(m_list, 1);

    QHBoxLayout *bar = new QHBoxLayout;
    m_plain = new QCheckBox(tr("Plain words"), this);
    m_plain->setObjectName(QStringLiteral("whyOnPlain"));
    m_plain->setToolTip(tr("Leave out the merge words (HTP, LTP): on by default in a Teaching show"));
    const TeachingSettings *teach = doc->teachingSettings();
    m_plain->setChecked(teach != nullptr && teach->enabled());
    bar->addWidget(m_plain);
    m_follow = new QCheckBox(tr("Follow selection"), this);
    m_follow->setObjectName(QStringLiteral("whyOnFollow"));
    m_follow->setToolTip(tr("Move to the light selected in the plot, the Rig Overview, the Fixture "
                            "Manager or the Workbench as you select it"));
    m_follow->setChecked(followSelection());
    bar->addWidget(m_follow);
    bar->addStretch(1);
    m_action = new QPushButton(this);
    m_action->setObjectName(QStringLiteral("whyOnAction"));
    m_action->setVisible(false);
    // Its room is kept: selecting a line must not move the list under the mouse.
    QSizePolicy keep = m_action->sizePolicy();
    keep.setRetainSizeWhenHidden(true);
    m_action->setSizePolicy(keep);
    m_action->setText(actionName(Action::LetGoMark));      // the widest label, for its room
    bar->addWidget(m_action);
    m_copy = new QPushButton(tr("Copy"), this);
    m_copy->setObjectName(QStringLiteral("whyOnCopy"));
    m_copy->setToolTip(tr("Copy the answer as text (for a note or a support thread)"));
    bar->addWidget(m_copy);
    QPushButton *close = new QPushButton(tr("Close"), this);
    bar->addWidget(close);
    root->addLayout(bar);

    // Asked again while shown, on the UI thread; stopped when hidden.
    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &WhyOnPanel::refresh);

    connect(m_channels, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        const QVariant v = m_channels->itemData(idx);
        if (v.isValid())
        {
            m_channel = v.toUInt();
            refresh();
        }
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        activate(m_list->row(item));
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &WhyOnPanel::updateAction);
    connect(m_action, &QPushButton::clicked, this, [this]() { runAction(m_list->currentRow()); });
    connect(m_plain, &QCheckBox::toggled, this, [this]() {
        m_storyAge = 0;
        refresh();
    });
    connect(m_follow, &QCheckBox::toggled, this, [](bool on) { setFollowSelection(on); });
    connect(m_copy, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_story->isVisibleTo(this) && !m_storyText.isEmpty()
                                              ? m_storyText + QStringLiteral("\n\n") + m_answer.toText()
                                              : m_answer.toText());
    });
    connect(close, &QPushButton::clicked, this, &QWidget::close);
}

WhyOnPanel::~WhyOnPanel()
{
    WhyOn::setWidgetNamer(nullptr);
}

WhyOn::Wording WhyOnPanel::wording() const
{
    return m_plain->isChecked() ? WhyOn::Wording::Simple : WhyOn::Wording::Crew;
}

void WhyOnPanel::setWording(WhyOn::Wording wording)
{
    m_plain->setChecked(wording == WhyOn::Wording::Simple);
}

void WhyOnPanel::setTarget(quint32 fixtureId, quint32 channel, quint32 universe, int address)
{
    m_fixtureId = fixtureId;
    m_channel = channel;
    m_universe = universe;
    m_address = address;
    m_storyAge = 0;
    fillChannels();
    refresh();
}

void WhyOnPanel::fillChannels()
{
    m_channels->clear();
    Fixture *fxi = m_doc->fixture(m_fixtureId);
    m_channels->setVisible(fxi != nullptr && fxi->channels() > 1);
    if (fxi == nullptr)
        return;
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        const QLCChannel *ch = fxi->channel(c);
        m_channels->addItem(ch != nullptr ? ch->name() : tr("Channel %1").arg(c + 1), c);
    }
    m_channels->setCurrentIndex(m_channels->findData(m_channel));
}

void WhyOnPanel::refresh()
{
    m_answer = m_fixtureId != Fixture::invalidId()
               ? WhyOn::explain(m_doc, m_fixtureId, m_channel, wording())
               : WhyOn::explainAddress(m_doc, m_universe, m_address, wording());
    m_subject->setText(m_answer.subject);
    m_headline->setText(m_answer.headline);
    if (m_fixtureId == Fixture::invalidId() && m_address < 0)
    {
        m_subject->setText(actionText());
        m_headline->setText(tr("Select a light (in the plot, the Rig Overview, the Fixture Manager or "
                               "the Workbench), then press %1.")
                            .arg(shortcut().toString(QKeySequence::NativeText)));
    }

    updateStory();

    // Four times a second: leave the list alone when nothing in it changed,
    // so a click (press and release) is never lost to a rebuild.
    QStringList signature;
    auto note = [&signature](const WhyOn::Line &l) {
        signature << QStringLiteral("%1/%2/%3/%4/%5/%6/%7").arg(int(l.kind)).arg(int(l.verdict))
                     .arg(l.cueId).arg(l.layerId).arg(l.functionId).arg(l.absAddress).arg(l.text);
    };
    for (const WhyOn::Line &l : std::as_const(m_answer.whyNot))
        note(l);
    signature << QString();
    for (const WhyOn::Line &l : std::as_const(m_answer.lines))
        note(l);
    signature << (m_answer.approximate ? QStringLiteral("~") : QString());
    if (signature == m_signature && m_list->count() > 0)
    {
        updateAction();
        return;
    }
    m_signature = signature;

    const int keep = m_list->currentRow();
    const QSignalBlocker block(m_list);
    m_list->clear();
    m_rows.clear();
    const QColor dim = palette().color(QPalette::Disabled, QPalette::Text);
    auto header = [this](const QString &text) {
        QListWidgetItem *item = new QListWidgetItem(text, m_list);
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        item->setFlags(Qt::ItemIsEnabled);
    };
    QList<WhyOn::Line> all;
    if (!m_answer.whyNot.isEmpty())
        header(tr("Why it isn't on:"));
    for (const WhyOn::Line &l : m_answer.whyNot)
        all << l;
    const int firstLine = all.count();
    all << m_answer.lines;
    for (int i = 0; i < all.count(); ++i)
    {
        if (i == firstLine && !m_answer.whyNot.isEmpty())
            header(tr("What sets it:"));
        const WhyOn::Line &l = all.at(i);
        m_rows << l;
        QListWidgetItem *item = new QListWidgetItem(i < firstLine ? QStringLiteral("✖ ") + l.text : l.text, m_list);
        item->setData(kRowRole, m_rows.count() - 1);
        QFont f = item->font();
        switch (l.verdict)
        {
            case WhyOn::Verdict::Wins:
                f.setBold(true);
                break;
            case WhyOn::Verdict::Lower:
            case WhyOn::Verdict::Under:
            case WhyOn::Verdict::Replaced:
                item->setForeground(dim);
                break;
            default:
                break;
        }
        item->setFont(f);
        const bool goes = l.cueId != 0xFFFFFFFF || l.functionId != 0xFFFFFFFF || l.widgetId != 0xFFFFFFFF
                          || l.kind == WhyOn::Kind::SimpleDesk || l.kind == WhyOn::Kind::Park
                          || l.kind == WhyOn::Kind::GroupMaster
                          || l.kind == WhyOn::Kind::Highlight || l.kind == WhyOn::Kind::Focus;
        const Action act = actionFor(l);
        QString tip = goes ? tr("Click to go there") : QString();
        if (act != Action::NoAction)
            tip += (tip.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("Select it: %1").arg(actionName(act));
        item->setToolTip(tip);
    }
    if (m_answer.approximate)
    {
        QListWidgetItem *item = new QListWidgetItem(
            tr("≈ read between frames, or a blend that can only be estimated"), m_list);
        item->setForeground(dim);
        item->setFlags(Qt::ItemIsEnabled);
    }
    if (keep >= 0 && keep < m_list->count())
        m_list->setCurrentRow(keep);
    updateAction();
}

/* Student mode: plain sentences per group the light is in (a light in no
   group is told on its own), beside the chain. Once a second at most: a
   big group asks each of its fixtures. */
void WhyOnPanel::updateStory()
{
    const bool on = wording() == WhyOn::Wording::Simple && m_fixtureId != Fixture::invalidId();
    if (!on)
    {
        m_storyText.clear();
        m_storyAge = 0;
        m_story->hide();
        return;
    }
    if (m_storyAge > 0 && m_storyAge < 4 && m_story->isVisibleTo(this))
    {
        ++m_storyAge;
        return;
    }
    m_storyAge = 1;
    QStringList paragraphs;
    int told = 0;
    for (FixtureGroup *grp : m_doc->fixtureGroups())
    {
        if (!grp->fixtureList().contains(m_fixtureId))
            continue;
        const WhyOn::GroupAnswer g = WhyOn::explainGroup(m_doc, grp->id());
        if (g.isValid())
            paragraphs << g.toText();
        if (++told >= 2)
            break;
    }
    if (told == 0)
        if (Fixture *fxi = m_doc->fixture(m_fixtureId))
        {
            const WhyOn::GroupAnswer g = WhyOn::explainFixtures(m_doc, fxi->name(), QList<quint32>() << m_fixtureId, false);
            if (g.isValid())
                paragraphs << g.toText();
        }
    m_storyText = paragraphs.join(QStringLiteral("\n"));
    m_story->setText(m_storyText);
    m_story->setVisible(!m_storyText.isEmpty());
}

void WhyOnPanel::activate(int row)
{
    if (const WhyOn::Line *line = lineAt(row))
        jump(*line);
}

void WhyOnPanel::jump(const WhyOn::Line &line)
{
    if (s_jumpOverride)
    {
        s_jumpOverride(line);
        return;
    }
    ShowTreeView *tree = ShowTreeView::instance();
    switch (line.kind)
    {
        case WhyOn::Kind::LeftBehind:
            // The writer that left it: where it is, as its own kind would go.
            if (line.cueId == 0xFFFFFFFF && line.functionId == 0xFFFFFFFF)
            {
                if (line.widgetId != 0xFFFFFFFF)
                    bringForward(VirtualConsole::instance());
                else if (line.absAddress != 0xFFFFFFFF)
                    bringForward(SimpleDesk::instance());
                break;
            }
            if (line.cueId == 0xFFFFFFFF)
            {
                if (tree != nullptr)
                    tree->openFunction(line.functionId);
                break;
            }
            Q_FALLTHROUGH();
        case WhyOn::Kind::Cue:
        case WhyOn::Kind::Hit:
        case WhyOn::Kind::Preview:
        case WhyOn::Kind::Released:
        case WhyOn::Kind::Skipped:
        case WhyOn::Kind::Solo:
        case WhyOn::Kind::BlindEdit:
            if (tree != nullptr)
            {
                bool ok = line.layerId != 0xFFFFFFFF ? tree->selectNode(line.layerId, line.cueId) : false;
                if (!ok)
                    ok = tree->selectNode(line.cueId);
                if (!ok && line.functionId != 0xFFFFFFFF)
                    ok = tree->reveal(line.functionId);
                tree->bringForward();
            }
            break;
        case WhyOn::Kind::Function:
        case WhyOn::Kind::Effect:
        case WhyOn::Kind::TrackedRestack:
            if (tree != nullptr && line.functionId != 0xFFFFFFFF)
            {
                if (m_doc->showTree() != nullptr && m_doc->showTree()->isGenerated(line.functionId))
                    tree->reveal(line.functionId);
                else
                    tree->openFunction(line.functionId);
            }
            break;
        case WhyOn::Kind::SimpleDesk:
            bringForward(SimpleDesk::instance());
            break;
        case WhyOn::Kind::GroupMaster:
            // Where the group (and its master) is set up.
            bringForward(FixtureManager::instance());
            break;
        case WhyOn::Kind::VCWidget:
            bringForward(VirtualConsole::instance());
            break;
        case WhyOn::Kind::Park:
        case WhyOn::Kind::Highlight:
        case WhyOn::Kind::Flash:
        case WhyOn::Kind::Focus:
        case WhyOn::Kind::Mark:
            bringForward(Monitor::instance());
            break;
        case WhyOn::Kind::NotPatched:
            bringForward(FixtureManager::instance());
            break;
        default:
            if (line.widgetId != 0xFFFFFFFF)
                bringForward(VirtualConsole::instance());
            break;
    }
}

void WhyOnPanel::place(const QPoint &globalPos)
{
    if (globalPos.isNull())
        return;
    QPoint at = globalPos + QPoint(12, 12);
    if (QScreen *screen = QGuiApplication::screenAt(globalPos))
    {
        const QRect r = screen->availableGeometry();
        at.setX(qMin(at.x(), r.right() - width()));
        at.setY(qMin(at.y(), r.bottom() - height()));
    }
    move(at);
}

void WhyOnPanel::showEvent(QShowEvent *e)
{
    QFrame::showEvent(e);
    m_timer->start();
}

void WhyOnPanel::hideEvent(QHideEvent *e)
{
    m_timer->stop();
    QFrame::hideEvent(e);
}
