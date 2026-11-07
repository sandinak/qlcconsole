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
#include <QStringList>
#include <QString>

class QJsonObject;
class MonitorProperties;
class StageTarget;
struct FixtureRigProps;
struct SprinkleSpan;
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
        Wand,           ///< a staff, wound round it
        SprinkleLoops   ///< a fairy string looped across a fountain's gap
    };
    const int kShapes = int(SprinkleLoops) + 1;

    /** Its name for a menu ("Inner wall top ring"). */
    QString name(int shape);
    /** The paths that need a fountain. */
    inline bool needsFountain(int shape)
    {
        return (shape >= InnerWallTop && shape <= FlameSpiral) || shape == SprinkleLoops;
    }

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

    /** Sprinkle loops: the spans a string of @p n LEDs is hung in, in LED
     *  order -- its table (clamped to @p n, overlaps trimmed) or, with none,
     *  rp.sprinklePoints places evenly round the U with
     *  rp.sprinkleSpansPerPoint spans each and the LEDs shared out equally. */
    QVector<SprinkleSpan> sprinkleLayout(const FixtureRigProps &rp, int n);
    /** How far the middle of a span of @p leds LEDs hangs below the straight
     *  line between its two wall tops, @p chord apart (metres, measured
     *  straight between them): 30% of the chord for a span of
     *  the string's average size (@p meanLeds), more for a longer one and
     *  less for a shorter, held to 10%..100% of the chord. */
    double sprinkleSag(double chord, int leds, double meanLeds);
    /** Spans along the U at one place are this far apart (metres). */
    const double kSprinklePitch = 0.05;

    /** What is wrong with a span table for a string of @p leds LEDs (0 = not
     *  known), in words; empty when it is fine. */
    QString checkSprinkleSpans(const QVector<SprinkleSpan> &spans, int leds);
    /** The compact form the show file keeps ("1-30@0.083;31-60@0.083"). */
    QString sprinkleSpansToString(const QVector<SprinkleSpan> &spans);
    QVector<SprinkleSpan> sprinkleSpansFromString(const QString &text);
    /** A WLED's /json/state (or /json) reply as spans: one per segment
     *  (start, stop exclusive), in LED order, grouped @p spansPerPoint to a
     *  place; @p points gets how many places. @p notReversed lists the
     *  segments (1-based) whose Reverse is not what makes every span run
     *  inner to outer (every second one ticked). False, with @p error, when
     *  it is not a reply with segments. */
    bool sprinkleSpansFromWled(const QByteArray &json, int spansPerPoint,
                               QVector<SprinkleSpan> &spans, int &points,
                               QString *error = nullptr, QList<int> *notReversed = nullptr);
    /** Write Segments, the reverse: the /json/state request that makes a
     *  WLED's segments @p spans -- one segment a span (start = firstLed - 1,
     *  stop = lastLed, exclusive), Reverse on every second one so every span
     *  runs inner to outer, the LEDs between spans in none (dark), and each
     *  segment the device has now (@p state, its /json/state reply) past the
     *  spans cleared ("stop":0). Refused (false, @p error in words, nothing
     *  in @p request) when there are no spans, more than @p maxSegments
     *  (info.leds.maxseg, 0 = not known), or one runs past @p deviceLeds
     *  (info.leds.count, 0 = not known). */
    bool sprinkleSpansToWled(const QVector<SprinkleSpan> &spans, const QByteArray &state,
                             int maxSegments, int deviceLeds, QJsonObject &request,
                             QString *error = nullptr);
    /** What a WLED's /json/state reply @p state has otherwise than the
     *  segments sprinkleSpansToWled() writes for @p spans, one line each;
     *  empty when it matches. */
    QStringList wledSegmentsDiffer(const QVector<SprinkleSpan> &spans, const QByteArray &state);

    /** The world positions of @p n LEDs of @p fid along its pixel path, in
     *  head order; empty when it has none (or its fountain is gone). */
    QVector<QVector3D> positions(const Doc *doc, MonitorProperties *props, quint32 fid, int n);
}

/** @} */

#endif
