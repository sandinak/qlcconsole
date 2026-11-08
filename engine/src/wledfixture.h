/*
  Q Light Controller Plus
  wledfixture.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDFIXTURE_H
#define WLEDFIXTURE_H

#include <QStringList>
#include <QPair>
#include <QList>

class Fixture;

/** @addtogroup engine Engine
 * @{
 */

/** A WLED fixture in WLED's own DMX "Effect" mode (the WLED Effect and WLED
 *  Lantern definitions): the console sends the effect, its speed, intensity,
 *  palette and three colours, and the device works out every LED. Its
 *  channels are found by the definitions' names. */
namespace WledFixture
{
    /** Whether @p fx is a WLED whose mode has an Effect channel -- one an
     *  effect is RUN ON rather than computed for. */
    bool runsEffects(const Fixture *fx);

    /** Whether @p fx is a string of LEDs run by a WLED -- driven by WLED's
     *  own API, or a WLED definition -- rather than a light that is hung and
     *  aimed. Fixture Properties shows such a string what a string has
     *  (connection, LEDs, string length, pixel path) and hides the mounted
     *  light's rows (aim, tilt, beam, masking, orientation), keeping their
     *  stored values. A WLED driving something that pans or tilts is a
     *  mounted light again: that is the way back in for one that drives one. */
    bool isPixelString(const Fixture *fx);

    /** The index of @p fx's channel called @p name, -1 when its mode has
     *  none. */
    int channel(const Fixture *fx, const QString &name);

    /** The Effect channel value that selects WLED effect @p effect (by name,
     *  any case), -1 when @p fx does not have it. */
    int effectValue(const Fixture *fx, const QString &effect);

    /** The effects @p fx's Effect channel offers, in its order. */
    QStringList effectNames(const Fixture *fx);

    /** The "freeze" bit of the Effect Option channel: set, a WLED driven
     *  by its own API (Network (WLED)) stops its effect on the frame it is
     *  showing (every segment's "frz"), clear it runs again. WLED's own
     *  DMX reads only the top two bits of that channel (mirror, reverse),
     *  so over Art-Net / E1.31 it is ignored. The definitions name the
     *  ranges ("..., frozen"); the WLED output (WledState) reads them. */
    constexpr int kFreezeOption = 0x20;

    /** Whether Effect Option value @p option asks a WLED to freeze. */
    inline bool freezes(int option) { return (option & kFreezeOption) != 0; }

    /** A WLED's effect settings at rest -- Effect, Speed, Intensity,
     *  Palette and Option each at its channel's default (0: Solid), as
     *  (channel index, value) for the ones @p fx's mode has; empty when it
     *  runs no effects. A scene on it sets them under its own values, so a
     *  look that sets only a colour shows that colour solid, not the last
     *  effect latched on; at the default, so a cue is the same however it
     *  is reached (run to, or jumped back to). */
    QList<QPair<int, int> > settingsAtRest(const Fixture *fx);

    /** Whether Hold can freeze @p fx at the end of a run: a WLED that runs
     *  effects, driven by its own API. Over DMX it keeps running. */
    bool canFreeze(const Fixture *fx);
}

/** @} */

#endif
