/*
  Q Light Controller Plus
  pixelpath.h

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

#ifndef PIXELPATH_H
#define PIXELPATH_H

#include <QVector>
#include <QVector3D>
#include <QString>

class MonitorProperties;
class StageTarget;
struct FixtureRigProps;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Where a pixel fixture's LEDs are when they are not where its body is: a
 * WLED strip round a fountain's wall, up its flame, or wound round a wand
 * (docs/design/FOUNTAIN_WAND.md). The LEDs go along the path in head order.
 */
namespace PixelPath
{
    enum Shape
    {
        None = 0,
        InnerWallTop,   ///< round a fountain's inner wall, on its top
        OuterWallTop,   ///< round its outer wall, on its top
        BaseRing,       ///< round the edge of its base
        FlameSpiral,    ///< wound up its flame
        Wand            ///< a staff, wound round it
    };
    const int kShapes = int(Wand) + 1;

    /** Its name for a menu ("Inner wall top ring"). */
    QString name(int shape);
    /** The paths that need a fountain. */
    inline bool needsFountain(int shape) { return shape >= InnerWallTop && shape <= FlameSpiral; }

    /** @p n points evenly spaced by length along @p line; a closed line
     *  runs back to its first point. Open: the middles of n equal stretches. */
    QVector<QVector3D> along(const QVector<QVector3D> &line, int n, bool closed = false);

    /** @p n points on a helix of @p turns turns round the line from @p from
     *  to @p to at @p radius, the first at @p from, the last at @p to. */
    QVector<QVector3D> helix(const QVector3D &from, const QVector3D &to, double radius,
                             double turns, int n);

    /** A wand's staff radius and LED spacing when they aren't set. */
    const double kWandRadius = 0.0125;
    const double kLedSpacing = 0.02;
    /** Turns of a strip wound along a staff @p length long: @p strip metres
     *  of it (0 = @p n LEDs at kLedSpacing) round a staff @p diameter across
     *  (0 = kWandRadius's). At least half a turn. */
    double autoWandTurns(int n, double length, double strip = 0.0, double diameter = 0.0);
    /** How far up a wand its string starts: a hand's width above the butt. */
    const float kWandGrip = 0.08f;
    /** The turns a wand of @p n LEDs is drawn with: its set turns, else Auto
     *  from its string length and diameter along the staff above the grip. */
    double wandTurns(const FixtureRigProps &rp, int n);
    /** Turns for a spiral up a flame @p height high, @p n LEDs. */
    double autoSpiralTurns(int n, double height);

    /** Where a wand held by @p person has its middle (metres): holdHeight
     *  above them, a hand's reach downstage. */
    QVector3D heldCentre(const MonitorProperties *props, const StageTarget *person, const FixtureRigProps &rp);
    /** A wand's two ends, as it is drawn: held (leaning holdTilt forward,
     *  centred on heldCentre()) or standing where it was placed. */
    bool wandEnds(MonitorProperties *props, quint32 fid, QVector3D &bottom, QVector3D &top);

    /** The world positions of @p n LEDs of @p fid along its pixel path, in
     *  head order; empty when it has none (or its fountain is gone). */
    QVector<QVector3D> positions(const Doc *doc, MonitorProperties *props, quint32 fid, int n);
}

/** @} */

#endif
