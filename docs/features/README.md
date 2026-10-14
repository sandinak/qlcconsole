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

In the app, **F1** opens the guide for whatever you're looking at, and
**Help** lists them all.

| Guide | What it covers |
|---|---|
| [Getting started](getting-started.md) | Starting the app, show files and autosave, finding your way around, and the ways a show comes together — designing first, patch first, at a venue, a row of like structures |
| [What's new](whats-new.md) | The latest changes |
| [Lighting Studio](lighting-studio.md) | Building the physical rig: trusses, towers, platforms, pipes, stands, hangers, fixture mounting, locks; the Angled 3D view and Rig Overview, lit by the show (beams, haze, gobos, lanterns) |
| [Programming Tab](programming-tab.md) | Building "looks" out of reusable palettes instead of raw DMX, drag-and-drop targets, the inline look editor and effect browser, scene transitions, WLED effects |
| [Fixture Manager](fixture-manager.md) | Fixture-group folders, the head-layout grid editor, the inventory of unpatched fixtures and the Patch dialog |
| [Stage Targets &amp; Aim Solving](stage-targets.md) | Marking points/areas/people in the room and having fixtures aim there automatically |
| [Other improvements](other-improvements.md) | Connections/patch overhaul, the WLED output, the readiness footer and its fixes, cloud-storage safety and the show cache, Layers panel, Studio groups, Blind/Park/Highlight, and more |
| [Shows (the timeline)](shows.md) | Tracks, items, sequences and cues, the show end, track flags, following timecode |
| [Show control (QLab)](show-control.md) | Cue numbers and cue lists; QLab (or anything else) firing cues over MIDI Show Control or OSC |
| [Effects](effects.md) | Every built-in effect, by what it does, and writing your own |
| [Keyboard and mouse](shortcuts.md) | Every shortcut, by view |
| [Troubleshooting](troubleshooting.md) | When lights, the Lighting Studio or Rig Overview, files or fixtures aren't doing what you expect |

## The rough shape of a show, end to end

You can work in either order. The fork is built so you can **design the rig
first and patch it last** — the way a show is usually planned, before
anyone knows which node a fixture will end up on:

1. **Stock the inventory** in **Fixture Manager** — add the fixtures you
   have (or plan to hire) without addresses. They're real fixtures you can
   place, group and program; they just send no DMX yet (see
   [Fixture Manager](fixture-manager.md)).
2. **Build the physical rig** in **Lighting Studio** — trusses, towers,
   risers, platforms — and drag fixtures from the Inventory panel onto it,
   so the app knows where everything actually is in 3D space (see
   [Lighting Studio](lighting-studio.md)).
3. **Design the network** — place the Art-Net/sACN nodes and wireless
   receivers where they'll be rigged; the Studio draws the cable runs and
   the Network Report lists ports, universes and run lengths.
4. **Patch** — the Patch dialog addresses a selection in one go, and can
   place each fixture on the nearest node with room. Then wire the
   universes up in **Connections** — discover interfaces, patch them out,
   see what's actually talking (see
   [Other improvements](other-improvements.md#connections--patch)). WLED
   devices get a universe each, by IP.
5. **Organize fixtures** into groups, laid out in the head-layout grid —
   or straight from where they stand on stage — for effects and
   Programming-tab drag targets (see [Fixture Manager](fixture-manager.md)).
6. **Mark places that matter** — a mark on the deck, a drum kit, a person —
   as **Stage Targets**, so moving heads can aim at them instead of you
   dialing in pan/tilt by hand (see [Stage Targets](stage-targets.md)).
7. **Build looks** in the **Programming Tab** by dragging color/dimmer/aim
   palettes onto scenes, instead of setting raw channel values, and give
   scenes transitions (see [Programming Tab](programming-tab.md)).
8. **Check the footer.** When it says **Ready**, nothing is unpatched,
   clashing, unrouted or missing; when it says **Not ready**, click it and
   fix what it lists (see
   [Other improvements](other-improvements.md#readiness-footer)).
9. Run the show from Virtual Console / Simple Desk / Show Manager, same as
   stock QLC+.

If you already know the patch, the stock order still works: patch in
Connections first, add fixtures at their addresses, then build the rig
around them.

## A note on the screenshots

Every screenshot in this guide is taken from the app itself, running a real
(if synthetic) test rig — not mockups. Menu wording and layout will drift
slightly release to release; the workflow and concepts won't.
