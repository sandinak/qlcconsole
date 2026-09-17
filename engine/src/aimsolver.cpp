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
    // A free-placed fixture stands on the floor/deck → floor-mounted (the
    // rig-props default is TopHung, which leaks in when an entry is auto-created).
    frame.floorMounted = (rp.trussId == Truss::invalidId())
                         || rp.mountingType == Truss::FloorMounted;

    // Fixture world position in metres; a free-placed fixture on a platform sits
    // at the deck height (mirror the target's platform handling).
    frame.position = mProps->fixtureRigPosition(fixtureId);
    if (rp.trussId == Truss::invalidId())
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

    // Horizontal azimuth: clockwise degrees from downstage (Y+ = upstage).
    float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(dx), double(-dy))));
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

QList<AimSolver::SceneAim> AimSolver::sceneAims(Doc *doc, const Scene *scene)
{
    QList<SceneAim> out;
    if (doc == NULL || scene == NULL)
        return out;
    MonitorProperties *mProps = doc->monitorProperties();
    if (mProps == NULL)
        return out;

    // The heads: the scene's fixtures and its groups' members, once each.
    QList<quint32> heads;
    foreach (quint32 fid, scene->fixtures())
        if (heads.contains(fid) == false)
            heads << fid;
    foreach (quint32 gid, scene->fixtureGroups())
    {
        FixtureGroup *fg = doc->fixtureGroup(gid);
        if (fg == NULL)
            continue;
        foreach (quint32 fid, fg->fixtureList())
            if (heads.contains(fid) == false)
                heads << fid;
    }

    const bool subject = sceneFollowsSubject(doc, scene);
    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == NULL || pal->type() != QLCPalette::Aim)
            continue;
        StageTarget *tgt = mProps->stageTarget(pal->stageTargetId());
        if (tgt == NULL)
            continue;
        const QVector3D point = mProps->targetAimPoint(tgt, subject);

        foreach (quint32 fid, heads)
        {
            Fixture *fxi = doc->fixture(fid);
            if (fxi == NULL)
                continue;
            if (fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
                && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
                continue;               // nothing to point
            SceneAim aim;
            aim.fixtureId = fid;
            aim.targetId = tgt->id();
            aim.point = point;
            if (reach(doc, fid, point, aim.solution))
                out << aim;
        }
    }
    return out;
}
