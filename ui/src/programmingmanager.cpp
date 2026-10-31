/*
  Q Light Controller Plus
  programmingmanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyle>
#include <QApplication>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
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
#include <QPointer>
#include <algorithm>
#include <functional>

#include <QDebug>
#include "mainbar.h"
#include "speedmasterspanel.h"
#include "programmingmanager.h"
#include "effectrunsmenu.h"
#include "effectorigin.h"
#include "controllearndialog.h"
#include "workbenchstack.h"
#include "paletteops.h"
#include "usedby.h"
#include "virtualconsole.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcframe.h"
#include "showcontrol.h"
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
#include "programmingoutline.h"
#include "functionselection.h"
#include "functionwizard.h"
#include "audioplugincache.h"
#include "sequence.h"
#include "script.h"
#include "audio.h"
#include "video.h"
#include "app.h"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include "workbench.h"
#include "effectdesigner.h"
#include "workbenchstack.h"
#include "splitterhandles.h"
#include <QButtonGroup>
#include <QHeaderView>
#include <QCollator>
#include <QMimeData>
#include <QDataStream>
#include <QTreeWidgetItemIterator>
#include "lookstackcheck.h"
#include "speedmasters.h"


namespace
{
/** Build ▸ Effects: effect palettes (drag them onto a layer, double-click to
 *  edit) and the classic effects, EFX and RGB Matrix functions (click to
 *  load in the Workbench, drag onto a collection as a layer). */
class EffectsList final : public QTreeWidget
{
public:
    enum { KindRole = Qt::UserRole + 40, IdRole };
    enum { PaletteItem = 1, FunctionItem };

    EffectsList(QWidget *parent) : QTreeWidget(parent)
    {
        setObjectName(QStringLiteral("buildEffects"));
        setHeaderHidden(true);
        setColumnCount(2);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setUniformRowHeights(true);
        header()->setStretchLastSection(false);
        header()->setSectionResizeMode(0, QHeaderView::Stretch);
        header()->setSectionResizeMode(1, QHeaderView::Interactive);
    }

    /** The folder / kind column: as wide as it needs, but never more than a
     *  third of the list -- the names come first. */
    void fitColumns()
    {
        resizeColumnToContents(1);
        const int cap = qMax(60, viewport()->width() / 3);
        if (columnWidth(1) > cap)
            setColumnWidth(1, cap);
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QTreeWidget::resizeEvent(e);
        fitColumns();
    }

    QStringList mimeTypes() const override
    {
        return { QString::fromLatin1(FunctionsTreeWidget::paletteDragMimeType()),
                 QString::fromLatin1(ProgrammingOutline::rowMimeType()) };
    }
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        QByteArray pals, rows;
        QDataStream ps(&pals, QIODevice::WriteOnly), rs(&rows, QIODevice::WriteOnly);
        for (QTreeWidgetItem *it : items)
        {
            const quint32 id = it->data(0, IdRole).toUInt();
            if (it->data(0, KindRole).toInt() == PaletteItem)
                ps << id;
            else if (it->data(0, KindRole).toInt() == FunctionItem)
                rs << qint32(ProgrammingOutline::FunctionRow) << id << id;
        }
        if (pals.isEmpty() && rows.isEmpty())
            return nullptr;
        QMimeData *mime = new QMimeData;
        if (!pals.isEmpty())
            mime->setData(QString::fromLatin1(FunctionsTreeWidget::paletteDragMimeType()), pals);
        if (!rows.isEmpty())
            mime->setData(QString::fromLatin1(ProgrammingOutline::rowMimeType()), rows);
        return mime;
    }
};
}

static ProgrammingManager *s_buildTab = nullptr;

ProgrammingManager *ProgrammingManager::instance()
{
    return s_buildTab;
}

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_clipboardFunction(Function::invalidId())
    , m_clipboardPalette(QLCPalette::invalidId())
{
    s_buildTab = this;
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(3);
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
        /* The drop-down says when Auto is on ("when we set move in black to
           auto .. should change the item in the menubar to indicate same"):
           lit as the Show Tree's MIB button is, "Move in black · Auto", and
           "MIB · n" while it pre-positions n heads. */
        markMenu->setStyleSheet(QStringLiteral(
            "QToolButton#markMenu[mibAuto=\"true\"] { background: palette(highlight);"
            "  color: palette(highlighted-text); border-radius: 4px;"
            "  padding: 2px 14px 2px 5px; }"));   // room for the arrow
        auto markText = [markMenu, this]() {
            ProgrammerController *pc = m_doc->programmer();
            const bool on = pc != nullptr && pc->isAutoMoveInBlack();
            const int presets = pc != nullptr ? pc->moveInBlackPresetCount() : 0;
            // MIDDLE DOT, as the Show Tree's button.
            markMenu->setText(!on ? tr("Move in black")
                              : presets > 0 ? tr("MIB \302\267 %1").arg(presets)
                                            : tr("Move in black \302\267 Auto"));
            markMenu->setToolTip(!on ? tr("Mark, unmark and auto move-in-black (off)")
                                 : presets > 0
                                     ? tr("Auto move-in-black is on: pre-positioning %n head(s) "
                                          "in black for the next cue.", "", presets)
                                     : tr("Auto move-in-black is on: nothing to pre-position "
                                          "right now."));
            if (markMenu->property("mibAuto").toBool() != on)
            {
                markMenu->setProperty("mibAuto", on);
                markMenu->style()->unpolish(markMenu);
                markMenu->style()->polish(markMenu);
            }
        };
        if (ProgrammerController *pc = m_doc->programmer())
        {
            connect(pc, &ProgrammerController::autoMoveInBlackChanged, markMenu,
                    [markText](bool) { markText(); });
            connect(pc, &ProgrammerController::moveInBlackPresetsChanged, markMenu,
                    [markText](int) { markText(); });
        }
        markText();

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

        root->addWidget(bar);
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

        root->addWidget(m_followSpotPanel);
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

    /* Two panes (docs/design/WORKFLOW_DESIGN.md §3.3): the navigator by section, and the
       Workbench -- the same in every section. */
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("programmingSplitter"));
    /* A handle moves the edge and stops at a panel's minimum: never snaps a
       panel shut ("it jumps and ... only shows that"). */
    splitter->setChildrenCollapsible(false);
    m_mainSplit = splitter;
    root->addWidget(splitter, 1);

    m_workbench = new Workbench(m_doc, this);
    splitter->addWidget(buildNavigator());
    splitter->addWidget(m_workbench);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 7);
    SplitterHandles::remember(splitter, QStringLiteral("programming/buildSplitter"));

    /* Once, on a first run -- queued, so it lands after the window is up.
       Never on a headless platform ("offscreen", or "minimal" under xvfb):
       nobody can dismiss a dialog there (it hung a CI test for five
       minutes). QLC_NO_RECOVERY_PROMPT, which scripted runs set, skips it
       too; the flag isn't consumed then, so the first real run still shows
       it. */
    const QString platform = QGuiApplication::platformName();
    if (platform != QLatin1String("offscreen") && platform != QLatin1String("minimal")
        && !qEnvironmentVariableIsSet("QLC_NO_RECOVERY_PROMPT")
        && takeFirstRunGuideFlag())
        QMetaObject::invokeMethod(this, [this]() { showProgrammingGuide(); },
                                  Qt::QueuedConnection);

    // Estimated electrical load of what is lit (Design mode only); the
    // readout is the app status-bar chip, this just drives the timer.
    initPowerTimer();

    // The Workbench: what it edits, the tools follow.
    connect(m_workbench, &Workbench::fixturesSelected, this, &ProgrammingManager::slotSelectionTools);
    connect(m_workbench, &Workbench::currentPaletteIdChanged,
            this, &ProgrammingManager::currentPaletteIdChanged);
    connect(m_workbench, &Workbench::guideRequested, this, &ProgrammingManager::showProgrammingGuide);
    connect(m_workbench, &Workbench::revealPaletteRequested, this, &ProgrammingManager::revealPalette);
    connect(m_workbench, &Workbench::sendToShowTreeRequested, this, &ProgrammingManager::sendToShowTree);
    connect(m_workbench, &Workbench::paletteEdited, this, [this](quint32 id) {
        // A name or a value: the lists that show it. An auto-rename re-sorts
        // the tree; the palette is found again after it settles.
        m_outline->scheduleRebuild();
        QTimer::singleShot(0, this, [this, id]() {
            m_paletteTree->updateTree();
            const QList<QTreeWidgetItem *> sel = m_paletteTree->selectedItems();
            if (!sel.isEmpty() && m_paletteTree->itemPaletteId(sel.first()) == id)
                revealPalette(id);
            rebuildEffects();
        });
    });
    // What it loads, the navigator shows (quietly: no second load).
    connect(m_workbench, &Workbench::loaded, this, [this](quint32 fid) {
        if (m_navigating || fid == Function::invalidId())
            return;
        Function *f = m_doc->function(fid);
        if (f == nullptr)
            return;
        ProgrammingOutline::Row r;
        r.kind = qobject_cast<Scene *>(f) != nullptr ? ProgrammingOutline::SceneRow : ProgrammingOutline::FunctionRow;
        r.scene = r.id = fid;
        m_navigating = true;
        QSignalBlocker block(m_outline);
        m_outline->selectRow(r);
        m_navigating = false;
    });

    // The navigator: its selection loads the Workbench; its requests are the tab's.
    connect(m_outline, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotOutlineSelectionChanged);
    connect(m_outline, &ProgrammingOutline::pageRequested, this, [this]() {
        m_workbench->rows()->setFocus(Qt::OtherFocusReason);
    });
    connect(m_outline, &ProgrammingOutline::scenesEdited, m_workbench, &Workbench::scenesEdited);
    connect(m_outline, &ProgrammingOutline::newSceneRequested, this, [this](const QString &folder) {
        addNewFunction(Function::SceneType, folder);
    });
    connect(m_outline, &ProgrammingOutline::duplicateRequested, this, &ProgrammingManager::duplicateFunctions);
    connect(m_outline, &ProgrammingOutline::deleteScenesRequested, this, &ProgrammingManager::deleteFunctions);
    connect(m_outline, &ProgrammingOutline::editGroupLayoutRequested, m_workbench, &Workbench::showGroupLayout);
    connect(m_outline, &ProgrammingOutline::revealPaletteRequested, this, &ProgrammingManager::revealPalette);
    // Its right-click: the Function Manager's tools too.
    connect(m_outline, &ProgrammingOutline::extendMenu, this, &ProgrammingManager::addFunctionTools);
    connect(m_outline, &ProgrammingOutline::removeFolderRequested, this, &ProgrammingManager::removeFunctionFolder);

    connect(m_doc, &Doc::paletteAdded, m_paletteTree, &FunctionsTreeWidget::addPalette);
    connect(m_doc, &Doc::paletteRemoved, this, [this](quint32) { m_paletteTree->updateTree(); });
    connect(m_doc, &Doc::loaded, m_paletteTree, &FunctionsTreeWidget::updateTree);
    connect(m_doc, &Doc::cleared, m_paletteTree, &FunctionsTreeWidget::updateTree);
    // One connect() per signal: a braced list of &Doc::signal connects
    // nothing on Windows (it holds the DLL import thunks' addresses).
    const auto effects = [this](quint32) { rebuildEffects(); };
    connect(m_doc, &Doc::functionAdded, this, effects);
    connect(m_doc, &Doc::functionRemoved, this, effects);
    connect(m_doc, &Doc::paletteAdded, this, effects);
    connect(m_doc, &Doc::paletteRemoved, this, effects);
    connect(m_doc, &Doc::loaded, this, [this]() { rebuildEffects(); });
    connect(m_doc, &Doc::cleared, this, [this]() { rebuildEffects(); });

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
    // A rename in the palette list shows on the Workbench's rows.
    connect(m_paletteTree, &FunctionsTreeWidget::paletteRenamed,
            this, [this](quint32) { m_workbench->refreshAll(); });
    // Double-click a palette in the list: edit it in the Workbench.
    connect(m_paletteTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotPaletteDoubleClicked(QTreeWidgetItem*)));

    // Cmd/Ctrl-C then Cmd/Ctrl-V duplicates the selected function.
    QShortcut *copySc = new QShortcut(QKeySequence::Copy, this);
    copySc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copySc, SIGNAL(activated()), this, SLOT(slotCopy()));
    QShortcut *pasteSc = new QShortcut(QKeySequence::Paste, this);
    pasteSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteSc, SIGNAL(activated()), this, SLOT(slotPaste()));

    // ⌘Z / ⇧⌘Z: the one shared history (the Show Tree's, the palette
    // operations', every scene and collection edit here), newest first.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &ProgrammingManager::slotUndo);
    QShortcut *redoSc = new QShortcut(QKeySequence::Redo, this);
    redoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoSc, &QShortcut::activated, this, &ProgrammingManager::slotRedo);
    // ⇧⌘N: the next thing where you are -- a scene in the selected folder, a
    // step or a scene layer in the Workbench, an effect (newHere()).
    QShortcut *newSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    newSc->setObjectName(QStringLiteral("buildNewHere"));
    newSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newSc, &QShortcut::activated, this, [this]() { newHere(); });

    // Delete / Backspace (⌫ on a Mac) on the palette list: delete what's
    // selected — asking first, with where it's used. (The outline and the
    // Workbench take their own: a row comes out of its scene.)
    for (const QKeySequence &key : { QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) })
    {
        QShortcut *sc = new QShortcut(key, m_paletteTree);
        // WidgetShortcut: only with the tree itself focused, so ⌫ in an
        // inline rename editor stays a text edit.
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { deleteTreeSelection(m_paletteTree); });
    }

    // The section last left open (Scenes the first time).
    setSection(Section(QSettings().value(QStringLiteral("programming/buildSection"),
                                         int(ScenesSection)).toInt()));
    rebuildEffects();
}

ProgrammingManager::~ProgrammingManager()
{
    if (s_buildTab == this)
        s_buildTab = nullptr;
}

void ProgrammingManager::keyPressEvent(QKeyEvent *ev)
{
    /* Esc that nothing took (an inline rename takes its own): the Workbench
       backs out one level -- from the navigator too, where the focus stays
       after a palette is double-clicked open. Lent to the Show Tree, it is
       the tree's. */
    if (ev->key() == Qt::Key_Escape && ev->modifiers() == Qt::NoModifier
        && !isWorkbenchLent() && m_workbench->backOut())
    {
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}

/****************************************************************************
 * The navigator
 ****************************************************************************/

QWidget *ProgrammingManager::buildNavigator()
{
    QWidget *nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("buildNavigator"));
    QVBoxLayout *col = new QVBoxLayout(nav);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(3);

    // Palettes | Effects | Scenes: only the navigator changes with them.
    QHBoxLayout *seg = new QHBoxLayout;
    seg->setSpacing(0);
    m_sections = new QButtonGroup(this);
    m_sections->setExclusive(true);
    const QStringList names = { tr("Palettes"), tr("Effects"), tr("Scenes") };
    const QStringList tips = {
        tr("Palettes: colours, levels, positions, beams, gobos... in their folders. "
           "Drag one onto a layer; double-click to edit it."),
        tr("Effects: effect palettes, and the classic EFX and RGB Matrix functions."),
        tr("Scenes: your scene folders, with their scenes, collections and chasers. "
           "Click one to load it in the Workbench.") };
    for (int i = 0; i < names.size(); ++i)
    {
        QToolButton *b = new QToolButton(nav);
        b->setObjectName(QStringLiteral("buildSection%1").arg(i));
        b->setText(names.at(i));
        b->setToolTip(tips.at(i));
        b->setCheckable(true);
        b->setAutoRaise(false);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_sections->addButton(b, i);
        seg->addWidget(b);
    }
    col->addLayout(seg);

    /* One search for all of Build: what it finds in Palettes, Effects and
       Scenes together, under a header per section; a click goes there. */
    m_buildSearch = new QLineEdit(nav);
    m_buildSearch->setObjectName(QStringLiteral("buildSearch"));
    m_buildSearch->setPlaceholderText(tr("Search palettes, effects and scenes…"));
    m_buildSearch->setClearButtonEnabled(true);
    col->addWidget(m_buildSearch);
    connect(m_buildSearch, &QLineEdit::textChanged, this, &ProgrammingManager::search);

    m_navSplit = new QSplitter(Qt::Vertical, nav);
    m_navSplit->setObjectName(QStringLiteral("buildNavSplitter"));
    m_navSplit->setChildrenCollapsible(false);
    m_sectionStack = new QStackedWidget(m_navSplit);
    m_sectionStack->setObjectName(QStringLiteral("buildSections"));
    m_sectionStack->addWidget(buildPalettesSection());
    m_sectionStack->addWidget(buildEffectsSection());
    m_sectionStack->addWidget(buildScenesSection());
    m_searchResults = new QTreeWidget(m_sectionStack);
    m_searchResults->setObjectName(QStringLiteral("buildSearchResults"));
    m_searchResults->setHeaderHidden(true);
    m_searchResults->setColumnCount(2);
    m_searchResults->setRootIsDecorated(false);
    m_searchResults->header()->setStretchLastSection(false);
    m_searchResults->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_searchResults->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sectionStack->addWidget(m_searchResults);
    connect(m_searchResults, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *it, int) { goToSearchResult(it); });
    connect(m_searchResults, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *it, int) { goToSearchResult(it); });
    m_navSplit->addWidget(m_sectionStack);
    // The section picked by hand is the one the tab opens on next time.
    connect(m_sections, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_buildSearch != nullptr && !m_buildSearch->text().isEmpty())
        {
            QSignalBlocker quiet(m_buildSearch);
            m_buildSearch->clear();
        }
        setSection(Section(id));
        QSettings().setValue(QStringLiteral("programming/buildSection"), id);
    });

    // Groups and fixtures, under every section: dragged onto a layer.
    QWidget *src = new QWidget(m_navSplit);
    QVBoxLayout *srcCol = new QVBoxLayout(src);
    srcCol->setContentsMargins(0, 0, 0, 0);
    srcCol->setSpacing(2);
    srcCol->addWidget(new QLabel(tr("Groups & fixtures"), src));
    // Find a group or fixture by name in a long list (as Layers does).
    QLineEdit *fixGroupFilter = new QLineEdit(src);
    fixGroupFilter->setObjectName(QStringLiteral("fixGroupFilter"));
    fixGroupFilter->setPlaceholderText(tr("Search fixtures & groups…"));
    fixGroupFilter->setClearButtonEnabled(true);
    srcCol->addWidget(fixGroupFilter);
    m_fixGroupSource = new FixtureGroupSource(m_doc, src);
    m_fixGroupSource->setObjectName("programmingFixturesGroups");
    srcCol->addWidget(m_fixGroupSource, 1);
    connect(fixGroupFilter, &QLineEdit::textChanged,
            m_fixGroupSource, &FixtureGroupSource::setFilterText);
    connect(m_fixGroupSource, &QTreeWidget::itemSelectionChanged,
            this, &ProgrammingManager::slotFixGroupSourceSelectionChanged);
    // Double-click a group: its head layout in the editor pane.
    connect(m_fixGroupSource, &FixtureGroupSource::groupDoubleClicked,
            m_workbench, &Workbench::showGroupLayout);
    m_navSplit->addWidget(src);
    m_navSplit->setStretchFactor(0, 3);
    m_navSplit->setStretchFactor(1, 2);
    SplitterHandles::remember(m_navSplit, QStringLiteral("programming/navSplitter"));
    col->addWidget(m_navSplit, 1);
    return nav;
}

QWidget *ProgrammingManager::buildScenesSection()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("buildScenes"));
    QVBoxLayout *navCol = new QVBoxLayout(page);
    navCol->setContentsMargins(0, 0, 0, 0);
    navCol->setSpacing(3);

    m_outline = new ProgrammingOutline(m_doc, page);

    QHBoxLayout *navToolbar = new QHBoxLayout;
    navToolbar->setContentsMargins(0, 0, 0, 0);
    QLineEdit *funcFilter = new QLineEdit(page);
    funcFilter->setObjectName(QStringLiteral("outlineFilter"));
    funcFilter->setPlaceholderText(tr("Filter…"));
    funcFilter->setClearButtonEnabled(true);
    navToolbar->addWidget(funcFilter, 1);

    // Expand all / Collapse all (also ⌘→ / ⌘←, and ⌥-click a triangle for
    // everything under it).
    QToolButton *expandBtn = new QToolButton(page);
    expandBtn->setObjectName(QStringLiteral("outlineExpandAll"));
    expandBtn->setText(QStringLiteral("⊞"));
    expandBtn->setToolTip(tr("Expand all (⌘→). ⌥-click a triangle to open everything under it."));
    connect(expandBtn, &QToolButton::clicked, m_outline, &ProgrammingOutline::expandEverything);
    navToolbar->addWidget(expandBtn);
    QToolButton *collapseBtn = new QToolButton(page);
    collapseBtn->setObjectName(QStringLiteral("outlineCollapseAll"));
    collapseBtn->setText(QStringLiteral("⊟"));
    collapseBtn->setToolTip(tr("Collapse all (⌘←)"));
    connect(collapseBtn, &QToolButton::clicked, m_outline, &ProgrammingOutline::collapseEverything);
    navToolbar->addWidget(collapseBtn);

    /* + New: a scene in the selected row's folder (the button); the menu
       has a collection, a chaser, and a scene in a new folder. Here, not on
       the app's menu bar, so it travels with the tab when detached. */
    m_addFuncBtn = new QToolButton(page);
    m_addFuncBtn->setObjectName("programmingAddFunction");
    m_addFuncBtn->setText(tr("Scene"));
    m_addFuncBtn->setIcon(QIcon(":/edit_add.png"));
    m_addFuncBtn->setToolTip(tr("A new scene, in the folder of the row selected (the menu: a collection, "
                                "a chaser, a new folder)"));
    m_addFuncBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *addFuncMenu = new QMenu(m_addFuncBtn);
    addFuncMenu->setToolTipsVisible(true);
    QAction *addSceneAction  = addFuncMenu->addAction(QIcon(":/scene.png"), tr("New scene"));
    QAction *addCollAction   = addFuncMenu->addAction(QIcon(":/collection.png"), tr("New collection"));
    addCollAction->setToolTip(tr("Layers that play together: scenes, EFX, RGB Matrices. It loads in the Workbench."));
    QAction *addChaserAction = addFuncMenu->addAction(QIcon(":/chaser.png"), tr("New chaser"));
    addChaserAction->setToolTip(tr("Steps that play one after another (its steps are edited in its step table)"));
    addFuncMenu->addSeparator();
    QAction *addFolderAction = addFuncMenu->addAction(QIcon(":/folder.png"), tr("New scene in a new folder…"));
    // W2: habit 1, "chase the scenes" -- the scenes selected, in the order picked.
    QAction *chaseAction = addFuncMenu->addAction(QIcon(":/chaser.png"), tr("Chaser from selection"));
    chaseAction->setObjectName(QStringLiteral("chaserFromSelection"));
    chaseAction->setToolTip(tr("A new chaser with the scenes selected on the left as its steps, in the order "
                               "you selected them"));
    connect(addFuncMenu, &QMenu::aboutToShow, this, [this, chaseAction]() {
        chaseAction->setEnabled(!m_outline->selectedFunctionsInOrder().isEmpty());
    });
    auto here = [this]() { return m_outline->folderFor(m_outline->currentItem()); };
    connect(chaseAction, &QAction::triggered, this, [this, here]() {
        makeChaserFrom(m_outline->selectedFunctionsInOrder(), here());
    });
    connect(m_addFuncBtn, &QToolButton::clicked, this, [this, here]() {
        addNewFunction(Function::SceneType, here());
    });
    connect(addSceneAction, &QAction::triggered, this, [this, here]() {
        addNewFunction(Function::SceneType, here());
    });
    connect(addCollAction, &QAction::triggered, this, [this, here]() {
        addNewFunction(Function::CollectionType, here());
    });
    connect(addChaserAction, &QAction::triggered, this, [this, here]() {
        addNewFunction(Function::ChaserType, here());
    });
    connect(addFolderAction, &QAction::triggered, this, [this, here]() {
        const QString in = here();
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New folder"),
            in.isEmpty() ? tr("Folder name:") : tr("Folder name (in %1):").arg(in),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        addNewFunction(Function::SceneType, in.isEmpty() ? name : in + QLatin1Char('/') + name);
    });
    /* The Functions window's tools live here now (board Q32): a new
       function of any type, the wizard, the startup function. Its ⌘1…⌘9
       come with them, on this page. */
    addFuncMenu->addSeparator();
    {
        struct NewType { Function::Type type; const char *icon; QString text; QString tip; };
        const QList<NewType> types = {
            { Function::SceneType,      ":/scene.png",      tr("Scene"),       QString() },
            { Function::ChaserType,     ":/chaser.png",     tr("Chaser"),      QString() },
            { Function::SequenceType,   ":/sequence.png",   tr("Sequence"),
              tr("A chaser whose steps are all one scene's channels, each step its own values") },
            { Function::EFXType,        ":/efx.png",        tr("EFX"),         QString() },
            { Function::CollectionType, ":/collection.png", tr("Collection"),  QString() },
            { Function::RGBMatrixType,  ":/rgbmatrix.png",  tr("RGB Matrix"),  QString() },
            { Function::ScriptType,     ":/script.png",     tr("Script"),      QString() },
            { Function::AudioType,      ":/audio.png",      tr("Audio…"),      tr("An audio function for each file picked") },
            { Function::VideoType,      ":/video.png",      tr("Video…"),      tr("A video function for each file picked") },
            { Function::ShowType,       ":/show.png",       tr("Show"),
              tr("A timeline of tracks (it opens in the Workbench; Shows has it too)") } };
        for (int i = 0; i < types.size(); ++i)
        {
            const NewType &t = types.at(i);
            QAction *a = new QAction(QIcon(t.icon), t.text, page);
            a->setObjectName(QStringLiteral("buildNew%1").arg(Function::typeToString(t.type).remove(QLatin1Char(' '))));
            a->setData(int(t.type));
            if (!t.tip.isEmpty())
                a->setToolTip(t.tip);
            if (i < 9)      // ⌘1…⌘9, the Functions window's order
            {
                a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)));
                a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
                page->addAction(a);
            }
            const Function::Type type = t.type;
            connect(a, &QAction::triggered, this, [this, here, type]() { addNewFunction(type, here()); });
            m_newFunctionActions << a;
        }
        m_wizardAction = new QAction(QIcon(":/wizard.png"), tr("Function Wizard…"), page);
        m_wizardAction->setObjectName(QStringLiteral("buildFunctionWizard"));
        m_wizardAction->setToolTip(tr("Make chasers, scenes and effects for the fixtures you pick, in one go"));
        connect(m_wizardAction, &QAction::triggered, this, &ProgrammingManager::runFunctionWizard);
        m_startupAction = new QAction(QIcon(":/autostart.png"), tr("Startup function…"), page);
        m_startupAction->setObjectName(QStringLiteral("buildStartupFunction"));
        m_startupAction->setToolTip(tr("The function that starts by itself when the show goes to Operate mode"));
        connect(m_startupAction, &QAction::triggered, this, &ProgrammingManager::chooseStartupFunction);
    }
    addFunctionTools(addFuncMenu);
    m_addFuncBtn->setMenu(addFuncMenu);
    navToolbar->addWidget(m_addFuncBtn);
    // Match the main window's icon/text display preference.
    applyToolbarLabelMode();

    /* What the outline is, in one line, always there; during a drag the
       same line says what the drop will do, before you let go. */
    const QString outlineWhat = tr("Your scene folders. Click a scene, collection or chaser to load it "
                                   "in the Workbench; drag rows between scenes.");
    m_outlineHint = new QLabel(outlineWhat, page);
    m_outlineHint->setProperty("idleText", outlineWhat);
    m_outlineHint->setObjectName(QStringLiteral("outlineHint"));
    m_outlineHint->setWordWrap(true);
    m_outlineHint->setStyleSheet(QStringLiteral("QLabel { font-style: italic; }"));
    m_outlineHint->setToolTip(tr("Folders hold scenes, collections and chasers; a scene holds its targets "
                                 "(groups ◆, fixtures) and its looks (palettes; an effect's own colour and "
                                 "dimmer sit under it). Drag a row to another scene to move it, hold ⌥ to "
                                 "copy; drag a scene onto the Workbench to make it a layer of the collection "
                                 "there. F2 renames, ⌫ removes, ⌘Z undoes."));
    navCol->addLayout(navToolbar);
    navCol->addWidget(m_outlineHint);
    navCol->addWidget(m_outline, 1);
    connect(funcFilter, &QLineEdit::textChanged, m_outline, &ProgrammingOutline::setFilterText);
    connect(m_outline, &ProgrammingOutline::chaserRequested, this, &ProgrammingManager::makeChaserFrom);
    connect(m_outline, &ProgrammingOutline::sendToShowTreeRequested, this, &ProgrammingManager::sendToShowTree);
    // The idle line says what the outline is; a drag hint replaces it.
    const QString idleText = tr("Your scene folders. Click a scene, collection or chaser to load it "
                                "in the Workbench; drag rows between scenes.");
    m_outlineHint->setProperty("idleText", idleText);
    m_outlineHint->setText(idleText);
    connect(m_outline, &ProgrammingOutline::hintChanged, this, [this](const QString &text) {
        m_outlineHint->setText(text.isEmpty() ? m_outlineHint->property("idleText").toString() : text);
        m_outlineHint->setStyleSheet(text.isEmpty() ? QStringLiteral("QLabel { font-style: italic; }")
                                                    : QStringLiteral("QLabel { font-weight: bold; }"));
    });
    return page;
}

QWidget *ProgrammingManager::buildPalettesSection()
{
    QTabWidget *tabs = new QTabWidget(this);
    tabs->setObjectName("programmingSources");
    tabs->setDocumentMode(true);

    QWidget *palTab = new QWidget(tabs);
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
    // Multi-select: several palettes dragged onto a layer together, or saved
    // as a Bundle in one go.
    m_paletteTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Folders first (any palette type inside) unless grouped by type.
    m_paletteTree->setPaletteTypesFirst(
        QSettings().value(QStringLiteral("programming/palettetree/groupByType"), false).toBool());
    m_paletteTree->updateTree();
    palCol->addWidget(m_paletteTree, 1);
    QLabel *how = new QLabel(tr("Drag onto a layer. Double-click to edit; right-click to add one."), palTab);
    how->setWordWrap(true);
    palCol->addWidget(how);
    connect(paletteFilter, &QLineEdit::textChanged, m_paletteTree, &FunctionsTreeWidget::filterByText);
    connect(m_paletteTree, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotPaletteTreeMenu(QPoint)));
    tabs->addTab(palTab, tr("Palettes"));

    m_bundleCache = new BundleCache(this);
    m_bundleCache->rescan();
    m_bundleBrowser = new BundleBrowser(m_doc, m_bundleCache, tabs);
    connect(m_bundleBrowser, &BundleBrowser::stampRequested, this, &ProgrammingManager::slotStampBundle);
    connect(m_bundleBrowser, &BundleBrowser::saveAsBundleRequested, this, &ProgrammingManager::slotSaveAsBundle);
    tabs->addTab(m_bundleBrowser, tr("Bundles"));
    return tabs;
}

QWidget *ProgrammingManager::buildEffectsSection()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("buildEffectsPage"));
    QVBoxLayout *col = new QVBoxLayout(page);
    col->setContentsMargins(2, 2, 2, 2);
    QHBoxLayout *top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    QLineEdit *filter = new QLineEdit(page);
    filter->setObjectName(QStringLiteral("buildEffectsFilter"));
    filter->setPlaceholderText(tr("Filter…"));
    filter->setClearButtonEnabled(true);
    top->addWidget(filter, 1);

    /* + New: an effect palette in the designer (the button); the menu has
       the classic effects too, as the Scenes section's + New has. */
    QToolButton *newBtn = new QToolButton(page);
    newBtn->setObjectName(QStringLiteral("buildEffectsNew"));
    newBtn->setText(tr("New"));
    newBtn->setIcon(QIcon(":/edit_add.png"));
    newBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newBtn->setToolTip(tr("A new effect palette, designed in the Workbench (the menu: a classic EFX or "
                          "RGB Matrix)"));
    newBtn->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *newMenu = new QMenu(newBtn);
    newMenu->setToolTipsVisible(true);
    QAction *a = newMenu->addAction(QIcon(":/script.png"), tr("Effect palette…"), this,
                                    [this]() { designEffect(QLCPalette::invalidId()); });
    a->setObjectName(QStringLiteral("buildEffectsNewPalette"));
    a->setToolTip(tr("A script effect, designed in the Workbench on the Try on groups: plays in scenes, "
                     "looks and the Show Tree"));
    newMenu->addSection(tr("QLC+ effects (EFX, RGB Matrix)"));
    a = newMenu->addAction(QIcon(":/efx.png"), tr("EFX"), this,
                           [this]() { newClassicEffect(Function::EFXType); });
    a->setObjectName(QStringLiteral("buildEffectsNewEfx"));
    a->setToolTip(tr("A QLC+ EFX function on the Try on groups' heads, loaded in the Workbench"));
    a = newMenu->addAction(QIcon(":/rgbmatrix.png"), tr("RGB Matrix"), this,
                           [this]() { newClassicEffect(Function::RGBMatrixType); });
    a->setObjectName(QStringLiteral("buildEffectsNewMatrix"));
    a->setToolTip(tr("A QLC+ RGB Matrix function on the first Try on group, loaded in the Workbench"));
    newBtn->setMenu(newMenu);
    connect(newBtn, &QToolButton::clicked, this, [this]() { designEffect(QLCPalette::invalidId()); });
    top->addWidget(newBtn);
    col->addLayout(top);

    m_effectsList = new EffectsList(page);
    m_effectsList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_effectsList->setEditTriggers(QAbstractItemView::EditKeyPressed);
    col->addWidget(m_effectsList, 1);
    QLabel *how = new QLabel(tr("Click an effect palette to design it in the Workbench; drag it onto a layer. "
                                "Click an EFX or RGB Matrix to load it, or drag it onto a collection. "
                                "Right-click for more."), page);
    how->setWordWrap(true);
    col->addWidget(how);
    connect(filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (QTreeWidgetItemIterator it(m_effectsList); *it; ++it)
            if ((*it)->childCount() == 0)
                (*it)->setHidden(!text.isEmpty() && !(*it)->text(0).contains(text, Qt::CaseInsensitive));
    });
    // A click (or Return) opens it: an effect palette in the designer, a
    // classic effect in the Workbench with its own editor.
    auto activate = [this](QTreeWidgetItem *it) {
        if (it == nullptr || m_effectsRebuilding)
            return;
        const quint32 id = it->data(0, EffectsList::IdRole).toUInt();
        const int kind = it->data(0, EffectsList::KindRole).toInt();
        if (kind == EffectsList::PaletteItem)
        {
            EffectDesigner *ed = effectDesigner();
            if (ed == nullptr || ed->originalId() != id)
                designEffect(id);
        }
        else if (kind == EffectsList::FunctionItem && m_workbench->loadedFunction() != id)
            m_workbench->load(id);
    };
    connect(m_effectsList, &QTreeWidget::itemClicked, this, [activate](QTreeWidgetItem *it, int) { activate(it); });
    connect(m_effectsList, &QTreeWidget::itemActivated, this, [activate](QTreeWidgetItem *it, int) { activate(it); });
    connect(m_effectsList, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = effectsContextMenu(m_effectsList->itemAt(pos), this);
        menu->exec(m_effectsList->viewport()->mapToGlobal(pos));
        delete menu;
    });
    // F2 renames in place.
    connect(m_effectsList, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *it, int column) {
        if (m_effectsRebuilding || column != 0)
            return;
        const quint32 id = it->data(0, EffectsList::IdRole).toUInt();
        const QString name = it->text(0).trimmed();
        if (it->data(0, EffectsList::KindRole).toInt() == EffectsList::PaletteItem)
        {
            const QLCPalette *p = m_doc->palette(id);
            if (p != nullptr && !name.isEmpty() && name != p->name() && m_doc->paletteOps()->rename(id, name))
            {
                m_paletteTree->updateTree();
                m_workbench->refreshAll();
            }
        }
        else if (it->data(0, EffectsList::KindRole).toInt() == EffectsList::FunctionItem)
        {
            Function *f = m_doc->function(id);
            if (f != nullptr && !name.isEmpty() && name != f->name())
            {
                m_doc->paletteOps()->functionStep(tr("rename %1 to %2").arg(f->name(), name), { id },
                                                  [&](QList<quint32> &) {
                    f->setName(name);
                    return true;
                });
                m_outline->scheduleRebuild();
                m_workbench->refreshAll();
            }
        }
        rebuildEffects();
    });
    // Delete / Backspace: what is selected, asking first (as the palette list).
    for (const QKeySequence &key : { QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) })
    {
        QShortcut *sc = new QShortcut(key, m_effectsList);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this]() {
            QList<quint32> pals, fns;
            effectTargets(m_effectsList->currentItem(), pals, fns);
            if (!pals.isEmpty())
                deletePalettes(pals);
            if (!fns.isEmpty())
                deleteFunctions(fns);
        });
    }
    return page;
}

void ProgrammingManager::rebuildEffects()
{
    if (m_effectsList == nullptr || m_effectsRebuildQueued)
        return;
    m_effectsRebuildQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_effectsRebuildQueued = false;
        rebuildEffectsNow();
    });
}

void ProgrammingManager::rebuildEffectsNow()
{
    if (m_effectsList == nullptr)
        return;
    // Back where it was: the current row and the selection, by what they are.
    QTreeWidgetItem *cur = m_effectsList->currentItem();
    const QPair<int, quint32> curKey = cur == nullptr ? qMakePair(0, 0u)
        : qMakePair(cur->data(0, EffectsList::KindRole).toInt(), cur->data(0, EffectsList::IdRole).toUInt());
    QSet<QPair<int, quint32>> selected;
    for (QTreeWidgetItem *it : m_effectsList->selectedItems())
        selected.insert(qMakePair(it->data(0, EffectsList::KindRole).toInt(),
                                  it->data(0, EffectsList::IdRole).toUInt()));
    m_effectsRebuilding = true;
    QSignalBlocker block(m_effectsList);

    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    m_effectsList->clear();
    QTreeWidgetItem *pals = new QTreeWidgetItem(m_effectsList, { tr("Effect palettes") });
    QTreeWidgetItem *classic = new QTreeWidgetItem(m_effectsList, { tr("QLC+ effects (EFX, RGB Matrix)") });
    for (QTreeWidgetItem *h : { pals, classic })
    {
        h->setFlags(Qt::ItemIsEnabled);
        QFont f = h->font(0);
        f.setBold(true);
        h->setFont(0, f);
    }
    QList<QLCPalette *> effects;
    for (QLCPalette *p : m_doc->palettes())
        if (p != nullptr && p->type() == QLCPalette::Effect && m_doc->paletteOps()->isManaged(p->id()))
            effects << p;
    std::sort(effects.begin(), effects.end(), [&coll](QLCPalette *a, QLCPalette *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    const Qt::ItemFlags leaf = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
    for (QLCPalette *p : effects)
    {
        /* What it was made from, grey beside it ("based on Fire"); on hover,
           which of its settings differ from that one. */
        const QString base = EffectOrigin::baseName(m_doc, p);
        const QStringList diffs = base.isEmpty() ? QStringList() : EffectOrigin::differences(m_doc, p);
        const QString folder = PaletteOps::folderOf(p);
        const QString based = base.isEmpty() || (diffs.isEmpty() && p->name().compare(base, Qt::CaseInsensitive) == 0)
                                  ? QString() : EffectOrigin::basedOnLine(base, diffs.size());
        QTreeWidgetItem *it = new QTreeWidgetItem(pals, { p->name(), based.isEmpty() ? folder
                                                          : folder.isEmpty() ? based
                                                          : based + QStringLiteral(" · ") + folder });
        it->setIcon(0, QIcon(p->iconResource()));
        it->setData(0, EffectsList::KindRole, EffectsList::PaletteItem);
        it->setData(0, EffectsList::IdRole, p->id());
        it->setFlags(leaf);
        it->setForeground(1, QBrush(Qt::gray));
        it->setToolTip(0, tr("Effect palette “%1”: click to design it in the Workbench, drag it onto a layer.")
                              .arg(p->name()));
        if (!based.isEmpty())
            it->setToolTip(1, EffectOrigin::differencesTip(base, diffs));
    }
    QList<Function *> fns;
    ShowTreeController *st = m_doc->showTree();
    for (Function *f : m_doc->functions())
        if (f != nullptr && (f->type() == Function::EFXType || f->type() == Function::RGBMatrixType)
            && f->isVisible() && (st == nullptr || !st->isGenerated(f->id())))
            fns << f;
    std::sort(fns.begin(), fns.end(), [&coll](Function *a, Function *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    for (Function *f : fns)
    {
        const QString folder = f->path(true);
        QTreeWidgetItem *it = new QTreeWidgetItem(classic, { f->name(), folder.isEmpty()
            ? Function::typeToString(f->type()) : Function::typeToString(f->type()) + QStringLiteral(" · ") + folder });
        it->setIcon(0, f->getIcon());
        it->setData(0, EffectsList::KindRole, EffectsList::FunctionItem);
        it->setData(0, EffectsList::IdRole, f->id());
        it->setFlags(leaf);
        it->setToolTip(0, tr("A classic %1: click to load it in the Workbench, drag it onto a collection.")
                              .arg(Function::typeToString(f->type())));
    }
    if (effects.isEmpty())
        new QTreeWidgetItem(pals, { tr("None yet: + New ▸ Effect palette") });
    if (fns.isEmpty())
        new QTreeWidgetItem(classic, { tr("None yet: + New ▸ EFX / RGB Matrix") });
    m_effectsList->expandAll();
    static_cast<EffectsList *>(m_effectsList)->fitColumns();

    for (QTreeWidgetItemIterator it(m_effectsList); *it; ++it)
    {
        const QPair<int, quint32> key((*it)->data(0, EffectsList::KindRole).toInt(),
                                      (*it)->data(0, EffectsList::IdRole).toUInt());
        if (key.first == 0)
            continue;
        if (key == curKey)
            m_effectsList->setCurrentItem(*it, 0, QItemSelectionModel::NoUpdate);
        if (selected.contains(key))
            (*it)->setSelected(true);
    }
    m_effectsRebuilding = false;
}

void ProgrammingManager::selectEffectRow(bool palette, quint32 id)
{
    if (m_effectsRebuildQueued)
        rebuildEffectsNow();      // the row may be new
    const int kind = palette ? EffectsList::PaletteItem : EffectsList::FunctionItem;
    for (QTreeWidgetItemIterator it(m_effectsList); *it; ++it)
        if ((*it)->data(0, EffectsList::KindRole).toInt() == kind
            && (*it)->data(0, EffectsList::IdRole).toUInt() == id)
        {
            QSignalBlocker block(m_effectsList);
            m_effectsList->clearSelection();
            m_effectsList->setCurrentItem(*it);
            (*it)->setSelected(true);
            m_effectsList->scrollToItem(*it);
            return;
        }
}

void ProgrammingManager::effectTargets(QTreeWidgetItem *clicked, QList<quint32> &palettes,
                                       QList<quint32> &functions) const
{
    palettes.clear();
    functions.clear();
    if (clicked == nullptr || clicked->data(0, EffectsList::KindRole).toInt() == 0)
        return;
    const QList<QTreeWidgetItem *> rows = clicked->isSelected() ? m_effectsList->selectedItems()
                                                                : QList<QTreeWidgetItem *>({ clicked });
    for (QTreeWidgetItem *it : rows)
    {
        const quint32 id = it->data(0, EffectsList::IdRole).toUInt();
        if (it->data(0, EffectsList::KindRole).toInt() == EffectsList::PaletteItem)
            palettes << id;
        else if (it->data(0, EffectsList::KindRole).toInt() == EffectsList::FunctionItem)
            functions << id;
    }
}

EffectDesigner *ProgrammingManager::effectDesigner() const
{
    return qobject_cast<EffectDesigner *>(m_workbench->effectPane());
}

bool ProgrammingManager::designEffect(quint32 paletteId)
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p != nullptr && p->type() != QLCPalette::Effect)
        return false;
    Workbench::EffectRequest req;
    req.palette = p != nullptr ? paletteId : QLCPalette::invalidId();
    if (m_sections->checkedId() != EffectsSection)
        setSection(EffectsSection);
    if (!m_workbench->designEffect(req))
        return false;
    EffectDesigner *ed = effectDesigner();
    if (ed == nullptr)
        return true;
    // Saved as a palette, or the palette updated: listed, selected, and on
    // the bench again (a fresh copy of it).
    connect(ed, &EffectDesigner::effectChosen, this,
            [this](quint32 pid, const QList<quint32> &, EffectDesigner::Outcome outcome) {
        m_paletteTree->updateTree();
        rebuildEffects();
        QTimer::singleShot(0, this, [this, pid, outcome]() {
            if (m_doc->palette(pid) == nullptr)
                return;
            selectEffectRow(true, pid);
            if (effectDesigner() != nullptr)
                designEffect(pid);
            // Saved as a palette: named in place, the name typed over the
            // one it was given (no name box first).
            if (outcome == EffectDesigner::NewPalette && m_effectsList->currentItem() != nullptr)
                m_effectsList->editItem(m_effectsList->currentItem(), 0);
        });
    });
    if (p != nullptr)
        selectEffectRow(true, paletteId);
    else
        m_effectsList->clearSelection();
    return true;
}

quint32 ProgrammingManager::newClassicEffect(Function::Type type)
{
    Function *f = nullptr;
    if (type == Function::EFXType)
        f = new EFX(m_doc);
    else if (type == Function::RGBMatrixType)
        f = new RGBMatrix(m_doc);
    else
        return Function::invalidId();
    if (!m_doc->addFunction(f))
    {
        delete f;
        return Function::invalidId();
    }
    f->setName(type == Function::EFXType ? tr("New EFX %1").arg(f->id()) : tr("New RGB Matrix %1").arg(f->id()));
    // Its heads: the Try on groups', as the designer's bench plays.
    const QList<quint32> tryOn = EffectDesigner::savedTryOnGroups(m_doc);
    if (EFX *efx = qobject_cast<EFX *>(f))
    {
        for (quint32 gid : tryOn)
            if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
                for (const GroupHead &head : g->headList())
                    efx->addFixture(head.fxi, head.head);
    }
    else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
    {
        if (!tryOn.isEmpty())
            m->setFixtureGroup(tryOn.first());
    }
    if (m_sections->checkedId() != EffectsSection)
        setSection(EffectsSection);
    m_outline->scheduleRebuild();
    selectEffectRow(false, f->id());
    m_workbench->load(f->id());
    return f->id();
}

QMenu *ProgrammingManager::effectsContextMenu(QTreeWidgetItem *clicked, QWidget *parent)
{
    QMenu *menu = new QMenu(parent);
    menu->setObjectName(QStringLiteral("buildEffectsMenu"));
    menu->setToolTipsVisible(true);
    QList<quint32> pals, fns;
    effectTargets(clicked, pals, fns);
    const int kind = clicked == nullptr ? 0 : clicked->data(0, EffectsList::KindRole).toInt();
    const quint32 id = clicked == nullptr ? 0 : clicked->data(0, EffectsList::IdRole).toUInt();
    const QPersistentModelIndex index = clicked == nullptr ? QPersistentModelIndex()
                                                           : QPersistentModelIndex(m_effectsList->indexFromItem(clicked));

    // On a row: what it is first.
    if (kind == EffectsList::PaletteItem)
    {
        QAction *a = menu->addAction(tr("Design / edit…"), this, [this, id]() { designEffect(id); });
        a->setObjectName(QStringLiteral("effectsDesign"));
        a->setToolTip(tr("In the effect designer, in the Workbench: a copy on the bench until you update "
                         "the palette or save a new one"));
        menu->setDefaultAction(a);
    }
    else if (kind == EffectsList::FunctionItem)
    {
        QAction *a = menu->addAction(tr("Open in editor"), this, [this, id]() {
            selectEffectRow(false, id);
            m_workbench->load(id);
        });
        a->setObjectName(QStringLiteral("effectsOpen"));
        a->setToolTip(tr("Load it in the Workbench, with its own (QLC+) editor"));
        menu->setDefaultAction(a);
    }
    // An effect function (EFX, RGB Matrix): fired from a MIDI or OSC control.
    if (kind == EffectsList::FunctionItem && fns.size() <= 1)
        ControlLearnDialog::addTo(menu, m_doc, ControlMap::function(id), this);
    // How it runs: Loop | Once | N times (a classic effect: its Run order).
    if (kind == EffectsList::PaletteItem)
        EffectRunsMenu::add(menu, m_doc, pals.isEmpty() ? QList<quint32>{ id } : pals, this);
    else if (kind == EffectsList::FunctionItem)
        EffectRunsMenu::addForFunction(menu, m_doc, m_doc->function(id));
    if (kind != 0)
    {
        QAction *a = menu->addAction(tr("Duplicate"), this, [this, kind, id]() {
            if (kind == EffectsList::PaletteItem)
            {
                const quint32 copy = m_doc->paletteOps()->duplicate(id);
                if (m_doc->palette(copy) == nullptr)
                    return;
                m_paletteTree->updateTree();
                designEffect(copy);
            }
            else
            {
                // One step on the shared history, as the outline's Duplicate.
                quint32 copyId = Function::invalidId();
                m_doc->paletteOps()->functionStep(tr("Duplicate"), QList<quint32>(),
                                                  [&](QList<quint32> &made) {
                    Function *src = m_doc->function(id);
                    Function *copy = src == nullptr ? nullptr : src->createCopy(m_doc);
                    if (copy == nullptr)
                        return false;
                    copy->setName(m_doc->nextDuplicateName(src));
                    made << copy->id();
                    copyId = copy->id();
                    return true;
                });
                if (m_doc->function(copyId) == nullptr)
                    return;
                m_outline->scheduleRebuild();
                selectEffectRow(false, copyId);
                m_workbench->load(copyId);
            }
        });
        a->setObjectName(QStringLiteral("effectsDuplicate"));
        a = menu->addAction(tr("Rename"), this, [this, index]() {
            if (index.isValid())
                m_effectsList->editItem(m_effectsList->itemFromIndex(index), 0);
        });
        a->setObjectName(QStringLiteral("effectsRename"));
        a->setShortcut(QKeySequence(Qt::Key_F2));
        a = menu->addAction(tr("Move to folder…"), this, [this, pals, fns]() {
            QString current;
            if (!pals.isEmpty())
                current = PaletteOps::folderOf(m_doc->palette(pals.first()));
            else if (Function *f = m_doc->function(fns.first()))
                current = f->path(true);
            bool ok = false;
            const QString folder = QInputDialog::getText(this, tr("Move to folder"),
                tr("Folder (e.g. \"01-Song\"; empty for none):"), QLineEdit::Normal, current, &ok).trimmed();
            if (!ok)
                return;
            if (!pals.isEmpty())
            {
                m_doc->paletteOps()->move(pals, folder);
                m_paletteTree->updateTree();
            }
            if (!fns.isEmpty())
            {
                m_doc->paletteOps()->functionStep(tr("move to folder %1").arg(folder), fns, [&](QList<quint32> &) {
                    for (quint32 fid : fns)
                        if (Function *f = m_doc->function(fid))
                            f->setPath(folder);
                    return true;
                });
                m_outline->scheduleRebuild();
            }
            rebuildEffects();
        });
        a->setObjectName(QStringLiteral("effectsMove"));
        menu->addSeparator();
        const int n = pals.size() + fns.size();
        a = menu->addAction(n > 1 ? tr("Delete %1 effects").arg(n) : tr("Delete"), this, [this, pals, fns]() {
            if (!pals.isEmpty())
                deletePalettes(pals);
            if (!fns.isEmpty())
                deleteFunctions(fns);
        });
        a->setObjectName(QStringLiteral("effectsDelete"));
        menu->addSeparator();
    }

    // Everywhere (empty space too): a new one.
    QAction *a = menu->addAction(QIcon(":/script.png"), tr("New effect…"), this,
                                 [this]() { designEffect(QLCPalette::invalidId()); });
    a->setObjectName(QStringLiteral("effectsNewPalette"));
    a->setToolTip(tr("A new effect palette, designed in the Workbench on the Try on groups"));
    a = menu->addAction(QIcon(":/efx.png"), tr("New EFX"), this, [this]() { newClassicEffect(Function::EFXType); });
    a->setObjectName(QStringLiteral("effectsNewEfx"));
    a = menu->addAction(QIcon(":/rgbmatrix.png"), tr("New RGB Matrix"), this,
                        [this]() { newClassicEffect(Function::RGBMatrixType); });
    a->setObjectName(QStringLiteral("effectsNewMatrix"));
    return menu;
}

void ProgrammingManager::setSection(Section section)
{
    if (section < PalettesSection || section > ScenesSection)
        section = ScenesSection;
    if (QAbstractButton *b = m_sections->button(int(section)))
        b->setChecked(true);
    m_sectionStack->setCurrentIndex(int(section));
}

ProgrammingManager::Section ProgrammingManager::section() const
{
    // Searching shows the results over it: the section is the one picked.
    if (m_sectionStack->currentIndex() > int(ScenesSection))
        return Section(qMax(0, m_sections->checkedId()));
    return Section(m_sectionStack->currentIndex());
}

namespace
{
enum { SearchSectionRole = Qt::UserRole + 60, SearchKindRole, SearchIdRole };
enum { SearchPalette = 1, SearchFunction };
}

void ProgrammingManager::search(const QString &text)
{
    if (m_searchResults == nullptr)
        return;
    const QString q = text.trimmed();
    if (q.isEmpty())
    {
        // Back to the section it was on.
        m_searchResults->clear();
        m_sectionStack->setCurrentIndex(qMax(0, m_sections->checkedId()));
        return;
    }
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    struct Hit { QString name, where; int kind; quint32 id; QIcon icon; };
    QList<Hit> hits[3];
    ShowTreeController *st = m_doc->showTree();
    for (QLCPalette *p : m_doc->palettes())
    {
        if (p == nullptr || !m_doc->paletteOps()->isManaged(p->id()) || !p->name().contains(q, Qt::CaseInsensitive))
            continue;
        const int sec = p->type() == QLCPalette::Effect ? EffectsSection : PalettesSection;
        hits[sec] << Hit{ p->name(), PaletteOps::folderOf(p), SearchPalette, p->id(), QIcon(p->iconResource()) };
    }
    for (Function *f : m_doc->functions())
    {
        if (f == nullptr || !f->isVisible() || (st != nullptr && st->isGenerated(f->id()))
            || !f->name().contains(q, Qt::CaseInsensitive))
            continue;
        int sec = -1;
        if (f->type() == Function::EFXType || f->type() == Function::RGBMatrixType)
            sec = EffectsSection;
        else if (f->type() == Function::SceneType || f->type() == Function::CollectionType
                 || f->type() == Function::ChaserType)
            sec = ScenesSection;
        if (sec < 0)
            continue;
        hits[sec] << Hit{ f->name(), f->path(true), SearchFunction, f->id(), f->getIcon() };
    }

    m_searchResults->clear();
    const QStringList names = { tr("Palettes"), tr("Effects"), tr("Scenes") };
    for (int sec = 0; sec < 3; ++sec)
    {
        if (hits[sec].isEmpty())
            continue;
        std::sort(hits[sec].begin(), hits[sec].end(), [&coll](const Hit &a, const Hit &b) {
            return coll.compare(a.name, b.name) < 0;
        });
        QTreeWidgetItem *head = new QTreeWidgetItem(m_searchResults,
            { tr("%1 (%2)").arg(names.at(sec)).arg(hits[sec].size()) });
        head->setFlags(Qt::ItemIsEnabled);
        head->setData(0, SearchSectionRole, sec);
        QFont f = head->font(0);
        f.setBold(true);
        head->setFont(0, f);
        for (const Hit &h : hits[sec])
        {
            QTreeWidgetItem *it = new QTreeWidgetItem(head, { h.name, h.where });
            it->setIcon(0, h.icon);
            it->setData(0, SearchSectionRole, sec);
            it->setData(0, SearchKindRole, h.kind);
            it->setData(0, SearchIdRole, h.id);
            it->setForeground(1, QBrush(Qt::gray));
            it->setToolTip(0, tr("Go to it in %1").arg(names.at(sec)));
        }
    }
    if (m_searchResults->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(m_searchResults, { tr("Nothing called “%1”").arg(q) });
        none->setFlags(Qt::NoItemFlags);
    }
    m_searchResults->expandAll();
    m_sectionStack->setCurrentWidget(m_searchResults);
}

void ProgrammingManager::goToSearchResult(QTreeWidgetItem *item)
{
    if (item == nullptr || !item->data(0, SearchKindRole).isValid())
        return;
    const Section sec = Section(item->data(0, SearchSectionRole).toInt());
    const int kind = item->data(0, SearchKindRole).toInt();
    const quint32 id = item->data(0, SearchIdRole).toUInt();
    {
        QSignalBlocker quiet(m_buildSearch);
        m_buildSearch->clear();
    }
    m_searchResults->clear();
    setSection(sec);
    QSettings().setValue(QStringLiteral("programming/buildSection"), int(sec));
    if (sec == PalettesSection)
    {
        if (QLCPalette *p = m_doc->palette(id))
            if (QTreeWidgetItem *it = m_paletteTree->paletteItem(p))
            {
                m_paletteTree->clearSelection();
                m_paletteTree->setCurrentItem(it);
                m_paletteTree->scrollToItem(it);
            }
    }
    else if (sec == EffectsSection)
    {
        rebuildEffectsNow();
        const int want = kind == SearchPalette ? EffectsList::PaletteItem : EffectsList::FunctionItem;
        for (QTreeWidgetItemIterator it(m_effectsList); *it; ++it)
            if ((*it)->data(0, EffectsList::KindRole).toInt() == want
                && (*it)->data(0, EffectsList::IdRole).toUInt() == id)
            {
                QSignalBlocker quiet(m_effectsList);
                m_effectsList->clearSelection();
                m_effectsList->setCurrentItem(*it);
                m_effectsList->scrollToItem(*it);
                break;
            }
    }
    else
    {
        m_outline->rebuild();
        for (QTreeWidgetItemIterator it(m_outline); *it; ++it)
        {
            const int k = (*it)->data(0, ProgrammingOutline::KindRole).toInt();
            if ((k == ProgrammingOutline::SceneRow || k == ProgrammingOutline::FunctionRow)
                && (*it)->data(0, ProgrammingOutline::IdRole).toUInt() == id)
            {
                m_outline->selectRow(ProgrammingOutline::rowOf(*it));
                m_outline->scrollToItem(m_outline->currentItem());
                break;
            }
        }
    }
}

/* ⇧⌘N: in the Workbench (Build's own, not lent), what is loaded there. */
static bool inWorkbench(Workbench *bench)
{
    QWidget *focus = QApplication::focusWidget();
    return bench != nullptr && focus != nullptr && bench->isAncestorOf(focus);
}

QString ProgrammingManager::newHereLabel() const
{
    if (!isWorkbenchLent() && inWorkbench(m_workbench))
    {
        const QString bench = m_workbench->newHereLabel();
        if (!bench.isEmpty())
            return bench;
    }
    switch (section())
    {
    case EffectsSection:
        return tr("New Effect");
    case ScenesSection:
    {
        const QString folder = m_outline->folderFor(m_outline->currentItem());
        return folder.isEmpty() ? tr("New Scene") : tr("New Scene in %1").arg(folder);
    }
    default:
        return QString();
    }
}

bool ProgrammingManager::newHere()
{
    // Typing a name in the outline: not now.
    QWidget *focus = QApplication::focusWidget();
    if (qobject_cast<QLineEdit *>(focus) != nullptr && m_outline->isAncestorOf(focus))
        return false;
    if (!isWorkbenchLent() && inWorkbench(m_workbench) && !m_workbench->newHereLabel().isEmpty())
        return m_workbench->newHere() != Function::invalidId();
    if (section() == EffectsSection)
        return designEffect(QLCPalette::invalidId());
    if (section() != ScenesSection || m_doc->paletteOps() == nullptr)
        return false;
    // As + Scene makes one, but one undo step: ⌘Z takes it away again.
    const QString folder = m_outline->folderFor(m_outline->currentItem());
    quint32 made = Function::invalidId();
    m_doc->paletteOps()->functionStep(tr("new scene"), {}, [&](QList<quint32> &news) {
        Scene *s = new Scene(m_doc);
        if (!folder.isEmpty())
            s->setPath(folder);
        if (!m_doc->addFunction(s))
        {
            delete s;
            return false;
        }
        s->setName(QStringLiteral("%1 %2").arg(tr("New Scene")).arg(s->id()));
        news << s->id();
        made = s->id();
        return true;
    });
    if (made == Function::invalidId())
        return false;
    m_outline->rebuild();
    showFunction(made);
    // Named in place, as F2 would.
    QTreeWidgetItem *cur = m_outline->currentItem();
    if (cur != nullptr && (cur->flags() & Qt::ItemIsEditable))
    {
        m_outline->setFocus(Qt::ShortcutFocusReason);
        m_outline->editItem(cur, ProgrammingOutline::NameCol);
    }
    return true;
}

void ProgrammingManager::slotOutlineSelectionChanged()
{
    /* The Workbench follows the navigator: a scene, collection or chaser
       loads; a look, a target in a scene opened up here loads its scene with
       that row selected. A folder leaves the Workbench as it is. */
    if (m_navigating)
        return;
    QTreeWidgetItem *cur = m_outline->currentItem();
    if (cur == nullptr || !cur->isSelected())
    {
        const QList<QTreeWidgetItem *> sel = m_outline->selectedItems();
        cur = sel.isEmpty() ? nullptr : sel.first();
    }
    if (cur == nullptr)
        return;
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(cur);
    m_navigating = true;
    switch (row.kind)
    {
    case ProgrammingOutline::SceneRow:
    case ProgrammingOutline::FunctionRow:
        if (m_workbench->loadedFunction() != row.id || m_workbench->stack() == nullptr)
            m_workbench->load(row.id);
        else if (row.kind == ProgrammingOutline::SceneRow)
        {
            ProgrammingOutline::Row r = row;
            m_workbench->selectRow(r);
        }
        break;
    case ProgrammingOutline::PaletteRow:
    case ProgrammingOutline::GroupRow:
    case ProgrammingOutline::FixtureRow:
    {
        // The scene it is in, loaded, unless what is loaded holds it already.
        bool held = false;
        if (QTreeWidgetItem *layer = m_workbench->rows()->sceneItem(row.scene))
            held = layer != nullptr;
        if (!held)
            m_workbench->load(row.scene);
        // Every selected row of that scene, selected there too.
        ProgrammingOutline *rows = m_workbench->rows();
        QSignalBlocker block(rows);
        rows->clearSelection();
        QTreeWidgetItem *first = nullptr;
        for (const ProgrammingOutline::Row &r : m_outline->selectedRows())
            if (r.scene == row.scene)
                if (QTreeWidgetItem *it = rows->itemFor(r))
                {
                    it->setSelected(true);
                    for (QTreeWidgetItem *a = it->parent(); a != nullptr; a = a->parent())
                        a->setExpanded(true);
                    if (first == nullptr || r == row)
                        first = it;
                }
        if (first != nullptr)
        {
            rows->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
            rows->scrollToItem(first);
        }
        block.unblock();
        emit rows->itemSelectionChanged();
        break;
    }
    default:
        break;
    }
    m_navigating = false;
}

void ProgrammingManager::revealScene(quint32 sceneId)
{
    ProgrammingOutline::Row r;
    r.kind = ProgrammingOutline::SceneRow;
    r.scene = r.id = sceneId;
    if (!m_outline->selectRow(r))
        m_workbench->load(sceneId);
}

/****************************************************************************
 * W2: Chaser from selection, Send to Show Tree
 ****************************************************************************/

bool ProgrammingManager::sendToShowTree(quint32 chaserId)
{
    ShowTreeView *tree = ShowTreeView::instance();
    if (tree == nullptr)
        return false;
    const bool sent = tree->sendToShowTree(chaserId, this);
    // Filed under Imported/…, and no longer one to send.
    m_outline->rebuild();
    m_workbench->refreshAll();
    return sent;
}

void ProgrammingManager::makeChaserFrom(const QList<quint32> &functionIds, const QString &folder)
{
    const quint32 made = FunctionStack::makeChaser(m_doc, functionIds, folder);
    if (made == Function::invalidId())
        return;
    m_outline->rebuild();
    showFunction(made);
}

void ProgrammingManager::showFunction(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (f == nullptr)
        return;
    setSection(ScenesSection);
    ProgrammingOutline::Row r;
    r.kind = qobject_cast<Scene *>(f) != nullptr ? ProgrammingOutline::SceneRow : ProgrammingOutline::FunctionRow;
    r.scene = r.id = fid;
    // Folders lists scenes, collections and chasers; anything else (an EFX,
    // a script...) loads in the Workbench with no row selected.
    m_outline->selectRow(r);
    if (m_workbench->loadedFunction() != fid)
        m_workbench->load(fid);
}

bool ProgrammingManager::openFunction(quint32 fid)
{
    ProgrammingManager *pm = instance();
    if (pm == nullptr || pm->m_doc->function(fid) == nullptr)
        return false;
    // Docked in the Show Tree: the Workbench comes home first.
    if (pm->isWorkbenchLent())
        if (ShowTreeView *tree = ShowTreeView::instance())
            tree->closeWorkbench();
    foreach (QWidget *w, QApplication::topLevelWidgets())
        if (App *app = qobject_cast<App *>(w))
            app->showContext(App::tr("Build"));
    pm->showFunction(fid);
    return true;
}

void ProgrammingManager::addFunctionTools(QMenu *menu)
{
    if (menu == nullptr || m_newFunctionActions.isEmpty())
        return;
    QMenu *sub = menu->addMenu(QIcon(":/function.png"), tr("New function"));
    sub->setObjectName(QStringLiteral("buildNewFunctionMenu"));
    sub->setToolTipsVisible(true);
    sub->addActions(m_newFunctionActions);
    menu->addAction(m_wizardAction);
    menu->addAction(m_startupAction);
    const bool design = m_doc->mode() == Doc::Design;
    for (QAction *a : m_newFunctionActions)
        a->setEnabled(design);
    m_wizardAction->setEnabled(design);
}

QList<quint32> ProgrammingManager::addMediaFunctions(Function::Type type, const QStringList &files,
                                                     const QString &folder)
{
    QList<quint32> made;
    QStringList refused;
    for (const QString &fn : files)
    {
        Function *f = nullptr;
        if (type == Function::AudioType)
        {
            Audio *audio = new Audio(m_doc);
            if (audio->setSourceFileName(fn))
                f = audio;
            else
                delete audio;
        }
        else if (type == Function::VideoType)
        {
            Video *video = new Video(m_doc);
            if (video->setSourceUrl(fn))
                f = video;
            else
                delete video;
        }
        if (f == nullptr)
        {
            refused << QFileInfo(fn).fileName();
            continue;
        }
        if (!folder.isEmpty())
            f->setPath(folder);
        if (!m_doc->addFunction(f))
        {
            delete f;
            continue;
        }
        made << f->id();
    }
    if (!refused.isEmpty())
        QMessageBox::warning(this, type == Function::AudioType ? tr("Unsupported audio file")
                                                               : tr("Unsupported video file"),
                             tr("These can't be played with qlcconsole:\n%1").arg(refused.join(QLatin1Char('\n'))));
    if (!made.isEmpty())
    {
        m_outline->rebuild();
        showFunction(made.last());
    }
    return made;
}

void ProgrammingManager::runFunctionWizard()
{
    FunctionWizard fw(this, m_doc);
    if (fw.exec() == QDialog::Accepted)
    {
        m_outline->rebuild();
        rebuildEffects();
    }
}

void ProgrammingManager::chooseStartupFunction()
{
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    fs.showNone(true);
    fs.setSelection({ m_doc->startupFunction() });
    if (fs.exec() == QDialog::Accepted && !fs.selection().isEmpty())
    {
        m_doc->setStartupFunction(fs.selection().first());
        m_doc->setModified();
    }
}

void ProgrammingManager::removeFunctionFolder(const QString &path)
{
    if (path.isEmpty())
        return;
    const int cut = path.lastIndexOf(QLatin1Char('/'));
    const QString up = cut > 0 ? path.left(cut) : QString();
    QList<Function *> all;
    QList<quint32> ids;
    for (Function *f : m_doc->functions())
    {
        const QString p = f->path(true);
        if (p == path || p.startsWith(path + QLatin1Char('/')))
        {
            all << f;
            ids << f->id();
        }
    }
    if (all.isEmpty())
        return;
    m_doc->paletteOps()->functionStep(tr("remove folder %1").arg(path), ids, [&](QList<quint32> &) {
        for (Function *f : all)
        {
            const QString rest = f->path(true).mid(path.length());   // "" or "/sub"
            QString to = up + rest;
            if (to.startsWith(QLatin1Char('/')))
                to = to.mid(1);
            f->setPath(to);
        }
        return true;
    });
    m_outline->rebuild();
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
    revealPalette(pid);              // select + scroll to it in the list
    m_workbench->editPalette(pid);   // edit it at once
}

void ProgrammingManager::revealPalette(quint32 paletteId)
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == NULL)
        return;
    QTreeWidgetItem *item = m_paletteTree->paletteItem(p);
    if (item == NULL)
        return;
    setSection(PalettesSection);
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
        m_workbench->editPalette(pid);
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
    m_workbench->selectFixtures(fixtureIds);
}

void ProgrammingManager::slotSelectionTools()
{
    if (m_flashBtn)
        m_flashBtn->setEnabled(!m_workbench->selectedFixtures().isEmpty());
    updateParkButtonLabel();
    updateMarkButtonLabel();
}

void ProgrammingManager::slotCopy()
{
    // Palette list focused with a palette selected? Copy the palette instead.
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
        if (r.kind == ProgrammingOutline::SceneRow || r.kind == ProgrammingOutline::FunctionRow)
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
    if (m_doc->function(m_clipboardFunction) != NULL)
        duplicateFunctions({ m_clipboardFunction });
}

void ProgrammingManager::duplicatePalette(quint32 pid)
{
    const quint32 copyId = m_doc->paletteOps()->duplicate(pid);
    QLCPalette *copy = m_doc->palette(copyId);
    if (copy == NULL)
        return;
    m_paletteTree->updateTree();
    revealPalette(copy->id());
    m_workbench->editPalette(copy->id());
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
    connect(m_powerTimer, &QTimer::timeout, this, &ProgrammingManager::powerTick);
    // A fixture added, re-patched, its definition or circuit changed: the
    // estimate reads more than the levels.
    connect(m_doc, &Doc::modified, this, [this]() { m_powerDirty = true; });
    connect(m_doc, &Doc::fixtureAdded, this, [this]() { m_powerDirty = true; });
    connect(m_doc, &Doc::fixtureRemoved, this, [this]() { m_powerDirty = true; });
    connect(m_doc, &Doc::fixtureChanged, this, [this]() { m_powerDirty = true; });
}

quint64 ProgrammingManager::powerInputsKey() const
{
    size_t h = 0x9e3779b9;
    QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : std::as_const(universes))
        h = qHashMulti(h, u != nullptr ? u->preGMValues() : QByteArray());
    m_doc->inputOutputMap()->releaseUniverses(false);

    foreach (const PowerSource &src, m_doc->powerDistribution()->sources())
    {
        h = qHashMulti(h, src.type, src.voltage, src.vaRating, src.circuits.size());
        foreach (const PowerCircuit &cir, src.circuits)
            h = qHashMulti(h, cir.voltage, cir.ratedAmps, cir.deratePercent,
                           cir.demandPercent, cir.fixtures);
    }
    QSettings settings;
    h = qHashMulti(h, settings.value(QStringLiteral("power/defaultVoltage"), 120.0).toDouble(),
                   settings.value(QStringLiteral("power/powerFactor"), 0.9).toDouble(),
                   m_doc->fixtures().size());
    return quint64(h);
}

void ProgrammingManager::powerTick()
{
    if (m_doc->mode() != Doc::Design)
        return;
    /* It used to re-estimate every half second whatever happened: a show left
       open and idle ran the whole rig's power model twice a second forever. */
    if (!m_powerDirty && powerInputsKey() == m_powerKey)
        return;
    recomputePower();
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
    // What this estimate is of (powerTick() skips the next if it is the same).
    m_powerKey = powerInputsKey();
    m_powerDirty = false;
    ++m_powerRuns;

    // Take a consistent pre-Grand-Master snapshot of all universes (the
    // "designed peak", free of Grand Master/blackout). claimUniverses() shares
    // the MasterTimer's write mutex, so only the copy is made under it
    // (release with changed=false: read-only): the estimate itself, a few ms
    // on a big rig, used to hold the engine's next tick back twice a second.
    QVector<QByteArray> values;
    {
        QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
        values.reserve(universes.size());
        for (Universe *u : std::as_const(universes))
            values << (u != nullptr ? u->preGMValues() : QByteArray());
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    PowerEstimator::PreGMReader reader =
        [&values](quint32 uni, quint32 addr) -> uchar
        {
            if (uni >= quint32(values.size()) || addr >= quint32(values.at(int(uni)).size()))
                return 0;
            return uchar(values.at(int(uni)).at(int(addr)));
        };

    QHash<quint32, double> perFixture;
    PowerEstimator::estimateWatts(m_doc, reader, &perFixture);

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
    return m_workbench != nullptr ? m_workbench->currentPaletteId() : QLCPalette::invalidId();
}

quint32 ProgrammingManager::currentSceneId() const
{
    return m_workbench != nullptr ? m_workbench->currentSceneId() : Function::invalidId();
}

void ProgrammingManager::slotModeChanged()
{
    // The Workbench starts and stops its own preview; here the power chip
    // and the follow-spot pin.
    updatePowerEstimateActive();
    if (m_doc->mode() == Doc::Design)
    {
        // The live follow-spot pin means nothing while designing: only the
        // draggable StageTarget remains.
        if (Monitor *mon = Monitor::instance())
            mon->setFollowSpotPin(false, 0.0f, 0.0f);
    }
    else
    {
        // Operate: the pin is meaningful -- seed it at the scene's aim.
        if (ProgrammerController *pc = m_doc->programmer())
            pc->seedStageAimFromScene(m_workbench->currentSceneId());
    }
}

void ProgrammingManager::showEvent(QShowEvent *ev)
{
    // Pick up folder/path changes made in the Functions tab while away,
    // preserving the current scene selection.
    m_paletteTree->updateTree();
    // Clicks in the 2D view pick fixtures for the Workbench.
    if (Monitor *mon = Monitor::instance())
        connect(mon, &Monitor::fixturesSelected,
                this, &ProgrammingManager::slotFixturesSelected, Qt::UniqueConnection);
    // Auto-bind joystick axes from I/O → Mapping → Joystick profile if not yet bound.
    if (ProgrammerController *pc = m_doc->programmer())
        pc->autoBindFromProfile();
    updatePowerEstimateActive();
    slotParkChanged(); // reflect any parks restored from the workspace
    QWidget::showEvent(ev);
}

void ProgrammingManager::hideEvent(QHideEvent *ev)
{
    // The Workbench stops its own preview as it hides.
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
        case Function::ScriptType:     f = new Script(m_doc);     base = tr("New Script"); break;
        case Function::SequenceType:
        {
            // A sequence plays one scene's channels: its own, hidden.
            Scene *bound = new Scene(m_doc);
            bound->setVisible(false);
            if (!m_doc->addFunction(bound))
            {
                delete bound;
                return;
            }
            Sequence *seq = new Sequence(m_doc);
            seq->setBoundSceneID(bound->id());
            f = seq;
            base = tr("New Sequence");
            break;
        }
        case Function::AudioType:
        case Function::VideoType:
        {
            const bool audio = type == Function::AudioType;
            const QStringList ext = audio ? m_doc->audioPluginCache()->getSupportedFormats()
                                          : Video::getVideoCapabilities();
            const QString filter = (audio ? tr("Audio Files (%1)") : tr("Video Files (%1)")).arg(ext.join(QLatin1Char(' ')))
                                   + QStringLiteral(";;") + tr("All Files (*)");
            const QStringList files = QFileDialog::getOpenFileNames(this, audio ? tr("Open Audio File")
                                                                                : tr("Open Video File"),
                                                                    QDir::homePath(), filter);
            if (!files.isEmpty())
                addMediaFunctions(type, files, folder);
            return;
        }
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

    // Selected in the navigator, loaded in the Workbench, ready for F2 and drops.
    m_outline->rebuild();
    showFunction(f->id());
}

void ProgrammingManager::duplicateFunctions(const QList<quint32> &fids)
{
    // One step on the shared history: ⌘Z takes the copies away, ⌘⇧Z brings
    // them back with their ids.
    quint32 last = Function::invalidId();
    m_doc->paletteOps()->functionStep(fids.size() > 1 ? tr("Duplicate %1 functions").arg(fids.size())
                                                      : tr("Duplicate"),
                                      QList<quint32>(), [&](QList<quint32> &made) {
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
            made << copy->id();
            last = copy->id();
        }
        return !made.isEmpty();
    });

    if (m_doc->function(last) == NULL)
        return;

    m_outline->rebuild();
    showFunction(last);
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

    deleteFunctionsUndoably(fids);
}

void ProgrammingManager::deleteFunctionsUndoably(const QList<quint32> &fids)
{
    /* One step on the shared history. The functions using them (collections,
       chasers, shows) ride in it whole, so Undo brings each deleted function
       back with its id and puts it back where it was in each of them. Cues
       name a function by id: they find it again by themselves. VC buttons and
       cue lists let go of a deleted function; Undo hands it back to them. */
    QList<quint32> gone, keep;
    QStringList names;
    ProgrammerController *pc = m_doc->programmer();
    for (quint32 fid : fids)
    {
        Function *f = m_doc->function(fid);
        if (f == NULL || gone.contains(fid))
            continue;
        gone << fid;
        names << f->name();
        if (pc != NULL)
            for (const ProgrammerController::Usage &u : pc->functionUsage(fid))
                if (!keep.contains(u.ownerId))
                    keep << u.ownerId;
    }
    if (gone.isEmpty())
        return;

    QList<QPair<QPointer<VCButton>, quint32>> buttons;
    QList<QPair<QPointer<VCCueList>, quint32>> cueLists;
    if (VirtualConsole *vc = VirtualConsole::instance())
        if (vc->contents() != NULL)
        {
            for (VCButton *b : vc->contents()->findChildren<VCButton *>())
                if (gone.contains(b->function()))
                    buttons << qMakePair(QPointer<VCButton>(b), b->function());
            for (VCCueList *cl : vc->contents()->findChildren<VCCueList *>())
                if (gone.contains(cl->chaserID()))
                    cueLists << qMakePair(QPointer<VCCueList>(cl), cl->chaserID());
        }
    ShowControl *sc = m_doc->showControl();
    const quint32 mainCueList = sc != NULL && gone.contains(sc->mainCueList())
                                    ? sc->mainCueList() : Function::invalidId();

    const QString label = gone.size() > 1 ? tr("Delete %1 functions").arg(gone.size())
                                          : tr("Delete %1").arg(names.first());
    // Delete by id, never by tree item: the first deletion emits functionRemoved,
    // which rebuilds the outline (and tears down the canvas if it held one of
    // these) — so any QTreeWidgetItem* captured before the loop would dangle.
    const bool stepped = m_doc->paletteOps()->functionStep(label, gone + keep, [&](QList<quint32> &) {
        for (quint32 fid : gone)
            m_doc->deleteFunction(fid);
        return true;
    });
    if (!stepped || (buttons.isEmpty() && cueLists.isEmpty() && mainCueList == Function::invalidId()))
        return;

    Doc *doc = m_doc;
    m_doc->paletteOps()->attachToLastStep([doc, buttons, cueLists, mainCueList](bool undone) {
        // Redone: the functions go again, and the widgets let go as before.
        if (!undone)
            return;
        for (const QPair<QPointer<VCButton>, quint32> &b : buttons)
            if (!b.first.isNull() && doc->function(b.second) != NULL
                && b.first->function() == Function::invalidId())
                b.first->setFunction(b.second);
        for (const QPair<QPointer<VCCueList>, quint32> &cl : cueLists)
            if (!cl.first.isNull() && doc->function(cl.second) != NULL
                && cl.first->chaserID() == Function::invalidId())
                cl.first->setChaser(cl.second);
        ShowControl *sc = doc->showControl();
        if (mainCueList != Function::invalidId() && sc != NULL && doc->function(mainCueList) != NULL
            && sc->mainCueList() == UINT_MAX)
            sc->setMainCueList(mainCueList);
    });
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
    for (quint32 fid : m_workbench->selectedFixtures())
        pc->flashFixture(fid);
}

/*****************************************************************************
 * Park (hold fixtures out of cue output)
 *****************************************************************************/

void ProgrammingManager::slotParkSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == NULL || m_workbench->selectedFixtures().isEmpty())
        return;

    // Toggle: if the whole selection is already parked, unpark it; else park.
    bool allParked = true;
    for (quint32 fid : m_workbench->selectedFixtures())
        if (!pc->isFixtureParked(fid))
            allParked = false;

    if (allParked)
        pc->unparkFixtures(m_workbench->selectedFixtures());
    else
        pc->parkFixtures(m_workbench->selectedFixtures());
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
    if (pc == NULL || m_workbench->selectedFixtures().isEmpty())
        return;

    // Toggle: if the whole selection is already marked, unmark it; else mark.
    bool allMarked = true;
    for (quint32 fid : m_workbench->selectedFixtures())
        if (!pc->isFixtureMarked(fid))
            allMarked = false;

    if (allMarked)
        pc->unmarkFixtures(m_workbench->selectedFixtures());
    else
        pc->markFixtures(m_workbench->selectedFixtures());
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

    bool anySel = !m_workbench->selectedFixtures().isEmpty();
    bool allMarked = anySel;
    for (quint32 fid : m_workbench->selectedFixtures())
        if (!pc->isFixtureMarked(fid))
            allMarked = false;

    m_markBtn->setEnabled(anySel);
    m_markBtn->setText(anySel && allMarked ? tr("Unmark") : tr("Mark"));
}

void ProgrammingManager::slotSnapshotLive()
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_workbench->currentSceneId()));
    if (scene == NULL)
    {
        QMessageBox::information(this, tr("Snapshot"),
            tr("Select a scene in the Workbench first — the snapshot bakes into it."));
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
    // Shown, played, one step on the history.
    m_workbench->sceneEditedOutside(tr("snapshot into %1").arg(scene->name()));
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
    bool anySel = !m_workbench->selectedFixtures().isEmpty();
    bool allParked = anySel;
    for (quint32 fid : m_workbench->selectedFixtures())
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
    LookEditor *le = m_workbench->lookEditor();
    if (le && le->paletteId() != QLCPalette::invalidId())
        le->setPalette(le->paletteId());
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
    // Show the tapped tempo (the Beat button follows the spin box) without
    // setting it again: slotBpmChanged() would restart the beat the tap set.
    disconnect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged),
               this, &ProgrammingManager::slotBpmChanged);
    m_bpmSpin->setValue(qRound(bpm));
    connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ProgrammingManager::slotBpmChanged);
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
        "<li><b>Palettes | Effects | Scenes</b> at the top left picks what the "
        "navigator lists; the <b>Workbench</b> on the right stays as it is.</li>"
        "<li>In <b>Scenes</b>, click a scene, collection or chaser: it loads in the "
        "Workbench. <b>+ Scene</b> makes one (its menu: a collection, a chaser).</li>"
        "<li>Drag <b>palettes</b> (Palettes section) onto a scene's layer to build "
        "the look — a colour, a dimmer level, a position, an effect.</li>"
        "<li>Drag <b>fixture groups</b> on as targets to follow their membership, "
        "or individual <b>fixtures</b> to fix them in place. They are under every "
        "section; <b>Search fixtures &amp; groups</b> narrows a long list.</li>"
        "<li>No palettes yet? Right-click the list in <b>Palettes</b> to create "
        "your first one.</li>"
        "</ul>"
        "<p><b>Layers.</b> A collection's members are its layers: a scene opens up "
        "into its targets and looks, an EFX or RGB Matrix plays as it is (▣). "
        "<b>+ Scene</b>, <b>+ EFX</b> and <b>+ RGB Matrix</b> add one (a new EFX or "
        "matrix is made on the selected layer's groups), ▲ ▼ reorder, ⌫ takes one "
        "out. <b>▣ Open</b> loads a layer on its own; <b>↩</b> comes back.</p>"
        "<p><b>Looks and Targets.</b> Every look applies to every target. Click a "
        "row and the editor pane edits it; drag a row to another scene to move it "
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
        "<p>Every edit is one step on the history the Show Tree shares (⌘Z). "
        "Reopen this any time from <i>How this works</i>, or press F1 for "
        "the whole guide.</p>"));
    QPushButton *more = box.addButton(tr("Open the Full Guide"), QMessageBox::HelpRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == more)
        HelpBrowser::open(QStringLiteral("build.md"));
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

    /* Step chaser forward or back: only where a profile binds it on
       purpose (the bundled Xbox and PlayStation profiles no longer put it
       on A/B: a pad picked up mid-show must never move the show, u8).
       A Show Tree cue list is never stepped behind show control's back:
       its step is a GO or Back through the console's one run path, with
       its sub-cues, LOAD and the preview hold. Any other running chaser
       (a VC cue list) steps, as in QLC+. */
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

    ShowTreeController *st = m_doc->showTree();
    foreach (Function *f, m_doc->functions())
    {
        Chaser *ch = qobject_cast<Chaser*>(f);
        if (ch == nullptr || !ch->isRunning())
            continue;
        bool treeList = false, mainOrParallel = false;
        if (st != nullptr)
            for (const ShowTree::Production &p : st->productions())
                for (const ShowTree::CueList &l : p.lists)
                    if (l.chaser == ch->id())
                    {
                        treeList = true;
                        mainOrParallel = !l.looks && !l.isSubCues();
                    }
        if (treeList && !mainOrParallel)
            continue;              // sub-cues and hits: Sub GO / their own keys
        if (treeList)
        {
            ShowControl::Request r;
            r.command = stepType == ChaserNextStep ? ShowControl::Go : ShowControl::Back;
            r.chaser = ch->id();
            m_doc->showControl()->run(r, ShowControl::RoutePad);
            continue;
        }
        ch->setAction(chaserAction);
    }
}

// ─── Bundle stamp & save ──────────────────────────────────────────────────────

void ProgrammingManager::slotStampBundle(const QString &bundleName)
{
    const quint32 sceneId = m_workbench->currentSceneId();
    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (!scene)
    {
        QMessageBox::information(this, tr("No look selected"),
            tr("Select a scene in the Workbench before stamping a Bundle."));
        return;
    }

    const QLCBundle bundle = m_bundleCache->bundle(bundleName);
    if (!bundle.isValid()) return;

    // Replace: remove all existing palettes from this scene before stamping.
    // (One step on the shared history: the Workbench records it from the
    // scene as it was.)
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
    // Sync effect instances at once — old ones go, new ones from the looks
    // now — so it takes whether or not this tab is showing (the preview's
    // refresh is gated on it); then the Workbench needn't sync them again.
    if (m_doc->effectScriptRunner())
    {
        m_doc->effectScriptRunner()->syncScene(sceneId);
        m_workbench->markEffectsSynced();
    }
    m_paletteTree->updateTree();
    m_workbench->sceneEditedOutside(tr("stamp %1 on %2").arg(bundleName, scene->name()));
    m_outline->scheduleRebuild();
}

void ProgrammingManager::slotSaveAsBundle()
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_workbench->currentSceneId()));
    if (!scene)
    {
        QMessageBox::information(this, tr("No look selected"),
            tr("Select a scene in the Workbench to save as a Bundle."));
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
    m_outline->rebuild();
    m_workbench->refreshAll();
}

void ProgrammingManager::slotRedo()
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || !st->canRedoShared() || !st->redoShared())
        return;
    m_paletteTree->updateTree();
    m_outline->rebuild();
    m_workbench->refreshAll();
}

/*****************************************************************************
 * W3: the Workbench lent to the Show Tree
 *****************************************************************************/

Workbench *ProgrammingManager::lendWorkbench()
{
    if (m_workbench == nullptr || m_workbenchStandIn != nullptr || m_mainSplit == nullptr)
        return nullptr;
    const int index = m_mainSplit->indexOf(m_workbench);
    if (index < 0)
        return nullptr;
    // What Build had loaded comes back with it.
    m_lentFrom.reset(m_workbench->stack() != nullptr ? m_workbench->stack()->reopen(nullptr) : nullptr);
    QTreeWidgetItem *cur = m_workbench->rows()->currentItem();
    m_lentSelection = cur != nullptr && cur->isSelected()
                      ? cur->data(0, ProgrammingOutline::KeyRole).toString() : QString();

    QLabel *note = new QLabel(tr("The Workbench is in the Show Tree, open on a cue.\n"
                                 "↩ Show Tree there brings it back here."));
    note->setObjectName(QStringLiteral("workbenchLentNote"));
    note->setAlignment(Qt::AlignCenter);
    note->setWordWrap(true);
    note->setEnabled(false);
    m_workbenchStandIn = note;
    m_mainSplit->replaceWidget(index, note);     // the Workbench's parent: none, for now
    return m_workbench;
}

void ProgrammingManager::returnWorkbench(Workbench *workbench)
{
    if (workbench == nullptr || workbench != m_workbench || m_workbenchStandIn == nullptr)
        return;
    const int index = m_mainSplit->indexOf(m_workbenchStandIn);
    QWidget *standIn = m_workbenchStandIn;
    m_workbenchStandIn = nullptr;
    if (index >= 0)
        m_mainSplit->replaceWidget(index, m_workbench);
    else
        m_mainSplit->addWidget(m_workbench);
    standIn->deleteLater();
    m_workbench->setReturnTo(QString(), nullptr);
    // Loaded as it was (if it is still there).
    WorkbenchStack *back = m_lentFrom.isNull() ? nullptr : m_lentFrom->reopen(m_workbench);
    m_lentFrom.reset();
    if (back != nullptr)
    {
        m_workbench->setStack(back);
        if (!m_lentSelection.isEmpty())
            for (QTreeWidgetItemIterator it(m_workbench->rows()); *it; ++it)
                if ((*it)->data(0, ProgrammingOutline::KeyRole).toString() == m_lentSelection)
                {
                    m_workbench->rows()->setCurrentItem(*it);
                    break;
                }
    }
    else
        m_workbench->clear();
    m_workbench->show();
}
