# Test coverage

An audit of qlcconsole's user-facing features against its tests, made for
the 0.2.x release candidate (2026-10-22, branch `test/coverage-audit`).

**Where the features came from:** the help pages (`docs/features/*.md`),
CHANGELOG (Unreleased and 0.2.x), What's new, TODO.md's Done items and the
course lessons (`docs/course/level-*`). Levels 2 and 3 of the course are not
written yet.

**What counts as covered:** a feature is *covered* when a test drives its main
behaviour and asserts the result, not just that it was built. It is *partial*
when only part of the behaviour is asserted, or only the engine half of a UI
feature is. It is *none* when no test reaches it.

**Risk:**

- **high:** playback, output, save/load and undo.
- **med:** features a show relies on.
- **low:** UI conveniences.

**Left out on purpose:** four features have their own agents writing tests
for them in parallel:

- Esc backing out of editors
- Show Tree cue drag and renumber, including the cue Move up/down menu
- the File ▸ Import submenu
- fixture icons (consistency, the curtain icon, multi-select definition edit)

**Opt-in tests:** some tests only run when an environment variable is set,
and none of them is counted as coverage here. They include CourseShow's
`COURSE_RIG`, `COURSE_BUILD` and `COURSE_SHOTS`; `OFFICE_*`; and the
"real show" tests in Workbench, ProgrammingOutline, FixtureManager and
EffectDesigner.

The rows group related behaviours. A suite named as `Suite_Test (all)` covers
that row with every test in the suite. Tests added in this audit are marked
*(new)*.

## Totals

| | covered | partial | none | rows |
|---|---|---|---|---|
| Before | 162 | 52 | 42 | 256 |
| After | 202 | 37 | 17 | 256 |
| High risk, before | 94 | 17 | 17 | 128 |
| High risk, after | 119 | 8 | 1 | 128 |

"After" includes tests marked QEXPECT_FAIL. An XFAIL drives the behaviour and
asserts what the help promises, and it fails today because of a real bug
(see [Bugs found](#bugs-found)). Those rows are *covered (bug)* when the
rest of the behaviour passes. They are *partial (bug)* when the broken part
is the behaviour itself.

| Area | rows | covered before → after | partial before → after | none before → after |
|---|---|---|---|---|
| Patch/Fixtures | 43 | 28 → 31 | 8 → 7 | 7 → 5 |
| Build/Workbench | 49 | 31 → 39 | 10 → 7 | 8 → 3 |
| Show Tree | 37 | 30 → 34 | 3 → 2 | 4 → 1 |
| Run/VC | 24 | 8 → 17 | 9 → 5 | 7 → 2 |
| Studio/Monitor | 35 | 22 → 25 | 6 → 6 | 7 → 4 |
| Output/engine | 31 | 19 → 25 | 6 → 4 | 6 → 2 |
| Import/Export | 7 | 5 → 5 | 2 → 2 | 0 → 0 |
| Show control | 7 | 6 → 6 | 1 → 1 | 0 → 0 |
| Save/Load | 14 | 8 → 12 | 3 → 2 | 3 → 0 |
| Course | 9 | 5 → 8 | 4 → 1 | 0 → 0 |

## Coverage by area

### Patch/Fixtures

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Fixture tree: Groups / Inventory / Power / Universes sections, empty universes pre-built | FixtureTreeWidget_Test::inventoryRootIsAlwaysPresentInTheManager, ::treeCounts | partial | partial | low |
| Inventory filed by manufacturer and model, with counts | FixtureTreeWidget_Test::inventoryIsOrganisedByManufacturerAndModel; FixtureManager_Test::deletingInventoryFixturesUpdatesTheFolderCounts | covered | covered | low |
| Duplicate names root, Number the Duplicates, names kept unique | FixtureTreeWidget_Test::duplicateNamesAreListedAndARenameToATakenNameIsRefused; Doc_Test::fixtureNamesAreUnique; AddFixture_Test::aTakenNameIsRefusedWhenEditingAndNumberedWhenAdding; FixtureCreator_Test::aSingleAddWithATakenNameIsNumbered | covered | covered | med |
| Address clashes root; "Patch the Clashing Fixtures…" | FixtureTreeWidget_Test::addressClashesGetTheirOwnRootAtTheTop; Inventory_Test::patchingAClashClearsIt; PatchInventoryDialog_Test::aClashSaysWhereItWantedToBe (menu path not driven) | partial | partial | med |
| Right pane: info / grid editor / power / universe usage | FixtureManager_Test::selectingAGroupOpensItsLayoutEditor, ::aNetworkFixturesInfoStatesItsConnection; UniverseUsageWidget_Test::titleFollowsARename | partial | partial | low |
| Rename universe / group / fixture in place (Return, F2); empty name refused | FixtureTreeWidget_Test::enterOnAUniverseRowRenamesIt, ::emptyUniverseEditIsRejected, ::enterOnAGroupRowRenamesItInPlace, ::f2OnAGroupAndAnEmptyNameIsRejected (fixture success path not driven) | covered | covered | low |
| Group editor Name follows a tree rename; double-click opens editor | none | none | none | low |
| Add to inventory (toolbar, root menu, quantity up to 999, "Add more X") | FixtureManager_Test::choosingAddToInventoryFromTheMenuStagesAFixture, ::addFixtureWithInventorySelectedStagesIt, ::theInventoryActionsFollowTheAddAction; AddFixture_Test::inventoryModeHidesAddressAndIgnoresOverlap, ::inventoryQuantityIsNotLimitedByAUniverse, ::addingMoreStartsOnTheSameModelAndMode | covered | covered | med |
| Unpatched (staged) fixture sends no DMX and claims no address | Inventory_Test::unpatchedFixtureNeverReachesTheUniverse, ::unpatchedFixtureClaimsNoAddress; StagedOutput_Test (9) | covered | covered | high |
| Patch dialog: proposal, live conflicts, Apply all-or-nothing, problems reported | PatchInventoryDialog_Test::opensOnAProposalForEveryStagedFixture, ::editingIntoACollisionFlagsItAndBlocksApply, ::applyPatchesTheFixtures, ::problemsDuringApplyAreReported; Inventory_Test::patchFixturesIsAllOrNothing | covered | covered | high |
| Patch dialog: mode per row, universes by name, New universe N | PatchInventoryDialog_Test::aModeIsChosenFromTheDefinitionsModes, ::modesTheFixtureCannotMoveToAreOfferedButDisabled, ::universesArePickedByNameInNameOrder, ::newUniversesAndNotesAreSpelledOut | covered | covered | high |
| Place by nearest node; first-fit packing; network fixture's own universe | Inventory_Test::eachNodeTakesTheFixturesNearIt, ::plannerPacksDmxFirstFitAroundPatchedFixtures, ::plannerGivesEachIpItsOwnUniverse; PatchInventoryDialog_Test::nearestNodeProposalsShowWhereTheyGoAndApply | covered | covered | high |
| Addressing order: by name / movers first | PatchInventoryDialog_Test::theSameGroupLandsTheSameWayInNameOrder, ::moversFirstThenByKind | covered | covered | med |
| Addressing order "as they hang"; order remembered | none | none | none | med |
| Into free space, no-room flag, Carry on, Fit by changing modes; keep spacing / packed / one universe each | Inventory_Test::fitKeepsWhatIsThereAndFillsTheGaps, ::whatHasNoRoomIsFlaggedAndCanCarryOn, ::fitByChangingModesShrinksTheBiggestFirst, ::placeKeepsSpacingAsOneBlock | covered | covered | high |
| Patch the others like [structure] | Inventory_Test::likeStructuresArePatchedAlike; PatchInventoryDialog_Test::likeStructuresAreMatched (Studio menu entry not driven) | partial | partial | med |
| Patch Apply is one ⌘Z; moving a patched fixture is Design-only | Monitor_Test::aPatchCanBeUndone, ::aModeChangedByThePatchIsUndone; Inventory_Test::movingAPatchedFixtureIsDesignModeOnly | covered | covered | high |
| Drop fixtures or a group on a universe opens the Patch dialog there | FixtureTreeWidget_Test::aDropOnAUniverseNamesItAndItsFixtures (mime only) | partial | partial | med |
| Return to inventory (keeps groups/looks, Design only) | FixtureManager_Test::returningToInventoryReleasesTheAddress; Inventory_Test::unpatchReturnsAFixtureToTheInventory, ::unpatchRefusesInOperateModeAndForStagedFixtures | covered | covered | high |
| Connection DMX / Art-Net / WLED per fixture; batch IPs; WLED gets the WLED output | AddFixture_Test::connectionAndIpAreReported; FixtureCreator_Test::aBatchOfNetworkFixturesGetsOneIpEach; Inventory_Test::aWledFixtureGetsTheWledOutputAimedAtIt | covered | covered | high |
| Patch list (rows, one-based, clash, mount, IP), CSV / PDF / Copy, labels | PatchList_Test (all); PatchListDialog_Test (all); PatchLabels_Test (all) | covered | covered | low |
| Grid editor: head-type tags, drag heads, arrow picker, marquee/Ctrl select, Move fixture | FixtureManager_Test::populatingAndMovingAFixtureGroupBlockThroughRealUi (blocks only) | partial | partial | low |
| Grid editor: add group as block, drag block with collision guard | FixtureManager_Test::populatingAndMovingAFixtureGroupBlockThroughRealUi, ::draggingASubGroupBlockMovesItThroughTheRealDropPath | covered | covered | low |
| Grid editor Ctrl+Z (50-deep group-layout undo) | none | none | none | med |
| Folders: New Group… on a folder, drag groups between folders, folder rename updates sub-paths | none | none | none | low |
| Move to Folder ▸, Select All Similar, ⇧⌘N new group here | FixtureManager_Test::moveToFolderMenuFilesGroups, ::selectAllSimilarSelectsTheRowsAlike, ::newHereMakesAGroupInTheFolder, ::createGroupFromSelectionStartsInTheCurrentFolder | covered | covered | low |
| Create Group(s) from Selection (one / per head type / per fixture, preview, layout) | HeadTypeGroups_Test (all 13) | covered | covered | low |
| LED Layout editor (rows, swap, grid, flip, preview order, save to show / default) | LedLayout_Test (all 17) | covered | covered | med |
| Split by Head Type; Lay Out from the Stage | Monitor_Test::theSplitWizardMapsRowsAndMakesAGroupPerKind, ::aGroupIsLaidOutAgainFromTheStage | covered | covered | low |
| Rebuild Composite from Members | none | none | none | med |
| Change Definition (drop-in) | HeadTypeGroups_Test::aColourRingIsAHeadTypeOfItsOwn | covered | covered | high |
| Output Rules dialog: tri-state, menus in Fixture Manager / Build / Show Tree Library | FixtureManager_Test::outputRulesForFixturesAndGroups; ShowTreeView_Test::libraryGroupToolsSelectSimilarAndMove | covered | covered | low |
| Output Rules change is one rig undo step | Monitor_Test::outputRulesAreOneRigUndoStep (new; no redo exists) | none | covered | high |
| Stage Look ▸ menu, Fixture Properties look, plot redraw | Monitor_Test::theStageLookMenuSetsEveryFixtureSelected, ::fixturePropertiesSetsTheStageLook; Fixture_Test::stageLookSaveLoad | covered | covered | low |
| Universe row: Patch output / Change / Unpatch / several / Show in Connections | FixtureUniversePatch_Test (all 7) | covered | covered | high |
| Definition source Included / Local; newest local wins; duplicate local defs footer | QLCFixtureDefCache_Test::aLocalDefinitionSaysItReplacesTheIncludedOne, ::ofTwoLocalCopiesTheNewestIsUsed; ProgrammingManager_Test::duplicateDefinitionsAreListedWithWhatIsUsed | covered | covered | med |
| Fixture editor Repair view, Problems panel, Physical units | FixtureDefCheck_Test (16); FixtureEditorCheck_Test (4); EditPhysical_Test; MeasurementUnits_Test | covered | covered | med |
| ETC ColorSource Spot / Spot jr / CYC definitions; Eos _RGB personalities resolve Exact | EosImporter_Test::colorSourceDefinitionsAndTheirRgbPersonalities (new) | partial | covered | med |
| No "declares no light output" warning for curtain / hazer / fan | FixtureDefCheck_Test::onlyALightIsAskedForLightOutput; ProgrammingManager_Test::aCurtainIsNotAskedForLightOutput | covered | covered | low |
| Readiness footer: unpatched, no output, clashes, duplicates, silent universes, load problems; fixes | ProgrammingManager_Test::unpatchedAndSilentFixturesAreInTheFooter, ::otherRigProblemsAreInTheFooter, ::loadProblemsAreInTheFooter, ::theReadinessDialogFixesWhatIsSelected | covered | covered | high |
| Connections ⌘N / ⌘D add and delete a universe (undo) | ConnectionsTree_Test::ctrlNAddsAndCtrlDDeletesAUniverseAndUndoPutsItBack (new) | none | covered | med |
| Definition updates: scan, diff, update / keep / ignore, backup, undo, links, open show follows | FixtureDefUpdates_Test (18); DefinitionUpdates_Test::listsUsedAndUnusedAndUpdatesWithUndo, ::aChangedLayoutIsAskedFirst, ::theFooterSaysSo | covered | covered | high |
| "Also list customised copies" in the Definition Updates dialog | FixtureDefUpdates_Test::scanFindsOldCopiesUsedOrNot (engine only) | partial | partial | low |

### Build/Workbench

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Section switch drives the navigator; Effects/Palettes rows load the Workbench | Workbench_Test::theNavigatorDrivesTheWorkbench | covered | covered | low |
| Palette list: add by type, double-click to edit | ProgrammingManager_Test::undoHereReachesTheSharedHistory, ::aNewPanTiltIsTriedOnTheLookSoTheRigFollows | covered | covered | low |
| Groups & fixtures search; right-click Select similar / Create group / Move to folder | ProgrammingManager_Test::fixturesAndGroupsNarrowToASearch; FixtureManager_Test::selectAllSimilarSelectsTheRowsAlike (Build-side menu only in an env-gated real-show test) | partial | partial | low |
| Toolbar placement, detached tab, narrow-window » | MainBar_Test::everyTabsToolsAreInTheTopBar, ::aDetachedTabKeepsItsTools, ::aNarrowWindowFoldsTheTabsTools; ProgrammingManager_Test::theToolbarFitsWithItsMenus | covered | covered | low |
| Highlight drives the selection to full over a cue | ProgrammerController_Test::highlightDrivesTheSelectionToFull (new, XFAIL: bug B1); StagedOutput_Test::highlightSkipsStagedFixtures | partial | partial (bug) | med |
| Flash the selection | StagedOutput_Test::flashSkipsStagedFixtures (staged rule only) | partial | partial | med |
| Park holds a fixture; saved with the show; Unpark all | ProgrammerController_Test::aParkIsSavedWithTheShow (new), ::aParkedFixtureHoldsItsOutput (new, XFAIL: bug B1); StagedOutput_Test::parkSkipsStagedFixtures | partial | partial (bug) | high |
| Beat menu: internal beat, BPM, button reads the tempo | ProgrammingManager_Test::theToolbarFitsWithItsMenus | covered | covered | med |
| Tap sets tempo and beat; the Beat button shows the tapped tempo | ProgrammingManager_Test::fourTapsHalfASecondApartShow120Bpm (new; bug B6 fixed); MasterTimer_Test::tapSetsTheTempoAndTheBeat | partial | covered | med |
| More ▸ Snapshot live output into the scene (one undo step) | ProgrammingManager_Test::snapshotBakesTheLiveOutputIntoTheScene (new) | none | covered | high |
| Save Positions: two-click arm/confirm keeps Aim drags and nudges | ProgrammingManager_Test::savePositionsSavesOnlyOnTheSecondClick (new) | none | covered | high |
| Resize handles never hide a pane; double-click resets | ProgrammingManager_Test::theHandlesMoveEdgesAndNeverHideAPanel (reset not driven) | covered | covered | low |
| Workbench: a scene is one layer, a collection's members are layers; header info | Workbench_Test::aSceneIsOneLayer, ::aCollectionsMembersAreLayers, ::sharedMembersSayWhereElse, ::whatIsLoadedPlays | covered | covered | high |
| Show Tree-generated functions locked with "Show in Show Tree" | ShowTreeView_Test::generatedFunctionsAreLockedElsewhere | covered | covered | med |
| Editor pane follows the clicked row; ▣ Open and ↩ back | Workbench_Test::aCollectionsMembersAreLayers, ::openAndBackKeepTheSelection; ProgrammingOutline_Test::aClickedRowFillsThePage | covered | covered | med |
| + Scene / + EFX / + RGB Matrix, drag in, reorder, remove | Workbench_Test::layersAddRemoveReorderOnTheSharedHistory, ::newEffectsAreMadeOnTheSelectedLayer | covered | covered | high |
| The lower layer wins a shared channel; a reorder follows in the DMX | Workbench_Test::theLowerLayerWinsAndFollowsAReorder (new) | partial | covered | high |
| Own copy / Make own copy | WorkbenchChaser_Test::makeOwnCopySplitsASharedMember | covered | covered | high |
| Chaser walker, step header, step table, step's collection edits the chaser | WorkbenchChaser_Test::walkingAChasersSteps, ::theStepHeaderEditsTheStep, ::editingAStepsCollectionChangesTheChasersOutput, ::chaserFromSelectionAndDropsOnAChaser | covered | covered | high |
| Notes grow while typed | WorkbenchChaser_Test::theNoteOpensUpWhileTyped | covered | covered | low |
| Live preview in Design (loaded item plays, Stop/Play, leaving stops) | Workbench_Test::whatIsLoadedPlays | covered | covered | high |
| Workbench in Operate: loaded function is show output; hiding stops only its own run | Workbench_Test::operateRunsWhatIsLoadedAndStopsOnlyItsOwn (new; one XFAIL: bug B3) | none | partial (bug) | high |
| One shared undo history (Build and Show Tree); new EFX/RGB undo | ProgrammingManager_Test::undoHereReachesTheSharedHistory; Workbench_Test::newEffectsAreMadeOnTheSelectedLayer; ProgrammingOutline_Test::editsAreStepsOnTheSharedHistory | covered | covered | high |
| Build ▸ Effects: + New, design, Save as palette, Update | EffectDesigner_Test::buildEffectsMakesAndDesignsEffects, ::aNewEffectOnTheBenchSavesAsAPalette, ::aNamedEffectIsACopyUntilUpdated | covered | covered | med |
| Build ▸ Effects right-click Duplicate / Rename / Move / Delete (asks, names uses) | EffectDesigner_Test::buildEffectsRightClickActionsChangeTheShow (new; EFX duplicate/delete undo XFAIL: bug B2) | partial | covered (bug) | med |
| Make a layer into a cue / scene / new scene; one-off kept while history plays it | EffectDesigner_Test::makeALayerIntoACue, ::makeALayerIntoAScene, ::makeALayerIntoANewScene, ::makeALayerFromTheWorkbench | covered | covered | high |
| Effect designer: preview only in Design on screen, one-off, preset, Cancel | EffectDesigner_Test::thePreviewPlaysOnlyInDesignOnScreen, ::keepOnALayerMakesAOneOff, ::saveAsPresetListsIt, ::cancelLeavesNoTrace | covered | covered | med |
| Outline rows, badges, Folders/Classic, Functions window → Build ▸ Classic | ProgrammingOutline_Test::everySceneAndWhatIsInItIsARow, ::badgesSayWhatAndHoverSaysWhy; WorkbenchChaser_Test::classicListsWhatTheFunctionManagerDoes; AppViews_Test::functionsOpensBuildScenesClassic | covered | covered | low |
| New function ▸ every type, wizard, startup function, folders rename/delete | WorkbenchChaser_Test::functionsWindowToolsLiveInBuild; ProgrammingManager_Test::aFolderRenamesWithWhatIsInIt | covered | covered | med |
| Duplicate / ⌘C ⌘V of functions | ProgrammingManager_Test::duplicateAndCopyPasteFileACopyBesideIt (new; undo XFAIL: bug B2) | none | covered (bug) | med |
| Delete function(s): question names uses, deleted everywhere | ProgrammingManager_Test::deletingAFunctionTakesItOutOfWhatUsesIt (new; undo XFAIL: bug B2) | partial | covered (bug) | high |
| Outline drag and drop table, ⌫ several, F2 rename, expand/collapse | ProgrammingOutline_Test::rowsMoveBetweenScenesAndOptionCopies, ::sourcesDropOntoAScene, ::filterAndRemoveSeveral, ::expandCollapseAndOptionClick; ProgrammingManager_Test::scenesDragIntoAFolderAndBackOut | covered | covered | med |
| ⌘A in the outline; Move up/down a look; Show in Palettes; Workbench remembers what is expanded | none | none | none | low |
| ⇧⌘N New here (scene in folder, layer, chaser step) | ProgrammingManager_Test::newHereMakesASceneInTheFolder; Workbench_Test::newHereMakesAStepOrALayer | covered | covered | med |
| Delete palette names uses and clears them; one ⌘Z restores | ProgrammingManager_Test::deletingAPaletteSaysWhereItIsUsed, ::deletingHereClearsTheShowTreesUses | covered | covered | high |
| Palette folders hold any type; older per-type folders merge | QLCPalette_Test::olderShowsTypeFoldersBecomeSharedFolders; ProgrammingManager_Test::paletteFoldersHoldAnyTypeInBothViews | covered | covered | high |
| Bundles: Save as bundle, stamp onto a scene in one undo step | ProgrammingManager_Test::aBundleSavedFromALookStampsInOneStep (new) | none | covered | high |
| Unpatched targets flagged | ProgrammingManager_Test::unpatchedTargetsAreFlaggedInTheProgrammingView | covered | covered | med |
| Transition UI on the scene page | ProgrammingManager_Test::aScenesTransitionsAreSetOnItsCanvas, ::aScenesTimingIsAllOnItsCanvas | covered | covered | med |
| Look Editor: colour, aim, gobo/shutter, fades, name follows value, try on | LookEditor_Test (aLineLooksSpreadIsSetOnTheAimPage, aShutterLookOffersClosed, aLooksNameFollowsItsColour…); PaletteNaming_Test (all); Scene_Test::aTriedOnPalettePlaysButIsNotKept | covered | covered | med |
| Look Editor: Dimmer capability bar, Beam and Strobe editors, Amber/UV visibility | none | none | none | low |
| Effect browser: own window, tabs, search, Recent, previews, settings widgets | LookEditor_Test::theEffectPickerIsAWindowOfItsOwn, ::effectsArePickedFromABrowserWithWledsOwn, ::previewsShowWhatEachEffectDoes, ::anEffectsTextIsTypedIn | covered | covered | med |
| Stack checker (⚠ on rows) | LookStack_Test::theCheckerSaysWhatWontWork; LooksDrop_Test::whatWontWorkIsMarkedInTheList | covered | covered | med |
| Speed masters window and footer | SpeedMastersPanel_Test (all) | covered | covered | med |
| Workspace views (Classic / qlcconsole / Everything) | AppViews_Test::eachWorkspaceViewShowsItsTabs, ::theViewIsSavedWithTheShow, ::aShowWithoutAViewOpensInItsDefault, ::upgradingAClassicShowOffersTheConsoleView | covered | covered | med |
| Show Tree cue opened in the docked Workbench (W3) | ShowTreeWorkbench_Test (all 9) | covered | covered | high |
| The clicked layer's scene is what a control surface addresses | Workbench_Test::aCollectionsMembersAreLayers (currentSceneId only) | partial | partial | med |
| Workbench → 2D Monitor follows the scene being edited | Monitor_Test::editingALookUpdatesTheRigView (setActiveScene called directly) | partial | partial | med |
| Designer In context / Solo; Try on ▾ groups remembered | none | none | none | low |

### Show Tree

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Tree of Production → List → Cue → Layer, carried-in ⤳ rows, badges with hover, column widths remembered | ShowTreeView_Test::theTreeShowsProductionsListsCuesAndLayers, ::trackedLayersShowGreyWithWhereTheyCameFrom, ::trackingIsABadgeWithDetailsOnHover, ::theTreeColumnsResizeAndAreRemembered | covered | covered | med |
| "Changed: …" status after an edit (rebake chain) | ShowTree_Test::theRebakeChain, ::aBakeChangesOnlyTheChain | covered | covered | med |
| Cue page fields (number, name, mode, fades, hold, note) | ShowTreeView_Test::cueFieldsEditTheCue | covered | covered | med |
| Import chaser: steps become cues, same-output check, repeats tracked, HTP mixes | ShowTree_Test::importReadsScenesIncludesAndOpaqueFunctions, ::importChecksTheOutput, ::importTracksWhatRepeats, ::importListsHtpConflicts, ::importedRawScenesBlendAsTheChaserDid | covered | covered | high |
| Import in place, Restore original, import sheet (repoint VC, Undo import) | ShowTree_Test::importInPlaceFilesTheOriginals, ::restoreOriginalPutsTheShowBack; ShowTreeView_Test::theImportSheetReadsAndRepoints, ::theImportSheetCanUndoOrLeaveVcAlone | covered | covered | high |
| Converter / bridge, Capture into palettes, Upgrade this show (one undo step), old-show banner | ShowTree_Test::bridgeTellsCueListsFromEffects, ::captureARawSceneIntoPalettes, ::upgradeAShowIsOneUndoStep; ShowTreeView_Test::upgradeThisShow, ::anOldShowOpensWithABanner | covered | covered | high |
| Send to Show Tree from Build | ShowTreeView_Test::sendToShowTreeFromBuild | covered | covered | med |
| Groups dropped on a cue / layer; functions dropped as ▣ | ShowTreeView_Test::groupsDroppedOnACueBecomeLayers, ::groupsDroppedOnALayerJoinIt, ::dropsOnTheCuePage; ShowTree_Test::addingAGroupToATrackedLayer | covered | covered | high |
| ▣ page "Also in:"; Library Pin / Unpin | none | none | none | low |
| Remove a group from a layer | ShowTree_Test::addingAGroupToATrackedLayer; ShowTreeView_Test::namedLayersShowWhatTheyHold | covered | covered | med |
| Remove a single fixture from a layer | ShowTree_Test::removingAFixtureFromALayerIsOneStepAndKeepsTracking (new) | none | covered | med |
| Split / Merge layers; edit several layers; group sets; child layers | ShowTree_Test::splitALayerKeepsTracking, ::mergeLayersKeepsTracking, ::groupSetsAreOneTargetAndKeepTracking, ::childLayersExcludeTheirFixturesFromTheParent; ShowTreeView_Test::editSeveralLayersAtOnce | covered | covered | high |
| Where layers overlap; who wins; None / Clear; cue looks reach own layers | ShowTree_Test::overlappingLayersLowerWins, ::oneRuleUpAndDownTheChain, ::noneStopsAnInheritedValue, ::cueLooksReachOnlyOwnLayers | covered | covered | high |
| ⋯ menu "Set to / Set in this cue", recent palettes first | CourseShow_Test::lessonOneSetInThisCue, ::lessonsOneTwoToOneFourWithTheMouse (menu driven in the course) | partial | partial | low |
| Typed / picked values reuse palettes; one-offs to Recent, dropped when unused | ShowTree_Test::aTypedValueReusesAPalette, ::anEditRebakesAndDropsWhatIsUnused; ShowTreeView_Test::oneOffValuesGoToRecentNotAll | covered | covered | med |
| Colour rings, gobos by name, effects, line aims | ShowTree_Test::ringGroupsAreLayersOfTheirOwn, ::goboPalettesMatchByNamePerFixtureType, ::aCueEffectIsOneAcrossItsLayers, ::aCueAimIsOneFormationByPosition | covered | covered | high |
| Tracking / Non-tracking lists; Track / Block / Cue-only; Release; base cue; includes | ShowTree_Test::trackingAcrossThreeCues_3_2, ::aBlockCue_3_3, ::cueOnlyAndAnInclude_3_4, ::includesAreCycleProof; ShowTreeView_Test::trackingControlsOnTheListAndCuePages | covered | covered | high |
| However you arrive (GO, Back, jump) the cue looks the same | TrackBack_Test::backThenGoKeepsTheColourTheCuesBeforeSet, ::colourTracksTheSameWhicheverWayYouArrive | covered | covered | high |
| Skip cues (tracking as if absent, jump still plays, saved, UI) | ShowTree_Test::skippedCuesTrackAsIfNotThere, ::skippedCuesSaveLoadAndUndo; TrackBack_Test::skippedCuesPlayAsIfDeleted; ShowTreeView_Test::skipCuesInTheTree | covered | covered | high |
| Text colours | ShowTree_Test::textColoursInheritSaveLoadAndUndo; ShowTreeView_Test::textColoursInTheTreePagesAndWorkbench; CueRunner_Test::textColoursOnEveryLine | covered | covered | low |
| Set in this cue vs Update cue X; Record as new cue; Duplicate; copy/paste; numbering | ShowTree_Test::updateCueWhereATrackedValueWasSet, ::recordTheRunningCueOrEverythingLit, ::duplicateACue, ::copyAndPasteCues, ::numbering | covered | covered | high |
| Design preview = GO; preview over a running list; Operate is blind | ShowTree_Test::previewPlaysWhatGoPlays, ::inDesignTheEditWinsOverARunningList, ::noPreviewInLiveMode; ShowTreeView_Test::selectingACuePreviewsItOnTheRig | covered | covered | high |
| App Blind mutes the preview too | MainBar_Test::theShowControlsWorkInBothPlacements; OutputRules_Test::blindFollowsItsSetting (no preview + Blind case) | partial | partial | med |
| Command line: every word, one undo step, fails as a whole, help, Tab completion | ShowTree_Test::commandLine, ::commandLineFailsAsAWhole, ::everyCommandWordHasHelpAndItsExamplesParse, ::commandLineCompletesEveryWord | covered | covered | high |
| Undo / redo; one history for tree and palettes | ShowTree_Test::undoAndRedo, ::oneHistoryForTreeAndPalettes; ShowTreeView_Test::everyActionIsOneUndoStep | covered | covered | high |
| Undo capped at 200 steps | ShowTree_Test::undoKeepsTheLastTwoHundredSteps (new) | none | covered | med |
| Library: filters, folders, palettes, group tools; deleting a palette clears every use | ShowTreeView_Test::theLibraryFilters, ::palettesInTheLibrary, ::libraryGroupToolsSelectSimilarAndMove; ShowTree_Test::deletingAPaletteClearsEveryUse | covered | covered | high |
| Multi-select delete; ⇧⌘N New here; Detach to plain scene | ShowTreeView_Test::severalRowsSelectedDeleteAndDrag, ::newHereMakesTheNextThing; ShowTree_Test::detachIsOneUndoStep | covered | covered | high |
| Right-click Move up / Move down a cue | not in scope (cue drag/renumber is owned by another agent) | none | excluded | med |
| Reusable layers and cues; Convert to tracking; Detach to plain chaser | ShowTree_Test::reusableLayersAreLiveLinked, ::reusablesSaveLoadAndUndo, ::convertToTrackingPlaysTheSame, ::detachAListToAPlainChaser | covered | covered | high |
| Looks (busking), Make VC page, Copy to / Adopt as look | ShowTree_Test::aLookBakesToACollectionOfItsOwn, ::looksNeverTrack, ::aLookPlaysOverARunningListHtpLtp; ShowTreeView_Test::makeVcPageAndAdoptAsLook | covered | covered | med |
| Productions (current plays GO, main list, timecode source, saved) | ShowTreePlay_Test::theCurrentProductionIsWhatGoPlays, ::eachProductionHasItsTimecodeSource, ::productionsSaveAndLoad | covered | covered | high |
| Show Tree sub-cues (tracking, Sub GO, auto-follow, saved) | ShowTreePlay_Test::subGoPlaysThemOnTheRig, ::autoFollowSubCuesGoOnByThemselves, ::subCuesSaveLoadUndoAndDelete | covered | covered | high |
| Baking: deterministic, shared scenes; tree saves/loads; a show without a tree saves as before | ShowTree_Test::bakeIsDeterministic, ::identicalLayersShareOneScene, ::theTreeSavesAndLoadsWithTheShow, ::aShowWithoutATreeSavesAsBefore | covered | covered | high |
| A Show Tree show opens and plays in an older build that skips <ShowTree> | ShowTree_Test::anOlderReaderPlaysTheBakedFunctions (new); ShowTree_Test::theBridgeLinkSavesAndAnOlderReaderSkipsIt | partial | covered | high |
| Move in black per list / per cue, decision matrix, typed, saved, MIB button in three places | ShowTree_Test::moveInBlackDecisionMatrix, ::moveInBlackSaveLoadUndo; MarkPlanner_Test::perListAndPerCueMoveInBlack; MainBar_Test::moveInBlackIsOneSettingInThreePlaces | covered | covered | high |
| Move in black lead time: a cue sooner than the lead time is not moved in black | MarkPlanner_Test::aCueTooSoonIsNotMovedInBlack (new) | none | covered | high |
| Open the function a ▣ layer plays; Lighting Studio-style item icons (non-fixture) | ShowTreeView_Test::openAnOpaqueLayer, ::openLandsInTheWorkbench; ItemIcons_Test::thePieIsTheLevel, ::aTargetShowsItsKind | covered | covered | low |

### Run/VC

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Run tab cue runner: GO / Sub GO / Back / Pause / Stop, what changes, click loads, double-click jumps, Space is GO | CueRunner_Test::goSubGoBackAndWhatChanges, ::clickLoadsDoubleClickJumps, ::spaceIsGoInTheRunTab, ::runnersAndShowControlMirrorOneState | covered | covered | high |
| Cue Runner VC widget: inputs, keys, feedback, save/load, Swap Widget, handles | CueRunner_Test::vcActionsFromInputsAndKeys, ::vcFeedbackFollowsTheCue, ::theVcWidgetSavesLoadsAndCopies, ::swapWidgetOnAPage | covered | covered | high |
| One top bar, » fold, Show Controls placement, Blackout/Blind colours, kiosk | MainBar_Test::everyTabsToolsAreInTheTopBar, ::thePlacementRoundTripsAndSwitchesLive, ::blackoutAndBlindLightUpInEveryTheme, ::kioskModeDropsTheBar | covered | covered | low |
| Stop all / Blackout / Blind / Design-Operate buttons and shortcuts | MainBar_Test::theShowControlsWorkInBothPlacements; MasterTimer_Test::stopAllFunctions | covered | covered | high |
| Blind is cleared and greyed in Operate | MainBar_Test::operateClearsBlindAndGreysIt (new) | none | covered | high |
| Stop all ▸ Fade 1 s / 5 s … and stop | MainBar_Test::fadeOneSecondAndStopFadesTheLightOut (new) | none | covered | high |
| Show-mode Lock ⌘L | MainBar_Test::showModeLockLocksAndUnlocks (new) | none | covered | high |
| Global GO ⌘↩ / Back ⌘⇧↩ (Control menu) | MainBar_Test::goAndBackStepTheMainCueList (new) | partial | covered | high |
| Footer Next: menu (GO, Back, Stop, Load ▸ sub-cues, Open the Cue List) and production prefix | MainBar_Test::theFootersNextMenuLoadsASubCue, ::theFooterNamesTheCurrentProduction (new); SubCuesUi_Test::theFooterSaysWhatSubGoPlays | partial | covered | med |
| Footer Grand Master slider | MainBar_Test::theFooterGrandMasterDimsTheRig (new) | partial | covered | med |
| Footer speed masters and load meter | SpeedMastersPanel_Test::theFooterShowsEachMastersTempo (load meter untested) | partial | partial | low |
| Workspace view menus: Window menu, break-out, F1 help on what's in front | AppViews_Test::theWindowMenuFindsAndPutsBackBrokenOutViews, ::shiftClickBreaksATabOutDoubleClickDoesNot, ::helpOpensOnWhatIsInFront | covered | covered | low |
| Design while cues run asks first (No stays LIVE, Yes stops and goes dark) | CourseShow_Test::lessonFiveDesignWhileRunningAsksFirst (new) | partial | covered | med |
| Crash on quit with the Show Tree open (fix) | AppViews_Test::quittingWithTheShowTreeShowing | covered | covered | high |
| Software updates: check, ETag, rate limit, verify, never during a show, policy | Updates_Test (30) | covered | covered | med |
| Update installer copes with a stale DMG | Updates_Test::macHelperPrepare, ::macHelperInstall (no stale-attach case) | partial | partial | med |
| Install updates automatically; Restart and Install reopens the show | Updates_Test::nothingInstallsDuringShow (signal only) | partial | partial | med |
| VC Cue List: Cue column, Next:, skips, sub-cues nested | VCCueList_Test::cuesSkipsAndNextAreShown; SubCuesUi_Test::theCueListNestsAndHighlightsSubCues | covered | covered | med |
| Shows timeline: per-track flags at playback (solo, solo-safe, hold last, priority, intensity) | same tests as Output/engine ▸ Shows timeline engine (new; XFAIL: bugs B1, B7, B9) | partial | partial (bug) | high |
| Shows timeline: Undo of an edit (keeps track settings and marker links) | ShowManager_Test::undoTakesBackADrop, ::undoPutsAMarkerBack, ::undoKeepsEveryTracksSettings (new suite; bug B4 fixed) | none | covered | high |
| Shows timeline: ⌘T exact timing, scale cue timing, copy/paste, transport | EndHandle_Test (end handle only); ShowRunner_Test::stopRunner | none | none | med |
| Shows timeline: end handle, markers, time division | EndHandle_Test (5); Show_Test::timeDivision, ::markers | partial | partial | low |
| Control Map: learn a controller button to toggle a function | ProgrammerController_Test::aLearnedControlTogglesItsFunction (new; feedback not asserted) | none | covered | high |
| Command-line -o / -p / -k / -w; a bare file name is not opened | untestable without moving parseArgs() out of main.cpp | none | none (untestable) | high |

### Studio/Monitor

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Opening the Studio and Rig Overview (window, shortcut, centre stage, F1) | AppViews_Test::theRigOverviewOpensWithoutTakingYouToTheStudio; Monitor_Test::theRigOverviewShortcutTogglesIt, ::theAngledViewStandsOnCentreStage | covered | covered | low |
| Four planes (Top/Front/Side editable, Angled view-only), rulers, count badges, crosshair | Monitor_Test::angledViewProjectsAndRefusesEdits, ::dropStaysPutTopView, ::fixturesThatBuryTheirStructureBecomeACount, ::theCrosshairsSayWhereThePointerIs | covered | covered | med |
| Angled camera mouse controls; GPU box remembered; Quality combo | Monitor_Test::studioZoomAtKeepsCursorPointFixed, ::theGpuDrawsWhatTheCpuDraws | partial | partial | low |
| Rig Overview follows the Studio's layers; Layers menu in any mode | Monitor_Test::theRigViewHidesWhatTheStudioHides, ::layersShowAndHideFromTheRigOverviewInAnyMode | covered | covered | med |
| Stage structures: truss, bars, towers, pipes, stands, platforms, steps, rounded front, socks, faces, hangers | Monitor_Test (many: aBarShowsInItsParentTrusssOwnEditor, aMoverOnAShelfSitsOnItRatherThanInIt, theTrussEditorSpansTwoStands, hangersAreRiggingCopiedToEveryTower…); MonitorProperties_Test::stepsAreAFlightOfTreads, ::aTrussSockIsSavedWithItsColourAndStretch | covered | covered | med |
| Platform stacking and no-overlap collision on drag | none | none | none | med |
| Add Steps defaults; Shape Platform/Steps combo | MonitorProperties_Test::stepsAreAFlightOfTreads (model only) | partial | partial | med |
| Scenery kinds, editor preview/Cancel, curtains open/fly, block light | Monitor_Test::everySceneryKindIsAddedWhereItUsuallyGoes, ::theCycloramaEditorPreviewsAndCancelReverts; Scenery_Test (all); BeamOcclusion_Test::aDrapeBlocksClosedNotOpenOrFlown | covered | covered | med |
| Curtains driven by a curtain fixture | Scenery_Test::aDrivenCurtainOpensAsItsChannelMoves; Monitor_Test::aDrapeDrivenByACurtainFixtureOpensAsItsChannelMoves; BeamOcclusion_Test::aDrivenCurtainBlocksAsItsFixtureHasIt | covered | covered | high |
| Mounting: orientation, face, placement, inspector (DMX address, mixed), multi-drag, rotation handles, decks, Shift-drop attach | Monitor_Test::theInspectorMovesAFixturesDmxAddress, ::theInspectorShowsMixedAndSetsOnlyWhatIsSet, ::cmdClickBuildsASelectionThatDragsTogether, ::fixturesAreStoodOnTheDeckUnderThem, ::theAttachBorderShowsOnlyWithShift | covered | covered | high |
| Every rig dialog: live, Cancel reverts, OK one undo | Monitor_Test::aRigDialogPreviewsCancelsAndUndoes, ::fixturePropertiesCancelAndUndoToo, ::anInspectorEditIsOneUndoStep | covered | covered | high |
| Stage looks (set, shapes, practicals, Auto as closest look) | Monitor_Test::everyStageLookHasItsShape, ::aLanternIsAPracticalThatGlowsAllRound, ::autoFixturesCanBeDrawnAsTheirClosestLook; MonitorProperties_Test::autoStageLooksIsSavedWithTheShow | covered | covered | med |
| Layer locks (old locks fold into a Locked layer) | Monitor_Test::aLockIsItsLayersAndDrawsRed; MonitorProperties_Test::itemLocksFoldIntoALockedLayer | covered | covered | high |
| Folder (group) position lock, inherited by sub-folders | Monitor_Test::aLockedFolderHoldsItsMembersAndItsSubFolders (new) | none | covered | med |
| Global "Edit Plot" layout lock refuses drags | Monitor_Test::aLockedPlotRefusesADrag (new), ::aDropOnALockedPlotIsRefused | partial | covered | med |
| Select All / Similar, plot ↔ Layers sync, rotate selection, plot undo | Monitor_Test::selectAllTakesEverythingSelectable, ::selectAllSimilarFindsLikeGroupsAndFixtures, ::rotatingASelectionTurnsItAboutItsMiddle, ::oneUndoPutsBackAWholeMixedDrag | covered | covered | high |
| ⌘C / ⌘V structures with their fixtures | Monitor_Test::copyAndPasteDuplicatesATrussWithItsFixtures (new; fixtures XFAIL: bug B5) | none | partial (bug) | med |
| Units are the user's, converted exactly | MeasurementUnits_Test (all); MonitorProperties_Test::gridUnitsConvertExactly; AppViews_Test::settingsChoosesTheUnits | covered | covered | high |
| Beams, pools, gobos, prisms, footprint, reach overlay, washes, brightness, haze, rings, lasers | Monitor_Test (aBeamStopsAtWhatItLandsOn, aGoboPoolIsDrawnThroughItsMask, aSelectedHeadShowsEverywhereItCanHit, aFixturesLightFollowsWhatItsDefinitionPutsOut, ledsGlowInTheHaze, aColourRingIsFoundAndDrawn, lasersThrowStarsAndPencilBeams…) | covered | covered | med |
| Calibration offsets in the DMX, out of the views | AimSolver_Test::calibrationIsInTheDmxAndOutOfThePicture; Monitor_Test::aCalibratedHeadsPoolLandsOnItsMark | covered | covered | high |
| Fixture Properties Test orientation → Aim at target (was 180° out) | Monitor_Test::testOrientationAimsWhereTheSolverSays (new) | none | covered | med |
| Focus mode (session, nudge, save/revert/zero/undo, views draw intent, keyboard/controller, map) | FocusSession_Test (all); FocusMode_Test (all); Monitor_Test::focusModeRunsFromTheKeyboard, ::focusDrawsTheIntentNotTheNudge | covered | covered | high |
| Controller kept awake, paused-with-reason, controller test window | ControllerKeepAlive_Test (all); FocusMode_Test::theControllerTestListsEachInput | covered | covered | low |
| HID hotplug / wake rescan / line numbers (plugins/hid has no test dir) | none; GamepadLayout_Test covers the Xbox mappings only | none | none | med |
| Xbox mappings per model and transport | GamepadLayout_Test (all) | covered | covered | med |
| Lanterns, light source / glass, WLED LED order | Monitor_Test::aLanternHangsOffATowerAndGlowsAllRound, ::aWledLanternShowsItsEffectLedByLed; QLCPhysical_Test::aLanternBodyRoundTrips | covered | covered | med |
| Network nodes, wireless receivers, cable runs, Network Report | Monitor_Test::addNetworkNodePlacesItWhereTheMenuWasOpened, ::theNetworkOverlayDrawsCableRunsInAddressOrder; NetworkTopology_Test; NetworkReport_Test; NetworkReportDialog_Test | covered | covered | low |
| Layers panel bulk: Move to Layer, New Folder / Layer from Selection | none | none | none | med |
| Studio groups: editor, Seed / Adopt, Create Studio Group from Selection | MonitorProperties_Test::studioFrameDerivation, ::studioFrameXmlRoundTrip (model only) | partial | partial | med |
| DMX lens (universe colours, ⚠ clash) and Power lens | Monitor_Test::aStagedFixtureOnThePlotNeverClaimsAUniverse | partial | partial | med |
| Stage targets: kinds, aim point, Target / Person Here, bound to structure | MonitorProperties_Test::targetKindsAndAimPoint, ::targetFollowsWhatItIsBoundTo; Monitor_Test::addTargetAccepted, ::addingAPersonStandsOneWhereYouClicked | covered | covered | high |
| Delete target asks, naming scenes and palettes | none | none | none | med |
| Line targets (spread, group, cross, edited in the studio) | LineTarget_Test (20); Monitor_Test::aLineTargetIsEditedInTheStudio, ::headsAimedAtALineGoToTheirOwnPoints | covered | covered | high |
| Steps as aim surfaces (target on a tread) | MonitorProperties_Test::stepsAreAFlightOfTreads (treads only) | partial | partial | med |
| LED Layout drawn in the plot and Rig Overview | LedLayout_Test::thePlotDrawsHeadsWhereTheLayoutSays; Monitor_Test::theRigViewDrawsHeadsWhereTheLedMapSays | covered | covered | med |

### Output/engine

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Output rules: Ignore GM, Ignore Blackout (and Blind), Switch, power limits, bits follow moves | OutputRules_Test (grandMasterLeavesExemptAlone, blackoutKeepsExemptLive, blindFollowsItsSetting, switchSnapsAtHalf, switchNeverFades, powerLimitsLeaveARelayAlone, movedFixtureTakesItsBits…) | covered | covered | high |
| Output rule On when not in use (armed at Operate or show open; saved) | OutputRules_Test::onAtOpenFollowsItsSetting, ::onAtOpenYieldsToTheShow, ::onAtOpenSettingIsSavedWithTheShow | covered | covered | high |
| Look-ahead (MIB) scales a dimmerless fixture's colour as playback does | CueOutput_Test::aDimmerlessFixturesColourIsScaledByTheLooksDimmer (new) | none | covered | high |
| How a look stacks (colour, dimmer, effects, dimmerless fixtures) | LookStack_Test (18); EffectInstance_Test::aSharedDimmerTakesItsBrightestHead, ::effectsOnOneChannelLayerAsADeskDoes | covered | covered | high |
| A group target follows its membership live | LookStack_Test::aLookOnAGroupFollowsItsMembersLive (new; live follow XFAIL: bug B8); LookStack_Test::theSameOnAFixtureGroup | partial | partial (bug) | high |
| Transitions: per-head delay, wipes, Edge, effects follow | Scene_Test::transitionTimingAndPlaces, ::aLookWipesInAndOutAcrossTheStage; EffectInstance_Test::anEffectArrivesByItsScenesTransition | covered | covered | high |
| Transition ignored when the scene's tempo is Beats | Scene_Test::aTransitionIsIgnoredWhenTheTempoIsBeats (new) | none | covered | med |
| Shutter looks (Closed closes) and Auto-open shutters | ShutterAutoOpen_Test (13); LookEditor_Test::aShutterLookOffersClosed | covered | covered | high |
| Effects: big-rig cache, groups laid out on stage, WLED ports, matrix effects, Matrix Image, text, step test | EffectInstance_Test (aWholeRigOfPixelsTicksQuickly, groupsAreLaidAcrossTheStageWhereTheyStand, everyWledPortRuns, matrixEffectsRunOnBoxesAndTheirModes, matrixImagePlaysAGifKeptInTheShow, theTextEffectSpellsOnAGrid…) | covered | covered | high |
| Individual colour / dimmer / position / pixel / beam effects' output | EffectInstance_Test::aRainbowWithALookColourSweepsThatColour; SpreadLayer_Test::everyScriptThatTakesItUsesIt (spread only) | partial | partial | med |
| Audio Spectrum / VU and MIDI Keys / Flash / Comet / Puddles effects | none | none | none | med |
| Movement effects run round the look's aim; follow spot fades near limits / when blocked; zoom lowers the aim | EffectInstance_Test::aMovementEffectMovesRoundWhereTheLookPoints, ::followSpotFadesAHeadThatCannotReach, ::followSpotZoomsToHoldSizeOnSubject; Monitor_Test::aWiderFollowSpotAimsLowerOnThePerson | covered | covered | high |
| Follow spot moves the shared aim point (heads converge) | EffectInstance_Test::followSpotMovesEveryHeadToTheSamePoint (new) | partial | covered | high |
| One beat clock; tap; external beat pulls gently | MasterTimer_Test::tapSetsTheTempoAndTheBeat, ::alignBeatPutsTheBeatNow, ::theBeatClockRunsAtTheTempo; EffectInstance_Test::effectsStartedApartAreOnTheSameBeat | covered | covered | high |
| Spread; speed masters (engine) | SpreadLayer_Test (11); SpeedMasters_Test (all) | covered | covered | high |
| Aim solver; line of sight; Dim lights that can't see | AimSolver_Test (9); BeamOcclusion_Test (14) | covered | covered | high |
| Only transmit on the rig network | InputOutputMap_Test::rigNetworkHeldOffTheRigNetwork, ::rigNetworkSaveLoadRoundTrip; ArtNet_Test::rigNetworkGateHoldsEverySend; Wled_Test::rigNetworkGateIsDecidedOffTheSendTick | covered | covered | high |
| Patches follow the network; offline patches kept and reconnect | InputOutputMap_Test::followsNetworkToAnotherMachine, ::offlinePatchIsInertAndListed, ::offlinePatchReconnectsWhenNetworkAppears | covered | covered | high |
| Connections tree: devices, By universe, port picker, multi-patch, Undo Patch Change | ConnectionsTree_Test (59); PatchUndo_Test (17) | covered | covered | high |
| Output trouble detection (Art-Net / E1.31), node IP setup | ArtNet_Test::sendHealth*, ::nodeWatch*, ::ipProgIsUnicastToAReachableNode; E131_Test::sustainedFailureIsReportedOnceAndRecovers | covered | covered | high |
| WLED output, config check, backup lanterns | Wled_Test (all); WledConfigCheck_Test (4); WledDevices_Test (6) | covered | covered | high |
| Power estimate, sources/circuits, max draw cap | PowerDistribution_Test; FixturePower_Test (6); Monitor_Test::measuringPowerDrivesEachStateAndKeepsTheReadings | covered | covered | high |
| Power circuit Auto-assign and UPS runtime | none (logic lives in PowerDistributionWidget) | none | none | med |
| Stop all ▸ fade and stop (engine) | MasterTimer_Test::fadeAndStopAllFadesTheOutputThenStops (new); MainBar_Test::fadeOneSecondAndStopFadesTheLightOut (new) | none | covered | high |
| A last look held after a Show stops (Operate) | LastLookEffect_Test::holdSurvivesTheUniverseFrame (new, XFAIL: bug B1) | partial | partial (bug) | high |
| A chaser deleted while the MasterTimer thread runs it | Chaser_Test::deletingAChaserWhileTheTimerRunsIsSafe (new), ::deletingARunningFunctionLetsGoOfItsChannels | partial | covered | high |
| The show's LED maps read on the DMX thread while they are set | MonitorProperties_Test::anLedMapReadWhileItIsSetIsWhole (new) | none | covered | med |
| Shows timeline engine: per-track intensity, solo / solo-safe, hold last, priority, show end | ShowRunner_Test::aSoloTrackPlaysAndTheOthersOnlyIfSoloSafe, ::holdLastKeepsTheLookThroughAGap, ::anOverrideTrackWinsAndABackgroundTrackYields, ::aTrackAtHalfIntensityPlaysAtHalf, ::theShowStopsAtItsConfiguredEnd (new; XFAIL: bugs B1, B7, B9); Track_Test::newFlagsRoundTrip | partial | partial (bug) | high |
| Show timecode follow, override, freeze | ShowRunner_Test::timecodeFollow, ::timecodeDriveAndFreeze; TimecodeSource_Test (all) | covered | covered | high |
| GO on the beat / bar | SpeedMasters_Test::aGoLandsOnTheNextBeat, ::aGoOnTheBarWaitsForTheBar | covered | covered | high |
| Chaser sub-cues | SubCues_Test (12) | covered | covered | high |

### Import/Export

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Collect for Transfer | ShowEmbedding_Test::aCollectedFolderOpensCompleteAsQxw (App slot not driven) | partial | partial | med |
| Eos import: wizard, mapper choices, cue lists into the Show Tree, magic sheets | EosImportWizard_Test::readsTheExport, ::choicesGoToTheMapper; EosImporter_Test::cueLists, ::realExportIntoShowTree, ::magicSheet | covered | covered | med |
| Eos special circuits → output rules and Eos/ groups; $$Options X | EosImporter_Test::specialCircuits, ::circuitClassifier; EosImportWizard_Test::specialCircuitsPage; EosAsciiParser_Test::patchOptionsAndSubModes | covered | covered | high |
| Eos subs become VC Playback sliders | EosImporter_Test::subsForTheConsole; EosImportWizard_Test::subFaders | covered | covered | med |
| QXW import (palettes follow scenes) | QxwImporter_Test (all) | covered | covered | med |
| Patch list CSV / PDF, labels | PatchList_Test; PatchListDialog_Test; PatchLabels_Test | covered | covered | low |
| ⌘D dump DMX values into a function | DmxDumpFactoryProperties_Test (properties only) | partial | partial | med |

### Show control

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| Cue numbers, main list, Renumber Cues (chaser editor), saved | ShowControl_Test::aStepsCueIsItsNumberOrItsPosition, ::cueNumbersAndTheMainListAreSaved; ShowControlUi_Test::theChaserEditorSetsCuesAndTheList | covered | covered | high |
| Skip a step (playback); chaser editor skip UI | ShowControl_Test::skippedStepsArePassedOver, ::autoFollowPassesOverSkippedSteps (editor tick not driven) | partial | partial | med |
| GO / LOAD / STOP / BACK / PAUSE / RESUME / RESET / ALL OFF | ShowControl_Test::goPlaysTheCueAndThenTheNext, ::loadPutsACueOnStandby, ::pauseResumeStopAndAllOff | covered | covered | high |
| OSC decode / fire / listen settings; MSC decode, device, chosen input | ShowControl_Test::oscIsDecoded, ::oscFiresTheCue, ::mscIsDecoded, ::mscOnlyOnTheChosenInput; ShowControlUi_Test::oscFromTheNetworkFiresTheCue, ::editingThePageChangesTheListener | covered | covered | high |
| Sub-cues from QLab (GO / LOAD / STOP 4.2, /subgo, /subback) | ShowControl_Test::aSubCueIsFoundByNumber, ::goASubCueByNumberPlaysItsCueFirst, ::oscSubGoAndSubBackStepTheCuePlaying, ::mscGoAndLoadASubCue | covered | covered | high |
| Checking the hookup: last command, errors, port in use; timecode health | ShowControlUi_Test::aCommandReceivedShowsAsLast, ::aPortInUseShowsAnError, ::theTimecodeSourceAndHealth | covered | covered | med |
| Auto Move in Black: one setting in three places; a new show resets it | MainBar_Test::moveInBlackIsOneSettingInThreePlaces; MarkPlanner_Test::theShowsMoveInBlackSavesLoadsAndResets | covered | covered | high |

### Save/Load

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| .qlcc save / open round trip; .qxw opens; a missing file is an error | Autosave_Test::aQlccShowSavesAndOpensAgain, ::aMissingShowFileIsAnErrorNotAnEmptyShow; QxwImporter_Test | covered | covered | high |
| Autosave writes a recovery copy that loads back; nothing when unmodified | Autosave_Test::aModifiedShowWritesARecoveryCopyThatLoadsBack, ::anUntitledShowsRecoveryCopyIsWhereStartUpLooks (new) | partial | covered | high |
| Recovery offered at the next launch; saving / discarding removes the copy | Autosave_Test::aRecoveryFileIsKeptWhenThereIsNobodyToAsk, ::savingOrDiscardingRemovesTheRecoveryCopy (new); the prompt itself is untestable offscreen (seam needed) | none | partial | high |
| Address clash loads unpatched and saves unchanged; inventory fields round-trip | Inventory_Test::aClashingFixtureLoadsUnpatchedAndSavesUnchanged, ::xmlRoundTripKeepsInventoryFields | covered | covered | high |
| Output rules saved only when set; older readers skip them | OutputRules_Test::rulesRoundTripAsText, ::savedOnlyWhenSetAndSkippedByOlderReaders | covered | covered | high |
| Park saved with the show, dropped by a new show, back on load | ProgrammerController_Test::aParkIsSavedWithTheShow (new) | none | covered | high |
| Missing definition keeps make/model on save; show cache | Fixture_Test::aMissingDefinitionIsKeptNotLostOnSave, ::theShowCacheKeepsDefinitionsOnThisComputer; AppViews_Test::aShowsDefinitionsComeFromTheShowCache | covered | covered | high |
| Saving with missing definitions asks first; footer Fetch Definitions gives them back | AppViews_Test::missingDefinitionsAreAskedAboutAndFetchedBack (new) | none | covered | high |
| Online-only cloud files: noted not waited on; Download Now / Skip | QLCFile_Test::onlineOnlyFilesAreNotedNotWaitedOn; ProgrammingManager_Test::filesInCloudStorageNotHereAreAWarning (Download Now / Skip need a cloud stub) | partial | partial | high |
| .qlcc embeds non-shipped files; another computer: same / different / Use This Computer's / Install / Undo | ShowEmbedding_Test (all); ShowDifferences_Test (all); DefinitionUpdates_Test::showDefinitionsListsEverythingAndUndoes | covered | covered | high |
| Units: show stored exactly, the user's choice seeds from the first show | MeasurementUnits_Test::seedFromFirstShow, ::roundTripStoredMm; MonitorProperties_Test::gridSizeSavedWholeAndExact | covered | covered | high |
| Stale deck Face corrected on load; hung fixtures straightened | MonitorProperties_Test::aChosenFaceSurvivesSaveAndRepair, ::aFixtureOnAHangerHangsStraight | covered | covered | high |
| A course show built with the mouse saves, opens again and plays | CourseShow_Test::aMouseBuiltShowSavesOpensAndPlays (new) | partial | covered | high |
| Track flags persist | Track_Test::newFlagsRoundTrip, ::garbageAttrsAreSafe | covered | covered | high |

### Course

| Feature | Test(s) | Before | After | Risk |
|---|---|---|---|---|
| L0.2 Rig Overview opens, Run tab GO ×3 | CourseShow_Test::lessonZeroTwoTheVirtualStage | covered | covered | low |
| L0.2 room light and views actually switch the render | CourseShow_Test::lessonZeroTwoTheVirtualStage (lists the combos only) | partial | partial | low |
| L1.1 production, list, cues, drops, Set in this cue, typed, plays in the Run tab | CourseShow_Test::lessonOneWithTheMouse, ::lessonOneSetInThisCue, ::lessonOneTyped, ::lessonOnePlaysInTheRunTab | covered | covered | high |
| L1.2 Set to, Level one-off, Make reusable, undo/redo, cue looks, colour picker | CourseShow_Test::lessonsOneTwoToOneFourWithTheMouse, ::lessonTwoColourPicker | covered | covered | high |
| L1.3 aim, Block cue (⊘ badge, dark stage), auto-open spots, extensions | CourseShow_Test::lessonsOneTwoToOneFourWithTheMouse (Block badge and dark stage now asserted), ::lessonThreeExtension | partial | covered | high |
| L1.4 fades, hold, follow in the Run tab | CourseShow_Test::lessonFourPlaysTheFades | covered | covered | high |
| L1.5 Operate, GO / Back / LOAD / jump / Stop; Design while running asks | CourseShow_Test::lessonFiveOperate, ::lessonFiveRunsTheShow, ::lessonFiveDesignWhileRunningAsksFirst (new) | partial | covered | high |
| Every lesson's Save As keeps the student's show | CourseShow_Test::aMouseBuiltShowSavesOpensAndPlays (new) | partial | covered | high |
| The teaching show is safe (no outputs, embedded definitions, groups / palettes / targets) | CourseShow_Test::theTeachingShowIsSafeAndComplete | covered | covered | high |

## Bugs found

Each bug below has a test that asserts what the help says, marked
`QEXPECT_FAIL` with a `BUG:` message. Bugs B4 and B6 were small and safe, so
they are fixed here and their tests pass.

| # | Bug | Where | Test(s) | State |
|---|---|---|---|---|
| B1 | **DMX sources that hold channels lose their intensity.** Park, Highlight and the last-look holder write with `Universe::write()` during the MasterTimer tick. The universe thread's `processFaders()` then runs `zeroIntensityChannels()` and lays the running faders on top, before the frame goes out. So a parked or highlighted fixture does not hold its dimmer, and a running cue overrides it. The same happens to a timeline track's *Hold last look* and to the look a show *parks* at its end: both go dark. LTP channels (pan, tilt) do hold when no fader writes them, which is why Move in black works. | `engine/src/parkeffect.cpp`, `highlighteffect.cpp`, `lastlookeffect.cpp` `writeDMX()`; `Universe::processFaders()` | ProgrammerController_Test::aParkedFixtureHoldsItsOutput, ::highlightDrivesTheSelectionToFull; LastLookEffect_Test::holdSurvivesTheUniverseFrame; ShowRunner_Test::holdLastKeepsTheLookThroughAGap, ::theShowStopsAtItsConfiguredEnd | open (XFAIL). Needs a post-fader write path in the universe: a core mixer change, not one for the night before a release. |
| B2 | **Deleting or duplicating a function in Build is not undoable.** This covers Duplicate, ⌘C ⌘V and the EFX Duplicate/Delete in Build ▸ Effects. They call `createCopy` / `Doc::deleteFunction` without `PaletteOps::functionStep`, so ⌘Z can't take them back. Palettes and function rename/move are undoable. | `ProgrammingManager::duplicateFunctions`, `deleteFunctions`, `effectsContextMenu` | ProgrammingManager_Test::deletingAFunctionTakesItOutOfWhatUsesIt, ::duplicateAndCopyPasteFileACopyBesideIt; EffectDesigner_Test::buildEffectsRightClickActionsChangeTheShow | open (XFAIL). The help only promises undo for palettes and edits. |
| B3 | **Hiding the Workbench in Operate can stop a function it didn't start.** `stopOperate()` stops with `FunctionParent::master()`, which clears every source. So a function that a VC button or chaser was already running when the Workbench loaded it stops when the Workbench hides. | `Workbench::stopOperate` | Workbench_Test::operateRunsWhatIsLoadedAndStopsOnlyItsOwn | open (XFAIL). The fix touches the preview → operate hand-over. |
| B4 | **Timeline Undo reset every track's settings.** The undo snapshot kept only a track's name, mute, colour and items. Undo rebuilds the tracks, so it reset Hold last look, Solo / Solo-safe, priority, intensity, the track lock and item locks. It also dropped a marker's cue-list link. | `ShowManager::captureSnapshot` / `restoreSnapshot` | ShowManager_Test (new suite) | **fixed** in this branch |
| B5 | **A pasted truss arrives without its fixtures.** `TrussClip` carries none, though the shortcuts help says structures copy "with their fixtures". | `Monitor::slotCopySelected` / `pasteClipboard` | Monitor_Test::copyAndPasteDuplicatesATrussWithItsFixtures | open (XFAIL) |
| B6 | **The Beat button kept the old tempo after a Tap.** `slotTapBeat()` set the BPM under a `QSignalBlocker`, and the button only follows the spin box's `valueChanged`. | `ProgrammingManager::slotTapBeat` | ProgrammingManager_Test::fourTapsHalfASecondApartShow120Bpm | **fixed** in this branch |
| B7 | **An Override track doesn't win an intensity channel.** Track priority orders the faders, but intensity is still merged highest-wins (HTP). shows.md says "Override (this track wins)". This may be intended HTP behaviour; if so, the help needs a caveat. | `ShowRunner` `trackFadePriority`; universe HTP | ShowRunner_Test::anOverrideTrackWinsAndABackgroundTrackYields | open (XFAIL): decide whether it's the code or the docs |
| B8 | **A playing scene doesn't follow its group's membership.** Group fixtures are resolved into faders when the scene starts, and nothing re-resolves them on `FixtureGroup::changed`. build.md promises membership is "followed live". A restart picks the change up. | `Scene::write` fader build | LookStack_Test::aLookOnAGroupFollowsItsMembersLive | open (XFAIL) |
| B9 | **A track's intensity set before Start is ignored.** `Show::preRun()` queues each track's Show *attribute*, which stays 1.0 because `setTrackIntensity()` / `Track::setIntensity()` never update it. That 1.0 overwrites `Track::intensity()`, so a track saved at 50% plays at full until its fader moves. | `Show::preRun` | ShowRunner_Test::aTrackAtHalfIntensityPlaysAtHalf | open (XFAIL). The attribute and the track submaster are two sources for one value; which one wins needs a decision. |

## Not tested, and why

- **Command line (`-o`, `-p`, `-k`, `-w`; a bare file name is not opened).**
  `parseArgs()` lives in `main/main.cpp`, which no test links. The seam
  needed: move it into the ui library as
  `QLCArgs::parse(const QStringList &)`.
- **The autosave recovery prompt.** `App::checkAutosaveRecovery()` returns
  before asking on the offscreen and minimal platforms, whatever the
  environment says. What is tested: the copy is written, kept with nobody to
  ask, and removed by Save or Discard. The seam needed: a
  `QLC_RECOVERY_ANSWER=yes|no` override checked before the platform test.
  Removal of the copy at a clean quit (`~App`) would need a second App in
  the process.
- **Cloud storage Download Now / Skip.** These need a stand-in for an
  online-only file provider. Only the "noted, not waited on" startup path is
  tested.
- **HID hotplug and line numbering.** `plugins/hid` has no test directory
  and needs a stubbed device enumerator.
- **The Lighting Studio's rig undo has no redo,** so there is nothing to test
  there.
- **Audio and MIDI effects** need a fake spectrum / note source. **Power
  circuit Auto-assign** lives in `PowerDistributionWidget`; it would be
  testable once its bin-packing moves into the engine.
- **Low-risk UI conveniences** were left for after the release. These are the
  rows marked *none* or *partial* with risk *low* above: the grid editor's
  marquee and arrow picker, Library pins, ⌘A in the outline, Look Editor
  sub-pages, outline contrast, and the Rig Overview camera mouse controls.

## Seams and product changes made in this audit

- No test hooks were added.
- Two small fixes (B4, B6), each covered by its tests.
- One test-only robustness fix: ProgrammerController_Test's cleanup now
  stops whatever a failed test left running, before clearing the Doc.
- The new suite `ui/test/showmanager` is registered in
  `ui/test/CMakeLists.txt`.

## Keeping this up to date

When a feature lands, add its row here with its test.
