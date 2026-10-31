/*
  Q Light Controller Plus
  showtreeview_test.h

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

#ifndef SHOWTREEVIEW_TEST_H
#define SHOWTREEVIEW_TEST_H

#include <QObject>

class FixtureGroup;
class Doc;

class ShowTreeView_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void theTreeShowsProductionsListsCuesAndLayers();
    /** ⇧⌘N, New (here): nothing -- a production; a production -- a cue
     *  list; a list -- a cue at its end; a cue -- the next cue after it
     *  (D23 numbering; a sub-cue: the next main cue); groups in the
     *  Library (focused) or a layer -- a layer on them; a layer with
     *  nothing picked -- the cue and the Library's filter; a look -- a
     *  look. Each one undo step, selected, renaming in place. */
    void newHereMakesTheNextThing();
    void trackedLayersShowGreyWithWhereTheyCameFrom();
    void groupsDroppedOnACueBecomeLayers();
    void groupsDroppedOnALayerJoinIt();
    void aPaletteOnTheCueIsACueLookAndTheHintSaysWhatItReaches();
    void layerRowsShowOwnAndInheritedValues();
    void beamRowsSayWhatTheyApplyTo();
    void goboLooksMatchByNamePerType();
    void oneOffValuesGoToRecentNotAll();
    void noneAndClear();
    void cueFieldsEditTheCue();
    void undoAndRedoFromTheView();
    void theLibraryFilters();
    void theLibraryShowsGroupsInTheirFolders();
    void importingAChaserSelectsTheNewList();
    void inlinePalettesStayOutOfThePaletteTree();
    void dropsOnTheTreeWidget();
    void anEffectGetsLooksOfItsOwn();
    /** An effect row: a one-off says so in italics, beside Make reusable;
     *  its ⋯ sets how it runs (Runs: Once, At the end: Release). */
    void anEffectRowSaysOneOffAndHowItRuns();
    void theLookEditorEditsARowsPalette();
    void cuesReorderAndRenumber();
    void selectingACuePreviewsItOnTheRig();
    void inDesignTheEditShowsOverItsRunningList();
    void theLatestActionWinsAndOperateIsBlind();
    // P2: tracking in the tab
    void trackingControlsOnTheListAndCuePages();
    void includesAddMoveRemoveAndNoLoops();
    void nextCueReceivesSaysWhatPassesOn();
    void aCarriedInLayerCanBeReleased();
    void updateCueWhereATrackedValueWasSet();
    void recordAsNewCueFromTheHeader();
    void theLibraryShowsWhatTheListUses();
    void theCommandLine();
    // P2 answers (2026-10-17)
    void recordAsksWhenTheChoicesDiffer();
    void severalGroupsOneLayerOrEach();
    void aPickedValueReusesAPalette();
    void pictureOfARealShow();
    void pictureOfTrackingInTheTree();
    void aDimmedColourOnPixelBarsReachesTheUniverse();
    // Drop on the page (2026-10-17)
    void dropsOnTheCuePage();
    void dropsOnTheLayerPage();
    // Documenting gaps closed (2026-10-17)
    void mergeLayersBySelectingThem();
    void severalRowsSelectedDeleteAndDrag();
    void aNewCueListIsTracking();
    void generatedFunctionsAreLockedElsewhere();
    void originalsOpenViewOnlyElsewhere();
    void everyActionIsOneUndoStep();

    // P3 (2026-10-17)
    void groupSetsInTheLibrary();
    void libraryGroupToolsSelectSimilarAndMove();
    void libraryGroupToolsOnARealShow();
    void childLayersByMouse();
    void renameInPlace();
    void lineAimsInTheTab();
    void planViewOfALineAim();
    void copyPasteAndHelp();
    void libraryTreeEditorAndAddingWithoutDragging();
    void aSplitSavedWhileDockedDoesNotHideThePage();
    void namedLayersShowWhatTheyHold();
    void editSeveralLayersAtOnce();

    // P4 (2026-10-17): reusable layers and cues, Detach a list, Convert
    void reusablesInTheTreeAndTheLibrary();
    void convertAndDetachFromTheListPage();

    // R2: palettes in the Library; R3: the Looks section
    void anEffectMadeInBuildIsInTheLibraryAtOnce();
    void renameSelectsTheWords();
    void palettesInTheLibrary();
    void aPaletteDeletedFromTheLibraryComesBackWithUndo();
    void paletteStepsAreOneUndoStepAndSharedWithProgramming();
    void looksSectionInTheTree();
    void hitsLiveUnderTheirPlace();
    void makeVcPageAndAdoptAsLook();
    void makeVcPageUpdatesItsFrame();
    void looksAreOneUndoStepAndTyped();

    // P5: multiple productions
    void theProductionSwitcherAndPage();

    // P6: sub-cues in the tree
    void subCuesInTheTree();

    // Tracking in the tree: a badge, details on hover (2026-10-21)
    void trackingIsABadgeWithDetailsOnHover();
    void theTreeColumnsResizeAndAreRemembered();

    // Skipped cues (2026-10-21)
    void skipCuesInTheTree();
    void listModeIsAFlag();
    void releaseAFunctionLayer();
    void cueModeFromTheMenus();
    void textColoursInTheTreePagesAndWorkbench();
    /** Q38: Move in black per list (page, right-click, command) and per cue
     *  (page, right-click, nomib), the badges and the MIB button's tooltip. */
    void moveInBlackPerListAndCueInTheTree();
    /** A picture of a real show with cues skipped: QLC_SKIPCUE_SHOW (a
     *  .qlcc, read only) and QLC_SKIPCUE_SHOT (a .png). */
    void pictureOfSkippedCues();

    // W0: seam fixes
    void openAnOpaqueLayer();
    void openLandsInTheWorkbench();
    void theImportSheetReadsAndRepoints();
    void theImportSheetCanUndoOrLeaveVcAlone();
    void sendToShowTreeFromBuild();
    // W5: the bridge
    void anOldShowOpensWithABanner();
    void theConverterImportsCueListsInPlace();
    void restoreOriginalPointsVcBack();
    void captureFromTheOpaqueRow();
    void upgradeThisShow();
    void upgradeListsWhatIsUpgradedAlready();
    // UX review (2026-10-23)
    /** Q59: the command line's syntax folded away, shown on ? / F1 / the
     *  button, remembered. */
    void commandSyntaxFoldsAwayAndIsRemembered();
    /** Q61: Level… / Colour… open docked in the window, not in a separate
     *  window that can fall behind it: Return / Apply applies, Esc / Cancel
     *  just closes, one at a time. */
    void pickersDockInTheWindow();
    /** Q60: a typed time is kept when focus leaves its field -- a click on
     *  another cue, Tab, the page changing; Esc puts it back. */
    void typedTimesAreKeptWhenFocusLeaves();
    void aTimeNotYetLeftIsWhatGoPlays();
    /** A time typed with either decimal mark is the same time, in any
     *  language: "1.5" in German is not 15 s (L1). */
    void aTimeTakesEitherDecimalMark();
    /** Q96: + Production, + Sub-cue and Merge under More ▾; + Production
     *  on the bar too while the show has none. */
    void theHeaderFoldsUnderMore();
    /** Q93: a cue's right-click keeps what acts on a selection; + Production
     *  only on a production or empty space. */
    void aCuesRightClickIsShort();
    /** Q75: a show with one production has no Current on its page, and GO
     *  plays its list; a second production brings Current back. */
    void oneProductionHasNothingToChooseAndGoPlaysIt();
    /** Renumber this level asks first while OSC / MSC is listening: No
     *  changes nothing, Yes renumbers; not listening, no question. */
    void renumberThisLevelAsksFirstWhileShowControlListens();
    /** One word per idea p2: the menus say Copy to a hit and Adopt as a
     *  hit (no "look"), and each makes a hit. */
    void theHitMenusSayHit();
    /** The MIB button reads "MIB · n" while n heads are pre-positioned. */
    void theMibButtonCountsWhatItPrePositions();
    /** Q104: each flag column names itself on hover; right-click the
     *  header hides one; in a narrow pane they fold before the names. */
    void theFlagColumnsNameThemselvesHideAndFold();
    /** COURSE_PLAN §8 #18: + Palette → Effect, twice: the second starts as
     *  New Effect with the picker open, whatever the editor showed. */
    void aSecondNewEffectPaletteStartsAfresh();

    /** 0.3.1 (lyr): cut / copy / paste and drag move layers between cues,
     *  hits and within a cue; Runs on an effect and `release` on a function
     *  layer. */
    void cutAndPasteMovesLayersInOneUndoStep();
    void pasteOnALayerGoesAfterIt();
    void aDragMovesLayersAndAltCopiesThem();
    void aLayerRowIsDraggableAndAClickOnlySelects();
    void layersIntoAHitLoseTheirRelease();
    void effectRunsIsAnUndoStep();
    void releaseTakesAFunctionLayer();

private:
    quint32 dimmer(int level, const QString &name);
    quint32 colour(const QColor &c, const QString &name);
    FixtureGroup *group(const QString &name, int from, int count);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
