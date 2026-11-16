/*
  Q Light Controller Plus - Unit test
  aimsolver_test.h

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

#ifndef AIMSOLVER_TEST_H
#define AIMSOLVER_TEST_H

#include <QObject>

class AimSolver_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Wherever the old answer was within pan range, the angles are exactly
     *  what they always were -- reach is new information, not a new aim. */
    void anglesAreUnchangedWherePanFits();
    /** 270 degrees of tilt leaves a 45-degree cone around the base the head
     *  cannot point into: straight down, for a spot standing on a tower. */
    void uprightOnATowerCannotLookStraightDown();
    /** Hung, the same head's blind cone points at the roof instead. */
    void hungUnderABarReachesTheStage();
    /** A short pan reaches behind by swinging round and tilting back. */
    void aShortPanReachesBehindByTiltingBack();
    /** reach() reads the rig: a spot on top of a tower, from the Doc. */
    void reachReadsTheRig();
    /** A scene's Aim look: which heads it points, at what, and which of them
     *  cannot get there. */
    void aSceneSaysWhichHeadsCannotReach();
    void aDeckHeadStandsAtTheDecksHeight();
    /** Calibration offsets are in the DMX and out of the picture: a head
     *  with offsets aimed at a mark sends different DMX from one without,
     *  and that DMX turned back into a beam lands on the mark. */
    void calibrationIsInTheDmxAndOutOfThePicture();
    /** No offset, no change: the inverse is the plain inverse. */
    void zeroOffsetsInverseIsUnchanged();
    /** "When things are aimed, they go where they should": for each rig in
     *  a table (hung, standing, on the floor with no height, turned, inverted,
     *  calibrated, 8- and 16-bit, a two-arm beam with an indexed pan band; at
     *  a point, a person and a line, with Spread), an Aim look's DMX turned
     *  back into a beam with the rig views' own arithmetic passes through
     *  each head's aim point, and that point is on the target. */
    void aimedHeadsLandOnTheirTarget_data();
    void aimedHeadsLandOnTheirTarget();
    /** A definition with pan/tilt 0 aims over the default 540 x 270, and the
     *  DMX it writes lands the beam on the mark; one without pan/tilt
     *  channels cannot be aimed. */
    void zeroRangeDefinitionsStillAim();
    /** An indexed band's coarse-only pan lands on the nearest step. */
    void anIndexedBandRoundsToTheNearestStep();

    // An Aim at an Area covers it (AREA_COVER.md).
    void areaZonesFollowWhereLightsHang();
    void areaAimsAtSubjectHeightAboveTheRiser();
    void areaCoverageFollowsBeamWidth();
    void areaOfNoSizeIsAPoint();
    void areaGivesEachArmItsOwnZone();
    void areaCoversHisChorus();
    void areaCoverNeedsHeadAndDeck();
    void areaTiesGoToTheBestCover();

    /** The pan turn, stored with the head (PanTurn). Centred by the action,
     *  a target swept across the stage and back moves the pan smoothly,
     *  clear of both channel ends, never round, for a 540 and a 630 degree
     *  head whose zero faces away from the stage (the stage across turn
     *  0's seam) and a 360 facing it; hung and standing, inverted or not.
     *  Every answer still points at its target; turn 0 is the old answer. */
    void aSweepAcrossTheStageStaysOnOneTurn_data();
    void aSweepAcrossTheStageStaysOnOneTurn();
    /** A 360 degree head has one pan for each bearing: nothing to choose,
     *  and its answers are what they always were. */
    void aHeadWithOneTurnHasNothingToChoose();
    /** M1: a 360 head with a calibration offset answers the same on any
     *  stored turn. */
    void aCalibratedOneTurnHeadIgnoresAStoredTurn();
    /** Two Aim looks either side of turn 0's seam, the turn centred: their
     *  pans are the short way apart, and a fade lands between the marks. */
    void looksEitherSideOfTheSeamFadeTheShortWay();
    /** Save and load: the turn goes in the show (only when not 0) and
     *  comes back; a show without it loads on turn 0, today's DMX. */
    void thePanTurnIsSavedWithTheShow();
    /** A calibration nudge, a centimetre move, a resized grid, a change of
     *  units: the stored turn and the pan stay put. */
    void nothingButThePersonChangesTheTurn();
    /** Over the stage: turn 0 (its pan zero) whatever the action or move. */
    void aHeadOverTheStageKeepsItsPanZero();
    /** The proposal: 0 while the stage fits, else the turn with most room;
     *  a move proposes only when the stage crosses the seam. */
    void theProposalPutsTheStageClearOfTheSeam();
    /** A front truss just inside the grid (no decks) is out front: the
     *  stage in its near-vertical cone is left out (PadBench). */
    void aFrontTrussJustInsideTheGridIsOutFront();
    /** A seam within kClearDeg of the stage counts as on it. */
    void aSeamJustOffTheStageCountsAsOnIt();

    // Review of d1dd9d380c (the per-frame reference): what must not happen.
    void reviewACalibrationNudgeNeverSwingsTheHeadRound();
    void reviewACentimetreMoveKeepsTheTurn();
    void reviewTheUnitsSettingDoesNotTurnAHeadRound();
    void reviewOverheadLooksThatFadedShortStillDo();
    void reviewAWalkUpTheSideNeverSwingsRound();
    /** Review of c8824b22ce: every stored turn, every mount, invert, pan
     *  offset, 540/630/360, tilt-limited heads on the flipped branch --
     *  every reachable answer points at its target, a 360 ignores the turn,
     *  and a main-branch answer stays in its turn's window. */
    void review_everyTurnPointsAtItsTarget();
    /** Review (pre-existing, not the turn): the flipped branch (pan half a
     *  turn round, tilt mirrored) with a tilt offset still points at the
     *  target. */
    void review_theFlippedBranchKeepsTheTiltOffset();
    /** After the review: live is Operate, or a running scene setting it. */
    void aHeadIsLiveInOperate();
    /** After the review: exact span ends, kept until the head moves. */
    void theStageSpanIsExactAndKept();
};

#endif
