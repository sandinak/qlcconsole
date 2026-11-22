/*
  Q Light Controller Plus - Unit test
  spreadeffects_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SPREADEFFECTS_TEST_H
#define SPREADEFFECTS_TEST_H

#include <QObject>

class Doc;

/* The effects that spread from a point or a side (Fire Spread, Spell
   Spread; lib/spread.js), on the kinds of light DanceHysteria's rig has: a
   WLED lantern in its Effect mode, a silk flame on a relay, the Step Row 64
   Heads step bars and the PL-4003 bars (their definitions copied into
   data/, never the show). */
class SpreadEffects_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Spell Spread, the script: each light reached in proportion to its
     *  distance from the point, nothing written for it before (it keeps
     *  what is under it), the look's colour after, a whiter flash as it
     *  arrives (never on a colour wheel), each kind of head, a ragged edge,
     *  the cue's time, never starting again, white with no colour. */
    void spellSpreadReachesEachLightInTurn();
    /** From a side: Upstage runs downstage, Downstage up, Stage Left and
     *  Stage Right across (+X is stage left); heads in one line all at once. */
    void spellSpreadSweepsInFromASide();
    /** On a rig: each pixel's place on the floor and the heads' extent come
     *  from where the bars stand, worked out once, again when one moves;
     *  From Upstage lights the upstage bar first, pixel bars sweep pixel by
     *  pixel from a side. */
    void spellSpreadSweepsTheRigFromWhereItStands();
    /** Fire Spread on the show's kinds: the lantern takes colour and level
     *  and its effect channels are left alone; the relay switches on as the
     *  fire catches and stays on (never a level between, never chattering);
     *  the step bars and PL-4003s burn pixel by pixel and are dark, pixel
     *  by pixel, until the fire reaches them. */
    void fireSpreadOnTheShowsKinds();
    /** The show's washes (an XL-450 on each truss, a step top's wall
     *  washer: RGB and white pixels, no dimmer), over black, are dark until
     *  the fire reaches them: none says it has a dimmer, the effect writes
     *  nothing to them at the start and nothing lit just before its catch,
     *  burning after; 0 on the wire at the start. */
    void fireSpreadLeavesTheWashesDarkUntilItReachesThem();
    /** Board Q259, over what's there: a cue under two RGB washes lights
     *  them green; Fire Spread over them from the first wash's place. On
     *  the wire the wash the fire hasn't reached keeps the cue's green,
     *  value for value; the one it has burns in the fire's colours. */
    void fireSpreadCastsOverWhatIsThere();
    /** Board Q259, hold at the end: run Once (At the end: Hold, Length
     *  auto) it runs until the fire has reached every head -- not the
     *  generic 8 s -- and that last frame stays, every wash burning, frame
     *  for frame long after. On the wire: held, then let go to 0 by each
     *  of the cue released, GO to a cue without the effect, Stop all and
     *  the emergency stop (All Off). Once to Release still ends at its
     *  time, not when the fire has spread. */
    void fireSpreadHoldsAsItEndsUntilLetGo();
    /** Fire Spread's Colours: Fire (the default), and Front and Behind
     *  with no colour to be the front, are byte for byte the script as it
     *  was before the choice (data/fire-spread-0412.js), on every kind of
     *  head, ragged, flaring, cooled, over the cue's time. */
    void fireSpreadInFireIsAsItWas();
    /** Front and Behind: the look's first colour as the front reaches a
     *  head, its second after the backfill (a short crossfade), flickering
     *  in level or steady (Flicker Behind); dark before the front; a
     *  colour wheel on the nearest slot, never between; a dimmer burns as
     *  Fire's, a relay switches; one colour: fire behind the front. */
    void fireSpreadFrontAndBehind();
    /** The backfill Front Width behind each head; with an Edge ragged in
     *  its own seeded way, never ahead of the front, the same seed the
     *  same. */
    void fireSpreadBackfillIsRaggedBySeed();
    /** Lightning Spread, One Burst (the script, a fixed seed): heads nearer
     *  the Aim target are hit first, each flashes to full the tick it is
     *  hit and flickers out in two or more strikes (dark between them),
     *  then every head is at rest (nothing written) and state.done; the
     *  same seed gives the same burst, another seed another. */
    void lightningSpreadBurstsOutAndFlickersOut();
    /** Run Once (At the end: Hold, Length auto) a burst ends when the last
     *  head has flickered out and holds that rest; on Loop it bursts once
     *  and stays at rest. */
    void lightningSpreadOneBurstEndsAtRest();
    /** Static Generator: the head at the centre is never dark, arcs reach
     *  heads out to the Reach and flicker there, never beyond it; it never
     *  says it is done. */
    void lightningSpreadStaticGeneratorStaysInItsReach();
    /** A dimmer flickers in level; a moving head gets its dimmer and is
     *  never moved (no pan or tilt written); a relay is not lightning's. */
    void lightningSpreadOnDimmersAndMovers();
    void lightningSpreadStrikesWhiteHotWithABlueEdge();
    void amberPulseHeadsWanderOnTheirOwn();
    /** Spell Spread on the same kinds: the lantern in the colour at the
     *  look's level, effect channels untouched; the relay is not its; a
     *  pixel not yet reached is not written. */
    void spellSpreadOnTheShowsKinds();
    /** The spreads over 96 step bars (6,144 heads): what a tick costs,
     *  host and script, reported (QLC_SHOT_DIR gets the numbers); the
     *  places are worked out once at that size. Lightning Spread's script
     *  costs no more than twice Fire Spread's, spreading. */
    void theStepBarsTickCost();
    /** What the HOST costs a tick over the same 96 bars, piece by piece
     *  (the cells, the look's base values, the per-head arguments, reading
     *  the answers back), for an effect that says nothing and one that
     *  colours every head: reported (QLC_SHOT_DIR gets the numbers) and
     *  held to a budget. The script's own cost is theStepBarsTickCost's
     *  business; this is what every effect on a big rig pays whatever it
     *  draws. */
    void theHostTickCostOverTheStepBars();
    /** Through the scene, the effect runner and the faders to DMX: a cue
     *  under the step bars plays blue; the spell's red reaches the pixels
     *  near its point and the rest stay blue. Last: its scenes are left
     *  playing. */
    void theCueUnderShowsUntilTheSpellArrives();

private:
    Doc *m_doc = nullptr;
};

#endif
