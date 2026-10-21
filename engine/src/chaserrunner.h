/*
  Q Light Controller Plus
  chaserrunner.h

  Copyright (c) Heikki Junnila
                Massimo Callegari
                Jano Svitok

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

#ifndef CHASERRUNNER_H
#define CHASERRUNNER_H

#include <limits>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QSet>

#include "function.h"
#include "chaseraction.h"

class QElapsedTimer;
class GenericFader;
class FadeChannel;
class ChaserStep;
class Function;
class Universe;
class Chaser;
class Doc;

/** @addtogroup engine_functions Functions
 * @{
 */

typedef struct
{
    int m_index;                        //! Index of the step from the original Chaser
    Function *m_function;               //! Currently active function
    quint32 m_elapsed;                  //! Elapsed milliseconds
    quint32 m_elapsedBeats;             //! Elapsed beats
    uint m_fadeIn;                      //! Step fade in in ms
    uint m_fadeOut;                     //! Step fade out in ms
    uint m_duration;                    //! Step hold in ms
    Universe::BlendMode m_blendMode;    //! The original Function blend mode
    int m_intensityOverrideId;          //! An ID to control the step intensity
    int m_pIntensityOverrideId;         //! An ID to control the step parent intensity
    bool m_subCues;                     //! Its function is a chase held as sub-cues (Sub GO)
} ChaserRunnerStep;

class ChaserRunner final : public QObject
{
    Q_OBJECT

public:
    ChaserRunner(const Doc* doc, const Chaser* chaser, quint32 startTime = 0);
    ~ChaserRunner();

private slots:
    void slotChaserChanged();

private:
    const Doc *m_doc;
    const Chaser *m_chaser;

    /************************************************************************
     * Speed
     ************************************************************************/
private:
    /** Get the currently active fade in value (See Chaser::SpeedMode) */
    uint stepFadeIn(int stepIdx) const;

    /** Get the currently active fade out value (See Chaser::SpeedMode) */
    uint stepFadeOut(int stepIdx) const;

    /** Get the currently active duration value (See Chaser::SpeedMode) */
    uint stepDuration(int stepIdx) const;

private:
    bool m_updateOverrideSpeeds;

    /************************************************************************
     * Step control
     ************************************************************************/
public:
    void setAction(ChaserAction &action);

    /**
     * Produce a tap event to the runner, possibly producing a next() call.
     */
    void tap();

    /**
     * Get the current step number.
     *
     * @return Current step number
     */
    int currentStepIndex() const;

    /** Effective duration (ms) of the currently running step (speed-mode aware).
     *  0 = holds until a manual GO. */
    uint currentStepDuration() const { return stepDuration(currentStepIndex()); }

    /**
     * External-clock mode. When enabled the runner NEVER advances a step on its
     * own elapsed time — the step changes only on an explicit action
     * (SetStepIndex from a ShowRunner driving the cue by the show clock, or a
     * manual GO / Next / Previous). Used when a chaser block runs inside a Show
     * so its cues follow the SHOW/timecode clock instead of free-running on the
     * MasterTimer (see ShowRunner). Infinite-hold cues stay manual-GO barriers.
     */
    void setExternalClock(bool enable) { m_externalClock = enable; }
    bool isExternalClock() const { return m_externalClock; }

    /**
     * Held as sub-cues (SUBCUES_DESIGN.md): this chase plays as a step's
     * sub-cues on a Sub GO. Its holds are not counted (a step changes only on
     * an action), a step change lands at once (the outer chase quantizes Sub
     * GO), and a step index is taken in list order, whatever the run order.
     */
    void setSubCueHeld(bool enable) { m_subCueHeld = enable; }
    bool isSubCueHeld() const { return m_subCueHeld; }
    /** Held, but a step with a finite hold still goes on by itself after it
     *  (ChaserStep::SubGoFollow: auto-follow sub-cues). */
    void setSubCueFollow(bool enable) { m_subCueFollow = enable; }

    /**
     * Compute next step for manual fading
     */
    int computeNextStep(int currentStepIndex) const;
    /** computeNextStep() as if no step were skipped. */
    int computeNextStepIgnoringSkips(int currentStepIndex) const;

    /**
     * Get the running step number.
     *
     * @return Running step number
     */
    int runningStepsNumber() const;

    /**
     * Get the first step of the running list.
     * If none is running this returns NULL
     */
    ChaserRunnerStep *currentRunningStep() const;

private:
    /**
     * Shuffle the current steps order
     */
    static void shuffle(QVector<int> & data);

    /**
     * Retrieve the randomized index of a
     * step at the given index
     */
    int randomStepIndex(int step) const;

    /**
     * Convenience function to fill the m_oder
     * array with all the steps indices
     */
    void fillOrder();

    /**
     * Fill the m_order array with the step indices
     * and the given size
     */
    void fillOrder(int size);

signals:
    /** Tells that the current step number has changed. */
    void currentStepChanged(int stepNumber);

private:
    Function::Direction m_direction;        //! Run-time direction (reversed by ping-pong)
    QList <ChaserRunnerStep *> m_runnerSteps;  //! Queue of the currently running steps
    QList <ChaserRunnerStep *> m_leavingSteps; //! Left by a step change, not yet let go of (leaveSteps())
    QList <ChaserRunnerStep *> m_handedOverSteps; //! Sharing with the new step: stopped on the next write
    /** Handed over to (or from) a chase held as sub-cues: its Collection
     *  claims what they share one write later than a Collection step does,
     *  so these are stopped a write after that. */
    QList <ChaserRunnerStep *> m_handedOverLater;
    quint32 m_startOffset;                  //! Start step offset time in milliseconds
    ChaserAction m_pendingAction;           //! Action to be performed on steps at the next write call
    int m_lastRunStepIdx;                   //! Index of the last step ran
    bool m_externalClock;                   //! If true, steps advance only on explicit action (show-clock driven)
    bool m_subCueHeld = false;              //! Played as sub-cues: steps change only on an action
    bool m_subCueFollow = false;            //! ...except a finite hold, which follows on (SubGoFollow)
    ChaserAction m_pendingSubAction;        //! A Sub GO / Sub Back / jump to a sub-cue for the next write
    quint32 m_lastFunctionID;               //! ID of the last Function ran (Scene only)
    QElapsedTimer *m_roundTime;             //! Counts the time between steps
    /** On a speed master: its last whole beat counted (none yet: min). */
    qint64 m_masterBeat = std::numeric_limits<qint64>::min();

    /** GO on the beat: the beat (or bar) a held step change was asked in;
     *  it lands when the clock passes it. min = nothing held. */
    qint64 m_goHeldIn = std::numeric_limits<qint64>::min();
    /** The clock a held GO waits on, in beats (or bars). */
    double goClockUnits() const;

    /** GO on the beat for Sub GO: as m_goHeldIn. */
    qint64 m_subGoHeldIn = std::numeric_limits<qint64>::min();

public:
    /** A GO (or Sub GO) is being held for the beat (the cue list shows it). */
    bool goHeld() const { return m_goHeldIn != std::numeric_limits<qint64>::min()
                                 || m_subGoHeldIn != std::numeric_limits<qint64>::min(); }

    /** The running step at the current index, or null. */
    ChaserRunnerStep *currentIndexStep() const;
private:
    /** Stop a running step's function; a chase held as its sub-cues is let
     *  go of (no longer held). */
    void stopStepFunction(ChaserRunnerStep *step, bool preserveAttributes = false);
    /** Pass a pending Sub GO / Sub Back / sub-cue jump on to the chase
     *  playing as the current step's sub-cues. */
    void applySubAction(const ChaserAction &action);

private:
    QVector<int> m_order;                   //! Array of step indices in a randomized order

    /************************************************************************
     * Intensity
     ************************************************************************/
public:
    /** Adjust the intensities of chaser steps */
    void adjustStepIntensity(qreal fraction, int stepIndex = -1, int fadeControl = 0);

    /************************************************************************
     * Running
     ************************************************************************/
private:
    /**
     * Stop every running Function and clear the running list.
     *
     * @a handOver: a step change. A step whose function is a Collection is
     * not stopped here but left for leaveSteps(), once the new step is
     * known (SHOW_TREE_DESIGN.md §4: a Scene two consecutive cues share
     * keeps running across GO). Anything else -- the chaser stopping -- stops
     * everything at once, those left too.
     */
    void clearRunningList(bool handOver = false);

    /**
     * Let go of the steps a step change left (m_leavingSteps), now that the
     * new step has started. One whose Collection shares a member with the
     * new step's is stopped on the next write() instead, when the new step's
     * Collection has started and claimed that member too: stopped now, it
     * would stop the shared member before the new step could take it over,
     * and that member would start again from nothing (a re-fade, an effect
     * from its first frame). The rest stop at once, as before.
     */
    void leaveSteps();

    /** Stop and forget the steps held over by leaveSteps() a write ago. */
    void stopHandedOverSteps();

    /************************************************************************
     * Tracked state (Back, a jump)
     ************************************************************************/
private:
    /**
     * A step started from write(): restack the stage as the list played
     * from the top would have left it, unless this is the step forward
     * playback goes to next (GO, auto-follow), and put the step's members'
     * faders in its order (raiseMembers()).
     *
     * Only for a step that plays a Collection (a Show Tree cue, or a chase
     * held as its sub-cues): each is the cue's whole look, but an LTP
     * channel the cue doesn't set -- the colour of a fixture it leaves dark,
     * or lights with only a dimmer -- keeps what the stage had. GO from the
     * cue before leaves it what the cues before set last; Back, or a jump,
     * left what the cue played before the jump set. restack() writes what
     * the cues before set last on those channels (or, for a channel only
     * later cues set, what it was when the list started), under every
     * fader, until the next step change: a leaving cue's fade-out ends in
     * it, and the next GO carries on from it as from forward playback.
     * Single Shot and Loop lists, played forward; other run orders have no
     * "from the top" and are left as they were.
     */
    void stepStarted(int index, QList<Universe *> universes, bool jump);
    /** Whether @a to is the step forward playback goes to from @a from
     *  (-1: the list starting); @a wrap: round from the last step of a
     *  Loop list to its first too (GO, auto-follow -- not a jump, which
     *  plays the first cue as from the top). */
    bool isNextStep(int from, int to, bool wrap) const;
    /** What step @a step plays its look with: its Collection; for sub-cues
     *  the first, or all of them, in order, when they follow on by
     *  themselves -- or @a every (what Sub GO may have played). */
    QList<quint32> stepLooks(const ChaserStep &step, bool every) const;
    void restack(int index, QList<Universe *> universes);
    /** Let go of the restacked state: its channels keep their values (LTP). */
    void dismissRestack();
    /** The faders of @a f's members (at any depth), in member order, on top
     *  of their priority, so the later member wins what they both set
     *  whichever of them was running first. */
    void raiseMembers(const Function *f, QList<Universe *> universes, int depth = 0);

    int m_restackFrom = -1;                 //! The step started last (-1: none yet)
    QSet<int> m_visited;                    //! The steps started since the list started
    QSet<QPair<quint32, quint32> > m_touched; //! Fixture channels a restack has put back
    QList<QByteArray> m_startStage;         //! The universes when the list started
    QList<QPair<Universe *, QSharedPointer<GenericFader> > > m_restackFaders;
    int m_raiseFor = 0;                     //! Writes left to keep the current step's members in order

    /** Whether @a from and @a to are Collections with a member in common
     *  (at any depth), or one holds the other. */
    static bool sharesMembers(const Function *from, const Function *to);

    /**
     * Start a Chaser step Function with the given $index, at the given
     * master and step intensity and from the given $elapsed time.
     * $fadeControl specifies how the step Function should fade, according to
     * the Chaser::FadeControlMode enumeration:
     * - Chaser::FromFunction will use the original Function fadeIn time
     * - Chaser::Crossfade means that the user is manually overriding the Function intensity with a slider
     *                     so the Function fadeIn time will be set to 0
     * - Chaser::LinkedCrossfade is like Crossfade, and the Function will also be requested
     *                           to use the Universe::AdditiveBlend mode
     */
    void startNewStep(int index, MasterTimer *timer, qreal mIntensity, qreal sIntensity,
                      int fadeControl, quint32 elapsed = 0);

    /**
     * Get the index of the next step that should be started,
     * according to the Chaser running order
     */
    int getNextStepIndex();
    /** getNextStepIndex() as if no step were skipped. */
    int nextStepIndexIgnoringSkips();

private:
    FunctionParent functionParent() const;

public:
    /**
     * Call this from the parent function's write() method to run the steps.
     *
     * @param timer The MasterTimer that runs the show
     * @param universes DMX address space
     * @return true if the chaser should continue, otherwise false
     */
    bool write(MasterTimer* timer, QList<Universe*> universes);

    /** If running, pauses the runner and all the current running steps. */
    void setPause(bool enable, QList<Universe *> universes);

    /**
     * Perform postRun operations. Call this from the parent function's postRun().
     *
     * @param timer The MasterTimer that runs the show
     * @param universes DMX address space
     */
    void postRun(MasterTimer* timer, QList<Universe *> universes);
};

/** @} */

#endif
