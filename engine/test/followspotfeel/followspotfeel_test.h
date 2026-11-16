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
