/*
  Q Light Controller Plus - Unit test
  controlmap_test.h

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

#ifndef CONTROLMAP_TEST_H
#define CONTROLMAP_TEST_H

#include <QObject>

class IOPluginStub;
class Scene;
class Doc;

class ControlMap_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void learningBindsTheNextPress();
    void toggleStartsAndStops();
    void momentaryIsOnWhileHeld();
    void startOnlyAndStopOnly();
    void oneControlOneTarget();
    void theLightFollowsWhatItFires();
    // CS3: the light says ready, running, held and stopped, each from the
    // one place that owns the answer (docs/design/SCOPED_CONTROLS_2026-11-22.md)
    void theLightSaysReadyRunningAndDark();
    void aLightIsDarkWhereAPressWouldBeRefused();
    /** A desk whose profile gives its buttons a dim level (the PMJ's
     *  15/127): a hit's key is lit only while its place is live, dim
     *  otherwise -- not dim at the board's idle glow when it is ready. */
    void hitLampLitOnlyInItsPlace();
    /** APC mini mk2 / Launchpad Mini MK3 (a colour as their LowerValue):
     *  their lamps exactly as before the PMJ rule. */
    void padsWithALowerValueKeepTheirLamps();
    void theLightGoesDarkAfterTheStopUntilTheNextGo();
    void aHeldGoBlinksTheControlTwice();
    void aCueListWithNothingToGoIsDark();
    void theDimLightIsSwitchableAndSavedWithTheShow();
    void theLightsComeBackWhenTheControllerDoes();
    // the independent review's lamp findings
    void aNewShowTakesTheLightsWithIt();
    void theDimSwitchNeverDarkensTheStop();
    void manyFunctionsAtOnceAreOneLightPass();
    void aLightThatDidNotChangeIsNotSentAgain();
    void aCueListGoesAndACueJumps();
    void aCueMadeASubCueStillJumps();
    void aHitFires();
    void savedWithTheShowAndLoadedBack();
    void aShowWithoutBindingsSavesAsBefore();
    void aDeletedFunctionTakesItsBinding();
    void nothingListensWithoutBindings();
    void namesSayWhatAControlIs();
    void aComputerKeyIsAControl();
    void aKeyFiresInOperateOnly();
    void aKeyHitFiresAsItsMidiTriggerDoes();
    void aKeyIsArmedInDesignByTheRunList();
    void aKeyIsArmedInDesignByTheSelection();
    void keysAreSavedWithTheShow();

    // CS1: the surface safety floor (docs/design/CONTROL_SURFACES_AUDIT_2026-11-17.md)
    void aSurfacePressFollowsTheModeAndTheArming();
    void aRefusedPressHasNothingToRelease();
    void aRefusedPressSaysSo();
    void whatRunsCanAlwaysBeLetGo();
    void aControllerGoneLetsGoOfWhatItHeld();
    void theStopIsLearnedOnAButtonWithASecondPress();
    void theStopRefusesAFaderWhateverItSends();
    void theStopRefusesAControlTheProfileCallsAFader();
    void everyPressOfTheStopIsAllOff();
    void theStopIsNotHeldByTheDoubleGoGuard();
    void theStopSurvivesAReplugAndRepeats();
    void aStopOnAProfileFaderStillFires();
    void aRefusedAllOffDoesNotHoldBackTheNext();
    void aStopWithNoIdLoadsAsTheStop();
    void theStopIsSavedWithTheShow();
    void aKeyFollowsTheSurfaceRule();
    // the independent review's findings
    void theStopWorksWhileALearnIsListening();
    void aRealSecondPressIsNotDebounced();
    void aStopOnAProfileKnobFiresAndSaysSo();
    void aBoundFaderIsFlaggedLoudlyAndStillStops();
    void theStopFiresPerThePressRule();
    void aLostReleaseAtLearnTimeDoesNotSwallowTheStop();
    void aDeviceThatDropsItsZerosStillStops();
    void aStopWithAnotherIdLoadsAsTheOneStop();
    void aSecondValueAfterTheLearnPressEndsTheSwallow();
    void aLearnTakesAHeldControlAndLetsGoOfIt();
    void aResetLetsGoOfTheInput();
    void aBindingRemovedWhileHeldStillLetsGo();
    void aStopOnAKeyDoesNothingAndSaysSo();
    void aHeldRefusedPressSaysItOnce();
    void aReplugFreesThePressThatTaughtTheStop();
    // Phase R (CONTROL_SURFACES_EVAL §7): recovery
    void stopStateDumpOnReplug();
    /** A replug's dump of "pressed" on a learned GO fires nothing (said);
     *  a release then press inside the window fires one GO. */
    void triggerIgnoresTheReplugDump();
    /** The dump's two shapes ({127}, OpenDeck's {0, 127, 0}) inside the
     *  burst GO nothing; a release and press after it, one GO. */
    void aTriggerIgnoresTheReplugBurst();
    void stopDeviceMissingSaysWhich();
    void aReleaseActsOnWhatWasPressed();
    // Places for triggers (Q295, Q296, Q303; CONTROL_SCOPES phase 3)
    void aTriggerAtACueIsArmedOnlyWhileItIsLive();
    void theNarrowestArmedTriggerWins();
    void twoAtOnePlaceStepThrough();
    void placesAreSavedAndAnOldShowIsEverywhere();
    void theStopIsEverywhereAndAboveEveryPlace();
    void aKeyAtAPlaceFollowsItToo();
    void aKeyOutOfItsPlaceFollowsTheKeyRule();
    void twoPlacesOfOneScopeDoNotStepThrough();
    void aHitLearnedWiderThanItsPlaceDoesNotHoldTheControl();
    void aPlaceTakenIsNeverMovedOnto();
    void aTriggerWhosePlaceIsDeletedWidensAsAHitDoes();
    void aPlaceFromANewerVersionIsKeptAndNeverFires();
    void aHitsTriggerFollowsTheHitWhenItMoves();
    void aPlaceThatGoesNeverWidensOntoATakenOne();
    // Phase F (CONTROL_SURFACES_EVAL §6): a cue list's Back, and what a
    // Learn hears (a fader, a latching button)
    void cueListBackStepsBack();
    void backAndGoOnOneListCoexist();
    void aLearnHearsAFaderAndALatchingButton();

private:
    /** A press (value) or a release (0) on @a channel of input universe 0,
     *  through the stub plugin and the input patch, as a controller sends. */
    void send(quint32 channel, uchar value);
    void tick(int n = 1);
    Scene *scene(const QString &name, int channel);
    /** A controller that misbehaves: values sent in order on one control. */
    void sweepTargets(bool viaKey);
    void sendAll(quint32 channel, const QList<int> &values);
    /** A cue list of @a cues cues on one fixture group, each a Dimmer
     *  look at full: the list's id. */
    quint32 cueList(int cues, QList<quint32> *cueIds = nullptr);

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif
