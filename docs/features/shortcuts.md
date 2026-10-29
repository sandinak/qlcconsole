# Keyboard and mouse

⌘ is Ctrl on Windows and Linux; ⌥ is Alt.

## Everywhere

| Keys | Does |
|---|---|
| **F1** | Help on what you're looking at (the tab, window or editor in front); in the Show Tree's command line, its syntax lines |
| ⌘N / ⌘O / ⌘S | New / Open / Save workspace |
| ⇧⌘S | Save As (Ctrl+Shift+S on Windows and Linux too) |
| ⌘, | **Settings…** (autosave, appearance, …): in the qlcconsole menu on a Mac, File elsewhere |
| ⌘F12 | Switch between **Design** and **Operate** |
| **Esc** | **Back out** of an editor to what was showing before: a palette opened from a list, a group's layout, the effect bench, a ▣ layer, the Workbench docked in the Show Tree, the Show Tree's Look Editor. What you changed stays changed. In a rename or another inline edit, Esc cancels just that edit; in a dialog it closes the dialog. Plain Esc stops nothing |
| ⌘⇧Esc | **Stop ALL functions** (its arrow: fade 1/5/10/30 s and stop) |
| ⌘D | Dump the current DMX values into a function |
| ⇧⌘N | **New here**: the next thing for where you are. In the Show Tree a production, a cue list, the next cue, a layer or a hit; in Build a scene in the folder, a step or a scene layer in the Workbench, an effect; in Fixtures a group in the folder. **Edit** names it first ([Show Tree](show-tree.md#new-here), [Build](build.md#new-here)). The Virtual Console keeps ⇧⌘N for an animation widget |
| ⌘↩ / ⌘⇧↩ | **GO** / **Back** on the main cue list ([Show control](show-control.md#whats-next-and-go-from-anywhere)); with a [venue focus cue](focus-calls.md#a-venue-focus-cue-pre-show) waiting, ⌘↩ begins its calls instead |
| ⌥⌘↩ / ⌥⌘⇧↩ | **Sub GO** / **Sub Back**: the next / previous [sub-cue](show-control.md#sub-cues) of the cue playing |
| ⇧⌘1 … ⇧⌘9, ⇧⌘0 | Bring forward the view's tabs, numbered as they are on screen (⇧⌘0 the tenth). qlcconsole: Connections, Fixtures, Lighting Studio, Build, Show Tree, Virtual Console, Simple Desk, **Run** ([the cue runner](show-tree.md#the-cue-runner)). Classic QLC+: Fixtures, Functions, Shows, Virtual Console, Simple Desk, Inputs/Outputs. Everything: Connections, Fixtures, Lighting Studio, Functions, Build, Show Tree, Shows, Virtual Console, Simple Desk, Run. They renumber when the [view](getting-started.md#views) changes; a tab the view hides has no number in the Window menu, and choosing it switches to Everything |
| Space (in the Run tab) | **GO** for the list the Run tab plays |
| ⇧⌘M | Lighting Studio |
| ⌥⌘F | **Quick focus** (View ▸ Quick Focus…) |
| ⇧⌘R | **Rig Overview** — and again to close it when it's in front |
| ⌘W | Close the window in front: the Rig Overview, a structure's editor (as Cancel), a dialog. In the Classic Functions tab ⌘W is the Function Wizard |
| ⌘Q | Quit (Ctrl+Q on Windows and Linux) |
| ⌘M | Minimize |
| ⌃⌘F (also ⌘F11; Ctrl+F11 on Windows and Linux) | **Full Screen** (View ▸ Full Screen), and back |
| ⇧F1 | The stock QLC+ reference (online) |

**Right-click** a **tab** → **Open in Its Own Window**, or **Shift-click**
it, to break it out into its own window (or **Window ▸ Move … to Its Own
Window**). On a Mac, if the app isn't in front, the first click only brings
it forward — Shift-click again, or use the right-click menu, which works
either way; **Window ▸ Put Back in Main Window** returns
it. **Window ▸ Shift-Click a Tab to Open It in a Window** turns the
Shift-click off, if it gets in the way.

## Lighting Studio (the plot)

| Keys / mouse | Does |
|---|---|
| Click / ⌘-click / ⇧-click | Select / add to or take from the selection |
| Drag | Move what's selected — **only moves** |
| **⇧ held as you drop** a fixture | Hang it on the truss or pipe, or stand it on the deck, it's over (outline turns green while it would) |
| Drag a mounted fixture well off | Takes it off its truss / deck |
| Double-click | Edit (a structure opens its editor; a fixture its properties) |
| Right-click | Everything else: Mount on (Truss / Boom / Hanger / Tower / **Deck**), Patch, Patch Others Like This, Rotate, groups, layers, Delete (asks first)… |
| ⇧-drag on empty canvas | Pan |
| ⇧-scroll | Zoom, centred on the pointer |
| ⌘A | Select all |
| ⌘C / ⌘V | Copy / paste trusses, decks, targets, scenery, towers, booms and stands; a truss brings its fixtures as new copies, and the paste asks: **Patch at next free addresses** (after the originals), **Leave unpatched** (**Patch Others Like This** addresses them later), **Truss only** or **Cancel**. The status line says what happened |
| ⌘Z | Undo the last move, paste or edit |
| ] / [ | Turn the selection 15° clockwise / anticlockwise |
| } / { | Turn it 1° |
| ⌘G / ⇧⌘G | Group / ungroup the selection |
| Pinch | Zoom |
| Click a footer box (View, Overlay) | Its next choice; its arrow lists them all |
| ⇧-drag / ⌥-drag a line target's handle | Keep it level or upright / extend an end along the line |

**In a structure's editor:** drag fixtures to place them, drag empty
canvas to pan (in 45°, to swing), scroll or pinch to zoom, ⌘- or ⇧-click
to add to the selection, double-click to edit, ⌘Z undoes the editor's
last change, a selected bar's ring rotates it (⇧ snaps to 15°), Return or
F2 renames a folder in its list. **⌘W closes it as Cancel**, putting back
everything it did.

**In Plot layers:** Return or F2 renames a row, Esc cancels; drag a
fixture's row onto a hanger, bar, tower or boom to hang it there.

**In the Rig Overview / Angled view:** drag to swing, ⇧-drag to pan, scroll
or pinch to zoom, right-click a fixture for **Fixture Properties…** or
**LED Layout…**, ⇧⌘R or ⌘W to close.

**Quick focus** (⌘⌥F, or View + Menu on the controller) and **Venue
focus**: ] / [ (or Tab / ⇧Tab) next and previous light, arrows nudge 0.1°
(⇧ for 1°), + / − step size, , / . spot down / flood out (⇧ 5°), ; / '
softer / sharper, G one light or all, Return save (⇧Return save all),
Backspace revert (⇧Backspace every unsaved light), 0 zero, R the other
reference (in Venue focus R or T, the target page), PgDn / PgUp next /
previous focus call, U next unsaved, N how many unsaved, D mark dead, H
hold every light, W say the light (⇧W feedback), ⌘Z undo the last save,
Esc leave. See [Quick focus](lighting-studio.md#quick-focus) and
[Running venue focus](focus-calls.md#running-venue-focus).

**Focus this cue / Focus this look**: PgDn / PgUp layers, ] / [ (or
Tab / ⇧Tab) fixtures or groups, G scope, arrows move 1° (⇧ 5°), , / .
spot down / flood out, ; / ' softer / sharper, P the D-pad page, Return
keep (⇧Return keep in the cue it tracks from), S make that the rule,
Backspace back to the cue, 0 clear the exception, U next change not kept,
W say the light (⇧W feedback), H hold, ⌘Z undo, Esc leave. See
[Focus this cue](refine.md#changing-values).

## Build

| Keys / mouse | Does |
|---|---|
| Drag a group onto a scene (outline or Workbench) | Dynamic target (follows the group) |
| Drag a fixture onto a scene | Fixed target |
| Drag a palette onto a scene | Add it as a look |
| Drag a scene onto the Workbench's layers | Add it to the collection loaded |
| ⌫ on a Workbench layer | Take it out of the collection (the function stays) |
| Double-click a ▣ layer | Open it in the Workbench (↩ comes back) |
| **Esc** | Back out one level: a palette double-clicked open goes back to the page it covered (as its **↩ Back**), a group's layout to its scene (**← Back**), the effect bench to the scene it was opened from (asking first if the effect hasn't been kept), a ▣ layer to its collection (**↩**) ([Backing out](build.md#backing-out-esc)) |
| ⌘→ / ⌘← | Expand / collapse every row |
| ⌘Z / ⇧⌘Z | Undo / redo the last change, here or in the Show Tree (one history) |
| ⇧⌘N | New here: a scene in the selected folder; in the Workbench a scene layer (collection) or the next step (chaser); in Effects a new effect ([New here](build.md#new-here)) |
| Click a row (outline) | Load it in the Workbench |
| Double-click a palette / a group | The palette in the Look Editor / the group's head layout |
| **Return** (navigator / Workbench rows) | To the Workbench's layers / to the editor pane |
| **Return** / **F2** (Palettes, Effects, Groups &amp; fixtures) | Rename in place |
| ⌫ (Palettes, Effects) | Delete what's selected, asking first and saying where it's used |
| ⌫ (a look or target row) | Take it out of the scene; on a cue's value row in the Workbench, clear it |
| ⌥→ / ⌥← · ⌥-click a triangle | Open / close the row and everything under it |
| **⌥ held as you drop** rows | Copy instead of move |
| **Esc** (a step's Fade, Hold or Cue field) | Put back what the step has |
| ⇧↩ / ⌥↩ (in a note) | New line |
| ⌘X / ⌘C / ⌘V (chaser editor) | Cut / copy / paste steps |

## Show Tree

| Keys / mouse | Does |
|---|---|
| Drag from the Library onto a cue, a layer, or the page | One layer on everything dropped; onto a layer, added to it ([Adding layers](show-tree.md#adding-layers)) |
| **⌥ held as you drop** groups | A layer for each group (a group set: each member) |
| **⇧ held as you drop** groups on a layer | A child layer on them ([Child layers](show-tree.md#child-layers)) |
| Drop below the tree's last row | A new layer in the selected cue |
| **Return** / double-click (in the Library) | What is selected there goes to the tree's selection, as a drop would ([Adding layers](show-tree.md#adding-layers)) |
| **⌘F**, or **/** in the tree | To the Library's filter; type, then Return adds the first match |
| **Return** / **F2** (in the tree) | Rename the row in place ([Renaming](show-tree.md#renaming)) |
| **⇧⌘N** | New here: a production, a cue list, the next cue after the selected one, a layer on the Library's groups, a hit ([New here](show-tree.md#new-here)) |
| **⌘C** / **⌘V** / **⌘D** (in the tree) | Copy cues or layers / paste after (into) the selected cue / duplicate |
| ⌥-click **+ Layer** | A layer for each group selected in the Library |
| Drag cues up or down (before / after a row) | Move them there, renumbered for their place; with **⌥**, include it ([Reordering and numbering](show-tree.md#reordering-and-numbering)) |
| Delete / Backspace (in the tree) | Delete the selected cue or layer (a list or production asks first) |
| **:** (in the tree) | To the command line at the bottom ([The command line](show-tree.md#the-command-line)) |
| **Esc** (in the command line) | Close the help, then back to the tree |
| **Esc** (anywhere else in the tab) | The Workbench docked: back out a level, then **↩ Show Tree**. Otherwise the Look Editor under the page closes (**↩ Back** on it does the same) |
| **Tab** (in the command line) | Complete the word; again, the next match |
| `help` / `?` (in the command line) | Every word, or `help <word>` with examples |
| ↑ / ↓ (in the command line) | Earlier / later commands |
| ⌘Z / ⇧⌘Z | Undo / Redo in the tree (in the command line ⌘Z undoes typing: type `undo`) |
| **⌘/** (in the tree) | Skip the selected cues, or put them back |
| **⌥↩** (Library) | A layer for each group selected |
| ↓ / Esc (Library filter) | Into the list / back to the tree |
| **F2** / ⌫ (Library) | Rename / delete the palette or folder |
| **?** typed first, or **F1** (command line) | The syntax lines |
| **Esc** (a cue's fade, hold or wait) | Put the cue's own time back |
| Return / Esc (the Level… / Colour… strip) | Apply / cancel |
| Click / double-click a cue (Run tab, Cue Runner) | LOAD it / jump to it now |

## Connections

| Keys | Does |
|---|---|
| ⌘N | Add a universe (Universes tab) |
| ⌘D | Delete the selected universe (Universes tab) |
| ⌘Z | Undo the last patch change (**Undo Patch Change**; Devices or Universes) |
| Double-click a universe | Its connection window: protocol, interface, port, input and feedback ([Editing a connection](other-improvements.md#editing-a-connection)) |
| ⌘-click universes, then right-click | Retarget them, or set their transmit mode, all at once |

## Fixtures

| Keys | Does |
|---|---|
| **Return** / **F2** | Rename the selected fixture, group, group folder or universe in place; Esc cancels ([Layout](fixture-manager.md#layout)) |
| Double-click | A group opens its grid editor, a fixture its properties, a folder renames |
| ⌘Z | Undo the rig's last change: a Patch dialog **Apply**, properties, renames, plot symbols, output rules |
| Drag fixtures or a group onto a universe | The **Patch** dialog, packed into that universe, to review and apply |

## Build ▸ Scenes (what the Functions window had)

| Keys | Does |
|---|---|
| ⌘1 … ⌘9 | New scene, chaser, sequence, EFX, collection, RGB matrix, script, audio, video (with the Scenes section focused) |
| ⌘C then ⌘V | Duplicate (the Functions window's Clone) |
| ⌫ / Delete | Delete |
| F2 | Rename |
| ⌥→ / ⌥← | Next / previous fixture tab in a scene editor |

The Function Wizard and the startup function are on **+ Scene ▾**; a new
folder is **+ Scene ▾ → New scene in a new folder…**.

## Shows (timeline)

| Keys | Does |
|---|---|
| Space / ⌘Space | Play / stop |
| ⌘H | New show |
| ⌘N | Add a track or an existing function |
| ⌘E / ⌘A / ⌘D | New sequence / audio / video |
| ⌘C / ⌘V / Delete | Copy / paste / delete |
| ⌘G | Snap to grid |
| ⌘T | Item start time and duration |
| ⌘K | Lock item |
| ⌘Z | Undo |
| ⌘L | Change the item's colour |

## Functions (Classic QLC+ and Everything)

| Keys | Does |
|---|---|
| ⌘1 … ⌘9 | New scene, chaser, sequence, EFX, collection, RGB matrix, script, audio, video |
| ⌘L | New folder |
| ⌘W | Function Wizard |
| ⌘C | Clone |
| Delete | Delete |
| ⌘A | Select all |

## Virtual Console (Design mode)

| Keys | Does |
|---|---|
| ⇧⌘B / G / U / I / K / D / X / C / F / O / L / A / T / N | Add a button, button matrix (grid), slider (⇧⌘U: ⇧⌘S is Save As), slider matrix, knob, speed dial, XY pad, cue list, frame, solo frame, label, audio triggers, clock, animation |
| ⌘X / ⌘C / ⌘V / Delete | Cut / copy / paste / delete widgets |
| ⌘E | Widget properties |
| ⇧B / ⇧I / ⇧F / ⇧O | Background colour / image, foreground colour, font (⇧⌥ resets) |
| ⇧S / ⇧R / ⇧⌥S | Sunken / raised / no frame |
| ⇧C / ⇧⌥C | Choose a button's icon / no icon |
| ⇧↑ / ⇧↓ | Raise / lower |
| Drag a Cue Runner's dotted bars | Its buttons' height; GO's share of the row ([the cue runner](show-tree.md#the-cue-runner)) |
