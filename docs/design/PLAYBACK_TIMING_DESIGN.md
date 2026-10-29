# Playback timing — Design

Branch `feat/playback-timing` (2026-10-12). docs/project/TODO.md backlog #4 (beat-quantized
GO and Swop) and #5 (keyframe shapes and spatial transitions), plus the
clickable Load chip. Builds on the beat clock and speed masters
(SPEED_SPREAD_DESIGN.md S1/S2).

## Where things stand

- **GO**: every step change -- cue list Next/Previous/set, VC button step,
  the Programming tab's chaser step, the show timeline, the chaser editor --
  becomes a `ChaserAction` through `ChaserRunner::setAction()` and is applied
  in `ChaserRunner::write()` on the next tick. Nothing waits for a beat.
- **Flash**: `Function::flash()` is honoured by Scene only (it adds itself
  as a DMX source, at Flashing priority with "flash override"). No Swop or
  solo-flash; VCSoloFrame is a latched solo inside a frame.
- **Fades are straight lines**: `FadeChannel::calculateCurrent()` is
  `elapsed / fadeTime`, so a mover's move between cues is linear DMX.
- **SceneTransition** (wipes, from centre, dissolve, along bars) gives each
  head its own delay and fade -- a scene's own In/Out, applied when the
  scene starts or stops (so also as a chase step), never chosen per step.
- **Load chip**: the whole tick's time, peak per second; not clickable, and
  no per-function cost is measured.

## P1 — GO on the beat *(DONE 2026-10-12)*

A chaser (and so a cue list) gets **GO lands: at once / on the next beat /
on the next bar** (`Chaser::goQuantize`, XML `GoQuantize`, default at once).
A step change asked for in `setAction()` is held in `write()` until the
chaser's clock crosses the next whole beat (or bar, 4 beats): its speed
master's if it has one, else the main beat clock. Asking again while one is
held replaces it (two GOs in one beat = one step further, not two). Stop is
never held. The chaser editor sets it; the cue list shows a held GO.
A GO from stopped waits for the beat too (rather than starting step 1 and
jumping on the beat); a show-driven chaser is never held. Bars are counted
from the clock's beat 0 (a tap sets the beat, not the bar).

## P2 — Fade shapes

`FadeChannel` gets a **shape**: Linear (as now), S-curve, Ease in, Ease out
-- applied to the fade's progress, so 8- and 16-bit channels alike (a
mover's move eases into and out of its mark). A Scene has a fade shape
(XML `FadeShape`), a chase step can override it (handed down at start,
like the speed master), and a SceneTransition's per-head fade uses it.

## P3 — Transitions per chase step

A chase step can choose a **transition** (any SceneTransition shape) for
the look it starts, timed by the step's fade in: handed down to the step's
scene at start (`Function::setInheritedTransition`), overriding the
scene's own In for that run. The step's fade out uses the scene's Out (or
the same shape when the step sets one). Sequences (one bound scene, values
swapped) are out of scope -- their steps never restart the scene.

## P4 — Swop

A VC button action **Swop**: while held, its function plays at full and
every other function's **intensity** is knocked out (colour, position and
beam keep their values, so the rig doesn't move or change colour behind the
swop). Engine: a swop owner on InputOutputMap; faders not owned by the
swopping function contribute nothing on intensity (HTP) channels while it
is held. Optional **on the beat**: the swop engages on the next beat.

## P5 — The Load chip

MasterTimer times each function's `write()` (and each DMX source) and keeps a
per-second snapshot. Clicking the footer's Load chip lists the heaviest
functions: name, type, ms per tick, share of the budget.

## Order

P1, P2, P3, P4, P5 -- each its own commit and gate.
