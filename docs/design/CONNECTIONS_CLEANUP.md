# Connections: Devices and Overview

Status: **decided and done** (board Q153, 2026-10-26: *"A, and switch
Fixture Manager's Patch to the new window"*). Built in 0.3.0 RC3 -- see
[What was built](#what-was-built) at the end. The proposal is kept below as
it was put.

Branson, testing 0.3.0 RC1: *"In connections, Devices and Overview are the
same thing different ways. Start a thread to look at those two and consider
a cleanup."*

This compares the two sub-tabs and offers three ways to clean them up. The
third sub-tab, **Universes** (QLC+'s own Inputs/Outputs page), stays as it
is: it is the Classic QLC+ view's page and the reference for stock behaviour.

## What each one is

**Devices** (`ConnectionsTree`, ui/src/connectionstree.*) follows the signal
outward: protocol → interface → devices heard on it → their ports → the
universes on them. **By universe** turns it into one row per universe, each
saying where it goes. On the right, what the selected universe is sending,
channel by channel. Show control & sync rows at the foot.

**Overview** (`UniversePatchGrid`, ui/src/universepatchgrid.*) is a
spreadsheet, one row per universe: Uni, Name, Usage, then Output / Out IP /
Out U / Mode always, and Input / In / Profile, Feedback / FB IP / FB U and PT
as column groups you turn on from its toolbar. Every cell edits in place.

## Side by side

| | Devices | Overview |
|---|---|---|
| One row per universe | **By universe** | always |
| Where a universe goes, in words | "Art-Net → CS1 (172.18.2.211) · port 1 · universe 0:0:0" | the plugin:line, the IP and the port address in three cells |
| What a universe carries | Carries column (fixtures, heads, channels used) | Usage column (fixtures, highest channel) |
| Live input on a universe | the row flashes | its input cell flashes |
| Devices heard on the network, their ports, which are free | **yes** (only here) | no |
| Devices added by hand, from the device library, node IP | **yes** (only here) | no |
| Naming interfaces and devices | **yes** (only here) | no |
| Patches waiting for an interface that is not here | **yes**, with Forget | not shown as such |
| Output trouble (a node not answering, off the rig network) | **yes**, in red | no |
| What a universe is sending (512 channels) | **yes**, on the right | no |
| Show control & sync (OSC, MSC, MTC) | **yes** | no |
| Patch / edit a universe | the connection window (Edit connection…, double-click) | inline combos and cells |
| Input, profile, feedback, passthrough | in the connection window | inline, in column groups |
| Several universes at once | Patch / Unpatch / Retarget N selected, in the order clicked, one undo step | edit one cell, the selection follows (auto-increment of the port address) |
| Add / remove a universe | right-click | toolbar and right-click |
| Undo Patch Change | yes | yes (the same action) |

**The overlap** is the per-universe patch: both list every universe with
where it goes, what it carries and its input/feedback, and both can change
it. That is what reads as "the same thing different ways".

**Only Devices** has the network side: what is out there, which ports are
free, trouble, names, hand-added nodes, the output view, show control.

**Only Overview** has density: every universe's whole patch on one screen
with nothing to open, and type-down editing (fix the IP of twelve universes
by editing twelve cells). Its one bulk trick, auto-incrementing the port
address down a selection, is what Devices' "Patch N selected universes to…"
does in clicked order with conflicts refused.

Since RC2 the connection window (`ConnectionEditor`) covers every field the
Overview row edits -- output, interface, IP, port address, transmit mode,
input, profile, feedback, passthrough -- plus sACN's and MIDI's own, which
the grid does not have. So the grid no longer holds anything that can only
be edited there.

## Options

### A. One page: Devices, with By universe as a table

Fold Overview into Devices. **By universe** becomes the grid's view of the
same rows -- the universe columns of Overview (Output, IP, U, Mode, Input,
Profile, Feedback, PT, toggled as now) as read-only columns, edited through
the connection window (double-click, or Edit connection…). The wiring view
stays as it is. The Overview sub-tab goes; Connections has Devices and, in
Classic, Universes.

```
Connections  [ Devices ] [ Universes ]
 [x] By universe   [ ] Show unused   Columns: Input Feedback PT    Undo  Rescan
 ┌──────────────────────────────────────────────────────────────┬──────────┐
 │ Uni  Name     Goes to                       Mode  In   FB  PT│ sending  │
 │ 1    Stage L  Art-Net → CS1 · port 1 · 0:0:0 Std  —    —   □ │ ▮▮▮▯▯▯…  │
 │ 2    Stage R  Art-Net → CS1 · port 2 · 0:0:1 Std  —    —   □ │          │
 │ 3    House    sACN 3 · unicast 10.0.0.4      Full —    —   □ │          │
 └──────────────────────────────────────────────────────────────┴──────────┘
```

Gains: one place, one set of words, one way to edit; the wiring and the
table can never disagree. Loses: typing straight into a cell (every edit
is a window, though one that opens filled in, on the field you
double-clicked).

### B. Keep both, give each a distinct job

**Devices** is the network: what is out there, its ports, its trouble, its
names, show control. Its universe rows stop editing patches beyond a
double-click into the connection window. **Overview** is the patch sheet:
the one place patches are bulk-edited inline, renamed **Patch** to say so,
gaining the columns it lacks (sACN universe and priority, MIDI mode) and the
Carries/trouble markers so it reads like Devices. By universe goes from
Devices, since Patch is that list.

```
Connections  [ Network ] [ Patch ] [ Universes ]
  Network: protocol → interface → node → ports      (who is out there)
  Patch:   one row per universe, every cell editable (where each goes)
```

Gains: each tab has one question to answer; keeps fast inline editing.
Loses: two places can still change a patch (the sheet and the window), so
the two must keep using one patch path (`UniversePatcher`), and the grid
gains columns to keep up.

### C. Leave both, hide Overview by view

Keep the code; show Overview only in the **Everything** view (as other stock
tabs are hidden per view, board Q35/Q36), and Devices in qlcconsole. Nothing
is lost for anyone who wants the grid; the qlcconsole view has one page.

Gains: no work beyond a view rule; no behaviour lost. Loses: the duplication
stays in the code and in the help, and the grid falls further behind (no
sACN, no MIDI mode, no trouble markers).

## Recommendation

**A.** The connection window already does everything the Overview row does,
and more; what the grid still adds is density, which By universe can give
as columns without a second editing surface. One page, one set of words for
where a universe goes, one edit path (and one undo step per edit) is the
same direction as the rest of 0.3's consolidation (one Fixture Properties,
one connection window). If inline cell editing turns out to be missed on a
big rig, B's sheet can come back later as By universe's editable mode
without bringing back a second tab.

**Board question:** *Connections has Devices and Overview showing the same
per-universe patch. A: fold Overview into Devices (By universe gets the
grid's columns, edits through the connection window; the Overview tab
goes). B: keep both with separate jobs (Devices = the network, Overview
renamed Patch = the bulk patch sheet). C: keep both, show Overview only in
the Everything view. Recommended: A.*

## What was built

Option A, 0.3.0 RC3.

- **By universe has the patch as columns**: Output, IP, U, Mode, Input
  (with its Art-Net universe or MIDI channel), Profile, Feedback (with its
  address) and PT, read-only, each shown or hidden from the list header's
  right-click or a **Columns** button (By universe only) and remembered
  (QSettings `connections/universeColumns`; Mode, Input, Feedback and PT
  to begin with, since Detail already says where each goes). A
  double-click on a column opens the connection window on that part.
  `ConnectionsTree::setUniverseColumns()`.
- **The Overview sub-tab is gone** and `UniversePatchGrid` with it.
  Connections has Devices and Universes in every view; Classic still opens
  on Universes.
- **The Fixture Manager's universe rows** open the connection window,
  filled in with that universe: **Patch output…**, or **Edit connection…**
  on a patched one (it was **Change interface / target…**).
  `UniversePatcher::patchUniverse()` (the step-by-step questions) and
  `ConnectionsTree::patchUnpatchedUniverseTo()` are removed; UniversePatcher
  stays -- the connection window, the plural patch/unpatch/retarget and the
  folded port rows all use it.

What the Overview grid edited inline, and where it is now:

| Overview | Now |
|---|---|
| Name | the connection window's Name; inline rename on the row |
| Output device (plugin · line) | the window's Protocol and Interface |
| MIDI Out/FB role toggle | the window: Protocol *Not patched*, Feedback to the MIDI port |
| Out IP, Out U (Art-Net) | the window's Send to / Node IP / Universe on the node |
| Out IP, Out U (sACN) | the window's sACN universe, multicast/unicast (the grid wrote `outputIP`, which E1.31 does not read) |
| Mode (Art-Net), MIDI output mode | the window's Transmit mode, MIDI output mode |
| Input device, In (Art-Net input universe), Profile | the window's Input, Art-Net input universe, Input profile |
| In (MIDI channel) | the window's MIDI input channel -- **fixed**: it was one off against the plugin's numbering (0-15, 16 for every channel) |
| Input "Edit…" (the input plugin's settings) | Configure on the protocol's row in Devices |
| Feedback, FB IP, FB U | the window's Feedback to, Feedback address, Feedback universe |
| PT (shown, not edited) | the window's Passthrough; the PT column |
| Usage | the Carries column |
| Several rows: IP, port address numbered up | Retarget N selected universes… (and Patch N selected universes to…) |
| Several rows: transmit mode | **added**: Transmit mode of N selected universes (Art-Net, sACN) |
| Add / Remove Universe, Rescan, Undo Patch Change | Devices' right-click, Rescan and Undo Patch Change |
| Input activity flash | the row flashes |
