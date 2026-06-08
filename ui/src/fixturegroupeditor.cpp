/*
  Q Light Controller
  fixturegroupeditor.cpp

  Copyright (c) Heikki Junnila

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

#include <QTableWidgetItem>
#include <QTableWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QMenu>
#include <QHeaderView>
#include <QSettings>
#include <QLineEdit>
#include <QSpinBox>
#include <QDebug>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QApplication>
#include <QMouseEvent>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QRubberBand>
#include <QShortcut>
#include <QKeySequence>
#include <QDrag>
#include <QTimer>
#include <climits>

#include "fixturegroupeditor.h"
#include "fixturetreewidget.h"
#include "fixtureselection.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

#define SETTINGS_GEOMETRY "fixturegroupeditor/geometry"

#define PROP_FIXTURE Qt::UserRole
#define PROP_HEAD Qt::UserRole + 1

// Mime type marking an internal drag of selected grid cells (payload is
// carried in member state, not the mime data itself).
static const char *CELLS_DRAG_MIME = "application/x-qlc-groupcells";

FixtureGroupEditor::FixtureGroupEditor(FixtureGroup* grp, Doc* doc, QWidget* parent)
    : QWidget(parent)
    , m_grp(grp)
    , m_doc(doc)
{
    Q_ASSERT(grp != NULL);
    Q_ASSERT(doc != NULL);

    setupUi(this);

    m_nameEdit->setText(m_grp->name());
    m_xSpin->setValue(m_grp->size().width());
    m_ySpin->setValue(m_grp->size().height());

    connect(m_nameEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(slotNameEdited(const QString&)));
    connect(m_xSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotXSpinValueChanged(int)));
    connect(m_ySpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotYSpinValueChanged(int)));

    connect(m_rightButton, SIGNAL(clicked()),
            this, SLOT(slotRightClicked()));
    connect(m_leftButton, SIGNAL(clicked()),
            this, SLOT(slotLeftClicked()));
    connect(m_downButton, SIGNAL(clicked()),
            this, SLOT(slotDownClicked()));
    connect(m_upButton, SIGNAL(clicked()),
            this, SLOT(slotUpClicked())),
    connect(m_removeButton, SIGNAL(clicked()),
            this, SLOT(slotRemoveFixtureClicked()));

    // Add a whole fixture group as a block (preserving its layout).
    QPushButton *addGroupBtn = new QPushButton(tr("Add group as block…"), this);
    horizontalLayout->addWidget(addGroupBtn);
    connect(addGroupBtn, SIGNAL(clicked()), this, SLOT(slotAddGroupBlock()));

    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setIconSize(QSize(20, 20));

    // Accept fixtures dragged from the fixture tree, dropped onto a cell.
    m_table->setAcceptDrops(true);
    m_table->viewport()->setAcceptDrops(true);
    m_table->viewport()->installEventFilter(this);

    // Right-click a sub-group cell to move the whole sub-group as a unit.
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotTableContextMenu(QPoint)));

    // Coalesce the burst of sectionResized signals (Stretch mode emits one per
    // column) into a single font relayout, so rendering stays cheap.
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(0);
    connect(m_resizeTimer, SIGNAL(timeout()), this, SLOT(relayoutCellFonts()));

    // Wireframe shown over the destination while dragging a block.
    m_dropBand = new QRubberBand(QRubberBand::Rectangle, m_table->viewport());

    // Ctrl+Z undoes the last layout edit.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, SIGNAL(activated()), this, SLOT(slotUndo()));

    updateTable();
}

bool FixtureGroupEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_table->viewport())
    {
        const char *mimeType = FixtureTreeWidget::fixtureDragMimeType();

        // --- Selection & internal drag -------------------------------------
        // We drive selection ourselves (index-precise) so it never over-grabs:
        //  - Shift+click/drag: rectangle from the stored anchor to the cursor.
        //  - Ctrl+click:       toggle a single cell.
        //  - plain press on an occupied cell: arm a block move.
        //  - plain press on an empty cell:    start a marquee selection.
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            m_dragMode = NoDrag;
            if (me->button() == Qt::LeftButton)
            {
                const QLCPoint cell = cellAt(me->pos());
                const QModelIndex idx = m_table->model()->index(cell.y(), cell.x());
                const Qt::KeyboardModifiers mods = me->modifiers();
                m_dragStartPos = me->pos();

                if (mods & Qt::ShiftModifier)
                {
                    // Extend a rectangle from the existing anchor; let a drag
                    // keep extending it.
                    selectCellRange(m_marqueeAnchor, cell);
                    m_dragMode = Marquee;
                    return true;
                }
                if (mods & Qt::ControlModifier)
                {
                    m_table->selectionModel()->select(idx, QItemSelectionModel::Toggle);
                    m_table->setCurrentCell(cell.y(), cell.x(), QItemSelectionModel::NoUpdate);
                    slotCellActivated(cell.y(), cell.x());
                    m_marqueeAnchor = cell;
                    return true;
                }
                if (mods == Qt::NoModifier)
                {
                    m_marqueeAnchor = cell; // anchor for a later Shift+click
                    const bool occupied = m_table->item(cell.y(), cell.x()) != NULL;
                    if (occupied)
                    {
                        // Keep an existing multi-selection so it can be dragged;
                        // otherwise select just this cell.
                        if (m_table->selectionModel()->isSelected(idx) == false)
                        {
                            m_table->clearSelection();
                            m_table->setCurrentCell(cell.y(), cell.x(),
                                                    QItemSelectionModel::ClearAndSelect);
                            slotCellActivated(cell.y(), cell.x());
                        }
                        m_dragMode = MoveBlock;
                    }
                    else
                    {
                        selectCellRange(cell, cell);
                        m_dragMode = Marquee;
                    }
                    return true;
                }
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) == 0)
                return false;
            if (m_dragMode == Marquee)
            {
                selectCellRange(m_marqueeAnchor, cellAt(me->pos()));
                return true;
            }
            if (m_dragMode == MoveBlock)
            {
                if ((me->pos() - m_dragStartPos).manhattanLength()
                        >= QApplication::startDragDistance())
                {
                    m_dragMode = NoDrag;
                    startCellDrag();
                }
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            if (m_dragMode != NoDrag)
            {
                m_dragMode = NoDrag;
                return true;
            }
        }

        if (event->type() == QEvent::DragEnter)
        {
            QDragEnterEvent *de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasFormat(mimeType) ||
                de->mimeData()->hasFormat(CELLS_DRAG_MIME)) { de->acceptProposedAction(); return true; }
        }
        else if (event->type() == QEvent::DragMove)
        {
            QDragMoveEvent *dm = static_cast<QDragMoveEvent*>(event);
            // Internal block move: show the wireframe and only accept the drop
            // where the block would actually fit.
            if (dm->mimeData()->hasFormat(CELLS_DRAG_MIME))
            {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                const QLCPoint c = cellAt(dm->pos());
#else
                const QLCPoint c = cellAt(dm->position().toPoint());
#endif
                const int dx = c.x() - m_dragAnchor.x();
                const int dy = c.y() - m_dragAnchor.y();
                showDropPreview(dx, dy);
                if (canMoveHeads(m_dragCells, dx, dy))
                    dm->acceptProposedAction();
                else
                    dm->ignore();
                return true;
            }
            if (dm->mimeData()->hasFormat(mimeType)) { dm->acceptProposedAction(); return true; }
        }
        else if (event->type() == QEvent::DragLeave)
        {
            hideDropPreview();
        }
        else if (event->type() == QEvent::Drop)
        {
            QDropEvent *dr = static_cast<QDropEvent*>(event);

            // Internal move of selected cells.
            if (dr->mimeData()->hasFormat(CELLS_DRAG_MIME))
            {
                hideDropPreview();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                const QLCPoint c = cellAt(dr->pos());
#else
                const QLCPoint c = cellAt(dr->position().toPoint());
#endif
                const int dx = c.x() - m_dragAnchor.x();
                const int dy = c.y() - m_dragAnchor.y();
                if (dx != 0 || dy != 0)
                {
                    // Defer the table rebuild until the drop fully returns
                    // (consistent with the other drop handlers; avoids
                    // rebuilding the widget from inside its own drop).
                    const QList<QLCPoint> cells = m_dragCells;
                    QTimer::singleShot(0, this, [this, cells, dx, dy]() {
                        moveHeads(cells, dx, dy);
                    });
                }
                dr->acceptProposedAction();
                return true;
            }

            if (dr->mimeData()->hasFormat(mimeType) == false)
                return false;

            // Cell under the cursor (fall back to the current cell).
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QModelIndex idx = m_table->indexAt(dr->pos());
#else
            QModelIndex idx = m_table->indexAt(dr->position().toPoint());
#endif
            int col = idx.isValid() ? idx.column() : m_column;
            int row = idx.isValid() ? idx.row() : m_row;

            QByteArray data = dr->mimeData()->data(mimeType);
            QDataStream stream(&data, QIODevice::ReadOnly);
            int maxX = m_grp->size().width() - 1;
            int maxY = m_grp->size().height() - 1;
            bool added = false;
            beginEdit();
            while (stream.atEnd() == false)
            {
                quint32 fid = 0;
                stream >> fid;
                if (m_doc->fixture(fid) == NULL)
                    continue;
                // assignFixture places all of the fixture's heads from this
                // cell; spread successive fixtures across the row.
                if (m_grp->assignFixture(fid, QLCPoint(col, row)))
                {
                    added = true;
                    maxX = qMax(maxX, col);
                    maxY = qMax(maxY, row);
                    col++;
                }
            }

            if (added)
            {
                if (maxX + 1 > m_grp->size().width() || maxY + 1 > m_grp->size().height())
                {
                    m_grp->setSize(QSize(qMax(maxX + 1, m_grp->size().width()),
                                         qMax(maxY + 1, m_grp->size().height())));
                    m_xSpin->blockSignals(true); m_xSpin->setValue(m_grp->size().width());  m_xSpin->blockSignals(false);
                    m_ySpin->blockSignals(true); m_ySpin->setValue(m_grp->size().height()); m_ySpin->blockSignals(false);
                }
                endEdit();
            }
            else
            {
                cancelEdit();
            }
            dr->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FixtureGroupEditor::startCellDrag()
{
    // Capture the occupied selected cells; the anchor is the cell the drag
    // started on (so the drop point yields the move delta).
    m_dragCells.clear();
    foreach (QTableWidgetItem *item, m_table->selectedItems())
    {
        if (item != NULL)
            m_dragCells << QLCPoint(item->column(), item->row());
    }
    if (m_dragCells.isEmpty())
        return;

    const QModelIndex anchorIdx = m_table->indexAt(m_dragStartPos);
    m_dragAnchor = anchorIdx.isValid()
            ? QLCPoint(anchorIdx.column(), anchorIdx.row())
            : m_dragCells.first();

    QDrag *drag = new QDrag(m_table);
    QMimeData *mime = new QMimeData;
    mime->setData(CELLS_DRAG_MIME, QByteArray("1")); // payload kept in members
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    hideDropPreview(); // in case the drag ended outside the viewport
}

FixtureGroupEditor::~FixtureGroupEditor()
{
}

void FixtureGroupEditor::updateTable()
{
    // Store these since they might get reset
    int savedRow = m_row;
    int savedCol = m_column;

    // Freeze repaints/relayouts while we tear down and rebuild every cell.
    m_table->setUpdatesEnabled(false);

    disconnect(m_table, SIGNAL(cellChanged(int,int)),
               this, SLOT(slotCellChanged(int,int)));
    disconnect(m_table, SIGNAL(cellPressed(int,int)),
               this, SLOT(slotCellActivated(int,int)));
    disconnect(m_table->horizontalHeader(), SIGNAL(sectionResized(int,int,int)),
            this, SLOT(slotResized()));

    m_table->clear();

    m_table->setRowCount(m_grp->size().height());
    m_table->setColumnCount(m_grp->size().width());

    QMapIterator <QLCPoint,GroupHead> it(m_grp->headsMap());
    while (it.hasNext() == true)
    {
        it.next();

        QLCPoint pt(it.key());

        GroupHead head(it.value());
        Fixture* fxi = m_doc->fixture(head.fxi);
        if (fxi == NULL)
            continue;

        QIcon icon = fxi->getIconFromType();
        QString str = QString("%1 H:%2\nA:%3 U:%4").arg(fxi->name())
                                               .arg(head.head + 1)
                                               .arg(fxi->address() + 1)
                                               .arg(fxi->universe() + 1);

        QTableWidgetItem* item = new QTableWidgetItem(icon, str);
        item->setData(PROP_FIXTURE, head.fxi);
        item->setData(PROP_HEAD, head.head);
        item->setToolTip(str);

        // Colour cells by sub-group so blocks pasted from another group are
        // visually distinct (stable hue per sub-group id).
        const quint32 sg = m_grp->headSubGroup(pt);
        if (sg != 0)
            item->setBackground(QBrush(QColor::fromHsv(int((sg * 53) % 360), 90, 235)));

        m_table->setItem(pt.y(), pt.x(), item);
    }

    connect(m_table, SIGNAL(cellPressed(int,int)),
            this, SLOT(slotCellActivated(int,int)));
    connect(m_table, SIGNAL(cellChanged(int,int)),
            this, SLOT(slotCellChanged(int,int)));
    connect(m_table->horizontalHeader(), SIGNAL(sectionResized(int,int,int)),
            this, SLOT(slotResized()));

    if (savedRow < m_table->rowCount() && savedCol < m_table->columnCount())
    {
        m_row = savedRow;
        m_column = savedCol;
    }
    else
    {
        m_row = 0;
        m_column = 0;
    }

    m_table->setCurrentCell(m_row, m_column);

    m_table->setUpdatesEnabled(true);
    relayoutCellFonts();
}

void FixtureGroupEditor::slotNameEdited(const QString& text)
{
    m_grp->setName(text);
}

void FixtureGroupEditor::slotXSpinValueChanged(int value)
{
    m_grp->setSize(QSize(value, m_grp->size().height()));
    updateTable();
}

void FixtureGroupEditor::slotYSpinValueChanged(int value)
{
    m_grp->setSize(QSize(m_grp->size().width(), value));
    updateTable();
}

void FixtureGroupEditor::slotRightClicked()
{
    addFixtureHeads(Qt::RightArrow);
}

void FixtureGroupEditor::slotLeftClicked()
{
    addFixtureHeads(Qt::LeftArrow);
}

void FixtureGroupEditor::slotDownClicked()
{
    addFixtureHeads(Qt::DownArrow);
}

void FixtureGroupEditor::slotUpClicked()
{
    addFixtureHeads(Qt::UpArrow);
}


void FixtureGroupEditor::slotRemoveFixtureClicked()
{
    // Remove every selected occupied cell (falls back to the current cell).
    QList<QLCPoint> pts;
    foreach (QTableWidgetItem *item, m_table->selectedItems())
        if (item != NULL)
            pts << QLCPoint(item->column(), item->row());
    if (pts.isEmpty() && m_table->currentItem() != NULL)
        pts << QLCPoint(m_column, m_row);
    if (pts.isEmpty())
        return;

    beginEdit();
    bool removed = false;
    foreach (const QLCPoint &p, pts)
        if (m_grp->resignHead(p))
            removed = true;
    if (removed)
        endEdit();
    else
        cancelEdit();
}

void FixtureGroupEditor::slotCellActivated(int row, int column)
{
    m_row = row;
    m_column = column;

    if (m_table->currentItem() == NULL)
        m_removeButton->setEnabled(false);
    else
        m_removeButton->setEnabled(true);
}

void FixtureGroupEditor::slotCellChanged(int row, int column)
{
    if (row < 0 || column < 0)
    {
        updateTable();
        return;
    }

    QMap <QLCPoint,GroupHead> hash = m_grp->headsMap();
    QLCPoint from(m_column, m_row);
    QLCPoint to(column, row);
    GroupHead fromHead;
    GroupHead toHead;

    if (hash.contains(from) == true)
        fromHead = hash[from];
    if (hash.contains(to) == true)
        toHead = hash[to];

    m_grp->swap(from, to);

    updateTable();
    m_table->setCurrentCell(row, column);
    slotCellActivated(row, column);
}

void FixtureGroupEditor::slotResized()
{
    // Stretch mode fires sectionResized once per column; coalesce the burst
    // into a single relayout on the next event-loop pass.
    m_resizeTimer->start();
}

void FixtureGroupEditor::relayoutCellFonts()
{
    // Fitting each cell's font is O(cells); for big groups it dominates render
    // time (and the result is illegibly small anyway). Skip it past a cap and
    // let the cells use the default font.
    static const int kFontFitCellCap = 600;
    if (m_table->rowCount() * m_table->columnCount() > kFontFitCellCap)
        return;

    disconnect(m_table, SIGNAL(cellChanged(int,int)),
               this, SLOT(slotCellChanged(int,int)));

    float cellWidth = (float)(m_table->columnWidth(0) - m_table->iconSize().width());
    QFont font = m_table->font();
    QFontMetrics fm(font);
    float pSizeF = font.pointSizeF();

    for (int y = 0; y < m_table->rowCount(); y++)
    {
        for (int x = 0; x < m_table->columnCount(); x++)
        {
            QTableWidgetItem* item = m_table->item(y, x);
            if (item != NULL)
            {
                QFont scaledFont = font;
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
                float baseWidth  = (float)fm.width(item->text());
#else
                float baseWidth  = (float)fm.horizontalAdvance(item->text());
#endif
                float factor = cellWidth / baseWidth;
                if (factor != 1)
                    scaledFont.setPointSizeF((pSizeF * factor) + 2);
                else
                    scaledFont.setPointSize(font.pointSize() - 2);

                item->setFont(scaledFont);
            }
        }
    }

    connect(m_table, SIGNAL(cellChanged(int,int)),
            this, SLOT(slotCellChanged(int,int)));
}

void FixtureGroupEditor::addFixtureHeads(Qt::ArrowType direction)
{
    FixtureSelection fs(this, m_doc);
    fs.setMultiSelection(true);
    fs.setSelectionMode(FixtureSelection::Heads);
    fs.setDisabledHeads(m_grp->headList());
    if (fs.exec() == QDialog::Accepted)
    {
        int row = m_row;
        int col = m_column;
        beginEdit();
        foreach (GroupHead gh, fs.selectedHeads())
        {
            m_grp->assignHead(QLCPoint(col, row), gh);
            if (direction == Qt::RightArrow)
                col++;
            else if (direction == Qt::DownArrow)
                row++;
            else if (direction == Qt::LeftArrow)
                col--;
            else if (direction == Qt::UpArrow)
                row--;
        }

        endEdit();
        m_table->setCurrentCell(row, col);
    }
}

void FixtureGroupEditor::slotTableContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (idx.isValid() == false)
        return;
    const QLCPoint pt(idx.column(), idx.row());
    const GroupHead gh = m_grp->head(pt);
    if (gh.isValid() == false)
        return; // empty cell

    const quint32 sg = m_grp->headSubGroup(pt);
    const Fixture *fxi = m_doc->fixture(gh.fxi);

    QMenu menu(this);

    // Whole-fixture move: shifts every head of this fixture as one block.
    const QString fxName = (fxi != NULL) ? fxi->name() : tr("fixture");
    QAction *fUp    = menu.addAction(tr("Move %1 up").arg(fxName));
    QAction *fDown  = menu.addAction(tr("Move %1 down").arg(fxName));
    QAction *fLeft  = menu.addAction(tr("Move %1 left").arg(fxName));
    QAction *fRight = menu.addAction(tr("Move %1 right").arg(fxName));

    // Sub-group move (only when this cell belongs to a pasted block).
    QAction *up = NULL, *down = NULL, *left = NULL, *right = NULL;
    if (sg != 0)
    {
        menu.addSeparator();
        up    = menu.addAction(tr("Move sub-group up"));
        down  = menu.addAction(tr("Move sub-group down"));
        left  = menu.addAction(tr("Move sub-group left"));
        right = menu.addAction(tr("Move sub-group right"));
    }

    QAction *c = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (c == NULL)            return;
    else if (c == fUp)        moveFixture(gh.fxi, 0, -1);
    else if (c == fDown)      moveFixture(gh.fxi, 0, 1);
    else if (c == fLeft)      moveFixture(gh.fxi, -1, 0);
    else if (c == fRight)     moveFixture(gh.fxi, 1, 0);
    else if (c == up)         moveSubGroup(sg, 0, -1);
    else if (c == down)       moveSubGroup(sg, 0, 1);
    else if (c == left)       moveSubGroup(sg, -1, 0);
    else if (c == right)      moveSubGroup(sg, 1, 0);
}

bool FixtureGroupEditor::canMoveHeads(const QList<QLCPoint>& pts, int dx, int dy) const
{
    if (pts.isEmpty() || (dx == 0 && dy == 0))
        return false;

    foreach (const QLCPoint &p, pts)
    {
        const QLCPoint np(p.x() + dx, p.y() + dy);
        if (np.x() < 0 || np.y() < 0)
            return false;
        if (m_grp->head(np).isValid() && pts.contains(np) == false)
            return false;
    }
    return true;
}

void FixtureGroupEditor::moveHeads(const QList<QLCPoint>& pts, int dx, int dy)
{
    if (canMoveHeads(pts, dx, dy) == false)
        return;

    // Snapshot the heads and their sub-group tags before we disturb anything.
    QList<GroupHead> heads;
    QList<quint32> tags;
    foreach (const QLCPoint &p, pts)
    {
        heads << m_grp->head(p);
        tags  << m_grp->headSubGroup(p);
    }

    beginEdit();

    // Move: clear the block, then re-place (and re-tag) at the new cells.
    foreach (const QLCPoint &p, pts)
        m_grp->resignHead(p);

    int maxX = m_grp->size().width() - 1;
    int maxY = m_grp->size().height() - 1;
    for (int i = 0; i < pts.size(); i++)
    {
        const QLCPoint np(pts[i].x() + dx, pts[i].y() + dy);
        m_grp->assignHead(np, heads[i]);
        if (tags[i] != 0)
            m_grp->setHeadSubGroup(np, tags[i]);
        maxX = qMax(maxX, np.x());
        maxY = qMax(maxY, np.y());
    }

    if (maxX + 1 > m_grp->size().width() || maxY + 1 > m_grp->size().height())
    {
        m_grp->setSize(QSize(qMax(maxX + 1, m_grp->size().width()),
                             qMax(maxY + 1, m_grp->size().height())));
        m_xSpin->blockSignals(true); m_xSpin->setValue(m_grp->size().width());  m_xSpin->blockSignals(false);
        m_ySpin->blockSignals(true); m_ySpin->setValue(m_grp->size().height()); m_ySpin->blockSignals(false);
    }

    endEdit();
}

void FixtureGroupEditor::moveSubGroup(quint32 sg, int dx, int dy)
{
    if (sg == 0)
        return;

    QList<QLCPoint> pts;
    QMapIterator<QLCPoint, GroupHead> it(m_grp->headsMap());
    while (it.hasNext())
    {
        it.next();
        if (m_grp->headSubGroup(it.key()) == sg)
            pts << it.key();
    }
    moveHeads(pts, dx, dy);
}

void FixtureGroupEditor::moveFixture(quint32 fxid, int dx, int dy)
{
    QList<QLCPoint> pts;
    QMapIterator<QLCPoint, GroupHead> it(m_grp->headsMap());
    while (it.hasNext())
    {
        it.next();
        if (it.value().fxi == fxid)
            pts << it.key();
    }
    moveHeads(pts, dx, dy);
}

void FixtureGroupEditor::slotAddGroupBlock()
{
    // Pick another fixture group.
    QList<FixtureGroup*> others;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g != NULL && g->id() != m_grp->id())
            others.append(g);
    if (others.isEmpty())
        return;

    QStringList names;
    foreach (FixtureGroup *g, others)
        names << g->name();

    bool ok = false;
    const QString picked = QInputDialog::getItem(
        this, tr("Add group as block"),
        tr("Insert this group's fixtures as a block at the selected cell:"),
        names, 0, false, &ok);
    if (!ok || picked.isEmpty())
        return;
    FixtureGroup *src = others.at(names.indexOf(picked));

    const QMap<QLCPoint, GroupHead> srcHeads = src->headsMap();
    if (srcHeads.isEmpty())
        return;

    // Normalise the source layout to its top-left so it pastes as a block
    // anchored at the current cell.
    int minX = INT_MAX, minY = INT_MAX;
    QMapIterator<QLCPoint, GroupHead> mit(srcHeads);
    while (mit.hasNext()) { mit.next(); minX = qMin(minX, mit.key().x()); minY = qMin(minY, mit.key().y()); }

    const QList<GroupHead> existing = m_grp->headList();
    int maxX = m_grp->size().width() - 1;
    int maxY = m_grp->size().height() - 1;

    beginEdit();
    QMapIterator<QLCPoint, GroupHead> it(srcHeads);
    while (it.hasNext())
    {
        it.next();
        const GroupHead &gh = it.value();
        if (existing.contains(gh)) // already placed in this group
            continue;
        const int x = m_column + (it.key().x() - minX);
        const int y = m_row + (it.key().y() - minY);
        m_grp->assignHead(QLCPoint(x, y), gh);
        m_grp->setHeadSubGroup(QLCPoint(x, y), src->id()); // tag the block
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    }

    // Grow the grid to fit the pasted block.
    if (maxX + 1 > m_grp->size().width() || maxY + 1 > m_grp->size().height())
    {
        m_grp->setSize(QSize(qMax(maxX + 1, m_grp->size().width()),
                             qMax(maxY + 1, m_grp->size().height())));
        m_xSpin->blockSignals(true); m_xSpin->setValue(m_grp->size().width());  m_xSpin->blockSignals(false);
        m_ySpin->blockSignals(true); m_ySpin->setValue(m_grp->size().height()); m_ySpin->blockSignals(false);
    }

    endEdit();
}

/****************************************************************************
 * Edit batching + undo
 ****************************************************************************/

void FixtureGroupEditor::pushUndo()
{
    static const int kMaxUndo = 50;
    GroupState s;
    s.size = m_grp->size();
    s.heads = m_grp->headsMap();
    s.subGroups = m_grp->headSubGroupMap();
    m_undoStack.append(s);
    while (m_undoStack.size() > kMaxUndo)
        m_undoStack.removeFirst();
}

void FixtureGroupEditor::beginEdit()
{
    pushUndo();
    m_grp->blockSignals(true); // suppress the per-head changed() storm
}

void FixtureGroupEditor::endEdit()
{
    m_grp->blockSignals(false);
    m_grp->notifyChanged(); // single refresh of any listeners (tree, menus)
    if (m_doc != NULL)
        m_doc->setModified();
    updateTable();
}

void FixtureGroupEditor::cancelEdit()
{
    m_grp->blockSignals(false);
    if (m_undoStack.isEmpty() == false)
        m_undoStack.removeLast();
}

void FixtureGroupEditor::slotUndo()
{
    if (m_undoStack.isEmpty())
        return;

    const GroupState s = m_undoStack.takeLast();
    m_grp->restoreState(s.size, s.heads, s.subGroups);
    if (m_doc != NULL)
        m_doc->setModified();

    m_xSpin->blockSignals(true); m_xSpin->setValue(s.size.width());  m_xSpin->blockSignals(false);
    m_ySpin->blockSignals(true); m_ySpin->setValue(s.size.height()); m_ySpin->blockSignals(false);
    updateTable();
}

/****************************************************************************
 * Marquee selection + drag preview
 ****************************************************************************/

QLCPoint FixtureGroupEditor::cellAt(const QPoint& p) const
{
    int c = m_table->columnAt(p.x());
    int r = m_table->rowAt(p.y());
    // Clamp points past the last row/column to the nearest valid cell.
    if (c < 0) c = (p.x() < 0) ? 0 : qMax(0, m_table->columnCount() - 1);
    if (r < 0) r = (p.y() < 0) ? 0 : qMax(0, m_table->rowCount() - 1);
    return QLCPoint(c, r);
}

void FixtureGroupEditor::selectCellRange(const QLCPoint& a, const QLCPoint& b)
{
    const int maxC = m_table->columnCount() - 1;
    const int maxR = m_table->rowCount() - 1;
    if (maxC < 0 || maxR < 0)
        return;

    // Clamp both corners into the grid so a stale anchor can't escape bounds.
    const int c0 = qBound(0, qMin(a.x(), b.x()), maxC);
    const int c1 = qBound(0, qMax(a.x(), b.x()), maxC);
    const int r0 = qBound(0, qMin(a.y(), b.y()), maxR);
    const int r1 = qBound(0, qMax(a.y(), b.y()), maxR);

    QItemSelection sel;
    const QModelIndex topLeft = m_table->model()->index(r0, c0);
    const QModelIndex bottomRight = m_table->model()->index(r1, c1);
    sel.select(topLeft, bottomRight);
    m_table->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);

    const int curC = qBound(0, b.x(), maxC);
    const int curR = qBound(0, b.y(), maxR);
    m_table->setCurrentCell(curR, curC, QItemSelectionModel::NoUpdate);
    slotCellActivated(curR, curC);
}

void FixtureGroupEditor::showDropPreview(int dx, int dy)
{
    if (m_dragCells.isEmpty())
        return;

    // Bounding box of the dragged block, shifted by the candidate delta.
    int c0 = INT_MAX, c1 = INT_MIN, r0 = INT_MAX, r1 = INT_MIN;
    foreach (const QLCPoint &p, m_dragCells)
    {
        c0 = qMin(c0, p.x() + dx); c1 = qMax(c1, p.x() + dx);
        r0 = qMin(r0, p.y() + dy); r1 = qMax(r1, p.y() + dy);
    }
    c0 = qMax(0, c0); r0 = qMax(0, r0);
    c1 = qMin(m_table->columnCount() - 1, c1);
    r1 = qMin(m_table->rowCount() - 1, r1);
    if (c1 < c0 || r1 < r0)
    {
        hideDropPreview();
        return;
    }

    const QRect tl = m_table->visualRect(m_table->model()->index(r0, c0));
    const QRect br = m_table->visualRect(m_table->model()->index(r1, c1));
    m_dropBand->setGeometry(tl.united(br));
    m_dropBand->show();
}

void FixtureGroupEditor::hideDropPreview()
{
    if (m_dropBand != NULL)
        m_dropBand->hide();
}
