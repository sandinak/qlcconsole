/*

  Q Light Controller Plus
  app.cpp

  Copyright (c) Heikki Junnila,
                Christopher Staite
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

#include <QContextMenuEvent>
#include <QMenu>
#include <QActionGroup>
#include <QToolButton>
#include <QTableWidget>
#include <QtWidgets>
#include <cstdio>
#include <unistd.h>
#include <QtCore>

#if defined(WIN32) || defined(Q_OS_WIN)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#include "functionliveeditdialog.h"
#include "inputoutputmanager.h"
#include "functionselection.h"
#include "effectdesigner.h"
#include "itemicons.h"
#include "functionmanager.h"
#include "programmingmanager.h"
#include "showtreecontroller.h"
#include "cuerunner.h"
#include "showtreeview.h"
#include "workbench.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "virtualconsole.h"
#include "fixturemanager.h"
#include "dmxdumpfactory.h"
#include "showmanager.h"
#include "show.h"
#include "timecodecalibrationdialog.h"
#include "chaser.h"
#include "mastertimer.h"
#include "timecodesource.h"
#include "addresstool.h"
#include "simpledesk.h"
#include "appsettings.h"
#include "capturemanager.h"
#include "livecapturedialog.h"
#include "aboutbox.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "studiogroupeditor.h"
#include "studioplaneview.h"
#include "structurestudioview.h"
#include "studiocomponentbrowser.h"
#include "studiotemplate.h"
#include "monitorproperties.h"
#include "vcframe.h"
#include "speedmastersfooter.h"
#include "updatemanager.h"
#include "app.h"
#include "measurementunits.h"
#include "effectscriptrunner.h"
#include <QPointer>
#include <thread>
#include <QRegularExpression>
#include <memory>
#include <atomic>
#include <QProgressDialog>
#include "doc.h"
#include "shutterautoopen.h"
#include "fixture.h"
#include "programmercontroller.h"
#include "qxwimporter.h"
#include "importselectiondialog.h"
#include "eosimportwizard.h"
#include "eosimporter.h"
#include "lastlookeffect.h"

#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "patchinventorydialog.h"
#include "audioplugincache.h"
#include "rgbscriptscache.h"
#include "videoprovider.h"
#include "qlcconfig.h"
#include "helpbrowser.h"
#include "showcontrollistener.h"
#include "chaserstep.h"
#include "showcontrol.h"
#include "fixturegroup.h"
#include "qlcfile.h"
#include "apputil.h"
#include "wledconfigcheck.h"
#include "controlsurfaceengine.h"
#include "pmjoverlay.h"
#include "showstatus.h"
#include "showstatusdialog.h"
#include "showembedding.h"
#include "showdifferencesdialog.h"
#include "definitionupdatesdialog.h"
#include "qlcioplugin.h"
#include "startupwindow.h"
#include "mainbar.h"
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

#if defined(WIN32) || defined(Q_OS_WIN)
#   include "hotplugmonitor.h"
#endif

#if defined(__APPLE__) || defined(Q_OS_MAC)
extern void qt_set_sequence_auto_mnemonic(bool b);
#endif

#if defined(WIN32) || defined(Q_OS_WIN)
// Defined in Windows 11 headers but not in earlier versions.
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

typedef BOOL (WINAPI *SetProcessInformationType)(
    HANDLE hProcess,
    PROCESS_INFORMATION_CLASS ProcessInformationClass,
    LPVOID ProcessInformation,
    DWORD ProcessInformationSize
);
#endif

//#define DEBUG_SPEED

#ifdef DEBUG_SPEED
 #include <QTime>
 QTime speedTime;
#endif

#define SETTINGS_GEOMETRY      QStringLiteral("workspace/geometry")
#define SETTINGS_WORKINGPATH   QStringLiteral("workspace/workingpath")
#define SETTINGS_RECENTFILE    QStringLiteral("workspace/recent")
#define SETTINGS_AUTOSAVE_ENABLED  QStringLiteral("workspace/autosave/enabled")
#define SETTINGS_AUTOSAVE_INTERVAL QStringLiteral("workspace/autosave/interval")
#define SETTINGS_TAB_LABEL_MODE    QStringLiteral("workspace/tabLabelMode")
#define SETTINGS_THEME             QStringLiteral("workspace/theme")
#define SETTINGS_SHOW_FOOTER_LOAD  QStringLiteral("workspace/showFooterLoad")
#define SETTINGS_SHOW_FOOTER_POWER QStringLiteral("workspace/showFooterPower")
#define SETTINGS_SHOW_FOOTER_GM    QStringLiteral("workspace/showFooterGM")
#define SETTINGS_SHOW_FOOTER_SPEED QStringLiteral("workspace/showFooterSpeedMasters")
#define KXMLQLCWorkspaceWindow QStringLiteral("CurrentWindow")
#define KXMLQLCWorkspaceView QStringLiteral("View")

#define MAX_RECENT_FILES    10
#define DEFAULT_AUTOSAVE_INTERVAL 5  // 5 minutes

#define KModeTextOperate QObject::tr("Operate")
#define KModeTextDesign QObject::tr("Design")
#define KUniverseCount 4

// One beginStep() call per top-level phase in init()/initDoc() below --
// keep this in sync with the actual count so the StartupWindow's progress
// bar lands exactly on "full" when boot finishes, not short or wrapping.
#define STARTUP_STEP_COUNT 13

/*****************************************************************************
 * Initialization
 *****************************************************************************/

App::App()
    : QMainWindow()
    , m_tab(NULL)
    , m_overscan(false)
    , m_noGui(false)
    , m_loadProgressDialog(NULL)
    , m_startupWindow(NULL)
    , m_doc(NULL)

    , m_fileNewAction(NULL)
    , m_fileOpenAction(NULL)
    , m_fileImportAction(NULL)
    , m_fileSaveAction(NULL)
    , m_fileSaveAsAction(NULL)

    , m_modeToggleAction(NULL)
    , m_controlMonitorAction(NULL)
    , m_addressToolAction(NULL)
    , m_controlBlackoutAction(NULL)
    , m_controlBlindAction(NULL)
    , m_timelineSuspendAction(NULL)
    , m_followMtcAction(NULL)
    , m_lastLookAction(NULL)
    , m_autoMibAction(NULL)
    , m_clearLastLookAction(NULL)
    , m_controlPanicAction(NULL)
    , m_dumpDmxAction(NULL)
    , m_liveEditAction(NULL)
    , m_liveEditVirtualConsoleAction(NULL)
    , m_captureLiveEditsAction(NULL)
    , m_captureStoreAction(NULL)
    , m_captureUndoAction(NULL)

    , m_helpIndexAction(NULL)
    , m_helpAboutAction(NULL)
    , m_quitAction(NULL)
    , m_fileOpenMenu(NULL)
    , m_fadeAndStopMenu(NULL)

    , m_toolbar(NULL)

    , m_dumpProperties(NULL)
    , m_videoProvider(NULL)

    , m_autosaveTimer(NULL)
    , m_autosaveEnabled(true)
    , m_autosaveInterval(DEFAULT_AUTOSAVE_INTERVAL)

    , m_statusModeLabel(NULL)
    , m_statusDirtyLabel(NULL)
    , m_statusProgrammerLabel(NULL)
    , m_statusSelectionLabel(NULL)
    , m_statusPadModeLabel(NULL)
    , m_statusShowLockLabel(NULL)
    , m_statusBlackoutLabel(NULL)
    , m_statusTimecodeLabel(NULL)
    , m_statusLoadLabel(NULL)
    , m_statusTimelineLabel(NULL)
    , m_healthTimer(NULL)
{
    QCoreApplication::setOrganizationName("qlcplus");
    QCoreApplication::setOrganizationDomain("qlcplus.org");
    QCoreApplication::setApplicationName(APPNAME);
}

App::~App()
{
    QSettings settings;

    // Stop autosave timer
    if (m_autosaveTimer != NULL)
    {
        m_autosaveTimer->stop();
        delete m_autosaveTimer;
        m_autosaveTimer = NULL;
    }

    // Remove autosave file on clean exit (document was saved or discarded)
    if (m_doc != NULL && m_doc->isModified() == false)
        removeAutosaveFile();

    // Don't save kiosk-mode window geometry because that will screw things up
    if (m_doc->isKiosk() == false && QLCFile::hasWindowManager())
        settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    else
        settings.setValue(SETTINGS_GEOMETRY, QVariant());

    // A tab's borrowed tools go home before the tabs go.
    if (m_mainBar != NULL)
        m_mainBar->setHosting(false);

    if (Monitor::instance() != NULL)
        delete Monitor::instance();

    if (FixtureManager::instance() != NULL)
        delete FixtureManager::instance();

    if (FunctionManager::instance() != NULL)
        delete FunctionManager::instance();

    if (ShowManager::instance() != NULL)
        delete ShowManager::instance();

    if (InputOutputManager::instance() != NULL)
        delete InputOutputManager::instance();

    if (VirtualConsole::instance() != NULL)
        delete VirtualConsole::instance();

    if (SimpleDesk::instance() != NULL)
        delete SimpleDesk::instance();

    // The Programming tab is NOT a singleton like the other managers, so it
    // isn't torn down above — it would otherwise be destroyed by the tab widget
    // AFTER m_doc below, and its embedded ChaserEditor's destructor touches
    // m_doc->functions() → use-after-free / segfault on exit. Delete it (and its
    // editors) while the Doc is still alive.
    if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
        delete pm;

    // The Show Tree tab is the same trap: its hide event (the window still
    // showing when App goes) stops its preview through the Doc's
    // ShowTreeController, which is freed with m_doc below.
    if (ShowTreeView::instance() != NULL)
        delete ShowTreeView::instance();

    // Same ordering trap: the overlay and the engine are both children of App,
    // so QObject child deletion tears them down in construction order — engine
    // first — and ~PMJOverlay's unregisterDevice() then lands on freed memory.
    // Delete the overlay while its engine is still alive.
    if (m_pmjOverlay != NULL)
    {
        delete m_pmjOverlay;
        m_pmjOverlay = NULL;
    }

    if (m_controlSurfaceEngine != NULL)
    {
        delete m_controlSurfaceEngine;
        m_controlSurfaceEngine = NULL;
    }

    if (m_dumpProperties != NULL)
        delete m_dumpProperties;

    if (m_videoProvider != NULL)
        delete m_videoProvider;

    if (m_doc != NULL)
        delete m_doc;

    m_doc = NULL;
}

void App::startup(const QString &workspaceFile)
{
    // Applied here, before StartupWindow exists, rather than later in
    // init() (where this used to run) -- qApp->setPalette() inside
    // applyTheme() is application-wide, so doing it first means the
    // startup window itself opens already in the user's chosen theme
    // instead of flashing the native palette for the length of boot.
    QSettings settings;
    m_theme = settings.value(SETTINGS_THEME, ThemeDefault).toInt();
    applyTheme();

    if (m_noGui == false)
    {
        const int steps = STARTUP_STEP_COUNT + (workspaceFile.isEmpty() ? 0 : 1);
        m_startupWindow = new StartupWindow(steps);
        m_startupWindow->show();
    }

    init();

    if (m_startupWindow != NULL)
    {
        m_startupWindow->beginStep(tr("Checking output readiness"));
        const QList<InputOutputMap::DanglingPatch> dangling =
                m_doc->inputOutputMap()->danglingOutputPatches();
        m_startupWindow->logDetail(dangling.isEmpty()
                ? tr("All outputs ready")
                : tr("%1 pending patch(es)").arg(dangling.count()));
    }
    updateOutputReadiness();

    // Loaded here, still under the startup window, rather than by main()
    // after App::show() -- that used to pop a second, unbranded dialog
    // (createLoadProgressDialog) right after the main window appeared.
    // createLoadProgressDialog()/slotLoadProgress() detect m_startupWindow
    // is still alive and log into it instead of creating that dialog.
    if (workspaceFile.isEmpty() == false)
    {
        if (loadXML(workspaceFile) == QFile::NoError)
            updateFileOpenMenu(workspaceFile);
    }

    delete m_startupWindow;
    m_startupWindow = NULL;

    slotModeDesign();
    slotDocModified(false);

    // Activate FixtureManager
    setActiveWindow(FixtureManager::staticMetaObject.className());
}

void App::enableOverscan()
{
    m_overscan = true;
}

void App::disableGUI()
{
    m_noGui = true;
}


void App::init()
{
    QSettings settings;

    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Restoring window state"));

    // The top bar (initToolBar()) and the tabs, one above the other. The
    // bar is a plain row here rather than a QMainWindow toolbar: it holds a
    // tab's own QToolBar, and a toolbar inside a toolbar pays both their
    // margins (8 px of the height this bar exists to save).
    QWidget *central = new QWidget(this);
    central->setObjectName("MainCentral");
    QVBoxLayout *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    m_tab = new QTabWidget(central);
    m_tab->setObjectName("MainTabs");
    m_tab->setTabPosition(QTabWidget::South);
    /* A quiet line above the tabs about the view (showViewNotice()). */
    {
        QFrame *notice = new QFrame(central);
        notice->setObjectName(QStringLiteral("viewNotice"));
        notice->setFrameShape(QFrame::StyledPanel);
        QHBoxLayout *row = new QHBoxLayout(notice);
        row->setContentsMargins(8, 2, 4, 2);
        m_viewNoticeText = new QLabel(notice);
        m_viewNoticeText->setObjectName(QStringLiteral("viewNoticeText"));
        m_viewNoticeText->setWordWrap(true);
        row->addWidget(m_viewNoticeText, 1);
        m_viewNoticeButton = new QPushButton(notice);
        m_viewNoticeButton->setObjectName(QStringLiteral("viewNoticeButton"));
        row->addWidget(m_viewNoticeButton);
        QToolButton *close = new QToolButton(notice);
        close->setObjectName(QStringLiteral("viewNoticeClose"));
        close->setText(QStringLiteral("×"));
        close->setAutoRaise(true);
        close->setToolTip(tr("Dismiss"));
        row->addWidget(close);
        connect(m_viewNoticeButton, &QPushButton::clicked, this, [this]() {
            std::function<void()> act = m_viewNoticeAction;
            hideViewNotice();
            if (act)
                act();
        });
        connect(close, &QToolButton::clicked, this, &App::hideViewNotice);
        notice->hide();
        m_viewNotice = notice;
        centralLayout->addWidget(notice);
    }
    centralLayout->addWidget(m_tab, 1);
    setCentralWidget(central);

#if defined(__APPLE__) || defined(Q_OS_MAC)
    m_tab->setElideMode(Qt::TextElideMode::ElideNone);
    qt_set_sequence_auto_mnemonic(true);
#endif

    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid() == true)
    {
        this->restoreGeometry(var.toByteArray());

        // Clamp the restored window to the current screen: a geometry saved on
        // a bigger/other display (or made too tall by an old minimum size) can
        // land partly off-screen and become impossible to grab/resize.
        AppUtil::ensureWindowOnScreen(this);
    }
    else
    {
        /* Application geometry and window state */
        QSize size = settings.value("/workspace/size").toSize();
        if (size.isValid() == true)
        {
            resize(size);
        }
        else
        {
            if (QLCFile::hasWindowManager() == false)
            {
                QScreen *screen = QGuiApplication::screens().first();
                QRect geometry = screen->geometry();
                if (m_noGui == true)
                {
                    setGeometry(geometry.width(), geometry.height(), 1, 1);
                }
                else
                {
                    int w = geometry.width();
                    int h = geometry.height();
                    if (m_overscan == true)
                    {
                        // if overscan is requested, introduce a 5% margin
                        w = (float)geometry.width() * 0.95;
                        h = (float)geometry.height() * 0.95;
                    }
                    setGeometry((geometry.width() - w) / 2, (geometry.height() - h) / 2, w, h);
                }
            }
            else
                resize(800, 600);
        }

        QVariant state = settings.value("/workspace/state", Qt::WindowNoState);
        if (state.isValid() == true)
            setWindowState(Qt::WindowState(state.toInt()));
    }

    QVariant dir = settings.value(SETTINGS_WORKINGPATH);
    if (dir.isValid() == true)
        m_workingDirectory = QDir(dir.toString());

    // The engine object
    initDoc();
    // Main view actions
    initActions();
    // Slim live-control toolbar (the native menu bar is built in initMenuBar()
    // below, after the workspace tabs exist so its "View" tab-jumps match them).
    initToolBar();

    m_dumpProperties = new DmxDumpFactoryProperties(KUniverseCount);

    // Create primary views.
    m_tab->setIconSize(QSize(24, 24));

    // Helper to add a tab and record the original label/icon for mode switching.
    auto addTab = [this](QWidget *w, const QIcon &icon, const QString &text) {
        // QTabWidget's internal QStackedWidget sizes itself to the LARGEST
        // of ALL its pages by default (documented Qt behavior), not just
        // the currently-visible one — so whichever of the 8 tabs happens to
        // need the most width sets the floor for the whole window
        // regardless of which tab is active. Confirmed empirically: the
        // window's minimum width stayed identical across different active
        // tabs and was unaffected by trimming the status bar. Ignored on
        // both axes excludes a page from that max-of-all-pages aggregation
        // (it still renders and lays out normally once it IS the current
        // page — this only stops it from being counted while hidden).
        w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        const int idx = m_tab->addTab(w, icon, text);
        m_tabOriginals.append(qMakePair(text, icon));
        // F1 on this view opens its guide (helpTopicFor()).
        static const QHash<QString, QString> guide = {
            { QStringLiteral("Connections"),     QStringLiteral("other-improvements.md#connections--patch") },
            { QStringLiteral("Fixtures"),        QStringLiteral("fixture-manager.md") },
            { QStringLiteral("Lighting Studio"), QStringLiteral("lighting-studio.md") },
            { QStringLiteral("Functions"),       QStringLiteral("other-improvements.md#window-menu") },
            { QStringLiteral("Build"),           QStringLiteral("build.md") },
            { QStringLiteral("Show Tree"),       QStringLiteral("show-tree.md") },
            { QStringLiteral("Shows"),           QStringLiteral("shows.md") },
            { QStringLiteral("Virtual Console"), QStringLiteral("getting-started.md#running-the-show") },
            { QStringLiteral("Simple Desk"),     QStringLiteral("getting-started.md#running-the-show") },
            { QStringLiteral("Run"),             QStringLiteral("show-tree.md#the-cue-runner") } };
        if (!w->property("helpTopic").isValid())
            w->setProperty("helpTopic", guide.value(text, QStringLiteral("getting-started.md")));
        // Stored as tab data (travels with the tab through detach/reattach's
        // removeTab()/insertTab(), unlike m_tabOriginals' fixed construction-
        // order indexing) so the window title can find the right label
        // regardless of what's been detached before it.
        m_tab->tabBar()->setTabData(idx, text);
        if (m_startupWindow != NULL)
            m_startupWindow->logDetail(text);
    };

    // Tab order follows the build workflow: rig/setup first (Connections, then
    // the fixtures patched onto it, then Lighting Studio's physical layout) →
    // build content (Functions, Programming) → run it (Shows, Virtual Console,
    // Simple Desk).
    //
    // Connections comes first because it is the precondition for everything
    // after it: a fixture patched to a universe with no output is just a row
    // in a table. Setting up the signal path is genuinely the first thing you
    // do on a new rig, and the first thing you check when a show is dark.
    //
    // "Hardware" used to name the Fixture Manager, which was confusing while
    // sitting next to an I/O tab, and becomes plainly wrong now that I/O shows
    // the actual hardware -- interfaces, nodes, dongles. The fixture tab is
    // "Fixtures" (what it manages, and what every other console calls it) and
    // the I/O tab is "Connections" (the signal path, covering network, USB and
    // MIDI without implying any one of them).
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Building workspace"));
    QWidget* w = new InputOutputManager(m_tab, m_doc);
    addTab(w, QIcon(":/input_output.png"), tr("Connections"));
    w = new FixtureManager(m_tab, m_doc);
    addTab(w, QIcon(":/fixture.png"), tr("Fixtures"));
    // Lighting Studio (Monitor) used to be a lazily-created standalone
    // window (Qt::Window flag), created on first use via createAndShow()
    // and destroyed on close (WA_DeleteOnClose) — inconsistent with every
    // other tab, which is why it never picked up the app-wide title-bar/
    // tab-label-mode/detach conventions. Now a permanent tab like the rest,
    // constructed once here; still detachable into its own window via the
    // same double-click mechanism as any other tab if you want it visible
    // alongside another tab or on a second monitor.
    w = new Monitor(m_tab, m_doc);
    addTab(w, QIcon(":/grid.png"), tr("Lighting Studio"));
    // Functions: shown or hidden by setFunctionsWindowShown() (below).
    w = new FunctionManager(m_tab, m_doc);
    addTab(w, QIcon(":/function.png"), tr("Functions"));
    QPointer<ProgrammingManager> buildTab;
    {
        ProgrammingManager *pm = new ProgrammingManager(m_tab, m_doc);
        buildTab = pm;
        connect(pm, &ProgrammingManager::requestSave, this, &App::slotFileSave);
        connect(pm, &ProgrammingManager::powerEstimateChanged, this,
                [this](double amps, double kw, bool overload, const QString &detail) {
            if (m_statusPowerLabel == NULL)
                return;
            if (amps <= 0.0 && kw <= 0.0)   // cleared (Operate mode) → hide the chip
            {
                m_statusPowerLabel->hide();
                return;
            }
            if (m_showFooterPower == false)   // Settings ▸ Appearance: chip off
                return;
            m_statusPowerLabel->setText(overload
                ? tr("%1 A · %2 kW  OVERLOAD").arg(amps, 0, 'f', 1).arg(kw, 0, 'f', 2)
                : tr("%1 A · %2 kW").arg(amps, 0, 'f', 1).arg(kw, 0, 'f', 2));
            const QString sheet = overload
                ? QStringLiteral("QLabel { color: #ff5555; font-weight: bold; }")
                : QString();
            if (m_statusPowerLabel->styleSheet() != sheet)   // every 500 ms: only on a change
                m_statusPowerLabel->setStyleSheet(sheet);
            if (!detail.isEmpty() && m_statusPowerLabel->toolTip() != detail)
                m_statusPowerLabel->setToolTip(detail);
            m_statusPowerLabel->show();
        });
        w = pm;
    }
    if (ProgrammerController *pc = m_doc->programmer())
    {
        // View + Menu on the controller.
        connect(pc, &ProgrammerController::focusModeRequested, this, &App::slotFocusMode);
        connect(pc, &ProgrammerController::dangleFixturesChanged, this,
                [this](const QList<quint32> &fixtureIds) {
            if (m_statusDangleLabel == NULL)
                return;
            if (fixtureIds.isEmpty())
            {
                m_statusDangleLabel->hide();
                return;
            }
            m_statusDangleLabel->setText(tr("⚠︎ %1 marked fixture(s) not in upcoming cue")
                                          .arg(fixtureIds.size()));
            QStringList names;
            for (quint32 fid : fixtureIds)
            {
                Fixture *fxi = m_doc->fixture(fid);
                names << (fxi != NULL ? fxi->name() : tr("Fixture %1").arg(fid));
            }
            m_statusDangleLabel->setToolTip(names.join(QStringLiteral("\n")));
            m_statusDangleLabel->show();
        });
    }
    // Build (WORKFLOW_DESIGN.md §3.1; it was "Programming"): makes the
    // show's palettes, effects, scenes and collections.
    addTab(w, ItemIcons::buildIcon(), tr("Build"));   // a hard hat
    // The Show Tree beside Build (SHOW_TREE_DESIGN.md D2): writing a show as
    // one tree of cue lists, cues and layers -- what Build made, in order.
    {
        ShowTreeView *tree = new ShowTreeView(m_tab, m_doc);
        // W3: a cue opens in Build's Workbench, docked in the tree.
        tree->setWorkbenchLender(
            [buildTab]() -> Workbench * { return buildTab.isNull() ? nullptr : buildTab->lendWorkbench(); },
            [buildTab](Workbench *wb) {
                if (!buildTab.isNull())
                    buildTab->returnWorkbench(wb);
            });
        w = tree;
    }
    addTab(w, QIcon(":/chaser.png"), tr("Show Tree"));
    w = new ShowManager(m_tab, m_doc);
    addTab(w, QIcon(":/show.png"), tr("Shows"));
    w = new VirtualConsole(m_tab, m_doc);
    addTab(w, QIcon(":/virtualconsole.png"), tr("Virtual Console"));
    w = new SimpleDesk(m_tab, m_doc);
    addTab(w, QIcon(":/slidermatrix.png"), tr("Simple Desk"));
    /* The cue runner's full-screen home (SHOW_TREE_DESIGN.md §6B, D26):
       the operator's view -- big targets, keyboard GO, nothing to bump. It
       follows the current production; the Cue Runner VC widget is its
       other home. */
    w = new CueRunner(m_doc, CueRunner::Full, m_tab);
    w->setObjectName(QStringLiteral("runTab"));
    addTab(w, QIcon(":/chaser.png"), tr("Run"));
    /* The view (View ▸ Workspace): qlcconsole until a show says otherwise.
       Functions has no tab in it (board Q32): Window → Functions opens
       Build ▸ Scenes ▸ Classic, where everything it did lives. */
    setWorkspaceView(ConsoleView, false);
    // Load and apply the tab label mode preference.
    {
        QSettings settings;
        m_tabLabelMode = settings.value(SETTINGS_TAB_LABEL_MODE, TabIconAndText).toInt();
    }
    applyTabLabelMode();

    // Theme is loaded/applied earlier now, in App::startup() before this
    // window and the StartupWindow are constructed -- see the comment
    // there. Nothing to do here anymore.

    // Load the footer load/power/GM chip visibility preferences (View menu).
    {
        QSettings settings;
        m_showFooterLoad = settings.value(SETTINGS_SHOW_FOOTER_LOAD, true).toBool();
        m_showFooterPower = settings.value(SETTINGS_SHOW_FOOTER_POWER, true).toBool();
        m_showFooterGM = settings.value(SETTINGS_SHOW_FOOTER_GM, true).toBool();
        m_showFooterSpeed = settings.value(SETTINGS_SHOW_FOOTER_SPEED, true).toBool();
    }

    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Finalizing UI"));

    // Build the native menu bar now that the workspace tabs exist, so the
    // View menu can offer accurate "jump to tab" entries.
    initMenuBar();

    /* Shift-click a tab: its view in a window of its own (eventFilter()). It
       was a double-click, which broke views out by accident. */
    m_tab->tabBar()->installEventFilter(this);
    connect(m_tab, SIGNAL(currentChanged(int)), this, SLOT(slotTabChanged(int)));
    // The tab on show lends its own tools to the top bar.
    if (m_mainBar != NULL)
        m_mainBar->hostTabTools(m_tab->currentWidget());

    // Listen to blackout changes and toggle m_controlBlackoutAction
    connect(m_doc->inputOutputMap(), SIGNAL(blackoutChanged(bool)), this, SLOT(slotBlackoutChanged(bool)));
    connect(m_doc->inputOutputMap(), SIGNAL(outputInhibitedChanged(bool)), this, SLOT(slotOutputInhibitedChanged(bool)));
    /* A patch kept offline reconnected (its network appeared): the footer
       notice has to drop it without waiting for the next mode change. */
    connect(m_doc->inputOutputMap(), &InputOutputMap::offlinePatchesChanged,
            this, [this]() { updateOutputReadiness(); updateRigNetworkFooter(); });

    // Listen to DMX value changes and update each Fixture values array
    connect(m_doc->inputOutputMap(), SIGNAL(universeWritten(quint32, const QByteArray&)),
            this, SLOT(slotUniverseWritten(quint32, const QByteArray&)));

    // Enable/Disable panic button
    connect(m_doc->masterTimer(), SIGNAL(functionListChanged()), this, SLOT(slotRunningFunctionsChanged()));
    slotRunningFunctionsChanged();

    // Start up in non-modified state
    m_doc->resetModified();

#if defined(WIN32) || defined(Q_OS_WIN)
    HotPlugMonitor::setWinId(winId());
    
    // When on Windows 11, disable system timer resolution throttling when
    // app is minimised, occluded, etc.
    disableTimerResolutionThrottling();
#endif

    this->setStyleSheet(AppUtil::getStyleSheet("MAIN"));

    m_videoProvider = new VideoProvider(m_doc, this);

    // The rig network transmit gate is this machine's choice: on unless
    // turned off in Settings. (The networks themselves come with the show.)
    m_doc->inputOutputMap()->setTransmitGateEnabled(
            QSettings().value(rigNetworkGateSettingKey(), true).toBool());

    // Initialize status bar
    initStatusBar();

    // The show controls: top bar (default) or status bar. After the status
    // bar, which the second puts them in.
    m_showControlsPlacement = QSettings().value(showControlsPlacementKey(),
                                                ShowControlsInTopBar).toInt();
    applyShowControlsPlacement();

    // Initialize autosave
    initAutosave();
}

void App::setActiveWindow(const QString& name)
{
    if (name.isEmpty() == true)
        return;
    // A show saved with the Functions window in front: its home now.
    if (name == QLatin1String(FunctionManager::staticMetaObject.className()))
    {
        showFunctions();
        return;
    }

    for (int i = 0; i < m_tab->count(); i++)
    {
        QWidget* widget = m_tab->widget(i);
        if (widget != NULL && widget->metaObject()->className() == name)
        {
            m_tab->setCurrentIndex(i);
            break;
        }
    }
}

#if defined(WIN32) || defined(Q_OS_WIN)
/* Qt 6 takes the result as qintptr*. Declared with Qt 5's long* this never
   overrode anything, so WM_DEVICECHANGE never reached HotPlugMonitor and a
   USB device plugged in after launch went unnoticed on Windows. */
bool App::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
    long res = 0;
    const bool handled = HotPlugMonitor::parseWinEvent(message, &res);
    if (handled && result != NULL)
        *result = res;
    return handled;
}

void App::disableTimerResolutionThrottling()
{
    // On Windows 11, we want it to always honour system timer resolution requests,
    // because otherwise by default when an application is minimised, or otherwise
    // non-visible or non-audible to the end-user, Windows may ignore timer
    // resolution requests and not give a higher resolution than the default system
    // timer resolution (typically 15.625 ms).
    
    // Note: we must resolve the SetProcessInformation API function at run-time
    // because it does not exist prior to Windows 8. On supported Windows versions
    // earlier than 11, the call to SetProcessInformation will just fail, which we
    // can ignore.
    
    HMODULE hKernel32 = LoadLibrary(L"kernel32.dll");
    Q_ASSERT(hKernel32 != NULL); // Shouldn't ever fail because kernel32 already loaded into every process

    // Extra void* cast to avoid -Wcast-function-type warning.
    SetProcessInformationType pfnSetProcessInformation = (SetProcessInformationType)(void *)GetProcAddress(hKernel32, "SetProcessInformation");

    if (pfnSetProcessInformation != NULL)
    {
        PROCESS_POWER_THROTTLING_STATE pwrState = {
                .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                .ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
                .StateMask = 0 // Disables timer resolution throttling
        };

        if (!pfnSetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pwrState, sizeof(pwrState)))
        {
            qWarning() << Q_FUNC_INFO << "SetProcessInformation() failed with error" << GetLastError() << "(ignore if Windows version < 11)";
        }
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "SetProcessInformation() API does not exist on this version of Windows";
    }
}
#endif

void App::closeEvent(QCloseEvent* e)
{
    if (m_doc->mode() == Doc::Operate && m_doc->isKiosk() == false)
    {
        QMessageBox::warning(this,
                             tr("Cannot exit in Operate mode"),
                             tr("You must switch back to Design mode " \
                                "to close the application."));
        e->ignore();
        return;
    }

    if (m_doc->isKiosk() == false)
    {
        if (saveModifiedDoc(tr("Close"), tr("Do you wish to save the current workspace " \
                                            "before closing the application?")) == true)
        {
            e->accept();
            // A staged update goes in now that qlcconsole is quitting -- and
            // only if nothing is running (UpdateManager::aboutToQuit()). The
            // helper waits for this process to end: make sure it does, even
            // with a help or detached window still open.
            if (m_updateManager != nullptr
                && m_updateManager->aboutToQuit(m_restartForUpdate, fileName()))
            {
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            }
        }
        else
        {
            e->ignore();
        }
    }
    else
    {
        if (m_doc->isKiosk() == true)
        {
            int result = QMessageBox::warning(this, tr("Close the application?"),
                                              tr("Do you wish to close the application?"),
                                              QMessageBox::Yes, QMessageBox::No);
            if (result == QMessageBox::No)
            {
                e->ignore();
                return;
            }
        }

        e->accept();
    }
}

/*****************************************************************************
 * Progress dialog
 *****************************************************************************/

void App::createLoadProgressDialog(const QString& fileName)
{
    if (m_noGui == true)
        return;

    // Called during App::startup()'s own workspaceFile load too (loadXML()
    // doesn't know which case it's in) -- log into the still-open
    // StartupWindow instead of a second dialog, and let App::startup()
    // close that window itself once the load returns.
    if (m_startupWindow != NULL)
    {
        m_startupWindow->beginStep(tr("Loading workspace"));
        m_startupWindow->logDetail(QFileInfo(fileName).fileName());
        return;
    }

    if (m_loadProgressDialog != NULL)
        return;

    m_loadProgressDialog = new QProgressDialog(this);
    m_loadProgressDialog->setWindowModality(Qt::ApplicationModal);
    m_loadProgressDialog->setCancelButton(NULL);
    // Busy indicator, not a percentage: the number of fixtures/functions in a
    // workspace isn't known until the file has been parsed, and a made-up
    // percentage is worse than an honest spinner.
    m_loadProgressDialog->setRange(0, 0);
    m_loadProgressDialog->setMinimumDuration(0);
    m_loadProgressDialog->setLabelText(QString("<B>%1</B><BR/>%2")
                                       .arg(tr("Loading workspace"))
                                       .arg(QFileInfo(fileName).fileName()));
    m_loadProgressDialog->show();
    m_loadProgressDialog->raise();
    QApplication::processEvents();
}

void App::destroyLoadProgressDialog()
{
    // Nothing to close: this load ran under StartupWindow, which
    // App::startup() owns closing once loadXML() returns to it.
    if (m_startupWindow != NULL)
        return;

    delete m_loadProgressDialog;
    m_loadProgressDialog = NULL;
}

void App::slotLoadProgress(const QString& stage, int count)
{
    if (m_startupWindow != NULL)
    {
        m_startupWindow->logDetail(count > 0 ? QStringLiteral("%1 (%2)").arg(stage).arg(count)
                                              : stage);
        return;
    }

    if (m_loadProgressDialog == NULL)
        return;

    m_loadProgressDialog->setLabelText(QString("<B>%1</B><BR/>%2")
                                       .arg(stage)
                                       .arg(count));
    QApplication::processEvents();
}

/*****************************************************************************
 * Doc
 *****************************************************************************/

void App::clearDocument()
{
    m_doc->masterTimer()->stop();
    VirtualConsole::instance()->resetContents();
    ShowManager::instance()->clearContents();
    m_doc->clearContents();
    applyUnitPreference(false);
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
    SimpleDesk::instance()->clearContents();
    m_doc->inputOutputMap()->resetUniverses();
    setFileName(QString());
    // A new show: the qlcconsole view (a loaded one sets its own, loadXML()).
    m_loadedCreator.clear();
    m_upgradeOfferMade = false;
    hideViewNotice();
    setWorkspaceView(ConsoleView, false);
    m_doc->resetModified();
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
}

void App::applyUnitPreference(bool fromShow)
{
    MonitorProperties *mp = m_doc->monitorProperties();
    if (fromShow)
        MeasurementUnits::seedFromFirstShow(mp->gridUnits() == MonitorProperties::Feet);
    /* The show's own Units only said what its grid size was in; from here
       on it's the person's, and is saved that way. */
    mp->convertGridUnits(MonitorProperties::gridUnitsFor(MeasurementUnits::preferredLength()));
}

Doc *App::doc()
{
    return m_doc;
}

void App::initDoc()
{
    Q_ASSERT(m_doc == NULL);
    m_doc = new Doc(this);

    connect(m_doc, SIGNAL(modified(bool)), this, SLOT(slotDocModified(bool)));
    connect(m_doc->showEmbedding(), &ShowEmbedding::changed, this,
            [this]() { AppUtil::updateShowEmbeddingStatus(m_doc); });
    connect(m_doc, SIGNAL(needAutosave()), this, SLOT(slotDocAutosave()));
    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged(Doc::Mode)));
    connect(m_doc, SIGNAL(loadProgress(QString,int)), this, SLOT(slotLoadProgress(QString,int)));

    /* Fixtures not patched, or whose definition says nothing about how much
       light they put out, go in the footer's "Not ready" list. Re-checked a
       moment after any fixture change -- a load or a bulk patch is hundreds
       of changes, and one check covers them all. */
    QTimer *fixtureReadiness = new QTimer(this);
    fixtureReadiness->setObjectName(QStringLiteral("fixtureReadinessTimer"));
    fixtureReadiness->setSingleShot(true);
    fixtureReadiness->setInterval(200);
    connect(fixtureReadiness, &QTimer::timeout, this, [this]() {
        AppUtil::updateFixtureReadiness(m_doc);
    });
    auto recheckFixtures = [fixtureReadiness]() { fixtureReadiness->start(); };
    connect(m_doc, &Doc::fixtureAdded, fixtureReadiness, recheckFixtures);
    connect(m_doc, &Doc::fixtureRemoved, fixtureReadiness, recheckFixtures);
    connect(m_doc, &Doc::fixtureChanged, fixtureReadiness, recheckFixtures);
    connect(m_doc, &Doc::loaded, fixtureReadiness, recheckFixtures);
    /* Definitions of the user's older than the current version of the same
       fixture: checked when a show has loaded (which of them it uses). */
    connect(m_doc, &Doc::loaded, this, [this]() {
        QTimer::singleShot(0, this, [this]() { AppUtil::updateDefinitionUpdatesStatus(m_doc); });
    });
    // An effect look that would not start: the footer, a moment later.
    if (m_doc->effectScriptRunner() != nullptr)
        connect(m_doc->effectScriptRunner(), &EffectScriptRunner::failuresChanged,
                fixtureReadiness, recheckFixtures);

    /* Each WLED read for whether it is set up the way this show drives it
       (WledConfigCheck): what does not match goes in the footer and on the
       fixture, with Fix on Device. Read again a moment after the show loads
       or a fixture changes (a patch, a mode), and every so often anyway. */
    WledConfigCheck *wledCheck = new WledConfigCheck(m_doc, this);

    /* Cues fired from outside (QLab): OSC on a port of our own, MSC on a
       patched MIDI input (SHOW_CONTROL_DESIGN.md). */
    m_showControlListener = new ShowControlListener(m_doc, this);
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->setShowControlListener(m_showControlListener);
    QTimer *wledRecheck = new QTimer(this);
    wledRecheck->setSingleShot(true);
    wledRecheck->setInterval(1500);
    connect(wledRecheck, &QTimer::timeout, wledCheck, &WledConfigCheck::checkNow);
    auto recheckWled = [wledRecheck]() { wledRecheck->start(); };
    connect(m_doc, &Doc::loaded, wledRecheck, recheckWled);
    connect(m_doc, &Doc::fixtureAdded, wledRecheck, recheckWled);
    connect(m_doc, &Doc::fixtureChanged, wledRecheck, recheckWled);
    connect(m_doc, &Doc::fixtureRemoved, wledRecheck, recheckWled);
    wledRecheck->start();
    // Once at startup too: files skipped while the libraries loaded (in cloud
    // storage, not on this computer) are worth saying with no show open.
    fixtureReadiness->start();

    /* ...and each can be fixed from there: the unpatched selected and
       patched, a definition opened to say what it puts out. */
    ShowStatus::instance()->setAction("simpledesk.held", tr("Release"),
        [](QWidget *, const QVariantList &items) {
            if (SimpleDesk *desk = SimpleDesk::instance())
            {
                foreach (const QVariant &v, items)
                    desk->releaseUniverse(v.toInt());
            }
        });
    ShowStatus::instance()->setAction("wled.config", tr("Fix on Device…"),
        [](QWidget *parent, const QVariantList &items) {
            QList<quint32> ids;
            foreach (const QVariant &v, items)
                ids << v.toUInt();
            if (WledConfigCheck::instance() != nullptr)
                WledConfigCheck::instance()->confirmFix(parent, ids);
        });
    ShowStatus::instance()->setAction("fixtures.unpatched", tr("Patch…"),
        [this](QWidget *parent, const QVariantList &items) {
            QList<quint32> ids;
            foreach (const QVariant &v, items)
                ids << v.toUInt();
            PatchInventoryDialog dlg(m_doc, ids, parent);
            dlg.exec();
            AppUtil::updateFixtureReadiness(m_doc);
        });
    ShowStatus::instance()->setAction("fixtures.output", tr("Edit Definition…"),
        [this](QWidget *parent, const QVariantList &items) {
            QStringList files;
            foreach (const QVariant &v, items)
                if (!files.contains(v.toString()))
                    files << v.toString();
            foreach (const QString &file, files)
            {
                if (file.isEmpty())
                {
                    QMessageBox::information(parent, tr("No definition to edit"),
                        tr("A generic fixture has no definition file. Change it to "
                           "its real model in the Fixture Manager to give it one."));
                    continue;
                }
                watchDefinition(file);
                AppUtil::launchFixtureEditor(file, parent);
            }
        });
    ShowStatus::instance()->setAction("fixtures.clash", tr("Re-patch…"),
        [this](QWidget *parent, const QVariantList &items) {
            QList<quint32> ids;
            foreach (const QVariant &v, items)
                ids << v.toUInt();
            PatchInventoryDialog dlg(m_doc, ids, parent);
            dlg.exec();
            AppUtil::updateFixtureReadiness(m_doc);
        });
    ShowStatus::instance()->setAction("fixtures.duplicates", tr("Number Them"),
        [this](QWidget *, const QVariantList &items) {
            QStringList names;
            foreach (const QVariant &v, items)
                names << v.toString();
            m_doc->makeFixtureNamesUnique(names);
            AppUtil::updateFixtureReadiness(m_doc);
        });
    /* Opened in the fixture editor, which says where the file breaks and
       offers repairs; saved there, it is watched and loads. */
    ShowStatus::instance()->setAction("definitions.failed", tr("Repair in Fixture Editor…"),
        [this](QWidget *parent, const QVariantList &items) {
            foreach (const QVariant &v, items)
            {
                const QString file = v.toString();
                watchDefinition(file);
                AppUtil::launchFixtureEditor(file, parent);
            }
        });
    auto trashDefinitions = [this](QWidget *parent, const QVariantList &items) {
            QStringList files, names;
            foreach (const QVariant &v, items)
                if (!files.contains(v.toString()))
                {
                    files << v.toString();
                    names << QFileInfo(v.toString()).fileName();
                }
            if (files.isEmpty()
                || QMessageBox::question(parent, tr("Move to Trash"),
                       tr("Move %n definition file(s) to the Trash?\n\n%1", "", files.size())
                           .arg(names.join("\n")),
                       QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
                return;
            QStringList failed;
            foreach (const QString &file, files)
            {
                if (QFile::moveToTrash(file))
                    m_doc->fixtureDefCache()->forgetFailedDefinition(file);
                else
                    failed << QFileInfo(file).fileName();
            }
            if (!failed.isEmpty())
                QMessageBox::warning(parent, tr("Move to Trash"),
                    tr("Could not move these to the Trash:\n\n%1").arg(failed.join("\n")));
            AppUtil::updateFixtureReadiness(m_doc);
        };
    ShowStatus::instance()->addAction("definitions.failed", tr("Move to Trash…"), trashDefinitions);
    /* Online-only files: fetch them without holding anything up -- a thread
       of their own reads each one (which makes the cloud send it), however
       long that takes. Fixtures and effects then load at once; anything else
       loads next time qlcconsole starts. */
    /* What the show carries: keep the ones only it has; review the ones that
       differ from this computer's (SHOW_EMBEDDING_DESIGN.md). */
    ShowStatus::instance()->setAction("embedded.showonly", tr("Install on This Computer"),
        [this](QWidget *parent, const QVariantList &items) {
            QStringList failed;
            foreach (const QVariant &v, items)
            {
                QString error;
                if (!m_doc->showEmbedding()->installShows(v.toString(), &error))
                    failed << error;
            }
            if (!failed.isEmpty())
                QMessageBox::warning(parent, tr("Install on This Computer"), failed.join("\n"));
        });
    ShowStatus::instance()->setAction("embedded.differ", tr("Review…"),
        [this](QWidget *parent, const QVariantList &) {
            ShowDifferencesDialog dlg(m_doc, parent);
            dlg.exec();
        });
    ShowStatus::instance()->setAction("definitions.outofdate", tr("Review…"),
        [this](QWidget *parent, const QVariantList &items) {
            DefinitionUpdatesDialog dlg(m_doc, parent);
            QStringList files;
            foreach (const QVariant &v, items)
                files << v.toString();
            dlg.select(files);
            dlg.exec();
            AppUtil::updateDefinitionUpdatesStatus(m_doc);
        });
    ShowStatus::instance()->setAction("fixtures.nodefinition", tr("Fetch Definitions"),
        [this](QWidget *parent, const QVariantList &) { recoverUnavailableDefinitions(parent); });
    ShowStatus::instance()->setAction("storage.onlineonly", tr("Download Now"),
        [this](QWidget *parent, const QVariantList &items) {
            QStringList paths;
            foreach (const QVariant &v, items)
                paths << v.toString();
            if (paths.isEmpty())
                return;
            QPointer<App> self(this);
            QPointer<QWidget> where(parent);
            std::thread([self, where, paths]() {
                QStringList fetched;
                for (const QString &path : paths)
                {
                    QFile f(path);
                    if (f.open(QIODevice::ReadOnly))
                    {
                        f.readAll();
                        f.close();
                    }
                    if (!QLCFile::isOnlineOnly(path))
                        fetched << path;
                }
                QMetaObject::invokeMethod(qApp, [self, where, paths, fetched]() {
                    if (self.isNull())
                        return;
                    bool scripts = false;
                    QStringList later;
                    for (const QString &path : fetched)
                    {
                        QLCFile::forgetSkippedOnlineOnly(path);
                        if (path.endsWith(QLatin1String(".qxf"), Qt::CaseInsensitive))
                            self->m_doc->fixtureDefCache()->loadQXF(path, true);
                        else if (path.endsWith(QLatin1String(".js"), Qt::CaseInsensitive))
                            scripts = true;
                        else
                            later << QFileInfo(path).fileName();
                    }
                    if (scripts && self->m_doc->effectScriptRunner() != nullptr)
                        self->m_doc->effectScriptRunner()->cache()->rescan();
                    AppUtil::updateFixtureReadiness(self->m_doc);
                    QString text = tr("Downloaded %1 of %2.").arg(fetched.size()).arg(paths.size());
                    if (fetched.size() < paths.size())
                        text += QLatin1Char(' ') + tr("The rest did not come -- is the cloud storage "
                                                      "connected?");
                    if (!later.isEmpty())
                        text += QLatin1Char(' ') + tr("These load next time qlcconsole starts: %1")
                                                       .arg(later.join(QStringLiteral(", ")));
                    QMessageBox::information(where, tr("Cloud storage"), text);
                });
            }).detach();
        });
    ShowStatus::instance()->setAction("definitions.duplicates", tr("Move to Trash…"), trashDefinitions);
    auto openConnections = [this](QWidget *parent, const QVariantList &items) {
            InputOutputManager *io = InputOutputManager::instance();
            if (io == NULL || items.isEmpty())
                return;
            switchToTabContaining(io);
            io->selectUniverse(items.first().toUInt());
            if (QDialog *dlg = qobject_cast<QDialog *>(parent))
                dlg->accept();          // out of the way of the tab it opened
        };
    ShowStatus::instance()->setAction("universes.nooutput", tr("Open Connections"), openConnections);
    ShowStatus::instance()->setAction("outputs.problems", tr("Open Connections"), openConnections);
    // The Fixture Manager's universe rows: "Show in Connections".
    ShowStatus::instance()->setAction("connections.universe", tr("Show in Connections"), openConnections);
#ifdef DEBUG_SPEED
    speedTime.start();
#endif
    /* Surface any user content left under the old QLC+-named data dir into
       the current (qlcconsole) one, so the rename doesn't orphan it */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Merging legacy user data"));
    QLCFile::mergeLegacyUserData();

    /* Load user fixtures first so that they override system fixtures */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading fixture definitions"));
    m_doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    m_doc->fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());
    if (m_startupWindow != NULL)
        m_startupWindow->logDetail(tr("%1 manufacturer(s)")
                                    .arg(m_doc->fixtureDefCache()->manufacturers().count()));
    QTimer::singleShot(0, this, [this]() { AppUtil::updateDefinitionUpdatesStatus(m_doc); });

    /* Load channel modifiers templates */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading channel modifiers"));
    m_doc->modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
    m_doc->modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());
    if (m_startupWindow != NULL)
        m_startupWindow->logDetail(tr("%1 template(s)")
                                    .arg(m_doc->modifiersCache()->templateNames().count()));

    /* Load RGB scripts */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading RGB scripts"));
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::systemScriptsDirectory());
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());
    if (m_startupWindow != NULL)
        m_startupWindow->logDetail(tr("%1 script(s)")
                                    .arg(m_doc->rgbScriptsCache()->names().count()));

    /* Load plugins */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading I/O plugins"));
    connect(m_doc->ioPluginCache(), &IOPluginCache::pluginLoaded, this,
            [this](const QString &name) {
        if (m_startupWindow != NULL)
            m_startupWindow->logDetail(name);
    });
    m_doc->ioPluginCache()->load(IOPluginCache::systemPluginDirectory());
    /* Outputs in trouble -- WLED devices that stop answering or fade to a
       weak signal, Art-Net / E1.31 outputs that can't send, Art-Net nodes
       that stop answering: the footer, whichever plugin says so. */
    /* By name, not by member pointer: QLCIOPlugin is compiled into the engine
       and into every plugin, so on Windows its signal has no single exporting
       DLL, and a pointer-to-member connection there silently never fires. */
    if (QTimer *t = findChild<QTimer *>(QStringLiteral("fixtureReadinessTimer")))
    {
        foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
            connect(plugin, SIGNAL(targetProblemsChanged()), t, SLOT(start()));
    }

    /* Load audio decoder plugins
     * This doesn't use a AudioPluginCache::systemPluginDirectory() cause
     * otherwise the qlcconfig.h creation should have been moved into the
     * audio folder, which doesn't make much sense */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading audio plugins"));
    m_doc->audioPluginCache()->load(QLCFile::systemDirectory(AUDIOPLUGINDIR, KExtPlugin));
    if (m_startupWindow != NULL)
        m_startupWindow->logDetail(tr("%1 audio device(s)")
                                    .arg(m_doc->audioPluginCache()->audioDevicesList().count()));

    /* Restore outputmap settings */
    Q_ASSERT(m_doc->inputOutputMap() != NULL);

    /* Load input plugins & profiles */
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Loading input profiles"));
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::userProfileDirectory());
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::systemProfileDirectory());
    m_doc->inputOutputMap()->loadDefaults();
    if (m_startupWindow != NULL)
        m_startupWindow->logDetail(tr("%1 profile(s)")
                                    .arg(m_doc->inputOutputMap()->profileNames().count()));

#ifdef DEBUG_SPEED
    qDebug() << "[App] Doc initialization took" << speedTime.elapsed() << "ms";
#endif

    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Starting universes"));
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();

    // Control-surface engine (CONTROL_SURFACE_DESIGN.md): device-agnostic
    // core, plus the PMJ Black 1 overlay. Registering the PMJ here doesn't
    // require the board to actually be connected/patched — it just means
    // its role table + LED sink are ready the moment it is.
    if (m_startupWindow != NULL)
        m_startupWindow->beginStep(tr("Initializing control surfaces"));
    m_controlSurfaceEngine = new ControlSurfaceEngine(this);
    m_pmjOverlay = new PMJOverlay(m_doc, m_controlSurfaceEngine, this, this);
}

void App::watchDefinition(const QString &file)
{
    if (m_definitionWatcher == nullptr)
    {
        m_definitionWatcher = new QFileSystemWatcher(this);
        connect(m_definitionWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
            /* After the editor has finished writing -- and an editor that
               saves by replacing the file drops it from the watch, so it is
               put back. */
            QTimer::singleShot(300, this, [this, path]() {
                if (QFile::exists(path) && !m_definitionWatcher->files().contains(path))
                    m_definitionWatcher->addPath(path);
                // A file that would not load: try it again.
                if (m_doc->fixtureDefCache()->failedDefinitions().contains(path))
                    m_doc->fixtureDefCache()->loadQXF(path, true);
                QSet<QLCFixtureDef *> defs;
                foreach (Fixture *fx, m_doc->fixtures())
                    if (fx != NULL && fx->fixtureDef() != NULL
                        && fx->fixtureDef()->definitionSourceFile() == path)
                        defs.insert(fx->fixtureDef());
                foreach (QLCFixtureDef *def, defs)
                    m_doc->fixtureDefCache()->reloadPhysical(def);
                AppUtil::updateFixtureReadiness(m_doc);
            });
        });
    }
    if (!m_definitionWatcher->files().contains(file))
        m_definitionWatcher->addPath(file);
}

void App::updateOutputReadiness()
{
    if (m_doc == NULL)
        return;

    const QList<InputOutputMap::DanglingPatch> bad =
        m_doc->inputOutputMap()->danglingOutputPatches();

    if (bad.isEmpty() == true)
    {
        ShowStatus::instance()->clearStatus("output.dangling");
        return;
    }

    QStringList items;
    int rangeCount = 0;
    QSet<quint32> keptUniverses;
    QSet<quint32> heldUniverses;
    QStringList missing;
    foreach (const InputOutputMap::DanglingPatch &p, bad)
    {
        if (p.heldReason.isEmpty() == false)
        {
            // Held by the rig network gate: the red footer chip of its own
            // (updateRigNetworkFooter()) says why; listed here too.
            heldUniverses.insert(p.universe);
            items << tr("Universe %1 — %2 output held: %3")
                        .arg(p.universe + 1).arg(p.pluginName, p.heldReason);
        }
        else if (p.missingInterface.isEmpty() == false)
        {
            // Kept offline: the interface/network (an Art-Net address, most
            // often), device or plugin this patch names is not present here
            // -- typically a laptop away from the show network. The patch is
            // untouched, saved unchanged, and reconnects by itself.
            keptUniverses.insert(p.universe);
            if (missing.contains(p.missingInterface) == false)
                missing << p.missingInterface;
            const QString kind = p.kind == "input" ? tr("input")
                               : p.kind == "feedback" ? tr("feedback") : tr("output");
            items << (p.pluginMissing
                ? tr("Universe %1 — %2 %3 \"%4\": plugin not installed here — kept")
                      .arg(p.universe + 1).arg(kind, p.pluginName, p.missingInterface)
                : tr("Universe %1 — %2 %3 \"%4\" not present — kept, will reconnect")
                      .arg(p.universe + 1).arg(kind, p.pluginName, p.missingInterface));
        }
        else
        {
            rangeCount++;
            items << tr("Universe %1 — %2 line %3 does not exist here (offers %4)")
                        .arg(p.universe + 1).arg(p.pluginName).arg(p.line).arg(p.availableLines);
        }
    }

    // The summary line (shown in the footer chip's tooltip) explains the
    // SITUATION, not just which universes -- "has no output" read like a
    // configuration mistake to fix, when on a laptop away from the show
    // network it is simply expected and will resolve itself back on-site.
    QStringList summaryParts;
    if (keptUniverses.isEmpty() == false)
        summaryParts << tr("%1 universe%2 patched to interfaces not present (%3) — kept, will reconnect")
                            .arg(keptUniverses.count())
                            .arg(keptUniverses.count() == 1 ? QString() : "s")
                            .arg(missing.join(", "));
    if (rangeCount > 0)
        summaryParts << tr("%1 universe%2 patched to a line this machine doesn't have")
                            .arg(rangeCount).arg(rangeCount == 1 ? QString() : "s");
    if (heldUniverses.isEmpty() == false)
        summaryParts << tr("%n universe(s) held — not on the rig network", "",
                           heldUniverses.count());

    // Registered into ShowStatus rather than written straight to the footer
    // -- see showstatus.h. items carries the per-universe breakdown as an
    // actual list for the full-detail dialog; detail is just the one
    // closing sentence that applies to all of them.
    ShowStatus::instance()->setStatus("output.dangling", ShowStatus::Warning,
        summaryParts.join(" · "),
        rangeCount > 0
            ? tr("Re-patch these universes in Connections, or open the workspace "
                 "on the machine it was built for.")
            : tr("Patches follow the network: they reconnect by themselves when "
                 "any interface on that network appears. Nothing is sent meanwhile."),
        items);

    foreach (const QString &line, items)
        qWarning() << "[output]" << line;
}

void App::slotDocModified(bool state)
{
    updateWindowTitle();

    /* Saving changes what the programmer chip should SAY without changing what
       the programmer holds, so it has to be re-rendered here too -- otherwise
       it keeps its red "unsaved" styling until the next unrelated programmer
       event. */
    if (m_doc != NULL)
        slotProgrammerDirtyChanged(m_doc->hasProgrammerValues()
                                   || !m_doc->editedSceneIds().isEmpty());

    if (m_statusDirtyLabel != NULL)
    {
        if (state == true)
        {
            m_statusDirtyLabel->setText(tr("● Unsaved changes"));
            m_statusDirtyLabel->setStyleSheet("QLabel { color: #d08020; font-weight: bold; }");
        }
        else
        {
            m_statusDirtyLabel->setText(tr("✓ Saved"));
            m_statusDirtyLabel->setStyleSheet("QLabel { color: gray; }");
        }
    }
}

void App::updateWindowTitle()
{
    QString caption(APPNAME);

    if (fileName().isEmpty() == false)
        caption += QString(" - ") + QDir::toNativeSeparators(fileName());
    else
        caption += tr(" - New Workspace");

    if (m_doc != NULL && m_doc->isModified())
        caption += QString(" *");

    // m_tab->tabText() is empty under Icons-Only tab-label mode; the tab's
    // own tabData (set once in addTab(), travels with the tab through
    // detach/reattach) keeps the real label regardless of display mode or
    // how many tabs before it have been detached.
    const int tabIdx = m_tab != NULL ? m_tab->currentIndex() : -1;
    if (tabIdx >= 0)
        caption += QString(" - ") + tabDisplayLabel(m_tab->tabBar()->tabData(tabIdx).toString());

    setWindowTitle(caption);
}

void App::slotTabChanged(int index)
{
    Q_UNUSED(index)
    // Also how a tab moved to its own window (or put away) gets its tools
    // back: it stops being current first.
    if (m_mainBar != NULL)
        m_mainBar->hostTabTools(m_tab->currentWidget());
    updateWindowTitle();
}

void App::slotDocAutosave()
{
    saveXML(autoSaveFileName(), true);
}

void App::slotUniverseWritten(quint32 idx, const QByteArray &ua)
{
    foreach (Fixture *fixture, m_doc->fixtures())
    {
        if (!fixture->isPatched() || fixture->universe() != idx)
            continue;

        fixture->setChannelValues(ua);
    }
}

/*****************************************************************************
 * Main application Mode
 *****************************************************************************/

void App::enableKioskMode()
{
    // Turn on operate mode
    m_doc->setKiosk(true);
    m_doc->setMode(Doc::Operate);

    // No need for these
    m_tab->removeTab(m_tab->indexOf(FixtureManager::instance()));
    m_tab->removeTab(m_tab->indexOf(FunctionManager::instance()));
    m_tab->removeTab(m_tab->indexOf(ShowManager::instance()));
    m_tab->removeTab(m_tab->indexOf(SimpleDesk::instance()));
    m_tab->removeTab(m_tab->indexOf(InputOutputManager::instance()));

    // Hide the tab bar to save some pixels
    m_tab->tabBar()->hide();

    // No need for the toolbar, nor the show controls (in it, or in the
    // status bar). The tab's tools go home first.
    if (m_mainBar != NULL)
        m_mainBar->setHosting(false);
    delete m_showControls;
    m_showControls = NULL;
    delete m_toolbar;
    m_toolbar = NULL;
    m_mainBar = NULL;   // was m_toolbar's
}

void App::createKioskCloseButton(const QRect& rect)
{
    QPushButton* btn = new QPushButton(VirtualConsole::instance()->contents());
    btn->setIcon(QIcon(":/exit.png"));
    btn->setToolTip(tr("Exit"));
    btn->setGeometry(rect);
    connect(btn, SIGNAL(clicked()), this, SLOT(close()));
    btn->show();
}

void App::slotModeOperate()
{
    // Mirror slotModeDesign()'s "there's something you might lose" checkpoint
    // in the other direction — but only when there's something genuinely
    // worth flagging: uncommitted programmer values (colors/positions/etc.
    // dragged in the pad/Programming tab but never saved into a scene) that
    // are about to start affecting live output the instant Operate engages.
    // Deliberately NOT gated on Doc::isModified() — that's true almost
    // continuously while actively building a show, so warning on it would
    // be exactly the "friction on every single toggle" this was meant to
    // avoid, not a real heads-up.
    if (m_doc->isProgrammerDirty())
    {
        int result = QMessageBox::warning(
                         this,
                         tr("Switch to Operate Mode"),
                         tr("There are unsaved programmer edits (colors, "
                            "positions, etc. not yet saved into a scene).\n"
                            "Go live anyway? They'll affect output immediately."),
                         QMessageBox::Yes,
                         QMessageBox::No);
        if (result == QMessageBox::No)
            return;
    }
    m_doc->setMode(Doc::Operate);
}

void App::slotModeDesign()
{
    if (m_doc->masterTimer()->runningFunctions() > 0)
    {
        int result = QMessageBox::warning(
                         this,
                         tr("Switch to Design Mode"),
                         tr("There are still running functions.\n"
                            "Really stop them and switch back to "
                            "Design mode?"),
                         QMessageBox::Yes,
                         QMessageBox::No);

        if (result == QMessageBox::No)
            return;
        else
            m_doc->masterTimer()->stopAllFunctions();
    }

    m_liveEditVirtualConsoleAction->setChecked(false);
    m_doc->setMode(Doc::Design);
}

void App::slotModeToggle()
{
    if (m_doc->mode() == Doc::Design)
        slotModeOperate();
    else
        slotModeDesign();
}

void App::slotModeChanged(Doc::Mode mode)
{
    // Re-check on every mode change: going to Operate is exactly when "can this
    // rig actually output?" matters, and it's the last moment before someone
    // starts running cues.
    updateOutputReadiness();

    if (mode == Doc::Operate)
    {
        /* Disable editing features */
        m_fileNewAction->setEnabled(false);
        m_fileOpenAction->setEnabled(false);
        m_liveEditAction->setEnabled(true);
        m_liveEditVirtualConsoleAction->setEnabled(true);
        m_captureLiveEditsAction->setEnabled(true);

        m_modeToggleAction->setIcon(QIcon(":/design.png"));
        m_modeToggleAction->setText(tr("Design"));
        m_modeToggleAction->setToolTip(tr("Switch to design mode"));

        if (m_statusModeChipLabel != NULL)
        {
            m_statusModeChipLabel->setText(tr("OPERATE"));
            // #2e7d32 (Material green 800) is tuned for a light background —
            // computed ~3.25:1 against the dark themes' window color, marginal
            // even for bold text. #43a047 (green 600) keeps the same "live"
            // green identity with real contrast margin on both light and dark.
            m_statusModeChipLabel->setStyleSheet("QLabel { font-weight: bold; color: #43a047; }");
        }

        // Blind is a Design-only build aid — never let a muted rig survive into
        // a live show. Force it off and disable the toggle in Operate.
        if (m_doc != NULL)
            m_doc->inputOutputMap()->setOutputInhibited(false);
        if (m_controlBlindAction != NULL)
            m_controlBlindAction->setEnabled(false);
    }
    else if (mode == Doc::Design)
    {
        /* Enable editing features */
        m_fileNewAction->setEnabled(true);
        m_fileOpenAction->setEnabled(true);
        if (m_controlBlindAction != NULL)
            m_controlBlindAction->setEnabled(true);
        m_liveEditAction->setEnabled(false);
        m_liveEditVirtualConsoleAction->setEnabled(false);
        m_captureLiveEditsAction->setEnabled(false);
        if (m_captureLiveEditsAction->isChecked())
            m_captureLiveEditsAction->setChecked(false);

        m_modeToggleAction->setIcon(QIcon(":/operate.png"));
        m_modeToggleAction->setText(tr("Operate"));
        m_modeToggleAction->setToolTip(tr("Switch to operate mode"));

        if (m_statusModeChipLabel != NULL)
        {
            m_statusModeChipLabel->setText(tr("DESIGN"));
            // DESIGN is the neutral state (vs. OPERATE's deliberate green) —
            // palette(text) instead of a hardcoded hex so it actually follows
            // App::applyTheme()'s app-wide QPalette swap (confirmed real via
            // qApp->setPalette()) rather than staying tuned to the light
            // default and going low-contrast on the dark themes, same
            // problem the hardcoded #555 here had.
            m_statusModeChipLabel->setStyleSheet(
                "QLabel { font-weight: bold; color: palette(text); }");
        }
    }

    // The "under timeline control" chip + exit button are Operate-only.
    slotTimelineControlChanged();
}

/*****************************************************************************
 * Actions and toolbar
 *****************************************************************************/

void App::initActions()
{
    /* File actions */
    m_fileNewAction = new QAction(QIcon(":/filenew.png"), tr("&New"), this);
    m_fileNewAction->setShortcut(QKeySequence("CTRL+N"));
    connect(m_fileNewAction, SIGNAL(triggered(bool)), this, SLOT(slotFileNew()));

    m_fileOpenAction = new QAction(QIcon(":/fileopen.png"), tr("&Open"), this);
    m_fileOpenAction->setShortcut(QKeySequence("CTRL+O"));
    connect(m_fileOpenAction, SIGNAL(triggered(bool)), this, SLOT(slotFileOpen()));

    // File ▸ Import ▸ Any Supported File…: the submenu's last entry, the
    // one Import there used to be (it picks the importer by extension). The
    // typed entries above it come from importSources().
    m_fileImportAction = new QAction(QIcon(":/fileimport.png"), tr("&Any Supported File…"), this);
    m_fileImportAction->setObjectName(QStringLiteral("fileImport_any"));
    m_fileImportAction->setToolTip(tr("Import any file the app can read, picking the importer "
                                       "by its extension, without replacing what's already open"));
    connect(m_fileImportAction, SIGNAL(triggered(bool)), this, SLOT(slotFileImport()));

    m_fileSaveAction = new QAction(QIcon(":/filesave.png"), tr("&Save"), this);
    m_fileSaveAction->setShortcut(QKeySequence("CTRL+S"));
    /* Application-wide, not window-wide. Saving the workspace is an operation
       on the DOCUMENT, and the document is the same one whichever window has
       focus -- so Cmd+S has to work from the Lighting Studio pulled off into
       its own window, which is exactly where somebody has been arranging a rig
       for an hour and most wants to save. */
    m_fileSaveAction->setShortcutContext(Qt::ApplicationShortcut);
    m_fileSaveAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_fileSaveAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSave()));

    m_fileSaveAsAction = new QAction(QIcon(":/filesaveas.png"), tr("Save &As..."), this);
    connect(m_fileSaveAsAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSaveAs()));

    /* Control actions */
    m_modeToggleAction = new QAction(QIcon(":/operate.png"), tr("&Operate"), this);
    m_modeToggleAction->setToolTip(tr("Switch to operate mode"));
    m_modeToggleAction->setShortcut(QKeySequence("CTRL+F12"));
    connect(m_modeToggleAction, SIGNAL(triggered(bool)), this, SLOT(slotModeToggle()));

    m_controlMonitorAction = new QAction(QIcon(":/grid.png"), tr("&Lighting Studio"), this);
    // NOT Cmd+M — macOS reserves that for Minimize, which swallowed it.
    m_controlMonitorAction->setShortcut(QKeySequence("CTRL+SHIFT+M"));
    m_controlMonitorAction->setToolTip(tr("Lighting Studio — 2D plot, rigging & fixture layout"));
    connect(m_controlMonitorAction, SIGNAL(triggered(bool)), this, SLOT(slotControlMonitor()));

    m_addressToolAction = new QAction(QIcon(":/diptool.png"), tr("Address Tool"), this);
    connect(m_addressToolAction, SIGNAL(triggered()), this, SLOT(slotAddressTool()));

    m_controlBlackoutAction = new QAction(QIcon(":/blackout.png"), tr("Toggle &Blackout"), this);
    m_controlBlackoutAction->setCheckable(true);
    m_controlBlackoutAction->setIconText(tr("Blackout"));
    m_controlBlackoutAction->setToolTip(tr("Blackout: every light to zero, whatever is running. "
                                           "Fixtures set to ignore Blackout (house and work lights, "
                                           "power relays -- Output Rules…) stay as they are. "
                                           "Lit red while on."));
    connect(m_controlBlackoutAction, SIGNAL(triggered(bool)), this, SLOT(slotControlBlackout()));
    m_controlBlackoutAction->setChecked(m_doc->inputOutputMap()->blackout());

    // Blind — global output inhibit: mute the physical rig while the 2D preview
    // keeps updating, so looks can be built without hitting the stage. Design-mode
    // only (disabled in Operate; forced off on →Operate). Sibling of Blackout.
    m_controlBlindAction = new QAction(QIcon(":/blind.png"), tr("Toggle Bl&ind"), this);
    m_controlBlindAction->setCheckable(true);
    m_controlBlindAction->setIconText(tr("Blind"));
    m_controlBlindAction->setToolTip(tr(
        "Blind: mute the physical rig but keep the 2D preview live, so you can "
        "build a look without hitting the stage. Turn off to take it live. "
        "Fixtures set to ignore Blackout stay as they are. "
        "Design mode only. Lit blue while on."));
    connect(m_controlBlindAction, SIGNAL(triggered(bool)), this, SLOT(slotControlBlind(bool)));
    m_controlBlindAction->setChecked(m_doc->inputOutputMap()->outputInhibited());

    // Show-mode lock — when on, programmer parameter writes drop
    // silently. Selection nav, save, revert, pad-mode all still work.
    // Designed to prevent accidental edits during a live show.
    // (Uses a padlock icon, NOT the blackout icon, so it isn't mistaken for a
    // second Blackout button.)
    m_showLockAction = new QAction(QIcon(":/unlock.png"),
                                   tr("Show-mode &Lock"), this);
    m_showLockAction->setShortcut(QKeySequence("CTRL+L"));
    m_showLockAction->setCheckable(true);
    m_showLockAction->setToolTip(tr(
        "When locked, slider/knob writes are dropped — protects "
        "against accidental edits during a live show. Selection, "
        "Save, Revert, and pad-mode toggles still work."));
    connect(m_showLockAction, SIGNAL(triggered(bool)),
            this, SLOT(slotShowModeLock(bool)));

    // Follow MIDI Time Code — global toggle (moved out of the Show Manager
    // toolbar so it is reachable from anywhere and MIDI-mappable via a VC
    // FollowTimecode button). Arms/disarms MTC-follow on the current show.
    m_followMtcAction = new QAction(QIcon(":/clock.png"),
                                    tr("Follow MIDI Time &Code"), this);
    m_followMtcAction->setCheckable(true);
    m_followMtcAction->setToolTip(tr(
        "Follow incoming MIDI Time Code (e.g. from Logic) on the current show. "
        "The timeline chases the timecode; when it stops, the show holds for "
        "manual GO. Set the source/offset in the Show Manager toolbar."));
    connect(m_followMtcAction, SIGNAL(toggled(bool)),
            this, SLOT(slotFollowTimecodeToggled(bool)));

    // Last-look persistence: a stopped/finished show holds its final look instead
    // of blacking out (Operate). The toggle enables/disables the behaviour; the
    // clear action drops a currently-held look (also a MIDI-mappable VCButton).
    m_lastLookAction = new QAction(QIcon(":/star.png"), tr("Hold &Last Look"), this);
    m_lastLookAction->setCheckable(true);
    m_lastLookAction->setChecked(m_doc->lastLookEnabled());
    m_lastLookAction->setToolTip(tr(
        "When a show stops or ends (Operate), hold its final look on the rig "
        "instead of blacking out, until the next cue takes over."));
    connect(m_lastLookAction, SIGNAL(toggled(bool)),
            this, SLOT(slotLastLookToggled(bool)));

    /* Auto move-in-black in the menu bar too, ticked while it is on: the
       show's one setting, the same switch as Build's "Move in black" and the
       Show Tree's MIB button (they all follow the controller). */
    m_autoMibAction = new QAction(tr("Auto &Move in Black"), this);
    m_autoMibAction->setObjectName(QStringLiteral("autoMoveInBlackAction"));
    m_autoMibAction->setCheckable(true);
    m_autoMibAction->setToolTip(tr(
        "While the show runs, pre-position any dark mover the next cue will "
        "reveal. Saved with the show; the same switch as Build → Move in black "
        "and the Show Tree's MIB button."));
    if (ProgrammerController *pc = m_doc->programmer())
    {
        m_autoMibAction->setChecked(pc->isAutoMoveInBlack());
        connect(m_autoMibAction, &QAction::toggled, pc, [this, pc](bool on) {
            pc->setAutoMoveInBlack(on);
            const QSignalBlocker b(m_autoMibAction);
            m_autoMibAction->setChecked(pc->isAutoMoveInBlack());   // if it said no
        });
        connect(pc, &ProgrammerController::autoMoveInBlackChanged, m_autoMibAction, [this](bool on) {
            const QSignalBlocker b(m_autoMibAction);
            m_autoMibAction->setChecked(on);
        });
    }

    m_clearLastLookAction = new QAction(QIcon(":/fileclose.png"),
                                        tr("&Clear Held Last Look"), this);
    m_clearLastLookAction->setToolTip(tr(
        "Drop a stopped show's held last look now (the held channels go dark)."));
    connect(m_clearLastLookAction, SIGNAL(triggered()),
            this, SLOT(slotClearLastLook()));

    // Exit / resume timeline control — an Operate-mode VC takeover. While a show
    // is driving the rig, checking this suspends the timeline's output (the
    // Virtual Console owns the rig) while the playhead keeps tracking timecode,
    // so unchecking resumes exactly where the timeline now is. Enabled only when
    // a show is actually running in Operate. Sibling of the transport controls.
    m_timelineSuspendAction = new QAction(QIcon(":/player_pause.png"),
                                          tr("Exit &Timeline Control"), this);
    m_timelineSuspendAction->setCheckable(true);
    m_timelineSuspendAction->setEnabled(false);
    m_timelineSuspendAction->setToolTip(tr(
        "Suspend the running timeline so the Virtual Console owns the rig. The "
        "playhead keeps tracking timecode; turn off to resume timeline control. "
        "Available while a show is running in Operate mode."));
    connect(m_timelineSuspendAction, SIGNAL(triggered(bool)),
            this, SLOT(slotControlTimelineSuspend(bool)));

    m_liveEditAction = new QAction(QIcon(":/liveedit.png"), tr("Live edit a function"), this);
    connect(m_liveEditAction, SIGNAL(triggered()), this, SLOT(slotFunctionLiveEdit()));
    m_liveEditAction->setEnabled(false);

    m_liveEditVirtualConsoleAction = new QAction(QIcon(":/liveedit_vc.png"), tr("Toggle Virtual Console Live edit"), this);
    connect(m_liveEditVirtualConsoleAction, SIGNAL(triggered()), this, SLOT(slotLiveEditVirtualConsole()));
    m_liveEditVirtualConsoleAction->setCheckable(true);
    m_liveEditVirtualConsoleAction->setEnabled(false);

    m_captureLiveEditsAction = new QAction(QIcon(":/liveedit.png"), tr("Capture Live Edits"), this);
    m_captureLiveEditsAction->setToolTip(tr("Record VC fader/XYPad changes and fold them back into running scenes"));
    m_captureLiveEditsAction->setCheckable(true);
    m_captureLiveEditsAction->setEnabled(false);
    connect(m_captureLiveEditsAction, SIGNAL(toggled(bool)), this, SLOT(slotCaptureLiveEdits(bool)));

    m_captureStoreAction = new QAction(QIcon(":/filesave.png"), tr("Store"), this);
    m_captureStoreAction->setToolTip(tr("Open the diff dialog to commit captured edits to scenes"));
    m_captureStoreAction->setVisible(false);
    m_captureStoreAction->setEnabled(false);
    connect(m_captureStoreAction, SIGNAL(triggered()), this, SLOT(slotCaptureStore()));

    m_captureUndoAction = new QAction(QIcon(":/back.png"), tr("Undo Last Store"), this);
    m_captureUndoAction->setToolTip(tr("Revert the most recent capture commit"));
    m_captureUndoAction->setVisible(false);
    m_captureUndoAction->setEnabled(false);
    connect(m_captureUndoAction, SIGNAL(triggered()), this, SLOT(slotCaptureUndo()));

    m_dumpDmxAction = new QAction(QIcon(":/add_dump.png"), tr("Dump DMX values to a function"), this);
    m_dumpDmxAction->setShortcut(QKeySequence("CTRL+D"));
    connect(m_dumpDmxAction, SIGNAL(triggered()), this, SLOT(slotDumpDmxIntoFunction()));

    m_controlPanicAction = new QAction(QIcon(":/panic.png"), tr("Stop ALL functions!"), this);
    m_controlPanicAction->setShortcut(QKeySequence("CTRL+SHIFT+ESC"));
    // The show controls' button says the short form (iconText); the Control
    // menu keeps the full name.
    m_controlPanicAction->setIconText(tr("Stop all"));
    m_controlPanicAction->setToolTip(tr("Stop ALL functions (%1). Hold for Fade and stop.")
            .arg(m_controlPanicAction->shortcut().toString(QKeySequence::NativeText)));
    connect(m_controlPanicAction, SIGNAL(triggered(bool)), this, SLOT(slotControlPanic()));

    m_fadeAndStopMenu = new QMenu();
    QAction *fade1 = new QAction(tr("Fade 1 second and stop"), this);
    fade1->setData(QVariant(1000));
    connect(fade1, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade1);

    QAction *fade5 = new QAction(tr("Fade 5 seconds and stop"), this);
    fade5->setData(QVariant(5000));
    connect(fade5, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade5);

    QAction *fade10 = new QAction(tr("Fade 10 seconds and stop"), this);
    fade10->setData(QVariant(10000));
    connect(fade10, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade10);

    QAction *fade30 = new QAction(tr("Fade 30 seconds and stop"), this);
    fade30->setData(QVariant(30000));
    connect(fade30, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade30);

    m_controlPanicAction->setMenu(m_fadeAndStopMenu);

    /* Help actions. The app's own guides (HelpBrowser), opened on what is
       in front; the stock QLC+ reference, online, kept for what is unchanged
       from it. */
    m_helpContextAction = new QAction(QIcon(":/help.png"), tr("qlcconsole &Help"), this);
    m_helpContextAction->setShortcuts({ QKeySequence(Qt::Key_F1), QKeySequence(QKeySequence::HelpContents) });
    m_helpContextAction->setShortcutContext(Qt::ApplicationShortcut);
    m_helpContextAction->setToolTip(tr("Help on what you're looking at"));
    connect(m_helpContextAction, &QAction::triggered, this, &App::slotHelpContext);

    /* The main cue list's GO and Back (SHOW_CONTROL_DESIGN.md), from any
       window: what the footer's "Next:" says GO plays. */
    m_cueGoAction = new QAction(tr("GO (Main Cue List)"), this);
    m_cueGoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    m_cueGoAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_cueGoAction, &QAction::triggered, this, [this]() {
        m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() });
    });
    m_cueBackAction = new QAction(tr("Back (Main Cue List)"), this);
    m_cueBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return));
    m_cueBackAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_cueBackAction, &QAction::triggered, this, [this]() {
        m_doc->showControl()->execute({ ShowControl::Back, QString(), QString() });
    });
    /* ...and its Sub GO / Sub Back (SUBCUES_DESIGN.md): through the sub-cues
       of the cue playing, 4.1, 4.2..., while GO goes on to 5. ⌥⌘↩ / ⌥⌘⇧↩
       on macOS (Qt's CTRL is ⌘ there). */
    m_cueSubGoAction = new QAction(tr("Sub GO (Main Cue List)"), this);
    m_cueSubGoAction->setShortcut(cueSubGoShortcut());
    m_cueSubGoAction->setShortcutContext(Qt::ApplicationShortcut);
    // Through show control, as GO is: what came of it is reported the
    // same way (Connections → Show control & sync, last command).
    connect(m_cueSubGoAction, &QAction::triggered, this, [this]() {
        m_doc->showControl()->execute({ ShowControl::SubGo, QString(), QString() });
    });
    m_cueSubBackAction = new QAction(tr("Sub Back (Main Cue List)"), this);
    m_cueSubBackAction->setShortcut(cueSubBackShortcut());
    m_cueSubBackAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_cueSubBackAction, &QAction::triggered, this, [this]() {
        m_doc->showControl()->execute({ ShowControl::SubBack, QString(), QString() });
    });

    m_helpIndexAction = new QAction(tr("QLC+ Reference (online)"), this);
    m_helpIndexAction->setShortcut(QKeySequence("SHIFT+F1"));
    connect(m_helpIndexAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpIndex()));

    m_helpAboutAction = new QAction(QIcon(":/qlcconsole.png"), tr("&About qlcconsole"), this);
    // macOS relocates this to the application menu ("About QLC+").
    m_helpAboutAction->setMenuRole(QAction::AboutRole);
    connect(m_helpAboutAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpAbout()));

    /* Software Update (UpdateManager): GitHub Releases, never during a show.
       Gone entirely when IT has turned update checks off. */
    m_checkUpdatesAction = new QAction(tr("Check for Updates…"), this);
    m_checkUpdatesAction->setObjectName(QStringLiteral("checkForUpdates"));
    m_checkUpdatesAction->setMenuRole(QAction::NoRole);   // stays in Help on macOS
    m_checkUpdatesAction->setVisible(UpdateChecker::isDisabledByPolicy() == false);
    connect(m_checkUpdatesAction, &QAction::triggered, this, [this]() {
        if (m_updateManager != nullptr)
            m_updateManager->checkNow();
    });

    /* Settings action */
    m_appSettingsAction = new QAction(QIcon(":/configure.png"), tr("&Settings"), this);
    m_appSettingsAction->setShortcut(QKeySequence(tr("CTRL+,", "Settings")));
    // macOS relocates this to the application menu ("Preferences…").
    m_appSettingsAction->setMenuRole(QAction::PreferencesRole);
    connect(m_appSettingsAction, SIGNAL(triggered(bool)), this, SLOT(slotAppSettings()));

    if (QLCFile::hasWindowManager() == false)
    {
        m_quitAction = new QAction(QIcon(":/exit.png"), tr("Quit qlcconsole"), this);
        m_quitAction->setShortcut(QKeySequence("CTRL+ALT+Backspace"));
        connect(m_quitAction, SIGNAL(triggered(bool)), this, SLOT(close()));
    }
}

void App::initToolBar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("MainToolBar");
    // Its own height, never a share of the window's: the tabs take the rest.
    m_toolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *column = new QVBoxLayout(m_toolbar);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    QWidget *row = new QWidget(m_toolbar);
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4, 0, 4, 0);
    column->addWidget(row);
    // A hairline under it, as under a QMainWindow toolbar area.
    QFrame *line = new QFrame(m_toolbar);
    line->setObjectName("MainToolBarLine");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(1);
    line->setForegroundRole(QPalette::Mid);
    column->addWidget(line);
    if (QVBoxLayout *central = qobject_cast<QVBoxLayout *>(centralWidget()->layout()))
        central->insertWidget(0, m_toolbar);

    // A macOS-native "unified title bar" merge (icons sitting level with the
    // window title, like Safari/Mail) was attempted here and repeatedly
    // failed for months of iteration -- root-caused 2026-09-03: Qt's
    // setUnifiedTitleAndToolBarOnMac() on the Cocoa platform plugin never
    // actually creates a native NSToolbar object (confirmed both by a
    // documented Qt/Cocoa limitation and, directly, by instrumenting
    // AppKit's own NSWindow.toolbar and finding it nil at every check, on
    // two separate machines). Every native call this toolbar tried
    // (NSWindowToolbarStyleUnifiedCompact included) was therefore setting a
    // property on a toolbar that never existed -- structurally incapable of
    // doing anything, not a timing bug. A real fix needs an actual
    // NSToolbar built in Objective-C++; deliberately not pursued (Branson's
    // call) since it's macOS-only new surface area and Windows/Linux have no
    // equivalent "unified title bar" concept to begin with -- a toolbar as
    // its own row is already the normal, correct look there. This bar is
    // an ordinary row on every platform (since 2026-10 not even a
    // QMainWindow toolbar, see above), following the same "Toolbar Style"
    // preference as everything else (applyTabLabelMode()).

    // Native-first layout: the full command set lives in the menu bar (see
    // initMenuBar). This bar is the ONE row above the tabs (MainBar): the
    // current tab's own tools on the left, borrowed from the tab while it
    // is on show, and the live show controls pinned on the right. It used
    // to be two rows -- this bar's big icon-over-word show buttons (~58 px)
    // and then each tab's own toolbar (~36 px) -- about 94 px before any
    // content. Settings can move the show controls to the status bar
    // instead (applyShowControlsPlacement()).
    m_mainBar = new MainBar(row);
    rowLayout->addWidget(m_mainBar);

    m_showControls = new ShowControls(m_doc, m_controlPanicAction, m_controlBlackoutAction,
                                      m_controlBlindAction, m_modeToggleAction, m_mainBar);
    m_mainBar->setShowControls(m_showControls, m_showControls->modeSwitch());
}

QString App::showControlsPlacementKey()
{
    return QStringLiteral("workspace/showControlsPlacement");
}

void App::setShowControlsPlacement(int placement)
{
    if (placement != ShowControlsInStatusBar)
        placement = ShowControlsInTopBar;
    QSettings().setValue(showControlsPlacementKey(), placement);
    if (placement == m_showControlsPlacement)
        return;
    m_showControlsPlacement = placement;
    applyShowControlsPlacement();
}

void App::applyShowControlsPlacement()
{
    if (m_toolbar == NULL || m_mainBar == NULL || m_showControls == NULL)
        return;     // kiosk: neither

    if (m_showControlsPlacement == ShowControlsInStatusBar)
    {
        // The top is the tab's alone again: its tools go home to its own
        // top edge, and this bar has nothing left to show.
        m_mainBar->setHosting(false);
        m_toolbar->hide();
        m_showControls->setInStatusBar(true);
        statusBar()->addPermanentWidget(m_showControls);     // last: rightmost
        statusBar()->addPermanentWidget(m_showControls->modeSwitch());
        m_showControls->show();
        m_showControls->modeSwitch()->show();
        // The switch says the mode right here, and Blackout lights up: the
        // DESIGN / BLACKOUT chips would say it all twice, in a footer short
        // of room.
        if (m_statusModeChipLabel != NULL)
            m_statusModeChipLabel->hide();
        if (m_statusBlackoutLabel != NULL)
            m_statusBlackoutLabel->hide();
    }
    else
    {
        statusBar()->removeWidget(m_showControls);
        statusBar()->removeWidget(m_showControls->modeSwitch());
        m_showControls->setInStatusBar(false);
        m_mainBar->setShowControls(m_showControls, m_showControls->modeSwitch());
        if (m_statusModeChipLabel != NULL)
            m_statusModeChipLabel->show();
        if (m_statusBlackoutLabel != NULL)
            m_statusBlackoutLabel->setVisible(m_doc->inputOutputMap()->blackout());
        m_mainBar->setHosting(true);
        m_mainBar->hostTabTools(m_tab != NULL ? m_tab->currentWidget() : NULL);
        m_toolbar->show();
    }
}

void App::initMenuBar()
{
    QMenuBar* mb = menuBar();

    /* ---- File ---- */
    QMenu* fileMenu = mb->addMenu(tr("&File"));
    fileMenu->addAction(m_fileNewAction);
    fileMenu->addAction(m_fileOpenAction);
    m_fileImportMenu = fileMenu->addMenu(QIcon(":/fileimport.png"), tr("&Import"));
    m_fileImportMenu->setObjectName(QStringLiteral("fileImportMenu"));
    populateImportMenu(m_fileImportMenu);
    // Recent-files list becomes an "Open Recent" submenu (kept up to date by
    // updateFileOpenMenu(); no longer attached to the removed toolbar button).
    updateFileOpenMenu("");
    if (m_fileOpenMenu != NULL)
    {
        m_fileOpenMenu->setTitle(tr("Open &Recent"));
        fileMenu->addMenu(m_fileOpenMenu);
    }
    fileMenu->addSeparator();
    fileMenu->addAction(m_fileSaveAction);
    fileMenu->addAction(m_fileSaveAsAction);
    {
        QAction *collect = new QAction(tr("Collect for &Transfer…"), this);
        collect->setObjectName(QStringLiteral("fileCollectForTransfer"));
        collect->setToolTip(tr("Write the show and every definition and script it uses "
                               "that is not part of the app into one folder, to take to "
                               "another computer"));
        connect(collect, &QAction::triggered, this, &App::slotFileCollectForTransfer);
        fileMenu->addAction(collect);

        QAction *showDefs = new QAction(tr("Show &Definitions…"), this);
        showDefs->setObjectName(QStringLiteral("fileShowDefinitions"));
        showDefs->setToolTip(tr("The definitions and scripts this show carries, and how each "
                                "compares with this computer's"));
        connect(showDefs, &QAction::triggered, this, [this]() {
            ShowDifferencesDialog dlg(m_doc, this, true);
            dlg.exec();
        });
        fileMenu->addAction(showDefs);

        // Upgrade this show (WORKFLOW_DESIGN.md §3.7): the W5 converter
        // over every chaser and collection, Capture into palettes, one undo
        // step. Only ever opened from here or the Show Tree's banner.
        QAction *upgrade = new QAction(tr("&Upgrade This Show…"), this);
        upgrade->setObjectName(QStringLiteral("fileUpgradeShow"));
        upgrade->setToolTip(tr("Make the show's cue-list chasers Show Tree cue lists, checked channel "
                               "for channel, and raw scenes palettes: every part optional, one undo step"));
        connect(upgrade, &QAction::triggered, this, [this]() {
            ShowTreeView *view = ShowTreeView::instance();
            if (view == nullptr)
                return;
            switchToTabContaining(view);
            view->openUpgrade();
            // A Classic QLC+ show now with a Show Tree: offer the view
            // that shows it (once).
            offerConsoleViewAfterUpgrade();
        });
        fileMenu->addAction(upgrade);
    }
    fileMenu->addSeparator();
    fileMenu->addAction(m_appSettingsAction);      // → app menu (Preferences) on macOS
    // Quit: m_quitAction only exists when there's no window manager (kiosk/RPi).
    // On macOS it's NULL and, now that we own the menu bar, Qt no longer supplies
    // a working Quit — so provide one with QuitRole (→ app menu) + the standard
    // shortcut (⌘Q on macOS, Ctrl+Q elsewhere).
    {
        QAction *quitAction = m_quitAction;
        if (quitAction == NULL)
        {
            quitAction = new QAction(tr("Quit qlcconsole"), this);
            connect(quitAction, SIGNAL(triggered(bool)), this, SLOT(close()));
        }
        quitAction->setMenuRole(QAction::QuitRole);
        quitAction->setShortcut(QKeySequence::Quit);
        // Same reasoning as Save: quitting is an application-level act.
        quitAction->setShortcutContext(Qt::ApplicationShortcut);
        fileMenu->addAction(quitAction);
    }

    /* ---- Edit: Undo / Redo, named (open 24) ---- */
    {
        // One history for the Show Tree's steps and the palette operations
        // (either tab), plus Programming's own canvas edits. The menu names
        // the step; ⌘Z / ⇧⌘Z stay each tab's own shortcuts (a second
        // binding here would make them ambiguous), shown beside the text.
        QMenu *editMenu = mb->addMenu(tr("&Edit"));
        QAction *undo = editMenu->addAction(tr("Undo"));
        undo->setObjectName(QStringLiteral("editUndo"));
        QAction *redo = editMenu->addAction(tr("Redo"));
        redo->setObjectName(QStringLiteral("editRedo"));
        auto current = [this]() -> QWidget * {
            return m_tab != nullptr ? m_tab->currentWidget() : nullptr;
        };
        auto programming = [current]() -> ProgrammingManager * {
            QWidget *w = current();
            ProgrammingManager *pm = qobject_cast<ProgrammingManager *>(w);
            return pm != nullptr ? pm : (w != nullptr ? w->findChild<ProgrammingManager *>() : nullptr);
        };
        auto treeView = [current]() -> ShowTreeView * {
            QWidget *w = current();
            ShowTreeView *v = qobject_cast<ShowTreeView *>(w);
            return v != nullptr ? v : (w != nullptr ? w->findChild<ShowTreeView *>() : nullptr);
        };
        connect(editMenu, &QMenu::aboutToShow, this, [this, undo, redo, programming]() {
            QString u, r;
            if (ProgrammingManager *pm = programming())
            {
                u = pm->undoText();
                r = pm->redoText();
            }
            else if (ShowTreeController *st = m_doc->showTree())
            {
                u = st->canUndoShared() ? st->sharedUndoText() : QString();
                r = st->canRedoShared() ? st->sharedRedoText() : QString();
            }
            undo->setText(u.isEmpty() ? tr("Undo") : tr("Undo %1").arg(u));
            undo->setEnabled(!u.isEmpty());
            redo->setText(r.isEmpty() ? tr("Redo") : tr("Redo %1").arg(r));
            redo->setEnabled(!r.isEmpty());
        });
        connect(undo, &QAction::triggered, this, [this, programming, treeView]() {
            if (ProgrammingManager *pm = programming())
                QMetaObject::invokeMethod(pm, "slotUndo");
            else if (ShowTreeView *v = treeView())
                QMetaObject::invokeMethod(v, "slotUndo");
            else if (m_doc->showTree() != nullptr)
                m_doc->showTree()->undoShared();
        });
        connect(redo, &QAction::triggered, this, [this, programming, treeView]() {
            if (ProgrammingManager *pm = programming())
                QMetaObject::invokeMethod(pm, "slotRedo");
            else if (ShowTreeView *v = treeView())
                QMetaObject::invokeMethod(v, "slotRedo");
            else if (m_doc->showTree() != nullptr)
                m_doc->showTree()->redoShared();
        });

        /* ⇧⌘N, New (here): named for what it would make where you are
           ("New Cue After 0.0.3"). The key is each tab's own (Show Tree,
           Build, Fixtures -- broken out into a window too), as ⌘Z is: a
           binding here would clash with the Virtual Console's ⇧⌘N. */
        editMenu->addSeparator();
        QAction *newHere = editMenu->addAction(tr("New"));
        newHere->setObjectName(QStringLiteral("editNewHere"));
        // Where it goes: the tab (or broken-out window) holding the focus,
        // else the tab in front.
        auto newHereHost = [current]() -> QWidget * {
            for (QWidget *w = QApplication::focusWidget(); w != nullptr; w = w->parentWidget())
                if (qobject_cast<ShowTreeView *>(w) != nullptr || qobject_cast<ProgrammingManager *>(w) != nullptr
                    || qobject_cast<FixtureManager *>(w) != nullptr)
                    return w;
            QWidget *w = current();
            if (w == nullptr)
                return nullptr;
            if (qobject_cast<ShowTreeView *>(w) != nullptr || qobject_cast<ProgrammingManager *>(w) != nullptr
                || qobject_cast<FixtureManager *>(w) != nullptr)
                return w;
            if (ShowTreeView *v = w->findChild<ShowTreeView *>())
                return v;
            if (ProgrammingManager *pm = w->findChild<ProgrammingManager *>())
                return pm;
            return w->findChild<FixtureManager *>();
        };
        connect(editMenu, &QMenu::aboutToShow, this, [newHere, newHereHost]() {
            QWidget *host = newHereHost();
            QString label;
            if (ShowTreeView *v = qobject_cast<ShowTreeView *>(host))
                label = v->newHereLabel();
            else if (ProgrammingManager *pm = qobject_cast<ProgrammingManager *>(host))
                label = pm->newHereLabel();
            else if (FixtureManager *fm = qobject_cast<FixtureManager *>(host))
                label = fm->newHereLabel();
            const QString key = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N).toString(QKeySequence::NativeText);
            newHere->setText(label.isEmpty() ? tr("New (%1)").arg(key) : tr("%1 (%2)").arg(label, key));
            newHere->setEnabled(!label.isEmpty());
        });
        connect(newHere, &QAction::triggered, this, [newHereHost]() {
            QWidget *host = newHereHost();
            if (ShowTreeView *v = qobject_cast<ShowTreeView *>(host))
                v->newHere();
            else if (ProgrammingManager *pm = qobject_cast<ProgrammingManager *>(host))
                pm->newHere();
            else if (FixtureManager *fm = qobject_cast<FixtureManager *>(host))
                fm->newHere();
        });
    }

    /* ---- View: tools + jump to a workspace tab ---- */
    QMenu* viewMenu = mb->addMenu(tr("&View"));
    /* View ▸ Workspace (board Q35/Q36): which tabs show -- Classic QLC+,
       qlcconsole or Everything. Saved with the show; only the tabs change. */
    {
        QMenu *workspace = viewMenu->addMenu(tr("Workspace"));
        workspace->setObjectName(QStringLiteral("viewWorkspaceMenu"));
        QActionGroup *group = new QActionGroup(workspace);
        group->setExclusive(true);
        const struct { WorkspaceView view; const char *name; const char *tip; } views[] = {
            { ClassicView, "viewWorkspaceClassic",
              QT_TR_NOOP("The QLC+ tabs, in QLC+'s order: Fixtures, Functions, Shows, "
                         "Virtual Console, Simple Desk, Inputs/Outputs") },
            { ConsoleView, "viewWorkspaceConsole",
              QT_TR_NOOP("Connections, Fixtures, Lighting Studio, Build, Show Tree, Shows, "
                         "Virtual Console, Simple Desk and Run; Functions lives in Build ▸ "
                         "Scenes ▸ Classic") },
            { EverythingView, "viewWorkspaceEverything",
              QT_TR_NOOP("Every tab") } };
        for (const auto &v : views)
        {
            QAction *a = workspace->addAction(workspaceViewName(v.view));
            a->setObjectName(QString::fromLatin1(v.name));
            a->setToolTip(tr(v.tip));
            a->setStatusTip(tr(v.tip));
            a->setCheckable(true);
            a->setData(int(v.view));
            group->addAction(a);
            const WorkspaceView view = v.view;
            connect(a, &QAction::triggered, this, [this, view]() { setWorkspaceView(view); });
            m_workspaceViewActions << a;
        }
        updateWorkspaceViewActions();
        viewMenu->addSeparator();
    }
    // m_controlMonitorAction (Ctrl+Shift+M) is deliberately NOT added to this
    // menu — Lighting Studio is a regular tab now, so it already gets a jump
    // entry from the loop below like every other tab; adding this too would
    // just duplicate it. The action itself (and its shortcut) still exists
    // and still works via slotControlMonitor().
    viewMenu->addAction(m_addressToolAction);

    /* The rig overview is a way of LOOKING at the show, so it belongs in the
       View menu next to the tab jumps rather than buried in the Lighting Studio
       footer -- it is not a studio tool, it is a window onto the whole rig. The
       footer button stays as the in-context way to reach it. */
    m_rigOverviewAction = viewMenu->addAction(tr("Rig Overview…"));
    m_rigOverviewAction->setShortcut(QKeySequence("CTRL+SHIFT+R"));
    m_rigOverviewAction->setToolTip(tr("An angled, read-only view of the whole rig"));
    connect(m_rigOverviewAction, &QAction::triggered, this, &App::slotRigOverview);

    /* Focus mode: aligning the rig before a show (FOCUS_MODE_DESIGN.md). The
       Lighting Studio and the Rig Overview have it on their bars; this is the
       same thing from anywhere, and View + Menu on the controller asks too. */
    QAction *focusAct = viewMenu->addAction(tr("Focus Mode…"));
    focusAct->setObjectName(QStringLiteral("focusModeMenuAction"));
    focusAct->setShortcut(QKeySequence("CTRL+ALT+F"));
    focusAct->setToolTip(tr("Aim each moving head at its reference and nudge its calibration offsets"));
    connect(focusAct, &QAction::triggered, this, &App::slotFocusMode);

    /* Appearance -- theme, toolbar style, which chips the footer shows and
       the speed masters' beat -- is in Settings ▸ Appearance (Q65); View is
       only what you look at. */

    /* ---- Fixtures: document-wide fixture actions ----
     * Import/Export/Remap and Channels Fade Configuration used to live on
     * Fixture Manager's own toolbar, but none of them act on a specific
     * tree row (they're document-wide), so a right-click context menu
     * doesn't fit them the way it fits Properties/Remove — this menu is
     * their home instead (2026-09-03 modernization pass). */
    if (FixtureManager::instance() != NULL)
    {
        QMenu* fixturesMenu = mb->addMenu(tr("F&ixtures"));

        // Launches the standalone Fixture Editor (qlcconsole-fixtureeditor)
        // — editing fixture DEFINITIONS (channels/modes/capabilities), not
        // Fixture Manager's own patch/properties. Branson specifically asked
        // for the editor here, not a jump to the Fixture Manager tab (which
        // is already one click away via its own tab, and via View's jump
        // list) — this menu's whole reason to exist is actions that don't
        // live anywhere else yet.
        QAction* editorAction = fixturesMenu->addAction(tr("Fixture Editor..."));
        connect(editorAction, &QAction::triggered, this, [this]() {
            AppUtil::launchFixtureEditor(QString(), this);
        });
        QAction* updatesAction = fixturesMenu->addAction(tr("Definition Updates…"));
        updatesAction->setObjectName(QStringLiteral("fixturesDefinitionUpdates"));
        updatesAction->setToolTip(tr("Your fixture definitions that are older than the current "
                                     "version, and updating them"));
        connect(updatesAction, &QAction::triggered, this, [this]() {
            DefinitionUpdatesDialog dlg(m_doc, this);
            dlg.exec();
            AppUtil::updateDefinitionUpdatesStatus(m_doc);
        });
        fixturesMenu->addSeparator();

        FixtureManager::instance()->populateFixturesMenu(fixturesMenu);
    }

    /* ---- Control: playback + live editing ---- */
    QMenu* ctrlMenu = mb->addMenu(tr("&Control"));
    ctrlMenu->addAction(m_modeToggleAction);
    ctrlMenu->addSeparator();
    ctrlMenu->addAction(m_controlBlackoutAction);
    ctrlMenu->addAction(m_controlBlindAction);
    ctrlMenu->addAction(m_showLockAction);
    ctrlMenu->addSeparator();
    ctrlMenu->addAction(m_controlPanicAction);      // fade-and-stop options as submenu
    ctrlMenu->addAction(m_dumpDmxAction);
    ctrlMenu->addSeparator();
    ctrlMenu->addAction(m_cueGoAction);
    ctrlMenu->addAction(m_cueBackAction);
    ctrlMenu->addAction(m_cueSubGoAction);
    ctrlMenu->addAction(m_cueSubBackAction);
    ctrlMenu->addSeparator();
    QMenu* liveMenu = ctrlMenu->addMenu(tr("&Live Edit"));
    liveMenu->addAction(m_liveEditAction);
    liveMenu->addAction(m_liveEditVirtualConsoleAction);
    liveMenu->addAction(m_captureLiveEditsAction);
    liveMenu->addAction(m_captureStoreAction);
    liveMenu->addAction(m_captureUndoAction);
    ctrlMenu->addSeparator();
    ctrlMenu->addAction(m_followMtcAction);
    ctrlMenu->addAction(m_timelineSuspendAction);
    ctrlMenu->addAction(m_lastLookAction);
    ctrlMenu->addAction(m_clearLastLookAction);
    ctrlMenu->addSeparator();
    ctrlMenu->addAction(m_autoMibAction);

    /* ---- Window: every view, wherever it is ----
       "the menubar needs a windows thing so I can find them .. if I break out
       a tab it's hard to find". Each view is listed with its shortcut and
       brings it forward -- the tab, or its own window when it has been broken
       out (marked so). A broken-out view can be put back from here, one or
       all of them. */
    QMenu* windowMenu = mb->addMenu(tr("&Window"));
    windowMenu->setObjectName("windowMenu");
    for (int i = 0; i < m_tabOriginals.size(); i++)
    {
        QAction* jump = windowMenu->addAction(m_tabOriginals.at(i).second,
                                              m_tabOriginals.at(i).first);
        // Ctrl+Shift+<n> (⇧⌘n on macOS): the plain Ctrl+<n> row is taken by
        // Build ▸ Scenes' "new function" shortcuts.
        // The tenth (Run) is ⇧⌘0.
        jump->setShortcut(QKeySequence(QString("CTRL+SHIFT+%1").arg((i + 1) % 10)));
        /* Resolve by LABEL, not by construction order. Detaching a view calls
           removeTab(), so every index after it shifts and a raw
           setCurrentIndex(i) lands on the wrong page -- or on nothing at all
           for the detached view itself. */
        const QString label = m_tabOriginals.at(i).first;
        jump->setData(label);
        if (label == tr("Functions"))
        {
            jump->setObjectName(QStringLiteral("windowFunctions"));
            jump->setToolTip(tr("Every function by type, as the Function Manager listed them: Build ▸ Scenes ▸ Classic"));
            connect(jump, &QAction::triggered, this, &App::showFunctions);
        }
        else
            connect(jump, &QAction::triggered, this, [this, label]() {
                showContext(label);
            });
        m_windowJumps << jump;
    }
    /* The Rig Overview is a window of its own, so it is offered here too, not
       only under View: the Window menu is where people go looking for one
       ("there's no option in Windows"). The same action, so one shortcut. */
    windowMenu->addAction(m_rigOverviewAction);
    /* Effects: Build ▸ Effects, their one home (Q68) -- every effect
       palette, EFX and RGB Matrix of the show, designed in the Workbench.
       There is no separate Effects window any more. */
    QAction *effectsAct = windowMenu->addAction(tr("Effects"));
    effectsAct->setObjectName(QStringLiteral("windowEffects"));
    effectsAct->setToolTip(tr("Every effect in the show, and the effect designer: Build ▸ Effects"));
    connect(effectsAct, &QAction::triggered, this, &App::showEffects);
    EffectDesigner::plugIntoWorkbench();
    windowMenu->addSeparator();
    m_windowDockMenu = windowMenu->addMenu(tr("Put Back in Main Window"));
    m_windowDockAll = windowMenu->addAction(tr("Put All Back in Main Window"));
    connect(m_windowDockAll, &QAction::triggered, this, &App::reattachAllContexts);
    m_windowBreakOut = windowMenu->addAction(tr("Move to Its Own Window"));
    m_windowBreakOut->setObjectName(QStringLiteral("windowBreakOut"));
    connect(m_windowBreakOut, &QAction::triggered, this, [this]() {
        if (m_tab != NULL && m_tab->currentIndex() >= 0)
            slotDetachContext(m_tab->currentIndex());
    });
    m_windowShiftClick = windowMenu->addAction(tr("Shift-Click a Tab to Open It in a Window"));
    m_windowShiftClick->setObjectName(QStringLiteral("windowShiftClick"));
    m_windowShiftClick->setCheckable(true);
    m_windowShiftClick->setChecked(shiftClickBreaksOut());
    connect(m_windowShiftClick, &QAction::toggled, this, &App::setShiftClickBreaksOut);
    windowMenu->addSeparator();
    QAction *minimize = windowMenu->addAction(tr("Minimize"));
    minimize->setShortcut(QKeySequence(QStringLiteral("CTRL+M")));
    connect(minimize, &QAction::triggered, this, []() {
        if (QWidget *w = QApplication::activeWindow())
            w->showMinimized();
    });
    connect(windowMenu, &QMenu::aboutToShow, this, &App::updateWindowMenu);
    updateWindowMenu();

    /* ---- Help ---- */
    QMenu* helpMenu = mb->addMenu(tr("&Help"));
    helpMenu->setObjectName(QStringLiteral("helpMenu"));
    helpMenu->addAction(m_helpContextAction);
    helpMenu->addSeparator();
    const QList<QPair<QString, QString> > guides = {
        { tr("Getting Started"),    QStringLiteral("getting-started.md") },
        { tr("What's New"),         QStringLiteral("whats-new.md") },
        { tr("Keyboard and Mouse"), QStringLiteral("shortcuts.md") },
        { tr("Effects"),            QStringLiteral("effects.md") },
        { tr("Troubleshooting"),    QStringLiteral("troubleshooting.md") },
        { tr("All Guides"),         QStringLiteral("README.md") } };
    for (const auto &g : guides)
    {
        const QString topic = g.second;
        helpMenu->addAction(g.first, this, [topic]() { HelpBrowser::open(topic); });
    }
    helpMenu->addSeparator();
    helpMenu->addAction(m_helpIndexAction);
    helpMenu->addAction(m_checkUpdatesAction);
    helpMenu->addAction(m_helpAboutAction);         // → app menu (About) on macOS
}

/*****************************************************************************
 * File action slots
 *****************************************************************************/

bool App::handleFileError(QFile::FileError error)
{
    QString msg;

    switch (error)
    {
        case QFile::NoError:
            return true;
        break;
        case QFile::ReadError:
            msg = tr("Unable to read from file");
        break;
        case QFile::WriteError:
            msg = tr("Unable to write to file");
        break;
        case QFile::FatalError:
            msg = tr("A fatal error occurred");
        break;
        case QFile::ResourceError:
            msg = tr("Unable to access resource");
        break;
        case QFile::OpenError:
            msg = tr("Unable to open file for reading or writing");
        break;
        case QFile::AbortError:
            msg = tr("Operation was aborted");
        break;
        case QFile::TimeOutError:
            msg = tr("Operation timed out");
        break;
        default:
        case QFile::UnspecifiedError:
            msg = tr("An unspecified error has occurred. Nice.");
        break;
    }

    QMessageBox::warning(this, tr("File error"), msg);

    return false;
}

bool App::saveModifiedDoc(const QString & title, const QString & message)
{
    // if it's not modified, there's nothing to save
    if (m_doc->isModified() == false)
        return true;

    int result = QMessageBox::warning(this, title,
                                          message,
                                          QMessageBox::Yes |
                                          QMessageBox::No |
                                          QMessageBox::Cancel);
    if (result == QMessageBox::Yes)
    {
        slotFileSave();
        // we check whether m_doc is not modified anymore, rather than
        // result of slotFileSave() since the latter returns NoError
        // in cases like when the user pressed cancel in the save dialog
        if (m_doc->isModified() == false)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (result == QMessageBox::No)
    {
        /* Discarded, on purpose: so is its recovery copy. Left behind (the
           document is still "modified", so closing doesn't remove it either),
           it was offered back at every start from then on. */
        removeAutosaveFile();
        return true;
    }
    else
    {
        return false;
    }
}

void App::updateFileOpenMenu(QString addRecent)
{
    QSettings settings;
    QStringList menuRecentList;

    if (m_fileOpenMenu == NULL)
    {
        m_fileOpenMenu = new QMenu(this);
        QPalette p = palette();
        QString style = QString("QMenu { background: %1;"
                        "border: 1px solid black; font:bold; }"
                        "QMenu::item { background-color: transparent; padding: 5px 10px 5px 10px; border: 1px solid black; }"
                        "QMenu::item:selected { background-color: #2D8CFF; }").arg(p.color(QPalette::Window).name());
        m_fileOpenMenu->setStyleSheet(style);
        connect(m_fileOpenMenu, SIGNAL(triggered(QAction*)),
                this, SLOT(slotRecentFileClicked(QAction*)));
    }

    foreach (QAction* a, m_fileOpenMenu->actions())
    {
        menuRecentList.append(a->text());
        m_fileOpenMenu->removeAction(a);
    }

    if (addRecent.isEmpty() == false)
    {
        menuRecentList.removeAll(addRecent); // in case the string is already present, remove it...
        menuRecentList.prepend(addRecent); // and add it to the top
        for (int i = 0; i < menuRecentList.count(); i++)
        {
            settings.setValue(QString("%1%2").arg(SETTINGS_RECENTFILE).arg(i), menuRecentList.at(i));
            m_fileOpenMenu->addAction(menuRecentList.at(i));
        }
    }
    else
    {
        for (int i = 0; i < MAX_RECENT_FILES; i++)
        {
            QVariant recent = settings.value(QString("%1%2").arg(SETTINGS_RECENTFILE).arg(i));
            if (recent.isValid() == true)
            {
                menuRecentList.append(recent.toString());
                m_fileOpenMenu->addAction(menuRecentList.at(i));
            }
        }
    }

    // The recent files live in the File ▸ Open Recent submenu (added in
    // initMenuBar). Disable that submenu when there are no recent files.
    m_fileOpenMenu->setEnabled(menuRecentList.isEmpty() == false);
}

bool App::slotFileNew()
{
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("New Workspace"), msg) == false)
    {
        return false;
    }

    clearDocument();
    return true;
}

QFile::FileError App::slotFileOpen()
{
    QString fn;

    /* Check that the user is aware of losing previous changes */
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("Open Workspace"), msg) == false)
    {
        /* Second thoughts... Cancel loading. */
        return QFile::NoError;
    }

    /* Create a file open dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Open Workspace"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.selectFile(fileName());
    if (m_workingDirectory.exists() == true)
        dialog.setDirectory(m_workingDirectory);

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("qlcconsole Workspaces (*%1)").arg(KExtWorkspaceConsole);
    filters << tr("QLC+ Workspaces (*%1)").arg(KExtWorkspace);
#if defined(WIN32) || defined(Q_OS_WIN)
    filters << tr("All Files (*.*)");
#else
    filters << tr("All Files (*)");
#endif
    dialog.setNameFilters(filters);

    /* Append useful URLs to the dialog */
    QList <QUrl> sidebar;
    sidebar.append(QUrl::fromLocalFile(QDir::homePath()));
    sidebar.append(QUrl::fromLocalFile(QDir::rootPath()));
    dialog.setSidebarUrls(sidebar);

    /* Get file name */
    if (dialog.exec() != QDialog::Accepted)
        return QFile::NoError;
    QSettings settings;
    m_workingDirectory = dialog.directory();
    settings.setValue(SETTINGS_WORKINGPATH, m_workingDirectory.absolutePath());

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return QFile::NoError;

    /* Clear existing document data */
    clearDocument();

#ifdef DEBUG_SPEED
    speedTime.restart();
#endif

    /* Load the file */
    QFile::FileError error = loadXML(fn);
    if (handleFileError(error) == true)
        m_doc->resetModified();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Project loaded in" << speedTime.elapsed() << "ms.";
#endif

    /* Update these in any case, since they are at least emptied now as
       a result of calling clearDocument() a few lines ago. */
    //if (FunctionManager::instance() != NULL)
    //    FunctionManager::instance()->updateTree();
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();

    updateFileOpenMenu(fn);

    return error;
}

Doc *App::loadScratchDoc(const QString &fileName, QString &error)
{
    /* Load a workspace file into a throwaway Doc -- NOT m_doc. Doc::loadXML()
       reads IDs verbatim from the XML and silently drops anything that
       collides with what's already present, which is fine for a Doc that
       started empty (this one) but would be exactly wrong for merging into
       the live one. QxwImporter does the actual collision-aware merge,
       working from this scratch copy. Caller owns the returned Doc. */
    QXmlStreamReader *reader = QLCFile::getXMLReader(fileName);
    if (reader == NULL || reader->device() == NULL || reader->hasError())
    {
        error = tr("Unable to read %1").arg(fileName);
        return NULL;
    }
    while (!reader->atEnd())
    {
        if (reader->readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (reader->hasError() || reader->dtdName() != KXMLQLCWorkspace)
    {
        QLCFile::releaseXMLReader(reader);
        error = tr("%1 is not a workspace file").arg(fileName);
        return NULL;
    }

    Doc *scratchDoc = new Doc(this);
    scratchDoc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    scratchDoc->fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());
    scratchDoc->modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
    scratchDoc->modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());

    bool loaded = false;
    if (reader->readNextStartElement() == true && reader->name() == KXMLQLCWorkspace)
    {
        while (reader->readNextStartElement())
        {
            if (reader->name() == KXMLQLCEngine)
            {
                loaded = scratchDoc->loadXML(*reader);
                break;
            }
            else
            {
                reader->skipCurrentElement();
            }
        }
    }
    QLCFile::releaseXMLReader(reader);

    if (loaded == false)
    {
        error = tr("%1 could not be loaded").arg(fileName);
        delete scratchDoc;
        return NULL;
    }

    return scratchDoc;
}

/* File ▸ Import's table. One row per kind of file that can be brought into
   the open show; the submenu, its sections, each entry's file dialog and
   "Any Supported File…" all read it. Next expected: QLab (a workspace's cue
   lists into the Show Tree), on the EosImportWizard pattern
   (EOS_IMPORT_DESIGN.md) -- one row here and its handler. Only importers
   that work go in: no disabled placeholders. */
QList<App::ImportSource> App::importSources() const
{
#if defined(WIN32) || defined(Q_OS_WIN)
    const QString all = tr("All Files (*.*)");
#else
    const QString all = tr("All Files (*)");
#endif
    const QString fromShow = tr("From a show file");
    const QString fixtures = tr("Fixtures && patch");

    QList<ImportSource> sources;
    sources << ImportSource{
        QStringLiteral("workspace"), fromShow,
        tr("QLC+ / qlcconsole &Workspace (.qxw, .qlcc)…"),
        tr("Import from a Workspace"),
        tr("Bring fixtures, groups, functions and palettes in from another workspace, "
           "without replacing what's already open"),
        QStringLiteral(":/qlcconsole.png"),
        QStringList{ tr("Workspaces (*%1 *%2)").arg(KExtWorkspaceConsole, KExtWorkspace),
                     tr("qlcconsole Workspaces (*%1)").arg(KExtWorkspaceConsole),
                     tr("QLC+ Workspaces (*%1)").arg(KExtWorkspace), all },
        QStringList{ KExtWorkspaceConsole, KExtWorkspace },
        &App::importWorkspaceFile };
    sources << ImportSource{
        QStringLiteral("eos"), fromShow,
        tr("ETC &Eos Show (ASCII .asc)…"),
        tr("Import an Eos show"),
        tr("An ETC Eos-family show exported as USITT ASCII: patch, groups, palettes, "
           "presets, subs, and its cue lists into the Show Tree"),
        QString(),
        QStringList{ tr("ETC Eos shows, USITT ASCII export (*.asc *.ASC)"), all },
        QStringList{ QStringLiteral(".asc") },
        &App::importEosShow };
    sources << ImportSource{
        QStringLiteral("fixturelist"), fixtures,
        tr("QLC+ &Fixtures List (.qxfl)…"),
        tr("Import Fixtures List"),
        tr("Add the fixtures and groups of a Fixtures List (Fixtures ▸ Export fixtures) "
           "to this show"),
        QStringLiteral(":/fixture.png"),
        QStringList{ tr("Fixtures List (*%1)").arg(KExtFixtureList), all },
        QStringList{ KExtFixtureList },
        &App::importFixtureListFile };
    return sources;
}

void App::populateImportMenu(QMenu *menu)
{
    QString section;
    foreach (const ImportSource &src, importSources())
    {
        if (src.section != section)
        {
            section = src.section;
            menu->addSection(section);
        }
        QAction *a = menu->addAction(src.label);
        a->setObjectName(QStringLiteral("fileImport_") + src.key);
        a->setToolTip(src.tip);
        a->setStatusTip(src.tip);
        if (src.icon.isEmpty() == false)
            a->setIcon(QIcon(src.icon));
        const QString key = src.key;
        connect(a, &QAction::triggered, this, [this, key]() {
            foreach (const ImportSource &s, importSources())
            {
                if (s.key != key)
                    continue;
                const QString fn = pickImportFile(s.title, s.filters);
                if (fn.isEmpty() == false)
                    runImport(s, fn);
                return;
            }
        });
    }
    // The one Import there used to be, by extension, for muscle memory.
    menu->addSeparator();
    menu->addAction(m_fileImportAction);
}

QString App::pickImportFile(const QString &title, const QStringList &filters)
{
    if (m_importFilePicker)
        return m_importFilePicker(title, filters);

    /* Read-only: the current document is untouched until the user confirms
       a selection in the importer that follows. */
    QFileDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    if (m_workingDirectory.exists() == true)
        dialog.setDirectory(m_workingDirectory);
    dialog.setNameFilters(filters);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return QString();
    return dialog.selectedFiles().first();
}

void App::runImport(const ImportSource &source, const QString &path)
{
    if (m_importRunHook)
    {
        m_importRunHook(source.key, path);
        return;
    }
    (this->*source.handler)(path);
}

void App::importFile(const QString &path)
{
    const QList<ImportSource> sources = importSources();
    Q_ASSERT(sources.isEmpty() == false);
    foreach (const ImportSource &src, sources)
    {
        foreach (const QString &ext, src.extensions)
        {
            if (path.endsWith(ext, Qt::CaseInsensitive))
            {
                runImport(src, path);
                return;
            }
        }
    }
    // Unknown: read as a workspace, which says so if it isn't one.
    runImport(sources.first(), path);
}

void App::slotFileImport()
{
    QStringList patterns;
    QStringList own;
    const QList<ImportSource> sources = importSources();
    foreach (const ImportSource &src, sources)
    {
        foreach (const QString &ext, src.extensions)
            patterns << QStringLiteral("*") + ext;
        // Each type's own filter, then All Files once at the end.
        for (int i = 0; i < src.filters.size() - 1; ++i)
            if (own.contains(src.filters.at(i)) == false)
                own << src.filters.at(i);
    }
    QStringList filters;
    filters << tr("All supported files (%1)").arg(patterns.join(QLatin1Char(' ')));
    filters << own;
    filters << sources.first().filters.last();

    const QString fn = pickImportFile(tr("Import"), filters);
    if (fn.isEmpty() == false)
        importFile(fn);
}

void App::importWorkspaceFile(const QString &fn)
{
    QString err;
    Doc *scratchDoc = loadScratchDoc(fn, err);
    if (scratchDoc == NULL)
    {
        QMessageBox::warning(this, tr("Import"), err);
        return;
    }

    ImportSelectionDialog picker(this, scratchDoc);
    if (picker.exec() != QDialog::Accepted)
    {
        delete scratchDoc;
        return;
    }

    QList<quint32> fixtures = picker.selectedFixtures();
    QList<quint32> groups = picker.selectedFixtureGroups();
    QList<quint32> functions = picker.selectedFunctions();
    QList<quint32> palettes = picker.selectedPalettes();
    if (fixtures.isEmpty() && groups.isEmpty() && functions.isEmpty() && palettes.isEmpty())
    {
        delete scratchDoc;
        return;
    }

    QxwImportResult result = QxwImporter::import(scratchDoc, m_doc, fixtures, groups, functions, palettes);
    delete scratchDoc;
    showImportSummary(result);
}

void App::importFixtureListFile(const QString &path)
{
    const int added = FixtureManager::importFixtureList(m_doc, path);
    if (added < 0)
    {
        QMessageBox::warning(this, tr("Import"),
                             tr("%1 is not a Fixtures List").arg(QDir::toNativeSeparators(path)));
        return;
    }
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
}

void App::showImportSummary(const QxwImportResult &result, const QStringList &before,
                            const QStringList &after)
{
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();

    QString summary = tr("Imported %1 fixture(s), %2 fixture group(s), %3 function(s).")
                        .arg(result.fixturesImported).arg(result.fixtureGroupsImported).arg(result.functionsImported);
    if (result.palettesImported > 0)
        summary += tr("\n%1 palette(s) came along with the imported scenes.").arg(result.palettesImported);
    if (result.fixturesPlaced > 0)
        summary += tr("\n%1 fixture(s) kept their position on the 2D map (layer/group reset).")
                     .arg(result.fixturesPlaced);
    if (result.layersCreated > 0 || result.mapGroupsCreated > 0)
        summary += tr("\n2D map: %1 layer(s) and %2 group(s) created (existing ones reused).")
                     .arg(result.layersCreated).arg(result.mapGroupsCreated);
    if (result.fixturesPowerPatched > 0)
        summary += tr("\n%1 fixture(s) were re-patched for power%2.")
                     .arg(result.fixturesPowerPatched)
                     .arg(result.powerSourcesCreated > 0
                            ? tr(" (%1 new source(s) created)").arg(result.powerSourcesCreated)
                            : QString());
    if (result.idsRemapped > 0)
        summary += tr("\n%1 ID(s) were remapped due to conflicts with the current workspace.").arg(result.idsRemapped);
    if (result.fixturesRelocated > 0)
        summary += tr("\n%1 fixture(s) were moved to a free DMX address.").arg(result.fixturesRelocated);
    if (result.warnings.isEmpty() == false)
        summary += tr("\n\n%1 item(s) skipped:\n").arg(result.warnings.size()) + result.warnings.join("\n");
    if (!after.isEmpty())
        summary += QStringLiteral("\n\n") + after.join(QStringLiteral("\n"));

    if (before.isEmpty())
    {
        QMessageBox::information(this, tr("Import complete"), summary);
        return;
    }
    // An Eos import's report runs long: what was read, then what landed.
    QMessageBox box(QMessageBox::Information, tr("Import complete"), summary, QMessageBox::Ok, this);
    box.setDetailedText(before.join(QStringLiteral("\n")));
    box.exec();
}

void App::importEosShow(const QString &asciiPath)
{
    EosImportWizard wizard(this, m_doc, asciiPath);
    if (wizard.exec() != QDialog::Accepted)
        return;

    EosImportResult eos;
    QApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    Doc *scratchDoc = EosImporter::load(wizard.bundle(), eos, wizard.mapper());
    QApplication::restoreOverrideCursor();
    if (scratchDoc == NULL)
    {
        QMessageBox::warning(this, tr("Import"), eos.warnings.join("\n"));
        return;
    }

    ImportSelectionDialog picker(this, scratchDoc);
    picker.setWindowTitle(tr("Import from the Eos show"));
    picker.selectAll();
    if (picker.exec() != QDialog::Accepted)
    {
        delete scratchDoc;
        return;
    }

    QApplication::setOverrideCursor(AppUtil::nativeCursorShape(Qt::WaitCursor));
    QxwImportResult result = QxwImporter::import(scratchDoc, m_doc, picker.selectedFixtures(),
                                                 picker.selectedFixtureGroups(), picker.selectedFunctions(),
                                                 picker.selectedPalettes());
    delete scratchDoc;

    QStringList treeLines;
    if (wizard.toShowTree())
    {
        QList<quint32> chasers;
        for (quint32 id : eos.chasers)
            if (result.functionMap.contains(id))
                chasers << result.functionMap.value(id);
        EosImporter::toShowTree(m_doc, chasers, &treeLines);
    }
    // Subs: a Playback fader each on the Virtual Console.
    QList<EosImportedSub> subs;
    for (EosImportedSub s : eos.subs)
        if (result.functionMap.contains(s.function))
        {
            s.function = result.functionMap.value(s.function);
            subs << s;
        }
    EosImportWizard::addSubFaders(m_doc, subs, &treeLines);
    QApplication::restoreOverrideCursor();

    QStringList read;
    read << tr("Read from %1: %2 channel(s) (%3 patched as a generic dimmer, %4 staged without an address), "
               "%5 group(s), %6 palette(s), %7 cue(s) in %8 list(s), %9 preset(s), %10 sub(s).")
            .arg(QFileInfo(wizard.bundle().asciiPath).fileName())
            .arg(eos.fixturesParsed).arg(eos.fixturesUnmapped).arg(eos.fixturesUnpatched)
            .arg(eos.fixtureGroupsParsed).arg(eos.palettesParsed).arg(eos.cuesParsed)
            .arg(eos.cueListsParsed).arg(eos.presetsParsed).arg(eos.submastersParsed);
    read << eos.warnings;
    if (!eos.warnings.isEmpty())
        treeLines.prepend(tr("%n note(s) on what the Eos file holds and what didn't come across: Show Details.",
                             "", int(eos.warnings.count())));
    showImportSummary(result, read, treeLines);
}

QFile::FileError App::slotFileSave()
{
    QFile::FileError error;
    QString asfName = autoSaveFileName();

    /* Attempt to save with the existing name. Fall back to Save As (which
       asks about missing definitions itself). */
    if (fileName().isEmpty() == true)
        error = slotFileSaveAs();
    else if (!confirmSaveWithoutDefinitions())
        return QFile::NoError;
    else
        error = saveXML(fileName());

    if (handleFileError(error))
    {
        // Remove autosave file on successful save
        removeAutosaveFile();
    }
    return error;
}

QFile::FileError App::slotFileSaveAs()
{
    QString fn;
    QString asfName = autoSaveFileName();

    if (!confirmSaveWithoutDefinitions())
        return QFile::NoError;

    /* Create a file save dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Save Workspace As"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile(fileName());

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("qlcconsole Workspaces (*%1)").arg(KExtWorkspaceConsole);
#if defined(WIN32) || defined(Q_OS_WIN)
    filters << tr("All Files (*.*)");
#else
    filters << tr("All Files (*)");
#endif
    dialog.setNameFilters(filters);

    /* Append useful URLs to the dialog */
    QList <QUrl> sidebar;
    sidebar.append(QUrl::fromLocalFile(QDir::homePath()));
    sidebar.append(QUrl::fromLocalFile(QDir::rootPath()));
    dialog.setSidebarUrls(sidebar);

    /* Get file name */
    if (dialog.exec() != QDialog::Accepted)
        return QFile::NoError;

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return QFile::NoError;

    /* Default to .qlcc; also accept .qxw (QLC+ import) */
    if (fn.right(5) != KExtWorkspaceConsole && fn.right(4) != KExtWorkspace)
        fn += KExtWorkspaceConsole;

    /* Set the workspace path before saving the new XML. In this way local files
       can be loaded even if the workspace file will be moved */
    m_doc->setWorkspacePath(QFileInfo(fn).absolutePath());

    /* Save the document and set workspace name */
    QFile::FileError error = saveXML(fn);

    if (handleFileError(error))
    {
        /* remove autosave file if present */
        QFile asFile(asfName);
        if (asFile.exists())
            asFile.remove();
    }

    updateFileOpenMenu(fn);
    return error;
}

void App::slotFileCollectForTransfer()
{
    if (!confirmSaveWithoutDefinitions())
        return;

    const QString base = m_fileName.isEmpty() ? tr("Show") : QFileInfo(m_fileName).completeBaseName();
    const QString start = m_fileName.isEmpty() ? QDir::homePath() : QFileInfo(m_fileName).absolutePath();
    const QString parent = QFileDialog::getExistingDirectory(this,
        tr("Collect for Transfer: where to make the “%1” folder").arg(base), start);
    if (parent.isEmpty())
        return;

    const QDir dir(QDir(parent).absoluteFilePath(base));
    if (dir.exists() && !dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()
        && QMessageBox::question(this, tr("Collect for Transfer"),
               tr("There is already a folder called “%1” there. Files in it with the same "
                  "names will be replaced.\n\nCollect into it anyway?").arg(base),
               QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    if (!QDir().mkpath(dir.absolutePath()))
    {
        QMessageBox::warning(this, tr("Collect for Transfer"),
                             tr("Could not make the folder %1.").arg(dir.absolutePath()));
        return;
    }

    /* The show as a .qlcc (it carries the same files), saved as a copy: this
       window goes on with the show it has open. */
    const QString show = dir.absoluteFilePath(base + KExtWorkspaceConsole);
    const QFile::FileError error = saveXML(show, true);
    if (error != QFile::NoError)
    {
        handleFileError(error);
        return;
    }
    QStringList written;
    const bool ok = m_doc->showEmbedding()->writeTransferFolder(dir.absolutePath(), &written);
    if (!ok)
        QMessageBox::warning(this, tr("Collect for Transfer"),
            tr("The show is in %1, but some of the files it uses could not be written there.")
                .arg(dir.absolutePath()));
    else
        QMessageBox::information(this, tr("Collect for Transfer"),
            tr("Collected in %1: the show, and %n file(s) it uses that are not part of the "
               "app (fixture definitions, scripts, LED maps).", "", written.size())
                .arg(QDir::toNativeSeparators(dir.absolutePath())));
}

/*****************************************************************************
 * Control action slots
 *****************************************************************************/

void App::slotRigOverview()
{
    /* Ask the Lighting Studio for it: the Monitor owns the window (and the
       raise-the-existing-one behaviour), so the menu entry and the footer
       button open the SAME window rather than two that drift apart. But do
       NOT bring the Studio tab forward to do it -- the overview is a window
       of its own, opened to look at the rig beside whatever you are working
       in ("every time I open the rig view .. the studio tab comes up"). */
    if (Monitor *mon = Monitor::instance())
        mon->toggleStageOverview();
}

void App::slotFocusMode()
{
    if (Monitor *mon = Monitor::instance())
        mon->openFocusMode();
}

void App::slotControlMonitor()
{
    Monitor::createAndShow(this, m_doc);
}

void App::slotAddressTool()
{
    AddressTool at(this);
    at.exec();
}

void App::slotControlBlackout()
{
    m_doc->inputOutputMap()->setBlackout(!m_doc->inputOutputMap()->blackout());
}

void App::slotBlackoutChanged(bool state)
{
    m_controlBlackoutAction->setChecked(state);

    if (m_statusBlackoutLabel != NULL)
    {
        m_statusBlackoutLabel->setText(state ? tr("● BLACKOUT ") : QString());
        // With the show controls in the status bar, their Blackout button
        // is lit red right beside it: the chip would say it twice, in a
        // footer short of room.
        m_statusBlackoutLabel->setVisible(state
                && m_showControlsPlacement != ShowControlsInStatusBar);
    }
}

void App::slotControlBlind(bool checked)
{
    // Blind is a Design-mode build aid; refuse to arm it during a live show.
    if (checked && m_doc != NULL && m_doc->mode() != Doc::Design)
    {
        if (m_controlBlindAction != NULL)
            m_controlBlindAction->setChecked(false);
        return;
    }
    if (m_doc != NULL)
        m_doc->inputOutputMap()->setOutputInhibited(checked);
}

void App::slotOutputInhibitedChanged(bool state)
{
    // Engine is the source of truth: keep the toolbar action and the status chip
    // in step (Blind may be cleared elsewhere, e.g. forced off on →Operate).
    if (m_controlBlindAction != NULL && m_controlBlindAction->isChecked() != state)
        m_controlBlindAction->setChecked(state);

    // Turn the WHOLE footer blue while Blind is armed so it's unmistakable at a
    // glance. Force white text on the plain labels for contrast; labels that set
    // their own colour (dirty/autosave) keep theirs (their inline stylesheet wins
    // over this ancestor rule), which still reads fine on blue.
    QStatusBar *sb = statusBar();
    if (sb != NULL)
        sb->setStyleSheet(state
            ? QStringLiteral("QStatusBar { background: #1565c0; } "
                             "QStatusBar QLabel { color: white; }")
            : QString());
}

void App::slotFooterGrandMasterMoved(int value)
{
    if (m_doc == NULL)
        return;
    // Avoid a redundant write/feedback-loop tick when this only just moved
    // in response to slotFooterGrandMasterValueChanged() below.
    if (m_doc->inputOutputMap()->grandMasterValue() == uchar(value))
        return;
    m_doc->inputOutputMap()->setGrandMasterValue(uchar(value));
}

void App::slotFooterGrandMasterValueChanged(uchar value)
{
    if (m_statusGrandMasterSlider != NULL)
    {
        m_statusGrandMasterSlider->blockSignals(true);
        m_statusGrandMasterSlider->setValue(value);
        m_statusGrandMasterSlider->blockSignals(false);
    }
    if (m_statusGrandMasterValueLabel != NULL)
        m_statusGrandMasterValueLabel->setText(
            QString("%1%").arg(qRound(double(value) * 100.0 / 255.0)));
}

void App::slotShowModeLock(bool checked)
{
    if (m_doc != NULL)
        m_doc->setShowLocked(checked);
}

void App::slotShowLockedChanged(bool locked)
{
    if (m_showLockAction != NULL)
    {
        m_showLockAction->setChecked(locked);
        // Closed padlock when locked, open when not — reads as a lock, not a
        // second Blackout button.
        m_showLockAction->setIcon(QIcon(locked ? ":/lock.png" : ":/unlock.png"));
    }
    if (m_statusShowLockLabel != NULL)
    {
        if (locked)
        {
            m_statusShowLockLabel->setText(tr("SHOW LOCKED"));
            m_statusShowLockLabel->show();
        }
        else
        {
            m_statusShowLockLabel->hide();
        }
    }
}

void App::slotControlPanic()
{
    m_doc->masterTimer()->stopAllFunctions();
}

void App::slotFadeAndStopAll()
{
    QAction *action = (QAction *)sender();
    int timeout = action->data().toInt();

    m_doc->masterTimer()->fadeAndStopAll(timeout);
}

void App::slotRunningFunctionsChanged()
{
    if (m_doc->masterTimer()->runningFunctions() > 0)
        m_controlPanicAction->setEnabled(true);
    else
        m_controlPanicAction->setEnabled(false);
}

void App::slotDumpDmxIntoFunction()
{
    DmxDumpFactory ddf(m_doc, m_dumpProperties, this);
    if (ddf.exec() != QDialog::Accepted)
        return;
}

void App::slotFunctionLiveEdit()
{
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType | Function::EFXType | Function::RGBMatrixType);
    fs.disableFilters(Function::ShowType | Function::ScriptType | Function::CollectionType | Function::AudioType);

    if (fs.exec() == QDialog::Accepted)
    {
        if (fs.selection().count() > 0)
        {
            FunctionLiveEditDialog fle(m_doc, fs.selection().first(), this);
            fle.exec();
        }
    }
}

void App::slotLiveEditVirtualConsole()
{
    VirtualConsole::instance()->toggleLiveEdit();
}

void App::slotCaptureLiveEdits(bool checked)
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm == NULL)
        return;

    if (checked)
    {
        cm->setCapturing(true);

        if (m_captureCoalesceTimer == nullptr)
        {
            m_captureCoalesceTimer = new QTimer(this);
            m_captureCoalesceTimer->setSingleShot(true);
            connect(m_captureCoalesceTimer, &QTimer::timeout,
                    this, &App::slotCapturePendingChanged);
        }
        if (m_captureCountConnection)
            disconnect(m_captureCountConnection);
        // overrideRecorded fires ~50 Hz (queued from the DMX thread) while a
        // control moves; coalesce the burst so buildPlan() runs at most ~10 Hz.
        m_captureCountConnection = connect(cm, &CaptureManager::overrideRecorded,
                                           this, [this](quint32, quint32, uchar) {
            if (!m_captureCoalesceTimer->isActive())
                m_captureCoalesceTimer->start(100);
        });
        connect(cm, &CaptureManager::undoStackChanged,
                this, &App::slotCaptureUndoStackChanged, Qt::UniqueConnection);
        connect(cm, &CaptureManager::changesApplied,
                this, &App::slotCapturePendingChanged, Qt::UniqueConnection);
        connect(cm, &CaptureManager::undoPerformed,
                this, [this](const QString& summary) {
            setStatusMessage(summary);
        }, Qt::UniqueConnection);
        connect(cm, &CaptureManager::autoStored,
                this, [this](const QString& summary) {
            setStatusMessage(summary);
            slotCapturePendingChanged();
        }, Qt::UniqueConnection);

        m_captureStoreAction->setVisible(true);
        m_captureUndoAction->setVisible(true);
        slotCapturePendingChanged();
        slotCaptureUndoStackChanged();
        setStatusMessage(tr("Capturing live edits — move VC sliders / XY pads to record"));
    }
    else
    {
        if (m_captureCountConnection)
        {
            disconnect(m_captureCountConnection);
            m_captureCountConnection = QMetaObject::Connection();
        }
        if (m_captureCoalesceTimer != nullptr)
            m_captureCoalesceTimer->stop();
        // Capture stopped: if anything was recorded, present the diff
        // dialog so the user can apply or save-as-new. The dialog itself
        // is responsible for clearing overrides on apply/cancel.
        if (cm->overrideCount() > 0)
        {
            LiveCaptureDialog dlg(m_doc, this);
            dlg.exec();
        }
        cm->setCapturing(false);
        m_captureStoreAction->setVisible(false);
        m_captureStoreAction->setEnabled(false);
        m_captureStoreAction->setText(tr("Store"));
        m_captureUndoAction->setVisible(false);
        m_captureUndoAction->setEnabled(false);
        clearStatusMessage();
    }
}

void App::slotCaptureStore()
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm == NULL || cm->overrideCount() == 0)
        return;
    LiveCaptureDialog dlg(m_doc, this);
    dlg.exec();
    slotCapturePendingChanged();
}

void App::slotCaptureUndo()
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm == NULL || !cm->canUndo())
        return;
    cm->undoLast();
    slotCapturePendingChanged();
}

void App::slotCapturePendingChanged()
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm == NULL)
        return;
    int channels = cm->overrideCount();
    int scenes = cm->impactedSceneCount();
    if (channels > 0)
    {
        m_captureStoreAction->setEnabled(true);
        m_captureStoreAction->setText(tr("Store (%1 scene%2, %3 ch)")
                                      .arg(scenes)
                                      .arg(scenes == 1 ? QString() : tr("s"))
                                      .arg(channels));
        setStatusMessage(tr("Capturing live edits — %1 channel(s) recorded across %2 scene(s)")
                         .arg(channels).arg(scenes));
    }
    else
    {
        m_captureStoreAction->setEnabled(false);
        m_captureStoreAction->setText(tr("Store"));
        if (cm->isCapturing())
            setStatusMessage(tr("Capturing live edits — move VC sliders / XY pads to record"));
    }
}

void App::slotCaptureUndoStackChanged()
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm == NULL)
        return;
    bool can = cm->canUndo();
    m_captureUndoAction->setEnabled(can);
    if (can)
    {
        m_captureUndoAction->setText(tr("Undo: %1").arg(cm->lastUndoLabel()));
        m_captureUndoAction->setToolTip(tr("Revert the most recent %1").arg(cm->lastUndoLabel()));
    }
    else
    {
        m_captureUndoAction->setText(tr("Undo Last Store"));
        m_captureUndoAction->setToolTip(tr("Revert the most recent capture commit"));
    }
}

void App::showEffects()
{
    showContext(tr("Build"));
    if (ProgrammingManager *pm = ProgrammingManager::instance())
        pm->setSection(ProgrammingManager::EffectsSection);
}

void App::showFunctions()
{
    if (m_functionsWindowShown)
    {
        showContext(tr("Functions"));
        return;
    }
    showContext(tr("Build"));
    if (ProgrammingManager *pm = ProgrammingManager::instance())
        pm->showClassic();
}

void App::setFunctionsWindowShown(bool shown)
{
    m_functionsWindowShown = shown;
    applyWorkspaceTabs();
}

/*****************************************************************************
 * Workspace views (View ▸ Workspace)
 *****************************************************************************/

QStringList App::workspaceViewTabs(WorkspaceView view)
{
    switch (view)
    {
    case ClassicView:
        // Stock QLC+ 4.x, in its order (Connections reads "Inputs/Outputs").
        return { tr("Fixtures"), tr("Functions"), tr("Shows"), tr("Virtual Console"),
                 tr("Simple Desk"), tr("Connections") };
    case ConsoleView:
        // Functions folds into Build ▸ Scenes ▸ Classic (board Q32). Simple
        // Desk, a QLC+ user's channel check, sits where Everything has it:
        // after the Virtual Console, before Run (Q71).
        return { tr("Connections"), tr("Fixtures"), tr("Lighting Studio"), tr("Build"),
                 tr("Show Tree"), tr("Shows"), tr("Virtual Console"), tr("Simple Desk"),
                 tr("Run") };
    case EverythingView:
    default:
        return { tr("Connections"), tr("Fixtures"), tr("Lighting Studio"), tr("Functions"),
                 tr("Build"), tr("Show Tree"), tr("Shows"), tr("Virtual Console"),
                 tr("Simple Desk"), tr("Run") };
    }
}

QString App::workspaceViewKey(WorkspaceView view)
{
    switch (view)
    {
    case ClassicView:    return QStringLiteral("classic");
    case EverythingView: return QStringLiteral("everything");
    case ConsoleView:
    default:             return QStringLiteral("qlcconsole");
    }
}

App::WorkspaceView App::workspaceViewFromKey(const QString &key, bool *ok)
{
    const QString k = key.trimmed().toLower();
    if (ok != nullptr)
        *ok = true;
    if (k == QLatin1String("classic"))
        return ClassicView;
    if (k == QLatin1String("everything"))
        return EverythingView;
    if (k == QLatin1String("qlcconsole"))
        return ConsoleView;
    if (ok != nullptr)
        *ok = false;
    return ConsoleView;
}

QString App::workspaceViewName(WorkspaceView view)
{
    switch (view)
    {
    case ClassicView:    return tr("Classic QLC+");
    case EverythingView: return tr("Everything");
    case ConsoleView:
    default:             return tr("qlcconsole");
    }
}

App::WorkspaceView App::defaultViewForLoadedShow() const
{
    /* A show with a Show Tree was made here: qlcconsole. One without, saved
       by QLC+ (or anything but qlcconsole): it opens the way QLC+ showed it. */
    const ShowTreeController *tree = m_doc != NULL ? m_doc->showTree() : nullptr;
    if (tree != nullptr && !tree->productions().isEmpty())
        return ConsoleView;
    if (m_loadedCreator == QLatin1String(APPNAME))
        return ConsoleView;
    return ClassicView;
}

QString App::tabDisplayLabel(const QString &label) const
{
    if (m_workspaceView == ClassicView && label == tr("Connections"))
        return tr("Inputs/Outputs");
    return label;
}

QStringList App::effectiveViewTabs() const
{
    QStringList order = workspaceViewTabs(m_workspaceView);
    const QString functions = tr("Functions");
    if (m_functionsWindowShown && !order.contains(functions))
    {
        // At its place among everything (after Lighting Studio).
        const QStringList all = workspaceViewTabs(EverythingView);
        const int at = all.indexOf(functions);
        int i = 0;
        while (i < order.size() && all.indexOf(order.at(i)) < at)
            ++i;
        order.insert(i, functions);
    }
    else if (!m_functionsWindowShown)
        order.removeAll(functions);
    return order;
}

bool App::tabInView(const QString &label) const
{
    return effectiveViewTabs().contains(label);
}

int App::tabInsertIndex(const QString &label) const
{
    if (m_tab == NULL)
        return 0;
    const QStringList order = effectiveViewTabs();
    const int pos = order.indexOf(label);
    if (pos < 0)
        return m_tab->count();
    int idx = 0;
    for (int t = 0; t < m_tab->count(); ++t)
    {
        const int o = order.indexOf(m_tab->tabBar()->tabData(t).toString());
        if (o >= 0 && o < pos)
            ++idx;
    }
    return idx;
}

QWidget *App::contextWidget(const QString &label) const
{
    if (m_tab != NULL)
        for (int t = 0; t < m_tab->count(); ++t)
            if (m_tab->tabBar()->tabData(t).toString() == label)
                return m_tab->widget(t);
    foreach (DetachedContext *dw, findChildren<DetachedContext *>())
        if (dw->centralWidget() != NULL && dw->centralWidget()->property("tabLabel").toString() == label)
            return dw->centralWidget();
    foreach (QWidget *w, m_parkedContexts)
        if (w != NULL && w->property("tabLabel").toString() == label)
            return w;
    return nullptr;
}

void App::setWorkspaceView(WorkspaceView view, bool markModified)
{
    const bool changed = view != m_workspaceView;
    m_workspaceView = view;
    m_functionsWindowShown = workspaceViewTabs(view).contains(tr("Functions"));
    applyWorkspaceTabs();
    if (changed)
        hideViewNotice();
    // Saved with the show, but not an edit of it: no undo step.
    if (changed && markModified && m_doc != NULL)
        m_doc->setModified();
}

void App::applyWorkspaceTabs()
{
    if (m_tab == NULL)
        return;
    QWidget *front = m_tab->currentWidget();
    const QStringList order = effectiveViewTabs();
    const QList<DetachedContext *> windows = findChildren<DetachedContext *>();
    auto inWindow = [&windows](QWidget *w) {
        foreach (DetachedContext *dw, windows)
            if (dw->centralWidget() == w)
                return true;
        return false;
    };

    /* Every tab off the bar (the widgets live on), then the view's back on in
       its order. Signals held so the bar doesn't hand its tools round for
       each step. */
    const bool blocked = m_tab->blockSignals(true);
    QHash<QString, QWidget *> docked;
    while (m_tab->count() > 0)
    {
        const QString label = m_tab->tabBar()->tabData(0).toString();
        QWidget *w = m_tab->widget(0);
        w->setProperty("tabLabel", label);
        m_tab->removeTab(0);
        docked.insert(label, w);
    }

    QHash<QString, QWidget *> widgets;
    for (int k = 0; k < m_tabOriginals.size(); ++k)
    {
        const QString label = m_tabOriginals.at(k).first;
        QWidget *w = docked.value(label, nullptr);
        if (w == nullptr)
            w = contextWidget(label);
        if (w == nullptr)
            continue;
        widgets.insert(label, w);
        w->setProperty("tabLabel", label);
        w->setProperty("tabIcon", QVariant::fromValue(m_tabOriginals.at(k).second));
        const bool want = order.contains(label);
        w->setProperty("parked", !want);
        if (want)
            m_parkedContexts.removeAll(w);
        else if (!m_parkedContexts.contains(w))
            m_parkedContexts << w;
        // In a window of its own: it stays there (parked: closing it puts
        // it away rather than back on a bar that doesn't show it).
        if (!want && !inWindow(w))
        {
            w->setParent(this);
            w->hide();
        }
    }

    foreach (const QString &label, order)
    {
        // Docked, or put away until now: on the bar. In a window: stays.
        QWidget *w = widgets.value(label, nullptr);
        if (w == nullptr || inWindow(w))
            continue;
        w->setParent(m_tab);
        const int idx = m_tab->addTab(w, w->property("tabIcon").value<QIcon>(), label);
        m_tab->tabBar()->setTabData(idx, label);
        w->setProperty("tabIndex", idx);
    }

    if (front != NULL && m_tab->indexOf(front) >= 0)
        m_tab->setCurrentWidget(front);
    m_tab->blockSignals(blocked);
    applyTabLabelMode();
    updateWorkspaceViewActions();
    slotTabChanged(m_tab->currentIndex());
}

void App::updateWorkspaceViewActions()
{
    foreach (QAction *a, m_workspaceViewActions)
        a->setChecked(a->data().toInt() == int(m_workspaceView));
}

void App::showViewNotice(const QString &text, const QString &buttonText,
                         const std::function<void()> &action)
{
    if (m_viewNotice == nullptr)
        return;
    m_viewNoticeText->setText(text);
    m_viewNoticeAction = action;
    m_viewNoticeButton->setText(buttonText);
    m_viewNoticeButton->setVisible(!buttonText.isEmpty() && action != nullptr);
    m_viewNotice->show();
}

void App::hideViewNotice()
{
    if (m_viewNotice == nullptr)
        return;
    m_viewNotice->hide();
    m_viewNoticeAction = nullptr;
}

bool App::offerConsoleViewAfterUpgrade()
{
    if (m_upgradeOfferMade || m_workspaceView != ClassicView)
        return false;
    const ShowTreeController *tree = m_doc != NULL ? m_doc->showTree() : nullptr;
    if (tree == nullptr || tree->productions().isEmpty())
        return false;              // nothing upgraded: nothing new to show
    m_upgradeOfferMade = true;
    showViewNotice(tr("This show now has a Show Tree. The qlcconsole view shows it, with Build "
                      "and Run, beside the QLC+ tabs you know."),
                   tr("Switch to the qlcconsole view"),
                   [this]() { setWorkspaceView(ConsoleView); });
    return true;
}

void App::showContext(const QString &tabLabel)
{
    /* Still docked? Switch to it and bring the main window forward.
       Matching on the label rather than the index is what makes this survive
       detaching: tabData is set once in addTab() and travels with the page
       through removeTab()/insertTab(). */
    for (int t = 0; t < m_tab->count(); ++t)
    {
        if (m_tab->tabBar()->tabData(t).toString() != tabLabel)
            continue;
        m_tab->setCurrentIndex(t);
        if (isMinimized())
            showNormal();
        raise();
        activateWindow();
        return;
    }

    /* Detached: the view IS open, it just lives in its own window now. Bring
       that window to the front. Picking an already-detached view used to do
       nothing visible, because there was no tab left here to select. */
    foreach (DetachedContext *dw, findChildren<DetachedContext *>())
    {
        QWidget *w = dw->centralWidget();
        if (w == NULL || w->property("tabLabel").toString() != tabLabel)
            continue;
        if (dw->isMinimized())
            dw->showNormal();
        else
            dw->show();
        dw->raise();
        dw->activateWindow();
        return;
    }

    /* Not in this view (View ▸ Workspace): show everything, and say so --
       the show is the same, only the tabs on show change. */
    if (!tabInView(tabLabel) && contextWidget(tabLabel) != nullptr
        && workspaceViewTabs(EverythingView).contains(tabLabel))
    {
        const WorkspaceView was = m_workspaceView;
        setWorkspaceView(EverythingView);
        if (tabInView(tabLabel))
        {
            showViewNotice(tr("%1 isn't in the %2 view: showing Everything (View ▸ Workspace).")
                               .arg(tabLabel, workspaceViewName(was)),
                           tr("Back to %1").arg(workspaceViewName(was)),
                           [this, was]() { setWorkspaceView(was); });
            showContext(tabLabel);
            return;
        }
    }

    /* Put away (parkContext()): open it in a window of its own. */
    foreach (QWidget *w, m_parkedContexts)
        if (w != NULL && w->property("tabLabel").toString() == tabLabel)
        {
            openContextWindow(w, tabLabel);
            return;
        }
}

void App::parkContext(const QString &tabLabel)
{
    for (int t = 0; t < m_tab->count(); ++t)
    {
        if (m_tab->tabBar()->tabData(t).toString() != tabLabel)
            continue;
        QWidget *context = m_tab->widget(t);
        context->setProperty("tabIndex", t);
        context->setProperty("tabIcon", QVariant::fromValue(m_tab->tabIcon(t)));
        context->setProperty("tabLabel", tabLabel);
        context->setProperty("parked", true);
        m_tab->removeTab(t);
        context->setParent(this);
        context->hide();
        m_parkedContexts << context;
        updateWindowTitle();
        return;
    }
}

QStringList App::detachedContexts() const
{
    QStringList out;
    foreach (DetachedContext *dw, findChildren<DetachedContext *>())
        if (dw->centralWidget() != NULL      // put back, a window holds nothing
            && !dw->centralWidget()->property("parked").toBool())   // no tab to go back to
            out << dw->centralWidget()->property("tabLabel").toString();
    return out;
}

void App::updateWindowMenu()
{
    const QStringList away = detachedContexts();
    foreach (QAction *jump, m_windowJumps)
    {
        const QString label = jump->data().toString();
        const QString shown = tabDisplayLabel(label);
        jump->setText(away.contains(label) ? tr("%1 — in its own window").arg(shown) : shown);
    }
    if (m_windowDockMenu != NULL)
    {
        m_windowDockMenu->clear();
        foreach (const QString &label, away)
            m_windowDockMenu->addAction(label, this, [this, label]() { reattachContext(label); });
        m_windowDockMenu->setEnabled(!away.isEmpty());
    }
    if (m_windowDockAll != NULL)
        m_windowDockAll->setEnabled(!away.isEmpty());
    if (m_windowBreakOut != NULL)
    {
        const int idx = m_tab != NULL ? m_tab->currentIndex() : -1;
        const QString label = idx >= 0 ? m_tab->tabBar()->tabData(idx).toString() : QString();
        m_windowBreakOut->setText(label.isEmpty() ? tr("Move to Its Own Window")
                                                  : tr("Move %1 to Its Own Window").arg(label));
        m_windowBreakOut->setEnabled(idx >= 0);
    }
}

#define SETTINGS_SHIFT_CLICK_BREAKS_OUT "workspace/shiftClickBreaksOut"

bool App::shiftClickBreaksOut() const
{
    return QSettings().value(SETTINGS_SHIFT_CLICK_BREAKS_OUT, true).toBool();
}

void App::setShiftClickBreaksOut(bool on)
{
    QSettings().setValue(SETTINGS_SHIFT_CLICK_BREAKS_OUT, on);
    if (m_windowShiftClick != NULL && m_windowShiftClick->isChecked() != on)
        m_windowShiftClick->setChecked(on);
}

void App::reattachContext(const QString &tabLabel)
{
    foreach (DetachedContext *dw, findChildren<DetachedContext *>())
        if (dw->centralWidget() != NULL && dw->centralWidget()->property("tabLabel").toString() == tabLabel)
        {
            dw->close();                // closing puts it back (slotReattachContext)
            showContext(tabLabel);
            return;
        }
}

void App::reattachAllContexts()
{
    foreach (DetachedContext *dw, findChildren<DetachedContext *>())
        if (dw->centralWidget() != NULL)
            dw->close();
}

void App::slotDetachContext(int index)
{
    /* Get the widget that has been shift-clicked */
    QWidget *context = m_tab->widget(index);
    context->setProperty("tabIndex", index);
    context->setProperty("tabIcon", QVariant::fromValue(m_tab->tabIcon(index)));
    // m_tab->tabText() is empty under Icons-Only tab-label mode; tabData
    // (set once in addTab()) keeps the real label regardless of display mode.
    QString tabLabel = m_tab->tabBar()->tabData(index).toString();
    if (tabLabel.isEmpty())
        tabLabel = m_tab->tabText(index);
    context->setProperty("tabLabel", tabLabel);

    qDebug() << "[TabBreakOut] detaching" << context << "tab" << index;

    // Remove the tab-bar entry (the widget itself isn't deleted) — without
    // this, the tab strip is left with a dangling entry for a page that no
    // longer lives here once setCentralWidget() below reparents it out, and
    // m_tab's currentIndex/currentChanged (which the window title tracks)
    // never shifts to reflect what's actually still showing.
    m_tab->removeTab(index);
    updateWindowTitle();
    openContextWindow(context, tabLabel);
}

void App::openContextWindow(QWidget *context, const QString &tabLabel)
{
    DetachedContext *detachedWindow = new DetachedContext(this);
    detachedWindow->setCentralWidget(context);
    detachedWindow->resize(800, 600);
    // Identify both the showfile and which tab this window holds — a
    // detached window otherwise carries no title at all.
    QString title(APPNAME);
    if (fileName().isEmpty() == false)
        title += QString(" - ") + QDir::toNativeSeparators(fileName());
    else
        title += tr(" - New Workspace");
    title += QString(" - ") + tabLabel;
    detachedWindow->setWindowTitle(title);
    detachedWindow->show();
    context->show();
    // In front, not opened behind the main window (where it looks as if the
    // Shift-click did nothing).
    detachedWindow->raise();
    detachedWindow->activateWindow();

    connect(detachedWindow, SIGNAL(closing()),
            this, SLOT(slotReattachContext()));
}

void App::slotReattachContext()
{
    DetachedContext *window = qobject_cast<DetachedContext *>(sender());

    QWidget *context = window->centralWidget();
    int tabIndex = context->property("tabIndex").toInt();
    QIcon tabIcon = context->property("tabIcon").value<QIcon>();
    QString tabLabel = context->property("tabLabel").toString();

    if (context->property("parked").toBool())
    {
        // It has no tab: put it away again until it is asked for
        context->setParent(this);
        context->hide();
        return;
    }

    // Its place in the view now (the view may have changed meanwhile).
    tabIndex = tabInsertIndex(tabLabel);
    qDebug() << "Reattaching context" << tabIndex << tabLabel << context;

    context->setParent(m_tab);
    m_tab->insertTab(tabIndex, context, tabIcon, tabLabel);
    // insertTab() creates a fresh tab-bar entry with no data of its own;
    // restore it so a later re-detach (or the window title) can still find
    // the real label under Icons-Only tab-label mode.
    m_tab->tabBar()->setTabData(tabIndex, tabLabel);
    applyTabLabelMode();
    updateWindowTitle();
}

/*****************************************************************************
 * Help action slots
 *****************************************************************************/

QString App::helpTopicFor(QWidget *w) const
{
    /* The nearest "helpTopic" up from what has focus -- a dialog, a view, a
       tab -- else, in the main window, the tab on screen. */
    for (QWidget *p = w; p != nullptr; p = p->parentWidget())
    {
        if (HelpBrowser *help = qobject_cast<HelpBrowser *>(p))
            return help->currentFile();
        const QVariant topic = p->property("helpTopic");
        if (topic.isValid() && !topic.toString().isEmpty())
            return topic.toString();
    }
    if (m_tab != nullptr && m_tab->currentWidget() != nullptr
        && (w == nullptr || w->window() == this))
    {
        const QVariant topic = m_tab->currentWidget()->property("helpTopic");
        if (topic.isValid())
            return topic.toString();
    }
    return QStringLiteral("getting-started.md");
}

void App::slotHelpContext()
{
    QWidget *w = QApplication::focusWidget();
    if (w == nullptr)
        w = QApplication::activeWindow();
    HelpBrowser::open(helpTopicFor(w));
}

void App::slotHelpIndex()
{
    QDesktopServices::openUrl(QUrl("https://docs.qlcplus.org/"));
}

void App::slotHelpAbout()
{
    AboutBox ab(this);
    ab.exec();
}

void App::showShowControlSettings()
{
    if (InputOutputManager::instance() == NULL || m_tab == NULL)
        return;
    // By label, so a Connections detached into its own window comes forward too.
    showContext(tr("Connections"));
    InputOutputManager::instance()->showShowControlPage();
}

void App::slotAppSettings()
{
    AppSettings settings(this, this);
    settings.exec();
}

void App::slotRecentFileClicked(QAction *recent)
{
    if (recent == NULL)
        return;

    QString recentAbsPath = recent->text();
    QFile testFile(recentAbsPath);
    if (testFile.exists() == false)
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("File not found!\nThe selected file has been moved or deleted."),
                              QMessageBox::Close);
        return;
    }

    /* Check that the user is aware of losing previous changes */
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("Open Workspace"), msg) == false)
    {
        /* Second thoughts... Cancel loading. */
        return;
    }

    m_workingDirectory = QFileInfo(recentAbsPath).absoluteDir();
    QSettings settings;
    settings.setValue(SETTINGS_WORKINGPATH, m_workingDirectory.absolutePath());

    /* Clear existing document data */
    clearDocument();

#ifdef DEBUG_SPEED
    speedTime.restart();
#endif

    /* Load the file */
    QFile::FileError error = loadXML(recentAbsPath);
    if (handleFileError(error) == true)
        m_doc->resetModified();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Project loaded in" << speedTime.elapsed() << "ms.";
#endif

    /* Update these in any case, since they are at least emptied now as
       a result of calling clearDocument() a few lines ago. */
    //if (FunctionManager::instance() != NULL)
    //    FunctionManager::instance()->updateTree();
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

void App::setFileName(const QString& fileName)
{
    m_fileName = fileName;
}

QString App::fileName() const
{
    return m_fileName;
}

QString App::autoSaveFileName() const
{
    return autosaveFilePath();
}

/*****************************************************************************
 * Definitions a show needs, kept on this computer
 *****************************************************************************/

namespace
{
/** Letters and digits only, lower case: "PHS Chorus" ~ "PHS-Chorus-8". */
QString squash(const QString &s)
{
    QString out;
    for (const QChar &c : s)
        if (c.isLetterOrNumber())
            out += c.toLower();
    return out;
}

/** The online-only definition files that may be @p manufacturer @p model,
 *  by their names (what is in them cannot be read without fetching them). */
QStringList candidatesFor(const QString &manufacturer, const QString &model)
{
    QStringList out;
    const QString man = squash(manufacturer), mod = squash(model);
    foreach (const QString &path, QLCFile::skippedOnlineOnly())
    {
        if (!path.endsWith(QString(KExtFixture), Qt::CaseInsensitive))
            continue;
        const QString name = squash(QFileInfo(path).completeBaseName());
        if ((!mod.isEmpty() && name.contains(mod)) || (!man.isEmpty() && name.contains(man)))
            out << path;
    }
    return out;
}
}

QStringList App::fetchFromCloud(const QStringList &paths, QWidget *parent)
{
    QStringList fetched;
    if (paths.isEmpty())
        return fetched;
    QProgressDialog progress(parent != nullptr ? parent : this);
    progress.setWindowTitle(tr("Cloud storage"));
    progress.setCancelButtonText(tr("Skip"));
    progress.setRange(0, paths.size());
    progress.setMinimumDuration(0);
    progress.setWindowModality(Qt::ApplicationModal);
    for (int i = 0; i < paths.size(); ++i)
    {
        const QString path = paths.at(i);
        progress.setValue(i);
        progress.setLabelText(tr("Waiting for the cloud to send %1…\n(%2 of %3 -- Skip to go on without the rest)")
                                  .arg(QFileInfo(path).fileName()).arg(i + 1).arg(paths.size()));
        // Reading it is what makes the cloud send it -- in a thread, so a
        // cloud that never answers holds up nothing but this window.
        auto done = std::make_shared<std::atomic<bool> >(false);
        std::thread([path, done]() {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
                f.readAll();
            done->store(true);
        }).detach();
        while (!done->load() && !progress.wasCanceled())
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (progress.wasCanceled())
            break;
        if (!QLCFile::isOnlineOnly(path))
        {
            fetched << path;
            QLCFile::forgetSkippedOnlineOnly(path);
        }
    }
    progress.setValue(paths.size());
    return fetched;
}

void App::prepareShowDefinitions(const QString &fileName)
{
    // The show itself first: not here, it cannot even be read.
    if (QLCFile::isOnlineOnly(fileName))
        fetchFromCloud(QStringList() << fileName, this);
    QFile f(fileName);
    if (QLCFile::isOnlineOnly(fileName) || !f.open(QIODevice::ReadOnly))
        return;
    const QString text = QString::fromUtf8(f.readAll());
    f.close();

    // What it names, and which of those are not to hand.
    static const QRegularExpression fixtureRe(
        QStringLiteral("<Fixture>\\s*<Manufacturer>([^<]*)</Manufacturer>\\s*<Model>([^<]*)</Model>"));
    QLCFixtureDefCache *defs = m_doc->fixtureDefCache();
    QList<QPair<QString, QString> > missing;
    QRegularExpressionMatchIterator it = fixtureRe.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const QPair<QString, QString> fx(m.captured(1).trimmed(), m.captured(2).trimmed());
        if (fx.second == QString(KXMLFixtureGeneric) || fx.second == QString(KXMLFixtureRGBPanel)
            || missing.contains(fx) || defs->fixtureDef(fx.first, fx.second) != nullptr)
            continue;
        if (defs->loadFromShowCache(fx.first, fx.second))
            continue;                               // the copy kept for shows
        missing << fx;
    }
    if (missing.isEmpty())
        return;

    // Not here, not cached: the cloud copies that may be them.
    QStringList fetch;
    for (const auto &fx : missing)
        foreach (const QString &path, candidatesFor(fx.first, fx.second))
            if (!fetch.contains(path))
                fetch << path;
    foreach (const QString &path, fetchFromCloud(fetch, this))
        defs->loadQXF(path, true);
    // And keep them, so the next run of this show needs nothing remote.
    for (const auto &fx : missing)
        defs->cacheForShows(defs->fixtureDef(fx.first, fx.second));
}

void App::cacheShowDefinitions()
{
    QLCFixtureDefCache *defs = m_doc->fixtureDefCache();
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != nullptr && !fx->definitionUnavailable())
            defs->cacheForShows(fx->fixtureDef());
}

bool App::confirmSaveWithoutDefinitions()
{
    QStringList models;
    int count = 0;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || !fx->definitionUnavailable())
            continue;
        ++count;
        const QString m = fx->unavailableManufacturer() + QLatin1Char(' ') + fx->unavailableModel();
        if (!models.contains(m))
            models << m;
    }
    if (count == 0)
        return true;
    return QMessageBox::warning(this, tr("Save"),
        tr("%n fixture(s) have no definition on this computer and are standing in as "
           "dimmers:\n\n%1\n\nThey are saved as what they really are -- their make, model "
           "and mode -- so nothing is lost. But until their definitions are back they "
           "cannot be programmed properly. Fetch them first (the footer's warning) if "
           "you can.\n\nSave anyway?", "", count).arg(models.join(QStringLiteral("\n"))),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Save;
}

void App::recoverUnavailableDefinitions(QWidget *parent)
{
    QLCFixtureDefCache *defs = m_doc->fixtureDefCache();
    /* Copied into the user folder since qlcconsole started (a show brought
       to a new computer, its definitions after it): read the folder again. */
    defs->load(QLCFixtureDefCache::userDefinitionDirectory());
    QList<Fixture *> waiting;
    QStringList fetch;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || !fx->definitionUnavailable())
            continue;
        waiting << fx;
        if (defs->fixtureDef(fx->unavailableManufacturer(), fx->unavailableModel()) != nullptr
            || defs->loadFromShowCache(fx->unavailableManufacturer(), fx->unavailableModel()))
            continue;
        foreach (const QString &path, candidatesFor(fx->unavailableManufacturer(), fx->unavailableModel()))
            if (!fetch.contains(path))
                fetch << path;
    }
    foreach (const QString &path, fetchFromCloud(fetch, parent))
        defs->loadQXF(path, true);

    // Each its own definition back -- where the mode is there and the
    // channel count is what it was patched as.
    int restored = 0;
    QStringList still;
    foreach (Fixture *fx, waiting)
    {
        QLCFixtureDef *def = defs->fixtureDef(fx->unavailableManufacturer(), fx->unavailableModel());
        QLCFixtureMode *mode = def != nullptr ? def->mode(fx->unavailableMode()) : nullptr;
        if (mode != nullptr && quint32(mode->channels().size()) == fx->channels())
        {
            defs->cacheForShows(def);
            fx->setFixtureDefinition(def, mode);
            ++restored;
        }
        else
            still << fx->name();
    }
    if (restored > 0)
        m_doc->setModified();
    AppUtil::updateFixtureReadiness(m_doc);
    QMessageBox::information(parent, tr("Definitions"),
        still.isEmpty() ? tr("%n fixture(s) have their definitions back.", "", restored)
                        : tr("%1 fixture(s) have their definitions back; these still have none: %2")
                              .arg(restored).arg(still.join(QStringLiteral(", "))));
}

QFile::FileError App::loadXML(const QString& fileName)
{
    QFile::FileError retval = QFile::NoError;

    if (fileName.isEmpty() == true)
        return QFile::OpenError;

    /* The definitions and scripts the show carries, put in use before any of
       it loads: where this computer has none, or a different one, the show's
       own (SHOW_EMBEDDING_DESIGN.md). Its folder's Fixtures/, EffectScripts/
       and RGBScripts/ too, for what neither has. */
    if (m_doc->showEmbedding()->readWorkspace(fileName))
        m_doc->showEmbedding()->apply(QFileInfo(fileName).absolutePath());

    // Every definition it needs on this computer before it loads.
    prepareShowDefinitions(fileName);

    QXmlStreamReader *doc = QLCFile::getXMLReader(fileName);
    if (doc == NULL || doc->device() == NULL || doc->hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from" << fileName;
        return QFile::ReadError;
    }

    while (!doc->atEnd())
    {
        if (doc->readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (doc->hasError())
    {
        QLCFile::releaseXMLReader(doc);
        return QFile::ResourceError;
    }

    /* Set the workspace path before loading the new XML. In this way local files
       can be loaded even if the workspace file has been moved */
    m_doc->setWorkspacePath(QFileInfo(fileName).absolutePath());

    if (doc->dtdName() == KXMLQLCWorkspace)
    {
        // The main window is already up by the time a -o/--open workspace is
        // read, so a big file reads as a frozen app without this.
        createLoadProgressDialog(fileName);

        if (loadXML(*doc) == false)
        {
            retval = QFile::ReadError;
        }
        else
        {
            setFileName(fileName);
            m_doc->resetModified();
            retval = QFile::NoError;
            cacheShowDefinitions();     // the next run of it needs nothing remote

            // Tabs constructed before this load (e.g. the app's startup-active
            // tab, built during App::startup() — BEFORE a -o/--open command-line
            // workspace has actually loaded) captured an empty doc. Force a
            // refresh now that the doc is fully populated, same as the
            // "recent file" open path already does. Without this, a workspace
            // whose saved CurrentWindow is "Monitor" starts on a Lighting
            // Studio tab that was filled from the empty doc and never
            // refilled — trusses/fixtures silently missing despite loading
            // correctly into the doc (confirmed present in Fixture Manager,
            // whose tab is built lazily on first view and so isn't affected).
            slotLoadProgress(tr("Building views"), 0);
            updateOutputReadiness();
            if (FixtureManager::instance() != NULL)
                FixtureManager::instance()->updateView();
            if (Monitor::instance() != NULL)
                Monitor::instance()->updateView();
        }
    }
    else
    {
        retval = QFile::ReadError;
        qWarning() << Q_FUNC_INFO << fileName
                   << "is not a workspace file";
    }

    destroyLoadProgressDialog();

    QLCFile::releaseXMLReader(doc);

    return retval;
}

bool App::loadXML(QXmlStreamReader& doc, bool goToConsole, bool fromMemory)
{
    if (doc.readNextStartElement() == false)
        return false;

    if (doc.name() != KXMLQLCWorkspace)
    {
        qWarning() << Q_FUNC_INFO << "Workspace node not found";
        return false;
    }

    QString activeWindowName = doc.attributes().value(KXMLQLCWorkspaceWindow).toString();
    // The view it was saved in (absent: an older show; see below).
    const QString viewKey = doc.attributes().value(KXMLQLCWorkspaceView).toString();
    m_loadedCreator.clear();
    m_upgradeOfferMade = false;

    while (doc.readNextStartElement())
    {
        if (doc.name() == KXMLQLCEngine)
        {
            m_doc->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCVirtualConsole)
        {
            VirtualConsole::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCSimpleDesk)
        {
            SimpleDesk::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLFixture)
        {
            /* Legacy support code, nowadays in Doc */
            Fixture::loader(doc, m_doc);
        }
        else if (doc.name() == KXMLQLCFunction)
        {
            /* Legacy support code, nowadays in Doc */
            Function::loader(doc, m_doc);
        }
        else if (doc.name() == KXMLQLCCreator)
        {
            /* Only who saved it: a show without a view opens in Classic QLC+
               when QLC+ saved it (defaultViewForLoadedShow()). */
            while (doc.readNextStartElement())
            {
                if (doc.name() == KXMLQLCCreatorName)
                    m_loadedCreator = doc.readElementText().trimmed();
                else
                    doc.skipCurrentElement();
            }
            /* And auto-open shutters for a show saved without it: on when
               qlcconsole saved it, off (as QLC+) otherwise. */
            m_doc->shutterAutoOpen()->setLoadingCreator(m_loadedCreator);
        }
        else if (doc.name() == KXMLShowEmbedded)
        {
            /* Read before anything else (App::loadXML(fileName)). */
            doc.skipCurrentElement();
        }
        else if (doc.name() == QLatin1String("AppState"))
        {
            // Defer restoring windows until after the full workspace is loaded
            // so that Monitor and tabs are fully initialised first.
            // Collect the state now and act after the while loop.
            while (doc.readNextStartElement())
            {
                // "MonitorWindow" (pre-tab-conversion files: Monitor was a
                // lazily-created standalone Qt::Window with its own open/
                // geometry persistence) is silently ignored now — Monitor is
                // a permanent tab like any other, always present, and if it
                // was detached into its own window that's already covered by
                // the generic "DetachedWindow" case below.
                if (doc.name() == QLatin1String("DetachedWindow"))
                {
                    QString cls = doc.attributes().value("class").toString();
                    int tabIdx  = doc.attributes().value("tabIndex").toInt();
                    QByteArray geo = QByteArray::fromBase64(
                        doc.attributes().value("geometry").toLatin1());
                    // Find the view that matches the class name and detach
                    // it: a tab, or one the current view has put away (the
                    // show's own view is applied after this, below).
                    QList<QWidget *> candidates;
                    for (int t = 0; t < m_tab->count(); ++t)
                        candidates << m_tab->widget(t);
                    candidates << m_parkedContexts;
                    for (QWidget *w : candidates)
                    {
                        if (w && QString(w->metaObject()->className()) == cls)
                        {
                            const int t = m_tab->indexOf(w);
                            w->setProperty("tabIndex", tabIdx);
                            if (t >= 0)
                            {
                                w->setProperty("tabIcon",  QVariant::fromValue(m_tab->tabIcon(t)));
                                w->setProperty("tabLabel", m_tab->tabBar()->tabData(t).toString());
                                // Without this, the tab strip is left with a
                                // dangling entry for a page that no longer
                                // lives there once setCentralWidget() below
                                // reparents it out (same fix as the
                                // interactive detach in slotDetachContext()).
                                m_tab->removeTab(t);
                            }
                            DetachedContext *dw = new DetachedContext(this);
                            dw->setCentralWidget(w);
                            if (!geo.isEmpty())
                                dw->restoreGeometry(geo);
                            else
                                dw->resize(800, 600);
                            // Saved geometry can be from a different machine's
                            // screen layout entirely (this is per-workspace-file
                            // state, unlike the main window's local QSettings),
                            // or restoreGeometry() above can have silently failed
                            // outright (Qt version/format mismatch) and left a
                            // tiny construction-time default -- fix up either way.
                            AppUtil::ensureWindowOnScreen(dw);
                            dw->show();
                            w->show();
                            connect(dw, SIGNAL(closing()), this, SLOT(slotReattachContext()));
                            break;
                        }
                    }
                }
                doc.skipCurrentElement();
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Workspace tag:" << doc.name();
            doc.skipCurrentElement();
        }
    }

    /* The show's view (View ▸ Workspace), before the tab it was saved on is
       brought forward. Not saved with one: an old QLC+ show opens in
       Classic QLC+, anything else in qlcconsole. */
    {
        bool known = false;
        WorkspaceView view = workspaceViewFromKey(viewKey, &known);
        if (!known)
            view = defaultViewForLoadedShow();
        setWorkspaceView(view, false);
    }

    if (goToConsole == true)
        // Force the active window to be Virtual Console
        setActiveWindow(VirtualConsole::staticMetaObject.className());
    else
        // Set the active window to what was saved in the workspace file
        setActiveWindow(activeWindowName);

    applyUnitPreference(true);

    // Perform post-load operations
    VirtualConsole::instance()->postLoad();

    /* What went wrong loading it: in the footer with every other problem,
       where it stays until it is fixed -- not a dialog seen once and gone
       (CONSOLIDATION_REVIEW #5). Missing definitions are left out here: the
       footer lists those fixtures already, with a fix. */
    if (fromMemory == false)
        AppUtil::updateLoadProblems(m_doc->errorLog());

    m_doc->inputOutputMap()->startUniverses();

    return true;
}

QFile::FileError App::saveXML(const QString& fileName, bool autosave)
{
    QString tempFileName(fileName);
    tempFileName += ".temp";
    QFile file(tempFileName);
    if (file.open(QIODevice::WriteOnly) == false)
        return file.error();

    QXmlStreamWriter doc(&file);
    doc.setAutoFormatting(true);
    doc.setAutoFormattingIndent(1);

    doc.writeStartDocument();
    doc.writeDTD(QString("<!DOCTYPE %1>").arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCWorkspace);
    doc.writeAttribute("xmlns", QString("%1%2").arg(KXMLQLCplusNamespace).arg(KXMLQLCWorkspace));
    /* Currently active window */
    QWidget* widget = m_tab->currentWidget();
    if (widget != NULL)
        doc.writeAttribute(KXMLQLCWorkspaceWindow, QString(widget->metaObject()->className()));
    /* The view it is shown in (View ▸ Workspace). An attribute, which QLC+
       and older qlcconsole builds read past. */
    doc.writeAttribute(KXMLQLCWorkspaceView, workspaceViewKey(m_workspaceView));

    doc.writeStartElement(KXMLQLCCreator);
    doc.writeTextElement(KXMLQLCCreatorName, APPNAME);
    doc.writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    doc.writeTextElement(KXMLQLCCreatorAuthor, QLCFile::currentUserName());
    doc.writeEndElement(); // close KXMLQLCCreator

    /* What the show uses that is not part of the app, carried in it: the
       version in use, never quietly this computer's. A .qlcc only -- a .qxw
       stays as QLC+ can read it. Before <Engine>, so it is read first. */
    if (ShowEmbedding::embedsIn(fileName))
    {
        m_doc->showEmbedding()->collect();
        m_doc->showEmbedding()->saveXML(&doc);
    }

    /* Write engine components to the XML document */
    m_doc->saveXML(&doc);

    /* Write virtual console to the XML document */
    VirtualConsole::instance()->saveXML(&doc);

    /* Write Simple Desk to the XML document */
    SimpleDesk::instance()->saveXML(&doc);

    /* Write open window state */
    doc.writeStartElement("AppState");
    // Monitor is a permanent tab now (see App::init()), not a lazily-shown
    // standalone window — its own "MonitorWindow" persistence is retired;
    // if it's currently detached, the DetachedWindow loop below already
    // captures it like any other tab (its className is "Monitor").
    const QList<DetachedContext*> detached = findChildren<DetachedContext*>();
    for (DetachedContext *dw : detached)
    {
        QWidget *ctx = dw->centralWidget();
        if (!ctx) continue;
        doc.writeStartElement("DetachedWindow");
        doc.writeAttribute("class", QString(ctx->metaObject()->className()));
        doc.writeAttribute("tabIndex", QString::number(ctx->property("tabIndex").toInt()));
        doc.writeAttribute("geometry",
            QString::fromLatin1(dw->saveGeometry().toBase64()));
        doc.writeEndElement();
    }
    doc.writeEndElement(); // AppState

    doc.writeEndElement(); // close KXMLQLCWorkspace

    /* End the document and close all the open elements */
    doc.writeEndDocument();
#ifdef Q_OS_UNIX
    // Flush just this file's data to disk before the atomic rename below.
    // A global sync() flushes EVERY dirty buffer system-wide and blocks the
    // UI thread — seconds of beachball when the workspace lives on a slow or
    // external volume (e.g. /Volumes/Ext). fsync() on this fd only writes our
    // ~tens-of-KB workspace, which is milliseconds.
    file.flush();
    if (file.handle() != -1)
        fsync(file.handle());
#endif
    file.close();

    // Save to actual requested file name
    QFile currFile(fileName);
    if (currFile.exists() && !currFile.remove())
    {
        qWarning() << "Could not erase" << fileName;
        return currFile.error();
    }
    if (!file.rename(fileName))
    {
        qWarning() << "Could not rename" << tempFileName << "to" << fileName;
        return file.error();
    }

    if (!autosave)
    {
        /* Set the file name for the current Doc instance and
           set it also in an unmodified state. */
        setFileName(fileName);
        m_doc->resetModified();
        cacheShowDefinitions();     // what it uses, kept on this computer
    }

    return QFile::NoError;
}

void App::slotLoadDocFromMemory(QString xmlData)
{
    if (xmlData.isEmpty())
        return;

    /* Clear existing document data */
    clearDocument();

    QBuffer databuf;
    databuf.setData(xmlData.simplified().toUtf8());
    databuf.open(QIODevice::ReadOnly | QIODevice::Text);

    //qDebug() << "Buffer data:" << databuf.data();
    QXmlStreamReader doc(&databuf);

    if (doc.hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from XML in memory";
        return;
    }

    while (!doc.atEnd())
    {
        if (doc.readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (doc.hasError())
    {
        qDebug() << "XML has errors:" << doc.errorString();
        return;
    }

    if (doc.dtdName() == KXMLQLCWorkspace)
        loadXML(doc, true, true);
    else
        qDebug() << "XML doesn't have a Workspace tag";
}

void App::slotSaveAutostart(QString fileName)
{
    /* Set the workspace path before saving the new XML. In this way local files
       can be loaded even if the workspace file will be moved */
    m_doc->setWorkspacePath(QFileInfo(fileName).absolutePath());

    /* Save the document and set workspace name */
    QFile::FileError error = saveXML(fileName);
    handleFileError(error);
}

/*****************************************************************************
 * Autosave
 *****************************************************************************/

void App::initAutosave()
{
    QSettings settings;

    // Load autosave settings
    m_autosaveEnabled = settings.value(SETTINGS_AUTOSAVE_ENABLED, true).toBool();
    m_autosaveInterval = settings.value(SETTINGS_AUTOSAVE_INTERVAL, DEFAULT_AUTOSAVE_INTERVAL).toInt();

    // Create autosave timer
    m_autosaveTimer = new QTimer(this);
    connect(m_autosaveTimer, SIGNAL(timeout()), this, SLOT(slotAutosave()));

    // Start timer if enabled
    if (m_autosaveEnabled && m_autosaveInterval > 0)
    {
        m_autosaveTimer->start(m_autosaveInterval * 60 * 1000);  // Convert minutes to ms
        qDebug() << "[Autosave] Enabled with interval of" << m_autosaveInterval << "minutes";
    }

    // Check for recovery file
    checkAutosaveRecovery();
}

bool App::isAutosaveEnabled() const
{
    return m_autosaveEnabled;
}

void App::setAutosaveEnabled(bool enable)
{
    m_autosaveEnabled = enable;

    QSettings settings;
    settings.setValue(SETTINGS_AUTOSAVE_ENABLED, enable);

    if (enable && m_autosaveInterval > 0)
    {
        m_autosaveTimer->start(m_autosaveInterval * 60 * 1000);
        qDebug() << "[Autosave] Enabled with interval of" << m_autosaveInterval << "minutes";
    }
    else
    {
        m_autosaveTimer->stop();
        m_lastAutosaveTime.clear();
        qDebug() << "[Autosave] Disabled";
    }

    updateStatusBar();
}

int App::autosaveInterval() const
{
    return m_autosaveInterval;
}

void App::setAutosaveInterval(int minutes)
{
    if (minutes < 1)
        minutes = 1;  // Minimum 1 minute

    m_autosaveInterval = minutes;

    QSettings settings;
    settings.setValue(SETTINGS_AUTOSAVE_INTERVAL, minutes);

    // Restart timer with new interval if enabled
    if (m_autosaveEnabled)
    {
        m_autosaveTimer->start(m_autosaveInterval * 60 * 1000);
        qDebug() << "[Autosave] Interval changed to" << minutes << "minutes";
    }
}

/*****************************************************************************
 * Tab label mode
 *****************************************************************************/

int App::tabLabelMode() const
{
    return m_tabLabelMode;
}

void App::setTabLabelMode(int mode)
{
    if (m_tabLabelMode == mode)
        return;
    m_tabLabelMode = mode;
    QSettings settings;
    settings.setValue(SETTINGS_TAB_LABEL_MODE, mode);
    applyTabLabelMode();
}

void App::applyTabLabelMode()
{
    if (!m_tab)
        return;
    const int count = m_tab->count();
    for (int i = 0; i < count; ++i)
    {
        /* By the tab's own label, not its place: with a view put away or in
           a window of its own, the places no longer line up. */
        int o = -1;
        for (int k = 0; k < m_tabOriginals.size() && o < 0; ++k)
            if (m_tabOriginals.at(k).first == m_tab->tabBar()->tabData(i).toString())
                o = k;
        if (o < 0)
            continue;
        const QString origText = tabDisplayLabel(m_tabOriginals.at(o).first);
        const QIcon   &origIcon = m_tabOriginals.at(o).second;
        switch (m_tabLabelMode)
        {
        case TabIconOnly:
            m_tab->setTabText(i, QString());
            m_tab->setTabIcon(i, origIcon);
            break;
        case TabTextOnly:
            m_tab->setTabText(i, origText);
            m_tab->setTabIcon(i, QIcon());
            break;
        default: // TabIconAndText
            m_tab->setTabText(i, origText);
            m_tab->setTabIcon(i, origIcon);
            break;
        }
    }

    // The same mode for the show controls (Design | Operate stays words).
    if (m_showControls != NULL)
        m_showControls->setLabelMode(m_tabLabelMode);

    // Keep the 2D monitor window's toolbars in sync if it is open.
    if (Monitor::instance() != NULL)
        Monitor::instance()->applyToolbarLabelMode();

    // Same for each manager's own toolbar — previously only the main
    // window's toolbar/tab bar (above) honored this setting; every
    // per-manager toolbar was stuck on Qt's default (icons only) regardless.
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->applyToolbarLabelMode();
    if (FunctionManager::instance() != NULL)
        FunctionManager::instance()->applyToolbarLabelMode();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->applyToolbarLabelMode();
    if (VirtualConsole::instance() != NULL)
        VirtualConsole::instance()->applyToolbarLabelMode();
    if (ShowManager::instance() != NULL)
        ShowManager::instance()->applyToolbarLabelMode();
    if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
        pm->applyToolbarLabelMode();

    // A tab's tools in the top bar keep their words beside the icons there,
    // whatever the tab just set.
    if (m_mainBar != NULL)
        m_mainBar->restyleHostedTools();
}

void App::switchToTabContaining(QWidget *w)
{
    if (m_tab == NULL || w == NULL)
        return;
    int idx = m_tab->indexOf(w);
    if (idx >= 0)
        m_tab->setCurrentIndex(idx);
}

int App::theme() const
{
    return m_theme;
}

QList<QPair<int, QString> > App::themeChoices()
{
    return { { ThemeDefault,     tr("Default") },
             { ThemeTan,         tr("Tan") },
             { ThemeBlue,        tr("Blue") },
             { ThemeQLCOriginal, tr("QLC+ Original") },
             { ThemeRedShift,    tr("Red Shift") },
             { ThemeVSCodeDark,  tr("VS Code Dark") } };
}

bool App::footerChipShown(FooterChip chip) const
{
    switch (chip)
    {
    case FooterLoad:         return m_showFooterLoad;
    case FooterPower:        return m_showFooterPower;
    case FooterGrandMaster:  return m_showFooterGM;
    case FooterSpeedMasters:
    default:                 return m_showFooterSpeed;
    }
}

void App::setFooterChipShown(FooterChip chip, bool on)
{
    QSettings settings;
    switch (chip)
    {
    case FooterLoad:
        m_showFooterLoad = on;
        settings.setValue(SETTINGS_SHOW_FOOTER_LOAD, on);
        if (m_statusLoadLabel != NULL)
            m_statusLoadLabel->setVisible(on);
        break;
    case FooterPower:
        m_showFooterPower = on;
        settings.setValue(SETTINGS_SHOW_FOOTER_POWER, on);
        // Turned back on, the chip stays hidden until the next
        // powerEstimateChanged repopulates it (Design mode only).
        if (m_statusPowerLabel != NULL && on == false)
            m_statusPowerLabel->hide();
        break;
    case FooterGrandMaster:
        m_showFooterGM = on;
        settings.setValue(SETTINGS_SHOW_FOOTER_GM, on);
        if (m_statusGrandMasterBox != NULL)
            m_statusGrandMasterBox->setVisible(on);
        break;
    case FooterSpeedMasters:
        m_showFooterSpeed = on;
        settings.setValue(SETTINGS_SHOW_FOOTER_SPEED, on);
        if (m_statusSpeedMasters != NULL)
            m_statusSpeedMasters->setVisible(on);
        break;
    }
}

void App::reloadFooterBeatOptions()
{
    if (m_statusSpeedMasters != NULL)
        m_statusSpeedMasters->reloadBeatOptions();
}

void App::setTheme(int theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    QSettings settings;
    settings.setValue(SETTINGS_THEME, theme);
    applyTheme();
}

void App::applyTheme()
{
    // Backstage color themes: warm/cool low-glare palettes, the kind
    // lighting consoles traditionally offer so the screen doesn't spill
    // light or wreck night vision. QPalette (not per-theme stylesheets) is
    // the mechanism: the chrome QSS (resources/qss/default.qss) already
    // reads colors via palette(...) functions rather than hardcoding them,
    // specifically so a QPalette swap here is all it takes for that chrome
    // to follow along. Content areas (panels, trees, lists, dialogs) follow
    // QPalette everywhere; some native macOS chrome (plain buttons, combo
    // boxes with no QSS of their own) only partially does, since this app
    // uses the native widget style outside of default.qss's reach — a
    // known Qt/macOS behavior, not a bug here.
    qApp->setPalette(themePalette(m_theme, m_defaultPalette));

    // Qt doesn't always re-evaluate palette(...)-based QSS on a palette
    // change alone — force the chrome stylesheet to re-polish against it.
    this->setStyleSheet(AppUtil::getStyleSheet("MAIN"));
}

QPalette App::themePalette(int theme, const QPalette &base)
{
    QPalette pal = base;

    switch (theme)
    {
    case ThemeTan:
    {
        pal.setColor(QPalette::Window, QColor("#3a3428"));
        pal.setColor(QPalette::WindowText, QColor("#ecdfc4"));
        pal.setColor(QPalette::Base, QColor("#2e2920"));
        pal.setColor(QPalette::AlternateBase, QColor("#362f24"));
        pal.setColor(QPalette::Text, QColor("#ecdfc4"));
        pal.setColor(QPalette::Button, QColor("#4a4232"));
        pal.setColor(QPalette::ButtonText, QColor("#ecdfc4"));
        pal.setColor(QPalette::Highlight, QColor("#b8863b"));
        pal.setColor(QPalette::HighlightedText, QColor("#1a1610"));
        pal.setColor(QPalette::ToolTipBase, QColor("#4a4232"));
        pal.setColor(QPalette::ToolTipText, QColor("#ecdfc4"));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#8a8168"));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8a8168"));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#8a8168"));
        break;
    }
    case ThemeBlue:
    {
        pal.setColor(QPalette::Window, QColor("#262b33"));
        pal.setColor(QPalette::WindowText, QColor("#d7e0ea"));
        pal.setColor(QPalette::Base, QColor("#1e222a"));
        pal.setColor(QPalette::AlternateBase, QColor("#262b33"));
        pal.setColor(QPalette::Text, QColor("#d7e0ea"));
        pal.setColor(QPalette::Button, QColor("#333a44"));
        pal.setColor(QPalette::ButtonText, QColor("#d7e0ea"));
        pal.setColor(QPalette::Highlight, QColor("#3f6fa8"));
        pal.setColor(QPalette::HighlightedText, QColor("#eef4fb"));
        pal.setColor(QPalette::ToolTipBase, QColor("#333a44"));
        pal.setColor(QPalette::ToolTipText, QColor("#d7e0ea"));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#767f8a"));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#767f8a"));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#767f8a"));
        break;
    }
    case ThemeQLCOriginal:
    {
        // Classic medium-grey Qt/Fusion chrome — clear contrast between
        // window/panel and button, white "paper" for text fields/lists,
        // not just a light tint. The first pass here (near-white
        // #efefef/#ffffff throughout) had too little contrast between
        // Window/Base/Button to read as anything but a flat white wash —
        // corrected per Branson's live check at the rig.
        pal.setColor(QPalette::Window, QColor("#d4d4d4"));
        pal.setColor(QPalette::WindowText, QColor("#000000"));
        pal.setColor(QPalette::Base, QColor("#ffffff"));
        pal.setColor(QPalette::AlternateBase, QColor("#eaeaea"));
        pal.setColor(QPalette::Text, QColor("#000000"));
        pal.setColor(QPalette::Button, QColor("#c0c0c0"));
        pal.setColor(QPalette::ButtonText, QColor("#000000"));
        pal.setColor(QPalette::Highlight, QColor("#3d7fc1"));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(QPalette::ToolTipBase, QColor("#ffffdc"));
        pal.setColor(QPalette::ToolTipText, QColor("#000000"));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#8a8a8a"));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8a8a8a"));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#8a8a8a"));
        break;
    }
    case ThemeRedShift:
    {
        // Night-vision-preserving: blue channel kept near-zero everywhere
        // (the same principle as a red stage torch/astronomy light), so
        // working backstage in the dark doesn't blow out your eyes'
        // adaptation. A little green is let through for legibility (pure
        // red-on-red has poor contrast between UI elements) but blue stays
        // essentially off throughout.
        pal.setColor(QPalette::Window, QColor("#241008"));
        pal.setColor(QPalette::WindowText, QColor("#ffb08a"));
        pal.setColor(QPalette::Base, QColor("#150a05"));
        pal.setColor(QPalette::AlternateBase, QColor("#1e0f08"));
        pal.setColor(QPalette::Text, QColor("#ffb08a"));
        pal.setColor(QPalette::Button, QColor("#3a1f10"));
        pal.setColor(QPalette::ButtonText, QColor("#ffb08a"));
        pal.setColor(QPalette::Highlight, QColor("#b8481f"));
        pal.setColor(QPalette::HighlightedText, QColor("#fff0e0"));
        pal.setColor(QPalette::ToolTipBase, QColor("#3a1f10"));
        pal.setColor(QPalette::ToolTipText, QColor("#ffb08a"));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#8a6250"));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8a6250"));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#8a6250"));
        break;
    }
    case ThemeVSCodeDark:
    {
        // VS Code's "Dark+" default — the editor/sidebar greys and the
        // iconic #007acc accent blue.
        pal.setColor(QPalette::Window, QColor("#1e1e1e"));
        pal.setColor(QPalette::WindowText, QColor("#d4d4d4"));
        pal.setColor(QPalette::Base, QColor("#252526"));
        pal.setColor(QPalette::AlternateBase, QColor("#2d2d30"));
        pal.setColor(QPalette::Text, QColor("#d4d4d4"));
        pal.setColor(QPalette::Button, QColor("#3c3c3c"));
        pal.setColor(QPalette::ButtonText, QColor("#d4d4d4"));
        pal.setColor(QPalette::Highlight, QColor("#007acc"));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(QPalette::ToolTipBase, QColor("#252526"));
        pal.setColor(QPalette::ToolTipText, QColor("#cccccc"));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#6a6a6a"));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6a6a6a"));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#6a6a6a"));
        break;
    }
    default: // ThemeDefault
        break;
    }

    /* Off macOS the app runs on Fusion (main.cpp; offscreen renders do too),
       which draws bevels, frames, separators and default.qss's palette(mid)/
       palette(midlight) borders and hovers from the shade roles. Left at the
       native palette's values they are a light theme's greys -- bright lines
       and pale hover blocks across a dark theme. Derive them from the theme's
       own Button/Window colours, as QPalette(button, window) would, and make
       the accent the theme's highlight rather than the system accent colour.
       The native macOS style is left exactly as it was. */
    if (theme != ThemeDefault
        && QApplication::style()->name().compare(QLatin1String("fusion"), Qt::CaseInsensitive) == 0)
    {
        const QPalette derived(pal.color(QPalette::Button), pal.color(QPalette::Window));
        const QPalette::ColorRole shades[] = { QPalette::Light, QPalette::Midlight,
                                               QPalette::Mid, QPalette::Dark,
                                               QPalette::Shadow };
        const QPalette::ColorGroup groups[] = { QPalette::Active, QPalette::Inactive,
                                                QPalette::Disabled };
        for (QPalette::ColorGroup g : groups)
            for (QPalette::ColorRole r : shades)
                pal.setColor(g, r, derived.color(g, r));
        pal.setColor(QPalette::PlaceholderText,
                     pal.color(QPalette::Disabled, QPalette::Text));
        pal.setColor(QPalette::Accent, pal.color(QPalette::Highlight));
        pal.setColor(QPalette::Link, pal.color(QPalette::Highlight).lighter(130));
    }

    return pal;
}

QString App::autosaveFilePath() const
{
    if (m_fileName.isEmpty())
    {
        // For unsaved documents, use a default location
        QString userDir;
#if defined(WIN32) || defined(Q_OS_WIN)
        LPTSTR home = (LPTSTR) malloc(256 * sizeof(TCHAR));
        GetEnvironmentVariable(TEXT("UserProfile"), home, 256);
        userDir = QString("%1/%2").arg(QString::fromUtf16(reinterpret_cast<char16_t*>(home)))
                                  .arg(USERQLCPLUSDIR);
        free(home);
#else
        userDir = QString("%1/%2").arg(getenv("HOME")).arg(USERQLCPLUSDIR);
#endif
        return userDir + QDir::separator() + "untitled.qxw.autosave";
    }

    return m_fileName + ".autosave";
}

void App::slotAutosave()
{
    // Only autosave if document has been modified
    if (m_doc == NULL || m_doc->isModified() == false)
        return;

    QString autosavePath = autosaveFilePath();

    // Ensure directory exists
    QFileInfo fi(autosavePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    qDebug() << "[Autosave] Saving to" << autosavePath;

    // Save to autosave file (similar to saveXML but without changing m_fileName)
    QString tempFileName = autosavePath + ".temp";
    QFile file(tempFileName);
    if (file.open(QIODevice::WriteOnly) == false)
    {
        qWarning() << "[Autosave] Failed to open file for writing:" << tempFileName;
        return;
    }

    QXmlStreamWriter doc(&file);
    doc.setAutoFormatting(true);
    doc.setAutoFormattingIndent(1);
    doc.writeStartDocument();
    doc.writeDTD(QString("<!DOCTYPE %1>").arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCWorkspace);
    doc.writeAttribute("xmlns", QString("%1%2").arg(KXMLQLCplusNamespace).arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCCreator);
    doc.writeTextElement(KXMLQLCCreatorName, APPNAME);
    doc.writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    doc.writeTextElement(KXMLQLCCreatorAuthor, QLCFile::currentUserName());
    doc.writeEndElement();

    // A .qlcc's recovery copy carries what the show does.
    if (ShowEmbedding::embedsIn(m_fileName))
    {
        m_doc->showEmbedding()->collect();
        m_doc->showEmbedding()->saveXML(&doc);
    }

    m_doc->saveXML(&doc);
    VirtualConsole::instance()->saveXML(&doc);
    SimpleDesk::instance()->saveXML(&doc);

    doc.writeEndElement();
    doc.writeEndDocument();
#ifdef Q_OS_UNIX
    // fsync this file only — a global sync() beachballs the UI on external
    // volumes (see saveXML). Autosave runs on a timer, so an inline global
    // sync() would periodically freeze the app too.
    file.flush();
    if (file.handle() != -1)
        fsync(file.handle());
#endif
    file.close();

    // Move temp file to actual autosave file
    QFile autosaveFile(autosavePath);
    if (autosaveFile.exists() && !autosaveFile.remove())
    {
        qWarning() << "[Autosave] Could not remove old autosave file:" << autosavePath;
        return;
    }
    if (!file.rename(autosavePath))
    {
        qWarning() << "[Autosave] Could not rename temp file to:" << autosavePath;
        return;
    }

    // Update last autosave time and the consolidated dirty/autosave/saved
    // chip. Autosave does NOT clear Doc::isModified() (it's a recovery
    // copy, not a substitute for a real save — see saveXML()'s `if
    // (!autosave)` guard around resetModified()), so the doc is still
    // "dirty" here; this label says so anyway, reassuring that the current
    // state IS captured somewhere. The very next edit re-fires
    // slotDocModified(true) (Doc::setModified() emits unconditionally, not
    // just on the dirty transition) and flips this back to "Unsaved
    // changes" on its own — no extra tracking state needed.
    m_lastAutosaveTime = QTime::currentTime().toString("hh:mm:ss");
    if (m_statusDirtyLabel != NULL)
    {
        m_statusDirtyLabel->setText(tr("Autosaved %1").arg(m_lastAutosaveTime));
        m_statusDirtyLabel->setStyleSheet("QLabel { color: gray; }");
    }

    qDebug() << "[Autosave] Successfully saved to" << autosavePath;
}

void App::checkAutosaveRecovery()
{
    // Check for autosave file for untitled documents
    QString userDir;
#if defined(WIN32) || defined(Q_OS_WIN)
    LPTSTR home = (LPTSTR) malloc(256 * sizeof(TCHAR));
    GetEnvironmentVariable(TEXT("UserProfile"), home, 256);
    userDir = QString("%1/%2").arg(QString::fromUtf16(reinterpret_cast<char16_t*>(home)))
                              .arg(USERQLCPLUSDIR);
    free(home);
#else
    userDir = QString("%1/%2").arg(getenv("HOME")).arg(USERQLCPLUSDIR);
#endif

    QString untitledAutosave = userDir + QDir::separator() + "untitled.qxw.autosave";
    QFile autosaveFile(untitledAutosave);

    if (autosaveFile.exists())
    {
        /* A modal prompt cannot be answered where there is nobody to answer it.
           On the offscreen/minimal platforms -- the unit-test gate, headless
           soak harnesses, CI -- QMessageBox renders happily and then blocks
           App::init() forever, so the app never finishes starting and the test
           binary hangs until its watchdog fires. Skip the prompt there and
           leave the file alone, so a later interactive session can still
           offer the recovery. QLC_NO_RECOVERY_PROMPT forces the same
           behaviour for scripted runs on a real display. */
        const QString platform = QGuiApplication::platformName();
        if (platform == QLatin1String("offscreen")
            || platform == QLatin1String("minimal")
            || qEnvironmentVariableIsSet("QLC_NO_RECOVERY_PROMPT"))
        {
            qDebug() << "[Autosave] Recovery file present but no interactive "
                        "display; left untouched:" << untitledAutosave;
            return;
        }

        QFileInfo fi(untitledAutosave);
        QString lastModified = QLocale().toString(fi.lastModified(), QLocale::LongFormat);

        const QString question = tr("An autosave file was found from a previous "
            "session (last modified: %1). Recover the unsaved work?").arg(lastModified);

        // During boot (m_startupWindow still open) this asks inline in that
        // same window instead of popping a separate QMessageBox -- a boot
        // sequence that's still deciding whether to load a recovery file
        // has no business spawning a second, unrelated-looking window.
        bool recover = (m_startupWindow != NULL)
                ? m_startupWindow->askYesNo(question)
                : QMessageBox::question(this, tr("Autosave Recovery"), question,
                                         QMessageBox::Yes | QMessageBox::No,
                                         QMessageBox::Yes) == QMessageBox::Yes;

        if (recover == true)
        {
            // Load the autosave file
            QFile::FileError error = loadXML(untitledAutosave);
            if (error == QFile::NoError)
            {
                // Clear the filename so user must "Save As"
                setFileName(QString());
                m_doc->setModified();
                qDebug() << "[Autosave] Recovered from" << untitledAutosave;
            }
        }
        else
        {
            // Remove the autosave file if user doesn't want it
            autosaveFile.remove();
            qDebug() << "[Autosave] Recovery declined, removed" << untitledAutosave;
        }
    }
}

void App::removeAutosaveFile()
{
    QString autosavePath = autosaveFilePath();
    QFile autosaveFile(autosavePath);

    if (autosaveFile.exists())
    {
        if (autosaveFile.remove())
            qDebug() << "[Autosave] Removed autosave file:" << autosavePath;
        else
            qWarning() << "[Autosave] Failed to remove autosave file:" << autosavePath;
    }
}

/*****************************************************************************
 * Status Bar
 *****************************************************************************/

void App::initStatusBar()
{
    QStatusBar* sb = statusBar();

    // Create mode label (far left). No stretch: a dedicated spacer centres the
    // global MTC/Load chips between the mode label and the right-hand chips.
    m_statusModeLabel = new QLabel(this);
    m_statusModeLabel->setObjectName("footerReadiness");
    m_statusModeLabel->setMinimumWidth(160);
    m_statusModeLabel->installEventFilter(this);   // click → full ShowStatus details
    sb->addWidget(m_statusModeLabel, 0);

    // Rig network gate (InputOutputMap::transmitGateStatus()), right beside
    // the readiness chip: "Output held — not on the rig network". Hidden
    // while there is nothing to say.
    m_statusRigNetLabel = new QLabel(this);
    m_statusRigNetLabel->setObjectName("footerRigNetwork");
    m_statusRigNetLabel->setCursor(Qt::PointingHandCursor);
    m_statusRigNetLabel->setProperty("helpTopic", QStringLiteral("other-improvements.md#only-transmit-on-the-rig-network"));
    m_statusRigNetLabel->installEventFilter(this);  // click → Settings, rig networks
    m_statusRigNetLabel->hide();
    sb->addWidget(m_statusRigNetLabel, 0);

    // Software Update's chip: "Update available: 0.1.1". Never a pop-up --
    // a show may be running -- just this, until it is clicked.
    m_statusUpdateLabel = new QLabel(this);
    m_statusUpdateLabel->hide();
    sb->addWidget(m_statusUpdateLabel, 0);
    if (m_noGui == false)
    {
        m_updateManager = new UpdateManager(m_doc, this);
        m_updateManager->setFooterLabel(m_statusUpdateLabel);
        connect(m_updateManager, &UpdateManager::restartRequested, this, [this]() {
            m_restartForUpdate = true;
            if (close() == false)
                m_restartForUpdate = false;     // kept open (unsaved changes, cancel)
        });
        m_updateManager->scheduleChecks();
    }
    if (m_doc != NULL)
    {
        connect(m_doc->inputOutputMap(), &InputOutputMap::transmitGateChanged,
                this, [this]() { updateRigNetworkFooter(); updateOutputReadiness(); });
        updateRigNetworkFooter();
    }

    // Grand Master footer fader (far left, right after the mode label — moved
    // here from the right-hand permanent-chip group per Branson). GM
    // previously lived only inside the Virtual Console tab, unlike its
    // safety-tier siblings (Blackout, Blind, Show Lock), all global-toolbar/
    // footer. Compact horizontal slider, always visible and adjustable
    // regardless of the active tab. Driven by the same InputOutputMap GM
    // hooks the VC-embedded GrandMasterSlider widget uses, so both stay in
    // sync. Non-permanent (addWidget, not addPermanentWidget) — like
    // m_statusModeLabel, it would be temporarily hidden by statusBar()->
    // showMessage(), which this app doesn't currently use.
    {
        QWidget *gmBox = new QWidget(this);
        gmBox->setObjectName("footerGrandMaster");
        m_statusGrandMasterBox = gmBox;
        QHBoxLayout *gmLayout = new QHBoxLayout(gmBox);
        gmLayout->setContentsMargins(0, 0, 4, 0);
        gmLayout->setSpacing(4);
        gmLayout->addWidget(new QLabel(tr("GM"), gmBox));
        m_statusGrandMasterSlider = new QSlider(Qt::Horizontal, gmBox);
        m_statusGrandMasterSlider->setRange(0, 255);
        // Cap, don't fix, the width — a hard setFixedWidth() here (stacked
        // right next to m_statusModeLabel's own hard 300px minimum, both in
        // the status bar's non-stretching left region) forced the whole
        // window's minimum size wider than some displays, making it
        // un-shrinkable and pushing it off-screen. A max lets the layout
        // actually compress this under pressure instead.
        m_statusGrandMasterSlider->setMaximumWidth(70);
        m_statusGrandMasterSlider->setToolTip(tr("Grand Master"));
        gmLayout->addWidget(m_statusGrandMasterSlider);
        m_statusGrandMasterValueLabel = new QLabel(gmBox);
        // Fixed to the widest possible reading ("100%") so dragging the
        // fader doesn't reflow every chip to its right as the digit count
        // changes (1% vs 100%) — same reasoning as m_statusLoadLabel below.
        m_statusGrandMasterValueLabel->setFixedWidth(
            QFontMetrics(font()).horizontalAdvance(QStringLiteral("100%")) + 2);
        gmLayout->addWidget(m_statusGrandMasterValueLabel);
        if (m_doc != NULL)
        {
            uchar gmVal = m_doc->inputOutputMap()->grandMasterValue();
            m_statusGrandMasterSlider->setValue(gmVal);
            m_statusGrandMasterValueLabel->setText(
                QString("%1%").arg(qRound(double(gmVal) * 100.0 / 255.0)));
            connect(m_doc->inputOutputMap(), &InputOutputMap::grandMasterValueChanged,
                    this, &App::slotFooterGrandMasterValueChanged);
        }
        connect(m_statusGrandMasterSlider, &QSlider::valueChanged,
                this, &App::slotFooterGrandMasterMoved);
        gmBox->setVisible(m_showFooterGM);   // Settings ▸ Appearance
        sb->addWidget(gmBox, 0);
    }

    // The speed masters beside the GM: each master's tempo, lit on its beat;
    // a click opens the Speed Masters window at it.
    if (m_doc != NULL)
    {
        m_statusSpeedMasters = new SpeedMastersFooter(m_doc, this);
        m_statusSpeedMasters->setVisible(m_showFooterSpeed);   // Settings ▸ Appearance
        sb->addWidget(m_statusSpeedMasters, 0);
    }

    // Spacer that pushes the centred health chips (MTC/Load, created below)
    // rightward off the mode label. The status bar's built-in message area
    // supplies the balancing stretch on their right, so the two chips sit
    // centred between the mode label and the right-hand permanent chip group.
    QWidget *centreSpacer = new QWidget(this);
    sb->addWidget(centreSpacer, 1);

    // Global system-health chips: MIDI Time Code + engine load. These are
    // app-wide (the timecode source and MasterTimer are global), so they live
    // in the status bar and stay visible across tabs. Centred (not in the
    // right-hand group) per the footer layout.
    // Monospace so the changing digits keep a constant width and don't jiggle
    // the neighbouring chips as they update.
    QFont chipFont = font();
    chipFont.setStyleHint(QFont::Monospace);
    chipFont.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

    /* What the main cue list's GO plays next (SHOW_CONTROL_DESIGN.md);
       click for GO, Back, Stop and loading another cue. Hidden when the
       show has no cue list to follow. */
    m_statusStandby = new QToolButton(this);
    m_statusStandby->setObjectName(QStringLiteral("footerStandby"));
    m_statusStandby->setAutoRaise(true);
    m_statusStandby->setPopupMode(QToolButton::InstantPopup);
    m_statusStandby->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_statusStandby->setMenu(new QMenu(m_statusStandby));
    m_statusStandby->setToolTip(tr("The main cue list: what GO (%1) plays next. Click for "
                                   "GO, Back, Stop, or to load another cue.")
                                .arg(m_cueGoAction->shortcut().toString(QKeySequence::NativeText)));
    m_statusStandby->setProperty("helpTopic", QStringLiteral("show-control.md"));
    m_statusStandby->hide();
    connect(m_statusStandby->menu(), &QMenu::aboutToShow, this, &App::slotFillStandbyMenu);
    sb->addWidget(m_statusStandby, 0);
    QTimer *standby = new QTimer(this);
    connect(standby, &QTimer::timeout, this, &App::slotUpdateStandby);
    standby->start(250);

    m_statusTimecodeLabel = new QLabel(this);
    m_statusTimecodeLabel->setObjectName("footerTimecode");
    m_statusTimecodeLabel->setFont(chipFont);
    m_statusTimecodeLabel->setToolTip(tr("MIDI Time Code. Grey: no source. "
        "Amber: connected but not advancing (spoken scene / manual GO). "
        "Green: rolling.\n\nClick to bind the timecode source / Follow MTC."));
    m_statusTimecodeLabel->setCursor(Qt::PointingHandCursor);
    m_statusTimecodeLabel->installEventFilter(this);   // click → bind menu
    sb->addWidget(m_statusTimecodeLabel, 0);

    m_statusLoadLabel = new QLabel(this);
    m_statusLoadLabel->setObjectName("footerLoad");
    m_statusLoadLabel->setFont(chipFont);
    m_statusLoadLabel->setAlignment(Qt::AlignCenter);
    // Fixed width sized to the widest reading, so the % / ms changing never
    // shifts the MTC chip left/right.
    m_statusLoadLabel->setFixedWidth(
        QFontMetrics(chipFont).horizontalAdvance(QStringLiteral("Load: 00.00 / 00 ms (000%)")) + 14);
    m_statusLoadLabel->setToolTip(tr("Engine tick compute time vs the per-tick "
        "budget. Amber above 60%, red at/over budget (dropped frames likely)."));
    m_statusLoadLabel->setVisible(m_showFooterLoad);   // Settings ▸ Appearance
    sb->addWidget(m_statusLoadLabel, 0);

    // Estimated electrical load (Design mode) — the designed peak draw across the
    // rig's power sources/circuits. Red when a circuit or UPS is over its limit.
    m_statusPowerLabel = new QLabel(this);
    m_statusPowerLabel->setFont(chipFont);
    m_statusPowerLabel->setToolTip(tr("Estimated peak electrical load across the rig "
        "(Design mode). Red = a circuit or UPS is over its rated/derated limit.\n\n"
        "Click to open the circuits editor."));
    m_statusPowerLabel->setCursor(Qt::PointingHandCursor);
    m_statusPowerLabel->installEventFilter(this);   // click → circuits dialog
    m_statusPowerLabel->hide();   // shown once a Design-mode estimate arrives
    sb->addWidget(m_statusPowerLabel, 0);

    // Move-in-black dangle warning — a Marked (positioned-but-dark) fixture
    // that no upcoming cue is about to light, i.e. a pre-set nothing is going
    // to reveal. Checked continuously regardless of Auto move-in-black being
    // on (manual marks dangle too). Amber pill; hidden when nothing dangles.
    m_statusDangleLabel = new QLabel(this);
    m_statusDangleLabel->setFont(chipFont);
    m_statusDangleLabel->setStyleSheet(
        "QLabel { color:#000000; background:#f5a623; padding:1px 6px; border-radius:3px; }");
    m_statusDangleLabel->hide();
    sb->addWidget(m_statusDangleLabel, 0);

    // "Under timeline control" chip — visible only in Operate while a show drives
    // the rig. Green = timeline driving; amber = suspended (VC takeover). Sits
    // with the centred health chips (the timeline is a global runtime state).
    m_statusTimelineLabel = new QLabel(this);
    m_statusTimelineLabel->setToolTip(tr("Timeline control. Green: the show timeline "
        "is driving the rig. Amber: suspended — the Virtual Console has taken over "
        "(the playhead keeps tracking; resume to hand it back)."));
    m_statusTimelineLabel->hide();
    sb->addWidget(m_statusTimelineLabel, 0);

    // Mirror centreSpacer on the right of the chip group: QStatusBar's own
    // built-in message-area stretch isn't a real, predictable counterpart to
    // an explicit stretch=1 spacer, so relying on it left the MTC/Load/Power
    // chips reading as right-justified instead of centred. Two matching
    // spacers bracketing the group is the reliable way to centre it.
    QWidget *centreSpacerRight = new QWidget(this);
    sb->addWidget(centreSpacerRight, 1);

    // Smooth the chip readout: anchor on each fresh position, then a ~30Hz timer
    // glides the displayed time between packets (bounded, like the show cursor),
    // so the chip reads smoothly even though MTC arrives in chunks.
    m_tcWall.start();
    m_tcDisplayTimer = new QTimer(this);
    m_tcDisplayTimer->setInterval(33);
    connect(m_tcDisplayTimer, &QTimer::timeout, this, &App::slotTimecodeStatusChanged);

    if (m_doc != NULL)
    {
        TimecodeSource *tc = m_doc->timecodeSource();
        connect(tc, &TimecodeSource::timeChanged, this, [this](quint32 ms) {
            m_tcAnchorMs = ms;                       // fresh position → re-anchor
            m_tcAnchorWallMs = m_tcWall.elapsed();
            slotTimecodeStatusChanged();
        });
        connect(tc, SIGNAL(runningChanged(bool)), this, SLOT(slotTimecodeStatusChanged()));
        // The source is picked here or in Connections' Show control & sync page.
        connect(tc, &TimecodeSource::sourceUniverseChanged,
                this, &App::slotTimecodeStatusChanged);
    }
    slotTimecodeStatusChanged();

    // Keep the timeline chip + exit button in sync with the ShowManager's
    // running/suspended state. (The singleton exists — its tab is built before
    // the status bar.)
    if (ShowManager::instance() != NULL)
    {
        connect(ShowManager::instance(), SIGNAL(timelineControlChanged()),
                this, SLOT(slotTimelineControlChanged()));
        connect(ShowManager::instance(), SIGNAL(followTimecodeChanged(bool)),
                this, SLOT(slotFollowTimecodeChanged(bool)));
        slotFollowTimecodeChanged(ShowManager::instance()->followTimecode());
    }
    slotTimelineControlChanged();

    m_healthTimer = new QTimer(this);
    m_healthTimer->setInterval(500);
    connect(m_healthTimer, SIGNAL(timeout()), this, SLOT(slotUpdateHealthFooter()));
    m_healthTimer->start();
    slotUpdateHealthFooter();

    // Programmer selection indicator (between mode and dirty). Hidden
    // when nothing is selected, otherwise shows fully-selected fixture
    // group names or a fixture count fallback.
    m_statusSelectionLabel = new QLabel(this);
    m_statusSelectionLabel->setAlignment(Qt::AlignRight);
    m_statusSelectionLabel->hide();
    sb->addPermanentWidget(m_statusSelectionLabel);

    // Pad-grid mode indicator. Shows "Pad: Fixture select" etc. when
    // the matrix is in any mode other than Off; hides otherwise.
    m_statusPadModeLabel = new QLabel(this);
    m_statusPadModeLabel->setAlignment(Qt::AlignRight);
    m_statusPadModeLabel->hide();
    sb->addPermanentWidget(m_statusPadModeLabel);

    // Show-mode lock indicator. Hidden when unlocked; bold text when on
    // (plain text + color, no icon — matches the other passive-readout
    // chips: Mode, Timecode, Power. Blackout/Blind are deliberately the
    // exception, kept visually distinct since they're meant to interrupt).
    // #a06000 (amber/caution), not Blackout's #e60000 red — Show Lock is a
    // deliberate, safe "don't let go move" toggle, not a hazard state, and
    // reusing Blackout's exact red made them read as the same alarm tier.
    // Reuses the app's existing amber convention (connectionstree.cpp,
    // universepatchgrid.cpp both already use #a06000 for "pending/caution")
    // rather than inventing a new color.
    m_statusShowLockLabel = new QLabel(this);
    m_statusShowLockLabel->setAlignment(Qt::AlignRight);
    m_statusShowLockLabel->setStyleSheet("QLabel { color: #a06000; font-weight: bold; }");
    m_statusShowLockLabel->hide();
    sb->addPermanentWidget(m_statusShowLockLabel);

    // Persistent Design/Operate indicator — see app.h for why this is
    // separate from the mode-toggle button (which shows the destination
    // mode, not the current one). Always visible.
    m_statusModeChipLabel = new QLabel(this);
    m_statusModeChipLabel->setObjectName("footerMode");
    m_statusModeChipLabel->setAlignment(Qt::AlignRight);
    m_statusModeChipLabel->setCursor(Qt::PointingHandCursor);
    m_statusModeChipLabel->setToolTip(tr("Click to switch Design/Operate mode."));
    m_statusModeChipLabel->installEventFilter(this);   // click → toggle mode
    sb->addPermanentWidget(m_statusModeChipLabel);
    // Seed directly rather than calling slotModeChanged() here — that
    // function also toggles File/New/Open and live-edit actions, which may
    // not exist yet at this point in construction. slotModeChanged() keeps
    // this in sync on every real mode change from here on.
    if (m_doc != NULL && m_doc->mode() == Doc::Operate)
    {
        m_statusModeChipLabel->setText(tr("OPERATE"));
        m_statusModeChipLabel->setStyleSheet("QLabel { font-weight: bold; color: #43a047; }");
    }
    else
    {
        m_statusModeChipLabel->setText(tr("DESIGN"));
        m_statusModeChipLabel->setStyleSheet(
            "QLabel { font-weight: bold; color: palette(text); }");
    }

    // Blackout indicator (global, same reasoning as Blind used to have its own
    // label before the whole-footer-blue treatment made it redundant —
    // Blackout doesn't own the whole footer, so it needs its own chip to be
    // visible at all). Starts empty + hidden. Driven by
    // InputOutputMap::blackoutChanged.
    m_statusBlackoutLabel = new QLabel(this);
    m_statusBlackoutLabel->setAlignment(Qt::AlignRight);
    m_statusBlackoutLabel->setStyleSheet("QLabel { color: #e60000; font-weight: bold; }");
    m_statusBlackoutLabel->hide();
    sb->addPermanentWidget(m_statusBlackoutLabel);

    // Programmer dirty indicator. Hidden when clean, red bullet + text
    // when dirty. Mirrors the in-frame SaveProgrammer button highlight.
    m_statusProgrammerLabel = new QLabel(this);
    m_statusProgrammerLabel->setAlignment(Qt::AlignRight);
    m_statusProgrammerLabel->hide();
    sb->addPermanentWidget(m_statusProgrammerLabel);

    // Consolidated Unsaved/Autosaved/Saved indicator — was three separate
    // chips (a Saved/Unsaved toggle, plus an always-visible "Autosave:
    // Enabled"/"Last autosave: HH:MM:SS" chip); folded into one, driven by
    // slotDocModified() (unsaved/saved) and the autosave completion point
    // in saveXML() (autosaved). Output-readiness moved out entirely — it
    // now lives in m_statusModeLabel's own slot (far left, see
    // updateOutputReadiness()), not a chip here.
    m_statusDirtyLabel = new QLabel(this);
    m_statusDirtyLabel->setAlignment(Qt::AlignRight);
    sb->addPermanentWidget(m_statusDirtyLabel);
    slotDocModified(m_doc != NULL ? m_doc->isModified() : false);

    if (m_doc != NULL)
    {
        connect(m_doc, SIGNAL(programmerDirtyChanged(bool)),
                this, SLOT(slotProgrammerDirtyChanged(bool)));
        connect(m_doc, SIGNAL(programmerSelectionChanged()),
                this, SLOT(slotProgrammerSelectionChanged()));
        connect(m_doc, &Doc::padModeChanged,
                this, &App::slotPadModeChanged);
        connect(m_doc, SIGNAL(showLockedChanged(bool)),
                this, SLOT(slotShowLockedChanged(bool)));
        slotProgrammerDirtyChanged(m_doc->isProgrammerDirty());
        slotProgrammerSelectionChanged();
        slotPadModeChanged(m_doc->padMode());
        slotShowLockedChanged(m_doc->isShowLocked());
    }

    // A source registers into ShowStatus, not into this widget directly
    // (see showstatus.h) -- this one connection is what makes that reach
    // the footer at all, for every current AND future source alike.
    connect(ShowStatus::instance(), &ShowStatus::changed,
            this, &App::updateStatusBar);
    /* And whether anything is patched out at all ("No outputs: preview
       only"): a universe gaining or losing its outputs, or coming and going. */
    {
        InputOutputMap *io = m_doc->inputOutputMap();
        auto watchUniverse = [this](Universe *u) {
            if (u != NULL)
                connect(u, &Universe::outputPatchesCountChanged, this, &App::updateStatusBar,
                        Qt::UniqueConnection);
        };
        foreach (Universe *u, io->universes())
            watchUniverse(u);
        connect(io, &InputOutputMap::universeAdded, this, [this, io, watchUniverse](quint32 id) {
            foreach (Universe *u, io->universes())
                if (u != NULL && u->id() == id)
                    watchUniverse(u);
            updateStatusBar();
        });
        connect(io, &InputOutputMap::universeRemoved, this, &App::updateStatusBar);
    }

    updateStatusBar();
}

QMenu *App::tabContextMenu(int index)
{
    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("tabContextMenu"));
    QWidget *view = m_tab->widget(index);
    if (view == nullptr)
        return menu;
    QAction *open = menu->addAction(tr("Open in Its Own Window"));
    open->setObjectName(QStringLiteral("tabOpenInWindow"));
    connect(open, &QAction::triggered, this, [this, view]() {
        // After the menu has gone: detaching removes the tab under it.
        QTimer::singleShot(0, this, [this, view]() {
            const int at = m_tab->indexOf(view);
            if (at >= 0)
                slotDetachContext(at);
        });
    });
    return menu;
}

bool App::eventFilter(QObject *watched, QEvent *event)
{
    /* Right-click a tab: Open in Its Own Window. A Shift-click does the same,
       but on a Mac the first click on a window that isn't in front only
       brings it forward -- the Shift-click is lost. */
    if (m_tab != NULL && watched == m_tab->tabBar() && event->type() == QEvent::ContextMenu)
    {
        QContextMenuEvent *ce = static_cast<QContextMenuEvent *>(event);
        const int idx = m_tab->tabBar()->tabAt(ce->pos());
        if (idx >= 0)
        {
            QMenu *menu = tabContextMenu(idx);
            menu->exec(ce->globalPos());
            menu->deleteLater();
            return true;
        }
    }

    if (m_tab != NULL && watched == m_tab->tabBar() && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && (me->modifiers() & Qt::ShiftModifier)
            && shiftClickBreaksOut())
        {
            const int idx = m_tab->tabBar()->tabAt(me->pos());
            if (idx >= 0)
            {
                // Not from inside the tab bar's own event: detaching removes the tab.
                QWidget *view = m_tab->widget(idx);
                QTimer::singleShot(0, this, [this, view]() {
                    const int at = m_tab->indexOf(view);
                    if (at >= 0)
                        slotDetachContext(at);
                });
                return true;
            }
        }
    }

    // Click the status-bar MTC chip → a menu to BIND the timecode: arm Follow
    // MTC and pick which universe carries the code (or accept any).
    if (watched == m_statusTimecodeLabel && event->type() == QEvent::MouseButtonPress
        && m_doc != NULL)
    {
        TimecodeSource *tc = m_doc->timecodeSource();
        QMenu menu(this);

        // --- Sync health: is the incoming clock steady and at real time? ---
        // Read-only diagnostic measured in TimecodeSource: advance rate vs wall
        // clock, inter-update jitter, and the effective update interval. Lets you
        // validate a MTC feed per configuration before trusting it to drive cues.
        if (tc != NULL && tc->lastUniverse() >= 0)
        {
            const double rate = tc->rateEstimate();
            const double jit  = tc->jitterMs();
            const double ivl  = tc->avgIntervalMs();
            QString verdict;
            switch (tc->health())
            {
                case TimecodeSource::Healthy:  verdict = tr("✓ healthy"); break;
                case TimecodeSource::Usable:   verdict = tr("~ usable"); break;
                case TimecodeSource::Unstable: verdict = tr("⚠ unstable"); break;
                default:                       verdict = tr("held (no fresh code)"); break;
            }

            QAction *sh = menu.addAction(tr("Sync: %1").arg(verdict));
            sh->setEnabled(false);
            if (tc->isRunning())
            {
                QAction *det = menu.addAction(
                    tr("   %1× real-time · jitter %2 ms · ~%3 ms/update")
                        .arg(rate, 0, 'f', 2)
                        .arg(qRound(jit))
                        .arg(qRound(ivl)));
                det->setEnabled(false);
            }
            menu.addSeparator();
        }

        if (m_followMtcAction != NULL)
        {
            menu.addAction(m_followMtcAction);
            menu.addSeparator();
        }

        // Assisted per-show offset calibration: tap in time with the music,
        // average → suggested timecodeOffset. Needs a current show to write to.
        Show *curShow = qobject_cast<Show*>(
            m_doc->function(ShowManager::instance() != NULL
                                ? ShowManager::instance()->currentShowId()
                                : Function::invalidId()));
        QAction *aCal = menu.addAction(tr("Calibrate offset…"));
        aCal->setEnabled(curShow != NULL);
        if (curShow == NULL)
            aCal->setToolTip(tr("Select a show in the Show Manager first."));
        connect(aCal, &QAction::triggered, this, [this]() { openTimecodeCalibration(); });
        menu.addSeparator();

        QAction *hdr = menu.addAction(tr("Timecode source universe"));
        hdr->setEnabled(false);

        QActionGroup *grp = new QActionGroup(&menu);
        const qint32 cur = (tc != NULL) ? tc->sourceUniverse() : -1;

        QAction *anyA = menu.addAction(tr("Any universe (auto-detect)"));
        anyA->setCheckable(true);
        anyA->setActionGroup(grp);
        anyA->setChecked(cur < 0);
        connect(anyA, &QAction::triggered, this, [this, tc]() {
            if (tc && tc->sourceUniverse() != -1)
            {
                tc->setSourceUniverse(-1);
                m_doc->setModified();      // saved with the show
            }
            slotTimecodeStatusChanged();
        });

        /* Only universes that could actually deliver a frame. Timecode reaches
           the engine down exactly one path -- QLCIOPlugin::timeCodeChanged --
           and the MIDI plugin is the only implementation that emits it, so a
           universe with no input patch, or one patched to Art-Net/OSC/DMX-USB,
           is guaranteed to stay silent. Listing every universe made the menu
           dozens of rows long with nearly all of them dead, and worse, let you
           bind the source to one that could never carry code: the chip then
           sits on "armed — waiting" forever with nothing to investigate.

           Two exceptions stay listed even when they do not qualify: the
           CURRENT selection, so a binding that lost its MIDI patch is visible
           and clearable rather than invisible, and any universe that has
           actually delivered code, so an unexpected-but-working source is
           never filtered out of its own menu. */
        int midiInputs = 0;
        for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
        {
            const quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
            InputPatch *ip = m_doc->inputOutputMap()->inputPatch(uniID);
            const bool isMidi = (ip != NULL && ip->pluginName() == "MIDI");
            const bool selected = (qint32(uniID) == cur);
            const bool sawCode = (tc != NULL && tc->lastUniverse() == qint32(uniID));

            if (isMidi)
                midiInputs++;
            else if (selected == false && sawCode == false)
                continue;

            QString nm = m_doc->inputOutputMap()->getUniverseNameByIndex(i);
            if (nm.isEmpty())
                nm = tr("Universe %1").arg(i + 1);
            if (isMidi)
                nm += QString("  —  %1").arg(ip->inputName());
            else if (ip != NULL && ip->pluginName().isEmpty() == false)
                nm += tr("  —  %1 input: cannot carry MTC").arg(ip->pluginName());
            else
                nm += tr("  —  no MIDI input");
            QAction *a = menu.addAction(nm);
            a->setCheckable(true);
            a->setActionGroup(grp);
            a->setChecked(qint32(uniID) == cur);
            connect(a, &QAction::triggered, this, [this, tc, uniID]() {
                if (tc && tc->sourceUniverse() != qint32(uniID))
                {
                    tc->setSourceUniverse(qint32(uniID));
                    m_doc->setModified();  // saved with the show
                }
                slotTimecodeStatusChanged();   // reflect the selection immediately
            });
        }

        menu.addSeparator();
        if (midiInputs == 0)
        {
            QAction *warn = menu.addAction(tr("⚠ No MIDI input patched — no MTC can arrive"));
            warn->setEnabled(false);
        }
        else if (tc != NULL && tc->lastUniverse() >= 0)
        {
            QAction *hint = menu.addAction(tr("(last code seen on universe %1)")
                                               .arg(tc->lastUniverse() + 1));
            hint->setEnabled(false);
        }
        else
        {
            QAction *hint = menu.addAction(tr("(waiting for the source to roll…)"));
            hint->setEnabled(false);
        }
        menu.addAction(tr("Patch a MIDI input (Connections)…"), this, [this]() {
            if (InputOutputManager::instance() != NULL && m_tab != NULL)
                m_tab->setCurrentWidget(InputOutputManager::instance());
        });

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        menu.exec(m_statusTimecodeLabel->mapToGlobal(me->pos()));
        return true;
    }

    // Click the status-bar Power chip → the circuits editor (the readout
    // moved here from the Programming canvas; this is its "Circuits…" button).
    if (watched == m_statusPowerLabel && event->type() == QEvent::MouseButtonPress)
    {
        if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
            pm->openCircuitsDialog();
        return true;
    }

    // Click the footer Design/Operate chip → same action the toolbar
    // toggle button triggers (trigger(), not a direct slot call, so it
    // still respects the action's own enabled state).
    if (watched == m_statusModeChipLabel && event->type() == QEvent::MouseButtonPress
        && m_modeToggleAction != NULL)
    {
        m_modeToggleAction->trigger();
        return true;
    }

    if (watched == m_statusRigNetLabel && event->type() == QEvent::MouseButtonPress)
    {
        showRigNetworkSettings();
        return true;
    }

    // Click "Not ready" → the full detail behind it (see updateStatusBar()'s
    // three densities). A no-op when nothing is registered / it currently
    // reads "Ready" — showStatusDetails() itself checks and returns.
    if (watched == m_statusModeLabel && event->type() == QEvent::MouseButtonPress)
    {
        // "No outputs: preview only" with nothing else listed: the fix is
        // giving a universe an output.
        if (m_statusModeLabel->property("previewOnly").toBool()
            && ShowStatus::instance()->allEntries().isEmpty())
            showContext(tr("Connections"));
        else
            showStatusDetails();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void App::openTimecodeCalibration()
{
    Show *curShow = qobject_cast<Show*>(
        m_doc->function(ShowManager::instance() != NULL
                            ? ShowManager::instance()->currentShowId()
                            : Function::invalidId()));
    TimecodeCalibrationDialog *d = new TimecodeCalibrationDialog(m_doc, curShow, this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->show();
    d->raise();
}

void App::captureScenarioIfRequested()
{
    // Automated UI-eyeballing hook. When QLC_SHOT_DIR is set, defer past startup
    // so layout settles, then run ONE scripted scenario that navigates to a
    // state and dumps offscreen PNG grabs (QWidget::grab() renders even under
    // QT_QPA_PLATFORM=offscreen), then quits. No-op when the env is unset, so it
    // never touches a normal interactive run.
    const QByteArray dirEnv = qgetenv("QLC_SHOT_DIR");
    if (dirEnv.isEmpty())
        return;

    QTimer::singleShot(700, this, [this, dirEnv]() {
        const QString outDir = QString::fromLocal8Bit(dirEnv);
        QDir().mkpath(outDir);

        // Grabs can come out tiny if the window never got a real size offscreen.
        if (width() < 400 || height() < 300)
            resize(1280, 860);

        auto save = [&](QWidget *w, const QString &name) {
            if (w == NULL)
                return;
            const QString path = outDir + "/" + name + ".png";
            w->grab().save(path);
            fprintf(stderr, "[SHOT] %s\n", path.toLocal8Bit().constData());
        };

        // Optionally switch to a named tab (substring, case-insensitive).
        const QString tabWant = QString::fromLocal8Bit(qgetenv("QLC_SHOT_TAB"));
        if (tabWant.isEmpty() == false && m_tab != NULL)
        {
            for (int i = 0; i < m_tab->count(); i++)
                if (m_tab->tabText(i).contains(tabWant, Qt::CaseInsensitive))
                {
                    m_tab->setCurrentIndex(i);
                    break;
                }
        }

        // In the Programming tab, select a function so its subtree nests +
        // expands. QLC_SHOT_FUNC = name substring; default = first Show.
        if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
        {
            const QString funcWant = QString::fromLocal8Bit(qgetenv("QLC_SHOT_FUNC"));
            Function *pick = NULL;
            foreach (Function *f, m_doc->functions())
            {
                if (f == NULL)
                    continue;
                if (funcWant.isEmpty() == false)
                {
                    if (f->name().contains(funcWant, Qt::CaseInsensitive))
                    {
                        pick = f;
                        break;
                    }
                }
                else if (f->type() == Function::ShowType && pick == NULL)
                {
                    pick = f;
                }
            }
            // Optional: force a configured show length so the end handle renders
            // as a distinct (red) line for screenshot verification.
            const QByteArray lenEnv = qgetenv("QLC_SHOT_SHOWLEN");
            if (lenEnv.isEmpty() == false && pick != NULL
                    && pick->type() == Function::ShowType)
                qobject_cast<Show *>(pick)->setConfiguredDuration(lenEnv.toUInt());

            if (pick != NULL)
                pm->showFunction(pick->id());
            // Expand the nav trees so nested subtrees (Show→tracks→functions)
            // are visible in the grab.
            foreach (QTreeWidget *tw, pm->findChildren<QTreeWidget *>())
                tw->expandAll();
        }

        qApp->processEvents();
        save(this, "main");

        // Focused grab of just the embedded timeline view, if present (scrolled
        // home so content + end handle are on-screen).
        if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
            if (QGraphicsView *tv = pm->findChild<QGraphicsView *>())
            {
                tv->horizontalScrollBar()->setValue(0);
                tv->verticalScrollBar()->setValue(0);
                qApp->processEvents();
                save(tv, "timeline");
            }

        // Optionally open + grab the 2D monitor window (QLC_SHOT_MONITOR=1).
        if (qgetenv("QLC_SHOT_MONITOR") == QByteArray("1"))
        {
            Monitor::createAndShow(this, m_doc);
            qApp->processEvents();
            if (Monitor *mon = Monitor::instance())
            {
                mon->resize(1150, 780);
                const QByteArray pov = qgetenv("QLC_SHOT_MONITOR_POV");
                if (!pov.isEmpty())
                    if (MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>())
                    {
                        if (pov == "front") gv->setViewPOV(MonitorGraphicsView::PovFront);
                        else if (pov == "side") gv->setViewPOV(MonitorGraphicsView::PovSide);
                    }
                qApp->processEvents();
                save(mon, "monitor");
            }
        }

        // Optionally check every collapsible-group toggle in the I/O grid and
        // grab it (QLC_SHOT_IOGROUPS=1), to verify the hidden column groups.
        if (qgetenv("QLC_SHOT_IOGROUPS") == QByteArray("1"))
        {
            foreach (QToolButton *tb, findChildren<QToolButton *>())
            {
                const QString t = tb->text();
                if (t == "Feedback" || t == "Passthrough" || t == "Input")
                    if (tb->defaultAction() && tb->defaultAction()->isCheckable()
                            && !tb->defaultAction()->isChecked())
                        tb->defaultAction()->trigger();
            }
            qApp->processEvents();
            save(this, "io_groups");
        }

        // Optionally scroll the Connections Overview grid to a given row and
        // grab it (QLC_SHOT_IOROW=<row>), to verify deep rows render.
        const QByteArray ioRowEnv = qgetenv("QLC_SHOT_IOROW");
        if (ioRowEnv.isEmpty() == false)
        {
            foreach (QTableWidget *tw, findChildren<QTableWidget *>())
            {
                if (tw->columnCount() >= 8 && tw->rowCount() > 10)
                {
                    tw->scrollToItem(tw->item(ioRowEnv.toInt(), 0),
                                     QAbstractItemView::PositionAtTop);
                    qApp->processEvents();
                    save(this, "io_scrolled");
                    break;
                }
            }
        }

        // Optionally pop + grab the Fixture Studio group editor (its 3-plane
        // canvas), one PNG per plane. QLC_SHOT_STUDIO = a group id number, or a
        // name substring, or "1"/empty to pick the first studio (hasFrame) group.
        const QByteArray studioEnv = qgetenv("QLC_SHOT_STUDIO");
        if (studioEnv.isEmpty() == false && m_doc != NULL)
        {
            MonitorProperties *mp = m_doc->monitorProperties();
            quint32 gid = 0;
            bool byNum = false;
            const quint32 asNum = QString::fromLatin1(studioEnv).toUInt(&byNum);
            foreach (const MonitorProperties::MonitorGroup &g, mp->groups())
            {
                if (g.hasFrame == false)
                    continue;
                if (byNum && asNum > 1)          // explicit id
                {
                    if (g.id == asNum) { gid = g.id; break; }
                }
                else if (studioEnv != QByteArray("1") && studioEnv.isEmpty() == false
                         && byNum == false)      // name substring
                {
                    if (g.name.contains(QString::fromLocal8Bit(studioEnv),
                                        Qt::CaseInsensitive)) { gid = g.id; break; }
                }
                if (gid == 0)                    // first studio group as fallback
                    gid = g.id;
            }
            if (gid != 0)
            {
                StudioGroupEditor *dlg = new StudioGroupEditor(m_doc, gid, this);
                dlg->resize(900, 640);
                dlg->show();
                qApp->processEvents();
                // Grab each plane: drive the plane view directly so we don't
                // depend on which combo is first in the child list.
                static const char *planeName[3] = { "top", "front", "side" };
                if (StudioPlaneView *pv = dlg->findChild<StudioPlaneView *>())
                {
                    for (int pl = 0; pl < 3; ++pl)
                    {
                        pv->setPlane(StudioPlaneView::Plane(pl));
                        qApp->processEvents();
                        save(dlg, QString("studio_%1").arg(planeName[pl]));
                    }
                }
                else
                {
                    save(dlg, "studio_top");
                }
            }
        }

        // Optionally grab the structure Studio canvas (stand/tower/truss).
        // QLC_SHOT_STRUCTURE = "kind:id" (kind 0=Stand,1=Tower,2=Truss), one PNG
        // per plane.
        const QByteArray structEnv = qgetenv("QLC_SHOT_STRUCTURE");
        if (structEnv.isEmpty() == false && m_doc != NULL)
        {
            const QList<QByteArray> parts = structEnv.split(':');
            const int kind = parts.value(0).toInt();
            const quint32 sid = parts.value(1).toUInt();
            StructureStudioView *sv = new StructureStudioView(
                m_doc, StructureStudioView::Kind(kind), sid, this);
            sv->resize(720, 560);
            sv->show();
            qApp->processEvents();
            static const char *pn[3] = { "top", "front", "side" };
            for (int pl = 0; pl < 3; ++pl)
            {
                sv->setPlane(StructureStudioView::Plane(pl));
                qApp->processEvents();
                save(sv, QString("structure_%1").arg(pn[pl]));
            }
        }

        // Optionally seed a component + grab the Studio Components browser. Saves
        // the first studio group to the library, grabs the browser, then removes
        // the seeded file so the real library is left untouched.
        if (qgetenv("QLC_SHOT_COMPONENTS") == QByteArray("1") && m_doc != NULL)
        {
            MonitorProperties *mp = m_doc->monitorProperties();
            quint32 gid = 0;
            foreach (const MonitorProperties::MonitorGroup &g, mp->groups())
                if (g.hasFrame) { gid = g.id; break; }
            QString seeded;
            if (gid != 0)
                seeded = StudioTemplate::saveToLibrary(m_doc, gid,
                             QStringLiteral("Preview Component"), NULL);
            StudioComponentBrowser *br = new StudioComponentBrowser(
                m_doc, QList<quint32>(), this);
            br->resize(640, 440);
            br->show();
            qApp->processEvents();
            save(br, "components");
            if (!seeded.isEmpty())
                StudioTemplate::removeFile(seeded);
        }

        // Optionally pop + grab the timecode calibration dialog.
        if (qgetenv("QLC_SHOT_CALIBRATE") == QByteArray("1"))
        {
            openTimecodeCalibration();
            qApp->processEvents();
            foreach (QWidget *w, qApp->topLevelWidgets())
                if (TimecodeCalibrationDialog *d =
                        qobject_cast<TimecodeCalibrationDialog *>(w))
                    save(d, "calibrate");
        }

        if (qgetenv("QLC_SHOT_STAY") != QByteArray("1"))
            qApp->quit();
    });
}

void App::slotUpdateStandby()
{
    if (m_statusStandby == NULL || m_doc == NULL)
        return;
    ShowControl *sc = m_doc->showControl();
    Chaser *list = sc->cueList(QString());
    if (list == NULL)
    {
        m_statusStandby->hide();
        return;
    }
    QString text = standbyNextText(sc, list);
    // The cue playing has sub-cues on Sub GO: what Sub GO plays first
    // ("Sub: 4.2 · Next: 5"), or that there are none left in it.
    const QString sub = standbySubCueText(list);
    if (!sub.isEmpty())
        text = sub + QStringLiteral(" \u00b7 ") + text;
    // The Show Tree's current production (P5): whose GO this is.
    if (const ShowTree::Production *prod = m_doc->showTree()->production(m_doc->currentProduction()))
        text = prod->name + QStringLiteral(" \u25b8 ") + text;
    if (m_statusStandby->text() != text)
        m_statusStandby->setText(text);
    m_statusStandby->show();
}

QString App::standbyNextText(const ShowControl *sc, const Chaser *list)
{
    if (sc == NULL || list == NULL)
        return QString();
    const int next = sc->standbyStep(list);
    if (next < 0)
        return tr("Next: (end of %1)").arg(list->name());

    const int sub = sc->standbySubCue(list);
    const Chaser *named = sub < 0 ? list : list->subCueChase(next);
    const int index = sub < 0 ? next : sub;
    QString text = tr("Next: %1").arg(ShowControl::cueName(list, next, sub));
    const QList<ChaserStep> steps = named != NULL ? named->steps() : QList<ChaserStep>();
    const Doc *doc = list->doc();
    if (doc != NULL && index < steps.count())
    {
        if (const Function *f = doc->function(steps.at(index).fid))
            text += QStringLiteral(" \u2014 ") + f->name();
    }
    return text;
}

QKeySequence App::cueSubGoShortcut()
{
    return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Return);
}

QKeySequence App::cueSubBackShortcut()
{
    return QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Return);
}

QString App::standbySubCueText(const Chaser *list)
{
    if (list == NULL || !list->isRunning())
        return QString();
    const int step = list->currentStepIndex();
    if (!list->isSubGoStep(step) || list->currentSubCueIndex() < 0)
        return QString();
    const int sub = list->nextSubCueIndex();
    if (sub < 0)
        return tr("Sub: (end of %1)").arg(list->stepCue(step));
    return tr("Sub: %1").arg(list->subCue(step, sub));
}

void App::slotFillStandbyMenu()
{
    QMenu *menu = m_statusStandby->menu();
    menu->clear();
    ShowControl *sc = m_doc->showControl();
    Chaser *list = sc->cueList(QString());
    if (list == NULL)
        return;

    menu->addAction(m_cueGoAction);
    menu->addAction(m_cueBackAction);
    if (list->hasSubGoSteps())
    {
        menu->addAction(m_cueSubGoAction);
        menu->addAction(m_cueSubBackAction);
    }
    menu->addAction(tr("Stop %1").arg(list->name()), this, [sc]() {
        sc->execute({ ShowControl::Stop, QString(), QString() });
    });
    QMenu *load = menu->addMenu(tr("Load"));
    const QList<ChaserStep> steps = list->steps();
    for (int i = 0; i < steps.count(); ++i)
    {
        const Function *f = m_doc->function(steps.at(i).fid);
        QString label = list->stepCue(i);
        if (f != NULL)
            label += QStringLiteral(" \u2014 ") + f->name();
        if (steps.at(i).skip)
            label += tr(" (skipped)");
        const QString cue = list->stepCue(i);
        QAction *a = load->addAction(label, this, [sc, cue]() {
            sc->execute({ ShowControl::Load, cue, QString() });
        });
        a->setCheckable(true);
        a->setChecked(i == sc->standbyStep(list) && sc->standbySubCue(list) < 0);

        // A cue on Sub GO: its sub-cues, indented under it (LOAD 4.2).
        if (!list->isSubGoStep(i))
            continue;
        const Chaser *inner = list->subCueChase(i);
        const QList<ChaserStep> subSteps = inner->steps();
        for (int j = 0; j < subSteps.count(); ++j)
        {
            const QString subCue = list->subCue(i, j);
            QString subLabel = QStringLiteral("    ") + subCue;
            if (const Function *sf = m_doc->function(subSteps.at(j).fid))
                subLabel += QStringLiteral(" \u2014 ") + sf->name();
            if (subSteps.at(j).skip)
                subLabel += tr(" (skipped)");
            QAction *sa = load->addAction(subLabel, this, [sc, subCue]() {
                sc->execute({ ShowControl::Load, subCue, QString() });
            });
            sa->setCheckable(true);
            sa->setChecked(i == sc->standbyStep(list) && j == sc->standbySubCue(list));
        }
    }
    // Which production GO plays (P5): the same choice as the Show Tree's
    // switcher.
    ShowTreeController *tree = m_doc->showTree();
    if (!tree->productions().isEmpty())
    {
        QMenu *prods = menu->addMenu(tr("Production"));
        QActionGroup *group = new QActionGroup(prods);
        QList<QPair<quint32, QString>> choices { { ShowTree::invalidId(), tr("None (the main cue list)") } };
        for (const ShowTree::Production &p : tree->productions())
            choices << qMakePair(p.id, p.name);
        for (const auto &c : choices)
        {
            const quint32 pid = c.first;
            QAction *a = prods->addAction(c.second, this, [tree, pid]() {
                tree->setCurrentProduction(pid);
            });
            a->setCheckable(true);
            a->setChecked(pid == tree->currentProduction());
            group->addAction(a);
        }
    }
    menu->addSeparator();
    const quint32 id = list->id();
    menu->addAction(tr("Open the Cue List"), this, [this, id]() {
        if (ProgrammingManager *pm = findChild<ProgrammingManager *>())
        {
            switchToTabContaining(pm);
            pm->showFunction(id);
        }
    });
}

void App::slotTimecodeStatusChanged()
{
    if (m_statusTimecodeLabel == NULL || m_doc == NULL)
        return;

    TimecodeSource *tc = m_doc->timecodeSource();
    const char *green = "QLabel { color:#ffffff; background:#2e7d32; padding:1px 6px; border-radius:3px; }";
    const char *amber = "QLabel { color:#000000; background:#f5a623; padding:1px 6px; border-radius:3px; }";
    const char *grey  = "QLabel { color:#dddddd; background:#555555; padding:1px 6px; border-radius:3px; }";

    // Apply a stylesheet only when it actually changes — re-parsing CSS on every
    // MTC packet is needless GUI-thread work on the timecode path.
    auto applyStyle = [this](const char *s) {
        if (m_tcLastStyle != QLatin1String(s))
        {
            m_tcLastStyle = QLatin1String(s);
            m_statusTimecodeLabel->setStyleSheet(s);
        }
    };

    if (tc->lastUniverse() < 0)
    {
        if (m_tcDisplayTimer != nullptr) m_tcDisplayTimer->stop();
        // A specific source SELECTED but no timecode received yet → show it as
        // armed/waiting (amber), not a bare grey "no source".
        if (tc->sourceUniverse() >= 0 && m_doc != NULL)
        {
            QString uname;
            InputOutputMap *iom = m_doc->inputOutputMap();
            for (quint32 i = 0; i < iom->universesCount(); i++)
                if (qint32(iom->getUniverseID(i)) == tc->sourceUniverse())
                {
                    uname = iom->getUniverseNameByIndex(i);
                    if (uname.isEmpty()) uname = tr("Universe %1").arg(i + 1);
                    break;
                }
            m_statusTimecodeLabel->setText(uname.isEmpty()
                ? tr("MTC ◌ armed — waiting")
                : tr("MTC ◌ %1 — waiting").arg(uname));
            applyStyle(amber);
        }
        else
        {
            m_statusTimecodeLabel->setText(tr("MTC: no source"));
            applyStyle(grey);
        }
        return;
    }

    // While rolling, glide the readout from the last anchor at ~real time,
    // capped so it can't run away (matches the show cursor's extrapolation).
    const bool running = tc->isRunning();
    if (m_tcDisplayTimer != nullptr)
    {
        if (running && !m_tcDisplayTimer->isActive())   m_tcDisplayTimer->start();
        else if (!running && m_tcDisplayTimer->isActive()) m_tcDisplayTimer->stop();
    }
    qint64 dt = m_tcWall.elapsed() - m_tcAnchorWallMs;
    if (dt < 0) dt = 0;
    if (dt > 400) dt = 400;   // SHOW_TC_EXTRAP_CAP_MS
    quint32 ms = running ? (m_tcAnchorMs + quint32(dt)) : m_tcAnchorMs;
    int fps = tc->fps() > 0 ? tc->fps() : 30;
    uint totalSec = ms / 1000;
    int hh = totalSec / 3600;
    int mm = (totalSec % 3600) / 60;
    int ss = totalSec % 60;
    int ff = int((ms % 1000) * fps / 1000);
    QString code = QString("%1:%2:%3:%4")
            .arg(hh, 2, 10, QChar('0')).arg(mm, 2, 10, QChar('0'))
            .arg(ss, 2, 10, QChar('0')).arg(ff, 2, 10, QChar('0'));

    // Append the run-of-show context: the current section (marker) under the
    // playhead and, if that section links a manual cue list, its next GO — so
    // the always-visible footer tells the operator where they are in the set
    // and what to fire, from any tab.
    QString ctx;
    if (ShowManager::instance() != NULL)
    {
        Show *show = qobject_cast<Show*>(
            m_doc->function(ShowManager::instance()->currentShowId()));
        if (show != NULL && show->isRunning())
        {
            const quint32 off = show->timecodeOffset();
            const quint32 posMs = (ms > off) ? ms - off : 0;
            // Past the show's end: the cursor parks at the end and the rig holds;
            // surface the overage so "complete" reads clearly, not a frozen clock.
            const quint32 endMs = show->totalDuration();
            if (endMs > 0 && posMs > endMs)
                ctx = tr("  ·  ✓ complete +%1s past end").arg((posMs - endMs) / 1000);
            QMapIterator<quint32, ShowMarker> it(show->markers());
            while (ctx.isEmpty() && it.hasNext())
            {
                it.next();
                if (posMs >= it.key() && posMs < it.value().end)
                {
                    // Prefix the section with a glyph so it reads as show
                    // content (where we are in the set), not as text arriving
                    // from the MTC stream itself.
                    ctx = QString("  ·  ▸ %1").arg(it.value().label);
                    Chaser *mc = qobject_cast<Chaser*>(
                        m_doc->function(it.value().cueListId));
                    if (mc != NULL)
                        ctx += mc->isRunning() ? tr("  ·  ▶ manual armed: %1").arg(mc->name())
                                               : tr("  ·  manual: %1").arg(mc->name());
                    break;
                }
            }
        }
    }

    if (running)
    {
        m_statusTimecodeLabel->setText(QString("MTC ● %1 @%2fps%3").arg(code).arg(fps).arg(ctx));
        applyStyle(green);
    }
    else
    {
        m_statusTimecodeLabel->setText(QString("MTC ❚❚ %1 (holding)%2").arg(code).arg(ctx));
        applyStyle(amber);
    }
}

void App::slotUpdateHealthFooter()
{
    if (m_statusLoadLabel != NULL && m_showFooterLoad && m_doc != NULL && m_doc->masterTimer() != NULL)
    {
        // Peak tick over the poll window — far more informative than the last
        // tick, which is tiny/noisy on a light show.
        //
        // Load is the CPU the tick actually burned, NOT its wall duration. The
        // wall figure includes any time the timer thread was descheduled
        // mid-tick, so on a contended machine it reported "896% load" while the
        // engine was idle — telling the operator to simplify the show when the
        // real problem was the OS not scheduling the console. The gap between
        // the two is exactly that scheduling jitter, so it is worth showing on
        // its own rather than hiding inside the load number.
        MasterTimer *mt = m_doc->masterTimer();
        double wallMs = mt->tickComputePeakMs();
        double cpuMs = mt->tickCpuPeakMs();
        double budget = double(MasterTimer::tick());
        // Platforms with no per-thread CPU clock report 0; fall back to wall.
        const bool haveCpu = cpuMs > 0.0;
        double loadMs = haveCpu ? cpuMs : wallMs;
        int pct = budget > 0 ? int((loadMs / budget) * 100.0) : 0;
        // A tick that took far longer in wall time than it spent on CPU was
        // stalled by the scheduler, not by us.
        double stallMs = haveCpu ? (wallMs - cpuMs) : 0.0;
        const bool stalled = stallMs >= 2.0;
        if (stalled)
            m_statusLoadLabel->setText(tr("Load: %1 / %2 ms (%3%)  stall %4 ms")
                                       .arg(loadMs, 0, 'f', 2).arg(int(budget))
                                       .arg(pct).arg(stallMs, 0, 'f', 0));
        else
            m_statusLoadLabel->setText(tr("Load: %1 / %2 ms (%3%)")
                                       .arg(loadMs, 0, 'f', 2).arg(int(budget)).arg(pct));
        m_statusLoadLabel->setToolTip(
            tr("Engine load: %1 ms of CPU per tick against a %2 ms budget.\n"
               "Worst tick wall time: %3 ms.\n"
               "A large gap between the two means the OS is not scheduling the\n"
               "timer thread promptly (jitter), not that the engine is busy.")
                .arg(loadMs, 0, 'f', 2).arg(int(budget)).arg(wallMs, 0, 'f', 2));
        const char *green = "QLabel { color:#ffffff; background:#2e7d32; padding:1px 6px; border-radius:3px; }";
        const char *amber = "QLabel { color:#000000; background:#f5a623; padding:1px 6px; border-radius:3px; }";
        const char *red   = "QLabel { color:#ffffff; background:#c62828; padding:1px 6px; border-radius:3px; }";
        // Only on a change: setStyleSheet() re-polishes the label (and can
        // re-lay the status bar) even when handed the same sheet, and this
        // runs twice a second for as long as the app is open.
        const QString sheet = QString::fromLatin1(pct >= 100 ? red
                                                  : ((pct >= 60 || stalled) ? amber : green));
        if (m_statusLoadLabel->styleSheet() != sheet)
            m_statusLoadLabel->setStyleSheet(sheet);
    }

    // Safety refresh for the timecode chip between watchdog transitions.
    slotTimecodeStatusChanged();
}

void App::slotControlTimelineSuspend(bool checked)
{
    if (ShowManager::instance() != NULL)
        ShowManager::instance()->setTimelineSuspended(checked);
    // The ShowManager emits timelineControlChanged(), which refreshes the chip
    // and button; call directly too in case nothing was running (no-op there).
    slotTimelineControlChanged();
}

void App::slotTimelineControlChanged()
{
    ShowManager *sm = ShowManager::instance();
    const bool active = sm != NULL && sm->timelineControlActive();
    const bool suspended = sm != NULL && sm->timelineSuspended();
    // "Running" here means a show is running in Operate (driving or suspended) —
    // the state in which the exit/resume control is meaningful.
    const bool running = active || suspended;

    if (m_statusTimelineLabel != NULL)
    {
        const char *green = "QLabel { color:#ffffff; background:#1565c0; padding:1px 6px; border-radius:3px; font-weight:bold; }";
        const char *amber = "QLabel { color:#000000; background:#f5a623; padding:1px 6px; border-radius:3px; font-weight:bold; }";
        if (active)
        {
            m_statusTimelineLabel->setText(tr("● UNDER TIMELINE CONTROL"));
            m_statusTimelineLabel->setStyleSheet(green);
            m_statusTimelineLabel->show();
        }
        else if (suspended)
        {
            m_statusTimelineLabel->setText(tr("❚❚ TIMELINE SUSPENDED — VC control"));
            m_statusTimelineLabel->setStyleSheet(amber);
            m_statusTimelineLabel->show();
        }
        else
        {
            m_statusTimelineLabel->hide();
        }
    }

    if (m_timelineSuspendAction != NULL)
    {
        m_timelineSuspendAction->setEnabled(running);
        m_timelineSuspendAction->blockSignals(true);
        m_timelineSuspendAction->setChecked(suspended);
        m_timelineSuspendAction->blockSignals(false);
        m_timelineSuspendAction->setText(suspended ? tr("Resume &Timeline Control")
                                                    : tr("Exit &Timeline Control"));
    }
}

void App::slotFollowTimecodeToggled(bool checked)
{
    if (ShowManager::instance() != NULL)
        ShowManager::instance()->setFollowTimecode(checked);
}

void App::slotFollowTimecodeChanged(bool enabled)
{
    if (m_followMtcAction != NULL && m_followMtcAction->isChecked() != enabled)
    {
        m_followMtcAction->blockSignals(true);
        m_followMtcAction->setChecked(enabled);
        m_followMtcAction->blockSignals(false);
    }
}

void App::slotLastLookToggled(bool checked)
{
    if (m_doc != NULL)
        m_doc->setLastLookEnabled(checked); // disabling also clears any held look
}

void App::slotClearLastLook()
{
    if (m_doc != NULL && m_doc->lastLook() != NULL)
        m_doc->lastLook()->clear();
}

void App::slotProgrammerSelectionChanged()
{
    if (m_statusSelectionLabel == NULL || m_doc == NULL)
        return;

    const QList<quint32> selection = m_doc->programmerSelection();
    if (selection.isEmpty())
    {
        m_statusSelectionLabel->hide();
        return;
    }

    // Find every fixture group whose entire fixture list is contained
    // in the current selection — those are "fully selected" groups,
    // worth naming explicitly. Fixtures not covered by any such group
    // are tallied as "+ N more".
    QSet<quint32> selectedSet;
    for (quint32 fid : selection)
        selectedSet.insert(fid);

    QStringList groupNames;
    QSet<quint32> coveredFixtures;
    for (FixtureGroup *grp : m_doc->fixtureGroups())
    {
        if (grp == NULL)
            continue;
        const QList<quint32> grpFixtures = grp->fixtureList();
        if (grpFixtures.isEmpty())
            continue;
        bool fullyContained = true;
        for (quint32 fid : grpFixtures)
        {
            if (!selectedSet.contains(fid))
            {
                fullyContained = false;
                break;
            }
        }
        if (fullyContained)
        {
            groupNames << grp->name();
            for (quint32 fid : grpFixtures)
                coveredFixtures.insert(fid);
        }
    }

    const int extra = selection.size() - coveredFixtures.size();
    QString text;
    if (groupNames.isEmpty())
    {
        text = tr("Selected: %n fixture(s)", "", selection.size());
    }
    else
    {
        text = tr("Selected: %1").arg(groupNames.join(QStringLiteral(", ")));
        if (extra > 0)
            text += tr(" + %n more fixture(s)", "", extra);
    }
    m_statusSelectionLabel->setText(text);
    m_statusSelectionLabel->show();
}

void App::slotPadModeChanged(Doc::PadMode mode)
{
    if (m_statusPadModeLabel == NULL)
        return;
    QString label;
    QString color = QStringLiteral("#0a8");
    switch (mode)
    {
    case Doc::PadModeFixtureSelect:
        label = tr("Fixture select");
        break;
    case Doc::PadModeGoboSelect:
        label = tr("Gobo select");
        break;
    case Doc::PadModeColorPalette:
        label = tr("Color palette");
        break;
    case Doc::PadModeOff:
    default:
        m_statusPadModeLabel->hide();
        return;
    }
    m_statusPadModeLabel->setText(
        tr("<span style='color:%1;font-weight:bold;'>"
           "▦ Pad: %2</span>").arg(color).arg(label));
    m_statusPadModeLabel->show();
}

void App::slotProgrammerDirtyChanged(bool dirty)
{
    if (m_statusProgrammerLabel == NULL)
        return;
    if (!dirty || m_doc == NULL)
    {
        m_statusProgrammerLabel->hide();
        return;
    }
    const int sceneCount = m_doc->editedSceneIds().size();
    const bool hasNew = m_doc->hasProgrammerValues();

    QStringList parts;
    if (sceneCount > 0)
        parts << tr("%n scene(s) edited", "", sceneCount);
    if (hasNew)
        parts << tr("new values pending");
    const QString detail = parts.join(QStringLiteral(", "));

    /* Two different situations wear the same red warning otherwise.
       The programmer holds a pre-edit snapshot per scene so Revert can undo an
       edit, and that snapshot deliberately SURVIVES a save -- you can save your
       work and still change your mind. But a red "● Programmer: 2 scenes
       edited" after saving reads as "you have unsaved work", which is exactly
       what the operator has just dealt with, so it teaches them to ignore the
       one indicator that matters when it is real.

       Red while the document is genuinely dirty; a calmer "held" while it is
       saved and the only thing outstanding is the ability to revert. Values
       captured in the programmer but not yet stored anywhere are always red,
       saved document or not -- those really would be lost. */
    const bool docDirty = m_doc->isModified();
    if (docDirty || hasNew)
    {
        m_statusProgrammerLabel->setText(
            tr("<span style='color:#e60000;font-weight:bold;'>"
               "● Programmer: %1</span>").arg(detail));
        m_statusProgrammerLabel->setToolTip(
            tr("The programmer is holding edits that are not saved yet."));
    }
    else
    {
        m_statusProgrammerLabel->setText(
            tr("<span style='color:#909090;font-weight:bold;'>"
               "◍ Programmer: %1 (saved, revertable)</span>").arg(detail));
        m_statusProgrammerLabel->setToolTip(
            tr("Saved. The programmer still holds each scene's pre-edit state, "
               "so Revert can undo these edits."));
    }
    m_statusProgrammerLabel->show();
}

void App::updateStatusBar()
{
    // Update mode message — a real rig-readiness problem takes priority
    // over both the transient m_statusMessage and the idle "Ready" text,
    // since "are we ready" is exactly what this slot means. The problem
    // itself may come from anywhere: ShowStatus is a small registry any
    // part of the app can declare a reason into (see showstatus.h) rather
    // than this slot hand-checking one hardcoded source, so a second and
    // third source (PMJ hardware gone missing, a fixture profile that
    // failed to load, ...) need no changes here at all.
    if (m_statusModeLabel != NULL)
    {
        // Three densities on purpose, cheapest first: the chip itself says
        // only THAT something is wrong (not what — "Not ready" stays just
        // as short as "Ready" does, and does not grow or shrink as sources
        // come and go), the tooltip lists each source's own short reason
        // (one line per ShowStatus entry, not its full explanation), and a
        // click opens every entry's complete detail text in one place.
        const QList<ShowStatus::Entry> all = ShowStatus::instance()->allEntries();
        /* An Ok entry is information (definitions that came with the show),
           not a reason the show is not ready: "Ready", with a note to click. */
        QList<ShowStatus::Entry> entries, notes;
        foreach (const ShowStatus::Entry &e, all)
            (e.severity == ShowStatus::Ok ? notes : entries) << e;
        /* Nothing patched to any output: nothing reaches a rig, so nothing
           listed can go wrong on stage. Grey, not red -- red that a class or
           a laptop session is told to ignore teaches people to ignore red.
           Universes "with no output" are what this chip already says. */
        const bool previewOnly = rigHasOutputs() == false;
        if (previewOnly)
        {
            QList<ShowStatus::Entry> toCheck;
            foreach (const ShowStatus::Entry &e, entries)
                if (e.key != QLatin1String("universes.nooutput"))
                    toCheck << e;
            if (toCheck.isEmpty() && m_statusMessage.isEmpty() == false)
            {
                m_statusModeLabel->setText(m_statusMessage);
                m_statusModeLabel->setStyleSheet(QString());
            }
            else
            {
                m_statusModeLabel->setText(toCheck.isEmpty()
                    ? tr("No outputs: preview only")
                    : tr("No outputs: preview only · %n to check", "", toCheck.count()));
                m_statusModeLabel->setStyleSheet("QLabel { color: #909090; }");
            }
            m_statusModeLabel->setCursor(Qt::PointingHandCursor);
            QStringList lines;
            lines << tr("Nothing is patched to an output, so nothing reaches a rig: "
                        "looks play on screen only. Give a universe an output in "
                        "Connections to light the rig.");
            foreach (const ShowStatus::Entry &e, all)
                if (e.key != QLatin1String("universes.nooutput"))
                    lines << (e.summary.isEmpty() ? e.key : e.summary);
            lines << QString() << (toCheck.isEmpty() && notes.isEmpty()
                                   ? tr("Click to open Connections") : tr("Click for details"));
            m_statusModeLabel->setToolTip(lines.join("\n"));
            m_statusModeLabel->setProperty("previewOnly", true);
            return;
        }
        m_statusModeLabel->setProperty("previewOnly", false);
        if (entries.isEmpty() && !notes.isEmpty())
        {
            m_statusModeLabel->setText(m_statusMessage.isEmpty()
                ? tr("Ready · %n note(s)", "", notes.count())
                : m_statusMessage + QStringLiteral(" · ") + tr("%n note(s)", "", notes.count()));
            m_statusModeLabel->setStyleSheet(QString());
            m_statusModeLabel->setCursor(Qt::PointingHandCursor);
            QStringList lines;
            foreach (const ShowStatus::Entry &e, notes)
                lines << (e.summary.isEmpty() ? e.key : e.summary);
            lines << QString() << tr("Click for details");
            m_statusModeLabel->setToolTip(lines.join("\n"));
        }
        else if (entries.isEmpty() == false)
        {
            m_statusModeLabel->setText(entries.count() == 1
                ? tr("Not ready")
                : tr("Not ready (%1)").arg(entries.count()));
            m_statusModeLabel->setStyleSheet("QLabel { color: #e04030; font-weight: bold; }");
            m_statusModeLabel->setCursor(Qt::PointingHandCursor);

            QStringList lines;
            foreach (const ShowStatus::Entry &e, all)
                lines << (e.summary.isEmpty() ? e.key : e.summary);
            lines << QString() << tr("Click for details");
            m_statusModeLabel->setToolTip(lines.join("\n"));
        }
        else
        {
            m_statusModeLabel->setStyleSheet(QString());
            m_statusModeLabel->setToolTip(QString());
            m_statusModeLabel->setCursor(Qt::ArrowCursor);
            if (m_statusMessage.isEmpty())
                m_statusModeLabel->setText(tr("Ready"));
            else
                m_statusModeLabel->setText(m_statusMessage);
        }
    }
}

QString App::rigNetworkGateSettingKey()
{
    return QStringLiteral("network/onlyTransmitOnRigNetworks");
}

QString App::rigNetworkFooterText(const InputOutputMap::TransmitGateStatus &st, bool *alarm)
{
    typedef InputOutputMap::TransmitGateStatus S;
    if (alarm)
        *alarm = st.state == S::Held || st.state == S::Overridden;

    const QString nets = st.networks.join(QStringLiteral(", "));
    const QString connected = st.connected.isEmpty() ? tr("no network")
                                                     : st.connected.join(QStringLiteral(", "));
    switch (st.state)
    {
        case S::Held:
            if (st.held >= st.networkOutputs)
                return tr("Output held — not on the rig network (%1). Connected: %2")
                        .arg(nets, connected);
            return tr("%1 of %2 outputs held — not on the rig network (%3). Connected: %4")
                    .arg(st.held).arg(st.networkOutputs).arg(nets, connected);
        case S::NotSet:
            return tr("Rig network not set — network output goes out on whatever network "
                      "this machine is on. Set it");
        case S::Overridden:
            return tr("Transmitting anyway — rig network check lifted for this session");
        case S::Off:
            return tr("Rig network check off — network output goes out on any network");
        case S::Clear:
        default:
            return QString();
    }
}

void App::updateRigNetworkFooter()
{
    if (m_statusRigNetLabel == NULL || m_doc == NULL)
        return;
    const InputOutputMap::TransmitGateStatus st = m_doc->inputOutputMap()->transmitGateStatus();
    bool alarm = false;
    const QString text = rigNetworkFooterText(st, &alarm);
    if (text.isEmpty())
    {
        m_statusRigNetLabel->hide();
        return;
    }
    m_statusRigNetLabel->setText(text);
    m_statusRigNetLabel->setStyleSheet(alarm
        ? QStringLiteral("QLabel { color: #e60000; font-weight: bold; }")
        : QStringLiteral("QLabel { color: #d08000; font-weight: bold; }"));
    QStringList tip = st.reasons;
    if (tip.isEmpty() == false)
        tip << QString();
    tip << tr("Click to set the show's rig networks.");
    m_statusRigNetLabel->setToolTip(tip.join("\n"));
    m_statusRigNetLabel->show();
}

void App::showRigNetworkSettings()
{
    AppSettings settings(this, this);
    settings.focusRigNetworks();
    settings.exec();
}

void App::showStatusDetails()
{
    if (ShowStatus::instance()->allEntries().isEmpty())
        return;
    ShowStatusDialog dlg(this);
    dlg.exec();
}

bool App::rigHasOutputs() const
{
    if (m_doc == NULL || m_doc->inputOutputMap() == NULL)
        return false;
    InputOutputMap *io = m_doc->inputOutputMap();
    for (quint32 u = 0; u < io->universesCount(); u++)
        if (io->outputPatchesCount(u) > 0)
            return true;
    return false;
}

void App::setStatusMessage(const QString& message)
{
    m_statusMessage = message;
    updateStatusBar();
}

void App::clearStatusMessage()
{
    m_statusMessage.clear();
    updateStatusBar();
}
