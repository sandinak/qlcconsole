/*
  Q Light Controller Plus
  programmingmanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QGuiApplication>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
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
#include <QToolButton>
#include <QIcon>
#include <algorithm>
#include <functional>

#include <QDebug>
#include "mainbar.h"
#include "speedmasterspanel.h"
#include "programmingmanager.h"
#include "paletteops.h"
#include "usedby.h"
#include "helpbrowser.h"
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
#include "showtreecontroller.h"
#include "showtreeview.h"
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
#include <QStackedWidget>
#include "functionmanager.h"
#include "programmingoutline.h"
#include "lookstackcheck.h"
#include "speedmasters.h"


ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_funcEditor(nullptr)
    , m_currentScene(Function::invalidId())
    , m_canvasFunction(Function::invalidId())
    , m_previewFunction(Function::invalidId())
    , m_trialScene(Function::invalidId())
    , m_operateFunction(Function::invalidId())
    , m_clipboardFunction(Function::invalidId())
    , m_clipboardPalette(QLCPalette::invalidId())
{
    QHBoxLayout *root = new QHBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("programmingSplitter"));
    /* A handle moves the edge and stops at a panel's minimum: never snaps a
       panel shut ("it jumps and ... only shows that"), which a collapsible
       splitter does the moment a drag passes a minimum. */
    splitter->setChildrenCollapsible(false);
    m_mainSplit = splitter;
    root->addWidget(splitter);

    // --- Left: the outline -- every scene folder, scene, target and look ---
    QWidget *navPanel = new QWidget(this);
    QVBoxLayout *navCol = new QVBoxLayout(navPanel);
    navCol->setContentsMargins(0, 0, 0, 0);
    navCol->setSpacing(3);

    m_outline = new ProgrammingOutline(m_doc, navPanel);

    QHBoxLayout *navToolbar = new QHBoxLayout;
    navToolbar->setContentsMargins(0, 0, 0, 0);
    QLineEdit *funcFilter = new QLineEdit(this);
    funcFilter->setObjectName(QStringLiteral("outlineFilter"));
    funcFilter->setPlaceholderText(tr("Filter…"));
    funcFilter->setClearButtonEnabled(true);
    navToolbar->addWidget(funcFilter, 1);

    // Expand all / Collapse all (also ⌘→ / ⌘←, and ⌥-click a triangle for
    // everything under it).
    QToolButton *expandBtn = new QToolButton(this);
    expandBtn->setObjectName(QStringLiteral("outlineExpandAll"));
    expandBtn->setText(QStringLiteral("⊞"));
    expandBtn->setToolTip(tr("Expand all (⌘→). ⌥-click a triangle to open everything under it."));
    connect(expandBtn, &QToolButton::clicked, m_outline, &ProgrammingOutline::expandEverything);
    navToolbar->addWidget(expandBtn);
    QToolButton *collapseBtn = new QToolButton(this);
    collapseBtn->setObjectName(QStringLiteral("outlineCollapseAll"));
    collapseBtn->setText(QStringLiteral("⊟"));
    collapseBtn->setToolTip(tr("Collapse all (⌘←)"));
    connect(collapseBtn, &QToolButton::clicked, m_outline, &ProgrammingOutline::collapseEverything);
    navToolbar->addWidget(collapseBtn);

    // "+ Scene": a scene in the selected row's folder, or in a new folder.
    // Embedded here (not on the app's menu bar) so it travels along when this
    // tab is detached into its own window (App::slotDetachContext).
    m_addFuncBtn = new QToolButton(this);
    m_addFuncBtn->setObjectName("programmingAddFunction");
    m_addFuncBtn->setText(tr("Scene"));
    m_addFuncBtn->setIcon(QIcon(":/edit_add.png"));
    m_addFuncBtn->setToolTip(tr("A new scene, in the folder of the row selected"));
    m_addFuncBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *addFuncMenu = new QMenu(m_addFuncBtn);
    QAction *addSceneAction  = addFuncMenu->addAction(QIcon(":/scene.png"), tr("New scene"));
    QAction *addFolderAction = addFuncMenu->addAction(QIcon(":/folder.png"), tr("New scene in a new folder…"));
    connect(m_addFuncBtn, &QToolButton::clicked, this, [this]() {
        addNewFunction(Function::SceneType, m_outline->folderFor(m_outline->currentItem()));
    });
    connect(addSceneAction, &QAction::triggered, this, [this]() {
        addNewFunction(Function::SceneType, m_outline->folderFor(m_outline->currentItem()));
    });
    connect(addFolderAction, &QAction::triggered, this, [this]() {
        const QString in = m_outline->folderFor(m_outline->currentItem());
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New folder"),
            in.isEmpty() ? tr("Folder name:") : tr("Folder name (in %1):").arg(in),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        addNewFunction(Function::SceneType, in.isEmpty() ? name : in + QLatin1Char('/') + name);
    });
    m_addFuncBtn->setMenu(addFuncMenu);
    navToolbar->addWidget(m_addFuncBtn);
    // Match the main window's icon/text display preference.
    applyToolbarLabelMode();

    /* What the outline is, in one line, always there: it is new, and a tree
       of everything doesn't say on its own that you edit by clicking and
       move by dragging. During a drag the same line says what the drop will
       do, before you let go. */
    const QString outlineWhat = tr("Every scene and what's in it. Click a row to edit it; "
                                   "drag rows between scenes.");
    m_outlineHint = new QLabel(outlineWhat, navPanel);
    m_outlineHint->setObjectName(QStringLiteral("outlineHint"));
    m_outlineHint->setWordWrap(true);
    m_outlineHint->setStyleSheet(QStringLiteral("QLabel { font-style: italic; }"));
    m_outlineHint->setToolTip(tr("Folders hold scenes; a scene holds its targets (groups ◆, fixtures) "
                                 "and its looks (palettes; an effect's own colour and dimmer sit under it). "
                                 "Drag a row to another scene to move it, hold ⌥ to copy; drag within a "
                                 "scene to reorder its looks (lower rows win). Drag palettes, groups and "
                                 "fixtures in from the right. F2 renames, ⌫ removes, ⌘Z undoes."));

    navCol->addLayout(navToolbar);
    navCol->addWidget(m_outlineHint);
    navCol->addWidget(m_outline, 1);
    connect(funcFilter, &QLineEdit::textChanged, m_outline, &ProgrammingOutline::setFilterText);
    connect(m_outline, &ProgrammingOutline::hintChanged, this, [this, outlineWhat](const QString &text) {
        m_outlineHint->setText(text.isEmpty() ? outlineWhat : text);
        m_outlineHint->setStyleSheet(text.isEmpty() ? QStringLiteral("QLabel { font-style: italic; }")
                                                    : QStringLiteral("QLabel { font-weight: bold; }"));
    });
    splitter->addWidget(navPanel);

    // --- Center: the selected scene's canvas ---
    QWidget *canvasPanel = new QWidget(this);
    m_canvasLayout = new QVBoxLayout(canvasPanel);

    // Toolbar: Highlight + Followspot toggles
    {
        QHBoxLayout *toolbar = new QHBoxLayout;
        toolbar->setContentsMargins(0, 0, 0, 0);

        m_highlightBtn = new QPushButton(tr("Highlight"), this);
        m_highlightBtn->setObjectName("programmingHighlight");
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

        m_unparkBtn = new QPushButton(tr("Unpark all"), this);
        m_unparkBtn->setToolTip(tr("Release every parked fixture"));
        m_unparkBtn->setEnabled(false);

        m_markBtn = new QPushButton(tr("Mark"), this);
        m_markBtn->setToolTip(tr("Move-in-black: pre-position the selected DARK fixtures "
                                 "(pan/tilt, colour, gobo, beam) to where they'll be revealed, "
                                 "held out of cue output. Auto-releases when a cue lights them; "
                                 "saved with the workspace."));
        m_markBtn->setEnabled(false);

        m_unmarkBtn = new QPushButton(tr("Unmark all"), this);
        m_unmarkBtn->setToolTip(tr("Release every marked fixture"));
        m_unmarkBtn->setEnabled(false);

        m_autoMibBtn = new QPushButton(tr("Auto MIB"), this);
        m_autoMibBtn->setCheckable(true);
        m_autoMibBtn->setToolTip(tr("Automatic move-in-black: while a chaser or the show "
                                    "timeline runs, look ahead to the next cue and pre-position "
                                    "any dark mover it will reveal — as long as there's enough "
                                    "lead time. Releases each fixture as its cue lights it."));

        m_mibGapSpin = new QDoubleSpinBox(this);
        m_mibGapSpin->setRange(0.1, 10.0);
        m_mibGapSpin->setSingleStep(0.1);
        m_mibGapSpin->setDecimals(1);
        m_mibGapSpin->setSuffix(tr(" s lead"));
        m_mibGapSpin->setToolTip(tr("Move-in-black lead time: the next cue must be at least this "
                                    "far away for a dark mover to be pre-set. Below it there's no "
                                    "time to move in black, so it's skipped."));

        m_snapshotBtn = new QPushButton(tr("Snapshot"), this);
        m_snapshotBtn->setToolTip(tr("Bake the current live DMX output into the open "
            "scene as static values — capture a look built on an external console."));
        connect(m_snapshotBtn, SIGNAL(clicked()), this, SLOT(slotSnapshotLive()));

        // BPM / internal beat generator

        m_bpmSpin = new QSpinBox(this);
        m_bpmSpin->setRange(20, 400);
        m_bpmSpin->setValue(120);
        m_bpmSpin->setToolTip(tr("Beats per minute for internal beat generator"));
        m_bpmSpin->setEnabled(false);

        m_tapBtn = new QPushButton(tr("Tap"), this);
        m_tapBtn->setToolTip(tr("Tap to set BPM from live tempo"));
        m_tapBtn->setEnabled(false);
        toolbar->addWidget(m_tapBtn);

        m_bpmBtn = new QPushButton(tr("Beat On"), this);
        m_bpmBtn->setCheckable(true);
        m_bpmBtn->setToolTip(tr("Enable QLC+ internal beat generator (drives _beat/_bpm in generators)"));

        QPushButton *mastersBtn = new QPushButton(tr("Masters"), this);
        mastersBtn->setObjectName(QStringLiteral("speedMastersButton"));
        mastersBtn->setToolTip(tr("The four speed masters: a tempo, tap, rate and pause each, "
                                  "for effects, looks and chases to run on"));
        connect(mastersBtn, &QPushButton::clicked, this, [this]() {
            SpeedMastersPanel::open(m_doc, this);
        });


        m_speedSpin = new QSpinBox(this);
        m_speedSpin->setRange(10, 500);
        m_speedSpin->setSingleStep(10);
        m_speedSpin->setValue(100);
        m_speedSpin->setSuffix(tr("%"));
        m_speedSpin->setToolTip(tr("Joystick speed for position editing (100% = 2 m/s at full deflection)"));

        m_saveBtn = new QPushButton(tr("Save Positions"), this);
        m_saveBtn->setEnabled(false);
        m_saveBtn->setToolTip(tr(
            "Finalize joystick-driven pan/tilt/aim edits and save the workspace.\n"
            "Only these joystick edits need this button — every other look edit "
            "here (color, dimmer, gradient, …) is already live; the footer's "
            "● Unsaved changes indicator tracks those."));
        toolbar->addWidget(m_saveBtn);

        /* One row, no scrolling ("can we make this row more useful using a
           dropdown paradigm like we did for the studio options? right now
           scrolling is a bit obnoxious"): what is pressed all the time stays a
           button -- Highlight, Flash, Tap, Save Positions -- and the rest sits
           in drop-downs, as the Lighting Studio's footer does. The buttons
           behind the menu items are kept (hidden): their state, slots and
           enabling carry on exactly as before, and each item mirrors its
           button when the menu opens. */
        auto dropDown = [this](const QString &text, const QString &name) {
            QToolButton *b = new QToolButton(this);
            b->setObjectName(name);
            b->setText(text);
            b->setPopupMode(QToolButton::InstantPopup);
            b->setToolButtonStyle(Qt::ToolButtonTextOnly);
            QMenu *m = new QMenu(b);
            m->setToolTipsVisible(true);
            b->setMenu(m);
            return b;
        };
        auto mirror = [](QMenu *m, QPushButton *btn, const QString &text) {
            btn->hide();
            QAction *a = m->addAction(text);
            a->setToolTip(btn->toolTip());
            a->setCheckable(btn->isCheckable());
            QObject::connect(a, &QAction::triggered, btn, [btn]() { btn->click(); });
            QObject::connect(m, &QMenu::aboutToShow, a, [a, btn]() {
                a->setEnabled(btn->isEnabled());
                a->setChecked(btn->isChecked());
            });
            return a;
        };
        auto fieldAction = [this](QMenu *m, const QString &label, QWidget *field) {
            QWidget *row = new QWidget(this);
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(24, 2, 12, 2);
            rl->addWidget(new QLabel(label, row));
            rl->addStretch(1);
            field->setParent(row);
            rl->addWidget(field);
            QWidgetAction *wa = new QWidgetAction(m);
            wa->setDefaultWidget(row);
            m->addAction(wa);
        };

        QToolButton *parkMenu = dropDown(tr("Park"), QStringLiteral("parkMenu"));
        mirror(parkMenu->menu(), m_parkBtn, tr("Park selected fixtures"));
        mirror(parkMenu->menu(), m_unparkBtn, tr("Unpark all"));

        QToolButton *markMenu = dropDown(tr("Move in black"), QStringLiteral("markMenu"));
        mirror(markMenu->menu(), m_markBtn, tr("Mark selected (pre-position in black)"));
        mirror(markMenu->menu(), m_unmarkBtn, tr("Unmark all"));
        markMenu->menu()->addSeparator();
        mirror(markMenu->menu(), m_autoMibBtn, tr("Auto move-in-black"));
        fieldAction(markMenu->menu(), tr("Lead time"), m_mibGapSpin);

        QToolButton *beatMenu = dropDown(tr("Beat"), QStringLiteral("beatMenu"));
        mirror(beatMenu->menu(), m_bpmBtn, tr("Internal beat on"));
        fieldAction(beatMenu->menu(), tr("BPM"), m_bpmSpin);
        beatMenu->menu()->addSeparator();
        mastersBtn->hide();
        QAction *mastersAct = beatMenu->menu()->addAction(tr("Speed masters…"));
        mastersAct->setToolTip(mastersBtn->toolTip());
        connect(mastersAct, &QAction::triggered, mastersBtn, &QPushButton::click);
        // The button says the tempo, so it needn't be opened to read it.
        auto beatText = [beatMenu, this]() {
            beatMenu->setText(m_bpmBtn->isChecked() ? tr("Beat %1").arg(m_bpmSpin->value())
                                                    : tr("Beat off"));
        };
        connect(m_bpmBtn, &QPushButton::toggled, beatMenu, [beatText](bool) { beatText(); });
        connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged), beatMenu,
                [beatText](int) { beatText(); });
        beatText();

        QToolButton *moreMenu = dropDown(tr("More"), QStringLiteral("programmingMoreMenu"));
        mirror(moreMenu->menu(), m_snapshotBtn, tr("Snapshot live output into the scene"));
        fieldAction(moreMenu->menu(), tr("Jog speed"), m_speedSpin);

        // Highlight, Flash | Park, Move in black ... Beat, Tap | More, Save Positions
        while (toolbar->count() > 0)
            delete toolbar->takeAt(0);                 // the items, not the widgets
        delete toolbar;

        // A QToolBar, so the main window's top bar can borrow it while this
        // is the current tab (MainBar). It was a row of ~15 controls whose
        // summed minimum widths (~1866 px) held the whole window wider than
        // most screens, kept in check by a horizontal scroll area; a
        // toolbar folds what doesn't fit into its » menu instead.
        QToolBar *bar = new QToolBar(this);
        bar->setObjectName(QStringLiteral("programmingToolBar"));
        bar->setMovable(false);
        bar->setFloatable(false);
        bar->addWidget(m_highlightBtn);
        bar->addWidget(m_flashBtn);
        bar->addWidget(parkMenu);
        bar->addWidget(markMenu);
        {
            QWidget *stretch = new QWidget(bar);
            stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            bar->addWidget(stretch);
        }
        bar->addWidget(beatMenu);
        bar->addWidget(m_tapBtn);
        bar->addWidget(moreMenu);
        bar->addWidget(m_saveBtn);

        m_canvasLayout->addWidget(bar);
        MainBar::registerTabTools(this, bar);

        connect(m_highlightBtn, &QPushButton::toggled,
                this, &ProgrammingManager::slotHighlightToggled);
        connect(m_flashBtn, &QPushButton::clicked,
                this, &ProgrammingManager::slotFlashSelection);
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
    {
        QHBoxLayout *titleRow = new QHBoxLayout();
        titleRow->addWidget(m_canvasTitle, 1);
        m_groupBackButton = new QPushButton(this);
        m_groupBackButton->setToolTip(tr("Back to the scene that was open before this group"));
        m_groupBackButton->hide();
        connect(m_groupBackButton, &QPushButton::clicked, this, [this]() {
            const quint32 sid = m_groupReturnScene;
            if (m_doc->function(sid) != NULL)
                revealScene(sid);
        });
        titleRow->addWidget(m_groupBackButton);
        m_canvasLayout->addLayout(titleRow);
    }

    /* The page: what the selected outline row edits, with the room
       (PROGRAMMING_REDESIGN.md §2.3). One widget per kind of row. */
    m_page = new QStackedWidget(this);
    m_page->setObjectName(QStringLiteral("programmingPage"));
    m_canvasLayout->addWidget(m_page, 1);

    // Empty: one line plus a way in (the full explanation is behind the
    // button, and shows itself once on a first run).
    m_emptyPage = new QWidget(m_page);
    {
        QVBoxLayout *phl = new QVBoxLayout(m_emptyPage);
        phl->addStretch();
        m_canvasHint = new QLabel(tr("Select a row in the outline: a scene, one of its targets or "
                                     "one of its looks."), m_emptyPage);
        m_canvasHint->setWordWrap(true);
        m_canvasHint->setAlignment(Qt::AlignCenter);
        phl->addWidget(m_canvasHint);
        QHBoxLayout *bl = new QHBoxLayout;
        bl->addStretch();
        m_openInFunctionsBtn = new QPushButton(tr("Open in Functions"), m_emptyPage);
        m_openInFunctionsBtn->setObjectName(QStringLiteral("openInFunctions"));
        m_openInFunctionsBtn->hide();
        connect(m_openInFunctionsBtn, &QPushButton::clicked, this, [this]() { openInFunctions(m_pageFunction); });
        bl->addWidget(m_openInFunctionsBtn);
        QPushButton *guideBtn = new QPushButton(tr("How this works"), m_emptyPage);
        guideBtn->setFlat(true);
        guideBtn->setCursor(Qt::PointingHandCursor);
        connect(guideBtn, &QPushButton::clicked,
                this, [this]() { showProgrammingGuide(); });
        bl->addWidget(guideBtn);
        bl->addStretch();
        phl->addLayout(bl);
        phl->addStretch();
    }
    m_page->addWidget(m_emptyPage);

    // A scene: what it is, what's wrong with it, what fires it, its timing.
    m_scenePage = new QWidget(m_page);
    m_scenePage->setObjectName(QStringLiteral("programmingScenePage"));
    {
        QVBoxLayout *sl = new QVBoxLayout(m_scenePage);
        sl->setContentsMargins(4, 4, 4, 4);
        m_sceneHeading = new QLabel(m_scenePage);
        m_sceneHeading->setObjectName(QStringLiteral("sceneHeading"));
        m_sceneHeading->setStyleSheet(QStringLiteral("QLabel { font-size: 15pt; font-weight: bold; }"));
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
        QHBoxLayout *btns = new QHBoxLayout;
        QPushButton *dupBtn = new QPushButton(tr("Duplicate"), m_scenePage);
        dupBtn->setToolTip(tr("A copy, named on from this one"));
        connect(dupBtn, &QPushButton::clicked, this, [this]() {
            if (m_currentScene != Function::invalidId())
                duplicateFunctions({ m_currentScene });
        });
        QPushButton *snapBtn = new QPushButton(tr("Snapshot"), m_scenePage);
        snapBtn->setToolTip(m_snapshotBtn->toolTip());
        connect(snapBtn, &QPushButton::clicked, this, &ProgrammingManager::slotSnapshotLive);
        QPushButton *expandScene = new QPushButton(tr("Show its rows"), m_scenePage);
        expandScene->setToolTip(tr("Open this scene, and its effects, in the outline"));
        connect(expandScene, &QPushButton::clicked, this, [this]() {
            m_outline->setSubtreeExpanded(m_outline->sceneItem(m_currentScene), true);
        });
        btns->addWidget(dupBtn);
        btns->addWidget(snapBtn);
        btns->addWidget(expandScene);
        btns->addStretch();
        sl->addLayout(btns);
        QLabel *timing = new QLabel(tr("Timing"), m_scenePage);
        timing->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; margin-top: 8px; }"));
        sl->addWidget(timing);
        m_sceneCanvasLayout = new QVBoxLayout;
        m_sceneCanvasLayout->setContentsMargins(0, 0, 0, 0);
        sl->addLayout(m_sceneCanvasLayout);
        QLabel *how = new QLabel(tr("Its targets and looks are its rows in the outline: drag palettes, "
                                    "groups and fixtures onto it from the right, click a look to edit it."),
                                 m_scenePage);
        how->setWordWrap(true);
        how->setStyleSheet(QStringLiteral("QLabel { font-style: italic; margin-top: 8px; }"));
        sl->addWidget(how);
        sl->addStretch(1);
    }
    m_page->addWidget(m_scenePage);

    /* A look: the Look Editor at full height (it was what was left under the
       canvas). Scrolls when the window is shorter than it. */
    m_lookEditor = new LookEditor(m_doc, this);
    m_lookEditor->setObjectName("lookEditor");
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
        connect(m_targetLayoutBtn, &QPushButton::clicked, this, [this]() { loadGroupEditor(m_targetGroup); });
        hl->addWidget(m_targetLayoutBtn);
        targetCol->addLayout(hl);
    }
    m_page->addWidget(m_targetPage);

    // Something hosted whole: a group's layout editor, or a function opened
    // from elsewhere (showFunction()).
    m_functionPage = new QWidget(m_page);
    m_functionLayout = new QVBoxLayout(m_functionPage);
    m_functionLayout->setContentsMargins(0, 0, 0, 0);
    m_page->addWidget(m_functionPage);

    /* Once, on a first run -- queued, so it lands after the window is up rather
       than during construction. Never on a headless platform ("offscreen", or
       "minimal", which the Linux test runner uses under xvfb): nobody can read
       or dismiss a dialog there, and it hung a test on a fresh CI runner for
       five minutes waiting for a click. QLC_NO_RECOVERY_PROMPT, which scripted
       runs on a real display already set to skip the autosave prompt, skips it
       too: on a fresh Linux soak box under Xvfb it held up the web API that
       drives the run. The flag isn't consumed there, so the first real run
       still shows it. */
    const QString platform = QGuiApplication::platformName();
    if (platform != QLatin1String("offscreen") && platform != QLatin1String("minimal")
        && !qEnvironmentVariableIsSet("QLC_NO_RECOVERY_PROMPT")
        && takeFirstRunGuideFlag())
        QMetaObject::invokeMethod(this, [this]() { showProgrammingGuide(); },
                                  Qt::QueuedConnection);

    // Per-fixture DMX channel editor, shown on the target page when target
    // rows are selected. Scrolls inside the page if the console is taller.
    m_fixtureConsole = new FixtureConsole(this, m_doc);
    m_fixtureScroll = new QScrollArea(this);
    m_fixtureScroll->setWidgetResizable(true);
    m_fixtureScroll->setFrameShape(QFrame::NoFrame);
    m_fixtureScroll->setMinimumHeight(200);
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
    targetCol->addWidget(m_fixtureScroll, 1);

    // Shown instead of the console when several selected fixed fixtures are of
    // different types (editing them by channel index would be meaningless).
    m_fixtureMixedNote = new QLabel(this);
    m_fixtureMixedNote->setWordWrap(true);
    m_fixtureMixedNote->setAlignment(Qt::AlignCenter);
    m_fixtureMixedNote->setMinimumHeight(120);
    m_fixtureMixedNote->setStyleSheet("QLabel { color: gray; }");
    m_fixtureMixedNote->hide();
    targetCol->addWidget(m_fixtureMixedNote, 1);
    showPage(EmptyPage);

    // Estimated electrical load of the previewed look (Design mode only);
    // the readout is the app status-bar chip, this just drives the timer.
    initPowerTimer();

    connect(m_fixtureConsole, SIGNAL(valueChanged(quint32,quint32,uchar)),
            this, SLOT(slotFixtureValueChanged(quint32,quint32,uchar)));
    connect(m_fixtureConsole, SIGNAL(checked(quint32,quint32,bool)),
            this, SLOT(slotFixtureChecked(quint32,quint32,bool)));
    splitter->addWidget(canvasPanel);

    // --- Right: drag sources (palette tree + fixtures & groups tree) ---
    // ── Source panel: tabbed (Palettes | Bundles | Fixtures) ─────────────
    auto *sourceTab = new QTabWidget(this);
    sourceTab->setObjectName("programmingSources");
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
    m_paletteTree->setObjectName("programmingPalettes");
    m_paletteTree->setDisplayFilter(FunctionsTreeWidget::PalettesOnly);
    m_paletteTree->setHeaderHidden(true);
    m_paletteTree->setColumnHidden(1, true);
    m_paletteTree->setSortingEnabled(true);
    m_paletteTree->sortByColumn(0, Qt::AscendingOrder);
    m_paletteTree->setExternalDragMode(true);
    m_paletteTree->setAutoExpandDelay(600);
    m_paletteTree->setContextMenuPolicy(Qt::CustomContextMenu);
    // Multi-select so several palettes can be dragged onto a look together, or
    // saved as a Bundle in one action from the context menu.
    m_paletteTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Folders first (any palette type inside) unless the user chose to group
    // by type — persisted, toggled in the right-click menu.
    m_paletteTree->setPaletteTypesFirst(
        QSettings().value(QStringLiteral("programming/palettetree/groupByType"), false).toBool());
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
    // Find a group or fixture by name in a long list (as Layers does).
    QLineEdit *fixGroupFilter = new QLineEdit(sourcePanel);
    fixGroupFilter->setObjectName(QStringLiteral("fixGroupFilter"));
    fixGroupFilter->setPlaceholderText(tr("Search fixtures & groups…"));
    fixGroupFilter->setClearButtonEnabled(true);
    srcCol->addWidget(fixGroupFilter);
    m_fixGroupSource = new FixtureGroupSource(m_doc, sourcePanel);
    m_fixGroupSource->setObjectName("programmingFixturesGroups");
    srcCol->addWidget(m_fixGroupSource, 2);
    connect(fixGroupFilter, &QLineEdit::textChanged,
            m_fixGroupSource, &FixtureGroupSource::setFilterText);
    connect(m_fixGroupSource, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotFixGroupSourceSelectionChanged);
    // Double-click a group in the source tree -> visualize/edit its head layout
    // in the central canvas.
    connect(m_fixGroupSource, &FixtureGroupSource::groupDoubleClicked,
            this, &ProgrammingManager::loadGroupEditor);

    splitter->addWidget(sourcePanel);

    splitter->setStretchFactor(0, 2); // outline
    splitter->setStretchFactor(1, 4); // page
    splitter->setStretchFactor(2, 2); // sources

    // Where the handles were left, next time. ("outline": the panes were
    // the function tree and the canvas before; their sizes aren't these.)
    {
        QSettings st;
        splitter->restoreState(st.value(QStringLiteral("programming/outlineSplitter")).toByteArray());
    }
    connect(splitter, &QSplitter::splitterMoved, this, [splitter]() {
        QSettings().setValue(QStringLiteral("programming/outlineSplitter"), splitter->saveState());
    });

    // The outline: its selection fills the page; its requests are the tab's.
    connect(m_outline, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotOutlineSelectionChanged);
    connect(m_outline, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        const ProgrammingOutline::Row r = ProgrammingOutline::rowOf(it);
        if (r.kind == ProgrammingOutline::OtherRow)
            openInFunctions(r.id);
    });
    connect(m_outline, &ProgrammingOutline::pageRequested, this, [this]() {
        QWidget *page = m_page->currentWidget();
        page->setFocus(Qt::OtherFocusReason);
        if (QWidget *next = page->nextInFocusChain())
            if (page->isAncestorOf(next))
                next->setFocus(Qt::TabFocusReason);
    });
    connect(m_outline, &ProgrammingOutline::scenesEdited, this, [this](const QList<quint32> &ids) {
        if (!ids.contains(m_currentScene))
            return;
        if (m_canvas != NULL)
            m_canvas->reload();
        if (m_trialScene != Function::invalidId() || !m_lookEditor->trialNote().isEmpty())
            updateTrialPalette(m_lookEditor->paletteId());
        refreshPreview();
        recomputePower();
        updateScenePage();
    });
    connect(m_outline, &ProgrammingOutline::newSceneRequested, this, [this](const QString &folder) {
        addNewFunction(Function::SceneType, folder);
    });
    connect(m_outline, &ProgrammingOutline::duplicateRequested, this, &ProgrammingManager::duplicateFunctions);
    connect(m_outline, &ProgrammingOutline::deleteScenesRequested, this, &ProgrammingManager::deleteFunctions);
    connect(m_outline, &ProgrammingOutline::openInFunctionsRequested, this, &ProgrammingManager::openInFunctions);
    connect(m_outline, &ProgrammingOutline::editGroupLayoutRequested, this, &ProgrammingManager::loadGroupEditor);
    connect(m_outline, &ProgrammingOutline::revealPaletteRequested, this, &ProgrammingManager::revealPalette);

    connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
        // The canvas (SceneGroupLooks), the look editor and a hosted editor
        // all hold a raw Function*/Scene*. Doc emits this BEFORE it deletes
        // the function, so tear them down now — otherwise the next
        // interaction dereferences freed memory.
        if (id == m_currentScene || id == m_canvasFunction)
        {
            loadCanvas(Function::invalidId());
            showPage(EmptyPage);
        }
    });
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 id) {
        if (id == m_currentScene)
            QTimer::singleShot(0, this, [this]() { updateScenePage(); });
    });
    connect(m_doc, &Doc::cleared, this, [this]() {
        m_currentScene   = Function::invalidId();
        m_canvasFunction = Function::invalidId();
        m_trialScene     = Function::invalidId();   // ids are reused by the next show
        clearEditors();
        m_editBaseline.reset();
        showPage(EmptyPage);
    });
    // Any step on the shared history (here, in the Show Tree): the open
    // scene as it now is becomes the baseline, and shows as it now is.
    if (PaletteOps *ops = m_doc->paletteOps())
        connect(ops, &PaletteOps::changed, this, [this]() {
            recaptureBaseline();
            // An undo / redo from either tab may have changed the open scene.
            if (m_refreshQueued)
                return;
            m_refreshQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_refreshQueued = false;
                if (m_canvas != NULL)
                    m_canvas->reload();
                refreshPreview();
                updateScenePage();
            });
        });

    connect(m_doc, &Doc::paletteAdded, m_paletteTree, &FunctionsTreeWidget::addPalette);
    connect(m_doc, &Doc::paletteRemoved, this, [this](quint32) { m_paletteTree->updateTree(); });
    connect(m_doc, &Doc::loaded, m_paletteTree, &FunctionsTreeWidget::updateTree);
    connect(m_doc, &Doc::cleared, m_paletteTree, &FunctionsTreeWidget::updateTree);

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
    connect(m_lookEditor, SIGNAL(paletteChanged(quint32)),
            this, SLOT(slotLookEdited(quint32)));
    connect(m_lookEditor, &LookEditor::lookFocusChanged,
            this, &ProgrammingManager::currentPaletteIdChanged);
    connect(m_lookEditor, &LookEditor::lookFocusChanged,
            this, &ProgrammingManager::updateTrialPalette);
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

    // ⌘Z / ⇧⌘Z: canvas edits, a bundle stamp and the shared history
    // (the Show Tree's and the palette operations'), newest first.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &ProgrammingManager::slotUndo);
    QShortcut *redoSc = new QShortcut(QKeySequence::Redo, this);
    redoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoSc, &QShortcut::activated, this, &ProgrammingManager::slotRedo);

    // Delete / Backspace (⌫ on a Mac) on the palette tree: delete what's
    // selected — asking first, with where it's used. (The outline takes its
    // own: a row comes out of its scene.)
    for (FunctionsTreeWidget *tree : { m_paletteTree })
    {
        for (const QKeySequence &key : { QKeySequence(Qt::Key_Delete),
                                         QKeySequence(Qt::Key_Backspace) })
        {
            QShortcut *sc = new QShortcut(key, tree);
            sc->setContext(Qt::WidgetShortcut);
            // WidgetShortcut: only with the tree itself focused, so ⌫ in an
            // inline rename editor stays a text edit.
            connect(sc, &QShortcut::activated, this, [this, tree]() {
                deleteTreeSelection(tree);
            });
        }
    }
}

ProgrammingManager::~ProgrammingManager()
{
    stopPreview();
}

void ProgrammingManager::slotOutlineSelectionChanged()
{
    /* The page follows the outline: a scene row edits the scene, a look row
       its palette (the Look Editor), a target row its channels. With several
       rows selected, the current one says which page; the others still drag,
       remove and move with it. */
    QTreeWidgetItem *cur = m_outline->currentItem();
    if (cur == nullptr || !cur->isSelected())
    {
        const QList<QTreeWidgetItem *> sel = m_outline->selectedItems();
        cur = sel.isEmpty() ? nullptr : sel.first();
    }
    if (cur == nullptr)
        return;
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(cur);
    m_pageFunction = Function::invalidId();
    m_openInFunctionsBtn->hide();

    switch (row.kind)
    {
    case ProgrammingOutline::SceneRow:
        loadCanvas(row.id);
        m_lookEditor->setPalette(QLCPalette::invalidId());
        m_doc->setFocusedPalette(QLCPalette::invalidId());
        updateScenePage();
        showPage(ScenePage);
        break;
    case ProgrammingOutline::PaletteRow:
        loadCanvas(row.scene);
        m_lookEditor->setPalette(row.id);
        m_doc->setFocusedPalette(row.id);
        showPage(LookPage);
        break;
    case ProgrammingOutline::GroupRow:
    case ProgrammingOutline::FixtureRow:
    {
        loadCanvas(row.scene);
        // Every target row selected in this scene: groups by their members.
        QList<quint32> fixtures;
        QStringList names;
        m_targetGroup = row.kind == ProgrammingOutline::GroupRow ? row.id : UINT_MAX;
        for (const ProgrammingOutline::Row &r : m_outline->selectedRows())
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
        slotFixturesSelected(fixtures);
        showPage(TargetPage);
        break;
    }
    case ProgrammingOutline::OtherRow:
    {
        Function *f = m_doc->function(row.id);
        loadCanvas(Function::invalidId());
        if (f != nullptr)
        {
            m_pageFunction = f->id();
            m_canvasHint->setText(tr("“%1” is a %2: not a scene, so it isn't edited here.")
                                      .arg(f->name(), Function::typeToString(f->type()).toLower()));
            m_openInFunctionsBtn->show();
        }
        showPage(EmptyPage);
        break;
    }
    case ProgrammingOutline::FolderRow:
    {
        loadCanvas(Function::invalidId());
        std::function<int(QTreeWidgetItem *)> count = [&count](QTreeWidgetItem *it) {
            int n = it->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::SceneRow ? 1 : 0;
            if (n == 0)
                for (int i = 0; i < it->childCount(); ++i)
                    n += count(it->child(i));
            return n;
        };
        const int scenes = count(cur);
        m_canvasHint->setText(tr("Folder “%1”: %n scene(s). Open a scene in it to edit it, "
                                 "or + Scene to add one here.", "", scenes)
                                  .arg(cur->data(0, ProgrammingOutline::PathRole).toString()));
        showPage(EmptyPage);
        break;
    }
    default:
        break;
    }
}

void ProgrammingManager::showPage(Page page)
{
    QWidget *w = page == ScenePage ? m_scenePage : page == LookPage ? m_lookPage
               : page == TargetPage ? m_targetPage : page == FunctionPage ? m_functionPage
               : m_emptyPage;
    if (page == EmptyPage && m_pageFunction == Function::invalidId() && m_currentScene == Function::invalidId()
        && m_outline != nullptr && m_outline->selectedItems().isEmpty())
        m_canvasHint->setText(tr("Select a row in the outline: a scene, one of its targets or "
                                 "one of its looks."));
    m_page->setCurrentWidget(w);
    // The fixture console's channels are drawn when it shows (a switch from
    // one fixture to another fires no showEvent).
    if (page == TargetPage && !m_fixtureScroll->isHidden())
        m_fixtureConsole->showChannels();
}

ProgrammingManager::Page ProgrammingManager::currentPage() const
{
    QWidget *w = m_page->currentWidget();
    return w == m_scenePage ? ScenePage : w == m_lookPage ? LookPage
         : w == m_targetPage ? TargetPage : w == m_functionPage ? FunctionPage : EmptyPage;
}

void ProgrammingManager::updateScenePage()
{
    Scene *scene = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (scene == nullptr || m_sceneHeading == nullptr)
        return;
    m_sceneHeading->setText(scene->name());

    QSet<quint32> fixtures;
    for (quint32 gid : scene->fixtureGroups())
        if (FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (quint32 fid : g->fixtureList())
                fixtures.insert(fid);
    for (quint32 fid : scene->fixtures())
        fixtures.insert(fid);
    QStringList lines;
    lines << (scene->path(true).isEmpty() ? tr("Not in a folder") : tr("In %1").arg(scene->path(true)));
    lines << tr("%n target(s)", "", scene->fixtureGroups().size() + scene->fixtures().size())
             + QStringLiteral(" · ") + tr("%n look(s)", "", scene->palettes().size())
             + QStringLiteral(" · ") + tr("%n fixture(s)", "", fixtures.size());
    if (!scene->values().isEmpty())
        lines << tr("Sets %n raw channel value(s) directly", "", scene->values().size());
    const QStringList vc = UsedBy::vcLines({ scene->id() });
    lines << (vc.isEmpty() ? tr("Fired by: nothing on the Virtual Console")
                           : tr("Fired by: %1").arg(vc.join(QStringLiteral(", "))));
    const int uses = functionUsageCount(scene->id());
    if (uses > 0)
        lines << tr("Used in %n chaser(s) or collection(s)", "", uses);
    m_sceneSummary->setText(lines.join(QLatin1Char('\n')));

    // The same notes the outline's ⚠ counts, in full.
    QStringList warn;
    if (QTreeWidgetItem *it = m_outline->sceneItem(scene->id()))
        if (it->text(ProgrammingOutline::BadgeCol).contains(QStringLiteral("⚠")))
            for (const QString &l : it->toolTip(ProgrammingOutline::BadgeCol).split(QLatin1Char('\n')))
                if (!l.startsWith(tr("Fired by")) && !l.startsWith(tr("Sets ")) && !l.startsWith(tr("Has a transition"))
                    && !l.startsWith(tr("Its effects keep time")))
                    warn << QStringLiteral("⚠ ") + l;
    m_sceneWarnings->setText(warn.join(QLatin1Char('\n')));
    m_sceneWarnings->setVisible(!warn.isEmpty());
}

void ProgrammingManager::openInFunctions(quint32 fid)
{
    FunctionManager *fm = FunctionManager::instance();
    if (fm == nullptr || m_doc->function(fid) == nullptr)
        return;
    // The tab that holds the Function Manager, wherever the window keeps it.
    for (QWidget *w = fm->parentWidget(); w != nullptr; w = w->parentWidget())
        if (QTabWidget *tabs = qobject_cast<QTabWidget *>(w))
            if (tabs->indexOf(fm) >= 0)
            {
                tabs->setCurrentWidget(fm);
                break;
            }
    fm->selectFunction(fid);
}

void ProgrammingManager::revealScene(quint32 sceneId)
{
    ProgrammingOutline::Row r;
    r.kind = ProgrammingOutline::SceneRow;
    r.scene = r.id = sceneId;
    if (!m_outline->selectRow(r))
    {
        loadCanvas(sceneId);
        updateScenePage();
        showPage(ScenePage);
    }
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

    m_groupBackButton->hide();
    stopPreview();
    m_currentScene = sceneId;
    m_canvasFunction = sceneId;
    m_lastSyncedEffectPalettes.clear(); // new scene — force re-sync on first refreshPreview
    m_lookEditor->setPalette(QLCPalette::invalidId());
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(sceneId);
    clearEditors();

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
    {
        m_doc->setFocusedScene(Function::invalidId());
        if (ProgrammerController *pc = m_doc->programmer())
            pc->seedStageAimFromScene(Function::invalidId());
        m_editBaseline.reset();
        updateTitle();
        return;
    }

    m_selectedFixtures.clear();
    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();
    m_lookEditor->setContextScene(scene);
    m_doc->setFocusedScene(sceneId);
    recaptureBaseline();     // edits on the page record from here

    /* The scene's timing row (fades, transitions, speed), on the scene
       page. Its Targets and Looks lists are the outline's rows now, so
       they're kept hidden (the drop logic and the tests still reach them). */
    m_canvas = new SceneGroupLooks(scene, m_doc, m_scenePage, /*includeFixtureTargets*/ true);
    m_canvas->setListsVisible(false);
    // Only its edits: what is selected is the outline's to say.
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    m_sceneCanvasLayout->addWidget(m_canvas);

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

    m_groupBackButton->hide();
    stopPreview();
    m_currentScene = Function::invalidId();
    m_editBaseline.reset();
    m_canvasFunction = f->id();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(Function::invalidId());
    clearEditors();
    m_fixtureScroll->hide(); // non-scene: no per-fixture editor
    m_fixtureMixedNote->hide();

    // Host the stock editor for the function type; collections and chasers
    // accept functions dragged from the left tree (external drag mode).
    QWidget *ed = NULL;
    bool dragIn = false;
    switch (f->type())
    {
    case Function::CollectionType:
        if (m_doc->showTree()->isGenerated(f->id()))
        {
            // A cue's collection (D13): edited in the Show Tree.
            ed = ShowTreeView::lockedBanner(this, m_doc, f->id());
            break;
        }
        ed = new CollectionEditor(this, qobject_cast<Collection*>(f), m_doc);
        dragIn = true;
        break;
    case Function::ChaserType:
        ed = new ChaserEditor(this, qobject_cast<Chaser*>(f), m_doc);
        // A cue list's chaser (D13): its steps and name are the Show Tree's.
        if (m_doc->showTree()->isGenerated(f->id()))
            static_cast<ChaserEditor*>(ed)->setLocked(ShowTreeView::lockedBanner(ed, m_doc, f->id()));
        else
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

    Q_UNUSED(dragIn)

    if (ed == NULL)
    {
        m_pageFunction = f->id();
        m_canvasHint->setText(
            tr("\"%1\" (%2) — open it in the Functions tab to edit.")
            .arg(f->name()).arg(Function::typeToString(f->type())));
        m_openInFunctionsBtn->show();
        showPage(EmptyPage);
        m_canvasFunction = Function::invalidId(); // nothing previewable shown
        updateTitle();
        return;
    }

    m_funcEditor = ed;
    m_functionLayout->addWidget(m_funcEditor, 1);
    m_funcEditor->show();
    showPage(FunctionPage);
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

    // Remember the scene to go back to (not another group: the scene before).
    if (m_currentScene != Function::invalidId())
        m_groupReturnScene = m_currentScene;
    Function *back = m_doc->function(m_groupReturnScene);

    stopPreview();
    m_currentScene = Function::invalidId();
    m_editBaseline.reset();
    m_canvasFunction = Function::invalidId(); // nothing previewable
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    if (Monitor::instance() != NULL)
        Monitor::instance()->setActiveScene(Function::invalidId());

    clearEditors();

    m_fixtureScroll->hide();
    m_fixtureMixedNote->hide();

    // Reuse the func-editor slot as the hosted widget so clearEditors()
    // tears it down on the next selection.
    m_funcEditor = new FixtureGroupEditor(grp, m_doc, this);
    m_functionLayout->addWidget(m_funcEditor, 1);
    m_funcEditor->show();
    showPage(FunctionPage);

    m_canvasTitle->setText(tr("Fixture group: %1   —   %n head(s)", "", grp->headList().count())
                           .arg(grp->name()));
    if (back != NULL)
    {
        m_groupBackButton->setText(tr("← Back to %1").arg(back->name()));
        m_groupBackButton->show();
    }
    else
        m_groupBackButton->hide();
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

void ProgrammingManager::slotLookEdited(quint32 paletteId)
{
    // A look's value changed (including name edits) in the editor:
    // refresh the preview and update any tree/canvas items that show the name.
    refreshPreview();
    // Update the palette tree so a renamed palette shows its new name.
    m_paletteTree->updateTree();
    // Update the canvas's looks list (tiles show palette name), and the
    // outline's rows (names, swatches, fade badges).
    if (m_canvas)
        m_canvas->reload();
    m_outline->scheduleRebuild();
    // A look's own fade is the scene's: one step on the history.
    recordSceneEdit(tr("edit a look's fade in %1").arg(m_doc->function(m_currentScene) != nullptr
                                                        ? m_doc->function(m_currentScene)->name() : QString()),
                    QStringLiteral("lookfade:%1").arg(paletteId));

    /* "Didn't change colour when colour was changed in program from white to
       blue." This refreshed the Programming tab's OWN preview, but nothing
       here ever told the rest of the app the SCENE changed -- a palette is
       not a Function, so editing one is invisible to anything listening for
       Doc::functionChanged(), which is exactly what the rig view (and
       anything else caching a resolved look, e.g. StructureStudioView) waits
       for. Every scene that actually uses this palette gets told, since one
       palette can be shared across several looks. */
    foreach (Function *f, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene*>(f);
        if (s != nullptr && (s->palettes().contains(paletteId)
                             || s->trialPalette() == paletteId))
            m_doc->notifyFunctionChanged(s->id());
    }
}

void ProgrammingManager::updateTrialPalette(quint32 paletteId)
{
    /* "I just tried a new pan/tilt ... moved it around and the rig isn't
       following." A palette only reaches the rig through a look that plays
       it, and a new one is in none -- so the pad moved a value nothing sent.
       While it is being edited, it is tried on the look on the canvas: played
       over the look's own (Scene::runtimePalettes()), never saved into it. */
    Scene *old = qobject_cast<Scene*>(m_doc->function(m_trialScene));
    if (old != nullptr)
    {
        old->setTrialPalette(QLCPalette::invalidId());
        m_doc->notifyFunctionChanged(old->id());
    }
    m_trialScene = Function::invalidId();

    QLCPalette *p = m_doc->palette(paletteId);
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
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
    // Re-play only the look on the canvas: a look the canvas has left was
    // already stopped, and refreshPreview() STARTS whatever is on the canvas
    // (never wanted for, say, a Show that was just opened).
    const bool touchesCanvas = (old != nullptr && old->id() == m_currentScene)
                               || m_trialScene != Function::invalidId();
    if (touchesCanvas && m_currentScene == m_canvasFunction)
        refreshPreview();
}

void ProgrammingManager::createPalette(int paletteType)
{
    const QLCPalette::PaletteType type =
        static_cast<QLCPalette::PaletteType>(paletteType);

    // Unfiled, unless a folder (or a palette in one) is selected: then there.
    // Folders hold any palette type; the type is the palette's own.
    QString path;
    const QList<QTreeWidgetItem*> sel = m_paletteTree->selectedItems();
    if (sel.isEmpty() == false)
    {
        const QString cp = sel.first()->text(1); // folder COL_PATH ("Palettes/…")
        if (cp.isEmpty())
        {
            QLCPalette *sp = m_doc->palette(m_paletteTree->itemPaletteId(sel.first()));
            if (sp != NULL)
                path = sp->path();
        }
        else
        {
            // A tree folder (by type: under its type heading) to the
            // stored, type-independent path.
            const QString stored = m_paletteTree->paletteStorePath(cp);
            if (stored != QLatin1String("Palettes"))
                path = stored + QLatin1Char('/');
        }
    }

    // The shared operation (PaletteOps, as the Show Tree's Library): a
    // start value so it is valid and saves, one palette undo step.
    const quint32 pid = m_doc->paletteOps()->create(type, path);
    if (pid == QLCPalette::invalidId())
        return;
    revealPalette(pid);            // select + scroll to it in the source tree
    m_lookEditor->setPalette(pid); // edit it inline immediately
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
        /* From a fixture group's layout view: that view fills the canvas, and
           the look editor opened under it had no room ("clicked back on an
           effect .. I can't get back to the effect"). Go back to the scene
           first, then open the look on it. */
        if (!m_groupBackButton->isHidden() && m_doc->function(m_groupReturnScene) != NULL)
            revealScene(m_groupReturnScene);
        m_lookEditor->setPalette(pid);
        showLookEditorPanel(); // pivot from the DMX console if it's showing
    }
}

void ProgrammingManager::showLookEditorPanel()
{
    showPage(LookPage);
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
    /* A look is being edited and this is the same selection, re-announced by
       the canvas rebuilding after that very edit (every colour slider tick
       reloads it): keep the look editor. Swapping in the fixtures' faders
       here threw the user out of the colour picker on every click ("it drops
       out of the color picker"). A real change of selection still swaps. */
    if (currentPage() == LookPage && fixtureIds == m_selectedFixtures
        && m_currentScene == m_selectedFixturesScene)
        return;
    m_selectedFixtures = fixtureIds;
    m_selectedFixturesScene = m_currentScene;
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
        if (currentPage() == TargetPage)
            showPage(s != NULL ? ScenePage : EmptyPage);
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
        m_fixtureScroll->hide();
        m_fixtureMixedNote->setText(
            tr("%n fixed fixture(s) of different types are selected.\n"
               "Select fixtures of the same type to edit their DMX channels together.",
               "", fixtureIds.size()));
        m_fixtureMixedNote->show();
        showPage(TargetPage);
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

    m_fixtureMixedNote->hide();
    m_fixtureScroll->show();
    showPage(TargetPage);
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
    // A fader drag is one step.
    recordSceneEdit(tr("set channels in %1").arg(s->name()), QStringLiteral("channel:%1").arg(ch));
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
    recordSceneEdit(tr("release a channel in %1").arg(s->name()));
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

    for (const ProgrammingOutline::Row &r : m_outline->selectedRows())
        if (r.kind == ProgrammingOutline::SceneRow)
        {
            m_clipboardFunction = r.id;
            return;
        }
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
    if (copy->type() == Function::SceneType)
    {
        m_outline->rebuild();
        revealScene(copy->id());
    }
}

void ProgrammingManager::duplicatePalette(quint32 pid)
{
    const quint32 copyId = m_doc->paletteOps()->duplicate(pid);
    QLCPalette *copy = m_doc->palette(copyId);
    if (copy == NULL)
        return;
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
    // Dropping the palette being tried on INTO the look ends the trial.
    if (m_trialScene != Function::invalidId() || !m_lookEditor->trialNote().isEmpty())
        updateTrialPalette(m_lookEditor->paletteId());
    // One step on the shared history, from the baseline (the scene as of
    // the last step) to now.
    recordSceneEdit(tr("edit %1").arg(m_doc->function(m_currentScene) != nullptr
                                      ? m_doc->function(m_currentScene)->name() : QString()));
    updateScenePage();
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

void ProgrammingManager::initPowerTimer()
{
    // Low-frequency refresh so a held fader / palette change settles into the
    // reading without us hooking every DMX write. Design-mode-gated below.
    m_powerTimer = new QTimer(this);
    m_powerTimer->setInterval(500);
    connect(m_powerTimer, &QTimer::timeout, this, &ProgrammingManager::recomputePower);
}

void ProgrammingManager::updatePowerEstimateActive()
{
    // The estimate lives in the app status bar (always visible in Design
    // mode), so the timer runs whenever we're in Design mode — regardless of
    // which tab is on screen.
    const bool active = (m_doc->mode() == Doc::Design);
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
            emit powerEstimateChanged(0.0, 0.0, false, QString());   // clear the chip in Operate
        }
    }
}

void ProgrammingManager::recomputePower()
{
    // Design mode only (the estimate is a "designed peak"); runs across tabs now
    // that the readout is in the always-visible status bar.
    if (m_doc->mode() != Doc::Design)
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
    QHash<quint32, double> fixtureAmps;   // for the tooltip's breakdown

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
                fixtureAmps.insert(fxId, perFixture.value(fxId, 0.0) / cir.effectiveVoltage(srcV));
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
        fixtureAmps.insert(it.key(), it.value() / defaultVoltage);
    }

    // Push the estimate to the app's always-visible status-bar chip.
    const QString detail = powerDetailText(PowerEstimator::byType(m_doc, perFixture, fixtureAmps),
                                           totalAmps, totalWatts / 1000.0, defaultVoltage);
    emit powerEstimateChanged(totalAmps, totalWatts / 1000.0, overload, detail);
}

QString ProgrammingManager::powerDetailText(const QList<PowerEstimator::TypeLoad> &rows,
                                            double amps, double kw, double defaultVoltage)
{
    auto watts = [](double w) {
        return (w >= 1000.0) ? tr("%1 kW").arg(w / 1000.0, 0, 'f', 2)
                             : tr("%1 W").arg(w, 0, 'f', 0);
    };

    QStringList lines;
    lines << tr("Estimated load: %1 A, %2 kW").arg(amps, 0, 'f', 1).arg(kw, 0, 'f', 2);
    lines << tr("What is lit now, before the Grand Master; at %1 V where no circuit gives a voltage.")
                 .arg(defaultVoltage, 0, 'f', 0);

    const int shown = 6;
    QStringList guessed;
    bool anyRows = false;
    for (int i = 0; i < rows.size(); ++i)
    {
        const PowerEstimator::TypeLoad &t = rows.at(i);
        if (t.basis == PowerEstimator::Guessed)
            guessed << t.name;
        if (i >= shown || t.watts <= 0.0)
            continue;
        if (!anyRows)
        {
            lines << QString() << tr("Biggest draws:");
            anyRows = true;
        }
        QString how;
        switch (t.basis)
        {
            case PowerEstimator::Measured:
                how = tr("measured, %1 each at full").arg(watts(t.fullWatts));
                // A reading far over the definition's rating is worth a second look.
                if (t.ratedWatts > 0 && t.fullWatts > 2.0 * t.ratedWatts)
                    how += tr(" -- the definition says %1; check the readings").arg(watts(t.ratedWatts));
                break;
            case PowerEstimator::Rated:
                how = tr("rated %1 each").arg(watts(t.fullWatts));
                break;
            case PowerEstimator::Guessed:
                how = tr("no power in its definition: guessed at %1 each").arg(watts(t.fullWatts));
                break;
            case PowerEstimator::NoLoad:
                break;
        }
        lines << tr("  %1 x %2: %3 A (%4), %5").arg(t.count).arg(t.name)
                     .arg(t.amps, 0, 'f', 1).arg(watts(t.watts)).arg(how);
    }

    lines << QString()
          << tr("Each fixture draws its full power times its level (dimmer times the "
                "average of its colours); dark, a mover idles at 15% of it and anything "
                "else at 2 W. A measured fixture uses its own readings.");
    if (!guessed.isEmpty())
        lines << tr("No power given for: %1. Set the power in the definition (or measure "
                    "the fixture) for a real figure.").arg(guessed.join(QStringLiteral(", ")));
    lines << QString() << tr("Click to open the circuits editor.");
    return lines.join(QLatin1Char('\n'));
}

void ProgrammingManager::openCircuitsDialog()
{
    PowerDistributionDialog dlg(m_doc, this);
    dlg.exec();
    // Assignments/ratings may have changed; refresh the footer.
    recomputePower();
}

void ProgrammingManager::applyToolbarLabelMode()
{
    // Mirror App::TabLabelMode: 0 = Icon+Text (-> text beside icon here —
    // this button sits in a single-row toolbar, not a multi-row one, so
    // "beside" reads better than "under"), 1 = Icons only, 2 = Text only.
    // Same "workspace/tabLabelMode" setting as every other manager's
    // toolbar.
    Qt::ToolButtonStyle style = Qt::ToolButtonTextBesideIcon;
    const int mode = QSettings().value(QStringLiteral("workspace/tabLabelMode"), 0).toInt();
    if (mode == 1)
        style = Qt::ToolButtonIconOnly;
    else if (mode == 2)
        style = Qt::ToolButtonTextOnly;

    if (m_addFuncBtn)
        m_addFuncBtn->setToolButtonStyle(style);
}

quint32 ProgrammingManager::currentPaletteId() const
{
    return m_lookEditor ? m_lookEditor->paletteId() : QLCPalette::invalidId();
}

quint32 ProgrammingManager::currentSceneId() const
{
    return m_currentScene;
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
        updatePowerEstimateActive(); // back in Design → start timer

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
        updatePowerEstimateActive(); // Operate → stop timer, clear the chip

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
    // Connect to 2D monitor fixture selection so clicks there match Targets list.
    if (Monitor *mon = Monitor::instance())
        connect(mon, &Monitor::fixturesSelected,
                this, &ProgrammingManager::slotFixturesSelected, Qt::UniqueConnection);
    // Auto-bind joystick axes from I/O → Mapping → Joystick profile if not yet bound.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->autoBindFromProfile();
    startPreview();
    updatePowerEstimateActive();
    slotParkChanged(); // reflect any parks restored from the workspace
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
    updatePowerEstimateActive(); // stops the timer
    QWidget::hideEvent(ev);
}

void ProgrammingManager::addNewFunction(Function::Type type, const QString &folder)
{
    Function *f = NULL;
    QString base;
    switch (type)
    {
        case Function::SceneType:      f = new Scene(m_doc);      base = tr("New Scene"); break;
        case Function::ChaserType:     f = new Chaser(m_doc);     base = tr("New Chaser"); break;
        case Function::CollectionType: f = new Collection(m_doc); base = tr("New Collection"); break;
        case Function::EFXType:        f = new EFX(m_doc);        base = tr("New EFX"); break;
        case Function::RGBMatrixType:  f = new RGBMatrix(m_doc);  base = tr("New RGB Matrix"); break;
        case Function::ShowType:       f = new Show(m_doc);       base = tr("New Show"); break;
        default: return;
    }
    if (f == NULL)
        return;

    if (folder.isEmpty() == false)
        f->setPath(folder);

    if (m_doc->addFunction(f) == false)
    {
        delete f;
        return;
    }
    f->setName(QString("%1 %2").arg(base).arg(f->id()));

    // Selected in the outline, its page open, ready for F2 and drops.
    if (f->type() == Function::SceneType)
    {
        m_outline->rebuild();
        revealScene(f->id());
    }
    else
        loadFunctionEditor(f);
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

    if (last->type() == Function::SceneType)
    {
        m_outline->rebuild();
        revealScene(last->id());
    }
}

void ProgrammingManager::deleteFunctions(const QList<quint32> &fids)
{
    if (fids.isEmpty())
        return;
    // What the Show Tree made goes with its cue list, there (D13).
    if (ShowTreeView::refuseGenerated(this, m_doc, fids, tr("Delete Functions")))
        return;

    const QString msg = deleteFunctionsMessage(fids);
    if (msg.isEmpty())
        return;

    if (QMessageBox::question(this, tr("Delete Functions"), msg,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
        return;

    // Delete by id, never by tree item: the first deletion emits functionRemoved,
    // which rebuilds the outline (and tears down the canvas if it held one of
    // these) — so any QTreeWidgetItem* captured before the loop would dangle.
    for (quint32 fid : fids)
        m_doc->deleteFunction(fid);
}

// "Where is this used" lines for a delete question, capped so the dialog
// stays on screen.
static QString usageBlock(const QStringList &lines)
{
    const int kMax = 12;
    QStringList shown = lines.mid(0, kMax);
    for (QString &l : shown)
        l.prepend(QStringLiteral("  • "));
    QString out = shown.join("\n");
    if (lines.size() > kMax)
        out += "\n" + QObject::tr("  … and %1 more").arg(lines.size() - kMax);
    return out;
}

QString ProgrammingManager::deleteFunctionsMessage(const QList<quint32> &fids) const
{
    ProgrammerController *pc = m_doc->programmer();
    QStringList names;
    QString used;
    int usedCount = 0;
    for (quint32 fid : fids)
    {
        Function *f = m_doc->function(fid);
        if (f == NULL)
            continue;
        names.append(f->name());
        const QStringList lines = (pc != NULL)
            ? ProgrammerController::usageLines(pc->functionUsage(fid)) : QStringList();
        if (lines.isEmpty() == false)
        {
            usedCount++;
            used += QString("\n\n") + tr("“%1” is used in %n place(s):", "", lines.size())
                        .arg(f->name())
                    + "\n" + usageBlock(lines);
        }
    }
    if (names.isEmpty())
        return QString();

    QString msg = names.size() > 1
        ? tr("Delete %1 functions?\n\n%2").arg(names.size()).arg(names.join(", "))
        : tr("Delete function “%1”?").arg(names.first());
    // These may be shown nested under a collection/chaser/show; deleting is
    // GLOBAL, not an unlink — say so, and where they're used.
    msg += QString("\n\n") + tr("This deletes the function itself, everywhere — "
                                 "not just from one collection or chaser. "
                                 "To only take it out of one, use “Remove from …”.");
    if (usedCount > 0)
        msg += used + QString("\n\n")
             + tr("Deleting takes it out of all of those.");
    else
        msg += QString("\n\n") + tr("Nothing else uses it.");
    return msg;
}

QString ProgrammingManager::deletePalettesMessage(const QList<quint32> &pids) const
{
    // The one Used by (P-D13): cues and looks by name, scenes, effect
    // slots and the VC buttons firing them -- as the Show Tree asks.
    return UsedBy::deletePalettesQuestion(m_doc, pids);
}

bool ProgrammingManager::deletePalettes(const QList<quint32> &pids, bool confirm)
{
    const QString msg = deletePalettesMessage(pids);
    if (msg.isEmpty())
        return false;
    if (confirm && QMessageBox::question(this,
            pids.size() > 1 ? tr("Delete palettes") : tr("Delete palette"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;

    // One palette undo step (PaletteOps), every use cleared with them (open
    // 26): Undo -- here or in the Show Tree -- brings them back, in every
    // scene, slot, cue and look they were in.
    if (!m_doc->paletteOps()->remove(pids))
        return false;
    m_paletteTree->updateTree();
    return true;
}

void ProgrammingManager::deleteTreeSelection(FunctionsTreeWidget *tree)
{
    if (tree == NULL)
        return;
    const QList<QTreeWidgetItem*> sel = tree->selectedItems();
    if (sel.isEmpty())
        return;

    // A lone folder: delete it (contents move up).
    if (sel.size() == 1 && tree->isUserFolder(sel.first()))
    {
        deleteTreeFolder(tree, sel.first());
        return;
    }

    QList<quint32> pids;
    for (QTreeWidgetItem *it : sel)
    {
        const quint32 pid = tree->itemPaletteId(it);
        if (pid != QLCPalette::invalidId() && !pids.contains(pid))
            pids << pid;
    }
    deletePalettes(pids);
}

void ProgrammingManager::slotPaletteTreeMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.setToolTipsVisible(true);   // off by default on most platforms (incl. macOS)
    // Type list, like the function tree's New menu. Only the types the
    // engine resolves per-fixture are offered.
    struct { const char *label; int type; const char *tip; } types[] = {
        { QT_TR_NOOP("New Color"),      QLCPalette::Color,
          QT_TR_NOOP("RGB(+White/Amber/UV) mix, applied per fixture.") },
        { QT_TR_NOOP("New Dimmer"),     QLCPalette::Dimmer,
          QT_TR_NOOP("A single intensity value.") },
        { QT_TR_NOOP("New Pan/Tilt"),   QLCPalette::PanTilt,
          QT_TR_NOOP("Raw pan/tilt angles, set directly on the XY pad — the "
                     "same numbers apply to every target fixture regardless "
                     "of where it actually sits on stage. Use Aim instead if "
                     "you want fixtures in different positions to converge "
                     "on the same spot.") },
        { QT_TR_NOOP("New Aim"),        QLCPalette::Aim,
          QT_TR_NOOP("A point in stage space (or a Stage Target) that each "
                     "target fixture's pan/tilt is computed to point at, "
                     "using its own rigged position — needs fixtures placed "
                     "in Lighting Studio first. Use Pan/Tilt instead for "
                     "direct, position-independent angle values.") },
        { QT_TR_NOOP("New Beam"),       QLCPalette::Beam,
          QT_TR_NOOP("Focus / Frost / Iris.") },
        { QT_TR_NOOP("New Gobo"),       QLCPalette::Gobo,
          QT_TR_NOOP("A gobo wheel selection.") },
        { QT_TR_NOOP("New Shutter"),    QLCPalette::Shutter,
          QT_TR_NOOP("Shutter/strobe open-close state.") },
        { QT_TR_NOOP("New Strobe"),     QLCPalette::Strobe,
          QT_TR_NOOP("Strobe rate.") },
        { QT_TR_NOOP("New Effect"),     QLCPalette::Effect,
          QT_TR_NOOP("A scripted/reactive effect (audio, MIDI, time-based).") },
    };
    QList<QAction*> newActions;
    for (uint i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        QAction *a = menu.addAction(tr(types[i].label));
        a->setData(types[i].type);
        a->setToolTip(tr(types[i].tip));
        a->setStatusTip(tr(types[i].tip));
        newActions << a;
    }

    menu.addSeparator();
    QAction *aNewFolder = menu.addAction(tr("New folder…"));
    // Folders first (default: a song's folder holds its colours, aims and
    // effects together) or grouped by type first. Persisted.
    QAction *aByType = menu.addAction(tr("Group by type"));
    aByType->setCheckable(true);
    aByType->setChecked(m_paletteTree->paletteTypesFirst());

    QTreeWidgetItem *clickedPalItem = m_paletteTree->itemAt(pos);
    QAction *aRenameFolder = NULL;
    QAction *aDeleteFolder = NULL;
    if (m_paletteTree->isUserFolder(clickedPalItem))
    {
        aRenameFolder = menu.addAction(tr("Rename folder"));
        aDeleteFolder = menu.addAction(tr("Delete folder"));
    }

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
    if (chosen == aByType)
    {
        m_paletteTree->setPaletteTypesFirst(aByType->isChecked());
        QSettings().setValue(QStringLiteral("programming/palettetree/groupByType"),
                             aByType->isChecked());
        return;
    }
    if (chosen == aRenameFolder)
    {
        m_paletteTree->editItem(clickedPalItem, 0);
        return;
    }
    if (chosen == aDeleteFolder)
    {
        deleteTreeFolder(m_paletteTree, clickedPalItem);
        return;
    }

    if (chosen == aDelete)
    {
        deletePalettes(targetPalettes);
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
                m_paletteTree->filePalette(m_doc->palette(id), full);
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
        m_doc->paletteOps()->setPath({ pid }, path.isEmpty() ? QString()
                                                             : QStringLiteral("Palettes/") + path);
        m_paletteTree->updateTree();
    }
}

bool ProgrammingManager::deleteTreeFolder(FunctionsTreeWidget *tree, QTreeWidgetItem *item,
                                          bool confirm)
{
    if (tree == NULL || tree->isUserFolder(item) == false)
        return false;

    const int count = tree->folderContentCount(item);
    if (confirm && count > 0)
    {
        // Where its contents go: the parent folder, or the top level.
        QTreeWidgetItem *parent = item->parent();
        const QString where = (parent != NULL && tree->isUserFolder(parent))
                ? tr("the “%1” folder").arg(parent->text(0))
                : tr("the top level");
        const QString msg = tr("Delete the folder “%1”?").arg(item->text(0))
                + QString("\n\n")
                + tr("The %n item(s) in it move up to %1. Nothing is deleted.", "", count)
                      .arg(where);
        if (QMessageBox::question(this, tr("Delete folder"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
    }
    tree->removeFolderKeepContents(item);
    return true;
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
            if (fxi == NULL || !fxi->isPatched())
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
        m_saveBtn->setText(tr("Save Positions"));
        m_saveBtn->setEnabled(true);
    }

    // Re-show whatever palette the look editor already has open, in case
    // its value was just changed from outside the widget itself (a control
    // surface nudging pan/tilt directly on the engine side, rather than the
    // user dragging the XY pad) — the joystick's Aim-target case doesn't
    // touch a palette this widget displays, so this is a no-op for it; for
    // a PanTilt palette it's the only thing that repaints the dot.
    if (m_lookEditor && m_lookEditor->paletteId() != QLCPalette::invalidId())
        m_lookEditor->setPalette(m_lookEditor->paletteId());
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
                if (m_saveBtn) m_saveBtn->setText(tr("Save Positions"));
            }
        });
    }
    else
    {
        // Second press: confirmed — commit joystick position then save workspace.
        m_saveArmed = false;
        m_saveBtn->setText(tr("Save Positions"));
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
    // The tempo from the last few taps, and the beat on this one -- every
    // effect on the clock lands on it (MasterTimer::tap()).
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (!ioMap || !m_bpmBtn->isChecked())
        return;
    const double bpm = ioMap->tapBeat();
    const QSignalBlocker block(m_bpmSpin);
    m_bpmSpin->setValue(qRound(bpm));
}

/* True exactly once, ever: the first time anybody asks. Consuming the flag as
 * it answers is the whole point -- a first-run hint that forgets to record
 * itself is a hint that nags on every launch, and that failure is invisible to
 * whoever wrote it because their own flag is already set. */
bool ProgrammingManager::takeFirstRunGuideFlag()
{
    static const QString key = QStringLiteral("programming/guideSeen");
    QSettings s;
    if (s.value(key, false).toBool())
        return false;
    s.setValue(key, true);
    return true;
}

/* The full explanation, on demand.
 *
 * Shown once on a first run and thereafter only when asked for, so the panel is
 * not permanently carrying text nobody is reading any more. */
void ProgrammingManager::showProgrammingGuide()
{
    showGuide(this);
}

void ProgrammingManager::showGuide(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle(tr("Building a look"));
    box.setIcon(QMessageBox::NoIcon);
    box.setTextFormat(Qt::RichText);
    box.setText(tr(
        "<b>Scenes hold looks. Looks are made of palettes.</b>"
        "<ul>"
        "<li>The <b>outline</b> on the left is every scene and what's in it. "
        "Click a scene, or <b>+ Scene</b> to make one.</li>"
        "<li>Drag <b>palettes</b> from the right onto it to build the look — "
        "a colour, a dimmer level, a position, an effect.</li>"
        "<li>Drag <b>fixture groups</b> on as targets to follow their "
        "membership, or individual <b>fixtures</b> to fix them in place.</li>"
        "<li>No palettes yet? Right-click the <b>Palettes</b> tab on the right "
        "to create your first one.</li>"
        "<li>Long list? <b>Search fixtures &amp; groups</b> above it narrows to "
        "what matches.</li>"
        "</ul>"
        "<p><b>Looks and Targets.</b> Open a scene in the outline: its rows are "
        "its targets and its looks. Every look applies to every target. Click a "
        "row and the page edits it; drag a row to another scene to move it "
        "(hold ⌥ to copy), or within the scene to reorder its looks.</p>"
        "<p><b>Effects.</b> In an effect look, pick from the <b>browser</b>: "
        "search, or a category tab, each effect running on a strip of lights "
        "beside its name. The <b>WLED</b> tab is WLED's own effects: WLED "
        "fixtures run them themselves; other fixtures run the console's version "
        "where there is one.</p>"
        "<p><b>Transitions.</b> <i>In</i> and <i>Out</i> on the scene's page "
        "set how the whole look arrives and leaves — a wipe across the stage, "
        "from the centre, a dissolve, along each bar — instead of the scene's "
        "fade. <i>Edge</i> is how soft the line is.</p>"
        "<p>Reopen this any time from <i>How this works</i>, or press F1 for "
        "the whole guide.</p>"));
    QPushButton *more = box.addButton(tr("Open the Full Guide"), QMessageBox::HelpRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == more)
        HelpBrowser::open(QStringLiteral("programming-tab.md"));
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

    // Replace: remove all existing palettes from this scene before stamping.
    // (One step on the shared history: slotCanvasModified() records it from
    // the scene as it was.)
    const QList<quint32> was = scene->palettes();
    for (quint32 pid : was)
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
                np->setPath(QString()); // unfiled: folders are the user's
            }
            else if (entry.type == "Dimmer")
            {
                np = new QLCPalette(QLCPalette::Dimmer);
                np->setValue(entry.dimmerValue);
                np->setPath(QString()); // unfiled: folders are the user's
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
                np->setPath(QString()); // unfiled: folders are the user's
            }
            else if (entry.type == "PanTilt")
            {
                np = new QLCPalette(QLCPalette::PanTilt);
                np->setValue(entry.pan, entry.tilt);
                np->setPath(QString()); // unfiled: folders are the user's
            }
            else if (entry.type == "Beam")
            {
                np = new QLCPalette(QLCPalette::Beam);
                // setValues(), not three setValue() calls: setValue() CLEARS the
                // list before appending, so the first two would be discarded and
                // the palette would end up as [iris] — which valuesFromFixtures()
                // reads as focus, driving the wrong channel.
                np->setValues(QVariantList() << entry.focus << entry.frost << entry.iris);
                np->setPath(QString()); // unfiled: folders are the user's
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
    slotCanvasModified();              // one step; restart preview so DMX updates
    m_outline->scheduleRebuild();
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

void ProgrammingManager::recaptureBaseline()
{
    m_editBaseline = m_currentScene != Function::invalidId() && m_doc->paletteOps() != nullptr
                     ? m_doc->paletteOps()->sceneSnapshot(m_currentScene)
                     : QSharedPointer<Scene>();
}

void ProgrammingManager::recordSceneEdit(const QString &label, const QString &mergeKey)
{
    PaletteOps *ops = m_doc->paletteOps();
    if (ops == nullptr || m_editBaseline.isNull() || m_editBaseline->id() != m_currentScene)
        return;
    // A step (or nothing changed): either way the scene as it is now is the
    // baseline for the next edit (PaletteOps::changed recaptures it too).
    if (!ops->recordSceneStep(label, { m_editBaseline }, mergeKey))
        recaptureBaseline();
}

QString ProgrammingManager::undoText() const
{
    ShowTreeController *st = m_doc->showTree();
    return st != nullptr && st->canUndoShared() ? st->sharedUndoText() : QString();
}

QString ProgrammingManager::redoText() const
{
    ShowTreeController *st = m_doc->showTree();
    return st != nullptr && st->canRedoShared() ? st->sharedRedoText() : QString();
}

/* One history (open 24, P1): the Show Tree's steps, the palette operations
   and the scene edits made here are on it, so ⌘Z / ⇧⌘Z take back and put
   back whatever was done last, in either tab. */
void ProgrammingManager::slotUndo()
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || !st->canUndoShared() || !st->undoShared())
        return;
    m_paletteTree->updateTree();
    if (m_canvas != NULL)
        m_canvas->reload();
    m_outline->rebuild();
    refreshPreview();
    updateScenePage();
}

void ProgrammingManager::slotRedo()
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || !st->canRedoShared() || !st->redoShared())
        return;
    m_paletteTree->updateTree();
    if (m_canvas != NULL)
        m_canvas->reload();
    m_outline->rebuild();
    refreshPreview();
    updateScenePage();
}
