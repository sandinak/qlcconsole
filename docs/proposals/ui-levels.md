# Essentials and Everything — what a beginner sees

**Status:** On the back burner (2026-10-13). Branson is rethinking the
approach -- hide or not, a wizard or something else -- so the levels below
are a record of what's there, not a plan to build.
**Owner:** Branson / Claude
**Related:** NEXT_SESSION.md item 2

## The ask

Branson: *"students may be intimidated by SOOO many options."* The direction
agreed so far:

- **One app, one show format.** No "lite" build; a show made in Essentials
  opens with everything in Everything, and the other way round.
- **View → Level: Essentials / Everything**, offered on first run, changeable
  any time.
- **Every panel and control is tagged with a level.** Essentials hides what
  isn't tagged E.
- **An advanced setting that is in use still shows in Essentials**, marked
  (say a small "advanced" badge), so a student opening a show someone else
  built never has a setting acting on the show that they can't see. The
  "In use when" column says how each one is detected.
- **Guided first steps:** patch → look → cue → play.

## How to mark this up

Every control below has a **Level**:

- **E** — Essentials: shown to everyone.
- **A** — Everything only: hidden in Essentials unless it's in use.
- **—** — read-only information, or follows its parent; nothing to decide.

Change any letter you disagree with, and write anything else in **Notes** at
the end of a section. Where a whole panel or menu is A, its rows are A too
unless marked otherwise. My proposals assume a student's first show: a few
fixtures, one or two universes, some looks, a cue list, run it.

## The tabs

| Tab | Level | Why |
|---|---|---|
| Connections | E | Output has to go somewhere. Essentials shows the Devices view only. |
| Fixtures | E | Patching. |
| Lighting Studio | E | Placing fixtures is how aim works; Essentials keeps the basics. |
| Programming | E | Looks and scenes. |
| Shows | A | The timeline is a second way to run a show; the cue list is enough to start. |
| Virtual Console | E | Where the show is played from (a cue list, a few buttons). Its Add menu is cut down. |
| Simple Desk | E | Checking a fixture channel by channel is the first thing to reach for when something's wrong. |
| Functions (window) | A | The classic function manager; Programming covers a first show. |

Notes:

## Main window

### Toolbar

| Control | What it does | Level | In use when |
|---|---|---|---|
| Stop ALL (hold: fade and stop) | Panic: stop everything | E | |
| Blackout | All output to black | E | |
| Blind | Rig dark, preview live (Design only) | A | it's on |
| Operate / Design | Switch mode | E | |

### Footer

| Control | What it does | Level | In use when |
|---|---|---|---|
| Ready / Not ready | Readiness; click for the list | E | |
| GM fader | Grand master | E | |
| Speed masters | Tempo per master; click for the window | A | a master is used |
| MTC chip | Timecode status and menu | A | MTC follow on |
| Load | Engine time per tick | A | amber or red |
| Power | Estimated draw | A | circuits set up |
| Dangle | A marked dark fixture no cue lights | A | shown |
| Timeline | Timeline driving / suspended | A | shown |
| Selection, Pad mode, Show Lock | Status chips | — | |
| DESIGN / OPERATE | Mode; click to switch | E | |
| Blackout, Programmer dirty, Saved | Status chips | — | |

### File

| Control | What it does | Level | In use when |
|---|---|---|---|
| New, Open, Open Recent, Save, Save As, Quit | Files | E | |
| Import… | Bring in parts of another show | A | |
| Settings | Application Settings | E | |

### View

| Control | What it does | Level | In use when |
|---|---|---|---|
| Rig Overview… | 3D view of the whole rig | E | |
| Theme ▸ | Colour themes | E | |
| Toolbar Style ▸ | Icons / text | A | not the default |
| Show Engine Load / Power / Grand Master / Speed Masters in Footer | Footer chips | A | |
| Speed Master Beat in Footer ▸ | Blink / bar / none; always / while playing | A | |
| Address Tool | DIP-switch calculator | E | |
| **Level ▸ Essentials / Everything** (new) | This proposal | E | |

### Fixtures

| Control | What it does | Level | In use when |
|---|---|---|---|
| Fixture Editor… | Edit fixture definitions | A | |
| Import / Export fixtures… | Fixture lists to and from files | A | |
| Remap fixtures… | Move a show onto a different rig | A | |
| Patch List… | Every fixture's address; CSV / PDF | E | |
| Channel Behaviour & Modifiers… | HTP/LTP, fades, curves | A | any modifier or forced HTP/LTP set |

### Control

| Control | What it does | Level | In use when |
|---|---|---|---|
| Operate / Design, Blackout, Stop ALL | As the toolbar | E | |
| Blind | As the toolbar | A | on |
| Show-mode Lock | Locks out accidental edits | A | on |
| Dump DMX values to a function | Capture output into a scene | A | |
| Live Edit ▸ (5 items) | Edit running functions / VC | A | capture on |
| Follow MIDI Time Code | Show chases MTC | A | on |
| Exit Timeline Control | VC owns the rig | A | on |
| Hold Last Look / Clear Held | Stopped show holds its look | A | on |

### Window and Help

| Control | What it does | Level | In use when |
|---|---|---|---|
| Tab jumps ⇧⌘1…8 | Go to a view | E | |
| Put Back / Move to Its Own Window, Shift-click option | Break out views | A | a view is broken out |
| Minimize | | E | |
| Help menu (all) | Guides | E | |

### Settings dialog

| Control | What it does | Level | In use when |
|---|---|---|---|
| Autosave on, interval | Recovery copies | E | |
| Tab labels | Duplicates View → Toolbar Style | A | |
| Effect script editor | In-app or external | A | external |

Notes:

## Connections

Essentials: the **Devices** tab only; Overview and Detailed are A.

### Devices

| Control | What it does | Level | In use when |
|---|---|---|---|
| By universe | Universe-ordered list | E | |
| Show unused… | Idle protocols / universes | E | |
| Rescan | Look for devices now | E | |
| Undo Patch Change | Undo the last patch edit | E | |
| Add Universe | | E | |
| Patch a universe to this port / Patch to… / Patch a universe here | Wire output | E | |
| Rename universe… | | E | |
| Unpatch from this interface | | E | |
| Delete universe entirely… | | A | |
| Change target address / port… | Output IP, Art-Net universe | A | set |
| Retarget N selected / Reroute N universes | Bulk moves | A | |
| Add a protocol ▸ / Add an interface… / Stop showing this interface | Pin lines | A | a pinned line |
| Add a target… / from device library… / Name this target / Add a port / Forget | Manual unicast nodes | A | manual targets exist |
| Name this interface… | Alias | A | an alias |
| Configure <plugin>… / Configure interface… | Plugin settings | A | |
| MIDI output mode ▸, MIDI input channel ▸ | MIDI patch params | A | not default |
| Art-Net input universe…, Transmit mode ▸ | Art-Net params | A | not default |
| Passthrough | Input straight to output | A | on |
| Feedback to ▸ / Change feedback address / Use this MIDI port for feedback | Controller feedback | A | a feedback patch |
| Input profile ▸ | Controller mapping | A | a profile set |
| Output view (512 channels) | What a universe sends | E | |

### Overview, Detailed, Input Profile Editor

All A (the Detailed tab and the profile editor are stock QLC+ and for input
devices and audio). In use when: a universe has an input, feedback or
passthrough (the Overview shows those column groups only then already).

Notes:

## Fixtures

| Control | What it does | Level | In use when |
|---|---|---|---|
| Search fixtures… | Filter | E | |
| Add fixture… | Add at an address | E | |
| Add to inventory… / Patch Inventory… | Design first, patch later | E | |
| Add RGB panel… | Pixel panel wizard | A | |
| Channel Groups tab, Add Channel Group… | Stock channel groups | A | any exist |
| Tree: Universes, Inventory, Fixture Groups | | E | |
| Tree: Power | Circuits | A | sources exist |
| Tree: Address clashes, Duplicate names | Problems | — | shown only when present |
| Rename (F2), double-click for properties | | E | |

### Fixture right-click

| Control | What it does | Level | In use when |
|---|---|---|---|
| Fixture Properties… | | E | |
| Definition & Mode… | Change model or mode | E | |
| Edit Fixture Definition… | Open in the Fixture Editor | A | |
| Patch… / Return to Inventory… | | E | |
| Test Fixture…, Locate | Find it on the rig | E | |
| Reset | Send the definition's reset | A | |
| Group ▸ Create Group from Selection / Add to / Remove from | | E | |
| Group ▸ Move to Folder / Remove Group (keep fixtures) | | E | |
| Group ▸ Rebuild Composite from Members | Sub-group blocks | A | a composite group |
| Power ▸ | Circuits | A | assigned |
| Channel Behaviour & Modifiers… | | A | set |
| Delete… | | E | |
| Add Power Source / Add Circuit (empty area) | | A | sources exist |
| Number the Duplicates, Patch the Clashing Fixtures | Fixes | E | |

### Head-layout grid editor

| Control | What it does | Level | In use when |
|---|---|---|---|
| Name, Width, Height | | E | |
| Fill Right / Left / Down / Up, Remove | | E | |
| Add group as block…, Move sub-group | Composite layouts | A | sub-groups exist |

### Fixture Properties

| Control | What it does | Level | In use when |
|---|---|---|---|
| Name, DMX universe / address, Patch… | | E | |
| Mounted on (read-only), Position | | — | |
| Colour (gel) | | E | |
| Orientation | Upright / hung | E | |
| Face, Placement, Angle, Tilt | Fine placement on a structure | A | not default |
| Light, Glass, LED count | How the rig draws its light | A | not default |
| WLED: Fix on Device… | | — | shown only for WLED |
| Max draw, Profile…, Measure…, Copy to Same Fixtures… | Power limiting | A | a maximum or readings set |
| Height offset, Aim — front of device | Aim calibration | A | not 0 |
| Pan / Tilt offset, Invert pan / tilt | Calibration | A | set |
| Test orientation + Test, Locate | Check a mover | E | |
| X / Y, Icon rotation | | E | |
| Identify / Reset | | A | |

### Add Fixture

| Control | What it does | Level | In use when |
|---|---|---|---|
| Model tree + search, Name, Mode, Universe, Address, Quantity | | E | |
| Address gap | Space between fixtures | A | not 0 |
| Channels (generic) | | E | |
| Connection: DMX / Network / WLED, IP address | | A | a network fixture |
| Address Tool | | E | |

### Patch dialog

| Control | What it does | Level | In use when |
|---|---|---|---|
| Table (mode, universe, address per row), Apply, Re-plan | | E | |
| Place selected at universe / address + how + order, Place | | E | |
| Place by nearest node | Needs network nodes on the plot | A | nodes placed |
| Patch the others like + Match | Rows of like structures | A | |
| Carry on into the next universe / Fit by changing modes | Shown only when something doesn't fit | — | |

### Other

| Control | What it does | Level | In use when |
|---|---|---|---|
| Patch List (Copy, CSV, PDF) | | E | |
| Remap Fixtures dialog | | A | |
| Power distribution pane and its dialogs | Sources, circuits, auto-assign, power check, venue files | A | sources exist |
| Power profile / Measure wizard | | A | readings set |

Notes:

## Lighting Studio

### Toolbar and footer

| Control | What it does | Level | In use when |
|---|---|---|---|
| Edit Plot / Plot Locked | | E | |
| Inventory panel toggle | | E | |
| Undo, Remove selected | | E | |
| Add ▸ Fixture, Truss, Platform/Riser | | E | |
| Add ▸ Steps | | A | steps exist |
| Add ▸ Scenery ▸ (9 kinds), Fountain…, Image | Set pieces | A | any exist |
| Add ▸ Network Node | | A | nodes exist |
| Add ▸ Target Position | Aim targets | E | |
| Add ▸ Person | | A | a person target |
| More ▸ Group / Ungroup, Select All, Copy / Paste | | E | |
| More ▸ Set background colour | | A | not default |
| More ▸ Network Topology… / Network Report… | | A | nodes exist |
| More ▸ Patch List… | | E | |
| Layers handle | Layers sidebar | A | more than one layer |
| Grid toggle, stage size, units | | E | |
| Grid subdivisions, Snap + Snap to | | A | snap on |
| Show ▸ Rulers, Fixture labels | | E | |
| Show ▸ Fixtures on the rig, Stage centre lines | | A | |
| Show ▸ Where heads CANNOT reach / Light footprint | Coverage overlays | A | |
| ⌖ 0,0 origin menu | Where rulers read zero | A | not the default |
| Overlay (Normal / Power / DMX / Network / Stage only) | | A | not Normal |
| View Top / Front / Side | | E | |
| ⟳ rotation | Turn the plot 90° | A | rotated |
| N fixture(s) off stage | | — | shown only when needed |
| DMX-grid mode (its toolbar) | Channel grid view | A | |

### Right-click on empty plot

| Control | What it does | Level | In use when |
|---|---|---|---|
| Add Here ▸ Fixture, Truss, Platform, Target | | E | |
| Add Here ▸ Boom, Electric, Stand, Tower | Structures only in this menu | E | |
| Add Here ▸ Steps, Scenery, Fountain, Person, Image, Network Node | | A | any exist |
| Group / Ungroup, Delete, Duplicate, Copy, Paste here | | E | |
| Rotate…, Align ▸, Distribute ▸ | | E | |
| Browse Studio Components… | | A | library not empty |

### Right-click on a fixture

| Control | What it does | Level | In use when |
|---|---|---|---|
| Fixture Properties…, Locate, Patch… | | E | |
| Edit the structure it hangs on | | E | |
| Mount on ▸ Truss / Boom / Tower / Deck, Unmount | | E | |
| Mount on ▸ Hanger | | A | hangers exist |
| Set Mounting Height… | | E | |
| Facing (pan zero) ▸ | | A | not Downstage |
| Select All Similar | | E | |
| Split by Head Type… | | A | |
| Gobo Wheels… | Custom gobo glass | A | a swap set |
| Show Light Footprint | | A | |
| Studio Group items (Edit / Create / Stamp Component / Lock) | | A | a studio group |
| Remove from View | | E | |

### Right-click on a structure or mark

| Control | What it does | Level | In use when |
|---|---|---|---|
| Edit…, Rename…, Delete | | E | |
| Add ▸ Fixture / Bar / Crossbar / Target Here / Person Here | | E (Person A) | |
| Patch Others Like This (N)… | | A | |
| Show Light Footprints | | A | |
| Lock / Unlock Layer, Move to Layer, New Layer from Selection | | A | more than one layer |
| Show on Map / in Layers | | A | |
| Save as Gear Model… (network node) | | A | user models exist |
| Fixture Layout… / Create Fixture Layout…, Ungroup | | A | a layout |
| Folders (New / Remove from) | | A | folders exist |
| Create Fixture Group from Selection…, Remove from group | | E | |

### Layers panel, Inventory panel

| Control | What it does | Level | In use when |
|---|---|---|---|
| Layers panel (all) | | A | more than one layer, or one hidden or locked |
| Fixture-group rows: Lay Out from the Stage…, Split by Head Type…, Create from N Groups… | | A | |
| Inventory panel: Filter, Add to inventory…, drag onto the plot | | E | |

### Structure editors

| Editor | Level | Notes / in use when |
|---|---|---|
| Truss: name, position, height, direction, length | E | |
| Truss: profile, on stand / other end on stand, sock and its pattern, bar on truss, face, stand-off | A | not default |
| Platform: name, position, size, height | E | |
| Platform: front curve, steps fields, colour, top, solid, stackable | A | not default |
| Boom / Electric / Stand | E | |
| Boom: on a stand, clamp part-way, hang from truss | A | set |
| Tower: size, position, shelves | E | |
| Tower: faces, sides, printed pattern | A | set |
| Hanger, Scenery, Fountain, Image | A | any exist |
| Group (frame), Studio Group Editor, Components browser | A | a studio group |
| Studio pane: View, Locked, Put on Face, Distribute, Add Bar | E | |
| Studio pane: Angle ▸, ⟳ | A | |

### Rig Overview

| Control | What it does | Level | In use when |
|---|---|---|---|
| View, Live, Beams | | E | |
| Room, Haze | | E | |
| Quality, fps | | A | not default |

### Stage targets, network, power

| Control | What it does | Level | In use when |
|---|---|---|---|
| Edit Target: name, X / Y / Z, colour | | E | |
| Edit Target: subject (person / drum kit / bound / area), follows, aim height override, size | | A | set |
| Add Network Node, Network Topology, Network Report | | A | nodes exist |
| Power Distribution dialog | | A | sources exist |
| Fixture Group from the Stage dialog | | A | |

Notes:

## Programming

### Toolbar

| Control | What it does | Level | In use when |
|---|---|---|---|
| Highlight, Flash | Find the selected fixtures | E | |
| Park ▾ | Hold fixtures out of the cues | A | anything parked |
| Move in black ▾ (mark, auto, lead) | Pre-position dark movers | A | marks or auto on |
| Beat ▾ Internal beat, BPM | | E | |
| Beat ▾ Speed masters… | | A | a master is used |
| Tap | | E | |
| More ▾ Snapshot live output | | A | |
| More ▾ Jog speed | | A | not default |
| Save Positions | Keep joystick edits | E | |
| Followspot settings | Bind a joystick | A | bound |

### Functions tree

| Control | What it does | Level | In use when |
|---|---|---|---|
| Filter, double-click to open | | E | |
| Add ▾ New Scene, New Chaser, New Folder | | E | |
| Add ▾ New Collection, New Show | | E | |
| Add ▾ New EFX, New RGB Matrix | Stock QLC+ effects | A | any exist |
| Right-click: Duplicate, Delete, New… | | E | |
| Learn / Clear MIDI Trigger… | | A | a trigger set |
| Group types by role | Tree order | A | on |

### Palettes, Bundles, Fixtures & groups

| Control | What it does | Level | In use when |
|---|---|---|---|
| Palettes: Filter, drag onto a look | | E | |
| New Color, Dimmer, Aim, Effect | | E | |
| New Pan/Tilt, Beam, Gobo, Shutter, Strobe | | E | |
| New folder…, Move to folder…, Duplicate, Delete | | E | |
| Save as Bundle… | | A | |
| Bundles tab (all) | Shareable palette sets | A | user bundles exist |
| Fixtures & groups: search, drag onto Targets | | E | |
| Double-click a group: its head layout | | E | |
| Rename universe…, Create / Rename / Move / Delete group | | E | |

### Scene canvas

| Control | What it does | Level | In use when |
|---|---|---|---|
| Fade in / out | The scene's fade | E | |
| Transition in / out (shape, time) | Wipes, dissolves | A | a transition set |
| Edge | Transition softness | A | not default |
| Speed | What its effects keep time with | A | not "As played" |
| Scope | Intent-only label | A | set |
| Targets: drop, Remove, warnings | | E | |
| Looks: drop, order ▲▼, Remove | | E | |
| Looks: Fade In / Fade Out cells, Reset to step | Per-look fades | A | any set |
| Looks: Feed effect ▸ / Move to fixtures | Nest a colour under an effect | A | nesting used |
| Stack-checker line | | — | |
| How this works | | E | |

### Look editor

| Control | What it does | Level | In use when |
|---|---|---|---|
| Name, used by N scenes | | E | |
| Fade in / out (per look) | | A | not "scene" |
| Color: picker, R G B, W A UV | | E | |
| Dimmer bar, Strobe rate | | E | |
| Pan/Tilt pad + presets | | E | |
| Aim at | | E | |
| Beam: Focus, Frost, Iris | | E | |
| Gobo / Shutter: picker | | E | |
| Gobo / Shutter: Wheel of, raw Value | | A | |
| Effect: Pick / Change effect…, ? | | E | |
| Effect: parameters | | E | |
| Effect: Spread (amount, axis, direction, wings, blocks) | | A | amount not 0 |
| Effect: Speed | | A | set |
| Effect: One-shot length | | A | set |
| Effect: Palette Bindings, Target Bindings | | A | bound |
| Effect: Input bindings + Bind, Learn range | MIDI / OSC control | A | bound |
| Effect: Persist across restarts | | A | on |
| Effect: New script…, Edit script…, Import…, Export…, Save as Effect… | Authoring | A | |

### Effect browser, Speed Masters

| Control | What it does | Level | In use when |
|---|---|---|---|
| Effect browser: search, tabs, list, info, Done, Cancel | | E | |
| Effect browser: WLED tab | | A | WLED fixtures in the show |
| Speed Masters window (all) | | A | a master is used |

Notes:

## Shows (whole tab A)

| Control | Level | In use when |
|---|---|---|
| Show ▾, shows combo, Edit ▾, Play, time readout | A | a show exists |
| Length ▾, Time division + BPM, Snap to Grid | A | set |
| Follow MTC, TC offset ▾ | A | on |
| Tracks: Mute, Solo, Lock, Intensity, right-click (solo-safe, hold last, priority) | A | set |
| Markers lane | A | markers exist |
| Sequence / audio / video items | A | |

Notes:

## Functions window (A)

New scene / chaser / sequence / EFX / collection / RGB Matrix / script /
audio / video / folder, Startup Function, Function Wizard, Palettes…,
Clone, Delete. In use when: a startup function is set, or functions of a
type Programming doesn't make exist (script, audio, video).

Notes:

## Virtual Console

| Control | What it does | Level | In use when |
|---|---|---|---|
| Add ▾ Button, Slider, Cue list, Frame, Label | | E | |
| Add ▾ Button Matrix, Slider Matrix, Knob, Speed Dial, XY pad, Solo frame, Audio Triggers, Clock, Animation, Show control, Programmer Frame | | A | any exist |
| Edit ▾ Cut, Copy, Paste, Delete, Properties, Rename | | E | |
| Edit ▾ stacking, colours, background image, font | | A | |
| VC Fixture Widget Wizard, VC Settings | | A | |

Notes:

## Simple Desk

| Control | What it does | Level | In use when |
|---|---|---|---|
| Universe, pages, faders, Reset universe | | E | |
| "Holding N channels" banner | | — | |
| View mode | | A | |
| Cue Stack tab, Channel groups tab | | A | a cue stack recorded |

Notes:

## Found on the way: things nothing opens

Not levels, but worth a decision while we're here:

- **Preferences dialog** (`preferencesdialog.cpp`) — Autosave backups and
  Locate-flash colour/intensity; compiled in, never opened.
- **Manage Trusses** (`Monitor::slotManageTrusses`) — no callers.
- **Riser face editor** (`riserfaceeditor.cpp`) — never constructed.
- **Old fixture properties form** (`monitorfixturepropertieseditor.ui`) —
  replaced by Fixture Properties.
- The main **toolbar's right-click** is set up but does nothing.
- **Settings → Tab labels** duplicates **View → Toolbar Style**.

Remove them, wire them up, or leave them?
