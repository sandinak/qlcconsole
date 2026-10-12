# Spread, speed masters and Link — Design

TODO.md backlog #1 (Branson, 2026-10-11). Companion to `EFFECTSCRIPT_API.md`
and `CONTROL_SURFACE_DESIGN.md`.

Decisions taken 2026-10-11:

- **"Link" means both** Ableton Link *and* masters that follow each other.
  Masters following masters is built now; **Ableton Link is deferred** — its
  library is GPLv2-or-later, so a build with it would ship as GPLv3 (the app
  is Apache 2.0). It stays a slot in the beat-source design (S5).
- **A fixed few speed masters** (four), always there, so a surface can carry
  them without setup — and **plan automapping onto control surfaces** (S4).

---

## Where things stand (2026-10-11)

- **One global beat, measured wrong.** MasterTimer has a BPM (an `int`), a
  beat source (None / Internal / External: plugin MIDI clock or OS2L, audio)
  and `requestBeat()`. Effects get `_beat`, `_bpm`, `_beatCount`
  (effectinstance.cpp `buildInputsObject()`), but `_beat` is
  `fmod(time since THIS instance started, beat length)`: two effects started
  at different moments are out of phase with each other, and neither a tap
  nor an external beat ever pulls them onto the beat.
- **Tap** (Programming toolbar) uses the last two taps only and sets the
  speed, never the phase.
- **No speed masters.** An effect's rate is its own `speed`/`rate` param in
  Hz, meaning different things in different scripts.
- **Spread is per script**, four ways: `spread` in degrees (the phasers,
  pulses, native strobe), `phaseSpread` 0..1 (orbit, diamond, figure8,
  lissajous), `spread` 0..1 (rainbow, ballyhoo), `spread` in steps
  (gobo-chase), plus `offset` / `reverse` here and there. No wings, blocks
  or groups anywhere.
- What a script gets per head already: `pos{x,y,z}` (metres), `grid{col,row,
  cols,rows}` and its place in the `fixtures` array, from
  `EffectInstance::effectiveCells()` (stage grid for several groups, the head
  layout for one, list order otherwise).

---

## S1 — One beat clock *(DONE 2026-10-11)*

The thing everything else stands on.

- MasterTimer keeps a **beat position**: a running `double` count of beats
  since the clock started, anchored (time, position) and re-anchored on
  every BPM change so it never jumps. `beatPosition()` is readable from any
  thread (mutex-guarded anchor).
- **Fractional BPM** (`double bpm()`); `bpmNumber()` stays, rounded, for the
  stock callers.
- `requestBeat()` (an external beat, a tap) **sets the phase**: the beat
  falls now. External sources nudge rather than snap when they're close, so
  a steady MIDI clock doesn't jitter the lights.
- Effects read the clock: `_beat` = fractional part of the beat position,
  `_beatCount` = beats since the instance started (as documented, now counted
  on the shared clock), and a new `_beatPos` (the shared position itself).
  Two effects on the same BPM are now on the same beat.
- **Tap** averages the last four taps (a gap of 2 s starts over) and puts
  the beat on the last tap.

## S2 — Speed masters (and masters following masters) *(DONE 2026-10-11)*

- **Four masters**, saved in the show. Each is either
  - **Own tempo** — its own BPM and tap, its own beat position; or
  - **Follow** — the main clock or another master, times a **ratio**
    (¼, ½, 1, 2, 4; plus ⅓/¾/1½ for swing). This is the "linking masters"
    half of Link. A follow chain that would loop is refused.
  - plus **Pause** (the position stops; effects on it freeze) and a
    **rate fader** (0–200 %) for riding it live.
- An **effect is put on a master** in the look editor ("Speed: own / Main
  clock / Master 1–4"), stored as a reserved palette param
  (`__speedMaster`, like the existing `__durationMs`). On a master, the
  host feeds the script **beats as time**: `_time` advances one per beat,
  so the effect's speed reads **cycles per beat** (1 = one per beat,
  0.25 = one per bar) and stays locked to the beat. The look editor relabels
  the speed control accordingly. Off a master, nothing changes.
- **Chasers and sequences** on a master: a chase whose tempo is Beats
  steps on its master's beats (ChaserRunner counts the master's whole
  beats; paused, it holds). A Time chase keeps its times.
- **Storage (as built):** the masters are a `<SpeedMasters>` element in
  the show (written only once something differs from the defaults). An
  effect's choice is the palette param `__speedMaster`; a function's (scene,
  chaser, collection, any) is its `SpeedMaster` attribute; a chase step's is
  the step's `SpeedMaster` attribute. Values: `Inherit` (default, not
  written), `Free`, `Main`, `1`..`4`. At run time a chase sets
  `Function::setInheritedSpeedMaster()` on the step's function before
  starting it (the step's choice, else the chase's effective one), a
  collection on each member; `postRun()` drops it, like the fade priority.
  The innermost explicit choice wins: effect, look, step, chase/collection.
- UI: a **Speed Masters** strip (BPM, Tap, follow + ratio, pause, rate) on
  the Programming toolbar's overflow, the Virtual Console (a widget per
  master: tap + fader + BPM), and the control surface (S4).

## S3 — The spread layer *(DONE 2026-10-11)*

The host works out each head's **phase** so scripts don't each do it.

- Per effect (reserved palette params `__spread*`, set in the look editor
  under one **Spread** heading):
  - **Amount** — how far the phase runs across the selection, in cycles
    (0 = all together, 1 = one full wave across them).
  - **Axis** — list order, head-layout grid (columns or rows), or stage
    position (left→right, front→back, height).
  - **Direction** — forward, reverse, centre-out, out-to-centre.
  - **Wings** — 1 (none), 2, 3, 4…: the selection split into mirrored parts.
  - **Blocks** — every N neighbouring heads share a phase.
- Exposed per head as `fixture.phase` (0..1) — additive to the API
  (EFFECTSCRIPT_API.md's compatibility rule), 0 with no spread set.
- The scripts move over one by one: phasers, pulses, orbit/diamond/figure8/
  lissajous, rainbow, ballyhoo, gobo-chase use `fixture.phase`. Their old
  spread params still load (old shows keep working) and are hidden in the
  look editor once the host spread is set.
- The look editor's live preview shows the phase across the plot (a
  gradient on the fixtures) while the Spread settings are edited — built
  as a small plan in the Spread row (SpreadPreview, from
  EffectInstance::spreadPreview()).
- **Target axes**: out from / around the look's Aim target (the Stage
  Shape centre), floor-plan distance and angle; no Aim look → centre
  stage.
- **As built:** `engine/src/spreadlayer.{h,cpp}` (pure; the phases), the
  instance sets each cell's `phase` (cycles, not wrapped) and
  `inputs._spread`; a script opts in with `effect.hostSpread = "<its own
  params>"`. Fifteen scripts take it: the dimmer/colour/pan-tilt phasers,
  native strobe, the amber/iris/UV/zoom pulses, orbit, diamond, figure 8,
  Lissajous, rainbow, ballyhoo, gobo chase; then Stage Shape (outline
  place), drawn path and warm/cool.

## S4 — Control-surface automapping

Uses the existing surface engine (`engine/src/controlsurface*`: roles,
pages, LED feedback):

- New roles: `SpeedLevel[N]` (master N's rate fader), `SpeedTap[N]`
  (tap master N), `SpeedPause[N]`.
- A **Speed page**: on any surface with four or more faders the page maps
  itself — faders 1–4 → the masters' rates, the buttons above them → tap,
  below → pause — no per-show setup. Each tap button's LED **flashes on its
  master's beat** (and dims while paused).
- `Static::Tap` (the always-there tap, never paged) taps the main clock.
  On the PMJ that's the `Favorites` button, the binding left pending in
  CONTROL_SURFACE_DESIGN.md.
- Overlays (PMJ now, APC40 next) can override the automap; the automap is
  the default for a device with no overlay.

## S5 — Ableton Link (deferred)

A fourth beat source beside Internal, Plugin (MIDI clock / OS2L) and Audio:
Link's session tempo and beat phase drive the main clock (S1), so every
master following it follows Link. Blocked on the licence decision; a CMake
option off by default is the likely shape when it comes.

---

## Order

S1 → S2 → S3 → S4. S1 is a fix as much as a feature (effects out of phase
with each other today) and everything else reads its clock. S3 doesn't
depend on S2 and can move ahead if wanted.
