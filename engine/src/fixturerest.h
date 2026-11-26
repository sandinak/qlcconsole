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
#include <QString>
#include "qlcengine_export.h"
#include "qlcphysical.h"
#include "fixture.h"

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

    /** A pan or tilt channel (coarse or fine): Stop ALL leaves it where
     *  it is, so no head swings to its definition's default. */
    QLC_ENGINE_EXPORT bool holdsPosition(const QLCChannel *ch);

    /** What @p fx behaves as (Fixture::Behaviour, board Q350/Q351): the
     *  LD's word for it (Fixture::behaviourSetting()), else the guess.
     *  Read when the rig changes, a stop comes or a cue is picked, never
     *  per frame (the OutputRuleKeeper keeps what it needs). */
    QLC_ENGINE_EXPORT Fixture::Behaviour behaviour(const Fixture *fx);

    /** What @p fx behaves as when nobody has said, from its definition
     *  and its plot symbol: a curtain (Scenery::isCurtainFixture()), the
     *  driver of a drape on the plot, a turntable, winch, hoist or scrim
     *  is Machinery; a kabuki or a dropping scrim One-shot; a hazer,
     *  fogger, flame, CO2, confetti or spark effect -- anything gas
     *  powered -- and a laser of class 3R, 3B, 4 or Unknown a Hazard (a
     *  silk flame is a light); a relay (the Switch output rule, or named
     *  so) and house or work lights on when not in use (the On at open
     *  rule) a Switch; the rest -- house dimmers by name too --,
     *  lasers of class 1 to 2M or none said too, a Light. */
    QLC_ENGINE_EXPORT Fixture::Behaviour guessBehaviour(const Fixture *fx);

    /** @p fx's laser class: the unit's own (Fixture Properties' Laser
     *  class), else its definition's (its mode's physical, else the
     *  definition's); none when neither says. */
    QLC_ENGINE_EXPORT QLCPhysical::LaserClass laserClass(const Fixture *fx);

    /** A laser whose class is 3R, 3B, 4 or Unknown that the LD has said is
     *  a Light: what Show Review lists, and what the properties ask a
     *  typed yes for. */
    QLC_ENGINE_EXPORT bool isHazardousLaserAsLight(const Fixture *fx);

    /** Behaves as a Hazard: never fired by an edit, a preview or (until
     *  arming, Q352/Q353) a run; off the moment a stop comes. */
    QLC_ENGINE_EXPORT bool isHazard(const Fixture *fx);

    /** Stage machinery, a one-shot or a switch (behaviour()): it moves
     *  only when a run plays it (GO, Back, a jump, in Design or Operate).
     *  Held where it is while a cue is picked in Design
     *  (OutputRuleKeeper::holdMachineryForEdit()), and a stop -- Stop ALL,
     *  Fade and stop, the emergency stop -- freezes every channel of it
     *  where it is (OutputRuleKeeper::holdStageMachinery()) until the next
     *  GO; putAllAtRest() leaves it. The one predicate both read. */
    QLC_ENGINE_EXPORT bool freezesOnStop(const Fixture *fx);

    /** holdsPosition() for channel @p channel of fixture @p fixtureId:
     *  what a stop that lets go of a held level (the Simple Desk's, a VC
     *  slider's) keeps, so no head swings. False for no such fixture. */
    QLC_ENGINE_EXPORT bool holdsPosition(const Doc *doc, quint32 fixtureId, quint32 channel);

    /** holdsPosition() for the patched channel at @p universeAddress
     *  (universe << 9 | address). False where nothing is patched. */
    QLC_ENGINE_EXPORT bool holdsPositionAt(const Doc *doc, quint32 universeAddress);

    /** Every patched fixture's channels to their rest value, in the
     *  universes, for whatever no longer drives them: fog and haze off,
     *  effects stopped, gobos and colour wheels back to open; pan and tilt
     *  are left where they are (holdsPosition()), and stage machinery
     *  (freezesOnStop()) is left whole. What plays
     *  still drives its own channels the next frame. Returns how many
     *  channels were written. */
    QLC_ENGINE_EXPORT int putAllAtRest(Doc *doc);

    /** What Stop ALL and the emergency stop do once they have stopped the
     *  functions (both through here, so the two cannot drift apart): every
     *  fixture put at rest now (putAllAtRest()), and again when the last fade
     *  out is over -- only when nothing has started since. On the main
     *  thread (the fixture list's); it takes each universe's frame lock as a
     *  patch does and never waits for the MasterTimer. */
    QLC_ENGINE_EXPORT void restAfterStop(Doc *doc);

    /** Q326, every stop (Stop ALL timed or not, the emergency stop): each
     *  Show-only pan and tilt channel (holdsPosition()) held at what went
     *  out there last (Universe::holdShowOnlyPositions()), so no Show-only
     *  head moves; the next write there that counts drives it again. Safe
     *  from any thread. */
    QLC_ENGINE_EXPORT void holdShowOnlyHeads(Doc *doc);

    /** Stop ALL: every function stops (over @p fadeMs, 0 at once) and,
     *  once they have, every fixture is put at rest (putAllAtRest()), so a
     *  fog machine stops pumping and a gobo or WLED effect stops running.
     *  At once, and again when the last fade out is over -- only when
     *  nothing has started since. */
    QLC_ENGINE_EXPORT void stopAll(Doc *doc, int fadeMs = 0);
}

/** @} */

#endif
