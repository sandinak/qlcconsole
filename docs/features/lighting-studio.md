# Lighting Studio (the rig view)

## Why

Stock QLC+ only has a flat 2D plot — fixture icons on a grid. That can't
answer "what does this rig actually look like assembled," "is this fixture
really sitting on top of that platform," or "can this spot even reach that
mark from where it's hung." Lighting Studio is this fork's answer: a
dedicated structural view of the physical rig — trusses, towers, platforms,
pipes, stands — with fixtures mounted onto it in real 3D space, viewable
from Top, Front, Side, or a free 3D angle.

It's built and iterated against a real test rig over many rounds, each
driven by a specific real-world complaint and backed by a regression test —
the recurring lesson threaded through that history: *a mounting property
isn't really implemented until the model derives from it, the UI can set it,
**and** a test drives it end to end.*

## Opening it

- **View → Lighting Studio** (⇧⌘3) opens the 2D plot with the Layers panel,
  where you build the rig and place structures/fixtures.
- **Double-click any structure** (truss, tower, pipe, stand, platform) opens
  its own dedicated editor — geometry fields, a live elevation preview, and
  everything mounted on it.
- **View → Rig Overview…** (⇧⌘R) opens a read-only, freely-rotatable 3D
  view of the *whole* rig at once — the fastest way to sanity-check a build.

## The four planes

| Plane | What it shows | Editable? |
|---|---|---|
| **Top** | Looking straight down — X/Y layout | Yes — drag fixtures/structures |
| **Front** | Elevation from downstage — X/Z | Yes |
| **Side** | Elevation from the wing — Y/Z | Yes |
| **Angled** | A free axonometric 3D view | View only — a screen point in a 3D projection has no single, honest 3D position to drag to |

### Top view — the plot you already know

![Lighting Studio Top view: platforms, trusses, targets, and aim lines](../images/lighting-studio-top-view.png)

Structures and fixtures on the familiar flat plot, plus (fork-specific) the
dashed and solid lines you can see radiating from fixtures to **Stage
Targets** — see [Stage Targets](stage-targets.md).

### Front / Side — elevation, and count badges

![Lighting Studio Front view showing numbered count badges on crowded risers](../images/lighting-studio-front-view-badges.png)

An elevation view is where a rig's vertical assembly actually becomes
readable — towers standing up, trusses at their hung height, risers stacked.
It's also where a structure's own fixtures can visually bury it: a wall of
fixture icons on a truss makes the truss itself unclickable. When a
structure's mounted fixtures cover more than 40% of its on-screen footprint
*in the current view*, they collapse into a numbered blue **count badge**
(seen above on several risers) instead — click it to reveal them again. It's
computed per structure, per view: the same rig can show individual fixtures
from the Top but a badge from the Front.

### Angled — the free 3D view

![Rig Overview: the whole rig in a free 45° 3D view](../images/lighting-studio-rig-overview-3d.png)

Drag to swing the camera, shift-drag to pan, wheel to zoom. Toggle **Live**
to shade fixtures by their actual current output, and **Beams** to draw
light cones from anything currently emitting. This is the same rendering a
per-structure editor's own Angled tab uses, just showing everything at once.

## Stage structures

Five structure types, each its own object with its own geometry:

- **Truss** — a horizontal or vertical run, drawn as an open lattice (four
  chords, end frames, alternating diagonal bracing) rather than a solid box
  — both because that's what a truss looks like, and so you can see
  anything rigged inside or behind one. A **bar** (a short cross-piece hung
  on a parent truss — e.g. a batten across the front of an overhead truss)
  is internally just a child truss, so mounting, rendering, grouping, and
  deletion all reuse the same machinery.
- **Tower** — a vertical box with **shelves** at set heights; fixtures mount
  on a shelf via local coordinates, or at the very top or bottom of the
  tower.
- **Pipe** — covers both a **boom** (vertical pipe — on a stand, hung from a
  truss, or free-standing) and a **house electric** (horizontal pipe, same
  object with an orientation flag). Fixtures mount up it at a height and
  facing angle.
- **Stand** — a placeable base a pipe stands on; the pipe follows if the
  stand moves.
- **StagePlatform** — risers, decks, steps. Its top can be Solid, Clear
  (translucent, so fixtures mounted inside show through), or Open (no top
  face at all). Platforms can stack on top of each other, deriving their
  base height automatically, and collide realistically so dragging one
  doesn't scramble the pile underneath it.

Add one from the **Add** menu on the toolbar or the Layers panel; each
type's own editor exposes width/depth/height, position, and (for
pipes/booms) run angle.

## Mounting a fixture

![A truss's own editor, with a mounted fixture's properties panel open](../images/lighting-studio-truss-editor.png)

Double-clicking a structure opens its own **Lighting Studio Editor** — the
truss shown above has one fixture on it (`LED Movinghead+Circle`). Geometry
fields on the left, a live elevation preview in the middle (drag a fixture
in it to reposition), and a **Fixtures on this object** list.

Click a fixture in that list (or in the preview) to see its own mount panel:

![A mounted fixture's rig properties: mount, colour, orientation, face, placement, angle, tilt](../images/lighting-studio-fixture-mount-properties.png)

- **Mounted on** — which structure and where along/on it.
- **Orientation** — Upright / Inverted, independent of how it's physically
  bolted on.
- **Face** — which plane the fixture is drawn/edited in (Top / Front /
  Side) — this has to agree with *how* it's actually mounted, or the
  fixture looks right from one angle and wrong from another (see the
  callout below).
- **Placement** — **On the surface** (seated on/against the structure) or
  **Inside** (centred in its volume — for something rigged between truss
  chords, or under a Clear/Open platform top, so it reads correctly without
  needing to be visually "seated" on anything).
- **Angle / Tilt** — fine orientation.

A fixture standing on a riser/deck, a truss face, a tower shelf, or a pipe
each get their own mount fields, but only ever **one** primary mount at a
time — you can't leave a stale mount behind when you re-attach a fixture
somewhere else.

> **A fixture correct from Top but wrong from Front usually means Face is
> stale.** A deck-mounted fixture in particular needs Face set to "Top
> surface" — if it's left at its default ("Front / downstage", meant for a
> fixture bolted to a vertical face), it'll show right from above but
> appear to float or sink through the deck in an elevation view. As of this
> fork, a deck mount left at that default is corrected automatically the
> next time the workspace loads; setting it explicitly in this panel avoids
> the issue entirely.

## Locks

There are several independent lock layers, and it matters which one you're
setting:

| Lock | Set from | Freezes |
|---|---|---|
| **Global layout lock** | The 🔒 "Edit Plot" toolbar button | The whole plot at once |
| **Per-layer lock** | The Layers panel | Everything on that layer |
| **Per-group lock ("position lock")** | The Layers panel / group properties | Every member's position (locking a parent group locks its children too) |
| **Per-object lock** | Right-click a truss/platform/pipe/stand/tower → "Lock Position", or bulk from the Layers panel | That one structure — and, following it, every fixture mounted on it |

Fixtures don't carry a lock of their own — a fixture's movability always
comes from whatever it's mounted on (plus layer/group/layout locks above
it). **Locking a truss locks the fixtures riding it, everywhere** — the main
2D plot and every per-object Lighting Studio editor honor it, not just
whichever view you set the lock from.

The 🔒/🔓 toggle inside a per-object editor is a separate, session-only
convenience — it starts locked every time the editor opens, purely so a
stray drag can't move something while you're just looking. It's independent
of, and doesn't override, the real locks above.

## Beams, footprint, and coverage

When a fixture is live (Design or Operate mode), the rig view shows what
it's actually doing, not just where it sits:

- **Live colour** — a fixture shades by its real current output, reading
  every color-primary channel the fixture defines (RGB/W/Amber/UV/CMY and
  more), not just the common ones. An unlit fixture still reads as an
  object in the room at low ambient brightness, not a flat black shape.
- **Beams** — a moving head or any other emitting fixture (pars, washes,
  bars too, not just movers) draws a light cone from itself to wherever it
  actually lands — the floor, or the nearest platform top in its path —
  with a landing pool shaped like the true conic section where the beam
  meets that surface.
- **Light footprint** (the Show menu / a structure or fixture's context
  menu) — toggles an overlay of **everywhere a selected moving head's
  pan/tilt range can possibly reach**, not just where it's currently
  pointed. A head standing upright with a narrow tilt range can have a
  "coverage" that's nothing at all — and if so, the overlay tells you why in
  plain language (e.g. *"can light nothing here: standing upright, 190° of
  tilt reaches 5° below level — 35 m out from 3.0 m up"*) rather than just
  silently drawing an empty overlay.
- **"Where selected heads CANNOT reach"** — the complementary overlay: deck
  area none of the selected heads can aim at, shaded red.

Both overlays are visualization aids only — they aren't saved with the
workspace.

## Fixture outlines and overlap warnings

Two small but load-bearing visual cues:

- **Outline contrast** — a fixture's body edge is brightened by a fixed
  amount rather than scaled multiplicatively, so a fixture with a declared
  black housing (common — plenty of real fixtures *are* black) is still
  clearly outlined against the dark canvas instead of nearly disappearing
  into it.
- **Overlap warnings** — if two fixtures' physical bodies actually occupy
  the same real-world space (two heads stacked at the same truss offset, or
  a deck fixture dragged into another's footprint), both draw with a flat
  warning-red outline instead of their normal contrast tint — deliberately
  independent of either fixture's own color, so it reads as a warning
  rather than an odd shade.

## Terminology

| Term | Meaning |
|---|---|
| **Plane** | Which of Top/Front/Side/Angled the view is currently showing. |
| **Bar** | A short cross-piece truss hung on a parent truss — modeled as a child truss, not a separate object type. |
| **Placement: Inside** | A fixture centred in its host structure's volume rather than seated on its face/top — for fixtures rigged inside a truss or under a see-through platform top. |
| **Face** | Which plane (Top/Front/Side) a mounted fixture is drawn and edited in — has to agree with how it's actually attached. |
| **Position lock** | A group-level lock that freezes every member's position but leaves them selectable. |
| **Count badge** | The numbered circle a structure's own fixtures collapse into when they'd otherwise cover more than 40% of it on screen. |
| **Light footprint / coverage** | Everywhere a fixture's full pan/tilt range can reach — distinct from where it's currently pointed. |
| **Rig Overview** | The read-only, whole-rig 3D window (View → Rig Overview…). |
