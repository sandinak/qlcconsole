/*
  Q Light Controller Plus
  programmerstrip.cpp

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

#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QWidgetAction>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QToolButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QMenu>

#include "programmerstrip.h"
#include "programmercontroller.h"
#include "speedmasterspanel.h"
#include "measurementunits.h"
#include "inputoutputmap.h"
#include "scenevalue.h"
#include "workbench.h"
#include "universe.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

ProgrammerStrip::ProgrammerStrip(Doc *doc, const Host &host, QWidget *parent)
    : QToolBar(parent)
    , m_doc(doc)
    , m_host(host)
{
    setObjectName(QStringLiteral("programmingToolBar"));
    setMovable(false);
    setFloatable(false);

    m_highlightBtn = new QPushButton(tr("Highlight"), this);
    m_highlightBtn->setObjectName("programmingHighlight");
    m_highlightBtn->setCheckable(true);
    m_highlightBtn->setToolTip(tr("Drive selected fixtures to full white so you can identify them in the rig"));

    m_flashBtn = new QPushButton(tr("Flash"), this);
    m_flashBtn->setToolTip(tr("Momentarily flash the selected fixtures to identify them in the rig"));
    m_flashBtn->setEnabled(false);

    m_parkBtn = new QPushButton(tr("Park"), this);
    m_parkBtn->setToolTip(tr("Hold the selected fixtures at their current values, out of cue "
                             "output, until unparked (saved with the workspace)"));
    m_parkBtn->setEnabled(false);

    m_unparkBtn = new QPushButton(tr("Unpark All"), this);
    m_unparkBtn->setToolTip(tr("Release every parked fixture"));
    m_unparkBtn->setEnabled(false);

    m_markBtn = new QPushButton(tr("Mark"), this);
    m_markBtn->setToolTip(tr("Move-in-black: pre-position the selected DARK fixtures "
                             "(pan/tilt, colour, gobo, beam) to where they'll be revealed, "
                             "held out of cue output. Auto-releases when a cue lights them; "
                             "saved with the workspace."));
    m_markBtn->setEnabled(false);

    m_unmarkBtn = new QPushButton(tr("Unmark All"), this);
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

    // BPM / internal beat generator
    m_bpmSpin = new QSpinBox(this);
    m_bpmSpin->setRange(20, 400);
    m_bpmSpin->setValue(120);
    m_bpmSpin->setToolTip(tr("Beats per minute for internal beat generator"));
    m_bpmSpin->setEnabled(false);

    m_tapBtn = new QPushButton(tr("Tap"), this);
    m_tapBtn->setToolTip(tr("Tap to set BPM from live tempo"));
    m_tapBtn->setEnabled(false);

    m_bpmBtn = new QPushButton(tr("Beat On"), this);
    m_bpmBtn->setCheckable(true);
    m_bpmBtn->setToolTip(tr("Enable QLC+ internal beat generator (drives _beat/_bpm in generators)"));

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setRange(10, 500);
    m_speedSpin->setSingleStep(10);
    m_speedSpin->setValue(100);
    m_speedSpin->setSuffix(tr("%"));
    m_speedSpin->setToolTip(tr("Joystick speed for position editing (100% = %1 a second at full deflection)")
                                .arg(MeasurementUnits::formatLength(2000.0)));

    m_saveBtn = new QPushButton(tr("Save Positions"), this);
    m_saveBtn->setObjectName(QStringLiteral("savePositions"));
    m_saveBtn->setEnabled(false);
    m_saveBtn->setToolTip(tr(
        "Finalize joystick-driven pan/tilt/aim edits and save the workspace.\n"
        "Only these joystick edits need this button — every other look edit "
        "here (color, dimmer, gradient, …) is already live; the footer's "
        "● Unsaved changes indicator tracks those."));

    /* One row, no scrolling: what is pressed all the time stays a button --
       Highlight, Flash, Tap, Save Positions -- and the rest sits in
       drop-downs, as the Lighting Studio's footer does. The buttons behind
       the menu items are kept (hidden): their state, slots and enabling are
       in one place, and each item mirrors its button when the menu opens. */
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
    mirror(parkMenu->menu(), m_unparkBtn, tr("Unpark All"));

    m_markMenu = dropDown(tr("Move in Black"), QStringLiteral("markMenu"));
    mirror(m_markMenu->menu(), m_markBtn, tr("Mark selected (pre-position in black)"));
    mirror(m_markMenu->menu(), m_unmarkBtn, tr("Unmark All"));
    m_markMenu->menu()->addSeparator();
    mirror(m_markMenu->menu(), m_autoMibBtn, tr("Auto move-in-black"));
    fieldAction(m_markMenu->menu(), tr("Lead time"), m_mibGapSpin);
    /* The drop-down says when Auto is on: lit as the Show Tree's MIB button
       is, "Move in black · Auto", and "MIB · n" while it pre-positions n
       heads. */
    m_markMenu->setStyleSheet(QStringLiteral(
        "QToolButton#markMenu[mibAuto=\"true\"] { background: palette(highlight);"
        "  color: palette(highlighted-text); border-radius: 4px;"
        "  padding: 2px 14px 2px 5px; }"));   // room for the arrow

    QToolButton *beatMenu = dropDown(tr("Beat"), QStringLiteral("beatMenu"));
    m_beatMenu = beatMenu;
    mirror(beatMenu->menu(), m_bpmBtn, tr("Internal beat on"));
    fieldAction(beatMenu->menu(), tr("BPM"), m_bpmSpin);
    beatMenu->menu()->addSeparator();
    QAction *mastersAct = beatMenu->menu()->addAction(tr("Speed Masters…"));
    mastersAct->setObjectName(QStringLiteral("speedMastersAction"));
    mastersAct->setToolTip(tr("The four speed masters: a tempo, tap, rate and pause each, "
                              "for effects, looks and chases to run on"));
    connect(mastersAct, &QAction::triggered, this, [this]() { SpeedMastersPanel::open(m_doc, this); });
    // The button says the tempo, so it needn't be opened to read it.
    connect(m_bpmBtn, &QPushButton::toggled, this, [this](bool) { updateBeatText(); });
    connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updateBeatText(); });

    QToolButton *moreMenu = dropDown(tr("More"), QStringLiteral("programmingMoreMenu"));
    mirror(moreMenu->menu(), m_snapshotBtn, tr("Snapshot live output into the scene"));
    fieldAction(moreMenu->menu(), tr("Jog speed"), m_speedSpin);

    // Highlight, Flash | Park, Move in black ... Beat, Tap | Save Positions, More
    addWidget(m_highlightBtn);
    addWidget(m_flashBtn);
    addWidget(parkMenu);
    addWidget(m_markMenu);
    {
        QWidget *stretch = new QWidget(this);
        stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        addWidget(stretch);
    }
    addWidget(beatMenu);
    addWidget(m_tapBtn);
    /* Save Positions before More: a toolbar folds its last items into »
       first, and More is the least used; at 1024 wide Save Positions went
       into » (UX-36). */
    addWidget(m_saveBtn);
    addWidget(moreMenu);

    connect(m_snapshotBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotSnapshotLive);
    connect(m_highlightBtn, &QPushButton::toggled, this, &ProgrammerStrip::slotHighlightToggled);
    connect(m_flashBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotFlashSelection);
    connect(m_parkBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotParkSelection);
    connect(m_unparkBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotUnparkAll);
    connect(m_markBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotMarkSelection);
    connect(m_unmarkBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotUnmarkAll);
    connect(m_autoMibBtn, &QPushButton::toggled, this, [this](bool on) {
        if (ProgrammerController *pc = m_doc->programmer())
            pc->setAutoMoveInBlack(on);
    });
    connect(m_mibGapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double s) {
        if (ProgrammerController *pc = m_doc->programmer())
            pc->setMoveInBlackDarkGapMs(int(s * 1000.0 + 0.5));
    });
    connect(m_saveBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotSavePositions);
    connect(m_speedSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (ProgrammerController *pc = m_doc->programmer())
            pc->setJoystickSensitivity(v / 100.0f);
    });
    connect(m_bpmBtn, &QPushButton::toggled, this, &ProgrammerStrip::slotBpmToggled);
    connect(m_bpmSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProgrammerStrip::slotBpmChanged);
    connect(m_tapBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotTapBeat);

    // The followspot's axis bindings: kept for their wiring, never shown
    // (axis binding lives in I/O ▸ Mapping ▸ Joystick).
    {
        QGroupBox *panel = new QGroupBox(tr("Followspot settings"), this);
        m_followSpotPanel = panel;
        QFormLayout *form = new QFormLayout(panel);
        form->setContentsMargins(4, 4, 4, 4);
        form->setSpacing(4);
        QHBoxLayout *xRow = new QHBoxLayout;
        m_fsBindXBtn = new QPushButton(tr("Bind X (pan)"), panel);
        m_fsXLabel = new QLabel(tr("—"), panel);
        xRow->addWidget(m_fsBindXBtn);
        xRow->addWidget(m_fsXLabel, 1);
        form->addRow(tr("Pan axis:"), xRow);
        QHBoxLayout *yRow = new QHBoxLayout;
        m_fsBindYBtn = new QPushButton(tr("Bind Y (tilt)"), panel);
        m_fsYLabel = new QLabel(tr("—"), panel);
        yRow->addWidget(m_fsBindYBtn);
        yRow->addWidget(m_fsYLabel, 1);
        form->addRow(tr("Tilt axis:"), yRow);
        QLabel *inputNote = new QLabel(
            tr("<small>⚠ Controller must be assigned as a universe <b>Input</b> in the "
               "Input/Output Manager before Bind will detect it.</small>"), panel);
        inputNote->setWordWrap(true);
        form->addRow(inputNote);
        m_fsClearBtn = new QPushButton(tr("Clear Bindings"), panel);
        form->addRow(QString(), m_fsClearBtn);
        m_fsJoyLabel = new QLabel(tr("—"), panel);
        m_fsJoyLabel->setStyleSheet("font-family: monospace;");
        form->addRow(tr("Joystick:"), m_fsJoyLabel);
        panel->hide();
        connect(m_fsBindXBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotFollowSpotBindX);
        connect(m_fsBindYBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotFollowSpotBindY);
        connect(m_fsClearBtn, &QPushButton::clicked, this, &ProgrammerStrip::slotFollowSpotClearBindings);
    }

    // Every strip follows the one state, whoever changed it.
    if (ProgrammerController *pc = m_doc->programmer())
    {
        connect(pc, &ProgrammerController::axisBindingChanged,
                this, &ProgrammerStrip::slotFollowSpotBindingChanged);
        connect(pc, &ProgrammerController::highlightActiveChanged, this, [this](bool on) {
            QSignalBlocker b(m_highlightBtn);
            m_highlightBtn->setChecked(on);
        });
        connect(pc, &ProgrammerController::parkChanged, this, &ProgrammerStrip::slotParkChanged);
        connect(pc, &ProgrammerController::markChanged, this, &ProgrammerStrip::slotMarkChanged);
        connect(pc, &ProgrammerController::programmerSelectionChanged,
                this, &ProgrammerStrip::slotSelectionTools);
        connect(pc, &ProgrammerController::autoMoveInBlackChanged, this, [this, pc](bool on) {
            QSignalBlocker b1(m_autoMibBtn), b2(m_mibGapSpin);
            m_autoMibBtn->setChecked(on);
            m_mibGapSpin->setValue(pc->moveInBlackDarkGapMs() / 1000.0);
            updateMarkMenuText();
        });
        connect(pc, &ProgrammerController::moveInBlackPresetsChanged, this,
                [this](int) { updateMarkMenuText(); });
        connect(pc, &ProgrammerController::joystickSensitivityChanged, this, [this](float s) {
            QSignalBlocker b(m_speedSpin);
            m_speedSpin->setValue(qRound(s * 100.0f));
        });
        connect(pc, &ProgrammerController::joystickUpdated, this, &ProgrammerStrip::slotJoystickUpdated);
        connect(pc, &ProgrammerController::designPositionWritten,
                this, &ProgrammerStrip::slotDesignPositionWritten);
        // Seed from the (possibly already loaded) workspace.
        QSignalBlocker b1(m_autoMibBtn), b2(m_mibGapSpin), b3(m_highlightBtn), b4(m_speedSpin);
        m_autoMibBtn->setChecked(pc->isAutoMoveInBlack());
        m_mibGapSpin->setValue(pc->moveInBlackDarkGapMs() / 1000.0);
        m_highlightBtn->setChecked(pc->isHighlightActive());
        m_speedSpin->setValue(qRound(pc->joystickSensitivity() * 100.0f));
    }
    if (InputOutputMap *ioMap = m_doc->inputOutputMap())
    {
        connect(ioMap, &InputOutputMap::beatGeneratorTypeChanged, this, &ProgrammerStrip::slotBeatFromEngine);
        connect(ioMap, &InputOutputMap::bpmNumberChanged, this, [this](int) { slotBeatFromEngine(); });
    }
    slotBeatFromEngine();
    updateMarkMenuText();
    slotParkChanged();
    slotMarkChanged();
    slotSelectionTools();
}

ProgrammerStrip::~ProgrammerStrip()
{
}

void ProgrammerStrip::showEvent(QShowEvent *ev)
{
    // Auto-bind the joystick's axes from I/O ▸ Mapping ▸ Joystick if not yet
    // bound, and show what a show loaded since restored (parks, marks).
    if (ProgrammerController *pc = m_doc->programmer())
        pc->autoBindFromProfile();
    slotParkChanged();
    slotMarkChanged();
    slotSelectionTools();
    slotBeatFromEngine();
    QToolBar::showEvent(ev);
}

QList<quint32> ProgrammerStrip::selection() const
{
    ProgrammerController *pc = m_doc->programmer();
    return pc != nullptr ? pc->programmerSelection() : QList<quint32>();
}

/*****************************************************************************
 * Highlight, Flash
 *****************************************************************************/

void ProgrammerStrip::slotHighlightToggled(bool on)
{
    if (ProgrammerController *pc = m_doc->programmer())
        pc->setHighlightActive(on);
}

void ProgrammerStrip::slotFlashSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    for (quint32 fid : selection())
        pc->flashFixture(fid);
}

void ProgrammerStrip::slotSelectionTools()
{
    m_flashBtn->setEnabled(!selection().isEmpty());
    updateParkButtonLabel();
    updateMarkButtonLabel();
}

/*****************************************************************************
 * Park (hold fixtures out of cue output)
 *****************************************************************************/

void ProgrammerStrip::slotParkSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    const QList<quint32> sel = selection();
    if (pc == nullptr || sel.isEmpty())
        return;
    // Toggle: if the whole selection is already parked, unpark it; else park.
    bool allParked = true;
    for (quint32 fid : sel)
        if (!pc->isFixtureParked(fid))
            allParked = false;
    if (allParked)
        pc->unparkFixtures(sel);
    else
        pc->parkFixtures(sel);
}

void ProgrammerStrip::slotUnparkAll()
{
    if (ProgrammerController *pc = m_doc->programmer())
        pc->unparkAllFixtures();
}

void ProgrammerStrip::slotParkChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    m_unparkBtn->setEnabled(pc->hasParkedFixtures());
    updateParkButtonLabel();
}

void ProgrammerStrip::updateParkButtonLabel()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    // If every selected fixture is already parked, offer Unpark instead of Park.
    const QList<quint32> sel = selection();
    const bool anySel = !sel.isEmpty();
    bool allParked = anySel;
    for (quint32 fid : sel)
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
 * Mark (move-in-black: pre-position dark fixtures)
 *****************************************************************************/

void ProgrammerStrip::slotMarkSelection()
{
    ProgrammerController *pc = m_doc->programmer();
    const QList<quint32> sel = selection();
    if (pc == nullptr || sel.isEmpty())
        return;
    // Toggle: if the whole selection is already marked, unmark it; else mark.
    bool allMarked = true;
    for (quint32 fid : sel)
        if (!pc->isFixtureMarked(fid))
            allMarked = false;
    if (allMarked)
        pc->unmarkFixtures(sel);
    else
        pc->markFixtures(sel);
}

void ProgrammerStrip::slotUnmarkAll()
{
    if (ProgrammerController *pc = m_doc->programmer())
        pc->unmarkAllFixtures();
}

void ProgrammerStrip::slotMarkChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    m_unmarkBtn->setEnabled(pc->hasMarkedFixtures());
    updateMarkButtonLabel();
}

void ProgrammerStrip::updateMarkButtonLabel()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    const QList<quint32> sel = selection();
    const bool anySel = !sel.isEmpty();
    bool allMarked = anySel;
    for (quint32 fid : sel)
        if (!pc->isFixtureMarked(fid))
            allMarked = false;
    m_markBtn->setEnabled(anySel);
    m_markBtn->setText(anySel && allMarked ? tr("Unmark") : tr("Mark"));
}

void ProgrammerStrip::updateMarkMenuText()
{
    ProgrammerController *pc = m_doc->programmer();
    const bool on = pc != nullptr && pc->isAutoMoveInBlack();
    const int presets = pc != nullptr ? pc->moveInBlackPresetCount() : 0;
    // MIDDLE DOT, as the Show Tree's button.
    m_markMenu->setText(!on ? tr("Move in Black")
                        : presets > 0 ? tr("MIB \302\267 %1").arg(presets)
                                      : tr("Move in Black \302\267 Auto"));
    m_markMenu->setToolTip(!on ? tr("Mark, unmark and auto move-in-black (off)")
                           : presets > 0
                               ? tr("Auto move-in-black is on: pre-positioning %n head(s) "
                                    "in black for the next cue.", "", presets)
                               : tr("Auto move-in-black is on: nothing to pre-position "
                                    "right now."));
    if (m_markMenu->property("mibAuto").toBool() != on)
    {
        m_markMenu->setProperty("mibAuto", on);
        m_markMenu->style()->unpolish(m_markMenu);
        m_markMenu->style()->polish(m_markMenu);
    }
}

/*****************************************************************************
 * Snapshot live DMX into the open scene
 *****************************************************************************/

void ProgrammerStrip::slotSnapshotLive()
{
    Workbench *bench = m_host.workbench ? m_host.workbench() : nullptr;
    Scene *scene = bench != nullptr ? qobject_cast<Scene *>(m_doc->function(bench->currentSceneId())) : nullptr;
    if (scene == nullptr)
    {
        QMessageBox::information(this, tr("Snapshot"),
            !m_host.noScene.isEmpty() ? m_host.noScene
                                      : tr("Select a scene in the Workbench first — the snapshot bakes into it."));
        return;
    }

    // Read the current pre-Grand-Master output for every patched fixture and
    // collect the values; apply AFTER releasing the universe lock (never run a
    // dialog while holding it). A fixture with all-zero output is skipped.
    QList<SceneValue> captured;
    int fixCount = 0;
    {
        QList<Universe *> universes = m_doc->inputOutputMap()->claimUniverses();
        foreach (Fixture *fxi, m_doc->fixtures())
        {
            if (fxi == nullptr || !fxi->isPatched())
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
    bench->sceneEditedOutside(tr("snapshot into %1").arg(scene->name()));
}

/*****************************************************************************
 * Save Positions
 *****************************************************************************/

void ProgrammerStrip::slotDesignPositionWritten()
{
    // New edits light the button and take back a pending confirm.
    m_saveArmed = false;
    m_saveBtn->setText(tr("Save Positions"));
    m_saveBtn->setEnabled(true);
}

void ProgrammerStrip::slotSavePositions()
{
    if (!m_saveArmed)
    {
        // First press: arm -- click again within 3 s to confirm.
        m_saveArmed = true;
        m_saveBtn->setText(tr("Confirm? (click again)"));
        QTimer::singleShot(3000, this, [this]() {
            if (m_saveArmed)
            {
                m_saveArmed = false;
                m_saveBtn->setText(tr("Save Positions"));
            }
        });
        return;
    }
    // Second press: confirmed -- commit the joystick position, then save.
    m_saveArmed = false;
    m_saveBtn->setText(tr("Save Positions"));
    m_saveBtn->setEnabled(false);
    if (ProgrammerController *pc = m_doc->programmer())
        pc->commitDesignJoystick();
    emit requestSave();
}

/*****************************************************************************
 * BPM / internal beat generator
 *****************************************************************************/

void ProgrammerStrip::slotBpmToggled(bool on)
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (ioMap == nullptr)
        return;
    m_bpmSpin->setEnabled(on);
    m_tapBtn->setEnabled(on);
    // Its own changes coming back are not news (the type's change sets the
    // engine's last tempo first, before this spin box's).
    m_settingBeat = true;
    if (on)
    {
        ioMap->setBeatGeneratorType(InputOutputMap::Internal);
        ioMap->setBpmNumber(m_bpmSpin->value());
    }
    else
        ioMap->setBeatGeneratorType(InputOutputMap::Disabled);
    m_settingBeat = false;
}

void ProgrammerStrip::slotBpmChanged(int bpm)
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (ioMap == nullptr || !m_bpmBtn->isChecked())
        return;
    m_settingBeat = true;
    ioMap->setBpmNumber(bpm);
    m_settingBeat = false;
}

void ProgrammerStrip::slotTapBeat()
{
    // The tempo from the last few taps, and the beat on this one -- every
    // effect on the clock lands on it (MasterTimer::tap()).
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (ioMap == nullptr || !m_bpmBtn->isChecked())
        return;
    const double bpm = ioMap->tapBeat();
    // Show the tapped tempo without setting it again: slotBpmChanged()
    // would restart the beat the tap set.
    {
        QSignalBlocker b(m_bpmSpin);
        m_bpmSpin->setValue(qRound(bpm));
    }
    updateBeatText();
}

void ProgrammerStrip::slotBeatFromEngine()
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    if (ioMap == nullptr || m_settingBeat)
        return;
    const bool on = ioMap->beatGeneratorType() == InputOutputMap::Internal;
    {
        QSignalBlocker b(m_bpmBtn);
        m_bpmBtn->setChecked(on);
    }
    m_bpmSpin->setEnabled(on);
    m_tapBtn->setEnabled(on);
    const int bpm = ioMap->bpmNumber();
    if (on && bpm >= m_bpmSpin->minimum() && bpm <= m_bpmSpin->maximum() && bpm != m_bpmSpin->value())
    {
        QSignalBlocker b(m_bpmSpin);
        m_bpmSpin->setValue(bpm);
    }
    updateBeatText();
}

void ProgrammerStrip::updateBeatText()
{
    m_beatMenu->setText(m_bpmBtn->isChecked() ? tr("Beat %1").arg(m_bpmSpin->value())
                                              : tr("Beat off"));
}

/*****************************************************************************
 * Followspot bindings
 *****************************************************************************/

void ProgrammerStrip::slotFollowSpotBindX()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    m_fsBindXBtn->setText(tr("Move X Axis…"));
    m_fsBindYBtn->setEnabled(false);
    pc->bindPanAxis();
}

void ProgrammerStrip::slotFollowSpotBindY()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    m_fsBindYBtn->setText(tr("Move Y Axis…"));
    m_fsBindXBtn->setEnabled(false);
    pc->bindTiltAxis();
}

void ProgrammerStrip::slotFollowSpotClearBindings()
{
    if (ProgrammerController *pc = m_doc->programmer())
        pc->clearAxisBindings();
}

void ProgrammerStrip::slotFollowSpotBindingChanged()
{
    ProgrammerController *pc = m_doc->programmer();
    if (pc == nullptr)
        return;
    m_fsXLabel->setText(pc->panAxisLabel());
    m_fsYLabel->setText(pc->tiltAxisLabel());
    /* Bound from a HID profile: the profile handles it, no manual Bind
       buttons. Shown only in manual mode. */
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

void ProgrammerStrip::slotJoystickUpdated(float pan, float tilt)
{
    // Only while the hidden panel would show it: no text churn per stick event.
    if (!m_followSpotPanel->isVisible())
        return;
    m_fsJoyLabel->setText(QString("Pan %1  Tilt %2")
                              .arg(double(pan), 4, 'f', 2)
                              .arg(double(tilt), 4, 'f', 2));
}
