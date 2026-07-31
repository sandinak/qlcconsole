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
#include "functionmanager.h"
#include "programmingmanager.h"
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
#include "studiocomponentbrowser.h"
#include "studiotemplate.h"
#include "monitorproperties.h"
#include "vcframe.h"
#include "app.h"
#include "doc.h"
#include "lastlookeffect.h"

#include "qlcfixturedefcache.h"
#include "audioplugincache.h"
#include "rgbscriptscache.h"
#include "videoprovider.h"
#include "qlcconfig.h"
#include "fixturegroup.h"
#include "qlcfile.h"
#include "apputil.h"

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
#define KXMLQLCWorkspaceWindow QStringLiteral("CurrentWindow")

#define MAX_RECENT_FILES    10
#define DEFAULT_AUTOSAVE_INTERVAL 5  // 5 minutes

#define KModeTextOperate QObject::tr("Operate")
#define KModeTextDesign QObject::tr("Design")
#define KUniverseCount 4

/*****************************************************************************
 * Initialization
 *****************************************************************************/

App::App()
    : QMainWindow()
    , m_tab(NULL)
    , m_overscan(false)
    , m_noGui(false)
    , m_progressDialog(NULL)
    , m_doc(NULL)

    , m_fileNewAction(NULL)
    , m_fileOpenAction(NULL)
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
    , m_statusAutosaveLabel(NULL)
    , m_statusProgrammerLabel(NULL)
    , m_statusSelectionLabel(NULL)
    , m_statusPadModeLabel(NULL)
    , m_statusShowLockLabel(NULL)
    , m_statusBlindLabel(NULL)
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

    if (m_dumpProperties != NULL)
        delete m_dumpProperties;

    if (m_videoProvider != NULL)
        delete m_videoProvider;

    if (m_doc != NULL)
        delete m_doc;

    m_doc = NULL;
}

void App::startup()
{
#if defined(__APPLE__) || defined(Q_OS_MAC)
    createProgressDialog();
#endif

    init();
    slotModeDesign();
    slotDocModified(false);

#if defined(__APPLE__) || defined(Q_OS_MAC)
    destroyProgressDialog();
#endif

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

    setWindowIcon(QIcon(":/qlcplus.png"));

    m_tab = new QTabWidget(this);
    m_tab->setTabPosition(QTabWidget::South);
    setCentralWidget(m_tab);

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
        QScreen *scr = QGuiApplication::screenAt(frameGeometry().center());
        if (scr == NULL)
            scr = QGuiApplication::primaryScreen();
        if (scr != NULL)
        {
            const QRect avail = scr->availableGeometry();
            QRect g = geometry();
            g.setSize(g.size().boundedTo(avail.size()));
            if (g.right()  > avail.right())  g.moveRight(avail.right());
            if (g.bottom() > avail.bottom()) g.moveBottom(avail.bottom());
            if (g.left()   < avail.left())   g.moveLeft(avail.left());
            if (g.top()    < avail.top())    g.moveTop(avail.top());
            setGeometry(g);
        }
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
        m_tab->addTab(w, icon, text);
        m_tabOriginals.append(qMakePair(text, icon));
    };

    QWidget* w = new FixtureManager(m_tab, m_doc);
    addTab(w, QIcon(":/fixture.png"), tr("Fixtures"));
    w = new FunctionManager(m_tab, m_doc);
    addTab(w, QIcon(":/function.png"), tr("Functions"));
    {
        ProgrammingManager *pm = new ProgrammingManager(m_tab, m_doc);
        connect(pm, &ProgrammingManager::requestSave, this, &App::slotFileSave);
        w = pm;
    }
    addTab(w, QIcon(":/scene.png"), tr("Programming"));
    w = new ShowManager(m_tab, m_doc);
    addTab(w, QIcon(":/show.png"), tr("Shows"));
    w = new VirtualConsole(m_tab, m_doc);
    addTab(w, QIcon(":/virtualconsole.png"), tr("Virtual Console"));
    w = new SimpleDesk(m_tab, m_doc);
    addTab(w, QIcon(":/slidermatrix.png"), tr("Simple Desk"));
    w = new InputOutputManager(m_tab, m_doc);
    addTab(w, QIcon(":/input_output.png"), tr("Inputs/Outputs"));

    // Load and apply the tab label mode preference.
    {
        QSettings settings;
        m_tabLabelMode = settings.value(SETTINGS_TAB_LABEL_MODE, TabIconAndText).toInt();
    }
    applyTabLabelMode();

    // Build the native menu bar now that the workspace tabs exist, so the
    // View menu can offer accurate "jump to tab" entries.
    initMenuBar();

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    /* Detach the tab's widget onto a new window on doubleClick */
    connect(m_tab, SIGNAL(tabBarDoubleClicked(int)), this, SLOT(slotDetachContext(int)));
#endif

    // Listen to blackout changes and toggle m_controlBlackoutAction
    connect(m_doc->inputOutputMap(), SIGNAL(blackoutChanged(bool)), this, SLOT(slotBlackoutChanged(bool)));
    connect(m_doc->inputOutputMap(), SIGNAL(outputInhibitedChanged(bool)), this, SLOT(slotOutputInhibitedChanged(bool)));

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

    // Initialize status bar
    initStatusBar();

    // Initialize autosave
    initAutosave();
}

void App::setActiveWindow(const QString& name)
{
    if (name.isEmpty() == true)
        return;

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
bool App::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    //qDebug() << Q_FUNC_INFO << eventType;
    return HotPlugMonitor::parseWinEvent(message, result);
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

void App::createProgressDialog()
{
    m_progressDialog = new QProgressDialog;
    m_progressDialog->setCancelButton(NULL);
    m_progressDialog->show();
    m_progressDialog->raise();
    m_progressDialog->setRange(0, 10);
    slotSetProgressText(QString());
    QApplication::processEvents();
}

void App::destroyProgressDialog()
{
    delete m_progressDialog;
    m_progressDialog = NULL;
}

void App::slotSetProgressText(const QString& text)
{
    if (m_progressDialog == NULL)
        return;

    static int progress = 0;
    m_progressDialog->setValue(progress++);
    m_progressDialog->setLabelText(QString("<B>%1</B><BR/>%2")
                                   .arg(tr("Starting Q Light Controller Plus"))
                                   .arg(text));
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
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
    SimpleDesk::instance()->clearContents();
    m_doc->inputOutputMap()->resetUniverses();
    setFileName(QString());
    m_doc->resetModified();
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
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
    connect(m_doc, SIGNAL(needAutosave()), this, SLOT(slotDocAutosave()));
    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged(Doc::Mode)));
#ifdef DEBUG_SPEED
    speedTime.start();
#endif
    /* Load user fixtures first so that they override system fixtures */
    m_doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    m_doc->fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());

    /* Load channel modifiers templates */
    m_doc->modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
    m_doc->modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());

    /* Load RGB scripts */
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::systemScriptsDirectory());
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());

    /* Load plugins */
    connect(m_doc->ioPluginCache(), SIGNAL(pluginLoaded(const QString&)),
            this, SLOT(slotSetProgressText(const QString&)));
    m_doc->ioPluginCache()->load(IOPluginCache::systemPluginDirectory());

    /* Load audio decoder plugins
     * This doesn't use a AudioPluginCache::systemPluginDirectory() cause
     * otherwise the qlcconfig.h creation should have been moved into the
     * audio folder, which doesn't make much sense */
    m_doc->audioPluginCache()->load(QLCFile::systemDirectory(AUDIOPLUGINDIR, KExtPlugin));

    /* Restore outputmap settings */
    Q_ASSERT(m_doc->inputOutputMap() != NULL);

    /* Load input plugins & profiles */
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::userProfileDirectory());
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::systemProfileDirectory());
    m_doc->inputOutputMap()->loadDefaults();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Doc initialization took" << speedTime.elapsed() << "ms";
#endif

    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
}

void App::slotDocModified(bool state)
{
    QString caption(APPNAME);

    if (fileName().isEmpty() == false)
        caption += QString(" - ") + QDir::toNativeSeparators(fileName());
    else
        caption += tr(" - New Workspace");

    if (state == true)
        setWindowTitle(caption + QString(" *"));
    else
        setWindowTitle(caption);

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

void App::slotDocAutosave()
{
    saveXML(autoSaveFileName(), true);
}

void App::slotUniverseWritten(quint32 idx, const QByteArray &ua)
{
    foreach (Fixture *fixture, m_doc->fixtures())
    {
        if (fixture->universe() != idx)
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

    // No need for the toolbar
    delete m_toolbar;
    m_toolbar = NULL;
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

    m_fileSaveAction = new QAction(QIcon(":/filesave.png"), tr("&Save"), this);
    m_fileSaveAction->setShortcut(QKeySequence("CTRL+S"));
    m_fileSaveAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_fileSaveAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSave()));

    m_fileSaveAsAction = new QAction(QIcon(":/filesaveas.png"), tr("Save &As..."), this);
    connect(m_fileSaveAsAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSaveAs()));

    /* Control actions */
    m_modeToggleAction = new QAction(QIcon(":/operate.png"), tr("&Operate"), this);
    m_modeToggleAction->setToolTip(tr("Switch to operate mode"));
    m_modeToggleAction->setShortcut(QKeySequence("CTRL+F12"));
    connect(m_modeToggleAction, SIGNAL(triggered(bool)), this, SLOT(slotModeToggle()));

    m_controlMonitorAction = new QAction(QIcon(":/monitor.png"), tr("&Monitor"), this);
    m_controlMonitorAction->setShortcut(QKeySequence("CTRL+M"));
    connect(m_controlMonitorAction, SIGNAL(triggered(bool)), this, SLOT(slotControlMonitor()));

    m_addressToolAction = new QAction(QIcon(":/diptool.png"), tr("Address Tool"), this);
    connect(m_addressToolAction, SIGNAL(triggered()), this, SLOT(slotAddressTool()));

    m_controlBlackoutAction = new QAction(QIcon(":/blackout.png"), tr("Toggle &Blackout"), this);
    m_controlBlackoutAction->setCheckable(true);
    connect(m_controlBlackoutAction, SIGNAL(triggered(bool)), this, SLOT(slotControlBlackout()));
    m_controlBlackoutAction->setChecked(m_doc->inputOutputMap()->blackout());

    // Blind — global output inhibit: mute the physical rig while the 2D preview
    // keeps updating, so looks can be built without hitting the stage. Design-mode
    // only (disabled in Operate; forced off on →Operate). Sibling of Blackout.
    m_controlBlindAction = new QAction(QIcon(":/blind.png"), tr("Toggle Bl&ind"), this);
    m_controlBlindAction->setCheckable(true);
    m_controlBlindAction->setToolTip(tr(
        "Blind: mute the physical rig but keep the 2D preview live, so you can "
        "build a look without hitting the stage. Turn off to take it live. "
        "Design mode only."));
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

    QAction *fade10 = new QAction(tr("Fade 10 second and stop"), this);
    fade10->setData(QVariant(10000));
    connect(fade10, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade10);

    QAction *fade30 = new QAction(tr("Fade 30 second and stop"), this);
    fade30->setData(QVariant(30000));
    connect(fade30, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade30);

    m_controlPanicAction->setMenu(m_fadeAndStopMenu);

    /* Help actions */
    m_helpIndexAction = new QAction(QIcon(":/help.png"), tr("&Index"), this);
    m_helpIndexAction->setShortcut(QKeySequence("SHIFT+F1"));
    connect(m_helpIndexAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpIndex()));

    m_helpAboutAction = new QAction(QIcon(":/qlcplus.png"), tr("&About QLC+"), this);
    // macOS relocates this to the application menu ("About QLC+").
    m_helpAboutAction->setMenuRole(QAction::AboutRole);
    connect(m_helpAboutAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpAbout()));

    /* Settings action */
    m_appSettingsAction = new QAction(QIcon(":/configure.png"), tr("&Settings"), this);
    m_appSettingsAction->setShortcut(QKeySequence(tr("CTRL+,", "Settings")));
    // macOS relocates this to the application menu ("Preferences…").
    m_appSettingsAction->setMenuRole(QAction::PreferencesRole);
    connect(m_appSettingsAction, SIGNAL(triggered(bool)), this, SLOT(slotAppSettings()));

    if (QLCFile::hasWindowManager() == false)
    {
        m_quitAction = new QAction(QIcon(":/exit.png"), tr("Quit QLC+"), this);
        m_quitAction->setShortcut(QKeySequence("CTRL+ALT+Backspace"));
        connect(m_quitAction, SIGNAL(triggered(bool)), this, SLOT(close()));
    }
}

void App::initToolBar()
{
    m_toolbar = new QToolBar(tr("Workspace"), this);
    m_toolbar->setFloatable(false);
    m_toolbar->setMovable(false);
    m_toolbar->setAllowedAreas(Qt::TopToolBarArea);
    m_toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
    addToolBar(m_toolbar);

    // Native-first layout: the full command set lives in the menu bar (see
    // initMenuBar). The toolbar keeps only the handful of live show controls
    // you reach for mid-show. A flexible spacer flushes them to the right so
    // they sit away from the workspace tabs, where the Operate button has
    // always been.
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_toolbar->addAction(m_controlPanicAction);   // Stop ALL (hold = fade menu)
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_controlBlackoutAction);
    m_toolbar->addAction(m_controlBlindAction);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_modeToggleAction);      // Design/Operate (rightmost)

    QToolButton* btn = qobject_cast<QToolButton*>(m_toolbar->widgetForAction(m_controlPanicAction));
    if (btn != NULL)
        btn->setPopupMode(QToolButton::DelayedPopup); // click = stop, hold = fade menu
}

void App::initMenuBar()
{
    QMenuBar* mb = menuBar();

    /* ---- File ---- */
    QMenu* fileMenu = mb->addMenu(tr("&File"));
    fileMenu->addAction(m_fileNewAction);
    fileMenu->addAction(m_fileOpenAction);
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
            quitAction = new QAction(tr("Quit QLC+"), this);
            connect(quitAction, SIGNAL(triggered(bool)), this, SLOT(close()));
        }
        quitAction->setMenuRole(QAction::QuitRole);
        quitAction->setShortcut(QKeySequence::Quit);
        fileMenu->addAction(quitAction);
    }

    /* ---- View: tools + jump to a workspace tab ---- */
    QMenu* viewMenu = mb->addMenu(tr("&View"));
    viewMenu->addAction(m_controlMonitorAction);
    viewMenu->addAction(m_addressToolAction);
    viewMenu->addSeparator();
    // Jump-to-tab entries, taken from the real tab list (m_tabOriginals keeps
    // each tab's label/icon regardless of the icon-only/text-only display mode).
    for (int i = 0; i < m_tabOriginals.size(); i++)
    {
        QAction* jump = viewMenu->addAction(m_tabOriginals.at(i).second,
                                            m_tabOriginals.at(i).first);
        // Ctrl+Shift+<n> (⇧⌘n on macOS): the plain Ctrl+<n> row is taken by
        // the Function Manager's "add function" shortcuts.
        jump->setShortcut(QKeySequence(QString("CTRL+SHIFT+%1").arg(i + 1)));
        connect(jump, &QAction::triggered, this, [this, i]() {
            if (i < m_tab->count())
                m_tab->setCurrentIndex(i);
        });
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

    /* ---- Help ---- */
    QMenu* helpMenu = mb->addMenu(tr("&Help"));
    helpMenu->addAction(m_helpIndexAction);
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
    filters << tr("Workspaces (*%1)").arg(KExtWorkspace);
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

QFile::FileError App::slotFileSave()
{
    QFile::FileError error;
    QString asfName = autoSaveFileName();

    /* Attempt to save with the existing name. Fall back to Save As. */
    if (fileName().isEmpty() == true)
        error = slotFileSaveAs();
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

    /* Create a file save dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Save Workspace As"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile(fileName());

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("Workspaces (*%1)").arg(KExtWorkspace);
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

    /* Always use the workspace suffix */
    if (fn.right(4) != KExtWorkspace)
        fn += KExtWorkspace;

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

/*****************************************************************************
 * Control action slots
 *****************************************************************************/

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

    if (m_statusBlindLabel != NULL)
    {
        m_statusBlindLabel->setText(state ? tr("● BLIND — rig muted, preview only ") : QString());
        m_statusBlindLabel->setVisible(state);
    }

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
            m_statusShowLockLabel->setText(
                tr("<span style='color:#e60000;font-weight:bold;'>"
                   "🔒 Show locked</span>"));
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

void App::slotDetachContext(int index)
{
    /* Get the widget that has been double-clicked */
    QWidget *context = m_tab->widget(index);
    context->setProperty("tabIndex", index);
    context->setProperty("tabIcon", QVariant::fromValue(m_tab->tabIcon(index)));
    context->setProperty("tabLabel", m_tab->tabText(index));

    qDebug() << "Detaching context" << context;

    DetachedContext *detachedWindow = new DetachedContext(this);
    detachedWindow->setCentralWidget(context);
    detachedWindow->resize(800, 600);
    detachedWindow->show();
    context->show();

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

    qDebug() << "Reattaching context" << tabIndex << tabLabel << context;

    context->setParent(m_tab);
    m_tab->insertTab(tabIndex, context, tabIcon, tabLabel);
}

/*****************************************************************************
 * Help action slots
 *****************************************************************************/

void App::slotHelpIndex()
{
    QDesktopServices::openUrl(QUrl("https://docs.qlcplus.org/"));
}

void App::slotHelpAbout()
{
    AboutBox ab(this);
    ab.exec();
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

QFile::FileError App::loadXML(const QString& fileName)
{
    QFile::FileError retval = QFile::NoError;

    if (fileName.isEmpty() == true)
        return QFile::OpenError;

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
        if (loadXML(*doc) == false)
        {
            retval = QFile::ReadError;
        }
        else
        {
            setFileName(fileName);
            m_doc->resetModified();
            retval = QFile::NoError;
        }
    }
    else
    {
        retval = QFile::ReadError;
        qWarning() << Q_FUNC_INFO << fileName
                   << "is not a workspace file";
    }

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
            /* Ignore creator information */
            doc.skipCurrentElement();
        }
        else if (doc.name() == "AppState")
        {
            // Defer restoring windows until after the full workspace is loaded
            // so that Monitor and tabs are fully initialised first.
            // Collect the state now and act after the while loop.
            while (doc.readNextStartElement())
            {
                if (doc.name() == "MonitorWindow"
                    && doc.attributes().value("open") == "1")
                {
                    QByteArray geo = QByteArray::fromBase64(
                        doc.attributes().value("geometry").toLatin1());
                    Monitor::createAndShow(this, m_doc);
                    if (Monitor::instance() && !geo.isEmpty())
                        Monitor::instance()->restoreGeometry(geo);
                }
                else if (doc.name() == "DetachedWindow")
                {
                    QString cls = doc.attributes().value("class").toString();
                    int tabIdx  = doc.attributes().value("tabIndex").toInt();
                    QByteArray geo = QByteArray::fromBase64(
                        doc.attributes().value("geometry").toLatin1());
                    // Find the tab widget that matches the class name and detach it
                    for (int t = 0; t < m_tab->count(); ++t)
                    {
                        QWidget *w = m_tab->widget(t);
                        if (w && QString(w->metaObject()->className()) == cls)
                        {
                            w->setProperty("tabIndex", tabIdx);
                            w->setProperty("tabIcon",  QVariant::fromValue(m_tab->tabIcon(t)));
                            w->setProperty("tabLabel", m_tab->tabText(t));
                            DetachedContext *dw = new DetachedContext(this);
                            dw->setCentralWidget(w);
                            if (!geo.isEmpty())
                                dw->restoreGeometry(geo);
                            else
                                dw->resize(800, 600);
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

    if (goToConsole == true)
        // Force the active window to be Virtual Console
        setActiveWindow(VirtualConsole::staticMetaObject.className());
    else
        // Set the active window to what was saved in the workspace file
        setActiveWindow(activeWindowName);

    // Perform post-load operations
    VirtualConsole::instance()->postLoad();

    if (m_doc->errorLog().isEmpty() == false &&
        fromMemory == false)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Warning"),
                        tr("Some errors occurred while loading the project:") + "<br><br>" + m_doc->errorLog(),
                        QMessageBox::Ok);
        msg.setTextFormat(Qt::RichText);
        QSpacerItem* horizontalSpacer = new QSpacerItem(800, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = (QGridLayout*)msg.layout();
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msg.exec();
    }

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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    doc.setCodec("UTF-8");
#endif

    doc.writeStartDocument();
    doc.writeDTD(QString("<!DOCTYPE %1>").arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCWorkspace);
    doc.writeAttribute("xmlns", QString("%1%2").arg(KXMLQLCplusNamespace).arg(KXMLQLCWorkspace));
    /* Currently active window */
    QWidget* widget = m_tab->currentWidget();
    if (widget != NULL)
        doc.writeAttribute(KXMLQLCWorkspaceWindow, QString(widget->metaObject()->className()));

    doc.writeStartElement(KXMLQLCCreator);
    doc.writeTextElement(KXMLQLCCreatorName, APPNAME);
    doc.writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    doc.writeTextElement(KXMLQLCCreatorAuthor, QLCFile::currentUserName());
    doc.writeEndElement(); // close KXMLQLCCreator

    /* Write engine components to the XML document */
    m_doc->saveXML(&doc);

    /* Write virtual console to the XML document */
    VirtualConsole::instance()->saveXML(&doc);

    /* Write Simple Desk to the XML document */
    SimpleDesk::instance()->saveXML(&doc);

    /* Write open window state */
    doc.writeStartElement("AppState");
    if (Monitor::instance() != NULL && Monitor::instance()->isVisible())
    {
        doc.writeStartElement("MonitorWindow");
        doc.writeAttribute("open", "1");
        doc.writeAttribute("geometry",
            QString::fromLatin1(Monitor::instance()->saveGeometry().toBase64()));
        doc.writeEndElement();
    }
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
    for (int i = 0; i < count && i < m_tabOriginals.size(); ++i)
    {
        const QString &origText = m_tabOriginals.at(i).first;
        const QIcon   &origIcon = m_tabOriginals.at(i).second;
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

    // Apply the same mode to the main toolbar so "Text only" hides toolbar
    // icons and "Icons only" hides toolbar text labels.
    if (m_toolbar)
    {
        switch (m_tabLabelMode)
        {
        case TabIconOnly:
            m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
            break;
        case TabTextOnly:
            m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
            break;
        default:
            m_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            break;
        }
    }

    // Keep the 2D monitor window's toolbars in sync if it is open.
    if (Monitor::instance() != NULL)
        Monitor::instance()->applyToolbarLabelMode();
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    doc.setCodec("UTF-8");
#endif
    doc.writeStartDocument();
    doc.writeDTD(QString("<!DOCTYPE %1>").arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCWorkspace);
    doc.writeAttribute("xmlns", QString("%1%2").arg(KXMLQLCplusNamespace).arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCCreator);
    doc.writeTextElement(KXMLQLCCreatorName, APPNAME);
    doc.writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    doc.writeTextElement(KXMLQLCCreatorAuthor, QLCFile::currentUserName());
    doc.writeEndElement();

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

    // Update last autosave time and status bar
    m_lastAutosaveTime = QTime::currentTime().toString("hh:mm:ss");
    updateStatusBar();

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
        QFileInfo fi(untitledAutosave);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QString lastModified = QLocale().toString(fi.lastModified(), QLocale::LongFormat);
#else
        QString lastModified = fi.lastModified().toString(Qt::DefaultLocaleLongDate);
#endif

        int result = QMessageBox::question(this,
            tr("Autosave Recovery"),
            tr("An autosave file was found from a previous session.\n"
               "Last modified: %1\n\n"
               "Do you want to recover the unsaved work?").arg(lastModified),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (result == QMessageBox::Yes)
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
    m_statusModeLabel->setMinimumWidth(300);
    sb->addWidget(m_statusModeLabel, 0);

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

    m_statusTimecodeLabel = new QLabel(this);
    m_statusTimecodeLabel->setFont(chipFont);
    m_statusTimecodeLabel->setToolTip(tr("MIDI Time Code. Grey: no source. "
        "Amber: connected but not advancing (spoken scene / manual GO). "
        "Green: rolling.\n\nClick to bind the timecode source / Follow MTC."));
    m_statusTimecodeLabel->setCursor(Qt::PointingHandCursor);
    m_statusTimecodeLabel->installEventFilter(this);   // click → bind menu
    sb->addWidget(m_statusTimecodeLabel, 0);

    m_statusLoadLabel = new QLabel(this);
    m_statusLoadLabel->setFont(chipFont);
    m_statusLoadLabel->setAlignment(Qt::AlignCenter);
    // Fixed width sized to the widest reading, so the % / ms changing never
    // shifts the MTC chip left/right.
    m_statusLoadLabel->setFixedWidth(
        QFontMetrics(chipFont).horizontalAdvance(QStringLiteral("Load: 00.00 / 00 ms (000%)")) + 14);
    m_statusLoadLabel->setToolTip(tr("Engine tick compute time vs the per-tick "
        "budget. Amber above 60%, red at/over budget (dropped frames likely)."));
    sb->addWidget(m_statusLoadLabel, 0);

    // "Under timeline control" chip — visible only in Operate while a show drives
    // the rig. Green = timeline driving; amber = suspended (VC takeover). Sits
    // with the centred health chips (the timeline is a global runtime state).
    m_statusTimelineLabel = new QLabel(this);
    m_statusTimelineLabel->setToolTip(tr("Timeline control. Green: the show timeline "
        "is driving the rig. Amber: suspended — the Virtual Console has taken over "
        "(the playhead keeps tracking; resume to hand it back)."));
    m_statusTimelineLabel->hide();
    sb->addWidget(m_statusTimelineLabel, 0);

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

    // Show-mode lock indicator. Hidden when unlocked; red 🔒 when on.
    m_statusShowLockLabel = new QLabel(this);
    m_statusShowLockLabel->setAlignment(Qt::AlignRight);
    m_statusShowLockLabel->hide();
    sb->addPermanentWidget(m_statusShowLockLabel);

    // Blind indicator (global — Blind is an app-wide output mute, so it lives in
    // the status bar and stays visible across tabs). While armed the WHOLE footer
    // turns blue; this label is the white "BLIND" caption on it. Starts empty +
    // hidden (QStatusBar re-shows permanent widgets when it first appears, so a
    // label with permanent text would show even when inactive — keep it empty
    // until slotOutputInhibitedChanged fills it in). Driven by
    // InputOutputMap::outputInhibitedChanged.
    m_statusBlindLabel = new QLabel(this);
    m_statusBlindLabel->setAlignment(Qt::AlignRight);
    m_statusBlindLabel->setStyleSheet("QLabel { color: white; font-weight: bold; }");
    m_statusBlindLabel->hide();
    sb->addPermanentWidget(m_statusBlindLabel);

    // Programmer dirty indicator (between mode and autosave). Hidden
    // when clean, red bullet + text when dirty. Mirrors the in-frame
    // SaveProgrammer button highlight.
    m_statusProgrammerLabel = new QLabel(this);
    m_statusProgrammerLabel->setAlignment(Qt::AlignRight);
    m_statusProgrammerLabel->hide();
    sb->addPermanentWidget(m_statusProgrammerLabel);

    // Unsaved-changes indicator. Placed immediately before the autosave label so
    // "unsaved changes" sits adjacent to "last autosave" at the far right.
    // Permanent widget (same framed/aligned group as autosave — a normal
    // addWidget item renders at a slightly different baseline).
    m_statusDirtyLabel = new QLabel(this);
    m_statusDirtyLabel->setAlignment(Qt::AlignRight);
    sb->addPermanentWidget(m_statusDirtyLabel);
    slotDocModified(m_doc != NULL ? m_doc->isModified() : false);

    // Autosave label (far right, adjacent to the unsaved-changes indicator).
    m_statusAutosaveLabel = new QLabel(this);
    m_statusAutosaveLabel->setAlignment(Qt::AlignRight);
    sb->addPermanentWidget(m_statusAutosaveLabel);

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

    updateStatusBar();
}

bool App::eventFilter(QObject *watched, QEvent *event)
{
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
            if (!tc->isRunning())
                verdict = tr("held (no fresh code)");
            else if (qAbs(rate - 1.0) <= 0.05 && jit < 40.0)
                verdict = tr("✓ healthy");
            else if (qAbs(rate - 1.0) <= 0.15 && jit < 120.0)
                verdict = tr("~ usable");
            else
                verdict = tr("⚠ unstable");

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
        connect(anyA, &QAction::triggered, this, [tc]() { if (tc) tc->setSourceUniverse(-1); });

        int patchedInputs = 0;
        for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
        {
            const quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
            QString nm = m_doc->inputOutputMap()->getUniverseNameByIndex(i);
            if (nm.isEmpty())
                nm = tr("Universe %1").arg(i + 1);
            // Annotate with the MIDI input actually patched to this universe —
            // MTC can only arrive where an input plugin is patched.
            InputPatch *ip = m_doc->inputOutputMap()->inputPatch(uniID);
            if (ip != NULL && !ip->pluginName().isEmpty())
            {
                nm += QString("  —  %1: %2").arg(ip->pluginName(), ip->inputName());
                patchedInputs++;
            }
            else
            {
                nm += tr("  —  no MIDI input");
            }
            QAction *a = menu.addAction(nm);
            a->setCheckable(true);
            a->setActionGroup(grp);
            a->setChecked(qint32(uniID) == cur);
            connect(a, &QAction::triggered, this, [tc, uniID]() {
                if (tc) tc->setSourceUniverse(qint32(uniID));
            });
        }

        menu.addSeparator();
        if (patchedInputs == 0)
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
        menu.addAction(tr("Patch a MIDI input (Inputs/Outputs)…"), this, [this]() {
            if (InputOutputManager::instance() != NULL && m_tab != NULL)
                m_tab->setCurrentWidget(InputOutputManager::instance());
        });

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        menu.exec(m_statusTimecodeLabel->mapToGlobal(me->pos()));
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

        // Optionally scroll the Inputs/Outputs Overview grid to a given row and
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
        m_statusTimecodeLabel->setText(tr("MTC: no source"));
        applyStyle(grey);
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
    if (m_statusLoadLabel != NULL && m_doc != NULL && m_doc->masterTimer() != NULL)
    {
        // Peak tick over the poll window — far more informative than the last
        // tick, which is tiny/noisy on a light show.
        double ms = m_doc->masterTimer()->tickComputePeakMs();
        double budget = double(MasterTimer::tick());
        int pct = budget > 0 ? int((ms / budget) * 100.0) : 0;
        m_statusLoadLabel->setText(tr("Load: %1 / %2 ms (%3%)")
                                   .arg(ms, 0, 'f', 2).arg(int(budget)).arg(pct));
        const char *green = "QLabel { color:#ffffff; background:#2e7d32; padding:1px 6px; border-radius:3px; }";
        const char *amber = "QLabel { color:#000000; background:#f5a623; padding:1px 6px; border-radius:3px; }";
        const char *red   = "QLabel { color:#ffffff; background:#c62828; padding:1px 6px; border-radius:3px; }";
        m_statusLoadLabel->setStyleSheet(pct >= 100 ? red : (pct >= 60 ? amber : green));
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
    m_statusProgrammerLabel->setText(
        tr("<span style='color:#e60000;font-weight:bold;'>"
           "● Programmer: %1</span>").arg(detail));
    m_statusProgrammerLabel->show();
}

void App::updateStatusBar()
{
    // Update mode message
    if (m_statusModeLabel != NULL)
    {
        if (m_statusMessage.isEmpty())
            m_statusModeLabel->setText(tr("Ready"));
        else
            m_statusModeLabel->setText(m_statusMessage);
    }

    // Update autosave status
    if (m_statusAutosaveLabel != NULL)
    {
        if (m_autosaveEnabled)
        {
            if (m_lastAutosaveTime.isEmpty())
                m_statusAutosaveLabel->setText(tr("Autosave: Enabled"));
            else
                m_statusAutosaveLabel->setText(tr("Last autosave: %1").arg(m_lastAutosaveTime));
        }
        else
        {
            m_statusAutosaveLabel->setText(tr("Autosave: Disabled"));
        }
    }
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
