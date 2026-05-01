# Programmer-mode templates

A *programmer template* is a Virtual Console layout pre-laid-out for a
specific MIDI control surface. Once built, you import the
`Programmer` frame into your show workspace, point each select-button
at your own fixture groups, and your surface "just works" for every
show.

This directory will hold one workspace per supported surface. Each
matches a MIDI input profile under
`resources/inputprofiles/`.

| Surface           | Input profile                          | Template       |
|-------------------|----------------------------------------|----------------|
| Akai APC40 mkII   | `Akai-APC40-mkII.qxi` (already shipped)| see below      |

## Building the APC40 mkII template

This needs to be done once, with an APC40 mkII actually connected so
the input bindings are correct. Save the result here as
`apc40-mkII-programmer.qxw`.

### Prerequisites

1. APC40 mkII connected.
2. **Tools → Inputs/Outputs**: enable the APC40 input, attach the
   `Akai APC40 mkII` profile.
3. Mode: **Operate** is not required for editing — work in **Design**.

### Layout (mapping to APC40 mkII controls)

Lay out a `Programmer` VCFrame with these widgets. Channel numbers
refer to the input-profile channels in
`Akai-APC40-mkII.qxi`.

#### Parameter sliders (8 column faders)

For each, create a `VCSlider`, set **Mode: Parameter**, pick the role,
leave Control Byte = MSB, and bind external input to the listed APC40
slider channel.

| APC40 fader | Profile name | Slider role        | Notes                   |
|-------------|--------------|--------------------|-------------------------|
| 1           | `Slider 1`   | `Dimmer` (Intensity)| Master dimmer            |
| 2           | `Slider 2`   | `Red`              |                          |
| 3           | `Slider 3`   | `Green`            |                          |
| 4           | `Slider 4`   | `Blue`             |                          |
| 5           | `Slider 5`   | `White`            | Falls back silently for non-RGBW fixtures |
| 6           | `Slider 6`   | `Pan` (MSB)        |                          |
| 7           | `Slider 7`   | `Tilt` (MSB)       |                          |
| 8           | `Slider 8`   | `Shutter`          |                          |
| Master      | `Master`     | leave as `Submaster`| Existing role             |

#### Knobs (8 device knobs — for fine pan/tilt and beam params)

Use `VCSlider` widgets too (visual style: knob if you prefer). Set
Mode = `Parameter` and pair MSB/LSB across two knobs each:

| APC40 knob | Profile name | Slider role | Control Byte |
|------------|--------------|-------------|--------------|
| 1          | `Knob 9`     | `Pan`       | LSB (fine)   |
| 2          | `Knob 10`    | `Tilt`      | LSB (fine)   |
| 3          | `Knob 11`    | `Gobo`      | MSB          |
| 4          | `Knob 12`    | `Effect`    | MSB          |
| 5          | `Knob 13`    | `Beam`      | MSB          |
| 6          | `Knob 14`    | `Speed`     | MSB          |
| 7          | `Knob 15`    | `Cyan`      | MSB          |
| 8          | `Knob 16`    | `Magenta`   | MSB          |

(Adjust to match the kind of fixtures you typically run; the role
selector covers all standard QLC+ channel groups.)

#### Selection buttons (8 track-row + a few transport)

For each, create a `VCButton`, set **Action: Programmer: Select
Fixtures**, and bind external input.

| APC40 button row             | Profile name(s)      | Mode      | Initial fixtures/groups |
|------------------------------|----------------------|-----------|--------------------------|
| Track row 1–8 (top scene btn)| `Clip Stop 1`–`8`    | `Replace` | leave empty; the user will assign their groups in their show workspace |
| Stop All Clips               | `Stop All Clips`     | n/a       | bind to a `Clear Selection` action — set this VCButton to **Action: Programmer: Select Fixtures** with NO fixtures or groups checked and Mode = `Replace` (a Replace with empty == clear) |
| Bank Up / Bank Down          | `Up Arrow`/`Down Arrow` | optional | could swap VCFrame pages for color/position/effect parameter banks |

If you want the buttons to *also* recall a stored look, use a separate
button row (e.g. scene launch grid) bound to `Toggle` mode pointing at
your scenes/collections, and reserve clip-stop buttons for selection.

#### Capture / Store / Undo (optional but recommended)

Drop in a small VCFrame with three more buttons bound to the global
toolbar actions (Operate-mode only):

- **Capture Live Edits** — bind to `Solo` button or any free
  transport button on the APC40, set up via QLC+'s **Tools →
  External controls → Map…** menu (this maps a MIDI control to the
  global toolbar action rather than to a VC widget).
- **Store** — same.
- **Undo Last Store** — same.

This makes capture / store / undo single-button on the surface so the
operator never reaches for a mouse during a show.

### Save and check in

`File → Save As…` → `apc40-mkII-programmer.qxw`. Place the file in
this directory and add it to the build's resource install (see the
project root `CMakeLists.txt`'s template install rules).

### Customising for your show

1. Open your show workspace.
2. **File → Open** the template; the Programmer frame is in its VC.
3. Cut it from the template, paste into your show's VC.
4. Right-click each Selection button → Properties → check the fixture
   groups you want it to target.
5. Done. Sliders are role-bound — they automatically follow whatever
   the buttons select.

## Adding new surfaces

For a non-APC40 controller:

1. Make sure an input profile exists in
   `resources/inputprofiles/` (or contribute one).
2. Repeat the build steps with that profile attached.
3. Save here as `<vendor>-<model>-programmer.qxw` and PR.

The widget code is surface-agnostic; the only per-surface artefact is
the template `.qxw` file.
