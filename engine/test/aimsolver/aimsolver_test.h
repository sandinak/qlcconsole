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
};

#endif
