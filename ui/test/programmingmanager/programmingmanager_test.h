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
    void aCurtainIsNotAskedForLightOutput();
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
    void thePowerChipSaysWhatItsFigureIsMadeOf();
    /** (idle) The power estimate is made again only when what it reads
     *  changed: idle ticks cost no estimate; a level or an edit is one. */
    void anIdleShowIsNotReEstimated();

    /** "I can't seem to drag a scene to a folder in the programming view":
     *  in the outline, scenes dropped on a folder row move into it, onto
     *  empty space back out to the top; the show is modified and the paths
     *  survive save + load; a scene dropped on a scene is refused (no
     *  includes here). */
    void scenesDragIntoAFolderAndBackOut();
    /** A folder renamed in the outline (F2) takes everything filed in it
     *  and under it along. */
    void aFolderRenamesWithWhatIsInIt();
    /** Palettes and palette folders drag into folders too, including an
     *  empty folder just made from the menu. */
    void palettesAndTheirFoldersDragIntoFolders();
    /** Delete Folder on the palette tree hands what is in it to the parent
     *  folder -- nothing is deleted. */
    void deletingAFolderKeepsWhatIsInIt();
    /** "it was drag selecting other things vs dragging to folder": press on
     *  an outline row that isn't selected and move -> it's a drag of that
     *  row, not a rubber-band selection growing over the rows passed. */
    void pressAndMoveOnARowDragsIt();
    /** W1 (reverses P-D12): collections and chasers are full rows in
     *  Build ▸ Scenes, making their own folders; a click loads one in the
     *  Workbench; deleting a scene says it is the function itself and where
     *  it's used. */
    void collectionsAndChasersAreFullRows();
    /** "can't remove anything here .. no delete .. AND delete should check
     *  for usage and warn the user with a popup that shows where": deleting a
     *  palette names every scene look and effect-look slot using it, and
     *  takes it out of them; Delete/Backspace on the palette tree deletes. */
    void deletingAPaletteSaysWhereItIsUsed();
    /** "be able to make top level things here and put any other type under
     *  em": palette folders hold any type; folders first by default, or
     *  grouped by type with the same folders under each type; renaming,
     *  moving into and deleting a folder act on it for every type. */
    void paletteFoldersHoldAnyTypeInBothViews();
    void libraryAndProgrammingAgreeOnFolders();
    void undoHereReachesTheSharedHistory();
    void deletingHereClearsTheShowTreesUses();
    /** ⇧⌘N in Build: a folder or a scene selected -- a new scene in that
     *  folder (one undo step), selected and renaming; the Effects section
     *  -- a new effect; the Palettes section -- nothing. */
    void newHereMakesASceneInTheFolder();

    /** More ▸ Snapshot live output: with something on the rig, each lit
     *  fixture's live bytes baked into the scene being edited; one step. */
    void snapshotBakesTheLiveOutputIntoTheScene();
    /** Save Positions asks twice: the first click only arms (a nudge or
     *  three seconds take it back); the second saves, once. */
    void savePositionsSavesOnlyOnTheSecondClick();
    /** A Bundle saved from a look's stack, stamped on another look: its
     *  stack becomes the bundle's, in order; one ⌘Z puts the old back. */
    void aBundleSavedFromALookStampsInOneStep();
    /** Delete of a scene in Build, answered Yes: gone, and out of the
     *  collection and the chaser step using it; answered No: kept. */
    void deletingAFunctionTakesItOutOfWhatUsesIt();
    void undoingADeleteHandsItBackToItsVcWidgets();
    /** Delete of a function a Shows track plays: gone from the track; ⌘Z
     *  puts the item back with its start and duration; ⌘⇧Z takes it out. */
    void undoingADeletePutsItBackOnItsShowTrack_data();
    void undoingADeletePutsItBackOnItsShowTrack();
    /** ⌘C ⌘V and Duplicate: a copy beside the original, its values and
     *  looks, the next name in the pattern; one ⌘Z takes it away. */
    void duplicateAndCopyPasteFileACopyBesideIt();
    /** Beat ▸ Tap: four taps 500 ms apart -- 120 BPM, on the Beat button. */
    void fourTapsHalfASecondApartShow120Bpm();

private:
    Doc* m_doc;
};

#endif
