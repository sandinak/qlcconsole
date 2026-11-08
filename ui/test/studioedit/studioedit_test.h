/*
  Q Light Controller Plus
  studioedit_test.h

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

#ifndef STUDIOEDIT_TEST_H
#define STUDIOEDIT_TEST_H

#include <QObject>

/* Editing stage features by hand in the Lighting Studio (docs/features/
   lighting-studio.md "Stretch and turn by hand", "Grouping stage
   features"): the stretch and rotate handles on a platform, a truss and
   scenery -- shown only while nothing holds it still, a readout in
   Settings' unit, one undo step a drag -- and Group ▸ on the plot, on an
   object and on a Studio canvas: New Group…, add to a group already on the
   layer, Remove from Group, greyed with the reason when a lock stops it. */
class StudioEdit_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void aShapeStretchesFromTheOppositeSideAndSnaps();
    void handlesShowOnlyWhileNothingHoldsIt();
    void stretchingAPlatformIsOneUndoStepWithAReadoutInSettingsUnits();
    void theKnobTurnsInFifteenDegreeStepsUnlessAltIsHeld();
    void aTrussStretchedAtItsNearEndKeepsItsLightsWhereTheyAre();
    void aCycStretchesButDoesNotTurnAndAGroupTurnsAsOne();
    void groupMenuMakesAddsToAndRemovesFromGroupsOneUndoEach();
    void groupMenuIsGreyedWithTheReasonWhenALockStopsIt();
    void theCanvasMenuHasGroupInPlaceOfGroupSelected();
    void aStudioCanvasOffersGroupForItsStructure();
    void theBoomTopHandleIsHiddenWhileLocked();
    void picturesForReview();
};

#endif
