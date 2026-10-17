# Stage Targets &amp; Aim Solving

## Why

Pointing a moving head at something specific — a mark, a drum kit, a
person — normally means dialing in raw pan/tilt DMX by hand, separately for
every fixture, because every fixture has a different rig position and mount
orientation. Move the truss, or re-hang the fixture, and you redo all of it.

This fork lets you mark **where** something is in 3D stage space once, as a
**Stage Target**, and have each fixture's own rig geometry decide **how** to
point there. Move the target — or the structure it's bound to — and every
look that aims at it re-solves automatically.

## Target kinds

| Kind | Represents | Drawn as |
|---|---|---|
| **None** (a plain mark) | A bare point | Amber crosshair |
| **Person** | A standing subject of a given **Height** (5'9" unless set), drawn that tall and aimed at the chest — 80% of it | Crosshair over a lofted human silhouette |
| **Drum Kit** | A low, wide subject | Crosshair over a footprint rectangle, aimed lower by default |
| **Area** | A region of deck, not a point | A dashed-outline footprint rectangle |
| **Structure** | Bound to a truss/platform/steps/tower — its position *follows* whatever it's bound to | Crosshair, position always live |
| **Line** | A curve in space — every light aimed at it gets its **own** point on it, evenly spaced | The curve, square handles at its ends, round ones at its mid points, and a crosshair at its centre |

A target being **aimed at** by the currently-open scene shows fading
concentric rings around its crosshair — that's a separate, temporary fact
from the target merely existing (a target is always drawn once created).

## Creating and binding a target

Targets are created from the **2D Monitor plot** (Lighting Studio's Top
view or the standalone Monitor window):

1. Right-click the floor, or a structure, and choose **"Target Here"** (or
   **"Person Here"** for a ready-made Person target).
2. The **Edit Target** dialog opens: name, X/Y/Z, a **Subject** kind
   (None/Person/Drum Kit/Structure/Area), a **Follows** structure picker
   (for a Structure-kind target), a per-target aim-height override, size
   (for Area/Drum Kit), and color.
3. An Aim palette named after the target is created for you automatically
   (unless one already exists) — an **Aim** palette, not a Pan/Tilt one,
   specifically because Pan/Tilt carries fixed angles and won't follow the
   target if it moves later.

To actually point fixtures there: in either the general Palette editor or
the Programming tab's [Look Editor](programming-tab.md#the-inline-look-editor),
pick the target from the Aim palette's target combo, then drag that palette
onto a scene as a look — see [Programming Tab](programming-tab.md). Every
aimable fixture the scene reaches (directly or through a group) points at
the target when the look resolves.

Deleting a target doesn't touch the palettes that reference it — any Aim
look using a deleted target just stops resolving, and the fixtures hold
their last position. The delete confirmation names every scene that would
be affected before you commit.

## Line targets

A **Line** spreads lights out instead of converging them: every head an Aim
look points at a line gets its own point on it, the first light on one end,
the last on the other and the rest evenly between — evenly *along the
curve*, however it bends.

**Making one:** right-click the floor → **Target Line Here** (or the
Lighting Studio's **Add > Add Target Line**): a straight 4 m line across
the stage, centred where you clicked. An existing target becomes one by
choosing **Line** as its Subject in the Edit Target dialog.

**Shaping it** (Lighting Studio, Top, Front or Side view, in Design mode):

| Do this | And |
|---|---|
| Drag an **end** (square handle) | Moves that end: extend, shorten, swing it round |
| **⌥-drag** an end | Extends or shortens it along the line's own direction only |
| **Double-click the line** | Adds a point there, on the curve, so nothing jumps |
| Drag a **mid point** (round handle) | Bends the line through it |
| **Double-click a mid point** | Takes it out (right-click > **Remove Point** too) |
| Drag the **line itself** or its centre | Moves the whole line, its shape with it |
| **⇧-drag** a handle | Keeps the drag level or upright on screen |
| ⌘Z | Undoes the last of any of these |

From above, a handle keeps its height above whatever is under it; the
**Front** and **Side** views drag it up and down — so between the views a
line can curve in any direction. Double-click the centre crosshair (or
right-click > Edit) for the target's dialog.

**Which light goes where:** by where the lights hang, measured along the
line — the stage-left head takes the stage-left end — so a full spread never
crosses beams by accident. The 2D plot's aim traces and the Rig Overview's
beams each go to their own point.

The target's own point — what a follow spot, an effect pointed at the
target, or an older qlcconsole build sees — is the line's **centre**.

### Steps as aim surfaces

A flight of **Steps** (Lighting Studio: Add > Add Steps, or a platform
whose Shape is Steps) is a real surface for aiming, tread by tread, not
one box:

- A target **bound to the steps** aims at the tread under the middle of
  the flight — and, like any bound target, follows the steps if they're
  moved, restacked or re-heightened.
- A target that stands somewhere on the steps — a Person, say — is aimed
  at its height **above the tread it's on**, not above the floor or the
  top of the flight. The same goes for a follow-spot subject walking up
  them.
- A beam that passes its aim point lands on the tread it actually hits.

## The Aim Solver

For each fixture, the solver starts from a cached **Frame**: its rigged
world position, whether it's floor-mounted or hung (which flips which way
"home" tilt points), its pan-zero facing direction, calibration offsets, and
its physical pan/tilt range.

Given a target point, it computes the bearing and elevation to it, converts
that into a raw pan/tilt pair, and — because a yoke can usually reach the
same point two different ways (swing-and-tilt-forward, or the long way
round and tilt back through home) — picks whichever solution actually stays
in range, preferring the one that doesn't need to swing all the way around.
It reports **margin**: how much spare travel is left before the nearest
limit. Negative margin means the fixture physically can't get there at all.

There's no line-of-sight or obstruction check — reachability is purely a
question of pan/tilt range against the fixture's own calibrated limits.

## Reachability

An Aim look's Targets panel in the Programming tab flags, in red, exactly
which fixtures can't reach their assigned target — worse than the ordinary
amber "nothing covers this channel" warning, because the fixture isn't
idle, it's stuck at its limit pointing somewhere else.

In the Lighting Studio itself, two overlays answer the same question spatially
(Show menu, or a fixture/structure's own context menu):

- **Light footprint** — everywhere a selected head's full pan/tilt sweep
  *can* reach, shaded and named; layered on top, a hatched pattern shows
  where it's *currently* pointed. If a head can reach nothing at all from
  where it's rigged, a plain-language note explains why (e.g. standing
  upright with a tilt range that never dips below level) instead of just
  drawing nothing.
- **"Where selected heads CANNOT reach"** — the inverse: deck area none of
  the selected heads can aim at, at a configurable subject height.

## Follow-spot behavior

Follow-spot is an **Effect**-type palette (`followspot.js`) rather than a
special mode. Two things change when a scene combines a follow-spot effect
with an Aim look:

- The Aim look's point is no longer the target's stored Z, but subject
  height (chest height by default) above whatever's actually under the
  target right now — so it tracks a person walking across a riser, not just
  across the flat floor.
- **The beam's size moves the aim.** On a Person, each head aims at the
  chest while its beam is narrow; as its zoom opens (or its iris closes
  less), it aims lower, so the top of the spot stays just over the head —
  covering more of them without washing the wall behind — down to the waist
  at most. Each head works from its own throw and its own beam, read live
  from its zoom and iris channels, so a near head and a far one land the
  same way. The Lighting Studio and the Rig Overview draw the same thing.
- Movement stays **target-converged**: the effect moves the shared aim
  point, and the scene's Aim look re-aims every head at it — which is what
  keeps multiple heads genuinely converged on the same spot, rather than
  each flying its own per-fixture angle steps (which diverge from different
  rig positions). Per-fixture "free-fly" from a joystick is only used when
  no Aim look is present at all.

A beam does **not** stop at the point it's aimed at — it passes through and
lands on whatever real surface is actually behind it (the floor, or a
platform), fading out on its own if it hits nothing within range. Light
doesn't stop just because that's where you pointed it.

## Terminology

| Term | Meaning |
|---|---|
| **Stage Target** | A named 3D point with a kind, optional structure binding, optional footprint, and color. |
| **Kind** | What a target represents — None, Person, Drum Kit, Structure, Area or Line — governing its default aim height and how it's drawn. |
| **Line** | A target that is a curve: each light aimed at it gets its own, evenly spaced point along it. |
| **Aim palette** | A palette that names a target instead of carrying fixed DMX values; resolved per-fixture through the Aim Solver. |
| **Height** (Person) | How tall a person is, standing on whatever is under them: the figure is drawn that tall and light is aimed at 80% of it. A person without one is 5'9". |
| **Follow-spot aim height (chest)** | Where a follow spot aims above the deck — a chest, not how tall anyone is — overriding a target's stored Z in follow-spot mode. It never sizes a person: a Person is aimed at 80% of its own Height. (Called "subject height" before.) |
| **Reachable / margin** | Whether a fixture's calibrated pan/tilt range can actually get to a target, and how much spare travel it has if so. |
| **Light footprint** | Everywhere a fixture's full pan/tilt sweep could land — as opposed to where it's currently pointed. |
| **Bound target** | A Structure-kind target whose position is recomputed live from whatever it's bound to. |
