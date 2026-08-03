/*
  Q Light Controller Plus
  monitorlayerspanel.h

  A dockable "Layers" side panel for the 2-D Monitor. Presents the map as a
  tree: layers at the top level, group folders nested under their layer
  (group-of-groups supported), and every map item as a leaf under its group or
  directly under its layer. Layers can be added / removed / reordered and each
  carries a visibility + lock toggle; the selected layer is the active one
  (where newly-placed items land). Names of layers and groups are edited inline
  (Enter / F2 / double-click). A multi-selection of items can be grouped or
  moved to a layer from the context menu. Editing is disabled in Operate mode.

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

#ifndef MONITORLAYERSPANEL_H
#define MONITORLAYERSPANEL_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QPair>
#include <QColor>
#include <QTreeWidget>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>

class QTreeWidgetItem;
class QToolButton;
class QPoint;
class QEvent;
class Doc;
class MonitorGraphicsView;
class MonitorProperties;

/** QTreeWidget subclass that hands drops to us instead of doing its own row
 *  moves. Overriding dropEvent (and NOT calling the base) is the only reliable
 *  way to stop Qt's InternalMove from reparenting/removing rows behind our back
 *  — the event-filter approach raced Qt's own drop machinery. */
class LayersTreeWidget final : public QTreeWidget
{
    Q_OBJECT
public:
    explicit LayersTreeWidget(QWidget *parent = nullptr) : QTreeWidget(parent) {}

signals:
    /** Emitted on drop: the dragged rows and the row they were dropped onto
     *  (nullptr if dropped on empty space). */
    void itemsDropped(const QList<QTreeWidgetItem *> &dragged, QTreeWidgetItem *target);

    /** Bracket the whole drag: dragStarted before Qt's nested drag loop, and
     *  dragFinished after it exits. The panel suppresses tree rebuilds in
     *  between (rebuilding mid-drag frees rows Qt is still using) and does a
     *  single reload on dragFinished. */
    void dragStarted();
    void dragFinished();

protected:
    // Let the base handle drag-move so the drop INDICATOR is drawn, then force
    // acceptance so a drop is allowed anywhere in the tree.
    void dragEnterEvent(QDragEnterEvent *e) override { QTreeWidget::dragEnterEvent(e); e->acceptProposedAction(); }
    void dragMoveEvent(QDragMoveEvent *e) override { QTreeWidget::dragMoveEvent(e); e->acceptProposedAction(); }

    void dropEvent(QDropEvent *e) override
    {
        // Do the model change via the signal, but DON'T let the base move rows.
        emit itemsDropped(selectedItems(), itemAt(e->pos()));
        e->setDropAction(Qt::CopyAction);   // never a Move → Qt won't remove rows
        e->accept();
    }

    void startDrag(Qt::DropActions supportedActions) override
    {
        emit dragStarted();
        QTreeWidget::startDrag(supportedActions);   // runs the nested drag loop + delivers the drop
        emit dragFinished();                        // loop has exited — safe to rebuild now
    }
};

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class MonitorLayersPanel final : public QWidget
{
    Q_OBJECT

public:
    MonitorLayersPanel(Doc *doc, MonitorGraphicsView *view, QWidget *parent = nullptr);

    /** Rebuild the tree from MonitorProperties. Call whenever layers, groups or
     *  items may have changed (e.g. after a workspace load or a group op). */
    void reload();

signals:
    /** Emitted when the in-panel close/hide button is clicked. */
    void closeRequested();

protected:
    /** Intercepts Enter/Return on the tree to start inline rename. */
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void slotAddLayer();
    void slotRemoveLayer();
    void slotMoveUp();
    void slotMoveDown();
    void slotMoveSelectionHere();
    void slotItemClicked(QTreeWidgetItem *item, int column);
    void slotItemDoubleClicked(QTreeWidgetItem *item, int column);
    void slotItemChanged(QTreeWidgetItem *item, int column);
    void slotContextMenu(const QPoint &pos);
    void updateEditableState();

private:
    /** Node kinds stashed in a tree item's data. */
    enum NodeType { NodeLayer = 0, NodeGroup = 1, NodeItem = 2 };

    /** One placeable map item, gathered once per reload. */
    struct ItemDesc
    {
        QString kind;       ///< "fixture" / "truss" / "platform" / "target" / "power"
        quint32 id;
        QString name;
        quint32 layerId;
        quint32 groupId;    ///< immediate group (0 = loose)
        QColor  color;      ///< platform colour (for the tree swatch); else invalid
    };

    QList<ItemDesc> gatherItems() const;
    void buildGroupNode(QTreeWidgetItem *parent, quint32 groupId,
                        const QList<ItemDesc> &items);
    void addItemLeaf(QTreeWidgetItem *parent, const ItemDesc &d);
    /** The tree icon for an item KIND (fixture/truss/platform/pipe/stand/tower/
     *  image/power). @p color tints the platform swatch. */
    QIcon kindIcon(const QString &kind, const QColor &color) const;

    /** Remove the given (kind,id) leaves from their current group. A fixture
     *  mounted on the group's anchor structure is also detached from that mount
     *  (keeping its world position) so the auto-grouping cannot re-add it. */
    void removeLeavesFromGroup(const QList<QPair<QString, quint32> > &targets);

    /** (kind,id) of every leaf item covered by the current tree selection
     *  (selected leaves plus every leaf under a selected group). */
    QList<QPair<QString, quint32> > selectedObjects() const;
    void collectLeaves(QTreeWidgetItem *node, QList<QPair<QString, quint32> > &out) const;

    /** Perform a drag-to-reparent drop of @p dragged onto @p target (layer /
     *  group / item container). Reparents items and nests/moves groups. */
    void handleTreeDrop(const QList<QTreeWidgetItem *> &dragged, QTreeWidgetItem *target);

    /** Layer id owning the current row (walks up to the top-level layer node). */
    quint32 currentLayerId() const;
    bool currentIsLayer() const;

    void beginRenameCurrent();
    /** Rename the underlying map item (fixture/truss/platform/power). */
    void renameMapItem(const QString &kind, quint32 id, const QString &name);

    /** Per-item position lock for an object by (kind,id): truss / platform /
     *  image / power. Fixtures have no per-item lock (returns false / no-op). */
    bool objectLocked(const QString &kind, quint32 id) const;
    void setObjectLocked(const QString &kind, quint32 id, bool locked);
    /** True if this kind supports a per-item position lock. */
    bool kindLockable(const QString &kind) const;
    /** Reorder the layer at @p row by @p delta (±1), renumbering all orders. */
    void reorderLayer(int row, int delta);
    /** Select and scroll to the top-level layer node with the given id. */
    void selectLayerNode(quint32 layerId);
    void updateButtons();
    void toggleVisible(quint32 layerId, bool visible);
    void toggleLocked(quint32 layerId, bool locked);

    Doc                 *m_doc;
    MonitorProperties   *m_props;
    MonitorGraphicsView *m_view;

    LayersTreeWidget *m_tree;
    QToolButton *m_addBtn;
    QToolButton *m_removeBtn;
    QToolButton *m_upBtn;
    QToolButton *m_downBtn;
    QToolButton *m_moveHereBtn;
    QToolButton *m_hideBtn;

    /** False in Operate mode: all structural editing is blocked. */
    bool m_editable = true;

    /** True while a tree drag is in flight — reload() is suppressed so we never
     *  rebuild the tree inside Qt's nested drag event loop. */
    bool m_suppressReload = false;

    /** After a drop, the (queued) reload scrolls this layer back into view so
     *  the target layer never appears to "vanish". <0 = none. */
    int m_focusLayerAfterReload = -1;

    /** Guards row-widget/selection/edit handlers while reload() repopulates. */
    bool m_reloading = false;
};

/** @} */

#endif // MONITORLAYERSPANEL_H
