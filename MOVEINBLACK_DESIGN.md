# Move-in-Black — automatic cue look-ahead (mark planner)

Design doc for the piece the operator actually needs: **automatic** pre-positioning
of dark fixtures before an upcoming cue lights them. Slice 1 (manual `Mark` button
+ `MarkEffect` hold/hand-off) is the *mechanism*; this doc is the *planner* that
drives it from the running show. Convention: written before code (à la
`FIXTURESTUDIO_DESIGN.md`).

## The workflow it serves

Branson's show = **Collections** (of Scenes) stepped by a **Chaser** or the **Show
timeline**. When a cue reveals a mover that was dark, the audience sees it *sweep*
into position. The planner removes that: while the fixture is dark, it pre-aims it
to the **next cue's** pan/tilt/colour/gobo/beam, so the reveal is instant.

Fully **automatic** with a **dark-gap gate**; one shared planner fed by **both**
drivers (operator choices).

## Decisions (locked)

- **Driver:** shared layer — Chaser *and* Show timeline both feed the same planner.
- **Trigger:** auto; pre-set once the current cue is stable, **only if** the next
  cue is far enough away to actually move in black. Global on/off.

## Architecture

Three parts. Only the first is new engine surface; the third already exists.

### 1. `CueLookahead` — the shared "what fires next" abstraction

A thin provider that, for whatever is currently driving the show, answers:

```
struct UpcomingCue { Function *fn; int fireInMs; };   // fn = Scene or Collection
QList<UpcomingCue> CueLookahead::upcoming(int horizonMs) const;
```

- **Chaser** driver: `currentStepIndex()` → `computeNextStep()` → `stepAt(idx)->fid`
  gives the next Function; `fireInMs` = current step's remaining hold/duration
  (from `ChaserStep` fadeIn/hold/duration + the runner's elapsed).
- **Show timeline** driver: next `ShowFunction` per track with `startTime()` >
  playhead; `fireInMs = startTime() − playhead`. (Show already sequences these.)

"Currently driving" = the running Chaser/Show that owns the master output (the
Programming tab / Operate already tracks the active playback — reuse that; do NOT
plan from a scene the operator is merely previewing).

One cue ahead is the default horizon; the list form leaves room for N-ahead later.

### 2. `MarkPlanner` — computes marks from the look-ahead

Owned by ProgrammerController, ticked on cue-change (chaser step advance / show
cue boundary) and on a coarse timer as a fallback. Global enable flag
(persisted); a `darkGapMs` threshold (default ~800 ms — needs rig tuning).

Per evaluation:

1. `cue = CueLookahead::upcoming(horizon).first()`. If `cue.fireInMs < darkGapMs`
   → **skip** (no time to move in black; it would sweep anyway).
2. Compute the **next cue's per-fixture output offline** (§3 below): for each
   fixture, its would-be master-intensity + non-intensity channel values.
3. For each **aimable** fixture (has Pan/Tilt — fixed heads never mark):
   - **dark now?** live master intensity ≈ 0 (`Universe::preGMValue`).
   - **lit next?** next-cue master intensity > 0.
   - **would move?** next-cue non-intensity ≠ current non-intensity.
   - all three → `markFixture(fid, nextNonIntensityValues)` (planner-sourced,
     distinct from the manual "capture current output" path).
4. Fixtures previously auto-marked but no longer dark→lit→moving in the current
   plan → `unmarkFixture`. Manual marks are left alone.

Release is already handled: `MarkEffect` auto-hands-off the instant the cue lifts
intensity past its threshold. So the planner only ever *adds/refreshes* marks; the
engine drops them on reveal.

### 3. Offline cue-output computation (reuse, don't invent)

A Scene already knows how to expand its palettes + baked values into
`SceneValue`s (`valuesFromFixtureGroups`/`valuesFromFixtures` + `values()`), the
same path `EffectInstance::buildSceneBaseValues` uses. A Collection = union of its
member Scenes' values (later member wins on a shared channel, matching run order).
From the merged per-fixture channel map:
- **intensity** = value on `masterIntensityChannel()`.
- **non-intensity** = every other channel (what a mark holds).

No function is *started* to do this — it's a pure value computation, so it's safe
to run ahead of time on the GUI thread.

## Control surface

- **Global toggle** — "Auto move-in-black (pre-set dark movers)" — a checkbox/
  button in the Programming toolbar (near Mark) + persisted setting. Off by default.
- **Dark-gap** — the minimum lead time; a setting, seeded ~0.8 s.
- Manual `Mark`/`Unmark` still work and coexist (planner ignores manual marks).
- Future: per-cue "force live" / "force mark" overrides; per-fixture opt-out.

## Build slices

- **2a — offline cue output.** Compute a Function's (Scene/Collection) per-fixture
  intensity + non-intensity map. Unit-testable in isolation; the riskiest logic.
- **2b — `CueLookahead`.** Chaser + Show providers → `upcoming(horizon)`. Verify
  `fireInMs` against real step/cue timing.
- **2c — `MarkPlanner`** wired to cue-change + toggle + dark-gap; feeds MarkEffect.
- **2d — dangle detector** (slice 3 in TODO): a positioned-but-dark fixture that
  matches an upcoming cue = valid pre-set; matches nothing = warn. Falls out of
  the plan the planner already computes.

## Open questions / risks

- **Timing accuracy of `fireInMs`** on the Chaser (fade vs hold vs duration modes;
  external/beat clock) and Show (tempo). The dark-gap gate depends on it.
- **Collection merge semantics** for intensity/colour when members overlap — pick
  "run-order last wins" and verify against how the Collection actually plays.
- **Plan churn** — recompute only on cue boundaries + a coarse timer, not every
  tick, to avoid marking/unmarking thrash.
- **Multi-look cues** — a Collection with its own effects/RGBScripts: the planner
  handles static palette/scene values; effect-driven position is out of scope for
  2a–2c.
- **Rig verification** — the whole feature is only truly testable on hardware/a
  visualiser; build with the offline computation unit-tested and the timing
  logged.
