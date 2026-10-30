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
    /** A new Shutter look shows the type's Open (what it sends); picking
     *  Closed stores a value that closes, a Closed at 0 too; a strobe-only
     *  shutter is not offered (its one "Strobe" is a Strobe look's). */
    void aShutterLookOffersClosed();
    /** A slot a unit has fitted with other glass (Gobo Wheels…) is listed
     *  as that glass, by name, with its picture. */
    void aSwappedSlotShowsItsOwnGlass();
    /** "when selecting an effect we need a better way than a drop down ...
     *  AND for WLED based fixtures we should be selecting WLED effects". */
    void effectsArePickedFromABrowserWithWledsOwn();
    /** "can we put the sets in alpha order?" */
    void anEffectsTextIsTypedIn();
    /** Q95: New script… / Edit script… under one Script ▾. */
    void scriptsAreUnderOneScriptButton();
    void anEffectsPictureIsShownAndCleared();
    void previewsShowWhatEachEffectDoes();
    void anEffectIsPutOnASpeedMaster();
    void ownSpeedHasASetting();
    void aLooksSpeedMasterAndSpreadSurviveADoubleClick();
    void theHostSpreadStandsInForTheEffectsOwn();
    void theSpreadIsPreviewed();
    void theEffectPickerIsAWindowOfItsOwn();
    void aNewEffectLookStartsAsANewEffect();
    void thePickerOpensWhenTheEditorIsShown();
    void wledOnlyEffectsNeedAWledTarget();
    void theBrowsersListsAreAToZ();
    /** Where an effect came from: Built-in · Mine · This show filters, a
     *  "mine" mark and the palette mark, "based on Color Chase" with what
     *  differs on hover; one of the show's is copied in when picked. */
    void anEffectSaysWhereItCameFrom();
    /** Runs: Loop | Once | N times, At the end: Hold | Release, Length --
     *  on every effect, kept on the look. */
    void anyEffectRunsOnceOrNTimes();
    void anEffectSettingTakesATypedValue();
    void aWavesLengthIsOneCycle();
    /** "if we go from Red to Dark Red .. we should auto update the title ..
     *  only update the color part .. the folder should stay consistent."
     *  The name field follows live; the value and the name are one Undo. */
    void aLooksNameFollowsItsColour();
    /** A name of the user's own is never rewritten. */
    void aCustomNameStays();
    /** "Intensity 45%" -> "Intensity 60%"; "Strobe 40%" -> "Strobe 70%". */
    void aLevelInTheNameFollowsTheLevel();
    /** A new colour in a folder whose colours share a naming is named
     *  their way ("Act2 Red", "Act2 Blue" -> "Act2 Amber"). */
    void aNewColourIsNamedLikeItsSiblings();
};

#endif
