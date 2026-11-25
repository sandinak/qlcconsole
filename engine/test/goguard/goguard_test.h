/*
  Q Light Controller Plus
  goguard_test.h

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

#ifndef GOGUARD_TEST_H
#define GOGUARD_TEST_H

#include <QObject>
#include <QList>

#include "showcontrol.h"

class Chaser;
class Doc;

/* The double-GO guard (board Q233; the rules at ShowControl::run()): each
   rule as a sweep across every GO source -- every route into run(), and
   the real paths of the pad and OSC -- on a clock the test moves. */
class GoGuard_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Rule 1: every source after every source, the same one twice too: a
     *  GO 0.4 s after one is held, the cue does not move, and the hold
     *  names the source that took it. */
    void aSecondGoInTheWindowIsHeldFromEverySource();
    /** Rule 1: held up to the window's end, played at it; a held GO does
     *  not lengthen it. */
    void theWindowRunsFromTheGoThatPlayed();
    /** Rule 1: 0 turns it off, and the engine alone starts off. */
    void zeroTurnsItOff();
    /** Rule 3: Back, Sub GO, Sub Back and Stop are held after a GO, and
     *  hold a GO after them, from every source. */
    void backSubCuesAndStopAreHeldTheSame();
    /** Rule 3: ALL OFF is never held, from any source, and clears the
     *  window; Pause, Resume, Load and Reset are not held. */
    void theEmergencyStopIsNeverHeld();
    /** Rule 4: lists are independent, from every source. */
    void cueListsAreIndependent();
    /** decided: timecode GOs are never held and open no window. */
    void timecodeIsNeverHeld();
    /** Rule 2: the words, and how long heldNotice() keeps them. */
    void theHoldIsSaidInOneText();
    /** Rule 2 on the pad: a long buzz and the words aloud, whoever's GO
     *  was held -- in the Run role only; the pad's own GO is said once. */
    void thePadBuzzesLongForEveryHold();
    /** Rule 5: OSC, MSC and the pad's own path are held in run(), before
     *  the run handler sees them. */
    void heldBeforeTheRunHandler();
    /** Rule 6: the guard's clock is read on a command, never per frame. */
    void nothingIsTimedPerFrame();
    /** A held GO never reaches execute(), so the emergency stop's latch on
     *  Show-only fixtures stays set through it; ALL OFF clears the window,
     *  so the GO after it plays and lets the latch go. */
    void aHeldGoNeverLetsGoOfTheEmergencyStop();
    /** Rule 5 off run(): a VC button's step on a plain chaser (outside the
     *  Show Tree) is held the same way, and opens the window for run(). */
    void aVcStepOnAPlainChaserIsHeld();

    /** Q328, rule 1 follows the cue: after a 0-fade cue a GO 300 ms on
     *  goes, one 100 ms on is held ("cue 1 still starting"); after a 2 s
     *  fade a GO at 400 ms goes (its fade has begun). */
    void theGuardFollowsTheCue();
    /** Q328: a cue marked Fast has no guard after it -- the GO straight
     *  after goes; the cue after it is guarded again. */
    void aFastCueHasNoGuard();
    /** Q328: a cue the runner has not begun is held up to the setting and
     *  no longer; a setting under goBegunMs() caps that too; Fast and the
     *  floor change nothing for Back (rule 3 keeps the whole window). */
    void theCapHolds();
    /** Q328: every route after every route follows the same one rule:
     *  held at 100 ms, played at 300 ms, never held after a Fast cue. */
    void everyRouteSharesTheCueRule();
    /** Q328: Fast is saved with the show (the Show Tree cue and its chaser
     *  step, Fast="1") and read back; a show without it reads Off. */
    void fastIsSavedAndLoaded();

private:
    Chaser *makeList(int steps, const QString &name);
    Chaser *makeSubCueList();
    void tick();
    void stopAll();
    /** The routes a person or a device GOs by: all but RouteTimecode. */
    static QList<ShowControl::Route> sources();
    bool send(ShowControl::Command command, ShowControl::Route route, Chaser *list,
              const QString &cue = QString());

    Doc *m_doc = nullptr;
    qint64 m_now = 0;
    int m_clockReads = 0;
};

#endif
