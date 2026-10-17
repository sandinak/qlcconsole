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
    **⋯** on a row sets it here (any palette of that kind, recent ones first,
    or a one-off level or colour), turns it **off here** (None), or clears it
    so it inherits again. *"applies to 4 of 8"* means only 4 of the layer's
    fixtures have that attribute; the rest ignore it.
- **Right — the Library.** **Recent** (what you just used, one-off values
  included), **Pinned** (right-click → Pin), and **All**: groups, palettes by
  kind, and functions to play as they are. One filter box narrows them all.

## Building a cue list

1. **+ Production**, then **+ Cue list**. A list named with a leading number
   ("01-Everybody Scream") numbers its first cue 1.0.0.
2. **+ Cue** adds one after the selected cue, numbered for you: the next
   whole number if it's free, else a point cue (0.0.4 and 0.0.5 taken →
   0.0.4.5). Cues are **never renumbered** — show control fires them by
   number.
3. Drag **groups** from the Library onto the cue: one layer each (or select
   them and **+ Layer**). Fixtures dragged together make one layer.
4. Drag **palettes** onto a layer to set them, or onto the **cue** to set a
   *cue look* — it reaches every layer of the cue that doesn't set that
   attribute itself. Before you let go, the line at the top right says what
   the drop will do: *"Blue → cue look on 1 Preshow: reaches 2 of 3 layers"*.
5. A **function** (an EFX, an RGB Matrix, a scene with channel values) dropped
   on a cue plays as it is, as a layer of its own (▣).

Every change is one step of **Undo** (⌘Z) / **Redo** (⇧⌘Z) in this tab.

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

A layer marked **Release** leaves when its cue is left: back to what the
**base cue** gives it (right-click a cue → Make the base cue), else dark.
**Include cue** (right-click a cue) brings another cue's whole look into it,
and follows that cue when it changes.

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

## One-off values

⋯ → **One-off level…** or **One-off colour…** sets a value without making a
palette for it. It shows in **Recent**; right-click it there → **Make
reusable…** to name it and file it with your palettes.
