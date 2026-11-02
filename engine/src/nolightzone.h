/*
  Q Light Controller Plus - qlcconsole
  nolightzone.h

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

#ifndef NOLIGHTZONE_H
#define NOLIGHTZONE_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <climits>

#include "qlcengine_export.h"

class QLCPhysical;
class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

/**
 * An area of the plot that must stay dark: the orchestra pit, the
 * conductor, a camera position, the audience's eyes. A floor rectangle
 * (stage metres, +X stage left, +Y downstage) from @c floorZ up @c height
 * metres. Show Review (PWR-11) asks whether a lit beam's cone crosses it.
 *
 * Saved in the show, with the rest of the plot (MonitorProperties); not in
 * the venue file, which carries only the power today -- decided (can change).
 */
struct QLC_ENGINE_EXPORT NoLightZone
{
    /** What the zone keeps out. NoLight: every lit beam. NoBeams: the
     *  directional beams -- a lens narrower than kNarrowBeamDeg -- and
     *  lasers (not a wash, which only fills). NoLasers: lasers and star
     *  projectors. Decided (can change). */
    enum Kind { NoLight = 0, NoBeams = 1, NoLasers = 2 };
    static constexpr double kNarrowBeamDeg = 25.0;

    quint32 id = 0;
    QString name;
    float x = 0.0f, y = 0.0f;           ///< the upstage-stage-right corner of the floor rectangle
    float width = 2.0f, depth = 2.0f;   ///< along X, along Y
    float floorZ = 0.0f;                ///< the bottom of the zone (a pit is below the stage)
    float height = 2.5f;                ///< how far up it reaches from floorZ
    int kind = NoLight;

    static QString kindToString(int kind);
    static int stringToKind(const QString &s);
    static QString kindLabel(int kind);

    /** Whether a light of this kind is kept out of it. @a laser: a laser or
     *  star projector; @a coneDegrees: the full beam angle. */
    bool applies(bool laser, double coneDegrees) const;

    bool contains(const QVector3D &p) const;
    /** Whether the ray from @a p along unit @a d enters it within @a range
     *  metres (it stops at the stage floor, z = 0, when it comes down, so
     *  a pit zone has to reach up to it). */
    bool hitByRay(const QVector3D &p, const QVector3D &d, double range) const;
    /** Whether any of @a rays from @a p crosses it. */
    bool hitByRays(const QVector3D &p, const QVector<QVector3D> &rays, double range) const;

    void saveXML(QXmlStreamWriter *w) const;
    /** Reads one <NoLightZone> element (leaves the reader at its end). */
    static bool loadXML(QXmlStreamReader &r, NoLightZone &z);
};

/** The rays Show Review follows for a beam (PWR-09's sampling, shared). */
namespace LightRays
{
    /** The axis plus three rings of sixteen rays across a cone of half
     *  angle @a halfRad: the rays that matter are often between the axis
     *  and the edge. */
    QLC_ENGINE_EXPORT QVector<QVector3D> cone(const QVector3D &dir, double halfRad);
    /** The same for a laser's rectangular scan window: @a hDeg wide and
     *  @a vDeg tall (full angles), @a dir through its middle, wide edge
     *  horizontal. The axis, the corners, the edge midpoints and a grid
     *  between. */
    QLC_ENGINE_EXPORT QVector<QVector3D> window(const QVector3D &dir, double hDeg, double vDeg);
    /** A laser's scan window, full angles. Decided (can change): H is the
     *  definition's widest lens angle (a 40 degree fan when it declares
     *  none), V its narrowest when that is declared and narrower, else the
     *  same as H -- the definition has no separate vertical angle. */
    QLC_ENGINE_EXPORT void laserWindow(const QLCPhysical &phy, double &hDeg, double &vDeg);
}

/** @} */

#endif
