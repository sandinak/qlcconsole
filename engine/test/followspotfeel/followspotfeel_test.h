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
