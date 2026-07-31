/*
  Q Light Controller Plus
  monitorgraphicsview.cpp

  Copyright (C) Massimo Callegari

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

#ifndef MONITORGRAPHICSVIEW_H
#define MONITORGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <QSet>
#include <QVector3D>

#include "fixture.h"
#include "function.h"

class MonitorProperties;
class MonitorFixtureItem;
class TrussItem;
class PlatformItem;
class MonitorBoomItem;
class PowerSourceItem;
class TargetItem;
class MonitorImageItem;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class MonitorGraphicsView final : public QGraphicsView
{
    Q_OBJECT

public:
    MonitorGraphicsView(Doc *doc, QWidget *parent = 0);
    ~MonitorGraphicsView();

    /** Read-only point-of-view for the 2D projection. Top is the fully-editable
     *  default; Front and Side are elevation views (display only). */
    enum ViewPOV { PovTop = 0, PovFront = 1, PovSide = 2 };

    /** Set the current point of view and rebuild the projection. */
    void setViewPOV(ViewPOV pov);

    /** Current point of view. */
    ViewPOV viewPOV() const { return m_pov; }

    /** True when the view is an elevation (Front/Side) — editing is disabled. */
    bool isElevation() const { return m_pov != PovTop; }

    /** True if @p t is a bar we allow the user to DRAG in the current elevation
     *  view (WYSIWYG placement). Scope: a tower crossbar (Across) shown broadside
     *  in the Front view → drag = height (Along) + horizontal (cross-shift). */
    bool elevationBarDraggable(const class Truss *t) const;

    /** True if the truss-bound fixture @p fid can be dragged in the current
     *  elevation view — it slides ALONG its truss/bar (Front view only). */
    bool elevationFixtureDraggable(quint32 fid) const;

    /** "Stage view": hide everything except the scenic/structural features
     *  (trusses, platforms, images) so the bare stage can be seen. Fixtures,
     *  targets and power sources are hidden while on. */
    void setStageFeaturesOnly(bool on);
    bool stageFeaturesOnly() const { return m_stageOnly; }

    /** "Build/Rig focus": fixtures are ghosted (faint + click-through) and
     *  targets/power hidden, so trusses/platforms/images are the interaction
     *  target — build structure without fighting the lights, while still seeing
     *  where they are. */
    void setBuildFocus(bool on);
    bool buildFocus() const { return m_buildFocus; }

    /** Set the graphics view size in monitor units */
    void setGridSize(QSize size);

    /** Get the grid size in monitor units */
    QSize gridSize() const { return m_gridSize; }

    /** Set the number of sub-divisions drawn inside each grid cell */
    void setGridSubdivisions(int subdivisions);

    /** Show/hide the grid lines (the cell geometry is still computed, so item
     *  positions are unaffected). */
    void setGridVisible(bool on);
    bool gridVisible() const { return m_gridVisible; }

    /** Get the current number of grid sub-divisions */
    int gridSubdivisions() const { return m_gridSubdivisions; }

    /** Lock/unlock the layout. When locked fixtures cannot be moved
     *  but can still be selected (rubber-band selection surface) */
    void setLayoutLocked(bool locked);

    /** Return true if the layout is currently locked */
    bool layoutLocked() const { return m_layoutLocked; }

    /** Set the snap-to-grid level (0 = off, 1 = full, 2 = 1/2, 4 = 1/4) */
    void setSnapDivisions(int divisions);

    /** Get the current snap-to-grid level */
    int snapDivisions() const { return m_snapDivisions; }

    /** Set the measure unit to use */
    void setGridMetrics(float value);

    /** Get the currently selected fixture ID.
     *  Fixture::invalidId is returned if none is selected */
    quint32 selectedFixtureID();

    /** Return a list of the fixture IDs in the current view */
    QList <quint32> fixturesID() const;

    /** Retrieve the currently selected MonitorFixtureItem.
     *  Return NULL if none */
    MonitorFixtureItem *getSelectedItem();

    /** Return all selected MonitorFixtureItems (may be empty). */
    QList<MonitorFixtureItem *> selectedFixtureItems() const;

    /** Fixture Studio (FIXTURESTUDIO_DESIGN.md): promote the current fixture
     *  multi-selection into a studio group — a MonitorGroup carrying a local
     *  frame whose members' world positions are derived. Returns the new group
     *  id, or 0 if fewer than two fixtures are selected. */
    quint32 createStudioGroupFromSelection();

    /** Open the modal studio-group editor (frame origin/rotation + member
     *  local offsets) for @p groupId, refreshing the map live. */
    void openStudioGroupEditor(quint32 groupId);

    /** Open @p groupId in the studio editor, first promoting a plain group into
     *  a studio group (enable its frame, adopting current fixture positions) if
     *  it is not one yet. This is the Layers-panel entry point — edit an
     *  existing group (US-1, …) as a studio object directly. */
    void openStudioGroupForGroup(quint32 groupId);

    /** Stamp a saved studio template (component) onto the current fixture
     *  selection: pick a .json, bind its roles to the selected fixtures in
     *  order, and drop a placed studio group. */
    void stampStudioTemplate();

    /** Open the browsable Studio Components library. Any component can be stamped
     *  onto the current fixture selection (bound to roles in order); also the place
     *  to rename/delete/import components. Works with no selection (browse only). */
    void browseStudioComponents();

    /** Return the MonitorFixtureItem for the given fixture ID, or nullptr if not in the view. */
    MonitorFixtureItem *fixtureItemForId(quint32 fxId) const;

    /** Set the gel color of the fixture with the given ID */
    void setFixtureGelColor(quint32 id, QColor col);

    /** Set the rotation degrees of the fixture with the given ID */
    void setFixtureRotation(quint32 id, ushort degrees);

    /** Show/hide fixtures items labels */
    void showFixturesLabels(bool visible);

    /** Re-evaluate every fixture label from the global toggle AND its layer's
     *  labels flag. Call after a layer's labels toggle changes. */
    void refreshFixtureLabels();

    /** Bounding rect (viewport pixels) of the currently selected fixtures, or an
     *  invalid QRect if none. Used to band the rulers with the selection's
     *  extent while it is moved. */
    QRect selectionViewportRect() const;

    /** Return the gel color of the fixture with the given ID */
    QColor fixtureGelColor(quint32 id);

    /** Add a fixture to the current view */
    void addFixture(quint32 id, QPointF pos = QPointF(0, 0));

    /** Remove the fixture with the given ID from the view
     *  If no ID is specified, the currently selected
     *  fixture will be removed (if possible)
     */
    bool removeFixture(quint32 id = Fixture::invalidId());

    /** Remove all fixtures from the current view */
    void clearFixtures();

    /** Support function to convert a position in millimeters
     *  to a position in pixels */
    QPointF realPositionToPixels(qreal xpos, qreal ypos);

    /** Project a world point (millimetres, X=stage-right, Y=upstage, Z=height)
     *  to scene pixels for the current POV. Top: (X,Y); Front: (X,Z↑);
     *  Side: (Y,Z↑). In Top view this equals realPositionToPixels(x,y). */
    QPointF projectMm(qreal xMm, qreal yMm, qreal zMm) const;

    /** Inverse of realPositionToPixels — convert scene-pixel position to
     *  millimetres (same units as MonitorProperties fixture positions). */
    QPointF pixelsToRealPosition(qreal px, qreal py);

    /** Update the position and the scale of the fixture with
     *  the given ID
     */
    void updateFixture(quint32 id);

    /** Reposition every riser-mounted fixture from its platform geometry. Call
     *  after a platform is moved/resized so its mounted fixtures follow. */
    void refreshRiserFixtures();

    /** Set a background image for the view */
    void setBackgroundImage(QString filename);

    /** Set a flat background color (pass an invalid QColor to clear). */
    void setBackgroundColor(const QColor &color);

    /** Retrieve the path to the background image currently set */
    QString backgroundImage() { return m_backgroundImage; }

    /** Re-sync every fixture item's "bound to truss" flag from its rig props.
     *  Call after a truss is removed so its ex-fixtures stop behaving as bound. */
    void refreshFixtureBindings();

    /** Re-derive every child-bar truss's origin from its parent, redraw the
     *  trusses, and reposition fixtures riding on the bars. Call after a truss
     *  moves/resizes so bars (and their fixtures) follow their parent. */
    void followParentTrusses();

    /** Rebuild the truss overlay items from MonitorProperties. */
    void updateTrusses();

    /** Rebuild the platform overlay items from MonitorProperties. */
    void updatePlatforms();

    /** Rebuild the power-source markers from the power-distribution model. */
    void updatePowerSources();

    /** Power view: tint each source marker with its circuits' colours (key
     *  "sourceIdx:circuitIdx"). Empty map restores the plain markers. */
    void setPowerSourceColors(const QHash<QString, QColor> &circuitColors);

    /** Rebuild the placeable image items from MonitorProperties (only those on
     *  the plane matching the current POV are shown). */
    void updateImages();

    /** Rebuild the target marker items from MonitorProperties. */
    void updateTargets();

    /** Reposition existing TargetItems in place from their StageTarget data
     *  (no recreate) — used when the joystick moves an aim target live so the
     *  marker tracks without the churn of updateTargets(). */
    void repositionTargetItems();

    /** Rebuild the aim-line overlays for all selected TargetItems. */
    void updateAimLines();

    /** Recompute every item's visibility and movability from the three lock
     *  sources — the global layout lock, the item's own per-item lock, and its
     *  organizational layer's visible/locked state. Call after any layer change
     *  or global-lock toggle, and at the end of each item-rebuild path. */
    void refreshItemLayerState();

    /** Assign every currently-selected map item (fixture / truss / platform /
     *  target / power source) to the given organizational layer, then refresh.
     *  Returns the number of items reassigned. */
    int setSelectedItemsLayer(quint32 layerId);

    /** True if at least one map item is currently selected on the canvas. */
    bool hasSelection() const;

    /** How many fixtures are selected (align/distribute need >= 2 / >= 3). */
    int selectedFixtureCount() const;

    /** Align the selected fixtures' XY (Top view). Modes: 0=same X (column),
     *  1=left, 2=right, 3=same Y (row), 4=top, 5=bottom. Returns count moved. */
    int alignSelectedFixtures(int mode);
    /** Evenly space the selected fixtures along X (horizontal) or Y. */
    int distributeSelectedFixtures(bool horizontal);

    /** Reassign every item on layer @p fromLayerId to @p toLayerId, then
     *  refresh. Used when a layer is deleted so its members fall back to a real
     *  layer (rather than an id that a future addLayer() could re-issue). */
    void reassignLayerItems(quint32 fromLayerId, quint32 toLayerId);

    /** Group the current canvas selection (needs >= 2 items) into a new map
     *  group; returns the number of items grouped (0 if fewer than 2). Grouped
     *  items select and move together. */
    int groupSelectedItems();

    /** Ungroup every group represented in the current selection; returns the
     *  number of items ungrouped. */
    int ungroupSelectedItems();

    /** Dissolve a specific group by one level: its child groups and items are
     *  promoted to the group's parent. Used by the Layers-tree context menu. */
    void ungroupById(quint32 groupId);

    /** True if the current selection contains at least one grouped item. */
    bool selectionHasGroup() const;

    /** True if the current selection has >= 2 items (i.e. is groupable). */
    bool selectionGroupable() const;

    /** Create registry entries for any item groupId that lacks one (migration
     *  from the anonymous first-cut groups). Call after a workspace load. */
    void ensureGroupRegistry();

    /** Ensure a riser (platform) and the fixtures mounted on it share a map
     *  group (named after the platform); mounted fixtures are forced in. Called
     *  after the Face Editor mounts fixtures. */
    void ensurePlatformGroup(quint32 platformId);

    /** Ensure the truss and its bound fixtures share a map group: create the
     *  group (named after the truss) if needed, and pull in any bound fixtures
     *  that are still ungrouped. Leaves manually-grouped items alone. */
    void ensureTrussGroup(quint32 trussId);

    /** Select on the canvas every item under group @p gid (recursively through
     *  nested sub-groups). Used by the Layers tree when a group folder is
     *  clicked. */
    void selectItemsInGroup(quint32 gid);

    /** Select a single map item by kind ("fixture"/"truss"/"platform"/
     *  "target"/"power") and id. Used by the Layers tree item leaves. */
    void selectMapItem(const QString &kind, quint32 id);

    /** Select a set of map items (kind,id) on the canvas at once. Used by the
     *  Layers tree so a multi-selection there can be grouped / moved. */
    void selectMapItems(const QList<QPair<QString, quint32> > &items);

    /** Open the editor for a map item (emits the matching *DoubleClicked signal
     *  the Monitor already handles). Used by a Layers-tree double-click. */
    void requestEditItem(const QString &kind, quint32 id);

    /** Reparent a set of (kind,id) items to a layer (loose, ungrouped). */
    void reparentToLayer(const QList<QPair<QString, quint32> > &items, quint32 layerId);

    /** Reparent a set of (kind,id) items into a group (adopting its layer). */
    void reparentToGroup(const QList<QPair<QString, quint32> > &items, quint32 groupId);

    /** Move a group to be top-level under a layer (subtree layer updated). */
    void reparentGroupToLayer(quint32 groupId, quint32 layerId);

    /** Nest a group inside another group (no-op if that would form a cycle). */
    void reparentGroupToGroup(quint32 groupId, quint32 parentGroupId);

    /** Bind fixture @p fid to truss @p trussId, snapping it to the nearest point
     *  on the truss line and pulling it into the truss's group. Used by the
     *  fixture context menu and the Layers-tree drag-onto-truss (2D drop-to-bind
     *  was removed so assignment is now always explicit). */
    void attachFixtureToTruss(quint32 fid, quint32 trussId);
    void attachFixtureToBoom(quint32 fid, quint32 boomId);

    /** Unbind fixture @p fid from its truss (leaves it where it sits). */
    void detachFixtureFromTruss(quint32 fid);

    /** Show or hide the followspot beam-position pin on the floor map.
     *  @p xMeters, @p yMeters give the floor hit in metres (stage space).
     *  Pass visible=false to hide the pin. */
    void setFollowSpotPin(bool visible, float xMeters, float yMeters);

    /** Highlight (orange border) a set of fixtures by ID; clears previous highlights. */
    void highlightFixtures(const QList<quint32> &ids);

    /** Set the active scene whose target palette links are shown as aim lines.
     *  Pass Function::invalidId() to hide all aim lines. */
    void setActiveScene(quint32 sceneId);

    /* -------------------------------------------------------------------- *
     *  Rulers / coordinate readout / settable 0,0 origin
     * -------------------------------------------------------------------- */

    /** A single ruler graduation: its position along the ruler in *viewport*
     *  pixels, the label to draw, and whether it's a major (labelled) tick. */
    struct RulerTick { qreal pixel; QString label; bool major; };

    /** Graduations for the top (horizontal=true) or left (horizontal=false)
     *  ruler, already mapped through the current zoom/pan to viewport pixels
     *  and offset by the stage origin. POV-aware (Top X/Y, Front X/Z, Side Y/Z). */
    QVector<RulerTick> rulerTicks(bool horizontal) const;

    /** Axis short name for the current POV: horizontal → X or Y; vertical →
     *  Y or Z (height in elevation views). */
    QString axisName(bool horizontal) const;

    /** Unit suffix for readouts ("m" or "ft"), derived from the grid metric. */
    QString unitSuffix() const;

    /** Convert a viewport point to the display coordinate pair (horizontal,
     *  vertical) in the current grid units, relative to the stage origin. */
    QPointF viewportToReadout(const QPoint &vp) const;

    /** The world point (metres, stage coords) shown as 0,0 by the rulers. */
    QPointF stageOriginMetres() const;

    /** Set the stage origin (metres) and refresh the rulers/readout. */
    void setStageOriginMetres(const QPointF &metres);

    /** Enter "click to place 0,0" mode: the next left-click on the canvas sets
     *  the stage origin to that point instead of selecting. Top view only. */
    void beginPickOrigin();

    /** True while a beginPickOrigin() pick is armed. */
    bool pickingOrigin() const { return m_pickingOrigin; }

    /** Ask the rulers to repaint (grid metrics / origin / zoom changed). */
    void refreshRulers() { emit rulersChanged(); }

protected:
    /** Triggers the whole view repaint and metrics
     *  computation */
    void updateGrid();

    /** Event caught when the GraphicsView is resized */
    void resizeEvent(QResizeEvent *event) override;

    /** Scroll wheel zooms the view around the cursor. */
    void wheelEvent(QWheelEvent *event) override;

    /** Trackpad pinch-to-zoom (macOS native zoom gesture). */
    bool viewportEvent(QEvent *event) override;

    /** Shift+left-drag on empty canvas pans the view. */
    void mousePressEvent(QMouseEvent *event) override;

    /** Double-click detects fixture / truss and emits the appropriate signal. */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /** During fixture drags, highlights any truss under the cursor. */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** Right-click on empty canvas emits contextMenuRequested; on items routes to them. */
    void contextMenuEvent(QContextMenuEvent *event) override;

    /** Push the current snap configuration to a single fixture item */
    void applySnapToItem(MonitorFixtureItem *item);

    /** Push the current snap configuration to every fixture item */
    void applySnapToAllItems();

public:
    /** Undo the last fixture move (Cmd/Ctrl+Z). */
    void undoLastMove();

    /** Record a stage-feature paste on the undo stack so Ctrl+Z removes the
     *  copies (and any palettes created for pasted targets). IDs are the
     *  newly-created feature/palette IDs. */
    void recordFeaturePaste(const QList<quint32> &trussIds,
                            const QList<quint32> &platformIds,
                            const QList<quint32> &targetIds,
                            const QList<quint32> &paletteIds);

    /** Update a fixture item's real position (mm) and reposition it on the canvas.
     *  Call this after programmatically changing a fixture's truss offset. */
    void moveFixtureTo(quint32 fid, QPointF mmPos);

public slots:
    void mouseReleaseEvent(QMouseEvent *e) override;

protected slots:
    /** Slot called when a MonitorFixtureItem is dropped after a drag */
    void slotFixtureMoved(MonitorFixtureItem *item);

    /** Slot called when a TrussItem is dropped after a drag */
    void slotTrussMoved(TrussItem *item);

    /** Slot called when a PlatformItem is dropped after a drag */
    void slotPlatformMoved(PlatformItem *item);
    void slotBoomMoved(MonitorBoomItem *item);

    /** Slot called when a PowerSourceItem is dropped after a drag */
    void slotPowerSourceMoved(PowerSourceItem *item);
    void slotPowerSourceLockToggled(int sourceIndex);

    /** Slot called when a TargetItem is dropped after a drag */
    void slotTargetMoved(TargetItem *item);

    /** Slot called when a MonitorImageItem is dropped after a drag */
    void slotImageMoved(MonitorImageItem *item);

private:
    /** Resolve a (kind,id) descriptor to its canvas item, or nullptr. */
    QGraphicsItem *itemFor(const QString &kind, quint32 id) const;

    /** Read/write the map-group id (0 = ungrouped) of a heterogeneous item. */
    quint32 itemGroupId(QGraphicsItem *gi) const;
    void setItemGroupId(QGraphicsItem *gi, quint32 gid);

    /** All canvas items whose IMMEDIATE group is the given (non-zero) id. */
    QList<QGraphicsItem *> itemsInGroup(quint32 gid) const;

    /** All canvas items under @p top, following nested sub-groups. */
    QList<QGraphicsItem *> itemsUnderGroup(quint32 top) const;

    /** Walk parentGroupId up to the outermost group containing @p g. */
    quint32 topLevelGroup(quint32 g) const;

    /** True if group @p g is @p ancestor or nested somewhere beneath it. */
    bool groupIsUnder(quint32 g, quint32 ancestor) const;

    /** Read/write an item's layer (heterogeneous). */
    quint32 itemLayerId(QGraphicsItem *gi) const;
    void setItemLayer(QGraphicsItem *gi, quint32 layerId);

    /** Move a whole group subtree (the group, its descendant groups and all
     *  their items) onto @p layerId. */
    void setGroupSubtreeLayer(quint32 gid, quint32 layerId);

    /** Next unused map-group id (registry-authoritative once migrated). */
    quint32 nextMapGroupId() const;

    /** Count of structural items (trusses + platforms) in a group — used to
     *  decide whether an auto-group is dedicated (anchorable) or a manual mix. */
    int structuralMembersOf(quint32 gid) const;

    /** Extend the current selection to include every group-mate of any selected
     *  grouped item (the select-together behaviour). Re-entrancy guarded. */
    void extendSelectionToGroups();

    /** Scene-pixel Y of the floor (Z=0) line — bottom of the grid — used as the
     *  baseline for elevation (Front/Side) views. */
    qreal floorPixelY() const;

    /** Snap a scene-pixel coordinate to the current grid/subdivision. */
    QPointF snapScenePos(const QPointF &p) const;
    /** Snapshot the real positions of the items that are about to move. */
    void captureMoveUndo();

signals:
    /** Signal emitted after fixture point -> metrics conversion */
    void fixtureMoved(quint32 id, QPointF pos);

    /** Signal emitted when a platform is moved (new origin in metres). */
    void platformMoved(quint32 id, QPointF originMetres);

    /** Signal emitted when a target is moved (new XY position in metres). */
    void targetMoved(quint32 id, QPointF posMetres);

    /** Signal emitted when the user double-clicks a target item */
    void targetDoubleClicked(quint32 tid);

    /** Signal emitted when the graphics view is clicked */
    void viewClicked(QMouseEvent *e);

    /** Signal emitted when the user double-clicks a fixture item */
    void fixtureDoubleClicked(quint32 fid);

    /** Signal emitted when the user double-clicks a truss item */
    void trussDoubleClicked(quint32 tid);

    /** Emitted from the truss context menu — Monitor confirms + removes it. */
    void trussRemoveRequested(quint32 tid);

    /** Emitted from the truss context menu / strip — add a bar hung on this
     *  truss. @p offset < 0 means "use a sensible default (mid-span)". */
    void addBarToTrussRequested(quint32 parentTrussId, float offset);

    /** Emitted from the platform context menu — Monitor confirms + removes it. */
    void platformRemoveRequested(quint32 pid);

    /** Emitted when the user double-clicks / edits an image object. */
    void imageDoubleClicked(quint32 imageId);
    /** Emitted from the image context menu — Monitor confirms + removes it. */
    void imageRemoveRequested(quint32 imageId);

    /** Signal emitted when the user double-clicks a platform item */
    void platformDoubleClicked(quint32 pid);

    /** Signal emitted when the user right-clicks empty canvas space */
    void contextMenuRequested(QPointF scenePos);

    /** Emitted whenever the canvas selection changes, so the toolbar can update
     *  the enabled state of the Group / Ungroup actions. */
    void mapSelectionChanged();

    /** Emitted when the group structure changes (group / ungroup), so the
     *  Layers tree can rebuild its folders. */
    void mapStructureChanged();

    /** Signal re-emitted from TrussItem: add a fixture at offset metres on truss. */
    void addFixtureToTrussRequested(quint32 trussId, float offsetMetres);

    /** Emitted whenever the ruler mapping changes (zoom, pan, resize, POV,
     *  grid metrics, origin) so the ruler widgets can repaint. */
    void rulersChanged();

    /** Emitted on mouse-move over the canvas with the live readout coordinate
     *  (horizontal, vertical) in current grid units, relative to the origin. */
    void cursorReadout(QPointF hv);

    /** Emitted after the stage origin is set via a click-to-place pick. */
    void originPicked();

private:
    Doc *m_doc;
    QGraphicsScene *m_scene;

    /** Size of the grid. How many horizontal and vertical cells */
    QSize m_gridSize;

    /** Size of a grid cell in pixels */
    int m_cellPixels;

    /** X offset of the grid to keep it centered */
    qreal m_xOffset;

    /** Y offset of the grid to keep it centered */
    qreal m_yOffset;

    /** The unit used by the grid. Meters = 1000mm, Feet = 304.8mm */
    float m_unitValue;

    /** List of Fixture items represented graphically */
    QList <QGraphicsLineItem *> m_gridItems;

    /** Flag to enable/disable the grid rendering */
    bool m_gridEnabled;
    bool m_gridVisible = true;   ///< show/hide the grid LINES (geometry always computed)

    /** Number of sub-divisions drawn inside each grid cell */
    int m_gridSubdivisions;

    /** Flag indicating whether the layout is locked (fixtures not movable) */
    bool m_layoutLocked;

    /** Snap-to-grid level (0 = off, 1 = full, 2 = 1/2, 4 = 1/4) */
    int m_snapDivisions;

    /** Path to the view background image */
    QString m_backgroundImage;

    QPixmap m_bgPixmap;

    QGraphicsPixmapItem *m_bgItem;

    /** Map of the rendered MonitorFixtureItem with their ID */
    QHash <quint32, MonitorFixtureItem*> m_fixtures;

    /** Interactive truss items keyed by truss ID. */
    QHash <quint32, TrussItem*> m_trussItems;

    /** Interactive platform items keyed by platform ID. */
    QHash <quint32, PlatformItem*> m_platformItems;
    QHash <quint32, MonitorBoomItem*> m_boomItems;

    /** Power-source markers, indexed positionally to PowerDistribution::sources(). */
    QList <PowerSourceItem*> m_powerSourceItems;

    /** Interactive target marker items keyed by target ID. */
    QHash <quint32, TargetItem*> m_targetItems;

    /** Placeable image items keyed by image ID. */
    QHash <quint32, MonitorImageItem*> m_imageItems;

    /** Aim-line overlays (rebuilt on selection change). */
    QList <QGraphicsLineItem*> m_aimLines;

    /** FollowSpot current-position pin (circle + crosshair), created lazily. */
    QGraphicsEllipseItem    *m_fsPinCircle = nullptr;
    QGraphicsLineItem       *m_fsPinH      = nullptr;
    QGraphicsLineItem       *m_fsPinV      = nullptr;
    QGraphicsSimpleTextItem *m_fsPinLabel  = nullptr;  ///< "no joystick" warning

    /** Scene whose palette→target links are shown as aim lines.
     *  invalidId() means no scene is active → no lines drawn. */
    quint32 m_activeSceneId = Function::invalidId();
    /** Guards against re-queueing an aim-line rebuild that is already pending
        (a joystick tick emits functionChanged once per fixture per channel). */
    bool m_aimLinesUpdatePending = false;

    /** Truss IDs that were highlighted (cursor over them) at the moment of the
     *  last mouse release.  Populated in mouseReleaseEvent, consumed in
     *  slotFixtureMoved to bind fixtures to the truss the user visually dropped
     *  them onto — more reliable than comparing fixture-centre geometry. */
    QSet<quint32> m_droppedOnTrussIds;

    /** Tagged undo entry — covers fixture moves, truss moves and stage-feature
     *  pastes so that Ctrl+Z replays operations in the order they were
     *  performed. */
    struct UndoEntry {
        enum Type { FixtureMove, TrussMove, PlatformMove, TargetMove,
                    FeaturePaste } type;
        QHash<quint32, QPointF>   fixturePositions; ///< type == FixtureMove
        QHash<quint32, QVector3D> trussOrigins;     ///< type == TrussMove
        QHash<quint32, QPointF>   platformOrigins;  ///< type == PlatformMove (metres)
        QHash<quint32, QPointF>   targetPositions;  ///< type == TargetMove (metres)
        // type == FeaturePaste: IDs created by a paste, removed on undo.
        QList<quint32> pastedTrussIds;
        QList<quint32> pastedPlatformIds;
        QList<quint32> pastedTargetIds;
        QList<quint32> pastedPaletteIds;
    };
    QList<UndoEntry> m_moveUndo;

    /** Set by mouseDoubleClickEvent; suppresses the viewClicked emission on
     *  the paired mouseReleaseEvent so it doesn't immediately close the editor
     *  that the double-click just opened. */
    bool m_suppressNextViewClick = false;

    /** Selection snapshot taken at mousePressEvent time, before Qt's default
     *  handler clears the selection. Used by mouseDoubleClickEvent to restore
     *  a multi-selection that was active when the user double-clicked. */
    QList<QGraphicsItem *> m_savedSelection;

    /** Positions snapshot taken at move-start, committed to m_moveUndo on
     *  drop if anything actually moved. */
    QHash<quint32, QPointF> m_pendingMoveUndo;

    /** Guards extendSelectionToGroups() against the re-entrant selectionChanged
     *  it triggers while adding group-mates to the selection. */
    bool m_extendingSelection = false;

    /** Current point of view (Top = editable default). */
    ViewPOV m_pov = PovTop;

    /** True while an update*() method is deleting+recreating items. Guards
     *  extendSelectionToGroups from dynamic_cast-ing half-deleted items when
     *  QGraphicsScene::removeItem fires selectionChanged mid-teardown. */
    bool m_itemsRebuilding = false;

    /** True while a "click to place 0,0" pick is armed (beginPickOrigin). */
    bool m_pickingOrigin = false;

    /** "Stage view" — hide fixtures/targets/power, keep only stage features. */
    bool m_stageOnly = false;

    /** "Build/Rig focus" — ghost fixtures (faint, click-through), hide
     *  targets/power, so structure is the interaction target. */
    bool m_buildFocus = false;

    /** Topmost item at a scene point, skipping ghosted fixtures (so a Build-focus
     *  pick reaches the structure beneath a faint fixture). */
    QGraphicsItem *topPickableAt(const QPointF &scenePos) const;

    /** Origin offset (in current grid units) for the horizontal / vertical
     *  ruler axes, given the current POV. */
    qreal originUnitsH() const;
    qreal originUnitsV() const;
};

/** @} */

#endif // MONITORGRAPHICSVIEW_H
