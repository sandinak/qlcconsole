/*
  Q Light Controller Plus
  fixturevisualtraits.h

  Copyright (c) Massimo Callegari

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

#ifndef FIXTUREVISUALTRAITS_H
#define FIXTUREVISUALTRAITS_H

#include <QPainterPath>
#include <QRectF>

class Fixture;

// ---------------------------------------------------------------------------
// Fixture "what does this actually look like" classifier, shared by every
// renderer that draws a fixture as an icon (StructureStudioView's elevation
// editor, MonitorFixtureItem's 2D plan view). Reads only data ALREADY present
// in every fixture definition (Type, per-channel Group/Colour/Preset, Physical
// dimensions) -- no new fixture-def schema, so it applies retroactively to the
// whole existing library, not just fixtures someone hand-tags going forward.
// ---------------------------------------------------------------------------

enum class FixtureSilhouette { Bar, Mover, Par, Generic };

#include <QSize>
#include <QColor>
#include <QVector3D>

struct FixtureVisualTraits
{
    FixtureSilhouette kind = FixtureSilhouette::Generic;
    bool  hasFocus = false;         ///< a Beam-group Focus/Zoom channel exists
    bool  rgbw = false;             ///< colour-mixing (2+ of R/G/B/W) vs a wheel
    bool  hasWheel = false;         ///< a Colour-group channel with no single primary colour
    int   headCount = 1;
    bool  multiHeadPanTilt = false; ///< 2+ heads, each with its own Pan or Tilt channel
    float physW = 0.0f, physH = 0.0f, physD = 0.0f;  ///< metres; 0 = not declared
    /** The definition's declared pixel grid (the Physical tab's "Layout
     *  (Columns x Rows)" -- an XL-450 says 15 x 5). QSize() when the
     *  definition does not declare one (layoutSize() defaults to 1x1) or when
     *  it cannot hold this mode's heads. Columns run along the fixture's LONG
     *  axis, rows across its height. */
    QSize layout;
    /** Beam angle in degrees from the definition's <Lens>. 0 = not declared,
     *  which has to mean "no idea, show it from anywhere" -- most definitions
     *  say 0 and dimming those by viewing angle would hide half a rig. */
    float beamDeg = 0.0f;
};

FixtureVisualTraits classifyFixture(Fixture *fx);

/** A fixture's LIVE look, from the DMX values it is being sent right now.
 *
 *  One colour and one level for the whole fixture, which is what a rig-wide
 *  view needs -- MonitorFixtureItem's per-head computeColor()/computeAlpha()
 *  work off channel-index lists it builds for itself, and are not reusable
 *  without dragging that whole structure along.
 *
 *  @param colour  the emitted colour; untouched when the fixture has no colour
 *                 channels at all (a plain dimmer keeps whatever it was given).
 *  @param dimmer  0..255 overall level.
 *  @return false when the fixture has nothing to say (no def, no channels), so
 *          callers can fall back to their static appearance. */
bool fixtureLiveState(Fixture *fx, QColor &colour, uchar &dimmer);

/** The same, from a caller-supplied set of channel values rather than from the
 *  fixture itself. A renderer walking a whole rig takes ONE snapshot up front,
 *  so a frame shows a single instant instead of tearing across the stage as
 *  the engine keeps writing underneath it. */
bool fixtureLiveState(Fixture *fx, const QByteArray &values, QColor &colour,
                      uchar &dimmer);

/** The same, for ONE head of a multi-head fixture.
 *
 * A pixel bar is not one colour. Reducing all 64 heads to a single colour by
 * taking the per-primary maximum turns any pattern that is not uniform into a
 * pale wash -- red pixels and blue pixels together report max-red AND
 * max-blue. Drawing each head from its own channels is what makes a step
 * front show what it is actually doing.
 *
 * A head that carries no dimmer of its own is scaled by the fixture-wide
 * master, which is the usual arrangement for a pixel bar.
 *
 * @return false if there is no such head, or it has no channels. */
bool fixtureHeadLiveState(Fixture *fx, int head, QColor &colour, uchar &dimmer);
/** Per-head, from caller-supplied values -- see the note above. */
/** The fixture-wide master intensity level, or -1 if it has none.
 *
 *  This is a walk of every channel the fixture has, so a caller drawing ALL of
 *  a bar's heads must do it ONCE and hand the answer to
 *  fixtureHeadLiveState(). Recomputing it per head cost 49 ms of a 90 ms frame
 *  on a rig of 64-pixel bars. */
int fixtureMasterLevel(Fixture *fx, const QByteArray &values);

/** Per-head, from caller-supplied values. Pass @a masterLevel from
 *  fixtureMasterLevel() when drawing many heads of one fixture; -2 means "work
 *  it out", which is correct but expensive in a loop. */
bool fixtureHeadLiveState(Fixture *fx, const QByteArray &values, int head,
                          QColor &colour, uchar &dimmer, int masterLevel = -2);


/** Where a moving head is currently POINTING, in world space, from the pan and
 *  tilt it is being driven with right now.
 *
 *  Conventions, all of them already established elsewhere and repeated here
 *  because getting one backwards is invisible until someone looks at a rig:
 *    - pan 0 (the centre DMX value) faces DOWNSTAGE, and @c panZeroDir turns
 *      that clockwise seen from above -- see FixtureRigProps::panZeroDir.
 *    - +Y is downstage and +X is stage left (see barFaceVector() in
 *      monitorproperties.cpp), so a clockwise quarter turn from downstage
 *      points stage right, which is -X.
 *    - tilt 0 points straight DOWN, the home of a hung mover, and tilt swings
 *      the beam up toward whatever pan is facing.
 *
 *  @return false when the fixture has no pan or tilt to read, so callers can
 *          leave the head in its rest position. */
bool fixtureAimDirection(Fixture *fx, const struct FixtureRigProps &rp, QVector3D &dir);

// ---------------------------------------------------------------------------
// Shared Mover silhouettes -- pure geometry (a rect in, a path out), so
// StructureStudioView's elevation editor and MonitorFixtureItem's 2D canvas
// draw and hit-test the IDENTICAL shape for a Mover instead of two
// independently-drawn approximations of "what a moving head looks like".
// ---------------------------------------------------------------------------

/** A yoke-mount "compact head unit" silhouette: a base block flush with the
 *  mounting surface, two arms rising from it, and the head suspended between
 *  them -- what a moving head/scanner looks like from the Front or Side.
 *  $hung mirrors it (base at the TOP, arms/head hanging below) for a
 *  TopHung structural mount; false = sitting (base at the bottom, arms/head
 *  rising above it). $headCount > 1 draws one full unit per head, each
 *  centred in its own equal slice of $r's width, for a genuinely multi-head
 *  fixture (e.g. a twin-head unit) instead of one unit stretched across the
 *  whole footprint. */
QPainterPath moverElevationPath(const QRectF &r, bool hung = false, int headCount = 1);

/** The Mover silhouette as seen from directly above: a round head with a
 *  flat chord on its local "front" edge (where the beam exits) inset in a
 *  square base footprint (the yoke's footprint from above) -- "circles in a
 *  square", not a plain oval. $headCount > 1 draws one full unit per head,
 *  each centred in its own equal slice of $r's width -- see
 *  moverElevationPath(). */
QPainterPath moverPlanPath(const QRectF &r, int headCount = 1);

#endif // FIXTUREVISUALTRAITS_H
