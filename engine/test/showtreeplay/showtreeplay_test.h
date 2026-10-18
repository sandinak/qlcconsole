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

/* The Show Tree playing a show (SHOW_TREE_DESIGN.md §8): P5 several
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

private:
    quint32 dimmer(int level, const QString &name);
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
    void tick(int n = 1);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
