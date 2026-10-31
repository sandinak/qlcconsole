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

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "refinesession.h"
#include "focussession.h"
#include "showcontrol.h"
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
    connect(m_flash, &QTimer::timeout, this, &PadRun::showLight);
    if (m_pc != nullptr)
    {
        RefineSession *r = m_pc->refineSession();
        // Focus this cue ended (Menu, its B held, the laptop, a new show).
        connect(r, &RefineSession::stopped, this, [this]() {
            if (!m_restarting)
                walkEnded();
        });
        // The solo follows the layer walked.
        connect(r, &RefineSession::changed, this, &PadRun::syncSolo);
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
        case GamepadRole::Y:            return SubGo;
        case GamepadRole::X:            return Stop;
        // The triggers step the big unit, as in every tool (a focus call, a layer).
        case GamepadRole::RightTrigger: return NextList;
        case GamepadRole::LeftTrigger:  return PreviousList;
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
    return action == Go || action == Back || action == SubGo || action == Stop;
}

QString PadRun::label(Action action)
{
    switch (action)
    {
        case Arm:          return tr("Hold to arm GO, Back, Sub GO, Stop");
        case Go:           return tr("GO ({LB} held)");
        case Back:         return tr("Back ({LB} held)");
        case SubGo:        return tr("Sub GO ({LB} held)");
        case Stop:         return tr("Stop the list ({LB} held)");
        case NextList:     return tr("Next cue list");
        case PreviousList: return tr("Previous cue list");
        case WhereAmI:     return tr("Say the list and what's next");
        case Walk:         return tr("Walk the show: cues, layers, adjust, tag");
        case NoAction:     break;
    }
    return QString();
}

QList<PadRun::Action> PadRun::actions()
{
    return { Arm, Go, Back, SubGo, Stop, NextList, PreviousList, WhereAmI, Walk };
}

QStringList PadRun::mapLines(PadNames::Family family)
{
    QStringList out;
    for (Action a : actions())
        out << QStringLiteral("%1: %2").arg(PadNames::name(roleFor(a), family),
                                            PadNames::expand(label(a), family));
    return out;
}

ShowControl::Command PadRun::commandFor(Action action)
{
    switch (action)
    {
        case Back:  return ShowControl::Back;
        case SubGo: return ShowControl::SubGo;
        case Stop:  return ShowControl::Stop;
        default:    break;
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
    m_enabled = on;
    emit enabledChanged(on);
    if (say && m_pc != nullptr)
        m_pc->tellOperator(on ? m_pc->padText(tr("Run role. Hold {LB} and press {A} to GO."))
                              : tr("Run role off."),
                           ControllerScheme::ConfirmPriority);
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

    const Action action = actionFor(role);

    if (role == GamepadRole::LeftTrigger || role == GamepadRole::RightTrigger)
    {
        ControllerScheme::TriggerButton &t = role == GamepadRole::RightTrigger ? m_rt : m_lt;
        if (t.update(value / 255.0f))
            trigger(action);
        return true;
    }

    const bool down = value > 0;
    if (action == Arm)
    {
        const bool was = isArmed();
        if (down)
            m_armedOn.insert(universe);
        else
            m_armedOn.remove(universe);
        if (was != isArmed())
        {
            showLight();
            emit armedChanged(isArmed());
        }
        return true;
    }

    // Everything else acts on the press; the rest of the map is swallowed.
    if (!down || action == NoAction)
        return true;

    if (needsArm(action) && !m_armedOn.contains(universe))
    {
        emit refused(action);
        if (m_pc != nullptr)
            m_pc->tellOperator(m_pc->padText(tr("Hold {LB} to arm.")), ControllerScheme::ConfirmPriority);
        return true;
    }
    trigger(action);
    return true;
}

void PadRun::reset()
{
    m_rt.reset();
    m_lt.reset();
    if (m_armedOn.isEmpty())
        return;
    m_armedOn.clear();
    showLight();
    emit armedChanged(false);
}

void PadRun::showLight()
{
    if (m_pc == nullptr || m_flash->isActive())
        return;
    // A tool on the pad has its own lamp (the save colours).
    if (m_pc->focusSession()->isActive() || m_pc->refineSession()->isActive())
        return;
    m_pc->setPadLight(isArmed() ? armedColour() : QColor());
}

/*****************************************************************************
 * Commands
 *****************************************************************************/

bool PadRun::trigger(Action action)
{
    switch (action)
    {
        case NextList:
            stepList(1);
            return true;
        case PreviousList:
            stepList(-1);
            return true;
        case WhereAmI:
            if (m_pc != nullptr)
                m_pc->tellOperator(wherePhrase(), ControllerScheme::WhereNowPriority);
            return true;
        case Walk:
            return startWalk();
        case Go: case Back: case SubGo: case Stop:
            break;
        case Arm: case NoAction:
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
        if (action != Stop)
        {
            m_flash->stop();
            m_pc->setPadLight(wentColour());
            m_flash->start();
        }
    }
    else
    {
        m_pc->padRumble(1, ControllerScheme::rumbleLongMs());
        m_pc->tellOperator(tr("%1: nothing to play.").arg(ShowControl::commandName(r.command)),
                           ControllerScheme::ConfirmPriority);
    }
    return done;
}

QList<quint32> PadRun::lists() const
{
    QList<quint32> out;
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st == nullptr)
        return out;
    const quint32 current = st->currentProduction();
    bool any = false;
    for (const ShowTree::Production &p : st->productions())
        any = any || p.id == current;
    for (const ShowTree::Production &p : st->productions())
    {
        if (any && p.id != current)
            continue;
        for (const ShowTree::CueList &l : p.lists)
            if (!l.looks && !l.isSubCues() && l.chaser != 0xFFFFFFFF
                && qobject_cast<Chaser *>(m_doc->function(l.chaser)) != nullptr)
                out << l.chaser;
    }
    return out;
}

quint32 PadRun::listChaser() const
{
    if (m_list == UINT_MAX || !lists().contains(m_list))
        return UINT_MAX;
    return m_list;
}

void PadRun::stepList(int delta)
{
    const QList<quint32> all = lists();
    if (all.isEmpty())
    {
        if (m_pc != nullptr)
            m_pc->tellOperator(tr("No cue lists."), ControllerScheme::ConfirmPriority);
        return;
    }
    quint32 at = listChaser();
    if (at == UINT_MAX && m_doc->showControl() != nullptr)
        at = m_doc->showControl()->mainCueList();
    const int n = all.size();
    const int i = all.indexOf(at);
    const int next = i < 0 ? (delta > 0 ? 0 : n - 1) : ((i + delta) % n + n) % n;
    m_list = all.at(next);
    emit listChanged(m_list);
    if (m_pc != nullptr)
        m_pc->tellOperator(tr("%1.").arg(listName(m_list)), ControllerScheme::WherePriority);
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
        case PreviousLayer: return tr("Previous layer");
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
    // The picked list (else the main one), as a Show Tree list.
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
    // The arm lets go: the walking map has no GO.
    const bool wasArmed = isArmed();
    m_armedOn.clear();
    m_rt.reset();
    m_lt.reset();
    m_walkRt.reset();
    m_walkLt.reset();
    m_holdY.reset();
    if (wasArmed)
        emit armedChanged(false);
    if (!m_walking)
    {
        m_walking = true;
        emit walkingChanged(true);
    }
    syncSolo();
}

void PadRun::walkEnded()
{
    if (!m_walking)
        return;
    m_walking = false;
    m_pc->refineSession()->setWalking(false);
    m_holdY.reset();
    m_walkRt.reset();
    m_walkLt.reset();
    setSolo(false);
    emit walkingChanged(false);
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

bool PadRun::handleWalk(GamepadRole::Role role, uchar value)
{
    if (!isWalking())
        return false;
    RefineSession *r = m_pc->refineSession();
    const bool down = value > 0;
    switch (walkActionFor(role))
    {
        case NextCue:
        case PreviousCue:
        {
            ControllerScheme::TriggerButton &t = role == GamepadRole::RightTrigger ? m_walkRt : m_walkLt;
            if (t.update(value / 255.0f))
                stepCue(role == GamepadRole::RightTrigger ? 1 : -1);
            return true;
        }
        case NextLayer:
        case PreviousLayer:
            // At Group or Fixture scope the bumpers are Focus this cue's.
            if (r->scope() != RefineSession::LayerScope)
                return false;
            if (down)
                stepLayer(role == GamepadRole::RB ? 1 : -1);
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
    if (isWalking() && m_holdY.due(dt))
        toggleNeedsWork();
}
