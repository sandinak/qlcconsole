# Programmer mode — selection-driven parameter controllers

**Status:** in progress on branch `programmer-mode`
**Owner:** active
**Related:**
[capture-live-edits](../../engine/src/capturemanager.h) (this feature
feeds the same recording pipeline)

## Why

VC widgets today bind to *specific* `(fixture, channel)` pairs. With a
72-button / 8-fader / 8-knob surface like the APC40 mk2, that means
manually wiring every fader to every fixture's red channel and so on
— hours of one-show setup that doesn't transfer when fixtures change.

Every full-size lighting console (Hog, MA, Eos, Avolites) instead uses
the *select-then-modify* paradigm: buttons pick fixtures or groups
into a current selection; faders and knobs are bound to *roles* (R, G,
B, Pan, Tilt, Intensity, ...) that resolve against whatever's
currently selected. Configure the surface once; use anywhere.

This proposal brings that paradigm to QLC+, sized to ride on top of
existing primitives.

## What QLC+ already has

- [`QLCChannel::PrimaryColour`](../../engine/src/qlcchannel.h)
  enumerates the role taxonomy:
  `IntensityDimmer`, `IntensityRed/Green/Blue/White/Cyan/Magenta/`
  `Yellow/Amber/UV` (each with a `Fine` LSB variant), plus
  Pan/Tilt MSB+LSB through `QLCChannel::Group::Pan`/`Tilt`.
- [`Fixture::channelNumber(int type, int controlByte, int head)`](../../engine/src/fixture.h)
  resolves "give me the Red MSB channel on this fixture" against the
  fixture's mode definition. Same lookup that `VCXYPadFixture` uses to
  find Pan/Tilt MSB.
- `FixtureGroup` already exists.
- `VCButton`, `VCSlider`, `VCXYPad` already participate in the input
  profile / DMX write loop.
- `CaptureManager` (sibling feature) already records `(fxi, channel,
  value)` overrides — so any new widget that feeds this pipeline
  inherits Apply / Save-as-new / Undo / auto-store-on-chaser
  automatically.

So **no engine refactoring is required**. The work is additive.

## Architecture

### 1. Programmer selection state on `Doc`

```cpp
class Doc {
    QList<quint32> programmerSelection() const;
    void setProgrammerSelection(const QList<quint32>& fixtureIds);
    void addToProgrammerSelection(const QList<quint32>& fixtureIds);
    void removeFromProgrammerSelection(const QList<quint32>& fixtureIds);
    void toggleInProgrammerSelection(const QList<quint32>& fixtureIds);
    void clearProgrammerSelection();
    bool isInProgrammerSelection(quint32 fixtureId) const;

signals:
    void programmerSelectionChanged();
};
```

Order is preserved (so a "select first" parameter can target the
selection's head fixture). Membership lookup is backed by a `QSet`
member for O(1) `isInProgrammerSelection`.

The selection survives mode toggles. Cleared on workspace clear.

### 2. New `VCButton` actions

Extend
[`VCButton::Action`](../../ui/src/virtualconsole/vcbutton.h):

```cpp
enum Action {
    Toggle,
    Flash,
    Blackout,
    StopAll,
    SelectFixtures   // new
};
```

A `SelectFixtures` button stores:

- A list of fixture ids and/or fixture-group ids (group members are
  resolved at click time so dynamic group changes are reflected).
- A `SelectionMode`: `Replace` / `Add` / `Remove` / `Toggle`.

On press, the button calls the appropriate `Doc` setter. The button's
checked state binds to `Doc::programmerSelectionChanged()`: it
appears "active" when *all* of its fixtures are currently selected.

### 3. New `VCSlider` mode: `Parameter`

Extend
[`VCSlider::SliderMode`](../../ui/src/virtualconsole/vcslider.h):

```cpp
enum SliderMode { Submaster, Level, Playback, ClickAndGo, Parameter };
```

A `Parameter` slider stores:

- `QLCChannel::Group` (Intensity, Pan, Tilt, Colour, Gobo, Effect,
  Speed, Maintenance, Beam, Shutter)
- For Intensity: a `QLCChannel::PrimaryColour` (Red, Green, Blue,
  White, Amber, UV, ... or Dimmer for the master dimmer).
- A `controlByte`: MSB (default) or LSB (for fine sliders / pan-tilt
  precision).
- A `BlendMode`: Absolute (set all selected to slider value, default)
  or Relative (apply slider delta to each fixture's current
  programmer value).

`writeDMXParameter()` runs each tick when value changed:

1. Read `Doc::programmerSelection()`.
2. For each fixture, call `Fixture::channelNumber(role, controlByte,
   head)` (or scan all heads for multi-head moving heads).
3. For each resolved channel: write via `GenericFader` (same path as
   `Level` mode) **and** call `CaptureManager::recordOverride()` so
   capture / undo / auto-store all just work.

A slider that resolves to no channels (selection has no fixtures with
the role) shows a subtle visual cue and is harmless — just nothing
moves.

### 4. Properties UIs

- `VCSliderProperties`: a "Parameter" tab with Group + PrimaryColour +
  ControlByte + BlendMode dropdowns.
- `VCButtonProperties`: a "Selection" tab with a fixture-tree picker,
  a fixture-group picker, and the SelectionMode radio.

### 5. Surface-agnostic templates

Per-controller workspace files under
`resources/templates/programmer/`:

- `apc40-mk2.qxw` (first target) — pre-laid widgets matching APC40
  mk2's 8-fader / 9-fader-master / 8-knob / 8-button track-rows /
  scene-launch grid. Plus the matching MIDI input profile under
  `resources/inputprofiles/Akai-APC40-MK2-Programmer.qxi`.
- Future: `launchpad-pro.qxw`, `xtouch-mini.qxw`,
  `behringer-bcr2000.qxw`.

A template is just a `.qxw` containing a fixture-agnostic VCFrame
called "Programmer" with widgets in `Parameter` and `SelectFixtures`
modes. The user opens their own workspace, imports the Programmer
frame, then assigns their fixtures to the existing buttons (drag a
fixture or group onto a button).

### 6. Visual feedback to the surface

For controllers that accept input feedback (most do — APC40 lights up
buttons via MIDI feedback), the existing input-feedback channel does
the work. `SelectFixtures` buttons emit feedback when their selected
state changes via `Doc::programmerSelectionChanged()` listeners
already in `VCWidget`.

## Phasing

1. **Selection state on `Doc`**. Pure engine; no UI.
2. **`VCButton` `SelectFixtures` action** + minimal save/load. No UI
   editor yet; properties are XML-only at first to validate the
   plumbing.
3. **`VCSlider` `Parameter` mode** + `CaptureManager` hook + minimal
   save/load.
4. **`VCSlider` properties UI** for Parameter mode.
5. **`VCButton` properties UI** for SelectFixtures.
6. **APC40 mk2 template** (workspace + input profile + docs).
7. **16-bit Pan/Tilt fine encoders** — extend Parameter mode to a
   "fine" companion slider that pairs to a "coarse" via a property.
8. **VCKnob widget** — appearance sugar for parameter-mode controls
   that *want* to look like knobs on an APC40-style surface. Optional;
   sliders styled small look fine.

## Out of scope (defer)

- **Per-VCFrame selection scope**. Global selection (matches console
  convention) is simpler and covers 95% of cases. If two-operator
  setups demand it, revisit.
- **Selection groups beyond raw `FixtureGroup`**. (Hog-style "groups
  of groups", recordable subsets, etc.) Use `FixtureGroup` for now.
- **Encoder banks / parameter pages**. APC40's "device" mode page
  switching can be done with a button that swaps which Parameter
  mode each slider is in via VC pages — defer until someone asks.
- **Effect engines parameters** (Pan-tilt circles, intensity
  ripples). Real consoles call these "FX engines"; QLC+ calls them
  EFX. Programmer integration with EFX is a separate proposal.

## Files most likely to change

Engine:
- [`engine/src/doc.{h,cpp}`](../../engine/src/doc.h)

UI core:
- [`ui/src/virtualconsole/vcbutton.{h,cpp}`](../../ui/src/virtualconsole/vcbutton.h)
- [`ui/src/virtualconsole/vcbuttonproperties.{h,cpp}`](../../ui/src/virtualconsole/vcbuttonproperties.h)
- [`ui/src/virtualconsole/vcslider.{h,cpp}`](../../ui/src/virtualconsole/vcslider.h)
- [`ui/src/virtualconsole/vcsliderproperties.{h,cpp}`](../../ui/src/virtualconsole/vcsliderproperties.h)

New resources:
- `resources/templates/programmer/apc40-mk2.qxw`
- `resources/inputprofiles/Akai-APC40-MK2-Programmer.qxi`

## Effort estimate

- Stage 1 (Doc selection): half-day
- Stage 2 (Button action + load/save): half-day
- Stage 3 (Slider Parameter mode + capture hook): one day
- Stage 4 (Slider properties UI): half-day
- Stage 5 (Button properties UI): half-day
- Stage 6 (APC40 template + profile + docs): one day
- Stage 7 (16-bit fine pairing): half-day
- Stage 8 (VCKnob optional): half-day

Total: ~4–5 focused days for a fully shippable feature.

## Test approach

- Unit-ish: a workspace with a moving head + an RGBW par + a fixture
  group containing both. Manually exercise all 4 button actions
  (Replace/Add/Remove/Toggle) in design mode; verify selection state
  via debug log.
- Capture integration: select one fixture, move R slider in capture
  mode, click Store, verify the right scene takes the change. Repeat
  with two fixtures selected; verify both get the value (Absolute
  mode).
- APC40 mk2 hardware test: import template, add two fixture groups,
  walk through select-then-modify across each group, confirm with
  capture's auto-store on chaser advance.
