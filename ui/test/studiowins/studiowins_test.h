/*
  Q Light Controller Plus
  studiowins_test.h

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

#ifndef STUDIOWINS_TEST_H
#define STUDIOWINS_TEST_H

#include <QObject>

/* The small Lighting Studio wins from the Capture comparison
   (docs/features/lighting-studio.md): saved camera views, frost and colour
   temperature in the beam, shutter and strobe in the Rig Overview, numbering
   in plot order, patch clashes on the DMX overlay, label fields on the plot. */
class StudioWins_Test final : public QObject
{
    Q_OBJECT

private slots:
    void savedViewsRoundTripWithTheShow();
    void theViewsMenuAddsAppliesRenamesAndDeletes();

    void frostSoftensTheEdgeAndFadesTheGoboMask();
    void ctoWarmsAndCtbCoolsTheBeam();
    void frostAndCtoAreReadFromTheChannels();
    void aClosedShutterDrawsNoBeamInTheRigOverview();
    void aStrobeBlinksInTheRigOverview();

    void numberingFollowsThePlotOrderAndOneUndoRestores();
    void numberingCanPatchInThatOrderIntoFreeAddresses();
    void theNumberItemIsOnTheSelectionMenuOfThePlot();

    void theDmxOverlayOutlinesPatchClashesAndFixingClearsThem();

    void labelFieldsRoundTripWithTheShow();
    void theLabelSaysTheChosenFieldsAroundTheIcon();
};

#endif
