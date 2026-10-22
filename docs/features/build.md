# Build Tab

*"Scenes hold looks. Looks are made of palettes."* — the app's own guide text.

**Build** (it was the *Programming* tab) is where the show's vocabulary is
made: palettes, effects, scenes and collections. The [Show Tree](show-tree.md)
puts them in order and runs them. Build works on ordinary QLC+ functions —
a scene stays a `Scene`, a collection a `Collection` — so a show built the
old way opens, plays and saves exactly as QLC+ made it: nothing is converted
when it loads, and nothing asks you to convert it.

## Why

Stock QLC+ builds a Scene by setting raw per-channel DMX values, fixture by
fixture. Want "all the blue washes a little dimmer"? Re-touch every fixture's
channels by hand. Change your mind about the exact shade of blue? Re-touch
every scene that used it.

Build adds an indirection layer. A Scene stops being a flat bag of baked DMX
values and becomes a declaration of:

- **Targets** — which fixtures or groups this scene affects
- **Looks** — which reusable **palettes** (Color, Dimmer, Pan/Tilt, Aim, Beam,
  Gobo, Shutter, Strobe, Effect) apply to them

Edit a palette once — say, nudge "Deep Blue" a little warmer — and every
scene that uses it updates. Raw channel values still work, side by side with
looks, and a collection of scenes, EFX and RGB Matrices still plays the way
QLC+ plays it.

## Layout

![The Build tab: the navigator on the left, a scene in the Workbench, a look in the editor pane](../images/build-tab-layout.png)

1. **Palettes | Effects | Scenes** — the section switch. Only the navigator
   under it changes.
2. **The navigator** — here Build ▸ Scenes: your scene folders, with their
   scenes, collections and chasers ([The outline](#the-outline)).
3. **The Workbench's header** — ↩ back, where it is, its name and kind,
   where it is used, what fires it, what is on the rig.
4. **Its layers** — what is loaded, one row per layer
   ([The Workbench](#the-workbench)).
5. **+ Scene, + EFX, + RGB Matrix**, ▲ ▼, **Remove**, **▣ Open**.
6. **The editor pane** — what the selected row edits; here, the Look Editor.
7. **Groups &amp; fixtures** — under every section, to drag onto a layer.

Two panes, left to right:

- **Left — the navigator, by section.**
  - **Palettes**: the palette list (filterable, foldered; right-click to add
    one, double-click to edit one) and a **Bundles** tab.
  - **Effects**: the effect palettes, and the classic effects — every EFX and
    RGB Matrix in the show. Double-click an effect palette to edit it; click
    an EFX or RGB Matrix to load it in the Workbench; drag either onto a
    layer or a collection.
  - **Scenes**: [the outline](#the-outline) of your scene folders. Click a
    scene, a collection or a chaser and it loads in the Workbench.

  Under every section, **Groups &amp; fixtures**, with a **Search fixtures &
  groups…** box: a row stays when it or anything under it matches, the
  folders on the way down open, and a matching group keeps all its
  fixtures. The section you pick is the one the tab opens on next time.
- **Right — [the Workbench](#the-workbench)**: what is loaded, its layers,
  and the editor pane for the selected row. It is the same in every
  section: switching to Palettes to drag a colour onto a layer leaves the
  Workbench as it was.
- **The tab's tools**, in the top bar beside the show controls (or over
  the tab, with the show controls in the status bar, or the tab in a
  window of its own): **Highlight**, **Flash**, **Tap** and
  **Save Positions** as buttons; **Park** (park selected, unpark all),
  **Move in black** (mark, unmark, auto move-in-black and its lead time),
  **Beat** (internal beat on, BPM, speed masters — the button reads the
  tempo) and **More** (snapshot live output, jog speed) as drop-downs.
  In a narrow window, what doesn't fit is under **»**.

**Resizing:** drag the handles — between the navigator and the Workbench,
between the layers and the editor pane, and over Groups &amp; fixtures. A
handle moves its edge and stops where a pane reaches its smallest — no pane
snaps shut. **Double-click a handle** to put it back where it started. The
badge columns are handles of their own (drag the line between the columns).
Where you leave them is remembered.

## The Workbench

One editor for "a stack of things that play together". It loads a scene, a
collection, a chaser or any other function and shows it the same way every
time: layers you can open, add, remove and reorder, an editor for the
selected row, and a live preview.

| Loaded | Its layers | The editor pane, nothing selected |
|---|---|---|
| a **scene** | one layer, opened up into its targets and looks | the scene's page (its layer is selected) |
| a **collection** | one layer per member, in order: a scene opens up into its targets and looks; anything else — a raw-value scene shows its fixtures and **▣**, an EFX, an RGB Matrix, a chaser, a nested collection — is a **▣** row that plays as it is | what it holds, where it's used, what fires it |
| a **chaser** | one row per step ("2 · 0.0.3-Announcer"), each opened with ▣ Open | its step table, as the Chaser editor has it |
| an **EFX, RGB Matrix, show, script…** | one row | its stock editor |

**The header** says where it is (its folder), its name and kind, what it
holds, **Used in** (the collections, chaser steps and shows that play it),
**Fired by** (Virtual Console buttons) and what is **on the rig**, with
**Stop** / **Play**. Something the Show Tree made for its cues is marked so,
with **Show in Show Tree**; it is changed there, not here.

**Click a row** and the editor pane edits it, at full height:

| Selected | The editor pane |
|---|---|
| a scene layer | its name, folder, what it holds, its warnings in full, where it's used, what fires it, and its **Timing**: fades, [transitions](#transitions), speed master |
| a look | the [Look Editor](#the-inline-look-editor) |
| a group or fixture | its fixtures' channels, as the scene sets them (several fixtures of one type together); a group has **Edit layout** |
| an EFX or RGB Matrix layer | its stock editor (EFX editor, RGB Matrix editor) |
| a chaser layer | its step table |
| a nested collection | **▣ Open** it |

The scene of the row you clicked is the one a control surface addresses.

**▣ Open** (the button, a double-click, or right-click → **Open**) loads
that layer in the Workbench on its own — a nested collection's layers, a
scene by itself, a chaser's steps. **↩** in the header (it names where it
goes) comes back to what was loaded before, with the row you opened
selected again. Loading something from the navigator starts afresh.

**Adding, removing, reordering** — on a collection:

- **+ Scene** adds scenes (or any function) from the show, picked in a list,
  after the selected layer; its menu's **New empty scene** makes one, filed
  with the collection.
- **+ EFX** / **+ RGB Matrix** make a **new** one on the selected layer's
  groups — an EFX on its heads, an RGB Matrix on its first group — add it
  after that layer and open it in the editor pane. With no layer selected,
  it goes at the end, on nothing yet.
- **Drag** a scene, collection or chaser from the navigator onto the layers
  to add it (above the row you drop on; on empty space, at the bottom), or
  drag an EFX or RGB Matrix from Build ▸ Effects. Drag a layer within the
  list to reorder it.
- **▲ ▼** move the selected layer; the lower layer wins where two set the
  same channel (the one added last, as QLC+ plays a collection).
- **Remove** (or **⌫** on a layer row) takes the layer out. The function
  stays in the show, as *Remove from* always did.

A lone scene is one layer: nothing is added to it here. Load a collection to
build a stack.

**Palettes, groups and fixtures** drag onto any scene layer exactly as in the
outline (below): a palette dropped on a layer is a base look, on an effect
its own; a group a dynamic target, a fixture a fixed one. Rows drag between
layers — move, or copy with **⌥**.

**Shared members.** A scene in more than one collection or chaser step
carries a badge, **also in 2**; hover for where (*0.0.4-SHOUT, Full Show
(step 3)*). An edit here changes it everywhere it is used.

**Live preview.** In Design mode what is loaded plays — a collection with
all its layers — and what you edit plays as you edit it. A layer added or
taken out of a playing collection is played at once. **Stop** in the header
holds it; **Play** (or loading something) plays again. Leaving the tab stops
it. In Operate mode what is loaded runs as the show output, as before.

**Undo.** Every Workbench edit — a layer added, removed or reordered, a new
EFX or RGB Matrix (Undo takes the new function away too; Redo brings it
back with its id), and every edit in the outline and the editor pane — is
one step on the **one history** Build and the Show Tree share: **⌘Z** takes
it back, **⇧⌘Z** puts it back, in either tab.

### On a Show Tree cue

The Show Tree opens a cue, a look or a layer in this same Workbench, in
place of its page (double-click, **✎ Workbench**, or `wb`): the cue's own
layers, each opening into one row per attribute, and — grey, read-only —
what tracks in, with where it comes from. Every edit there is a step of the
tree's. While it is there, Build shows a note in its place; **↩ Show Tree**
hands it back, loaded as it was. See [Working on a cue in the
Workbench](show-tree.md#working-on-a-cue-in-the-workbench).

## The outline

Build ▸ Scenes: one tree of your scene folders, so a scene doesn't have to be
loaded to see or change what is in it.

**Rows.** Folders → scenes, collections and chasers. A scene opens into its
targets (◆ a group, a fixture) and looks (a colour shows its swatch). A
colour or dimmer under an effect is that effect's own; one above every
effect is the look's own. Collections and chasers are **full rows** with a
badge saying what they are (*collection · 3*, *chaser · 6*, *VC ×1*); an
EFX, RGB Matrix or show filed in one of these folders shows there too. The
scenes and collections the Show Tree made for its cues stay hidden, as
everywhere.

**Click a row** and the Workbench loads it: a scene, collection or chaser
on its own; a look or target in a scene, its scene with that row selected
(or, when the collection loaded holds that scene, that row in it). A folder
leaves the Workbench as it is.

**Open and close.** **⊞ / ⊟** over the outline (or **⌘→ / ⌘←**) open or
close every row. **⌥-click a triangle** to open or close that row and
everything under it (**⌥→ / ⌥←** on the keyboard). What you leave open is
open again next time, per show. The Workbench's layers remember theirs too,
per collection.

**Badges**, in the right-hand column; hover for the details:

| Badge | Means |
|---|---|
| **⚠ n** | n things to look at: a target with no look for what it can do, heads that can't reach their aim target, unpatched fixtures, or a look the [stack checker](effects.md#how-a-look-stacks) flags. On a group: how many fixtures, marked ⚠ if any of that is about them. |
| **VC ×n** | n Virtual Console buttons fire the scene. |
| **▣** | The scene sets raw channel values directly (a fixed fixture row shows how many). On the Workbench, a layer that plays as it is. |
| **⇄** | It has a [transition](#transitions). |
| **⏱** | Its effects keep time with a speed master. |
| **◷** | A look with a fade of its own. |
| **also in n** | On the Workbench: a layer used in n other collections or chaser steps. |

**Drag and drop.** The line over the outline (and over the Workbench's
layers) says what a drop will do before you let go.

| Drag | Onto | Does |
|---|---|---|
| a look or target row | another scene (or a row in it) | **moves** it there; hold **⌥** to **copy**. A look keeps its own fade, a fixed fixture its raw values, an effect brings its own colour and dimmer. |
| a look row | another look in its own scene | reorders: it goes above that one (lower rows win) |
| a colour or dimmer row | an effect row | makes it that effect's own |
| a palette (Build ▸ Palettes) | a scene, or a row in it | adds it: a base look, or above the look it was dropped on, or the effect's own if dropped on an effect |
| a group or fixture | a scene | a dynamic or fixed target |
| a scene, collection or chaser | a folder; empty space | files it there; takes it out of its folder |
| a scene, collection or chaser | the Workbench's layers | adds it to the collection loaded, as a layer |
| a scene | another scene in the outline | nothing — scenes don't include scenes; drop it on the Workbench to make it a layer of a collection |

**Several at once.** ⇧/⌘-click rows — across scenes too — then drag them,
or press **⌫** to take them all out of their scenes (the palettes, groups
and fixtures themselves stay). ⌫ on scene, collection or chaser rows asks
before deleting them, naming where each is used.

**Rename** with **F2** (or a slow second click): a scene, collection,
chaser, a folder (what is filed in it follows) or a palette.

**Right-click** a row for the same actions: remove, move a look up or down,
show a palette in Build ▸ Palettes, rename, duplicate, delete, a new scene
in that folder, open or close everything under it.

**+ Scene** (over the outline) makes a scene in the folder of the selected
row; its menu also makes a **collection**, a **chaser**, or a scene in a new
folder. Each loads in the Workbench, ready to name (F2) and fill.

## Filing into folders

**Scenes, collections and chasers** file by dragging their rows in the
outline onto a folder row (empty space takes them back out to the top);
**F2** on a folder row renames it, and what is filed in it follows. **+ Scene
→ New scene in a new folder…** makes a folder with a scene in it. Folders
are saved with the show and show the same in the Functions window. Filing
and renaming are steps on the history.

### Taking something out of a collection, chaser or show

A collection's members are its layers in the [Workbench](#the-workbench):
select one and press **⌫** (or **Remove**). The function stays in the
show. A chaser's steps are edited in its step table (the editor pane when a
chaser is loaded); a show's tracks in **Shows**. The Functions window (Window
menu) still has the stock editors.

### Deleting palettes and functions

A scene, collection or chaser: select its row in the outline (several,
⇧/⌘-click) and press **⌫**, or right-click → **Delete…**. The question says
it deletes the function itself, everywhere, and names each collection,
chaser step and show track using it. (On the Workbench, ⌫ only takes a layer
out.)

A palette: in Build ▸ Palettes, right-click → **Delete**, or select and press **⌫**.
The question lists every place the palette is used — each scene and which
look row, each effect look whose slot uses it — and says what happens:
scenes lose that look (their other looks and values stay), effect looks lose
that slot's palette and use their look's colours. With a folder selected, ⌫
deletes the folder: what was in it moves up a level, nothing is deleted.

A palette the Show Tree uses is named by its **cues and looks** ("Cue 1.0.1
Verse in Full Show — Towers"), not by the scenes the tree made for them, and
the **VC buttons** that fire those are listed too.

**Palette folders hold any type.** A folder in Build ▸ Palettes can hold
colours, aims, dimmers and effects together — say a folder per song,
"01-Everybody Scream", with everything that song uses. Right-click empty
space → **New folder…** makes one at the top level. Drag a palette onto a
folder to file it; drag a folder into another. The tab shows folders first,
each palette with its type's icon, and the palettes in no folder below them.
Tick **Group by type** in the right-click menu to see it the other way
round: Aim, Color, Dimmer… first, with the same folders under each type (it
is remembered). In that view a folder is still one folder: renaming or
deleting it under Color does it for every type, and dropping a palette on it
files the palette there whatever its type.

Shows saved before this had a folder per type (Color › 01-Song, Aim ›
01-Song); they open with those merged into one folder per name, and effect
looks that had been filed automatically under Effect › category › engine are
left in no folder. Only what is really that old layout changes: a folder
you made that happens to be named like a type (*Color* holding dimmers) or
*Colour* keeps its name. Saving keeps the new layout; an older version of
qlcconsole opening such a show shows the same folders at the top level. The
Show Tree's Library uses the same folders.

### Palettes here and in the Show Tree

New, Duplicate, Delete and Move to folder of a palette are the same
operations as in the Show Tree's Library ([Palettes in the
Library](show-tree.md#palettes-in-the-library)), and each is one step on the
**one history** the two tabs share: **⌘Z** here (or the Show Tree's) takes
back whatever was done last — an edit in the outline or the Workbench, a
Bundle stamp, a palette made, renamed, filed or deleted in either tab, or a
Show Tree change — and **⇧⌘Z** puts back what was undone last. Deleting a
palette here clears it from everything that uses it — scenes, effect slots
and the Show Tree's cues and looks, as the Library's Delete does — and one
⌘Z brings it back with its id, in every one of them.
The **Edit** menu's Undo / Redo name the step. Effect design, `.qxfx` files
and Bundles stay here.

## Building a look, step by step

1. Open the **Build** tab.
2. If you don't have any palettes yet, switch to **Palettes**, right-click
   the list and pick a type — Color, Dimmer, Pan/Tilt, Aim, Beam, Gobo, Shutter, Strobe,
   or Effect. It opens straight into the Look Editor for you to set its
   value; drag it into a folder afterward to file it.
3. Switch to **Scenes** and click a scene in the outline, or **+ Scene** to
   make one (it goes in the folder of the row selected; F2 names it). It
   loads in the Workbench.
4. **Drag a fixture group onto the scene's layer** to add it as a **dynamic**
   target — its membership is followed live, so adding a fixture to that
   group later pulls it into every scene using it. **Drag an individual
   fixture** instead for a **fixed** target with static membership.
5. **Drag a palette onto the layer** (switch to Palettes for the list: the
   Workbench stays as it is). Dropped on the scene's row, it becomes a base look (painted directly onto the targets). Dropped
   onto an Effect's row, a Color/Dimmer palette instead goes *under* it and
   belongs to that effect only — its colours, its level; the ones above
   every effect are the look's own and feed every effect that has none of
   its own. See [How a look stacks](effects.md#how-a-look-stacks).
6. Click a look's row to edit it in the Look Editor in the editor pane. Every edit
   there applies immediately; there's no separate "apply" step.
7. Reorder looks by dragging a look's row onto another in the same scene —
   the list is a stack read top to bottom, and the lower look wins on what
   two of them set (two Colours are colour 1 and colour 2 instead; see
   [How a look stacks](effects.md#how-a-look-stacks)). Right-click → **Move
   up / Move down** does the same a row at a time. Select rows and press ⌫
   to take them out of the scene.
8. Moving on to the next scene that uses the same group or look? Drag the
   row there (⌥ copies it).
9. Gathering scenes into a collection? **+ Scene ▸ New collection** over the
   outline, then drag scenes onto its layers — or load one you have, and add
   an EFX or RGB Matrix on a layer's groups with **+ EFX** / **+ RGB
   Matrix** ([The Workbench](#the-workbench)).
10. Save the workspace normally (⌘S).

If some of a scene's targets are still in the inventory (not patched), the
scene and the group show **⚠**, and the scene's page says so: they send no
DMX, and the look shows in the Lighting Studio and the Rig Overview only
until you patch them in the Fixture Manager.

A **Bundle** is a related, separate shortcut: a portable, shareable snapshot
of an ordered palette stack (no fixture targets attached). "Stamping" one
from the Bundles tab onto the selected scene replaces its whole look stack
in one action — handy for re-using a "look" across shows without re-dragging
every palette. A stamp is one step on the history (⌘Z).

## Transitions

A scene can arrive and leave as a **transition** rather than everything
fading together: a wipe across the stage, from the centre, a dissolve. The
row is under **Timing** on the scene's page:

**In:** [shape] [time]  **Out:** [shape] [time]  **Edge:** [%]

| Shape | What happens |
|---|---|
| **None (the scene's fade)** | No transition; the scene's own fade times apply. |
| **Fade** | Everything together over the time. |
| **Wipe to stage left / Wipe to stage right** | Across the stage. |
| **Wipe up / Wipe down** | By height. |
| **Wipe front to back / Wipe back to front** | Downstage to upstage, or the reverse. |
| **From the centre / To the centre** | Outward from, or inward to, the middle of the stage. |
| **Dissolve** | Heads in a fixed random order. |
| **Along each bar** | Pixel by pixel along every bar. |

Each head's moment comes from **where it is on stage** (its place in the
rig), so the rig needs to be laid out in Lighting Studio for wipes to mean
anything. **Edge** sets how soft the leading edge is: each head's own fade
as a share of the whole time — 100% and everything fades together, low and
it's a sharp line.

The **whole look** follows the transition, effects included, cell by cell.
Channels that can't fade (gobo, shutter and the like) switch when the
transition reaches them, not at the start.

Things to know:

- An active transition **replaces the scene's fade time**.
- It doesn't apply when the scene's tempo is **Beats**.
- If you change it while the scene is previewing, restart the scene to see
  the new one — the transition in is worked out when the scene starts.

## The inline Look Editor

![The Color look editor: basic-color swatches, a wheel picker, and R/G/B/W sliders](../images/programming-tab-look-editor-color.png)

The editor shown depends on the selected look's palette type:

| Type | What you get |
|---|---|
| **Color** | A color-wheel picker, a grid of basic-color swatches, and six numbered sliders (1 R · 2 G · 3 B · 4 White · 5 Amber · 6 UV — matching a physical-fader numbering convention). Amber/UV only appear if your target fixtures actually have those emitters. Fixtures with a colour wheel or a colour ring add a **Wheel / ring** list: *Nearest to the colour* (each wheel and ring takes its slot nearest the colour), or a slot by name — a ring's chase, which no colour can pick. See [Colour rings](show-tree.md#colour-rings). |
| **Dimmer** | A gradient bar marked with the fixture's actual intensity capability regions (e.g. strobe bands), with a live capability-name readout. |
| **Pan/Tilt** | An XY pad plus Home/Down/Front/Left/Right preset buttons — the same raw pan/tilt pair applied to every target, regardless of where it's physically rigged. |
| **Aim** | A combo box listing your **Stage Targets** instead of raw angles — each fixture's actual pan/tilt is computed from its rig position, so fixtures in different spots converge on the same point. Needs fixtures placed in Lighting Studio first. See [Stage Targets](stage-targets.md). |
| **Beam** | Focus / Frost / Iris sliders. |
| **Gobo / Shutter** | A slider plus a picker of the target fixture's actual named capabilities, with thumbnail previews. |
| **Strobe** | A single rate slider (0–100%). |
| **Effect** | The **effect browser** (below), a dynamically-built parameter panel, and Save/Export/Import for sharing `.qxfx` effect files. |

Every type except Effect also shows a per-look **Fade in / Fade out**
override (leave at "step" to inherit the scene or chaser's own timing). A
scene [transition](#transitions), when set, takes the place of the scene's
fade.

If the editor is showing a palette that isn't in the scene selected in the
outline, the scene **tries it on**: it plays on top of the look's own
palettes so you can see it on the rig, but it's never saved into the look.
The editor says "Trying it on “&lt;look&gt;” — not saved into it"; drag the
palette onto the look to keep it. With no look open it says "Not in a look".

![The Look Editor on an Effect look](../images/programming-tab-look-editor-effect.png)

1. **Change effect…** opens the effect browser (below).
2. **Spread** — how the effect is spread across the heads
   ([Spread](effects.md#spread)).
3. **Speed** — the speed master it runs on
   ([Speed masters](effects.md#speed-masters)).

### The effect browser

![The effect browser](../images/programming-tab-effect-browser.png)

1. **Search effects…** — every category at once.
2. **The tabs**: Recent, Color, Dimmer, Position, Beam, Other, WLED.
3. **The effects**, each with a live preview; the one picked is playing.
4. **What the picked effect does**, what it works with and its settings.
5. **Done** keeps it; **Cancel** puts back what the look had.

An Effect look picks its effect from a browser rather than a dropdown. The
browser is a **window of its own**, so once an effect is picked its
settings have the look editor's whole width. Over the settings, one line
names the effect, with **Change effect…** to open the browser again:

- **Every effect you pick plays at once**, with its own settings, so the
  rig shows it while you browse; the **arrow keys** step through the list,
  playing each as they go.
- **Done** (or Enter, or closing the window) keeps what is picked;
  **Cancel** (or Esc) puts back the effect the look had — its settings and
  name too.
- A new effect look, with nothing picked yet, opens the browser by itself.
- The window remembers where you put it.

- A **Search effects…** box searches every category at once, whichever tab
  is open.
- Tabs: **Recent**, **Color**, **Dimmer**, **Position**, **Beam**, **Other**
  and **WLED**. Recent is remembered between sessions.
- Each row shows the effect's name, what it does, and a live preview of
  what it does:
  - a **colour or dimmer** effect: a strip of 16 lights running it (amber
    and UV in their own colours, a shutter strobe blinking);
  - a **position** effect: where the heads point — the shape it traces (a
    diamond, an orbit, a figure 8, Stage Shape's outline round its target)
    as a faint line, every head as a dot along it, four of them bright —
    zoomed to fit its movement. One that only follows a joystick (a
    follow-spot) is shown being steered;
  - a **gobo, iris or zoom** effect: a row of beams seen end-on — their
    gobo pattern, or their size opening and closing.

  Presets sit under the effect they're built from.

Two effects are called fire-something:

- **Fire** is the RGB Matrix flame, rising from the lowest heads on stage
  (its Orientation setting flips it).
- **Firelight** is the per-fixture flicker. (Older looks that used it keep
  it.)

RGB Matrix scripts that share a name with an effect script are listed as
"&lt;name&gt; (RGB Matrix)" — Confetti, Lightning, Meteor, Plasma.

Settings come in a few shapes: a slider for a number, a list for a choice,
a pad for a drawn path, a compass for a direction, and — for an effect
that shows words — a **text box**. What you type is taken as you type it,
so the rig shows it as you go.

#### Text

**Text** puts words on a pixel grid — the look's group as laid out in its
head layout: a run of LED strips, a matrix. It is not the WLED text (that
one is drawn by the WLED device); this works on anything whose pixels are
heads. Settings:

- **text** — the words, in a 5 × 7 font covering every printable
  character. The grid needs to be 7 pixels tall for a whole letter.
- **style** — **Scroll** brings the text in from the right and out past the
  left, round and round; **Still** holds it in the middle.
- **speed** (pixels a second, scrolling), **size** (pixels per dot of a
  letter, 1–4, for a big matrix), **level**.
- **mirror** / **flip** — if it reads backwards or upside down, the strips
  count from the other end or the rows the other way up.

The letters are drawn in the look's **first colour**, over its **second**
when the look has one (otherwise the background is left as the rest of the
look sets it).

(The RGB Matrix's own **Text** algorithm, in the Functions window, gained a
**Centered** animation style too: the whole text still, centred across and
down, the X/Y offsets nudging it.)

#### Step Test Pattern

A diagnostic card for LED-strip steps whose fixtures are named the usual
way — front strips **"&lt;step&gt; #&lt;n&gt;"** ("DS1 #3": step DS1, its
third strip) and top bars **"&lt;step&gt;t&lt;n&gt;"** ("CS2t1"). On every
step's front:

- strip *n* lights its first *n* LEDs white, so the order of the strips
  and the end each counts from can be read;
- the last LED of every strip is white, so a strip's length can be checked;
- from the 10th LED, one vertical line per step number (DS1 one, CS3
  three), every other LED — **blue for DS, green for CS, red for US**;
- the step's **name** in the middle, white, and beside it a **spinner** as
  tall as the step going round once a second — if it stops, the output has
  frozen.

On every top bar a dim blue dot runs back and forth over the colour
pixels. If a name reads backwards, the strips count from the other end:
turn on **mirror**. Which strip is the top row comes from the strips'
heights on the rig (highest first), else their numbers.

**Effects on a big rig.** An effect's script is handed every head of every
fixture each tick; that list is now kept between ticks and only what
changed is updated, so a rig of 96 strips × 64 pixels costs about 8 ms a
tick in a release build (it was 68). Renaming or moving a fixture shows up
on the next tick.

An effect run across two or more groups is laid out **as the rig stands on
stage**: one grid with every head where the Lighting Studio draws it — across is
stage left to right, up is height (or upstage, when everything is at one
height). A sine over the towers, bars and steps then moves across the stage
as one, instead of group by group. An effect on a single group still uses
that group's own layout.

### WLED effects

Picking an effect from the **WLED** tab has the WLED fixtures in the look
run it **on the device itself**: they're sent the effect, its speed,
intensity, palette and option, the look's three colours and the look's
dimmer. Every other fixture in the look runs qlcconsole's own port of that
effect where one exists — 160 of them, every 1D effect WLED has: Android, Aurora, Blends, Blink, Blink Rainbow, Bouncing Balls, Bpm, Breathe, Candle, Candle Multi, Chase, Chase 2, Chase 3, Chase Flash, Chase Flash Rnd, Chase Rainbow, Chase Random, Chunchun, Color Clouds, Colorful, Colorloop, Colortwinkles, Colorwaves, Dancing Shadows, Dissolve, Dissolve Rnd, Drip, Dynamic, Dynamic Smooth, Fade, Fairy, Fairytwinkle, Fill Noise, Fire 2012, Fire Flicker, Fireworks, Fireworks 1D, Fireworks Starburst, Flow, Flow Stripe, Glitter, Gradient, Halloween Eyes, Heartbeat, Hyper Sparkle, ICU, Juggle, Lake, Lighthouse, Lightning, Loading, Meteor, Meteor Smooth, Multi Comet, Noise 1, Noise 2, Noise 3, Noise 4, Noise Pal, Oscillate, Pacifica, PacMan, Palette, Percent, Perlin Move, Phased, Phased Noise, Plasma, Police, Popcorn, Pride 2015, PS 1D Balance, PS Chase, PS Dancing Shadows, PS DripDrop, PS Fire 1D, PS Fireworks 1D, PS Hourglass, PS Pinball, PS Sparkler, PS Spray 1D, PS Springy, PS Starburst, Railway, Rain, Rainbow, Rainbow Runner, Random Colors, Ripple, Ripple Rainbow, Rolling Balls, Running, Running Dual, Saw, Scan, Scan Dual, Scanner, Scanner Dual, Shimmer, Sine, Sinelon, Sinelon Dual, Sinelon Rainbow, Solid, Solid Glitter, Solid Pattern, Solid Pattern Tri, Sparkle, Sparkle Dark, Sparkle+, Spots, Spots Fade, Stream, Stream 2, Strobe, Strobe Mega, Strobe Rainbow, Sunrise, Sweep, Sweep Random, Tetrix, Theater, Theater Rainbow, Traffic Light, Tri Fade, Tri Wipe, TV Simulator, Twinkle, Twinklecat, Twinklefox, Twinkleup, Two Areas, Two Dots, Washing Machine, Wavesins, Wipe, Wipe Random; and, following the audio input (Inputs/Outputs; with none they rest, dark), the sound-reactive DJ Light, Freqmap, Freqpixels, Freqwave, Gravcenter, Gravcentric, Gravfreq, Gravimeter, Juggles, Matripix, Midnoise, Noisefire, Noisemeter, Noisemove, Plasmoid, PS GEQ 1D, PS Sonic Boom, PS Sonic Stream, Puddlepeak, Puddles, Ripple Peak, Rocktaves, Waterfall — written from what each does on a device (not from WLED's
code), so they look the same on the WLED fixtures and the rest.
Hyper Sparkle, Police and Two Areas are WLED's older names, offered when a
device's firmware still lists them. WLED's 2D effects, and Copy Segment,
Image and Slow Transition (which need the device's other segments, its
stored images or its colour history), run on WLED devices only.

A WLED effect with no port runs on a WLED device and nowhere else, so the
browser only offers it when the look's targets include a WLED device;
otherwise the WLED tab lists just the ported ones. (An effect the look
already runs stays listed either way.)

The WLED list comes from a WLED fixture in the show, or from the installed
WLED fixture definition if there isn't one.

Red warning text appears if a look's type has no matching channel on any of
its targets (e.g. a Gobo look applied to fixtures with no gobo wheel), and —
for Aim looks specifically — the outline marks with ⚠ (hover for which)
the fixtures that physically **can't reach** the point they're aimed at (see
[Stage Targets](stage-targets.md#reachability)).

## Design vs. Operate mode

It follows the app's global Design/Operate mode:

- **Design mode**: what the Workbench has loaded plays live so you can see
  what you're building (**Stop** in its header holds it), and edits don't
  disturb a running show elsewhere.
- **Operate mode**: what is loaded runs as the actual show output.

The 2D Monitor plot, if open, stays in sync with the scene being edited.

## Saving your work

Ordinary look edits (color, dimmer, gradient, gobo, targets…) apply live —
there's nothing to "commit," just save the workspace as usual.

The one exception: dragging an Aim target's position or nudging a Pan/Tilt
palette with a joystick/HID controller in Design mode needs an explicit
**Save Positions** button (a two-click "arm, then confirm" action) before it
persists to the file — because that's the one edit path that isn't already
fully live.

**Snapshot** (toolbar button) is a different thing entirely: it bakes the
scene's *current live DMX output* into it as static values — useful for
capturing a look you built live on an external console.

## Terminology

| Term | Meaning |
|---|---|
| **Look** | A palette as applied inside a specific scene: a look row in the outline or the Workbench. |
| **Outline** | Build ▸ Scenes: the tree of scene folders, scenes, collections and chasers, each scene's targets and looks. |
| **Workbench** | The editor on the right: what is loaded, its layers, and the editor pane. |
| **Layer** | One row at the top of the Workbench: a scene, or a function that plays as it is (▣). A collection's members are its layers. |
| **Editor pane** | The Workbench's right-hand pane: what the selected row edits. |
| **Palette** | A reusable, named preset (Color, Dimmer, Pan/Tilt, Aim, Beam, Gobo, Shutter, Strobe, Effect) that expands per-fixture at run time. Lives in its own tree, independent of any scene. |
| **Target** | A fixture or group a scene's looks apply to — **dynamic** (a group, membership tracked live) or **fixed** (an individual fixture, static). |
| **Base look** | A look positioned before any Effect in the list — paints fixtures directly. |
| **Nested look** | A Color/Dimmer look placed *under* an Effect — feeds that effect's gradient/master instead of painting fixtures itself. |
| **Bundle** | A portable, target-free snapshot of an ordered palette stack, "stampable" onto any scene. |
| **Snapshot** | Bakes a scene's current live output into it as static values. |
