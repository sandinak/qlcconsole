/*
  Q Light Controller Plus
  righeads.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef RIGHEADS_H
#define RIGHEADS_H

#include <QVector>
#include <QVector3D>

class Doc;
class Fixture;
class QByteArray;
struct FixtureRigProps;

/** Where a fixture and each of its heads are on the stage, in world metres.
 *
 *  The rig view draws from these and an effect lays its grid out from them,
 *  so the pixel an effect lights is the pixel the rig view shows lit. They
 *  lived in the rig view alone; a second copy for effects would have drifted. */
namespace RigHeads
{
    /** Unit vector along a fixture's length, from its face and angle. */
    QVector3D axis(const FixtureRigProps &rp);

    /** The fixture's own orthonormal frame: @p L along its length, @p N out of
     *  the surface it is mounted on, @p H the remaining in-face axis -- tilt
     *  (roll about L) and flip included. */
    void frame(const FixtureRigProps &rp, QVector3D &L, QVector3D &N, QVector3D &H);

    /** A fixture's world CENTRE. A derived position (truss, pipe, tower,
     *  riser, studio frame) already is one; a free or deck-mounted fixture's
     *  stored XY is the TOP-LEFT of its plot icon, so it is moved by half its
     *  declared width and height. */
    QVector3D fixtureCentre(Doc *doc, quint32 fid);

    /** Each head's position, indexed by head: where the rig view paints that
     *  head on the fixture's face -- its cell in the declared pixel grid (or
     *  the show's head map of that size), else one row along the length. An
     *  empty vector when the fixture is unknown. */
    QVector<QVector3D> headPositions(Doc *doc, quint32 fid);

    /** How many LEDs make a fixture's light: 0 when it is one point. The
     *  fixture's own setting wins (FixtureRigProps::lightLeds), then what the
     *  device itself reports (@p deviceLeds, 0 = not known -- a WLED says how
     *  many it drives), then the definition (QLCPhysical::leds()). A definition
     *  that names no LEDs is a point unless the fixture says otherwise.
     *  @p layout, when given, gets their QLCPhysical::LedLayout. */
    int lightLeds(Doc *doc, quint32 fid, int deviceLeds = 0, int *layout = nullptr);

    /** Where each of those LEDs is (world metres), the first first -- in the
     *  body the rig view draws: a hanging cluster spread through it, or tape
     *  wound round a core in its middle. Empty when its light is one point. */
    QVector<QVector3D> lightPoints(Doc *doc, quint32 fid, int deviceLeds = 0);

    /** Whether its light is seen through clear glass (each LED seen) rather
     *  than diffusing glass (the default: it glows) -- the fixture's own
     *  setting (FixtureRigProps::lightGlass), else the definition's. */
    bool clearGlass(Doc *doc, quint32 fid);

    /** The full angle of @p fx's beam right now, in degrees, from its channel
     *  @p values: the lens its zoom channel has it at (the definition's
     *  narrowest to widest -- which way the channel runs is read, both are
     *  common), narrowed by an iris as far as it is closed. A fixed lens is its
     *  one angle; a range with no zoom channel, the widest. 0 = no lens
     *  declared at all. One rule for the views that draw the beam and the
     *  follow spot that aims it. */
    double beamAngle(const Fixture *fx, const QByteArray &values);
    /** True when the fixture's mode has a zoom channel. */
    bool hasZoomChannel(const Fixture *fx);
    /** The definition's lens range, degrees; false when it has none to
     *  choose in (a fixed cone, or no lens declared). */
    bool lensRange(const Fixture *fx, double &lo, double &hi);
    /** What a lens zoomed by hand was set to (FixtureRigProps::lensDeg),
     *  held to lensRange(); 0 when not set or there is no range. */
    double setLensAngle(const Fixture *fx);
}

#endif
