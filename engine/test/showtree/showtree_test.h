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

private:
    quint32 dimmer(int level, const QString &name);
    quint32 colour(const QString &name);
    FixtureGroup *group(const QString &name, const QList<quint32> &fixtures);
    QHash<quint32, uchar> settle(int ticks = 10);
    void tick(int n = 1);
    void go(Chaser *c);
    void stopAll();

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
