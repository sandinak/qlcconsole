/*
  Q Light Controller Plus
  aimsolver.cpp

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

#include <QtMath>
#include <QDebug>

#include "aimsolver.h"
#include "linecurve.h"
#include "righeads.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scene.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

bool AimSolver::floorMounted(const FixtureRigProps &rp)
{
    /* What it is BOLTED to decides, not what it is missing. This used to read
       "no truss, therefore standing on the deck", which is wrong for every
       fixture hung from something we do not model yet -- and, worse, the views
       never applied it, so a free-placed head was solved standing and drawn
       hanging. A head standing on a truss top is recorded as FloorMounted by
       the mount itself (that is what "Mounting=FloorMounted" on a truss
       means), and a head on the TOP face of a riser stands on it. */
    if (rp.mountingType == Truss::FloorMounted)
        return true;
    if (rp.riserPlatformId != UINT_MAX && rp.riserFace == 1)
        return true;
    // Deck mount means standing on the platform's top: there is nothing to
    // hang from. The rig props' TopHung default used to leak through here, so
    // of eight identical heads on one deck, the ones never explicitly set to
    // FloorMounted were solved (and drawn) upside down -- aimed "up" at a
    // target above them, past their tilt limit.
    if (rp.onDeck())
        return true;
    return false;
}

bool AimSolver::frameFor(Doc *doc, quint32 fixtureId, Frame &frame)
{
    if (doc == NULL)
        return false;
    Fixture *fixture = doc->fixture(fixtureId);
    MonitorProperties *mProps = doc->monitorProperties();
    if (fixture == NULL || fixture->fixtureMode() == NULL || mProps == NULL)
        return false;
    if (!mProps->containsFixture(fixtureId))
        return false;

    const FixtureRigProps rp = mProps->fixtureRigProps(fixtureId);
    frame.floorMounted = floorMounted(rp);

    // Fixture world position in metres; a free-placed fixture on a platform sits
    // at the deck height (mirror the target's platform handling).
    frame.position = mProps->fixtureRigPosition(fixtureId);
    // Only a free-placed head gets the height of whatever platform it stands
    // over: every mount (truss, pipe, tower, deck, riser) already puts the
    // head at its real height, and adding the platform again lifted a
    // deck-mounted head to twice the deck's height.
    if (rp.primaryMount() == FixtureRigProps::NoMount)
        frame.position.setZ(frame.position.z()
                            + mProps->platformHeightAt(frame.position.x(), frame.position.y()));

    frame.panZeroDir    = rp.panZeroDir;
    frame.panOffsetDeg  = rp.panOffsetDeg;
    frame.tiltOffsetDeg = rp.tiltOffsetDeg;
    frame.panInvert     = rp.panInvert;
    frame.tiltInvert    = rp.tiltInvert;

    const QLCPhysical phy = fixture->fixtureMode()->physical();
    frame.panMax  = float(phy.focusPanMax());  if (frame.panMax  == 0.0f) frame.panMax  = 360.0f;
    frame.tiltMax = float(phy.focusTiltMax()); if (frame.tiltMax == 0.0f) frame.tiltMax = 270.0f;
    return true;
}

AimSolver::Solution AimSolver::solve(const Frame &frame, const QVector3D &targetWorld)
{
    const float dx = targetWorld.x() - frame.position.x();
    const float dy = targetWorld.y() - frame.position.y();
    const float dz = targetWorld.z() - frame.position.z();
    const float horizDist = qSqrt(dx * dx + dy * dy);

    /* Horizontal bearing of the target, in the SAME terms as a head's pan zero:
       0 = downstage (+Y), 90 = stage right (-X), turning the way the "Facing
       (pan zero)" menu names them. It used to measure from upstage, which put
       every solved pan exactly 180 degrees out from the direction the plot and
       the rig view then drew the beam -- aim a head at a mark and its beam
       went to the mirror image of that mark through the head. Nothing else in
       the app used the old sense; MonitorFixtureItem, fixtureAimDirection and
       the facing menu all agree on this one. */
    float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(-dx), double(dy))));
    if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;

    float relativePan = azimuthDeg - frame.panZeroDir;
    while (relativePan >  180.0f) relativePan -= 360.0f;
    while (relativePan < -180.0f) relativePan += 360.0f;

    float panRaw = frame.panMax / 2.0f + relativePan + frame.panOffsetDeg;
    // Floor mount = hung fixture flipped 180° about horizontal → pan sense
    // reverses (and tilt negates). The panInvert flag toggles from there.
    bool invertPan = frame.panInvert;
    if (frame.floorMounted)
        invertPan = !invertPan;
    if (invertPan) panRaw = frame.panMax - panRaw;

    const float elevDeg = float(qRadiansToDegrees(qAtan2(double(dz), double(horizDist))));

    float tiltRaw;
    if (frame.floorMounted)
        // Floor: tilt centre = straight up; tilt down toward the target =
        // centre - (90 - elev).
        tiltRaw = frame.tiltMax / 2.0f - 90.0f + elevDeg + frame.tiltOffsetDeg;
    else
        // TopHung/SideArm: tilt centre = straight down; deviation = 90 + elev.
        tiltRaw = frame.tiltMax / 2.0f + (90.0f + elevDeg) + frame.tiltOffsetDeg;
    if (frame.tiltInvert) tiltRaw = frame.tiltMax - tiltRaw;

    /* Every way of pointing at the same place: the same pan a whole turn
       either side, or half a turn round with the tilt mirrored through home.
       Invert maps x to max - x on both axes, which commutes with all of these,
       so they can be taken after it. */
    auto marginOf = [&](float pan, float tilt) {
        return qMin(qMin(pan, frame.panMax - pan), qMin(tilt, frame.tiltMax - tilt));
    };
    float bestPan = panRaw, bestTilt = tiltRaw;
    float best = marginOf(panRaw, tiltRaw);
    if (best < 0.0f)
    {
        for (int k = -2; k <= 2; ++k)
        {
            if (k == 0)
                continue;
            const float pan = panRaw + 180.0f * float(k);
            const float tilt = (k % 2 != 0) ? frame.tiltMax - tiltRaw : tiltRaw;
            /* A real improvement only. Mirroring leaves the tilt shortfall
               exactly as it was, so when tilt is what is out of range the two
               tie -- and a tie decided by rounding swung an unreachable head
               half a turn for nothing. */
            const float m = marginOf(pan, tilt);
            if (m > best + 0.01f)
            {
                best = m;
                bestPan = pan;
                bestTilt = tilt;
            }
        }
    }

    Solution out;
    out.panDeg = qBound(0.0f, bestPan, frame.panMax);
    out.tiltDeg = qBound(0.0f, bestTilt, frame.tiltMax);
    out.marginDeg = best;
    return out;
}

bool AimSolver::reach(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                      Solution &solution)
{
    Frame frame;
    if (frameFor(doc, fixtureId, frame) == false)
        return false;
    solution = solve(frame, targetWorld);
    return true;
}

bool AimSolver::aimDegrees(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                           float &panDeg, float &tiltDeg)
{
    Solution s;
    if (reach(doc, fixtureId, targetWorld, s) == false)
        return false;
    panDeg = s.panDeg;
    tiltDeg = s.tiltDeg;
    return true;
}

QVector3D AimSolver::aimPointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                                 bool subjectMode)
{
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || target == nullptr)
        return QVector3D();
    Frame frame;
    Fixture *fx = doc->fixture(fixtureId);
    if (fx == nullptr || !frameFor(doc, fixtureId, frame))
        return mProps->targetAimPoint(target, subjectMode);
    return mProps->targetAimPointFor(target, subjectMode, frame.position,
                                     RigHeads::beamAngle(fx, fx->channelValues()));
}

bool AimSolver::sceneFollowsSubject(Doc *doc, const Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return false;

    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Effect &&
            p->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QList<quint32> AimSolver::aimable(Doc *doc, const QList<quint32> &fixtures)
{
    QList<quint32> heads;
    if (doc == NULL)
        return heads;
    MonitorProperties *mProps = doc->monitorProperties();
    foreach (quint32 fid, fixtures)
    {
        if (heads.contains(fid))
            continue;
        Fixture *fxi = doc->fixture(fid);
        if (fxi == NULL || (mProps != NULL && !mProps->containsFixture(fid)))
            continue;
        if (fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
            && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
            continue;               // nothing to point
        heads << fid;
    }
    return heads;
}

QList<quint32> AimSolver::aimHeads(Doc *doc, const Scene *scene, quint32 paletteId)
{
    if (doc == NULL || scene == NULL)
        return QList<quint32>();
    if (paletteId == UINT_MAX)
        return aimHeads(doc, scene);
    /* What the look lays out over: a formation's span when it is one with
       other scenes, less the heads a lower layer aims itself. */
    return aimable(doc, scene->layoutFixtures(paletteId));
}

QList<quint32> AimSolver::aimHeads(Doc *doc, const Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return QList<quint32>();

    QList<quint32> all;
    foreach (quint32 gid, scene->fixtureGroups())
    {
        FixtureGroup *fg = doc->fixtureGroup(gid);
        if (fg == NULL)
            continue;
        foreach (quint32 fid, fg->fixtureList())
            all << fid;
    }
    all << scene->fixtures();
    return aimable(doc, all);
}

QList<quint32> AimSolver::orderedLineHeads(Doc *doc, const StageTarget *target,
                                          const QList<quint32> &heads, LineSpread::Order order)
{
    if (doc == NULL || target == NULL || order != LineSpread::AlongLine)
        return heads;
    /* By where the heads hang, measured from the line's first end towards
       its last: the stage-left head takes the stage-left end, so a full
       spread never crosses beams by accident. */
    const QVector<QVector3D> pts = target->linePoints();
    if (pts.size() < 2)
        return heads;
    QVector<QVector3D> where;
    foreach (quint32 fid, heads)
    {
        Frame f;
        where << (frameFor(doc, fid, f) ? f.position : QVector3D());
    }
    QList<quint32> ordered;
    foreach (int i, LineSpread::orderAlong(where, pts.first(), pts.last()))
        ordered << heads.at(i);
    return ordered;
}

QHash<quint32, double> AimSolver::lineFractions(Doc *doc, const StageTarget *target,
                                                const QList<quint32> &heads,
                                                const LineSpread::Settings &settings)
{
    QHash<quint32, double> out;
    if (doc == NULL || target == NULL || !target->isLine() || heads.isEmpty())
        return out;
    const QList<quint32> ordered = orderedLineHeads(doc, target, heads, settings.order);
    const QVector<double> fr = LineSpread::fractions(ordered.size(), settings);
    for (int i = 0; i < ordered.size(); ++i)
        out.insert(ordered.at(i), fr.at(i));
    return out;
}

QVector3D AimSolver::linePoint(Doc *doc, const StageTarget *target, double fraction)
{
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || target == NULL)
        return QVector3D();
    return LineCurve(mProps->targetLinePoints(target)).pointAt(fraction);
}

QHash<quint32, QVector3D> AimSolver::linePoints(Doc *doc, const QLCPalette *look,
                                                const StageTarget *target,
                                                const QList<quint32> &heads)
{
    QHash<quint32, QVector3D> out;
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || look == NULL || target == NULL || !target->isLine())
        return out;
    const LineSpread::Settings settings = LineSpread::fromParams(look->effectParamValues());
    const QHash<quint32, double> fr = lineFractions(doc, target, heads, settings);
    const LineCurve curve(mProps->targetLinePoints(target));
    for (auto it = fr.constBegin(); it != fr.constEnd(); ++it)
        out.insert(it.key(), curve.pointAt(it.value()));
    return out;
}

BeamOcclusion::Hit AimSolver::lineOfSight(Doc *doc, const BeamOcclusion::Model &model,
                                          quint32 fixtureId, const QVector3D &from,
                                          const QVector3D &point)
{
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || model.isEmpty())
    {
        BeamOcclusion::Hit clear;
        clear.clearance = BeamOcclusion::clearanceReach();
        clear.distance = (point - from).length();
        clear.point = point;
        return clear;
    }
    return model.segment(from, point, BeamOcclusion::ownMounts(mProps, fixtureId));
}

BeamOcclusion::Hit AimSolver::lineOfSight(Doc *doc, quint32 fixtureId, const QVector3D &point,
                                          const BeamOcclusion::Model *model)
{
    Frame frame;
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || !frameFor(doc, fixtureId, frame))
    {
        BeamOcclusion::Hit clear;
        clear.clearance = BeamOcclusion::clearanceReach();
        clear.point = point;
        return clear;
    }
    if (model != nullptr)
        return lineOfSight(doc, *model, fixtureId, frame.position, point);
    const BeamOcclusion::ModelPtr current = mProps->occlusionModel(doc);
    return lineOfSight(doc, *current, fixtureId, frame.position, point);
}

static QList<AimSolver::SceneAim> collectAims(Doc *doc, const Scene *scene, bool dimmingOnly)
{
    QList<AimSolver::SceneAim> out;
    if (doc == NULL || scene == NULL)
        return out;
    MonitorProperties *mProps = doc->monitorProperties();
    if (mProps == NULL)
        return out;

    const bool subject = AimSolver::sceneFollowsSubject(doc, scene);
    BeamOcclusion::ModelPtr sightModel;
    foreach (quint32 pid, scene->runtimePalettes())   // a tried-on Aim draws too
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == NULL || pal->type() != QLCPalette::Aim)
            continue;
        if (dimmingOnly && !pal->dimWhenBlocked())
            continue;
        if (sightModel.isNull())
            sightModel = mProps->occlusionModel(doc);
        StageTarget *tgt = mProps->stageTarget(pal->stageTargetId());
        if (tgt == NULL)
            continue;
        // The heads it lays out over (a formation across several scenes:
        // all of them) and the ones this scene writes (the scene's groups'
        // members and its fixtures, less any it leaves out), once each.
        const QList<quint32> heads = AimSolver::aimHeads(doc, scene, pid);
        const QList<quint32> written = AimSolver::aimable(doc, scene->writtenFixtures(pid));
        // A line gives each head its own point on it.
        const QHash<quint32, QVector3D> onLine = AimSolver::linePoints(doc, pal, tgt, heads);
        foreach (quint32 fid, written)
        {
            AimSolver::Frame frame;
            if (!AimSolver::frameFor(doc, fid, frame))
                continue;
            // A line target: the head's own point on the line is what it
            // aims at, so that is the point whose sight is checked.
            const QVector3D point = tgt->isLine()
                    ? onLine.value(fid, mProps->targetAimPoint(tgt))
                    : AimSolver::aimPointFor(doc, fid, tgt, subject);
            AimSolver::SceneAim aim;
            aim.fixtureId = fid;
            aim.targetId = tgt->id();
            aim.paletteId = pal->id();
            aim.point = point;
            aim.solution = AimSolver::solve(frame, point);
            aim.sight = AimSolver::lineOfSight(doc, *sightModel, fid, frame.position, point);
            aim.dimsWhenBlocked = pal->dimWhenBlocked();
            out << aim;
        }
    }
    return out;
}

QList<AimSolver::SceneAim> AimSolver::sceneAims(Doc *doc, const Scene *scene)
{
    return collectAims(doc, scene, false);
}

QHash<quint32, BeamOcclusion::Hit> AimSolver::blockedDimmedHeads(Doc *doc, const Scene *scene)
{
    QHash<quint32, BeamOcclusion::Hit> out;
    if (doc == NULL || scene == NULL)
        return out;
    // The cheap question first: does any Aim look here ask for it at all?
    bool asked = false;
    foreach (quint32 pid, scene->runtimePalettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal != NULL && pal->type() == QLCPalette::Aim && pal->dimWhenBlocked())
        {
            asked = true;
            break;
        }
    }
    if (!asked)
        return out;
    foreach (const SceneAim &a, collectAims(doc, scene, true))
        if (a.sight.blocked && !out.contains(a.fixtureId))
            out.insert(a.fixtureId, a.sight);
    return out;
}
