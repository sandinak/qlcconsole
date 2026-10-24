# Troubleshooting

Start with the **footer**: if it says **Not ready (N)**, click it — most
problems that stop a show (unpatched or clashing fixtures, universes going
nowhere, missing definitions, network outputs that can't send or nodes
that stopped answering, WLED devices not answering or not set up) are
listed there with a fix.

## Lights

**Nothing comes out of a universe, or only part of a rig lights.**
Connections → tick **By universe**: every universe with where it goes.
One reading "not patched" has no output — right-click it and patch it to
an interface. Select a universe to see the channels it is actually
sending: if the values are there, the problem is downstream (node, cable,
the fixture's address); if not, it's the look or the patch.

**An output or node is reported in trouble** ("not reachable", "stopped
answering", "DMX output short", "not outputting DMX" — in the footer and in
red on the universe's row in Connections). "Not reachable": check the
console's own cable and network settings, and that the address is right.
"Stopped answering": the node is off, unplugged, or its switch port is down.
"Short" / "not outputting": the DMX run from that node port — cable,
connectors, terminator, a failed fixture. Each clears by itself once fixed
(sends after 30 s working, nodes after two answered polls, ports after 10 s
clean). See
[When an output is in trouble](other-improvements.md#when-an-output-is-in-trouble).

**Universes say "not present — kept, will reconnect" after opening a
show.** The show was patched to a network this computer isn't on (a laptop
away from the show VLAN, typically). Nothing is wrong and nothing is lost:
the patches are kept and saved unchanged, and reconnect by themselves when
any interface on that network appears — no re-patching. If the computer
*is* on that network, check that the interface has an address on it. See
[Patches follow the network](other-improvements.md#patches-follow-the-network).

**The footer says "Output held — not on the rig network".** This computer
isn't on any of the networks the show's output belongs on, so its Art-Net,
sACN, OSC and WLED output is held — kept, and nothing sent — rather than
put on whatever network it is on. The footer names the show's networks and
where this computer is connected. Join the show network and it resumes by
itself; if it *is* on the show network, click the footer and check the
show's rig networks in **Settings ▸ General ▸ Network**. See
[Only transmit on the rig network](other-improvements.md#only-transmit-on-the-rig-network).

**A new Art-Net node doesn't show up, or shows amber "set its IP".** It's
still on its factory address (2.x.x.x, 10.x.x.x, its maker's default) or
self-assigned (169.254.x.x), not on your network. Press **Rescan**; if it
appears, right-click it → **Set node IP address…**. If it doesn't, or the
dialog says it can't be sent to, give this computer a temporary address on
the node's network first. See
[Setting up a new Art-Net node](other-improvements.md#setting-up-a-new-art-net-node).

**A look stays on the fixtures after its function is removed.** Fixed — a
function deleted while it runs now lets go of its channels. On an older
build, stop the function before deleting it.

**A light stays on through Blackout, or the Grand Master doesn't dim
it.** It has [output rules](fixture-manager.md#output-rules) — a house or
work light, a power relay (an Eos import sets them on its special
circuits). Its icon carries a small amber badge; right-click it →
**Output Rules…** to see or change them. A relay that comes on by itself
is **On when not in use**: when it does is **Settings ▸ Advanced ▸
Output**.

**A colour/effect on a group lights fixtures outside it.** A look on a
fixture group sets only that group's own heads (a group of the colour
heads of a bar with white heads leaves the white ones alone).

**An effect reads backwards or upside down** (Text, Step Test Pattern, a
pixel chase). The strips count from the other end: use the effect's
**mirror** (and **flip**) setting, or reorder the group's head layout.

**The output stutters.** The footer's **Load** (turn it on in Settings ▸
Appearance) shows how much of each 20 ms tick the show uses. Heavy pixel effects on many universes, or a
debug build, can fill it — run a release build, and see
[Keeping it quick](getting-started.md#keeping-it-quick).

**A fixture draws more than its circuit allows.** Give it a maximum draw:
Fixture Properties → **Max draw** → **Measure…**, then **Copy to Same
Fixtures…**. See
[A fixture's maximum draw](other-improvements.md#a-fixtures-maximum-draw).

## The Lighting Studio and the Rig Overview

**A fixture floats above a deck, or sinks into it, in Front view.**
Either it isn't on the deck (it's free-standing at some height — Fixture
Properties says "Mounted on: free / on the floor" and shows its Z), or its
**Face** is stale. Select it (or the whole row) → right-click → **Mount on
→ Deck → The deck under each**: it stands on the deck's top, upright.

**Dropping a fixture hung it on the wrong thing.** A plain drag only moves
a fixture now; hanging it takes **Shift** as you drop, and a fixture at
one height isn't offered a truss at another. To undo a wrong mount:
right-click → **Unmount from "…"**, or ⌘Z.

**A low fixture looks like it's on top of a truss in the Top view.** It's
drawn on top so you can click it; one with a structure well above it is
drawn **faded and dashed**. Check its height in Front or Side.

**Lanterns on hangers draw as dark squares.** A hanger fixture kept the
face of what it hung on before; it is straightened when the show loads
(and when it's hung) on this build. Re-open the show.

**Something is missing from the Rig Overview.** Its layer is hidden — check
the overview's **Plot layers** menu (or the Lighting Studio's Plot layers panel; it is
the same switch).

**The Rig Overview is slow.** Make sure its **GPU** box is ticked, then
**Quality: Fast**, hide layers you don't need, and use a release build.

**Light shows through a closed curtain.** Lights just behind a closed
drape used to glow through it (their haze glow and the pools of light on
the back of the cloth); this build hides them. If a light still shows,
check the drape is closed where the light is — an open or flown part
hides nothing — and that it isn't a scrim, which is see-through.

**The Rig Overview draws wrongly** — garbled beams, missing structures,
black patches. Untick its **GPU** box: it draws on the processor instead,
and a graphics driver can't get it wrong. See [Keeping it quick](getting-started.md#keeping-it-quick).

**I can't click a truss for the fixtures on it.** Show → untick
**Fixtures on the rig** to hide mounted fixtures while placing structures;
crowded structures in Front/Side collapse their fixtures into a count
badge you can click.

## Files and fixtures

**A fixture definition won't load.** The footer lists it; **Repair in
Fixture Editor…** opens it at the line that breaks, with fixes it has
tried. Definitions you made live in the **Fixtures** folder of your user
folder: `~/Library/Application Support/qlcconsole` on a Mac,
`%USERPROFILE%\qlcconsole` on Windows, `~/.qlcconsole` on Linux (your own
effect scripts, RGB scripts and LED maps are in **EffectScripts**,
**RGBScripts** and **HeadLayouts** beside it).

**A show opened from the terminal came up empty.** Pass it with **-o**
(`qlcconsole -o show.qlcc`); a bare file name isn't opened, and saving then
can overwrite it.

**The app stopped without saving.** Open it again — it offers to recover
the unsaved work from its autosave.

**A show from another computer is missing fixtures.** Save it there as a
**.qlcc**: it then carries every definition and script of its own (or use
**File ▸ Collect for Transfer…** for a folder with them beside it). A .qxw
doesn't; its definitions come from this computer's library or show cache,
and the footer offers to find a missing one. See
[Opening a show on another computer](fixture-manager.md#opening-a-show-on-another-computer).

**The footer says items in this show differ from this computer's.** The
show is using its own copy, so nothing has changed under the rig.
**Review…** says which looks newer and whether this computer's copy would
work in its place; pick **Use This Computer's**, **Keep the Show's** or
**Install the Show's** for each.

## Finding things

**A tab is missing.** The show's workspace view hides it: **View ▸
Workspace** picks **Classic QLC+**, **qlcconsole** or **Everything**, and
the view is saved with the show (an old QLC+ show opens in Classic QLC+,
without Build, the Show Tree or the Lighting Studio). Choosing a hidden
tab from **Window** shows it anyway, switching to Everything with a line
to go back. See [Views](getting-started.md#views).

**⌘Z did nothing.** Undo works in **Build**, the **Show Tree**,
**Fixtures** and the **Lighting Studio** so far (Fixtures and the Studio
share the rig's history: patching, properties, renames, plot symbols,
output rules); adding or deleting fixtures and editing groups aren't undone
yet; on other tabs **Edit ▸ Undo** reads *Nothing
to undo on this tab* and is greyed. The menu always undoes the tab in
front, never another tab's step. On **Connections** it is that tab's own
⌘Z, **Undo Patch Change** (one step, asked first, no redo).

**Where did the Functions tab go?** The qlcconsole view has none; the
Classic QLC+ and Everything views show it. **Window ▸ Functions** opens it,
switching to Everything with a line above the tabs to go back. New
functions of every type, the Function Wizard and the startup function are
also on Build's **+ Scene ▾** and the outline's right-click
([Coming from the Functions window](build.md#coming-from-the-functions-window)).

**A Show Tree cue changed its number.** A cue's number is its place in
the list: dragging a cue renumbers it for where it lands, and typing a new
number moves it. Adding a cue never renumbers the others. ⌘Z puts it
back; see [Reordering and numbering](show-tree.md#reordering-and-numbering).

**Esc didn't stop anything.** Plain Esc backs out of an editor (a palette,
a group's layout, the effect bench, the Workbench docked in the Show
Tree) to what was showing before. **Stop all** is **⌘⇧Esc**
([Keyboard and mouse](shortcuts.md)).

**A shortcut does nothing.** Check [Keyboard and mouse](shortcuts.md):
some keys only work in one view (the Virtual Console's add-widget keys
only in its Design mode, ⌘1 … ⌘9 for a new function only in Build ▸
Scenes).

**Help** — F1 opens the help for whatever you're looking at.
