/*
  Q Light Controller
  showrunner.h

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

#ifndef SHOWRUNNER_H
#define SHOWRUNNER_H

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QHash>
#include <QSet>

#include <function.h>

class ShowFunction;
class Function;
class Chaser;
class Track;
class Show;
class Doc;
class Universe;

/** @addtogroup engine_functions Functions
 * @{
 */

class ShowRunner final : public QObject
{
    Q_OBJECT

public:
    ShowRunner(const Doc *doc, quint32 showID, quint32 startTime = 0);
    ~ShowRunner();

    /** Start the runner */
    void start();

    /** If running, pauses the runner and all the current running functions. */
    void setPause(bool enable);

    /** Stop the runner */
    void stop();

    void write(MasterTimer *timer, const QList<Universe*> &universes);

    /*********************************************************************
     * Timecode follow (e.g. MIDI Time Code)
     *********************************************************************/
public:
    /** When enabled, the runner's elapsed clock is driven by an external
     *  absolute position (setExternalTime) instead of the MasterTimer tick.
     *  When external positions stop arriving the clock simply holds at its
     *  last value, which yields manual-GO / spoken-scene behaviour. */
    void setTimecodeFollow(bool enable);
    bool timecodeFollow() const { return m_timecodeFollow; }

    /** Set the external absolute position (ms) to follow. Thread-safe:
     *  called from the UI thread while write() runs on the timer thread. */
    void setExternalTime(quint32 ms);

    /*********************************************************************
     * Timeline suspend (Operate-mode VC takeover)
     *********************************************************************/
public:
    /** Suspend/resume timeline OUTPUT while keeping the playhead position.
     *  While suspended the runner stops every running child function (so the
     *  Virtual Console owns the rig) but the elapsed clock keeps tracking the
     *  timecode, so releasing the takeover resumes exactly where the timeline
     *  now is. Thread-safe: set from the UI thread, applied on the timer thread
     *  inside write(). */
    void setSuspended(bool enable);
    bool isSuspended() const;

private:
    bool m_suspended;        // effective state, owned by the timer thread
    bool m_suspendRequest;   // requested state, guarded by m_tcMutex
    bool m_tcHolding;        // TC stopped while following → children paused (fades freeze)

private:
    /** Relocate the runner to an absolute position: stop running functions,
     *  skip past ones that already ended, and let write() restart the ones
     *  active at the new position (with the correct time offset). */
    void seekTo(quint32 targetMs);

    bool m_timecodeFollow;
    bool m_externalTimeSet;
    bool m_externalTimeFresh; // a new external position arrived since last tick
    quint32 m_externalTime;
    quint32 m_msSinceFresh;   // ms of interpolation since the last fresh position
    QMutex m_tcMutex;

private:
    const Doc *m_doc;

    /** The reference of the show to play */
    Show* m_show;

    /** The list of time-based Functions the Show needs to play */
    QList <ShowFunction *> m_timeFunctions;

    /** Index of the item in m_timeFunctions to be considered for playback */
    int m_currentTimeFunctionIndex;

    /** Elapsed time since runner start. Used also to move the cursor in the track view */
    quint32 m_elapsedTime;

    /** The list of beat-based Functions the Show needs to play */
    QList <ShowFunction *> m_beatFunctions;

    /** Index of the item in m_beatFunctions to be considered for playback */
    int m_currentBeatFunctionIndex;

    /** Elapsed beats since runner start */
    quint32 m_elapsedBeats;

    /** Flag used to sinchronize playback to beats */
    bool beatSynced;

    /** Total time the runner has to run */
    quint32 m_totalRunTime;

    /** List of the currently running Functions and their stop time */
    QList < QPair<Function *, quint32> > m_runningQueue;

    /*********************************************************************
     * Chaser lockstep — a chaser block's cues follow the SHOW clock
     *********************************************************************/
private:
    /** Step a chaser should be on at @p localMs into its block: walk the
     *  cumulative timeline dwells and return the cue whose span contains it
     *  (clamped to the last cue past the end). -1 if the chaser has no steps. */
    int stepAtLocalMs(Chaser *c, quint32 localMs) const;

    /** Release show-clock ownership of a child being stopped/relocated (so a
     *  later standalone run of the same chaser self-advances again) and drop its
     *  lockstep state. */
    void releaseChild(Function *f);

    /** True if @p track currently produces no output: muted, or (when any track
     *  is soloed) neither soloed nor solo-safe. @p anySolo is precomputed once. */
    bool trackSilent(Track *track, bool anySolo) const;

    /** The track owning the ShowFunction that references @p f, or NULL. */
    Track *trackForFunction(Function *f) const;

    /** Start the ShowFunction @p sf (owned by @p track) at @p offset ms into it,
     *  wiring intensity override + fade priority + show-clock. Shared by the
     *  normal start phase and the live-unmute path. */
    void startChild(ShowFunction *sf, Track *track, quint32 offset);

    /** Per-frame enforcement of live mute/solo/solo-safe: stop children of tracks
     *  that just went silent, start the active cues of tracks that just became
     *  audible. Targeted (only changed tracks), so it doesn't disturb the rest. */
    void enforceLiveMuteSolo();

    /** Show start time (ms) of each running child — for the lockstep offset. */
    QHash<Function *, quint32> m_childStartMs;
    /** Track ids that were effectively silent last frame (live mute/solo diff). */
    QSet<quint32> m_lastSilent;

private:
    FunctionParent functionParent() const;

signals:
    void timeChanged(quint32 time);
    void showFinished();

    /************************************************************************
     * Intensity
     ************************************************************************/
public:
    /**
     * Adjust the intensity of show track
     */
    void adjustIntensity(qreal fraction, Track *track);

private:
    QMap<quint32, qreal> m_intensityMap;

};

/** @} */

#endif
