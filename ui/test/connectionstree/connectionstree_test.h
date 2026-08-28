/*
  Q Light Controller Plus - Test Unit
  connectionstree_test.h

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

#ifndef CONNECTIONSTREE_TEST_H
#define CONNECTIONSTREE_TEST_H

#include <QObject>

class Doc;

class ConnectionsTree_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /* Construction and the rebuild contract */
    void initial();
    void refreshLeavesEditingEnabled();

    /* Visibility gating -- a hidden tab must cost nothing */
    void timerOnlyRunsWhileVisible();
    void firstShowRescansAndRestartsTheClock();

    /* Rescan reaches every plugin, not just the ones with a dialog */
    void rescanReachesEveryPlugin();

    /* Which lines are worth showing */
    void unpatchedLineIsHiddenByDefault();
    void hardwareLineIsShownWithoutAPatch();
    void newlyAppearedLineIsShown();
    void showUnusedRevealsEverything();

    /* Discovered and hand-declared targets */
    void discoveredDeviceAppearsUnderItsLine();
    void manualTargetIsRendered();
    void discoveredTargetWinsOverManualOne();
    void manualTargetPortsAreRendered();

    /* Labels */
    void lineAliasIsShownOnTheRow();
    void targetAliasIsShownOnTheRow();

    /* Default expansion and its memory */
    void defaultExpansionStopsBelowTheInterface();
    void operatorExpansionSurvivesRebuild();

    /* The "still searching" banner */
    void bannerHidesOnceSomethingIsHeard();

    /* Repointing an existing patch */
    void retargetWritesTargetOntoTheExistingPatch();
    void retargetToBroadcastRemovesTheParameters();

    /* Plugin parameters previously reachable only from Overview */
    void patchParameterDistinguishesAbsentFromZero();
    void setPatchParameterWritesOutputAndInputPatches();

    /* Bulk retarget across a selection */
    void selectedUniversesRejectsAMixedPluginSelection();
    void selectedUniversesDeduplicatesFannedOutUniverses();
    void bulkRetargetNumbersPortsUpwardAndIsOneUndoStep();

    /* Live input activity */
    void inputActivityTintsTheUniverseRow();

    /* Tooltip rendering */
    void propertyTooltipRendersPairs();
    void deviceRowCarriesItsPropertiesAsATooltip();

private:
    Doc *m_doc;
};

#endif
