/*
  Q Light Controller Plus
  showtreebackwash.cpp

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

/* Backwash in the Show Tree (Show Review PWR-12): each cue that puts light
   on the cyc (or a scrim, or any scenery kept for light on purpose) that
   nobody meant is marked where you work -- "⚠ cyc" in its flags (amber), or
   "cyc ✓" (green) once it is marked meant, nothing once it is a Not a
   Problem (it is listed in Show Review under Accepted) -- the detail on hover (ShowReviewMarker::tooltip()) -- and a click on the mark
   opens the Show Review row; a right-click changes its state there (I Meant It,
   Not a Problem…, Not Meant, Show Me the Layer, Open in Show Review). A layer
   marked meant offers to take it back from its right-click.

   The marks are read from Show Review's last result: it runs a moment after
   the show changes, in Design (App's review timer), never on the GO path. */

#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QPainter>
#include <QAction>
#include <QMenu>
#include <QBrush>
#include <QHash>

#include "showtreeview.h"
#include "showreviewpanel.h"
#include "showreviewmarker.h"
#include "contextmenus.h"
#include "showreview.h"
#include "monitorproperties.h"
#include "scenery.h"
#include "fixture.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
/* A cue row's own flags text and tooltip, before the mark: buildTree() makes
   new rows, so a row without these has not been marked yet. */
const int BaseFlagsRole = Qt::UserRole + 40;
const int BaseTipRole = Qt::UserRole + 41;
const int BackwashKeyRole = Qt::UserRole + 42;

/* The flags column, badge by badge: a warning's badge in the warning colour,
   the others (Mix, Track, skip...) in the row's own. One colour for the
   whole cell painted "Mix" orange beside "⚠ cyc". */
class FlagsDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QStringList warn = index.data(ShowTreeView::WarnPiecesRole).toStringList()
                                 + index.data(ShowTreeView::MeantPiecesRole).toStringList();
        if (warn.isEmpty() && index.data(ShowTreeView::NoteMarkRole).toString().isEmpty())
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString text = opt.text;
        opt.text.clear();
        QStyle *style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        const QRect r = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget).adjusted(2, 0, -2, 0);
        const QColor normal = (opt.state & QStyle::State_Selected)
                              ? opt.palette.color(QPalette::HighlightedText)
                              : opt.palette.color(QPalette::Text);
        const QColor fg = index.data(Qt::ForegroundRole).canConvert<QBrush>()
                          && !(opt.state & QStyle::State_Selected)
                          ? index.data(Qt::ForegroundRole).value<QBrush>().color() : normal;
        painter->save();
        painter->setFont(opt.font);
        const QFontMetrics fm(opt.font);
        int x = r.left();
        const QList<QPair<QString, QColor>> pieces = ShowTreeView::flagPieces(index, fg, opt.palette.color(QPalette::Base));
        for (int i = 0; i < pieces.count(); ++i)
        {
            const QString piece = pieces.at(i).first + (i + 1 < pieces.count() ? QStringLiteral(", ") : QString());
            const int w = fm.horizontalAdvance(piece);
            const QString shown = (x + w > r.right()) ? fm.elidedText(piece, Qt::ElideRight, r.right() - x) : piece;
            painter->setPen(pieces.at(i).second);
            painter->drawText(QRect(x, r.top(), r.right() - x, r.height()),
                              Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, shown);
            x += w;
            if (x >= r.right())
                break;
        }
        painter->restore();
        Q_UNUSED(text)
    }
};
}

QColor ShowTreeView::warningColour()
{
    return QColor(0xd0, 0x8a, 0x1c);
}

QColor ShowTreeView::meantColour(const QColor &background)
{
    return background.lightness() < 128 ? QColor(0x5c, 0xb8, 0x5c) : QColor(0x2e, 0x7d, 0x32);
}

QList<QPair<QString, QColor>> ShowTreeView::flagPieces(const QModelIndex &index, const QColor &normal, const QColor &background)
{
    QList<QPair<QString, QColor>> out;
    const QStringList warn = index.data(WarnPiecesRole).toStringList();
    const QStringList meant = index.data(MeantPiecesRole).toStringList();
    for (const QString &piece : index.data(Qt::DisplayRole).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts))
        out << qMakePair(piece, warn.contains(piece) ? warningColour()
                                : meant.contains(piece) ? meantColour(background.isValid() ? background : normal) : normal);
    // Q212: the notes mark, first (it is short, and stays in view when the
    // column is narrow) -- open notes in the warning colour.
    const QString mark = index.data(NoteMarkRole).toString();
    if (!mark.isEmpty())
        out.prepend(qMakePair(mark, index.data(NoteOpenRole).toBool() ? warningColour() : normal));
    return out;
}

void ShowTreeView::setupBackwashMarks()
{
    if (m_tree != nullptr)
        m_tree->setItemDelegateForColumn(FlagsCol, new FlagsDelegate(m_tree));
    if (m_doc == nullptr || m_doc->showReview() == nullptr)
        return;
    connect(m_doc->showReview(), &ShowReview::reviewed, this, &ShowTreeView::applyBackwashMarks);
    // Not a Problem / Reopen: the badge follows at once, not at the next run.
    connect(m_doc->showReview(), &ShowReview::acceptancesChanged, this, &ShowTreeView::applyBackwashMarks);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (item == nullptr || column != FlagsCol || item->data(0, KindRole).toInt() != CueNode)
            return;
        if (!item->data(FlagsCol, BackwashKeyRole).toString().isEmpty())
            openBackwashFinding(item->data(0, IdRole).toUInt());
    });
}

void ShowTreeView::applyBackwashMarks()
{
    if (m_doc == nullptr || m_doc->showReview() == nullptr || m_tree == nullptr)
        return;
    // Cue -> the PWR-12 findings breaching in it, in Show Review's order:
    // open ones (the amber mark) and ones marked meant (the green one). One
    // not a problem is neither: no badge.
    QHash<quint32, QList<int>> byCue, meantByCue;
    const QList<ReviewFinding> &found = m_doc->showReview()->lastResult().findings;
    for (int i = 0; i < found.count(); ++i)
    {
        const ReviewFinding &f = found.at(i);
        if (f.check != QLatin1String("PWR-12") || (!f.isOpen() && !f.intended))
            continue;
        for (quint32 cue : ShowReview::findingCues(f))
            (f.intended ? meantByCue : byCue)[cue] << i;
    }
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->data(0, KindRole).toInt() != CueNode)
            continue;
        if (!item->data(FlagsCol, BaseFlagsRole).isValid())
        {
            item->setData(FlagsCol, BaseFlagsRole, item->text(FlagsCol));
            item->setData(FlagsCol, BaseTipRole, item->toolTip(FlagsCol));
        }
        const QString mark = tr("⚠ cyc");
        const QString meantMark = tr("cyc ✓");
        QStringList warn = item->data(FlagsCol, WarnPiecesRole).toStringList();
        QStringList meantPieces = item->data(FlagsCol, MeantPiecesRole).toStringList();
        const QString base = item->data(FlagsCol, BaseFlagsRole).toString();
        const QString baseTip = item->data(FlagsCol, BaseTipRole).toString();
        const quint32 cueId = item->data(0, IdRole).toUInt();
        const QList<int> hits = byCue.value(cueId), meantHits = meantByCue.value(cueId);
        warn.removeAll(mark);
        meantPieces.removeAll(meantMark);
        if (hits.isEmpty() && meantHits.isEmpty())
        {
            if (!item->data(FlagsCol, BackwashKeyRole).toString().isEmpty())
            {
                item->setText(FlagsCol, base);
                item->setToolTip(FlagsCol, baseTip);
                item->setData(FlagsCol, BackwashKeyRole, QString());
                item->setData(FlagsCol, WarnPiecesRole, warn);
                item->setData(FlagsCol, MeantPiecesRole, meantPieces);
            }
            continue;
        }
        // The detail -- which fixtures, how much, on what, who meant it, the
        // cues, the answers -- from the one builder every mark shares.
        QList<ReviewFinding> marked;
        for (int i : hits)
            marked << found.at(i);
        for (int i : meantHits)
            marked << found.at(i);
        QStringList text;
        if (!base.isEmpty())
            text << base;
        if (!hits.isEmpty())
        {
            text << mark;
            warn << mark;       // the mark alone, not Mix beside it
        }
        if (!meantHits.isEmpty())
        {
            text << meantMark;
            meantPieces << meantMark;
        }
        item->setText(FlagsCol, text.join(QStringLiteral(", ")));
        item->setData(FlagsCol, WarnPiecesRole, warn);
        item->setData(FlagsCol, MeantPiecesRole, meantPieces);
        QString tip = ShowReviewMarker::tooltip(m_doc, marked);
        if (!baseTip.isEmpty())
            tip.replace(QStringLiteral("<qt>"), QStringLiteral("<qt>") + baseTip.toHtmlEscaped()
                                                .replace(QLatin1Char('\n'), QStringLiteral("<br>"))
                                                + QStringLiteral("<br><br>"));
        item->setToolTip(FlagsCol, tip);
        item->setData(FlagsCol, BackwashKeyRole, found.at(hits.isEmpty() ? meantHits.first() : hits.first()).key());
    }
}

QString ShowTreeView::backwashFindingKey(quint32 cueId) const
{
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == CueNode && (*it)->data(0, IdRole).toUInt() == cueId)
            return (*it)->data(FlagsCol, BackwashKeyRole).toString();
    return QString();
}

bool ShowTreeView::openBackwashFinding(quint32 cueId)
{
    const QString key = backwashFindingKey(cueId);
    if (key.isEmpty())
        return false;
    ShowReviewPanel *panel = ShowReviewPanel::openShared();
    if (panel == nullptr)
        return false;
    // The open filter hides a meant row; a cue with only the green mark opens on All.
    panel->setFilter(ShowReviewMarker::cueFindings(m_doc, cueId).isEmpty() ? ShowReviewPanel::All
                                                                           : ShowReviewPanel::OpenOnly);
    panel->setSearch(QString());
    return panel->selectFinding(key);
}

bool ShowTreeView::backwashMarkMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item == nullptr || m_tree->columnAt(pos.x()) != FlagsCol
        || item->data(0, KindRole).toInt() != CueNode
        || item->data(FlagsCol, BackwashKeyRole).toString().isEmpty())
        return false;
    const QList<ReviewFinding> marked = ShowReviewMarker::cueAll(m_doc, item->data(0, IdRole).toUInt());
    if (marked.isEmpty())
        return false;
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("stWarningMenu"));
    menu.setToolTipsVisible(true);
    {
        ContextMenu cm(&menu);
        ShowReviewMarker::fillMenu(cm, m_doc, marked, this);
    }
    m_lastWarningMenu = &menu;
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
    m_lastWarningMenu = nullptr;
    return true;
}

void ShowTreeView::backwashLayerMenu(QMenu *menu, quint32 cueId, quint32 layerId)
{
    const Cue *cue = m_st->cue(cueId);
    const Layer *layer = cue != nullptr ? cue->layer(layerId) : nullptr;
    if (layer == nullptr || layer->meantOnScenery.isEmpty())
        return;
    QAction *a = menu->addAction(tr("Light on the Cyc: Not Meant"), this, [this, cueId, layerId]() {
        if (ShowReview::clearLayerMeant(m_doc, cueId, layerId))
            m_doc->showReview()->run();
    });
    a->setObjectName(QStringLiteral("stBackwashNotMeant"));
    a->setToolTip(tr("Take back I Meant It for this layer: Show Review warns again where its light "
                     "lands on the cyc"));
}
