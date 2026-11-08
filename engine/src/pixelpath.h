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
struct StringAnchor;
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
        SprinkleLoops,  ///< a fairy string looped across a fountain's gap
        FreeString      ///< a fairy string laid anywhere, through its anchors
    };
    const int kShapes = int(FreeString) + 1;

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
     *  (info.leds.count, 0 = not known). @p reverse, when it has a flag for
     *  each span (in LED order), says which are reversed instead of every
     *  second one: a free string's own Reverse per span. */
    bool sprinkleSpansToWled(const QVector<SprinkleSpan> &spans, const QByteArray &state,
                             int maxSegments, int deviceLeds, QJsonObject &request,
                             QString *error = nullptr,
                             const QVector<bool> &reverse = QVector<bool>());
    /** What a WLED's /json/state reply @p state has otherwise than the
     *  segments sprinkleSpansToWled() writes for @p spans, one line each;
     *  empty when it matches. @p reverse as there. */
    QStringList wledSegmentsDiffer(const QVector<SprinkleSpan> &spans, const QByteArray &state,
                                   const QVector<bool> &reverse = QVector<bool>());

    /** Free string (anchors): the gap between its LEDs, metres -- its String
     *  length (rp.wandStripLength, first LED to last) shared out over the
     *  @p n - 1 gaps, or with none set, LED tape (kLedSpacing). */
    double freeStringSpacing(const FixtureRigProps &rp, int n);
    /** How long a string of @p n LEDs is, first LED to last. */
    double freeStringLength(const FixtureRigProps &rp, int n);
    /** Where a new free string goes before it has anchors: two, a little in
     *  front of (downstage of) @p at, the fixture's own place, at its height,
     *  @p length apart across the stage (at least a foot). */
    QVector<StringAnchor> defaultStringAnchors(const QVector3D &at, double length);
    /** The anchors a free string is laid through: its own, or while it has
     *  fewer than two, defaultStringAnchors() from @p at (one anchor: that one
     *  and a string length on from it). */
    QVector<StringAnchor> freeStringAnchors(const FixtureRigProps &rp, const QVector3D &at, int n);
    /** @p n LEDs along the line through @p anchors, LED 1 first, @p spacing
     *  apart by length along it. LED 1 sits at the first anchor. An anchor
     *  that pins an LED holds that LED there; the LEDs between two pins are
     *  spread evenly between them (along the line). Before the first pin and
     *  after the last the LEDs keep @p spacing: a string shorter than its
     *  anchors stops where it runs out, one longer runs on straight past the
     *  last anchor. Pins that do not rise along the string are ignored
     *  (checkStringAnchors() says why). */
    QVector<QVector3D> freeStringPositions(const QVector<StringAnchor> &anchors, int n, double spacing);
    /** One stretch of a free string, anchor to anchor, by its LEDs. */
    struct FreeSpan
    {
        int  anchor = 0;        ///< the anchor it starts at (0-based)
        int  firstLed = 1;      ///< 1-based, inclusive
        int  lastLed = 1;
        bool reverse = false;   ///< its anchor's Reverse
    };
    /** Which LEDs lie on each span between two anchors, as
     *  freeStringPositions() lays them (LEDs before the first anchor count
     *  with the first span, past the last with the last). A span no LED
     *  lies on is left out. */
    QVector<FreeSpan> freeStringSpans(const QVector<StringAnchor> &anchors, int n, double spacing);
    /** The free string's spans as WLED segments: @p spans for
     *  sprinkleSpansToWled(), @p reverse each one's Reverse. */
    void freeSpansAsSegments(const QVector<FreeSpan> &free, QVector<SprinkleSpan> &spans,
                             QVector<bool> &reverse);
    /** How long the line through @p anchors is, metres. */
    double anchorLineLength(const QVector<StringAnchor> &anchors);
    /** What is wrong with @p anchors' pins for a string of @p leds LEDs (0 =
     *  not known), in words; empty when they are fine. */
    QString checkStringAnchors(const QVector<StringAnchor> &anchors, int leds);
    /** The compact form the show file keeps ("1.000,2.000,0.500,0,0;..."):
     *  x, y, height (metres), pinned LED (0 none), reversed (0 or 1). */
    QString stringAnchorsToString(const QVector<StringAnchor> &anchors);
    QVector<StringAnchor> stringAnchorsFromString(const QString &text);

    /** The world positions of @p n LEDs of @p fid along its pixel path, in
     *  head order; empty when it has none (or its fountain is gone). */
    QVector<QVector3D> positions(const Doc *doc, MonitorProperties *props, quint32 fid, int n);
}

/** @} */

#endif
