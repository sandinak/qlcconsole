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
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QFrame>
#include <QSettings>
#include <algorithm>

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
#include "track.h"
#include "showfunction.h"
#include "efx.h"
#include "rgbmatrix.h"
#include "chasereditor.h"
#include "collectioneditor.h"
#include "efxeditor.h"
#include "rgbmatrixeditor.h"
#include "show.h"
#include "showmanager/showtimelineeditor.h"
#include "fixturegroupeditor.h"
#include "fixturegroup.h"
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
#include "bundlecache.h"
#include "bundlebrowser.h"
#include "bundleeditor.h"
#include "qlcbundle.h"
#include "monitor/monitor.h"
#include <QTabWidget>

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
    // Order the top-level type categories by role (Show · Chaser · Collection ·
    // RGB Matrix · EFX · Scene) rather than A–Z — persisted, toggle in the
    // right-click menu.
    {
        QSettings s;
        m_funcTree->setTypeOrderByRole(
            s.value(QStringLiteral("programming/functree/typeOrderByRole"), true).toBool());
    }
    m_funcTree->setContextMenuPolicy(Qt::CustomContextMenu);
    // Shift/Ctrl-click to select several functions at once, then drag, delete or
    // duplicate them together. mimeData() already streams every selected id.
    m_funcTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
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

        m_flashBtn = new QPushButton(tr("Flash"), this);
        m_flashBtn->setToolTip(tr("Momentarily flash the selected fixtures to identify them in the rig"));
        m_flashBtn->setEnabled(false);
        toolbar->addWidget(m_flashBtn);

        m_parkBtn = new QPushButton(tr("Park"), this);
        m_parkBtn->setToolTip(tr("Hold the selected fixtures at their current values, out of cue "
                                 "output, until unparked (saved with the workspace)"));
        m_parkBtn->setEnabled(false);
        toolbar->addWidget(m_parkBtn);

        m_unparkBtn = new QPushButton(tr("Unpark all"), this);
        m_unparkBtn->setToolTip(tr("Release every parked fixture"));
        m_unparkBtn->setEnabled(false);
        toolbar->addWidget(m_unparkBtn);

        m_markBtn = new QPushButton(tr("Mark"), this);
        m_markBtn->setToolTip(tr("Move-in-black: pre-position the selected DARK fixtures "
                                 "(pan/tilt, colour, gobo, beam) to where they'll be revealed, "
                                 "held out of cue output. Auto-releases when a cue lights them; "
                                 "saved with the workspace."));
        m_markBtn->setEnabled(false);
        toolbar->addWidget(m_markBtn);

        m_unmarkBtn = new QPushButton(tr("Unmark all"), this);
        m_unmarkBtn->setToolTip(tr("Release every marked fixture"));
        m_unmarkBtn->setEnabled(false);
        toolbar->addWidget(m_unmarkBtn);

        m_autoMibBtn = new QPushButton(tr("Auto MIB"), this);
        m_autoMibBtn->setCheckable(true);
        m_autoMibBtn->setToolTip(tr("Automatic move-in-black: while a chaser or the show "
                                    "timeline runs, look ahead to the next cue and pre-position "
                                    "any dark mover it will reveal — as long as there's enough "
                                    "lead time. Releases each fixture as its cue lights it."));
        toolbar->addWidget(m_autoMibBtn);

        m_mibGapSpin = new QDoubleSpinBox(this);
        m_mibGapSpin->setRange(0.1, 10.0);
        m_mibGapSpin->setSingleStep(0.1);
        m_mibGapSpin->setDecimals(1);
        m_mibGapSpin->setSuffix(tr(" s lead"));
        m_mibGapSpin->setToolTip(tr("Move-in-black lead time: the next cue must be at least this "
                                    "far away for a dark mover to be pre-set. Below it there's no "
                                    "time to move in black, so it's skipped."));
        toolbar->addWidget(m_mibGapSpin);

        m_snapshotBtn = new QPushButton(tr("Snapshot"), this);
        m_snapshotBtn->setToolTip(tr("Bake the current live DMX output into the open "
            "scene as static values — capture a look built on an external console."));
        connect(m_snapshotBtn, SIGNAL(clicked()), this, SLOT(slotSnapshotLive()));
        toolbar->addWidget(m_snapshotBtn);

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
        m_bpmBtn->setToolTip(tr("Enable QLC+ internal beat generator (drives _beat/_bpm in generators)"));
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

        // The row above packs ~15 controls (Highlight/Flash/Park/Unpark/Mark/
        // Unmark/Auto MIB/Snapshot/BPM/Tap/Beat On/Jog/Save) into one
        // QHBoxLayout with no wrapping, so their summed minimum widths (~1866px)
        // become this panel's minimum — and since QTabWidget's minimum size is
        // the max over all pages, that drags the WHOLE app window wider than
        // most screens and makes it unresizable. Wrap it in a horizontally
        // scrollable, frameless container so it keeps its natural minimum
        // width but doesn't force the rest of the layout to match.
        QWidget *toolbarWidget = new QWidget(this);
        toolbarWidget->setLayout(toolbar);

        QScrollArea *toolbarScroll = new QScrollArea(this);
        toolbarScroll->setWidget(toolbarWidget);
        toolbarScroll->setWidgetResizable(false);
        toolbarScroll->setFrameShape(QFrame::NoFrame);
        toolbarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        toolbarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        toolbarScroll->setFixedHeight(toolbarWidget->sizeHint().height() + 4);

        m_canvasLayout->addWidget(toolbarScroll);

        // Blind-active banner: full-width blue bar pinned at the very top of the
        // canvas (above the toolbar) so "the rig is muted" can't be missed while
        // the 2D preview keeps showing the look. Hidden until Blind is armed.
        m_blindBanner = new QLabel(tr("⬤  BLIND — rig muted, preview only.  "
                                      "Turn off Blind to take the look live."), this);
        m_blindBanner->setAlignment(Qt::AlignCenter);
        m_blindBanner->setStyleSheet(
            QStringLiteral("QLabel { background: #1565c0; color: white; font-weight: bold; "
                           "padding: 4px; border-radius: 3px; }"));
        m_blindBanner->hide();
        m_canvasLayout->insertWidget(0, m_blindBanner);

        connect(m_highlightBtn, &QPushButton::toggled,
                this, &ProgrammingManager::slotHighlightToggled);
        connect(m_flashBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotFlashSelection);
        // Blind is armed from the global toolbar (App); reflect the engine state
        // in the canvas banner so building in-tab shows the muted-rig warning.
        connect(m_doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged,
                this, &ProgrammingManager::slotBlindStateChanged);
        connect(m_parkBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotParkSelection);
        connect(m_unparkBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotUnparkAll);
        connect(m_markBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotMarkSelection);
        connect(m_unmarkBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotUnmarkAll);
        connect(m_autoMibBtn, &QPushButton::toggled, this, [this](bool on) {
            if (ProgrammerController *pc = m_doc->programmer())
                pc->setAutoMoveInBlack(on);
        });
        connect(m_mibGapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double s) {
            if (ProgrammerController *pc = m_doc->programmer())
                pc->setMoveInBlackDarkGapMs(int(s * 1000.0 + 0.5));
        });
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
            connect(pc, &ProgrammerController::parkChanged,
                    this, &ProgrammingManager::slotParkChanged);
            connect(pc, &ProgrammerController::markChanged,
                    this, &ProgrammingManager::slotMarkChanged);
            connect(pc, &ProgrammerController::autoMoveInBlackChanged, this,
                    [this, pc](bool on) {
                QSignalBlocker b1(m_autoMibBtn), b2(m_mibGapSpin);
                m_autoMibBtn->setChecked(on);
                m_mibGapSpin->setValue(pc->moveInBlackDarkGapMs() / 1000.0);
            });
            // Seed from the (possibly already-loaded) workspace.
            {
                QSignalBlocker b1(m_autoMibBtn), b2(m_mibGapSpin);
                m_autoMibBtn->setChecked(pc->isAutoMoveInBlack());
                m_mibGapSpin->setValue(pc->moveInBlackDarkGapMs() / 1000.0);
            }
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
    // FixtureConsole lays out one ConsoleChannel per DMX channel in a plain
    // QHBoxLayout with no wrap — a many-channel fixture (e.g. a 20+ channel
    // moving head) makes this scroll area's un-scrolled minimumSizeHint very
    // wide, which (with widgetResizable=true and no policy override) would
    // otherwise leak straight into the whole main window's minimum width the
    // moment a fixed-fixture target is selected. Ignored + a small floor
    // keeps the horizontal scrollbar doing its job without pinning the
    // window's minimum size to whatever fixture happens to be selected.
    m_fixtureScroll->setSizePolicy(QSizePolicy::Ignored, m_fixtureScroll->sizePolicy().verticalPolicy());
    m_fixtureScroll->setMinimumWidth(150);
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
    // ── Source panel: tabbed (Palettes | Bundles | Fixtures) ─────────────
    auto *sourceTab = new QTabWidget(this);
    sourceTab->setDocumentMode(true);

    // ── Palettes tab ──────────────────────────────────────────────────────
    QWidget *palTab = new QWidget(sourceTab);
    QVBoxLayout *palCol = new QVBoxLayout(palTab);
    palCol->setContentsMargins(2, 2, 2, 2);
    QLineEdit *paletteFilter = new QLineEdit(palTab);
    paletteFilter->setPlaceholderText(tr("Filter…"));
    paletteFilter->setClearButtonEnabled(true);
    palCol->addWidget(paletteFilter);
    m_paletteTree = new FunctionsTreeWidget(m_doc, palTab);
    m_paletteTree->setDisplayFilter(FunctionsTreeWidget::PalettesOnly);
    m_paletteTree->setHeaderHidden(true);
    m_paletteTree->setColumnHidden(1, true);
    m_paletteTree->setSortingEnabled(true);
    m_paletteTree->sortByColumn(0, Qt::AscendingOrder);
    m_paletteTree->setExternalDragMode(true);
    m_paletteTree->setContextMenuPolicy(Qt::CustomContextMenu);
    // Multi-select so several palettes can be dragged onto a look together, or
    // saved as a Bundle in one action from the context menu.
    m_paletteTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    organizeEffectPalettes();
    m_paletteTree->updateTree();
    palCol->addWidget(m_paletteTree, 1);
    palCol->addWidget(new QLabel(tr("Right-click to add a palette."), palTab));
    connect(paletteFilter, &QLineEdit::textChanged,
            m_paletteTree, &FunctionsTreeWidget::filterByText);
    sourceTab->addTab(palTab, tr("Palettes"));

    // ── Bundles tab ───────────────────────────────────────────────────────
    m_bundleCache = new BundleCache(this);
    m_bundleCache->rescan();
    m_bundleBrowser = new BundleBrowser(m_doc, m_bundleCache, sourceTab);
    connect(m_bundleBrowser, &BundleBrowser::stampRequested,
            this, &ProgrammingManager::slotStampBundle);
    connect(m_bundleBrowser, &BundleBrowser::saveAsBundleRequested,
            this, &ProgrammingManager::slotSaveAsBundle);
    sourceTab->addTab(m_bundleBrowser, tr("Bundles"));

    // ── Right column: tabs on top, fixtures below (always visible) ────────
    QWidget *sourcePanel = new QWidget(this);
    QVBoxLayout *srcCol  = new QVBoxLayout(sourcePanel);
    srcCol->setContentsMargins(0, 0, 0, 0);
    srcCol->setSpacing(2);
    srcCol->addWidget(sourceTab, 3);

    srcCol->addWidget(new QLabel(tr("Fixtures & groups"), sourcePanel));
    m_fixGroupSource = new FixtureGroupSource(m_doc, sourcePanel);
    srcCol->addWidget(m_fixGroupSource, 2);
    connect(m_fixGroupSource, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotFixGroupSourceSelectionChanged);
    // Double-click a group in the source tree -> visualize/edit its head layout
    // in the central canvas.
    connect(m_fixGroupSource, &FixtureGroupSource::groupDoubleClicked,
            this, &ProgrammingManager::loadGroupEditor);

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

    // Drag scene(s) / other collection(s) onto a collection node in the nav tree
    // to add them as members. Deferred for the same reason as above (the tree is
    // rebuilt, invalidating the drag's item pointers).
    connect(m_funcTree, &FunctionsTreeWidget::functionsDroppedOnCollection,
            this, [this](quint32 containerId, const QList<quint32> &fids) {
        QTimer::singleShot(0, this,
            [this, containerId, fids]() { addFunctionsToCollection(containerId, fids); });
    });

    // Keep the trees in sync with the Doc (functor connects so we can call
    // the trees' non-slot helpers directly).
    connect(m_doc, &Doc::functionAdded, m_funcTree, &FunctionsTreeWidget::addFunction);
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
        // The canvas (SceneGroupLooks), the look editor and the chaser/
        // collection editor all hold a raw Function*/Scene*. Doc emits this
        // BEFORE it deletes the function, so tear them down now — otherwise
        // the next canvas interaction dereferences freed memory.
        if (id == m_currentScene || id == m_canvasFunction)
            loadCanvas(Function::invalidId());

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
    connect(m_doc, &Doc::loaded, this, [this]() {
        organizeEffectPalettes();
        m_paletteTree->updateTree();
    });
    connect(m_doc, &Doc::cleared, m_paletteTree, &FunctionsTreeWidget::updateTree);

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
    connect(m_lookEditor, SIGNAL(paletteChanged(quint32)),
            this, SLOT(slotLookEdited()));
    // Lightweight live-value change (effect params, etc.) — only refresh the
    // DMX preview, do NOT rebuild the look list or palette tree.  This avoids
    // a reload() → lookSelected → setPalette → rebuildEffectDynWidget cycle
    // that was causing all effect sliders to jump when any one slider moved.
    connect(m_lookEditor, &LookEditor::paletteValueChanged,
            this, [this](quint32 pid) {
        // Push edited Effect params onto the LIVE instance so a param change
        // (axis, Learn'd note range, a slider) takes effect immediately, WITHOUT
        // recreating the instance (which would reset the effect's JS state). The
        // sync-signature path only reloads on a script SWAP, not a param edit.
        if (m_doc->effectScriptRunner())
            m_doc->effectScriptRunner()->updateEffectParams(pid);
        refreshPreview();
    });
    // Refresh canvas palette tiles when a palette is renamed in the tree.
    connect(m_paletteTree, &FunctionsTreeWidget::paletteRenamed,
            this, [this](quint32) { if (m_canvas) m_canvas->reload(); });
    // An auto-rename (e.g. Effect → its effect's name) re-sorts the tree; rebuild
    // then re-reveal the item so it stays findable. Deferred so the rename's own
    // tree refresh settles first.
    connect(m_lookEditor, &LookEditor::paletteRenamed, this, [this](quint32 id) {
        QTimer::singleShot(0, this, [this, id]() {
            m_paletteTree->updateTree();
            revealPalette(id);
        });
    });

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

    // Ctrl-Z undoes the last bundle stamp.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &ProgrammingManager::slotUndo);
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
        qDebug() << "[PM] loadCanvas:" << sceneId << (f ? f->name() : "(null)")
                    << "mode=" << (m_doc->mode() == Doc::Design ? "Design" : "Operate")
                    << "previewFn=" << m_previewFunction
                    << "operateFn=" << m_operateFunction;
    }
    if (sceneId == m_currentScene && m_canvas != NULL)
        return;

    stopPreview();
    m_currentScene = sceneId;
    m_canvasFunction = sceneId;
    m_lastSyncedEffectPalettes.clear(); // new scene — force re-sync on first refreshPreview
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
    resetUndoFor(sceneId);   // start a fresh undo history for this scene

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
    // The look editor outlives the canvas, so drop its Scene* here too — it
    // null-checks the pointer everywhere, but a stale one is a use-after-free.
    m_lookEditor->setContextScene(NULL);
    m_doc->setFocusedScene(Function::invalidId());
    m_doc->setFocusedPalette(QLCPalette::invalidId());
}

void ProgrammingManager::showFunction(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (f != NULL)
        loadFunctionEditor(f);
}

void ProgrammingManager::loadFunctionEditor(Function *f)
{
    if (f == NULL)
        return;

    stopPreview();
    m_currentScene = Function::invalidId();
    resetUndoFor(Function::invalidId());  // scene undo doesn't apply to func editors
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
    case Function::ShowType:
        // Embedded timeline: build the show additively by dragging functions
        // from the left tree onto tracks (byte-compatible drag payload).
        ed = new ShowTimelineEditor(this, qobject_cast<Show*>(f), m_doc);
        dragIn = true;
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
    // A Show owns its own transport (Play/Stop in the timeline editor) — never
    // auto-play it on open. Other editors preview live so the view reflects them.
    if (f->type() != Function::ShowType)
        startPreview();
    updateTitle();
}

void ProgrammingManager::loadGroupEditor(quint32 groupId)
{
    FixtureGroup *grp = m_doc->fixtureGroup(groupId);
    if (grp == NULL)
        return;

    stopPreview();
    m_currentScene = Function::invalidId();
    resetUndoFor(Function::invalidId());
    m_canvasFunction = Function::invalidId(); // nothing previewable
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(Function::invalidId());

    m_funcTree->setExternalDragMode(false);
    syncMemberNodes(Function::invalidId());
    clearEditors();

    m_canvasPlaceholder->hide();
    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();
    m_lookEditor->hide(); // group layout editor stands alone in the canvas

    // Reuse the func-editor slot as the hosted canvas widget so clearEditors()
    // tears it down on the next selection.
    m_funcEditor = new FixtureGroupEditor(grp, m_doc, this);
    m_canvasLayout->insertWidget(m_canvasLayout->indexOf(m_lookEditor), m_funcEditor, 1);
    m_funcEditor->show();

    m_canvasTitle->setText(tr("Fixture group: %1   —   %n head(s)", "", grp->headList().count())
                           .arg(grp->name()));
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

bool ProgrammingManager::organizeEffectPalettes()
{
    if (m_doc->effectScriptRunner() == NULL)
        return false;
    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();

    bool changed = false;
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p == NULL || p->type() != QLCPalette::Effect || p->scriptPath().isEmpty())
            continue;

        // Only auto-organize palettes still on the BARE default folder; never
        // touch a folder the user (or a prior pick) already set.
        QString norm = p->path();
        if (norm.endsWith('/')) norm.chop(1);
        if (!(norm.isEmpty() || norm == QLatin1String("Palettes/Effect")))
            continue;

        const QString base = scache->nameFromPath(p->scriptPath());
        const EffectScriptCache::ScriptMeta meta = scache->scriptMeta(base);
        const QString engine = meta.displayName.isEmpty() ? base : meta.displayName;
        const QString cat    = EffectScriptCache::categoryForTypes(meta.fixtureTypes);
        if (engine.isEmpty() || cat.isEmpty())
            continue;

        const QString folder = QString("Palettes/Effect/%1/%2/").arg(cat, engine);
        if (p->path() != folder)
        {
            p->setPath(folder);
            changed = true;
        }
    }
    return changed;   // cosmetic: persists when the user next saves
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
    case QLCPalette::Strobe:  p->setValue(0.25f); break; // default to 25% rate
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
    revealPalette(p->id());            // select + scroll to it in the source tree
    m_lookEditor->setPalette(p->id()); // edit it inline immediately
    showLookEditorPanel();
}

void ProgrammingManager::revealPalette(quint32 paletteId)
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == NULL)
        return;
    QTreeWidgetItem *item = m_paletteTree->paletteItem(p);
    if (item == NULL)
        return;
    // Expand the folder chain so the item is visible, then select + centre it.
    for (QTreeWidgetItem *a = item->parent(); a != NULL; a = a->parent())
        a->setExpanded(true);
    m_paletteTree->setCurrentItem(item);
    m_paletteTree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
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
    if (m_flashBtn)
        m_flashBtn->setEnabled(!fixtureIds.isEmpty());
    updateParkButtonLabel();
    updateMarkButtonLabel();
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
        // Honour the colour-set offset so stacked colour looks show on the right
        // sets (primary/secondary/…) in the console preview, matching runtime.
        const int cOff = QLCPalette::colorSetOffset(m_doc, s->palettes(), pid);
        foreach (const SceneValue &scv, pal->valuesFromFixtureGroups(m_doc, s->fixtureGroups(), nullptr, cOff))
            if (scv.fxi == lead)
            {
                m_fixtureConsole->setSceneValue(scv);
                lockedChannels << scv.channel;
            }
        foreach (const SceneValue &scv, pal->valuesFromFixtures(m_doc, s->fixtures(), nullptr, cOff))
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
        qDebug() << "[PM] startPreview: skipped (mode is Operate)";
        return;
    }
    if (isVisible() == false)
    {
        qDebug() << "[PM] startPreview: skipped (tab not visible)";
        return;
    }
    if (m_previewFunction == m_canvasFunction)
    {
        qDebug() << "[PM] startPreview: already running canvas" << m_canvasFunction;
        return;
    }

    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
    {
        qDebug() << "[PM] startPreview: canvas" << m_canvasFunction << "not found";
        return;
    }

    qDebug() << "[PM] startPreview: starting" << m_canvasFunction << f->name();
    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_previewFunction = m_canvasFunction;
    // Seed the Effect-palette cache so the first canvas modification
    // (e.g. dropping a target group) doesn't falsely see the set as
    // "changed" and call syncScene(), which would tear down JS instances.
    m_lastSyncedEffectPalettes = effectPaletteSignature(qobject_cast<Scene*>(f));
    updateTitle();
}

void ProgrammingManager::stopPreview()
{
    if (m_previewFunction == Function::invalidId())
        return;
    Function *f = m_doc->function(m_previewFunction);
    qDebug() << "[PM] stopPreview: stopping" << m_previewFunction
                << (f ? f->name() : "(null)");
    if (f != NULL)
        f->stop(FunctionParent::master());
    m_previewFunction = Function::invalidId();
    updateTitle();
}

QStringList ProgrammingManager::effectPaletteSignature(Scene *scene) const
{
    QStringList sig;
    if (scene == NULL)
        return sig;
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL || p->type() != QLCPalette::Effect)
            continue;
        // id + script identity: a script swap on the same palette id must read as
        // "changed" so the running instance reloads; a param drag must not.
        sig << QString::number(pid) + QLatin1Char('|') + p->scriptPath()
               + QLatin1Char('|') + p->effectPreset();
    }
    return sig;
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
            // Only re-sync EffectInstances when the set of Effect-type palettes
            // actually changed. Non-Effect palette edits (Shutter, Color, Dimmer)
            // must NOT recreate instances — that resets JS state and breaks
            // continuously running effects (candle flicker loses its phase, etc.).
            if (m_doc->effectScriptRunner())
            {
                const QStringList effectPals = effectPaletteSignature(s);
                if (effectPals != m_lastSyncedEffectPalettes)
                {
                    m_lastSyncedEffectPalettes = effectPals;
                    m_doc->effectScriptRunner()->syncScene(m_canvasFunction);
                }
            }
        }
        // collections/others reflect their members' own changes
    }
    else
    {
        f->start(m_doc->masterTimer(), FunctionParent::master());
        m_previewFunction = m_canvasFunction;
        // Seed the cache with what the scene already has so the first
        // canvas modification (e.g. dropping a target group) doesn't
        // incorrectly see "effect palette set changed" and call syncScene().
        m_lastSyncedEffectPalettes = effectPaletteSignature(qobject_cast<Scene*>(f));
        updateTitle();
    }
    // NOTE: do NOT schedule a power recompute here. refreshPreview() runs on
    // every live edit — including every tick of a slider drag — and a per-call
    // QTimer::singleShot() queued dozens of overlapping recomputePower() passes.
    // Each claims the universe write mutex (contending with the 50 Hz DMX thread)
    // and estimates watts over every fixture, flooding the GUI thread and
    // starving widget repaints (param sliders rendered stale handles until a full
    // repaint). The repeating m_powerTimer (500 ms, Design-mode-gated) already
    // keeps the footer fresh; structural changes call recomputePower() directly.
}

void ProgrammingManager::slotCanvasModified()
{
    // Record the pre-edit state for Ctrl+Z (the baseline still holds the state
    // from before this edit; push it and re-capture).
    pushUndoSnapshot();
    // Re-apply the scene's targets/looks to the live preview (resetRuntime
    // if running, else start) — no start/stop churn.
    refreshPreview();
    // Re-populate the fixture console: a look add / remove / REORDER changes the
    // channel values the selected fixture outputs, but the console is otherwise
    // only rebuilt on a fixture-selection change — so a reorder would leave the
    // sliders stale. Re-run the prefill for the current selection.
    if (!m_selectedFixtures.isEmpty())
        slotFixturesSelected(m_selectedFixtures);
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
    // The estimate now lives in the app status bar (always visible in Design
    // mode), so the timer runs whenever we're in Design mode — regardless of
    // which tab is on screen. The old in-tab footer is retired.
    const bool active = (m_doc->mode() == Doc::Design);
    if (m_powerFooter != nullptr)
        m_powerFooter->setVisible(false);
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
            emit powerEstimateChanged(0.0, 0.0, false);   // clear the chip in Operate
        }
    }
}

void ProgrammingManager::recomputePower()
{
    // Design mode only (the estimate is a "designed peak"); runs across tabs now
    // that the readout is in the always-visible status bar.
    if (m_doc->mode() != Doc::Design)
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

    m_powerAmpsLabel->setText(tr("%1 A").arg(totalAmps, 0, 'f', 1));
    m_powerKwLabel->setText(tr("%1 kW").arg(totalWatts / 1000.0, 0, 'f', 2));
    m_powerOverloadLabel->setVisible(overload);

    // Push the same estimate to the app's always-visible status-bar chip.
    emit powerEstimateChanged(totalAmps, totalWatts / 1000.0, overload);
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
        qDebug() << "[PM] startOperateScene: skipped (mode is Design)";
        return;
    }
    if (!isVisible())
    {
        qDebug() << "[PM] startOperateScene: skipped (tab not visible)";
        return;
    }
    if (m_operateFunction == m_canvasFunction)
    {
        qDebug() << "[PM] startOperateScene: already running canvas" << m_canvasFunction;
        return;
    }

    Function *f = m_doc->function(m_canvasFunction);
    if (!f)
    {
        qDebug() << "[PM] startOperateScene: canvas" << m_canvasFunction << "not found";
        return;
    }

    qDebug() << "[PM] startOperateScene: starting" << m_canvasFunction << f->name();
    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_operateFunction = m_canvasFunction;
}

void ProgrammingManager::stopOperateScene()
{
    if (m_operateFunction == Function::invalidId()) return;
    Function *f = m_doc->function(m_operateFunction);
    qDebug() << "[PM] stopOperateScene: stopping" << m_operateFunction
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
        qDebug() << "[PM] slotModeChanged → Design:"
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
        // (Blind's Design-only force-off is owned by App::slotModeChanged, which
        // clears the global output inhibit; the banner follows via the signal.)

        // Design → Operate: reclassify the running preview as operate scene.
        // Do NOT stop and restart — same function, no timing gap, lights stay on.
        qDebug() << "[PM] slotModeChanged → Operate:"
                    << "previewFunction=" << m_previewFunction
                    << "operateFunction=" << m_operateFunction
                    << "canvasFunction=" << m_canvasFunction
                    << "currentScene=" << m_currentScene;
        if (m_previewFunction != Function::invalidId())
        {
            Function *f = m_doc->function(m_previewFunction);
            qDebug() << "[PM]   reclassify preview→operate:" << m_previewFunction
                        << (f ? f->name() : "(null)") << "running=" << (f ? f->isRunning() : false);
            m_operateFunction = m_previewFunction;
            m_previewFunction = Function::invalidId();
        }
        else
        {
            qDebug() << "[PM]   no preview function, starting fresh operate scene";
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
    slotParkChanged(); // reflect any parks restored from the workspace
    // Blind survives tab switches — reflect the current engine state on return.
    slotBlindStateChanged(m_doc->inputOutputMap()->outputInhibited());
    QWidget::showEvent(ev);
}

void ProgrammingManager::hideEvent(QHideEvent *ev)
{
    qDebug() << "[PMVIS] hideEvent: Programming tab HIDDEN -> stopping preview"
                << "spontaneous=" << ev->spontaneous();
    stopPreview();
    stopOperateScene(); // don't leave a scene outputting when you leave the tab
    // NOTE: Blind is intentionally left engaged across tab switches — it's a
    // global output mute with a persistent status-bar chip, so it stays visible
    // and safe even when this tab isn't shown. It's still forced off on →Operate.
    if (Monitor *mon = Monitor::instance())
        disconnect(mon, &Monitor::fixturesSelected,
                   this, &ProgrammingManager::slotFixturesSelected);
    updatePowerFooterActive(); // stops the timer / hides the footer
    QWidget::hideEvent(ev);
}

QString ProgrammingManager::funcFolderPathFor(QTreeWidgetItem *it) const
{
    if (it == NULL)
        return QString();
    const QString colPath = it->text(1); // COL_PATH
    if (colPath.isEmpty() == false)
        return colPath; // a folder/category row
    Function *f = m_doc->function(m_funcTree->itemFunctionId(it));
    return f != NULL ? f->path() : QString();
}

QString ProgrammingManager::selectedFuncFolderPath() const
{
    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    return sel.isEmpty() ? QString() : funcFolderPathFor(sel.first());
}

void ProgrammingManager::slotFuncTreeMenu(const QPoint &pos)
{
    // Which function(s) is this menu acting on? (Right-click doesn't change the
    // selection, so resolve the target from the item under the cursor.)
    QTreeWidgetItem *clickedItem = m_funcTree->itemAt(pos);
    const QList<quint32> targetIds = funcTreeTargetIds(clickedItem);
    // A new function is created in the folder the user clicked in, not wherever
    // the selection happened to be.
    const QString clickedFolder = funcFolderPathFor(clickedItem);

    QMenu menu(this);

    // Duplicate / Delete first, but only when a function is actually targeted —
    // on empty space or a folder the menu is just the "New …" palette.
    QAction *aDuplicate = NULL;
    QAction *aDelete    = NULL;
    if (!targetIds.isEmpty())
    {
        aDuplicate = menu.addAction(targetIds.size() > 1
                                        ? tr("Duplicate %1 Functions").arg(targetIds.size())
                                        : tr("Duplicate"));
        aDelete = menu.addAction(targetIds.size() > 1
                                     ? tr("Delete %1 Functions").arg(targetIds.size())
                                     : tr("Delete"));
        menu.addSeparator();
    }

    // Static MIDI trigger (Control Map — Phase 0): bind a controller button/pad
    // straight to this function, no Virtual Console widget required.
    QAction *aLearnMidi = NULL;
    QAction *aClearMidi = NULL;
    if (targetIds.size() == 1)
    {
        ProgrammerController *pc = m_doc->programmer();
        const quint32 fid = targetIds.first();
        aLearnMidi = menu.addAction(tr("Learn MIDI Trigger…"));
        if (pc != NULL && pc->hasFunctionTrigger(fid))
            aClearMidi = menu.addAction(tr("Clear MIDI Trigger (%1)")
                                            .arg(pc->functionTriggerLabel(fid)));
        menu.addSeparator();
    }

    QAction *aScene  = menu.addAction(tr("New Scene"));
    QAction *aChaser = menu.addAction(tr("New Chaser"));
    QAction *aColl   = menu.addAction(tr("New Collection"));
    QAction *aEFX    = menu.addAction(tr("New EFX"));
    QAction *aMatrix = menu.addAction(tr("New RGB Matrix"));
    QAction *aShow   = menu.addAction(tr("New Show"));
    menu.addSeparator();
    QAction *aFolder = menu.addAction(tr("New Folder"));

    menu.addSeparator();
    // Order top-level type categories by role (Show · Chaser · Collection · RGB
    // Matrix · EFX · Scene) vs. classic A–Z. Persisted.
    QAction *aTypeOrder = menu.addAction(tr("Group types by role"));
    aTypeOrder->setCheckable(true);
    aTypeOrder->setChecked(m_funcTree->typeOrderByRole());

    QAction *chosen = menu.exec(m_funcTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;
    if (chosen == aTypeOrder)
    {
        const bool en = aTypeOrder->isChecked();
        m_funcTree->setTypeOrderByRole(en);
        QSettings().setValue(
            QStringLiteral("programming/functree/typeOrderByRole"), en);
        return;
    }
    if (chosen == aDuplicate)
    {
        duplicateFunctions(targetIds);
        return;
    }
    if (chosen == aDelete)
    {
        deleteFunctions(targetIds);
        return;
    }
    if (chosen == aFolder)
    {
        m_funcTree->addFolder();
        return;
    }
    if (chosen == aLearnMidi)
    {
        ProgrammerController *pc = m_doc->programmer();
        if (pc != NULL && !targetIds.isEmpty())
        {
            const quint32 fid = targetIds.first();
            pc->learnFunctionTrigger(fid);
            Function *tf = m_doc->function(fid);
            QMessageBox::information(this, tr("Learn MIDI Trigger"),
                tr("Press a button or pad on your MIDI controller now to bind it "
                   "to \"%1\".\n\nThe control will toggle the function and its LED "
                   "will follow the run state.")
                    .arg(tf != NULL ? tf->name() : tr("this function")));
        }
        return;
    }
    if (chosen == aClearMidi)
    {
        if (ProgrammerController *pc = m_doc->programmer())
            if (!targetIds.isEmpty())
                pc->clearFunctionTrigger(targetIds.first());
        return;
    }

    Function *f = NULL;
    QString base;
    if (chosen == aScene)       { f = new Scene(m_doc);      base = tr("New Scene"); }
    else if (chosen == aChaser) { f = new Chaser(m_doc);     base = tr("New Chaser"); }
    else if (chosen == aColl)   { f = new Collection(m_doc); base = tr("New Collection"); }
    else if (chosen == aEFX)    { f = new EFX(m_doc);        base = tr("New EFX"); }
    else if (chosen == aMatrix) { f = new RGBMatrix(m_doc);  base = tr("New RGB Matrix"); }
    else if (chosen == aShow)   { f = new Show(m_doc);       base = tr("New Show"); }
    if (f == NULL)
        return;

    const QString folder = clickedFolder;
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

QList<quint32> ProgrammingManager::funcTreeTargetIds(QTreeWidgetItem *clicked) const
{
    QList<quint32> ids;

    // If the clicked row is part of a multi-selection, act on the whole
    // selection; otherwise act on the clicked row alone. Folder rows carry no
    // function id and are skipped.
    const QList<QTreeWidgetItem*> selected = m_funcTree->selectedItems();
    const bool clickedIsSelected = clicked != NULL && selected.contains(clicked);

    const QList<QTreeWidgetItem*> &rows =
        clickedIsSelected ? selected
                          : (clicked != NULL ? QList<QTreeWidgetItem*>{clicked}
                                             : QList<QTreeWidgetItem*>{});
    for (QTreeWidgetItem *it : rows)
    {
        const quint32 fid = m_funcTree->itemFunctionId(it);
        if (fid != Function::invalidId() && !ids.contains(fid))
            ids.append(fid);
    }
    return ids;
}

void ProgrammingManager::duplicateFunctions(const QList<quint32> &fids)
{
    Function *last = NULL;
    for (quint32 fid : fids)
    {
        Function *src = m_doc->function(fid);
        if (src == NULL)
            continue;

        Function *copy = src->createCopy(m_doc);
        if (copy == NULL)
            continue;

        // Smart-increment the source name ("Verse 1" -> "Verse 2",
        // "01-01.02-Reville" -> "01-01.03-Reville"), same as the Function
        // Manager, rather than a generic "(Copy)" suffix.
        copy->setName(m_doc->nextDuplicateName(src));
        last = copy;
    }

    if (last == NULL)
        return;

    // The tree rebuilt itself as each copy was added; re-fetch the item.
    QTreeWidgetItem *it = m_funcTree->functionItem(last);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
}

void ProgrammingManager::addFunctionsToCollection(quint32 containerId,
                                                  const QList<quint32> &fids)
{
    Collection *col = qobject_cast<Collection*>(m_doc->function(containerId));
    if (col == NULL)
        return;

    int added = 0;
    int skipped = 0;
    for (quint32 fid : fids)
    {
        // Skip the collection itself, anything already in it, and anything that
        // would form a cycle (a function that already contains this collection).
        Function *f = m_doc->function(fid);
        if (f == NULL || fid == containerId || col->functions().contains(fid)
                || f->contains(containerId))
        {
            skipped++;
            continue;
        }
        if (col->addFunction(fid))
            added++;
        else
            skipped++;
    }

    if (added > 0)
    {
        m_doc->setModified();

        // Rebuild the nav tree's nested view if this collection's members are on
        // screen, and refresh the canvas editor if this collection is open.
        if (m_memberContainer == containerId)
            syncMemberNodes(containerId);
        if (m_canvasFunction == containerId && m_funcEditor != NULL)
            loadFunctionEditor(col);   // cheapest correct refresh of the member list
    }

    if (skipped > 0 && added == 0)
        QMessageBox::information(this, tr("Nothing added"),
            tr("Those functions are already in the collection (or would create a loop)."));
}

void ProgrammingManager::deleteFunctions(const QList<quint32> &fids)
{
    if (fids.isEmpty())
        return;

    QStringList names;
    int inContainers = 0;   // how many targets are referenced by a chaser/collection
    for (quint32 fid : fids)
    {
        if (Function *f = m_doc->function(fid))
        {
            names.append(f->name());
            if (functionUsageCount(fid) > 0)
                inContainers++;
        }
    }
    if (names.isEmpty())
        return;

    QString msg = fids.size() > 1
        ? tr("Delete %1 functions?\n\n%2").arg(names.size()).arg(names.join(", "))
        : tr("Delete function \"%1\"?").arg(names.first());
    // These may be shown as members of an open collection/chaser; deleting is
    // GLOBAL, not just an unlink, so call that out.
    if (inContainers > 0)
        msg += tr("\n\nThis also removes %1 from every chaser and collection that uses %2.")
                   .arg(inContainers > 1 || fids.size() > 1 ? tr("them") : tr("it"))
                   .arg(inContainers > 1 || fids.size() > 1 ? tr("them") : tr("it"));

    if (QMessageBox::question(this, tr("Delete Functions"), msg,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    // Delete by id, never by tree item: the first deletion emits functionRemoved,
    // which rebuilds the func tree (and tears down the canvas if it held one of
    // these) — so any QTreeWidgetItem* captured before the loop would dangle.
    for (quint32 fid : fids)
        m_doc->deleteFunction(fid);
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
        { QT_TR_NOOP("New Strobe"),     QLCPalette::Strobe },
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

    // Palettes the menu acts on: the whole selection if the clicked row is part
    // of it, else just the clicked row.
    const QList<quint32> targetPalettes =
        paletteTreeTargetIds(m_paletteTree->itemAt(pos));

    QAction *aMove = NULL;
    QAction *aDuplicate = NULL;
    QAction *aBundle = NULL;
    const quint32 pid =
        m_paletteTree->itemPaletteId(m_paletteTree->itemAt(pos));
    if (pid != QLCPalette::invalidId())
    {
        aDuplicate = menu.addAction(tr("Duplicate"));
        aMove = menu.addAction(tr("Move to folder…"));
        menu.addSeparator();
        aBundle = menu.addAction(targetPalettes.size() > 1
                                     ? tr("Save %1 palettes as Bundle…").arg(targetPalettes.size())
                                     : tr("Save as Bundle…"));
    }

    QAction *aDelete = NULL;
    if (targetPalettes.isEmpty() == false)
    {
        menu.addSeparator();
        aDelete = menu.addAction(targetPalettes.size() > 1
                                     ? tr("Delete %1 palettes").arg(targetPalettes.size())
                                     : tr("Delete"));
    }

    QAction *chosen = menu.exec(m_paletteTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (chosen == aBundle)
    {
        saveBundleFromPalettes(targetPalettes);
        return;
    }

    if (chosen == aDelete)
    {
        // Count how many scenes reference these palettes so the operator knows
        // the looks that will lose them (deletePalette detaches them cleanly).
        QSet<quint32> usingScenes;
        foreach (Function *f, m_doc->functions())
        {
            Scene *s = qobject_cast<Scene*>(f);
            if (s == NULL)
                continue;
            foreach (quint32 pid2, targetPalettes)
                if (s->palettes().contains(pid2))
                    usingScenes.insert(s->id());
        }
        QString msg = targetPalettes.size() > 1
            ? tr("Delete these %1 palettes?").arg(targetPalettes.size())
            : tr("Delete this palette?");
        if (usingScenes.isEmpty() == false)
            msg += QString("\n\n") + tr("%n scene(s) use them; the look will be "
                       "removed from those scenes.", "", usingScenes.count());
        if (QMessageBox::question(this, tr("Delete palette"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        // Collect ids first — the first delete rebuilds the tree.
        foreach (quint32 delId, targetPalettes)
            m_doc->deletePalette(delId);
        m_paletteTree->updateTree();
        return;
    }

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

QTreeWidgetItem *ProgrammingManager::addMemberLeaf(QTreeWidgetItem *parentNode, quint32 mid,
                                                   int orderIdx, int width, bool ordered,
                                                   QSet<quint32> &visited, int depth)
{
    Function *mf = m_doc->function(mid);
    if (mf == NULL)
        return NULL;
    // Prefix ordered (chaser step / timeline placement) members with a
    // zero-padded index so the alphabetical tree keeps their order — BUT only
    // when the member's own name doesn't already start with a number (users who
    // encode order in the name, e.g. "00-01.02-…", shouldn't get a redundant
    // prefix). The prefix is display-only; it is never written back to the name.
    const QString mname = mf->name();
    const bool nameIsNumbered = mname.isEmpty() == false && mname.at(0).isDigit();
    QTreeWidgetItem *ci = new QTreeWidgetItem(parentNode);
    ci->setText(0, (ordered && !nameIsNumbered)
                ? QString("%1. %2").arg(orderIdx, width, 10, QChar('0')).arg(mname)
                : mname);
    ci->setIcon(0, mf->getIcon());
    ci->setData(0, Qt::UserRole, mid); // so itemFunctionId() resolves it
    // Read-only nav: selectable + double-click to edit, not draggable.
    ci->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // Recurse into sub-containers (collection inside a chaser, a chaser on a
    // show track, …), guarding against cycles. Leave collapsed — expand to drill.
    if (visited.contains(mid) == false)
    {
        visited.insert(mid);
        addMemberChildren(ci, mid, visited, depth + 1);
    }
    return ci;
}

void ProgrammingManager::addMemberChildren(QTreeWidgetItem *treeNode,
                                           quint32 containerId,
                                           QSet<quint32> &visited, int depth)
{
    if (depth > 8)
        return;
    Function *c = m_doc->function(containerId);

    // Show → tracks → functions: mirror the Show Manager timeline hierarchy —
    // one folder per track, the placed functions nested underneath in timeline
    // (start-time) order. A track folder is an inert nav label (no function id).
    if (Show *sh = qobject_cast<Show*>(c))
    {
        foreach (Track *trk, sh->tracks())
        {
            if (trk == NULL)
                continue;
            QTreeWidgetItem *tn = new QTreeWidgetItem(treeNode);
            tn->setText(0, trk->name().isEmpty() ? tr("Track") : trk->name());
            tn->setData(0, Qt::UserRole, Function::invalidId()); // not a function
            tn->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            QList<ShowFunction*> sfs = trk->showFunctions();
            std::sort(sfs.begin(), sfs.end(), [](ShowFunction *a, ShowFunction *b) {
                return a->startTime() < b->startTime();
            });
            const int width = qMax(2, QString::number(sfs.count()).length());
            int idx = 0;
            foreach (ShowFunction *sf, sfs)
            {
                if (sf == NULL)
                    continue;
                idx++;
                addMemberLeaf(tn, sf->functionID(), idx, width, true, visited, depth);
            }
            tn->setExpanded(true);
        }
        return;
    }

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

    const int width = qMax(2, QString::number(members.count()).length());
    int idx = 0;
    foreach (quint32 mid, members)
    {
        idx++;
        addMemberLeaf(treeNode, mid, idx, width, ordered, visited, depth);
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

void ProgrammingManager::slotFlashSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL)
        return;
    for (quint32 fid : m_selectedFixtures)
        pc->flashFixture(fid);
}

/*****************************************************************************
 * Blind (mute physical output, keep 2D preview)
 *****************************************************************************/

void ProgrammingManager::slotBlindStateChanged(bool on)
{
    // Reflect the engine's Blind state in the in-context canvas banner.
    if (m_blindBanner != nullptr)
        m_blindBanner->setVisible(on);
}

/*****************************************************************************
 * Park (hold fixtures out of cue output)
 *****************************************************************************/

void ProgrammingManager::slotParkSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL || m_selectedFixtures.isEmpty())
        return;

    // Toggle: if the whole selection is already parked, unpark it; else park.
    bool allParked = true;
    for (quint32 fid : m_selectedFixtures)
        if (!pc->isFixtureParked(fid))
            allParked = false;

    if (allParked)
        pc->unparkFixtures(m_selectedFixtures);
    else
        pc->parkFixtures(m_selectedFixtures);
}

void ProgrammingManager::slotUnparkAll()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc)
        pc->unparkAllFixtures();
}

void ProgrammingManager::slotMarkSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL || m_selectedFixtures.isEmpty())
        return;

    // Toggle: if the whole selection is already marked, unmark it; else mark.
    bool allMarked = true;
    for (quint32 fid : m_selectedFixtures)
        if (!pc->isFixtureMarked(fid))
            allMarked = false;

    if (allMarked)
        pc->unmarkFixtures(m_selectedFixtures);
    else
        pc->markFixtures(m_selectedFixtures);
}

void ProgrammingManager::slotUnmarkAll()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc)
        pc->unmarkAllFixtures();
}

void ProgrammingManager::slotMarkChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL)
        return;
    if (m_unmarkBtn)
        m_unmarkBtn->setEnabled(pc->hasMarkedFixtures());
    updateMarkButtonLabel();
    // The 2D monitor refreshes its ghost outlines via its own markChanged
    // connection (see Monitor).
}

void ProgrammingManager::updateMarkButtonLabel()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL || m_markBtn == NULL)
        return;

    bool anySel = !m_selectedFixtures.isEmpty();
    bool allMarked = anySel;
    for (quint32 fid : m_selectedFixtures)
        if (!pc->isFixtureMarked(fid))
            allMarked = false;

    m_markBtn->setEnabled(anySel);
    m_markBtn->setText(anySel && allMarked ? tr("Unmark") : tr("Mark"));
}

void ProgrammingManager::slotSnapshotLive()
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene == NULL)
    {
        QMessageBox::information(this, tr("Snapshot"),
            tr("Open a scene in the canvas first — the snapshot bakes into it."));
        return;
    }

    // Read the current pre-Grand-Master output for every patched fixture and
    // collect the values; apply AFTER releasing the universe lock (never run a
    // dialog while holding it). A fixture with all-zero output is skipped.
    QList<SceneValue> captured;
    int fixCount = 0;
    {
        QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
        foreach (Fixture *fxi, m_doc->fixtures())
        {
            if (fxi == NULL)
                continue;
            const quint32 uni = fxi->universe();
            if (uni >= quint32(universes.size()))
                continue;
            Universe *u = universes[uni];
            const quint32 base = fxi->address();
            const quint32 chans = fxi->channels();

            bool hasOutput = false;
            for (quint32 c = 0; c < chans; c++)
                if (u->preGMValue(int(base + c)) != 0) { hasOutput = true; break; }
            if (hasOutput == false)
                continue;

            fixCount++;
            for (quint32 c = 0; c < chans; c++)
                captured.append(SceneValue(fxi->id(), c, u->preGMValue(int(base + c))));
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
    }

    if (captured.isEmpty())
    {
        QMessageBox::information(this, tr("Snapshot"),
            tr("Nothing to capture — no fixture is currently outputting."));
        return;
    }

    if (QMessageBox::question(this, tr("Snapshot into \"%1\"").arg(scene->name()),
            tr("Bake the current live output of %1 fixture(s) (%2 channel values) "
               "into this scene as static values? Existing values on those "
               "channels are overwritten.").arg(fixCount).arg(captured.count()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    foreach (const SceneValue &scv, captured)
        scene->setValue(scv, /*blind*/ false, /*checkHTP*/ false);

    m_doc->setModified();
    if (m_canvas != NULL)
        m_canvas->reload();   // show the newly baked values (scene already open)
    refreshPreview();
}

void ProgrammingManager::slotParkChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL)
        return;
    if (m_unparkBtn)
        m_unparkBtn->setEnabled(pc->hasParkedFixtures());
    updateParkButtonLabel();
}

void ProgrammingManager::updateParkButtonLabel()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL || m_parkBtn == NULL)
        return;

    // If every selected fixture is already parked, offer Unpark instead of Park.
    bool anySel = !m_selectedFixtures.isEmpty();
    bool allParked = anySel;
    for (quint32 fid : m_selectedFixtures)
        if (!pc->isFixtureParked(fid))
            allParked = false;

    m_parkBtn->setEnabled(anySel);
    if (anySel && allParked)
    {
        m_parkBtn->setText(tr("Unpark"));
        m_parkBtn->setToolTip(tr("Release the park on the selected fixtures"));
    }
    else
    {
        m_parkBtn->setText(tr("Park"));
        m_parkBtn->setToolTip(tr("Hold the selected fixtures at their current values, out of cue "
                                 "output, until unparked (saved with the workspace)"));
    }
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

// ─── Bundle stamp & save ──────────────────────────────────────────────────────

void ProgrammingManager::slotStampBundle(const QString &bundleName)
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (!scene)
    {
        QMessageBox::information(this, tr("No look selected"),
            tr("Select a look in the canvas before stamping a Bundle."));
        return;
    }

    const QLCBundle bundle = m_bundleCache->bundle(bundleName);
    if (!bundle.isValid()) return;

    // Save undo snapshot before replacing (palette list + per-look fades)
    m_stampUndoSceneId   = scene->id();
    m_stampUndoPalettes  = scene->palettes();
    m_stampUndoFades.clear();
    for (quint32 pid : m_stampUndoPalettes)
    {
        const int in = scene->paletteFadeIn(pid), out = scene->paletteFadeOut(pid);
        if (in >= 0 || out >= 0)
            m_stampUndoFades[pid] = qMakePair(in, out);
    }

    // Replace: remove all existing palettes from this scene before stamping
    for (quint32 pid : m_stampUndoPalettes)
        scene->removePalette(pid);

    for (const BundleEntry &entry : bundle.palettes)
    {
        quint32 paletteId = QLCPalette::invalidId();

        // De-dup: look for an existing palette that matches this entry.
        if (entry.type == "Color")
        {
            for (QLCPalette *p : m_doc->palettes())
            {
                if (p->type() != QLCPalette::Color) continue;
                if (p->rgbValue().name() == entry.color)
                { paletteId = p->id(); break; }
            }
        }
        else if (entry.type == "Dimmer")
        {
            for (QLCPalette *p : m_doc->palettes())
            {
                if (p->type() != QLCPalette::Dimmer) continue;
                if (p->value().toInt() == entry.dimmerValue)
                { paletteId = p->id(); break; }
            }
        }
        // Effects are never de-duped — params may differ per look.

        if (paletteId == QLCPalette::invalidId())
        {
            // Create a new palette from the entry.
            QLCPalette *np = nullptr;
            if (entry.type == "Color")
            {
                np = new QLCPalette(QLCPalette::Color);
                np->setValue(entry.color);
                np->setPath("Palettes/Color/");
            }
            else if (entry.type == "Dimmer")
            {
                np = new QLCPalette(QLCPalette::Dimmer);
                np->setValue(entry.dimmerValue);
                np->setPath("Palettes/Dimmer/");
            }
            else if (entry.type == "Effect")
            {
                np = new QLCPalette(QLCPalette::Effect);
                // Resolve base name → full path via the script cache
                EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
                QString fullPath = scache->scriptPath(entry.script);
                if (fullPath.isEmpty())
                    fullPath = entry.script;  // fallback: store as-is
                np->setScriptPath(fullPath);
                np->setEffectPreset(entry.effectPreset);
                np->setEffectParamValues(entry.params);
                np->setPath(QString("Palettes/Effect/"));
            }
            else if (entry.type == "PanTilt")
            {
                np = new QLCPalette(QLCPalette::PanTilt);
                np->setValue(entry.pan, entry.tilt);
                np->setPath("Palettes/PanTilt/");
            }
            else if (entry.type == "Beam")
            {
                np = new QLCPalette(QLCPalette::Beam);
                // setValues(), not three setValue() calls: setValue() CLEARS the
                // list before appending, so the first two would be discarded and
                // the palette would end up as [iris] — which valuesFromFixtures()
                // reads as focus, driving the wrong channel.
                np->setValues(QVariantList() << entry.focus << entry.frost << entry.iris);
                np->setPath("Palettes/Beam/");
            }

            if (!np) continue;

            np->setName(entry.name.isEmpty()
                        ? tr("New %1").arg(entry.type) : entry.name);
            if (!m_doc->addPalette(np)) { delete np; continue; }
            paletteId = np->id();
        }

        scene->addPalette(paletteId);
        // Restore the look's fade override carried by the bundle (if any).
        if (entry.fadeIn >= 0 || entry.fadeOut >= 0)
            scene->setPaletteFade(paletteId, entry.fadeIn, entry.fadeOut);
    }

    m_doc->setModified();
    // Sync effect instances immediately — destroys old ones, creates new ones
    // based on the current palette list. Done BEFORE reload/slotCanvasModified
    // so the change takes effect regardless of whether this tab is visible
    // (refreshPreview() is gated on isVisible() and would miss hidden-tab stamps).
    if (m_doc->effectScriptRunner())
    {
        m_doc->effectScriptRunner()->syncScene(m_currentScene);
        // Update cache so the next refreshPreview() call doesn't redundantly
        // re-sync when the scene is already running with these palettes.
        Scene *stampScene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
        m_lastSyncedEffectPalettes = effectPaletteSignature(stampScene);
    }
    m_paletteTree->updateTree();
    if (m_canvas) m_canvas->reload();  // rebuild look list to show new entries
    slotCanvasModified();              // restart preview so DMX updates
}

void ProgrammingManager::slotSaveAsBundle()
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (!scene)
    {
        QMessageBox::information(this, tr("No look selected"),
            tr("Select a look in the canvas to save as a Bundle."));
        return;
    }

    const QList<quint32> paletteIds = scene->palettes();
    if (paletteIds.isEmpty())
    {
        QMessageBox::information(this, tr("Look is empty"),
            tr("Add at least one palette to the look before saving it as a Bundle."));
        return;
    }
    saveBundleFromPalettes(paletteIds, scene);
}

QList<quint32> ProgrammingManager::paletteTreeTargetIds(QTreeWidgetItem *clicked) const
{
    QList<quint32> ids;
    const QList<QTreeWidgetItem*> selected = m_paletteTree->selectedItems();
    const bool clickedIsSelected = clicked != NULL && selected.contains(clicked);

    const QList<QTreeWidgetItem*> &rows =
        clickedIsSelected ? selected
                          : (clicked != NULL ? QList<QTreeWidgetItem*>{clicked}
                                             : QList<QTreeWidgetItem*>{});
    for (QTreeWidgetItem *it : rows)
    {
        const quint32 id = m_paletteTree->itemPaletteId(it);
        if (id != QLCPalette::invalidId() && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

void ProgrammingManager::saveBundleFromPalettes(const QList<quint32> &paletteIds, Scene *sourceScene)
{
    if (paletteIds.isEmpty())
    {
        QMessageBox::information(this, tr("No palettes"),
            tr("Select at least one palette to save as a Bundle."));
        return;
    }

    BundleEditor dlg(m_doc, m_bundleCache, paletteIds, sourceScene, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (!m_bundleCache->saveBundle(dlg.bundle()))
    {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not write the Bundle file."));
        return;
    }
    m_bundleBrowser->refresh();
}

Scene *ProgrammingManager::snapshotScene(Scene *scene)
{
    if (scene == NULL)
        return NULL;
    Scene *snap = new Scene(m_doc); // detached (not addFunction'd); we own it
    snap->copyFrom(scene);          // full state: values/fixtures/groups/palettes/fades
    return snap;
}

void ProgrammingManager::resetUndoFor(quint32 sceneId)
{
    qDeleteAll(m_undoStack);
    m_undoStack.clear();
    delete m_undoBaseline;
    m_undoBaseline = NULL;
    m_undoSceneId = sceneId;
    if (sceneId != Function::invalidId())
    {
        Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
        m_undoBaseline = snapshotScene(scene);
    }
}

void ProgrammingManager::pushUndoSnapshot()
{
    // Only track scene-canvas edits, and only for the scene the baseline is for.
    if (m_currentScene == Function::invalidId() || m_currentScene != m_undoSceneId
            || m_undoBaseline == NULL)
        return;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene == NULL)
        return;

    // The baseline is the pre-edit state — push it; the edit is now current, so
    // re-capture the baseline as the current state for the next edit.
    m_undoStack.append(m_undoBaseline);
    const int kMaxUndo = 25;
    while (m_undoStack.size() > kMaxUndo)
        delete m_undoStack.takeFirst();
    m_undoBaseline = snapshotScene(scene);
}

void ProgrammingManager::slotUndo()
{
    // General scene undo first; fall back to the bundle-stamp undo when there's
    // no scene-edit history to unwind.
    if (m_undoStack.isEmpty() || m_currentScene != m_undoSceneId)
    {
        slotUndoStamp();
        return;
    }
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene == NULL)
    {
        slotUndoStamp();
        return;
    }

    Scene *snap = m_undoStack.takeLast();
    const QString keepName = scene->name(); // undo canvas edits, not a tree rename
    scene->copyFrom(snap);   // restore the pre-edit state
    scene->setName(keepName);
    delete snap;
    // The restored state is the new baseline (don't let the reload re-push it).
    delete m_undoBaseline;
    m_undoBaseline = snapshotScene(scene);

    m_doc->setModified();
    if (m_canvas != NULL)
        m_canvas->reload();
    refreshPreview();
}

void ProgrammingManager::slotUndoStamp()
{
    if (m_stampUndoSceneId == quint32(-1)) return;

    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_stampUndoSceneId));
    if (!scene) return;

    // Remove the currently stamped palettes
    for (quint32 pid : scene->palettes())
        scene->removePalette(pid);

    // Restore the saved palette list + their per-look fade overrides
    for (quint32 pid : m_stampUndoPalettes)
        scene->addPalette(pid);
    for (auto it = m_stampUndoFades.constBegin(); it != m_stampUndoFades.constEnd(); ++it)
        scene->setPaletteFade(it.key(), it.value().first, it.value().second);

    // Consume the undo entry so a second Ctrl-Z won't repeat
    m_stampUndoSceneId  = quint32(-1);
    m_stampUndoPalettes.clear();
    m_stampUndoFades.clear();

    m_doc->setModified();
    m_paletteTree->updateTree();
    if (m_canvas) m_canvas->reload();
    slotCanvasModified();
}
