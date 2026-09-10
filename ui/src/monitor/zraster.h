/*
  Q Light Controller Plus
  zraster.h

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

#ifndef ZRASTER_H
#define ZRASTER_H

#include <QVector>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QSize>

/** @addtogroup ui_monitor Monitor
 * @{
 */

/**
 * A depth-buffered software rasteriser for the rig view.
 *
 * WHY THIS EXISTS
 *
 * The rig view used to sort whole primitives by a single depth value and paint
 * them back to front. One depth cannot describe a polygon whose depth varies
 * across it, and every artefact in that view came from the same place: half of
 * a step blanking off-axis, only the nearest fixture inside a step showing, an
 * in-step unit painting over the tape outside it, truss webbing punching
 * through a deck. Each was patched with a layering epsilon. There were four.
 *
 * A depth buffer makes the whole family impossible instead of individually
 * fixable.
 *
 * WHY IT IS CHEAP
 *
 * Depth across a PLANAR polygon is an affine function of screen x,y: the rig
 * view's projection is orthographic and its depth function is linear in world
 * space, so there is no perspective divide anywhere. One plane fit per polygon
 * is therefore EXACT rather than an approximation, and the inner loop is an add
 * and a compare. Measured against the old queue at 1806 primitives: 0.248 ms
 * against 0.513 ms, and correct where the old one dropped half the strip.
 *
 * WHAT IT DELIBERATELY LEAVES ROOM FOR
 *
 * Haze, gobos and shadows all need exactly this and cannot be done without it:
 * screen-space volumetrics integrate along a view ray up to the scene depth at
 * each pixel; a shadow map is this same rasteriser run from a light's point of
 * view; a gobo is a texture projected through the transform that shadow map
 * already establishes. Hence @ref depthAt() and the ability to rasterise
 * depth-only from any projection -- they are the hooks those need.
 */
class ZRaster
{
public:
    ZRaster();

    /**
     * Start a frame.
     *
     * @param size        final image size
     * @param supersample 1 = none, 2 = render at double and scale down. A flat
     *                    rasteriser has no antialiasing of its own; this buys
     *                    it back for 4x the fill, measured at ~1.3 ms for a
     *                    full rig frame.
     * @param background  cleared colour
     */
    void begin(const QSize &size, int supersample, const QColor &background);

    /**
     * A filled convex polygon with per-vertex depth.
     *
     * @param depthWrite false for translucent surfaces: they must still be
     *                   depth-TESTED against what is already there, but must
     *                   not occlude each other, which is why they are drawn
     *                   after the opaque pass and back to front among
     *                   themselves. A depth buffer is order-independent only
     *                   for opaque geometry.
     */
    void poly(const QPointF *pts, const double *zs, int n,
              const QColor &fill, bool depthWrite = true);

    /** A line of finite width, as a quad with depth down its length. */
    void thickLine(const QPointF &a, const QPointF &b, double za, double zb,
                   double width, const QColor &colour, bool depthWrite = true);

    /** The finished image, downsampled if supersampling was asked for. */
    QImage resolve() const;

    /** Scene depth at a final-image pixel, or -infinity where nothing was
     *  drawn. Larger is nearer, matching the view's convention. This is what
     *  makes pixel-accurate hit-testing possible, and what volumetrics would
     *  integrate against. */
    double depthAt(const QPoint &p) const;

    bool isValid() const { return m_colour.isNull() == false; }

private:
    QImage         m_colour;
    QVector<float> m_depth;
    int            m_ss;
    QSize          m_size;
};

/** @} */

#endif
