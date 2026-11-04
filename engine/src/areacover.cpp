/*
  Q Light Controller Plus
  areacover.cpp

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
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <atomic>
#include <QMutex>

#include "areacover.h"
#include "aimsolver.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "righeads.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

static QString tr(const char *text)
{
    return QCoreApplication::translate("AreaCover", text);
}

bool AreaCover::covers(const StageTarget *target)
{
    return target != nullptr && target->kind() == StageTarget::Kind::Area
           && target->footprint().width() > 0.0 && target->footprint().height() > 0.0;
}

float AreaCover::subjectHeight(const StageTarget *target)
{
    if (target != nullptr && target->hasAimHeightOverride())
        return target->aimHeightOverride();
    return StageTarget::defaultAimHeight(StageTarget::Kind::Person);
}

QRectF AreaCover::areaRect(const MonitorProperties *props, const StageTarget *target)
{
    if (props == nullptr || !covers(target))
        return QRectF();
    const QVector3D c = props->targetAimPoint(target);
    const QSizeF fp = props->targetFootprint(target);      // in the blocking read (BLOCKING.md)
    return QRectF(double(c.x()) - fp.width() / 2.0, double(c.y()) - fp.height() / 2.0,
                  fp.width(), fp.height());
}

namespace
{
/* How many distinct values @p v holds, values closer than @p tol counting as one. */
int levels(QVector<double> v, double tol)
{
    if (v.isEmpty())
        return 0;
    std::sort(v.begin(), v.end());
    int n = 1;
    for (int i = 1; i < v.size(); ++i)
        if (v.at(i) - v.at(i - 1) > tol)
            ++n;
    return n;
}

/* The lights' depth key: low and downstage first, high and upstage last. A
   tower's shelves differ only in height; a front and a back truss only in
   depth; both read the same way. */
double depthKey(const QVector3D &p)
{
    return double(p.z()) - double(p.y());
}

/* The least-cost one-to-one match of rows (lights) to columns (cells) of a
   square @p cost: the Hungarian method, O(n^3), in whole numbers so a tie is
   a tie on every platform. Row i gets column out[i]. @p u and @p v, when
   given, get the potentials it ends with: cost[i][j] - u[i] - v[j] is never
   below 0, and is 0 on every pair some least-cost match can use. */
QVector<int> assign(const QVector<QVector<qint64>> &cost, QVector<qint64> *uOut = nullptr,
                    QVector<qint64> *vOut = nullptr)
{
    const int n = cost.size();
    const qint64 inf = std::numeric_limits<qint64>::max() / 4;
    // 1-based, with potentials u (rows) and v (columns); p[j]: row on column j.
    QVector<qint64> u(n + 1, 0), v(n + 1, 0);
    QVector<int> p(n + 1, 0), way(n + 1, 0);
    for (int i = 1; i <= n; ++i)
    {
        p[0] = i;
        int j0 = 0;
        QVector<qint64> minv(n + 1, inf);
        QVector<bool> used(n + 1, false);
        do
        {
            used[j0] = true;
            const int i0 = p[j0];
            qint64 delta = inf;
            int j1 = 0;
            for (int j = 1; j <= n; ++j)
                if (!used[j])
                {
                    const qint64 cur = cost[i0 - 1][j - 1] - u[i0] - v[j];
                    if (cur < minv[j])
                    {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta)
                    {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            for (int j = 0; j <= n; ++j)
                if (used[j])
                {
                    u[p[j]] += delta;
                    v[j] -= delta;
                }
                else
                    minv[j] -= delta;
            j0 = j1;
        } while (p[j0] != 0);
        do
        {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }
    QVector<int> out(n, 0);
    for (int j = 1; j <= n; ++j)
        if (p[j] > 0)
            out[p[j] - 1] = j - 1;
    if (uOut != nullptr)
        *uOut = u.mid(1);
    if (vOut != nullptr)
        *vOut = v.mid(1);
    return out;
}

/* How many lights each row takes, downstage row first: as even as can be,
   the extra ones downstage. */
QVector<int> rowSizes(int n, int rows)
{
    QVector<int> out(rows, n / rows);
    for (int i = 0; i < n % rows; ++i)
        ++out[i];
    return out;
}

/* What tells exactly tied matches apart: each light's beam, and the area's
   surface (AreaCover::zones() fills it in). */
struct Lens
{
    const MonitorProperties *props = nullptr;
    float subject = 1.4f;
    QVector<double> degrees;    //!< per light: its lens as declared (or set by hand)
    QVector<LightMask> masks;   //!< per light: tape over its front
};

/* A beam for the tie-break's own sample test: in double, the same rule as
   AreaCover::coverage()'s. */
struct TieCone
{
    double ox = 0.0, oy = 0.0, oz = 0.0, ax = 0.0, ay = 0.0, az = 1.0, cosHalf = 1.0, half = 0.0;
    LightMask mask;
    QVector3D axis;
    bool valid = false;
};

TieCone tieCone(const QVector3D &origin, double x, double y, double z, double degrees, const LightMask &mask)
{
    TieCone k;
    k.ox = double(origin.x()); k.oy = double(origin.y()); k.oz = double(origin.z());
    const double dx = x - k.ox, dy = y - k.oy, dz = z - k.oz;
    const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-4 || degrees <= 0.0)
        return k;
    k.ax = dx / len; k.ay = dy / len; k.az = dz / len;
    k.half = qBound(0.1, degrees, 170.0) / 2.0;
    k.cosHalf = std::cos(qDegreesToRadians(k.half));
    k.mask = mask;
    k.axis = QVector3D(float(k.ax), float(k.ay), float(k.az));
    k.valid = true;
    return k;
}

bool tieLit(const TieCone &k, double x, double y, double z)
{
    const double dx = x - k.ox, dy = y - k.oy, dz = z - k.oz;
    const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len <= 1e-4 || dx * k.ax + dy * k.ay + dz * k.az < k.cosHalf * len)
        return false;
    if (k.mask.isNone())
        return true;
    double ax, ay;
    LightRays::angles(k.axis, QVector3D(float(dx / len), float(dy / len), float(dz / len)), ax, ay);
    return !(ax < -k.half + double(k.mask.left) || ax > k.half - double(k.mask.right)
             || ay < -k.half + double(k.mask.top) || ay > k.half - double(k.mask.bottom));
}

int bitCount(quint64 w)
{
    int c = 0;
    for (; w != 0; w &= w - 1)
        ++c;
    return c;
}

/* Bounds on the tie-break, so a big rig stays interactive: past these the
   plain least-squares pick stands. */
constexpr int kTieMaxLights = 96;       // lights in the area
constexpr int kTieMaxPairs = 3000;      // light/cell pairs some tied match can use
constexpr int kTieMaxTries = 4000;      // swaps tried
constexpr int kTieSamplesAcross = 32;   // the coarse grid the candidates are scored on

/* Among the matches as cheap as @p match (all least-squares, exactly tied:
   a UST's two arms, the washes mirrored on two towers), the one that lights
   the most of @p area at head height AND on the deck (Q213). Pairs some
   least-cost match can use are the tight ones under the potentials @p u, @p v.
   First a second assignment over those pairs, by how much of its own cell
   each light lights; then swaps of two lights that keep the cost, scored on
   the whole area, kept while they gain. Everything is counted in whole
   samples, so the pick is the same on every platform. */
void breakTies(const QRectF &area, const QVector<QRectF> &cell, const QVector<QVector3D> &where,
               const QVector<QVector<qint64>> &cost, const QVector<qint64> &u, const QVector<qint64> &v,
               const Lens &lens, QVector<int> &match)
{
    const int n = where.size();
    if (n < 2 || n > kTieMaxLights || lens.props == nullptr
        || lens.degrees.size() != n || lens.masks.size() != n)
        return;
    QVector<int> pairOf(n * n, -1);     // i * n + j -> its index among the tight pairs
    int pairs = 0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (cost[i][j] - u[i] - v[j] == 0)
                pairOf[i * n + j] = pairs++;
    if (pairs <= n || pairs > kTieMaxPairs)
        return;                         // one way only (or too many to weigh)

    // The coarse grid: each sample at head height and on the surface under it.
    const int cols = kTieSamplesAcross;
    const int rows = qBound(3, int(std::lround(double(cols) * area.height() / area.width())), kTieSamplesAcross);
    const int samples = cols * rows, words = (samples + 63) / 64;
    const double sw = area.width() / double(cols), sh = area.height() / double(rows);
    QVector<double> sx(samples), sy(samples), sz(samples);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const int at = r * cols + c;
            sx[at] = area.left() + (double(c) + 0.5) * sw;
            sy[at] = area.top() + (double(r) + 0.5) * sh;
            sz[at] = double(lens.props->platformHeightAt(float(sx[at]), float(sy[at])));
        }
    // Each cell's own samples, and the point a light aims at in it.
    QVector<quint64> inCell(n * words, 0);
    QVector<double> aimZ(n);
    int perCell = 1;
    for (int j = 0; j < n; ++j)
    {
        const QRectF &cj = cell.at(j);
        int count = 0;
        for (int at = 0; at < samples; ++at)
            if (sx[at] >= cj.left() && sx[at] < cj.right() && sy[at] >= cj.top() && sy[at] < cj.bottom())
            {
                inCell[j * words + at / 64] |= quint64(1) << (at % 64);
                ++count;
            }
        perCell = qMax(perCell, count);
        const QPointF c = cj.center();
        aimZ[j] = double(lens.props->platformHeightAt(float(c.x()), float(c.y()))) + double(lens.subject);
    }
    // What each tight pair lights: at head height, and on the deck.
    QVector<quint64> head(pairs * words, 0), deck(pairs * words, 0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            const int p = pairOf[i * n + j];
            if (p < 0)
                continue;
            const QPointF c = cell.at(j).center();
            const TieCone k = tieCone(where.at(i), c.x(), c.y(), aimZ[j], lens.degrees.at(i), lens.masks.at(i));
            if (!k.valid)
                continue;
            for (int at = 0; at < samples; ++at)
            {
                const quint64 bit = quint64(1) << (at % 64);
                if (tieLit(k, sx[at], sy[at], sz[at] + double(lens.subject)))
                    head[p * words + at / 64] |= bit;
                if (tieLit(k, sx[at], sy[at], sz[at]))
                    deck[p * words + at / 64] |= bit;
            }
        }

    /* The second assignment: least-squares first, then the fewest samples
       of its own cell each light leaves dark at either height. K keeps the
       first ahead of the second whatever the second adds up to. */
    const qint64 K = qint64(n) * qint64(perCell) + 1;
    QVector<QVector<qint64>> second(n, QVector<qint64>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            int dark = 0;
            const int p = pairOf[i * n + j];
            for (int w = 0; w < words; ++w)
            {
                const quint64 lit = p < 0 ? 0 : (head[p * words + w] & deck[p * words + w]);
                dark += bitCount(inCell[j * words + w] & ~lit);
            }
            second[i][j] = cost[i][j] * K + dark;
        }
    QVector<int> pick = assign(second);
    for (int i = 0; i < n; ++i)
        if (pairOf[i * n + pick.at(i)] < 0 || pairOf[i * n + match.at(i)] < 0)
            return;                     // cannot happen; keep the plain pick if it does

    // Then the whole area: swap two lights while the cost holds and it gains.
    QVector<quint64> h(words), d(words);
    auto score = [&](const QVector<int> &m) {
        h.fill(0);
        d.fill(0);
        for (int i = 0; i < n; ++i)
        {
            const int p = pairOf[i * n + m.at(i)];
            for (int w = 0; w < words; ++w)
            {
                h[w] |= head[p * words + w];
                d[w] |= deck[p * words + w];
            }
        }
        int lit = 0;
        for (int w = 0; w < words; ++w)
            lit += bitCount(h[w] & d[w]);
        return lit;
    };
    /* The separate count is only a guide (a light's spill lights its
       neighbours' cells too): start from whichever of the two lights more. */
    int best = score(pick);
    const int plain = score(match);
    if (plain >= best)
    {
        pick = match;
        best = plain;
    }
    int tries = 0;
    for (bool gained = true; gained && tries < kTieMaxTries;)
    {
        gained = false;
        for (int i = 0; i < n && tries < kTieMaxTries; ++i)
            for (int k = i + 1; k < n && tries < kTieMaxTries; ++k)
            {
                const int a = pick.at(i), b = pick.at(k);
                if (pairOf[i * n + b] < 0 || pairOf[k * n + a] < 0)
                    continue;
                ++tries;
                pick[i] = b;
                pick[k] = a;
                const int s = score(pick);
                if (s > best)
                {
                    best = s;
                    gained = true;
                }
                else
                {
                    pick[i] = a;
                    pick[k] = b;
                }
            }
    }
    match = pick;
}

/* AreaCover::cells(), with exact ties settled by @p lens when there is one. */
QVector<QRectF> layout(const QRectF &area, const QVector<QVector3D> &where, int *rowsOut, const Lens *lens);
}

QVector<QRectF> AreaCover::cells(const QRectF &area, const QVector<QVector3D> &where, int *rowsOut)
{
    return layout(area, where, rowsOut, nullptr);
}

namespace
{
QVector<QRectF> layout(const QRectF &area, const QVector<QVector3D> &where, int *rowsOut, const Lens *lens)
{
    const int n = where.size();
    QVector<QRectF> out(n);
    if (rowsOut != nullptr)
        *rowsOut = 0;
    if (n == 0 || area.width() <= 0.0 || area.height() <= 0.0)
        return out;

    // The shape the lights hang in: distinct sides across, distinct levels deep.
    QVector<double> xs, ds;
    for (const QVector3D &p : where)
    {
        xs << double(p.x());
        ds << depthKey(p);
    }
    const double hangShape = double(levels(xs, 0.3)) / double(levels(ds, 0.3));

    // The grid: square cells for this area, shaped like the rig.
    int best = 1;
    double bestCost = 1e300;
    for (int r = 1; r <= n; ++r)
    {
        const double cols = double(n) / double(r);
        const double cellW = area.width() / cols, cellD = area.height() / double(r);
        const double cost = qAbs(std::log(cellW / cellD)) + 1.5 * qAbs(std::log((cols / double(r)) / hangShape));
        if (cost < bestCost - 1e-9)
        {
            bestCost = cost;
            best = r;
        }
    }
    const QVector<int> sizes = rowSizes(n, best);
    if (rowsOut != nullptr)
        *rowsOut = best;

    // The cells, downstage row first, each row stage right to left.
    QVector<QRectF> cell;
    const double rowD = area.height() / double(best);
    for (int r = 0; r < best; ++r)
    {
        const double cellW = area.width() / double(sizes.at(r));
        const double top = area.bottom() - double(r + 1) * rowD;     // row 0 downstage
        for (int c = 0; c < sizes.at(r); ++c)
            cell << QRectF(area.left() + double(c) * cellW, top, cellW, rowD);
    }

    /* Who takes which: each light where it sits among the others -- across
       (its x) and in depth (its depth key), both scaled 0..1 -- matched to
       the cells as they sit in the area, so the total squared distance is
       least. That keeps the stage-right light stage right and the low shelf
       in front, and a least-squares match never crosses two of them over
       in that space: a regular rig's beams don't cross. */
    double xLo = 1e300, xHi = -1e300, kLo = 1e300, kHi = -1e300;
    for (const QVector3D &p : where)
    {
        xLo = qMin(xLo, double(p.x()));
        xHi = qMax(xHi, double(p.x()));
        kLo = qMin(kLo, depthKey(p));
        kHi = qMax(kHi, depthKey(p));
    }
    auto norm = [](double v, double lo, double hi) {
        return hi - lo > 1e-6 ? (v - lo) / (hi - lo) : 0.5;
    };
    QVector<QVector<qint64>> cost(n, QVector<qint64>(n));
    for (int i = 0; i < n; ++i)
    {
        const double sx = norm(double(where.at(i).x()), xLo, xHi);
        const double sd = norm(depthKey(where.at(i)), kLo, kHi);
        for (int j = 0; j < n; ++j)
        {
            const double cx = (cell.at(j).center().x() - area.left()) / area.width();
            const double cd = (area.bottom() - cell.at(j).center().y()) / area.height();
            /* In whole units of 1e-9: lights that sit alike (a UST's two arms,
               a wash on each tower) tie exactly, and which of the tied matches
               wins must not hang on the last bit of a fused multiply-add (arm64
               macOS) versus a plain one (x86-64 GCC). */
            const double d2 = (sx - cx) * (sx - cx) + (sd - cd) * (sd - cd);
            cost[i][j] = std::llround(d2 * 1e9);
        }
    }
    QVector<qint64> u, v;
    QVector<int> match = assign(cost, &u, &v);
    // Of the tied ones, the match that lights the most of the area.
    if (lens != nullptr)
        breakTies(area, cell, where, cost, u, v, *lens, match);
    for (int i = 0; i < n; ++i)
        out[i] = cell.at(match.at(i));
    return out;
}
}

QVector3D AreaCover::pointAt(const MonitorProperties *props, const StageTarget *target,
                             const QPointF &offset)
{
    if (props == nullptr || target == nullptr)
        return QVector3D();
    const QVector3D c = props->targetAimPoint(target);
    const float x = c.x() + float(offset.x()), y = c.y() + float(offset.y());
    return QVector3D(x, y, props->platformHeightAt(x, y) + subjectHeight(target));
}

namespace
{
/* The zones, worked out: the least-squares rule and its tie-break. */
QList<AreaCover::Zone> workOutZones(Doc *doc, MonitorProperties *props, const StageTarget *target,
                                    const QList<GroupHead> &members)
{
    QList<AreaCover::Zone> out;
    const QRectF area = AreaCover::areaRect(props, target);
    QList<GroupHead> placed;
    QVector<QVector3D> where;
    /* The beams that settle a tie: each light's lens as its definition
       declares it (or as set by hand), not its zoom now -- where a light's
       zone is must not move with what is playing. */
    Lens lens;
    lens.props = props;
    lens.subject = AreaCover::subjectHeight(target);
    for (const GroupHead &m : members)
    {
        AimSolver::Frame f;
        if (!AimSolver::frameFor(doc, m, f))
            continue;
        placed << m;
        where << f.position;
        const double deg = RigHeads::beamAngle(doc->fixture(m.fxi), QByteArray());
        lens.degrees << (deg > 0.0 ? deg : AreaCover::defaultBeamDegrees());
        lens.masks << props->fixtureRigProps(m.fxi).mask();
    }
    const QVector<QRectF> cell = layout(area, where, nullptr, &lens);
    for (int i = 0; i < placed.size(); ++i)
    {
        AreaCover::Zone z;
        z.member = placed.at(i);
        z.cell = cell.at(i);
        z.offset = cell.at(i).center() - area.center();
        z.aim = AreaCover::pointAt(props, target, z.offset);
        out << z;
    }
    return out;
}

/* Every input the zones depend on, as numbers: the area, the subject
   height, each member with where it hangs, its lens and its tape, and the
   surface under the tie-break's sample grid. The same numbers give the same
   zones, so a match is a cache hit. Fills @p key without allocating once it
   has grown to size. */
void zoneKey(Doc *doc, MonitorProperties *props, const StageTarget *target,
             const QList<GroupHead> &members, std::vector<double> &key)
{
    key.clear();
    const QRectF area = AreaCover::areaRect(props, target);
    key.push_back(area.x());
    key.push_back(area.y());
    key.push_back(area.width());
    key.push_back(area.height());
    key.push_back(double(AreaCover::subjectHeight(target)));
    key.push_back(double(members.size()));
    for (const GroupHead &m : members)
    {
        key.push_back(double(m.fxi));
        key.push_back(double(m.head));
        AimSolver::Frame f;
        if (!AimSolver::frameFor(doc, m, f))
        {
            key.push_back(0.0);
            continue;
        }
        key.push_back(1.0);
        key.push_back(double(f.position.x()));
        key.push_back(double(f.position.y()));
        key.push_back(double(f.position.z()));
        key.push_back(RigHeads::beamAngle(doc->fixture(m.fxi), QByteArray()));
        const LightMask mask = props->fixtureRigProps(m.fxi).mask();
        key.push_back(double(mask.top));
        key.push_back(double(mask.bottom));
        key.push_back(double(mask.left));
        key.push_back(double(mask.right));
    }
    const int cols = kTieSamplesAcross;
    const int rows = qBound(3, int(std::lround(double(cols) * area.height() / area.width())), kTieSamplesAcross);
    const double sw = area.width() / double(cols), sh = area.height() / double(rows);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            key.push_back(double(props->platformHeightAt(float(area.left() + (double(c) + 0.5) * sw),
                                                         float(area.top() + (double(r) + 0.5) * sh))));
}

/* zones() is asked on the playback tick: Scene::write() -> updateSightDim()
   -> AimSolver::blockedDimmedHeads() -> memberPoints(), every tick an Aim
   look that dims blocked heads plays. The tie-break must never run there:
   the last few results are kept, by their inputs, and handed back shared
   (no allocation) while the inputs hold. */
struct ZoneMemo
{
    std::vector<double> key;
    QList<AreaCover::Zone> zones;
    QHash<GroupHead, QVector3D> points;
    quint64 used = 0;
};
constexpr int kMemoSize = 8;
QMutex g_memoMutex;
ZoneMemo g_memo[kMemoSize];
quint64 g_memoClock = 0;
std::atomic<quint64> g_workedOut { 0 };

/* Whether each kept zone's aim is still at subject height above what is
   under it now (the surface there is read fresh, as pointAt() does). */
bool aimsHold(const MonitorProperties *props, const StageTarget *target, const QList<AreaCover::Zone> &zones)
{
    const float subject = AreaCover::subjectHeight(target);
    for (const AreaCover::Zone &z : zones)
        if (props->platformHeightAt(z.aim.x(), z.aim.y()) + subject != z.aim.z())
            return false;
    return true;
}

void memoZones(Doc *doc, const StageTarget *target, const QList<GroupHead> &members,
               QList<AreaCover::Zone> *zones, QHash<GroupHead, QVector3D> *points)
{
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props == nullptr || !AreaCover::covers(target) || members.isEmpty())
        return;
    static thread_local std::vector<double> key;
    zoneKey(doc, props, target, members, key);
    {
        QMutexLocker locker(&g_memoMutex);
        for (ZoneMemo &m : g_memo)
            if (m.used != 0 && m.key == key && aimsHold(props, target, m.zones))
            {
                m.used = ++g_memoClock;
                if (zones != nullptr)
                    *zones = m.zones;
                if (points != nullptr)
                    *points = m.points;
                return;
            }
    }
    const QList<AreaCover::Zone> worked = workOutZones(doc, props, target, members);
    g_workedOut.fetch_add(1, std::memory_order_relaxed);
    QHash<GroupHead, QVector3D> pts;
    for (const AreaCover::Zone &z : worked)
        pts.insert(z.member, z.aim);
    {
        QMutexLocker locker(&g_memoMutex);
        ZoneMemo *oldest = &g_memo[0];
        for (ZoneMemo &m : g_memo)
            if (m.used < oldest->used)
                oldest = &m;
        oldest->key = key;
        oldest->zones = worked;
        oldest->points = pts;
        oldest->used = ++g_memoClock;
    }
    if (zones != nullptr)
        *zones = worked;
    if (points != nullptr)
        *points = pts;
}
}

QList<AreaCover::Zone> AreaCover::zones(Doc *doc, const StageTarget *target,
                                        const QList<GroupHead> &members)
{
    QList<Zone> out;
    memoZones(doc, target, members, &out, nullptr);
    return out;
}

QHash<GroupHead, QVector3D> AreaCover::memberPoints(Doc *doc, const StageTarget *target,
                                                    const QList<GroupHead> &members)
{
    QHash<GroupHead, QVector3D> out;
    memoZones(doc, target, members, nullptr, &out);
    return out;
}

quint64 AreaCover::zonesWorkedOut()
{
    return g_workedOut.load(std::memory_order_relaxed);
}

QVector<QPointF> AreaCover::footprint(const QVector3D &origin, const QVector3D &aim,
                                      double beamDeg, int points)
{
    QVector<QPointF> out;
    const QVector3D axis3 = aim - origin;
    if (axis3.length() < 1e-4f || beamDeg <= 0.0 || points < 3)
        return out;
    const QVector3D a = axis3.normalized();
    // Two directions across the beam.
    QVector3D u = QVector3D::crossProduct(a, qAbs(a.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0));
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(a, u).normalized();
    const double half = qDegreesToRadians(qBound(0.1, beamDeg, 170.0) / 2.0);
    const float ca = float(qCos(half)), sa = float(qSin(half));
    const double reach = 40.0;
    out.reserve(points);
    for (int i = 0; i < points; ++i)
    {
        const double phi = 2.0 * M_PI * double(i) / double(points);
        const QVector3D d = a * ca + (u * float(qCos(phi)) + v * float(qSin(phi))) * sa;
        const double dz = double(d.z());
        const double drop = double(aim.z()) - double(origin.z());
        double t = qAbs(dz) > 1e-6 ? drop / dz : -1.0;
        QVector3D at;
        const double flat = std::hypot(double(d.x()), double(d.y()));
        if (t > 0.0 && t * flat <= reach)
            at = origin + d * float(t);
        else
        {
            // Never comes down (or too far out): cut it off, out along its bearing.
            const double s = flat > 1e-6 ? reach / flat : 0.0;
            at = origin + QVector3D(d.x(), d.y(), 0) * float(s);
        }
        out << QPointF(double(at.x()), double(at.y()));
    }
    return out;
}

static QList<QRectF> darkRuns(const QRectF &area, int cols, int rows, const QVector<bool> &lit)
{
    QList<QRectF> out;
    if (cols <= 0 || rows <= 0 || lit.size() != cols * rows)
        return out;
    const double w = area.width() / double(cols), h = area.height() / double(rows);
    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < cols)
        {
            if (lit.at(r * cols + c))
            {
                ++c;
                continue;
            }
            const int start = c;
            while (c < cols && !lit.at(r * cols + c))
                ++c;
            out << QRectF(area.left() + double(start) * w, area.top() + double(r) * h,
                          double(c - start) * w, h);
        }
    }
    return out;
}

QList<QRectF> AreaCover::Coverage::gapRects() const { return darkRuns(area, cols, rows, lit); }
QList<QRectF> AreaCover::Coverage::deckGapRects() const { return darkRuns(area, cols, rows, deckLit); }
QList<QRectF> AreaCover::Coverage::headGapRects() const { return darkRuns(area, cols, rows, headLit); }

QString AreaCover::Coverage::caption() const
{
    if (!valid())
        return QString();
    const int pct = int(std::floor(fraction * 100.0 + 0.5));
    if (gap.isEmpty() || pct >= 100)
        return tr("Covers all of %1").arg(name);
    return tr("Covers %1% of %2 \u00b7 %3").arg(pct).arg(name, missingPhrase());
}

QString AreaCover::Coverage::missingPhrase() const
{
    if (gap.isEmpty())
        return QString();
    // Dark in the same places at both heights: one gap, no need to split it.
    if (deckLit == headLit)
        return gapPhrase();
    QStringList parts;
    if (!deckGap.isEmpty())
        parts << (deckGap == tr("all over") ? tr("deck dark all over") : tr("deck dark %1").arg(deckGap));
    if (!headGap.isEmpty())
        parts << (headGap == tr("all over") ? tr("heads dark all over") : tr("heads dark %1").arg(headGap));
    return parts.join(QStringLiteral(" \u00b7 "));
}

QString AreaCover::Coverage::gapPhrase() const
{
    if (gap.isEmpty())
        return QString();
    if (gap == tr("all over"))
        return tr("gaps all over");
    return tr("gap %1").arg(gap);
}

QString AreaCover::gapName(const QVector<bool> &lit, int cols, int rows)
{
    if (cols <= 0 || rows <= 0 || lit.size() != cols * rows)
        return QString();
    // Unlit samples on a 3 x 3 split: row 0 upstage, column 0 stage right.
    int count[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    int total = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (!lit.at(r * cols + c))
            {
                ++count[qMin(2, r * 3 / rows)][qMin(2, c * 3 / cols)];
                ++total;
            }
    if (total == 0)
        return QString();
    // Dark over most of the area: no one place to name.
    int darkParts = 0;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
        {
            const int cells = ((r + 1) * rows / 3 - r * rows / 3) * ((c + 1) * cols / 3 - c * cols / 3);
            if (cells > 0 && count[r][c] * 2 >= cells)
                ++darkParts;
        }
    if (darkParts >= 5)
        return tr("all over");
    static const char *const names[3][3] = {
        { QT_TRANSLATE_NOOP("AreaCover", "upstage right"), QT_TRANSLATE_NOOP("AreaCover", "upstage centre"),
          QT_TRANSLATE_NOOP("AreaCover", "upstage left") },
        { QT_TRANSLATE_NOOP("AreaCover", "stage right"), QT_TRANSLATE_NOOP("AreaCover", "centre stage"),
          QT_TRANSLATE_NOOP("AreaCover", "stage left") },
        { QT_TRANSLATE_NOOP("AreaCover", "downstage right"), QT_TRANSLATE_NOOP("AreaCover", "downstage centre"),
          QT_TRANSLATE_NOOP("AreaCover", "downstage left") } };
    // The biggest part, and the next when it holds a good share too.
    int first = -1, second = -1;
    for (int i = 0; i < 9; ++i)
    {
        const int n = count[i / 3][i % 3];
        if (n == 0)
            continue;
        if (first < 0 || n > count[first / 3][first % 3])
        {
            second = first;
            first = i;
        }
        else if (second < 0 || n > count[second / 3][second % 3])
            second = i;
    }
    QString out = tr(names[first / 3][first % 3]);
    if (second >= 0 && count[second / 3][second % 3] * 4 >= total)
        out = tr("%1 and %2").arg(out, tr(names[second / 3][second % 3]));
    return out;
}

AreaCover::Coverage AreaCover::coverage(const MonitorProperties *props, const StageTarget *target,
                                        const QList<Beam> &beams, int samplesAcross)
{
    Coverage cov;
    if (props == nullptr || !covers(target))
        return cov;
    cov.name = target->name();
    cov.area = areaRect(props, target);
    cov.beams = beams;
    cov.cols = qBound(4, samplesAcross, 200);
    cov.rows = qBound(3, int(std::lround(double(cov.cols) * cov.area.height() / cov.area.width())), 200);
    cov.lit.fill(false, cov.cols * cov.rows);
    cov.headLit.fill(false, cov.cols * cov.rows);
    cov.deckLit.fill(false, cov.cols * cov.rows);

    struct Cone { QVector3D o, a; float cosHalf; double half; LightMask mask; };
    QVector<Cone> cones;
    cones.reserve(beams.size());
    for (Beam &b : cov.beams)
    {
        const QVector3D ax = b.aim - b.origin;
        if (ax.length() < 1e-4f || b.degrees <= 0.0)
            continue;
        if (b.patch.isEmpty())
            b.patch = footprint(b.origin, b.aim, b.degrees);
        cones << Cone { b.origin, ax.normalized(),
                        float(qCos(qDegreesToRadians(qBound(0.1, b.degrees, 170.0) / 2.0))),
                        qBound(0.1, b.degrees, 170.0) / 2.0, b.mask };
    }
    const float subject = subjectHeight(target);
    const double w = cov.area.width() / double(cov.cols), h = cov.area.height() / double(cov.rows);
    auto inBeam = [&cones](const QVector3D &pt) {
        for (const Cone &k : cones)
        {
            const QVector3D d = pt - k.o;
            const float len = d.length();
            if (len > 1e-4f && QVector3D::dotProduct(d, k.a) >= k.cosHalf * len)
            {
                if (!k.mask.isNone())
                {
                    // Tape: the point has to be in what is left of the cone.
                    double ax, ay;
                    LightRays::angles(k.a, d / len, ax, ay);
                    if (ax < -k.half + double(k.mask.left) || ax > k.half - double(k.mask.right)
                        || ay < -k.half + double(k.mask.top) || ay > k.half - double(k.mask.bottom))
                        continue;
                }
                return true;
            }
        }
        return false;
    };
    int bothCount = 0, headOnlyCount = 0, deckOnlyCount = 0;
    for (int r = 0; r < cov.rows; ++r)
        for (int c = 0; c < cov.cols; ++c)
        {
            const float x = float(cov.area.left() + (double(c) + 0.5) * w);
            const float y = float(cov.area.top() + (double(r) + 0.5) * h);
            // A spot is lit only if it is lit at head height AND on the
            // deck, riser or step top under it (Q213).
            const float surface = props->platformHeightAt(x, y);
            const bool head = inBeam(QVector3D(x, y, surface + subject));
            const bool deck = inBeam(QVector3D(x, y, surface));
            const int at = r * cov.cols + c;
            cov.headLit[at] = head;
            cov.deckLit[at] = deck;
            cov.lit[at] = head && deck;
            bothCount += head && deck ? 1 : 0;
            headOnlyCount += head && !deck ? 1 : 0;
            deckOnlyCount += deck && !head ? 1 : 0;
        }
    const double all = double(cov.cols * cov.rows);
    cov.fraction = double(bothCount) / all;
    cov.headOnly = double(headOnlyCount) / all;
    cov.deckOnly = double(deckOnlyCount) / all;
    cov.deckGap = gapName(cov.deckLit, cov.cols, cov.rows);
    cov.headGap = gapName(cov.headLit, cov.cols, cov.rows);
    cov.gap = gapName(cov.lit, cov.cols, cov.rows);
    return cov;
}

AreaCover::Coverage AreaCover::coverage(Doc *doc, const StageTarget *target,
                                        const QList<GroupHead> &members, const Scene *scene)
{
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props == nullptr || !covers(target))
        return Coverage();
    QList<Beam> beams;
    const QList<Zone> zs = zones(doc, target, members);
    QHash<quint32, double> degrees;           // per fixture: its arms share a lens
    for (const Zone &z : zs)
    {
        Fixture *fx = doc->fixture(z.member.fxi);
        AimSolver::Frame f;
        if (fx == nullptr || !AimSolver::frameFor(doc, z.member, f))
            continue;
        if (!degrees.contains(fx->id()))
        {
            // The zoom as the scene has it, else as the fixture is now.
            QByteArray values = fx->channelValues();
            if (values.size() < int(fx->channels()))
                values.append(QByteArray(int(fx->channels()) - values.size(), char(0)));
            if (scene != nullptr)
                for (const SceneValue &sv : scene->values())
                    if (sv.fxi == fx->id() && sv.channel < quint32(values.size()))
                        values[int(sv.channel)] = char(sv.value);
            const double deg = RigHeads::beamAngle(fx, values);
            degrees.insert(fx->id(), deg > 0.0 ? deg : defaultBeamDegrees());
        }
        Beam b;
        b.member = z.member;
        b.origin = f.position;
        b.aim = z.aim;
        b.degrees = degrees.value(fx->id());
        b.mask = props->fixtureRigProps(fx->id()).mask();
        beams << b;
    }
    Coverage cov = coverage(props, target, beams);
    cov.zones = zs;
    return cov;
}
