/*
  Q Light Controller Plus
  followspotoperator.cpp

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
#include <cmath>

#include "followspotoperator.h"
#include "monitorproperties.h"

namespace
{
/* How far off the stage each one stands, metres: a tower just off the
   wing, an upstage bridge just behind, a booth out in the house and the
   back of the house twice as far. */
const double kOffStage = 1.0;
const double kFrontOfHouse = 12.0;
const double kBackOfHouse = 24.0;
}

FollowSpotOperator FollowSpotOperator::fromPreset(Preset p)
{
    FollowSpotOperator op;
    op.preset = p;
    op.facing = presetFacing(p);
    op.eyeMm = presetEyeMm(p);
    return op;
}

int FollowSpotOperator::presetFacing(Preset p)
{
    switch (p)
    {
        case StageLeftTower:    return 270;     // looking toward stage right
        case StageRightTower:   return 90;      // toward stage left
        case Upstage:           return 180;     // downstage, at the house
        case BackOfHouse:
        case FrontOfHouse:
        default:                return 0;       // upstage
    }
}

int FollowSpotOperator::presetEyeMm(Preset p)
{
    switch (p)
    {
        case StageLeftTower:
        case StageRightTower:   return 4000;    // a tower's platform
        case Upstage:           return 3000;
        case BackOfHouse:       return 6000;    // a balcony rail
        case FrontOfHouse:
        default:                return kDefaultEyeMm;
    }
}

QString FollowSpotOperator::presetName(Preset p)
{
    switch (p)
    {
        case StageLeftTower:    return tr("SL tower");
        case StageRightTower:   return tr("SR tower");
        case Upstage:           return tr("Upstage");
        case BackOfHouse:       return tr("Back of house");
        case FrontOfHouse:
        default:                return tr("Front of house");
    }
}

QString FollowSpotOperator::presetKey(Preset p)
{
    switch (p)
    {
        case StageLeftTower:    return QStringLiteral("SL");
        case StageRightTower:   return QStringLiteral("SR");
        case Upstage:           return QStringLiteral("US");
        case BackOfHouse:       return QStringLiteral("BOH");
        case FrontOfHouse:
        default:                return QStringLiteral("FOH");
    }
}

FollowSpotOperator::Preset FollowSpotOperator::presetFromKey(const QString &key)
{
    for (Preset p : presets())
        if (presetKey(p).compare(key, Qt::CaseInsensitive) == 0)
            return p;
    return FrontOfHouse;
}

QList<FollowSpotOperator::Preset> FollowSpotOperator::presets()
{
    return { FrontOfHouse, StageLeftTower, StageRightTower, Upstage, BackOfHouse };
}

int FollowSpotOperator::snapFacing(double degrees)
{
    if (!std::isfinite(degrees))
        return 0;
    const int steps = int(std::lround(degrees / kStep));
    return ((steps * kStep) % 360 + 360) % 360;
}

QString FollowSpotOperator::facingName(int facing)
{
    switch (snapFacing(facing))
    {
        case 0:     return tr("0° (upstage)");
        case 90:    return tr("90° (toward stage left)");
        case 180:   return tr("180° (downstage)");
        case 270:   return tr("270° (toward stage right)");
        default:    return QStringLiteral("%1°").arg(snapFacing(facing));
    }
}

QString FollowSpotOperator::viewpointText() const
{
    if (isFrontOfHouse())
        return QString();
    if (facing == presetFacing(preset))
        return tr("from %1").arg(presetName(preset));
    return tr("from %1, facing %2°").arg(presetName(preset)).arg(facing);
}

QPointF FollowSpotOperator::position(Preset p, const MonitorProperties *props)
{
    // Centre stage and the grid's edges (world 0,0 is the grid's corner).
    const QPointF c = props != nullptr ? QPointF(props->stageCentre().x(), props->stageCentre().y())
                                       : QPointF();
    const double w = 2.0 * c.x(), d = 2.0 * c.y();
    switch (p)
    {
        case StageLeftTower:    return QPointF(w + kOffStage, c.y());
        case StageRightTower:   return QPointF(-kOffStage, c.y());
        case Upstage:           return QPointF(c.x(), -kOffStage);
        case BackOfHouse:       return QPointF(c.x(), d + kBackOfHouse);
        case FrontOfHouse:
        default:                return QPointF(c.x(), d + kFrontOfHouse);
    }
}

int FollowSpotOperator::facingFromPush(const QPointF &from, const QPointF &centre, double sx, double sy)
{
    if (std::hypot(sx, sy) < 0.5)
        return -1;
    const QPointF to = centre - from;
    if (std::hypot(to.x(), to.y()) < 1e-6)
        return -1;
    // The bearing to centre (clockwise from upstage), less the push's own
    // angle (clockwise from straight up): what is straight ahead.
    const double bearing = std::atan2(to.x(), -to.y()) * 180.0 / M_PI;
    const double push = std::atan2(sx, sy) * 180.0 / M_PI;
    return snapFacing(bearing - push);
}

QPointF FollowSpotOperator::onFloor(double sx, double sy, int facing)
{
    const double r = snapFacing(facing) * M_PI / 180.0;
    return onFloor(sx, sy, std::cos(r), std::sin(r));
}

double FollowSpotOperator::viewAzimuth(int facing)
{
    // The view looks along (-sin A, -cos A) on the plan; facing looks along (sin F, -cos F).
    double a = -double(snapFacing(facing));
    while (a <= -180.0)
        a += 360.0;
    return a;
}

double FollowSpotOperator::viewElevation(const MonitorProperties *props) const
{
    const QPointF c = props != nullptr ? QPointF(props->stageCentre().x(), props->stageCentre().y())
                                       : QPointF();
    const QPointF at = position(props);
    const double run = std::hypot(c.x() - at.x(), c.y() - at.y());
    const double el = std::atan2(eyeMm / 1000.0, qMax(run, 0.1)) * 180.0 / M_PI;
    return qBound(5.0, el, 85.0);
}

int FollowSpotOperator::planTurns(int facing)
{
    // Facing 270 (from the SL tower): stage left -- the plan's right -- turned to the bottom, one turn.
    return int(std::lround((360 - snapFacing(facing)) / 90.0)) % 4;
}
