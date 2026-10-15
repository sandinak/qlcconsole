# Troubleshooting

Start with the **footer**: if it says **Not ready (N)**, click it — most
problems that stop a show (unpatched or clashing fixtures, universes going
nowhere, missing definitions, WLED devices not answering or not set up)
are listed there with a fix.

## Lights

**Nothing comes out of a universe, or only part of a rig lights.**
Connections → tick **By universe**: every universe with where it goes.
One reading "not patched" has no output — right-click it and patch it to
an interface. Select a universe to see the channels it is actually
sending: if the values are there, the problem is downstream (node, cable,
the fixture's address); if not, it's the look or the patch.

**A look stays on the fixtures after its function is removed.** Fixed — a
function deleted while it runs now lets go of its channels. On an older
build, stop the function before deleting it.

**A colour/effect on a group lights fixtures outside it.** A look on a
fixture group sets only that group's own heads (a group of the colour
heads of a bar with white heads leaves the white ones alone).

**An effect reads backwards or upside down** (Text, Step Test Pattern, a
pixel chase). The strips count from the other end: use the effect's
**mirror** (and **flip**) setting, or reorder the group's head layout.

**The output stutters.** The footer's **Load** shows how much of each
20 ms tick the show uses. Heavy pixel effects on many universes, or a
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
the overview's **Layers** menu (or the Lighting Studio's Layers panel; it is
the same switch).

**The Rig Overview is slow.** Make sure its **GPU** box is ticked, then
**Quality: Fast**, hide layers you don't need, and use a release build.

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
tried. Definitions you made live in
`~/Library/Application Support/qlcconsole/Fixtures`.

**A show opened from the terminal came up empty.** Pass it with **-o**
(`qlcconsole -o show.qlcc`); a bare file name isn't opened, and saving then
can overwrite it.

**The app stopped without saving.** Open it again — it offers to recover
the unsaved work from its autosave.

**A show from another computer is missing fixtures.** Their definitions
come from the show's own **show cache**; if one is missing there too, the
footer offers to find it in your library.

## Finding things

**Where did the Functions tab go?** **Window → Functions** (⇧⌘4) — it
opens in its own window.

**A shortcut does nothing.** Check [Keyboard and mouse](shortcuts.md):
some keys only work in one view (the Virtual Console's add-widget keys
only in its Design mode, the Functions keys only in its window).

**Help** — F1 opens the help for whatever you're looking at.
