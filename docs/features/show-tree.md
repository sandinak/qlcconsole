# Show Tree

One tree for writing a show, top to bottom: **Production → Cue list → Cue →
Layer → looks**. A *layer* is a group (or some fixtures) and the looks it
plays in that cue. The scenes, collections and the chaser that play it are
made for you, filed out of the way, and kept up to date as you edit — you
never file a scene by hand.

The **Show Tree** tab sits beside Programming. Programming stays the place
for palettes, one-off scenes and effect work; the Show Tree uses the same
palettes and groups.

## The tab

- **Left — the tree.** Productions, their cue lists, each cue (number and
  name), and each cue's layers. Under a cue, grey rows marked ⤳ are layers
  still on stage from an earlier cue (*tracked*), from an included cue, or
  back to the base cue.
- **Middle — the selected cue or layer.**
  - A **cue**: number, name, note, mode, fade in, fade out and hold, its cue
    looks, every layer on stage in it (grey ones with where they come from),
    and what the **next cue receives**.
  - A **layer**: one row per attribute — Colour, Dimmer, Position, Gobo,
    Zoom, Focus / Frost / Iris, Shutter, and its effects. A value set here is
    plain; one coming from the cue or an earlier cue is grey and says where:
    *"Blue — from cue 1 Preshow"*, *"45% — tracked from 0.0.3 Announcer"*.
    **✎** opens the value's palette in the **Look Editor** under the tree
    (the Programming tab's; a named palette is shared, so the editor says
    so before you change it everywhere). **⋯** on a row sets it here (any palette of that kind, recent ones first,
    or a one-off level or colour), turns it **off here** (None), or clears it
    so it inherits again. *"applies to 4 of 8"* means only 4 of the layer's
    fixtures have that attribute (for a gobo by name: that gobo); the rest
    ignore it.
- **Right — the Library.** **Recent** (what you just used, one-off values
  included), **In this cue list** (the groups, palettes and functions the
  selected list already uses, most used first: *"Towers ×6"*), **Pinned**
  (right-click → Pin), and **All**: groups in the
  Fixture Manager's folders, palettes by kind, and functions to play as they
  are. One filter box narrows them all — a group at any depth, or a folder by
  its name; clearing it closes again what it opened.
- **Top right** says what is on the rig: *"▶ On the rig: 0.0.4 SHOUT"* (see
  [Seeing it on the rig](#seeing-it-on-the-rig)), and after a change which
  cues it changed: *"Changed: 0.0.4 SHOUT, 0.0.5 WHO ARE WE"*.

## Building a cue list

1. **+ Production**, then **+ Cue list**. A list named with a leading number
   ("01-Everybody Scream") numbers its first cue 1.0.0.
2. **+ Cue** adds one after the selected cue, numbered for you: the next
   whole number if it's free, else a point cue (0.0.4 and 0.0.5 taken →
   0.0.4.5). Cues are **never renumbered** — show control fires them by
   number.
3. Drag **groups** from the Library onto the cue. Groups (and fixtures)
   dragged together make **one layer** on all of them; hold **⌥** as you drop
   for a layer per group. Or select them and **+ Layer**.
   Drop a group on a **layer** to add it to that layer. Right-click a layer
   on several groups → **Remove &lt;group&gt; from layer** takes one out.
4. Drag **palettes** onto a layer to set them, or onto the **cue** to set a
   *cue look* — it reaches every layer of the cue that doesn't set that
   attribute itself. Before you let go, the line at the top right says what
   the drop will do: *"Blue → cue look on 1 Preshow: reaches 2 of 3 layers"*.
5. A **function** (an EFX, an RGB Matrix, a scene with channel values) dropped
   on a cue plays as it is, as a layer of its own (▣).
6. Drag a **cue** onto another cue of its list to move it there (its number
   stays); hold **⌥** as you drop to make the other cue **include** it.

Every change is one step of **Undo** (⌘Z) / **Redo** (⇧⌘Z) in this tab.

A layer stays one layer when its groups change: added to in a cue it
tracked into, it takes the tracked layer's place and keeps its values;
added to where it is set, the change carries on to the cues it tracks into.

## Seeing it on the rig

In **Design** mode, selecting a cue — or a layer in it — plays that cue on
the rig, as the Programming tab plays its scene, and every change you make
to it shows at once. Selecting another cue moves the preview there (a layer
both cues share keeps running: no re-fade). Selecting the list, nothing, or
leaving the tab stops it. **Blind** mutes it like anything else, and the
status-bar power estimate counts it.

In Design, **what you are editing is what shows** — also while a cue list
is **running for real** (footer GO, a Virtual Console Cue List), its own or
another. The cue you are on takes precedence on every channel it sets: a
dimmer you edit down shows even over a running cue at full. The running
list keeps going underneath, and the top right says so (*"▶ On the rig:
1 One, over Full Show running"*). Stop the preview (select the list, or
nothing, or leave the tab) and the stage is what really runs again: the
running list's current cue, as it started. Nothing a chaser or the Virtual
Console started is stopped. An effect (EFX, matrix) in the cue mixes with
what runs as it normally would.

In **Live** (Operate) mode the running cue list owns the stage: selecting a
cue doesn't touch it, and your changes reach it when the cue plays (on GO).

## What wins

Inside a cue, for each layer and attribute, the first that sets it:

1. the effect's own looks (inside that effect only)
2. the layer's looks
3. the cue's looks
4. included cues
5. what tracked in from the cue before

**This cue beats earlier cues; inside a cue the nearer one wins.**

## Tracking

A cue list is **Non-tracking** (each cue plays only what it holds — how an
imported chaser plays) or **Tracking** (select the list to change it). In a
tracking list each cue's **mode** says what it starts from:

| Mode | Starts from | Passes on |
|---|---|---|
| **Track** | the cue before | everything on stage |
| **Block** | nothing | everything on stage |
| **Cue-only** | the cue before | what it started from — its changes don't pass on |

Set the cue's mode on its page or with right-click → **Mode**. The tree
marks a Block cue ⊘ and a Cue-only cue ◐.

- **Release.** A layer marked **Release** (the check box on its page) leaves
  when its cue is left: back to what the **base cue** gives it, else dark.
  On a grey (carried-in) layer, ticking it makes the layer this cue's own
  first. Released layers drop out even of a list that keeps everything else.
- **Base cue.** Pick it on the list's page, or tick *"The list's base cue"*
  on a cue (or right-click a cue → Make the base cue). Marked ■ base.
- **Includes.** The cue page lists the cues a cue includes: **+** adds one
  (one that would make a loop — a cue that already plays from this one — is
  greyed and says why), **−** takes one out, **↑ ↓** reorder them (the
  later one wins). Or drag a cue onto another with **⌥**, or right-click →
  Include cue. An included cue's output comes in as if you stood in it,
  and follows it when it changes.
- **Next cue receives** on a cue's page says what the cue after it starts
  from: each layer with its values, what goes back to the base, what a
  release leaves dark — or *"nothing"* when the next cue is a Block cue.

A change re-plays the cue, the cues after it that track from it up to a
Block cue, and every cue that includes one of them; the top right names
them.

### Changing a tracked value

A grey value came from an earlier cue. Its **⋯** offers two things:

- **Set in this cue** — the value changes here (the layer becomes this
  cue's own) and from here on;
- **Update cue 0.0.3 Announcer** — the value changes *where it was set*, so
  every cue it tracks into follows.

### Record and duplicate

**Record** (top, or right-click a cue → Record as new cue) freezes what the
selected cue puts on stage — its own layers, what tracked in, what it
includes — into a new **Block** cue right after it, every layer its own.
**Duplicate cue** copies a cue's own content (not what tracks into it).
Both are numbered like + Cue.

## Playing it

Each cue list plays as a chaser of the same name, filed under
`Productions/<production>`. Pick it in a Virtual Console Cue List, GO it from
the footer, or fire its cues from QLab by number. The scenes and collections
it plays are hidden and locked: change them here. To take a layer out of the
tree's hands, right-click it → **Detach to plain scene**.

A show saved with a Show Tree still opens and plays in a build without one:
the chaser and its cues are ordinary functions.

## Importing a chaser

**Import chaser…** turns a chaser you already have into a cue list, in a new
production; the chaser itself is not touched.

- Each step becomes a cue (its number from the step, or from a name like
  `0.0.4-SHOUT`; fades, hold and note kept).
- A scene of groups and palettes becomes a layer; a step's collection used
  inside another step becomes an **include**; anything else plays as it is.
- The list is Non-tracking, so it plays exactly as the chaser did. Where two
  scenes in one cue set the same group (the chaser ran both and the brighter
  intensity won), the import says so: the cue list keeps the later one.

## The command line

**Command** (top) shows a line for typing what you would say; it calls the
same operations as the mouse, and every command is **one undo step** (a
command that fails part way changes nothing). **Tab** completes names from
the Library, **↑ ↓** recall earlier commands, and the line under it lists
the words:

| Type | Does |
|---|---|
| `cue 1.0.1 "Verse"` | selects cue 1.0.1, or makes it |
| `cue + "Verse"` | a new cue after the selected one |
| `track` · `block` · `cueonly` · `default` · `base` | the selected cue's mode, or makes it the base (can follow `cue …`) |
| `Towers colour Blue dimmer 45` | a layer on Towers (or the one there), then its looks |
| `Towers USTs dimmer Full` | a layer each |
| `Towers+USTs dimmer Full` | one layer on both |
| `cue look colour Red` | a cue look |
| `Steps effect Sinelon colour Red` | an effect, and its own colour |
| `Towers gobo none` · `Towers gobo clear` | off here · inherit again |
| `Steps colour #33aa55` · `dimmer 45` | a one-off value |
| `release Towers` | Release on (or off) |
| `include 0.0.3` | include a cue (again: take it out) |
| `record` · `dup` · `undo` · `redo` | as the buttons |
| `list "01-Opener"` | a new (tracking) cue list in the production |

Names match whole words and ignore case; quote a name with spaces if Tab
doesn't finish it (`"AT W ALL" dimmer Full`).

## One-off values

⋯ → **One-off level…** or **One-off colour…** sets a value without making a
palette for it. It shows in **Recent**; right-click it there → **Make
reusable…** to name it and file it with your palettes.

## Gobos by name

A gobo look can mean *the same gobo* on fixtures with different wheels.
Open it in the Look Editor (✎), pick the gobo on one type's wheel
(**Wheel of** chooses the type): the look takes that gobo's name ("Dots"),
and every other type plays **its own** gobo of that name. A type whose
wheel has none ignores the look, and the row's *"applies to"* counts only
the ones that have it. The editor lists, per type, what it plays.

- Pick a slot on another type's wheel and it is kept for that type — useful
  when the name differs ("Dot Ring" on one, "Dots" on the other).
- **Gobo name** changes the name. A type finds its gobo by, in order:
  1. a slot **picked** for that type;
  2. the **same name** — case, spaces, `-` and `_` don't matter. A gobo's
     picture names it too, with its folder: `ClayPaky/gobo00006` (a picture
     name only ever matches that picture);
  3. **singular or plural**, without the word "gobo" or numbers: "Dot",
     "Dots" and "Gobo 3 Dots" are the same;
  4. the **one** slot whose name starts with or contains it ("Breakup"
     finds "Breakup Rotating").

  If a step finds **more than one** gobo on a type ("Dots" with only "Dots
  Shake" and "Dots Rotating"), it isn't guessed: that type ignores the
  look, the row says *"ambiguous: pick per type…"* (hover for which), and
  the link opens the Look Editor to pick it on that type's wheel. The same
  name twice on one wheel (an "Open", say) counts as one gobo.
- A gobo look that was never edited this way plays one DMX value on every
  fixture, as before.
