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
}

#endif
