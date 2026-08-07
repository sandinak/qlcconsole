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

## Build slices (after the layout is confirmed)

- **P0 — decide the real PMJ layout** (buttons / encoders / faders / banks) and the
  static-core assignments. *Needs the hardware / OpenDeck config in hand.*
- **P1 — Design layout**: Fixtures/Groups select grid + paged parameter encoders +
  static core, in Design mode; LED selection feedback.
- **P2 — Runtime layout**: Cues grid + step nav + static core, in Operate; current-
  cue LED.
- **P3 — Xbox roles**: confirm the design-joystick handoff + add a runtime busking
  profile (triggers/bumpers/d-pad) as an HID profile.
- **P4 — Page/mode engine**: the surface auto-repaints on mode/tab/selection change
  (the piece that makes paging feel intelligent rather than manual).

## Open — need from Branson

- The **actual PMJ hardware map**: how many buttons (grid shape), how many encoders,
  any faders, and whether it has its own bank/shift buttons + LED-per-button.
- Whether runtime busking should be **Xbox-only** or the PMJ encoders also live-
  assign during the show.
- Which controls are sacred **static** for *your* hands (everyone's muscle memory
  differs).
