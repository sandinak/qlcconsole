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
| **Person** | A standing subject, aimed at chest height by default | Crosshair over a lofted human silhouette |
| **Drum Kit** | A low, wide subject | Crosshair over a footprint rectangle, aimed lower by default |
| **Area** | A region of deck, not a point | A dashed-outline footprint rectangle |
| **Structure** | Bound to a truss/platform/steps/tower — its position *follows* whatever it's bound to | Crosshair, position always live |

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

In the rig view itself, two overlays answer the same question spatially
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
| **Kind** | What a target represents — None, Person, Drum Kit, Structure, or Area — governing its default aim height and how it's drawn. |
| **Aim palette** | A palette that names a target instead of carrying fixed DMX values; resolved per-fixture through the Aim Solver. |
| **Subject height** | The follow-spot notion of a person's chest height above the deck, overriding a target's stored Z in follow-spot mode. |
| **Reachable / margin** | Whether a fixture's calibrated pan/tilt range can actually get to a target, and how much spare travel it has if so. |
| **Light footprint** | Everywhere a fixture's full pan/tilt sweep could land — as opposed to where it's currently pointed. |
| **Bound target** | A Structure-kind target whose position is recomputed live from whatever it's bound to. |
