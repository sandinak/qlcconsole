# Area cover (board Q209)

An Aim look at an **Area** target (a sized `StageTarget`, Kind::Area) now
*covers* the area. Before, every light pointed at the area's centre at the
area's own Z: a pile of beams in the middle and dark corners, and the
footprint was only ever drawn. The user's answer on the board (2026-11-03):

> Cover the area: split it into one zone per light by where each hangs
> (low/front lights take the front ranks, high/back lights take the back
> ranks on the risers), aim each zone at subject height above the deck or
> riser actually under it, then show coverage and any gaps on the plan, with
> a Show Review note when a gap is left.

No new model: it is the Aim look and the Area target that already exist,
solved in `engine/src/areacover.{h,cpp}` (next to `linespread`), used by
everything that already resolves an Aim look's points (QLCPalette's DMX,
`AimSolver::sceneAims`, the rig view's aim trace) and drawn in the plan view.

## Zones

The members are the aim members (`AimSolver::aimMembers`: each arm of a
multi-arm fixture is its own). The area is split into **one cell per member**,
in rows from downstage to upstage, each row split evenly across the stage.

- **How many rows.** For each row count r (1..n), the n members are dealt
  out as evenly as possible (the extra ones to the downstage rows), and the
  grid is scored on two things: how square its cells are for this area
  (`|ln(cellWidth / cellDepth)|`), and how well its shape (columns / rows)
  matches the shape the lights hang in (distinct side positions / distinct
  depth levels, clustered at 0.3 m), weighted 1.5. The lowest score wins;
  ties go to fewer rows. A regular rig gets its own shape (a 3 × 2 truss
  rig on a 6 × 3 m area: 3 × 2 cells; two towers of three shelves: 2 × 3);
  a ragged one gets near-square cells.
- **Who takes which cell.** Every member has a *depth key*, `z − y` (metres
  up, minus metres downstage): low and downstage lights have the lowest
  keys, high and upstage ones the highest. Each member gets a place in the
  unit square: its x and its depth key, each scaled 0..1 over the members.
  Each cell has one too: across the area, and from the downstage edge (0)
  to the upstage edge (1). Members and cells are matched one to one so the
  total squared distance is least (the Hungarian method; n is a few dozen
  at most). So the stage-right light takes a stage-right cell and the low
  shelf the front rank; a least-squares match never crosses two pairs in
  that square, so a regular rig's beams don't cross in plan. It also keeps
  a tie sensible: eight two-arm beams all on one riser behind a row of
  front washes take the back two ranks column by column, the two arms of a
  bar one in front of the other, where a sort would have dealt them out
  stage right first and crossed half of them over.
- **Height.** Each member aims at its cell's centre, at **subject height**
  above whatever is under that point: `platformHeightAt(x, y)` (deck,
  riser, step tread) plus the target's aim-height override if it has one,
  else a person's 1.4 m. Not the area's own Z.
- **Points stay points.** An Area with no size (or any other kind) aims as
  before; a zero-size area is a point aim at its centre.

## Coverage

Each member's beam is a cone from its origin (the arm's own) along its aim
with the angle `RigHeads::beamAngle()` reads: the zoom channel's value from
the scene (else the fixture's), else the definition's lens degrees, else
**20°** when the definition gives no lens. The area is sampled on a grid
(about 48 across) at subject height above the surface under each sample; a
sample is lit when it lies inside any cone. The covered fraction is lit
samples over all samples. The gap is named by where most of its samples
fall on a 3 × 3 split (upstage / centre / downstage × stage right / centre
/ stage left; +X is stage left).

The plan view (the Show Tree layer's Position rows, the formation editor)
draws the area outline, each zone, each beam's footprint at subject height
(the cone cut by the horizontal plane at its aim point), and hatches the
gaps. Caption: "Covers 86% of Chorus · gap upstage left".

## Show Review: MOV-13

"An area left partly dark" (Movement, Note): a cue whose Aim look at an area
leaves more than 10% of it unlit. It names the cue, the area, how much is
covered and where the gap is, and says what fixes it: zoom wider, add a
light, or make the area smaller.

## Old shows

No file-format change. Existing area aims now spread over their area and
aim at subject height, so their DMX changes: CHANGELOG ▸ Changed.

## Cost

Zones are a sort of n members and a few dozen grid scores, done once per
Aim-look expansion like a line's places (not per frame unless the look is
expanded per frame, as a line's already is). Coverage sampling (48 × ~18
samples × n cones) runs only for the plan view and Show Review.
