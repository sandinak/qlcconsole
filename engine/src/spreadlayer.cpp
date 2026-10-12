/*
  Q Light Controller Plus
  spreadlayer.cpp

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

#include <algorithm>
#include <cmath>

#include "spreadlayer.h"

SpreadLayer::Settings SpreadLayer::fromParams(const QMap<QString, double> &params)
{
    Settings s;
    s.amount = params.value(paramName("Amount"), 0.0);
    s.axis = Axis(qBound(0, int(params.value(paramName("Axis"), 0)), int(AroundTarget)));
    s.direction = Direction(qBound(0, int(params.value(paramName("Direction"), 0)), int(EndsIn)));
    s.wings = qMax(1, int(params.value(paramName("Wings"), 1)));
    s.blocks = qMax(1, int(params.value(paramName("Blocks"), 1)));
    return s;
}

QVector<double> SpreadLayer::phases(const QVector<Point> &points, const Settings &s,
                                    const QVector3D *centre)
{
    QVector<double> out(points.size(), 0.0);
    if (!s.isActive() || points.isEmpty())
        return out;

    /* No target: centre stage. (The middle of the heads was the first
       idea, but a truss of heads has its middle ON the line -- round it,
       every head is just left or right of it.) */
    const QVector3D c = centre != nullptr ? *centre : QVector3D();

    /* Where each head is along the axis. Heads at the same place (one
       column, one bar's heads seen from the side) are one step. */
    auto key = [&s, &c](const Point &p) -> double {
        switch (s.axis)
        {
            case Columns: return p.col;
            case Rows:    return p.row;
            // Millimetres, rounded: a few mm apart is the same place.
            case StageX:  return std::round(p.pos.x() * 1000.0);
            case StageY:  return std::round(p.pos.y() * 1000.0);
            case StageZ:  return std::round(p.pos.z() * 1000.0);
            // On the floor plan: centimetres out, tenths of a degree round.
            case FromTarget:
                return std::round(std::hypot(p.pos.x() - c.x(), p.pos.y() - c.y()) * 100.0);
            case AroundTarget:
            {
                // 0 straight downstage of it (-y), on round towards +x.
                double deg = std::atan2(p.pos.x() - c.x(), -(p.pos.y() - c.y())) * 180.0 / M_PI;
                if (deg < 0)
                    deg += 360.0;
                return std::round(deg * 10.0);
            }
            case Order:
            default:      return p.order;
        }
    };
    QVector<double> keys;
    keys.reserve(points.size());
    for (const Point &p : points)
        keys << key(p);
    QVector<double> steps = keys;
    std::sort(steps.begin(), steps.end());
    steps.erase(std::unique(steps.begin(), steps.end()), steps.end());

    int n = steps.size();
    const int blocks = qMin(s.blocks, n);
    const int nBlocks = (n + blocks - 1) / blocks;
    const int wings = qBound(1, s.wings, nBlocks);
    const int perWing = (nBlocks + wings - 1) / wings;

    for (int i = 0; i < points.size(); ++i)
    {
        const int step = int(std::lower_bound(steps.begin(), steps.end(), keys.at(i)) - steps.begin());
        int r = step / blocks;                  // the block it's in
        int m = nBlocks;
        if (wings > 1)
        {
            const int wing = r / perWing;
            r = r % perWing;
            if (wing % 2 == 1)                  // every other wing runs back
                r = perWing - 1 - r;
            m = perWing;
        }

        double t = 0;                           // 0..1 along it
        switch (s.direction)
        {
            case Reverse:
                t = m > 0 ? double(m - 1 - r) / m : 0;
            break;
            case CentreOut:
            case EndsIn:
            {
                const double c = (m - 1) / 2.0;
                const double d = std::fabs(r - c);       // 0 in the middle .. c at the ends
                const double span = c + 1.0;
                t = s.direction == CentreOut ? d / span : (c - d) / span;
            }
            break;
            case Forward:
            default:
                t = m > 0 ? double(r) / m : 0;
            break;
        }
        out[i] = t * s.amount;
    }
    return out;
}
