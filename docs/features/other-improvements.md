# Other improvements

Smaller fork-specific features that don't need a full guide of their own.

## Connections &amp; patch

![The Connections tab: protocols, interfaces, and the universes they carry](../images/connections-devices-tab.png)

The Connections tab reworks stock QLC+'s I/O patching into one place that
answers "how is this console actually wired," following the signal outward:
**protocol → interface → devices heard on it → the universes they carry.**
Three sub-tabs — **Devices** (shown above), **Overview**, and **Detailed** —
give the same information at different levels of depth.

Right-click an interface to patch a universe to it or configure the
protocol; right-click a universe to rename it, set its feedback line or
input profile, unpatch it, or delete it. A full patch-change **undo**
(Ctrl+Z, from any of the three sub-tabs) snapshots and restores the whole
patch state, not just the last single edit.

Compared to stock QLC+, this fork also fixed a number of real patching
gaps: device discovery correctly attributing ArtNet nodes instead of
defaulting unplaceable ones to loopback, universe renumbering under live
controllers, rescan working on every plugin type, hardware lines no longer
silently filtered from the "heard on" liveness view, and manual
targets/ports/aliases that persist with the workspace.

## Layers panel

A docked, tree-based organizer for everything on the 2D map — fixtures,
trusses, platforms, targets, power sources, images — each layer with its own
visibility, lock, and stacking order, plus a separate nested **group** tree
for spatial select-together groupings underneath it. New items land on
whichever layer is currently active.

Multi-select a set of rows for bulk actions: move to layer, lock/unlock
position, or "New Folder/Layer from selection…" (creates and populates it in
one step). Rows are labeled with what kind of grouping they actually are —
*on truss*, *on riser*, *frame*, *fixture group* — so double-clicking one
reliably opens the right editor instead of guessing between four
visually-similar folder kinds.

## Studio groups (Fixture Studio)

A richer relative of the plain spatial group described in
[Lighting Studio](lighting-studio.md): a group can additionally carry an
origin and rotation, promoting it to an editable rigid body whose members
are positioned in **group-local** coordinates rather than raw world
coordinates. Built for cases like a run of steps, each with a front face and
a top, composed into "all step fronts" / "all step tops" sub-groupings.

- A dedicated **3-plane editor** (Top/Front/Side) draws a frame's members as
  draggable bars, with per-face distribute/put-on-face actions and reference
  geometry (a platform's footprint, or a truss line) drawn in.
- A group can bind to a plain [Fixture Manager](fixture-manager.md) group
  two ways: **Seed** (lay members out from the group's cell layout) or
  **Adopt** (read their current studio position back into cells).
- **Save an arrangement as a template**, browsable in a component library,
  and "stamp" it onto a new selection elsewhere.

Reachable from the Layers tree (double-click a group node) or the plot
(right-click a set of fixtures → "Create Studio Group").

## DMX &amp; power overlay lenses

Two "More → Overlay" modes on the main 2D plot (distinct from the rig
view's beam/footprint overlays):

- **DMX lens** — colours every fixture by its DMX universe, so fixtures
  sharing a colour share a physical DMX run, and flags overlapping address
  ranges on the same universe in red with an "⚠ address clash" tooltip — a
  patch-planning aid.
- **Power lens** — colours fixtures by power circuit/source (part of the
  power-distribution model below).

## Blind / Park / Highlight / Flash

A classroom-safety and rigging-focus trio, available from the Programming
tab toolbar and Control menu:

- **Blind** mutes physical DMX output while the 2D preview keeps updating —
  build and check a look with the room dark. Design-mode only; force-cleared
  the moment you switch to Operate.
- **Park** holds selected fixtures' output frozen, out of cue control, and
  persists that with the workspace.
- **Highlight** / **Flash** briefly drive selected fixtures to full white,
  for identifying which physical fixture is which.

## Power distribution

A model of real electrical circuits sitting underneath the rig: a
**PowerSource** (voltage) feeds **PowerCircuits** (rated amps, derate %),
each carrying a set of fixtures. A live amperage/wattage estimate appears in
the footer in Design mode; a circuit-assignment dialog can auto-bin-pack
fixtures onto circuits, and UPS/battery entries get a runtime estimate.

## Show timeline &amp; MTC follow

The Show Manager tab and the Programming tab's embedded timeline editor
share: per-track intensity submasters, solo / solo-safe / hold-last /
priority flags, MIDI Timecode follow (auto-detect or a manual source
override) with freeze-on-stop and locate/seek, a draggable show-length end
handle, and footer sync-health/load indicators. Virtual Console widgets can
override the timeline per-channel while it's live.

## Control Map (MIDI learn)

A lightweight way to bind a MIDI trigger straight onto a function without
building a Virtual Console widget for it first: pick a function, hit
"learn," send the MIDI event you want to trigger it. (This is the first of
several planned phases — a dedicated grid dialog, fader/submaster targets,
and look-recall banking aren't built yet.)
