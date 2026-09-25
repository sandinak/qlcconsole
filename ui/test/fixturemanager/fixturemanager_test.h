/*
  Q Light Controller Plus - Test Unit
  fixturemanager_test.h

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

#ifndef FIXTUREMANAGER_TEST_H
#define FIXTUREMANAGER_TEST_H

#include <QObject>

class Doc;

class FixtureManager_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Right-clicking the Inventory root must offer inventory actions, and NOT
     *  the generic "Add fixture..." (which patches, and asks for a universe). */
    void rightClickOnInventoryOffersInventoryActionsNotAGenericAdd();
    /** ...while the same click on the Universes root still offers the normal add. */
    void rightClickOnUniversesStillOffersAFixtureAdd();
    /** The toolbar's plain "Add fixture" with the Inventory root selected stages
     *  the fixture instead of asking for a universe and address. */
    void addFixtureWithInventorySelectedStagesIt();

    /** A patched fixture's right-click offers Return to inventory; a staged one's does not. */
    void returnToInventoryIsOfferedForPatchedFixturesOnly();
    /** Confirming it releases the fixture's address and keeps the fixture. */
    void returningToInventoryReleasesTheAddress();

    /** Right-clicking a manufacturer or model folder in the inventory offers to
     *  patch every staged fixture beneath it. */
    void rightClickOnAnInventoryFolderOffersToPatchWhatIsInIt();
    void choosingReturnToInventoryFromTheMenuReleasesIt();
    void choosingAddToInventoryFromTheMenuStagesAFixture();
    void choosingPatchFromTheMenuPatchesThoseFixtures();
    void aNetworkFixturesInfoStatesItsConnection();
    void theInventoryActionsFollowTheAddAction();
    void editingAStagedFixtureKeepsItStaged();
    void deletingInventoryFixturesUpdatesTheFolderCounts();

    /** Deleting a fixture that lives under a universe folder (the
     *  Fixture Manager tree's default shape) must actually remove its
     *  row, not just the underlying Doc object. */
    void deletingAFixtureRemovesItsRowUnderItsUniverse();
    /** Clicking a fixture group opens its layout editor. */
    void selectingAGroupOpensItsLayoutEditor();
    /** Integration test for the fixture-group workflow: create two groups
     *  through the real "Create Fixture Group" dialog (the shared factory
     *  Fixture Manager, the Programming tab and the Studio all use), open
     *  one's real layout editor by selecting it (as
     *  selectingAGroupOpensItsLayoutEditor() already proves opens it),
     *  click a real empty cell to set where the next paste lands, paste
     *  the other group in as a block through the real "Add group as
     *  block..." button's own handler and its real picker dialog, then
     *  move that block as a unit and confirm every head landed exactly
     *  where the geometry says. */
    void populatingAndMovingAFixtureGroupBlockThroughRealUi();
    /** The sibling workflow test above deliberately anchors its paste on an
     *  empty cell to sidestep collision behaviour (see its own step-5
     *  comment). This test asks the question directly: paste a block
     *  anchored so it lands on a cell another fixture already occupies, and
     *  confirm what actually happens to the occupant. */
    void pastingABlockOntoAnOccupiedCellOverwritesTheOccupant();
    /** The real UI gesture for moving a sub-group is a mouse drag, which
     *  internally builds a genuine QDrag/QDropEvent session
     *  (FixtureGroupEditor::startCellDrag()) -- a materially bigger, riskier
     *  thing to fake under offscreen QTest than synthetic mouse events (no
     *  window manager, no real drop target to negotiate with). This drives
     *  the real Drop branch of eventFilter() with a synthetic QDropEvent
     *  instead, exercising its actual dx/dy-from-drop-position math and
     *  canMoveHeads() guard while skipping QDrag::exec() itself. */
    void draggingASubGroupBlockMovesItThroughTheRealDropPath();

private:
    Doc* m_doc;
};

#endif
