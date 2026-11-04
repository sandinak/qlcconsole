/*
  Q Light Controller Plus
  areacover.h

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

#ifndef AREACOVER_H
#define AREACOVER_H

#include "nolightzone.h"
#include <QVector3D>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QList>
#include <QHash>
#include <QString>

#include "grouphead.h"

class Doc;
class Scene;
class StageTarget;
class MonitorProperties;

/** @addtogroup engine Engine
 * @{
 */

/**
 * An Aim look at an Area covers the area (docs/design/AREA_COVER.md, board
 * Q209): the area is split into one zone per light by where each hangs --
 * low and downstage lights take the downstage ranks, high and upstage ones
 * the ranks behind, stage right lights the stage right cells -- and each
 * light aims at its zone's centre, at subject height above the deck, riser
 * or step actually under it. Coverage says how much of the area the beams,
 * as wide as their zoom has them, really light, and where they leave a gap.
 *
 * Plan coordinates throughout: x across the stage (+X stage left), y up and
 * down it (+Y downstage), metres.
 */
namespace AreaCover
{
    /** Whether an Aim at @p target covers it: an Area with a size. Anything
     *  else -- and an Area of no size -- is a point aim, as before. */
    bool covers(const StageTarget *target);

    /** The height above the surface an area is lit at: the target's
     *  aim-height override, else a person's chest (1.4 m). */
    float subjectHeight(const StageTarget *target);

    /** The area in plan, centred on where the target resolves to
     *  (MonitorProperties::targetAimPoint(), so a bound area follows). */
    QRectF areaRect(const MonitorProperties *props, const StageTarget *target);

    /** The beam width used when a fixture's definition gives none. */
    constexpr double defaultBeamDegrees() { return 20.0; }

    /** The pure rule: one cell of @p area for each light hanging at
     *  @p where (in that order), and the grid's shape. Rows run from
     *  downstage (row 0, the largest y) upstage; each row is split evenly
     *  across. Each light sits among the others across (x) and in depth
     *  (its depth key z - y: low and downstage first), both scaled 0..1, and
     *  the cells are matched to the lights so the total squared distance
     *  between the two is least (docs/design/AREA_COVER.md).
     *  @p rows, when given, gets how many rows the area was cut into. */
    QVector<QRectF> cells(const QRectF &area, const QVector<QVector3D> &where, int *rows = nullptr);

    /** One light's share of the area. */
    struct Zone
    {
        GroupHead member;
        QRectF cell;            //!< its part of the area, plan metres
        QVector3D aim;          //!< the cell's centre, at subject height above the surface there
        QPointF offset;         //!< the cell's centre from the area's centre
    };

    /** The zones of @p target over @p members (AimSolver::aimMembers()), in
     *  the members' order. Members with no rig geometry get none. Empty if
     *  the target is not a covered area. */
    QList<Zone> zones(Doc *doc, const StageTarget *target, const QList<GroupHead> &members);
    /** Each member's aim point: what an Aim look at the area writes. */
    QHash<GroupHead, QVector3D> memberPoints(Doc *doc, const StageTarget *target,
                                             const QList<GroupHead> &members);
    /** The aim point @p offset from the area's centre, read fresh: at
     *  subject height above the surface there (a dragged area is followed). */
    QVector3D pointAt(const MonitorProperties *props, const StageTarget *target, const QPointF &offset);

    /** Where a cone from @p origin along @p aim, @p beamDeg wide, meets the
     *  level plane through @p aim: its outline in plan (a circle straight
     *  down, an ellipse at an angle, cut off 40 m out where it never comes
     *  down). @p points around. */
    QVector<QPointF> footprint(const QVector3D &origin, const QVector3D &aim, double beamDeg,
                               int points = 48);

    /** One light's beam on the area. */
    struct Beam
    {
        GroupHead member;
        QVector3D origin;       //!< where it shines from (the arm's own origin)
        QVector3D aim;          //!< its zone point
        double degrees = 0.0;   //!< its beam, as wide as its zoom has it
        QVector<QPointF> patch; //!< its footprint at the aim's height (plan)
        LightMask mask;         //!< tape over its front (none: the whole cone)
    };

    /** How much of an area the beams light. */
    struct Coverage
    {
        QString name;           //!< the area's name
        QRectF area;            //!< plan metres
        QList<Zone> zones;
        QList<Beam> beams;
        int cols = 0, rows = 0; //!< the sample grid; sample (c, r) row 0 upstage
        QVector<bool> lit;      //!< cols x rows, row-major
        double fraction = 1.0;  //!< lit samples / all samples
        QString gap;            //!< where the gap is ("upstage left"), empty: none
        bool valid() const { return !area.isEmpty(); }
        /** The unlit samples, as plan rectangles (runs along each row). */
        QList<QRectF> gapRects() const;
        /** "Covers 86% of Chorus · gap upstage left" (no gap: "Covers all of Chorus"). */
        QString caption() const;
        /** "gap upstage left", "gaps all over", or empty. */
        QString gapPhrase() const;
    };

    /** Coverage of area @p target by @p members aimed at it, each beam as
     *  wide as @p scene's zoom has it (the fixture's own values where the
     *  scene sets none), else its lens, else defaultBeamDegrees(). */
    Coverage coverage(Doc *doc, const StageTarget *target, const QList<GroupHead> &members,
                      const Scene *scene = nullptr);
    /** The same from given beams (tests, and anything that knows them):
     *  @p samplesAcross samples across the area. */
    Coverage coverage(const MonitorProperties *props, const StageTarget *target,
                      const QList<Beam> &beams, int samplesAcross = 48);

    /** Where most of an unlit set of samples lies, in crew words: "upstage
     *  left", "centre stage", "downstage right" -- or two of them joined by
     *  "and" when the gap is split, or "all over" when most of the area is
     *  dark. Empty when everything is lit. */
    QString gapName(const QVector<bool> &lit, int cols, int rows);

    /** A gap bigger than this share of an area is worth a note (MOV-13). */
    constexpr double gapThreshold() { return 0.10; }
}

/** @} */

#endif
