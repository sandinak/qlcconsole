/*
  Q Light Controller Plus
  linecurve.h

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

#ifndef LINECURVE_H
#define LINECURVE_H

#include <QVector3D>
#include <QVector>

/** @addtogroup engine Engine
 * @{
 */

/**
 * The curve of a line target (docs/design/LINE_TARGET_DESIGN.md): a smooth 3-D curve
 * through every control point -- centripetal Catmull-Rom, which passes
 * through each handle and does not loop or overshoot when points are
 * bunched -- measured by arc length, so "a fifth of the way along" is a
 * fifth of the distance along the curve and lights spread on it are evenly
 * spaced on the curve, not along X. Two points are a straight line.
 *
 * Pure geometry: no Doc, no Qt widgets. Cheap to build (a few hundred
 * samples), so callers build one when they need it rather than caching.
 */
class LineCurve
{
public:
    explicit LineCurve(const QVector<QVector3D> &points, int samplesPerSpan = 48);

    /** At least two control points. */
    bool isValid() const { return m_points.size() >= 2; }

    QVector<QVector3D> controlPoints() const { return m_points; }

    /** Total length along the curve, in the points' units. */
    double length() const { return m_length; }

    /** The point @p fraction (0..1, clamped) of the way along the curve. */
    QVector3D pointAt(double fraction) const;

    /** Which span (control point i to i+1) the point @p fraction of the way
     *  along lies in: a new point there is inserted at index span + 1. */
    int spanAt(double fraction) const;

    /** The sampled curve, for drawing and hit-testing, and how far along
     *  (0..1) each sample is. */
    const QVector<QVector3D> &samples() const { return m_samples; }
    const QVector<double> &sampleFractions() const { return m_fractions; }

    /** The fraction of the way along at which the curve passes nearest @p p. */
    double nearestFraction(const QVector3D &p) const;

    /** The control points shifted so the curve's half-way point is at the
     *  origin; @p shift receives how far they moved (subtract it from the
     *  points, add it to whatever they are relative to). */
    static QVector<QVector3D> recentred(const QVector<QVector3D> &points, QVector3D *shift = nullptr);

private:
    QVector<QVector3D> m_points;
    QVector<QVector3D> m_samples;
    QVector<double> m_fractions;     ///< parallel to m_samples
    QVector<int> m_sampleSpan;       ///< parallel to m_samples
    double m_length = 0.0;
};

/** @} */

#endif
