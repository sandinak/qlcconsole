/*
  Q Light Controller Plus - qlcconsole
  lightmap.cpp

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
#include <cmath>

#include "lightmap.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "fixture.h"

double LightMap::defaultHeight()
{
    return 66.0 * 0.0254;           // 5' 6"
}

double LightMap::defaultDarkLux()
{
    return 10.0 / (0.3048 * 0.3048);  // 10 fc
}

double LightMap::referenceLumens()
{
    return 3500.0;
}

double LightMap::fixtureLumens(const Fixture *fx, bool *known)
{
    if (known != nullptr)
        *known = false;
    if (fx == nullptr || fx->fixtureMode() == nullptr)
        return referenceLumens();
    const QLCPhysical &phys = fx->fixtureMode()->physical();
    static const double lumensPerWatt = 35.0;         // LED, after its optics
    if (known != nullptr)
        *known = phys.declaresOutput();
    if (phys.bulbLumens() > 0)
        return phys.bulbLumens();
    if (phys.powerConsumption() > 0)
        return phys.powerConsumption() * lumensPerWatt;
    return referenceLumens();
}

double LightMap::beamDegrees(const QLCPhysical &phys)
{
    const double declared = qMax(phys.lensDegreesMin(), phys.lensDegreesMax());
    return declared >= 170.0 ? 150.0 : declared > 0.0 ? declared : 40.0;
}

LightMap::Source LightMap::makeSource(quint32 fixture, const QVector3D &apex, const QVector3D &dir,
                                      double beamDeg, double lumens, double level, double softness)
{
    Source s;
    s.fixture = fixture;
    s.apex = apex;
    s.dir = dir.normalized();
    const double half = qDegreesToRadians(qBound(1.0, beamDeg, 160.0) / 2.0);
    const double edge = qMin(M_PI_2 * 0.99, half * (1.1 + 0.3 * qBound(0.0, softness, 1.0)));
    s.halfRad = float(half);
    s.edgeRad = float(edge);
    s.cosHalf = float(qCos(half));
    s.cosEdge = float(qCos(edge));
    /* Its lumens over the solid angle of its beam: a narrow spot puts the
       same light on a smaller patch, so more on each square metre of it. */
    const double omega = 2.0 * M_PI * (1.0 - qCos(half));
    s.candela = float(qMax(0.0, lumens) * qBound(0.0, level, 1.0) / qMax(1e-4, omega));
    return s;
}

double LightMap::profile(const Source &s, double cosAngle)
{
    if (cosAngle <= double(s.cosEdge))
        return 0.0;
    const double a = std::acos(qBound(-1.0, cosAngle, 1.0));
    const double half = double(s.halfRad);
    if (a <= half)
    {
        const double u = a / qMax(1e-6, half);
        return 1.0 - 0.5 * u * u;           // half as bright at the beam angle
    }
    const double edge = double(s.edgeRad);
    return 0.5 * (edge - a) / qMax(1e-6, edge - half);
}

namespace
{
/* One source's light at p, without the line of sight. */
inline double direct(const LightMap::Source &s, const QVector3D &p, float &dist)
{
    const QVector3D rel = p - s.apex;
    const float d2 = rel.lengthSquared();
    if (d2 < 1e-6f || s.candela <= 0.0f)
        return 0.0;
    dist = std::sqrt(d2);
    const float c = QVector3D::dotProduct(rel, s.dir) / dist;
    if (c <= s.cosEdge)
        return 0.0;
    const double k = LightMap::profile(s, double(c));
    // A light a hand's breadth away is not brighter than one at a quarter metre.
    return double(s.candela) * k / qMax(0.0625, double(d2));
}
}

double LightMap::luxAt(const QVector<Source> &sources, const BeamOcclusion::Model *sight,
                       const QVector3D &p)
{
    double sum = 0.0;
    for (const Source &s : sources)
    {
        float dist = 0.0f;
        const double e = direct(s, p, dist);
        if (e <= 0.0)
            continue;
        if (sight != nullptr && !sight->isEmpty() && sight->segment(s.apex, p, s.exclude).blocked)
            continue;
        sum += e;
    }
    return sum;
}

QVector3D LightMap::Grid::point(int col, int row) const
{
    return QVector3D(float(area.left() + (col + 0.5) * step), float(area.top() + (row + 0.5) * step),
                     z.value(index(col, row), float(height)));
}

QRectF LightMap::stageArea(const MonitorProperties *mp)
{
    if (mp == nullptr)
        return QRectF();
    const double width = double(mp->gridSize().x() * mp->gridUnitMetres());
    const double front = double(mp->downstageEdgeY());
    if (width <= 0.0 || front <= 0.0)
        return QRectF();
    return QRectF(0.0, 0.0, width, front);
}

LightMap::Grid LightMap::layout(const MonitorProperties *mp, const QRectF &area, double height,
                                double darkLux, int maxCells)
{
    Grid g;
    g.height = height;
    g.darkLux = darkLux;
    if (!area.isValid() || area.width() <= 0.0 || area.height() <= 0.0)
        return g;
    const double cells = qMax(1, maxCells);
    g.step = qMax(0.15, std::sqrt(area.width() * area.height() / cells));
    g.cols = qMax(1, int(std::ceil(area.width() / g.step - 1e-6)));
    g.rows = qMax(1, int(std::ceil(area.height() / g.step - 1e-6)));
    // Centred on the area: the last column does not hang off one side.
    g.area = QRectF(area.center().x() - g.cols * g.step / 2.0, area.center().y() - g.rows * g.step / 2.0,
                    g.cols * g.step, g.rows * g.step);
    g.z.resize(g.cols * g.rows);
    for (int r = 0; r < g.rows; ++r)
        for (int c = 0; c < g.cols; ++c)
        {
            const float x = float(g.area.left() + (c + 0.5) * g.step);
            const float y = float(g.area.top() + (r + 0.5) * g.step);
            const float deck = mp != nullptr ? mp->platformHeightAt(x, y) : 0.0f;
            g.z[g.index(c, r)] = deck + float(height);
        }
    return g;
}

bool LightMap::compute(Grid &grid, const QVector<Source> &sources, const BeamOcclusion::ModelPtr &sight,
                       const std::atomic<bool> *cancel)
{
    grid.lux.fill(0.0f, grid.cols * grid.rows);
    const BeamOcclusion::Model *model = (sight.isNull() || sight->isEmpty()) ? nullptr : sight.data();
    for (int r = 0; r < grid.rows; ++r)
    {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
            return false;
        for (int c = 0; c < grid.cols; ++c)
        {
            const QVector3D p = grid.point(c, r);
            grid.lux[grid.index(c, r)] = float(luxAt(sources, model, p));
        }
    }
    return true;
}

QList<QVector<int>> LightMap::darkSpots(const Grid &grid)
{
    QList<QVector<int>> spots;
    if (!grid.isValid() || grid.lux.size() != grid.cols * grid.rows)
        return spots;
    QVector<char> seen(grid.lux.size(), 0);
    QVector<int> stack;
    for (int i = 0; i < grid.lux.size(); ++i)
    {
        if (seen.at(i) || grid.lux.at(i) >= float(grid.darkLux))
            continue;
        QVector<int> spot;
        stack.clear();
        stack << i;
        seen[i] = 1;
        while (!stack.isEmpty())
        {
            const int k = stack.takeLast();
            spot << k;
            const int c = k % grid.cols, r = k / grid.cols;
            const int nb[4][2] = { { c - 1, r }, { c + 1, r }, { c, r - 1 }, { c, r + 1 } };
            for (const auto &n : nb)
            {
                if (n[0] < 0 || n[1] < 0 || n[0] >= grid.cols || n[1] >= grid.rows)
                    continue;
                const int j = grid.index(n[0], n[1]);
                if (seen.at(j) || grid.lux.at(j) >= float(grid.darkLux))
                    continue;
                seen[j] = 1;
                stack << j;
            }
        }
        std::sort(spot.begin(), spot.end());
        spots << spot;
    }
    std::stable_sort(spots.begin(), spots.end(),
                     [](const QVector<int> &a, const QVector<int> &b) { return a.size() > b.size(); });
    return spots;
}
