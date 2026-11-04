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
- **Exact ties go to coverage** (fix/04-area-tie, 2026-11-04). The squared
  distances are counted in whole units of 1e-9, so a tie is exact on every
  platform (arm64 fused multiply-adds and x86-64 plain ones once picked
  different ties). Real rigs tie a lot: a UST's two arms sit alike, and the
  washes mirrored on two towers swap without changing the total. On his
  Chorus 384 matches tie and light 53% to 76% at both heights. Among them
  `AreaCover::zones()` takes the one that lights the most: the pairs some
  least-cost match can use are the zero-reduced-cost ones under the
  Hungarian potentials; each such pair's beam is sampled on a coarse grid
  (32 across, head height and surface, as bitsets); a second assignment on
  cost × K + "samples of its own cell this light leaves dark" and the plain
  pick are scored on the whole area, the better one is improved by swapping
  two lights while both new pairs stay tight and the count gains. The beams
  are each light's lens as declared or set by hand, not its zoom now (a
  zone must not move with what is playing). Bounded: skipped past 96
  lights or 3000 tight pairs, at most 4000 swaps tried. The Chorus's
  zones take 0.2 ms (27 µs before; Release, M4 Max), its coverage 0.33 ms
  (0.14). `AreaCover::cells()` itself stays the pure rule (no beams, plain
  pick).
- **Never on the tick.** An Aim look that dims blocked heads asks for the
  zones every playback tick (`Scene::write` → `updateSightDim` →
  `AimSolver::blockedDimmedHeads` → `memberPoints`). `zones()` and
  `memberPoints()` keep the last 8 results by their inputs (area, subject
  height, each member's position, lens and tape, the surface under the
  sample grid; each kept aim is re-checked against the surface under it)
  and hand them back shared, without allocating. Anything that changes
  an input is a new entry, worked out once. `hotpath_test`
  (`anAreaAimThatDimsWorksOutItsZonesOnce`) holds it: once in 100 ticks,
  0 allocations per lookup, no more allocations per tick than a point aim.
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
(about 48 across) twice (board Q213, 2026-11-03: "a spot counts as lit only
if it is lit at head height AND on the deck/riser surface; the plan shows
which one is missing"): at subject height above the surface under each
sample, and on the surface itself (`platformHeightAt`: deck, riser or step
top). A sample is lit at a height when it lies inside any cone there, and
**lit** when it is lit at both. The covered fraction is lit samples over all
samples. Two more fractions say what is missing: `headOnly` (lit at head
height, deck dark) and `deckOnly` (lit on the deck, head height dark). A
50° tower wash can read 100% at head height and leave the deck corners
dark; a near-horizontal beam does not reach the deck near the light at all;
a steep narrow beam is the other way, its circle wider on the deck than at
head height. The aim points are unchanged (head height); only the measuring
and the display are. Each gap is named by where most of its samples fall on
a 3 × 3 split (upstage / centre / downstage × stage right / centre / stage
left; +X is stage left): the deck-dark samples and the head-dark samples
are named separately.

The plan view (the Show Tree layer's Position rows, the formation editor)
draws the area outline, each zone, each beam's footprint at subject height
(the cone cut by the horizontal plane at its aim point), and hatches the
gaps two ways with a small legend: the deck dark in red slanting one way,
head height dark in violet slanting the other, crossed where both. Caption:
"Covers 82% of Chorus · deck dark upstage left · heads dark downstage
centre" ("gap upstage left" when both are dark in the same places).

## Show Review: MOV-13

"An area left partly dark" (Movement, Note): a cue whose Aim look at an area
leaves more than 10% of it unlit (lit = at head height and on the deck). It
names the cue, the area, how much is covered and which is missing where
("deck dark upstage left"), and says what fixes it: zoom wider, add a
light, or make the area smaller.

## Old shows

No file-format change. Existing area aims now spread over their area and
aim at subject height, so their DMX changes: CHANGELOG ▸ Changed.

## Cost

Zones are a sort of n members and a few dozen grid scores, done once per
Aim-look expansion like a line's places (not per frame unless the look is
expanded per frame, as a line's already is). Coverage sampling (48 × ~18
samples × n cones) runs only for the plan view and Show Review.
