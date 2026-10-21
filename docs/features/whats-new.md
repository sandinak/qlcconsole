# What's new

The latest changes, newest first. Each links to the guide with the detail.

## 0.2.2 (next update)

- **The version on the boot window.** While qlcconsole starts, its window
  shows the release — "qlcconsole 0.2.2" — and below it the build and
  commit, so you can tell which version is starting.
- **No false warning for a curtain.** A show with a stage curtain no longer
  lists it under "fixture types declare no light output": a curtain is not
  a light. Hazers, smoke machines and fans with no light of their own are
  left out too; a real light with no output set is still listed.

### The Programming tab's outline

- **Every scene and what's in it, in one tree.** The left of the
  Programming tab is now an outline: folders, scenes, and under each scene
  its targets and looks. Click a row and the middle edits it — the Look
  Editor gets the whole height. Drag a row to another scene to move it (⌥
  copies it), open everything with ⊞, and ⌘Z / ⇧⌘Z undo and redo scene
  edits with everything else. See
  [The outline](programming-tab.md#the-outline).

### Definition updates

- **Old definitions, brought up to date.** **Fixtures → Definition
  Updates…** lists your fixture definitions that are older than the
  current version of the same fixture — the included one, or a newer copy
  of your own — the ones the open show uses first. Each says what an
  update changes (modes and channels added, removed or changed), and
  **Update from Current**, **Update All**, **Keep Mine** or **Ignore This
  Version**. Every update keeps the old files in a dated backup, and **Undo
  Last Update** puts them back. A copy someone changed on purpose is never
  called old. The footer says when there are any. See
  [Definition Updates](definition-updates.md).
- **Synced QLC+ folder.** Links into the old QLC+ folder are handled
  explicitly: update the synced original (every computer gets it), or
  replace the link with a file of its own. Old originals left behind there
  are found too.
- **Old copies inside a show.** **File → Show Definitions…** lists what a
  .qlcc carries and how each compares with this computer's — which is
  newer (by the version that saved it, then the dates), and what changes.
  **Use This Computer's** updates the show's copy, each fixture keeping its
  mode by name; a changed channel layout names the fixtures and functions
  it touches and asks first; **Undo** goes back in one step. When the
  show's copy is the newer one, **Install the Show's** puts it in your
  folder. See [Old definitions inside a show](definition-updates.md#old-definitions-inside-a-show).

### LED Layout: a pixel fixture as it is built

- **LED Layout…** on a fixture's right-click menu (Fixture Manager, the
  Lighting Studio plot and Layers tree, the Rig Overview) and in Fixture
  Properties opens the layout of its model: every head in its cell,
  numbered, coloured by kind — RGB rows a rainbow in head order, white rows
  white. Type the rows (`RGB, W, RGB, W, RGB` for an XL-450), click to swap
  or move heads, flip, resize the grid, and **Preview Order** walks the
  heads one by one — on the real fixture too — to check it. No group is
  made. **Save to Show**, and **Save as Default for This Model** to use it
  in new shows; a show's own layout still wins. Matrix effects, groups by
  head type, the plot and the Rig Overview all draw from it. See
  [LED Layout](fixture-manager.md#led-layout).

## 0.2.1 (October 2026)

### Focus mode: the controller on screen

- **A map of the controller.** The Focus mode panel now draws the
  controller, each control labelled with what it does there — Next head on
  RB, Save on A, Nudge on the left stick, Fine on the right, the step on the
  D-pad. Buttons light while held and the sticks show where they are, so
  the mapping can be checked on the real pad. The labels come from the
  controller profile's roles: another make of pad shows its own names, and
  what it lacks is greyed. Click a control to do it.
- **Controller or keyboard.** **Use: Controller | Keyboard**, in the setup
  and the panel. It starts on the controller when one is connected,
  otherwise the keyboard, and remembers your choice. On the keyboard the
  map shows the keys and the controller is ignored, so a resting pad can't
  drift a head; the keys work with either. A controller unplugged
  mid-session hands over to the keyboard, and is offered back when it
  returns. See [Controller or keyboard](lighting-studio.md#controller-or-keyboard).

### Shutters open by themselves

- **Auto-open shutters.** A spot whose shutter rests closed now lights
  from a level alone: give it White at Full and it opens its shutter, as a
  lighting console does. A shutter look (Closed, Strobe...) still wins, and
  a light with no level stays as it is. It is on for every show, saved
  with the show, and turned off in the new **Settings → Advanced** tab,
  which also now holds the effect script editor choice and the Show control
  (QLab) button. See [Auto-open shutters](other-improvements.md#auto-open-shutters).

### The window

- **One top bar instead of two.** The open tab's own tools and the show
  controls share one row: the tab's tools on the left (**»** holds what
  doesn't fit in a narrow window), and on the right **Stop all**,
  **Blackout**, **Blind** and a **Design | Operate** switch with the mode
  you are in lit. Blackout lights red and Blind blue while they are on.
  About 57 px more for the tab on every tab. **Settings → Show Controls**
  can move the show controls to the status bar instead; it changes at
  once. See [Finding your way around](getting-started.md#finding-your-way-around).

### Effects

- **Confetti on a two-arm beam, and two new look warnings.** A dimmer
  effect on a fixture whose one master dimmer serves several heads takes
  its brightest head, so a beam with one lit arm lights (it went black).
  The Looks list now marks a Colour look that is all zero (**Black**), and
  a position look on fixtures that can't move because their definition
  marks every Pan or Tilt channel Fine (**Can't move**). See
  [The stack checker](effects.md#the-stack-checker).

### Show control

- **Sub-cues from QLab.** `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play a
  sub-cue (its cue first); OSC `/subgo` and `/subback` step the cue
  playing; LOAD 4.2 puts one on standby and **Next:** shows it. A GO by
  number on a paused list now plays that cue (it used to resume where it
  was). See [Sub-cues from QLab](show-control.md#sub-cues-from-qlab).

## 0.2.0 — Show Tree (October 2026)

### Software updates

- **Update check and install.** qlcconsole checks GitHub Releases once a day
  in the background, and from **Help → Check for Updates…**. A newer
  version shows as a footer chip, **Update available: 0.2.0**, never as a
  pop-up. Its window shows the release notes, with **Install**, **Later**
  and **Skip This Version**. On macOS and Windows, Install downloads the
  update and checks it against the release's SHA-256 list (and, on macOS,
  its signature). It then installs when you quit, or on **Restart and
  Install**, reopening the same show. It never installs in Perform mode or
  while anything is running. Settings → Software Update has the channel
  (Stable, or Pre-releases such as Show Tree) and **Install updates
  automatically**. IT can turn it all off. On Linux and the Pi, it only
  tells you. See [Software updates](updates.md).

### Show Tree

- **Movers with a colour ring.** A ring of LEDs round a mover's lens is a
  head type of its own: group by head type makes **Movers** and **Ring**
  groups, and a Colour look on each sets only the wheel or only the ring
  (on both, each its nearest slot: black is the ring's off, any colour the
  nearest hue). The ring's chases are picked by name — the Look Editor's
  **Wheel / ring** list, or `Ring colour Red/Yellow-Chase`. The 2D plot
  and the Rig Overview draw the ring, live, chases stepping round.
  **Change Definition…** moves fixtures to a drop-in definition with the
  ring as its own head. See [Colour rings](show-tree.md#colour-rings).
- **Several productions, sub-cues in the tree, and the cue runner.** A show
  can hold several **productions**; the **current** one (the header's
  **Production:** switcher, or the footer's GO menu) is what the footer GO,
  ⌘↩, QLab / OSC / MSC and the runner play -- its **main list**, with a
  **timecode** source of its own if it needs one. A cue gets **sub-cues**
  4.1, 4.2… with **+ Sub-cue**: each with its own layers, fade and **Sub GO**
  or **Auto-follow**; they track from one another and leave the next cue
  alone. The new **Run** tab (⇧⌘0) is the operator's view: the cue list
  with ▶ / STANDBY / NEXT / SUB NEXT, a *what changes* line from the
  tracking ("Towers → Red; BackStage in → Green"), the timecode, LIVE /
  DESIGN and big GO, Sub GO, Back, Sub Back, Pause and Stop buttons (Space
  is GO); click a cue to LOAD it, double-click to jump. The **Cue Runner**
  VC widget is the same runner on a page (compact or list), and **Swap
  Widget** turns a Cue List into one. OSC gains `/subgo` and `/subback`, and
  sub-cues answer to their numbers. Typed: `production …`, `main`, `sub …`,
  `follow`, `subgo`. See [Productions](show-tree.md#productions),
  [Sub-cues](show-tree.md#sub-cues) and
  [The cue runner](show-tree.md#the-cue-runner).
- **Looks (busking) and palettes in the Library.** A production now has a
  **Looks** section beside its cue lists: looks built with the same layers
  and rows as cues, which never track and play from VC buttons over
  whatever runs. **Make VC page** puts a button for each on the Virtual
  Console; **Adopt as look** turns an existing scene into one; **Copy to
  look** freezes a cue. The Library's **Palettes** gain the everyday work —
  **+ Palette** by type, rename with **F2**, folders that hold any type,
  drag to file, duplicate, delete that names every cue, look and VC button
  using it — on one **Undo** with the tree (and with the Programming tab's
  palette list). Typed: `palette …`, `look …`, `vcpage`, `adopt`. See
  [Looks](show-tree.md#looks-busking) and
  [Palettes in the Library](show-tree.md#palettes-in-the-library).
- **Show Tree P4: reusable layers and cues, Convert to tracking, Detach a
  list.** Right-click a layer or a cue → **Make reusable…**: it goes into
  the Library (All → Reusable) and can be dropped into any cue of any list
  — a "BackStage" wash in ten cues, a "Walk-in" state in every song. Every
  use stays linked: change one and they all change; **Unlink here** makes a
  copy of its own. **Convert to tracking…** on an imported list takes out
  what only repeats the cue before, turns "includes the cue before" into
  tracking and shows you the changes first — every cue plays exactly as
  before. **Detach to plain chaser…** hands a list back as an ordinary
  chaser. See [Reusable layers and cues](show-tree.md#reusable-layers-and-cues),
  [Convert to tracking](show-tree.md#convert-to-tracking) and
  [Detach to plain chaser](show-tree.md#detach-to-plain-chaser).
- **Show Tree P3: group sets, child layers, cue-wide effects, line aims.**
  Make a **group set** (Towers = SL + SR) once in the Library and use it by
  name anywhere; **⇧-drop** a group onto a layer for a **child layer** that
  sets only what it sets — the parent never shows through it. An effect on
  the cue runs as **one effect across** its layers; a line target on the cue
  aims them all as **one formation**, numbered by where the lights hang, with
  **Spread, Grouping and Cross** rows of their own. Rename a row in place
  with **Return**, copy and paste cues with **⌘C / ⌘V**, and type **`help`**
  in the command line for every word with examples. See
  [Group sets](show-tree.md#group-sets), [Child layers](show-tree.md#child-layers),
  [Line aims](show-tree.md#line-aims) and [Help](show-tree.md#help).
- **Show Tree: Library | Tree | Editor, and adding without dragging.** The
  Library is on the left now, beside the tree. **Return** or a double-click
  in the Library adds what is selected to the tree's selected cue or layer;
  **⌘F** (or **/**) jumps to the filter, type and press Return. A drop below
  the tree's last row is a new layer in the selected cue. A named layer
  is just its name in the tree; its page says what it holds. Select several
  layers and edit them together — Release, a colour, a dimmer, all in one
  step. See [Adding layers](show-tree.md#adding-layers) and
  [Editing several layers](show-tree.md#editing-several-layers).
- **Write a show as one tree.** A new **Show Tree** tab beside Programming:
  productions, cue lists, cues, and in each cue the layers — a group and the
  looks it plays. Drag groups onto a cue for layers and palettes onto a layer
  (or onto the cue, for every layer in it); values that come from the cue or
  an earlier cue show grey with where they came from. The scenes, collections
  and the chaser that play it are made and kept up to date for you, and a
  layer that stays the same from one cue to the next keeps running through
  GO (no re-fade, effects keep their place). **Import chaser…** turns a
  chaser you have into a cue list that plays exactly as it did. See
  [Show Tree](show-tree.md).
- **See the cue you're editing.** In Design mode, selecting a cue in the
  Show Tree plays it on the rig and every edit shows at once; in Live it
  stays blind until GO. Several groups dropped together make one layer (⌥ for
  a layer each), a group dropped on a layer joins it, and the Library shows
  groups in the Fixture Manager's folders. See
  [Seeing it on the rig](show-tree.md#seeing-it-on-the-rig).
- **Show Tree: Record which stage, split / merge, values that reuse
  palettes.** Record asks whether to record the selected cue, the running
  cue or everything lit when they differ (or does what Settings → Show Tree
  says). Several groups make one layer everywhere — drop, + Layer, command
  line — and a layer each with ⌥ or a comma (`Towers, USTs`); right-click
  to **Split** a layer per group or **Merge** layers. A level or colour you
  type or pick reuses the palette that already plays it; a typed one that
  doesn't exist yet becomes a palette named like yours. See
  [Record and duplicate](show-tree.md#record-and-duplicate) and
  [Values you pick or type](show-tree.md#values-you-pick-or-type).
- **Show Tree: drop on the page, a command line that's always there.**
  The cue page takes drops as the tree does — groups on it make a layer
  (several at once: one layer, ⌥ for one each), onto a layer's row join it,
  even a grey carried-in one — and the drop area and the line at the top
  say what will happen. The command line now sits at the bottom of the tab
  (**:** from the tree), and **Tab** completes every word it takes —
  fixtures, folder paths, functions, cue lists, cue numbers, palettes by
  kind — Tab again for the next. A dimmed RGB bar now shows its colour on
  the Rig Overview (it drew black). See
  [Adding layers](show-tree.md#adding-layers) and
  [The command line](show-tree.md#the-command-line).
- **Gobo looks by name.** A gobo look picked on one fixture type's wheel
  plays the gobo of the same name on every other type's own wheel; types
  without it ignore it. See [Gobos by name](show-tree.md#gobos-by-name).
- **A scene two cues share no longer re-fades on GO.** A cue list stepping
  from one collection to another that holds the same scene (or the collection
  before it) now keeps that scene running instead of stopping and starting
  it.

### Stage targets

- **Line targets.** A new kind of aim target: a curve in space that spreads
  the lights aimed at it evenly along it, stage-left light to the stage-left
  end. Drag its ends to stretch or swing it, double-click it to add a point
  and drag that to bend it, drag the line to move it all. An Aim look on
  a line has a **Spread** slider (-5 crossed .. 0 all on the centre .. +5
  end to end) and **Group / Cross** for halves or pairs that swap sides or
  mirror. See [Line targets](stage-targets.md#line-targets).

### Fixture Manager

- **One menu item for one group or many.** Select fixtures and/or groups
  → **Group → Create Group(s) from Selection…**: one group of every head
  (made as the old *Create Group from Selection…* made it — groups
  selected go in as blocks keeping their layout, fixtures off the stage
  fill a grid you can size), a group per head type across them ("Bar · RGB
  pixels" with all 128 RGB pixels of eight bars, "Bar · White pixels" with
  their 32 whites; a par's single head stays "Par · RGB"), or a group per
  head type for each fixture, in a folder. The groups are listed — names,
  head counts, layout — before any is made. It replaces the two separate
  items. See
  [Create Group(s) from Selection](fixture-manager.md#create-groups-from-selection).

### Effects

- **Matrix effects for pixel boxes.** Fourteen effects in a new **Matrix**
  tab — bars, chevrons, ripples, rain, a checkerboard, sparkle, radar, an
  iris, an equaliser, snakes, wipes, colour noise, a box-by-box chase and a
  strobe — drawn across a whole run of boxes as one picture or in each box,
  on the beat where it counts. Every one has a **white** setting for boxes
  with white pixels (the Warmdance XL-450's): the white part of the colour
  plus accents, follow brightness, accents only, RGB only or white only.
  A pixel box in a look with no group is now drawn on as its own grid.
  See [Matrix effects](effects.md#matrix-effects).

### Lighting Studio

- **Beams stop at risers and the floor.** A wash thrown across the stage —
  the TrussW bars over DanceHysteria's steps — ran straight through the
  risers and on below the stage floor in a side elevation. Now a beam stops
  at the first riser, step or platform it meets (its side as well as its
  top), washes the top and face it reaches, and nothing of it shows behind
  a riser or below the floor, in the Studio and the Rig Overview alike. See
  [Beams](lighting-studio.md#beams-footprint-and-coverage).
- **Curtains that open in the show.** A main drape or traveler can be
  **Driven by** a curtain fixture (**Add curtain fixture** in its editor):
  as the fixture's **Open** channel goes from 0 to 255 the drape opens, and
  **Fly** takes a main drape up and out. The plot and the Rig Overview follow
  it while it fades. Use it in scenes, chasers and the Show Tree like any
  fixture; the fade time is how long the move takes. The curtain fixture
  isn't a light, so it draws no power and doesn't show in Inventory. Patch it
  to a curtain controller or relay box to move the real one. See
  [Curtains that move](lighting-studio.md#curtains-that-move).

### Follow spot and aiming

- **Focus mode: align the rig with the controller.** Step through the moving
  heads with the bumpers — each aimed at its reference (the scene's aim, a
  target, a line target's own point), lit and ringed in the views — nudge
  its calibration offsets with the sticks or D-pad until the light lands,
  and save with A (B reverts, X zeroes, Y checks a second reference). The
  keyboard does it all too. From the Lighting Studio, the Rig Overview,
  View → Focus Mode… (⌘⌥F) or View + Menu on the controller. See
  [Focus mode](lighting-studio.md#focus-mode).
- **Calibration offsets stay out of the picture.** A head's pan/tilt
  offsets are in its DMX but no longer in the views: its beam is drawn on
  the target it is aimed at. See
  [Calibration offsets](lighting-studio.md#calibration-offsets).
- **Line of sight: lights go out when they can't see the target.** A follow
  spot's new **Fade When Blocked** fades a head while a closed drape, a
  wall, a riser or a tower stands between it and the subject, and brings it
  back when the way is clear; an Aim look's **Dim lights that can't see the
  target** does the same for any aimed look. Both are off unless you turn
  them on. The Targets panel names what's in the way, the plot draws a
  blocked aim line red to its blocker, and **Show → Line of sight** picks
  what counts and can stop the 3D views' beams at it. See
  [Line of sight](stage-targets.md#line-of-sight).

### Programming

- **Palette folders hold any type.** A song's folder can hold its colours,
  aims and effects together; the Palettes tab shows folders first, or
  **Group by type** (right-click) for type first with the same folders under
  each. Older shows' per-type folders merge on open. See
  [Filing into folders](programming-tab.md#filing-into-folders).

### Outputs

- **Only transmit on the rig network.** A show can name the networks its
  output belongs on (**Settings → Network**, for example 172.18.0.0/20,
  saved with the show). Art-Net, sACN, OSC and WLED output on any other
  network — a laptop at school — is held, kept and sent nothing, and
  resumes by itself on the rig network. The footer says it in red: "Output
  held — not on the rig network (172.18.0.0/20). Connected: en0
  10.4.12.7"; click it for the setting. **Transmit anyway, this session…**
  asks first and is never saved. See
  [Only transmit on the rig network](other-improvements.md#only-transmit-on-the-rig-network).
- **Backup WLED lanterns.** A WLED universe can drive several devices: a
  backup lantern mirrors the primary, nothing programmed twice. Right-click
  the universe → **WLED Devices / Backup Lanterns…** → **Add Backup**. Each
  device is sent to on its own, so a dead one never holds up the others,
  and one that comes back is sent the look again. Connections says "1 of 2
  answering" while one is down. See
  [Backup lanterns](other-improvements.md#backup-lanterns).

## 0.1.0 (October 2026)

### Programming

- **Drag scenes and palettes into folders.** In the Programming tab, a scene
  (or several, or a whole folder) dragged onto a folder moves into it, and
  onto the type's heading or empty space moves back out; pressing on a row
  and moving now always drags it, where before it could rubber-band select
  instead. Right-click a folder to rename or delete it — deleting keeps what
  was in it. See [Filing into folders](programming-tab.md#filing-into-folders).
- **Take things out of a collection, chaser or show from the side tree.**
  Right-click rows listed under one → **Remove from “…”** (the functions
  stay). Delete on those rows now reads **Delete Function Everywhere…** and
  says where else the function is used. See
  [Taking something out](programming-tab.md#taking-something-out-of-a-collection-chaser-or-show).
- **Deleting says where it's used.** Deleting a palette or a function in the
  Programming tab (menu or ⌫) lists every scene look, effect-look slot,
  collection, chaser step and show track using it before you confirm. See
  [Deleting palettes and functions](programming-tab.md#deleting-palettes-and-functions).
- **A look keeps its Speed and Spread when you try another effect.**
  Clicking or double-clicking another effect's tile in the look editor put
  the look back on *As the look* with no spread; now both stay. See
  [Speed masters](effects.md#speed-masters). Closing the app with a speed
  master's name still being typed no longer crashes.

### Fixture definitions

- **Sizes and weights in your own units.** **Settings → Units** picks mm,
  cm, m, inches or feet (weight in kg or lb to match); the fixture
  editor's Physical tab shows width, height, depth and weight in it, and
  takes any unit typed with the number — `7.5"`, `6' 3"`, `19.05 cm`,
  `26.5 lbs`. Files still keep millimetres and kilograms. See
  [The fixture editor](fixture-manager.md#the-fixture-editor).
- **The Studio's grid follows it too** — rulers, readouts, labels and
  length fields in metres (mm/cm/m) or feet (in/ft); the grid's own
  Meters/Feet choice is gone. Shows open in your units and are saved in
  them; if you'd never chosen, the first show you open in feet makes it
  feet. See [Selecting and undo on the plot](lighting-studio.md#selecting-and-undo-on-the-plot).

### Sub-cues

- **Cues inside a cue.** A chase used as a step of a cue list lists its
  steps under it as **4.1, 4.2…**. Set the step's **Sub-cues** to **Sub
  GO** and they wait for **Sub GO** (⌥⌘↩; Sub Back ⌥⌘⇧↩) while GO still
  goes on to cue 5. The chaser editor and the Virtual Console cue list show
  them nested, the cue list highlights the one playing and has Sub GO /
  Sub Back buttons, and the footer says **Sub: 4.2 · Next: 5**. Older
  shows play as before. See [Sub-cues](show-control.md#sub-cues).

### Outputs

- **Patch a universe from the Fixture Manager.** Right-click a universe row
  in the fixtures list: **Patch output…**, or on a patched one the menu says
  where it goes and offers **Change interface / target…** and **Unpatch
  output**; with several selected, **Patch N selected universes to…** /
  **Unpatch N selected universes**, in the order you clicked them. The same
  questions and undo as Connections. See
  [Patching a universe's output](fixture-manager.md#patching-a-universes-output).
- **Patch or unpatch several universes at once.** In Connections, select
  universes and right-click: **Patch N selected universes to…** puts them on
  a device's ports in the order you clicked them (more universes than ports:
  the rest are named and left alone), and **Unpatch N selected universes**
  removes them all in one go — each one undo step. See
  [Connections &amp; patch](other-improvements.md#connections--patch).
- **Pick the port from a list.** Patching a universe to Art-Net (or WLED)
  lists every port of every device on that interface, free ones first and
  held ones saying by whom; a held port is refused. **Change interface /
  target…** asks the interface again, so a universe on the wrong network
  card can move without unpatching. "Port" is now the node's socket and
  "universe" its Art-Net address. See
  [Connections &amp; patch](other-improvements.md#connections--patch).
- **The power estimate is believable.** A fixture whose definition gives no
  power is guessed at 150 W for a mover and 50 W for anything else (it was
  40 W per channel, so a pixel bar read kilowatts), and hovering the footer
  figure explains it: the biggest draws, each measured, rated or guessed.
  See [Power distribution](other-improvements.md#power-distribution).

- **New Art-Net nodes can be found and given an address.** A node still on
  its factory address (2.x.x.x and the like) shows in Connections in amber
  with "set its IP"; **Rescan** now also polls each interface's limited
  broadcast so such a node can hear it; and right-click → **Set node IP
  address…** sets its IP, mask, gateway or DHCP (ArtIpProg), after a
  confirmation. A node no network here can reach gets the command for a
  temporary address instead. See
  [Setting up a new Art-Net node](other-improvements.md#setting-up-a-new-art-net-node).
- **A node's DHCP state reads right.** Connections had "DHCP capable" from
  the bit that says the node is *using* DHCP.
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

- **A closed curtain hides what is behind it.** Lit steps just upstage of
  the closed main drape glowed through it in the Rig Overview — their haze
  glow, and the pools their light made on the back of the cloth. Now the
  drape (or a wall, or a platform) hides them, and the glow shows only
  where the curtain is open. See [Haze](lighting-studio.md#haze).
- **A mover hung on a truss sits where the truss editor shows it** in the
  Rig Overview and the Angled view — just under the bar — instead of half
  a body lower.
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
