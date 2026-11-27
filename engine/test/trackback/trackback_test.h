/*
  Q Light Controller Plus
  trackback_test.h

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

#ifndef TRACKBACK_TEST_H
#define TRACKBACK_TEST_H

#include <QObject>
#include <QList>
#include <QMap>

class Chaser;
class Fixture;
class Doc;

/* The output at a cue is the same however the cue was reached: GO from the
   top, GO GO Back GO, a jump, Back from the cue after. */
class TrackBack_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void backThenGoKeepsTheColourTheCuesBeforeSet();
    void colourTracksTheSameWhicheverWayYouArrive();
    void fadesHoldsAndLoop();
    void blockCueOnlyReleaseIncludesAndMix();
    void aLongList();
    /** Skipped cues (2026-10-21): GO from the top plays exactly what the
     *  list with them deleted plays; every way of arriving lands the same
     *  (the restack walks past them); a jump to one plays it. */
    void skippedCuesPlayAsIfDeleted();
    /** The report (2026-11-26): a curtain an early cue opens stayed closed
     *  when a later cue was loaded; the house lights at half and a relay
     *  the same. A curtain driven by its Open/Fly channels (LTP), one
     *  opened by its intensity (the Curtain plot symbol: HTP), house
     *  dimmers and a Switch relay: every way of arriving lands on each cue
     *  as GO from the top does -- 0 (house out, curtain closed) too. */
    void curtainsHouseAndRelaysTrackOnAJump_data();
    void curtainsHouseAndRelaysTrackOnAJump();
    /** His second report: GO to the cue that opens the traveller and,
     *  while it is still opening, jump on (or Back): it stopped where it
     *  was, part open, and stayed. It ends where the run from the top
     *  leaves it -- the channel and the plot's pose of it. */
    void aJumpDuringTheOpeningFadeEndsOpen_data();
    void aJumpDuringTheOpeningFadeEndsOpen();
    /** Board 2026-11-26 (safety): Stop ALL, Fade and stop or the
     *  emergency stop while a curtain moves -- opening, closing, flying
     *  out -- stops it where it is (it could be dragging a set piece), and
     *  it stays there: the cue letting go, putting the rig at rest, a
     *  function started and stopped after, never move it. The lights still
     *  go out, a flame relay off. The next GO carries on. */
    void aStopLeavesACurtainWhereItIs_data();
    void aStopLeavesACurtainWhereItIs();
    /** And the footer says so: "Curtain stopped at 40% — GO to carry on";
     *  gone at the GO. */
    void theFooterSaysWhereTheCurtainStopped();
    /** Board Q350/Q351, "Behaves as": a relay (Switch), house lights and
     *  a kabuki drop (One-shot) stay where they are when a cue is picked
     *  in Design -- only a run moves them -- while a light previews. */
    void switchesAndDropsHoldWhileEditing();
    /** A Back never fires a one-shot again: dropped stays dropped, Back to
     *  the cue before the drop too; the next GO plays on. */
    void aBackNeverFiresAOneShotAgain();
    /** A hazard (a CO2 jet, a fogger) is never fired -- by a pick, a
     *  preview or, until arming lands (Q352), a GO -- and the footer says
     *  why. A parked one parks off. */
    void aHazardIsNeverFiredYet();
    /** The emergency stop: a switch and a one-shot hold their state, a
     *  hazard is off, the lights go out. */
    void aStopHoldsSwitchesAndDrops();
    /** Board 053p review, decided: an emergency stop during an edit's
     *  let-go glide, then a GO: the GO drops what is left of the glide, so
     *  the cue owns the curtain from where the stop froze it. */
    void aGoAfterAStopMidGlideDropsTheGlide();
    /** Park wins over cues, the edit hold and the stop's release. */
    void parkWinsOverHoldsAndCues();
    /** The field: guessed (old shows load unchanged), set, saved; the
     *  definition's laser class; Show Review lists what was set by hand. */
    void behavesAsIsGuessedSetAndSaved();

    /* The independent review of feat/053-behaves-as (each fails on
       fa11984b3; base APIs only, so they build there). */
    /** Input passthrough never reaches a hazard held safe. */
    void passthroughNeverFiresAHazard();
    /** A hazard parked at a value (run to 255 as a Light, parked, then made
     *  a Hazard) is held at 0, not at what the Park saved. */
    void aParkedHazardStaysOff();
    /** A light overlapping a hazard's address, parked later, never writes
     *  over the hazard's hold (the hold sits on top of every Park). */
    void theHazardHoldWinsOverALaterPark();
    /** Made a hazard mid-show: held from the very next frame, not after
     *  the event loop's rebuild. */
    void aHazardIsHeldFromTheNextFrame();
    /** A Back from any route (a chaser's own step: the Simple Desk, a pad,
     *  the chaser editor's Test) holds the one-shot. */
    void everyBackHoldsTheOneShot();
    /** Guessed by whole words; a light by name or definition is never
     *  Machinery or One-shot by name alone; MagicFX's confetti guns. */
    void theGuessReadsWholeWords();
    /** Reading what a rig behaves as again (the Fixture Manager's column,
     *  on every fixtureChanged) costs nothing once guessed. */
    void theGuessIsKept();
    /** OutputLayer forgets the glides that are over. */
    void finishedGlidesAreForgotten();
    void courseShows_data();
    void courseShows();
    void aRealShow();
    /** Diagnostic, env-gated (QLC_CURTAIN_SHOW): his show's curtain. */
    void hisCurtain();

private:
    enum Step { Go, Back, Jump };
    struct Move
    {
        Step step;
        int cue;     //!< Jump: where to; else unused
    };

    /** The purpose-built list (§ the task): A red, B blue, A green, Block,
     *  a tracking cue. Its chaser. */
    Chaser *colourList();
    /** One with every mode: Block, Cue-only, Release, an include, a cue
     *  that mixes. Its chaser. */
    Chaser *everyModeList();
    /** House to half, House out + the curtains open, two stage cues. */
    /** @a showOnly: the curtains and the house relay Show only; @a operate:
     *  the show in Operate (else Design); @a unpatched: the curtain
     *  fixtures unpatched (only drawn). */
    Chaser *houseAndCurtainList(bool showOnly = false, bool operate = false, bool unpatched = false);
    void curtainRows();

    bool rig();
    Chaser *chaserOf(quint32 listId) const;
    bool set(quint32 cue, quint32 group, quint32 palette);

    void tick(int n = 1);
    QByteArray settle(int ticks);
    QByteArray land(Chaser *c);
    /** settle(m_settle), stopping sooner once nothing more can change. */
    QByteArray settleCue(Chaser *c);
    /** Every fade over and the cue held until a GO: the next tick changes
     *  nothing but what an effect moves. */
    bool still(Chaser *c) const;
    void act(Chaser *c, const Move &m);
    void stopAll();
    /** What each cue of @a c outputs played straight from the top. */
    QMap<int, QByteArray> straightRun(Chaser *c);
    /** Plays @a moves from a dark stage, comparing every cue landed on
     *  with @a ref: the channels that differ, summed; @a why says where. */
    int walk(Chaser *c, const QMap<int, QByteArray> &ref, const QList<Move> &moves, QString *why);
    /** Every short sequence and a set of random walks over @a c. */
    int everyWay(Chaser *c, int randomWalks, QString *why);
    bool loadShow(const QString &path);
    /** The house list with cue 2 opening the curtains over @a fadeMs, a
     *  Block cue 5 closing them (the house at half, a flame on) and a cue 6
     *  flying the drape out, both over @a fadeMs. */
    Chaser *movingCurtainList(bool showOnly, int fadeMs);
    /** Stop ALL (@a fadeMs > 0: Fade and stop), as Control ▸ Stop all
     *  does it, the timer played here meanwhile. */
    void stopAllFromControl(int fadeMs);
    Fixture *fixtureNamed(const QString &name) const;
    /** Board Q351's rig: a stage wash (Light), a house relay (Switch), a
     *  kabuki drop (One-shot) and a CO2 jet (Hazard), one dimmer each;
     *  cue 1 the wash full, the rest out; cue 2 the wash half, the relay
     *  on, the drop and the jet fired; cue 3 the wash full (the rest
     *  track). @p fadeMs on every cue. */
    Chaser *behavesList(int fadeMs = 0);
    int at(const char *fixture, int ch = 0);
    quint32 m_c1 = 0, m_c2 = 0, m_c3 = 0;

    Doc *m_doc = nullptr;
    QMap<int, QByteArray> m_moving;   //!< per cue of the straight run: channels an effect moves
    int m_settle = 8;                 //!< ticks for the list's longest fade
    quint32 m_list = 0;
    quint32 m_a = 0, m_b = 0, m_all = 0;
    quint32 m_red = 0, m_blue = 0, m_green = 0, m_white = 0;
    quint32 m_full = 0, m_half = 0, m_front = 0, m_side = 0;
};

#endif
