/*
  Q Light Controller Plus - Test Unit
  bandtargets_test.cpp

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

#ifndef BANDTARGETS_TEST_H
#define BANDTARGETS_TEST_H

#include <QObject>

/* Band-instrument stage targets: Keyboard, Grand and Upright piano, Rack and
 * Speaker, beside the Drum Kit. */
class BandTargets_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Each kind's default footprint, aim height and body height. */
    void sizesAndHeights();
    /** Kind strings, footprint and turn survive save and load; unknown degrades. */
    void saveAndLoadRoundTrip();
    /** An Aim at one of them lands at its aim height above the deck. */
    void aimAtLandsAtTheAimHeight();
    /** Each draws as itself on the plot, from above and in an elevation. */
    void drawnOnThePlot();
    /** ] and [ turn the gear, not the other targets. */
    void turnsWithTheBrackets();
    /** Each adds a solid shape to the rig view. */
    void drawnInTheRigView();
    /** Writes the showband pictures when QLC_BAND_SHOTS is set. */
    void showbandShots();
    /** Writes the Edit Target dialog pictures when QLC_BAND_SHOTS is set. */
    void editTargetDialogShots();
};

#endif
