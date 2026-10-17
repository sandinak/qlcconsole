/*
  Q Light Controller Plus
  aimsolver.h

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

#ifndef AIMSOLVER_H
#define AIMSOLVER_H

#include <QVector3D>
#include <QList>
#include <QHash>

#include "linespread.h"

class Doc;
class Scene;
class StageTarget;
class QLCPalette;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Single source of truth for "what pan/tilt aims a fixture's beam at a world
 * point". BOTH the Aim palette (design preview) and the follow-spot effect's
 * pre-computed aim use this, so a fixture points the same way in Edit and Run —
 * previously they had two divergent formulas and the beam jumped on handoff.
 */
struct FixtureRigProps;

namespace AimSolver
{
    /** Everything the aim geometry needs about one rigged fixture, looked up
     *  once. Solving against a Frame is plain arithmetic, so a caller sweeping
     *  thousands of points -- a map of where a head can reach -- pays for the
     *  Doc lookups once rather than per point. */
    struct Frame
    {
        QVector3D position;          ///< beam origin, metres
        bool  floorMounted = false;  ///< home points up, not down
        float panZeroDir = 0.0f;
        float panOffsetDeg = 0.0f;
        float tiltOffsetDeg = 0.0f;
        bool  panInvert = false;
        bool  tiltInvert = false;
        float panMax = 360.0f;
        float tiltMax = 270.0f;
    };

    /** Which way up a head is rigged: standing (home points UP) or hanging.
     *  One rule, because the solver writing pan/tilt and the views drawing the
     *  beam from those same numbers must never disagree about it -- and they
     *  did: a standing head's DMX was drawn as though it hung. A fixture with
     *  no structure under it is standing on the deck, whatever the rig-props
     *  default says (TopHung leaks in whenever an entry is auto-created). */
    bool floorMounted(const ::FixtureRigProps &rp);

    /** Where a head has to point for a world point, and whether it can. */
    struct Solution
    {
        float panDeg = 0.0f;     ///< fixture degrees, within 0..panMax
        float tiltDeg = 0.0f;    ///< fixture degrees, within 0..tiltMax
        /** Degrees to spare before the nearest pan or tilt limit, on the chosen
         *  way of pointing there. Negative = past a limit: the head cannot
         *  point there, and the angles above are the nearest it can get. */
        float marginDeg = 0.0f;
        bool reachable() const { return marginDeg >= 0.0f; }
    };

    /** The fixture's aim geometry. False if it has no usable rig geometry. */
    bool frameFor(Doc *doc, quint32 fixtureId, Frame &frame);

    /** Solve @p frame for @p targetWorld.
     *
     *  A yoke reaches most points two ways: pan toward the point and tilt
     *  forward, or pan half a turn further and tilt back past home. The first
     *  is what this has always answered, and it is kept whenever it is within
     *  range, so a head does not swing round mid-move. The second is taken only
     *  when the first is out of range and the second is not as far out --
     *  which is the case a clamp used to get silently wrong. */
    Solution solve(const Frame &frame, const QVector3D &targetWorld);

    /** Compute the fixture's pan/tilt (degrees) to aim its beam at @p targetWorld
     *  (metres, absolute — the caller bakes in platform/subject height). Honours
     *  the fixture's rig position, mounting (floor vs hung), pan-zero facing,
     *  invert flags and calibration offsets. Returns false if the fixture has no
     *  usable rig geometry. */
    bool aimDegrees(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                    float &panDeg, float &tiltDeg);

    /** The same, keeping how much reach is left. False as aimDegrees(). */
    bool reach(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
               Solution &solution);

    /** Where head @p fixtureId should point for target @p target: the shared
     *  rule (MonitorProperties::targetAimPointFor()) with this head's place
     *  and its beam as wide as its zoom and iris have it now -- a wider beam
     *  following a person aims lower, so it covers them without top wash. */
    QVector3D aimPointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                          bool subjectMode);

    /** The heads an Aim look in @p scene points, in LOOK ORDER: each of the
     *  scene's groups' members in the group's head order, then the scene's
     *  own fixtures; once each; only heads that can point (pan or tilt) and
     *  are placed on the plot. */
    QList<quint32> aimHeads(Doc *doc, const Scene *scene);

    /** @p fixtures (in their order, once each) less any that cannot point or
     *  are not placed on the plot. */
    QList<quint32> aimable(Doc *doc, const QList<quint32> &fixtures);

    /** A line target: how far along it (0..1) each of @p heads lands, by
     *  @p settings. @p heads is any ordered list -- one look's heads, or the
     *  union of several -- in look order; Order "along the line" re-sorts it
     *  by where the heads hang. Empty if @p target is not a line. */
    QHash<quint32, double> lineFractions(Doc *doc, const StageTarget *target,
                                         const QList<quint32> &heads,
                                         const LineSpread::Settings &settings);

    /** @p heads in the order they take places on line @p target: as given
     *  (LookOrder), or by where they hang measured from the line's first end
     *  towards its last (AlongLine). */
    QList<quint32> orderedLineHeads(Doc *doc, const StageTarget *target,
                                    const QList<quint32> &heads, LineSpread::Order order);

    /** The world point @p fraction of the way along line target @p target. */
    QVector3D linePoint(Doc *doc, const StageTarget *target, double fraction);

    /** Each head's aim point for Aim look @p look on line @p target, over
     *  @p heads (as lineFractions()). */
    QHash<quint32, QVector3D> linePoints(Doc *doc, const QLCPalette *look,
                                         const StageTarget *target,
                                         const QList<quint32> &heads);

    /** True if @p scene carries a follow-spot effect, which is what makes an
     *  Aim look in it track a SUBJECT (chest height above the deck) rather
     *  than the target's own point. */
    bool sceneFollowsSubject(Doc *doc, const Scene *scene);

    /** One head a scene's Aim look points, where, and whether it can. */
    struct SceneAim
    {
        quint32   fixtureId;
        quint32   targetId;
        QVector3D point;         ///< the aim point, as the palette resolves it
        Solution  solution;
    };

    /** Every aimable head each Aim look in @p scene points at its target --
     *  the scene's fixtures and its groups' members, exactly the set the
     *  palette writes pan and tilt for -- solved against the same point the
     *  palette uses. Heads with no rig geometry are left out: there is no
     *  answer to give for them. */
    QList<SceneAim> sceneAims(Doc *doc, const Scene *scene);
}

/** @} */

#endif
