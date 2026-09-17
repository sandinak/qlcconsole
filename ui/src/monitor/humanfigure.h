/*
  Q Light Controller Plus
  humanfigure.h

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

#ifndef HUMANFIGURE_H
#define HUMANFIGURE_H

#include <QVector>

/** One horizontal cross-section of a body part: an ellipse at height z,
 *  centred at (cx, cy) with half-widths (rx, ry). Every value is a fraction
 *  of standing height; +x is stage left, +y downstage -- the way a person on
 *  the stage faces the house. */
struct FigureRing
{
    float z, cx, cy, rx, ry;
};

/** A body part as a stack of cross-sections, bottom to top. */
typedef QVector<FigureRing> FigureLoft;

/** A standing person as lofts: two legs, the torso, the neck, the head and
 *  two arms.
 *
 *  Shared by the rig view, which builds a shaded mesh from it, and the plot,
 *  which draws its outline, so the two draw the same body. Boxes read as a
 *  robot at any distance; a handful of rounded, tapering sections is enough to
 *  read as a person from across the room, which is all a rig needs. */
const QVector<FigureLoft> &humanFigureLofts();

#endif
