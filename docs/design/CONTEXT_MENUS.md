# Context menus: inventory and canon

*2026-11-02. Ask: "go thru every submenu and right click and make sure
we're consistently laid out … right click on an FG (fixture group) no
matter where should have consistency .. or a fixture .. or a stage
feature".*

This page does three things. It lists every right-click menu by the kind of
object you click and the place you click it (§1, as it was on 2026-11-02).
It names what disagrees. It sets the canon that every menu follows (§3,
revised 2026-11-03: the place's own items on top, then the kind's menu). The canon lives in code in
`ui/src/contextmenus.{h,cpp}`, and `ui/test/contextmenus` checks each
migrated menu against it, so new drift fails the gate.

Notation in the tables:

| Mark | Meaning |
|---|---|
| `---` | a separator |
| `▸` | a submenu |
| `(if …)` | the item is conditional |
| `[x]` | the item is checkable |

## 1. Inventory (before)

### 1.1 Fixture group

| Place | Items, in order |
|---|---|
| **Fixture Manager** tree, group row (`fixturemanager.cpp` `slotContextMenuRequested`) | Fixture Properties… · Edit Fixture Definition… (if one shared) · Plot Symbol ▸ · Output Rules… · `---` · Test Fixture… · Locate · Reset · `---` · Select All Similar · Group ▸ [Create Group(s) from Selection… · Add to Group ▸ · `---` · Move to Folder ▸ · Remove Group (Keep Fixtures) · Rebuild Composite from Members] · Channel Behaviour && Modifiers… · `---` · Delete… · `---` · Expand All · Collapse All |
| **Build ▸ Groups & fixtures** (`fixturegroupsource.cpp`) | Select all similar · `---` · Create fixture group from selection… · Output Rules… · Rename group… · Move to folder ▸ · `---` · Delete group |
| **Build ▸ Scenes** outline, group row inside a scene (`programmingoutline.cpp`) | Why is this on? · `---` · Remove from the scene · Edit the group's layout · Focus this look… (Stack mode) · `---` · Expand all · Collapse all |
| **Plot Layers** group row (`monitorlayerspanel.cpp`) | Select All Similar (N) · Select All with the Same Fixtures (N) · `---` · Remove "X" from "Truss" · Create Fixture Group from N Groups… · Lay Out from the Stage… · Split "X" by Head Type… · `---` · Delete Fixture Group "X"… · `---` · Move to Layer ▸ · New Layer from Selection… |
| **Show Tree ▸ Library** group (`showtreeview.cpp` `slotLibraryContextMenu`) | Pin/Unpin · `---` · Select all similar · Make set… · Move to folder ▸ · `---` · Output Rules… |

### 1.2 Fixture

| Place | Items, in order |
|---|---|
| **Fixture Manager** tree | Fixture Properties… · Definition && Mode… · Change Definition… · Edit Fixture Definition… · Plot Symbol ▸ · LED Layout… · Output Rules… · `---` · Patch… · Return to Inventory… · `---` · Test Fixture… · Locate · Reset · Why is this on? · `---` · Select All Similar · Group ▸ · Power ▸ · Channel Behaviour && Modifiers… · `---` · Delete… · `---` · Expand All · Collapse All |
| **Plot** map and **Plot Layers** list (`monitorgraphicsview.cpp` `buildItemMenu`, the same one for both) | Fixture Properties… · LED Layout… · Edit Truss…/Pipe…/Tower… (its mount) · Locate · Why is this on? · Patch… · Split by Head Type… · Select All Similar (N) · Select Group “name” · Gobo Wheels… · [x] Show Light Footprint · `---` · Mount on ▸ · … · `---` · Show in Plot Layers / Show on Map · Rename… · `---` · Remove from Folder · New Folder from Selection… · Remove from Fixture Group · Create Fixture Group from Selection… · Move to Layer ▸ · New Layer from Selection… · `---` · Remove from View |
| **Riser face editor** (`monitorfixtureitem.cpp` `contextMenuEvent`) | Locate · Why is this on? · `---` · Facing (pan-zero) ▸ · Remove from Truss "X" · Set Mounting Height… |
| **Structure Studio** fixture (`monitor.cpp`) | Edit fixture… · Remove from Truss |
| **Build ▸ Groups & fixtures** | Select all similar · `---` · Create fixture group from fixture… · Output Rules… |
| **Build ▸ Scenes** outline, fixture row | Why is this on? · `---` · Remove from the scene · Focus this look… |
| **Show Tree ▸ Library** fixture | Pin/Unpin · Make set… · Output Rules… |
| **Simple Desk / console** channel | Why is this on? · `---` · capabilities |

### 1.3 Stage features (structures, scenery, marks)

Truss, platform, boom, stand, tower, hanger, scenery, target, image, power
source and network node already share one builder, which serves both the map
and Plot Layers (`MonitorGraphicsView::buildItemMenu`):

Edit X… · Patch Others Like This (N)… · Duplicate Setup to… · `---` · Add ▸ ·
`---` · [x] Show Light Footprints · `---` · Lock/Unlock Position · Lock Layer
"L" · `---` · Show on Map / Show in Plot Layers · Rename… · Fixture Layout… ·
Ungroup · `---` · *organise* · `---` · Delete X

What disagrees with that builder:

| Menu | Items | Problem |
|---|---|---|
| Stage piece (`buildStagePieceMenu`) | X Properties… · `---` · Show on Map · [x] Shown · [x] Locked · [x] Auto position | "Properties…" here, but "Edit X…" for every other feature |
| No-light zone on the map | Edit No-light Zone… · Remove No-light Zone | "Remove" means delete here, while every other feature says "Delete X" |
| Truss/Pipe/Platform/Stand/Tower/Target/Network/Power item `contextMenuEvent`s | "Add Fixture to Truss here…", "Lock Truss", … | Unreachable, because the view never forwards right-clicks to its items. These are dead copies with a third vocabulary. |
| Empty plot (`Monitor::slotCanvasContextMenu`) | Add Here ▸ · `---` · Group selected (Ctrl+G) · Ungroup (Ctrl+Shift+G) · Rotate…  ([ / ]) · `---` · Browse Studio Components… · `---` · Align fixtures ▸ · Distribute fixtures ▸ · `---` · Delete selected · Duplicate selected · Copy selected features · Paste features here | Shortcuts are typed into the labels, sentence case, and Delete is not last |

### 1.4 Palettes, effects and functions

| Kind | Place | Items |
|---|---|---|
| Palette | Build ▸ Palettes | New Color … New Formation · `---` · New folder… · [x] Group by type · Rename folder · Delete folder · Duplicate · Move to folder… · `---` · Save as Bundle… · `---` · Delete |
| Effect palette | Build ▸ Effects | Design / edit… · Runs ▸ · Duplicate · Rename · Move to folder… · `---` · Delete · `---` · New effect… · New EFX · New RGB Matrix |
| Palette | Show Tree ▸ Library | Pin · `---` · Edit · Rename · Duplicate · Move to folder ▸ · Used by ▸ · `---` · Delete… |
| Scene/chaser/… | Build ▸ Scenes | Rename · Duplicate · Delete… · Learn trigger… · Chaser from selection · Send to Show Tree… · `---` · New scene · New function ▸ · Function Wizard… · Startup function… · `---` · Expand all · Collapse all |
| Function | Functions tab | &Clone · Select &all · `---` · &Delete · `---` · New &scene … · New fo&lder · `---` · New p&alette… · Function &Wizard · &Palettes… · `---` · [x] Show the Show Tree's own functions |
| EFX / RGB Matrix | Build ▸ Effects | Open in editor · Learn trigger… · Runs ▸ · Duplicate · Rename · Move to folder… · `---` · Delete · `---` · New effect… · New EFX · New RGB Matrix |

### 1.5 Show Tree

The menus for production, cue list, look folder, cue, sub-cue, hit, cue
layer, tracked layer, the tree header, the attribute ⋯ menus and the cue
stack layer row are all built in `ShowTreeView::slotTreeContextMenu` and
`TreeCueStack::showMenu`. Each of them puts creation first ("+ Cue list…",
"+ Hit", "+ Cue", "+ Sub-cue") and Delete last. They mix sentence case with
"Open in Workbench". A layer is "Delete" in the tree but "Remove the layer"
in the cue stack. The cue menu says "Mode ▸", while the stack says "Cue mode ▸".

### 1.6 Virtual Console, Connections, power, timeline

**Virtual Console.** Right-clicking a widget opens the menu bar's own Edit
menu: Cut · Copy · Paste · Delete · `---` · Widget Properties · Rename Widget
· Swap Widget · `---` · &Background ▸ · &Foreground ▸ · F&ont ▸ · F&rame ▸ ·
Stacking &order ▸ · `---` · the widget's own ▸. Delete is fourth. The
properties item has no "…" even though it opens a dialog. "Background Color"
and "Font Colour" are spelled differently in the same menu.

**Connections.** The universe row menu is Edit connection… · `---` · Rename
universe… · Unpatch from this interface · `---` · Delete universe entirely…,
and then "Collapse/Expand everything below · Add Universe" follow, so Delete is not last.

**Inputs/Outputs.** The universe list says "Add U&niverse" and "&Delete
Universe".

**Power widget.** Removing is "Remove", "Remove %1 items" and "Unassign".

**Show timeline.** "Change name" for a track, "Rename marker…" for a
marker. "Delete cue" comes first in a sequence item's menu.

## 2. What disagrees, in short

1. **The same object has a different menu in every place.** A fixture
   group has five menus with five orders. One place has Output Rules, two
   have Rename, two have Delete, and only one opens its head layout.
2. **Verbs.** Delete, Remove, Unassign and Forget all mean destroy
   somewhere. "Remove" means both "destroy" (a no-light zone, the cue stack's
   "Remove the layer") and "take out of a container" (from the scene, from a
   truss). Rename appears as "Rename", "Rename…", "Rename group…",
   "Rename palette", "Change name", "Name this target…" and "Rename Widget".
   Edit appears as "Fixture Properties…", "Edit fixture…", "Edit the group's
   layout", "Design / edit…", "Open in editor", "Open", "Edit" and "Widget
   Properties".
3. **Case and ellipsis.** Fixture Manager and Plot use Title Case. Build,
   Show Tree and the Library use sentence case. "…" appears on inline
   renames ("Rename group…") and is missing from dialogs ("Widget
   Properties", "Function &Wizard", "Delete group", which confirms).
4. **Order.** The open/edit item is missing or mid-menu in most Build and
   Library menus. Destructive items come mid-menu in Build ▸ Scenes ("Delete…"
   then "Learn trigger…"), Effects ("Delete" then "New effect…"), the
   Functions tab, the Virtual Console and Connections.
5. **Duplicated meaning.** The Plot's two "Group" ideas: map folders ("Group
   selected (Ctrl+G)", "Ungroup") are not fixture groups. The map's "Layer"
   (a drawing layer) and the Workbench's "layer" (a part of a look) share a
   word. The Show Tree's "Make set…" and Build's "Create fixture group from
   selection…" both make a grouping from a selection.
6. **Dead copies.** Nine graphics-item `contextMenuEvent`s on the plot can
   never open. They carry a third set of names.

## 3. The canon

### 3.1 Sections

**Decided 2026-11-03 (user): place-specific items on top, then the kind
menu.** The ask, with the Show Tree's look row (Set to ▸ / None (off here) /
Clear (inherit), and nothing else) beside Build's palette menu: *"a line on
rig view or in tree .. should still have the same paradigm where the right
click can edit all the things that I can in the build .. consistency will be
the right thing. … context is a top .. line .. and then consistent options
below that are consistently ordered for any menu we have."* Why: the first
version (2026-11-02) filed a place's own items in a *Place* section in the
middle (5 of 8), so the object's own items were split around them, and the
same palette had a different-looking menu in every place. Now a menu is two
blocks: what only this place offers, at the top where the eye starts; one
separator; then the object's own menu, built by one builder per kind and so
identical wherever the object is right-clicked.

Every context menu is built from sections in this fixed order. A section
with nothing in it is left out. A separator stands between two sections
that are both present, and nowhere else at the top level.

| # | Section | What goes in it | Examples |
|---|---|---|---|
| 1 | **Place** | What only this place offers: the context. First. | Set to ▸ · None (off here) · Clear (inherit) · Remove from Truss · Remove from Scene · Pin · Make Set… · Solo This Layer · + Cue · Show on Map |
| 2 | **View** | The list itself, not the object | Expand All · Collapse All · [x] Group by Type |
| 3 | **Open** | The kind block starts: its own editor, the default action (a double-click does the same) | Fixture Properties… · Edit Head Layout… · Edit Palette · Open in Workbench · Edit Truss… |
| 4 | **Kind** | What this kind of thing does | Locate · Why is this on? · Patch… · Output Rules… · Used By ▸ · Runs ▸ · Learn Trigger… · Add ▸ |
| 5 | **Selection** | Picking out more like it | Selection ▸ · Select All Similar |
| 6 | **Organise** | Making something from it, and filing it | Create Fixture Group from Selection… · Move to Folder ▸ · Save as Bundle… · Make Reusable… |
| 7 | **Edit** | Clipboard and name | Cut · Copy · Paste · Duplicate · Rename |
| 8 | **Remove** | Deleting it, always last | Delete Fixture Group… · Delete Palette… · Delete Layer |

Sections 1–2 are the context; 3–8 the **kind block**. `ContextMenu::kindBlock()`
returns the kind block's item names and `ContextMenu::contextItems()` the
context's texts; the canon test compares kind blocks across places. A place
whose own items are built straight into the `QMenu` (the look row, the plot)
calls `ContextMenu::fileAsPlace()`: its items are filed in Place and its
separators dropped, and the kind block goes under them.

"Remove from …" (out of a container: a scene, a truss, a layer) is the
place's, so it is on top now; only Delete (destroying the thing) is in
section 8.

### 3.2 Words

- **Title Case.** Capitalise every word except *a, an, the, and, or, of,
  to, in, on, at, by, for, from, with, as* when they are not the first word.
- **"…" only when a dialog follows**, including a confirmation. An inline
  rename is "Rename" with no ellipsis. A delete that asks first is
  "Delete Fixture Group…".
- **Delete destroys. Remove takes out of a container.** "Remove from Scene",
  "Remove from Truss" and "Remove from Fixture Group" leave the thing in the
  show. "Delete Fixture Group…" deletes it. Plain "Remove" is never used for
  a destroy.
- **One name per action, everywhere.** For fixture groups:
  - "Edit Head Layout…"
  - "Output Rules…"
  - "Select All Similar"
  - "Move to Folder ▸"
  - "Rename"
  - "Delete Fixture Group…" / "Delete %n Fixture Groups…"

  For fixtures:
  - "Fixture Properties…"
  - "Locate"
  - "Why is this on?" (the panel's name, a question kept as asked: the one
    exception to Title Case)
  - "Patch…"
  - "Output Rules…"
  - "Select All Similar"
  - "Create Fixture Group from Selection…"

  For any stage feature:
  - "Edit X…"
  - "Show on Map" / "Show in Plot Layers"
  - "Rename"
  - "Delete X"
- **No shortcuts typed into labels.** A shortcut is set on the action, so Qt
  shows it in the menu's own column.
- **Counts** use plural forms: "Delete %n Fixture Groups…", not
  "Delete %1 groups".

### 3.3 Per kind

One builder per kind fills the kind block. A place passes hooks for what it
does its own way (rename in place, edit in its own editor); an empty hook
means the builder does it itself (a dialog), so the item is there in every
place. Object names are the same everywhere (`ctx…`); the canon test checks
them.

**Fixture group** — `ContextMenus::rig()` / `fixtureGroup()`.
- Open: Edit Head Layout…
- Kind: Locate, Why is this on? (its first light), Output Rules…, Split by
  Head Type… (if mixed), Lay Out from the Stage… (if on the plot)
- Selection: Selection ▸, Select All Similar
- Organise: Create Fixture Group from Selection… (several), Move to Folder ▸
- Edit: Rename (in place) / Rename…
- Remove: Delete Fixture Group…
- Places on top: Fixture Manager (Plot Symbol ▸, Test Fixture…, Group ▸,
  Power ▸, …), Build scene (Remove from Scene, Focus This Look…), Plot
  Layers (Remove from “Truss”, the plot's folders and layers), Library
  (Pin, Make Set…).

**Fixture** — `ContextMenus::rig()` / `fixture()`; a mixed selection of
groups and fixtures gets one menu for both.
- Open: Fixture Properties…
- Kind: Locate, Why is this on?, Patch… / Patch %n Fixtures…, LED Layout…
  (several heads), Gobo Wheels… (its own glass), Output Rules…, Split by
  Head Type… (mixed)
- Selection: Selection ▸ (Create Fixture Group from Selection… inside),
  Select All Similar; on the plot and Plot Layers, Select Group “name”
  (its plot folder)
- Edit: Rename / Rename… (one fixture)
- Places on top: Fixture Manager (definition, inventory, test, Group ▸,
  Power ▸; Delete… last), the plot and Plot Layers (Mount on ▸, the mount's
  editor, Unmount, Facing ▸, height, studio group, footprint, folders,
  layers, Remove from Fixture Group, Remove from View), Structure Studio
  (Remove from Truss, Add Bar/Hanger Here), the riser-face editor (Facing
  ▸, Remove from Truss, Set Mounting Height…), the Rig Overview (nothing:
  read-only, so only what looks into it), Build scene, Library.

**Palette** (any type; an effect palette too) — `ContextMenus::palette()`.
- Open: Edit Palette / Edit Effect (in the Workbench: no "…")
- Kind: Runs ▸ (effects), Used By ▸ (the cues, hits, scenes and effect
  slots using it, each a jump; VC buttons listed)
- Organise: Make Reusable… (a cue's one-off value), Move to Folder ▸,
  Save as Bundle…
- Edit: Duplicate, Rename / Rename…
- Remove: Delete Palette… / Delete %n Palettes…
- Places: Build ▸ Palettes, Build ▸ Effects, the Library (Pin on top), a
  cue's look row in the Show Tree and in the docked Workbench (Set to ▸ /
  Set in This Cue ▸, Update Cue ▸, Design Effect…, Level…, Colour…, Its Own
  Colour / Dimmer ▸, Fade In / Out ▸, None (off here), Clear (inherit) on
  top), a Build scene's look row (Show in Palettes, Move Up / Down, Remove
  from Scene on top), the Look Editor's palette slots (Clear on top).

**Function** (scene, chaser, EFX, collection, RGB Matrix, …) —
`ContextMenus::function()`.
- Open: Open in Workbench
- Kind: Learn Trigger…, Runs ▸ (EFX, RGB Matrix), Send to Show Tree…
  (chaser), Used By ▸ (the chasers and collections it is in; VC buttons)
- Organise: Move to Folder ▸
- Edit: Duplicate, Rename / Rename…
- Remove: Delete… / Delete %n Functions…
- Places: Build ▸ Scenes (Chaser from Selection on top), Build ▸ Effects,
  the Library (Adopt as a Hit, Pin on top), the Workbench's layer rows
  (Make Own Copy, Move Layer Up / Down, Remove Layer on top).

**Show Tree layer** — `ShowTreeView::fillLayerMenu()`, the tree's layer row
and the Workbench's (docked) layer row.
- Open: Open “function”… (a function played as it is: W0, first), Open in
  Workbench
- Kind: Capture into Palettes…, Release (Leaving This Cue), Why is this
  on?, Design Effect…, Focus This Cue…, Split Layer (One per Group), Add
  Child Layer ▸, Remove from Layer ▸, Detach to Plain Scene, Light on the
  Cyc: Not Meant
- Organise: Make Reusable… / Unlink Here (a Copy of Its Own)
- Edit: Cut, Copy, Paste After This Layer, Duplicate, Rename…
- Remove: Delete Layer
- Places on top: the tree (+ Cue List…, + Hit for This Cue, + Cue, +
  Sub-cue, Merge Layers, N Selected Layers ▸), the Workbench (Solo This
  Layer, Cue Mode ▸, Show in Tree). A layer carried into the cue has only
  Open, Release and Why is this on?, and two more Places on top, in the tree
  and the Workbench alike: Bring Into This Cue and Start Here Instead (the
  second names the cues that lose the layer in its tooltip and a confirm;
  both are greyed with the reason when they could not keep the show the
  same). Two items more in the Show Tree menus, by the ask of 2026-11-03.

**Cue / sub-cue / hit** — the tree's cue rows.
- Open: Open in Workbench
- Kind: Learn Trigger…, Skip Cue, Don't Move in Black into This Cue, Mode
  ▸, Colour ▸, Record as New Cue, Copy to a Hit, Renumber This Level…,
  Ripple Timecode…; a hit: Colour ▸, Make VC Page
- Organise: Make Sub-cue of … / Make Inline Cue, Make Cue Reusable… /
  Unlink Here, Move to Folder ▸ (a hit), Move Up, Move Down
- Edit: Copy, Paste After This Cue / Paste into This Cue, Duplicate
- Remove: Delete Cue / Delete Sub-cue / Delete Hit

**Cue list** — Learn Trigger…, Convert to Tracking…, Colour ▸, Move in
Black ▸, Make Main List, Play on GO, Detach to Plain Chaser…, Restore
Original… | Rename… | Delete Cue List…. **Production** — Make Current
Production | Rename… | Delete Production…. **Hits / hit folder** — Make VC
Page | Rename Folder | Delete Folder (New Folder… on top).

**Stage feature** (truss, platform, boom, stand, tower, hanger, scenery,
target, image, power source, network node) — the plot's one builder
(`MonitorGraphicsView::buildItemMenu`), the same for the map and Plot
Layers.
- Open: Edit X…
- Kind: Patch Others Like This…, Duplicate Setup to…, Add ▸, Used By ▸ (a
  target: the Aim palettes pointing at it, each opened to edit), Save as
  Gear Model… (network node), Fixture Layout…, Ungroup
- Selection: Selection ▸ (fixtures among the selection), Select All Similar
  (N) (the same kind: a target's kind, people of a set, a truss's type…),
  Select Group “name” (its plot folder; a click on the plot takes only the
  item)
- Edit: Rename (in place in the list) / Rename… (the map)
- Remove: Delete X / Delete Target…
- On top: Show Light Footprints, Lock Position, Lock Layer, Show on Map /
  Show in Plot Layers, the plot's folders and layers.
- A stage piece: Edit X… under Show on Map, Shown, Locked, Auto. A no-light
  zone: Edit No-light Zone… | Delete No-light Zone. A Structure Studio
  hanger: Edit Hanger… | Delete Hanger….

### 3.4 What is not migrated yet

These follow the words rule only where they share a text with a migrated
menu, and are listed for a follow-up.

**Virtual Console.** Its right-click menu *is* the menu bar's Edit menu
(Cut, Copy, Paste, Delete, Widget Properties, …, the widget's own ▸), so
the same `QAction`s serve both. Putting the widget's own items on top and
Delete last would reorder the menu bar's Edit menu too, which is QLC+'s
documented layout; it needs a menu of its own built for the right-click.
Not trivially compatible, so left (TODO.md).

**Line-param rows** (Spread / Grouping / Cross ⋯ on a line aim) and the
Look Editor's palette pick menus (a left-click on a slot): they pick a
value, not an object, so there is no kind block to add.

**Functions tab** (QLC+'s Function Manager, hidden by default since Q32):
on the canon's sections and words, not on the function builder.

**Connections, power widget and show timeline.** No overlap with the kinds
above.

**Counts.** Distinct menu texts in `ui/src` went from 505 to 496, and the
places in the code that add a menu item from 599 to 581 (`tr("…")` texts on
lines that add a menu item, before and after 2026-11-03): the shared
builders replaced per-place copies (palette menus in Build, Effects and the
Library; function menus in Build ▸ Scenes, Effects and the Workbench; the
two layer menus; the fixture menus of the plot, Structure Studio, the Rig
Overview and the riser-face editor). Several places' menus are longer
(a look row now carries its palette's menu): that is the ask.

### 3.6 Every menu, not only the builders' (2026-11-04)

The builders hold the kind block; the words hold everywhere. A second
audit (wfaudit2) found about 170 menu items outside the builders still in
sentence case: the Connections tree, the plot's Show, Lock and Origin
menus, the Focus and Venue menus, the Workbench's + Scene and + Effect,
Build's + New, the show timeline and the power widget. They are Title Case
now, and `ui/test/menuwords` reads every label given to `addAction()`,
`addMenu()` and `ContextMenu::add()` in `ui/src` (both sides of a ternary)
and fails on one that is not Title Case, has "..." for "…", or has a
shortcut typed into it. A disabled line of information in a menu ("(no
folder)", a note) is not an item and is not checked; nor is a question
("Why is this on?") or a prompt ending in ":".

The same test fails on a text that shows a length in metres only (a "%1 m"
with no feet text beside it): a show in feet reads feet.

Connections now follows the order: Collapse / Expand All Below and Add
Universe on top (the list's own), the row's items, its Delete last. Delete
destroys ("Delete Universe…", "Delete Target…", "Delete Port"); the pending
patch's "Forget this patch (unpatch)…" is **Unpatch…**.

The Workbench's **+ Effect** and Build ▸ Effects' **+ New** now read the
same: **Effect Palette…** first, then a "QLC+ effects" heading over EFX and
RGB Matrix (the Workbench said "Effect look…", last).

### 3.5 Reserved slots for work landing alongside

(Section names, not the old numbers: the order changed on 2026-11-03.)

- *Selection ▸*, shared by fixture and head menus (`ui/src/selectionmenu.*`),
  goes in Selection.
- The Plot's "New Map Layer from Selection…" is the plot's own (on top).
  That separates the drawing layer from the Workbench layer (2, point 5).
- The Show Tree's "Make Inline Cue" / "Make Sub-cue of …" on cue and sub-cue
  rows are in Organise.
- Show Review's "I meant it" is a review-row action, not an object menu.
  The cyc-backwash layer item is in Kind of a cue layer.
