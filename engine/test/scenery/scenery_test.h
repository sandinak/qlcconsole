/*
  Q Light Controller Plus - Test Unit
  scenery_test.h

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

#ifndef SCENERY_TEST_H
#define SCENERY_TEST_H

#include <QObject>

class Scenery_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aFlatCycRunsAcrossTheStageFacingDownstage();
    void theBowPullsTheMiddleUpstage();
    void rotationTurnsTheSheet();
    void aBeamStopsWhereItMeetsTheSheet();
    void aBeamThatPassesAboveBesideOrAwayMisses();
    void aBowedCycIsHitFurtherBackInTheMiddle();
    void aUWallRunsRoundItsFront();
    void aWallIsMetOnItsFaceAndOnItsTop();
    void anOpenCurtainHangsOnlyAtItsEnds();
    void aProsceniumIsTwoPiersAndAHeader();
    void xmlRoundTripAndStorage();
};

#endif
