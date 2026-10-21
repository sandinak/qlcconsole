/*
  Q Light Controller Plus
  linecurve.cpp

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

#include "linecurve.h"

namespace
{
/* The knot step of centripetal Catmull-Rom: the square root of the distance.
   Never zero, so two points on top of each other do not divide by it. */
double knotStep(const QVector3D &a, const QVector3D &b)
{
    return qMax(1e-4, std::sqrt(double((b - a).length())));
}

QVector3D lerp(const QVector3D &a, const QVector3D &b, double t0, double t1, double t)
{
    const double d = t1 - t0;
    if (std::fabs(d) < 1e-12)
        return a;
    return a * float((t1 - t) / d) + b * float((t - t0) / d);
}

/* One span, p1 to p2, by the Barry-Goldman pyramid. */
QVector3D catmullRom(const QVector3D &p0, const QVector3D &p1,
                     const QVector3D &p2, const QVector3D &p3, double u)
{
    const double t0 = 0.0;
    const double t1 = t0 + knotStep(p0, p1);
    const double t2 = t1 + knotStep(p1, p2);
    const double t3 = t2 + knotStep(p2, p3);
    const double t = t1 + (t2 - t1) * u;

    const QVector3D a1 = lerp(p0, p1, t0, t1, t);
    const QVector3D a2 = lerp(p1, p2, t1, t2, t);
    const QVector3D a3 = lerp(p2, p3, t2, t3, t);
    const QVector3D b1 = lerp(a1, a2, t0, t2, t);
    const QVector3D b2 = lerp(a2, a3, t1, t3, t);
    return lerp(b1, b2, t1, t2, t);
}
}

LineCurve::LineCurve(const QVector<QVector3D> &points, int samplesPerSpan)
    : m_points(points)
{
    if (m_points.size() < 2)
    {
        if (m_points.size() == 1)
        {
            m_samples << m_points.first();
            m_fractions << 0.0;
            m_sampleSpan << 0;
        }
        return;
    }

    const int n = m_points.size();
    const int per = qMax(2, samplesPerSpan);
    /* The ends continue straight on: a phantom point mirrored through each
       end, so the curve leaves an end heading at its neighbour. */
    auto at = [this, n](int i) -> QVector3D {
        if (i < 0)
            return m_points.at(0) * 2.0f - m_points.at(1);
        if (i >= n)
            return m_points.at(n - 1) * 2.0f - m_points.at(n - 2);
        return m_points.at(i);
    };

    QVector<double> lengths;
    m_samples.reserve((n - 1) * per + 1);
    m_samples << m_points.first();
    m_sampleSpan << 0;
    lengths << 0.0;
    for (int s = 0; s < n - 1; ++s)
    {
        const QVector3D p0 = at(s - 1), p1 = at(s), p2 = at(s + 1), p3 = at(s + 2);
        for (int k = 1; k <= per; ++k)
        {
            const QVector3D q = (k == per) ? p2 : catmullRom(p0, p1, p2, p3, double(k) / per);
            lengths << lengths.last() + double((q - m_samples.last()).length());
            m_samples << q;
            m_sampleSpan << s;
        }
    }
    m_length = lengths.last();
    m_fractions.resize(lengths.size());
    for (int i = 0; i < lengths.size(); ++i)
        m_fractions[i] = (m_length > 0.0) ? lengths.at(i) / m_length : double(i) / (lengths.size() - 1);
}

QVector3D LineCurve::pointAt(double fraction) const
{
    if (m_samples.isEmpty())
        return QVector3D();
    if (m_samples.size() == 1)
        return m_samples.first();
    const double f = qBound(0.0, fraction, 1.0);
    // The first sample at or past f.
    const auto it = std::lower_bound(m_fractions.constBegin(), m_fractions.constEnd(), f);
    int hi = int(it - m_fractions.constBegin());
    if (hi <= 0)
        return m_samples.first();
    if (hi >= m_samples.size())
        return m_samples.last();
    const int lo = hi - 1;
    const double span = m_fractions.at(hi) - m_fractions.at(lo);
    const double u = span > 0.0 ? (f - m_fractions.at(lo)) / span : 0.0;
    return m_samples.at(lo) + (m_samples.at(hi) - m_samples.at(lo)) * float(u);
}

int LineCurve::spanAt(double fraction) const
{
    if (m_points.size() < 2)
        return 0;
    const double f = qBound(0.0, fraction, 1.0);
    const auto it = std::lower_bound(m_fractions.constBegin(), m_fractions.constEnd(), f);
    const int i = qBound(0, int(it - m_fractions.constBegin()), m_sampleSpan.size() - 1);
    return qBound(0, m_sampleSpan.at(i), m_points.size() - 2);
}

double LineCurve::nearestFraction(const QVector3D &p) const
{
    if (m_samples.size() < 2)
        return 0.0;
    double best = -1.0, bestF = 0.0;
    for (int i = 0; i + 1 < m_samples.size(); ++i)
    {
        const QVector3D a = m_samples.at(i), b = m_samples.at(i + 1);
        const QVector3D ab = b - a;
        const double len2 = double(QVector3D::dotProduct(ab, ab));
        double u = len2 > 0.0 ? double(QVector3D::dotProduct(p - a, ab)) / len2 : 0.0;
        u = qBound(0.0, u, 1.0);
        const double d = double((a + ab * float(u) - p).lengthSquared());
        if (best < 0.0 || d < best)
        {
            best = d;
            bestF = m_fractions.at(i) + (m_fractions.at(i + 1) - m_fractions.at(i)) * u;
        }
    }
    return bestF;
}

QVector<QVector3D> LineCurve::recentred(const QVector<QVector3D> &points, QVector3D *shift)
{
    const QVector3D mid = LineCurve(points).pointAt(0.5);
    if (shift != nullptr)
        *shift = mid;
    QVector<QVector3D> out = points;
    for (QVector3D &p : out)
        p -= mid;
    return out;
}
