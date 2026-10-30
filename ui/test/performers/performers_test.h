/*
  Q Light Controller Plus - Test Unit
  performers_test.h

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

#ifndef PERFORMERS_TEST_H
#define PERFORMERS_TEST_H

#include <QObject>

/* People on the risers (PerformerFill, Monitor::fillWithPeople): rows of
 * Person targets on every tread, in a Performers layer, as one set that
 * moves with the riser, lit in costume in the rig view. */
class Performers_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** A performer line's count stands evenly along it, one on each end. */
    void countSpreadsEvenly();
    /** Each person's feet on the highest surface under them: the usual
     *  choir -- three riser tiers and the apron, four lines -- and a line
     *  up the steps. */
    void feetStandOnTheHighestSurface();
    /** Heights a mean give or take the spread, the same every time and
     *  after save and load; another seed, others. */
    void heightsVaryAndStayPut();
    /** Fill with people drops a line per tier (staggered, by spacing or per
     *  row) in the Performers layer as one set; again replaces it. */
    void fillDropsALinePerTier();
    /** The layer's eye hides them on the plot and in the rig view; its
     *  lock holds them still. The plot draws the people. */
    void theLayerHidesAndLocksThem();
    /** Moving or turning the riser carries its people; Undo carries them
     *  back; moved out from under them, their feet come down. */
    void movingTheRiserMovesThem();
    /** The set moves as one piece. */
    void theSetMovesAsOnePiece();
    /** Count, seed, spread and costume survive save and load; a plain line
     *  and a mark are written as before. */
    void saveAndLoadRoundTrip();
    /** Light on a costume reads in the costume's colour in the rig view. */
    void aCostumeTakesTheLightInItsColour();
    /** The dialog counts what it will make; the Add menu's Add People
     *  makes a line, or fills the selected risers. */
    void theDialogAndTheAddMenu();
    /** Any line becomes a performer line in its own dialog. */
    void anyLineTakesPeopleInItsDialog();
    /** 70 people do not slow the rig view or the plot noticeably. */
    void manyPeopleRenderCost();
    void keptFacesDrawTheSamePicture();
};

#endif
