/*
  Q Light Controller Plus
  padrunrules_test.h

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

#ifndef PADRUNRULES_TEST_H
#define PADRUNRULES_TEST_H

#include <QObject>
#include <QList>
#include <QPair>

#include "showcontrol.h"

class IOPluginStub;
class PadRun;
class Doc;

/* The operator's seven Pad Run rules (padrun.h), tried the way a pad on
   stage breaks them rather than the way they were built: pads that drop
   mid-hold and come back, triggers that jitter or never rest at zero,
   presses that bounce or arrive twice, the four of the stop in every
   order, two pads and the same pad twice, tools and the Controller Test
   starting mid-chord, and a seeded random sweep of the whole map. Pad
   input goes in as a plugin's does (the stub's roles, the patch). */
class PadRunRules_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /* Rule 3 (and 4): the pad going away */
    void aLostPadDisarmsOnTheRealPath();
    void aSafetyHeldAsThePadDropsIsNoSafetyAfter();
    void aSwappedPadForgetsWhatTheOldOneHeld();
    void aPadBackShowsTheStateAgain();

    /* Rules 3 and 5: the chords */
    void theStopsOwnBumpersDoNotArm();
    void aTriggerInTheArmChordSpendsIt();
    void everyOrderOfTheFour();
    void holdingTheFourIsOneStop();
    void theStopWorksWhileWalking();
    void theStopWorksWhileAToolHasThePad();

    /* Rule 4: real pads' shapes */
    void triggersThatJitterAreOnePull();
    void theSamePressTwiceIsOneGo();
    void aBouncingButton();
    void theDuplicateDevice();

    /* Rules 2 and 6: one state, the light */
    void theLightAfterAToolOrWalkEndsStopped();
    void aFadingCueShowsPlayingForItsFade();

    /* Rule 7, and tools taking the pad mid-chord */
    void aChordHeldAsAToolStartsIsForgotten();
    void theControllerTestMidChord();
    void theControllerTestSwallowsUnroledButtons();
    void aButtonLetGoDuringTheTestIsLetGo();

    /* Controller jobs (docs/design/CONTROLLER_JOBS.md), swept */
    void jobsEveryControlOnASecondPad_data();
    void jobsEveryControlOnASecondPad();
    void jobsOneRunnerAtATime();
    void jobsTheStopFromAPadWithNoJobs();
    void jobsOnePadNothingSavedIsAsBefore();
    void jobsEachPadsLight();
    void jobsTheFollowSpotSkipsAPad();
    void jobsFocusOnlyFromFocusPads();
    /** Board Q250 (a), each job on its own: the runner has Run cues only
     *  and is armed; the other pad has each of the eight job mixes, on
     *  either line. With Focus it starts the venue focus with LT + Menu
     *  and no arm (Menu alone never does), the runner stays armed with
     *  its GO family refused on the screen, FOCUS names the focus's pad,
     *  and after the end the runner's GO works with no re-arm. */
    void jobsVenueFocusFromAFocusPad_data();
    void jobsVenueFocusFromAFocusPad();
    /** The four from either pad while the other pad's venue focus runs. */
    void jobsTheStopDuringAnotherPadsFocus_data();
    void jobsTheStopDuringAnotherPadsFocus();
    /** One pad with Run cues and Focus: as before (armed, LT + Menu, the
     *  arm put by and back; not armed, refused saying to arm). */
    void jobsRunAndFocusOnOnePadAsBefore();
    /** A toggle hit on while another pad starts the venue focus: not cut;
     *  the runner's pull lets it go, a new pull is refused until the end. */
    void jobsAHitPlayingAcrossAnotherPadsFocus();
    /** A Focus pad's Menu while another pad is armed: no walk, and the
     *  words name the runner (never "disarm first" to a pad that can't). */
    void jobsTheWalkBesideTheRunner();
    /** A swipe on a pad without Focus starts no walk (the walk is Focus's). */
    void jobsASwipeIsTheFocusJobs();
    /** View + Menu on a Focus pad in the Run role: Focus mode is the Tools
     *  role's, said on the screen. */
    void jobsFocusModeInRunIsSaidOnScreen();
    /** The pad the focus tools read is one with the Focus job. */
    void jobsTheFocusControllerHasTheFocusJob();
    void jobsTakenAwayWhileInUse();
    /** Review 0.4.10: one pad listed twice (one address, two universes,
     *  CONTROLLER_JOBS.md 9: one controller) is told one light. */
    void reviewTheSamePadTwiceIsToldOneLight();

    /* Rules 1, 3, 4 and 5 over random presses */
    void theRulesHoldOverRandomPresses_data();
    void theRulesHoldOverRandomPresses();

private:
    IOPluginStub *loadStub();
    /** Four spots, a list: Opening, Blackout (no layer), Ballad. */
    void makeShow();
    /** A focus call on the spots and Opening a venue focus cue, waiting
     *  (Operate; the probe says the list is on Opening). */
    void makeFocusCue();
    /** The focus cue waiting again (the list off it and back). */
    void focusCueAgain();
    /** LT held + Menu on @p universe. */
    void ltMenu(quint32 universe);
    /** LT held + @p channel on @p universe. */
    void withLt(quint32 channel, quint32 universe);
    void press(quint32 channel, quint32 universe = 0);
    void axis(quint32 channel, uchar value, quint32 universe = 0);
    void hold(quint32 channel, bool down, quint32 universe = 0);
    void arm(bool on, quint32 universe = 0);
    /** The pad unplugged / plugged back, as InputOutputMap does it. */
    void drop(quint32 universe = 0);
    void back(quint32 universe = 0);
    /** Pads A (universe 0) and B (universe 1), told apart by address,
     *  with these jobs (ControllerJobs::Jobs as int). */
    void twoPads(int jobsA, int jobsB);
    /** The last light sent to input line @p line ("": its own; "none"). */
    QString lightOn(quint32 line) const;
    int goes() const;
    int stops() const;
    /** Announcements said since @p from that contain @p part. */
    int saidCount(const QString &part) const;

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QList<QPair<ShowControl::Request, ShowControl::Route>> m_runs;
    QStringList m_said;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0;
    QList<quint32> m_spots;
    quint32 m_running = 0;      // the cue the focus probe says the list is on
};

#endif
