/*
  Q Light Controller Plus - Unit test
  controlmapui_test.h

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

#ifndef CONTROLMAPUI_TEST_H
#define CONTROLMAPUI_TEST_H

#include <QObject>

class Scene;
class Doc;

class ControlMapUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void theLearnDialogBindsWhatItHears();
    void theLearnDialogShowsAndRemovesABinding();
    void theMenuActionSaysWhatIsBound();
    void buildOffersAssignOnlyInClassic();
    void theShowTreeOffersLearnTrigger();
    void thePanelListsChangesAndRemoves();
    void theShowControlPageHasTheMap();
    void theLearnDialogLearnsAComputerKey();
    void theLearnDialogRefusesTakenKeys();
    void aKeyFiresInOperateNotInATextField();
    void aKeyIsArmedFromTheShowTreeAndTheRunTab();
    void thePanelAndMenuListKeys();
    void theEmergencyStopLearnsOneButtonWithASecondPress();
    void aStopThatCannotWorkIsFlaggedOnTheButtonAndTheRow();
    void aStopOnAProfileFaderIsWarnedOnceAndOnlyOnItsOwnLine();
    // CS3: the panel says what each control's light shows, so the rule can
    // be read with no desk on the bench
    void thePanelSaysWhatEachLightShows();
    void thePanelSaysWhenAControlHasNoLight();
    void anEncoderLearnStopsListeningWhenThePanelHides();
    void learnKeyShots();
    void theSurfacesHubListsEveryPlace();
    void eachSetUpLandsOnItsPlace();
    void pmjRowMatchesExactProfileOnly();
    void grandMasterSetUpOpensGrandMasterTab();
    void grandMasterRowFollowsItsSetUp();
    void unpatchedDeviceListedWithSetUp();
    void outputDevicesAreNotControllers();
    void virtualPortsAreNotControllers();
    void otherDevicesRowHidesWhenNothingWaits();
    void theSurfacesStatusFollowsTheShow();
    void surfacesOffKeepsTheStop();
    void surfacesShots();
    // One job per control (Q297)
    void aTriggerLearnRefusesAWidgetsControlAndMovesIt();
    void aPmjControlIsRefusedWithNoMoveItHere();
    void theEncoderLearnRefusesAndMoves();
    void theInputPickersRefuseAndMove();
    void aMoveInAPropertiesWindowWaitsForOk();
    void theStopIsNeverTakenButCanTake();
    void theHubListsControlsWithTwoJobs();
    void collisionShots();
    // Places for triggers (Q295, Q296, Q303)
    void theLearnDialogOffersThePlace();
    void theShowTreeLearnsWhereYouRightClicked();
    void aGoLearnedFromTheTreeFiresInOperate();
    void onlyTheSamePlaceIsRefused();
    void aSteppingTriggerReopensAndSavesOnItsOwnControl();
    void moveItHereLetsGoOfASubControlsOwnInput();
    void theTableAndTheHubShowEachPlace();
    void theLightFollowsWhatIsArmedNow();
    void placeShots();
    void theFiresColumnNeverCutsAName();
    void cancelInTheCollisionBoxLeavesBothTriggers();
    void theHubSaysAPlaceDeletedOrFromANewerVersion();
    void moveItHereIsAllOrNothing();
    void anEncoderPresetAsksWhoHasItsControls();
    void anEncoderRefusalNamesEachOwnersControl();
    void aMoveForEachPresetIsTakenOnOk();
    void anEncoderLearnOnAnotherUniverseAsksForTheWholeBank();
    // Phase F (CONTROL_SURFACES_EVAL §6): Back, the kind of control a Learn
    // hears, and this computer's own owners (the pad, the app's keys)
    void aListLearnsItsGoAndItsBack();
    void goOnFaderRefused();
    void latchingButtonSuggestsStartOnly();
    void padRunButtonRefused();
    void keyboardOwnerNamesTheShortcut();
    void phaseFShots();
    // Phase A (CONTROL_SURFACES_EVAL §6, §7): Assign to Control… (Q310, Q325)
    void pressSelectsFiresNothing();
    void faderToZeroFiresNothing();
    void ownerShownBeforePress();
    void faderRefusedForGoAndHit();
    void latchingButtonSuggestsMomentary();
    void companionHintOnRunClient();
    void pressAnyControlNamesOwners();
    void phaseAShots();
    // Phase R (CONTROL_SURFACES_EVAL §6, §7): recovery
    void hubSaysTheStopsDeviceIsMissing();
    void recoveryShots();

private:
    Scene *scene(const QString &name);

private:
    Doc *m_doc = nullptr;
};

#endif
