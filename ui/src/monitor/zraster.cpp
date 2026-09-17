/*
  Q Light Controller Plus
  zraster.cpp

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
#include <algorithm>

#include "zraster.h"

#define Z_FAR (-1e30f)          // "nothing drawn here yet"

ZRaster::ZRaster()
    : m_ss(1)
{
}

void ZRaster::begin(const QSize &size, int supersample, const QColor &background)
{
    m_ss = qBound(1, supersample, 4);
    m_size = size.isValid() ? size : QSize(1, 1);

    const QSize buf(m_size.width() * m_ss, m_size.height() * m_ss);
    /* Premultiplied, so source-over is three multiply-adds and so the result
       can be blitted straight over whatever the widget has already painted --
       the floor grid, in practice. Untouched pixels stay fully transparent. */
    if (m_colour.size() != buf || m_colour.format() != QImage::Format_ARGB32_Premultiplied)
        m_colour = QImage(buf, QImage::Format_ARGB32_Premultiplied);
    m_colour.fill(background);

    const qsizetype px = qsizetype(buf.width()) * buf.height();
    if (m_depth.size() != px)
        m_depth.resize(int(px));
    m_depth.fill(Z_FAR);
    /* Cleared only where last frame put light: most frames draw no beams at
       all, and the rest light part of the view. */
    // Light is at the FINAL resolution -- see scan().
    const qsizetype lightPx = qsizetype(m_size.width()) * m_size.height();
    if (m_light.size() != lightPx * 3)
    {
        m_light.resize(int(lightPx * 3));
        m_light.fill(0.0f);
    }
    else if (m_anyLight && m_lit.isValid())
    {
        const QRect r = m_lit.intersected(QRect(QPoint(0, 0), m_size));
        for (int y = r.top(); y <= r.bottom(); ++y)
            std::fill_n(m_light.data() + (qsizetype(y) * m_size.width() + r.left()) * 3,
                        r.width() * 3, 0.0f);
    }
    m_anyLight = false;
    m_lit = QRect();

    m_bits = m_colour.bits();
    m_bpl = m_colour.bytesPerLine();
    m_depthPtr = m_depth.data();
    m_lightPtr = m_light.data();
}

void ZRaster::poly(const QPointF *pts, const double *zs, int n,
                   const QColor &fill, bool depthWrite, const double *alphaScale,
                   int rowFrom, int rowTo)
{
    scan(pts, zs, n, fill, depthWrite, alphaScale, false, rowFrom, rowTo, nullptr);
}

void ZRaster::light(const QPointF *pts, const double *zs, int n, const QColor &colour,
                    const double *alphaScale, int rowFrom, int rowTo, QRect *lit)
{
    scan(pts, zs, n, colour, false, alphaScale, true, rowFrom, rowTo, lit);
}

void ZRaster::addLit(const QRect &lit)
{
    if (lit.isValid() == false)
        return;
    m_lit |= lit;
    m_anyLight = true;
}

void ZRaster::scan(const QPointF *pts, const double *zs, int n, const QColor &fill,
                   bool depthWrite, const double *alphaScale, bool additive,
                   int rowFrom, int rowTo, QRect *lit)
{
    if (m_colour.isNull() || pts == nullptr || zs == nullptr || n < 3)
        return;
    if (fill.isValid() == false || fill.alpha() == 0)
        return;

    /* Light is soft: it gains nothing from supersampling, and at four times
       the pixels it was most of a beam-lit frame (16 ms of 39 on the demo
       rig). It is rasterised at the final resolution and depth-tested against
       the centre sample of each supersampled pixel. */
    const int W = additive ? m_size.width()  : m_colour.width();
    const int H = additive ? m_size.height() : m_colour.height();
    const double S = additive ? 1.0 : double(m_ss);
    const int ssW = m_colour.width();
    const int half = m_ss / 2;

    /* Fit z = A*x + B*y + C from three vertices.
     *
       This is EXACT, not a fit in the least-squares sense: the projection is
       orthographic and viewDepth() is linear in world space, so depth really is
       an affine function of screen position across any planar polygon. That is
       the entire reason this rasteriser can be a couple of adds per pixel. */
    double A = 0.0, B = 0.0, C = zs[0];
    {
        int k = 2;
        double det = 0.0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        const double x0 = pts[0].x() * S, y0 = pts[0].y() * S;
        // Walk forward until three vertices are not collinear on screen.
        for (; k < n; ++k)
        {
            x1 = pts[1].x() * S - x0; y1 = pts[1].y() * S - y0;
            x2 = pts[k].x() * S - x0; y2 = pts[k].y() * S - y0;
            det = x1 * y2 - x2 * y1;
            if (qAbs(det) > 1e-9)
                break;
        }
        if (k < n)
        {
            const double d1 = zs[1] - zs[0], d2 = zs[k] - zs[0];
            A = (d1 * y2 - d2 * y1) / det;
            B = (x1 * d2 - x2 * d1) / det;
            C = zs[0] - A * x0 - B * y0;
        }
        // Degenerate (a polygon edge-on to the eye): flat depth is right.
    }

    /* Per-vertex alpha, fitted the same way and for the same reason: it is
       affine across a planar polygon, so one plane fit is exact. */
    double aA = 0.0, aB = 0.0, aC = 1.0;
    if (alphaScale != nullptr)
    {
        const double x0 = pts[0].x() * S, y0 = pts[0].y() * S;
        aC = alphaScale[0];
        for (int k = 2; k < n; ++k)
        {
            const double x1 = pts[1].x() * S - x0, y1 = pts[1].y() * S - y0;
            const double x2 = pts[k].x() * S - x0, y2 = pts[k].y() * S - y0;
            const double det = x1 * y2 - x2 * y1;
            if (qAbs(det) <= 1e-9)
                continue;
            const double d1 = alphaScale[1] - alphaScale[0];
            const double d2 = alphaScale[k] - alphaScale[0];
            aA = (d1 * y2 - d2 * y1) / det;
            aB = (x1 * d2 - x2 * d1) / det;
            aC = alphaScale[0] - aA * x0 - aB * y0;
            break;
        }
    }

    double minY = pts[0].y() * S, maxY = minY;
    for (int i = 1; i < n; ++i)
    {
        minY = qMin(minY, pts[i].y() * S);
        maxY = qMax(maxY, pts[i].y() * S);
    }
    const int y0 = qMax(qMax(0, rowFrom), int(qFloor(minY)));
    const int y1 = qMin(qMin(H - 1, rowTo - 1), int(qCeil(maxY)));
    if (y1 < y0)
        return;

    const int alpha = fill.alpha();
    // Premultiplied source, computed once.
    const int sr = fill.red()   * alpha / 255;
    const int sg = fill.green() * alpha / 255;
    const int sb = fill.blue()  * alpha / 255;
    const QRgb src = qRgba(sr, sg, sb, alpha);
    const int ia = 255 - alpha;

    QVector<double> xs;
    xs.reserve(8);
    for (int y = y0; y <= y1; ++y)
    {
        const double sy = y + 0.5;
        xs.clear();
        for (int i = 0; i < n; ++i)
        {
            const double ax = pts[i].x() * S, ay = pts[i].y() * S;
            const double bx = pts[(i + 1) % n].x() * S, by = pts[(i + 1) % n].y() * S;
            if ((ay <= sy && by > sy) || (by <= sy && ay > sy))
                xs << ax + (sy - ay) / (by - ay) * (bx - ax);
        }
        if (xs.size() < 2)
            continue;
        std::sort(xs.begin(), xs.end());

        QRgb *line = additive ? nullptr : reinterpret_cast<QRgb *>(m_bits + qsizetype(y) * m_bpl);
        float *zline = m_depthPtr + (additive ? qsizetype(y * m_ss + half) * ssW
                                              : qsizetype(y) * W);

        for (int k = 0; k + 1 < xs.size(); k += 2)
        {
            const int xa = qMax(0, int(qCeil(xs.at(k) - 0.5)));
            const int xb = qMin(W - 1, int(qFloor(xs.at(k + 1) - 0.5)));
            if (xb < xa)
                continue;

            if (additive)
            {
                /* Light: add this much of the colour, wherever nothing opaque
                   stands in front of it. No depth write, so order is free. */
                const float fr = float(fill.red()), fg = float(fill.green()),
                            fb = float(fill.blue());
                const double a0 = alpha / 255.0;
                float z = float(A * (xa + 0.5) + B * sy + C);
                double av = (alphaScale != nullptr) ? aA * (xa + 0.5) + aB * sy + aC : 1.0;
                const double dav = (alphaScale != nullptr) ? aA : 0.0;
                float *lrow = m_lightPtr + qsizetype(y) * W * 3;
                if (lit != nullptr)
                    *lit |= QRect(xa, y, xb - xa + 1, 1);
                else
                {
                    m_lit |= QRect(xa, y, xb - xa + 1, 1);
                    m_anyLight = true;
                }
                for (int x = xa; x <= xb; ++x, z += float(A), av += dav)
                {
                    if (z <= zline[x * m_ss + half])
                        continue;
                    const float w = float(a0 * qBound(0.0, av, 1.0));
                    if (w <= 0.0f)
                        continue;
                    lrow[x * 3]     += fr * w;
                    lrow[x * 3 + 1] += fg * w;
                    lrow[x * 3 + 2] += fb * w;
                }
                continue;
            }
            /* Two loops, not one with a branch in it. The alpha test and the
               depth-write test are constant for the whole span, and leaving
               them inside cost more than the work they guard: hoisting them
               took a full-screen quad from 0.22 ms to a fraction of it. Float
               throughout for the same reason -- one conversion per pixel is a
               conversion too many. */
            const float zStart = float(A * (xa + 0.5) + B * sy + C);
            const float dz = float(A);

            if (alphaScale != nullptr)
            {
                /* Faded: alpha varies per pixel, so the flat-span shortcuts
                   below do not apply. */
                float z = zStart;
                double av = aA * (xa + 0.5) + aB * sy + aC;
                for (int x = xa; x <= xb; ++x, z += dz, av += aA)
                {
                    if (z <= zline[x])
                        continue;
                    const int a2 = qBound(0, int(alpha * av), 255);
                    if (a2 <= 0)
                        continue;
                    const QRgb d = line[x];
                    const int i2 = 255 - a2;
                    line[x] = qRgba(fill.red()   * a2 / 255 + qRed(d)   * i2 / 255,
                                    fill.green() * a2 / 255 + qGreen(d) * i2 / 255,
                                    fill.blue()  * a2 / 255 + qBlue(d)  * i2 / 255,
                                    a2 + qAlpha(d) * i2 / 255);
                    if (depthWrite)
                        zline[x] = z;
                }
            }
            else if (alpha >= 255 && depthWrite)
            {
                float z = zStart;
                for (int x = xa; x <= xb; ++x, z += dz)
                    if (z > zline[x]) { zline[x] = z; line[x] = src; }
            }
            else if (alpha >= 255)
            {
                float z = zStart;
                for (int x = xa; x <= xb; ++x, z += dz)
                    if (z > zline[x]) line[x] = src;
            }
            else
            {
                float z = zStart;
                for (int x = xa; x <= xb; ++x, z += dz)
                {
                    if (z <= zline[x])
                        continue;
                    const QRgb d = line[x];
                    line[x] = qRgba(sr + qRed(d)   * ia / 255,
                                    sg + qGreen(d) * ia / 255,
                                    sb + qBlue(d)  * ia / 255,
                                    alpha + qAlpha(d) * ia / 255);
                    if (depthWrite)
                        zline[x] = z;
                }
            }
        }
    }
}

void ZRaster::thickLine(const QPointF &a, const QPointF &b, double za, double zb,
                        double width, const QColor &colour, bool depthWrite,
                        int rowFrom, int rowTo)
{
    QPointF d = b - a;
    const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-6)
        return;
    d /= len;
    const QPointF nrm(-d.y() * width * 0.5, d.x() * width * 0.5);

    /* A line is a quad with depth running down its length: both ends of a given
       edge share that end's depth, so the plane fit describes it exactly. */
    const QPointF pts[4] = { a + nrm, b + nrm, b - nrm, a - nrm };
    const double  zs[4]  = { za, zb, zb, za };
    poly(pts, zs, 4, colour, depthWrite, nullptr, rowFrom, rowTo);
}

QImage ZRaster::lightImage(QPoint *topLeft) const
{
    if (m_anyLight == false || m_colour.isNull() || m_lit.isEmpty())
        return QImage();

    /* Roll-off: linear for a lone beam, easing toward a ceiling short of white
       as light piles up, so a crowd of beams is bright without being solid.
       A table, because this runs over every lit pixel of every frame. */
    static const float CEILING = 225.0f;
    static const int STEPS = 4096;
    static const QVector<float> curve = []() {
        QVector<float> t(STEPS + 1);
        for (int i = 0; i <= STEPS; ++i)
            t[i] = CEILING * (1.0f - float(qExp(-double(i) / double(CEILING))));
        return t;
    }();
    auto roll = [](float v) {
        return curve.at(qBound(0, int(v), STEPS));
    };

    // Just the lit rectangle; already at the final resolution.
    const int W = m_size.width();
    const QRect r = m_lit.intersected(QRect(QPoint(0, 0), m_size));
    if (r.isEmpty())
        return QImage();

    QImage out(r.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    for (int y = r.top(); y <= r.bottom(); ++y)
    {
        const float *lrow = m_light.constData() + qsizetype(y) * W * 3;
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y - r.top()));
        for (int x = r.left(); x <= r.right(); ++x)
        {
            const float lr = lrow[x * 3], lg = lrow[x * 3 + 1], lb = lrow[x * 3 + 2];
            if (lr <= 0.0f && lg <= 0.0f && lb <= 0.0f)
                continue;
            const int ir = int(roll(lr)), ig = int(roll(lg)), ib = int(roll(lb));
            // Premultiplied: alpha must cover the brightest channel.
            line[x - r.left()] = qRgba(ir, ig, ib, qMax(ir, qMax(ig, ib)));
        }
    }
    if (topLeft != nullptr)
        *topLeft = r.topLeft();
    return out;
}

QImage ZRaster::resolve() const
{
    if (m_colour.isNull())
        return QImage();
    if (m_ss <= 1)
        return m_colour;
    return m_colour.scaled(m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

double ZRaster::depthAt(const QPoint &p) const
{
    if (m_colour.isNull())
        return double(Z_FAR);
    const int x = p.x() * m_ss, y = p.y() * m_ss;
    if (x < 0 || y < 0 || x >= m_colour.width() || y >= m_colour.height())
        return double(Z_FAR);
    return double(m_depth.at(int(qsizetype(y) * m_colour.width() + x)));
}
