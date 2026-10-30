/*
  Q Light Controller Plus - Test Unit
  performersets_test.h

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

#ifndef PERFORMERSETS_TEST_H
#define PERFORMERSETS_TEST_H

#include <QObject>

/* Performer lines in a set: one dialog changes the costume and heights of
 * every line in it; a fill's lines stay out of the aim-target lists; a plain
 * line made a performer line keeps its own heights. */
class PerformerSets_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** setMates / copyLookToSet: same group, look copied, seeds and counts kept. */
    void theSetSharesItsLook();
    /** The Edit Target dialog's whole-set box changes every line; unticked, one. */
    void oneDialogChangesTheWholeSet();
    /** A fill's lines are not in the pickers; a lone line, or one a palette
     *  or the list holds selected, is. */
    void aimPickersSkipTheChoir();
    /** The Look Editor's and the palette dialog's target list use it. */
    void thePalettePickerSkipsTheChoir();
    /** Converting a plain line keeps its points' heights. */
    void aPlainLineKeepsItsHeights();
};

#endif
