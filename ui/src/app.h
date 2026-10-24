/*
  Q Light Controller Plus
  app.h

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#ifndef APP_H
#define APP_H

#include <QMainWindow>
#include <QKeySequence>
#include <QApplication>
#include <QString>
#include <QList>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QIcon>
#include <QPair>
#include <QPalette>
#include <functional>

#include "dmxdumpfactoryproperties.h"
#include "qlcfixturedefcache.h"
#include "doc.h"

class QProgressDialog;
class VideoProvider;
class QMessageBox;
class QToolButton;
class QMenu;
class QFileDialog;
class QTabWidget;
class QStatusBar;
class WebAccess;
class QToolBar;
class MainBar;
class ShowControls;
class QPixmap;
class QAction;
class QLabel;
class QSlider;
class App;
class StartupWindow;
class Chaser;
class ShowControl;

/** @addtogroup ui UI
 * @{
 */

#define KXMLQLCWorkspace QStringLiteral("Workspace")

class DetachedContext final : public QMainWindow
{
    Q_OBJECT

public:
    DetachedContext(QWidget *parent) : QMainWindow(parent) {}

protected slots:
    void closeEvent(QCloseEvent *ev) override
    {
        emit closing();
        // avoid the real context to be destroyed !
        setCentralWidget(NULL);
        QMainWindow::closeEvent(ev);
    }

signals:
    void closing();
};

class App final : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY(App)
    friend class HelpShots;     // sets up views for the help's pictures

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    App();
    ~App();
    /** @param workspaceFile If non-empty, loaded through the same
     *         StartupWindow used for the rest of boot -- see the comment
     *         on the StartupWindow member -- instead of main() loading it
     *         separately after the main window is already visible. */
    void startup(const QString &workspaceFile = QString());
    void enableOverscan();
    void disableGUI();

    /** If QLC_SHOT_DIR is set, after load schedule a one-shot scenario that
     *  navigates to a UI state and dumps offscreen PNG grabs (for automated UI
     *  eyeballing), then quits. No-op when the env var is unset. */
    void captureScenarioIfRequested();

private:
    void init();
    void closeEvent(QCloseEvent*) override;
    void setActiveWindow(const QString& name);
    /** Open the assisted timecode-offset calibration dialog for the current show. */
    void openTimecodeCalibration();

protected:
    /** Clicking the status-bar MTC chip opens the timecode-bind menu. */
    bool eventFilter(QObject *watched, QEvent *event) override;

#if defined(WIN32) || defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void disableTimerResolutionThrottling();
#endif

    /*********************************************************************
     * Tab label mode
     *********************************************************************/
public:
    enum TabLabelMode { TabIconAndText = 0, TabIconOnly = 1, TabTextOnly = 2 };

    int tabLabelMode() const;
    void setTabLabelMode(int mode);
    void applyTabLabelMode();

    /** Switch the main tab strip to whichever tab hosts @p w (no-op if @p w
     *  isn't a direct tab-page widget). Lets a tab's own content navigate
     *  elsewhere in the app — e.g. a Show-timeline clip's double-click
     *  jumping to the Programming tab to edit the look it plays. */
    void switchToTabContaining(QWidget *w);

private:
    int m_tabLabelMode = TabIconAndText;
    QList<QPair<QString, QIcon>> m_tabOriginals; // stored on first tab add

    /*********************************************************************
     * Workspace views (View ▸ Workspace; board Q35/Q36)
     *********************************************************************/
public:
    /** Which tabs the main window shows. Classic QLC+: the stock QLC+ tabs,
     *  in stock order and names; qlcconsole: the qlcconsole workflow
     *  (Functions folded into Build); Everything: every tab. Saved with the
     *  show ("View" on <Workspace>, which older readers ignore). Switching
     *  only shows and hides tabs -- never the show itself. */
    enum WorkspaceView { ClassicView = 0, ConsoleView = 1, EverythingView = 2 };

    WorkspaceView workspaceView() const { return m_workspaceView; }
    /** Show @p view's tabs. @p markModified: the show is modified (the view
     *  is saved with it; it is not an undo step). Hidden tabs keep their
     *  widgets; a view in a window of its own stays in it. */
    void setWorkspaceView(WorkspaceView view, bool markModified = true);
    /** The tab labels @p view shows, in its order. */
    static QStringList workspaceViewTabs(WorkspaceView view);
    /** "classic", "qlcconsole", "everything": as saved. */
    static QString workspaceViewKey(WorkspaceView view);
    static WorkspaceView workspaceViewFromKey(const QString &key, bool *ok = nullptr);
    /** What the View menu calls @p view. */
    static QString workspaceViewName(WorkspaceView view);
    /** A show saved without a view: Classic for an old QLC+ show (no Show
     *  Tree, not saved by qlcconsole), else qlcconsole. */
    WorkspaceView defaultViewForLoadedShow() const;
    /** What tab @p label reads as in the current view ("Inputs/Outputs" for
     *  Connections in Classic QLC+); its identity stays @p label. */
    QString tabDisplayLabel(const QString &label) const;
    /** A quiet line above the tabs: @p text, with a button @p buttonText
     *  running @p action when given; × puts it away. */
    void showViewNotice(const QString &text, const QString &buttonText = QString(),
                        const std::function<void()> &action = nullptr);
    void hideViewNotice();
    /** A device input came or went (InputDeviceNotices): the notice line,
     *  amber while @p warning; a plain one puts itself away after a while. */
    void showInputDeviceNotice(const QString &text, bool warning);
    /** After Upgrade This Show… on a Classic show that now has a Show Tree:
     *  offer (once per show) the qlcconsole view. True: offered. */
    bool offerConsoleViewAfterUpgrade();
    /** Q92: the first GO pressed in Design this session (Run tab or Control
     *  ▸ GO) offers Operate in the notice bar. Once a session. */
    void offerOperateAfterGo();

private:
    /** The current view's tabs, in order, with Functions in or out by
     *  setFunctionsWindowShown(). */
    QStringList effectiveViewTabs() const;
    /** Put the tab bar in step with the view: its tabs in its order, the
     *  rest put away (or left in their own windows). */
    void applyWorkspaceTabs();
    /** Classic QLC+ looks like QLC+ (Q78): its main toolbar, the stock
     *  Fixtures and Functions toolbars, and a footer of the mode, the Grand
     *  Master and Ready only. Run on every view change. */
    void applyClassicChrome();
    /** Whether @p label is a tab of the current view. */
    bool tabInView(const QString &label) const;
    /** Where @p label goes in the tab bar now: after the docked tabs that
     *  come before it in the current view. */
    int tabInsertIndex(const QString &label) const;
    /** The widget for tab @p label, wherever it is (tab, window, put away). */
    QWidget *contextWidget(const QString &label) const;
    void updateWorkspaceViewActions();

    WorkspaceView m_workspaceView = ConsoleView;
    QString m_loadedCreator;                   ///< <Creator><Name> of the show loaded
    bool m_upgradeOfferMade = false;           ///< offerConsoleViewAfterUpgrade(), per show
    QList<QAction *> m_workspaceViewActions;   ///< View ▸ Workspace
    QWidget *m_viewNotice = nullptr;
    class QLabel *m_viewNoticeText = nullptr;
    class QPushButton *m_viewNoticeButton = nullptr;
    std::function<void()> m_viewNoticeAction;
    class InputDeviceNotices *m_inputNotices = nullptr;

    /*********************************************************************
     * Show controls placement (MainBar)
     *********************************************************************/
public:
    /** Where Stop all / Blackout / Blind / Design|Operate sit: in the one
     *  top bar beside the current tab's own tools (the default), or in the
     *  status bar, leaving the top to the tab's tools alone. */
    enum ShowControlsPlacement { ShowControlsInTopBar = 0, ShowControlsInStatusBar = 1 };

    int showControlsPlacement() const { return m_showControlsPlacement; }
    /** Saved, and applied at once (no restart) */
    void setShowControlsPlacement(int placement);
    static QString showControlsPlacementKey();

private:
    void applyShowControlsPlacement();

    int m_showControlsPlacement = ShowControlsInTopBar;

    /*********************************************************************
     * Backstage color theme
     *********************************************************************/
public:
    enum Theme { ThemeDefault = 0, ThemeTan = 1, ThemeBlue = 2,
                 ThemeQLCOriginal = 3, ThemeRedShift = 4, ThemeVSCodeDark = 5 };

    int theme() const;
    void setTheme(int theme);
    void applyTheme();
    /** The palette @p theme gives, built on @p base (the native one for
     *  ThemeDefault) -- what applyTheme() sets; a test renders with it. */
    static QPalette themePalette(int theme, const QPalette &base);
    /** Every theme and its name, in the order Settings ▸ Appearance lists them. */
    static QList<QPair<int, QString> > themeChoices();

    /** The footer's optional chips (Settings ▸ Appearance), each shown or
     *  hidden for this computer (QSettings workspace/showFooter*). */
    enum FooterChip { FooterLoad, FooterPower, FooterGrandMaster, FooterSpeedMasters };
    bool footerChipShown(FooterChip chip) const;
    void setFooterChipShown(FooterChip chip, bool on);
    /** The speed masters' beat (SpeedMastersFooter's settings) read again. */
    void reloadFooterBeatOptions();

private:
    int m_theme = ThemeDefault;
    // Captured once, before any theme is ever applied (QApplication always
    // exists by the time App is constructed — see main.cpp), so selecting
    // "Default" can restore the true native palette exactly.
    QPalette m_defaultPalette = qApp->palette();

private:
    QTabWidget* m_tab;
    QDir m_workingDirectory;
    bool m_overscan;
    bool m_noGui;

    /*********************************************************************
     * Progress dialog
     *********************************************************************/
public:
    /** Re-check whether every patched universe can actually reach its output,
        and update the footer indicator. Cheap; safe to call after any load or
        re-patch. */
    void updateOutputReadiness();

    /** Re-read @p file's physical properties into the loaded definitions
     *  from it, whenever it changes on disk from now on -- a definition
     *  opened from the footer's "declare no light output" to set its lumens
     *  takes effect when it is saved, show still open. */
    void watchDefinition(const QString &file);
private:
    class QFileSystemWatcher *m_definitionWatcher = nullptr;
public:

    /** Busy dialog shown while a workspace is being read outside of boot
        (File > Open, a recent file, autosave recovery) -- the main window
        is already visible then, which is exactly when a large workspace
        looks like a hang. A no-op (routes into StartupWindow instead) when
        called during App::startup()'s own workspaceFile load, so callers
        don't need to know which case they're in. */
    void createLoadProgressDialog(const QString& fileName);
    void destroyLoadProgressDialog();

public slots:
    /** Drives createLoadProgressDialog()'s dialog (or, during boot,
        StartupWindow) from Doc::loadProgress(). */
    void slotLoadProgress(const QString& stage, int count);

private:
    QProgressDialog* m_loadProgressDialog;

    /** Owns the whole visible boot sequence: App::init()'s engine/plugin/UI
        build, and -- when App::startup() was given a workspaceFile --
        that file's load too, so a -o launch never shows more than this one
        window before the fully-loaded main window appears. NULL once
        startup() returns; every startup-time step guards on that so the
        same code path works when App::init() is called directly (as the
        autosave test does) with no StartupWindow at all. */
    StartupWindow* m_startupWindow;

    /*********************************************************************
     * Doc
     *********************************************************************/
public:
    void clearDocument();

    /** Show the stage in the person's unit (Settings -> Units): metres for
     *  mm/cm/m, feet for in/ft. After a show loads (@a fromShow: the first
     *  show ever opened may seed an unset preference from its own grid unit),
     *  after New, and when the preference changes. */
    void applyUnitPreference(bool fromShow);

    Doc *doc();
    /** Show control's OSC listener and settings (QLab). */
    class ShowControlListener *showControlListener() const { return m_showControlListener; }
    /** Open Connections ▸ Devices at its Show control & sync rows. */
    void showShowControlSettings();

    /** Open Settings at the show's rig networks (the rig network footer
     *  chip's click). */
    void showRigNetworkSettings();

    /** The QSettings key of "Only transmit on rig networks" (default on) */
    static QString rigNetworkGateSettingKey();

    /** The rig network footer chip's text for @a st, and whether it is the
     *  red kind (@a alarm) -- empty when there is nothing to say. */
    static QString rigNetworkFooterText(const InputOutputMap::TransmitGateStatus &st,
                                        bool *alarm = nullptr);

private slots:
    void slotDocModified(bool state);
    void slotDocAutosave();
    void slotUniverseWritten(quint32 idx, const QByteArray& ua);
    /** Re-run updateWindowTitle() when the active tab changes, so the title
     *  bar's tab name stays current. */
    void slotTabChanged(int index);

private:
    /** Rebuild and apply the main window's title: app name, showfile name
     *  (or "New Workspace"), a modified-state "*", and the active tab's
     *  name — so the title bar always identifies both the open file and
     *  which tab is showing, even before any edit has happened. */
    void updateWindowTitle();

private:
    void initDoc();

private:
    class ShowControlListener *m_showControlListener = nullptr;
    Doc* m_doc;

    /** Device-agnostic control-surface engine + the PMJ Black 1 overlay
     *  (CONTROL_SURFACE_DESIGN.md). Parented to `this`, no manual cleanup
     *  needed. Constructed at the end of initDoc(), once Doc's I/O map is
     *  fully ready. */
    class ControlSurfaceEngine* m_controlSurfaceEngine = nullptr;
    class PMJOverlay* m_pmjOverlay = nullptr;

    /*********************************************************************
     * Main operating mode
     *********************************************************************/
public:
    void enableKioskMode();
    void createKioskCloseButton(const QRect& rect);

public slots:
    void slotModeOperate();
    void slotModeDesign();
    void slotModeToggle();
    void slotModeChanged(Doc::Mode mode);

    /*********************************************************************
     * Actions and toolbar
     *********************************************************************/
private:
    void initActions();
    void initToolBar();
    /** Build the native (macOS top-of-screen) menu bar: File/View/Control/Help.
     *  About/Preferences/Quit are relocated to the application menu by Qt via
     *  their QAction::menuRole on macOS. */
    void initMenuBar();
    bool handleFileError(QFile::FileError error);
    bool saveModifiedDoc(const QString & title, const QString & message);

    /** Load @p fileName into a new, throwaway Doc (not m_doc) for File >
     *  Import to pick items from. Returns NULL and sets @p error on
     *  failure; caller owns the returned Doc on success. */
    Doc *loadScratchDoc(const QString &fileName, QString &error);

    /** File > Import of an ETC Eos show (@p asciiPath, its USITT ASCII
     *  export): the EosImportWizard, the usual picker and merge, then the
     *  cue lists into the Show Tree (EOS_IMPORT_DESIGN.md). */
    void importEosShow(const QString &asciiPath);
    /** File > Import's report: the merge, with @p before as its details
     *  (what an Eos file held) and @p after appended (the Show Tree). */
    void showImportSummary(const class QxwImportResult &result, const QStringList &before = QStringList(),
                           const QStringList &after = QStringList());
    /** File ▸ Import ▸ Workspace: a .qxw/.qlcc's fixtures, groups,
     *  functions and palettes, picked in ImportSelectionDialog and merged
     *  into the current show (QxwImporter). */
    void importWorkspaceFile(const QString &path);
    /** File ▸ Import ▸ Fixtures List: a Fixture Manager export (.qxfl),
     *  its fixtures and groups added to the current show. */
    void importFixtureListFile(const QString &path);

public:
    /** One entry of File ▸ Import: a kind of file the app can bring into
     *  the open show. importSources() is the whole table -- the submenu,
     *  its sections, each entry's file dialog filters and "Any Supported
     *  File…"'s dispatch by extension all come from it, so a new source
     *  (QLab is the next one expected, see EOS_IMPORT_DESIGN.md) is one
     *  more row there and one handler. List only importers that work. */
    struct ImportSource
    {
        QString key;            ///< the action's objectName is "fileImport_<key>"
        QString section;        ///< the submenu section it sits under
        QString label;          ///< the menu item
        QString title;          ///< its file dialog's title
        QString tip;
        QString icon;           ///< resource path; empty for none
        QStringList filters;    ///< the file dialog's name filters, its own first
        QStringList extensions; ///< ".qxw"...: what "Any Supported File…" matches
        void (App::*handler)(const QString &path);
    };
    QList<ImportSource> importSources() const;
    /** "Any Supported File…": the source whose extension @p path has (a
     *  workspace when none does, as the one Import did), then its handler. */
    void importFile(const QString &path);
    /** File ▸ Import ▸ (submenu). */
    QMenu *fileImportMenu() const { return m_fileImportMenu; }

    /** Test seams. A picker replaces the import file dialog (returns the
     *  path, or empty for Cancel); a run hook replaces calling the source's
     *  handler (it gets the source's key and the path). */
    typedef std::function<QString(const QString &title, const QStringList &filters)> ImportFilePicker;
    typedef std::function<void(const QString &key, const QString &path)> ImportRunHook;
    void setImportFilePicker(ImportFilePicker picker) { m_importFilePicker = picker; }
    void setImportRunHook(ImportRunHook hook) { m_importRunHook = hook; }

private:
    void populateImportMenu(QMenu *menu);
    QString pickImportFile(const QString &title, const QStringList &filters);
    void runImport(const ImportSource &source, const QString &path);
    QMenu *m_fileImportMenu = nullptr;
    ImportFilePicker m_importFilePicker;
    ImportRunHook m_importRunHook;

public slots:
    bool slotFileNew();
    QFile::FileError slotFileOpen();
    /** File ▸ Import ▸ Any Supported File…: one dialog for every source,
     *  dispatched by extension (importFile()). Imports merge into the
     *  CURRENT document (unlike Open, which replaces it). */
    void slotFileImport();
    QFile::FileError slotFileSave();
    QFile::FileError slotFileSaveAs();
    /** File → Collect for Transfer…: the show as a .qlcc in a folder of its
     *  own, with Fixtures/, EffectScripts/, RGBScripts/ and HeadLayouts/
     *  beside it (SHOW_EMBEDDING_DESIGN.md §6). */
    void slotFileCollectForTransfer();

    void slotControlMonitor();
    /** Open the rig overview (View menu). Delegates to the Monitor, which owns
     *  the window, so the menu and the studio footer share one instance. */
    void slotRigOverview();
    /** Focus mode: Monitor::openFocusMode(). */
    void slotFocusMode();
    void slotAddressTool();
    void slotControlBlackout();
    /** Toggle global Blind (output inhibit) from the main toolbar. */
    void slotControlBlind(bool checked);
    void slotBlackoutChanged(bool state);
    /** Show/hide the global Blind (output-inhibited) status-bar chip. */
    void slotOutputInhibitedChanged(bool state);
    /** Footer Grand Master fader moved by the user. */
    void slotFooterGrandMasterMoved(int value);
    /** Engine-side Grand Master value changed (VC's own GM fader, MIDI/DMX
     *  input, etc.) — keep the footer fader in sync. */
    void slotFooterGrandMasterValueChanged(uchar value);
    /** Toggle the running show's timeline suspend (Operate-mode VC takeover). */
    void slotControlTimelineSuspend(bool checked);
    /** Refresh the footer "under timeline control" chip + exit button state. */
    void slotTimelineControlChanged();
    /** Arm/disarm global timecode-follow from the main toolbar. */
    void slotFollowTimecodeToggled(bool checked);
    void slotFollowTimecodeChanged(bool enabled);
    void slotLastLookToggled(bool checked);
    void slotClearLastLook();
    void slotControlPanic();
    void slotFadeAndStopAll();
    void slotRunningFunctionsChanged();
    void slotDumpDmxIntoFunction();
    void slotFunctionLiveEdit();
    void slotLiveEditVirtualConsole();
    void slotCaptureLiveEdits(bool checked);
    void slotCaptureStore();
    void slotCaptureUndo();
    void slotCapturePendingChanged();
    void slotCaptureUndoStackChanged();
    /** Bring a view to the front by its tab label, wherever it lives: switch
     *  to its tab if it is docked, or raise its own window if it has been
     *  detached. */
    void showContext(const QString &tabLabel);
    /** Window → Functions: the Functions tab (the QLC+ Function Manager);
     *  from a view without it, showContext() switches view as it does for
     *  any hidden tab. */
    void showFunctions();
    /** What is overriding the rig right now (Q80), by name, in footer
     *  order: "Park (3)", "Highlight", "Hold at end", "Focus mode", "Blind". */
    QStringList activeOverrides() const;
    /** Let go of every override activeOverrides() lists (Focus mode may ask
     *  about unsaved offsets first). */
    void clearOverrides();
    /** Window → Effects: Build ▸ Effects, effects' one home (there is no
     *  Effects window of its own). */
    void showEffects();
    /** THE switch for the Functions window (Function Manager), board Q32:
     *  shown, it is a tab of its own again and Window → Functions brings it
     *  forward; hidden -- the qlcconsole view -- it has no tab and Window →
     *  Functions switches to a view that has it. setWorkspaceView() sets it
     *  for the view (shown in Classic QLC+ and Everything). Opening a
     *  function from elsewhere (Show Tree Open, Edit Chase…) loads it in the
     *  Workbench either way. */
    void setFunctionsWindowShown(bool shown);
    bool functionsWindowShown() const { return m_functionsWindowShown; }
    /** The help topic for @p w ("lighting-studio.md#mounting-a-fixture"):
     *  the nearest "helpTopic" property up its parents, else the tab on
     *  screen's, else Getting Started. */
    QString helpTopicFor(QWidget *w) const;
    /** Take a view off the tab bar, there to open from the Window menu in a
     *  window of its own (showContext()); closing that window puts it away
     *  again. Functions lives this way while hidden. */
    void parkContext(const QString &tabLabel);

    /** The views broken out into their own windows, by tab label. */
    QStringList detachedContexts() const;
    /** Put the view @p tabLabel back in the main window, and show it. */
    void reattachContext(const QString &tabLabel);
    /** Put every broken-out view back in the main window. */
    void reattachAllContexts();
    /** The Window menu, as things are: which views are in their own windows. */
    void updateWindowMenu();
    /** The Window menu's tab jumps in the view's order, numbered by it:
     *  ⇧⌘1 is the first tab the view shows, ⇧⌘0 the tenth. Tabs the view
     *  hides follow, with no shortcut. Run on every view change (Q69). */
    void updateWindowJumps();

    void slotDetachContext(int index);

    /** Shift-click on a tab breaks the view out into its own window (Window
     *  menu toggle, remembered). A plain double-click no longer does: it
     *  tore views off the tab bar by accident. */
    bool shiftClickBreaksOut() const;
    /** A tab's right-click menu (Open in Its Own Window). */
    QMenu *tabContextMenu(int index);
    void setShiftClickBreaksOut(bool on);
private:
    /** Show @p context in a window of its own, titled for @p tabLabel. */
    void openContextWindow(QWidget *context, const QString &tabLabel);
    QList<QWidget *> m_parkedContexts;      ///< views with no tab (parkContext())
    bool m_functionsWindowShown = false;    ///< setFunctionsWindowShown()
public slots:
    void slotReattachContext();

    void slotHelpIndex();
    /** F1: the help on what is in front (helpTopicFor()). */
    void slotHelpContext();
    void slotHelpAbout();

    void slotRecentFileClicked(QAction *recent);

    void slotAppSettings();

public:
    /** Update checks and installs (nullptr without a GUI). */
    class UpdateManager *updateManager() const { return m_updateManager; }
private:
    class UpdateManager *m_updateManager = nullptr;
    /** Software Update's "Restart and Install": the close in progress
     *  reopens the show once the update is in place. */
    bool m_restartForUpdate = false;

private:
    QAction* m_fileNewAction;
    QAction* m_fileOpenAction;
    QAction* m_fileImportAction;
    QAction* m_fileSaveAction;
    QAction* m_fileSaveAsAction;

    QAction* m_modeToggleAction;
    QAction* m_controlMonitorAction;
    QAction* m_addressToolAction;
    QAction* m_rigOverviewAction;
    /** Classic QLC+'s main toolbar (Q78); hidden in the other views. */
    class QToolBar* m_classicToolbar = nullptr;
    /** Classic QLC+'s footer: no load, power, timecode, Next or speed chips. */
    bool m_classicChrome = false;
    QList<QAction *> m_windowJumps;         ///< Window menu: a view each (Everything's order)
    QMenu *m_windowMenu = nullptr;
    QAction *m_windowHiddenSeparator = nullptr;  ///< Window menu: before the view's hidden tabs
    QAction *m_windowJumpsEnd = nullptr;         ///< Window menu: after the last tab jump
    QMenu *m_windowDockMenu = nullptr;      ///< Window > Put Back in Main Window
    QAction *m_windowDockAll = nullptr;
    QAction *m_windowBreakOut = nullptr;    ///< Window > Move <view> to Its Own Window
    QAction *m_windowShiftClick = nullptr;  ///< Window > Shift-Click a Tab ... (toggle)
    QAction* m_controlBlackoutAction;
    QAction* m_controlBlindAction;
    /** Exit/resume timeline control (Operate-mode VC takeover). */
    QAction* m_timelineSuspendAction;
    /** Global Follow-MTC toggle (moved out of the Show Manager toolbar). */
    QAction* m_followMtcAction;
    /** Last-look persistence: toggle (enable) + momentary clear of a held look. */
    QAction* m_lastLookAction;
    QAction* m_clearLastLookAction;
    QAction* m_controlPanicAction;
    QAction* m_cueGoAction = nullptr;       ///< the main cue list's GO
    QAction* m_cueBackAction = nullptr;
    QAction* m_cueSubGoAction = nullptr;    ///< its Sub GO (SUBCUES_DESIGN.md)
    QAction* m_cueSubBackAction = nullptr;
    QAction* m_dumpDmxAction;
    QAction* m_liveEditAction;
    QAction* m_liveEditVirtualConsoleAction;
    QAction* m_captureLiveEditsAction;
    QAction* m_captureStoreAction;
    QAction* m_captureUndoAction;
    QMetaObject::Connection m_captureCountConnection;
    /** Coalesces the ~50 Hz overrideRecorded burst into one buildPlan()/UI
     *  refresh: buildPlan() walks the whole Doc and locks the override hash
     *  (contending the DMX thread), so it must not run per DMX tick. */
    QTimer* m_captureCoalesceTimer = nullptr;
    QAction* m_appSettingsAction;

    QAction* m_helpIndexAction;
    QAction* m_helpContextAction = nullptr;
    QAction* m_helpAboutAction;
    QAction* m_checkUpdatesAction = nullptr;    ///< Help → Check for Updates…
    QAction* m_quitAction;
    QMenu* m_fileOpenMenu;
    QMenu* m_fadeAndStopMenu;

private:
    /** The one top bar: holds m_mainBar (the tab's tools + show controls) */
    QWidget* m_toolbar;
    MainBar* m_mainBar = nullptr;
    ShowControls* m_showControls = nullptr;

    /*********************************************************************
     * Utilities
     *********************************************************************/
private:
    DmxDumpFactoryProperties *m_dumpProperties;
    VideoProvider *m_videoProvider;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Set the name of the current workspace file
     */
    void setFileName(const QString& fileName);

    /**
     * Get the name of the current workspace file
     */
    QString fileName() const;

    /**
     * Get the autosave version of the name
     * of the current workspace file
     */
    QString autoSaveFileName() const;

    /**
     * Update the recent file drop down menu
     */
    void updateFileOpenMenu(QString addRecent);

    /**
     * Load workspace contents from a file with the given name.
     *
     * @param fileName The name of the file to load from.
     * @return QFile::NoError if successful.
     */
    QFile::FileError loadXML(const QString& fileName);

    /**
     * Load workspace contents from the given XML document.
     *
     * @param doc The XML document to load from.
     */
    bool loadXML(QXmlStreamReader &doc, bool goToConsole = false, bool fromMemory = false);

    /**
     * Save workspace contents to a file with the given name. Changes the
     * current workspace file name to the given fileName.
     *
     * @param fileName The name of the file to save to.
     * @return QFile::NoError if successful.
     */
    QFile::FileError saveXML(const QString& fileName, bool autosave = false);

    /*********************************************************************
     * Definitions a show needs, kept on this computer
     *********************************************************************/
public:
    /** Before @p fileName loads: every fixture definition it names, from
     *  the library, else the show cache, else fetched from cloud storage
     *  (with a window that says what it waits on, and lets you skip). */
    void prepareShowDefinitions(const QString &fileName);
    /** Copy every definition the show uses into the show cache, so the next
     *  run of it needs nothing remote. */
    void cacheShowDefinitions();
    /** Fixtures standing in as dimmers for a definition that was not here:
     *  fetch what can be fetched and give them their own back. */
    void recoverUnavailableDefinitions(QWidget *parent);
    /** A save with fixtures standing in for missing definitions: say so
     *  first. False when the user cancels. */
    bool confirmSaveWithoutDefinitions();
    /** Read @p paths (online-only files), which makes the cloud send them,
     *  in a thread, under a window saying what it waits on; Skip stops.
     *  The ones now on this computer. */
    QStringList fetchFromCloud(const QStringList &paths, QWidget *parent);

public slots:
    void slotLoadDocFromMemory(QString xmlData);

    void slotSaveAutostart(QString fileName);

private:
    QString m_fileName;

    /*********************************************************************
     * Autosave
     *********************************************************************/
public:
    /** Check if autosave is enabled */
    bool isAutosaveEnabled() const;

    /** Enable or disable autosave */
    void setAutosaveEnabled(bool enable);

    /** Get autosave interval in minutes */
    int autosaveInterval() const;

    /** Set autosave interval in minutes */
    void setAutosaveInterval(int minutes);

    /** Get the autosave file path for the current document */
    QString autosaveFilePath() const;

private slots:
    /** Slot called by autosave timer to perform autosave */
    void slotAutosave();

private:
    /** Initialize autosave system */
    void initAutosave();

    /** Check for autosave recovery file and offer to restore */
    void checkAutosaveRecovery();

    /** Remove autosave file for current document */
    void removeAutosaveFile();

private:
    QTimer *m_autosaveTimer;
    bool m_autosaveEnabled;
    int m_autosaveInterval;  // in minutes
    QString m_lastAutosaveTime;

    /*********************************************************************
     * Status Bar
     *********************************************************************/
public:
    /** Set the mode message shown in the status bar */
    void setStatusMessage(const QString& message);

    /** Clear the mode message from the status bar */
    void clearStatusMessage();

    /** True when any universe is patched to an output. Without one nothing
     *  reaches a rig: the readiness chip reads "No outputs: preview only",
     *  in grey, instead of a red "Not ready". */
    bool rigHasOutputs() const;

private:
    /** Initialize the status bar */
    void initStatusBar();

    /** Update the status bar display */
    void updateStatusBar();

    /** Full detail for every currently-registered ShowStatus entry, opened
     *  by clicking the "Not ready" chip -- the third and most complete of
     *  the chip's three information densities (see updateStatusBar()). */
    void showStatusDetails();

private slots:
    /** Programmer dirty/clean transition — refresh the status bar. */
    void slotProgrammerDirtyChanged(bool dirty);

    /** Programmer selection changed — refresh the "Selected: ..." label. */
    void slotProgrammerSelectionChanged();

    /** Pad-grid mode changed — refresh the "Pad: ..." label. */
    void slotPadModeChanged(Doc::PadMode mode);

    /** Refresh the global timecode chip (position/state colour). */
    /** The footer's "Next:" -- the main cue list's standby cue. */
    void slotUpdateStandby();
    void slotTimecodeStatusChanged();

    /** Poll + refresh the global engine-load chip (and timecode chip). */
    void slotUpdateHealthFooter();

public:
    /** The footer's sub-cue part for @a list (SUBCUES_DESIGN.md): "Sub: 4.2"
     *  (what Sub GO plays), "Sub: (end of 4)", or empty when the cue playing
     *  has no sub-cues on Sub GO. */
    static QString standbySubCueText(const Chaser *list);
    /** The footer's "Next:" part for @a list: what GO plays -- a LOADed
     *  sub-cue as its number and its own name ("Next: 4.2 — Wash"), a cue
     *  as its number and its function's ("Next: 5 — Blackout") -- or
     *  "Next: (end of <list>)". */
    static QString standbyNextText(const ShowControl *sc, const Chaser *list);
    /** Sub GO and Sub Back on the main cue list: ⌥⌘↩ and ⌥⌘⇧↩ on macOS
     *  (beside GO ⌘↩ and Back ⌘⇧↩). */
    static QKeySequence cueSubGoShortcut();
    static QKeySequence cueSubBackShortcut();

private:
    QLabel* m_statusModeLabel;
    /** Unsaved/Autosaved/Saved — one consolidated chip instead of three
     *  (a separate "Saved"/"Unsaved" chip plus an always-visible
     *  "Autosave: Enabled"/"Last autosave: HH:MM:SS" chip). Driven by the
     *  existing Doc::modified signal (unsaved/saved) and the autosave
     *  completion point (autosaved) — see slotDocModified() and the
     *  autosave path in saveXML(). */
    QLabel* m_statusDirtyLabel;
    QLabel* m_statusProgrammerLabel;
    QLabel* m_statusSelectionLabel;
    QLabel* m_statusPadModeLabel;
    QLabel* m_statusBlackoutLabel;
    /** Rig network transmit gate: red while network output is held because
     *  this machine is not on the show's rig network, amber when the show
     *  has none set or the check is off/lifted. Click opens the setting. */
    QLabel* m_statusRigNetLabel = nullptr;
    void updateRigNetworkFooter();
    /** "Update available: 0.1.1" -- driven by m_updateManager, hidden while
     *  there is nothing to say; a click opens Software Update. */
    QLabel* m_statusUpdateLabel = nullptr;
    /** Persistent Design/Operate indicator — the toolbar's mode-toggle
     *  button/icon always shows the mode a click would switch TO, not the
     *  current one (standard play/pause-style affordance), so there was no
     *  always-on "you are here" readout anywhere. This is that readout,
     *  same footer-chip pattern as Blackout/Blind. */
    QLabel* m_statusModeChipLabel = nullptr;
    /** The one override indicator (Q80): what is holding the rig over the
     *  show right now -- Park, Highlight, Hold at end, Focus mode, Blind --
     *  and a click clears them all. Hidden when nothing is. */
    class QToolButton* m_statusOverridesChip = nullptr;
    class QTimer* m_overridesPoll = nullptr;
    /** Compact footer Grand Master fader — GM previously lived only inside
     *  the Virtual Console tab, unlike its safety-tier siblings (Blackout,
     *  Blind, Show Lock), which are all global-toolbar/footer. This mirrors
     *  those, always visible and adjustable regardless of active tab. */
    QSlider* m_statusGrandMasterSlider = nullptr;
    QLabel* m_statusGrandMasterValueLabel = nullptr;
    /** The GM label+slider+value's shared container — toggled as one unit
     *  by m_showFooterGM, same pattern as Load/Power below. */
    QWidget* m_statusGrandMasterBox = nullptr;
    QLabel* m_statusTimecodeLabel;
    class QToolButton* m_statusStandby = nullptr;   ///< "Next: 12.5 — ..."
    QLabel* m_statusLoadLabel;
    QLabel* m_statusPowerLabel = nullptr;
    // Whether the engine-load / power-estimate / GM-fader footer chips are
    // shown at all, set in Settings ▸ Appearance and persisted
    // (workspace/showFooterLoad, workspace/showFooterPower,
    // workspace/showFooterGM). Independent of whether a chip currently has
    // anything to show (e.g. Power only ever appears once an estimate exists).
    bool m_operateOffered = false;   ///< Q92: offered once a session
    bool m_showFooterLoad = false;
    bool m_showFooterPower = true;
    bool m_showFooterGM = true;
    bool m_showFooterSpeed = true;
    class SpeedMastersFooter* m_statusSpeedMasters = nullptr;
    // Move-in-black dangle warning: marked (positioned-but-dark) fixtures no
    // upcoming cue is about to light. Hidden when empty; ProgrammerController
    // forwards MarkPlanner::dangleFixturesChanged() into this.
    QLabel* m_statusDangleLabel = nullptr;
    // Smooth (extrapolated) MTC chip readout, decoupled from the chunky packet
    // rate: anchor on each fresh position, then glide the display on a timer.
    QTimer* m_tcDisplayTimer = nullptr;
    QElapsedTimer m_tcWall;
    quint32 m_tcAnchorMs = 0;
    qint64  m_tcAnchorWallMs = 0;
    QString m_tcLastStyle;
    /** "Under timeline control" / "Timeline suspended" chip (Operate mode). */
    QLabel* m_statusTimelineLabel;
    QTimer* m_healthTimer;
    QString m_statusMessage;
};

/** @} */

#endif
