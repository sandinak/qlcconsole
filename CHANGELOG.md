# Changelog

All notable user-facing changes to qlcconsole are documented here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This is the fork's own changelog — for the detailed development log (with
per-feature build notes and deferred follow-ups), see [DONE.md](DONE.md) and
active work in [TODO.md](TODO.md).

Versions here are independent of the QLC+ base version embedded in the
in-app build string (see [RELEASE.md](RELEASE.md) for how the two relate).

## [0.1.0] - Unreleased

First public release. qlcconsole is a fork of
[QLC+ 4.14.2](https://github.com/mcallegari/qlcplus), rebuilt around a
console-first programmer-mode workflow for macOS.

### Added
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
- **Real-time DMX output timing (macOS)** — the DMX timer thread now
  requests real-time scheduling from the OS, fixing missed output ticks
  under scheduling pressure on a busy machine; Linux/Windows equivalents
  are in progress.
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
  the device to run. Tested against a stand-in so far, not yet a real WLED
  on a show network.
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

### Changed
- Versioning scheme: the in-app build string now tracks the QLC+ base version
  this fork tracks, plus a git-derived build count
  (`4.14.2+qlcconsole.<N>`); public releases additionally get a SemVer tag
  (see [RELEASE.md](RELEASE.md)).
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

### Fixed
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

[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
