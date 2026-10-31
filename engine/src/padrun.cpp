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
        case NoAction:     break;
    }
    return QString();
}

QList<PadRun::Action> PadRun::actions()
{
    return { Arm, Go, Back, SubGo, Stop, NextList, PreviousList, WhereAmI };
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
