/*
  Q Light Controller Plus - Test Unit
  programmingmanager_test.h

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

#ifndef PROGRAMMINGMANAGER_TEST_H
#define PROGRAMMINGMANAGER_TEST_H

#include <QObject>

class Doc;

class ProgrammingManager_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** "fixtures and groups needs a text field search ... that narrows the
     *  view similar to search the tree for layers". */
    void fixturesAndGroupsNarrowToASearch();

    /** "clicked on a fg .. it opened the fg view .. clicked back on an
     *  effect .. and I can't get back to the effect." */
    void fromAGroupsViewTheSceneAndItsEffectComeBack();
    /** Transitions set from the scene's canvas. */
    void aScenesTransitionsAreSetOnItsCanvas();
    /** "should throw a warning if we detect cloud based storage that's not
     *  local .. that's burned me before." */
    void filesInCloudStorageNotHereAreAWarning();
    void fixturesWithoutTheirDefinitionsAreAnError();
    void effectLooksThatCannotRunAreInTheFooter();
    void loadProblemsAreInTheFooter();
    /** Consolidation 1-2: the scene's timing in one place, one time
     *  control, one rule for which fades a look has. */
    void aScenesTimingIsAllOnItsCanvas();
    void theCanvasAndTheLookEditorAgreeOnFades();

    /** "Didn't change colour when colour was changed in program from white
     *  to blue." A palette isn't a Function, so editing one through the real
     *  look editor UI must explicitly tell the scenes that use it -- nothing
     *  does that for free. Drives the actual LookEditor color-change slot
     *  (not a manually-injected signal) and checks the owning scene's
     *  Doc::functionChanged actually fires. */
    void editingALookColourNotifiesItsScene();
    void theColourPickerStaysOpenWhileItsLookIsEdited();
    void theRigViewDrawsWhatTheScenesFadersSet();
    void theRigViewFollowsALookSwappedOnTheScene();
    void editingARunningEffectChangesItsOutput();
    void theToolbarFitsWithItsMenus();
    void theHandlesMoveEdgesAndNeverHideAPanel();
    void unpatchedTargetsAreFlaggedInTheProgrammingView();
    void anEffectIsALookForWhatItDrives();
    void unpatchedAndSilentFixturesAreInTheFooter();
    void theReadinessDialogFixesWhatIsSelected();
    void otherRigProblemsAreInTheFooter();
    void readinessItemsSitUnderHeadingsWithMoreThanOneFix();
    void duplicateDefinitionsAreListedWithWhatIsUsed();
    /** Integration test for the Programming tab's own drag-construction
     *  gestures: drag a palette onto the Looks list, a group and a fixture
     *  onto the Targets list, and a palette onto an existing Effect row to
     *  nest it -- all through real QMimeData built the same way the real
     *  source trees (FunctionsTreeWidget, FixtureGroupSource) build it, and
     *  a real synthetic QDropEvent delivered straight to
     *  SceneGroupLooks::dropEvent(). */
    void buildingALookThroughRealProgrammingTabDragAndDropStaysConsistent();
    /** SceneGroupLooks::dropEvent()'s column-precision guard: a palette
     *  dropped squarely on the Targets column, or a fixture group dropped
     *  squarely on the Looks column, is rejected outright. Also proves a
     *  drop at the SAME coordinates but the correct column succeeds
     *  immediately after -- so a passing rejection assertion can't just mean
     *  the coordinate space was wrong everywhere. */
    void droppingOnTheWrongColumnIsRejected();
    /** Dropping a fixture group that shares a member with an existing
     *  individual (fixed-target) fixture folds that fixed target away once
     *  the group's looks fully cover it -- including the degenerate case of
     *  a freshly-added fixed target with no baked values at all, which
     *  "fully covers" trivially and disappears the moment its group lands,
     *  regardless of whether any look was ever applied to it. */
    void droppingAGroupFoldsAwayAnAlreadyCoveredFixedTarget();
    /** "I just tried a new pan/tilt ... moved it around and the rig isn't
     *  following." A palette made from the palette list is in no look; while
     *  it's being edited it is tried on the look on the canvas (never saved
     *  into it), so moving the pad moves the rig -- and the editor says so. */
    void aNewPanTiltIsTriedOnTheLookSoTheRigFollows();
    /** A fixed target renamed anywhere shows its new name in the look's
     *  Targets list. */
    void aRenamedFixedTargetShowsItsNewName();

    /** "I can't seem to drag a scene to a folder in the programming view":
     *  scenes dropped on a folder move into it (with or without a collection
     *  open), onto the category/empty space move back out, the show is
     *  modified and the paths survive save + load; a drop on a collection row
     *  still adds members, and the payload still carries the function ids the
     *  collection/chaser/show editors read. */
    void scenesDragIntoAFolderAndBackOut();
    /** A folder dragged onto another folder moves inside it with everything
     *  in it; never into itself. */
    void aFolderDragsIntoAnotherFolder();
    /** Palettes and palette folders drag into folders too, including an
     *  empty folder just made from the menu. */
    void palettesAndTheirFoldersDragIntoFolders();
    /** "I can't right click on an empty folder to delete it": Delete Folder
     *  removes an empty folder outright; a folder with things in it hands
     *  them to its parent folder — nothing is deleted. */
    void deletingAFolderKeepsWhatIsInIt();
    /** "it was drag selecting other things vs dragging to folder": press on
     *  a row that isn't selected and move -> it's a drag of that row, not a
     *  rubber-band selection growing over the rows passed. */
    void pressAndMoveOnARowDragsIt();
    /** "No ability to remove from side menu": rows shown under a collection /
     *  chaser / show come out of it (membership only; the functions stay),
     *  one or several, from one or several containers; a chaser that holds a
     *  function twice loses only the step picked. "Delete" on such a row says
     *  it deletes the function everywhere and lists where it is used. */
    void rowsUnderAContainerComeOutOfIt();
    /** "can't remove anything here .. no delete .. AND delete should check
     *  for usage and warn the user with a popup that shows where": deleting a
     *  palette names every scene look and effect-look slot using it, and
     *  takes it out of them; Delete/Backspace on the palette tree deletes. */
    void deletingAPaletteSaysWhereItIsUsed();

private:
    Doc* m_doc;
};

#endif
