# Keyboard and mouse

⌘ is Ctrl on Windows and Linux; ⌥ is Alt.

## Everywhere

| Keys | Does |
|---|---|
| **F1** | Help on what you're looking at (the tab, window or editor in front) |
| ⌘N / ⌘O / ⌘S | New / Open / Save workspace |
| ⌘, | Settings (autosave, appearance, …) |
| ⌘F12 | Switch between **Design** and **Operate** |
| ⌘L | Show-mode lock |
| ⌘⇧Esc | **Stop ALL functions** (its arrow: fade 1/5/10/30 s and stop) |
| ⌘D | Dump the current DMX values into a function |
| ⌘↩ / ⌘⇧↩ | **GO** / **Back** on the main cue list ([Show control](show-control.md#whats-next-and-go-from-anywhere)) |
| ⌥⌘↩ / ⌥⌘⇧↩ | **Sub GO** / **Sub Back**: the next / previous [sub-cue](show-control.md#sub-cues) of the cue playing |
| ⇧⌘1 … ⇧⌘9, ⇧⌘0 | Bring forward Connections, Fixtures, Lighting Studio, **Functions** (Build ▸ Scenes ▸ Classic), Build, Show Tree, Shows, Virtual Console, Simple Desk, **Run** ([the cue runner](show-tree.md#the-cue-runner)) |
| Space (in the Run tab) | **GO** for the list the Run tab plays |
| ⇧⌘M | Lighting Studio |
| ⇧⌘R | **Rig Overview** — and again to close it when it's in front |
| ⌘W | Close the window in front (Rig Overview, a broken-out view…) |
| ⌘M | Minimize |
| ⇧F1 | The stock QLC+ reference (online) |

**Right-click** a **tab** → **Open in Its Own Window**, or **Shift-click**
it, to break it out into its own window (or **Window → Move … to Its Own
Window**). On a Mac, if the app isn't in front, the first click only brings
it forward — Shift-click again, or use the right-click menu, which works
either way; **Window → Put Back in Main Window** returns
it. **Window → Shift-Click a Tab to Open It in a Window** turns the
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
| ⌘C / ⌘V | Copy / paste structures with their fixtures |
| ⌘Z | Undo the last move, paste or edit |
| ] / [ | Turn the selection 15° clockwise / anticlockwise |
| } / { | Turn it 1° |

**In a structure's editor:** drag fixtures to place them, ⌘Z undoes the
editor's last change, a selected bar's ring rotates it (⇧ snaps to 15°).

**In the Rig Overview / Angled view:** drag to swing, ⇧-drag to pan, scroll
to zoom, ⇧⌘R or ⌘W to close.

**Focus mode** (⌘⌥F, or View + Menu on the controller): ] / [ next and
previous head, arrows nudge 0.1° (⇧ for 1°), + / − step size, Return save,
Backspace revert, 0 zero, R the other reference, ⌘Z undo the last save, Esc
leave. See [Focus mode](lighting-studio.md#focus-mode).

## Build

| Keys / mouse | Does |
|---|---|
| Drag a group onto a scene (outline or Workbench) | Dynamic target (follows the group) |
| Drag a fixture onto a scene | Fixed target |
| Drag a palette onto a scene | Add it as a look |
| Drag a scene onto the Workbench's layers | Add it to the collection loaded |
| ⌫ on a Workbench layer | Take it out of the collection (the function stays) |
| Double-click a ▣ layer | Open it in the Workbench (↩ comes back) |
| ⌘→ / ⌘← | Expand / collapse every row |
| ⌘Z / ⇧⌘Z | Undo / redo the last change, here or in the Show Tree (one history) |
| Double-click a scene / function | Open it on the canvas |

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
| **⌘C** / **⌘V** / **⌘D** (in the tree) | Copy cues or layers / paste after (into) the selected cue / duplicate |
| ⌥-click **+ Layer** | A layer for each group selected in the Library |
| Drag a cue onto another cue | Move it there; with **⌥**, include it |
| Delete / Backspace (in the tree) | Delete the selected cue or layer (a list or production asks first) |
| **:** (in the tree) | To the command line at the bottom ([The command line](show-tree.md#the-command-line)) |
| **Esc** (in the command line) | Close the help, then back to the tree |
| **Tab** (in the command line) | Complete the word; again, the next match |
| `help` / `?` (in the command line) | Every word, or `help <word>` with examples |
| ↑ / ↓ (in the command line) | Earlier / later commands |
| ⌘Z / ⇧⌘Z | Undo / Redo in the tree (in the command line ⌘Z undoes typing: type `undo`) |

## Connections

| Keys | Does |
|---|---|
| ⌘N | Add a universe (Detailed tab) |
| ⌘D | Delete the selected universe (Detailed tab) |
| ⌘Z | Undo the last patch change (any of the three tabs) |

## Build ▸ Scenes (what the Functions window had)

| Keys | Does |
|---|---|
| ⌘1 … ⌘9 | New scene, chaser, sequence, EFX, collection, RGB matrix, script, audio, video (with the Scenes section focused) |
| ⌘C then ⌘V | Duplicate (the Functions window's Clone) |
| ⌫ / Delete | Delete |
| F2 | Rename |
| ⌥→ / ⌥← | Next / previous fixture tab in a sequence's scene editor |

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

## Virtual Console (Design mode)

| Keys | Does |
|---|---|
| ⇧⌘B / G / S / I / K / D / X / C / F / O / L / A / T / N | Add a button, button matrix (grid), slider, slider matrix, knob, speed dial, XY pad, cue list, frame, solo frame, label, audio triggers, clock, animation |
| ⌘X / ⌘C / ⌘V / Delete | Cut / copy / paste / delete widgets |
| ⌘E | Widget properties |
| ⇧B / ⇧I / ⇧F / ⇧O | Background colour / image, foreground colour, font (⇧⌥ resets) |
| ⇧S / ⇧R / ⇧⌥S | Sunken / raised / no frame |
| ⇧↑ / ⇧↓ | Raise / lower |
| Drag a Cue Runner's dotted bars | Its buttons' height; GO's share of the row ([the cue runner](show-tree.md#the-cue-runner)) |
