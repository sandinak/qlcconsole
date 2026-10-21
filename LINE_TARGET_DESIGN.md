# Line targets — Design

Branson, 2026-10-16: *"we have point, area, we need line.. and the line can be
in space .. and curvable in multiple directions.. where you doubleclick to add
a mid point and drag to change.. handles on the ends to extend and retract and
move... grab the center click hold can move it around... and the lights will be
distributed along the line evenly. The effect should have a horizontal spread
slider to move the lights away from center 0->5 and across center 0 -> -5 ...
and a way to group them on the spreads .. so for 6 lights I could have them
1 2 3 cross 4 5 6 .. or an adjustment to have 1 and 2 .. then 3 and 4 .. etc."*

Post-release (after 0.1.0). Companion to `SPEED_SPREAD_DESIGN.md` (the Spread
vocabulary) and `POSITION_ARCH.md`.

---

## Decisions for Branson

**Decided 2026-10-16:**

- **Grouping (Q4).** The **Cross** control offers both readings of "1 2 3
  cross 4 5 6": *groups swap sides* (`4 5 6 | 1 2 3` at −5) and *each group
  flips in place / mirror* (`3 2 1 | 6 5 4`). **Pairs move as blocks**: each
  pair keeps its two points and the pairs cross as units (`5 6 3 4 1 2` at
  −5), i.e. Group = pairs + Cross = groups swap sides. "Share a point" was not
  what was meant for pairs; it stays only as a separate, minor option because
  it costs one flag in the maths (members' offset = 0) — drop it if unwanted.

**Pending Branson's review** (recommended answer first):

1. **Where do the lights sit at full spread (+5)?** *Pending Branson's
   review.* Rec: ends included — the first light on one end, the last on the
   other, the rest evenly between, so the end handles are exactly where the
   outer beams land. (Alternative: equal cells, half a gap at each end.) One
   light: the centre.
2. **What does the Spread slider do?** *Pending Branson's review.* Rec:
   linear in distance along the line, from the centre. 0 = every light on the
   centre point; +5 = the full even spread (Q1); +2.5 = the same pattern
   squeezed into the middle half; negative = the same distances on the
   **other** side of centre (beams cross), −5 = fully crossed. Nothing goes
   past the ends: to go wider, drag an end. Step 0.1.
3. **Which light goes to which point?** *Pending Branson's review.* Rec: by
   where the lights hang, measured along the line (stage-left light → the
   stage-left end), so +5 never crosses beams by accident and −5 always does.
   Option: "Look order" (the group's head order, then loose fixtures in the
   look's order). The line's own direction then doesn't matter.
4. *(Grouping — decided, above.)*
5. **Spread lives on the look, not the target.** *Pending Branson's review.*
   Rec: the Aim look (palette) carries Spread/Group/Cross, like an effect's
   Spread row, so two looks can aim at the same line with different spreads
   (duplicate the look).
6. **Double-click on a line inserts a point** — but double-click on a target
   opens its edit dialog today. *Pending Branson's review.* Rec: on a line,
   double-click the curve = add point, double-click a mid point = remove it,
   double-click the centre marker (or right-click → Edit…) = the dialog. Point
   targets unchanged.
7. **3D Rig Overview: show, don't edit, in the first cut.** *Pending
   Branson's review.* Rec: yes — targets are display-only there today; the
   Lighting Studio's Top / Front / Side views already give full 3D. Dragging
   handles in 3D is a later step if wanted.
8. **Animation.** *Pending Branson's review.* Rec: moving the slider moves the
   beams live; a cue crossfade from a +5 look to a −5 look fades pan/tilt
   (beams travel straight in pan/tilt space, close to but not exactly along
   the line). A spread that *breathes* on a speed master is an effect (P4).
9. **Follow spot / effects pointed at a line** use its centre point. *Pending
   Branson's review.* Rec: yes.

---

## Where things stand

- `StageTarget` (`engine/src/stagetarget.{h,cpp}`) is one point (`position`,
  mutex-guarded — the joystick writes it at 50 Hz while the DMX thread reads)
  plus a `Kind` persisted as a string: none / person / drumkit / structure /
  **area**. An Area is only a **drawn footprint** (W × D rectangle): every
  light still aims at its single point. So nothing distributes lights today —
  the line is the first target that gives each light its own point.
- An **Aim look** is a `QLCPalette::Aim` holding only a `stageTargetId`. Each
  fixture's pan/tilt is solved in `QLCPalette::valuesFromFixtures()` (case
  `Aim`) → `AimSolver::aimPointFor()` → `MonitorProperties::targetAimPointFor()`
  → `AimSolver::aimDegrees()`. The same point feeds `AimSolver::sceneAims()`
  (reach warnings), the Studio's aim lines, the Rig Overview's beams
  (`StructureStudioView::rebuildAimTargets`), the follow-spot and the design
  joystick (which drags the target's `position`).
- Editing: `TargetItem` in `MonitorGraphicsView` (Top/Front/Side via
  `projectMm`/`unprojectMm`), drag → `slotTargetMoved` → `UndoEntry::TargetMove`;
  Design mode only; no snapping for targets. Rig Overview draws targets only.
- Spread vocabulary (`SpreadLayer`, look editor "Spread" row): amount, axis,
  direction, **wings**, **blocks**, stored as reserved `__spread*` params. The
  line's grouping is the positional twin of wings/blocks.

---

## Model

- New `Kind::Line` (`"line"`). The target's **position is the line's centre**
  (half-way along it), and the line is a list of **control points stored
  relative to it**: two ends plus any number of mid points, full 3D (metres).
  Because the points ride on `position`, everything that already moves a
  target moves the whole line for free: the body drag, the design joystick,
  the follow-spot hold, a structure binding. After any point edit the points
  are re-centred so `position` stays the true centre — every old consumer
  that reads `position()` gets the spread-0 point.
- **Curve:** centripetal Catmull-Rom through the points (passes through every
  handle, no loops or overshoot when points are bunched). Two points = a
  straight line.
- **Even spacing:** an arc-length table (≈64 samples per span) maps "fraction
  along the line" to a 3D point, so lights are evenly spaced *along the
  curve*, not along X.
- Pure engine code, no Qt widgets: `engine/src/linecurve.{h,cpp}`
  (`pointAt(fraction)`, `length()`, `nearest(point)` for double-click insert),
  unit-tested.
- Thread safety: the point list sits under the same mutex as `position`.
- Aim height: points carry their own heights; a line made in plan sits on the
  deck/platform under the click. Kind aim-height defaults to 0 (the surface).

## Distribution and Spread (the maths)

Per Aim look on a line target, over the look's aimable fixtures — scene
fixtures plus group members, once each, the set `sceneAims()` already uses
(per fixture, not per head: a multi-head mover has one pan/tilt). Note this
must be computed over the **whole** set at once; today the palette is expanded
for groups and loose fixtures in two separate calls.

1. Order the N lights (Q3), slot k ∈ 0..N−1 at `u = k/(N−1)` along the line
   (Q1), i.e. offset from centre `d = u − ½` (−½..+½).
2. Split into groups of G (Q4: each light / pairs / threes / halves). Group
   centre `D` = mean of its members' `d`; member offset `e = d − D`. A group
   keeps its members' points relative to its centre, so pairs move as blocks.
   (Optional *Share a point*: groups re-spaced evenly end to end, `e = 0`.)
3. With spread `s` (−5..+5), `A = |s|/5`:
   `fraction = ½ + A·(σg·D + σm·e)`, where σg = σm = +1 for s ≥ 0, and for
   s < 0 either σg = −1 (Cross = *groups swap sides*) or σm = −1
   (Cross = *each group flips in place / mirror*).
   G = 1 with *groups swap sides* is the plain full mirror.
4. Aim point = `curve.pointAt(fraction)` + the target's position; solve as
   today.

Halves with an odd count: the middle light belongs to neither half and stays
on the centre. Defaults: spread +5, group 1, Cross = groups swap sides, order along the line
— i.e. a fresh line look is "evenly distributed", P1's whole behaviour.

### Six lights, line A→B (positions along the line)

At 0 every mode puts all six on the centre; ±2.5 is the ±5 pattern squeezed
into the middle half.

```
Each light            A---------------------------------------B
               +5     1       2       3       4       5       6
             +2.5               1   2   3   4   5   6
               -5     6       5       4       3       2       1

Halves, groups swap sides  ("1 2 3 cross 4 5 6", option A)
               +5     1       2       3       4       5       6
             -2.5               4   5   6   1   2   3
               -5     4       5       6       1       2       3

Halves, each flips in place / mirror  ("1 2 3 cross 4 5 6", option B)
               +5     1       2       3       4       5       6
               -5     3       2       1       6       5       4

Pairs, groups swap sides  ("1 and 2 .. then 3 and 4" -- chosen: blocks)
               +5     1       2       3       4       5       6
               -5     5       6       3       4       1       2

Pairs, each flips in place / mirror
               +5     1       2       3       4       5       6
               -5     2       1       4       3       6       5

Pairs share a point   (optional extra; not what was meant for pairs)
               +5     1+2                 3+4                 5+6
             +2.5               1+2       3+4       5+6
               -5     5+6                 3+4                 1+2
```

## Editing in the Lighting Studio (Top / Front / Side)

- **Add → Target Line**: a straight 4 m line across the stage, centred where
  you clicked, at deck height; then the target dialog (name, colour). The
  dialog's Kind combo can also turn a point into a line (4 m, centred on it).
- **Drawn as** the curve in the target's colour, a centre crosshair, square
  **end handles**, round **mid handles**; light-path lines run from each head
  to *its own* point (beams visibly converge/cross).
- **Press-drag the line body or centre** → moves the whole line (as a target
  drag today; same undo entry).
- **Drag an end handle** → moves that end (extend, retract, swing). *Alt-drag*
  = extend/retract along the line's direction only.
- **Drag a mid handle** → bends. Top moves it in X/Y, Front in X/height, Side
  in Y/height — so the line curves in any direction across the views.
- **Double-click the curve** → inserts a mid point exactly there (on the curve,
  so the shape doesn't jump). **Double-click a mid point** or right-click →
  *Remove point*. Ends can't be removed (a line keeps two points).
- **Shift-drag** locks to the horizontal or vertical axis of the view.
- No grid snapping (point targets don't snap either); locked targets and
  Run/Operate mode don't edit, as today.
- **Undo**: each gesture is one Cmd+Z step (the target-move undo extended to
  carry the point list).

## Programming tab — look editor

The Aim page keeps **Aim at:**; line targets are listed with a "Line:" prefix.
With a line chosen it adds:

```
Aim at:  [Line: DS edge        v]
Spread:  cross -5 |----------|----------| +5 spread     [ +5.0 ]
Group:   [Each light v]  Cross: [Groups swap sides v]  [ ] Share a point
         (Cross: "Groups swap sides" | "Each group flips in place (mirror)")
Order:   [Along the line v]
         A  1   2   3   4   5   6  B        <- live strip: numbered dots
```

The rig and the Studio move live as the slider moves (the scene re-resolves its
Aim values, as for a target drag). Reach warnings are per light, at its own
point.

## Rig Overview (3D)

Draws the curve (in its colour, on its real heights) with small dots at the
handles, and each beam lands on its own point — converging at 0, crossing
below 0. Display only in P1 (Q7).

## Storage (show XML)

```xml
<StageTarget ID="4" Name="DS edge" X="0.000" Y="1.500" Z="0.000" Kind="line">
  <LinePoint X="-2.000" Y="0.000" Z="0.000"/>   <!-- relative to X/Y/Z -->
  <LinePoint X="0.000"  Y="0.400" Z="0.000"/>
  <LinePoint X="2.000"  Y="0.000" Z="0.000"/>
</StageTarget>

<Palette ID="12" Type="Aim" StageTarget="4" ...>
  <EffectParam name="__lineSpread" value="-5"/>
  <EffectParam name="__lineGroup"  value="0"/>   <!-- 0 = halves -->
  <EffectParam name="__lineCross"  value="1"/>   <!-- 0 swap sides, 1 mirror -->
  <EffectParam name="__lineShare"  value="1"/>
  <EffectParam name="__lineOrder"  value="1"/>   <!-- 0 along the line, 1 look order -->
</Palette>
```

As built: the same `EffectParam` element and reserved-name convention
(`__line*`) as an effect's `__spread*`, one element per setting, written only
when it differs from the default (so each can be inherited on its own later). **Older builds**: an unknown
Kind reads as a plain point and child elements are skipped (both loaders
already `skipCurrentElement()`), so an old build sees a point target at the
line's centre with every light aimed there — sensible, not broken. Tests:
round-trip, and an old-format file loads unchanged.

## Phases

*Status 2026-10-17: P1, P2 and P3 built on `feat/line-targets` (see
DONE.md); P4 not started.*

- **P1 — Line target + editing + even distribution.** `Kind::Line`, points,
  `LineCurve` (Catmull-Rom, arc length), XML; per-light aim points through
  every consumer (palette expansion with the whole fixture set, `sceneAims`,
  Studio aim lines, Rig Overview beams, reach); Studio editing (add, body /
  end / mid drags, double-click insert/remove, Shift, undo); Rig Overview
  display; Add menu + dialog Kind. Fixed behaviour: spread +5, order along the
  line. Engine unit tests + offscreen UI test of the gestures; help topic.
- **P2 — Spread slider** (−5..+5), the look-editor rows, live strip, params
  saved.
- **P3 — Grouping**: Group (each / pairs / threes / halves), Cross (groups
  swap sides / each group flips in place), optional Share a point (+ Order
  option).
- **P4 (later, if wanted)** — spread on a control-surface fader; a "breathe"
  effect sweeping spread on a speed master; editing handles in the Rig
  Overview; effects that chase along the line.
