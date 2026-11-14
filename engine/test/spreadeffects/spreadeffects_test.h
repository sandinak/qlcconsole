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
    /** Spell Spread on the same kinds: the lantern in the colour at the
     *  look's level, effect channels untouched; the relay is not its; a
     *  pixel not yet reached is not written. */
    void spellSpreadOnTheShowsKinds();
    /** Both effects over 96 step bars (6,144 heads): what a tick costs,
     *  host and script, reported (QLC_SHOT_DIR gets the numbers); the
     *  places are worked out once at that size. */
    void theStepBarsTickCost();
    /** Through the scene, the effect runner and the faders to DMX: a cue
     *  under the step bars plays blue; the spell's red reaches the pixels
     *  near its point and the rest stay blue. Last: its scenes are left
     *  playing. */
    void theCueUnderShowsUntilTheSpellArrives();

private:
    Doc *m_doc = nullptr;
};

#endif
