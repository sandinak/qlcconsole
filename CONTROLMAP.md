# Control Map — static MIDI mapping (design + phased plan)

A persistent **{external input → action}** table that lives in the workspace,
is edited in one place, and is dispatched by one engine subscriber — with
LED/fader feedback back to the controller. The point: let a hardware MIDI wing
drive QLC+ (fire looks, run functions, push submasters, hit blackout) **without
placing a Virtual Console widget on screen for every control**. Zero canvas
real-estate.

This is the fork's answer to the stock QLC+ model, where a MIDI CC/note does
nothing until some on-screen VC widget claims that `(universe, channel)` input
source.

## Why this is feasible (architecture)

Input in QLC+ is already a **flat broadcast**, not a per-widget route:

```
plugin (MIDI/OSC/HID)
  → InputPatch::inputValueChanged(uni, ch, value, key)
  → InputOutputMap::inputValueChanged(uni, ch, value, key)   [the one signal]
  → any QObject that connect()s to it and self-filters by (universe, channel)
```

A VC widget is just the most common subscriber. Strip it down and a binding is
only four things:

> **(a set of input sources) + (a subscription to the broadcast) + (a
> self-filter) + (an action)**

The on-screen widget is incidental. Two existing precedents prove widget-free
binding already works in this codebase:

- **Grand Master** — its binding is stored in a *config object*
  (`VCProperties::setGrandMasterInputSource`), not on a widget; the slider only
  *reads* it. → storage model.
- **ProgrammerController** — already subscribes straight to the broadcast with
  no widget (`connectControllerInput()` / `slotControllerInputChanged()`), with
  a learn/capture flow that binds the next incoming control to a pan/tilt axis.
  → engine-subscriber + learn-mode template.

Reusable pieces to lean on:

- `InputOutputMap::inputValueChanged` — dispatch source.
- `InputOutputMap::sendFeedBack(uni, ch, value, params)` — LED/motor feedback
  (only VC widgets call it today; the plugin's `feedbackToMidi()` scales
  0–255 → MIDI, Note-On/Off per profile).
- `InputSelectionWidget` — the standalone assign/learn/custom-feedback widget
  used throughout the property editors.
- `QLCInputProfile` / `QLCInputChannel` — per-channel semantics (button vs
  slider vs encoder), so mode is often inferable, not chosen.

## Storage owner

`Doc` owns one `ControlMap` (mirrors `PowerDistribution` / `MonitorProperties`),
persisted in the `.qxw`. `Doc` — not `VCProperties` — because the whole point is
independence from the Virtual Console.

## Target vocabulary (end state)

| Target            | Actuation                                              |
|-------------------|-------------------------------------------------------|
| Function          | start / stop / flash (toggle or momentary)            |
| Function intensity| fader → `requestAttributeOverride(Intensity, v)`      |
| Submaster         | fader → group/level submaster                         |
| Look / Palette    | recall via ProgrammerController (reuse, don't dup)    |
| Global action     | blackout, Grand Master, blind, park, chaser next/prev, timeline transport (SuspendTimeline / FollowTimecode / ClearLastLook already exist as VCButton actions) |

Mode (momentary/toggle/fader) is inferred from the input channel's
`QLCInputChannel` type where possible, same trick VC widgets use.

## Phased plan

Each phase ships and is testable on `test-workspaces/surfacetesting.qxw` independently. Nothing
changes existing VC/engine behaviour — all additive.

### Phase 0 — Vertical slice *(BUILT — needs hardware validation)*
Prove the full loop before building the model: learn one control → toggle one
Function → light its LED.

- Lives in `ProgrammerController` (it already owns a widget-free subscription +
  learn mode). Minimal, in-memory, **not persisted** — deliberately does not
  lock the schema.
- `learnFunctionTrigger(fid)` / `clearFunctionTrigger(fid)` /
  `hasFunctionTrigger(fid)` / `functionTriggerLabel(fid)`; learn captures the
  next press in `slotControllerInputChanged`.
- Dispatch: a press toggles `Function::start`/`stop`
  (`FunctionParent::ManualVCWidget`). Feedback: `Function::running`/`stopped` →
  `sendFeedBack(uni, ch, 255|0)`.
- UI: right-click a function in the Programming tab's function tree → **Learn
  MIDI Trigger…** / **Clear MIDI Trigger**.
- **Validates the three risky unknowns:** button/note semantics, feedback value
  scaling, `FunctionParent` ownership — on real hardware, before the schema is
  fixed.
- *Known Phase-0 limitations (resolved by Phase 1+):* not saved to the `.qxw`;
  parent id reuses `ManualVCWidget` (could in theory collide with a VC widget of
  the same numeric id); toggle-only; Function targets only.

### Phase 1 — Model + persistence + dispatcher
- `engine/src/controlmap.{h,cpp}`: list of
  `ControlMapEntry { QLCInputSource source; TargetType type; quint32 targetId;
  Mode mode; /* + page, see Phase 6 */ }`.
- `Doc` owns one `ControlMap`; dirty-tracking; `<ControlMap>` load/save in the
  `.qxw`.
- Move the Phase 0 subscriber into `ControlMap` (or a small dispatcher):
  subscribe once, self-filter, resolve `Doc::function(id)`, act with a dedicated
  `FunctionParent` (add `FunctionParent::ControlMap` to avoid id collisions).
- **Exit:** bindings survive save/reload; many functions on many controls; no
  leaked subscriptions on repatch.

### Phase 2 — Authoring UI
- A **Control Map** dialog (grid): each row = an `InputSelectionWidget` (gives
  auto-detect capture, manual pick, *and* custom-feedback editing) + target
  picker (`Doc::functions()`) + mode combo. Add/remove/duplicate rows.
- Reached from a menu/toolbar entry — **not** a VC widget.
- **Exit:** build a 8–16-control map by ear (learn) without touching the canvas.

### Phase 3 — Faders & submasters
- Add `FunctionIntensity` and `Submaster` targets. Fader CC →
  `requestAttributeOverride` / `releaseAttributeOverride`.
- Infer fader vs button behaviour from the profile channel type.
- **Exit:** a CC/motor fader drives function intensity smoothly; 14-bit /
  relative encoders track without jumps.

### Phase 4 — Looks/palettes + global actions
- `Look/Palette` target → route through `ProgrammerController` (reuse its
  palette routing) — closes out the old "MIDI-mapped look recall" backlog item.
- `GlobalAction` target → blackout, Grand Master, blind, park, chaser next/prev,
  timeline transport (reuse the existing VCButton action handlers).
- **Exit:** a bare wing runs looks + transport + blackout with no VC page open.

### Phase 5 — Feedback polish
- Centralize feedback on every target state change
  (`Function::running`/`stopped`/`flashing`, submaster level, global toggles).
- Handle Note-Off vs Note-On-0 per profile; resend full state on (re)connect so
  the wing lights correctly at startup.
- **Exit:** wing LEDs/motor faders always mirror engine state, incl. after
  reload and re-patch.

### Phase 6 — Banking / pages *(decision-gated)*
Only if a small surface should control more than its physical control count. A
modifier/page layer (à la the APC40 SHIFT/BANK chaser stepping already used in
this fork) multiplies the map. **Touches the data model** — reserve a `page`
field on `ControlMapEntry` in Phase 1 even if the layer is built last.

## Open decisions
1. **Banking now or never?** If ever, reserve `page` on the entry in Phase 1 to
   avoid an XML migration later.
2. **Storage owner** — `Doc` (recommended, VC-independent) vs `VCProperties`
   (where Grand Master's binding lives). Recommendation: `Doc`.
