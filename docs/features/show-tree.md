# Show Tree

The Show Tree tab (beside Programming, **⇧⌘6**) is where you write a show
as cues. Everything is in one tree:

**Production → Cue list → Cue → Layer**

- A **layer** is a group (or several groups, or some fixtures) and the
  looks it plays in that cue, one per attribute: Colour, Dimmer, Position,
  Gobo, Zoom, Focus / Frost / Iris, Shutter and effects. Each look is a
  palette.
- A **cue list** can be **Tracking**, where a cue carries on whatever it
  doesn't change, as on an Eos or MA. It can also be **Non-tracking**, where
  each cue plays only what it holds, as a chaser does.
- Cues have **Track / Block / Cue-only** modes, **Release**, a **base cue**
  and **includes**. Values that come from somewhere else show grey and say
  where they came from.

**The Programming tab works alongside it.** You build palettes, groups,
one-off scenes and effects there. The Show Tree uses the same palettes and
groups, and the same Look Editor.

**You never build the playback by hand.** Each cue list plays as a
**chaser** of the same name, each cue as a **collection**, and each layer as
a **scene**. The tree makes these for you, files them under `Productions/`,
and rewrites them after every change. The scenes and collections are hidden.
The chaser is visible, so it plays anywhere a chaser does: a Virtual Console
Cue List, the footer GO, or show control by cue number. See
[Playing it](#playing-it).

## The tab

- **Header.** It has + Production, + Cue list, + Cue, + Layer, Record,
  Delete, Undo, Redo and Import chaser…. On the right are two pieces of
  status:
  - what the rig shows, such as *"▶ On the rig: 1.0.1 Verse"* (see
    [Seeing it on the rig](#seeing-it-on-the-rig));
  - while you drag, what the drop will do; after an edit, which cues it
    changed (*"Changed: 1.0.1 Verse, 1.0.2 Out"*).
- **Left: the tree.** It shows productions, cue lists, cues, and each cue's
  layers. Grey **⤳** rows under a cue are layers it doesn't hold itself but
  that are on stage in it. They are marked *tracked from*, *included from*
  or *base*. A cue's badges are **■ base**, **⊘ block**, **◐ cue-only** and
  *includes N*. A layer that plays a function as it is shows **▣**.
- **Middle: the page** for what is selected:
  - **A cue list** shows its mode and base cue.
  - **A cue** shows its number, name, note, mode, base check box, includes,
    fade in / out, hold, cue looks, every layer on stage in it, a dashed
    drop area, and *Next cue receives*.
  - **A layer** shows one row per attribute.
- **Right: the [Library](#the-library).** It holds groups, palettes and
  functions to drag in.
- **Bottom: the Look Editor**, when **✎** opens a value. Under it is
  **[the command line](#the-command-line)**, which is always there.

## Quick start

### From a chaser you already have

1. **Import chaser…** → pick the chaser. A new production named after it
   holds a Non-tracking cue list that plays exactly as the chaser did. The
   chaser itself is not touched.
2. A report says what was made:
   - Each step became a cue. Its number comes from the step's cue number,
     else from a name like `0.0.4-SHOUT`, else its position. Fades, hold and
     note are kept.
   - A scene of groups and palettes became a layer.
   - A step's collection used inside another step became an **include**.
   - Anything else plays as it is, with the reason. That covers raw channel
     values, EFX, RGB Matrix, a scene with its own fades, and so on.
   - Where two scenes in one cue set the same group, the chaser ran both:
     HTP on intensity, LTP on the rest. The report lists these cues, and the
     cue list keeps the later scene's values.
3. Click a cue. In Design mode it plays on the rig. Open a layer and change
   it.

**To convert the import to tracking:**

1. Select the list and set **Mode: Tracking**.
2. Pick a **Base cue**.
3. Tick **Release** on layers that should leave when their cue ends.
4. Delete layers that only repeat the cue before.
5. Remove includes of the cue before (the cue page, **Includes**, **−**).

*Next cue receives* on each cue page shows what carries on as you go.

### Three cues with the mouse

Use your own group and palette names in place of Towers, USTs, Blue and Red.

1. **+ Production**. Then, with it selected, **+ Cue list** named
   `01-Opener` (a leading number numbers its cues: the first is 1.0.0).
2. Select the list and set **Mode** to **Tracking**. **+ Cue list** makes a
   Non-tracking list.
3. **+ Cue** → **1.0.0**. Type the name `Preshow`.
4. Drag **Towers** from the Library (All → Groups) onto the dashed area of
   the cue page: that makes a layer. Drag **Blue** and a dimmer palette onto
   the layer's row.
5. **+ Cue** → **1.0.1** `Verse`. Towers is there already, grey: *tracked
   from 1.0.0 Preshow*.
   - Drag **USTs** onto the page and **Red** onto its row.
   - Click the grey Towers row. On its Dimmer row, **⋯ → Level…** 100 sets
     it in this cue.
6. **+ Cue** → **1.0.2** `Out`. Set its **Mode** to **Block**: it starts
   from nothing, so with no layers it is dark.
7. The list plays as the chaser **01-Opener**. GO it from the footer, or pick
   it in a Virtual Console Cue List.

### The same, typed

Click the command line (or press **:** in the tree), then:

```
list "01-Opener"
cue + "Preshow"
Towers colour Blue dimmer 45
cue + "Verse"
USTs colour Red
Towers dimmer 100
cue + "Out" block
```

- `list "…"` makes a **Tracking** list in the selected production (or the
  only one).
- Each `cue +` adds a cue after the selected one and selects it, so the
  lines after it act on that cue.
- `Towers dimmer 100` in 1.0.1 brings the tracked Towers layer into the cue,
  keeping its Blue.
- Every line is one undo step.

## Adding layers

Drag from the Library onto the **tree** or onto the **page** in the middle;
both take the same drops. The line at the top right says what a drop will
do before you let go, for example *"one layer on Towers + USTs in 1.0.0
Preshow (⌥: a layer for each group)"*.

- **Several groups (or fixtures) dropped together make one layer** on all
  of them. Hold **⌥** as you drop for a layer each, in one undo step.
- **+ Layer** does the same with what is selected in the Library: one
  layer, or **⌥-click** (or its arrow → *A layer for each*) for one each.
- **Onto a layer**, groups are added to that layer. That works on its row in
  the tree, its row on the cue page, or anywhere on its own page.
- **Onto a grey (carried-in) layer**, the layer is brought into this cue
  with the new groups added, keeping the values it carried. This is one
  undo step.
- **A palette** onto a layer sets that layer's look. Onto the cue (its row,
  or the page outside a layer) it is a **cue look**.
- **A function** (an EFX, an RGB Matrix, a scene with channel values) onto
  a cue plays as it is, as a layer of its own (▣). The tree doesn't set
  its looks, and it has no Release.
- After a drop on the cue page, the cue stays selected, so you can keep
  dropping.
- To take a group out, right-click a layer on several groups →
  **Remove &lt;group&gt; from layer** (never its last).

A layer stays **one layer** when its groups change. Added to in a cue it was
tracked into, it takes the tracked layer's place and values. Added to where
it is set, the change carries on to the cues it tracks into.

### Split and merge layers

- Right-click a layer on several groups → **Split layer (one per group)**.
  You get a layer per group (and one on its fixtures), each with the same
  looks. Nothing on stage changes, and the pieces keep tracking as the layer
  did: where the same layer is set again in a later cue, it is split there
  too.
- **Merge layers** makes one layer from several layers of one cue. It is
  **refused, with the reason**, rather than guessed, when:
  - they set an attribute differently (*"Towers sets Colour to Blue, USTs to
    Red"*);
  - one is released and another isn't;
  - merging would change what a later cue plays (*"Merging would change what
    cue 0.0.5 plays on USTs (Dimmer)"*).

  Merge needs several layers selected, and the tree doesn't take a
  multi-selection yet (see [Limits](#limits-and-not-yet)).

## Setting looks

A layer's page has one row per attribute:

| Attribute | |
|---|---|
| Colour | Colours dropped together are colour 1, 2, 3 |
| Dimmer | |
| Position | |
| Gobo, Zoom, Focus / Frost / Iris | The **Beam** family, indented; each resolves on its own |
| Shutter | Shutter and strobe |
| *kind* effect | One row per effect kind (Colour effect, Position effect…), plus an **Effect** row to add one |

Each row shows its value, where it comes from, and four controls:

- **A value set here** is plain. **One that comes from elsewhere** is grey
  and italic and says where: *"Blue — from cue 1.0.0 Preshow"* (a cue look),
  *"Intensity 45% — tracked from 1.0.0 Preshow"*, *"… — included from …"*,
  *"… — base, from …"*. A row with nothing on it shows —.
- **applies to N of M** appears when only some of the layer's fixtures have
  that attribute (or, for a gobo by name, that gobo). The rest ignore the
  look; hover to see which ones.
- **✎** opens the value's palette in the **Look Editor** at the bottom of the
  tab (the Programming tab's, see
  [The inline Look Editor](programming-tab.md#the-inline-look-editor)). A
  named palette is shared, and the editor says so: changing it changes every
  cue and scene that uses it.
- **⋯** opens a menu:
  - **Set to** (or **Set in this cue**, for a grey value): any palette of
    that kind, the ones you used recently first.
  - **Update cue X**, for a tracked or included value: see
    [Changing a tracked value](#changing-a-tracked-value).
  - **Level…** (Dimmer) or **Colour…** (Colour): a value you pick. See
    [Values you pick or type](#values-you-pick-or-type).
  - **Its own colour / dimmer** (on an effect row): see
    [Effects](#effects).
  - **None (off here)**: the attribute is off in this layer, whatever it
    would inherit.
  - **Clear (inherit)**: takes this layer's value away, so the row inherits
    again.

Setting anything on a grey layer makes it this cue's own, in the same undo
step, with its carried values kept.

**Cue looks.** A palette dropped on a cue (or `cue look …` typed) is a look
for the cue. It reaches every layer of the cue's own that doesn't set that
attribute itself. It does not reach grey (carried-in) layers. Before you let
go, the hint says how far it reaches: *"Blue → cue look on 1.0.0 Preshow:
reaches 2 of 3 layers"*.

**A Dimmer look on fixtures with no dimmer channel** (RGB bars, pixel heads)
dims their colour: Blue at 19% goes out as a dim blue, and the Rig Overview
shows it that way.

### What wins

Inside a cue, for each layer and attribute, the first of these that sets it
wins:

1. the effect's own looks (inside that effect only)
2. the layer's own looks
3. the cue's looks
4. included cues (the later include first)
5. what tracked in from the cue before

**This cue beats earlier cues; inside a cue, the nearer source wins.**

### Values you pick or type

A level or colour you pick (⋯ → **Level…** / **Colour…**) or type
(`dimmer 45`, `colour #33aa55`) **reuses the palette that plays exactly that
value**. Later you can adjust it everywhere at once by changing that
palette. If several palettes play it, the order is:

1. the one this cue list already uses most;
2. the one you used most recently;
3. the first by name.

Only plain Dimmer and Colour palettes are matched.

When no palette plays it:

- **Typed:** a **new palette** is made, named and filed the way your
  palettes of that kind are. With `Intensity 45%` in `Palettes/Dimmer`,
  `dimmer 30` makes `Intensity 30%` there. Colours get a colour's name or
  hex, as yours do (`Red`, `Sea Green #33aa55`). The command line says
  *reused "Intensity 45%"* or *new palette "Intensity 30%" in
  Palettes/Dimmer*. Undo takes the value off the cue; the new palette stays.
- **Picked:** you get a **one-off** value, just for this layer. It shows in
  the Library's **Recent**, not in All. **Make reusable** next to it on the
  row turns it into a palette in one click, named as above. To choose the
  name yourself, right-click it in Recent → **Make reusable…**. A one-off
  that no cue uses any more is deleted.

### Gobos by name

A gobo look can mean *the same gobo* on fixtures with different wheels.

1. Open the look in the Look Editor (✎).
2. Pick the gobo on one type's wheel (**Wheel of** chooses the type).

The look takes that gobo's name (for example "Dots"), and every other type
plays **its own** gobo of that name. A type whose wheel has none ignores the
look, and *applies to* counts only the fixtures that have it. The editor
lists, per type, what each one plays.

- Pick a slot on another type's wheel and it is kept for that type. This
  helps when the names differ ("Dot Ring" on one, "Dots" on the other).
- **Gobo name** changes the name. Each type finds its gobo by the first of
  these that finds anything:
  1. a slot **picked** for that type;
  2. the **same name**, where case, spaces, `-` and `_` don't matter. A
     gobo's picture names it too, with its folder: `ClayPaky/gobo00006` (a
     picture name only ever matches that picture);
  3. **singular or plural**, ignoring the word "gobo" and numbers: "Dot",
     "Dots" and "Gobo 3 Dots" are the same;
  4. the **one** slot whose name starts with or contains it, for 3 letters or
     more ("Breakup" finds "Breakup Rotating").
- If a step finds **more than one** gobo on a type ("Dots" when the wheel has
  only "Dots Shake" and "Dots Rotating"), nothing is guessed:
  - that type ignores the look;
  - the row says *"ambiguous: pick per type…"* (hover to see which);
  - the link opens the Look Editor so you can pick the gobo on that type's
    wheel.

  The same name twice on one wheel (an "Open", say) counts as one gobo.
- A gobo look that was never named this way plays one DMX value on every
  fixture, as before. Editing a gobo palette in the Look Editor (on either
  tab) names it.

### Effects

An effect palette dropped on a layer (or set from the **Effect** row's ⋯)
runs on that layer's fixtures. On the effect's row, **⋯ → Its own colour /
dimmer** gives it a colour or dimmer that applies inside the effect only
(*"Sinelon in Red"*). On the command line, a colour or dimmer right after an
effect is its own: `Steps effect Sinelon colour Red`.

## Tracking

Select a cue list to set its **Mode**:

- **Non-tracking**: each cue plays only its own layers and includes, as an
  imported chaser does.
- **Tracking**: a cue starts from the one before and changes only what it
  sets.

Each cue has a **mode** too, set on its page, with right-click → **Mode**, or
by typing it:

| Mode | Starts from | Passes on |
|---|---|---|
| **List default** | as Track in a Tracking list, as Block in a Non-tracking one | as that mode |
| **Track** | the cue before | everything on stage |
| **Block** (⊘) | nothing: only its own layers and includes | everything on stage |
| **Cue-only** (◐) | the cue before | what it started from: its changes don't pass on |

- **Release.** A layer with **Release** ticked (on its page) leaves when its
  cue is left. It goes back to what the **base cue** plays on that layer,
  else dark. Ticking it on a grey layer makes the layer this cue's own first.
- **Base cue.** Pick it on the list's page, tick *"The list's base cue"* on a
  cue, or right-click a cue → **Make the base cue**. It is marked ■ base.
- **Includes.** A cue plays the output of the cues it includes, as if you
  stood in them, under its own layers. When the included cue changes, so
  does this one.
  - On the cue page, **+** adds one. A cue that would make a loop (one that
    already plays from this cue) is greyed out with the reason.
  - **−** takes one out; **↑ ↓** reorder them (the later one wins).
  - You can also drag a cue onto another with **⌥** held, or right-click →
    **Include cue**.
- **Next cue receives**, on a cue's page, says what the cue after it starts
  from:
  - each layer with its values;
  - what goes *back to the base*;
  - what a release leaves *dark*;
  - *nothing* when the next cue is a Block cue.

After every change the top right names the cues it changed (*"Changed:
…"*). That is the edited cue, the cues after it that track from it (up to a
Block cue), and every cue that includes one of those.

**Hold.** A new cue holds **∞**: it waits for GO. A cue with a set hold
follows on by itself when the hold runs out, as a chaser step does.

### Changing a tracked value

A grey value came from an earlier cue. On its row, **⋯** offers two
choices:

- **Set in this cue**: the value changes here and from here on, and the
  layer becomes this cue's own;
- **Update cue 1.0.0 Preshow**: the value changes *where it was set*, so
  every cue it tracks into follows. Its **None (off from there on)** turns it
  off at the source.

### Record and duplicate

**Record** (in the header, or right-click a cue → **Record as new cue**)
freezes what is on stage into a new **Block** cue right after the selected
cue. Each layer becomes its own, with the looks it plays. "What is on stage"
can mean three things:

- **The selected cue**: what it puts on stage, including its own layers,
  what tracked in and what it includes. This is what Design previews.
- **The running cue**: the cue its cue list is playing for real (footer GO,
  a Virtual Console Cue List).
- **Everything lit**: every cue list running, the Design preview on top, and
  any other scene, EFX or RGB Matrix that is on (each as a layer that plays
  it as it is). Programmer and Simple Desk values are not recorded.

When only one of these has anything, or they would record the same thing,
Record just records it. When they differ, it **asks**. In Design the dialog
starts on the selected cue; in Operate, on the running cue. Tick **Don't ask
again** to make your answer the default.

The defaults live in **Settings** (⌘,) → **Show Tree**. There is one for
Design and one for Perform (Operate), each set to Ask or one of the three.
A default that names something not on stage (the running cue, with nothing
running) asks instead. Typing `record selected`, `record running` or
`record lit` skips the question.

**Duplicate cue** (right-click a cue, or `dup`) copies the cue's own content,
not what tracks into it. It is never the base cue.

Record and Duplicate number the new cue as + Cue does. + Cue goes after the
selected cue and counts up its last number when that is free (1.0.0 →
1.0.1). Otherwise it makes a point cue: with 0.0.4 and 0.0.5 taken, you get
0.0.4.5. **Cues are never renumbered**, because show control fires them by
number. You can type a number into a cue's **Number** field yourself.

## Seeing it on the rig

**Design mode: the cue you are editing is on the rig.**

- Selecting a cue, or a layer in it, plays that cue (its whole tracked
  state), and every change shows at once. The top right says *"▶ On the rig:
  1.0.1 Verse"*.
- Selecting another cue moves the preview there. A layer both cues share
  keeps running, with no re-fade.
- Selecting the list or nothing, or leaving the tab, stops the preview.
- This holds **also while a cue list is running for real**, its own or
  another (*"▶ On the rig: 1.0.1 Verse, over Full Show running"*). The cue
  you are on takes precedence on every channel it sets, so a dimmer you edit
  down shows even over a running cue at full. The running list keeps going
  underneath.
- When the preview stops, the stage is what really runs again. Nothing a
  chaser or the Virtual Console started is stopped.

**Operate (Perform) mode: the running cue list owns the stage.** Selecting a
cue doesn't touch the rig. Your changes reach the stage when the cue next
plays, on GO. The top right says *"Live: editing is blind, changes reach the
stage on GO"*.

- **Blind** (the app's Design-mode toggle) mutes the physical rig while the
  2D views keep showing, and that includes the preview. A Show Tree "blind
  edit" of its own is still to come.
- The footer's live **power estimate** counts the preview as anything else
  lit in Design (see
  [Power distribution](other-improvements.md#power-distribution)).

## The command line

The line at the **bottom** of the tab is for typing what you would say. It
is always there and calls the same operations as the mouse.

- **Getting to it.** It never takes the keyboard by itself: click it, or
  press **:** in the tree. **Esc** goes back to the tree.
- **Running a command.** **Enter** runs it. **↑ ↓** recall earlier commands.
  The hint under the line lists the words.
- **One undo step each.** Every command is one undo step. A command that
  fails part way changes nothing: the line stays, and the reason shows in
  red beside it.
- **Where it acts.** A command acts on the cue (and cue list) selected in
  the tree, and leaves the selection on what it made or touched.

| Type | Does |
|---|---|
| `list "01-Opener"` | Selects that cue list, or makes a new **Tracking** one in the selected production |
| `cue 1.0.1 "Verse"` | Selects cue 1.0.1, or makes it in number order. Renames it if the name differs |
| `cue + "Verse"` | A new cue after the selected one, numbered as + Cue does |
| `track` · `block` · `cueonly` · `default` | Sets the selected cue's mode (`default` = List default). Also works after `cue …`: `cue + "Out" block` |
| `base` | Makes the selected cue the base cue |
| `Towers colour Blue dimmer 45` | The cue's layer on Towers (made if needed; a grey one is brought in), then its looks |
| `Towers USTs dimmer 100` | **One** layer on both, as a drop of both does |
| `Towers+USTs dimmer 100` | The same |
| `Towers, USTs dimmer 100` | A layer **each**, as an ⌥-drop does |
| `Movers/Front/Spots colour Red` | A group by its folder path |
| `T1-CW colour Red` | A fixture by name works as a group does |
| `cue look colour Red` | A cue look |
| `Steps effect Sinelon colour Red` | An effect, and its own colour |
| `Towers gobo none` (or `off`) | None: off here |
| `Towers gobo clear` | Clear: inherit again (`effect none` / `clear` covers every effect there) |
| `Steps colour #33aa55` | Reuses the palette that plays exactly that, else makes one ([how](#values-you-pick-or-type)) |
| `Steps dimmer 45` (or `45%`) | The same for a level, 0–100 |
| `function Haze look, House lights` | Functions played as they are, a layer each |
| `release Towers` | Turns Release on, or off again |
| `include 0.0.3` | Includes that cue; again, takes it out |
| `record` | Record as new cue, as the button does (your setting, or it asks) |
| `record selected` · `record running` · `record lit` | Records what the word says |
| `dup` · `undo` · `redo` | As Duplicate cue, Undo and Redo |

**Names** are the Library's (groups, fixtures, palettes, functions), matched
whole and ignoring case; the longest name that fits wins. A name with spaces
works as it is (`AT W ALL dimmer 100`). Put it in quotes if it would clash
with another name or holds a `,` or `+`.

**Values.** A palette's name, `none` / `off` / `clear`, a level (Dimmer) or a
`#rrggbb` colour (Colour). Other attributes take palette names only.

**Other words it takes:** `color`; `dim` and `level`; `pos`; `focus`,
`frost` and `iris` (all Focus / Frost / Iris); `strobe` (Shutter); `fx`
(effect); `fn` (function); `cue-only`; `cue new`; and `layer` before a group.

### Tab completion

**Tab** completes the word you are typing, from what the line takes there:

| After | Tab offers |
|---|---|
| nothing yet | the command words, groups (also by folder path) and fixtures |
| a group or fixture | more groups, or the attributes |
| an attribute | that kind's palettes (an effect's for `effect`), and `none`, `off`, `clear` |
| `cue` | `look`, `+` and the list's cue numbers |
| `cue` and a number | the cue modes and `base` |
| `include` | the list's cue numbers |
| `list` | the cue lists (the selected production's first) |
| `function` | the functions |
| `release` | groups and fixtures |
| `record` | `selected`, `running`, `lit` |

- **One match:** Tab puts it in, with a space after it.
- **Several matches:** Tab completes the part they share and shows them
  beside the line (ten at a time, the current one in [brackets]). **Tab
  again** puts in the next one, round and round. With nothing typed yet,
  Tab only lists them, and Tab again steps through.
- **Names with spaces** complete as one word (`AT W` → `AT W ALL`).
- **Quotes.** A name you begin with a quote is closed for you. A name with a
  `,` or `+` in it is put in quotes.

In the command line, ⌘Z undoes your typing. Type `undo` to undo the tree.

## Undo

- **Every change in the tab is one step** of **Undo** (⌘Z) / **Redo** (⇧⌘Z),
  or the header buttons, whose tooltips name the step. That includes every
  drop of groups or palettes (an ⌥-drop of several groups too), every ⋯
  choice, every command, Record, Split, Merge and an import. Several
  functions dropped at once are one step each. Up to 200 steps.
- Undo puts the tree back and rebuilds the scenes, collections and chaser
  from it, including the chaser's playback settings and any one-off values.
- **What tree undo doesn't cover:**
  - a palette's values changed in the Look Editor (a colour dragged, a
    level moved): those are the palette's, as in the Programming tab;
  - **Make reusable**;
  - a palette a command made, which stays after undo.
- **Undo is not shared with the Programming tab.**

## The Library

The panel on the right. One filter box at its top narrows every section. It
finds a group at any depth, or a folder by its name, and clearing it closes
again what it opened.

- **Recent**: the last ten groups, palettes and functions you used in this
  session, one-off values included.
- **In this cue list**: what the selected list already uses, most used
  first, for example *"Towers ×6"*. It shows while a list or one of its cues
  is selected.
- **Pinned**: right-click anything → **Pin** (★). This is kept on this
  computer.
- **All**:
  - **Groups**, in the Fixture Manager's folders;
  - **Palettes**, by kind;
  - **Functions (played as they are)**: not chasers, and not what the tree
    made.

Select several items (⌘- or ⇧-click) to drag them together or use **+
Layer**.

## Editing the tree

Right-click in the tree for:

- **+ Production…**, **+ Cue list…**, **+ Cue**;
- on a cue: **Make the base cue**, **Include cue**, **Mode**, **Record as new
  cue**, **Duplicate cue**, **Move up** / **Move down**;
- on a layer: **Rename…**, **Detach to plain scene**, **Split layer**,
  **Merge layers**, **Remove &lt;group&gt; from layer**;
- **Rename…** on a production or cue list, and **Delete** on anything but a
  grey row.

**Moving cues.** Drag a cue onto another cue of its list to move it there;
its number stays. With **⌥** held, the drop makes the other cue include it
instead.

**Deleting.** **Delete** (or Backspace) in the tree deletes the selected cue
or layer at once. A cue list or production asks first. Undo brings any of
them back.

**Detach to plain scene** turns a layer into an ordinary, visible scene
named after the cue and layer. The layer then plays that scene as it is, and
the tree no longer edits its looks.

## Playing it

- Each cue list plays as a chaser of the same name, at
  `Productions/<production>` in the Functions window. Its steps carry the
  cue numbers, fades, holds and notes.
- Pick the chaser in a Virtual Console Cue List, GO it from the footer, or
  fire its cues from QLab by number ([Show control](show-control.md)).
- The cue collections and layer scenes are hidden, in
  `Productions/<production>/<list>`. A scene two cues play is the same
  scene, so it keeps running through GO, with no re-fade, and effects keep
  their place.
- **Change them here, not in the Functions window.** The next change in the
  tree rewrites the chaser's steps, name and folder, and the hidden
  functions. The chaser's playback settings are yours to set and are kept:
  run order, speed modes, GO on the beat.
- A show saved with a Show Tree still opens and plays in a build without
  one. The chaser and its cues are ordinary functions there; only the tree
  is lost if you save it there.

## Keys and mouse

| Keys / mouse | Does |
|---|---|
| ⇧⌘6 | Bring the Show Tree forward |
| Drag from the Library onto a cue, a layer, or the page | One layer on everything dropped; onto a layer, added to it |
| **⌥** held as you drop groups | A layer for each group |
| ⌥-click **+ Layer** | A layer for each group selected in the Library |
| Drag a cue onto another cue | Move it there; with **⌥**, include it |
| Delete / Backspace (in the tree) | Delete the selected cue or layer (a list or production asks) |
| **:** (in the tree) | To the command line |
| **Esc** (in the command line) | Back to the tree |
| **Tab** (in the command line) | Complete the word; again, the next match |
| ↑ / ↓ (in the command line) | Earlier / later commands |
| Enter (in the command line) | Run it |
| ⌘Z / ⇧⌘Z | Undo / Redo in the tree (in the command line: type `undo`) |
| F1 | This guide |

## Limits and not yet

- **Merge layers can't be reached by mouse yet.** The tree selects one row
  at a time, and Merge needs several layers of one cue selected, so it stays
  greyed.
- **+ Cue list makes a Non-tracking list; `list "…"` makes a Tracking one.**
  Set the mode on the list's page either way.
- **Effects in the cue don't take precedence in the Design preview.** The
  cue's looks win over a running list, but an EFX, RGB Matrix or other
  function played as it is mixes with what runs, as it normally would.
- **Gobo names ignore numbers** in the singular/plural step. So "Gobo 3
  Shake" can find a wheel's only "Gobo 2 Shake" (with several, it is
  ambiguous). Pick the slot for that type to be sure. A second gobo wheel
  and a swapped glass aren't matched by name.
- **An effect can have its own Colour and Dimmer, not its own Gobo.** Prism
  and gobo rotation have no rows yet.
- **Blind editing in the Show Tree** (editing a cue without previewing it in
  Design) is still to be decided. Today only the app's Blind, which mutes
  the whole rig, does that.
- **Edits in Operate are blind** and reach the stage on the cue's next GO.
- **Import** doesn't carry a step's skip flag, its own speed master or Sub GO
  steps.
- **Later:**
  - sub-cues in the tree;
  - the cue runner (a Run tab and a VC widget);
  - group sets and child layers;
  - line aims in the tree;
  - a production switcher for show control.
