# Refine

Changing a cue from the stage, with the controller: where its lights
point, their zoom, focus, colour and gobo — the cue's own values, seen on
the real rig as you move them.

Refine is not focusing. [Focus calls](focus-calls.md) correct the rig's
calibration for a venue; Refine changes what a **cue** does. The
controller is laid out the same way in both, so it only has to be learnt
once.

## Starting

In the [Show Tree](show-tree.md), in Design or Operate:

- a cue's page: **Refine on the controller…**, or
- right-click a layer: **Refine on the controller…** (it starts on that
  layer).

In Design the cue is previewed on the rig. In Operate the show carries on
as it is running (see [Refine during the show](#refine-during-the-show)).
The Refine panel shows the cue, the layer, the scope, what the D-pad sets,
and what is pending.

In Build's [Workbench](build.md#the-workbench):

- **Refine…** in the Workbench's header, for what is loaded: a Show Tree
  cue (from its selected layer) or a Build scene, or
- right-click a cue's layer, a scene, or a scene's group or fixture row:
  **Refine on the controller…** (it starts on that row).

The controller works exactly the same on a Build scene; see
[On a Build scene](#on-a-build-scene).

## Scope: layer, group, fixture

**Y** goes Layer → Group → Fixture → Layer.

- **Layer** — the layer's own look changes, for all of its fixtures.
  A position keeps its fanning: the whole fan moves.
- **Group** — one of the layer's groups (only where the layer has more
  than one). A move is an **offset** from each fixture's own position, so
  a fan inside the group stays a fan: it is kept as a position exception
  on each of the group's fixtures. Zoom, focus, colour and gobo are kept
  as one exception for the group.
- **Fixture** — one fixture. Kept as an **exception**: a
  [child layer](show-tree.md) of that layer on just that fixture, with the
  value as its own look. The layer still lights it with everything else.

The **triggers** step the cue's layers (a new layer starts at Layer
scope); the **bumpers** step the groups or fixtures. A bumper at Layer
scope picks the first fixture.

## Changing values

| Do | Controller | Keyboard |
|---|---|---|
| Next / previous layer | RT / LT | PgDn / PgUp |
| Next / previous fixture (or group) | RB / LB | ] / [ |
| Scope: layer → group → fixture | Y | G |
| Move (coarse, fine) | Left stick, right stick | Arrows (⇧: 5°) |
| Zoom wider / narrower | D-pad up / down | . / , |
| Focus nearer / farther | D-pad left / right | ; / ' |
| D-pad page: beam / colour / gobo | LS (left stick click) | P |
| Keep in the cue | A | Return |
| Back to the cue | B | Backspace |
| Clear the exception | X | 0 |
| Undo | View | ⌘Z |
| Where am I? | RS (right stick click) | W |
| Leave | Menu | Esc |

On the **colour** and **gobo** pages, D-pad left and right step through
the show's colour or gobo palettes and say the name of each.

What you change is **pending**: the rig shows it at once, but the cue is
not changed until **A**. Then everything pending is kept as **one** Show
Tree step (⌘Z or View undoes it). **B** drops what is pending and the rig
shows the cue again. Leaving with something pending asks whether to keep
it.

**X** at Group or Fixture scope takes the exception off: the child layer
loses what it set on this page (position, zoom and focus on the beam
page), and goes when nothing is left on it. The fixture is lit by its
layer again.

Positions are kept in whole degrees, as the Show Tree's position values
are.

A layer aimed at a stage target (an Aim position) cannot be moved by
degrees as a whole: it would stop following its target. Move the target
in the Lighting Studio, or pick one fixture (Y) and give it a position of
its own.

## On a Build scene

A Build scene has no layers of its own, so Refine takes the nearest
thing to each part of a cue:

- **The layer is the scene.** With a collection loaded, the triggers step
  its scenes; with one scene, they stay on it.
- **The groups are the scene's group rows** (and its loose fixtures, as
  one).
- **Layer scope edits the scene's looks: its palettes.** A position, zoom
  or focus palette has its value moved in place, as the Look Editor does —
  a palette other scenes use changes there too. With no palette for it,
  the scene's own values on every fixture. On the colour or gobo page, the
  palette picked takes the place of the scene's.
- **An exception** (Group or Fixture scope) leaves those fixtures out of
  the attribute's palettes and gives them the scene's own values — the
  fixture row's ▣ count shows them. **X** puts them back in the palette.
- Each keep is one step of Build's undo (⌘Z, or View on the controller).

## Where am I?

The same as [Focus calls](focus-calls.md#telling-the-operator-where-they-are):

- **The lights.** At Group or Fixture scope, what is picked keeps the
  cue's level and the rest of the layer dims to 15% — only the fixtures
  the cue has brighter than that; one already lower is never raised. On
  every change what is picked blinks three times.
- **The voice.** "AT spots, layer." "AT spots, fixture 3 of 8."
  "DS wash, group 2 of 3." "Kept." "Back to the cue." **Where am I?**
  adds the cue: "Cue 4, AT spots, fixture 3 of 8."

## Refine during the show

Refine works in Operate too, and switching between Design and Operate
does not end it. In Operate the cue list runs on as it is: what you move
is shown on the rig at once, and **A** keeps it in the cue as in Design.
The running cue plays it from its **next GO**; until then Refine holds what
was kept on the rig, and the voice says "Kept. It plays from the next GO."
Leaving Refine lets go of that hold. Back in Design, the cue is previewed
with everything kept.
