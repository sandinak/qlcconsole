/*
  Q Light Controller Plus - Test Unit
  patchinventorydialog_test.h

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

#ifndef PATCHINVENTORYDIALOG_TEST_H
#define PATCHINVENTORYDIALOG_TEST_H

#include <QObject>

class Doc;

class PatchInventoryDialog_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void opensOnAProposalForEveryStagedFixture();
    void onlyTheGivenFixturesAreListed();
    void editingIntoACollisionFlagsItAndBlocksApply();
    void applyPatchesTheFixtures();
    void replanDiscardsEdits();
    void aGenericDimmersChannelsAreChosenHere();
    void aModeIsChosenFromTheDefinitionsModes();
    void modesTheFixtureCannotMoveToAreOfferedButDisabled();
    void aSceneThatSetsChannelsLocksTheChannelCount();

    void replanKeepsTheModesChosen();
    void changingOneRowsModeLeavesTheOthers();
    void aSingleModeFixtureShowsItsModeAsText();
    void aCollisionWithTheShowIsFlaggedToo();
    void newUniversesAndNotesAreSpelledOut();
    void problemsDuringApplyAreReported();

    void nearestNodeProposalsShowWhereTheyGoAndApply();
    void untickingNearestNodeProposesFirstFit();
    void aHandEditedUniverseDropsTheProposedPort();
    void withNoNodesOnThePlotThereIsNothingToTick();

private:
    Doc *m_doc;
};

#endif
