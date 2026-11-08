/*
  Q Light Controller Plus
  freestring_test.h

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

#ifndef FREESTRING_TEST_H
#define FREESTRING_TEST_H

#include <QObject>

/* A Free string pixel path (PixelPath::FreeString): a WLED fairy string
   laid anywhere on the stage through anchors placed on the plot -- the
   inspector's rows, laying it with Add Anchor, dragging an anchor, the
   anchor's menu, the anchor table and Write Segments (against a fake WLED
   on this computer; never a real device). */
class FreeString_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void theFreeStringIsOfferedWithoutAFountainAndSaysWhatToDo();
    void addAnchorThenClicksOnThePlanLayTheString();
    void draggingAnAnchorMovesItInOneUndoStep();
    void theAnchorMenuPinsReversesInsertsAndDeletes();
    void theAnchorTableTakesExactValuesInSettingsUnits();
    void addAnchorFromADialogClosesItSoThePlanCanBeClicked();
    void writeSegmentsWritesASegmentPerSpanWithItsOwnReverse();
    void picturesForReview();
};

#endif
