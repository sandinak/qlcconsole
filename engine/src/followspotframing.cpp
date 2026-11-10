/*
  Q Light Controller Plus
  followspotframing.cpp

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

#include <QCoreApplication>
#include <QtMath>
#include <cmath>

#include "followspotframing.h"

QString FollowSpotFraming::framingKey() { return QStringLiteral("framing"); }
QString FollowSpotFraming::scaleKey() { return QStringLiteral("framingSize"); }
QString FollowSpotFraming::widthKey() { return QStringLiteral("sizeOnSubject"); }
QString FollowSpotFraming::beamKey() { return QStringLiteral("beamAngle"); }

double FollowSpotFraming::span(int framing)
{
    // From just over the head (5% headroom) down to: mid chest, the
    // waist, the floor.
    switch (framing)
    {
    case HeadShoulders: return 0.40;
    case WaistUp:       return 0.60;
    case FullBody:      return 1.10;
    default:            return 0.0;
    }
}

double FollowSpotFraming::level(double level)
{
    // Not known (negative, NaN): straight on. Straight down (0): steepest.
    if (!(level >= 0.0))
        return 1.0;
    return qBound(0.25, level, 1.0);
}

double FollowSpotFraming::widthM(const Look &l, const Pad &p, double heightM, double lvl)
{
    if (l.framing != Custom)
    {
        const double h = heightM > 0.0 ? heightM : 1.75;
        const double w = qMax(kLeast * h, span(l.framing) * h * level(lvl));
        return w * l.scale * p.scale;
    }
    const double cm = p.widthCm > 0.0 ? p.widthCm : l.widthCm;
    return cm > 0.0 ? cm / 100.0 : 0.0;
}

double FollowSpotFraming::angleFor(double widthM, double distM)
{
    if (widthM <= 0.0 || distM <= 0.05)
        return 0.0;
    return 2.0 * qRadiansToDegrees(std::atan(widthM / 2.0 / distM));
}

QString FollowSpotFraming::framingName(int framing)
{
    switch (framing)
    {
    case HeadShoulders: return QCoreApplication::translate("FollowSpotFraming", "Head & Shoulders");
    case WaistUp:       return QCoreApplication::translate("FollowSpotFraming", "Waist Up");
    case FullBody:      return QCoreApplication::translate("FollowSpotFraming", "Full Body");
    default:            return QString();
    }
}
