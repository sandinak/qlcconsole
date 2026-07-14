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
class PowerSourceItem;
class TargetItem;
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

    /** Set the graphics view size in monitor units */
    void setGridSize(QSize size);

    /** Get the grid size in monitor units */
    QSize gridSize() const { return m_gridSize; }

    /** Set the number of sub-divisions drawn inside each grid cell */
    void setGridSubdivisions(int subdivisions);

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

    /** Return the MonitorFixtureItem for the given fixture ID, or nullptr if not in the view. */
    MonitorFixtureItem *fixtureItemForId(quint32 fxId) const;

    /** Set the gel color of the fixture with the given ID */
    void setFixtureGelColor(quint32 id, QColor col);

    /** Set the rotation degrees of the fixture with the given ID */
    void setFixtureRotation(quint32 id, ushort degrees);

    /** Show/hide fixtures items labels */
    void showFixturesLabels(bool visible);

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

    /** Inverse of realPositionToPixels — convert scene-pixel position to
     *  millimetres (same units as MonitorProperties fixture positions). */
    QPointF pixelsToRealPosition(qreal px, qreal py);

    /** Update the position and the scale of the fixture with
     *  the given ID
     */
    void updateFixture(quint32 id);

    /** Set a background image for the view */
    void setBackgroundImage(QString filename);

    /** Set a flat background color (pass an invalid QColor to clear). */
    void setBackgroundColor(const QColor &color);

    /** Retrieve the path to the background image currently set */
    QString backgroundImage() { return m_backgroundImage; }

    /** Rebuild the truss overlay items from MonitorProperties. */
    void updateTrusses();

    /** Rebuild the platform overlay items from MonitorProperties. */
    void updatePlatforms();

    /** Rebuild the power-source markers from the power-distribution model. */
    void updatePowerSources();

    /** Power view: tint each source marker with its circuits' colours (key
     *  "sourceIdx:circuitIdx"). Empty map restores the plain markers. */
    void setPowerSourceColors(const QHash<QString, QColor> &circuitColors);

    /** Rebuild the target marker items from MonitorProperties. */
    void updateTargets();

    /** Reposition existing TargetItems in place from their StageTarget data
     *  (no recreate) — used when the joystick moves an aim target live so the
     *  marker tracks without the churn of updateTargets(). */
    void repositionTargetItems();

    /** Rebuild the aim-line overlays for all selected TargetItems. */
    void updateAimLines();

    /** Show or hide the followspot beam-position pin on the floor map.
     *  @p xMeters, @p yMeters give the floor hit in metres (stage space).
     *  Pass visible=false to hide the pin. */
    void setFollowSpotPin(bool visible, float xMeters, float yMeters);

    /** Highlight (orange border) a set of fixtures by ID; clears previous highlights. */
    void highlightFixtures(const QList<quint32> &ids);

    /** Set the active scene whose target palette links are shown as aim lines.
     *  Pass Function::invalidId() to hide all aim lines. */
    void setActiveScene(quint32 sceneId);

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

    /** Slot called when a PowerSourceItem is dropped after a drag */
    void slotPowerSourceMoved(PowerSourceItem *item);

    /** Slot called when a TargetItem is dropped after a drag */
    void slotTargetMoved(TargetItem *item);

private:
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

    /** Signal emitted when the user double-clicks a platform item */
    void platformDoubleClicked(quint32 pid);

    /** Signal emitted when the user right-clicks empty canvas space */
    void contextMenuRequested(QPointF scenePos);

    /** Signal re-emitted from TrussItem: add a fixture at offset metres on truss. */
    void addFixtureToTrussRequested(quint32 trussId, float offsetMetres);

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

    /** Power-source markers, indexed positionally to PowerDistribution::sources(). */
    QList <PowerSourceItem*> m_powerSourceItems;

    /** Interactive target marker items keyed by target ID. */
    QHash <quint32, TargetItem*> m_targetItems;

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
};

/** @} */

#endif // MONITORGRAPHICSVIEW_H
