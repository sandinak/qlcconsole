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
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>
#include <QScreen>
#include <QTimer>
#include <QLabel>

#include "whyonpanel.h"
#include "virtualconsole.h"
#include "showtreecontroller.h"
#include "teachingsettings.h"
#include "showtreeview.h"
#include "simpledesk.h"
#include "fixturemanager.h"
#include "qlcchannel.h"
#include "vcwidget.h"
#include "monitor.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QPointer<WhyOnPanel> s_panel;
WhyOnPanel::Jump s_jumpOverride;

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
    bar->addStretch(1);
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
    connect(m_plain, &QCheckBox::toggled, this, &WhyOnPanel::refresh);
    connect(m_copy, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_answer.toText());
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

    const int keep = m_list->currentRow();
    m_list->clear();
    const QColor dim = palette().color(QPalette::Disabled, QPalette::Text);
    for (const WhyOn::Line &l : m_answer.lines)
    {
        QListWidgetItem *item = new QListWidgetItem(l.text, m_list);
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
        item->setToolTip(goes ? tr("Click to go there") : QString());
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
}

void WhyOnPanel::activate(int row)
{
    if (row < 0 || row >= m_answer.lines.count())
        return;
    jump(m_answer.lines.at(row));
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
        case WhyOn::Kind::Cue:
        case WhyOn::Kind::Hit:
        case WhyOn::Kind::Preview:
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
            bringForward(Monitor::instance());
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
