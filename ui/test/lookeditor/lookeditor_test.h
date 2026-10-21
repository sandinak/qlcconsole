/*
  Q Light Controller Plus - Test Unit
  lookeditor_test.h

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

#ifndef LOOKEDITOR_TEST_H
#define LOOKEDITOR_TEST_H

#include <QObject>

class LookEditor_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** An Aim look on a line shows the line's Spread; the slider and the
     *  box move together, write the look's param, and the strip shows where
     *  the lights land. A point target hides them. */
    void aLineLooksSpreadIsSetOnTheAimPage();
    /** Group, Cross, Share and Order are set on the Aim page, each its own
     *  param, and the strip shows the crossing. */
    void aLineLooksGroupingIsSetOnTheAimPage();
    void aColourDarkerThanAFifthIsCalledOut();
    void aDarkRedLookWarnsUntilItIsBrightened();
    /** A colour look offers the colour ring's slots by name (Wheel /
     *  ring): picking a chase makes the look name it; a colour slot sets the
     *  picker too; "Nearest to the colour" goes back. */
    void aColourLookCanPickARingSlot();
    void anAimLookCanDimLightsThatCannotSeeItsTarget();
    /** "if I create a gobo it doesn't give me a gobo selector?" A gobo made
     *  from the palette list has no look yet; the picker falls back to the
     *  workspace's fixtures, with a chooser when several wheels differ. */
    void aNewGoboOffersTheWorkspacesWheels();
    /** A slot a unit has fitted with other glass (Gobo Wheels…) is listed
     *  as that glass, by name, with its picture. */
    void aSwappedSlotShowsItsOwnGlass();
    /** "when selecting an effect we need a better way than a drop down ...
     *  AND for WLED based fixtures we should be selecting WLED effects". */
    void effectsArePickedFromABrowserWithWledsOwn();
    /** "can we put the sets in alpha order?" */
    void anEffectsTextIsTypedIn();
    void anEffectsPictureIsShownAndCleared();
    void previewsShowWhatEachEffectDoes();
    void anEffectIsPutOnASpeedMaster();
    void aLooksSpeedMasterAndSpreadSurviveADoubleClick();
    void theHostSpreadStandsInForTheEffectsOwn();
    void theSpreadIsPreviewed();
    void theEffectPickerIsAWindowOfItsOwn();
    void wledOnlyEffectsNeedAWledTarget();
    void theBrowsersListsAreAToZ();
};

#endif
