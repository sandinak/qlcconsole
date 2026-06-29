# Position Management — Architecture & Spec

This is the design target. Where today's code disagrees it is a bug to fix, not
the spec to follow (see "Delta from current code" at the end).

## Principles

- Traditional DMX-value scenes are unaffected by any of this — original
  workflow is preserved.
- Scenes built from **Looks** compose their looks together. Looks are **not**
  precedented; they combine under one consistent ruleset.
- **One effect engine.** All effects run through the JS effect-script system
  (`EffectScriptRunner` + scripts in `resources/effectscripts/`). The native
  C++ `FollowSpotEffect` is retired. Any effect can subscribe to host inputs
  (joystick, beat/BPM, time) and extend a scene's behaviour. Followspot is the
  reference case precisely because it has niche needs (joystick subscription,
  per-fixture seeding from a target, scene-to-scene handoff).
- **Joystick works in both Design (Edit) and Run mode** (mouse *and* joystick).
- An effect is active **only when its Effect look is attached to the scene** —
  never implicit.
- **At most one effect script per scene**, and it is the **only dynamic
  element** — every other look is a **static value** for the run. This is what
  lets subscribed look data be resolved once and held (below).

## Position look types — the scene's starting point

A Pan/Tilt look sets where the scene *starts*; effects modify it at runtime.

- **Standard Pan/Tilt look** (PanTilt palette): raw pan/tilt angles, applied
  **identically to every head** → parallel beams. It is a fixed look with no
  spatial target. "Point all heads at the same angle," not "at a location."
- **Target Pan/Tilt look** (Aim palette): a named stage **location**. The host
  computes per-fixture pan/tilt from rig geometry (AimSolver) so every selected
  fixture's beam **converges on that point**. Recomputed live whenever the
  target moves.

Only a Target look makes beams converge on a place; a Standard look only sets a
common angle.

Other look types referenced below: **Color**, **Dimmer/Brightness**, **Gobo**,
and a **Focus** look type (new — needed for the D-pad focus control; see Edit
mode). A scene's gamepad controls light up only for the look types it actually
carries.

## Effects and the influence contract

Each effect script declares its relationship to the scene so the host can
validate combinations and flag errors *before* driving DMX. An effect declares:

- **reads** (subscribes) — look **domains** whose current per-fixture values the
  host should inject so the script can combine/alter them (see "Combining &
  altering looks"). A read domain the script does **not** also control is a pure
  dependency (e.g. read position to drive intensity).
- **controls** — channel domains it **writes**. Followspot: **Pan/Tilt**.
- **requires** — inputs/reads that **must** be present or the combination is an
  error. Followspot: a **joystick**.
- **contradicts** — influences whose presence makes the combination an error.
  Followspot: a **Standard Pan/Tilt look** (fixed angles conflict with live
  flying of the same channels).

Read and write use the **same domain vocabulary** (see Runtime data contract),
so a script reads `f.color` and returns `intent.color` symmetrically.

Host validation, at the moment a look/effect is combined onto a scene:

- **Contradiction present → error.** Show a **popup dialog** that explains why
  the combination is invalid (e.g. "Followspot controls Pan/Tilt and cannot be
  added to a scene with a fixed Pan/Tilt look"), and refuse the combination.
- **Required input missing → error** the same way.

This contract is general: every effect carries the same three declarations, so
the host can catch nonsensical stacks (two effects both controlling Pan/Tilt, a
colour effect on a fixture with no colour, etc.) the same way.

### Followspot's contract

- **controls** Pan/Tilt.
- **requires** a joystick.
- **contradicts** a Standard Pan/Tilt look → **ERROR** (popup).
- **Shortest-path, no wipe.** On any scene transition the beam takes the
  **shortest path** between its old and new positions — it must never sweep the
  long way round or do a full-range wipe across the room. In pan, pick the angle
  representation nearest the current beam (no >180° swing, no extra revolution
  even on >360° heads); move directly to the new position.
- **Target is implicit.** The followspot reads the Target from the scene's
  attached **Target look** — it has no `followTarget` binding of its own.
  - With a Target look present → the beam seeds from it (per `followMode`).
  - With **no** Target look → **free-fly**: the operator flies the beam from
    centre. Free-fly drives **only the fixtures included in the scene**.
- **subscribes** to the global joystick data channel (device input), plus its
  own `followMode` and `personHeight` parameters.

## Beam vs. target — and why convergence needs a shared point

The followspot moves a **single shared aim point** in floor space; the scene's
Aim look re-aims **every** head at it through rig geometry, so multiple heads
stay **converged** on the spot. Per-fixture angle flying can't do this — equal
angle steps from different fixture geometries diverge — so it is used **only**
for free-fly (no target).

- The **target** is the saved location the Aim look points at.
- **Edit mode:** the joystick moves the **saved target itself** (authoring); the
  move **persists** into the look.
- **Run mode:** the joystick moves a **transient** copy — the saved target is
  recorded on Run entry and **restored on exit**, so flying the followspot live
  never corrupts the authored target. Only the effect's params (`followMode`,
  `personHeight`) are saved.
- **Free-fly (no target):** the followspot.js effect flies each head's angle
  itself (single-head use); there is no shared point to converge on.

The followspot.js effect **defers** (emits no pan/tilt) whenever a target is
present, so the host's shared-point move is the sole driver and they never
fight.

## Joystick ownership

- If the focused scene has an effect that **subscribes to the joystick**
  (followspot), that effect **owns the joystick in both modes** and flies the
  **beam** setpoint. It does **not** move the target.
- Otherwise the joystick is an **authoring tool** for the selected scene — the
  full Edit-mode gamepad map below.

## followMode — seeding when a followspot takes over

On scene activation / transition, the followspot seeds each fixture's beam from
the scene's Target look:

- **lastPosition** (default): hold the beam where the previous scene left it.
  Persisted **per fixture across scene changes**, so heads don't jump on
  transitions. Falls back to the Target look, then physical centre, the first
  time when nothing has been recorded yet.
- **snapToTarget**: seed from the Target look's pre-aimed angles on **every**
  transition.

Either way the move to the seeded position obeys the **shortest-path, no-wipe**
rule above — `lastPosition` simply has zero distance to travel, `snapToTarget`
takes the short way to the target.

## Effect-script API tenets

The script engine is a product surface, not just plumbing. These tenets are
binding on the API design and on every script we ship.

- **Testable in isolation.** A script's `tick` is a **pure function** of its
  arguments `(fixtures, inputs, palettes, params, state, data)` → an array of
  intent objects. No host handles, no DMX, no globals, no wall-clock — so a test
  can feed inputs and assert on the returned intents with no app running. We
  keep a lightweight harness that runs a script's `tick` over canned inputs.
- **Readable / easy to encode.** A script is a small declarative manifest
  (name, inputs, params, data channels, targets, the influence contract) plus
  one `tick`. Scripts speak **high-level intent** — degrees, colours, 0..1
  levels, palette refs — never raw DMX addresses; the host converts.
- **Well-defined, extensible API.** Versioned via `apiVersion`. Adding a new
  input, param, data channel, or intent key must be **backward-compatible** —
  existing scripts keep working untouched. The full surface is documented in one
  place.
- **Clear data subscription.** Subscribing to a scene/host data value is one
  declarative line (`effect.dataChannels = ["joystick"]`) and the value arrives
  in `data.joystick.{...}`. Inputs, params, and targets are declared the same
  obvious way. No wiring code in the script.
- **Consistent return shape, symmetric with reads.** Every `tick` returns **one
  intent object per fixture**, drawn from one shared key vocabulary (`pan`,
  `tilt`, `dimmer`, `r`/`g`/`b`/`w`/`a`/`uv`, `gobo`, `prism`, `shutter`,
  `focus`, `color`). The **same vocabulary** is what a script reads back as
  `f.<domain>` for subscribed looks, so read and write mirror each other. An
  **omitted key means "don't touch"** — the scene's look retains that channel.
  Units are consistent: degrees for angles, 0..1 for normalized levels, 0..255
  for colour components.

## Runtime data contract

What a script's `tick` is handed, split by what actually changes during a run:

- **Resolved once and held — subscribed look data.** Because there is one effect
  script and every other look is static, the host resolves the script's
  subscribed look **domains** (and static fixture geometry) **on first tick** and
  caches them on the instance. It re-resolves **only on a look-change event**
  (palette edited, or a Target's location moved) — never per tick. In a steady
  run this happens exactly once.
- **What the cached fixture object carries:** geometry (`hasPanTilt`, `panRange`,
  `tiltRange`), pre-computed `aimAt` target angles for the script's declared
  target slots, and the resolved value for every look **domain the script
  subscribes to** (`f.position`, `f.intensity`, `f.color`, `f.shutter`, …).
- **Per tick — only the genuinely dynamic inputs.** `inputs`, the subscribed
  `data` channels (`data.joystick.{pan,tilt,sensitivity,deadzone}`, beat/BPM/
  time), the script's own evolving `state`, and `lastSpot` handoff. These are
  cheap and carry all the run-time motion; the look data does not.
- **Read/write symmetry & units:** a script reads `f.<domain>` and returns
  `intent.<domain>` in the **same vocabulary and units** — `position {pan,tilt}`
  in degrees (and/or floor `{x,y}` in metres), `intensity` 0..1, `color {r,g,b}`
  0..255, plus `gobo`, `prism`, `shutter`, `focus`.
- **Omitted intent key = "don't touch"** — the scene's look keeps that channel.

A script today only sees its **own bound** palettes, not the scene's full set of
looks. Multi-domain combine effects (below) require the host to resolve and
inject the scene's current per-fixture look values for subscribed domains —
that is new capability, listed in the Delta.

## Combining & altering looks (multi-domain effects)

An effect may **subscribe to several look domains, transform them together, and
write the result.** The host resolves the scene's per-fixture values for each
subscribed domain **once at start** (re-resolving only on a look-change event,
per the Runtime data contract) and injects them on the fixture object; the
script combines them with its per-tick `inputs`/`state` and returns modified
intents, which win at Override priority. Read-modify-write on the same domain
(read base `intensity`, scale, write `intensity`) is the normal pattern.

Reference scripts this must support:

- **position → intensity** — subscribe `position` + `intensity`; vary brightness
  by where the beam points (e.g. dim toward the back of stage).
- **position → color** — subscribe `position` + `color`; tint by location.
- **intensity + color rainbow** — subscribe `intensity` + `color`; cycle hue.
- **color + shutter strobe** — subscribe `color` + `shutter`; emit a different
  colour on each strobe cycle.

Each is a small, single-purpose script — readability and composability over one
do-everything effect.

## Composition & priority across functions

- **Priority:** effect scripts write at `Universe::Override`; Scenes and
  RGBMatrix (RGBScript) write at `Universe::Auto`. So an effect **wins** any
  channel it controls over the scene/matrix; domains it omits pass through.
- **Two script engines:** EffectScript (looks, this system) and RGBScript
  (RGBMatrix functions) are **separate**. In a Collection/Chaser they run
  concurrently and compose only at the universe level — different domains
  coexist; same domain → Override (effect) beats Auto (matrix).
- **Contract scope (open decision):** the influence contract validates looks
  **within one scene**. It does **not** cross-check functions composed in a
  Collection or Chaser, so two functions could both drive the same domain with
  no popup — the Override just silently wins. Decide whether contradiction
  checking must also run at **composition time**.

## Case matrix

`Look` × `followspot attached?` × `mode` → behaviour.

| Look | Followspot | Mode | Heads | Joystick | Pin | Persisted |
|------|-----------|------|-------|----------|-----|-----------|
| Standard P/T | no  | Edit | fixed angles (parallel) | authoring map | none | look angles |
| Standard P/T | no  | Run  | fixed angles (parallel) | idle | none | — |
| Standard P/T | yes | any  | **ERROR** (popup) | — | — | — |
| Target | no  | Edit | converge on target | moves **+ saves** target location | on target | target location |
| Target | no  | Run  | converge on target | idle | on target | — |
| Target | yes | Edit | converge on moving spot | moves **+ saves** the target (heads converge) | on spot | target location |
| Target | yes | Run  | converge on moving spot | moves **transient** aim point (target restored on exit) | on spot | params only |
| None | yes | any | free-fly from centre, **scene fixtures only** | flies each head's angle | on beam | params only |
| None / Standard | no | Run | scene drives DMX normally | idle | none | — |

Notes:
- "converge on target" = AimSolver recomputes per-fixture pan/tilt when the
  target moves (`resetRuntime()` re-resolves the Aim look).
- In **Edit** with a Target look **and** followspot attached, the joystick flies
  the beam (effect owns it); use the **mouse** to place the target, or detach
  the followspot.

## Edit-mode gamepad authoring (forthcoming — extend later)

In Edit mode the gamepad is an **extension of the mouse** and works inherently
on the **currently selected scene**, editing that scene's looks live. Controls
are active only for the look types the scene carries; this replaces no mouse
behaviour, it parallels it.

Control map (when no joystick-subscribing effect owns the stick):

- **Left stick (X/Y)** → Pan/Tilt. Updates the P/T values of the selected scene.
- **D-pad Up/Down** → Brightness (Dimmer look).
- **D-pad Left/Right** → Focus (requires the new **Focus** look type).
- **Right stick** (when the relevant look is attached):
  - **Up/Down** → Gobo 1.
  - **Left/Right** → Gobo 2 if it exists, else Prism.
  - **X/Y** → move the colour wheel between colours when a **Color** look is
    selected (white is the default).
- **Bumpers (L/R)** → step the "unique fixture" selection within the scene.
  - Bumping in a direction **blinks** the now-selected fixture so the operator
    can see which head they're editing.
  - While a unique fixture is selected, edits apply to **that fixture only** and
    are **saved as a per-fixture override**, distinct from the shared look.
- **Right trigger (×2)** → Save. Blinks **twice** to confirm.
- **Left trigger (×2)** → Revert to the last saved setting.

## Save semantics

- If **all** the scene's fixtures hold the **same** value for an edited
  attribute, Save updates the **impacted look(s)** (the shared palette) — one
  change propagates to every fixture via the look.
- If individual fixtures were edited uniquely (bumper-selected), those edits are
  saved as **per-fixture overrides** rather than written back into the shared
  look.

## Delta from current code (the work to do)

1. **[DONE] Retire native `FollowSpotEffect`** (`engine/src/followspoteffect.{h,cpp}`)
   and its routing in `ProgrammerController`. The PanTilt-palette velocity path
   it served disappears entirely — Standard-P/T + followspot is now an error.
2. **[DONE] Run effect scripts in Edit (Design) mode.** `EffectScriptRunner`'s
   Design-mode early-return (slotTick) and DMX suppression (writeDMX) removed;
   the previewed scene's effect now runs and writes in both modes.
3. **[DONE] Drop the `mode() != Operate` early-return** in
   `ProgrammerController::applyDesignJoystick()`. The Aim-target authoring move
   now runs in both modes, gated only by the followspot-effect yield (ownership,
   not mode). Note: the yield still matches on the "followspot" script path; once
   the influence contract lands it should key off "subscribes to joystick".
4. **Send `deadzone`** (and any other global joystick settings) on the
   `joystick` data channel — `Doc` currently sends only pan/tilt/sensitivity, so
   scripts hard-fall-back to 0.05.
5. **Add the influence contract** (`controls` / `requires` / `contradicts`) to
   the effect-script API, host-side validation, and the **popup** error UI.
6. **Drop the followspot `followTarget` binding** — read the Target implicitly
   from the scene's Target look; free-fly (scene fixtures) when none is present.
7. **Enforce shortest-path / no-wipe** on every followspot transition (extend
   the existing `nearestPan` logic to cover all seeds, not just snapToTarget).
8. **Lock in the API tenets**: keep `tick` a pure function, ship a small
   script-test harness, version the API (`apiVersion`), document the full
   manifest + intent vocabulary, and keep data-channel subscription one-line.
9. **Resolve & inject subscribed look domains — once, then on change.** Add
   `effect.reads`, resolve the scene's per-fixture values for the subscribed
   domains (`position/intensity/color/shutter/gobo/prism/focus`) **on first tick
   and cache them**, re-resolving only on a look-change/target-moved event (not
   per tick). Inject on the fixture object so combine effects read-modify-write.
   Ship the reference combine scripts (position→intensity, position→color,
   rainbow, colour-strobe).
10. **Decide composition-time contract checking** (collections/chasers), not
    just scene-assembly checking.
11. **New work (later): the Edit-mode gamepad authoring map**, a **Focus look
    type**, per-fixture unique-edit/override selection, and the Save/Revert
    trigger workflow.
