/*
  Q Light Controller Plus
  aimsolver.h

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

#ifndef AIMSOLVER_H
#define AIMSOLVER_H

#include <QVector3D>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Single source of truth for "what pan/tilt aims a fixture's beam at a world
 * point". BOTH the Aim palette (design preview) and the follow-spot effect's
 * pre-computed aim use this, so a fixture points the same way in Edit and Run —
 * previously they had two divergent formulas and the beam jumped on handoff.
 */
namespace AimSolver
{
    /** Compute the fixture's pan/tilt (degrees) to aim its beam at @p targetWorld
     *  (metres, absolute — the caller bakes in platform/subject height). Honours
     *  the fixture's rig position, mounting (floor vs hung), pan-zero facing,
     *  invert flags and calibration offsets. Returns false if the fixture has no
     *  usable rig geometry. */
    bool aimDegrees(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                    float &panDeg, float &tiltDeg);
}

/** @} */

#endif
