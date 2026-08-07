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
