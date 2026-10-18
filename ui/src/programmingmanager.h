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
#include "function.h"
#include "powerdistribution.h"

class QTreeWidgetItem;
class Scene;
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
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
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
    friend class HelpShots;     // opens scenes for the help's pictures

public:
    ProgrammingManager(QWidget *parent, Doc *doc);
    ~ProgrammingManager();

    /** Select + host the function with the given id in the canvas, exactly as
     *  clicking it in the nav tree would (nests its subtree, previews it).
     *  Public entry for external navigation / automated screenshots. */
    void showFunction(quint32 fid);

    /** Open the power-distribution (circuits) editor dialog. Public entry so
     *  the app status-bar Power chip can reach it — the readout moved there,
     *  taking the old in-canvas "Circuits…" button with it. */
    void openCircuitsDialog();

    /** Match the main window's icon/text display preference
     *  ("workspace/tabLabelMode"). */
    void applyToolbarLabelMode();

    /** The palette the look editor is currently showing (invalidId() if
     *  none) — the ground truth for "what the user is looking at right
     *  now," independent of ProgrammerController's separate focused-scene/
     *  palette tracking (which only updates when a scene is freshly opened
     *  in this tab, and doesn't survive a relaunch). A control surface
     *  nudging a value the on-screen widget shows should follow this, not
     *  that tracking. */
    quint32 currentPaletteId() const;

    /** The scene currently shown in the canvas (Function::invalidId() if
     *  none, e.g. nothing open or a non-scene function is open) — lets a
     *  control surface enumerate "what should Select/Load(1-10) address
     *  right now" without its own scene-tracking. */
    quint32 currentSceneId() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    void slotFunctionActivated(QTreeWidgetItem *item);
    void slotFuncTreeMenu(const QPoint &pos);
    void slotPaletteTreeMenu(const QPoint &pos);
    void slotCanvasModified();
    void slotModeChanged();
    void slotLookEdited(quint32 paletteId);
    /** The look editor now shows @p paletteId: if the look on the canvas
     *  doesn't have it, try it on that look (Scene::setTrialPalette) so the
     *  rig follows the edit; say so, or say there is no look to show it on. */
    void updateTrialPalette(quint32 paletteId);
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

    /** General canvas undo (Ctrl+Z): revert the open scene one edit. Falls back
     *  to the bundle-stamp undo when there's no scene-edit history. */
    void slotUndo();

    // Power/amperage estimate (Design mode only)
    /** Re-estimate the load of the previewed look and refresh the footer. */
    void recomputePower();

    // Highlight
    void slotHighlightToggled(bool on);
    /** Momentarily flash the selected fixtures to identify them in the rig. */
    void slotFlashSelection();

    // Snapshot live DMX into the open scene (capture an external-console look)
    void slotSnapshotLive();

    // Park (hold fixtures out of cue output)
    void slotParkSelection();
    void slotUnparkAll();
    /** Refresh Park/Unpark button state from the ProgrammerController. */
    void slotParkChanged();

    // Mark (move-in-black: pre-position dark fixtures)
    void slotMarkSelection();
    void slotUnmarkAll();
    /** Refresh Mark/Unmark button state from the ProgrammerController. */
    void slotMarkChanged();

    // Followspot
    void slotFollowSpotBindX();
    void slotFollowSpotBindY();
    void slotFollowSpotClearBindings();
    void slotFollowSpotBindingChanged();
    void slotJoystickUpdated(float pan, float tilt);
    /** Dispatches controller button actions from ProgrammerController. */
    /** The full 'how a look is built' explanation. Shown once on a first
     *  run, and on demand after that. */
    void showProgrammingGuide();
public:
    /** The full 'how a look is built' explanation, from anywhere that needs
     *  a way back to it. */
    static void showGuide(QWidget *parent);
private:
    /** True exactly once, ever -- and records that it has answered. */
    static bool takeFirstRunGuideFlag();

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
    /** Enable/label the Park button (Park vs Unpark) from selection + park state. */
    void updateParkButtonLabel();
    void updateMarkButtonLabel();
    void loadCanvas(quint32 sceneId);
    /** Host the stock editor for a non-scene function in the canvas. */
    void loadFunctionEditor(Function *function);
    /** Create a new function of @p type in @p folder, name it, select it in the
     *  nav tree, and open it in the canvas. Shared by the func-tree's
     *  right-click "New …" menu and the nav toolbar's "Add" menu. No-op if
     *  @p type isn't one of the creatable types (Scene/Chaser/Collection/EFX/
     *  RGBMatrix/Show). */
    void addNewFunction(Function::Type type, const QString &folder);
    /** Host the fixture-group head-layout editor in the canvas (visualize a
     *  group double-clicked in the Fixtures & Groups source). */
    void loadGroupEditor(quint32 groupId);
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
    /** Select and scroll the source palette tree to @p paletteId (e.g. after
     *  creating a palette or auto-renaming an effect, so it's easy to find). */
    void revealPalette(quint32 paletteId);


    /** Duplicate palette $pid ("… (copy)"), add it, and open it for editing. */
    void duplicatePalette(quint32 pid);

    /** Folder path a func-tree row lives in (its own path, or the folder path if
     *  it is a folder row). Empty for null / root. */
    QString funcFolderPathFor(QTreeWidgetItem *item) const;
    /** Folder path of the current func-tree selection (for new functions). */
    QString selectedFuncFolderPath() const;
    /** How many chasers/collections reference this function. */
    int functionUsageCount(quint32 fid) const;

    /** Function ids the func-tree context menu should act on: the whole current
     *  selection if @p clicked is part of it, otherwise just @p clicked. Folder
     *  rows contribute no id. */
    QList<quint32> funcTreeTargetIds(QTreeWidgetItem *clicked) const;
    /** Same convention for the palette tree. */
    QList<quint32> paletteTreeTargetIds(QTreeWidgetItem *clicked) const;
    /** Open the Bundle editor pre-filled with @p paletteIds and save the result. */
    void saveBundleFromPalettes(const QList<quint32> &paletteIds, Scene *sourceScene = nullptr);
    /** Duplicate each function (smart-incremented name), select the last copy. */
    void duplicateFunctions(const QList<quint32> &fids);

    /** A function row shown nested under a collection / chaser / show track
     *  in the nav tree: which container it belongs to and where in it. */
    struct MemberRef
    {
        quint32 container = 0;   ///< Collection / Chaser / Show id
        quint32 fid = 0;         ///< the member function
        int stepIndex = -1;      ///< Collection member / Chaser step index
        quint32 trackId = 0;     ///< Show: the track
        quint32 showFuncId = 0;  ///< Show: the placed ShowFunction
    };

    /** Member rows among the rows a context-menu click on $clicked acts on
     *  (the selection if $clicked is in it). */
    QList<MemberRef> funcTreeMemberTargets(QTreeWidgetItem *clicked) const;

    /** Take the $refs out of their containers — the membership only, the
     *  functions stay. Asks first for chaser steps / show items (they lose
     *  their timing) unless $confirm is false. Returns how many were removed. */
    int removeMembers(const QList<MemberRef> &refs, bool confirm = true);

    /** The confirmation text for deleting palettes $pids: where each is used
     *  (scene looks, effect-look slots) and what deleting does to them. */
    QString deletePalettesMessage(const QList<quint32> &pids) const;

    /** Delete palettes $pids after asking (unless $confirm is false). */
    bool deletePalettes(const QList<quint32> &pids, bool confirm = true);

    /** Delete/Backspace on a tree: the selection's functions or palettes, or
     *  the selected folder. */
    void deleteTreeSelection(FunctionsTreeWidget *tree);

    /** The confirmation text for deleting $fids (says it is the function
     *  itself, everywhere, and where else each one is used). */
    QString deleteFunctionsMessage(const QList<quint32> &fids) const;
    /** Delete each function after confirmation. Collects by id first, since the
     *  first deletion rebuilds the tree and invalidates every item pointer. */
    void deleteFunctions(const QList<quint32> &fids);

    /** Delete the user folder $item of $tree. Its functions/palettes and
     *  sub-folders move up into the parent folder — nothing is deleted. A
     *  folder with something in it asks first (unless $confirm is false).
     *  Returns true if the folder was removed. */
    bool deleteTreeFolder(FunctionsTreeWidget *tree, QTreeWidgetItem *item,
                          bool confirm = true);

    /** Add @p fids to collection @p containerId (skipping self / duplicates /
     *  cycles) and refresh the nav tree + canvas editor if they show it. Called
     *  when functions are dropped onto a collection node in the nav tree. */
    void addFunctionsToCollection(quint32 containerId, const QList<quint32> &fids);

    /** Show the members/steps of the given collection/chaser nested under
     *  its node in the function tree (read-only nav; invalidId clears). */
    void syncMemberNodes(quint32 containerId);
    /** Recursively nest the members of a container function under treeNode
     *  (collections-in-chasers-in-…, and shows as tracks→functions); visited
     *  guards against cycles. */
    void addMemberChildren(QTreeWidgetItem *treeNode, quint32 containerId,
                           QSet<quint32> &visited, int depth);
    /** Create one nested member leaf (with optional zero-padded order prefix)
     *  under parentNode and recurse into it. Shared by chaser/collection member
     *  lists and show-track placements. */
    QTreeWidgetItem *addMemberLeaf(QTreeWidgetItem *parentNode, quint32 mid,
                                   int orderIdx, int width, bool ordered,
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
    QToolButton *m_addFuncBtn; //!< "Add" dropdown consolidating New Scene/Chaser/…/Folder

    QVBoxLayout *m_canvasLayout;
    /** The canvas over the editor of what's selected, a handle between. */
    class QSplitter *m_canvasSplit = nullptr;
    class QSplitter *m_mainSplit = nullptr;
    QVBoxLayout *m_canvasTopLayout = nullptr;   //!< the canvas / function editor
    QWidget *m_editorPane = nullptr;            //!< look editor / fixture console
    QVBoxLayout *m_editorLayout = nullptr;
    /** Show the editor pane only while one of its editors shows. */
    void syncEditorPane();
    QLabel *m_canvasTitle;
    /** In a fixture group's layout view: back to the scene that was open
     *  before it ("clicked back on an effect .. I can't get back"). */
    class QPushButton *m_groupBackButton = nullptr;
    quint32 m_groupReturnScene = UINT_MAX;
    QWidget *m_canvasPlaceholder;   ///< empty-state: one line plus a way in
    QLabel  *m_canvasHint;          ///< the one line
    SceneGroupLooks *m_canvas;
    QWidget *m_funcEditor;     //!< stock editor for non-scene functions
    LookEditor *m_lookEditor;      //!< bottom pane; sizes to content (no scroll)
    QScrollArea *m_fixtureScroll;  //!< wraps the per-fixture console (bottom)
    FixtureConsole *m_fixtureConsole;
    QLabel *m_fixtureMixedNote;    //!< shown when selected fixed fixtures differ in type
    QList<quint32> m_selectedFixtures; //!< fixed fixtures edited together (same type)
    quint32 m_selectedFixturesScene = UINT_MAX; //!< the scene m_selectedFixtures was announced for
    quint32 m_currentScene;       //!< scene shown in canvas (invalid if non-scene)
    quint32 m_canvasFunction;     //!< any function shown in canvas (preview target)
    quint32 m_previewFunction;    //!< the function we started for live preview
    quint32 m_trialScene;         //!< scene a palette is being tried on (updateTrialPalette)
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
    // Per-look fade overrides (paletteId -> {fadeInMs, fadeOutMs}) at stamp time,
    // so undo restores them (stamping removePalette()s them away).
    QHash<quint32, QPair<int, int> > m_stampUndoFades;
    quint32              m_stampUndoSceneId = quint32(-1);

    // General per-scene undo: detached Scene copies of the pre-edit state.
    // m_undoBaseline holds the state as of the last load/undo; on each canvas
    // edit it is pushed to m_undoStack and re-captured. Scene-canvas only.
    QList<Scene*>        m_undoStack;
    Scene               *m_undoBaseline = nullptr;
    quint32              m_undoSceneId = quint32(-1);
    /** Snapshot @p scene's full state into a detached Scene (caller owns it). */
    Scene *snapshotScene(Scene *scene);
    /** Reset the undo history for a newly loaded scene (id invalid clears it). */
    void resetUndoFor(quint32 sceneId);
    /** Record the pre-edit baseline before the current edit takes effect. */
    void pushUndoSnapshot();

    // Last Effect-palette signature synced to ESR — used to avoid recreating
    // instances on every refreshPreview() call when only non-Effect palettes (or
    // a running effect's own params) changed. The signature includes each Effect
    // palette's id AND its script path + preset, so SWITCHING the effect script
    // on an existing palette (same id) still forces a re-sync; a param-slider
    // drag (same script) does not, preserving live JS state.
    QStringList          m_lastSyncedEffectPalettes;

    /** Build the Effect-palette sync signature for @p scene (id|script|preset per
     *  Effect palette). Empty scene / non-scene → empty list. */
    QStringList effectPaletteSignature(Scene *scene) const;

    // Toolbar buttons
    QPushButton *m_highlightBtn;
    QPushButton *m_flashBtn    = nullptr;
    QPushButton *m_parkBtn     = nullptr;
    QPushButton *m_unparkBtn   = nullptr;
    QPushButton *m_markBtn     = nullptr;
    QPushButton *m_unmarkBtn   = nullptr;
    QPushButton *m_autoMibBtn  = nullptr;
    QDoubleSpinBox *m_mibGapSpin = nullptr;
    QPushButton *m_snapshotBtn = nullptr;

    // BPM / internal beat generator
    QPushButton  *m_bpmBtn;
    QSpinBox     *m_bpmSpin;
    QPushButton  *m_tapBtn;

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

    // Power/amperage estimate (Design mode only). The readout lives in the
    // app status-bar chip now (powerEstimateChanged); the timer here just
    // drives the periodic re-estimate, stopped whenever the tab is hidden or
    // the show goes live, so Run mode pays nothing.
    QTimer      *m_powerTimer = nullptr;

public:
    /** The chip's tooltip: the biggest draws by fixture type, where each
     *  figure comes from, and what the estimate assumes. */
    static QString powerDetailText(const QList<PowerEstimator::TypeLoad> &rows,
                                   double amps, double kw, double defaultVoltage);
private:
    /** Start the periodic re-estimate timer. */
    void initPowerTimer();
    /** Start/stop the periodic re-estimate per mode; clears the status-bar
     *  chip on the way into Operate. */
    void updatePowerEstimateActive();

signals:
    /** Emitted when the user confirms Save in the Programming tab. */
    void requestSave();
    /** Re-emits LookEditor::lookFocusChanged — the on-screen look changed
     *  (or was redrawn in place). Lets a control-surface overlay recompute
     *  what's "in use right now" (e.g. LED highlighting) without polling. */
    void currentPaletteIdChanged(quint32 paletteId);
    /** Live power estimate for the app status-bar chip (Design mode). Amps, kW,
     *  whether any circuit/source is overloaded, and what it is made of (the
     *  biggest draws and what the figure assumes -- the chip's tooltip). */
    void powerEstimateChanged(double amps, double kw, bool overload, const QString &detail);
};

/** @} */

#endif
