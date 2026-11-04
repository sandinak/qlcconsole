/*
  Q Light Controller Plus
  shownotespane.cpp

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

#include <QPlainTextEdit>
#include <QInputDialog>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QKeyEvent>
#include <QCheckBox>
#include <QPixmap>
#include <QBuffer>
#include <QLocale>
#include <QLabel>
#include <QMenu>

#include "shownotespane.h"
#include "showtreecontroller.h"
#include "structurestudioview.h"
#include "monitor.h"
#include "doc.h"

using namespace ShowTree;

static ShowNotesPane::ViewProvider s_provider;

void ShowNotesPane::setViewProvider(const ViewProvider &provider)
{
    s_provider = provider;
}

StructureStudioView *ShowNotesPane::rigView(bool open)
{
    if (s_provider)
        return s_provider(open);
    if (Monitor *mon = Monitor::instance())
        return mon->stageOverviewView(open);
    return nullptr;
}

QString ShowNotesPane::whenText(const QString &iso)
{
    if (iso.isEmpty())
        return tr("cue note");
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso;
    const bool thisYear = dt.date().year() == QDate::currentDate().year();
    return QLocale().toString(dt, thisYear ? QStringLiteral("d MMM HH:mm") : QStringLiteral("d MMM yyyy HH:mm"));
}

ShowNotesPane::ShowNotesPane(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("stNotesPane"));
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(4);

    QHBoxLayout *head = new QHBoxLayout();
    head->setContentsMargins(0, 0, 0, 0);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("stNotesTitle"));
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setTextFormat(Qt::PlainText);
    // Long names end in "…" rather than widening the column.
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    head->addWidget(m_title, 1);
    m_all = new QToolButton(this);
    m_all->setObjectName(QStringLiteral("stNotesAll"));
    m_all->setText(tr("All notes…"));
    m_all->setAutoRaise(true);
    m_all->setToolTip(tr("Every note in the show, to filter, jump to, print or export after rehearsal"));
    connect(m_all, &QToolButton::clicked, this, &ShowNotesPane::allNotesRequested);
    head->addWidget(m_all);
    v->addLayout(head);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("stNotesScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_entriesHost = new QWidget(scroll);
    m_entries = new QVBoxLayout(m_entriesHost);
    m_entries->setContentsMargins(0, 0, 0, 0);
    m_entries->setSpacing(6);
    m_entries->addStretch(1);
    scroll->setWidget(m_entriesHost);
    v->addWidget(scroll, 1);

    m_empty = new QLabel(this);
    m_empty->setObjectName(QStringLiteral("stNotesEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setEnabled(false);
    m_entries->insertWidget(0, m_empty);

    m_sizeWarning = new QLabel(this);
    m_sizeWarning->setObjectName(QStringLiteral("stNotesSizeWarning"));
    m_sizeWarning->setWordWrap(true);
    m_sizeWarning->hide();
    v->addWidget(m_sizeWarning);

    m_input = new QPlainTextEdit(this);
    m_input->setObjectName(QStringLiteral("stNoteInput"));
    m_input->setPlaceholderText(tr("Add a note — Return adds, Shift+Return for a new line"));
    m_input->setTabChangesFocus(true);
    {
        const int line = m_input->fontMetrics().lineSpacing();
        m_input->setFixedHeight(line * 2 + 12);
    }
    m_input->installEventFilter(this);
    v->addWidget(m_input);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    m_snapshot = new QPushButton(tr("Snapshot the View"), this);
    m_snapshot->setObjectName(QStringLiteral("stNoteSnapshot"));
    m_snapshot->setToolTip(tr("Adds the note with a snapshot of the view: the rig view's camera, a small "
                              "picture of it and the cue on stage. Show Me on the note puts it back."));
    connect(m_snapshot, &QPushButton::clicked, this, [this]() { submit(true); });
    m_add = new QPushButton(tr("Add"), this);
    m_add->setObjectName(QStringLiteral("stNoteAdd"));
    m_add->setToolTip(tr("Adds the note, dated and signed with Your name (Show Review), open until ticked done"));
    connect(m_add, &QPushButton::clicked, this, [this]() { submit(false); });
    buttons->addStretch(1);
    buttons->addWidget(m_snapshot);
    buttons->addWidget(m_add);
    v->addLayout(buttons);

    if (ShowTreeController *st = controller())
    {
        connect(st, &ShowTreeController::notesChanged, this, &ShowNotesPane::refresh);
        connect(st, &ShowTreeController::changed, this, &ShowNotesPane::refresh);
    }
    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, &ShowNotesPane::refresh);
    refresh();
}

QSize ShowNotesPane::sizeHint() const
{
    // Room for two or three notes under the page, not a sliver.
    return QSize(QWidget::sizeHint().width(), qMax(QWidget::sizeHint().height(), 300));
}

ShowTreeController *ShowNotesPane::controller() const
{
    return m_doc != nullptr ? m_doc->showTree() : nullptr;
}

void ShowNotesPane::setItem(NoteKind kind, quint32 target, quint32 layerCue)
{
    if (m_kind == kind && m_target == target && m_layerCue == layerCue)
        return;
    m_kind = kind;
    m_target = target;
    m_layerCue = kind == NoteKind::Layer ? layerCue : 0xFFFFFFFF;
    refresh();
}

void ShowNotesPane::clearItem()
{
    if (m_target == 0xFFFFFFFF)
        return;
    m_target = 0xFFFFFFFF;
    m_layerCue = 0xFFFFFFFF;
    refresh();
}

void ShowNotesPane::refresh()
{
    ShowTreeController *st = controller();
    // Take the old entries down (the empty label and the stretch stay).
    while (m_entries->count() > 2)
    {
        QLayoutItem *it = m_entries->takeAt(1);
        if (it->widget() != nullptr)
            it->widget()->deleteLater();
        delete it;
    }
    m_shown.clear();

    Note probe;
    probe.kind = m_kind;
    probe.target = m_target;
    probe.cue = m_layerCue;
    const bool have = st != nullptr && hasItem() && st->noteTargetExists(probe);
    m_input->setEnabled(have);
    m_add->setEnabled(have);
    m_snapshot->setEnabled(have);
    if (!have)
    {
        m_title->setText(tr("Notes"));
        m_empty->setText(tr("Select a cue, a layer, a list or a palette to see its notes and add one."));
        m_empty->show();
    }
    else
    {
        m_title->setText(tr("Notes — %1").arg(st->noteTargetLabel(probe)));
        const QList<Note> list = st->notes(m_kind, m_target);
        m_empty->setText(tr("No notes yet. Type one below: what you were thinking, what to fix."));
        m_empty->setVisible(list.isEmpty());
        int at = 1;
        for (const Note &n : list)
        {
            m_entries->insertWidget(at++, entryWidget(n));
            m_shown << n.id;
        }
    }

    // The pictures go into the show file: say so when they make it big.
    const qint64 bytes = st != nullptr ? st->noteImageBytes() : 0;
    if (bytes > ShowTreeController::noteImageWarnBytes())
    {
        m_sizeWarning->setText(tr("⚠ Snapshot pictures add %1 MB to the show file. Remove the snapshots you "
                                  "no longer need (⋯ on a note).")
                               .arg(QString::number(double(bytes) * 4.0 / 3.0 / (1024.0 * 1024.0), 'f', 1)));
        m_sizeWarning->show();
    }
    else
    {
        m_sizeWarning->hide();
    }
}

QWidget *ShowNotesPane::entryWidget(const Note &n)
{
    QFrame *box = new QFrame(m_entriesHost);
    box->setObjectName(QStringLiteral("stNoteEntry"));
    box->setProperty("noteId", n.id);
    box->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *v = new QVBoxLayout(box);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(2);

    QHBoxLayout *top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    QCheckBox *done = new QCheckBox(box);
    done->setObjectName(QStringLiteral("stNoteDone"));
    done->setChecked(n.done);
    done->setToolTip(n.done ? tr("Done. Untick to open it again.")
                            : tr("Open. Tick it when it's done (an open note on a cue is Needs work)."));
    const quint32 id = n.id;
    connect(done, &QCheckBox::toggled, this, [this, id](bool on) {
        if (ShowTreeController *st = controller())
            st->setNoteDone(id, on);
    });
    top->addWidget(done);
    QStringList meta { whenText(n.when) };
    if (!n.author.isEmpty())
        meta << n.author;
    QLabel *metaLabel = new QLabel(meta.join(QStringLiteral(" · ")), box);
    metaLabel->setObjectName(QStringLiteral("stNoteMeta"));
    metaLabel->setEnabled(false);
    top->addWidget(metaLabel, 1);
    QToolButton *more = new QToolButton(box);
    more->setObjectName(QStringLiteral("stNoteMore"));
    more->setText(QStringLiteral("⋯"));
    more->setAutoRaise(true);
    more->setToolTip(tr("Edit, snapshot, delete"));
    connect(more, &QToolButton::clicked, this, [this, id, more]() { entryMenu(id, more); });
    top->addWidget(more);
    v->addLayout(top);

    QLabel *text = new QLabel(n.text.isEmpty() ? tr("(snapshot)") : n.text, box);
    text->setObjectName(QStringLiteral("stNoteText"));
    text->setWordWrap(true);
    text->setTextFormat(Qt::PlainText);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (n.done)
    {
        QFont f = text->font();
        f.setStrikeOut(true);
        text->setFont(f);
        text->setEnabled(false);
    }
    v->addWidget(text);

    if (n.view.valid)
    {
        ShowTreeController *st = controller();
        QHBoxLayout *snap = new QHBoxLayout();
        snap->setContentsMargins(0, 2, 0, 0);
        QLabel *pic = new QLabel(box);
        pic->setObjectName(QStringLiteral("stNotePicture"));
        QPixmap pm;
        if (st != nullptr && !n.view.image.isEmpty())
            pm.loadFromData(st->noteImage(n.view.image));
        if (!pm.isNull())
        {
            pic->setPixmap(pm.scaledToWidth(128, Qt::SmoothTransformation));
            pic->setToolTip(tr("The rig view when the note was written"));
        }
        else
        {
            pic->setText(tr("(no picture)"));
            pic->setEnabled(false);
        }
        snap->addWidget(pic, 0, Qt::AlignTop);
        QVBoxLayout *side = new QVBoxLayout();
        side->setContentsMargins(0, 0, 0, 0);
        QLabel *stage = new QLabel(n.view.cueLabel.isEmpty() ? tr("Nothing on stage")
                                                             : tr("On stage: %1").arg(n.view.cueLabel), box);
        stage->setObjectName(QStringLiteral("stNoteOnStage"));
        stage->setWordWrap(true);
        stage->setEnabled(false);
        side->addWidget(stage);
        QPushButton *show = new QPushButton(tr("Show Me"), box);
        show->setObjectName(QStringLiteral("stNoteShowMe"));
        const bool operate = m_doc != nullptr && m_doc->mode() == Doc::Operate;
        show->setToolTip(operate ? tr("Puts the rig view back as it was (Operate: the camera only; nothing "
                                      "changes on stage)")
                                 : tr("Puts the rig view back as it was and selects the cue that was on "
                                      "stage, so it is on the rig again"));
        connect(show, &QPushButton::clicked, this, [this, id]() { showMe(id); });
        side->addWidget(show, 0, Qt::AlignLeft);
        side->addStretch(1);
        snap->addLayout(side, 1);
        v->addLayout(snap);
    }
    return box;
}

void ShowNotesPane::entryMenu(quint32 noteId, QToolButton *button)
{
    ShowTreeController *st = controller();
    const Note *n = st != nullptr ? st->note(noteId) : nullptr;
    if (n == nullptr)
        return;
    QMenu menu(this);
    QAction *edit = menu.addAction(tr("Edit…"));
    QAction *snap = menu.addAction(n->view.valid ? tr("Snapshot the View Again") : tr("Snapshot the View"));
    QAction *unsnap = menu.addAction(tr("Remove Snapshot"));
    unsnap->setEnabled(n->view.valid);
    menu.addSeparator();
    QAction *del = menu.addAction(tr("Delete Note"));
    QAction *picked = menu.exec(button->mapToGlobal(QPoint(0, button->height())));
    if (picked == edit)
    {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(this, tr("Edit note"), tr("Note"), n->text, &ok);
        if (ok)
            st->setNoteText(noteId, text);
    }
    else if (picked == snap)
    {
        const NoteView view = snapshotView();
        if (view.valid)
            st->setNoteView(noteId, view);
    }
    else if (picked == unsnap)
    {
        st->setNoteView(noteId, NoteView());
    }
    else if (picked == del)
    {
        st->removeNote(noteId);
    }
}

bool ShowNotesPane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && !(ke->modifiers() & (Qt::ShiftModifier | Qt::AltModifier)))
        {
            submit(false);
            return true;
        }
        if (ke->key() == Qt::Key_Escape)
        {
            m_input->clear();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ShowNotesPane::submit(bool snapshot)
{
    const QString text = m_input->toPlainText();
    if (text.trimmed().isEmpty() && !snapshot)
        return;
    if (addNote(text, snapshot) != 0xFFFFFFFF)
        m_input->clear();
}

quint32 ShowNotesPane::addNote(const QString &text, bool snapshot)
{
    ShowTreeController *st = controller();
    if (st == nullptr || !hasItem())
        return 0xFFFFFFFF;
    const NoteView view = snapshot ? snapshotView() : NoteView();
    return st->addNote(m_kind, m_target, text, view, m_layerCue);
}

NoteView ShowNotesPane::snapshotView()
{
    NoteView out;
    ShowTreeController *st = controller();
    StructureStudioView *view = rigView(true);
    if (view != nullptr)
    {
        const StructureStudioView::Camera c = view->camera();
        out.valid = true;
        out.plane = c.plane;
        out.rotation = c.rotation;
        out.azimuth = c.azimuth;
        out.elevation = c.elevation;
        out.zoomed = c.zoomed;
        out.scale = c.scale;
        out.originX = c.origin.x();
        out.originY = c.origin.y();
        out.width = c.width;
        out.height = c.height;

        // A small picture: about 480 px wide, JPEG (PNG when there is no
        // JPEG writer), so a note costs tens of KB in the show file.
        const QPixmap grab = view->grab();
        if (!grab.isNull() && st != nullptr)
        {
            const QPixmap small = grab.width() > imageWidth()
                                  ? grab.scaledToWidth(imageWidth(), Qt::SmoothTransformation) : grab;
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!small.save(&buffer, "JPG", 75))
            {
                bytes.clear();
                buffer.seek(0);
                small.save(&buffer, "PNG");
            }
            out.image = st->addNoteImage(bytes);
        }
    }
    if (st != nullptr)
    {
        // What is on stage: the cue on the rig in Design, else the cue a
        // list is playing for real.
        quint32 onStage = 0xFFFFFFFF;
        if (st->previewState() == ShowTreeController::PreviewState::Playing)
            onStage = st->previewedCue();
        if (onStage == 0xFFFFFFFF)
            for (const Production &p : st->productions())
                for (const CueList &l : p.lists)
                    if (onStage == 0xFFFFFFFF && !l.looks)
                        onStage = st->runningCue(l.id);
        if (onStage != 0xFFFFFFFF)
            if (const CueList *l = st->listOfCue(onStage))
            {
                out.valid = true;
                out.cue = onStage;
                out.cueLabel = st->cueLabel(*l, l->indexOf(onStage));
            }
    }
    return out;
}

bool ShowNotesPane::showMe(quint32 noteId)
{
    ShowTreeController *st = controller();
    const Note *n = st != nullptr ? st->note(noteId) : nullptr;
    if (n == nullptr || !n->view.valid)
        return false;
    const NoteView v = n->view;
    if (v.width > 0 || v.height > 0 || v.zoomed)
        if (StructureStudioView *view = rigView(true))
        {
            StructureStudioView::Camera c;
            c.plane = v.plane;
            c.rotation = v.rotation;
            c.azimuth = v.azimuth;
            c.elevation = v.elevation;
            c.zoomed = v.zoomed;
            c.scale = v.scale;
            c.origin = QPointF(v.originX, v.originY);
            c.width = v.width;
            c.height = v.height;
            view->setCamera(c);
        }
    // Operate: the stage is the show's; only the camera moves.
    const bool operate = m_doc != nullptr && m_doc->mode() == Doc::Operate;
    if (!operate && v.cue != 0xFFFFFFFF && st->cue(v.cue) != nullptr && selectCue)
        selectCue(v.cue);
    return true;
}
