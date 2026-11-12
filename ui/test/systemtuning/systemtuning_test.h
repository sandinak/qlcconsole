/*
  Q Light Controller Plus - Test Unit
  systemtuning_test.h

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

#ifndef SYSTEMTUNING_TEST_H
#define SYSTEMTUNING_TEST_H

#include <QObject>

class Doc;

class SystemTuning_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    /* The check, per operating system, from a made-up probe */
    void linuxTimerRefusedSaysHowToGrantIt();
    void linuxTimerGrantedIsGood();
    void linuxGovernorWifiKernelAndPi();
    void macLowPowerModeAndAppNap();
    void windowsPowerPlans();
    void everySystemListsTheTimerFirst();
    void importantProblemsAreOnlyTheImportantOnes();

    /* The real probe on this machine */
    void theRealProbeKnowsThisSystem();
    void theLookRunsBelowTheShow();

    /* Settings */
    void settingsDefaultsAndAreRemembered();

    /* Runtime: full speed and Show Status during Operate */
    void fullSpeedIsHeldOnlyDuringOperate();
    void keepDisplayAwakeGoesWithTheHold();
    void showStatusSaysAnImportantProblemOnlyInOperate();
    void showStatusCanBeTurnedOff();

    /* Settings ▸ Performance */
    void thePageListsTheItemsAndCopiesTheCommand();
    void thePageSavesItsSettings();
    void theFullSpeedBoxUpdatesTheList();

    void toolOutputIsRead();
    void frameTimingCountsLateAndDropped();
    void measuredItemFollowsTheLateFrames();
    void universeThreadsAdviceFollowsTheNumbers();
    void theMachineItemKnowsTheArchitecture();
    void batteryAndHeatAndThePi();
    void wifiMattersMostWhenItCarriesDmx();
    void localNetworkOnlyWhenEveryOutputFails();
    void screenLockFollowsKeepDisplayAwake();
    void theKeepAwakeBoxUpdatesTheList();
    void backupsRunningIdleAndBuiltIn();
    void updatesPendingRunningAndScheduled();
    void otherProgramsAddUpByName();
    void memoryDiskAndRigSettings();
    void hostLookRunsOffTheUiThread();
    void aLightLookKeepsTheDeepAnswers();
    void theRealLookIsBounded();

private:
    Doc *m_doc = nullptr;
};

#endif
