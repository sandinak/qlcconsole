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
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

bool AimSolver::aimDegrees(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                           float &panDeg, float &tiltDeg)
{
    if (doc == NULL)
        return false;
    Fixture *fixture = doc->fixture(fixtureId);
    MonitorProperties *mProps = doc->monitorProperties();
    if (fixture == NULL || fixture->fixtureMode() == NULL || mProps == NULL)
        return false;
    if (!mProps->containsFixture(fixtureId))
        return false;

    FixtureRigProps rp = mProps->fixtureRigProps(fixtureId);
    // A free-placed fixture stands on the floor/deck → floor-mounted (the
    // rig-props default is TopHung, which leaks in when an entry is auto-created).
    if (rp.trussId == Truss::invalidId())
        rp.mountingType = Truss::FloorMounted;

    // Fixture world position in metres; a free-placed fixture on a platform sits
    // at the deck height (mirror the target's platform handling).
    QVector3D fixPos = mProps->fixtureRigPosition(fixtureId);
    if (rp.trussId == Truss::invalidId())
        fixPos.setZ(fixPos.z() + mProps->platformHeightAt(fixPos.x(), fixPos.y()));

    const float dx = targetWorld.x() - fixPos.x();
    const float dy = targetWorld.y() - fixPos.y();
    const float dz = targetWorld.z() - fixPos.z();
    const float horizDist = qSqrt(dx * dx + dy * dy);

    // Horizontal azimuth: clockwise degrees from downstage (Y+ = upstage).
    float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(dx), double(-dy))));
    if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;

    float relativePan = azimuthDeg - rp.panZeroDir;
    while (relativePan >  180.0f) relativePan -= 360.0f;
    while (relativePan < -180.0f) relativePan += 360.0f;

    const QLCPhysical phy = fixture->fixtureMode()->physical();
    float panMax  = float(phy.focusPanMax());  if (panMax  == 0.0f) panMax  = 360.0f;
    float tiltMax = float(phy.focusTiltMax()); if (tiltMax == 0.0f) tiltMax = 270.0f;

    float panRaw = panMax / 2.0f + relativePan + rp.panOffsetDeg;
    // Floor mount = hung fixture flipped 180° about horizontal → pan sense
    // reverses (and tilt negates). The panInvert flag toggles from there.
    bool invertPan = rp.panInvert;
    if (rp.mountingType == Truss::FloorMounted)
        invertPan = !invertPan;
    if (invertPan) panRaw = panMax - panRaw;
    panDeg = qBound(0.0f, panRaw, panMax);

    const float elevDeg = float(qRadiansToDegrees(qAtan2(double(dz), double(horizDist))));

    if (rp.mountingType == Truss::FloorMounted)
        // Floor: tilt centre = straight up; tilt down toward the target =
        // centre - (90 - elev).
        tiltDeg = tiltMax / 2.0f - 90.0f + elevDeg + rp.tiltOffsetDeg;
    else
        // TopHung/SideArm: tilt centre = straight down; deviation = 90 + elev.
        tiltDeg = tiltMax / 2.0f + (90.0f + elevDeg) + rp.tiltOffsetDeg;
    if (rp.tiltInvert) tiltDeg = tiltMax - tiltDeg;
    tiltDeg = qBound(0.0f, tiltDeg, tiltMax);

    return true;
}
