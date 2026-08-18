# Control-surface workflow — PMJ + Xbox

A vision for how the **PMJ (OpenDeck)** and the **Xbox controller** are used across
show **design → refinement → runtime**, so we map with intent, not just because we
can. Doc-first, before the PMJ mapping build.

## Philosophy: two hands, not two remotes

The surfaces are **complementary**, with distinct jobs — never redundant:

- **Xbox = the expressive / spatial hand.** Analog sticks and triggers: grab a
  beam and *point* it, swell intensity, sweep an effect. Continuous, gestural,
  played by feel. Best for *aiming and busking*.
- **PMJ = the structured / precise hand.** Buttons (select, fire, toggle),
  encoders (exact parameter values), LED feedback. Discrete, addressable, precise.
  Best for *selecting, dialling, and firing cues*.

You reach for the Xbox to **feel** a move and the PMJ to **nail a number** or
**pick a thing**. Most tasks use both within seconds of each other.

## Static core vs. context pages

- **Static core** (never pages — muscle memory): Blackout, Grand Master, Clear
  programmer, Record / Update, Blind, Tap tempo, Auto-MIB, and the **Mode/Page**
  selector itself. A handful of controls you can hit blind, always meaning the
  same thing.
- **Everything else pages by context.** The tool already knows the **mode**
  (Design vs Operate) and the **active tab / selection**, so the surface *follows
  what you're doing*: editing a scene → fixture-select + parameter encoders;
  running the show → cue banks. Explicit **page buttons** switch the encoder bank
  (Position / Colour / Beam / Effect) and the grid mode (Fixtures / Groups /
  Palettes / Cues). Paging is why a small surface can do a big rig.

## Capability roles (map to whatever the PMJ actually has)

Design in **roles**, then bind them to the PMJ's real buttons/encoders/faders:

| Role | Surface | Notes |
|---|---|---|
| **Select** fixture/head/group | PMJ grid (Fixtures/Groups mode) | LED = selected; banks page a big rig — the APC40 pattern |
| **Aim** (pan/tilt coarse→fine) | Xbox LS (coarse) + RS (fine) | reuses the design-mode joystick (writes pan/tilt to the focused scene) |
| **Dial a parameter** | PMJ encoders (paged) | relative encoders w/ live preview + LED-ring value (already built) |
| **Continuous expression** | Xbox triggers / bumpers | intensity swell, zoom, effect rate — live feel |
| **Fire / navigate cues** | PMJ grid (Cues mode) + GO/step | chaser next/prev on SHIFT/BANK (as on APC40) |
| **Global** | static core | blackout, GM, blind, tap, Auto-MIB |
| **Store** | Record/Update static + A/B on Xbox | Programmer Save updates scenes inside chasers/collections |

## Phase 1 — DESIGN (building a look)

Mode = Design, Programming tab. The surface is in **edit** layout.

1. **Select** the fixtures: PMJ grid in *Fixtures* mode — press to select heads
   (LEDs light the selection); page buttons swap banks (spots / washes / pixels).
2. **Aim**: grab the Xbox **left stick** → the selected movers pan/tilt live in the
   2D monitor; **right stick** for fine. **Triggers** = zoom / focus.
3. **Dial** the rest: PMJ encoders on the *Colour* page → hue / CTO / wheel; flip
   to *Beam* page → gobo / prism / iris; *Position* page for exact numeric pan/tilt.
   Every turn shows live in the monitor.
4. **Store**: static **Record** → save as a look/scene (SS-PP.II naming); static
   **Clear** resets the programmer for the next look.

*Walkthrough — "warm downstage wash":* Fixtures-page, tap the four wash buttons
(LEDs on) → intensity encoder up → Colour-page, amber → Record. ~8 seconds, eyes
on the rig.

## Phase 2 — REFINEMENT (tweaking what exists)

1. PMJ grid in *Looks/Cues* mode → press a look to **load it into the programmer**
   (LED shows which). 
2. **Nudge**: encoders fine-tune values; Xbox stick re-aims — all with live preview.
3. **Update**: static **Update** re-records into the same look — and the Programmer
   Save workflow propagates it into every chaser/collection that uses it.
4. **Compare**: hold a page button for a quick A/B (before/after) while deciding.

*Walkthrough — "spots sit 2° high across the chorus":* select the spot group (one
Groups-page button) → a hair of down-tilt on the Position encoder (or a tap of RS)
→ Update. Fixed for every cue that uses the group.

## Phase 3 — RUNTIME (operating the show)

Mode = Operate. The surface **repaints to cue layout**.

1. **Cues**: PMJ grid = cue list / song sections; pages per song. LED = current
   cue. GO + SHIFT/BANK = next / previous step.
2. **Static** always-there: Blackout, Grand Master, Blind, Tap, **Auto-MIB**, Flash.
3. **Busk with the Xbox** — the expressive layer *over* the running timeline:
   **left stick** = live pan/tilt override / followspot pin on a selected fixture
   (the monitor already has a followspot pin); **triggers** = manual intensity
   swell / bump; **bumpers** = effect rate/level; **d-pad** = nudge timing. You
   play the rig like an instrument without touching the programmed cues.

*Walkthrough — "unplanned drum solo":* timeline keeps running the song; you grab
the Xbox **right trigger** to swell the drum-riser wash and sweep a beam with the
**left stick** — pure performance, no reprogramming.

## The specific questions, answered

- **Paging + static:** yes — a static core you hit blind, everything else pages by
  context (Design vs Operate) and explicit page buttons.
- **Button sets to select individual fixtures (like APC40):** yes — Fixtures/Groups
  grid modes with LED selection + banks for big rigs.
- **Encoders for positions:** yes — as the *precise* complement to the stick's
  *spatial*. Encoders = numeric/fine + non-position params; stick = grab-and-point.
- **Joystick vs controller — both or one?** **Both, with clear roles.** Xbox =
  spatial/expressive (aim, swell, busk); PMJ = structured/precise (select, dial,
  fire). They hand off constantly; neither replaces the other.

## Reuse — already built, wire don't rebuild

- **Design-mode joystick** (`applyDesignJoystick` — writes pan/tilt to the focused
  scene). → Xbox aiming.
- **Relative encoders** — encoding picker + live preview + step + invert, profile-
  free per-widget mapping. → PMJ parameter encoders.
- **Followspot pin** in the 2D monitor. → Xbox runtime override.
- **Chaser step buttons** (SHIFT/BANK next/prev, per the APC40 memory). → PMJ cue nav.
- **Auto-MIB, Blind, Park, Flash, Highlight** — already have engine + buttons.
- **Per-widget MIDI mapping** + programmer-mode LED feedback (from the APC40 work).

## Hardware: PMJ Black 1 (actual — `resources/inputprofiles/PMJ-Black-1.qxi`)

A **10-strip + mode-button** board (not a pad grid), which maps almost 1:1 to the
workflow:

- **Faders (11):** `Master` + `Ch 1–10` → Grand Master + 10 strip levels.
- **10 channel strips:** each has `N` (select/fire), `N-Up`, `N-Down`, `N-Load` →
  the 10 addressable "things" in the current page.
- **Encoders (4):** `Enc 1–4` (with push) → the paged parameter encoders.
- **Transport:** `Go`, `Back`, `Left`, `Right`, `Pre Page`, `Next Page`.
- **Mode buttons:** `Favorites`, `Set`, `Effects`, `Groups`, `Looks`, `Macros`,
  `Fix Cont` → **these ARE the pages**, in hardware.
- **LEDs:** Note-On on **MIDI ch 9**, velocity = brightness **0–15** (per
  `PMJ-Black-1-idle.qxm`). This is the "what's valid in the moment" channel.

**Insight:** `Groups`/`Looks`/`Effects`/`Macros`/`Fix Cont` select what the 10
strips address; `Pre/Next Page` bank through >10 items; the LEDs show which strips
are populated/selected/active *right now*. The board is already a paging console —
we just teach the tool to drive it.

## The general control-surface engine (what Branson actually asked for)

Don't hard-map the PMJ. Build a **surface-agnostic layer** and make each device an
**overlay** onto it — so the PMJ, the **APC40 mk2**, and the next board all light
up and page the same way.

Three parts:

1. **Surface model** — from the device's input profile: its controls typed as
   *button (LED-capable)*, *encoder*, or *fader*, each with an input address and
   (if it has one) an LED/feedback address + brightness range. Read generically; a
   new device is just a profile + its LED addressing.
2. **Logical roles + pages** — the *meaning* layer, device-independent:
   `select[N]`, `load[N]`, `level[N]`, `param[E]`, `page:Groups|Looks|Effects|…`,
   `transport:go/back/next/prev`, and the static core. Each role has a
   **context-aware state** the engine computes from the app (mode, active tab,
   selection, what's populated): *valid / active / selected / empty* → drives the
   LED (bright / dim / off / colour where supported).
3. **Overlay** — a per-device binding of physical controls → logical roles + the
   LED map. The PMJ overlay binds `Groups`→page, `1..10`→select[1..10],
   `N-Load`→load[N], `Enc 1..4`→param[1..4], `Master`→GM, etc. The APC40 mk2
   overlay binds its pads/knobs to the *same* roles.

The engine owns the **feedback loop**: on any relevant app change (selection,
mode, page, cue) it recomputes role states and repaints every surface's LEDs. That
single generic loop is the "LED lighting to identify what's valid in the moment,"
for every board at once.

## Build slices (revised — engine-first, then overlays)

- **P0 — CONTROL SURFACE ENGINE (core, device-agnostic).** The surface model
  (typed controls + LED addresses from a profile), the logical role/page vocabulary
  with context-aware state, and the generic **feedback loop** that repaints LEDs on
  app change. No device specifics yet — proves the abstraction.
- **P1 — PMJ overlay + LED.** Bind the PMJ's controls to the roles; light the
  strips/modes per validity (populated/selected/active). Start with the **Design
  page** (Groups/Looks/Fix Cont → 10-strip select + Enc 1–4 params + Master GM).
- **P2 — Runtime page.** `Looks`/`Macros` as cue banks, `Go`/`Back`/`Left`/`Right`
  as transport, current-cue LED. Same engine, different page states.
- **P3 — APC40 mk2 overlay.** Second device onto the SAME roles — proves the
  engine generalises (and revives the APC40 work).
- **P4 — Xbox roles.** Confirm the design-joystick handoff + a runtime busking HID
  profile (sticks/triggers/bumpers) layered over the timeline.

## Open — confirm before/along P0

- **Static core for *your* hands**: which controls never page (proposed: `Master`
  = GM, plus a couple of mode buttons reserved for Blackout / Blind / Auto-MIB /
  Tap — say which).
- Whether the **10 faders** are strip levels (per selected fixture/group) or fixed
  submasters — affects P1.
- Runtime busking: **Xbox-only**, or PMJ encoders also live-assign mid-show.

## P1 plan — PMJ overlay + LED (2026-08-18, Branson back at the rig, PMJ connected)

Re-read `PMJ-Black-1.qxi` (the input profile) and `PMJ-Black-1-idle.qxm` (the LED
init template) against the 4 open "board facts" from the P0 handoff. Two are now
answered from the files themselves — no rig time needed for those. Two still need
you, now that the board's actually connected.

**Resolved from the files:**

- **LED output channel** — `PMJ-Black-1-idle.qxm`'s init message is 54 repeats of
  `98 <note> 0F`. `0x98` = Note-On on **MIDI channel 9** (0x90 | channel index 8).
  Matches the design doc's existing assumption — confirmed, not just assumed.
- **LED velocity scale** — the design doc's open question ("board wants 0-15; QLC
  feedback maps to 0-127 — which value is full?") turns out to be based on a
  slightly wrong premise. The profile's own `<Feedback LowerValue="15"
  UpperValue="127"/>` on every LED-capable button is the answer already baked in:
  **15 = dim/idle** (what the init message paints everything at, so the board
  isn't pitch-black on connect), **127 = full/active**, and presumably **0 =
  fully unlit** (a control QLC+ never touches, vs. one it's deliberately dimmed).
  This maps directly onto `ControlSurface::brightness()`'s existing 4-state curve
  if we treat `maxBrightness` as 127 and floor `State::Valid` at 15 instead of 0
  (a small tweak — right now `Valid` = `maxBrightness/4` ≈ 31, which would already
  read as lit-not-dim on this board; worth confirming by eye once the overlay's
  running, not something to guess blind).

**Resolved (2026-08-18), via `qlcplus-midi-profiler` (a separate, already
fairly mature tool Branson had built in a prior session — `/Users/branson/git/
qlcplus-midi-profiler`, see its own README for the full command set):**

1. **Encoder ROTATION.** `qlc-midi monitor OpenDeck` (after clearing a macOS
   Input-Monitoring permission gate that was silently blocking all MIDI input —
   not a bug in the tool, just needed granting to the terminal/VS Code process)
   showed `Enc 1-4` sending exactly `CC 11-14` on MIDI channel 9, value `1` one
   direction / `127` the other — the classic twos-complement relative-encoder
   pattern. Hand-corrected `maps/pmj-black-1.json` (type `Button` → `Encoder`)
   and regenerated with `qlc-midi generate --idle-level 15 --init-template
   profiles/PMJ-Black-1-idle.qxm` (the `--idle-level 15` matters — the plain
   default regenerates every LED's `Feedback` at `LowerValue="0"`, which would
   have silently undone the deliberate "steady dim glow instead of pitch black
   on connect" behavior the current profile already has). Copied into
   `resources/inputprofiles/PMJ-Black-1.qxi` — diff against the previous
   tracked version is now exactly the 4 encoder channels gaining `<Type>
   Encoder</Type><Movement Sensitivity="1"/>`, nothing else changed. **One
   config step left for Branson**: `generate` reported "MIDI channel encoding:
   embedded at bit 12 — set the QLC+ input line to 'any' MIDI channel" — the
   PMJ's QLC+ input line needs to be set to listen on **any** MIDI channel
   (not pinned to one) for the regenerated channel numbers to resolve
   correctly.
2. **Static-core button placement.** The board's actual named buttons (from the
   profile) are: `Go`, `Back`, `Left`, `Right`, `Pre Page`, `Next Page`, `O`,
   `Favorites`, `Set`, `Effects`, `Groups`, `Looks`, `Macros`, `Fix Cont` — no
   button is labeled Blackout/Blind/Tap/GM. Design doc's proposal: `O` →
   Blackout, `Set` → Blind, `Favorites` → Tap. **Your call** — keep that mapping,
   or reassign. (`Master` fader → Grand Master is the one static-core binding
   that's unambiguous — it's the only fader with no strip number.)

**Role table for PMJ Black 1 (Design page, Phase 1 of the design doc) — ready to
build once #1 above is answered:**

| PMJ control | Role | Notes |
|---|---|---|
| `Master` fader | `Level(-1)` = Grand Master | static core, never pages |
| `Ch 1-10` faders | `Level(1..10)` | per-strip level — the open "submaster vs. selected-fixture" question above still applies |
| `1-10` buttons | `Select(1..10)` | fire/select strip N on the current page |
| `N-Load` (1-10) | `Load(N)` | load item N into the programmer |
| `N-Up`/`N-Down` | *unassigned* | not in the current Role vocabulary — likely `Param` fine-nudge for strip N, or bank-within-strip; needs a decision, see below |
| `Enc 1-4` (push) | `Param(1..4)` push-to-... | reset-to-default is the EOS/Ma3 convention (see parity section below) — proposed, confirm |
| `Enc 1-4` (turn) | `Param(1..4)` | resolved — CC 11-14, ch9, `Encoder` type, ready to bind |
| `Groups`/`Looks`/`Effects`/`Macros`/`Fix Cont` | `Page(id)` | the page switch — hardware buttons ARE the page selector, exactly as the design doc already called out |
| `Go`/`Back`/`Left`/`Right` | `Transport::Go/Back/Left/Right` | direct 1:1 with the existing enum |
| `Pre Page`/`Next Page` | `Transport::Prev/Next` | banking through >10 items on the current page |
| `O` | proposed `Static::Blackout` | pending your call above |
| `Set` | proposed `Static::Blind` | pending your call above |
| `Favorites` | proposed `Static::Tap` | pending your call above |

`N-Up`/`N-Down` are the one set of physical controls with no obvious Role yet —
worth discussing directly: on a lot of small boards these step a value up/down by
a fixed increment (an alternative to grabbing the encoder), which would make them
a **discrete-step sibling to `Param`**, scoped to strip N rather than the paged
encoder bank. That reads as genuinely useful for "levels and positions" fine
refinement without needing to grab an encoder at all — worth confirming that's
what they're for on this board before binding them to something else.

## Parity with EOS / grandMA3

You asked for "honest parity" with how other consoles use their surfaces, not
just internal consistency — worth naming where the roles above already match a
convention those desks use, and where the PMJ's actual hardware can't quite get
there:

- **Parameter-category paging = EOS's Position/Color/Beam/... buttons, Ma3's
  encoder-bar pages.** Direct match: `Groups`/`Looks`/`Effects`/`Macros`/`Fix
  Cont` are exactly this idea, just PMJ-specific labels instead of ETC/MA's.
  Nothing to change — the design doc already landed here independently.
- **Context-aware LED = both desks' "populated vs. empty" fader/button
  indication** (EOS's fader-page LCD blanks an unpatched fader; Ma3 dims an
  empty executor). The `State::Empty/Valid/Selected/Active` vocabulary already
  models this generically — P1 is just actually wiring real app state into it
  for the PMJ specifically.
- **Highlight.** EOS's dedicated Highlight button (temporarily slam the
  selection to a visibility look) has a direct, already-built analog: Lighting
  Studio's `MonitorFixtureItem::setGhosted()`/the whole ghosted/locked-layer
  visual we already use — the same underlying idea (make the relevant thing
  obviously distinct), just currently a canvas-only affordance. Worth a future
  PMJ static-core button once one's free, but not blocking P1.
- **Where PMJ hardware genuinely can't match EOS/Ma3, and that's fine:** neither
  desk convention assumes a 10-strip board — EOS's channel/parameter encoders and
  Ma3's executor faders both assume either a full fader wing or a command-line
  keypad for exact numeric entry, which the PMJ doesn't have. The design doc's
  `Enc 1-4` + push-to-select-page pattern is the honest PMJ-scale equivalent of
  "dial in a category, then a number" — not a lesser version, a smaller-board
  version. Don't chase literal 1:1 parity (e.g. a command line) the board can't
  physically support; match the *convention* (page → precise value) at the scale
  this board actually offers.
- **One real gap worth closing for genuine parity:** both EOS and Ma3 let you
  step through the *current selection* (Next/Last, or a "Select Last" button)
  independent of the fixture-select grid — useful mid-focus-session without
  re-grabbing the grid. PMJ's `Left`/`Right` buttons are currently only slated
  for page-adjacent stepping in the table above; consider binding them to
  "step selection" instead (or in addition, context-dependent on which page is
  active) — flagging as a discussion point, not deciding unilaterally here.

## P2 — Selection mode: what a whole SCENE ties to the faders (2026-08-18)

P1 (slices 7-8) built fader/encoder mapping keyed on **whichever single
palette is focused in the Look Editor** — great for precise authoring of one
look, but a scene is usually several looks across several targets, and the
board should be useful without drilling into one specific look every time.
Branson: "it's clear when we select a specific palette, but what about the
entire scene — should we identify specific things on the controller to
specific things in the scene... have an option to select specific heads in a
scene and alter those values via slider?"

**The substrate for this already exists and is completely unused today:**
`ProgrammerController::programmerSubSelection()` — a `QSet<quint32>` of
fixture ids, toggled via `toggleInProgrammerSubSelection(fid)` /
`clearProgrammerSubSelection()`. It already has real engine teeth:
`tryRoutePaletteEdit()` (`engine/src/programmercontroller.cpp:520-523`)
checks it to decide whether a pad edit routes to the shared group palette or
to per-fixture channel overrides. Nothing in the UI or any hardware currently
sets it. This is exactly "select specific heads and alter those values" — P2
is wiring the PMJ into a mechanism that was already built for this, not
inventing a new one.

**Two modes, switched automatically by app state, no manual mode button:**

- **Look-edit mode** (P1, unchanged): a specific palette is focused in the
  Look Editor → faders/encoders are context-aware to *that palette's type*
  (`PMJOverlay::faderInUse()`, slices 7-8). Precise single-look authoring.
- **Selection mode** (new): nothing's focused in the Look Editor, or
  `programmerSubSelection()` is non-empty → faders/encoders take **fixed**
  roles and act on the selection. The quick, muscle-memory mode real consoles
  default to.

**Select(1-10) / Load(1-10) — finally wired, using the Role vocabulary these
were already named for:**

- **`Select(N)`**: toggle target N (by canvas order in the open scene) into
  or out of `programmerSubSelection()` — **toggle multi-select**, confirmed
  with Branson (builds a group like "select 1 thru 5" on a real console,
  rather than Select acting as a redundant single-pick radio that duplicates
  `Load`).
- **`Load(N)`**: `clearProgrammerSubSelection()` then select only N — jump
  straight to one thing, matching the Role's existing doc comment ("load
  item N into the programmer").
- **>10 targets in one scene**: confirmed with Branson — **page through**
  using the already-unwired `Page` button/role to bank-switch which 10
  targets Select/Load currently address, rather than silently capping at the
  first 10.

**Fixed encoders, always active in Selection mode:**

- **Enc 1/2 → Pan/Tilt (H/V)** — generalizes `nudgeDesignPanTilt()` (P1
  slice 4) to write per-fixture channel overrides for the *selection*
  (via the same per-fixture-override path `tryRoutePaletteEdit()` already
  routes to when a sub-selection is active), not just a focused PanTilt
  palette.
- **Enc 3/4 → Focus + Zoom** — confirmed with Branson. The next two most
  commonly-grabbed continuous beam parameters after pan/tilt.

**Fixed faders:**

- **7-10 → RGBW of the selection** — confirmed with Branson. A fast,
  always-available color path that doesn't require opening a Color look;
  writes per-fixture overrides the same way the encoders do.
- **1-6 → intensity, one fader per selected fixture** (up to 6 addressable
  at once) — this is the answer to the "`Level(1..10)`: submaster vs.
  selected-fixture" open question the P1 role table has been carrying since
  slice 1: **selected-fixture**, not submaster.

**Not yet designed — needed before this is buildable:**

- The concrete write path: a selection-keyed sibling to `nudgeDesignPanTilt`/
  `setDesignColorChannel`/`setDesignDimmerValue` that takes a `QSet<quint32>`
  (or reads `programmerSubSelection()` directly) instead of a `paletteId`,
  and writes per-fixture channel overrides through the same DMX-pad path
  `tryRoutePaletteEdit()` already exists for — needs a `ProgrammerController`
  method audit to find (or build) the right entry point for "write a raw
  channel value for fixture F, channel group G, right now."
- LED highlighting for Select(1-10)/Load(1-10) — the `State::Empty` default
  from P1 slice 1 should become real once these are wired: lit for targets
  that exist on the current page, brighter/`Selected` for whichever are in
  `programmerSubSelection()`.
- How "canvas order" is actually enumerated for a scene's targets (fixture
  groups first then loose fixtures? Declaration order? Needs a concrete,
  stable definition before Select(N) can mean the same thing twice in a
  row).
- Whether Enc 3/4 (Focus/Zoom) and faders 7-10 (RGBW) write into the
  selection's *existing* palette assignments (deviate a shared group look
  per-fixture, same as pan/tilt) or bypass palettes entirely for a raw
  per-fixture channel poke — almost certainly the former, for consistency
  with pan/tilt and with `tryRoutePaletteEdit()`'s existing model, but worth
  stating explicitly once implementation starts.
