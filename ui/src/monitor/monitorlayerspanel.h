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
#include <QSet>
#include <QColor>
#include <QTreeWidget>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include "qlceventpos.h"
#include "monitorproperties.h"

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

private:
    int m_scrollRoom = 0;           ///< how far the bar lies over the list

public:

    /** A row's name is being typed in. */
    bool isEditing() const { return state() == QAbstractItemView::EditingState; }

    /** Where the vertical scroll bar lies over the list (macOS overlay
     *  scroll bars), how far it reaches in: the room the row buttons keep
     *  clear on the right (scrollRoom()), so the bar never covers a layer's
     *  eye and lock. Measured from where Qt put the bar, not predicted from
     *  the style: a bar laid out beside the list (Fusion, Windows) needs
     *  none. (Not a viewport margin: QTreeView resets those.) */
    void reserveScrollBarRoom();
    int scrollRoom() const { return m_scrollRoom; }
    /** A row's buttons, padded clear of the bar. */
    void padRowWidget(QWidget *row) const;

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
    /** The bar now reaches a different distance into the list. */
    void scrollRoomChanged();

protected:
    void changeEvent(QEvent *e) override;
    /** After every relayout: the bar may have come, gone or moved. */
    void updateGeometries() override;

    // Let the base handle drag-move so the drop INDICATOR is drawn, then force
    // acceptance so a drop is allowed anywhere in the tree.
    void dragEnterEvent(QDragEnterEvent *e) override { QTreeWidget::dragEnterEvent(e); e->acceptProposedAction(); }
    void dragMoveEvent(QDragMoveEvent *e) override { QTreeWidget::dragMoveEvent(e); e->acceptProposedAction(); }

    void dropEvent(QDropEvent *e) override
    {
        // Do the model change via the signal, but DON'T let the base move rows.
        emit itemsDropped(selectedItems(), itemAt(qlcEventPos(e)));
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

    /** The right-click menu for @p item (null = blank space), built but not
     *  shown: the item's own section comes from the plot, so the list offers
     *  everything the plot does. */
    void buildContextMenu(class QMenu *menu, QTreeWidgetItem *item);

    /** Select and scroll to the row for @p kind / @p id ("Show in Layers").
     *  False when the list has no such row. */
    bool revealItem(const QString &kind, quint32 id);

    /** Mirror the plot's selection: select exactly the rows for @p objects
     *  (kind, id), open their folders and scroll to the first -- without
     *  handing the selection back to the plot. */
    void followMapSelection(const QList<QPair<QString, quint32> > &objects);

    /** Select the rows of the fixture groups @p ids (the first row of each, as
     *  a group can be listed in several places), and their fixtures on the
     *  plot. */
    void selectFixtureGroups(const QList<quint32> &ids);

signals:
    /** Emitted when the in-panel close/hide button is clicked. */
    void closeRequested();
    /** Double-clicking a Fixture Group node asks the host to open its head-layout
     *  mapping editor (same dialog as the Programming tab). */
    void editFixtureGroupRequested(quint32 fixtureGroupId);

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
    enum NodeType { NodeLayer = 0, NodeGroup = 1, NodeItem = 2, NodeFixtureGroup = 3 };

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
    /** Walks the (about-to-be-discarded) tree collecting a key per collapsed
     *  layer/group node, so reload() can restore collapse state afterward
     *  instead of hard-expanding everything -- see m_collapsedKeys. */
    void collectCollapsedNodes(QTreeWidgetItem *item, QSet<QString> &out) const;
    /** True if the layer/group node identified by (type, id) was collapsed
     *  before the reload() currently in progress captured m_collapsedKeys. */
    bool wasNodeCollapsed(NodeType type, quint32 id) const;
    /** Expand or collapse a node and all of its descendants. */
    static void setSubtreeExpanded(QTreeWidgetItem *node, bool expanded);
    /** Hide tree rows that don't match @p text (matches keep their ancestors
     *  visible); empty shows everything. Applies "Selected only" too. */
    void filterTree(const QString &text);
    /** The search box and "Selected only", together, on the rows as built. */
    void applyFilters();
    /** What is hung on what, as a tree: within one layer, a row moves under
     *  the row of what it is mounted on -- a lantern under its hanger, the
     *  hanger under its tower or truss, a bar under its truss or stand, a
     *  crossbar under its boom. Rows under a fixture group stay there. */
    void nestByMounts(QTreeWidgetItem *layerNode);
    /** "kind:id" of what @p kind / @p id is mounted on, or empty. */
    QString mountParentKey(const QString &kind, quint32 id) const;
    /** A row's lock badge: a padlock when the object is locked on its own. */
    QString lockBadge(const QString &kind, quint32 id) const;
    /** A new empty folder on @p layerId (inside @p parentGroupId when not 0),
     *  its name ready to type. */
    void addFolder(quint32 layerId, quint32 parentGroupId);
    /** Create a lighting Fixture Group (prompted name) from the given fixtures —
     *  the explicit "make a control group" action (distinct from map Folders). */
    void createFixtureGroupFrom(const QList<quint32> &fixtureIds);
    /** Under @p host, add a 💡 node for every Fixture Group whose members are ALL
     *  within @p fixtureSet (local to this context); records fid → node in @p map
     *  so the caller can nest those fixtures. Works for a group's members OR a
     *  layer's loose fixtures. */
    void addLocalFixtureGroups(QTreeWidgetItem *host, const QSet<quint32> &fixtureSet,
                               QHash<quint32, QList<QTreeWidgetItem *> > &map);
    /** The plot objects a fixture group stands for: its fixtures on the plot. */
    QList<QPair<QString, quint32> > fixtureGroupMembers(quint32 fgId) const;
    /** A Fixture Group row (no children). */
    QTreeWidgetItem *addFixtureGroupNode(QTreeWidgetItem *host, class FixtureGroup *fg);
    /** Fixture Groups listed by this reload, so the ones local nowhere -- a
     *  group across several trusses -- can be given a home at the end. */
    QSet<quint32> m_listedFixtureGroups;
    void addItemLeaf(QTreeWidgetItem *parent, const ItemDesc &d);
    /** A stage piece's own eye and lock on its row (Q147). */
    void addStagePieceToggles(QTreeWidgetItem *node, MonitorProperties::StagePiece piece);
    /** The tree icon for an item KIND (fixture/truss/platform/pipe/stand/tower/
     *  image/power). @p color tints the platform swatch. */
    QIcon kindIcon(const QString &kind, const QColor &color) const;

    /** Remove the given (kind,id) leaves from their current group. A fixture
     *  mounted on the group's anchor structure is also detached from that mount
     *  (keeping its world position) so the auto-grouping cannot re-add it. */
    void removeLeavesFromGroup(const QList<QPair<QString, quint32> > &targets);

    /** (kind,id) of every leaf item covered by the current tree selection
     *  (selected leaves plus every leaf under a selected group). */
    /** The selection when the last click began (see eventFilter()). */
    QList<QPair<QString, quint32> > m_pressSelection;
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
    /** False when the plot refused the name (a fixture name already taken). */
    bool renameMapItem(const QString &kind, quint32 id, const QString &name);

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
    /** Guards the tree -> canvas selection sync against feeding back. */
    bool m_syncingSelection = false;

    LayersTreeWidget *m_tree;
    class QLineEdit *m_search = nullptr;   //!< filter box above the tree
    /** "Selected only": the list shows the selection and what holds it. */
    QToolButton *m_selectedOnlyBtn = nullptr;
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
    bool m_reloadQueued = false;   ///< a deferred reload (after a rename) is pending
    bool m_reloadAfterEdit = false; ///< a reload asked for while a name was being typed

    /** After a drop, the (queued) reload scrolls this layer back into view so
     *  the target layer never appears to "vanish". <0 = none. */
    int m_focusLayerAfterReload = -1;

    /** Guards row-widget/selection/edit handlers while reload() repopulates. */
    bool m_reloading = false;

    /** Layer/group nodes the user had collapsed, captured at the start of the
     *  reload() currently in progress (from the tree about to be cleared) and
     *  consulted while rebuilding, so a reload — triggered by ~20 different
     *  things (toggling a layer's visibility/lock, renaming, drag-drop, add/
     *  remove layer, ...) — no longer force-expands every folder the user had
     *  deliberately closed. Keyed "<NodeType>:<id>"; only meaningful during a
     *  single reload() call, not persisted across app runs. */
    QSet<QString> m_collapsedKeys;
};

/** @} */

#endif // MONITORLAYERSPANEL_H
