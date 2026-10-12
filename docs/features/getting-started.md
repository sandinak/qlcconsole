# Getting started

This guide walks through starting qlcconsole and the ways a show usually
comes together — from an empty workspace, from a patch you already have, and
from a show file you're bringing to a venue. Each step links to the guide
with the detail.

Press **F1** anywhere for the help on what you're looking at; **Help →
qlcconsole Help** opens this guide set.

## Starting the app

Open **qlcconsole** like any app. A small window shows what it's loading
(fixture definitions, plugins, the workspace), then the main window
opens.

From a terminal:

```
qlcconsole -o path/to/show.qlcc     # open a show
qlcconsole -o show.qlcc -p          # open it straight into Operate mode
qlcconsole -k -o show.qlcc          # kiosk: Virtual Console only, operate mode
qlcconsole -w                       # also serve the web interface (port 9999)
qlcconsole -h                       # every option
```

Always pass the show with **-o**. A bare file name on the command line is
not opened — and saving then can write an empty workspace over it.

### Show files

- **.qlcc** is qlcconsole's own format, and what **Save** writes.
- **.qxw** (stock QLC+) opens fine; save it as .qlcc to keep everything this
  fork adds (the rig, palettes, looks, power profiles, layers…).
- **Autosave** writes a recovery copy every 5 minutes while there are
  unsaved changes (**Settings → Autosave** sets it on or off and how
  often). If the app stops without saving,
  the next launch offers to **recover the unsaved work**. A clean quit
  removes the recovery copy.
- A show keeps a copy of every **fixture definition** it uses that isn't
  shipped with the app — the **show cache** — so it opens the same on
  another computer. See
  [Missing definitions and the show cache](fixture-manager.md#missing-definitions-and-the-show-cache).
- A show kept in a **cloud folder** (iCloud, Dropbox, Synology…) is saved
  safely; see [Cloud storage](other-improvements.md#cloud-storage).

## Finding your way around

The tabs follow the order a show is built in:

| Tab | What it's for | Guide |
|---|---|---|
| **Connections** | How the console is wired: protocols, interfaces, the universes they carry, what each universe is sending | [Connections & patch](other-improvements.md#connections--patch) |
| **Fixtures** | The fixtures in the show, the inventory, groups and their head layouts, patching | [Fixture Manager](fixture-manager.md) |
| **Lighting Studio** | The physical rig: structures, where every fixture hangs, the network, the 3D Rig Overview | [Lighting Studio](lighting-studio.md) |
| **Programming** | Scenes built from looks, looks built from palettes and effects | [Programming Tab](programming-tab.md) |
| **Shows** | The timeline: tracks, sequences, cues, timecode | [Shows](shows.md) |
| **Virtual Console** | The operator's surface | stock QLC+ |
| **Simple Desk** | Raw channel faders | stock QLC+ |

**Window** lists every view with its shortcut (⇧⌘1 … ⇧⌘8). Double-click
a tab to break it out into its own window; Window brings it back.
**Functions** (the classic Function Manager — chasers, EFX, RGB matrices,
scripts, audio, video) has no tab: **Window → Functions** (⇧⌘4) opens it
in a window of its own. **View → Rig Overview** (⇧⌘R) opens the whole rig
in 3D.

**Design and Operate.** In **Design** mode (the default) you build: the
open scene plays live so you can see it, and nothing runs that you didn't
start. **Operate** (⌘F12) runs the show for real — and locks the editing
that would disturb it (moving fixtures, re-patching). The mode shows in
the footer.

**The footer** says whether the show is ready to run: **Ready**, or **Not
ready (N)** in red — click it for the list (unpatched or clashing
fixtures, universes going nowhere, missing definitions, WLED devices that
don't answer or aren't set up for the show…), each with its fix. Beside it,
**Load** is how much of each 20 ms DMX tick the show is using; **MTC** is
the timecode source. See
[Readiness footer](other-improvements.md#readiness-footer).

## Workflow 1 — a new show, designing first

The way a show is usually planned: decide the rig, then patch it. Nothing
needs an address until the end.

1. **Stock the inventory.** Fixtures tab → **Add to inventory**: the
   fixtures you have or will hire, with no address yet. They're real
   fixtures — you can place, group and program them; they just don't send
   DMX. ([Inventory](fixture-manager.md#inventory-design-the-rig-before-patching))
2. **Build the rig.** Lighting Studio → **Add**: trusses, towers,
   platforms and steps, pipes and stands, hangers, scenery (cyc, drapes,
   walls), fountains. Double-click any of them for its editor.
   ([Stage structures](lighting-studio.md#stage-structures))
3. **Put the fixtures on it.** Drag them from the **Inventory** panel onto
   a truss, tower or deck — they mount where you drop them. To move a
   fixture already on the plot onto a structure, **hold Shift as you drop**
   (a plain drag only moves it); a row of fixtures on a set of steps goes
   on with **Mount on → Deck → The deck under each**. Set **Orientation**
   (upright / hung) for anything that pans or tilts.
   ([Mounting a fixture](lighting-studio.md#mounting-a-fixture))
4. **Check it in 3D.** **⇧⌘R** — the Rig Overview. Hide layers in the
   Layers panel to see past things; the overview follows.
5. **Draw the network** (optional). **Add → Add Network Node** for each
   Art-Net/sACN node or wireless transmitter, where it'll really be rigged.
   ([Network nodes](lighting-studio.md#network-nodes-and-cable-runs))
6. **Patch.** Select the fixtures → **Patch…**: review the proposal (nearest
   node first, identical fixtures on contiguous addresses, one universe per
   network IP) and **Apply**. For a row of like structures — four towers,
   four step units — right-click one → **Patch Others Like This** so they're
   all addressed the same in their own universes.
   ([Patching](fixture-manager.md#patching-the-patch-dialog))
7. **Wire the universes.** Connections → right-click an interface → patch
   a universe to it. Select a universe to see what it's sending.
8. **Group the fixtures** for effects and looks: select on the plot →
   **Create Fixture Group from Selection**, or lay a group out from the
   stage. ([Groups](fixture-manager.md#creating-a-group))
9. **Mark the places that matter** — marks, a drum kit, a performer — as
   **Stage Targets**, so moving heads aim at them.
   ([Stage Targets](stage-targets.md))
10. **Build looks.** Programming tab: make palettes (colour, dimmer, aim,
    effects…), drag groups onto a scene's Targets and palettes onto its
    Looks. ([Programming Tab](programming-tab.md))
11. **Check the footer** says **Ready**, then run it from Virtual Console,
    Simple Desk or Shows.

## Workflow 2 — the patch comes first

When the rig is already hung and the addresses are fixed (a house rig, a
hired rig with a patch sheet):

1. **Connections**: discover the interfaces and nodes (**Rescan** if one is
   missing), patch each universe out, name the universes.
2. **Fixtures → Add** each fixture at its address — or add them to the
   inventory and use the Patch dialog's **Place selected at universe /
   address** to lay a block in at once.
3. **Lighting Studio**: build the structures and mount the fixtures on them
   (drag from the plot with Shift held, or **Mount on** from the right-click
   menu) so aim, beams and the 3D view know where they are.
4. Carry on from step 8 of Workflow 1.

## Workflow 3 — bringing a show to a venue

1. **Open the show** (⌘O, or `-o`). If the footer says definitions are
   missing, click it: they come from the show cache, your library, or can
   be repaired in the Fixture Editor.
2. **Connections**: are the nodes there? **Rescan**; check each universe
   goes where it should (**By universe** lists them in order with where
   each goes). Select a universe to watch what it sends.
3. **Check the footer** and work down its list.
4. **Test the rig.**
   - Fixture Properties → **Locate** flashes a fixture; **Test
     orientation** points a mover **straight up / down / out** (named for
     how it's mounted) or at a target, to check pan/tilt and calibration.
   - For LED-strip steps, a look with the **Step Test Pattern** effect
     shows each strip's number, each step's name and a spinner that stops
     if output freezes.
     ([Step Test Pattern](programming-tab.md#step-test-pattern))
5. **Power.** Fixtures that draw more than a circuit allows can be held to a
   maximum — measure them once and copy the profile to the same fixtures.
   ([A fixture's maximum draw](other-improvements.md#a-fixtures-maximum-draw))
6. Save under a new name for the venue.

## Workflow 4 — a row of like structures

Steps, towers and booms usually come in sets that should all be the same:

1. Build and rig **one** of them completely.
2. Copy and paste it (⌘C / ⌘V on the plot) for the others, or build them
   from a saved **Studio component**.
3. **Mount on → Deck → The deck under each** puts a selection of fixtures
   on their platforms' tops, upright, with nothing floating.
4. Right-click the first → **Patch Others Like This (N)…** and **Apply**:
   each fixture on the others gets the address of the one in the same place
   on the first, in its own structure's universe.
5. **Select All Similar** on a fixture picks every one of the same model,
   for setting a property (orientation, power profile) on all of them.

## Running the show

- **Operate** (⌘F12) — the show runs; editing that would disturb it is
  locked. **Show-mode Lock** (⌘L) locks the interface down further for an
  operator.
- **Stop ALL functions** (⌘⇧Esc), or its menu's **Fade and stop**, is the
  panic button.
- The Virtual Console, Simple Desk and Shows timeline work as in stock
  QLC+; the **Programming** tab's toolbar has Highlight / Flash / Park /
  Blind for working on a running show.

### GO on the beat

A chase (and the cue list that plays it) can hold its GO for the music:
the chaser editor's **GO at once / GO on the beat / GO on the bar**. Press
GO a little early and the next cue lands on the next beat (or bar — four
beats) of the chase's speed master, or the main beat when it has none.
While a GO is waiting, the cue list's **Next** button turns amber; two
presses in one beat are still one cue. Starting a stopped chase waits for
the beat too. A chase played by the Shows timeline goes when the timeline
says, whatever this is set to.

## Keeping it quick

- On a big show (hundreds of fixtures, many universes, pixel effects), run
  a **release build** of the app — a debug build does the same work several
  times slower, and the screen lags behind.
- In the Rig Overview, **Quality: Fast** draws a frame in about half the
  time of **Smooth**, and hiding layers you don't need leaves less to draw.
- The footer's **Load** shows how much of each 20 ms DMX tick the show
  uses; as it nears the whole tick, the output can't keep time and starts
  to stutter.

## Where next

- [What's new](whats-new.md) — the latest changes.
- [Keyboard and mouse](shortcuts.md) — every shortcut.
- [Effects](effects.md) — what each built-in effect does.
- [Troubleshooting](troubleshooting.md) — when something isn't right.
- The [feature guide index](README.md).
