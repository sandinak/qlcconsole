/*
  Q Light Controller
  showmanager.cpp

  Copyright (C) Massimo Callegari

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

#include <QInputDialog>
#include <QColorDialog>
#include <QElapsedTimer>
#include <QToolButton>
#include <QLineEdit>
#include <QMenu>
#include <QColorDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QComboBox>
#include <QSplitter>
#include <QSettings>
#include <QToolBar>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <QDebug>
#include <QUrl>

#include "functionselection.h"
#include "audioplugincache.h"
#include "rgbmatrixeditor.h"
#include "multitrackview.h"
#include "chasereditor.h"
#include "audioeditor.h"
#include "efxeditor.h"
#include "videoeditor.h"
#include "showmanager.h"
#include "sceneeditor.h"
#include "timingstool.h"
#include "qlcmacros.h"
#include "sequence.h"
#include "chaser.h"
#include "collection.h"
#include "functionstreewidget.h"
#include "timecodesource.h"
#include "mastertimer.h"
#include "inputoutputmap.h"

#define SETTINGS_HSPLITTER "showmanager/hsplitter"
#define SETTINGS_VSPLITTER "showmanager/vsplitter"

ShowManager* ShowManager::s_instance = NULL;

ShowManager::ShowManager(QWidget* parent, Doc* doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_show(NULL)
    , m_currentTrack(NULL)
    , m_currentScene(NULL)
    , m_sceneEditor(NULL)
    , m_currentEditor(NULL)
    , m_editorFunctionID(Function::invalidId())
    , m_selectedShowIndex(-1)
    , cursorMovedDuringPause(false)
    , m_splitter(NULL)
    , m_vsplitter(NULL)
    , m_showview(NULL)
    , m_funcTree(NULL)
    , m_toolbar(NULL)
    , m_showsCombo(NULL)
    , m_addShowAction(NULL)
    , m_renameShowAction(NULL)
    , m_deleteShowAction(NULL)
    , m_undoAction(NULL)
    , m_addTrackAction(NULL)
    , m_addSequenceAction(NULL)
    , m_addAudioAction(NULL)
    , m_addVideoAction(NULL)
    , m_copyAction(NULL)
    , m_pasteAction(NULL)
    , m_deleteAction(NULL)
    , m_colorAction(NULL)
    , m_lockAction(NULL)
    , m_timingsAction(NULL)
    , m_snapGridAction(NULL)
    , m_stopAction(NULL)
    , m_playAction(NULL)
    , m_followMtcAction(NULL)
    , m_tcSourceCombo(NULL)
{
    Q_ASSERT(s_instance == NULL);
    s_instance = this;

    Q_ASSERT(doc != NULL);

    new QVBoxLayout(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(0);

    initActions();
    initToolbar();

    m_splitter = new QSplitter(Qt::Vertical, this);
    layout()->addWidget(m_splitter);
    //initMultiTrackView();
    m_showview = new MultiTrackView();
    // add container for multitrack & function editors view
    QWidget* gcontainer = new QWidget(this);
    m_splitter->addWidget(gcontainer);
    gcontainer->setLayout(new QVBoxLayout);
    gcontainer->layout()->setContentsMargins(0, 0, 0, 0);

    m_showview->setRenderHint(QPainter::Antialiasing);
    m_showview->setAcceptDrops(true);
    m_showview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_showview->setBackgroundBrush(QBrush(QColor(88, 88, 88, 255), Qt::SolidPattern));
    connect(m_showview, SIGNAL(viewClicked(QMouseEvent *)),
            this, SLOT(slotViewClicked(QMouseEvent *)));

    connect(m_showview, SIGNAL(showItemMoved(ShowItem*,quint32,bool)),
            this, SLOT(slotShowItemMoved(ShowItem*,quint32,bool)));
    connect(m_showview, SIGNAL(timeChanged(quint32)),
            this, SLOT(slotUpdateTime(quint32)));
    connect(m_showview, SIGNAL(trackClicked(Track*)),
            this, SLOT(slotTrackClicked(Track*)));
    connect(m_showview, SIGNAL(trackDoubleClicked(Track*)),
            this, SLOT(slotTrackDoubleClicked(Track*)));
    connect(m_showview, SIGNAL(trackMoved(Track*,int)),
            this, SLOT(slotTrackMoved(Track*,int)));
    connect(m_showview, SIGNAL(trackDelete(Track*)),
            this, SLOT(slotTrackDelete(Track*)));
    connect(m_showview, &MultiTrackView::trackModified,
            m_doc, &Doc::setModified);
    connect(m_showview, SIGNAL(trackColorChangeRequested(Track*)),
            this, SLOT(slotTrackColorChangeRequested(Track*)));

    // split the multitrack view into two (left: tracks, right: function editors)
    m_vsplitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->widget(0)->layout()->addWidget(m_vsplitter);
    QWidget* mcontainer = new QWidget(this);
    mcontainer->setLayout(new QHBoxLayout);
    mcontainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_vsplitter->addWidget(mcontainer);

    // Drag source: a function tree on the left of the timeline. Drag any
    // function onto a track row / time position to insert it there.
    QSplitter *timelineSplit = new QSplitter(Qt::Horizontal, this);
    m_funcTree = new FunctionsTreeWidget(m_doc, this);
    m_funcTree->setDisplayFilter(FunctionsTreeWidget::FunctionsOnly);
    m_funcTree->setExternalDragMode(true);
    m_funcTree->setHeaderLabel(tr("Functions — drag onto a track"));
    m_funcTree->updateTree();
    m_funcTree->setMaximumWidth(280);
    timelineSplit->addWidget(m_funcTree);
    timelineSplit->addWidget(m_showview);
    timelineSplit->setStretchFactor(0, 0);
    timelineSplit->setStretchFactor(1, 1);
    m_vsplitter->widget(0)->layout()->addWidget(timelineSplit);

    connect(m_showview, SIGNAL(functionDropped(quint32,quint32,Track*)),
            this, SLOT(slotFunctionDropped(quint32,quint32,Track*)));
    connect(m_showview, SIGNAL(addAtRequested(quint32,Track*)),
            this, SLOT(slotAddAtRequested(quint32,Track*)));
    connect(m_showview, SIGNAL(newTrackRequested()),
            this, SLOT(slotNewTrackRequested()));
    connect(m_showview, SIGNAL(markerAddRequested(quint32)),
            this, SLOT(slotMarkerAddRequested(quint32)));
    connect(m_showview, SIGNAL(markerEditRequested(quint32)),
            this, SLOT(slotMarkerEditRequested(quint32)));
    connect(m_showview, SIGNAL(markerDeleteRequested(quint32)),
            this, SLOT(slotMarkerDeleteRequested(quint32)));
    connect(m_showview, SIGNAL(markerColorRequested(quint32)),
            this, SLOT(slotMarkerColorRequested(quint32)));
    connect(m_showview, SIGNAL(markerRelabelRequested(quint32,QString)),
            this, SLOT(slotMarkerRelabel(quint32,QString)));
    connect(m_showview, SIGNAL(markerMovedRequested(quint32,quint32,quint32,QString,QColor)),
            this, SLOT(slotMarkerMoved(quint32,quint32,quint32,QString,QColor)));
    connect(m_showview, SIGNAL(itemDroppedBelowTracks(ShowItem*)),
            this, SLOT(slotItemDroppedBelowTracks(ShowItem*)));

    // add container for function editors
    QWidget* ccontainer = new QWidget(this);
    m_vsplitter->addWidget(ccontainer);
    ccontainer->setLayout(new QVBoxLayout);
    ccontainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_vsplitter->widget(1)->hide();

    // add container for scene editor
    QWidget* container = new QWidget(this);
    m_splitter->addWidget(container);
    container->setLayout(new QVBoxLayout);
    container->layout()->setContentsMargins(0, 0, 0, 0);
    m_splitter->widget(1)->hide();

    // Follow incoming timecode: drive the running show + cursor. (The MTC and
    // engine-load chips are app-wide and live in the main status-bar footer.)
    TimecodeSource *tc = m_doc->timecodeSource();
    connect(tc, SIGNAL(timeChanged(quint32)), this, SLOT(slotTimecodePosition(quint32)));
    connect(tc, SIGNAL(runningChanged(bool)), this, SLOT(slotTimecodeRunningChanged(bool)));

    connect(m_doc, SIGNAL(clearing()), this, SLOT(slotDocClearing()));
    connect(m_doc, SIGNAL(functionRemoved(quint32)), this, SLOT(slotFunctionRemoved(quint32)));
    connect(m_doc, SIGNAL(loaded()), this, SLOT(slotDocLoaded()));
    // Master show lock makes the whole timeline read-only.
    connect(m_doc, SIGNAL(showLockedChanged(bool)), this, SLOT(slotShowLockedChanged(bool)));
    m_showview->setEditable(m_doc->isShowLocked() == false);

    QSettings settings;
    QVariant var = settings.value(SETTINGS_HSPLITTER);
    if (var.isValid() == true)
        m_splitter->restoreState(var.toByteArray());
    else
        m_splitter->setSizes(QList <int> () << int(this->width() / 2) << int(this->width() / 2));

    QVariant var2 = settings.value(SETTINGS_VSPLITTER);
    if (var2.isValid() == true)
        m_vsplitter->restoreState(var2.toByteArray());
    else
        m_vsplitter->setSizes(QList <int> () << int(this->width() / 2) << int(this->width() / 2));
}

ShowManager::~ShowManager()
{
    QSettings settings;
    settings.setValue(SETTINGS_HSPLITTER, m_splitter->saveState());
    settings.setValue(SETTINGS_VSPLITTER, m_vsplitter->saveState());

    ShowManager::s_instance = NULL;
}

ShowManager* ShowManager::instance()
{
    return s_instance;
}

void ShowManager::clearContents()
{
    hideRightEditor();
    showSceneEditor(NULL);
    m_showview->resetView();
    m_showsCombo->clear();
    m_show = NULL;
    m_currentScene = NULL;
    m_currentTrack = NULL;
}

void ShowManager::initActions()
{
    /* Manage actions */
    m_addShowAction = new QAction(QIcon(":/show.png"),
                                   tr("New s&how"), this);
    m_addShowAction->setShortcut(QKeySequence("CTRL+H"));
    connect(m_addShowAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddShow()));

    m_renameShowAction = new QAction(QIcon(":/editclear.png"),
                                     tr("&Rename show…"), this);
    m_renameShowAction->setToolTip(tr("Rename the current show"));
    connect(m_renameShowAction, SIGNAL(triggered(bool)),
            this, SLOT(slotRenameShow()));

    m_deleteShowAction = new QAction(QIcon(":/editdelete.png"),
                                     tr("&Delete show…"), this);
    m_deleteShowAction->setToolTip(tr("Delete the current show"));
    connect(m_deleteShowAction, SIGNAL(triggered(bool)),
            this, SLOT(slotDeleteShow()));

    m_undoAction = new QAction(QIcon(":/back.png"), tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);   // Ctrl/Cmd+Z
    m_undoAction->setToolTip(tr("Undo the last timeline edit"));
    m_undoAction->setEnabled(false);
    connect(m_undoAction, SIGNAL(triggered(bool)), this, SLOT(slotUndo()));

    m_addTrackAction = new QAction(QIcon(":/edit_add.png"),
                                   tr("Add a &track or an existing function"), this);
    m_addTrackAction->setShortcut(QKeySequence("CTRL+N"));
    connect(m_addTrackAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddItem()));

    m_addSequenceAction = new QAction(QIcon(":/sequence.png"),
                                    tr("New s&equence"), this);
    m_addSequenceAction->setShortcut(QKeySequence("CTRL+E"));
    connect(m_addSequenceAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddSequence()));

    m_addAudioAction = new QAction(QIcon(":/audio.png"),
                                    tr("New &audio"), this);
    m_addAudioAction->setShortcut(QKeySequence("CTRL+A"));
    connect(m_addAudioAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddAudio()));

    m_addVideoAction = new QAction(QIcon(":/video.png"),
                                    tr("New vi&deo"), this);
    m_addVideoAction->setShortcut(QKeySequence("CTRL+D"));
    connect(m_addVideoAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddVideo()));

    /* Edit actions */
    m_copyAction = new QAction(QIcon(":/editcopy.png"),
                                tr("&Copy"), this);
    m_copyAction->setShortcut(QKeySequence("CTRL+C"));
    connect(m_copyAction, SIGNAL(triggered(bool)),
            this, SLOT(slotCopy()));
    m_copyAction->setEnabled(false);

    m_pasteAction = new QAction(QIcon(":/editpaste.png"),
                               tr("&Paste"), this);
    m_pasteAction->setShortcut(QKeySequence("CTRL+V"));
    connect(m_pasteAction, SIGNAL(triggered(bool)),
            this, SLOT(slotPaste()));
    m_pasteAction->setEnabled(false);

    m_deleteAction = new QAction(QIcon(":/editdelete.png"),
                                 tr("&Delete"), this);
    m_deleteAction->setShortcut(QKeySequence("Delete"));
    connect(m_deleteAction, SIGNAL(triggered(bool)),
            this, SLOT(slotDelete()));
    m_deleteAction->setEnabled(false);

    m_colorAction = new QAction(QIcon(":/color.png"),
                                tr("Change Co&lor"), this);
    m_colorAction->setShortcut(QKeySequence("CTRL+L"));
    connect(m_colorAction, SIGNAL(triggered(bool)),
           this, SLOT(slotChangeColor()));
    m_colorAction->setEnabled(false);

    m_lockAction = new QAction(QIcon(":/lock.png"),
                               tr("Lock item"), this);
    m_lockAction->setShortcut(QKeySequence("CTRL+K"));
    connect(m_lockAction, SIGNAL(triggered()),
            this, SLOT(slotChangeLock()));
    m_lockAction->setEnabled(false);

    m_timingsAction = new QAction(QIcon(":/speed.png"),
                                  tr("Item start time and duration"), this);
    m_timingsAction->setShortcut(QKeySequence("CTRL+T"));
    connect(m_timingsAction, SIGNAL(triggered()),
            this, SLOT(slotShowTimingsTool()));
    m_timingsAction->setEnabled(false);

    m_snapGridAction = new QAction(QIcon(":/grid.png"),
                                   tr("Snap to &Grid"), this);
    m_snapGridAction->setShortcut(QKeySequence("CTRL+G"));
    m_snapGridAction->setCheckable(true);
    connect(m_snapGridAction, SIGNAL(triggered(bool)),
           this, SLOT(slotToggleSnapToGrid(bool)));

    m_stopAction = new QAction(QIcon(":/player_stop.png"),
                                 tr("St&op"), this);
    m_stopAction->setShortcut(QKeySequence("CTRL+SPACE"));
    connect(m_stopAction, SIGNAL(triggered(bool)),
            this, SLOT(slotStopPlayback()));

    m_playAction = new QAction(QIcon(":/player_play.png"),
                                 tr("&Play"), this);
    m_playAction->setShortcut(QKeySequence("SPACE"));
    connect(m_playAction, SIGNAL(triggered(bool)),
            this, SLOT(slotStartPlayback()));

    m_followMtcAction = new QAction(QIcon(":/clock.png"),
                                 tr("Follow &MIDI Time Code"), this);
    m_followMtcAction->setCheckable(true);
    m_followMtcAction->setToolTip(tr("Follow incoming MIDI Time Code (e.g. from Logic). "
                                     "The timeline chases the timecode; when it stops, the "
                                     "show holds for manual GO."));
    connect(m_followMtcAction, SIGNAL(toggled(bool)),
            this, SLOT(slotFollowMtcToggled(bool)));
}

void ShowManager::initToolbar()
{
    // Add a toolbar to the dock area
    m_toolbar = new QToolBar("Show Manager", this);
    m_toolbar->setFloatable(false);
    m_toolbar->setMovable(false);
    layout()->addWidget(m_toolbar);
    m_toolbar->addAction(m_addShowAction);
    m_showsCombo = new QComboBox();
    m_showsCombo->setFixedWidth(250);
    m_showsCombo->setMaxVisibleItems(30);
    connect(m_showsCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotShowsComboChanged(int)));
    m_toolbar->addWidget(m_showsCombo);
    m_toolbar->addAction(m_renameShowAction);
    m_toolbar->addAction(m_deleteShowAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_addTrackAction);
    m_toolbar->addAction(m_addSequenceAction);
    m_toolbar->addAction(m_addAudioAction);
    m_toolbar->addAction(m_addVideoAction);

    m_toolbar->addSeparator();
    m_toolbar->addAction(m_undoAction);
    m_toolbar->addAction(m_copyAction);
    m_toolbar->addAction(m_pasteAction);
    m_toolbar->addAction(m_deleteAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_colorAction);
    m_toolbar->addAction(m_lockAction);
    m_toolbar->addAction(m_timingsAction);
    m_toolbar->addAction(m_snapGridAction);
    m_toolbar->addSeparator();

    // Time label and playback buttons
    m_timeLabel = new QLabel("00:00:00.00");
    m_timeLabel->setFixedWidth(150);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont timeFont = QApplication::font();
    timeFont.setBold(true);
    timeFont.setPixelSize(20);
    m_timeLabel->setFont(timeFont);
    m_toolbar->addWidget(m_timeLabel);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_stopAction);
    m_toolbar->addAction(m_playAction);
    m_toolbar->addSeparator();

    // The Follow-MTC TOGGLE now lives on the global main toolbar (App), so it is
    // reachable from any tab and MIDI-mappable via a VC FollowTimecode button.
    // m_followMtcAction is kept as the internal per-show state holder (its
    // toggled() still runs slotFollowMtcToggled) but is no longer shown here.
    // The MTC SOURCE + timeline-offset controls stay here — they are Show-Manager
    // configuration, not the global arming toggle.
    slotFollowMtcToggled(m_followMtcAction->isChecked());

    // Small caption so it's clear these configure the (global) MTC follow.
    QLabel *tcLabel = new QLabel(tr("  MTC: "));
    m_toolbar->addWidget(tcLabel);

    m_tcSourceCombo = new QComboBox();
    m_tcSourceCombo->setFixedWidth(150);
    m_tcSourceCombo->setToolTip(tr("MIDI Time Code source. Auto = follow any input "
                                   "sending timecode; or lock onto one universe."));
    connect(m_tcSourceCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotTcSourceChanged(int)));
    m_toolbar->addWidget(m_tcSourceCombo);
    updateTcSourceCombo();

    // Timecode offset: which incoming SMPTE time maps to timeline 0. Logic
    // usually rolls from 01:00:00:00, so offer that as a one-click default.
    QToolButton *tcOffBtn = new QToolButton();
    tcOffBtn->setText(tr("TC 0…"));
    tcOffBtn->setToolTip(tr("Set which incoming timecode value lines up with the "
                            "start of this show's timeline."));
    tcOffBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *tcMenu = new QMenu(tcOffBtn);
    QAction *tcHour = tcMenu->addAction(tr("Timeline 0 = 01:00:00:00 (Logic default)"));
    QAction *tcNow  = tcMenu->addAction(tr("Timeline 0 = current timecode"));
    QAction *tcZero = tcMenu->addAction(tr("No offset (timeline 0 = 00:00:00:00)"));
    connect(tcHour, &QAction::triggered, this, [this]() {
        if (m_show) { m_show->setTimecodeOffset(3600000); m_doc->setModified(); } });
    connect(tcNow, &QAction::triggered, this, [this]() {
        if (m_show) { m_show->setTimecodeOffset(m_doc->timecodeSource()->positionMs());
                      m_doc->setModified(); } });
    connect(tcZero, &QAction::triggered, this, [this]() {
        if (m_show) { m_show->setTimecodeOffset(0); m_doc->setModified(); } });
    tcOffBtn->setMenu(tcMenu);
    m_toolbar->addWidget(tcOffBtn);

    /* Create an empty widget between help items to flush them to the right */
    QWidget* widget = new QWidget(this);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(widget);

    /* Add time division elements */
    QLabel* timeLabel = new QLabel(tr("Time division:"));
    m_toolbar->addWidget(timeLabel);

    m_timeDivisionCombo = new QComboBox();
    m_timeDivisionCombo->setFixedWidth(100);
    m_timeDivisionCombo->addItem(tr("Time"), Show::Time);
    m_timeDivisionCombo->addItem("BPM 4/4", Show::BPM_4_4);
    m_timeDivisionCombo->addItem("BPM 3/4", Show::BPM_3_4);
    m_timeDivisionCombo->addItem("BPM 2/4", Show::BPM_2_4);
    m_toolbar->addWidget(m_timeDivisionCombo);
    connect(m_timeDivisionCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotTimeDivisionTypeChanged(int)));

    m_bpmField = new QSpinBox();
    m_bpmField->setFixedWidth(70);
    m_bpmField->setMinimum(10);
    m_bpmField->setMaximum(240);
    m_bpmField->setValue(120);
    m_bpmField->setEnabled(false);
    m_toolbar->addWidget(m_bpmField);
    connect(m_bpmField, SIGNAL(valueChanged(int)),
            this, SLOT(slotBPMValueChanged(int)));
}

/*********************************************************************
 * Shows combo
 *********************************************************************/
void ShowManager::updateShowsCombo()
{
    int oldIndex = m_showsCombo->currentIndex();

    // protect poor Show Manager from drawing all the shows
    disconnect(m_showsCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotShowsComboChanged(int)));

    m_showsCombo->clear();
    foreach (Function* f, m_doc->functionsByType(Function::ShowType))
    {
        // Insert in ascii order
        int insertPosition = 0;
        while (insertPosition < m_showsCombo->count() &&
               QString::localeAwareCompare(m_showsCombo->itemText(insertPosition), f->name()) <= 0)
                    ++insertPosition;
        m_showsCombo->insertItem(insertPosition, f->name(), QVariant(f->id()));
    }
    if (m_showsCombo->count() > 0)
    {
        m_addTrackAction->setEnabled(true);
    }
    else
    {
        m_addTrackAction->setEnabled(false);
        m_addSequenceAction->setEnabled(false);
        m_addAudioAction->setEnabled(false);
        m_addVideoAction->setEnabled(false);
    }

    if (m_show == NULL || m_show->getTracksCount() == 0)
    {
        m_deleteAction->setEnabled(false);
        m_pasteAction->setEnabled(false);
    }
    else
    {
        if (m_doc->clipboard()->hasFunction())
            m_pasteAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
    }

    connect(m_showsCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotShowsComboChanged(int)));

    if (m_showsCombo->count() == 0)
    {
        m_showview->resetView();
        m_showview->setEmptyMessage(tr("No show yet.\n\nClick the “New Show” button "
            "(top-left) to create one, then drop scenes, chasers or collections "
            "onto its tracks."));
        m_show = NULL;
        m_currentScene = NULL;
        m_currentTrack = NULL;
        updateShowControls();
        return;
    }

    if (m_selectedShowIndex < 0 || m_selectedShowIndex >= m_showsCombo->count())
        m_selectedShowIndex = 0;

    m_showsCombo->setCurrentIndex(m_selectedShowIndex);

    if (oldIndex != m_selectedShowIndex)
        updateMultiTrackView();

    updateShowControls();
}

void ShowManager::slotShowsComboChanged(int idx)
{
    qDebug() << Q_FUNC_INFO << "Idx: " << idx;
    if (m_selectedShowIndex != idx)
    {
        m_selectedShowIndex = idx;
        // Undo history is per-show; switching shows starts fresh.
        m_undoStack.clear();
        if (m_undoAction != NULL)
            m_undoAction->setEnabled(false);
        hideRightEditor();
        updateMultiTrackView();
    }
}

void ShowManager::showSceneEditor(Scene *scene)
{
    if (m_sceneEditor != NULL)
    {
        emit functionManagerActive(false);       
        m_splitter->widget(1)->layout()->removeWidget(m_sceneEditor);
        m_splitter->widget(1)->hide();
        delete m_sceneEditor;
        m_sceneEditor = NULL;
    }

    if (scene == NULL)
        return;

    if (this->isVisible())
    {
        m_sceneEditor = new SceneEditor(m_splitter->widget(1), scene, m_doc, false);
        if (m_sceneEditor != NULL)
        {
            m_splitter->widget(1)->layout()->addWidget(m_sceneEditor);
            m_splitter->widget(1)->show();

            connect(this, SIGNAL(functionManagerActive(bool)),
                    m_sceneEditor, SLOT(slotFunctionManagerActive(bool)));
        }
    }
}

void ShowManager::hideRightEditor()
{
    if (m_currentEditor != NULL)
    {
        m_vsplitter->widget(1)->layout()->removeWidget(m_currentEditor);
        m_vsplitter->widget(1)->hide();
        delete m_currentEditor;
        m_currentEditor = NULL;
        m_editorFunctionID = Function::invalidId();
    }
}

void ShowManager::showRightEditor(Function *function)
{
    if (function != NULL && m_editorFunctionID == function->id())
        return;

    hideRightEditor();

    if (function == NULL || this->isVisible() == false)
        return;

    if (function->type() == Function::ChaserType)
    {
        Chaser *chaser = qobject_cast<Chaser*> (function);
        m_currentEditor = new ChaserEditor(m_vsplitter->widget(1), chaser, m_doc);
        if (m_currentEditor != NULL)
        {
            connect(m_currentEditor, SIGNAL(stepSelectionChanged(int)),
                    this, SLOT(slotStepSelectionChanged(int)));
        }
    }
    else if (function->type() == Function::SequenceType)
    {
        Sequence *sequence = qobject_cast<Sequence*> (function);
        m_currentEditor = new ChaserEditor(m_vsplitter->widget(1), sequence, m_doc);
        if (m_currentEditor != NULL)
        {
            ChaserEditor *editor = qobject_cast<ChaserEditor*>(m_currentEditor);

            editor->showOrderAndDirection(false);

            /** Signal from chaser editor to scene editor.
             *  When a step is clicked apply values immediately */
            connect(m_currentEditor, SIGNAL(applyValues(QList<SceneValue>&)),
                    m_sceneEditor, SLOT(slotSetSceneValues(QList <SceneValue>&)));

            /** Signal from scene editor to chaser editor.
             *  When a fixture value is changed, update the selected chaser step */
            connect(m_sceneEditor, SIGNAL(fixtureValueChanged(SceneValue,bool)),
                    m_currentEditor, SLOT(slotUpdateCurrentStep(SceneValue,bool)));

            connect(m_currentEditor, SIGNAL(stepSelectionChanged(int)),
                    this, SLOT(slotStepSelectionChanged(int)));
        }
    }
    else if (function->type() == Function::AudioType)
    {
        m_currentEditor = new AudioEditor(m_vsplitter->widget(1), qobject_cast<Audio*> (function), m_doc);
    }
    else if (function->type() == Function::RGBMatrixType)
    {
        m_currentEditor = new RGBMatrixEditor(m_vsplitter->widget(1), qobject_cast<RGBMatrix*> (function), m_doc);
    }
    else if (function->type() == Function::EFXType)
    {
        m_currentEditor = new EFXEditor(m_vsplitter->widget(1), qobject_cast<EFX*> (function), m_doc);
    }
    else if (function->type() == Function::VideoType)
    {
        m_currentEditor = new VideoEditor(m_vsplitter->widget(1), qobject_cast<Video*> (function), m_doc);
    }
    else
        return;

    if (m_currentEditor != NULL)
    {
        m_vsplitter->widget(1)->layout()->addWidget(m_currentEditor);
        m_vsplitter->widget(1)->show();
        m_currentEditor->show();
        m_editorFunctionID = function->id();
    }

}

void ShowManager::slotAddShow()
{
    bool ok;
    QString defaultName = QString("%1 %2").arg(tr("New Show")).arg(m_doc->nextFunctionID());
    QString showName = QInputDialog::getText(this, tr("Show name setup"),
                                         tr("Show name:"), QLineEdit::Normal,
                                         defaultName, &ok);

    if (ok == true)
    {
        m_show = new Show(m_doc);
        if (showName.isEmpty() == false)
            m_show->setName(showName);
        else
            m_show->setName(defaultName);
        Function *f = qobject_cast<Function*>(m_show);
        if (m_doc->addFunction(f) == true)
        {
            // modify the new selected Show index
            int insertPosition = 0;
            while (insertPosition < m_showsCombo->count() &&
                    QString::localeAwareCompare(m_showsCombo->itemText(insertPosition), m_show->name()) <= 0)
                ++insertPosition;
            m_selectedShowIndex = insertPosition;
            updateShowsCombo();
            m_copyAction->setEnabled(false);
            if (m_doc->clipboard()->hasFunction())
                m_pasteAction->setEnabled(true);
            showSceneEditor(NULL);
            hideRightEditor();
            m_currentScene = NULL;
            m_currentTrack = NULL;
        }
    }
}

void ShowManager::slotRenameShow()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Rename show"),
                                         tr("Show name:"), QLineEdit::Normal,
                                         m_show->name(), &ok);
    if (ok == false || name.trimmed().isEmpty())
        return;

    m_show->setName(name.trimmed());
    m_doc->setModified();
    // Rebuild the combo (keeps it alphabetically sorted + reselects this show).
    m_selectedShowIndex = -1;
    for (int i = 0; i < m_showsCombo->count(); i++)
        if (m_showsCombo->itemData(i).toUInt() == m_show->id())
            m_selectedShowIndex = i;
    updateShowsCombo();
}

void ShowManager::slotDeleteShow()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    if (QMessageBox::question(this, tr("Delete show"),
            tr("Delete the show \"%1\"? This removes its timeline (tracks and "
               "their timing); the scenes/chasers it references are NOT deleted.")
                .arg(m_show->name()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (m_show->isRunning())
        m_show->stopAndWait();

    quint32 id = m_show->id();
    hideRightEditor();
    showSceneEditor(NULL);
    m_currentScene = NULL;
    m_currentTrack = NULL;
    m_show = NULL;
    m_selectedShowIndex = 0;
    m_doc->deleteFunction(id);   // fires functionRemoved → updateShowsCombo
    updateShowsCombo();
}

void ShowManager::updateShowControls()
{
    const bool hasShow = (m_showsCombo != NULL && m_showsCombo->count() > 0);
    if (m_renameShowAction != NULL)
        m_renameShowAction->setEnabled(hasShow);
    if (m_deleteShowAction != NULL)
        m_deleteShowAction->setEnabled(hasShow);
    if (m_addTrackAction != NULL)
        m_addTrackAction->setEnabled(hasShow);
}

/*********************************************************************
 * Undo (coarse whole-timeline snapshots)
 *********************************************************************/

ShowManager::TimelineSnapshot ShowManager::captureSnapshot() const
{
    TimelineSnapshot snap;
    if (m_show == NULL)
        return snap;

    foreach (Track *track, m_show->tracks())
    {
        TrackSnapshot ts;
        ts.name = track->name();
        ts.mute = track->isMute();
        ts.color = track->color();
        ts.sceneID = track->getSceneID();
        foreach (ShowFunction *sf, track->showFunctions())
        {
            SFSnapshot fs;
            fs.functionID = sf->functionID();
            fs.startTime = sf->startTime();
            fs.duration = sf->duration();
            fs.color = sf->color();
            ts.funcs.append(fs);
        }
        snap.tracks.append(ts);
    }
    snap.markers = m_show->markers();
    return snap;
}

void ShowManager::pushUndoSnapshot()
{
    if (m_show == NULL)
        return;

    m_undoStack.append(captureSnapshot());
    // Keep only the last handful of changes.
    const int kMaxUndo = 25;
    while (m_undoStack.count() > kMaxUndo)
        m_undoStack.removeFirst();

    if (m_undoAction != NULL)
        m_undoAction->setEnabled(true);
}

void ShowManager::restoreSnapshot(const TimelineSnapshot &snap)
{
    if (m_show == NULL)
        return;

    // The current selection/editor point into objects we're about to delete.
    hideRightEditor();
    showSceneEditor(NULL);
    m_currentTrack = NULL;
    m_currentScene = NULL;

    // Tear down the current tracks (removeTrack deletes the Track + its
    // ShowFunctions). Iterate over a copied id list — tracks() is live.
    QList<quint32> ids;
    foreach (Track *t, m_show->tracks())
        ids.append(t->id());
    foreach (quint32 id, ids)
        m_show->removeTrack(id);

    // Rebuild tracks + their placed functions.
    foreach (const TrackSnapshot &ts, snap.tracks)
    {
        Track *track = new Track(ts.sceneID);
        track->setName(ts.name);
        track->setMute(ts.mute);
        track->setColor(ts.color);
        m_show->addTrack(track);
        foreach (const SFSnapshot &fs, ts.funcs)
        {
            ShowFunction *sf = track->createShowFunction(fs.functionID);
            sf->setStartTime(fs.startTime);
            sf->setDuration(fs.duration);
            sf->setColor(fs.color);
        }
    }

    // Replace markers.
    const QList<quint32> curKeys = m_show->markers().keys();
    foreach (quint32 k, curKeys)
        m_show->removeMarker(k);
    QMapIterator<quint32, ShowMarker> mit(snap.markers);
    while (mit.hasNext())
    {
        mit.next();
        m_show->setMarker(mit.key(), mit.value().end, mit.value().label, mit.value().color);
    }

    m_doc->setModified();
    updateMultiTrackView();
}

void ShowManager::slotUndo()
{
    if (m_undoStack.isEmpty() || m_show == NULL)
        return;

    // Don't disturb a live playback with a structural rebuild.
    if (m_show->isRunning())
        m_show->stopAndWait();

    TimelineSnapshot snap = m_undoStack.takeLast();
    restoreSnapshot(snap);

    if (m_undoAction != NULL)
        m_undoAction->setEnabled(m_undoStack.isEmpty() == false);
}

void ShowManager::slotAddItem()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    FunctionSelection fs(this, m_doc);
    // Forbid self-containment
    QList<quint32> disabledList;
    foreach (Function* function, m_doc->functions())
    {
        if (function->contains(m_show->id()))
            disabledList << function->id();
    }
    fs.setDisabledFunctions(disabledList);

    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType | Function::AudioType | Function::RGBMatrixType | Function::EFXType | Function::CollectionType);
    fs.disableFilters(Function::ShowType | Function::ScriptType);
    fs.showNewTrack(true);

    if (fs.exec() == QDialog::Accepted)
    {
        QList <quint32> ids = fs.selection();
        if (ids.count() == 0)
            return;
        quint32 selectedID = ids.first();

        /**
         * Here there are 8 cases:
         * 1) a new empty track
         * 2) an existing scene: create a new track with a 10 seconds Sequence
         * 3) an existing sequence
         *    3.1) append to an existing track
         *    3.2) create a new track bound to the Sequence's Scene ID
         * 4) an existing chaser
         *    4.1) append to the selected track
         *    4.3) create a new track
         * 5) an existing audio:
         *    5.1) append to the selected track
         *    5.2) create a new track
         * 6) an existing RGB Matrix:
         *    6.1) append to the selected track
         *    6.2) create a new track
         * 7) an existing EFX:
         *    7.1) append to the selected track
         *    7.2) create a new track
         * 8) an existing video:
         *    8.1) append to the selected track
         *    8.2) create a new track
         **/

        bool createTrack = false;
        quint32 newTrackBoundID = Function::invalidId();

        if (selectedID == Function::invalidId())
        {
            createTrack = true;
        }
        else
        {
            Function *selectedFunc = m_doc->function(selectedID);
            if (selectedFunc == NULL) // maybe a popup here ?
                return;

            /** 2) an existing scene */
            if (selectedFunc->type() == Function::SceneType)
            {
                m_currentScene = qobject_cast<Scene*>(selectedFunc);
                newTrackBoundID = selectedFunc->id();
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::ChaserType)
            {
                /** 4.1) add chaser to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addSequence(qobject_cast<Chaser*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 4.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::SequenceType)
            {
                Sequence *sequence = qobject_cast<Sequence*>(selectedFunc);
                quint32 chsSceneID = sequence->boundSceneID();
                foreach (Track *track, m_show->tracks())
                {
                    /** 3.1) append to an existing track */
                    if (track->getSceneID() == chsSceneID)
                    {
                        Sequence *newSequence = qobject_cast<Sequence*>(sequence->createCopy(m_doc, true));
                        newSequence->setName(sequence->name() + tr(" (Copy)"));
                        newSequence->setDirection(Function::Forward);
                        newSequence->setRunOrder(Function::SingleShot);
                        m_showview->addSequence(newSequence, track);
                        m_doc->setModified();
                        return;
                    }
                }
                /** 3.2) It is necessary to create a new track (below) */
                createTrack = true;
                newTrackBoundID = sequence->boundSceneID();
                m_currentScene = qobject_cast<Scene*>(m_doc->function(newTrackBoundID));
            }
            else if (selectedFunc->type() == Function::AudioType)
            {
                /** 5.1) add audio to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addAudio(qobject_cast<Audio*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 5.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::RGBMatrixType)
            {
                /** 6.1) add RGB Matrix to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addRGBMatrix(qobject_cast<RGBMatrix*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 6.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::EFXType)
            {
                /** 7.1) add EFX to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addEFX(qobject_cast<EFX*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 7.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::VideoType)
            {
                /** 8.1) add video to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addVideo(qobject_cast<Video*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 8.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
            else if (selectedFunc->type() == Function::CollectionType)
            {
                /** 9.1) add collection to the currently selected track */
                if (m_currentTrack != NULL)
                {
                    m_showview->addCollection(qobject_cast<Collection*>(selectedFunc), m_currentTrack);
                    m_doc->setModified();
                    return;
                }
                /** 9.2) It is necessary to create a new track (below) */
                createTrack = true;
            }
        }

        if (createTrack == true)
        {
            Track* newTrack = new Track(newTrackBoundID);
            if (newTrackBoundID != Function::invalidId() && m_currentScene != NULL)
                newTrack->setName(m_currentScene->name());
            else
                newTrack->setName(tr("Track %1").arg(m_show->tracks().count() + 1));

            m_show->addTrack(newTrack);
            m_showview->addTrack(newTrack);
            m_currentTrack = newTrack;
            if (newTrackBoundID == Function::invalidId())
                m_currentScene = NULL;
            else
                m_currentScene = qobject_cast<Scene*>(m_doc->function(newTrackBoundID));
        }

        if (selectedID != Function::invalidId())
        {
            Function *selectedFunc = m_doc->function(selectedID);
            if (selectedFunc == NULL) // maybe a popup here ?
                return;

            /** 2) create a 10 seconds Sequence on the current track */
            if (selectedFunc->type() == Function::SceneType)
            {
                Function* f = new Sequence(m_doc);
                Sequence *sequence = qobject_cast<Sequence*> (f);
                sequence->setBoundSceneID(m_currentScene->id());
                if (m_doc->addFunction(f) == true)
                {
                    sequence->setDirection(Function::Forward);
                    sequence->setRunOrder(Function::SingleShot);
                    sequence->setDurationMode(Chaser::PerStep);
                    m_currentScene->setVisible(false);
                    f->setName(QString("%1 %2").arg(tr("New Sequence")).arg(f->id()));
                    m_showview->addSequence(sequence, m_currentTrack);
                    ChaserStep step(m_currentScene->id(), m_currentScene->fadeInSpeed(), 10000, m_currentScene->fadeOutSpeed());
                    step.note = QString();
                    step.values.append(m_currentScene->values());
                    sequence->addStep(step);
                }
            }
            else if (selectedFunc->type() == Function::ChaserType)
            {
                /** 4.2) add chaser to the new track */
                m_showview->addSequence(qobject_cast<Chaser*>(selectedFunc), m_currentTrack);
            }
            else if (selectedFunc->type() == Function::SequenceType)
            {
                /** 3.2) create a new Scene and bind a Sequence clone to it */
                Sequence *sequence = qobject_cast<Sequence*>(selectedFunc);
                Sequence *newSequence = qobject_cast<Sequence*>(sequence->createCopy(m_doc, true));
                newSequence->setName(sequence->name() + tr(" (Copy)"));
                newSequence->setDirection(Function::Forward);
                newSequence->setRunOrder(Function::SingleShot);
                m_showview->addSequence(newSequence, m_currentTrack);
            }
            else if (selectedFunc->type() == Function::AudioType)
            {
                /** 5.2) add audio to the new track */
                Audio *audio = qobject_cast<Audio*> (selectedFunc);
                m_showview->addAudio(audio, m_currentTrack);
            }
            else if (selectedFunc->type() == Function::RGBMatrixType)
            {
                /** 6.2) add RGBMatrix to the new track */
                RGBMatrix *rgbm = qobject_cast<RGBMatrix*> (selectedFunc);
                m_showview->addRGBMatrix(rgbm, m_currentTrack);
            }
            else if (selectedFunc->type() == Function::EFXType)
            {
                /** 7.2) add EFX to the new track */
                EFX *efx = qobject_cast<EFX*> (selectedFunc);
                m_showview->addEFX(efx, m_currentTrack);
            }
            else if (selectedFunc->type() == Function::VideoType)
            {
                /** 8.2) add video to the new track */
                Video *video = qobject_cast<Video*> (selectedFunc);
                m_showview->addVideo(video, m_currentTrack);
            }
            else if (selectedFunc->type() == Function::CollectionType)
            {
                /** 9.2) add collection to the new track */
                Collection *collection = qobject_cast<Collection*> (selectedFunc);
                m_showview->addCollection(collection, m_currentTrack);
            }
        }
        m_doc->setModified();

        m_addSequenceAction->setEnabled(true);
        m_addAudioAction->setEnabled(true);
        m_addVideoAction->setEnabled(true);
        m_showview->activateTrack(m_currentTrack);
        m_deleteAction->setEnabled(true);
        m_showview->updateViewSize();
    }
}

void ShowManager::slotAddSequence()
{
    // Overlapping check
    if (checkOverlapping(m_showview->getTimeFromCursor(), 1000) == true)
    {
        QMessageBox::warning(this, tr("Overlapping error"), tr("Overlapping not allowed. Operation canceled."));
        return;
    }

    if (m_currentTrack->getSceneID() == Function::invalidId())
    {
        m_currentScene = new Scene(m_doc);
        m_currentScene->setVisible(false);

        if (m_doc->addFunction(m_currentScene))
            m_currentScene->setName(tr("Scene for %1 - Track %2").arg(m_show->name()).arg(m_currentTrack->id() + 1));
        m_currentTrack->setSceneID(m_currentScene->id());
    }

    Function* f = new Sequence(m_doc);
    Sequence *sequence = qobject_cast<Sequence*> (f);
    sequence->setBoundSceneID(m_currentScene->id());

    if (m_doc->addFunction(f) == true)
    {
        sequence->setRunOrder(Function::SingleShot);
        m_currentScene->setVisible(false);
        f->setName(QString("%1 %2").arg(tr("New Sequence")).arg(f->id()));
        // No editor-to-the-right in this view (see slotShowItemMoved); the
        // sequence's steps are edited in the Functions/Programming tabs.
        m_showview->addSequence(sequence, m_currentTrack);
    }
}

void ShowManager::slotAddAudio()
{
    QString fn;

    /* Create a file open dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Open Audio File"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    //dialog.selectFile(fileName());

    /* Append file filters to the dialog */
    QStringList extList = m_doc->audioPluginCache()->getSupportedFormats();

    QStringList filters;
    qDebug() << Q_FUNC_INFO << "Extensions: " << extList.join(" ");
    filters << tr("Audio Files (%1)").arg(extList.join(" "));
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
        return;

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return;

    Function* f = new Audio(m_doc);
    Audio *audio = qobject_cast<Audio*> (f);
    if (audio->setSourceFileName(fn) == false)
    {
        QMessageBox::warning(this, tr("Unsupported audio file"), tr("This audio file cannot be played with QLC+. Sorry."));
        delete f;
        return;
    }
    // Overlapping check
    if (checkOverlapping(m_showview->getTimeFromCursor(), audio->totalDuration()) == true)
    {
        QMessageBox::warning(this, tr("Overlapping error"), tr("Overlapping not allowed. Operation canceled."));
        delete f;
        return;
    }
    if (m_doc->addFunction(f) == true)
    {
        m_showview->addAudio(audio, m_currentTrack);
    }
}

void ShowManager::slotAddVideo()
{
    QString fn;

    /* Create a file open dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Open Video File"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    //dialog.selectFile(fileName());

    /* Append file filters to the dialog */
    QStringList extList = Video::getVideoCapabilities();

    QStringList filters;
    qDebug() << Q_FUNC_INFO << "Extensions: " << extList.join(" ");
    filters << tr("Video Files (%1)").arg(extList.join(" "));
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
        return;

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return;

    Function* f = new Video(m_doc);
    Video *video = qobject_cast<Video*> (f);
    if (video->setSourceUrl(fn) == false)
    {
        QMessageBox::warning(this, tr("Unsupported video file"), tr("This video file cannot be played with QLC+. Sorry."));
        delete f;
        return;
    }
    // Overlapping check
    if (checkOverlapping(m_showview->getTimeFromCursor(), video->totalDuration()) == true)
    {
        QMessageBox::warning(this, tr("Overlapping error"), tr("Overlapping not allowed. Operation canceled."));
        delete f;
        return;
    }
    if (m_doc->addFunction(f) == true)
    {
        m_showview->addVideo(video, m_currentTrack);
    }
}

void ShowManager::slotCopy()
{
    ShowItem *item = m_showview->getSelectedItem();
    if (item != NULL)
    {
        Function* function = m_doc->function(item->functionID());
        Q_ASSERT(function != NULL);

        m_doc->clipboard()->copyContent(m_show->id(), function);
        m_pasteAction->setEnabled(true);
    }
}

void ShowManager::slotPaste()
{
    if (m_doc->clipboard()->hasFunction() == false)
        return;

    // Get the Function copy and add it to Doc
    Function* clipboardCopy = m_doc->clipboard()->getFunction();
    quint32 copyDuration = clipboardCopy->totalDuration();

    // Overlapping check
    if (checkOverlapping(m_showview->getTimeFromCursor(), copyDuration) == true)
    {
        QMessageBox::warning(this, tr("Paste error"), tr("Overlapping paste not allowed. Operation canceled."));
        return;
    }
    //qDebug() << "Check overlap... cursor time:" << cursorTime << "msec";

    if (clipboardCopy != NULL)
    {
        // copy the function again, to allow multiple copies of the same function
        Function* newCopy = clipboardCopy->createCopy(m_doc, false);
        if (newCopy == NULL)
            return;

        if (clipboardCopy->type() == Function::ChaserType)
        {
            Chaser *chaser = qobject_cast<Chaser*>(newCopy);

            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            m_showview->addSequence(chaser, m_currentTrack);
        }
        else if (clipboardCopy->type() == Function::SequenceType)
        {
            Sequence *sequence = qobject_cast<Sequence*>(newCopy);

            if (m_currentScene == NULL)
            {
                // No scene on the current track -> copy it from source sequence
                Scene* clipboardCopyScene = qobject_cast<Scene*>(m_doc->function(sequence->boundSceneID()));
                if (clipboardCopyScene == NULL)
                {
                    delete newCopy;
                    return;
                }
                Scene* newScene = static_cast<Scene*>(clipboardCopyScene->createCopy(m_doc, true));
                if (newScene == NULL)
                {
                    delete newCopy;
                    return;
                }
                m_currentScene = newScene;
                m_currentTrack->setSceneID(m_currentScene->id());
            }
            else
            {
                // Verify the Chaser copy steps against the current Scene
                foreach (ChaserStep cs, sequence->steps())
                {
                    foreach (SceneValue scv, cs.values)
                    {
                        if (m_currentScene->checkValue(scv) == false)
                        {
                            QMessageBox::warning(this, tr("Paste error"), tr("Trying to paste on an incompatible Scene. Operation canceled."));
                            delete newCopy;
                            return;
                        }
                    }
                }
            }

            // Bind the sequence to the track Scene ID
            sequence->setBoundSceneID(m_currentScene->id());

            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            Track *track = m_currentTrack;
            track = m_show->getTrackFromSceneID(m_currentScene->id());
            m_showview->addSequence(sequence, track);
        }
        else if (clipboardCopy->type() == Function::AudioType)
        {
            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            Audio *audio = qobject_cast<Audio*>(newCopy);
            m_showview->addAudio(audio, m_currentTrack);
        }
        else if (clipboardCopy->type() == Function::RGBMatrixType)
        {
            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            RGBMatrix *rgbm = qobject_cast<RGBMatrix*>(newCopy);
            m_showview->addRGBMatrix(rgbm, m_currentTrack);
        }
        else if (clipboardCopy->type() == Function::EFXType)
        {
            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            EFX *efx = qobject_cast<EFX*>(newCopy);
            m_showview->addEFX(efx, m_currentTrack);
        }
        else if (clipboardCopy->type() == Function::VideoType)
        {
            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            Video *video = qobject_cast<Video*>(newCopy);
            m_showview->addVideo(video, m_currentTrack);
        }
        else if (clipboardCopy->type() == Function::SceneType)
        {
            if (m_doc->addFunction(newCopy) == false)
            {
                delete newCopy;
                return;
            }
            m_currentScene = qobject_cast<Scene*>(newCopy);
            Track* newTrack = new Track(m_currentScene->id());
            newTrack->setName(m_currentScene->name());
            m_show->addTrack(newTrack);
            //showSceneEditor(m_currentScene);
            m_showview->addTrack(newTrack);
            m_addSequenceAction->setEnabled(true);
            m_addAudioAction->setEnabled(true);
            m_addVideoAction->setEnabled(true);
            m_showview->activateTrack(newTrack);
            m_deleteAction->setEnabled(true);
            m_showview->updateViewSize();
        }
    }
}

void ShowManager::slotDelete()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    // Delete removes the SELECTED timeline item(s). It no longer nukes a track
    // by surprise when nothing is selected — tracks are deleted via the track
    // header's right-click menu (which confirms). Nothing selected => no-op.
    QList<ShowItem *> items = m_showview->selectedItems();
    if (items.isEmpty())
        return;

    // Gut-check when deleting more than one item.
    if (items.count() > 1)
    {
        QString msg = tr("Delete %1 items from the timeline?").arg(items.count())
                      + QString("\n");
        foreach (ShowItem *it, items)
            msg += QString("\n • ") + it->functionName();
        msg += QString("\n\n") + tr("(The functions themselves are kept.)");
        if (QMessageBox::question(this, tr("Delete items"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    pushUndoSnapshot();
    hideRightEditor();
    showSceneEditor(NULL);

    // Remove each item's ShowFunction from its owning track. Find the owner by
    // membership (robust vs. track-index drift).
    foreach (ShowItem *it, items)
    {
        ShowFunction *sf = it->showFunction();
        if (sf == NULL)
            continue;
        foreach (Track *track, m_show->tracks())
        {
            if (track->showFunctions().contains(sf))
            {
                track->removeShowFunction(sf);
                break;
            }
        }
    }

    m_currentScene = NULL;
    m_doc->setModified();
    updateMultiTrackView();   // rebuild cleanly (keeps view/model in sync)
}

void ShowManager::slotStopPlayback()
{
    m_playAction->setIcon(QIcon(":/player_play.png"));
    if (m_show != NULL && m_show->isRunning())
    {
        m_show->stop(functionParent());
        return;
    }
    m_showview->rewindCursor();
    m_timeLabel->setText("00:00:00.00");
}

void ShowManager::slotStartPlayback()
{
    if (m_showsCombo->count() == 0 || m_show == NULL)
        return;

    if (m_show->isRunning() == false)
    {
        cursorMovedDuringPause = false;
        m_show->start(m_doc->masterTimer(), functionParent(), m_showview->getTimeFromCursor());
        m_playAction->setIcon(QIcon(":/player_pause.png"));
    }
    else
    {
        if (m_show->isPaused())
        {
            m_playAction->setIcon(QIcon(":/player_pause.png"));
            if (cursorMovedDuringPause)
            {
                m_show->stop(functionParent());
                m_show->stopAndWait();
                cursorMovedDuringPause = false;
                m_show->start(m_doc->masterTimer(), functionParent(), m_showview->getTimeFromCursor());
            }
            else
            {
                m_show->setPause(false);
            }
        }
        else
        {
            m_playAction->setIcon(QIcon(":/player_play.png"));
            m_show->setPause(true);
        }
    }
}

void ShowManager::slotShowStopped()
{
    slotUpdateTime(m_showview->getTimeFromCursor());
    emit timelineControlChanged();
}

void ShowManager::slotShowRunningChanged()
{
    emit timelineControlChanged();
}

/*********************************************************************
 * Timeline control (global coordination — Operate mode)
 *********************************************************************/

bool ShowManager::timelineControlActive() const
{
    // The timeline drives the rig only in Operate, while a show is running and
    // not suspended. In Design the active-tab ownership model applies instead.
    return m_doc->mode() == Doc::Operate &&
           m_show != NULL && m_show->isRunning() &&
           m_show->isTimelineSuspended() == false;
}

bool ShowManager::timelineSuspended() const
{
    return m_show != NULL && m_show->isRunning() && m_show->isTimelineSuspended();
}

void ShowManager::setTimelineSuspended(bool enable)
{
    if (m_show == NULL || m_show->isRunning() == false)
        return;
    if (m_show->isTimelineSuspended() == enable)
        return;

    m_show->setTimelineSuspended(enable);
    emit timelineControlChanged();
}

void ShowManager::toggleTimelineSuspended()
{
    if (m_show == NULL || m_show->isRunning() == false)
        return;
    setTimelineSuspended(m_show->isTimelineSuspended() == false);
}

void ShowManager::setFollowTimecode(bool enable)
{
    if (m_followMtcAction != NULL && m_followMtcAction->isChecked() != enable)
    {
        // The toolbar action's toggled() slot does the real work + persistence.
        m_followMtcAction->setChecked(enable);
    }
    else
    {
        slotFollowMtcToggled(enable);
    }
    // slotFollowMtcToggled emits followTimecodeChanged for us (either via the
    // action's toggled() or the direct call above).
}

bool ShowManager::followTimecode() const
{
    return m_show != NULL && m_show->timecodeFollow();
}

void ShowManager::toggleFollowTimecode()
{
    setFollowTimecode(followTimecode() == false);
}

ShowFunction *ShowManager::addFunctionToTrack(Function *f, Track *track, quint32 startTime)
{
    if (f == NULL || track == NULL)
        return NULL;

    Function::Type t = f->type();

    // A bare Scene is placed directly as a simple timed clip (SceneItem). The
    // ShowRunner runs the Scene for the clip's duration — no hidden Sequence /
    // chase wrapper (which used to clutter the Functions list and confuse the
    // "a scene became a chase?" mental model).
    if (t == Function::SceneType)
    {
        Scene *scene = qobject_cast<Scene*>(f);
        ShowFunction *sf = track->createShowFunction(scene->id());
        sf->setStartTime(startTime);
        sf->setDuration(SceneItem::defaultDuration());
        m_showview->addScene(scene, track, sf);
        return sf;
    }

    ShowFunction *sf = track->createShowFunction(f->id());
    sf->setStartTime(startTime);

    switch (t)
    {
        case Function::ChaserType:
        case Function::SequenceType:
            m_showview->addSequence(qobject_cast<Chaser*>(f), track, sf);
        break;
        case Function::AudioType:
            m_showview->addAudio(qobject_cast<Audio*>(f), track, sf);
        break;
        case Function::RGBMatrixType:
            m_showview->addRGBMatrix(qobject_cast<RGBMatrix*>(f), track, sf);
        break;
        case Function::EFXType:
            m_showview->addEFX(qobject_cast<EFX*>(f), track, sf);
        break;
        case Function::VideoType:
            m_showview->addVideo(qobject_cast<Video*>(f), track, sf);
        break;
        case Function::CollectionType:
        {
            Collection *c = qobject_cast<Collection*>(f);
            if (sf->duration() == 0 && c != NULL && c->totalDuration() == 0)
                sf->setDuration(10000);
            m_showview->addCollection(c, track, sf);
        }
        break;
        default:
            track->removeShowFunction(sf); // unsupported on a timeline
            return NULL;
        break;
    }
    return sf;
}

void ShowManager::slotFunctionDropped(quint32 funcID, quint32 startTime, Track *track)
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    // Don't drop onto a locked track.
    if (track != NULL && track->isLocked())
        return;

    Function *f = m_doc->function(funcID);
    if (f == NULL)
        return;

    // Forbid dropping the show into itself (directly or transitively).
    if (f->id() == m_show->id() || f->contains(m_show->id()))
        return;

    pushUndoSnapshot();

    // No track under the drop => create a fresh track for it.
    if (track == NULL)
    {
        track = new Track();
        track->setName(tr("Track %1").arg(m_show->tracks().count() + 1));
        m_show->addTrack(track);
        m_showview->addTrack(track);
    }

    m_currentTrack = track;
    m_showview->activateTrack(track);
    ShowFunction *sf = addFunctionToTrack(f, track, startTime);
    if (sf != NULL)
        m_showview->resolveCollisions(track, sf); // DAW-style push

    m_doc->setModified();
    m_addSequenceAction->setEnabled(true);
    m_addAudioAction->setEnabled(true);
    m_addVideoAction->setEnabled(true);
    m_deleteAction->setEnabled(true);
    m_showview->updateViewSize();
}

void ShowManager::slotAddAtRequested(quint32 startTime, Track *track)
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    FunctionSelection fs(this, m_doc);
    QList<quint32> disabledList;
    foreach (Function *function, m_doc->functions())
    {
        if (function->contains(m_show->id()))
            disabledList << function->id();
    }
    fs.setDisabledFunctions(disabledList);
    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType |
                 Function::AudioType | Function::RGBMatrixType | Function::EFXType |
                 Function::CollectionType);
    fs.disableFilters(Function::ShowType | Function::ScriptType);

    if (fs.exec() != QDialog::Accepted)
        return;

    QList<quint32> ids = fs.selection();
    if (ids.isEmpty())
        return;

    slotFunctionDropped(ids.first(), startTime, track);
}

void ShowManager::slotShowLockedChanged(bool locked)
{
    m_showview->setEditable(locked == false);
}

void ShowManager::slotTrackColorChangeRequested(Track *track)
{
    if (track == NULL)
        return;
    QColor c = QColorDialog::getColor(track->color().isValid() ? track->color()
                                      : QColor(76, 98, 115), this, tr("Track Colour"));
    if (c.isValid() == false)
        return;
    track->setColor(c);
    m_doc->setModified();
    // Track::changed() -> TrackItem repaints; nothing else needed.
}

void ShowManager::slotMarkerAddRequested(quint32 time)
{
    if (m_show == NULL)
        return;
    bool ok = false;
    QString label = QInputDialog::getText(this, tr("Add Section Marker"),
                        tr("Label for this section of the show:"),
                        QLineEdit::Normal, QString(), &ok);
    if (ok == false || label.trimmed().isEmpty())
        return;

    // Default the section to run until the next marker, else 15 s.
    quint32 end = time + 15000;
    QMapIterator<quint32, ShowMarker> it(m_show->markers());
    while (it.hasNext())
    {
        it.next();
        if (it.key() > time && it.key() < end)
            end = it.key();
    }

    // Give new markers a colour cycled from a small palette.
    static const char *palette[] = { "#e0a820", "#4a90d9", "#5aa469", "#c0504d",
                                     "#8e6fb0", "#3fa8a0", "#d07030" };
    int idx = m_show->markers().count() % 7;
    pushUndoSnapshot();
    m_show->setMarker(time, end, label.trimmed(), QColor(palette[idx]));
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotMarkerEditRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    bool ok = false;
    QString label = QInputDialog::getText(this, tr("Rename Marker"),
                        tr("Marker label:"), QLineEdit::Normal, m.label, &ok);
    if (ok == false)
        return;
    pushUndoSnapshot();
    m_show->setMarker(time, m.end, label.trimmed(), m.color); // empty label removes it
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotMarkerRelabel(quint32 time, QString label)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    pushUndoSnapshot();
    // Empty label removes the marker (consistent with setMarker semantics).
    m_show->setMarker(time, m.end, label, m.color);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotMarkerColorRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    QColor c = QColorDialog::getColor(m.color.isValid() ? m.color : QColor("#e0a820"),
                                      this, tr("Marker Colour"));
    if (c.isValid() == false)
        return;
    pushUndoSnapshot();
    m_show->setMarker(time, m.end, m.label, c);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotMarkerDeleteRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    pushUndoSnapshot();
    m_show->removeMarker(time);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotMarkerMoved(quint32 oldStart, quint32 newStart, quint32 newEnd,
                                  QString label, QColor color)
{
    if (m_show == NULL || oldStart == UINT_MAX || label.isEmpty())
        return;
    pushUndoSnapshot();
    m_show->removeMarker(oldStart);
    m_show->setMarker(newStart, newEnd, label, color);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowManager::slotNewTrackRequested()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    pushUndoSnapshot();
    Track *track = new Track();
    track->setName(tr("Track %1").arg(m_show->tracks().count() + 1));
    m_show->addTrack(track);
    m_showview->addTrack(track);
    m_currentTrack = track;
    m_showview->activateTrack(track);
    m_doc->setModified();
    m_showview->updateViewSize();
}

void ShowManager::slotItemDroppedBelowTracks(ShowItem *item)
{
    if (item == NULL || m_show == NULL || m_doc->isShowLocked())
        return;

    if (QMessageBox::question(this, tr("New Track"),
            tr("Create a new track for \"%1\"?").arg(item->functionName()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        // Declined: rebuild so the item snaps back to its original track.
        updateMultiTrackView();
        return;
    }

    Track *track = new Track();
    track->setName(tr("Track %1").arg(m_show->tracks().count() + 1));
    m_show->addTrack(track);
    m_showview->addTrack(track);
    m_showview->moveItemToTrack(item, track);
    m_currentTrack = track;
    m_showview->activateTrack(track);
    m_doc->setModified();
    m_showview->updateViewSize();
}

/*********************************************************************
 * MIDI Time Code follow
 *********************************************************************/

void ShowManager::slotFollowMtcToggled(bool enable)
{
    if (m_show != NULL)
    {
        m_show->setTimecodeFollow(enable);

        // Turning follow OFF ends the follow session: otherwise the runner
        // reverts to normal playback and free-runs the cursor forward. Stop it
        // and park the playhead.
        if (enable == false && m_show->isRunning())
        {
            m_show->stop(functionParent());
            m_showview->stopPlayhead();
            m_playAction->setIcon(QIcon(":/player_play.png"));
        }
    }
    if (m_followMtcAction != NULL)
        m_followMtcAction->setText(enable ? tr("● FOLLOWING MTC")
                                          : tr("Follow MTC (off)"));

    // Keep global indicators (main-toolbar toggle, MIDI-mappable VC buttons) in
    // sync however the follow arming was changed.
    emit followTimecodeChanged(enable);
}

void ShowManager::updateTcSourceCombo()
{
    if (m_tcSourceCombo == NULL)
        return;

    m_tcSourceCombo->blockSignals(true);
    qint32 current = m_doc->timecodeSource()->sourceUniverse();
    m_tcSourceCombo->clear();
    m_tcSourceCombo->addItem(tr("Auto (any source)"), -1);

    QStringList names = m_doc->inputOutputMap()->universeNames();
    for (int i = 0; i < names.count(); i++)
    {
        // Only offer universes that actually have an input patched.
        if (m_doc->inputOutputMap()->inputPatch(i) != NULL)
            m_tcSourceCombo->addItem(names.at(i), i);
    }

    int idx = m_tcSourceCombo->findData(current);
    m_tcSourceCombo->setCurrentIndex(idx < 0 ? 0 : idx);
    m_tcSourceCombo->blockSignals(false);
}

void ShowManager::slotTcSourceChanged(int index)
{
    if (m_tcSourceCombo == NULL || index < 0)
        return;
    qint32 uni = m_tcSourceCombo->itemData(index).toInt();
    m_doc->timecodeSource()->setSourceUniverse(uni);
}

void ShowManager::slotTimecodePosition(quint32 msPosition)
{
    if (m_show == NULL)
        return;

    // Map the absolute timecode onto the (0-based) timeline via the offset.
    quint32 offset = m_show->timecodeOffset();
    // Auto-align to the SMPTE hour: Logic rolls from 01:00:00:00 by default, so
    // if no offset is set yet and timecode arrives hours-based, snap timeline 0
    // to that hour boundary. The user can still override via the TC 0… menu.
    if (offset == 0 && msPosition >= 3600000)
    {
        offset = (msPosition / 3600000) * 3600000;
        m_show->setTimecodeOffset(offset);
    }
    quint32 pos = (msPosition > offset) ? msPosition - offset : 0;

    if (m_show->isRunning())
    {
        // The runner emits timeChanged() every tick (50 Hz) and that already
        // drives the cursor via slotUpdateTimeAndCursor — feeding it here as
        // well would move the cursor twice per update (the "choppy" second
        // run). So only feed the runner; let it drive the (smooth) cursor.
        if (m_followMtcAction != NULL && m_followMtcAction->isChecked() &&
            m_show->isPaused() == false)
            m_show->setExternalTime(pos);
    }
    else
    {
        // Not playing — smooth-follow the cursor so the playhead still tracks.
        m_showview->setPlayheadTarget(pos);
        slotUpdateTime(pos);
    }
}

void ShowManager::slotTimecodeRunningChanged(bool running)
{
    // When timecode starts rolling and we are set to follow, run the show from
    // the current position so cues fire and the cursor chases Logic. When it
    // stops the runner simply holds its position (manual GO), so we don't stop.
    if (running && m_followMtcAction != NULL && m_followMtcAction->isChecked() &&
        m_show != NULL && m_show->isRunning() == false && showMayOutput())
    {
        quint32 offset = m_show->timecodeOffset();
        quint32 tc = m_doc->timecodeSource()->positionMs();
        quint32 pos = (tc > offset) ? tc - offset : 0;
        cursorMovedDuringPause = false;
        m_show->start(m_doc->masterTimer(), functionParent(), pos);
        m_playAction->setIcon(QIcon(":/player_pause.png"));
    }
}

void ShowManager::slotTimeDivisionTypeChanged(int idx)
{
    QVariant var = m_timeDivisionCombo->itemData(idx);
    if (var.isValid())
    {
        m_showview->setHeaderType((Show::TimeDivision)var.toInt());
        if (idx > 0)
            m_bpmField->setEnabled(true);
        else
            m_bpmField->setEnabled(false);
        if (m_show != NULL)
            m_show->setTimeDivision((Show::TimeDivision)var.toInt(), m_bpmField->value());
    }
}

void ShowManager::slotBPMValueChanged(int value)
{
    m_showview->setBPMValue(value);
    QVariant var = m_timeDivisionCombo->itemData(m_timeDivisionCombo->currentIndex());
    if (var.isValid() && m_show != NULL)
        m_show->setTimeDivision((Show::TimeDivision)var.toInt(), m_bpmField->value());
}

void ShowManager::slotViewClicked(QMouseEvent *event)
{
    Q_UNUSED(event)
    //qDebug() << Q_FUNC_INFO << "View clicked at pos: " << event->pos().x() << event->pos().y();
    showSceneEditor(NULL);
    hideRightEditor();
    m_colorAction->setEnabled(false);
    m_lockAction->setIcon(QIcon(":/lock.png"));
    m_lockAction->setEnabled(false);
    m_timingsAction->setEnabled(false);
    if (m_show != NULL && m_show->getTracksCount() == 0)
        m_deleteAction->setEnabled(false);
}

void ShowManager::slotShowItemMoved(ShowItem *item, quint32 time, bool moved)
{
    if (item == NULL)
        return;

    // Snapshot before a real reposition so Ctrl-Z can put it back.
    if (moved)
        pushUndoSnapshot();

    Q_UNUSED(time)

    quint32 fid = item->functionID();
    Function *f = m_doc->function(fid);
    if (f == NULL)
        return;

    // NOTE: clicking a timeline item no longer opens a Scene/Chaser editor on
    // the right. Building a full SceneEditor console (every fixture/channel) on
    // each click was slow enough to beachball on larger rigs, and content is
    // edited in the Programming / Functions tabs anyway — the timeline is for
    // arranging and timing. We just select the item + activate its track here.
    Sequence *sequence = qobject_cast<Sequence*>(f);
    if (sequence != NULL)
    {
        quint32 sceneID = sequence->boundSceneID();
        m_currentScene = qobject_cast<Scene*>(m_doc->function(sceneID));
        m_currentTrack = m_show->getTrackFromSceneID(sceneID);
        if (m_currentTrack == NULL)
            m_currentTrack = m_show->tracks().value(item->getTrackIndex(), NULL);
    }
    else
    {
        m_currentScene = NULL;
        m_currentTrack = m_show->tracks().value(item->getTrackIndex(), NULL);
    }
    if (m_currentTrack != NULL)
        m_showview->activateTrack(m_currentTrack);

    m_copyAction->setEnabled(true);
    m_deleteAction->setEnabled(true);
    m_colorAction->setEnabled(true);
    m_lockAction->setEnabled(true);
    if (item->isLocked() == false)
        m_lockAction->setIcon(QIcon(":/lock.png"));
    else
        m_lockAction->setIcon(QIcon(":/unlock.png"));
    m_timingsAction->setEnabled(true);

    if (moved == true)
        m_doc->setModified();
}

void ShowManager::slotUpdateTimeAndCursor(quint32 msec_time)
{
    // --- TEMP diagnostics: measure the real UI update interval + jitter. ---
    // Enable with env QLC_CURSOR_DEBUG=1. Logs every 50 updates: how often the
    // cursor is actually driven (expect ~20ms mean if the 50Hz engine updates
    // reach the UI cleanly; a high max = batching; ~80ms mean = 12Hz path).
    static const bool dbg = qEnvironmentVariableIntValue("QLC_CURSOR_DEBUG") != 0;
    if (dbg)
    {
        static QElapsedTimer et;
        static int n = 0;
        static qint64 sum = 0, mx = 0, prevMs = 0;
        static quint32 prevVal = 0;
        if (et.isValid())
        {
            qint64 e = et.nsecsElapsed() / 1000000;
            sum += e; if (e > mx) mx = e;
            if (++n % 50 == 0)
            {
                qWarning("[CURSOR] running=%d %d upd: interval mean %.1fms max %lldms; "
                         "value step ~%dms", m_show && m_show->isRunning(), n,
                         double(sum) / n, mx, int(msec_time) - int(prevVal));
                sum = 0; mx = 0; n = 0;
            }
        }
        prevVal = msec_time;
        prevMs = 0; (void)prevMs;
        et.restart();
    }

    slotUpdateTime(msec_time);
    m_showview->setPlayheadTarget(msec_time);
}

void ShowManager::slotUpdateTime(quint32 msec_time)
{
    uint h, m, s;

    h = msec_time / MS_PER_HOUR;
    msec_time -= (h * MS_PER_HOUR);

    m = msec_time / MS_PER_MINUTE;
    msec_time -= (m * MS_PER_MINUTE);

    s = msec_time / MS_PER_SECOND;
    msec_time -= (s * MS_PER_SECOND);

    QString str;
    if (m_show && m_show->isRunning())
    {
        str = QString("%1:%2:%3.%4").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'))
              .arg(s, 2, 10, QChar('0')).arg(msec_time / 100, 1, 10, QChar('0'));
    }
    else
        str = QString("%1:%2:%3.%4").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'))
              .arg(s, 2, 10, QChar('0')).arg(msec_time / 10, 2, 10, QChar('0'));

    m_timeLabel->setText(str);

    if (m_show != NULL && m_show->isPaused())
        cursorMovedDuringPause = true;
}

void ShowManager::slotTrackClicked(Track *track)
{
    m_currentTrack = track;
    if (track->getSceneID() == Function::invalidId())
        m_currentScene = NULL;
    else
    {
        Function *f = m_doc->function(track->getSceneID());
        if (f != NULL)
            m_currentScene = qobject_cast<Scene*>(f);
    }
    m_deleteAction->setEnabled(true);
    m_copyAction->setEnabled(true);
}

void ShowManager::slotTrackDoubleClicked(Track *track)
{
    bool ok;
    QString currentName = track->name();
    QString newTrackName = QInputDialog::getText(this, tr("Track name setup"),
                                         tr("Track name:"), QLineEdit::Normal,
                                         currentName, &ok);

    if (ok == true && newTrackName.isEmpty() == false)
    {
        track->setName(newTrackName);
        int idx = m_show->getAttributeIndex(track->name());
        m_show->renameAttribute(idx, track->name());
    }
}

void ShowManager::slotTrackMoved(Track *track, int direction)
{
    if (m_show != NULL && direction != 0)
    {
        pushUndoSnapshot();
        // direction is a signed row delta (±1 from the context menu, or several
        // rows from a header drag-reorder). moveTrack() swaps one row at a time.
        const int step = direction > 0 ? 1 : -1;
        for (int i = 0; i < qAbs(direction); i++)
            m_show->moveTrack(track, step);
    }
    updateMultiTrackView();
    m_doc->setModified();
}

void ShowManager::slotTrackDelete(Track *track)
{
    if (track == NULL || m_show == NULL || m_doc->isShowLocked())
        return;

    // Gut check — and if the track carries items, list them.
    QList<ShowFunction *> sfs = track->showFunctions();
    QString msg = tr("Delete track \"%1\"?").arg(track->name());
    if (sfs.isEmpty() == false)
    {
        msg += QString("\n\n") + tr("This will remove %1 item(s) from the timeline:")
                                 .arg(sfs.count()) + QString("\n");
        foreach (ShowFunction *sf, sfs)
        {
            Function *f = m_doc->function(sf->functionID());
            if (f != NULL)
                msg += QString("\n • ") + f->name();
        }
        msg += QString("\n\n") + tr("(The functions themselves are kept.)");
    }

    if (QMessageBox::question(this, tr("Delete Track"), msg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    pushUndoSnapshot();
    if (m_currentTrack == track)
        m_currentTrack = NULL;
    m_show->removeTrack(track->id());
    m_doc->setModified();
    updateMultiTrackView();
}

void ShowManager::slotChangeColor()
{
    ShowItem *item = m_showview->getSelectedItem();
    if (item != NULL)
    {
        QColor color = item->getColor();

        color = QColorDialog::getColor(color);
        if (!color.isValid())
            return;
        item->setColor(color);
        return;
    }
}

void ShowManager::slotChangeLock()
{
    ShowItem *item = m_showview->getSelectedItem();
    if (item != NULL)
    {
        if (item->isLocked() == false)
            m_lockAction->setIcon(QIcon(":/unlock.png"));
        else
            m_lockAction->setIcon(QIcon(":/lock.png"));
        item->setLocked(!item->isLocked());
    }
}

void ShowManager::slotShowTimingsTool()
{
    ShowItem *item = m_showview->getSelectedItem();

    if (item == NULL)
        return;

    TimingsTool *tt = new TimingsTool(item, this);

    Function *func = m_doc->function(item->functionID());
    if (func != NULL)
    {
        if (func->type() == Function::AudioType)
            tt->showDurationControls(false);
        if (func->type() == Function::RGBMatrixType || func->type() == Function::EFXType)
            tt->showDurationOptions(true);
    }

    connect(tt, SIGNAL(startTimeChanged(ShowItem*,int)),
            this, SLOT(slotShowItemStartTimeChanged(ShowItem*,int)));
    connect(tt, SIGNAL(durationChanged(ShowItem*,int,bool)),
            this, SLOT(slotShowItemDurationChanged(ShowItem*,int,bool)));
    tt->show();
}

void ShowManager::slotShowItemStartTimeChanged(ShowItem *item, int msec)
{
    if (item == NULL)
        return;

    if (item->isLocked() == false)
    {
        pushUndoSnapshot();
        item->setStartTime(msec);
        item->setPos(m_showview->getPositionFromTime(msec), item->y());
        m_doc->setModified();
    }
}

void ShowManager::slotShowItemDurationChanged(ShowItem *item, int msec, bool stretch)
{
    if (item == NULL)
        return;

    pushUndoSnapshot();
    item->setDuration(msec, stretch);
    m_doc->setModified();
}

void ShowManager::slotToggleSnapToGrid(bool enable)
{
    m_showview->setSnapToGrid(enable);
}

void ShowManager::slotChangeSize(int width, int height)
{
    if (m_showview != NULL)
        m_showview->setViewSize(width, height);
}

void ShowManager::slotStepSelectionChanged(int index)
{
    SequenceItem *seqItem = qobject_cast<SequenceItem*>(m_showview->getSelectedItem());
    if (seqItem != NULL)
        seqItem->setSelectedStep(index);
}

void ShowManager::slotDocClearing()
{
    m_showsCombo->clear();

    if (m_showview != NULL)
        m_showview->resetView();

    if (m_currentEditor != NULL)
    {
        m_vsplitter->widget(1)->layout()->removeWidget(m_currentEditor);
        delete m_currentEditor;
        m_currentEditor = NULL;
    }
    m_vsplitter->widget(1)->hide();

    if (m_sceneEditor != NULL)
    {
        emit functionManagerActive(false);
        m_splitter->widget(1)->layout()->removeWidget(m_sceneEditor);
        delete m_sceneEditor;
        m_sceneEditor = NULL;
    }
    m_splitter->widget(1)->hide();

    m_addTrackAction->setEnabled(false);
    m_addSequenceAction->setEnabled(false);
    m_addAudioAction->setEnabled(false);
    m_addVideoAction->setEnabled(false);
    m_copyAction->setEnabled(false);
    m_deleteAction->setEnabled(false);
    m_colorAction->setEnabled(false);
    m_timeLabel->setText("00:00:00.00");
}

void ShowManager::slotDocLoaded()
{
    m_show = NULL;
    m_currentScene = NULL;
    m_currentTrack = NULL;
    updateShowsCombo();
}

void ShowManager::slotFunctionRemoved(quint32 id)
{
    if (m_showsCombo->count() == 0)
        return;

    // check if ID is a Show
    for (int i = 0; i < m_showsCombo->count(); i++)
    {
        quint32 showID = m_showsCombo->itemData(i).toUInt();
        if (showID == id)
        {
            m_showsCombo->blockSignals(true);
            m_showsCombo->removeItem(i);

            if (i == m_selectedShowIndex)
            {
                m_show = NULL;
                m_selectedShowIndex = -1;
                updateMultiTrackView();
            }
            m_showsCombo->blockSignals(false);
            return;
        }
    }

    foreach (Function *function, m_doc->functionsByType(Function::ShowType))
    {
        Show *show = qobject_cast<Show*>(function);
        foreach (Track *track, show->tracks())
        {
            foreach (ShowFunction *sf, track->showFunctions())
            {
                if (sf->functionID() == id)
                    m_showview->deleteShowItem(track, sf);
            }

            // check if the Function being removed is a Scene bound to a Track
            if (track->getSceneID() == id)
                track->setSceneID(Function::invalidId());
        }
    }

    if (m_currentScene != NULL && m_currentScene->id() == id)
        m_currentScene = NULL;

    //if (isVisible())
    //    updateMultiTrackView();
}

void ShowManager::updateMultiTrackView()
{
    qDebug() << "[ShowManager] updateMultiTrackView...";
    m_showview->resetView();

    /* first of all get the ID of the selected Show */
    int idx = m_showsCombo->currentIndex();
    if (idx == -1)
        return;
    quint32 showID = m_showsCombo->itemData(idx).toUInt();

    m_show = qobject_cast<Show *>(m_doc->function(showID));
    if (m_show == NULL)
    {
        qDebug() << Q_FUNC_INFO << "Invalid show!";
        return;
    }

    // Reflect this show's timecode-follow state in the (now internal) action and
    // push it to the global main-toolbar toggle + any MIDI-mapped VC buttons.
    if (m_followMtcAction != NULL)
    {
        m_followMtcAction->blockSignals(true);
        m_followMtcAction->setChecked(m_show->timecodeFollow());
        m_followMtcAction->blockSignals(false);
    }
    emit followTimecodeChanged(m_show->timecodeFollow());
    // The active show changed — its running state defines timeline control.
    emit timelineControlChanged();
    updateTcSourceCombo();
    m_showview->setMarkers(m_show->markers());

    // disconnect BPM field and update the view manually, to
    // prevent m_show time division override
    disconnect(m_bpmField, SIGNAL(valueChanged(int)), this, SLOT(slotBPMValueChanged(int)));

    m_bpmField->setValue(m_show->timeDivisionBPM());
    m_showview->setBPMValue(m_show->timeDivisionBPM());
    int tIdx = m_timeDivisionCombo->findData(QVariant(m_show->timeDivisionType()));
    m_timeDivisionCombo->setCurrentIndex(tIdx);

    connect(m_bpmField, SIGNAL(valueChanged(int)), this, SLOT(slotBPMValueChanged(int)));
    // UniqueConnection so re-selecting the same show doesn't stack duplicate
    // connections (which would move the cursor N times per tick — choppy).
    connect(m_show, SIGNAL(timeChanged(quint32)), this, SLOT(slotUpdateTimeAndCursor(quint32)), Qt::UniqueConnection);
    connect(m_show, SIGNAL(showFinished()), this, SLOT(slotStopPlayback()), Qt::UniqueConnection);
    connect(m_show, SIGNAL(stopped(quint32)), this, SLOT(slotShowStopped()), Qt::UniqueConnection);
    // Refresh the global timeline-control indicator when the show starts (e.g.
    // via MTC auto-start, not just the Play button).
    connect(m_show, SIGNAL(running(quint32)), this, SLOT(slotShowRunningChanged()), Qt::UniqueConnection);

    Track *firstTrack = NULL;

    foreach (Track *track, m_show->tracks())
    {
        if (firstTrack == NULL)
            firstTrack = track;

        quint32 boundSceneID = track->getSceneID();
        if (boundSceneID != Function::invalidId())
        {
            Function *f = m_doc->function(boundSceneID);
            if (f == NULL || f->type() != Function::SceneType)
                track->setSceneID(Function::invalidId());
        }

        m_showview->addTrack(track);

        foreach (ShowFunction *sf, track->showFunctions())
        {
            Function *fn = m_doc->function(sf->functionID());
            if (fn != NULL)
            {
                if (fn->type() == Function::SceneType)
                {
                    Scene *scene = qobject_cast<Scene*>(fn);
                    m_showview->addScene(scene, track, sf);
                }
                else if (fn->type() == Function::ChaserType)
                {
                    Chaser *chaser = qobject_cast<Chaser*>(fn);
                    m_showview->addSequence(chaser, track, sf);
                }
                else if (fn->type() == Function::SequenceType)
                {
                    Sequence *sequence = qobject_cast<Sequence*>(fn);
                    m_showview->addSequence(sequence, track, sf);
                }
                else if (fn->type() == Function::AudioType)
                {
                    Audio *audio = qobject_cast<Audio*>(fn);
                    m_showview->addAudio(audio, track, sf);
                }
                else if (fn->type() == Function::RGBMatrixType)
                {
                    RGBMatrix *rgbm = qobject_cast<RGBMatrix*>(fn);
                    m_showview->addRGBMatrix(rgbm, track, sf);
                }
                else if (fn->type() == Function::EFXType)
                {
                    EFX *efx = qobject_cast<EFX*>(fn);
                    m_showview->addEFX(efx, track, sf);
                }
                else if (fn->type() == Function::VideoType)
                {
                    Video *video = qobject_cast<Video*>(fn);
                    m_showview->addVideo(video, track, sf);
                }
                else if (fn->type() == Function::CollectionType)
                {
                    Collection *collection = qobject_cast<Collection*>(fn);
                    m_showview->addCollection(collection, track, sf);
                }
            }
        }
    }
    /** Set first track active */
    if (firstTrack != NULL)
    {
        m_currentTrack = firstTrack;
        if (m_currentTrack->getSceneID() != Function::invalidId())
            m_currentScene = qobject_cast<Scene*>(m_doc->function(m_currentTrack->getSceneID()));
        m_showview->activateTrack(m_currentTrack);
        m_copyAction->setEnabled(true);
        m_addSequenceAction->setEnabled(true);
        m_addAudioAction->setEnabled(true);
        m_addVideoAction->setEnabled(true);
    }
    else
    {
        m_addSequenceAction->setEnabled(false);
        m_addAudioAction->setEnabled(false);
        m_addVideoAction->setEnabled(false);
        m_currentScene = NULL;
        showSceneEditor(NULL);
    }
    if (m_doc->clipboard()->hasFunction())
        m_pasteAction->setEnabled(true);

    // Empty-canvas hint: an existing show with no tracks yet.
    if (firstTrack == NULL)
        m_showview->setEmptyMessage(tr("Empty show — drop a scene, chaser or "
            "collection here, or right-click to add a track."));
    else
        m_showview->setEmptyMessage(QString());

    m_showview->updateViewSize();
}

bool ShowManager::checkOverlapping(quint32 startTime, quint32 duration)
{
    if (m_currentTrack == NULL)
        return false;

    foreach (ShowFunction *sf, m_currentTrack->showFunctions())
    {
        Function *func = m_doc->function(sf->functionID());
        if (func != NULL)
        {
            quint32 fst = sf->startTime();
            if ((startTime >= fst && startTime <= fst + sf->duration()) ||
                (fst >= startTime && fst <= startTime + duration))
            {
                return true;
            }
        }
    }

    return false;
}

void ShowManager::showEvent(QShowEvent* ev)
{
    qDebug() << Q_FUNC_INFO;
    emit functionManagerActive(true);
    QWidget::showEvent(ev);
    m_showview->show();
    m_showview->horizontalScrollBar()->setSliderPosition(0);
    m_showview->verticalScrollBar()->setSliderPosition(0);
    if (m_funcTree != NULL)
        m_funcTree->updateTree();
    updateShowsCombo();
}

void ShowManager::hideEvent(QHideEvent* ev)
{
    qDebug() << Q_FUNC_INFO;
    emit functionManagerActive(false);
    QWidget::hideEvent(ev);

    // Design mode: the active tab owns the rig. Leaving the Show tab releases
    // output so the show doesn't keep writing under (and fighting) whatever tab
    // is now active — e.g. the Programming-tab live preview. In Operate mode the
    // show runs globally and is left alone.
    if (m_doc->mode() == Doc::Design && m_show != NULL && m_show->isRunning())
        slotStopPlayback();

    if (m_currentEditor != NULL)
    {
        m_vsplitter->widget(1)->layout()->removeWidget(m_currentEditor);
        m_vsplitter->widget(1)->hide();
        delete m_currentEditor;
        m_currentEditor = NULL;
        m_editorFunctionID = Function::invalidId();
    }

    if (m_sceneEditor != NULL)
    {
        m_splitter->widget(1)->layout()->removeWidget(m_sceneEditor);
        m_splitter->widget(1)->hide();
        delete m_sceneEditor;
        m_sceneEditor = NULL;
    }

    ShowItem *item = m_showview->getSelectedItem();
    if (item != NULL)
        item->setSelected(false);
}

FunctionParent ShowManager::functionParent() const
{
    return FunctionParent::master();
}

bool ShowManager::showMayOutput() const
{
    // Operate mode: the show runs globally (VC-vs-timeline priority is handled
    // separately). Design mode: only the active tab owns the rig, so the show
    // may output only while the Show tab itself is visible.
    return m_doc->mode() == Doc::Operate || isVisible();
}
