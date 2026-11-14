/*
  Q Light Controller Plus
  boxfacing_test.h

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

#ifndef BOXFACING_TEST_H
#define BOXFACING_TEST_H

#include <QObject>

/* Two things on the plot (the Lighting Studio):
   - a Box: a prop beside Steps, LEDs across its front, a clear top
     ("I need a new prop like step .. called box. that we can put leds on
     the front and it will have a clear top");
   - a fixture's facing arrow dragged round to point it, on the plot and in
     Fixture Properties ("would be nice to grab the arrow showing direction
     and drag it around to point correctly"). */
class BoxFacing_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    // The Box
    void addBoxSitsBesideStepsWithItsDefaults();
    void aBoxSitsOnARiserAndDuplicatesAsABox();
    void turningAndMovingABoxKeepsItsFront();
    void aStripOnTheFrontLaysItsLedsAcrossIt();
    void theTopIsClear();
    void labelsNeverWrapAndNeverCoverTheFront();

    // The facing arrow
    void theArrowPointsWhereTheBeamGoes();
    void draggingTheArrowHeadTurnsTheFacingInOneUndoStep();
    void theHandleIsOnlyTheHeadAndOnlyWhenItCanTurn();
    void theDialInFixturePropertiesDragsAndFollowsTheField();

    void picturesForReview();
};

#endif
