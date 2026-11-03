/*
  Q Light Controller Plus
  showtreenotemarks.cpp

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

/* Notes beside the Show Tree (Q212): the pane under the page that follows
   the selection, the mark on rows with notes (open ones in the warning
   colour) with the newest note on hover, and All notes…. The notes
   themselves are the controller's (engine/src/showtreenotes.cpp). */

#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QSplitter>

#include "showtreeview.h"
#include "shownotespane.h"
#include "shownoteslist.h"
#include "showtreecontroller.h"
#include "showreview.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
/* The note line a row's tooltips end with, so the next pass takes it off
   before it adds the new one (the rest of the tooltip is buildTree()'s, or
   the backwash mark's). */
const int NoteTipRole = Qt::UserRole + 52;

/* What a tree row takes notes as: false for a row that takes none (a
   section, a folder, a carried layer). */
bool rowNoteItem(const QTreeWidgetItem *item, NoteKind *kind, quint32 *target, quint32 *cue)
{
    *cue = 0xFFFFFFFF;
    *target = item->data(0, ShowTreeView::IdRole).toUInt();
    switch (item->data(0, ShowTreeView::KindRole).toInt())
    {
        case ShowTreeView::ProductionNode: *kind = NoteKind::Production; return true;
        case ShowTreeView::ListNode: *kind = NoteKind::List; return true;
        case ShowTreeView::CueNode: *kind = NoteKind::Cue; return true;
        case ShowTreeView::LayerNode:
            *kind = NoteKind::Layer;
            *cue = item->data(0, ShowTreeView::CueRole).toUInt();
            return true;
        default:
        break;
    }
    return false;
}

QString htmlSuffix(const QString &suffix)
{
    return QStringLiteral("<br><br>") + suffix.trimmed().toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
}

void setTipSuffix(QTreeWidgetItem *item, int column, const QString &oldSuffix, const QString &suffix)
{
    QString tip = item->toolTip(column);
    // A rich tooltip (the backwash mark's) takes the line as HTML.
    const bool rich = tip.startsWith(QLatin1String("<qt>"));
    if (!oldSuffix.isEmpty())
    {
        if (rich)
            tip.replace(htmlSuffix(oldSuffix) + QStringLiteral("</qt>"), QStringLiteral("</qt>"));
        else if (tip.endsWith(oldSuffix))
            tip.chop(oldSuffix.length());
    }
    if (!suffix.isEmpty())
    {
        if (rich)
            tip.replace(QStringLiteral("</qt>"), htmlSuffix(suffix) + QStringLiteral("</qt>"));
        else
            tip += suffix;
    }
    item->setToolTip(column, tip);
}
}

void ShowTreeView::setupNotes(QSplitter *pageSplit)
{
    m_notesPane = new ShowNotesPane(m_doc, pageSplit);
    pageSplit->addWidget(m_notesPane);
    pageSplit->setStretchFactor(pageSplit->indexOf(m_notesPane), 2);
    m_notesPane->selectCue = [this](quint32 cueId) { selectNode(cueId); };
    connect(m_notesPane, &ShowNotesPane::allNotesRequested, this, [this]() { showAllNotes(); });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() { syncNotesPane(false); });
    connect(m_library, &QTreeWidget::itemSelectionChanged, this, [this]() { syncNotesPane(true); });
    if (m_st != nullptr)
        connect(m_st, &ShowTreeController::notesChanged, this, &ShowTreeView::applyNoteMarks);
    // After the backwash mark's pass (connected before), which rewrites
    // the flags tooltip of the cues it marks.
    if (m_doc->showReview() != nullptr)
    {
        connect(m_doc->showReview(), &ShowReview::reviewed, this, &ShowTreeView::applyNoteMarks);
        connect(m_doc->showReview(), &ShowReview::acceptancesChanged, this, &ShowTreeView::applyNoteMarks);
    }
}

void ShowTreeView::syncNotesPane(bool fromLibrary)
{
    if (m_notesPane == nullptr)
        return;
    if (fromLibrary)
    {
        // A palette picked in the Library: its notes. Anything else there
        // leaves the pane on the tree's row.
        QTreeWidgetItem *item = m_library->currentItem();
        if (item != nullptr && item->isSelected() && item->data(0, KindRole).toInt() == LibPalette)
            m_notesPane->setItem(NoteKind::Palette, item->data(0, IdRole).toUInt());
        return;
    }
    QTreeWidgetItem *item = m_tree->currentItem();
    NoteKind kind;
    quint32 target, cue;
    if (item != nullptr && rowNoteItem(item, &kind, &target, &cue))
        m_notesPane->setItem(kind, target, cue);
    else if (!m_reloading)
        m_notesPane->clearItem();
}

void ShowTreeView::applyNoteMarks()
{
    if (m_tree == nullptr || m_st == nullptr)
        return;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        NoteKind kind;
        quint32 target, cue;
        QString mark, suffix;
        bool open = false;
        if (rowNoteItem(item, &kind, &target, &cue))
        {
            int opened = 0;
            const int all = m_st->noteCount(kind, target, &opened);
            if (all > 0)
            {
                open = opened > 0;
                mark = open ? (opened > 1 ? QStringLiteral("✎ %1").arg(opened) : QStringLiteral("✎"))
                            : QStringLiteral("✎");
                if (const Note *n = m_st->latestNote(kind, target))
                {
                    QStringList meta { ShowNotesPane::whenText(n->when) };
                    if (!n->author.isEmpty())
                        meta << n->author;
                    meta << (n->done ? tr("done") : tr("open"));
                    QString text = n->text.isEmpty() ? tr("(snapshot of the view)") : n->text;
                    if (text.length() > 280)
                        text = text.left(279) + QStringLiteral("…");
                    suffix = QStringLiteral("\n\n✎ %1: %2").arg(meta.join(QStringLiteral(" · ")), text);
                    if (all > 1)
                        suffix += QLatin1Char('\n') + (all == 2 ? tr("1 more note in the notes pane")
                                                       : tr("%1 more notes in the notes pane").arg(all - 1));
                }
            }
        }
        const QString oldSuffix = item->data(0, NoteTipRole).toString();
        if (oldSuffix.isEmpty() && suffix.isEmpty() && mark.isEmpty()
            && item->data(FlagsCol, NoteMarkRole).toString().isEmpty())
            continue;
        item->setData(FlagsCol, NoteMarkRole, mark);
        item->setData(FlagsCol, NoteOpenRole, open);
        setTipSuffix(item, 0, oldSuffix, suffix);
        setTipSuffix(item, FlagsCol, oldSuffix, suffix);
        item->setData(0, NoteTipRole, suffix);
    }
}

ShowNotesList *ShowTreeView::showAllNotes()
{
    if (m_notesList == nullptr)
    {
        m_notesList = new ShowNotesList(m_doc, this);
        m_notesList->setWindowFlag(Qt::Window, true);
        m_notesList->jump = [this](const Note &n) {
            switch (n.kind)
            {
                case NoteKind::Layer: selectNode(n.target, n.cue); break;
                case NoteKind::Palette: editPalette(n.target); m_notesPane->setItem(NoteKind::Palette, n.target); break;
                default: selectNode(n.target); break;
            }
        };
        connect(m_notesList, &QObject::destroyed, this, [this]() { m_notesList = nullptr; });
    }
    m_notesList->refresh();
    m_notesList->show();
    m_notesList->raise();
    return m_notesList;
}
