/*
  Q Light Controller Plus
  colourmix.cpp

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

#include <QColor>
#include <climits>

#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "colourmix.h"

QVector<ColourMix::Wheel> ColourMix::wheels(const QLCFixtureMode *mode)
{
    QVector<Wheel> out;
    if (mode == nullptr)
        return out;
    const QVector<QLCChannel *> chans = mode->channels();
    for (int c = 0; c < chans.size(); ++c)
    {
        const QLCChannel *ch = chans.at(c);
        if (ch == nullptr || ch->group() != QLCChannel::Colour)
            continue;
        Wheel w;
        w.channel = quint32(c);
        for (const QLCCapability *cap : ch->capabilities())
        {
            QColor col = cap->resource(0).value<QColor>();
            if (!col.isValid())
                col = QColor(cap->name().trimmed());
            if (!col.isValid())
                continue;
            Slot s;
            s.r = col.red();
            s.g = col.green();
            s.b = col.blue();
            s.dmx = cap->middle();
            w.slots << s;
        }
        if (!w.slots.isEmpty())
            out << w;
    }
    return out;
}

int ColourMix::nearest(const Wheel &wheel, int r, int g, int b)
{
    int best = -1;
    int bestDist = INT_MAX;
    for (int i = 0; i < wheel.slots.size(); ++i)
    {
        const Slot &s = wheel.slots.at(i);
        const int dr = s.r - r, dg = s.g - g, db = s.b - b;
        const int d = dr * dr + dg * dg + db * db;
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

bool ColourMix::hasCMY(const QLCFixtureMode *mode)
{
    if (mode == nullptr)
        return false;
    bool c = false, m = false, y = false;
    for (const QLCChannel *ch : mode->channels())
    {
        if (ch == nullptr)
            continue;
        if (ch->group() != QLCChannel::Intensity && ch->group() != QLCChannel::Colour)
            continue;
        switch (ch->colour())
        {
            case QLCChannel::Cyan:    c = true; break;
            case QLCChannel::Magenta: m = true; break;
            case QLCChannel::Yellow:  y = true; break;
            default: break;
        }
    }
    return c && m && y;
}

bool ColourMix::showsRgbWithoutRgb(const QLCFixtureMode *mode)
{
    if (hasCMY(mode))
        return true;
    for (const Wheel &w : wheels(mode))
        if (w.slots.size() >= 2)
            return true;
    return false;
}
