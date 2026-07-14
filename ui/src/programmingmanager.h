/*
  Q Light Controller Plus
  programmingmanager.h

  Fork-owned top-level "Programming" tab: a drag-drop authoring surface.
  Three panes:
    - left:   a function tree (folders) of what you build; select a scene
              to edit it (chaser/collection canvases come later)
    - center: the selected scene's canvas (reuses SceneGroupLooks)
    - right:  a Palettes tree + a Fixtures & Groups tree, dragged into the
              canvas (palette -> look, group -> dynamic target, fixture ->
              fixed target)

  It edits plain Scenes, so everything stays compatible with the classic
  Function Manager / scene editor.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMINGMANAGER_H
#define PROGRAMMINGMANAGER_H

#include <QWidget>
#include <QSet>
#include <QElapsedTimer>

class QTreeWidgetItem;
class Function;
class FunctionsTreeWidget;
class FixtureGroupSource;
class Fixture;
class SceneGroupLooks;
class LookEditor;
class FixtureConsole;
class BundleBrowser;
class BundleCache;
class QScrollArea;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QWidget;
class QFrame;
class QTimer;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class ProgrammingManager final : public QWidget
{
    Q_OBJECT

public:
    ProgrammingManager(QWidget *parent, Doc *doc);
    ~ProgrammingManager();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotFunctionActivated(QTreeWidgetItem *item);
    void slotFuncTreeMenu(const QPoint &pos);
    void slotPaletteTreeMenu(const QPoint &pos);
    void slotCanvasModified();
    void slotModeChanged();
    void slotLookEdited();
    void slotCopy();
    void slotPaste();
    void slotPaletteDoubleClicked(QTreeWidgetItem *item);
    void slotFixturesSelected(const QList<quint32> &fixtureIds);
    void slotFixGroupSourceSelectionChanged();
    void slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value);
    void slotFixtureChecked(quint32 fxi, quint32 ch, bool state);

    // Bundle CRUD
    void slotStampBundle(const QString &bundleName);
    void slotSaveAsBundle();
    void slotUndoStamp();

    // Power/amperage estimate (Design mode only)
    /** Re-estimate the load of the previewed look and refresh the footer. */
    void recomputePower();
    /** Open the power-distribution (circuits) editor dialog. */
    void slotOpenCircuits();

    // Highlight
    void slotHighlightToggled(bool on);

    // Followspot
    void slotFollowSpotBindX();
    void slotFollowSpotBindY();
    void slotFollowSpotClearBindings();
    void slotFollowSpotBindingChanged();
    void slotJoystickUpdated(float pan, float tilt);
    /** Dispatches controller button actions from ProgrammerController. */
    void slotButtonAction(const QString &action);

    // BPM / internal beat generator
    void slotBpmToggled(bool on);
    void slotBpmChanged(int bpm);
    void slotTapBeat();

    // Design-mode joystick save
    void slotDesignPositionWritten();
    void slotSavePositions();
    void slotFollowSpotPinChanged(bool visible, float xMeters, float yMeters);

private:
    void loadCanvas(quint32 sceneId);
    /** Host the stock editor for a non-scene function in the canvas. */
    void loadFunctionEditor(Function *function);
    /** Tear down whichever canvas editor is currently shown. */
    void clearEditors();
    void updateTitle();

    /** True if two fixtures share a channel layout (same def+mode, or both
     *  generic with equal channel counts) and so can be edited together. */
    bool sameFixtureType(const Fixture *a, const Fixture *b) const;

    /** Show the look editor in the bottom panel (pivoting away from the
     *  per-fixture DMX console if it's showing). */
    void showLookEditorPanel();

    /** Create a new palette of the given QLCPalette::PaletteType in the
     *  selected palette folder and open it in the look editor. */
    void createPalette(int paletteType);

    /** Move Effect palettes still on the bare default folder into
     *  Palettes/Effect/<Category>/<Engine>/ so the tree mirrors the picker.
     *  Idempotent; leaves user-chosen folders alone. Cosmetic (no setModified).
     *  Returns true if any path changed. */
    bool organizeEffectPalettes();

    /** Duplicate palette $pid ("… (copy)"), add it, and open it for editing. */
    void duplicatePalette(quint32 pid);

    /** Strip leading "NN. " step-number prefixes accidentally baked into
     *  function names by an earlier nesting bug. */
    void repairFunctionNames();

    /** Folder path of the current func-tree selection (for new functions). */
    QString selectedFuncFolderPath() const;
    /** How many chasers/collections reference this function. */
    int functionUsageCount(quint32 fid) const;

    /** Show the members/steps of the given collection/chaser nested under
     *  its node in the function tree (read-only nav; invalidId clears). */
    void syncMemberNodes(quint32 containerId);
    /** Recursively nest the members of a container function under treeNode
     *  (collections-in-chasers-in-… ); visited guards against cycles. */
    void addMemberChildren(QTreeWidgetItem *treeNode, quint32 containerId,
                           QSet<quint32> &visited, int depth);

    /** Is fid anywhere in the container's tree (recursively)? */
    bool isContainerMember(quint32 containerId, quint32 fid) const;
    bool containerHas(quint32 containerId, quint32 fid,
                      QSet<quint32> &visited, int depth) const;

    /** Live-preview the canvas scene by running it (Design mode only), so
     *  the DMX/2D view reflects the look as you build. Blind/live handling
     *  comes later. */
    void startPreview();
    void stopPreview();
    /** In Operate mode, run the canvas scene directly (all channels live)
     *  so lights stay on when the show mode is active. A followspot.js effect
     *  look overrides pan/tilt on joystick moves on top. */
    void startOperateScene();
    void stopOperateScene();
    /** Re-apply the running preview after an edit WITHOUT churning
     *  start/stop (resetRuntime if running; start if not). */
    void refreshPreview();

private:
    Doc *m_doc;

    FunctionsTreeWidget *m_funcTree;

    QVBoxLayout *m_canvasLayout;
    QLabel *m_canvasTitle;
    QLabel *m_canvasPlaceholder;
    SceneGroupLooks *m_canvas;
    QWidget *m_funcEditor;     //!< stock editor for non-scene functions
    LookEditor *m_lookEditor;      //!< bottom pane; sizes to content (no scroll)
    QScrollArea *m_fixtureScroll;  //!< wraps the per-fixture console (bottom)
    FixtureConsole *m_fixtureConsole;
    QLabel *m_fixtureMixedNote;    //!< shown when selected fixed fixtures differ in type
    QList<quint32> m_selectedFixtures; //!< fixed fixtures edited together (same type)
    quint32 m_currentScene;       //!< scene shown in canvas (invalid if non-scene)
    quint32 m_canvasFunction;     //!< any function shown in canvas (preview target)
    quint32 m_previewFunction;    //!< the function we started for live preview
    quint32 m_operateFunction;   //!< scene we started in Operate mode (all channels live)
    quint32 m_clipboardFunction;  //!< Cmd-C source for Cmd-V duplicate
    quint32 m_clipboardPalette;   //!< Cmd-C source palette for Cmd-V duplicate
    quint32 m_memberContainer;    //!< collection/chaser whose members are nested

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource  *m_fixGroupSource;
    BundleBrowser       *m_bundleBrowser = nullptr;
    BundleCache         *m_bundleCache   = nullptr;

    // Bundle stamp undo (one-level: save prior palette list, restore on Ctrl+Z)
    QList<quint32>       m_stampUndoPalettes;
    quint32              m_stampUndoSceneId = quint32(-1);

    // Last Effect-palette set synced to ESR — used to avoid recreating instances
    // on every refreshPreview() call when only non-Effect palettes changed.
    QList<quint32>       m_lastSyncedEffectPalettes;

    // Toolbar buttons
    QPushButton *m_highlightBtn;

    // BPM / internal beat generator
    QPushButton  *m_bpmBtn;
    QSpinBox     *m_bpmSpin;
    QPushButton  *m_tapBtn;
    QElapsedTimer m_tapTimer;
    bool          m_tapActive = false;

    // Followspot config panel (kept for signal wiring; always hidden from UI)
    QWidget     *m_followSpotPanel;
    QPushButton *m_fsBindXBtn;
    QPushButton *m_fsBindYBtn;
    QPushButton *m_fsClearBtn;
    QLabel      *m_fsXLabel;
    QLabel      *m_fsYLabel;
    QLabel      *m_fsJoyLabel;  // live pan/tilt readout

    // Joystick speed spinner
    QSpinBox    *m_speedSpin = nullptr;

    // Save button (armed two-press confirmation)
    QPushButton *m_saveBtn = nullptr;
    bool         m_saveArmed = false;

    // Power/amperage footer (Design mode only). Hidden in Operate; the timer
    // is stopped whenever the tab is hidden or the show goes live, so Run mode
    // pays nothing.
    QFrame      *m_powerFooter = nullptr;
    QLabel      *m_powerAmpsLabel = nullptr;
    QLabel      *m_powerKwLabel = nullptr;
    QLabel      *m_powerOverloadLabel = nullptr;
    QPushButton *m_circuitsBtn = nullptr;
    QTimer      *m_powerTimer = nullptr;

    /** Build the footer widget and append it to the canvas layout. */
    void buildPowerFooter();
    /** Start/stop the periodic re-estimate and footer visibility per mode. */
    void updatePowerFooterActive();

signals:
    /** Emitted when the user confirms Save in the Programming tab. */
    void requestSave();
};

/** @} */

#endif
