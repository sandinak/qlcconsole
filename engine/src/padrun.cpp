/*
  Q Light Controller Plus
  padrun.cpp

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

#include <QTimer>
#include <QScopeGuard>
#include <climits>

#include "programmercontroller.h"
#include "controllerjobs.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include <atomic>
#include "showtreecontroller.h"
#include "refinesession.h"
#include "focussession.h"
#include "focuscallcue.h"
#include "outputrulekeeper.h"
#include "showcontrol.h"
#include "effectscriptrunner.h"
#include "functionparent.h"
#include "mastertimer.h"
#include "controlmap.h"
#include "chaser.h"
#include "function.h"
#include "padrun.h"
#include "doc.h"

PadRun::PadRun(Doc *doc, ProgrammerController *pc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_pc(pc)
{
    m_flash = new QTimer(this);
    m_flash->setSingleShot(true);
    m_flash->setInterval(wentFlashMs());
    m_chord = new QTimer(this);
    m_chord->setSingleShot(true);
    m_chord->setInterval(defaultRoleChordMs());
    connect(m_chord, &QTimer::timeout, this, &PadRun::roleChordFired);
    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    m_noticeTimer->setInterval(noticeMs());
    connect(m_noticeTimer, &QTimer::timeout, this, [this]() { showNotice(QString()); });
    m_blink = new QTimer(this);
    m_blink->setInterval(blinkMs());
    connect(m_blink, &QTimer::timeout, this, &PadRun::lightTick);
    // A finished one-shot hit lets go by itself, armed or not.
    m_oneShotTick = new QTimer(this);
    m_oneShotTick->setInterval(blinkMs());
    connect(m_oneShotTick, &QTimer::timeout, this, &PadRun::letGoFinishedOneShots);
    m_oneShotTick->start();
    hookShowControl();
    if (m_doc != nullptr)
    {
        /* The hit named before it fires: Operate only, and a hit going on
           or off (a VC button, the phone page) changes what a pull does.
           Queued: a function starts and stops on the DMX thread. */
        connect(m_doc, &Doc::modeChanged, this, &PadRun::syncHit);
        /* Into Operate, a walk beside the runner ends (Q252 is Design's;
           second review 1): the runner's GO is live again at once. */
        connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
            if (mode == Doc::Operate && m_walkBeside && isWalking())
            {
                const QString name = runnerName();
                endBesideWalk(name.isEmpty()
                              ? tr("Walk ended: the show is in Operate and another controller runs cues.")
                              : tr("Walk ended: the show is in Operate and %1 runs cues.").arg(name));
            }
        });
        if (MasterTimer *mt = m_doc->masterTimer())
        {
            connect(mt, &MasterTimer::functionStarted, this, &PadRun::syncHit, Qt::QueuedConnection);
            connect(mt, &MasterTimer::functionStopped, this, &PadRun::syncHit, Qt::QueuedConnection);
        }
    }
    if (m_pc != nullptr)
    {
        // A tool taking the pad, or letting it go: the screen says who has it.
        FocusSession *f = m_pc->focusSession();
        connect(f, &FocusSession::started, this, &PadRun::toolChanged);
        connect(f, &FocusSession::stopped, this, &PadRun::toolChanged);
        connect(m_pc->refineSession(), &RefineSession::started, this, &PadRun::toolChanged);
        connect(m_pc->refineSession(), &RefineSession::stopped, this, &PadRun::toolChanged);
        RefineSession *r = m_pc->refineSession();
        // Focus this cue ended (Menu, its B held, the laptop, a new show).
        connect(r, &RefineSession::stopped, this, [this]() {
            if (!m_restarting)
                walkEnded();
        });
        // The solo follows the layer walked.
        connect(r, &RefineSession::changed, this, &PadRun::syncSolo);
        // The walk's feel for the triggers follows Focus this cue.
        connect(r, &RefineSession::stopped, this, [this]() { updateTriggers(); });
    }
}

PadRun::~PadRun()
{
}

void PadRun::hookShowControl()
{
    /* Doc makes its ShowControl after the programmer, so this is also
       tried when the role comes on. (Stopped ends with the keeper's stop,
       stopEnded(), not here.) */
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st != nullptr && !m_showTreeHooked)
    {
        // The hit named on every surface follows the cue, the hits and their edits.
        m_showTreeHooked = true;
        connect(st, &ShowTreeController::hitArmingChanged, this, &PadRun::syncHit);
        connect(st, &ShowTreeController::changed, this, &PadRun::syncHit);
    }
    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    if (sc == nullptr || m_showControlHooked)
        return;
    m_showControlHooked = true;
    /* The double-GO guard held a command, from anywhere (ShowControl::run(),
       its rule 2): the pad says it as every surface does, in the same words
       -- one long buzz, and the voice. Not its notice: the footer's Next
       chip already shows the words, and the two together crowded the
       footer. In the Run role only: in Tools the pad isn't running the
       show. */
    connect(sc, &ShowControl::commandHeld, this, [this](const QString &text, quint32, ShowControl::Route) {
        m_heldNow = true;
        if (!m_enabled || m_pc == nullptr)
            return;
        m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
        m_pc->tellOperator(text, ControllerScheme::ConfirmPriority);
    });
}

/*****************************************************************************
 * The map
 *****************************************************************************/

PadRun::Action PadRun::actionFor(GamepadRole::Role role)
{
    switch (role)
    {
        // The bumpers are the mode only: together, the arm.
        case GamepadRole::LB:
        case GamepadRole::RB:           return Arm;
        case GamepadRole::LeftTrigger:  return Safety;
        case GamepadRole::A:            return Go;
        case GamepadRole::B:            return Back;
        case GamepadRole::X:            return SubGo;
        case GamepadRole::Y:            return SubBack;
        case GamepadRole::RightTrigger: return FireHit;
        case GamepadRole::RS:           return WhereAmI;
        case GamepadRole::Start:        return Walk;
        default:                        break;
    }
    return NoAction;
}

GamepadRole::Role PadRun::roleFor(Action action)
{
    // The arm is two controls; LB names it.
    if (action == Arm)
        return GamepadRole::LB;
    for (int r = GamepadRole::A; r <= GamepadRole::TouchClick; ++r)
        if (action != NoAction && actionFor(GamepadRole::Role(r)) == action)
            return GamepadRole::Role(r);
    return GamepadRole::None;
}

bool PadRun::needsArm(Action action)
{
    return action == Go || action == Back || action == SubGo || action == SubBack || action == FireHit;
}

bool PadRun::needsSafety(Action action)
{
    return action == Go || action == Back || action == SubGo || action == SubBack;
}

QString PadRun::label(Action action)
{
    switch (action)
    {
        case Arm:           return tr("{LB} + {RB} held: arm, and the same to disarm");
        case Safety:        return tr("The safety: hold for GO, Back, Sub GO and Sub Back");
        case Go:            return tr("GO (armed, {LT} held)");
        case Back:          return tr("Back (armed, {LT} held)");
        case SubGo:         return tr("Sub GO (armed, {LT} held)");
        case SubBack:       return tr("Sub Back (armed, {LT} held)");
        case FireHit:       return tr("Fire the armed hit (armed)");
        case WhereAmI:      return tr("Say the list and what's next");
        case Walk:          return tr("Walk the show: cues, layers, adjust, tag (not armed); "
                                      "armed, {LT} held: start the venue focus cue");
        case EmergencyStop: return emergencyStopLabel();
        case NoAction:      break;
    }
    return QString();
}

QString PadRun::emergencyStopLabel()
{
    return tr("Emergency stop, everything off: {LB} + {RB} + {LT} + {RT}");
}

QList<PadRun::Action> PadRun::actions()
{
    return { Arm, Safety, Go, Back, SubGo, SubBack, FireHit, WhereAmI, Walk };
}

QStringList PadRun::mapLines(PadNames::Family family)
{
    QStringList out;
    for (Action a : actions())
        out << QStringLiteral("%1: %2").arg(PadNames::name(roleFor(a), family),
                                            PadNames::expand(label(a), family));
    out << PadNames::expand(emergencyStopLabel(), family);
    return out;
}

ShowControl::Command PadRun::commandFor(Action action)
{
    switch (action)
    {
        case Back:          return ShowControl::Back;
        case SubGo:         return ShowControl::SubGo;
        case SubBack:       return ShowControl::SubBack;
        case EmergencyStop: return ShowControl::AllOff;
        default:            break;
    }
    return ShowControl::Go;
}

/*****************************************************************************
 * The role
 *****************************************************************************/

void PadRun::setEnabled(bool on, bool say)
{
    if (on == m_enabled)
        return;
    hookShowControl();
    // A walk beside the runner is the Run role's (second review 6).
    if (m_walkBeside && isWalking())
        endBesideWalk(tr("Walk ended: the controller role changed."));
    clear(false);       // the four were not read in Tools
    // Red stays while the keeper still holds the stop (one source).
    if (!showHeld())
        m_stopped = false;
    m_enabled = on;
    // Switched: the words saying where it is switched have done their job.
    if (m_notice == roleSwitchText())
        showNotice(QString());
    emit enabledChanged(on);
    sync();
    if (say && m_pc != nullptr)
        m_pc->tellOperator(on ? (padRunSupported() ? m_pc->padText(tr("Run role. Hold {LB} and {RB} to arm."))
                                                   : notSupportedText())
                              : tr("Run role off."),
                           ControllerScheme::ConfirmPriority);
}

void PadRun::refuse(Action action, const QString &text)
{
    emit refused(action);
    if (m_pc == nullptr)
        return;
    const QString said = m_pc->padText(text);
    m_pc->tellOperator(said, ControllerScheme::ConfirmPriority);
    // On the screen too: with lights-only feedback nothing is said.
    showNotice(said);
}

bool PadRun::handle(quint32 universe, GamepadRole::Role role, uchar value)
{
    if (!m_enabled || role == GamepadRole::None)
        return false;
    // The sticks stay the follow spot's; the touchpad is left alone.
    if (GamepadRole::isTouch(role)
        || role == GamepadRole::LeftX || role == GamepadRole::LeftY
        || role == GamepadRole::RightX || role == GamepadRole::RightY)
        return false;

    // Each line's pad worked out once for this event.
    const bool padIdsWere = m_padIdsOn;
    beginPadIds();
    const auto padIds = qScopeGuard([this, padIdsWere]() { endPadIds(padIdsWere); });

    const bool down = value > 0;
    const bool chordControl = role == GamepadRole::LB || role == GamepadRole::RB
                              || role == GamepadRole::LeftTrigger || role == GamepadRole::RightTrigger;
    bool rtPressed = false;
    /* The four were read first, by watchStop() (the stop is checked before
       anything else): this event's reading is taken from there, never made
       twice (a trigger's press is an edge). */
    const bool noted = m_noted.valid && m_noted.universe == universe
                       && m_noted.role == int(role) && m_noted.value == value;
    m_noted.valid = false;
    if (chordControl)
    {
        if (noted)
            rtPressed = m_noted.rtPressed;
        else
            rtPressed = noteHeld(universe, role, value);
    }
    else if (role != GamepadRole::None)
    {
        /* A button already down on the pad's other line (one pad seen on
           two universes): the pad's press was acted on there -- one
           press, one action. One line: never. */
        const bool elsewhere = down && downElsewhere(universe, role);
        Line &line = m_held[universe];
        if (down)
            line.buttons.insert(int(role));
        else
            line.buttons.remove(int(role));
        if (elsewhere)
            return true;
    }
    /* What the pad holds, every line of it (one pad seen on two universes
       is one pad): the chord, the stop, the safety read from here. */
    const QString pad = padId(universe);
    const Held h = padHeld(pad);

    if (chordControl)
    {
        /* All four: the emergency stop, armed or not, once a chord. */
        if (h.all())
        {
            if (!m_pads[pad].stopFired)
            {
                m_pads[pad].stopFired = true;
                emergencyStop();
            }
            return true;
        }
        m_pads[pad].stopFired = false;
        /* No Run cues job (docs/design/CONTROLLER_JOBS.md, 5): the arm
           chord is never timed and the hit never fires; each fresh try is
           told why. The stop above is every pad's. */
        if (!hasJob(universe, ControllerJobs::RunCues))
        {
            const bool chordNow = down && (role == GamepadRole::LB || role == GamepadRole::RB)
                                  && h.lb && h.rb && !h.anyTrigger();
            const bool hitNow = rtPressed && !h.lb && !h.rb && !h.lt.isDown();
            if (chordNow || hitNow)
                refuse(chordNow ? Arm : FireHit, noRunCuesText());
            return true;
        }
        /* Both bumpers and no trigger, held: the arm changes. Anything
           else of the four ends the timing. */
        armChordUpdate(universe, down && (role == GamepadRole::LB || role == GamepadRole::RB));
        /* RT's pull on its own is the hit (below) -- not with a bumper or
           LT down: LT + RT together is the stop in the making, never a
           hit. The rest of the four is done. */
        if (!rtPressed || h.lb || h.rb || h.lt.isDown())
            return true;
    }

    const Action action = actionFor(role);
    // Everything else acts on the press; the rest of the map is swallowed.
    if (!down || action == NoAction)
        return true;

    /* The jobs (CONTROLLER_JOBS.md 5 and 8): Menu is the walk or the venue
       focus -- Focus's; the rest of the map is Run cues'. */
    if (action == Walk)
    {
        if (!hasJob(universe, ControllerJobs::Focus))
        {
            refuse(action, noFocusText());
            return true;
        }
    }
    else if (!hasJob(universe, ControllerJobs::RunCues))
    {
        refuse(action, noRunCuesText());
        return true;
    }

    /* Rule 6: a venue focus begun (the venue being named; once the calls
       run the focus tool has the pad) -- no cue moves and no hit fires
       from here, and only LT + Menu ends it. RS still says where. */
    if (isFocusing())
    {
        if (action == Walk)
            focusMenu(universe);
        else if (action == WhereAmI)
            trigger(action);
        else if (isBesideFocus(universe))
        {
            /* Another pad's venue focus (board Q250): this pad's arm stays,
               and nothing new plays from it until the focus ends. A hit it
               left on is its own to let go: that pull starts nothing. */
            bool letsGo = false;
            if (action == FireHit && m_armed && sameController(universe, m_armUniverse)
                && nextHit(&letsGo) != ShowTree::invalidId() && letsGo)
                trigger(action);
            else
            {
                m_besideNotice = m_pc != nullptr ? m_pc->padText(focusBesideText()) : focusBesideText();
                refuse(action, focusBesideText());
            }
        }
        else if (m_pc != nullptr)
            showNotice(m_pc->padText(tr("Venue focus: nothing plays from the controller. "
                                        "{LT} + {Menu} finishes it.")));
        return true;
    }
    /* Another pad walks beside the runner (Q252): as beside a venue focus,
       nothing new plays from here until the walk ends; a hit left on is
       the runner's to let go. */
    if (isBesideWalk(universe))
    {
        bool letsGo = false;
        if (action == WhereAmI)
            trigger(action);
        else if (action == FireHit && m_armed && sameController(universe, m_armUniverse)
                 && nextHit(&letsGo) != ShowTree::invalidId() && letsGo)
            trigger(action);
        else
        {
            m_besideNotice = m_pc != nullptr ? m_pc->padText(walkBesideText()) : walkBesideText();
            refuse(action, walkBesideText());
        }
        return true;
    }
    // A venue focus cue waits: Menu is its start, armed and with LT held.
    if (action == Walk && isFocusCueWaiting())
    {
        focusStartPress(universe, h.lt.isDown());
        return true;
    }

    /* Menu walks. Another pad armed (board Q252): in Design the walk
       goes beside it, the runner keeping its arm; in Operate it waits,
       told who runs cues -- never "disarm first" to a pad that has no arm
       to drop. */
    if (action == Walk && m_armed && !sameController(universe, m_armUniverse))
    {
        if (m_doc != nullptr && m_doc->mode() == Doc::Operate)
        {
            const QString name = runnerName();
            refuse(action, name.isEmpty()
                   ? tr("Another controller is running cues: in Operate the walk waits until it disarms.")
                   : tr("%1 is running cues: in Operate the walk waits until it disarms.").arg(name));
            return true;
        }
        startWalkBeside(universe);
        return true;
    }

    if (needsArm(action))
    {
        if (!m_armed)
        {
            refuse(action, padRunSupported() ? tr("Not armed: hold {LB} and {RB}.") : notSupportedText());
            return true;
        }
        if (!sameController(universe, m_armUniverse))
        {
            refuse(action, runningCuesText());
            return true;
        }
        // The safety: LT held, for every cue command.
        if (needsSafety(action) && !h.lt.isDown())
        {
            switch (action)
            {
                case Go:        refuse(action, tr("GO is {LT} held and {A}.")); break;
                case Back:      refuse(action, tr("Back is {LT} held and {B}.")); break;
                case SubGo:     refuse(action, tr("Sub GO is {LT} held and {X}.")); break;
                default:        refuse(action, tr("Sub Back is {LT} held and {Y}.")); break;
            }
            return true;
        }
    }
    /* Menu walks: the walker owns the walk's tool from its start (Q135),
       as beside the runner -- so its sticks are the walk's and another
       pad's follow spot steers on beside it (board Q253). */
    if (action == Walk && m_pc != nullptr && !isWalking())
    {
        m_pc->claimController(universe);
        if (!trigger(action))
            m_pc->releaseControllerNow();
        return true;
    }
    trigger(action);
    return true;
}

/*****************************************************************************
 * The arm
 *****************************************************************************/

void PadRun::setArmChordMs(int ms)
{
    m_armChordMs = ms;
    for (const Pad &p : std::as_const(m_pads))
        if (p.chord != nullptr)
            p.chord->setInterval(ms);
}

bool PadRun::isArmChordPending() const
{
    for (const Pad &p : m_pads)
        if (p.chord != nullptr && p.chord->isActive())
            return true;
    return false;
}

void PadRun::stopArmChords()
{
    for (const Pad &p : std::as_const(m_pads))
        if (p.chord != nullptr)
            p.chord->stop();
}

void PadRun::armChordUpdate(quint32 universe, bool bumperDown)
{
    const QString pad = padId(universe);
    const Held h = padHeld(pad);
    // In FOCUS the bumpers are the focus tool's: no arm change (rule 6).
    const bool chord = h.lb && h.rb && !h.anyTrigger() && !h.spent && !isFocusing() && !isWalking();
    /* Each pad its own timing (CONTROLLER_JOBS.md 6): another pad's chord,
       squeezed or let go, never takes or stops this one's. */
    QTimer *&t = m_pads[pad].chord;
    if (chord)
    {
        if (t == nullptr)
        {
            t = new QTimer(this);
            t->setSingleShot(true);
            t->setInterval(m_armChordMs);
            connect(t, &QTimer::timeout, this, [this, pad]() { armChordFired(pad); });
        }
        // Timed from the second bumper down; a held chord is not restarted.
        if (!t->isActive())
            t->start();
    }
    else if (t != nullptr)
        t->stop();
    /* The runner's chord while another pad has the venue focus: its arm
       stays as it is until the focus ends, and the screen says why. */
    if (h.lb && h.rb && !h.anyTrigger() && !h.spent && isBesideTool(universe) && m_pc != nullptr)
    {
        m_besideNotice = isBesideFocus(universe) ? focusBesideText() : walkBesideText();
        showNotice(m_besideNotice);
    }
    /* Both bumpers with one trigger down: no chord, and the screen says
       why -- a trigger that does not come back up would otherwise make a
       pad that "will not arm" with nothing to show for it. */
    if (h.lb && h.rb && h.anyTrigger() && !h.all() && m_pc != nullptr)
        showNotice(!padRunSupported() ? notSupportedText()
                                      : m_pc->padText(tr("To arm, let go of everything, then hold {LB} and {RB}.")));
    /* Both bumpers down again, no trigger, but the chord is spent (it
       fired, was refused, or a trigger came with them; or a tool took the
       pad mid-chord): one bumper pressed again to try once more times
       nothing -- the screen says what does. */
    else if (bumperDown && h.lb && h.rb && !h.anyTrigger() && h.spent && !isFocusing() && !isWalking()
             && !isBesideTool(universe) && m_pc != nullptr)
        showNotice(!padRunSupported() ? notSupportedText()
                                      : m_pc->padText(tr("Let go of both bumpers, then hold {LB} and {RB} again.")));
}

void PadRun::armChordFired(const QString &pad)
{
    const Held h = padHeld(pad);
    if (!m_enabled || !h.lb || !h.rb || h.anyTrigger() || h.spent || isFocusing() || isWalking())
        return;
    // The pad's line holding a bumper now: the one the arm is made on.
    quint32 u = UINT_MAX;
    for (auto it = m_held.constBegin(); it != m_held.constEnd(); ++it)
        if ((it.value().lb || it.value().rb) && padId(it.key()) == pad && it.key() < u)
            u = it.key();
    if (u == UINT_MAX)
        return;
    /* Rule 3: one chord, one change. Decided (fired or refused), it is
       spent until both bumpers are up -- on every line of the pad -- so
       nothing that comes in while they stay held (a DualSense's resting
       trigger noise) times it again (pad-trace replay, 2026-11-16). */
    m_pads[pad].spent = true;
    if (!m_armed && !padRunSupported())
    {
        refuse(Arm, notSupportedText());
        return;
    }
    /* One runner (CONTROLLER_JOBS.md 6): another pad's arm -- or the arm a
       venue focus put by -- is not this one's to drop or share. */
    if (m_armed && !sameController(u, m_armUniverse))
    {
        refuse(Arm, runningCuesText());
        return;
    }
    if (!m_armed && m_focusArm != UINT_MAX && !sameController(u, m_focusArm))
    {
        refuse(Arm, runningCuesText());
        return;
    }
    if (!hasJob(u, ControllerJobs::RunCues))
    {
        refuse(Arm, noRunCuesText());
        return;
    }
    setArmed(!m_armed, m_armed ? m_armUniverse : u);
}

namespace
{
std::atomic<int> g_platform { int(PadRun::PlatformAuto) };
}

void PadRun::setPlatformForTest(Platform platform)
{
    g_platform = int(platform);
}

bool PadRun::padRunSupported()
{
    switch (Platform(g_platform.load()))
    {
    case PlatformWindows:
        return false;
    case PlatformOther:
        return true;
    case PlatformAuto:
    {
        // Test-only, read once: honoured only where the test guard ran.
        static const bool testAny = qEnvironmentVariable("QLC_TEST_OFFSCREEN_GUARD") == QLatin1String("1")
                                    && qEnvironmentVariable("QLC_PADRUN_PLATFORM") == QLatin1String("any");
        if (testAny)
            return true;
        break;
    }
    case PlatformReal:
        break;
    }
#ifdef Q_OS_WIN
    return false;
#else
    return true;
#endif
}

QString PadRun::notSupportedText()
{
    return tr("Running the show from a controller is macOS/Linux for now.");
}

bool PadRun::setArmed(bool on, quint32 universe)
{
    if (on == m_armed)
        return true;
    // Never the stop: only the arm is refused here (board Q257).
    if (on && !padRunSupported())
    {
        refuse(Arm, notSupportedText());
        return false;
    }
    // Disarmed: the pad that armed is the one told.
    if (!on)
        universe = m_armUniverse;
    if (on && (!m_enabled || isWalking() || !holder().isEmpty() || isFocusing()))
        return false;
    m_armed = on;
    m_armUniverse = on ? universe : UINT_MAX;
    if (on)
        m_stopped = false;
    m_playing = false;
    m_blinkOn = false;
    m_flash->stop();
    emit armedChanged(on);
    sync();
    if (m_pc != nullptr)
    {
        // A pad with no light is told by its motors: twice armed, once long not.
        if (on)
            m_pc->padRumble(2, ControllerScheme::rumbleShortMs(), universe);
        else
            m_pc->padRumble(1, ControllerScheme::rumbleLongMs(), universe);
        // Armed after the stop, the show still held: said, not plain ARMED.
        m_pc->tellOperator(!on ? tr("Disarmed.")
                               : showHeld() ? m_pc->padText(tr("Armed. The show is held from the emergency stop: "
                                                               "{LT} and {A} brings it back."))
                                            : m_pc->padText(tr("Armed. {LT} and {A} is GO.")),
                           ControllerScheme::ConfirmPriority);
    }
    return true;
}

void PadRun::emergencyStop()
{
    hookShowControl();
    /* Stopped first (stopStruck()), then the ALL OFF: the stop's own
       emergencyStopStruck() comes back here during run() and is the same
       stop, struck once. */
    // A stop on top of a stop is said once (review 0.4.13).
    const bool already = m_stopped;
    m_stopping = true;
    m_struckNow = false;
    stopStruck();
    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    bool done = false;
    if (sc != nullptr)
    {
        ShowControl::Request r;
        r.command = ShowControl::AllOff;
        done = sc->run(r, ShowControl::RoutePad);
    }
    m_stopping = false;
    m_struckNow = false;
    emit commandSent(ShowControl::AllOff, done);
    if (!already)
        sayStopped();
}

void PadRun::stopStruck()
{
    // The pad's own stop: struck already, before its ALL OFF went.
    if (m_stopping && m_struckNow)
        return;
    if (m_stopping)
        m_struckNow = true;
    /* Another route's, queued here: over already by the time it lands (a
       GO, a new session, another show loaded) -- the arm still drops and
       the tools still end, but the show is not painted red that its footer
       does not hold (one source). */
    const OutputRuleKeeper *keeper = m_doc != nullptr ? m_doc->outputRuleKeeper() : nullptr;
    const bool over = !m_stopping && keeper != nullptr && !keeper->emergencyStopped();
    const bool already = m_stopped;
    /* One stopped state, whatever route the stop came by (rule 2): the arm
       drops, a focus's put-by arm is forgotten, the state is Stopped -- all
       before a tool ends below, so nothing its end does re-arms the pad
       or says GO works again. */
    stopArmChords();
    const bool was = m_armed;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    m_focusArm = UINT_MAX;
    if (!over)
        m_stopped = true;
    m_playing = false;
    m_flash->stop();
    if (was)
        emit armedChanged(false);
    // E9: the focus tools end with it, quietly (the stop speaks for itself).
    if (m_pc != nullptr)
    {
        m_endingForStop = true;
        m_pc->focusCallCue()->cut();
        if (m_pc->focusSession()->isActive())
            m_pc->focusSession()->stop();
        if (m_pc->refineSession()->isActive())
            m_pc->refineSession()->stop();
        m_endingForStop = false;
    }
    sync();
    // From another route (OSC, MSC, the phone, the laptop): every pad told
    // too -- once, not again on top of a stop.
    if (!m_stopping && m_enabled && !over && !already)
        sayStopped();
}

void PadRun::stopEnded()
{
    /* The keeper's stop is over (OutputRuleKeeper::emergencyStopEnded()):
       a GO that really drives the show again (not a timecode one, not a
       Load or Reset), a new session, another show. The red goes with the
       footer's "held: emergency stop" -- one source (rule 2). An arm by
       the chord ends it on the pad as before. */
    if (m_stopping)
        return;
    /* A late one: a GO's "ended" queued from a plugin's thread, landing
       after a new stop was struck here. The keeper holds the new one. */
    if (showHeld())
        return;
    if (!m_stopped && !m_armed)
        return;
    m_stopped = false;
    m_shownState = -1;      // an arm's "held" words go with the hold
    sync();
}

bool PadRun::showHeld() const
{
    const OutputRuleKeeper *keeper = m_doc != nullptr ? m_doc->outputRuleKeeper() : nullptr;
    return keeper != nullptr && keeper->emergencyStopped();
}

void PadRun::sayStopped()
{
    if (m_pc == nullptr)
        return;
    // Every pad buzzes: the stop is everyone's (review 1).
    m_pc->padRumble(3, ControllerScheme::rumbleLongMs(), ProgrammerController::kAllPads);
    const QString said = tr("Emergency stop. Everything is off.");
    m_pc->tellOperator(said, ControllerScheme::ConfirmPriority);
    showNotice(said);
}

/*****************************************************************************
 * The state
 *****************************************************************************/

PadRun::State PadRun::state() const
{
    if (!m_enabled)
        return Tools;
    // Red after the emergency stop (rule 6), whatever has the pad since.
    if (m_stopped)
        return Stopped;
    // A venue focus begun, the pad here (rule 6); with no pad, never FOCUS.
    if (isFocusing() && !m_padGone)
        return Focus;
    if (isWalking())
        return Walking;
    if (!holder().isEmpty())
        return ToolHasPad;
    return m_armed ? Armed : Run;
}

QColor PadRun::stateColour() const
{
    switch (state())
    {
        case Armed:     return armedColour();
        case Stopped:   return stoppedColour();
        case Focus:     return focusColour();
        default:        break;
    }
    return QColor();
}

QColor PadRun::stateInk() const
{
    const QColor c = stateColour();
    if (!c.isValid())
        return QColor();
    return c == stoppedColour() ? QColor(Qt::white) : QColor(Qt::black);
}

QColor PadRun::lightColour() const
{
    switch (state())
    {
        case Armed:     return m_playing && m_blinkOn ? playingColour() : armedColour();
        case Stopped:   return stoppedColour();
        case Focus:     return focusColour();
        default:        break;
    }
    return QColor();
}

void PadRun::setTesting(bool on)
{
    if (on == m_testing)
        return;
    m_testing = on;
    roleChordCancel();
    stopArmChords();
    forgetHeld();       // what is held across the change was not the Run map's (but the four)
    emit testingChanged(on);
    toolChanged();
}

void PadRun::toolChanged()
{
    /* A venue focus cue's calls, started on the laptop while armed: FOCUS,
       which is not disarmed (rule 6) -- the arm is put by for the end. */
    if (m_armed && isFocusing() && !isBesideFocus(m_armUniverse))
        putArmBy();
    /* A tool started on the laptop takes the pad: the arm drops with it
       (no button did that), so it is not found armed when the tool ends.
       Not the venue focus a runner keeps its arm beside (Q250) -- but any
       other tool, whoever started the focus (review 2, 9). */
    const QString tool = (isFocusing() || m_walkBeside) ? toolOverFocus() : holder();
    if (m_armed && !tool.isEmpty())
    {
        reset();
        showNotice(tr("Disarmed: %1 has the controller.").arg(tool));
    }
    /* What was held when the pad changed hands (either way) is not known
       any more: the tool hears the releases, the Run map does not. A
       chord begun before is no chord after. */
    const QString now = holder();
    if (now != m_lastHolder)
    {
        m_lastHolder = now;
        stopArmChords();
        forgetHeld();
    }
    emit holderChanged();
    m_shownState = -1;      // the tool's name is part of what is shown
    sync();
}

void PadRun::sync()
{
    const State now = state();
    // The blink runs only while there is an arm to show it on.
    if (now == Armed)
    {
        if (!m_blink->isActive())
            m_blink->start();
    }
    else
    {
        m_blink->stop();
        m_playing = false;
        m_blinkOn = false;
    }
    updateTriggers();
    showLight();
    /* The words too: the runner's name comes and goes with a second pad
       (not on the first sync, which had none to compare with). */
    const QString text = roleText();
    const bool renamed = !m_shownText.isEmpty() && text != m_shownText;
    m_shownText = text;
    if (int(now) != m_shownState || renamed)
    {
        m_shownState = int(now);
        emit stateChanged();
    }
    syncHit();
}

void PadRun::letGoFinishedOneShots()
{
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    EffectScriptRunner *esr = m_doc != nullptr ? m_doc->effectScriptRunner() : nullptr;
    if (st == nullptr || esr == nullptr || m_doc->mode() != Doc::Operate)
        return;
    bool any = false;
    for (quint32 h : allHits())
    {
        Function *f = m_doc->function(st->lookFunction(h));
        if (f == nullptr || !f->isRunning() || esr->oneShotState(f->id()) != 1)
            continue;
        // Its run is over: stop it, and the cue's own levels come back.
        f->stop(FunctionParent::master());
        any = true;
    }
    if (any)
        syncHit();
}

void PadRun::syncHit()
{
    hookShowControl();
    // A new set of armed hits (a new cue, a list change) starts at 1 of n.
    const QList<quint32> armedNow = armedHits();
    if (armedNow != m_armedSet)
    {
        m_armedSet = armedNow;
        m_hitStep = 0;
    }
    const QString hit = hitText();
    const QString armed = state() == Armed ? hit : QString();
    if (hit == m_shownHit && armed == m_shownArmedHit)
        return;
    m_shownHit = hit;
    m_shownArmedHit = armed;
    emit hitChanged();
}

void PadRun::lightTick()
{
    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    const quint32 at = listChaser();
    const Chaser *chaser = sc == nullptr ? nullptr
                           : at != UINT_MAX ? qobject_cast<Chaser *>(m_doc->function(at)) : sc->cueList(QString());
    const bool playing = m_flash->isActive() || ShowControl::fadeProgress(chaser) >= 0;
    m_blinkOn = playing ? !m_blinkOn : false;
    m_playing = playing;
    showLight();
    syncHit();
}

QString PadRun::holder() const
{
    if (m_pc == nullptr || isWalking())
        return QString();
    const FocusSession *f = m_pc->focusSession();
    if (f->isActive())
        return f->isRunningCalls() ? tr("Venue Focus") : tr("Quick Focus");
    if (m_testing && !m_pc->refineSession()->isActive())
        return tr("Controller Test");
    const RefineSession *r = m_pc->refineSession();
    if (r->isActive())
        return r->isSceneMode() ? tr("Focus This Look") : tr("Focus This Cue");
    return QString();
}

QString PadRun::roleText() const
{
    switch (state())
    {
        case Tools:         return tr("Pad: Tools");
        case Run:           return tr("Pad: Run");
        case Armed:
        {
            /* Armed after the stop while the footer still holds the show:
               never a plain ARMED beside "held" (one source, rule 2). */
            const QString held = showHeld() ? tr(" \u00B7 held: LT + A brings the show back") : QString();
            // Two pads or more: which one runs cues (CONTROLLER_JOBS.md 9).
            if (padCount() > 1 && !runnerName().isEmpty())
                return tr("Pad: ARMED \u00B7 %1").arg(runnerName()) + held;
            return tr("Pad: ARMED") + held;
        }
        case Stopped:       return tr("Pad: STOPPED");
        case Walking:
            // Beside the runner (Q252), with more than one pad: who walks.
            if (m_walkBeside && padCount() > 1 && m_pc != nullptr && m_pc->controllerJobs() != nullptr
                && !m_pc->controllerJobs()->nameOfUniverse(m_walker).isEmpty())
                return tr("Pad: Walk \u00B7 %1").arg(m_pc->controllerJobs()->nameOfUniverse(m_walker));
            return tr("Pad: Walk");
        case ToolHasPad:    return tr("Pad: %1").arg(holder());
        case Focus:
            // The pad the focus is on, not the runner (CONTROLLER_JOBS.md 11).
            if (padCount() > 1 && !focusOwnerName().isEmpty())
                return tr("Pad: FOCUS \u00B7 %1").arg(focusOwnerName());
            return tr("Pad: FOCUS");
    }
    return QString();
}

QString PadRun::helpHtml() const
{
    if (m_pc == nullptr)
        return QString();
    const PadNames::Family fam = m_pc->padFamily();
    auto row = [](const QString &a, const QString &b) {
        return QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(a.toHtmlEscaped(), b.toHtmlEscaped());
    };
    QString pad = m_pc->followSpotStickName();
    QString out = QStringLiteral("<p><b>%1</b>%2</p>")
                      .arg(roleText().toHtmlEscaped(),
                           pad.isEmpty() ? QString() : QStringLiteral(" · ") + pad.toHtmlEscaped());
    const QString tool = m_enabled ? holder() : QString();
    if (state() == Focus)
        out += tr("<p><b>Venue focus has the controller.</b> Hold %1 and press %2 to finish it "
                  "(or finish it on the laptop); the list stays on the focus cue until you move "
                  "it on. Nothing plays from the controller meanwhile; the emergency stop still "
                  "works.</p>")
                   .arg(m_pc->padText(QStringLiteral("{LT}")).toHtmlEscaped(),
                        m_pc->padText(QStringLiteral("{Menu}")).toHtmlEscaped());
    else if (!tool.isEmpty())
        out += tr("<p><b>%1 has the controller</b> until it ends (finish or cancel it on the laptop); "
                  "then the controller runs the show again.</p>").arg(tool.toHtmlEscaped());
    if (!m_enabled)
        out += tr("<p>Tools: Focus Mode, Venue Focus, Focus This Cue and the follow spot. Click the "
                  "Run tab's Pad: button to run the show from the controller.</p>");
    out += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"2\"><tr><th align=left>%1</th>"
                          "<th align=left>%2</th></tr>")
               .arg(tr("Control"), tr("What It Does"));
    for (Action a : actions())
        out += row(PadNames::name(roleFor(a), fam), PadNames::expand(label(a), fam));
    out += QStringLiteral("<tr><td colspan=2><b>%1</b></td></tr>")
               .arg(PadNames::expand(emergencyStopLabel(), fam).toHtmlEscaped());
    out += QStringLiteral("<tr><td colspan=2>%1</td></tr>")
               .arg(tr("GO plays the list the Run tab plays: pick it there, never on the controller.").toHtmlEscaped());
    out += row(tr("Sticks, D-pad"), tr("The follow spot, when one plays (D-pad up / down: speed, left / right: beam size); "
                                       "otherwise nothing"));
    out += row(m_pc->padText(QStringLiteral("{View}")) + QStringLiteral(" + ") + PadNames::name(GamepadRole::LS, fam),
               tr("Centre Sticks"));
    out += row(m_pc->padText(QStringLiteral("{View}")) + QStringLiteral(" + ")
                   + m_pc->padText(QStringLiteral("{Menu}")),
               tr("In Tools, a tap: Focus mode. Run / Tools is switched on the computer "
                  "(the Run tab's Pad: button), never on the controller"));
    out += QStringLiteral("</table>");
    out += tr("<p><b>Walking</b> (%1 in Run):</p><table cellspacing=\"0\" cellpadding=\"2\">")
               .arg(PadNames::name(roleFor(Walk), fam).toHtmlEscaped());
    for (WalkAction a : walkActions())
        out += row(PadNames::name(roleFor(a), fam), PadNames::expand(label(a), fam));
    const QStringList extra = walkMapLines(fam).mid(walkActions().size());
    for (const QString &l : extra)
        out += QStringLiteral("<tr><td colspan=2>%1</td></tr>").arg(l.toHtmlEscaped());
    out += QStringLiteral("</table>");
    return out;
}

void PadRun::showNotice(const QString &text)
{
    if (text.isEmpty())
        m_noticeTimer->stop();
    else
        m_noticeTimer->start();
    if (text == m_notice)
        return;
    m_notice = text;
    emit noticeChanged();
}

void PadRun::reset()
{
    clear();
    sync();
}

void PadRun::clear(bool four)
{
    roleChordCancel();
    stopArmChords();
    if (four)
        forgetHeld();
    else
    {
        m_held.clear();
        for (Pad &p : m_pads)
        {
            p.stopFired = false;
            p.spent = false;
        }
    }
    m_swipe.reset();
    m_flash->stop();
    const bool was = m_armed;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    m_focusArm = UINT_MAX;
    if (was)
        emit armedChanged(false);
}

void PadRun::controllerLost()
{
    // Armed, or in FOCUS with an arm to go back to: either way, not armed now.
    const bool was = m_armed || m_focusArm != UINT_MAX;
    reset();
    if (was)
        showNotice(tr("Disarmed: the controller went away."));
}

void PadRun::forgetHeld()
{
    /* The other buttons go: their releases went to whatever had the pad
       (a walk, a tool, the test), and one left down would swallow the
       next press of it on the pad's other line. */
    for (auto it = m_held.begin(); it != m_held.end(); ++it)
        it.value().buttons.clear();
    // The four's bits stay; a chord begun before is no chord after.
    for (auto it = m_held.constBegin(); it != m_held.constEnd(); ++it)
    {
        const QString pad = padId(it.key());
        const Held h = padHeld(pad);
        m_pads[pad].spent = h.lb || h.rb;
    }
    for (auto it = m_pads.begin(); it != m_pads.end(); ++it)
    {
        const Held h = padHeld(it.key());
        if (!h.lb && !h.rb)
            it.value().spent = false;
    }
}

QString PadRun::toolOverFocus() const
{
    if (m_pc == nullptr)
        return QString();
    const RefineSession *r = m_pc->refineSession();
    if (r->isActive() && !isWalking() && !m_walkBeside)
        return r->isSceneMode() ? tr("Focus This Look") : tr("Focus This Cue");
    if (m_testing)
        return tr("Controller Test");
    const FocusSession *f = m_pc->focusSession();
    if (f->isActive() && !f->isRunningCalls())
        return tr("Quick Focus");
    return QString();
}

bool PadRun::noteHeld(quint32 universe, GamepadRole::Role role, uchar value)
{
    /* Each line keeps its own reading (its trigger hysteresis); the pad's
       is the lines OR'd (padHeld()). */
    Line &line = m_held[universe];
    bool rtPressed = false;
    switch (role)
    {
        case GamepadRole::LB:           line.lb = value > 0; break;
        case GamepadRole::RB:           line.rb = value > 0; break;
        case GamepadRole::LeftTrigger:  line.lt.update(value / 255.0f); break;
        case GamepadRole::RightTrigger:
            /* RT's press is the pad's: not when another line of it has RT
               down already (one pull, one hit). */
            rtPressed = line.rt.update(value / 255.0f) && !downElsewhere(universe, role);
            break;
        default:                        break;
    }
    /* A trigger with both bumpers is the stop (or on its way): no arm
       chord again until both bumpers are up -- letting go of the triggers
       first must not arm. The pad's bumpers and triggers, every line. */
    const QString pad = padId(universe);
    const Held h = padHeld(pad);
    if (h.lb && h.rb && h.anyTrigger())
        m_pads[pad].spent = true;
    else if (!h.lb && !h.rb)
        m_pads[pad].spent = false;
    return rtPressed;
}

bool PadRun::watchStop(quint32 universe, GamepadRole::Role role, uchar value)
{
    m_noted.valid = false;
    /* The Run role's stop (the Tools role has none, CONTROLLER_JOBS.md 4).
       The Controller Test open too: it shows the presses, and the stop
       still stops (review 0.4.10, M2). */
    if (!m_enabled)
        return false;
    if (role != GamepadRole::LB && role != GamepadRole::RB
        && role != GamepadRole::LeftTrigger && role != GamepadRole::RightTrigger)
        return false;
    const bool padIdsWere = m_padIdsOn;
    beginPadIds();
    const auto padIds = qScopeGuard([this, padIdsWere]() { endPadIds(padIdsWere); });
    m_noted.rtPressed = noteHeld(universe, role, value);
    m_noted.universe = universe;
    m_noted.role = int(role);
    m_noted.value = value;
    m_noted.valid = true;
    /* The pad's four: down on any of its lines. One pad listed twice
       sends the four on both: one stop a press (L1). */
    const QString pad = padId(universe);
    if (!padHeld(pad).all())
    {
        m_pads[pad].stopFired = false;
        return false;
    }
    if (!m_pads[pad].stopFired)
    {
        m_pads[pad].stopFired = true;
        emergencyStop();
    }
    return true;
}

quint32 PadRun::lineOfPad(const QString &pad, quint32 except) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    // "#n": a pad told by its universe alone has no other line.
    if (iom == nullptr || m_pc == nullptr || pad.isEmpty() || pad.startsWith(QLatin1Char('#')))
        return UINT_MAX;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (u != except && ip != nullptr && ip->isPatched() && m_pc->isPadUniverse(u) && padId(u) == pad)
            return u;
    }
    return UINT_MAX;
}

void PadRun::forgetPad(quint32 universe)
{
    /* The walker beside the runner went (second review 2) -- unless only
       that line of a pad listed twice went: the walk goes on on its twin
       line, as the tools do (ProgrammerController::padLost()). The gone
       line no longer says whose it was: the walker's pad is m_walkerId. */
    if (m_walkBeside && isWalking() && sameController(universe, m_walker))
    {
        const quint32 twin = universe == m_walker ? lineOfPad(m_walkerId, universe) : m_walker;
        if (twin == UINT_MAX)
            endBesideWalk(tr("Walk ended: the controller walking it went away."));
        else
            m_walker = twin;
    }
    /* That line's reading goes (a pad lost, or another device behind its
       patch, board Q253's fourth review: nothing it held is held); the
       pad's other line, still here, keeps its own: what each pad holds is
       re-read from what is left (the line gone may no longer say whose it
       was). The stop latch is the pad's: still set only while what is left
       holds all four of a stop that really fired, so a squeeze completed
       afresh -- on a line that joins mid-squeeze too -- stops. */
    m_held.remove(universe);
    m_walkTriggers.remove(universe);
    // The walk's modifier and level, if it was the walker's pad.
    if (isWalking() && m_pc != nullptr && m_pc->controllerOwner() != UINT_MAX
        && sameController(universe, m_pc->controllerOwner()))
    {
        m_lbHeld = false;
        m_lbUsed = false;
        m_levelUp = m_levelDown = 0.0f;
    }
    for (auto it = m_pads.begin(); it != m_pads.end(); ++it)
    {
        const Held h = padHeld(it.key());
        Pad &p = it.value();
        if (!h.all())
            p.stopFired = false;
        if (!h.lb && !h.rb)
            p.spent = false;
        if (p.chord != nullptr && !(h.lb && h.rb && !h.anyTrigger() && !h.spent))
            p.chord->stop();
    }
    if ((m_armed && m_armUniverse == universe) || m_focusArm == universe)
        controllerLost();
    else
        sync();     // the runner's name on the chip comes or goes with a pad
}

void PadRun::setPadPresent(bool present)
{
    if (m_padGone == !present)
        return;
    m_padGone = !present;
    // Gone in FOCUS: the arm the focus would go back to goes too (rule 6).
    if (!present && m_focusArm != UINT_MAX)
        controllerLost();
    sync();
}

void PadRun::showLight()
{
    if (m_pc == nullptr)
        return;
    // A tool on the pad has its own lamp (the save colours) -- but red
    // after the emergency stop, as every surface is.
    if ((m_pc->focusSession()->isActive() || m_pc->refineSession()->isActive())
        && state() != Stopped && state() != Focus)
        return;
    m_pc->setPadLight(lightColour());
}

/*****************************************************************************
 * Venue focus from the pad (rule 6)
 *****************************************************************************/

bool PadRun::isFocusing() const
{
    if (!m_enabled || m_pc == nullptr)
        return false;
    const FocusCallCue *fcc = m_pc->focusCallCue();
    if (fcc == nullptr)
        return false;
    if (fcc->state() == FocusCallCue::NamingVenue || fcc->state() == FocusCallCue::Running)
        return true;
    // Starting: the calls are up a moment before the cue says it runs.
    const FocusSession *f = m_pc->focusSession();
    return fcc->isArmed() && f->isActive() && f->isRunningCalls();
}

bool PadRun::isFocusCueWaiting() const
{
    const FocusCallCue *fcc = m_pc != nullptr ? m_pc->focusCallCue() : nullptr;
    return m_enabled && fcc != nullptr && fcc->isArmed() && !isFocusing();
}

void PadRun::focusStartPress(quint32 universe, bool safetyHeld)
{
    /* After the emergency stop nothing comes up from a pad until the show
       is brought back: a GO from a pad that runs cues or from the computer
       (Q254; review 5). The keeper's hold decides, not the pad's red: an
       arm alone does not bring the show back (one source, rule 2). */
    if (m_stopped || showHeld())
    {
        refuse(Walk, tr("Emergency stop: the show is held until a GO (the cue pad, or GO on the computer)."));
        return;
    }
    // The start is never one button (rules 3 and 6): LT + Menu, always.
    if (m_armed && sameController(universe, m_armUniverse))
    {
        // The runner: from its arm, which is put by.
        if (!safetyHeld)
        {
            refuse(Walk, tr("Armed. To walk, disarm first: hold {LB} and {RB}. "
                            "{LT} + {Menu} starts the venue focus."));
            return;
        }
        startFocus(universe);
        return;
    }
    /* A pad that runs cues, with nobody running them: from its arm, as
       ever (one pad with every job is as before). */
    if (!m_armed && hasJob(universe, ControllerJobs::RunCues))
    {
        refuse(Walk, !padRunSupported() ? notSupportedText()
                   : tr("Not armed. To start the venue focus, hold {LB} and {RB} to arm, then {LT} + {Menu}."));
        return;
    }
    /* The Focus job on its own (board Q250): a pad with no arm to give --
       no Run cues, or another pad runs them -- starts it with LT + Menu,
       and the runner keeps its arm. */
    if (!safetyHeld)
    {
        refuse(Walk, tr("To start the venue focus, hold {LT} and press {Menu}."));
        return;
    }
    startFocus(universe);
}

bool PadRun::startFocus(quint32 universe)
{
    if (m_pc == nullptr)
        return false;
    /* From the runner's own pad, FOCUS is not armed: the arm is put by, to
       come back when the focus ends -- before the start, so the calls
       taking the pad are no tool taking an armed pad. Told once the start
       has gone. From another pad (Q250) the runner's arm is not this
       pad's to move: it stays. */
    const bool fromArm = m_armed && sameController(universe, m_armUniverse);
    const quint32 armedOn = m_armUniverse;
    if (fromArm)
    {
        m_focusArm = armedOn;
        m_armed = false;
        m_armUniverse = UINT_MAX;
        stopArmChords();
        m_playing = false;
        m_flash->stop();
    }
    m_pc->claimController(universe);
    QString why;
    if (!m_pc->focusCallCue()->start(&why))
    {
        // Refused (no calls, no lights; already said): armed as it was.
        if (fromArm)
        {
            m_focusArm = UINT_MAX;
            m_armed = true;
            m_armUniverse = armedOn;
        }
        // The cue waits again: the controller the start claimed is let go.
        m_pc->releaseControllerSoon();
        if (!why.isEmpty())
            showNotice(m_pc->padText(why));
        sync();
        return false;
    }
    if (fromArm)
        emit armedChanged(false);
    m_pc->padRumble(1, ControllerScheme::rumbleShortMs(), fromArm ? UINT_MAX : universe);
    sync();
    return true;
}

bool PadRun::focusMenu(quint32 universe)
{
    if (!isFocusing() || m_pc == nullptr)
        return false;
    if (!hasJob(universe, ControllerJobs::Focus))
    {
        refuse(Walk, noFocusText());
        return true;
    }
    // The pad that has the focus ends it (Q135), not another.
    const quint32 owner = m_pc->controllerOwner();
    if (owner != 0xFFFFFFFF && !sameController(universe, owner))
    {
        refuse(Walk, tr("Another controller is in use."));
        return true;
    }
    if (!padHeld(padId(universe)).lt.isDown())
    {
        refuse(Walk, tr("Venue focus: hold {LT} and press {Menu} to finish it."));
        return true;
    }
    FocusCallCue *fcc = m_pc->focusCallCue();
    if (fcc->state() == FocusCallCue::NamingVenue)
        fcc->cancelStart();
    else
        // As the tool's own Leave: the laptop asks about unsaved work, then ends it.
        m_pc->focusSession()->apply(FocusSession::Exit);
    return true;
}

QString PadRun::lastCallHold()
{
    if (!isFocusing() || m_pc == nullptr)
        return QString();
    const QString text = m_pc->padText(tr("Last call. Hold {LT} and press {Menu} to finish."));
    showNotice(text);
    return text;
}

void PadRun::putArmBy()
{
    m_focusArm = m_armUniverse;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    stopArmChords();
    m_playing = false;
    m_flash->stop();
    emit armedChanged(false);
}

void PadRun::focusCueChanged()
{
    /* Begun from the laptop while armed (the venue to name first): FOCUS.
       Begun on another pad (Q250): the runner keeps its arm. */
    if (m_armed && isFocusing() && !isBesideFocus(m_armUniverse))
        putArmBy();
    /* Over (or the start given up): ARMED again, as it was before LT +
       Menu -- unless something else has happened to the pad since (the
       stop, the pad gone, another tool, the role). */
    bool told = false;
    if (!isFocusing() && m_focusArm != UINT_MAX)
    {
        const quint32 u = m_focusArm;
        m_focusArm = UINT_MAX;
        if (m_enabled && !m_stopped && !m_padGone && holder().isEmpty() && !isWalking())
        {
            m_armed = true;
            m_armUniverse = u;
            stopArmChords();
            emit armedChanged(true);
            // Its own pad, once (review 1, 6).
            if (m_pc != nullptr)
                m_pc->padRumble(2, ControllerScheme::rumbleShortMs(), u);
            told = true;
        }
    }
    const bool focusing = isFocusing();
    if (m_wasFocusing && !focusing)
    {
        // The words of a press refused beside it go with it (review 8).
        if (!m_besideNotice.isEmpty() && m_notice == m_besideNotice)
            showNotice(QString());
        m_besideNotice.clear();
        /* A tool has the pad as the focus ends (the laptop's Focus this
           cue ended it): the runner it was kept beside is disarmed now,
           and never told GO works (review 2). */
        const QString tool = holder();
        if (m_armed && !tool.isEmpty())
        {
            reset();
            showNotice(tr("Disarmed: %1 has the controller.").arg(tool));
        }
        /* Over while the runner kept its arm beside it: its pad is told by
           its motors, as an arm is, that GO works again (its light turns
           back) -- once. */
        else if (m_armed && !told && m_pc != nullptr)
            m_pc->padRumble(2, ControllerScheme::rumbleShortMs(), m_armUniverse);
    }
    m_wasFocusing = focusing;
    m_shownOwner = m_pc != nullptr ? m_pc->controllerOwner() : UINT_MAX;
    sync();
}

void PadRun::focusOwnerChanged()
{
    /* Only a venue focus's owner changes FOCUS: a claim made as a start
       goes (startFocus(), before the calls) must not show the moment
       between ARMED and FOCUS. */
    if (!isFocusing())
    {
        m_shownOwner = m_pc != nullptr ? m_pc->controllerOwner() : UINT_MAX;
        return;
    }
    /* A pad that can work it took it while the runner could too (the
       runner itself), or it has no pad now and the runner can work it:
       the runner's arm is put by, as for a focus begun on the laptop
       (review 4, 11). */
    if (m_armed && isFocusing() && !isBesideFocus(m_armUniverse))
        putArmBy();
    // Left with no pad (its job taken, its pad gone, review 11, 12): said.
    const quint32 owner = m_pc != nullptr ? m_pc->controllerOwner() : UINT_MAX;
    if (isFocusing() && owner == UINT_MAX && m_shownOwner != UINT_MAX && padCount() > 0)
        showNotice(tr("Venue focus has no controller now: a controller with the Focus job takes it "
                      "with a press, or finish it on the laptop."));
    m_shownOwner = owner;
    sync();
}

/*****************************************************************************
 * Controller jobs (docs/design/CONTROLLER_JOBS.md)
 *****************************************************************************/

bool PadRun::hasJob(quint32 universe, int job) const
{
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    return jobs == nullptr || jobs->has(universe, ControllerJobs::Job(job));
}

bool PadRun::sameController(quint32 a, quint32 b) const
{
    if (a == b)
        return true;
    if (a == UINT_MAX || b == UINT_MAX)
        return false;
    /* One pad seen on two universes (macOS can list one controller twice)
       is one controller: its twin's presses are its own. */
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    if (jobs == nullptr)
        return false;
    const QString ia = jobs->identityOfUniverse(a);
    return !ia.isEmpty() && !ControllerJobs::isNameOnly(ia) && ia == jobs->identityOfUniverse(b);
}

QString PadRun::padId(quint32 universe) const
{
    if (m_padIdsOn)
    {
        const auto it = m_padIds.constFind(universe);
        if (it != m_padIds.constEnd())
            return it.value();
    }
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    QString id = jobs != nullptr ? jobs->identityOfUniverse(universe) : QString();
    if (id.isEmpty() || ControllerJobs::isNameOnly(id))
        id = QStringLiteral("#%1").arg(universe);
    if (m_padIdsOn)
        m_padIds.insert(universe, id);
    return id;
}

void PadRun::beginPadIds()
{
    m_padIdsOn = true;
}

void PadRun::endPadIds(bool wasOn)
{
    m_padIdsOn = wasOn;
    if (!wasOn)
        m_padIds.clear();
}

PadRun::Held PadRun::padHeld(const QString &pad) const
{
    Held out;
    for (auto it = m_held.constBegin(); it != m_held.constEnd(); ++it)
    {
        const Line &line = it.value();
        if (!line.lb && !line.rb && !line.anyTrigger() && line.buttons.isEmpty())
            continue;
        if (padId(it.key()) != pad)
            continue;
        out.lb = out.lb || line.lb;
        out.rb = out.rb || line.rb;
        if (line.lt.isDown() && !out.lt.isDown())
            out.lt = line.lt;
        if (line.rt.isDown() && !out.rt.isDown())
            out.rt = line.rt;
        out.buttons.unite(line.buttons);
    }
    out.spent = m_pads.value(pad).spent;
    return out;
}

bool PadRun::downElsewhere(quint32 universe, GamepadRole::Role role) const
{
    const QString pad = padId(universe);
    for (auto it = m_held.constBegin(); it != m_held.constEnd(); ++it)
    {
        if (it.key() == universe)
            continue;
        const Line &line = it.value();
        bool down = false;
        switch (role)
        {
            case GamepadRole::LB:           down = line.lb; break;
            case GamepadRole::RB:           down = line.rb; break;
            case GamepadRole::LeftTrigger:  down = line.lt.isDown(); break;
            case GamepadRole::RightTrigger: down = line.rt.isDown(); break;
            default:                        down = line.buttons.contains(int(role)); break;
        }
        if (down && padId(it.key()) == pad)
            return true;
    }
    return false;
}

quint32 PadRun::runnerUniverse() const
{
    return m_armed ? m_armUniverse : m_focusArm;
}

QString PadRun::runnerName() const
{
    const quint32 u = runnerUniverse();
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    if (u == UINT_MAX || jobs == nullptr)
        return QString();
    return jobs->nameOfUniverse(u);
}

int PadRun::padCount() const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || m_pc == nullptr)
        return 0;
    /* One pad seen on two universes (one identity: macOS lists some pads
       twice) is one controller (CONTROLLER_JOBS.md 9): counted once. */
    QList<quint32> counted;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || !ip->isPatched() || !m_pc->isPadUniverse(u))
            continue;
        bool twin = false;
        for (quint32 v : std::as_const(counted))
            twin = twin || sameController(u, v);
        if (!twin)
            counted << u;
    }
    return int(counted.size());
}

quint32 PadRun::lightUniverse() const
{
    switch (state())
    {
        case Armed:     return m_armUniverse;
        case Focus:
            return focusOwnerUniverse();
        default:        break;
    }
    return UINT_MAX;
}

QString PadRun::noRunCuesText()
{
    return tr("This controller doesn't run cues.");
}

QString PadRun::noFocusText()
{
    return tr("This controller doesn't do focus.");
}

QString PadRun::roleSwitchText()
{
    return tr("Run / Tools is switched on the computer: the Run tab's Pad: button");
}

void PadRun::tellRefused(const QString &text)
{
    if (m_pc == nullptr || text.isEmpty())
        return;
    const QString said = m_pc->padText(text);
    m_pc->tellOperator(said, ControllerScheme::ConfirmPriority);
    showNotice(said);
}

quint32 PadRun::focusOwnerUniverse() const
{
    /* The pad that owns it (Q135), and no other: a runner's arm put by is
       not ownership, so any pad with Focus may take an ownerless focus
       (second review 3), and FOCUS names no pad until one does. */
    return m_pc != nullptr ? m_pc->controllerOwner() : UINT_MAX;
}

QString PadRun::focusOwnerName() const
{
    const quint32 u = focusOwnerUniverse();
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    if (u == UINT_MAX || jobs == nullptr)
        return QString();
    return jobs->nameOfUniverse(u);
}

bool PadRun::isBesideFocus(quint32 universe) const
{
    if (!isFocusing() || universe == UINT_MAX)
        return false;
    const quint32 owner = focusOwnerUniverse();
    if (owner != UINT_MAX)
        return !sameController(universe, owner);
    // No pad has it yet: a pad that can work it may take it (Q135).
    return !hasJob(universe, ControllerJobs::Focus);
}

QString PadRun::focusBesideText() const
{
    const QString name = focusOwnerName();
    return name.isEmpty() ? tr("Venue focus is running: nothing plays until it ends.")
                          : tr("Venue focus is running on %1: nothing plays until it ends.").arg(name);
}

bool PadRun::isBesideWalk(quint32 universe) const
{
    return m_walkBeside && isWalking() && universe != UINT_MAX && !sameController(universe, m_walker);
}

bool PadRun::isBesideTool(quint32 universe) const
{
    return isBesideFocus(universe) || isBesideWalk(universe);
}

QString PadRun::walkBesideText() const
{
    const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
    const QString name = jobs != nullptr ? jobs->nameOfUniverse(m_walker) : QString();
    return name.isEmpty() ? tr("Another controller is walking the show: nothing plays until the walk ends.")
                          : tr("%1 is walking the show: nothing plays until the walk ends.").arg(name);
}

PadRun::FocusStart PadRun::focusStart() const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || m_pc == nullptr)
        return ArmFirst;
    bool anyPad = false, anyFocus = false;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || !ip->isPatched() || !m_pc->isPadUniverse(u))
            continue;
        anyPad = true;
        if (!hasJob(u, ControllerJobs::Focus))
            continue;
        anyFocus = true;
        // No arm of its own to give: no Run cues, or another pad runs them.
        if (!hasJob(u, ControllerJobs::RunCues) || (m_armed && !sameController(u, m_armUniverse)))
            return NoArm;
    }
    return anyPad && !anyFocus ? NoFocusPad : ArmFirst;
}

QString PadRun::runningCuesText() const
{
    const QString name = runnerName();
    return name.isEmpty() ? tr("Another controller is running cues.")
                          : tr("%1 is running cues.").arg(name);
}

QString PadRun::chipNotice() const
{
    // The chip names the focus's pad already: its refusal, short.
    const QString owner = focusOwnerName();
    if (!m_notice.isEmpty() && !owner.isEmpty() && m_notice == focusBesideText()
        && roleText().contains(owner))
        return tr("nothing plays until the venue focus ends");
    if (m_walkBeside && !m_notice.isEmpty() && m_notice == walkBesideText() && m_pc != nullptr
        && m_pc->controllerJobs() != nullptr
        && roleText().contains(m_pc->controllerJobs()->nameOfUniverse(m_walker)))
        return tr("nothing plays until the walk ends");
    const QString name = runnerName();
    if (!m_notice.isEmpty() && !name.isEmpty() && m_notice == runningCuesText()
        && roleText().contains(name))
        return tr("another controller is running cues");
    return m_notice;
}

void PadRun::jobsChanged()
{
    // The walker beside the runner lost its Focus job (second review 2).
    if (m_walkBeside && isWalking() && !hasJob(m_walker, ControllerJobs::Focus))
    {
        const ControllerJobs *jobs = m_pc != nullptr ? m_pc->controllerJobs() : nullptr;
        const QString name = jobs != nullptr ? jobs->nameOfUniverse(m_walker) : QString();
        endBesideWalk(name.isEmpty() ? tr("Walk ended: the controller walking it no longer does focus.")
                                     : tr("Walk ended: %1 no longer does focus.").arg(name));
    }
    /* A job taken away while it is in use (CONTROLLER_JOBS.md 10): the
       runner's Run cues -- disarmed, with no button (rule 3's "no button
       at all"); the arm a venue focus put by is not given back. */
    if (m_armed && !hasJob(m_armUniverse, ControllerJobs::RunCues))
    {
        clear();
        sync();
        showNotice(tr("Disarmed: this controller no longer runs cues."));
        if (m_pc != nullptr)
            m_pc->tellOperator(tr("Disarmed."), ControllerScheme::ConfirmPriority);
        return;
    }
    if (m_focusArm != UINT_MAX && !hasJob(m_focusArm, ControllerJobs::RunCues))
        m_focusArm = UINT_MAX;
    sync();
}

/*****************************************************************************
 * Triggers and swipes
 *****************************************************************************/

PadTriggers::State PadRun::triggerState() const
{
    // Armed beside another pad's venue focus: the triggers fire nothing.
    return PadTriggers::forState(m_enabled, m_armed && !isFocusing() && !isWalking(), isWalking());
}

void PadRun::updateTriggers(bool force)
{
    if (m_pc != nullptr)
        m_pc->setPadTriggers(triggerState(), force);
}

void PadRun::touchValue(GamepadRole::Role role, float norm, quint32 universe)
{
    if (!m_enabled)
        return;
    m_swipe.setValue(role, norm);
    if (universe != UINT_MAX)
        m_touchUniverse = universe;
}

void PadRun::touchClick(bool down)
{
    if (m_enabled)
        m_swipe.setClick(down);
}

void PadRun::swipeTick(double dt)
{
    if (!m_enabled)
        return;
    const TouchSwipe::Swipe s = m_swipe.take(dt);
    if (s != TouchSwipe::None)
        swipe(s);
}

bool PadRun::swipe(TouchSwipe::Swipe s)
{
    if (!m_enabled || m_pc == nullptr || s == TouchSwipe::None)
        return false;
    const bool horizontal = s == TouchSwipe::Left || s == TouchSwipe::Right;
    const int delta = (s == TouchSwipe::Right || s == TouchSwipe::Down) ? 1 : -1;
    if (isWalking())
    {
        if (horizontal)
            return stepCue(delta);
        stepLayer(delta);
        return true;
    }
    // Run, not walking: up / down nothing (the list is the Run tab's);
    // armed, nothing at all (the pad is the cue map).
    if (!horizontal || m_armed)
        return false;
    // Run, not walking: a walk from the next / previous cue, Design only.
    if (m_doc->mode() == Doc::Operate)
        return refuseWalk(tr("In Operate the cue moves with GO."));
    // The pad that swiped owns the walk (Q135), as one that pressed Menu.
    if (m_touchUniverse != UINT_MAX)
        m_pc->claimController(m_touchUniverse);
    if (!startWalk())
    {
        m_pc->releaseControllerNow();
        return false;
    }
    return stepCue(delta);
}

/*****************************************************************************
 * Commands
 *****************************************************************************/

bool PadRun::trigger(Action action)
{
    switch (action)
    {
        case WhereAmI:
            if (m_pc != nullptr)
                m_pc->tellOperator(wherePhrase(), ControllerScheme::WhereNowPriority);
            return true;
        case Walk:
            return startWalk();
        case FireHit:
            return fireHit();
        case EmergencyStop:
            emergencyStop();
            return true;
        case Go: case Back: case SubGo: case SubBack:
            break;
        case Arm: case Safety: case NoAction:
            return false;
    }

    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    if (sc == nullptr)
        return false;
    ShowControl::Request r;
    r.command = commandFor(action);
    r.chaser = listChaser();
    // The one GO path: the App's guard decides, and says what it held.
    hookShowControl();
    m_heldNow = false;
    const bool done = sc->run(r, ShowControl::RoutePad);
    emit commandSent(r.command, done);
    if (m_pc == nullptr)
        return done;
    // Held by the double-GO guard: already buzzed and said, as everywhere.
    if (!done && m_heldNow)
        return done;
    if (done)
    {
        m_pc->padRumble(1, ControllerScheme::rumbleShortMs());
        // Playing: green at once, then the blink for as long as it fades.
        m_flash->start();
        m_playing = true;
        m_blinkOn = true;
        showLight();
    }
    else
    {
        m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
        const QString said = tr("%1: nothing to play.").arg(ShowControl::commandName(r.command));
        m_pc->tellOperator(said, ControllerScheme::ConfirmPriority);
        showNotice(said);
    }
    return done;
}

/*****************************************************************************
 * Hits
 *****************************************************************************/

QList<quint32> PadRun::allHits() const
{
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    return st != nullptr ? st->allHits() : QList<quint32>();
}

QList<quint32> PadRun::armedHits() const
{
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    return st != nullptr ? st->armedTier(allHits()) : QList<quint32>();
}

bool PadRun::fireHit()
{
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st == nullptr || m_pc == nullptr)
        return false;
    auto refuse = [this](const QString &why) {
        m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
        m_pc->tellOperator(why, ControllerScheme::ConfirmPriority);
        return false;
    };
    // Like a VC button: a hit fires in Operate only.
    if (m_doc->mode() != Doc::Operate)
        return refuse(tr("Hits fire in Operate."));
    const QList<quint32> tier = armedHits();
    if (tier.isEmpty())
        return refuse(tr("No hit armed."));

    // One running (a toggle on, or a one-shot still going): the press lets it go.
    for (quint32 h : tier)
    {
        Function *f = m_doc->function(st->lookFunction(h));
        if (f == nullptr || !f->isRunning())
            continue;
        EffectScriptRunner *esr = m_doc->effectScriptRunner();
        const ShowTree::Cue *c = st->cue(h);
        const QString name = c != nullptr ? c->name : f->name();
        // A one-shot that has finished and is still on plays again.
        if (esr != nullptr && esr->oneShotState(f->id()) == 1 && esr->restartOneShots(f->id()))
        {
            m_pc->padRumble(1, ControllerScheme::rumbleShortMs());
            m_pc->tellOperator(tr("%1.").arg(name), ControllerScheme::ConfirmPriority);
            emit hitFired(h, true);
            return true;
        }
        f->stop(FunctionParent::master());
        m_pc->padRumble(1, ControllerScheme::rumbleShortMs());
        m_pc->tellOperator(tr("%1 off.").arg(name), ControllerScheme::ConfirmPriority);
        emit hitFired(h, false);
        return true;
    }

    // Several at the narrowest place: each press takes the next.
    const quint32 pick = hitTurn();
    Function *f = m_doc->function(st->lookFunction(pick));
    if (f == nullptr)
        return refuse(tr("No hit armed."));
    nextHitTurn();
    f->start(m_doc->masterTimer(), FunctionParent::master());
    const ShowTree::Cue *c = st->cue(pick);
    m_pc->padRumble(1, ControllerScheme::rumbleShortMs());
    m_pc->tellOperator(tr("%1.").arg(c != nullptr ? c->name : f->name()), ControllerScheme::ConfirmPriority);
    emit hitFired(pick, true);
    return true;
}

quint32 PadRun::hitTurn() const
{
    const QList<quint32> tier = armedHits();
    return tier.isEmpty() ? ShowTree::invalidId() : tier.at(turnIn(tier) % tier.count());
}

int PadRun::turnIn(const QList<quint32> &tier) const
{
    /* The turn counts in the set syncHit() last saw. A set it has not seen
       yet is new, so it starts at 1 of n: syncHit() hears of a cue change
       a moment late (functionStarted() and currentStepChanged() come queued
       from the MasterTimer's thread), and a pull in that moment -- a busy
       machine, a quick operator -- took the old set's turn. */
    return tier == m_armedSet ? m_hitStep : 0;
}

void PadRun::nextHitTurn()
{
    /* No syncHit() here: the hit starting is what changes the words (its
       functionStarted(), a tick later), and telling the screens now would
       flash the next one's name for that tick before "… off". The set it
       counts in is taken now, though: syncHit() finding it later must not
       start it over and lose this pull's turn. */
    const QList<quint32> armedNow = armedHits();
    if (armedNow != m_armedSet)
    {
        m_armedSet = armedNow;
        m_hitStep = 0;
    }
    ++m_hitStep;
}

quint32 PadRun::nextHit(bool *letsGo, int *index, int *count) const
{
    if (letsGo != nullptr)
        *letsGo = false;
    if (index != nullptr)
        *index = 0;
    if (count != nullptr)
        *count = 0;
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    // As fireHit(): Operate only, the narrowest armed place.
    if (st == nullptr || m_doc->mode() != Doc::Operate)
        return ShowTree::invalidId();
    const QList<quint32> tier = armedHits();
    if (tier.isEmpty())
        return ShowTree::invalidId();
    quint32 pick = ShowTree::invalidId();
    bool off = false;
    // One on: the pull lets it go -- or, a one-shot that has finished, plays it again.
    for (quint32 h : tier)
    {
        const Function *f = m_doc->function(st->lookFunction(h));
        if (f == nullptr || !f->isRunning())
            continue;
        const EffectScriptRunner *esr = m_doc->effectScriptRunner();
        pick = h;
        off = esr == nullptr || esr->oneShotState(f->id()) != 1;
        break;
    }
    if (pick == ShowTree::invalidId())
    {
        pick = tier.at(turnIn(tier) % tier.count());
        if (m_doc->function(st->lookFunction(pick)) == nullptr)
            return ShowTree::invalidId();     // fireHit(): "No hit armed."
    }
    if (letsGo != nullptr)
        *letsGo = off;
    if (index != nullptr)
        *index = int(tier.indexOf(pick)) + 1;
    if (count != nullptr)
        *count = int(tier.count());
    return pick;
}

QString PadRun::hitText() const
{
    bool off = false;
    int index = 0, count = 0;
    const quint32 hit = nextHit(&off, &index, &count);
    if (hit == ShowTree::invalidId())
        return QString();
    const ShowTreeController *st = m_doc->showTree();
    const ShowTree::Cue *c = st->cue(hit);
    QString name = c != nullptr ? c->name : QString();
    if (name.isEmpty())
        if (const Function *f = m_doc->function(st->lookFunction(hit)))
            name = f->name();
    if (count > 1)
        return off ? tr("Hit \u25b8 %1 off (%2 of %3)").arg(name).arg(index).arg(count)
                   : tr("Hit \u25b8 %1 (%2 of %3)").arg(name).arg(index).arg(count);
    return off ? tr("Hit \u25b8 %1 off").arg(name) : tr("Hit \u25b8 %1").arg(name);
}

QString PadRun::armedHitText() const
{
    return state() == Armed ? hitText() : QString();
}

/*****************************************************************************
 * The role chord
 *****************************************************************************/

void PadRun::setRoleChordMs(int ms)
{
    m_chord->setInterval(ms);
}

void PadRun::roleChordBegin()
{
    // Not from a press made to test a button.
    if (m_chordPending || m_chordFired || m_testing)
        return;
    m_chordPending = true;
    m_chord->start();
}

void PadRun::roleChordCancel()
{
    m_chord->stop();
    m_chordPending = false;
    // A fired chord stays "fired" until its buttons are up (roleChordEnd()).
}

bool PadRun::roleChordEnd()
{
    const bool wasPending = m_chordPending;
    m_chord->stop();
    m_chordPending = false;
    m_chordFired = false;
    return wasPending;
}

void PadRun::roleChordFired()
{
    if (!m_chordPending)
        return;
    m_chordPending = false;
    /* Held (Q251): the role is not the pad's to switch -- the hold put a
       wired PlayStation clone into its Xbox mode, and the pad dropped off
       USB mid-show. Nothing changes: not the role, not the arm. Its
       release is no tap (no Focus mode). */
    m_chordFired = true;
    tellRefused(roleSwitchText());
}

quint32 PadRun::listChaserOf(const Doc *doc)
{
    // The Run tab's list (its picker, kept with the show), else the main one.
    const ControlMap *cm = doc != nullptr ? doc->controlMap() : nullptr;
    const ShowTreeController *st = doc != nullptr ? doc->showTree() : nullptr;
    if (cm == nullptr || st == nullptr)
        return UINT_MAX;
    const ShowTree::CueList *l = st->cueList(cm->runTabList());
    if (l == nullptr || l->looks || l->isSubCues() || qobject_cast<Chaser *>(doc->function(l->chaser)) == nullptr)
        return UINT_MAX;
    return l->chaser;
}

Chaser *PadRun::playedList(Doc *doc)
{
    ShowControl *sc = doc != nullptr ? doc->showControl() : nullptr;
    if (sc == nullptr)
        return nullptr;
    const quint32 at = listChaserOf(doc);
    return at != UINT_MAX ? qobject_cast<Chaser *>(doc->function(at)) : sc->cueList(QString());
}

QString PadRun::listName(quint32 chaser) const
{
    if (chaser == UINT_MAX && m_doc->showControl() != nullptr)
        chaser = m_doc->showControl()->mainCueList();
    if (const ShowTreeController *st = m_doc->showTree())
        for (const ShowTree::Production &p : st->productions())
            for (const ShowTree::CueList &l : p.lists)
                if (l.chaser == chaser && !l.name.isEmpty())
                    return l.name;
    if (Function *f = m_doc->function(chaser))
        return f->name();
    return tr("No cue list");
}

QString PadRun::wherePhrase() const
{
    ShowControl *sc = m_doc->showControl();
    if (sc == nullptr)
        return QString();
    const quint32 at = listChaser();
    Chaser *chaser = at != UINT_MAX ? qobject_cast<Chaser *>(m_doc->function(at)) : sc->cueList(QString());
    if (chaser == nullptr)
        return tr("No cue list.");
    const QString name = listName(chaser->id());
    const int step = sc->standbyStep(chaser);
    if (step < 0)
        return tr("%1. Nothing on standby.").arg(name);
    return tr("%1. Next: cue %2.").arg(name, ShowControl::cueName(chaser, step, sc->standbySubCue(chaser)));
}

/*****************************************************************************
 * Walking the show (Q192)
 *****************************************************************************/

PadRun::WalkAction PadRun::walkActionFor(GamepadRole::Role role)
{
    switch (role)
    {
        // The triggers step the big unit (the cue), the bumpers the next one down.
        case GamepadRole::RightTrigger: return NextCue;
        case GamepadRole::LeftTrigger:  return PreviousCue;
        case GamepadRole::RB:           return NextLayer;
        case GamepadRole::LB:           return PreviousLayer;
        case GamepadRole::Back:         return Solo;
        case GamepadRole::Y:            return TagCue;
        case GamepadRole::Start:        return LeaveWalk;
        default:                        break;
    }
    return WalkNoAction;
}

GamepadRole::Role PadRun::roleFor(WalkAction action)
{
    for (int r = GamepadRole::A; r <= GamepadRole::TouchClick; ++r)
        if (action != WalkNoAction && walkActionFor(GamepadRole::Role(r)) == action)
            return GamepadRole::Role(r);
    return GamepadRole::None;
}

QString PadRun::label(WalkAction action)
{
    switch (action)
    {
        case NextCue:       return tr("Next cue (Design: previewed, no GO)");
        case PreviousCue:   return tr("Previous cue");
        case NextLayer:     return tr("Next layer (group or fixture at that scope)");
        case PreviousLayer: return tr("Previous layer (tap; held: the modifier)");
        case Solo:          return tr("Solo the layer / the whole cue ({View} + {B}: undo)");
        case TagCue:        return tr("Hold: tag the cue Needs work (tap: layer, group, fixture)");
        case LeaveWalk:     return tr("Stop walking");
        case WalkNoAction:  break;
    }
    return QString();
}

QList<PadRun::WalkAction> PadRun::walkActions()
{
    return { NextCue, PreviousCue, NextLayer, PreviousLayer, Solo, TagCue, LeaveWalk };
}

QStringList PadRun::walkMapLines(PadNames::Family family)
{
    QStringList out;
    for (WalkAction a : walkActions())
        out << QStringLiteral("%1: %2").arg(PadNames::name(roleFor(a), family),
                                            PadNames::expand(label(a), family));
    // Hold LB: the modifier (a tap of it is the previous layer).
    out << PadNames::expand(tr("{LB} held + {RT} / {LT}: level up / down ({A} keeps it in the cue)"), family);
    out << PadNames::expand(tr("{LB} held + sticks: the light itself on its target, for every cue "
                               "(a fixture the cue aims); {LB} + {A} keeps it"), family);
    out << PadNames::expand(tr("Sticks, D-pad, {A} keep, {B} back, {X}, {LS}, {RS}: as Focus this cue"), family);
    return out;
}

bool PadRun::isWalking() const
{
    return m_walking && m_pc != nullptr && m_pc->refineSession()->isActive();
}

quint32 PadRun::walkCue() const
{
    return isWalking() ? m_pc->refineSession()->cueId() : 0xFFFFFFFF;
}

quint32 PadRun::cueInHand() const
{
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    if (st == nullptr)
        return 0xFFFFFFFF;
    // The Run tab's list (else the main one), as a Show Tree list.
    quint32 chaser = listChaser();
    if (chaser == UINT_MAX && sc != nullptr)
        chaser = sc->mainCueList();
    const ShowTree::CueList *list = nullptr;
    for (const ShowTree::Production &p : st->productions())
        for (const ShowTree::CueList &l : p.lists)
            if (l.chaser == chaser && !l.looks)
                list = &l;
    if (m_doc->mode() == Doc::Operate)
        return list != nullptr ? st->runningCue(list->id) : 0xFFFFFFFF;
    // Design: the cue picked in the tree first -- what the laptop shows.
    const quint32 edited = st->editedCue();
    if (const ShowTree::CueList *l = st->listOfCue(edited))
        if (!l->looks)
            return edited;
    if (list == nullptr || list->cues.isEmpty())
        return 0xFFFFFFFF;
    const Chaser *c = qobject_cast<const Chaser *>(m_doc->function(list->chaser));
    const int step = sc != nullptr && c != nullptr ? sc->standbyStep(c) : -1;
    return list->cues.at(step >= 0 && step < list->cues.size() ? step : 0).id;
}

bool PadRun::holdsDeletedCue() const
{
    if (m_doc == nullptr)
        return false;
    // The list cueInHand() reads.
    quint32 chaser = listChaser();
    if (chaser == UINT_MAX && m_doc->showControl() != nullptr)
        chaser = m_doc->showControl()->mainCueList();
    const Chaser *c = qobject_cast<const Chaser *>(m_doc->function(chaser));
    return c != nullptr && c->isRunning() && c->holdsRemovedStep();
}

QString PadRun::cueLabel(quint32 cueId) const
{
    const ShowTree::Cue *c = m_doc->showTree() != nullptr ? m_doc->showTree()->cue(cueId) : nullptr;
    if (c == nullptr)
        return QString();
    return c->number.isEmpty() ? c->name : c->number;
}

bool PadRun::tryStart(quint32 cueId, QString *why)
{
    RefineSession *r = m_pc->refineSession();
    r->setWalking(true);
    // Focus this cue stops itself before starting another cue: not the walk's end.
    m_restarting = true;
    const bool ok = r->start(cueId, 0xFFFFFFFF, why);
    m_restarting = false;
    if (!ok && !r->isActive())
        r->setWalking(false);
    return ok;
}

void PadRun::walkStarted()
{
    /* The walking map has no GO, and no arm (a walk never starts armed) --
       but a walk beside the runner (Q252) leaves the runner's arm. */
    const bool wasArmed = m_armed && !m_walkBeside;
    if (wasArmed)
    {
        m_armed = false;
        m_armUniverse = UINT_MAX;
    }
    stopArmChords();
    forgetHeld();
    /* The triggers as the walk begins; a cue step (this restart) keeps
       them: the pull that stepped is still down, and its next samples on
       either line are the same pull, never another step. */
    if (!m_walking)
        m_walkTriggers.clear();
    m_holdY.reset();
    releaseModifier();
    m_lbUsed = true;        // an LB still held from the step: not a tap
    if (wasArmed)
        emit armedChanged(false);
    if (!m_walking)
    {
        m_walking = true;
        emit walkingChanged(true);
    }
    syncSolo();
    sync();
}

void PadRun::walkEnded()
{
    if (!m_walking)
        return;
    m_walking = false;
    m_pc->refineSession()->setWalking(false);
    releaseModifier();
    m_holdY.reset();
    m_walkTriggers.clear();
    setSolo(false);
    forgetHeld();       // what was held while walking was the walk's (but the four)
    emit walkingChanged(false);
    if (m_walkBeside)
    {
        m_walkBeside = false;
        m_walker = UINT_MAX;
        if (!m_besideNotice.isEmpty() && m_notice == m_besideNotice)
            showNotice(QString());
        m_besideNotice.clear();
        /* The runner kept its arm (Q252): GO works again, its pad told
           once -- unless a tool took the pad as the walk ended (the
           laptop's Focus this cue), which disarms it (toolChanged()). Its
           start comes a moment after this stop: looked at then. */
        if (m_armed)
            QTimer::singleShot(0, this, [this]() {
                if (!m_armed || m_pc == nullptr)
                    return;
                const QString tool = holder();
                if (!tool.isEmpty())
                {
                    reset();
                    showNotice(tr("Disarmed: %1 has the controller.").arg(tool));
                    return;
                }
                m_pc->padRumble(2, ControllerScheme::rumbleShortMs(), m_armUniverse);
            });
    }
    sync();
    // Ended by the emergency stop: its own line speaks.
    if (!m_endingForStop)
        m_pc->tellOperator(tr("Walk off. Run role."), ControllerScheme::ConfirmPriority);
}

void PadRun::endBesideWalk(const QString &reason)
{
    if (!m_walkBeside || !isWalking() || m_pc == nullptr)
        return;
    // Ended here, not by the walker: what was not kept goes back, and is said.
    const RefineSession *r = m_pc->refineSession();
    const int pending = r->hasPending() ? r->pendingCount() : 0;
    m_pc->refineSession()->stop();      // its stopped(): walkEnded(), the runner's GO back
    QString said = reason;
    if (pending == 1)
        said += QLatin1Char(' ') + tr("1 change not kept went back.");
    else if (pending > 1)
        said += QLatin1Char(' ') + tr("%1 changes not kept went back.").arg(pending);
    tellRefused(said);
}

bool PadRun::startWalkBeside(quint32 universe)
{
    if (m_pc == nullptr)
        return false;
    // The walker owns the walk's tool (Q135); the runner keeps its arm.
    m_walkBeside = true;
    m_walker = universe;
    m_walkerId = padId(universe);
    m_pc->claimController(universe);
    if (!startWalk())
    {
        m_walkBeside = false;
        m_walker = UINT_MAX;
        /* Let go at once, as a plain Menu walk's refusal: another pad's
           Menu already queued in this event-loop turn takes the tools,
           never a walk started under this pad's name. */
        m_pc->releaseControllerNow();
        return false;
    }
    return true;
}

bool PadRun::refuseWalk(const QString &reason, QString *why)
{
    if (why != nullptr)
        *why = reason;
    m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
    m_pc->tellOperator(reason, ControllerScheme::ConfirmPriority);
    // On the screen too: with lights-only feedback nothing is said.
    showNotice(reason);
    return false;
}

bool PadRun::startWalk(quint32 cueId, QString *why)
{
    if (m_pc == nullptr)
        return false;
    // Armed, the pad is the cue map: the walk has its own (rule 3).
    if (m_armed && !m_walkBeside)
        return refuseWalk(m_pc->padText(tr("Armed. To walk, disarm first: hold {LB} and {RB}.")), why);
    if (cueId == 0xFFFFFFFF)
        cueId = cueInHand();
    /* The cue on stage deleted (its light held until GO): there is no cue
       left to walk -- said as it is, never "nothing running". */
    /* In Operate a GO from the pad needs the arm, and an armed pad does
       not walk: the words say the whole way round. */
    if (cueId == 0xFFFFFFFF && m_doc->mode() == Doc::Operate && holdsDeletedCue())
        return refuseWalk(m_pc->padText(tr("The cue on stage was deleted: its light is held until the next GO. "
                                           "Play a cue first (GO on the laptop, or arm, GO and disarm here), "
                                           "then press {Menu}.")), why);
    if (cueId == 0xFFFFFFFF)
        return refuseWalk(m_doc->mode() == Doc::Operate
                          ? m_pc->padText(tr("Nothing running to walk. Play a cue first (GO on the laptop, or arm, "
                                             "GO and disarm here), then press {Menu}; or walk the show in Design."))
                          : tr("No cue to walk: pick one in the Show Tree."), why);
    QString reason;
    if (!tryStart(cueId, &reason))
    {
        if (!m_pc->refineSession()->isActive())
            walkEnded();
        return refuseWalk(reason, why);
    }
    walkStarted();
    return true;
}

void PadRun::stopWalk()
{
    if (!isWalking())
        return;
    // Never throws away what is not kept (gap 5): said, as a start is.
    const RefineSession *r = m_pc->refineSession();
    if (r->hasPending())
    {
        refuseWalk(m_pc->padText(r->pendingCount() == 1
                                 ? tr("1 change not kept: keep it ({A}) or go back to the cue ({B}) first.")
                                 : tr("%1 changes not kept: keep them ({A}) or go back to the cue ({B}) first.")
                                       .arg(r->pendingCount())));
        return;
    }
    // Its stopped() ends the walk (and says it).
    m_pc->refineSession()->stop();
}

bool PadRun::stepCue(int delta)
{
    if (!isWalking())
        return false;
    RefineSession *r = m_pc->refineSession();
    if (m_doc->mode() == Doc::Operate)
    {
        // Quiet Refine (Q130): only the running cue; GO moves it.
        return refuseWalk(tr("In Operate the cue moves with GO."));
    }
    const ShowTreeController *st = m_doc->showTree();
    const quint32 from = r->cueId();
    const ShowTree::CueList *list = st->listOfCue(from);
    if (list == nullptr)
        return false;
    QList<quint32> ids;
    for (const ShowTree::Cue &c : list->cues)
        ids << c.id;
    const int at = ids.indexOf(from);
    // The next cue it can walk: one with no layer on fixtures is passed over.
    for (int i = at + delta; i >= 0 && i < ids.size(); i += delta)
    {
        QString why;
        if (tryStart(ids.at(i), &why))
        {
            walkStarted();
            return true;
        }
        // Refused with changes not kept: stay on this cue, and say why.
        if (r->isActive())
            return refuseWalk(why);
        // That cue had nothing to walk and Focus this cue let go: back.
        if (!tryStart(from))
        {
            walkEnded();
            // No walk now: its owner goes once this press is done (and is said).
            m_pc->releaseControllerSoon();
            return false;
        }
    }
    return refuseWalk(delta > 0 ? tr("Last cue.") : tr("First cue."));
}

void PadRun::stepLayer(int delta)
{
    if (!isWalking())
        return;
    RefineSession *r = m_pc->refineSession();
    delta > 0 ? r->nextLayer() : r->previousLayer();
}

bool PadRun::setSolo(bool on)
{
    if (on && !isWalking())
        return false;
    if (on && m_doc->mode() == Doc::Operate)
    {
        // Quiet Refine (Q130): the show is running; nothing goes dark.
        m_pc->tellOperator(tr("Solo is for Design: the show is running."), ControllerScheme::ConfirmPriority);
        return false;
    }
    if (on != m_solo)
    {
        m_solo = on;
        syncSolo();
        emit soloChanged(on);
        if (isWalking())
            m_pc->tellOperator(on ? tr("Solo. %1").arg(m_pc->refineSession()->wherePhrase())
                                  : tr("Whole cue."), ControllerScheme::ConfirmPriority);
    }
    if (!on && m_doc->showTree() != nullptr)
        m_doc->showTree()->setPreviewSolo(0xFFFFFFFF, 0xFFFFFFFF);
    return true;
}

void PadRun::syncSolo()
{
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st == nullptr)
        return;
    if (!m_solo || !isWalking())
    {
        if (st->previewSoloLayer() != 0xFFFFFFFF)
            st->setPreviewSolo(0xFFFFFFFF, 0xFFFFFFFF);
        return;
    }
    const RefineSession *r = m_pc->refineSession();
    st->setPreviewSolo(r->cueId(), r->currentLayer());
}

bool PadRun::toggleNeedsWork()
{
    if (!isWalking())
        return false;
    ShowTreeController *st = m_doc->showTree();
    const quint32 cueId = walkCue();
    const bool on = !st->isCueNeedsWork(cueId);
    if (!st->setCuesNeedsWork({ cueId }, on))
        return false;
    m_pc->padRumble(on ? 2 : 1, ControllerScheme::rumbleShortMs());
    m_pc->tellOperator(on ? tr("Cue %1: needs work.").arg(cueLabel(cueId))
                          : tr("Cue %1: needs work cleared.").arg(cueLabel(cueId)),
                       ControllerScheme::ConfirmPriority);
    return true;
}

void PadRun::releaseModifier()
{
    const bool was = m_lbHeld;
    m_lbHeld = false;
    m_levelUp = m_levelDown = 0.0f;
    if (m_pc == nullptr)
        return;
    RefineSession *r = m_pc->refineSession();
    r->setLightMode(false);
    if (m_levelMoved && was)
    {
        const QString level = r->levelPhrase();
        if (!level.isEmpty())
            m_pc->tellOperator(level, ControllerScheme::ConfirmPriority);
    }
    m_levelMoved = false;
}

bool PadRun::walkTriggerDown(quint32 universe, bool right) const
{
    for (auto it = m_walkTriggers.constBegin(); it != m_walkTriggers.constEnd(); ++it)
        if ((right ? it->rt : it->lt).isDown() && sameController(it.key(), universe))
            return true;
    return false;
}

bool PadRun::handleWalk(GamepadRole::Role role, uchar value, quint32 universe)
{
    if (!isWalking())
        return false;
    RefineSession *r = m_pc->refineSession();
    const bool down = value > 0;
    /* LB held is the modifier: anything done with it makes LB no tap. */
    if (m_lbHeld && role != GamepadRole::LB)
    {
        if (!GamepadRole::isAxis(role) && down)
            m_lbUsed = true;
        else if (role == GamepadRole::LeftX || role == GamepadRole::LeftY
                 || role == GamepadRole::RightX || role == GamepadRole::RightY)
        {
            if (qAbs(value / 255.0f - 0.5f) > 0.25f)
                m_lbUsed = true;
        }
        // Hold LB + A: keep the light itself (else A is Focus this cue's keep).
        if (role == GamepadRole::A && down && r->hasLightPending())
        {
            r->keepLight();
            return true;
        }
    }
    switch (walkActionFor(role))
    {
        case NextCue:
        case PreviousCue:
        {
            const bool right = role == GamepadRole::RightTrigger;
            // This line's reading; a step when the pad (any line) goes down.
            const bool was = walkTriggerDown(universe, right);
            WalkTriggers &line = m_walkTriggers[universe];
            (right ? line.rt : line.lt).update(value / 255.0f);
            const bool pressed = !was && walkTriggerDown(universe, right);
            if (m_lbHeld)
            {
                // With LB held the triggers are the level (walkTick()), not cues.
                (right ? m_levelUp : m_levelDown) = value / 255.0f;
                if (value / 255.0f > 0.05f)
                    m_lbUsed = true;
                return true;
            }
            if (pressed)
                stepCue(right ? 1 : -1);
            return true;
        }
        case PreviousLayer:
            // LB: held, the modifier; a tap on its own steps back on release.
            if (down)
            {
                if (!m_lbHeld)
                {
                    m_lbHeld = true;
                    m_lbUsed = false;
                    m_lightAsksAt = r->lightAsks();
                    m_levelMoved = false;
                    r->setLightMode(true);
                }
                return true;
            }
            if (!m_lbHeld)
                return true;
            {
                const bool tap = !m_lbUsed && r->lightAsks() == m_lightAsksAt;
                releaseModifier();
                if (tap)
                {
                    // At Group or Fixture scope LB steps those, as in Focus this cue.
                    if (r->scope() == RefineSession::LayerScope)
                        stepLayer(-1);
                    else
                        r->previousItem();
                }
            }
            return true;
        case NextLayer:
            // At Group or Fixture scope the bumpers are Focus this cue's.
            if (r->scope() != RefineSession::LayerScope)
                return false;
            if (down)
                stepLayer(1);
            return true;
        case TagCue:
            // A tap is the scope (on the release); held, the tag (walkTick()).
            if (down)
                m_holdY.press();
            else if (m_holdY.release())
                r->cycleScope();
            return true;
        case LeaveWalk:
            if (down)
                stopWalk();
            return true;
        case Solo:          // View: ProgrammerController's (its chords)
        case WalkNoAction:
            break;
    }
    return false;
}

void PadRun::walkTick(double dt)
{
    if (!isWalking())
        return;
    if (m_holdY.due(dt))
        toggleNeedsWork();
    if (!m_lbHeld)
        return;
    // Level: the triggers' pull with LB held, as fast as they are pulled.
    const float drive = m_levelUp - m_levelDown;
    RefineSession *r = m_pc->refineSession();
    if (qAbs(drive) > 0.05f)
    {
        if (r->nudgeLevel(drive * levelRate() * float(dt)))
            m_levelMoved = true;
    }
    else if (m_levelMoved)
    {
        // Let go of the trigger: the level said once.
        const QString level = r->levelPhrase();
        if (!level.isEmpty())
            m_pc->tellOperator(level, ControllerScheme::ConfirmPriority);
        m_levelMoved = false;
    }
}
