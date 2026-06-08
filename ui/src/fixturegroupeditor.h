/*
  Q Light Controller
  fixturegroupeditor.h

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

#ifndef FIXTUREGROUPEDITOR_H
#define FIXTUREGROUPEDITOR_H

#include <QWidget>
#include <QList>
#include <QPoint>
#include <QMap>
#include <QSize>

#include "ui_fixturegroupeditor.h"
#include "qlcpoint.h"
#include "grouphead.h"

class FixtureGroup;
class QRubberBand;
class QTimer;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

class FixtureGroupEditor final : public QWidget, public Ui_FixtureGroupEditor
{
    Q_OBJECT

public:
    FixtureGroupEditor(FixtureGroup* grp, Doc* doc, QWidget* parent);
    ~FixtureGroupEditor();

protected:
    /** Catch fixture drops on the layout grid (drag from the fixture tree). */
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateTable();
    void addFixtureHeads(Qt::ArrowType direction);
    /** Begin an internal drag of the currently selected (occupied) cells. */
    void startCellDrag();
    /** Shift the given set of head cells by (dx,dy) as a unit, preserving
     *  each head's sub-group tag. Used by the sub-group and whole-fixture
     *  move helpers. No-op if the move would go out of bounds or collide
     *  with a head outside the moving set. */
    void moveHeads(const QList<QLCPoint>& pts, int dx, int dy);
    /** True if every cell in $pts could be shifted by (dx,dy) without leaving
     *  the top/left edge or colliding with a head outside the moving set. */
    bool canMoveHeads(const QList<QLCPoint>& pts, int dx, int dy) const;
    /** Shift every head tagged with subGroupId by (dx,dy) as a unit. */
    void moveSubGroup(quint32 subGroupId, int dx, int dy);
    /** Shift every head of fixture $fxid by (dx,dy) as a unit. */
    void moveFixture(quint32 fxid, int dx, int dy);

    // --- Edit batching + undo -----------------------------------------------
    /** Snapshot the group's current layout onto the undo stack. Call before a
     *  mutating edit. */
    void pushUndo();
    /** Begin a batched edit: snapshot for undo and silence per-head signals. */
    void beginEdit();
    /** Finish a batched edit: emit one change, mark modified, refresh. */
    void endEdit();
    /** Abort a batched edit that changed nothing: unblock and drop the
     *  snapshot pushed by beginEdit(). */
    void cancelEdit();

    // --- Marquee selection + drag preview -----------------------------------
    /** Cell under viewport point $p, clamped into the grid. */
    QLCPoint cellAt(const QPoint& p) const;
    /** Select the rectangular block of cells between two corners. */
    void selectCellRange(const QLCPoint& a, const QLCPoint& b);
    /** Show/refresh the drop wireframe for the dragged block at delta (dx,dy). */
    void showDropPreview(int dx, int dy);
    /** Hide the drop wireframe. */
    void hideDropPreview();

private slots:
    void slotNameEdited(const QString& text);
    void slotXSpinValueChanged(int value);
    void slotYSpinValueChanged(int value);

    void slotRightClicked();
    void slotLeftClicked();
    void slotUpClicked();
    void slotDownClicked();
    void slotRemoveFixtureClicked();
    /** Add another fixture group's heads as a block (preserving its
     *  relative layout) at the current cell. */
    void slotAddGroupBlock();

    /** Right-click the grid: move the sub-group under the cursor as a unit. */
    void slotTableContextMenu(const QPoint &pos);

    void slotCellActivated(int row, int column);
    void slotCellChanged(int row, int column);
    void slotResized();
    /** Re-fit each cell's font to the column width. Skipped for large grids
     *  (it is O(cells) and dominates the cost of rendering big groups). */
    void relayoutCellFonts();
    /** Undo the last layout edit (Ctrl+Z). */
    void slotUndo();

private:
    FixtureGroup* m_grp; //! The group being edited
    Doc* m_doc;          //! The QLC engine object
    int m_row;           //! Currently selected row
    int m_column;        //! Currently selected column
    QTimer* m_resizeTimer; //! Coalesces sectionResized bursts into one relayout

    // Internal drag of selected cells (single or multi) within the grid.
    enum DragMode { NoDrag, Marquee, MoveBlock };
    DragMode m_dragMode = NoDrag; //! What a left-press started
    QPoint m_dragStartPos;        //! Press position that may turn into a drag
    QLCPoint m_marqueeAnchor;     //! First corner of a marquee selection
    QList<QLCPoint> m_dragCells;  //! Occupied cells captured at drag start
    QLCPoint m_dragAnchor;        //! Reference cell the drag started on
    QRubberBand* m_dropBand = nullptr; //! Wireframe shown at the drop target

    // Undo stack of full group layouts (size + heads + sub-group tags).
    struct GroupState
    {
        QSize size;
        QMap<QLCPoint, GroupHead> heads;
        QMap<QLCPoint, quint32> subGroups;
    };
    QList<GroupState> m_undoStack;
};

/** @} */

#endif
