/*
  Q Light Controller Plus
  showtreeplay_test.h

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

#ifndef SHOWTREEPLAY_TEST_H
#define SHOWTREEPLAY_TEST_H

#include <QObject>
#include <QHash>

class FixtureGroup;
class Chaser;
class Doc;

/* The Show Tree playing a show (docs/design/SHOW_TREE_DESIGN.md §8): P5 several
   productions, P6 sub-cues in the tree, P7 the cue runner. Offscreen, no
   DMX out: the universes are read, never sent. */
class ShowTreePlay_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // P5: multiple productions
    void theCurrentProductionIsWhatGoPlays();
    void aMainCueListSetElsewhereMovesTheCurrentProduction();
    void aProductionsMainList();
    void eachProductionHasItsTimecodeSource();
    void productionsSaveAndLoad();
    void oneUndoHistoryAcrossProductions();
    void looksSectionsPerProduction();
    void deletingTheCurrentProduction();
    void productionCommands();
    void aNewProductionIsCurrentWhenNothingElseHasGo();

    // P6: sub-cues in the tree
    void subCuesTrackFromTheirCueAndAreCueOnly();
    void subCuesBakeToAHeldChaser();
    void subGoPlaysThemOnTheRig();
    void autoFollowSubCuesGoOnByThemselves();
    void aSceneSharedThroughSubCuesNeverRefades();
    void subCuesSaveLoadUndoAndDelete();
    void numberingSkipsSubCueNumbers();
    void subCuesAreRefusedWhereTheyCantBe();
    void subCueCommands();

    // 0.4: sub-cue <-> inline cue, every step playing as before
    void subCuesBecomeInlineCuesAndBack();
    void aCueBecomesASubCueCarryingWhatItPassedOn();
    void aBlockCueBecomesASubCueAndBack();
    void aMiddleSubCueMovesAloneTheOthersStay();
    void severalCuesGoUnderTheFirstSelected();
    void subCueConversionKeepsIdsAndRefusesWhatItCant();
    void subCueConversionCommands();

    // P7: the cue runner
    void showControlStepsThroughSubCues();
    void whatChangesComesFromTheResolvedTracking();
    void cueOfStepMapsTheChaserBack();

    // The bake re-plans only what changed; hits fade (Q180)
    void incrementalBakeMatchesAFullBake();
    void hitsFadeInAndOut();
    void hitsAreArmedWhileTheirPlaceIsLive();

    /* 0.4: override hits */
    void anOverrideHitPullsIntensityDownAndFadesBackToTheShow();
    void stackedOverridesLastFiredWins();
    void stopAllAndAllOffReleaseOverrides();
    void overrideHitsSaveLoadUndoAndBlind();

    /* 0.4 coverage */
    void aPreviewOrBlindEditSendsNothingToQLab();
    void aHeldCueHitLetsGoOverItsFadeOutWhenItsCueEnds();
    void aListHitDoesNothingWhileItsListIsntRunning();

    /* From the independent evaluation of 0.4.12. */
    /** runningCue() finds a cue with sub-cues while it plays. */
    void theRunningCueWithSubCuesIsFound();
    /** A cue added or deleted earlier in a running list leaves the cue on
     *  stage as the running one, and the next GO plays the next cue and
     *  nothing of a cue that never played. */
    void anEarlierCueAddedOrDeletedMidRunKeepsTheCueOnStage();
    /** A cue LOADed on standby stays the one loaded when a cue is added
     *  before it: GO plays it. */
    void aLoadedCueStaysLoadedWhenACueIsAddedBeforeIt();
    /** The cue on stage deleted from its running list: its light held, no
     *  cue named as running, and GO plays the cue now in its place. */
    void theCueOnStageDeletedHoldsUntilGoThenTheNextPlays();
    /** Back while a deleted cue is held plays the cue before it; with the
     *  first cue deleted, the held cue stays and the list runs on. */
    void backWhileADeletedCueIsHeldPlaysTheCueBeforeIt();
    /** The first cue deleted on stage keeps the list's starting state: no
     *  ghost of it on a later Back. */
    void theFirstCueDeletedLeavesTheListsStartingState();
    /** A held deleted cue's Collection deleted from the show: let go of. */
    void aHeldDeletedCuesFunctionDeletedLetsGo();
    /** Undoing the delete of the held cue makes it the running cue again. */
    void undoingTheDeleteOfTheHeldCueMakesItRunAgain();
    /** Chaser::setSteps() mid-run: the GO after a step added at the top
     *  puts on nothing of it (the runner's tracked state follows). */
    void stepsSetMidRunCarryTheTrackedState();
    /** The main cue list picked outside the tree (a chaser editor) is not
     *  taken back by undoing an unrelated tree edit. */
    void aMainListPickedElsewhereSurvivesAnUnrelatedUndo();

private:
    quint32 dimmer(int level, const QString &name);
    /** One list, cue i setting fixture 0 to @a levels[i], current. */
    quint32 levelsList(const QList<int> &levels, QList<quint32> *cues);
    FixtureGroup *group(const QString &name, const QList<quint32> &fixtures);
    /** A production with one list of @a cues cues, each lighting group
     *  @a groupId at @a level: its list id. */
    quint32 oneListProduction(const QString &name, quint32 groupId, int cues, quint32 level,
                              quint32 *productionId = nullptr);
    QHash<quint32, uchar> settle(int ticks = 10);
    /** A Tracking list "Song": cue 1 (A at Half), cue 2 (A Full, B Half)
     *  with sub-cues 2.1 (B Full) and 2.2 (A Half), cue 3 (nothing of its
     *  own). Ids in order: list, c1, c2, c3, s1, s2. */
    QList<quint32> subCueShow();
    void go(Chaser *c);
    /** Run the main list of @a listId from the top through @a presses
     *  ('g' GO, 's' Sub GO): the rig after each. */
    QList<QHash<quint32, uchar>> walk(quint32 listId, const QString &presses);
    void tick(int n = 1);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
