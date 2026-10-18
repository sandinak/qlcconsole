# Changelog

All notable user-facing changes to qlcconsole are documented here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This is the fork's own changelog — for the detailed development log (with
per-feature build notes and deferred follow-ups), see [DONE.md](DONE.md) and
active work in [TODO.md](TODO.md).

Versions here are independent of the QLC+ base version embedded in the
in-app build string (see [RELEASE.md](RELEASE.md) for how the two relate).

## [Unreleased]

### Added
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

### Fixed
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
  [EFFECTSCRIPT_API.md](EFFECTSCRIPT_API.md) and
  [EFFECT_LIFECYCLE_DESIGN.md](EFFECT_LIFECYCLE_DESIGN.md).
- **Effect presets** — named, reusable configurations layered over effect
  scripts; see [EFFECTPRESET_DESIGN.md](EFFECTPRESET_DESIGN.md).
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
  show records) and the commit beside it (see [RELEASE.md](RELEASE.md)).
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

[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
