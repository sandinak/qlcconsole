# Changelog

All notable user-facing changes to qlcconsole are documented here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This is the fork's own changelog — for the detailed development log (with
per-feature build notes and deferred follow-ups), see [DONE.md](docs/project/DONE.md) and
active work in [TODO.md](docs/project/TODO.md).

Versions here are independent of the QLC+ base version embedded in the
in-app build string (see [RELEASE.md](docs/dev/RELEASE.md) for how the two relate).

## [0.3.1] - Unreleased

A home for QLC+ 4 users: QLC+ 4's habits that qlcconsole had dropped are
back, and a .qxw saved here is checked against QLC+ 4.14.4 itself.

### Based on QLC+ 4.14.4

qlcconsole's QLC+ base moves from 4.14.2 to **4.14.4**: the build string
reads `4.14.4+qlcconsole.<N>` (N keeps counting, so it never goes back).
4.14.3 was already in from February's upstream sync; 4.14.4's fixes are
taken in, each recorded with its upstream commit
([docs/dev/UPSTREAM.md](docs/dev/UPSTREAM.md) logs what was taken, adapted and
skipped). Upstream fixes taken:

- **New and Open forget the last show's startup function** (#1963).
- **A show or collection holding a deleted function** no longer reaches
  for it while playing or totting up its length; a Show's copied
  functions get IDs of their own.
- **Dimmer palettes keep a fractional value** from the file instead of
  reading it as 0.
- **Audio triggers split the spectrum in logarithmic bands** (40 Hz to
  5 kHz), with a level that rises fast and falls smoothly, as in QLC+
  4.14.4; RGB Audio and effect scripts hear the same bands.
- **Nested submasters**: a submaster in a frame inside another frame
  follows the outer one.
- A Virtual Console slider's channel-monitor reset button is sized
  properly and can't be reached before it exists.
- **Web access**: valid HTML, button tooltips, a cue-list row that closed
  its cell instead of the row, upload pickers that offer only .qxf/.d4,
  a rewritten keypad page (Clear, Enter sends), sturdier web-socket
  connections, and the slider's top label escaped like everything else.
- **Stock 4.14.4's network-server settings** in a show are kept when it
  is saved here, instead of dropped.
- **DMX USB**: udev rules for more FTDI chips (Linux). GPIO: libgpiod 2
  and input-level fixes. SPI: frequency label.
- **Translations**: updated Catalan, Czech, Dutch, Finnish, French,
  German, Italian, Japanese, Portuguese (Brazil) and Spanish.
- **Translation catalogues refreshed** from qlcconsole's own sources,
  the first time since the rename: every string the app shows (7,400 more
  in the app's catalogue, the engine's messages among them) is now there
  for translators, and strings that changed keep their old translation as
  a hint. Nothing new is translated yet: those strings show in English.
  `make translations` refreshes them every release.
- **Novation Launchpad MK2**: input profile with its colours and
  channels, and a welcome message template.
- **31 fixture definitions from QLC+ upstream**: 28 new (Acme Deluge,
  Robe Robin T.5 Profile, Robert Juliat Dalis 860, GLP Matrix Eye 2 and
  4, Chauvet FXpar3, Equinox Fusion 140 / 260ZR, Starway SupraKolor and
  more) and 3 updated (American DJ Jolt 300, Cameo ROOT PAR 6, beamZ
  LCB244).
- **48 more fixture definitions from QLC+ upstream** (synced to upstream
  master of 2026-10-29): 44 new (American DJ Focus Spot Two, Briteq
  BT-Theatre 150EZ / 200TW / 261FC, Chauvet Ovation CYC 1 FC and Kinta
  HP, ETC ColorSource Fresnel V-MAX, Martin MAC Aura XIP, Varytec LED
  Theatre Spot 120 FC and more) and 4 updated (ETC ColorSource Fresnel V,
  Pro-Lights CromoWash100, Rockville Motionstrip RGBW, Stairville
  CLB2.4). The fixture library is now synced from upstream every release
  (`tools/sync-fixtures.py`), keeping qlcconsole's own definitions and
  edits. 1,791 definitions.

### Added

- **One cue list per show, with song sections** (Q190). A production's
  **Numbering** says how its cues are numbered — Show.Part.Iteration
  (0.0.1 setup, 1.0.1 the first song), hundreds per song (Eos, MA), free
  numbers with sections by hand (QLab), or none — and the Show Tree and the
  Run tab show each song as a header row its cues fold under (*1 —
  Everybody Scream*), named on its page, with **Block at section start**.
  **+ Cue** and **+ Section** number to the scheme; a new show is offered
  the scheme its numbers already follow. **Merge lists into one…** turns a
  show built as a list per song (GO and Run played only the first) into
  one list: cues in order, numbers kept unless two clash (and said), each
  list's start Block so every cue plays as it did. Old shows load
  unchanged.
- **Where an effect came from, always visible**: the effect browser
  filters **Built-in · Mine · This show**; an effect you saved shows a
  small **mine** mark, the show's effect palettes the palette mark; one
  made from another has a grey **based on Fire** line, and hovering it
  lists the settings that differ. A one-off on a Show Tree layer says
  *one-off*, beside **Make reusable**. Picking one of the show's effects
  in the browser copies it into the look.
- **Build's search spans every section**: **Search palettes, effects and
  scenes…** over the navigator lists matches from Palettes, Effects and
  Scenes together, under headers with counts; a click goes to it in its
  section, and clearing the search goes back to where you were.
- **Any effect runs once, or N times**: **Runs: Loop | Once | N times**,
  **At the end: Hold | Release** and a **Length** (auto: the cue or chase
  step's time) on every effect, in the look editor, the effect designer,
  a Show Tree effect row's ⋯ and Build ▸ Effects' right-click (an EFX or
  RGB Matrix: Once is its Single shot run order). Before, only effects
  written as one-shots could stop.
- **Stack a movement and an intensity effect**, as the help now shows: a
  layer's Effect row (⋯ ▸ Set to, **Design a new effect…**) and the
  Workbench's **+ Effect ▾ ▸ Effect look…** add an effect beside the
  ones there; the designer's Keep on this layer no longer replaces an
  effect of another kind.
- **Drag a cue layer's look row onto another layer** in the docked
  Workbench to move it (⌥ copies), one undo step; press and drag moved
  nothing before, it selected rows.
- **Own speed has a setting.** In the look editor, an effect on **Own
  speed** shows **Own speed ×1.00** beside the Speed list: ×2 twice as
  fast, ×0.5 half. Every effect has it, also those with no speed of their
  own (Confetti, Matrix Sparkle); a change never jumps the effect.
- **Blind in Operate is the Blind view**: the plot, the studio and every
  fixture picture show the cue you are editing in the Show Tree while the
  stage plays the run; nothing on the wire changes. The footer turns blue
  and names the cue (docs/design/LIVE_BLIND.md).
- **Bring over QLC+ 4's settings, once** (asked): the first start that
  finds QLC+ 4's settings on the computer lists them and asks once
  whether to copy its recent shows (after qlcconsole's own), I/O and
  plugin preferences (default patch, hotplug, MIDI, Art-Net, sACN, OSC,
  DMX USB, uDMX) and the interface preferences that mean the same here
  (Virtual Console widget sizes, Simple Desk pages, slider style...).
  Never QLC+ 4's window layouts, never a setting qlcconsole already has,
  and QLC+ 4 itself is not changed. **Settings ▸ General ▸ QLC+ 4 ▸ Bring
  over…** does it later. Fixtures, profiles and scripts still come from
  QLC+'s user folder by themselves. Help: Coming from QLC+ 4 ▸ Your QLC+
  4 settings.
- **Fixed lights and lasers point where they face** (board Q172, part A):
  a fixed light (no pan or tilt: a par, a laser) has the **Face** row,
  which aims it (Back upstage and level, Bottom straight down; a yoke tilt
  already set is kept). Dropped on a truss's upstage or downstage chord on
  the plot, a fixed light with no face yet faces that side, so a laser on
  the back of a truss throws at the cyc. One rule (`RigHeads::fixedThrow`)
  now gives the 3D view, the plot's footprint, Quick focus and the
  orientation diagram the same throw; Quick focus used "straight down".
  In **Aim & Mount**, **Points (° from DS)** and **Tilt in yoke** ("45°
  down", "level", "10° up"; the up arrow tips it up; type "10 up").
- **A laser looks like a laser** (board Q172, part B): in the Rig
  Overview a laser is a box hung in a U yoke (clamp, side plates, cross
  bar), its dark window and round apertures (one per beam) toward where
  it throws, lit in its live colour while the housing stays dark; Tilt in
  yoke tips the box between the plates and the yoke turns to its bearing.
  Its beams leave the window. A definition with no size gets a sky
  laser's 200 × 110 × 190 mm, not a 0.30 m stick. On the plot, the new
  **Laser / star projector** symbol (its own **Effects** section) is a
  Laser's Auto symbol: "Auto (Laser)" named only the type before.
- **PlayStation pads by their own names** (board Q170): with a DualShock 4
  or DualSense, Quick focus, Venue focus and Focus this cue call every
  control what the pad calls it -- on the controller map (Cross, Circle,
  Square and Triangle in their own symbols and colours, L1, R1, Share on
  a DualShock 4, Create on a DualSense, Options, the PS button), in the
  panels ("Cross or Return to save, Circle to revert"), in the warnings
  and in the voice. The map draws the touchpad and the light bar in the
  status lamp's colour; the Controller test window shows "buttonA · Cross".
  The pad is told by its make and model, else by its profile. An Xbox pad
  reads as before. Help: Venue focus ▸ On a PlayStation pad.
- **The PlayStation touchpad as a trackpad** (on a Mac, board Q170): one
  finger dragged nudges the light by how far it moves (the whole pad is
  about 4° of pan; on the target page it moves the target); two fingers
  up or down zoom, sideways focus, one of the two a drag, said once in the
  crew's word; a click says the light, as R3. A landing, lift or click
  never jumps the light, a resting finger doesn't creep, and nothing moves
  while every light is held.
- **The voice waits until you stop moving** (board Q171): in Quick focus,
  Venue focus and Focus this cue, a "where" line is held until no newer
  step has come for **Waits** (Off / 250 / 400 / 700 ms, 400 by default,
  beside Voice), so stepping through eight lights says only the one you
  stopped on, instead of a stale "Fixture 2..." played to its end first.
  A newer location line then cuts a stale one being said, and a warning
  cuts one at once ("Controller lost" never waits behind "Fixture 3 of
  8"). Saves, warnings and Say the light are never held or cut; a tool's
  first line and entering a focus call are said at once. Off is the voice
  as before.
- **Control Map: MIDI and OSC triggers, saved with the show.** Right-click
  a scene or effect in Build, or a cue list, cue or hit in the Show Tree →
  **Learn trigger…**, then press a key, pad or button (a MIDI note or CC,
  or an OSC message, on any input universe): it fires that thing with no
  Virtual Console widget. A cue list GOes, a cue is jumped to, a hit or
  function plays as the binding's mode says: **Toggle**, **Momentary** (on
  while held), **Start only** or **Stop only**. The control's light
  follows what it fires where the universe has a feedback line, as a VC
  button's does. **Connections ▸ Devices ▸ Show control & sync ▸ Control
  Map** lists every trigger, changes its mode and removes it. Saved as
  `<ControlMap>` only when there is one; QLC+ 4 skips it (the save check
  names it). Replaces the unsaved toggle-only prototype that went with the
  old Programming tab. With no trigger, input costs nothing more.
- **VC buttons fire Show Tree hits**: a button's function picker
  has a **Hits** section (each production's hits, by folder), whatever
  the type filter. The button keeps the hit itself, not just the
  collection that plays it: renamed, rebuilt, deleted and undone, it still
  fires it. Toggle and Flash as before; **Make VC page** buttons do the
  same. Older shows' buttons fire what they fired.
- **VC effect controls**: a VC slider or knob in the new **Effect** mode
  moves one setting of an effect while it plays — **Speed**, **Size /
  Width**, **Rate**, **Offset / Spread**, **Direction**, **Intensity** or
  **Colour 1 hue**, found under each effect's own names (n/a where it has
  none) — on one effect palette, the effect playing on a group, or **the
  current effect** (started or selected last), so one fader serves every
  song. EFX and RGB Matrix functions count too. The value is a live
  override, gone when the effect stops; **Store** keeps it in the
  palette. External input feeds it; saved with the show
  ([design](docs/design/VC_EFFECT_CONTROLS.md)).
- **VC sliders and Speed Dials move speed masters**: a slider (or knob) in
  the new **Speed master** mode drives a master's **Rate** (middle = as
  set) or its **Tempo** between two BPMs, shows "100 %" / "128 BPM" (n/a
  on a following master) and follows the master moved elsewhere; a Speed
  Dial set to a master makes its time one beat and taps it. External
  input feeds both. A slider's properties no longer turn a Grand Master
  slider into a Level one.
- **People on the stage**: a **performer line** is a line target with a
  count of people spread evenly along it, each standing on the highest
  stage surface under them (a step's tread, a riser, the apron), with a
  mean height give or take a spread (each keeps theirs across reloads;
  Shuffle picks again) and a costume colour the rig view lights the body
  in. **Add ▸ Add People…** makes one; with risers selected it reads
  **Fill Selected Risers with People…** and makes a line along every
  tread (by spacing or so many to a row, staggered). Any line takes people
  in its dialog. Lines go in a **Performers** layer (its eye hides them,
  its lock holds them), a fill's lines are one set, and they ride their
  riser when it moves or turns. The rig view draws a small figure with
  fewer facets, so a crowd stays quick.
- **Web access saved with the show**: **Connections ▸ Devices ▸ Show
  control & sync ▸ Web access** turns on "Start web access with this show,
  on port" (9999 by default), with **Reachable from other devices on the
  network** and **Users log in** (as **-wa**). Opening the show starts the
  server; opening one without it stops it, and another port restarts it.
  The box shows the address to open and whether it runs; a port another
  program holds is said there and in the footer, and the show opens anyway.
  **-w** / **-wa** on the command line still win for the whole session. A
  show without the setting saves and loads as before.
- **Help ▸ Troubleshooting ▸ Report a Bug…**: a short form (title, what
  happened, what you expected, steps) that opens the project's GitHub bug
  form in the browser, filled in. **Details that will be included** shows,
  editable, each part ticked or not: version and build, OS and processor,
  Qt, the view, the show's size in counts, the I/O plugin names; the
  layout report and recent log lines start unticked. The show's name and
  path, fixture and group names, the user name, the home folder (as `~`),
  IP addresses and passwords are never included. A report too long for a
  link is cut, with the whole on the clipboard; **Copy Report** works
  without a GitHub account. The app sends nothing itself. The repo's bug
  template is now an issue form (`.github/ISSUE_TEMPLATE/bug_report.yml`).
- **A "Coming from QLC+ 4" help page** (docs/features/coming-from-qlc4.md,
  in the help and linked from the README and the docs index): what stays
  the same, what a 4.x user gains, installing beside QLC+ 4 and what
  comes across from its user folder, opening a .qxw and going back, a
  table of QLC+ 4 workflows and where each lives now, and what's
  different or not here.
- **Save As offers QLC+ 4's .qxw** ("QLC+ 4 Workspaces"), starting on it
  for a show opened from a .qxw; the extension follows the type chosen. A
  .qxw save works as before.
- **Full Screen**, as QLC+ 4 had it: **View ▸ Full Screen** and Classic's
  toolbar, Ctrl+F11 (⌃⌘F on macOS, ⌘F11 too), and the **-f** / **-f
  resize** command-line flag. They went with the slim toolbar in July.
- **New… and Delete input profiles** beside Edit in a universe's patch
  editor, as QLC+ 4's Add and Remove: a new profile is saved in your own
  profile folder and used by the universe; Delete asks, then removes the
  file.
- **Settings ▸ General ▸ Start new shows in the Classic view** (off by
  default; this computer's). An opened show keeps its own view.
- **The DMX Monitor**, QLC+ 4's Monitor: each patched fixture's channels
  and values, live, a universe at a time, in a window of its own from
  **View ▸ DMX Monitor** and Classic's toolbar.
- **Saving a .qxw says what QLC+ 4 will leave out**: the Show Tree, the
  Lighting Studio's rigging, speed masters, a venue's house fixtures and
  the like, named once a file in the line above the tabs after the save
  (never in its way), with **Save as .qlcc…**.
- **A gate test that stock QLC+ 4.14.4 opens a .qxw saved here** with its
  fixtures, groups, channel groups, palettes and functions whole: QLC+
  4.14.4's own engine, built from a pinned copy of its source, reads a
  show the app saved (ui/test/stockroundtrip).
- **Always start with the plot locked**, in the menu on the Lighting
  Studio's lock button (off by default; this computer's): every show you
  open starts with its whole plot locked, without being marked changed.
- **Plot layers shows what holds each thing still.** While the plot is
  locked, every layer's lock is drawn held from above (the padlock faded,
  an up-arrow badge, a dashed outline, "Locked by the plot") with the
  layer's own lock still readable under it, and unlocking the plot puts
  back exactly what each layer had. An object held by its layer or folder
  shows ↑🔒, one locked on its own 🔒; its tooltip names every lock
  holding it, and the plot's right-click menu says when the plot does.
- **A Back view in every structure editor** (truss, bar, pipe and boom,
  electric, stand, tower, platform, group) and in the Rig Overview: the
  Front elevation seen from upstage, looking downstage, with stage left on
  the left. **View** lists Top, Front, Back, Side, 45°; in an editor
  ⌘1–⌘5 (Ctrl elsewhere) switch between them. In Back a fixture on the
  upstage face of a truss is drawn on top and is the one a click picks.
  Dragging, **Put on Face** (it sets the Back face), **Distribute**, the
  rulers ("from behind: SL ◀ ▶ SR") and the H / V / Z readout work as in
  Front, and text reads normally, not mirrored.
- **A [Performance](docs/performance.md) page**: how much one computer can
  drive, measured on a Raspberry Pi 5, a Linux i7 and an Apple M4 (about
  80, 256 and 768 universes of a load heavier than a real show; Art-Net at
  the full 50 fps to 32, 64 and 128 universes), with long runs, memory,
  input load, the method and the data. **Getting started ▸ Choosing a
  computer** in the help sums it up. The charts are made by
  `tools/perf/plot.py` from `docs/perf-data/`; the measuring tools are in
  `stresstest/`.
- **The performance page has the finished long runs**: the M4's 12-hour
  run of the scaled show (5 late ticks an hour, no memory growth) and the
  Pi 5's full five-hour runs, which show that the scaled show is more
  than a Pi 5 can run (a third of its ticks late).
- Developer: **a performance guard, `make perf`** (`tools/perf/guard.py`).
  In about 5 minutes it runs a Release engine under the stress harness at
  fixed loads, plus the hot paths' CPU cost per operation, and compares
  the numbers with this machine's own committed baseline
  (`docs/perf-data/baseline-<host class>.csv`, tolerances from measured
  noise); a slowdown past tolerance exits nonzero. `make perf-record`
  rebases a baseline on purpose. `make beta` / `make release` need a
  passing guard on the release commit. `tools/perf/nightly.sh` runs it on
  origin/main each night with a history (not installed anywhere yet; see
  [Keeping it fast](docs/performance.md#keeping-it-fast)), and
  `tools/perf/plot.py` draws its trend. New gate tests
  (`engine/test/hotpath`) count the work on the hot paths: a static look
  sends one frame and then none, a running scene is one fader of its own
  channels, start/stop and chaser steps leave no faders behind, a matrix
  computes one map per step, not per frame.
- Developer: **`make perf` measures a real rig, the Show Tree, power,
  drawing and web clients**, not only Generic RGB fixtures. A mixed rig
  (`qlcstress --rig mixed`: moving heads with 16-bit pan/tilt, gobos,
  prism and zoom, measured LED pars under a power cap, pixel bars and
  strips, dimmers, laid out across a stage with groups over it) runs as
  three new points (the mixed load, pan/tilt EFX, matrices over stage
  groups); new CPU-per-operation points time the rig's frame, the power
  estimate, a 200-cue tracking Show Tree (GO under load, an edit), a frame
  of the 3D rig view and the 2D plot (`ui/test/renderperf`) and one change
  sent to 50 web clients. New gate tests: an idle rig view and an idle 2D
  plot draw nothing on a 240-fixture rig; pan/tilt EFX moves the fine
  bytes; the power estimate of 960 fixtures adds up; the cap holds every
  par; a GO builds nothing; 50 web clients get every change.
  `make perf-record PERF_ARGS=--points=...` adds points to a baseline
  without rebasing the rest ([The mixed
  rig](docs/performance.md#the-mixed-rig)).
- **Course: a student page per lesson**, a short link to give the class
  (`https://sandinak.github.io/qlcconsole/class/1-1/`, and 0.1–0.8,
  1.1–1.5, the Level 1 check, 3.3, 3.6, 3.7, 3.10, plus a student install
  page). Each is everything a student needs for that lesson and nothing
  else: the show file to download, the steps with the lesson's pictures,
  the words, the exit ticket without answers and the at-home task, at
  about a third of the handout's length. They have their own plain layout
  (no sidebar, search or links into the teacher material, phone-width,
  light and dark, no JavaScript) and are left out of the course site's
  menu and search. Each teacher guide starts with its link and a QR code;
  `docs/course/tools/student-pages.py` checks the pages.
- **Course: Level 2, a lesson at a time** (`/class/2-1/` to `/class/2-7/`, `/class/2-check/`):
  **2.1 Tracking and Block**: what tracks into a cue (S / T / R / M,
  *Tracked in*, *Next cue receives*), **Release** on the verse's step
  tops, a **Cue-only** flash, the Solo as the **Block** hard start, the
  base cue. **2.2 Palettes**: *Used by*, a song's own palette (Duplicate,
  named for its job in the Look Editor, a folder per song), a note taken
  with one change. **2.3 Parts of a group**: the bridge as a call and
  response: a group set, child layers (right-click and ⇧-drop), Split and
  Merge. **2.4 Effects**: effect palettes of the song's own (the effect
  picker), a sparkle with its own colour, a wave, a speed master. **2.5
  Moving lights and lines**: a fan on a line target, crossed (Spread −5)
  for the button, colour wheels and rings. **2.6 Busking with Hits**:
  three hits, Make VC page, one fired live over the running verse. **2.7
  Sub-cues**: stomps inside the chorus on Sub GO and auto-follow; then
  the Level 2 check, one full song from a cue sheet. Each
  lesson's start and finish shows, pictures and
  every step are made and checked by
  `courseshow_test` (`levelTwoWithTheMouse`, `buildLevelTwoShows`,
  `levelTwoPictures`).
- **Art-Net TimeCode in**: a show follows Art-Net TimeCode (ArtTimeCode)
  as it follows MIDI timecode. Patch an Art-Net input and it is offered
  as a timecode source, in the footer's MTC chip and in Connections ▸
  Show control & sync. All four rates are read (24, 25, 29.97 DF, 30).
- **The patch list shows each fixture's gel**: a Gel column (a swatch and
  its value) in Fixtures ▸ Patch List, its CSV, XLSX and PDF, from the
  Colour set in Fixture Properties. It shows only when some fixture has a
  gel, so the crew knows what to cut for each fixed light.
- **Help answers the QLC+ forum's common questions**: Coming from QLC+ 4 ▸
  *Questions QLC+ users ask*: a fader that overrides and lets go (Monitor
  and reset), ⌘D Dump DMX values, Flash's Override priority and Force LTP,
  and sending MIDI CC out.
- **File ▸ New from Template**: start a show from a ready-made one. The
  first is the course's **Show-choir stage (classroom)** (the rig, groups,
  stage targets, colours and levels, nothing patched to an output). It
  opens untitled, so Save asks for a name.
- **File ▸ Show Settings: open in Operate, and an Operate PIN.** "Open
  this show in Operate" opens the show ready to run (as -p does). A PIN
  of 4 to 8 digits keeps a running show in Operate: going back to Design
  asks for it, and Live Edit is off. It's saved with the show as a salted
  hash. Show Settings is the home Teaching mode's settings will share.
- **Menu keys**: Save As is ⇧⌘S (Ctrl+Shift+S) and Quit ⌘Q / Ctrl+Q on
  every platform, Windows included; Edit ▸ Undo and Redo show their keys
  (⌘Z / ⇧⌘Z; Ctrl+Z, and Ctrl+Y / Ctrl+Shift+Z on Windows); Settings… is
  called that on every platform, the Mac's application menu included. A
  new Virtual Console slider moves from ⇧⌘S to ⇧⌘U.

### Changed

- **The save buttons say where an effect goes**: "Save as Effect…" is
  now **Save to My effects…** (on this computer, for any show) and "Save
  as palette…" **Save in this show…**.
- **Show Tree: a cue list's mode is a flag**, not words in the second
  column: **⇢** (Tracking) or **B** (Non-tracking) under **M**, the words
  on hover. The column beside the names starts narrower.
- **Show Tree: skip a cue from its right-click menu.** **Skip cue** /
  **Unskip cue** acts on every selected cue in one undo step; a skipped
  cue stays in the list, grey, struck through and marked. The tick box on
  every cue row is gone (⌘/, the cue page and `skip` still work).
- **Show Tree: Mode and Release on the right-click menus.** A cue's
  **Mode ▸** (Track / Block / Cue-only, the selected cues in one step);
  a layer's **Release (leaving this cue)**, in the tree and the
  Workbench, which also has the cue's **Cue mode ▸**.
- **The rig shows the targets the cue uses**: the plot and the Rig
  Overview draw the targets the cue on show aims at (Live: what plays and
  Build's loaded scene; Blind: the cue in edit); all of them when nothing
  is on show, or with **Show ▸ All targets**. A target just placed stays.
- **Effects take edits while they play.** Changing an effect in the
  designer while a cue or scene plays it changes the rig at once (no
  restart); Cancel puts it back, Update keeps it. A running RGB Matrix
  turns round when its direction is changed.
- **Save as palette asks no name first**: the effect's own name, typed
  over in place in Build ▸ Effects.
- **Renaming in the Show Tree selects the words**, not the cue number
  ("Chorus" in "1.2.0 Chorus").
- **Design: the latest action wins.** Selecting a cue in the Show Tree
  (or clicking the one selected) shows it on the stage, also after GO;
  GO shows the run; a cue clicked in the Run tab in Design shows that cue
  and puts it on standby. Before, selecting after GO stayed off stage
  until Play.
- **Blind is blue, one blue, and only Blind**: the button, the footer
  (with its words back: what Blind is doing), the Show Tree's and the
  docked Workbench's line while editing is blind. The Run tab's DESIGN
  chip takes the theme's colour. **Blind moved to the View menu** (from
  Control).

- **A strip's Tilt tips its light up** on every upright face (Front, Back,
  Stage left, Stage right); on Back or Stage left a positive tilt used to
  point it down. The file keeps the old sense, so a show saved before
  opens with every bar where it was, and an older qlcconsole still reads
  a show saved here.
- **The help's Control Map section says it isn't in the app**: its "learn"
  entry went with the old Programming tab's function list, and the README
  no longer offers it. A MIDI or OSC control fires a function through a
  Virtual Console widget's external input, as in QLC+.
- **Course: Level 0 is lighting concepts, before any app.** Seven short
  lessons with no computers (20–30 minutes; two share a period) teach the
  why before the how: 0.1 why we light a show (the six jobs of light, the
  crew), 0.2 what light does, 0.3 building a look (front, fill, back,
  side, wash, special), 0.4 what the audience reads, 0.5 lighting a song
  (sections, a song map, lighting the staging), 0.6 anticipation and
  momentum (builds, hits, the button, pacing), 0.7 running a show (calls,
  standby / GO, busking). The virtual stage moves from 0.2 to **0.8**
  (`/class/0-8/`). The Level 0 check adds a song map: every change with
  what the audience should feel. Every app lesson (Level 1, Level 3's
  written lessons) now opens with a short *why* linked to the concept it
  applies, and the glossary has the concept words (busking, look, cue,
  build, sting, button, standby, call, contrast, wash, special,
  silhouette and more).
- **The project's direction is written down**:
  [docs/project/STRATEGY.md](docs/project/STRATEGY.md) (what qlcconsole is
  for, the roadmap to 0.5, the guardrails and what it won't build), and
  [SUPPORT.md](SUPPORT.md) now says what qlcconsole promises QLC+ 4 users.
  Questions about qlcconsole, its QLC+ 4 parts included, go to this repo's
  issues rather than the upstream forum.
- **The docs moved out of the repository root**: design notes and reviews
  to `docs/design/`, developer docs (RELEASE, the test map and matrix,
  upstream intake, the generator API, the rig test plan) to `docs/dev/`,
  TODO, DONE and the strategy to `docs/project/`; [docs/README.md](docs/README.md)
  lists the layout. Fork-only developer scripts (`artnet-config.py`,
  `artnet-probe.py`, `rdm-sim.py`, `drift-test.sh`, `soak-mastertimer.sh`,
  `help-shots.sh`) moved to `tools/`; stray backup files left the root.
- Developer: `tools/check-doc-links.py` fails the gate and CI on a
  relative link in any tracked Markdown file that points at a missing
  file.
- Classic QLC+'s main toolbar has the DMX Monitor where QLC+ had its
  Monitor, then the Rig Overview, the address tool and Full Screen.
- **The course site has no teacher material**: the teacher guides, slides
  outlines, course plan and volunteer pages are read on GitHub, and the
  site's links to them go there. The site's build fails if one comes back
  (`student-pages.py --site`).

### Fixed

- **The Lighting Studio plot draws the stage's real size**: a 50 ft stage in metres is 15.24 m, not 15 -- the outline, centring, centre line and the off-grid count use it. The structure editor's cursor readout (H / V / Z) now solves at the pixel its depth came from, removing a 0.5-1 px error on a slope.
- **The Lighting Studio lines up with itself.** The rest of the alignment
  audit: the grid ends at the stage's edge and sits in the middle of the
  canvas when the plot is turned 90°, and fixtures re-place with the turn;
  **Snap to** 1/2 and 1/4 are the step a move snaps to (they only switched
  snapping on) and a fixture snaps by its centre; a free fixture's icon is
  centred on the point its light and aim lines start from, in Top, Front
  and Side, and Align lines those centres up; a fixture's name label
  clicks and repaints upright at 90°; the selection halo no longer leaves
  traces zoomed out. In the structure editors and the Rig Overview a click
  picks a fixture on the shape that is drawn (a lantern, a mover's yoke, a
  twin's every head, a thick bar's full width), at 45° what is in front of
  it; a turned frame's bar ends lie on its body; the rulers are drawn over
  the rig; the Rig Overview's ruler and the H / V / Z readout count from
  the centre line (or ⌖ 0,0), like the plot and the target readouts; the
  studio group editor keeps the spot you grabbed under the pointer and
  rules its grid in round steps of the show's units.
- **A scene played as it is (▣) in the Show Tree can be released.** Its
  layer had no Release, on its page or in the Workbench, so a scene added
  with + Scene tracked on to the end of the list; now it leaves when the
  cue it is released in is left, as any layer does.
- **An EFX or RGB Matrix made in Build is in the Show Tree's Library at
  once** (also renamed or deleted), not on the next visit.
- **A cue time typed and not yet left is what GO plays.** A fade or hold
  typed (or stepped) in the Show Tree's cue page or the Workbench's step
  header, with the field still open, only applied after moving away and
  back; GO, Play and every GO from elsewhere now commit it first.
- **Picking another effect always plays it.** A look with a release time
  (or a scene with an out-transition) kept its old effect running; in the
  Show Tree a new pick only changed the settings, not the running effect;
  picking one preset or WLED effect after another left the old one's
  settings on screen.
- **Shows tab, from a review against QLC+ 4.14.4**
  ([SHOW_MANAGER_PARITY.md](docs/dev/SHOW_MANAGER_PARITY.md)): a click on
  an empty part of a track moves the playhead again, as in QLC+ 4 (a drag
  still selects); Undo no longer carries one show's history into another
  show or document; deleting a function a show uses no longer leaves a
  stale block that the next zoom or drop could crash on; a plain click on
  a chase block no longer nudges it or marks the show changed; renaming a
  track renames its intensity level too, so a Virtual Console slider
  shows the new name.

- **Several fixed lights can be tilted up together**: Fixture Properties
  for more than one fixture held their tilt at level or below.
- **The power estimate is three times cheaper and no longer holds the
  engine back.** In Design mode the status bar re-estimates twice a second
  while levels change; on a 960-fixture rig that took 3 ms, almost all of
  it a text search run on every fixture's (empty) lamp name, and the
  engine's next frame waited for it. Now 0.9 ms, and the engine waits only
  for the values to be copied. Fixture groups with hundreds of pixels
  list their fixtures in linear time (a Show Tree edit on a big show,
  15% faster).
- **An idle show no longer uses several cores redrawing the rig.** With
  the Rig Overview open and Live on, its timer redrew the whole rig 25
  or more times a second whether or not anything had changed: a show
  left open overnight used about 3.8 cores for 19 hours. The view now
  redraws only when a channel it draws has changed, the rig was edited,
  or a gobo, prism or flame is turning, and not at all while its window
  is hidden, minimised or covered. A running show looks the same at the
  same frame-rate cap. The power estimate in the status bar likewise
  re-estimates only when a level, a circuit or the show changes. Idle,
  a 206-fixture show with the Lighting Studio and Rig Overview up went
  from 4.6 cores to 0.04.
- **QLC+ 4 deleted fixtures from a .qxw saved here** (test matrix C2).
  It has no unpatched fixtures, and read each of the Inventory's as
  patched at universe 1, address 1: it kept the first and refused the
  rest -- and any fixture really patched there, with what used it (a
  scene's values, a channel group), lost for good once saved in QLC+.
  A .qxw now keeps the Inventory in a `<StagedFixtures>` element QLC+ 4
  reads past; qlcconsole reads it back, the .qxw notice names it, and
  the stock QLC+ 4.14.4 gate test has an Inventory fixture at universe 1,
  address 1. A .qlcc is unchanged.
- **Windows: old QLC+ user data never came across** (test matrix M2).
  The merge of `%USERPROFILE%\QLC+` into qlcconsole's folder used HOME,
  which a Windows desktop session doesn't set, so it wrote
  `\qlcconsole` at the root of the current drive (`C:\qlcconsole`, the
  install folder); and its "links" were Windows shell shortcuts named
  `*.qxf`, which no loader can read ("Resource error" at every start).
  It now works under `%USERPROFILE%`, copies the files once (a copy
  removed stays removed, a file of your own wins), and replaces a
  shortcut an earlier build left. Tested with the Windows layout
  simulated, so every platform's gate runs it.
- **HID pad profiles**: your own went to
  `Library/Application Support/QLC+/HIDProfiles` under the home folder
  on every platform (stock QLC+'s folder on a Mac; a stray `Library`
  folder on Windows and Linux); they are now in qlcconsole's user folder
  (a Mac still reads the old one). And the installed Xbox profile is
  found on Windows and Linux: the plugin looked for the macOS bundle's
  `Resources/HIDProfiles` everywhere.
- **Memory**: about 75 KB a show open no longer leaks (test matrix
  L1-L4; m2leak under LSan on Linux, 10 open/edit/run/close cycles:
  75,105 bytes a cycle before, 6 after; the menu's QActions after 10
  cycles 6,876 before, 261 after): a generic dimmer's
  definition outlived its fixture; building an RGB matrix started the
  audio capture (an FFT plan and a beat tracker) through a temporary
  RGB Audio's destructor; a fixture definition file that could not be
  opened leaked its file object (once per definition a show names that
  isn't in its folder); and the Fixture Manager's group menu kept every
  old action (1,323 more QActions per show open). At quit, a Focus
  session's light layer no longer calls into the destroyed show (test
  matrix L7: 3 UBSan reports at quit before, 0 after), nor the update
  footer into its half-destroyed label.
- **Thread safety** (test matrix TSan L2, L4, L5): the universe's value
  arrays are never shared, so a frame no longer reallocates one under a
  per-channel read from the GUI (Build's power estimate, the plot); an
  external beat that came in during a timer tick was lost at its end and
  is now the next tick's beat; a running function's elapsed time, read
  by effect scripts from the GUI thread, is atomic.
- **Art-Net sequence per node** (test matrix N1): each node counts its own
  ArtDmx Sequence 1, 2, 3..., however many nodes this console sends the
  same Art-Net universe to (DanceHysteria 0.6: 18 nodes on universe 0,
  each saw it jump by 18).
- **A stock show's network patch with no interface named** (`UID="None"`,
  a line number; test matrix C7) was bound to whichever interface this
  computer lists at that number -- another network -- and saving wrote
  that network into the show. It now follows the patch's target address,
  or waits, kept as it was, for you to patch it.
- **Collect for Transfer takes the gobo swaps' pictures**: your own gobo
  pictures (from your gobo folder or a file anywhere) go into `images/`
  with the rest and the collected show names them there; a gobo from the
  app's library stays a name. A swap's picture beside the show is saved
  relative to it.
- **Web access on a Pi**: an SSID or password typed on the network page
  is passed to nmcli whole; it was split on spaces ("My Show Net" became
  three arguments, and a password could add nmcli options), and the
  hotspot's name kept literal quote marks.
- **`QLC_SHOW_DEBUG`'s log** is written to `showdebug.log` in the user's
  qlcconsole folder instead of the fixed `/tmp/qlc_showdebug.log`, a name
  another user of the machine could take first (red team L3).
- **Heavy Art-Net input no longer starves the GUI and web access.**
  Every input value on channels 128-255, from any plugin, rebuilt the
  effect scripts' whole "midi" data (thousands of values per input
  universe); 16 universes of changing Art-Net did that ~90,000 times a
  second on the main thread, and a web client waited seconds for an
  answer. It is now rebuilt once per tick, when effects read it. And each
  frame of input now reaches the main thread as one hand-over per
  universe instead of one per value. On the Linux test machine, with 16
  universes of Art-Net coming in: web round trip 2 s -> 41 ms (as idle),
  CPU 2.2 -> 1.2 cores, no late timer ticks (was ~23 per 2 minutes).
- **Opening the app no longer sends Art-Net polls onto the network by
  itself.** A show saved with Connections in front polled every interface
  (ArtPoll, broadcast) as the window opened, patched or not, and so did a
  headless (offscreen) run. Arriving at Connections yourself still polls
  once; **Rescan** always does; open Art-Net outputs poll as before.
- **Re-addressing a fixture on universe 129 or above moved it 128
  universes down** (the address kept only 7 bits of the universe; QLC+
  has the same mask). Loading a show was not affected.
- **A fixture past the 1024-universe maximum is refused**, with a message,
  instead of being added with no universe under it (an RGB matrix on it
  crashed the app). Patching or moving one there is refused too.
- **A fixture put on the back face of a platform's frame group jumped to
  the front face** the first time it was dragged in a structure editor.
- **A Virtual Console Flash button on a Show Tree hit played nothing**:
  only scenes flashed, and a hit is a collection. Flash now plays the hit
  while held and lets go on release; a hit already on stays on.
- **A flaky test steadied**: AppViews_Test's GO-holds-the-preview test
  (and the two GO tests beside it) read a dimmer's output from the live
  universe array, which the output thread zeroes at the start of every
  frame before writing it again; on a loaded machine a read landed in
  between and saw 0 for a dimmer at full. They now read a whole frame
  (the copy taken under the frame lock), and wait for the run to stop and
  the Show Tree's sweep to fire instead of a fixed 400 ms. VenueRig_Test's
  not-ours-on-the-real-tick test read one value the same way and now
  reads a whole frame too.
- **No test can put a window, or a crash dialog, on the screen** (for
  developers): every test.sh, unittest.sh and every test program itself
  (engine/test/common/testguard.cpp, linked into each) run Qt offscreen
  unless QLC_TEST_REAL_SCREEN=1; a crashing test program on macOS exits
  without ReportCrash's "quit unexpectedly" dialog (QLC_TEST_CRASH_REPORTS=1
  keeps it). The gate fails when a test.sh or a test program lacks the guard
  (platforms/linux/check-test-guard.sh, with its own self-test).
- **sACN (E1.31) universes above 255 had no multicast address**: the
  default group was written 239.255.0.<n>, which is no address from
  universe 256 on, so such a universe's output went nowhere and its
  configuration dialog could crash. It is now 239.255.<high>.<low>, as
  E1.31 says; universes 1-255 are unchanged.
- **Art-Net**: an ArtPoll answered from an interface with no hardware
  address (a VPN tunnel, some USB adapters) was 5 bytes short, with its
  later fields out of place; every universe's first ArtDmx now carries
  Sequence 1, not 0 ("not sequenced"), above Port-Address 3 too; and a
  QLC+ workspace from before 2016 whose output IP was saved as its last
  two or three octets ("3.11") sends to the right address again.
- **A Script line with a syntax error no longer runs half of itself**:
  `setfixture:1 chanel:2 val:255` (a typo) set channel 0 of fixture 1 to
  0. A line the editor marks as an error now does nothing. And
  `wait:random(5)` (no upper bound) is reported as an error instead of
  waiting 49 days.
- **OSC feedback with no path** sent an empty datagram to the feedback
  target; it sends nothing.
- **Unpatching a universe could wipe another universe's output settings**
  (an Art-Net or sACN target, a port) from the next save, and left its
  own behind for the next line it was patched to: every plugin passed
  the line and the universe the wrong way round (also in QLC+).
- **RDM**: a SET to a device whose UID reads as a number (000000000001)
  carried a stray byte, and a discovery branch with no range could crash.
- **A web access upload that isn't a show no longer empties the console**:
  the open show was cleared before the file was read. And a show opened
  from a file after one sent over web access can run Script programs
  again (if allowed); the web mark stuck.
- **Importing from another show kept every member of a collection**: when
  the imported ids chained (one landing on the id the next one had), a
  member was dropped.
- **Code coverage campaign** (for developers): coverage.sh measures the
  whole build, the plugins and web access included, with branch coverage
  and a per-file table; the gaps that matter (show files, output, the
  network plugins, the Show Tree, venues, focus and Refine, web access
  auth, scripts) have new tests, and the bugs above came out of them.
  Numbers and how to reproduce them: docs/dev/TEST_MATRIX.md, "Coverage".

## [0.3.0] - 2026-10-26

qlcconsole 0.3.0: the Build tab and its Workbench (scenes, collections and
chasers edited as layers, shared with the Show Tree); the Classic QLC+ /
qlcconsole / Everything views and Upgrade This Show for older QLC+ shows;
house rigs, Quick focus, Venue focus and its per-venue corrections, Focus
this cue and stage marks for
touring a show between rooms; plot symbols and output rules on fixtures;
Eos shows imported with their special circuits and subs; skip cues, cue
colours and move in black on the Show Tree. Developed as 0.2.2: notes and
tests that cite "CHANGELOG 0.2.2" mean this section.

Release candidate 2 adds, from testing RC1: the Stage layer (stage marks
and the apron as pieces of their own) and the Rig Overview footer; the
Focus menu, its reasons and names, and Voice detail; one connection window
in Connections; a controller asleep or woken never failing silently; Plot
layers following the plot and nesting by mount, with locks per object and
per layer; the Classic QLC+ Functions tab without the Show Tree's own and
the scene editor's General above its sliders; Classic QLC+ saying when a
show plays Show Tree content; and faster Matrix effects.

Release candidate 3 adds, from testing RC2: the venue chip in the footer
and one venue menu wherever it opens (the chip, View ▸ Venue…, Focus ▸
Venues), amber with a count when the venue clashes; an apron that can sit
on the floor and stays deck when hidden; placing on a locked layer asking
where the object goes; the patch list grouped, filtered and exported to
Excel, with labels on tape; Connections' Overview folded into Devices,
and the Fixture Manager patching a universe in the same connection window;
and the connection window's MIDI input channel no longer one off.

Release candidate 4 adds, from testing RC3: the Labels dialog picking a
Brother PT-P710BT on the Mac automatically and starting on its preset and
printer; unpatched rows in the patch list showing "— not patched"; and a
steady RDM flag on Art-Net nodes whose poll replies carry a junk status
byte. From the test matrices: a damaged show file says so and is never
saved over; impossible universe numbers are refused; Office.qlcc no longer
opens already modified; numbers read with either decimal mark; the rig
view stays memory-safe across New, Open and deleting structures, and drops
junk plot zoom and flags; malformed Art-Net, RDM and E1.31 packets from the
network are dropped instead of read past their end; bad chaser and
sequence step numbers in a show file are refused; the RDM DEVICE_INFO
sensor count is read from the spec's offset; and the tests run clean under
ASan and UBSan, with no generic fader test hang. On Windows, the Build
outline, Build ▸ Effects and the Show Tree Library refresh when the show
changes (signals across DLLs), and CI's Windows tests now fail the build.

Release candidate 5 fixes, from testing RC4: the Focus screen no longer
changes its layout as the controller's buttons change what it says (nor
does Focus this cue); the voice falls back to the computer's own output
when the chosen "Voice to:" output is gone, instead of losing every line;
and Fixture Properties' orientation diagram draws a fixed light where it
really throws. From the test matrices: an RGB matrix whose script isn't
installed keeps it through a save; Save As to another folder keeps the
truss and tower pictures with the new show; four thread races found by
ThreadSanitizer (stop-all on File ▸ New or Open, a chaser's runner, the
run-state flags, Blind); and the show-file tests fit CI Windows' time.

Release candidate 6 fixes, from the overnight fuzzing and mutation runs:
a show with a fixture claiming more channels than a universe holds loads
it unpatched instead of writing past the universe; a fixture definition
with a huge head channel number no longer reads past its mode;
Connections ▸ Universes lists only the open show's universes, however
many shows were opened, and renames the universe picked; and new tests pin
the venue's fixtures after it is turned off or the show replaced, a Show
Tree of only productions, and a not-ours range's end.

Release candidate 7 brings the in-app help up to date with 0.3.0 (an
audit of every feature against the code; three pictures retaken and now
regenerated with the rest) and fixes: a chaser step's Fade and Hold take
either decimal mark, as the other time fields already did; Collect for
Transfer carries the show's pictures with it; and Set Mounting Height…
asks in the show's units. Two labels say what the app really does: the
autosave file names in Settings and the Patch List… tooltip.

Release candidate 8 is a security and packaging release, with fixes
from testing RC7. Security, from a red-team review (board Q175): **web
access listens on this computer only** (`--web-bind` opens it to the
network), and from anywhere else a browser can look but only a
logged-in user (`-wa`) with the right level may change the show; pages
on other sites can't drive it. **A Script's `systemcommand` is off by
default**: it runs a program only with Settings ▸ Advanced ▸ Allow
scripts to run programs on, never for a show that came in over the web,
and previews never run scripts. Malformed OSC packets, short or unknown
web commands, a huge upload and quitting with a browser open no longer
crash or hang the console; names in the web pages and in exported CSVs
can no longer run as code. Packaging (board Q177): the Mac app no longer
carries the build Mac's own fixtures and scripts; every installer ships
its licence texts and runs headless, and passes a new pre-release check
with no waiver; the Linux AppImage runs without Qt installed; on
Windows, an LGPL FFmpeg, one install per user, and an uninstaller that
removes the settings. On the plot: a locked plot or layer moves nothing
when dragged, aim targets included; the rulers count from the centre
line and hug what they measure; Align and Distribute line up centres
and keep heights; Plot layers selects every row picked; and lasers show
on the 2D plot. On the run: GO, Back and Sub GO stop the previews first,
so only the run reaches the stage, and a game pad's A and B no longer
step running chasers. And the main window follows every resize again
after a panel was moved to its own window.

### Added

- **The venue chip and one venue menu** (RC3, board Q150). The footer, beside
  Ready and the Grand Master, says the venue applied: **Venue: Lincoln HS**,
  or **Venue: none** (`VenueChip`, objectName `footerVenue`; always in the
  qlcconsole and Everything views, in Classic only once the show has a
  venue). A click opens the venue menu (`VenueMenu::fill`, one
  implementation): **Switch venue ▸** (the venues and None; clashes asked
  about first, as Apply now does), **New venue…**, **Rename…** (the venue
  applied; corrections, house rig and patch go with the name), **Rig and
  universes…**, **Venue focus…**, **Focus report…**, and **Clashes (n)…**
  when the venue applied has any (the venue page, its clash list in front).
  **View ▸ Venue…** opens the same menu, and the Lighting Studio's **Focus ▸
  Venues ▸** now is that menu (its Apply ▸ is Switch venue ▸). The chip
  turns amber with the count ("Venue: Lincoln HS · 3 clashes") while the
  venue clashes with the show's own rig, and follows a venue applied,
  renamed, deleted or confirmed and the rig changing (fixtures patched,
  moved, added or removed; a quarter-second after a burst). "Still at
  <venue>?" on opening is kept: until it is answered the chip reads
  **Venue: Lincoln HS?** and the menu starts with **Still at Lincoln HS?…**;
  the answer shows on the chip (`VenueCorrections::confirmedChanged`).
  New venue and Rename ask in small dialogs that are not modal.

- **Patch list: grouped, filtered, exported to Excel, and labels on tape**
  (RC3, board Q151). The patch list (`PatchListDialog`, now a tree) groups
  by **Location** (default, remembered as `patchlist/groupby`: the truss,
  bar, tower, riser or deck a fixture hangs on, a hanger under its truss
  or tower; else its first fixture group; else "Not placed", last),
  **Universe**, **Type** or **None**, in address order within each. **Show:
  All / Discrete / RGB Matrix & pixel fixtures** and **Owner: Tour / House
  / Rental** filter the list, the copy, the CSV, a new **XLSX** export
  (`XlsxWriter`: one sheet, stored ZIP, no new dependency) and the PDF,
  which follow the grouping (a first column for the heading). Pixel /
  matrix = an "LED Bar (Pixels)" definition, or a mode with two or more
  heads with RGB channels of their own (`PatchList::isPixelFixture`). The
  **Ch** column is gone: the Mode says the count when its name doesn't
  ("Open + Fly (2 ch)"). Right-click → **Print Label… / Print N
  Labels…**, and **Print Labels…** for the list shown. Labels
  (`PatchLabels`) say what a remembered template says, default
  **`{name} D{address}`** ("Spot 3 D101"; fields {name} {address}
  {universe} {mode} {location} {owner} {group}); the address is the DMX
  start address, "2.101" when the show patches more than one universe.
  New stocks: **Continuous tape** 12 / 24 mm (TZe; one line as tall as the
  tape prints, the label as long as the longest text needs, 25-150 mm, a
  cut gap) -- now the default -- and a **Brother PT-P710BT** preset (24 mm
  tape through Brother's macOS driver; Print picks that printer when it is
  there). **Print…** goes through the system print dialog (QPrinter, Qt
  PrintSupport now linked); **Save PDF…** writes the same pages; the
  dialog previews the first label.

- **House rig, tour package and universes per venue** (docs/design/VENUES.md, board
  Q137-Q138). Fixtures and fixture groups have an owner (`RigOwner`: Tour,
  House, Rental; a fixture follows its group unless it says otherwise),
  saved only when set. With a venue applied, House fixtures live in that
  venue's sidecar (`VenueRig`: the fixtures, their plot data, and which of
  them fill the show's groups), so switching venue swaps the house rig and
  leaves the tour package alone; a cue on a group plays on each venue's
  house rig; house ids are reserved and renumbered on a collision. Each
  universe is a Tour or House slot; a venue keeps the output patch of every
  slot (the show file keeps its own, `Universe::setShowOutputBase()`) and
  can mark universes or ranges not ours (`Universe::setNotOurs()`: nothing
  sent, a range zeroed; patching there refused). A clash check (overlaps,
  not ours, slots) runs before Apply now, on open and in the Patch dialog.
  UI: Owner in the group editor and Fixture Properties, Slot on the
  Connections toolbar, the venue page (Focus ▸ Venues ▸ Rig and universes…),
  the plot's Owner overlay, owners in the Inventory and the patch list.
- **The Focus menu and the focus tools' names** (board Q143, Q146, Q148,
  Q149). One **Focus ▾** drop-down on the Lighting Studio toolbar, like
  Add: Quick focus, Venue focus, Edit focus calls, Venues ▸ (Apply, New
  venue, Rig and universes), Practice (scramble) and Focus report
  (`FocusMenu`). An entry that cannot be used is greyed with the reason in
  its tooltip -- no lights placed on the plot, no focus calls, no venue, a
  show not saved -- there, on the Run tab's Venue focus, the Rig
  Overview's Quick focus and the Show Tree. Quick focus's setup says why
  Start is greyed (no moving heads on the plot, no stage target), never
  shows a blank Aim at ("No stage target yet") and offers **Add a
  target…**. Quick focus is now **Quick focus**, Focus calls **Venue
  focus**, Refine **Focus this cue** (in Build: **Focus this look**), and
  the cue page's "Focus call" tick box **Make this a venue focus cue**;
  the Show Tree cue page and the Workbench have a **Focus ▾** in their
  header. The Practice dialogs say plainly what a scramble is.
- **Voice detail** (Branson's testing, 2026-10-25): a **Voice** setting
  beside Feedback on every focus panel -- Terse ("F1 Spot 3", "Saved",
  "Saved all"), Standard, the default ("Fixture 1, Spot 3", "Saved
  fixture 1 to Lincoln HS", "Saved all, 6 lights"), Verbose ("Fixture 1
  of 8, Spot 3", "Saved Spot 3", the call's target and place) -- one
  setting for Quick focus, Venue focus and Focus this cue, kept with
  Feedback. Every stepping, call, save, save-all and revert line comes
  from one phrase table (`ControllerScheme::stepPhrase()` and its
  siblings); warnings and safety lines are never shortened. Holding A now
  confirms how many lights it saved, said and shown on the panel. Quick
  focus lists the selected lights that cannot be aimed and excludes them
  (the default) or cancels, rather than dropping them silently.

- **Venue focus, per-venue corrections and Focus this cue**
  (docs/design/FOCUS_REFINE_DESIGN.md, board Q105-Q136). One controller map
  (`ControllerScheme`) shared by Quick focus, Venue focus and Focus this
  cue, with the on-screen map labelled per tool. *Venue focus*
  (`FocusCalls`, saved in the show; Lighting Studio ▸ Focus ▸ Venue focus)
  runs the show's focus calls in order on the controller: triggers step the
  calls, bumpers the fixtures, the stick nudges, A saves, group scope saves
  and undoes as one. *Venues* (`VenueCorrections`): corrections, truss trims
  and target positions go to a sidecar file beside the show, written on every
  keep and re-applied on open; the show file never takes them
  (`MonitorProperties::setCalibrationBase()`); a venue manager (apply now,
  rename, delete), "Still at <venue>?" on open, the fixture-type check (a
  swapped light needs focus), the pad's target page, and Collect for Transfer
  copying the venue files. *Focus this cue* (`RefineSession`, `RefinePanel`): the
  same controller on a Show Tree cue's own values or a Build scene --
  position, zoom, focus, colour and gobo at Layer, Group or Fixture scope,
  previewed on the rig and kept by A as one Show Tree step (an inline
  palette, or a child layer for an exception); from the Show Tree's cue page
  and layer menu, the Workbench header's **Focus ▾** and its rows' menus;
  quiet in Operate. A *venue focus cue* arms on GO and starts on the
  controller's Start. Operator feedback: lights, voice or both (`say`,
  `spd-say`, Qt TextToSpeech, with a queue that never drops a confirmation
  and important lines always spoken), Say the light, the pad's light bar and
  rumble, an accessible save lamp, a held D-pad repeating, hold A for one
  second. Safety: a controller lost freezes everything it held, two pads --
  the first to press Start owns the tools -- and "nothing reaching the rig"
  (Blackout, Grand Master at zero, no output) is said. Help pages
  `focus-calls.md` and `refine.md`.

- **The focus report, practice, and the last pad moves** (board Q134,
  Q139, Q140; docs/design/FOCUS_REFINE_DESIGN.md section 13). `FocusReport`: a venue's
  focus per light and call -- target with its stage-marks position,
  correction, lens, checked by and when (`VenueCorrections::Check`, every
  save), notes, dead / swapped / needs-focus -- as HTML, PDF
  (`QPdfWriter`) or a Lightwright-style CSV, from the venue manager.
  Scramble practice (`VenueCorrections::startPractice()`): a practice
  venue with every moving head put off by a random amount, the real venue
  untouched and put back at the end, the views drawing where the
  scrambled lights land (`practiceViewProps()`), scored on error, time and
  saves, the score in the report. Hold B puts back every unsaved light as
  one undo step. Both stick clicks hold every light
  (`ProgrammerController::setHolding()`, H); View + X (D) marks a light
  dead; spot down / flood out / sharper / softer on the map and in the
  voice (`ControllerScheme::crewWord()`); calls say the target's place
  ("SL 2, US 1"); a light clicked on the plot is jumped to; the focus look
  also takes gobo, prism and frost out (`FocusSession::openBeamValue()`);
  a new venue can start from the last one (`addVenueFrom()`). On macOS
  the HID plugin reads DualShock 4 / DualSense pads through
  GameController into the Xbox layout (`GamepadLayout::PlayStation`,
  `playstation.qxhid`), so their light bar follows the save lamp.

- **Input devices follow their device** (Bluetooth pads asleep and awake):
  an input patch on a plugin whose lines are hardware (HID, MIDI) remembers
  its line's name (`InputPatch::inputUID()`: maker, model and serial). When
  the plugin rescans (`configurationChanged()`),
  `InputOutputMap::followInputDevices()` keeps a patch whose device is gone
  as an offline patch (profile, HID profile and parameters with it), reopens
  one whose device moved to another line on that line, and the retry puts a
  kept one back as soon as its device reappears -- at once on the rescan,
  not at the next 5 s tick. New signals `inputDeviceDisconnected()` /
  `inputDeviceConnected()`. `InputDeviceNotices` (UI) says it in the notice
  line ("Controller '…' disconnected — universe 2", amber; "… connected"),
  debounced 2.5 s so a Bluetooth flap says nothing, and as the footer
  readiness entry `input.devices`. Connections ▸ Devices draws an absent
  device as a greyed line, "not connected", with its universe; Quick focus
  says "Controller connected". On Linux the HID plugin now rescans when
  `/dev/input` changes, so a Bluetooth pad is noticed there too.
- **Stage marks, as stage pieces** (board Q147): the centre line / DSC
  mark, the curtain line, the number line and the apron are objects on the
  plot (`StagePieceItem`), on their own **Stage** layer in Plot layers
  (`MonitorProperties::ensureStageLayer()`, kept with the show, hidden and
  locked like any layer but never removed), each with its own eye and lock
  as well (`stagePieceVisible()`, `stagePieceLocked()`). A click selects one
  (a rubber band never does), the Layers list follows; the curtain line and
  the number line drag up and downstage and the apron by its front edge,
  one undo step a drag, snapped with Snap on; a locked piece or layer holds
  it. Double-click or **Properties…** (`StagePieceDialog`, one undo step,
  Cancel puts it back): Shown, Lock, and where it is as a crew measures it
  -- the curtain line Auto or a distance upstage of the front of the deck,
  the number line Auto or a distance downstage of the curtain line, the
  apron Auto or a depth and front curve, the centre piece's DSC mark and
  full-depth line -- every distance entered and read in the show's unit as
  it is when it opens (the positions read "1.00 m" on a feet show, built
  with the unit the Studio opened with). The curtain line is found from
  the most downstage main drape or proscenium across centre, else 4 ft /
  1.2 m upstage of the front of the deck (the biggest platform across
  centre and any butted to its front, or the grid's front edge); a set line
  is kept as the plan's Y and stays put when the deck or the other line
  moves. The number line is ticked and numbered in the show's unit out from
  CL, each number centred on its tick, long ticks only on numbered places,
  the same rule on the plot and the Rig Overview (`numberLineStride()`,
  `numberLineMajorTick()`, `numberLineLabel()`); the centre mark's own CL
  gives way to the number line's when that runs through it; with the full
  centre line on, the teal centre cross's up-and-down arm isn't drawn.
  Horizontal trusses and pipes downstage of the curtain line are labelled
  "FOH · 4.2 m DS of curtain line" (`curtainLineOffsetText()`). The Rig
  Overview draws the lines on its floor (Top and 45°, the words left out
  while it's small) and lists the pieces under Stage in its Plot layers
  menu. Saved as a `StagePieces` element on `Monitor` only once used; a show
  saved with the earlier **Show ▸ Stage marks** attributes (`StageMarks`,
  `CurtainLineY`, `NumberLineY`) loads as the same pieces in the same
  places, the apron it tinted now the apron piece on Auto, and saves the
  new way; an older show opens with none. The plot's **Show** menu keeps
  only view toggles: **Target positions** is there now.
- **The apron is its own piece** (Q147, Q146): it starts at the curtain line
  and runs downstage by its depth (`setApronDepth()`, `setApronFrontCurve()`,
  Auto to the main deck's front with its front curve), as wide and high as
  the main deck and in its colour, and it is real deck: its own platform on
  the Stage layer (`apronPlatform()`, kept to the curtain line by
  `syncApron()`, saved with the apron rather than as a platform), drawn as
  deck on the plot in every view and in the Rig Overview from above, at 45°
  and in 3D whatever marks are on (it was only a tint while the curtain line
  was on), stood on from the floor (`platformBaseZ()` 0), never the main
  deck. It never borrows another platform: the old **Apron depth** resized
  the platform butted to the deck's front, so on a stage with a centre riser
  there it moved the riser; now nothing else moves.
- **The Rig Overview's controls are in a footer bar** (Q145), like the
  Lighting Studio plot's and built the same way (a dark bar in a
  `ShrinkableBar`): **Show** (Target positions, shared with the plot) and
  **Plot layers** on the left, Live, Beams, Room, Haze, Quality, GPU, the
  frame rate, Focus and a new **Fit** (`StructureStudioView::fitToRig()`),
  and **View** on the right where the plot has its own. The title bar is as
  it was; the Close row under it is gone (⌘W and the title bar close it).
- **Snap takes the whole rig**: with the plot's **Snap** on, a dragged
  truss, tower, stand, pipe or scenery piece lands on the grid (it stayed
  where the mouse let go; platforms, power and nodes already snapped), and
  a grouped selection moves by the same delta.
- **Course lesson 3.6, Focus and calibration**: focus-call etiquette, the
  stage marks, focus calls on the controller with a venue, the scored
  scramble and its focus report as the hand-in, Focus this cue; a touring piece in
  the teacher guide. Checked step by step by `courseshow_test`
  (`lessonFocusAndCalibration`), which also takes its pictures. Help
  pictures of the venue page, the focus report and the practice score;
  Quick focus's controller map and save lamp now taken by `tools/help-shots.sh`.
- **Target positions in crew terms**: a readout beside each stage target on
  the plot, each figure/area in the Rig Overview and Quick focus's fixed-light
  figure ("SL 2.4 · US 3.1 m", "SR 7' 11\" · US 10'", "↑ 0.9 m" on a deck),
  measured from centre and the number line (the curtain line when only that
  is shown), live while dragged or nudged. One formatter,
  `MonitorProperties::formatStageOffset()`, with a compact and a spoken
  form ("SL 2, US 1") for calls and voice. **Show ▸ Target positions**
  (on the plot and the Rig Overview) hides them; on by default, saved with the show only when off
  (`TargetPositions="0"` on `Monitor`), not an undo step.
- **Quick focus save status**: a status lamp in the panel (`FocusStatusLamp`)
  -- green all saved, yellow the current light unsaved, red another light
  unsaved, a red ring round a yellow centre for both -- with the unsaved
  lights named beside it, live through the new `FocusSession::unsavedChanged()`.
  A save (A / Return) flashes it green and blinks the real fixture twice
  (dark and back, 150 ms a phase, in `FocusSession`'s own writes; stepped by
  `advanceBlink()`, so tests need no sleeps; `setRigFeedback(false)` turns the
  blink off); a revert does not blink. Lights with unsaved changes get a
  dashed yellow ring and a badge on the plot and in the Rig Overview
  (`Monitor::showFocusUnsaved()`), and the Exit question names them.
- **The beam in Quick focus** (Q103): D-pad up/down (or . and ,) widen and narrow
  the beam within the definition's lens range, D-pad right/left (or ' and ;)
  a focus channel's edge while focusing. A lens zoomed by hand is set
  and saved with the light (also **Beam (lens set to)** in Fixture
  Properties) and the beam is drawn at it; a zoom channel is driven while
  focusing only. **As selected** is always offered, from what is selected
  on the plot.
- **Quick focus for fixed lights** (board Q102): pars and fixed spots step
  through Quick focus too. Each is lit and a person stands where it lands at
  head height; move that spot (sticks, D-pad, arrows) to where the real
  light lands, and its Pan and Tilt are set from it. Save, revert and undo
  as for moving heads; it needs no target.
- **Aim a fixed light by Pan and Tilt** (board Q100): a par or profile's
  **Aim & Mount** has **Pan** (° from downstage, seen from above) and
  **Tilt** (° down from level); the 3D beam, where the light lands and what
  blocks it follow. Before, a single-head fixed light on a truss always
  pointed straight down and "Aim — front of device" (a moving head's pan
  home) did nothing for it.

- **A door in a wall** (board Q99): a wall or back wall has **Door width**,
  **Door height** and **Door at** in its editor. Light goes through the
  door; the wall beside and above it stops it.

- **One Overrides chip** (board Q80) in the footer: "Overrides: Park (3),
  Hold at end · Clear" whenever Park, Highlight, Hold at end, Quick focus or
  Blind holds the rig; one click lets go of all of them.
- **Build tab and the Workbench** (docs/design/WORKFLOW_DESIGN.md W1–W4). The
  Programming tab is **Build**: **Palettes | Effects | Scenes** picks what
  the navigator lists, with Groups & fixtures under each, and the
  **Workbench** on the right loads a scene, collection, chaser or any other
  function as layers. On a collection: **+ Scene**, **+ Effect ▾** (EFX,
  RGB Matrix), remove (⌫), ▲ ▼, drag scenes in, **▣ Open** / **↩**, **also in
  N** and **Own copy** for shared members. The editor pane hosts the Look
  Editor, the channel console, timing and the stock editors. Design mode
  previews what is loaded. Every edit is one step on the history the Show
  Tree shares. No file format change; old QLC+ shows open as they are.
- **Build ▸ Scenes outline.** Folders → scenes, collections and
  chasers, each scene opening into its targets and looks; ⊞ / ⊟, badges
  with details on hover, drag rows between scenes (⌥ copies), F2 renames.
- **Chasers in the Workbench.** A step walker (◀ ▶, the step's fades,
  hold, cue number and note in place), the step table, and the step's
  function as layers. **Chaser from selection**; scenes dropped on a
  chaser become steps; **Make this step's own copy**.
- **Effect designer and Build ▸ Effects.** The effect browser at full
  height beside the effect's settings and its own colour and dimmer,
  previewing live on the target heads. Keep it as a one-off, a new
  palette or an update. Hosted by **Design effect…** on a Show
  Tree layer and by Build ▸ Effects (**+ New ▾** and a right-click menu),
  effects' one home (there is no separate Effects window). **Make a layer** turns the bench's groups and
  effect into a new layer of the cue or scene it came from, the selected
  cue, a picked scene or a new one.
- **Show Tree ↔ Workbench (W3).** A cue, hit or layer opens in the
  Workbench docked in the Show Tree (double-click, **✎ Workbench**, **Open
  in Workbench**, `wb`): its own layers a row per attribute, tracked-in
  values grey and read-only, Solo, **↩ Show Tree**.
- **Workspace views.** **View ▸ Workspace ▸ Classic QLC+ / qlcconsole /
  Everything**, saved per show (an optional `View=` attribute older
  builds ignore). Classic shows QLC+'s tabs in its order (Connections reads
  *Inputs/Outputs*); qlcconsole shows Connections, Fixtures, Lighting
  Studio, Build, Show Tree, Virtual Console, Simple Desk and Run (Simple
  Desk, a QLC+ user's channel check, after the Virtual Console);
  Everything all ten. A show with no Show Tree not saved by qlcconsole opens in Classic.
  A Window-menu jump to a hidden tab switches to Everything with a *Back
  to …* line.
- **Upgrade This Show…** (File menu and the Show Tree's old-show line;
  W5): every chaser (cue list with its same-output check, effect kept as a
  function, or left alone) and collection, then Capture into palettes for
  raw scenes; one undo step. **Import in place** links the list to its
  chaser, files the originals under `Imported/…` and moves VC widgets,
  timelines and show control to the list; **Restore original…** undoes
  it. Older builds ignore the new XML.
- **A chaser made a cue list shows a sheet**: plays-the-same check, a row
  per cue, what plays as it is and why, and an offer (ticked) to move what
  fires the chaser to the new list. What consecutive steps repeat is set
  once and tracks. **Send to Show Tree…** in Build opens Upgrade This
  Show… with just that chaser ticked. A ▣ layer opens its function, with where else it plays.
- **Show Tree: cue order is number order.** A typed number (Number field,
  or F2 on the row as "number name") moves the cue to its place; a taken
  number, a sub-cue's number or one that would clash with the cue's own
  sub-cues is refused. Dragging one or several cues numbers each dropped
  cue for its new place by + Cue's rule (between 2 and 3 → 2.5) and
  renumbers nothing else; Move up / Move down do the same. **Renumber this
  level…** numbers a level from a start in steps, point cues and sub-cues
  following, and asks first while OSC/MSC is listening. One undo step
  each; numbers save as typed; nothing is re-sorted on load.
- **Skip cues.** Untick a cue, **⌘/**, **Skip cue** or `skip 0.0.3 [thru
  0.0.5]`: GO, auto-follow and Back pass over it and it tracks as if it
  weren't there; a jump still plays it. Saved as `Skip="1"`.
- **Move in black per cue list and per cue.** A list's **Move in black**
  On / Off / as the show, a cue's **Don't move in black into this cue**,
  ⊘ MIB / ▸ MIB badges, `mib on|off|show [list]`, `nomib` / `allowmib`.
  The show's switch is the Show Tree's **MIB** button (lead time in its
  menu, *MIB · n* while pre-positioning); Build's **Move in black ▾**
  (*Move in black · Auto*) shows and changes it.
- **Cue colours.** A text colour per cue list and cue (**Colour ▸**, the
  page's **Text colour**, `textcolour`), shown in the tree, the Run tab,
  the VC Cue Runner and Cue List and the Workbench header, kept readable
  on every theme.
- **⇧⌘N, New here** in the Show Tree, Build and the Fixtures tab; **Edit ▸
  New …** names what it makes.
- **Notes open up while typed** (cue notes, chaser step notes): wrap and
  grow to six lines; Shift+Return for a new line.
- **Group tools.** *Select All Similar*, *Move to Folder ▸* (folders in
  use, *New Folder…*) and *Create Group(s) from Selection…* in the
  selection's folder, in the Fixtures tab, Build's Groups & fixtures and
  the Show Tree's Library (with *Make set…*). **Return** / **F2** renames a
  group in the Fixtures tree.
- **Plot symbols.** Per fixture: Auto, or sixteen symbols in five sections
  (Moving; Fixed conventional; Fixed LED; Practicals: Lantern and Staff;
  Scenery: Curtain), from Fixture Properties or the Fixtures tab's **Plot Symbol ▸** (also in
  Operate: it changes only the picture). The 2D plot and the studio's
  Top/Front/Side views draw the symbol; a lantern glows all round, a staff
  is a pole wound with LED strip. Saved as `<StageLook>` only when not
  Auto. The plot's **Show** menu adds **Show Auto fixtures as their
  closest plot symbol**, saved with the show (off by default).
- **Curtain plot symbol** (Q51). A **Scenery** section in the plot-symbol
  picker (Fixture Properties, Plot Symbol ▸) with **Curtain**, saved as
  `<StageLook>Curtain</StageLook>`. A fixture marked Curtain — a relay on
  a dimmer channel that runs a traveler — is a curtain fixture: listed as
  a drape's driver, drawn on the plot as the drape (its fixture item
  hidden, its place kept), kept out of Inventory, shown with the curtain
  icon. With no *Open* / *Fly* channel its master intensity (else its
  first channel) opens it, 0 closed, 255 open. Back to Auto restores it;
  ⌘Z undoes it.
- **One icon per fixture, everywhere, and a curtain icon.** Every fixture
  and group list uses the same icon (its plot symbol, a curtain's drape,
  its kind), with the output-rules badge on top. Rows say what they are:
  a Dimmer look's pie, a Shutter look's state, a Strobe look's rate, a
  target's bullseye or line, a fixture's kind.
- **Output rules** for relays, house and work lights: **Ignore
  Blackout** (also through Blind), **Ignore the Grand Master**, **Switch
  (relay)** (on at 50 % and over, never faded, the Grand Master can't
  switch it off) and **On when not in use** (full whenever nothing sets
  it, from the switch to Operate or when the show opens: Settings ▸
  Advanced ▸ Output). Right-click **Output Rules…** in the Fixtures tab,
  Build's Groups & fixtures and the Show Tree's Library; one rig undo
  step; an amber badge; saved as an optional `OutputRules` element.
- **Eos import: special circuits and subs.** A **Special circuits** page
  reads relay, house, work and backstage / hall channels from their Eos
  labels (changeable per row or selection; `$$Options X` marks GM
  exempt), gives them output rules and groups under **Eos** (*Power
  relays*, *House & Work*, *Backstage*). Each sub becomes a Playback fader
  in a Virtual Console frame **Eos subs**.
- **Edit Fixture Definition… for several fixtures** (or a group) that
  share a definition; greyed with a reason when they don't.
- **ETC ColorSource Spot jr and ColorSource CYC** definitions, and the
  ColorSource Spot's Direct, 1-Channel and RGB modes (5-Channel keeps its
  name); Eos `ColorSource_*_RGB` personalities now match exactly.
- **Palette names follow their value** in the Look Editor (Red → Dark
  Red, Intensity 45% → 60%), only the part that names the value; the edit
  and the rename are one undo step.
- **Twelve more Matrix effects** (Fire, Starfield, Spiral, Lissajous,
  Kaleidoscope, Bouncing Balls, Fireworks, Scanner, Dissolve, Gradient
  Sweep, Box VU, Image with GIFs kept in the show) and an **Each box,
  staggered** scope for every scoped Matrix effect.
- **Cue Runner VC widget**: an External Input tab (an input and a key per
  action, with feedback) and an info-first layout with handles for the
  button height and the GO / Sub GO split.
- **Definition updates** (Fixtures ▸ Definition Updates…) and **File ▸
  Show Definitions…**: older definitions in the user folder or inside a
  show, with a diff, backups and undo.
- **LED Layout…** for a multi-head fixture, saved to the show or as the
  model's default.
- **Controller test** (Quick focus's setup; Settings ▸ Advanced ▸ Game
  controllers), and Quick focus saying when the controller is paused and
  why.

- **One connection window** (`ConnectionEditor`, ui/src/connectioneditor.*).
  Connections ▸ Devices: right-click a universe ▸ **Edit connection…**, or
  double-click it, in the wiring or By universe view. The universe, its
  protocol and interface, then that protocol's own settings in place (Art-Net:
  the node port, or an address typed, or broadcast, and the transmit mode;
  sACN: universe, multicast or unicast, priority, transmit mode; MIDI: output
  mode; WLED: its devices window; a USB widget: the device), then input,
  profile, feedback and passthrough. Patching and editing are the same
  window, the second filled in; OK is one Undo Patch Change step, Cancel
  changes nothing; a taken port and unicast with no address are refused.
  The plugin's own Configure… window opens from it. It replaces the
  protocol / interface / port cascade and the universe row's dozen editing
  entries (Change interface / target, Configure interface, Transmit mode,
  MIDI modes, Art-Net input universe, Passthrough, Feedback to, Input
  profile, the MIDI feedback swap, Change feedback address); "Patch to…" on
  an unpatched universe is Edit connection… too. Classic QLC+'s Universes
  page is unchanged. Devices vs Overview: docs/design/CONNECTIONS_CLEANUP.md.

- **Plot layers follows the plot, and shows what hangs on what** (RC2,
  Branson's testing 2026-10-25). Picking on the plot picks one row per
  object in Plot layers -- where it is, not again under each fixture group
  listing it -- opens only the branches holding them and scrolls to the
  first. A **Selected only** funnel (off by default) shows just the
  selection and what holds it. Rows nest by mount
  (`MonitorLayersPanel::nestByMounts()`): Tower ▸ Hanger ▸ Lantern, Truss ▸
  Bar ▸ Spot, Stand ▸ Truss, Boom ▸ Crossbar, a fixture under its bar,
  hanger, tower, boom or riser; a fixture dropped on a hanger's or bar's
  row, or on a fixture hung there, is hung there. Right-click a layer for
  **New Folder** (named in place), a folder for **New Folder Inside**.
  Tower shelves are not rows of their own.

- **Classic QLC+ says when a show uses the Show Tree** (RC2, board P45).
  A show whose Virtual Console plays Show Tree content -- a **Cue
  Runner**, or a button, Cue List or slider on a function the Show Tree
  made (a cue list, a hit; `VirtualConsole::usesShowTree()`) -- shows, in
  the Classic QLC+ footer, *This show uses the Show Tree. Open the
  qlcconsole view to edit it.* with **Switch to the qlcconsole view**
  (`App::updateShowTreeNotice()`). Never forced; **×** puts it away until
  the show is next opened. The content still plays in Classic QLC+, whose
  tabs stay stock (no Show Tree or Run). The VC widget is called **Cue
  Runner** everywhere (its type name read "Cue runner", the Add menu's
  tooltip "the Show Tree's cue runner").

### Changed

- **Web access listens on this computer only** (RC8, red team H2). `-w`
  used to listen on every network the computer was on; it now listens on
  127.0.0.1 unless started with **--web-bind** (**-wb**) and an address
  (0.0.0.0 for every network). On start it prints the address, and warns
  when it is reachable from the network (`WebAccessBase::defaultBindAddress()`;
  `WebAccess_Test::itListensOnlyOnLoopbackByDefault`, `::itListensWhereItIsTold`).
- **Web access: only this computer, or a user with the level for it,
  changes the show** (RC8, red team H1, H4, H8). With `-w` alone a browser
  on this computer can do everything and one anywhere else can look but
  change nothing (no widgets, Grand Master, channels, Operate, show or
  fixture loading, patching, users, system commands); "this computer" is a
  loopback connection under a loopback name, so a site that resolves to
  127.0.0.1 doesn't count. With `-wa`, every request needs an account with
  the level for it, the Grand Master needs Virtual Console and Simple Desk,
  Operate and the Pi's reboot / halt / network / hotspot / autostart need
  Everything; and until an admin account exists nobody is let in, except
  this computer to create the first admin (it used to make every visitor
  an admin). Usernames are letters, digits and `. _ @ -`
  (`WebAccessBase::authenticateRequest()`, `::permits()`;
  `WebAccess_Test::withoutAuthAProjectCannotBeLoaded`,
  `::withoutAuthThisComputerStillHasFullAccess`,
  `::withoutAuthTheNetworkCanLookButNotTouch`, `::aRebindingNameIsNotThisComputer`,
  `::authWithNoUsersIsNotOpenToEveryone`, `::theFirstVisitorCannotMakeThemselvesAdmin`,
  `::theFirstAdminComesFromThisComputer`, `::aLoggedInGuestCannotBlackOutTheShow`,
  `::systemCommandsNeedAnAdmin`).
- **Pages on other sites can't drive web access** (RC8, red team M1). A
  WebSocket or a POST whose Origin is neither the console's own page nor
  this computer is refused with 403 (cross-site WebSocket hijacking and
  cross-site forms) (`WebAccessBase::originAllowed()`;
  `WebAccess_Test::aPageOnAnotherSiteCannotDriveTheShow`,
  `::aFormOnAnotherSiteCannotLoadAProject`).
- **Scripts run programs only when allowed** (RC8, red team H3). A
  Script's `systemcommand` runs only with **Settings ▸ Advanced ▸ Allow
  scripts to run programs** on (this computer's, off by default); off, the
  line is skipped and the footer says which script asked for which
  program. A show loaded over web access never runs programs, whatever the
  setting (`Script::programsAllowed()`, `Doc::loadedFromWeb()`,
  `Doc::showActionRefused`; `HostileShow_Test::aSystemCommandIsOffUntilTheOperatorTurnsItOn`,
  `::aShowFromWebAccessNeverRunsPrograms`).
- **Previews never run scripts** (RC8, red team H3). A Script started by
  the Workbench (Build, and the Show Tree's docked Workbench, in Design or
  Operate), the Show Tree's preview or Solo -- itself, or as a member of a
  collection or chaser they play -- runs nothing and stops; started from
  the Virtual Console, a cue or the Function Manager it runs as before
  (`FunctionParent::preview()`, `Function::startedOnlyByPreview()`;
  `Workbench_Test::aPreviewNeverRunsAScript`).
- **OSC input listens on its own interface** (RC8, red team M6). An OSC
  universe patched to 127.0.0.1 listened on every network; it now binds
  the interface address it is patched to, so packets sent to a broadcast
  address or to another interface no longer arrive
  (`OSC_Test::theInputListensOnlyOnItsInterface`).
- **Spreadsheet-safe CSV** (RC8, red team M7). The patch list, focus
  report and network report put an apostrophe before a cell that starts
  with `=`, `+`, `-`, `@`, a tab or a carriage return and isn't a number,
  so Excel, Numbers or Lightwright show it as text instead of running it
  as a formula; XLSX cells stay inline text (`CsvField::escape()`;
  `PatchList_Test::aFormulaInANameIsNotRunBySpreadsheets`).
- **The Mac app no longer includes the builder's personal fixtures**
  (RC8 packaging, board Q177). The 0.2.x DMGs folded the build Mac's own
  user folder into the app: 95 personal fixture definitions, 38
  Finder copies like `ADJ-DP-415 2.qxf`, 23 extra RGB scripts (some
  replacing the stock ones of the same name), MIDI and modifier templates
  and a bundle. The app now carries the repository's resources only, and
  the release check fails a DMG with any resource file that is not
  byte-identical to the repository's. Your own fixtures and scripts in
  your user folder load as before.
- **Windows: the uninstaller removes the settings** (RC8 packaging). It
  deleted `HKCU\Software\qlcconsole`, which only the installer writes;
  the app's settings are `HKCU\Software\qlcplus\qlcconsole`, and those go
  now too (and `HKCU\Software\qlcplus` when nothing else is in it).
- **Windows: one qlcconsole per user** (RC8 packaging). Installing into a
  second folder used to take the first install's Apps & features entry,
  file associations and Start Menu over without a word. Now the installer
  asks (install over the existing one, or go back), and a silent install
  (`/S`) into another folder is refused with exit code 3. The in-app
  updater installs into the folder it runs from, so it is not affected.
- **Windows: Apps & features shows a publisher** (RC8 packaging).
- **Windows: LGPL FFmpeg** (RC8 packaging). The installer carried MSYS2's
  FFmpeg, built `--enable-gpl` (GPL 3, with x264/x265). It now carries an
  LGPL build of the same FFmpeg (BtbN's FFmpeg-Builds `lgpl-shared`, pinned
  by release and checksum), as the Mac and Linux builds always had.
- **Licence texts ship with the app** (RC8 packaging): `licenses/` in the
  Windows install folder, `Resources/licenses` in the Mac app,
  `share/qlcconsole/licenses` in the AppImage -- LGPL 2.1 and 3, GPL 2 and
  3, Apache 2.0, and THIRD-PARTY.txt saying which library is under which.
- **Every installer can run headless** (RC8 packaging): each carries Qt's
  offscreen platform plugin, so `qlcconsole -platform offscreen` works as
  shipped (scripted checks, CI, the release checks).
- **A workspace file on its own opens** (RC8 packaging):
  `qlcconsole show.qlcc` opens it like `-o show.qlcc` (an existing .qlcc
  or .qxw only), which is how a Linux file manager passes it. The Linux
  .desktop file runs `qlcconsole %f`, so `./qlcconsole.AppImage --version`
  and `-o FILE` work through the AppImage too.
- **GO holds the previews** (RC8, board Q178). GO, Back and Sub GO --
  from the Run tab, ⌘↩, the Control menu, OSC, MSC, the VC or a pad
  button bound to step a list -- now stop the Show Tree preview, the
  Workbench (Build or docked, Solo too), the Effect designer and the
  editors' Test plays **before** the command acts, so only the run's
  values reach the stage. From the console the line at the top says
  *Stopped the Show Tree preview so GO plays alone — Play brings it
  back*; from OSC, MSC and the VC it happens without a word. While a list
  runs in Design, selecting a cue stays off stage (**■ Held**) until you
  press **Play**, which previews over the run as before. Simple Desk, the
  VC, Focus/Refine and the programmer's flash keep running. Every console
  GO goes through one path (`App::runCommand()`), which the Run tab's GO
  used to call only after the GO
  (`AppViews_Test::goHoldsThePreviewSoOnlyTheRunPlays`,
  `Workbench_Test::goHoldsTheBuildPreview`).
- **A pad's A and B no longer step every running chaser** (RC8). The
  bundled Xbox and PlayStation profiles bound A/B to *step chaser*, so a
  pad picked up mid-show and pressed once moved every running list,
  behind show control's back. They now do nothing outside the focus
  tools; bind them in the profile editor if you want them. A Show Tree
  list stepped by a binding moves only as a GO or Back through show
  control (`FocusMode_Test::aPadPressMovesNoRunningList`,
  `GamepadLayout_Test::noShippedProfileStepsAList`).
- **One throw rule for fixed lights in 2D and 3D** (RC8). The plot's
  light footprint dropped every fixed light straight down; it now uses
  the 3D views' rule (`RigHeads::fixedThrow`): its pan/tilt aim, its
  chosen face, its riser or deck, then down off a truss. A laser faced
  Back throws upstage on the plot too
  (`Monitor_Test::theThrowIsOneRuleIn2DAnd3D`).

- **The Labels dialog finds a Brother PT-P710BT** (RC4, board Q163-1).
  With a printer installed whose name, description or model says PT-P710BT
  (or PT-P710), and no stock chosen before, the dialog opens on the
  Brother preset with that printer and says "Brother PT-P710BT found". A
  stock chosen before still wins; the continuous tape stays the default
  otherwise. (`PatchLabelsDialog::installedPrinters()`, a test seam over
  `QPrinterInfo::availablePrinters()`.)
- **Unpatched fixtures stand out in the patch list** (RC4, board Q163-6).
  The Patch cell of a fixture with no address reads "— not patched",
  italic, black on the house amber (readable on every theme), with a
  tooltip; its label still prints "—" for the address.

- **Hiding the apron moves nothing** (RC3, board Q155-2). Hiding the apron
  is only how it looks: its deck is no longer taken away (which unmounted
  and dropped the lights stood on it), but kept in the model, not drawn on
  the plot, in the Rig Overview or in a beam's way
  (`MonitorProperties::isHiddenApronPlatform`). Its deck is made the first
  time it is shown, never for a show that never showed it, and goes only
  with no depth. The apron has a **Height** (its properties; Auto is the
  main deck's height, as before; 0 is the floor, for an apron at floor
  height in front of a raised stage), saved as `Height` on the apron's
  StagePiece only when set. Saved shows need nothing changed; one saved
  with the apron hidden after it was shown gets its deck back, hidden.

- **Placing on a locked layer asks where** (RC3, board Q159-1). Placing a
  new object (Add ▸ anything, a fixture dropped from the inventory, a
  paste) while the active layer is locked opens a small chooser
  (`Monitor::askPlacementLayer`): the unlocked layers, the first picked as
  before, **Place on this layer**, **Unlock "<layer>" and place here**, or
  **Cancel** (nothing placed). The answer holds for the rest of that
  placement (every fixture of one drop, everything one paste makes) and is
  asked again next time. With every layer locked it offers Unlock or
  Cancel. A fixture put on the plot as a side effect (mounted from a
  structure's menu) still goes on the first unlocked layer without asking.

- **Connections: Overview folded into Devices** (RC3, board Q153,
  docs/design/CONNECTIONS_CLEANUP.md option A). Devices ▸ **By universe** carries
  the Overview grid's columns -- Output, IP, U, Mode, Input, Profile,
  Feedback, PT -- as read-only columns, each shown or hidden from the
  header's right-click or the **Columns** button and remembered
  (`connections/universeColumns`; Mode, Input, Feedback and PT to begin
  with). A double-click on one opens the connection window on that part.
  The Overview sub-tab is gone (`UniversePatchGrid` removed); Connections
  has Devices and QLC+'s own Universes page. What the grid did that
  Devices lacked: **Transmit mode of N selected universes** (Art-Net and
  sACN, one undo step) on the universe menu. The **Fixture Manager's
  universe rows** open the same connection window, filled in with that
  universe (**Patch output…**, or **Edit connection…** on a patched one),
  in place of the old step-by-step questions
  (`UniversePatcher::patchUniverse` removed).

- **Matrix effects cost less a tick** (RC2; the same frames). Bouncing
  Balls, Kaleidoscope, Lissajous and Starfield draw only the pixels a
  point can light and keep what does not change from tick to tick, and the
  Matrix library's `out()`, `point()` and `canvas()` do less per head; an
  effect's intents are read by reference (`EffectInstance::parseIntents`).
  On a Raspberry Pi 5, eight XL-450s, those four went from 12.6-14.0 ms of
  CPU a tick (staggered) to 6.2-7.8 ms; the output is the same frame for
  frame against the old scripts.
- **Locks are per object and per layer again** (RC2). An object's own lock
  (**Lock Position** in its menu, a power source's or node's toggle) holds
  that object and leaves it selectable, with a padlock on its Plot layers
  row; **Lock Layer** holds everything on the layer. Each object's
  `Locked` is its own again, not a copy of its layer's (the 2026-09-25
  "one lock" change); shows save `ItemLocks="1"` on the Monitor element.
  A show without it is put right on load (`MonitorProperties::
  migrateLegacyLocks()`): locks that only repeated a locked layer's are
  dropped, and the **"Locked" layer** the old load made for objects locked
  on their own is taken apart -- its objects keep their lock as their own,
  go back to the layer their folder, mount or fixtures are on, and the
  layer is removed when empty (else left as an ordinary unlocked layer).
- **A new object never lands on a locked layer**
  (`MonitorProperties::placementLayerId()`): with the active layer locked,
  it goes on the first unlocked one. A target placed on a locked active
  layer could not be picked, moved or edited at all.
- **Move to Layer on a structure moves what it carries.** A truss, tower or
  riser shown as its folder goes to the layer with its folder, bars,
  hangers and fixtures; it used to leave its folder, after which its
  fixtures never showed as on it again. Hangers follow their truss or
  tower's layer.

- **Classic QLC+ Functions leaves out the Show Tree's own functions.** The
  layer scenes, cue Collections and cue-list chasers the tree makes, and the
  originals an Import in place keeps for Restore original
  (`ShowTreeController::isLocked()`), are no longer listed in the Classic
  QLC+ Functions tab: they are edited in the Show Tree and only crowded the
  tree. **Show the Show Tree's own functions** (the tree's right-click menu,
  `functionmanager/showShowTreeFunctions`) lists all of them in every view,
  the hidden layer scenes too; off, the other views list what is visible,
  as QLC+ does (`FunctionsTreeWidget::setShowTreeFunctions()`). A bake
  re-files the list. A show with no Show Tree lists exactly what QLC+ does.
  The blue scenes reported as "empty" in 0.3 RC1 are not the tree's: they
  are the user's own scenes of looks on fixture groups (blue = dynamic: no
  channel values of their own), which the scene editor now lists (below).
- **Scene editor: General on top, the sliders below.** General is no longer
  the first tab: it is the top pane (the fixtures used, the channel groups
  used and, for a scene of looks made in Build, a read-only **Fixture groups
  and looks in this scene** list, each scrolling on its own), and the
  fixture and channel-group tabs with their sliders are the bottom pane,
  scrolling sideways for many channels, with a splitter between them
  (`sceneeditor/splitter`; a saved state that isn't a vertical split of two
  shown panes falls back to a third / two thirds). A scene with no fixture
  puts the sliders' pane away. The same editor serves Classic QLC+, the
  Everything view, Shows and Build's sequence editor.
- **Long rows of faders scroll sideways** (`HorizontalPan`): **Shift +
  wheel** or a sideways trackpad swipe scrolls a scene's slider row, over a
  fader too (a plain wheel turn still moves the fader); a drag that starts
  on the space between the faders pans the row, as does a middle-button
  drag anywhere on it.
- **Settings ▸ Appearance ▸ Always show scroll bars** (this computer's,
  `workspace/alwaysShowScrollBars`): every scroll bar is drawn all the
  time instead of macOS's overlay bars that appear only while scrolling (a
  `QProxyStyle` over the app style turning off `SH_ScrollBar_Transient`;
  Fusion off macOS never hid them). `AppUtil::saneStyle()` no longer hands
  out the app's own style, which this setting can replace.
- **Connections opens where you'd look for it**: in Classic QLC+ on its
  **Universes** sub-tab (QLC+'s own Inputs/Outputs page, was "Detailed"),
  in the other views on **Devices**.
- **Copy and paste scenery, towers, booms and stands** in the Lighting
  Studio, as trusses, decks and targets already did: everything they have
  (a wall's door too), offset and named "… copy"; one undo.
- **Paste a truss without its fixtures**: the paste's question has
  **Truss only**.
- **A quieter footer** (board Q88, Q89, Q98): the Engine Load chip is off
  unless you turn it on (Settings ▸ Appearance); the power estimate shows
  only for a show with its power set up; the MTC chip only once a timecode
  source is picked (Connections ▸ Devices ▸ MIDI timecode) or code is heard.
- **Move in Black has one switch** (Q91): the Show Tree's **MIB** button.
  Control ▸ Auto Move in Black and its Settings row are gone; Build's
  **Move in black ▾** still shows and changes it.
- **The Show Tree header** (Q90, Q96): **+ Production**, **+ Sub-cue** and
  **Merge** are under **More ▾** (+ Production is on the bar too while the
  show has none); **Upgrade This Show…** shows only while there are chasers
  to upgrade.
- **Upgrade This Show… says what's upgraded already**: a chaser upgraded
  in place is a greyed row with why (*Upgraded already: cue list …*, the
  same text as Build's Send to Show Tree…) and its own **Restore
  original…**; the Show Tree's own chasers are counted under the table, not
  listed; with nothing left it opens and says *Nothing left to upgrade*
  (File menu; the header button stays hidden).
- **A cue's right-click is shorter** (Q93): no Duplicate cue (Duplicate ⌘D
  is on the same menu), and Make the base cue, Mode and Include cue are on
  the cue's page only. + Production shows on a production or empty space.
- **One + Effect ▾ in the Workbench** (Q94) for an EFX or RGB Matrix, and
  one **Script ▾** in the Look Editor (Q95) for New script… / Edit script….
- **GO in Design offers Operate** (Q92), once a session, in the line at the
  top: GO still plays.
- **No Window ▸ Effects** (Q97): Build ▸ Effects is their home.
- **A group set dropped on a Build scene adds its groups** (board Q86):
  *Towers* onto a scene adds *SL* and *SR*, and the drag says so ("Add
  the groups of “Towers” (SL, SR) to “Base”"). It was refused without a
  word; a scene holds fixture groups, not the Show Tree's sets.
- **Show Tree and Run have their own tab icons**: a cue tree and a green
  GO button, instead of both using the chaser icon.
- **Show control and timecode are rows in Connections ▸ Devices** (board
  Q84): **OSC in**, **MIDI Show Control in** and **MIDI timecode**, each
  with its state ("Port 8000 · listening", "Off"); click one to set it up
  beside the tree. The **Show control & sync** sub-tab, the Settings ▸
  Advanced signpost to it and the empty QLab out box are gone.

- **One word per idea, part 2** (board Q77): the Show Tree's busking
  **Looks** are now **Hits** (+ Hit, the Hits section, Adopt as a hit, Copy
  to a hit, `hit "name"` — `look` still works); a layer's looks are
  unchanged. New sections are named Hits; older shows open as they were.
  The effect designer's **Save as preset…** is gone: a palette keeps the
  effect, **Export…** takes it to another show; existing presets stay in
  the browser.

- **Run is where GO is** (board Q74): the footer's **Next:** is status only
  (no GO/Back/Stop/Load menu of its own); a click brings the Run tab
  forward. ⌘↩ / Control ▸ GO and VC widgets work as before. Build ▸ Scenes
  stays in the qlcconsole view.

- **A smaller qlcconsole view** (board Q79): eight tabs in show order —
  Connections, Fixtures, Lighting Studio, Build, Show Tree, Virtual
  Console, Simple Desk, Run (⇧⌘1–⇧⌘8). Shows and Functions are in
  Everything. The **Rig Overview** opens in its own window, kept in front
  of the main window (it no longer docks: docked, it stopped the main
  window from getting narrower than about 1700 px).
- **One Upgrade door** (board Q76): Upgrade This Show… replaces the Show
  Tree's Import chaser… menu and Convert an old show…; Build's Send to Show
  Tree… opens it with just that chaser ticked (the list is filed and shown
  as before). File menu and the Run tab's empty state open it too.

- **Classic QLC+ looks like QLC+** (board Q78): QLC+'s main toolbar, the
  Fixtures tab's twelve buttons and the Functions tab's new-function
  buttons (icons, as QLC+ draws them), and a footer of the mode, the Grand
  Master and Ready only. The other views are unchanged.

- **Show-mode Lock is part of Operate** (board Q80): no separate Control ▸
  Show-mode Lock or ⌘L. Operate locks programmer faders on the Virtual
  Console and the Shows timeline; a console Grand Master fader now works
  in Operate (the lock used to drop it).

- **Edit ▸ Undo says where it works** (board Q81): it undoes the tab in
  front (Build, the Show Tree, Fixtures and the Lighting Studio -- the last
  two share the rig's history; renaming a fixture in the tree is now a
  step of it too; on Connections, its Undo Patch Change, Q115) and elsewhere
  reads "Nothing to undo on this tab", greyed. It no longer undoes a Show
  Tree step from another tab.

- **One production, nothing to choose** (board Q75). A show with a single
  production doesn't show Current, the Production switcher or "· main"
  (unless it has two lists): GO plays its list. A second production brings
  them back. Lesson 1.1 no longer warns students off the Current button.

- **Level… and Colour… are docked** in the Show Tree (board Q61): they open
  in a strip above the command line, inside the window, instead of a
  separate window that could fall behind the main one. Apply / Return sets
  the value, Cancel / Esc leaves it.
- **Help pictures never show this machine's network**: tools/help-shots.sh loads
  only the Loopback plugin (new QLC_IOPLUGIN_DIR override).

- **"Classic" means stock QLC+ only.** Build ▸ Scenes has no Classic
  arrangement any more (the Folders | Classic switch is gone; Scenes lists
  your scene folders). **Window ▸ Functions** opens the stock Functions tab,
  switching to Everything (with a way back) from the qlcconsole view; a show
  saved with Functions in front no longer switches view on open. Build ▸
  Effects' "Classic effects" heading reads "QLC+ effects (EFX, RGB Matrix)".
- **One word per idea** (Q70; labels only — saved shows, settings and
  VC buttons are unchanged). *Stage Look* is now **Plot Symbol**
  (Fixtures right-click, Fixture Properties, the plot's **Show ▸ Show
  Auto fixtures as their closest plot symbol**); the Lighting Studio's
  *Layers* panel is **Plot layers**; *Perform (Operate)* is **Operate**
  everywhere (Settings ▸ Show Tree); *Hold Last Look* / *Clear Held Last
  Look* are **Hold at End** / **Clear Hold at End** (Control menu, a
  Shows track's right-click, the VC button action). Help, the course
  and the help pictures use the new names.
- **File ▸ Import is a submenu** of source types — *From a show file*:
  QLC+ / qlcconsole Workspace, ETC Eos Show; *Fixtures & patch*: QLC+
  Fixtures List — each with its own file filters, then **Any Supported
  File…** (the old Import).
- **Esc backs out** of in-place editors one level: a palette over Build's
  rows (**↩ Back**), a group's layout, the effect bench (asking first if
  the effect isn't kept), a ▣ opened layer, the docked Workbench, the Show
  Tree's Look Editor. Edits stay; renames keep Esc; plain Esc is no
  playback action (Stop all is ⌘⇧Esc).
- **The Show Tree's command line folds its syntax away.** The line stays;
  the three lines of syntax under it show on the **?** button beside it,
  **?** typed first, or **F1** in the line, and the choice is remembered.
  An empty show's page just says how to start.
- **The Run tab with no main cue list** (a QLC+ 4 show) says *This show
  has no cue list yet. Upgrade turns its chasers into one.* with an
  **Upgrade This Show…** button, and GO and the other buttons are grey
  instead of a live-looking green.
- **The Build tab wears a hard-hat icon.**
- **Fixtures tab:** Plot Symbol ▸ is offered in Operate; the toolbar's
  Properties opens Fixture Properties for several fixtures.
- **A dimmer's Auto plot symbol is Ellipsoidal / profile spot** (a lantern
  is a practical now).
- **The boot window shows the version**, build and commit.
- **The Lighting Studio plot draws a pixel fixture's heads where its LED
  layout puts them.**
- **Tab shortcuts follow the view.** ⇧⌘1, ⇧⌘2… number the tabs the
  current workspace view shows, in its order (⇧⌘0 the tenth), and
  renumber when the view changes; the Window menu lists them so, then the
  hidden tabs with no shortcut. In Classic QLC+, Fixtures is ⇧⌘1; in
  qlcconsole the Show Tree is ⇧⌘5 and Run ⇧⌘8 (they were ⇧⌘6 and ⇧⌘0).
- **Settings ▸ Appearance.** Theme, toolbar style, the four footer chips
  (engine load, power estimate, grand master, speed masters) and the speed
  masters' beat moved out of the View menu into a new Appearance tab in
  Settings, applied on OK. The same settings, so earlier choices carry
  over. View now holds Workspace, Address Tool, Rig Overview and Focus
  Mode.
- **One switch each.** The workspace view is set only in View ▸ Workspace
  (Settings ▸ General no longer has a copy), and the toolbar style only in
  Settings ▸ Appearance.
- **No outputs: preview only.** With nothing patched to any output, the
  footer's readiness chip reads *No outputs: preview only* in grey (with
  *· N to check* when something else is listed; a click opens Connections
  or the list) instead of a red *Not ready*. Red is kept for real problems
  on a rig with outputs. The course no longer tells students to ignore red.
- **Auto-open shutters is off for a QLC+ 4.x show.** A show saved
  without the setting loads with it off unless qlcconsole saved it, so an
  old show sends the same DMX here as in QLC+. New shows keep it on, and a
  show now always saves the setting, on or off.
- **Settings, tooltip and menu text** name the paths the help uses:
  Settings ▸ Advanced ▸ Output in the Output Rules dialog, Build → Move in
  black in the move-in-black tooltips, *Fade 10 / 30 seconds and stop*.

### Fixed

- **One OSC packet could hang or crash the console** (RC8, red team H9,
  H10, M5). A bundle element size of 0xFFFFFFFC stepped the parser back
  and forth forever on the GUI thread; a huge size, a size cut short, type
  tags or a string with no end read past the datagram; a float of NaN,
  infinity or out of 0..1 became any byte. `OSCPacketizer::parsePacket()`
  now checks every length against the datagram as an unsigned size, ends
  a bundle on an element that doesn't fit, nests bundles eight deep at
  most, and clamps values to 0..255; the multi-value path cache and the
  path hash table keep 4096 paths at most (`OSC_Test`: 14 tests, 20000
  seeded random datagrams).
- **One short web command could crash the console** (RC8, red team H5,
  H6, L1, L2). `GM_VALUE` alone, `CH|1`, the cue list, frame, clock,
  matrix, IO and user commands read arguments that weren't there; an
  unknown widget id in `getWidgetType`/`getWidgetSubIdList` dereferenced
  null; a URL the parser can't read asserted; a body of any
  Content-Length was buffered (now 413 over 64 MB); and quitting with a
  browser connected deleted each connection twice. Each is checked
  (`WebAccess_Test::shortCommandsDoNotCrash`, `::unknownWidgetIdsDoNotCrash`,
  `::aMalformedUrlDoesNotAbort`, `::aHugeUploadIsRefusedEarly`,
  `::shutdownWithALiveClientIsClean`).
- **A fixture upload could write outside the fixture folder** (RC8, red
  team H7). The name the browser sent went straight into the path
  (`../escaped.qxf`); it is now the file name only, a `.qxf`, or refused.
  Files outside the web folder are served only when the show's Virtual
  Console uses them as a picture, and `..` never leaves it
  (`WebAccessBase::sanitizedFixtureFileName()`;
  `WebAccess_Test::anUploadedFixtureStaysInTheFixtureFolder`,
  `::withoutAuthAFixtureCannotBeStored`, `::uploadNamesAreSanitised`,
  `::filesOutsideTheWebFolderAreNotServed`).
- **Names in the web pages could run as script** (RC8, red team M2). Button,
  slider, label, frame, page, audio trigger and matrix captions, cue list
  function names and notes, universe, profile, device, fixture-file and
  user names were written into the pages as HTML; they are escaped, and
  the page's own updates set text, not HTML
  (`WebAccess_Test::namesFromTheShowAreEscapedInThePage`).
- **Two script lines could crash the timer** (RC8, red team M3, M4).
  `random(1,0)` divided by zero (a crash on x86); the bounds are taken
  either way round. `setfixture` on an unpatched fixture, or one whose
  universe the show doesn't have, wrote past the universe list; it is
  refused (`HostileShow_Test::randomWithEqualBoundsDoesNotCrashTheTimer`,
  `::randomTakesItsBoundsEitherWayRound`, `::setFixtureOnAnUnpatchedFixtureIsSafe`).
- **The Linux AppImage runs without Qt installed** (RC8 packaging). It
  carried a hand-kept list of Qt libraries that missed PrintSupport, the
  Qml/Quick libraries the multimedia plugin needs and FFmpeg's stubs, and
  took libftdi1, FFTW, libmad and FFmpeg itself (a GPL build) from
  whatever machine ran it. It now carries every library it needs beyond a
  desktop's base system (`platforms/linux/appimage-bundle.sh`), with
  `$ORIGIN`-relative library paths, Qt's LGPL FFmpeg, and the JPEG, GIF,
  ICO and SVG-icon plugins; checked on clean Ubuntu 22.04 and 24.04.
- **`./qlcconsole.AppImage --version` started the app** (RC8 packaging):
  the .desktop file's `Exec=qlcconsole --open %f` put `--open` before
  every argument.
- **Finder knew .qlcc, .qxw and .qxf only as "document"** (RC8
  packaging): the Mac app now declares them (`UTExportedTypeDeclarations`
  for .qlcc, `UTImportedTypeDeclarations` for QLC+'s .qxw and .qxf, all
  XML), so they get a kind and an icon of their own.
- **A drag on a locked plot could throw a deck away and save it** (RC8).
  With the plot locked, dragging a riser in a tight deck left it in place
  while the pointer moved, then stored it on release: snapped to the grid,
  pushed clear of its neighbour, it landed decks away and the show was
  marked changed (in Front, five metres downstage and off its riser). A
  drag of anything held still (the plot's lock, its layer's, its own)
  now stores nothing and says why on the status line: **Plot locked —
  Edit Plot to move**, **Layer locked — unlock it in Layers to move**
  (`Monitor_Test::aHeldObjectIsNeverMovedByADrag`, every structure kind,
  Top and Front, plot and layer locks).
- **A locked plot moves nothing, aim targets included** (RC8, board Q183).
  Targets used to stay draggable with the plot locked, so a mis-click on a
  target standing on a riser moved it. **Targets stay adjustable**, beside
  the lock while the plot is locked, lets them be dragged anyway; it is
  your setting (kept on this computer, not in the show) and off to start
  with.
- **Plot layers selected one row on the plot** (RC8). Cmd/Ctrl- or
  Shift-clicking several rows selected them all in the tree but only the
  last on the plot; a stage piece or a hanger in a multi-selection wasn't
  selected at all. Every selected row is now selected on the plot
  (`Monitor_Test::theLayersTreeSelectsEveryRowOnThePlot`).
- **A layer-locked object couldn't be selected even from Plot layers**
  (RC8, board Q178). It is now selectable from the tree (to read its
  properties) and click-through on the plot: a click, a rubber band or
  Select All reaches what is under it. It still can't be moved or deleted.
- **The rulers' band sat beside the item** (RC8). On a target it was
  offset by a foot and three and a half feet too wide (it included the
  room kept for the warning text), on a fixture its label and facing
  arrow; it went stale after a zoom, a scroll or a selection made without
  the mouse; the rulers were unreadable with the plot turned 90°; and in
  metres the teal centre line was 0.12 m off the readouts' centre. The
  band now hugs the object, follows the view, and every overlay works
  from one unrounded mapping (`PlotAlignment_Test`, 62 checks at 1x and
  2x pixel ratios).
- **The rulers count from the centre line** (RC8, board Q179): 0 on the
  centre line, marked **CL**, numbers out to each side with **SR | SL** in
  the corner, and the footer reads **SR 3.20** / **SL 1.50**, the same
  zero as the target readouts. A ⌖ origin you set still wins.
- **Two targets' readouts printed on top of each other** (RC8, board
  Q179). The second now sits under the first; a line target's readout
  shows only while the line is selected
  (`Monitor_Test::targetReadoutsStackAndALineShowsItsWhenSelected`).
- **Align and Distribute lined up corners and dropped lights to the
  floor** (RC8, board Q179). They lined up the fixtures' top-left corners,
  so different-sized fixtures didn't line up, and wrote a height of 0.
  They now line up centres (Same X/Y) or edges (Left/Right/Upstage/
  Downstage), space centres evenly, keep each height, and one Undo puts
  them back (`Monitor_Test::alignAndDistributeUseCentresAndKeepHeight`).
- **A free or deck fixture dragged in a structure editor landed half a
  fixture away and crept** (RC8, board Q179). The editor stored the
  fixture's centre where the plot keeps its corner: in Top it landed half
  its size off the pointer, and in Front and Side it crept along the
  hidden axis on every mouse move. It also jumped its centre to the
  pointer instead of keeping the spot you grabbed. Both fixed
  (`Monitor_Test::aStudioDragKeepsTheCornerAndTheGrab`).
- **The window's contents could stop following its size** (RC8, board
  Q178). After a tab was moved into its own window, the main window could
  grow on a Mac while its contents stayed at the old size. When the
  window and its contents disagree, the app now re-syncs and redraws them,
  and logs it; **Help ▸ Troubleshooting ▸ Copy Layout Report** copies
  every level's size for a bug report
  (`ViewLayout_Test::theLayoutReportAndTheSelfHeal`,
  `::theWindowFollowsResizesAroundADetach`).
- **The main window follows every resize again** (RC8, u9). It could get
  stuck after a tab was moved to its own window: the window grew or shrank
  but the tabs and the status bar stayed at their old size. The cause was
  moving the Rig Overview by its title bar. Qt took that move as a drag
  towards the main window's dock areas. The panel can dock nowhere, so the
  drag ended without being put back, and the main window's layout kept the
  state it had saved for the drag. In that state Qt ignores every resize
  until the app quits. The Rig Overview now moves as a plain window. The
  self-heal also checks that the main window's layout follows: if it is
  stuck, the heal ends a drag that is still holding the mouse, restores
  the layout's own state and lays it out again, and logs it. **Copy
  Layout Report** says whether the layout follows
  (`ViewLayout_Test::aRigOverviewMoveLeavesTheWindowFollowing`,
  `::aStuckMainLayoutIsHealedOnTheNextResize`).
- **A laser showed nothing on the 2D plot** (RC8, board Q178). The Top
  view never drew a laser's beams or a sky laser's stars. With **Show ▸
  Laser & star output** (on by default) every lit laser and star laser
  now shows where its light lands -- on the floor, a deck or the cyc's
  line -- in its live colour and level, about 15 times a second, turning
  with its prism rotation; lasers draw pencil lines out to their landings
  (`Monitor_Test::starFieldShowsOnTheTopPlot`).
- **A show's older copy of a definition was used without a word** (RC8,
  board Q178). When a show carries its own copy of a fixture definition
  and this computer has a different one, the show's stays in use (as
  before), but now the fixture's tooltip on the plot says so, and the line
  at the top says it once as the show opens, with **Review…** for **Use
  This Computer's**. Nothing switches by itself
  (`ShowDifferences_Test::anOlderEmbeddedDefinitionIsSaidOnThePlot`).
- **A line target's ring sat off its dot** (RC8, board Q178). A line
  target drew a point target's ring and crosshair at its 3-D half-way
  point, a few pixels off the mid handle, which read as a misplaced
  target. It now draws a small diamond on the curve at its centre, hidden
  when a handle sits on it; selected and locked are drawn along the curve,
  and its handles always grab before the centre
  (`Monitor_Test::aLineTargetHasNoRingOffItsCurve`).
- **The Show Tree's page came back at no width** (RC8). A split saved
  while the Workbench was docked there (four panes, the page's at 0) was
  restored on the next start; it is now dropped, and a split moved while
  docked isn't saved
  (`ShowTreeView_Test::aSplitSavedWhileDockedDoesNotHideThePage`).

- **A chaser step's Fade or Hold typed with a comma read as 0** (RC7).
  "1,5" in the Chaser editor's Fade In / Hold / Fade Out cells or the
  Workbench walker's step header set the time to nothing.
  `Function::stringToSpeed()` now reads ',' as '.', and "1.5s" / "1,5s"
  as one and a half seconds, so the audio editor and the Virtual
  Console's time fields take either mark too. Tests in de_DE, en_US and C
  (`Function_Test::stringToSpeedTakesEitherDecimalMark`,
  `WorkbenchChaser_Test::stepTimesTakeEitherDecimalMark`).
- **Collect for Transfer left the pictures behind** (RC7). The folder had
  no images/, and the collected show named the pictures where they were
  (the original folder's images/, or ~/Downloads): on another computer
  truss socks, tower faces and plot images were gone. Every picture the
  show uses (truss socks, tower faces, pictures on the plot, RGB matrix
  images, Virtual Console backgrounds and button icons) is copied into the
  folder (`ShowEmbedding::transferPictures()`), and the copy of the show
  names them there, relatively (`Doc::setComponentPathOverrides()`); the
  open show keeps its own paths. A picture placed on the plot (Add Image)
  is now saved relative to the show and kept in its images/ like truss
  and tower pictures. Gobo swaps stay names in the computer's gobo
  library. (`ShowFiles_Test::collectForTransferTakesThePictures`, written
  first and failing, `ShowEmbedding_Test::transferPicturesHaveAPlaceEach`.)
- **Set Mounting Height… asked in metres on a feet show** (RC7). The
  fixture's right-click item and its dialog now use the show's units, like
  the plot's other dialogs (`MonitorFixtureItem::askMountingHeight()`;
  `MonitorFixtureItem_Test::mountingHeightIsInTheShowsUnits`).
- **Two labels** (RC7). Settings ▸ Autosave said ".qxw.autosave" for
  every show: it is *Show*.qlcc.autosave (or .qxw.autosave) beside a saved
  show and untitled.qxw.autosave for one not saved yet. The Patch List…
  tooltip named CSV and PDF only; it lists copy, CSV, XLSX, PDF and
  fixture labels.
- **The help, brought up to date** (RC7). An audit of 129 features against
  the code: 79 outdated, partial or missing rows fixed across
  twelve guides; the three hand-taken pictures (show-tree-workbench,
  build-effects-menu, make-a-layer) showed removed buttons and are now
  HelpShots entries, regenerated by tools/help-shots.sh.
- **A fixture with more channels than a universe holds loads unpatched**
  (RC6, test matrix 2 fuzzing: N1). A damaged or hand-edited show with,
  say, `<Channels>926</Channels>` on a fixture kept all 926 channels with
  only its address reset, and opening it wrote past the universe's
  512-channel buffers (an unchecked heap write in the release build). The
  fixture now loads unpatched and the error log says why, as for a
  universe out of range; the address check counts the mode's own
  channels; and a universe ignores a channel default past its end, as it
  already did a channel's role and modifier. Also in upstream QLC+.
- **A fixture definition with a huge head channel number no longer reads
  past its mode** (RC6, test matrix 2 fuzzing: N2). A head `<Channel>` of
  2^31 or more (from a user fixture, an imported .qxf or a definition
  carried in a .qlcc) passed a signed bounds check and was read as a
  channel, a crash. It is skipped as an undefined channel, here and in
  the power, definition-check and stage-view code that reads head
  channels. Also in upstream QLC+.
- **Connections ▸ Universes lists the open show's universes, no more**
  (RC6, test matrix 2: M1). Each show opened added its universes to the
  list without removing the last show's (78, then 156, then 234 rows),
  and a rename went by row number, so a stale row renamed another
  universe. The list now follows the show's universes exactly, and
  rename, passthrough, the slot and Delete act on the row's own universe.
- **New tests for saved data the mutation run found unguarded** (RC6, test
  matrix 2 mutation gaps 1-3). With a venue turned off or the show
  replaced, no fixture id stays the venue's (a show fixture holding a
  former house id is saved with the show); a Show Tree of only a
  production, nothing baked yet, is saved and reopens; a not-ours range
  in the middle of a universe reads back with the same end. All three
  already behaved: the tests keep them so.
- **The Focus screen keeps its layout while you work** (RC5). Pressing the
  controller's buttons in Quick focus or Venue focus made the panel jump:
  every label that gained or lost a line, or appeared (the beam, the
  change since the save, what was just confirmed), resized the controller
  drawing under it, and a long fixture or device name widened the window.
  Each line now keeps a fixed room whatever it says (cut text is in its
  tooltip), the warnings under a fixture's name have a line of their own,
  the pan and tilt digits are all one width, and the "Use the controller"
  button keeps its place. Focus this cue's panel does the same. Switching
  between the controller and keyboard maps still resizes, as before.
- **The voice is never lost when its output is gone** (RC5). With "Voice
  to:" set to an output that isn't there (a controller unplugged, asleep
  or on Bluetooth), every spoken line, "Controller lost. Lights held."
  included, failed silently. The voice now speaks on the computer's own
  output instead, says once why, and goes back to the chosen output when
  it returns. The output is kept by ID as well as name, so two pads both
  called "Wireless Controller" are told apart.
- **Fixture Properties' orientation diagram shows where a fixed light
  throws** (RC5). It knew only the face's plane: Back drew downstage,
  Bottom drew like Top, and Stage left drew stage right; tilt and a
  Pan/Tilt aim were ignored. It now follows the 3D view's rule (the face's
  side, its tilt, or the aim), draws a light pointing straight down or up
  as a filled dot or a ring, and follows the aim as it is edited.
- **An RGB matrix whose script isn't installed keeps it** (RC5, test
  matrix C1). Its algorithm was dropped on the first save and the show
  reopened as Stripes, without a word (CORP-DEV-1.0's Plasma (Colors)
  matrices). The `<Algorithm>` is now kept as it was and written back
  unchanged, and opening the show says which matrix uses a script this
  computer doesn't have.
- **Save As to another folder keeps the pictures with the new show** (RC5,
  test matrix C3). A truss or tower picture was copied into the old show's
  images/ folder and the new file named images/… where there was none,
  for any save that hadn't set the show's folder first. The save now
  makes the target's folder the show's folder while it writes (and puts it
  back if the save fails), and a folder whose name merely starts the
  same ("Show 2" and "Show") is no longer taken for the show's own.
- **Thread races found by ThreadSanitizer** (RC5, test matrix 2: M1, M2,
  L1, L3). Stop-all could return while a function waited to start, which
  then ran on for File ▸ New or Open to delete under the timer (a rare
  crash; upstream's fix 7e2f4ae84 included). A chaser's runner was
  parented to the chaser from the timer thread. A function's run-state
  flags and Blind are now atomic, and a stop that keeps the function's
  attributes is seen with that choice.
- **The show-file tests fit CI Windows' five minutes** (RC5): each show is
  opened fewer times for the same checks, and Fixture Properties for every
  fixture is its own test program.
- **On Windows, Build and the Show Tree follow the show again** (RC4, test
  matrix W1–W3). The Build outline, Build ▸ Effects and the Show Tree
  Library never refreshed when a function, group, fixture or palette
  changed: their signals were connected from a braced list, which MinGW
  fills with DLL import thunks that connect nothing. Each now has its own
  connect. Also on Windows: a drape driven by a curtain fixture moves, the
  group editor follows its group, and the Controller Test lists its
  inputs (Fixture and FixtureGroup are now exported from the engine DLL,
  and plugin signals are connected by name). The Windows CI test step,
  which had been failing 16 of 190 programs unnoticed, now fails the
  build, and a new source check catches the pattern on every platform.
- **A new or opened show no longer reads freed stage structures** (RC4,
  test matrix M1). File ▸ New or Open deletes every truss, riser, pipe,
  stand, tower, scenery piece and target, and only then rebuilt the rig
  view, whose items still pointed at them: a heap-use-after-free on every
  show change with any structure on the plot, silent in a normal build but
  a possible crash. Deleting one structure (Delete, Undo of a paste) did
  the same to its own item. The model now says when a structure is about
  to go, and the rig view drops the items that borrow it first; a new show
  drops them all when it starts to clear.
- **A mounted fixture's rebuilt plot item no longer starts hidden or
  inverted** (RC4, test matrix SA1). The item rebuilt from a fixture's
  mount left its fixed zoom and flags uninitialised, and shows saved the
  junk (`FixedZoom="1804114660"` with Hidden / InvertedPan / InvertedTilt
  in `stage-structures-demo.qxw` and the course shows). They now start at
  zero, and on load an impossible fixed zoom is dropped with the flags
  saved beside it.
- **The studio's ray cache compares its keys by value** (RC4, test matrix
  SA4): -0.0 and +0.0 are one key, and so is every NaN, where a byte
  compare told them apart.
- **A malformed Art-Net or sACN packet is dropped, not read past** (RC4,
  test matrix 2, fuzzing). Every Art-Net receive path trusted the lengths
  inside the packet: an ArtDmx of 12–17 bytes, an ArtTodData whose UID
  count was more than it carried, and an ArtRdm whose message or
  parameter-data length ran past its end all read beyond the datagram
  (memory any host on the show network could reach). Each is now checked
  before it is read and a packet that doesn't add up is dropped. An
  ArtDmx or sACN packet is held to 512 channels (a longer one overran the
  512-byte input buffer), a sACN packet one byte short of its start code
  is dropped, and an Art-Net universe or RDM address with its top bit set
  is no longer sign-extended. An RDM DEVICE_INFO reply's sensor count is
  read from its own byte; it was the checksum.
- **A chaser or sequence step with a bad number loads** (RC4, test matrix
  2, fuzzing). A `<Step>` without `Number`, or with one negative or past
  the end, was inserted at that index: an abort in a Debug build and
  memory corruption in Release. It is now appended.
- **Undefined behaviour found by UBSan** (RC4, test matrix 2): a channel
  modifier above 127 went from float straight to char, and a new
  Virtual Console slider compared its mode against `SliderMode(-1)`. Both
  are well defined now, with the same results.
- **The fader race test can't starve** (RC4, test matrix 2):
  `GenericFader_Test::aChannelBeingSetUpStaysPutWhileTheUniverseWrites`
  hung to the 300 s watchdog under the Linux coverage build. Its writer
  thread re-took the fader's lock back to back, and a mutex isn't fair;
  the app's universe thread writes once a tick. The writer now lets the
  setter in between writes.
- **A damaged show file says so, and isn't saved over** (RC4, test matrix
  F2). A show cut short or not well formed opened silently as a smaller
  show, and a Save then replaced the original with less. It now opens as
  far as it can be read with *This show file is damaged or incomplete
  (line N)*, the footer keeps the message, and Save goes to Save As
  (offering *show (recovered)*) instead of writing over it. The engine's
  load returns false and logs the line.
- **A fixture on an impossible universe no longer freezes the load** (RC4,
  test matrix F1). `<Universe>65536</Universe>` added 65537 universes one
  at a time, each rebuilding the Show Control page's lists (minutes), and
  `-1` ended in an abort. A show now has at most 1024 universes; a fixture
  past that, negative or not a number loads unpatched and the footer says
  so. The Show Control page rebuilds once for a run of new universes.
  A show reaching the last universes no longer aborts on a Mac either:
  each universe's thread takes file handles, and the app now raises the
  open-file limit (256 by default) as Qt 6.10 and later do themselves.
- **Opening a show no longer marks it modified** (RC4, test matrix F3).
  Building the plot groups each truss with the fixtures rigged on it; a
  show saved without those groups (Office.qlcc, help-demo.qxw) asked
  "Save changes?" on close and wrote an autosave at open. The groups are
  derived the same at every open, so they no longer count as an edit.
- **Times take either decimal mark** (RC4, test matrix L1). In German,
  "1.5" typed into a fade was 15 s (the "." read as a thousands mark).
  Every fade and time field now reads "." and "," both as the decimal
  point, and shows the language's own.
- **The units setting lives with every other setting** (RC4, test matrix
  SA3). On macOS it was kept in `com.qlcplus.qlcconsole` instead of
  `org.qlcplus.qlcconsole`, so settings resets and exports missed it; an
  existing value is moved over once. The fixture editor's toolbar style
  now reads the app's real settings too.
- **Uninitialised members** (RC4, test matrix M2, M3, SA2): the Show Tree
  library filter and a VC button's state and action are set before the
  constructor reads them, and a show-embedding check holds the parameter
  list it points into.
- **An Art-Net node's RDM flag no longer flickers** (RC4, board Q165-a).
  The office CR041R fills its ArtPollReply Status1 from an uncleared
  buffer (0x00 / 0x74 / 0x6F / 0xA9 from reply to reply, the RDM bit set
  in about a quarter of them), so its Devices row flipped between "RDM"
  and "no RDM". The bit is now trusted only once the last five replies
  agree, with the same Status1; a node whose Status1 changes four or more
  times in sixteen replies is junk and reads "no RDM" until it has held
  for 64 replies; an ArtTodData reply proves RDM whatever Status1 says
  (`ArtNetRdmFlag`). The CR041R steadily reads "no RDM"; OLA (0xD2) reads
  "RDM".
- **The connection window's MIDI input channel was one off** (RC3). It
  stored channel N as N and "All channels" as 0, where the MIDI plugin
  reads 0-15 as channels 1-16 and 16 as every channel: "Channel 1" listened
  on channel 2, "All channels" on channel 1 only, and an input with no
  channel set opened as channel 1. It now reads and writes the plugin's
  numbers (the Overview grid had them right).

- **Line targets keep their colour, edit, and snap.** A target is drawn in
  the colour set in Edit Target whether locked or not (a red ring says
  locked; it used to turn wholly red). Targets follow their layer's lock in
  the Front and Side views too, and a dragged target or line handle lands on
  the grid with Snap on (one undo step). The release after a handle drag no
  longer re-snaps the line's centre.
- **The scroll bar no longer covers the layer buttons.** With overlay
  scroll bars (macOS) Plot layers stops a bar's width short of its edge, so
  the bar never lies over a layer's lock.
- **Hangers are picked from Plot layers** (and in a multi-pick).
- **A structure leaving the old "Locked" layer takes its folder** (RC2):
  a truss, tower, riser or boom shown as its folder goes back with the
  folder and everything in it, not alone (`migrateLegacyLocks()`).
- **Matrix Lissajous lights heads off its grid** (RC2). A head past the
  columns or rows its area's grid claims threw in the script ("Cannot
  read property 'r' of undefined") and blanked the whole tick; it now gets
  its own point's level and colour, as Bouncing Balls does.

- **Update All and the show definitions' results are a window** (`BulkResultDialog`),
  not an alert that grew off the screen with nothing to scroll and no Close.
  Definition Updates' Update All / Update from Current and Undo Last Update,
  and Show Definitions' Use This Computer's / Install the Show's: a summary
  ("Updated 14, unchanged 30, 2 errors"), the updated list (maker and model,
  file, old → new version, the show's fixtures using it and what changed) and
  the errors, each scrolling; sized to the screen, resizable, Close, Copy and
  Save as Text. The questions before an update list the files the same way.
- **Renaming a venue with a practice from it** (Branson's testing): the
  practice's way back now follows the new name, so ending it puts the
  renamed venue back on the rig; its corrections, trims, targets and
  house rig always stayed with it (one name inside the one sidecar).
- **Fixture Properties opens for every fixture.** A double-click or
  Fixture Properties… in the Fixture Manager did nothing for a fixture not
  on the plot (a staged one, another venue's house light, one never placed):
  the dialog was built around its plot item and returned without one. It
  opens now, its X, Y and icon rotation greyed with "Not on the plot";
  several selected edit together whether placed or not.
- **A double-click on a universe in Connections opens its connection**
  instead of starting a rename (a click on the selected row, or F2, still
  renames).
- **A controller asleep is never a silent failure** (RC2: an Xbox pad's
  auto-off, then Quick focus started: no message, and the woken pad drove
  nothing). Quick focus started with no controller says so on the panel and
  aloud ("No controller connected. Using the keyboard.", important, so in
  every Feedback setting; said before the first where line, on the new
  `FocusSession::starting()`); a pad that connects during a tool says
  "Controller back." whether it was lost mid-session or never there; and
  Quick focus takes the woken pad over by itself (`FocusModePanel::
  takeOverIfFree()`) unless the keys are nudging (then once they are still
  a second) or the keyboard was chosen (then the panel and voice say to
  click **Use the controller**). The setup dialog no longer remembers
  "Keyboard" when it was the only entry (no pad connected) -- that made
  every later session start on the keyboard with the pad ignored; the
  setting is read from a new key (`focusmode/inputDevice`), so a value left
  by that bug is not. The HID plugin rescans once more 2.5 s after any
  device or GameController-framework connect/disconnect, checks every 3 s
  for a device whose system entry is gone (`isStale()`), and drops a
  same-path device it cannot reopen instead of keeping it listed, patched
  and dead. Tests: `focusmode_test` (asleep at the start, woken mid-move,
  keys nudging, keyboard chosen), `focuscalls_test`, `inputdevicenotices_test`,
  and a new macOS `plugins/hid/test` (`hidgamecontroller_test`: a
  framework-read Xbox or DualSense pad disconnecting and reconnecting).
- **Re-patching an input left with no plugin no longer crashes.** An input
  unpatched by name with a valid line kept an `InputPatch` with no plugin,
  and patching it again dereferenced that null plugin.
- **An upgraded show's originals no longer say "Made by the Show Tree".**
  The chaser Import in place keeps for Restore original, and the scenes and
  collections only it plays that its cue list doesn't play as they are --
  wherever they are filed, so an Eos show's *Eos/Preshow* scenes too, and a
  scene Capture into palettes took out of the list -- open view only (Q141)
  under *Original from the import, kept for Restore original*, naming the
  cue to edit (*The show plays the Show Tree's cue list Haunted Variety
  Show; edit cue 6.1 Backout there*), with **Show in Show Tree** going to
  that cue. Before, they opened editable with nothing said, while the
  list's own chaser beside them, under the same name, opened locked as the
  tree's; editing an original changed nothing that plays. Generated scenes
  and collections keep "Made by the Show Tree — edit it there"; a scene
  the list plays as it is (a ▣ layer), or that something else plays too,
  stays the user's to edit. `ShowTreeController::originalFor()`,
  `originalWhere()`, `lockedTextFor()`, `isLocked()`, used by the
  Functions tab (editor and tooltip), Build's Workbench and Send to Show
  Tree.
- **Classic QLC+ Functions tab: the editor beside the tree, as in QLC+.**
  Picking one function in the tree opens its editor in the right pane
  (`FunctionManager::setOpenOnSelect()`, on in Classic QLC+ only; the other
  views keep double-click, so a click can start a drag into the open
  collection or chaser). A saved `functionmanager/splitter` state that had
  collapsed the editor (or the tree) no longer opens every editor as a
  strip the width of its minimum: `showEditorPane()` puts a pane under a
  fifth of the tab back to half and half. A scene or collection the Show
  Tree made (an Eos import's cue scenes are) opens in its editor, greyed,
  under the "Made by the Show Tree" banner, instead of the banner alone at
  the foot of an empty pane -- together these were "not using the entire
  screen" and "clicking on a scene doesn't open anything". The row being
  edited is a tint of the selection colour with the theme's text colour
  that reads on it (it was the selection colour lightened, under the row's
  own colour: orange on light blue in the dark themes). New
  `ui/test/viewlayout`: every tab of every view at 1280x800, 1920x1080 and
  2560x1440 fills the window (no gap at the right or bottom, no visible
  splitter pane at no width, no pane holding only a strip), and Classic's
  Functions tab opens a scene, a chaser and an EFX from a collapsed saved
  state.
- **Run tab: the state column fits "▷ STANDBY"** (Q115). It was cut to
  "▷ STA…" at the Run tab's large type; it now fits its longest word, bold,
  on a sub-cue's row.
- **A fixture group's row in Plot layers keeps its fixtures selected** on
  the plot: they lit and went out again a moment later.
- **The Show Tree's flags in columns** (board Q104): **S** (Sets), **T**
  (Tracks in) and **R** (Releases) — a count on a cue, a dot on a layer —
  **M** (Mode: B, C) and **F** (Flags: base, skip, no MIB, includes,
  reusable, mix, VC), each header naming itself on hover, instead of one
  column of symbols. Right-click the header to hide any; in a narrow pane
  they fold away before the names do.
- **Shorter layer names in the Show Tree**: a layer on many fixtures names
  the fixture groups they make up ("FOH + 1st Elect") and the rest as the
  first few and a count, instead of every fixture ("Next cue receives",
  the tree, menus).
- **Fixture Properties edits every selected fixture**: double-clicking one
  of several selected fixtures, on the plot or in Plot layers, opens the
  editor for all of them (fixtures in a group or on a truss opened alone
  before; a double-click in the list kept only its own row).
- **Mount on ▸ Truss / Boom / Hanger / Tower acts on every selected
  fixture** when the one right-clicked is among them (it says how many),
  as Deck already did; each keeps its place along.
- **The Lighting Studio keeps the stage in the middle**: a fixture far off
  the grid no longer leaves the view scrolled to one side after a resize,
  and the "stage left / downstage" edge labels no longer smear when the
  plot scrolls.
- **Fixtures dragged together onto a truss all hang on it** (Shift-drop),
  each where it landed along it, not only the one over the truss; in Front
  and Side every free fixture of the selection keeps its drop instead of
  snapping back.
- **Pages squash with the window** instead of being cut off on the right:
  the Lighting Studio's bottom bar, the Workbench's buttons and the
  Connections ▸ Devices row scroll sideways when the window is narrower
  than them. The Lighting Studio no longer needs 1006 px, Build 861 or
  Connections 823 (all now under 640).
- **The rig view no longer asks for your Downloads folder** (board Q85). A
  truss sock or tower face picture chosen from outside the show's folder is
  copied into an **images** folder next to the show when it's saved and
  saved relative to the show, like QLC+'s audio and video files; it opened
  the original every time before, which macOS asks about, and the show
  lost its pictures on another computer.
- **A fade or hold typed on a Show Tree cue is kept when you click away.**
  It was applied only on Return: clicking another cue lost it. Tab, a click
  on another cue or anywhere else, and the page moving on now apply it to
  the cue it was typed for; Esc in the field puts it back. A chaser step's
  times in the Workbench go to the step they were typed for when you walk
  to another.
- **A Closed shutter look closes** (it sent Open on spots with an Open
  capability, e.g. the AT-Spots); *Strobe* strobes, and auto-open defers to
  both.
- **Esc in the Look Editor's colour picker** no longer hides the picker
  for good.
- **A playing scene follows its group's members live**, as Build's help
  says. Its group's fixtures were resolved when it started, so a fixture
  put in the group stayed dark (and one taken out kept the look) until the
  scene restarted. A running scene now picks the change up on the next
  frame: a fixture that joins fades in over the look's fade-in, one that
  leaves is let go of over the scene's fade-out (or at once with none), as
  the scene stopping would, and the rest play on untouched.
- **Lighting Studio: a pasted truss brings its fixtures.** ⌘C / ⌘V (and
  Duplicate selected, Paste features here) copied the truss alone, though the
  shortcuts help said structures copy with their fixtures. Each fixture on a
  copied truss now comes too: a new fixture with the same definition, mode
  and settings, a name of its own ("LX1-1 #2"), hung at the same place on
  the copy. They arrive **unpatched** -- which addresses copies take is a
  patching choice (Patch Others Like This) -- and the plot's status line
  says so. One ⌘Z takes the truss and its fixtures away.
- **Pasting a truss with fixtures asks** (Q55): *Paste N fixtures with
  the truss* → **Patch at next free addresses** (after the originals in
  their universe, the Patch dialog's first-fit, spilling into the next
  universe when it is full), **Leave unpatched**, or **Cancel** (nothing
  pasted). Still one ⌘Z for the whole paste; the status line says where
  they went.
- **Hiding the Workbench stops only what it started.** It started and
  stopped what is loaded (the Design preview, the Operate run) with the
  Master parent, and a Master stop clears every source -- so a function a
  VC button, the Run tab or a cue was already running stopped when the
  Workbench hid or let go of it. It now plays under its own parent and
  takes only that away.
- **Build: deleting and duplicating a function can be undone.** Delete,
  Duplicate, ⌘C ⌘V and Build ▸ Effects' Duplicate / Delete of an EFX or RGB
  Matrix went straight to the show with no step on the shared history, so ⌘Z
  couldn't take them back. Each is now one `PaletteOps::functionStep`: Undo
  brings a deleted function back with its id, in its place in each
  collection, chaser and show track (they ride in the step whole), and hands
  it back to the VC buttons, cue lists and main cue list that let go of it;
  Redo deletes it again. A duplicate's Undo takes the copy away, its Redo
  brings it back with the same id.
- **Park, Highlight and a held last look hold their dimmers.** All three
  wrote the universes straight from the MasterTimer tick, before the
  universe thread's `processFaders()` -- which zeroes every intensity
  channel and then lays the running faders on top. So a parked or
  highlighted fixture went dark or showed the running cue, and a timeline
  track's *Hold at end* and the look a show parks on at its end both went
  dark (pan/tilt held; B1, in the fork since each feature came in). They now
  go out through faders of their own (`OutputLayer`), at new
  `Universe::FaderPriority` levels: **Park** on top, Override and past the
  Grand Master (`Universe::writePastGM`, a new `FadeChannel::PastGM`), still
  taken out by Blackout; **Highlight** just under it, Override, with the
  Grand Master and Blackout applying; the **held last look** under
  everything (`Hold`), merged like any running function -- intensity HTP, a
  function takes an LTP channel, and a starting cue still takes its fixtures
  from the hold. `ParkEffect`, `HighlightEffect` and `LastLookEffect` are no
  longer DMX sources. The precedence is written up on
  `Universe::FaderPriority`. **Flash** (`ProgrammerFlasher`) and **Focus
  mode** (`FocusSession`) had the same bug -- a flash or a focused head's
  light lost its dimmer to the frame and to a running cue -- and go out the
  same way, at `Flash` and `Focus` (Override, between Highlight and Park;
  GM and Blackout apply). `FocusSession` is no longer a DMX source;
  `ProgrammerFlasher` still is, only to time its flashes.
- **A Grand Master move no longer drops an On-when-not-in-use relay.**
  Moving the Grand Master recomputes the output at once
  (`Universe::slotGMValueChanged()` -> `updatePostGMValue()`), from the
  stored value -- 0 for an On-when-not-in-use channel nothing sets -- and
  the floor only came back with the next frame's `applyOnAtOpen()`, so a
  relay could drop for a frame. `updatePostGMValue()` now applies the floor
  itself (`onAtOpenValue()`), so every path that works out the output has
  it.
- **Shows timeline: Undo keeps each track's settings.** The timeline's undo
  snapshot (`ShowManager::captureSnapshot`) held a track's name, mute,
  colour and items only, and Undo rebuilds every track from it -- so any
  Undo reset Hold at end, Solo / Solo-safe, priority, intensity and the
  track lock on every track, unlocked locked items, and dropped a marker's
  cue-list link. They are now in the snapshot and put back.
- **Build: the Beat button shows the tempo a Tap set.** It kept the old
  tempo: `slotTapBeat()` set the BPM under a signal blocker the button's
  text follows. Only `slotBpmChanged()` (which would restart the tapped
  beat) is held off now.
- **A Closed shutter look closes.** A Shutter palette resolved to the
  fixture's Open capability whenever it had one (and to 0, open, on a
  strobe-preset channel), whatever the look's value -- so *Closed* (and
  *Strobe*) sent Open on the AT-Spots, and auto-open had nothing to defer
  to. `QLCPalette::shutterIntent()` now reads the look's value on the
  first target whose shutter has a tagged (then a named) Open, Closed or
  Strobe capability there, and `shutterValueFor()` sends each fixture the
  value as set where it means the same, else its own closed / open / a
  strobe at the same rate. No value or 0 (every look made before, the
  course shows' "Open" among them) is still Open; a strobe-preset channel
  never decides it and has no closed, so it stays open. The Shutter icon
  uses the same code, so a Closed look shows closed; the Look Editor shows
  an open look as the type's Open, keeps a Closed picked at 0 as 1, and no
  longer offers a strobe-only channel's single "Strobe" as a Shutter
  source.

- **Move in black no longer carries over to the next show**: a new show,
  or one saved without it, starts off with the default lead time.
- **An imported chaser of raw scenes plays as the chaser did** (Sample.qxw's
  par chasers ran at about half level); its cue-list number is no longer
  copied onto the new list's chaser.
- **The look-ahead's static output** scales a dimmerless fixture's colour
  by the look's dimmer.
- **Back, then GO, keeps the colours the cues set**: Back and a jump put
  back what the cues before set last, so every cue looks the same however
  you get to it.
- **No "declares no light output" warning** for a curtain, a fan, a hazer
  or a smoke machine.
- **Quick focus draws where the light should point** while you nudge, not
  the nudged DMX.
- **Game controllers**: kept working with qlcconsole behind other windows
  (App Nap); found again when switched back on or after sleep; HID lines
  numbered consistently; every Xbox model read in the right layout; the HID
  plugin no longer opens every HID device or writes a debug file.
- **Studio**: target items are dropped before a new or opened show deletes
  the targets.
- The show's LED layouts are guarded against a running matrix effect while
  they are edited.
- **A timeline Override track wins its dimmers too**, as the help says: a
  lower level on it pulls a light below a Normal track or a cue at full
  (intensity used to mix highest-wins). Park, Focus, a flash and a
  highlight still beat it, the Grand Master and Blackout still apply, and
  when its item ends what plays underneath comes back.
- **A track's intensity set before Start plays**: the track's fader and the
  show's attribute for it are one value, so the level set last (and saved
  with the show) is the one that plays. A track saved at 50% used to start
  at full.
- **Not-ours channels are refused every way a light gets an address.**
  Adding a fixture with an address there (Add Fixture, an RGB panel, the
  plot) and moving a patched one there (Fixture Properties) are refused now,
  as the Patch dialog always refused them; the Add Fixture dialog says the
  address is taken. One check (`Doc::isOnNotOurs`) serves them all.
- **Deleting a scene a timeline plays, then Undo, puts the Show back as it
  was**: one track, its item playing the scene. Undo used to add a second
  track playing a new "Copy of Copy of Walk" and leave a stray "Copy of
  Walk" in the show. Deleting it before the Shows tab was ever opened no
  longer leaves a dead item on the track.
- **ETC ColorSource CYC has a beam angle** (100°), so the plot no longer
  draws it at a guess. ETC publishes none ("asymmetrical"); the value is
  derived from its photometrics and noted in the definition.
- **A sock or tower-face picture in a show not yet saved** keeps its full
  path, whatever folder the app was started from.
- **A venue file from a newer build keeps what this build doesn't know**
  (elements between and inside venues, inside the house rig and a
  practice) through the next save, instead of dropping it.
- **A house light that clashes when a venue goes on goes in unpatched,
  whatever the clash** (docs/design/VENUES.md section 6): on channels the venue keeps,
  or in a Tour slot, as well as on a tour light's channels. It keeps the
  address it asked for and the clash list still names it. A tour light
  that clashes stays as the show has it.

### Known issues

- Under very heavy machine load a fade can finish late (seen in the course
  test's 3 s fade taking ~8 s with dozens of builds running); the universe
  threads run at normal priority.

## [0.2.1] - 2026-10-21

A point release of the Show Tree beta: one top bar, auto-open shutters,
a controller map in Focus mode, sub-cues from QLab, and fixes to Import
chaser, confetti on two-arm beams and the update installer.

### Added
- **Focus mode controller map and device choice.** The Focus mode panel
  draws the controller (an Xbox-style layout, vector, following the theme)
  with each control labelled by its Focus mode action, taken from the
  profile's gamepad roles rather than fixed text: a profile with other names
  (Cross, R1, Options…) shows them, a control it lacks is greyed, unused
  ones (triggers, stick clicks, Guide) are grey. Buttons light while held
  and the stick caps follow the sticks; a click on a control does it.
  **Use: Controller | Keyboard** in the setup dialog and the panel picks
  the device that drives the nudges and the map shown (the keys, labelled
  and lit as pressed, in Keyboard). Default: the controller when one Focus
  mode can use is connected, else the keyboard; the user's choice is kept
  per machine (`focusmode/input`). On the keyboard every controller input
  is swallowed and ignored -- sticks centred, nothing applied -- so a
  resting pad cannot drift a head; keyboard shortcuts work in both. A
  controller disconnecting mid-session switches to the keyboard with a
  note, and returning offers **Use the controller**.
  `ProgrammerController::focusController()` / `focusControllerChanged()` /
  `focusControllerInput()`, `FocusSession::setInputDevice()`.
- **Auto-open shutters** (Settings → Advanced, saved with the show, on by
  default). Like a console's auto open: a head given a level (its dimmer,
  or a colour when it has no dimmer) by any look, layer, cue, slider or
  programmer edit, whose shutter nothing sets, has its shutter opened on
  output, at the definition's open value (an open default, else the
  ShutterOpen / "Shutter open" capability; a strobe preset channel at 0;
  never an iris). An explicit shutter value -- Closed, Strobe, anything --
  always wins, including one a slider or programmer edit left behind; a
  head with no level, or a dimmer only resting at its default, is left
  alone. Each universe applies it after its faders on its own thread, on
  the post-GM values only, so DMX, the 2D view and the Rig Overview agree
  and turning it off restores exactly what the show sets. Shows that never
  turned it off save as before (`<ShutterAutoOpen Enabled="0"/>` only
  when off). The AT-Spots (Focus Spot Three Z, shutter resting at Off) now
  light from White, Full alone. The Programming tab stops warning "no look
  for: shutter" while it is on.
- **Sub-cues from QLab.** OSC `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play
  a sub-cue (its cue first); `/subgo` and `/subback` say why when they
  can't; LOAD 4.2 puts a sub-cue on standby and **Next:** in the footer
  and the cue list shows it ("Next: 4.2 — Wash"); STOP 4.2 stops the list
  only while 4.2 is the one playing. ⌥⌘↩ / ⌥⌘⇧↩ go through show control, so their result is
  reported with the rest.
- **Two new look warnings.** The Looks list marks a Colour look that is all
  zero (**Black**), and a position look on fixtures whose definition marks
  every Pan or Tilt channel Fine (**Can't move**: the head stays at 0).

### Changed
- **Settings has General and Advanced tabs.** Advanced holds the show's
  output behaviour (auto-open shutters) and two settings for experienced
  users moved off the one page: the effect script editor, and the Show
  control (QLab) pointer to Connections. Everything else stays on General.
- **One top bar instead of two.** The show controls and the open tab's
  own tools share one row: the tab's tools on the left, **Stop all**,
  **Blackout**, **Blind** and a **Design | Operate** switch (the mode you
  are in lit) on the right, as small icon-and-word buttons. About 57 px
  more for content on every tab (94 px of bars above it before, 37 px
  now). Blackout lights red and Blind blue while on, readable in all six
  themes. A narrow window folds the tab's tools into **»**; the show
  controls never fold. **Settings → Show Controls** can put them in the
  status bar instead, at its right end, leaving the top to the tab's own
  tools; it switches at once, no restart. Same actions and shortcuts
  either way; a tab in a window of its own keeps its tools at its top;
  kiosk mode still has neither. The Show Tree header and the Programming
  tab's row are toolbars now (they fold into » rather than being cut off
  or scrolled), and the Lighting Studio's toolbar uses the same 20 px
  icons as the others.

### Fixed
- **The Show Tree's drop hint stays in sight in the shared top bar.** The
  note that says where a drag will land (and, after a change, what it
  changed) sat at the end of the Show Tree's toolbar, so once the toolbar
  shared the top bar with the show controls it folded into **»** at any
  window narrower than about 1900 px — every laptop. It now stands beside
  the toolbar, just left of **Stop all**, and keeps up to 420 px; the
  buttons fold first.
- **Import chaser keeps an include brighter than the cue's own level.** A
  cue that includes another at full and sets the same fixtures lower itself
  (DanceHysteria's Full Show 0.0.5: 0.0.4's StepTops at full, its own at
  45%) played the 45% after import; the chaser had run both and the full
  level won. The importer now makes such a cue **mix** (Overlap: Mix, *≈
  mix* in the tree) — only where the tree's rule would change its output,
  checked channel by channel — and in a cue that mixes an include plays
  beside the cue's own layers on the same group instead of giving way to
  them, so it plays as the chaser did. The report says which cues and how
  many channels; Convert to tracking keeps them mixing. On DanceHysteria-0.6
  the import dry run is 0 channels different in every cue (Full Show 0.0.5
  marked; 0.0.6 and 0.0.7 already mixed and now match too). The dry run
  (`showtree_test importARealShow`) reads the show's embedded fixture
  definitions, as opening it does, with the user folder kept out, so it
  compares the same on a machine without the author's fixture files, starts
  each playback from the idle rig with channel defaults put back, and fails
  unless 0 channels differ.
- **A dimmer effect on a two-arm beam went black.** One master dimmer
  serving several heads got one scaled write per head, multiplied together:
  confetti's lit arm times its dark one was 0. The brightest head now sets
  it.
- **GO by number on a paused list resumed it and stayed put.** It now plays
  that cue.
- **An OSC float cue number matched nothing.** 2.2 sent as a float read as
  2.200000048; floats now read to the digits they hold.
- **The update installer copes with a stale disk image.** Something
  scanning a fresh download can leave the DMG attached with nothing
  mounted, and every later attach then failed ("the disk image would not
  open"). The macOS update helper now detaches such leftovers first,
  retries a transient attach failure (not a bad image), waits until the
  old mount is really gone, and cleans up if it is stopped part-way; the
  app asks it to stop before killing it, so that cleanup runs. Its error
  says what hdiutil said.
- **Two flaky tests steadied** (Updates_Test's disk images, built now
  without attaching anything; ShowControlUi_Test's OSC port, now checked
  and retried), and the course and office show tests build with GCC's
  -Werror.

### Tooling
- **`make dmg`, `make beta`, `make release` and `make attach`** build the
  signed universal DMG and publish to GitHub Releases through the REST API
  only (`platforms/macos/publish.sh`; docs/dev/RELEASE.md). `make test` builds the
  dev app.
- **Show Tree course** (docs/course): Levels 0 and 1, lessons 0.1-1.5 with
  the Level 1 check, in the course voice, the teaching show-choir rig and
  its shows, a browser practice version of Lesson 1.1, and a test that
  drives every lesson by the mouse.
- **Office show**: a generator for `Office.qlcc` (the office rig test
  show) and its test, run by the gate.

## [0.2.0] - 2026-10-21

The Show Tree release. A show is written as one tree — productions → cue
lists → cues → layers → looks — with tracking, sub-cues and a cue runner
for playback; line targets, line of sight and Focus mode for aiming;
curtains a fixture opens and flies; colour rings on movers; Matrix effects
for pixel boxes; backup WLED lanterns; and output held off the rig
network. Everything in 0.1.0 is in it too.

### Added
- **Software update check and install** (GitHub Releases). A background
  check after start-up and at most once a day, plus **Help → Check for
  Updates…**. It sends one unauthenticated GET of the releases list, with
  ETag / If-None-Match, waits out GitHub's rate-limit reset, and gives up
  quietly offline. Channels are Stable or Pre-release, defaulting to the
  build's own. Versions are compared SemVer-style, with `+qlcconsole.N`
  breaking ties. **Skip This Version** is available. A footer chip shows
  "Update available: X", and a non-modal dialog shows the release notes as
  Markdown. Downloads are verified against the release's `SHA256SUMS`, and
  anything that does not match is refused. On macOS the new app is staged
  beside the old one, after its code signature, Gatekeeper, Team ID and
  bundle id are checked. A detached helper swaps it in at quit, keeps the
  old app until the new one has started, and reopens the show on **Restart
  and Install**. A read-only /Applications falls back to opening the DMG.
  Windows runs the NSIS installer silently after quit. Linux and the Pi are
  notified only. Nothing installs in Perform mode, in kiosk mode, or while a
  function runs. "Install updates automatically" downloads in the
  background and installs at the next quit. `QLCCONSOLE_NO_UPDATES=1` or
  the system-wide `updates/disabled` turns it all off. `release.sh` now
  publishes `SHA256SUMS`, and `./release.sh attach` adds CI's files and
  their checksums.
- **Only transmit on the rig network.** A show can name its rig networks
  (Settings → Network, e.g. `172.18.0.0/20`, saved with the show as
  `<RigNetwork>` in the IO map). With **Only transmit on rig networks** on
  (this computer's setting, default on), Art-Net, E1.31/sACN, OSC and WLED
  output from any interface outside them is **held** — patch kept and saved
  unchanged, line not opened, nothing sent (no default-route unicast, no
  broadcast, no ArtPoll/ArtPollReply/discovery) — and resumes by itself
  within ~5 s on the rig network. Loopback and non-network outputs are not
  affected. A red footer chip ("Output held — not on the rig network
  (172.18.0.0/20). Connected: en0 10.4.12.7") opens the setting;
  **Transmit anyway, this session…** asks first and is never saved. A show
  with none set transmits as before, with an amber footer note and a
  suggestion taken from its patches' saved networks (offered, not written).
- **Focus mode** for aligning the rig before a show: step through the moving
  heads with the controller's bumpers, each aimed at its reference (a
  scene's aim, a stage target, a line target's own point) and lit, nudge its
  pan/tilt calibration offsets with the sticks (the follow spot's stick
  path) or D-pad, A to save (one undo step), B revert, X zero, Y a second
  reference, Menu to leave (asks about unsaved heads). Keyboard fallback for
  all of it. Lighting Studio / Rig Overview **Focus**, View → Focus Mode…
  (⌘⌥F), or View + Menu. Controller profiles gain optional gamepad
  `role=` attributes; the Xbox profile has them and the D-pad.
- **Show Tree** (new tab, ⇧⌘6): productions → cue lists → cues → layers →
  looks, with tracking (Block / Cue-only / Release), includes, one
  precedence rule up and down the chain, a plan view for line targets, and
  preview that matches GO. **Import chaser…** turns an existing chaser into
  a tree (0 channels differ on playback); **Convert to tracking** /
  **Detach to plain chaser** go the other way. A **Looks** section for
  busking (with **Make VC page**), the **Library** with palettes, Recent and
  Pinned, one undo history shared with the Programming tab, and a typed
  command line. Deleting a palette takes it off every look that uses it
  (one Undo puts it all back).
- **Productions, sub-cues and the cue runner.** Several productions per
  show, each with its own main list and timecode; sub-cues 4.1, 4.2… with
  Sub GO or Auto-follow; the **Run** tab (⇧⌘0, Space = GO) and the **Cue
  Runner** VC widget (and **Swap Widget** from a Cue List); OSC `/subgo`,
  `/subback`.
- **Line targets.** A stage target can be a line (or curve): an Aim look
  spreads its heads along it, crosses them, groups them in pairs or halves,
  in hanging or look order.
- **Colour rings on movers.** A mover's LED ring is a head type of its own
  (group by head type makes Movers and Ring groups); ring chases by name;
  drawn live in the 2D plot and Rig Overview; **Change Definition…** moves
  fixtures to a drop-in definition.
- **Create Group(s) from Selection** — one group, a group per head type, or
  a group per head type for each fixture, previewed before anything is made.
- **Palette folders hold any type** — folders first, or Group by type.
- **Matrix effects for pixel boxes.** Fourteen effects in a new Matrix tab
  (bars, chevrons, ripple, rain, checker, sparkle, radar, iris, equaliser,
  snakes, wipes, colour noise, box chase, strobe), across a run of boxes or
  in each box, with white-pixel options (the XL-450's).
- **Curtains that move.** A main drape or traveler can be driven by a
  curtain fixture (bundled **Generic › Stage Curtain**: Open, Open fine,
  Fly): it opens and flies as the channel fades, on the plot and in the Rig
  Overview, and blocks beams where it is (see Line of sight).
- **Backup WLED lanterns.** A WLED universe can drive several devices: a
  backup lantern mirrors the primary without programming anything twice.
  Right-click the universe in Connections (or the Fixture Manager) →
  **WLED Devices / Backup Lanterns…** to add, remove and reorder devices
  (the top one is the primary), each with its own address and channel count
  if it needs one; one undo step. Every device is sent to on its own, so a
  dead or hanging one never holds up the others, and one that comes back is
  sent the whole look again. Connections says "1 of 2 answering — … not
  answering", the footer names the silent backup, and the rig view shows
  the first device that answers. Saved as `outputIP2`, `outputIP3`… beside
  `outputIP`; older versions ignore them and drive the primary alone.
- **Line of sight for follow spots and aiming.** The stage's solid features
  — drapes and travelers as they hang (closed blocks, the open gap doesn't,
  flown is out of the way), walls and the proscenium, platform and riser
  sides, towers, and trusses/pipes/stands at least 20 cm thick — are checked
  between each aimed head and its point; a head's own mount never counts.
  The Followspot effect's **Fade When Blocked** (ms; 0 = off) fades a
  blocked head out and back; an Aim look's **Dim lights that can't see the
  target** (off by default) does the same for any aimed look. Reach
  warnings name the blocker ("blocked by Main drape"), the Lighting Studio
  plot draws blocked aim lines red to the blocker, and **Show → Line of
  sight** sets which kinds block and whether the 3D views stop beams at
  them. Effect scripts read `sceneTarget.blocked` / `blockedBy` /
  `blockedKind` / `blockedAt` / `clearance` (also on `aimAt` slots). A
  line target checks each head's own point on the line; a curtain driven by
  a fixture blocks where that fixture has it now.

### Changed
- **A new production is current when nothing else has GO.** In the Show
  Tree, a production made while no production is current and the main cue
  list isn't some other chaser becomes the current one, so a first
  production plays from the Run tab without **Make current**. Adding a
  production never takes GO from one the operator chose.
- **Coloured light reads as its colour in the rig views.** Light landing on
  a grey or white surface (a cyc, the deck) now takes it over in the
  light's colour: a full blue wash on the cyc draws blue, not white with a
  lavender tint. White light looks as before, and coloured surfaces (LED
  strips, drapes) keep their own colour.

### Fixed
- **Beams stop at risers and the floor in the rig views.** A wash thrown
  across a riser went through it, lighting the air behind it, and its cone
  carried on below the stage floor (the cone's edge was followed to the
  surface the middle of the beam landed on, a drape far beyond). A beam now
  stops at a platform's side as well as its top, each edge of the cone ends
  at what it meets, and a side or front elevation cuts the beam round the
  risers it meets and shows the wash on their tops and faces as a band.
- **Calibration offsets no longer show in the rig views.** A head's pan/tilt
  offsets correct the physical fixture: the DMX keeps them, but the plot,
  its gauges, the Studio, the Rig Overview, beams and footprints now draw
  the intended aim. Fixture Properties' **Test orientation → Aim at target**
  uses the aim solver (it was 180° out and solved standing heads as hung).
- macOS Xbox controller (raw mode): axes now arrive in the order the Xbox
  profile names them (the right stick was reported as the left trigger),
  and the D-pad is reported.
- **Crash on quit with the Show Tree open.** Closing the app while its
  window was still up could crash: the Show Tree tab stopped its preview on
  a show that had already been freed.
- **The cue page's drop area is readable in every theme.** "Drag groups or
  fixtures here…" was nearly invisible on the dark themes (Blue especially).
- **Chaser teardown use-after-free.** Deleting a chaser while its runner was
  still running freed the steps the runner reads (an intermittent "double
  free" on Linux).

## [0.1.0] - Unreleased

First public release. qlcconsole is a fork of
[QLC+ 4.14.2](https://github.com/mcallegari/qlcplus), rebuilt around a
console-first programmer-mode workflow.

**Platforms:**
- **macOS** — one app for Apple silicon and Intel Macs (macOS 12 Monterey
  or later), signed and notarized, as a DMG.
- **Windows** — a 64-bit installer (Windows 10 or later). Not code-signed
  yet, so SmartScreen asks before it runs.
- **Linux** — an x86_64 AppImage.

**Soak-tested** (2026-10-16 → 17, the DanceHysteria show, three 8-hour
phases — the real show, a scaled-up show, rough handling with reloads —
with DMX output stripped): no crashes or hangs on Linux x86_64 (Debian 13),
Linux arm64 or macOS in any phase. On Windows 10 (a VM) the real-show phase
ran clean; the scaled phase was cut short by a Windows Update reboot, and
a shorter Windows re-soak is planned.

### Added
- **QLab and show control.** Chase steps carry cue numbers (a Cue column in
  the chaser editor, with Renumber); a chase can be the main cue list or
  have a list number; **MIDI Show Control** and **OSC** can GO, LOAD, STOP,
  PAUSE, RESUME, RESET and ALL OFF them. The footer's **Next:** shows what
  the main list's GO plays next; GO (⌘↩) and Back (⌘⇧↩) work from any
  window; the Virtual Console cue list shows cue numbers and what's next.
  **Skip** a step without deleting it — GO, Next, Back and auto-follow pass
  over it.
- **Connections → Show control & sync** — one page for what controls the
  show from outside and what it syncs to: OSC in (port, listening or not,
  the last command), MSC in (device number, **Listen on** all MIDI inputs
  or just one, the last command), and the MIDI timecode source with its
  sync health. The timecode source is saved with the show.
- **Sub-cues** — a chase used as a cue lists its own steps under it as
  4.1, 4.2…; set to **Sub GO** they wait for Sub GO (⌥⌘↩, Sub Back ⌥⌘⇧↩)
  while GO moves on to the next cue. Shown in the chaser editor, the
  Virtual Console cue list (with Sub GO / Sub Back buttons) and the footer
  ("Sub: 4.2 · Next: 5").
- **Output trouble is reported** — an Art-Net or E1.31 output that can't
  send, an Art-Net node that stops answering, or a node port reporting a
  DMX short shows in the footer (with **Open Connections**) and in red on
  the universe's row in Connections. It settles before it warns, so a
  brief hiccup doesn't flicker on and off.
- **Shows carry their own definitions** — a `.qlcc` saves the fixture
  definitions and effect scripts it uses. Opened on another computer, the
  show's own copies are used (**Install on This Computer** keeps the ones
  it lacks); where they differ from that computer's the footer says so and
  **Review…** shows the differences. **File → Collect for Transfer…** gathers a
  show and everything it needs into one folder.
- **One beat clock and speed masters.** Every effect and beat chase reads
  one clock (effects started apart are on the same beat); a Tap on the
  Programming toolbar. Up to sixteen **speed masters**, each its own BPM and
  tap, rate and pause, or following the main clock or another master by a
  ratio; a look, chase, step or collection runs on one, and the footer
  shows each master's beat. **GO on the beat** holds a GO to the next beat
  or bar.
- **Spread** — an effect's phase spread across its heads: by order,
  columns, rows or across the stage, forward, reversed, from the centre or
  the ends, mirrored or in blocks, or out from / round the look's aim; a
  preview in the look editor.
- **The stack checker** — the Looks list marks a look the one below always
  covers, an effect another covers, a dimmer only some heads get, and says
  why.
- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, as CSV or a printable PDF (Fixtures → Patch List…).
  **Labels…** prints a label per fixture (all, or the selected rows) on
  Avery 5160 sheets or a 62 mm roll: name, universe and address, mode,
  and a network fixture's name (its IP on mouseover in the list).
- **Units** — Settings → Units picks mm, cm, m, inches or feet (kg or lb
  to match); fixture sizes and weights show in it and take any unit typed
  with the number (`7.5"`, `6' 3"`, `26.5 lb`), and the Studio's grid
  follows it. Files still store millimetres and kilograms.
- **In-app pictures** — the guides show the main window, footer,
  Programming tab, effect browser, Patch dialog and more with numbered
  marks; click one for full size.
- **The Rig Overview on the graphics card** (Metal on a Mac, OpenGL
  elsewhere) with its fixtures worked out on every core: a fully lit
  208-fixture show from about a second a frame to 15 ms.
  A **GPU** box turns it off. Its own **Layers** menu shows and hides
  layers — the same switch as the Studio's eye.
- **Washes look like washes** — a pool fades past its beam angle for a wash
  and stays crisp for a profile; PC and Fresnel lenses are softest. Pools
  show on decks and steps.
- **Programming tab** — a dedicated top-level tab for building scenes/"looks":
  searchable/foldered palette and function trees, drag-and-drop palettes onto
  looks, groups onto dynamic targets, fixtures onto fixed targets. Inline look
  editor for color, dimmer gradient, XY pan-tilt, and gobo/shutter. Live
  DMX/2D preview while editing (Design mode).
- **Group scenes** — scenes that target a fixture group dynamically, so a
  look can be reused across different fixture sets.
- **Fixture Manager enhancements** — fixture-group folders, empty-group
  creation, and a drag-and-drop head-layout grid editor (place fixtures on
  cells, add a whole group as a block, move sub-groups as a unit).
- **Show timeline** — explicit show length with a Logic-style end handle and
  park-at-end behavior, so a show doesn't scroll off into nothing once MTC
  runs past its end.
- **MIDI timecode sync-health** — live rate/jitter/latency readout with a
  healthy/unstable verdict, to help diagnose timing issues against real
  hardware.
- **Effect scripting system** — a scripted effect lifecycle (one-shot bursts,
  sweeps, reveals) with audio- and MIDI-reactive effects; see
  [EFFECTSCRIPT_API.md](docs/dev/EFFECTSCRIPT_API.md) and
  [EFFECT_LIFECYCLE_DESIGN.md](docs/design/EFFECT_LIFECYCLE_DESIGN.md).
- **Effect presets** — named, reusable configurations layered over effect
  scripts; see [EFFECTPRESET_DESIGN.md](docs/design/EFFECTPRESET_DESIGN.md).
- **Lighting Studio** — a rigging tab for the physical rig: place and rig
  trusses, towers, platforms and steps, pipes, stands, hangers, a
  cyclorama and power sources; drag a fixture onto a structure to mount it,
  with select/move-together group behavior; a Layers panel for organizing
  the plot; Power/DMX/Network/Stage-only overlays. The plot is seen from
  Top, Front or Side, and each structure's own editor adds an **Angled** 3D
  view; **View → Rig Overview…** (⇧⌘R) opens the whole rig in 3D in a window
  of its own. See [the feature guide](docs/features/lighting-studio.md).
- **The rig view is lit by the show** — in Design mode the selected look
  (or live output) lights the 3D rig: beams per head that carry past their
  target and land as pools on the deck, platforms or cyc; brightness from
  the fixture's declared lumens or watts; a Haze setting (None / Light /
  Medium / Heavy) with a glow round lit LEDs; gobos, gobo rotation, prisms
  and your own gobo images per fixture; pixel bars lit section by section;
  truss socks and tower faces (with printed patterns) glowing with the
  bars inside; people targets lit in the colour that reaches them. A moving
  head's footprint shows everywhere its pan and tilt can reach.
- **Lanterns and hangers** — hangers are rigging, like a truss: clamped to
  a truss or tower with an arm and a drop to one hook, edited in the rig
  view and copied to every tower in one step. A lantern body glows all
  round, with its LEDs (a hanging cluster or tape) seen through clear or
  diffusing glass.
- **Fixtures tab** — a Power folder for modeling power sources/circuits and
  assigning fixtures to them, plus a per-universe DMX address usage grid.
- **Connections tab** — the I/O manager rebuilt around a strict tree
  hierarchy (host → protocol → interface → target → port → universe), with
  right-click menus scoped to exactly what applies at each level, IP-first
  patching, rerouting a line's universes to a different interface in one
  step, and "portable" patches: opening a workspace on a machine missing the
  network it was built for keeps the mapping intact (instead of losing it)
  without broadcasting to the wrong place. Ctrl+Z undoes the last
  patch/universe change made here.
- **Control-surface engine** — a device-agnostic engine for hardware
  control surfaces (role/page vocabulary, context-aware LED feedback), with
  an OpenDeck PMJ Black 1 overlay underway; APC40 mk2 and Xbox controller
  support planned on the same engine.
- **Consistent startup window** — app icon/name/version, a scrolling log of
  what's loading, and a progress bar cover the whole boot sequence (plugin/
  fixture/profile loading, opening a workspace passed on the command line,
  autosave recovery) on every platform, replacing the old indeterminate,
  macOS-only progress dialog. The main window now only ever appears once
  everything is actually ready.
- **Real-time DMX output timing** — on macOS the DMX timer thread asks the
  OS for real-time scheduling, fixing missed output ticks on a busy
  machine. On Windows it has a thread of its own with "Pro Audio"
  priority; on Linux it asks for real-time priority, which a stock install
  doesn't grant (it then runs at normal priority).
- **Backstage color themes** — six selectable app-wide color themes (View →
  Theme): Default, Tan, Blue, QLC+ Original, Red Shift (keeps the blue
  channel near-zero, for working in the dark without wrecking night vision),
  and VS Code Dark.
- **Move-in-black / pre-positioning** — Mark cues to silently pre-position
  movers before a reveal, with a live dangle-detector footer chip flagging
  any fixture left un-marked.
- **Look scope** — a Scene/Look can record whether it is meant for the
  whole stage or a specific fixture group. It is kept in the show file but
  no longer shown in the Looks editor: it changes nothing that lights, and
  next to Targets it read as a second place to choose fixtures.
- Real **File → Import** for `.qxw` — merge fixtures, fixture groups, and
  functions (with their full dependency closure) from another workspace
  file into the one currently open, with automatic ID-collision remapping.
- Rebrand throughout: application, launcher, and fixture-editor binaries,
  macOS app bundle, and About box now identify as qlcconsole, with clear
  in-app attribution to QLC+ and its original authors.
- **Inventory-first workflow** — fixtures can be added to an inventory with
  no address, then placed, grouped and programmed before they are patched
  (they send no DMX until then). A docked Inventory panel in Lighting Studio
  drags them onto trusses, risers and towers; the **Patch** dialog patches
  or re-addresses a selection in one go (keeping their spacing or packed),
  and fixtures can be returned to the inventory in Design mode. Drop
  fixtures or a group on a universe in Fixture Manager to patch them there.
- **Network design** — place Art-Net/sACN nodes and wireless DMX
  transmitters/receivers on the plot (from a catalog of common gear, or
  your own saved models); the Network overlay draws each port's cable run
  through its fixtures, and **More → Network Report…** lists nodes, ports,
  universes and run lengths, with Copy and Save CSV. Unpatched fixtures can
  be placed on the nearest node port with room.
- **WLED output** — Wi-Fi LED devices running WLED get one universe each,
  by IP, sent as WLED's own JSON state only when the look changes (about ten
  times a second at most). Devices that don't answer, or are on weak Wi-Fi,
  are reported in the footer; while the rig view is open it shows what each
  device's LEDs are actually doing. Looks can pick WLED's own effects for
  the device to run, and **160 WLED effects** play in qlcconsole itself —
  every 1D effect, the 23 sound-reactive ones on the audio input. Tried on
  real devices only briefly so far.
- **Scene transitions** — a scene can have a Transition in and out that
  reaches each fixture by where it stands: wipes across, up/down or
  front-to-back, from/to the centre, dissolve, or along each bar, set on
  the scene's canvas in the Programming tab.
- **Effect browser** — the look editor's effect picker is a searchable
  browser with categories, a Recent list and a live 16-light preview of
  each effect. The RGB Matrix effects hidden behind same-named scripts are
  listed too ("Plasma (RGB Matrix)"), including the bottom-up Fire.
- **Readiness footer** — the "Ready" chip becomes "Not ready (N)" and lists
  unpatched fixtures, fixture types with no light output declared, address
  clashes, duplicate names, unreadable or duplicate definition files,
  universes with fixtures but no output, WLED devices not answering, files
  in cloud storage not on this computer, and fixtures with no definition
  here. Select rows (shift-click for a range) and fix them from the list.
  See [the feature guide](docs/features/other-improvements.md#readiness-footer).
- **Cloud-storage safety** — online-only files in a synced folder
  (Synology Drive, iCloud, OneDrive, Dropbox) are skipped at startup
  instead of waited on, with a footer warning and **Download Now**. A
  **show cache** on this computer keeps every non-shipped definition a show
  uses; before a show loads, each comes from the library, the cache, or a
  cloud fetch you can Skip.
- **Fixture definition repair** — a definition that won't load opens in
  the Fixture Editor's repair view, showing where it breaks and offering
  tested repairs; a Problems panel checks definitions as you edit, and each
  definition shows whether it is Included or your own (Local).
- **Stage targets** gain kinds (Person, Drum kit, Area, Structure), can
  follow the truss, platform or tower they're bound to, and scenes say
  "can't reach" when a head's pan/tilt range can't get there.
- **Layers panel** — rename any row with Enter, Select All Similar, fixture
  groups listed under the structures their fixtures are on, and plot
  selection mirrored in the list.
- **Plot** — Select All (⌘A), one undo per drag, and one lock per layer
  (everything locked is outlined red).
- Fixture names are unique: renaming to a taken name is refused, a
  duplicate is numbered on add or import, and older shows' duplicates can
  be numbered from the footer or Fixture Manager.
- A batch of network fixtures gets one IP each, counting up from the first.
- The look editor warns when a colour look is nearly dark (a hue picked
  from black), and a palette being edited is tried on the open look so the
  rig follows.
- Programming tab: search over Fixtures & groups, and "← Back to <scene>"
  from a fixture group's layout view.

- **In-app help** — F1 opens the guide for whatever is in front (each tab,
  the Rig Overview, Fixture Properties, the Patch dialog, the look editor,
  the power dialogs); the Help menu lists Getting Started, What's New,
  Keyboard and Mouse, Effects and Troubleshooting, with search across every
  guide. The guides are bundled into the app, and a test checks every link
  in them.
- New guides: Getting Started (startup and the ways a show comes together),
  What's New, Keyboard and Mouse, Effects, Troubleshooting.
- **Text** effect — words on a pixel grid, scrolling or still, in the look's
  colours; effect scripts can declare a `"text"` setting (a text box in the
  look editor). The RGB Matrix Text algorithm gains a **Centered** style.
- **Step Test Pattern** effect — strips counted, steps numbered and named,
  and a spinner that stops if output freezes.
- **Patch Others Like This** / the Patch dialog's **Match** — like
  structures (four towers, four steps) patched alike: each fixture at its
  counterpart's address, in its own structure's universe.
- **Mount on → Deck** — the selection stood on the deck under each (or a
  named one), upright, on its top, nothing floating.
- The Rig Overview follows the Lighting Studio's layers.

### Changed
- The version you see is the release: the About box, `--version` and
  Finder say **0.1.0**, with the build (`4.14.2+qlcconsole.<N>`, what a saved
  show records) and the commit beside it (see [RELEASE.md](docs/dev/RELEASE.md)).
- **Qt 6.8 or later only**; Qt 5 is no longer supported (the last tree that
  built on it is tagged `qt5-last`).
- Opening the app goes straight into qlcconsole — no window asking whether
  to open the Fixture Editor (Fixtures → Fixture Editor…; a double-clicked
  definition still opens in it).
- Showing and hiding layers, and their labels, work in Operate mode; locking
  stays a Design-mode change.
- A look stacks one way: top to bottom, the lower wins; an effect's colour
  replaces the base; dimmer effects scale and multiply; a Colour or Dimmer
  under an effect is that effect's only.
- Main tab order regrouped to follow the build workflow: Hardware →
  Inputs/Outputs → Lighting Studio → Functions → Programming → Shows →
  Virtual Console → Simple Desk.
- Function Manager, Virtual Console, the Programming tab, and the Show
  Editor each replaced their long rows of individual "New X" buttons with a
  compact "Add ▾"/"Edit ▾" dropdown; a View → Toolbar Style menu switches
  every toolbar at once between icons+text, icons-only, and text-only.
- Lock is now the layer's lock: locking any object locks its layer. Older
  shows' per-object locks move onto a locked layer named "Locked" on open.
- When two local definition files describe the same fixture, the most
  recently changed one is used.
- View → Rig Overview… opens its window without switching to the Lighting
  Studio tab.
- The effect script called "Fire" is now "Firelight" (looks using
  it keep it), so the RGB Matrix Fire is reachable again.

- A drag in the Lighting Studio only moves a fixture; **Shift** held as you
  drop hangs it on the truss or pipe, or stands it on the deck, it's over
  (the outline turns green while it would). Inventory drops still place.
  A mounted fixture is only offered a truss or pipe at its own height.
- **Functions** has no tab — Window → Functions (⇧⌘4) opens it in a window
  of its own.
- ⇧⌘R closes the Rig Overview when it's in front; ⌘W closes it too.
- Orientation is offered for anything that pans or tilts, bar-shaped or not,
  and Test orientation names its centred pose (straight up / down / out).
- Fixtures with a structure above them draw faded and dashed in the Top
  view; the rulers band any selection and mark the middle of what's dragged.
- The Connections Devices header is one row.
- Opening a show with many universes is much faster: a 94-universe show
  loads in about 9 s (was 34 s), in a tenth of the memory.
- A fixture whose definition isn't on this computer draws in the rig view
  as one small box (not a wall of red heads); the footer says which folder
  to copy the definition into, and **Fetch Definitions** looks there again.
- Off a Mac: the themes are readable (buttons, lists and splitter handles
  follow the theme), and the help spells keys Ctrl / Alt / Shift.
- Linux: the app icon shows at every size, and on Wayland.
- Effects on big pixel rigs: about 8 ms a tick for 96 × 64 pixels (was 68);
  the rig view about 9 ms a frame on a 200-fixture show (was 51).

### Fixed
- **Lights behind a closed curtain glowed through it in the Rig Overview.**
  Steps just upstage of the closed main drape showed their haze glow on the
  drape and on the floor in front of it, and the pools their light made on
  the back of the cloth. Opaque scenery and platforms now hide a light's
  glow, and pools on the far side of a drape stay there; where the drape is
  open the glow shows through the gap, and a scrim is still see-through.
- **Saving a fixture definition could stall for minutes on a busy Mac:**
  it flushed every disk instead of the one file. It now writes the file
  whole (or not at all) and flushes only that file.
- **Universe patches were lost when a show was opened away from its
  network.** Opening a show on a computer without the interface it was
  patched to (a laptop off the show VLAN) and saving it wrote the show
  without those patches — every Art-Net universe on that interface gone,
  however often it was re-patched. Patches that can't be opened are now
  kept, shown as offline ("not present — kept, will reconnect"), saved back
  unchanged, and reconnect by themselves when the network appears; inputs,
  feedback and patches to plugins not installed are kept the same way.
  Art-Net, E1.31 and OSC patches also **follow the network** rather than
  one interface address: the same show opens patched on any console on the
  show network, through whichever interface (or route) reaches it.
- A workspace whose last-active tab was Lighting Studio could come up with
  the whole 2D plot empty on a fresh launch — trusses, layers, and every
  fixture missing — even though the save was completely intact.
- Selecting a truss no longer drags along a fixture you click immediately
  afterward, and dragging a fixture bound to a truss no longer drags the
  truss along with it.
- A fixture dropped near (not exactly centered on) a truss can now be
  placed anywhere across the truss's width while still touching it,
  matching how fixtures are actually rigged, instead of always snapping to
  dead center.
- A show file with two fixtures on overlapping addresses dropped the
  second on load, with every group, scene and cue that used it. It now
  loads unpatched and flagged, and is saved back as it was.
- A fixture whose definition was missing was saved as "Generic", losing
  the real fixture for good. It now keeps and saves its manufacturer, model
  and mode, saving warns first, and Fetch Definitions restores it.
- Startup could hang for as long as cloud storage took to answer, or for
  good when it couldn't be reached.
- A steady effect could flash: the output sometimes caught it between
  frames and sent a blank one.
- An unpatched fixture in a running scene could crash the app.
- A gobo look sent nothing, and a new gobo look had no wheel to pick from.
- A look dropped on the canvas now lands where it was aimed, and a colour
  change to a look reaches every scene that uses it.
- Creating a palette or function in a new folder renamed the workspace's
  first look to the folder's name.
- Add Fixture gave every fixture in a network batch the same IP.
- Lanterns on hangers that kept an earlier mount's face drew on their side;
  a fixture on a hanger now always hangs straight.
- A light at the foot of a tower, moved under the bar on top of it, was
  hung on the bar.
- A copy of a fixture's physical data recompiled a regular expression and
  wrote a guessed power figure into the copy.
- **Don't Save** left the unsaved show's autosave behind, so every start
  offered the discarded work back.
- Two static looks on one channel kept the higher value (Blue then Red came
  out magenta-ish); a second Colour look moved a colour wheel; an Aim or a
  Dimmer after a movement effect did nothing; Breathe did nothing on movers
  with only a head dimmer.
- The speed master's beat blinked in the footer with nothing playing.
- The Xbox controller profile was installed where the HID plugin never
  looked.
- A crash under heavy load: with a busy show running, scenes and effects
  starting and stopping could corrupt memory and bring the app down (found
  in a 24-hour soak). Writes past the end of a universe are refused too.
- Lighting Studio's **Add Target**, **Add Person**, **Copy** and **Paste**
  did nothing.
- The patch grid didn't follow universes being added or removed, and the
  Function Manager's buttons didn't follow the selection.
- Windows: a slow tick could pile up hundreds of timer threads until the
  app stopped responding.
- Windows: the app felt sluggish — every output check walked every network
  interface; and USB devices plugged in while running weren't noticed.
- Art-Net send-failure warnings flickered on and off for a target that
  never answered; and Connections read a merging node port as "SHORT
  DETECTED".
- Engine: a fader's priority could be raised but never lowered, and an
  overriding 16-bit channel wrote only its low byte, to the coarse address.
  Nothing in 0.1.0 hit either yet; fixed ahead of the features that will.
- Rig Overview: an RGB fixture with no dimmer channel (pixel bars, many
  par cans) drew far too dark, or black, when dimmed: its level was counted
  twice.
- macOS: pressing the empty canvas in Structure Studio's Angled view to orbit
  crashed the app. Qt 6.11 draws its move cursor (and the wait/busy ones)
  from a colour-profiled picture that it frees too early (QTBUG-147602); the
  move cursor is now the open hand (closed while dragging) on macOS, and
  wait/busy the arrow. Linux and Windows are unchanged.
- macOS: the same Qt 6.11 fault (QTBUG-147602) could crash the app wherever
  one of its own pictures -- an icon, a gobo, a help picture -- was handed to
  AppKit, since 508 of the 660 bundled PNGs carried a colour profile. The
  profiles are removed (the pixels are unchanged, and the app never used
  them), and the test gate now fails if one comes back.

### Known issues
- **Windows:** on heavy shows (around 90 universes with thousands of
  pixel fixtures) the engine couldn't keep its tick rate. A fix is in
  (the test VM now ticks that show at about 8 ms on average) but it hasn't
  had a long soak on a heavy show yet. Moderate shows run normally.
- **macOS:** over a long run the on-screen app's memory grows with no crash
  or slowdown — about 50 MB an hour in the first 24-hour soak; in the
  second, 312 → 492 MB over the 8-hour real-show phase. Being tracked
  down.
- **Linux:** DMX output timing runs at normal priority unless the system
  grants real-time priority (CAP_SYS_NICE or an rtprio limit).
- The universal Mac app has no OLA plugin.
- Not yet tried against the real thing: QLab / MIDI Show Control end to
  end, Art-Net trouble reporting on a failing node, and opening a show's
  carried definitions on a machine without them.

[0.2.1]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.1
[0.2.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.0
[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
