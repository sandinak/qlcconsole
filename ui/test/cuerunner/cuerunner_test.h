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
    void theRunTabShowsHitsThatOverride();
    /** Teaching: each GO and Back is written in crew words, tracked layers named */
    void theTeachingLogSaysWhatEachGoDid();
    /** Not a Teaching show: no log pane, no lines kept, plain tooltips */
    void nothingShowsWhenTeachingIsOff();
    /** Teaching: terms on the Run tab's heads and buttons carry the glossary */
    void glossaryLinesOnTheRunTab();
    void goSubGoBackAndWhatChanges();
    /** The pad's role on the Run tab (0.4 Pad Run): Tools / Run / ARMED,
     *  a click switches it, and the tooltip carries the button map. */
    void thePadRoleShowsOnTheRunTab();
    void theRunTabHasTheGroupMasters();
    void clickLoadsDoubleClickJumps();
    void aClickInDesignShowsTheCue();
    void runnersAndShowControlMirrorOneState();
    void aRunnerFollowsOneListOfAnyProduction();
    /** Q75: one list, no Current production drop-down */
    void oneListHasNothingToChoose();
    /** 0.4.5: the Run tab's list is the pad's GO's list, saved with the show. */
    void theRunTabsListIsThePadsAndIsSaved();
    void theBoxNamesTheOpenedShowsProduction();
    void liveDesignAndTimecode();
    void theNextCueAtATimecodeAndItsCountdown();
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

    /* Resume after a crash (crash-resume-cue, CueResume). */
    /** Each GO, Back and Stop in Operate writes the record once; frames,
     *  Design and a show with no file write nothing. */
    void theLiveCueIsKeptOncePerCommand();
    /** Kill simulation: a fresh Doc after a crash offers the cue, in
     *  Operate only; Load as Standby LOADs it, plays nothing, GO goes on. */
    void afterACrashTheRunTabOffersTheCueAsStandby();
    /** Dismiss forgets it: the record goes, the next start offers nothing. */
    void dismissForgetsTheOffer();
    /** A clean close (another show, quit) leaves nothing to offer. */
    void aCleanCloseOffersNothing();
    /** A cue deleted since is said, never loaded -- not even the cue that
     *  has its number now. */
    void aDeletedCueIsSaidNotLoaded();
    /** A GO before Load as Standby takes the offer away. */
    void aGoOvertakesTheOffer();
    /** With QLC_SHOT_DIR: the Run tab before (no offer) and with it, at
     *  1024, 1280 and 1920 wide. */
    void resumeOfferShots();
    /** Q62: no main cue list -- GO and the rest grey, why, and Upgrade. */
    void noMainListGreysGoAndOffersUpgrade();

private:
    /** Production "Dance" (current) with "Song": cue 1 (A Half), cue 2
     *  (A Full, B Half) with sub-cues 2.1 (B Full) and 2.2 (A Half), cue 3;
     *  and production "Corporate" with "Keynote": one cue (B Full). Ids:
     *  song, c1, c2, c3, s1, s2, keynote. */
    QList<quint32> show();
    QHash<quint32, uchar> settle(int ticks = 10);
    /** A session of show() in Operate at cue @a gos (GOs from the top),
     *  its record in @a path for show file @a show, then a crash: the Doc
     *  goes without a clean close and a fresh one loads the same show. The
     *  show's XML. */
    void crashedSession(const QString &path, const QString &show, int gos);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
