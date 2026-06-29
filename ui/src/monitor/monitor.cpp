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

#include <QApplication>
#include <QActionGroup>
#include <QColorDialog>
#include <QCursor>
#include <QDialog>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QtMath>
#include <QSpacerItem>
#include <QByteArray>
#include <QCheckBox>
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
#include "powerdistribution.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "scenevalue.h"
#include "scene.h"
#include "mastertimer.h"
#include "dmxsource.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "fixture.h"
#include "monitorbackgroundselection.h"
#include "monitorgraphicsview.h"
#include "trussitem.h"
#include "platformitem.h"
#include "feetinchesspinbox.h"
#include "targetitem.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "fixtureselection.h"
#include "monitorfixture.h"
#include "monitorlayout.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "qlccapability.h"
#include "monitor.h"
#include "apputil.h"
#include "doc.h"

#include <QTimer>
#include "qlcfile.h"

#define SETTINGS_GEOMETRY "monitor/geometry"
#define SETTINGS_VSPLITTER "monitor/vsplitter"

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
    , m_currentUniverse(Universe::invalid())
    , m_graphicsToolBar(NULL)
    , m_splitter(NULL)
    , m_graphicsView(NULL)
    , m_gridWSpin(NULL)
    , m_gridHSpin(NULL)
    , m_unitsCombo(NULL)
    , m_labelsAction(NULL)
    , m_pasteAction(NULL)
{
    Q_ASSERT(doc != NULL);

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
    while (m_monitorFixtures.isEmpty() == false)
        delete m_monitorFixtures.takeFirst();

    saveSettings();

    /* Reset the singleton instance */
    Monitor::s_instance = NULL;
}

void Monitor::initView()
{
    qDebug() << Q_FUNC_INFO;

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

    /* Create a bunch of MonitorFixtures for each fixture */
    foreach (Fixture* fxi, m_doc->fixtures())
    {
        Q_ASSERT(fxi != NULL);
        if (m_currentUniverse == Universe::invalid() ||
            m_currentUniverse == fxi->universe())
                createMonitorFixture(fxi);
    }
}

void Monitor::showDMXView()
{
    qDebug() << Q_FUNC_INFO;

    hideFixtureItemEditor();

    m_graphicsView->hide();
    m_graphicsToolBar->hide();

    layout()->setMenuBar(m_DMXToolBar);
    m_DMXToolBar->show();
    m_scrollArea->show();

    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        quint32 uniID = m_doc->inputOutputMap()->getUniverseID(i);
        if (m_currentUniverse == Universe::invalid() || uniID == m_currentUniverse)
            m_doc->inputOutputMap()->setUniverseMonitor(i, true);
        else
            m_doc->inputOutputMap()->setUniverseMonitor(i, false);
    }
}

void Monitor::initGraphicsView()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout()->addWidget(m_splitter);
    QWidget* gcontainer = new QWidget(this);
    m_splitter->addWidget(gcontainer);
    gcontainer->setLayout(new QVBoxLayout);
    gcontainer->layout()->setContentsMargins(0, 0, 0, 0);

    m_graphicsView = new MonitorGraphicsView(m_doc, this);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setAcceptDrops(true);
    m_graphicsView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_graphicsView->setBackgroundBrush(QBrush(QColor(11, 11, 11, 255), Qt::SolidPattern));
    m_splitter->widget(0)->layout()->addWidget(m_graphicsView);

    connect(m_graphicsView, SIGNAL(fixtureMoved(quint32,QPointF)),
            this, SLOT(slotFixtureMoved(quint32,QPointF)));
    connect(m_graphicsView, SIGNAL(viewClicked(QMouseEvent*)),
            this, SLOT(slotViewClicked()));
    connect(m_graphicsView, &MonitorGraphicsView::fixtureDoubleClicked,
            this, &Monitor::slotFixtureDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::trussDoubleClicked,
            this, &Monitor::slotTrussDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::platformDoubleClicked,
            this, &Monitor::slotPlatformDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::targetDoubleClicked,
            this, &Monitor::slotTargetDoubleClicked);
    connect(m_graphicsView, &MonitorGraphicsView::contextMenuRequested,
            this, &Monitor::slotCanvasContextMenu);
    connect(m_graphicsView, &MonitorGraphicsView::addFixtureToTrussRequested,
            this, &Monitor::slotAddFixtureToTruss);

    QSettings settings;
    QVariant var2 = settings.value(SETTINGS_VSPLITTER);
    if (var2.isValid() == true)
        m_splitter->restoreState(var2.toByteArray());

    fillGraphicsView();
}

void Monitor::fillGraphicsView()
{
    m_graphicsView->clearFixtures();

    m_gridWSpin->blockSignals(true);
    m_gridHSpin->blockSignals(true);
    m_unitsCombo->blockSignals(true);
    m_gridSubdivSpin->blockSignals(true);
    m_snapCombo->blockSignals(true);

    if (m_props->gridUnits() == MonitorProperties::Meters)
    {
        m_graphicsView->setGridMetrics(1000.0);
        m_unitsCombo->setCurrentIndex(0);
    }
    else // m_props->gridUnits() == MonitorProperties::Feet
    {
        m_graphicsView->setGridMetrics(304.8);
        m_unitsCombo->setCurrentIndex(1);
    }

    m_gridWSpin->setValue(m_props->gridSize().x());
    m_gridHSpin->setValue(m_props->gridSize().z());
    m_gridSubdivSpin->setValue(m_props->gridSubdivisions());
    int snapIdx = m_snapCombo->findData(m_props->snapDivisions());
    m_snapCombo->setCurrentIndex(snapIdx >= 0 ? snapIdx : 0);
    m_gridWSpin->blockSignals(false);
    m_gridHSpin->blockSignals(false);
    m_unitsCombo->blockSignals(false);
    m_gridSubdivSpin->blockSignals(false);
    m_snapCombo->blockSignals(false);

    m_graphicsView->setGridSubdivisions(m_props->gridSubdivisions());
    m_graphicsView->setSnapDivisions(m_props->snapDivisions());
    m_graphicsView->setGridSize(QSize(m_props->gridSize().x(), m_props->gridSize().z()));
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

    m_graphicsView->showFixturesLabels(m_props->labelsVisible());

    // apply the persisted lock state once all fixtures are present
    m_lockAction->blockSignals(true);
    m_lockAction->setChecked(m_props->layoutLocked());
    updatePlotLockAppearance(m_props->layoutLocked());
    m_lockAction->blockSignals(false);
    m_graphicsView->setLayoutLocked(m_props->layoutLocked());
}

void Monitor::showGraphicsView()
{
    qDebug() << Q_FUNC_INFO;

    m_DMXToolBar->hide();
    m_scrollArea->hide();

    layout()->setMenuBar(m_graphicsToolBar);
    m_graphicsToolBar->show();
    m_graphicsView->show();

    // Graphics view needs to monitor all the universes
    for (quint32 i = 0; i < m_doc->inputOutputMap()->universesCount(); i++)
    {
        m_doc->inputOutputMap()->setUniverseMonitor(i, true);
    }
}

void Monitor::showCurrentView()
{
    if (m_props->displayMode() == MonitorProperties::DMX)
        showDMXView();
    else
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

void Monitor::setActiveScene(quint32 sceneId)
{
    if (m_graphicsView != NULL)
        m_graphicsView->setActiveScene(sceneId);
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
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());

    if (m_splitter != NULL)
    {
        QSettings settings;
        settings.setValue(SETTINGS_VSPLITTER, m_splitter->saveState());
    }

    if (m_monitorWidget != NULL)
        m_props->setFont(m_monitorWidget->font());
}

void Monitor::createAndShow(QWidget* parent, Doc* doc)
{
    QWidget* window = NULL;

    /* Must not create more than one instance */
    if (s_instance == NULL)
    {
        /* Create a separate window for OSX */
        s_instance = new Monitor(parent, doc, Qt::Window);
        window = s_instance;

        /* Set some common properties for the window and show it */
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowIcon(QIcon(":/monitor.png"));
        window->setWindowTitle(tr("Fixture Monitor"));
        window->setContextMenuPolicy(Qt::CustomContextMenu);

        QSettings settings;
        QVariant var = settings.value(SETTINGS_GEOMETRY);
        if (var.isValid() == true)
            window->restoreGeometry(var.toByteArray());
        else
        {
            QScreen *screen = QGuiApplication::screens().first();
            QRect rect = screen->availableGeometry();
            int rWd = rect.width() / 4;
            int rHd = rect.height() / 4;
            window->resize(rWd * 3, rHd * 3);
            window->move(rWd / 2, rHd / 2);
        }
        AppUtil::ensureWidgetIsVisible(window);
    }
    else
    {
        window = s_instance;
    }

    window->show();
    window->raise();
}

/****************************************************************************
 * Menu
 ****************************************************************************/

void Monitor::initDMXToolbar()
{
    QActionGroup* group;
    QAction* action;
    m_DMXToolBar = new QToolBar(this);

    /* Menu bar */
    Q_ASSERT(layout() != NULL);
    layout()->setMenuBar(m_DMXToolBar);

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
}

void Monitor::initGraphicsToolbar()
{
    QAction* action;

    m_graphicsToolBar = new QToolBar(this);

    /* Menu bar */
    Q_ASSERT(layout() != NULL);
    layout()->setMenuBar(m_graphicsToolBar);

    action = m_graphicsToolBar->addAction(tr("DMX View"));
    m_graphicsToolBar->addSeparator();
    action->setData(MonitorProperties::DMX);
    connect(action, SIGNAL(triggered(bool)),
            this, SLOT(slotSwitchMode()));

    QLabel *label = new QLabel(tr("Size"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsToolBar->addWidget(label);

    QVector3D gridSize = m_props->gridSize();

    m_gridWSpin = new QSpinBox();
    m_gridWSpin->setMinimum(1);
    m_gridWSpin->setValue(gridSize.x());
    m_graphicsToolBar->addWidget(m_gridWSpin);
    connect(m_gridWSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotGridWidthChanged(int)));

    QLabel *xlabel = new QLabel("x");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsToolBar->addWidget(xlabel);
    m_gridHSpin = new QSpinBox();
    m_gridHSpin->setMinimum(1);
    m_gridHSpin->setValue(gridSize.z());
    m_graphicsToolBar->addWidget(m_gridHSpin);
    connect(m_gridHSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotGridHeightChanged(int)));

    m_unitsCombo = new QComboBox();
    m_unitsCombo->addItem(tr("Meters"), MonitorProperties::Meters);
    m_unitsCombo->addItem(tr("Feet"), MonitorProperties::Feet);
    if (m_props->gridUnits() == MonitorProperties::Feet)
        m_unitsCombo->setCurrentIndex(1);
    m_graphicsToolBar->addWidget(m_unitsCombo);
    connect(m_unitsCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotGridUnitsChanged(int)));

    m_graphicsToolBar->addSeparator();

    QLabel *subdivLabel = new QLabel(tr("Subdivisions"));
    subdivLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsToolBar->addWidget(subdivLabel);
    m_gridSubdivSpin = new QSpinBox();
    m_gridSubdivSpin->setMinimum(1);
    m_gridSubdivSpin->setMaximum(8);
    m_gridSubdivSpin->setValue(m_props->gridSubdivisions());
    m_gridSubdivSpin->setToolTip(tr("Number of sub-divisions drawn inside each grid cell"));
    m_graphicsToolBar->addWidget(m_gridSubdivSpin);
    connect(m_gridSubdivSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotGridSubdivisionsChanged(int)));

    QLabel *snapLabel = new QLabel(tr("Snap"));
    snapLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsToolBar->addWidget(snapLabel);
    m_snapCombo = new QComboBox();
    m_snapCombo->addItem(tr("Off"), 0);
    m_snapCombo->addItem(tr("Full"), 1);
    m_snapCombo->addItem(tr("1/2"), 2);
    m_snapCombo->addItem(tr("1/4"), 4);
    int snapIdx = m_snapCombo->findData(m_props->snapDivisions());
    if (snapIdx >= 0)
        m_snapCombo->setCurrentIndex(snapIdx);
    m_snapCombo->setToolTip(tr("Snap fixtures to the grid while moving them"));
    m_graphicsToolBar->addWidget(m_snapCombo);
    connect(m_snapCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSnapChanged(int)));

    m_lockAction = m_graphicsToolBar->addAction(QIcon(":/lock.png"), tr("Edit Plot"));
    m_lockAction->setCheckable(true);
    m_lockAction->setChecked(m_props->layoutLocked());
    updatePlotLockAppearance(m_props->layoutLocked());
    connect(m_lockAction, SIGNAL(toggled(bool)), this, SLOT(slotLockToggled(bool)));

    m_graphicsToolBar->addSeparator();

    // 2D-map view overlay: recolours the plot by a chosen dimension. Power tints
    // fixtures by circuit; more views (DMX/Groups/...) slot in here later.
    QLabel *viewLabel = new QLabel(tr("View"));
    viewLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsToolBar->addWidget(viewLabel);
    m_viewCombo = new QComboBox();
    m_viewCombo->addItem(tr("Normal"),     ViewNormal);
    m_viewCombo->addItem(tr("Power"),      ViewPower);
    m_viewCombo->setToolTip(tr("Colour the plot by an overlay: Normal = live "
                               "output; Power = tint each fixture by its circuit."));
    m_graphicsToolBar->addWidget(m_viewCombo);
    connect(m_viewCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotMapViewChanged(int)));

    m_graphicsToolBar->addSeparator();

    // Consolidated Add button with popup menu
    QToolButton *addBtn = new QToolButton(this);
    addBtn->setIcon(QIcon(":/edit_add.png"));
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
        addMenu->addSeparator();
        addMenu->addAction(tr("Add Target Position"),
                           this, SLOT(slotAddTarget()));
        addBtn->setMenu(addMenu);
    }
    m_graphicsToolBar->addWidget(addBtn);

    // Single Remove button removes whatever is selected
    m_graphicsToolBar->addAction(QIcon(":/edit_remove.png"), tr("Remove selected"),
                                 this, SLOT(slotRemoveSelected()));

    // Copy / paste of stage features (trusses, platforms, targets) so identical
    // scenic elements can be replicated quickly.
    QAction *copyAct = m_graphicsToolBar->addAction(
        QIcon(":/editcopy.png"), tr("Copy selected features"),
        this, SLOT(slotCopySelected()));
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(copyAct);

    m_pasteAction = m_graphicsToolBar->addAction(
        QIcon(":/editpaste.png"), tr("Paste features"),
        this, SLOT(slotPasteFeatures()));
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_pasteAction->setEnabled(false);
    addAction(m_pasteAction);

    m_graphicsToolBar->addSeparator();

    m_graphicsToolBar->addAction(QIcon(":/image.png"), tr("Set background image"),
                       this, SLOT(slotSetBackground()));
    m_graphicsToolBar->addAction(QIcon(":/color.png"), tr("Set background color"),
                       this, SLOT(slotSetBackgroundColor()));

    m_labelsAction = m_graphicsToolBar->addAction(QIcon(":/label.png"), tr("Show/hide labels"));
    m_labelsAction->setCheckable(true);
    m_labelsAction->setChecked(m_props->labelsVisible());
    connect(m_labelsAction, SIGNAL(triggered(bool)), this, SLOT(slotShowLabels(bool)));

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

void Monitor::slotGridUnitsChanged(int index)
{
    Q_ASSERT(m_graphicsView != NULL);

    MonitorProperties::GridUnits units = MonitorProperties::Meters;

    QVariant var = m_unitsCombo->itemData(index);
    if (var.isValid())
        units = MonitorProperties::GridUnits(var.toInt());

    // Convert grid dimensions so the physical size stays the same.
    // m_gridSize stores values in the current display unit, so we must
    // re-express them in the new unit before changing the scale factor.
    if (units != m_props->gridUnits())
    {
        const double factor = (units == MonitorProperties::Feet) ? 3.28084 : (1.0 / 3.28084);
        int newW = qMax(1, qRound(m_gridWSpin->value() * factor));
        int newH = qMax(1, qRound(m_gridHSpin->value() * factor));

        m_gridWSpin->blockSignals(true);
        m_gridHSpin->blockSignals(true);
        m_gridWSpin->setValue(newW);
        m_gridHSpin->setValue(newH);
        m_gridWSpin->blockSignals(false);
        m_gridHSpin->blockSignals(false);

        m_graphicsView->setGridSize(QSize(newW, newH));
        m_props->setGridSize(QVector3D(newW, m_props->gridSize().y(), newH));
    }

    if (units == MonitorProperties::Meters)
        m_graphicsView->setGridMetrics(1000.0);
    else if (units == MonitorProperties::Feet)
        m_graphicsView->setGridMetrics(304.8);

    m_props->setGridUnits(units);
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
    if (m_viewCombo != NULL)
        applyMapView(m_viewCombo->currentData().toInt());
}

void Monitor::applyMapView(int view)
{
    if (m_graphicsView == NULL)
        return;

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
    else // ViewNormal: clear overlays
    {
        foreach (Fixture *fx, m_doc->fixtures())
        {
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it != NULL)
            {
                it->setViewTint(QColor());
                it->setToolTip(QString());
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
            m_doc->setModified();
        }
    }
    if (m_labelsAction->isChecked())
        slotShowLabels(true);
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

    QColor c = QColorDialog::getColor(initial, this, tr("Set background color"),
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

    // Check if a truss or platform item is selected in the scene
    foreach (QGraphicsItem *gi, m_graphicsView->scene()->selectedItems())
    {
        TrussItem *ti = dynamic_cast<TrussItem *>(gi);
        if (ti)
        {
            if (QMessageBox::question(this, tr("Remove Truss"),
                    tr("Remove truss '%1'?").arg(ti->truss()->name()),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            {
                quint32 tid = ti->trussId();
                m_props->removeTruss(tid);
                m_graphicsView->updateTrusses();
                m_doc->setModified();
            }
            return;
        }
        PlatformItem *pi = dynamic_cast<PlatformItem *>(gi);
        if (pi)
        {
            if (QMessageBox::question(this, tr("Remove Platform"),
                    tr("Remove platform '%1'?").arg(pi->platform()->name()),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            {
                quint32 pid = pi->platformId();
                m_props->removePlatform(pid);
                m_graphicsView->updatePlatforms();
                m_doc->setModified();
            }
            return;
        }
        TargetItem *tgi = dynamic_cast<TargetItem *>(gi);
        if (tgi)
        {
            if (QMessageBox::question(this, tr("Remove Target"),
                    tr("Remove target '%1'?").arg(tgi->target()->name()),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            {
                quint32 tid = tgi->targetId();
                m_props->removeStageTarget(tid);
                m_graphicsView->updateTargets();
                m_doc->setModified();
            }
            return;
        }
    }

    // Fall back to fixture remove
    hideFixtureItemEditor();
    if (m_graphicsView->removeFixture())
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
        t->setLocked(c.locked);
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
        p->setColor(c.color);
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
        newTargetIds.append(t->id());

        // Mirror slotAddTarget: give the copy its own linked PanTilt palette so
        // it is immediately usable rather than a dangling position.
        QLCPalette *pal = new QLCPalette(QLCPalette::PanTilt);
        pal->setName(t->name());
        pal->setValue(0, 0);
        pal->setStageTargetId(t->id());
        pal->setPath(QString("Palettes/%1/").arg(QLCPalette::typeToString(QLCPalette::PanTilt)));
        m_doc->addPalette(pal);
        newPaletteIds.append(pal->id());
    }

    m_graphicsView->updateTrusses();
    m_graphicsView->updatePlatforms();
    m_graphicsView->updateTargets();
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

void Monitor::slotFixtureDoubleClicked(quint32 /*fid*/)
{
    showFixtureItemEditor();
}

void Monitor::slotTrussDoubleClicked(quint32 tid)
{
    slotEditTruss(tid);
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
    StageTarget *t = m_props->addStageTarget();
    t->setName(tr("Target %1").arg(t->id() + 1));

    QPointF mm = m_graphicsView->pixelsToRealPosition(
        m_pendingAddScenePos.x(), m_pendingAddScenePos.y());
    t->setX(float(mm.x() / 1000.0));
    t->setY(float(mm.y() / 1000.0));
    t->setZ(0.0f);

    m_graphicsView->updateTargets();
    m_doc->setModified();

    // Open the edit dialog; if confirmed, auto-create a linked PanTilt palette.
    quint32 tid = t->id();
    slotEditTarget(tid);

    // slotEditTarget returns after the dialog. Re-fetch in case cancelled/renamed.
    StageTarget *updated = m_props->stageTarget(tid);
    if (!updated)
        return;   // user may have removed it (shouldn't happen, but guard anyway)

    // Auto-create a PanTilt palette linked to this target, unless one already exists.
    bool alreadyLinked = false;
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p && p->stageTargetId() == tid)
        { alreadyLinked = true; break; }
    }
    if (!alreadyLinked)
    {
        QLCPalette *pal = new QLCPalette(QLCPalette::PanTilt);
        pal->setName(updated->name());
        pal->setValue(0, 0);          // default: pan=0, tilt=0
        pal->setStageTargetId(tid);
        pal->setPath(QString("Palettes/%1/").arg(QLCPalette::typeToString(QLCPalette::PanTilt)));
        m_doc->addPalette(pal);
        m_doc->setModified();
    }
}

void Monitor::slotEditTarget(quint32 tid)
{
    StageTarget *t = m_props->stageTarget(tid);
    if (!t) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Target — %1").arg(t->name()));
    dlg.setMinimumWidth(320);

    QVBoxLayout *vl   = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    const bool isFeet_t = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitSfx_t = isFeet_t ? tr(" ft") : tr(" m");
    const double toDisp_t   = isFeet_t ? 3.28084 : 1.0;
    const double fromDisp_t = isFeet_t ? (1.0 / 3.28084) : 1.0;
    const double posRange_t = isFeet_t ? 164.0 : 50.0;   // ~50m in feet

    QDoubleSpinBox *xSpin = new QDoubleSpinBox(&dlg);
    xSpin->setRange(-posRange_t, posRange_t); xSpin->setSuffix(unitSfx_t); xSpin->setDecimals(2);
    xSpin->setValue(double(t->x()) * toDisp_t);
    form->addRow(tr("X (stage right):"), xSpin);

    QDoubleSpinBox *ySpin = new QDoubleSpinBox(&dlg);
    ySpin->setRange(-posRange_t, posRange_t); ySpin->setSuffix(unitSfx_t); ySpin->setDecimals(2);
    ySpin->setValue(double(t->y()) * toDisp_t);
    form->addRow(tr("Y (upstage):"), ySpin);

    QDoubleSpinBox *zSpin = new QDoubleSpinBox(&dlg);
    zSpin->setRange(0, isFeet_t ? 65.6 : 20.0); zSpin->setSuffix(unitSfx_t); zSpin->setDecimals(2);
    zSpin->setValue(double(t->z()) * toDisp_t);
    zSpin->setToolTip(tr("ABSOLUTE aim height above the stage floor for this "
                         "target (used by Pan/Tilt aim palettes). 0 = floor."));
    form->addRow(tr("Z (height):"), zSpin);

    // Follow-spot subject/user height — when a follow spot drives a fixture it
    // aims at this height above the platform the subject stands on, OVERRIDING
    // the target Z so the beam tracks the body across risers. Per-workspace.
    QDoubleSpinBox *subjSpin = new QDoubleSpinBox(&dlg);
    subjSpin->setRange(0, isFeet_t ? 32.8 : 10.0); subjSpin->setSuffix(unitSfx_t); subjSpin->setDecimals(2);
    subjSpin->setValue(double(m_props->aimSubjectHeight()) * toDisp_t);
    subjSpin->setToolTip(tr("Follow-spot subject height above the platform/deck "
                            "(overrides the target Z when a follow spot drives the "
                            "fixture). ~1.4 m hits the chest."));
    form->addRow(tr("Follow-spot subject height:"), subjSpin);

    QColor curColor = t->color().isValid() ? t->color() : QColor(255, 180, 0);
    QPushButton *colorBtn = new QPushButton(&dlg);
    colorBtn->setStyleSheet(QString("background-color: %1").arg(curColor.name()));
    QColor chosenColor = curColor;
    connect(colorBtn, &QPushButton::clicked, [&](){
        QColor c = QColorDialog::getColor(chosenColor, &dlg, tr("Target color"));
        if (c.isValid()) {
            chosenColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        }
    });
    form->addRow(tr("Color:"), colorBtn);

    vl->addLayout(form);
    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vl->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString newName = nameEdit->text().trimmed();
    if (!newName.isEmpty()) t->setName(newName);
    t->setX(float(xSpin->value() * fromDisp_t));
    t->setY(float(ySpin->value() * fromDisp_t));
    t->setZ(float(zSpin->value() * fromDisp_t));
    t->setColor(chosenColor);
    m_props->setAimSubjectHeight(float(subjSpin->value() * fromDisp_t));

    m_graphicsView->updateTargets();
    m_doc->setModified();
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

    m_graphicsView->updatePlatforms();
    m_doc->setModified();

    slotEditPlatform(p->id());
}

void Monitor::slotEditPlatform(quint32 pid)
{
    StagePlatform *p = m_props->platform(pid);
    if (!p) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Platform — %1").arg(p->name()));
    dlg.setMinimumWidth(360);

    QVBoxLayout *vl   = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(p->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    const bool isFeet_p = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitSfx_p = isFeet_p ? tr(" ft") : tr(" m");
    const double toDisp_p   = isFeet_p ? 3.28084 : 1.0;
    const double fromDisp_p = isFeet_p ? (1.0 / 3.28084) : 1.0;
    const double posRange_p = isFeet_p ? 164.0 : 50.0;
    const double sizeMax_p  = isFeet_p ? 328.0 : 100.0;
    const double hMax_p     = isFeet_p ? 32.8  : 10.0;

    QDoubleSpinBox *oxSpin = new QDoubleSpinBox(&dlg);
    oxSpin->setRange(-posRange_p, posRange_p); oxSpin->setSuffix(unitSfx_p); oxSpin->setDecimals(2);
    oxSpin->setValue(double(p->originX()) * toDisp_p);
    form->addRow(tr("Origin X:"), oxSpin);

    QDoubleSpinBox *oySpin = new QDoubleSpinBox(&dlg);
    oySpin->setRange(-posRange_p, posRange_p); oySpin->setSuffix(unitSfx_p); oySpin->setDecimals(2);
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

    // Color picker row
    QColor curColor = p->color().isValid() ? p->color() : QColor(80, 120, 200);
    QPushButton *colorBtn = new QPushButton(&dlg);
    colorBtn->setStyleSheet(QString("background-color: %1").arg(curColor.name()));
    QColor chosenColor = curColor;
    connect(colorBtn, &QPushButton::clicked, [&](){
        QColor c = QColorDialog::getColor(chosenColor, &dlg, tr("Platform color"));
        if (c.isValid()) {
            chosenColor = c;
            colorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        }
    });
    form->addRow(tr("Color:"), colorBtn);

    vl->addLayout(form);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vl->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    p->setName(nameEdit->text().trimmed().isEmpty() ? p->name() : nameEdit->text().trimmed());
    p->setOriginX(float(oxSpin->value() * fromDisp_p));
    p->setOriginY(float(oySpin->value() * fromDisp_p));
    p->setWidth(float(wSpin->value() * fromDisp_p));
    p->setDepth(float(dSpin->value() * fromDisp_p));
    p->setHeight(float(hSpin->value() * fromDisp_p));
    p->setColor(chosenColor);

    m_graphicsView->updatePlatforms();
    m_doc->setModified();
}

void Monitor::slotCanvasContextMenu(QPointF scenePos)
{
    m_pendingAddScenePos = scenePos;
    QMenu menu(this);
    menu.addAction(QIcon(":/fixture.png"), tr("Add Fixture here"),
                   this, SLOT(slotAddFixture()));
    menu.addAction(QIcon(":/group.png"), tr("Add Truss here"),
                   this, SLOT(slotAddTruss()));
    menu.addAction(tr("Add Platform/Riser here"),
                   this, SLOT(slotAddPlatform()));
    menu.addSeparator();
    menu.addAction(tr("Add Target Position here"),
                   this, SLOT(slotAddTarget()));

    menu.addSeparator();
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
    if (m_labelsAction->isChecked())
        slotShowLabels(true);
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
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        for (auto it = m_faders.begin(); it != m_faders.end(); ++it)
        {
            int idx = it.key();
            if (!it.value().isNull() && idx < ua.size())
                ua[idx]->dismissFader(it.value());
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
    }

    void setDegrees(float panDeg, float tiltDeg)
    {
        QSharedPointer<GenericFader> fader = m_faders.value(m_fixtureUniIdx);
        if (fader.isNull()) return;

        QList<SceneValue> svs;
        svs << m_fxi->positionToValues(QLCChannel::Pan,  panDeg);
        svs << m_fxi->positionToValues(QLCChannel::Tilt, tiltDeg);
        appendFixtureOnValues(m_fxi, svs);

        fader->removeAll();
        for (const SceneValue &sv : svs)
        {
            FadeChannel fc(m_doc, m_fxi->id(), sv.channel);
            fc.setTarget(sv.value);
            fc.setCurrent(sv.value);
            fc.setFadeTime(0);
            fader->add(fc);
        }
    }

private:
    Fixture *m_fxi;
    Doc     *m_doc;
    int      m_fixtureUniIdx = -1;
    QHash<int, QSharedPointer<GenericFader>> m_faders;
    QHash<int, Universe*>                    m_universes;
};

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
        appendFixtureOnValues(m_fxi, m_onValues);
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
        m_fader->removeAll();
        if (!on) return;
        for (const SceneValue &sv : m_onValues)
        {
            FadeChannel fc(m_doc, sv.fxi, sv.channel);
            fc.setTarget(sv.value);
            fc.setCurrent(sv.value);
            fc.setFadeTime(0);
            m_fader->add(fc);
        }
    }

private:
    Fixture      *m_fxi;
    Doc          *m_doc;
    Universe     *m_universe = nullptr;
    QSharedPointer<GenericFader> m_fader;
    QList<SceneValue>            m_onValues;
};

// ---------------------------------------------------------------------------

class TrussStripWidget : public QWidget
{
public:
    struct Slot
    {
        quint32 fid;
        QString name;
        QColor  gelColor;
        float   offset;   // metres along truss (mutable by drag)
    };

    // isVertical: true for tower/boom trusses — draws a vertical elevation bar
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
            setMinimumWidth(120);
            setMinimumHeight(260);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
        int barX = width() / 2 - kBarThick / 2;
        return QRect(barX, margin, kBarThick, height() - 2 * margin);
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
        int c = slotCentre(i);
        if (m_vertical)
            // Handle is horizontal slab centred on c (vertical axis)
            return QRect(width() / 2 - kHdW / 2, c - kHdH / 2, kHdW, kHdH);
        else
            return QRect(c - kHdW / 2,
                         hBarRect().top() + kBarThick / 2 - kHdH / 2,
                         kHdW, kHdH);
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
                QColor fill = m_slots[i].gelColor.isValid()
                              ? m_slots[i].gelColor : QColor(60, 120, 200);
                if (dragging) fill = fill.lighter(135);
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
                m_anchorPx  = m_vertical ? ev->y() : ev->x();
                m_anchorOff = m_slots[i].offset;
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
        if (m_vertical)
        {
            const QRect bar = vBarRect();
            if (bar.height() <= 0) return;
            // Moving up (smaller y) increases offset for a tower
            float delta = -float(ev->y() - m_anchorPx) / float(bar.height()) * m_len;
            m_slots[m_dragIdx].offset = qBound(0.0f, m_anchorOff + delta, m_len);
        }
        else
        {
            const QRect bar = hBarRect();
            if (bar.width() <= 0) return;
            float delta = float(ev->x() - m_anchorPx) / float(bar.width()) * m_len;
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
    float       m_anchorOff = 0.0f;
};

// ---------------------------------------------------------------------------

void Monitor::slotEditTruss(quint32 tid)
{
    Truss *t = m_props->truss(tid);
    if (!t) return;

    QDialog editDlg(this);
    editDlg.setWindowTitle(tr("Edit Truss — %1").arg(t->name()));
    editDlg.setMinimumWidth(420);

    QVBoxLayout *vl   = new QVBoxLayout(&editDlg);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &editDlg);
    form->addRow(tr("Name:"), nameEdit);

    QComboBox *typeCb = new QComboBox(&editDlg);
    typeCb->addItems({ tr("Horizontal"), tr("Vertical"), tr("Ground") });
    typeCb->setCurrentIndex(static_cast<int>(t->type()));
    form->addRow(tr("Type:"), typeCb);

    const bool isFeet_e = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitSfx_e = isFeet_e ? tr(" ft") : tr(" m");
    const double toDisp_e   = isFeet_e ? 3.28084 : 1.0;
    const double fromDisp_e = isFeet_e ? (1.0 / 3.28084) : 1.0;
    const double posRange_e = isFeet_e ? 164.0 : 50.0;
    const double lenMax_e   = isFeet_e ? 328.0 : 100.0;
    const double zMax_e     = isFeet_e ? 98.4  : 30.0;
    const double wMax_e     = isFeet_e ? 16.4  : 5.0;

    QDoubleSpinBox *originX = new QDoubleSpinBox(&editDlg);
    originX->setRange(-posRange_e, posRange_e); originX->setSuffix(unitSfx_e); originX->setDecimals(2);
    originX->setValue(t->origin().x() * toDisp_e);
    form->addRow(tr("Origin X:"), originX);

    QDoubleSpinBox *originY = new QDoubleSpinBox(&editDlg);
    originY->setRange(-posRange_e, posRange_e); originY->setSuffix(unitSfx_e); originY->setDecimals(2);
    originY->setValue(t->origin().y() * toDisp_e);
    form->addRow(tr("Origin Y:"), originY);

    QDoubleSpinBox *originZ = new QDoubleSpinBox(&editDlg);
    originZ->setRange(0, zMax_e); originZ->setSuffix(unitSfx_e); originZ->setDecimals(2);
    originZ->setValue(t->origin().z() * toDisp_e);
    form->addRow(tr("Height Z:"), originZ);

    float existingAngle = float(qRadiansToDegrees(
        qAtan2(double(t->direction().y()), double(t->direction().x()))));
    if (existingAngle < 0) existingAngle += 360.0f;
    QDoubleSpinBox *dirAngle = new QDoubleSpinBox(&editDlg);
    dirAngle->setRange(0, 359); dirAngle->setSuffix(QString::fromUtf8("°"));
    dirAngle->setDecimals(1); dirAngle->setValue(existingAngle);
    form->addRow(tr("Direction (°):"), dirAngle);

    QDoubleSpinBox *lenSpin = new QDoubleSpinBox(&editDlg);
    lenSpin->setRange(0.1, lenMax_e); lenSpin->setSuffix(unitSfx_e); lenSpin->setDecimals(2);
    lenSpin->setValue(t->length() * toDisp_e);
    form->addRow(tr("Length:"), lenSpin);

    QDoubleSpinBox *widthSpin = new QDoubleSpinBox(&editDlg);
    widthSpin->setRange(0.05, wMax_e); widthSpin->setSuffix(unitSfx_e); widthSpin->setDecimals(2);
    widthSpin->setValue(t->width() * toDisp_e);
    form->addRow(tr("Width:"), widthSpin);

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

    QLabel *stripLabel = new QLabel(tr("Fixture Placement  (drag to reposition)"), &editDlg);
    QFont sf = stripLabel->font(); sf.setBold(true); stripLabel->setFont(sf);

    if (isVertical)
    {
        // For vertical trusses put the elevation strip to the right of the form.
        QHBoxLayout *contentRow = new QHBoxLayout;
        QVBoxLayout *formCol    = new QVBoxLayout;
        formCol->addLayout(form);
        formCol->addStretch();
        contentRow->addLayout(formCol);

        QFrame *vsep = new QFrame(&editDlg);
        vsep->setFrameShape(QFrame::VLine);
        vsep->setFrameShadow(QFrame::Sunken);
        contentRow->addWidget(vsep);

        QVBoxLayout *stripCol = new QVBoxLayout;
        stripCol->addWidget(stripLabel);
        stripCol->addWidget(strip, 1);
        contentRow->addLayout(stripCol);
        vl->addLayout(contentRow);
    }
    else
    {
        vl->addLayout(form);
        QFrame *sep = new QFrame(&editDlg);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        vl->addWidget(sep);
        vl->addWidget(stripLabel);
        vl->addWidget(strip);
    }

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &editDlg);
    vl->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &editDlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &editDlg, &QDialog::reject);

    if (editDlg.exec() != QDialog::Accepted)
        return;

    t->setName(nameEdit->text());
    t->setType(static_cast<Truss::TrussType>(typeCb->currentIndex()));
    t->setOrigin(QVector3D(originX->value() * fromDisp_e,
                           originY->value() * fromDisp_e,
                           originZ->value() * fromDisp_e));
    float radians = float(qDegreesToRadians(dirAngle->value()));
    t->setDirection(QPointF(qCos(radians), qSin(radians)));
    t->setLength(lenSpin->value() * fromDisp_e);
    t->setWidth(widthSpin->value() * fromDisp_e);

    // Apply fixture placement changes from the strip.
    for (const TrussStripWidget::Slot &s : strip->placements())
    {
        FixtureRigProps rp = m_props->fixtureRigProps(s.fid);
        rp.trussOffset = s.offset;
        m_props->setFixtureRigProps(s.fid, rp);

        // Recompute mm position from the (possibly updated) truss geometry.
        QVector3D pos3d = t->positionAt(s.offset);  // metres
        QPointF mmPos(double(pos3d.x()) * 1000.0, double(pos3d.y()) * 1000.0);
        m_props->setFixturePosition(s.fid, 0, 0, QVector3D(mmPos.x(), mmPos.y(), 0));
        m_graphicsView->moveFixtureTo(s.fid, mmPos);
    }

    m_graphicsView->updateTrusses();
    m_doc->setModified();
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
    const QString unitSfx_a = isFeet_a ? tr(" ft") : tr(" m");
    const double toDisp_a   = isFeet_a ? 3.28084 : 1.0;
    const double fromDisp_a = isFeet_a ? (1.0 / 3.28084) : 1.0;
    const double posRange_a = isFeet_a ? 164.0 : 50.0;
    const double lenMax_a   = isFeet_a ? 328.0 : 100.0;
    const double zMax_a     = isFeet_a ? 98.4  : 30.0;
    const double wMax_a     = isFeet_a ? 16.4  : 5.0;

    QDoubleSpinBox *originX = new QDoubleSpinBox(&dlg);
    originX->setRange(-posRange_a, posRange_a); originX->setSuffix(unitSfx_a); originX->setDecimals(2);
    originX->setValue(pendMm.x() / 1000.0 * toDisp_a);
    form->addRow(tr("Origin X (stage right):"), originX);

    QDoubleSpinBox *originY = new QDoubleSpinBox(&dlg);
    originY->setRange(-posRange_a, posRange_a); originY->setSuffix(unitSfx_a); originY->setDecimals(2);
    originY->setValue(pendMm.y() / 1000.0 * toDisp_a);
    form->addRow(tr("Origin Y (upstage):"), originY);

    QDoubleSpinBox *originZ = new QDoubleSpinBox(&dlg);
    originZ->setRange(0, zMax_a); originZ->setSuffix(unitSfx_a); originZ->setDecimals(2);
    originZ->setValue(6.0 * toDisp_a);
    form->addRow(tr("Height Z:"), originZ);

    QDoubleSpinBox *dirAngle = new QDoubleSpinBox(&dlg);
    dirAngle->setRange(0, 359); dirAngle->setSuffix(QString::fromUtf8("°")); dirAngle->setDecimals(1);
    form->addRow(tr("Direction (° from stage-right):"), dirAngle);

    QDoubleSpinBox *length = new QDoubleSpinBox(&dlg);
    length->setRange(0.1, lenMax_a); length->setSuffix(unitSfx_a); length->setDecimals(2);
    length->setValue(6.0 * toDisp_a);
    form->addRow(tr("Length:"), length);

    QDoubleSpinBox *width = new QDoubleSpinBox(&dlg);
    width->setRange(0.05, wMax_a); width->setSuffix(unitSfx_a); width->setDecimals(2);
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

    m_graphicsView->updateTrusses();
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

    m_props->setLabelsVisible(visible);
    m_graphicsView->showFixturesLabels(visible);
}

void Monitor::slotFixtureMoved(quint32 fid, QPointF pos)
{
    Q_ASSERT(m_graphicsView != NULL);
    m_props->setFixturePosition(fid, 0, 0, QVector3D(pos.x(), pos.y(), 0));
    m_doc->setModified();
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
                connect(btn, &QPushButton::clicked, [=]() {
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

        if (!m_hasPan)
        {
            p.setPen(QColor(150, 150, 150));
            p.drawText(r, Qt::AlignCenter, tr("(no pan)"));
            return;
        }

        p.save();
        p.translate(c);

        const qreal bodyHalf = 12.0;

        // Pan-range wedge — centred on the FACING (Front of device).  Independent
        // of the icon rotation: the visual icon spin must never move the aim.
        const qreal wedgeReach = bodyHalf + 30.0;
        QRectF arcR(-wedgeReach, -wedgeReach, 2 * wedgeReach, 2 * wedgeReach);
        const int startQt = qRound((270.0 + double(m_facing) - double(m_panMax) / 2.0) * 16.0);
        const int spanQt  = qRound(double(m_panMax) * 16.0);
        p.setPen(QPen(QColor(120, 90, 160, 130), 2));
        p.drawArc(arcR, startQt, spanQt);

        // Body — rotated by the VISUAL icon rotation ONLY (cosmetic layout).
        p.save();
        p.rotate(m_rotation);
        p.setPen(QPen(QColor(150, 150, 150), 1));
        p.setBrush(QColor(33, 33, 33));
        p.drawRect(QRectF(-bodyHalf, -bodyHalf, 2 * bodyHalf, 2 * bodyHalf));
        p.restore();

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

        // BLUE arrow = icon (visual) rotation.  Shown only in this preview — never
        // in the 2D stage view.  Drawn shorter/under the green arrow.
        drawArrow(m_rotation, 20.0, 4.5, QColor(90, 150, 230));

        // GREEN arrow = facing / Front of device.  This drives the aim and is the
        // arrow shown in the 2D view.  Independent of the icon rotation above.
        drawArrow(m_facing, 28.0, 5.0, QColor(80, 200, 120));

        p.restore();
    }

private:
    bool  m_hasPan   = true;
    float m_facing   = 0.0f;
    float m_rotation = 0.0f;
    float m_panMax   = 360.0f;
};

void Monitor::showFixtureItemEditor()
{
    QList<MonitorFixtureItem *> items = m_graphicsView->selectedFixtureItems();
    if (items.isEmpty())
        return;

    // ---- Multi-selection: shared controls ----
    if (items.count() > 1)
    {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Fixture Properties"));
        QVBoxLayout *vl = new QVBoxLayout(&dlg);

        vl->addWidget(new QLabel(
            tr("<b>%1 fixtures selected</b>").arg(items.count())));

        QGroupBox *rigBox = new QGroupBox(tr("Rig Assignment"), &dlg);
        QFormLayout *rigForm = new QFormLayout(rigBox);

        QComboBox *mountCb = new QComboBox;
        mountCb->addItem(tr("Top-hung"),      QVariant(int(Truss::TopHung)));
        mountCb->addItem(tr("Floor mounted"), QVariant(int(Truss::FloorMounted)));
        mountCb->addItem(tr("Side arm"),      QVariant(int(Truss::SideArm)));
        rigForm->addRow(tr("Mounting:"), mountCb);

        QDoubleSpinBox *panZeroSpin = new QDoubleSpinBox;
        panZeroSpin->setRange(0, 359);
        panZeroSpin->setSuffix(QString::fromUtf8("°"));
        panZeroSpin->setDecimals(1);
        rigForm->addRow(tr("Pan-zero — Front of device:"), panZeroSpin);
        vl->addWidget(rigBox);

        QGroupBox *transformBox = new QGroupBox(tr("Transform"), &dlg);
        QFormLayout *transformForm = new QFormLayout(transformBox);
        QSpinBox *rotSpin = new QSpinBox;
        rotSpin->setRange(0, 359);
        rotSpin->setSuffix(QString::fromUtf8("°"));
        transformForm->addRow(tr("Icon rotation (visual):"), rotSpin);
        vl->addWidget(transformBox);

        QDialogButtonBox *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(btns);

        if (dlg.exec() != QDialog::Accepted)
            return;

        for (MonitorFixtureItem *fi : items)
        {
            fi->setRotation(rotSpin->value());
            m_props->setFixtureRotation(fi->fixtureID(), 0, 0,
                                        QVector3D(0, rotSpin->value(), 0));
            FixtureRigProps rp = m_props->fixtureRigProps(fi->fixtureID());
            rp.mountingType = static_cast<Truss::MountingType>(mountCb->currentData().toInt());
            rp.panZeroDir   = float(panZeroSpin->value());
            m_props->setFixtureRigProps(fi->fixtureID(), rp);
        }
        m_doc->setModified();
        return;
    }

    // ---- Single fixture ----
    MonitorFixtureItem *fxItem = items.first();
    Fixture *fxi = m_doc->fixture(fxItem->fixtureID());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Fixture Properties — %1").arg(fxItem->name()));
    QVBoxLayout *vl = new QVBoxLayout(&dlg);

    // Header: "Name  [right-aligned type]" on one line
    {
        QHBoxLayout *headerRow = new QHBoxLayout;
        QLabel *nameLabel = new QLabel(QString("<b>%1</b>").arg(fxItem->name()));
        headerRow->addWidget(nameLabel, 1);

        if (fxi && fxi->fixtureDef())
        {
            QString typeName = fxi->fixtureDef()->manufacturer()
                               + QLatin1Char(' ') + fxi->fixtureDef()->model();
            QLabel *typeLabel = new QLabel(typeName);
            typeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            typeLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
            headerRow->addWidget(typeLabel, 1);
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

        // DMX address
        QString addrStr = tr("Universe %1 • Ch %2")
            .arg(fxi ? int(fxi->universe()) + 1 : 0)
            .arg(fxi ? int(fxi->address()) + 1 : 0);
        if (fxi && fxi->channels() > 1)
            addrStr += tr("–%1").arg(int(fxi->address()) + int(fxi->channels()));
        QLabel *addrLabel = new QLabel(addrStr, &dlg);
        addrLabel->setStyleSheet("color: #aaa; font-size: 10px;");
        infoForm->addRow(tr("Address:"), addrLabel);

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

    // Rig Assignment (at top, per user request)
    QGroupBox *rigBox = new QGroupBox(tr("Rig Assignment"), &dlg);
    QFormLayout *rigForm = new QFormLayout(rigBox);

    QComboBox *trussCb = new QComboBox;
    trussCb->addItem(tr("(none)"), QVariant(Truss::invalidId()));
    for (Truss *t : m_props->trusses())
        trussCb->addItem(t->name(), t->id());
    rigForm->addRow(tr("Truss:"), trussCb);

    const bool isFeet_fx = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitSfx_fx = isFeet_fx ? tr(" ft") : tr(" m");
    const double toDisp_fx   = isFeet_fx ? 3.28084 : 1.0;
    const double fromDisp_fx = isFeet_fx ? (1.0 / 3.28084) : 1.0;

    QDoubleSpinBox *offsetSpin = new QDoubleSpinBox;
    offsetSpin->setRange(-100 * toDisp_fx, 100 * toDisp_fx);
    offsetSpin->setSuffix(unitSfx_fx);
    offsetSpin->setDecimals(2);
    rigForm->addRow(tr("Offset along truss:"), offsetSpin);

    QComboBox *mountCb = new QComboBox;
    mountCb->addItem(tr("Top-hung"),      QVariant(int(Truss::TopHung)));
    mountCb->addItem(tr("Floor mounted"), QVariant(int(Truss::FloorMounted)));
    mountCb->addItem(tr("Side arm"),      QVariant(int(Truss::SideArm)));
    rigForm->addRow(tr("Mounting:"), mountCb);

    QDoubleSpinBox *panZeroSpin = new QDoubleSpinBox;
    panZeroSpin->setRange(0, 359);
    panZeroSpin->setSuffix(QString::fromUtf8("°"));
    panZeroSpin->setDecimals(1);
    rigForm->addRow(tr("Pan-zero — Front of device:"), panZeroSpin);

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
    testModeCb->addItem(tr("Straight down"),             QVariant(0));
    testModeCb->addItem(tr("Aim at 0° (stage front)"),  QVariant(1));
    testModeCb->addItem(tr("Aim at target:"),            QVariant(2));
    testModeCb->setToolTip(tr("Straight down: center pan+tilt — confirms physical position.\n"
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
    testRowLayout->addWidget(testModeCb, 1);
    testRowLayout->addWidget(testTargetCb, 1);
    testRowLayout->addWidget(testBtn);
    testRowLayout->addWidget(locateBtn);
    rigForm->addRow(tr("Test orientation:"), testRowWidget);

    connect(testModeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        testTargetCb->setVisible(testModeCb->currentData().toInt() == 2);
    });

    FixtureRigProps rp = m_props->fixtureRigProps(fxItem->fixtureID());
    for (int i = 0; i < trussCb->count(); ++i)
    {
        if (trussCb->itemData(i).toUInt() == rp.trussId)
        { trussCb->setCurrentIndex(i); break; }
    }
    offsetSpin->setValue(double(rp.trussOffset) * toDisp_fx);
    mountCb->setCurrentIndex(static_cast<int>(rp.mountingType));
    panZeroSpin->setValue(double(rp.panZeroDir));
    panOffsetSpin->setValue(double(rp.panOffsetDeg));
    tiltOffsetSpin->setValue(double(rp.tiltOffsetDeg));
    panInvertCb->setChecked(rp.panInvert);
    tiltInvertCb->setChecked(rp.tiltInvert);
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
            // "Aim at target": compute full 3D geometry like PanTilt palettes do.
            quint32 tgtId = testTargetCb->currentData().toUInt();
            StageTarget *tgt = m_props->stageTarget(tgtId);
            FixtureRigProps lrp = m_props->fixtureRigProps(fxId_dlg);
            QVector3D fixPos = m_props->fixtureRigPosition(fxId_dlg);
            if (tgt && lrp.trussId != Truss::invalidId() && fixPos != QVector3D())
            {
                QVector3D tgtPos = tgt->position();
                tgtPos.setZ(tgtPos.z() + m_props->platformHeightAt(tgtPos.x(), tgtPos.y()));
                float dx = tgtPos.x() - fixPos.x();
                float dy = tgtPos.y() - fixPos.y();
                float dz = tgtPos.z() - fixPos.z();
                float horizDist = qSqrt(dx*dx + dy*dy);
                float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(dx), double(-dy))));
                if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;
                float relativePan = azimuthDeg - float(panZeroSpin->value());
                while (relativePan >  180.0f) relativePan -= 360.0f;
                while (relativePan < -180.0f) relativePan += 360.0f;
                float panRaw = panMax / 2.0f + relativePan + panOffset;
                if (lrp.panInvert) panRaw = panMax - panRaw;
                outPan = qBound(0.0f, panRaw, panMax);
                float elevDeg = float(qRadiansToDegrees(qAtan2(double(dz), double(horizDist))));
                float tiltOff = 90.0f + elevDeg;
                float tiltDeg = (lrp.mountingType == Truss::FloorMounted)
                                ? tiltMax / 2.0f - tiltOff + tiltOffset
                                : tiltMax / 2.0f + tiltOff + tiltOffset;
                if (lrp.tiltInvert) tiltDeg = tiltMax - tiltDeg;
                outTilt = qBound(0.0f, tiltDeg, tiltMax);
            } else {
                // Fallback: straight down when no rig data
                outPan  = panMax / 2.0f + panOffset;
                outTilt = tiltMax / 2.0f + tiltOffset;
            }
        } else if (mode == 1) {
            float relPan = -float(panZeroSpin->value());
            while (relPan >  180.0f) relPan -= 360.0f;
            while (relPan < -180.0f) relPan += 360.0f;
            outPan  = qBound(0.0f, panMax / 2.0f + relPan + panOffset, panMax);
            outTilt = horizTilt;
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
            foreach (quint32 gid, sc->fixtureGroups())
            {
                FixtureGroup *grp = m_doc->fixtureGroup(gid);
                if (grp && grp->fixtureList().contains(fxId_dlg))
                { sc->resetRuntime(); goto nextScene_dlg; }
            }
            nextScene_dlg:;
        }
    };

    // Live-update: when rig orientation params change, write to MonitorProperties
    // immediately so running scenes recompute geometry on the next tick.
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

    QDoubleSpinBox *xSpin = new QDoubleSpinBox;
    xSpin->setMaximum(m_graphicsView->gridSize().width());
    xSpin->setSingleStep(0.1); xSpin->setDecimals(3); xSpin->setSuffix(unit);
    xSpin->setValue(fxItem->realPosition().x() * toDisp_fx / 1000.0);
    posForm->addRow(tr("Horizontal:"), xSpin);

    QDoubleSpinBox *ySpin = new QDoubleSpinBox;
    ySpin->setMaximum(m_graphicsView->gridSize().height());
    ySpin->setSingleStep(0.1); ySpin->setDecimals(3); ySpin->setSuffix(unit);
    ySpin->setValue(fxItem->realPosition().y() * toDisp_fx / 1000.0);
    posForm->addRow(tr("Vertical:"), ySpin);

    QSpinBox *rotSpin = new QSpinBox;
    rotSpin->setRange(0, 359);
    rotSpin->setSuffix(QString::fromUtf8("°"));
    rotSpin->setValue(int(fxItem->rotation()));
    posForm->addRow(tr("Icon rotation (visual):"), rotSpin);

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
        preview->setFacing(panZeroSpin->value());
        preview->setIconRotation(rotSpin->value());
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

    // Gel color
    QGroupBox *gelBox = new QGroupBox(tr("Gel Color"), &dlg);
    QHBoxLayout *gelHl = new QHBoxLayout(gelBox);
    QToolButton *gelBtn = new QToolButton;
    gelBtn->setIconSize(QSize(28, 28));
    gelBtn->setToolTip(tr("Pick gel color"));
    QToolButton *gelResetBtn = new QToolButton;
    gelResetBtn->setIcon(QIcon(QStringLiteral(":/fileclose.png")));
    gelResetBtn->setIconSize(QSize(28, 28));
    gelResetBtn->setToolTip(tr("Reset gel color"));
    QColor gelColor = fxItem->getColor();
    if (gelColor.isValid())
    {
        QPixmap pm(28, 28); pm.fill(gelColor);
        gelBtn->setIcon(QIcon(pm));
    }
    gelHl->addWidget(new QLabel(tr("Color:")));
    gelHl->addWidget(gelBtn);
    gelHl->addWidget(gelResetBtn);
    gelHl->addStretch();
    vl->addWidget(gelBox);

    connect(gelBtn, &QToolButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(gelColor, &dlg);
        if (c.isValid())
        {
            gelColor = c;
            QPixmap pm(28, 28); pm.fill(c);
            gelBtn->setIcon(QIcon(pm));
        }
    });
    connect(gelResetBtn, &QToolButton::clicked, [&]() {
        gelColor = QColor();
        gelBtn->setIcon(QIcon());
    });

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
    vl->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
    {
        testSrc.reset(); // ensure DMX source is stopped on cancel
        // Restore original rig props (live edits may have written new values)
        m_props->setFixtureRigProps(fxId_dlg, rp);
        flushSceneCaches();
        return;
    }

    // Commit position
    QPointF newPos(xSpin->value() * fromDisp_fx * 1000.0, ySpin->value() * fromDisp_fx * 1000.0);
    fxItem->setPos(m_graphicsView->realPositionToPixels(newPos.x(), newPos.y()));
    fxItem->setRealPosition(newPos);
    m_props->setFixturePosition(fxItem->fixtureID(), 0, 0,
                                QVector3D(newPos.x(), newPos.y(), 0));

    // Commit rotation
    fxItem->setRotation(rotSpin->value());
    m_props->setFixtureRotation(fxItem->fixtureID(), 0, 0,
                                QVector3D(0, rotSpin->value(), 0));

    // Commit gel color
    fxItem->setGelColor(gelColor);
    m_props->setFixtureGelColor(fxItem->fixtureID(), 0, 0, gelColor);
    fxItem->slotUpdateValues();

    // Stop the test source before committing (ensure clean DMX handoff)
    testSrc.reset();

    // Commit rig props
    FixtureRigProps newRp;
    newRp.trussId       = trussCb->currentData().toUInt();
    newRp.trussOffset   = float(offsetSpin->value() * fromDisp_fx);
    newRp.mountingType  = static_cast<Truss::MountingType>(mountCb->currentData().toInt());
    newRp.panZeroDir    = float(panZeroSpin->value());
    newRp.panOffsetDeg  = float(panOffsetSpin->value());
    newRp.tiltOffsetDeg = float(tiltOffsetSpin->value());
    newRp.panInvert     = panInvertCb->isChecked();
    newRp.tiltInvert    = tiltInvertCb->isChecked();
    m_props->setFixtureRigProps(fxItem->fixtureID(), newRp);

    // Flush fader cache in any running scene that contains this fixture
    // (either as a fixed target or inside a fixture group) so target-geometry
    // recomputes with the new rig props on the next tick.
    const quint32 fxId = fxItem->fixtureID();
    foreach (Function *fn, m_doc->functions())
    {
        Scene *sc = qobject_cast<Scene *>(fn);
        if (!sc || !sc->isRunning()) continue;

        // Fixed channel values
        for (const SceneValue &sv : sc->values())
        {
            if (sv.fxi == fxId) { sc->resetRuntime(); goto nextScene; }
        }
        // Fixed fixture targets (no baked values but fixture-level membership)
        if (sc->fixtures().contains(fxId)) { sc->resetRuntime(); goto nextScene; }
        // Dynamic group targets
        foreach (quint32 gid, sc->fixtureGroups())
        {
            FixtureGroup *grp = m_doc->fixtureGroup(gid);
            if (grp && grp->fixtureList().contains(fxId))
            { sc->resetRuntime(); goto nextScene; }
        }
        nextScene:;
    }

    // Update bound state and snap to truss if binding changed
    bool nowBound = newRp.trussId != Truss::invalidId();
    fxItem->setBoundToTruss(nowBound);
    if (nowBound && (rp.trussId != newRp.trussId || rp.trussOffset != newRp.trussOffset))
    {
        Truss *t = m_props->truss(newRp.trussId);
        if (t)
        {
            QVector3D wp = t->positionAt(newRp.trussOffset);
            QPointF tp(wp.x() * 1000.0, wp.y() * 1000.0);
            fxItem->setPos(m_graphicsView->realPositionToPixels(tp.x(), tp.y()));
            fxItem->setRealPosition(tp);
            m_props->setFixturePosition(fxItem->fixtureID(), 0, 0,
                                        QVector3D(tp.x(), tp.y(), 0));
        }
    }
    else if (!nowBound)
    {
        fxItem->setEscapeMode(false);
    }

    m_doc->setModified();
}
