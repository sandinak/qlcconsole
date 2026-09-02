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
#include <QApplication>
#include <QString>
#include <QList>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QIcon>
#include <QPair>
#include <QPalette>

#include "dmxdumpfactoryproperties.h"
#include "qlcfixturedefcache.h"
#include "doc.h"

class QProgressDialog;
class VideoProvider;
class QMessageBox;
class QToolButton;
class QFileDialog;
class QTabWidget;
class QStatusBar;
class WebAccess;
class QToolBar;
class QPixmap;
class QAction;
class QLabel;
class QSlider;
class App;

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

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    App();
    ~App();
    void startup();
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
    bool nativeEvent(const QByteArray & eventType, void * message, long * result);
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
     * Backstage color theme
     *********************************************************************/
public:
    enum Theme { ThemeDefault = 0, ThemeTan = 1, ThemeBlue = 2,
                 ThemeQLCOriginal = 3, ThemeRedShift = 4, ThemeVSCodeDark = 5 };

    int theme() const;
    void setTheme(int theme);
    void applyTheme();

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

    void createProgressDialog();
    void destroyProgressDialog();

    /** Busy dialog shown while a workspace is being read. Unlike the startup
        dialog above this one runs with the main window already visible, which
        is exactly when a large workspace looks like a hang. */
    void createLoadProgressDialog(const QString& fileName);
    void destroyLoadProgressDialog();

public slots:
    void slotSetProgressText(const QString& text);

    /** Drives createLoadProgressDialog()'s dialog from Doc::loadProgress() */
    void slotLoadProgress(const QString& stage, int count);

private:
    QProgressDialog* m_progressDialog;
    QProgressDialog* m_loadProgressDialog;

    /*********************************************************************
     * Doc
     *********************************************************************/
public:
    void clearDocument();

    Doc *doc();

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

public slots:
    bool slotFileNew();
    QFile::FileError slotFileOpen();
    /** File > Import: merge fixtures/groups/functions from a second .qxw
     *  into the CURRENT document (unlike Open, does not replace it). */
    void slotFileImport();
    QFile::FileError slotFileSave();
    QFile::FileError slotFileSaveAs();

    void slotControlMonitor();
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
    void slotShowModeLock(bool checked);
    void slotShowLockedChanged(bool locked);
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
    void slotDetachContext(int index);
    void slotReattachContext();

    void slotHelpIndex();
    void slotHelpAbout();

    void slotRecentFileClicked(QAction *recent);

    void slotAppSettings();

private:
    QAction* m_fileNewAction;
    QAction* m_fileOpenAction;
    QAction* m_fileImportAction;
    QAction* m_fileSaveAction;
    QAction* m_fileSaveAsAction;

    QAction* m_modeToggleAction;
    QAction* m_controlMonitorAction;
    QAction* m_addressToolAction;
    QAction* m_controlBlackoutAction;
    QAction* m_controlBlindAction;
    QAction* m_showLockAction;
    /** Exit/resume timeline control (Operate-mode VC takeover). */
    QAction* m_timelineSuspendAction;
    /** Global Follow-MTC toggle (moved out of the Show Manager toolbar). */
    QAction* m_followMtcAction;
    /** Last-look persistence: toggle (enable) + momentary clear of a held look. */
    QAction* m_lastLookAction;
    QAction* m_clearLastLookAction;
    QAction* m_controlPanicAction;
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
    QAction* m_helpAboutAction;
    QAction* m_quitAction;
    QMenu* m_fileOpenMenu;
    QMenu* m_fadeAndStopMenu;

private:
    QToolBar* m_toolbar;

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
    void slotTimecodeStatusChanged();

    /** Poll + refresh the global engine-load chip (and timecode chip). */
    void slotUpdateHealthFooter();

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
    QLabel* m_statusShowLockLabel;
    QLabel* m_statusBlackoutLabel;
    /** Persistent Design/Operate indicator — the toolbar's mode-toggle
     *  button/icon always shows the mode a click would switch TO, not the
     *  current one (standard play/pause-style affordance), so there was no
     *  always-on "you are here" readout anywhere. This is that readout,
     *  same footer-chip pattern as Blackout/Blind. */
    QLabel* m_statusModeChipLabel = nullptr;
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
    QLabel* m_statusLoadLabel;
    QLabel* m_statusPowerLabel = nullptr;
    // Whether the engine-load / power-estimate / GM-fader footer chips are
    // shown at all, toggleable from the View menu and persisted
    // (workspace/showFooterLoad, workspace/showFooterPower,
    // workspace/showFooterGM). Independent of whether a chip currently has
    // anything to show (e.g. Power only ever appears once an estimate exists).
    bool m_showFooterLoad = true;
    bool m_showFooterPower = true;
    bool m_showFooterGM = true;
    QAction* m_showFooterLoadAction = nullptr;
    QAction* m_showFooterPowerAction = nullptr;
    QAction* m_showFooterGMAction = nullptr;
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
