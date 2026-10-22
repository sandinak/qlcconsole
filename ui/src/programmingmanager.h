/*
  Q Light Controller Plus
  programmingmanager.h

  Fork-owned top-level "Build" tab (WORKFLOW_DESIGN.md §3.3; it was
  "Programming"): it makes the show's vocabulary -- palettes, effects,
  scenes, collections.
    - left:   the navigator, by section (Palettes | Effects | Scenes): the
              palette list, effect palettes and classic effects, the outline
              of scene folders (ProgrammingOutline); groups and fixtures
              under it in every section, to drag
    - right:  the Workbench: what is loaded (a scene, a collection, a
              chaser...), its layers, and the editor for the selected row

  It edits plain functions, so everything stays compatible with the classic
  Function Manager and with QLC+.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMINGMANAGER_H
#define PROGRAMMINGMANAGER_H

#include <QWidget>
#include <QSet>
#include <QSharedPointer>
#include <QElapsedTimer>
#include "function.h"
#include "powerdistribution.h"

class QTreeWidgetItem;
class QTreeWidget;
class Scene;
class FunctionsTreeWidget;
class ProgrammingOutline;
class Workbench;
class QButtonGroup;
class QStackedWidget;
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
    /** What ⌘Z / ⇧⌘Z here would undo / redo, named (the Edit menu); empty:
     *  nothing. */
    QString undoText() const;
    QString redoText() const;

    ProgrammingManager(QWidget *parent, Doc *doc);
    ~ProgrammingManager();

    /** Load function @a fid in the Workbench, as clicking it in the
     *  navigator would (and select it there). Public entry for external
     *  navigation / automated screenshots. */
    void showFunction(quint32 fid);
    /** Send to Show Tree… on chaser @a chaserId (the Workbench's header
     *  button, a chaser row's right-click): the Show Tree's Import in
     *  place, its sheet offering to go there. Whether it was sent. */
    bool sendToShowTree(quint32 chaserId);

    /** The Workbench (W3 docks this same widget in the Show Tree). */
    Workbench *workbench() const { return m_workbench; }
    /** W3: lend the Workbench to the Show Tree (it docks it there); a note
     *  stands in its place. Null: lent already. */
    Workbench *lendWorkbench();
    /** ...and have it back, loaded as it was before it was lent. */
    void returnWorkbench(Workbench *workbench);
    bool isWorkbenchLent() const { return m_workbenchStandIn != nullptr; }

    /** The navigator's sections. */
    enum Section { PalettesSection = 0, EffectsSection, ScenesSection };
    void setSection(Section section);
    Section section() const;

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

    /** The scene the Workbench edits: the selected layer's, or the scene
     *  loaded (Function::invalidId() if none, e.g. a collection with no
     *  layer selected) — lets a control surface enumerate "what should
     *  Select/Load(1-10) address right now" without its own tracking. */
    quint32 currentSceneId() const;

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    /** The navigator's selection changed: load what is selected. */
    void slotOutlineSelectionChanged();
    void slotPaletteTreeMenu(const QPoint &pos);
    void slotModeChanged();
    void slotCopy();
    void slotPaste();
    void slotPaletteDoubleClicked(QTreeWidgetItem *item);
    /** Fixtures picked (the 2D view): the Workbench shows their channels. */
    void slotFixturesSelected(const QList<quint32> &fixtureIds);
    void slotFixGroupSourceSelectionChanged();
    /** What is selected changed in the Workbench: Flash, Park, Mark follow. */
    void slotSelectionTools();

    // Bundle CRUD
    void slotStampBundle(const QString &bundleName);
    void slotSaveAsBundle();

    /** Undo (⌘Z) / Redo (⇧⌘Z): the one shared history (open 24, P1): the
     *  Show Tree's steps, the palette operations and every scene and
     *  collection edit made here -- whatever was done last, in either tab. */
    void slotUndo();
    void slotRedo();

    // Power/amperage estimate (Design mode only)
    /** Re-estimate the load of what is lit and refresh the footer. */
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
    /** The navigator's three sections and the groups & fixtures under them. */
    QWidget *buildNavigator();
    QWidget *buildScenesSection();
    QWidget *buildPalettesSection();
    QWidget *buildEffectsSection();
    /** The Effects section's list: effect palettes and classic effects. */
    void rebuildEffects();

    /** Enable/label the Park button (Park vs Unpark) from selection + park state. */
    void updateParkButtonLabel();
    void updateMarkButtonLabel();

    /** Select @a sceneId's row in the navigator (and so load it). */
    void revealScene(quint32 sceneId);
    /** Create a new function of @p type in @p folder, name it, select it in
     *  the navigator and load it. No-op if @p type isn't one of the
     *  creatable types (Scene/Chaser/Collection/EFX/RGBMatrix/Show). */
    void addNewFunction(Function::Type type, const QString &folder);

    /** Create a new palette of the given QLCPalette::PaletteType in the
     *  selected palette folder and open it in the look editor. */
    void createPalette(int paletteType);
    /** Select and scroll the palette list to @p paletteId (the Palettes
     *  section shows). */
    void revealPalette(quint32 paletteId);

    /** Duplicate palette $pid ("… (copy)"), add it, and open it for editing. */
    void duplicatePalette(quint32 pid);

    /** Same convention for the palette tree: the selection if the clicked
     *  row is in it, otherwise the clicked row alone. */
    QList<quint32> paletteTreeTargetIds(QTreeWidgetItem *clicked) const;
    /** Open the Bundle editor pre-filled with @p paletteIds and save the result. */
    void saveBundleFromPalettes(const QList<quint32> &paletteIds, Scene *sourceScene = nullptr);
    /** W2: the Folders | Classic switch over the Scenes navigator. */
    class QLayout *buildArrangementSwitch(QWidget *page);
    /** Chaser from selection: a new chaser of @a functionIds, loaded. */
    void makeChaserFrom(const QList<quint32> &functionIds, const QString &folder);

    /** Duplicate each function (smart-incremented name), select the last copy. */
    void duplicateFunctions(const QList<quint32> &fids);

    /** The confirmation text for deleting palettes $pids: where each is used
     *  (scene looks, effect-look slots) and what deleting does to them. */
    QString deletePalettesMessage(const QList<quint32> &pids) const;

    /** Delete palettes $pids after asking (unless $confirm is false). */
    bool deletePalettes(const QList<quint32> &pids, bool confirm = true);

    /** Delete/Backspace on the palette tree: the selected palettes, or the
     *  selected folder. */
    void deleteTreeSelection(FunctionsTreeWidget *tree);

    /** The confirmation text for deleting $fids (says it is the function
     *  itself, everywhere, and where else each one is used). */
    QString deleteFunctionsMessage(const QList<quint32> &fids) const;
    /** Delete each function after confirmation. Collects by id first, since the
     *  first deletion rebuilds the outline and invalidates every item pointer. */
    void deleteFunctions(const QList<quint32> &fids);

    /** Delete the user folder $item of $tree. Its functions/palettes and
     *  sub-folders move up into the parent folder — nothing is deleted. A
     *  folder with something in it asks first (unless $confirm is false).
     *  Returns true if the folder was removed. */
    bool deleteTreeFolder(FunctionsTreeWidget *tree, QTreeWidgetItem *item,
                          bool confirm = true);

private:
    Doc *m_doc;

    Workbench *m_workbench = nullptr;
    // W3: while the Show Tree has the Workbench
    QWidget *m_workbenchStandIn = nullptr;
    QSharedPointer<class WorkbenchStack> m_lentFrom;     //!< what Build had loaded
    QString m_lentSelection;                            //!< its selected row's key

    // The navigator
    QButtonGroup *m_sections = nullptr;
    QStackedWidget *m_sectionStack = nullptr;
    ProgrammingOutline *m_outline = nullptr;
    QLabel *m_outlineHint = nullptr;  //!< what a drag would do / what the outline is
    QToolButton *m_addFuncBtn; //!< "+ New" dropdown
    class QSplitter *m_mainSplit = nullptr;
    class QSplitter *m_navSplit = nullptr;
    QTreeWidget *m_effectsList = nullptr;
    bool m_navigating = false;    //!< the navigator follows a load: don't load again

    quint32 m_clipboardFunction;  //!< Cmd-C source for Cmd-V duplicate
    quint32 m_clipboardPalette;   //!< Cmd-C source palette for Cmd-V duplicate

    FunctionsTreeWidget *m_paletteTree;
    FixtureGroupSource  *m_fixGroupSource;
    BundleBrowser       *m_bundleBrowser = nullptr;
    BundleCache         *m_bundleCache   = nullptr;

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
    // drives the periodic re-estimate, stopped whenever the show goes live,
    // so Run mode pays nothing.
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
    /** Emitted when the user confirms Save Positions. */
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
