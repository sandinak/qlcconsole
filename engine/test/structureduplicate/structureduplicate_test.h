/*
  Q Light Controller Plus - Test Unit
  structureduplicate_test.h

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

#ifndef STRUCTUREDUPLICATE_TEST_H
#define STRUCTUREDUPLICATE_TEST_H

#include <QObject>

class Doc;

class StructureDuplicate_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void namesFollowTheStructure();
    void aTowerIsDuplicatedToTwo();
    void theCopiesAreMountedLikeTheOriginals();
    void nextFreeFollowsTheOriginals();
    void anOffsetPatternIsApplied();
    void anOffsetThatClashesLeavesTheCopyUnpatched();
    void unpatchedLeavesThemInTheInventory();
    void groupsTakeTheCopiesOrGoParallel();
    void aGroupNamedForTheStructureIsAlwaysParallel();
    void mapGroupsFollowTheStructure();
    void anOccupiedTargetIsSkippedByDefault();
    void anOccupiedTargetCanTakeMore();
    void anOccupiedTargetCanBeReplaced();
    void replaceCountsTheScenesThatLoseLights();
    void undoPutsEverythingBack();
    void aDeckCarriesItsLightsToTheOtherDeck();
    void nothingToCopyIsSaid();
    void hangersAndChildBarsAreCopied();
    void replaceTakesTheTargetsHangersAndBarsAndUndoPutsThemBack();

private:
    Doc *m_doc = nullptr;
};

#endif
