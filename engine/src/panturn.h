/*
  Q Light Controller Plus
  panturn.h

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

#ifndef PANTURN_H
#define PANTURN_H

#include <QString>
#include <QList>

#include "aimsolver.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The turn a moving head's pan works in (FixtureRigProps::panTurn), and
 * the stage it has to cover.
 *
 * A head with more than 360 degrees of pan reaches most bearings at two
 * pans a turn apart. Whichever it answers with, somewhere round it is a
 * SEAM: the bearing where the answer jumps a whole turn, and a beam moved
 * across it swings round the other way. Turn 0 puts the seam straight
 * behind the pan zero. For a head out front whose zero faces away from
 * the stage that is in the middle of the stage, and a follow spot walked
 * across it "runs up against the start and end of the channel".
 *
 * The turn is part of the rig: stored, saved, and changed only by the
 * person -- the Fixture Properties buttons, Show Review's fix, or a
 * proposal the app makes when a head is placed or moved (and says so). It
 * is never worked out again on its own, so no calibration nudge, move of a
 * centimetre, resized grid or change of units can swing a head round.
 *
 * The STAGE SPAN is the bearings from the head to the stage outline: the
 * stage decks (Lighting Studio stage pieces) when there are any, else the
 * stage grid. A head over the stage sees it all round: the seam is on the
 * stage whatever the turn, and turn 0 -- its pan zero -- rules.
 */
namespace PanTurn
{
    /** How clear of the seam the stage has to be, degrees: a subject at
     *  the edge of the deck, or a step past it, is still on it. Less than
     *  this counts as on the seam. */
    constexpr float kClearDeg = 10.0f;
    /** The cone under a hung head, degrees from straight down to a subject,
     *  that is left out of its stage span: someone walking under a head
     *  swings its pan through every bearing whatever the turn. */
    constexpr double kNearConeDeg = 20.0;

    /** The bearings a head sees the stage across: from @c from clockwise
     *  (the bearings' sense: 0 downstage, 90 stage right), @c width
     *  degrees. @c overStage: the head is over the stage, it is all round. */
    struct Span
    {
        bool  valid = false;
        bool  overStage = false;
        float from = 0.0f;
        float width = 0.0f;
        float to() const;
    };

    /** Everything about one head's turn, for the views and Show Review. */
    struct Info
    {
        bool  valid = false;        ///< a moving head, placed, with a pan channel
        bool  hasTurns = false;     ///< more than 360 degrees of pan: a turn to choose
        int   turn = 0;             ///< stored
        int   lowest = 0, highest = 0;   ///< the turns it can take
        float seam = 0.0f;          ///< bearing of the seam at the stored turn
        float centre = 0.0f;        ///< bearing with the most room (seam + 180)
        Span  span;                 ///< the stage from the head
        float margin = 0.0f;        ///< degrees the stage is clear of the seam; < 0 crosses it
        int   proposal = 0;         ///< what "Centre the pan on the stage" sets
        float proposalMargin = 0.0f;
        /** The stage is on its seam, or within kClearDeg of it. */
        bool crossesSeam() const { return hasTurns && span.valid && !span.overStage && margin < kClearDeg; }
        /** Show Review's question: it crosses, and a turn clears it or does better. */
        bool fixable() const { return crossesSeam() && proposal != turn && proposalMargin > margin + 0.5f; }
    };

    /** The turns a head can take: @p lowest..@p highest, each a different
     *  seam. Both 0 for a head with one turn of pan or less. */
    void turnRange(const AimSolver::Frame &frame, int &lowest, int &highest);
    /** The bearing of the seam on turn @p turn (0..360). */
    float seamBearing(const AimSolver::Frame &frame, int turn);
    /** How far the span is clear of turn @p turn's seam, degrees: negative
     *  when it crosses it (by that much). */
    float margin(const AimSolver::Frame &frame, const Span &span, int turn);

    /** The stage from fixture @p fixtureId. Invalid with no geometry. */
    Span stageSpan(Doc *doc, quint32 fixtureId);
    /** All of it, read once. */
    Info info(Doc *doc, quint32 fixtureId);

    /** "Centre the pan on the stage": the turn that puts the stage span
     *  clear of the seam with the most room -- 0 whenever the stage already
     *  clears turn 0's seam by kClearDeg, and for a head over the stage
     *  (the person's pan zero rules). */
    int proposal(Doc *doc, quint32 fixtureId);
    /** After the person placed or moved the head (or patched it): its turn,
     *  changed only when the stage now crosses the seam at the turn it has
     *  and the proposal does better. */
    int proposalAfterMove(Doc *doc, quint32 fixtureId);

    /** Set the head's turn (clamped to what it can take). True if it changed. */
    bool setTurn(Doc *doc, quint32 fixtureId, int turn);
    /** "Centre the pan on the stage": setTurn(proposal()). */
    bool centre(Doc *doc, quint32 fixtureId);

    /** What Fixture Properties' "Pan works around:" says: "270° stage left
     *  ±180° (turn 1)", the bearing with the most room either way. */
    QString worksAround(const Info &info);
    /** Where the seam is, and what it means for the stage: clear of it, on
     *  it (and what Centre on Stage would do), or the head over the stage. */
    QString stageText(const Info &info);
    /** "Spot 1: turn 0 → 1" with what moves, for a status line or a
     *  confirmation: the seam's bearing before and after. */
    QString changeText(Doc *doc, quint32 fixtureId, int from, int to);
    /** 0 downstage, 90 stage right ...: the nearest of eight names. */
    QString bearingName(float bearing);
}

/** @} */

#endif
