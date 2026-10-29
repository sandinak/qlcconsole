# Effect lifecycle & timing

Design for effects that have a **natural life cycle** — a wand sweep, a burst, an
image reveal — that play once, end, and want to land in time with the show.
Extends the existing effect engine (loop/reactive effects, per-look fades, release
fade-out, RGBScript single-shot). Doc-first per repo convention.

## Lifecycle types

An effect declares one in its meta:

- **`loop`** *(default — today's behaviour)* — plasma, candle, generative. Runs
  forever.
- **`reactive`** — MIDI/audio. Alive while there's input; no fixed length. (Marked
  by subscribing to a data channel; already how idle-skip is decided.)
- **`oneshot`** — plays a defined animation once, then ends. The new type.

```js
effect.lifecycle        = "oneshot";
effect.syncTo           = "entrance";   // "entrance" | "span" | "exit"
effect.onFinish         = "hold";       // "hold" | "release"
effect.naturalDurationMs = 2000;        // fallback length
```

`syncTo` and `onFinish` are **declared by the script, overridable per Look**
(both forks resolved that way).

## Phase, not wall-clock

The runner feeds a one-shot its window; the script fits itself to it:

- `inputs.phase`    — 0→1 progress through the resolved duration
- `inputs.duration` — that duration in ms

A wand is just `pos = phase * width`. The **same script auto-fits any duration** —
no per-cue rewrite. (Loop/reactive effects ignore phase; they keep using
`inputs._time`.)

## Duration resolution (the "Look overrides Chase" precedence)

For a `oneshot`, the resolved duration is the first of:

1. **Look override** — an explicit time on the effect palette. *Wins.* → gives
   independent per-effect timing (two one-shots in one cue can differ).
2. **Cue / chase timing**, chosen by `syncTo`:
   - `entrance` → the cue/step **fade-in** (lands as the look comes up),
   - `span`     → the step **duration/hold** (fills the cue),
   - `exit`     → the **fade-out** (plays out as the look releases).
3. **`naturalDurationMs`** — the script's default, last resort.

Same precedence shape as the per-look fade overrides already built
([[per_look_fade_times]]).

## Completion & finish

A one-shot ends when `phase >= 1` **or** the script sets `state.done = true`
(whichever first — a script can finish early, e.g. all particles faded). On end,
per the effect's `onFinish` (Look may override):

- **`hold`** — keep emitting the final frame until the look changes or its
  **release fade-out** runs (reuse `f107fb4b1`). Runner can stop re-ticking and
  hold `m_lastResults` (a "hold" cousin of the idle-skip) to save load.
- **`release`** — stop driving; fixtures fall back to the look's base / other
  looks. A transient burst clears itself.

Retrigger: when the look re-runs (chase loops back to the step), phase restarts at
0 — it replays once, it doesn't loop within the step.

## Sourcing the cue timing

The effect runs inside a Scene (the look), which is a chase step or a timeline
cue. Timing sources, easiest → hardest:

- **entrance / exit** → the Scene's own `fadeInSpeed()` / `fadeOutSpeed()`
  (`sceneEnvelope()` already reads fade-in). A chaser applies its step fade to the
  scene, so this already reflects the chase fade. *Available now.*
- **span** → the chase **step duration**. The effect only knows its `sceneId`, not
  the driving chaser, so this needs a scene→running-chaser-step lookup (the
  `CueLookahead`/`Chaser::currentStepDuration()` accessors from the move-in-black
  work are the starting point). *Deferred to L2.*

## Build slices

- **L1 — lifecycle core (buildable + unit-testable now).** Meta declaration;
  feed `inputs.phase`/`inputs.duration`; one-shot completion (`phase>=1` /
  `state.done`); `hold` vs `release`. Duration = Look override → `naturalDurationMs`
  (cue-timing inheritance stubbed). Port one example one-shot (a **Wand** sweep) +
  convert the RGBScript `rgbLoop=Once` path to report completion here. Prove with
  the effect test harness.
- **L2 — cue-timing sync.** Resolve `entrance`/`exit` from the Scene fades and
  `span` from the driving chase step (scene→chaser-step lookup). This is the
  "in time with the show" half.
- **L3 — Look override UI.** A per-look **duration** cell in the Looks tree (beside
  the fade columns) + optional `syncTo`/`onFinish` overrides. Persisted like the
  fade overrides.

## Open questions

- **span without a chaser** (a scene run standalone / on the timeline): fall back to
  `naturalDurationMs`, or the timeline cue's own duration if we can read it.
- **hold + release fade interaction** — a held one-shot then a look-stop should run
  the release fade-out cleanly (already built; verify the ordering).
- **phase for loop effects** — leave `phase` undefined for loop/reactive so
  existing scripts are untouched; only one-shots read it.
- Whether `syncTo`/`onFinish` Look overrides need UI in L1 or can wait for L3
  (lean: script-declared in L1, UI in L3).
