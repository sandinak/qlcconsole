/*
  Q Light Controller Plus
  linespread.cpp

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

#include <QtGlobal>
#include <algorithm>
#include <cmath>

#include "linespread.h"

LineSpread::Settings LineSpread::fromParams(const QMap<QString, double> &params)
{
    Settings s;
    s.spread = qBound(-maxSpread(), params.value(paramName("Spread"), s.spread), maxSpread());
    s.group = qMax(0, int(params.value(paramName("Group"), s.group)));
    s.cross = Cross(qBound(0, int(params.value(paramName("Cross"), s.cross)), int(Mirror)));
    s.share = params.value(paramName("Share"), 0.0) != 0.0;
    s.order = Order(qBound(0, int(params.value(paramName("Order"), s.order)), int(LookOrder)));
    return s;
}

void LineSpread::toParams(const Settings &s, QMap<QString, double> &params)
{
    const Settings d;
    auto put = [&params](const char *what, double v, bool isDefault) {
        if (isDefault)
            params.remove(paramName(what));
        else
            params.insert(paramName(what), v);
    };
    put("Spread", s.spread, s.spread == d.spread);
    put("Group", s.group, s.group == d.group);
    put("Cross", int(s.cross), s.cross == d.cross);
    put("Share", s.share ? 1.0 : 0.0, s.share == d.share);
    put("Order", int(s.order), s.order == d.order);
}

QVector<double> LineSpread::fractions(int count, const Settings &s)
{
    QVector<double> out;
    if (count <= 0)
        return out;
    out.fill(0.5, count);
    if (count == 1)
        return out;

    // Even slots end to end, as offsets from the centre: -0.5 .. +0.5.
    QVector<double> d(count);
    for (int k = 0; k < count; ++k)
        d[k] = double(k) / (count - 1) - 0.5;

    // The groups, as [first, last] index ranges in order.
    QVector<QPair<int, int> > groups;
    if (s.group == Halves)
    {
        const int half = count / 2;
        groups << qMakePair(0, half - 1);
        if (count % 2 != 0)
            groups << qMakePair(half, half);        // the middle light, on its own
        groups << qMakePair(count - half, count - 1);
    }
    else
    {
        const int g = qMax(1, s.group);
        for (int first = 0; first < count; first += g)
            groups << qMakePair(first, qMin(count - 1, first + g - 1));
    }

    /* What crosses below 0. Groups of one cannot flip in place, and a single
       group has no other side to swap to: either way both choices mean the
       whole line mirrored, so give that rather than nothing at all. */
    Cross cross = s.cross;
    if (groups.size() == count || s.share)      // a shared point cannot flip
        cross = SwapSides;
    else if (groups.size() == 1)
        cross = Mirror;

    const double spread = qBound(-maxSpread(), s.spread, maxSpread());
    const double amount = std::fabs(spread) / maxSpread();
    const double sg = (spread < 0.0 && cross == SwapSides) ? -1.0 : 1.0;
    const double sm = (spread < 0.0 && cross == Mirror) ? -1.0 : 1.0;

    const int ng = groups.size();
    for (int gi = 0; gi < ng; ++gi)
    {
        const int first = groups.at(gi).first, last = groups.at(gi).second;
        double centre = 0.0;
        if (s.share)
        {
            // A group's lights on one point, the groups evenly end to end.
            centre = (ng > 1) ? double(gi) / (ng - 1) - 0.5 : 0.0;
        }
        else
        {
            for (int k = first; k <= last; ++k)
                centre += d.at(k);
            centre /= double(last - first + 1);
        }
        for (int k = first; k <= last; ++k)
        {
            const double within = s.share ? 0.0 : d.at(k) - centre;
            out[k] = qBound(0.0, 0.5 + amount * (sg * centre + sm * within), 1.0);
        }
    }
    return out;
}

QVector<int> LineSpread::orderAlong(const QVector<QVector3D> &positions,
                                    const QVector3D &from, const QVector3D &to)
{
    QVector<int> order(positions.size());
    for (int i = 0; i < order.size(); ++i)
        order[i] = i;
    const QVector3D dir = to - from;
    if (dir.length() < 1e-6f)
        return order;
    const QVector3D unit = dir.normalized();
    QVector<qint64> key(positions.size());
    for (int i = 0; i < positions.size(); ++i)
        key[i] = qint64(std::llround(double(QVector3D::dotProduct(positions.at(i) - from, unit)) * 1000.0));
    std::stable_sort(order.begin(), order.end(),
                     [&key](int a, int b) { return key.at(a) < key.at(b); });
    return order;
}
