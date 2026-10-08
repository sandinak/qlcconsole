/*
  Q Light Controller Plus - Test Unit
  fixturetreewidget_test.h

  Copyright (c) Massimo Callegari

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

#ifndef FIXTURETREEWIDGET_TEST_H
#define FIXTURETREEWIDGET_TEST_H

#include <QObject>

class Doc;
class FixtureTreeWidget_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void treeCounts();
    /** A rebuild must not silently drop the selection: the Fixture Manager's
     *  group layout editor depends on the tree still agreeing with it. */
    void selectionSurvivesARebuild();

    /** Enter on a "Universe N" row arms it editable and renames the
     *  universe once the edit commits. */
    void enterOnAUniverseRowRenamesIt();
    /** An empty or unchanged edit must not blank the universe's name. */
    void emptyUniverseEditIsRejected();

    /** Staged fixtures live under an Inventory root and never under a
     *  universe, whose placeholder 0/0 would otherwise put them in universe 1. */
    void inventoryFixturesGetTheirOwnRootNotAUniverse();
    /** The manager always shows an Inventory root, even when it is empty. */
    void inventoryRootIsAlwaysPresentInTheManager();
    /** Channel selection has no use for a fixture with no address. */
    void channelSelectionSkipsInventory();
    /** The inventory reads as manufacturer -> model -> fixture, alphabetical at
     *  every level, with a count on each folder. */
    void inventoryIsOrganisedByManufacturerAndModel();
    /** A fixture whose file address clashed is listed apart, at the top. */
    void addressClashesGetTheirOwnRootAtTheTop();
    void duplicateNamesAreListedAndARenameToATakenNameIsRefused();
    /** Fixtures or a group dropped on a universe, or on a fixture in one. */
    void aDropOnAUniverseNamesItAndItsFixtures();

private:
    Doc* m_doc;
};

#endif
