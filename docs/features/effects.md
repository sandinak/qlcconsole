# Effects

An **Effect** palette runs a small script on its look's fixtures, many
times a second: colour chases, flicker, pan/tilt shapes, pixel patterns,
music-driven effects. Pick one in the look editor's effect browser (see
[The effect browser](programming-tab.md#the-effect-browser)); its settings
appear beside the list. They're grouped here by what they do (the
browser's tabs group them by the kind of fixture they're for); its search
finds any of them by name.

Colours come from the **look**: an effect that uses a colour takes the
look's Colour palettes (first, second…) and its Dimmer, so the same effect
works in any colour.

## Colour

| Effect | What it does |
|---|---|
| **Color Alternate** | Cycles up to four colours across the group |
| **Color Chase** | A band of colour chases through the fixtures; a second colour fills behind it |
| **Color Gradient** | A smooth gradient between two colours, optionally moving |
| **Color Phaser** | A waveform crossfade between two colours, phase-spread across the fixtures |
| **Color Spread** | Two colours distributed across the group |
| **Rainbow** | Continuous hue rotation across the fixtures |
| **Spatial Gradient** | A gradient laid across the rig by where each fixture really is (its 3D position), at a direction you set on a compass |
| **Warm Cool** | Alternates warm (white) and cool (blue) across the fixtures |
| **Amber Pulse** / **UV Pulse** | A phase-shifted pulse or chase on the amber / UV emitters |

## Dimmer

| Effect | What it does |
|---|---|
| **Candle Flicker** | Organic per-fixture brightness flicker around the look's own level |
| **Dimmer Phaser** | A waveform intensity chase with phase spread |
| **Firelight** | Warm flickering fire colours, each fixture on its own |
| **Lightning** | Random storm flashes across the rig |
| **Sparkle** | Random fixtures flash and fade like stars |
| **Spotlight Chase** | A hot spot chases across the rig, with a dimmer trail |
| **Wave** | A travelling sine wave across the fixtures |
| **Native Strobe** | The fixture's own shutter strobe |

## Position (moving heads)

| Effect | What it does |
|---|---|
| **Ballyhoo** | Wide, pseudo-random audience-scan sweeps |
| **Diamond** / **Figure-8** / **Lissajous** | Shapes traced by each head |
| **Drawn Path** | Heads follow a path you draw in the look editor |
| **Fan** / **Orbit** / **Followspot** | Fan out, orbit, or follow a joystick's X/Y |
| **Pan/Tilt Phaser** | Independent pan and tilt sines, phase-spread |
| **Stage Shape** | Heads trace the outline of a shape on the stage floor |
| **Sweep** | A synchronised pan or tilt scan with a cascade |
| **Wander** | Organic drift, each head on its own path |

## Beam

| Effect | What it does |
|---|---|
| **Gobo Chase** | Steps through gobo wheel positions across the fixtures |
| **Iris Pulse** | Iris pulse or chase, closed to open |
| **Zoom Pulse** | Beam zoom pulse or chase, narrow to wide |

## Pixels and grids

These draw on a **grid**: the look's group as laid out in its head layout
(a run of strips, a matrix), or, across several groups, the rig as it
stands on stage.

| Effect | What it does |
|---|---|
| **Confetti** | Random coloured speckles that blink and fade |
| **Meteor** | A meteor with a fading trail sweeps across the pixels |
| **Plasma** | Smooth shifting colour fields |
| **Text** | Words on the grid — scrolling, or still in the middle; see [Text](programming-tab.md#text) |
| **Wand** | A bright wand sweeps across once, then clears |

## Music and control

| Effect | What it does |
|---|---|
| **Audio Spectrum** / **Audio VU** | Light with the live audio spectrum / level |
| **MIDI Keys** / **MIDI Flash** / **MIDI Comet** / **MIDI Puddles** | Light from the notes played on a MIDI keyboard — keys, a flash, comets, ripples |

## Diagnostics

| Effect | What it does |
|---|---|
| **Grid Probe** | Colours each pixel by its grid column (red) and row (blue) — shows how a group's layout maps onto the rig |
| **Step Test Pattern** | For LED-strip steps: each strip counted, each step numbered and named, a spinner that stops if output freezes — see [Step Test Pattern](programming-tab.md#step-test-pattern) |

## WLED

| Effect | What it does |
|---|---|
| **WLED device effect** | Any of the device's own effects, run on WLED fixtures themselves |
| **WLED Breathe** / **WLED Chase** | WLED's effects, with a port that runs on the look's other fixtures too |

See [WLED effects](programming-tab.md#wled-effects).

## RGB Matrix algorithms

The classic **RGB Matrix** function (Functions window) has its own
algorithms — Fire, Text (with a **Centered** style), Plasma and many
scripted ones. Where one shares a name with an effect, the browser lists it
as "&lt;name&gt; (RGB Matrix)".

## Writing your own

An effect is a JavaScript file in your **EffectScripts** folder
(`~/Library/Application Support/qlcconsole/EffectScripts` on a Mac); it
appears in the browser next time the app starts. The script is handed every
fixture (each head of a pixel fixture), the look's colours and its own
settings each tick, and returns what each should do. See the
[Effect script API](../../EFFECTSCRIPT_API.md) for the full reference.
