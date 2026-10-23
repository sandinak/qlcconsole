/*
  Q Light Controller Plus
  chaserwalker.cpp

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

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDataStream>
#include <QHeaderView>
#include <QTreeWidget>
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QLabel>
#include <QTimer>

#include "chaserwalker.h"
#include "noteedit.h"
#include "workbenchstack.h"
#include "programmingoutline.h"
#include "functionselection.h"
#include "qlceventpos.h"
#include "paletteops.h"
#include "chaserstep.h"
#include "collection.h"
#include "chaser.h"
#include "doc.h"

// The step table: the Chaser editor's columns.
enum { ColNum = 0, ColName, ColFadeIn, ColHold, ColFadeOut, ColDuration, ColNotes, ColCue };
enum { FadeInField = 0, HoldField, FadeOutField, CueField, NoteField };

ChaserWalker::ChaserWalker(FunctionStack *stack, QWidget *parent)
    : QWidget(parent)
    , m_stack(stack)
{
    setObjectName(QStringLiteral("chaserWalker"));
    QVBoxLayout *col = new QVBoxLayout(this);
    col->setContentsMargins(4, 0, 4, 0);
    col->setSpacing(3);

    // ◀ prev · step N · next ▶
    QHBoxLayout *walk = new QHBoxLayout;
    m_prev = new QToolButton(this);
    m_prev->setObjectName(QStringLiteral("walkerPrev"));
    m_prev->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_prev, &QToolButton::clicked, this, [this]() {
        commitPending();
        if (m_stack != nullptr)
            m_stack->setStep(m_stack->step() - 1);
    });
    walk->addWidget(m_prev);
    m_where = new QLabel(this);
    m_where->setObjectName(QStringLiteral("walkerStep"));
    m_where->setAlignment(Qt::AlignCenter);
    m_where->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
    walk->addWidget(m_where, 1);
    m_next = new QToolButton(this);
    m_next->setObjectName(QStringLiteral("walkerNext"));
    m_next->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_next, &QToolButton::clicked, this, [this]() {
        commitPending();
        if (m_stack != nullptr)
            m_stack->setStep(m_stack->step() + 1);
    });
    walk->addWidget(m_next);
    col->addLayout(walk);

    // The step's header: fade in · hold · fade out · cue · note.
    QHBoxLayout *fields = new QHBoxLayout;
    fields->setSpacing(4);
    // Top-aligned: the note grows downwards while it is typed in.
    auto label = [this, fields](const QString &text) {
        QLabel *l = new QLabel(text, this);
        l->setMinimumHeight(QLineEdit().sizeHint().height());
        fields->addWidget(l, 0, Qt::AlignTop);
        return l;
    };
    auto field = [this, fields, label](const QString &text, const QString &name, int width) {
        QLabel *l = label(text);
        QLineEdit *e = new QLineEdit(this);
        e->setObjectName(name);
        e->setMaximumWidth(width);
        e->setMinimumWidth(width);
        l->setBuddy(e);
        fields->addWidget(e, 0, Qt::AlignTop);
        return e;
    };
    const int timeWidth = fontMetrics().horizontalAdvance(QStringLiteral("00m00s.00")) + 12;
    m_fadeIn = field(tr("Fade in"), QStringLiteral("walkerFadeIn"), timeWidth);
    m_hold = field(tr("Hold"), QStringLiteral("walkerHold"), timeWidth);
    m_fadeOut = field(tr("Fade out"), QStringLiteral("walkerFadeOut"), timeWidth);
    m_cue = field(tr("Cue"), QStringLiteral("walkerCue"), fontMetrics().horizontalAdvance(QStringLiteral("000.00")) + 12);
    QLabel *noteLabel = label(tr("Note"));
    m_note = new NoteEdit(this);
    m_note->setObjectName(QStringLiteral("walkerNote"));
    noteLabel->setBuddy(m_note);
    fields->addWidget(m_note, 1);
    const QString timeTip = tr("As the Chaser editor writes it: 1s.50, 2m, ∞ (hold until GO). Return applies it.");
    for (QLineEdit *e : { m_fadeIn, m_hold, m_fadeOut })
        e->setToolTip(timeTip);
    m_cue->setToolTip(tr("The step's cue number for show control (empty: its place)"));
    m_note->setHelpTip(tr("The step's note, as in the Chaser editor and a VC Cue List. It opens up while "
                          "you type: Return applies it, Shift+Return starts a new line, Esc puts it back."));
    // Return or focus out applies a field, to the step it shows; walking to
    // another step applies it first (Q60). Esc puts it back.
    connect(m_fadeIn, &QLineEdit::editingFinished, this, [this]() {
        m_fadeIn->setModified(false);
        commitTime(FadeInField, m_fadeIn->text());
    });
    connect(m_hold, &QLineEdit::editingFinished, this, [this]() {
        m_hold->setModified(false);
        commitTime(HoldField, m_hold->text());
    });
    connect(m_fadeOut, &QLineEdit::editingFinished, this, [this]() {
        m_fadeOut->setModified(false);
        commitTime(FadeOutField, m_fadeOut->text());
    });
    connect(m_cue, &QLineEdit::editingFinished, this, [this]() {
        m_cue->setModified(false);
        commitText(CueField, m_cue->text(), m_shownStep);
    });
    for (QLineEdit *e : { m_fadeIn, m_hold, m_fadeOut, m_cue })
        e->installEventFilter(this);
    connect(m_note, &NoteEdit::editingFinished, this, [this]() { commitText(NoteField, m_note->text()); });
    col->addLayout(fields);

    // The step's function, shared: say so, and split it off.
    QHBoxLayout *shared = new QHBoxLayout;
    m_shared = new QLabel(this);
    m_shared->setObjectName(QStringLiteral("walkerShared"));
    m_shared->setWordWrap(true);
    m_shared->setStyleSheet(QStringLiteral("QLabel { color: #d08a2a; }"));
    shared->addWidget(m_shared, 1);
    m_ownCopy = new QPushButton(tr("Make this step's own copy"), this);
    m_ownCopy->setObjectName(QStringLiteral("walkerOwnCopy"));
    m_ownCopy->setToolTip(tr("Split it off: this step plays a copy of its own, and an edit here changes "
                             "only this step. One step on the history."));
    connect(m_ownCopy, &QPushButton::clicked, this, [this]() {
        if (m_stack != nullptr)
            m_stack->makeStepOwnCopy();
    });
    shared->addWidget(m_ownCopy);
    m_openStep = new QPushButton(this);
    m_openStep->setObjectName(QStringLiteral("walkerOpenStep"));
    m_openStep->setToolTip(tr("Load the step's function in the Workbench on its own; ↩ comes back to this step"));
    connect(m_openStep, &QPushButton::clicked, this, [this]() {
        if (m_stack != nullptr && m_stack->stepFunction() != Function::invalidId())
            emit openRequested(m_stack->stepFunction());
    });
    shared->addWidget(m_openStep);
    col->addLayout(shared);

    buildTable();
    col->addWidget(m_table);

    QHBoxLayout *bar = new QHBoxLayout;
    bar->setSpacing(4);
    auto tool = [this, bar](const QString &text, const QString &name, const QString &tip) {
        QToolButton *b = new QToolButton(this);
        b->setObjectName(name);
        b->setText(text);
        b->setToolTip(tip);
        bar->addWidget(b);
        return b;
    };
    m_addStep = tool(tr("+ Step…"), QStringLiteral("walkerAddStep"),
                     tr("Add scenes (or any function) from the show as steps after the one selected"));
    connect(m_addStep, &QToolButton::clicked, this, &ChaserWalker::addStepsPicked);
    m_dropHint = new QLabel(tr("or drag scenes onto the steps"), this);
    m_dropHint->setObjectName(QStringLiteral("walkerHint"));
    m_dropHint->setStyleSheet(QStringLiteral("QLabel { font-style: italic; }"));
    bar->addWidget(m_dropHint, 1);
    m_upStep = tool(QStringLiteral("▲"), QStringLiteral("walkerStepUp"), tr("Move the step earlier"));
    connect(m_upStep, &QToolButton::clicked, this, [this]() {
        if (m_stack != nullptr)
            m_stack->moveStep(m_stack->step(), m_stack->step() - 1);
    });
    m_downStep = tool(QStringLiteral("▼"), QStringLiteral("walkerStepDown"), tr("Move the step later"));
    connect(m_downStep, &QToolButton::clicked, this, [this]() {
        if (m_stack != nullptr)
            m_stack->moveStep(m_stack->step(), m_stack->step() + 1);
    });
    m_removeStep = tool(tr("Remove step"), QStringLiteral("walkerRemoveStep"),
                        tr("Take the step out of the chaser; its function stays in the show"));
    connect(m_removeStep, &QToolButton::clicked, this, [this]() {
        if (m_stack != nullptr)
            m_stack->removeStep(m_stack->step());
    });
    col->addLayout(bar);

    // What it shows changes with the show and with the history.
    if (m_stack != nullptr)
    {
        connect(m_stack, &WorkbenchStack::changed, this, &ChaserWalker::scheduleRefresh);
        // Walked to another step: show it at once.
        connect(m_stack, &WorkbenchStack::layersReplaced, this, &ChaserWalker::refresh);
        Doc *doc = m_stack->doc();
        connect(doc, &Doc::functionChanged, this, [this](quint32) { scheduleRefresh(); });
        connect(doc, &Doc::functionRemoved, this, [this](quint32) { scheduleRefresh(); });
        if (doc->paletteOps() != nullptr)
            connect(doc->paletteOps(), &PaletteOps::changed, this, &ChaserWalker::scheduleRefresh);
    }
    refresh();
}

void ChaserWalker::buildTable()
{
    m_table = new QTreeWidget(this);
    m_table->setObjectName(QStringLiteral("walkerSteps"));
    m_table->setRootIsDecorated(false);
    m_table->setAllColumnsShowFocus(true);
    m_table->setUniformRowHeights(true);
    // Only the note is typed in here (double-click it); the rest is the header's.
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setItemDelegateForColumn(ColNotes, new NoteDelegate(m_table));
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setHeaderLabels({ tr("#"), tr("Function"), tr("Fade In"), tr("Hold"), tr("Fade Out"),
                               tr("Duration"), tr("Notes"), tr("Cue") });
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->setAcceptDrops(true);
    m_table->viewport()->setAcceptDrops(true);
    m_table->viewport()->installEventFilter(this);
    m_table->setToolTip(tr("The chaser's steps, as the Chaser editor lists them. Click one to walk to it; "
                           "double-click to load it on its own."));
    connect(m_table, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
        if (m_refreshing || m_stack == nullptr || it == nullptr)
            return;
        commitPending();
        m_stack->setStep(m_table->indexOfTopLevelItem(it));
    });
    connect(m_table, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int column) {
        if (m_stack == nullptr || it == nullptr)
            return;
        const int i = m_table->indexOfTopLevelItem(it);
        Chaser *ch = m_stack->chaser();
        if (ch == nullptr || i < 0 || i >= ch->stepsCount())
            return;
        if (column == ColNotes && (it->flags() & Qt::ItemIsEditable))
            m_table->editItem(it, ColNotes);
        else
            emit openRequested(ch->steps().at(i).fid);
    });
    connect(m_table, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *it, int column) {
        if (m_refreshing || m_stack == nullptr || it == nullptr || column != ColNotes)
            return;
        commitText(NoteField, it->text(ColNotes), m_table->indexOfTopLevelItem(it));
    });
}

void ChaserWalker::scheduleRefresh()
{
    if (m_refreshQueued)
        return;
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshQueued = false;
        refresh();
    });
}

static QString timeText(Chaser::SpeedMode mode, uint ms)
{
    return mode == Chaser::Default ? ChaserWalker::tr("(default)") : Function::speedToString(ms);
}

void ChaserWalker::refresh()
{
    Chaser *ch = m_stack != nullptr ? m_stack->chaser() : nullptr;
    if (ch == nullptr)
        return;
    // Another step (or chaser) to show: what was typed for the one shown
    // goes to it first (Q60) -- a different chaser's, dropped.
    {
        const int to = qBound(0, m_stack->step(), qMax(0, ch->stepsCount() - 1));
        if (ch->id() != m_shownChaser)
            for (QLineEdit *e : { m_fadeIn, m_hold, m_fadeOut, m_cue })
                e->setModified(false);
        else if (to != m_shownStep)
            commitPending();
    }
    ch = m_stack->chaser();
    if (ch == nullptr)
        return;
    m_refreshing = true;
    Doc *doc = m_stack->doc();
    const QList<ChaserStep> steps = ch->steps();
    const int n = steps.size();
    const int cur = qBound(0, m_stack->step(), qMax(0, n - 1));
    const bool otherStep = cur != m_shownStep || ch->id() != m_shownChaser;
    m_shownStep = cur;
    m_shownChaser = ch->id();
    const bool readOnly = !m_stack->readOnlyReason().isEmpty();

    // The step table, as the Chaser editor fills it.
    const QBrush grey = m_table->palette().brush(QPalette::Disabled, QPalette::Text);
    while (m_table->topLevelItemCount() > n)
        delete m_table->takeTopLevelItem(m_table->topLevelItemCount() - 1);
    for (int i = 0; i < n; ++i)
    {
        QTreeWidgetItem *it = m_table->topLevelItem(i);
        if (it == nullptr)
            it = new QTreeWidgetItem(m_table);
        const ChaserStep &st = steps.at(i);
        Function *f = doc->function(st.fid);
        it->setText(ColNum, QString::number(i + 1));
        it->setText(ColName, f != nullptr ? f->name() : tr("(missing function %1)").arg(st.fid));
        it->setIcon(ColName, f != nullptr ? f->getIcon() : QIcon());
        uint fadeIn = ch->fadeInMode() == Chaser::Common ? ch->fadeInSpeed() : st.fadeIn;
        uint fadeOut = ch->fadeOutMode() == Chaser::Common ? ch->fadeOutSpeed() : st.fadeOut;
        uint duration = ch->durationMode() == Chaser::Common ? ch->duration() : st.duration;
        uint hold = ch->durationMode() == Chaser::Common ? Function::speedSubtract(duration, fadeIn) : st.hold;
        it->setText(ColFadeIn, ch->fadeInMode() == Chaser::Default ? QString() : Function::speedToString(fadeIn));
        it->setText(ColFadeOut, ch->fadeOutMode() == Chaser::Default ? QString() : Function::speedToString(fadeOut));
        it->setText(ColHold, Function::speedToString(hold));
        it->setText(ColDuration, Function::speedToString(duration));
        it->setText(ColNotes, st.note);
        it->setFlags(readOnly ? it->flags() & ~Qt::ItemIsEditable : it->flags() | Qt::ItemIsEditable);
        const bool own = !st.cueNumber.trimmed().isEmpty();
        it->setText(ColCue, own ? st.cueNumber.trimmed() : QString::number(i + 1));
        it->setForeground(ColCue, own ? m_table->palette().brush(QPalette::Text) : grey);
        QFont font = m_table->font();
        font.setStrikeOut(st.skip);
        it->setFont(ColName, font);
    }
    if (QTreeWidgetItem *it = m_table->topLevelItem(cur))
    {
        m_table->setCurrentItem(it);
        it->setSelected(true);
        m_table->scrollToItem(it);
    }
    // Room for up to eight steps; the table scrolls past that.
    const int rowH = m_table->sizeHintForRow(0) > 0 ? m_table->sizeHintForRow(0) : fontMetrics().height() + 4;
    m_table->setFixedHeight(m_table->header()->sizeHint().height() + rowH * qBound(2, n, 8) + 6);

    // ◀ step N ▶
    Function *sf = doc->function(m_stack->stepFunction());
    m_prev->setText(cur > 0 && cur - 1 < n && doc->function(steps.at(cur - 1).fid) != nullptr
                    ? tr("◀ %1").arg(cur) : tr("◀"));
    m_prev->setEnabled(cur > 0);
    m_prev->setToolTip(tr("The step before"));
    m_next->setText(cur + 1 < n ? tr("%1 ▶").arg(cur + 2) : tr("▶"));
    m_next->setEnabled(cur + 1 < n);
    m_next->setToolTip(tr("The step after"));
    if (n == 0)
        m_where->setText(tr("No steps yet: + Step, or drag scenes onto the table"));
    else
        m_where->setText(tr("Step %1 of %2 · %3").arg(cur + 1).arg(n)
                         .arg(sf != nullptr ? sf->name() : tr("(missing)")));

    // The step's header fields, with the chaser's timing modes.
    const ChaserStep st = n > 0 ? steps.at(cur) : ChaserStep();
    auto setField = [readOnly, n, otherStep](QLineEdit *e, const QString &text, bool editable, const QString &why) {
        // Typing in it is left alone -- unless it now shows another step.
        if (otherStep || !e->hasFocus() || !e->isModified())
            e->setText(text);
        e->setEnabled(n > 0 && editable && !readOnly);
        if (!why.isEmpty())
            e->setToolTip(why);
    };
    const QString commonWhy = tr("The same for every step (the chaser's setting, in its editor on the right)");
    const QString defaultWhy = tr("Each step's function's own (the chaser's setting, in its editor on the right)");
    auto modeWhy = [&](Chaser::SpeedMode m) {
        return m == Chaser::Common ? commonWhy : m == Chaser::Default ? defaultWhy : QString();
    };
    const uint fadeIn = ch->fadeInMode() == Chaser::Common ? ch->fadeInSpeed() : st.fadeIn;
    const uint duration = ch->durationMode() == Chaser::Common ? ch->duration() : st.duration;
    setField(m_fadeIn, timeText(ch->fadeInMode(), fadeIn), ch->fadeInMode() == Chaser::PerStep,
             modeWhy(ch->fadeInMode()));
    setField(m_hold, Function::speedToString(ch->durationMode() == Chaser::Common
                                             ? Function::speedSubtract(duration, fadeIn) : st.hold),
             ch->durationMode() == Chaser::PerStep, modeWhy(ch->durationMode()));
    setField(m_fadeOut, timeText(ch->fadeOutMode(), ch->fadeOutMode() == Chaser::Common ? ch->fadeOutSpeed()
                                                                                        : st.fadeOut),
             ch->fadeOutMode() == Chaser::PerStep, modeWhy(ch->fadeOutMode()));
    setField(m_cue, st.cueNumber, true, QString());
    if (!m_note->hasFocus())
        m_note->setText(st.note);
    m_note->setEnabled(n > 0 && !readOnly);

    // Shared: where else it plays.
    QStringList also;
    if (sf != nullptr)
        for (const QString &w : FunctionStack::alsoIn(doc, sf->id(), Function::invalidId()))
            if (w != tr("%1 (step %2)").arg(ch->name()).arg(cur + 1))
                also << w;
    m_shared->setVisible(!also.isEmpty());
    m_ownCopy->setVisible(!also.isEmpty() && !readOnly);
    if (!also.isEmpty())
    {
        QStringList shown = also.mid(0, 3);
        if (also.size() > 3)
            shown << tr("%n more", "", int(also.size() - 3));
        m_shared->setText(tr("also in %1: an edit here changes it there too")
                              .arg(shown.join(QStringLiteral(", "))));
        m_shared->setToolTip(also.join(QLatin1Char('\n')));
    }
    m_openStep->setVisible(sf != nullptr);
    if (sf != nullptr)
        m_openStep->setText(tr("▣ Open %1").arg(Function::typeToString(sf->type()).toLower()));

    m_addStep->setEnabled(!readOnly);
    m_upStep->setEnabled(!readOnly && cur > 0);
    m_downStep->setEnabled(!readOnly && cur + 1 < n);
    m_removeStep->setEnabled(!readOnly && n > 0);
    m_dropHint->setVisible(!readOnly);
    m_refreshing = false;
}

void ChaserWalker::commitPending()
{
    if (m_refreshing || m_stack == nullptr || m_stack->chaser() == nullptr
        || m_stack->chaser()->id() != m_shownChaser || m_shownStep < 0)
        return;
    const int step = m_shownStep;
    const struct { QLineEdit *edit; int which; } times[] = {
        { m_fadeIn, FadeInField }, { m_hold, HoldField }, { m_fadeOut, FadeOutField } };
    for (const auto &t : times)
        if (t.edit->isModified())
        {
            t.edit->setModified(false);
            commitTime(t.which, t.edit->text(), step);
        }
    if (m_cue->isModified())
    {
        m_cue->setModified(false);
        commitText(CueField, m_cue->text(), step);
    }
}

void ChaserWalker::commitTime(int which, const QString &text, int step)
{
    Chaser *ch = m_stack != nullptr ? m_stack->chaser() : nullptr;
    if (ch == nullptr || m_refreshing || ch->stepsCount() == 0)
        return;
    const int index = step >= 0 ? step : (m_shownStep >= 0 ? m_shownStep : m_stack->step());
    if (index >= ch->stepsCount())
        return;
    const ChaserStep was = ch->steps().at(index);
    const uint ms = Function::stringToSpeed(text.trimmed());
    QString label;
    if (which == FadeInField && ms != was.fadeIn)
        label = tr("set step %1's fade in in %2");
    else if (which == HoldField && ms != was.hold)
        label = tr("set step %1's hold in %2");
    else if (which == FadeOutField && ms != was.fadeOut)
        label = tr("set step %1's fade out in %2");
    if (label.isEmpty())
    {
        refresh();
        return;
    }
    m_stack->editStep(index, label.arg(index + 1).arg(ch->name()), [which, ms](ChaserStep &st) {
        // As the Chaser editor keeps them: a step's duration is its fade in + hold.
        if (which == FadeInField)
            st.fadeIn = ms;
        else if (which == HoldField)
            st.hold = ms;
        else
            st.fadeOut = ms;
        st.duration = Function::speedAdd(st.fadeIn, st.hold);
    });
    refresh();
}

void ChaserWalker::commitText(int which, const QString &text, int step)
{
    Chaser *ch = m_stack != nullptr ? m_stack->chaser() : nullptr;
    if (ch == nullptr || m_refreshing || ch->stepsCount() == 0)
        return;
    const int index = step >= 0 ? step : m_stack->step();
    if (index >= ch->stepsCount())
        return;
    const ChaserStep was = ch->steps().at(index);
    const QString value = which == CueField ? text.trimmed() : text;
    if ((which == CueField ? was.cueNumber : was.note) == value)
        return;
    m_stack->editStep(index, (which == CueField ? tr("set step %1's cue number in %2")
                                                 : tr("set step %1's note in %2")).arg(index + 1).arg(ch->name()),
                      [which, value](ChaserStep &st) {
        if (which == CueField)
            st.cueNumber = value;
        else
            st.note = value;
    });
    refresh();
}

void ChaserWalker::addStepsPicked()
{
    Chaser *ch = m_stack != nullptr ? m_stack->chaser() : nullptr;
    if (ch == nullptr)
        return;
    Doc *doc = m_stack->doc();
    FunctionSelection fs(this, doc);
    fs.setMultiSelection(true);
    QList<quint32> disabled;
    disabled << ch->id();
    for (Function *f : doc->functions())
        if (f != nullptr && f->contains(ch->id()))
            disabled << f->id();
    fs.setDisabledFunctions(disabled);
    if (fs.exec() != QDialog::Accepted)
        return;
    const int at = ch->stepsCount() > 0 ? m_stack->step() + 1 : -1;
    if (m_stack->insertSteps(fs.selection(), at))
        m_stack->setStep(at < 0 ? 0 : at);
}

/* Outline rows (scenes, functions) dragged onto the step table: steps. */
static QList<quint32> draggedFunctions(const QMimeData *mime)
{
    QList<quint32> ids;
    if (mime == nullptr || !mime->hasFormat(QString::fromLatin1(ProgrammingOutline::rowMimeType())))
        return ids;
    QByteArray data = mime->data(QString::fromLatin1(ProgrammingOutline::rowMimeType()));
    QDataStream in(&data, QIODevice::ReadOnly);
    while (!in.atEnd())
    {
        qint32 kind = 0;
        quint32 scene = 0, id = 0;
        in >> kind >> scene >> id;
        if ((kind == ProgrammingOutline::SceneRow || kind == ProgrammingOutline::FunctionRow) && !ids.contains(id))
            ids << id;
    }
    return ids;
}

bool ChaserWalker::eventFilter(QObject *watched, QEvent *event)
{
    // Esc in a header field: what the step has, nothing applied.
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
        for (QLineEdit *e : { m_fadeIn, m_hold, m_fadeOut, m_cue })
            if (watched == e && e->isModified())
            {
                e->setModified(false);
                refresh();
                e->selectAll();
                return true;
            }
    if (watched != m_table->viewport() || m_stack == nullptr || m_stack->chaser() == nullptr)
        return QWidget::eventFilter(watched, event);
    const QEvent::Type t = event->type();
    if (t != QEvent::DragEnter && t != QEvent::DragMove && t != QEvent::Drop && t != QEvent::DragLeave)
        return QWidget::eventFilter(watched, event);
    if (t == QEvent::DragLeave)
    {
        m_dropHint->setText(tr("or drag scenes onto the steps"));
        return false;
    }
    QDropEvent *de = static_cast<QDropEvent *>(event);
    const QList<quint32> ids = draggedFunctions(de->mimeData());
    QTreeWidgetItem *over = m_table->itemAt(qlcEventPos(de));
    const int at = over != nullptr ? m_table->indexOfTopLevelItem(over) : -1;
    QString plan = ids.isEmpty() ? QString()
                                 : FunctionStack::stepsPlan(m_stack->doc(), m_stack->chaser()->id(), ids);
    if (!m_stack->readOnlyReason().isEmpty())
        plan = QStringLiteral("!") + m_stack->readOnlyReason();
    const bool ok = !plan.isEmpty() && !plan.startsWith(QLatin1Char('!'));
    if (t != QEvent::Drop)
    {
        m_dropHint->setText(plan.startsWith(QLatin1Char('!')) ? plan.mid(1)
                            : ok && at >= 0 ? tr("%1, before step %2").arg(plan).arg(at + 1) : plan);
        if (ok)
        {
            de->setDropAction(Qt::CopyAction);
            de->accept();
        }
        else
            de->ignore();
        return true;
    }
    m_dropHint->setText(tr("or drag scenes onto the steps"));
    if (!ok)
    {
        de->ignore();
        return true;
    }
    if (m_stack->insertSteps(ids, at))
        m_stack->setStep(at < 0 ? m_stack->stepCount() - int(ids.size()) : at);
    // Never MoveAction: the outline would take the dragged rows out.
    de->setDropAction(Qt::CopyAction);
    de->accept();
    return true;
}
