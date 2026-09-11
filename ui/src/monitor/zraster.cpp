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
}

void ZRaster::poly(const QPointF *pts, const double *zs, int n,
                   const QColor &fill, bool depthWrite, const double *alphaScale)
{
    if (m_colour.isNull() || pts == nullptr || zs == nullptr || n < 3)
        return;
    if (fill.isValid() == false || fill.alpha() == 0)
        return;

    const int W = m_colour.width(), H = m_colour.height();
    const double S = double(m_ss);

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
    const int y0 = qMax(0, int(qFloor(minY)));
    const int y1 = qMin(H - 1, int(qCeil(maxY)));
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

        QRgb *line = reinterpret_cast<QRgb *>(m_colour.scanLine(y));
        float *zline = m_depth.data() + qsizetype(y) * W;

        for (int k = 0; k + 1 < xs.size(); k += 2)
        {
            const int xa = qMax(0, int(qCeil(xs.at(k) - 0.5)));
            const int xb = qMin(W - 1, int(qFloor(xs.at(k + 1) - 0.5)));
            if (xb < xa)
                continue;
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
                        double width, const QColor &colour, bool depthWrite)
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
    poly(pts, zs, 4, colour, depthWrite);
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
