/*
  Q Light Controller Plus
  cuerunner_test.h

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

#ifndef CUERUNNER_TEST_H
#define CUERUNNER_TEST_H

#include <QObject>
#include <QHash>

class Doc;

/* The cue runner (docs/design/SHOW_TREE_DESIGN.md §6B, P7): the Run tab's runner and
   the Cue Runner VC widget. Offscreen; the timer is stepped by hand and the
   universes read, never sent. */
class CueRunner_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void theRunnerShowsTheListAndItsSubCues();
    void goSubGoBackAndWhatChanges();
    void clickLoadsDoubleClickJumps();
    void aClickInDesignShowsTheCue();
    void runnersAndShowControlMirrorOneState();
    void aRunnerFollowsOneListOfAnyProduction();
    /** Q75: one list, no Current production drop-down */
    void oneListHasNothingToChoose();
    void theBoxNamesTheOpenedShowsProduction();
    void liveDesignAndTimecode();
    void spaceIsGoInTheRunTab();
    void theVcWidgetSavesLoadsAndCopies();
    void swapWidgetOnAPage();
    void vcActionsFromInputsAndKeys();
    void vcFeedbackFollowsTheCue();
    void vcResizeGrowsTheInfo();
    void vcHandlesSizeAndPersist();
    void vcPropertiesRoundTrip();
    void textColoursOnEveryLine();
    void screenshots();
    /** Q62: no main cue list -- GO and the rest grey, why, and Upgrade. */
    void noMainListGreysGoAndOffersUpgrade();

private:
    /** Production "Dance" (current) with "Song": cue 1 (A Half), cue 2
     *  (A Full, B Half) with sub-cues 2.1 (B Full) and 2.2 (A Half), cue 3;
     *  and production "Corporate" with "Keynote": one cue (B Full). Ids:
     *  song, c1, c2, c3, s1, s2, keynote. */
    QList<quint32> show();
    QHash<quint32, uchar> settle(int ticks = 10);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
