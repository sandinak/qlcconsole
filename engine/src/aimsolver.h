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
#include "beamocclusion.h"
#include "grouphead.h"

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

    /** The fixture's aim geometry. False if it has no usable rig geometry.
     *  A multi-arm fixture's frame is its body's: frameFor(GroupHead) gives
     *  each arm its own. */
    bool frameFor(Doc *doc, quint32 fixtureId, Frame &frame);

    /*************************************************************************
     * Arms: the heads of one fixture that point on their own
     *
     * A two-arm beam bar has two yokes, each on its own pan and tilt
     * channels, a body-width apart. Aimed as one, both arms took one angle
     * and landed a body-width apart -- on a line, two lights on one place
     * and half the line dark. So every arm is its own MEMBER of an aim: its
     * own origin, its own angle at a point (they converge, at slightly
     * different angles), its own place, number, group and side on a line.
     *
     * A member is a GroupHead: (fixture, head) for an arm, (fixture, -1) for
     * a fixture that aims as one -- a single yoke, or a moving bar whose
     * pixel heads ride one pan and tilt. The quint32 forms below are kept
     * and are FIXTURE-level (a multi-arm fixture is one member there, aimed
     * from its body's centre); everything that writes or draws an aim uses
     * the GroupHead forms.
     *************************************************************************/

    /** The heads of @p fx that point on their own: each has a pan or tilt
     *  channel no earlier head has. Empty unless there are at least two --
     *  a fixture with one yoke, or none, aims as one. */
    QVector<int> arms(const Fixture *fx);
    /** True when head @p head of @p fx is one of arms(). Allocation-free. */
    bool isArm(const Fixture *fx, int head);

    /** Where arm @p head of fixture @p fixtureId sits from the fixture's own
     *  aim origin (frameFor(quint32)), metres, in the world: the centre of
     *  its share of the body, along the body's length (RigHeads::frame()'s
     *  L) -- head i of n at (i - (n-1)/2) x width/n, the slices the rig view
     *  draws each arm in. The definition's Width (Physical Dimensions)
     *  sets the spacing (0.3 m when it declares none). Zero for -1, for a
     *  head that is not an arm, and for a fixture that aims as one. */
    QVector3D armOffset(Doc *doc, quint32 fixtureId, int head);

    /** The aim members of @p fixtures (in their order): aimable() with each
     *  multi-arm fixture opened up into its arms, in head order. */
    QList<GroupHead> aimMembers(Doc *doc, const QList<quint32> &fixtures);
    /** The members Aim look @p paletteId in @p scene lays out over: as
     *  aimHeads(doc, scene, paletteId), arms opened up. */
    QList<GroupHead> aimMembers(Doc *doc, const Scene *scene, quint32 paletteId);

    /** The member's aim geometry: the fixture's, from the arm's own origin
     *  (armOffset()). */
    bool frameFor(Doc *doc, const GroupHead &member, Frame &frame);

    /** Solve @p frame for @p targetWorld.
     *
     *  A yoke reaches most points two ways: pan toward the point and tilt
     *  forward, or pan half a turn further and tilt back past home. The first
     *  is what this has always answered, and it is kept whenever it is within
     *  range, so a head does not swing round mid-move. The second is taken only
     *  when the first is out of range and the second is not as far out --
     *  which is the case a clamp used to get silently wrong. */
    Solution solve(const Frame &frame, const QVector3D &targetWorld);

    /** The way back: a head's pan and tilt as DMX sends them (fixture
     *  degrees, CENTRED -- 0 is the middle of the range) turned into what they
     *  mean on the stage, with the calibration taken out.
     *
     *  Exactly the inverse of solve(), step for step and in reverse order: the
     *  mirrors first (invert flags, a standing head's flipped pan), then the
     *  calibration offsets, then the mounting. The offsets correct the
     *  PHYSICAL head -- DMX carries them so the real light lands where the
     *  design says -- so every view that turns DMX back into "where is it
     *  pointing" must remove them, or a head with a +3 degree pan offset aimed
     *  at a mark draws its beam 3 degrees off it. Pan zero, invert and
     *  standing are geometry, not calibration: applied both ways.
     *
     *  @p panRelDeg comes back as the bearing relative to the head's facing
     *  (FixtureRigProps::panZeroDir) and @p tiltRelDeg as the tilt away from
     *  its home (straight down hung, straight up standing), both in the same
     *  turning sense the views draw. */
    void intendedAngles(const ::FixtureRigProps &rp, double panCentredDeg, double tiltCentredDeg,
                        double &panRelDeg, double &tiltRelDeg);

    /** The same, as the world-space direction of the beam (unit vector;
     *  +Y downstage, +X stage left, +Z up). False only when it degenerates. */
    bool beamDirection(const ::FixtureRigProps &rp, double panCentredDeg, double tiltCentredDeg,
                       QVector3D &dir);

    /** Where head @p head of @p fx points, read back from @p values (its own
     *  channels, as Fixture::channelValues() has them): pan and tilt, coarse
     *  and fine, over each channel's pointing band (Fixture::positionFraction()),
     *  centred, through beamDirection(). The inverse of an Aim look's
     *  solve() + Fixture::positionToValues(), and the one every rig view draws
     *  a mover's beam with, so a test can check a solved aim lands where it
     *  should with the views' own arithmetic. @p panFrac / @p tiltFrac, when
     *  given, replace the read (a fraction of the DMX range, as
     *  MoveSpeed::Follower walks it). A head with no pan or tilt of its own
     *  (a shared yoke) reads the first head. False for a fixture with nothing
     *  to point. Allocation-free. */
    bool headDirection(const Fixture *fx, const QByteArray &values, int head,
                       const ::FixtureRigProps &rp, QVector3D &dir,
                       const double *panFrac = nullptr, const double *tiltFrac = nullptr);

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
    /** aimDegrees() and reach() for one member: an arm from its own origin. */
    bool aimDegrees(Doc *doc, const GroupHead &member, const QVector3D &targetWorld,
                    float &panDeg, float &tiltDeg);
    bool reach(Doc *doc, const GroupHead &member, const QVector3D &targetWorld,
               Solution &solution);

    /** Where head @p fixtureId should point for target @p target: the shared
     *  rule (MonitorProperties::targetAimPointFor()) with this head's place
     *  and its beam as wide as its zoom and iris have it now -- a wider beam
     *  following a person aims lower, so it covers them without top wash. */
    QVector3D aimPointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                          bool subjectMode, bool framed = false);
    /** The same for one member, from its own origin. @p framed: the follow
     *  spot frames its subject (Scene::followFramed()). */
    QVector3D aimPointFor(Doc *doc, const GroupHead &member, const StageTarget *target,
                          bool subjectMode, bool framed = false);

    /** The heads an Aim look in @p scene points, in LOOK ORDER: each of the
     *  scene's groups' members in the group's head order, then the scene's
     *  own fixtures; once each; only heads that can point (pan or tilt) and
     *  are placed on the plot. */
    QList<quint32> aimHeads(Doc *doc, const Scene *scene);

    /** The heads Aim look @p paletteId in @p scene lays out over: as
     *  above, but over the look's layout span when it is one formation with
     *  other scenes (a cue-level aim across layers), and less the heads the
     *  scene leaves out for it (Scene::layoutFixtures()). */
    QList<quint32> aimHeads(Doc *doc, const Scene *scene, quint32 paletteId);

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
    /** The same over members: every arm its own place, its own number in
     *  the order and its own group (Grouping 2 pairs the two arms of a
     *  two-arm beam when they hang side by side along the line). */
    QHash<GroupHead, double> lineMemberFractions(Doc *doc, const StageTarget *target,
                                           const QList<GroupHead> &members,
                                           const LineSpread::Settings &settings);

    /** @p heads in the order they take places on line @p target: as given
     *  (LookOrder), or by where they hang measured from the line's first end
     *  towards its last (AlongLine). */
    QList<quint32> orderedLineHeads(Doc *doc, const StageTarget *target,
                                    const QList<quint32> &heads, LineSpread::Order order);
    /** The same over members, each arm by where IT hangs. */
    QList<GroupHead> orderedLineMembers(Doc *doc, const StageTarget *target,
                                      const QList<GroupHead> &members, LineSpread::Order order);

    /** The world point @p fraction of the way along line target @p target. */
    QVector3D linePoint(Doc *doc, const StageTarget *target, double fraction);

    /** Where head @p fixtureId aims at @p fraction along line @p target: the
     *  point on the line, or -- for a head standing below the line that can't
     *  tilt down to it -- the subject height above it (the people on the
     *  line). The rule linePoints() and every rig view's aim trace share. */
    QVector3D linePointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                           double fraction);
    QVector3D linePointFor(Doc *doc, const GroupHead &member, const StageTarget *target,
                           double fraction);

    /** Each head's aim point for Aim look @p look on line @p target, over
     *  @p heads (as lineFractions()). */
    QHash<quint32, QVector3D> linePoints(Doc *doc, const QLCPalette *look,
                                         const StageTarget *target,
                                         const QList<quint32> &heads);
    /** The same over members (aimMembers()): what an Aim look writes. */
    QHash<GroupHead, QVector3D> lineMemberPoints(Doc *doc, const QLCPalette *look,
                                           const StageTarget *target,
                                           const QList<GroupHead> &members);

    /** True if @p scene carries a follow-spot effect, which is what makes an
     *  Aim look in it track a SUBJECT (chest height above the deck) rather
     *  than the target's own point. */
    bool sceneFollowsSubject(Doc *doc, const Scene *scene);

    /** Line of sight from head @p fixtureId to @p point: whether a solid
     *  feature of the stage stands between its beam origin and the point, and
     *  which one (BeamOcclusion). The head's own mount never blocks it. Clear
     *  when the head has no rig geometry. @p model, when given, is used
     *  instead of fetching the stage's current one -- a caller asking for
     *  many heads fetches it once. */
    BeamOcclusion::Hit lineOfSight(Doc *doc, quint32 fixtureId, const QVector3D &point,
                                   const BeamOcclusion::Model *model = nullptr);
    /** The same from a known beam origin. */
    BeamOcclusion::Hit lineOfSight(Doc *doc, const BeamOcclusion::Model &model, quint32 fixtureId,
                                   const QVector3D &from, const QVector3D &point);

    /** One head a scene's Aim look points, where, and whether it can. */
    struct SceneAim
    {
        quint32   fixtureId;
        int       head = -1;     ///< the arm, or -1: the fixture aims as one
        quint32   targetId;
        quint32   paletteId;     ///< the Aim look pointing it
        QVector3D point;         ///< the aim point, as the palette resolves it
        Solution  solution;
        /** Whether its beam gets there: the first thing on stage in the way. */
        BeamOcclusion::Hit sight;
        /** The Aim look dims it while it is blocked (QLCPalette::dimWhenBlocked). */
        bool dimsWhenBlocked = false;
    };

    /** Every aimable head each Aim look in @p scene points at its target --
     *  the scene's fixtures and its groups' members, exactly the set the
     *  palette writes pan and tilt for -- solved against the same point the
     *  palette uses. A multi-arm fixture has one entry per arm (SceneAim::head),
     *  each from the arm's own origin. Heads with no rig geometry are left out: there is no
     *  answer to give for them. */
    QList<SceneAim> sceneAims(Doc *doc, const Scene *scene);
    /** The heads an Aim look in @p scene that has "Dim lights that can't see
     *  the target" on points at something they cannot see, and what is in the
     *  way. Empty -- and cheap -- when no Aim look in it has that on. */
    QHash<quint32, BeamOcclusion::Hit> blockedDimmedHeads(Doc *doc, const Scene *scene);
    /** The same for the playback tick (Scene::write, every tick): only the
     *  blocked heads' fixture ids, once each, into @p into (cleared first).
     *  The member lists and mounts it works through are kept per thread and
     *  reused, so once they and @p into have grown to the rig's size a tick
     *  allocates nothing here. The same answer as blockedDimmedHeads()'s
     *  keys, in the order it first meets them. */
    void blockedDimmedFixtures(Doc *doc, const Scene *scene, QVector<quint32> &into);
}

/** @} */

#endif
