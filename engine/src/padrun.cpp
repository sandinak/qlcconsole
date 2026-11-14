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
#include <climits>

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "refinesession.h"
#include "focussession.h"
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
    m_armChord = new QTimer(this);
    m_armChord->setSingleShot(true);
    m_armChord->setInterval(armChordMs());
    connect(m_armChord, &QTimer::timeout, this, &PadRun::armChordFired);
    m_blink = new QTimer(this);
    m_blink->setInterval(blinkMs());
    connect(m_blink, &QTimer::timeout, this, &PadRun::lightTick);
    /* Stopped ends when the show is run again from anywhere (the keys, the
       Run tab): the pad's red would otherwise outlast what it reports. */
    if (m_doc != nullptr && m_doc->showControl() != nullptr)
        connect(m_doc->showControl(), &ShowControl::commandHandled, this, [this](const QString &, bool done) {
            if (m_stopped && !m_stopping && done)
            {
                m_stopped = false;
                sync();
            }
        });
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

/*****************************************************************************
 * The map
 *****************************************************************************/

PadRun::Action PadRun::actionFor(GamepadRole::Role role)
{
    switch (role)
    {
        case GamepadRole::LB:           return Arm;
        case GamepadRole::A:            return Go;
        case GamepadRole::B:            return Back;
        case GamepadRole::X:            return SubGo;
        case GamepadRole::Y:            return SubBack;
        case GamepadRole::RB:           return FireHit;
        case GamepadRole::RightTrigger: return HoldMain;
        case GamepadRole::LeftTrigger:  return HoldSub;
        case GamepadRole::RS:           return WhereAmI;
        case GamepadRole::Start:        return Walk;
        default:                        break;
    }
    return NoAction;
}

GamepadRole::Role PadRun::roleFor(Action action)
{
    for (int r = GamepadRole::A; r <= GamepadRole::TouchClick; ++r)
        if (action != NoAction && actionFor(GamepadRole::Role(r)) == action)
            return GamepadRole::Role(r);
    return GamepadRole::None;
}

bool PadRun::needsArm(Action action)
{
    return action == Go || action == Back || action == SubGo || action == SubBack || action == FireHit;
}

PadRun::Guard PadRun::guardFor(Action action)
{
    switch (action)
    {
        case Go: case Back:         return RightGuard;
        case SubGo: case SubBack:   return LeftGuard;
        case FireHit:               return EitherGuard;
        default:                    break;
    }
    return NoGuard;
}

QString PadRun::label(Action action)
{
    switch (action)
    {
        case Arm:           return tr("With {RB}, hold: arm, and the same to disarm");
        case Go:            return tr("GO (armed, {RT} held)");
        case Back:          return tr("Back (armed, {RT} held)");
        case SubGo:         return tr("Sub GO (armed, {LT} held)");
        case SubBack:       return tr("Sub Back (armed, {LT} held)");
        case FireHit:       return tr("Fire the armed hit (armed, {LT} or {RT} held); with {LB}, hold: arm");
        case HoldMain:      return tr("Hold for GO and Back");
        case HoldSub:       return tr("Hold for Sub GO and Sub Back");
        case WhereAmI:      return tr("Say the list and what's next");
        case Walk:          return tr("Walk the show: cues, layers, adjust, tag (not armed)");
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
    return { Arm, FireHit, HoldMain, Go, Back, HoldSub, SubGo, SubBack, WhereAmI, Walk };
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
    reset();
    m_stopped = false;
    m_enabled = on;
    emit enabledChanged(on);
    sync();
    if (say && m_pc != nullptr)
        m_pc->tellOperator(on ? m_pc->padText(tr("Run role. Hold {LB} and {RB} to arm."))
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

    const bool down = value > 0;
    const bool chordControl = role == GamepadRole::LB || role == GamepadRole::RB
                              || role == GamepadRole::LeftTrigger || role == GamepadRole::RightTrigger;
    Held &h = m_held[universe];
    switch (role)
    {
        case GamepadRole::LB:           h.lb = down; break;
        case GamepadRole::RB:           h.rb = down; break;
        case GamepadRole::LeftTrigger:  h.lt.update(value / 255.0f); break;
        case GamepadRole::RightTrigger: h.rt.update(value / 255.0f); break;
        default:                        break;
    }

    if (chordControl)
    {
        /* All four: the emergency stop, armed or not, once a chord. */
        if (h.all())
        {
            if (!h.stopFired)
            {
                h.stopFired = true;
                emergencyStop();
            }
            return true;
        }
        h.stopFired = false;
        /* Both bumpers and no trigger, held: the arm changes. Anything
           else of the four ends the timing. */
        armChordUpdate(universe);
        // RB with a trigger held is the hit (below); the rest is done.
        if (!(role == GamepadRole::RB && down && h.anyTrigger()))
            return true;
    }

    const Action action = actionFor(role);
    // Everything else acts on the press; the rest of the map is swallowed.
    if (!down || action == NoAction)
        return true;

    if (needsArm(action))
    {
        if (!m_armed)
        {
            refuse(action, tr("Not armed: hold {LB} and {RB}."));
            return true;
        }
        if (universe != m_armUniverse)
        {
            refuse(action, tr("Another controller is armed."));
            return true;
        }
        const Guard guard = guardFor(action);
        if (guard == RightGuard && !h.rt.isDown())
        {
            refuse(action, action == Go ? tr("GO is {RT} held and {A}.") : tr("Back is {RT} held and {B}."));
            return true;
        }
        if (guard == LeftGuard && !h.lt.isDown())
        {
            refuse(action, action == SubGo ? tr("Sub GO is {LT} held and {X}.")
                                           : tr("Sub Back is {LT} held and {Y}."));
            return true;
        }
        if (guard == EitherGuard && !h.anyTrigger())
            return true;        // RB alone: the arm chord's, or nothing
    }
    trigger(action);
    return true;
}

/*****************************************************************************
 * The arm
 *****************************************************************************/

void PadRun::setArmChordMs(int ms)
{
    m_armChord->setInterval(ms);
}

bool PadRun::isArmChordPending() const
{
    return m_armChord->isActive();
}

void PadRun::armChordUpdate(quint32 universe)
{
    const Held h = m_held.value(universe);
    const bool chord = h.lb && h.rb && !h.anyTrigger();
    if (chord)
    {
        // Timed from the second bumper down; a held chord is not restarted.
        if (!m_armChord->isActive() || m_chordUniverse != universe)
        {
            m_chordUniverse = universe;
            m_armChord->start();
        }
    }
    else if (m_chordUniverse == universe)
    {
        m_armChord->stop();
        m_chordUniverse = UINT_MAX;
    }
    /* Both bumpers with one trigger down: no chord, and the screen says
       why -- a trigger that does not come back up would otherwise make a
       pad that "will not arm" with nothing to show for it. */
    if (h.lb && h.rb && h.anyTrigger() && !h.all() && m_pc != nullptr)
        showNotice(m_pc->padText(tr("To arm, let go of the triggers.")));
}

void PadRun::armChordFired()
{
    const quint32 u = m_chordUniverse;
    m_chordUniverse = UINT_MAX;
    const Held h = m_held.value(u);
    if (!m_enabled || !h.lb || !h.rb || h.anyTrigger())
        return;
    // Another pad's arm is not this one's to drop.
    if (m_armed && u != m_armUniverse)
    {
        refuse(Arm, tr("Another controller is armed."));
        return;
    }
    setArmed(!m_armed, u);
}

bool PadRun::setArmed(bool on, quint32 universe)
{
    if (on == m_armed)
        return true;
    if (on && (!m_enabled || isWalking() || !holder().isEmpty()))
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
            m_pc->padRumble(2, ControllerScheme::rumbleShortMs());
        else
            m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
        m_pc->tellOperator(on ? m_pc->padText(tr("Armed. {RT} and {A} is GO.")) : tr("Disarmed."),
                           ControllerScheme::ConfirmPriority);
    }
    return true;
}

void PadRun::emergencyStop()
{
    m_armChord->stop();
    m_chordUniverse = UINT_MAX;
    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    bool done = false;
    if (sc != nullptr)
    {
        ShowControl::Request r;
        r.command = ShowControl::AllOff;
        m_stopping = true;      // its own commandHandled() does not end Stopped
        done = sc->run(r, ShowControl::RoutePad);
        m_stopping = false;
    }
    const bool was = m_armed;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    m_stopped = true;
    m_playing = false;
    m_flash->stop();
    emit commandSent(ShowControl::AllOff, done);
    if (was)
        emit armedChanged(false);
    sync();
    if (m_pc != nullptr)
    {
        m_pc->padRumble(3, ControllerScheme::rumbleLongMs());
        const QString said = tr("Emergency stop. Everything is off.");
        m_pc->tellOperator(said, ControllerScheme::ConfirmPriority);
        showNotice(said);
    }
}

/*****************************************************************************
 * The state
 *****************************************************************************/

PadRun::State PadRun::state() const
{
    if (!m_enabled)
        return Tools;
    if (isWalking())
        return Walking;
    if (!holder().isEmpty())
        return ToolHasPad;
    if (m_stopped)
        return Stopped;
    return m_armed ? Armed : Run;
}

QColor PadRun::stateColour() const
{
    switch (state())
    {
        case Armed:     return armedColour();
        case Stopped:   return stoppedColour();
        default:        break;
    }
    return QColor();
}

QColor PadRun::stateInk() const
{
    const QColor c = stateColour();
    if (!c.isValid())
        return QColor();
    return c.lightnessF() > 0.5f || c == armedColour() ? QColor(Qt::black) : QColor(Qt::white);
}

QColor PadRun::lightColour() const
{
    switch (state())
    {
        case Armed:     return m_playing && m_blinkOn ? playingColour() : armedColour();
        case Stopped:   return stoppedColour();
        default:        break;
    }
    return QColor();
}

void PadRun::toolChanged()
{
    /* A tool started on the laptop takes the pad: the arm drops with it
       (no button did that), so it is not found armed when the tool ends. */
    if (m_armed && !holder().isEmpty())
    {
        reset();
        showNotice(tr("Disarmed: %1 has the controller.").arg(holder()));
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
    if (int(now) != m_shownState)
    {
        m_shownState = int(now);
        emit stateChanged();
    }
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
}

QString PadRun::holder() const
{
    if (m_pc == nullptr || isWalking())
        return QString();
    const FocusSession *f = m_pc->focusSession();
    if (f->isActive())
        return f->isRunningCalls() ? tr("Venue Focus") : tr("Quick Focus");
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
        case Armed:         return tr("Pad: ARMED");
        case Stopped:       return tr("Pad: STOPPED");
        case Walking:       return tr("Pad: Walk");
        case ToolHasPad:    return tr("Pad: %1").arg(holder());
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
    if (!tool.isEmpty())
        out += tr("<p><b>%1 has the controller</b> until it ends (finish or cancel it on the laptop); "
                  "then the controller runs the show again.</p>").arg(tool.toHtmlEscaped());
    if (!m_enabled)
        out += tr("<p>Tools: Focus Mode, Venue Focus, Focus This Cue and the follow spot. Click the "
                  "Run tab's Pad: button, or hold %1 + %2 for a second, to run the show from the "
                  "controller.</p>")
                   .arg(m_pc->padText(QStringLiteral("{View}")).toHtmlEscaped(),
                        m_pc->padText(QStringLiteral("{Menu}")).toHtmlEscaped());
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
               tr("Hold 1 s: switch between Run and Tools"));
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
    roleChordCancel();
    m_armChord->stop();
    m_chordUniverse = UINT_MAX;
    m_held.clear();
    m_swipe.reset();
    m_flash->stop();
    const bool was = m_armed;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    if (was)
        emit armedChanged(false);
    sync();
}

void PadRun::controllerLost()
{
    const bool was = m_armed;
    reset();
    if (was)
        showNotice(tr("Disarmed: the controller went away."));
}

void PadRun::showLight()
{
    if (m_pc == nullptr)
        return;
    // A tool on the pad has its own lamp (the save colours).
    if (m_pc->focusSession()->isActive() || m_pc->refineSession()->isActive())
        return;
    m_pc->setPadLight(lightColour());
}

/*****************************************************************************
 * Triggers and swipes
 *****************************************************************************/

PadTriggers::State PadRun::triggerState() const
{
    return PadTriggers::forState(m_enabled, isArmed(), isWalking());
}

void PadRun::updateTriggers(bool force)
{
    if (m_pc != nullptr)
        m_pc->setPadTriggers(triggerState(), force);
}

void PadRun::touchValue(GamepadRole::Role role, float norm)
{
    if (m_enabled)
        m_swipe.setValue(role, norm);
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
    if (!startWalk())
        return false;
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
        case Arm: case HoldMain: case HoldSub: case NoAction:
            return false;
    }

    ShowControl *sc = m_doc != nullptr ? m_doc->showControl() : nullptr;
    if (sc == nullptr)
        return false;
    ShowControl::Request r;
    r.command = commandFor(action);
    r.chaser = listChaser();
    // The one GO path: the App's guard decides, and says what it held.
    const bool done = sc->run(r, ShowControl::RoutePad);
    emit commandSent(r.command, done);
    if (m_pc == nullptr)
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
    const quint32 pick = tier.at(m_hitStep % tier.count());
    Function *f = m_doc->function(st->lookFunction(pick));
    if (f == nullptr)
        return refuse(tr("No hit armed."));
    ++m_hitStep;
    f->start(m_doc->masterTimer(), FunctionParent::master());
    const ShowTree::Cue *c = st->cue(pick);
    m_pc->padRumble(1, ControllerScheme::rumbleShortMs());
    m_pc->tellOperator(tr("%1.").arg(c != nullptr ? c->name : f->name()), ControllerScheme::ConfirmPriority);
    emit hitFired(pick, true);
    return true;
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
    if (m_chordPending || m_chordFired)
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
    reset();
    m_chordFired = true;
    const bool on = !m_enabled;
    setEnabled(on, false);
    if (m_pc == nullptr)
        return;
    m_pc->padRumble(2, ControllerScheme::rumbleShortMs());
    m_pc->tellOperator(on ? m_pc->padText(tr("Run role. Hold {LB} and {RB} to arm."))
                          : tr("Tools role."),
                       ControllerScheme::ConfirmPriority);
}

quint32 PadRun::listChaser() const
{
    // The Run tab's list (its picker, kept with the show), else the main one.
    const ControlMap *cm = m_doc != nullptr ? m_doc->controlMap() : nullptr;
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (cm == nullptr || st == nullptr)
        return UINT_MAX;
    const ShowTree::CueList *l = st->cueList(cm->runTabList());
    if (l == nullptr || l->looks || l->isSubCues() || qobject_cast<Chaser *>(m_doc->function(l->chaser)) == nullptr)
        return UINT_MAX;
    return l->chaser;
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
    // The walking map has no GO, and no arm (a walk never starts armed).
    const bool wasArmed = m_armed;
    m_armed = false;
    m_armUniverse = UINT_MAX;
    m_armChord->stop();
    m_chordUniverse = UINT_MAX;
    m_held.clear();
    m_walkRt.reset();
    m_walkLt.reset();
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
    m_walkRt.reset();
    m_walkLt.reset();
    setSolo(false);
    m_held.clear();     // what was held while walking was the walk's
    emit walkingChanged(false);
    sync();
    m_pc->tellOperator(tr("Walk off. Run role."), ControllerScheme::ConfirmPriority);
}

bool PadRun::refuseWalk(const QString &reason, QString *why)
{
    if (why != nullptr)
        *why = reason;
    m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
    m_pc->tellOperator(reason, ControllerScheme::ConfirmPriority);
    return false;
}

bool PadRun::startWalk(quint32 cueId, QString *why)
{
    if (m_pc == nullptr)
        return false;
    // Armed, the pad is the cue map: the walk has its own (rule 3).
    if (m_armed)
        return refuseWalk(m_pc->padText(tr("Armed. To walk, disarm first: hold {LB} and {RB}.")), why);
    if (cueId == 0xFFFFFFFF)
        cueId = cueInHand();
    if (cueId == 0xFFFFFFFF)
        return refuseWalk(m_doc->mode() == Doc::Operate
                          ? tr("Nothing running to walk: GO first, or walk the show in Design.")
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

bool PadRun::handleWalk(GamepadRole::Role role, uchar value)
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
            ControllerScheme::TriggerButton &t = right ? m_walkRt : m_walkLt;
            const bool pressed = t.update(value / 255.0f);
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
