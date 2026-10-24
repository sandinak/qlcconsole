/*
  Q Light Controller Plus
  focuscalls_test.h

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

#ifndef FOCUSCALLS_TEST_H
#define FOCUSCALLS_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

class FocusCalls_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** One layout for every tool: the map by role, and back again. */
    void theControllerSchemeIsOneMap();
    /** A held D-pad arm repeats after a delay; nothing else does. */
    void aHeldArmRepeats();
    /** Feedback: lights, voice or both, cycled by a held RS; Say the light
     *  speaks whatever it is. */
    void feedbackIsLightsVoiceOrBoth();
    /** A trigger is a button with hysteresis. */
    void aTriggerPressesOnce();
    /** The show's calls survive a save and load, in order. */
    void callsAreSavedWithTheShow();
    /** A call's fixtures are its groups' then its own, once each; its name
     *  is made from them and the target when it has none. */
    void aCallNamesItself();
    /** Triggers step calls, bumpers fixtures; each call aims at its own
     *  target; Y switches to the whole call and back. */
    void runningCallsStepsCallsAndFixtures();
    /** Group scope nudges, saves and undoes the whole call as one. */
    void groupScopeWorksTheWholeCall();
    /** What is said, and the flash's pattern. */
    void theOperatorIsToldWhereTheyAre();
    /** The venue sidecar: named after the show, written on every save,
     *  read back on open and applied -- and the show file never takes a
     *  venue's corrections. */
    void correctionsGoToTheVenueNotTheShow();
    /** Switching venues puts one venue's corrections in place of the
     *  other's; none is the show as built. */
    void switchingVenues();
    /** A venue nobody named gets the date; names stay unique. */
    void venueNames();
    /** Q111: a calibration changed elsewhere (the fixture dialog) while a
     *  venue is applied goes into the venue and its file, not the show; a
     *  Focus mode nudge only when saved. */
    void anEditElsewhereGoesToTheVenue();
    /** A focus-call cue: GO arms it (said once), Start begins the calls in
     *  Operate, triggers step, past the last call it asks to leave, and
     *  leaving goes on with the list. */
    void aFocusCallCueArmsThenStartsOnTheController();
    /** Q120: no venue applied, the laptop asks; a name, or after a few
     *  seconds today's date, said. */
    void aFocusCallCueAsksForTheVenue();
    /** Q126: the pad's light bar and buzzes, "Spot 3 kept", "2 lights not
     *  saved", the change since the save and its ghost, hold A / B, both
     *  bumpers to the next unsaved light. */
    void thePadAndTheVoiceTellYouMore();
    /** The same through the programmer, as a controller's events arrive:
     *  both bumpers, A and B held, RS tapped and held, the light bar and the
     *  buzz sent to the controller's plugin. */
    void theControllerDoesItThroughTheProgrammer();
    /** Q131: warnings and refusals speak even in Lights only, with a slow
     *  long blink and one long buzz -- not a save's double blink. */
    void importantLinesAlwaysSpeak();
    /** Q129: a venue last changed over a day ago needs confirming on open;
     *  choosing or confirming it clears that. */
    void anOldVenueIsConfirmed();
    /** Gap 5: starting a controller tool never throws away another's (or
     *  its own) unsaved work: refused, saying what to do first. */
    void unsavedWorkIsNeverDiscarded();
    /** Q133: a correction remembers the fixture type and mode; a swapped
     *  fixture's is skipped and it needs focus; a deleted one's is kept as
     *  orphaned; an old file still applies, with one note. */
    void aSwappedFixtureNeedsFocus();
    /** Q128: a venue's own trims and target positions, in the sidecar and
     *  never the show; the target page moves a call's target, its heads
     *  following, and A keeps it for the venue. */
    void aVenueHasItsOwnTrimsAndTargets();
    /** Gap 8: the venue file travels -- a copy for another show file, and
     *  an earlier version's (or a Finder copy's) found and imported. */
    void theVenueFileTravels();
    /** Gap 10 and the review: the unsaved light breathes on the rig (lights
     *  feedback only); View + RS says how many are not saved; View undoes
     *  on its release, the same in every tool, and not after a chord. */
    void theUnsavedLightBreathesAndViewRsCounts();
    /** Safety: the pad dropping with a stick and A held -- everything is
     *  let go, nothing moves, no keep-all fires; said, and said when back.
     *  In Focus mode (and focus calls) and in Refine. */
    void theControllerLostFreezesEverything();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif
