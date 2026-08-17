# Show Lifecycle — Construction / Test-Validate / Production

Companion to the ongoing "make this cohesive, not haphazard" cleanup pass
(2026-08-14 discussion). Names the workflow phases a show actually moves
through and maps today's tabs/tools onto them, so future refinement work has
a north star instead of picking at whatever's nearest. Directly informs (but
doesn't replace) the "Look" backlog item — Look's *scope* metadata serves
Construction, its *state-as-base-for-effects* idea serves Production.

Status: **DESIGN ONLY** — the model below is settled, and the mechanism for
disabling real output during Construction turned out to already exist
(**Blind**, see below). What's still unbuilt is surfacing the rolled-up
connected/simulated state and defaulting Blind sensibly.

---

## The three phases, per Branson

- **Construction** — mostly done on a computer off the rig. Building the
  score: fixtures, groups, power planning, looks, functions, shows, VC.
- **Test/Validate** — connected to the rig in a fixed space. Matching what
  was built (esp. **Lighting Studio**, the 2D/3D visualizer) to physical
  reality before it travels.
- **Production** — running the rig on the road. Lighting Studio probably
  isn't needed here, but nice to have.

---

## Why this isn't a third `Doc::mode()`

Looked at how the tools people in this space already use handle the same
progression — ETC Eos, MA Lighting grandMA (onPC), Resolume. (QLab doesn't
really have this problem: build-machine and show-machine are usually the same
box in the same room, so it doesn't need the distinction.)

The pattern is consistent across all three: **the edit-safety axis and the
"am I talking to real hardware" axis are orthogonal, not the same dial.**

- Eos: **Blind/Live** is the edit-safety axis (edit cues without touching
  the stage) — unaffected by whether you're on the real console or the
  Offline Editor. The offline/on-site distinction is a *separate* fact about
  which machine you're running on and what's plugged into it.
- grandMA: **onPC** is the identical software used for both construction and
  on-site work; the connective tissue between the two is the **patch +
  visualizer pairing** (MA 3D / Capture / WYSIWYG elsewhere) — a dedicated
  verification step, not a mode swap.
- Resolume: compose the deck → **output-mapping** binds it to real screens at
  the venue → perform. Same shape: build → bind-to-reality → run.

Nobody adds a third top-level mode for "validate." They add a first-class,
*visible* connection state, paired with a verification tool (a visualizer).

**Conclusion:** `Doc::mode()` (Design/Operate) is already the correct,
unchanged edit-safety axis — it's this fork's Blind/Live equivalent. What's
missing is the second, orthogonal axis: **is this document's I/O bound to
real hardware, or fully simulated?** The three phases Branson named are the
three *meaningful combinations* of these two axes, not a state of their own:

| | Design | Operate |
|---|---|---|
| **Offline / simulated** | **Construction** | *(rehearsal — real cues, no hardware; rare, not a priority to support explicitly)* |
| **Connected to rig** | **Test/Validate** | **Production** |

---

## Detecting "connected to rig"

Not new plumbing to build from scratch — the fact already exists per-universe
via `InputOutputMap`/`OutputPatch`/`InputPatch`, just never rolled up or
surfaced. `OutputPatch::pluginName()` is empty when nothing's patched, and the
**Dummy** plugin (`plugins/dummy`) explicitly returns `"Dummy"` for
simulated/no-op output — already used to detect "no real destination" in
`fixturetreewidget.cpp`'s `universeDestination()` helper.

Proposed: a rolled-up `bool` (`InputOutputMap::isConnectedToHardware()` or
similar) = *true* if any patched universe's input or output plugin is
non-empty and not `"Dummy"`. Open question: does **Loopback**
(`plugins/loopback`) count as "connected"? Leaning no — it's dev/internal
routing plumbing, not a real rig — but worth confirming before building.

**AND** not actively silenced by **Blind**. This matters: a real interface
being *patched* doesn't mean it's *live* right now — see below.

### Blind is already the "disable real output" switch (2026-08-15 finding)

Asked whether Design should have an enable/disable switch for output — traced
it and this fork already has exactly that mechanism, just not named or scoped
around this framing:

- `InputOutputMap::setOutputInhibited(true)` → every `Universe::dumpOutput()`
  (`engine/src/universe.cpp`) returns **before calling any
  `OutputPatch::dump()`** — zero packets leave the machine, for every
  patched protocol at once (ArtNet, sACN, DMX-USB, whatever), not just
  ArtNet. The 2D preview keeps running against the internal buffer the whole
  time (`universeWritten()` still fires) — build and see looks safely with
  zero real transmission.
- This is **Blind** in this fork's UI (toolbar button, Control menu, footer
  chip) — and it's *already* scoped exactly right for this model:
  `app.cpp:866-871` force-disables and turns it off the instant the app
  switches to Operate mode ("never let a muted rig survive into a live
  show"). It is structurally a **Construction-only** safety feature already.
- Contrast with the two mechanisms that look similar but aren't it:
  `OutputPatch::blackout()` still transmits (an all-zero frame — correct for
  a *live* blackout, wrong for "stop talking to the network"), and
  `OutputPatch::paused()` still transmits (a frozen last frame). Neither
  actually silences the wire; only `outputInhibited`/Blind does.

**Refined "connected to rig" check:** a real, non-Dummy plugin is patched
**AND** `InputOutputMap::outputInhibited() == false`. Blind engaged should
read as "simulated," regardless of what's physically plugged in.

**Production tab mapping correction:** Blind does **not** belong in the
Production tools list below — it's forced off and disabled in Operate.
Blackout and Panic remain live-safe Production tools; Blind is
Construction/Design-only.

---

## Mapping today's tabs/tools to phase

- **Construction** — Hardware tab (patch, fixture groups, power planning),
  Functions, Programming (looks), Shows (timeline build), Virtual Console
  (layout build).
- **Test/Validate** — Inputs/Outputs (verifying real interfaces match the
  planned patch), **Lighting Studio** (the design-vs-reality checkpoint —
  same job MA 3D/Capture/Augment3d do elsewhere), the dangle detector,
  MarkPlanner/move-in-black, CueLookahead timing, power estimate vs. measured
  draw. Today this phase's only real "path" is a human reading
  `RIG_TEST_PLAN.md` — everything else is scattered, purpose-built tools with
  no shared home.
- **Production** — Simple Desk, the Operate-mode toolbar (Blackout/Panic —
  **not** Blind, which is Design-only and force-disabled in Operate), the
  footer safety chips (MTC/Load/Power/Dangle).
- **Spans phases** — Lighting Studio (heavy in Validate, optional-but-nice in
  Production, per Branson); the footer chips (Validate *and* Production both
  want live truth, not just Production).

---

## Gaps / follow-on work (none of this is built yet)

1. Compute and expose the rolled-up connected/simulated state
   (`InputOutputMap`) — real non-Dummy plugin patched **and** not
   `outputInhibited()` (Blind engaged counts as simulated even if hardware
   is physically patched). No new output-silencing mechanism needed — Blind
   already does that part.
2. Surface it — most natural fit is a footer chip alongside MTC/Load/Power
   (same family of "global system-health" chips built this session,
   `app.cpp`'s `initStatusBar()`).
3. **Default Blind sensibly.** Today Blind is opt-in — you have to remember
   to engage it. Given the actual worry (accidentally driving a real rig
   while just noodling in Construction), should Design mode auto-engage
   Blind whenever it detects a real interface patched, rather than trusting
   the user to flip it? Safety-by-default vs. an extra click to go live for
   an intentional Design-mode bench test — needs a decision, not just code.
4. Turn `RIG_TEST_PLAN.md`'s manual checklist into an actual in-app
   Test/Validate workflow — bigger, deliberately deferred; needs its own
   design pass on where it lives (its own tab? a mode of the Hardware tab?
   a checklist dialog gated on "connected"?).
5. Use this phase lens to retroactively tag `TODO.md`'s backlog — "which
   phase does this serve" is a sharper prioritization question than treating
   the backlog as one flat list.

## Open decisions

- Loopback plugin: connected or simulated? (leaning simulated/dev-only)
- Is Operate+Offline ("rehearsal") worth naming/supporting, or safe to ignore
  as an edge case nobody asked for?
- Auto-engage Blind on entering Design with real hardware patched — default
  on, default off, or a one-time prompt? (see gap 3 above)
- Where the Test/Validate checklist UI actually lives — deferred until (4)
  above gets picked up.
