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
#include <QInputDialog>
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
#include "monitorlayerspanel.h"
#include "monitorruler.h"
#include "riserfaceeditor.h"
#include "trussitem.h"
#include "platformitem.h"
#include "feetinchesspinbox.h"
#include "targetitem.h"
#include "truss.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "structurestudioview.h"
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
#include "app.h"
#include "apputil.h"
#include "doc.h"

#include <QTimer>
#include "qlcfile.h"

#define SETTINGS_GEOMETRY "monitor/geometry"
#define SETTINGS_VSPLITTER "monitor/vsplitter2"
#define SETTINGS_LAYERS_PANEL "monitor/layerspanel3"
#define SETTINGS_RULERS "monitor/rulers"
#define SETTINGS_GRID   "monitor/grid"

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
    , m_layersPanel(NULL)
    , m_gridWSpin(NULL)
    , m_gridHSpin(NULL)
    , m_unitsCombo(NULL)
    , m_povCombo(NULL)
    , m_labelsAction(NULL)
    , m_layersAction(NULL)
    , m_groupAction(NULL)
    , m_ungroupAction(NULL)
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

    // Cmd/Ctrl+Q while this detached window has focus should quit the app with
    // the SAME "save the workspace?" gutcheck as the main window — route to
    // App::close() (→ App::closeEvent → saveModifiedDoc), not just close us.
    QAction *quitAct = new QAction(this);
    quitAct->setShortcut(QKeySequence::Quit);
    quitAct->setShortcutContext(Qt::WindowShortcut);
    addAction(quitAct);
    connect(quitAct, &QAction::triggered, this, []() {
        foreach (QWidget *w, QApplication::topLevelWidgets())
        {
            if (App *app = qobject_cast<App *>(w))
            {
                app->close();   // runs the save-before-quit gutcheck
                return;
            }
        }
    });
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
    connect(m_graphicsView, &MonitorGraphicsView::targetDoubleClicked,
            this, &Monitor::slotTargetDoubleClicked);
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
    fl->addWidget(new QLabel(tr("Size:")));
    m_gridWSpin = new QSpinBox();
    m_gridWSpin->setMinimum(1);
    m_gridWSpin->setValue(gridSize.x());
    fl->addWidget(m_gridWSpin);
    connect(m_gridWSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridWidthChanged(int)));

    fl->addWidget(new QLabel("x"));
    m_gridHSpin = new QSpinBox();
    m_gridHSpin->setMinimum(1);
    m_gridHSpin->setValue(gridSize.z());
    fl->addWidget(m_gridHSpin);
    connect(m_gridHSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridHeightChanged(int)));

    m_unitsCombo = new QComboBox();
    m_unitsCombo->addItem(tr("Meters"), MonitorProperties::Meters);
    m_unitsCombo->addItem(tr("Feet"), MonitorProperties::Feet);
    if (m_props->gridUnits() == MonitorProperties::Feet)
        m_unitsCombo->setCurrentIndex(1);
    fl->addWidget(m_unitsCombo);
    connect(m_unitsCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotGridUnitsChanged(int)));
    addSep();

    // ---- (Grid) [Subdiv] ----  toggle + its modifier
    QToolButton *gridBtn = new QToolButton(footer);
    gridBtn->setText(tr("Grid"));
    gridBtn->setCheckable(true);
    gridBtn->setChecked(true);
    gridBtn->setToolTip(tr("Show or hide the grid lines"));
    fl->addWidget(gridBtn);
    connect(gridBtn, &QToolButton::toggled, this, [this](bool on) {
        m_graphicsView->setGridVisible(on);
        QSettings().setValue(SETTINGS_GRID, on);
    });
    fl->addWidget(new QLabel(tr("Subdiv")));
    m_gridSubdivSpin = new QSpinBox();
    m_gridSubdivSpin->setMinimum(1);
    m_gridSubdivSpin->setMaximum(8);
    m_gridSubdivSpin->setValue(m_props->gridSubdivisions());
    m_gridSubdivSpin->setToolTip(tr("Number of sub-divisions drawn inside each grid cell"));
    fl->addWidget(m_gridSubdivSpin);
    connect(m_gridSubdivSpin, SIGNAL(valueChanged(int)), this, SLOT(slotGridSubdivisionsChanged(int)));
    addSep();

    // Snap: a dedicated on/off toggle, plus the division to snap to.
    m_snapToggle = new QToolButton(footer);
    m_snapToggle->setText(tr("Snap"));
    m_snapToggle->setCheckable(true);
    m_snapToggle->setChecked(m_props->snapDivisions() > 0);
    m_snapToggle->setToolTip(tr("Snap fixtures to the grid while moving them"));
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
    fl->addWidget(m_snapCombo);

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

    // Rulers show/hide toggle.
    QToolButton *rulerBtn = new QToolButton(footer);
    rulerBtn->setText(tr("Rulers"));
    rulerBtn->setCheckable(true);
    rulerBtn->setToolTip(tr("Show or hide the measurement rulers"));
    fl->addWidget(rulerBtn);
    connect(rulerBtn, &QToolButton::toggled, this, [this](bool on) { setRulersVisible(on); });

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
    m_overlayCombo->installEventFilter(this);   // click the box to cycle
    fl->addWidget(m_overlayCombo);
    connect(m_overlayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_mapView = m_overlayCombo->currentData().toInt();
        applyMapView(m_mapView);
    });
    addSep();
    fl->addWidget(new QLabel(tr("View:")));
    m_povCombo->setToolTip(tr("Point of view — click to cycle, arrow for the full list"));
    m_povCombo->installEventFilter(this);       // click the box to cycle
    fl->addWidget(m_povCombo);
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

    updateModeIndicator();
}

bool Monitor::eventFilter(QObject *watched, QEvent *event)
{
    // Overlay / View dropdowns: clicking the BOX cycles to the next item; the
    // drop-down ARROW still opens the full list.
    if ((watched == m_overlayCombo || watched == m_povCombo || watched == m_dmxViewCombo)
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
    const bool build  = (m_graphicsView && m_graphicsView->buildFocus());

    QString text;
    QColor bg, fg(20, 20, 20);
    if (build)                     { text = tr("BUILD");      bg = QColor(220, 150, 40); }
    else if (overlay == ViewStage) { text = tr("STAGE ONLY"); bg = QColor(0, 150, 140); }
    else if (overlay == ViewPower) { text = tr("POWER");      bg = QColor(150, 90, 200); }
    else if (overlay == ViewDMX)   { text = tr("DMX");        bg = QColor(40, 130, 180); }
    else if (overlay == ViewNet)   { text = tr("NETWORK");    bg = QColor(60, 160, 120); }
    else { text = pov; bg = QColor(66, 66, 66); fg = QColor(210, 210, 210); }
    if (build || overlay != ViewNormal)
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

void Monitor::showGraphicsView()
{
    qDebug() << Q_FUNC_INFO;

    m_DMXToolBar->hide();
    m_scrollArea->hide();

    layout()->setMenuBar(m_graphicsToolBar);
    m_graphicsToolBar->show();
    m_graphicsView->show();

    // Entering the 2D view always lands in the editable Top POV; sync the combo
    // (which may still read "DMX" from the switch) without re-triggering it.
    if (m_povCombo != NULL)
    {
        m_graphicsView->setViewPOV(MonitorGraphicsView::PovTop);
        m_povCombo->blockSignals(true);
        m_povCombo->setCurrentIndex(1);   // 2D — Top
        m_povCombo->blockSignals(false);
        if (m_dmxViewCombo != NULL)
        {
            m_dmxViewCombo->blockSignals(true);
            m_dmxViewCombo->setCurrentIndex(1);
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
        window->setWindowTitle(tr("Lighting Studio"));
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

    /* View selector — so you can leave the DMX grid back to the 2D map. Mirrors
     *  the footer View combo; the Universe combo below sits to its right. */
    m_DMXToolBar->addSeparator();
    QLabel *dmxViewLabel = new QLabel(tr("View:"));
    dmxViewLabel->setMargin(5);
    m_DMXToolBar->addWidget(dmxViewLabel);
    m_dmxViewCombo = new QComboBox(this);
    m_dmxViewCombo->addItem(tr("DMX"),        -1);
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Top"),   int(MonitorGraphicsView::PovTop));
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Front"), int(MonitorGraphicsView::PovFront));
    m_dmxViewCombo->addItem(tr("2D \342\200\224 Side"),  int(MonitorGraphicsView::PovSide));
    m_dmxViewCombo->setToolTip(tr("Switch to the 2D map view — click to cycle, "
                                  "arrow for the full list"));
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

    /* Menu bar */
    Q_ASSERT(layout() != NULL);
    layout()->setMenuBar(m_graphicsToolBar);

    // Unified view selector: the DMX channel grid, or the 2D map from a chosen
    // point of view. Front/Side are read-only elevation views.
    // The View (POV) selector is created here (so early references are safe) but
    // PLACED in the footer bar (initGraphicsFooter), alongside the Overlay/Size/
    // Grid/Snap controls, so all the view modifiers live on one row.
    m_povCombo = new QComboBox();
    m_povCombo->addItem(tr("DMX"),        -1);
    m_povCombo->addItem(tr("2D \342\200\224 Top"),   int(MonitorGraphicsView::PovTop));
    m_povCombo->addItem(tr("2D \342\200\224 Front"), int(MonitorGraphicsView::PovFront));
    m_povCombo->addItem(tr("2D \342\200\224 Side"),  int(MonitorGraphicsView::PovSide));
    m_povCombo->setCurrentIndex(1);   // 2D Top
    m_povCombo->setToolTip(tr("Choose the view: DMX channel grid, or the 2D map "
                              "from top / front / side. Front & Side are read-only "
                              "elevation views that show height."));
    connect(m_povCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotPOVChanged(int)));

    // Grid Size / Units / Subdivisions / Snap have moved to the footer
    // measurement bar (initGraphicsFooter) to de-clutter the top toolbar.

    m_lockAction = m_graphicsToolBar->addAction(QIcon(":/lock.png"), tr("Edit Plot"));
    m_lockAction->setCheckable(true);
    m_lockAction->setChecked(m_props->layoutLocked());
    updatePlotLockAppearance(m_props->layoutLocked());
    connect(m_lockAction, SIGNAL(toggled(bool)), this, SLOT(slotLockToggled(bool)));

    m_graphicsToolBar->addSeparator();
    // Group/Ungroup, the Overlay selector, Copy/Paste, labels, background and
    // Build focus now live in the "More" popup (built after Add/Remove below),
    // to keep this toolbar lean. Grid/units/snap/rulers stay in the footer.

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
        addMenu->addAction(QIcon(":/image.png"), tr("Add Image"),
                           this, SLOT(slotAddImage()));
        addMenu->addSeparator();
        addMenu->addAction(tr("Add Target Position"),
                           this, SLOT(slotAddTarget()));
        addBtn->setMenu(addMenu);
    }
    m_graphicsToolBar->addWidget(addBtn);

    // Single Remove button removes whatever is selected
    m_graphicsToolBar->addAction(QIcon(":/edit_remove.png"), tr("Remove selected"),
                                 this, SLOT(slotRemoveSelected()));

    m_graphicsToolBar->addSeparator();

    // "More" popup — the secondary view/edit tools, folded off the toolbar to
    // keep it lean. (Most also have shortcuts and/or a right-click equivalent.)
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

    moreMenu->addSeparator();

    m_labelsAction = new QAction(QIcon(":/label.png"), tr("Show/hide labels"), this);
    m_labelsAction->setCheckable(true);
    m_labelsAction->setChecked(m_props->labelsVisible());
    connect(m_labelsAction, SIGNAL(triggered(bool)), this, SLOT(slotShowLabels(bool)));
    moreMenu->addAction(m_labelsAction);

    // Background image is now a placeable object (Add ▸ Add Image) that lives in
    // a layer; flat background colour stays here.
    moreMenu->addAction(QIcon(":/color.png"), tr("Set background color"),
                        this, SLOT(slotSetBackgroundColor()));

    // Build/Rig focus: ghost the lights (faint + click-through) so you build
    // structure (trusses/platforms/images) without fighting the fixtures.
    m_buildAction = new QAction(QIcon(":/configure.png"), tr("Build focus"), this);
    m_buildAction->setCheckable(true);
    m_buildAction->setToolTip(tr("Build/Rig focus: ghost the fixtures and make "
                                 "structure the click target, for laying out trusses "
                                 "and platforms. Toggle off to return to lighting."));
    connect(m_buildAction, &QAction::toggled, this, [this](bool on) {
        if (m_graphicsView) m_graphicsView->setBuildFocus(on);
        updateModeIndicator();
    });
    moreMenu->addAction(m_buildAction);

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
            if (fx != NULL && !universes.contains(fx->universe()))
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
            if (a == NULL) continue;
            const quint32 aS = a->address(), aE = a->address() + a->channels();
            for (int j = i + 1; j < fixtures.size(); j++)
            {
                Fixture *b = fixtures.at(j);
                if (b == NULL || a->universe() != b->universe()) continue;
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
            if (fx != NULL)
            {
                const QString t = transportOf(fx->universe());
                if (!transports.contains(t)) transports.append(t);
            }
        transports.sort();
        QHash<QString, QColor> xportColor;
        for (int i = 0; i < transports.size(); i++)
            xportColor.insert(transports[i],
                QColor::fromHsv(int(i * 360.0 / qMax(1, transports.size())) % 360, 190, 235));

        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx == NULL) continue;
            MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fx->id());
            if (it == NULL) continue;
            const QString t = transportOf(fx->universe());
            it->setViewTint(xportColor.value(t, QColor(120, 120, 120)));
            it->setToolTip(tr("%1 · Universe %2").arg(t).arg(fx->universe() + 1));
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
                m_graphicsView->refreshFixtureBindings();
                if (m_layersPanel) m_layersPanel->reload();
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
    // If the target isn't already part of the current selection (e.g. the studio
    // inspector's "Full properties…" asking for ONE fixture), select just it so
    // the DETAILED single-fixture editor opens — not whatever the map had selected.
    if (fid != 0)
    {
        MonitorFixtureItem *it = m_graphicsView->fixtureItemForId(fid);
        if (it == nullptr || !it->isSelected())
            m_graphicsView->selectFixtureExclusive(fid);
    }
    showFixtureItemEditor();
}

void Monitor::slotTrussDoubleClicked(quint32 tid)
{
    slotEditTruss(tid);
}

// A QTreeWidget that hands out fixture ids on drag (for the studio source tree →
// canvas drop). Qt5 uses the by-value mimeData() override.
namespace {
class FixtureSourceTree : public QTreeWidget
{
public:
    explicit FixtureSourceTree(QWidget *parent = nullptr) : QTreeWidget(parent) {}
protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override
    {
        QMimeData *m = new QMimeData;
        QByteArray b; QDataStream s(&b, QIODevice::WriteOnly);
        foreach (QTreeWidgetItem *it, items)
        {
            const quint32 fid = it->data(0, Qt::UserRole).toUInt();
            if (fid) s << fid;
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
    lv->addWidget(new QLabel(tr("Fixtures on this object:"), left));
    QTreeWidget *tree = new QTreeWidget(left);
    tree->setHeaderHidden(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // F2 renames a group folder (double-click is taken by open-head-layout).
    tree->setEditTriggers(QAbstractItemView::EditKeyPressed);
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
    planeCombo->addItems({ tr("Top"), tr("Front"), tr("Side") });
    bar->addWidget(planeCombo);
    QPushButton *lockBtn = new QPushButton(tr("🔒 Locked"), center);
    lockBtn->setCheckable(true); lockBtn->setChecked(true);
    lockBtn->setToolTip(tr("Locked: click to select fixtures. Unlock to drag them."));
    bar->addWidget(lockBtn);
    bar->addStretch();
    QPushButton *faceBtn = new QPushButton(tr("Put on Face"), center);
    faceBtn->setToolTip(tr("Pin the selected fixtures (or all) to the current "
                           "view's face (Top / Front / Side)."));
    bar->addWidget(faceBtn);
    QPushButton *distBtn = new QPushButton(tr("Distribute"), center);
    distBtn->setToolTip(tr("Space the selected fixtures (or all on this object) "
                           "evenly along the current face."));
    bar->addWidget(distBtn);
    QPushButton *addBarBtn = new QPushButton(tr("Add Bar"), center);
    addBarBtn->setVisible(kind == 0 || kind == 2 || kind == 4);
    bar->addWidget(addBarBtn);
    cv->addLayout(bar);

    StructureStudioView *view = new StructureStudioView(
        m_doc, StructureStudioView::Kind(kind), id, center);
    view->setMinimumSize(320, 300);
    planeCombo->setCurrentIndex(int(view->plane()));
    cv->addWidget(view, 1);
    QLabel *hint = new QLabel(tr("click a fixture to select · double-click to edit · "
                                 "drag to reposition · wheel zoom · shift-drag pan"), center);
    hint->setStyleSheet("color:#8a8e99;");
    cv->addWidget(hint);

    // ---- RIGHT: inline fixture inspector -----------------------------------
    QWidget *insp = new QWidget(body);
    insp->setObjectName("studioInspector");
    QVBoxLayout *iv = new QVBoxLayout(insp);
    QLabel *inspTitle = new QLabel(tr("(no fixture selected)"), insp);
    { QFont tf = inspTitle->font(); tf.setBold(true); inspTitle->setFont(tf); }
    inspTitle->setWordWrap(true);
    iv->addWidget(inspTitle);
    QFormLayout *inspForm = new QFormLayout;
    QPushButton *gelBtn = new QPushButton(insp);
    QComboBox *faceCombo = new QComboBox(insp);
    faceCombo->addItems({ tr("Top"), tr("Front"), tr("Side") });
    QDoubleSpinBox *angleSpin = new QDoubleSpinBox(insp);
    angleSpin->setRange(-180, 180); angleSpin->setDecimals(0); angleSpin->setSuffix(QStringLiteral("°"));
    QComboBox *mountCombo = new QComboBox(insp);   // logical orientation — any fixture
    mountCombo->addItem(tr("Upright (base down)"),     QVariant(int(Truss::FloorMounted)));
    mountCombo->addItem(tr("Sideways (base on side)"), QVariant(int(Truss::SideArm)));
    mountCombo->addItem(tr("Hung (base on top)"),      QVariant(int(Truss::TopHung)));
    inspForm->addRow(tr("Colour:"), gelBtn);
    inspForm->addRow(tr("Orientation:"), mountCombo);
    inspForm->addRow(tr("Face:"), faceCombo);
    inspForm->addRow(tr("Angle:"), angleSpin);
    iv->addLayout(inspForm);
    QPushButton *fullBtn = new QPushButton(tr("Full properties…"), insp);
    iv->addWidget(fullBtn);
    iv->addStretch();
    insp->setMinimumWidth(190);

    auto curFid = QSharedPointer<quint32>::create(0u);
    auto populate = [this, curFid, inspTitle, gelBtn, faceCombo, angleSpin, mountCombo, fullBtn, inspForm]() {
        const quint32 fid = *curFid;
        Fixture *fx = fid ? m_doc->fixture(fid) : nullptr;
        const bool have = (fx != nullptr);
        const bool frame = have && (m_props->fixtureFrameGroup(fid) != 0);
        const bool linear = have && fx->heads() > 1;   // strip / bar / tape
        inspTitle->setText(have ? fx->name() : tr("(no fixture selected)"));
        gelBtn->setEnabled(have); fullBtn->setEnabled(have);
        // Show the RELEVANT controls per fixture type: Orientation for a body with
        // a base (movers, pars); Face + Angle ("which way it runs") for a linear
        // strip/bar, and for any frame-group member.
        auto row = [inspForm](QWidget *w, bool vis) {
            w->setVisible(vis);
            if (QWidget *l = inspForm->labelForField(w)) l->setVisible(vis);
        };
        row(mountCombo, have && !linear);
        row(faceCombo,  have && (linear || frame));
        row(angleSpin,  have && (linear || frame));
        mountCombo->setEnabled(have); faceCombo->setEnabled(have); angleSpin->setEnabled(have);
        if (!have) { gelBtn->setStyleSheet(QString()); return; }
        const FixtureRigProps rp = m_props->fixtureRigProps(fid);
        const QColor gel = m_props->fixtureGelColor(fid, 0, 0);
        gelBtn->setText(gel.isValid() ? gel.name() : tr("set…"));
        gelBtn->setStyleSheet(gel.isValid()
            ? QString("background:%1; color:%2").arg(gel.name())
                .arg(gel.lightness() > 128 ? "#000" : "#fff") : QString());
        faceCombo->blockSignals(true); faceCombo->setCurrentIndex(qBound(0, rp.studioMount, 2)); faceCombo->blockSignals(false);
        angleSpin->blockSignals(true); angleSpin->setValue(double(rp.studioAngle)); angleSpin->blockSignals(false);
        mountCombo->blockSignals(true);
        mountCombo->setCurrentIndex(qMax(0, mountCombo->findData(int(rp.mountingType))));
        mountCombo->blockSignals(false);
    };
    populate();

    connect(view, &StructureStudioView::fixtureSelected, insp,
            [curFid, populate](quint32 fid){ *curFid = fid; populate(); });
    connect(gelBtn, &QPushButton::clicked, insp, [this, curFid, view, populate]() {
        if (*curFid == 0) return;
        const QColor c = QColorDialog::getColor(m_props->fixtureGelColor(*curFid, 0, 0),
                                                this, tr("Fixture Colour"));
        if (!c.isValid()) return;
        m_props->setFixtureGelColor(*curFid, 0, 0, c);
        m_graphicsView->updateFixture(*curFid);
        if (view) view->reload();
        populate();
    });
    connect(faceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), insp,
            [this, curFid, view, populate](int i) {
        if (*curFid == 0) return;
        if (view) view->setFixtureFace(*curFid, i);
        m_graphicsView->updateFixture(*curFid);
        populate();
    });
    connect(angleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), insp,
            [this, curFid, view](double v) {
        if (*curFid == 0) return;
        if (view) view->setFixtureAngle(*curFid, float(v));
        m_graphicsView->updateFixture(*curFid);
    });
    connect(mountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), insp,
            [this, curFid, view, mountCombo](int) {
        if (*curFid == 0) return;
        FixtureRigProps rp = m_props->fixtureRigProps(*curFid);
        rp.mountingType = static_cast<Truss::MountingType>(mountCombo->currentData().toInt());
        m_props->setFixtureRigProps(*curFid, rp);
        m_graphicsView->updateFixture(*curFid);
        if (view) view->reload();
    });
    connect(fullBtn, &QPushButton::clicked, insp,
            [this, curFid]() { if (*curFid) slotFixtureDoubleClicked(*curFid); });

    body->addWidget(left);
    body->addWidget(center);
    body->addWidget(insp);
    body->setStretchFactor(0, 0);
    body->setStretchFactor(1, 1);
    body->setStretchFactor(2, 0);

    // ---- Fixture tree: group folders → fixtures, with selection sync -------
    auto rebuildTree = [this, tree, view]() {
        tree->blockSignals(true);
        tree->clear();
        QMap<quint32, QTreeWidgetItem *> groupNodes;   // fixtureGroup id → node
        QTreeWidgetItem *ungrouped = nullptr;
        foreach (quint32 fid, view->mountedFixtures())
        {
            // Find the first fixture group this fixture belongs to.
            quint32 gid = 0; QString gname;
            foreach (FixtureGroup *g, m_doc->fixtureGroups())
                if (g && g->fixtureList().contains(fid)) { gid = g->id(); gname = g->name(); break; }
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
        }
        tree->blockSignals(false);
    };
    rebuildTree();

    auto rebuildSource = []() {};   // drag-source panel retired (right-click adds)

    // Tree selection → highlight on the canvas + populate the inspector.
    connect(tree, &QTreeWidget::itemSelectionChanged, tree, [tree, view, curFid, populate]() {
        QList<quint32> ids;
        foreach (QTreeWidgetItem *it, tree->selectedItems())
        {
            const quint32 fid = it->data(0, Qt::UserRole).toUInt();
            if (fid != 0) ids << fid;
        }
        view->setHighlight(ids);
        *curFid = ids.isEmpty() ? 0u : ids.first();
        populate();
    });
    // Canvas click → select the matching tree row.
    connect(view, &StructureStudioView::fixtureSelected, tree, [tree](quint32 fid) {
        tree->blockSignals(true);
        tree->clearSelection();
        QTreeWidgetItemIterator it(tree);
        while (*it) { if ((*it)->data(0, Qt::UserRole).toUInt() == fid) { (*it)->setSelected(true); } ++it; }
        tree->blockSignals(false);
    });
    // Rename a group folder (F2 or right-click Rename) → update the fixture group.
    connect(tree, &QTreeWidget::itemChanged, tree, [this](QTreeWidgetItem *it, int col) {
        if (col != 0) return;
        const quint32 gid = it->data(0, Qt::UserRole + 1).toUInt();
        if (gid == 0) return;
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        const QString nm = it->text(0).trimmed();
        if (g != nullptr && !nm.isEmpty() && g->name() != nm)
        {
            g->setName(nm);
            m_doc->setModified();
        }
    });
    // Double-click a tree fixture → edit it; a GROUP folder → its head-layout grid.
    connect(tree, &QTreeWidget::itemDoubleClicked, tree, [this](QTreeWidgetItem *it, int) {
        const quint32 fid = it->data(0, Qt::UserRole).toUInt();
        if (fid != 0) { slotFixtureDoubleClicked(fid); return; }
        const quint32 gid = it->data(0, Qt::UserRole + 1).toUInt();
        if (gid != 0) openGroupLayout(gid);
    });
    // Gather the fixture ids currently selected in the tree.
    auto selectedFids = [tree]() {
        QList<quint32> ids;
        foreach (QTreeWidgetItem *it, tree->selectedItems())
        {
            const quint32 fid = it->data(0, Qt::UserRole).toUInt();
            if (fid != 0) ids << fid;
        }
        return ids;
    };

    // Undo stack: snapshots of the fixture rig-props map. Ctrl+Z restores the
    // last snapshot. pushUndo() is called before each layout-mutating action.
    auto undoStack = QSharedPointer<QList<QMap<quint32, FixtureRigProps>>>::create();
    auto pushUndo = [this, undoStack]() {
        undoStack->append(m_props->fixtureRigPropsMap());
        while (undoStack->size() > 50) undoStack->removeFirst();
    };
    auto doUndo = [this, undoStack, view, rebuildTree]() {
        if (undoStack->isEmpty()) return;
        const QMap<quint32, FixtureRigProps> snap = undoStack->takeLast();
        for (auto it = snap.constBegin(); it != snap.constEnd(); ++it)
            m_props->setFixtureRigProps(it.key(), it.value());
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx) m_graphicsView->updateFixture(fx->id());
        m_graphicsView->updatePlatforms();
        m_doc->setModified();
        if (view) view->reload();
        rebuildTree();
    };
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, dlg);
    connect(undoSc, &QShortcut::activated, dlg, [doUndo]() { doUndo(); });

    // Right-click the tree → add fixtures / a group, or make a group from selection.
    connect(tree, &QTreeWidget::customContextMenuRequested, tree,
            [this, tree, kind, id, view, rebuildTree, rebuildSource, selectedFids, pushUndo](const QPoint &pos) {
        QMenu m;
        QAction *aFix = m.addAction(tr("Add Fixtures…"));
        QAction *aGrp = m.addAction(tr("Add Existing Fixture Group…"));
        const QList<quint32> selNow = selectedFids();
        QAction *aNew = nullptr;
        if (!selNow.isEmpty())
        {
            m.addSeparator();
            aNew = m.addAction(tr("New Fixture Group from Selection…"));
        }
        // Rename when a group folder is under the cursor.
        QTreeWidgetItem *hit = tree->itemAt(pos);
        QAction *aRen = nullptr, *aLay = nullptr;
        if (hit != nullptr && hit->data(0, Qt::UserRole + 1).toUInt() != 0)
        {
            m.addSeparator();
            aRen = m.addAction(tr("Rename Group"));
            aLay = m.addAction(tr("Head Layout…"));
        }
        QAction *chosen = m.exec(tree->viewport()->mapToGlobal(pos));
        if (chosen == aFix) { pushUndo(); studioAddFixture(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (chosen == aGrp) { pushUndo(); studioAddGroup(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (aNew && chosen == aNew) { studioCreateGroup(selNow, view); rebuildTree(); rebuildSource(); }
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

    connect(planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), body,
            [view](int i){ view->setPlane(StructureStudioView::Plane(i)); });
    view->setLocked(true);
    connect(lockBtn, &QPushButton::toggled, view, [view, lockBtn](bool on) {
        view->setLocked(on);
        lockBtn->setText(on ? tr("🔒 Locked") : tr("🔓 Unlocked"));
    });
    connect(view, &StructureStudioView::fixtureActivated, body,
            [this](quint32 fid){ slotFixtureDoubleClicked(fid); });
    connect(view, &StructureStudioView::fixtureMoved, body,
            [this](quint32 fid){ m_graphicsView->updateFixture(fid); });
    connect(addBarBtn, &QPushButton::clicked, body,
            [this, kind, id, view, rebuildSource]() { studioAddBar(kind, id, view); rebuildSource(); });
    // Keep the trees in sync after canvas-side edits.
    connect(view, &StructureStudioView::fixtureMoved, body, [rebuildTree](quint32){ rebuildTree(); });
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
            [this, kind, id, view, tree, selectedFids, rebuildTree, rebuildSource, pushUndo, refreshMap]
            (const QPoint &globalPos, quint32 fidUnder) {
        QMenu m;
        if (fidUnder != 0)
        {
            QAction *aEdit = m.addAction(tr("Edit fixture…"));
            QAction *aRem  = m.addAction(tr("Remove from object"));
            QAction *ch = m.exec(globalPos);
            if (ch == aEdit) slotFixtureDoubleClicked(fidUnder);
            else if (ch == aRem)
            {
                pushUndo();
                FixtureRigProps rp = m_props->fixtureRigProps(fidUnder);
                if (kind == 0 || kind == 4) rp.pipeId = Pipe::invalidId();
                else if (kind == 1) rp.towerId = Tower::invalidId();
                else if (kind == 2) rp.trussId = Truss::invalidId();
                else if (kind == 3) { rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
                                      rp.deckPlatformId  = FixtureRigProps::invalidPlatformId(); }
                m_props->setFixtureRigProps(fidUnder, rp);
                m_graphicsView->updateFixture(fidUnder);
                view->reload(); rebuildTree(); rebuildSource(); refreshMap();
            }
            return;
        }
        QAction *aFix = m.addAction(tr("Add Fixtures…"));
        QAction *aGrp = m.addAction(tr("Add Existing Fixture Group…"));
        const QList<quint32> selNow = selectedFids();
        QAction *aNew = selNow.isEmpty() ? nullptr
            : (m.addSeparator(), m.addAction(tr("New Fixture Group from Selection…")));
        QAction *ch = m.exec(globalPos);
        if (ch == aFix) { pushUndo(); studioAddFixture(kind, id, view); rebuildTree(); rebuildSource(); }
        else if (ch == aGrp) { pushUndo(); studioAddGroup(kind, id, view); rebuildTree(); rebuildSource(); }
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
        foreach (Pipe *p, m_props->pipes())
            if (p->standId() == id) return p->id();
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
    FixtureRigProps rp = m_props->fixtureRigProps(fid);
    rp.trussId = Truss::invalidId();
    rp.pipeId  = Pipe::invalidId();
    rp.towerId = Tower::invalidId();
    rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
    rp.deckPlatformId  = FixtureRigProps::invalidPlatformId();
    if (kind == 1) { if (Tower *tw = m_props->tower(id)) {
        rp.towerId = id; rp.towerShelf = 0;
        rp.towerU = tw->width() * 0.5f; rp.towerV = tw->depth() * 0.5f; } }
    else if (kind == 2) { if (Truss *t = m_props->truss(id)) {
        rp.trussId = id; rp.trussOffset = t->length() * 0.5f; } }
    else if (kind == 3) { if (StagePlatform *pl = m_props->platform(id)) {
        rp.riserPlatformId = id; rp.riserFace = FixtureRigProps::RiserTop;
        rp.riserU = pl->width() * 0.5f; rp.riserV = pl->depth() * 0.5f; } }
    m_props->setFixtureRigProps(fid, rp);
    m_graphicsView->updateFixture(fid);
}

void Monitor::studioCreateGroup(const QList<quint32> &fids, StructureStudioView *view)
{
    if (fids.isEmpty())
    {
        QMessageBox::information(this, tr("New Fixture Group"),
            tr("Select one or more fixtures in the tree first."));
        return;
    }
    // Name + matrix size (columns × rows), like a fixture group in the Fixtures tab.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("New Fixture Group"));
    QFormLayout *f = new QFormLayout(&dlg);
    // Suggest a matrix from the fixtures' SPATIAL layout + head counts: cluster
    // by the view's vertical axis to get rows, then cols = total heads / rows.
    int sugRows = 1, totalHeads = 0;
    {
        QList<double> vv;
        foreach (quint32 fid, fids)
        {
            Fixture *fx = m_doc->fixture(fid);
            totalHeads += fx ? qMax(1, fx->heads()) : 1;
            const QVector3D w = m_props->fixtureRigPosition(fid);
            vv << ((view && view->plane() == StructureStudioView::Top) ? double(w.y()) : double(w.z()));
        }
        std::sort(vv.begin(), vv.end());
        if (!vv.isEmpty())
        {
            double last = vv.first();
            for (double v : vv) if (v - last > 0.15) { ++sugRows; last = v; }
        }
    }
    const int sugCols = qMax(1, int(qCeil(qMax(1, totalHeads) / double(sugRows))));

    QLineEdit *nameE = new QLineEdit(tr("Group %1").arg(m_doc->fixtureGroups().count() + 1), &dlg);
    QSpinBox *colS = new QSpinBox(&dlg); colS->setRange(1, 512); colS->setValue(sugCols);
    QSpinBox *rowS = new QSpinBox(&dlg); rowS->setRange(1, 512); rowS->setValue(sugRows);
    colS->setToolTip(tr("Matrix width — heads are filled left→right, top→bottom."));
    f->addRow(tr("Name:"), nameE);
    f->addRow(tr("Columns (X):"), colS);
    f->addRow(tr("Rows (Y):"), rowS);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    f->addRow(bb);
    if (dlg.exec() != QDialog::Accepted || nameE->text().trimmed().isEmpty())
        return;

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName(nameE->text().trimmed());
    grp->setSize(QSize(colS->value(), rowS->value()));
    m_doc->addFixtureGroup(grp);
    foreach (quint32 fid, fids)
        grp->assignFixture(fid);         // fills the grid left→right, top→bottom
    m_doc->setModified();
    if (view) view->reload();            // the tree regroups under the new folder
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
    MonitorProperties::MonitorGroup g = m_props->group(gid);
    const QVector3D snapOrigin = g.origin;
    const float snapRot = g.rotation;
    const QString snapName = g.name;

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString sfx = isFeet ? tr(" ft") : tr(" m");
    const double toDisp = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(g.name));

    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;
    QLineEdit *nameEdit = new QLineEdit(g.name, geomW);
    form->addRow(tr("Name:"), nameEdit);
    auto mk = [&](double v) { QDoubleSpinBox *s = new QDoubleSpinBox(geomW);
        s->setRange(-200, 200); s->setDecimals(2); s->setSuffix(sfx); s->setValue(v * toDisp); return s; };
    QDoubleSpinBox *ox = mk(g.origin.x()), *oy = mk(g.origin.y()), *oz = mk(g.origin.z());
    QDoubleSpinBox *rot = new QDoubleSpinBox(geomW);
    rot->setRange(-180, 180); rot->setDecimals(0); rot->setSuffix(QStringLiteral("°")); rot->setValue(double(g.rotation));
    form->addRow(tr("Frame origin X:"), ox);
    form->addRow(tr("Frame origin Y:"), oy);
    form->addRow(tr("Frame origin Z:"), oz);
    form->addRow(tr("Rotation:"), rot);
    vl->addLayout(form);
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
        m_props->setGroupName(gid, snapName);
        m_props->setGroupOrigin(gid, snapOrigin);
        m_props->setGroupRotation(gid, snapRot);
        m_props->recomputeAnchoredFrames();
    }
    m_graphicsView->updatePlatforms();
    refreshMembers();
    m_doc->setModified();
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

    foreach (QListWidgetItem *it, list->selectedItems())
        mountFixtureOnStructure(it->data(Qt::UserRole).toUInt(), kind, id, boomId);
    if (view) view->reload();
    m_doc->setModified();
}

void Monitor::studioAddBar(int kind, quint32 id, StructureStudioView *view)
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
        bar->setLayerId(pipe->layerId());
        m_props->recomputeChildTrusses();
        m_graphicsView->updatePlatforms();
        if (m_layersPanel) m_layersPanel->reload();
        m_doc->setModified();
        if (view) view->reload();
        return;
    }

    if (kind == 2)   // truss → a child bar at mid-span
    {
        const quint32 barId = createBarOnTruss(id, -1.0f);
        if (barId != Truss::invalidId())
        {
            m_graphicsView->updateTrusses();
            if (m_layersPanel) m_layersPanel->reload();
            m_doc->setModified();
            if (view) view->reload();
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
    }
    m_props->recomputeChildTrusses();
    m_graphicsView->updatePlatforms();
    if (m_layersPanel) m_layersPanel->reload();
    m_doc->setModified();
    if (view) view->reload();
}

void Monitor::slotTrussRemoveRequested(quint32 tid)
{
    Truss *t = m_props->truss(tid);
    if (t == NULL)
        return;
    if (QMessageBox::question(this, tr("Remove Truss"),
            tr("Remove truss '%1'?").arg(t->name()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
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
    //  • Overhead truss  → an under-hung bar running along it.
    bar->setParentTrussId(parentId);
    bar->setParentOffset(offset >= 0.0f ? offset : parent->length() / 2.0f);   // Along
    if (parent->type() == Truss::Vertical)
    {
        bar->setBarFace(Truss::FaceDownstage);
        bar->setBarRun(Truss::RunAcross);
    }
    else
    {
        bar->setBarFace(Truss::FaceBottom);
        bar->setBarRun(Truss::RunAlong);
    }
    bar->setBarStandoff(0.0f);
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
    if (QMessageBox::question(this, tr("Remove Platform"),
            tr("Remove platform '%1'?").arg(p->name()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    m_props->removePlatform(pid);
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshRiserFixtures();
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

void Monitor::slotEditImage(quint32 id)
{
    if (!m_props->hasImage(id))
        return;
    MonitorProperties::MonitorImage img = m_props->image(id);

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString us  = isFeet ? tr(" ft") : tr(" m");
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
    connect(srcBtn, &QPushButton::clicked, this, [&]() {
        const QString f = QFileDialog::getOpenFileName(&dlg, tr("Choose Image"),
            QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
        if (!f.isEmpty()) { chosenSource = f; srcLabel->setText(QFileInfo(f).fileName()); }
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
        QDoubleSpinBox *s = new QDoubleSpinBox;
        s->setRange(lo, hi); s->setDecimals(2); s->setSuffix(us);
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
    form->addRow(tr("X (stage right):"), xSpin);
    form->addRow(tr("Y (upstage):"), ySpin);
    form->addRow(tr("Z (bottom, elevation):"), zSpin);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() != QDialog::Accepted)
        return;

    img.name    = nameEdit->text().trimmed().isEmpty() ? img.name : nameEdit->text().trimmed();
    img.source  = chosenSource;
    img.plane   = planeCb->currentData().toInt();
    img.width   = float(wSpin->value() * frD);
    img.height  = float(hSpin->value() * frD);
    img.rotation = float(rotSpin->value());
    img.originX = float(xSpin->value() * frD);
    img.originY = float(ySpin->value() * frD);
    img.originZ = float(zSpin->value() * frD);
    m_props->setImage(img);

    m_graphicsView->updateImages();
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

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString sfx = isFeet ? tr(" ft") : tr(" m");
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 33.0 : 10.0;

    // Snapshot for live-preview revert on Cancel.
    const float snapOX = s->originX(), snapOY = s->originY(), snapH = s->height();
    const float snapBR = s->baseRadius(), snapRot = s->rotation();
    const QString snapName = s->name();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(s->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(s->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *sp = new QDoubleSpinBox(&dlg);
        sp->setRange(lo, hi); sp->setDecimals(2); sp->setSuffix(sfx);
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
        // Revert the live preview.
        s->setName(snapName); s->setOriginX(snapOX); s->setOriginY(snapOY);
        s->setHeight(snapH); s->setBaseRadius(snapBR); s->setRotation(snapRot);
        m_props->recomputeStandMounts();
        m_graphicsView->updatePlatforms();
        m_graphicsView->updateTrusses();
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

void Monitor::slotEditTower(quint32 tid)
{
    Tower *t = m_props->tower(tid);
    if (!t) return;

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString sfx = isFeet ? tr(" ft") : tr(" m");
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 40.0 : 12.0;

    // Snapshot for revert on Cancel (geometry + shelves).
    const QString snapName = t->name();
    const float snapOX = t->originX(), snapOY = t->originY();
    const float snapW = t->width(), snapD = t->depth(), snapH = t->height();
    QList<float> snapShelves;
    for (int i = 0; i < t->shelfCount(); ++i) snapShelves << t->shelfHeight(i);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(t->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(t->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);
    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *sp = new QDoubleSpinBox(&dlg);
        sp->setRange(lo, hi); sp->setDecimals(2); sp->setSuffix(sfx);
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
    vl->addLayout(form);

    // Shelves — a list of heights the user can add / remove.
    vl->addWidget(new QLabel(tr("Shelves (height above floor):"), &dlg));
    QListWidget *shelfList = new QListWidget(&dlg);
    auto reloadShelves = [&]() {
        shelfList->clear();
        for (int i = 0; i < t->shelfCount(); ++i)
            shelfList->addItem(tr("Shelf %1  —  %2%3").arg(i + 1)
                .arg(double(t->shelfHeight(i)) * toDisp, 0, 'f', 2).arg(sfx));
    };
    reloadShelves();
    vl->addWidget(shelfList);

    QHBoxLayout *shelfBtns = new QHBoxLayout;
    QDoubleSpinBox *newShelf = new QDoubleSpinBox(&dlg);
    newShelf->setRange(0.0, hMax); newShelf->setDecimals(2); newShelf->setSuffix(sfx);
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
        if (view) view->reload();
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, wSpin, dSpin, hSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
                [applyLive](double){ applyLive(); });
    connect(nameEdit, &QLineEdit::textChanged, &dlg, [applyLive](const QString &){ applyLive(); });
    // Shelf add/remove already mutate the tower live — also refresh the canvas.
    connect(addShelfBtn, &QPushButton::clicked, &dlg, [view]() { if (view) view->reload(); });
    connect(delShelfBtn, &QPushButton::clicked, &dlg, [view]() { if (view) view->reload(); });

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QVBoxLayout *dlgL = new QVBoxLayout(&dlg);
    dlgL->addWidget(bodyPane, 1);
    dlgL->addWidget(bb);
    dlg.resize(940, 560);
    if (dlg.exec() != QDialog::Accepted)
    {
        // Revert geometry AND shelves (Cancel now truly cancels).
        t->setName(snapName); t->setOriginX(snapOX); t->setOriginY(snapOY);
        t->setWidth(snapW); t->setDepth(snapD); t->setHeight(snapH);
        while (t->shelfCount()) t->removeShelf(0);
        for (float z : snapShelves) t->addShelf(z);
        m_graphicsView->updatePlatforms();
        return;
    }

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

void Monitor::slotEditPipe(quint32 bid)
{
    Pipe *b = m_props->pipe(bid);
    if (!b) return;

    const bool isFeet = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString sfx = isFeet ? tr(" ft") : tr(" m");
    const double toDisp   = isFeet ? 3.28084 : 1.0;
    const double fromDisp = isFeet ? (1.0 / 3.28084) : 1.0;
    const double posR = isFeet ? 164.0 : 50.0;
    const double hMax = isFeet ? 66.0 : 20.0;

    // Snapshot core geometry for live-preview revert on Cancel.
    const float snapOX = b->originX(), snapOY = b->originY();
    const float snapLen = b->length(), snapBZ = b->baseZ(), snapRun = b->runAngle();

    QDialog dlg(this);
    const bool horiz = (b->orientation() == Pipe::Horizontal);
    dlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(b->name()));
    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl = new QVBoxLayout(geomW);
    QFormLayout *form = new QFormLayout;

    QLineEdit *nameEdit = new QLineEdit(b->name(), &dlg);
    form->addRow(tr("Name:"), nameEdit);

    auto mkSpin = [&](double val, double lo, double hi) {
        QDoubleSpinBox *s = new QDoubleSpinBox(&dlg);
        s->setRange(lo, hi); s->setDecimals(2); s->setSuffix(sfx);
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
    if (barOnPipe)
    {
        Pipe *pp = m_props->pipe(b->parentPipeId());
        const double maxUp = pp ? double(pp->length()) : hMax * fromDisp;
        upBoomSpin = mkSpin(b->parentPipeOffset(), 0.0, maxUp * toDisp);
        form->addRow(tr("Height up boom:"), upBoomSpin);
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
        vl->addWidget(new QLabel(tr("Fixtures on this pipe (height up the pipe · facing):"), &dlg));
        QTableWidget *tbl = new QTableWidget(mounted.size(), 3, &dlg);
        tbl->setHorizontalHeaderLabels(QStringList() << tr("Fixture") << tr("Height") << tr("Angle"));
        tbl->verticalHeader()->setVisible(false);
        tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 0; i < mounted.size(); ++i)
        {
            Fixture *fx = m_doc->fixture(mounted[i]);
            FixtureRigProps rp = m_props->fixtureRigProps(mounted[i]);
            QTableWidgetItem *ni = new QTableWidgetItem(fx ? fx->name() : QString::number(mounted[i]));
            ni->setFlags(ni->flags() & ~Qt::ItemIsEditable);
            tbl->setItem(i, 0, ni);
            QDoubleSpinBox *os = new QDoubleSpinBox(tbl);
            os->setRange(0.0, hMax); os->setDecimals(2); os->setSuffix(sfx);
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
        b->setOriginX(snapOX); b->setOriginY(snapOY); b->setHeight(snapLen);
        b->setBaseZ(snapBZ); b->setRunAngle(snapRun);
        m_props->recomputeChildTrusses();
        m_graphicsView->updatePlatforms();
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
}

void Monitor::slotEditPlatform(quint32 pid)
{
    StagePlatform *p = m_props->platform(pid);
    if (!p) return;

    // Snapshot for live-preview revert on Cancel.
    const QString snapName = p->name();
    const float snapOX = p->originX(), snapOY = p->originY();
    const float snapW = p->width(), snapD = p->depth(), snapH = p->height();
    const QColor snapColor = p->color();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(p->name()));

    QWidget *geomW = new QWidget(&dlg);
    QVBoxLayout *vl   = new QVBoxLayout(geomW);
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
    vl->addStretch();
    // (The old "Edit fixtures in Studio…" button is gone — this editor IS the
    //  studio now: the fixture tree + canvas below handle layout in one window.)

    // Canvas-centric object editor body; geometry form is its collapsible left.
    StructureStudioView *view = nullptr;
    QWidget *bodyPane = makeStudioPane(&dlg, 3 /*Platform*/, pid, geomW, &view);
    auto applyLive = [&]() {
        if (!nameEdit->text().trimmed().isEmpty()) p->setName(nameEdit->text().trimmed());
        p->setOriginX(float(oxSpin->value() * fromDisp_p));
        p->setOriginY(float(oySpin->value() * fromDisp_p));
        p->setWidth(float(wSpin->value() * fromDisp_p));
        p->setDepth(float(dSpin->value() * fromDisp_p));
        p->setHeight(float(hSpin->value() * fromDisp_p));
        m_props->recomputeAnchoredFrames();
        if (view) view->reload();
    };
    for (QDoubleSpinBox *sp : { oxSpin, oySpin, (QDoubleSpinBox*)wSpin,
                                (QDoubleSpinBox*)dSpin, (QDoubleSpinBox*)hSpin })
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
        p->setName(snapName); p->setOriginX(snapOX); p->setOriginY(snapOY);
        p->setWidth(snapW); p->setDepth(snapD); p->setHeight(snapH); p->setColor(snapColor);
        m_props->recomputeAnchoredFrames();
        m_graphicsView->updatePlatforms();
        m_graphicsView->refreshRiserFixtures();
        return;
    }

    p->setName(nameEdit->text().trimmed().isEmpty() ? p->name() : nameEdit->text().trimmed());
    p->setOriginX(float(oxSpin->value() * fromDisp_p));
    p->setOriginY(float(oySpin->value() * fromDisp_p));
    p->setWidth(float(wSpin->value() * fromDisp_p));
    p->setDepth(float(dSpin->value() * fromDisp_p));
    p->setHeight(float(hSpin->value() * fromDisp_p));
    p->setColor(chosenColor);

    m_props->recomputeAnchoredFrames();           // slaved studio frames follow the move/resize
    m_graphicsView->updatePlatforms();
    m_graphicsView->refreshRiserFixtures();       // mounted + frame fixtures follow size/height
    if (m_layersPanel) m_layersPanel->reload();   // reflect the new name/colour in the tree
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
    menu.addAction(tr("Add Boom here"),
                   this, SLOT(slotAddPipe()));
    menu.addAction(tr("Add Electric here"),
                   this, SLOT(slotAddElectric()));
    menu.addAction(tr("Add Stand here"),
                   this, SLOT(slotAddStand()));
    menu.addAction(tr("Add Tower here"),
                   this, SLOT(slotAddTower()));
    menu.addAction(QIcon(":/image.png"), tr("Add Image here"),
                   this, SLOT(slotAddImage()));
    menu.addSeparator();
    menu.addAction(tr("Add Target Position here"),
                   this, SLOT(slotAddTarget()));

    menu.addSeparator();
    QAction *groupAct = menu.addAction(QIcon(":/group.png"), tr("Group selected (Ctrl+G)"),
                                       this, SLOT(slotGroupItems()));
    groupAct->setEnabled(m_graphicsView != NULL && m_graphicsView->selectionGroupable());
    QAction *ungroupAct = menu.addAction(QIcon(":/ungroup.png"), tr("Ungroup (Ctrl+Shift+G)"),
                                         this, SLOT(slotUngroupItems()));
    ungroupAct->setEnabled(m_graphicsView != NULL && m_graphicsView->selectionHasGroup());

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
                m_anchorPx  = m_vertical ? ev->y() : ev->x();
                m_anchorPxX = ev->x();
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
                float dOff = -float(ev->y() - m_anchorPx) / float(bar.height()) * m_len;
                m_slots[m_dragIdx].offset = qBound(0.0f, m_anchorOff + dOff, m_len);
            }
            const qreal sc = hScalePx();
            if (sc > 0.0)
            {
                float c = m_anchorCross + float(ev->x() - m_anchorPxX) / float(sc);
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

    // Snapshot for live-preview revert on Cancel (only meaningful for a bar).
    const float  origAlong    = t->parentOffset();
    const int    origFace     = t->barFace();
    const float  origStandoff = t->barStandoff();
    const int    origRun      = t->barRun();
    const float  origLen      = t->length();
    const bool   origIsBar    = t->isChildBar();
    // Geometry snapshot for the live canvas preview of a FREE truss.
    const QVector3D origOrigin = t->origin();
    const QPointF   origDir    = t->direction();
    const Truss::TrussType origType = t->type();
    const float     origWidth  = t->width();

    QDialog editDlg(this);
    editDlg.setWindowTitle(tr("Lighting Studio Editor — %1").arg(t->name()));
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
    dirAngle->setToolTip(tr("On-screen run of a flat truss/bar: 0° = points stage-"
                            "right, 90° = points upstage. This is what makes it look "
                            "horizontal or vertical on the 2D map."));
    form->addRow(tr("Direction (°):"), dirAngle);

    QDoubleSpinBox *lenSpin = new QDoubleSpinBox(&editDlg);
    lenSpin->setRange(0.1, lenMax_e); lenSpin->setSuffix(unitSfx_e); lenSpin->setDecimals(2);
    lenSpin->setValue(t->length() * toDisp_e);
    form->addRow(tr("Length:"), lenSpin);

    QDoubleSpinBox *widthSpin = new QDoubleSpinBox(&editDlg);
    widthSpin->setRange(0.05, wMax_e); widthSpin->setSuffix(unitSfx_e); widthSpin->setDecimals(2);
    widthSpin->setValue(t->width() * toDisp_e);
    form->addRow(tr("Width:"), widthSpin);

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
    QDoubleSpinBox *alongSpin = new QDoubleSpinBox(&editDlg);
    alongSpin->setRange(-lenMax_e, lenMax_e); alongSpin->setSuffix(unitSfx_e); alongSpin->setDecimals(2);
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
    QDoubleSpinBox *standoffSpin = new QDoubleSpinBox(&editDlg);
    standoffSpin->setRange(0, zMax_e); standoffSpin->setSuffix(unitSfx_e); standoffSpin->setDecimals(2);
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
        reloadStudio();
    };
    for (QDoubleSpinBox *sp : { originX, originY, originZ, dirAngle, lenSpin, widthSpin })
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg,
                [applyGeomLive](double){ applyGeomLive(); });
    connect(typeCb, QOverload<int>::of(&QComboBox::currentIndexChanged), &editDlg,
            [applyGeomLive](int){ applyGeomLive(); });
    // The bar-preview hooks already recompute a child bar — mirror onto the canvas.
    connect(alongSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [reloadStudio](double){ reloadStudio(); });
    connect(standoffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &editDlg, [reloadStudio](double){ reloadStudio(); });

    Q_UNUSED(isVertical)
    vl->addWidget(bodyPane, 1);
    editDlg.resize(960, 580);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &editDlg);
    vl->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &editDlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &editDlg, &QDialog::reject);

    if (editDlg.exec() != QDialog::Accepted)
    {
        // Cancel: undo the live preview (restore the bar's original mount) and
        // remove any bars created via the strip's "Add bar here".
        if (origIsBar)
        {
            t->setParentOffset(origAlong);
            t->setBarFace(origFace);
            t->setBarStandoff(origStandoff);
            t->setBarRun(origRun);
            t->setLength(origLen);
            m_props->recomputeChildTrusses();
        }
        else
        {
            // Undo the live free-truss geometry preview.
            t->setType(origType);
            t->setOrigin(origOrigin);
            t->setDirection(origDir);
            t->setLength(origLen);
            t->setWidth(origWidth);
        }
        m_graphicsView->updateTrusses();
        for (quint32 barId : barsCreatedHere)
            m_props->removeTruss(barId);
        m_graphicsView->followParentTrusses();
        if (m_layersPanel) m_layersPanel->reload();
        return;
    }

    t->setName(nameEdit->text());
    t->setLength(lenSpin->value() * fromDisp_e);
    t->setWidth(widthSpin->value() * fromDisp_e);

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
        // Stand this truss on a Stand (its origin then rides the stand top).
        t->setStandId(standCb->currentData().toUInt());
        m_props->recomputeStandMounts();
    }

    // Child-bar positions (from any bars added via the canvas) still round-trip
    // through the strip's slots; fixtures are placed in the canvas now, so we no
    // longer write fixture offsets here (that would clobber a canvas drag).
    for (const TrussStripWidget::Slot &s : strip->placements())
    {
        if (s.barTrussId != Truss::invalidId())
            if (Truss *bar = m_props->truss(s.barTrussId))
            {
                bar->setParentOffset(s.offset);
                bar->setBarCrossShift(s.barCross);
            }
    }
    m_props->recomputeChildTrusses();   // bars re-derive origin

    // followParentTrusses() redraws trusses AND re-derives/repositions any child
    // bars (and their fixtures) — needed when the edited truss is a bar's parent.
    m_graphicsView->followParentTrusses();
    if (m_layersPanel) m_layersPanel->reload();   // reflect the new truss name in the tree
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
        // Logical orientation of the device (base direction), not a rig context.
        mountCb->addItem(tr("Upright (base down)"),     QVariant(int(Truss::FloorMounted)));
        mountCb->addItem(tr("Sideways (base on side)"), QVariant(int(Truss::SideArm)));
        mountCb->addItem(tr("Hung (base on top)"),      QVariant(int(Truss::TopHung)));
        rigForm->addRow(tr("Orientation:"), mountCb);

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
    QPushButton *detachTrussBtn = new QPushButton(tr("Remove from Truss"));
    detachTrussBtn->setToolTip(tr("Detach this fixture from its truss"));
    {
        QWidget *trussRow = new QWidget;
        QHBoxLayout *trussRowL = new QHBoxLayout(trussRow);
        trussRowL->setContentsMargins(0, 0, 0, 0);
        trussRowL->addWidget(trussCb, 1);
        trussRowL->addWidget(detachTrussBtn);
        rigForm->addRow(tr("Truss:"), trussRow);
    }

    const bool isFeet_fx = (m_props->gridUnits() == MonitorProperties::Feet);
    const QString unitSfx_fx = isFeet_fx ? tr(" ft") : tr(" m");
    const double toDisp_fx   = isFeet_fx ? 3.28084 : 1.0;
    const double fromDisp_fx = isFeet_fx ? (1.0 / 3.28084) : 1.0;

    QDoubleSpinBox *offsetSpin = new QDoubleSpinBox;
    offsetSpin->setRange(-100 * toDisp_fx, 100 * toDisp_fx);
    offsetSpin->setSuffix(unitSfx_fx);
    offsetSpin->setDecimals(2);
    rigForm->addRow(tr("Offset along truss:"), offsetSpin);

    // The "Remove from Truss" button (built above) just resets the combo/offset;
    // the detach is committed on OK where the rig props are written.
    connect(detachTrussBtn, &QPushButton::clicked, this, [trussCb, offsetSpin]() {
        trussCb->setCurrentIndex(0);   // (none)
        offsetSpin->setValue(0.0);
    });

    // Which side of the truss the fixture hangs on (affects Z in elevation).
    QComboBox *trussSideCb = new QComboBox;
    trussSideCb->addItem(tr("Under-hung (below)"), QVariant(int(FixtureRigProps::UnderHung)));
    trussSideCb->addItem(tr("Top-mounted (above)"), QVariant(int(FixtureRigProps::TopMounted)));
    trussSideCb->addItem(tr("Centered (on chord)"), QVariant(int(FixtureRigProps::Centered)));
    trussSideCb->setToolTip(tr("Vertical side of the truss the fixture sits on "
                               "(shown in Front/Side elevation views)"));
    rigForm->addRow(tr("Truss side:"), trussSideCb);

    // Horizontal position across the truss: Left / Centered / Right chord. Stays
    // attached to the truss — only slides sideways across its width.
    QComboBox *trussCrossCb = new QComboBox;
    trussCrossCb->addItem(tr("Left"),     -1);
    trussCrossCb->addItem(tr("Centered"),  0);
    trussCrossCb->addItem(tr("Right"),    +1);
    trussCrossCb->setToolTip(tr("Horizontal position across the truss width, "
                                "perpendicular to its run — Left/Right chord or "
                                "centred. The fixture stays attached to the truss."));
    rigForm->addRow(tr("Across truss:"), trussCrossCb);

    QComboBox *mountCb = new QComboBox;
    // Logical orientation of the device (base direction), not a rig context.
    mountCb->addItem(tr("Upright (base down)"),     QVariant(int(Truss::FloorMounted)));
    mountCb->addItem(tr("Sideways (base on side)"), QVariant(int(Truss::SideArm)));
    mountCb->addItem(tr("Hung (base on top)"),      QVariant(int(Truss::TopHung)));
    rigForm->addRow(tr("Orientation:"), mountCb);

    // Deck mount: a fixture standing on top of a platform ("floor mounted").
    QComboBox *deckCb = new QComboBox;
    deckCb->addItem(tr("(none)"), QVariant(FixtureRigProps::invalidPlatformId()));
    for (StagePlatform *p : m_props->platforms())
        deckCb->addItem(p->name(), p->id());
    deckCb->setToolTip(tr("Stand this fixture on top of a platform; its height "
                          "follows the platform's deck."));
    rigForm->addRow(tr("On platform (deck):"), deckCb);

    QDoubleSpinBox *deckHeightSpin = new QDoubleSpinBox;
    deckHeightSpin->setRange(-20 * toDisp_fx, 20 * toDisp_fx);
    deckHeightSpin->setSuffix(unitSfx_fx);
    deckHeightSpin->setDecimals(2);
    deckHeightSpin->setToolTip(tr("Height relative to the deck top (0 = sits on the "
                                  "deck; negative drops it below / under the deck)"));
    rigForm->addRow(tr("Height above deck:"), deckHeightSpin);

    // Truss and deck mounts are mutually exclusive — picking one clears the other.
    connect(trussCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [trussCb, deckCb](int) {
        if (trussCb->currentData().toUInt() != Truss::invalidId())
            deckCb->setCurrentIndex(0);
    });
    connect(deckCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [trussCb, deckCb](int) {
        if (deckCb->currentData().toUInt() != FixtureRigProps::invalidPlatformId())
            trussCb->setCurrentIndex(0);
    });

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
    trussSideCb->setCurrentIndex(trussSideCb->findData(int(rp.trussMountSide)));
    trussCrossCb->setCurrentIndex(trussCrossCb->findData(
        rp.trussCross < 0.0f ? -1 : (rp.trussCross > 0.0f ? +1 : 0)));
    for (int i = 0; i < deckCb->count(); ++i)
        if (deckCb->itemData(i).toUInt() == rp.deckPlatformId)
        { deckCb->setCurrentIndex(i); break; }
    deckHeightSpin->setValue(double(rp.deckHeightOffset) * toDisp_fx);
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
    newRp.trussMountSide = trussSideCb->currentData().toInt();
    // Across-truss position: Left/Right map to ± half the selected truss width
    // (onto a chord); Centered = 0.
    {
        const int crossSel = trussCrossCb->currentData().toInt();
        float halfW = 0.15f;   // fallback ≈ half a 12" truss
        if (Truss *ct = m_props->truss(newRp.trussId))
            halfW = ct->width() * 0.5f;
        newRp.trussCross = float(crossSel) * halfW;
    }
    newRp.deckPlatformId = deckCb->currentData().toUInt();
    newRp.deckHeightOffset = float(deckHeightSpin->value() * fromDisp_fx);
    newRp.panZeroDir    = float(panZeroSpin->value());
    newRp.panOffsetDeg  = float(panOffsetSpin->value());
    newRp.tiltOffsetDeg = float(tiltOffsetSpin->value());
    newRp.panInvert     = panInvertCb->isChecked();
    newRp.tiltInvert    = tiltInvertCb->isChecked();
    // Deck-mounting: default the fixture's Z to the platform top on first
    // assignment so it "sits on" the deck (the editor height is an offset above).
    m_props->setFixtureRigProps(fxItem->fixtureID(), newRp);
    if (newRp.onDeck())
        m_graphicsView->updateFixture(fxItem->fixtureID());

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
            // Centre the icon on the truss line (see MonitorGraphicsView::halfIcon).
            const QPointF tpPx = m_graphicsView->realPositionToPixels(tp.x(), tp.y());
            fxItem->setPos(tpPx.x() - fxItem->cellSize().width() / 2.0,
                           tpPx.y() - fxItem->cellSize().height() / 2.0);
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
