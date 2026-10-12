/*
  Q Light Controller Plus
  speedmasters_test.h

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

#ifndef SPEEDMASTERS_TEST_H
#define SPEEDMASTERS_TEST_H

#include <QObject>

class Doc;

class SpeedMasters_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void freshMastersRunOnTheirOwnAt120();
    void anOwnMasterRunsAtItsTempoAndRate();
    void aFollowerIsLockedToWhatItFollows();
    void followLoopsAreRefused();
    void pauseFreezesAndResumes();
    void tapSetsAnOwnMasterNotAFollower();
    void goingOwnCarriesOnFromTheFollow();
    void mastersAreSavedWithTheShow();
    void assignmentsAreSavedOnFunctionsAndSteps();
    void theInnermostChoiceWins();
    void aChaseHandsItsMasterToItsSteps();
    void aCollectionHandsItsMasterToItsMembers();
    void aBeatChaseStepsOnItsMastersBeats();
    void aGoLandsOnTheNextBeat();
    void aGoOnTheBarWaitsForTheBar();
    void goQuantizeIsSavedWithTheChase();
    void aShowUsesFourToSixteen();
    void aMasterFollowsTheShowTimeline();
    void aControlSurfaceDrivesAMaster();
    void onlyTheMastersSomethingIsOnAreUsed();

private:
    Doc *m_doc = nullptr;
};

#endif
