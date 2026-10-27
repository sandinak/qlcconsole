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

#include "fixture.h"

class Doc;
class QVector3D;

// ---------------------------------------------------------------------------
// Fixture "what does this actually look like" classifier, shared by every
// renderer that draws a fixture as an icon (StructureStudioView's elevation
// editor, MonitorFixtureItem's 2D plan view). Reads only data ALREADY present
// in every fixture definition (Type, per-channel Group/Colour/Preset, Physical
// dimensions) -- no new fixture-def schema, so it applies retroactively to the
// whole existing library, not just fixtures someone hand-tags going forward.
// ---------------------------------------------------------------------------

enum class FixtureSilhouette { Bar, Mover, Par, Generic, Lantern };

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
     *  say 0 and dimming those by viewing angle would hide half a rig.
     *
     *  On a ZOOM fixture these are a RANGE, and where the head currently sits
     *  in it is a live DMX question -- see fixtureBeamAngle(). */
    float beamMinDeg = 0.0f;
    float beamMaxDeg = 0.0f;
    /** Separate beams each head throws (the definition's Lens BeamsPerHead).
     *  A two-arm beam fixture is two heads of three beams: the channels only
     *  ever say "two things are controlled", never how many emitters each
     *  one lights. */
    int   beamsPerHead = 1;
    /** Its light leaves all round (a lantern, a bulb): no cone, it lights
     *  what is near it, falling off with distance (QLCPhysical::emission). */
    bool  omni = false;
    bool  laser = false;            ///< pencil-thin beams (QLCPhysical::LaserEmission)
    bool  stars = false;            ///< a star field (QLCPhysical::StarEmission)
    bool  flame = false;            ///< a silk flame (QLCPhysical::FlameEmission)
    /** How its pool lands: 0 a profile's crisp edge ... 1 a wash fading out
     *  past its beam angle; a PC or Fresnel lens a little softer still
     *  (poolEdgeSoftness()). */
    float edgeSoftness = 1.0f;
    /** A colour ring round the lens (findColourRing()): its macro channel,
     *  -1 for none. A ring that is a head of its own is not a mover unit:
     *  it is left out of headCount. */
    int   ringChannel = -1;
    int   ringHead = -1;              ///< its head, -1 when it has none of its own
    /** The look the LD gave it (Fixture::stageLook()); AutoLook draws by
     *  kind as the definition suggests. Not AutoLook: kind already follows
     *  it (stageLookSilhouette()), and the 2D shapes come from
     *  stageLookBodyPath(). */
    Fixture::StageLook look = Fixture::AutoLook;
};

FixtureVisualTraits classifyFixture(Fixture *fx);

// ---------------------------------------------------------------------------
// Stage looks (Fixture::StageLook): the LD's word for what a fixture looks
// like, over what its definition's type suggests. Pure geometry, shared by
// the 2D Monitor (MonitorFixtureItem) and the studio's flat views
// (StructureStudioView), so a followspot is the same barrel in both.
// ---------------------------------------------------------------------------

/** The silhouette a fixture with @p look draws as; @p autoKind (what the
 *  definition says) for AutoLook. The moving-head looks are Movers; every
 *  other look is a Par -- one body, its own shape -- except a lantern (see
 *  below), and a staff or a bar
 *  wash that is already a Bar, which keeps its real footprint and heads.
 *  The Lantern look is a practical (a hurricane or porch lantern): it is
 *  FixtureSilhouette::Lantern, glowing all round like the LED lanterns,
 *  and classifyFixture() marks it omni. */
FixtureSilhouette stageLookSilhouette(Fixture::StageLook look, FixtureSilhouette autoKind);

/** The look @p fx is DRAWN with: its own stage look; on Auto, its closest
 *  look (Fixture::autoStageLook()) when its show draws Auto fixtures so
 *  (MonitorProperties::autoStageLooks()), else AutoLook. classifyFixture()
 *  puts this in FixtureVisualTraits::look. */
Fixture::StageLook drawnStageLook(const Fixture *fx);

/** True when @p fx's declared size is made up (a generic dimmer's 300 mm a
 *  channel), so a stage look's own proportions are used instead. */
bool stageLookIgnoresPhysical(const Fixture *fx);

/** True when @p look's lens is square (a square wash, a staff, a bar): its
 *  heads are drawn as squares, not circles. */
bool stageLookSquareLens(Fixture::StageLook look);

/** The body of a fixture with @p look, inside @p r: a long barrel for a
 *  followspot (along r's long axis), a thin pole for a staff (a dot from
 *  above), a wide strip for a bar wash, a square for a square wash, a
 *  hurricane lantern's hook, cap, glass and base (its base from above), a squat
 *  box for an uplight; a mover's base seen from above (an octagon for a
 *  spot, a circle for a round wash, a square for a square wash), or with
 *  @p elevation the yoke silhouette (moverElevationPath(), @p hung, one unit
 *  per head of @p units). Empty for AutoLook: draw by kind. */
QPainterPath stageLookBodyPath(Fixture::StageLook look, const QRectF &r, bool elevation = false,
                               int units = 1, bool hung = false);

/** Where the light comes out of a fixture with @p look in @p r -- the lens
 *  its heads are drawn in (the barrel's front, the whole length of a staff
 *  or a bar). A null rect for AutoLook and the movers: lay heads out as
 *  before. */
QRectF stageLookLensRect(Fixture::StageLook look, const QRectF &r, bool elevation = false);

/** What is drawn OVER a fixture's lit lens so its look still reads when the
 *  lens is all one colour: a moving spot's gobo, an LED PAR's grid of LEDs,
 *  a Fresnel's rings, a cyc's or a strobe's cells, the turns of a staff's
 *  LED strip round its pole (seen from the side; none end on). Thin, translucent dark
 *  strokes and dots, so the head's own colour and level show through. An
 *  empty path when its look has none. @p lens is the head's rect. */
QPainterPath stageLookLensHint(Fixture::StageLook look, const QRectF &lens);

/** A sensible on-screen size, in px, for a fixture with @p look whose
 *  definition declares no dimensions, from @p base (the size an ordinary
 *  Par of that view gets): long for a followspot or a bar wash, thin and
 *  tall for a staff -- or, with @p elevation false (seen from above),
 *  small and round for a staff or a lantern. @p base unchanged for AutoLook
 *  and the movers. */
QSizeF stageLookDefaultSize(Fixture::StageLook look, double base, bool elevation = true);

/** How dark stageLookLensHint()'s fill is (0..255): a staff's hint is the
 *  pole between its turns of LED strip, nearly opaque; the rest are marks
 *  the lens shows through. */
int stageLookLensHintAlpha(Fixture::StageLook look);

/** A mover's colour ring: a ring of LEDs round the lens, driven by one
 *  colour-macro channel of named colours and chases ("LED Movinghead+Circle":
 *  off, Red ... White, Red/Yellow-Chase ... RGB/Random-Chase).
 *
 *  Found as, in order:
 *   1. a head whose only colour is a colour wheel/macro channel, on a
 *      fixture that moves -- HeadKinds::typeOf() "Ring" (or "Macro" beside
 *      another head): the definition says so;
 *   2. in a definition that doesn't (the ring channel inside the spot's head,
 *      or in no head): a Colour channel named "Circle", "Ring" or "Halo";
 *   3. or a Colour channel after the fixture's first one whose slots
 *      include an off and a chase -- a wheel has neither.
 *  A fixture with none of these draws no ring. @p head gets the ring's own
 *  head (-1: none). Returns the channel, or -1. */
int findColourRing(const Fixture *fx, int *head = nullptr);

/** What a ring shows for slot @p slotName (a capability's name) at
 *  @p seconds, as @p segments colours round it (black: dark). Off is all
 *  dark; a solid colour all that colour; a chase ("Red/Yellow-Chase",
 *  "RGB/Random-Chase") its colours in alternate segments, stepping round
 *  four times a second (random: a fresh colour per segment each step).
 *  @p animated, when given, says whether it moves. */
QVector<QColor> colourRingSegments(const QString &slotName, double seconds,
                                   int segments = 12, bool *animated = nullptr);

/** The ring of @p fx from @p values (a snapshot), or an empty list when it
 *  has none or the channel is off the end of the values. */
QVector<QColor> colourRingLive(const Fixture *fx, const QByteArray &values, double seconds,
                               int segments = 12, bool *animated = nullptr);

/** How soft the edge of a fixture's pool is, from what its definition says
 *  about its optics: one that can sharpen or shape its beam -- a gobo wheel,
 *  a focus, an iris -- is a profile, with a crisp edge (0.1); so is a beam
 *  bar (LED Bar (Beams)). Anything else is a wash (1.0); a PC or Fresnel
 *  lens, which exist to soften the edge, a little more (1.25). A very
 *  narrow beam (under 12 degrees) stays fairly crisp whatever it is.
 *  Head count says nothing about optics: a pixel bar can be either. */
float poolEdgeSoftness(bool shapesItsBeam, int fixtureType, const QString &lensName,
                       double beamMaxDeg);

/**
 * Where a fixture actually IS, in world metres.
 *
 * NOT simply MonitorProperties::fixtureRigPosition(). The 2D plot stores a
 * free or deck-mounted fixture's XY as the TOP-LEFT of its icon, so handing
 * that straight to anything that means "the fixture's position" puts it half a
 * footprint left and upstage. A DERIVED position (truss, pipe, tower, riser,
 * studio frame) is already a centre and is returned unchanged.
 *
 * One definition, because there were nearly two: the rig view corrects the
 * anchor, and anything else that reports a position has to agree with it or the
 * views disagree about where the same fixture is all over again.
 */
QVector3D fixtureWorldCentre(Doc *doc, quint32 fid);

/** How fast a star laser's pattern turns, degrees a second, from its prism
 *  rotation channel: 0 = still. The 3D views and the 2D plot turn it alike. */
double starTurnRate(const Fixture *fx, const QByteArray &vals);

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
                      uchar &dimmer, bool valuesAreComplete = true);

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

/** The beam angle a fixture is CURRENTLY throwing, in degrees.
 *
 *  A fixed-lens head declares one angle and that is the answer. A ZOOM head
 *  declares a min and a max, and its zoom channel says where between them it
 *  is sitting right now -- taking the maximum regardless drew every zoom
 *  fixture at its widest whatever the desk was telling it to do.
 *
 *  Returns 0 when the definition declares no lens at all, which callers must
 *  read as "unknown", never as "zero degrees". */
double fixtureBeamAngle(Fixture *fx, const QByteArray &values,
                        const struct FixtureVisualTraits &traits);

/** Per-head, from caller-supplied values. Pass @a masterLevel from
 *  fixtureMasterLevel() when drawing many heads of one fixture; -2 means "work
 *  it out", which is correct but expensive in a loop. */
bool fixtureHeadLiveState(Fixture *fx, const QByteArray &values, int head,
                          QColor &colour, uchar &dimmer, int masterLevel = -2);

/** What ONE head of a multi-head mover is throwing: its own channels PLUS
 *  every channel that belongs to no head.
 *
 *  A mover's arms share the fixture-wide channels -- master dimmer, strobe,
 *  often a colour wheel -- and those are deliberately left out of every head
 *  (the engine only recognises a master dimmer that sits in no head). Reading
 *  a head's channels alone, as a pixel bar can, would miss a shut shutter and
 *  a wheel. A dimmer inside the head wins over the fixture master, the same
 *  rule fixtureHeadLiveState() applies.
 *
 *  @return false if there is no such head. */
bool fixtureHeadBeamState(Fixture *fx, const QByteArray &values, int head,
                          QColor &colour, uchar &dimmer,
                          bool valuesAreComplete = true);


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
 *  @p head picks which head's pan and tilt to read; arms that move on their
 *  own point different ways. A head with no pan or tilt of its own (a shared
 *  yoke) falls back to the first head.
 *
 *  @return false when the fixture has no pan or tilt to read, so callers can
 *          leave the head in its rest position. */
bool fixtureAimDirection(Fixture *fx, const struct FixtureRigProps &rp, QVector3D &dir,
                         int head = 0);

/** Focus mode: while a session is aiming @p fid, where it is MEANT to point --
 *  at its reference, with no calibration and no nudge in it -- as a world
 *  direction (FocusSession::intendedDirection()). Every rig view draws this for
 *  the head being focused, in place of reading its DMX back, so the beam stays
 *  on its mark while the real head is nudged. fixtureAimDirection() answers it
 *  itself; the views that would otherwise draw a selected scene's aim ask
 *  first. False when no session is aiming @p fid. */
bool focusIntentDirection(Doc *doc, quint32 fid, QVector3D &dir);

/** The same as the plot's gauges read it: pan and tilt relative to home, as
 *  AimSolver::intendedAngles() gives them (FocusSession::intendedAngles()). */
bool focusIntentAngles(Doc *doc, quint32 fid, double &panRelDeg, double &tiltRelDeg);

/** Scramble practice (FOCUS_REFINE_DESIGN.md section 13): with a practice
 *  venue applied, @p fid's pan and tilt offsets in @p rp become its RIGHT
 *  ones, so a view turning its DMX back into a direction draws where the
 *  scrambled light really lands -- off its mark until it is focused back.
 *  False (and @p rp untouched) for any other light, or no practice. */
bool practiceViewProps(Doc *doc, quint32 fid, struct FixtureRigProps &rp);

/** Where the running focus session is aiming @p fid -- its reference point --
 *  for the aim trace. False when no session is aiming @p fid. */
bool focusIntentPoint(Doc *doc, quint32 fid, QVector3D &point);

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
