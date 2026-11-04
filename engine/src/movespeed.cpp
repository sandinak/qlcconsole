/*
  Q Light Controller Plus - qlcconsole
  movespeed.cpp

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
#include <algorithm>

#include "movespeed.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "scenevalue.h"
#include "qlcchannel.h"
#include "fixture.h"

namespace MoveSpeed
{

double secondsPer180(const Fixture *fx)
{
    const QLCFixtureDef *def = fx != nullptr ? fx->fixtureDef() : nullptr;
    if (def != nullptr && def->type() == QLCFixtureDef::Scanner)
        return 0.1;
    return 0.4;
}

double degreesPerSecond(const Fixture *fx)
{
    return 180.0 / secondsPer180(fx);
}

double panSpanDeg(const Fixture *fx)
{
    const double max = fx != nullptr ? fx->effectiveBeam().panMax : 0.0;
    return max > 0.0 ? max : 360.0;
}

double tiltSpanDeg(const Fixture *fx)
{
    const double max = fx != nullptr ? fx->effectiveBeam().tiltMax : 0.0;
    return max > 0.0 ? max : 270.0;
}

/* The axis's pointing band (Fixture::positionBand()): a head that points with
   DMX 0-127 and spins above that covers its whole degree range in that band,
   not in the whole channel. Fractions here are of the DMX range (value / 255).
   Allocation-free: the follower steps this every frame. */
static const QLCChannel *axisChannel(const Fixture *fx, QLCChannel::Group group)
{
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    const quint32 ch = fx != nullptr ? fx->channelNumber(group, QLCChannel::MSB) : QLCChannel::invalid();
    return (mode != nullptr && ch != QLCChannel::invalid()) ? mode->channel(ch) : nullptr;
}

/** How far along the band (0..1) a DMX fraction is; the fraction itself when
 *  the axis has no band (and no channel). */
static double bandPos(const QLCChannel *ch, double dmxFraction)
{
    return ch != nullptr ? Fixture::positionFraction(ch, dmxFraction * 255.0) : dmxFraction;
}

/** The share of the DMX range the pointing band covers (1 when it is all). */
static double bandWidth(const QLCChannel *ch)
{
    uchar lo = 0, hi = 255;
    if (ch != nullptr)
        Fixture::positionBand(ch, lo, hi);
    return hi > lo ? double(hi - lo) / 255.0 : 1.0;
}

double travelMs(const Fixture *fx, double panFrom, double tiltFrom, double panTo, double tiltTo)
{
    if (fx == nullptr)
        return 0.0;
    const QLCChannel *panCh = axisChannel(fx, QLCChannel::Pan);
    const QLCChannel *tiltCh = axisChannel(fx, QLCChannel::Tilt);
    const double panDeg = qAbs(bandPos(panCh, panTo) - bandPos(panCh, panFrom)) * panSpanDeg(fx);
    const double tiltDeg = qAbs(bandPos(tiltCh, tiltTo) - bandPos(tiltCh, tiltFrom)) * tiltSpanDeg(fx);
    return 1000.0 * qMax(panDeg, tiltDeg) / degreesPerSecond(fx);
}

void positionOf(const Fixture *fx, const QList<SceneValue> &values,
                double &pan, double &tilt, bool &hasPan, bool &hasTilt)
{
    hasPan = hasTilt = false;
    pan = tilt = 0.0;
    if (fx == nullptr)
        return;
    const quint32 panCh = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 tiltCh = fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    for (const SceneValue &sv : values)
    {
        if (sv.fxi != fx->id())
            continue;
        if (panCh != QLCChannel::invalid() && sv.channel == panCh)
        {
            pan = sv.value / 255.0;
            hasPan = true;
        }
        else if (tiltCh != QLCChannel::invalid() && sv.channel == tiltCh)
        {
            tilt = sv.value / 255.0;
            hasTilt = true;
        }
    }
}

bool Follower::atRest(double panTo, double tiltTo) const
{
    return valid && qFuzzyCompare(pan + 1.0, panTo + 1.0) && qFuzzyCompare(tilt + 1.0, tiltTo + 1.0);
}

bool Follower::step(const Fixture *fx, double panTo, double tiltTo, qint64 nowMs, qint64 snapAfterMs)
{
    const qint64 dt = nowMs - stamp;
    if (!valid || dt > snapAfterMs || fx == nullptr)
    {
        pan = panTo;
        tilt = tiltTo;
        stamp = nowMs;
        valid = true;
        return false;
    }
    if (dt <= 0)
        return !atRest(panTo, tiltTo);
    stamp = nowMs;
    const double deg = degreesPerSecond(fx) * double(dt) / 1000.0;
    auto approach = [](double &at, double to, double maxFrac) {
        const double d = to - at;
        at = qAbs(d) <= maxFrac ? to : at + (d > 0 ? maxFrac : -maxFrac);
    };
    // A degree is a smaller share of the DMX range on a banded axis.
    approach(pan, panTo, deg / panSpanDeg(fx) * bandWidth(axisChannel(fx, QLCChannel::Pan)));
    approach(tilt, tiltTo, deg / tiltSpanDeg(fx) * bandWidth(axisChannel(fx, QLCChannel::Tilt)));
    return !atRest(panTo, tiltTo);
}

}
