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

private:
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
