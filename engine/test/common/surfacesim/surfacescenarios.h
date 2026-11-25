/*
  Q Light Controller Plus
  surfacescenarios.h

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

#ifndef SURFACESCENARIOS_H
#define SURFACESCENARIOS_H

#include "simsurface.h"

/**
 * What hardware does that a person does not mean, each as one call on any
 * simulated surface (docs/design/SURFACE_SIMULATOR.md, "Device
 * scenarios"). A test sets the show up, runs one, and asserts what the
 * show did -- usually that it did nothing.
 */
namespace SurfaceScenarios
{
    /** The cable out for @p awayMs and back in; on its return the device
     *  sends @p dump (its state, control by control) before anyone
     *  touches it. */
    inline bool unplugReplug(SimSurface *dev, const QList<QPair<QString, int>> &dump = {}, int awayMs = 30)
    {
        if (!dev->unplug())
            return false;
        dev->wait(awayMs);
        return dev->replug(dump);
    }

    /** The computer slept: the system drops the device and lists it again
     *  on waking, and the board -- which never knew -- repeats what it last
     *  sent (@p held: its controls as they are). */
    inline bool sleepWake(SimSurface *dev, const QList<QPair<QString, int>> &held = {}, int asleepMs = 50)
    {
        return unplugReplug(dev, held, asleepMs);
    }

    /** A momentary button: on while held. */
    inline bool momentary(SimSurface *dev, const QString &control, int holdMs = 40)
    {
        return dev->tap(control, holdMs);
    }

    /** A latching button (a board in toggle mode): each press sends the
     *  other state and the release sends nothing. @p latched: its state,
     *  flipped. */
    inline bool latchingPress(SimSurface *dev, const QString &control, bool *latched)
    {
        *latched = !*latched;
        return *latched ? dev->press(control) : dev->release(control);
    }

    /** A fader left at @p at whose pot reads @p at and @p at + 1 by turns. */
    inline bool faderJitter(SimSurface *dev, const QString &control, int at, int reads = 10)
    {
        return dev->jitter(control, at, reads);
    }
}

#endif
