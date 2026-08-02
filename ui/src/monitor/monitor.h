/*
  Q Light Controller
  monitor.h

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

#ifndef MONITOR_H
#define MONITOR_H

#include <QWidget>
#include <QHash>
#include <QList>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QString>

#include "monitorproperties.h"

class MonitorGraphicsView;
class MonitorLayersPanel;
class MonitorFixture;
class MonitorLayout;
class QScrollArea;
class QComboBox;
class QSplitter;
class QToolBar;
class QToolButton;
class QSpinBox;
class QAction;
class QLabel;
class Fixture;
class Monitor;
class QTimer;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class Monitor final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(Monitor)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** Get the monitor singleton instance. Can be NULL. */
    static Monitor* instance();

    /** Create or show Monitor */
    static void createAndShow(QWidget* parent, Doc* doc);

    /** Apply the workspace tab-label mode (icons only / text only / text under
     *  icon) to the monitor's own toolbars, so this window matches the main
     *  window. Reads the same "workspace/tabLabelMode" setting; called at
     *  toolbar build and live from App when the preference changes. */
    void applyToolbarLabelMode();

    /** Normal public destructor */
    ~Monitor();

    /** Refresh fixtures & show current view */
    void updateView();

    /** Highlight the given fixtures (orange border) in the 2D graphics view.
     *  Passing an empty list clears all highlights. */
    void highlightFixtures(const QList<quint32> &ids);

    /** Set the active scene context for aim-line rendering.
     *  Pass Function::invalidId() to hide all aim lines. */
    void setActiveScene(quint32 sceneId);

    /** Show or move the followspot current-position pin on the 2D floor map.
     *  visible=false hides the pin. xMeters/yMeters are in stage space. */
    void setFollowSpotPin(bool visible, float xMeters, float yMeters);
protected:
    /** Initialize the monitor view */
    void initView();

    /** Initialize the monitor view in DMX mode */
    void initDMXView();

    /** Initialize settings & fill fixtures */
    void fillDMXView();

    /** show */
    void showDMXView();

    /** Initialize the monitor view in 2D graphics mode */
    void initGraphicsView();

    /** Track the cursor over the canvas viewport to drive the ruler markers. */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /** Initialize settings & fill fixtures */
    void fillGraphicsView();

    /** show */
    void showGraphicsView();

    void showCurrentView();
protected:
    void saveSettings();

    /** Protected constructor to prevent multiple instances. */
    Monitor(QWidget* parent, Doc* doc, Qt::WindowFlags f = Qt::Widget);

protected:
    /** The singleton Monitor instance */
    static Monitor* s_instance;
    Doc* m_doc;
    MonitorProperties *m_props;

    /*********************************************************************
     * Running functions
     *********************************************************************/
protected slots:
    void slotFunctionStarted(quint32 id);

    /*********************************************************************
     * Menu
     *********************************************************************/
protected:

    /** Create DMX view toolbar */
    void initDMXToolbar();

    void initGraphicsToolbar();

protected slots:
    /** Menu action slot for font selection */
    void slotChooseFont();

    /** Menu action slot for channel style selection */
    void slotChannelStyleTriggered();

    /** Menu action slot for value style selection */
    void slotValueStyleTriggered();

    /** Menu action slot to trigger display mode switch */
    void slotSwitchMode();

    /********************************************************************
     * Monitor Fixtures
     ********************************************************************/
public:
    /** Update monitor fixture labels */
    void updateFixtureLabelStyles();

protected:
    /** Create a new MonitorFixture* and append it to the layout */
    void createMonitorFixture(Fixture* fxi);

protected slots:
    /** Slot for fixture additions (to append the new fixture to layout) */
    void slotFixtureAdded(quint32 fxi_id);

    /** Slot for fixture contents & layout changes */
    void slotFixtureChanged(quint32 fxi_id);

    /** Slot for fixture removals (to remove the fixture from layout) */
    void slotFixtureRemoved(quint32 fxi_id);

    /** Slot called when a universe combo item is selected */
    void slotUniverseSelected(int index);

signals:
    void channelStyleChanged(MonitorProperties::ChannelStyle style);
    void valueStyleChanged(MonitorProperties::ValueStyle style);
    /** Emitted when the user clicks a fixture (or empty space) in the 2D view.
     *  @p fixtureIds is the current graphics-view selection (may be empty). */
    void fixturesSelected(QList<quint32> fixtureIds);

protected:
    QToolBar* m_DMXToolBar;
    QScrollArea* m_scrollArea;
    QWidget* m_monitorWidget;
    MonitorLayout* m_monitorLayout;
    QList <MonitorFixture*> m_monitorFixtures;
    quint32 m_currentUniverse;

    /********************************************************************
     * Graphics View
     ********************************************************************/

    /** Open the fixture properties / test dialog for a specific fixture ID.
     *  Can be called from outside the Monitor (e.g. SimpleDesk, Fixture Manager).
     *  The Monitor must already be constructed; call Monitor::createAndShow() first. */
public:
    void showFixturePropertiesById(quint32 fxId);

protected:
    /** Dismiss any open fixture property editor (no-op with popup design). */
    void hideFixtureItemEditor();

    /** Open a modal fixture properties dialog for the current selection. */
    /** @param onlyFid if non-zero, edit JUST that fixture (studio single path,
     *  bypassing the map selection / group select-together); else edit the
     *  current map selection. */
    void showFixtureItemEditor(quint32 onlyFid = 0);

protected slots:
    /** Slot called when the grid width changes */
    void slotGridWidthChanged(int value);

    /** Slot called when the grid height changes */
    void slotGridHeightChanged(int value);

    /** Slot called when the unit metrics changes */
    void slotGridUnitsChanged(int index);

    /** Slot called when the grid sub-divisions value changes */
    void slotGridSubdivisionsChanged(int value);

    /** Slot called when the snap-to-grid level changes */
    void slotSnapChanged(int index);

    /** Slot called when the 2D-map view overlay changes (Normal/Power/...) */
    void slotMapViewChanged(int index);

    /** Slot called when the layout lock state is toggled */
    void slotLockToggled(bool locked);

    /** Slot called when the Layers-panel toolbar toggle is clicked */
    void slotToggleLayersPanel(bool show);

    /** Slot called when the point-of-view combo (Top/Front/Side) changes */
    void slotPOVChanged(int index);

    /** Group / ungroup the current 2D-map selection. */
    void slotGroupItems();
    void slotUngroupItems();

    /** Enable/disable the Group/Ungroup actions for the current selection. */
    void slotMapSelectionChanged();

    /** Update the Plot/Locked toggle's icon, label and tooltip for the given
     *  state. "Edit Plot" (unlocked) vs "Plot Locked" (frozen for the show). */
    void updatePlotLockAppearance(bool locked);

    /** Slot called when the user wants to add
     *  a fixture to the graphics view */
    void slotAddFixture();

    /** Slot called when the user wants to remove
     *  a fixture from the graphics view */
    void slotRemoveFixture();

    /** Slot called when the user wants to set
     *  a background picture on the graphics view */
    void slotSetBackground();

    /** Add a new truss interactively and refresh the canvas. */
    void slotAddTruss();

    /** Open the truss-management dialog (edit / remove existing trusses). */
    void slotManageTrusses();

    /** Open the truss-edit form for a specific truss ID (from double-click). */
    void slotEditTruss(quint32 tid);

    /** Create a cross-bar hung on @p parentId at @p offset (metres; <0 = mid-
     *  span), attached + grouped. Returns the new bar's id (invalid on failure).
     *  Does NOT redraw/open an editor — callers decide. */
    quint32 createBarOnTruss(quint32 parentId, float offset);

    /** Remove whichever fixture or truss is currently selected in the view. */
    void slotRemoveSelected();

    /** Open the background color picker. */
    void slotSetBackgroundColor();

    /** Show the fixture properties editor on double-click. */
    void slotFixtureDoubleClicked(quint32 fid);
    /** Edit ONE fixture's properties (studio single-fixture path). */
    void editFixtureProperties(quint32 fid);

    /** Open truss edit dialog on double-click. */
    void slotTrussDoubleClicked(quint32 tid);
    void slotTrussRemoveRequested(quint32 tid);
    void slotAddBarToTruss(quint32 parentId, float offset);
    void slotAddBarToPipe(quint32 parentPipeId);
    void slotPlatformRemoveRequested(quint32 pid);
    void slotAddImage();
    void slotEditImage(quint32 id);
    void slotImageRemoveRequested(quint32 id);

    /** Show the Add context menu at the given scene position. */
    void slotCanvasContextMenu(QPointF scenePos);

    /** Add a fixture onto a truss at the given offset (from TrussItem right-click). */
    void slotAddFixtureToTruss(quint32 trussId, float offsetMetres);

    /** Add a new platform and open its editor. */
    void slotAddPlatform();
    void slotAddPipe();
    void slotAddElectric();
    void slotAddStand();
    void slotEditStand(quint32 sid);
    void slotAddTower();
    void slotEditTower(quint32 tid);

    /** Build the embedded graphical structure canvas pane (plane switcher + the
     *  StructureStudioView) for the object editors. kind: 0=Stand,1=Tower,2=Truss.
     *  If @p outView is non-null it receives the view so the caller can reload()
     *  it after a live geometry edit. */
    /** The canvas-centric object-editor body: a splitter of
     *  [ Geometry (collapsible) + fixtures-by-group tree | canvas | inspector ].
     *  @p geometryForm is the per-kind geometry widget (may be null). */
    class QWidget *makeStudioPane(class QDialog *dlg, int kind, quint32 id,
                                  class QWidget *geometryForm,
                                  class StructureStudioView **outView);

    /** Structure-studio actions: mount fixtures / a whole fixture group onto the
     *  structure, or add a bar. kind: 0=Stand,1=Tower,2=Truss,3=Platform,4=Pipe. */
    void studioAddFixture(int kind, quint32 id, class StructureStudioView *view);
    void studioAddGroup(int kind, quint32 id, class StructureStudioView *view);
    void studioAddBar(int kind, quint32 id, class StructureStudioView *view);
    /** Mount one fixture on the structure (clears its other mounts). */
    void mountFixtureOnStructure(quint32 fid, int kind, quint32 id, quint32 boomId);
    /** The pipe id to hang fixtures on for a stand/pipe kind (invalid if none). */
    quint32 structureBoomId(int kind, quint32 id) const;
    /** Create a NEW fixture group from the given fixtures (prompts for a name). */
    void studioCreateGroup(const QList<quint32> &fids, class StructureStudioView *view);
    /** Open the Fixtures-tab head-layout grid editor for a fixture group. */
    void openGroupLayout(quint32 groupId);
    /** Open the unified studio editor for a studio (frame) group — replaces the
     *  old standalone Studio Group window. */
    void openGroupStudio(quint32 groupId);
    /** Evenly space @p fids (or all on-object fixtures) along the current face. */
    void studioDistribute(int kind, quint32 id, class StructureStudioView *view,
                          const QList<quint32> &fids);

    /** Open the platform edit dialog for the given platform ID. */
    void slotEditPlatform(quint32 pid);
    void slotEditPipe(quint32 bid);

    /** Open platform edit on double-click. */
    void slotPlatformDoubleClicked(quint32 pid);

    /** Add a new stage target and open its editor. */
    void slotAddTarget();

    /** Open the target edit dialog for the given target ID. */
    void slotEditTarget(quint32 tid);

    /** Open target edit on double-click. */
    void slotTargetDoubleClicked(quint32 tid);

    /** Copy the currently selected stage features (trusses / platforms /
     *  targets) into the internal clipboard. */
    void slotCopySelected();

    /** Paste the clipboard's stage features back onto the canvas, offset from
     *  their originals so the copies are immediately visible and draggable. */
    void slotPasteFeatures();

    /** Slot called when the user wants to show
     *  or hide fixtures labels */
    void slotShowLabels(bool visible);

    /** Slot called when a fixture is moved in the graphics view */
    void slotFixtureMoved(quint32 fid, QPointF pos);

    /** Slot called when the graphics view is clicked */
    void slotViewClicked();

protected:
    QToolBar* m_graphicsToolBar;
    /** Custom toolbar buttons (added via addWidget, so the toolbar's button
     *  style doesn't reach them) — restyled explicitly in applyToolbarLabelMode(). */
    QToolButton* m_addBtn = nullptr;
    QToolButton* m_moreBtn = nullptr;
    QSplitter* m_splitter;
    /** Scene-space position of the last canvas right-click; used to place new
     *  fixtures/trusses at the cursor location. */
    QPointF m_pendingAddScenePos;
    MonitorGraphicsView* m_graphicsView;
    MonitorLayersPanel* m_layersPanel;
    QSpinBox* m_gridWSpin;
    QSpinBox *m_gridHSpin;
    QComboBox *m_unitsCombo;
    QSpinBox *m_gridSubdivSpin;
    QComboBox *m_snapCombo;
    QToolButton *m_snapToggle = nullptr;
    int m_mapView = 0;       ///< current 2D-map overlay (0=Normal/1=Power/2=Stage; was m_viewCombo)
    QComboBox *m_povCombo;   ///< Top / Front / Side point of view
    QComboBox *m_overlayCombo = nullptr;   ///< Normal / Power / DMX / Network / Stage
    QComboBox *m_dmxViewCombo = nullptr;   ///< View selector shown in the DMX-grid toolbar

    /* Footer measurement bar (built in initGraphicsFooter). */
    class MonitorRuler *m_hRuler = nullptr;  ///< top ruler
    class MonitorRuler *m_vRuler = nullptr;  ///< left ruler
    QWidget   *m_rulerCorner = nullptr;      ///< top-left corner filler
    QLabel    *m_readoutLabel = nullptr;     ///< live X/Y coordinate readout
    QLabel    *m_modeLabel = nullptr;        ///< prominent current-mode chip
    /** Refresh the footer mode chip from the POV / overlay / Build state. */
    void updateModeIndicator();
    QAction   *m_rulersAction = nullptr;     ///< show/hide rulers toggle
    /** Build the footer bar (grid/units/snap controls + rulers toggle + origin
     *  + live readout) and the ruler widgets around the graphics view. */
    void initGraphicsFooter(QWidget *gcontainer, QWidget *viewArea);
    /** Show/hide the ruler strips (persisted). */
    void setRulersVisible(bool on);

    /** 2D-map view overlays (see slotMapViewChanged). */
    enum MapView { ViewNormal = 0, ViewPower = 1, ViewStage = 2, ViewDMX = 3, ViewNet = 4 };
    /** Apply a view overlay: tint/annotate every placed fixture. */
    void applyMapView(int view);
    QAction *m_labelsAction;
    QAction *m_buildAction;
    QAction *m_lockAction;
    QAction *m_layersAction;
    QAction *m_groupAction;
    QAction *m_ungroupAction;
    QAction *m_addPlatformAction;
    QAction *m_pasteAction;

private:
    /** Value snapshots of stage features for the copy/paste clipboard. The
     *  engine models are owned by MonitorProperties and may be deleted, so we
     *  store geometry/appearance by value rather than holding pointers. */
    struct TrussClip
    {
        QString   name;
        int       type;        ///< Truss::TrussType
        QVector3D origin;       ///< metres
        QPointF   direction;    ///< normalised XY
        float     length;       ///< metres
        float     width;        ///< metres
        int       profile;      ///< Truss::Profile
        bool      locked;
    };
    struct PlatformClip
    {
        QString name;
        float   originX, originY;   ///< metres
        float   width, depth, height;
        QColor  color;
    };
    struct TargetClip
    {
        QString name;
        float   x, y, z;            ///< metres
        QColor  color;
    };

    QList<TrussClip>    m_trussClipboard;
    QList<PlatformClip> m_platformClipboard;
    QList<TargetClip>   m_targetClipboard;
    /** How many times the current clipboard has been pasted; each paste
     *  cascades a little further so repeats don't stack exactly. Reset on copy. */
    int                 m_pasteCount = 0;

    /** True when there is anything to paste. */
    bool clipboardHasFeatures() const;

    /** Re-create the clipboard features, translating each by (dxM, dyM) metres.
     *  Shared by the keyboard/toolbar (cascading offset) and "Paste here"
     *  (cursor-anchored) paths. */
    void pasteClipboard(float dxM, float dyM);
};

/** @} */

#endif
