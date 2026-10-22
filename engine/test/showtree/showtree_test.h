/*
  Q Light Controller Plus
  showtree_test.h

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

#ifndef SHOWTREE_TEST_H
#define SHOWTREE_TEST_H

#include <QObject>
#include <QHash>

#include "showtreecommand.h"

class FixtureGroup;
class Chaser;
class Doc;

class ShowTree_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // The model and the cascade (§2, §3), no Doc behind them
    void globalLayerEffect_3_1();
    void trackingAcrossThreeCues_3_2();
    void aBlockCue_3_3();
    void cueOnlyAndAnInclude_3_4();
    void cueLooksReachOnlyOwnLayers();
    void noneStopsAnInheritedValue();
    void aNonTrackingListPlaysEachCueAlone();
    void anIncludeThatWouldLoopIsLeftOut();
    void numbering();
    void numberedNames();
    void xmlRoundTrip();
    void aLayerWithTwoGroupsTracksAcrossCues();
    void addingAGroupToATrackedLayer();
    void aBlockCueKeepsItsOwnLayerWhenAGroupIsAdded();

    // Baking (§4)
    void bakeMakesAChaserCollectionsAndScenes();
    void identicalLayersShareOneScene();
    void bakeIsDeterministic();
    void anEditRebakesAndDropsWhatIsUnused();
    void undoAndRedo();
    void inlinePalettesComeBackWithUndo();
    void aRunningSceneIsNotDeletedUnderTheShow();
    void theTreeSavesAndLoadsWithTheShow();
    void aShowWithoutATreeSavesAsBefore();
    void lookReachCountsFixturesThatHaveIt();
    /** Branson's mover with a colour ring as two heads: layers on its
     *  Movers and Ring groups set the wheel and the ring apart, track,
     *  preview, and the command line names the ring group and its chases. */
    void ringGroupsAreLayersOfTheirOwn();
    void goboPalettesMatchByNamePerFixtureType();

    // Preview: the selected cue on the rig (Design mode)
    void previewPlaysTheSelectedCue();
    void anEditShowsOnThePreviewAtOnce();
    void aSceneTwoPreviewedCuesShareRunsOn();
    void noPreviewInLiveMode();
    void inDesignTheEditWinsOverARunningList();

    // Import (§7)
    void importReadsScenesIncludesAndOpaqueFunctions();
    void importedListPlaysAsTheChaserDid();
    void importListsHtpConflicts();
    void importARealShow();
    void importAnIncludeBrighterThanTheCuesOwnMixes();
    void dryRunUsesTheShowsOwnDefinitions();
    // W0: the import report's own check; raw scenes blend as in the
    // chaser; the stock sample; something outside the tree in an undo step
    void importChecksTheOutput();
    void importedRawScenesBlendAsTheChaserDid();
    void importTheSampleShow();
    void anUndoStepCarriesWhatItChangedOutside();

    // P2: tracking in the tab, D22/D23, the Library's list usage, the
    // command line
    void theRebakeChain();
    void aBakeChangesOnlyTheChain();
    void includesAreCycleProof();
    void updateCueWhereATrackedValueWasSet();
    void recordAsNewCueFreezesTheStage();
    void duplicateACue();
    void usageInAList();
    void previewShowsTheTrackedState();
    void convertAnImportToTracking();
    void commandLine();
    void commandLineFailsAsAWhole();
    void commandLineCompletes();
    void commandLineCompletesEveryWord_data();
    void commandLineCompletesEveryWord();
    void commandLineTabCyclesAndRuns();

    // P2 answers (2026-10-17)
    void recordTheRunningCueOrEverythingLit();
    void splitALayerKeepsTracking();
    void mergeLayersKeepsTracking();
    void aTypedValueReusesAPalette();
    void aNewPaletteIsNamedAsTheShowsAre();

    // Documenting gaps closed (2026-10-17)
    void aNewListIsTrackingAnImportedOneIsNot();
    void makeReusableIsOneUndoStep();
    void detachIsOneUndoStep();
    void generatedFunctionsAreFoundInTheTree();

    // P3: Scene::excludedFixtures, layout spans
    void sceneExcludedFixturesAreNotWritten();
    void exclusionIsThreadSafeWithLivePreview();

    // P3: group sets, child layers, overlap
    void groupSetsAreOneTargetAndKeepTracking();
    void childLayersExcludeTheirFixturesFromTheParent();
    void overlappingLayersLowerWins();
    void childLayerEditsWhileTheDmxThreadRuns();
    void anImportedOverlapMixesAsTheChaserDid();

    // P3: cue-level effects (D10), line aims (D16-D19)
    void aCueEffectIsOneAcrossItsLayers();
    void cueSpreadReachesOnlyLineAims();
    void aCueAimIsOneFormationByPosition();
    void aSpreadChangeFadesOverTheCueFade();

    // The command line's help; copy / paste
    void everyCommandWordHasHelpAndItsExamplesParse();
    void copyAndPasteCues();

    // P4: reusable layers and cues, Detach a list, Convert to tracking
    void reusableLayersAreLiveLinked();
    void reusableCuesInSeveralLists();
    void reusablesSaveLoadAndUndo();
    void detachAListToAPlainChaser();
    void convertToTrackingPlaysTheSame();
    void convertToTrackingSyntheticLists();
    void reuseDetachAndConvertTyped();

    // R1/R2: palette housekeeping (PaletteOps), Used by, one history
    void paletteOpsFoldersAnyType();
    void paletteOpsUndoBringsADeletedPaletteBack();
    void deletingAPaletteClearsEveryUse();
    void paletteUsageNamesCuesLooksAndScenes();
    void oneHistoryForTreeAndPalettes();
    void paletteCommands();

    // R3: the Looks section
    void aLookBakesToACollectionOfItsOwn();
    void looksNeverTrack();
    void aLookPlaysOverARunningListHtpLtp();
    void lookFoldersSaveLoadAndUndo();
    void adoptASceneAsALook();
    void recordLitTakesARunningLooksLayers();
    void lookCommands();

    // Answers 12-20 (2026-10-18)
    void oneRuleUpAndDownTheChain();
    void aChildDimmerDimsItsParentsColourWithoutADimmer();
    void previewPlaysWhatGoPlays();
    // Skipped cues (2026-10-21)
    void skippedCuesTrackAsIfNotThere();
    void aSkippedCueStillPlaysWhenJumpedToAndIncluded();
    void skippedCuesBakeToSkippedSteps();
    void skippedCuesSaveLoadAndUndo();
    void skipAndUnskipTyped();
    void importAndConvertKeepTheSkip();

private:
    /** What a dry run of the import on a show file found (importARealShow). */
    struct DryRun
    {
        struct Played
        {
            QString list, cue;
            int lit = 0, differ = 0;
        };
        bool loaded = false;
        int lists = 0, cues = 0;
        int differ = 0;             //!< channels, imported vs the chaser, all cues
        int convertedDiffer = 0;    //!< channels, converted vs imported
        int staticDiffer = 0;       //!< channels, the import report's own check (W0)
        int showDefinitions = 0;    //!< fixture definitions used from the show
        QList<Played> played;
        QStringList mixedIncludes;
        QStringList problems;
    };
    DryRun dryRunImport(const QString &path);

    quint32 dimmer(int level, const QString &name);
    quint32 colour(const QString &name);
    FixtureGroup *group(const QString &name, const QList<quint32> &fixtures);
    /** A show with one of every kind of word the command line takes. */
    ShowTreeCommand::Context completionShow();
    QHash<quint32, uchar> settle(int ticks = 10);
    void tick(int n = 1);
    void go(Chaser *c);
    void stopAll();
    QList<QHash<quint32, uchar>> playAll(Chaser *c);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
