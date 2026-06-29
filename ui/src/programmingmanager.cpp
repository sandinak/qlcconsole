/*
  Q Light Controller Plus
  programmingmanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QShowEvent>
#include <QHideEvent>
#include <QTreeWidgetItem>
#include <QShortcut>
#include <QScrollArea>
#include <QMenu>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QFrame>
#include <QSettings>

#include <QDebug>
#include "programmingmanager.h"
#include "programmercontroller.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "scenegrouplooks.h"
#include "lookeditor.h"
#include "fixtureconsole.h"
#include "functionparent.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "efx.h"
#include "rgbmatrix.h"
#include "chasereditor.h"
#include "collectioneditor.h"
#include "efxeditor.h"
#include "rgbmatrixeditor.h"
#include "chaserstep.h"
#include "chaseraction.h"
#include "scenevalue.h"
#include "powerdistribution.h"
#include "powerdistributiondialog.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "effectscriptrunner.h"
#include "monitor/monitor.h"

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_funcEditor(nullptr)
    , m_currentScene(Function::invalidId())
    , m_canvasFunction(Function::invalidId())
    , m_previewFunction(Function::invalidId())
    , m_operateFunction(Function::invalidId())
    , m_clipboardFunction(Function::invalidId())
    , m_clipboardPalette(QLCPalette::invalidId())
    , m_memberContainer(Function::invalidId())
{
    QHBoxLayout *root = new QHBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter);

    // --- Left: function tree (folders) of what you build ---
    QWidget *navPanel = new QWidget(this);
    QVBoxLayout *navCol = new QVBoxLayout(navPanel);
    navCol->setContentsMargins(0, 0, 0, 0);
    navCol->addWidget(new QLabel(tr("Scenes / functions"), this));
    QLineEdit *funcFilter = new QLineEdit(this);
    funcFilter->setPlaceholderText(tr("Filter…"));
    funcFilter->setClearButtonEnabled(true);
    navCol->addWidget(funcFilter);
    m_funcTree = new FunctionsTreeWidget(m_doc, this);
    m_funcTree->setDisplayFilter(FunctionsTreeWidget::FunctionsOnly);
    m_funcTree->setHeaderHidden(true);
    m_funcTree->setColumnHidden(1, true);
    m_funcTree->setSortingEnabled(true);
    m_funcTree->sortByColumn(0, Qt::AscendingOrder);
    m_funcTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_funcTree->updateTree();
    navCol->addWidget(m_funcTree, 1);
    connect(funcFilter, &QLineEdit::textChanged,
            m_funcTree, &FunctionsTreeWidget::filterByText);
    navCol->addWidget(new QLabel(tr("Right-click to add a scene/function or folder."), this));
    splitter->addWidget(navPanel);

    // --- Center: the selected scene's canvas ---
    QWidget *canvasPanel = new QWidget(this);
    m_canvasLayout = new QVBoxLayout(canvasPanel);

    // Toolbar: Highlight + Followspot toggles
    {
        QHBoxLayout *toolbar = new QHBoxLayout;
        toolbar->setContentsMargins(0, 0, 0, 0);

        m_highlightBtn = new QPushButton(tr("Highlight"), this);
        m_highlightBtn->setCheckable(true);
        m_highlightBtn->setToolTip(tr("Drive selected fixtures to full white so you can identify them in the rig"));
        toolbar->addWidget(m_highlightBtn);

        toolbar->addStretch(1);

        // BPM / internal beat generator
        toolbar->addWidget(new QLabel(tr("BPM:"), this));

        m_bpmSpin = new QSpinBox(this);
        m_bpmSpin->setRange(20, 400);
        m_bpmSpin->setValue(120);
        m_bpmSpin->setToolTip(tr("Beats per minute for internal beat generator"));
        m_bpmSpin->setEnabled(false);
        toolbar->addWidget(m_bpmSpin);

        m_tapBtn = new QPushButton(tr("Tap"), this);
        m_tapBtn->setToolTip(tr("Tap to set BPM from live tempo"));
        m_tapBtn->setEnabled(false);
        toolbar->addWidget(m_tapBtn);

        m_bpmBtn = new QPushButton(tr("Beat On"), this);
        m_bpmBtn->setCheckable(true);
        m_bpmBtn->setToolTip(tr("Enable QLC+ internal beat generator (drives _beat/_bpm in effect scripts)"));
        toolbar->addWidget(m_bpmBtn);

        toolbar->addWidget(new QLabel(tr("Jog:"), this));

        m_speedSpin = new QSpinBox(this);
        m_speedSpin->setRange(10, 500);
        m_speedSpin->setSingleStep(10);
        m_speedSpin->setValue(100);
        m_speedSpin->setSuffix(tr("%"));
        m_speedSpin->setToolTip(tr("Joystick speed for position editing (100% = 2 m/s at full deflection)"));
        toolbar->addWidget(m_speedSpin);

        m_saveBtn = new QPushButton(tr("Save"), this);
        m_saveBtn->setEnabled(false);
        m_saveBtn->setToolTip(tr("Save joystick position edits to the workspace file"));
        toolbar->addWidget(m_saveBtn);

        m_canvasLayout->addLayout(toolbar);

        connect(m_highlightBtn, &QPushButton::toggled,
                this, &ProgrammingManager::slotHighlightToggled);
        connect(m_saveBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotSavePositions);
        connect(m_speedSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
                    ProgrammerController *pc = m_doc->programmer();
                    if (pc) pc->setJoystickSensitivity(v / 100.0f);
                });
        connect(m_bpmBtn, &QPushButton::toggled,
                this, &ProgrammingManager::slotBpmToggled);
        connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ProgrammingManager::slotBpmChanged);
        connect(m_tapBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotTapBeat);
    }

    // Followspot config panel (hidden until activated)
    {
        m_followSpotPanel = new QGroupBox(tr("Followspot settings"), this);
        QFormLayout *form = new QFormLayout(m_followSpotPanel);
        form->setContentsMargins(4, 4, 4, 4);
        form->setSpacing(4);

        QHBoxLayout *xRow = new QHBoxLayout;
        m_fsBindXBtn = new QPushButton(tr("Bind X (pan)"), this);
        m_fsXLabel = new QLabel(tr("—"), this);
        xRow->addWidget(m_fsBindXBtn);
        xRow->addWidget(m_fsXLabel, 1);
        form->addRow(tr("Pan axis:"), xRow);

        QHBoxLayout *yRow = new QHBoxLayout;
        m_fsBindYBtn = new QPushButton(tr("Bind Y (tilt)"), this);
        m_fsYLabel = new QLabel(tr("—"), this);
        yRow->addWidget(m_fsBindYBtn);
        yRow->addWidget(m_fsYLabel, 1);
        form->addRow(tr("Tilt axis:"), yRow);

        // Note: controller must be assigned as universe input in I/O Manager
        QLabel *inputNote = new QLabel(
            tr("<small>⚠ Controller must be assigned as a universe <b>Input</b> in the "
               "Input/Output Manager before Bind will detect it.</small>"), this);
        inputNote->setWordWrap(true);
        form->addRow(inputNote);

        m_fsClearBtn = new QPushButton(tr("Clear bindings"), this);
        form->addRow(QString(), m_fsClearBtn);

        m_fsJoyLabel = new QLabel(tr("—"), this);
        m_fsJoyLabel->setStyleSheet("font-family: monospace;");
        form->addRow(tr("Joystick:"), m_fsJoyLabel);

        m_canvasLayout->addWidget(m_followSpotPanel);
        m_followSpotPanel->hide(); // axis binding lives in I/O → Mapping → Joystick

        connect(m_fsBindXBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotFollowSpotBindX);
        connect(m_fsBindYBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotFollowSpotBindY);
        connect(m_fsClearBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotFollowSpotClearBindings);
        // Sync with engine state
        ProgrammerController *pc = m_doc->programmer();
        if (pc)
        {
            connect(pc, &ProgrammerController::axisBindingChanged,
                    this, &ProgrammingManager::slotFollowSpotBindingChanged);
            connect(pc, &ProgrammerController::highlightActiveChanged,
                    m_highlightBtn, &QPushButton::setChecked);
            connect(pc, &ProgrammerController::joystickUpdated,
                    this, &ProgrammingManager::slotJoystickUpdated);
            connect(pc, &ProgrammerController::buttonActionTriggered,
                    this, &ProgrammingManager::slotButtonAction);
            connect(pc, &ProgrammerController::designPositionWritten,
                    this, &ProgrammingManager::slotDesignPositionWritten);
            connect(pc, &ProgrammerController::followSpotPinChanged,
                    this, &ProgrammingManager::slotFollowSpotPinChanged);
        }
    }

    m_canvasTitle = new QLabel(this);
    m_canvasTitle->setStyleSheet("font-weight: bold;");
    m_canvasTitle->setWordWrap(true);
    m_canvasLayout->addWidget(m_canvasTitle);
    m_canvasPlaceholder = new QLabel(
        tr("Select or create a scene on the left, then drag palettes, "
           "fixture groups and fixtures from the right onto it."), this);
    m_canvasPlaceholder->setWordWrap(true);
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasLayout->addWidget(m_canvasPlaceholder, 1);
    // Inline look editor pinned to the bottom of the center panel. It is NOT
    // scrolled: it always renders fully at its preferred height (Fixed
    // vertical), and the canvas above (stretch) shrinks to make room.
    m_lookEditor = new LookEditor(m_doc, this);
    // Take the look editor's full preferred height; the canvas above (Expanding)
    // shrinks first to make room. Maximum (not Fixed) so the window can still be
    // resized very short if needed (the canvas hits its min first).
    m_lookEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_lookEditor->setMaximumHeight(340); // matches the capped picker/XY pad
    m_canvasLayout->addWidget(m_lookEditor);

    // Per-fixture DMX channel editor, shown at the bottom when fixed-fixture
    // targets are selected (instead of the look editor). Height-bounded so a
    // tall console scrolls inside the panel instead of overrunning it.
    m_fixtureConsole = new FixtureConsole(this, m_doc);
    m_fixtureScroll = new QScrollArea(this);
    m_fixtureScroll->setWidgetResizable(true);
    m_fixtureScroll->setFrameShape(QFrame::NoFrame);
    // Minimum so the channel faders always have usable height (they were
    // getting squished to nothing when the Targets list was tall); scrolls
    // within the band if the console is taller.
    m_fixtureScroll->setMinimumHeight(200);
    m_fixtureScroll->setMaximumHeight(400);
    m_fixtureScroll->setWidget(m_fixtureConsole);
    m_fixtureScroll->hide();
    m_canvasLayout->addWidget(m_fixtureScroll);

    // Shown instead of the console when several selected fixed fixtures are of
    // different types (editing them by channel index would be meaningless).
    m_fixtureMixedNote = new QLabel(this);
    m_fixtureMixedNote->setWordWrap(true);
    m_fixtureMixedNote->setAlignment(Qt::AlignCenter);
    m_fixtureMixedNote->setMinimumHeight(120);
    m_fixtureMixedNote->setStyleSheet("QLabel { color: gray; }");
    m_fixtureMixedNote->hide();
    m_canvasLayout->addWidget(m_fixtureMixedNote);

    // Estimated electrical load of the previewed look (Design mode only).
    buildPowerFooter();

    connect(m_fixtureConsole, SIGNAL(valueChanged(quint32,quint32,uchar)),
            this, SLOT(slotFixtureValueChanged(quint32,quint32,uchar)));
    connect(m_fixtureConsole, SIGNAL(checked(quint32,quint32,bool)),
            this, SLOT(slotFixtureChecked(quint32,quint32,bool)));
    splitter->addWidget(canvasPanel);

    // --- Right: drag sources (palette tree + fixtures & groups tree) ---
    QWidget *sourcePanel = new QWidget(this);
    QVBoxLayout *srcCol = new QVBoxLayout(sourcePanel);
    srcCol->setContentsMargins(0, 0, 0, 0);

    srcCol->addWidget(new QLabel(tr("Palettes (looks)"), this));
    QLineEdit *paletteFilter = new QLineEdit(this);
    paletteFilter->setPlaceholderText(tr("Filter…"));
    paletteFilter->setClearButtonEnabled(true);
    srcCol->addWidget(paletteFilter);
    m_paletteTree = new FunctionsTreeWidget(m_doc, this);
    m_paletteTree->setDisplayFilter(FunctionsTreeWidget::PalettesOnly);
    m_paletteTree->setHeaderHidden(true);
    m_paletteTree->setColumnHidden(1, true);
    m_paletteTree->setSortingEnabled(true);
    m_paletteTree->sortByColumn(0, Qt::AscendingOrder);
    m_paletteTree->setExternalDragMode(true); // emit palette MIME on drag
    m_paletteTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_paletteTree->updateTree();
    srcCol->addWidget(m_paletteTree, 1);
    srcCol->addWidget(new QLabel(tr("Right-click to add a palette."), this));
    connect(paletteFilter, &QLineEdit::textChanged,
            m_paletteTree, &FunctionsTreeWidget::filterByText);

    srcCol->addWidget(new QLabel(tr("Fixtures & groups"), this));
    m_fixGroupSource = new FixtureGroupSource(m_doc, this);
    srcCol->addWidget(m_fixGroupSource, 1);
    connect(m_fixGroupSource, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotFixGroupSourceSelectionChanged);

    splitter->addWidget(sourcePanel);

    splitter->setStretchFactor(0, 2); // nav
    splitter->setStretchFactor(1, 3); // canvas
    splitter->setStretchFactor(2, 2); // sources

    // Open on DOUBLE-click only; single-click just selects, so a function
    // can be click-dragged from the tree into a collection/chaser canvas
    // without the canvas switching out from under the drag.
    connect(m_funcTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotFunctionActivated(QTreeWidgetItem*)));
    connect(m_funcTree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotFuncTreeMenu(QPoint)));
    connect(m_paletteTree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotPaletteTreeMenu(QPoint)));

    // Drag palettes onto folders to re-file them. Defer the rebuild: this fires
    // from the tree's dropEvent, and updateTree() deletes the items Qt's drag
    // machinery still references (use-after-free) if done synchronously.
    connect(m_paletteTree, &FunctionsTreeWidget::paletteDroppedToFolder,
            this, [this]() {
        QTimer::singleShot(0, this, [this]() { m_paletteTree->updateTree(); m_doc->setModified(); });
    });

    // Keep the trees in sync with the Doc (functor connects so we can call
    // the trees' non-slot helpers directly).
    connect(m_doc, &Doc::functionAdded, m_funcTree, &FunctionsTreeWidget::addFunction);
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32) {
        const quint32 c = m_memberContainer;
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
        if (c != Function::invalidId())
            syncMemberNodes(c);   // re-nest members (node is fresh after rebuild)
    });
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 id) {
        m_funcTree->functionNameChanged(id);
        if (id == m_memberContainer)
            syncMemberNodes(m_memberContainer); // membership may have changed
    });
    connect(m_doc, &Doc::loaded, this, [this]() {
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
    });
    connect(m_doc, &Doc::cleared, this, [this]() {
        m_currentScene   = Function::invalidId();
        m_canvasFunction = Function::invalidId();
        clearEditors();
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
    });

    connect(m_doc, &Doc::paletteAdded, m_paletteTree, &FunctionsTreeWidget::addPalette);
    connect(m_doc, &Doc::paletteRemoved, this, [this](quint32) { m_paletteTree->updateTree(); });
    connect(m_doc, &Doc::loaded, m_paletteTree, &FunctionsTreeWidget::updateTree);
    connect(m_doc, &Doc::cleared, m_paletteTree, &FunctionsTreeWidget::updateTree);

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
    connect(m_lookEditor, SIGNAL(paletteChanged(quint32)),
            this, SLOT(slotLookEdited()));
    // Lightweight live-value change (effect params, etc.) — only refresh the
    // DMX preview, do NOT rebuild the look list or palette tree.  This avoids
    // a reload() → lookSelected → setPalette → rebuildEffectDynWidget cycle
    // that was causing all effect sliders to jump when any one slider moved.
    connect(m_lookEditor, &LookEditor::paletteValueChanged,
            this, [this](quint32) { refreshPreview(); });
    // Refresh canvas palette tiles when a palette is renamed in the tree.
    connect(m_paletteTree, &FunctionsTreeWidget::paletteRenamed,
            this, [this](quint32) { if (m_canvas) m_canvas->reload(); });

    // Double-click a palette in the source tree to edit it inline too.
    connect(m_paletteTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotPaletteDoubleClicked(QTreeWidgetItem*)));

    // Cmd/Ctrl-C then Cmd/Ctrl-V duplicates the selected function.
    QShortcut *copySc = new QShortcut(QKeySequence::Copy, this);
    copySc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copySc, SIGNAL(activated()), this, SLOT(slotCopy()));
    QShortcut *pasteSc = new QShortcut(QKeySequence::Paste, this);
    pasteSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteSc, SIGNAL(activated()), this, SLOT(slotPaste()));
}

ProgrammingManager::~ProgrammingManager()
{
    stopPreview();
}

void ProgrammingManager::slotFunctionActivated(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;

    const quint32 fid = m_funcTree->itemFunctionId(item);
    Function *f = m_doc->function(fid);
    if (f == NULL)
        loadCanvas(Function::invalidId()); // folder / category -> placeholder
    else if (f->type() == Function::SceneType)
        loadCanvas(fid);
    else
        loadFunctionEditor(f);
}

void ProgrammingManager::loadCanvas(quint32 sceneId)
{
    {
        Function *f = m_doc->function(sceneId);
        qCritical() << "[PM] loadCanvas:" << sceneId << (f ? f->name() : "(null)")
                    << "mode=" << (m_doc->mode() == Doc::Design ? "Design" : "Operate")
                    << "previewFn=" << m_previewFunction
                    << "operateFn=" << m_operateFunction;
    }
    if (sceneId == m_currentScene && m_canvas != NULL)
        return;

    stopPreview();
    m_currentScene = sceneId;
    m_canvasFunction = sceneId;
    m_lookEditor->setPalette(QLCPalette::invalidId());
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(sceneId);
    m_funcTree->setExternalDragMode(false); // only collections/chasers need it
    // Keep the collection/chaser context (its nested subtree) when drilling
    // into one of ITS member scenes, so you can go back and forth; clear it
    // only when opening an unrelated scene.
    if (isContainerMember(m_memberContainer, sceneId) == false)
        syncMemberNodes(Function::invalidId());
    clearEditors();

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
    {
        m_doc->setFocusedScene(Function::invalidId());
        if (ProgrammerController *pc = m_doc->programmer())
            pc->seedStageAimFromScene(Function::invalidId());
        m_canvasPlaceholder->setText(
            tr("Select or create a scene on the left, then drag palettes, "
               "fixture groups and fixtures from the right onto it."));
        m_canvasPlaceholder->show();
        updateTitle();
        return;
    }

    m_canvasPlaceholder->hide();
    m_selectedFixtures.clear();
    m_fixtureScroll->hide(); // default to the look editor at the bottom
    m_fixtureMixedNote->hide();
    m_lookEditor->show();
    m_lookEditor->setContextScene(scene);
    m_doc->setFocusedScene(sceneId);

    m_canvas = new SceneGroupLooks(scene, m_doc, this, /*includeFixtureTargets*/ true);
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    connect(m_canvas, SIGNAL(lookSelected(quint32)),
            m_lookEditor, SLOT(setPalette(quint32)));
    connect(m_canvas, SIGNAL(fixturesSelected(QList<quint32>)),
            this, SLOT(slotFixturesSelected(QList<quint32>)));
    // Selecting a look switches the bottom back to the look editor and
    // notifies the programmer controller so APC40 sliders target that palette.
    connect(m_canvas, &SceneGroupLooks::lookSelected, this, [this](quint32 pid) {
        m_doc->setFocusedPalette(pid);
        if (pid != QLCPalette::invalidId()) { m_fixtureScroll->hide(); m_fixtureMixedNote->hide(); m_lookEditor->show(); }
    });
    // Insert above the bottom editors.
    m_canvasLayout->insertWidget(m_canvasLayout->indexOf(m_lookEditor), m_canvas, 1);

    stopOperateScene();  // stop the previous scene before starting the new one
    startPreview();
    startOperateScene(); // no-op in Design mode; keeps all channels live in Operate mode
    // Seed the aim/pin from this scene's Aim target (if any) so the pin appears
    // on the target.  Live position is driven by the scene's followspot.js
    // effect (single engine), not a native effect.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->seedStageAimFromScene(sceneId);
    updateTitle();
}

void ProgrammingManager::clearEditors()
{
    if (m_canvas != NULL)
    {
        m_canvas->deleteLater();
        m_canvas = NULL;
    }
    if (m_funcEditor != NULL)
    {
        m_funcEditor->deleteLater();
        m_funcEditor = NULL;
    }
    m_doc->setFocusedScene(Function::invalidId());
    m_doc->setFocusedPalette(QLCPalette::invalidId());
}

void ProgrammingManager::loadFunctionEditor(Function *f)
{
    if (f == NULL)
        return;

    stopPreview();
    m_currentScene = Function::invalidId();
    m_canvasFunction = f->id();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(Function::invalidId());
    clearEditors();
    m_canvasPlaceholder->hide();
    m_fixtureScroll->hide(); // non-scene: no per-fixture editor
    m_fixtureMixedNote->hide();
    m_lookEditor->show();

    // Host the stock editor for the function type; collections and chasers
    // accept functions dragged from the left tree (external drag mode).
    QWidget *ed = NULL;
    bool dragIn = false;
    switch (f->type())
    {
    case Function::CollectionType:
        ed = new CollectionEditor(this, qobject_cast<Collection*>(f), m_doc);
        dragIn = true;
        break;
    case Function::ChaserType:
        ed = new ChaserEditor(this, qobject_cast<Chaser*>(f), m_doc);
        dragIn = true;
        break;
    case Function::EFXType:
        ed = new EFXEditor(this, qobject_cast<EFX*>(f), m_doc);
        break;
    case Function::RGBMatrixType:
        ed = new RGBMatrixEditor(this, qobject_cast<RGBMatrix*>(f), m_doc);
        break;
    default:
        break;
    }

    m_funcTree->setExternalDragMode(dragIn);
    // If this function is nested inside the current container's tree (e.g. a
    // collection under the open chaser), keep that context + subtree — just
    // show/preview the selected function. Otherwise establish a new context.
    if (isContainerMember(m_memberContainer, f->id()) == false)
        syncMemberNodes(dragIn ? f->id() : Function::invalidId());

    if (ed == NULL)
    {
        m_canvasPlaceholder->setText(
            tr("\"%1\" (%2) — open it in the Functions tab to edit.")
            .arg(f->name()).arg(Function::typeToString(f->type())));
        m_canvasPlaceholder->show();
        m_canvasFunction = Function::invalidId(); // nothing previewable shown
        updateTitle();
        return;
    }

    m_funcEditor = ed;
    m_canvasLayout->insertWidget(m_canvasLayout->indexOf(m_lookEditor), m_funcEditor, 1);
    m_funcEditor->show();
    startPreview();   // run it live so the view reflects it (Design mode)
    updateTitle();
}

void ProgrammingManager::updateTitle()
{
    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
    {
        m_canvasTitle->setText(tr("Programming"));
        return;
    }
    const QString kind = (f->type() == Function::SceneType)
                         ? tr("scene") : Function::typeToString(f->type());
    const bool live = (m_previewFunction == m_canvasFunction);
    const int uses = functionUsageCount(m_canvasFunction);
    m_canvasTitle->setText(
        tr("Editing %1: %2   —   used in %n place(s)   [ live preview: %3 ]",
           "", uses)
        .arg(kind)
        .arg(f->name())
        .arg(live ? tr("ON") : tr("OFF — Design mode only")));
}

bool ProgrammingManager::isContainerMember(quint32 containerId, quint32 fid) const
{
    if (containerId == Function::invalidId())
        return false;
    QSet<quint32> visited;
    return containerHas(containerId, fid, visited, 0);
}

bool ProgrammingManager::containerHas(quint32 containerId, quint32 fid,
                                      QSet<quint32> &visited, int depth) const
{
    if (depth > 8 || visited.contains(containerId))
        return false;
    visited.insert(containerId);

    Function *c = m_doc->function(containerId);
    QList<quint32> members;
    if (Collection *col = qobject_cast<Collection*>(c))
        members = col->functions();
    else if (Chaser *ch = qobject_cast<Chaser*>(c))
        foreach (const ChaserStep &s, ch->steps())
            members << s.fid;
    else
        return false;

    foreach (quint32 mid, members)
    {
        if (mid == fid)
            return true;
        if (containerHas(mid, fid, visited, depth + 1))
            return true;
    }
    return false;
}

void ProgrammingManager::slotLookEdited()
{
    // A look's value changed (including name edits) in the editor:
    // refresh the preview and update any tree/canvas items that show the name.
    refreshPreview();
    // Update the palette tree so a renamed palette shows its new name.
    m_paletteTree->updateTree();
    // Update the canvas's looks list (tiles show palette name).
    if (m_canvas)
        m_canvas->reload();
}

void ProgrammingManager::createPalette(int paletteType)
{
    const QLCPalette::PaletteType type =
        static_cast<QLCPalette::PaletteType>(paletteType);
    QLCPalette *p = new QLCPalette(type);
    p->setName(tr("New %1").arg(QLCPalette::typeToString(type)));

    // Sensible starting value so the palette is valid/saveable.
    switch (type)
    {
    case QLCPalette::Color:   p->setValue(QColor(Qt::white).name()); break;
    case QLCPalette::Dimmer:  p->setValue(255); break;
    case QLCPalette::PanTilt: p->setValue(270, 135); break;
    case QLCPalette::Aim:     break; // no m_values; target set in LookEditor
    case QLCPalette::Effect:  break; // no value needed; script path set in LookEditor
    default:                  p->setValue(0); break; // Gobo / Shutter / …
    }

    // Default to a type-named folder; override with selected folder if one is active.
    p->setPath(QString("Palettes/%1/").arg(QLCPalette::typeToString(type)));
    const QList<QTreeWidgetItem*> sel = m_paletteTree->selectedItems();
    if (sel.isEmpty() == false)
    {
        QString cp = sel.first()->text(1); // folder COL_PATH ("Palettes/…")
        if (cp.isEmpty())
        {
            QLCPalette *sp = m_doc->palette(m_paletteTree->itemPaletteId(sel.first()));
            if (sp != NULL)
                cp = sp->path();
        }
        if (cp.isEmpty() == false)
            p->setPath(cp);
    }

    if (m_doc->addPalette(p) == false)
    {
        delete p;
        return;
    }
    m_doc->setModified();
    m_lookEditor->setPalette(p->id()); // edit it inline immediately
    showLookEditorPanel();
}

void ProgrammingManager::slotPaletteDoubleClicked(QTreeWidgetItem *item)
{
    const quint32 pid = m_paletteTree->itemPaletteId(item);
    if (pid != QLCPalette::invalidId())
    {
        m_lookEditor->setPalette(pid);
        showLookEditorPanel(); // pivot from the DMX console if it's showing
    }
}

void ProgrammingManager::showLookEditorPanel()
{
    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();
    m_lookEditor->show();
}

void ProgrammingManager::slotFixGroupSourceSelectionChanged()
{
    QList<quint32> fixtures;
    for (QTreeWidgetItem *it : m_fixGroupSource->selectedItems())
    {
        const int kind = it->data(0, FixtureGroupSource::KindRole).toInt();
        if (kind == FixtureGroupSource::FixtureNode)
        {
            const quint32 fid = it->data(0, FixtureGroupSource::IdRole).toUInt();
            if (!fixtures.contains(fid))
                fixtures << fid;
        }
        else if (kind == FixtureGroupSource::GroupNode)
        {
            const quint32 gid = it->data(0, FixtureGroupSource::IdRole).toUInt();
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g)
                for (quint32 fid : g->fixtureList())
                    if (!fixtures.contains(fid))
                        fixtures << fid;
        }
    }
    slotFixturesSelected(fixtures);
}

void ProgrammingManager::slotFixturesSelected(const QList<quint32> &fixtureIds)
{
    m_selectedFixtures = fixtureIds;
    // Feed selection to ProgrammerController so Highlight/Followspot effects
    // know which fixtures to steer.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->setProgrammerSelection(fixtureIds);
    if (Monitor::instance() != NULL)
        Monitor::instance()->highlightFixtures(fixtureIds);
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (fixtureIds.isEmpty() || s == NULL
        || m_doc->fixture(fixtureIds.first()) == NULL)
    {
        m_fixtureScroll->hide();
        m_fixtureMixedNote->hide();
        m_lookEditor->show();
        return;
    }

    // Channel edits are mirrored across the selected fixtures by channel
    // index, which is only meaningful when they share the same type. If the
    // selection mixes types, show a note instead of a misleading console.
    const quint32 lead = fixtureIds.first();
    Fixture *leadFx = m_doc->fixture(lead);
    int mismatched = 0;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == NULL || sameFixtureType(leadFx, f) == false)
            mismatched++;
    }

    if (fixtureIds.size() > 1 && mismatched > 0)
    {
        m_lookEditor->hide();
        m_fixtureScroll->hide();
        m_fixtureMixedNote->setText(
            tr("%n fixed fixture(s) of different types are selected.\n"
               "Select fixtures of the same type to edit their DMX channels together.",
               "", fixtureIds.size()));
        m_fixtureMixedNote->show();
        return;
    }

    // The console edits the FIRST selected fixture as a template; channel
    // edits are mirrored to all selected fixtures (see below). Prefill so the
    // console reflects what the scene actually outputs for this fixture:
    // first the applied looks (palettes are resolved per fixture, not baked
    // into values), then the baked scene values on top — matching the runtime
    // order in Scene::writeDMX (palettes first, m_values override).
    m_fixtureConsole->blockSignals(true);
    m_fixtureConsole->setFixture(lead);
    // Checkable channels default to CHECKED; start them all unchecked so only
    // the channels this scene actually drives (baked values + the channels its
    // applied looks impact) end up selected. This keeps static scenes narrow
    // so several can be layered on one fixture (colour / intensity / position
    // / gobo as separate scenes) without clobbering each other.
    m_fixtureConsole->setChecked(false);

    // Baked values first, then applied looks override them (palette paramount,
    // matching Scene::writeDMX). Look-controlled channels are checked but
    // greyed out — they're driven by the palette, not directly editable.
    foreach (const SceneValue &scv, s->values())
        if (scv.fxi == lead)
            m_fixtureConsole->setSceneValue(scv);

    QList<quint32> lockedChannels;
    foreach (quint32 pid, s->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (pal == NULL)
            continue;
        foreach (const SceneValue &scv, pal->valuesFromFixtureGroups(m_doc, s->fixtureGroups()))
            if (scv.fxi == lead)
            {
                m_fixtureConsole->setSceneValue(scv);
                lockedChannels << scv.channel;
            }
        foreach (const SceneValue &scv, pal->valuesFromFixtures(m_doc, s->fixtures()))
            if (scv.fxi == lead)
            {
                m_fixtureConsole->setSceneValue(scv);
                lockedChannels << scv.channel;
            }
    }
    foreach (quint32 ch, lockedChannels)
        m_fixtureConsole->setChannelEnabled(ch, false);
    m_fixtureConsole->blockSignals(false);

    m_lookEditor->hide();
    m_fixtureMixedNote->hide();
    m_fixtureScroll->show();
    // The scroll area may already have been visible (fixture->fixture switch),
    // so no showEvent fires for the rebuilt console — reveal its channels
    // explicitly so single and multi selections render identically.
    m_fixtureConsole->showChannels();
}

bool ProgrammingManager::sameFixtureType(const Fixture *a, const Fixture *b) const
{
    if (a == NULL || b == NULL)
        return false;
    if (a == b)
        return true;
    // Identical definition + mode => identical channel layout. Generic
    // fixtures (no def) match by channel count.
    if (a->fixtureDef() != NULL || b->fixtureDef() != NULL)
        return a->fixtureDef() == b->fixtureDef()
            && a->fixtureMode() == b->fixtureMode();
    return a->channels() == b->channels();
}

void ProgrammingManager::slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value)
{
    Q_UNUSED(fxi)
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (s == NULL)
        return;
    // Apply the change to every selected fixture that has this channel.
    foreach (quint32 fid, m_selectedFixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != NULL && ch < f->channels())
            s->setValue(fid, ch, value);
    }
    m_doc->setModified();
    refreshPreview();
}

void ProgrammingManager::slotFixtureChecked(quint32 fxi, quint32 ch, bool state)
{
    Q_UNUSED(fxi)
    if (state) // enabling emits a value via valueChanged separately
        return;
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (s == NULL)
        return;
    foreach (quint32 fid, m_selectedFixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != NULL && ch < f->channels())
            s->unsetValue(fid, ch);
    }
    m_doc->setModified();
    refreshPreview();
}

void ProgrammingManager::slotCopy()
{
    // Palette tree focused with a palette selected? Copy the palette instead.
    if (m_paletteTree->hasFocus())
    {
        const QList<QTreeWidgetItem*> psel = m_paletteTree->selectedItems();
        if (psel.isEmpty() == false)
        {
            const quint32 pid = m_paletteTree->itemPaletteId(psel.first());
            if (pid != QLCPalette::invalidId())
            {
                m_clipboardPalette = pid;
                return;
            }
        }
    }

    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    if (sel.isEmpty())
        return;
    const quint32 fid = m_funcTree->itemFunctionId(sel.first());
    if (fid != Function::invalidId())
        m_clipboardFunction = fid;
}

void ProgrammingManager::slotPaste()
{
    if (m_paletteTree->hasFocus() && m_clipboardPalette != QLCPalette::invalidId())
    {
        duplicatePalette(m_clipboardPalette);
        return;
    }

    Function *src = m_doc->function(m_clipboardFunction);
    if (src == NULL)
        return;
    Function *copy = src->createCopy(m_doc);
    if (copy == NULL)
        return;
    copy->setName(m_doc->nextDuplicateName(src));

    QTreeWidgetItem *it = m_funcTree->functionItem(copy);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
}

void ProgrammingManager::duplicatePalette(quint32 pid)
{
    QLCPalette *src = m_doc->palette(pid);
    if (src == NULL)
        return;
    QLCPalette *copy = src->createCopy();
    if (copy == NULL)
        return;
    copy->setName(src->name() + tr(" (copy)"));
    if (m_doc->addPalette(copy) == false)
    {
        delete copy;
        return;
    }
    m_doc->setModified();
    m_paletteTree->updateTree();
    QTreeWidgetItem *it = m_paletteTree->paletteItem(copy);
    if (it != NULL)
    {
        m_paletteTree->setCurrentItem(it);
        m_paletteTree->scrollToItem(it);
    }
    m_lookEditor->setPalette(copy->id());
    showLookEditorPanel();
}

void ProgrammingManager::startPreview()
{
    // Live-preview the canvas function (scene, collection, …) by running it.
    // Design mode only, so we don't hijack a running show.
    if (m_doc->mode() != Doc::Design)
    {
        qCritical() << "[PM] startPreview: skipped (mode is Operate)";
        return;
    }
    if (isVisible() == false)
    {
        qCritical() << "[PM] startPreview: skipped (tab not visible)";
        return;
    }
    if (m_previewFunction == m_canvasFunction)
    {
        qCritical() << "[PM] startPreview: already running canvas" << m_canvasFunction;
        return;
    }

    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
    {
        qCritical() << "[PM] startPreview: canvas" << m_canvasFunction << "not found";
        return;
    }

    qCritical() << "[PM] startPreview: starting" << m_canvasFunction << f->name();
    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_previewFunction = m_canvasFunction;
    updateTitle();
}

void ProgrammingManager::stopPreview()
{
    if (m_previewFunction == Function::invalidId())
        return;
    Function *f = m_doc->function(m_previewFunction);
    qCritical() << "[PM] stopPreview: stopping" << m_previewFunction
                << (f ? f->name() : "(null)");
    if (f != NULL)
        f->stop(FunctionParent::master());
    m_previewFunction = Function::invalidId();
    updateTitle();
}

void ProgrammingManager::refreshPreview()
{
    // Apply an edit to the live preview WITHOUT a stop/start cycle (rapid
    // start/stop on the MasterTimer thread races and crashes). If the
    // function is already running, just reset its runtime so the next write
    // re-expands values/palettes; otherwise start it (e.g. a scene that
    // auto-stopped while empty, now that it has content).
    if (m_doc->mode() != Doc::Design || isVisible() == false)
        return;
    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
        return;

    if (f->isRunning())
    {
        if (Scene *s = qobject_cast<Scene*>(f))
        {
            s->resetRuntime();
            // Effect palettes are not written by Scene::write(), so resetRuntime
            // doesn't trigger a functionStarted signal. Re-sync EffectInstances
            // explicitly so newly-added/removed Effect palettes take effect.
            if (m_doc->effectScriptRunner())
                m_doc->effectScriptRunner()->syncScene(m_canvasFunction);
        }
        // collections/others reflect their members' own changes
    }
    else
    {
        f->start(m_doc->masterTimer(), FunctionParent::master());
        m_previewFunction = m_canvasFunction;
        updateTitle();
    }
    // Values just changed in the preview; re-estimate after they settle.
    QTimer::singleShot(50, this, &ProgrammingManager::recomputePower);
}

void ProgrammingManager::slotCanvasModified()
{
    // Re-apply the scene's targets/looks to the live preview (resetRuntime
    // if running, else start) — no start/stop churn.
    refreshPreview();
    // Targets/looks changed → the load changed too.
    recomputePower();
}

/****************************************************************************
 * Power / amperage estimate (Design mode only)
 ****************************************************************************/

void ProgrammingManager::buildPowerFooter()
{
    m_powerFooter = new QFrame(this);
    m_powerFooter->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout *fl = new QHBoxLayout(m_powerFooter);
    fl->setContentsMargins(6, 2, 6, 2);

    QLabel *caption = new QLabel(tr("Estimated load:"), m_powerFooter);
    m_powerAmpsLabel = new QLabel(QStringLiteral("0.0 A"), m_powerFooter);
    QFont bold = m_powerAmpsLabel->font();
    bold.setBold(true);
    m_powerAmpsLabel->setFont(bold);
    m_powerKwLabel = new QLabel(QStringLiteral("0.0 kW"), m_powerFooter);
    m_powerKwLabel->setStyleSheet(QStringLiteral("QLabel { color: gray; }"));

    m_powerOverloadLabel = new QLabel(tr("OVERLOAD"), m_powerFooter);
    m_powerOverloadLabel->setStyleSheet(
        QStringLiteral("QLabel { color: white; background: #c0392b; "
                       "padding: 0 6px; border-radius: 3px; font-weight: bold; }"));
    m_powerOverloadLabel->hide();

    m_circuitsBtn = new QPushButton(tr("Circuits…"), m_powerFooter);
    connect(m_circuitsBtn, &QPushButton::clicked,
            this, &ProgrammingManager::slotOpenCircuits);

    fl->addWidget(caption);
    fl->addWidget(m_powerAmpsLabel);
    fl->addWidget(m_powerKwLabel);
    fl->addWidget(m_powerOverloadLabel);
    fl->addStretch(1);
    fl->addWidget(m_circuitsBtn);

    m_canvasLayout->addWidget(m_powerFooter);

    // Low-frequency refresh so a held fader / palette change settles into the
    // reading without us hooking every DMX write. Design-mode-gated below.
    m_powerTimer = new QTimer(this);
    m_powerTimer->setInterval(500);
    connect(m_powerTimer, &QTimer::timeout, this, &ProgrammingManager::recomputePower);
}

void ProgrammingManager::updatePowerFooterActive()
{
    // Footer + timer live only in Design mode while the tab is on screen, so
    // Operate/Run mode pays nothing for this feature.
    const bool active = (m_doc->mode() == Doc::Design) && isVisible();
    if (m_powerFooter != nullptr)
        m_powerFooter->setVisible(active);
    if (m_powerTimer != nullptr)
    {
        if (active)
        {
            if (!m_powerTimer->isActive())
                m_powerTimer->start();
            recomputePower();
        }
        else
        {
            m_powerTimer->stop();
        }
    }
}

void ProgrammingManager::recomputePower()
{
    // Same gate as refreshPreview(): never compute outside Design mode.
    if (m_doc->mode() != Doc::Design || isVisible() == false)
        return;
    if (m_powerAmpsLabel == nullptr)
        return;

    // Take a consistent pre-Grand-Master snapshot of all universes (the
    // "designed peak", free of Grand Master/blackout). claimUniverses() shares
    // the MasterTimer's write mutex; release with changed=false (read-only).
    QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
    PowerEstimator::PreGMReader reader =
        [&universes](quint32 uni, quint32 addr) -> uchar
        {
            if (uni >= quint32(universes.size()))
                return 0;
            return universes[uni]->preGMValue(int(addr));
        };

    QHash<quint32, double> perFixture;
    PowerEstimator::estimateWatts(m_doc, reader, &perFixture);
    m_doc->inputOutputMap()->releaseUniverses(false);

    // Total amps: sum each source's watts / its own voltage (handles mixed
    // voltages), plus any unassigned fixtures at the global default voltage.
    PowerDistribution *pd = m_doc->powerDistribution();
    QSettings settings;
    const double defaultVoltage =
        settings.value(QStringLiteral("power/defaultVoltage"), 120.0).toDouble();
    const double powerFactor =
        settings.value(QStringLiteral("power/powerFactor"), 0.9).toDouble();

    double totalWatts = 0.0;
    double totalAmps = 0.0;
    bool overload = false;
    QSet<quint32> assigned;

    foreach (const PowerSource &src, pd->sources())
    {
        const double srcV = (src.voltage > 0.0) ? src.voltage : defaultVoltage;
        double sourceWatts = 0.0;
        foreach (const PowerCircuit &cir, src.circuits)
        {
            double cw = 0.0;
            foreach (quint32 fxId, cir.fixtures)
            {
                cw += perFixture.value(fxId, 0.0);
                assigned.insert(fxId);
            }
            totalWatts += cw;
            sourceWatts += cw;
            const double camps = cw / cir.effectiveVoltage(srcV);
            totalAmps += camps;
            if (camps > cir.deratedLimit())
                overload = true;
        }
        // A UPS source overloads when its apparent load exceeds its VA rating.
        if (src.isUPS())
        {
            const double va = (powerFactor > 0.0) ? sourceWatts / powerFactor : sourceWatts;
            if (va > src.vaRating)
                overload = true;
        }
    }

    // Fixtures not on any circuit still draw — count them at the default voltage.
    for (QHash<quint32, double>::const_iterator it = perFixture.constBegin();
         it != perFixture.constEnd(); ++it)
    {
        if (assigned.contains(it.key()))
            continue;
        totalWatts += it.value();
        totalAmps += it.value() / defaultVoltage;
    }

    // TEMP power diagnostic: log only when the total moves, plus a sample of the
    // first fixture's intensity-channel preGM values, to see if intensity edits
    // reach the universe the estimate reads.
    static double s_lastWatts = -1.0;
    if (qAbs(totalWatts - s_lastWatts) > 0.5)
    {
        s_lastWatts = totalWatts;
        QString sample;
        QList<Universe*> u2 = m_doc->inputOutputMap()->claimUniverses();
        const QList<Fixture*> fxs = m_doc->fixtures();
        for (int n = 0; n < fxs.size() && n < 3; n++)
        {
            Fixture *fx = fxs.at(n);
            if (!fx || !fx->fixtureMode()) continue;
            QString ch;
            const QVector<QLCChannel*> cs = fx->fixtureMode()->channels();
            for (int i = 0; i < cs.size(); i++)
                if (cs.at(i)->group() == QLCChannel::Intensity)
                    ch += QString(" ch%1=%2").arg(i)
                          .arg(int(u2.at(fx->universe())->preGMValue(fx->address() + i)));
            sample += QString(" [fx%1 w=%2%3]").arg(fx->id())
                      .arg(perFixture.value(fx->id(), 0.0), 0, 'f', 0).arg(ch);
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
        qCritical() << "[PWR] totalWatts=" << totalWatts << "amps=" << totalAmps << sample;
    }

    m_powerAmpsLabel->setText(tr("%1 A").arg(totalAmps, 0, 'f', 1));
    m_powerKwLabel->setText(tr("%1 kW").arg(totalWatts / 1000.0, 0, 'f', 2));
    m_powerOverloadLabel->setVisible(overload);
}

void ProgrammingManager::slotOpenCircuits()
{
    PowerDistributionDialog dlg(m_doc, this);
    dlg.exec();
    // Assignments/ratings may have changed; refresh the footer.
    recomputePower();
}

void ProgrammingManager::startOperateScene()
{
    // In Operate mode the preview can't run, so start the scene directly so all
    // channels (dimmer, colour, etc.) stay live.  A followspot.js effect look on
    // the scene then overrides pan/tilt on joystick moves on top of this.
    //
    // Do NOT guard on f->isRunning(): stopPreview() defers its stop to the next
    // timer tick, so isRunning() is still true here when we're called immediately
    // after stopPreview().  The m_operateFunction guard prevents double-starts.
    if (m_doc->mode() != Doc::Operate)
    {
        qCritical() << "[PM] startOperateScene: skipped (mode is Design)";
        return;
    }
    if (!isVisible())
    {
        qCritical() << "[PM] startOperateScene: skipped (tab not visible)";
        return;
    }
    if (m_operateFunction == m_canvasFunction)
    {
        qCritical() << "[PM] startOperateScene: already running canvas" << m_canvasFunction;
        return;
    }

    Function *f = m_doc->function(m_canvasFunction);
    if (!f)
    {
        qCritical() << "[PM] startOperateScene: canvas" << m_canvasFunction << "not found";
        return;
    }

    qCritical() << "[PM] startOperateScene: starting" << m_canvasFunction << f->name();
    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_operateFunction = m_canvasFunction;
}

void ProgrammingManager::stopOperateScene()
{
    if (m_operateFunction == Function::invalidId()) return;
    Function *f = m_doc->function(m_operateFunction);
    qCritical() << "[PM] stopOperateScene: stopping" << m_operateFunction
                << (f ? f->name() : "(null)");
    if (f) f->stop(FunctionParent::master());
    m_operateFunction = Function::invalidId();
}

void ProgrammingManager::slotModeChanged()
{
    if (m_doc->mode() == Doc::Design)
    {
        // Operate → Design: App::slotModeDesign() calls
        // MasterTimer::stopAllFunctions() (synchronously) BEFORE the mode
        // actually changes, so the operate scene has already been stopped by
        // the time this fires.  Reclassifying it as a still-running preview
        // would leave a dead function flagged as "running" — the stage reverts
        // out of the scene (intensity off) and stays that way until the scene
        // is re-selected (because startPreview()'s "already running" guard then
        // skips the restart).  Clear the stale handles and start a fresh
        // preview.  This is a start only (no stop), so there's no MasterTimer
        // start/stop race — the previous run is already fully stopped.
        qCritical() << "[PM] slotModeChanged → Design:"
                    << "operateFunction=" << m_operateFunction
                    << "previewFunction=" << m_previewFunction
                    << "canvasFunction=" << m_canvasFunction
                    << "currentScene=" << m_currentScene;
        m_operateFunction = Function::invalidId();
        m_previewFunction = Function::invalidId();
        startPreview();
        updatePowerFooterActive(); // back in Design → show footer, start timer

        // Operate → Design: the live follow-spot pin has no meaning while
        // designing — hide it so only the draggable StageTarget remains.
        if (Monitor *mon = Monitor::instance())
            mon->setFollowSpotPin(false, 0.0f, 0.0f);
    }
    else
    {
        // Design → Operate: reclassify the running preview as operate scene.
        // Do NOT stop and restart — same function, no timing gap, lights stay on.
        qCritical() << "[PM] slotModeChanged → Operate:"
                    << "previewFunction=" << m_previewFunction
                    << "operateFunction=" << m_operateFunction
                    << "canvasFunction=" << m_canvasFunction
                    << "currentScene=" << m_currentScene;
        if (m_previewFunction != Function::invalidId())
        {
            Function *f = m_doc->function(m_previewFunction);
            qCritical() << "[PM]   reclassify preview→operate:" << m_previewFunction
                        << (f ? f->name() : "(null)") << "running=" << (f ? f->isRunning() : false);
            m_operateFunction = m_previewFunction;
            m_previewFunction = Function::invalidId();
        }
        else
        {
            qCritical() << "[PM]   no preview function, starting fresh operate scene";
            startOperateScene();  // nothing was running; start fresh
        }
        updatePowerFooterActive(); // Operate → hide footer, stop timer

        // Design → Operate: now the follow-spot pin is meaningful — seed it at
        // the focused scene's aim target so it appears immediately (the gate in
        // slotFollowSpotPinChanged now lets it through).
        if (ProgrammerController *pc = m_doc->programmer())
            pc->seedStageAimFromScene(m_currentScene);
    }
}

void ProgrammingManager::showEvent(QShowEvent *ev)
{
    // Pick up folder/path changes made in the Functions tab while away,
    // preserving the current scene selection.
    m_paletteTree->updateTree();
    m_funcTree->updateTree();
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene != NULL)
    {
        QTreeWidgetItem *it = m_funcTree->functionItem(scene);
        if (it != NULL)
            m_funcTree->setCurrentItem(it);
    }
    // Connect to 2D monitor fixture selection so clicks there match Targets list.
    if (Monitor *mon = Monitor::instance())
        connect(mon, &Monitor::fixturesSelected,
                this, &ProgrammingManager::slotFixturesSelected, Qt::UniqueConnection);
    // Auto-bind joystick axes from I/O → Mapping → Joystick profile if not yet bound.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->autoBindFromProfile();
    startPreview();
    updatePowerFooterActive();
    QWidget::showEvent(ev);
}

void ProgrammingManager::hideEvent(QHideEvent *ev)
{
    qCritical() << "[PMVIS] hideEvent: Programming tab HIDDEN -> stopping preview"
                << "spontaneous=" << ev->spontaneous();
    stopPreview();
    stopOperateScene(); // don't leave a scene outputting when you leave the tab
    if (Monitor *mon = Monitor::instance())
        disconnect(mon, &Monitor::fixturesSelected,
                   this, &ProgrammingManager::slotFixturesSelected);
    updatePowerFooterActive(); // stops the timer / hides the footer
    QWidget::hideEvent(ev);
}

QString ProgrammingManager::selectedFuncFolderPath() const
{
    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    if (sel.isEmpty())
        return QString();
    QTreeWidgetItem *it = sel.first();
    const QString colPath = it->text(1); // COL_PATH
    if (colPath.isEmpty() == false)
        return colPath; // a folder/category is selected
    Function *f = m_doc->function(m_funcTree->itemFunctionId(it));
    return f != NULL ? f->path() : QString();
}

void ProgrammingManager::slotFuncTreeMenu(const QPoint &pos)
{
    QMenu menu(this);
    QAction *aScene  = menu.addAction(tr("New Scene"));
    QAction *aChaser = menu.addAction(tr("New Chaser"));
    QAction *aColl   = menu.addAction(tr("New Collection"));
    QAction *aEFX    = menu.addAction(tr("New EFX"));
    QAction *aMatrix = menu.addAction(tr("New RGB Matrix"));
    menu.addSeparator();
    QAction *aFolder = menu.addAction(tr("New Folder"));
    menu.addSeparator();
    QAction *aRepair = menu.addAction(tr("Repair: strip leading \"NN. \" from names"));

    QAction *chosen = menu.exec(m_funcTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;
    if (chosen == aFolder)
    {
        m_funcTree->addFolder();
        return;
    }
    if (chosen == aRepair)
    {
        repairFunctionNames();
        return;
    }

    Function *f = NULL;
    QString base;
    if (chosen == aScene)       { f = new Scene(m_doc);      base = tr("New Scene"); }
    else if (chosen == aChaser) { f = new Chaser(m_doc);     base = tr("New Chaser"); }
    else if (chosen == aColl)   { f = new Collection(m_doc); base = tr("New Collection"); }
    else if (chosen == aEFX)    { f = new EFX(m_doc);        base = tr("New EFX"); }
    else if (chosen == aMatrix) { f = new RGBMatrix(m_doc);  base = tr("New RGB Matrix"); }
    if (f == NULL)
        return;

    const QString folder = selectedFuncFolderPath();
    if (folder.isEmpty() == false)
        f->setPath(folder);

    if (m_doc->addFunction(f) == false)
    {
        delete f;
        return;
    }
    f->setName(QString("%1 %2").arg(base).arg(f->id()));

    QTreeWidgetItem *it = m_funcTree->functionItem(f);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
    // Selection no longer opens the canvas (double-click does), so open the
    // freshly created function explicitly.
    if (f->type() == Function::SceneType)
        loadCanvas(f->id());
    else
        loadFunctionEditor(f);
}

void ProgrammingManager::slotPaletteTreeMenu(const QPoint &pos)
{
    QMenu menu(this);
    // Type list, like the function tree's New menu. Only the types the
    // engine resolves per-fixture are offered.
    struct { const char *label; int type; } types[] = {
        { QT_TR_NOOP("New Color"),      QLCPalette::Color },
        { QT_TR_NOOP("New Dimmer"),     QLCPalette::Dimmer },
        { QT_TR_NOOP("New Pan/Tilt"),   QLCPalette::PanTilt },
        { QT_TR_NOOP("New Aim"),        QLCPalette::Aim },
        { QT_TR_NOOP("New Beam"),       QLCPalette::Beam },
        { QT_TR_NOOP("New Gobo"),       QLCPalette::Gobo },
        { QT_TR_NOOP("New Shutter"),    QLCPalette::Shutter },
        { QT_TR_NOOP("New Effect"),     QLCPalette::Effect },
    };
    QList<QAction*> newActions;
    for (uint i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        QAction *a = menu.addAction(tr(types[i].label));
        a->setData(types[i].type);
        newActions << a;
    }

    menu.addSeparator();
    QAction *aNewFolder = menu.addAction(tr("New folder…"));

    QAction *aMove = NULL;
    QAction *aDuplicate = NULL;
    const quint32 pid =
        m_paletteTree->itemPaletteId(m_paletteTree->itemAt(pos));
    if (pid != QLCPalette::invalidId())
    {
        aDuplicate = menu.addAction(tr("Duplicate"));
        aMove = menu.addAction(tr("Move to folder…"));
    }

    QAction *chosen = menu.exec(m_paletteTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (newActions.contains(chosen))
    {
        createPalette(chosen->data().toInt());
    }
    else if (chosen == aNewFolder)
    {
        const QString parentPath =
            m_paletteTree->paletteFolderPathFor(m_paletteTree->itemAt(pos));
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("New palette folder"), tr("Folder name:"),
            QLineEdit::Normal, tr("New folder"), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        const QString full = parentPath + "/" + name.trimmed();

        // Move any selected palettes into the new folder so it persists;
        // otherwise just create an (initially empty) folder node to drag into.
        QList<quint32> pids;
        foreach (QTreeWidgetItem *it, m_paletteTree->selectedItems())
        {
            const quint32 id = m_paletteTree->itemPaletteId(it);
            if (id != QLCPalette::invalidId())
                pids << id;
        }
        if (pids.isEmpty() == false)
        {
            foreach (quint32 id, pids)
            {
                QLCPalette *p = m_doc->palette(id);
                if (p != NULL)
                    p->setPath(full);
            }
            m_doc->setModified();
            m_paletteTree->updateTree();
        }
        else
        {
            QTreeWidgetItem *f = m_paletteTree->ensurePaletteFolder(full);
            if (f != NULL)
            {
                m_paletteTree->setCurrentItem(f);
                m_paletteTree->scrollToItem(f);
            }
        }
    }
    else if (chosen == aDuplicate)
    {
        duplicatePalette(pid);
    }
    else if (chosen == aMove)
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL)
            return;
        bool ok = false;
        const QString folder = QInputDialog::getText(
            this, tr("Move palette to folder"),
            tr("Folder path (e.g. \"Shutters\"; empty for none):"),
            QLineEdit::Normal, p->path(), &ok);
        if (!ok)
            return;
        QString path = folder.trimmed();
        // Palette paths are stored with the internal "Palettes/" prefix.
        if (path.isEmpty())
            p->setPath(QString());
        else
            p->setPath(QStringLiteral("Palettes/") + path);
        m_doc->setModified();
        m_paletteTree->updateTree();
    }
}

void ProgrammingManager::syncMemberNodes(quint32 containerId)
{
    // CRITICAL: block the tree's signals for the whole rebuild. The nested
    // member items carry a function id and get prefixed display text ("001.
    // …"); without this, each setText() fires itemChanged -> the tree's rename
    // handler bakes that prefixed text into the real function name (and saves
    // it), which is what produced the runaway "001. 001. 001. …" names.
    const bool wasBlocked = m_funcTree->blockSignals(true);

    // Remove members previously nested under the old container's node. After
    // a tree rebuild the node is fresh (no children), so this is a no-op then.
    if (m_memberContainer != Function::invalidId())
    {
        Function *old = m_doc->function(m_memberContainer);
        if (old != NULL)
        {
            QTreeWidgetItem *n = m_funcTree->functionItem(old);
            if (n != NULL)
                while (n->childCount() > 0)
                    delete n->takeChild(0);
        }
    }

    m_memberContainer = containerId;

    Function *c = m_doc->function(containerId);
    QTreeWidgetItem *node = (c != NULL) ? m_funcTree->functionItem(c) : NULL;
    if (node != NULL)
    {
        // Recursively nest members (chaser -> collections -> scenes -> …).
        QSet<quint32> visited;
        visited.insert(containerId);
        addMemberChildren(node, containerId, visited, 0);
        node->setExpanded(true);
    }

    m_funcTree->blockSignals(wasBlocked);
}

void ProgrammingManager::repairFunctionNames()
{
    // Strip leading zero-padded step-number prefixes ("001. 001. … name") that
    // an earlier bug baked into function names. The prefix always has a space
    // after the dot, so it won't touch genuine names like "00-01.02-Setup".
    QRegularExpression re(QStringLiteral("^(\\s*\\d+\\.\\s+)+"));
    int fixed = 0;
    foreach (Function *f, m_doc->functions())
    {
        if (f == NULL)
            continue;
        const QString cleaned = QString(f->name()).remove(re);
        if (cleaned.isEmpty() == false && cleaned != f->name())
        {
            f->setName(cleaned);
            fixed++;
        }
    }
    if (fixed > 0)
    {
        m_doc->setModified();
        m_funcTree->updateTree();
    }
    QMessageBox::information(this, tr("Repair names"),
        tr("Cleaned leading numbering from %n function name(s).", "", fixed));
}

void ProgrammingManager::addMemberChildren(QTreeWidgetItem *treeNode,
                                           quint32 containerId,
                                           QSet<quint32> &visited, int depth)
{
    if (depth > 8)
        return;
    Function *c = m_doc->function(containerId);
    QList<quint32> members;
    bool ordered = false; // chaser steps have a meaningful order; collections don't
    if (Collection *col = qobject_cast<Collection*>(c))
        members = col->functions();
    else if (Chaser *ch = qobject_cast<Chaser*>(c))
    {
        ordered = true;
        foreach (const ChaserStep &s, ch->steps())
            members << s.fid;
    }
    else
        return; // not a container

    // The tree sorts alphabetically, so for ordered containers (chasers)
    // prefix each child with a zero-padded step number — the sort then
    // matches step order, and the number is useful to see.
    const int width = qMax(2, QString::number(members.count()).length());
    int idx = 0;

    foreach (quint32 mid, members)
    {
        idx++;
        Function *mf = m_doc->function(mid);
        if (mf == NULL)
            continue;
        // Prefix ordered (chaser) members with a zero-padded step number so
        // the alphabetical tree keeps step order — BUT only when the member's
        // own name doesn't already start with a number (users who encode order
        // in the name, e.g. "00-01.02-…", shouldn't get a redundant prefix).
        const QString mname = mf->name();
        const bool nameIsNumbered = mname.isEmpty() == false && mname.at(0).isDigit();
        QTreeWidgetItem *ci = new QTreeWidgetItem(treeNode);
        ci->setText(0, (ordered && !nameIsNumbered)
                    ? QString("%1. %2").arg(idx, width, 10, QChar('0')).arg(mname)
                    : mname);
        ci->setIcon(0, mf->getIcon());
        ci->setData(0, Qt::UserRole, mid); // so itemFunctionId() resolves it
        // Read-only nav: selectable + double-click to edit, not draggable.
        ci->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        // Recurse into sub-containers (collection inside a chaser, etc.),
        // guarding against cycles. Leave them collapsed — expand to drill in.
        if (visited.contains(mid) == false)
        {
            visited.insert(mid);
            addMemberChildren(ci, mid, visited, depth + 1);
        }
    }
}

int ProgrammingManager::functionUsageCount(quint32 fid) const
{
    int n = 0;
    foreach (Function *f, m_doc->functions())
    {
        if (f == NULL)
            continue;
        if (Collection *c = qobject_cast<Collection*>(f))
        {
            if (c->functions().contains(fid))
                n++;
        }
        else if (Chaser *ch = qobject_cast<Chaser*>(f))
        {
            foreach (const ChaserStep &s, ch->steps())
                if (s.fid == fid) { n++; break; }
        }
    }
    return n;
}

/*****************************************************************************
 * Highlight
 *****************************************************************************/

void ProgrammingManager::slotHighlightToggled(bool on)
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc)
        pc->setHighlightActive(on);
}

/*****************************************************************************
 * Followspot
 *****************************************************************************/

void ProgrammingManager::slotFollowSpotBindX()
{
    ProgrammerController *pc = m_doc->programmer();
    if (!pc) return;
    m_fsBindXBtn->setText(tr("Move X axis…"));
    m_fsBindYBtn->setEnabled(false);
    pc->bindPanAxis();
}

void ProgrammingManager::slotFollowSpotBindY()
{
    ProgrammerController *pc = m_doc->programmer();
    if (!pc) return;
    m_fsBindYBtn->setText(tr("Move Y axis…"));
    m_fsBindXBtn->setEnabled(false);
    pc->bindTiltAxis();
}

void ProgrammingManager::slotFollowSpotClearBindings()
{
    ProgrammerController *pc = m_doc->programmer();
    if (!pc) return;
    pc->clearAxisBindings();
}

void ProgrammingManager::slotFollowSpotBindingChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (!pc) return;

    m_fsXLabel->setText(pc->panAxisLabel());
    m_fsYLabel->setText(pc->tiltAxisLabel());

    /* When the binding came from a HID profile, the user doesn't need manual
       Bind buttons — the profile handles it.  Show them only in manual mode. */
    const bool profileBound = pc->isBoundFromProfile();
    m_fsBindXBtn->setVisible(!profileBound);
    m_fsBindYBtn->setVisible(!profileBound);
    m_fsClearBtn->setVisible(!profileBound);

    if (!profileBound)
    {
        m_fsBindXBtn->setText(tr("Bind X (pan)"));
        m_fsBindYBtn->setText(tr("Bind Y (tilt)"));
        m_fsBindXBtn->setEnabled(true);
        m_fsBindYBtn->setEnabled(true);
    }
}

void ProgrammingManager::slotJoystickUpdated(float pan, float tilt)
{
    m_fsJoyLabel->setText(
        QString("Pan %1  Tilt %2")
            .arg(double(pan),  4, 'f', 2)
            .arg(double(tilt), 4, 'f', 2));
}

void ProgrammingManager::slotDesignPositionWritten()
{
    // Enable save button and reset armed state (new edits invalidate a pending confirm).
    if (m_saveBtn)
    {
        m_saveArmed = false;
        m_saveBtn->setText(tr("Save"));
        m_saveBtn->setEnabled(true);
    }
}

void ProgrammingManager::slotFollowSpotPinChanged(bool visible, float xMeters, float yMeters)
{
    // The follow-spot PIN is the LIVE beam position the operator drives — it
    // belongs to Operate mode only.  In Design you set the aim by dragging the
    // StageTarget marker (which stays visible), so the pin must never appear
    // while building looks.  This is the single choke point for every pin-show
    // path (scene focus, joystick move, …), so gate it here once.
    if (m_doc->mode() != Doc::Operate)
        visible = false;
    if (Monitor *mon = Monitor::instance())
        mon->setFollowSpotPin(visible, xMeters, yMeters);
}

void ProgrammingManager::slotSavePositions()
{
    if (!m_saveBtn) return;

    if (!m_saveArmed)
    {
        // First press: arm — user must click again within 3 s to confirm.
        m_saveArmed = true;
        m_saveBtn->setText(tr("Confirm? (click again)"));
        // Auto-disarm after 3 seconds if the user doesn't confirm.
        QTimer::singleShot(3000, this, [this]() {
            if (m_saveArmed)
            {
                m_saveArmed = false;
                if (m_saveBtn) m_saveBtn->setText(tr("Save"));
            }
        });
    }
    else
    {
        // Second press: confirmed — commit joystick position then save workspace.
        m_saveArmed = false;
        m_saveBtn->setText(tr("Save"));
        m_saveBtn->setEnabled(false);
        ProgrammerController *pc = m_doc->programmer();
        if (pc) pc->commitDesignJoystick();
        emit requestSave();
    }
}

// ---------------------------------------------------------------------------
// BPM / internal beat generator
// ---------------------------------------------------------------------------

void ProgrammingManager::slotBpmToggled(bool on)
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (!ioMap) return;

    if (on)
    {
        ioMap->setBeatGeneratorType(InputOutputMap::Internal);
        ioMap->setBpmNumber(m_bpmSpin->value());
        m_bpmSpin->setEnabled(true);
        m_tapBtn->setEnabled(true);
    }
    else
    {
        ioMap->setBeatGeneratorType(InputOutputMap::Disabled);
        m_bpmSpin->setEnabled(false);
        m_tapBtn->setEnabled(false);
        m_tapActive = false;
    }
}

void ProgrammingManager::slotBpmChanged(int bpm)
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (!ioMap || !m_bpmBtn->isChecked()) return;
    ioMap->setBpmNumber(bpm);
}

void ProgrammingManager::slotTapBeat()
{
    if (!m_tapActive)
    {
        // First tap — just start the clock
        m_tapTimer.start();
        m_tapActive = true;
        return;
    }

    qint64 elapsed = m_tapTimer.elapsed();
    m_tapTimer.restart();

    if (elapsed < 200 || elapsed > 3000)
    {
        // Outside 20–300 BPM range; treat as a fresh first tap
        m_tapActive = false;
        return;
    }

    int bpm = qRound(60000.0 / (double)elapsed);
    bpm = qBound(20, bpm, 400);
    m_bpmSpin->setValue(bpm);  // triggers slotBpmChanged via valueChanged
}

void ProgrammingManager::slotButtonAction(const QString &action)
{
    ProgrammerController *pc = m_doc->programmer();

    if (action == QLatin1String("followspot_toggle"))
    {
        // Followspot is now the followspot.js effect look attached to a scene,
        // not a separately-toggled native effect.  Button retained as a no-op
        // until it is repurposed (e.g. attach/detach the effect look).
        return;
    }

    if (action == QLatin1String("highlight_toggle"))
    {
        if (pc && m_highlightBtn)
            pc->setHighlightActive(!m_highlightBtn->isChecked());
        return;
    }

    if (action == QLatin1String("programmer_clear"))
    {
        if (pc)
            pc->clearProgrammerValues();
        return;
    }

    /* Step chaser forward or back: act on all currently running chasers */
    ChaserActionType stepType = ChaserNoAction;
    if (action == QLatin1String("chaser_step_forward"))
        stepType = ChaserNextStep;
    else if (action == QLatin1String("chaser_step_back"))
        stepType = ChaserPreviousStep;

    if (stepType == ChaserNoAction)
        return;

    ChaserAction chaserAction;
    chaserAction.m_action = stepType;
    chaserAction.m_masterIntensity = 1.0;
    chaserAction.m_stepIntensity = 1.0;
    chaserAction.m_fadeMode = 0;
    chaserAction.m_stepIndex = -1;

    foreach (Function *f, m_doc->functions())
    {
        Chaser *ch = qobject_cast<Chaser*>(f);
        if (ch && ch->isRunning())
            ch->setAction(chaserAction);
    }
}
