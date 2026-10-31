/*
  Q Light Controller Plus
  chaser.h

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

#ifndef CHASER_H
#define CHASER_H

#include <QMutex>
#include "qlcengine_export.h"
#include <QColor>
#include <QList>

#include "function.h"
#include "scene.h"
#include "chaserstep.h"
#include "chaserrunner.h"

class QFile;
class QString;
class ChaserStep;
class MasterTimer;
class QXmlStreamReader;

#define KXMLQLCChaserSpeedModes QStringLiteral("SpeedModes")

/** @addtogroup engine_functions Functions
 * @{
 */

/**
 * Chaser is a meta-function; it consists of other functions that are run in a
 * sequential order. Chaser contains information only on the running order,
 * direction and interval applied for its member functions. Therefore, a chaser
 * itself is nothing without other functions (more than usually, scenes) as its
 * members. If a member function's behaviour is changed, those changes are also
 * applied automatically to those chasers that hold the changed function(s) as
 * their members.
 */
class QLC_ENGINE_EXPORT Chaser : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Chaser)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Chaser(Doc* doc);
    virtual ~Chaser();

    /** @reimp */
    virtual QIcon getIcon() const override;

private:
    quint32 m_legacyHoldBus;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Chaser contents
     *********************************************************************/
public:
    /**
     * Add the given step to the chaser, either at the specified index, given
     * that 0 <= index < size or at the end if size < index < 0. The same
     * function can exist any number of times in a chaser. No checks for the
     * function's validity are made at this point, except that the chaser's own
     * ID cannot be added (i.e. a chaser cannot be its own direct member).
     *
     * @param step The step to add
     * @param index Insertion point. -1 to append.
     */
    bool addStep(const ChaserStep& step, int index = -1);

    /**
     * Remove a function from the given step index. If the given index is
     * out of bounds, this returns false.
     *
     * @param index The index number to remove
     * @return true if successful, otherwise false
     */
    bool removeStep(int index);

    /**
     * Replace the step at the given $index with the new $step.
     *
     * @param step The new step to replace with
     * @param index The index of the step to replace
     * @return true if successful, otherwise false (index out of bounds)
     */
    bool replaceStep(const ChaserStep& step, int index);

    /**
     * Move a step from $sourceIdx to $destIdx
     *
     * @param sourceIdx Source index of the step
     * @param destIdx Destination index of the step
     * @return true if successful, otherwise false (index out of bounds or dest == source)
     */
    bool moveStep(int sourceIdx, int destIdx);

    /** Get the Chaser steps number */
    int stepsCount() const;

    /**
     * Get a chaser step from a given index
     *
     * @return The requested Chaser Step
     */
    ChaserStep *stepAt(int idx);

    /**
     * Get the chaser's list of steps
     *
     * @return List of function Chaser Steps
     */
    QList <ChaserStep> steps() const;

    /**
     * Index of the cue live at @a localMs when this chaser is driven by an
     * external (show) clock, treating a 0 / infinite step duration as
     * @a untimedCueMs. Computed under m_stepListMutex WITHOUT copying the step
     * list — called every tick per running chaser, so it must not allocate.
     * Returns -1 for an empty chaser, or the last index past the final cue.
     *
     * @param localMs Elapsed position within the chaser block (ms)
     * @param untimedCueMs Dwell to assume for an un-timed (0/infinite) cue
     * @return Live cue index, or -1 if empty
     */
    int stepIndexAtLocalMs(quint32 localMs, quint32 untimedCueMs) const;

    /** @reimpl */
    void setTotalDuration(quint32 msec) override;

    /** @reimpl */
    quint32 totalDuration() override;

public slots:
    /**
     * Catches Doc::functionRemoved() so that destroyed members can be
     * removed immediately. This method removes all occurrences of the
     * given function ID, while removeStep() only removes one function ID
     * at the given index.
     *
     * @param fid The ID of the function that was removed
     */
    void slotFunctionRemoved(quint32 fid);

signals:
    void stepChanged(int index);
    void stepsListChanged(quint32 fid);

protected:
    QList <ChaserStep> m_steps;
    // Guards m_steps. Recursive because createRunner() holds it while building a
    // ChaserRunner that calls steps() back. mutable so the const readers
    // (steps/stepsCount) can lock — they MUST, or a UI-thread repaint racing the
    // timer thread's step reads crashes (SequenceItem::paint copying m_steps
    // mid-reallocation).
    mutable QRecursiveMutex m_stepListMutex;

    /*********************************************************************
     * Speed modes
     *********************************************************************/
public:
    enum SpeedMode {
        Default = 0, //! Use step function's own speed setting
        Common,  //! Impose a common chaser-specific speed to all steps
        PerStep  //! Impose a step-specific speed to each step
    };
    Q_ENUM(SpeedMode)

    void setFadeInMode(SpeedMode mode);
    SpeedMode fadeInMode() const;

    void setFadeOutMode(SpeedMode mode);
    SpeedMode fadeOutMode() const;

    void setDurationMode(SpeedMode mode);
    SpeedMode durationMode() const;

    static QString speedModeToString(SpeedMode mode);
    static SpeedMode stringToSpeedMode(const QString& str);

    /*********************************************************************
     * GO on the beat (docs/design/PLAYBACK_TIMING_DESIGN.md, P1)
     *********************************************************************/
public:
    /** When a step change asked for (a GO, Back, a jump to a step) lands:
     *  at once, or held to the next whole beat or bar (4 beats) of the
     *  chaser's clock -- its speed master's, else the main beat clock's. */
    enum GoQuantize { GoNow = 0, GoOnBeat, GoOnBar };
    GoQuantize goQuantize() const { return m_goQuantize; }
    void setGoQuantize(GoQuantize q);
    static QString goQuantizeToString(GoQuantize q);
    static GoQuantize stringToGoQuantize(const QString &str);

private:
    GoQuantize m_goQuantize = GoNow;

    /*********************************************************************
     * Cues for show control (docs/design/SHOW_CONTROL_DESIGN.md)
     *********************************************************************/
public:
    /** The number a show controller picks this cue list by ("2"); empty
     *  for none. */
    QString cueListNumber() const { return m_cueListNumber; }
    void setCueListNumber(const QString &number);

    /** Step @a index's cue: its cue number, or its position counting from
     *  1 when it has none. Empty for a step that isn't there. */
    QString stepCue(int index) const;
    /** Whether step @a index has a cue number of its own (set by hand or
     *  by renumbering), not just its position. */
    bool stepHasCueNumber(int index) const;
    /** The step whose cue is @a cue (cuesMatch()), or -1. */
    int findCue(const QString &cue) const;
    /** Numbers every step 1, 2, 3... in order. */
    void renumberCues();
    /** Step @a index is skipped (ChaserStep::skip); false for one not there. */
    bool isStepSkipped(int index) const;
    /** Skips step @a index, or puts it back. */
    void setStepSkipped(int index, bool skip);
    /** The first step GO plays from stopped: the first not skipped (the last,
     *  running backward); -1 when every step is skipped. */
    int firstPlayableStep() const;
    /** Two cue numbers are the same cue: as numbers when both read as
     *  numbers ("12.50" is "12.5"), else as text, ignoring case and spaces
     *  round them. */
    static bool cuesMatch(const QString &a, const QString &b);

private:
    QString m_cueListNumber;

    /*********************************************************************
     * Sub-cues (docs/design/SUBCUES_DESIGN.md)
     *********************************************************************/
public:
    /** Step @a index's function as a chase -- its steps are the step's
     *  sub-cues -- or null when it isn't one (or isn't there). */
    Chaser *subCueChase(int index) const;
    /** How step @a index's sub-cues advance (ChaserStep::subCues). */
    ChaserStep::SubCueMode stepSubCues(int index) const;
    void setStepSubCues(int index, ChaserStep::SubCueMode mode);
    /** Step @a index is a chase held as sub-cues: SubGo, and its function
     *  is a chase. */
    bool isSubGoStep(int index) const;
    /** Any step is (the cue list shows Sub GO). */
    bool hasSubGoSteps() const;
    /** How many sub-cues step @a index has: its chase's steps; 0 when it
     *  isn't a chase. */
    int subCueCount(int index) const;
    /** Sub-cue @a subIndex of step @a index: the inner step's own cue
     *  number, else "<parent>.<n>" counting from 1 ("4.2"). Empty for one
     *  that isn't there. */
    QString subCue(int index, int subIndex) const;
    /** The sub-cue whose cue is @a cue (cuesMatch()): true and its step and
     *  sub-cue in @a index and @a subIndex; false for none. Sub-cues only. */
    bool findSubCue(const QString &cue, int *index, int *subIndex) const;
    /** A cue by number as show control reads one: a step (its sub-cue -1)
     *  -- a main cue numbered "4.1" wins -- else a sub-cue. */
    bool findCueOrSubCue(const QString &cue, int *index, int *subIndex) const;

    /** The sub-cue playing in the step playing: its chase's step, or -1
     *  when the step playing isn't a chase (or nothing plays). */
    int currentSubCueIndex() const;
    /** What Sub GO plays next in the step playing, or -1: none left, or
     *  the step isn't held as sub-cues. */
    int nextSubCueIndex() const;
    /** What Sub Back plays, or -1. */
    int previousSubCueIndex() const;

    /** The next step from @a from, @a delta (+1 / -1) at a time in list
     *  order, that isn't skipped; -1 at the end. From -1 (forward) or
     *  stepsCount() (backward): the first. */
    int nextPlayableStep(int from, int delta) const;

    /** Sub GO, Sub Back, and a jump to sub-cue @a subIndex of step @a index
     *  (that step first, if it isn't the one playing). ChaserActions, as
     *  setAction() takes them; a stopped chase plays the jump when started. */
    void subGo();
    void subBack();
    void jumpToSubCue(int index, int subIndex);

    /** This chase plays as a step's sub-cues (ChaserRunner::setSubCueHeld):
     *  set by the chase whose step it is, before it starts it. */
    void setSubCueHeld(bool held);
    bool isSubCueHeld() const;
    /** Held, an inner step with a finite hold still goes on by itself
     *  (ChaserStep::SubGoFollow): set with setSubCueHeld(). */
    void setSubCueFollow(bool follow);

    /** About to be started again in this tick, having just been stopped (a
     *  step's chase whose next step is the same chase): until the new run
     *  starts, actions are kept for it, and the run being torn down neither
     *  takes them nor plays. Only when stopped (and not paused), so the
     *  start is sure to follow; false otherwise. */
    bool beginRestart();

private:
    bool m_subCueHeld = false;
    bool m_subCueFollow = false;
    bool m_restarting = false;

public:

protected:
    SpeedMode m_fadeInMode;
    SpeedMode m_fadeOutMode;
    SpeedMode m_holdMode;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
protected:
    bool loadXMLSpeedModes(QXmlStreamReader &root);

public:
    /** @reimpl */
    virtual bool saveXML(QXmlStreamWriter *doc) const override;

    /** @reimpl */
    virtual bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    virtual void postLoad() override;

    /*********************************************************************
     * Start/Next/Previous
     * ChaserRunner wrappers
     *********************************************************************/
public:
    enum FadeControlMode
    {
        FromFunction = 0,
        Blended,
        Crossfade,
        BlendedCrossfade
    };

    /** @reimpl */
    void tap() override;

    /** Set an action to be performed on steps.
     *  Depending on the action type, it might be applied immediately
     *  or deferred to the next write() call */
    void setAction(ChaserAction &action);

    /** Get the current step number */
    int currentStepIndex() const;

    /** Compute next step for manual fading */
    int computeNextStep(int currentStepIndex) const;

    /** Run this chaser under an external (show/timecode) clock: its cues advance
     *  only when a ShowRunner steps them from the show clock (or the operator
     *  GOes), never on the chaser's own MasterTimer elapsed time. Set by
     *  ShowRunner when the chaser is a block on a Show timeline; must be cleared
     *  when the show releases it so standalone playback self-advances again. */
    void setShowClocked(bool enable);
    bool isShowClocked() const { return m_showClocked; }

    /** Get the running step number. */
    int runningStepsNumber() const;

    /** Get the first step of the running list. If none is running this returns NULL */
    ChaserRunnerStep currentRunningStep() const;
    /** A GO is waiting for its beat (goQuantize()). */
    bool goHeld() const;

    /** Effective duration (ms) of the currently running step, honouring the
     *  Common/PerStep speed mode. 0 = holds until a manual GO. Used by the
     *  move-in-black look-ahead to gauge how soon the next cue fires. */
    uint currentStepDuration() const;

private:
    ChaserAction m_startupAction;

public:
    /** @reimp */
    virtual bool contains(quint32 functionId) const override;

    /** @reimp */
    QList<quint32> components() const override;

    /*********************************************************************
     * Running
     *********************************************************************/
private:
    /**
     * Create a ChaserRunner object from the given Chaser. The chaser's
     * step mutex is locked & unlocked in this method.
     *
     * @param self The parent Chaser function to create a runner for
     * @param doc The engine object
     * @return NULL if unsuccessful, otherwise a new ChaserRunner*
     */
    void createRunner(quint32 startTime = 0);

public:
    /** @reimp */
    void preRun(MasterTimer* timer) override;

    /** @reimp */
    void setPause(bool enable) override;

    /** @reimp */
    void write(MasterTimer* timer, QList<Universe *> universes) override;

    /** @reimp */
    void postRun(MasterTimer* timer, QList<Universe *> universes) override;

signals:
    /** Tells that the current step number has changed. */
    void currentStepChanged(int stepNumber);

private:
    QRecursiveMutex m_runnerMutex;
    ChaserRunner* m_runner;
    bool m_showClocked;     //! Steps driven by a ShowRunner's clock, not the MasterTimer

    /*************************************************************************
     * Intensity
     *************************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId) override;

    /** Adjust the intensities of chaser steps. */
    void adjustStepIntensity(qreal fraction, int stepIndex = -1,
                             FadeControlMode fadeControl = FromFunction);
};

/** @} */

#endif
