/*
  Q Light Controller Plus - qlcconsole
  movespeed.h

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

#ifndef MOVESPEED_H
#define MOVESPEED_H

#include <QList>
#include "qlcengine_export.h"

class Fixture;
class SceneValue;

/**
 * Realistic move speed (STRATEGY.md 0.5): a moving head takes time to
 * travel. Definitions carry the pan and tilt range in degrees but no speed,
 * so the speed is a default per fixture type.
 *
 * decided (can change): 0.4 s per 180 degrees for a moving head (a typical
 * spot), 0.1 s for a scanner (a mirror is light). Pan and tilt have their own
 * motors and run together, so a move takes as long as its slower axis.
 *
 * Positions here are fractions 0..1 of the axis range (a DMX value / 255):
 * what a cue's Position palettes resolve to.
 */
namespace MoveSpeed
{
    /** Seconds a head of @a fx's type needs for 180 degrees of an axis. */
    QLC_ENGINE_EXPORT double secondsPer180(const Fixture *fx);
    /** Degrees per second, from secondsPer180(). */
    QLC_ENGINE_EXPORT double degreesPerSecond(const Fixture *fx);
    /** The full pan / tilt range in degrees (the definition's, else 360 / 270). */
    QLC_ENGINE_EXPORT double panSpanDeg(const Fixture *fx);
    QLC_ENGINE_EXPORT double tiltSpanDeg(const Fixture *fx);

    /** How long, in ms, @a fx needs to go from one pan / tilt (fractions of
     *  the range) to another: the slower of the two axes. 0 for a fixture
     *  that cannot move. */
    QLC_ENGINE_EXPORT double travelMs(const Fixture *fx, double panFrom, double tiltFrom,
                                      double panTo, double tiltTo);

    /** The pan and tilt (fractions) @a values set on @a fx (its first head's
     *  coarse channels, fine added when set). Either one may be missing:
     *  @a hasPan / @a hasTilt say which. */
    QLC_ENGINE_EXPORT void positionOf(const Fixture *fx, const QList<SceneValue> &values,
                                      double &pan, double &tilt, bool &hasPan, bool &hasTilt);

    /**
     * A head catching up with where its DMX says it should be, no faster than
     * it can travel. Plain data, one per head: no allocation to step it.
     */
    struct QLC_ENGINE_EXPORT Follower
    {
        double pan = 0.0;       //!< where the head is drawn (fraction)
        double tilt = 0.0;
        qint64 stamp = 0;       //!< ms of the last step
        bool valid = false;     //!< false: the next step snaps to its target

        /** Move toward @a panTo / @a tiltTo for the time since the last
         *  step. A first step, or a gap over @a snapAfterMs (the view was
         *  hidden), snaps. Returns whether the head is still travelling. */
        bool step(const Fixture *fx, double panTo, double tiltTo, qint64 nowMs,
                  qint64 snapAfterMs = 1500);
        bool atRest(double panTo, double tiltTo) const;
    };
}

#endif
