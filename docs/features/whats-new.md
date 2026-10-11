# What's new

The latest changes, newest first. Each links to the guide with the detail.

## October 2026

### Help

- **In-app help.** **F1** opens the guide for whatever is in front — the
  tab, the Rig Overview, Fixture Properties, the Patch dialog, the look
  editor, the power dialogs. **Help** lists the guides: Getting Started,
  What's New, Keyboard and Mouse, Effects, Troubleshooting, and all of
  them; search finds a word in every guide. The stock QLC+ reference is
  still there, online, as **QLC+ Reference** (⇧F1).
- New guides: [Shows (the timeline)](shows.md) — what tracks, items,
  cues and the show end are; [Getting started](getting-started.md) (starting the app and
  the ways a show comes together), [Keyboard and mouse](shortcuts.md),
  [Effects](effects.md) and [Troubleshooting](troubleshooting.md).

### Lighting Studio

- **A drag only moves; Shift-drop attaches.** Dropping a fixture over a
  truss, pipe or deck no longer hangs it there unless you hold **Shift** as
  you let go; the outline turns green while it would, and updates the
  moment Shift is pressed or released. Coming off still needs no Shift, and
  fixtures dragged in from the Inventory are placed where they land.
  ([Mounting](lighting-studio.md#mounting-a-fixture))
- **Height counts when attaching.** A fixture mounted on something (the
  foot of a tower, a deck) is only offered a truss or pipe within half a
  metre of its own height — no more lights at a tower's base hung on the
  bar on top.
- **Mount on → Deck.** Stands every selected fixture on the deck under it
  (or a named one): on its top, upright, nothing floating, nudged inside a
  deck it sat on the edge of.
- **Fixtures under structures** draw faded with a dashed outline in the
  Top view, still clickable.
- **Rulers mark the middle** of whatever you drag — a drape's centre line,
  not where you grabbed it — and band any selection, not only fixtures.
- **Orientation for anything that pans or tilts**, even bar-shaped (a
  two-arm beam bar standing on a step); **Test orientation** names its
  centred pose for it — straight up, down or out.
- **Lanterns on hangers hang straight.** A face left over from an earlier
  mount drew some on their side; it's cleared when the show loads.
- **Rig Overview** (⇧⌘R) **follows the Studio's layers** — hide a layer,
  it leaves the overview — and the same shortcut, or ⌘W, closes it.
- **Faster rig view**: a 200-fixture show draws in about 9 ms a frame (it
  was 51).

- **A truss between two stands** — the truss editor's **Other end on
  stand**: it runs from one stand's top to the other's and follows them.
- **Hangers on the 2D plot** — arm and hook, double-click to edit.
- **A patch can be undone** — the Patch dialog's Apply is one ⌘Z, on the
  Fixtures tab or in the Studio.
- A **house electric** (horizontal pipe) lists its fixtures by distance
  **along** it, up to its length, rather than a boom's height.
- The **Network Topology** dialog works like every rig dialog: the plot
  follows as you edit, Cancel puts the network back, OK is one undo.
- The Virtual Console's **button matrix** and **animation** keys moved to
  ⇧⌘G and ⇧⌘N, off the app's Lighting Studio and Rig Overview keys.

### Patching

- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, to save as **CSV** or a printable **PDF** for the crew.
  **Fixtures → Patch List…**, or the Studio's **More** menu.
  ([The patch list](fixture-manager.md#the-patch-list))
- **Patch Others Like This.** Right-click a structure: the other
  structures of its kind with the same fixtures are patched as it is — each
  fixture at the address of its counterpart, in its own structure's
  universe. The Patch dialog offers the same **Match** whenever its fixtures
  span several structures.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))

### Effects

- **Text** — words on a pixel grid, scrolling or still in the middle, in
  the look's colours; settings that are words get a text box.
  ([Text](programming-tab.md#text))
- **Step Test Pattern** — a diagnostic card for LED-strip steps: strips
  counted, steps numbered and named, a spinner that stops if output
  freezes. ([Step Test Pattern](programming-tab.md#step-test-pattern))
- The RGB Matrix **Text** algorithm has a **Centered** style.
- **Much faster on big pixel rigs**: 96 strips × 64 pixels ticks in about
  8.5 ms (was 68), leaving the screen free to keep up.
- The **effect list sits beside its settings** in the look editor, so both
  fit on an ordinary screen.

### Views and windows

- **Shift-click a tab** to put it in a window of its own — a double-click
  no longer does it by accident. **Window → Move … to Its Own Window** does
  the same for the view in front, and the Window menu can turn the
  Shift-click off. ([Window menu](other-improvements.md#window-menu))
- **Functions has no tab** — **Window → Functions** (⇧⌘4) opens it in its
  own window. ([Window menu](other-improvements.md#window-menu))
- The **Window** menu lists every view, wherever it is, and puts
  broken-out ones back.
- **Connections**' Devices header is one row; the list gets the height.

### Earlier this month

- **Power**: a fixture's **maximum draw** — amps, proportional, measured
  state by state, set on many at once and copied to the same fixtures —
  holds a hungry fixture under its circuit.
  ([Maximum draw](other-improvements.md#a-fixtures-maximum-draw))
- **Connections**: only a universe's name is edited in place; select a
  universe to see what it's sending, live.
- A look on a fixture **group** sets only that group's own heads.
- A function deleted while running lets go of its channels.
- **Scenery** (cyc, walls, drapes with an opening, a proscenium, a rounded
  front), **fountains** (with a silk-flame fixture and a labelled diagram),
  **lasers and star projectors**, a beam split across the surfaces it
  crosses, group rotation, and count badges on towers.
  ([Lighting Studio](lighting-studio.md))
