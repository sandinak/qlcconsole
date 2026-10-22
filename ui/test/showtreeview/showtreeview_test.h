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
    void theLookEditorEditsARowsPalette();
    void selectingACuePreviewsItOnTheRig();
    void inDesignTheEditShowsOverItsRunningList();
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
    void everyActionIsOneUndoStep();

    // P3 (2026-10-17)
    void groupSetsInTheLibrary();
    void childLayersByMouse();
    void renameInPlace();
    void lineAimsInTheTab();
    void planViewOfALineAim();
    void copyPasteAndHelp();
    void libraryTreeEditorAndAddingWithoutDragging();
    void namedLayersShowWhatTheyHold();
    void editSeveralLayersAtOnce();

    // P4 (2026-10-17): reusable layers and cues, Detach a list, Convert
    void reusablesInTheTreeAndTheLibrary();
    void convertAndDetachFromTheListPage();

    // R2: palettes in the Library; R3: the Looks section
    void palettesInTheLibrary();
    void aPaletteDeletedFromTheLibraryComesBackWithUndo();
    void paletteStepsAreOneUndoStepAndSharedWithProgramming();
    void looksSectionInTheTree();
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
    /** A picture of a real show with cues skipped: QLC_SKIPCUE_SHOW (a
     *  .qlcc, read only) and QLC_SKIPCUE_SHOT (a .png). */
    void pictureOfSkippedCues();

    // W0: seam fixes
    void openAnOpaqueLayer();
    void openUsesTheFunctionsWindowsEditor();
    void theImportSheetReadsAndRepoints();
    void theImportSheetCanUndoOrLeaveVcAlone();
    void sendToShowTreeFromBuild();
    // W5: the bridge
    void anOldShowOpensWithABanner();
    void theConverterImportsCueListsInPlace();
    void restoreOriginalPointsVcBack();
    void captureFromTheOpaqueRow();

private:
    quint32 dimmer(int level, const QString &name);
    quint32 colour(const QColor &c, const QString &name);
    FixtureGroup *group(const QString &name, int from, int count);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
