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
    /** Rule 3 (pad-trace replay, 2026-11-16): a chord that fired (or was
     *  refused) does not fire again while both bumpers stay held, whatever
     *  else comes in -- a DualSense's resting trigger noise above all. */
    void aHeldChordFiresOnce_data();
    void aHeldChordFiresOnce();
    void everyOrderOfTheFour();
    void holdingTheFourIsOneStop();
    void theStopWorksWhileWalking();
    void theStopWorksWhileAToolHasThePad();

    /* Rule 4: real pads' shapes */
    void triggersThatJitterAreOnePull();
    void theSamePressTwiceIsOneGo();
    void aBouncingButton();
    void theDuplicateDevice();
    /** One pad seen on two universes, one identity (eval 0.4.12): one
     *  press is one action -- one GO, one hit, one stop -- whichever line
     *  brings it first, in step or a gesture behind. */
    void aPadOnTwoUniversesIsOnePad_data();
    void aPadOnTwoUniversesIsOnePad();
    /** Twin pads told apart (address, USB port) stay two pads: each its
     *  own stop, its own chord, one runner. */
    void twinPadsToldApartAreTwoPads_data();
    void twinPadsToldApartAreTwoPads();

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
    /** CONTROLLER_JOBS.md invariant 6 (eval 0.4.12): another pad's
     *  chord, squeezed and let go inside the runner's, leaves the
     *  runner's chord timing untouched. */
    void anotherPadsChordLeavesTheRunnersTiming();
    /** The sweep: A holds its chord while B does each thing a pad can do
     *  with the four, from every arm state. A's chord ends as it would
     *  alone; B's never arms or disarms anything. */
    void everyChordOnTwoPads_data();
    void everyChordOnTwoPads();
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
    /* Review of a430351ef (Q250): invariants -- feedback goes to the pad
       whose action it is; a tool taking the pad drops the runner's arm,
       whoever started the focus; the four's bits are never cleared. */
    /** 1: the runner's triggers, buzzes and button names are its own. */
    void reviewFeedbackGoesToThePadWhoseActionItIs();
    /** 2: the laptop's Focus this cue over a beside focus disarms. */
    void reviewALaptopToolDisarmsABesideRunner();
    /** 9: so does the Controller Test. */
    void reviewTheControllerTestDisarmsABesideRunner();
    /** 3: the four held across a tool taking the pad still stop. */
    void reviewTheFourSurviveAToolTakingThePad();
    /** 4: a laptop start beside a runner without Focus. */
    void reviewALaptopStartBesideARunnerWithoutFocus();
    /** 5: no venue focus from a pad after the emergency stop. */
    void reviewNoVenueFocusFromAStoppedShow();
    /** 6: one double buzz at the focus end (and at a start given up). */
    void reviewOneBuzzAtTheFocusEnd();
    /** 10: a View let go in another pad's FOCUS is let go. */
    void reviewAViewLetGoInFocusIsLetGo();
    /** 11: the focus pad's Focus job taken mid-focus. */
    void reviewTheFocusJobTakenMidFocus_data();
    void reviewTheFocusJobTakenMidFocus();
    /** 12: the focus pad unplugged mid-focus (second review 3: with a
     *  runner that has Focus too, the pad back takes it again). */
    void reviewTheFocusPadUnpluggedMidFocus_data();
    void reviewTheFocusPadUnpluggedMidFocus();
    /* Board Q252 (a): a Focus pad's walk while another pad is armed --
       Design yes, Operate no; the runner as beside a venue focus. */
    void q252AWalkBesideTheRunnerInDesign();
    void q252NoWalkBesideTheRunnerInOperate();
    void q252TheStopDuringABesideWalk_data();
    void q252TheStopDuringABesideWalk();
    void q252ALaptopToolMidWalkDisarmsTheRunner();
    /* Second review of 46cfdd708 */
    /** 1: Operate mid-walk ends a walk beside the runner. */
    void review2OperateMidWalkEndsABesideWalk();
    /** 2: the walker loses Focus, or is unplugged, mid-walk. */
    void review2TheWalkerLosesFocusMidWalk();
    void review2TheWalkerUnpluggedMidWalk();
    /** 5: the runner's swipe beside a walk steps nothing. */
    void review2ARunnersSwipeBesideAWalk();
    /** 6: the role switched mid-walk ends a walk beside the runner. */
    void review2ARoleSwitchEndsABesideWalk();
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
    /** The feedback of @p kind ("triggers", "rumble") sent to @p line,
     *  in order. */
    QList<QVariantMap> feedbackOn(quint32 line, const QString &kind) const;
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
