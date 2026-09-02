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
- **Lighting Studio** — a 2D rigging/plot tab: place and rig trusses,
  platforms, pipes, stands, towers, and power sources; drag a fixture onto a
  truss to bind it, with a visual tether line back to the truss and
  select/move-together group behavior; a Layers panel for organizing the
  plot; Top/Front/Side points of view and Power/DMX/Network/Stage-only
  overlays.
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
- **Look scope** — a Scene/Look can declare whether it targets the whole
  stage or a specific fixture group, surfaced as a Scope combo in the Looks
  editor.
- Real **File → Import** for `.qxw` — merge fixtures, fixture groups, and
  functions (with their full dependency closure) from another workspace
  file into the one currently open, with automatic ID-collision remapping.
- Rebrand throughout: application, launcher, and fixture-editor binaries,
  macOS app bundle, and About box now identify as qlcconsole, with clear
  in-app attribution to QLC+ and its original authors.

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

[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
