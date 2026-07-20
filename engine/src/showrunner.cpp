/*
  Q Light Controller
  showrunner.cpp

  Copyright (c) Massimo Callegari

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

#include <QMutex>
#include <QDebug>
#include <QFile>
#include <QTextStream>

#include "showrunner.h"
#include "function.h"
#include "chaser.h"
#include "chaserstep.h"
#include "chaseraction.h"
#include "track.h"
#include "universe.h"
#include "timecodesource.h"
#include "show.h"

#define TIMER_INTERVAL 50

// Env-gated file log (QLC_SHOW_DEBUG=1 → /tmp/qlc_showdebug.log). qDebug is
// swallowed by main.cpp's handler, so diagnostics go to a file. Remove once the
// MTC show drive is verified.
static void srDbg(const QString &line)
{
    static const bool on = qEnvironmentVariableIntValue("QLC_SHOW_DEBUG") != 0;
    if (on == false)
        return;
    QFile f("/tmp/qlc_showdebug.log");
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << line << "\n";
}

static bool compareShowFunctions(const ShowFunction *sf1, const ShowFunction *sf2)
{
    if (sf1->startTime() < sf2->startTime())
        return true;
    return false;
}

/** Map a track's role to a fader priority. Background yields to everything (a
 *  base wash the VC/other tracks beat); Override wins LTP (an accent); Normal
 *  is Auto (symmetric last-writer-wins, the default). */
static int trackFadePriority(Track *track)
{
    if (track == NULL)
        return Universe::Auto;
    switch (track->priority())
    {
        case Track::Background: return Universe::Background;
        case Track::Override:   return Universe::Override;
        default:                return Universe::Auto;
    }
}

ShowRunner::ShowRunner(const Doc* doc, quint32 showID, quint32 startTime)
    : QObject(NULL)
    , m_doc(doc)
    , m_currentTimeFunctionIndex(0)
    , m_elapsedTime(startTime)
    , m_currentBeatFunctionIndex(0)
    , m_elapsedBeats(0)
    , beatSynced(false)
    , m_totalRunTime(0)
    , m_timecodeFollow(false)
    , m_externalTimeSet(false)
    , m_externalTimeFresh(false)
    , m_externalTime(0)
    , m_msSinceFresh(0)
    , m_suspended(false)
    , m_suspendRequest(false)
    , m_tcHolding(false)
{
    Q_ASSERT(m_doc != NULL);
    Q_ASSERT(showID != Show::invalidId());

    m_show = qobject_cast<Show*>(m_doc->function(showID));
    if (m_show == NULL)
        return;

    // Build the queue from ALL tracks — mute/solo/solo-safe are enforced LIVE at
    // start/run time (trackSilent + the per-frame diff in write()), so toggling
    // them mid-run takes effect without a restart.
    foreach (Track *track, m_show->tracks())
    {
        // some sanity checks
        if (track == NULL ||
            track->id() == Track::invalidId())
                continue;

        // get all the functions of the track and append them to the runner queue
        foreach (ShowFunction *sfunc, track->showFunctions())
        {
            if (sfunc->startTime() + sfunc->duration(m_doc) <= startTime)
                continue;

            Function *f = m_doc->function(sfunc->functionID());
            if (f == NULL)
                continue;

            if (f->tempoType() == Function::Time)
                m_timeFunctions.append(sfunc);
            else
                m_beatFunctions.append(sfunc);

            if (sfunc->startTime() + sfunc->duration(m_doc) > m_totalRunTime)
                m_totalRunTime = sfunc->startTime() + sfunc->duration(m_doc);
        }

        // Initialize the intensity map from the track's submaster level.
        m_intensityMap[track->id()] = track->intensity();
    }

    std::sort(m_timeFunctions.begin(), m_timeFunctions.end(), compareShowFunctions);
    std::sort(m_beatFunctions.begin(), m_beatFunctions.end(), compareShowFunctions);

#if 1
    qDebug() << "Ordered list of ShowFunctions (time):";
    foreach (ShowFunction *sfunc, m_timeFunctions)
        qDebug() << "[Show] Function ID:" << sfunc->functionID() << "start time:" << sfunc->startTime() << "duration:" << sfunc->duration(m_doc);

    qDebug() << "Ordered list of ShowFunctions (beats):";
    foreach (ShowFunction *sfunc, m_beatFunctions)
        qDebug() << "[Show] Function ID:" << sfunc->functionID() << "start time:" << sfunc->startTime() << "duration:" << sfunc->duration(m_doc);
#endif
    m_runningQueue.clear();

    qDebug() << "ShowRunner created";
}

ShowRunner::~ShowRunner()
{
}

void ShowRunner::start()
{
    qDebug() << "ShowRunner started";
}

void ShowRunner::setPause(bool enable)
{
    // Freeze every child on MTC-stop: the show-clocked chaser already holds its
    // cue (a frozen clock issues no step change), and pausing additionally
    // freezes any in-progress fade so the whole look stops dead. A SEPARATE cue
    // list (not a show child) is untouched, so a hand cue can still override the
    // frozen fixtures while the show is paused.
    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        f->setPause(enable);
    }
}

void ShowRunner::stop()
{
    m_elapsedTime = 0;
    m_elapsedBeats = 0;
    m_currentTimeFunctionIndex = 0;
    m_currentBeatFunctionIndex = 0;

    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        releaseChild(f);
        f->stop(functionParent());
    }

    m_runningQueue.clear();
    m_childStartMs.clear();
    m_suspended = false;
    m_tcHolding = false;
    {
        QMutexLocker locker(&m_tcMutex);
        m_suspendRequest = false;
    }
    qDebug() << "ShowRunner stopped";
}

FunctionParent ShowRunner::functionParent() const
{
    return FunctionParent(FunctionParent::Function, m_show->id());
}

void ShowRunner::setTimecodeFollow(bool enable)
{
    QMutexLocker locker(&m_tcMutex);
    m_timecodeFollow = enable;
    if (enable == false)
        m_externalTimeSet = false;
}

void ShowRunner::setExternalTime(quint32 ms)
{
    QMutexLocker locker(&m_tcMutex);
    m_externalTime = ms;
    m_externalTimeSet = true;
    m_externalTimeFresh = true;
}

void ShowRunner::setSuspended(bool enable)
{
    // Only record the request here; the actual stop/seek is done on the timer
    // thread inside write() so we never mutate m_runningQueue concurrently.
    QMutexLocker locker(&m_tcMutex);
    m_suspendRequest = enable;
}

bool ShowRunner::isSuspended() const
{
    return m_suspended;
}

void ShowRunner::seekTo(quint32 targetMs)
{
    // Stop everything currently playing.
    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        releaseChild(m_runningQueue.at(i).first);
        m_runningQueue.at(i).first->stop(functionParent());
    }
    m_runningQueue.clear();
    m_childStartMs.clear();

    // Rewind and skip past functions that have fully ended before the target,
    // so write()'s start phase only (re)starts the ones actually active now.
    m_currentTimeFunctionIndex = 0;
    while (m_currentTimeFunctionIndex < m_timeFunctions.count())
    {
        ShowFunction *sf = m_timeFunctions.at(m_currentTimeFunctionIndex);
        if (sf->startTime() + sf->duration(m_doc) <= targetMs)
            m_currentTimeFunctionIndex++;
        else
            break;
    }

    m_currentBeatFunctionIndex = 0;
    m_elapsedTime = targetMs;
}

// Nominal on-timeline dwell for an un-timed (0-duration) cue, so a Hold=0 cue
// occupies a visible, walkable span under the show clock instead of flashing
// past. Matches the Show Manager's SequenceItem SEQ_NOMINAL_STEP_MS so the
// engine advance and the timeline block widths agree.
#define SHOW_NOMINAL_CUE_MS 3000

// On-timeline dwell (ms) of a cue: its dragged/Common duration, or a nominal
// dwell for an un-timed (0) or legacy infinite-hold cue. A show is pure timecode
// — every cue is time-positioned; there are no manual-GO holds inside a show.
static quint32 cueDwellMs(quint32 rawDuration)
{
    if (rawDuration == 0 || rawDuration == Function::infiniteSpeed())
        return SHOW_NOMINAL_CUE_MS;
    return rawDuration;
}

int ShowRunner::stepAtLocalMs(Chaser *c, quint32 localMs) const
{
    // Work off a locked COPY of the steps (never stepAt() pointers): this runs on
    // the timer thread while the UI may repaint / edit the same chaser, and an
    // unlocked read of m_steps races those and crashes.
    const QList<ChaserStep> steps = c->steps();
    const int count = steps.count();
    if (count == 0)
        return -1;
    const bool common = (c->durationMode() == Chaser::Common);
    const quint32 commonDur = c->duration();
    quint32 acc = 0;
    for (int s = 0; s < count; s++)
    {
        quint32 dwell = cueDwellMs(common ? commonDur : steps.at(s).duration);
        if (localMs < acc + dwell)
            return s;
        acc += dwell;
    }
    return count - 1;               // past the last cue: clamp (block stop ends it)
}

void ShowRunner::releaseChild(Function *f)
{
    if (f == NULL)
        return;
    // Hand show-clock ownership back so a later standalone run self-advances.
    Chaser *c = qobject_cast<Chaser *>(f);
    if (c != NULL)
        c->setShowClocked(false);
    m_childStartMs.remove(f);
}

bool ShowRunner::trackSilent(Track *track, bool anySolo) const
{
    if (track == NULL)
        return true;
    if (track->isMute())
        return true;
    if (anySolo && track->isSolo() == false && track->isSoloSafe() == false)
        return true;
    return false;
}

Track *ShowRunner::trackForFunction(Function *f) const
{
    if (f == NULL || m_show == NULL)
        return NULL;
    foreach (Track *track, m_show->tracks())
    {
        if (track == NULL)
            continue;
        foreach (ShowFunction *sf, track->showFunctions())
            if (sf->functionID() == f->id())
                return track;
    }
    return NULL;
}

void ShowRunner::startChild(ShowFunction *sf, Track *track, quint32 offset)
{
    if (sf == NULL || track == NULL)
        return;
    Function *f = m_doc->function(sf->functionID());
    if (f == NULL)
        return;

    // Never start a function that's already in the running queue. Without this,
    // enforceLiveMuteSolo() (un-mute path) and the Phase-1 start loop can both
    // start the same cue in one frame (e.g. after a suspend/resume that re-seeks
    // and re-establishes the index), doubling it in the queue + double-releasing.
    for (int i = 0; i < m_runningQueue.count(); i++)
        if (m_runningQueue.at(i).first == f)
            return;

    int intOverrideId = f->requestAttributeOverride(Function::Intensity,
                                                    m_intensityMap.value(track->id(), 1.0));
    sf->setIntensityOverrideId(intOverrideId);
    f->setFadePriority(trackFadePriority(track));
    // A chaser block runs under the SHOW clock (never self-advances).
    if (Chaser *ch = qobject_cast<Chaser *>(f))
        ch->setShowClocked(true);
    f->start(m_doc->masterTimer(), functionParent(), offset);
    m_runningQueue.append(QPair<Function *, quint32>(f, sf->startTime() + sf->duration(m_doc)));
    m_childStartMs.insert(f, sf->startTime());
}

void ShowRunner::enforceLiveMuteSolo()
{
    if (m_show == NULL)
        return;

    bool anySolo = false;
    foreach (Track *track, m_show->tracks())
        if (track != NULL && track->isSolo()) { anySolo = true; break; }

    QSet<quint32> silentNow;
    foreach (Track *track, m_show->tracks())
        if (track != NULL && track->id() != Track::invalidId() && trackSilent(track, anySolo))
            silentNow.insert(track->id());

    if (silentNow == m_lastSilent)
        return; // no change — nothing to do

    // Tracks that JUST went silent: stop their running children.
    for (int i = m_runningQueue.count() - 1; i >= 0; i--)
    {
        Function *f = m_runningQueue.at(i).first;
        Track *track = trackForFunction(f);
        if (track == NULL)
            continue;
        const bool nowSilent = silentNow.contains(track->id());
        const bool wasSilent = m_lastSilent.contains(track->id());
        if (nowSilent && wasSilent == false)
        {
            releaseChild(f);
            f->stop(functionParent());
            m_runningQueue.removeAt(i);
        }
    }

    // Tracks that JUST became audible: start their cues active at this position.
    foreach (Track *track, m_show->tracks())
    {
        if (track == NULL || track->id() == Track::invalidId())
            continue;
        const bool nowSilent = silentNow.contains(track->id());
        const bool wasSilent = m_lastSilent.contains(track->id());
        if (nowSilent || wasSilent == false)
            continue; // only tracks that transitioned silent -> audible

        foreach (ShowFunction *sf, track->showFunctions())
        {
            Function *f = m_doc->function(sf->functionID());
            if (f == NULL || f->tempoType() != Function::Time)
                continue;
            const quint32 start = sf->startTime();
            const quint32 end = start + sf->duration(m_doc);
            if (m_elapsedTime < start || m_elapsedTime >= end)
                continue; // not active at the current position
            // Skip if already running.
            bool running = false;
            for (int i = 0; i < m_runningQueue.count(); i++)
                if (m_runningQueue.at(i).first == f) { running = true; break; }
            if (running == false)
                startChild(sf, track, m_elapsedTime - start);
        }
    }

    m_lastSilent = silentNow;
}

void ShowRunner::write(MasterTimer *timer, const QList<Universe*> &universes)
{
    //qDebug() << Q_FUNC_INFO << "elapsed:" << m_elapsedTime << ", total:" << m_totalRunTime;

    // In timecode-follow mode the elapsed clock chases an external absolute
    // position. Incoming timecode only arrives ~12x/second, so snapping to it
    // each tick makes the cursor jump in ~80ms steps. Instead we ADVANCE the
    // clock at the tick rate between updates (smooth 50Hz) and RESYNC when a
    // fresh position arrives — a small forward correction if we drifted, or a
    // locate (seek) on a real backward jump.
    bool following = false;
    {
        QMutexLocker locker(&m_tcMutex);
        following = m_timecodeFollow;
        bool set = m_externalTimeSet;
        bool fresh = m_externalTimeFresh;
        quint32 target = m_externalTime;
        m_externalTimeFresh = false;
        locker.unlock();

        if (following && set)
        {
            if (fresh)
                m_msSinceFresh = 0;
            else
                m_msSinceFresh += MasterTimer::tick();

            // Timecode is "live" while fresh positions keep arriving; a short
            // window rides over normal MTC jitter. Once it lapses, HOLD (the
            // freeze / manual-GO fallback when Logic stops) — kept small so the
            // playhead stops promptly on pause.
            // Freeze/thaw the running children (scenes, chasers, …) with the
            // timecode: when TC stops, the show clock holds — but the children's
            // own fades/steps run on the MasterTimer and would keep going. Pause
            // them so an in-progress fade FREEZES at TC-stop and resumes on
            // TC-restart (Branson: "the fade stops").
            // Freeze threshold — the SAME shared constant the watchdog uses for
            // the "holding" chip (SHOW_TC_HOLD_MS), so the rig freeze and the
            // indicator flip together.
            const bool holding = (m_msSinceFresh > SHOW_TC_HOLD_MS);
            if (holding != m_tcHolding)
            {
                m_tcHolding = holding;
                setPause(holding);
            }

            if (m_msSinceFresh <= SHOW_TC_HOLD_MS)
            {
                // Smooth base advance every frame (this is what keeps the cursor
                // gliding at 50Hz regardless of the ~12Hz MTC rate).
                m_elapsedTime += MasterTimer::tick();

                // Gentle phase-lock toward the true position when one arrives:
                // snap only on a real locate/jump, otherwise ease a fraction of
                // the drift so the motion stays smooth (no per-update chunking).
                if (fresh)
                {
                    qint64 drift = qint64(target) - qint64(m_elapsedTime);
                    if (drift < -200 || drift > 500)
                    {
                        if (target < m_elapsedTime)
                            seekTo(target);
                        else
                            m_elapsedTime = target;
                    }
                    else if (drift > 0)
                    {
                        m_elapsedTime += drift / 4; // ease forward ~25% per update
                    }
                    // drift <= 0 (slightly ahead): no backward nudge — the base
                    // advance keeps gliding and timecode catches up.
                }
            }
        }
    }

    // If follow was disarmed while the children were frozen (TC-hold), thaw them.
    if (following == false && m_tcHolding)
    {
        setPause(false);
        m_tcHolding = false;
    }

    // Timeline suspend (Operate-mode VC takeover). The elapsed clock above keeps
    // tracking the timecode so the playhead stays synced with Logic; here we
    // simply hand the rig to the Virtual Console: on entering suspend, stop every
    // running child function (releasing its faders); on leaving, relocate to the
    // current position so the functions active NOW restart with the right offset.
    bool wantSuspend;
    {
        QMutexLocker locker(&m_tcMutex);
        wantSuspend = m_suspendRequest;
    }
    if (wantSuspend != m_suspended)
    {
        m_suspended = wantSuspend;
        if (m_suspended)
        {
            // Hold the current look (last-look holder) before releasing the
            // children, so exiting to VC control HANDS OVER the look instead of
            // blacking out when the VC isn't already driving those channels. The
            // VC overrides per-channel (LTP); on resume the restarted show cues
            // reclaim their fixtures (per-fixture yield clears the hold).
            if (m_doc != NULL && m_doc->lastLookEnabled() && m_show != NULL)
                m_doc->captureLastLook(m_show->id(), universes);

            for (int i = 0; i < m_runningQueue.count(); i++)
            {
                releaseChild(m_runningQueue.at(i).first);
                m_runningQueue.at(i).first->stop(functionParent());
            }
            m_runningQueue.clear();
            m_childStartMs.clear();
        }
        else
        {
            // Resume: re-establish the functions active at the current position.
            // seekTo() sets the indices/clock; the start phase below restarts them.
            seekTo(m_elapsedTime);
        }
    }
    // Conclusive gate probe (~1s): logs BEFORE the suspend early-return so we can
    // tell a suspended show (clock moves, nothing starts) from one that never
    // reaches Phase 1. Shows the queue and how far the start indices have walked.
    {
        static int gn = 0;
        if ((gn++ % 50) == 0)
            srDbg(QString("[GATE] elapsed=%1 follow=%2 hold=%3 suspend=%4 wantSuspend=%5 queue=%6 timeFuncs=%7 idx=%8 mode=%9")
                  .arg(m_elapsedTime).arg(following).arg(m_tcHolding).arg(m_suspended)
                  .arg(wantSuspend).arg(m_runningQueue.count()).arg(m_timeFunctions.count())
                  .arg(m_currentTimeFunctionIndex)
                  .arg(m_doc->mode() == Doc::Operate ? "OP" : "DES"));
    }

    if (m_suspended)
    {
        emit timeChanged(m_elapsedTime);
        return;
    }

    // Phase 1. Check all the Functions that need to be started
    // m_timeFunctions is ordered by startup time, so when we found an entry
    // with start time greater than m_elapsed, this phase is over
    bool startFunctionsDone = false;

    // check synchronization to beats (if show is beat-based)
    if (m_show->tempoType() == Function::Beats)
    {
        //qDebug() << Q_FUNC_INFO << "isBeat:" << timer->isBeat() << ", elapsed beats:" << m_elapsedBeats;

        if (timer->isBeat())
        {
            if (beatSynced == false)
            {
                beatSynced = true;
                qDebug() << "Beat synced";
            }
            else
                m_elapsedBeats += 1000;
        }

        if (beatSynced == false)
            return;
    }

    // Apply any queued track-intensity submaster changes (from the UI thread).
    applyPendingIntensity();

    // Live mute/solo/solo-safe: stop/start only the tracks whose silence changed
    // since last frame, so toggling them mid-run takes effect immediately. Not
    // while suspended (the VC owns the rig then). Also refreshes m_lastSilent,
    // which the start gate below reads.
    if (m_suspended == false)
        enforceLiveMuteSolo();

    // check if there are time-based functions to start
    while (startFunctionsDone == false)
    {
        if (m_currentTimeFunctionIndex == m_timeFunctions.count())
            break;

        ShowFunction *sf = m_timeFunctions.at(m_currentTimeFunctionIndex);
        quint32 funcStartTime = sf->startTime();
        quint32 functionTimeOffset = 0;
        Function *f = m_doc->function(sf->functionID());

        // this should happen only when a Show is not started from 0
        if (m_elapsedTime > funcStartTime)
        {
            functionTimeOffset = m_elapsedTime - funcStartTime;
            funcStartTime = m_elapsedTime;
        }
        if (m_elapsedTime >= funcStartTime)
        {
            // Symmetric LTP + per-track priority/intensity/show-clock: startChild
            // wires it all. A silent track's cue is SKIPPED here (mute/solo);
            // enforceLiveMuteSolo() starts it if the track is later un-silenced.
            Track *track = trackForFunction(f);
            if (track != NULL && m_lastSilent.contains(track->id()) == false)
            {
                startChild(sf, track, functionTimeOffset);
                srDbg(QString("[START] '%1' type=%2 at elapsed=%3 offset=%4 running=%5 prio=%6")
                      .arg(f->name()).arg(f->type()).arg(m_elapsedTime).arg(functionTimeOffset)
                      .arg(f->isRunning()).arg(f->fadePriority()));
            }
            m_currentTimeFunctionIndex++;
        }
        else
            startFunctionsDone = true;
    }

    startFunctionsDone = false;

    // check if there are beat-based functions to start
    while (startFunctionsDone == false)
    {
        if (m_currentBeatFunctionIndex == m_beatFunctions.count())
            break;

        ShowFunction *sf = m_beatFunctions.at(m_currentBeatFunctionIndex);
        quint32 funcStartTime = sf->startTime();
        quint32 functionTimeOffset = 0;
        Function *f = m_doc->function(sf->functionID());

        // this should happen only when a Show is not started from 0
        if (m_elapsedBeats > funcStartTime)
        {
            functionTimeOffset = m_elapsedBeats - funcStartTime;
            funcStartTime = m_elapsedBeats;
        }
        if (m_elapsedBeats >= funcStartTime)
        {
            // Skip a silent track's cue (mute/solo). Beat shows don't get the
            // live un-mute restart (they're legacy vs the timecode model), but a
            // muted beat track still stays silent.
            Track *track = trackForFunction(f);
            if (track != NULL && m_lastSilent.contains(track->id()) == false)
                startChild(sf, track, functionTimeOffset);
            m_currentBeatFunctionIndex++;
        }
        else
            startFunctionsDone = true;
    }

    // Phase 2. Check if we need to stop some running Functions
    // It is done in reverse order for two reasons:
    // 1- m_runningQueue is not ordered by stop time
    // 2- to avoid messing up with indices when an entry is removed
    for (int i = m_runningQueue.count() - 1; i >= 0; i--)
    {
        Function *func = m_runningQueue.at(i).first;
        quint32 stopTime = m_runningQueue.at(i).second;
        quint32 currTime = func->tempoType() == Function::Time ? m_elapsedTime : m_elapsedBeats;

        // if we passed the function stop time
        if (currTime >= stopTime)
        {
            // Hold-last: if this function's track is flagged, snapshot its look
            // into the last-look holder BEFORE releasing, so it persists on the
            // rig instead of going dark (a "stays up between numbers" track).
            // Operate only; the track's next cue clears it (per-fixture yield).
            if (m_doc != NULL && m_doc->mode() == Doc::Operate && m_doc->lastLookEnabled())
            {
                foreach (Track *track, m_show->tracks())
                {
                    if (track != NULL && track->isHoldLast()
                            && track->showFunctions().size() > 0)
                    {
                        bool onThisTrack = false;
                        foreach (ShowFunction *sf2, track->showFunctions())
                            if (sf2->functionID() == func->id()) { onThisTrack = true; break; }
                        if (onThisTrack)
                        {
                            m_doc->captureLastLook(func->id(), universes, /*append*/true);
                            break;
                        }
                    }
                }
            }
            // stop the function
            releaseChild(func);
            func->stop(functionParent());
            // remove it from the running queue
            m_runningQueue.removeAt(i);
        }
    }

    // Chaser lockstep (pure timecode): drive each running chaser's cue straight
    // from the SHOW clock. The chaser is show-clocked (never self-advances), so
    // its cue is purely a function of the show position: the cue whose cumulative
    // on-timeline span contains localMs. No manual-GO holds inside a show — every
    // cue fires on the clock. Edge-triggered on a change of target, and BOTH
    // directions, so a Logic locate/scrub jumps the cue straight to the new
    // position. When the clock is frozen (MTC stopped) target == cur → no change,
    // so the cue holds; the children are also paused above so fades freeze too.
    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        Chaser *c = qobject_cast<Chaser *>(f);
        if (c == NULL || c->tempoType() != Function::Time || c->stepsCount() == 0)
            continue;

        const int cur = c->currentStepIndex();
        if (cur < 0)
            continue;               // not on a step yet (chaser runner not written)

        const quint32 startMs = m_childStartMs.value(f, 0);
        const quint32 localMs = (m_elapsedTime > startMs) ? m_elapsedTime - startMs : 0;
        const int target = stepAtLocalMs(c, localMs);

        if (target >= 0 && target != cur)
        {
            ChaserAction a;
            a.m_action = ChaserSetStepIndex;
            a.m_stepIndex = target;
            a.m_masterIntensity = 1.0;
            a.m_stepIntensity = 1.0;
            a.m_fadeMode = Chaser::FromFunction;   // use the step's own fade times
            c->setAction(a);
        }
    }

    // Env-gated diagnostics (QLC_SHOW_DEBUG=1 → /tmp/qlc_showdebug.log). qDebug is
    // swallowed by main.cpp's handler, so log to a file. ~1s cadence. Shows the
    // clock and, per chaser child, whether it's show-clocked and which cue the
    // barrier drive holds/advances it to. Remove once MTC drive is verified.
    static const bool scDbg = qEnvironmentVariableIntValue("QLC_SHOW_DEBUG") != 0;
    if (scDbg)
    {
        static int wn = 0;
        if ((wn++ % 50) == 0)
        {
            QFile lf("/tmp/qlc_showdebug.log");
            if (lf.open(QIODevice::Append | QIODevice::Text))
            {
                QTextStream ts(&lf);
                ts << "[RUNNER] elapsed=" << m_elapsedTime
                   << " follow=" << following << " hold=" << m_tcHolding
                   << " susp=" << m_suspended << " queue=" << m_runningQueue.count();
                for (int i = 0; i < m_runningQueue.count(); i++)
                {
                    Chaser *c = qobject_cast<Chaser *>(m_runningQueue.at(i).first);
                    if (c == NULL)
                        continue;
                    const int cur = c->currentStepIndex();
                    const quint32 startMs = m_childStartMs.value(c, 0);
                    const quint32 localMs = (m_elapsedTime > startMs) ? m_elapsedTime - startMs : 0;
                    const QList<ChaserStep> dsteps = c->steps();
                    Function *cf = (cur >= 0 && cur < dsteps.count())
                                   ? m_doc->function(dsteps.at(cur).fid) : NULL;
                    ts << " | chaser='" << c->name() << "' clocked=" << c->isShowClocked()
                       << " paused=" << c->isPaused() << " cur=" << cur
                       << " localMs=" << localMs
                       << " tgt=" << stepAtLocalMs(c, localMs)
                       << " curFn='" << (cf ? cf->name() : QString("?"))
                       << "' curFnRunning=" << (cf ? cf->isRunning() : false);
                }
                ts << "\n";
            }
        }
    }

    // Phase 3. Check if this is the end of the Show. In timecode-follow mode
    // the external source owns the clock (it may loop or relocate), so we
    // never auto-finish — we just report the position and hold.
    if (following == false)
    {
        if (m_elapsedTime >= m_totalRunTime)
        {
            if (m_show != NULL)
                m_show->stop(functionParent());
            emit showFinished();
            return;
        }

        m_elapsedTime += MasterTimer::tick();
    }

    emit timeChanged(m_elapsedTime);
}

/************************************************************************
 * Intensity
 ************************************************************************/

void ShowRunner::adjustIntensity(qreal fraction, Track *track)
{
    if (track == NULL)
        return;
    // Called from the UI thread (track submaster fader, Show::adjustAttribute)
    // AND the timer thread (preRun). Never touch m_intensityMap / m_runningQueue
    // here — they belong to the timer thread and it mutates them every frame.
    // Record the request; applyPendingIntensity() applies it on the timer thread.
    QMutexLocker locker(&m_tcMutex);
    m_pendingIntensity[track->id()] = fraction;
}

void ShowRunner::applyPendingIntensity()
{
    QMap<quint32, qreal> pending;
    {
        QMutexLocker locker(&m_tcMutex);
        if (m_pendingIntensity.isEmpty())
            return;
        pending.swap(m_pendingIntensity);
    }

    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it)
    {
        m_intensityMap[it.key()] = it.value();
        Track *track = (m_show != NULL) ? m_show->track(it.key()) : NULL;
        if (track == NULL)
            continue;
        foreach (ShowFunction *sf, track->showFunctions())
        {
            Function *f = m_doc->function(sf->functionID());
            if (f == NULL)
                continue;
            for (int i = 0; i < m_runningQueue.count(); i++)
                if (m_runningQueue.at(i).first == f)
                    f->adjustAttribute(it.value(), sf->intensityOverrideId());
        }
    }
}

