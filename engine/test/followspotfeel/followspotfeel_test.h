/*
  Q Light Controller Plus
  followspotfeel_test.h

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

#ifndef FOLLOWSPOTFEEL_TEST_H
#define FOLLOWSPOTFEEL_TEST_H

#include <QObject>

class Doc;

class FollowSpotFeel_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /* The feel, on its own (FollowSpotDrive). */
    void theStickIsTheSameBothWays();
    void startsAndStopsAreEased();
    void theTimeATickTookIsTheTimeMoved();
    void speedSteps();
    void theScriptAndTheHostAgree();

    /* On the real path: stick -> controller -> MasterTimer -> DMX. */
    void theSubjectWalksAtTopSpeedEvenly();
    void everyFrameMovesOnceWhateverTheClocks();
    void leftAndRightAtTheSameSpeed();
    void fineStickAndSpeedStepAdd();
    void topSpeedIsTheLooks();
    void aSixteenBitHeadGlides();
    void freeFlyPansAtTopSpeedBothWays();
    void deletingTheSceneLetsGo();
    /** Heads whose pan zero faces away from the stage (the stage across
     *  turn 0's seam), each turn centred on the stage by the action: the
     *  stick walks the subject across and back and every head's pan moves
     *  smoothly, clear of both channel ends, never round. */
    void aWalkAcrossTheSeamNeverSwingsRound();
    /* Review 0.4.10 (H2): nothing a pad pushed stays pushed when its job
       or binding goes -- in any order, nothing left for the next test. */
    /** Review 0.4.10: a stick bound by hand, held, its pad's Follow spot
     *  job taken away, then let go: the subject must stop. */
    void reviewAHandBoundStickHeldAsItsJobGoes();
    /** Board Q257's follow-up: on Windows a pad's follow spot stick moves
     *  no head (DMX), the same push that moves it elsewhere. */
    void windowsAPadsStickMovesNoSpot();
    /** Review 0.4.10: two pads, A steering with its fine stick pushed; the
     *  Follow spot job handed to B; A lets go: the subject must stop. */
    void reviewAFineStickHeldAsTheJobIsHandedOver();
    /** A stick and fine stick held as the pad is unpatched (no release
     *  ever comes): the subject stops. */
    void aStickHeldAsItsPadGoesStops();
    /** Review 0.4.10 verify: two pads with Follow spot, B's fine stick
     *  held, A's fine stick touched after; B's job taken away and B lets
     *  go (its release is dropped): the subject must stop. */
    void reviewBFineStickHeldAsItsJobGoesAfterATouch();
    /** Review 0.4.10 verify: as above, B unpatched (unplugged) while its
     *  fine stick is held: the subject must stop. */
    void reviewBFineStickHeldAsItsPadGoesAfterATouch();
    /* Board Q253: the follow spot beside a tool on another pad. */
    /** Every tool (Quick Focus, Venue Focus, Focus This Cue, Focus This
     *  Look, the walk) x its lights inside the follow spot's or not x the
     *  spot on the tool owner's pad or another: the spot steers on beside
     *  a tool on other lights from another pad; on its own lights it holds
     *  (sticks zeroed, its status says why); the tool's own pad's sticks
     *  are the tool's. A stick held through the tool's start or end, or
     *  through the hold, never jumps the spot: it moves again only once
     *  centred and pushed. */
    void q253TheSpotBesideEveryTool_data();
    void q253TheSpotBesideEveryTool();
    /** One pad seen twice (one identity on two universes) steers once:
     *  its fine stick is one share, and with the follow spot and the tool
     *  on that pad its twin line's stick is the tool's, not the spot's. */
    void q253ATwinPadDoesNotDoubleSteer();
    /** Twin follow-ups (0.4.13): the tool's owning line of a pad listed
     *  twice goes, its twin line stays: the pad keeps the tool -- its stick
     *  steers the tool, not the follow spot beside it, and the panels name
     *  it. */
    void aToolKeepsItsTwinPadWhenOneLineGoes();
    /** Twin follow-ups (0.4.13): one D-pad press on a pad listed twice is
     *  one speed or size step of the follow spot, with no tool and with a
     *  tool on another pad. */
    void aTwinPadsDpadIsOneFollowSpotStep_data();
    void aTwinPadsDpadIsOneFollowSpotStep();
    /** The review of f16599314: lights the tool aims at the follow spot's
     *  own subject (a focus reference, an Aim look) move when the spot
     *  moves it, so the spot holds for them too, every tool. */
    void q253TargetTiedLightsHoldTheSpot_data();
    void q253TargetTiedLightsHoldTheSpot();
    /** A free-fly follow spot (no subject: the script flies the heads) on
     *  the tool's lights: through the hold the script hears no speed and
     *  the heads stay where they were, every tool. */
    void q253AFreeFlySpotHoldsStill_data();
    void q253AFreeFlySpotHoldsStill();
    /** A walk's cue steps (RT, a swipe) keep the walker its owner: its
     *  stick stays the walk's, another Focus pad is told it is in use; a
     *  pad steering the spot beside it keeps its held fine stick. */
    void q253AWalksCueStepsKeepItsOwnerAndTheSpot_data();
    void q253AWalksCueStepsKeepItsOwnerAndTheSpot();
    /** Second review: a cue's follow spot ends while a venue focus aims at
     *  its subject: the subject's glide back waits while the focus runs,
     *  then glides home. */
    void q253TheRestoreGlideWaitsForTheTool();
    /** The emergency stop ends every tool (E9) and Q253's hold with it:
     *  nothing says the spot holds, the tool's sticks are centred, and a
     *  stick held through the stop moves the spot again only once centred
     *  and pushed -- every tool. */
    void q253TheStopEndsTheToolAndItsHold_data();
    void q253TheStopEndsTheToolAndItsHold();

    /* Where the operator stands (FollowSpotOperator). */
    void stickUpGoesAwayFromTheOperator();
    void frontOfHouseKeepsItsRightAndGoesUpstage();
    void freeFlyIsNotTurned();
    void theOperatorIsKeptPerStickAndVenue();
    void anOldShowIsFrontOfHouse();
    void recalibratingSnapsTo15Degrees();
    void theOperatorViewsTurnTheRigView();

    /* The numbers, printed (QLC_FSPOT_REPORT=1). */
    void report();
    void realTimers();
    void realTimersFreeFly();

private:
    Doc *m_doc = nullptr;
};

#endif
