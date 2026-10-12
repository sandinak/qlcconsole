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
| **Stage Shape** | Heads trace the outline of a shape (circle, polygon, star, line) round the look's Aim target, on the floor or stood up as a wall. With **Spread** on, the spread decides which head takes which point — Axis *stage left to right* puts the leftmost heads on the left of the line, no beams crossing |
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

### On the beat

Effects that follow the beat (a **Beat sync** setting, or a beat-made
effect) all read **one beat clock**, so they're on the same beat however far
apart they were started. Turn the clock on in the Programming toolbar's
**Beat** menu (**Internal beat on**, and the **BPM**; the button then reads
"Beat 120"), or **Tap** it: the tempo comes from
your last four taps (pause two seconds to start over), and the beat lands
on your last tap — every beat effect jumps onto it. A single tap just moves
the beat. An external beat (MIDI clock, audio) pulls the clock onto its
beat gently rather than jolting it.

### Spread

Effects that chase across their heads share one **Spread** row in the look
editor, so they all spread the same way: the dimmer, colour and pan/tilt
phasers, the pulses, native strobe, rainbow, warm/cool, gobo chase — and
every movement effect: orbit, diamond, figure 8, Lissajous, ballyhoo,
sweep, wave, wander, fan, drawn path, spotlight chase, and Stage Shape's
outline:

- **Amount** — how far the effect's cycle runs across the heads: **1** is
  one full wave across them, **0.5** half a wave, **2** two waves. At
  **its own** (0) the effect uses its own spread setting, as before.
- **Axis** — what puts the heads in order: **in look order**, the head
  layout's **columns** or **rows**, or where they stand: **stage left to
  right**, **front to back**, **by height** — or round the look's **Aim
  target**: **out from the target** (nearest first, a ripple outward) or
  **around the target** (from straight downstage of it, round towards
  stage right, a sweep). Heads at the same place share a phase (a column, a bar seen
  from the side). With no Aim look, the target axes use centre stage.
- **Direction** — **forward**, **reverse**, **centre out**, **ends in**.
- **Wings** — split the heads into mirrored parts, each running the
  spread (2 wings: out from the middle seam both ways).
- **Blocks of** — neighbouring heads that share a phase.

With the spread on, the effect's own spread (and a phaser's reverse) is
hidden — the Spread row stands in for them — and a **preview** under it
shows the look's heads where they stand, coloured by phase (the same colour
is the same moment in the cycle; a cross marks the target).

On **Stage Shape**: a closed shape gives each head its phase as its place
round the outline (amount 1 = evenly round it once); a **Line** runs the
heads end to end at amount 1. Any direction works: *reverse* walks them the
other way round, *centre out* / *ends in* and **wings** pair heads up on
the same point, which is a look in itself.

### Speed masters

Tempo masters for effects to run on, so one tap or one fader moves
everything on a master together. Open them from the Programming toolbar's
**Beat** menu → **Speed masters…**. **Masters:** at the top sets how many
the show uses — 4 to 16 (fixed slots, so a control surface can map them).
Each row has:

- **Follows** — **Its own tempo** (a BPM — typed numbers apply as you
  type — **Tap**, a **Rate** fader from 0 to 200 %), or the **Main
  clock**, the **Show timeline** or another master at a **Ratio** (×¼ to
  ×4): ×2 plays two beats to the other's one, on the other's beats; ×½ one
  to its two. A master can't follow one that follows it.
- **Show timeline** follows the running show's beat grid (its BPM) at its
  playhead — so when the show follows MIDI timecode, everything on the
  master locks to the song's bars and beats. With no show running it
  follows the main clock.
- **Pause** — the master stops; everything on it freezes where it is.
  A following master picks up on the beat again when un-paused.
- A light that flashes on the master's beat.
- **Inputs…** — drive the master from a MIDI controller (or any input):
  **Tap** and **Pause** on buttons (Pause: press to stop, again to go on),
  **Rate** on a fader — its middle is the tempo as set, the top double,
  the bottom stopped. Move the control with **Auto Detect** on, or choose
  it; the button then reads how many are mapped. Saved with the show.

The masters also sit in the **footer**, beside the Grand Master: a button
for each, reading its tempo ("Pars 120", or "paused") and lit on its beat;
click one to open this window at that master. **View → Show Speed Masters
in Footer** turns them off. With more than eight in use the names are
shortened (an unnamed one shows its number, "S9"); the tooltip has it all.

**What runs on a master** — choose **Speed** in any of these; the
innermost choice wins:

| Where | Setting | Default |
|---|---|---|
| The effect | Look editor → **Speed** | As the look |
| The look | The look's header → **Speed** | As played |
| A chase step | The chaser's **Speed** column | As the chase |
| A chase | The chaser editor's **Speed** | As played |
| A collection | The collection editor's **Speed** | As played |

"As played" means whatever started it says: a chase hands its master (or
the step's own) to the look it plays, a collection to its members — so the
same look can run on Speed 1 in one chase and Speed 3 in another. With
nothing set anywhere an effect keeps its own speed, as before.

On the main clock or a master, an effect's **rate** (or **speed**) counts
**cycles per beat** — 1 is once a beat, 0.25 once a bar — and the look
editor labels it "(per beat)". It stays on the beat, follows the master's
tap and rate, and stops when it's paused.

A **chase that counts in beats** (its tempo set to Beats) on a master
steps on that master's beats — tap the master and the chase follows; pause
it and the chase holds on its step. A chase counting in time keeps its
times; only the effects in its looks follow the master.

## Diagnostics

| Effect | What it does |
|---|---|
| **Grid Probe** | Colours each pixel by its grid column (red) and row (blue) — shows how a group's layout maps onto the rig |
| **Step Test Pattern** | For LED-strip steps: each strip counted, each step numbered and named, a spinner that stops if output freezes — see [Step Test Pattern](programming-tab.md#step-test-pattern) |

## WLED

| Effect | What it does |
|---|---|
| **WLED device effect** | Any of the device's own effects, run on WLED fixtures themselves |
| **WLED Android, Aurora, Blink, Blink Rainbow, Bouncing Balls, Bpm, Breathe, Candle, Candle Multi, Chase, Chase Flash, Chase Rainbow, Chunchun, Colorful, Colorloop, Colortwinkles, Colorwaves, Dancing Shadows, Dissolve, Drip, Dynamic, Dynamic Smooth, Fade, Fairy, Fill Noise, Fire 2012, Fire Flicker, Fireworks 1D, Flow, Glitter, Gradient, Halloween Eyes, Heartbeat, Hyper Sparkle, Juggle, Lake, Lightning, Loading, Meteor, Multi Comet, Noise Pal, Oscillate, Pacifica, Palette, Phased, Plasma, Police, Popcorn, Pride 2015, Railway, Rain, Rainbow, Rainbow Runner, Random Colors, Ripple, Ripple Rainbow, Rolling Balls, Running, Running Dual, Saw, Scan, Scanner, Sine, Sinelon, Sinelon Dual, Sinelon Rainbow, Solid, Solid Glitter, Sparkle, Sparkle Dark, Spots, Spots Fade, Stream, Strobe, Strobe Rainbow, Sunrise, Sweep, Tetrix, Theater, Theater Rainbow, Traffic Light, Tri Fade, Tri Wipe, TV Simulator, Twinkle, Twinklecat, Twinklefox, Two Areas, Two Dots, Washing Machine, Wavesins, Wipe** | WLED's effects with a qlcconsole version that runs on the look's other fixtures too, at WLED's speed, intensity, palette and colours |

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
