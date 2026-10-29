# qlcconsole documentation

Everything written about qlcconsole, in one place. Start with the
[project README](../README.md) for what qlcconsole is and its major
features.

## Where things are

| Folder | What's in it |
|---|---|
| [features/](features/README.md) | The user guide, which is also the in-app help |
| [course/](course/README.md) | The show choir course and its show files |
| [design/](design/) | Design notes and reviews: why features work the way they do |
| [dev/](dev/) | For developers: releases, tests, upstream intake, the generator API |
| [project/](project/) | The strategy, open work and the development log |
| [proposals/](proposals/) | Ideas written up but not scheduled |
| [performance.md](performance.md), [perf-data/](perf-data/) | How much one computer can drive |

The repository root keeps only the README, CHANGELOG, CONTRIBUTING, SUPPORT,
CLAUDE.md and the licence, next to the build and packaging scripts.
Fork-only developer scripts (Art-Net probes, the RDM simulator, soak and
drift tests, help pictures) are in [`tools/`](../tools/).

## Direction

[**Strategy**](project/STRATEGY.md) (adopted 2026-10-28) — what qlcconsole
is and isn't, a home for QLC+ 4 users, the ROI of every candidate, the
roadmap to 0.5, and the guardrails each change follows.

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
| [Generator API](dev/EFFECTSCRIPT_API.md) | Writing your own effects |

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
| [Workflow design](design/WORKFLOW_DESIGN.md) | Build, then schedule: one workflow from an old QLC+ show to the Show Tree |
| [Show Tree design](design/SHOW_TREE_DESIGN.md) · [Sub-cues](design/SUBCUES_DESIGN.md) · [Move in black](design/MOVEINBLACK_DESIGN.md) | The cue model, sub-cues, move in black |
| [Programming redesign](design/PROGRAMMING_REDESIGN.md) · [Programming role](design/PROGRAMMING_ROLE_DESIGN.md) | The Build tab next to the Show Tree |
| [Venue focus and Focus this cue](design/FOCUS_REFINE_DESIGN.md) · [Quick focus](design/FOCUS_MODE_DESIGN.md) | The focus tools, the controller and the voice |
| [Venues](design/VENUES.md) | House rig, tour package and universes per venue |
| [Connections cleanup](design/CONNECTIONS_CLEANUP.md) | Folding the Overview into Devices; one connection window |
| [Show review](design/SHOW_REVIEW.md) | A post-0.3 design, not built yet |
| [Show control](design/SHOW_CONTROL_DESIGN.md) · [Playback timing](design/PLAYBACK_TIMING_DESIGN.md) | QLab and timecode; fades, holds and follows |
| [Line targets](design/LINE_TARGET_DESIGN.md) · [Spread and speed](design/SPEED_SPREAD_DESIGN.md) · [Positions](design/POSITION_ARCH.md) | Aiming and effect spread |
| [Effects](design/EFFECTPRESET_DESIGN.md) · [Effect lifecycle](design/EFFECT_LIFECYCLE_DESIGN.md) | Generators, bundles and looks |
| [Eos import](design/EOS_IMPORT_DESIGN.md) · [Shows that carry their definitions](design/SHOW_EMBEDDING_DESIGN.md) · [Show lifecycle](design/SHOW_LIFECYCLE_DESIGN.md) | Bringing shows in, and keeping them whole |
| [Network and cabling](design/NETWORK_CABLING_DESIGN.md) · [Fixture Manager](design/FIXTUREMANAGER_MODERNIZE_DESIGN.md) · [Fixture Studio](design/FIXTURESTUDIO_DESIGN.md) | The rig network, patching and groups |
| [Control surfaces](design/CONTROL_SURFACE_DESIGN.md) · [Control Map](dev/CONTROLMAP.md) | PMJ, Xbox and MIDI mapping |
| [Show Tree kickoff](design/SHOW_TREE_KICKOFF.md) | The Show Tree's planning start |
| [Proposals](proposals/inventory-first-workflow.md) | [Inventory first](proposals/inventory-first-workflow.md), [programmer mode](proposals/programmer-mode.md), [UI levels](proposals/ui-levels.md) |
| Reviews | [Consolidation](design/CONSOLIDATION_REVIEW.md), [workflow UX](design/WORKFLOW_UX_REVIEW.md), [workflow subtleties](dev/WORKFLOW-SUBTLETIES.md), [visual consistency](design/VISUAL_CONSISTENCY_REVIEW.md), [rig 3D strategy](design/RIG3D_STRATEGY_REVIEW.md) |

## For developers

| Document | What |
|---|---|
| [CLAUDE.md](../CLAUDE.md) | Building, running, the test gate, where things live in the code, conventions |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Contributing, coding style, pictures without colour profiles |
| [Test map](dev/TEST_MAP.md) · [Test matrix](dev/TEST_MATRIX.md) | Which test covers what in this release; the extended test matrix |
| [RELEASE.md](dev/RELEASE.md) | Version numbers, signed macOS releases, Windows and Linux builds |
| [Upstream intake](dev/UPSTREAM.md) | The QLC+ base (4.14.4), what each intake of upstream fixes took, adapted and skipped, and how to do the next one |
| [CHANGELOG.md](../CHANGELOG.md) · [TODO.md](project/TODO.md) · [DONE.md](project/DONE.md) | What changed; open work; the detailed development log |
| [Strategy](project/STRATEGY.md) · [Show Tree overnight log](project/SHOW_TREE_OVERNIGHT.md) · [Notes](project/NOTES.md) | Direction and guardrails; a build log; loose ideas |
| [Manual GUI tests](dev/testing_st.md) | On-site checklist from the programmer-mode work |
| [Live-rig test plan](dev/RIG_TEST_PLAN.md) · [Stress test report](dev/STRESS_TEST_REPORT.md) | Testing on real hardware and under load |

Keeping the docs honest:

- `helpbrowser_test` fails on a user-guide link to a missing page,
  heading or picture, and on a page that stops rendering part way. A new
  page goes in `ui/src/help.qrc` and the help browser's topic list.
- `tools/check-doc-links.py` fails on a relative link in any tracked
  Markdown file (docs, README, the help, the course) that points at a
  missing file. The gate and CI run it.
- `tools/help-shots.sh` retakes the user guide's pictures from the app,
  offscreen; `courseshow_test` does the course's lessons and takes their
  pictures.
