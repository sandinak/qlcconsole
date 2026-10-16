# What's new

The latest changes, newest first. Each links to the guide with the detail.

## October 2026

### Sub-cues

- **Cues inside a cue.** A chase used as a step of a cue list lists its
  steps under it as **4.1, 4.2…**. Set the step's **Sub-cues** to **Sub
  GO** and they wait for **Sub GO** (⌥⌘↩; Sub Back ⌥⌘⇧↩) while GO still
  goes on to cue 5. The chaser editor and the Virtual Console cue list show
  them nested, the cue list highlights the one playing and has Sub GO /
  Sub Back buttons, and the footer says **Sub: 4.2 · Next: 5**. Older
  shows play as before. See [Sub-cues](show-control.md#sub-cues).

### Outputs

- **Art-Net and E1.31 say when an output is in trouble** — in the footer
  and in red on the universe's row in Connections: an address that can't
  be reached, an Art-Net node that stopped answering, a node port reporting
  a DMX short or no longer outputting. Judged with a delay each way, so they
  don't flicker, and they clear by themselves. See
  [When an output is in trouble](other-improvements.md#when-an-output-is-in-trouble).
- **The log no longer flaps** "is failing" / "recovered" for a node that
  isn't there: one line when it starts, one when it has really recovered.
- **A node port's DMX short shows as a short.** Connections had the
  "short" and "merging" flags of a node's ports the wrong way round.

### Shows that travel

- **A .qlcc carries its own definitions and scripts.** Saving embeds every
  fixture definition, effect script and RGB script the show uses that isn't
  part of the app, so it opens complete on another computer — no more red
  stand-ins for your own fixtures. A .qxw is saved as before.
- **Differences warn; the show keeps its own.** Where this computer's copy
  differs, the show's is used and the footer says so; **Review…** shows
  which looks newer and whether switching is safe for the modes and
  settings the show uses, with **Use This Computer's**, **Keep the Show's**
  and **Install the Show's**. Ones only the show has are a footer note with
  **Install on This Computer**. Save never swaps versions behind your back.
- **File → Collect for Transfer…** writes the show and those files into one
  folder; a show's folder's Fixtures/, EffectScripts/ and RGBScripts/ are
  searched when it opens.
- On **Windows and Linux**, your own effect scripts, LED maps, presets,
  bundles, gobos and network gear are now found: they live in the same user
  folder as your fixture definitions (they were looked for at the Mac's
  path).
- See [Opening a show on another computer](fixture-manager.md#opening-a-show-on-another-computer).

### Show control

- **QLab can fire the lights' cues.** A chase's steps have **cue numbers**
  (a Cue column in the chaser editor; a step without one answers to its
  position), a chase can be the **main cue list** or have a list number,
  and **MIDI Show Control** (on a patched MIDI input) or **OSC** (on a port
  of its own) can GO, LOAD, STOP, PAUSE, RESUME, RESET or ALL OFF them.
  See [Show control (QLab)](show-control.md).
- **Connections → Show control &amp; sync**: OSC in, MIDI Show Control in
  and MIDI timecode in one place, each live — listening or why not, the
  last command and when, the timecode's source and sync health. It replaces
  the Settings dialog's Show control group (which now points there); the
  settings carry over. MSC can be kept to **one MIDI port** (by name, for
  every show) and the **timecode source is saved with the show**.
- **Next:** in the footer says what the main cue list's GO plays next; GO
  (⌘↩) and Back (⌘⇧↩) work from any window, and the Virtual Console cue
  list shows each cue's number and what's next.
- **Skip a step** without deleting it: untick it in the chaser editor (or
  right-click → Skip); GO, Next, Back and auto-follow pass over it.

### Help

- **In-app help.** **F1** opens the guide for whatever is in front — the
  tab, the Rig Overview, Fixture Properties, the Patch dialog, the look
  editor, the power dialogs. **Help** lists the guides: Getting Started,
  What's New, Keyboard and Mouse, Effects, Troubleshooting, and all of
  them; search finds a word in every guide. The stock QLC+ reference is
  still there, online, as **QLC+ Reference** (⇧F1).
- New guides: [Shows (the timeline)](shows.md) — what tracks, items,
  cues and the show end are; [Getting started](getting-started.md) (starting the app and
  the ways a show comes together), [Keyboard and mouse](shortcuts.md),
  [Effects](effects.md) and [Troubleshooting](troubleshooting.md).
- **Pictures with numbered marks** on the parts each guide talks about:
  the main window and the footer ([Getting started](getting-started.md#finding-your-way-around)),
  the [Programming tab](programming-tab.md#layout), the effect look editor
  and the [effect browser](programming-tab.md#the-effect-browser), the
  [stack checker](effects.md#the-stack-checker),
  [Connections by universe](other-improvements.md#connections--patch) and the
  [Patch dialog](fixture-manager.md#patching-the-patch-dialog).

### Lighting Studio

- **Washes look like washes.** A pool now fades out past its beam angle
  for a wash (pars, pixel bars, wash movers) and stays crisp for a profile
  (a gobo, focus or iris channel, or a beam bar); PC and Fresnel lenses
  are softest. See
  [Washes and profiles](lighting-studio.md#washes-and-profiles-the-edge-of-a-pool).
- **A lit rig draws about fifteen times faster**: the decks are worked
  out once a frame, where each ray lands is kept while the rig stays put,
  and a pool split across a deck's edge is drawn only as finely as it
  shows (a 208-fixture show fully lit: from about a second a frame to 70
  ms).
- **The Rig Overview draws on the graphics card** (Metal on a Mac,
  OpenGL elsewhere), and works out its fixtures on every core at once:
  the same picture, and the lit 208-fixture show from about 70 ms a frame
  to 15 (from a stutter to smooth). The **GPU** box in its toolbar turns it off
  if a driver draws it wrongly. See
  [Angled — the free 3D view](lighting-studio.md#angled--the-free-3d-view).
- **Show or hide layers from the Rig Overview** — its **Layers** menu, the
  same switch as the Studio's eye. The eye and the labels toggle now work in
  Operate mode as well.
- **Pools show on decks and steps.** A beam landing on a platform was drawn
  only where it reached the bare floor; the part on the deck was lost.

- **The Rig Overview stands on centre stage** — on the floor, in the
  middle of the stage grid — and turns about it, instead of centring
  whatever the rig's bounding box was (which floated the floor half a
  truss-height below the middle).
- **The Lighting Studio and the Rig Overview show a running position effect on an Aim look.** A head
  under a look with an Aim target was always drawn pointing at the target —
  so Stage Shape, or an orbit round a target, never showed. Selected, a
  look still shows where its target is; running, the heads are drawn where
  they're actually sent.
- **A drag only moves; Shift-drop attaches.** Dropping a fixture over a
  truss, pipe or deck no longer hangs it there unless you hold **Shift** as
  you let go; the outline turns green while it would, and updates the
  moment Shift is pressed or released. Coming off still needs no Shift, and
  fixtures dragged in from the Inventory are placed where they land.
  ([Mounting](lighting-studio.md#mounting-a-fixture))
- **Height counts when attaching.** A fixture mounted on something (the
  foot of a tower, a deck) is only offered a truss or pipe within half a
  metre of its own height — no more lights at a tower's base hung on the
  bar on top.
- **Mount on → Deck.** Stands every selected fixture on the deck under it
  (or a named one): on its top, upright, nothing floating, nudged inside a
  deck it sat on the edge of.
- **Fixtures under structures** draw faded with a dashed outline in the
  Top view, still clickable.
- **Rulers mark the middle** of whatever you drag — a drape's centre line,
  not where you grabbed it — and band any selection, not only fixtures.
- **Orientation for anything that pans or tilts**, even bar-shaped (a
  two-arm beam bar standing on a step); **Test orientation** names its
  centred pose for it — straight up, down or out.
- **Lanterns on hangers hang straight.** A face left over from an earlier
  mount drew some on their side; it's cleared when the show loads.
- **Rig Overview** (⇧⌘R) **follows the Studio's layers** — hide a layer,
  it leaves the overview — and the same shortcut, or ⌘W, closes it.
- **Faster drawing of the rig**: a 200-fixture show draws in about 9 ms a frame (it
  was 51).

- **A truss between two stands** — the truss editor's **Other end on
  stand**: it runs from one stand's top to the other's and follows them.
- **Hangers on the 2D plot** — arm and hook, double-click to edit.
- **A patch can be undone** — the Patch dialog's Apply is one ⌘Z, on the
  Fixtures tab or in the Studio.
- A **house electric** (horizontal pipe) lists its fixtures by distance
  **along** it, up to its length, rather than a boom's height.
- The **Network Topology** dialog works like every rig dialog: the plot
  follows as you edit, Cancel puts the network back, OK is one undo.
- The Virtual Console's **button matrix** and **animation** keys moved to
  ⇧⌘G and ⇧⌘N, off the app's Lighting Studio and Rig Overview keys.

### Patching

- **Patching fits fixtures into what's free, in one order.** The Patch
  dialog's Place now defaults to **into the free space**: the fixtures are
  addressed one after another around whatever else is in the universe —
  no more conflicts from moving a patched fixture's block onto its
  neighbours — in one **addressing order** you choose: by name, movers
  first then by kind, or as they hang. The same group always lands the
  same way. Any
  with no room are flagged and left where they were, with **Carry on into
  the next universe** or **Fit by changing modes** to finish the patch.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))
- **Change a mode in the Patch dialog**, patched fixture or not; Apply
  changes it, and ⌘Z puts it back. A different head count is allowed
  unless a fixture group lays out heads the new mode doesn't have.

- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, to save as **CSV** or a printable **PDF** for the crew.
  **Fixtures → Patch List…**, or the Studio's **More** menu.
  ([The patch list](fixture-manager.md#the-patch-list))
- **A label per fixture** from the patch list: **Labels…** saves a PDF —
  the name and the address (U3 · 101) large, the universe's name, the mode
  and channels, a network fixture's name and where it hangs small — on Avery 5160 sheets (30 to a Letter page) or a 62 mm
  roll, for all the fixtures or the selected rows.
  ([Labels](fixture-manager.md#labels))
- **Patch Others Like This.** Right-click a structure: the other
  structures of its kind with the same fixtures are patched as it is — each
  fixture at the address of its counterpart, in its own structure's
  universe. The Patch dialog offers the same **Match** whenever its fixtures
  span several structures.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))

### Programming

- **160 WLED effects run on any fixture** — every 1D effect of WLED's: Android, Aurora, Blends, Blink, Blink Rainbow, Bouncing Balls, Bpm, Breathe, Candle, Candle Multi, Chase, Chase 2, Chase 3, Chase Flash, Chase Flash Rnd, Chase Rainbow, Chase Random, Chunchun, Color Clouds, Colorful, Colorloop, Colortwinkles, Colorwaves, Dancing Shadows, Dissolve, Dissolve Rnd, Drip, Dynamic, Dynamic Smooth, Fade, Fairy, Fairytwinkle, Fill Noise, Fire 2012, Fire Flicker, Fireworks, Fireworks 1D, Fireworks Starburst, Flow, Flow Stripe, Glitter, Gradient, Halloween Eyes, Heartbeat, Hyper Sparkle, ICU, Juggle, Lake, Lighthouse, Lightning, Loading, Meteor, Meteor Smooth, Multi Comet, Noise 1, Noise 2, Noise 3, Noise 4, Noise Pal, Oscillate, Pacifica, PacMan, Palette, Percent, Perlin Move, Phased, Phased Noise, Plasma, Police, Popcorn, Pride 2015, PS 1D Balance, PS Chase, PS Dancing Shadows, PS DripDrop, PS Fire 1D, PS Fireworks 1D, PS Hourglass, PS Pinball, PS Sparkler, PS Spray 1D, PS Springy, PS Starburst, Railway, Rain, Rainbow, Rainbow Runner, Random Colors, Ripple, Ripple Rainbow, Rolling Balls, Running, Running Dual, Saw, Scan, Scan Dual, Scanner, Scanner Dual, Shimmer, Sine, Sinelon, Sinelon Dual, Sinelon Rainbow, Solid, Solid Glitter, Solid Pattern, Solid Pattern Tri, Sparkle, Sparkle Dark, Sparkle+, Spots, Spots Fade, Stream, Stream 2, Strobe, Strobe Mega, Strobe Rainbow, Sunrise, Sweep, Sweep Random, Tetrix, Theater, Theater Rainbow, Traffic Light, Tri Fade, Tri Wipe, TV Simulator, Twinkle, Twinklecat, Twinklefox, Twinkleup, Two Areas, Two Dots, Washing Machine, Wavesins, Wipe, Wipe Random — and the sound-reactive DJ Light, Freqmap, Freqpixels, Freqwave, Gravcenter, Gravcentric, Gravfreq, Gravimeter, Juggles, Matripix, Midnoise, Noisefire, Noisemeter, Noisemove, Plasmoid, PS GEQ 1D, PS Sonic Boom, PS Sonic Stream, Puddlepeak, Puddles, Ripple Peak, Rocktaves, Waterfall, which follow the audio input (Inputs/Outputs) as the device follows its microphone — so a look with one runs it on its WLED
  devices and its other fixtures alike.
- **Effect previews show what the effect does.** A position effect draws
  the shape its heads trace, with the heads along it, instead of a strip
  of lights; gobo, iris and zoom effects show a row of beams; amber and UV
  pulses light in their own colours. ([The effect browser](programming-tab.md#the-effect-browser))
- **WLED effects only where they can run.** A WLED effect that only runs on
  a WLED device is offered only when the look's targets include one;
  without, the WLED tab lists the ones that run on any fixture.
- **A Programming toolbar that fits.** Highlight, Flash, Tap and Save
  Positions stay buttons; Park, Move in black, Beat (it reads the tempo)
  and More are drop-downs — one row, no scrolling.
- **The effect browser is a window of its own.** Once an effect is picked,
  its settings have the look editor's full width; **Change effect…** opens
  the browser, where every pick (and every arrow-key step) plays at once,
  and **Cancel** or Esc puts back the effect you had.
  ([The effect browser](programming-tab.md#the-effect-browser))

### Playback

- **GO on the beat.** A chase or cue list can hold a GO to the next beat
  or bar — of its speed master, or the main beat — so a cue pressed a
  little early lands on the music. The Next button shows a GO waiting.
  ([GO on the beat](getting-started.md#go-on-the-beat))

### Programming tab

- **The Looks list checks the stack** — a look the one below always covers,
  an effect another covers, a dimmer only some heads get, an effect for
  fixtures the look hasn't… are marked in the list with why, and counted
  under it. ([The stack checker](effects.md#the-stack-checker))

- **The handles move the edges you drag** — no pane snaps shut, and the
  canvas can be narrowed (its fades-and-transitions line wraps; it was 1300
  px at its narrowest, so a handle squeezed the panes beside it instead).
  A new handle sits **between the looks and the Look Editor**, which
  scrolls when made short. Positions are remembered.
  ([Layout](programming-tab.md#layout))

### Effects

- **A look stacks the same way every time** — top to bottom, the lower
  wins; colours are colour 1, 2, 3; an effect's colour replaces the base
  instead of mixing with it; dimmer effects scale and multiply; a Colour or
  Dimmer under an effect is that effect's only, and the ones above are the
  look's own, for every effect. Fixed along the way: two static looks on a
  channel kept the *higher* value (Blue then Red came out magenta-ish, two
  Pan/Tilts kept the bigger pan); a second Colour look moved a colour
  wheel; an Aim or a Dimmer placed after a movement effect did nothing;
  a Dimmer look didn't dim a fixture with no dimmer; two effects on one
  channel went by which started first, not their order in the list.
  ([How a look stacks](effects.md#how-a-look-stacks))

- **Breathe and the other dimmer effects work on movers with a head
  dimmer** (no master dimmer in their definition), and a colour-wheel
  fixture's wheel holds its colour while they breathe instead of hopping
  slots.
- **Movement effects run round the look's aim.** With an Aim or Pan/Tilt
  look in it, a ballyhoo, orbit, figure 8 and the rest move over that point
  instead of the middle of the heads' range (straight down, on a hung
  mover). ([Position](effects.md#position-moving-heads))

- **How the footer shows a master's beat** — a blink, a bar across each
  beat, or nothing; always, or only while something on that master plays
  (**View → Speed Master Beat in Footer**).
- **Speed masters in the footer** — a button for each master something is
  on (an unused one neither shows nor flashes, there or in its window),
  reading its tempo and lit on its beat, with a green level bar while its
  rate is moved; a click opens the Speed Masters window at it
  (**View → Show Speed Masters in Footer**). Each master's **Inputs…**
  maps its Tap, Rate and Pause to a MIDI controller.
  ([Speed masters](effects.md#speed-masters))
- **Effects layer as on a desk.** A colour effect replaces the look's
  colour on its heads instead of adding to it (Cyan under a rainbow washed
  every hue pale); a dimmer effect dims what's there, the colour effect's
  included. ([Effects](effects.md))
- **Rainbow with a colour in the look sweeps that colour** — a wave of
  Cyan across the heads, no rainbow; with two or more, it sweeps through
  them.
- **A rainbow on a wash with no dimmer is a rainbow.** On an RGBW head with
  no master dimmer (a Cobra Wash in 13-channel mode, say) an effect's own
  dimmer put red, green, blue and white all at full under its colour — a
  rainbow came out white.
- **Spread.** One Spread row for every chasing effect — amount in cycles,
  in look order or across columns, rows or the stage, **out from or round
  the look's target**, forward, reverse, centre out or ends in, with wings
  and blocks — and a preview of the heads coloured by phase. Every
  movement effect takes it too (orbit, figure 8, ballyhoo, wave, sweep,
  wander, fan, spotlight chase…), and **Stage Shape** uses it for which
  head goes where on the line or shape.
  ([Spread](effects.md#spread))
- **Speed masters.** 4 to 16 tempo masters (Programming toolbar →
  **Beat → Speed masters…**), one of which can follow the **show timeline**
  (and so timecode): each its own tempo, tap, rate and pause, or following the
  main clock or another master at ×¼ … ×4. Put an effect, a look, a chase
  step, a chase or a collection on one; on a master an effect's rate counts
  per beat; a chase counting in beats steps on its master's beats.
  ([Speed masters](effects.md#speed-masters))
- **Effects keep the beat together.** Every effect that follows the beat
  reads one beat clock — two started at different moments used to stay
  that far apart for good. **Tap** now averages your last four taps and
  puts the beat on the last one, pulling every beat effect onto it.
  ([On the beat](effects.md#on-the-beat))
- **Text** — words on a pixel grid, scrolling or still in the middle, in
  the look's colours; settings that are words get a text box.
  ([Text](programming-tab.md#text))
- **Step Test Pattern** — a diagnostic card for LED-strip steps: strips
  counted, steps numbered and named, a spinner that stops if output
  freezes. ([Step Test Pattern](programming-tab.md#step-test-pattern))
- The RGB Matrix **Text** algorithm has a **Centered** style.
- **Much faster on big pixel rigs**: 96 strips × 64 pixels ticks in about
  8.5 ms (was 68), leaving the screen free to keep up.
- The **effect list sits beside its settings** in the look editor, so both
  fit on an ordinary screen.

### Views and windows

- **Connections → By universe lists the universes in use** — fixtures
  patched in, an output or an input — and says how many it hides; **Show
  unused universes** shows them all.

- **Connections holds still.** The list refreshes every few seconds to
  catch nodes coming and going; it no longer drops the universe you picked
  or scrolls back to the top when it does, and it waits while a right-click
  menu or dialog is open, so **Patch…** acts on the row you chose.
- **Right-click a tab → Open in Its Own Window.** Works on the first
  click, even when the app isn't in front (a Mac spends that click
  bringing the window forward, so a Shift-click there did nothing).
- **Shift-click a tab** to put it in a window of its own — a double-click
  no longer does it by accident. **Window → Move … to Its Own Window** does
  the same for the view in front, and the Window menu can turn the
  Shift-click off. ([Window menu](other-improvements.md#window-menu))
- **Functions has no tab** — **Window → Functions** (⇧⌘4) opens it in its
  own window. ([Window menu](other-improvements.md#window-menu))
- The **Window** menu lists every view, wherever it is, and puts
  broken-out ones back.
- **Connections**' Devices header is one row; the list gets the height.

### Earlier this month

- **Power**: a fixture's **maximum draw** — amps, proportional, measured
  state by state, set on many at once and copied to the same fixtures —
  holds a hungry fixture under its circuit.
  ([Maximum draw](other-improvements.md#a-fixtures-maximum-draw))
- **Connections**: only a universe's name is edited in place; select a
  universe to see what it's sending, live.
- A look on a fixture **group** sets only that group's own heads.
- A function deleted while running lets go of its channels.
- **Scenery** (cyc, walls, drapes with an opening, a proscenium, a rounded
  front), **fountains** (with a silk-flame fixture and a labelled diagram),
  **lasers and star projectors**, a beam split across the surfaces it
  crosses, group rotation, and count badges on towers.
  ([Lighting Studio](lighting-studio.md))
