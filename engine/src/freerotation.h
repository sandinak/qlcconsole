/*
  Q Light Controller Plus
  freerotation.h

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

#ifndef FREEROTATION_H
#define FREEROTATION_H

#include <QtGlobal>
#include "qlcengine_export.h"

class QLCFixtureMode;
class QLCChannel;
class Fixture;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A head that turns round and round on pan or tilt (free / endless /
 * continuous rotation), read from its definition -- so a spin is asked for
 * as "clockwise at 40% of its speed", never as a DMX number.
 *
 * Definitions spell it two ways:
 *  - a channel of its own ("Endless Pan rotation", "Pan Motor continuous
 *    rotation"): ranges for each way round and a value that stops it;
 *  - a band on the pan channel itself (the Junman two-arm beam: 0-127 points
 *    the head -- RotationIndexed -- and 128-255 spins it). Such a head stops
 *    by pointing: there is no stop value.
 * The ranges come from the capability presets (RotationClockwise*,
 * RotationCounterClockwise*, RotationStop), and where a definition only
 * says so in words ("Forwards Pan rotation from fast to slow") from those.
 * Only Pan and Tilt channels count: a gobo or prism that rotates is not the
 * head turning.
 */
class QLC_ENGINE_EXPORT FreeRotation
{
public:
    enum Axis { Pan = 0, Tilt = 1 };

    /** One way round: its DMX range and which end is slow. */
    struct Range
    {
        bool valid = false;
        uchar lo = 0, hi = 0;
        bool slowToFast = true;     ///< lo is the slow end
        double rpmSlow = 0.0;       ///< > 0: revolutions per minute at the slow end, as the definition gives them
        double rpmFast = 0.0;       ///< ...and at the fast end
        bool hasRpm() const { return rpmSlow > 0.0 && rpmFast > rpmSlow; }
    };

    /** How one head spins on one axis. */
    struct Spin
    {
        quint32 channel = UINT_MAX;  ///< fixture-relative channel; UINT_MAX: it doesn't spin
        bool band = false;           ///< on the channel that points the head: it stops by pointing
        bool points = false;         ///< the channel has a range that points the head too
        Range cw, ccw;
        int stop = -1;               ///< the DMX that stops it; -1: none (a band)

        bool canSpin() const { return channel != UINT_MAX && (cw.valid || ccw.valid); }
        /** It goes both ways round. */
        bool reverses() const { return cw.valid && ccw.valid; }
        bool hasRpm() const { return cw.hasRpm() || ccw.hasRpm(); }
        /** The DMX for @p speed: > 0 clockwise, < 0 counter-clockwise, its
         *  size 0..1 from the slowest the head goes to the fastest; 0 stops
         *  it (-1 when it has no stop value: a band, which points instead).
         *  A head that only goes one way goes that way. -1: it doesn't spin. */
        int value(double speed) const;
        /** The speed (0..1) that turns it at @p rpm, clockwise when @p
         *  clockwise: -1 when the definition gives no rpm. */
        double speedForRpm(double rpm, bool clockwise = true) const;
        /** Read back: the signed speed @p dmx spins it at (as value() takes
         *  it), 0 when @p dmx doesn't spin it. */
        double speedOf(uchar dmx) const;
    };

    /** How head @p head of @p mode spins on @p axis (canSpin() false: it
     *  doesn't). A channel of its own is preferred over a band on the
     *  pointing channel. @p head out of range: the mode as a whole. */
    static Spin find(const QLCFixtureMode *mode, int head, Axis axis);
    static Spin find(const Fixture *fixture, int head, Axis axis);

    /** @p fixture has a head (@p head; -1 any) that spins on pan or tilt. */
    static bool canSpin(const Fixture *fixture, int head = -1);

    /** @p ch only spins the head -- it has ways round to turn and no band
     *  that points it. Such a channel is never the head's pointing channel
     *  (QLCFixtureHead), even when the definition lists it after Pan. */
    static bool spinsOnly(const QLCChannel *ch);

    /** How @p ch spins a head, read from its capabilities, with channel 0
     *  for the caller to set (UINT_MAX when it doesn't spin). @p pointing:
     *  it is the head's pointing channel, so a band. */
    static Spin read(const QLCChannel *ch, bool pointing = false);
};

/** @} */

#endif
