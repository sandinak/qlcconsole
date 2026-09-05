/*
  Q Light Controller - Fixture Definition Editor
  app.cpp

  Copyright (C) Heikki Junnila

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

#include <QDesktopServices>
#include <QMdiSubWindow>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QWidgetList>
#include <QMessageBox>
#include <QDockWidget>
#include <QSettings>
#include <QMdiArea>
#include <QMenuBar>
#include <QToolBar>
#include <QToolTip>
#include <QAction>
#include <QLabel>
#include <QColor>
#include <QDebug>
#include <QMenu>
#include <QIcon>
#include <QUrl>

#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "avolitesd4parser.h"

#include "app.h"
#include "fixtureeditor.h"
#include "fixturebrowser.h"

#define SETTINGS_GEOMETRY "workspace/geometry"
#define SETTINGS_OPENDIALOGSTATE "workspace/opendialog"
#define SETTINGS_WORKINGPATH "workspace/workingpath"

App *_app;

App::App(QWidget *parent) : QMainWindow(parent)
{
    _app = this;

    m_fileMenu = NULL;
    m_helpMenu = NULL;
    m_toolBar = NULL;

    m_copyChannel = NULL;

    setWindowTitle(App::longName());
    setWindowIcon(QIcon(":/qlcconsole-fixtureeditor.png"));
    QMdiArea *mdiArea = new QMdiArea(this);
    mdiArea->setBackground(QBrush(QColor(0x60, 0x60, 0x60)));
    setCentralWidget(mdiArea);

    // Side panel: every fixture definition already on disk, searchable by
    // manufacturer/model, double-click to open -- previously the only way
    // in was File > Open's raw file dialog, with no browsing by name at
    // all. A dock (not a splitter replacing centralWidget()) so every
    // existing qobject_cast<QMdiArea*>(centralWidget()) call site
    // (loadFixtureDefinition() and friends) keeps working untouched.
    QDockWidget *browserDock = new QDockWidget(tr("Fixture Library"), this);
    browserDock->setObjectName("FixtureBrowserDock");
    // Fixed in place on purpose: with QDockWidget's default features this
    // panel has its own close button, and there's no menu bar (see
    // initMenuBar() below) offering any way to bring it back once
    // dismissed -- accidentally closing it would permanently strand you
    // without the browser for the rest of the session.
    browserDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    FixtureBrowser *browser = new FixtureBrowser(browserDock);
    browserDock->setWidget(browser);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);
    connect(browser, &FixtureBrowser::definitionActivated,
            this, &App::loadFixtureDefinition);

    QCoreApplication::setOrganizationName("qlcplus");
    QCoreApplication::setOrganizationDomain("qlcplus.org");
    QCoreApplication::setApplicationName(FXEDNAME);

    initActions();
    //initMenuBar();
    initToolBar();

    QSettings settings;
    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid() == true)
        restoreGeometry(var.toByteArray());

    QVariant dir = settings.value(SETTINGS_WORKINGPATH);
    if (dir.isValid() == false || QDir(dir.toString()).exists() == false)
        m_workingDirectory = QLCFixtureDefCache::userDefinitionDirectory();
    else
        m_workingDirectory = QDir(dir.toString());


    this->raise();
}

App::~App()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());

    setCopyChannel(NULL);

    // Remove the reference to the application
    _app = NULL;
}

QString App::longName()
{
    return QString("%1 - %2").arg(APPNAME, FXEDNAME);
}

QString App::version()
{
    return QString("Version %1").arg(APPVERSION);
}

void App::loadFixtureDefinition(const QString& path)
{
    QLCFixtureDef *fixtureDef = NULL;

    /* Attempt to create a fixture definition from the selected file */
    QString error(tr("Unrecognized file extension: %1").arg(path));
    if (path.endsWith(KExtFixture, Qt::CaseInsensitive) == true)
        fixtureDef = loadQXF(path, error);
    else if (path.endsWith(KExtAvolitesFixture, Qt::CaseInsensitive) == true)
        fixtureDef = loadD4(path, error);
    else
        fixtureDef = NULL;

    if (fixtureDef != NULL)
    {
        /* Create a new sub window and put a fixture editor widget
           in that sub window with the newly-created fixture def */
        QMdiSubWindow *sub = new QMdiSubWindow(centralWidget());
        QLCFixtureEditor *editor = new QLCFixtureEditor(sub, fixtureDef, path);

        sub->setWidget(editor);
        sub->setAttribute(Qt::WA_DeleteOnClose);
        qobject_cast<QMdiArea*> (centralWidget())->addSubWindow(sub);

        // check if sub-window is outside main area
        if (sub->x() >= this->width() || sub->y() >= this->height())
            sub->setGeometry(0, 0, sub->width(), sub->height());

        editor->show();
        sub->show();
    }
    else
    {
        QMessageBox::warning(this, tr("Fixture loading failed"),
                             tr("Unable to load fixture definition: ") + error);
    }
}

QLCFixtureDef *App::loadQXF(const QString& path, QString& errorMsg) const
{
    QLCFixtureDef *fixtureDef = new QLCFixtureDef;
    Q_ASSERT(fixtureDef != NULL);

    QFile::FileError error = fixtureDef->loadXML(path);
    if (error != QFile::NoError)
    {
        delete fixtureDef;
        fixtureDef = NULL;
        errorMsg = QLCFile::errorString(error);
    }

    return fixtureDef;
}

QLCFixtureDef *App::loadD4(const QString& path, QString& errorMsg) const
{
    QLCFixtureDef *fixtureDef = new QLCFixtureDef;

    AvolitesD4Parser parser;
    if (parser.loadXML(path, fixtureDef) == false)
    {
        errorMsg = parser.lastError();
        delete fixtureDef;
        fixtureDef = NULL;
    }

    return fixtureDef;
}

void App::closeEvent(QCloseEvent *e)
{
    /* Accept the close event by default */
    e->accept();

    QListIterator <QMdiSubWindow*> it(
        qobject_cast<QMdiArea*> (centralWidget())->subWindowList());
    while (it.hasNext() == true)
    {
        QLCFixtureEditor *editor;
        QMdiSubWindow *sub;

        sub = it.next();
        Q_ASSERT(sub != NULL);

        editor = static_cast<QLCFixtureEditor*> (sub->widget());
        Q_ASSERT(editor != NULL);

        editor->show();
        editor->setFocus();

        if (editor->close() == false)
        {
            /* Ignore the close event if just one editor refuses */
            e->ignore();
            break;
        }
    }
}

/*****************************************************************************
 * Copy channel
 *****************************************************************************/

void App::setCopyChannel(QLCChannel *ch)
{
    if (m_copyChannel != NULL)
        delete m_copyChannel;
    m_copyChannel = NULL;

    if (ch != NULL)
        m_copyChannel = ch->createCopy();

    emit clipboardChanged();
}

QLCChannel *App::copyChannel() const
{
    return m_copyChannel;
}

/*****************************************************************************
 * Actions, toolbar & menubar
 *****************************************************************************/

void App::initActions()
{
    /* File actions */
    m_fileNewAction = new QAction(QIcon(":/filenew.png"),
                                  tr("&New"), this);
    m_fileNewAction->setShortcut(QKeySequence("CTRL+N"));
    connect(m_fileNewAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFileNew()));

    m_fileOpenAction = new QAction(QIcon(":/fileopen.png"),
                                   tr("&Open"), this);
    m_fileOpenAction->setShortcut(QKeySequence("CTRL+O"));
    connect(m_fileOpenAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFileOpen()));

    m_fileSaveAction = new QAction(QIcon(":/filesave.png"),
                                   tr("&Save"), this);
    m_fileSaveAction->setShortcut(QKeySequence("CTRL+S"));
    connect(m_fileSaveAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFileSave()));

    m_fileSaveAsAction = new QAction(QIcon(":/filesaveas.png"),
                                     tr("Save &As..."), this);
    m_fileSaveAsAction->setShortcut(QKeySequence("CTRL+SHIFT+S"));
    connect(m_fileSaveAsAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFileSaveAs()));

    m_fileQuitAction = new QAction(QIcon(":/exit.png"),
                                   tr("&Quit"), this);
    m_fileQuitAction->setShortcut(QKeySequence("CTRL+Q"));
    connect(m_fileQuitAction, SIGNAL(triggered(bool)),
            this, SLOT(slotFileQuit()));

    /* Help actions */
    m_helpIndexAction = new QAction(QIcon(":/help.png"),
                                    tr("Index"), this);
    m_helpIndexAction->setShortcut(QKeySequence("SHIFT+F1"));
    connect(m_helpIndexAction, SIGNAL(triggered(bool)),
            this, SLOT(slotHelpIndex()));

    m_helpAboutQtAction = new QAction(QIcon(":/qt.png"),
                                      tr("About Qt..."), this);
    connect(m_helpAboutQtAction, SIGNAL(triggered(bool)),
            this, SLOT(slotHelpAboutQt()));
}

void App::initToolBar()
{
    m_toolBar = new QToolBar(App::longName(), this);
    m_toolBar->setIconSize(QSize(24, 24));
    addToolBar(m_toolBar);
    m_toolBar->setMovable(false);

    m_toolBar->addAction(m_fileNewAction);
    m_toolBar->addAction(m_fileOpenAction);
    m_toolBar->addAction(m_fileSaveAction);
    m_toolBar->addAction(m_fileSaveAsAction);

    m_toolBar->addSeparator();

    m_toolBar->addAction(m_helpIndexAction);
    // "About Fixture Definition Editor..." deliberately left off -- this is
    // one app in the qlcconsole suite, not a standalone product; the main
    // window's own About already covers it, and duplicating that here was
    // just clutter.

    // Follows the SAME "Toolbar Style" preference as qlcconsole's own main
    // toolbar (App::applyTabLabelMode(), ui/src/app.cpp) rather than a
    // separate setting local to this app -- constructed with that app's
    // explicit org/name rather than this one's (QCoreApplication's, set
    // just below: organizationName matches, but applicationName doesn't,
    // so a plain QSettings here would read/write an unrelated file).
    QSettings mainAppSettings(QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"));
    switch (mainAppSettings.value(QStringLiteral("workspace/tabLabelMode"), 0).toInt())
    {
    case 1: // TabIconOnly
        m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        break;
    case 2: // TabTextOnly
        m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
        break;
    default: // TabIconAndText
        m_toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        break;
    }
}

void App::initMenuBar()
{
    /* File Menu */
    m_fileMenu = new QMenu(menuBar());
    m_fileMenu->setTitle(tr("&File"));
    m_fileMenu->addAction(m_fileNewAction);
    m_fileMenu->addAction(m_fileOpenAction);
    m_fileMenu->addAction(m_fileSaveAction);
    m_fileMenu->addAction(m_fileSaveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_fileQuitAction);

    /* Help menu */
    m_helpMenu = new QMenu(menuBar());
    m_helpMenu->setTitle(tr("&Help"));
    m_helpMenu->addAction(m_helpIndexAction);
    m_helpMenu->addSeparator();
    m_helpMenu->addAction(m_helpAboutQtAction);

    menuBar()->addMenu(m_fileMenu);
    menuBar()->addMenu(m_helpMenu);
}

/*****************************************************************************
 * File action slots
 *****************************************************************************/

void App::slotFileNew()
{
    QLCFixtureEditor *editor;
    QMdiSubWindow *sub;

    sub = new QMdiSubWindow(centralWidget());
    editor = new QLCFixtureEditor(sub, new QLCFixtureDef());

    sub->setWidget(editor);
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWindowIcon(QIcon(":/fixture.png"));

    qobject_cast<QMdiArea*> (centralWidget())->addSubWindow(sub);

    editor->show();
    sub->show();
}

void App::slotFileOpen()
{
    QSettings settings;

    /* Create a file open dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Open a fixture definition"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    QStringList filters;
    filters << KAllFilter;
    filters << KQXFFilter;
    filters << KD4Filter;
    dialog.setNameFilters(filters);

    QVariant var = settings.value(SETTINGS_OPENDIALOGSTATE);
    if (var.isValid() == true)
        dialog.restoreState(var.toByteArray());

    dialog.setDirectory(m_workingDirectory);

    /* Execute the dialog */
    if (dialog.exec() != QDialog::Accepted)
        return;

    /* Get a file name */
    QStringListIterator it(dialog.selectedFiles());
    while (it.hasNext() == true)
        loadFixtureDefinition(it.next());

    settings.setValue(SETTINGS_OPENDIALOGSTATE, dialog.saveState());
    m_workingDirectory = dialog.directory();
    settings.setValue(SETTINGS_WORKINGPATH, m_workingDirectory.absolutePath());
}

void App::slotFileSave()
{
    QLCFixtureEditor *editor;
    QMdiSubWindow *sub;

    sub = (qobject_cast<QMdiArea*> (centralWidget()))->activeSubWindow();
    if (sub == NULL)
        return;

    editor = static_cast<QLCFixtureEditor*> (sub->widget());
    if (editor == NULL)
        return;

    editor->save();
}

void App::slotFileSaveAs()
{
    QLCFixtureEditor *editor;
    QMdiSubWindow *sub;

    sub = (qobject_cast<QMdiArea*> (centralWidget()))->activeSubWindow();
    if (sub == NULL)
        return;

    editor = static_cast<QLCFixtureEditor*> (sub->widget());
    if (editor == NULL)
        return;

    editor->saveAs();
}

void App::slotFileQuit()
{
    close();
}

/*****************************************************************************
 * Help action slots
 *****************************************************************************/

void App::slotHelpIndex()
{
    QDesktopServices::openUrl(QUrl("https://docs.qlcplus.org/v4/fixture-definition-editor"));
}

void App::slotHelpAboutQt()
{
    QMessageBox::aboutQt(this, App::longName());
}
