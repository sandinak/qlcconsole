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

#include "showtreecontroller.h"
#include "venuecorrections.h"
#include "focuscallcue.h"
#include "focussession.h"
#include "showcontrol.h"
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
        }, Qt::QueuedConnection);
    }
    if (m_session != nullptr)
        connect(m_session, &FocusSession::stopped, this, &FocusCallCue::sessionStopped);
    if (m_doc != nullptr)
        connect(m_doc, &Doc::clearing, this, [this]() {
            m_watched.clear();
            m_cue = m_list = kInvalid;
            setState(Idle);
        });
}

FocusCallCue::~FocusCallCue()
{
}

QString FocusCallCue::armedPhrase()
{
    return tr("Focus calls ready. Press Start to begin.");
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
    if (cueId != m_finished)
        m_finished = kInvalid;
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
    emit announce(armedPhrase());
}

bool FocusCallCue::start(QString *why)
{
    auto refuse = [this, why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        emit announce(reason);
        return false;
    };
    if (m_state != Armed)
        return false;
    if (m_calls == nullptr || m_calls->count() == 0)
        return refuse(tr("This show has no focus calls yet."));
    /* Saves go to a venue: the one applied, else one named for today
       (Q110), where the show has a file to keep it beside. */
    VenueCorrections *venues = m_session->venues();
    if (venues != nullptr && !venues->isActive() && !venues->filePath().isEmpty())
        venues->setActiveVenue(venues->addVenue());

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
    if (m_state != Armed)
        return;
    m_cue = m_list = kInvalid;
    setState(Idle);
}

void FocusCallCue::sessionStopped()
{
    if (m_state != Running)
        return;
    const quint32 chaser = chaserOf(m_list);
    m_finished = m_cue;
    m_cue = m_list = kInvalid;
    setState(Idle);
    // Done: on with the list.
    if (chaser != kInvalid)
    {
        emit continueRequested(chaser);
        if (ShowControl *sc = m_doc->showControl())
        {
            ShowControl::Request r;
            r.command = ShowControl::Go;
            r.chaser = chaser;
            sc->execute(r);
        }
    }
}

void FocusCallCue::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged(state);
}
