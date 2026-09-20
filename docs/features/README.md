# qlcconsole feature guide

qlcconsole is a fork of [QLC+](https://www.qlcplus.org/) 4.14.2 (Q Light
Controller Plus). Stock QLC+ programs a show as a flat list of fixtures, each
edited with raw per-channel DMX sliders. This fork adds a layer on top of
that aimed at designing and running a **physical rig**: build the truss/riser
structure the fixtures actually hang from, place fixtures on it, aim them at
places in the room, and build looks out of reusable presets instead of
per-fixture sliders.

Everything in this guide is fork-specific — none of it exists in upstream
QLC+. The stock scene editor, Virtual Console, Simple Desk, Show Manager
timeline, and I/O plugin system all still work the way they always have.

## Start here

| Guide | What it covers |
|---|---|
| [Lighting Studio](lighting-studio.md) | The 3D-ish rig view: trusses, towers, platforms, pipes, stands, fixture mounting, locks, beam/coverage visualization |
| [Programming Tab](programming-tab.md) | Building "looks" out of reusable palettes instead of raw DMX, drag-and-drop targets, the inline look editor |
| [Fixture Manager](fixture-manager.md) | Fixture-group folders and the head-layout grid editor |
| [Stage Targets &amp; Aim Solving](stage-targets.md) | Marking points/areas/people in the room and having fixtures aim there automatically |
| [Other improvements](other-improvements.md) | Connections/patch overhaul, Layers panel, Studio groups, count badges, Blind/Park/Highlight, and more |

## The rough shape of a show, end to end

1. **Patch and wire it up** in **Connections** — discover interfaces, patch
   universes, see what's actually talking (see
   [Other improvements](other-improvements.md#connections--patch)).
2. **Organize fixtures** in **Fixture Manager** — group them, lay them out in
   the head-layout grid for XY/EFX effects and Programming-tab drag targets
   (see [Fixture Manager](fixture-manager.md)).
3. **Build the physical rig** in **Lighting Studio** — trusses, towers,
   risers, platforms — and mount fixtures onto it so the app knows where
   everything actually is in 3D space (see
   [Lighting Studio](lighting-studio.md)).
4. **Mark places that matter** — a mark on the deck, a drum kit, a person —
   as **Stage Targets**, so moving heads can aim at them instead of you
   dialing in pan/tilt by hand (see [Stage Targets](stage-targets.md)).
5. **Build looks** in the **Programming Tab** by dragging color/dimmer/aim
   palettes onto scenes, instead of setting raw channel values (see
   [Programming Tab](programming-tab.md)).
6. Run the show from Virtual Console / Simple Desk / Show Manager, same as
   stock QLC+.

## A note on the screenshots

Every screenshot in this guide is taken from the app itself, running a real
(if synthetic) test rig — not mockups. Menu wording and layout will drift
slightly release to release; the workflow and concepts won't.
