# qlcconsole documentation

Everything written about qlcconsole, in one place. Start with the
[project README](../README.md) for what qlcconsole is and its major
features.

## User guide

[**docs/features/**](features/README.md) — how to use the app, feature by
feature, with pictures taken from the app. The same pages are the in-app
help: **F1** opens the one for whatever you're looking at, and **Help**
lists them all.

| Page | What it covers |
|---|---|
| [Getting started](features/getting-started.md) | Starting the app, show files, the tabs and the three views, importing, the ways a show comes together |
| [What's new](features/whats-new.md) | The latest changes, release by release |
| [Show Tree](features/show-tree.md) | Cue lists, cues and layers, tracking, hits, productions, the Run tab and the Cue Runner |
| [Build](features/build.md) | Palettes, effects, scenes and collections in the Workbench |
| [Lighting Studio](features/lighting-studio.md) | The plot, structures, mounting, plot layers and locks, stage marks and the apron, Quick focus, the Rig Overview |
| [Venue focus](features/focus-calls.md) | Focus calls, venues and their corrections, the focus report, scramble practice |
| [House rig and tour package](features/venues.md) | The venue chip, owners, universes per venue, the clash check |
| [Focus this cue](features/refine.md) | Changing a cue or a Build scene from the stage |
| [Fixture Manager](features/fixture-manager.md) | Groups, the head-layout grid, the inventory, the Patch dialog, the patch list and labels |
| [Stage targets](features/stage-targets.md) | Places on stage that moving heads aim at |
| [Other improvements](features/other-improvements.md) | Connections and patch, WLED, the readiness footer, cloud storage, and more |
| [Shows](features/shows.md) · [Show control](features/show-control.md) · [Effects](features/effects.md) | The timeline, QLab over OSC and MIDI Show Control, every built-in effect |
| [Definition updates](features/definition-updates.md) · [Software updates](features/updates.md) | Keeping fixture definitions and the app up to date |
| [Keyboard and mouse](features/shortcuts.md) · [Troubleshooting](features/troubleshooting.md) | Every shortcut; what to check when something isn't working |
| [Generator API](../EFFECTSCRIPT_API.md) | Writing your own effects |

## Performance

[**Performance**](performance.md) — how much one computer can drive: the
safe operating envelope on a Raspberry Pi 5, a Linux i7 and an Apple M4,
Art-Net output rates, long runs and memory, with charts, the method and
the data ([`perf-data/`](perf-data/)) to reproduce them.

## The course

[**docs/course/**](course/README.md) — *Lighting a show choir with the
Show Tree*: a course for high-school show choir tech crews, from "what
does lighting do?" to running a real show, on a virtual stage.

- [Course plan](course/COURSE_PLAN.md): audience, levels, lessons,
  assessment.
- [Level 0](course/level-0/index.md) · [Level 1](course/level-1/index.md)
  · [Level 2](course/level-2/index.md) · [Level 3](course/level-3/index.md):
  each lesson's teacher guide, handout, slides, exit ticket and at-home
  work.
- [Install](course/install.md) · [Glossary](course/GLOSSARY.md) ·
  [The teaching show](course/rig.md) · [The show files](course/shows/README.md)
- [For volunteers](course/VOLUNTEER.md): helping write the course.

The lessons point into the user guide for the full story of each tool,
and the user guide's index points back here.

## Design notes

How and why features work the way they do: the decisions, written before
or while each was built. They are working documents; where a design note
and the user guide disagree, the user guide describes the app as it is.

| Note | About |
|---|---|
| [Workflow design](../WORKFLOW_DESIGN.md) | Build, then schedule: one workflow from an old QLC+ show to the Show Tree |
| [Show Tree design](../SHOW_TREE_DESIGN.md) · [Sub-cues](../SUBCUES_DESIGN.md) · [Move in black](../MOVEINBLACK_DESIGN.md) | The cue model, sub-cues, move in black |
| [Programming redesign](../PROGRAMMING_REDESIGN.md) · [Programming role](../PROGRAMMING_ROLE_DESIGN.md) | The Build tab next to the Show Tree |
| [Venue focus and Focus this cue](../FOCUS_REFINE_DESIGN.md) · [Quick focus](../FOCUS_MODE_DESIGN.md) | The focus tools, the controller and the voice |
| [Venues](../VENUES.md) | House rig, tour package and universes per venue |
| [Connections cleanup](CONNECTIONS_CLEANUP.md) | Folding the Overview into Devices; one connection window |
| [Show review](SHOW_REVIEW.md) | A post-0.3 design, not built yet |
| [Show control](../SHOW_CONTROL_DESIGN.md) · [Playback timing](../PLAYBACK_TIMING_DESIGN.md) | QLab and timecode; fades, holds and follows |
| [Line targets](../LINE_TARGET_DESIGN.md) · [Spread and speed](../SPEED_SPREAD_DESIGN.md) · [Positions](../POSITION_ARCH.md) | Aiming and effect spread |
| [Effects](../EFFECTPRESET_DESIGN.md) · [Effect lifecycle](../EFFECT_LIFECYCLE_DESIGN.md) | Generators, bundles and looks |
| [Eos import](../EOS_IMPORT_DESIGN.md) · [Shows that carry their definitions](../SHOW_EMBEDDING_DESIGN.md) · [Show lifecycle](../SHOW_LIFECYCLE_DESIGN.md) | Bringing shows in, and keeping them whole |
| [Network and cabling](../NETWORK_CABLING_DESIGN.md) · [Fixture Manager](../FIXTUREMANAGER_MODERNIZE_DESIGN.md) · [Fixture Studio](../FIXTURESTUDIO_DESIGN.md) | The rig network, patching and groups |
| [Control surfaces](../CONTROL_SURFACE_DESIGN.md) · [Control Map](../CONTROLMAP.md) | PMJ, Xbox and MIDI mapping |
| [Proposals](proposals/inventory-first-workflow.md) | [Inventory first](proposals/inventory-first-workflow.md), [programmer mode](proposals/programmer-mode.md), [UI levels](proposals/ui-levels.md) |
| Reviews | [Consolidation](../CONSOLIDATION_REVIEW.md), [workflow UX](../WORKFLOW_UX_REVIEW.md), [workflow subtleties](../WORKFLOW-SUBTLETIES.md), [visual consistency](../VISUAL_CONSISTENCY_REVIEW.md), [rig 3D strategy](../RIG3D_STRATEGY_REVIEW.md) |

## For developers

| Document | What |
|---|---|
| [CLAUDE.md](../CLAUDE.md) | Building, running, the test gate, where things live in the code, conventions |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Contributing, coding style, pictures without colour profiles |
| [Test map](TEST_MAP.md) · [Test matrix](TEST_MATRIX.md) | Which test covers what in this release; the extended test matrix |
| [RELEASE.md](../RELEASE.md) | Version numbers, signed macOS releases, Windows and Linux builds |
| [CHANGELOG.md](../CHANGELOG.md) · [TODO.md](../TODO.md) · [DONE.md](../DONE.md) | What changed; open work; the detailed development log |
| [Live-rig test plan](../RIG_TEST_PLAN.md) · [Stress test report](../STRESS_TEST_REPORT.md) | Testing on real hardware and under load |

Keeping the docs honest:

- `helpbrowser_test` fails on a user-guide link to a missing page,
  heading or picture, and on a page that stops rendering part way. A new
  page goes in `ui/src/help.qrc` and the help browser's topic list.
- `./help-shots.sh` retakes the user guide's pictures from the app,
  offscreen; `courseshow_test` does the course's lessons and takes their
  pictures.
