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
- Rebrand throughout: application, launcher, and fixture-editor binaries,
  macOS app bundle, and About box now identify as qlcconsole, with clear
  in-app attribution to QLC+ and its original authors.

### Changed
- Versioning scheme: the in-app build string now tracks the QLC+ base version
  this fork tracks, plus a git-derived build count
  (`4.14.2+qlcconsole.<N>`); public releases additionally get a SemVer tag
  (see [RELEASE.md](RELEASE.md)).

[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
