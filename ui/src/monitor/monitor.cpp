/*
  Q Light Controller Plus
  monitor.cpp

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

#include <QDateTime>
#include <QApplication>
#include <QActionGroup>
#include <QTabWidget>
#include <QMainWindow>
#include <QColorDialog>
#include <QSet>
#include <QStyleOptionComboBox>
#include <QStyle>
#include <algorithm>
#include <QFileDialog>
#include <QFileInfo>
#include <QCursor>
#include <QDialog>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>
#include <cmath>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QContextMenuEvent>
#include <functional>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QWidgetAction>
#include <QInputDialog>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "studiotemplate.h"
#include "mainbar.h"
#include <QLineEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QShortcut>
#include <QSharedPointer>
#include <QMimeData>
#include <QDataStream>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QtMath>
#include <QSpacerItem>
#include <QByteArray>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QSplitter>
#include <QSettings>
#include <QToolBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QAction>
#include <QScreen>
#include <QLabel>
#include <QDebug>
#include <QFont>
#include <QIcon>

#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include "monitorfixtureitem.h"
#include "focusmodepanel.h"
#include "aimsolver.h"
#include "powerdistribution.h"
#include "networktopology.h"
#include "networkgearcatalog.h"
#include "networktopologydialog.h"
#include "networkreportdialog.h"
#include "patchlistdialog.h"
#include "fixturecreator.h"
#include "monitorinventorypanel.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "righeads.h"
#include "lightcontrols.h"
#include "fixturefields.h"
#include "scenevalue.h"
#include "scene.h"
#include "mastertimer.h"
#include "dmxsource.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "fixture.h"
#include "monitorbackgroundselection.h"
#include "monitorgraphicsview.h"
#include "monitorlayerspanel.h"
#include "monitorruler.h"
#include "riserfaceeditor.h"
#include "trussitem.h"
#include "platformitem.h"
#include "pipeitem.h"
#include "standitem.h"
#include "toweritem.h"
#include "monitorimageitem.h"
#include "feetinchesspinbox.h"
#include "rigeditsession.h"
#include "colourswatchbutton.h"
#include "targetitem.h"
#include "truss.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "structurestudioview.h"
#include "rhiraster.h"
#include "powerdistributiondialog.h"
#include "fixturegroupeditor.h"
#include "studiogroupeditor.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "fixtureselection.h"
#include "monitorfixture.h"
#include "monitorlayout.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "qlccapability.h"
#include "monitor.h"
#include "ledlayouteditor.h"
#include "beamocclusion.h"
#include "fountaindiagram.h"
#include "fixturemanager.h"
#include "patchinventorydialog.h"
#include "createfixturegroup.h"
#include "app.h"
#include "apputil.h"
#include "doc.h"
#include "programmercontroller.h"
#include "focussession.h"

#include <QTimer>
#include "qlcfile.h"
#include "qlceventpos.h"

#define SETTINGS_VSPLITTER "monitor/vsplitter2"
#define SETTINGS_LAYERS_PANEL "monitor/layerspanel3"
#define SETTINGS_RULERS "monitor/rulers"
#define SETTINGS_GRID   "monitor/grid"
#define SETTINGS_CENTERLINES "monitor/centerlines"
#define SETTINGS_LINEOFSIGHT "monitor/lineOfSightBeams"
#define SETTINGS_MOUNTEDFIXTURES "monitor/mountedfixtures"
#define SETTINGS_STUDIOROTATION "monitor/studiorotation"
#define SETTINGS_PLOTROTATION "monitor/plotrotation"
#define SETTINGS_STUDIOANGLE "monitor/studioangle"

Monitor* Monitor::s_instance = NULL;

/*****************************************************************************
 * Initialization
 *****************************************************************************/

Monitor::Monitor(QWidget* parent, Doc* doc, Qt::WindowFlags f)
    : QWidget(parent, f)
    , m_doc(doc)
    , m_props(NULL)
    , m_DMXToolBar(NULL)
    , m_scrollArea(NULL)
    , m_monitorWidget(NULL)
    , m_monitorLayout(NULL)
    , m_dmxViewPopulated(false)
    , m_currentUniverse(Universe::invalid())
    , m_graphicsToolBar(NULL)
    , m_splitter(NULL)
    , m_graphicsView(NULL)
    , m_layersPanel(NULL)
    , m_inventoryPanel(NULL)
    , m_gridWSpin(NULL)
    , m_gridHSpin(NULL)
    , m_unitsLabel(NULL)
    , m_povCombo(NULL)
    , m_labelsAction(NULL)
    , m_layersAction(NULL)
    , m_inventoryAction(NULL)
    , m_groupAction(NULL)
    , m_ungroupAction(NULL)
    , m_pasteAction(NULL)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(s_instance == NULL);
    s_instance = this;

    m_props = m_doc->monitorProperties();

    /* Master layout for toolbar and scroll area */
    new QVBoxLayout(this);

    initView();

    /* Listen to fixture additions and changes from Doc */
    connect(m_doc, SIGNAL(fixtureAdded(quint32)),
            this, SLOT(slotFixtureAdded(quint32)));
    connect(m_doc, SIGNAL(fixtureChanged(quint32)),
            this, SLOT(slotFixtureChanged(quint32)));
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)),
            this, SLOT(slotFixtureRemoved(quint32)));
    connect(m_doc->masterTimer(), SIGNAL(functionStarted(quint32)),
            this, SLOT(slotFunctionStarted(quint32)));
    // Repaint fixture ghost outlines when the mark set changes (add/remove or an
    // auto-release on reveal). The fixture item's paint() reads isFixtureMarked().
    if (m_doc->programmer() != NULL)
        connect(m_doc->programmer(), &ProgrammerController::markChanged, this, [this]() {
            if (m_graphicsView != NULL && m_graphicsView->scene() != NULL)
                m_graphicsView->scene()->update();
        });
    // Re-render targets on Design/Operate switch so their drag-ability tracks the
    // mode (editable while designing, frozen in a show).
    connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode) {
        if (m_graphicsView != NULL)
            m_graphicsView->updateTargets();
    });
}

void Monitor::slotFunctionStarted(quint32 id)
{
    if (m_props->displayMode() == MonitorProperties::Graphics)
    {
        QString bgImage = m_props->customBackground(id);
        if (m_graphicsView != NULL && bgImage.isEmpty() == false)
            m_graphicsView->setBackgroundImage(bgImage);
    }
}

Monitor::~Monitor()
{
    cancelAllLocateSessions();

    while (m_monitorFixtures.isEmpty() == false)
        delete m_monitorFixtures.takeFirst();

    saveSettings();

    /* Reset the singleton instance */
    Monitor::s_instance = NULL;
}

void Monitor::initView()
{
    qDebug() << Q_FUNC_INFO;

    /* Both views' toolbars live in one holder, the layout's menu bar, and a
       view switch only shows one and hides the other. The holder is what
       the main window's top bar borrows while this is the current tab
       (MainBar), so it must stay put: setMenuBar() on each switch would
       pull whichever bar it named back out of the top bar. */
    m_toolsHolder = new QWidget(this);
    m_toolsHolder->setObjectName(QStringLiteral("monitorToolbars"));
    QVBoxLayout *holder = new QVBoxLayout(m_toolsHolder);
    holder->setContentsMargins(0, 0, 0, 0);
    holder->setSpacing(0);
    layout()->setMenuBar(m_toolsHolder);
    MainBar::registerTabTools(this, m_toolsHolder);

    initDMXToolbar();
    initDMXView();
    initGraphicsToolbar();
    initGraphicsView();

    showCurrentView();
}

void Monitor::initDMXView()
{
    /* Scroll area that contains the monitor widget */
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    layout()->addWidget(m_scrollArea);

    /* Monitor widget that contains all MonitorFixtures */
    m_monitorWidget = new QWidget(m_scrollArea);
    m_monitorWidget->setBackgroundRole(QPalette::Dark);
    m_monitorLayout = new MonitorLayout(m_monitorWidget);
    m_monitorLayout->setSpacing(1);
    m_monitorLayout->setContentsMargins(1, 1, 1, 1);

    m_scrollArea->setWidget(m_monitorWidget);

    fillDMXView();
}

void Monitor::fillDMXView()
{
    while (m_monitorFixtures.isEmpty() == false)
        delete m_monitorFixtures.takeFirst();

    m_monitorWidget->setFont(m_props->font());
    m_dmxViewPopulated = false;

    /* Building the fixture widgets is deferred until the DMX view is actually
       shown. Each MonitorFixture creates three stylesheet'd QLabels per DMX
       channel, so a large workspace means tens of thousands of widgets -- and
       every one of them also subscribes to its Fixture's valuesChanged(), so
       an unseen DMX view keeps costing during live output too. If the view is
       on screen right now, rebuild it immediately. */
    if (m_scrollArea != NULL && m_scrollArea->isVisible() == true)
        populateDMXView();
}

void Monitor::populateDMXView()
{
    if (m_dmxViewPopulated == true)
        return;

    m_dmxViewPopulated = true;

    /* Create a bunch of MonitorFixtures for each fixture */
    foreach (Fixture* fxi, m_doc->fixtures())
    {
        Q_ASSERT(fxi != NULL);
        // A staged fixture has no place in a universe grid.
        if (!fxi->isPatched())
            continue;
        if (m_currentUniverse == Universe::invalid() ||
            m_currentUniverse == fxi->universe())
                createMonitorFixture(fxi);
    }

    m_monitorLayout->sort();
    m_monitorWidget->updateGeometry();
}

void Monitor::showDMXView()
{
    qDebug() << Q_FUNC_INFO;

    /* Deferred from fillDMXView(): build the widgets now that they'll be seen */
    populateDMXView();

    hideFixtureItemEditor();

    m_graphicsView->hide();
    m_graphicsToolBar->hide();

    m_DMXToolBar->show();
    m_scrollArea->show();

    if (m_dmxViewCombo != NULL)
    {
        m_dmxViewCombo->blockSignals(true);
        m_dmxViewCombo->setCurrentIndex(0);   // "DMX"
        m_dmxViewCombo->blockSignals(false);
    }

    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
        if (m_currentUniverse == Universe::invalid() || uniID == m_currentUniverse)
            m_doc->inputOutputMap()->setUniverseMonitor(i, true);
        else
            m_doc->inputOutputMap()->setUniverseMonitor(i, false);
    }

    /* No local Quit action here any more. There used to be one, so Cmd+Q would
       reach App::close() from this window -- but the main window's Quit has the
       same shortcut, and two actions claiming one sequence in the same scope is
       an ambiguous overload: Qt fires NEITHER and just warns. That is why Cmd+Q
       did nothing here, and Cmd+S nothing at all, since it had no local action
       to be ambiguous with in the first place.

       Both are application-level operations on the one shared document, so
       they are Qt::ApplicationShortcut on the App's own actions now (see
       App::initActions) and fire from whichever window has focus, with the
       save-before-quit gutcheck intact. */
}

void Monitor::initGraphicsView()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout()->addWidget(m_splitter);
    QWidget* gcontainer = new QWidget(this);
    m_splitter->addWidget(gcontainer);
    QVBoxLayout *gcLayout = new QVBoxLayout(gcontainer);
    gcLayout->setContentsMargins(0, 0, 0, 0);
    gcLayout->setSpacing(0);

    // The canvas sits in a 2x2 grid: rulers along the top and left edges frame
    // the graphics view; the top-left corner is a small filler. The footer bar
    // is added below by initGraphicsFooter().
    QWidget *viewArea = new QWidget(gcontainer);
    QGridLayout *viewGrid = new QGridLayout(viewArea);
    viewGrid->setContentsMargins(0, 0, 0, 0);
    viewGrid->setSpacing(0);

    m_graphicsView = new MonitorGraphicsView(m_doc, this);
    m_graphicsView->setRigRestorer([this](const QByteArray &snap) { restoreRig(snap); });
    m_graphicsView->setRigSnapshotter([this]() { return rigSnapshot(); });
    if (m_undoAction != nullptr)
    {
        connect(m_undoAction, &QAction::triggered, m_graphicsView, &MonitorGraphicsView::undoLastMove);
        connect(m_graphicsView, &MonitorGraphicsView::undoChanged, m_undoAction,
                [this]() { m_undoAction->setEnabled(m_graphicsView->canUndo()); });
    }
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setAcceptDrops(true);
    // No frame border so the rulers (aligned to the widget edge) line up
    // exactly with the viewport pixels the tick math produces.
    m_graphicsView->setFrameShape(QFrame::NoFrame);
    m_graphicsView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_graphicsView->setBackgroundBrush(QBrush(QColor(11, 11, 11, 255), Qt::SolidPattern));
    viewGrid->addWidget(m_graphicsView, 1, 1);
    viewGrid->setRowStretch(1, 1);
    viewGrid->setColumnStretch(1, 1);
    gcLayout->addWidget(viewArea);

    // Layers side panel, docked on the LEFT (first splitter pane) as most apps
    // put their layers/objects panel. Hideable via the toolbar toggle.
    m_layersPanel = new MonitorLayersPanel(m_doc, m_graphicsView, this);
    m_splitter->insertWidget(0, m_layersPanel);
    m_splitter->setStretchFactor(0, 0);   // panel: fixed-ish width
    m_splitter->setStretchFactor(1, 1);   // canvas: takes the slack
    // Never let a pane collapse to zero width — otherwise a splitter state saved
    // when the panel was hidden restores it at 0px and the tree stays invisible.
    m_splitter->setChildrenCollapsible(false);
    // The panel's in-panel × hides it by un-checking the toolbar toggle.
    connect(m_layersPanel, &MonitorLayersPanel::closeRequested, this, [this]() {
        if (m_layersAction != NULL)
            m_layersAction->setChecked(false);
    });
    // Inventory panel, docked on the RIGHT: fixtures staged but not yet placed,
    // to drag onto the plot. Hidden until asked for (toolbar "Inventory").
    m_inventoryPanel = new MonitorInventoryPanel(m_doc, m_graphicsView, this);
    m_splitter->addWidget(m_inventoryPanel);
    m_splitter->setStretchFactor(m_splitter->indexOf(m_inventoryPanel), 0);
    m_inventoryPanel->setMinimumWidth(180);
    {
        const bool show = QSettings().value(QStringLiteral("monitor/showInventory"), false).toBool();
        m_inventoryPanel->setVisible(show);
        if (m_inventoryAction != NULL)
        {
            m_inventoryAction->blockSignals(true);
            m_inventoryAction->setChecked(show);
            m_inventoryAction->blockSignals(false);
        }
    }
    connect(m_inventoryPanel, &MonitorInventoryPanel::closeRequested, this, [this]() {
        if (m_inventoryAction != NULL)
            m_inventoryAction->setChecked(false);
    });
    connect(m_inventoryPanel, &MonitorInventoryPanel::addRequested, this, &Monitor::slotAddToInventory);
    connect(m_graphicsView, &MonitorGraphicsView::fixturesDropped, this, &Monitor::slotFixturesDropped);
    // Keep the panel true to the show: fixtures come and go, get patched, or
    // land on the plot from elsewhere.
    connect(m_doc, &Doc::fixtureAdded, m_inventoryPanel, [this](quint32) { m_inventoryPanel->reload(); });
    connect(m_doc, &Doc::fixtureRemoved, m_inventoryPanel, [this](quint32) { m_inventoryPanel->reload(); });
    connect(m_doc, &Doc::fixtureChanged, m_inventoryPanel, [this](quint32) { m_inventoryPanel->reload(); });
    // A repatch changes which port a fixture is on, so the cable runs follow.
    connect(m_doc, &Doc::fixtureChanged, m_graphicsView, [this](quint32) { m_graphicsView->scheduleNetworkRuns(); });

    // Double-clicking a Fixture Group node in the panel opens its head-layout editor.
    /* "Show in Layers" from the plot's menu: open the list if it is hidden,
       then bring the row into view. */
    connect(m_graphicsView, &MonitorGraphicsView::selectInLayersRequested, this,
            [this](const QList<QPair<QString, quint32> > &objects) {
        if (m_layersPanel != nullptr)
            m_layersPanel->followMapSelection(objects);
    });
    connect(m_graphicsView, &MonitorGraphicsView::revealInLayersRequested, this,
            [this](const QString &kind, quint32 id) {
        if (m_layersPanel == nullptr)
            return;
        if (!m_layersPanel->isVisible())
            m_layersPanel->setVisible(true);
        m_layersPanel->reload();
        m_layersPanel->revealItem(kind, id);
    });
    connect(m_layersPanel, &MonitorLayersPanel::editFixtureGroupRequested,
            this, [this](quint32 fgId) { openGroupLayout(fgId); if (m_layersPanel) m_layersPanel->reload(); });

    connect(m_graphicsView, SIGNAL(fixtureMoved(quint32,QPointF)),
            this, SLOT(slotFixtureMoved(quint32,QPointF)));
    connect(m_graphicsView, SIGNAL(viewClicked(QMouseEvent*)),
            this, SLOT(slotViewClicked()));
    connect(m_graphicsView, &MonitorGraphicsView::fixtureDoubleClicked,
            this, &Monitor::slotFixtureDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::trussDoubleClicked,
            this, &Monitor::slotTrussDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::trussRemoveRequested,
            this, &Monitor::slotTrussRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::addBarToTrussRequested,
            this, &Monitor::slotAddBarToTruss);
    connect(m_graphicsView, &MonitorGraphicsView::addBarToPipeRequested,
            this, &Monitor::slotAddBarToPipe);
    connect(m_graphicsView, &MonitorGraphicsView::platformRemoveRequested,
            this, &Monitor::slotPlatformRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::pipeRemoveRequested,
            this, &Monitor::slotPipeRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::hangerRemoveRequested,
            this, &Monitor::slotHangerRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::hangerDoubleClicked,
            this, &Monitor::slotEditHanger);
    connect(m_graphicsView, &MonitorGraphicsView::standRemoveRequested,
            this, &Monitor::slotStandRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::sceneryRemoveRequested,
            this, &Monitor::slotSceneryRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::sceneryDoubleClicked,
            this, &Monitor::slotEditScenery);
    connect(m_graphicsView, &MonitorGraphicsView::towerRemoveRequested,
            this, &Monitor::slotTowerRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::imageDoubleClicked,
            this, &Monitor::slotEditImage);
    connect(m_graphicsView, &MonitorGraphicsView::imageRemoveRequested,
            this, &Monitor::slotImageRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::platformDoubleClicked,
            this, &Monitor::slotPlatformDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::pipeDoubleClicked,
            this, &Monitor::slotEditPipe);
    connect(m_graphicsView, &MonitorGraphicsView::standDoubleClicked,
            this, &Monitor::slotEditStand);
    connect(m_graphicsView, &MonitorGraphicsView::towerDoubleClicked,
            this, &Monitor::slotEditTower);
    connect(m_graphicsView, &MonitorGraphicsView::studioGroupEditRequested,
            this, &Monitor::openGroupStudio);
    connect(m_graphicsView, &MonitorGraphicsView::powerDoubleClicked,
            this, [this](quint32) {
        PowerDistributionDialog dlg(m_doc, this);
        dlg.exec();
        m_graphicsView->updatePowerSources();
        if (m_layersPanel) m_layersPanel->reload();
    });
    connect(m_graphicsView, &MonitorGraphicsView::networkDoubleClicked,
            this, &Monitor::slotEditNetworkNode);
    connect(m_graphicsView, &MonitorGraphicsView::networkRemoveRequested,
            this, &Monitor::slotNetworkNodeRemoveRequested);
    connect(m_graphicsView, &MonitorGraphicsView::targetDoubleClicked,
            this, &Monitor::slotTargetDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::addItemRequested, this,
            [this](const QString &what, QPointF scenePos) {
        /* The menus ask, this places: one path for "add one of these here",
           whether the click was on bare deck or on a platform. */
        m_pendingAddScenePos = scenePos;
        if (what == QLatin1String("fixture"))     slotAddFixture();
        else if (what == QLatin1String("target")) slotAddTarget();
        else if (what == QLatin1String("targetline")) slotAddTargetLine();
        else if (what == QLatin1String("person")) slotAddPerson();
    });
    connect(m_graphicsView, &MonitorGraphicsView::contextMenuRequested,
            this, &Monitor::slotCanvasContextMenu);
    connect(m_graphicsView, &MonitorGraphicsView::addFixtureToTrussRequested,
            this, &Monitor::slotAddFixtureToTruss);
    connect(m_graphicsView, &MonitorGraphicsView::mapSelectionChanged,
            this, &Monitor::slotMapSelectionChanged);
    // QUEUED: a structure change can originate mid-drag-drop; rebuilding the
    // tree synchronously would free rows Qt's drag machinery still holds. Queued
    // delivery defers the reload until the drop event has fully unwound.
    connect(m_graphicsView, &MonitorGraphicsView::mapStructureChanged,
            this, [this]() { if (m_layersPanel) m_layersPanel->reload(); },
            Qt::QueuedConnection);

    QSettings settings;
    QVariant var2 = settings.value(SETTINGS_VSPLITTER);
    if (var2.isValid() == true)
        m_splitter->restoreState(var2.toByteArray());

    // Restore the layers-panel visibility (visible by default so it's
    // discoverable). The matching toolbar action is synced in initGraphicsToolbar().
    const bool showLayers = settings.value(SETTINGS_LAYERS_PANEL, true).toBool();
    m_layersPanel->setVisible(showLayers);
    if (showLayers)
    {
        // Guarantee the panel gets real width even if the restored splitter
        // state (saved with one pane) would otherwise give it zero. Panel is the
        // left pane (index 0).
        const int total = qMax(600, m_splitter->width());
        m_splitter->setSizes(QList<int>() << 260 << (total - 260));
    }
    if (m_layersAction != NULL)
    {
        m_layersAction->blockSignals(true);
        m_layersAction->setChecked(showLayers);
        m_layersAction->blockSignals(false);
    }

    initGraphicsFooter(gcontainer, viewArea);

    fillGraphicsView();
}

void Monitor::initGraphicsFooter(QWidget *gcontainer, QWidget *viewArea)
{
    QGridLayout *viewGrid = qobject_cast<QGridLayout *>(viewArea->layout());

    /* ---- Ruler strips framing the canvas (top + left edges) ---- */
    m_rulerCorner = new QWidget(viewArea);
    m_rulerCorner->setFixedSize(MonitorRuler::thickness(), MonitorRuler::thickness());
    m_rulerCorner->setAutoFillBackground(true);
    {
        QPalette pal = m_rulerCorner->palette();
        pal.setColor(QPalette::Window, QColor(26, 26, 26));
        m_rulerCorner->setPalette(pal);
    }
    // The Layers popout handle lives IN the ruler corner (top-left, at the
    // rulers' intersection), not floating over the grid.
    if (m_layersAction != NULL)
    {
        QToolButton *layersHandle = new QToolButton(m_rulerCorner);
        layersHandle->setDefaultAction(m_layersAction);
        layersHandle->setAutoRaise(true);
        layersHandle->setIconSize(QSize(14, 14));
        layersHandle->setToolTip(tr("Show or hide the Layers sidebar"));
        QVBoxLayout *cl = new QVBoxLayout(m_rulerCorner);
        cl->setContentsMargins(1, 1, 1, 1);
        cl->addWidget(layersHandle);
    }

    m_hRuler = new MonitorRuler(m_graphicsView, MonitorRuler::Horizontal, viewArea);
    m_vRuler = new MonitorRuler(m_graphicsView, MonitorRuler::Vertical, viewArea);
    if (viewGrid != NULL)
    {
        viewGrid->addWidget(m_rulerCorner, 0, 0);
        viewGrid->addWidget(m_hRuler, 0, 1);
        viewGrid->addWidget(m_vRuler, 1, 0);
    }

    // Live readout: mouse-move over the canvas updates the footer label and the
    // ruler cursor markers.
    connect(m_graphicsView, &MonitorGraphicsView::cursorReadout,
            this, [this](QPointF hv) {
        if (m_readoutLabel != NULL)
            m_readoutLabel->setText(tr("%1: %2   %3: %4  %5")
                .arg(m_graphicsView->axisName(true))
                .arg(hv.x(), 0, 'f', 2)
                .arg(m_graphicsView->axisName(false))
                .arg(hv.y(), 0, 'f', 2)
                .arg(m_graphicsView->unitSuffix()));
    });
    // Move the ruler cursor markers in sync (needs the viewport pixel, which we
    // recompute from the readout is awkward — instead track the raw mouse via an
    // event filter on the viewport).
    m_graphicsView->viewport()->installEventFilter(this);

    // After an origin pick completes, un-arm any UI affordance and refresh.
    connect(m_graphicsView, &MonitorGraphicsView::originPicked,
            this, [this]() { m_graphicsView->refreshRulers(); });

    /* ---- Footer measurement bar ---- */
    QWidget *footer = new QWidget(gcontainer);
    footer->setAutoFillBackground(true);
    {
        QPalette pal = footer->palette();
        pal.setColor(QPalette::Window, QColor(34, 34, 34));
        footer->setPalette(pal);
    }
    QHBoxLayout *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(6, 2, 6, 2);
    fl->setSpacing(6);

    auto addSep = [&]() {
        QFrame *sep = new QFrame();
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        fl->addWidget(sep);
    };

    // ---- Size: [x] [y] [units] ----  (Overlay + View live on the RIGHT below)
    QVector3D gridSize = m_props->gridSize();
    /* One "Grid" control instead of six widgets. The button itself is the
       show/hide toggle -- the thing actually reached for during a session --
       and the SETUP behind it (stage size, units, subdivisions) lives in its
       drop-down, because those are set once when a show is built and then left
       alone. MenuButtonPopup keeps the toggle a single click; only the arrow
       opens the menu. The widgets and their connections are unchanged, just
       re-homed, so nothing about their behaviour moves with them. */
    m_gridWSpin = new QSpinBox();
    m_gridWSpin->setMinimum(1);
    m_gridWSpin->setValue(gridSize.x());
    connect(m_gridWSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridWidthChanged(int)));

    m_gridHSpin = new QSpinBox();
    m_gridHSpin->setMinimum(1);
    m_gridHSpin->setValue(gridSize.z());
    connect(m_gridHSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridHeightChanged(int)));

    // The unit is the person's, set in Settings -> Units (App applies it to
    // the show); shown here, not chosen here.
    m_unitsLabel = new QLabel();
    m_unitsLabel->setObjectName(QStringLiteral("monitorGridUnitsLabel"));
    m_unitsLabel->setToolTip(tr("Metres for mm, cm and m; feet for inches and feet. "
                                "Change it in Settings, Units."));
    updateUnitsLabel();

    m_gridSubdivSpin = new QSpinBox();
    m_gridSubdivSpin->setMinimum(1);
    m_gridSubdivSpin->setMaximum(8);
    m_gridSubdivSpin->setValue(m_props->gridSubdivisions());
    m_gridSubdivSpin->setToolTip(tr("Number of sub-divisions drawn inside each grid cell"));
    connect(m_gridSubdivSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridSubdivisionsChanged(int)));

    QToolButton *gridBtn = new QToolButton(footer);
    gridBtn->setText(tr("Grid"));
    gridBtn->setCheckable(true);
    gridBtn->setChecked(true);
    gridBtn->setToolTip(tr("Show or hide the grid lines — the arrow sets the "
                           "stage size, units and subdivisions"));
    gridBtn->setPopupMode(QToolButton::MenuButtonPopup);
    {
        QWidget *gw = new QWidget(footer);
        QFormLayout *gfl = new QFormLayout(gw);
        gfl->setContentsMargins(10, 8, 10, 8);
        QWidget *sizeRow = new QWidget(gw);
        QHBoxLayout *srl = new QHBoxLayout(sizeRow);
        srl->setContentsMargins(0, 0, 0, 0);
        srl->addWidget(m_gridWSpin);
        srl->addWidget(new QLabel("x", sizeRow));
        srl->addWidget(m_gridHSpin);
        gfl->addRow(tr("Stage size:"), sizeRow);
        gfl->addRow(tr("Units:"), m_unitsLabel);
        gfl->addRow(tr("Subdivisions:"), m_gridSubdivSpin);

        QMenu *gm = new QMenu(gridBtn);
        QWidgetAction *wa = new QWidgetAction(gm);
        wa->setDefaultWidget(gw);
        gm->addAction(wa);
        gridBtn->setMenu(gm);
    }
    fl->addWidget(gridBtn);
    connect(gridBtn, &QToolButton::toggled, this, [this](bool on) {
        m_graphicsView->setGridVisible(on);
        QSettings().setValue(SETTINGS_GRID, on);
    });

    // Snap: a dedicated on/off toggle, plus the division to snap to.
    m_snapToggle = new QToolButton(footer);
    m_snapToggle->setText(tr("Snap"));
    m_snapToggle->setCheckable(true);
    m_snapToggle->setChecked(m_props->snapDivisions() > 0);
    m_snapToggle->setToolTip(tr("Snap fixtures to the grid while moving them — "
                                "the arrow picks the division"));
    m_snapToggle->setPopupMode(QToolButton::MenuButtonPopup);
    fl->addWidget(m_snapToggle);

    m_snapCombo = new QComboBox();
    m_snapCombo->addItem(tr("Full"), 1);
    m_snapCombo->addItem(tr("1/2"), 2);
    m_snapCombo->addItem(tr("1/4"), 4);
    {
        const int d = m_props->snapDivisions();
        int snapIdx = m_snapCombo->findData(d > 0 ? d : 1);
        if (snapIdx >= 0)
            m_snapCombo->setCurrentIndex(snapIdx);
    }
    m_snapCombo->setEnabled(m_snapToggle->isChecked());
    m_snapCombo->setToolTip(tr("Grid subdivision to snap to"));
    {
        QWidget *sw = new QWidget(footer);
        QFormLayout *sfl = new QFormLayout(sw);
        sfl->setContentsMargins(10, 8, 10, 8);
        sfl->addRow(tr("Snap to:"), m_snapCombo);
        QMenu *sm = new QMenu(m_snapToggle);
        QWidgetAction *wa = new QWidgetAction(sm);
        wa->setDefaultWidget(sw);
        sm->addAction(wa);
        m_snapToggle->setMenu(sm);
    }

    auto applySnap = [this]() {
        const int div = m_snapToggle->isChecked() ? m_snapCombo->currentData().toInt() : 0;
        m_snapCombo->setEnabled(m_snapToggle->isChecked());
        m_graphicsView->setSnapDivisions(div);
        m_props->setSnapDivisions(div);
        m_doc->setModified();
    };
    connect(m_snapToggle, &QToolButton::toggled, this, [applySnap](bool) { applySnap(); });
    connect(m_snapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [applySnap](int) { applySnap(); });

    /* separator */
    {
        QFrame *sep = new QFrame();
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        fl->addWidget(sep);
    }

    /* One "Show" menu instead of four separate toggles. They are all the same
       kind of thing -- what gets DRAWN on the plot -- so they read better as a
       checklist than as four competing buttons, and the row gets three slots
       back. QActions rather than QToolButtons: the restore code below sets them
       with the same blockSignals()/setChecked() calls either way. */
    QToolButton *showBtn = new QToolButton(footer);
    showBtn->setText(tr("Show"));
    showBtn->setPopupMode(QToolButton::InstantPopup);
    showBtn->setToolTip(tr("Choose what is drawn on the plot"));
    QMenu *showMenu = new QMenu(showBtn);
    showBtn->setMenu(showMenu);
    fl->addWidget(showBtn);

    QAction *rulerBtn = showMenu->addAction(tr("Rulers"));
    rulerBtn->setCheckable(true);
    rulerBtn->setToolTip(tr("Show or hide the measurement rulers"));
    connect(rulerBtn, &QAction::toggled, this, [this](bool on) { setRulersVisible(on); });

    QAction *labelsBtn = showMenu->addAction(tr("Fixture labels"));
    labelsBtn->setCheckable(true);
    labelsBtn->setChecked(m_props->labelsVisible());
    connect(labelsBtn, &QAction::toggled, this, [this](bool on) { slotShowLabels(on); });

    /* Fixtures-on-structures. A truss or riser covered by its own fixtures is
       hard to click in the top view; hiding them clears the plot while
       structures are being placed. Free-standing fixtures are never hidden --
       there is nothing under them to reach. */
    QAction *mountedBtn = showMenu->addAction(tr("Fixtures on the rig"));
    mountedBtn->setCheckable(true);
    mountedBtn->setToolTip(tr("Show or hide the fixtures mounted on trusses, "
                              "pipes, towers and platforms, so the structure "
                              "underneath can be clicked"));
    connect(mountedBtn, &QAction::toggled, this, [this](bool on) {
        m_graphicsView->setMountedFixturesVisible(on);
        QSettings().setValue(SETTINGS_MOUNTEDFIXTURES, on);
    });

    /* Where the selected moving heads cannot aim: a design aid for deciding
       how to rig them (on top of a tower, or hung under a bar) before the
       load-in, so not remembered between sessions. */
    QAction *reachBtn = showMenu->addAction(tr("Where selected heads CANNOT reach"));
    reachBtn->setCheckable(true);
    reachBtn->setToolTip(tr("Shade, in the top view, where the selected moving "
                            "heads cannot point at subject height"));
    connect(reachBtn, &QAction::toggled, this, [this](bool on) {
        m_graphicsView->setReachOverlayVisible(on);
    });

    /* Where the selected heads' light lands: the answer to "what is this
       fixture covering", and what a count bubble shows when you double-click
       it. Top view, and not remembered -- it is a question you ask, not a way
       you leave the plot. */
    QAction *projectionBtn = showMenu->addAction(tr("Light footprint: everywhere selected heads CAN light"));
    projectionBtn->setCheckable(true);
    projectionBtn->setToolTip(tr("Shade every part of the deck the selected heads "
                                 "can put light on, with where each is pointed "
                                 "now drawn on top"));
    connect(projectionBtn, &QAction::toggled, this, [this](bool on) {
        m_graphicsView->setProjectionOverlayVisible(on);
    });
    /* Asking for one head's footprint from its own menu switches the overlay
       on; the tick here has to say so, or the next click on it turns them OFF
       by "turning them on". */
    connect(m_graphicsView, &MonitorGraphicsView::projectionOverlayChanged,
            projectionBtn, [projectionBtn](bool on) {
        QSignalBlocker block(projectionBtn);
        projectionBtn->setChecked(on);
    });

    /* Line of sight: what stands in a beam's way. The kinds are the
       stage's own (saved with the show) and are what a follow spot's Fade
       When Blocked, an Aim look's "Dim lights that can't see the target" and
       the blocked aim traces go by; whether the rig views stop beams at
       them too is a view choice, remembered here. */
    QMenu *sightMenu = showMenu->addMenu(tr("Line of sight"));
    sightMenu->setObjectName("lineOfSightMenu");
    sightMenu->setToolTipsVisible(true);
    QAction *sightBeams = sightMenu->addAction(tr("Beams stop at what is in their way"));
    sightBeams->setObjectName("lineOfSightBeams");
    sightBeams->setCheckable(true);
    sightBeams->setToolTip(tr("In the 3D views, a beam stops at a tower, a truss or a riser's "
                              "side in its way, as it already does at decks, drapes and walls"));
    {
        const bool on = QSettings().value(SETTINGS_LINEOFSIGHT, false).toBool();
        sightBeams->setChecked(on);
        StructureStudioView::setLineOfSightBeams(on);
    }
    connect(sightBeams, &QAction::toggled, this, [this](bool on) {
        StructureStudioView::setLineOfSightBeams(on);
        QSettings().setValue(SETTINGS_LINEOFSIGHT, on);
        foreach (StructureStudioView *v, findChildren<StructureStudioView *>())
            v->update();
    });
    sightMenu->addSection(tr("What blocks a beam"));
    foreach (int kind, BeamOcclusion::kinds())
    {
        QAction *k = sightMenu->addAction(BeamOcclusion::kindName(kind));
        k->setObjectName(QStringLiteral("lineOfSightKind%1").arg(kind));
        k->setCheckable(true);
        k->setChecked((m_props->occlusionKinds() & kind) != 0);
        if (kind == BeamOcclusion::Trusses || kind == BeamOcclusion::Pipes
            || kind == BeamOcclusion::Stands)
            k->setToolTip(tr("Only when at least %1 cm thick: a beam lights round a thin pipe")
                              .arg(qRound(m_props->occlusionMinSize() * 100.0f)));
        connect(k, &QAction::toggled, this, [this, kind](bool on) {
            const int kinds = m_props->occlusionKinds();
            m_props->setOcclusionKinds(on ? (kinds | kind) : (kinds & ~kind));
            m_doc->setModified();
            m_graphicsView->updateAimLines();
            foreach (StructureStudioView *v, findChildren<StructureStudioView *>())
                v->update();
        });
    }
    // A show loaded later brings its own kinds: tick them to match on opening.
    connect(sightMenu, &QMenu::aboutToShow, this, [this, sightMenu]() {
        foreach (QAction *a, sightMenu->actions())
        {
            const QString name = a->objectName();
            if (!name.startsWith(QLatin1String("lineOfSightKind")))
                continue;
            const int kind = name.mid(15).toInt();
            QSignalBlocker block(a);
            a->setChecked((m_props->occlusionKinds() & kind) != 0);
        }
    });

    QAction *centerBtn = showMenu->addAction(tr("Stage centre lines"));
    centerBtn->setCheckable(true);
    connect(centerBtn, &QAction::toggled, this, [this](bool on) {
        m_graphicsView->setCenterLinesVisible(on);
        QSettings().setValue(SETTINGS_CENTERLINES, on);
    });

    // Origin / centering control: click-to-place or a preset. The ⌖ glyph
    // marks it as the "set the 0,0 origin / centre" control.
    QToolButton *originBtn = new QToolButton(footer);
    originBtn->setText(QString::fromUtf8("\xE2\x8C\x96") + tr(" 0,0"));
    originBtn->setPopupMode(QToolButton::InstantPopup);
    originBtn->setToolTip(tr("Set where the rulers read 0,0 (origin / centre)"));
    {
        QMenu *om = new QMenu(originBtn);
        om->addAction(tr("Click to place 0,0…"), this, [this]() {
            m_graphicsView->beginPickOrigin();
        });
        om->addSeparator();
        om->addAction(tr("Stage centre"), this, [this]() {
            const double mpu = double(m_props->gridUnits() == MonitorProperties::Feet ? 0.3048 : 1.0);
            m_graphicsView->setStageOriginMetres(
                QPointF(m_props->gridSize().x() * mpu / 2.0,
                        m_props->gridSize().z() * mpu / 2.0));
        });
        om->addAction(tr("Downstage centre"), this, [this]() {
            const double mpu = double(m_props->gridUnits() == MonitorProperties::Feet ? 0.3048 : 1.0);
            m_graphicsView->setStageOriginMetres(
                QPointF(m_props->gridSize().x() * mpu / 2.0,
                        m_props->gridSize().z() * mpu));
        });
        om->addAction(tr("Top-left (0,0)"), this, [this]() {
            m_graphicsView->setStageOriginMetres(QPointF(0, 0));
        });
        originBtn->setMenu(om);
    }
    fl->addWidget(originBtn);

    fl->addStretch(1);

    // ---- Overlay + View, RIGHT-justified ----  the single source of state.
    fl->addWidget(new QLabel(tr("Overlay:")));
    m_overlayCombo = new QComboBox(footer);
    m_overlayCombo->addItem(tr("Normal"),     ViewNormal);
    m_overlayCombo->addItem(tr("Power"),      ViewPower);
    m_overlayCombo->addItem(tr("DMX"),        ViewDMX);
    m_overlayCombo->addItem(tr("Network"),    ViewNet);
    m_overlayCombo->addItem(tr("Stage only"), ViewStage);
    m_overlayCombo->setCurrentIndex(qMax(0, m_overlayCombo->findData(m_mapView)));
    m_overlayCombo->setToolTip(tr("Recolour / filter the plot — click to cycle, "
                                  "arrow for the full list"));
    m_overlayCombo->setProperty("cycleOnClick", true);
    m_overlayCombo->installEventFilter(this);   // click the box to cycle
    fl->addWidget(m_overlayCombo);
    connect(m_overlayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_mapView = m_overlayCombo->currentData().toInt();
        applyMapView(m_mapView);
    });
    addSep();
    fl->addWidget(new QLabel(tr("View:")));
    m_povCombo->setToolTip(tr("Point of view — click to cycle, arrow for the full list"));
    m_povCombo->setProperty("cycleOnClick", true);
    m_povCombo->installEventFilter(this);       // click the box to cycle
    fl->addWidget(m_povCombo);

    /* Turn the plot in quarter steps -- beside the point-of-view control,
       because it is the same kind of setting: how you are LOOKING at the rig,
       not anything about the rig. Some designers read a plan with downstage at
       the TOP; 180 degrees gives them that. Nothing in the workspace moves, and
       quarter turns keep stage left/right truthful, with the edge labels
       turning to match. */
    QToolButton *plotRotBtn = new QToolButton(footer);
    plotRotBtn->setToolTip(tr("Turn the plot 90° clockwise. A view setting "
                              "only — nothing in the workspace moves."));
    fl->addWidget(plotRotBtn);

    /* No overview button here: it lives in the main View menu (View > Rig
       Overview), because it is a window onto the whole show rather than a
       Lighting Studio tool. */
    auto applyPlotRotation = [this, plotRotBtn](int turns) {
        m_graphicsView->setViewRotation(turns);
        static const char *names[] = { "0°", "90°", "180°", "270°" };
        plotRotBtn->setText(QString("⟳ %1").arg(names[m_graphicsView->viewRotation() & 3]));
    };
    connect(plotRotBtn, &QToolButton::clicked, this, [this, applyPlotRotation]() {
        const int next = (m_graphicsView->viewRotation() + 1) & 3;
        applyPlotRotation(next);
        QSettings().setValue(SETTINGS_PLOTROTATION, next);
    });
    addSep();

    /* "N off stage" -- hidden unless it is true.
     *
       stage-structures-demo.qxw carries two fixtures stranded 13.8 m upstage,
       detached from a truss years ago and never seen since: absent from the
       plan, absent from the rig as anything you would notice, and quietly
       stretching the 3-D overview's framing around empty floor. Nothing in the
       app said so. Clicking selects them and scrolls them into view, which is
       only possible now the scene rect reaches that far. */
    m_offStageBtn = new QToolButton(footer);
    m_offStageBtn->setAutoRaise(true);
    m_offStageBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_offStageBtn->setVisible(false);
    m_offStageBtn->setStyleSheet("QToolButton { color: #e0a030; }");
    connect(m_offStageBtn, &QToolButton::clicked, this, [this]() {
        if (m_graphicsView != NULL)
            m_graphicsView->selectFixtures(m_graphicsView->fixturesOffGrid());
    });
    fl->addWidget(m_offStageBtn);
    connect(m_graphicsView, &MonitorGraphicsView::offGridFixturesChanged,
            this, [this](int n) {
        if (m_offStageBtn == NULL)
            return;
        m_offStageBtn->setVisible(n > 0);
        m_offStageBtn->setText(tr("%n fixture(s) off stage", "", n));
        m_offStageBtn->setToolTip(tr("%n fixture(s) sit outside the stage grid, so "
                                     "they are not where the plan normally shows "
                                     "anything and they stretch the rig view's "
                                     "framing. Click to select them.", "", n));
    });
    addSep();

    // Live coordinate readout, right-aligned.
    m_readoutLabel = new QLabel(tr("—"));
    m_readoutLabel->setMinimumWidth(160);
    m_readoutLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont rf = m_readoutLabel->font();
    rf.setStyleHint(QFont::Monospace);
    m_readoutLabel->setFont(rf);
    fl->addWidget(m_readoutLabel);

    qobject_cast<QVBoxLayout *>(gcontainer->layout())->addWidget(footer);

    // Restore ruler visibility (default on).
    QSettings settings;
    const bool showRulers = settings.value(SETTINGS_RULERS, true).toBool();
    rulerBtn->blockSignals(true);
    rulerBtn->setChecked(showRulers);
    rulerBtn->blockSignals(false);
    setRulersVisible(showRulers);

    // Restore grid visibility (default on).
    const bool showGrid = settings.value(SETTINGS_GRID, true).toBool();
    gridBtn->blockSignals(true);
    gridBtn->setChecked(showGrid);
    gridBtn->blockSignals(false);
    m_graphicsView->setGridVisible(showGrid);

    // Restore centre-lines visibility (default on).
    const bool showCenterLines = settings.value(SETTINGS_CENTERLINES, true).toBool();
    centerBtn->blockSignals(true);
    centerBtn->setChecked(showCenterLines);
    centerBtn->blockSignals(false);
    m_graphicsView->setCenterLinesVisible(showCenterLines);

    // Restore the plot rotation (default upright).
    applyPlotRotation(settings.value(SETTINGS_PLOTROTATION, 0).toInt());

    // Restore fixtures-on-structures visibility (default on).
    const bool showMounted = settings.value(SETTINGS_MOUNTEDFIXTURES, true).toBool();
    mountedBtn->blockSignals(true);
    mountedBtn->setChecked(showMounted);
    mountedBtn->blockSignals(false);
    m_graphicsView->setMountedFixturesVisible(showMounted);

    updateModeIndicator();
}

bool Monitor::eventFilter(QObject *watched, QEvent *event)
{
    /* Clicking the BOX cycles to the next item; the drop-down ARROW still opens
       the full list. Opted into with a "cycleOnClick" property rather than by
       naming specific combos, so the feature editors and the rig overview get
       the same behaviour as the plot -- the whole point being that a View combo
       works the same wherever it appears. */
    if (watched->property("cycleOnClick").toBool()
        && event->type() == QEvent::MouseButtonPress)
    {
        QComboBox *cb = static_cast<QComboBox *>(watched);
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QStyleOptionComboBox opt;
        opt.initFrom(cb);
        opt.subControls = QStyle::SC_All;
        const QRect arrow = cb->style()->subControlRect(
            QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, cb);
        if (arrow.contains(me->pos()))
            return false;                       // arrow → open the list as usual
        if (cb->count() > 0)
            cb->setCurrentIndex((cb->currentIndex() + 1) % cb->count());
        return true;                            // box → cycle, no popup
    }

    if (m_graphicsView != NULL && watched == m_graphicsView->viewport())
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (m_hRuler != NULL) m_hRuler->setCursorPixel(me->pos().x());
            if (m_vRuler != NULL) m_vRuler->setCursorPixel(me->pos().y());
            // Band the rulers with the selection's extent, so while you drag a
            // fixture you can read exactly where it sits on the ruler.
            const QRect sr = m_graphicsView->selectionViewportRect();
            if (sr.isValid())
            {
                if (m_hRuler != NULL) m_hRuler->setItemRange(sr.left(), sr.right());
                if (m_vRuler != NULL) m_vRuler->setItemRange(sr.top(), sr.bottom());
                /* Dragging it, the mark is its middle, not wherever it was
                   grabbed ("when moving the drape .. the mark on the ruler
                   should be the center of the drape"). */
                if (me->buttons() & Qt::LeftButton)
                {
                    if (m_hRuler != NULL) m_hRuler->setCursorPixel(sr.center().x());
                    if (m_vRuler != NULL) m_vRuler->setCursorPixel(sr.center().y());
                }
            }
            else
            {
                if (m_hRuler != NULL) m_hRuler->setItemRange(-1, -1);
                if (m_vRuler != NULL) m_vRuler->setItemRange(-1, -1);
            }
        }
        else if (event->type() == QEvent::Leave)
        {
            if (m_hRuler != NULL) m_hRuler->setCursorPixel(-1);
            if (m_vRuler != NULL) m_vRuler->setCursorPixel(-1);
            if (m_readoutLabel != NULL) m_readoutLabel->setText(tr("—"));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Monitor::updateModeIndicator()
{
    if (m_modeLabel == NULL)
        return;
    const QString pov = m_povCombo ? m_povCombo->currentText() : QString();
    const int overlay = m_mapView;

    QString text;
    QColor bg, fg(20, 20, 20);
    if (overlay == ViewStage)      { text = tr("STAGE ONLY"); bg = QColor(0, 150, 140); }
    else if (overlay == ViewPower) { text = tr("POWER");      bg = QColor(150, 90, 200); }
    else if (overlay == ViewDMX)   { text = tr("DMX");        bg = QColor(40, 130, 180); }
    else if (overlay == ViewNet)   { text = tr("NETWORK");    bg = QColor(60, 160, 120); }
    else { text = pov; bg = QColor(66, 66, 66); fg = QColor(210, 210, 210); }
    if (overlay != ViewNormal)
        text += QStringLiteral("   ·   ") + pov;   // keep the POV visible too

    QPalette pal = m_modeLabel->palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    m_modeLabel->setPalette(pal);
    QFont f = m_modeLabel->font(); f.setBold(true); m_modeLabel->setFont(f);
    m_modeLabel->setText(text);
}

void Monitor::setRulersVisible(bool on)
{
    if (m_hRuler != NULL)      m_hRuler->setVisible(on);
    if (m_vRuler != NULL)      m_vRuler->setVisible(on);
    if (m_rulerCorner != NULL) m_rulerCorner->setVisible(on);
    QSettings settings;
    settings.setValue(SETTINGS_RULERS, on);
}

void Monitor::fillGraphicsView(bool keepView)
{
    m_graphicsView->clearFixtures();

    m_gridWSpin->blockSignals(true);
    m_gridHSpin->blockSignals(true);
    m_gridSubdivSpin->blockSignals(true);
    m_snapCombo->blockSignals(true);

    if (m_props->gridUnits() == MonitorProperties::Meters)
        m_graphicsView->setGridMetrics(1000.0);
    else // m_props->gridUnits() == MonitorProperties::Feet
        m_graphicsView->setGridMetrics(304.8);
    updateUnitsLabel();

    m_gridWSpin->setValue(m_props->gridSize().x());
    m_gridHSpin->setValue(m_props->gridSize().z());
    m_gridSubdivSpin->setValue(m_props->gridSubdivisions());
    int snapIdx = m_snapCombo->findData(m_props->snapDivisions());
    m_snapCombo->setCurrentIndex(snapIdx >= 0 ? snapIdx : 0);
    m_gridWSpin->blockSignals(false);
    m_gridHSpin->blockSignals(false);
    m_gridSubdivSpin->blockSignals(false);
    m_snapCombo->blockSignals(false);

    m_graphicsView->setGridSubdivisions(m_props->gridSubdivisions());
    m_graphicsView->setSnapDivisions(m_props->snapDivisions());
    m_graphicsView->setGridSize(QSize(m_props->gridSize().x(), m_props->gridSize().z()));
    /* This runs when a workspace's monitor settings are applied, which is
       exactly the point a leftover zoom from the previous document stops
       making sense. */
    if (!keepView)
        m_graphicsView->resetViewZoom();
    m_graphicsView->setBackgroundImage(m_props->commonBackgroundImage());
    m_graphicsView->setBackgroundColor(m_props->commonBackgroundColor());

    foreach (quint32 fid, m_props->fixtureItemsID())
    {
        if (m_doc->fixture(fid) != NULL)
        {
            PreviewItem item = m_props->fixtureItem(fid, 0, 0);
            m_graphicsView->addFixture(fid, QPointF(item.m_position.x(), item.m_position.y()));
            qDebug() << "Gel color:" << item.m_color;
            m_graphicsView->setFixtureGelColor(fid, item.m_color);
            m_graphicsView->setFixtureRotation(fid, item.m_rotation.y());
        }
    }

    m_graphicsView->refreshFixtureLabels();   // render the loaded per-layer state

    // apply the persisted lock state once all fixtures are present
    m_lockAction->blockSignals(true);
    m_lockAction->setChecked(m_props->layoutLocked());
    updatePlotLockAppearance(m_props->layoutLocked());
    m_lockAction->blockSignals(false);
    m_graphicsView->setLayoutLocked(m_props->layoutLocked());

    // Migrate any anonymous first-cut groups into the registry, then rebuild
    // the Layers tree.
    m_graphicsView->ensureGroupRegistry();
    if (m_layersPanel != NULL)
        m_layersPanel->reload();
}

/* A generic dimmer: its "mode" is a channel count, not a named mode. */
static bool isGenericDimmer(const Fixture *fx)
{
    return fx->fixtureDef() == nullptr
           || (fx->fixtureDef()->manufacturer() == KXMLFixtureGeneric
               && fx->fixtureDef()->model() == KXMLFixtureGeneric);
}

QByteArray Monitor::rigSnapshot() const
{
    QHash<quint32, QString> names;
    QHash<quint32, QByteArray> power;     // what each draws, and its cap (undoable too)
    foreach (Fixture *fx, m_doc->fixtures())
    {
        names.insert(fx->id(), fx->name());
        QByteArray xml;
        QXmlStreamWriter w(&xml);
        fx->powerProfile().saveXML(&w);
        power.insert(fx->id(), xml);
    }
    // The network too: its nodes, ports and receivers are edited like the rig.
    QByteArray network;
    {
        QXmlStreamWriter w(&network);
        m_doc->networkTopology()->saveXML(&w);
    }
    // And where each fixture is patched, so a re-address can be undone.
    QHash<quint32, QList<quint32> > patch;
    foreach (Fixture *fx, m_doc->fixtures())
        patch.insert(fx->id(), { fx->isPatched() ? 1u : 0u, fx->universe(), fx->address() });
    // And each fixture's mode: the Patch dialog can change it to fit.
    QHash<quint32, QString> modes;
    QHash<quint32, quint32> dimmerChannels;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (isGenericDimmer(fx))
            dimmerChannels.insert(fx->id(), fx->channels());
        else if (fx->fixtureMode() != nullptr)
            modes.insert(fx->id(), fx->fixtureMode()->name());
    }
    // And each one's stage look (Fixture Properties sets it), then each
    // fixture's output rules (Output Rules…).
    QHash<quint32, int> looks;
    QHash<quint32, int> rules;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx->stageLook() != Fixture::AutoLook)
            looks.insert(fx->id(), int(fx->stageLook()));
        rules.insert(fx->id(), int(fx->outputRules()));
    }
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds << m_props->snapshot(m_doc) << names << power << network << patch << modes << dimmerChannels
       << looks << rules;
    return out;
}

void Monitor::restoreRig(const QByteArray &snap)
{
    QByteArray props;
    QHash<quint32, QString> names;
    QHash<quint32, QByteArray> power;
    QByteArray network;
    QDataStream ds(snap);
    ds >> props >> names;
    if (!ds.atEnd())
        ds >> power;
    if (!ds.atEnd())
        ds >> network;
    QHash<quint32, QList<quint32> > patch;
    if (!ds.atEnd())
        ds >> patch;
    QHash<quint32, QString> modes;
    QHash<quint32, quint32> dimmerChannels;
    if (!ds.atEnd())
        ds >> modes >> dimmerChannels;
    QHash<quint32, int> looks;
    const bool hasLooks = !ds.atEnd();
    if (hasLooks)
        ds >> looks;
    QHash<quint32, int> rules;
    const bool haveRules = !ds.atEnd();
    if (haveRules)
        ds >> rules;
    if (props.isEmpty())
        return;
    if (hasLooks)
    {
        foreach (Fixture *fx, m_doc->fixtures())
            fx->setStageLook(Fixture::StageLook(looks.value(fx->id(), int(Fixture::AutoLook))));
    }
    /* The patch: unpatch what wasn't, then put the rest back where it was --
       as one step, so fixtures can swap or shift onto each other's channels.
       Modes first: they decide the footprints being put back. */
    {
        QList<Doc::FixturePatch> back;
        QSet<quint32> remoded;
        for (auto it = modes.constBegin(); it != modes.constEnd(); ++it)
        {
            Fixture *fx = m_doc->fixture(it.key());
            if (fx == nullptr || fx->fixtureDef() == nullptr || fx->fixtureMode() == nullptr
                || fx->fixtureMode()->name() == it.value())
                continue;
            if (QLCFixtureMode *m = fx->fixtureDef()->mode(it.value()))
            {
                fx->blockSignals(true);
                fx->setFixtureDefinition(fx->fixtureDef(), m);
                fx->blockSignals(false);
                remoded.insert(fx->id());
            }
        }
        for (auto it = dimmerChannels.constBegin(); it != dimmerChannels.constEnd(); ++it)
        {
            Fixture *fx = m_doc->fixture(it.key());
            if (fx == nullptr || fx->channels() == it.value() || it.value() == 0 || !isGenericDimmer(fx))
                continue;
            QLCFixtureDef *def = fx->genericDimmerDef(int(it.value()));
            fx->blockSignals(true);
            fx->setFixtureDefinition(def, fx->genericDimmerMode(def, int(it.value())));
            fx->blockSignals(false);
            remoded.insert(fx->id());
        }
        for (auto it = patch.constBegin(); it != patch.constEnd(); ++it)
        {
            Fixture *fx = m_doc->fixture(it.key());
            if (fx == nullptr || it.value().size() < 3)
                continue;
            const bool wasPatched = it.value().at(0) != 0;
            if (!wasPatched && fx->isPatched())
                m_doc->unpatchFixture(fx->id());
            else if (wasPatched && (!fx->isPatched() || fx->universe() != it.value().at(1)
                                    || fx->address() != it.value().at(2) || remoded.contains(fx->id())))
                back << Doc::FixturePatch{ fx->id(), it.value().at(1), it.value().at(2) };
        }
        if (!back.isEmpty())
            m_doc->patchFixtures(back);
    }
    if (!network.isEmpty())
    {
        QXmlStreamReader r(network);
        if (r.readNextStartElement())
            m_doc->networkTopology()->loadXML(r, m_doc);
    }
    for (auto it = power.constBegin(); it != power.constEnd(); ++it)
    {
        Fixture *fx = m_doc->fixture(it.key());
        if (fx == nullptr)
            continue;
        PowerProfile pp;
        QXmlStreamReader r(it.value());
        if (r.readNextStartElement())
            pp.loadXML(r);
        fx->setPowerProfile(pp);
    }
    if (haveRules)
    {
        foreach (Fixture *fx, m_doc->fixtures())
            fx->setOutputRules(Fixture::OutputRules(rules.value(fx->id(), 0)));
    }

    m_props->restoreSnapshot(props, m_doc);
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
    {
        Fixture *fx = m_doc->fixture(it.key());
        if (fx != nullptr && fx->name() != it.value())
            fx->setName(it.value());
    }

    if (m_graphicsView != nullptr)
    {
        fillGraphicsView(true);
        m_graphicsView->updateNetworkNodes();
        m_graphicsView->refreshAllItems();
        m_graphicsView->refreshFixtureBindings();
        m_graphicsView->refreshFixtureCountBadges();
    }
    m_doc->setModified();
}

void Monitor::undo()
{
    if (m_graphicsView != nullptr)
        m_graphicsView->undoLastMove();
}

void Monitor::pushRigUndo(const QByteArray &before)
{
    if (m_graphicsView != nullptr && !before.isEmpty() && rigSnapshot() != before)
        m_graphicsView->pushRigUndo(before);
}

void Monitor::showGraphicsView()
{
    qDebug() << Q_FUNC_INFO;

    m_DMXToolBar->hide();
    m_scrollArea->hide();

    m_graphicsToolBar->show();
    m_graphicsView->show();

    // Entering the 2D view always lands in the editable Top POV; sync the combo
    // (which may still read "DMX" from the switch) without re-triggering it.
    if (m_povCombo != NULL)
    {
        m_graphicsView->setViewPOV(MonitorGraphicsView::PovTop);
        m_povCombo->blockSignals(true);
        m_povCombo->setCurrentIndex(0);   // 2D — Top
        m_povCombo->blockSignals(false);
        if (m_dmxViewCombo != NULL)
        {
            m_dmxViewCombo->blockSignals(true);
            m_dmxViewCombo->setCurrentIndex(0);
            m_dmxViewCombo->blockSignals(false);
        }
        if (m_lockAction != NULL) m_lockAction->setEnabled(true);
        if (m_snapCombo != NULL)  m_snapCombo->setEnabled(true);
    }

    // Graphics view needs to monitor all the universes
    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        m_doc->inputOutputMap()->setUniverseMonitor(i, true);
    }
}

void Monitor::showCurrentView()
{
    // The DMX channel-grid view is retired — it never fit the studio map and
    // broke elevation/POV handling. The 2D studio map is now the only view.
    if (m_props->displayMode() != MonitorProperties::Graphics)
        m_props->setDisplayMode(MonitorProperties::Graphics);
    showGraphicsView();
}

void Monitor::updateView()
{
    fillDMXView();
    fillGraphicsView();
    showCurrentView();
}

void Monitor::highlightFixtures(const QList<quint32> &ids)
{
    if (m_graphicsView != NULL)
        m_graphicsView->highlightFixtures(ids);
}

void Monitor::showFocusFixture(quint32 fid)
{
    QList<quint32> ids;
    if (fid != UINT_MAX)
        ids << fid;
    if (m_graphicsView != NULL)
        m_graphicsView->highlightFixtures(ids);
    if (m_overviewView != NULL)
        m_overviewView->setHighlight(ids);
}

void Monitor::openFocusMode()
{
    QList<quint32> selection;
    if (m_doc->programmer() != NULL)
        selection = m_doc->programmer()->programmerSelection();
    FocusModePanel::open(m_doc, window(), m_activeSceneId, selection);
}

void Monitor::setActiveScene(quint32 sceneId)
{
    m_activeSceneId = sceneId;
    if (m_graphicsView != NULL)
        m_graphicsView->setActiveScene(sceneId);
    /* The overview shows the same aim, so it needs the same scene. Without
       this you could set a target, watch the studio draw its dashed lines, and
       find the rig view's heads still pointing wherever they were left. */
    if (m_overviewView != NULL)
        m_overviewView->setActiveScene(sceneId);
}

void Monitor::setFollowSpotPin(bool visible, float xMeters, float yMeters)
{
    if (m_graphicsView != NULL)
        m_graphicsView->setFollowSpotPin(visible, xMeters, yMeters);
}

Monitor* Monitor::instance()
{
    return s_instance;
}

void Monitor::saveSettings()
{
    QSettings settings;
    // No more standalone-window geometry to save — Monitor is a permanent
    // tab now; its DetachedContext (if detached) saves its own geometry
    // into the workspace file's AppState, same as any other tab.

    if (m_splitter != NULL)
    {
        QSettings settings;
        settings.setValue(SETTINGS_VSPLITTER, m_splitter->saveState());
    }

    // Persist the INTENDED visibility (the toolbar toggle), not isVisible():
    // at quit time the Monitor window is closing so the panel already reports
    // not-visible, which would wrongly save "hidden" on every exit.
    if (m_layersAction != NULL)
        settings.setValue(SETTINGS_LAYERS_PANEL, m_layersAction->isChecked());

    if (m_monitorWidget != NULL)
        m_props->setFont(m_monitorWidget->font());
}

void Monitor::createAndShow(QWidget* parent, Doc* doc)
{
    Q_UNUSED(parent)
    Q_UNUSED(doc)

    // Monitor is constructed once as a permanent tab (App::init()), not
    // lazily here — this now just makes sure it's what the user is looking
    // at: switch to its tab, or if it's currently detached into its own
    // window (same double-click mechanism as any other tab), bring that
    // window forward instead.
    Q_ASSERT(s_instance != NULL);
    if (s_instance == NULL)
        return;

    for (QWidget *p = s_instance->parentWidget(); p != NULL; p = p->parentWidget())
    {
        if (QTabWidget *tabs = qobject_cast<QTabWidget *>(p))
        {
            tabs->setCurrentWidget(s_instance);
            return;
        }
        if (QMainWindow *mw = qobject_cast<QMainWindow *>(p))
        {
            // Belt-and-suspenders recovery: a workspace opened on a different
            // machine/monitor layout than it was saved on can leave this
            // window off-screen or degenerate-tiny (see AppUtil::
            // ensureWindowOnScreen's own comment) despite the load-time fix in
            // App::loadXML(). Re-checking here means "View -> Lighting Studio"
            // (or its Ctrl+Shift+M shortcut) doubles as a manual "pop it back
            // onto this screen" recovery action any time it's invoked, with no
            // separate UI needed -- including for a window already broken
            // before this fix existed, on a workspace already loaded.
            AppUtil::ensureWindowOnScreen(mw);
            mw->show();
            mw->raise();
            mw->activateWindow();
            return;
        }
    }
}

/****************************************************************************
 * Menu
 ****************************************************************************/

void Monitor::initDMXToolbar()
{
    QActionGroup* group;
    QAction* action;
    m_DMXToolBar = new QToolBar(this);
    // The app-wide 20x20 toolbar icon (Fixtures, Shows, ...); unset, it
    // was the platform's 32 and a row half again as tall as the others.
    m_DMXToolBar->setIconSize(QSize(20, 20));
    m_toolsHolder->layout()->addWidget(m_DMXToolBar);

    action = m_DMXToolBar->addAction(tr("2D View"));
    m_DMXToolBar->addSeparator();
    action->setData(MonitorProperties::Graphics);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotSwitchMode()));

    /* Font */
    m_DMXToolBar->addAction(QIcon(":/fonts.png"), tr("Font"),
                       this, SLOT(slotChooseFont()));

    m_DMXToolBar->addSeparator();

    /* Channel style */
    group = new QActionGroup(this);
    group->setExclusive(true);

    action = m_DMXToolBar->addAction(tr("DMX Channels"));
    action->setToolTip(tr("Show absolute DMX channel numbers"));
    action->setCheckable(true);
    action->setData(MonitorProperties::DMXChannels);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotChannelStyleTriggered()));
    m_DMXToolBar->addAction(action);
    group->addAction(action);
    if (m_props->channelStyle() == MonitorProperties::DMXChannels)
        action->setChecked(true);

    action = m_DMXToolBar->addAction(tr("Relative Channels"));
    action->setToolTip(tr("Show channel numbers relative to fixture"));
    action->setCheckable(true);
    action->setData(MonitorProperties::RelativeChannels);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotChannelStyleTriggered()));
    m_DMXToolBar->addAction(action);
    group->addAction(action);
    if (m_props->channelStyle() == MonitorProperties::RelativeChannels)
        action->setChecked(true);

    m_DMXToolBar->addSeparator();

    /* Value display style */
    group = new QActionGroup(this);
    group->setExclusive(true);

    action = m_DMXToolBar->addAction(tr("DMX Values"));
    action->setToolTip(tr("Show DMX values 0-255"));
    action->setCheckable(true);
    action->setData(MonitorProperties::DMXValues);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotValueStyleTriggered()));
    m_DMXToolBar->addAction(action);
    group->addAction(action);
    action->setChecked(true);
    if (m_props->valueStyle() == MonitorProperties::DMXValues)
        action->setChecked(true);

    action = m_DMXToolBar->addAction(tr("Percent Values"));
    action->setToolTip(tr("Show percentage values 0-100%"));
    action->setCheckable(true);
    action->setData(MonitorProperties::PercentageValues);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotValueStyleTriggered()));
    m_DMXToolBar->addAction(action);
    group->addAction(action);
    if (m_props->valueStyle() == MonitorProperties::PercentageValues)
        action->setChecked(true);

    /* View selector — so you can leave the DMX grid back to the 2D map. Mirrors
     *  the footer View combo; the Universe combo below sits to its right. */
    m_DMXToolBar->addSeparator();
    QLabel *dmxViewLabel = new QLabel(tr("View:"));
    dmxViewLabel->setMargin(5);
    m_DMXToolBar->addWidget(dmxViewLabel);
    m_dmxViewCombo = new QComboBox(this);
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Top"),   int(MonitorGraphicsView::PovTop));
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Front"), int(MonitorGraphicsView::PovFront));
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Side"),  int(MonitorGraphicsView::PovSide));
    m_dmxViewCombo->setToolTip(tr("Switch to the 2D map view — click to cycle, "
                                  "arrow for the full list"));
    m_dmxViewCombo->setProperty("cycleOnClick", true);
    m_dmxViewCombo->installEventFilter(this);   // click the box to cycle
    m_DMXToolBar->addWidget(m_dmxViewCombo);
    connect(m_dmxViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        // Drive the footer combo, which owns the actual view-switch logic.
        if (m_povCombo != NULL && m_povCombo->currentIndex() != idx)
            m_povCombo->setCurrentIndex(idx);
    });

    /* Universe combo box */
    m_DMXToolBar->addSeparator();

    QLabel *uniLabel = new QLabel(tr("Universe"));
    uniLabel->setMargin(5);
    m_DMXToolBar->addWidget(uniLabel);

    QComboBox *uniCombo = new QComboBox(this);
    uniCombo->addItem(tr("All universes"), Universe::invalid());
    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
        uniCombo->addItem(m_doc->inputOutputMap()->getUniverseNameByIndex(i), uniID);
    }
    connect(uniCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotUniverseSelected(int)));
    m_DMXToolBar->addWidget(uniCombo);

    if (QLCFile::hasWindowManager() == false)
    {
        QWidget* widget = new QWidget(this);
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_DMXToolBar->addWidget(widget);

        action = m_DMXToolBar->addAction(tr("Close"));
        action->setToolTip(tr("Close this window"));
        action->setIcon(QIcon(":/delete.png"));
        connect(action, SIGNAL(triggered(bool)),
                this, SLOT(close()));
        m_DMXToolBar->addAction(action);
        group->addAction(action);
    }

    // Match the main window's icon/text display preference.
    applyToolbarLabelMode();
}

void Monitor::initGraphicsToolbar()
{
    QAction* action;

    m_graphicsToolBar = new QToolBar(this);
    // The app-wide 20x20 toolbar icon (Fixtures, Shows, ...); unset, it
    // was the platform's 32 and a row half again as tall as the others.
    m_graphicsToolBar->setIconSize(QSize(20, 20));
    m_toolsHolder->layout()->addWidget(m_graphicsToolBar);

    // Unified view selector: the DMX channel grid, or the 2D map from a chosen
    // point of view. Front/Side are read-only elevation views.
    // The View (POV) selector is created here (so early references are safe) but
    // PLACED in the footer bar (initGraphicsFooter), alongside the Overlay/Size/
    // Grid/Snap controls, so all the view modifiers live on one row.
    m_povCombo = new QComboBox();
    m_povCombo->addItem(tr("2D \342\200\224 Top"),   int(MonitorGraphicsView::PovTop));
    m_povCombo->addItem(tr("2D \342\200\224 Front"), int(MonitorGraphicsView::PovFront));
    m_povCombo->addItem(tr("2D \342\200\224 Side"),  int(MonitorGraphicsView::PovSide));
    m_povCombo->setCurrentIndex(0);   // 2D Top (DMX grid retired)
    m_povCombo->setToolTip(tr("Choose the 2D map view: top / front / side. "
                              "Front & Side are read-only elevation views that show height."));
    connect(m_povCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotPOVChanged(int)));

    // Grid Size / Units / Subdivisions / Snap have moved to the footer
    // measurement bar (initGraphicsFooter) to de-clutter the top toolbar.

    m_lockAction = m_graphicsToolBar->addAction(QIcon(":/lock.png"), tr("Edit Plot"));
    m_lockAction->setCheckable(true);
    m_lockAction->setChecked(m_props->layoutLocked());
    updatePlotLockAppearance(m_props->layoutLocked());
    connect(m_lockAction, SIGNAL(toggled(bool)), this, SLOT(slotLockToggled(bool)));

    m_inventoryAction = m_graphicsToolBar->addAction(QIcon(":/fixture.png"), tr("Inventory"));
    m_inventoryAction->setCheckable(true);
    m_inventoryAction->setToolTip(tr("Show the fixtures staged in the inventory, ready to drag onto the plot"));
    connect(m_inventoryAction, &QAction::toggled, this, [this](bool on) {
        if (m_inventoryPanel != NULL)
            m_inventoryPanel->setVisible(on);
        QSettings().setValue(QStringLiteral("monitor/showInventory"), on);
    });

    // One undo for the rig: moves, pastes, and every edit dialog's OK.
    // (Connected to the view in initGraphicsView(), which comes after.)
    m_undoAction = m_graphicsToolBar->addAction(QIcon(":/undo.png"), tr("Undo"));
    m_undoAction->setToolTip(tr("Undo the last move, paste or edit on the rig (%1)")
                             .arg(QKeySequence(QKeySequence::Undo).toString(QKeySequence::NativeText)));
    m_undoAction->setObjectName(QStringLiteral("plotUndo"));
    m_undoAction->setEnabled(false);

    m_graphicsToolBar->addSeparator();
    // Group/Ungroup, the Overlay selector, Copy/Paste, labels and background
    // live in the "More" popup (built after Add/Remove below), to keep this
    // toolbar lean. Grid/units/snap/rulers stay in the footer.

    // Consolidated Add button with popup menu
    m_addBtn = new QToolButton(this);
    QToolButton *addBtn = m_addBtn;
    addBtn->setIcon(QIcon(":/edit_add.png"));
    addBtn->setText(tr("Add"));
    addBtn->setToolTip(tr("Add…"));
    addBtn->setPopupMode(QToolButton::InstantPopup);
    {
        QMenu *addMenu = new QMenu(addBtn);
        addMenu->addAction(QIcon(":/fixture.png"), tr("Add Fixture"),
                           this, SLOT(slotAddFixture()));
        addMenu->addAction(QIcon(":/group.png"), tr("Add Truss"),
                           this, SLOT(slotAddTruss()));
        addMenu->addAction(tr("Add Platform/Riser"),
                           this, SLOT(slotAddPlatform()));
        addMenu->addAction(tr("Add Steps"),
                           this, SLOT(slotAddSteps()));
        QMenu *sceneryMenu = addMenu->addMenu(tr("Add Scenery"));
        foreach (int kind, Scenery::kinds())
            sceneryMenu->addAction(Scenery::kindName(kind), this, [this, kind]() { slotAddScenery(kind); });
        addMenu->addAction(tr("Add Fountain…"), this, [this]() { slotAddFountain(); });
        addMenu->addAction(QIcon(":/image.png"), tr("Add Image"),
                           this, SLOT(slotAddImage()));
        addMenu->addAction(tr("Add Network Node"),
                           this, SLOT(slotAddNetworkNode()));
        addMenu->addSeparator();
        addMenu->addAction(tr("Add Target Position"),
                           this, SLOT(slotAddTarget()));
        addMenu->addAction(tr("Add Target Line"),
                           this, SLOT(slotAddTargetLine()));
        addMenu->addAction(tr("Add Person"),
                           this, SLOT(slotAddPerson()));
        addBtn->setMenu(addMenu);
    }
    m_graphicsToolBar->addWidget(addBtn);

    // Single Remove button removes whatever is selected
    m_graphicsToolBar->addAction(QIcon(":/edit_remove.png"), tr("Remove selected"),
                                 this, SLOT(slotRemoveSelected()));

    m_graphicsToolBar->addSeparator();

    // "More" popup — the secondary view/edit tools, folded off the toolbar to
    // keep it lean. (Most also have shortcuts and/or a right-click equivalent.)
    /* Focus mode: align the rig before a show, a head at a time
       (FOCUS_MODE_DESIGN.md). Also View > Focus Mode, the Rig Overview, and
       View + Menu on the controller. */
    QAction *focusAct = m_graphicsToolBar->addAction(QIcon(":/target.png"), tr("Focus"));
    focusAct->setObjectName(QStringLiteral("focusModeAction"));
    focusAct->setToolTip(tr("Focus mode: aim each moving head at its reference in turn and "
                            "nudge its calibration offsets until it lands there "
                            "(controller: View + Menu)"));
    connect(focusAct, &QAction::triggered, this, &Monitor::openFocusMode);

    m_moreBtn = new QToolButton(this);
    QToolButton *moreBtn = m_moreBtn;
    moreBtn->setIcon(QIcon(":/configure.png"));
    moreBtn->setText(tr("More"));
    moreBtn->setToolTip(tr("More view and editing tools"));
    moreBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *moreMenu = new QMenu(moreBtn);

    // Overlay now lives as a dropdown on the footer bar (initGraphicsFooter),
    // so the current overlay is always visible instead of buried in this menu.


    // Group / ungroup the selection (also Cmd/Ctrl+G, Cmd/Ctrl+Shift+G, and the
    // right-click menu). Enabled state is driven by the current selection.
    m_groupAction = new QAction(QIcon(":/group.png"), tr("Group"), this);
    m_groupAction->setToolTip(tr("Group the selected items so they select and move together (Ctrl+G)"));
    m_groupAction->setEnabled(false);
    connect(m_groupAction, SIGNAL(triggered()), this, SLOT(slotGroupItems()));
    moreMenu->addAction(m_groupAction);

    m_ungroupAction = new QAction(QIcon(":/ungroup.png"), tr("Ungroup"), this);
    m_ungroupAction->setToolTip(tr("Ungroup the selected group (Ctrl+Shift+G)"));
    m_ungroupAction->setEnabled(false);
    connect(m_ungroupAction, SIGNAL(triggered()), this, SLOT(slotUngroupItems()));
    moreMenu->addAction(m_ungroupAction);

    moreMenu->addSeparator();

    moreMenu->addAction(tr("Select All (Ctrl+A)"), this, [this]() {
        if (m_graphicsView != NULL)
            m_graphicsView->selectAllMapItems();
    });

    // Copy / paste of stage features (trusses, platforms, targets). addAction()
    // keeps the Cmd/Ctrl+C/V shortcuts alive on the window even off the toolbar.
    QAction *copyAct = new QAction(QIcon(":/editcopy.png"), tr("Copy selected features"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyAct, SIGNAL(triggered()), this, SLOT(slotCopySelected()));
    moreMenu->addAction(copyAct);
    addAction(copyAct);

    m_pasteAction = new QAction(QIcon(":/editpaste.png"), tr("Paste features"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_pasteAction->setEnabled(false);
    connect(m_pasteAction, SIGNAL(triggered()), this, SLOT(slotPasteFeatures()));
    moreMenu->addAction(m_pasteAction);
    addAction(m_pasteAction);

    // (Show/hide labels moved to a footer button, beside Snap / Rulers.)

    // Background image is now a placeable object (Add ▸ Add Image) that lives in
    // a layer; flat background colour stays here.
    moreMenu->addAction(QIcon(":/color.png"), tr("Set background colour"),
                        this, SLOT(slotSetBackgroundColor()));
    moreMenu->addAction(tr("Network Topology…"), this, [this]() { slotEditNetworkNode(0); });
    moreMenu->addAction(tr("Network Report…"), this, &Monitor::slotNetworkReport);
    moreMenu->addAction(tr("Patch List…"), this, [this]() {
        PatchListDialog dlg(m_doc, this);
        dlg.exec();
    });

    moreBtn->setMenu(moreMenu);
    m_graphicsToolBar->addWidget(moreBtn);

    // The Layers toggle is NOT on the toolbar — it lives as a small popout
    // handle at the top-left of the canvas (added in initGraphicsFooter).
    m_layersAction = new QAction(QIcon(":/frame.png"), tr("Layers"), this);
    m_layersAction->setCheckable(true);
    m_layersAction->setToolTip(tr("Show or hide the Layers sidebar"));
    connect(m_layersAction, SIGNAL(toggled(bool)), this, SLOT(slotToggleLayersPanel(bool)));

    if (QLCFile::hasWindowManager() == false)
    {
        QWidget* widget = new QWidget(this);
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_graphicsToolBar->addWidget(widget);

        action = m_graphicsToolBar->addAction(tr("Close"));
        action->setToolTip(tr("Close this window"));
        action->setIcon(QIcon(":/delete.png"));
        connect(action, SIGNAL(triggered(bool)),
                this, SLOT(close()));
        m_graphicsToolBar->addAction(action);
    }

    // Match the main window's icon/text display preference.
    applyToolbarLabelMode();
}

void Monitor::applyToolbarLabelMode()
{
    // Mirror App::TabLabelMode: 0 = Icon+Text (→ text under icon),
    // 1 = Icons only, 2 = Text only. Same "workspace/tabLabelMode" setting.
    Qt::ToolButtonStyle style = Qt::ToolButtonTextUnderIcon;
    const int mode = QSettings().value(QStringLiteral("workspace/tabLabelMode"), 0).toInt();
    if (mode == 1)
        style = Qt::ToolButtonIconOnly;
    else if (mode == 2)
        style = Qt::ToolButtonTextOnly;

    if (m_DMXToolBar)      m_DMXToolBar->setToolButtonStyle(style);
    if (m_graphicsToolBar) m_graphicsToolBar->setToolButtonStyle(style);
    // Custom buttons added via addWidget don't inherit the toolbar style.
    if (m_addBtn)          m_addBtn->setToolButtonStyle(style);
    if (m_moreBtn)         m_moreBtn->setToolButtonStyle(style);
}

void Monitor::slotChooseFont()
{
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, m_monitorWidget->font(), this);
    if (ok == true)
    {
        m_monitorWidget->setFont(f);
        m_props->setFont(f);
    }
}

void Monitor::slotChannelStyleTriggered()
{
    QAction* action = qobject_cast<QAction*> (QObject::sender());
    Q_ASSERT(action != NULL);

    action->setChecked(true);
    m_props->setChannelStyle(MonitorProperties::ChannelStyle(action->data().toInt()));
    emit channelStyleChanged(m_props->channelStyle());
}

void Monitor::slotValueStyleTriggered()
{
    QAction* action = qobject_cast<QAction*> (QObject::sender());
    Q_ASSERT(action != NULL);

    action->setChecked(true);
    m_props->setValueStyle(MonitorProperties::ValueStyle(action->data().toInt()));
    emit valueStyleChanged(m_props->valueStyle());
}

void Monitor::slotSwitchMode()
{
    QAction* action = qobject_cast<QAction*> (QObject::sender());
    Q_ASSERT(action != NULL);

    m_props->setDisplayMode(MonitorProperties::DisplayMode(action->data().toInt()));
    showCurrentView();
}

/****************************************************************************
 * Fixture added/removed stuff
 ****************************************************************************/

void Monitor::updateFixtureLabelStyles()
{
    QListIterator <MonitorFixture*> it(m_monitorFixtures);
    while (it.hasNext() == true)
        it.next()->updateLabelStyles();
}

void Monitor::createMonitorFixture(Fixture* fxi)
{
    MonitorFixture* mof = new MonitorFixture(m_monitorWidget, m_doc);
    mof->setFixture(fxi->id());
    mof->slotChannelStyleChanged(m_props->channelStyle());
    mof->slotValueStyleChanged(m_props->valueStyle());
    mof->show();

    /* Make mof listen to value & channel style changes */
    connect(this, SIGNAL(valueStyleChanged(MonitorProperties::ValueStyle)),
            mof, SLOT(slotValueStyleChanged(MonitorProperties::ValueStyle)));
    connect(this, SIGNAL(channelStyleChanged(MonitorProperties::ChannelStyle)),
            mof, SLOT(slotChannelStyleChanged(MonitorProperties::ChannelStyle)));

    m_monitorLayout->addItem(new MonitorLayoutItem(mof));
    m_monitorFixtures.append(mof);
}

void Monitor::slotFixtureAdded(quint32 fxi_id)
{
    /* Nothing to keep in sync while the DMX view is unbuilt -- populateDMXView()
       will pick this fixture up from the Doc when the view is first shown.
       Adding it here would leave a list holding only the fixtures added since
       load, which the view would then treat as complete. */
    if (m_dmxViewPopulated == false)
        return;

    Fixture* fxi = m_doc->fixture(fxi_id);
    if (fxi != NULL)
        createMonitorFixture(fxi);
}

void Monitor::slotFixtureChanged(quint32 fxi_id)
{
    QListIterator <MonitorFixture*> it(m_monitorFixtures);
    while (it.hasNext() == true)
    {
        MonitorFixture* mof = it.next();
        if (mof->fixture() == fxi_id)
            mof->setFixture(fxi_id);
    }

    m_monitorLayout->sort();
    m_monitorWidget->updateGeometry();

    m_graphicsView->updateFixture(fxi_id);
}

void Monitor::slotFixtureRemoved(quint32 fxi_id)
{
    QMutableListIterator <MonitorFixture*> it(m_monitorFixtures);
    while (it.hasNext() == true)
    {
        MonitorFixture* mof = it.next();
        if (mof->fixture() == fxi_id)
        {
            it.remove();
            delete mof;
        }
    }

    m_graphicsView->removeFixture(fxi_id);
}

void Monitor::slotUniverseSelected(int index)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    m_currentUniverse = combo->itemData(index).toUInt();

    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
        if (m_currentUniverse == Universe::invalid() || uniID == m_currentUniverse)
            m_doc->inputOutputMap()->setUniverseMonitor(i, true);
        else
            m_doc->inputOutputMap()->setUniverseMonitor(i, false);
    }

    fillDMXView();
}

/********************************************************************
 * Graphics View
 ********************************************************************/

void Monitor::slotGridWidthChanged(int value)
{
    Q_ASSERT(m_graphicsView != NULL);

    m_graphicsView->setGridSize(QSize(value, m_gridHSpin->value()));
    m_props->setGridSize(QVector3D(value, m_props->gridSize().y(), m_gridHSpin->value()));
}

void Monitor::slotGridHeightChanged(int value)
{
    Q_ASSERT(m_graphicsView != NULL);

    m_graphicsView->setGridSize(QSize(m_gridWSpin->value(), value));
    m_props->setGridSize(QVector3D(m_gridWSpin->value(), m_props->gridSize().y(), value));
}

void Monitor::updateUnitsLabel()
{
    if (m_unitsLabel == NULL)
        return;
    m_unitsLabel->setText(m_props->gridUnits() == MonitorProperties::Feet
                          ? tr("Feet (Settings, Units)") : tr("Metres (Settings, Units)"));
}

void Monitor::unitsChanged()
{
    // App has converted the stage (MonitorProperties::convertGridUnits);
    // redraw everything that rules or labels it.
    updateView();
    for (QWidget *w : QApplication::topLevelWidgets())
        for (StructureStudioView *v : w->findChildren<StructureStudioView *>())
            v->update();
}

void Monitor::slotGridSubdivisionsChanged(int value)
{
    Q_ASSERT(m_graphicsView != NULL);

    m_graphicsView->setGridSubdivisions(value);
    m_props->setGridSubdivisions(value);
    m_doc->setModified();
}

void Monitor::slotSnapChanged(int index)
{
    Q_ASSERT(m_graphicsView != NULL);

    int divisions = 0;
    QVariant var = m_snapCombo->itemData(index);
    if (var.isValid())
        divisions = var.toInt();

    m_graphicsView->setSnapDivisions(divisions);
    m_props->setSnapDivisions(divisions);
    m_doc->setModified();
}

void Monitor::slotMapViewChanged(int)
{
    // The overlay is now chosen from the "More ▸ Overlay" submenu, which sets
    // m_mapView and calls applyMapView() directly. Kept for any external callers.
    applyMapView(m_mapView);
    updateModeIndicator();
}

void Monitor::applyMapView(int view)
{
    if (m_graphicsView == NULL)
        return;

    // "Stage only" hides fixtures/targets/power (handled in the view). It clears
    // colour overlays too (falls through to the ViewNormal branch below).
    m_graphicsView->setStageFeaturesOnly(view == ViewStage);

    // The Network overlay also draws each port's cable run, node -> fixtures in
    // address order, with its stats on hover.
    m_graphicsView->setNetworkRunsVisible(view == ViewNet);

    if (view == ViewPower)
    {
        PowerDistribution *pd = m_doc->powerDistribution();

        // Give each circuit a distinct hue (flat index across all sources).
        int total = 0;
        foreach (const PowerSource &src, pd->sources())
            total += src.circuits.size();
        QHash<QString, QColor> circuitColor;
        int idx = 0;
        for (int s = 0; s < pd->sources().size(); s++)
            for (int c = 0; c < pd->sources().at(s).circuits.size(); c++)
            {
                const int hue = int(idx * 360.0 / qMax(1, total)) % 360;
                circuitColor.insert(QString("%1:%2").arg(s).arg(c),
                                    QColor::fromHsv(hue, 200, 235));
                idx++;
            }

        foreach (Fixture *fx, m_doc->fixtures())
        {
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it == NULL)
                continue;
            int s = -1, c = -1;
            pd->circuitOf(fx->id(), s, c);
            if (s >= 0 && c >= 0)
            {
                it->setViewTint(circuitColor.value(QString("%1:%2").arg(s).arg(c),
                                                   QColor(120, 120, 120)));
                it->setToolTip(tr("Power: %1 / %2")
                               .arg(pd->sources().at(s).name)
                               .arg(pd->sources().at(s).circuits.at(c).name));
            }
            else
            {
                it->setViewTint(QColor(70, 70, 70));   // unassigned = dim grey
                it->setToolTip(tr("Power: unassigned to a circuit"));
            }
        }
        // Tint the source markers with their circuits' colours so a source and
        // the fixtures on its circuits read as the same colour.
        m_graphicsView->setPowerSourceColors(circuitColor);
    }
    else if (view == ViewDMX)
    {
        const QList<Fixture *> fixtures = m_doc->fixtures();

        // A distinct hue per DMX universe — fixtures sharing a colour share a
        // universe (a DMX output/run), so cabling reads at a glance.
        QList<quint32> universes;
        foreach (Fixture *fx, fixtures)
            if (fx != NULL && fx->isPatched() && !universes.contains(fx->universe()))
                universes.append(fx->universe());
        std::sort(universes.begin(), universes.end());
        QHash<quint32, QColor> uniColor;
        QHash<quint32, QString> uniXport;   // universe → transport (ArtNet/DMX/…)
        InputOutputMap *iom = m_doc->inputOutputMap();
        for (int i = 0; i < universes.size(); i++)
        {
            uniColor.insert(universes[i],
                QColor::fromHsv(int(i * 360.0 / qMax(1, universes.size())) % 360, 200, 235));
            OutputPatch *op = iom ? iom->outputPatch(universes[i], 0) : NULL;
            uniXport.insert(universes[i], op ? op->pluginName() : QString());
        }

        // Flag fixtures whose address ranges overlap on the SAME universe.
        QSet<quint32> conflicted;
        QHash<quint32, quint32> conflictWith;
        for (int i = 0; i < fixtures.size(); i++)
        {
            Fixture *a = fixtures.at(i);
            if (a == NULL || !a->isPatched()) continue;
            const quint32 aS = a->address(), aE = a->address() + a->channels();
            for (int j = i + 1; j < fixtures.size(); j++)
            {
                Fixture *b = fixtures.at(j);
                if (b == NULL || !b->isPatched() || a->universe() != b->universe()) continue;
                const quint32 bS = b->address(), bE = b->address() + b->channels();
                if (aS < bE && bS < aE)   // ranges overlap → address clash
                {
                    conflicted.insert(a->id()); conflicted.insert(b->id());
                    conflictWith.insert(a->id(), b->id());
                    conflictWith.insert(b->id(), a->id());
                }
            }
        }

        foreach (Fixture *fx, fixtures)
        {
            if (fx == NULL) continue;
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it == NULL) continue;
            if (!fx->isPatched())
            {
                it->setViewTint(QColor(70, 70, 70));
                it->setToolTip(tr("Unpatched (inventory)"));
                continue;
            }
            const quint32 uni = fx->universe() + 1;        // 1-based for display
            const quint32 a0 = fx->address() + 1;
            const quint32 a1 = fx->address() + fx->channels();
            const QString xport = uniXport.value(fx->universe());
            const QString via = xport.isEmpty() ? QString()
                                                : QStringLiteral(" [%1]").arg(xport);
            if (conflicted.contains(fx->id()))
            {
                it->setViewTint(QColor(210, 60, 60));       // clash = red
                Fixture *other = m_doc->fixture(conflictWith.value(fx->id()));
                it->setToolTip(tr("Universe %1%2 · A %3–%4 · ⚠ address clash with %5")
                               .arg(uni).arg(via).arg(a0).arg(a1)
                               .arg(other ? other->name() : tr("another fixture")));
            }
            else
            {
                it->setViewTint(uniColor.value(fx->universe(), QColor(120, 120, 120)));
                it->setToolTip(tr("Universe %1%2 · A %3–%4 (%5 ch)")
                               .arg(uni).arg(via).arg(a0).arg(a1).arg(fx->channels()));
            }
        }
        m_graphicsView->setPowerSourceColors(QHash<QString, QColor>());
    }
    else if (view == ViewNet)
    {
        // Colour fixtures by their universe's TRANSPORT (ArtNet / sACN / a DMX
        // interface / unpatched), so you see which parts of the rig are on the
        // network vs a physical DMX line.
        InputOutputMap *iom = m_doc->inputOutputMap();
        auto transportOf = [iom](quint32 universe) -> QString {
            OutputPatch *op = iom ? iom->outputPatch(universe, 0) : NULL;
            const QString p = op ? op->pluginName() : QString();
            return p.isEmpty() ? tr("(unpatched)") : p;
        };

        // Distinct hue per distinct transport name.
        QStringList transports;
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx != NULL && fx->isPatched())
            {
                const QString t = transportOf(fx->universe());
                if (!transports.contains(t)) transports.append(t);
            }
        transports.sort();
        QHash<QString, QColor> xportColor;
        for (int i = 0; i < transports.size(); i++)
            xportColor.insert(transports[i],
                QColor::fromHsv(int(i * 360.0 / qMax(1, transports.size())) % 360, 190, 235));

        // With a topology defined, a DMX fixture on a universe no port carries
        // has no physical path to the console -- say so. (With no nodes at all
        // every fixture would be flagged, which is noise, not information.)
        const NetworkTopology *nt = m_doc->networkTopology();
        const QList<quint32> unservedList = nt->nodes().isEmpty() ? QList<quint32>()
                                                                  : nt->unservedFixtures(m_doc);
        const QSet<quint32> unserved(unservedList.begin(), unservedList.end());

        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx == NULL) continue;
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it == NULL) continue;
            if (!fx->isPatched())
            {
                it->setViewTint(QColor(70, 70, 70));
                it->setToolTip(tr("Unpatched (inventory)"));
                continue;
            }
            const QString t = transportOf(fx->universe());
            it->setViewTint(xportColor.value(t, QColor(120, 120, 120)));
            it->setToolTip(tr("%1 · Universe %2").arg(t).arg(fx->universe() + 1));
            if (unserved.contains(fx->id()))
            {
                it->setViewTint(QColor(230, 110, 60));
                it->setToolTip(tr("%1 · Universe %2 · ⚠ no node port carries this universe -- "
                                  "no cable run reaches it").arg(t).arg(fx->universe() + 1));
            }
        }
        m_graphicsView->setPowerSourceColors(QHash<QString, QColor>());
    }
    else // ViewNormal: clear overlays
    {
        foreach (Fixture *fx, m_doc->fixtures())
        {
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it != NULL)
            {
                it->setViewTint(QColor());
                it->restoreBaseToolTip();   // hover always shows name/manuf/model/addr
            }
        }
        m_graphicsView->setPowerSourceColors(QHash<QString, QColor>());
    }
}

void Monitor::slotLockToggled(bool locked)
{
    Q_ASSERT(m_graphicsView != NULL);

    m_graphicsView->setLayoutLocked(locked);
    updatePlotLockAppearance(locked);
    m_props->setLayoutLocked(locked);
    m_doc->setModified();
}

void Monitor::slotToggleLayersPanel(bool show)
{
    if (m_layersPanel == NULL)
        return;
    if (show)
        m_layersPanel->reload();   // resync in case layers changed while hidden
    m_layersPanel->setVisible(show);
    if (show && m_splitter != NULL)
    {
        // Give the panel a real width when revealed (a splitter state saved
        // while it was hidden would otherwise leave it at zero px). Panel is the
        // left pane (index 0).
        const int total = qMax(600, m_splitter->width());
        m_splitter->setSizes(QList<int>() << 260 << (total - 260));
    }
}

void Monitor::toggleStageOverview()
{
    if (m_overviewDlg != NULL && m_overviewDlg->isVisible() && m_overviewDlg->isActiveWindow())
    {
        m_overviewDlg->close();
        return;
    }
    showStageOverview();
}

void Monitor::showStageOverview()
{
    /* A read-only angled look at the whole rig. StructureStudioView already
       knows how to project and paint a structure and the fixtures on it;
       StageKind points it at every structure instead of one, so there is a
       single renderer to keep correct rather than two that drift apart. */
    /* MODELESS, and remembered: it is a reference window you keep open beside
       the work, not a question to answer. exec() made it modal, which locked
       the rest of the app -- "when in the overview I can't go away from the
       studio". Reusing the one instance also means a second click raises the
       window you already have instead of stacking another. */
    if (m_overviewDlg != NULL)
    {
        if (m_overviewView != NULL)
            m_overviewView->setActiveScene(m_activeSceneId);
        m_overviewDlg->show();
        m_overviewDlg->raise();
        m_overviewDlg->activateWindow();
        return;
    }

    QDialog *dlgPtr = new QDialog(this);
    QDialog &dlg = *dlgPtr;
    m_overviewDlg = dlgPtr;
    connect(dlgPtr, &QObject::destroyed, this, [this]() {
        m_overviewDlg = NULL;
        m_overviewView = NULL;
    });
    dlg.setAttribute(Qt::WA_DeleteOnClose);
    dlg.setWindowTitle(tr("Rig Overview"));
    dlg.setProperty("helpTopic", QStringLiteral("lighting-studio.md#angled--the-free-3d-view"));
    // ⌘W closes it like any other window; ⇧⌘R (toggleStageOverview()) too.
    QShortcut *closeKey = new QShortcut(QKeySequence::Close, dlgPtr);
    connect(closeKey, &QShortcut::activated, dlgPtr, &QDialog::close);
    dlg.resize(1100, 760);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);

    StructureStudioView *view =
        new StructureStudioView(m_doc, StructureStudioView::StageKind, 0, &dlg);
    view->setPlane(StructureStudioView::Angled);
    // Stand on the floor at centre stage and turn about it.
    view->setStandOnCentreStage(true);
    view->setLocked(true);
    m_overviewView = view;
    view->setActiveScene(m_activeSceneId);
    /* Opened in the middle of Focus mode: ring the head being focused, as the
       Studio already does -- it was only ever told when the head CHANGED. */
    if (m_doc->programmer() != NULL && m_doc->programmer()->focusSession() != NULL
        && m_doc->programmer()->focusSession()->isActive())
        view->setHighlight({ m_doc->programmer()->focusSession()->currentFixture() });

    /* Read-only, but a fixture can still be looked into: its properties,
       and where each of its heads is (the LED layout). */
    connect(view, &StructureStudioView::canvasContextMenu, &dlg,
            [this, ovDlg = &dlg](const QPoint &globalPos, quint32 fidUnder) {
        if (m_doc->fixture(fidUnder) == nullptr)
            return;
        QMenu m(ovDlg);
        m.addAction(tr("Fixture Properties…"), this, [this, fidUnder]() {
            editFixtureProperties(fidUnder);
        });
        QAction *led = m.addAction(tr("LED Layout…"), this, [this, fidUnder, ovDlg]() {
            LedLayoutDialog::edit(m_doc, fidUnder, ovDlg);
        });
        led->setObjectName(QStringLiteral("ledLayoutAction"));
        led->setEnabled(LedLayoutDialog::canEdit(m_doc, fidUnder));
        m.exec(globalPos);
    });

    QHBoxLayout *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("View:"), &dlg));
    QComboBox *planeCombo = new QComboBox(&dlg);
    planeCombo->addItems({ tr("Top"), tr("Front"), tr("Side"), tr("45°") });
    planeCombo->setCurrentIndex(int(StructureStudioView::Angled));
    planeCombo->setProperty("cycleOnClick", true);   // same as the plot's View
    planeCombo->installEventFilter(this);
    planeCombo->setToolTip(tr("Click to cycle, arrow for the full list"));
    bar->addWidget(planeCombo);
    connect(planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [view](int i) { view->setPlane(StructureStudioView::Plane(i)); });
    /* Live output: the whole point of watching a rig rather than reading a
       plot. Default ON when the desk is running, so opening the window during a
       show shows the show. */
    /* Checked state has to be UNMISTAKABLE. A checkable QToolButton on macOS
       renders its on state as a barely-different grey, and "why can't I see the
       beams" turned out to be exactly that: beam brightness comes from DMX, so
       with Live off there is nothing to draw, and nothing on screen said Live
       was off. */
    const QString toggleStyle =
        "QToolButton { padding: 2px 8px; border: 1px solid #4a4f5a;"
        " border-radius: 3px; }"
        "QToolButton:checked { background: #2d6cdf; color: white;"
        " border: 1px solid #5b8ee8; }";

    QToolButton *liveBtn = new QToolButton(&dlg);
    liveBtn->setText(tr("Live"));
    liveBtn->setCheckable(true);
    liveBtn->setStyleSheet(toggleStyle);
    liveBtn->setToolTip(tr("Colour and brightness from the DMX being sent, "
                           "instead of the fixtures' gel colours. Beams need "
                           "this: how bright a beam is comes from the DMX."));
    bar->addWidget(liveBtn);
    connect(liveBtn, &QToolButton::toggled, view,
            [view](bool on) { view->setLiveValues(on); });
    /* On by default, not only in Operate. Opening the window while a scene is
       up and seeing nothing change is the wrong first impression -- and with
       the room level applied, an idle rig still reads perfectly well. */
    liveBtn->setChecked(true);

    /* Beams. Their own switch because they are the one thing here that draws
       over the rig rather than in it: unmissable when you want to see what a
       look is doing in the air, in the way when you are reading the plot. */
    QToolButton *beamBtn = new QToolButton(&dlg);
    beamBtn->setText(tr("Beams"));
    beamBtn->setCheckable(true);
    beamBtn->setStyleSheet(toggleStyle);
    beamBtn->setToolTip(tr("Draw the light moving heads are throwing, out to "
                           "whatever it lands on"));
    bar->addWidget(beamBtn);
    connect(beamBtn, &QToolButton::toggled, view,
            [view](bool on) { view->setBeams(on); });
    beamBtn->setChecked(true);

    /* How much light is in the ROOM. Without it the rendered colours are as
       vivid as a test card whatever the rig is doing, and a blackout still
       shows bright scenery. */
    bar->addWidget(new QLabel(tr("Room:"), &dlg));
    QComboBox *ambientCombo = new QComboBox(&dlg);
    ambientCombo->addItem(tr("Work light"), 1.00);
    ambientCombo->addItem(tr("House"),      0.62);
    ambientCombo->addItem(tr("Dim"),        0.30);
    ambientCombo->addItem(tr("Blackout"),   0.00);
    ambientCombo->setCurrentIndex(1);
    ambientCombo->setProperty("cycleOnClick", true);
    ambientCombo->installEventFilter(this);
    ambientCombo->setToolTip(tr("Ambient light in the room, separate from what "
                                "the rig is emitting — click to cycle"));
    bar->addWidget(ambientCombo);
    connect(ambientCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [view, ambientCombo](int) {
        view->setAmbient(ambientCombo->currentData().toDouble());
    });
    view->setAmbient(ambientCombo->currentData().toDouble());

    /* Haze: whether the beams show IN THE AIR. Only the shafts -- where the
       light lands and what it lights do not change with it. */
    bar->addWidget(new QLabel(tr("Haze:"), &dlg));
    QComboBox *hazeCombo = new QComboBox(&dlg);
    hazeCombo->addItem(tr("None"),   0.0);
    hazeCombo->addItem(tr("Light"),  0.4);
    hazeCombo->addItem(tr("Medium"), 1.0);
    hazeCombo->addItem(tr("Heavy"),  1.8);
    hazeCombo->setCurrentIndex(2);
    hazeCombo->setProperty("cycleOnClick", true);
    hazeCombo->installEventFilter(this);
    hazeCombo->setToolTip(tr("How much haze is in the air: how visible the "
                             "beams are between the fixture and where they "
                             "land — click to cycle"));
    bar->addWidget(hazeCombo);
    connect(hazeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [view, hazeCombo](int) {
        view->setHaze(hazeCombo->currentData().toDouble());
    });
    view->setHaze(hazeCombo->currentData().toDouble());

    /* Fidelity against speed, for the machine this is running on: sharper
       edges cost fill, and the rate is only a ceiling -- a frame that takes
       longer than the rate allows still slows the view down on its own. Both
       are remembered. */
    bar->addWidget(new QLabel(tr("Quality:"), &dlg));
    QComboBox *qualityCombo = new QComboBox(&dlg);
    qualityCombo->addItem(tr("Fast"),     1);
    qualityCombo->addItem(tr("Smooth"),   2);
    qualityCombo->addItem(tr("Sharp"),    3);
    qualityCombo->addItem(tr("Sharpest"), 4);
    qualityCombo->setCurrentIndex(qMax(0, qualityCombo->findData(view->supersample())));
    qualityCombo->setToolTip(tr("Edge smoothing: Fast draws at screen resolution, "
                                "Sharpest at four times it in each direction"));
    bar->addWidget(qualityCombo);
    connect(qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [view, qualityCombo](int) {
        view->setSupersample(qualityCombo->currentData().toInt());
    });

    /* Layers, from here: show or hide one without the Studio open. The same
       switch as the Studio's eye (saved with the show), so the two agree --
       and in any mode, since it is how you look at the rig, not a change to
       it. Rebuilt each time it opens, so it is the show's layers as they are. */
    QToolButton *layersBtn = new QToolButton(&dlg);
    layersBtn->setObjectName(QStringLiteral("rigLayers"));
    layersBtn->setText(tr("Layers"));
    layersBtn->setToolTip(tr("Show or hide a layer (the same as its eye in the Studio)"));
    layersBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *layersMenu = new QMenu(layersBtn);
    layersBtn->setMenu(layersMenu);
    connect(layersMenu, &QMenu::aboutToShow, layersMenu, [this, layersMenu]() {
        layersMenu->clear();
        MonitorProperties *props = m_doc->monitorProperties();
        auto setShown = [this, props](quint32 id, bool on) {
            props->setLayerVisible(id, on);
            m_doc->setModified();
            if (m_graphicsView != NULL)
                m_graphicsView->refreshItemLayerState();   // -> this view, the Studio's list
        };
        const QList<MonitorProperties::MonitorLayer> layers = props->layers();
        for (const MonitorProperties::MonitorLayer &lyr : layers)
        {
            QAction *a = layersMenu->addAction(lyr.name);
            a->setCheckable(true);
            a->setChecked(lyr.visible);
            const quint32 id = lyr.id;
            connect(a, &QAction::toggled, layersMenu, [setShown, id](bool on) { setShown(id, on); });
        }
        layersMenu->addSeparator();
        QAction *all = layersMenu->addAction(tr("Show all"));
        connect(all, &QAction::triggered, layersMenu, [setShown, layers]() {
            for (const MonitorProperties::MonitorLayer &lyr : layers)
                if (!lyr.visible)
                    setShown(lyr.id, true);
        });
    });
    bar->addWidget(layersBtn);

    /* On the graphics card when there is one (RhiRaster): the same picture,
       drawn there instead of on the CPU. Off falls back to the CPU renderer,
       for comparing or for a driver that misbehaves. */
    QCheckBox *gpuCheck = new QCheckBox(tr("GPU"), &dlg);
    gpuCheck->setObjectName(QStringLiteral("rigGpu"));
    gpuCheck->setEnabled(RhiRaster::available());
    gpuCheck->setChecked(view->gpuActive());
    gpuCheck->setToolTip(RhiRaster::available()
        ? tr("Draw the rig on the graphics card. Off draws it on the CPU, "
             "the same picture, more slowly.")
        : tr("No graphics card this view can draw on: the rig is drawn on "
             "the CPU."));
    bar->addWidget(gpuCheck);
    connect(gpuCheck, &QCheckBox::toggled, &dlg, [view](bool on) { view->setGpu(on); });

    QComboBox *rateCombo = new QComboBox(&dlg);
    for (int fps : { 25, 30, 60, 120 })
        rateCombo->addItem(tr("%1 fps").arg(fps), fps);
    rateCombo->setCurrentIndex(qMax(0, rateCombo->findData(view->maxFrameRate())));
    rateCombo->setToolTip(tr("Most live repaints per second. A heavy frame "
                             "still slows the view below this on its own."));
    bar->addWidget(rateCombo);
    connect(rateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [view, rateCombo](int) {
        view->setMaxFrameRate(rateCombo->currentData().toInt());
    });

    /* Focus mode from here too: the Overview is where you watch the beams
       land while you align them. */
    QToolButton *focusBtn = new QToolButton(&dlg);
    focusBtn->setObjectName(QStringLiteral("rigFocus"));
    focusBtn->setText(tr("Focus"));
    focusBtn->setToolTip(tr("Focus mode: aim each moving head at its reference in turn and "
                            "nudge its calibration offsets"));
    bar->addWidget(focusBtn);
    connect(focusBtn, &QToolButton::clicked, this, &Monitor::openFocusMode);

    bar->addStretch();
    bar->addWidget(new QLabel(tr("drag to swing · shift-drag to pan · "
                                 "wheel to zoom · read-only"), &dlg));
    vl->addLayout(bar);
    vl->addWidget(view, 1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::close);
    vl->addWidget(bb);

    /* Follow the rig while it is open: structures and fixtures move under it as
       the plot is edited, and a stale overview is worse than none. */
    connect(m_graphicsView, &MonitorGraphicsView::mapSelectionChanged, view,
            [view]() { view->reload(); });
    // A layer hidden in the Studio is hidden here too.
    connect(m_graphicsView, &MonitorGraphicsView::layerStateChanged, view,
            &StructureStudioView::layersChanged);

    dlg.show();
}

void Monitor::slotPOVChanged(int index)
{
    if (m_graphicsView == NULL || m_povCombo == NULL)
        return;
    const int data = m_povCombo->itemData(index).toInt();

    if (data < 0)
    {
        // "DMX" entry: switch to the channel-grid view.
        m_props->setDisplayMode(MonitorProperties::DMX);
        showCurrentView();
        return;
    }

    m_props->setDisplayMode(MonitorProperties::Graphics);
    m_graphicsView->setViewPOV(MonitorGraphicsView::ViewPOV(data));

    // Elevation views are read-only: the plot-lock and snap controls don't apply.
    const bool elevation = (data != int(MonitorGraphicsView::PovTop));
    if (m_lockAction != NULL) m_lockAction->setEnabled(!elevation);
    if (m_snapCombo != NULL)  m_snapCombo->setEnabled(!elevation);
    updateModeIndicator();
}

void Monitor::slotGroupItems()
{
    if (m_graphicsView != NULL)
        m_graphicsView->groupSelectedItems();
}

void Monitor::slotRotateSelection()
{
    if (m_graphicsView == NULL || m_graphicsView->scene()->selectedItems().isEmpty())
        return;
    bool ok = false;
    const double deg = QInputDialog::getDouble(this, tr("Rotate"),
                                               tr("Turn the selection clockwise (seen from above) by, degrees "
                                                  "-- less than 0 turns it the other way:"),
                                               15.0, -360.0, 360.0, 1, &ok);
    if (ok)
        m_graphicsView->rotateSelectedItems(deg);
}

void Monitor::slotUngroupItems()
{
    if (m_graphicsView != NULL)
        m_graphicsView->ungroupSelectedItems();
}

void Monitor::slotMapSelectionChanged()
{
    if (m_graphicsView == NULL)
        return;
    if (m_groupAction != NULL)
        m_groupAction->setEnabled(m_graphicsView->selectionGroupable());
    if (m_ungroupAction != NULL)
        m_ungroupAction->setEnabled(m_graphicsView->selectionHasGroup());
}

void Monitor::updatePlotLockAppearance(bool locked)
{
    if (m_lockAction == NULL)
        return;

    m_lockAction->setIcon(QIcon(locked ? ":/lock.png" : ":/unlock.png"));
    // The toggle flips between editing the plot and freezing it for the show.
    m_lockAction->setText(locked ? tr("Plot Locked") : tr("Edit Plot"));
    m_lockAction->setToolTip(locked
        ? tr("Plot locked — the rig is frozen for the show.\n"
             "Fixtures can be selected but not moved, and facing arrows are hidden;\n"
             "aim targets stay adjustable. Click to edit the plot.")
        : tr("Editing the plot — arrange & aim the rig.\n"
             "Drag a fixture to move it; Alt-drag a moving head to rotate its facing.\n"
             "Click to lock the plot for the show."));
}

void Monitor::slotAddFixture()
{
    Q_ASSERT(m_graphicsView != NULL);

    if (m_props->layoutLocked())
    {
        QMessageBox::information(this, tr("Plot locked"),
            tr("Unlock the plot (Edit Plot) before adding fixtures — a fixture "
               "added to a locked plot can't be positioned."));
        return;
    }

    QList <quint32> disabled = m_graphicsView->fixturesID();
    /* Get a list of new fixtures to add to the scene */
    FixtureSelection fs(this, m_doc);
    fs.setMultiSelection(true);
    fs.setDisabledFixtures(disabled);
    if (fs.exec() == QDialog::Accepted)
    {
        // Convert pending scene-px position to mm (use 0,0 when added from toolbar)
        QPointF mm = m_pendingAddScenePos.isNull()
                     ? QPointF(0, 0)
                     : m_graphicsView->pixelsToRealPosition(
                           m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
        m_pendingAddScenePos = QPointF();  // clear after use

        QListIterator <quint32> it(fs.selection());
        while (it.hasNext() == true)
        {
            quint32 fid = it.next();
            m_graphicsView->addFixture(fid, mm);
            m_props->setFixturePosition(fid, 0, 0, QVector3D(mm.x(), mm.y(), 0));
            m_props->setFixtureFlags(fid, 0, 0, 0);
            m_props->setFixtureLayer(fid, m_props->activeLayerId());  // land on the selected layer
            m_doc->setModified();
        }
    }
    if (m_layersPanel) m_layersPanel->reload();
    m_graphicsView->refreshFixtureLabels();   // apply per-layer labels to the new item
}

void Monitor::slotRemoveFixture()
{
    Q_ASSERT(m_graphicsView != NULL);

    hideFixtureItemEditor();
    if (m_graphicsView->removeFixture() == true)
        m_doc->setModified();
}

void Monitor::slotSetBackground()
{
    Q_ASSERT(m_graphicsView != NULL);

    MonitorBackgroundSelection mbgs(this, m_doc);

    if (mbgs.exec() == QDialog::Accepted)
    {
        if (m_props->commonBackgroundImage().isEmpty() == false)
            m_graphicsView->setBackgroundImage(m_props->commonBackgroundImage());
        else
            m_graphicsView->setBackgroundImage(QString());

        m_doc->setModified();
    }
}

void Monitor::slotSetBackgroundColor()
{
    Q_ASSERT(m_graphicsView != NULL);

    QColor initial = m_props->commonBackgroundColor();
    if (!initial.isValid())
        initial = Qt::darkGray;

    QColor c = QColorDialog::getColor(initial, this, tr("Set background colour"),
                                      QColorDialog::ShowAlphaChannel);
    if (!c.isValid())
        return;

    m_props->setCommonBackgroundColor(c);
    m_graphicsView->setBackgroundColor(c);
    m_doc->setModified();
}

void Monitor::slotRemoveSelected()
{
    Q_ASSERT(m_graphicsView != NULL);

    // Gather EVERY selected structural feature, by type (was: truss/platform/
    // target only — booms, stands and towers fell through and silently no-op'd).
    QList<quint32> trussIds, platIds, pipeIds, standIds, towerIds, targetIds, imageIds;
    foreach (QGraphicsItem *gi, m_graphicsView->scene()->selectedItems())
    {
        if (auto *ti  = dynamic_cast<TrussItem *>(gi))        trussIds  << ti->trussId();
        else if (auto *pi  = dynamic_cast<PlatformItem *>(gi)) platIds  << pi->platformId();
        else if (auto *bi  = dynamic_cast<PipeItem *>(gi))     pipeIds  << bi->pipeId();
        else if (auto *si  = dynamic_cast<StandItem *>(gi))    standIds << si->standId();
        else if (auto *twi = dynamic_cast<TowerItem *>(gi))    towerIds << twi->towerId();
        else if (auto *tgi = dynamic_cast<TargetItem *>(gi))   targetIds << tgi->targetId();
        else if (auto *ii  = dynamic_cast<MonitorImageItem *>(gi)) imageIds << ii->imageId();
    }
    const int nFeat = trussIds.size() + platIds.size() + pipeIds.size()
                    + standIds.size() + towerIds.size() + targetIds.size() + imageIds.size();

    // Nothing structural selected → fall back to fixture remove (unchanged).
    if (nFeat == 0)
    {
        hideFixtureItemEditor();
        if (m_graphicsView->removeFixture())
            m_doc->setModified();
        return;
    }

    // Exactly one feature → reuse the graceful per-feature slot (name + attached-
    // fixture handling). Targets/images have no attached fixtures.
    if (nFeat == 1)
    {
        if (!trussIds.isEmpty())       slotTrussRemoveRequested(trussIds.first());
        else if (!platIds.isEmpty())   slotPlatformRemoveRequested(platIds.first());
        else if (!pipeIds.isEmpty())   slotPipeRemoveRequested(pipeIds.first());
        else if (!standIds.isEmpty())  slotStandRemoveRequested(standIds.first());
        else if (!towerIds.isEmpty())  slotTowerRemoveRequested(towerIds.first());
        else if (!imageIds.isEmpty())  slotImageRemoveRequested(imageIds.first());
        else if (!targetIds.isEmpty())
        {
            const quint32 tid = targetIds.first();
            StageTarget *tg = m_props->stageTarget(tid);
            if (confirmFeatureDelete(tr("Target"), tg ? tg->name() : QString(),
                                     QList<quint32>()) != DeleteCancel)
            {
                m_props->removeStageTarget(tid);
                m_graphicsView->updateTargets();
                m_doc->setModified();
            }
        }
        return;
    }

    // Multiple features → one combined confirm summarising the attached fixtures.
    QList<quint32> fids;
    foreach (quint32 id, trussIds) fids += fixturesOnFeature(TrussFeature, id);
    foreach (quint32 id, pipeIds)  fids += fixturesOnFeature(PipeFeature, id);
    foreach (quint32 id, towerIds) fids += fixturesOnFeature(TowerFeature, id);
    foreach (quint32 id, platIds)  fids += fixturesOnFeature(PlatformFeature, id);
    fids = QSet<quint32>(fids.begin(), fids.end()).values();   // de-dup

    const FeatureDeleteChoice choice = confirmFeatureDelete(
        tr("%n selected feature(s)", "", nFeat), QString(), fids);
    if (choice == DeleteCancel)
        return;
    if (choice == DeleteWithFixtures)
    {
        foreach (quint32 fid, fids) m_graphicsView->removeFixture(fid);
    }

    foreach (quint32 id, trussIds)  m_props->removeTruss(id);
    foreach (quint32 id, platIds)   m_props->removePlatform(id);
    foreach (quint32 id, pipeIds)   m_props->removePipe(id);
    foreach (quint32 id, standIds)  m_props->removeStand(id);
    foreach (quint32 id, towerIds)  m_props->removeTower(id);
    foreach (quint32 id, targetIds) m_props->removeStageTarget(id);
    foreach (quint32 id, imageIds)  m_props->removeImage(id);

    m_props->recomputeStandMounts();
    m_graphicsView->updateTrusses();
    m_graphicsView->updatePlatforms();
    m_graphicsView->updateImages();
    m_graphicsView->updateTargets();
    m_graphicsView->refreshFixtureBindings();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

bool Monitor::clipboardHasFeatures() const
{
    return !m_trussClipboard.isEmpty()
        || !m_platformClipboard.isEmpty()
        || !m_targetClipboard.isEmpty();
}

void Monitor::slotCopySelected()
{
    QList<TrussClip>    trusses;
    QList<PlatformClip> platforms;
    QList<TargetClip>   targets;

    foreach (QGraphicsItem *gi, m_graphicsView->scene()->selectedItems())
    {
        if (TrussItem *ti = dynamic_cast<TrussItem *>(gi))
        {
            Truss *t = ti->truss();
            TrussClip c;
            c.name = t->name();   c.type = int(t->type());
            c.origin = t->origin(); c.direction = t->direction();
            c.length = t->length(); c.width = t->width();
            c.profile = int(t->profile()); c.locked = t->locked();
            trusses.append(c);
        }
        else if (PlatformItem *pi = dynamic_cast<PlatformItem *>(gi))
        {
            StagePlatform *p = pi->platform();
            PlatformClip c;
            c.name = p->name();
            c.originX = p->originX(); c.originY = p->originY();
            c.width = p->width(); c.depth = p->depth(); c.height = p->height();
            c.rotation = p->rotation(); c.frontCurve = p->frontCurve();
            c.color = p->color();
            platforms.append(c);
        }
        else if (TargetItem *tg = dynamic_cast<TargetItem *>(gi))
        {
            StageTarget *t = tg->target();
            TargetClip c;
            c.name = t->name();
            c.x = t->x(); c.y = t->y(); c.z = t->z();
            c.color = t->color();
            c.kind = int(t->kind());
            c.linePoints = t->linePoints();
            targets.append(c);
        }
    }

    if (trusses.isEmpty() && platforms.isEmpty() && targets.isEmpty())
        return;   // nothing copyable selected: keep any previous clipboard

    m_trussClipboard    = trusses;
    m_platformClipboard = platforms;
    m_targetClipboard   = targets;
    m_pasteCount        = 0;

    if (m_pasteAction)
        m_pasteAction->setEnabled(true);
}

void Monitor::pasteClipboard(float dxM, float dyM)
{
    if (!clipboardHasFeatures())
        return;

    QList<quint32> newTrussIds, newPlatformIds, newTargetIds, newPaletteIds;

    foreach (const TrussClip &c, m_trussClipboard)
    {
        Truss *t = m_props->addTruss();
        t->setName(c.name + tr(" copy"));
        t->setType(Truss::TrussType(c.type));
        t->setOrigin(c.origin + QVector3D(dxM, dyM, 0.0f));
        t->setDirection(c.direction);
        t->setLength(c.length);
        t->setWidth(c.width);
        t->setProfile(Truss::Profile(c.profile));
        t->setLocked(false);   // a duplicate must be movable, even if the source was locked
        t->setLayerId(m_props->activeLayerId());
        newTrussIds.append(t->id());
    }

    foreach (const PlatformClip &c, m_platformClipboard)
    {
        StagePlatform *p = m_props->addPlatform();
        p->setName(c.name + tr(" copy"));
        p->setOriginX(c.originX + dxM);
        p->setOriginY(c.originY + dyM);
        p->setWidth(c.width);
        p->setDepth(c.depth);
        p->setHeight(c.height);
        p->setRotation(c.rotation);
        p->setFrontCurve(c.frontCurve);
        p->setColor(c.color);
        p->setLayerId(m_props->activeLayerId());
        newPlatformIds.append(p->id());
    }

    foreach (const TargetClip &c, m_targetClipboard)
    {
        StageTarget *t = m_props->addStageTarget();
        t->setName(c.name + tr(" copy"));
        t->setX(c.x + dxM);
        t->setY(c.y + dyM);
        t->setZ(c.z);
        t->setColor(c.color);
        // A pasted line is the same line, not a point at its centre.
        if (StageTarget::Kind(c.kind) == StageTarget::Kind::Line)
        {
            t->setLinePoints(c.linePoints);
            t->makeLine();
        }
        newTargetIds.append(t->id());

        // Mirror slotAddTarget: give the copy its own linked PanTilt palette so
        // it is immediately usable rather than a dangling position.
        QLCPalette *pal = new QLCPalette(QLCPalette::PanTilt);
        pal->setName(t->name());
        pal->setValue(0, 0);
        pal->setStageTargetId(t->id());
        m_doc->addPalette(pal);
        newPaletteIds.append(pal->id());
    }

    m_graphicsView->updateTrusses();
    m_graphicsView->updatePlatforms();
    m_graphicsView->updateTargets();
    if (m_layersPanel) m_layersPanel->reload();   // show the copies in the tree
    m_doc->setModified();

    // Register the paste on the canvas undo stack so Ctrl+Z removes the copies.
    m_graphicsView->recordFeaturePaste(newTrussIds, newPlatformIds,
                                       newTargetIds, newPaletteIds);
}

void Monitor::slotPasteFeatures()
{
    if (!clipboardHasFeatures())
        return;

    // Cascade each successive paste so repeated Ctrl+V copies don't stack
    // exactly on top of one another.
    ++m_pasteCount;
    const float step = 0.5f * float(m_pasteCount);   // metres
    pasteClipboard(step, step);
}

void Monitor::slotFixtureDoubleClicked(quint32 fid)
{
    // Map double-click: edit whatever is selected (single OR the multi-selection
    // the map restored) — keeps multi-edit on the 2D plot.
    /* ...unless that selection does not hold THIS fixture. "Edit Fixture…" on
       a Layers row asks for one the plot cannot select at all -- a washer
       inside a step, under its deck -- and with nothing selected the editor
       quietly did nothing ("right click on US1t1 to edit fixture I get
       nothing"). */
    if (fid != Fixture::invalidId())
    {
        bool inSelection = false;
        foreach (MonitorFixtureItem *it, m_graphicsView->selectedFixtureItems())
            if (it != nullptr && it->fixtureID() == fid)
                inSelection = true;
        if (!inSelection)
        {
            editFixtureProperties(fid);
            return;
        }
    }
    showFixtureItemEditor();
}

QString Monitor::centredPoseName(quint32 fid) const
{
    switch (m_props->fixtureRigProps(fid).mountingType)
    {
        case Truss::FloorMounted: return tr("Straight up");
        case Truss::SideArm:      return tr("Straight out");
        default:                  return tr("Straight down");
    }
}

void Monitor::fillOrientationCombo(QComboBox *cb, const FixtureRigProps &rp) const
{
    cb->blockSignals(true);
    cb->clear();
    auto addOpt = [cb](const QString &lbl, int mt, int side) {
        cb->addItem(lbl);
        const int i = cb->count() - 1;
        cb->setItemData(i, mt, Qt::UserRole);
        cb->setItemData(i, side, Qt::UserRole + 1);
    };
    Pipe  *mp = (rp.pipeId  != Pipe::invalidId())  ? m_props->pipe(rp.pipeId)   : nullptr;
    Truss *mt = (rp.trussId != Truss::invalidId()) ? m_props->truss(rp.trussId) : nullptr;
    const bool onTower    = (rp.towerId != Tower::invalidId());
    const bool vertical   = (mp && mp->isVertical()) || (mt && mt->type() == Truss::Vertical);
    const bool horizontal = (mp && !mp->isVertical()) || (mt && mt->type() != Truss::Vertical);
    if (onTower)
    { addOpt(tr("On shelf (upright)"), Truss::FloorMounted, -1);
      addOpt(tr("Under shelf (hung)"), Truss::TopHung, -1); }
    else if (vertical)
    { addOpt(tr("Sideways (clamped)"), Truss::SideArm,      FixtureRigProps::Centered);
      // Exception: a fixture can sit UPRIGHT on the top cap of a tower/vertical
      // truss (a followspot, a base-down mover) — not everything on a vertical
      // is side-clamped.
      addOpt(tr("Upright (on top)"),   Truss::FloorMounted, FixtureRigProps::TopMounted); }
    else if (horizontal)
    { addOpt(tr("On top"),    Truss::FloorMounted, FixtureRigProps::TopMounted);
      addOpt(tr("Underhung"), Truss::TopHung,      FixtureRigProps::UnderHung);
      addOpt(tr("Side arm"),  Truss::SideArm,      FixtureRigProps::Centered); }
    else
    { addOpt(tr("Upright (base down)"),     Truss::FloorMounted, -1);
      addOpt(tr("Sideways (base on side)"), Truss::SideArm,      -1);
      addOpt(tr("Hung (base on top)"),      Truss::TopHung,      -1); }
    int sel = 0;
    for (int i = 0; i < cb->count(); ++i)
        if (cb->itemData(i, Qt::UserRole).toInt() == int(rp.mountingType)) { sel = i; break; }
    cb->setCurrentIndex(sel);
    cb->blockSignals(false);
}

bool Monitor::fixtureIsMounted(quint32 fid) const
{
    const FixtureRigProps &rp = m_props->fixtureRigProps(fid);
    return rp.trussId != Truss::invalidId() || rp.pipeId != Pipe::invalidId()
        || rp.towerId != Tower::invalidId()
        || rp.riserPlatformId != FixtureRigProps::invalidPlatformId()
        || rp.deckPlatformId != FixtureRigProps::invalidPlatformId()
        || m_props->fixtureFrameGroup(fid) != 0;
}

bool Monitor::isLinearFixture(Fixture *fx) const
{
    if (fx == nullptr) return false;
    if (fx->heads() > 1) return true;                 // LED tape / multi-cell bar
    if (const QLCFixtureMode *m = fx->fixtureMode())  // a wide single-head bar/wash
    {
        const qreal w = m->physical().width();
        const qreal d = qMax(m->physical().height(), m->physical().depth());
        if (w > 0 && d > 0 && w > 1.8 * d) return true;
    }
    return false;
}

/* A studio tree row's fixture id. Folder rows never set Qt::UserRole, so a
   plain toUInt() yields 0 -- which is a REAL fixture id (the first fixture in
   any workspace), not a "no fixture" marker. Read it as Fixture::invalidId()
   when the role is unset so fixture 0 behaves like every other fixture. */
static quint32 studioRowFid(const QTreeWidgetItem *it)
{
    const QVariant v = it->data(0, Qt::UserRole);
    return v.isValid() ? v.toUInt() : Fixture::invalidId();
}

/* A studio tree row's BAR (child truss) id, for a truss editor's "Fixtures
   and bars on this object" tree -- see studioRowFid()'s note above for why
   an unset role has to read back as invalid rather than 0. */
static quint32 studioRowBarId(const QTreeWidgetItem *it)
{
    const QVariant v = it->data(0, Qt::UserRole + 2);
    return v.isValid() ? v.toUInt() : Truss::invalidId();
}

void Monitor::editFixtureProperties(quint32 fid)
{
    // The studio's single-fixture path (inspector 'Fixture Properties…', canvas
    // double-click, tree double-click): ALWAYS edit just this one fixture,
    // bypassing the map selection (and its group select-together).
    if (fid != Fixture::invalidId())
        showFixtureItemEditor(fid);
}

void Monitor::slotTrussDoubleClicked(quint32 tid)
{
    slotEditTruss(tid);
}

// A QTreeWidget that hands out fixture ids on drag (for the studio source tree →
// canvas drop). Qt5 passes the item list by value; Qt6 by const-ref.
namespace {
class FixtureSourceTree : public QTreeWidget
{
public:
    explicit FixtureSourceTree(QWidget *parent = nullptr) : QTreeWidget(parent) {}
protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        QMimeData *m = new QMimeData;
        QByteArray b; QDataStream s(&b, QIODevice::WriteOnly);
        foreach (QTreeWidgetItem *it, items)
        {
            const quint32 fid = studioRowFid(it);
            if (fid != Fixture::invalidId()) s << fid;
        }
        m->setData(QStringLiteral("application/x-qlc-fid"), b);
        return m;
    }
};
} // namespace

QWidget *Monitor::makeStudioPane(QDialog *dlg, int kind, quint32 id,
                                 QWidget *geometryForm, StructureStudioView **outView)
{
    // Cmd-W / Ctrl-W closes the editor window (standard, was missing).
    QShortcut *closeSc = new QShortcut(QKeySequence::Close, dlg);
    connect(closeSc, &QShortcut::activated, dlg, &QDialog::reject);

    // The canvas-centric object editor: a splitter of
    //   [ Geometry (collapsible) + fixtures-by-group tree | canvas | inspector ].
    QSplitter *body = new QSplitter(Qt::Horizontal, dlg);

    // ---- LEFT: geometry (collapsible) + the fixture tree -------------------
    QWidget *left = new QWidget(body);
    QVBoxLayout *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    if (geometryForm != nullptr)
    {
        QGroupBox *geoBox = new QGroupBox(tr("Geometry"), left);
        geoBox->setCheckable(true);
        geoBox->setChecked(true);
        QVBoxLayout *gv = new QVBoxLayout(geoBox);
        gv->setContentsMargins(6, 4, 6, 6);
        gv->addWidget(geometryForm);
        connect(geoBox, &QGroupBox::toggled, geometryForm, &QWidget::setVisible);
        lv->addWidget(geoBox);
    }
    lv->addWidget(new QLabel(kind == 2 ? tr("Fixtures and bars on this object:")
                                       : tr("Fixtures on this object:"), left));
    QTreeWidget *tree = new QTreeWidget(left);
    tree->setHeaderHidden(true);
    // Sharing the column with the (often ten-plus-row) Geometry form above it,
    // this tree had no floor: on a modestly sized window it could be squeezed
    // down to one or two visible rows, silently scrolling anything past the
    // first few fixtures -- including a truss's own "Bars" section -- out of
    // view with no visual hint that there was more below the fold ("still no
    // truss .. this time iterate until you can prove its on that screen": it
    // was there, just scrolled off). A floor forces the SPLITTER/dialog to
    // give up space instead of the list quietly disappearing.
    tree->setMinimumHeight(160);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // F2 renames a group folder (double-click is taken by open-head-layout).
    tree->setEditTriggers(QAbstractItemView::EditKeyPressed);
    // Enter/Return also renames the current editable row (consistent with the
    // other trees); scoped to the tree so it doesn't shadow dialog defaults.
    for (int k : {int(Qt::Key_Return), int(Qt::Key_Enter)})
    {
        QShortcut *sc = new QShortcut(QKeySequence(k), tree);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, tree, [tree]() {
            QTreeWidgetItem *it = tree->currentItem();
            if (it != NULL && (it->flags() & Qt::ItemIsEditable)
                && tree->selectedItems().size() <= 1)
                tree->editItem(it, 0);
        });
    }
    lv->addWidget(tree, 1);
    left->setMinimumWidth(220);
    // (The drag-source panel was retired — adding fixtures is a right-click on the
    //  tree OR the canvas: "Add Fixtures…" opens a multi-select picker.)

    // ---- CENTER: toolbar + graphical canvas --------------------------------
    QWidget *center = new QWidget(body);
    QVBoxLayout *cv = new QVBoxLayout(center);
    cv->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("View:"), center));
    QComboBox *planeCombo = new QComboBox(center);
    planeCombo->addItems({ tr("Top"), tr("Front"), tr("Side"), tr("45°") });
    planeCombo->setItemData(3, tr("An angled look from above and to the side. "
                                  "For viewing only — drag in Top / Front / Side "
                                  "to move things."), Qt::ToolTipRole);
    // Click-to-cycle, exactly as the plot's View combo behaves.
    planeCombo->setProperty("cycleOnClick", true);
    planeCombo->installEventFilter(this);
    planeCombo->setToolTip(tr("Click to cycle, arrow for the full list"));
    bar->addWidget(planeCombo);
    /* Turn the view in quarter steps. Some designers read a plan with downstage
       at the TOP; 180 degrees gives them that. It is a view transform only --
       nothing in the workspace moves -- and quarter turns keep the handedness
       honest, so stage left/right stay truthful (the edge labels turn with it).
       Remembered across sessions, since it is a personal reading preference. */
    /* Camera angles for the 45-degree view, in the same drop-down shape as the
       plot's Grid button: the numbers are there when wanted, and out of the way
       otherwise. Only meaningful in the angled view, so it enables with it.
       Dragging the empty canvas orbits too -- these are for setting an exact
       angle, or reading back the one a drag arrived at. */
    QToolButton *angleBtn = new QToolButton(center);
    angleBtn->setText(tr("Angle"));
    angleBtn->setPopupMode(QToolButton::InstantPopup);
    angleBtn->setToolTip(tr("Camera angles for the 45° view — or just drag the "
                            "empty canvas to swing it"));
    QDoubleSpinBox *azSpin = new QDoubleSpinBox(center);
    azSpin->setRange(0.0, 359.0); azSpin->setSuffix(QString::fromUtf8("°"));
    QDoubleSpinBox *elSpin = new QDoubleSpinBox(center);
    elSpin->setRange(0.0, 90.0); elSpin->setSuffix(QString::fromUtf8("°"));
    {
        QWidget *aw = new QWidget(center);
        QFormLayout *afl = new QFormLayout(aw);
        afl->setContentsMargins(10, 8, 10, 8);
        afl->addRow(tr("Around the stage:"), azSpin);
        afl->addRow(tr("Height of eye:"), elSpin);
        QMenu *am = new QMenu(angleBtn);
        QWidgetAction *wa = new QWidgetAction(am);
        wa->setDefaultWidget(aw);
        am->addAction(wa);
        angleBtn->setMenu(am);
    }
    bar->addWidget(angleBtn);

    QToolButton *rotBtn = new QToolButton(center);
    rotBtn->setToolTip(tr("Turn the view 90° clockwise. A view setting only — "
                          "nothing in the workspace moves."));
    bar->addWidget(rotBtn);

    QPushButton *lockBtn = new QPushButton(tr("🔒 Locked"), center);
    lockBtn->setCheckable(true); lockBtn->setChecked(true);
    lockBtn->setToolTip(tr("Locked: click to select fixtures. Unlock to drag them."));
    bar->addWidget(lockBtn);
    bar->addStretch();
    cv->addLayout(bar);
    /* The actions on a row of their own: sharing one with the view controls,
       they were squeezed below their text in an editor of ordinary width
       ("t on Fa", "stribu", "dd Ba"). */
    QHBoxLayout *actBar = new QHBoxLayout;
    QPushButton *faceBtn = new QPushButton(tr("Put on Face"), center);
    faceBtn->setToolTip(tr("Pin the selected fixtures (or all) to the current "
                           "view's face (Top / Front / Side)."));
    actBar->addWidget(faceBtn);
    QPushButton *distBtn = new QPushButton(tr("Distribute"), center);
    distBtn->setToolTip(tr("Space the selected fixtures evenly, in a line: all of "
                           "them (or none picked) along the whole object, a few "
                           "of them between the outermost two. Their height and "
                           "offset across it come from the middle one."));
    actBar->addWidget(distBtn);
    QPushButton *addBarBtn = new QPushButton(tr("Add Bar"), center);
    addBarBtn->setVisible(kind == 0 || kind == 2 || kind == 4);
    actBar->addWidget(addBarBtn);
    const QList<QPushButton *> actBtns = { faceBtn, distBtn, addBarBtn };
    foreach (QPushButton *b, actBtns)
        b->setMinimumWidth(b->sizeHint().width());
    actBar->addStretch();
    cv->addLayout(actBar);

    StructureStudioView *view = new StructureStudioView(
        m_doc, StructureStudioView::Kind(kind), id, center);
    view->setMinimumSize(320, 300);
    planeCombo->setCurrentIndex(int(view->plane()));

    auto pushAngle = [view, azSpin, elSpin]() {
        view->setAngledView(azSpin->value(), elSpin->value());
        QSettings().setValue(SETTINGS_STUDIOANGLE,
                             QString("%1,%2").arg(azSpin->value()).arg(elSpin->value()));
    };
    QObject::connect(azSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     view, [pushAngle](double) { pushAngle(); });
    QObject::connect(elSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     view, [pushAngle](double) { pushAngle(); });
    // An orbit drag has to write back, or the numbers would go stale the moment
    // the canvas is used.
    QObject::connect(view, &StructureStudioView::angledViewChanged, azSpin,
                     [azSpin, elSpin](double az, double el) {
        azSpin->blockSignals(true); azSpin->setValue(az); azSpin->blockSignals(false);
        elSpin->blockSignals(true); elSpin->setValue(el); elSpin->blockSignals(false);
        QSettings().setValue(SETTINGS_STUDIOANGLE, QString("%1,%2").arg(az).arg(el));
    });
    {
        const QStringList a = QSettings().value(SETTINGS_STUDIOANGLE).toString().split(',');
        if (a.size() == 2)
        {
            azSpin->setValue(a.at(0).toDouble());
            elSpin->setValue(a.at(1).toDouble());
        }
    }


    auto applyRotation = [view, rotBtn](int turns) {
        view->setRotation(turns);
        static const char *names[] = { "0°", "90°", "180°", "270°" };
        rotBtn->setText(QString("⟳ %1").arg(names[view->rotation() & 3]));
    };
    applyRotation(QSettings().value(SETTINGS_STUDIOROTATION, 0).toInt());
    QObject::connect(rotBtn, &QToolButton::clicked, view, [view, applyRotation]() {
        const int next = (view->rotation() + 1) & 3;
        applyRotation(next);
        QSettings().setValue(SETTINGS_STUDIOROTATION, next);
    });
    cv->addWidget(view, 1);
#if defined(Q_OS_MACOS)
    const QString addClick = tr("⌘- or ⇧-click to add more");
#else
    const QString addClick = tr("Ctrl- or Shift-click to add more");
#endif
    QLabel *hint = new QLabel(tr("click a fixture to select · %1 · "
                                 "double-click to edit · drag to reposition (all "
                                 "selected move together) · drag the background "
                                 "to pan · wheel to zoom").arg(addClick), center);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#8a8e99;");
    cv->addWidget(hint);

    // ---- RIGHT: inline fixture inspector -----------------------------------
    /* The quick subset of Fixture Properties: the same rows (FixtureFields),
       for every fixture selected -- what they differ on reads "mixed" -- each
       change applied at once, and one Ctrl+Z step. */
    QWidget *insp = new QWidget(body);
    insp->setObjectName("studioInspector");
    QVBoxLayout *iv = new QVBoxLayout(insp);
    QLabel *inspTitle = new QLabel(tr("(no fixture selected)"), insp);
    { QFont tf = inspTitle->font(); tf.setBold(true); inspTitle->setFont(tf); }
    inspTitle->setWordWrap(true);
    iv->addWidget(inspTitle);
    QFormLayout *inspForm = new QFormLayout;
    FixtureFields *fields = new FixtureFields(this, inspForm, insp, view);

    /* A bar's own rows: what it hangs from, and the one field worth editing
     * inline (its position along the parent); everything else it has is one
     * click away (Fixture Properties… opens the bar's editor). Range set once
     * real bar data arrives (populateBar()). */
    QLabel *barOnLbl = new QLabel(insp);
    barOnLbl->setStyleSheet("color:#b0b4c0;");
    inspForm->addRow(tr("Mounted on:"), barOnLbl);
    QDoubleSpinBox *barAlongSpin = new FeetInchesSpinBox(m_props->gridUnits() == MonitorProperties::Feet, insp);
    barAlongSpin->setDecimals(2);
    inspForm->addRow(tr("Along truss:"), barAlongSpin);

    iv->addLayout(inspForm);
    QPushButton *fullBtn = new QPushButton(tr("Fixture Properties…"), insp);
    fullBtn->setObjectName(QStringLiteral("inspectorFullProperties"));
    iv->addWidget(fullBtn);
    iv->addStretch();
    insp->setMinimumWidth(190);

    /* Ctrl+Z here: the fixtures' rig and colours as they were before each
       change -- a drag, an inspector field, a layout action. A run of steps
       on one field (a spin box held down) is one undo. Restoring values, not
       objects, so nothing the dialog around this holds is swapped out. */
    struct StudioUndo
    {
        QMap<quint32, FixtureRigProps> rig;
        QHash<quint32, QColor> gels;
        QString field;
        qint64 at = 0;
    };
    auto undoStack = QSharedPointer<QList<StudioUndo>>::create();
    auto pushUndoFor = [this, undoStack](const QString &field) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!field.isEmpty() && !undoStack->isEmpty() && undoStack->last().field == field
            && now - undoStack->last().at < 1500)
        {
            undoStack->last().at = now;     // the same run of steps
            return;
        }
        StudioUndo u;
        u.rig = m_props->fixtureRigPropsMap();
        foreach (quint32 fid, m_props->fixtureItemsID())
            u.gels.insert(fid, m_props->fixtureGelColor(fid, 0, 0));
        u.field = field;
        u.at = now;
        undoStack->append(u);
        while (undoStack->size() > 50) undoStack->removeFirst();
    };
    auto pushUndo = [pushUndoFor]() { pushUndoFor(QString()); };
    connect(fields, &FixtureFields::aboutToEdit, insp, [pushUndoFor](const QString &field) {
        pushUndoFor(field);
    });

    // Fixture::invalidId(), NOT 0: fixture 0 is the first fixture in any
    // workspace, so a 0 "nothing selected" sentinel silently dropped every
    // inspector edit made to it.
    auto curFid = QSharedPointer<quint32>::create(Fixture::invalidId());
    /* Every fixture selected, in the list or on the canvas; curFid is the one
       whose kind decides the rows. An edit goes to all of them ("if I select
       all the fixtures .. apply tilt/angle changes to all of them at same
       time"). */
    auto curFids = QSharedPointer<QList<quint32>>::create();
    auto targets = [curFid, curFids]() {
        if (*curFid == Fixture::invalidId())
            return QList<quint32>();
        return curFids->contains(*curFid) ? *curFids : QList<quint32>{ *curFid };
    };
    // Truss::invalidId() (UINT_MAX) — a bar is a Truss, not a Fixture, so it
    // needs its own "nothing selected" slot alongside curFid, not to share it.
    auto curBarId = QSharedPointer<quint32>::create(UINT_MAX);
    auto row = [inspForm](QWidget *w, bool vis) {
        w->setVisible(vis);
        if (QWidget *l = inspForm->labelForField(w)) l->setVisible(vis);
    };
    auto populate = [this, curFid, curBarId, targets, inspTitle, fields, fullBtn]() {
        const quint32 fid = *curFid;
        Fixture *fx = (fid != Fixture::invalidId()) ? m_doc->fixture(fid) : nullptr;
        const int many = targets().size();
        inspTitle->setText(fx != nullptr ? (many > 1 ? tr("%1 fixtures selected").arg(many) : fx->name())
                           : (*curBarId != UINT_MAX) ? QString()   // populateBar() sets it
                           : tr("(no fixture selected)"));
        fullBtn->setEnabled(fx != nullptr);
        if (*curBarId != UINT_MAX || fx == nullptr)
        {
            fields->hideAll();
            return;
        }
        fields->setFixtures(targets(), fid);
    };
    // The heading is the fixture's name: a rename elsewhere shows here too.
    connect(m_doc, &Doc::fixtureChanged, inspTitle, [curFid, populate](quint32 fid) {
        if (*curFid == fid)
            populate();
    });
    populate();

    /* The bar half of the same panel: "when I select it .. it doesn't
       hilight nor does the right side change to the bar data." */
    auto populateBar = [this, curFid, curBarId, inspTitle, fullBtn, barOnLbl, barAlongSpin, row]() {
        const quint32 barId = *curBarId;
        Truss *bar = (barId != UINT_MAX) ? m_props->truss(barId) : nullptr;
        const bool have = (bar != nullptr);
        row(barAlongSpin, have);
        row(barOnLbl, have);
        if (!have)
        {
            // A FIXTURE is showing (or nothing is): its rows are FixtureFields'.
            if (*curFid == Fixture::invalidId())
            {
                inspTitle->setText(tr("(no fixture selected)"));
                fullBtn->setEnabled(false);
            }
            return;
        }
        fullBtn->setEnabled(true);
        inspTitle->setText(bar->name());
        Truss *parent = m_props->truss(bar->parentTrussId());
        const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
        const double toDisp = isFeet ? 3.28084 : 1.0;
        barOnLbl->setText(parent ? tr("Truss %1").arg(parent->name()) : QString("—"));
        barAlongSpin->blockSignals(true);
        barAlongSpin->setRange(0.0, parent ? double(parent->length()) * toDisp : 100.0);
        barAlongSpin->setValue(double(bar->parentOffset()) * toDisp);
        // The bar's OWN lock only, same reasoning as dragBarTo() -- a bar is
        // its own structural object, not a fixture attached to one.
        barAlongSpin->setEnabled(!bar->locked());
        barAlongSpin->blockSignals(false);
    };
    populateBar();

    connect(view, &StructureStudioView::fixtureSelected, insp,
            [curFid, curFids, curBarId, populate, populateBar](quint32 fid){
                *curFid = fid; *curBarId = UINT_MAX;
                if (!curFids->contains(fid))
                    *curFids = (fid == Fixture::invalidId()) ? QList<quint32>() : QList<quint32>{ fid };
                populate(); populateBar();
            });
    connect(view, &StructureStudioView::fixturesSelected, insp,
            [curFid, curFids, populate](const QList<quint32> &fids){
                *curFids = fids;
                if (!fids.contains(*curFid))
                    *curFid = fids.isEmpty() ? Fixture::invalidId() : fids.first();
                populate();
            });
    connect(view, &StructureStudioView::barSelected, insp,
            [curFid, curBarId, populate, populateBar](quint32 barId){
                *curBarId = barId; *curFid = Fixture::invalidId(); populate(); populateBar();
            });
    connect(barAlongSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), insp,
            [this, curBarId, view, pushUndoFor](double v) {
        if (*curBarId == UINT_MAX) return;
        Truss *bar = m_props->truss(*curBarId);
        if (bar == nullptr) return;
        Truss *parent = m_props->truss(bar->parentTrussId());
        if (parent == nullptr || bar->locked()) return;
        const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
        const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
        bar->setParentOffset(qBound(0.0f, float(v * fromDisp), parent->length()));
        m_props->recomputeChildTrusses();
        m_doc->setModified();
        if (view) view->reload();
        m_graphicsView->updateTrusses();
    });
    connect(fullBtn, &QPushButton::clicked, insp,
            [this, curFid, curBarId, targets]() {
        if (*curBarId != UINT_MAX)
            slotEditTruss(*curBarId);
        else if (targets().size() > 1)
            editFixturesProperties(targets());
        else if (*curFid != Fixture::invalidId())
            editFixtureProperties(*curFid);
    });

    body->addWidget(left);
    body->addWidget(center);
    body->addWidget(insp);
    body->setStretchFactor(0, 0);
    body->setStretchFactor(1, 1);
    body->setStretchFactor(2, 0);

    // ---- Fixture tree: group folders → fixtures, with selection sync -------
    auto rebuildTree = [this, tree, view, kind, id]() {
        tree->blockSignals(true);
        tree->clear();
        // The fixtures on THIS object — a fixture group counts as "local" only if
        // every one of its members is among them (not a cross-assembly / global
        // group). This matches the Layers tree so the two listings agree.
        const QList<quint32> mounted = view->mountedFixtures();
        const QSet<quint32> mountedSet(mounted.begin(), mounted.end());
        QMap<quint32, QTreeWidgetItem *> groupNodes;   // fixtureGroup id → node
        QTreeWidgetItem *ungrouped = nullptr;
        foreach (quint32 fid, mounted)
        {
            // Find the first LOCAL fixture group this fixture belongs to (all its
            // members are on this object); global groups don't organise the tree.
            quint32 gid = 0; QString gname;
            foreach (FixtureGroup *g, m_doc->fixtureGroups())
            {
                if (g == nullptr || !g->fixtureList().contains(fid))
                    continue;
                bool local = true;
                foreach (quint32 m, g->fixtureList())
                    if (!mountedSet.contains(m)) { local = false; break; }
                if (local) { gid = g->id(); gname = g->name(); break; }
            }
            QTreeWidgetItem *parent;
            if (gid == 0)
            {
                if (!ungrouped)
                {
                    ungrouped = new QTreeWidgetItem(tree, QStringList(tr("Ungrouped")));
                    ungrouped->setExpanded(true);
                }
                parent = ungrouped;
            }
            else
            {
                if (!groupNodes.contains(gid))
                {
                    QTreeWidgetItem *gn = new QTreeWidgetItem(tree, QStringList(gname));
                    gn->setExpanded(true);
                    gn->setData(0, Qt::UserRole + 1, gid);   // folder → fixture-group id
                    gn->setFlags(gn->flags() | Qt::ItemIsEditable);   // F2 / Rename
                    groupNodes.insert(gid, gn);
                }
                parent = groupNodes.value(gid);
            }
            Fixture *fx = m_doc->fixture(fid);
            QTreeWidgetItem *it = new QTreeWidgetItem(parent, QStringList(fx ? fx->name() : QString::number(fid)));
            it->setData(0, Qt::UserRole, fid);
            it->setFlags(it->flags() | Qt::ItemIsEditable);    // Return / F2 renames the fixture
        }
        /* A bar hung on THIS truss is a child Truss, not a fixture, so it never
           shows up above -- mountedFixtures() only walks m_doc->fixtures(). Left
           out entirely, a bar just added here had nowhere in this editor to
           appear or be positioned ("truss on T1 is STILL not part of T1 .. it
           should show up in the truss editor as something I can place"), even
           though the data model already has it correctly parented. List it here
           too, under its own heading, double-click opening its own editor
           (slotEditTruss) where its face/along-truss/lock fields actually live. */
        if (kind == 2)
        {
            QTreeWidgetItem *barsNode = nullptr;
            foreach (Truss *t, m_props->trusses())
            {
                if (t == nullptr || t->parentTrussId() != id)
                    continue;
                if (!barsNode)
                {
                    barsNode = new QTreeWidgetItem(tree, QStringList(tr("Bars")));
                    barsNode->setExpanded(true);
                }
                QTreeWidgetItem *it = new QTreeWidgetItem(barsNode, QStringList(t->name()));
                it->setData(0, Qt::UserRole + 2, t->id());
                it->setFlags(it->flags() | Qt::ItemIsEditable);   // ...and a bar
            }
        }
        tree->blockSignals(false);
    };
    rebuildTree();

    /* A fixture renamed anywhere (the edit dialog, Layers, the Fixture
       Manager) shows its new name in this list and on the canvas. Deferred:
       a rename made IN this list arrives mid-edit of its own row. */
    {
        auto pending = std::make_shared<bool>(false);
        connect(m_doc, &Doc::fixtureChanged, tree, [tree, view, rebuildTree, pending](quint32) {
            if (*pending)
                return;
            *pending = true;
            QTimer::singleShot(0, tree, [view, rebuildTree, pending]() {
                *pending = false;
                rebuildTree();
                view->update();
            });
        });
    }

    auto rebuildSource = []() {};   // drag-source panel retired (right-click adds)

    // Tree selection → highlight on the canvas + populate the inspector.
    connect(tree, &QTreeWidget::itemSelectionChanged, tree,
            [tree, view, curFid, curFids, curBarId, populate, populateBar]() {
        QList<quint32> ids;
        quint32 barId = UINT_MAX;
        foreach (QTreeWidgetItem *it, tree->selectedItems())
        {
            const quint32 fid = studioRowFid(it);
            if (fid != Fixture::invalidId()) { if (!ids.contains(fid)) ids << fid; continue; }
            /* A folder row -- a fixture group, or "Ungrouped" -- stands for
               the fixtures listed under it. Not keyed on the group id: 0 is a
               real fixture group id, and "Ungrouped" has none. */
            if (studioRowBarId(it) == Truss::invalidId() && it->childCount() > 0)
            {
                std::function<void(QTreeWidgetItem *)> addUnder = [&](QTreeWidgetItem *n) {
                    for (int c = 0; c < n->childCount(); ++c)
                    {
                        const quint32 cf = studioRowFid(n->child(c));
                        if (cf != Fixture::invalidId() && !ids.contains(cf)) ids << cf;
                        addUnder(n->child(c));
                    }
                };
                addUnder(it);
                continue;
            }
            if (barId == UINT_MAX)   // first bar row wins; a bar has no multi-select drag
                barId = studioRowBarId(it);
        }
        *curFids = ids;
        view->setHighlight(ids);
        view->setBarHighlight(ids.isEmpty() ? barId : UINT_MAX);
        *curFid = ids.isEmpty() ? Fixture::invalidId() : ids.first();
        *curBarId = ids.isEmpty() ? barId : UINT_MAX;
        populate();
        populateBar();
    });
    // Canvas click → select the matching tree row.
    connect(view, &StructureStudioView::fixtureSelected, tree, [tree](quint32 fid) {
        tree->blockSignals(true);
        tree->clearSelection();
        QTreeWidgetItemIterator it(tree);
        while (*it) { if (studioRowFid(*it) == fid) { (*it)->setSelected(true); } ++it; }
        tree->blockSignals(false);
    });
    // Cmd-click on the canvas: the tree selects all of them.
    connect(view, &StructureStudioView::fixturesSelected, tree, [tree](const QList<quint32> &fids) {
        tree->blockSignals(true);
        tree->clearSelection();
        QTreeWidgetItemIterator it(tree);
        while (*it) { if (fids.contains(studioRowFid(*it))) { (*it)->setSelected(true); } ++it; }
        tree->blockSignals(false);
    });
    // Same, for a bar clicked on the canvas.
    connect(view, &StructureStudioView::barSelected, tree, [tree](quint32 barId) {
        tree->blockSignals(true);
        tree->clearSelection();
        QTreeWidgetItemIterator it(tree);
        while (*it) { if (studioRowBarId(*it) == barId) { (*it)->setSelected(true); } ++it; }
        tree->blockSignals(false);
    });
    // Rename a group folder (F2 or right-click Rename) → update the fixture group.
    connect(tree, &QTreeWidget::itemChanged, tree, [this, tree, view, curFid, populate](QTreeWidgetItem *it, int col) {
        if (col != 0) return;
        const QString nm = it->text(0).trimmed();
        // Put the stored name back without re-entering this handler.
        auto restore = [tree, it](const QString &name) {
            tree->blockSignals(true);
            it->setText(0, name);
            tree->blockSignals(false);
        };

        // A fixture row: rename the fixture itself, everywhere it is shown.
        if (it->data(0, Qt::UserRole).isValid())
        {
            Fixture *fx = m_doc->fixture(it->data(0, Qt::UserRole).toUInt());
            if (fx == nullptr) return;
            if (nm.isEmpty()) { restore(fx->name()); return; }
            if (fx->name() != nm)
            {
                fx->setName(nm);
                m_doc->setModified();
                m_graphicsView->updateFixture(fx->id());
                if (m_layersPanel) m_layersPanel->reload();
                if (view) view->reload();
                if (*curFid == fx->id()) populate();         // the inspector's title
            }
            if (it->text(0) != nm) restore(nm);
            return;
        }
        // A bar row.
        if (studioRowBarId(it) != Truss::invalidId())
        {
            Truss *bar = m_props->truss(studioRowBarId(it));
            if (bar == nullptr) return;
            if (nm.isEmpty()) { restore(bar->name()); return; }
            if (bar->name() != nm)
            {
                bar->setName(nm);
                m_doc->setModified();
                m_graphicsView->updateTrusses();
                if (m_layersPanel) m_layersPanel->reload();
                if (view) view->reload();
            }
            return;
        }
        // A group folder.
        const quint32 gid = it->data(0, Qt::UserRole + 1).toUInt();
        if (gid == 0) return;
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != nullptr && nm.isEmpty()) { restore(g->name()); return; }
        if (g != nullptr && g->name() != nm)
        {
            g->setName(nm);
            m_doc->setModified();
        }
    });
    // Double-click a tree fixture → edit it; a bar → its own truss editor
    // (face/along-truss/lock live there, not in this one); a GROUP folder →
    // its head-layout grid.
    connect(tree, &QTreeWidget::itemDoubleClicked, tree, [this](QTreeWidgetItem *it, int) {
        const quint32 fid = studioRowFid(it);
        if (fid != Fixture::invalidId()) { editFixtureProperties(fid); return; }
        const quint32 barId = studioRowBarId(it);
        if (barId != Truss::invalidId()) { slotEditTruss(barId); return; }
        const quint32 gid = it->data(0, Qt::UserRole + 1).toUInt();
        if (gid != 0) openGroupLayout(gid);
    });
    // Gather the fixture ids currently selected in the tree.
    auto selectedFids = [tree]() {
        QList<quint32> ids;
        foreach (QTreeWidgetItem *it, tree->selectedItems())
        {
            const quint32 fid = studioRowFid(it);
            if (fid != Fixture::invalidId()) ids << fid;
        }
        return ids;
    };

    auto doUndo = [this, undoStack, view, rebuildTree, populate]() {
        if (undoStack->isEmpty()) return;
        const StudioUndo snap = undoStack->takeLast();
        for (auto it = snap.rig.constBegin(); it != snap.rig.constEnd(); ++it)
            m_props->setFixtureRigProps(it.key(), it.value());
        for (auto it = snap.gels.constBegin(); it != snap.gels.constEnd(); ++it)
        {
            m_props->setFixtureGelColor(it.key(), 0, 0, it.value());
            m_graphicsView->setFixtureGelColor(it.key(), it.value());
        }
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx) m_graphicsView->updateFixture(fx->id());
        m_graphicsView->updatePlatforms();
        m_doc->setModified();
        if (view) view->reload();
        rebuildTree();
        populate();
    };
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, dlg);
    connect(undoSc, &QShortcut::activated, dlg, [doUndo]() { doUndo(); });

    /* Take fixtures off THIS object, from the list as well as the canvas ("no
       way to remove fixture from tower"). The full unmount for a stand, tower,
       truss or boom -- out of its group too, or it stays listed under a
       structure it is no longer on; a platform clears its riser/deck mount. */
    const QString objectNoun = (kind == 1) ? tr("Tower") : (kind == 2) ? tr("Truss")
                             : (kind == 3) ? tr("Platform") : (kind == 4) ? tr("Boom") : tr("Stand");
    auto removeFromObject = [this, kind](const QList<quint32> &fids) {
        foreach (quint32 fid, fids)
        {
            if (kind == 3)
            {
                FixtureRigProps rp = m_props->fixtureRigProps(fid);
                rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
                rp.deckPlatformId  = FixtureRigProps::invalidPlatformId();
                m_props->setFixtureRigProps(fid, rp);
                m_graphicsView->updateFixture(fid);
            }
            else
                m_graphicsView->unmountFixture(fid);
        }
        m_doc->setModified();
    };

    // Right-click the tree → add fixtures / a group, or make a group from selection.
    connect(tree, &QTreeWidget::customContextMenuRequested, tree,
            [this, tree, kind, id, view, rebuildTree, rebuildSource, selectedFids, pushUndo,
             objectNoun, removeFromObject](const QPoint &pos) {
        QMenu m;
        QAction *aFix = m.addAction(tr("Add Fixtures…"));
        QAction *aGrp = m.addAction(tr("Add Existing Fixture Group…"));
        const QList<quint32> selNow = selectedFids();
        QAction *aNew = nullptr;
        QAction *aRemove = nullptr;
        if (!selNow.isEmpty())
        {
            m.addSeparator();
            aRemove = m.addAction(selNow.size() > 1
                                      ? tr("Remove %1 Fixtures from %2").arg(selNow.size()).arg(objectNoun)
                                      : tr("Remove from %1").arg(objectNoun));
        }
        if (!selNow.isEmpty())
        {
            m.addSeparator();
            aNew = m.addAction(tr("New Fixture Group from Selection…"));
        }
        // Rename whatever is under the cursor: a group folder, a fixture, a bar.
        QTreeWidgetItem *hit = tree->itemAt(pos);
        QAction *aRen = nullptr, *aLay = nullptr;
        if (hit != nullptr && hit->data(0, Qt::UserRole + 1).toUInt() != 0)
        {
            m.addSeparator();
            aRen = m.addAction(tr("Rename Group"));
            aLay = m.addAction(tr("Head Layout…"));
        }
        else if (hit != nullptr && (hit->flags() & Qt::ItemIsEditable))
        {
            m.addSeparator();
            aRen = m.addAction(studioRowBarId(hit) != Truss::invalidId() ? tr("Rename Bar")
                                                                        : tr("Rename Fixture"));
        }
        QAction *chosen = m.exec(tree->viewport()->mapToGlobal(pos));
        if (chosen == aFix) { pushUndo(); studioAddFixture(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (chosen == aGrp) { pushUndo(); studioAddGroup(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (aNew && chosen == aNew) { studioCreateGroup(selNow, view); rebuildTree(); rebuildSource(); }
        else if (aRemove && chosen == aRemove)
        {
            pushUndo();
            removeFromObject(selNow);
            view->reload(); rebuildTree(); rebuildSource();
        }
        else if (aRen && chosen == aRen) { tree->editItem(hit, 0); }
        else if (aLay && chosen == aLay) { openGroupLayout(hit->data(0, Qt::UserRole + 1).toUInt()); }
    });
    auto refreshMap = [this]() {
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx) m_graphicsView->updateFixture(fx->id());
    };

    connect(distBtn, &QPushButton::clicked, tree,
            [view, selectedFids, pushUndo, refreshMap]() {
        pushUndo(); view->distributeOnFace(selectedFids()); refreshMap();
    });
    connect(faceBtn, &QPushButton::clicked, tree,
            [view, selectedFids, pushUndo, refreshMap]() {
        pushUndo(); view->putOnFace(selectedFids()); refreshMap();
    });
    connect(view, &StructureStudioView::editAboutToStart, tree, [pushUndo]() { pushUndo(); });

    angleBtn->setEnabled(view->plane() == StructureStudioView::Angled);
    connect(planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), body,
            [view, angleBtn](int i){
        view->setPlane(StructureStudioView::Plane(i));
        angleBtn->setEnabled(view->plane() == StructureStudioView::Angled);
    });
    view->setLocked(true);
    connect(lockBtn, &QPushButton::toggled, view, [view, lockBtn](bool on) {
        view->setLocked(on);
        lockBtn->setText(on ? tr("🔒 Locked") : tr("🔓 Unlocked"));
    });
    connect(view, &StructureStudioView::fixtureActivated, body,
            [this](quint32 fid){ editFixtureProperties(fid); });
    connect(view, &StructureStudioView::fixtureMoved, body,
            [this](quint32 fid){ m_graphicsView->updateFixture(fid); });
    // Double-click a bar on the canvas → its own editor (Along truss/Face/Lock
    // live there, not in this one) — same target slotEditTruss() already
    // reaches from the sidebar tree's own double-click.
    connect(view, &StructureStudioView::barActivated, body,
            [this](quint32 barId){ slotEditTruss(barId); });
    // Double-click a hanger's bracket, arm or chain → the hanger editor.
    connect(view, &StructureStudioView::hangerActivated, body,
            [this, view](quint32 hid){ slotEditHanger(hid); view->reload(); });
    // Dragged a bar on the canvas → sync the main 2D plot's truss item.
    connect(view, &StructureStudioView::barMoved, body,
            [this](quint32){ m_graphicsView->updateTrusses(); });
    connect(addBarBtn, &QPushButton::clicked, body,
            [this, kind, id, view, rebuildSource]() { studioAddBar(kind, id, view); rebuildSource(); });
    // Keep the trees in sync after canvas-side edits.
    connect(view, &StructureStudioView::fixtureMoved, body, [rebuildTree](quint32){ rebuildTree(); });
    connect(view, &StructureStudioView::barMoved, body, [rebuildTree](quint32){ rebuildTree(); });
    // A boom resized on the canvas → redraw it (and its fixtures) on the 2D map.
    connect(view, &StructureStudioView::structureChanged, body,
            [this, refreshMap]() { m_graphicsView->updatePlatforms(); refreshMap(); });

    // Drop a fixture from the source tree onto a face → mount + land it there.
    connect(view, &StructureStudioView::fixturesDropped, body,
            [this, kind, id, view, rebuildTree, rebuildSource, pushUndo, refreshMap]
            (const QList<quint32> &fids, const QPointF &pos) {
        const quint32 boomId = structureBoomId(kind, id);
        if (kind == 0 && boomId == Pipe::invalidId())
        {
            QMessageBox::information(this, tr("Add Fixture"),
                tr("Add a bar to this stand first, then drop fixtures on it."));
            return;
        }
        pushUndo();
        foreach (quint32 fid, fids)
        {
            mountFixtureOnStructure(fid, kind, id, boomId);
            view->placeFixtureAt(fid, pos);   // land at the drop point
        }
        view->reload(); rebuildTree(); rebuildSource(); refreshMap();
    });

    // Right-click the canvas → the SAME add options as the tree (the preferred
    // multi-select workflow), or edit/remove the fixture under the cursor.
    connect(view, &StructureStudioView::canvasContextMenu, body,
            [this, kind, id, view, tree, selectedFids, rebuildTree, rebuildSource, pushUndo, refreshMap,
             objectNoun, removeFromObject]
            (const QPoint &globalPos, quint32 fidUnder) {
        QMenu m;
        /* "i can't add cross bar/truss on T2 .. that option is missing": the
           toolbar's Add Bar in both menus -- over a fixture too, since a
           fixture rigged inside a truss can cover most of it -- and on a
           truss, where you clicked. */
        float barAt = -1.0f;
        auto addBarAction = [&]() -> QAction * {
            if (kind == 2)
            {
                Truss *tr2 = m_props->truss(id);
                if (tr2 == nullptr || tr2->isChildBar())
                    return nullptr;             // a bar carries no bars
                barAt = view->offsetAlongTrussAt(id, view->mapFromGlobal(globalPos));
                m.addSeparator();
                return m.addAction(barAt >= 0.0f ? tr("Add Bar Here") : tr("Add Bar"));
            }
            if (kind == 0 || kind == 4)
            {
                m.addSeparator();
                return m.addAction(tr("Add Crossbar"));
            }
            return nullptr;
        };
        /* A hanger -- bracket, arm and chain, one fixture on its hook -- off a
           truss (a tower truss too) or a Tower, where you clicked. */
        auto addHangerAction = [&]() -> QAction * {
            if (kind == 2)
            {
                Truss *tr2 = m_props->truss(id);
                if (tr2 == nullptr)
                    return nullptr;
                if (barAt < 0.0f)
                    barAt = view->offsetAlongTrussAt(id, view->mapFromGlobal(globalPos));
                return m.addAction(barAt >= 0.0f ? tr("Add Hanger Here") : tr("Add Hanger"));
            }
            if (kind == 1)
                return m.addAction(tr("Add Hanger"));
            return nullptr;
        };
        /* Right-click a hanger (not the lantern on it): edit it, or take it off. */
        const quint32 hangerUnder = (fidUnder == Fixture::invalidId())
            ? view->hitTestHanger(view->mapFromGlobal(globalPos)) : UINT_MAX;
        if (hangerUnder != UINT_MAX)
        {
            QAction *aEditH = m.addAction(tr("Edit Hanger…"));
            QAction *aRemH  = m.addAction(tr("Remove Hanger"));
            QAction *ch = m.exec(globalPos);
            if (ch == aEditH)
                slotEditHanger(hangerUnder);
            else if (ch == aRemH)
                slotHangerRemoveRequested(hangerUnder);
            if (ch != nullptr)
            {
                view->reload(); rebuildTree(); rebuildSource();
            }
            return;
        }
        if (fidUnder != Fixture::invalidId())
        {
            QAction *aEdit = m.addAction(tr("Edit fixture…"));
            QAction *aRem  = m.addAction(tr("Remove from %1").arg(objectNoun));
            QAction *aBarF = addBarAction();
            QAction *aHangF = addHangerAction();
            QAction *ch = m.exec(globalPos);
            if (aHangF && ch == aHangF) { studioAddHanger(kind, id, view, barAt); rebuildTree(); rebuildSource(); }
            else if (aBarF && ch == aBarF) { studioAddBar(kind, id, view, barAt); rebuildTree(); rebuildSource(); }
            else if (ch == aEdit) editFixtureProperties(fidUnder);
            else if (ch == aRem)
            {
                pushUndo();
                removeFromObject({ fidUnder });
                view->reload(); rebuildTree(); rebuildSource(); refreshMap();
            }
            return;
        }
        QAction *aFix = m.addAction(tr("Add Fixtures…"));
        QAction *aGrp = m.addAction(tr("Add Existing Fixture Group…"));
        QAction *aBar = addBarAction();
        QAction *aHang = addHangerAction();
        const QList<quint32> selNow = selectedFids();
        QAction *aNew = selNow.isEmpty() ? nullptr
            : (m.addSeparator(), m.addAction(tr("New Fixture Group from Selection…")));
        QAction *ch = m.exec(globalPos);
        if (ch == aFix) { pushUndo(); studioAddFixture(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (ch == aGrp) { pushUndo(); studioAddGroup(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (aBar && ch == aBar) { studioAddBar(kind, id, view, barAt); rebuildTree(); rebuildSource(); }
        else if (aHang && ch == aHang) { studioAddHanger(kind, id, view, barAt); rebuildTree(); rebuildSource(); }
        else if (aNew && ch == aNew) { studioCreateGroup(selNow, view); rebuildTree(); rebuildSource(); }
        Q_UNUSED(tree)
    });

    if (outView) *outView = view;
    return body;
}

quint32 Monitor::structureBoomId(int kind, quint32 id) const
{
    if (kind == 4) return id;
    if (kind == 0)
    {
        foreach (Pipe *p, m_props->pipes())
            if (p->standId() == id) return p->id();
    }
    return Pipe::invalidId();
}

void Monitor::mountFixtureOnStructure(quint32 fid, int kind, quint32 id, quint32 boomId)
{
    if (kind == 5) return;   // studio groups add via 'Group settings…' (not a rig mount)
    if (kind == 0 || kind == 4)
    {
        m_graphicsView->attachFixtureToPipe(fid, boomId);   // clears other mounts
        return;
    }
    const FixtureRigProps was = m_props->fixtureRigProps(fid);
    if (!(kind == 1 && was.onTower() && was.towerId == id))
        m_graphicsView->leaveMountGroupsOf(fid);    // off its old tower/pipe
    FixtureRigProps rp = was;
    rp.trussId = Truss::invalidId();
    rp.pipeId  = Pipe::invalidId();
    rp.towerId = Tower::invalidId();
    rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
    rp.deckPlatformId  = FixtureRigProps::invalidPlatformId();
    if (kind == 1) { if (Tower *tw = m_props->tower(id)) {
        rp.towerId = id; rp.towerMountSide = FixtureRigProps::TowerShelf; rp.towerShelf = 0;
        rp.towerU = tw->width() * 0.5f; rp.towerV = tw->depth() * 0.5f; } }
    else if (kind == 2) { if (Truss *t = m_props->truss(id)) {
        rp.trussId = id; rp.trussOffset = t->length() * 0.5f; } }
    else if (kind == 3) { if (StagePlatform *pl = m_props->platform(id)) {
        rp.riserPlatformId = id; rp.riserFace = FixtureRigProps::RiserTop;
        rp.riserU = pl->width() * 0.5f; rp.riserV = pl->depth() * 0.5f; } }
    m_props->setFixtureRigProps(fid, rp);

    /* Join the structure's auto-group as well as its mount.
     *
       The Layers tree nests a fixture under a structure by GROUP, not by mount,
       so a fixture attached here but left ungrouped is on the truss and absent
       from the tree underneath it -- "the added fixtures aren't showing up in
       the tree under the truss". The plot's own attachFixtureToTruss() has
       always done this; dropping onto the structure canvas did not, so which
       route you used decided whether the fixture appeared. */
    quint32 gid = 0;
    if (kind == 2)      { if (Truss *t = m_props->truss(id))          gid = t->groupId(); }
    else if (kind == 3) { if (StagePlatform *pl = m_props->platform(id)) gid = pl->groupId(); }
    if (gid != 0)
        m_props->setFixtureGroup(fid, gid);

    /* A fixture picked from the editor's list may never have been placed on
       the plot: give it an icon where its new mount puts it, or it is rigged
       and invisible -- and uncounted by the structure's bubble. */
    m_graphicsView->ensureFixtureOnPlot(fid);
    m_graphicsView->updateFixture(fid);

    // A tower gets its group formed on first mount (it has none until then).
    if (kind == 1)
        m_graphicsView->joinTowerGroup(fid, id);
}

void Monitor::studioCreateGroup(const QList<quint32> &fids, StructureStudioView *view)
{
    if (fids.isEmpty())
    {
        QMessageBox::information(this, tr("New Fixture Group"),
            tr("Select one or more fixtures in the tree first."));
        return;
    }
    // Shared creator: grid suggested from the fixtures' 2-D layout, name/size
    // prompt, and fixtures arranged into the grid IN PHYSICAL ORDER (so a wide
    // panel maps left→right instead of being transposed by fixture order).
    if (CreateFixtureGroup::createFromFixtures(m_doc, fids, this) == NULL)
        return;
    if (view)
        view->reload();
}

void Monitor::studioDistribute(int kind, quint32 id, StructureStudioView *view,
                               const QList<quint32> &sel)
{
    QList<quint32> fids = sel.isEmpty() ? view->mountedFixtures() : sel;
    const int n = fids.size();
    if (n == 0) return;
    for (int i = 0; i < n; ++i)
    {
        const float t = (i + 0.5f) / float(n);   // even fractional position
        const quint32 fid = fids[i];
        FixtureRigProps rp = m_props->fixtureRigProps(fid);
        const quint32 fg = m_props->fixtureFrameGroup(fid);
        if (fg != 0 && kind == 3)
        {
            // Frame-group fixture on a platform → space along the width (X).
            StagePlatform *pl = m_props->platform(id);
            const QVector3D cur = m_props->fixtureRigPosition(fid);
            const float x = pl ? pl->originX() + t * pl->width() : cur.x();
            rp.groupLocal = m_props->worldToGroupLocal(fg, QVector3D(x, cur.y(), cur.z()));
        }
        else if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId())
        {
            if (StagePlatform *pl = m_props->platform(rp.riserPlatformId))
                rp.riserU = t * pl->width();
        }
        else if (rp.pipeId != Pipe::invalidId())
        {
            if (Pipe *p = m_props->pipe(rp.pipeId)) rp.pipeOffset = t * p->length();
        }
        else if (rp.trussId != Truss::invalidId())
        {
            if (Truss *tt = m_props->truss(rp.trussId)) rp.trussOffset = t * tt->length();
        }
        else if (rp.towerId != Tower::invalidId())
        {
            if (Tower *tw = m_props->tower(rp.towerId)) rp.towerU = t * tw->width();
        }
        m_props->setFixtureRigProps(fid, rp);
        m_graphicsView->updateFixture(fid);
    }
    if (view) view->reload();
    m_doc->setModified();
}

void Monitor::openGroupStudio(quint32 gid)
{
    if (!m_props->hasGroup(gid)) return;
    RigEditSession edit(this);
    MonitorProperties::MonitorGroup g = m_props->group(gid);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — Group %1").arg(g.name));

    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;
    QLineEdit *nameEdit = new QLineEdit(g.name, geomW);
    form->addRow(tr("Name:"), nameEdit);
    auto mk = [&](double v) { QDoubleSpinBox *s = new FeetInchesSpinBox(isFeet, geomW);
        s->setRange(-200, 200); s->setDecimals(2); s->setValue(v * toDisp); return s; };
    QDoubleSpinBox *ox = mk(g.origin.x()), *oy = mk(g.origin.y()), *oz = mk(g.origin.z());
    QDoubleSpinBox *rot = new QDoubleSpinBox(geomW);
    rot->setRange(-180, 180); rot->setDecimals(0); rot->setSuffix(QStringLiteral("°")); rot->setValue(double(g.rotation));
    form->addRow(tr("Frame origin X:"), ox);
    form->addRow(tr("Frame origin Y:"), oy);
    form->addRow(tr("Frame origin Z:"), oz);
    form->addRow(tr("Rotation:"), rot);
    vl->addLayout(form);
    /* Saving the arrangement you have just built is the POINT of laying one
       out -- lay a step out once, stamp it onto the other eleven -- and it was
       two levels down, behind a button whose label does not even fit. Direct,
       where the work happens. */
    QPushButton *saveCompBtn = new QPushButton(tr("Save as Component…"), geomW);
    saveCompBtn->setToolTip(tr("Store this arrangement in the component library, "
                               "then stamp it onto another set of fixtures from "
                               "the canvas (right-click → Studio Components)."));
    vl->addWidget(saveCompBtn);
    connect(saveCompBtn, &QPushButton::clicked, &dlg, [this, gid, nameEdit, &dlg]() {
        bool ok = false;
        const QString name = QInputDialog::getText(&dlg, tr("Save as Component"),
            tr("Component name:"), QLineEdit::Normal, nameEdit->text(), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        QString err;
        const QString path = StudioTemplate::saveToLibrary(m_doc, gid, name.trimmed(), &err);
        if (path.isEmpty())
            QMessageBox::warning(&dlg, tr("Save as Component"),
                                 tr("Could not save component: %1").arg(err));
        else
            QMessageBox::information(&dlg, tr("Save as Component"),
                tr("Saved “%1”.\n\nSelect another set of fixtures on the canvas, "
                   "then right-click → Studio Components → Stamp onto selection.")
                    .arg(name.trimmed()));
    });

    QPushButton *setBtn = new QPushButton(tr("Group settings… (bind · seed · component)"), geomW);
    vl->addWidget(setBtn);
    vl->addStretch();

    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 5 /*Group*/, gid, geomW, &view);

    auto refreshMembers = [this, gid]() {
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx && m_props->fixtureFrameGroup(fx->id()) == gid) m_graphicsView->updateFixture(fx->id());
    };
    auto applyLive = [=]() {
        m_props->setGroupName(gid, nameEdit->text());
        m_props->setGroupOrigin(gid, QVector3D(float(ox->value() * fromDisp),
                                               float(oy->value() * fromDisp),
                                               float(oz->value() * fromDisp)));
        m_props->setGroupRotation(gid, float(rot->value()));
        m_props->recomputeAnchoredFrames();
        if (view) view->reload();
        refreshMembers();
    };
    for (QDoubleSpinBox *s : { ox, oy, oz, rot })
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [applyLive](double){ applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });
    connect(setBtn, &QPushButton::clicked, &dlg, [this, gid, view, refreshMembers]() {
        // The old Studio Group window, tucked behind this button — bind/seed/
        // adopt/cell-pitch/save-as-component all still live there.
        StudioGroupEditor sg(m_doc, gid, this);
        connect(&sg, &StudioGroupEditor::changed, this, [refreshMembers]() { refreshMembers(); });
        sg.exec();
        if (view) view->reload();
        refreshMembers();
    });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);
    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }
    m_graphicsView->updatePlatforms();
    refreshMembers();
    m_doc->setModified();
    edit.accept();
}

void Monitor::openGroupLayout(quint32 groupId)
{
    FixtureGroup *g = m_doc->fixtureGroup(groupId);
    if (g == nullptr) return;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Head Layout — %1").arg(g->name()));
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    // The same grid editor the Fixtures tab uses — it edits the group live.
    FixtureGroupEditor *ed = new FixtureGroupEditor(g, m_doc, &dlg);
    vl->addWidget(ed, 1);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.resize(720, 520);
    dlg.exec();
    m_doc->setModified();
}

void Monitor::studioAddGroup(int kind, quint32 id, StructureStudioView *view)
{
    const quint32 boomId = structureBoomId(kind, id);
    if ((kind == 0) && boomId == Pipe::invalidId())
    {
        QMessageBox::information(this, tr("Add Fixture Group"),
            tr("Add a bar to this stand first, then add a group to it."));
        return;
    }
    // Pick a fixture group.
    QStringList names; QList<quint32> ids;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g) { names << g->name(); ids << g->id(); }
    if (names.isEmpty())
    {
        QMessageBox::information(this, tr("Add Fixture Group"),
            tr("There are no fixture groups yet. Create one from the Fixtures tab."));
        return;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, tr("Add Fixture Group"),
        tr("Group:"), names, 0, false, &ok);
    if (!ok) return;
    FixtureGroup *g = m_doc->fixtureGroup(ids.value(names.indexOf(pick)));
    if (g == nullptr) return;
    foreach (quint32 fid, g->fixtureList())
        mountFixtureOnStructure(fid, kind, id, boomId);
    if (view) view->reload();
    m_doc->setModified();
}

void Monitor::studioAddFixture(int kind, quint32 id, StructureStudioView *view)
{
    if (kind == 5)   // studio group
    {
        QMessageBox::information(this, tr("Add Fixtures"),
            tr("Add fixtures to a studio group with 'Group settings… → Add fixtures'."));
        return;
    }
    // A stand needs a boom/bar to hang fixtures on; a pipe IS the boom.
    const quint32 boomId = structureBoomId(kind, id);
    if (kind == 0 && boomId == Pipe::invalidId())
    {
        QMessageBox::information(this, tr("Add Fixture"),
            tr("Add a bar to this stand first (Add Bar), then hang fixtures on it."));
        return;
    }

    // Which fixtures are already on this structure — offer only the rest.
    QDialog pick(this);
    pick.setWindowTitle(tr("Add Fixtures to Structure"));
    pick.setMinimumWidth(320);
    QVBoxLayout *pv = new QVBoxLayout(&pick);
    pv->addWidget(new QLabel(tr("Choose fixtures to mount here:"), &pick));
    QListWidget *list = new QListWidget(&pick);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr) continue;
        const FixtureRigProps &rp = m_props->fixtureRigProps(fx->id());
        bool here = (kind == 1 && rp.towerId == id)
                 || (kind == 2 && rp.trussId == id)
                 || (kind == 3 && (rp.riserPlatformId == id || rp.deckPlatformId == id))
                 || (kind == 4 && rp.pipeId == id)
                 || (kind == 0 && rp.pipeId != Pipe::invalidId()
                        && m_props->pipe(rp.pipeId) && m_props->pipe(rp.pipeId)->standId() == id);
        if (here) continue;
        QListWidgetItem *it = new QListWidgetItem(fx->name(), list);
        it->setData(Qt::UserRole, fx->id());
    }
    pv->addWidget(list);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pick);
    connect(bb, &QDialogButtonBox::accepted, &pick, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &pick, &QDialog::reject);
    pv->addWidget(bb);
    if (pick.exec() != QDialog::Accepted)
        return;

    QList<quint32> chosen;
    foreach (QListWidgetItem *it, list->selectedItems())
        chosen << it->data(Qt::UserRole).toUInt();

    // Some may already be on ANOTHER feature — moving them detaches them there.
    int mountedElsewhere = 0;
    foreach (quint32 fid, chosen) if (fixtureIsMounted(fid)) ++mountedElsewhere;
    if (mountedElsewhere > 0 &&
        QMessageBox::question(this, tr("Move Fixtures"),
            tr("%1 of the chosen fixtures are already on another feature. "
               "Move them here (detach them from where they are)?").arg(mountedElsewhere),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    foreach (quint32 fid, chosen)
        mountFixtureOnStructure(fid, kind, id, boomId);
    if (view) view->reload();
    m_doc->setModified();
}

QString Monitor::mountDescription(const FixtureRigProps &rp, quint32 fid) const
{
    return m_props->mountDescription(rp, fid);
}

void Monitor::studioAddBar(int kind, quint32 id, StructureStudioView *view, float offset)
{
    if (kind == 1)   // tower
    {
        QMessageBox::information(this, tr("Add Bar"),
            tr("Towers use shelves — use 'Add shelf' in the tower fields."));
        return;
    }

    if (kind == 4)   // a pipe → a crossbar hung on it (vertical booms only)
    {
        Pipe *pipe = m_props->pipe(id);
        if (pipe == nullptr) return;
        if (!pipe->isVertical())
        {
            QMessageBox::information(this, tr("Add Bar"),
                tr("A crossbar hangs on a vertical boom. This pipe is horizontal."));
            return;
        }
        Pipe *bar = m_props->addPipe();
        bar->setName(tr("%1 Crossbar").arg(pipe->name()));
        bar->setOrientation(Pipe::Horizontal);
        bar->setParentPipeId(id);
        bar->setParentPipeOffset(pipe->length());
        bar->setLength(1.0f);
        bar->setRunAngle(0.0f);
        bar->setBaseRadius(0.0f);
        // Clear of the boom's own point in Top view -- see crossbarStandoff()'s
        // own doc comment and WORKFLOW-SUBTLETIES.md #14.
        bar->setCrossbarStandoff(0.15f);
        bar->setLayerId(pipe->layerId());
        m_props->recomputeChildTrusses();
        m_graphicsView->updatePlatforms();
        if (m_layersPanel) m_layersPanel->reload();
        m_doc->setModified();
        if (view) view->reload();
        return;
    }

    if (kind == 2)   // truss → a child bar at @p offset (mid-span when -1)
    {
        const quint32 barId = createBarOnTruss(id, offset);
        if (barId != Truss::invalidId())
        {
            m_graphicsView->updateTrusses();
            if (m_layersPanel) m_layersPanel->reload();
            m_doc->setModified();
            if (view) view->reload();
            /* A bar is a child Truss, not a fixture -- this Structure Studio
               editor's tree/canvas only knows about mounted fixtures
               (StructureStudioView::mountedFixtures() walks m_doc->fixtures(),
               never Truss::parentTrussId()), so a bar just added here has
               nowhere in THIS editor to appear or be positioned. Open the
               truss editor on it directly, exactly as slotAddBarToTruss()
               already does for the tree's own "Add bar" action. */
            slotEditTruss(barId);
        }
        return;
    }

    // kind 0 → stand. A vertical boom if it has none yet, else a crossbar on it.
    Stand *s = m_props->stand(id);
    if (s == nullptr) return;
    quint32 boomId = Pipe::invalidId();
    foreach (Pipe *p, m_props->pipes())
        if (p->standId() == id && p->isVertical()) { boomId = p->id(); break; }

    Pipe *bar = m_props->addPipe();
    bar->setLayerId(s->layerId());
    if (boomId == Pipe::invalidId())
    {
        // First bar → a vertical boom standing on the stand. Default its hangable
        // length to (most of) the stand height so there's real room to rig;
        // fully adjustable afterwards in the boom editor.
        bar->setName(tr("%1 Boom").arg(s->name()));
        bar->setOrientation(Pipe::Vertical);
        bar->setStandId(id);
        bar->setLength(qMax(s->height(), 1.5f));
        bar->setBaseRadius(0.0f);
    }
    else
    {
        // Otherwise a horizontal crossbar on the existing boom, at its top.
        Pipe *boom = m_props->pipe(boomId);
        bar->setName(tr("%1 Crossbar").arg(s->name()));
        bar->setOrientation(Pipe::Horizontal);
        bar->setParentPipeId(boomId);
        bar->setParentPipeOffset(boom ? boom->length() : 1.5f);
        bar->setLength(1.0f);
        bar->setRunAngle(0.0f);
        bar->setBaseRadius(0.0f);
        // Clear of the boom's own point in Top view -- see crossbarStandoff()'s
        // own doc comment and WORKFLOW-SUBTLETIES.md #14.
        bar->setCrossbarStandoff(0.15f);
    }
    m_props->recomputeChildTrusses();
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    if (view) view->reload();
}

QList<quint32> Monitor::fixturesOnFeature(FeatureKind kind, quint32 id) const
{
    QList<quint32> out;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        const quint32 fid = fx->id();
        const FixtureRigProps &rp = m_props->fixtureRigProps(fid);
        bool hit = false;
        switch (kind)
        {
            case TrussFeature:  hit = (rp.trussId == id); break;
            case PipeFeature:   hit = (rp.pipeId  == id); break;
            case TowerFeature:  hit = (rp.towerId == id); break;
            case PlatformFeature:
                hit = (rp.riserPlatformId == id || rp.deckPlatformId == id);
                if (!hit)   // a studio frame group anchored to this platform
                {
                    const quint32 gid = m_props->fixtureFrameGroup(fid);
                    if (gid != 0)
                    {
                        const MonitorProperties::MonitorGroup g = m_props->group(gid);
                        hit = (g.anchorKind == QStringLiteral("platform") && g.anchorId == id);
                    }
                }
                break;
        }
        if (hit)
            out << fid;
    }
    return out;
}

Monitor::FeatureDeleteChoice Monitor::confirmFeatureDelete(
    const QString &kind, const QString &name, const QList<quint32> &fids)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Delete"));
    // With a name it reads "Delete stand “SL Stand”?"; without one (a multi-select
    // summary) the kind already carries the count: "Delete 3 selected features?".
    QString msg = name.isEmpty() ? tr("Delete %1?").arg(kind)
                                 : tr("Delete %1 “%2”?").arg(kind.toLower(), name);

    QPushButton *goBtn   = nullptr;   // detach & keep, or plain delete
    QPushButton *withBtn = nullptr;   // also remove the fixtures from the plot
    if (!fids.isEmpty())
    {
        msg += QStringLiteral("\n\n");
        msg += tr("%n fixture(s) mounted on it will be DETACHED and left in place. "
                  "You can also remove them from the plot.", "", fids.count());
        goBtn   = box.addButton(tr("Detach && Keep"),      QMessageBox::AcceptRole);
        withBtn = box.addButton(tr("Remove Fixtures Too"), QMessageBox::DestructiveRole);
    }
    else
    {
        goBtn = box.addButton(tr("Delete"), QMessageBox::AcceptRole);
    }
    QPushButton *cancelBtn = box.addButton(QMessageBox::Cancel);
    box.setText(msg);
    box.setDefaultButton(goBtn);
    box.exec();

    if (box.clickedButton() == cancelBtn)          return DeleteCancel;
    if (withBtn && box.clickedButton() == withBtn) return DeleteWithFixtures;
    return DeleteDetach;
}

void Monitor::slotTrussRemoveRequested(quint32 tid)
{
    Truss *t = m_props->truss(tid);
    if (t == NULL)
        return;
    const QList<quint32> fids = fixturesOnFeature(TrussFeature, tid);
    const FeatureDeleteChoice choice = confirmFeatureDelete(tr("Truss"), t->name(), fids);
    if (choice == DeleteCancel)
        return;
    if (choice == DeleteWithFixtures)
    {
        foreach (quint32 fid, fids) m_graphicsView->removeFixture(fid);
    }
    m_props->removeTruss(tid);
    m_graphicsView->updateTrusses();
    m_graphicsView->refreshFixtureBindings();   // ex-fixtures are no longer bound
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

quint32 Monitor::createBarOnTruss(quint32 parentId, float offset)
{
    Truss *parent = m_props->truss(parentId);
    if (parent == NULL)
        return Truss::invalidId();

    Truss *bar = m_props->addTruss();
    bar->setName(tr("Bar %1").arg(bar->id() + 1));
    bar->setLength(qMax(1.0f, parent->length() * 0.3f));
    // A bar/pipe is slimmer than the truss it hangs on (~50 mm pipe).
    bar->setWidth(0.05f);
    // Truss-LOCAL mount defaults, per parent orientation:
    //  • Vertical tower  → a horizontal crossbar ACROSS the Downstage face
    //    (the "bar across the front" case).
    //  • Overhead truss  → a bar running ALONG it, offset slightly
    //    downstage (not straight underneath -- see the comment below).
    bar->setParentTrussId(parentId);
    bar->setParentOffset(offset >= 0.0f ? offset : parent->length() / 2.0f);   // Along
    if (parent->type() == Truss::Vertical)
    {
        bar->setBarFace(Truss::FaceDownstage);
        bar->setBarRun(Truss::RunAcross);
        bar->setBarStandoff(0.0f);
    }
    else
    {
        /* NOT FaceBottom: barFaceVector(FaceBottom) is a pure Z vector, so
         * barStandoff -- "distance off that face" -- can never separate a
         * RunAlong bar from its parent in Top view no matter how large it
         * is set (recomputeChildTrusses()'s `base = attach + faceN *
         * (half + barStandoff)` gets zero X/Y contribution from a Z-only
         * faceN). The bar's line rendered EXACTLY on top of the truss's
         * own line, with no pixel a click could mean "the bar" rather
         * than "the truss" (WORKFLOW-SUBTLETIES.md #1). FaceDownstage
         * gives a real Y offset instead -- the same face the Vertical
         * case above already relies on for its own separation -- while
         * RunAlong keeps the bar parallel to/under the truss, same as
         * before. barStandoff on TOP of that is now meaningful, so it
         * gets an explicit nonzero default too, not just the bare
         * parent->width()/2 baseline every bar gets for free. */
        bar->setBarFace(Truss::FaceDownstage);
        bar->setBarRun(Truss::RunAlong);
        bar->setBarStandoff(0.15f);
    }
    bar->setBarCrossShift(0.0f);
    bar->setLayerId(parent->layerId());
    m_props->recomputeChildTrusses();                  // derive world geometry
    m_graphicsView->ensureTrussGroup(parentId);        // bar joins the parent's group
    if (parent->groupId() != 0)
        bar->setGroupId(parent->groupId());
    return bar->id();
}

void Monitor::slotAddBarToTruss(quint32 parentId, float offset)
{
    const quint32 barId = createBarOnTruss(parentId, offset);
    if (barId == Truss::invalidId())
        return;
    m_graphicsView->updateTrusses();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    slotEditTruss(barId);   // open the editor so the run/drop can be tweaked
}

void Monitor::slotAddBarToPipe(quint32 parentPipeId)
{
    Pipe *parent = m_props->pipe(parentPipeId);
    if (parent == NULL || !parent->isVertical())
        return;

    Pipe *bar = m_props->addPipe();
    bar->setName(tr("%1 Crossbar").arg(parent->name()));
    bar->setOrientation(Pipe::Horizontal);
    bar->setParentPipeId(parentPipeId);
    bar->setParentPipeOffset(parent->length());   // hang at the top of the boom
    bar->setLength(qMin(1.0f, parent->length()));  // a short T-bar by default
    bar->setRunAngle(0.0f);                        // runs stage-right
    bar->setBaseRadius(0.0f);
    // Clear of the boom's own point in Top view -- see crossbarStandoff()'s
    // own doc comment and WORKFLOW-SUBTLETIES.md #14.
    bar->setCrossbarStandoff(0.15f);
    bar->setColor(parent->color());
    bar->setLayerId(parent->layerId());

    m_props->recomputeChildTrusses();   // derive the bar's base from the boom
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    slotEditPipe(bar->id());   // let the user set run length / offset up the boom
}

void Monitor::slotPlatformRemoveRequested(quint32 pid)
{
    StagePlatform *p = m_props->platform(pid);
    if (p == NULL)
        return;
    const QList<quint32> fids = fixturesOnFeature(PlatformFeature, pid);
    const FeatureDeleteChoice choice = confirmFeatureDelete(tr("Platform"), p->name(), fids);
    if (choice == DeleteCancel)
        return;
    if (choice == DeleteWithFixtures)
    {
        foreach (quint32 fid, fids) m_graphicsView->removeFixture(fid);
    }
    m_props->removePlatform(pid);
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshRiserFixtures();
    m_graphicsView->refreshFixtureBindings();   // frame-group members detached
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotPipeRemoveRequested(quint32 pid)
{
    Pipe *b = m_props->pipe(pid);
    if (b == NULL)
        return;
    const QList<quint32> fids = fixturesOnFeature(PipeFeature, pid);
    const FeatureDeleteChoice choice = confirmFeatureDelete(tr("Boom"), b->name(), fids);
    if (choice == DeleteCancel)
        return;
    if (choice == DeleteWithFixtures)
    {
        foreach (quint32 fid, fids) m_graphicsView->removeFixture(fid);
    }
    m_props->removePipe(pid);
    m_graphicsView->updatePlatforms();          // pipes/stands/towers live here
    m_graphicsView->refreshFixtureBindings();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotHangerRemoveRequested(quint32 hid)
{
    Hanger *h = m_props->hanger(hid);
    if (h == nullptr)
        return;
    const quint32 hung = m_props->fixtureOnHanger(hid);
    if (hung != UINT_MAX)
    {
        Fixture *fx = m_doc->fixture(hung);
        if (QMessageBox::question(this, tr("Remove Hanger"),
                tr("%1 hangs on %2. Remove the hanger? The fixture stays where it is, "
                   "unmounted.").arg(fx ? fx->name() : tr("A fixture"), h->name()),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        m_graphicsView->unmountFixture(hung);
    }
    m_props->removeHanger(hid);
    m_graphicsView->updateHangers();
    m_graphicsView->refreshFixtureBindings();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::studioAddHanger(int kind, quint32 id, StructureStudioView *view, float offset)
{
    Hanger *h = nullptr;
    if (kind == 2 && m_props->truss(id) != nullptr)
    {
        Truss *t = m_props->truss(id);
        h = m_props->addHanger(Hanger::OnTruss, id);
        // Where it was clicked; else three quarters of the way up (along).
        h->setAlong(offset >= 0.0f ? offset : t->length() * 0.75f);
    }
    else if (kind == 1 && m_props->tower(id) != nullptr)
    {
        h = m_props->addHanger(Hanger::OnTower, id);
        h->setAlong(m_props->tower(id)->height() * 0.75f);
    }
    if (h == nullptr)
        return;
    m_props->recomputeHangers();
    m_graphicsView->updateHangers();
    m_doc->setModified();
    if (m_layersPanel) m_layersPanel->reload();
    slotEditHanger(h->id());
    if (view != nullptr)
        view->reload();
}

int Monitor::copyHangerToEveryTower(quint32 hid)
{
    Hanger *src = m_props->hanger(hid);
    if (src == nullptr)
        return 0;
    /* "Every tower": the trusses that stand up (not bars hung off others)
       and the Tower structures -- each one that does not already carry the
       same hanger at the same height and the same way. */
    auto alreadyHas = [&](Hanger::ParentKind kind, quint32 pid) {
        foreach (Hanger *h, m_props->hangersOn(kind, pid))
            if (qAbs(h->along() - src->along()) < 0.01f && qAbs(h->angle() - src->angle()) < 0.5f)
                return true;
        return false;
    };
    auto copyOnto = [&](Hanger::ParentKind kind, quint32 pid) {
        Hanger *h = m_props->addHanger(kind, pid);
        h->setAlong(src->along());
        h->setAngle(src->angle());
        h->setArm(src->arm());
        h->setDrop(src->drop());
    };
    int added = 0;
    foreach (Truss *t, m_props->trusses())
        if (t->type() == Truss::Vertical && !t->isChildBar()
            && !alreadyHas(Hanger::OnTruss, t->id()))
        {
            copyOnto(Hanger::OnTruss, t->id());
            ++added;
        }
    foreach (Tower *tw, m_props->towers())
        if (!alreadyHas(Hanger::OnTower, tw->id()))
        {
            copyOnto(Hanger::OnTower, tw->id());
            ++added;
        }
    m_props->recomputeHangers();
    if (added > 0)
    {
        m_doc->setModified();
        if (m_layersPanel) m_layersPanel->reload();
    }
    return added;
}

void Monitor::slotEditHanger(quint32 hid)
{
    Hanger *h = m_props->hanger(hid);
    if (h == nullptr)
        return;
    RigEditSession edit(this);
    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;

    QString on;
    bool vertical = true;
    float maxAlong = 20.0f;
    if (h->parentKind() == Hanger::OnTruss)
    {
        if (Truss *t = m_props->truss(h->parentId()))
        {
            on = t->name();
            vertical = (t->type() == Truss::Vertical);
            maxAlong = t->length();
        }
    }
    else if (Tower *tw = m_props->tower(h->parentId()))
    {
        on = tw->name();
        maxAlong = tw->height();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Hanger — %1").arg(h->name()));
    QFormLayout *form = new QFormLayout(&dlg);
    QLineEdit *nameEdit = new QLineEdit(h->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    form->addRow(tr("On:"), new QLabel(on.isEmpty() ? tr("(nothing -- its structure is gone)") : on, &dlg));
    auto mkSpin = [&](double val, double hi) {
        QDoubleSpinBox *sp = new FeetInchesSpinBox(isFeet, &dlg);
        sp->setRange(0.0, hi * toDisp); sp->setDecimals(2);
        sp->setValue(val * toDisp);
        return sp;
    };
    QDoubleSpinBox *alongSpin = mkSpin(h->along(), qMax(0.1f, maxAlong));
    form->addRow(vertical ? tr("Height:") : tr("Along:"), alongSpin);
    QComboBox *dirCb = new QComboBox(&dlg);
    const QList<QPair<QString, int> > dirs = {
        { tr("Downstage"), 0 }, { tr("Downstage left"), 45 }, { tr("Stage left"), 90 },
        { tr("Upstage left"), 135 }, { tr("Upstage"), 180 }, { tr("Upstage right"), 225 },
        { tr("Stage right"), 270 }, { tr("Downstage right"), 315 } };
    int best = 0;
    double bestD = 1e9;
    for (int i = 0; i < dirs.size(); ++i)
    {
        dirCb->addItem(dirs.at(i).first, dirs.at(i).second);
        const double d = qAbs(std::fmod(double(h->angle()) - dirs.at(i).second + 540.0, 360.0) - 180.0);
        if (d < bestD) { bestD = d; best = i; }
    }
    dirCb->setCurrentIndex(best);
    const int initialDir = best;
    dirCb->setToolTip(tr("Which way the arm reaches, seen from above"));
    form->addRow(tr("Points:"), dirCb);
    QDoubleSpinBox *armSpin = mkSpin(h->arm(), 5.0);
    armSpin->setToolTip(tr("How far the arm reaches out from the face of the structure"));
    form->addRow(tr("Arm:"), armSpin);
    QDoubleSpinBox *dropSpin = mkSpin(h->drop(), 10.0);
    dropSpin->setToolTip(tr("How far the hook hangs below the end of the arm"));
    form->addRow(tr("Drop:"), dropSpin);

    // What hangs on it: chosen here too, one hook.
    QComboBox *hangCb = new QComboBox(&dlg);
    hangCb->addItem(tr("(nothing)"), UINT_MAX);
    const quint32 hungNow = m_props->fixtureOnHanger(hid);
    foreach (Fixture *fx, m_doc->fixtures())
        hangCb->addItem(fx->name(), fx->id());
    hangCb->setCurrentIndex(qMax(0, hangCb->findData(hungNow)));
    form->addRow(tr("Hangs:"), hangCb);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton *copyAll = bb->addButton(tr("Copy to Every Tower"), QDialogButtonBox::ActionRole);
    copyAll->setToolTip(tr("The same hanger -- height, direction, arm, drop -- on every tower "
                           "that does not already have one there"));
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto apply = [&]() {
        h->setName(nameEdit->text().trimmed().isEmpty() ? h->name() : nameEdit->text().trimmed());
        h->setAlong(float(alongSpin->value() * fromDisp));
        if (dirCb->currentIndex() != initialDir)
            h->setAngle(float(dirCb->currentData().toInt()));
        h->setArm(float(armSpin->value() * fromDisp));
        h->setDrop(float(dropSpin->value() * fromDisp));
        m_props->recomputeHangers();
        const quint32 hang = hangCb->currentData().toUInt();
        const quint32 hung = m_props->fixtureOnHanger(hid);
        if (hang != hung)
        {
            if (hang == UINT_MAX)
                m_graphicsView->unmountFixture(hung);
            else
                m_graphicsView->attachFixtureToHanger(hang, hid);
        }
        m_graphicsView->updateTrusses();
        m_graphicsView->updatePlatforms();
        m_graphicsView->refreshFixtureBindings();
    };
    // Live: the hanger moves as its fields change; Cancel puts it back.
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [&](const QString &) { apply(); });
    for (QDoubleSpinBox *sp : { alongSpin, armSpin, dropSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double) { apply(); });
    for (QComboBox *cb : { dirCb, hangCb })
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { apply(); });
    connect(copyAll, &QPushButton::clicked, &dlg, [&]() {
        apply();
        const int n = copyHangerToEveryTower(hid);
        QMessageBox::information(&dlg, tr("Copy to Every Tower"),
            n == 0 ? tr("Every tower already has this hanger.")
                   : tr("Added this hanger to %n more tower(s).", "", n));
    });

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }
    apply();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotStandRemoveRequested(quint32 sid)
{
    Stand *s = m_props->stand(sid);
    if (s == NULL)
        return;
    // A stand carries no fixtures directly — booms/trusses stand on it and are
    // detached (left in place) by removeStand. No fixtures list to offer.
    const FeatureDeleteChoice choice = confirmFeatureDelete(tr("Stand"), s->name(),
                                                            QList<quint32>());
    if (choice == DeleteCancel)
        return;
    m_props->removeStand(sid);
    m_props->recomputeStandMounts();
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshFixtureBindings();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotSceneryRemoveRequested(quint32 sid)
{
    Scenery *s = m_props->scenery(sid);
    if (s == NULL)
        return;
    // Nothing is mounted on scenery, so there are no fixtures to offer.
    const FeatureDeleteChoice choice = confirmFeatureDelete(Scenery::kindName(s->kind()), s->name(),
                                                            QList<quint32>());
    if (choice == DeleteCancel)
        return;
    m_props->removeScenery(sid);
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotTowerRemoveRequested(quint32 tid)
{
    Tower *tw = m_props->tower(tid);
    if (tw == NULL)
        return;
    const QList<quint32> fids = fixturesOnFeature(TowerFeature, tid);
    const FeatureDeleteChoice choice = confirmFeatureDelete(tr("Tower"), tw->name(), fids);
    if (choice == DeleteCancel)
        return;
    if (choice == DeleteWithFixtures)
    {
        foreach (quint32 fid, fids) m_graphicsView->removeFixture(fid);
    }
    m_props->removeTower(tid);
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshFixtureBindings();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotAddImage()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose Image"),
        QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
    if (file.isEmpty())
        return;

    const quint32 id = m_props->addImage(file);
    MonitorProperties::MonitorImage img = m_props->image(id);
    img.layerId = m_props->activeLayerId();
    // Default a Floor decal at a sensible size.
    img.plane   = MonitorProperties::MonitorImage::Floor;
    img.width   = qMin(3.0f, float(m_props->gridSize().x()));
    img.height  = qMin(3.0f, float(m_props->gridSize().z()));
    // Centre it on the right-click point (m_pendingAddScenePos), like the other
    // "Add … here" actions; falls back to (0,0) from the toolbar Add menu.
    const QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    img.originX = qMax(0.0f, float(mm.x() / 1000.0) - img.width / 2.0f);
    img.originY = qMax(0.0f, float(mm.y() / 1000.0) - img.height / 2.0f);
    m_props->setImage(img);

    m_graphicsView->updateImages();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    // Open the editor so the user can set plane / size straight away.
    slotEditImage(id);
}

void Monitor::slotAddToInventory()
{
    const QList<quint32> made = FixtureCreator::addFromDialog(this, m_doc, true,
                                                              InputOutputMap::invalidUniverse(), NULL);
    if (!made.isEmpty() && m_inventoryPanel != NULL)
    {
        if (m_inventoryAction != NULL)
            m_inventoryAction->setChecked(true);
        m_inventoryPanel->reload();
    }
}

void Monitor::slotFixturesDropped(const QList<quint32> &fixtureIds, QPointF scenePos)
{
    if (m_props->layoutLocked())
    {
        QMessageBox::information(this, tr("Plot locked"),
            tr("Unlock the plot (Edit Plot) before placing fixtures -- a fixture "
               "placed on a locked plot can't be positioned."));
        return;
    }

    // Same placement as Add > Fixture, but at the drop point. Several at once
    // fan out sideways so they don't land in a single stack; the release logic
    // below then mounts each one on whatever truss/riser/tower it landed on.
    const QPointF base = m_graphicsView->pixelsToRealPosition(scenePos.x(), scenePos.y());
    int n = 0;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == NULL || m_graphicsView->fixturesID().contains(fid))
            continue;
        const QPointF mm(base.x() + n * 400.0, base.y());
        m_graphicsView->addFixture(fid, mm);
        m_props->setFixturePosition(fid, 0, 0, QVector3D(mm.x(), mm.y(), 0));
        m_props->setFixtureFlags(fid, 0, 0, 0);
        m_props->setFixtureLayer(fid, m_props->activeLayerId());
        m_graphicsView->finishFixtureDrop(fid);
        n++;
    }
    if (n > 0)
        m_doc->setModified();
    if (m_layersPanel) m_layersPanel->reload();
    m_graphicsView->refreshFixtureLabels();
    if (m_inventoryPanel) m_inventoryPanel->reload();
}

void Monitor::slotEditImage(quint32 id)
{
    if (!m_props->hasImage(id))
        return;
    RigEditSession edit(this);
    MonitorProperties::MonitorImage img = m_props->image(id);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toD  = isFeet ? 3.28084 : 1.0;
    const double frD  = isFeet ? (1.0 / 3.28084) : 1.0;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Image — %1").arg(img.name));
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(img.name);
    form->addRow(tr("Name:"), nameEdit);

    QLabel *srcLabel = new QLabel(QFileInfo(img.source).fileName());
    srcLabel->setToolTip(img.source);
    QPushButton *srcBtn = new QPushButton(tr("Change…"));
    QString chosenSource = img.source;
    std::function<void()> applyImage;      // set below, once its fields exist
    connect(srcBtn, &QPushButton::clicked, this, [&]() {
        const QString f = QFileDialog::getOpenFileName(&dlg, tr("Choose Image"),
            QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
        if (!f.isEmpty())
        {
            chosenSource = f;
            srcLabel->setText(QFileInfo(f).fileName());
            if (applyImage) applyImage();
        }
    });
    {
        QWidget *row = new QWidget; QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(srcLabel, 1); rl->addWidget(srcBtn);
        form->addRow(tr("Image:"), row);
    }

    QComboBox *planeCb = new QComboBox;
    planeCb->addItem(tr("Floor (2D Top)"),      int(MonitorProperties::MonitorImage::Floor));
    planeCb->addItem(tr("Front backdrop"),      int(MonitorProperties::MonitorImage::FrontBackdrop));
    planeCb->addItem(tr("Side backdrop"),       int(MonitorProperties::MonitorImage::SideBackdrop));
    planeCb->setCurrentIndex(planeCb->findData(img.plane));
    planeCb->setToolTip(tr("Which view this image belongs to: Floor shows in 2D Top, "
                           "Front/Side show in the matching elevation view."));
    form->addRow(tr("Plane:"), planeCb);

    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *s = new FeetInchesSpinBox(isFeet);
        s->setRange(lo, hi); s->setDecimals(2);
        s->setValue(val * toD); return s;
    };
    QDoubleSpinBox *wSpin = mkSpin(img.width,  0.05, 200);
    QDoubleSpinBox *hSpin = mkSpin(img.height, 0.05, 200);
    QDoubleSpinBox *xSpin = mkSpin(img.originX, -200, 200);
    QDoubleSpinBox *ySpin = mkSpin(img.originY, -200, 200);
    QDoubleSpinBox *zSpin = mkSpin(img.originZ, -50, 50);
    QDoubleSpinBox *rotSpin = new QDoubleSpinBox;
    rotSpin->setRange(-360, 360); rotSpin->setDecimals(1);
    rotSpin->setSuffix(QString::fromUtf8("°"));
    rotSpin->setValue(double(img.rotation));
    rotSpin->setToolTip(tr("Rotate the image clockwise about its centre"));
    form->addRow(tr("Width:"), wSpin);
    form->addRow(tr("Height:"), hSpin);
    form->addRow(tr("Rotation:"), rotSpin);
    form->addRow(tr("X (stage left):"), xSpin);
    form->addRow(tr("Y (downstage):"), ySpin);
    form->addRow(tr("Z (bottom, elevation):"), zSpin);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    // Live: the image follows its fields; Cancel puts it back.
    applyImage = [&]() {
        MonitorProperties::MonitorImage now = img;
        now.name    = nameEdit->text().trimmed().isEmpty() ? img.name : nameEdit->text().trimmed();
        now.source  = chosenSource;
        now.plane   = planeCb->currentData().toInt();
        now.width   = float(wSpin->value() * frD);
        now.height  = float(hSpin->value() * frD);
        now.rotation = float(rotSpin->value());
        now.originX = float(xSpin->value() * frD);
        now.originY = float(ySpin->value() * frD);
        now.originZ = float(zSpin->value() * frD);
        m_props->setImage(now);
        m_graphicsView->updateImages();
    };
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [&](const QString &) { applyImage(); });
    connect(planeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { applyImage(); });
    for (QDoubleSpinBox *sp : { wSpin, hSpin, xSpin, ySpin, zSpin, rotSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double) { applyImage(); });

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }
    applyImage();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotAddNetworkNode()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Network Node"));
    QFormLayout *form = new QFormLayout(&dlg);

    /* From a model: the gear's transport and ports filled in (the user's
       own models first, then the built-in list), or Custom to set it up by
       hand as before. */
    const QList<NetworkGearModel> gear = NetworkGearCatalog::all();
    QComboBox *modelCb = new QComboBox(&dlg);
    modelCb->setObjectName("networkGearModel");
    modelCb->addItem(tr("Custom — set up by hand"), -1);
    for (int i = 0; i < gear.size(); ++i)
    {
        const NetworkGearModel &m = gear.at(i);
        modelCb->addItem(tr("%1 — %2 %3, %4").arg(m.displayName())
                             .arg(m.ports)
                             .arg(m.transport == NetworkNode::WirelessDMX
                                      ? (m.ports == 1 ? tr("universe") : tr("universes"))
                                      : (m.ports == 1 ? tr("port") : tr("ports")))
                             .arg(NetworkNode::transportName(m.transport))
                             + (m.builtIn ? QString() : tr("  (yours)")),
                         i);
    }
    modelCb->setToolTip(tr("Your own models live in %1 as JSON files; "
                           "Save as Gear Model… on a node's menu adds one.")
                        .arg(NetworkGearCatalog::userDirectory().absolutePath()));
    form->addRow(tr("Model:"), modelCb);

    QLineEdit *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("e.g. SR ArtNet Node"));
    form->addRow(tr("Name:"), nameEdit);

    QComboBox *transportCb = new QComboBox(&dlg);
    transportCb->setObjectName("networkTransport");
    transportCb->addItem(tr("ArtNet"), int(NetworkNode::ArtNet));
    transportCb->addItem(tr("sACN"), int(NetworkNode::sACN));
    transportCb->addItem(tr("Interface"), int(NetworkNode::Interface));
    transportCb->addItem(tr("Wireless DMX"), int(NetworkNode::WirelessDMX));
    form->addRow(tr("Transport:"), transportCb);

    auto modelChosen = [&gear, modelCb, nameEdit, transportCb]() {
        const int gi = modelCb->currentData().toInt();
        const bool custom = gi < 0 || gi >= gear.size();
        transportCb->setEnabled(custom);
        if (custom)
        {
            nameEdit->setPlaceholderText(tr("e.g. SR ArtNet Node"));
            return;
        }
        transportCb->setCurrentIndex(transportCb->findData(gear.at(gi).transport));
        nameEdit->setPlaceholderText(gear.at(gi).model);
    };
    connect(modelCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, modelChosen);

    // Pre-fill position from the right-click point, like Add Truss/Add Image.
    QPointF pendMm = m_pendingAddScenePos.isNull()
                     ? QPointF(0, 0)
                     : m_graphicsView->pixelsToRealPosition(
                           m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    m_pendingAddScenePos = QPointF();

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posRange = isFeet ? 164.0 : 50.0;

    QDoubleSpinBox *xSpin = new FeetInchesSpinBox(isFeet, &dlg);
    xSpin->setRange(-posRange, posRange); xSpin->setDecimals(2);
    xSpin->setValue(pendMm.x() / 1000.0 * toDisp);
    form->addRow(tr("X (stage left):"), xSpin);

    QDoubleSpinBox *ySpin = new FeetInchesSpinBox(isFeet, &dlg);
    ySpin->setRange(-posRange, posRange); ySpin->setDecimals(2);
    ySpin->setValue(pendMm.y() / 1000.0 * toDisp);
    form->addRow(tr("Y (downstage):"), ySpin);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    NetworkTopology *nt = m_doc->networkTopology();
    NetworkNode node;
    const int gi = modelCb->currentData().toInt();
    if (gi >= 0 && gi < gear.size())
    {
        // Named as typed, else after the model -- numbered when there are two.
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty())
        {
            name = gear.at(gi).model;
            int n = 1;
            for (const NetworkNode &other : nt->nodes())
                if (other.name == name || other.name.startsWith(name + " #"))
                    ++n;
            if (n > 1)
                name = QString("%1 #%2").arg(gear.at(gi).model).arg(n);
        }
        node = NetworkGearCatalog::makeNode(gear.at(gi), name);
    }
    else
    {
        node.name = nameEdit->text().trimmed().isEmpty()
                    ? tr("Node %1").arg(nt->nodes().size() + 1) : nameEdit->text().trimmed();
        node.transport = transportCb->currentData().toInt();
    }
    node.placed = true;
    node.posX = xSpin->value() * fromDisp;
    node.posY = ySpin->value() * fromDisp;
    node.layerId = m_props->activeLayerId();   // land on the selected layer
    nt->nodes().append(node);

    m_graphicsView->updateNetworkNodes();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    // Straight on to its ports: giving a hand-built node some, or a model's
    // their universes.
    slotEditNetworkNode(quint32(nt->nodes().size() - 1));
    // A wireless transmitter's receivers go where its fixtures are (or beside
    // it), even if the ports were left for later.
    m_graphicsView->placeWirelessReceivers();
}

void Monitor::slotEditNetworkNode(quint32 nodeIndex)
{
    /* Like every rig dialog: the plot follows the edits as they're made,
       Cancel puts the network back, OK is one undo step. */
    RigEditSession edit(this);
    NetworkTopologyDialog dlg(m_doc, int(nodeIndex), this);
    connect(&dlg, &NetworkTopologyDialog::changed, this, [this]() {
        m_graphicsView->updateNetworkNodes();
    });
    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        m_graphicsView->updateNetworkNodes();
        return;
    }
    // Nodes are keyed to map items by index and the dialog may have added,
    // removed or reordered them, so rebuild everything from the settled list.
    m_graphicsView->updateNetworkNodes();
    /* "If I add a wDMX transmitter .. shouldn't there be a wDMX receiver
       somewhere for the fixtures?" Every wireless port has one; it just had
       no place until someone typed it in. Now it is put near the fixtures
       on its universe, and can be dragged from there. */
    m_graphicsView->placeWirelessReceivers();
    if (m_layersPanel) m_layersPanel->reload();
}

void Monitor::slotNetworkReport()
{
    NetworkReportDialog dlg(m_doc, this);
    dlg.exec();
}

void Monitor::slotNetworkNodeRemoveRequested(quint32 nodeIndex)
{
    NetworkTopology *nt = m_doc->networkTopology();
    if (int(nodeIndex) >= nt->nodes().size())
        return;
    if (QMessageBox::question(this, tr("Delete Network Node"),
            tr("Delete network node '%1'?").arg(nt->nodes().at(int(nodeIndex)).name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    // Index-based, like PowerDistribution's sources() -- safe because every
    // mutation here is immediately followed by a full rebuild (updateNetworkNodes()),
    // so no later index reference outlives this removal.
    nt->nodes().removeAt(int(nodeIndex));
    m_graphicsView->updateNetworkNodes();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotImageRemoveRequested(quint32 id)
{
    if (!m_props->hasImage(id))
        return;
    if (QMessageBox::question(this, tr("Remove Image"),
            tr("Remove image '%1'?").arg(m_props->image(id).name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    m_props->removeImage(id);
    m_graphicsView->updateImages();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotPlatformDoubleClicked(quint32 pid)
{
    slotEditPlatform(pid);
}

void Monitor::slotTargetDoubleClicked(quint32 tid)
{
    slotEditTarget(tid);
}

void Monitor::slotAddTarget()
{
    addTarget(false);
}

void Monitor::slotAddTargetLine()
{
    addTarget(true);
}

void Monitor::addTarget(bool line)
{
    StageTarget *t = m_props->addStageTarget();
    if (line)
    {
        t->makeLine();
        int n = 0;
        foreach (StageTarget *other, m_props->stageTargets())
            if (other != nullptr && other->isLine())
                n++;
        t->setName(tr("Line %1").arg(n));
    }
    else
    {
        t->setName(tr("Target %1").arg(t->id() + 1));
    }

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    t->setX(float(mm.x() / 1000.0));
    t->setY(float(mm.y() / 1000.0));
    // Sit on whatever solid platform it was dropped over, not at deck level --
    // same rule a drag follows, so placing and moving agree.
    t->setZ(m_graphicsView->floorHeightAt(t->x(), t->y()));
    t->setLayerId(m_props->activeLayerId());   // land on the selected layer

    m_graphicsView->updateTargets();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    // Open the edit dialog; if confirmed, auto-create a linked PanTilt palette.
    quint32 tid = t->id();
    slotEditTarget(tid);

    // slotEditTarget returns after the dialog. Re-fetch in case cancelled/renamed.
    StageTarget *updated = m_props->stageTarget(tid);
    if (!updated)
        return;   // user may have removed it (shouldn't happen, but guard anyway)

    /* Auto-create an AIM palette linked to this target, unless one already
       exists. It was creating a PanTilt palette, which is the wrong thing in
       two ways: a PanTilt palette carries fixed pan/tilt values, so it does
       not follow the target when the target moves, and nothing that looks for
       "what aims at this target" -- the canvas glow, the light-path lines --
       recognises it. An Aim palette resolves per fixture from the rig geometry,
       which is the whole reason StageTarget exists. */
    bool alreadyLinked = false;
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p && p->stageTargetId() == tid && p->type() == QLCPalette::Aim)
        { alreadyLinked = true; break; }
    }
    if (!alreadyLinked)
    {
        QLCPalette *pal = new QLCPalette(QLCPalette::Aim);
        pal->setName(updated->name());
        // Aim carries no values of its own: the target IS the value, and each
        // fixture solves its own pan/tilt from where it sits on the rig.
        pal->setStageTargetId(tid);
        m_doc->addPalette(pal);
        m_doc->setModified();
    }
}

void Monitor::slotAddPerson()
{
    StageTarget *t = m_props->addStageTarget();
    t->setKind(StageTarget::Kind::Person);
    t->setPersonHeight(StageTarget::defaultPersonHeight());   // drawn this tall, aimed at its chest

    /* Numbered among the people, not among the targets: "Person 1" next to
       "Target 4" is how a choir ends up impossible to read. */
    int n = 0;
    foreach (StageTarget *other, m_props->stageTargets())
        if (other != nullptr && other->kind() == StageTarget::Kind::Person)
            n++;
    t->setName(tr("Person %1").arg(n));

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    t->setX(float(mm.x() / 1000.0));
    t->setY(float(mm.y() / 1000.0));
    t->setZ(m_graphicsView->floorHeightAt(t->x(), t->y()));   // stand on the deck
    t->setLayerId(m_props->activeLayerId());

    m_graphicsView->updateTargets();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotEditTarget(quint32 tid)
{
    StageTarget *t = m_props->stageTarget(tid);
    if (!t) return;
    RigEditSession edit(this);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Target — %1").arg(t->name()));
    dlg.setMinimumWidth(320);

    QVBoxLayout *vl   = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    const bool isFeet_t = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp_t   = isFeet_t ? 3.28084 : 1.0;
    const double fromDisp_t = isFeet_t ? (1.0 / 3.28084) : 1.0;
    const double posRange_t = isFeet_t ? 164.0 : 50.0;   // ~50m in feet

    QDoubleSpinBox *xSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    xSpin->setRange(-posRange_t, posRange_t); xSpin->setDecimals(2);
    xSpin->setValue(double(t->x()) * toDisp_t);
    form->addRow(tr("X (stage left):"), xSpin);

    QDoubleSpinBox *ySpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    ySpin->setRange(-posRange_t, posRange_t); ySpin->setDecimals(2);
    ySpin->setValue(double(t->y()) * toDisp_t);
    form->addRow(tr("Y (downstage):"), ySpin);

    QDoubleSpinBox *zSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    zSpin->setRange(0, isFeet_t ? 65.6 : 20.0); zSpin->setDecimals(2);
    zSpin->setValue(double(t->z()) * toDisp_t);
    zSpin->setToolTip(tr("ABSOLUTE aim height above the stage floor for this "
                         "target (used by Pan/Tilt aim palettes). 0 = floor."));
    form->addRow(tr("Z (height):"), zSpin);

    // Follow-spot subject/user height — when a follow spot drives a fixture it
    // aims at this height above the platform the subject stands on, OVERRIDING
    // the target Z so the beam tracks the body across risers. Per-workspace.
    QDoubleSpinBox *subjSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    subjSpin->setRange(0, isFeet_t ? 32.8 : 10.0); subjSpin->setDecimals(2);
    subjSpin->setValue(double(m_props->aimSubjectHeight()) * toDisp_t);
    subjSpin->setToolTip(tr("Where a follow spot aims above the platform/deck -- a "
                            "chest, not how tall anyone is (overrides the target Z "
                            "when a follow spot drives the fixture). ~1.4 m hits the "
                            "chest. A Person is aimed at 80% of its own Height "
                            "instead."));
    form->addRow(tr("Follow-spot aim height (chest):"), subjSpin);

    /* WHAT this target is. A kind changes where light is aimed and what is
       drawn at the point -- a drum kit is not a vocalist, and neither is a
       mark on the deck. */
    QComboBox *kindCombo = new QComboBox(&dlg);
    kindCombo->addItem(tr("None — a mark on the deck"),
                       int(StageTarget::Kind::None));
    kindCombo->addItem(tr("Person"),        int(StageTarget::Kind::Person));
    kindCombo->addItem(tr("Drum kit"),      int(StageTarget::Kind::DrumKit));
    kindCombo->addItem(tr("Bound to a structure"),
                       int(StageTarget::Kind::Structure));
    kindCombo->addItem(tr("Area"),          int(StageTarget::Kind::Area));
    kindCombo->addItem(tr("Line — lights spread along it"),
                       int(StageTarget::Kind::Line));
    kindCombo->setCurrentIndex(kindCombo->findData(int(t->kind())));
    kindCombo->setToolTip(tr("What is standing here. Decides the height light "
                             "is aimed at and what is drawn in the rig view."));
    form->addRow(tr("Subject:"), kindCombo);

    /* How tall a Person is: drawn this tall, and aimed at the chest (80% of
       it). */
    QDoubleSpinBox *personSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    personSpin->setObjectName(QStringLiteral("personHeight"));
    personSpin->setRange(0.5 * toDisp_t, 3.0 * toDisp_t);
    personSpin->setDecimals(2);
    personSpin->setValue(double(t->effectivePersonHeight()) * toDisp_t);
    personSpin->setToolTip(tr("How tall this person is. They are drawn this tall, and "
                              "light is aimed at their chest -- 80% of it."));
    form->addRow(tr("Height:"), personSpin);

    /* What it FOLLOWS. A bound target takes its position from the structure
       every time it is read, so moving or restacking the step takes the target
       with it -- which is what actually happens between load-ins. */
    QComboBox *boundCombo = new QComboBox(&dlg);
    boundCombo->addItem(tr("(not bound)"), QString());
    foreach (StagePlatform *pl, m_props->platforms())
        if (pl != nullptr)
            boundCombo->addItem(tr("Platform — %1").arg(pl->name()),
                                QString("platform:%1").arg(pl->id()));
    foreach (Truss *tr_ , m_props->trusses())
        if (tr_ != nullptr)
            boundCombo->addItem(tr("Truss — %1").arg(tr_->name()),
                                QString("truss:%1").arg(tr_->id()));
    foreach (Tower *tw, m_props->towers())
        if (tw != nullptr)
            boundCombo->addItem(tr("Tower — %1").arg(tw->name()),
                                QString("tower:%1").arg(tw->id()));
    if (t->isBound())
    {
        const int ix = boundCombo->findData(QString("%1:%2")
                                            .arg(t->boundType()).arg(t->boundId()));
        if (ix >= 0)
            boundCombo->setCurrentIndex(ix);
    }
    form->addRow(tr("Follows:"), boundCombo);

    /* Per-target aim height. One override is what stops the Subject list
       needing a new entry for every one-off. */
    QCheckBox *ovrChk = new QCheckBox(tr("Override aim height"), &dlg);
    ovrChk->setChecked(t->hasAimHeightOverride());
    QDoubleSpinBox *ovrSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    ovrSpin->setRange(0, isFeet_t ? 32.8 : 10.0);
    ovrSpin->setDecimals(2);
    ovrSpin->setValue(double(m_props->targetAimHeight(t, true)) * toDisp_t);
    ovrSpin->setEnabled(ovrChk->isChecked());
    connect(ovrChk, &QCheckBox::toggled, ovrSpin, &QWidget::setEnabled);
    ovrSpin->setToolTip(tr("How far above whatever it is standing on to aim. "
                           "Unticked, the Subject decides."));
    QHBoxLayout *ovrRow = new QHBoxLayout;
    ovrRow->addWidget(ovrChk);
    ovrRow->addWidget(ovrSpin);
    form->addRow(tr("Aim height:"), ovrRow);

    /* Footprint. A drum kit is not a point, and neither is an area: this is
       what lets the view show whether a beam actually COVERS the thing. */
    QDoubleSpinBox *wSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    wSpin->setRange(0, isFeet_t ? 98.0 : 30.0);
    wSpin->setDecimals(2);
    wSpin->setValue(t->footprint().width() * toDisp_t);
    QDoubleSpinBox *dSpin = new FeetInchesSpinBox(isFeet_t, &dlg);
    dSpin->setRange(0, isFeet_t ? 98.0 : 30.0);
    dSpin->setDecimals(2);
    dSpin->setValue(t->footprint().height() * toDisp_t);
    QHBoxLayout *sizeRow = new QHBoxLayout;
    sizeRow->addWidget(wSpin);
    sizeRow->addWidget(new QLabel(tr("x"), &dlg));
    sizeRow->addWidget(dSpin);
    form->addRow(tr("Size (W x D):"), sizeRow);

    /* Only the rows that mean something for the chosen Subject. */
    auto syncRows = [&]() {
        const StageTarget::Kind k =
            StageTarget::Kind(kindCombo->currentData().toInt());
        boundCombo->setEnabled(k == StageTarget::Kind::Structure);
        const bool sized = (k == StageTarget::Kind::DrumKit
                            || k == StageTarget::Kind::Area);
        wSpin->setEnabled(sized);
        dSpin->setEnabled(sized);
        personSpin->setEnabled(k == StageTarget::Kind::Person);
        // A line's points carry their own heights.
        ovrChk->setEnabled(k != StageTarget::Kind::Line);
        ovrSpin->setEnabled(k != StageTarget::Kind::Line && ovrChk->isChecked());
        /* Unticked, show what the Subject gives -- which for a Person is the
           chest of its height, or the follow-spot subject height above. */
        if (ovrChk->isChecked() == false)
            ovrSpin->setValue(k != StageTarget::Kind::Person
                              ? double(StageTarget::defaultAimHeight(k)) * toDisp_t
                              : personSpin->value() * double(StageTarget::chestShare()));
    };
    connect(kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dlg, [&](int) {
        syncRows();
    });
    connect(personSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            &dlg, [&](double) { syncRows(); });
    connect(subjSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            &dlg, [&](double) { syncRows(); });
    syncRows();

    QColor curColor = t->color().isValid() ? t->color() : QColor(255, 180, 0);
    ColourSwatchButton *colorBtn = new ColourSwatchButton(curColor, tr("Target colour"), &dlg);
    colorBtn->setResetColour(QColor(255, 180, 0));
    QColor chosenColor = curColor;
    connect(colorBtn, &ColourSwatchButton::colourChanged, &dlg, [&](const QColor &c) { chosenColor = c; });
    form->addRow(tr("Colour:"), colorBtn);

    vl->addLayout(form);
    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vl->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto apply = [&]() {
        QString newName = nameEdit->text().trimmed();
        if (!newName.isEmpty()) t->setName(newName);
        t->setX(float(xSpin->value() * fromDisp_t));
        t->setY(float(ySpin->value() * fromDisp_t));
        t->setZ(float(zSpin->value() * fromDisp_t));
        t->setColor(chosenColor);
        m_props->setAimSubjectHeight(float(subjSpin->value() * fromDisp_t));
        // Position and subject height both change what a head can reach.
        m_props->notifyStageTargetsChanged();

        t->setKind(StageTarget::Kind(kindCombo->currentData().toInt()));
        if (t->isLine())
            t->makeLine();          // a point turned into a line gets a 4 m one
        if (t->kind() == StageTarget::Kind::Person)
            t->setPersonHeight(float(personSpin->value() * fromDisp_t));
        const QString bound = boundCombo->currentData().toString();
        if (t->kind() == StageTarget::Kind::Structure && bound.isEmpty() == false)
        {
            const QStringList parts = bound.split(QLatin1Char(':'));
            if (parts.size() == 2)
                t->setBinding(parts.at(0), parts.at(1).toUInt());
        }
        else
        {
            t->clearBinding();
        }
        t->setAimHeightOverride(ovrChk->isChecked()
                                ? float(ovrSpin->value() * fromDisp_t) : -1.0f);
        t->setFootprint(QSizeF(wSpin->value() * fromDisp_t,
                               dSpin->value() * fromDisp_t));
        m_graphicsView->updateTargets();
    };
    // Live: the target follows its fields; Cancel puts it back.
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [&](const QString &) { apply(); });
    for (QDoubleSpinBox *sp : { xSpin, ySpin, zSpin, subjSpin, personSpin, ovrSpin, wSpin, dSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double) { apply(); });
    for (QComboBox *cb : { kindCombo, boundCombo })
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { apply(); });
    connect(ovrChk, &QCheckBox::toggled, &dlg, [&](bool) { apply(); });
    connect(colorBtn, &ColourSwatchButton::colourChanged, &dlg, [&](const QColor &) { apply(); });

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }
    apply();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotAddPipe()
{
    Pipe *b = m_props->addPipe();
    b->setName(tr("Boom %1").arg(b->id() + 1));

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    b->setOriginX(float(mm.x() / 1000.0));
    b->setOriginY(float(mm.y() / 1000.0));
    b->setLayerId(m_props->activeLayerId());

    m_graphicsView->updatePlatforms();   // rebuilds platforms + pipes
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    slotEditPipe(b->id());
}

void Monitor::slotAddStand()
{
    Stand *s = m_props->addStand();
    s->setName(tr("Stand %1").arg(s->id() + 1));
    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    s->setOriginX(float(mm.x() / 1000.0));
    s->setOriginY(float(mm.y() / 1000.0));
    s->setLayerId(m_props->activeLayerId());
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    slotEditStand(s->id());
}

void Monitor::slotEditStand(quint32 sid)
{
    Stand *s = m_props->stand(sid);
    if (!s) return;
    RigEditSession edit(this);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 33.0 : 10.0;


    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — Stand %1").arg(s->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(s->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *sp = new FeetInchesSpinBox(isFeet, &dlg);
        sp->setRange(lo, hi); sp->setDecimals(2);
        sp->setValue(val * toDisp); return sp;
    };
    QDoubleSpinBox *oxSpin = mkSpin(s->originX(), -posR, posR);
    QDoubleSpinBox *oySpin = mkSpin(s->originY(), -posR, posR);
    QDoubleSpinBox *hSpin  = mkSpin(s->height(), 0.1, hMax);
    QDoubleSpinBox *brSpin = mkSpin(s->baseRadius(), 0.05, isFeet ? 6.6 : 2.0);
    QDoubleSpinBox *rotSpin = new QDoubleSpinBox(&dlg);
    rotSpin->setRange(-180, 180); rotSpin->setDecimals(0); rotSpin->setSuffix(QStringLiteral("°"));
    rotSpin->setValue(double(s->rotation()));
    rotSpin->setToolTip(tr("Orientation of the legs on the floor — spin so they "
                           "tuck under a riser. Doesn't re-aim what's mounted."));
    form->addRow(tr("Position X:"), oxSpin);
    form->addRow(tr("Position Y:"), oySpin);
    form->addRow(tr("Top height:"), hSpin);
    form->addRow(tr("Base radius:"), brSpin);
    form->addRow(tr("Leg rotation:"), rotSpin);
    vl->addLayout(form);
    vl->addStretch();

    // Canvas-centric object editor body (tree + canvas + inspector) with the
    // geometry form as its collapsible left section; live preview as fields change.
    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 0 /*Stand*/, sid, geomW, &view);
    auto applyLive = [&]() {
        s->setName(nameEdit->text());
        s->setOriginX(float(oxSpin->value() * fromDisp));
        s->setOriginY(float(oySpin->value() * fromDisp));
        s->setHeight(float(hSpin->value() * fromDisp));
        s->setBaseRadius(float(brSpin->value() * fromDisp));
        s->setRotation(float(rotSpin->value()));
        m_props->recomputeStandMounts();
        if (view) view->reload();
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, hSpin, brSpin, rotSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [applyLive](double){ applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);
    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }

    s->setName(nameEdit->text());
    s->setOriginX(float(oxSpin->value() * fromDisp));
    s->setOriginY(float(oySpin->value() * fromDisp));
    s->setHeight(float(hSpin->value() * fromDisp));
    s->setBaseRadius(float(brSpin->value() * fromDisp));
    s->setRotation(float(rotSpin->value()));

    m_props->recomputeStandMounts();     // pipes/trusses on this stand follow
    m_graphicsView->updatePlatforms();
    m_graphicsView->updateTrusses();     // a truss standing on it moved
    foreach (Fixture *fx, m_doc->fixtures())
        if (m_props->fixtureRigProps(fx->id()).onPipe())
            m_graphicsView->updateFixture(fx->id());
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotAddCyclorama()
{
    slotAddScenery(Scenery::Cyclorama);
}

void Monitor::slotAddScenery(int kind)
{
    const double mpu = (m_props->gridUnits() == MonitorProperties::Feet) ? 0.3048 : 1.0;
    const double stageW = double(m_props->gridSize().x()) * mpu;
    const double stageD = double(m_props->gridSize().z()) * mpu;
    const bool atClick = !m_pendingAddScenePos.isNull();
    QPointF click;
    if (atClick)
    {
        const QPointF mm = m_graphicsView->pixelsToRealPosition(
            m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
        click = QPointF(mm.x() / 1000.0, mm.y() / 1000.0);
        m_pendingAddScenePos = QPointF();
    }
    const float fullW = float(qMax(2.0, stageW - 1.0));

    /* Where each usually goes: the cyc and back wall across the back, the
       main drape across the front, a traveler mid-stage, legs and a border
       two-thirds of the way down, a scrim just in front of the cyc. */
    auto make = [&](double x, double y) {
        Scenery *s = m_props->addScenery();
        s->setKind(kind);
        s->setColor(Scenery::defaultColor(kind));
        s->setName(tr("%1 %2").arg(Scenery::kindName(kind)).arg(s->id() + 1));
        s->setOriginX(float(atClick ? click.x() : x));
        s->setOriginY(float(atClick ? click.y() : y));
        s->setWidth(fullW);
        s->setHeight(6.0f);
        s->setLayerId(m_props->activeLayerId());
        return s;
    };
    Scenery *s = nullptr;
    switch (kind)
    {
        case Scenery::BackWall:
            s = make(stageW / 2.0, 0.1);
            s->setThickness(0.2f);
            break;
        case Scenery::MainDrape:
            s = make(stageW / 2.0, qMax(1.0, stageD - 0.5));
            break;
        case Scenery::Proscenium:
            // Framing the stage at its front: the opening the stage's width.
            s = make(stageW / 2.0, qMax(1.0, stageD - 0.2));
            s->setWidth(float(stageW + 2.0));
            s->setHeight(7.0f);
            s->setThickness(0.4f);
            s->setOpeningWidth(float(qMax(2.0, stageW - 0.5)));
            s->setOpeningHeight(5.5f);
            break;
        case Scenery::Traveler:
            s = make(stageW / 2.0, stageD / 2.0);
            break;
        case Scenery::Leg:
        {
            // A pair, one each side, unless it was put somewhere by hand.
            s = make(0.6, stageD * 0.66);
            s->setWidth(1.2f);
            if (!atClick)
            {
                Scenery *other = make(stageW - 0.6, stageD * 0.66);
                other->setWidth(1.2f);
            }
            break;
        }
        case Scenery::Border:
            s = make(stageW / 2.0, stageD * 0.66);
            s->setHeight(1.2f);
            s->setBottom(4.8f);
            break;
        case Scenery::Scrim:
            s = make(stageW / 2.0, 1.0);
            break;
        case Scenery::Wall:
            s = make(stageW / 2.0, stageD / 2.0);
            s->setWidth(1.0f);
            s->setHeight(0.5f);
            s->setThickness(0.1f);
            break;
        case Scenery::Cyclorama:
        default:
            // Across the back: centred, at the upstage edge of the stage.
            s = make(stageW / 2.0, 0.3);
            s->setName(tr("Cyclorama %1").arg(s->id() + 1));
            break;
    }
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    slotEditScenery(s->id());
}

void Monitor::slotAddFountain()
{
    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double mpu = isFeet ? 0.3048 : 1.0;
    const double stageW = double(m_props->gridSize().x()) * mpu;
    const double stageD = double(m_props->gridSize().z()) * mpu;
    QPointF centre(stageW / 2.0, qMax(1.0, stageD - 1.5));
    if (!m_pendingAddScenePos.isNull())
    {
        const QPointF mm = m_graphicsView->pixelsToRealPosition(
            m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
        centre = QPointF(mm.x() / 1000.0, mm.y() / 1000.0);
        m_pendingAddScenePos = QPointF();
    }

    /* The sizes, as built: 3 ft across, 4 ft deep with a round front; a 5"
       base; walls of 4" blocks, the outer 8" high and the inner 14". */
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Fountain"));
    QFormLayout *form = new QFormLayout;
    auto mkSpin = [&](double metres, const char *name, const QString &tip) {
        QDoubleSpinBox *sp = new FeetInchesSpinBox(isFeet, &dlg);
        sp->setRange(0.01, isFeet ? 33.0 : 10.0);
        sp->setDecimals(3);
        sp->setValue(metres * toDisp);
        sp->setObjectName(QString::fromLatin1(name));
        sp->setToolTip(tip);
        return sp;
    };
    QDoubleSpinBox *wSpin = mkSpin(0.9144, "fountainWidth", tr("Across, outside to outside"));
    QDoubleSpinBox *dSpin = mkSpin(1.2192, "fountainDepth", tr("From the back to the front of the curve"));
    QDoubleSpinBox *baseSpin = mkSpin(0.127, "fountainBase", tr("The base it stands on"));
    QDoubleSpinBox *outerSpin = mkSpin(0.2032, "fountainOuter", tr("The outer wall, above the base"));
    QDoubleSpinBox *innerSpin = mkSpin(0.3556, "fountainInner", tr("The inner wall, above the base"));
    QDoubleSpinBox *thickSpin = mkSpin(0.1016, "fountainThickness", tr("How thick each wall is"));
    QDoubleSpinBox *gapSpin = mkSpin(0.1016, "fountainGap", tr("Between the outer wall and the inner one"));
    QCheckBox *flameChk = new QCheckBox(tr("A silk flame in the middle"), &dlg);
    flameChk->setChecked(true);
    flameChk->setObjectName("fountainFlame");
    flameChk->setToolTip(tr("A Generic Silk Flame (relay), staged: patch it to the relay's channel"));
    form->addRow(tr("Width:"), wSpin);
    form->addRow(tr("Depth:"), dSpin);
    form->addRow(tr("Base height:"), baseSpin);
    form->addRow(tr("Outer wall:"), outerSpin);
    form->addRow(tr("Inner wall:"), innerSpin);
    form->addRow(tr("Wall thickness:"), thickSpin);
    form->addRow(tr("Gap:"), gapSpin);
    form->addRow(QString(), flameChk);
    /* Beside the fields, what each one measures: the fountain from above and
       cut through its front, every value where it is taken, the field being
       edited picked out ("a side display showing what each measurement is"). */
    FountainDiagram *diagram = new FountainDiagram(&dlg);
    const QList<QPair<QString, QDoubleSpinBox *> > fields = {
        { QStringLiteral("width"), wSpin }, { QStringLiteral("depth"), dSpin },
        { QStringLiteral("base"), baseSpin }, { QStringLiteral("outer"), outerSpin },
        { QStringLiteral("inner"), innerSpin }, { QStringLiteral("thickness"), thickSpin },
        { QStringLiteral("gap"), gapSpin } };
    for (const auto &fd : fields)
    {
        const QString key = fd.first;
        QDoubleSpinBox *sp = fd.second;
        auto show = [diagram, key, sp, fromDisp]() {
            diagram->setValue(key, sp->value() * fromDisp, sp->text().trimmed());
        };
        show();
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), diagram, [show](double) { show(); });
    }
    connect(flameChk, &QCheckBox::toggled, diagram, &FountainDiagram::setFlame);
    connect(qApp, &QApplication::focusChanged, diagram, [diagram, fields](QWidget *, QWidget *now) {
        QString key;
        for (const auto &fd : fields)
            if (now != nullptr && (now == fd.second || fd.second->isAncestorOf(now)))
                key = fd.first;
        diagram->setHighlight(key);
    });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QHBoxLayout *hl = new QHBoxLayout;
    hl->addLayout(form);
    hl->addWidget(diagram, 1);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    vl->addLayout(hl);
    vl->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const float W = float(wSpin->value() * fromDisp), D = float(dSpin->value() * fromDisp);
    const float base = float(baseSpin->value() * fromDisp);
    const float t = float(thickSpin->value() * fromDisp), gap = float(gapSpin->value() * fromDisp);
    const float back = float(centre.y()) - D / 2.0f;
    const quint32 layer = m_props->activeLayerId();
    const quint32 gid = m_props->nextGroupId();
    const int n = gid;
    m_props->createGroup(gid, tr("Fountain %1").arg(n), layer, 0);

    // The base: as wide and deep as the fountain, its front a half circle.
    StagePlatform *pl = m_props->addPlatform();
    pl->setName(tr("Fountain %1 base").arg(n));
    pl->setOriginX(float(centre.x()) - W / 2.0f);
    pl->setOriginY(back);
    pl->setWidth(W);
    pl->setDepth(D);
    pl->setHeight(base);
    pl->setFrontCurve(W / 2.0f);
    pl->setColor(QColor(120, 116, 108));
    pl->setLayerId(layer);
    pl->setGroupId(gid);

    /* The walls: Us on the base, open at the back. A wall's line is its
       middle, so the outer one is half a block in from the edge and the
       inner one a block, the gap and half a block more. */
    auto wall = [&](const QString &name, float inset, float height) {
        Scenery *s = m_props->addScenery();
        s->setKind(Scenery::Wall);
        s->setName(name);
        s->setOutline(Scenery::RoundedU);
        s->setOriginX(float(centre.x()));
        s->setOriginY(back);
        s->setWidth(qMax(0.1f, W - 2.0f * inset));
        s->setDepth(qMax(0.05f, D - inset));
        s->setThickness(t);
        s->setHeight(height);
        s->setBottom(base);
        s->setColor(Scenery::defaultColor(Scenery::Wall));
        s->setLayerId(layer);
        s->setGroupId(gid);
        return s;
    };
    wall(tr("Fountain %1 outer wall").arg(n), t / 2.0f, float(outerSpin->value() * fromDisp));
    const float innerInset = t + gap + t / 2.0f;
    wall(tr("Fountain %1 inner wall").arg(n), innerInset, float(innerSpin->value() * fromDisp));

    // The flame, standing on the base in the middle of the inner wall.
    if (flameChk->isChecked())
    {
        QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(QStringLiteral("Generic"),
                                                                   QStringLiteral("Silk Flame (relay)"));
        if (def != nullptr && !def->modes().isEmpty())
        {
            Fixture *fx = new Fixture(m_doc);
            fx->setFixtureDefinition(def, def->modes().first());
            fx->setName(tr("Fountain %1 flame").arg(n));
            fx->setPatched(false);
            if (m_doc->addFixture(fx))
            {
                const QPointF at(centre.x(), double(back) + double(D - innerInset) * 0.5);
                m_props->setFixturePosition(fx->id(), 0, 0,
                                            QVector3D(float(at.x() * 1000.0), float(at.y() * 1000.0), 0.0f));
                FixtureRigProps rp = m_props->fixtureRigProps(fx->id());
                rp.deckPlatformId = pl->id();
                rp.mountingType = Truss::FloorMounted;
                m_props->setFixtureRigProps(fx->id(), rp);
                m_props->setFixtureGroup(fx->id(), gid);
                m_graphicsView->addFixture(fx->id(), QPointF(at.x() * 1000.0, at.y() * 1000.0));
            }
            else
                delete fx;
        }
    }

    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotEditScenery(quint32 sid)
{
    Scenery *s = m_props->scenery(sid);
    if (s == NULL)
        return;
    RigEditSession edit(this);
    const QString oldName = s->name();

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;

    // Snapshot for reverting the live preview on Cancel.

    QDialog dlg(this);
    dlg.setWindowTitle(tr("%1 -- %2").arg(Scenery::kindName(s->kind()), s->name()));
    QFormLayout *form = new QFormLayout;
    QLineEdit *nameEdit = new QLineEdit(s->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *sp = new FeetInchesSpinBox(isFeet, &dlg);
        sp->setRange(lo, hi); sp->setDecimals(2);
        sp->setValue(val * toDisp);
        return sp;
    };
    QDoubleSpinBox *xSpin   = mkSpin(s->originX(), -posR, posR);
    QDoubleSpinBox *ySpin   = mkSpin(s->originY(), -posR, posR);
    QDoubleSpinBox *wSpin   = mkSpin(s->width(), 0.1, posR * 2);
    QDoubleSpinBox *hSpin   = mkSpin(s->height(), 0.1, isFeet ? 100.0 : 30.0);
    QDoubleSpinBox *bSpin   = mkSpin(s->bottom(), 0.0, isFeet ? 100.0 : 30.0);
    QDoubleSpinBox *bowSpin = mkSpin(s->bow(), isFeet ? -33.0 : -10.0, isFeet ? 33.0 : 10.0);
    QDoubleSpinBox *depthSpin = mkSpin(s->depth(), 0.05, posR);
    QDoubleSpinBox *thickSpin = mkSpin(s->thickness(), 0.0, isFeet ? 10.0 : 3.0);
    thickSpin->setDecimals(3);
    // A curtain whose opening was never set shows the one it is drawn with.
    const float openNow = (s->hasState() && s->openingWidth() >= s->width()) ? 0.68f * s->width()
                                                                             : s->openingWidth();
    QDoubleSpinBox *openWSpin = mkSpin(openNow, 0.1, posR * 2);
    QDoubleSpinBox *openHSpin = mkSpin(s->openingHeight(), 0.1, isFeet ? 100.0 : 30.0);
    openWSpin->setObjectName("sceneryOpeningWidth");
    openHSpin->setObjectName("sceneryOpeningHeight");
    openWSpin->setToolTip(tr("How wide the opening is, centred: a proscenium's piers, or an open "
                             "curtain's halves gathered at its ends, are the rest of the width"));
    openHSpin->setToolTip(tr("How high the opening is; the header is the rest of the height"));
    QComboBox *kindCombo = new QComboBox(&dlg);
    foreach (int k, Scenery::kinds())
        kindCombo->addItem(Scenery::kindName(k), k);
    kindCombo->setCurrentIndex(qMax(0, kindCombo->findData(s->kind())));
    kindCombo->setObjectName("sceneryKind");
    QComboBox *outlineCombo = new QComboBox(&dlg);
    outlineCombo->addItem(tr("Straight or bowed"), int(Scenery::SheetOutline));
    outlineCombo->addItem(tr("U, rounded front"), int(Scenery::RoundedU));
    outlineCombo->setCurrentIndex(qMax(0, outlineCombo->findData(s->outline())));
    outlineCombo->setObjectName("sceneryOutline");
    outlineCombo->setToolTip(tr("A U has a half-round front as wide as it is, and straight sides "
                                "running back to its Depth -- a fountain's rim, a planter"));
    QComboBox *stateCombo = new QComboBox(&dlg);
    for (int st : { int(Scenery::Closed), int(Scenery::Open), int(Scenery::Flown) })
        stateCombo->addItem(Scenery::stateName(st), st);
    stateCombo->setCurrentIndex(qMax(0, stateCombo->findData(s->state())));
    stateCombo->setObjectName("sceneryState");
    stateCombo->setToolTip(tr("Open: split, each half gathered at its end. Flown out: taken up, "
                              "out of the way of the light"));
    /* Driven by a fixture: a curtain that opens as a channel moves. */
    QList<quint32> createdCurtains;
    QComboBox *driveCombo = new QComboBox(&dlg);
    driveCombo->setObjectName("sceneryDriver");
    driveCombo->setToolTip(tr("A curtain fixture that moves it: as its Open channel goes from 0 "
                              "to 255 the curtain opens, as its Fly channel does it flies out. "
                              "Not driven, it stays as Curtain says"));
    auto fillDrivers = [&](quint32 select) {
        const QSignalBlocker block(driveCombo);
        driveCombo->clear();
        driveCombo->addItem(tr("Nothing (stays as set)"), uint(Fixture::invalidId()));
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx != nullptr && (Scenery::isCurtainFixture(fx) || fx->id() == select))
                driveCombo->addItem(fx->name(), uint(fx->id()));
        driveCombo->setCurrentIndex(qMax(0, driveCombo->findData(uint(select))));
    };
    fillDrivers(s->driverFixture());
    QPushButton *newCurtainBtn = new QPushButton(tr("Add curtain fixture"), &dlg);
    newCurtainBtn->setObjectName("sceneryAddCurtain");
    newCurtainBtn->setToolTip(tr("Add a Generic > Stage Curtain fixture for it and drive it by that: "
                                 "use it in scenes, chasers and cues like any fixture -- the fade "
                                 "time is how long the move takes. Patch it to the curtain's "
                                 "controller or relay box to move the real one"));
    QWidget *driveRow = new QWidget(&dlg);
    {
        QHBoxLayout *hl = new QHBoxLayout(driveRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(driveCombo, 1);
        hl->addWidget(newCurtainBtn);
    }
    depthSpin->setToolTip(tr("From the open back to the front of the curve"));
    thickSpin->setToolTip(tr("How thick it is; 0 for cloth"));
    QDoubleSpinBox *rotSpin = new QDoubleSpinBox(&dlg);
    rotSpin->setRange(-180, 180); rotSpin->setDecimals(0); rotSpin->setSuffix(QStringLiteral("°"));
    rotSpin->setValue(double(s->rotation()));
    xSpin->setToolTip(tr("The middle of the bottom edge, across the stage"));
    ySpin->setToolTip(tr("The middle of the bottom edge, up/down stage"));
    bSpin->setToolTip(tr("Height of the bottom edge above the floor (0 = to the deck)"));
    bowSpin->setToolTip(tr("How far the middle curves away from the audience; 0 is flat, "
                           "less than 0 curves toward them"));
    rotSpin->setToolTip(tr("0 runs across the stage facing the audience"));
    form->addRow(tr("Kind:"), kindCombo);
    form->addRow(tr("Centre X:"), xSpin);
    form->addRow(tr("Centre Y:"), ySpin);
    form->addRow(tr("Shape:"), outlineCombo);
    form->addRow(tr("Width:"), wSpin);
    form->addRow(tr("Depth:"), depthSpin);
    form->addRow(tr("Height:"), hSpin);
    form->addRow(tr("Thickness:"), thickSpin);
    form->addRow(tr("Bottom trim:"), bSpin);
    form->addRow(tr("Bow:"), bowSpin);
    form->addRow(tr("Facing:"), rotSpin);
    form->addRow(tr("Curtain:"), stateCombo);
    form->addRow(tr("Driven by:"), driveRow);
    form->addRow(tr("Opening width:"), openWSpin);
    form->addRow(tr("Opening height:"), openHSpin);
    // Each row only where it means something for the kind.
    auto showRow = [form](QWidget *field, bool on) {
        field->setVisible(on);
        if (QWidget *label = form->labelForField(field))
            label->setVisible(on);
    };
    auto showRows = [&]() {
        const int k = kindCombo->currentData().toInt();
        const bool arch = (k == Scenery::Proscenium);
        const bool built = (k == Scenery::Wall || k == Scenery::BackWall || arch);
        const bool u = built && !arch && outlineCombo->currentData().toInt() == Scenery::RoundedU;
        showRow(outlineCombo, built && !arch);
        showRow(depthSpin, u);
        showRow(bowSpin, !u && !arch);
        showRow(thickSpin, built);
        const bool curtain = (k == Scenery::MainDrape || k == Scenery::Traveler);
        const bool driven = curtain && driveCombo->currentData().toUInt() != Fixture::invalidId();
        showRow(openWSpin, arch || (curtain && (driven || stateCombo->currentData().toInt() == Scenery::Open)));
        showRow(openHSpin, arch);
        showRow(stateCombo, k == Scenery::MainDrape || k == Scenery::Traveler);
        showRow(driveRow, curtain);
        dlg.adjustSize();
    };
    QColor chosen = s->color();
    ColourSwatchButton *colorBtn = new ColourSwatchButton(chosen, tr("Cloth colour"), &dlg);
    colorBtn->setResetColour(Scenery::defaultColor(s->kind()));
    form->addRow(tr("Colour:"), colorBtn);
    showRows();

    auto apply = [&]() {
        s->setName(nameEdit->text().trimmed().isEmpty() ? oldName : nameEdit->text().trimmed());
        s->setOriginX(float(xSpin->value() * fromDisp));
        s->setOriginY(float(ySpin->value() * fromDisp));
        s->setWidth(float(wSpin->value() * fromDisp));
        s->setHeight(float(hSpin->value() * fromDisp));
        s->setBottom(float(bSpin->value() * fromDisp));
        s->setBow(float(bowSpin->value() * fromDisp));
        s->setRotation(float(rotSpin->value()));
        s->setKind(kindCombo->currentData().toInt());
        const bool built = s->isBuilt();
        s->setOutline(built && s->kind() != Scenery::Proscenium ? outlineCombo->currentData().toInt()
                                                                : int(Scenery::SheetOutline));
        if (s->kind() == Scenery::Proscenium)
            s->setBow(0.0f);
        s->setOpeningWidth(float(openWSpin->value() * fromDisp));
        s->setOpeningHeight(float(openHSpin->value() * fromDisp));
        s->setDepth(float(depthSpin->value() * fromDisp));
        s->setThickness(built ? float(thickSpin->value() * fromDisp) : 0.0f);
        s->setState(stateCombo->currentData().toInt());
        s->setDriverFixture(driveCombo->currentData().toUInt());
        s->setColor(chosen);
        m_graphicsView->updatePlatforms();
    };
    for (QDoubleSpinBox *sp : { xSpin, ySpin, wSpin, hSpin, bSpin, bowSpin, rotSpin, depthSpin, thickSpin,
                                openWSpin, openHSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&apply](double) { apply(); });
    for (QComboBox *cb : { kindCombo, outlineCombo, stateCombo, driveCombo })
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) {
            showRows();
            apply();
        });
    connect(newCurtainBtn, &QPushButton::clicked, &dlg, [&]() {
        apply();                // the kind it is now decides the mode
        if (Fixture *fx = addCurtainFixture(sid))
        {
            createdCurtains << fx->id();
            fillDrivers(fx->id());
            showRows();
            apply();
        }
    });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [&apply](const QString &) { apply(); });
    connect(colorBtn, &ColourSwatchButton::colourChanged, &dlg, [&](const QColor &c) {
        chosen = c;
        apply();
    });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    vl->addLayout(form);
    vl->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        // A curtain fixture added for it goes too.
        foreach (quint32 fid, createdCurtains)
            m_doc->deleteFixture(fid);
        return;
    }
    apply();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

Fixture *Monitor::addCurtainFixture(quint32 sid)
{
    Scenery *s = m_props->scenery(sid);
    if (s == nullptr || !s->hasState())
        return nullptr;
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(QStringLiteral("Generic"),
                                                               QStringLiteral("Stage Curtain"));
    if (def == nullptr || def->modes().isEmpty())
        return nullptr;
    // A main drape opens and flies; a traveler runs on its track only.
    QLCFixtureMode *mode = def->mode(s->kind() == Scenery::MainDrape ? QStringLiteral("Open + Fly")
                                                                     : QStringLiteral("Open"));
    if (mode == nullptr)
        mode = def->modes().first();
    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setName(tr("%1 curtain").arg(s->name().isEmpty() ? Scenery::kindName(s->kind()) : s->name()));
    /* Unpatched: it moves in the plot from the looks that hold it; patched to
       the curtain's controller (or a relay box) it moves the real one. */
    fx->setPatched(false);
    if (!m_doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    s->setDriverFixture(fx->id());
    m_graphicsView->updatePlatforms();
    m_doc->setModified();
    return fx;
}

void Monitor::slotAddTower()
{
    Tower *t = m_props->addTower();
    t->setName(tr("Tower %1").arg(t->id() + 1));
    t->addShelf(0.6f);   // a couple of default shelves
    t->addShelf(1.5f);
    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    t->setOriginX(float(mm.x() / 1000.0));
    t->setOriginY(float(mm.y() / 1000.0));
    t->setLayerId(m_props->activeLayerId());
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    slotEditTower(t->id());
}

/* An image to print on a face or a sock: "Pattern…" picks one, "×" takes
   it off; the label says which. Calls @p changed after either. */
static QWidget *makePatternPicker(QWidget *parent, QString *path, float *opacity,
                                  const std::function<void()> &changed)
{
    QWidget *w = new QWidget(parent);
    QHBoxLayout *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    QPushButton *choose = new QPushButton(QObject::tr("Pattern…"), w);
    choose->setToolTip(QObject::tr("An image printed over it, stretched to each face"));
    QToolButton *clear = new QToolButton(w);
    clear->setText(QStringLiteral("×"));
    clear->setToolTip(QObject::tr("No pattern: plain colour"));
    QLabel *name = new QLabel(w);
    name->setStyleSheet("color:#b0b4c0;");
    // How much of the print shows over the colour beneath it.
    QSpinBox *opacitySpin = new QSpinBox(w);
    opacitySpin->setRange(0, 100);
    opacitySpin->setSuffix(QStringLiteral("%"));
    opacitySpin->setValue(qRound(*opacity * 100.0f));
    opacitySpin->setToolTip(QObject::tr("Opacity of the print: 100% the print as it is, "
                                        "less and the colour beneath shows through"));
    auto show = [path, name, clear, opacitySpin]() {
        name->setText(path->isEmpty() ? QObject::tr("(plain)") : QFileInfo(*path).fileName());
        name->setToolTip(*path);
        clear->setEnabled(!path->isEmpty());
        opacitySpin->setEnabled(!path->isEmpty());
    };
    QObject::connect(opacitySpin, QOverload<int>::of(&QSpinBox::valueChanged), w,
                     [opacity, changed](int v) {
        *opacity = float(v) / 100.0f;
        changed();
    });
    show();
    QObject::connect(choose, &QPushButton::clicked, w, [w, path, show, changed]() {
        const QString f = QFileDialog::getOpenFileName(w, QObject::tr("Pattern image"),
            path->isEmpty() ? QString() : QFileInfo(*path).absolutePath(),
            QObject::tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
        if (f.isEmpty())
            return;
        *path = f;
        show();
        changed();
    });
    QObject::connect(clear, &QToolButton::clicked, w, [path, show, changed]() {
        path->clear();
        show();
        changed();
    });
    lay->addWidget(choose);
    lay->addWidget(clear);
    lay->addWidget(name, 1);
    lay->addWidget(opacitySpin);
    return w;
}

void Monitor::slotEditTower(quint32 tid)
{
    Tower *t = m_props->tower(tid);
    if (!t) return;
    RigEditSession edit(this);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString sfx = isFeet ? tr(" ft") : tr(" m");
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 40.0 : 12.0;


    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — Tower %1").arg(t->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *sp = new FeetInchesSpinBox(isFeet, &dlg);
        sp->setRange(lo, hi); sp->setDecimals(2);
        sp->setValue(val * toDisp); return sp;
    };
    QDoubleSpinBox *oxSpin = mkSpin(t->originX(), -posR, posR);
    QDoubleSpinBox *oySpin = mkSpin(t->originY(), -posR, posR);
    QDoubleSpinBox *wSpin  = mkSpin(t->width(), 0.05, isFeet ? 33.0 : 10.0);
    QDoubleSpinBox *dSpin  = mkSpin(t->depth(), 0.05, isFeet ? 33.0 : 10.0);
    QDoubleSpinBox *hSpin  = mkSpin(t->height(), 0.1, hMax);
    form->addRow(tr("Position X:"), oxSpin);
    form->addRow(tr("Position Y:"), oySpin);
    form->addRow(tr("Width:"), wSpin);
    form->addRow(tr("Depth:"), dSpin);
    form->addRow(tr("Height:"), hSpin);
    QDoubleSpinBox *facingSpin = new QDoubleSpinBox(&dlg);
    facingSpin->setRange(-180.0, 180.0);
    facingSpin->setDecimals(1);
    facingSpin->setSuffix(QStringLiteral("°"));
    facingSpin->setValue(double(t->rotation()));
    facingSpin->setObjectName("towerFacing");
    facingSpin->setToolTip(tr("Turned about its middle; what is on its shelves turns with it"));
    form->addRow(tr("Facing:"), facingSpin);

    /* Faces: opaque panels on its sides -- a black facade, or a printed one --
       on the sides ticked, over the stretch of its height given. */
    QWidget *facesRow = new QWidget(&dlg);
    QHBoxLayout *facesLay = new QHBoxLayout(facesRow);
    facesLay->setContentsMargins(0, 0, 0, 0);
    QCheckBox *facesCb = new QCheckBox(tr("Faces"), facesRow);
    facesCb->setChecked(t->hasFaces());
    facesCb->setToolTip(tr("Opaque panels over the tower's sides"));
    QColor *facesColour = new QColor(t->facesColour());
    ColourSwatchButton *facesColourBtn = new ColourSwatchButton(*facesColour, tr("Faces colour"), facesRow);
    facesColourBtn->setResetColour(QColor(24, 24, 26));
    facesColourBtn->setToolTip(tr("The panels' colour"));
    QDoubleSpinBox *facesFromSpin = mkSpin(t->facesFrom(), 0.0, hMax);
    QDoubleSpinBox *facesToSpin = mkSpin(t->facesTo(), 0.0, hMax);
    facesFromSpin->setToolTip(tr("How high the panels start"));
    facesToSpin->setToolTip(tr("How high they go -- its top covers the whole tower"));
    facesLay->addWidget(facesCb);
    facesLay->addWidget(facesColourBtn);
    facesLay->addWidget(new QLabel(tr("from"), facesRow));
    facesLay->addWidget(facesFromSpin);
    facesLay->addWidget(new QLabel(tr("to"), facesRow));
    facesLay->addWidget(facesToSpin);
    form->addRow(tr("Faces:"), facesRow);
    QWidget *sidesRow = new QWidget(&dlg);
    QHBoxLayout *sidesLay = new QHBoxLayout(sidesRow);
    sidesLay->setContentsMargins(0, 0, 0, 0);
    QList<QPair<QCheckBox *, int> > sideCbs;
    for (const auto &sd : { qMakePair(tr("Front"), int(Tower::FrontSide)),
                            qMakePair(tr("Back"), int(Tower::BackSide)),
                            qMakePair(tr("Stage left"), int(Tower::LeftSide)),
                            qMakePair(tr("Stage right"), int(Tower::RightSide)) })
    {
        QCheckBox *cb = new QCheckBox(sd.first, sidesRow);
        cb->setChecked(t->facesSides() & sd.second);
        sidesLay->addWidget(cb);
        sideCbs << qMakePair(cb, sd.second);
    }
    sidesLay->addStretch(1);
    form->addRow(tr("On sides:"), sidesRow);
    QString *facesPattern = new QString(t->facesPattern());
    float *facesOpacity = new float(t->facesPatternOpacity());
    std::function<void()> applyFaces;               // set below, once the canvas exists
    QWidget *facesPatternRow = makePatternPicker(&dlg, facesPattern, facesOpacity, [&applyFaces]() {
        if (applyFaces) applyFaces();
    });
    form->addRow(tr("Printed:"), facesPatternRow);
    vl->addLayout(form);

    // Shelves — a list of heights the user can add / remove.
    vl->addWidget(new QLabel(tr("Shelves (height above floor):"), &dlg));
    QListWidget *shelfList = new QListWidget(&dlg);
    shelfList->setToolTip(tr("Double-click a shelf to edit its height."));
    auto reloadShelves = [&]() {
        shelfList->clear();
        for (int i = 0; i < t->shelfCount(); ++i)
            shelfList->addItem(tr("Shelf %1  —  %2%3").arg(i + 1)
                .arg(double(t->shelfHeight(i)) * toDisp, 0, 'f', 2).arg(sfx));
    };
    reloadShelves();
    vl->addWidget(shelfList);

    QHBoxLayout *shelfBtns = new QHBoxLayout;
    QDoubleSpinBox *newShelf = new FeetInchesSpinBox(isFeet, &dlg);
    newShelf->setRange(0.0, hMax); newShelf->setDecimals(2);
    newShelf->setValue(1.0 * toDisp);
    QPushButton *addShelfBtn = new QPushButton(tr("Add shelf"), &dlg);
    QPushButton *delShelfBtn = new QPushButton(tr("Remove selected"), &dlg);
    shelfBtns->addWidget(newShelf);
    shelfBtns->addWidget(addShelfBtn);
    shelfBtns->addWidget(delShelfBtn);
    shelfBtns->addStretch(1);
    vl->addLayout(shelfBtns);
    connect(addShelfBtn, &QPushButton::clicked, &dlg, [&]() {
        t->addShelf(float(newShelf->value() * fromDisp)); reloadShelves();
    });
    connect(delShelfBtn, &QPushButton::clicked, &dlg, [&]() {
        if (shelfList->currentRow() >= 0) { t->removeShelf(shelfList->currentRow()); reloadShelves(); }
    });

    vl->addStretch();

    // Canvas-centric object editor body; geometry form is its collapsible left.
    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 1 /*Tower*/, tid, geomW, &view);
    auto applyLive = [&]() {
        t->setName(nameEdit->text());
        t->setOriginX(float(oxSpin->value() * fromDisp));
        t->setOriginY(float(oySpin->value() * fromDisp));
        t->setWidth(float(wSpin->value() * fromDisp));
        t->setDepth(float(dSpin->value() * fromDisp));
        t->setHeight(float(hSpin->value() * fromDisp));
        m_props->turnTower(t->id(), float(facingSpin->value()));
        if (view) view->reload();
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, wSpin, dSpin, hSpin, facingSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [applyLive](double){ applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });
    // Shelf add/remove already mutate the tower live — also refresh the canvas.
    connect(addShelfBtn, &QPushButton::clicked, &dlg, [view]() { if (view) view->reload(); });
    connect(delShelfBtn, &QPushButton::clicked, &dlg, [view]() { if (view) view->reload(); });
    // Double-click a shelf row to edit its height in place (Tower::
    // setShelfHeight() -- doesn't reorder the list, so this can't reassign
    // which shelf any already-mounted fixture is on, unlike remove+re-add).
    connect(shelfList, &QListWidget::itemDoubleClicked, &dlg, [&, view](QListWidgetItem *) {
        const int row = shelfList->currentRow();
        if (row < 0) return;
        bool ok = false;
        const double z = QInputDialog::getDouble(&dlg, tr("Edit Shelf Height"),
            tr("Height above floor:"), double(t->shelfHeight(row)) * toDisp,
            0.0, hMax, 2, &ok);
        if (!ok) return;
        t->setShelfHeight(row, float(z * fromDisp));
        reloadShelves();
        if (view) view->reload();
    });

    // The faces apply live, so the canvas shows them as they are set.
    applyFaces = [=]() {
        t->setFaces(facesCb->isChecked());
        int sides = 0;
        for (const auto &sc : sideCbs)
            if (sc.first->isChecked())
                sides |= sc.second;
        t->setFacesSides(sides);
        t->setFacesColour(*facesColour);
        const float to = float(facesToSpin->value() * fromDisp);
        t->setFacesRange(float(facesFromSpin->value() * fromDisp),
                         qAbs(to - t->height()) < 1e-3f ? -1.0f : to);     // its top: follows the height
        t->setFacesPattern(*facesPattern);
        t->setFacesPatternOpacity(*facesOpacity);
        const bool on = facesCb->isChecked();
        for (QWidget *w : { static_cast<QWidget *>(facesColourBtn), static_cast<QWidget *>(facesFromSpin),
                            static_cast<QWidget *>(facesToSpin), sidesRow, facesPatternRow })
            w->setEnabled(on);
        if (view) view->reload();
        m_graphicsView->updatePlatforms();
    };
    for (QWidget *w : { static_cast<QWidget *>(facesColourBtn), static_cast<QWidget *>(facesFromSpin),
                        static_cast<QWidget *>(facesToSpin), sidesRow, facesPatternRow })
        w->setEnabled(t->hasFaces());
    connect(facesCb, &QCheckBox::toggled, &dlg, [&applyFaces](bool) { applyFaces(); });
    for (QDoubleSpinBox *sp : { facesFromSpin, facesToSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [&applyFaces](double) { applyFaces(); });
    for (const auto &sc : sideCbs)
        connect(sc.first, &QCheckBox::toggled, &dlg, [&applyFaces](bool) { applyFaces(); });
    connect(facesColourBtn, &ColourSwatchButton::colourChanged, &dlg,
            [facesColour, &applyFaces](const QColor &c) {
        *facesColour = c;
        applyFaces();
    });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);
    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        delete facesColour;
        delete facesPattern;
        delete facesOpacity;
        return;
    }
    delete facesColour;
    delete facesPattern;
    delete facesOpacity;

    t->setName(nameEdit->text());
    t->setOriginX(float(oxSpin->value() * fromDisp));
    t->setOriginY(float(oySpin->value() * fromDisp));
    t->setWidth(float(wSpin->value() * fromDisp));
    t->setDepth(float(dSpin->value() * fromDisp));
    t->setHeight(float(hSpin->value() * fromDisp));

    m_graphicsView->updatePlatforms();
    foreach (Fixture *fx, m_doc->fixtures())
        if (m_props->fixtureRigProps(fx->id()).towerId == tid)
            m_graphicsView->updateFixture(fx->id());
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotAddElectric()
{
    Pipe *b = m_props->addPipe();
    b->setName(tr("Electric %1").arg(b->id() + 1));
    b->setOrientation(Pipe::Horizontal);
    b->setLength(2.0f);       // ~6-7 ft run
    b->setBaseZ(3.0f);        // overhead hang height
    b->setRunAngle(0.0f);     // runs stage left-right
    b->setBaseRadius(0.0f);   // no stand

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    b->setOriginX(float(mm.x() / 1000.0));
    b->setOriginY(float(mm.y() / 1000.0));
    b->setLayerId(m_props->activeLayerId());

    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    slotEditPipe(b->id());
}

void Monitor::slotAddPlatform()
{
    StagePlatform *p = m_props->addPlatform();
    p->setName(tr("Platform %1").arg(p->id() + 1));

    // Place new platform near the pending add position (from context-menu click),
    // or at stage centre if invoked from the toolbar.
    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    p->setOriginX(float(mm.x() / 1000.0));
    p->setOriginY(float(mm.y() / 1000.0));
    p->setLayerId(m_props->activeLayerId());   // land on the selected layer

    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    slotEditPlatform(p->id());
}

void Monitor::slotAddSteps()
{
    /* "Steps as a stage feature .. count, rise, run and width. Default 8"
       rise with 8" run and 2 steps for a total 16" high and 3' wide." A
       platform with a stepped shape, so it drags, locks, layers, carries
       fixtures and catches light like any other deck. */
    StagePlatform *p = m_props->addPlatform();
    p->setName(tr("Steps %1").arg(p->id() + 1));
    p->setSteps(2, 0.2032f, 0.2032f, 0.9144f, StagePlatform::ClimbUpstage);

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    p->setOriginX(float(mm.x() / 1000.0));
    p->setOriginY(float(mm.y() / 1000.0));
    p->setLayerId(m_props->activeLayerId());

    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();

    slotEditPlatform(p->id());
}

void Monitor::slotEditPipe(quint32 bid)
{
    Pipe *b = m_props->pipe(bid);
    if (!b) return;
    RigEditSession edit(this);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 66.0 : 20.0;


    QDialog dlg(this);
    const bool horiz = (b->orientation() == Pipe::Horizontal);
    dlg.setWindowTitle(tr("Lighting Studio Editor — Boom %1").arg(b->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(b->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *s = new FeetInchesSpinBox(isFeet, &dlg);
        s->setRange(lo, hi); s->setDecimals(2);
        s->setValue(val * toDisp); return s;
    };
    QDoubleSpinBox *oxSpin = mkSpin(b->originX(), -posR, posR);
    QDoubleSpinBox *oySpin = mkSpin(b->originY(), -posR, posR);
    QDoubleSpinBox *hSpin  = mkSpin(b->height(), 0.1, hMax);
    QDoubleSpinBox *bzSpin = mkSpin(b->baseZ(), 0.0, hMax);
    form->addRow(tr("Position X:"), oxSpin);
    form->addRow(tr("Position Y:"), oySpin);
    form->addRow(horiz ? tr("Run length:") : tr("Pipe height:"), hSpin);

    // Horizontal run direction (electric) — degrees, 0 = stage left-right.
    QDoubleSpinBox *runSpin = new QDoubleSpinBox(&dlg);
    runSpin->setRange(-180, 180); runSpin->setDecimals(0); runSpin->setSuffix(QStringLiteral("°"));
    runSpin->setValue(double(b->runAngle()));
    if (horiz)
        form->addRow(tr("Run angle:"), runSpin);

    // Crossbar on a boom: how far up the parent boom it attaches. When set, the
    // base position/stand/truss controls below are derived and don't apply.
    const bool barOnPipe = b->isBarOnPipe();
    QDoubleSpinBox *upBoomSpin = nullptr;
    QDoubleSpinBox *crossbarStandoffSpin = nullptr;
    if (barOnPipe)
    {
        Pipe *pp = m_props->pipe(b->parentPipeId());
        const double maxUp = pp ? double(pp->length()) : hMax * fromDisp;
        upBoomSpin = mkSpin(b->parentPipeOffset(), 0.0, maxUp * toDisp);
        form->addRow(tr("Height up boom:"), upBoomSpin);

        // Sideways clearance from the boom's own point in Top view (the
        // Pipe analogue of a Truss bar's "standoff" field) -- without it, a
        // crossbar renders exactly on top of its own boom with no way to
        // tell them apart (WORKFLOW-SUBTLETIES.md #14).
        crossbarStandoffSpin = mkSpin(b->crossbarStandoff(), 0.0, isFeet ? 6.6 : 2.0);
        crossbarStandoffSpin->setToolTip(tr("How far this crossbar sits clear of its own "
                                            "boom in the plan view. 0 puts it exactly on "
                                            "the boom's own point, indistinguishable from it."));
        form->addRow(tr("Standoff from boom:"), crossbarStandoffSpin);
    }
    form->addRow(tr("Base height (Z):"), bzSpin);

    QCheckBox *standChk = new QCheckBox(tr("On a stand (base)"), &dlg);
    standChk->setChecked(b->hasStand());
    QDoubleSpinBox *brSpin = mkSpin(b->hasStand() ? b->baseRadius() : 0.4f, 0.05, isFeet ? 6.6 : 2.0);
    brSpin->setEnabled(b->hasStand());
    connect(standChk, &QCheckBox::toggled, brSpin, &QWidget::setEnabled);
    form->addRow(QString(), standChk);
    form->addRow(tr("Base radius:"), brSpin);

    // Hang from a truss (drop-arm): base is derived from the truss, pipe hangs
    // below the attach point; overrides position/stand.
    QComboBox *trussCombo = new QComboBox(&dlg);
    trussCombo->addItem(tr("(free-standing)"), quint32(Truss::invalidId()));
    foreach (Truss *t, m_props->trusses())
    {
        if (t == NULL || t->isChildBar())
            continue;
        trussCombo->addItem(t->name().isEmpty() ? tr("Truss %1").arg(t->id()) : t->name(), t->id());
        if (t->id() == b->parentTrussId())
            trussCombo->setCurrentIndex(trussCombo->count() - 1);
    }
    form->addRow(tr("Hang from truss:"), trussCombo);
    QDoubleSpinBox *trussOffSpin = mkSpin(b->trussOffset(), 0.0, isFeet ? 200.0 : 60.0);
    form->addRow(tr("Along truss:"), trussOffSpin);

    // Stand on a Stand object (base derived from the stand top).
    QComboBox *standCombo = new QComboBox(&dlg);
    standCombo->addItem(tr("(free / own base)"), quint32(Stand::invalidId()));
    foreach (Stand *st, m_props->stands())
    {
        standCombo->addItem(st->name().isEmpty() ? tr("Stand %1").arg(st->id()) : st->name(), st->id());
        if (st->id() == b->standId())
            standCombo->setCurrentIndex(standCombo->count() - 1);
    }
    form->addRow(tr("On stand:"), standCombo);

    // Where up the stand the pipe/bar clamps. Off = at the stand top; on = at an
    // explicit height along the post (a bar clamped part-way up a stand).
    QCheckBox *standHtChk = new QCheckBox(tr("Clamp part-way up the stand"), &dlg);
    standHtChk->setChecked(b->hasStandOffset());
    QDoubleSpinBox *standHtSpin = mkSpin(b->hasStandOffset() ? b->standOffset() : b->baseZ(),
                                         0.0, hMax);
    standHtSpin->setEnabled(b->hasStandOffset());
    connect(standHtChk, &QCheckBox::toggled, standHtSpin, &QWidget::setEnabled);
    form->addRow(QString(), standHtChk);
    form->addRow(tr("Height up stand:"), standHtSpin);
    vl->addLayout(form);

    // Fixtures mounted on this pipe — per-fixture height up the pipe + facing angle.
    QList<quint32> mounted;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL && m_props->fixtureRigProps(fx->id()).pipeId == bid)
            mounted << fx->id();

    QList<QDoubleSpinBox *> offSpins, angSpins;
    if (!mounted.isEmpty())
    {
        /* Up a boom, along a house electric: each fixture at its own place on
           it, no further than the pipe goes. */
        const bool across = !b->isVertical();
        vl->addWidget(new QLabel(across ? tr("Fixtures on this pipe (distance along the pipe · facing):")
                                        : tr("Fixtures on this pipe (height up the pipe · facing):"), &dlg));
        QTableWidget *tbl = new QTableWidget(mounted.size(), 3, &dlg);
        tbl->setObjectName(QStringLiteral("pipeFixtures"));
        tbl->setHorizontalHeaderLabels(QStringList() << tr("Fixture")
                                       << (across ? tr("Along") : tr("Height")) << tr("Angle"));
        tbl->verticalHeader()->setVisible(false);
        tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 0; i < mounted.size(); ++i)
        {
            Fixture *fx = m_doc->fixture(mounted[i]);
            FixtureRigProps rp = m_props->fixtureRigProps(mounted[i]);
            QTableWidgetItem *ni = new QTableWidgetItem(fx ? fx->name() : QString::number(mounted[i]));
            ni->setFlags(ni->flags() & ~Qt::ItemIsEditable);
            tbl->setItem(i, 0, ni);
            QDoubleSpinBox *os = new FeetInchesSpinBox(isFeet, tbl);
            os->setRange(0.0, qMax(0.1, double(b->length()) * toDisp)); os->setDecimals(2);
            os->setValue(double(rp.pipeOffset) * toDisp);
            tbl->setCellWidget(i, 1, os); offSpins << os;
            QDoubleSpinBox *as = new QDoubleSpinBox(tbl);
            as->setRange(-180, 180); as->setDecimals(0); as->setSuffix(QStringLiteral("°"));
            as->setValue(double(rp.pipeAngle));
            tbl->setCellWidget(i, 2, as); angSpins << as;
        }
        vl->addWidget(tbl);
    }

    vl->addStretch();

    // Canvas-centric object editor body; geometry form is its collapsible left.
    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 4 /*Pipe*/, bid, geomW, &view);
    auto applyLive = [&]() {
        b->setName(nameEdit->text());
        b->setOriginX(float(oxSpin->value() * fromDisp));
        b->setOriginY(float(oySpin->value() * fromDisp));
        b->setHeight(float(hSpin->value() * fromDisp));
        b->setBaseZ(float(bzSpin->value() * fromDisp));
        b->setRunAngle(float(runSpin->value()));
        m_props->recomputeChildTrusses();
        if (view) view->reload();
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, hSpin, bzSpin, runSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [applyLive](double){ applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }

    b->setName(nameEdit->text());
    b->setOriginX(float(oxSpin->value() * fromDisp));
    b->setOriginY(float(oySpin->value() * fromDisp));
    b->setHeight(float(hSpin->value() * fromDisp));
    b->setBaseZ(float(bzSpin->value() * fromDisp));
    b->setBaseRadius(standChk->isChecked() ? float(brSpin->value() * fromDisp) : 0.0f);
    b->setRunAngle(float(runSpin->value()));

    const quint32 tid = trussCombo->currentData().toUInt();
    if (tid != Truss::invalidId())
    {
        b->setParentTrussId(tid);
        b->setTrussOffset(float(trussOffSpin->value() * fromDisp));
    }
    else
    {
        b->setParentTrussId(Truss::invalidId());
    }
    b->setStandId(standCombo->currentData().toUInt());   // invalid = free
    // <0 sentinel = mount at the stand top; else an explicit height up the post.
    b->setStandOffset(standHtChk->isChecked()
                          ? float(standHtSpin->value() * fromDisp) : -1.0f);
    if (barOnPipe && upBoomSpin != nullptr)
        b->setParentPipeOffset(float(upBoomSpin->value() * fromDisp));
    if (barOnPipe && crossbarStandoffSpin != nullptr)
        b->setCrossbarStandoff(float(crossbarStandoffSpin->value() * fromDisp));
    // Stands first, then truss-hung + bars-on-pipes derive from their parents.
    m_props->recomputeChildTrusses();

    for (int i = 0; i < mounted.size(); ++i)
    {
        FixtureRigProps rp = m_props->fixtureRigProps(mounted[i]);
        rp.pipeOffset = float(offSpins[i]->value() * fromDisp);
        rp.pipeAngle  = float(angSpins[i]->value());
        m_props->setFixtureRigProps(mounted[i], rp);
    }

    m_graphicsView->updatePlatforms();   // refresh the pipe item
    foreach (quint32 fid, mounted)
        m_graphicsView->updateFixture(fid);
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotEditPlatform(quint32 pid)
{
    StagePlatform *p = m_props->platform(pid);
    if (!p) return;
    RigEditSession edit(this);


    QDialog dlg(this);
    dlg.setWindowTitle(p->isSteps() ? tr("Lighting Studio Editor — Steps %1").arg(p->name())
                                    : tr("Lighting Studio Editor — Riser %1").arg(p->name()));

    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl   = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(p->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    const bool isFeet_p = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp_p   = isFeet_p ? 3.28084 : 1.0;
    const double fromDisp_p = isFeet_p ? (1.0 / 3.28084) : 1.0;
    const double posRange_p = isFeet_p ? 164.0 : 50.0;
    const double sizeMax_p  = isFeet_p ? 328.0 : 100.0;
    const double hMax_p     = isFeet_p ? 32.8  : 10.0;

    QDoubleSpinBox *oxSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    oxSpin->setRange(-posRange_p, posRange_p); oxSpin->setDecimals(2);
    oxSpin->setValue(double(p->originX()) * toDisp_p);
    form->addRow(tr("Origin X:"), oxSpin);

    QDoubleSpinBox *oySpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    oySpin->setRange(-posRange_p, posRange_p); oySpin->setDecimals(2);
    oySpin->setValue(double(p->originY()) * toDisp_p);
    form->addRow(tr("Origin Y:"), oySpin);

    // In feet mode these accept feet-and-inches (5' 6") or decimal feet (5.5).
    FeetInchesSpinBox *wSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    wSpin->setRange(0.1, sizeMax_p);
    wSpin->setValue(double(p->width()) * toDisp_p);
    form->addRow(tr("Width (X):"), wSpin);

    FeetInchesSpinBox *dSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    dSpin->setRange(0.1, sizeMax_p);
    dSpin->setValue(double(p->depth()) * toDisp_p);
    form->addRow(tr("Depth (Y):"), dSpin);

    FeetInchesSpinBox *hSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    hSpin->setRange(0.0, hMax_p);
    hSpin->setValue(double(p->height()) * toDisp_p);
    form->addRow(tr("Height:"), hSpin);

    FeetInchesSpinBox *curveSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    curveSpin->setRange(0.0, sizeMax_p / 2.0);
    curveSpin->setValue(double(p->frontCurve()) * toDisp_p);
    curveSpin->setObjectName("platformFrontCurve");
    curveSpin->setToolTip(tr("A rounded front: how far the middle of the downstage edge reaches past "
                             "its corners (0 is square; half the width is a half circle). The depth "
                             "is measured at the middle."));
    form->addRow(tr("Front curve:"), curveSpin);

    QDoubleSpinBox *facingSpin = new QDoubleSpinBox(&dlg);
    facingSpin->setRange(-180.0, 180.0);
    facingSpin->setDecimals(1);
    facingSpin->setSuffix(QStringLiteral("°"));
    facingSpin->setValue(double(p->rotation()));
    facingSpin->setObjectName("platformFacing");
    facingSpin->setToolTip(tr("Turned about its middle; what stands on it and what is fixed "
                              "to its faces turn with it"));
    form->addRow(tr("Facing:"), facingSpin);

    /* A flight of steps: count, rise, run and width say it all, and the box
       above follows from them (read-only while it is steps). */
    QComboBox *shapeCombo = new QComboBox(&dlg);
    shapeCombo->addItem(tr("Platform"), int(StagePlatform::BoxShape));
    shapeCombo->addItem(tr("Steps"), int(StagePlatform::StepsShape));
    shapeCombo->setCurrentIndex(p->isSteps() ? 1 : 0);
    form->insertRow(1, tr("Shape:"), shapeCombo);
    QSpinBox *countSpin = new QSpinBox(&dlg);
    countSpin->setRange(1, 60);
    countSpin->setValue(p->stepCount());
    form->addRow(tr("Steps:"), countSpin);
    FeetInchesSpinBox *riseSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    riseSpin->setRange(0.01, hMax_p);
    riseSpin->setValue(double(p->stepRise()) * toDisp_p);
    riseSpin->setToolTip(tr("Height of each step"));
    form->addRow(tr("Rise:"), riseSpin);
    FeetInchesSpinBox *runSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    runSpin->setRange(0.01, sizeMax_p);
    runSpin->setValue(double(p->stepRun()) * toDisp_p);
    runSpin->setToolTip(tr("Depth of each tread"));
    form->addRow(tr("Run:"), runSpin);
    FeetInchesSpinBox *stepWSpin = new FeetInchesSpinBox(isFeet_p, &dlg);
    stepWSpin->setRange(0.1, sizeMax_p);
    stepWSpin->setValue(double(p->isSteps() ? p->stepWidth() : p->width()) * toDisp_p);
    stepWSpin->setToolTip(tr("Across the treads"));
    form->addRow(tr("Step width:"), stepWSpin);
    QComboBox *climbCombo = new QComboBox(&dlg);
    climbCombo->addItem(tr("Upstage (bottom step downstage)"), int(StagePlatform::ClimbUpstage));
    climbCombo->addItem(tr("Downstage"), int(StagePlatform::ClimbDownstage));
    climbCombo->addItem(tr("Stage left"), int(StagePlatform::ClimbStageLeft));
    climbCombo->addItem(tr("Stage right"), int(StagePlatform::ClimbStageRight));
    climbCombo->setCurrentIndex(climbCombo->findData(int(p->stepsClimb())));
    climbCombo->setToolTip(tr("Which way you walk going up"));
    form->addRow(tr("Climb toward:"), climbCombo);
    auto showShape = [form, shapeCombo, countSpin, riseSpin, runSpin, stepWSpin, climbCombo,
                      wSpin, dSpin, hSpin, curveSpin]() {
        const bool steps = shapeCombo->currentIndex() == 1;
        curveSpin->setVisible(!steps);
        if (QWidget *l = form->labelForField(curveSpin)) l->setVisible(!steps);
        for (QWidget *w : { (QWidget *)countSpin, (QWidget *)riseSpin, (QWidget *)runSpin,
                            (QWidget *)stepWSpin, (QWidget *)climbCombo })
        {
            w->setVisible(steps);
            if (QWidget *l = form->labelForField(w)) l->setVisible(steps);
        }
        for (QWidget *w : { (QWidget *)wSpin, (QWidget *)dSpin, (QWidget *)hSpin })
            w->setEnabled(!steps);
    };
    showShape();

    // Color picker row
    QColor curColor = p->color().isValid() ? p->color() : QColor(80, 120, 200);
    ColourSwatchButton *colorBtn = new ColourSwatchButton(curColor, tr("Platform colour"), &dlg);
    colorBtn->setResetColour(QColor(80, 120, 200));
    QColor chosenColor = curColor;
    connect(colorBtn, &ColourSwatchButton::colourChanged, &dlg, [&](const QColor &c) {
        chosenColor = c;
        /* Applied straight away, like the size spin boxes and the two
           checkboxes in this same dialog: the platform on the canvas shows it
           while it can still be compared against the stage. Cancel puts it
           back with the rest (RigEditSession). */
        p->setColor(c);
        m_graphicsView->updatePlatforms();
        m_doc->setModified();
    });
    form->addRow(tr("Colour:"), colorBtn);

    /* What the deck is made of. A clear top is why you would rig fixtures
       INSIDE a step at all: they show through it and their light goes up. */
    QComboBox *topCombo = new QComboBox(&dlg);
    topCombo->addItem(tr("Solid"), int(StagePlatform::SolidTop));
    topCombo->addItem(tr("Clear (lights inside show through)"), int(StagePlatform::ClearTop));
    topCombo->addItem(tr("Open frame (no top)"), int(StagePlatform::OpenTop));
    topCombo->setCurrentIndex(topCombo->findData(int(p->topMaterial())));
    topCombo->setToolTip(tr("A clear or open top lets fixtures rigged inside the "
                            "step be seen, and their light broadcast up. The "
                            "fixtures also need their Placement set to "
                            "\"Inside\" — otherwise they sit on the deck."));
    form->addRow(tr("Top:"), topCombo);
    connect(topCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [p, this, topCombo](int) {
        p->setTopMaterial(StagePlatform::TopMaterial(topCombo->currentData().toInt()));
        m_doc->setModified();
        m_graphicsView->updatePlatforms();
    });

    // No-overlap collision properties.
    QCheckBox *solidChk = new QCheckBox(tr("Solid — can't overlap other solid platforms"), &dlg);
    solidChk->setChecked(p->solid());
    /* Say what this is NOT: it reads like "is this thing solid or hollow", and
       it is only about whether two platforms may be dragged through each other.
       To rig a light inside a step you set the fixture's Placement, not this. */
    solidChk->setToolTip(tr("A layout rule only: it stops two solid platforms "
                            "being dragged through each other. It does not make "
                            "the step hollow — to rig a fixture inside one, set "
                            "that fixture's Placement to \"Inside\"."));
    form->addRow(tr("Collision:"), solidChk);
    QCheckBox *stackChk = new QCheckBox(tr("Stackable — may sit on top (step / riser)"), &dlg);
    stackChk->setChecked(p->stackable());
    stackChk->setToolTip(tr("A stackable platform is exempt from the no-overlap rule, "
                            "so it can be placed on top of a solid deck."));
    form->addRow(QString(), stackChk);
    connect(solidChk, &QCheckBox::toggled, &dlg, [p, this](bool on) {
        p->setSolid(on); m_doc->setModified();
    });
    connect(stackChk, &QCheckBox::toggled, &dlg, [p, this](bool on) {
        p->setStackable(on); m_doc->setModified();
    });

    vl->addLayout(form);
    vl->addStretch();
    // (The old "Edit fixtures in Studio…" button is gone — this editor IS the
    //  studio now: the fixture tree + canvas below handle layout in one window.)

    // Canvas-centric object editor body; geometry form is its collapsible left.
    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 3 /*Platform*/, pid, geomW, &view);
    bool applying = false;
    auto applyLive = [&]() {
        if (applying) return;
        applying = true;
        if (!nameEdit->text().trimmed().isEmpty()) p->setName(nameEdit->text().trimmed());
        p->setOriginX(float(oxSpin->value() * fromDisp_p));
        p->setOriginY(float(oySpin->value() * fromDisp_p));
        if (shapeCombo->currentIndex() == 1)
        {
            p->setSteps(countSpin->value(), float(riseSpin->value() * fromDisp_p),
                        float(runSpin->value() * fromDisp_p), float(stepWSpin->value() * fromDisp_p),
                        StagePlatform::Climb(climbCombo->currentData().toInt()));
            // The box follows the flight.
            for (QDoubleSpinBox *sp : { (QDoubleSpinBox *)wSpin, (QDoubleSpinBox *)dSpin,
                                        (QDoubleSpinBox *)hSpin })
                sp->blockSignals(true);
            wSpin->setValue(double(p->width()) * toDisp_p);
            dSpin->setValue(double(p->depth()) * toDisp_p);
            hSpin->setValue(double(p->height()) * toDisp_p);
            for (QDoubleSpinBox *sp : { (QDoubleSpinBox *)wSpin, (QDoubleSpinBox *)dSpin,
                                        (QDoubleSpinBox *)hSpin })
                sp->blockSignals(false);
        }
        else
        {
            p->setBoxShape();
            p->setWidth(float(wSpin->value() * fromDisp_p));
            p->setDepth(float(dSpin->value() * fromDisp_p));
            p->setHeight(float(hSpin->value() * fromDisp_p));
            p->setFrontCurve(float(curveSpin->value() * fromDisp_p));
        }
        m_props->turnPlatform(p->id(), float(facingSpin->value()));
        m_props->recomputeAnchoredFrames();
        m_graphicsView->updatePlatforms();
        m_graphicsView->refreshRiserFixtures();
        if (view) view->reload();
        applying = false;
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, (QDoubleSpinBox*)wSpin,
                                (QDoubleSpinBox*)dSpin, (QDoubleSpinBox*)hSpin,
                                (QDoubleSpinBox*)riseSpin, (QDoubleSpinBox*)runSpin,
                                (QDoubleSpinBox*)stepWSpin, (QDoubleSpinBox*)curveSpin, facingSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [applyLive](double){ applyLive(); });
    connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [applyLive](int){ applyLive(); });
    connect(climbCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [applyLive](int){ applyLive(); });
    connect(shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
            [applyLive, showShape](int){ showShape(); applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);

    if (dlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }

    applyLive();      // shape, steps and box, as the dialog last showed them
    p->setColor(chosenColor);

    m_props->recomputeAnchoredFrames();           // slaved studio frames follow the move/resize
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshRiserFixtures();       // mounted + frame fixtures follow size/height
    if (m_layersPanel) m_layersPanel->reload();   // reflect the new name/colour in the tree
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotCanvasContextMenu(QPointF scenePos)
{
    m_pendingAddScenePos = scenePos;
    QMenu menu(this);
    /* Everything you can add, under one "Add here" -- the list had grown to
       ten entries at the top level and buried the things you do to what is
       already there. Grouped the way the plot is built: lights, then what
       holds them, then what they point at. */
    QMenu *addMenu = menu.addMenu(tr("Add Here"));
    addMenu->addAction(QIcon(":/fixture.png"), tr("Fixture…"),
                       this, SLOT(slotAddFixture()));
    addMenu->addSeparator();
    addMenu->addAction(QIcon(":/group.png"), tr("Truss"),
                       this, SLOT(slotAddTruss()));
    addMenu->addAction(tr("Platform/Riser"),
                       this, SLOT(slotAddPlatform()));
    addMenu->addAction(tr("Steps"),
                       this, SLOT(slotAddSteps()));
    addMenu->addAction(tr("Boom"),
                       this, SLOT(slotAddPipe()));
    addMenu->addAction(tr("Electric"),
                       this, SLOT(slotAddElectric()));
    addMenu->addAction(tr("Stand"),
                       this, SLOT(slotAddStand()));
    addMenu->addAction(tr("Tower"),
                       this, SLOT(slotAddTower()));
    QMenu *sceneryMenu = addMenu->addMenu(tr("Scenery"));
    foreach (int kind, Scenery::kinds())
        sceneryMenu->addAction(Scenery::kindName(kind), this, [this, kind]() { slotAddScenery(kind); });
    addMenu->addAction(tr("Fountain…"), this, [this]() { slotAddFountain(); });
    addMenu->addSeparator();
    addMenu->addAction(tr("Target Position"),
                       this, SLOT(slotAddTarget()));
    addMenu->addAction(tr("Target Line"),
                       this, SLOT(slotAddTargetLine()));
    addMenu->addAction(tr("Person"),
                       this, SLOT(slotAddPerson()));
    addMenu->addSeparator();
    addMenu->addAction(QIcon(":/image.png"), tr("Image"),
                       this, SLOT(slotAddImage()));
    addMenu->addAction(tr("Network Node"),
                       this, SLOT(slotAddNetworkNode()));

    menu.addSeparator();
    QAction *groupAct = menu.addAction(QIcon(":/group.png"), tr("Group selected (Ctrl+G)"),
                                       this, SLOT(slotGroupItems()));
    groupAct->setEnabled(m_graphicsView != NULL && m_graphicsView->selectionGroupable());
    QAction *ungroupAct = menu.addAction(QIcon(":/ungroup.png"), tr("Ungroup (Ctrl+Shift+G)"),
                                         this, SLOT(slotUngroupItems()));
    ungroupAct->setEnabled(m_graphicsView != NULL && m_graphicsView->selectionHasGroup());
    QAction *rotateAct = menu.addAction(tr("Rotate…  ([ / ])"), this, [this]() { slotRotateSelection(); });
    rotateAct->setEnabled(m_graphicsView != NULL && !m_graphicsView->scene()->selectedItems().isEmpty());

    // Fixture Studio component library — browse/stamp saved components. Stamps
    // onto the current fixture selection (the browser disables stamp if none).
    menu.addSeparator();
    menu.addAction(tr("Browse Studio Components…"), this, [this]() {
        if (m_graphicsView != NULL)
            m_graphicsView->browseStudioComponents();
    });

    // Align / distribute selected fixtures (Top view) — clean up a hand-placed
    // row/column so their X/Y match exactly.
    const int nFix = (m_graphicsView != NULL) ? m_graphicsView->selectedFixtureCount() : 0;
    if (nFix >= 2)
    {
        menu.addSeparator();
        QMenu *alignMenu = menu.addMenu(tr("Align fixtures"));
        alignMenu->addAction(tr("Same Y (straight row)"), this, [this]() { m_graphicsView->alignSelectedFixtures(3); });
        alignMenu->addAction(tr("Same X (straight column)"), this, [this]() { m_graphicsView->alignSelectedFixtures(0); });
        alignMenu->addSeparator();
        alignMenu->addAction(tr("Upstage edge"),   this, [this]() { m_graphicsView->alignSelectedFixtures(4); });
        alignMenu->addAction(tr("Downstage edge"), this, [this]() { m_graphicsView->alignSelectedFixtures(5); });
        alignMenu->addAction(tr("Left edge"),      this, [this]() { m_graphicsView->alignSelectedFixtures(1); });
        alignMenu->addAction(tr("Right edge"),     this, [this]() { m_graphicsView->alignSelectedFixtures(2); });
        if (nFix >= 3)
        {
            QMenu *distMenu = menu.addMenu(tr("Distribute fixtures"));
            distMenu->addAction(tr("Evenly left↔right"), this, [this]() { m_graphicsView->distributeSelectedFixtures(true); });
            distMenu->addAction(tr("Evenly up↔down"),    this, [this]() { m_graphicsView->distributeSelectedFixtures(false); });
        }
    }

    menu.addSeparator();
    // Delete whatever is selected (truss / platform / boom / stand / tower /
    // target / image / fixtures) — a reliable path when a right-click just misses
    // a small feature's hit-area. Detaches or removes attached fixtures gracefully.
    if (!m_graphicsView->scene()->selectedItems().isEmpty())
    {
        menu.addAction(QIcon(":/delete.png"), tr("Delete selected"),
                       this, SLOT(slotRemoveSelected()));
    }
    // Duplicate = copy the current selection and paste it offset by ~0.5 m, in
    // one step. Works for trusses / platforms / targets.
    if (!m_graphicsView->scene()->selectedItems().isEmpty())
    {
        menu.addAction(QIcon(":/editcopy.png"), tr("Duplicate selected"),
                       this, [this]() {
            slotCopySelected();
            if (clipboardHasFeatures())
                pasteClipboard(0.5f, 0.5f);   // small offset so the copy is visible
        });
    }
    menu.addAction(QIcon(":/editcopy.png"), tr("Copy selected features"),
                   this, SLOT(slotCopySelected()));
    if (clipboardHasFeatures())
    {
        QAction *pasteHere = menu.addAction(QIcon(":/editpaste.png"),
                                            tr("Paste features here"));
        connect(pasteHere, &QAction::triggered, this, [this, scenePos]() {
            // Anchor the paste so the first clipboard feature lands at the
            // cursor; the rest keep their relative offsets.
            QPointF refM;
            if (!m_trussClipboard.isEmpty())
                refM = QPointF(m_trussClipboard.first().origin.x(),
                               m_trussClipboard.first().origin.y());
            else if (!m_platformClipboard.isEmpty())
                refM = QPointF(m_platformClipboard.first().originX,
                               m_platformClipboard.first().originY);
            else
                refM = QPointF(m_targetClipboard.first().x,
                               m_targetClipboard.first().y);

            QPointF curMm = m_graphicsView->pixelsToRealPosition(scenePos.x(), scenePos.y());
            pasteClipboard(float(curMm.x() / 1000.0) - refM.x(),
                           float(curMm.y() / 1000.0) - refM.y());
        });
    }

    menu.exec(QCursor::pos());
}

void Monitor::slotAddFixtureToTruss(quint32 trussId, float offsetMetres)
{
    Q_ASSERT(m_graphicsView != NULL);

    if (m_props->layoutLocked())
    {
        QMessageBox::information(this, tr("Plot locked"),
            tr("Unlock the plot (Edit Plot) before adding fixtures."));
        return;
    }

    Truss *t = m_props->truss(trussId);
    if (!t) return;

    QList<quint32> disabled = m_graphicsView->fixturesID();
    FixtureSelection fs(this, m_doc);
    fs.setMultiSelection(true);
    fs.setDisabledFixtures(disabled);
    if (fs.exec() != QDialog::Accepted)
        return;

    // Compute world position at the requested offset (mm)
    QVector3D worldM = t->positionAt(offsetMetres);
    QPointF mm(worldM.x() * 1000.0, worldM.y() * 1000.0);

    foreach (quint32 fid, fs.selection())
    {
        // Set rig props BEFORE addFixture so the item reads the correct trussId
        // and setBoundToTruss(true) is called during item creation.
        FixtureRigProps rp;
        rp.trussId     = trussId;
        rp.trussOffset = offsetMetres;
        m_props->setFixtureRigProps(fid, rp);

        m_graphicsView->addFixture(fid, mm);
        m_props->setFixturePosition(fid, 0, 0, QVector3D(mm.x(), mm.y(), 0));
        m_props->setFixtureFlags(fid, 0, 0, 0);
        m_doc->setModified();
    }
    m_graphicsView->refreshFixtureLabels();   // apply per-layer labels to the new item
}

// ---------------------------------------------------------------------------
// Draggable fixture-placement strip embedded in the truss edit dialog.
// No Q_OBJECT — callers read results via slots() on dialog accept.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// DMX helpers + Override-priority fixture test/locate classes.
// ---------------------------------------------------------------------------

/** Helper: find the first "shutter open" channel for a fixture.
 *  Returns QLCChannel::invalid() if not found; sets outValue to the midpoint. */
static quint32 fixtureShutterChannel(Fixture *fxi, uchar &outValue)
{
    if (!fxi->fixtureMode()) return QLCChannel::invalid();
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        QLCChannel *ch = fxi->fixtureMode()->channel(c);
        if (!ch || ch->group() != QLCChannel::Shutter) continue;
        for (const QLCCapability *cap : ch->capabilities())
        {
            const QString n = cap->name().toLower();
            if (n.contains("open") && !n.contains("strobe") && !n.contains("close"))
            {
                outValue = cap->middle();
                return c;
            }
        }
    }
    return QLCChannel::invalid();
}

/** Append full-intensity + shutter-open SceneValues for fxi. */
static void appendFixtureOnValues(Fixture *fxi, QList<SceneValue> &svs)
{
    if (!fxi->fixtureMode()) return;

    // masterIntensityChannel() only finds channels where headForChannel(i)==-1
    // (i.e. not assigned to any head). On most moving heads the dimmer IS a
    // head channel, so we fall back to scanning all fixture channels directly.
    quint32 mi = fxi->masterIntensityChannel();
    if (mi == QLCChannel::invalid())
    {
        for (quint32 c = 0; c < fxi->channels(); ++c)
        {
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (ch && ch->group() == QLCChannel::Intensity
                && ch->controlByte() == QLCChannel::MSB
                && ch->colour() == QLCChannel::NoColour)
            {
                mi = c;
                break;
            }
        }
    }
    if (mi != QLCChannel::invalid())
        svs << SceneValue(fxi->id(), mi, 255);

    // For RGB/RGBW/LED wash fixtures: set every Intensity-group colour channel
    // (Red, Green, Blue, White, Amber, UV …) to 255 so the fixture emits
    // white/full-on light during test and locate.  This covers both
    // Colour-group presets and IntensityRed/Green/Blue-style presets.
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        QLCChannel *ch = fxi->fixtureMode()->channel(c);
        if (!ch || ch->controlByte() != QLCChannel::MSB) continue;
        const bool isColourGroup    = (ch->group() == QLCChannel::Colour);
        const bool isIntensityColour = (ch->group() == QLCChannel::Intensity
                                        && ch->colour() != QLCChannel::NoColour);
        if ((isColourGroup || isIntensityColour)
            && ch->colour() != QLCChannel::NoColour)
        {
            svs << SceneValue(fxi->id(), c, 255);
        }
    }

    uchar shutterVal = 0;
    quint32 shutterCh = fixtureShutterChannel(fxi, shutterVal);
    if (shutterCh != QLCChannel::invalid())
        svs << SceneValue(fxi->id(), shutterCh, shutterVal);
}

/** Test-orientation override for a single fixture.
 *
 *  Acquires Override-priority GenericFaders for every universe with
 *  setZeroAll(true) so all other fixtures are blacked out while the test
 *  is active.  The test fixture's fader also writes pan/tilt + full
 *  intensity + open shutter so the fixture is visible on the rig. */
class FixtureOrientationTest
{
public:
    FixtureOrientationTest(Fixture *fxi, Doc *doc)
        : m_fxi(fxi), m_doc(doc)
    {
        quint32 fxUni = fxi->universe();
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        for (int i = 0; i < ua.size(); ++i)
        {
            QSharedPointer<GenericFader> fader = ua[i]->requestFader(Universe::Override);
            fader->setName(QStringLiteral("FixtureTest"));
            fader->setZeroAll(true);
            m_faders[i] = fader;
            m_universes[i] = ua[i];
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
        m_fixtureUniIdx = int(fxUni);
    }

    ~FixtureOrientationTest()
    {
        /* Dismissing a fader only unhooks it (Universe::dismissFader takes it
           out of the list and nothing else) -- it does not clear what the
           fader last wrote. While a scene is running something else overwrites
           those channels on the next tick and nobody notices. In Design mode
           with nothing running, nothing does: the fixture stayed at the test
           position with the lamp open until the app was restarted.

           So zero the channels this test drove, before letting go of the
           fader that is driving them. */
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        for (auto it = m_faders.begin(); it != m_faders.end(); ++it)
        {
            int idx = it.key();
            if (it.value().isNull() || idx >= ua.size())
                continue;

            it.value()->removeAll();
            ua[idx]->dismissFader(it.value());
        }

        /* Reset only the fixture's own address range. Universe::reset() with
           no argument would blank the entire universe, which is a far bigger
           hammer than "stop testing one light" and would knock out anything
           else the operator had running. */
        if (m_fixtureUniIdx >= 0 && m_fixtureUniIdx < ua.size() && m_fxi != nullptr)
            ua[m_fixtureUniIdx]->reset(int(m_fxi->address()), int(m_fxi->channels()));

        m_doc->inputOutputMap()->releaseUniverses(true);
    }

    void setDegrees(float panDeg, float tiltDeg)
    {
        QSharedPointer<GenericFader> fader = m_faders.value(m_fixtureUniIdx);
        if (fader.isNull()) return;

        QList<SceneValue> svs;
        svs << m_fxi->positionToValues(QLCChannel::Pan,  panDeg);
        svs << m_fxi->positionToValues(QLCChannel::Tilt, tiltDeg);
        appendFixtureOnValues(m_fxi, svs);

        // Swapped in whole: an emptied fader caught by its universe's thread
        // went out as a blank frame -- the beam blinked while being dragged.
        QList<FadeChannel> fcs;
        for (const SceneValue &sv : svs)
        {
            FadeChannel fc(m_doc, m_fxi->id(), sv.channel);
            fc.setTarget(sv.value);
            fc.setCurrent(sv.value);
            fc.setFadeTime(0);
            fcs << fc;
        }
        fader->replaceAll(fcs);
    }

private:
    Fixture *m_fxi;
    Doc     *m_doc;
    int      m_fixtureUniIdx = -1;
    QHash<int, QSharedPointer<GenericFader>> m_faders;
    QHash<int, Universe*>                    m_universes;
};

/* ---- Locate appearance, operator-configurable ----------------------------
 *
 * Locate used to be three full-intensity white flashes, which is a lot of
 * light for "which one is this" -- painful next to the fixture, and in a
 * blacked-out house it lights the room. Colour, level and count are settings
 * so it can be dialled down to something you can stand next to, or up when the
 * rig is in daylight. Read at flash time rather than cached, so a change in
 * Preferences applies to the very next Locate without a restart.
 */
#define SETTINGS_LOCATE_COLOR     "monitor/locate/color"
#define SETTINGS_LOCATE_INTENSITY "monitor/locate/intensity"
#define SETTINGS_LOCATE_FLASHES   "monitor/locate/flashes"

QColor Monitor::locateColor()
{
    QSettings s;
    const QVariant v = s.value(SETTINGS_LOCATE_COLOR);
    QColor c = v.isValid() ? QColor(v.toString()) : QColor(Qt::white);
    return c.isValid() ? c : QColor(Qt::white);
}

int Monitor::locateIntensity()
{
    QSettings s;
    const int v = s.value(SETTINGS_LOCATE_INTENSITY, 255).toInt();
    return qBound(1, v, 255);
}

int Monitor::locateFlashes()
{
    QSettings s;
    const int v = s.value(SETTINGS_LOCATE_FLASHES, 3).toInt();
    return qBound(1, v, 10);
}

/** Values that make a fixture visible for LOCATE, at the configured colour and
 *  level -- as opposed to appendFixtureOnValues(), which is the flat
 *  everything-to-255 the orientation TEST wants.
 *
 *  Colour is applied per emitter: an RGB(W/A/UV) fixture gets the picked
 *  colour's components, and the white/amber/UV emitters are driven only by
 *  how white the pick actually is, so choosing deep blue does not quietly come
 *  out pale because the white emitter stayed at full. */
static void appendFixtureLocateValues(Fixture *fxi, QList<SceneValue> &svs,
                                      const QColor &colour, int level)
{
    if (!fxi->fixtureMode()) return;

    quint32 mi = fxi->masterIntensityChannel();
    if (mi == QLCChannel::invalid())
    {
        for (quint32 c = 0; c < fxi->channels(); ++c)
        {
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (ch && ch->group() == QLCChannel::Intensity
                && ch->controlByte() == QLCChannel::MSB
                && ch->colour() == QLCChannel::NoColour)
            { mi = c; break; }
        }
    }
    if (mi != QLCChannel::invalid())
        svs << SceneValue(fxi->id(), mi, uchar(level));

    /* A fixture with no dimmer channel has to be dimmed through its emitters,
       or "50% locate" would be full brightness on exactly the LED washes most
       likely to be blinding. */
    const bool scaleByLevel = (mi == QLCChannel::invalid());
    const double k = scaleByLevel ? (level / 255.0) : 1.0;

    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        QLCChannel *ch = fxi->fixtureMode()->channel(c);
        if (!ch || ch->controlByte() != QLCChannel::MSB) continue;
        const bool isColourGroup     = (ch->group() == QLCChannel::Colour);
        const bool isIntensityColour = (ch->group() == QLCChannel::Intensity
                                        && ch->colour() != QLCChannel::NoColour);
        if (!(isColourGroup || isIntensityColour))
            continue;

        int v = 0;
        switch (ch->colour())
        {
        case QLCChannel::Red:   v = colour.red();   break;
        case QLCChannel::Green: v = colour.green(); break;
        case QLCChannel::Blue:  v = colour.blue();  break;
        // White/amber/UV follow the neutral part of the pick: full for white,
        // nothing for a saturated colour.
        case QLCChannel::White: v = qMin(qMin(colour.red(), colour.green()), colour.blue()); break;
        case QLCChannel::Amber: v = qMin(colour.red(), colour.green()) / 2;                  break;
        case QLCChannel::UV:    v = colour.blue() / 4;                                       break;
        default: continue;
        }
        svs << SceneValue(fxi->id(), c, uchar(qBound(0.0, v * k, 255.0)));
    }

    uchar shutterVal = 0;
    quint32 shutterCh = fixtureShutterChannel(fxi, shutterVal);
    if (shutterCh != QLCChannel::invalid())
        svs << SceneValue(fxi->id(), shutterCh, shutterVal);
}

/** Locate flash — no blackout; just overrides the fixture's intensity + shutter
 *  so it pops up over any running scene.  Call setOn(false/true) to strobe. */
class FixtureLocate
{
public:
    FixtureLocate(Fixture *fxi, Doc *doc) : m_fxi(fxi), m_doc(doc)
    {
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        quint32 fxUni = fxi->universe();
        if (fxUni < quint32(ua.size()))
        {
            m_universe = ua[int(fxUni)];
            m_fader = m_universe->requestFader(Universe::Override);
            m_fader->setName(QStringLiteral("FixtureLocate"));
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
        appendFixtureLocateValues(m_fxi, m_onValues,
                                  Monitor::locateColor(), Monitor::locateIntensity());
        setOn(true);
    }

    ~FixtureLocate()
    {
        if (m_universe && !m_fader.isNull())
            m_universe->dismissFader(m_fader);
    }

    void setOn(bool on)
    {
        if (m_fader.isNull()) return;
        // Swapped in whole, never emptied first (see setDegrees).
        QList<FadeChannel> fcs;
        if (on)
            for (const SceneValue &sv : m_onValues)
            {
                FadeChannel fc(m_doc, sv.fxi, sv.channel);
                fc.setTarget(sv.value);
                fc.setCurrent(sv.value);
                fc.setFadeTime(0);
                fcs << fc;
            }
        m_fader->replaceAll(fcs);
    }

private:
    Fixture      *m_fxi;
    Doc          *m_doc;
    Universe     *m_universe = nullptr;
    QSharedPointer<GenericFader> m_fader;
    QList<SceneValue>            m_onValues;
};

// ---------------------------------------------------------------------------

struct Monitor::LocateSession
{
    std::shared_ptr<FixtureLocate> src;
    QTimer *timer = nullptr;
    int step = 0;
};

void Monitor::cancelAllLocateSessions()
{
    qDeleteAll(m_locateSessions);
    m_locateSessions.clear();
}

void Monitor::locateFixture(quint32 fxId)
{
    // Re-triggering while already flashing cancels it early -- matches the
    // rig editor's own Locate button, which stops on a second click too.
    auto it = m_locateSessions.find(fxId);
    if (it != m_locateSessions.end())
    {
        delete it.value();
        m_locateSessions.erase(it);
        return;
    }

    Fixture *fxi = m_doc->fixture(fxId);
    if (fxi == nullptr)
        return;
    // No address, nothing to flash: it would light whatever real fixture
    // happens to sit at the placeholder universe 1 / address 1.
    if (!fxi->isPatched())
        return;

    LocateSession *session = new LocateSession;
    session->src = std::make_shared<FixtureLocate>(fxi, m_doc);
    session->src->setOn(true);
    session->step = 1;
    session->timer = new QTimer(this);
    session->timer->setInterval(300);
    connect(session->timer, &QTimer::timeout, this, [this, fxId]() {
        auto sit = m_locateSessions.find(fxId);
        if (sit == m_locateSessions.end())
            return;
        LocateSession *s = sit.value();
        ++s->step;
        /* Odd steps are on (300 ms), even are off (150 ms), so N flashes take
           2N steps and the run ends on the step after the last off. The count
           came from the rig editor's fixed three; it is a setting now, because
           three full-intensity flashes is a lot of light to stand next to. */
        if (s->step > 2 * Monitor::locateFlashes())
        {
            m_locateSessions.erase(sit);
            delete s;
            return;
        }
        const bool isOn = (s->step % 2 == 1);
        s->src->setOn(isOn);
        s->timer->setInterval(isOn ? 300 : 150);
    });
    m_locateSessions.insert(fxId, session);
    session->timer->start();
}

void Monitor::locateFixtures(const QList<quint32> &ids)
{
    /* Sessions are keyed by fixture id, so a group Locate is just several of
       them running side by side -- each fixture keeps its own flash timing and
       its own fader, and re-triggering the group cancels exactly the ones that
       were already flashing. */
    foreach (quint32 id, ids)
        locateFixture(id);
}

void Monitor::resetFixture(quint32 fxId)
{
    Fixture *fxi = m_doc->fixture(fxId);
    if (fxi == nullptr || fxi->fixtureMode() == nullptr || !fxi->isPatched())
        return;

    // Same "reset" Maintenance-capability detection the Fixture Test
    // quick-dialog uses (see slotTestFixture()'s dialog further down) --
    // duplicated rather than shared since that dialog also handles Identify,
    // which this right-click action doesn't.
    int resetCh = -1;
    uchar resetVal = 0;
    for (int ci = 0; ci < fxi->fixtureMode()->channels().count(); ci++)
    {
        const QLCChannel *ch = fxi->fixtureMode()->channel(ci);
        if (!ch || ch->group() != QLCChannel::Maintenance)
            continue;
        for (const QLCCapability *cap : ch->capabilities())
        {
            if (cap->name().toLower().contains("reset"))
            {
                resetCh = ci;
                resetVal = uchar((cap->min() + cap->max()) / 2);
                break;
            }
        }
        if (resetCh >= 0)
            break;
    }
    if (resetCh < 0)
        return;

    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
    const quint32 u = fxi->universe();
    if (u < quint32(ua.size()))
    {
        Universe *uni = ua[int(u)];
        uni->write(int(fxi->address()) + resetCh, resetVal);
        const QByteArray pg = uni->postGMValues()->mid(0, uni->usedChannels());
        uni->dumpOutput(pg, true);
    }
    m_doc->inputOutputMap()->releaseUniverses(false);
}

class TrussStripWidget : public QWidget
{
public:
    struct Slot
    {
        quint32 fid;
        QString name;
        QColor  gelColor;
        float   offset;   // metres along truss (mutable by drag)
        // When set, this slot is a child BAR hung on the truss (offset = its
        // parentOffset) rather than a fixture — drawn as a relative-length
        // segment, and the edit dialog writes offset back to the bar's
        // parentOffset. barLength = the bar's own length (metres) for the segment.
        quint32 barTrussId = Truss::invalidId();
        float   barLength  = 0.0f;   ///< extent along the strip axis (segment size)
        int     barRun     = 0;      ///< Truss::BarRun (Along/Across/Drop)
        float   barTrueLen = 0.0f;   ///< the bar's real length (metres)
        float   barCross   = 0.0f;   ///< bar cross-shift (metres, 0 = centred)
    };

    // isVertical: true for tower/pipe trusses — draws a vertical elevation bar
    // (offset=0 at bottom, offset=length at top) instead of a horizontal strip.
    // displayFactor/unitStr: multiply metre values by factor for label display.
    TrussStripWidget(float trussLength, const QList<Slot> &slotList,
                     bool isVertical = false,
                     float displayFactor = 1.0f, const QString &unitStr = "m",
                     QWidget *parent = nullptr)
        : QWidget(parent)
        , m_len(trussLength > 0.0f ? trussLength : 1.0f)
        , m_slots(slotList)
        , m_vertical(isVertical)
        , m_displayFactor(displayFactor)
        , m_unitStr(unitStr)
    {
        if (m_vertical)
        {
            setMinimumWidth(150);
            setMinimumHeight(360);   // room for long pipes/drops
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        }
        else
        {
            setMinimumHeight(90);
            setMinimumWidth(320);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        setMouseTracking(true);
    }

    const QList<Slot> &placements() const { return m_slots; }

    void setTrussLength(float len)
    {
        m_len = len > 0.0f ? len : 1.0f;
        for (Slot &s : m_slots)
            s.offset = qBound(0.0f, s.offset, m_len);
        update();
    }

    /** Called with the metre offset when the user right-clicks the strip and
     *  chooses "Add bar here". The dialog wires this to create a child bar. */
    void setAddBarCallback(const std::function<void(float)> &cb) { m_addBarCb = cb; }

    /** Add a bar marker to the strip (after it was created) and repaint. */
    void addBarSlot(quint32 barId, const QString &name, float offset,
                    float barLength, int barRun = 0, float trueLen = 0.0f)
    {
        Slot s;
        s.fid = Fixture::invalidId();
        s.barTrussId = barId;
        s.barLength = barLength;
        s.barRun = barRun;
        s.barTrueLen = trueLen;
        s.name = name;
        s.offset = qBound(0.0f, offset, m_len);
        m_slots.append(s);
        update();
    }

protected:
    // Metre offset along the truss for a widget-pixel position.
    float offsetAtPos(const QPoint &p) const
    {
        if (m_vertical)
        {
            const QRect bar = vBarRect();
            if (bar.height() <= 0) return 0.0f;
            return qBound(0.0f, float(bar.bottom() - p.y()) / bar.height() * m_len, m_len);
        }
        const QRect bar = hBarRect();
        if (bar.width() <= 0) return 0.0f;
        return qBound(0.0f, float(p.x() - bar.left()) / bar.width() * m_len, m_len);
    }

    void contextMenuEvent(QContextMenuEvent *ev) override
    {
        if (!m_addBarCb) return;
        const float off = offsetAtPos(ev->pos());
        QMenu m(this);
        QAction *add = m.addAction(tr("Add bar here"));
        if (m.exec(ev->globalPos()) == add)
            m_addBarCb(off);
    }

protected:
    enum { kPad = 14, kBarThick = 16, kHdW = 40, kHdH = 22 };

    // Horizontal mode: bar runs left-to-right, offset=0 on left
    QRect hBarRect() const
    {
        int margin = kPad + kHdW / 2;
        return QRect(margin, 26, width() - 2 * margin, kBarThick);
    }

    // Vertical mode: bar runs bottom-to-top, offset=0 at bottom
    QRect vBarRect() const
    {
        int margin = kPad + kHdH / 2;
        // Centre the bar in the space BETWEEN the tick-number gutter (left) and
        // the length/label gutter (right) so it reads centred in the container.
        const int leftGutter = 34, rightGutter = 66;
        int barX = leftGutter + (width() - leftGutter - rightGutter - kBarThick) / 2;
        barX = qMax(leftGutter, barX);
        return QRect(barX, margin, kBarThick, height() - 2 * margin);
    }

    // --- Front-elevation horizontal axis (vertical/tower strip only) ---
    // A tower is shown as a vertical line; an Across crossbar is a horizontal
    // segment placed by height (Along, vertical) and cross-shift (horizontal).
    int towerCenterX() const { return vBarRect().center().x(); }
    float hHalfRange() const
    {
        float h = 1.0f;
        for (const Slot &s : m_slots)
            if (s.barTrussId != Truss::invalidId() && s.barRun == 1 /*Across*/)
                h = qMax(h, s.barTrueLen * 0.5f + qAbs(s.barCross));
        return qMax(h, 0.5f);
    }
    qreal hScalePx() const
    {
        const int tx = towerCenterX();
        int half = qMin(tx - 38, width() - tx - 70);
        half = qMax(20, half);
        return qreal(half) / hHalfRange();
    }
    // True for a slot drawn as a horizontal crossbar on the tower elevation.
    bool isVerticalCrossbar(int i) const
    {
        return m_vertical && m_slots[i].barTrussId != Truss::invalidId()
            && m_slots[i].barRun == 1 /*Across*/;
    }

    // Centre pixel for slot i in the appropriate axis
    int slotCentre(int i) const
    {
        if (m_vertical)
        {
            const QRect bar = vBarRect();
            // offset=0 → bottom, offset=len → top
            int cy = bar.bottom() - int(m_slots[i].offset / m_len * bar.height());
            return qBound(bar.top(), cy, bar.bottom());
        }
        else
        {
            const QRect bar = hBarRect();
            int cx = bar.left() + int(m_slots[i].offset / m_len * bar.width());
            return qBound(bar.left(), cx, bar.right());
        }
    }

    QRect handleRect(int i) const
    {
        const bool isBar = (m_slots[i].barTrussId != Truss::invalidId());
        // Tower crossbar: a HORIZONTAL segment, placed by height + cross-shift.
        if (isVerticalCrossbar(i))
        {
            const int kPipe = 12;
            const QRect bar = vBarRect();
            int y = bar.bottom() - int(m_slots[i].offset / m_len * bar.height());
            y = qBound(bar.top(), y, bar.bottom());
            const qreal sc = hScalePx();
            int cx = towerCenterX() + int(m_slots[i].barCross * sc);
            int w  = qMax(kPipe, int(m_slots[i].barTrueLen * sc));
            return QRect(cx - w / 2, y - kPipe / 2, w, kPipe);
        }
        if (m_vertical)
        {
            const QRect bar = vBarRect();
            int startY = bar.bottom() - int(m_slots[i].offset / m_len * bar.height());
            if (isBar)
            {
                // A slim pipe of RELATIVE length running up from the attach point.
                const int kPipe = 12;
                int lenPx = qMax(kPipe, int(m_slots[i].barLength / m_len * bar.height()));
                int top = qBound(bar.top(), startY - lenPx, bar.bottom() - kPipe);
                return QRect(width() / 2 - kPipe / 2, top, kPipe,
                             qMin(lenPx, bar.bottom() - top));
            }
            int c = qBound(bar.top(), startY, bar.bottom());
            return QRect(width() / 2 - kHdW / 2, c - kHdH / 2, kHdW, kHdH);
        }
        else
        {
            const QRect bar = hBarRect();
            int startX = bar.left() + int(m_slots[i].offset / m_len * bar.width());
            if (isBar)
            {
                const int kPipe = 12;
                int lenPx = qMax(kPipe, int(m_slots[i].barLength / m_len * bar.width()));
                int left = qBound(bar.left(), startX, bar.right() - kPipe);
                return QRect(left, bar.top() + kBarThick / 2 - kPipe / 2,
                             qMin(lenPx, bar.right() - left), kPipe);
            }
            int c = qBound(bar.left(), startX, bar.right());
            return QRect(c - kHdW / 2, bar.top() + kBarThick / 2 - kHdH / 2, kHdW, kHdH);
        }
    }

    float tickStep() const
    {
        if (m_len > 20.0f) return 5.0f;
        if (m_len > 8.0f)  return 2.0f;
        if (m_len > 4.0f)  return 1.0f;
        return 0.5f;
    }

    void paintHorizontal(QPainter &p)
    {
        const QRect bar = hBarRect();
        QLinearGradient grad(bar.topLeft(), bar.bottomLeft());
        grad.setColorAt(0.0, QColor(90, 90, 90));
        grad.setColorAt(0.5, QColor(55, 55, 55));
        grad.setColorAt(1.0, QColor(75, 75, 75));
        p.fillRect(bar, grad);
        p.setPen(QColor(140, 140, 140));
        p.drawRect(bar);

        float step = tickStep();
        p.setFont(QFont("sans-serif", 7));
        for (float m = 0.0f; m <= m_len + 0.001f; m += step)
        {
            int x = bar.left() + int((m / m_len) * bar.width());
            bool major = (fmod(double(m), 1.0) < 0.01);
            p.setPen(QColor(130, 130, 130));
            p.drawLine(x, bar.bottom(), x, bar.bottom() + (major ? 7 : 4));
            if (major)
            {
                p.setPen(QColor(150, 150, 150));
                p.drawText(x - 14, bar.bottom() + 9, 28, 12,
                           Qt::AlignHCenter,
                           QString::number(double(m * m_displayFactor), 'f', 1));
            }
        }
        p.setPen(QColor(120, 120, 120));
        p.drawText(bar.right() + 2, bar.bottom() + 9, 20, 12, Qt::AlignLeft, m_unitStr);
        p.drawText(bar.left() - 7, bar.bottom() + 9, 14, 12, Qt::AlignHCenter, "0");
    }

    void paintVertical(QPainter &p)
    {
        const QRect bar = vBarRect();
        QLinearGradient grad(bar.topLeft(), bar.topRight());
        grad.setColorAt(0.0, QColor(90, 90, 90));
        grad.setColorAt(0.5, QColor(55, 55, 55));
        grad.setColorAt(1.0, QColor(75, 75, 75));
        p.fillRect(bar, grad);
        p.setPen(QColor(140, 140, 140));
        p.drawRect(bar);

        // Label "Top" / "Base"
        p.setPen(QColor(150, 150, 150));
        p.setFont(QFont("sans-serif", 7));
        p.drawText(bar.right() + 4, bar.top() + 10, 40, 12, Qt::AlignLeft,
                   QString("%1%2").arg(double(m_len * m_displayFactor), 0, 'f', 1).arg(m_unitStr));
        p.drawText(bar.right() + 4, bar.bottom() - 4, 40, 12, Qt::AlignLeft,
                   QString("0%1").arg(m_unitStr));

        float step = tickStep();
        for (float m = 0.0f; m <= m_len + 0.001f; m += step)
        {
            // offset=0 → bottom of bar
            int y = bar.bottom() - int((m / m_len) * bar.height());
            bool major = (fmod(double(m), 1.0) < 0.01);
            p.setPen(QColor(130, 130, 130));
            p.drawLine(bar.left() - (major ? 7 : 4), y, bar.left(), y);
            if (major)
            {
                p.setPen(QColor(150, 150, 150));
                p.drawText(bar.left() - 32, y - 6, 28, 12,
                           Qt::AlignRight, QString::number(int(m + 0.5f)));
            }
        }
    }

    void paintHandles(QPainter &p)
    {
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < m_slots.size(); ++i)
            {
                bool dragging = (m_dragIdx == i);
                if ((pass == 0) == dragging) continue;

                QRect r = handleRect(i);
                const bool isBar = (m_slots[i].barTrussId != Truss::invalidId());
                QColor fill = isBar ? QColor(220, 150, 40)   // bars = amber
                            : (m_slots[i].gelColor.isValid()
                              ? m_slots[i].gelColor : QColor(60, 120, 200));
                if (dragging) fill = fill.lighter(135);

                if (isBar)
                {
                    // Draw as a rounded PIPE with a light centreline highlight,
                    // and the label BESIDE it (the pipe is too slim for text).
                    p.setRenderHint(QPainter::Antialiasing, true);
                    p.setBrush(fill);
                    p.setPen(QPen(dragging ? Qt::white : QColor(150, 95, 15), 1));
                    const int rad = qMin(r.width(), r.height()) / 2;
                    p.drawRoundedRect(r, rad, rad);
                    p.setPen(QPen(QColor(255, 225, 160, 180), 1));
                    if (m_vertical)
                        p.drawLine(r.center().x(), r.top() + 2, r.center().x(), r.bottom() - 2);
                    else
                        p.drawLine(r.left() + 2, r.center().y(), r.right() - 2, r.center().y());
                    p.setRenderHint(QPainter::Antialiasing, false);

                    QString label = m_slots[i].name;
                    if (label.length() > 10) label = label.left(9) + QChar(0x2026);
                    p.setPen(QColor(230, 200, 140));
                    QFont f("sans-serif", 7); f.setBold(dragging); p.setFont(f);
                    if (m_vertical)
                        p.drawText(r.right() + 3, r.center().y() - 6, 60, 12,
                                   Qt::AlignLeft | Qt::AlignVCenter, label);
                    else
                        p.drawText(r.center().x() - 30, r.top() - 13, 60, 12,
                                   Qt::AlignHCenter | Qt::AlignBottom, label);
                    continue;
                }

                p.fillRect(r, fill);
                p.setPen(dragging ? Qt::white : QColor(200, 200, 200));
                p.drawRect(r);

                p.save();
                p.setClipRect(r.adjusted(1, 1, -1, -1));
                p.setPen(Qt::white);
                QFont f("sans-serif", 7);
                f.setBold(dragging);
                p.setFont(f);
                QString label = m_slots[i].name;
                if (label.length() > 10)
                    label = label.left(9) + QChar(0x2026);
                p.drawText(r, Qt::AlignCenter, label);
                p.restore();

                if (dragging)
                {
                    p.setPen(Qt::white);
                    p.setFont(QFont("sans-serif", 7));
                    QString val = QString("%1%2").arg(double(m_slots[i].offset * m_displayFactor), 0, 'f', 2).arg(m_unitStr);
                    if (m_vertical)
                        p.drawText(r.left(), r.bottom() + 2, r.width(), 12,
                                   Qt::AlignHCenter, val);
                    else
                        p.drawText(r.left(), r.bottom() + 2, r.width(), 12,
                                   Qt::AlignHCenter, val);
                }
            }
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        if (m_vertical)
            paintVertical(p);
        else
            paintHorizontal(p);

        paintHandles(p);

        if (m_slots.isEmpty())
        {
            p.setPen(QColor(120, 120, 120));
            p.setFont(QFont("sans-serif", 9));
            p.drawText(rect(), Qt::AlignCenter, "No fixtures on this truss");
        }
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton) return;
        m_dragIdx = -1;
        for (int i = 0; i < m_slots.size(); ++i)
        {
            if (handleRect(i).contains(ev->pos()))
            {
                m_dragIdx   = i;
                m_anchorPx  = m_vertical ? qlcEventPos(ev).y() : qlcEventPos(ev).x();
                m_anchorPxX = qlcEventPos(ev).x();
                m_anchorOff = m_slots[i].offset;
                m_anchorCross = m_slots[i].barCross;
                setCursor(Qt::ClosedHandCursor);
                update();
                return;
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if (m_dragIdx < 0)
        {
            bool overHandle = false;
            for (int i = 0; i < m_slots.size(); ++i)
                if (handleRect(i).contains(ev->pos())) { overHandle = true; break; }
            setCursor(overHandle ? Qt::OpenHandCursor : Qt::ArrowCursor);
            return;
        }
        if (isVerticalCrossbar(m_dragIdx))
        {
            // 2D placement: vertical = height (Along), horizontal = cross-shift.
            const QRect bar = vBarRect();
            if (bar.height() > 0)
            {
                float dOff = -float(qlcEventPos(ev).y() - m_anchorPx) / float(bar.height()) * m_len;
                m_slots[m_dragIdx].offset = qBound(0.0f, m_anchorOff + dOff, m_len);
            }
            const qreal sc = hScalePx();
            if (sc > 0.0)
            {
                float c = m_anchorCross + float(qlcEventPos(ev).x() - m_anchorPxX) / float(sc);
                if (qAbs(c * sc) < 9.0)   // bump-snap to centred on the truss
                    c = 0.0f;
                m_slots[m_dragIdx].barCross = c;
            }
            update();
            return;
        }
        if (m_vertical)
        {
            const QRect bar = vBarRect();
            if (bar.height() <= 0) return;
            // Moving up (smaller y) increases offset for a tower
            float delta = -float(qlcEventPos(ev).y() - m_anchorPx) / float(bar.height()) * m_len;
            m_slots[m_dragIdx].offset = qBound(0.0f, m_anchorOff + delta, m_len);
        }
        else
        {
            const QRect bar = hBarRect();
            if (bar.width() <= 0) return;
            float delta = float(qlcEventPos(ev).x() - m_anchorPx) / float(bar.width()) * m_len;
            m_slots[m_dragIdx].offset = qBound(0.0f, m_anchorOff + delta, m_len);
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        m_dragIdx = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }

private:
    float       m_len;
    QList<Slot> m_slots;
    bool        m_vertical     = false;
    float       m_displayFactor = 1.0f;
    QString     m_unitStr       = "m";
    int         m_dragIdx   = -1;
    int         m_anchorPx  = 0;
    int         m_anchorPxX = 0;     ///< x anchor for 2D crossbar drag
    float       m_anchorOff = 0.0f;
    float       m_anchorCross = 0.0f;
    std::function<void(float)> m_addBarCb;
};

// ---------------------------------------------------------------------------

void Monitor::slotEditTruss(quint32 tid)
{
    Truss *t = m_props->truss(tid);
    if (!t) return;
    RigEditSession edit(this);


    QDialog editDlg(this);
    // Named by KIND: a truss and its auto-group share a name, and both used to
    // open a window titled identically, with no way to tell which you had.
    editDlg.setWindowTitle(tr("Lighting Studio Editor — Truss %1").arg(t->name()));
    editDlg.setMinimumWidth(420);

    QVBoxLayout *vl   = new QVBoxLayout(&editDlg);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &editDlg);
    form->addRow(tr("Name:"), nameEdit);

    QComboBox *typeCb = new QComboBox(&editDlg);
    typeCb->addItems({ tr("Horizontal (flat / in-plane)"),
                       tr("Vertical (tower / hanging drop)"),
                       tr("Ground (floor)") });
    typeCb->setCurrentIndex(static_cast<int>(t->type()));
    typeCb->setToolTip(tr("The PLANE, not the on-screen direction: a Horizontal "
                          "truss/bar lies flat and can run any way in the plane — "
                          "its screen orientation is the Direction angle below. "
                          "Vertical = a tower, or (as a bar) a drop that hangs down."));
    form->addRow(tr("Type:"), typeCb);

    const bool isFeet_e = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp_e   = isFeet_e ? 3.28084 : 1.0;
    const double fromDisp_e = isFeet_e ? (1.0 / 3.28084) : 1.0;
    const double posRange_e = isFeet_e ? 164.0 : 50.0;
    const double lenMax_e   = isFeet_e ? 328.0 : 100.0;
    const double zMax_e     = isFeet_e ? 98.4  : 30.0;
    const double wMax_e     = isFeet_e ? 16.4  : 5.0;

    QDoubleSpinBox *originX = new FeetInchesSpinBox(isFeet_e, &editDlg);
    originX->setRange(-posRange_e, posRange_e); originX->setDecimals(2);
    originX->setValue(t->origin().x() * toDisp_e);
    form->addRow(tr("Origin X:"), originX);

    QDoubleSpinBox *originY = new FeetInchesSpinBox(isFeet_e, &editDlg);
    originY->setRange(-posRange_e, posRange_e); originY->setDecimals(2);
    originY->setValue(t->origin().y() * toDisp_e);
    form->addRow(tr("Origin Y:"), originY);

    QDoubleSpinBox *originZ = new FeetInchesSpinBox(isFeet_e, &editDlg);
    originZ->setRange(0, zMax_e); originZ->setDecimals(2);
    originZ->setValue(t->origin().z() * toDisp_e);
    form->addRow(tr("Height Z:"), originZ);

    float existingAngle = float(qRadiansToDegrees(
        qAtan2(double(t->direction().y()), double(t->direction().x()))));
    if (existingAngle < 0) existingAngle += 360.0f;
    QDoubleSpinBox *dirAngle = new QDoubleSpinBox(&editDlg);
    dirAngle->setRange(0, 359); dirAngle->setSuffix(QString::fromUtf8("°"));
    dirAngle->setDecimals(1); dirAngle->setValue(existingAngle);
    dirAngle->setToolTip(tr("On-screen run of a flat truss/bar: 0° = points stage-"
                            "right, 90° = points upstage. This is what makes it look "
                            "horizontal or vertical on the 2D map."));
    form->addRow(tr("Direction (°):"), dirAngle);

    QDoubleSpinBox *lenSpin = new FeetInchesSpinBox(isFeet_e, &editDlg);
    lenSpin->setRange(0.1, lenMax_e); lenSpin->setDecimals(2);
    lenSpin->setValue(t->length() * toDisp_e);
    form->addRow(tr("Length:"), lenSpin);

    QDoubleSpinBox *widthSpin = new FeetInchesSpinBox(isFeet_e, &editDlg);
    widthSpin->setRange(0.05, wMax_e); widthSpin->setDecimals(2);
    widthSpin->setValue(t->width() * toDisp_e);
    form->addRow(tr("Width:"), widthSpin);

    QComboBox *profileCb = new QComboBox(&editDlg);
    profileCb->addItem(tr("Square (box)"), int(Truss::SquareTruss));
    profileCb->addItem(tr("Triangle"),     int(Truss::TriangleTruss));
    profileCb->setCurrentIndex(profileCb->findData(int(t->profile())));
    profileCb->setToolTip(tr("Cross-section shape — drawn distinctly on the map."));
    form->addRow(tr("Profile:"), profileCb);

    // --- Stand on a Stand: origin rides the stand top (Origin X/Y/Z ignored) ---
    QComboBox *standCb = new QComboBox(&editDlg);
    standCb->addItem(tr("(free — on the floor)"), quint32(Stand::invalidId()));
    for (Stand *st : m_props->stands())
    {
        standCb->addItem(st->name().isEmpty() ? tr("Stand %1").arg(st->id()) : st->name(), st->id());
        if (st->id() == t->standId())
            standCb->setCurrentIndex(standCb->count() - 1);
    }
    standCb->setToolTip(tr("Stand this truss ON a stand — its origin then rides "
                           "the stand top and follows it."));
    form->addRow(tr("On stand:"), standCb);

    /* Its other end on a second stand: a horizontal truss between two stands
       (a goal post), its run and length following them. */
    QComboBox *endStandCb = new QComboBox(&editDlg);
    endStandCb->setObjectName(QStringLiteral("trussEndStand"));
    endStandCb->addItem(tr("(none — its own length and direction)"), quint32(Stand::invalidId()));
    for (Stand *st : m_props->stands())
    {
        endStandCb->addItem(st->name().isEmpty() ? tr("Stand %1").arg(st->id()) : st->name(), st->id());
        if (st->id() == t->endStandId())
            endStandCb->setCurrentIndex(endStandCb->count() - 1);
    }
    endStandCb->setToolTip(tr("A horizontal truss between two stands: its other end rests on this "
                              "one, and its direction and length follow the two stands."));
    form->addRow(tr("Other end on stand:"), endStandCb);
    auto spanState = [=]() {
        const bool horizontal = typeCb->currentIndex() == int(Truss::Horizontal);
        const bool onStand = standCb->currentData().toUInt() != Stand::invalidId();
        endStandCb->setEnabled(horizontal && onStand);
        const bool spans = endStandCb->isEnabled()
                           && endStandCb->currentData().toUInt() != Stand::invalidId()
                           && endStandCb->currentData().toUInt() != standCb->currentData().toUInt();
        // The stands set them.
        lenSpin->setEnabled(!spans);
        dirAngle->setEnabled(!spans);
    };
    connect(typeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg, [=](int) { spanState(); });
    connect(standCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg, [=](int) { spanState(); });
    connect(endStandCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg, [=](int) { spanState(); });
    spanState();

    /* A sock: diffusing fabric over the truss, for LED bars rigged Inside it
       to glow through. Its colour (white unless told otherwise) and the
       stretch of the truss it covers. */
    QWidget *sockRow = new QWidget(&editDlg);
    QHBoxLayout *sockLay = new QHBoxLayout(sockRow);
    sockLay->setContentsMargins(0, 0, 0, 0);
    QCheckBox *sockCb = new QCheckBox(tr("Sock"), sockRow);
    sockCb->setChecked(t->hasSock());
    sockCb->setToolTip(tr("A diffusing fabric sleeve over the truss: LED bars rigged "
                          "Inside it glow through it instead of throwing beams"));
    QColor *sockColour = new QColor(t->sockColour());
    ColourSwatchButton *sockColourBtn = new ColourSwatchButton(*sockColour, tr("Sock colour"), sockRow);
    sockColourBtn->setResetColour(Qt::white);
    sockColourBtn->setToolTip(tr("The sock's colour"));
    QDoubleSpinBox *sockFromSpin = new FeetInchesSpinBox(isFeet_e, sockRow);
    QDoubleSpinBox *sockToSpin = new FeetInchesSpinBox(isFeet_e, sockRow);
    for (QDoubleSpinBox *sp : { sockFromSpin, sockToSpin })
    {
        sp->setRange(0.0, lenMax_e);
        sp->setDecimals(2);
    }
    sockFromSpin->setValue(t->sockFrom() * toDisp_e);
    sockToSpin->setValue(t->sockTo() * toDisp_e);
    sockFromSpin->setToolTip(tr("Where along the truss the sock starts"));
    sockToSpin->setToolTip(tr("Where along the truss it ends -- its far end covers the whole truss"));
    sockLay->addWidget(sockCb);
    sockLay->addWidget(sockColourBtn);
    sockLay->addWidget(new QLabel(tr("from"), sockRow));
    sockLay->addWidget(sockFromSpin);
    sockLay->addWidget(new QLabel(tr("to"), sockRow));
    sockLay->addWidget(sockToSpin);
    form->addRow(tr("Sock:"), sockRow);
    QString *sockPattern = new QString(t->sockPattern());
    float *sockOpacity = new float(t->sockPatternOpacity());
    std::function<void()> applySockFn;               // set below, once the canvas exists
    QWidget *sockPatternRow = makePatternPicker(&editDlg, sockPattern, sockOpacity, [&applySockFn]() {
        if (applySockFn) applySockFn();
    });
    sockPatternRow->setEnabled(t->hasSock());
    form->addRow(tr("Sock printed:"), sockPatternRow);

    // --- Bar attachment: make this truss a "bar" hung on a parent truss ---
    QComboBox *parentCb = new QComboBox(&editDlg);
    parentCb->addItem(tr("(free — not a bar)"), Truss::invalidId());
    for (Truss *pt : m_props->trusses())
        if (pt->id() != tid)   // can't parent to self
            parentCb->addItem(pt->name().isEmpty() ? tr("Truss %1").arg(pt->id()) : pt->name(),
                              pt->id());
    { int pidx = parentCb->findData(t->parentTrussId()); parentCb->setCurrentIndex(pidx >= 0 ? pidx : 0); }
    parentCb->setToolTip(tr("Hang this truss as a BAR on a parent truss. Its origin "
                            "then follows the parent (Origin X/Y/Z are ignored)."));
    form->addRow(tr("Bar on truss:"), parentCb);

    // Along the parent truss (slides top↔bottom on a tower, left↔right overhead).
    QDoubleSpinBox *alongSpin = new FeetInchesSpinBox(isFeet_e, &editDlg);
    alongSpin->setRange(-lenMax_e, lenMax_e); alongSpin->setDecimals(2);
    alongSpin->setValue(t->parentOffset() * toDisp_e);
    alongSpin->setToolTip(tr("Position ALONG the parent truss"));
    QLabel *alongLbl = new QLabel(tr("Along truss:")); form->addRow(alongLbl, alongSpin);

    // Which face of the truss the bar rides (stage-relative).
    QComboBox *faceCb = new QComboBox(&editDlg);
    faceCb->addItem(tr("Bottom (under-hung)"), Truss::FaceBottom);
    faceCb->addItem(tr("Top"),                 Truss::FaceTop);
    faceCb->addItem(tr("Downstage (front)"),   Truss::FaceDownstage);
    faceCb->addItem(tr("Upstage (back)"),      Truss::FaceUpstage);
    faceCb->addItem(tr("Stage right"),         Truss::FaceStageRight);
    faceCb->addItem(tr("Stage left"),          Truss::FaceStageLeft);
    faceCb->setCurrentIndex(faceCb->findData(t->barFace()));
    faceCb->setToolTip(tr("Which side of the truss the bar mounts on — e.g. "
                          "Downstage to face the audience."));
    QLabel *faceLbl = new QLabel(tr("Face:")); form->addRow(faceLbl, faceCb);

    // Stand-off off that face (the drop / reach).
    QDoubleSpinBox *standoffSpin = new FeetInchesSpinBox(isFeet_e, &editDlg);
    standoffSpin->setRange(0, zMax_e); standoffSpin->setDecimals(2);
    standoffSpin->setValue(t->barStandoff() * toDisp_e);
    standoffSpin->setToolTip(tr("Distance off that face (0 = on the truss)"));
    QLabel *standoffLbl = new QLabel(tr("Stand-off:")); form->addRow(standoffLbl, standoffSpin);

    // The bar's run.
    QComboBox *runCb = new QComboBox(&editDlg);
    runCb->addItem(tr("Along (parallel to truss)"),   Truss::RunAlong);
    runCb->addItem(tr("Across (pipe / cross-bar)"),   Truss::RunAcross);
    runCb->addItem(tr("Drop (hangs straight down)"),  Truss::RunDrop);
    runCb->setCurrentIndex(runCb->findData(t->barRun()));
    runCb->setToolTip(tr("How the bar runs: along the truss, out as a pipe, or "
                         "hanging straight down."));
    QLabel *runLbl = new QLabel(tr("Run:")); form->addRow(runLbl, runCb);

    // A bar's geometry is DERIVED — hide the raw Origin/Direction/Type rows when
    // this truss is a bar (and toggle live as the parent combo changes).
    auto setBarMode = [&](bool isBar) {
        alongLbl->setVisible(isBar); alongSpin->setVisible(isBar);
        faceLbl->setVisible(isBar);  faceCb->setVisible(isBar);
        standoffLbl->setVisible(isBar); standoffSpin->setVisible(isBar);
        runLbl->setVisible(isBar);   runCb->setVisible(isBar);
        // Raw geometry only for free trusses.
        typeCb->setEnabled(!isBar);
        originX->setEnabled(!isBar); originY->setEnabled(!isBar); originZ->setEnabled(!isBar);
        dirAngle->setEnabled(!isBar);
    };
    setBarMode(parentCb->currentData().toUInt() != Truss::invalidId());
    connect(parentCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg,
            [=](int){ setBarMode(parentCb->currentData().toUInt() != Truss::invalidId()); });

    // LIVE PREVIEW: apply the bar's mount params to the canvas as they change
    // (only while it's already a bar), so the user sees it before committing.
    auto applyBarPreview = [=]() {
        if (!t->isChildBar())
            return;
        t->setParentOffset(float(alongSpin->value() * fromDisp_e));
        t->setBarFace(faceCb->currentData().toInt());
        t->setBarStandoff(float(standoffSpin->value() * fromDisp_e));
        t->setBarRun(runCb->currentData().toInt());
        t->setLength(lenSpin->value() * fromDisp_e);
        m_props->recomputeChildTrusses();
        m_graphicsView->followParentTrusses();
    };
    connect(alongSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [=](double){ applyBarPreview(); });
    connect(standoffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [=](double){ applyBarPreview(); });
    connect(lenSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [=](double){ applyBarPreview(); });
    connect(faceCb,       QOverload<int>::of(&QComboBox::currentIndexChanged),  &editDlg, [=](int){ applyBarPreview(); });
    connect(runCb,        QOverload<int>::of(&QComboBox::currentIndexChanged),  &editDlg, [=](int){ applyBarPreview(); });

    // ------------------------------------------------------------------
    // Fixture placement strip
    // ------------------------------------------------------------------
    QList<TrussStripWidget::Slot> stripSlots;
    const QMap<quint32, FixtureRigProps> &allRig = m_props->fixtureRigPropsMap();
    for (auto it = allRig.cbegin(); it != allRig.cend(); ++it)
    {
        if (it.value().trussId != tid) continue;
        TrussStripWidget::Slot s;
        s.fid    = it.key();
        s.offset = it.value().trussOffset;
        Fixture *fxi = m_doc->fixture(s.fid);
        s.name = fxi ? fxi->name() : QString("Fixture %1").arg(s.fid);
        s.gelColor = m_graphicsView->fixtureGelColor(s.fid);
        stripSlots.append(s);
    }
    // Child BARS hung on this truss also ride the strip (draggable to reposition
    // their attach point — "place bar on truss" via the Fixture Placement strip,
    // which sits on the side for vertical trusses).
    for (Truss *bar : m_props->trusses())
    {
        if (bar->parentTrussId() != tid)
            continue;
        TrussStripWidget::Slot s;
        s.fid    = Fixture::invalidId();
        s.barTrussId = bar->id();
        // Segment length on the strip = the bar's extent along the PARENT'S strip
        // axis. A tower's strip runs in height, so only a DROP bar extends along
        // it; a flat truss's strip runs along it, so only an ALONG bar does.
        // Everything else is a point marker at its attach position.
        if (t->type() == Truss::Vertical)
            s.barLength = (bar->barRun() == Truss::RunDrop) ? bar->length() : 0.0f;
        else
            s.barLength = (bar->barRun() == Truss::RunAlong) ? bar->length() : 0.0f;
        s.barRun     = bar->barRun();
        s.barTrueLen = bar->length();
        s.barCross   = bar->barCrossShift();
        s.offset = bar->parentOffset();
        s.name   = bar->name().isEmpty() ? tr("Bar %1").arg(bar->id()) : bar->name();
        stripSlots.append(s);
    }
    // Sort by current offset so overlapping handles are predictable
    std::sort(stripSlots.begin(), stripSlots.end(),
              [](const TrussStripWidget::Slot &a, const TrussStripWidget::Slot &b){
                  return a.offset < b.offset; });

    bool isVertical = (t->type() == Truss::Vertical);
    TrussStripWidget *strip = new TrussStripWidget(t->length(), stripSlots, isVertical,
                                                   float(toDisp_e), isFeet_e ? "ft" : "m",
                                                   &editDlg);
    connect(lenSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [strip, fromDisp_e](double v){ strip->setTrussLength(float(v * fromDisp_e)); });

    // Right-click the strip → "Add bar here": create a child bar attached at the
    // clicked offset and show it as a draggable marker. The callback only fires
    // while the modal dialog is open, so a stack-local tracks the bars created
    // here — a Cancel removes them again.
    QList<quint32> barsCreatedHere;
    strip->setAddBarCallback([this, tid, t, strip, &barsCreatedHere](float offset) {
        const quint32 barId = createBarOnTruss(tid, offset);
        if (barId == Truss::invalidId())
            return;
        barsCreatedHere.append(barId);
        Truss *bar = m_props->truss(barId);
        // Extent along the PARENT'S strip axis (same rule as the pre-existing
        // bars): a horizontal pipe on a vertical tower is a point, not a pipe.
        float ext = 0.0f;
        if (bar != nullptr)
        {
            if (t->type() == Truss::Vertical)
                ext = (bar->barRun() == Truss::RunDrop) ? bar->length() : 0.0f;
            else
                ext = (bar->barRun() == Truss::RunAlong) ? bar->length() : 0.0f;
        }
        strip->addBarSlot(barId, bar ? bar->name() : tr("Bar"), offset, ext,
                          bar ? bar->barRun() : 0, bar ? bar->length() : 0.0f);
    });

    QLabel *stripLabel = new QLabel(tr("Fixture Placement  (drag to reposition; "
                                       "right-click to add a bar)"), &editDlg);
    QFont sf = stripLabel->font(); sf.setBold(true); stripLabel->setFont(sf);

    // Truss geometry pane = just the form; fixtures are placed directly in the
    // full graphical canvas (drag along the truss), like every other object — no
    // separate placement strip. The strip object stays alive only so any child-
    // bar writes on OK keep working; it is NOT shown.
    stripLabel->setVisible(false);
    strip->setVisible(false);
    StructureStudioView *studioView = nullptr;
    QWidget *geomW = new QWidget(&editDlg);
    QVBoxLayout *gvl = new QVBoxLayout(geomW);
    gvl->setContentsMargins(0, 0, 0, 0);
    gvl->addLayout(form);
    QWidget *bodyPane = makeStudioPane(&editDlg, 2 /*Truss*/, tid, geomW, &studioView);
    auto reloadStudio = [studioView]() { if (studioView) studioView->reload(); };
    // Free-truss geometry applies live so the canvas tracks the fields.
    auto applyGeomLive = [=]() {
        if (t->isChildBar()) { reloadStudio(); return; }
        t->setType(static_cast<Truss::TrussType>(typeCb->currentIndex()));
        t->setOrigin(QVector3D(originX->value() * fromDisp_e,
                               originY->value() * fromDisp_e,
                               originZ->value() * fromDisp_e));
        const float rad = float(qDegreesToRadians(dirAngle->value()));
        t->setDirection(QPointF(qCos(rad), qSin(rad)));
        t->setLength(lenSpin->value() * fromDisp_e);
        t->setWidth(widthSpin->value() * fromDisp_e);
        t->setProfile(static_cast<Truss::Profile>(profileCb->currentData().toInt()));
        reloadStudio();
    };
    for (QDoubleSpinBox *sp : { originX, originY, originZ, dirAngle, lenSpin, widthSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg,
                [applyGeomLive](double){ applyGeomLive(); });
    connect(typeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg,
            [applyGeomLive](int){ applyGeomLive(); });
    connect(profileCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg,
            [applyGeomLive](int){ applyGeomLive(); });
    // The sock applies live too: tick it and the canvas wraps the truss.
    auto applySock = [=]() {
        t->setSock(sockCb->isChecked());
        t->setSockColour(*sockColour);
        const float to = float(sockToSpin->value() * fromDisp_e);
        t->setSockRange(float(sockFromSpin->value() * fromDisp_e),
                        qAbs(to - t->length()) < 1e-3f ? -1.0f : to);   // its end: follows the length
        t->setSockPattern(*sockPattern);
        t->setSockPatternOpacity(*sockOpacity);
        sockColourBtn->setEnabled(sockCb->isChecked());
        sockFromSpin->setEnabled(sockCb->isChecked());
        sockToSpin->setEnabled(sockCb->isChecked());
        sockPatternRow->setEnabled(sockCb->isChecked());
        reloadStudio();
    };
    applySockFn = applySock;
    sockColourBtn->setEnabled(t->hasSock());
    sockFromSpin->setEnabled(t->hasSock());
    sockToSpin->setEnabled(t->hasSock());
    connect(sockCb, &QCheckBox::toggled, &editDlg, [applySock](bool) { applySock(); });
    connect(sockFromSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg,
            [applySock](double) { applySock(); });
    connect(sockToSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg,
            [applySock](double) { applySock(); });
    connect(sockColourBtn, &ColourSwatchButton::colourChanged, &editDlg,
            [sockColour, applySock](const QColor &c) {
        *sockColour = c;
        applySock();
    });
    QObject::connect(&editDlg, &QObject::destroyed, [sockColour, sockPattern, sockOpacity]() {
        delete sockColour;
        delete sockPattern;
        delete sockOpacity;
    });

    // The bar-preview hooks already recompute a child bar — mirror onto the canvas.
    connect(alongSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [reloadStudio](double){ reloadStudio(); });
    connect(standoffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [reloadStudio](double){ reloadStudio(); });

    Q_UNUSED(isVertical)
    vl->addWidget(bodyPane, 1);
    // Tall enough that the Geometry form's ten-plus rows and the fixtures/bars
    // list below it don't have to fight each other for space by default.
    editDlg.resize(960, 700);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &editDlg);
    vl->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &editDlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &editDlg, &QDialog::reject);

    if (editDlg.exec() != QDialog::Accepted)
    {
        edit.cancel();
        return;
    }

    t->setName(nameEdit->text());
    t->setLength(lenSpin->value() * fromDisp_e);
    t->setWidth(widthSpin->value() * fromDisp_e);
    t->setProfile(static_cast<Truss::Profile>(profileCb->currentData().toInt()));

    const quint32 parentId = parentCb->currentData().toUInt();
    t->setParentTrussId(parentId);
    if (t->isChildBar())
    {
        // Truss-LOCAL bar: geometry (origin/direction/type) is DERIVED from these
        // — the raw Type/Origin/Direction fields are ignored for a bar.
        t->setParentOffset(float(alongSpin->value() * fromDisp_e));   // Along
        t->setBarFace(faceCb->currentData().toInt());
        t->setBarStandoff(float(standoffSpin->value() * fromDisp_e));
        t->setBarRun(runCb->currentData().toInt());
        m_props->recomputeChildTrusses();                // derive world geometry now
        m_graphicsView->ensureTrussGroup(parentId);      // bar joins the parent's group
        if (Truss *pt = m_props->truss(parentId))
            if (pt->groupId() != 0)
                t->setGroupId(pt->groupId());
    }
    else
    {
        // Free truss: apply the raw geometry fields.
        t->setType(static_cast<Truss::TrussType>(typeCb->currentIndex()));
        t->setOrigin(QVector3D(originX->value() * fromDisp_e,
                               originY->value() * fromDisp_e,
                               originZ->value() * fromDisp_e));
        const float radians = float(qDegreesToRadians(dirAngle->value()));
        t->setDirection(QPointF(qCos(radians), qSin(radians)));
        // Stand this truss on a Stand (its origin then rides the stand top),
        // and its other end on another (it then spans them).
        t->setStandId(standCb->currentData().toUInt());
        t->setEndStandId(endStandCb->isEnabled() ? endStandCb->currentData().toUInt()
                                                 : quint32(Stand::invalidId()));
        m_props->recomputeStandMounts();
    }

    /* Child-bar positions used to round-trip through the strip's slots on OK
       -- fine while the (now permanently hidden) strip was the only way to
       move one. It is not anymore: a bar dragged on the canvas
       (StructureStudioView::dragBarTo()) already writes straight to the live
       Truss the moment you drag it, same as a fixture always has. Reading
       strip->placements() here unconditionally OVERWROTE that with
       whatever offset the bar had when this dialog first opened -- so a
       canvas drag looked like it worked (it moved on screen immediately)
       right up until OK silently put it back ("moving the bar to the top of
       the truss didn't save its vertical position"). recomputeChildTrusses()
       still runs, in case this dialog's OWN fields (Length/Width/Origin —
       T-1's own geometry, which a bar's derived position depends on) changed. */
    m_props->recomputeChildTrusses();   // bars re-derive origin from THIS truss's own edits

    // followParentTrusses() redraws trusses AND re-derives/repositions any child
    // bars (and their fixtures) — needed when the edited truss is a bar's parent.
    m_graphicsView->followParentTrusses();
    if (m_layersPanel) m_layersPanel->reload();   // reflect the new truss name in the tree
    m_doc->setModified();
    edit.accept();
}

void Monitor::slotAddTruss()
{
    Q_ASSERT(m_graphicsView != NULL);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Truss"));
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("e.g. Front Wash"));
    form->addRow(tr("Name:"), nameEdit);

    QComboBox *typeCb = new QComboBox(&dlg);
    typeCb->addItems({ tr("Horizontal"), tr("Vertical"), tr("Ground") });
    form->addRow(tr("Type:"), typeCb);

    // Pre-fill origin from the right-click position if invoked via context menu.
    QPointF pendMm = m_pendingAddScenePos.isNull()
                     ? QPointF(0, 0)
                     : m_graphicsView->pixelsToRealPosition(
                           m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    m_pendingAddScenePos = QPointF();

    const bool isFeet_a = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp_a   = isFeet_a ? 3.28084 : 1.0;
    const double fromDisp_a = isFeet_a ? (1.0 / 3.28084) : 1.0;
    const double posRange_a = isFeet_a ? 164.0 : 50.0;
    const double lenMax_a   = isFeet_a ? 328.0 : 100.0;
    const double zMax_a     = isFeet_a ? 98.4  : 30.0;
    const double wMax_a     = isFeet_a ? 16.4  : 5.0;

    QDoubleSpinBox *originX = new FeetInchesSpinBox(isFeet_a, &dlg);
    originX->setRange(-posRange_a, posRange_a); originX->setDecimals(2);
    originX->setValue(pendMm.x() / 1000.0 * toDisp_a);
    form->addRow(tr("Origin X (stage left):"), originX);

    QDoubleSpinBox *originY = new FeetInchesSpinBox(isFeet_a, &dlg);
    originY->setRange(-posRange_a, posRange_a); originY->setDecimals(2);
    originY->setValue(pendMm.y() / 1000.0 * toDisp_a);
    form->addRow(tr("Origin Y (downstage):"), originY);

    QDoubleSpinBox *originZ = new FeetInchesSpinBox(isFeet_a, &dlg);
    originZ->setRange(0, zMax_a); originZ->setDecimals(2);
    originZ->setValue(6.0 * toDisp_a);
    form->addRow(tr("Height Z:"), originZ);

    QDoubleSpinBox *dirAngle = new QDoubleSpinBox(&dlg);
    dirAngle->setRange(0, 359); dirAngle->setSuffix(QString::fromUtf8("°")); dirAngle->setDecimals(1);
    form->addRow(tr("Direction (° from stage-right):"), dirAngle);

    QDoubleSpinBox *length = new FeetInchesSpinBox(isFeet_a, &dlg);
    length->setRange(0.1, lenMax_a); length->setDecimals(2);
    length->setValue(6.0 * toDisp_a);
    form->addRow(tr("Length:"), length);

    QDoubleSpinBox *width = new FeetInchesSpinBox(isFeet_a, &dlg);
    width->setRange(0.05, wMax_a); width->setDecimals(2);
    width->setValue(0.29 * toDisp_a);
    form->addRow(tr("Width:"), width);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    Truss *t = m_props->addTruss();
    t->setName(nameEdit->text().isEmpty()
               ? tr("Truss %1").arg(t->id() + 1) : nameEdit->text());
    t->setType(static_cast<Truss::TrussType>(typeCb->currentIndex()));
    t->setOrigin(QVector3D(originX->value() * fromDisp_a,
                           originY->value() * fromDisp_a,
                           originZ->value() * fromDisp_a));

    float radians = qDegreesToRadians(dirAngle->value());
    t->setDirection(QPointF(qCos(radians), qSin(radians)));
    t->setLength(length->value() * fromDisp_a);
    t->setWidth(width->value() * fromDisp_a);
    t->setLayerId(m_props->activeLayerId());   // land on the selected layer

    m_graphicsView->updateTrusses();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
}

void Monitor::slotManageTrusses()
{
    Q_ASSERT(m_graphicsView != NULL);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Manage Trusses"));
    dlg.resize(400, 320);

    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    QListWidget *list = new QListWidget(&dlg);
    vl->addWidget(list);

    const bool isFeet_m = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitStr_m = isFeet_m ? "ft" : "m";
    const double toDisp_m   = isFeet_m ? 3.28084 : 1.0;
    auto repopulate = [&]() {
        list->clear();
        for (Truss *t : m_props->trusses())
        {
            QListWidgetItem *item = new QListWidgetItem(
                QString("[%1] %2  (%3)  Z=%4 %6  L=%5 %6")
                    .arg(t->id()).arg(t->name())
                    .arg(Truss::typeToString(t->type()))
                    .arg(double(t->origin().z()) * toDisp_m, 0, 'f', 2)
                    .arg(double(t->length()) * toDisp_m, 0, 'f', 2)
                    .arg(unitStr_m));
            item->setData(Qt::UserRole, t->id());
            list->addItem(item);
        }
    };
    repopulate();

    QHBoxLayout *btnRow = new QHBoxLayout();
    vl->addLayout(btnRow);

    QPushButton *editBtn   = new QPushButton(tr("Edit…"), &dlg);
    QPushButton *removeBtn = new QPushButton(tr("Remove"), &dlg);
    QPushButton *closeBtn  = new QPushButton(tr("Close"), &dlg);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    connect(removeBtn, &QPushButton::clicked, &dlg, [&]() {
        QListWidgetItem *sel = list->currentItem();
        if (!sel) return;
        quint32 tid = sel->data(Qt::UserRole).toUInt();
        if (QMessageBox::question(&dlg, tr("Remove Truss"),
                tr("Remove truss '%1'?").arg(m_props->truss(tid)->name()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        m_props->removeTruss(tid);
        repopulate();
        m_graphicsView->updateTrusses();
        m_doc->setModified();
    });

    connect(editBtn, &QPushButton::clicked, &dlg, [&]() {
        QListWidgetItem *sel = list->currentItem();
        if (!sel) return;
        quint32 tid = sel->data(Qt::UserRole).toUInt();
        slotEditTruss(tid);
        repopulate();
    });

    dlg.exec();
}

void Monitor::slotShowLabels(bool visible)
{
    Q_ASSERT(m_graphicsView != NULL);

    // Master: bulk-set every layer's labels flag, then render (per-layer flags
    // are the source of truth, so the panel's per-layer toggles stay effective).
    m_graphicsView->showFixturesLabels(visible);
}

void Monitor::slotFixtureMoved(quint32 fid, QPointF pos)
{
    Q_ASSERT(m_graphicsView != NULL);
    // Preserve the mounting height (Z) — an XY move must not zero it, or the
    // fixture would drop to the floor in the elevation views.
    const float z = m_props->fixturePosition(fid, 0, 0).z();
    m_props->setFixturePosition(fid, 0, 0, QVector3D(pos.x(), pos.y(), z));
    m_doc->setModified();
    // The position is only now what the tally should be counting -- the view
    // emitted this before the store had it.
    m_graphicsView->refreshOffGridTally();
}

void Monitor::slotViewClicked()
{
    Q_ASSERT(m_graphicsView != NULL);

    hideFixtureItemEditor();

    // Emit the current fixture selection so the Programming tab can sync.
    QList<quint32> ids;
    for (MonitorFixtureItem *item : m_graphicsView->selectedFixtureItems())
        ids.append(item->fixtureID());
    emit fixturesSelected(ids);
}

void Monitor::hideFixtureItemEditor()
{
    // No-op: fixture properties are now shown in a modal popup dialog.
}

void Monitor::showFixturePropertiesById(quint32 fxId)
{
    MonitorFixtureItem *item = m_graphicsView->fixtureItemForId(fxId);
    if (item)
    {
        // Select the item and use the full editor (includes position/rotation/gel)
        m_graphicsView->scene()->clearSelection();
        item->setSelected(true);
        showFixtureItemEditor();
    }
    else
    {
        // Fixture not placed in 2D view — show the compact test-only dialog
        Fixture *fxi = m_doc->fixture(fxId);
        if (!fxi) return;

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Fixture Test — %1").arg(fxi->name()));
        QVBoxLayout *vl = new QVBoxLayout(&dlg);

        // Header
        QLabel *nameLabel = new QLabel(QString("<b>%1</b>").arg(fxi->name()), &dlg);
        vl->addWidget(nameLabel);
        if (fxi->fixtureDef())
        {
            QLabel *typeLabel = new QLabel(
                fxi->fixtureDef()->manufacturer() + " " + fxi->fixtureDef()->model(), &dlg);
            typeLabel->setStyleSheet("color:#aaa; font-size:10px;");
            vl->addWidget(typeLabel);
        }

        // Address info
        QString addrStr = tr("Universe %1 • Ch %2")
            .arg(int(fxi->universe()) + 1).arg(int(fxi->address()) + 1);
        if (fxi->channels() > 1)
            addrStr += tr("–%1").arg(int(fxi->address()) + int(fxi->channels()));
        if (!fxi->isPatched())
            addrStr = tr("Unpatched (inventory)");
        QLabel *addrLabel = new QLabel(addrStr, &dlg);
        addrLabel->setStyleSheet("color:#aaa; font-size:10px;");
        vl->addWidget(addrLabel);

        QFrame *sep = new QFrame; sep->setFrameShape(QFrame::HLine); vl->addWidget(sep);

        // Identify / Reset buttons (same logic as full dialog)
        int resetCh = -1; uchar resetVal = 0;
        int identifyCh = -1; uchar identifyVal = 0;
        if (fxi->fixtureMode())
        {
            for (int ci = 0; ci < fxi->fixtureMode()->channels().count(); ci++)
            {
                const QLCChannel *ch = fxi->fixtureMode()->channel(ci);
                if (!ch || ch->group() != QLCChannel::Maintenance) continue;
                for (const QLCCapability *cap : ch->capabilities())
                {
                    const QString capName = cap->name().toLower();
                    if (resetCh < 0 && capName.contains("reset"))
                    { resetCh = ci; resetVal = uchar((cap->min() + cap->max()) / 2); }
                    if (identifyCh < 0 && capName.contains("identif"))
                    { identifyCh = ci; identifyVal = uchar((cap->min() + cap->max()) / 2); }
                }
            }
        }

        if (identifyCh >= 0 || resetCh >= 0)
        {
            QHBoxLayout *hl = new QHBoxLayout;
            auto makeMaintBtn = [&](const QString &label, const QString &sentLabel,
                                    const QString &tip, int ch, uchar val) {
                QPushButton *btn = new QPushButton(label, &dlg);
                btn->setToolTip(tip);
                hl->addWidget(btn);
                if (!fxi->isPatched())
                {
                    btn->setEnabled(false);
                    btn->setToolTip(tr("Patch this fixture first: it is in the inventory and has no DMX address"));
                }
                connect(btn, &QPushButton::clicked, [=]() {
                    if (!fxi->isPatched())
                        return;
                    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
                    quint32 u = fxi->universe();
                    if (u < quint32(ua.size()))
                    {
                        Universe *uni = ua[int(u)];
                        uni->write(int(fxi->address()) + ch, val);
                        const QByteArray pg = uni->postGMValues()->mid(0, uni->usedChannels());
                        uni->dumpOutput(pg, true);
                    }
                    m_doc->inputOutputMap()->releaseUniverses(false);
                    btn->setEnabled(false); btn->setText(sentLabel);
                    QTimer::singleShot(3000, btn, [btn, label]() {
                        btn->setEnabled(true); btn->setText(label);
                    });
                });
            };
            if (identifyCh >= 0)
                makeMaintBtn(tr("Identify"), tr("Identifying…"),
                             tr("Flash/beep to confirm DMX address"), identifyCh, identifyVal);
            if (resetCh >= 0)
                makeMaintBtn(tr("Reset"), tr("Resetting…"),
                             tr("Send fixture reset (hold ~3s)"), resetCh, resetVal);
            hl->addStretch();
            vl->addLayout(hl);
        }

        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(bb);
        dlg.exec();
    }
}

// Live preview of how a moving head's facing reads on the 2D stage layout:
// a body square + green facing arrow (panZeroDir) and a faint pan-range wedge
// over DS/US/SR/SL stage labels. Mirrors MonitorFixtureItem's look so the user
// can confirm orientation before saving without hunting for the icon behind the
// dialog. Plain QWidget (no Q_OBJECT) — same pattern as TrussStripWidget.
class FacingPreviewWidget : public QWidget
{
public:
    explicit FacingPreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(116, 116);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setToolTip(tr("Orientation preview (DS=downstage, US=upstage, SR/SL=stage "
                      "right/left).\nGREEN arrow = Front of device (Pan-zero) — drives "
                      "the aim, shown in the 2D view.\nBLUE arrow = icon rotation — "
                      "visual only, never affects the aim."));
    }

    void setHasPan(bool h)      { m_hasPan = h; update(); }
    void setFacing(double deg)  { m_facing = float(deg); update(); }
    void setIconRotation(double deg) { m_rotation = float(deg); update(); }
    void setPanMax(double deg)  { m_panMax = deg > 0 ? float(deg) : 360.0f; update(); }
    void setFace(int f)         { m_face = f; update(); }   ///< studioMount (0=Top,1=Front,2=Side)

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QPointF c = r.center();

        p.fillRect(r, QColor(24, 24, 24));
        p.setPen(QPen(QColor(60, 60, 60), 1));
        p.drawRect(r);

        // Stage-reference labels (screen space): DS bottom, US top, SR/SL sides.
        p.setPen(QColor(150, 150, 150));
        QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
        p.drawText(QRectF(r.left(), r.bottom() - 13, r.width(), 12), Qt::AlignHCenter, tr("DS"));
        p.drawText(QRectF(r.left(), r.top() + 1,     r.width(), 12), Qt::AlignHCenter, tr("US"));
        p.drawText(QRectF(r.right() - 16, r.top(), 15, r.height()), Qt::AlignVCenter | Qt::AlignRight, tr("SR"));
        p.drawText(QRectF(r.left() + 1,   r.top(), 15, r.height()), Qt::AlignVCenter | Qt::AlignLeft,  tr("SL"));

        p.save();
        p.translate(c);
        const qreal bodyHalf = 12.0;

        // Helper: draw an arrow from centre toward (sin a, cos a) — the same
        // mapping the 2D fixture item uses (0°=DS, 90°=SR, 180°=US, 270°=SL).
        auto drawArrow = [&](float angleDeg, qreal reach, qreal halfHead, const QColor &col) {
            const qreal a = qDegreesToRadians(qreal(angleDeg));
            const QPointF dir(qSin(a), qCos(a));
            const QPointF tip  = dir * (bodyHalf + reach);
            const QPointF perp(-dir.y(), dir.x());
            const QPointF base = tip - dir * (halfHead * 1.8);
            p.setPen(QPen(col, 2));
            p.drawLine(QPointF(0, 0), tip);
            p.setBrush(col);
            p.setPen(QPen(col, 1));
            QPolygonF head; head << tip << (base + perp * halfHead) << (base - perp * halfHead);
            p.drawPolygon(head);
        };
        auto drawBody = [&](float rotDeg) {
            p.save(); p.rotate(rotDeg);
            p.setPen(QPen(QColor(150, 150, 150), 1)); p.setBrush(QColor(33, 33, 33));
            p.drawRect(QRectF(-bodyHalf, -bodyHalf, 2 * bodyHalf, 2 * bodyHalf));
            p.restore();
        };

        // No pan: a FIXED fixture still faces a direction, from its mounting face —
        // Front→DS (out to the audience), Side→SR, Top→straight down (a target).
        if (!m_hasPan)
        {
            drawBody(0);
            if (m_face == 0)   // top face: beam perpendicular to this plan view
            {
                p.setPen(QPen(QColor(80, 200, 120), 1.6)); p.setBrush(QColor(80, 200, 120));
                p.drawEllipse(QPointF(0, 0), 4.5, 4.5);
                p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(0, 0), 9, 9);
            }
            else               // front→DS (0°), side→SR (90°)
            {
                drawArrow((m_face == 2) ? 90.0f : 0.0f, 28.0, 5.0, QColor(80, 200, 120));
            }
            p.restore();
            return;
        }

        // Pan-range wedge — centred on the FACING (Front of device).
        const qreal wedgeReach = bodyHalf + 30.0;
        QRectF arcR(-wedgeReach, -wedgeReach, 2 * wedgeReach, 2 * wedgeReach);
        const int startQt = qRound((270.0 + double(m_facing) - double(m_panMax) / 2.0) * 16.0);
        const int spanQt  = qRound(double(m_panMax) * 16.0);
        p.setPen(QPen(QColor(120, 90, 160, 130), 2));
        p.drawArc(arcR, startQt, spanQt);

        drawBody(m_rotation);                                  // body: icon rotation only
        drawArrow(m_rotation, 20.0, 4.5, QColor(90, 150, 230)); // BLUE = icon rotation
        drawArrow(m_facing, 28.0, 5.0, QColor(80, 200, 120));   // GREEN = facing (drives aim)

        p.restore();
    }

private:
    bool  m_hasPan   = true;
    int   m_face     = 1;      ///< studioMount for a fixed fixture's facing
    float m_facing   = 0.0f;
    float m_rotation = 0.0f;
    float m_panMax   = 360.0f;
};

void Monitor::showFixtureItemEditor(quint32 onlyFid)
{
    // A specific fixture (studio single-fixture path) bypasses the map selection
    // — the group "select-together" would otherwise re-select all members and
    // force the multi editor.
    QList<quint32> fids;
    if (onlyFid != Fixture::invalidId())
        fids << onlyFid;
    else
        foreach (MonitorFixtureItem *it, m_graphicsView->selectedFixtureItems())
            fids << it->fixtureID();
    showFixtureItemEditor(fids);
}

void Monitor::editFixturesProperties(const QList<quint32> &fids)
{
    showFixtureItemEditor(fids);
}

void Monitor::showFixtureItemEditor(const QList<quint32> &fidList)
{
    QList<MonitorFixtureItem *> items;
    QList<quint32> fids;
    foreach (quint32 fid, fidList)
    {
        if (MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fid))
        {
            items << it;
            fids << fid;
        }
    }
    if (items.isEmpty())
        return;
    // Live as it is edited; Cancel puts back whatever was tried; OK is one
    // undo step.
    RigEditSession edit(this);

    // ---- Several fixtures: the same rows, "mixed" where they differ ----
    if (items.count() > 1)
    {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Fixture Properties — %n fixtures", "", fids.count()));
        dlg.setProperty("helpTopic", QStringLiteral("lighting-studio.md#mounting-a-fixture"));
        QVBoxLayout *vl = new QVBoxLayout(&dlg);
        QLabel *head = new QLabel(tr("<b>%1 fixtures.</b> A row they differ on reads "
                                     "“mixed”; set it, and it is set on all of them.")
                                  .arg(fids.count()), &dlg);
        head->setWordWrap(true);
        vl->addWidget(head);

        QGroupBox *box = new QGroupBox(tr("Fixture"), &dlg);
        QFormLayout *form = new QFormLayout(box);
        FixtureFields *fields = new FixtureFields(this, form, box);
        fields->setFixtures(fids, fids.first());

        QDoubleSpinBox *panZeroSpin = new QDoubleSpinBox(box);
        panZeroSpin->setObjectName(QStringLiteral("fixtureAim"));
        panZeroSpin->setMaximum(359);
        panZeroSpin->setSuffix(QString::fromUtf8("°"));
        panZeroSpin->setDecimals(1);
        form->addRow(tr("Aim — front of device (° from DS):"), panZeroSpin);
        QDoubleSpinBox *rotSpin = new QDoubleSpinBox(box);
        rotSpin->setObjectName(QStringLiteral("fixtureIconRotation"));
        rotSpin->setMaximum(359);
        rotSpin->setDecimals(0);
        rotSpin->setSuffix(QString::fromUtf8("°"));
        form->addRow(tr("Icon rotation (visual):"), rotSpin);
        vl->addWidget(box);

        auto loadExtras = [this, fids, panZeroSpin, rotSpin]() {
            const float aim = m_props->fixtureRigProps(fids.first()).panZeroDir;
            const float rot = m_props->fixtureItem(fids.first(), 0, 0).m_rotation.y();
            bool aimDiffers = false, rotDiffers = false;
            foreach (quint32 fid, fids)
            {
                aimDiffers = aimDiffers || !qFuzzyCompare(1.0f + m_props->fixtureRigProps(fid).panZeroDir, 1.0f + aim);
                rotDiffers = rotDiffers || !qFuzzyCompare(1.0f + m_props->fixtureItem(fid, 0, 0).m_rotation.y(), 1.0f + rot);
            }
            for (QDoubleSpinBox *sp : { panZeroSpin, rotSpin })
                sp->blockSignals(true);
            FixtureFields::spinMixed(panZeroSpin, aimDiffers, 0.0);
            if (!aimDiffers)
                panZeroSpin->setValue(double(aim));
            FixtureFields::spinMixed(rotSpin, rotDiffers, 0.0);
            if (!rotDiffers)
                rotSpin->setValue(double(rot));
            for (QDoubleSpinBox *sp : { panZeroSpin, rotSpin })
                sp->blockSignals(false);
        };
        loadExtras();
        connect(panZeroSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [this, fids, panZeroSpin, loadExtras](double v) {
            if (FixtureFields::spinIsMixed(panZeroSpin))
                return;
            foreach (quint32 fid, fids)
            {
                FixtureRigProps rp = m_props->fixtureRigProps(fid);
                rp.panZeroDir = float(v);
                m_props->setFixtureRigProps(fid, rp);
                if (MonitorFixtureItem *fi = m_graphicsView->fixtureItemForId(fid))
                    fi->setFacing(rp.panZeroDir);
            }
            loadExtras();
        });
        connect(rotSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [this, fids, rotSpin, loadExtras](double v) {
            if (FixtureFields::spinIsMixed(rotSpin))
                return;
            foreach (quint32 fid, fids)
            {
                m_props->setFixtureRotation(fid, 0, 0, QVector3D(0, float(v), 0));
                if (MonitorFixtureItem *fi = m_graphicsView->fixtureItemForId(fid))
                    fi->setRotation(int(v));
            }
            loadExtras();
        });

        QDialogButtonBox *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(btns);

        if (dlg.exec() != QDialog::Accepted)
        {
            edit.cancel();
            return;
        }
        m_doc->setModified();
        edit.accept();
        return;
    }

    // ---- Single fixture ----
    MonitorFixtureItem *fxItem = items.first();
    Fixture *fxi = m_doc->fixture(fxItem->fixtureID());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Fixture Properties — %1").arg(fxItem->name()));
    dlg.setProperty("helpTopic", QStringLiteral("lighting-studio.md#mounting-a-fixture"));
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    QLineEdit *nameEdit = nullptr;

    // Header: "Name  [right-aligned type]" on one line
    {
        QHBoxLayout *headerRow = new QHBoxLayout;
        // The name is editable here: "when I edit a fixture from studio I
        // should be able to change its name".
        nameEdit = new QLineEdit(fxi != nullptr ? fxi->name() : fxItem->name());
        nameEdit->setObjectName("fixtureNameEdit");
        nameEdit->setPlaceholderText(tr("Fixture name"));
        QFont nf = nameEdit->font();
        nf.setBold(true);
        nameEdit->setFont(nf);
        headerRow->addWidget(new QLabel(tr("Name:")));
        headerRow->addWidget(nameEdit, 2);

        if (fxi && fxi->fixtureDef())
        {
            QString typeName = fxi->fixtureDef()->manufacturer()
                               + QLatin1Char(' ') + fxi->fixtureDef()->model();
            QLabel *typeLabel = new QLabel(typeName);
            typeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            typeLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
            headerRow->addWidget(typeLabel, 1);
        }
        // Where each of its heads really is: the model's LED layout.
        if (fxi != nullptr && LedLayoutDialog::canEdit(m_doc, fxi->id()))
        {
            QPushButton *ledBtn = new QPushButton(tr("LED Layout…"));
            ledBtn->setObjectName(QStringLiteral("ledLayoutButton"));
            ledBtn->setToolTip(tr("Where each of this model's %n head(s) is in the fixture", "",
                                  fxi->heads()));
            const quint32 ledFid = fxi->id();
            connect(ledBtn, &QPushButton::clicked, &dlg, [this, ledFid, propsDlg = &dlg]() {
                LedLayoutDialog::edit(m_doc, ledFid, propsDlg);
            });
            headerRow->addWidget(ledBtn);
        }
        vl->addLayout(headerRow);
    }
    QFrame *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    vl->addWidget(sep);

    // --- Fixture info (read-only, live-updating) ---
    {
        QFormLayout *infoForm = new QFormLayout;
        infoForm->setContentsMargins(0, 2, 0, 4);

        // Live Pan/Tilt readout — only for fixtures that have P/T channels
        QLabel *panLabel  = nullptr;
        QLabel *tiltLabel = nullptr;
        bool hasPan  = fxi && fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB, 0) != QLCChannel::invalid();
        bool hasTilt = fxi && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) != QLCChannel::invalid();

        if (hasPan)
        {
            panLabel = new QLabel("—", &dlg);
            panLabel->setStyleSheet("color: #aaa; font-size: 10px;");
            infoForm->addRow(tr("Pan:"), panLabel);
        }
        if (hasTilt)
        {
            tiltLabel = new QLabel("—", &dlg);
            tiltLabel->setStyleSheet("color: #aaa; font-size: 10px;");
            infoForm->addRow(tr("Tilt:"), tiltLabel);
        }
        vl->addLayout(infoForm);

        // Refresh the P/T labels every 150ms while the dialog is open
        if ((hasPan || hasTilt) && fxi)
        {
            auto readPTLabel = [fxi, this](int chType, QLabel *lbl) {
                if (!lbl) return;
                quint32 msbCh = fxi->channelNumber(chType, QLCChannel::MSB, 0);
                quint32 lsbCh = fxi->channelNumber(chType, QLCChannel::LSB, 0);
                if (msbCh == QLCChannel::invalid()) return;

                uchar msb = 0, lsb = 0;
                {
                    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
                    quint32 u = fxi->universe();
                    if (u < quint32(ua.size()))
                    {
                        msb = ua[int(u)]->postGMValue(int(fxi->address() + msbCh));
                        if (lsbCh != QLCChannel::invalid())
                            lsb = ua[int(u)]->postGMValue(int(fxi->address() + lsbCh));
                    }
                    m_doc->inputOutputMap()->releaseUniverses(false);
                }

                float maxDeg = (chType == QLCChannel::Pan)
                    ? (fxi->fixtureMode() ? fxi->fixtureMode()->physical().focusPanMax()  : 0.0f)
                    : (fxi->fixtureMode() ? fxi->fixtureMode()->physical().focusTiltMax() : 0.0f);
                if (maxDeg <= 0) maxDeg = (chType == QLCChannel::Pan) ? 360.0f : 270.0f;

                bool hasFine = (lsbCh != QLCChannel::invalid());
                quint32 raw16 = hasFine ? (quint32(msb) << 8 | quint32(lsb)) : quint32(msb) << 8;
                float deg = (raw16 / 65535.0f) * maxDeg;

                QString txt;
                if (hasFine)
                    txt = tr("%1° • MSB %2 / Fine %3").arg(deg, 0, 'f', 1).arg(msb).arg(lsb);
                else
                    txt = tr("%1° • DMX %2").arg(deg, 0, 'f', 1).arg(msb);
                lbl->setText(txt);
            };

            QTimer *ptTimer = new QTimer(&dlg);
            ptTimer->setInterval(150);
            connect(ptTimer, &QTimer::timeout, [=]() {
                readPTLabel(QLCChannel::Pan,  panLabel);
                readPTLabel(QLCChannel::Tilt, tiltLabel);
            });
            ptTimer->start();
            // Populate immediately
            readPTLabel(QLCChannel::Pan,  panLabel);
            readPTLabel(QLCChannel::Tilt, tiltLabel);
        }
    }

    QFrame *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    vl->addWidget(sep2);

    /* What it is and how it sits: the studio inspector's rows (FixtureFields),
       live like the rest of this dialog. */
    QGroupBox *fixtureBox = new QGroupBox(tr("Fixture"), &dlg);
    QFormLayout *fixtureForm = new QFormLayout(fixtureBox);
    FixtureFields *fields = new FixtureFields(this, fixtureForm, fixtureBox);
    fields->setFixtures({ fxItem->fixtureID() }, fxItem->fixtureID());
    vl->addWidget(fixtureBox);

    // Its mount's fine-tuning, its aim and calibration, testing it.
    QGroupBox *rigBox = new QGroupBox(tr("Aim & Mount"), &dlg);
    QFormLayout *rigForm = new QFormLayout(rigBox);

    const bool isFeet_fx = (m_props->gridUnits() == MonitorProperties::Feet);
    const double toDisp_fx   = isFeet_fx ? 3.28084 : 1.0;
    const double fromDisp_fx = isFeet_fx ? (1.0 / 3.28084) : 1.0;

    // Fine height nudge above/below a structural mount (truss/bar/tower).
    QDoubleSpinBox *heightOffSpin = new FeetInchesSpinBox(isFeet_fx);
    heightOffSpin->setRange(-10 * toDisp_fx, 10 * toDisp_fx);
    heightOffSpin->setDecimals(2);
    heightOffSpin->setToolTip(tr("Raise (+) or lower (−) the fixture relative to its "
                                 "mount — e.g. lift a followspot above the truss."));
    rigForm->addRow(tr("Height offset:"), heightOffSpin);


    QDoubleSpinBox *panZeroSpin = new QDoubleSpinBox;
    panZeroSpin->setRange(0, 359);
    panZeroSpin->setSuffix(QString::fromUtf8("°"));
    panZeroSpin->setDecimals(1);
    rigForm->addRow(tr("Aim — front of device (° from DS):"), panZeroSpin);

    QDoubleSpinBox *panOffsetSpin = new QDoubleSpinBox;
    panOffsetSpin->setRange(-720, 720);   // 540° pan fixtures need up to ±270°; 720 gives headroom
    panOffsetSpin->setSuffix(QString::fromUtf8("°"));
    panOffsetSpin->setDecimals(1);
    panOffsetSpin->setToolTip(tr("Calibration offset added to every computed pan angle"));
    rigForm->addRow(tr("Pan offset (cal):"), panOffsetSpin);

    QDoubleSpinBox *tiltOffsetSpin = new QDoubleSpinBox;
    tiltOffsetSpin->setRange(-360, 360);  // generous range for any tilt fixture
    tiltOffsetSpin->setSuffix(QString::fromUtf8("°"));
    tiltOffsetSpin->setDecimals(1);
    tiltOffsetSpin->setToolTip(tr("Calibration offset added to every computed tilt angle"));
    rigForm->addRow(tr("Tilt offset (cal):"), tiltOffsetSpin);

    QCheckBox *panInvertCb = new QCheckBox(tr("Invert pan"));
    panInvertCb->setToolTip(tr("Reverse pan direction: use when increasing DMX physically moves\n"
                               "the fixture the wrong way (mirrors the computed value around centre)"));
    QCheckBox *tiltInvertCb = new QCheckBox(tr("Invert tilt"));
    tiltInvertCb->setToolTip(tr("Reverse tilt direction: use when increasing DMX physically tilts\n"
                                "the fixture the wrong way (mirrors the computed value around centre)"));
    QWidget *invertRow = new QWidget;
    QHBoxLayout *invertLayout = new QHBoxLayout(invertRow);
    invertLayout->setContentsMargins(0, 0, 0, 0);
    invertLayout->addWidget(panInvertCb);
    invertLayout->addSpacing(16);
    invertLayout->addWidget(tiltInvertCb);
    invertLayout->addStretch();
    rigForm->addRow(tr("Direction:"), invertRow);

    // Test Orientation controls — mode selector + target combo + toggle button
    QWidget *testRowWidget = new QWidget;
    QHBoxLayout *testRowLayout = new QHBoxLayout(testRowWidget);
    testRowLayout->setContentsMargins(0, 0, 0, 0);
    testRowLayout->setSpacing(4);
    QComboBox *testModeCb = new QComboBox;
    testModeCb->setObjectName(QStringLiteral("testOrientation"));
    testModeCb->addItem(centredPoseName(fxItem->fixtureID()), QVariant(0));
    testModeCb->addItem(tr("Aim at 0° (stage front)"),  QVariant(1));
    testModeCb->addItem(tr("Aim at target:"),            QVariant(2));
    /* The centred pose is straight down only for a hung fixture: say what it
       is for this one, and follow its Orientation as it is changed here. */
    connect(m_props, &MonitorProperties::rigPropsChanged, testModeCb, [this, testFid = fxItem->fixtureID(), testModeCb](quint32 f) {
        if (f == testFid)
            testModeCb->setItemText(0, centredPoseName(testFid));
    });
    testModeCb->setToolTip(tr("Centred: pan and tilt at their middle -- straight down for a hung fixture,\n"
                              "straight up for an upright one -- confirms physical position.\n"
                              "Aim at 0° (stage front): pans the fixture to aim horizontally\n"
                              "toward downstage (0°) given the current panZeroDir setting.\n"
                              "Aim at target: aims at a stage target using rig geometry —\n"
                              "adjust offsets/panZero until it tracks the target correctly."));
    // Target picker — only visible when mode == 2
    QComboBox *testTargetCb = new QComboBox;
    for (StageTarget *t : m_props->stageTargets())
        testTargetCb->addItem(t->name(), t->id());
    testTargetCb->setVisible(false);
    QPushButton *testBtn = new QPushButton(tr("Test"));
    testBtn->setCheckable(true);
    testBtn->setToolTip(tr("While checked, overrides fixture DMX with the selected test pose.\n"
                           "Changes to panZeroDir and offsets take effect immediately.\n"
                           "Blacks out all other fixtures while active."));
    QPushButton *locateBtn = new QPushButton(tr("Locate"));
    locateBtn->setToolTip(tr("Flash this fixture at full intensity 3 times to identify it on the rig.\n"
                             "Works in any running scene without affecting other fixtures."));
    // Both drive real DMX at the fixture's address, which a staged fixture
    // does not have.
    if (fxi && !fxi->isPatched())
    {
        const QString why = tr("Patch this fixture first: it is in the inventory and has no DMX address");
        testBtn->setEnabled(false);
        testBtn->setToolTip(why);
        locateBtn->setEnabled(false);
        locateBtn->setToolTip(why);
    }
    testRowLayout->addWidget(testModeCb, 1);
    testRowLayout->addWidget(testTargetCb, 1);
    testRowLayout->addWidget(testBtn);
    rigForm->addRow(tr("Test orientation:"), testRowWidget);

    // Locate (flash-to-identify) is useful for ANY fixture, not just movers —
    // an LED bar/wash has no orientation to test but still needs identifying
    // on the rig. Kept as its own row, separate from Test Orientation (which
    // is hidden entirely below for a non-mover, since Pan/Tilt-only controls
    // mean nothing without them) so Locate stays visible regardless.
    QWidget *locateRowWidget = new QWidget;
    QHBoxLayout *locateRowLayout = new QHBoxLayout(locateRowWidget);
    locateRowLayout->setContentsMargins(0, 0, 0, 0);
    locateRowLayout->addWidget(locateBtn);
    locateRowLayout->addStretch();
    rigForm->addRow(tr("Identify:"), locateRowWidget);

    connect(testModeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        testTargetCb->setVisible(testModeCb->currentData().toInt() == 2);
    });

    const FixtureRigProps rp = m_props->fixtureRigProps(fxItem->fixtureID());
    heightOffSpin->setValue(double(rp.mountZOffset) * toDisp_fx);
    panZeroSpin->setValue(double(rp.panZeroDir));
    panOffsetSpin->setValue(double(rp.panOffsetDeg));
    tiltOffsetSpin->setValue(double(rp.tiltOffsetDeg));
    panInvertCb->setChecked(rp.panInvert);
    tiltInvertCb->setChecked(rp.tiltInvert);

    // ---- Per-type: show only fields that make sense for THIS fixture ----------
    // Placement is done in the Lighting Studio (drag/drop onto a truss/platform/
    // bar); "Mounted on" says where. Pan/tilt cal + test only for movers.
    const bool hasPanCh  = fxi && fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB, 0) != QLCChannel::invalid();
    const bool hasTiltCh = fxi && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) != QLCChannel::invalid();
    const bool isMover = (hasPanCh || hasTiltCh);
    const bool isStrip = isLinearFixture(fxi);

    auto hideRow = [rigForm](QWidget *w) {
        if (!w) return;
        w->setVisible(false);
        if (QWidget *l = rigForm->labelForField(w)) l->setVisible(false);
    };

    // Height offset only makes sense on a truss / bar / tower mount.
    const bool structMount = rp.trussId != Truss::invalidId()
        || rp.pipeId != Pipe::invalidId() || rp.towerId != Tower::invalidId();
    if (!structMount) hideRow(heightOffSpin);
    // Aim (pan-zero / front of device) applies to movers AND fixed beams (their
    // facing); a strip uses Face+Angle instead. Pan/tilt CAL + Test are movers-only.
    if (isStrip) hideRow(panZeroSpin);
    if (!isMover)
    { hideRow(panOffsetSpin); hideRow(tiltOffsetSpin);
      hideRow(invertRow); hideRow(testRowWidget); }

    vl->addWidget(rigBox);

    // --- Test orientation DMX source (owned by shared_ptr; cleaned up when dialog closes) ---
    const quint32 fxId_dlg = fxItem->fixtureID();
    auto makeTestDegrees = [&](float &outPan, float &outTilt) {
        float panMax = 360.0f, tiltMax = 270.0f;
        if (fxi && fxi->fixtureMode()) {
            float pm = float(fxi->fixtureMode()->physical().focusPanMax());
            float tm = float(fxi->fixtureMode()->physical().focusTiltMax());
            if (pm > 0.0f) panMax  = pm;
            if (tm > 0.0f) tiltMax = tm;
        }
        const float panOffset  = float(panOffsetSpin->value());
        const float tiltOffset = float(tiltOffsetSpin->value());
        const float horizTilt  = qBound(0.0f, tiltMax / 2.0f + 90.0f + tiltOffset, tiltMax);
        const int   mode       = testModeCb->currentData().toInt();

        if (mode == 2) {
            /* "Aim at target": the solver every Aim look uses, so the test
               pose is the show's pose. This had a copy of its own that
               measured the bearing from upstage (180 degrees out) and solved a
               standing head as though it hung. */
            quint32 tgtId = testTargetCb->currentData().toUInt();
            StageTarget *tgt = m_props->stageTarget(tgtId);
            AimSolver::Solution sol;
            if (tgt != nullptr
                && AimSolver::reach(m_doc, fxId_dlg,
                                    AimSolver::aimPointFor(m_doc, fxId_dlg, tgt, false), sol))
            {
                outPan = sol.panDeg;
                outTilt = sol.tiltDeg;
            } else {
                // Fallback: straight down when no rig data
                outPan  = panMax / 2.0f + panOffset;
                outTilt = tiltMax / 2.0f + tiltOffset;
            }
        } else if (mode == 1) {
            /* Level, straight downstage: a point far out front at the head's
               own height, through the same solver. */
            AimSolver::Frame frame;
            if (AimSolver::frameFor(m_doc, fxId_dlg, frame))
            {
                const AimSolver::Solution sol =
                    AimSolver::solve(frame, frame.position + QVector3D(0.0f, 1000.0f, 0.0f));
                outPan = sol.panDeg;
                outTilt = sol.tiltDeg;
            }
            else
            {
                float relPan = -float(panZeroSpin->value());
                while (relPan >  180.0f) relPan -= 360.0f;
                while (relPan < -180.0f) relPan += 360.0f;
                outPan  = qBound(0.0f, panMax / 2.0f + relPan + panOffset, panMax);
                outTilt = horizTilt;
            }
        } else {
            outPan  = panMax / 2.0f + panOffset;
            outTilt = tiltMax / 2.0f + tiltOffset;
        }
    };

    // Shared pointer so it's deleted when the dialog closes regardless of path
    std::shared_ptr<FixtureOrientationTest> testSrc;

    auto refreshTest = [&]() {
        if (!testSrc) return;
        float pd, td;
        makeTestDegrees(pd, td);
        testSrc->setDegrees(pd, td);
    };

    connect(testBtn, &QPushButton::toggled, [&](bool on) {
        if (on && fxi) {
            testSrc = std::make_shared<FixtureOrientationTest>(fxi, m_doc);
            refreshTest();
        } else {
            testSrc.reset();
        }
    });

    // --- Locate: flash the fixture 3× at full intensity without blackout ---
    std::shared_ptr<FixtureLocate> locateSrc;
    QTimer *locateTimer = new QTimer(&dlg);
    locateTimer->setSingleShot(false);
    int locateStep = 0;

    connect(locateBtn, &QPushButton::clicked, [&]() {
        if (locateTimer->isActive())
        {
            locateTimer->stop();
            locateSrc.reset();
            locateBtn->setEnabled(true);
            return;
        }
        if (!fxi) return;
        locateStep = 1;  // step 1 = first "on" phase just started
        locateSrc = std::make_shared<FixtureLocate>(fxi, m_doc);
        locateSrc->setOn(true);
        locateBtn->setEnabled(false);
        locateTimer->setInterval(300);
        locateTimer->start();
    });
    connect(locateTimer, &QTimer::timeout, [&]() {
        ++locateStep;
        // Steps 1,3,5 = on (300 ms); steps 2,4,6 = off (150 ms); step 7 = done
        if (locateStep > 6)
        {
            locateTimer->stop();
            locateSrc.reset();
            locateBtn->setEnabled(true);
            return;
        }
        bool isOn = (locateStep % 2 == 1);
        locateSrc->setOn(isOn);
        locateTimer->setInterval(isOn ? 300 : 150);
    });

    // Helper: flush the scene fader cache for running scenes that contain fxId.
    // Called both when the user makes live edits and on Cancel (to restore).
    auto flushSceneCaches = [&]() {
        foreach (Function *fn, m_doc->functions())
        {
            Scene *sc = qobject_cast<Scene *>(fn);
            if (!sc || !sc->isRunning()) continue;
            for (const SceneValue &sv : sc->values())
                if (sv.fxi == fxId_dlg) { sc->resetRuntime(); goto nextScene_dlg; }
            if (sc->fixtures().contains(fxId_dlg)) { sc->resetRuntime(); goto nextScene_dlg; }
            {
            foreach (quint32 gid, sc->fixtureGroups())
            {
                FixtureGroup *grp = m_doc->fixtureGroup(gid);
                if (grp && grp->fixtureList().contains(fxId_dlg))
                { sc->resetRuntime(); goto nextScene_dlg; }
            }
            }
            nextScene_dlg:;
        }
    };

    // Live-update: when rig orientation params change, write to MonitorProperties
    // immediately so running scenes recompute geometry on the next tick.
    /* Declared ahead of liveRigUpdate so it can call it; the Position block that
       defines it is built further down. */
    std::function<void()> refreshStagePos;

    auto liveRigUpdate = [&]() {
        FixtureRigProps live = m_props->fixtureRigProps(fxId_dlg);
        live.panZeroDir    = float(panZeroSpin->value());
        live.panOffsetDeg  = float(panOffsetSpin->value());
        live.tiltOffsetDeg = float(tiltOffsetSpin->value());
        live.panInvert     = panInvertCb->isChecked();
        live.tiltInvert    = tiltInvertCb->isChecked();
        m_props->setFixtureRigProps(fxId_dlg, live);
        // Keep the 2D facing arrow in sync with the spinbox live.
        if (MonitorFixtureItem *fi = m_graphicsView->fixtureItemForId(fxId_dlg))
            fi->setFacing(live.panZeroDir);
        flushSceneCaches();
        refreshTest();
        if (refreshStagePos) refreshStagePos();   // a mount change moves it
    };
    connect(testModeCb,   QOverload<int>::of(&QComboBox::currentIndexChanged),  [&](int)   { refreshTest(); });
    connect(testTargetCb, QOverload<int>::of(&QComboBox::currentIndexChanged),  [&](int)   { refreshTest(); });
    connect(panZeroSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ liveRigUpdate(); });
    connect(panOffsetSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ liveRigUpdate(); });
    connect(tiltOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ liveRigUpdate(); });
    connect(panInvertCb,    &QCheckBox::toggled, [&](bool) { liveRigUpdate(); });
    connect(tiltInvertCb,   &QCheckBox::toggled, [&](bool) { liveRigUpdate(); });

    // Position & Orientation  (placed above Rig Assignment, per user request).
    // Spinboxes on the left, live facing preview on the right to save height.
    QGroupBox *posBox = new QGroupBox(tr("Position & Orientation"), &dlg);
    QHBoxLayout *posRow = new QHBoxLayout(posBox);
    QFormLayout *posForm = new QFormLayout;
    posRow->addLayout(posForm, 1);
    const QString unit = isFeet_fx ? tr("ft") : tr("m");

    // X / Y on the plot, as every length is entered (the show's units).
    QDoubleSpinBox *xSpin = new FeetInchesSpinBox(isFeet_fx);
    xSpin->setObjectName(QStringLiteral("fixtureX"));
    xSpin->setMaximum(m_graphicsView->gridSize().width());
    xSpin->setSingleStep(0.1); xSpin->setDecimals(2);
    xSpin->setValue(fxItem->realPosition().x() * toDisp_fx / 1000.0);
    posForm->addRow(tr("X:"), xSpin);

    QDoubleSpinBox *ySpin = new FeetInchesSpinBox(isFeet_fx);
    ySpin->setObjectName(QStringLiteral("fixtureY"));
    ySpin->setMaximum(m_graphicsView->gridSize().height());
    ySpin->setSingleStep(0.1); ySpin->setDecimals(2);
    ySpin->setValue(fxItem->realPosition().y() * toDisp_fx / 1000.0);
    posForm->addRow(tr("Y:"), ySpin);

    QSpinBox *rotSpin = new QSpinBox;
    rotSpin->setRange(0, 359);
    rotSpin->setSuffix(QString::fromUtf8("°"));
    rotSpin->setValue(int(fxItem->rotation()));
    posForm->addRow(tr("Icon rotation (visual):"), rotSpin);

    /* Where the fixture actually ENDS UP, in stage coordinates.
     *
       X / Y above are the fixture's own plot XY, which for a
       MOUNTED fixture is not where it is: the position is derived from the
       structure, and the only hint of the height was "10.0 ft along" buried in
       the Rig Assignment text -- an offset along a tower, not a height you
       could read. Nothing in this dialog answered "so where is it?".

       Read-only on purpose: for a mounted fixture the answer belongs to the
       mount, and editing it here would just be overwritten. */
    QLabel *stagePosLbl = new QLabel(&dlg);
    stagePosLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    refreshStagePos = [this, stagePosLbl, fxId_dlg, toDisp_fx, unit]() {
        const QVector3D w = fixtureWorldCentre(m_doc, fxId_dlg);
        stagePosLbl->setText(tr("X %1%4   Y %2%4   Z %3%4")
                             .arg(double(w.x()) * toDisp_fx, 0, 'f', 2)
                             .arg(double(w.y()) * toDisp_fx, 0, 'f', 2)
                             .arg(double(w.z()) * toDisp_fx, 0, 'f', 2)
                             .arg(unit));
    };
    refreshStagePos();
    posForm->addRow(tr("Stage position:"), stagePosLbl);

    // A linear fixture (strip / bar / tape) is oriented by its Face and Angle
    // (the Fixture rows above) — not an icon rotation.
    if (isStrip)
    {
        rotSpin->setVisible(false);
        if (QWidget *l = posForm->labelForField(rotSpin)) l->setVisible(false);
    }

    // Live facing preview: how this fixture's orientation reads on the 2D stage
    // once saved. Reflects the Rig-Assignment pan-zero direction below (and the
    // icon Rotation) as they change.
    {
        const bool hasPanFx = fxi &&
            fxi->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0) != QLCChannel::invalid();
        float panMaxFx = 360.0f;
        if (fxi && fxi->fixtureMode() && fxi->fixtureMode()->physical().focusPanMax() > 0)
            panMaxFx = float(fxi->fixtureMode()->physical().focusPanMax());

        FacingPreviewWidget *preview = new FacingPreviewWidget(posBox);
        preview->setHasPan(hasPanFx);
        preview->setPanMax(panMaxFx);
        preview->setFace(rp.studioMount);   // fixed fixtures face by their mount face
        preview->setFacing(panZeroSpin->value());
        preview->setIconRotation(rotSpin->value());
        connect(fields, &FixtureFields::edited, preview, [this, preview, fxId_dlg]() {
            preview->setFace(m_props->fixtureRigProps(fxId_dlg).studioMount);
            preview->update();
        });
        posRow->addWidget(preview, 0, Qt::AlignTop);

        // The pan-zero control lives in the Rig Assignment block below; keep the
        // preview in lock-step with it and with the icon Rotation field.
        connect(panZeroSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                preview, &FacingPreviewWidget::setFacing);
        connect(rotSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                [preview](int v) { preview->setIconRotation(v); });
    }

    // Insert the Position & Orientation box directly above Rig Assignment.
    const int rigIdx = vl->indexOf(rigBox);
    vl->insertWidget(rigIdx >= 0 ? rigIdx : vl->count(), posBox);

    // A Fixture-row edit: running scenes recompute, the stage position and a
    // running test follow.
    connect(fields, &FixtureFields::edited, &dlg, [&]() {
        flushSceneCaches();
        refreshTest();
        refreshStagePos();
    });

    // Live, like every rig dialog: position, icon rotation, height offset.
    auto applyPosition = [&]() {
        const QPointF newPos(xSpin->value() * fromDisp_fx * 1000.0, ySpin->value() * fromDisp_fx * 1000.0);
        fxItem->setPos(m_graphicsView->realPositionToPixels(newPos.x(), newPos.y()));
        fxItem->setRealPosition(newPos);
        m_props->setFixturePosition(fxId_dlg, 0, 0, QVector3D(newPos.x(), newPos.y(), 0));
        refreshStagePos();
    };
    for (QDoubleSpinBox *sp : { xSpin, ySpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double) { applyPosition(); });
    connect(rotSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, [&](int v) {
        fxItem->setRotation(v);
        m_props->setFixtureRotation(fxId_dlg, 0, 0, QVector3D(0, v, 0));
    });
    connect(heightOffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double v) {
        FixtureRigProps live = m_props->fixtureRigProps(fxId_dlg);
        live.mountZOffset = float(v * fromDisp_fx);
        m_props->setFixtureRigProps(fxId_dlg, live);
        m_graphicsView->updateFixture(fxId_dlg);
        flushSceneCaches();
        refreshStagePos();
    });
    /* The name when it is left, not at every key: another fixture's name is
       refused, said once. */
    auto applyName = [&]() {
        if (fxi == nullptr || nameEdit == nullptr)
            return;
        const QString newName = nameEdit->text().trimmed();
        if (!newName.isEmpty() && newName != fxi->name()
            && !AppUtil::refuseTakenFixtureName(this, m_doc, fxi->id(), newName))
        {
            fxi->setName(newName);
            m_doc->setModified();
        }
    };
    connect(nameEdit, &QLineEdit::editingFinished, &dlg, [&]() { applyName(); });

    // DMX Controls: Reset button (sends the fixture's Reset capability value)
    if (fxi && fxi->fixtureMode())
    {
        // Find the first Maintenance channel that has a Reset capability
        // Scan for Reset and Identify capabilities in Maintenance channels
        int resetCh = -1;    uchar resetVal = 0;
        int identifyCh = -1; uchar identifyVal = 0;
        for (int ci = 0; ci < fxi->fixtureMode()->channels().count(); ci++)
        {
            const QLCChannel *ch = fxi->fixtureMode()->channel(ci);
            if (!ch || ch->group() != QLCChannel::Maintenance) continue;
            for (const QLCCapability *cap : ch->capabilities())
            {
                const QString capName = cap->name().toLower();
                if (resetCh < 0 && capName.contains("reset"))
                {
                    resetCh  = ci;
                    resetVal = uchar((cap->min() + cap->max()) / 2);
                }
                if (identifyCh < 0 && capName.contains("identif"))
                {
                    identifyCh  = ci;
                    identifyVal = uchar((cap->min() + cap->max()) / 2);
                }
            }
        }

        if (resetCh >= 0 || identifyCh >= 0)
        {
            QGroupBox *dmxBox = new QGroupBox(tr("DMX Controls"), &dlg);
            QHBoxLayout *dmxHl = new QHBoxLayout(dmxBox);

            if (identifyCh >= 0)
            {
                QPushButton *identBtn = new QPushButton(tr("Identify"), dmxBox);
                identBtn->setToolTip(tr("Makes the fixture flash/beep to confirm its DMX address.\n"
                                        "Sends the Identify capability value (Maintenance channel)."));
                dmxHl->addWidget(identBtn);
                const int iCh = identifyCh; const uchar iVal = identifyVal;
                connect(identBtn, &QPushButton::clicked, [&, iCh, iVal]() {
                    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
                    quint32 fxUni = fxi->universe();
                    if (fxUni < quint32(ua.size()))
                    {
                        Universe *uni = ua[int(fxUni)];
                        uni->write(int(fxi->address()) + iCh, iVal);
                        const QByteArray postGM = uni->postGMValues()->mid(0, uni->usedChannels());
                        uni->dumpOutput(postGM, true);
                    }
                    m_doc->inputOutputMap()->releaseUniverses(false);
                    identBtn->setEnabled(false);
                    identBtn->setText(tr("Identifying…"));
                    QTimer::singleShot(3000, identBtn, [identBtn]() {
                        identBtn->setEnabled(true);
                        identBtn->setText(tr("Identify"));
                    });
                });
            }

            if (resetCh >= 0)
            {
                QPushButton *resetBtn = new QPushButton(tr("Reset"), dmxBox);
                resetBtn->setToolTip(tr("Sends the fixture's reset DMX value (Maintenance channel).\n"
                                        "Hold for ~3 seconds while the fixture resets."));
                dmxHl->addWidget(resetBtn);
                const int rCh = resetCh; const uchar rVal = resetVal;
                connect(resetBtn, &QPushButton::clicked, [&, rCh, rVal]() {
                    QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
                    quint32 fxUni = fxi->universe();
                    if (fxUni < quint32(ua.size()))
                    {
                        Universe *uni = ua[int(fxUni)];
                        uni->write(int(fxi->address()) + rCh, rVal);
                        const QByteArray postGM = uni->postGMValues()->mid(0, uni->usedChannels());
                        uni->dumpOutput(postGM, true);
                    }
                    m_doc->inputOutputMap()->releaseUniverses(false);
                    resetBtn->setEnabled(false);
                    resetBtn->setText(tr("Resetting…"));
                    QTimer::singleShot(3000, resetBtn, [resetBtn]() {
                        resetBtn->setEnabled(true);
                        resetBtn->setText(tr("Reset"));
                    });
                });
            }

            dmxHl->addStretch();
            vl->addWidget(dmxBox);
        }
    }

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    /* Its definition and mode -- the one thing this does not change -- in
       the stock dialog, from here: Fixture Properties is THE fixture dialog
       (CONSOLIDATION_REVIEW #3). The name it may change is shown again. */
    if (fxi != nullptr)
    {
        QPushButton *defBtn = btns->addButton(tr("Definition && Mode…"), QDialogButtonBox::ActionRole);
        defBtn->setObjectName(QStringLiteral("definitionAndModeButton"));
        defBtn->setToolTip(tr("Change this fixture's definition or mode (and its name, "
                              "connection and address) in the fixture dialog"));
        const quint32 defFid = fxi->id();
        connect(defBtn, &QPushButton::clicked, &dlg, [this, &dlg, defFid, nameEdit]() {
            if (FixtureManager::editDefinitionAndMode(&dlg, m_doc, defFid))
                if (Fixture *f = m_doc->fixture(defFid))
                    if (nameEdit != nullptr)
                        nameEdit->setText(f->name());
        });
    }
    vl->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
    {
        testSrc.reset(); // ensure DMX source is stopped on cancel
        // Put back what live edits wrote (the plot items are rebuilt: fxItem
        // is not to be touched after this).
        edit.cancel();
        flushSceneCaches();
        return;
    }

    // Everything is applied already; the name may still be in its field.
    testSrc.reset();
    applyName();
    flushSceneCaches();
    m_doc->setModified();
    edit.accept();
}
