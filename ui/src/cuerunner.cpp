/*
  Q Light Controller Plus
  cuerunner.cpp

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

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QShortcut>
#include <QLabel>
#include <QTimer>

#include "showtreecontroller.h"
#include "timecodesource.h"
#include "chaserrunner.h"
#include "cuerunner.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
enum { RoleStep = Qt::UserRole, RoleSub = Qt::UserRole + 1, RoleCue = Qt::UserRole + 2 };
enum { ColState = 0, ColCue, ColName, ColTiming, ColNote };

QString seconds(uint ms)
{
    return QStringLiteral("%1s").arg(ms / 1000.0);
}
} // namespace

CueRunner::CueRunner(Doc *doc, Size size, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_size(size)
    , m_follow(invalidId())
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("cueRunner"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#the-cue-runner"));
    buildLayout();

    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &CueRunner::refresh);
    ShowTreeController *st = m_doc->showTree();
    connect(st, &ShowTreeController::changed, this, [this]() { m_treeDirty = true; });
    connect(st, &ShowTreeController::currentProductionChanged, this, [this]() {
        m_treeDirty = true;
        fillFollowBox();
        refresh();
    });
    connect(m_doc->showControl(), &ShowControl::mainCueListChanged, this, [this]() { m_treeDirty = true; });
    connect(m_doc->showControl(), &ShowControl::commandHandled, this, [this]() { refresh(); });
    refresh();
}

CueRunner::~CueRunner()
{
}

QString CueRunner::sizeToString(Size size)
{
    switch (size)
    {
        case Full: return QStringLiteral("Full");
        case List: return QStringLiteral("List");
        default: return QStringLiteral("Compact");
    }
}

CueRunner::Size CueRunner::stringToSize(const QString &str)
{
    if (str == QLatin1String("Full"))
        return Full;
    if (str == QLatin1String("List"))
        return List;
    return Compact;
}

void CueRunner::buildLayout()
{
    const bool full = m_size == Full;
    const bool compact = m_size == Compact;
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(full ? 8 : 2, full ? 8 : 2, full ? 8 : 2, full ? 8 : 2);
    v->setSpacing(full ? 6 : 2);

    QFont big = font();
    big.setPointSizeF(big.pointSizeF() * (full ? 1.6 : 1.0));
    QFont huge = font();
    huge.setPointSizeF(huge.pointSizeF() * (full ? 2.6 : compact ? 1.5 : 1.2));
    huge.setBold(true);

    // Header: production · list, the timecode, LIVE / DESIGN.
    QHBoxLayout *head = new QHBoxLayout();
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("crHeader"));
    QFont hf = big;
    hf.setBold(true);
    m_header->setFont(hf);
    head->addWidget(m_header, 1);
    if (!compact)
    {
        m_followBox = new QComboBox(this);
        m_followBox->setObjectName(QStringLiteral("crFollow"));
        m_followBox->setToolTip(tr("Which cue list this runner plays: the current production's main list (what "
                                   "the footer GO and show control play), or one list of any production"));
        head->addWidget(m_followBox);
        connect(m_followBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            setFollow(m_followBox->itemData(index).toUInt());
        });
    }
    m_timecode = new QLabel(this);
    m_timecode->setObjectName(QStringLiteral("crTimecode"));
    m_timecode->setFont(big);
    m_timecode->setToolTip(tr("The timecode the current production follows (its source, else the show's)"));
    head->addWidget(m_timecode);
    m_mode = new QLabel(this);
    m_mode->setObjectName(QStringLiteral("crMode"));
    m_mode->setFont(hf);
    m_mode->setToolTip(tr("LIVE (Operate): the cue lists own the stage. DESIGN: the Show Tree previews what "
                          "is selected. ⌘F12 switches."));
    head->addWidget(m_mode);
    v->addLayout(head);

    if (!compact)
    {
        m_tree = new QTreeWidget(this);
        m_tree->setObjectName(QStringLiteral("crCues"));
        m_tree->setHeaderLabels({ QString(), tr("Cue"), tr("Name"), tr("Timing"), tr("Note") });
        m_tree->setRootIsDecorated(true);
        m_tree->setUniformRowHeights(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setFont(big);
        m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
        m_tree->setToolTip(tr("Click a cue: LOAD it (it becomes STANDBY, what GO plays). Double-click: jump to "
                              "it now (a sub-cue: its cue first)."));
        connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
            load(rowCue(item));
        });
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
            jump(rowCue(item));
        });
        v->addWidget(m_tree, 1);
    }

    m_now = new QLabel(this);
    m_now->setObjectName(QStringLiteral("crNow"));
    m_now->setFont(hf);
    m_now->setWordWrap(true);
    v->addWidget(m_now);
    m_next = new QLabel(this);
    m_next->setObjectName(QStringLiteral("crNext"));
    m_next->setFont(big);
    m_next->setWordWrap(true);
    m_next->setToolTip(tr("What GO plays, and what changes on stage: only the attributes that differ from "
                          "now, from the tracking"));
    v->addWidget(m_next);
    m_subNext = new QLabel(this);
    m_subNext->setObjectName(QStringLiteral("crSubNext"));
    m_subNext->setFont(big);
    m_subNext->setWordWrap(true);
    v->addWidget(m_subNext);

    auto button = [this](const QString &text, const QString &tip, const char *name, const QFont &f) {
        QToolButton *b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setObjectName(QLatin1String(name));
        b->setFont(f);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        return b;
    };
    QHBoxLayout *bar = new QHBoxLayout();
    m_go = button(tr("GO"), tr("GO (⌘↩, Space in the Run tab): the loaded cue, else the next"), "crGo", huge);
    m_go->setStyleSheet(QStringLiteral("QToolButton { background: #2e7d32; color: white; }"));
    m_subGo = button(tr("Sub GO"), tr("Sub GO (⌥⌘↩): the next sub-cue of the cue playing"), "crSubGo", huge);
    connect(m_go, &QToolButton::clicked, this, &CueRunner::go);
    connect(m_subGo, &QToolButton::clicked, this, &CueRunner::subGo);
    QFont small = big;
    if (compact)
        small.setPointSizeF(font().pointSizeF());
    m_subBack = button(tr("Sub Back"), tr("Sub Back (⌥⌘⇧↩): the previous sub-cue"), "crSubBack", small);
    m_back = button(tr("Back"), tr("Back (⌘⇧↩): the previous main cue (its sub-cues dropped)"), "crBack", small);
    m_pause = button(tr("Pause"), tr("Pause / Resume: holds the fade and an auto-follow's wait"), "crPause", small);
    m_stop = button(tr("Stop"), tr("Stop this list (⌘⇧Esc stops everything)"), "crStop", small);
    connect(m_subBack, &QToolButton::clicked, this, &CueRunner::subBack);
    connect(m_back, &QToolButton::clicked, this, &CueRunner::back);
    connect(m_pause, &QToolButton::clicked, this, &CueRunner::pauseResume);
    connect(m_stop, &QToolButton::clicked, this, &CueRunner::stop);
    if (compact)
    {
        bar->addWidget(m_go, 2);
        bar->addWidget(m_subGo, 1);
        for (QToolButton *b : { m_subBack, m_back, m_pause, m_stop })
            b->hide();
    }
    else
    {
        bar->addWidget(m_back, 1);
        bar->addWidget(m_subBack, 1);
        bar->addWidget(m_go, 3);
        bar->addWidget(m_subGo, 2);
        bar->addWidget(m_pause, 1);
        bar->addWidget(m_stop, 1);
    }
    if (full)
    {
        for (QToolButton *b : { m_go, m_subGo })
            b->setMinimumHeight(96);
        for (QToolButton *b : { m_subBack, m_back, m_pause, m_stop })
            b->setMinimumHeight(64);
        // Keyboard GO in the Run tab, as QLab's: Space.
        QShortcut *space = new QShortcut(QKeySequence(Qt::Key_Space), this, nullptr, nullptr,
                                         Qt::WidgetWithChildrenShortcut);
        connect(space, &QShortcut::activated, this, &CueRunner::go);
    }
    v->addLayout(bar, full ? 0 : 1);
    fillFollowBox();
}

void CueRunner::setRunnerSize(Size size)
{
    if (size == m_size)
        return;
    m_size = size;
    qDeleteAll(findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
    for (QShortcut *s : findChildren<QShortcut *>(QString(), Qt::FindDirectChildrenOnly))
        delete s;
    delete layout();
    m_header = nullptr;
    m_followBox = nullptr;
    m_tree = nullptr;
    buildLayout();
    m_treeDirty = true;
    refresh();
}

void CueRunner::setFollow(quint32 listId)
{
    if (listId == m_follow)
        return;
    m_follow = listId;
    m_treeDirty = true;
    fillFollowBox();
    refresh();
    emit followChanged();
}

void CueRunner::fillFollowBox()
{
    if (m_followBox == nullptr)
        return;
    ShowTreeController *st = m_doc->showTree();
    QSignalBlocker block(m_followBox);
    m_followBox->clear();
    const Production *cur = st->production(st->currentProduction());
    m_followBox->addItem(cur == nullptr ? tr("The main cue list") : tr("Current production (%1)").arg(cur->name),
                         invalidId());
    for (const Production &p : st->productions())
        for (quint32 lid : st->playableLists(p.id))
            m_followBox->addItem(QStringLiteral("%1 · %2").arg(p.name, st->cueList(lid)->name), lid);
    m_followBox->setCurrentIndex(qMax(0, m_followBox->findData(m_follow)));
}

Chaser *CueRunner::chaser() const
{
    if (m_follow != invalidId())
    {
        const CueList *l = m_doc->showTree()->cueList(m_follow);
        return l == nullptr ? nullptr : qobject_cast<Chaser *>(m_doc->function(l->chaser));
    }
    return m_doc->showControl()->cueList(QString());
}

quint32 CueRunner::listId() const
{
    const Chaser *ch = chaser();
    quint32 lid = invalidId();
    if (ch != nullptr)
        m_doc->showTree()->productionOfChaser(ch->id(), &lid);
    return lid;
}

bool CueRunner::execute(ShowControl::Command command, const QString &cue)
{
    Chaser *ch = chaser();
    if (ch == nullptr)
        return false;
    ShowControl::Request r;
    r.command = command;
    r.cue = cue;
    r.chaser = ch->id();
    const bool ok = m_doc->showControl()->execute(r);
    refresh();
    return ok;
}

void CueRunner::go()
{
    execute(ShowControl::Go);
}

void CueRunner::back()
{
    execute(ShowControl::Back);
}

void CueRunner::subGo()
{
    execute(ShowControl::SubGo);
}

void CueRunner::subBack()
{
    execute(ShowControl::SubBack);
}

void CueRunner::pauseResume()
{
    Chaser *ch = chaser();
    if (ch != nullptr)
        execute(ch->isPaused() ? ShowControl::Resume : ShowControl::Pause);
}

void CueRunner::stop()
{
    execute(ShowControl::Stop);
}

void CueRunner::load(const QString &cue)
{
    if (!cue.isEmpty())
        execute(ShowControl::Load, cue);
}

void CueRunner::jump(const QString &cue)
{
    if (!cue.isEmpty())
        execute(ShowControl::Go, cue);
}

QString CueRunner::rowCue(const QTreeWidgetItem *item) const
{
    return item == nullptr ? QString() : item->data(ColCue, RoleCue).toString();
}

void CueRunner::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    m_timer->start();
    refresh();
}

void CueRunner::hideEvent(QHideEvent *ev)
{
    m_timer->stop();
    QWidget::hideEvent(ev);
}

/****************************************************************************
 * What it shows
 ****************************************************************************/

void CueRunner::rebuildTree()
{
    if (m_tree == nullptr)
        return;
    ShowTreeController *st = m_doc->showTree();
    Chaser *ch = chaser();
    const quint32 lid = listId();
    const QString key = QStringLiteral("%1|%2|%3").arg(ch == nullptr ? 0u : ch->id()).arg(lid)
                        .arg(ch == nullptr ? 0 : ch->stepsCount());
    if (!m_treeDirty && key == m_builtFor)
        return;
    m_treeDirty = false;
    m_builtFor = key;
    m_tree->clear();
    if (ch == nullptr)
        return;

    auto row = [](QTreeWidgetItem *item, int step, int sub, const QString &cue, const QString &name,
                  const QString &timing, const QString &note) {
        item->setData(ColCue, RoleStep, step);
        item->setData(ColCue, RoleSub, sub);
        item->setData(ColCue, RoleCue, cue);
        item->setText(ColCue, cue);
        item->setText(ColName, name);
        item->setText(ColTiming, timing);
        item->setText(ColNote, note);
    };

    const CueList *list = st->cueList(lid);
    if (list != nullptr)
    {
        for (int i = 0; i < list->cues.count(); ++i)
        {
            const Cue &c = list->cues.at(i);
            QStringList timing;
            if (c.fadeIn > 0)
                timing << tr("fade %1").arg(seconds(c.fadeIn));
            if (int(c.hold) >= 0)
                timing << tr("follow %1").arg(seconds(c.hold));
            QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
            row(item, i, -1, st->cueNumber(*list, i), c.name, timing.join(QStringLiteral(" · ")), c.note);
            if (const CueList *subs = st->cueList(st->subCueList(c.id)))
            {
                for (int k = 0; k < subs->cues.count(); ++k)
                {
                    const Cue &s = subs->cues.at(k);
                    QStringList t;
                    t << (k == 0 ? tr("with GO") : s.follow ? tr("auto %1").arg(seconds(s.wait)) : tr("Sub GO"));
                    if (s.fadeIn > 0)
                        t << tr("fade %1").arg(seconds(s.fadeIn));
                    QTreeWidgetItem *si = new QTreeWidgetItem(item);
                    row(si, i, k, st->cueNumber(*subs, k), s.name, t.join(QStringLiteral(" · ")), s.note);
                }
                item->setExpanded(true);
            }
        }
        return;
    }

    // A plain chaser (the main cue list isn't a tree list): its steps.
    const QList<ChaserStep> steps = ch->steps();
    for (int i = 0; i < steps.count(); ++i)
    {
        const Function *f = m_doc->function(steps.at(i).fid);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
        row(item, i, -1, ch->stepCue(i), f == nullptr ? QString() : f->name(),
            steps.at(i).fadeIn > 0 ? tr("fade %1").arg(seconds(steps.at(i).fadeIn)) : QString(), steps.at(i).note);
        for (int k = 0; k < ch->subCueCount(i); ++k)
        {
            QTreeWidgetItem *si = new QTreeWidgetItem(item);
            row(si, i, k, ch->subCue(i, k), QString(), QString(), QString());
        }
        item->setExpanded(ch->isSubGoStep(i));
    }
}

/* A cue's label as the runner says it: "2.1 Hit", or the step's cue and
   function for a plain chaser. */
static QString cueText(Doc *doc, Chaser *ch, quint32 listId, int step, int sub)
{
    ShowTreeController *st = doc->showTree();
    const quint32 cueId = st->cueOfStep(listId, step, sub);
    if (const CueList *l = st->listOfCue(cueId))
        return st->cueLabel(*l, l->indexOf(cueId));
    if (sub >= 0)
        return ch->subCue(step, sub);
    const QList<ChaserStep> steps = ch->steps();
    const Function *f = step >= 0 && step < steps.count() ? doc->function(steps.at(step).fid) : nullptr;
    return f == nullptr ? ch->stepCue(step) : QStringLiteral("%1 %2").arg(ch->stepCue(step), f->name());
}

void CueRunner::updateStates()
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = chaser();
    const quint32 lid = listId();

    // Header.
    {
        QString head;
        if (ch == nullptr)
            head = m_follow == invalidId() ? tr("No main cue list") : tr("No such cue list");
        else
        {
            quint32 pid = st->productionOfChaser(ch->id());
            const Production *p = st->production(pid);
            const CueList *l = st->cueList(lid);
            head = p != nullptr && l != nullptr ? QStringLiteral("%1 · %2").arg(p->name, l->name) : ch->name();
        }
        m_header->setText(head);
    }
    // LIVE / DESIGN, the timecode.
    {
        const bool live = m_doc->mode() == Doc::Operate;
        m_mode->setText(live ? tr("LIVE") : tr("DESIGN"));
        m_mode->setStyleSheet(live ? QStringLiteral("QLabel { color: white; background: #c62828; padding: 1px 6px; }")
                                   : QStringLiteral("QLabel { color: white; background: #1565c0; padding: 1px 6px; }"));
        m_timecode->setText(timecodeText());
    }

    const bool running = ch != nullptr && ch->isRunning();
    const int cur = running ? ch->currentStepIndex() : -1;
    const int curSub = running ? ch->currentSubCueIndex() : -1;
    const int standby = ch == nullptr ? -1 : sc->standbyStep(ch);
    const bool loaded = ch != nullptr && sc->loadedStep(ch->id()) >= 0;
    const int loadedSub = loaded ? sc->loadedSubCue(ch->id()) : -1;
    const int subNext = running && ch->isSubGoStep(cur) ? ch->nextSubCueIndex() : -1;

    // The fade of what plays: the sub-cue's own when it has sub-cues.
    int progress = -1;
    if (running)
    {
        ChaserRunnerStep step(ch->currentRunningStep());
        if (Chaser *inner = qobject_cast<Chaser *>(step.m_function))
            if (inner->isRunning())
                step = inner->currentRunningStep();
        if (step.m_function != nullptr && step.m_fadeIn != Function::defaultSpeed() && step.m_fadeIn > 0
            && step.m_elapsed < step.m_fadeIn)
            progress = int(100.0 * step.m_elapsed / step.m_fadeIn);
    }

    // Rows: done, ▶ running, ▷ STANDBY, NEXT, SUB NEXT.
    if (m_tree != nullptr)
    {
        const QBrush grey = palette().brush(QPalette::Disabled, QPalette::Text);
        const QBrush normal = palette().brush(QPalette::Text);
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        {
            QTreeWidgetItem *item = *it;
            const int step = item->data(ColCue, RoleStep).toInt();
            const int sub = item->data(ColCue, RoleSub).toInt();
            QString state;
            bool done = false;
            if (sub < 0)
            {
                if (step == cur)
                    state = progress >= 0 && curSub < 0 ? tr("▶ %1%").arg(progress) : tr("▶");
                else if (step == standby)
                    state = loaded && loadedSub < 0 ? tr("▷ STANDBY") : tr("NEXT");
                else
                    done = running && step < cur;
            }
            else if (step == cur)
            {
                if (sub == curSub)
                    state = progress >= 0 ? tr("▶ %1%").arg(progress) : tr("▶");
                else if (sub == subNext)
                    state = tr("SUB NEXT");
                done = sub < curSub;
            }
            if (sub >= 0 && step == standby && loaded && sub == loadedSub)
                state = tr("▷ STANDBY");
            if (item->text(ColState) != state)
                item->setText(ColState, state);
            QFont f = item->font(ColName);
            const bool bold = !state.isEmpty();
            if (f.bold() != bold)
            {
                f.setBold(bold);
                for (int c = 0; c <= ColNote; ++c)
                    item->setFont(c, f);
            }
            for (int c = 0; c <= ColNote; ++c)
                item->setForeground(c, done ? grey : normal);
        }
    }

    // Now / next / sub next, each with what changes.
    const quint32 nowCue = running ? st->cueOfStep(lid, cur, curSub) : invalidId();
    if (ch == nullptr)
        m_now->setText(QString());
    else if (!running)
        m_now->setText(tr("Stopped"));
    else
    {
        QString t = tr("▶ %1").arg(cueText(m_doc, ch, lid, cur, -1));
        if (curSub >= 0)
            t += QStringLiteral(" · ") + cueText(m_doc, ch, lid, cur, curSub);
        if (ch->isPaused())
            t += tr("  (paused)");
        else if (progress >= 0)
            t += tr("  %1%").arg(progress);
        m_now->setText(t);
    }

    if (ch == nullptr || standby < 0)
        m_next->setText(ch == nullptr ? QString() : tr("NEXT: end of the list"));
    else
    {
        // GO into a cue with sub-cues plays its first.
        int toSub = loaded ? loadedSub : -1;
        if (toSub < 0 && ch->isSubGoStep(standby))
            toSub = 0;
        const quint32 toCue = st->cueOfStep(lid, standby, toSub);
        QString t = (loaded ? tr("▷ STANDBY %1") : tr("NEXT %1")).arg(cueText(m_doc, ch, lid, standby, toSub < 0 || !loaded ? -1 : toSub));
        if (toCue != invalidId())
        {
            const QString what = st->changeLine(nowCue, toCue);
            t += QStringLiteral(": ") + (what.isEmpty() ? tr("no change") : what);
        }
        m_next->setText(t);
    }

    if (subNext >= 0)
    {
        QString t = tr("SUB NEXT %1").arg(cueText(m_doc, ch, lid, cur, subNext));
        const quint32 toCue = st->cueOfStep(lid, cur, subNext);
        if (toCue != invalidId())
        {
            const QString what = st->changeLine(nowCue, toCue);
            t += QStringLiteral(": ") + (what.isEmpty() ? tr("no change") : what);
        }
        m_subNext->setText(t);
    }
    else if (running && ch->isSubGoStep(cur))
        m_subNext->setText(tr("SUB: end of %1").arg(ch->stepCue(cur)));
    else
        m_subNext->setText(QString());
    m_subNext->setVisible(!m_subNext->text().isEmpty());

    // Buttons.
    m_go->setEnabled(ch != nullptr && standby >= 0);
    m_subGo->setEnabled(subNext >= 0);
    m_subBack->setEnabled(running && ch->isSubGoStep(cur) && ch->previousSubCueIndex() >= 0);
    m_back->setEnabled(running);
    m_pause->setEnabled(running);
    m_pause->setText(running && ch->isPaused() ? tr("Resume") : tr("Pause"));
    m_stop->setEnabled(running);
}

void CueRunner::refresh()
{
    rebuildTree();
    updateStates();
}

QString CueRunner::headerText() const
{
    return m_header->text();
}

QString CueRunner::nowText() const
{
    return m_now->text();
}

QString CueRunner::nextText() const
{
    return m_next->text();
}

QString CueRunner::subNextText() const
{
    return m_subNext->text();
}

QString CueRunner::modeText() const
{
    return m_mode->text();
}

QString CueRunner::timecodeText() const
{
    TimecodeSource *tc = m_doc->timecodeSource();
    if (tc == nullptr || tc->health() == TimecodeSource::NoSource)
        return tr("TC —");
    const quint32 ms = tc->positionMs();
    const int fps = qMax(1, tc->fps());
    const QString t = QStringLiteral("%1:%2:%3:%4")
                      .arg(ms / 3600000, 2, 10, QLatin1Char('0'))
                      .arg((ms / 60000) % 60, 2, 10, QLatin1Char('0'))
                      .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'))
                      .arg(int((ms % 1000) * fps / 1000), 2, 10, QLatin1Char('0'));
    return tc->isRunning() ? tr("TC %1").arg(t) : tr("TC %1 hold").arg(t);
}
