# Context menus: inventory and canon

*2026-11-02. Ask: "go thru every submenu and right click and make sure
we're consistently laid out … right click on an FG (fixture group) no
matter where should have consistency .. or a fixture .. or a stage
feature".*

This page does three things. It lists every right-click menu by the kind of
object you click and the place you click it. It names what disagrees. It
sets the canon that every menu follows. The canon lives in code in
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
| **Plot** map and **Plot Layers** list (`monitorgraphicsview.cpp` `buildItemMenu`, the same one for both) | Fixture Properties… · LED Layout… · Edit Truss…/Pipe…/Tower… (its mount) · Locate · Why is this on? · Patch… · Split by Head Type… · Select All Similar (N) · Gobo Wheels… · [x] Show Light Footprint · `---` · Mount on ▸ · … · `---` · Show in Plot Layers / Show on Map · Rename… · `---` · Remove from Folder · New Folder from Selection… · Remove from Fixture Group · Create Fixture Group from Selection… · Move to Layer ▸ · New Layer from Selection… · `---` · Remove from View |
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

Every context menu is built from sections in this fixed order. A section
with nothing in it is left out. A separator stands between two sections
that are both present, and nowhere else at the top level.

| # | Section | What goes in it | Examples |
|---|---|---|---|
| 1 | **Open** | The thing's own editor. It is first and, where the place has one, the default action. | Fixture Properties… · Edit Head Layout… · Edit Truss… · Edit Palette… · Open in Workbench |
| 2 | **Kind** | What this kind of thing does | Locate · Why Is This On? · Patch… · Output Rules… · Split by Head Type… · Add ▸ |
| 3 | **Selection** | Picking out more like it | Select All Similar · (the shared *Selection ▸* submenu, see 3.5) |
| 4 | **Organise** | Making something from it, and filing it | Create Fixture Group from Selection… · Move to Folder ▸ · Move to Layer ▸ · Make Set… · Pin |
| 5 | **Place** | What only this place offers, in a fixed slot | Show on Map · Remove from Truss · Lock Position · Focus This Look… |
| 6 | **View** | The list itself, not the object | Expand All · Collapse All · [x] Group by Type |
| 7 | **Edit** | Clipboard and name | Cut · Copy · Paste · Duplicate · Rename |
| 8 | **Remove** | Taking it out, then destroying it, always last | Remove from Scene · Delete Fixture Group… |

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
  - "Why Is This On?"
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

**Fixture group.** Every place calls `ContextMenus::fixtureGroup()`.
- **Open:** Edit Head Layout…
- **Kind:**
  - Locate
  - Output Rules…
  - Split by Head Type… (if mixed)
  - Lay Out from the Stage… (if on the plot)
- **Selection:** Select All Similar
- **Organise:**
  - Create Fixture Group from Selection… (if several are selected)
  - Move to Folder ▸
- **Place** items, by place:
  - Fixture Manager: Plot Symbol ▸, Test Fixture…, Group ▸ extras
  - Build outline: Remove from Scene, Focus This Look…
  - Plot Layers: Remove from Truss, Move to Layer ▸
  - Library: Pin, Make Set…
- **Edit:** Rename
- **Remove:** Delete Fixture Group…

What each place supplies:
- Edit Head Layout opens wherever the place edits a layout: the Fixture
  Manager pane, the Workbench, or a dialog everywhere else.
- Rename is inline where the list can edit the row in place, and a dialog
  otherwise.

**Fixture.** Every place calls `ContextMenus::fixture()`.
- **Open:** Fixture Properties…
- **Kind:**
  - Locate
  - Why Is This On? (one fixture)
  - Patch… / Patch %n Fixtures…
  - Output Rules…
- **Selection:** Select All Similar, or the *Selection ▸* submenu from
  `selectionmenu.*` once it lands (3.5)
- **Organise:** Create Fixture Group from Selection…
- **Place** items, by place:
  - Fixture Manager: definition, power, inventory
  - Plot: Mount on ▸, footprints, layers
  - Build outline: Remove from Scene, Focus This Look…
- **Remove:** Remove from Scene / Remove from Truss / Delete Fixture…

**Stage feature** (truss, platform, boom, stand, tower, hanger, scenery,
target, image, power source, network node, stage piece, no-light zone).
These keep the one Plot builder (`buildItemMenu`), with three changes:
- Stage pieces say "Edit X…" like everything else, not "X Properties…".
- A no-light zone's destroy item is "Delete No-light Zone".
- The dead item `contextMenuEvent`s are gone.

**Palette.** Every place calls `ContextMenus::palette()`.
- **Open:** Edit Palette…
- **Kind:** Runs ▸ (effects)
- **Organise:** Move to Folder…, Save as Bundle…
- **Place:** Used by ▸ in the Library
- **Edit:** Duplicate, Rename
- **Remove:** Delete Palette… / Delete %n Palettes…
- "New …" items belong to empty space and folders. They are not in an
  object's menu.

**Function** (scene, chaser, EFX, collection, RGB matrix).
- **Open:** Open in Workbench / Open in Editor
- **Kind:** Learn Trigger…, Runs ▸, Chaser from Selection, Send to Show Tree…
- **Edit:** Duplicate, Rename
- **Remove:** Delete…
- "New …" items go to empty space and folders.

### 3.4 What is not migrated yet

These menus follow the words rule only where they share a text with a
migrated menu, and are listed for a follow-up.

**Show Tree cue/list/layer menus** (`slotTreeContextMenu`). Another change
(Make Inline Cue / Make Sub-cue) was landing in the same function. Their
slots in the canon:
- Open: Open in Workbench
- Kind: Release, Skip, Mode ▸
- Organise: Make Reusable…, Make Inline Cue, Make Sub-cue of…
- Edit: Copy / Cut / Paste / Duplicate / Rename
- Remove: Delete

**Virtual Console.** Its right-click menu *is* the menu bar's Edit menu, so
reordering it moves the menu bar too.

**Connections, power widget and show timeline.** No overlap with the three
kinds asked about.

### 3.5 Reserved slots for work landing alongside

- *Selection ▸*, shared by fixture and head menus (`ui/src/selectionmenu.*`),
  goes in section 3.
- The Plot's "New Layer from Selection…" becomes "New Map Layer from
  Selection…" in section 4. That separates the drawing layer from the
  Workbench layer (2, point 5).
- The Show Tree's "Make Inline Cue" / "Make Sub-cue of …" on cue and sub-cue
  rows go in section 4 (Organise).
- Show Review's "I meant it" is a review-row action, not an object menu.
  The cyc-backwash layer item goes in section 2 of a cue layer.
