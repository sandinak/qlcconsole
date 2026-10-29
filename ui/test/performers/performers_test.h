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
    /** A 3-tier flight: a row on every tread, each at its tread's height,
     *  the bottom row downstage; by spacing (staggered) and per row. */
    void fillStandsARowOnEveryTier();
    /** A deck holds a row to every 0.9 m of depth; a turned riser turns
     *  its rows; a rounded front cuts the corners. */
    void deckRowsAndTurnedRisers();
    /** They land in a "Performers" layer (made once) as one named group,
     *  with the height and costume asked for. */
    void theyAreOneSetInThePerformersLayer();
    /** Filling again replaces the set, keeping one an aim palette uses. */
    void fillingAgainReplaces();
    /** The layer's eye hides them on the plot and in the rig view; its
     *  lock holds them still. */
    void theLayerHidesAndLocksThem();
    /** Moving the riser carries its people; Undo carries them back. */
    void movingTheRiserMovesThem();
    /** The set moves as one piece. */
    void theSetMovesAsOnePiece();
    /** Height and costume survive save and load; a show without them is
     *  read and written as before. */
    void saveAndLoadRoundTrip();
    /** Light on a costume reads in the costume's colour in the rig view. */
    void aCostumeTakesTheLightInItsColour();
    /** The dialog counts what it will make; the Add menu's Person entry
     *  becomes the fill with risers selected. */
    void theDialogAndTheAddMenu();
    /** 60 people do not slow the rig view or the plot noticeably. */
    void manyPeopleRenderCost();
};

#endif
