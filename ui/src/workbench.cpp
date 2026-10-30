/*
  Q Light Controller Plus
  workbench.cpp

  Copyright (C) Branson Matheson

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

#include <QTreeWidgetItemIterator>
#include <QStackedWidget>
#include <QScrollArea>
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>

#include "workbench.h"
#include "blindstyle.h"
#include "shrinkablebar.h"
#include "workbenchstack.h"
#include "chaserwalker.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "fixturegroupeditor.h"
#include "functionselection.h"
#include "splitterhandles.h"
#include "scenegrouplooks.h"
#include "rgbmatrixeditor.h"
#include "collectioneditor.h"
#include "fixtureconsole.h"
#include "functionparent.h"
#include "showtreeview.h"
#include "effectdesigner.h"
#include "treecuestack.h"
#include "refinepanel.h"
#include "focusmenu.h"
#include "chasereditor.h"
#include "sceneeditor.h"
#include "sequence.h"
#include "scripteditor.h"
#include "audioeditor.h"
#include "videoeditor.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "monitor/monitor.h"
#include "showmanager/showtimelineeditor.h"
#include "lookeditor.h"
#include "efxeditor.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "fixture.h"
#include "chaser.h"
#include "script.h"
#include "audio.h"
#include "video.h"
#include "scene.h"
#include "show.h"
#include "efx.h"
#include "doc.h"

/* A sequence's editor, as the Functions window had it: its steps (the
   chaser editor) over its bound scene's channels (the scene editor); a click
   on a step puts that step's values on the scene, a fader moved writes into
   the step selected. Null: its bound scene is gone. */
static QWidget *sequenceEditor(QWidget *parent, Sequence *sequence, Doc *doc)
{
    Scene *bound = qobject_cast<Scene *>(doc->function(sequence->boundSceneID()));
    if (bound == nullptr)
        return nullptr;
    QSplitter *split = new QSplitter(Qt::Vertical, parent);
    split->setObjectName(QStringLiteral("workbenchSequenceEditor"));
    split->setChildrenCollapsible(false);
    ChaserEditor *steps = new ChaserEditor(split, sequence, doc);
    SceneEditor *channels = new SceneEditor(split, bound, doc, false);
    split->addWidget(steps);
    split->addWidget(channels);
    QObject::connect(steps, SIGNAL(applyValues(QList<SceneValue>&)),
                     channels, SLOT(slotSetSceneValues(QList<SceneValue>&)));
    QObject::connect(channels, SIGNAL(fixtureValueChanged(SceneValue,bool)),
                     steps, SLOT(slotUpdateCurrentStep(SceneValue,bool)));
    return split;
}

#define SETTINGS_SPLITTER QStringLiteral("programming/workbenchSplitter")

/** The Workbench plays what is loaded (the Design preview, the Operate run)
 *  under its own parent: hiding it lets go of that run only. A function a
 *  VC button, the Run tab or a cue was already running keeps running --
 *  Function::stop(master()) would clear every source. */
static FunctionParent workbenchParent()
{
    return FunctionParent::preview(0x7);
}

static QList<quint32> membersOf(Function *f)
{
    if (Collection *c = qobject_cast<Collection *>(f))
        return c->functions();
    return QList<quint32>();
}

Workbench::Workbench(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("workbench"));
    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setObjectName(QStringLiteral("workbenchSplitter"));
    // A handle stops at a pane's minimum: never snaps a pane shut.
    m_split->setChildrenCollapsible(false);
    root->addWidget(m_split);

    // --- The middle column: header, layers, add bar ---
    QWidget *column = new QWidget(m_split);
    column->setObjectName(QStringLiteral("workbenchColumn"));
    QVBoxLayout *col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(3);
    buildHeader(column);

    m_rows = new ProgrammingOutline(m_doc, column);
    m_rows->setStack(nullptr);
    const QString rowsWhat = tr("Its layers. Click a row to edit it; drag palettes, groups and "
                                "fixtures onto a layer, scenes onto the list.");
    m_rowsHint = new QLabel(rowsWhat, column);
    m_rowsHint->setObjectName(QStringLiteral("workbenchHint"));
    m_rowsHint->setWordWrap(true);
    m_rowsHint->setStyleSheet(QStringLiteral("QLabel { font-style: italic; }"));
    col->addWidget(m_rowsHint);
    col->addWidget(m_rows, 1);
    buildAddBar(column);
    m_split->addWidget(column);

    // --- The editor pane ---
    buildEditorPane();
    m_split->setStretchFactor(0, 3);
    m_split->setStretchFactor(1, 4);
    SplitterHandles::remember(m_split, SETTINGS_SPLITTER);

    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);
    m_restartTimer->setInterval(30);
    connect(m_restartTimer, &QTimer::timeout, this, [this]() {
        // Start again once the stop has landed (the timer thread stops a
        // function on its next tick).
        Function *f = m_doc->function(m_stack != nullptr ? m_stack->previewFunction() : Function::invalidId());
        if (f != nullptr && f->isRunning() && ++m_restartTries < 40)
        {
            m_restartTimer->start();
            return;
        }
        startPreview();
    });

    // The rows: their selection fills the editor pane.
    connect(m_rows, &QTreeWidget::itemSelectionChanged, this, &Workbench::slotRowsSelectionChanged);
    connect(m_rows, &ProgrammingOutline::scenesEdited, this, &Workbench::scenesEdited);
    connect(m_rows, &ProgrammingOutline::layersEdited, this, &Workbench::layersChanged);
    connect(m_rows, &ProgrammingOutline::openRequested, this, &Workbench::open);
    connect(m_rows, &ProgrammingOutline::editGroupLayoutRequested, this, &Workbench::showGroupLayout);
    connect(m_rows, &ProgrammingOutline::refineRequested, this,
            [this](quint32 scene, quint32 group, quint32 fixture) { openRefine(scene, group, fixture); });
    connect(m_rows, &ProgrammingOutline::revealPaletteRequested, this, &Workbench::revealPaletteRequested);
    connect(m_rows, &ProgrammingOutline::hintChanged, this, [this, rowsWhat](const QString &text) {
        m_rowsHint->setText(text.isEmpty() ? rowsWhat : text);
        m_rowsHint->setStyleSheet(text.isEmpty() ? QStringLiteral("QLabel { font-style: italic; }")
                                                 : QStringLiteral("QLabel { font-weight: bold; }"));
    });
    connect(m_rows, &ProgrammingOutline::pageRequested, this, [this]() {
        QWidget *page = m_page->currentWidget();
        page->setFocus(Qt::OtherFocusReason);
        if (QWidget *next = page->nextInFocusChain())
            if (page->isAncestorOf(next))
                next->setFocus(Qt::TabFocusReason);
    });

    // The show: what is loaded may go, change or be put back.
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
        /* The canvas, the look editor and a hosted editor hold a raw
           Function*. Doc says so BEFORE it deletes the function: let go now. */
        if (id == m_previewFunction)
            m_previewFunction = Function::invalidId();
        if (id == m_operateFunction)
            m_operateFunction = Function::invalidId();
        if (id == loadedFunction())
        {
            clear();
            return;
        }
        if (id == m_currentScene || id == m_editorFunction)
        {
            loadCanvas(Function::invalidId());
            clearEditors();
            m_editorFunction = Function::invalidId();
            showPage(OverviewPage);
        }
    });
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 id) {
        if (id != loadedFunction() && id != m_currentScene)
            return;
        QTimer::singleShot(0, this, [this]() {
            updateHeader();
            updateScenePage();
            if (m_page->currentWidget() == m_overviewPage)
                updateOverview();
            // A collection's layers changed (anywhere): play it as it now is.
            Function *root = m_doc->function(m_previewFunction);
            if (root != nullptr && membersOf(root) != m_previewMembers)
                restartPreview();
        });
    });
    connect(m_doc, &Doc::cleared, this, [this]() {
        // Ids are reused by the next show.
        m_previewFunction = m_operateFunction = m_trialScene = Function::invalidId();
        m_back.clear();
        clear();
    });
    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
    // RC8: a GO (from anywhere) takes the stage: what previews here stops
    // and holds until Play -- the run's values alone reach the rig.
    if (m_doc->showTree() != nullptr)
        connect(m_doc->showTree(), &ShowTreeController::stageTakenForRun, this, &Workbench::holdForRun);
    // Any step on the shared history (here, in the Show Tree): the scene as
    // it now is becomes the baseline, and everything shows as it now is.
    if (PaletteOps *ops = m_doc->paletteOps())
        connect(ops, &PaletteOps::changed, this, [this]() {
            recaptureBaseline();
            if (m_refreshQueued)
                return;
            m_refreshQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_refreshQueued = false;
                refreshAll();
            });
        });

    connect(m_lookEditor, SIGNAL(paletteChanged(quint32)), this, SLOT(slotLookEdited(quint32)));
    connect(m_lookEditor, &LookEditor::lookFocusChanged, this, &Workbench::currentPaletteIdChanged);
    connect(m_lookEditor, &LookEditor::lookFocusChanged, this, &Workbench::updateTrialPalette);
    // A live value (an effect's parameter): the preview only -- never the
    // rows, which would rebuild the editor's sliders under the drag.
    connect(m_lookEditor, &LookEditor::paletteValueChanged, this, [this](quint32 pid) {
        if (m_doc->effectScriptRunner())
            m_doc->effectScriptRunner()->updateEffectParams(pid);
        refreshPreview();
    });
    connect(m_lookEditor, &LookEditor::paletteRenamed, this, [this](quint32 id) {
        m_rows->scheduleRebuild();
        emit paletteEdited(id);
    });
    connect(m_fixtureConsole, SIGNAL(valueChanged(quint32,quint32,uchar)),
            this, SLOT(slotFixtureValueChanged(quint32,quint32,uchar)));
    connect(m_fixtureConsole, SIGNAL(checked(quint32,quint32,bool)),
            this, SLOT(slotFixtureChecked(quint32,quint32,bool)));

    updateHeader();
    updateAddBar();
    showPage(EmptyPage);
}

Workbench::~Workbench()
{
    // A page the back end lent goes back before it would go with us.
    releaseLentPane();
    stopPreview();
    stopOperate();
    // The rows hold the stack: let go of it before it goes.
    m_rows->setStack(nullptr);
}

/****************************************************************************
 * Building
 ****************************************************************************/

QList<quint32> Workbench::refineScenes() const
{
    QList<quint32> out;
    if (qobject_cast<FunctionStack *>(m_stack) == nullptr || !m_stack->readOnlyReason().isEmpty())
        return out;
    for (const WorkbenchStack::Layer &l : m_stack->layers())
        if (l.kind == WorkbenchStack::Layer::SceneLayer
                && qobject_cast<Scene *>(m_doc->function(l.functionId)) != nullptr)
            out << l.functionId;
    return out;
}

bool Workbench::openRefine(quint32 sceneId, quint32 groupId, quint32 fixtureId)
{
    if (TreeCueStack *tree = qobject_cast<TreeCueStack *>(m_stack))
    {
        const int at = selectedLayer();
        const quint32 layer = at >= 0 ? tree->layerIdAt(at) : 0xFFFFFFFF;
        return RefinePanel::open(m_doc, window(), tree->cueId(), layer) != nullptr;
    }
    const QList<quint32> scenes = refineScenes();
    if (scenes.isEmpty())
        return false;
    if (sceneId == 0xFFFFFFFF)
        sceneId = scenes.contains(m_currentScene) ? m_currentScene : scenes.first();
    return RefinePanel::openScene(m_doc, window(), scenes, sceneId, groupId, fixtureId) != nullptr;
}

void Workbench::fillFocusMenu(QMenu *m)
{
    m->clear();
    TreeCueStack *tree = qobject_cast<TreeCueStack *>(m_stack);
    QAction *self = m->addAction(tree != nullptr ? tr("Focus this cue\342\200\246") : tr("Focus this look\342\200\246"),
                                 this, [this]() { openRefine(); });
    self->setObjectName(QStringLiteral("workbenchRefine"));
    FocusMenu::setAvailable(self, FocusMenu::focusCueBlocked(m_doc),
                            m_doc->programmer()->padText(
                                tr("Move, zoom, focus and recolour this from the stage with the controller -- "
                                   "{LT}/{RT} step layers, {LB}/{RB} fixtures, {Y} the scope, {A} keeps")));
    if (tree == nullptr)
        return;
    const quint32 cueId = tree->cueId();
    const ShowTree::Cue *cue = m_doc->showTree()->cue(cueId);
    const ShowTree::CueList *list = cue != nullptr ? m_doc->showTree()->listOfCue(cueId) : nullptr;
    if (list == nullptr || list->looks)
        return;
    QAction *make = m->addAction(tr("Make this a venue focus cue"));
    make->setObjectName(QStringLiteral("workbenchFocusCue"));
    make->setCheckable(true);
    make->setChecked(cue->focusCall);
    FocusMenu::setAvailable(make, cue->focusCall ? QString() : FocusMenu::venueFocusCueBlocked(m_doc),
                            tr("Pre-show: GO onto this cue arms the show's focus calls; Start on the "
                               "controller begins them"));
    connect(make, &QAction::toggled, this, [this, cueId](bool on) {
        m_doc->showTree()->setCuesFocusCall({ cueId }, on);
    });
}

void Workbench::buildHeader(QWidget *column)
{
    QVBoxLayout *col = qobject_cast<QVBoxLayout *>(column->layout());
    QWidget *head = new QWidget(column);
    head->setObjectName(QStringLiteral("workbenchHeader"));
    m_headerLayout = new QVBoxLayout(head);
    m_headerLayout->setContentsMargins(4, 4, 4, 4);
    m_headerLayout->setSpacing(2);

    QHBoxLayout *crumbRow = new QHBoxLayout;
    m_backBtn = new QToolButton(head);
    m_backBtn->setObjectName(QStringLiteral("workbenchBack"));
    m_backBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_backBtn->setStyleSheet(QStringLiteral("QToolButton { color: #e0b44a; }"));
    connect(m_backBtn, &QToolButton::clicked, this, [this]() { goBack(); });
    crumbRow->addWidget(m_backBtn);
    m_crumb = new QLabel(head);
    m_crumb->setObjectName(QStringLiteral("workbenchCrumb"));
    m_crumb->setEnabled(false);       // greyed: where, not what
    crumbRow->addWidget(m_crumb, 1);
    m_headerLayout->addLayout(crumbRow);

    QHBoxLayout *titleRow = new QHBoxLayout;
    m_title = new QLabel(head);
    m_title->setObjectName(QStringLiteral("workbenchTitle"));
    m_title->setStyleSheet(QStringLiteral("QLabel { font-size: 15pt; font-weight: bold; }"));
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleRow->addWidget(m_title, 1);
    m_kind = new QLabel(head);
    m_kind->setObjectName(QStringLiteral("workbenchKind"));
    m_kind->setStyleSheet(QStringLiteral("QLabel { border: 1px solid palette(mid); border-radius: 3px; "
                                         "padding: 0 5px; font-size: 9pt; }"));
    titleRow->addWidget(m_kind);
    // A chaser: Send to Show Tree… (W5's Import in place, from here).
    m_sendToTreeBtn = new QToolButton(head);
    m_sendToTreeBtn->setObjectName(QStringLiteral("workbenchSendToTree"));
    m_sendToTreeBtn->setText(tr("Send to Show Tree…"));
    m_sendToTreeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_sendToTreeBtn->setToolTip(tr("Make this chaser a Show Tree cue list in place, checked to play the same; "
                                   "the chaser is filed under Imported/… and Restore original puts it back"));
    m_sendToTreeBtn->hide();
    connect(m_sendToTreeBtn, &QToolButton::clicked, this, [this]() {
        const quint32 id = loadedFunction();
        if (qobject_cast<Chaser *>(m_doc->function(id)) != nullptr)
            emit sendToShowTreeRequested(id);
    });
    titleRow->addWidget(m_sendToTreeBtn);
    /* Focus ▾ (board Q149; docs/design/FOCUS_REFINE_DESIGN.md 6B): what is loaded,
       from the stage, on the controller -- Focus this cue (a Show Tree cue,
       as the tree does) or Focus this look (a scene, as its own looks and
       exceptions); a cue can be made the venue focus cue here too. */
    m_refineBtn = new QToolButton(head);
    m_refineBtn->setObjectName(QStringLiteral("workbenchFocusMenu"));
    m_refineBtn->setText(tr("Focus"));
    m_refineBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_refineBtn->setPopupMode(QToolButton::InstantPopup);
    m_refineBtn->setToolTip(tr("Focus what is loaded from the stage, with the controller"));
    m_refineBtn->hide();
    {
        QMenu *m = new QMenu(m_refineBtn);
        m->setObjectName(QStringLiteral("workbenchFocusPopup"));
        m->setToolTipsVisible(true);
        connect(m, &QMenu::aboutToShow, this, [this, m]() { fillFocusMenu(m); });
        m_refineBtn->setMenu(m);
        fillFocusMenu(m);
    }
    titleRow->addWidget(m_refineBtn);
    m_headerLayout->addLayout(titleRow);

    m_info = new QLabel(head);
    m_info->setObjectName(QStringLiteral("workbenchInfo"));
    m_info->setWordWrap(true);
    m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_headerLayout->addWidget(m_info);

    // What is on the rig, with Stop / Play.
    QHBoxLayout *rigRow = new QHBoxLayout;
    m_onRig = new QLabel(head);
    m_onRig->setObjectName(QStringLiteral("workbenchOnRig"));
    rigRow->addWidget(m_onRig, 1);
    m_playBtn = new QToolButton(head);
    m_playBtn->setObjectName(QStringLiteral("workbenchPlay"));
    m_playBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_playBtn, &QToolButton::clicked, this, [this]() {
        if (m_ownPreview && m_stack != nullptr && m_stack->ownPreviewHeld())
        {
            // Play over a running list (RC8): the hold lets go.
            m_heldByRun = false;
            m_stack->releaseRunHold();
            m_stack->startOwnPreview();
        }
        else if (m_previewFunction != Function::invalidId() || m_ownPreview)
        {
            m_previewHeld = true;
            m_heldByRun = false;
            stopPreview();
        }
        else
        {
            m_previewHeld = false;
            if (m_heldByRun && m_stack != nullptr)
                m_stack->releaseRunHold();
            m_heldByRun = false;
            startPreview();
        }
        updateOnRig();
    });
    rigRow->addWidget(m_playBtn);
    // W3: a cue plays in context (what tracks in too) or Solo.
    m_soloBtn = new QToolButton(head);
    m_soloBtn->setObjectName(QStringLiteral("workbenchSolo"));
    m_soloBtn->setText(tr("Solo"));
    m_soloBtn->setCheckable(true);
    m_soloBtn->setToolTip(tr("Solo: only the selected layer plays. Off: the whole cue plays, in context "
                             "(what tracks in too), the selected layer highlighted"));
    m_soloBtn->hide();
    connect(m_soloBtn, &QToolButton::toggled, this, [this](bool on) { setSolo(on); });
    rigRow->addWidget(m_soloBtn);
    m_headerLayout->addLayout(rigRow);

    col->addWidget(head);
}

void Workbench::buildAddBar(QWidget *column)
{
    QVBoxLayout *col = qobject_cast<QVBoxLayout *>(column->layout());
    QWidget *bar = new QWidget(column);
    bar->setObjectName(QStringLiteral("workbenchAddBar"));
    QHBoxLayout *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(4);

    m_addSceneBtn = new QToolButton(bar);
    m_addSceneBtn->setObjectName(QStringLiteral("workbenchAddScene"));
    m_addSceneBtn->setText(tr("+ Scene"));
    m_addSceneBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *sceneMenu = new QMenu(m_addSceneBtn);
    sceneMenu->setToolTipsVisible(true);
    sceneMenu->addAction(tr("From the show…"), this, &Workbench::addExisting)
        ->setToolTip(tr("Pick scenes (or any function) already in the show"));
    sceneMenu->addAction(tr("New empty scene"), this, [this]() { addNew(Function::SceneType); })
        ->setToolTip(tr("A new scene, filed with this collection, as a layer"));
    m_addSceneBtn->setMenu(sceneMenu);
    connect(m_addSceneBtn, &QToolButton::clicked, this, &Workbench::addExisting);
    hl->addWidget(m_addSceneBtn);

    // One + Effect ▾ for the QLC+ effects, as Build ▸ Effects' + New ▾ (Q94).
    m_addEffectBtn = new QToolButton(bar);
    m_addEffectBtn->setObjectName(QStringLiteral("workbenchAddEffect"));
    m_addEffectBtn->setText(tr("+ Effect"));
    m_addEffectBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *effectMenu = new QMenu(m_addEffectBtn);
    effectMenu->setToolTipsVisible(true);
    m_addEfxBtn = effectMenu->addAction(QIcon(QStringLiteral(":/efx.png")), tr("EFX"));
    m_addEfxBtn->setObjectName(QStringLiteral("workbenchAddEfx"));
    connect(m_addEfxBtn, &QAction::triggered, this, [this]() { addNew(Function::EFXType); });
    m_addMatrixBtn = effectMenu->addAction(QIcon(QStringLiteral(":/rgbmatrix.png")), tr("RGB Matrix"));
    m_addMatrixBtn->setObjectName(QStringLiteral("workbenchAddMatrix"));
    connect(m_addMatrixBtn, &QAction::triggered, this, [this]() { addNew(Function::RGBMatrixType); });
    // An effect look (a sparkle, a wave, a ballyhoo...): the designer, made
    // a layer here -- beside the effects there, a movement and an
    // intensity effect together.
    effectMenu->addSeparator();
    QAction *designLook = effectMenu->addAction(tr("Effect look…"));
    designLook->setObjectName(QStringLiteral("workbenchDesignEffect"));
    designLook->setToolTip(tr("Design an effect look and make it a layer here, beside the effects "
                              "already there (a movement and an intensity effect together)"));
    connect(designLook, &QAction::triggered, this, [this]() { designEffect(EffectRequest()); });
    m_addEffectBtn->setMenu(effectMenu);
    hl->addWidget(m_addEffectBtn);
    hl->addStretch(1);

    auto tool = [bar, hl](const QString &text, const QString &name, const QString &tip) {
        QToolButton *b = new QToolButton(bar);
        b->setObjectName(name);
        b->setText(text);
        b->setToolTip(tip);
        hl->addWidget(b);
        return b;
    };
    m_upBtn = tool(QStringLiteral("▲"), QStringLiteral("workbenchUp"),
                   tr("Move the layer up (lower layers win where they share a channel, as last in a collection)"));
    connect(m_upBtn, &QToolButton::clicked, this, [this]() { moveSelectedLayer(-1); });
    m_downBtn = tool(QStringLiteral("▼"), QStringLiteral("workbenchDown"), tr("Move the layer down"));
    connect(m_downBtn, &QToolButton::clicked, this, [this]() { moveSelectedLayer(+1); });
    m_removeBtn = tool(tr("Remove"), QStringLiteral("workbenchRemove"),
                       tr("Take the layer out (⌫); the function itself stays in the show"));
    connect(m_removeBtn, &QToolButton::clicked, this, &Workbench::removeSelectedLayers);
    m_ownCopyBtn = tool(tr("Own copy"), QStringLiteral("workbenchOwnCopy"),
                        tr("The layer is shared (also in…): split it off as a copy of its own here, so "
                           "an edit changes only this one"));
    connect(m_ownCopyBtn, &QToolButton::clicked, this, &Workbench::makeOwnCopySelected);
    m_openBtn = tool(tr("▣ Open"), QStringLiteral("workbenchOpen"),
                     tr("Load the selected layer in the Workbench on its own (double-click); ↩ comes back"));
    connect(m_openBtn, &QToolButton::clicked, this, &Workbench::openSelected);

    // Scrolls sideways rather than holding Build wide (ShrinkableBar).
    col->addWidget(new ShrinkableBar(bar, column));
}

void Workbench::buildEditorPane()
{
    m_page = new QStackedWidget(m_split);
    m_page->setObjectName(QStringLiteral("programmingPage"));
    m_split->addWidget(m_page);

    // Nothing loaded / nothing selected: one line and a way in.
    m_emptyPage = new QWidget(m_page);
    {
        QVBoxLayout *l = new QVBoxLayout(m_emptyPage);
        l->addStretch();
        m_emptyHint = new QLabel(tr("Click a scene, collection or chaser on the left: it loads here."),
                                 m_emptyPage);
        m_emptyHint->setWordWrap(true);
        m_emptyHint->setAlignment(Qt::AlignCenter);
        l->addWidget(m_emptyHint);
        QHBoxLayout *bl = new QHBoxLayout;
        bl->addStretch();
        QPushButton *guide = new QPushButton(tr("How this works"), m_emptyPage);
        guide->setFlat(true);
        guide->setCursor(Qt::PointingHandCursor);
        connect(guide, &QPushButton::clicked, this, &Workbench::guideRequested);
        bl->addWidget(guide);
        bl->addStretch();
        l->addLayout(bl);
        l->addStretch();
    }
    m_page->addWidget(m_emptyPage);

    // What is loaded, as a whole: a collection's summary, a chaser's steps.
    m_overviewPage = new QWidget(m_page);
    m_overviewPage->setObjectName(QStringLiteral("workbenchOverview"));
    {
        m_overviewLayout = new QVBoxLayout(m_overviewPage);
        m_overviewLayout->setContentsMargins(4, 4, 4, 4);
        m_overviewHeading = new QLabel(m_overviewPage);
        m_overviewHeading->setStyleSheet(QStringLiteral("QLabel { font-size: 13pt; font-weight: bold; }"));
        m_overviewHeading->setWordWrap(true);
        m_overviewLayout->addWidget(m_overviewHeading);
        m_overviewText = new QLabel(m_overviewPage);
        m_overviewText->setObjectName(QStringLiteral("workbenchOverviewText"));
        m_overviewText->setWordWrap(true);
        m_overviewText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_overviewLayout->addWidget(m_overviewText);
        m_overviewLayout->addStretch(1);
    }
    m_page->addWidget(m_overviewPage);

    // A scene: what it is, what's wrong with it, what fires it, its timing.
    m_scenePage = new QWidget(m_page);
    m_scenePage->setObjectName(QStringLiteral("programmingScenePage"));
    {
        QVBoxLayout *sl = new QVBoxLayout(m_scenePage);
        sl->setContentsMargins(4, 4, 4, 4);
        m_sceneHeading = new QLabel(m_scenePage);
        m_sceneHeading->setObjectName(QStringLiteral("sceneHeading"));
        m_sceneHeading->setStyleSheet(QStringLiteral("QLabel { font-size: 13pt; font-weight: bold; }"));
        m_sceneHeading->setWordWrap(true);
        sl->addWidget(m_sceneHeading);
        m_sceneSummary = new QLabel(m_scenePage);
        m_sceneSummary->setObjectName(QStringLiteral("sceneSummary"));
        m_sceneSummary->setWordWrap(true);
        m_sceneSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sl->addWidget(m_sceneSummary);
        m_sceneWarnings = new QLabel(m_scenePage);
        m_sceneWarnings->setObjectName(QStringLiteral("sceneWarnings"));
        m_sceneWarnings->setWordWrap(true);
        m_sceneWarnings->setStyleSheet(QStringLiteral("QLabel { color: #d08a2a; }"));
        sl->addWidget(m_sceneWarnings);
        QLabel *timing = new QLabel(tr("Timing"), m_scenePage);
        timing->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; margin-top: 8px; }"));
        sl->addWidget(timing);
        m_sceneCanvasLayout = new QVBoxLayout;
        m_sceneCanvasLayout->setContentsMargins(0, 0, 0, 0);
        sl->addLayout(m_sceneCanvasLayout);
        QLabel *how = new QLabel(tr("Its targets and looks are the rows under it: drag palettes, groups "
                                    "and fixtures onto it, click a look to edit it."), m_scenePage);
        how->setWordWrap(true);
        how->setStyleSheet(QStringLiteral("QLabel { font-style: italic; margin-top: 8px; }"));
        sl->addWidget(how);
        sl->addStretch(1);
    }
    m_page->addWidget(m_scenePage);

    // A look: the Look Editor at full height; scrolls when the window is shorter.
    m_lookEditor = new LookEditor(m_doc, this);
    m_lookEditor->setObjectName(QStringLiteral("lookEditor"));
    // ↩ Back: a palette opened from a list goes back to what was showing.
    connect(m_lookEditor, &LookEditor::backRequested, this, [this]() { backOut(); });
    {
        QScrollArea *lookScroll = new QScrollArea(m_page);
        lookScroll->setObjectName(QStringLiteral("programmingLookPage"));
        lookScroll->setWidgetResizable(true);
        lookScroll->setFrameShape(QFrame::NoFrame);
        lookScroll->setWidget(m_lookEditor);
        m_lookPage = lookScroll;
    }
    m_page->addWidget(m_lookPage);

    // A target: which group or fixtures, and their channels.
    m_targetPage = new QWidget(m_page);
    m_targetPage->setObjectName(QStringLiteral("programmingTargetPage"));
    QVBoxLayout *targetCol = new QVBoxLayout(m_targetPage);
    targetCol->setContentsMargins(4, 4, 4, 4);
    {
        QHBoxLayout *hl = new QHBoxLayout;
        m_targetHeading = new QLabel(m_targetPage);
        m_targetHeading->setObjectName(QStringLiteral("targetHeading"));
        m_targetHeading->setWordWrap(true);
        hl->addWidget(m_targetHeading, 1);
        m_targetLayoutBtn = new QPushButton(tr("Edit layout"), m_targetPage);
        m_targetLayoutBtn->setToolTip(tr("The group's head layout (as in the Fixture Manager)"));
        connect(m_targetLayoutBtn, &QPushButton::clicked, this, [this]() { showGroupLayout(m_targetGroup); });
        hl->addWidget(m_targetLayoutBtn);
        targetCol->addLayout(hl);
    }
    /* The channel console: one fader per channel in a row that doesn't wrap,
       so a many-channel fixture would otherwise make its minimum width the
       window's. Ignored + a small floor keeps the scroll bar doing that job. */
    m_fixtureConsole = new FixtureConsole(this, m_doc);
    m_fixtureScroll = new QScrollArea(m_targetPage);
    m_fixtureScroll->setWidgetResizable(true);
    m_fixtureScroll->setFrameShape(QFrame::NoFrame);
    m_fixtureScroll->setMinimumHeight(200);
    m_fixtureScroll->setSizePolicy(QSizePolicy::Ignored, m_fixtureScroll->sizePolicy().verticalPolicy());
    m_fixtureScroll->setMinimumWidth(150);
    m_fixtureScroll->setWidget(m_fixtureConsole);
    targetCol->addWidget(m_fixtureScroll, 1);
    // Several fixed fixtures of different types: channel by index means nothing.
    m_fixtureMixedNote = new QLabel(m_targetPage);
    m_fixtureMixedNote->setWordWrap(true);
    m_fixtureMixedNote->setAlignment(Qt::AlignCenter);
    m_fixtureMixedNote->setMinimumHeight(120);
    m_fixtureMixedNote->setStyleSheet(QStringLiteral("QLabel { color: gray; }"));
    m_fixtureMixedNote->hide();
    targetCol->addWidget(m_fixtureMixedNote, 1);
    m_page->addWidget(m_targetPage);

    // A function's stock editor (EFX, RGB Matrix, chaser...), or a group's layout.
    m_functionPage = new QWidget(m_page);
    m_functionPage->setObjectName(QStringLiteral("workbenchFunctionPage"));
    m_functionLayout = new QVBoxLayout(m_functionPage);
    m_functionLayout->setContentsMargins(0, 0, 0, 0);
    {
        QHBoxLayout *hl = new QHBoxLayout;
        hl->setContentsMargins(4, 4, 4, 0);
        m_functionHeading = new QLabel(m_functionPage);
        m_functionHeading->setObjectName(QStringLiteral("workbenchFunctionHeading"));
        m_functionHeading->setWordWrap(true);
        hl->addWidget(m_functionHeading, 1);
        m_groupBackButton = new QPushButton(m_functionPage);
        m_groupBackButton->setObjectName(QStringLiteral("workbenchGroupBack"));
        m_groupBackButton->setToolTip(tr("Back to what was open before this group (Esc)"));
        m_groupBackButton->hide();
        connect(m_groupBackButton, &QPushButton::clicked, this, [this]() {
            const quint32 sid = m_groupReturnScene;
            if (m_doc->function(sid) == nullptr)
            {
                // No scene to go back to: the page the rows have selected.
                slotRowsSelectionChanged();
                return;
            }
            ProgrammingOutline::Row r;
            r.kind = ProgrammingOutline::SceneRow;
            r.scene = r.id = sid;
            if (m_rows->selectRow(r))
                slotRowsSelectionChanged();
            else
                load(sid);
        });
        hl->addWidget(m_groupBackButton);
        m_functionLayout->addLayout(hl);
    }
    m_page->addWidget(m_functionPage);
}

/****************************************************************************
 * Loading
 ****************************************************************************/

quint32 Workbench::loadedFunction() const
{
    return m_stack != nullptr ? m_stack->rootFunction() : Function::invalidId();
}

void Workbench::load(quint32 functionId)
{
    if (m_doc->function(functionId) == nullptr)
        return;
    setStackImpl(new FunctionStack(m_doc, functionId, this), Fresh);
}

void Workbench::open(quint32 functionId)
{
    if (m_doc->function(functionId) == nullptr || functionId == loadedFunction())
        return;
    setStackImpl(new FunctionStack(m_doc, functionId, this), Push);
}

void Workbench::setStack(WorkbenchStack *stack, bool keepHistory)
{
    setStackImpl(stack, keepHistory ? Push : Fresh);
}

void Workbench::setStackImpl(WorkbenchStack *stack, History history)
{
    if (history == Push && m_stack != nullptr)
    {
        Back b;
        b.label = m_stack->title();
        QTreeWidgetItem *cur = m_rows->currentItem();
        if (cur != nullptr && cur->isSelected())
            b.selectedKey = cur->data(0, ProgrammingOutline::KeyRole).toString();
        // What to reopen: a copy of what was loaded, opened afresh on the
        // way back (it may be gone by then).
        QSharedPointer<WorkbenchStack> proto(m_stack->reopen(nullptr));
        if (!proto.isNull())
        {
            b.make = [proto]() -> WorkbenchStack * { return proto->reopen(nullptr); };
            m_back << b;
        }
    }
    else if (history == Fresh)
        m_back.clear();

    stopPreview();
    stopOperate();
    m_restartTimer->stop();
    m_editorFunction = Function::invalidId();
    loadCanvas(Function::invalidId());
    clearEditors();
    m_lookEditor->setPalette(QLCPalette::invalidId());

    WorkbenchStack *old = m_stack;
    m_stack = stack;
    if (m_stack != nullptr)
    {
        m_stack->setParent(this);
        connect(m_stack, &WorkbenchStack::changed, this, [this]() {
            updateHeader();
            updateAddBar();
            // W3: a cue edited in the tree (its steps don't go through
            // PaletteOps): the rows as they now are, the Solo layer replayed.
            if (m_stack != nullptr && m_stack->takesDrops())
            {
                m_rows->scheduleRebuild();
                syncStackPreview();
            }
        });
        connect(m_stack, &WorkbenchStack::layersReplaced, this, &Workbench::stackLayersReplaced);
    }
    {
        QSignalBlocker block(m_rows);
        m_rows->setStack(m_stack);
    }
    if (old != nullptr)
        old->deleteLater();
    installStackWidget();
    // Held by a GO: loading something else keeps it off stage while the
    // show runs (RC8); otherwise what is loaded plays.
    m_heldByRun = m_heldByRun && m_doc->showTree() != nullptr && !m_doc->showTree()->listsRunning().isEmpty();
    m_previewHeld = m_heldByRun;
    updateHeader();

    // What to show first: a scene's own layer, a function's own editor; a
    // collection or chaser as a whole.
    Function *root = m_doc->function(loadedFunction());
    if (root != nullptr && root->type() != Function::CollectionType && root->type() != Function::ChaserType
        && root->type() != Function::SequenceType && m_rows->topLevelItemCount() > 0)
        selectLayer(0);
    else
    {
        m_rows->clearSelection();
        slotRowsSelectionChanged();
    }
    updateAddBar();
    startPreview();
    startOperate();
    updateOnRig();
    emit loaded(loadedFunction());
}

void Workbench::clear()
{
    m_back.clear();
    setStackImpl(nullptr, Fresh);
}

bool Workbench::backOut()
{
    // A palette opened from a list: the page the rows have selected again
    // (the look's edits are the palette's already).
    if (m_paletteDetour && currentPage() == LookPage)
    {
        slotRowsSelectionChanged();
        m_rows->setFocus(Qt::OtherFocusReason);
        return true;
    }
    // A group's layout: its ← Back.
    if (currentPage() == FunctionPage && qobject_cast<FixtureGroupEditor *>(m_funcEditor) != nullptr
        && !m_groupBackButton->isHidden())
    {
        m_groupBackButton->click();
        m_rows->setFocus(Qt::OtherFocusReason);
        return true;
    }
    // The effect bench: left as clicking away leaves it (its draft goes with
    // the designer, as its Cancel), back to the scene it was opened from --
    // asked first when what is designed hasn't been kept.
    if (m_benchPane && effectPane() != nullptr)
    {
        EffectDesigner *ed = qobject_cast<EffectDesigner *>(effectPane());
        if (ed != nullptr && ed->hasUnkeptChanges())
        {
            const QString title = tr("Effect designer");
            const QString text = tr("Leave the effect designer? This effect hasn't been kept.");
            bool leave = false;
            if (m_confirm)
                leave = m_confirm(title, text);
            else
            {
                QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, this);
                QPushButton *keep = box.addButton(tr("Keep designing"), QMessageBox::RejectRole);
                QPushButton *go = box.addButton(tr("Leave"), QMessageBox::DestructiveRole);
                box.setDefaultButton(keep);
                box.setEscapeButton(keep);
                box.exec();
                leave = box.clickedButton() == go;
            }
            if (!leave)
                return true;        // Esc was answered: still designing
        }
        const quint32 scene = m_benchFromScene;
        clear();
        if (m_doc->function(scene) != nullptr)
            load(scene);
        else if (m_returnTo)
            m_returnTo();
        m_rows->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return goBack();
}

void Workbench::keyPressEvent(QKeyEvent *ev)
{
    // Esc backs out one level; with nowhere to go it goes on (ignored).
    if (ev->key() == Qt::Key_Escape && ev->modifiers() == Qt::NoModifier && backOut())
    {
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}

bool Workbench::canGoBack() const
{
    return !m_back.isEmpty() || m_returnTo;
}

bool Workbench::goBack()
{
    while (!m_back.isEmpty())
    {
        const Back b = m_back.takeLast();
        WorkbenchStack *s = b.make ? b.make() : nullptr;
        if (s == nullptr)
            continue;
        setStackImpl(s, Pop);
        if (!b.selectedKey.isEmpty())
            for (QTreeWidgetItemIterator it(m_rows); *it; ++it)
                if ((*it)->data(0, ProgrammingOutline::KeyRole).toString() == b.selectedKey)
                {
                    m_rows->clearSelection();
                    m_rows->setCurrentItem(*it);
                    (*it)->setSelected(true);
                    m_rows->scrollToItem(*it);
                    break;
                }
        return true;
    }
    if (m_returnTo)
    {
        m_returnTo();
        return true;
    }
    return false;
}

void Workbench::setReturnTo(const QString &label, const std::function<void()> &go)
{
    m_returnLabel = label;
    m_returnTo = label.isEmpty() ? std::function<void()>() : go;
    updateHeader();
}

bool Workbench::selectRow(const ProgrammingOutline::Row &row)
{
    return m_rows->selectRow(row);
}

void Workbench::selectLayer(int index)
{
    QTreeWidgetItem *it = m_rows->layerItem(index);
    if (it == nullptr)
        return;
    m_rows->clearSelection();
    m_rows->setCurrentItem(it);
    it->setSelected(true);
}

/****************************************************************************
 * The header and the add bar
 ****************************************************************************/

void Workbench::updateHeader()
{
    // ↩ what was loaded before, or where the Workbench was opened from.
    if (!m_back.isEmpty())
    {
        m_backBtn->setText(tr("↩ %1").arg(m_back.last().label));
        m_backBtn->setToolTip(tr("Back to “%1”, as you left it").arg(m_back.last().label));
        m_backBtn->show();
    }
    else if (m_returnTo)
    {
        m_backBtn->setText(tr("↩ %1").arg(m_returnLabel));
        m_backBtn->setToolTip(tr("Back to %1").arg(m_returnLabel));
        m_backBtn->show();
    }
    else
        m_backBtn->hide();

    if (m_lockedBanner != nullptr)
    {
        m_lockedBanner->deleteLater();
        m_lockedBanner = nullptr;
    }
    {
        const quint32 id = m_stack == nullptr ? Function::invalidId() : loadedFunction();
        const Function *f = m_doc->function(id);
        m_sendToTreeBtn->setVisible(f != nullptr && f->type() == Function::ChaserType
                                    && m_doc->showTree()->whyNotSend(id).isEmpty());
        // An original Import in place kept for Restore original (Q141):
        // looked at, greyed, under the banner -- what plays is the list.
        m_page->setEnabled(m_doc->showTree()->originalFor(id) == Function::invalidId());
        // Refine edits what is loaded: not offered on a read-only stack
        // (an original, or one the Show Tree made) -- refineScenes() is empty.
        m_refineBtn->setVisible(qobject_cast<TreeCueStack *>(m_stack) != nullptr
                                || !refineScenes().isEmpty());
    }
    if (m_stack == nullptr)
    {
        m_crumb->setText(tr("Workbench"));
        m_title->setText(tr("Nothing loaded"));
        m_title->setStyleSheet(QStringLiteral("QLabel { font-size: 15pt; font-weight: bold; }"));
        m_kind->hide();
        m_info->setText(tr("Click a scene, collection or chaser on the left. Its layers come up here, "
                           "the selected row's editor on the right."));
        return;
    }
    const QString where = m_stack->where();
    m_crumb->setText(where.isEmpty() ? tr("Workbench") : where.split(QLatin1Char('/')).join(QStringLiteral(" › ")));
    m_title->setText(m_stack->title());
    {
        // A Show Tree cue's text colour, readable on the header.
        const QString hex = m_stack->titleColour();
        const QColor c = hex.isEmpty() ? QColor()
                         : ShowTree::readableOn(QColor(hex), m_title->palette().color(QPalette::Window));
        m_title->setStyleSheet(QStringLiteral("QLabel { font-size: 15pt; font-weight: bold;%1 }")
                               .arg(c.isValid() ? QStringLiteral(" color: %1;").arg(c.name()) : QString()));
    }
    m_kind->setText(m_stack->kindText().toUpper());
    m_kind->setVisible(!m_stack->kindText().isEmpty());
    m_info->setText(m_stack->infoLines().join(QLatin1Char('\n')));
    // Made by the Show Tree (D13): say so, with the way there.
    if (!m_stack->readOnlyReason().isEmpty() && loadedFunction() != Function::invalidId())
    {
        m_lockedBanner = ShowTreeView::lockedBanner(m_info->parentWidget(), m_doc, loadedFunction());
        m_headerLayout->addWidget(m_lockedBanner);
    }
    updateOnRig();
}

void Workbench::updateOnRig()
{
    Function *playing = m_doc->function(m_previewFunction);
    Function *live = m_doc->function(m_operateFunction);
    const bool canPlay = m_stack != nullptr && m_stack->previewFunction() != Function::invalidId();
    if (m_soloBtn != nullptr)
    {
        m_soloBtn->setVisible(m_stack != nullptr && m_stack->canSolo());
        QSignalBlocker block(m_soloBtn);
        m_soloBtn->setChecked(m_stack != nullptr && m_stack->solo());
    }
    if (m_ownPreview && m_stack != nullptr && m_stack->ownPreviewHeld())
    {
        // RC8: a list runs and GO took the stage -- Play previews over it.
        m_onRig->setText(m_stack->ownPreviewText());
        m_onRig->setStyleSheet(QString());
        m_playBtn->setText(tr("Play"));
        m_playBtn->setToolTip(tr("Preview this over the running show (the edit wins on the channels it plays)"));
        m_playBtn->setVisible(true);
        return;
    }
    if (m_ownPreview && m_stack != nullptr)
    {
        // W3: the back end says what plays (the Show Tree's words).
        const QString text = m_stack->ownPreviewText();
        m_onRig->setText(text.isEmpty() ? tr("▶ On the rig: %1").arg(m_stack->title()) : text);
        m_onRig->setStyleSheet(QStringLiteral("QLabel { color: #6cc08f; }"));
        m_playBtn->setText(tr("Stop"));
        m_playBtn->setToolTip(tr("Stop the preview (it plays again when you Play or load something)"));
        m_playBtn->setVisible(true);
        return;
    }
    if (playing != nullptr)
    {
        m_onRig->setText(tr("▶ On the rig: %1").arg(playing->name()));
        m_onRig->setStyleSheet(QStringLiteral("QLabel { color: #6cc08f; }"));
        m_playBtn->setText(tr("Stop"));
        m_playBtn->setToolTip(tr("Stop the preview (it plays again when you Play or load something)"));
        m_playBtn->setVisible(true);
    }
    else if (live != nullptr)
    {
        m_onRig->setText(tr("▶ Live: %1").arg(live->name()));
        m_onRig->setStyleSheet(QStringLiteral("QLabel { color: #6cc08f; }"));
        m_playBtn->setVisible(false);
    }
    else
    {
        m_onRig->setStyleSheet(QString());
        if (!canPlay)
            m_onRig->setText(QString());
        else if (m_doc->mode() != Doc::Design && m_stack->playsItself())
        {
            // Operate: only the run is live; a cue edited here is blind
            // (docs/design/LIVE_BLIND.md) -- blue, as Blind is everywhere.
            m_onRig->setText(tr("Blind: edits reach the stage on GO"));
            m_onRig->setStyleSheet(BlindStyle::labelSheet());
        }
        else if (m_doc->mode() != Doc::Design)
            m_onRig->setText(tr("Operate: nothing previews here"));
        else if (m_heldByRun)
            m_onRig->setText(tr("■ Held — GO took the stage. Play to preview over the show."));
        else
            m_onRig->setText(tr("■ Not playing"));
        m_playBtn->setText(tr("Play"));
        m_playBtn->setToolTip(m_heldByRun ? tr("Preview this over the running show")
                                          : tr("Play what is loaded on the rig (Design mode)"));
        m_playBtn->setVisible(canPlay && m_doc->mode() == Doc::Design);
    }
}

int Workbench::selectedLayer() const
{
    QTreeWidgetItem *cur = m_rows->currentItem();
    if (cur == nullptr || !cur->isSelected())
    {
        const QList<QTreeWidgetItem *> sel = m_rows->selectedItems();
        cur = sel.isEmpty() ? nullptr : sel.first();
    }
    return m_rows->layerOf(cur);
}

void Workbench::updateAddBar()
{
    const bool add = m_stack != nullptr && m_stack->canAddLayers();
    const QString why = m_stack == nullptr ? tr("Load a collection first")
                      : !m_stack->readOnlyReason().isEmpty() ? m_stack->readOnlyReason()
                      : tr("A scene is one layer: load a collection to add layers to it");
    const int layer = selectedLayer();
    const QList<WorkbenchStack::Layer> layers = m_stack != nullptr ? m_stack->layers()
                                                                   : QList<WorkbenchStack::Layer>();
    const bool hasGroups = layer >= 0 && !layerGroups(layer).isEmpty();

    m_addSceneBtn->setEnabled(add);
    m_addSceneBtn->setToolTip(add ? tr("Add a scene from the show as a layer (the menu: a new empty one)") : why);
    m_addEffectBtn->setEnabled(add);
    m_addEffectBtn->setToolTip(add ? tr("A new EFX or RGB Matrix, added as a layer") : why);
    m_addEfxBtn->setEnabled(add);
    m_addEfxBtn->setToolTip(!add ? why
                            : hasGroups ? tr("A new EFX on the selected layer's fixtures, added after it")
                                        : tr("A new EFX, added as a layer (select a layer with groups to "
                                             "make it on their fixtures)"));
    m_addMatrixBtn->setEnabled(add);
    m_addMatrixBtn->setToolTip(!add ? why
                               : hasGroups ? tr("A new RGB Matrix on the selected layer's first group, added after it")
                                           : tr("A new RGB Matrix, added as a layer (select a layer with a "
                                                "group to make it on that group)"));
    // W3: a layer carried into a cue isn't the cue's to move or take out.
    const bool tracked = layer >= 0 && layer < layers.size() && layers.at(layer).tracked;
    const bool reorder = m_stack != nullptr && m_stack->canReorderLayers() && layer >= 0 && !tracked;
    m_upBtn->setEnabled(reorder && layer > 0);
    m_downBtn->setEnabled(reorder && layer + 1 < layers.size());
    m_removeBtn->setEnabled(m_stack != nullptr && m_stack->canRemoveLayers() && layer >= 0 && !tracked);
    bool openable = false;
    if (layer >= 0 && layer < layers.size())
        openable = layers.at(layer).functionId != loadedFunction()
                   && layers.at(layer).kind != WorkbenchStack::Layer::CueLayer;
    m_openBtn->setEnabled(openable);
    m_ownCopyBtn->setEnabled(m_stack != nullptr && m_stack->canMakeOwnCopy(layer));
}

/****************************************************************************
 * Layers: add, remove, reorder, open
 ****************************************************************************/

QList<quint32> Workbench::layerGroups(int layer) const
{
    if (m_stack == nullptr)
        return QList<quint32>();
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    if (layer < 0 || layer >= layers.size())
        return QList<quint32>();
    Scene *s = qobject_cast<Scene *>(m_doc->function(layers.at(layer).functionId));
    return s != nullptr ? s->fixtureGroups() : QList<quint32>();
}

QList<GroupHead> Workbench::layerHeads(int layer) const
{
    QList<GroupHead> out;
    if (m_stack == nullptr)
        return out;
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    if (layer < 0 || layer >= layers.size())
        return out;
    Function *f = m_doc->function(layers.at(layer).functionId);
    if (Scene *s = qobject_cast<Scene *>(f))
    {
        for (quint32 gid : s->fixtureGroups())
            if (FixtureGroup *g = m_doc->fixtureGroup(gid))
                for (const GroupHead &h : g->headList())
                    if (!out.contains(h))
                        out << h;
        for (quint32 fx : s->fixtures())
            if (Fixture *fixture = m_doc->fixture(fx))
                for (int h = 0; h < qMax(1, fixture->heads()); ++h)
                    if (!out.contains(GroupHead(fx, h)))
                        out << GroupHead(fx, h);
    }
    return out;
}

void Workbench::addExisting()
{
    if (m_stack == nullptr || !m_stack->canAddLayers())
        return;
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(true);
    QList<quint32> disabled;
    if (loadedFunction() != Function::invalidId())
    {
        disabled << loadedFunction();
        for (const WorkbenchStack::Layer &l : m_stack->layers())
            disabled << l.functionId;
        for (Function *f : m_doc->functions())
            if (f != nullptr && f->contains(loadedFunction()))
                disabled << f->id();
    }
    fs.setDisabledFunctions(disabled);
    if (fs.exec() != QDialog::Accepted)
        return;
    const QList<quint32> ids = fs.selection();
    const int layer = selectedLayer();
    if (m_stack->addLayers(ids, layer >= 0 ? layer + 1 : -1))
        layersChanged();
}

quint32 Workbench::addNew(Function::Type type)
{
    if (m_stack == nullptr || !m_stack->canAddLayers())
        return Function::invalidId();
    const int layer = selectedLayer();
    const QList<quint32> groups = layerGroups(layer);
    const quint32 made = m_stack->addNew(type, layer, layerHeads(layer),
                                         groups.isEmpty() ? FixtureGroup::invalidId() : groups.first());
    if (made == Function::invalidId())
        return made;
    layersChanged();
    // The new layer, selected, its editor open.
    m_rows->rebuild();
    for (int i = 0; i < m_rows->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = m_rows->topLevelItem(i);
        if (it->data(0, ProgrammingOutline::IdRole).toUInt() == made)
        {
            selectLayer(m_rows->layerOf(it));
            break;
        }
    }
    return made;
}

QString Workbench::newHereLabel() const
{
    FunctionStack *fs = qobject_cast<FunctionStack *>(m_stack);
    if (fs == nullptr)
        return QString();
    if (Chaser *ch = fs->chaser())
    {
        if (!fs->readOnlyReason().isEmpty())
            return QString();
        return ch->stepsCount() == 0 ? tr("New Step in %1").arg(ch->name())
                                     : tr("New Step After %1 in %2").arg(fs->step() + 1).arg(ch->name());
    }
    Function *f = m_doc->function(fs->functionId());
    return f != nullptr && fs->canAddLayers() ? tr("New Scene in %1").arg(f->name()) : QString();
}

quint32 Workbench::newHere()
{
    FunctionStack *fs = qobject_cast<FunctionStack *>(m_stack);
    if (fs == nullptr || newHereLabel().isEmpty())
        return Function::invalidId();
    quint32 made = Function::invalidId();
    if (fs->chaser() != nullptr)
    {
        made = fs->addNewStep();
        // The step's scene, its one layer, selected.
        if (made != Function::invalidId() && m_rows->topLevelItemCount() > 0)
            selectLayer(0);
    }
    else
        made = addNew(Function::SceneType);
    if (made == Function::invalidId())
        return made;
    // Named in place, as F2 would.
    QTreeWidgetItem *cur = m_rows->currentItem();
    if (cur != nullptr && (cur->flags() & Qt::ItemIsEditable))
    {
        m_rows->setFocus(Qt::ShortcutFocusReason);
        m_rows->editItem(cur, 0);
    }
    return made;
}

void Workbench::removeSelectedLayers()
{
    QList<int> layers;
    for (QTreeWidgetItem *it : m_rows->selectedItems())
        if (it->parent() == nullptr && m_rows->layerOf(it) >= 0)
            layers << m_rows->layerOf(it);
    if (layers.isEmpty() && selectedLayer() >= 0)
        layers << selectedLayer();
    m_rows->removeLayers(layers);
}

void Workbench::moveSelectedLayer(int delta)
{
    const int layer = selectedLayer();
    if (m_stack == nullptr || layer < 0 || !m_stack->moveLayer(layer, layer + delta))
        return;
    m_rows->rebuild();
    selectLayer(layer + delta);
    layersChanged();
}

void Workbench::openSelected()
{
    const int layer = selectedLayer();
    if (m_stack == nullptr || layer < 0)
        return;
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    if (layer < layers.size())
        open(layers.at(layer).functionId);
}

void Workbench::layersChanged()
{
    updateHeader();
    updateAddBar();
    if (m_page->currentWidget() == m_overviewPage)
        updateOverview();
    // A collection starts its members when it starts: play it again.
    Function *root = m_doc->function(m_previewFunction);
    if (root != nullptr && membersOf(root) != m_previewMembers)
        restartPreview();
}

/****************************************************************************
 * The editor pane
 ****************************************************************************/

void Workbench::showPage(Page page)
{
    QWidget *w = page == ScenePage ? m_scenePage : page == LookPage ? m_lookPage
               : page == TargetPage ? m_targetPage : page == FunctionPage ? m_functionPage
               : page == OverviewPage && m_stack != nullptr ? m_overviewPage : m_emptyPage;
    if (w == m_overviewPage)
        updateOverview();
    // Whatever shows now is the rows' page (editPalette() sets it again).
    m_paletteDetour = false;
    m_lookEditor->setBackText(QString());
    m_page->setCurrentWidget(w);
    // An original Import in place kept for Restore original (Q141): looked
    // at, greyed, under the header's banner -- what plays is the list.
    m_page->setEnabled(m_stack == nullptr
                       || m_doc->showTree()->originalFor(loadedFunction()) == Function::invalidId());
    // The bench's designer has the width; the layers come back with anything else.
    m_split->widget(0)->setVisible(!(w == m_functionPage && m_benchPane && !m_effectPane.isNull()
                                     && m_funcEditor == m_effectPane));
    // The console's channels are drawn when it shows (a switch from one
    // fixture to another fires no showEvent).
    if (page == TargetPage && !m_fixtureScroll->isHidden())
        m_fixtureConsole->showChannels();
}

Workbench::Page Workbench::currentPage() const
{
    QWidget *w = m_page->currentWidget();
    return w == m_scenePage ? ScenePage : w == m_lookPage ? LookPage
         : w == m_targetPage ? TargetPage : w == m_functionPage ? FunctionPage
         : w == m_overviewPage ? OverviewPage : EmptyPage;
}

/* An editor going away later (deleteLater) still holds its Function*, and
   the Doc's signals still reach it until then. Doc::clearContents() deletes
   the functions and then removes the fixtures in one go: an EFX editor
   answered fixtureRemoved by reading its deleted EFX (heap corruption, seen
   on Linux). Cut it, and everything in it, off from the Doc now. */
static void retireEditor(QWidget *editor, Doc *doc)
{
    QObject::disconnect(doc, nullptr, editor, nullptr);
    for (QObject *child : editor->findChildren<QObject *>())
        QObject::disconnect(doc, nullptr, child, nullptr);
    editor->hide();
    editor->deleteLater();
}

void Workbench::updateOverview()
{
    if (m_overviewEditor != nullptr)
    {
        retireEditor(m_overviewEditor, m_doc);
        m_overviewEditor = nullptr;
    }
    if (m_stack == nullptr)
        return;
    m_overviewHeading->setText(m_stack->title());
    QStringList lines = m_stack->infoLines();
    Function *root = m_doc->function(loadedFunction());
    if (root != nullptr && root->type() == Function::CollectionType)
    {
        lines << QString()
              << tr("Its layers play together, in this order: where two set the same channel, "
                    "the lower one wins (the one added last).")
              << tr("Click a layer to edit it. + Scene adds one from the show; + EFX and + RGB Matrix make a "
                    "new one on the selected layer's groups. Drag scenes here from the left, palettes, "
                    "groups and fixtures onto a layer.");
    }
    else if (Sequence *seq = qobject_cast<Sequence *>(root))
    {
        lines << QString()
              << tr("A sequence: every step is its scene's channels at their own values. Click a step "
                    "to see its values below; move a fader to change the step selected.");
        if (QWidget *ed = sequenceEditor(m_overviewPage, seq, m_doc))
        {
            m_overviewEditor = ed;
            m_overviewLayout->insertWidget(m_overviewLayout->count() - 1, ed, 10);
        }
    }
    else if (Chaser *ch = qobject_cast<Chaser *>(root))
    {
        lines << QString()
              << tr("Walk its steps on the left (◀ ▶, or click one in the step table): each step's "
                    "layers come up under it, to work on as in a collection. Its run order, speeds "
                    "and timing modes, as the Chaser editor has them:");
        // The chaser's own step table; a cue list's is the Show Tree's (D13).
        ChaserEditor *ed = new ChaserEditor(m_overviewPage, ch, m_doc);
        ShowTreeController *st = m_doc->showTree();
        if (st != nullptr && st->isGenerated(ch->id()))
            ed->setLocked(ShowTreeView::lockedBanner(ed, m_doc, ch->id()));
        m_overviewEditor = ed;
        m_overviewLayout->insertWidget(m_overviewLayout->count() - 1, ed, 10);
    }
    m_overviewText->setText(lines.join(QLatin1Char('\n')));
}

void Workbench::slotRowsSelectionChanged()
{
    /* The editor pane follows the rows: a scene row edits the scene, a look
       row its palette (the Look Editor), a target row its channels, a ▣ row
       its function's stock editor. With several rows selected, the current
       one says which page; the others still drag, remove and move with it. */
    updateAddBar();
    QTreeWidgetItem *cur = m_rows->currentItem();
    if (cur == nullptr || !cur->isSelected())
    {
        const QList<QTreeWidgetItem *> sel = m_rows->selectedItems();
        cur = sel.isEmpty() ? nullptr : sel.first();
    }
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(cur);
    m_groupBackButton->hide();

    switch (row.kind)
    {
    case ProgrammingOutline::SceneRow:
        m_editorFunction = Function::invalidId();
        loadCanvas(row.id);
        m_lookEditor->setPalette(QLCPalette::invalidId());
        m_doc->setFocusedPalette(QLCPalette::invalidId());
        updateScenePage();
        showPage(ScenePage);
        break;
    case ProgrammingOutline::PaletteRow:
        m_editorFunction = Function::invalidId();
        loadCanvas(row.scene);
        m_lookEditor->setPalette(row.id);
        m_doc->setFocusedPalette(row.id);
        showPage(LookPage);
        break;
    case ProgrammingOutline::GroupRow:
    case ProgrammingOutline::FixtureRow:
    {
        m_editorFunction = Function::invalidId();
        loadCanvas(row.scene);
        // Every target row selected in this scene: groups by their members.
        QList<quint32> fixtures;
        QStringList names;
        m_targetGroup = row.kind == ProgrammingOutline::GroupRow ? row.id : UINT_MAX;
        for (const ProgrammingOutline::Row &r : m_rows->selectedRows())
        {
            if (r.scene != row.scene)
                continue;
            if (r.kind == ProgrammingOutline::GroupRow)
                if (FixtureGroup *g = m_doc->fixtureGroup(r.id))
                {
                    names << g->name();
                    for (quint32 fid : g->fixtureList())
                        if (!fixtures.contains(fid))
                            fixtures << fid;
                }
            if (r.kind == ProgrammingOutline::FixtureRow)
                if (Fixture *f = m_doc->fixture(r.id))
                {
                    names << f->name();
                    if (!fixtures.contains(r.id))
                        fixtures << r.id;
                }
        }
        m_targetHeading->setText(tr("<b>%1</b> — %n fixture(s) in this scene's targets. "
                                    "Their channels, as the scene sets them:", "", fixtures.size())
                                 .arg(names.join(QStringLiteral(", ")).toHtmlEscaped()));
        m_targetLayoutBtn->setVisible(m_targetGroup != UINT_MAX);
        m_selectedFixtures.clear();     // a real change of selection
        showFixtures(fixtures);
        showPage(TargetPage);
        break;
    }
    case ProgrammingOutline::FunctionRow:
        loadCanvas(Function::invalidId());
        loadFunctionEditor(row.id);
        if (m_stack != nullptr)
            m_stack->setFocusLayer(m_rows->layerOf(cur));
        break;
    case ProgrammingOutline::CueLayerRow:
    case ProgrammingOutline::AttributeRow:
        showStackRow(row);
        break;
    default:
    {
        // Nothing selected: a scene is still the scene; anything else, the whole.
        m_editorFunction = Function::invalidId();
        Function *root = m_doc->function(loadedFunction());
        if (qobject_cast<Scene *>(root) != nullptr)
        {
            loadCanvas(root->id());
            updateScenePage();
            showPage(ScenePage);
        }
        else
        {
            loadCanvas(Function::invalidId());
            clearEditors();
            // W3: a cue's own page (number, mode, fades, includes...).
            if (m_stack != nullptr)
                m_stack->setFocusLayer(-1);
            QWidget *pane = m_stack != nullptr ? m_stack->lendPane(-1, -1) : nullptr;
            if (pane != nullptr)
                hostLentPane(pane);
            else
                showPage(m_stack != nullptr ? OverviewPage : EmptyPage);
        }
        break;
    }
    }
}

void Workbench::updateScenePage()
{
    Scene *scene = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (scene == nullptr)
        return;
    m_sceneHeading->setText(scene->name());
    FunctionStack info(m_doc, scene->id());
    QStringList lines;
    lines << (scene->path(true).isEmpty() ? tr("Not in a folder") : tr("In %1").arg(scene->path(true)));
    lines << info.infoLines();
    m_sceneSummary->setText(lines.join(QLatin1Char('\n')));

    // The same notes the row's ⚠ counts, in full.
    QStringList warn;
    if (QTreeWidgetItem *it = m_rows->sceneItem(scene->id()))
        if (it->text(ProgrammingOutline::BadgeCol).contains(QStringLiteral("⚠")))
            for (const QString &l : it->toolTip(ProgrammingOutline::BadgeCol).split(QLatin1Char('\n')))
                if (!l.startsWith(tr("Fired by")) && !l.startsWith(tr("Sets ")) && !l.startsWith(tr("Has a transition"))
                    && !l.startsWith(tr("Its effects keep time")) && !l.startsWith(tr("Also in")))
                    warn << QStringLiteral("⚠ ") + l;
    m_sceneWarnings->setText(warn.join(QLatin1Char('\n')));
    m_sceneWarnings->setVisible(!warn.isEmpty());
}

void Workbench::loadCanvas(quint32 sceneId)
{
    if (sceneId == m_currentScene && (m_canvas != nullptr || sceneId == Function::invalidId())
        && m_funcEditor == nullptr)
        return;

    clearEditors();
    m_currentScene = sceneId;
    m_lookEditor->setPalette(QLCPalette::invalidId());
    if (Monitor::instance() != nullptr)
        Monitor::instance()->setActiveScene(sceneId);

    Scene *scene = qobject_cast<Scene *>(m_doc->function(sceneId));
    // It plays already (as what is loaded, or in it): its effects are synced.
    m_lastSyncedEffects = effectSignature(scene);
    if (scene == nullptr)
    {
        m_currentScene = Function::invalidId();
        m_doc->setFocusedScene(Function::invalidId());
        if (ProgrammerController *pc = m_doc->programmer())
            pc->seedStageAimFromScene(Function::invalidId());
        m_editBaseline.reset();
        return;
    }

    m_selectedFixtures.clear();
    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();
    m_lookEditor->setContextScene(scene);
    m_doc->setFocusedScene(sceneId);
    recaptureBaseline();     // edits in the pane record from here

    /* The scene's timing (fades, transitions, speed), on the scene page. Its
       Targets and Looks lists are the rows now, so they're kept hidden (the
       drop logic and the tests still reach them). */
    m_canvas = new SceneGroupLooks(scene, m_doc, m_scenePage, /*includeFixtureTargets*/ true);
    m_canvas->setListsVisible(false);
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    m_sceneCanvasLayout->addWidget(m_canvas);

    // The aim / pin from this scene's Aim target, if it has one.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->seedStageAimFromScene(sceneId);
}

void Workbench::clearEditors()
{
    releaseLentPane();
    if (m_canvas != nullptr)
    {
        retireEditor(m_canvas, m_doc);
        m_canvas = nullptr;
    }
    if (m_funcEditor != nullptr)
    {
        retireEditor(m_funcEditor, m_doc);
        m_funcEditor = nullptr;
    }
    // The look editor outlives the canvas: drop its Scene* too.
    m_lookEditor->setContextScene(nullptr);
    m_doc->setFocusedScene(Function::invalidId());
    m_doc->setFocusedPalette(QLCPalette::invalidId());
}

void Workbench::loadFunctionEditor(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (m_funcEditor != nullptr)
    {
        retireEditor(m_funcEditor, m_doc);
        m_funcEditor = nullptr;
    }
    m_editorFunction = fid;
    if (f == nullptr)
    {
        showPage(OverviewPage);
        return;
    }
    m_currentScene = Function::invalidId();
    m_editBaseline.reset();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(nullptr);

    ShowTreeController *st = m_doc->showTree();
    const bool generated = st != nullptr && st->isGenerated(fid);
    const bool isRoot = fid == loadedFunction();
    QWidget *ed = nullptr;
    QString note;
    switch (f->type())
    {
    case Function::EFXType:
        ed = new EFXEditor(m_functionPage, qobject_cast<EFX *>(f), m_doc);
        break;
    case Function::RGBMatrixType:
        ed = new RGBMatrixEditor(m_functionPage, qobject_cast<RGBMatrix *>(f), m_doc);
        break;
    case Function::ChaserType:
    case Function::SequenceType:
        if (f->type() == Function::ChaserType)
        {
            ChaserEditor *ce = new ChaserEditor(m_functionPage, qobject_cast<Chaser *>(f), m_doc);
            if (generated)
                ce->setLocked(ShowTreeView::lockedBanner(ce, m_doc, fid));
            ed = ce;
        }
        else if ((ed = sequenceEditor(m_functionPage, qobject_cast<Sequence *>(f), m_doc)) == nullptr)
            note = tr("A sequence whose scene is gone: it has nothing to play.");
        break;
    case Function::CollectionType:
        if (generated)
            ed = ShowTreeView::lockedBanner(m_functionPage, m_doc, fid);
        else
            note = tr("A collection: its own layers play together. ▣ Open it to work on them, ↩ to come back.");
        break;
    case Function::ShowType:
        ed = new ShowTimelineEditor(m_functionPage, qobject_cast<Show *>(f), m_doc);
        break;
    case Function::ScriptType:
        ed = new ScriptEditor(m_functionPage, qobject_cast<Script *>(f), m_doc);
        break;
    case Function::AudioType:
        ed = new AudioEditor(m_functionPage, qobject_cast<Audio *>(f), m_doc);
        break;
    case Function::VideoType:
        ed = new VideoEditor(m_functionPage, qobject_cast<Video *>(f), m_doc);
        break;
    case Function::SceneType:
        note = tr("A scene played as a step. ▣ Open it to work on it, ↩ to come back.");
        break;
    default:
        break;
    }
    QString heading = tr("<b>%1</b> — %2").arg(f->name().toHtmlEscaped(),
                                               Function::typeToString(f->type()).toHtmlEscaped());
    if (!isRoot && !note.isEmpty())
        heading += QStringLiteral("<br>") + note.toHtmlEscaped();
    else if (!isRoot)
        heading += QStringLiteral("<br><i>") + tr("Played as it is, in what is loaded.").toHtmlEscaped()
                   + QStringLiteral("</i>");
    m_functionHeading->setText(heading);
    if (ed == nullptr && !isRoot)
    {
        // Nothing to host: a way to open it.
        QWidget *w = new QWidget(m_functionPage);
        QVBoxLayout *l = new QVBoxLayout(w);
        QPushButton *openBtn = new QPushButton(tr("▣ Open “%1”").arg(f->name()), w);
        openBtn->setObjectName(QStringLiteral("workbenchOpenHere"));
        connect(openBtn, &QPushButton::clicked, this, [this, fid]() { open(fid); });
        l->addWidget(openBtn, 0, Qt::AlignLeft);
        l->addStretch(1);
        ed = w;
    }
    if (ed != nullptr)
    {
        m_funcEditor = ed;
        m_functionLayout->addWidget(ed, 1);
        ed->show();
    }
    showPage(FunctionPage);
}

void Workbench::showGroupLayout(quint32 groupId)
{
    FixtureGroup *grp = m_doc->fixtureGroup(groupId);
    if (grp == nullptr)
        return;
    // Back to the scene that was open (not another group: the scene before).
    if (m_currentScene != Function::invalidId())
        m_groupReturnScene = m_currentScene;
    Function *back = m_doc->function(m_groupReturnScene);

    clearEditors();
    m_currentScene = Function::invalidId();
    m_editorFunction = Function::invalidId();
    m_editBaseline.reset();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    if (Monitor::instance() != nullptr)
        Monitor::instance()->setActiveScene(Function::invalidId());
    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();

    m_funcEditor = new FixtureGroupEditor(grp, m_doc, m_functionPage);
    m_functionLayout->addWidget(m_funcEditor, 1);
    m_funcEditor->show();
    m_functionHeading->setText(tr("<b>Fixture group: %1</b> — %n head(s)", "", grp->headList().count())
                               .arg(grp->name().toHtmlEscaped()));
    showPage(FunctionPage);
    // ← Back (Esc): to the scene that was open, else to what the rows have.
    m_groupBackButton->setText(back != nullptr ? tr("← Back to %1").arg(back->name()) : tr("← Back"));
    m_groupBackButton->show();
}

void Workbench::editPalette(quint32 paletteId)
{
    if (m_doc->palette(paletteId) == nullptr)
        return;
    /* From a fixture group's layout: that fills the pane, and the look
       editor opened under it had no room. Go back to the scene first. */
    if (!m_groupBackButton->isHidden() && m_doc->function(m_groupReturnScene) != nullptr)
        m_groupBackButton->click();
    m_lookEditor->setPalette(paletteId);
    showPage(LookPage);
    // Over what was showing (from a list, not the look row selected):
    // ↩ Back, and Esc, go back to it.
    QTreeWidgetItem *cur = m_rows->currentItem();
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(cur);
    if (cur == nullptr || !cur->isSelected() || row.kind != ProgrammingOutline::PaletteRow
        || row.id != paletteId)
    {
        m_paletteDetour = true;
        m_lookEditor->setBackText(tr("↩ Back"));
    }
}

quint32 Workbench::currentPaletteId() const
{
    return m_lookEditor != nullptr ? m_lookEditor->paletteId() : QLCPalette::invalidId();
}

/****************************************************************************
 * Looks, targets, channels
 ****************************************************************************/

void Workbench::slotLookEdited(quint32 paletteId)
{
    // A look's value changed (a name too): the preview, the rows, the lists.
    refreshPreview();
    if (m_canvas != nullptr)
        m_canvas->reload();
    m_rows->scheduleRebuild();
    emit paletteEdited(paletteId);
    // A look's own fade is the scene's: one step on the history.
    recordSceneEdit(tr("edit a look's fade in %1").arg(m_doc->function(m_currentScene) != nullptr
                                                        ? m_doc->function(m_currentScene)->name() : QString()),
                    QStringLiteral("lookfade:%1").arg(paletteId));

    /* A palette isn't a Function: editing one is invisible to anything that
       waits for Doc::functionChanged() (the rig view, anything caching a
       resolved look). Every scene that uses it is told. */
    for (Function *f : m_doc->functions())
    {
        Scene *s = qobject_cast<Scene *>(f);
        if (s != nullptr && (s->palettes().contains(paletteId) || s->trialPalette() == paletteId))
            m_doc->notifyFunctionChanged(s->id());
    }
}

void Workbench::updateTrialPalette(quint32 paletteId)
{
    /* A palette only reaches the rig through a look that plays it, and a new
       one is in none. While it is being edited it is tried on the scene being
       edited: played over its own looks, never saved into it. */
    Scene *old = qobject_cast<Scene *>(m_doc->function(m_trialScene));
    if (old != nullptr)
    {
        old->setTrialPalette(QLCPalette::invalidId());
        m_doc->notifyFunctionChanged(old->id());
    }
    m_trialScene = Function::invalidId();

    QLCPalette *p = m_doc->palette(paletteId);
    // W3: a cue's value is in the cue already (nothing to try it on): say
    // whether it is shared, as the tree's Look Editor does.
    if (m_stack != nullptr && m_stack->takesDrops())
    {
        ShowTreeController *st = m_doc->showTree();
        m_lookEditor->setTrialNote(p == nullptr ? QString()
                                   : st != nullptr && st->isInlinePalette(paletteId)
                                     ? tr("A one-off value of this cue")
                                     : tr("A shared palette: changing it changes every cue and scene that uses it"));
        return;
    }
    Scene *scene = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    QString note;
    if (p != nullptr && p->type() != QLCPalette::Effect)
    {
        if (scene == nullptr)
            note = tr("Not in a look — open one and drag this onto it to see it on the rig.");
        else if (!scene->palettes().contains(paletteId))
        {
            scene->setTrialPalette(paletteId);
            m_trialScene = scene->id();
            m_doc->notifyFunctionChanged(scene->id());
            note = tr("Trying it on “%1” — not saved into it. Drag it onto the look to keep it.")
                       .arg(scene->name());
        }
    }
    m_lookEditor->setTrialNote(note);
    if ((old != nullptr && old->id() == m_currentScene) || m_trialScene != Function::invalidId())
        refreshPreview();
}

void Workbench::slotCanvasModified()
{
    // Dropping the palette being tried on INTO the look ends the trial.
    if (m_trialScene != Function::invalidId() || !m_lookEditor->trialNote().isEmpty())
        updateTrialPalette(m_lookEditor->paletteId());
    // One step on the shared history, from the baseline to now.
    recordSceneEdit(tr("edit %1").arg(m_doc->function(m_currentScene) != nullptr
                                      ? m_doc->function(m_currentScene)->name() : QString()));
    updateScenePage();
    refreshPreview();
    // A look added / removed / reordered changes what the console shows.
    if (!m_selectedFixtures.isEmpty())
        showFixtures(m_selectedFixtures);
}

void Workbench::scenesEdited(const QList<quint32> &sceneIds)
{
    if (!sceneIds.contains(m_currentScene))
        return;
    if (m_canvas != nullptr)
        m_canvas->reload();
    if (m_trialScene != Function::invalidId() || !m_lookEditor->trialNote().isEmpty())
        updateTrialPalette(m_lookEditor->paletteId());
    refreshPreview();
    updateScenePage();
}

void Workbench::sceneEditedOutside(const QString &label)
{
    if (m_canvas != nullptr)
        m_canvas->reload();
    m_rows->scheduleRebuild();
    slotCanvasModified();
    Q_UNUSED(label)
}

void Workbench::markEffectsSynced()
{
    m_lastSyncedEffects = effectSignature(qobject_cast<Scene *>(m_doc->function(m_currentScene)));
}

void Workbench::refreshAll()
{
    if (m_canvas != nullptr)
        m_canvas->reload();
    m_rows->rebuild();
    refreshPreview();
    updateScenePage();
    updateHeader();
    updateAddBar();
    if (m_page->currentWidget() == m_overviewPage)
        updateOverview();
    Function *root = m_doc->function(m_previewFunction);
    if (root != nullptr && membersOf(root) != m_previewMembers)
        restartPreview();
}

void Workbench::selectFixtures(const QList<quint32> &fixtureIds)
{
    showFixtures(fixtureIds);
}

bool Workbench::sameFixtureType(const Fixture *a, const Fixture *b) const
{
    if (a == nullptr || b == nullptr)
        return false;
    if (a == b)
        return true;
    // The same definition and mode: the same channels. Generic: by count.
    if (a->fixtureDef() != nullptr || b->fixtureDef() != nullptr)
        return a->fixtureDef() == b->fixtureDef() && a->fixtureMode() == b->fixtureMode();
    return a->channels() == b->channels();
}

void Workbench::showFixtures(const QList<quint32> &fixtureIds)
{
    /* A look is being edited and this is the same selection, re-announced
       after that very edit: keep the look editor (swapping in the faders
       threw the user out of the colour picker on every click). */
    if (currentPage() == LookPage && fixtureIds == m_selectedFixtures
        && m_currentScene == m_selectedFixturesScene)
        return;
    m_selectedFixtures = fixtureIds;
    m_selectedFixturesScene = m_currentScene;
    // Highlight, Flash, Park and the followspot steer what is selected.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->setProgrammerSelection(fixtureIds);
    if (Monitor::instance() != nullptr)
        Monitor::instance()->highlightFixtures(fixtureIds);
    emit fixturesSelected(fixtureIds);

    Scene *s = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (fixtureIds.isEmpty() || s == nullptr || m_doc->fixture(fixtureIds.first()) == nullptr)
    {
        m_fixtureScroll->hide();
        m_fixtureMixedNote->hide();
        if (currentPage() == TargetPage)
            showPage(s != nullptr ? ScenePage : OverviewPage);
        return;
    }

    // Edits mirror across the selected fixtures by channel index: only
    // meaningful when they share a type.
    const quint32 lead = fixtureIds.first();
    Fixture *leadFx = m_doc->fixture(lead);
    int mismatched = 0;
    for (quint32 fid : fixtureIds)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr || !sameFixtureType(leadFx, f))
            ++mismatched;
    }
    if (fixtureIds.size() > 1 && mismatched > 0)
    {
        m_fixtureScroll->hide();
        m_fixtureMixedNote->setText(
            tr("%n fixed fixture(s) of different types are selected.\n"
               "Select fixtures of the same type to edit their DMX channels together.",
               "", fixtureIds.size()));
        m_fixtureMixedNote->show();
        showPage(TargetPage);
        return;
    }

    /* The console edits the first as a template, mirrored to all. Prefilled
       as the scene outputs it: raw values first, then the looks over them
       (palettes win, as in Scene::writeDMX); look channels checked but
       greyed -- the palette drives them. Only the channels the scene drives
       are checked, so scenes layer on one fixture without clobbering. */
    m_fixtureConsole->blockSignals(true);
    m_fixtureConsole->setFixture(lead);
    m_fixtureConsole->setChecked(false);
    for (const SceneValue &scv : s->values())
        if (scv.fxi == lead)
            m_fixtureConsole->setSceneValue(scv);
    QList<quint32> lockedChannels;
    for (quint32 pid : s->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal == nullptr)
            continue;
        const int cOff = QLCPalette::colorSetOffset(m_doc, s->palettes(), pid);
        for (const SceneValue &scv : pal->valuesFromFixtureGroups(m_doc, s->fixtureGroups(), nullptr, cOff))
            if (scv.fxi == lead)
            {
                m_fixtureConsole->setSceneValue(scv);
                lockedChannels << scv.channel;
            }
        for (const SceneValue &scv : pal->valuesFromFixtures(m_doc, s->fixtures(), nullptr, cOff))
            if (scv.fxi == lead)
            {
                m_fixtureConsole->setSceneValue(scv);
                lockedChannels << scv.channel;
            }
    }
    for (quint32 ch : lockedChannels)
        m_fixtureConsole->setChannelEnabled(ch, false);
    m_fixtureConsole->blockSignals(false);

    m_fixtureMixedNote->hide();
    m_fixtureScroll->show();
    showPage(TargetPage);
    m_fixtureConsole->showChannels();
}

void Workbench::slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value)
{
    Q_UNUSED(fxi)
    Scene *s = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (s == nullptr)
        return;
    for (quint32 fid : m_selectedFixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != nullptr && ch < f->channels())
            s->setValue(fid, ch, value);
    }
    m_doc->setModified();
    refreshPreview();
    // A fader drag is one step.
    recordSceneEdit(tr("set channels in %1").arg(s->name()), QStringLiteral("channel:%1").arg(ch));
}

void Workbench::slotFixtureChecked(quint32 fxi, quint32 ch, bool state)
{
    Q_UNUSED(fxi)
    if (state) // enabling emits a value via valueChanged separately
        return;
    Scene *s = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (s == nullptr)
        return;
    for (quint32 fid : m_selectedFixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != nullptr && ch < f->channels())
            s->unsetValue(fid, ch);
    }
    m_doc->setModified();
    refreshPreview();
    recordSceneEdit(tr("release a channel in %1").arg(s->name()));
}

/****************************************************************************
 * The live preview
 ****************************************************************************/

QStringList Workbench::effectSignature(Scene *scene) const
{
    QStringList sig;
    if (scene == nullptr)
        return sig;
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == nullptr || p->type() != QLCPalette::Effect)
            continue;
        // id + script: a script swap must reload the instance; a param drag must not.
        // The WLED name too: unported WLED effects share one stand-in script.
        sig << QString::number(pid) + QLatin1Char('|') + p->scriptPath()
               + QLatin1Char('|') + p->effectPreset() + QLatin1Char('|') + p->wledEffect();
    }
    return sig;
}

void Workbench::startPreview()
{
    // What is loaded plays, in Design only: never over a running show.
    if (m_doc->mode() != Doc::Design || !isVisible() || m_previewHeld || m_stack == nullptr)
    {
        updateOnRig();
        return;
    }
    if (m_stack->playsItself())
    {
        // W3: the Show Tree's preview (the cue in context, or Solo).
        m_stack->startOwnPreview();
        m_ownPreview = true;
        updateOnRig();
        return;
    }
    const quint32 fid = m_stack->previewFunction();
    if (fid == m_previewFunction)
        return;
    if (m_previewFunction != Function::invalidId())
        stopPreview();
    Function *f = m_doc->function(fid);
    if (f == nullptr)
    {
        updateOnRig();
        return;
    }
    f->start(m_doc->masterTimer(), workbenchParent());
    m_previewFunction = fid;
    m_previewMembers = membersOf(f);
    m_lastSyncedEffects = effectSignature(qobject_cast<Scene *>(m_doc->function(m_currentScene)));
    updateOnRig();
}

void Workbench::stopPreview()
{
    if (m_ownPreview)
    {
        m_ownPreview = false;
        if (m_stack != nullptr)
            m_stack->stopOwnPreview();
        updateOnRig();
    }
    if (m_previewFunction == Function::invalidId())
        return;
    Function *f = m_doc->function(m_previewFunction);
    if (f != nullptr)
        f->stop(workbenchParent());
    m_previewFunction = Function::invalidId();
    m_previewMembers.clear();
    updateOnRig();
}

void Workbench::restartPreview()
{
    if (m_previewFunction == Function::invalidId())
        return;
    stopPreview();
    m_restartTries = 0;
    m_restartTimer->start();
}

void Workbench::refreshPreview()
{
    // An edit, applied WITHOUT a stop/start (rapid start/stop on the timer
    // thread races): reset the scene's runtime so the next write re-reads it.
    if (m_doc->mode() != Doc::Design || !isVisible() || m_previewHeld || m_stack == nullptr
        || m_ownPreview)
        return;
    Function *root = m_doc->function(m_stack->previewFunction());
    if (root == nullptr)
        return;
    if (!root->isRunning() && !m_restartTimer->isActive())
    {
        // It stopped on its own (a scene empty until now): start it.
        root->start(m_doc->masterTimer(), workbenchParent());
        m_previewFunction = root->id();
        m_previewMembers = membersOf(root);
        m_lastSyncedEffects = effectSignature(qobject_cast<Scene *>(m_doc->function(m_currentScene)));
        updateOnRig();
        return;
    }
    Scene *s = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (s != nullptr && s->isRunning())
    {
        s->resetRuntime();
        /* Effects are made again only when the set of effect looks changed:
           anything else must not reset a running effect's state. */
        if (m_doc->effectScriptRunner())
        {
            const QStringList sig = effectSignature(s);
            if (sig != m_lastSyncedEffects)
            {
                m_lastSyncedEffects = sig;
                m_doc->effectScriptRunner()->syncScene(s->id());
            }
        }
    }
}

void Workbench::startOperate()
{
    /* Operate: the preview can't run, so what is loaded is started directly,
       all channels live; a followspot effect look steers pan/tilt over it.
       No isRunning() guard: a stop lands on the next tick. */
    if (m_doc->mode() != Doc::Operate || !isVisible() || m_stack == nullptr)
        return;
    // A cue edited in Live is blind (the tree's rule): nothing started here.
    if (m_stack->playsItself())
        return;
    const quint32 fid = m_stack->previewFunction();
    if (m_operateFunction == fid)
        return;
    Function *f = m_doc->function(fid);
    if (f == nullptr)
        return;
    f->start(m_doc->masterTimer(), workbenchParent());
    m_operateFunction = fid;
    updateOnRig();
}

void Workbench::stopOperate()
{
    if (m_operateFunction == Function::invalidId())
        return;
    if (Function *f = m_doc->function(m_operateFunction))
        f->stop(workbenchParent());
    m_operateFunction = Function::invalidId();
    updateOnRig();
}

void Workbench::slotModeChanged()
{
    if (m_doc->mode() == Doc::Design)
    {
        /* Operate -> Design: the app stopped every function before the mode
           changed, so the operate run is already gone. Forget it and start a
           fresh preview (a start only: no start/stop race). */
        m_operateFunction = Function::invalidId();
        m_previewFunction = Function::invalidId();
        startPreview();
    }
    else
    {
        // Design -> Operate: the same run carries on as the live one (a
        // cue's own preview stops: Live is blind in the tree).
        if (m_ownPreview)
            stopPreview();
        else if (m_previewFunction != Function::invalidId())
        {
            m_operateFunction = m_previewFunction;
            m_previewFunction = Function::invalidId();
        }
        else
            startOperate();
    }
    updateOnRig();
}

void Workbench::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    startPreview();
    startOperate();
}

void Workbench::holdForRun(QStringList *held)
{
    // Only what previews here: an Operate run (startOperate) is the
    // operator's, not a preview.
    if (m_previewFunction == Function::invalidId() && !m_ownPreview)
        return;
    if (held != nullptr)
    {
        if (!m_ownPreview)
            held->append(tr("the Workbench preview"));
        else if (m_stack != nullptr && m_stack->solo())
            held->append(tr("the Solo preview"));
        // In context, it is the Show Tree's preview: said already.
    }
    m_previewHeld = true;
    m_heldByRun = true;
    stopPreview();
    updateOnRig();
}

void Workbench::playPicked()
{
    if (m_doc->mode() != Doc::Design || !m_heldByRun)
        return;
    m_previewHeld = false;
    m_heldByRun = false;
    if (m_stack != nullptr)
        m_stack->releaseRunHold();
    startPreview();
    updateOnRig();
}

void Workbench::hideEvent(QHideEvent *ev)
{
    // Leaving the tab (or the dock): nothing left playing from here.
    stopPreview();
    stopOperate();
    QWidget::hideEvent(ev);
}

/****************************************************************************
 * The shared history
 ****************************************************************************/

void Workbench::recaptureBaseline()
{
    m_editBaseline = m_currentScene != Function::invalidId() && m_doc->paletteOps() != nullptr
                     ? m_doc->paletteOps()->sceneSnapshot(m_currentScene)
                     : QSharedPointer<Scene>();
}

void Workbench::recordSceneEdit(const QString &label, const QString &mergeKey)
{
    PaletteOps *ops = m_doc->paletteOps();
    if (ops == nullptr || m_editBaseline.isNull() || m_editBaseline->id() != m_currentScene)
        return;
    // A step (or nothing changed): either way the scene as it is now is the
    // baseline for the next edit (PaletteOps::changed recaptures it too).
    if (!ops->recordSceneStep(label, { m_editBaseline }, mergeKey))
        recaptureBaseline();
}

/****************************************************************************
 * W3: a Show Tree cue loaded (TreeCueStack); W4's plug in the editor pane
 ****************************************************************************/

Workbench::EffectPaneFactory Workbench::s_effectPane;

void Workbench::setEffectPaneFactory(const EffectPaneFactory &factory)
{
    s_effectPane = factory;
}

void Workbench::hostLentPane(QWidget *pane)
{
    if (pane == nullptr)
        return;
    if (m_lentPane != pane)
    {
        releaseLentPane();
        m_lentPane = pane;
        m_functionLayout->addWidget(pane, 1);
    }
    m_functionHeading->clear();
    m_functionHeading->hide();
    pane->show();
    showPage(FunctionPage);
}

void Workbench::releaseLentPane()
{
    if (m_lentPane.isNull())
        return;
    QWidget *pane = m_lentPane;
    m_lentPane.clear();
    m_functionLayout->removeWidget(pane);
    m_functionHeading->show();
    if (m_stack != nullptr)
        m_stack->returnPane(pane);
    else
        pane->hide();
}

void Workbench::showStackRow(const ProgrammingOutline::Row &row)
{
    if (m_stack == nullptr)
        return;
    const int layer = row.kind == ProgrammingOutline::CueLayerRow ? int(row.id) : int(row.scene);
    const int attr = row.kind == ProgrammingOutline::AttributeRow ? int(row.id) : -1;
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    if (layer < 0 || layer >= layers.size())
        return;
    m_editorFunction = Function::invalidId();
    loadCanvas(Function::invalidId());
    clearEditors();
    m_stack->setFocusLayer(layer);

    // The selected layer, highlighted on the 2D view; Flash / Park follow.
    const QList<quint32> fixtures = m_stack->layerFixtures(layer);
    m_selectedFixtures = fixtures;
    m_selectedFixturesScene = UINT_MAX;
    if (ProgrammerController *pc = m_doc->programmer())
        pc->setProgrammerSelection(fixtures);
    if (Monitor::instance() != nullptr)
        Monitor::instance()->highlightFixtures(fixtures);
    emit fixturesSelected(fixtures);

    const WorkbenchStack::Layer &l = layers.at(layer);
    if (attr >= 0 && attr < l.attributes.size())
    {
        const WorkbenchStack::Layer::Attribute &a = l.attributes.at(attr);
        // An effect: W4's designer, when it is plugged in.
        if (a.key.startsWith(QLatin1String("Effect.")) && s_effectPane && designEffect())
            return;
        // A value with a palette: the Look Editor, at full height, tried on
        // the layer's own fixtures. Changing it changes every use (the
        // Look Editor says so for a shared one).
        if (m_doc->palette(a.palette) != nullptr)
        {
            Scene *ctx = qobject_cast<Scene *>(m_doc->function(m_stack->layerScene(layer)));
            m_lookEditor->setContextScene(ctx);
            m_lookEditor->setPalette(a.palette);
            m_doc->setFocusedPalette(a.palette);
            showPage(LookPage);
            return;
        }
    }
    // The layer (or a value without a palette): the back end's page for it
    // -- the Show Tree's layer page, every attribute with its ⋯.
    if (QWidget *pane = m_stack->lendPane(layer, attr))
        hostLentPane(pane);
    else
        showPage(OverviewPage);
}

bool Workbench::designEffect()
{
    if (!s_effectPane || m_stack == nullptr || !m_stack->takesDrops())
        return false;
    QTreeWidgetItem *cur = m_rows->currentItem();
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(cur);
    if (row.kind != ProgrammingOutline::CueLayerRow && row.kind != ProgrammingOutline::AttributeRow)
        return false;
    const int layer = m_rows->layerOf(cur);
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    if (layer < 0 || layer >= layers.size())
        return false;
    EffectRequest req;
    req.cueId = layers.at(layer).cueId;
    req.layerId = layers.at(layer).layerId;
    req.targetSignature = layers.at(layer).targetSignature;
    if (row.kind == ProgrammingOutline::AttributeRow && int(row.id) < layers.at(layer).attributes.size())
    {
        req.key = layers.at(layer).attributes.at(int(row.id)).key;
        req.palette = layers.at(layer).attributes.at(int(row.id)).palette;
    }
    QWidget *w = s_effectPane(this, req, m_functionPage);
    if (w == nullptr)
        return false;
    hostEffectPane(w, tr("<b>Design effect</b> — %1").arg(layers.at(layer).label.toHtmlEscaped()), false);
    return true;
}

bool Workbench::designEffect(const EffectRequest &request)
{
    if (request.cueId != 0xFFFFFFFF)
    {
        // A cue layer's: the one above, on the layer selected.
        return designEffect();
    }
    if (!s_effectPane)
        return false;
    // Where it was opened from (Make a layer goes there first): what is
    // loaded, or the bench's own when it shows already.
    EffectRequest req = request;
    if (req.fromCue == 0xFFFFFFFF && req.fromScene == 0xFFFFFFFF)
    {
        if (m_benchPane && effectPane() != nullptr)
        {
            req.fromCue = m_benchFromCue;
            req.fromScene = m_benchFromScene;
        }
        else if (TreeCueStack *cs = qobject_cast<TreeCueStack *>(m_stack))
        {
            req.fromCue = cs->cueId();
        }
        else if (qobject_cast<Scene *>(m_doc->function(loadedFunction())) != nullptr
                 && (m_doc->showTree() == nullptr || !m_doc->showTree()->isGenerated(loadedFunction())))
        {
            req.fromScene = loadedFunction();
        }
    }
    // The bench: nothing loaded behind it, so its preview is the only one playing.
    clear();
    m_benchFromCue = req.fromCue;
    m_benchFromScene = req.fromScene;
    QWidget *w = s_effectPane(this, req, m_functionPage);
    if (w == nullptr)
        return false;
    const QLCPalette *p = m_doc->palette(request.palette);
    hostEffectPane(w, p != nullptr ? tr("<b>Design effect</b> — %1, on the bench").arg(p->name().toHtmlEscaped())
                                   : tr("<b>Design effect</b> — a new effect, on the bench"), true);
    return true;
}

void Workbench::hostEffectPane(QWidget *pane, const QString &heading, bool bench)
{
    clearEditors();
    m_groupBackButton->hide();
    m_funcEditor = pane;
    m_effectPane = pane;
    m_benchPane = bench;
    m_functionHeading->setText(heading);
    m_functionLayout->addWidget(pane, 1);
    pane->show();
    showPage(FunctionPage);
}

void Workbench::setSolo(bool solo)
{
    if (m_stack == nullptr || !m_stack->canSolo() || m_stack->solo() == solo)
        return;
    m_stack->setSolo(solo);
    syncStackPreview();
    updateOnRig();
}

bool Workbench::isSolo() const
{
    return m_stack != nullptr && m_stack->solo();
}

void Workbench::syncStackPreview()
{
    // The back end decides what plays (the cue, or its Solo layer); asked
    // again after anything that may change it.
    if (m_stack == nullptr || !m_stack->playsItself() || m_previewHeld)
        return;
    if (m_ownPreview)
        m_stack->startOwnPreview();
    else
        startPreview();
    updateOnRig();
}

/****************************************************************************
 * W2: the back end's header widget, steps walked, shared layers split off
 ****************************************************************************/

void Workbench::installStackWidget()
{
    if (m_stackWidget != nullptr)
        m_stackWidget->deleteLater();
    m_stackWidget = nullptr;
    QWidget *column = m_rowsHint->parentWidget();
    QVBoxLayout *col = qobject_cast<QVBoxLayout *>(column->layout());
    QWidget *w = m_stack != nullptr ? m_stack->createHeaderWidget(column) : nullptr;
    if (w == nullptr || col == nullptr)
        return;
    // Under the header, over the layers.
    col->insertWidget(col->indexOf(m_rowsHint), w);
    m_stackWidget = w;
    if (ChaserWalker *walker = qobject_cast<ChaserWalker *>(w))
        connect(walker, &ChaserWalker::openRequested, this, &Workbench::open);
}

void Workbench::stackLayersReplaced()
{
    // Another set of layers (a chaser's next step): nothing of the last
    // one stays in the editor pane.
    m_restartTimer->stop();
    m_editorFunction = Function::invalidId();
    loadCanvas(Function::invalidId());
    clearEditors();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_rows->rebuild();
    m_rows->clearSelection();
    slotRowsSelectionChanged();
    updateHeader();
    updateAddBar();
    // What previews now (the step walked to) plays instead.
    if (m_stack != nullptr && m_stack->previewFunction() != m_previewFunction)
    {
        stopPreview();
        startPreview();
    }
    if (m_stack != nullptr && m_operateFunction != Function::invalidId()
        && m_stack->previewFunction() != m_operateFunction)
    {
        stopOperate();
        startOperate();
    }
    updateOnRig();
}

void Workbench::makeOwnCopySelected()
{
    const int layer = selectedLayer();
    if (m_stack == nullptr || !m_stack->canMakeOwnCopy(layer))
        return;
    if (m_stack->makeOwnCopy(layer) == Function::invalidId())
        return;
    m_rows->rebuild();
    selectLayer(layer);
    layersChanged();
}
