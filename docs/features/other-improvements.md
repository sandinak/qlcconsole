# Other improvements

Smaller fork-specific features that don't need a full guide of their own.

## Connections &amp; patch

![The Connections tab: protocols, interfaces, and the universes they carry](../images/connections-devices-tab.png)

The Connections tab reworks stock QLC+'s I/O patching into one place that
answers "how is this console actually wired," following the signal outward:
**protocol → interface → devices heard on it → the universes they carry.**
Three sub-tabs — **Devices** (shown above), **Overview**, and **Detailed** —
give the same information at different levels of depth.

Because Devices follows the wiring, a patched universe appears only under
the output it goes to, and only unpatched ones gather in their own
**Unpatched** folder. Tick **By universe** for the other way round: every
universe in number order, each with where it goes ("Art-Net → CS1
(172.18.2.211) port 1", "WLED → 172.18.2.55", or "not patched") and what it
carries — same right-click actions, renamed inline the same way. The choice
is remembered. By universe, only the universes **in use** are listed — a
fixture patched into it, an output or an input — with a line at the foot
saying how many are hidden; tick **Show unused universes** to see them all.
(They're only hidden: universes are numbered by position and every patch
refers to them, so one from the middle can't be removed without renumbering
the rest.) Only the **Device** column is edited in place; Detail and
Carries describe the row and aren't text to type over.

![Connections, by universe, with Universe 1's output showing](../images/connections-by-universe.png)

1. **By universe** — every universe in use, in number order.
2. **The universes**, each with where it goes and what it carries.
3. **What the selected universe is sending**, channel by channel.

Select a universe and the panel on the right shows what it is **sending**,
live: all 512 channels, each with its value and a bar as high, the channels
of each patched fixture shaded together and named where they start. The
values are what goes out, after the grand master; hover a channel for whose
it is ("Front Par, channel 1 — Dimmer").

Right-click an interface to patch a universe to it or configure the
protocol; right-click a universe to rename it, set its feedback line or
input profile, unpatch it, or delete it. (The Devices tab keeps all of this
to one row over the list — **By universe**, **Show unused…**, a one-line
"Right-click a row for what it can do" whose tooltip has the whole
explanation, the search status, **Undo Patch Change** and **Rescan** — so
the list and the output view get the window's height.) A full patch-change **undo**
(Ctrl+Z, from any of the three sub-tabs) snapshots and restores the whole
patch state, not just the last single edit.

Compared to stock QLC+, this fork also fixed a number of real patching
gaps: device discovery correctly attributing ArtNet nodes instead of
defaulting unplaceable ones to loopback, universe renumbering under live
controllers, rescan working on every plugin type, hardware lines no longer
silently filtered from the "heard on" liveness view, and manual
targets/ports/aliases that persist with the workspace.

### WLED output

A new output plugin for Wi-Fi LED controllers running WLED (lanterns,
strings, small pixel props). Each WLED device gets **one universe of its
own, aimed at its IP address** — add the fixture with the connection
"Network (WLED, JSON over Wi-Fi)" and patching does the rest, using the
WLED output rather than borrowing Art-Net.

- It sends WLED's own JSON state, not a DMX stream, and only **when the look
  actually changes** — at most about ten times a second per device, with a
  matching WLED transition so the steps blend. A held look is sent once; a
  long fade goes as a handful of states ending on the last one. This keeps
  Wi-Fi traffic small.
- Every few seconds it checks in with each device. Any that **don't answer,
  or answer on weak Wi-Fi** (weaker than -75 dBm), are listed by fixture name
  in the [readiness footer](#readiness-footer).
- While the Lighting Studio or the Rig Overview is showing a WLED fixture, it asks that device
  what its LEDs are actually showing and draws that — a **live view** of the
  real pixels. It stops asking when the view is closed.

**Is each WLED set up the way the show drives it?** Every 30 s (and a
moment after the show loads or a fixture changes) each WLED fixture's own
settings are read from the device (`/json/cfg`) and compared:

- Driven through **WLED's API**: it should not take realtime streams — an
  E1.31/Art-Net stream on its universe would take it over from the show.
- Driven by **Art-Net or sACN**: it has to listen for that protocol, on the
  universe its output sends, at its patched address, in the DMX mode its
  definition's mode needs (Effect (15ch) → WLED's *Effect*, Single DRGB →
  *Single DRGB*, Single RGB → *Single RGB*).

What does not match is listed in the footer ("N WLEDs not set up as this
show drives them") and on the fixture itself (a **WLED** row in the studio
inspector and Fixture Properties). **Fix on Device…**, in both places, shows
exactly what it would change on the device and writes it there only when you
agree, then reads it back.

Effects that the WLED device runs itself are picked in the look editor —
see [WLED effects](programming-tab.md#wled-effects).

So far the WLED output has been checked against a stand-in that answers
like a WLED device, not yet against real WLED hardware on a show network —
try it on your own devices before relying on it for a show.

## Window menu

Every view is in **Window**, with its shortcut (⇧⌘1 Connections, ⇧⌘2
Fixtures, ⇧⌘3 Lighting Studio, ⇧⌘4 Functions, ⇧⌘5 Programming, ⇧⌘6 Shows,
⇧⌘7 Virtual Console, ⇧⌘8 Simple Desk): choosing one brings it forward — its
tab, or its own window when it has been broken out (right-click a tab →
**Open in Its Own Window**, Shift-click it, or **Move … to Its Own
Window**),
which the menu marks "in its own window". **Put Back in Main Window**
returns one, **Put All Back in Main Window** all of them; **Minimize** (⌘M)
as usual.

**Functions has no tab.** It sat beside Programming and was easy to hit by
mistake, and Programming now covers building scenes and looks. The
classic **Function Manager** — chasers, sequences, EFX, collections, RGB
matrices, scripts, audio and video — is still there: **Window → Functions**
(⇧⌘4) opens it in a window of its own, and closing that window puts it away
again (it does not come back as a tab).

## Readiness footer

The status chip at the bottom of the window reads **Ready** when nothing
needs attention, and **Not ready (N)** in red when something does. Hover it
for a one-line reason per problem; click it for the full list, where each
problem can be fixed on the spot. It checks for:

- fixtures **not patched** (they appear in the Lighting Studio but send no DMX);
- fixture types that **declare no light output** (no lumens or watts, so the
  Lighting Studio and the Rig Overview have to guess how bright they are);
- **address clashes** — a show file with two fixtures on overlapping
  channels now loads the second one unpatched and flagged, instead of
  silently dropping it;
- **duplicate fixture names**;
- **definitions that can't be read**, grouped under *Local* (your own) and
  *Included* (shipped with the app), each with the reason and, where another
  copy of the same fixture loaded instead, which one is being used;
- **duplicate definitions** — two of your own files for the same fixture,
  where only the newest is used;
- **universes with fixtures but no output**, so what they send goes
  nowhere;
- **WLED devices not answering**, or on weak Wi-Fi;
- **WLEDs not set up as the show drives them** (see WLED output), with
  **Fix on Device…**;
- **channels the Simple Desk is holding** — a channel touched on the desk
  overrides every look and scene until released, so a look that "doesn't
  change the fixture" is often this. Listed by universe and channel, with
  **Release**;
- **files in cloud storage that aren't on this computer** (see
  [Cloud storage](#cloud-storage));
- **fixtures with no definition here** — standing in as plain dimmers until
  their definition is back.

Select the rows you want (shift-click for a range; click a heading to take
everything under it) and use the fix offered for that problem:
**Patch…**, **Re-patch…** (clashes), **Edit Definition…**, **Repair in
Fixture Editor…**, **Move to Trash…**, **Number Them** (gives duplicates
"#2", "#3", keeping the name on the patched one), **Open Connections**,
**Download Now**, or **Fetch Definitions**. The list updates as each fix
lands.

"Repair in Fixture Editor…" opens the broken file in the Fixture Editor's
repair view, which shows where the file breaks and why, offers repairs it
has already tried, and closes again when you're done. Once it's saved, the
definition loads without restarting.

## Cloud storage

Show files, fixture definitions and scripts often live in a synced folder
(Synology Drive, iCloud Drive, OneDrive, Dropbox). Those services can leave
a file **online-only** — listed in the folder, but not actually on this
computer — and reading one means waiting on the network. With the network
out of reach, that wait used to hang the whole startup.

- **Startup never waits on the cloud.** Online-only files are skipped
  instead of fetched, and the footer says how many, in which folders, and
  which service holds them. **Download Now** fetches them in the
  background; fixtures and effects then load straight away, anything else
  on the next start. Better still, set the folder to stay on this computer.
- **The show cache.** Every definition a show uses that doesn't ship with
  the app is kept in a cache on this computer, refreshed whenever the show
  loads or saves — so a production run never needs the network.
- **Before a show loads**, each definition it names comes from the fixture
  library, else the show cache, else is fetched from the cloud under a
  "Waiting for the cloud…" window with a **Skip** button — and then cached.
- **Nothing lost on save.** A fixture whose definition isn't here still
  stands in as a generic dimmer, but it now **keeps its real manufacturer,
  model and mode** and saves them. (Stock QLC+ saved it as "Generic", and the
  real fixture was gone for good.) Saving with such fixtures warns first,
  and **Fetch Definitions** in the footer restores each one once its
  definition is back.

## Layers panel

A docked, tree-based organizer for everything on the 2D map — fixtures,
trusses, platforms, targets, power sources, images — each layer with its own
visibility, lock, and stacking order, plus a separate nested **group** tree
for spatial select-together groupings underneath it. New items land on
whichever layer is currently active.

Multi-select a set of rows for bulk actions: move to layer, or "New
Folder/Layer from selection…" (creates and populates it in one step). Rows
are labeled with what kind of grouping they actually are — *on truss*, *on
riser*, *on pipe*, *frame*, *fixture group* — so double-clicking one
reliably opens the right editor instead of guessing between four
visually-similar folder kinds.

- **One lock: the layer's.** Locking anything — from its right-click menu
  ("Lock Layer “Rig”"), the Layers panel, or a power source's or network
  node's own toggle — locks its whole layer, and everything locked is
  outlined in red. (Older shows' per-object locks move onto a locked layer
  called "Locked" when opened, so nothing that was held comes loose.) A
  folder's own "Lock Folder Position" still holds a folder still while
  leaving it selectable.
- **Rename any row** — select it and press Enter (or F2), fixture groups
  included. The editor starts from the bare name, without the "· on truss"
  label, and a rename shows everywhere the fixture appears.
- **Fixture groups are listed under the structures** their fixtures are on
  — under every group a fixture belongs to — or under their layer, or under
  "Fixture Groups" when they span layers. Right-click a group's row to move
  its fixtures to another layer, lay it out again from the stage, or remove
  it from a truss/tower.
- **Select All Similar** on a fixture group's row selects the other groups
  of the same fixtures laid out the same way (the four step fronts find each
  other); on a fixture, every fixture of the same model and mode. Selecting
  plot items also selects their rows here.

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

### A fixture's maximum draw

Some fixtures are power hogs — a wall-washer bar pulling 6 A at full — and
a breaker doesn't care that the show only ran them full for one cue. Each
fixture can be held to a **maximum draw**, in amps (Fixture Properties →
**Max draw**, set on several at once with a multi-selection).

That needs its draw **measured**, because LED fixtures are far from linear:
the same bar drew 1.7 A at half, not 3, and 2 A with only red at full. The
readings are the whole fixture's current off a clamp meter, standby included:
each colour alone at full, everything at half, everything off, and
(optionally) everything at full.

- **Measure…** lights the fixture a state at a time — over anything else, and
  past its cap — and asks for each reading, then gives it back. Its other
  channels are left alone, so stop what else is running on it first. The
  readings can be used for all the fixtures selected (each keeps its own
  maximum).
- **Profile…** types in readings already taken. A colour left unmeasured
  shares what is left of the everything-at-full reading. With several
  selected, it opens on the readings of one that has been measured.
- **Copy to Same Fixtures…** gives a measured fixture's readings to every
  other fixture of the same make, model and mode in the show — measure one
  bar, copy, done. Each keeps its own maximum. With several selected, the
  measured one among them is the source, wherever it is in the selection.

From those it works out what the fixture draws at whatever it's sent: each
colour's share, split among its pixels, bent by the curve the half reading
gives. Whenever that would go over the maximum, its colours are scaled down
**together** — the colour holds, only the brightness drops — on what its
universe sends. Red alone at 2 A under a 3 A maximum is left as it is; full
white at 6 A is sent at about 69%. The universe panel in Connections, the
Lighting Studio, the Rig Overview and the power panel all show the held-back output, and the power
panel's footer names the fixtures being held back. A fixture with a maximum
but no readings isn't limited, and says so. The live and full-load
estimates use the readings (and the cap) wherever a fixture has them.

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
