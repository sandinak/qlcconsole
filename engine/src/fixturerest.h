/*
  Q Light Controller Plus
  fixturerest.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREREST_H
#define FIXTUREREST_H

#include <QtGlobal>
#include "qlcengine_export.h"

class Fixture;
class QLCChannel;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/** What a fixture's channels are when nothing drives them, and which
 *  fixtures make something other than light. Stop ALL puts every patched
 *  fixture here; Locate leaves the ones that make smoke or laser light
 *  alone (UX-70, UX-72). */
namespace FixtureRest
{
    /** A hazer, fogger or fan: it puts something in the air, not light.
     *  Its Intensity channel is its output (an Entour Faze's "Output
     *  Volume"), so it is never flashed and it stops at 0. */
    QLC_ENGINE_EXPORT bool makesAtmosphere(const Fixture *fx);

    /** A laser: never fired to find it, and off when nothing drives it. */
    QLC_ENGINE_EXPORT bool isLaser(const Fixture *fx);

    /** A channel that runs a fog or haze output on any fixture (a moving
     *  head or an effect light with a fog channel): named so. */
    QLC_ENGINE_EXPORT bool isAtmosphereChannel(const QLCChannel *ch);

    /** Channel @p channel of @p fx with nothing driving it: its default
     *  (a WLED's effect settings included: Solid), and 0 -- off -- on a
     *  hazer, fogger, fan or laser and on any fog or haze channel. */
    QLC_ENGINE_EXPORT uchar restValue(const Fixture *fx, quint32 channel);

    /** Every patched fixture's channels to their rest value, in the
     *  universes, for whatever no longer drives them: fog and haze off,
     *  effects stopped, gobos and colour wheels back to open. What plays
     *  still drives its own channels the next frame. Returns how many
     *  channels were written. */
    QLC_ENGINE_EXPORT int putAllAtRest(Doc *doc);

    /** Stop ALL: every function stops (over @p fadeMs, 0 at once) and,
     *  once they have, every fixture is put at rest (putAllAtRest()), so a
     *  fog machine stops pumping and a gobo or WLED effect stops running.
     *  At once, and again when the last fade out is over -- only when
     *  nothing has started since. */
    QLC_ENGINE_EXPORT void stopAll(Doc *doc, int fadeMs = 0);
}

/** @} */

#endif
