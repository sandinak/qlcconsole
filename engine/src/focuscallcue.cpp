/*
  Q Light Controller Plus
  focuscallcue.cpp

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

#include "showtreecontroller.h"
#include "venuecorrections.h"
#include "focuscallcue.h"
#include "focussession.h"
#include "mastertimer.h"
#include "focuscalls.h"
#include "chaser.h"
#include "doc.h"

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;
}

FocusCallCue::FocusCallCue(Doc *doc, FocusSession *session, FocusCalls *calls, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_session(session)
    , m_calls(calls)
{
    /* A cue list playing: watch its steps. The MasterTimer says so from its
       own thread; everything here is done on the main one. */
    if (m_doc != nullptr && m_doc->masterTimer() != nullptr)
    {
        connect(m_doc->masterTimer(), &MasterTimer::functionStarted, this,
                [this](quint32 id) { watch(id); }, Qt::QueuedConnection);
        connect(m_doc->masterTimer(), &MasterTimer::functionStopped, this, [this](quint32 id) {
            if (m_state == Armed && id == chaserOf(m_list))
                disarm();
            // Done, and the list stopped: nothing waits on the cue now.
            if (m_finishedList != kInvalid && id == chaserOf(m_finishedList))
            {
                m_finished = m_finishedList = kInvalid;
                setDone(false);
            }
        }, Qt::QueuedConnection);
    }
    if (m_session != nullptr)
    {
        connect(m_session, &FocusSession::stopped, this, &FocusCallCue::sessionStopped);
    }
    // Nobody named the venue in time: today's date (Q120).
    m_askTimer = new QTimer(this);
    m_askTimer->setSingleShot(true);
    connect(m_askTimer, &QTimer::timeout, this, [this]() { useVenue(QString()); });
    if (m_doc != nullptr)
        connect(m_doc, &Doc::clearing, this, [this]() {
            m_watched.clear();
            m_cue = m_list = kInvalid;
            m_finished = m_finishedList = kInvalid;
            setState(Idle);
            setDone(false);
        });
}

FocusCallCue::~FocusCallCue()
{
}

QString FocusCallCue::armedPhrase()
{
    return tr("Venue focus ready. Press Start to begin.");
}

QString FocusCallCue::readyPhrase() const
{
    if (!m_runRole)
        return armedPhrase();
    /* The Run role (PadRun rule 6): LT + Menu -- from the arm on a pad that
       runs cues, with no arm on a Focus pad that has none to give (Q250). */
    switch (m_startProbe ? m_startProbe() : 0)
    {
        case 1:     return tr("Venue focus ready. Hold {LT} and press {Menu} to begin.");
        case 2:     return tr("Venue focus ready. No controller does focus: start it on the laptop.");
        default:    break;
    }
    return tr("Venue focus ready. Armed, hold {LT} and press {Menu} to begin.");
}

void FocusCallCue::setRunRole(bool on)
{
    m_runRole = on;
}

void FocusCallCue::setDone(bool done)
{
    if (done == m_done)
        return;
    m_done = done;
    emit doneChanged(done);
}

quint32 FocusCallCue::chaserOf(quint32 listId) const
{
    const ShowTree::CueList *l = m_doc->showTree()->cueList(listId);
    return l != nullptr ? l->chaser : kInvalid;
}

void FocusCallCue::watch(quint32 functionId)
{
    quint32 listId = kInvalid;
    if (m_doc->showTree()->productionOfChaser(functionId, &listId) == kInvalid || listId == kInvalid)
        return;
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(functionId));
    if (chaser == nullptr)
        return;
    bool known = false;
    for (const QPointer<Chaser> &c : m_watched)
        known = known || c == chaser;
    if (!known)
    {
        m_watched << chaser;
        connect(chaser, &Chaser::currentStepChanged, this,
                [this, functionId](int) { stepped(functionId); }, Qt::QueuedConnection);
    }
    stepped(functionId);
}

void FocusCallCue::stepped(quint32 chaserId)
{
    quint32 listId = kInvalid;
    if (m_doc->showTree()->productionOfChaser(chaserId, &listId) == kInvalid)
        return;
    cueCurrent(listId, m_doc->showTree()->runningCue(listId));
}

void FocusCallCue::cueCurrent(quint32 listId, quint32 cueId)
{
    if (m_state == Running)
        return;     // the calls run on; the list waits for them
    const bool focus = cueId != kInvalid && m_doc->showTree()->isCueFocusCall(cueId);
    if (cueId != m_finished && m_finished != kInvalid && listId == m_finishedList)
    {
        // The list moved off the cue that was done: the next GO went.
        m_finished = m_finishedList = kInvalid;
        setDone(false);
    }
    if (!focus)
    {
        if (m_state == Armed && listId == m_list)
            disarm();
        return;
    }
    if (m_state == Armed && m_cue == cueId)
        return;
    // Just done with it: the list is going on from it, not arming it again.
    if (cueId == m_finished)
        return;
    m_cue = cueId;
    m_list = listId;
    setState(Armed);
    // The crew's cue, through the headsets: nothing moves until Start.
    emit announce(readyPhrase());
}

bool FocusCallCue::start(QString *why)
{
    auto refuse = [this, why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        emit announce(reason, ControllerScheme::ImportantPriority);
        return false;
    };
    if (m_state != Armed)
        return false;
    // No calls of the show's own: the ones its targets make.
    if (m_calls == nullptr || !m_calls->ensureCalls())
        return refuse(m_calls != nullptr && !m_calls->hasOrCanBuild() ? m_calls->whyNone()
                                                                      : tr("This show has no focus calls yet."));
    bool anyLight = false;
    for (const FocusCall &c : m_calls->calls())
        anyLight = anyLight || !FocusCalls::fixturesOf(m_doc, c).isEmpty();
    if (!anyLight)
        return refuse(tr("The focus calls have no lights: their groups are empty."));
    /* Saves go to a venue, and a show moves between rooms: ALWAYS confirm
       it on the laptop (Q120, Q129) -- the one applied, or a name -- and if
       nobody answers in a few seconds, a new venue for today, so last
       week's room is never corrected by mistake. */
    VenueCorrections *venues = m_session->venues();
    if (venues != nullptr && !venues->filePath().isEmpty())
    {
        setState(NamingVenue);
        const QString current = venues->activeVenue();
        emit announce(current.isEmpty()
                      ? tr("Name this venue on the laptop, or wait for today's date.")
                      : tr("Still at %1? Hold {A} on the pad for yes, or answer on the laptop, or wait for a new venue.").arg(current),
                      ControllerScheme::ImportantPriority);
        emit venueNeeded(VenueCorrections::defaultVenueName(), m_askTimeout, current);
        m_askTimer->start(m_askTimeout);
        return true;
    }
    return beginCalls(why);
}

bool FocusCallCue::useVenue(const QString &name)
{
    if (m_state != NamingVenue)
        return false;
    m_askTimer->stop();
    VenueCorrections *venues = m_session->venues();
    const QString wanted = name.trimmed();
    if (!wanted.isEmpty() && wanted == venues->activeVenue())
    {
        venues->confirmVenue();
        emit announce(tr("Still at %1.").arg(wanted));
    }
    else
    {
        const QString applied = venues->addVenue(wanted);
        venues->setActiveVenue(applied);
        emit announce(wanted.isEmpty() ? tr("New venue, named for today.")
                                       : tr("Venue %1.").arg(applied));
    }
    /* Straight on to the calls: never through Armed, which would read as
       the focus over (PadRun rule 6) between the name and the calls. */
    QString why;
    if (!beginCalls(&why))
    {
        setState(Armed);
        emit announce(why, ControllerScheme::ImportantPriority);
        return false;
    }
    return true;
}

bool FocusCallCue::cancelStart()
{
    if (m_state != NamingVenue)
        return false;
    m_askTimer->stop();
    setState(Armed);
    emit announce(tr("Venue focus not started."), ControllerScheme::ImportantPriority);
    return true;
}

bool FocusCallCue::asksStill() const
{
    return m_state == NamingVenue && !m_session->venues()->activeVenue().isEmpty();
}

bool FocusCallCue::answerStillFromPad()
{
    if (m_state != NamingVenue)
        return false;
    const QString current = m_session->venues()->activeVenue();
    // Yes only: a different room is named on the laptop (or by voice).
    if (current.isEmpty())
        return false;
    return useVenue(current);
}

void FocusCallCue::holdVenueAsk()
{
    m_askTimer->stop();
}

bool FocusCallCue::beginCalls(QString *why)
{
    auto refuse = [this, why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        emit announce(reason, ControllerScheme::ImportantPriority);
        return false;
    };
    m_calls->ensureCalls();
    FocusSession::Options o;
    o.calls = m_calls->sessionCalls();
    o.order = FocusSession::ByStagePosition;
    o.others = FocusSession::OthersDim;
    o.allowOperate = true;
    o.endAfterLastCall = true;
    QString reason;
    if (!m_session->start(o, &reason))
        return refuse(reason);
    setState(Running);
    return true;
}

void FocusCallCue::disarm()
{
    if (m_state != Armed && m_state != NamingVenue)
        return;
    m_askTimer->stop();
    m_cue = m_list = kInvalid;
    setState(Idle);
}

void FocusCallCue::cut()
{
    /* No focus cue after the stop: the ALL OFF stops its list, which lets
       go of a waiting cue a tick later anyway (functionStopped ->
       disarm()). Running calls, a start asking for the venue and a cue
       waiting all go now, at once: the operator GOes onto the focus cue
       again to have it. */
    if (m_state == Idle)
        return;
    m_askTimer->stop();
    m_cue = m_list = kInvalid;
    setState(Idle);
}

void FocusCallCue::sessionStopped()
{
    if (m_state != Running)
        return;
    const quint32 running = m_runningProbe ? m_runningProbe(m_list)
                                           : m_doc->showTree()->runningCue(m_list);
    const bool stillOnIt = running == m_cue;
    const bool asked = m_session->endedByOperator();
    m_finished = m_cue;
    m_finishedList = m_list;
    m_cue = m_list = kInvalid;
    setState(Idle);
    /* Ended by something else (Refine started, the show closed): said, and
       the list is left where it is. */
    if (!asked)
    {
        emit announce(tr("Venue focus ended."), ControllerScheme::ImportantPriority);
        return;
    }
    /* Done: the list stays on the focus cue until the operator moves it on
       (PadRun rule 6, 2026-11-15) -- from the pad, the laptop or anywhere,
       the list never goes on by itself. Said, and shown, as waiting. */
    if (!stillOnIt)
        return;
    setDone(true);
    emit announce(m_runRole ? tr("Venue focus done. {LT} and {A} for the next cue.")
                            : tr("Venue focus done. GO for the next cue."));
}

void FocusCallCue::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged(state);
}
