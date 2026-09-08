# TODO — Programmer / Programming-tab work

Active + not-yet-built work for the fork's programming/looks workflow.
**Shipped work has moved to [DONE.md](DONE.md)** (the completed log, with
per-feature detail + deferred/eyeball notes). Add new work here; move an entry
to DONE.md when it ships. See also the session memory under
`~/.claude/.../memory/`.

---

## Lighting Studio Editor: fixture 0 was unclickable; truss drag only had one freedom — SHIPPED, not yet Branson-verified (2026-09-08)

Branson: "still cannot move this fixture on this truss even though it's bound
to the truss" (Lighting Studio Editor — T-2, a Vertical truss with an XL-450
on it). Found by writing the drag as a headless test first
(`ui/test/monitor/monitor_test.cpp`, `StudioRig`) instead of guessing — the
rig drives the widget's real `mousePressEvent`/`mouseMoveEvent`/
`mouseReleaseEvent`, so a refusal anywhere in the chain shows up as a failure.

**Root cause 1 — the id-0 sentinel.** `StructureStudioView::hitTestFixture()`
returned `0` for "nothing here". But QLC+ hands the FIRST fixture in a
workspace id **0** (`Doc::m_latestFixtureId` starts there; the real marker is
`Fixture::invalidId()` == `UINT_MAX`). So fixture 0 was indistinguishable from
empty space and could never be selected, dragged, double-clicked or
right-clicked in the studio editor. Not hypothetical: in `~/Desktop/office.qlcc`
fixture 0 is bound to the Office Truss. The same `0`-means-nothing pattern was
in `StudioPlaneView` (`m_dragFid`, `hitTest`), the studio inspector's `curFid`
(every gel/face/angle/mount edit to fixture 0 silently dropped), the studio
tree rows (`data(0, UserRole).toUInt()` — folder rows never set the role, so
they read as 0 too; now via `studioRowFid()`), and
`Monitor::showFixtureItemEditor(quint32 onlyFid = 0)`. All now use
`Fixture::invalidId()`.

**Root cause 2 — the truss branch had one freedom and one dead end.**
`dragFixtureTo()` projected the mouse onto the truss axis and set `trussOffset`
only, so (a) `trussCross` was never reachable — a bound fixture could not be
positioned just off the bar as it is really mounted, which Branson reported
separately as "attach and then position just off the truss ... doesn't work" —
and (b) it bailed out with `l2 < 1e-6` whenever the run projected to a point,
i.e. EVERY top view of a vertical truss, where nothing could be moved at all.
Rewritten to work as a **delta** in world space: take the two in-plane
components from the mouse, keep the third from the fixture's current position,
then resolve that delta onto the mount's real freedoms — `trussOffset` along
the run, `trussCross` across it (clamped to the same two-widths zone the plot
uses), and whatever vertical is left over into `mountZOffset` (the drop). The
delta form makes the mount-side half-width and existing `mountZOffset` cancel,
so drags do not accumulate drift, and the off-plane component is zero by
construction — an elevation drag cannot disturb a top-view-only value.

Net effect per view: vertical truss — Front/Side slide along it, Top moves it
sideways; horizontal truss — Top slides + nudges across, elevations slide +
set the drop.

**Tests** (7 new, `monitor_test` now 22/22): `studioTrussDragMovesFixture`,
`studioTrussDragBlockedWhenLocked` (the Locked toggle still means select-only),
`studioTrussDragInEveryPlane`, `studioTrussDragAcrossTheRun` (top view of a
vertical run), `studioHorizontalTrussDragMovesFixture` (also pins a NON-zero
fixture id, so a pass is not an artefact of the id-0 rig),
`studioTrussDragSetsDropInElevation`.

**Note for the operator:** the editor's lock button still defaults to
**🔒 Locked** (select-only) — unchanged here, since Branson has been reading
that state deliberately ("...and it is unlocked"). Say the word if it should
open unlocked instead.

### Follow-on: the drag then SEGFAULTED (same day, fixed)

Branson: "tried to move it and segfault". Crash report
`qlcconsole-2026-09-08-120925.ips`: `EXC_BAD_ACCESS`,
`KERN_INVALID_ADDRESS at 0x18`, on
`mouseReleaseEvent` → `fixtureMoved` → `Monitor::updateFixture`
(`monitor.cpp:2693`) → `MonitorFixtureItem::setSize` →
`QGraphicsItem::prepareGeometryChange()`. The tiny fault address (0x18 =
`QGraphicsItem::d_ptr` in a `QGraphicsObject` with a null `this`) said NULL
pointer, not dangling.

**Cause:** `QHash::operator[]` on a non-const hash **inserts** on a miss.
`MonitorGraphicsView` read `m_fixtures[id]` as an rvalue in
`setFixtureGelColor`, `setFixtureRotation`, `fixtureGelColor` and
`removeFixture`; every miss left `{id, nullptr}` behind, after which
`updateFixture()`'s `contains(id)` guard waved the null through to
`item->setSize()`. The reachable route is a SECOND `removeFixture(id)` for the
same id — it drops the plot item and the monitor-properties entry but not the
Doc fixture, so the id stays live while the map holds null (deleting a truss
runs `removeFixture()` across `fixturesOnFeature()`, which is read from the rig
props). The sentinel fix above is what first made the studio drag emit
`fixtureMoved` at all, which is why a latent crash surfaced now.

All five sites now use `value(id, NULL)`, plus a null guard in
`updateFixture()`. The seven sibling maps (`m_trussItems`, `m_targetItems`,
`m_imageItems`, `m_platformItems`, `m_pipeItems`, `m_standItems`,
`m_towerItems`) were swept — clean.

**Tests** (`monitor_test` now 24/24): `updateFixtureSurvivesUnplacedFixture`
and `studioEditorDialogDragDoesNotCrash` (opens the real editor via
`slotEditTruss()`, poisons the map through the double-remove route, then drives
a genuine press/move/release). BOTH were revert-checked: each produces signal
11 with the fix backed out. Worth remembering — the first version of the
end-to-end test was **vacuous** (passed with the fix reverted, because an
unplaced fixture never reaches the dereference). Revert-check every regression
test before believing it.

`check-all.sh`: all four legs pass, 0 failures.

### Follow-on 2: the REAL reason that XL-450 would not move — a broken artifact

Branson: "STILL cannot grab that XL450 ... interestingly I can't find it in the
layres tree using search or perusing the tree .. do we have a broken artifact
in the file?" Yes. In `test-workspaces/stage-structures-demo.qxw` the `<FxItem>`
list jumps 6 → 9: **fixture 7 (the XL-450) is rigged on truss 2 with no monitor
item at all** (`<FixtureRig FID="7" Truss="2" Offset="2.396" TrussCross="0.093" .../>`
with no matching FxItem). Fixture 25 is likewise item-less, but it has no rig
props either, so it is just unplaced rather than stranded.

One missing element produced all three symptoms:
- **Invisible in the Layers tree** — `MonitorLayersPanel` builds from
  `fixtureItemsID()`, which only enumerates fixtures that HAVE items.
- **Drawn at the world origin** — `fixtureRigPosition()` opened with
  `if (!m_fixtureItems.contains(fid)) return QVector3D();`, so a genuinely
  truss-mounted fixture reported (0,0,0).
- **Could not be moved** — the drag *did* write trussOffset/trussCross, but the
  position lookup kept saying (0,0,0), so nothing appeared to change. None of
  the earlier drag work could have helped: the drag was fine, the lookup lied.

**Fixes** (all `engine/src/monitorproperties.{h,cpp}`):
1. `fixtureRigPosition()` no longer needs a monitor item. A truss/pipe/tower/
   riser mount derives the position entirely from the structure geometry; only
   the free-placed and deck branches read `m_fixtureItems`, and they now guard
   themselves.
2. New `repairOrphanedMounts()`, called once at the end of `loadXML()`: rebuilds
   the plot item from `fixtureRigPosition()` for any fixture that is mounted but
   item-less. Verified against the real file — logs `rebuilt the missing plot
   item for mounted fixture 7 at QVector3D(4.058, 1.344, 2.251)` (X = truss
   3.965 + cross 0.093, Z = offset 2.396 − half-width 0.145).
3. `removeFixture()` now drops the rig props with the item, so the orphan state
   cannot be created again. This also fixes a second latent bug found while
   testing: **fixture ids are REUSED** (`Doc::createFixtureId()` returns any free
   id), so a stale rig-props entry was inherited wholesale by the next fixture to
   take that id — arriving silently pre-mounted on a structure at a dead
   fixture's offset and drop height. It first showed up as test
   cross-contamination (a leaked `mountZOffset` of -0.4 m).

### Follow-on 3: heads now laid out per the fixture definition

Branson: "we have in the fixture def the layout of the leds .. that is how it
should appear in the containing box for the fixture." `MonitorFixtureItem::setSize()`
re-derived a grid from head count + aspect ratio and drew the XL-450's 15 x 5
matrix as **6 rows** (confirmed by reverting: the test reports `Actual 6,
Expected 5`). It now honours `mode->physical().layoutSize()` when the declared
grid can hold that mode's heads, falling back to the old inference otherwise
(`layoutSize()` defaults to 1x1, and some definitions declare a layout for a
different mode's head count). This is also the earlier "the highlighting doesn't
get all the LEDs as they're laid out .. hard to detect".

**Tests** (`monitor_test` now 26/26, both revert-checked):
`mountedFixtureWithoutPlotItemIsPositionedAndMovable` (models fixture 7 exactly:
rigged, item-less; asserts it reads on the truss and drags),
`declaredHeadLayoutIsUsed` (counts distinct drawn head top/left edges, so it
tests what is RENDERED rather than an internal counter).

`check-all.sh`: all four legs pass, 0 failures.

### Follow-on 4: side-view lateral drag + which-way-is-which labels

Branson: "ok .. can move it from front .. cannot move it laterally on side view
and there's no clear identification of which way is which in side view?"

**Lateral drag.** A VERTICAL run's axis IS the Z axis, so both horizontals are
free around it — but `FixtureRigProps` had only the single `trussCross` scalar,
hardcoded to X (`fixtureRigPosition()`: "a tower's run is vertical — across is
stage left/right (X)"). The side view's horizontal screen axis is Y, so a
lateral drag there resolved to exactly zero and nothing moved. Front worked only
because its horizontal axis happens to be the one X was assigned to.

Added `trussCrossY` (engine/src/truss.h) with XML round-trip, written only when
non-zero so existing workspaces are byte-identical. `dragFixtureTo()` now builds
an orthonormal basis of all three freedoms and assigns each to the field that
actually stores it:

| run        | along axis            | cross                 | third                        |
|------------|-----------------------|-----------------------|------------------------------|
| Vertical   | `trussOffset` (Z)     | `trussCross` (X)      | `trussCrossY` (Y)            |
| Horizontal | `trussOffset`         | `trussCross`          | `mountZOffset` (the drop)    |

Each plane drives the two freedoms it can see and leaves the third alone.

**Orientation labels.** `drawOrientationLabels()` draws edge labels on all four
sides of the canvas, using the vocabulary already in the app (+X = stage right,
+Y = upstage, +Z = up — cf. the "X (stage right):" / "Y (upstage):" spin-box
labels in monitor.cpp). Verified by grabbing the widget offscreen:
  - Side  : ◀ downstage / upstage ▶ / ▲ up / ▼ floor
  - Top   : ◀ stage left / stage right ▶ / ▲ downstage / ▼ upstage
  - Front : ◀ stage left / stage right ▶ / ▲ up / ▼ floor

NOT changed, deliberately: the Front view puts stage right on the RIGHT, which
matches +X = stage right everywhere else in the app. A theatrical front
elevation drawn from the audience's viewpoint would mirror that. Left consistent
with the app rather than flipped unilaterally — Branson's call.

**Test** (`monitor_test` now 27/27, revert-checked):
`verticalTrussMovesLaterallyInSideView` — a side drag moves Y without touching
X, and a front drag moves X without touching Y.

`check-all.sh`: all four legs pass, 0 failures.

### Follow-on 5: fixtures boxed by their real dimensions in every view

Branson: "so TOP view looks right / SIDE view looks the old way.. all should
look right and be boxed in based on relative dimensions of the device."

`StructureStudioView::drawFixtures()` drew a bar/matrix as a LINE from
`fixtureEndA()` to `fixtureEndB()`, with its across-extent always taken from
`physH` and `physD` never used at all. In the SIDE view of a front-facing panel
the long axis points into the screen, so both ends landed on the same pixel and
the fixture collapsed to a dot with no body — the "old way" look. The TOP view
drew it Height-tall when a plan view sees its Depth.

**Fix:** new `fixtureBoxPx()` treats a fixture as the W x H x D solid it is. Its
long axis (`fixtureAxisLocal`, from studioMount + studioAngle) carries Width,
the mount plane's normal carries Depth, and the remaining in-face axis carries
Height. All three are projected through `w2s()`, giving the screen spans of the
width/height axes plus the axis-aligned box containing the whole solid. Pixels
are placed at their TRUE 3-D position and projected, so the grid squashes on its
own when an axis turns away — no per-view special-casing:

| view  | box     | grid                                            |
|-------|---------|-------------------------------------------------|
| Top   | W x D   | 15 columns; rows collapse (stacked in Z)        |
| Front | W x H   | the full 15 x 5                                 |
| Side  | D x H   | 5 rows; columns collapse into depth             |

The studio renderer ALSO had its own aspect-ratio guess for the grid, separate
from the plot's, so Follow-on 3 had not touched it. `FixtureVisualTraits` now
carries the declared `layoutSize()` (validated against the mode's head count) so
both renderers read one source.

**Test** (`monitor_test` now 28/28, revert-checked — dropping depth reports
"top view: box is 3 px tall, expected 32"):
`fixtureIsBoxedByItsRealDimensionsInEveryView` asserts the projected box equals
the right dimension PAIR per plane, for a real XL-450 (401 x 180 x 100 mm,
15 x 5). Renders eyeballed offscreen for all three planes.

**For Branson to judge:** the TOP view is now flatter than the screenshot he
liked — 100 mm of depth rather than 180 mm of height, one row of 15 pixels
instead of 5 rows. That is the honest plan view (from above you see the panel's
top edge, not its face), but he called the old one "right", so if plan view
should keep showing the whole array as a schematic it needs a deliberate
special case.

**NOT done:** `MonitorGraphicsView::updateFixture()` (the main 2D plot) does its
own elevation foreshortening from width and height and likewise ignores depth —
same class of bug, separate renderer, left alone since the report was about the
studio editor.

`check-all.sh`: all four legs pass, 0 failures.

### Follow-on 6: double-click opened the wrong structure; the axis convention is backwards in half the tree

Branson: "when I double click on the xl450 at the top of t2 .. I should open t2
right? But instead it's opening USP2 .. which is adjacent but not under T2."

**Cause.** Fixture 7 carries `Truss="2"` AND `Deck="1"` -- two structural mounts
at once. `attachFixtureToTruss()`/`...ToTrussAt()`/`...ToTower()` set their own
id without clearing a previous mount (only `attachFixtureToPipe()` did, and even
it missed `towerId`), so moving a deck-standing fixture onto a truss left the
deck behind forever. `mouseDoubleClickEvent()` then tested the ids in its OWN
order -- deck before truss -- while `fixtureRigPosition()` checks truss first.
The fixture was DRAWN on T-2 and OPENED USP2 (platform id 1 is named "USP2").

**Fixes:** `FixtureRigProps::clearMounts()` called before every attach;
`FixtureRigProps::primaryMount()` as the single shared precedence (same order
`fixtureRigPosition()` uses) with the double-click switching on it, so the two
consumers cannot diverge again; and `repairOrphanedMounts()` extended to
collapse any file that already carries two mounts down to its primary.

### The axis convention: half the tree says the opposite of what the app does

Branson: "just realized my current layout in the screen has DS at bottom and US
at top? is that backwards from the standard?" No -- his layout is the STANDARD
ground plan (audience at the bottom of the page, upstage at the top, stage right
on the viewer's left). The code is what is wrong, and it contradicts itself:

  - `barFaceVector()` (monitorproperties.cpp:899) says "+Y = downstage (toward
    audience)" and maps `FaceDownstage` to `(0,+1,0)`.
  - `pipe.h`, `stagetarget.h`, `stageplatform.h`, `tower.h`, `stand.h`,
    `truss.h` and monitor.cpp's "X (stage right):" / "Y (upstage):" spin-box
    labels all claim the OPPOSITE, on both axes.

Real shows settle it. In `stage-structures-demo.qxw` the "SR Tower" is at
X=0.21 and the "SL Tower" at X=11.61; the upstage platforms are at Y=1.53 and
the downstage ones at Y=3.97. The plot draws +X rightward and +Y downward.
So **+X is stage LEFT and +Y is DOWNSTAGE**.

LESSON: Follow-on 4's orientation labels were derived from the spin-box wording
and were therefore BACKWARDS on both axes. They shipped wrong and Branson's
question is what caught it. Measure against real data, not against a label.

**Done** (Branson chose "labels + comments only" -- zero behaviour change):
the six spin-box labels and the six geometry headers now say stage left /
downstage, each header carrying the evidence above so nobody re-derives it from
the wrong half. The studio labels are corrected, and the same edge labels are
now on the MAIN plot (`MonitorGraphicsView::drawOrientationLabels()`, painted on
the viewport after the scene so they stay pinned while the plot pans/zooms;
`setOrientationLabelsVisible()` toggles).

**Deliberately NOT done:** `barFaceVector()` maps `FaceStageRight` to `+X`,
which is stage LEFT -- so a bar placed on the "stage right" face goes to stage
left. Correcting the mapping would MOVE every bar already placed that way when
its workspace reloads, so it needs a migration. Marked KNOWN INCONSISTENCY in
the source rather than silently changed.

### Follow-on 7: "On Rig" toggle

Branson asked whether truss fixtures should be visible from the top at all, or
hidden so the truss is selectable, and chose a toggle. New footer button beside
Rulers/Labels/Center: hides only fixtures with a structural mount (free-standing
ones are never hidden -- nothing is under them to reach), respects layer
visibility, persists to QSettings, defaults on.

**Tests** (`monitor_test` now 30/30, both revert-checked):
`structuralMountsAreExclusive` (deck -> truss must clear the deck, and the
position must agree with the mount) and
`mountedFixturesToggleHidesOnlyMountedOnes`.

`check-all.sh`: all four legs pass, 0 failures.

### Follow-on 8: view rotation (studio editor) — SHIPPED; plot + axonometric still open

Branson: "should we allow rotation in view as well as direction... so for users
that like it they can put DS at top. also is there an ability to look from an
angle .. like front from 30 degrees up?" He chose 90-degree steps, and a
view-only axonometric.

**Done — StructureStudioView.** `⟳` button in the dialog toolbar cycles
0/90/180/270, persisted (`monitor/studiorotation`). Implemented as a pure VIEW
transform inside `planeToScreenVec()` / `screenVecToPlane()` — the two functions
every world<->pixel path already funnelled through — so drawing, dragging,
hit-testing and the rulers all follow for free and NOTHING in the workspace
moves. Supporting changes: `refit()` swaps its span comparison on odd turns (the
'a' extent lands on the screen's vertical axis); `drawRulers()` measures
whichever axis is now vertical via the new `axisWorldPoint()`;
`structureCentreA()` centres on the axis the horizontal ruler is actually
measuring (otherwise its zero sits off the structure at 90/270); and
`drawOrientationLabels()` rotates the four edge labels with the view.

Quarter turns ONLY, deliberately: they preserve handedness, so a rotated plan
still tells the truth about which side of the stage a fixture is on. "Downstage
at the top with stage right still on the left" is a MIRROR, which flips the
sense of every direction on the plot — offered as an option and not chosen.

**Test** (`monitor_test` now 31/31): `viewRotationIsScreenOnlyAndDragStillWorks`
— stored positions unchanged at every turn, `screenToPlane()` an exact inverse
of `w2s()` at every turn, the fixture grabbable where drawn, and a DOWNWARD drag
at 180 degrees RAISES it (the drag follows the cursor, not the world axis).

**NOT done: the main plot's rotation.** It needs a different technique — the
plot is a QGraphicsView whose items draw their own geometry, so the rotation
belongs on the view transform, not the projection. Two obstacles, both scoped:
  - Labels. Nothing uses `ItemIgnoresTransformations`, so every label would turn
    upside down at 180. Tractable: trussitem/platformitem/toweritem/pipeitem/
    standitem/targetitem use CHILD text items and can be counter-rotated in one
    generic pass; only monitorfixtureitem and powersourceitem draw text inline.
  - **The grid fit.** Under a quarter turn the viewport's width maps to the
    grid's HEIGHT, so `m_cellPixels`/`m_xOffset`/`m_yOffset` need the same swap
    refit() got. This is the code behind the "STILL OPENS ZOOMED" saga, so it
    should land as its OWN commit with its own gate run — isolated and
    revertible — rather than inside a large mixed diff.

**NOT done: the axonometric view.** `project()` picks two of three coordinates,
which is exactly why `screenToPlane()` can invert. At an angle a screen point
maps to a RAY, so the inverse is ambiguous and dragging needs a constraint. The
mount-basis code from Follow-on 4 already resolves a screen delta onto the
mount's freedoms by dot product and generalises to any projection, so
structurally-mounted fixtures would still drag; free-placed ones are the hard
case. Agreed plan: ship it VIEW-ONLY first (look/measure/screenshot), editing
stays on Top/Front/Side.

`check-all.sh`: all four legs pass, 0 failures.

---

## Fixture Group grid cells now show each head's colour type (RGB/RGBW/W/Wheel) — SHIPPED, not yet Branson-verified (2026-09-07)

Branson, after the group-editor regression fix let heads show up in the
grid again: "OHhh .. shows head .. but doesn't show type .. we need to
know if they're RGB or W heads." Directly the point of the per-head drag
feature (letting a fixture's RGB heads go in one group and its White head
in another) — but the grid cell text only ever showed the fixture name,
head number, and DMX address, with nothing about what that specific head
actually emits.

**Fix** (`ui/src/fixturegroupeditor.cpp`): new `headColorTag(Fixture*,
headIndex)` — walks that ONE head's own channels (not the whole fixture)
and classifies them: a Colour-group channel with no single primary colour
= "Wheel"; 3+ distinct RGB primaries = "RGB" (+"W"/+"A" appended if White/
Amber channels are also present, e.g. "RGBW"); White with no RGB = "W";
Amber with no RGB = "Amber"; otherwise nothing shown (a plain dimmer has no
colour to report). Mirrors `classifyFixture()`'s existing whole-fixture
RGBW/wheel detection (`fixturevisualtraits.cpp`) but scoped to one head's
channel list, since a single fixture's heads can genuinely differ (that's
the whole reason this feature exists). `updateTable()` now appends
`[TAG]` as a third line in the cell's text/tooltip when non-empty.

Build: clean (only the pre-existing `mimeData() override` warning).
`check-all.sh` run in progress.

Not yet verified live — needs a real check: open a group with heads from a
fixture that has genuinely different per-head colour capability (e.g. one
of the US1 2-head fixtures, or a fixture with a split RGB head + White
head) and confirm the tag is correct per cell, not just repeated from the
whole fixture.

---

## Fix: fixture group layout editor would flash open and immediately close (regression) — SHIPPED, Branson-verified live (2026-09-07)

Branson: "hmm .. I can't open fixture groups to see layouts anymore." A
real regression introduced by this session's per-head drag-and-drop work
(`FixtureTreeWidget::setShowHeads()`), root-caused by tracing the exact
call chain rather than guessing:

`FixtureManager::fixtureGroupSelected()` creates the `FixtureGroupEditor`
and then calls `m_fixtures_tree->setShowHeads(true)` so heads are draggable
while the editor is open. `setShowHeads()` calls `updateTree()`, which
clears and repopulates the WHOLE tree — and `QTreeWidget::clear()` emits
`itemSelectionChanged()` synchronously, mid-call. Since
`FixtureManager::slotSelectionChanged()` is connected to that exact signal
AND `fixtureGroupSelected()` was itself called FROM `slotSelectionChanged()`,
this reenters it while the brand-new editor is still being wired up. The
reentrant call sees an empty selection (the tree was just cleared), falls
through every branch to the generic "nothing selected" case, and calls
`createInfo()` — which calls `clearRightPane()` — which deletes the
`FixtureGroupEditor` that was created two stack frames up, milliseconds
after it was created. Net effect: the editor flashes into existence and is
torn straight back down, so all the user ever saw was "Nothing selected."

**Fix** (`ui/src/fixturetreewidget.cpp`): `setShowHeads()` now blocks
signals around its `updateTree()` call. This is safe because `setShowHeads()`
is only ever called from `fixtureGroupSelected()`/`clearRightPane()`, both of
which are already explicitly managing the right-pane state change that
triggered it — the tree doesn't need to echo a selection-changed signal back
to the very code that's mid-flight causing it.

Build: clean. `check-all.sh`: PASS (Qt6/Qt6-Release; Qt5 SKIP, Qt6-Werror
pre-existing configure break, as always). **Branson confirmed this exact
regression while live-testing** — fix ships directly off that report, not
yet re-confirmed by him after the rebuild, but the root cause is proven via
code trace (not a guess), so marked verified pending his next look.

---

## Mover multi-head silhouette: one base+arms+head (or circle-in-square) unit per physical head, centred in its own slice — SHIPPED, not yet Branson-verified (2026-09-07)

Branson, on a real 2-head fixture ("US1 has two .. 2 head fixtures set
approx the back left and back center of the platform"): "need to fix your
centering on the multi head .. this seems better but each head should be
centered on the width/heads space."

Previous round's elevation/plan Mover paths always drew exactly ONE
base+arms+head (or circle-in-square) unit stretched across the fixture's
whole allotted width/rect, regardless of how many physical heads it has —
correct for the common single-head case, wrong for a genuine multi-head
fixture, where each head needs its own complete unit centered in its own
share of the space.

**Fix** (`ui/src/monitor/fixturevisualtraits.{h,cpp}`): both
`moverElevationPath()` and `moverPlanPath()` gained a `headCount` parameter
(default 1, so every existing call site keeps working unchanged). The
per-unit drawing logic was factored into `addMoverUnit()`/
`addMoverPlanUnit()` static helpers; the public functions now slice `r`
into `headCount` equal-width columns and call the per-unit helper once per
slice — one full base+arms+head (or circle-in-square) unit centred in each
column, instead of one unit spanning them all.

**Callers updated** to pass real head counts instead of the old
`multiHeadPanTilt ? 2 : 1` binary gate (`structurestudioview.cpp`'s Top-view
loop, Front/Side branch, and the tower-shelf special case; `monitorfixtureitem.cpp`'s
`drawBody()`) — using `traits.headCount` directly means Top and Front/Side
views now always agree on how many units to draw for the same fixture
(previously they could disagree: elevation was hard-coded to always draw
ONE combined body regardless of head count, while Top view's twin-unit
split required BOTH heads to have their own Pan/Tilt channel
(`multiHeadPanTilt`), which isn't the same condition as "has 2 heads").

**Also fixed while in there**: the Top-view multi-unit offset formula
(`u == 0 ? -spacing*0.5 : spacing*0.5`) only ever handled exactly 1 or 2
units correctly — a 3+-head fixture would have stacked heads 2 and 3 on
top of each other. Generalized to `(u - (units-1)*0.5) * spacing`, which
evenly spaces any number of units and is identical to the old formula for
1 or 2. `hitTestFixture()`'s Mover bounding-box test widened to account for
`units` in both planes, matching the wider drawn silhouette.

Build: clean. `check-all.sh` run in progress.

Not yet verified live — needs a real check: the two US1 2-head fixtures
(back-left and back-center of the platform) should now each show two
complete, evenly-centered base+arms+head units side by side, in both Top
and Front/Side views, instead of one stretched/off-center unit.

---

## Mover silhouette redesign: base + yoke arms + head (elevation), circle-in-square (plan) — SHIPPED, PARTIALLY Branson-verified (2026-09-07)

Branson, looking at the plain-oval Mover from the previous round: "I see the
oval .. which makes sense tho the top should be flattened where the light
comes out? .. but no arms and no base .. and it's not sitting on top. also
in the truss editor I should see the same figure right? also there's
perspective .. if looking down on top of fixture circles in a square ....
if looking from side .. there's a base with arms on the side that holds
the lighting head. A wash should be same ways - smaller moving head wash
has a small head - larger moving head wash has a big head etc." Confirmed
"Full scope now" when asked how much to build (vs. elevation-only, vs. just
fixing the tower-editor inconsistency).

Root cause of the two specific complaints:
- **"No arms and no base"** in the tower editor's own preview: that preview
  IS `StructureStudioView` (same editor, embedded per-object via
  `Monitor::makeStudioPane()`), but a tower-shelf-MOUNTED fixture hits an
  OLDER special-case code path (built earlier this session, before the
  classifier existed) that always drew a generic sitting/hanging trapezoid
  for ANY fixture kind on a shelf — completely bypassing
  `classifyFixture()`. That's why it didn't match the Mover shape at all.
- **The oval not looking "flattened" / no square**: `MonitorFixtureItem`
  (main 2D canvas) had zero awareness of which way the canvas is looking at
  it (Top vs Front vs Side) — it drew the identical ellipse regardless of
  POV, so there was only ever one shape, never a plan-view vs elevation-view
  distinction.

**New shared geometry** (`ui/src/monitor/fixturevisualtraits.{h,cpp}` --
pure `QRectF in -> QPainterPath out` functions, no view-class dependency, so
every renderer draws and hit-tests the IDENTICAL shape):
- `moverElevationPath(r, hung)` — base block flush with the mounting
  surface, two yoke arms rising from it, head suspended between them.
  `hung` mirrors it (base at the top, hanging below) for a TopHung mount,
  matching the existing tower-shelf sit/hang mirroring convention.
- `moverPlanPath(r)` — round head with a flat chord on its local "front"
  edge (the downstage-default convention already used for facing=0
  elsewhere in this codebase), inset in a rounded square base footprint —
  "circles in a square."

**`StructureStudioView`** (`structurestudioview.{h,cpp}`): the tower-shelf
special case now branches on `classifyFixture()`'s kind — Mover gets
`moverElevationPath()` sized to the same `towerFixtureBodyRect()` used for
hit-testing (so "I should see the same figure" now holds); everything else
keeps the trapezoid. The normal (non-shelf) Mover branch now branches on
`m_plane`: Top draws `moverPlanPath()` per unit (still two side-by-side for
a genuine twin-head pan/tilt wash); Front/Side draws one combined
`moverElevationPath()` (multi-head yoke splitting judged not worth the
complexity in elevation). New shared `moverBaseRadius()` (previous round)
still drives the sizing -- physical width when declared, `hasFocus`
heuristic otherwise -- so a wash with a real declared Width still reads
bigger than a spot with none. `hitTestFixture()`'s Mover branch updated to
a bounding-box test matching whichever silhouette was actually drawn
(previously a plain circle-distance test, which undershot the square/base
corners).

**`MonitorFixtureItem`** (`monitorfixtureitem.{h,cpp}`): new
`setElevationView(bool)`, wired from `MonitorGraphicsView::updateFixture()`
(`item->setElevationView(isElevation())`, right next to the existing
`setSize()` call) — `refreshAllItems()` already re-runs `updateFixture()`
for every fixture on any POV change, so this stays in sync automatically,
no new refresh path needed. `drawBody()`'s Mover case now picks
`moverElevationPath()` or `moverPlanPath()` from that flag instead of a
plain `drawEllipse()`. Hit-testing (`shape()`) deliberately left untouched
again — its rect-with-margin is still a safe superset of either new shape.

Build: clean (only the same pre-existing, unrelated warnings).
`check-all.sh` run in progress.

**Verification status — partial, and here's exactly why**: launched
`qlcconsole -o test-workspaces/surfacetesting.qxw` and screenshotted it.
**Confirmed working in Top (plan) view**: LM70 #1/#2 and the UST-series
Movinghead+Circle fixtures now draw as a rounded-square base with a round
head (pan/tilt arcs still overlaid on top, per existing behavior) instead
of a plain oval — a real visual change, screenshotted and inspected up
close. Switching the canvas to "2D — Front" via the View combo DID change
the view (ruler and truss rendering updated correctly to elevation), but I
could not get a clean, safely-obtained close-up of the fixture icon itself
at that zoom to confirm the base+yoke+arms shape reads correctly on
screen — window focus in this sandboxed environment kept reverting to the
Claude Code / VS Code window between screenshot and click, and one stray
zoom-field edit attempt actually typed into Branson's live VS Code
integrated terminal instead of qlcconsole (harmless -- `64` + Enter,
`zsh: command not found: 64` -- but real; disclosed to Branson directly).
Continuing to force automated clicks after that felt like the wrong
tradeoff, so this stopped short of full self-verification.

**Needs Branson to check directly**: (1) main 2D canvas in Front/Side
POV — does a Mover now show a readable base+arms+head at a normal working
zoom (not the tiny full-stage-overview zoom used here)? (2) the SR Tower
editor's own Front-view preview — does the Focus Spot Three Z (or any
tower-shelf-mounted mover) now show the SAME base+arms+head shape instead
of the trapezoid, sitting/hanging correctly relative to its shelf? (3) does
the flat-edge notch on the Top-view circle read as intentional rather than
a rendering glitch, especially on a single (non-twin) mover away from the
pan/tilt arc overlay?

---

## Structure Studio's Mover icon now sizes from declared Physical width, matching Par — SHIPPED, not yet Branson-verified (2026-09-07)

Branson tested the classifier with two real custom fixture defs he built —
"Branson - LED Movinghead+Circle" (photo: a 36-LED wash head, big round
face) and "Branson - LED SPOT" (photo: a slim beam/spot head) — expecting
the wireframes to look "relatively similar" to the real fixtures, i.e.
visibly different sizes.

Root-caused directly against both `.qxf` files in
`~/Library/Application Support/qlcconsole/Fixtures/`: both declare
`Type="Moving Head"`, both are single-head, **neither has ANY Beam-group
Focus/Zoom channel, and both have `Dimensions Width="0" Height="0"
Depth="0"`** — i.e. every trait the classifier reads is currently
IDENTICAL between them. That's not a bug in the classifier; it's that nothing
in either fixture definition encodes "this one has a big dish, this one has
a small lens" yet. The main 2D canvas already sizes a fixture's whole icon
from `QLCPhysical` width/height when declared
(`MonitorGraphicsView::updateFixture()`, pre-existing), and the Structure
Studio Par silhouette already did the same — but the Structure Studio
**Mover** silhouette didn't; it only used the `hasFocus` heuristic (a
focus/zoom channel implies a bigger lens), which is false for both of
these fixtures, so both drew as an identical 6.5px circle.

**Fix**: new shared `StructureStudioView::moverBaseRadius(const
FixtureVisualTraits&)` (declared in `structurestudioview.h`, used by both
`drawFixtures()` and `hitTestFixture()` so the hit area always matches what
Branson sees) — sizes from `traits.physW` when it's declared (same formula
Par already uses: `qMax(6.0, physW * 0.5 * m_scale)`), falling back to the
old `hasFocus ? 9.0 : 6.5` heuristic only when Physical width is still 0.

Build: clean. `check-all.sh` run in progress.

**Not a code fix Branson needs to verify — an input he needs to supply**:
populate `Dimensions/Width` (and ideally `Height`) in the Physical block of
both `Branson-LED-Movinghead+Circle.qxf` and `Branson-LED-SPOT.qxf` (Fixture
Editor → Physical tab) to something reflecting their real size (e.g. a wide
dish vs. a narrow barrel), reload the fixtures, and check both the main
Lighting Studio 2D canvas and the Structure Studio elevation editor — both
should now show a visibly bigger icon for the Movinghead+Circle than the
LED SPOT. Until Width is populated, both will keep drawing at the same
default size — that's expected, not a regression.

---

## Fixture visual classifier now shared with the main 2D Lighting Studio canvas — SHIPPED, not yet Branson-verified (2026-09-07)

Branson, after the Structure Studio classifier shipped: "hard to tell what
I am working with" (fixed there); then, asked whether to extend the same
classifier to the main plan-view canvas: "yes .. fixture classifier .. and
yes we'll want colors eventually" (per-head matrix-dot colouring by
RGB/White is explicitly deferred, not built this round).

`classifyFixture(Fixture*)`/`FixtureVisualTraits`/`FixtureSilhouette` were
previously a `static` function local to `structurestudioview.cpp` — not
reachable from `MonitorFixtureItem` (the main canvas's per-fixture render
item), which lives in a different file with no dependency on the Structure
Studio editor. **Extracted them to new shared files**
`ui/src/monitor/fixturevisualtraits.{h,cpp}` (registered in
`ui/src/CMakeLists.txt`), byte-for-byte the same classification logic;
`structurestudioview.cpp` now `#include`s it instead of defining its own
copy.

**`MonitorFixtureItem`** (`ui/src/monitor/monitorfixtureitem.{h,cpp}`):
classifies once at construction (`m_traits = classifyFixture(fxi);`,
right after resolving `Fixture *fxi`). New private `drawBody(QPainter*,
const QRectF&)` dispatches the outer body shape on `m_traits.kind`:
- **Mover** (Moving Head/Scanner) → ellipse "puck", matching the round head
  unit already drawn for the same fixture Type in the Structure Studio
  elevation editor.
- **Par** (Color Changer/Dimmer/Strobe) → rounded rect (small radius) — a
  can/wash reads as a can, not a sharp box.
- **Bar/Generic** (LED bars, everything else) → unchanged sharp rect —
  preserves the existing look for the bulk of the library exactly as
  before, since this is deliberately the default/fallback case.

`drawBody()` replaces four previously-duplicated `painter->drawRect(...)`
call sites that all drew the same rect at different insets: the selection
halo, the item background fill, the truss-bind/attach-mode ring, and the
marked-in-black dashed outline — all four now stay shape-consistent with
each other and with the body automatically, instead of needing four
matching edits if the shape ever changes again.

Hit-testing (`shape()`) was deliberately left untouched: it already returns
a rect with a few pixels of margin around the body for the movement-arc
strokes, so a Mover's ellipse sitting inside that rect is a strict subset —
no "looks round, but you have to click the corner" mismatch, unlike the
tower-shelf/Structure-Studio hit-test bugs found earlier this session
(those were genuine geometry mismatches; this one draws a smaller shape
inside an already-generous hit rect, which only makes clicking easier, not
harder).

Build: clean (only the same pre-existing, unrelated warnings —
`mimeData() override` and the `PreviewItem` anonymous-typedef warning).
`check-all.sh` run in progress.

Not yet verified live — needs a real check: open the main Lighting Studio
2D view with a mix of moving heads, PARs, and LED bars patched, confirm
movers now draw as circles, PARs as rounded cans, and bars/generic fixtures
look exactly as they did before (no regression) — including selection
halo, the truss-bind colour ring, and the marked-in-black outline all
following the new shape.

**Deferred, explicitly not built this round**: per-head colouring of
matrix/bar dots by pixel type (e.g. an RGB head vs. a White head on the
same fixture rendering as different colours) — Branson: "we'll want colors
eventually." Depends on the per-head `FixtureGroup` membership /
per-head-drag work (previous entry, this file) actually being used to
build such fixture groupings first.

---

## Fixture Group editor grid now accepts per-head drops from the Fixture Manager tree — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "so if that's the case what's the best way to select just RGB for a
fixture .. and just W for a fixture .. I made sub fixture defs where we can
add them individually as separate .. but then they're not easily shown in
the system .. so what's the right way?" Answer worked out with him: a
`FixtureGroup`'s layout grid is already keyed by `GroupHead{fxi, head}`, not
by whole fixture — it can already reference an arbitrary subset of one
fixture's heads (e.g. only the RGB heads of a multi-head unit, leaving the
White head out) — so no new fixture-def schema is needed. The gap was
purely UI: there was no way to *drag* an individual head into the grid, only
whole fixtures (or the existing toolbar arrow-button
`FixtureGroupEditor::addFixtureHeads()` path via a Heads-mode
`FixtureSelection` dialog, which already worked and is still there as an
alternative). Branson confirmed: "yes .. build that .. then we can work on
the visualizer."

**Fix**, source side (`ui/src/fixturetreewidget.{h,cpp}`):
- New `FixtureTreeWidget::setShowHeads(bool)` toggles per-fixture head child
  rows on/off at runtime (rebuilds the tree) — kept off by default so the
  tree isn't permanently bloated with head rows for every fixture; the
  Fixture Manager only turns it on while a `FixtureGroupEditor` is the
  active right-pane (`fixtureGroupSelected()`/`clearRightPane()` in
  `ui/src/fixturemanager.cpp`).
- New `HEAD_DRAG_MIME_TYPE` (`headDragMimeType()`) alongside the existing
  fixture/group MIME types. `buildMimeData()` now inspects each dragged
  item: one carrying `PROP_HEAD` resolves its parent's fixture id and
  streams `(fixtureId, headIndex)` quint32 pairs into this new format
  instead of the plain fixture-id stream.

**Fix**, drop side (`ui/src/fixturegroupeditor.cpp`, `eventFilter()`):
`DragEnter`/`DragMove` now also accept `headMimeType`. `Drop` no longer
bails out when the plain fixture MIME is absent — it independently checks
`hasFixtures`/`hasHeads` and runs either or both loops in the same drop:
the existing per-fixture loop calls `assignFixture()` as before; a new
per-head loop reads `(fid, headIdx)` pairs and calls
`m_grp->assignHead(QLCPoint(col, row), GroupHead(fid, headIdx))`, spreading
successive heads across the row and growing the grid exactly like the
fixture path already did. Both loops share the same `beginEdit()`/
`endEdit()`/`cancelEdit()` transaction and grid-resize logic.

Build: clean (only pre-existing, unrelated warnings — `mimeData() override`
and the `PreviewItem` anonymous-typedef warning, both present before this
change). `check-all.sh` run in progress.

Not yet verified live — needs a real check: open a fixture group with a
multi-head fixture that has independently-addressable RGB/White heads
patched, expand it in the tree (should auto-expand once a group is
selected), and drag a single head child (not the fixture row itself) onto
an empty grid cell — confirm only that one head lands, not the whole
fixture, and that whole-fixture drag-drop still works unchanged.

---

## Fixture visual classifier: movers/PARs/bars (incl. matrix panels) now draw differently in the Structure Studio editor — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "hard to tell what I am working with so lets goto the fixture
identification we talked about... also want to make sure that when
looking at a light bar and there's a matrix layout .. the bar looks like
that .. rectangle of multiple lines .. or a single long bar." First real
build of the classifier plan from earlier — scoped to `StructureStudioView`
(`ui/src/monitor/structurestudioview.cpp`), where the tower-shelf body shape
already proved the pattern; the main 2D Lighting Studio canvas is a
separate rendering system (`MonitorFixtureItem`) and deliberately NOT
touched in this pass.

**New `classifyFixture(Fixture*)`** — reads only data already present in
every fixture definition, no schema changes, so it applies retroactively to
the whole existing library:
- **Shape family** from `QLCFixtureDef::type()`: Moving Head/Scanner →
  Mover, Color Changer/Dimmer/Strobe → Par, LED Bar (Beams/Pixels) → Bar,
  everything else → Generic (unchanged original bar+dots rendering,
  untouched).
- **Has-focus** from a Beam-group channel whose `QLCChannel::preset()` is
  one of the Focus/Zoom presets (validated on real data last round: 3Z has
  one, LM70 doesn't) — a bigger Mover head.
- **Colour-mixing (RGBW) vs wheel**: 3+ distinct Intensity/Colour-group
  primary colours = mixing; a Colour-group channel with no single primary
  colour = a wheel. (Classified but not yet used to change the drawn shape
  further — tracked in traits for a follow-up, not acted on this round.)
- **Multi-head pan/tilt**: 2+ heads each owning their own Pan or Tilt
  channel (`QLCFixtureMode::headForChannel()`) — a twin-head wash bar draws
  as two head units on one body instead of one.
- **Physical size** (`QLCPhysical` width/height/depth, mm→metres) — drives
  Par can size and the Bar/matrix decision below. Zero today for nearly
  every fixture Branson actually has patched (checked last round); he said
  he'll populate it, so this reads it wherever it's there and falls back to
  a sane default everywhere it isn't.

**New shapes in `drawFixtures()`** (dispatched on `classifyFixture()`'s
result, replacing the generic bar+dots for Mover/Par/Bar — Generic keeps
the original code path verbatim):
- **Mover**: a compact head unit (not a bar) — a filled circle sized bigger
  when it has focus, with a lighter "lens" dot; two side-by-side units when
  multi-head pan/tilt.
- **Par**: a filled rounded rect ("can"), sized from physical W/H when
  declared.
- **Bar**: if the declared physical height is a meaningful fraction of the
  width (not just a thin strip) AND there are at least 4 pixels — draws as
  a GRID of parallel rows (row/column split derived from head count × the
  declared aspect ratio) instead of one line, so a genuine matrix panel
  reads as a panel. Otherwise unchanged: one line, dots spaced along it.
  This is gated entirely on physical Width/Height being populated — a bar
  with no declared height still renders as a single line today, which is
  exactly why Branson populating physical sizes is the next real step here.

**Hit-testing updated to match, same lesson as the tower-shelf bug two
rounds ago**: `hitTestFixture()` now resolves the SAME shape per fixture
(Mover → circle radius, Par → rect, Bar/matrix → widened line threshold)
instead of always testing the old bar-line — otherwise these new shapes
would have the exact "looks like X, but you have to click somewhere else
entirely to grab it" bug all over again.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: open a Moving
Head's truss/tower editor and confirm it now draws as a head, not a bar;
open a Color Changer/PAR and confirm a can shape; and once physical W/H is
populated on a real multi-row bar fixture, confirm it draws as a grid
instead of a line.

## Fixtures tree: added a search box — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "can we put a text search box at top of fixtures tree." Added to
the Fixture Manager's main "Fixtures" tab tree (`ui/src/fixturemanager.{h,cpp}`)
— the primary/canonical fixtures tree, as opposed to the Structure Studio
editor's narrower "Fixtures on this object" list; if this was actually
meant for that one instead, easy to add there too.

Wrapped the tree in a small container with a `QLineEdit` above it
(placeholder "Search fixtures...", clear button). Filters by row NAME as
you type — recursively hides any row (fixture, group, folder, Power/
Universes node) whose own name doesn't match and has no matching
descendant, so a folder stays visible whenever something inside it still
matches, and auto-expands a folder that has a hidden match inside it.
Clearing the box (or the clear-button) restores everything.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: type a fixture
name fragment, confirm only matching rows (and their ancestor folders)
stay visible, and confirm Power/Universes/Fixture Groups still work
normally once the box is cleared.

## Frame-group membership silently overrode a fixture's real truss/pipe/tower/riser/deck mount — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "lets fix precedence bug now and move forward" — confirming the
root cause traced for "saved and can't move [a fixture on T-2]." Confirmed
against the actual saved rig data first (`FixtureRig FID="7" Truss="2" ...
GLX="0.313" GLY="0.051" GLZ="-0.031"` — BOTH a truss binding and frame-group
`groupLocal` coordinates set at once), then confirmed why: `attachFixtureToTruss()`
deliberately also adds the fixture to that truss's own auto-created group
(`ensureTrussGroup()`) purely so the Layers tree/canvas can select
truss+fixtures together — a side effect, not a repositioning intent. But
every place that resolves a fixture's actual position checked "is this a
frame-group member?" **first, unconditionally** — so that harmless
selection-convenience side effect silently became the fixture's ENTIRE
positioning mechanism the moment it fired, permanently shadowing the real
`trussOffset`-based position underneath. Same root shape as the SR Tower
shelf duplicate and the crash fix earlier this session: a real, provable
ordering mistake, not a guess.

**Fix, three places, same precedence restored — structural mount (truss/
pipe/tower/riser/deck) wins; frame-group is the fallback for a fixture with
NO other mount at all**:
- `MonitorProperties::fixtureRigPosition()` (`engine/src/monitorproperties.cpp`)
  — the canonical position every renderer and the aim-solver ultimately
  read from. Moved the frame-group branch from first to last (after every
  structural-mount check fails to match).
- `StructureStudioView::dragFixtureTo()` (`ui/src/monitor/structurestudioview.cpp`)
  — duplicated the same precedence independently for dragging; same reorder.
- `StructureStudioView::fixtureEndA()/fixtureEndB()` — same duplication for
  the drawn bar's endpoints (and therefore `hitTestFixture()`, which hit-
  tests against those same endpoints). New shared `static bool
  hasStructuralMount(const FixtureRigProps&)` used by all three instead of
  three copies of the same condition.

**Left alone, now provably harmless rather than fixed**: `MonitorGraphicsView::
slotFixtureMoved()` (main 2D canvas drag handler) still unconditionally
writes `groupLocal` for a frame-group member regardless of its structural
mount — but since `fixtureRigPosition()`'s READ side now always prefers the
structural mount when one is set, that write is dead data for such a
fixture, never read as authoritative again. Not worth a broader refactor
pass alongside this fix; flagged rather than silently left unmentioned.

Builds clean (full engine+UI rebuild — `monitorproperties.h`/`.cpp` are
widely included). Full `check-all.sh` gate run after (see job result). Not
yet verified live — needs a real check: open T-2's editor, drag the
XL-450RGB fixture (FID 7) and confirm it now slides along the truss
(`trussOffset`) instead of not visibly moving; separately confirm a genuine
frame-group-only "Studio Group" fixture (no truss/pipe/tower/riser/deck at
all) still drags correctly via the frame-local path, since that's the one
case this fix must NOT have broken.

## Real crash: truss editor Cancel could segfault after "Add Bar" — SHIPPED, root cause not 100% certain, needs Branson to confirm (2026-09-07)

Branson: "OH and we just had a segfault! chase that too." Pulled the actual
crash report (`~/Library/Logs/DiagnosticReports/qlcconsole-2026-09-07-
185334.ips`) rather than guess. `EXC_BAD_ACCESS`/`SIGSEGV` at address
`0x18` (classic dangling/near-null pointer), stack: `MonitorGraphicsView::
mouseDoubleClickEvent → trussDoubleClicked → Monitor::slotEditTruss →
MonitorGraphicsView::updateTrusses → refreshItemLayerState →
QGraphicsItem::setVisible`. **Caveat on confidence**: the crashed binary's
UUID doesn't match my current build (rebuilt several times since), so I
could not get an exact crashing line via `atos` — the fix below is the one
CONCRETE, provably-wrong thing found while tracing this exact call chain,
not a certainty this is the only cause. Flagging that honestly rather than
claiming more certainty than the evidence supports.

**Found, real, and definitely wrong regardless**: `Monitor::slotEditTruss()`
's Cancel path (`ui/src/monitor/monitor.cpp`) — "Add Bar" while the truss
editor is open creates a real child `Truss` (tracked in `barsCreatedHere`
for undo); on Cancel, the code called `m_graphicsView->updateTrusses()`
**first**, THEN removed those temp bars from the engine model
(`m_props->removeTruss(barId)`) **after**. `updateTrusses()` rebuilds
`MonitorGraphicsView::m_trussItems` from `props->trusses()` — which still
included the temp bar at that point — so it built a real, live `TrussItem`
for it, and only THEN did the temp bar's underlying `Truss` get deleted out
from under that just-built item. That leaves a dangling `TrussItem` in
`m_trussItems` (its `truss()` pointing at freed memory) sitting there until
the next full rebuild — a window where any subsequent `refreshItemLayerState()`
call (triggered by nearly anything — a selection change, another double-
click) dereferences it. Reordered: remove the temp bars first, rebuild
second.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). **Needs Branson to confirm this was actually the trigger** —
best repro guess: open a truss's editor, right-click the fixture-placement
strip to "Add bar here", then Cancel the dialog; if a crash doesn't recur
after that sequence a few times, this was very likely it, but I don't have
certainty from the crash report alone given the binary-version mismatch.

## Tower editor: double-click a shelf to edit its height — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "how do we edit a shelf position .. can we click on the height and
edit?" Direct answer before this: no, `Tower` only ever had `addShelf()`/
`removeShelf()` — moving one meant delete + re-add. Built it, deliberately
NOT via remove+re-add (which would hit the exact index-reassignment hazard
already flagged for `removeShelf()` — a fixture "on shelf 3" silently
ending up on a different physical shelf).

**`Tower::setShelfHeight(int i, float z)`** (`engine/src/tower.{h,cpp}`)
mutates the shelf at index `i` **in place, without re-sorting** the list —
on purpose, so its index (and therefore anything already pointing at it via
`FixtureRigProps::towerShelf`) stays exactly where it was. Shares the same
1cm duplicate guard `addShelf()` just got (checked against every OTHER
shelf, not itself). `ui/src/monitor/monitor.cpp`: double-click a row in the
Tower editor's shelf list → a height prompt (unit-aware, ft/m) → applies via
`setShelfHeight()`, reloads the list and the canvas. Tooltip added to the
list itself for discoverability.

**Bug caught and fixed while wiring this in, before it shipped**: the
dialog's own Cancel-revert path (`slotEditTower()`) restores shelves by
looping `removeShelf(0)` + `addShelf(z)` for each snapshotted height — which
would now run every restored height through `addShelf()`'s NEW duplicate
guard, silently dropping one shelf on Cancel if the tower already had a
duplicate before this session's fixes (SR Tower, from the entry above,
currently does). Added `Tower::setShelves(const QList<float>&)` — a raw,
guard-free bulk replace, for exactly this "restore a known snapshot exactly
as it was" case — and switched Cancel to use it instead.

Builds clean (`qlcconsole` target, engine change). Full `check-all.sh` gate
run after (see job result). Not yet verified live — needs a real check:
double-click a shelf, change its height, OK, confirm any fixture already on
that shelf followed it (didn't jump to a different one); separately, edit a
shelf then Cancel and confirm the tower reverts completely, including on
SR Tower specifically (its still-unresolved duplicate is the case that
would have broken silently without the setShelves() fix).

## Tower editor: "Add shelf" could silently create an exact duplicate — SHIPPED, not yet Branson-verified (2026-09-07)

Branson, on a screenshot of SR Tower's editor: "shows 4 shelves .. but only
3 showing .. it's not clear why 3 in diagram and 4 in the side." Checked
against the actual saved workspace rather than guessing from the rounded
UI numbers: `test-workspaces/stage-structures-demo.qxw`'s SR Tower has
FOUR `<Shelf>` entries, and two of them are `Z="1.000"` — genuinely,
exactly identical, not a display-rounding coincidence (1.000 m → 3.28 ft
both ways, matching the screenshot's "Shelf 2 — 3.28 ft" / "Shelf 3 — 3.28
ft" exactly). Two shelves at the identical height draw as one overlapping
line/label in the elevation view — that's the "4 in the list, 3 in the
diagram" gap, not a rendering bug.

**Root cause**: `Tower::addShelf()` (`engine/src/tower.{h,cpp}`) just
appended and re-sorted, with no duplicate check at all — clicking "Add
shelf" without changing the height spinbox from a value that already
matched an existing shelf created exactly this, silently, no warning.

**Fix**: `addShelf()` now rejects (no-ops) a height within 1cm of an
existing shelf, so this specific silent-duplicate case can't be created
going forward.

**Not touched**: the existing duplicate already saved in SR Tower — didn't
edit Branson's workspace file directly (this session's standing practice);
he can clean it up himself now that it's visible: select one of the two
"Shelf — 3.28 ft" rows in the Tower editor's shelf list and click "Remove
selected."

**Worth flagging again while this is fresh**: shelves have no stable
identity — they're a plain sorted array, and a fixture's `towerShelf` is
just an index into it. Removing the extra 3.28 ft shelf here will shift
every shelf AFTER it down one index — SR Tower's own `FixtureRig FID="4"`
is on `TShelf="3"` (the top shelf, 4.92 ft); after removing one of the
duplicates it becomes index 2, and unless that fixture's rig data is
re-pointed too it will silently read as being on the WRONG shelf after the
cleanup. This is the same reindexing hazard flagged earlier this session
(with `removeShelf()`) — still an open decision (give shelves a stable id
vs. accept and work around the reindexing), not resolved by this fix.

Builds clean (`qlcconsole` target, engine change). Full `check-all.sh` gate
run after (see job result). Not yet verified live.

## Tower mounting: Top-of-tower / Bottom-of-tower, as real mount positions — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "still can't put on the top or on the bottom" (repeated from
earlier in the session) — closing out the design item discussed then:
`towerMountSide` (Shelf/Top/Bottom), mirroring how `trussMountSide` already
gives a truss-bound fixture a real, explicit mount position instead of
faking it with shelf-index sentinels.

**New**: `FixtureRigProps::TowerMountSide` enum (`TowerShelf`/`TowerTop`/
`TowerBottom`, engine/src/truss.h) + `towerMountSide` field, defaulting to
`TowerShelf` (0) so every existing saved file keeps its current meaning
unchanged. Wired through everywhere a tower mount is read or written:

- `MonitorProperties::fixtureRigPosition()` (engine/src/monitorproperties.cpp)
  derives Z from `tower->height()` (Top) or `0` (Bottom) instead of
  `shelfPos()` when mount side isn't Shelf — same function every renderer/
  aim-solver already calls, so this is correct everywhere for free.
- XML save/load: new `TSide` attribute alongside the existing `Tower`/
  `TShelf`/`TU`/`TV` ones; absent (old file) → defaults to `TowerShelf`.
- Canvas right-click "Mount on Tower ▶ <tower>" submenu
  (`ui/src/monitor/monitorgraphicsview.cpp`) gained "Top of tower"/"Bottom
  of tower" entries above the per-shelf list, both checkable/showing
  current state like the shelf entries already do. Picking either forces
  `mountingType` back to `FloorMounted` — "hung" has no meaning at the very
  top or bottom of a tower (nothing above/below to hang under/from).
- The tree-drop attach fix from earlier this session
  (`attachFixtureToTower()`) and the Structure Studio "Add Fixtures…"
  picker (`Monitor::mountFixtureOnStructure()`) both now explicitly reset
  `towerMountSide` back to `TowerShelf` on a fresh/plain attach — without
  this, a fixture previously mounted at Top/Bottom that got drag-attached
  or picker-attached to a tower would silently keep rendering at its old
  Top/Bottom position instead of landing on the shelf the action implied.
- The rig Properties dialog's "Mounted on:" label now reads "Tower N ·
  top" / "· bottom" / "· shelf N" instead of always assuming a shelf index.

Builds clean (full engine+UI rebuild — `truss.h` is widely included). Full
`check-all.sh` gate run after (see job result). Not yet verified live —
needs a real check: right-click a fixture, Mount on Tower → Top of tower,
confirm it renders sitting on the tower's cap (not a phantom shelf 0);
same for Bottom; re-attach the same fixture to a shelf afterward and
confirm it comes off Top/Bottom correctly, not stuck.

## Tower editor: fixture hit-testing didn't match the new body shape — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "I can't seem to grab lights in the tower editor and move them
consistently." Self-inflicted regression from the previous round, found by
reading `hitTestFixture()` rather than guessing: it still measured distance
to the OLD generic bar-line (`fixtureEndA()`→`fixtureEndB()`, which for a
tower-shelf fixture lies flat in the XY plane and barely projects onto an
elevation view at all) while `drawFixtures()` was, since the last round,
drawing an entirely different trapezoid body extending 10-20px above/below
that line. Clicking where the fixture visibly IS mostly missed; clicking
near the old invisible line sometimes hit — exactly "can't grab... 
consistently."

**Fix**: new shared `StructureStudioView::towerFixtureBodyRect(quint32)`
(`ui/src/monitor/structurestudioview.{h,cpp}`) computes the tower-shelf
body's screen rect once; both `drawFixtures()` (derives its trapezoid
corners from the rect) and `hitTestFixture()` (hit-tests against the same
rect, padded a few px for a grab margin) now read from the identical
geometry, so what's drawn and what's clickable can't drift apart again the
way they just did.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: click directly on
a tower-shelf fixture's visible body (not just near the shelf line) in
Front/Side view and confirm it selects/drags reliably.

**Separately answered, not a code change**: "still can't put on the top or
on the bottom" — correct, this is the `towerMountSide` (Shelf/Top/Bottom)
field discussed earlier in the session; still not built, still awaiting a
go-ahead. "How do I edit the tower to add/move shelves?" — **Add** already
works (the Shelves list + spinbox + "Add shelf" button in the Tower
editor's Geometry panel). **Move (edit an existing shelf's height) does
not exist at all** — `Tower` only has `addShelf()`/`removeShelf()`, no
`setShelfHeight()`; today the only way to reposition one is delete +
re-add. Checked before promising a quick fix: shelves are stored as a
plain sorted `QList<float>` with **no stable identity** — `addShelf()`
re-sorts the whole list every time, and a fixture's `towerShelf` is just an
index into it. So `removeShelf()` **already** silently reassigns which
physical shelf every fixture at a higher index refers to today (shelf 3
removed → whatever was shelf 4 is now shelf 3, and any fixture that was
mounted on the OLD shelf 4 now silently reads as being on the new shelf 3,
wrong height). Adding a "move" action the same way (edit height + re-sort)
would extend that same pre-existing reindexing hazard rather than fix
something clean — flagging this now rather than quietly building a "move
shelf" feature on top of it. Worth deciding together: give shelves a
stable id (bigger change, fixes the removeShelf hazard too) vs. accept the
index-reassignment behavior as-is and just add the height-edit UI on top
of it.

Branson, refining the marker from the previous round: "when we have
something on shelf .. it should have some size and show sitting on top ..
and when hanging should show inverted from the shelf." The small triangle
marker from the prior fix wasn't it — he wants the fixture's own drawn
shape to read as a body resting on (or hanging from) the shelf, not a
generic bar with a tiny separate icon next to it.

**What every OTHER mount already draws vs. what a tower shelf needs**:
every fixture, regardless of mount kind, was drawn via the same generic
"bar" (`fixtureEndA()`→`fixtureEndB()`, a line the length of the fixture's
real physical width, oriented by `studioMount`/`studioAngle`) plus small
head-dots — sensible for a truss/pipe/boom mount, where a fixture really is
"a bar running along something." A shelf-mounted fixture isn't that; it's a
free-standing unit resting on (or hung under) a surface, so the same bar
representation read as a floating line near the shelf regardless of
orientation.

**Fix** (`StructureStudioView::drawFixtures()`,
`ui/src/monitor/structurestudioview.cpp`): a tower-shelf-mounted fixture,
in Front/Side view, now skips the generic bar entirely and draws a sized
trapezoid instead — base flush with the shelf line, real width from
`fixtureLenM(fid)` (the fixture's actual declared physical width, same
source the bar used) scaled to the canvas, tapering toward the free end.
**Sitting** (`FloorMounted`): body rises above the shelf, base down — reads
as standing on it. **Hanging** (`TopHung`): the exact same shape, mirrored
vertically — base still flush with the shelf, body hangs below it. Same
shape, flipped, is what actually makes "hung" look inverted rather than
just "the same icon, a bit lower," which was the gap in the previous
triangle-marker attempt. Selection highlight and the name label both carry
over from the generic path. Every other mount kind (truss/pipe/platform/Top
view) is untouched — this only replaces the bar for the specific
tower-shelf + elevation-view case.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: a tower-shelf
fixture set to "On shelf (upright)" should show a body sitting above the
shelf line; switching its Orientation to "Under shelf (hung)" should flip
it to hang below, mirrored, not just nudge down slightly.

---

## Tower editor: shelf labels + a hang/sit visual marker — SHIPPED, not yet Branson-verified (2026-09-07)

Branson, mid-thought: "when editing a tower I can't drag a fixture to a
different height on the tower .. need to also specify hanging from shelf or
sitting on shelf[[typo]] sorry.. that's there.. needs to be visual" then,
separately: "we need labels on the tower editor to identify which shelf is
which." Investigated before building anything, since the self-correction
made the actual ask ambiguous on its own.

**Confirmed what already exists (the "sorry, that's there" part)**:
`StructureStudioView::dragFixtureTo()` (`ui/src/monitor/
structurestudioview.cpp`) already snaps a dragged tower-mounted fixture to
the nearest shelf **by height** — but only registers as a height drag in
the Front/Side plane (`m_plane`), which a tower's editor defaults to
**Top** (footprint view, where height isn't visible at all — `m_plane =
(kind == TowerKind) ? Top : Front;`). A `View: [Top/Front/Side]` combo
already exists in the editor toolbar (`Monitor::makeStudioPane()`) to
switch. Separately, "hang from shelf" vs "sit on shelf" already exists too
— `FixtureRigProps::mountingType` (`Truss::TopHung`/`FloorMounted`), set via
the rig Properties dialog's "Orientation" combo (`Under shelf (hung)` /
`On shelf (upright)`), and `MonitorProperties::fixtureRigPosition()`
already nudges a hung fixture's Z down 0.15 m from the shelf. So: none of
the underlying mechanisms were missing — matches his own correction.

**What was actually missing ("needs to be visual")**: confirmed by reading
`drawFixtures()`/the Tower elevation-rendering block — shelves drew as
plain unlabelled horizontal lines, and the 0.15 m hang/sit Z nudge had zero
dedicated visual cue (easy to miss at this canvas's usual zoom level, and
nothing to look at while STILL deciding where to drop). Two additions,
both in `ui/src/monitor/structurestudioview.cpp`:
1. Each shelf line in Front/Side view is now labelled "Shelf N — height"
   (respects the workspace's ft/m unit setting) — directly answers "labels
   ... to identify which shelf is which."
2. `drawFixtures()` now draws a small triangle marker at every tower-shelf-
   mounted fixture, pointing away from the shelf plane — up when sitting
   on it (`FloorMounted`), down when hanging under it (`TopHung`) — an
   explicit, unambiguous cue instead of relying on the subtle position
   nudge alone.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: open a tower with
2+ shelves in its editor, switch to Front or Side view, confirm each shelf
line now shows its number/height, drag a fixture between shelves and
confirm it's obvious which one it landed on, and toggle its Orientation
between "On shelf"/"Under shelf" to confirm the triangle marker flips.

**Not done — separate, bigger design items raised in the same conversation,
deliberately not started without Branson's sign-off**: (1) whether fixture
attachment should move toward an explicit menu (the canvas already has one
— "Attach to Truss"/"Mount on Boom"/"Mount on Tower" — just not surfaced in
the Layers panel yet) instead of continuing to harden drag-and-drop; (2) a
dedicated `towerMountSide` (Shelf/Top/Bottom) field so a tower's cap and
base are distinct, real mount positions rather than fake shelf indices.
Both discussed, neither built — real scope, not one-line fixes.

---

## Layers panel: dragging a fixture onto a tower/pipe, or a not-yet-grouped truss, didn't attach it — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "if I drag lights on a tower or truss etc in layers .. it should
attach them." Root-caused before touching anything — two distinct, real
gaps in `MonitorLayersPanel::handleTreeDrop()`
(`ui/src/monitor/monitorlayerspanel.cpp`), not one.

**Gap 1**: the existing attach-on-drop logic only fired when the drop
target was a **GROUP node anchored to a truss** (`g.anchorKind ==
"truss"`) — but a truss only GETS an anchored group once it already has at
least one fixture on it (`ensureTrussGroup()`, called from
`attachFixtureToTruss()` itself). A fresh, empty truss is just a plain
tree item until then, so dropping the FIRST fixture onto it hit none of
this — it silently fell through to "move to this layer," never attaching.

**Gap 2**: pipe and tower were never handled by the group-anchor check at
all, even though `attachFixtureToPipe()` already existed
(`ui/src/monitor/monitorgraphicsview.{h,cpp}`) for the canvas drag-drop
path — and there was **no `attachFixtureToTower()` at all**; a tower's only
attach path anywhere in the app was the per-shelf right-click menu.
Checked first — confirmed pipe/tower have no "anchored group" concept in
this codebase at all (only `ensureTrussGroup()`/`ensurePlatformGroup()`
exist), so generalizing the group-anchor check itself wouldn't have helped
either kind.

**Fix**:
1. New `MonitorGraphicsView::attachFixtureToTower(quint32 fid, quint32
   towerId)`, mirroring `attachFixtureToPipe()`'s shape — mounts on shelf 0
   / shelf-centred by default (matching the existing per-shelf menu's own
   default), unless the fixture is already on that exact tower, in which
   case its current shelf is kept.
2. `handleTreeDrop()` now resolves an attach target from the drop **item
   itself** first (any bare truss/pipe/tower row, grouped or not — covers
   gap 1 for all three kinds and gap 2 for pipe/tower directly), falling
   back to the truss-anchored-GROUP check only when the direct-item check
   comes up empty (an already-populated truss, dropped via its group
   node — unchanged behavior, still truss-only since that's the only kind
   with a group concept).

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: drag a fixture
straight onto a brand-new, empty truss's row, then onto a tower's row,
then onto a pipe's row, and confirm all three attach (not just move to
that layer) — plus that dragging onto an already-populated truss (via its
group) still works as before.

---

## Layers panel: bulk "Lock position"/"Unlock position" on a multi-selection — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "can we add the ability to lock stage features inside a layer? So
we can make sure they can't impact move/attach?" The underlying protection
already existed and already covers both halves of this — a locked truss/
platform/pipe/stand/tower/target/image/power source can't be dragged
(`MonitorGraphicsView::refreshItemLayerState()`) **and** already refuses a
NEW fixture attaching onto it (`trussUnderFixture(..., forAttach=true)`
skips locked trusses, from the "locked-truss refusal" work already on this
branch) — but the "Lock position" toggle that sets it was only reachable
one object at a time, from each type's own single-item context menu. Ask
was specifically about doing this for everything on/selected within a
layer at once.

Added bulk "Lock position"/"Unlock position" to the Layers panel's existing
multi-select context menu (same block as "Move to layer" and "New Layer
from selection…", `ui/src/monitor/monitorlayerspanel.cpp`) — two separate
actions rather than one state-dependent toggle, since a mixed-state
selection (some already locked, some not) makes a single toggle's "current
state" ambiguous; "Lock position" locks every selected lockable object,
"Unlock position" unlocks all of them, both reusing the existing
`setObjectLocked()`/`kindLockable()` dispatch (no new per-kind logic).
Fixtures are silently excluded from the filtered list — per the existing
model they have no per-item lock at all (only layer/group/global lock), so
offering an action that would silently do nothing for them would be worse
than not offering it.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: multi-select a
mix of trusses/platforms/etc. (plus a fixture, to confirm it's correctly
excluded from having any effect), "Lock position", then confirm none of
the locked ones can be dragged AND a free fixture can no longer attach onto
a locked truss.

---

## Layers panel: "New Layer from selection…" — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "in layres can we have multi select 'new layre from selection'."
The Layers panel's multi-select context menu (`ui/src/monitor/
monitorlayerspanel.cpp`) already had the two halves of this — "New Folder
from selection…" (create + populate a folder in one step) right above, and
a "Move to layer" submenu listing existing layers — just not the one-step
combined version for layers themselves, which is what was asked for:
select 2+ objects, get a brand-new layer with exactly that selection on it,
without first creating an empty layer and then separately moving things
onto it.

Added "New Layer from selection…" to that same multi-select menu block,
right after "Move to layer" — same shape as "New Folder from selection…":
prompts for a name (`QInputDialog`, defaulting to "Layer N"), then
`MonitorProperties::addLayer(name)` + `setActiveLayerId()` (matching
`slotAddLayer()`'s own behavior) + `MonitorGraphicsView::reparentToLayer
(objs, lid)` — the same call "Move to layer" already uses, which addresses
items by kind+id rather than requiring them to be selected/visible in the
current view — then `m_focusLayerAfterReload` so the tree opens straight to
the new layer after `reload()`, matching "New Folder from selection…"'s own
post-creation UX.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: multi-select 2+
objects in the Layers tree (or canvas), right-click → "New Layer from
selection…", name it, confirm a new layer appears with exactly those
objects on it and the tree scrolls/expands to show it.

---

## Detached tab windows (Lighting Studio) landed unusably tiny when a workspace opens on a different machine — SHIPPED, not yet Branson-verified (2026-09-07)

Branson: "we need a way to redirect windows that were on a different screen
on a different machine when last saved" — then, after the first pass below
didn't fix it: "restarted and the window is still off screen .. we need a
way to pop it back in to the current screen .. interestingly when I select
lighting studio under view it opens the functions tab in the main window?"
Root-caused for real using live evidence (System Events window inspection
of the actual running process) rather than trusting the first theory,
matching this session's own "stop guessing" standard.

**First pass (real, but not THE bug)**: a workspace's `DetachedWindow`
entries (`App::loadXML()`, `ui/src/app.cpp`) store each detached tab's raw
`QWidget::saveGeometry()` blob *inside the .qxw file* — unlike the main
window's geometry, which lives in local `QSettings` and never leaves this
machine. The main window already clamped its own restored geometry onto
whichever screen sits under it; detached tab windows never got the same
treatment. Fixed that gap — necessary, but Branson's report after
rebuilding proved it wasn't sufficient, so investigated further instead of
declaring victory.

**Actual root cause, confirmed live**: queried the real running process's
windows directly (`osascript`/System Events, not a guess) —
`test-workspaces/stage-structures-demo.qxw` has `CurrentWindow=
"FunctionManager"` (so Functions correctly showing active is NOT a bug,
that's literally what the file says) plus a `DetachedWindow class="Monitor"
geometry="..."` entry. The live process showed the main window normally on
screen, **plus a second, UNNAMED window at the exact same position, sized
100×62 points** — Lighting Studio's detached window, restored to a
practically-invisible sliver. `QWidget::restoreGeometry()` can *fail
outright* (Qt version/format mismatch between the saving and loading
machine, or corruption) and silently leaves the widget at whatever tiny
size it had right after construction — my first-pass clamp only bounded
size from ABOVE (`boundedTo`), so a too-SMALL restored/default size sailed
right through untouched. That also explains the View-menu report:
`Monitor::createAndShow()` (`ui/src/monitor/monitor.cpp`) correctly detects
the tab is detached and raises that window — but raising a 100×62 sliver is
visually indistinguishable from nothing happening, so it just looked like
clicking "Lighting Studio" left Functions showing.

**Fix**: moved the clamp logic into a proper shared utility,
`AppUtil::ensureWindowOnScreen(QWidget*, QSize minSize = {400,300})`
(`ui/src/apputil.{h,cpp}`) — finds the screen under the window's centre
(falling back to primary), then **both** floors the size up to `minSize`
**and** clamps it down to fit the screen's available area, then
repositions so the whole rect fits inside it. Wired in three places: the
main window's own restore (`App::init()`, was already using the old
private version), the `DetachedWindow` restore loop (`App::loadXML()`),
and — this is the actual "pop it back" mechanism Branson asked for —
`Monitor::createAndShow()`'s raise-if-detached branch now calls it every
time, unconditionally. That makes View → Lighting Studio (Ctrl+Shift+M)
self-healing: any time it's invoked, even against a window that was
already broken before this fix shipped, on a workspace already loaded, it
fixes the geometry before raising rather than only fixing it at load time.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check on the SAME
already-running process/workspace that surfaced this: choose View →
Lighting Studio (or Ctrl+Shift+M) and confirm the detached window actually
becomes visible and usable-sized, not just raised-but-invisible.

---

## Right-click "Locate" and "Reset" directly on a fixture — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "be nice to have reset as a right click option on a fixture" /
"be nice to have locate as a right click option on a fixture" — both
already existed one level deep (inside the rig Properties dialog, or the
old "Test Fixture..." quick dialog's own buttons); this flattens them onto
the context menu directly, no dialog required.

**`Monitor::locateFixture(quint32)` / `Monitor::resetFixture(quint32)`**
(`ui/src/monitor/monitor.{h,cpp}`) — new public methods, reusing the
existing mechanisms rather than duplicating their DMX logic:
- `locateFixture()` drives the same `FixtureLocate` 3-flash sequence the
  rig editor's own "Locate" button uses, but self-owned instead of
  dialog-scoped — a new private `Monitor::LocateSession` (forward-declared
  in the header, defined next to `FixtureLocate` in the .cpp since it holds
  one) + `QHash<quint32, LocateSession*> m_locateSessions` tracks one
  in-progress flash per fixture id, so triggering it from a right-click
  works with no dialog open at all. Calling it again on a fixture that's
  still flashing cancels early (matches the rig editor's own button
  toggle-off behavior). `Monitor::~Monitor()` now calls a new
  `cancelAllLocateSessions()` first, so an in-progress flash can't outlive
  the Monitor and leak its Override fader.
- `resetFixture()` reuses the exact same Maintenance-channel "reset"
  capability detection the old "Test Fixture..." quick-dialog already had
  (scan for a `QLCChannel::Maintenance`-group channel with a capability
  name containing "reset", write its range midpoint once) — duplicated
  rather than shared with that dialog's copy, since the dialog also
  handles Identify, which this action doesn't.

**`FixtureManager`** (`ui/src/fixturemanager.{h,cpp}`): two new actions,
`m_locateAction`/`m_resetAction`, enabled under the exact same single-
fixture-selected gating as the existing `m_testAction` (both Design and
Operate mode — Locate/Reset are hardware diagnostics, useful in either),
inserted into the context menu right after "Test Fixture...". Both actions
show unconditionally for a single selected fixture (same as Test Fixture)
rather than being gated on the fixture actually having a reset capability —
matches this exact file's own existing precedent (Test Fixture's outer menu
action doesn't check for Maintenance capabilities either; its own dialog
just shows nothing extra if there aren't any).

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — needs a real check: right-click a
fixture, confirm Locate flashes it 3x and Reset sends its reset command
(if it has one) — including confirming Locate started from a right-click
still cancels cleanly if clicked again mid-flash.

## New target: landed on the wrong layer and wasn't shown in the Layers tree — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "also added a target .. should show up in layers on the current
layre we're on. And also be visible on the stage .. can't see it." Two
separate things bundled in one report — root-caused each before touching
anything.

**Layer assignment — a real, clear bug, fixed.** `Monitor::slotAddTarget()`
(`ui/src/monitor.cpp`) was the ONE stage-object creation slot that never
called `setLayerId(m_props->activeLayerId())` — every sibling (`slotAddPipe`,
`slotAddStand`, `slotAddTruss`, `slotAddPlatform`, `slotAddTower`, …) sets
this immediately after positioning the new object; `slotAddTarget()` simply
never did, so a new target always silently landed on layer 0 ("Default")
regardless of which layer was actually active — exactly why it didn't show
up under "the current layer we're on" in the tree. It was ALSO missing the
`if (m_layersPanel) m_layersPanel->reload()` call every sibling makes right
after, so even a target that did land on the right layer wouldn't refresh
the tree to show it until something else happened to trigger a reload.
Fixed: added both lines, matching the established pattern exactly.

**Visibility on the 2D canvas — investigated, NOT changed, flagging as a
design question rather than deciding unilaterally.** `MonitorGraphicsView::
updateTargets()` (`ui/src/monitor/monitorgraphicsview.cpp:2962-2977`)
deliberately shows a `StageTarget` ONLY when it's referenced by an
`Aim`-type `QLCPalette` that belongs to the scene currently focused in the
Programming tab (`m_activeSceneId`) — explicit comment: "With no scene
focused there is nothing to aim, so show nothing." This makes a target the
ONE stage-object type that's conditionally hidden by default; every other
kind (truss/platform/pipe/stand/tower/fixture) is unconditionally visible
once placed. Compounding this: `slotAddTarget()`'s own auto-created
companion palette is typed `QLCPalette::PanTilt`, not `QLCPalette::Aim` —
per an explicit comment elsewhere in the codebase
(`programmingmanager.cpp:1027`, "`case QLCPalette::Aim: break; // no
m_values; target set in LookEditor`"), `stageTargetId()` is only meaningful
on an Aim-type palette; setting it on a PanTilt one (as `slotAddTarget()`
does) is inert, dead data. So a fresh target never gets a working Aim
palette at all, and even if it did, that palette would still need adding to
whichever scene is currently focused to satisfy the visibility gate above.
**Not changed** — whether targets should default to always-visible (like
every other stage object) or the auto-created palette should be Aim-typed
and auto-attached to the focused scene are real design decisions with more
than one reasonable answer, not a one-line bug fix; flagged here for
Branson to decide rather than guessed at.

Builds clean (`qlcconsole` target, layer-assignment fix only). Full
`check-all.sh` gate run after (see job result). Not yet verified live —
needs a real check: add a target while a non-Default layer is active,
confirm it appears immediately under that layer in the Layers tree.

## Front (elevation) view: moving things relative to a truss — investigated, NOT YET fixed, needs Branson to confirm which case (2026-09-05)

Branson: "also in front view .. can't move things relative to the truss ..
should be able to do that." Traced the relevant gating in
`ui/src/monitor/monitorgraphicsview.cpp` rather than guess a fix:

- A **fixture already bound to a truss** IS elevation-draggable today
  (`elevationFixtureDraggable()`, `:2327-2333` — slides it along the truss,
  handled in `slotFixtureMoved()`'s elevation branch, `:4450-4488`) — this
  path looks structurally present and correct on read-through.
- A fixture mounted on a **pipe or tower** instead of a truss is NOT
  elevation-draggable at all — `elevationFixtureDraggable()` only checks
  `rp.trussId`, never `rp.pipeId`/`rp.towerId`. If the fixture in question
  is actually pipe/tower-mounted (easy to conflate with "truss" informally),
  this would fully explain "can't move."
- A **child bar/crossbar** (e.g. a batten hung off a larger truss) is only
  elevation-draggable when its PARENT is a vertical tower
  (`elevationBarDraggable()`, `:2335-2346`,
  `parent->type() == Truss::Vertical`) — one hung off a horizontal truss
  isn't elevation-draggable at all.

Three genuinely different mechanisms, three different fixes, and "things"
in the report doesn't pin down which one Branson actually hit. Rather than
patch all three speculatively, need one concrete detail back: what exactly
was being dragged (a fixture, or a bar/crossbar?) and what is IT actually
mounted to (a truss, a pipe, a tower)? No code changed for this item yet.

## Fixture rig editor: "Locate" was hidden for LED bars/washes — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "also .. led bars and washes need a locate button." Confirmed and
fixed directly — a clear, unambiguous UI gap, not a design question.

**Root cause**: `Monitor::showFixtureItemEditor()` (`ui/src/monitor.cpp`)
bundled the "Locate" button (flash-to-identify — useful for ANY fixture)
into the same `testRowWidget` as the Pan/Tilt-specific "Test Orientation"
controls (mode combo, target combo, "Test" toggle). That whole row gets
hidden with `hideRow(testRowWidget)` for any fixture with no Pan or Tilt
channel (`!isMover`) — correct for the orientation-test controls themselves
(meaningless without Pan/Tilt), but it took Locate down with it, even
though `FixtureLocate` (the class behind the button) never touches Pan/Tilt
at all — it only sets intensity/colour channels + shutter, which every
fixture has some form of.

**Fix**: split Locate out into its own row (`locateRowWidget`, a plain
`QHBoxLayout` holding just the button), added via a separate `rigForm->
addRow(tr("Identify:"), ...)` call, outside the `!isMover` hide block. Test
Orientation (mode/target/Test button) stays mover-only exactly as before;
Locate is now unconditional.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — same collision-avoidance as the
entries above — needs a real check: open the rig editor for an LED bar or
wash (no Pan/Tilt), confirm "Identify:" / Locate is now visible and flashes
the fixture.

## Fixture rig editor's "Test" — investigated a report that it doesn't light an American DJ Focus Spot Three Z; code looks correct, root cause NOT YET confirmed (2026-09-05)

Branson: "on the 3z when I do test it doesn't light .. It has intensity and
shutter does test activeate both?" ("the 3z" = the patched **American DJ
Focus Spot Three Z**, confirmed from the workspace's fixture list — not a
channel or setting name).

**Direct answer to the question asked**: yes, by design, both "Test" and
"Locate" are supposed to drive intensity AND shutter together —
`appendFixtureOnValues()` (`ui/src/monitor.cpp`) sets the fixture's
detected master-intensity channel (or every Colour-group channel, for an
RGB fixture) to 255, THEN separately scans for a Shutter-group channel
whose capability name contains "open" (and not "strobe"/"close") and sets
it to that capability's midpoint value — both channels, one call, used by
both features.

**Investigated the Three Z's own bundled definition specifically** (not
guessed) to rule out the obvious "fixture data misclassified" explanation
that fits this shape of bug so well elsewhere: confirmed byte-identical to
`~/git/qlcplus`'s copy (not a fork-introduced issue either way). Its
"Master Dimmer" channel uses `Preset="IntensityMasterDimmer"`, which
resolves to `Group=Intensity, MSB, NoColour` (`qlcchannel.cpp:172-174`) —
exactly what both `masterIntensityChannel()` and this feature's own
fallback loop look for. Its "Strobe/Shutter" channel's first "Open"
capability (DMX 8-15, `Preset="ShutterOpen"`) has a clean name with neither
"strobe" nor "close" in it, so the exclusion filter doesn't wrongly skip
it. Traced the toggle-on path too (`testBtn`'s `toggled` connection →
`refreshTest()` → `FixtureOrientationTest::setDegrees()`) — it does call
`appendFixtureOnValues()` immediately, not just on a later change.

**So the fixture definition and the write path both look structurally
correct** — I don't have a confirmed root cause, and won't guess one just
to have shipped something, per the standing "stop guessing" bar from
earlier this session. No code changed for this specific report. To narrow
it down, need from Branson: (1) does the fixture's **Pan/Tilt actually
move** when Test is toggled on (proves the Override-priority fader is
reaching the universe at all, vs. nothing reaching it) — if pan/tilt moves
but the light stays dark, that's the more interesting half of this bug;
(2) a DMX monitor/console reading of the fixture's Master Dimmer + Strobe/
Shutter channel values while Test is toggled on, if available at the rig.

---

## Lighting Studio: opening/saving the fixture editor corrupted a truss-bound fixture's dragged position — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "everytime I save the eidtor it moves the fixture position .. I
think it's because the relative position I move it to on the truss isn't
stored in the fixture def and when I open it it's wrong and so when it
saves it it's wrong." His hypothesis pointed at the right area (the rig
editor's truss-position fields); root-caused the exact mechanism before
touching anything — turned out to be **two** independent bugs in
`Monitor::showFixtureItemEditor()` (`ui/src/monitor/monitor.cpp`), both
firing on essentially every save of a fixture with a non-zero across-truss
offset.

**Bug 1 — the "Across truss" combo silently re-buckets a continuous value.**
Canvas drag-to-any-position (shipped 2026-08-17, see the truss-geometry
entries in DONE.md) lets `FixtureRigProps::trussCross` be any continuous
float — a fixture can sit anywhere across a truss's width, not just
dead-centre. But this dialog's "Across truss" field is only a 3-item combo
(Left / Centered / Right, ±half the truss width or 0) — and Save
**unconditionally** re-derived `newRp.trussCross` from whichever of those 3
buckets the combo happened to be sitting on, discarding the real continuous
value every single time the dialog was accepted, whether or not the user
ever touched that control. Loading the dialog already lossy-bucketed
whatever continuous value was there (`rp.trussCross < 0 ? -1 : ...`), so
this was a guaranteed round-trip loss: open → snap to a bucket → save →
overwrite the real value with that bucket.

Fix: capture the combo's initially-loaded index
(`initialTrussCrossIndex`) right after loading it. Save now only
re-derives `newRp.trussCross` from the combo when its index actually
**changed** from that — an untouched control leaves `newRp.trussCross`
exactly as it started (it's a copy of the existing rig,
`FixtureRigProps newRp = m_props->fixtureRigProps(...)`, so simply not
writing to it preserves the original continuous value). Left/Centered/Right
still work as explicit quick-set actions when actually chosen.

**Bug 2 — the post-save reposition math drops the cross offset entirely.**
Separately, whenever the truss binding or along-truss offset changed (which
also happens spuriously from float round-trip through the offset spinbox's
display-unit conversion, so this fired more often than it looked like it
should), the dialog recomputed the fixture's stored/on-screen XY via a raw
`Truss::positionAt(newRp.trussOffset)` — the truss **centerline only**,
with no cross term at all. This is the exact same "positionAt() ignores
cross" shape of bug the 2026-08-17 truss work already found and fixed for
canvas dragging (`slotTrussMoved()`, switched to
`MonitorProperties::fixtureRigPosition()`) — but this dialog's own
reposition code was never updated to match, so it kept silently recentring
any off-centre fixture back onto the truss's centerline on save.

Fix: swapped the raw `t->positionAt(newRp.trussOffset)` call for
`m_props->fixtureRigPosition(fxItem->fixtureID())` — `setFixtureRigProps
(newRp)` already ran a few lines above this, so this reads the
just-stored rig through the same authoritative position derivation
`aimsolver.cpp`/`effectinstance.cpp` already trust (cross offset, mount-side
Z, and the mount height nudge all included), instead of a narrower
duplicate that drops terms.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — same collision-avoidance as the
entries above (Branson has his own instance running) — needs a real check:
drag a truss-bound fixture to an off-centre cross position, open its
Properties dialog without touching "Across truss," hit OK, and confirm the
fixture stays exactly where it was dragged instead of snapping back toward
the truss centerline.

---

## Lighting Studio: triple-click a grouped/truss-bound fixture to open its editor — SHIPPED, not yet Branson-verified (2026-09-05)

Branson, right after the truss-double-click fix above: "I am fine with
isolation selection .. but a triple click should open the fixture editor."
Confirms the drill-in (double-click isolates) behavior is wanted as-is —
the ask is a faster way to reach the editor afterward than a whole separate
second double-click.

**Mechanism**: Qt has no native triple-click event — a real third rapid
press just arrives at `mousePressEvent()` as an ordinary
`QEvent::MouseButtonPress` (Qt only ever turns the *second* press within
`QApplication::doubleClickInterval()` into `QEvent::MouseButtonDblClick`; a
third press gets no special treatment from Qt itself). So detecting a
triple-click means the double-click handler has to leave a breadcrumb for
the press handler to notice.

**Implementation**: when `mouseDoubleClickEvent()`'s isolate branch fires
(a grouped/truss-bound fixture, first double-click, drills in rather than
opening an editor), it now also records the fixture + a running
`QElapsedTimer` (`m_lastFixtureDoubleClickItem`/`m_lastFixtureDoubleClickTimer`,
new members in `monitorgraphicsview.h`). `mousePressEvent()` checks this
first, before anything else: if a new left-press lands on that SAME
fixture (resolved via `topPickableAt()`, the identical ghosted-item-aware
hit-test the double-click handler itself uses, not plain `itemAt()`) within
`QApplication::doubleClickInterval()` of that timestamp, it's treated as
the triple-click — opens the fixture's own editor
(`emit fixtureDoubleClicked(fi->fixtureID())`) immediately, sets
`m_suppressNextViewClick` (same convention the double-click path already
uses so the paired release doesn't immediately close what was just
opened), and consumes the press (no call to the base class, matching how
every other "handled" branch in this same double-click/press code already
works — never calling the Qt base handler when a real item was hit).
Deliberately scoped to just fixtures (what was asked) — the same "single
click groups, double click isolates" pattern also exists for bare
truss/platform/pipe/stand/tower selection (`drilledIntoGroup()`), so the
same triple-click trick could extend there too if ever wanted, but wasn't
built now since it wasn't asked for.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — same collision-avoidance as the two
entries above (Branson has his own instance running) — needs a real check:
single-click a truss's fixture group (whole thing highlights), double-click
one fixture (isolates just it, nothing opens), then a further quick click
on that same fixture (should open its editor immediately — no need for a
second full double-click).

---

## Lighting Studio: double-clicking a truss-bound fixture always edited the truss — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "when adding a fixture to a truss on the studio .. when I
doubleclick the fixture, it edits the truss." Root-caused before touching
anything.

**Root cause**: `MonitorGraphicsView::mouseDoubleClickEvent()`
(`ui/src/monitor/monitorgraphicsview.cpp:3856-3903`) had two competing rules
for a truss-bound fixture, and the wrong one always won. An early,
unconditional block ("a fixture mounted ON a feature → open that feature's
editor," written for the platform/riser case where the mounting feature is
literally covered by its own fixtures) checked `rp.trussId` **first** and
returned immediately — before ever reaching the later, more specific block
built for exactly this scenario: "grouped or truss-bound fixture: the FIRST
double-click drills in (selects just this fixture, distinct highlight) so
it can be dragged/detached; a SECOND double-click, now isolated, opens the
fixture's own editor." That second block's `fi->isBoundToTruss()` condition
was real, working code — just permanently unreachable, because the early
`rp.trussId` check above it fired first on every single double-click,
truss-bound or not, isolated or not, and returned before the isolation
check ever ran.

**Fix**: removed the `rp.trussId` branch from the early "open the feature's
editor" block. Truss binding now falls straight through to the existing
drill-in-then-edit logic, which was already correct and already built for
this. Pipe/tower/platform/riser mounts keep the old immediate-redirect
behavior — deliberately not touched, since those features really can be
fully covered by their own mounted fixture (no uncovered spot left to
double-click directly), unlike a truss, which is a long mostly-uncovered
bar you can still double-click elsewhere on to reach its own editor.

**Noticed, not fixed (same shape of bug, not what was reported)**: the same
early-return ordering issue could in principle also make a *grouped*
platform/pipe/tower-mounted fixture's drill-in unreachable (`itemGroupId(fi)
!= 0` is also checked by the later block, but the earlier platform/pipe/
tower checks fire regardless of group membership) — not confirmed as an
actual live bug, since it depends on a fixture being both mounted AND
manually grouped, and not raised by Branson; flagged here rather than
guessed at or fixed unprompted.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after (see
job result). Not yet verified live — Branson had his own instance running
throughout, so this shipped without relaunching to avoid colliding with it
— needs a real check: double-click a truss-bound fixture once (should
isolate/highlight just that fixture, not open anything), double-click it
again (should now open the fixture's own editor, not the truss's).

---

## Fixtures page: a universe patched in Connections didn't appear there at all — SHIPPED, not yet Branson-verified (2026-09-05)

Branson: "if I patch a universe in the connections editor .. it doesn't
appear on the fixtures page." Investigated before touching anything, per the
standing "root-cause, don't guess" bar from earlier in this session.

**Root cause, confirmed by reading the code, not assumed**:
`FixtureTreeWidget::updateTree()` (`ui/src/fixturetreewidget.cpp:887`) only
ever built the "Universes" folder — and every universe row inside it — by
looping over **patched fixtures**. A universe row existed purely as a side
effect of a fixture happening to sit in it; a universe that was patched in
Connections/Devices but had no fixture assigned to it yet had **no row at
all**, not a hidden or greyed-out one. This broke the same "always present,
even with nothing in it yet" convention this file already uses for the
neighboring "Fixture Groups" and "Power" folders one section up — both exist
specifically so there's something to right-click and create the first
member on; "Universes" was the one folder that never got that treatment.

**Fix**: the "Universes" folder and one row per universe are now pre-built
from `Doc::inputOutputMap()->universesCount()` *before* the fixture loop
runs, so every patched universe shows up immediately regardless of fixture
count. The fixture loop then just fills in each universe's fixtures as
before; its old lazy-create-on-first-fixture path is kept, but now only
fires for the fixture-picker dialogs that don't show groups at all
(`m_showGroups == false`) — those never showed empty universes and don't
need to, since there's nothing to pick from one.

Builds clean (`qlcconsole` target). Full `check-all.sh` gate run after:
Qt6 and Qt6-Release both pass clean (Qt5 skipped, not installed on this
machine); `Qt6-Werror` fails at CMake *configure* time on an unrelated,
pre-existing break (`"dangling-else" is not known` as a warning category) —
confirmed unrelated by checking it's a configure-stage CMake error, not a
compile error in anything this change touched. Not yet verified live —
Branson had his own instance running against a live session throughout, so
this shipped without relaunching to avoid colliding with it (same collision
risk documented earlier this session) — needs a real check: patch a new
universe in Connections, switch to the Fixtures page without touching
anything else, confirm the new universe's row is there immediately.

---

## Fixture Manager — modernize + look at integrating with qlcconsole *(2026-09-02 audit, 2026-09-03 follow-up: both open questions answered + shipped, editor merge still open)*

Branson: look at Fixture Manager and see what can be done to modernize it and
maybe integrate it more with the rest of qlcconsole. Followed up same night
("run based on a discrete modernization... investigate integration with the
main tool... do this work in a branch independently where we can not impact
main") while Branson was asleep — full writeup, decisions, and what was
deliberately left alone: **[FIXTUREMANAGER_MODERNIZE_DESIGN.md]
(FIXTUREMANAGER_MODERNIZE_DESIGN.md)**. Branch:
`feat/fixturemanager-modernization` — **not merged to `main`**, needs
Branson's review first (both to sanity-check the shipped fix and to weigh in
on the two bigger open questions the doc lays out, neither of which was
resolved unilaterally).

Short version: the audit found the tree/CRUD structure itself is fine and
fork-original (not touched), RDM is fully separable and stays exactly as
dormant as it was (contributes nothing to current crowding — that's the
fork's own Power/folders/composite-rebuild features), and the one concrete,
single-answer fix was the context menu's construction order, which put a
fixed Properties/Test/Remove/Ungroup block first and unconditionally
(inherited from stock upstream QLC+) instead of following the
Connections/Devices convention this session established
(`appendUniversalMenuActions()` — specific-to-row first, generic last).
Fixed and traced line-by-line against the existing selection-based
enable/disable logic to confirm zero behavior regression. Two bigger
findings — the always-visible 15-action toolbar vs. Connections' toolbar-
free design, and three different UI routes to "put fixtures in a group"
with no menu-text distinction between assign-semantics and copy-semantics —
are real but are product decisions with more than one reasonable answer;
documented, not decided, in the design doc.

**Interactively verified** (2026-09-03, `cliclick` installed): right-clicked
the Power root for real — now shows only Add power source/Add fixture/Add
RGB panel/Add fixture to group, with Properties/Test Fixture/Delete items/
Remove fixture from group gone (previously listed disabled); a fixture row
still shows the full menu including all four, confirming the gating is
selection-aware, not a blanket removal.

### Follow-up: Branson answered the two open questions, plus new asks (2026-09-03) — toolbar/menu work SHIPPED, editor merge NOT started

Branson came back with five asks in one message, the last one flagged "ONE
MORE TIME" (frustration that the 2026-09-02 title-bar merge fix, marked
SHIPPED but never interactively verified, evidently hadn't actually fixed
what he was seeing). Asked three scoping questions via AskUserQuestion
before touching code, since two of the asks map directly onto this doc's
open questions and one is a brand-new, large architectural decision:

- **Fixture Editor integration → "Full tab/panel merge"** chosen. This is a
  big rearchitecture (the standalone `fixtureeditor/` app has its own
  window and its own fixture-definition model, currently has zero in-app
  entry point at all) and has **not been started** — needs its own
  investigation + design pass, tracked as a new item below this one.
- **Toolbar trim → "Trim to creation-only"** chosen (this doc's
  recommendation). **Shipped**, see below.
- **Three-routes-to-group wording → "Leave as-is for now"** — not touched.

**Title bar, actually root-caused this time.** The 2026-09-02 fix (locking
the toolbar to icon-only) reduced button height but didn't touch the real
cause: since macOS 11 (Big Sur), AppKit's default "unified" `NSToolbar`
style puts the window title on its own centered line above the toolbar row
whenever a window has more than a couple of toolbar items — Qt's
`setUnifiedTitleAndToolBarOnMac()` only merges the background; it has no
API for the older single-line "compact" style. Confirmed by screenshot
(traffic lights + title on one line, Stop/Blackout/Blind/Operate visibly
lower on a second line, inside the same dark unified background — easy to
mistake for "basically merged" from a quick glance, which is likely why the
2026-09-02 fix looked plausible without an actual screenshot check). Fixed
with a direct AppKit call: new `ui/src/mactoolbarstyle.{h,mm}`
(`macSetWindowToolbarStyleUnifiedCompact()`, sets
`NSWindowToolbarStyleUnifiedCompact` on the native `NSWindow`), called from
`App::initToolBar()` after forcing native window creation via `winId()`.
**Interactively verified**: before/after screenshots — title and toolbar
icons now share one row, matching Safari/Mail/Xcode's compact-toolbar look.

**`check-all.sh` caught a real regression from this fix**: both Qt6 and
Qt6-Release `check` runs SIGSEGV'd immediately in `autosave_test`'s
`initTestCase()` — which constructs a real `App` (and so hits
`initToolBar()`) under `QT_QPA_PLATFORM=offscreen` for headless test runs.
`macSetWindowToolbarStyleUnifiedCompact()` was reinterpret-casting
`QWindow::winId()` to `NSView*` unconditionally; under the offscreen QPA
platform that's never a real Cocoa view, so messaging `.window` on it
crashed (SEGV_ACCERR) — a nil check alone wouldn't have caught this, since
the garbage pointer wasn't nil, just not a valid Objective-C object. Fixed
by guarding on `QGuiApplication::platformName() == "cocoa"` before touching
anything AppKit-side. Re-ran `autosave_test` directly (7/7 PASS) and the
full `check-all.sh` gate after the fix.

**Fixture Manager toolbar, actually trimmed.** Turned out to be more
tangled than the doc's own preview implied — three tabs (Fixtures, Channel
Groups, RDM-dormant) share one toolbar with per-tab retargeted meaning, and
two categories of action didn't fit a plain "move to context menu": global/
doc-wide actions (Import/Export/Remap/Fade Config — not tied to any row),
and the Channel Groups tab, which had zero context menu of its own before
this. Asked two more scoping questions, both resolved toward full
consistency rather than partial/lower-risk options:

- **`ui/src/fixturemanager.{h,cpp}`**: toolbar now shows only creation
  actions per tab — an "Add ▾" dropdown (Add Fixture.../Add RGB panel...,
  consolidating what were two separate always-visible buttons) + "Add
  fixture to group..." on the Fixtures tab; a single "Add Channel Group..."
  on the Channel Groups tab (split out from the old shared `m_addAction`,
  which used to silently mean different things per tab with only a tooltip
  hinting at the swap — a real instance of the "unclear wording" Branson
  flagged). Properties/Test/Remove/Ungroup/Move Up/Down moved to context-
  menu-only; Expand/Collapse-all moved into the fixtures tree's context menu
  (matching Connections' convention). Channel Groups tab got a **new**
  context menu (`slotChannelGroupContextMenuRequested()`) it never had
  before — Properties/Move Up/Move Down/Remove, reusing the same tab-aware
  actions/slots the toolbar used, so no new business logic, just a new
  place to reach it from. Also renamed the inner "Fixture Groups" sub-tab to
  "Fixtures" — it actually holds the whole patch (fixtures, groups, power,
  universes), not just groups; the old label undersold its contents (a
  second, smaller "concise wording" fix).
- **`ui/src/app.{h,cpp}`**: new "Fixtures" menu bar entry (between View and
  Control) holding Import/Export/Remap/Fade Configuration —
  `FixtureManager::populateFixturesMenu()` builds it, called from
  `App::initMenuBar()`.

Builds clean. **Interactively verified**: toolbar screenshot (Add ▾ + Add
fixture to group ▾ only, tab relabeled), Fixtures menu bar contents via
accessibility API (Import/Export/Remap/separator/Fade Configuration), and
the fixtures-tree context menu including the new Expand/Collapse-all
entries, via a real right-click. **Not verified**: the new Channel Groups
context menu specifically (a `cliclick` coordinate slipped and landed a
single click in the VS Code window instead mid-session — harmless, no text
typed, but stopped further blind-coordinate GUI automation for this pass
rather than risk another one; the code is a direct mirror of the
already-verified fixtures-tree pattern, reusing its exact actions/slots).
Branson confirmed live (2026-09-05): the Add ▾ dropdown is there and working.

Icon differentiation (ask: "sub icons... identification via grouping") was
**not done** — the tree's Power root/sources/circuits and Universes root
all still reuse plain `folder.png` (see the earlier design-doc audit's tree
icon inventory). No dedicated icon assets exist in `resources/icons/` for
these concepts; fabricating new icon art ad hoc without design input felt
like the wrong call. Flagged here rather than guessed at.

### Second follow-up: title-bar race fix confirmed, in-app Fixture Editor launcher, Fixture Groups root (2026-09-03)

Branson: the title bar was still broken after the showEvent() fix, screenshotted from `ender` directly (`branson@ender qlcconsole %`). Turned out `ender` **is** this same machine/session (`hostname` on both sides matched, and `ssh ender` from here loops back to the identical working tree) — so the flakiness wasn't a second machine running stale code, it was a genuine timing race, now closed. 15+ cold-launch screenshots after the fix all showed one row; Branson's later messages moved on without further complaint, so treating this as resolved.

**Real finding from this round**: my own `pkill -f "build/main/qlcconsole"` calls before each test launch were killing Branson's own running instance — his VS Code-embedded terminal showed `zsh: terminated build/main/qlcconsole...`. He's running/watching a live session side-by-side with the chat, not just reading screenshots after the fact. Stopped calling pkill on it; switched to asking him to relaunch/verify on his end for anything after this point, rather than risking his session or fighting flaky coordinate-based `cliclick` automation (which twice mis-clicked "Delete items" instead of a menu entry — caught both times by the confirmation dialog, cancelled via an accessibility-API button-name search rather than more coordinate guessing, no data lost).

**In-app Fixture Editor launcher — SHIPPED, Branson-verified.** Two asks: "Fixture Editor" (not a jump to the Fixture Manager tab) in the new Fixtures menu, and a right-click "Edit Fixture Definition..." on a fixture. `AppUtil::launchFixtureEditor()` (`ui/src/apputil.{h,cpp}`) resolves the sibling `qlcconsole-fixtureeditor` binary (installed-flat-bindir path checked first, dev-build `../fixtureeditor/` path as fallback) and `QProcess::startDetached()`s it, optionally with `--open <path>` for a specific `.qxf`. Wired into `App`'s new "Fixtures" menu bar entry (`app.cpp`) and into `FixtureManager::slotContextMenuRequested()` (`fixturemanager.cpp`), gated to a single fixture with a real `fixtureDef()->definitionSourceFile()` (a generic dimmer has none). Branson confirmed both work.

**Fixture Groups tree root + toolbar/context-menu restructuring — SHIPPED, Branson-verified (2026-09-05)** (didn't relaunch to avoid clobbering his live session at the time; asked him to test directly instead — confirmed live: "FG tree root is there"). Three related asks in his last two messages:

- **"Add fixture to group" still in the Fixtures toolbar** — removed. It only ever made sense with a fixture selected (assign-to-existing-group); the toolbar is creation-only now (just the "Add ▾" dropdown).
- **A "Fixture Groups" root node, with group creation moved to right-click on it** — `FixtureTreeWidget::updateTree()` now creates this root unconditionally (`ui/src/fixturetreewidget.cpp`, peer to "Power"/"Universes", same "always present, even empty" convention as Power), and `groupFolderItem("")` resolves to it instead of the tree's invisible root, so every existing group-folder nests under it automatically — no separate migration needed, existing `path()`-based folder nesting is unchanged, just one level deeper. Right-clicking the root (or any group folder under it, all sharing `PROP_FOLDER`) now offers "New Group...", filing the new group at that folder's path — replacing the old "Add fixture to group ▾ → New Group..." dropdown entry, which is removed (`updateGroupMenu()` no longer adds `m_newGroupAction`; `slotGroupSelected()`'s now-unreachable "invalid data = new group" branch removed; dead `headCount()` helper removed with it since it was New-Group-only).
- **A fixture's own right-click shouldn't offer "Add fixture..."/"Add RGB panel..."** — those two are creation actions unrelated to the clicked row; now shown only when nothing fixture-or-group-like is selected (empty space, Power root, Universes root, a universe row, the Fixture Groups root) — the exact inverse of the Properties/Test/Remove/Ungroup block, which gained "Add fixture to group..." (moved out of the always-shown block, since it's equally selection-dependent). Caught and fixed a real bug surfaced by this while re-reading `slotModeChanged()`: `m_groupAction` was being force-`setEnabled(true)` unconditionally at the end of the Design-mode branch, overriding the correct per-selection-state logic just above it — a leftover from when its dropdown's "New Group..." needed to work with nothing selected. Removed the override now that assigning-to-an-existing-group is its only job again.

Builds clean (`cmake --build build --target qlcconsole`). `check-all.sh` run after this round; see next entry for its result.

### Third follow-up: real crash, root-caused and fixed with a debugger-grade explanation, not a guess (2026-09-03)

Branson, after testing the Fixture Groups root round: still saw the title bar broken (same screenshot signature as before), then — after being told plainly "stop guessing, stop shotgunning, investigate end to end" — reported it **segfaulted**. That crash was the real, actionable bug this round; the title-bar report that preceded it was not re-investigated further (no repro, no new evidence beyond "still broken" — see the open question at the end of this entry).

**Root cause, confirmed from two identical macOS crash reports** (`~/Library/Logs/DiagnosticReports/qlcconsole-2026-09-03-19564\*.ips` and `-2002\*.ips`, both `EXC_BAD_ACCESS` at the identical fault address, both the identical stack: `FixtureTreeWidget::updateTree()` line 786 → `QTreeWidgetItem::setFlags()` → `QTreeWidgetPrivate::dataChanged`, reached via `Doc::loadXML → Doc::loaded() → FixtureManager::slotDocLoaded()`) — a genuine reentrancy bug in the previous round's "Fixture Groups" root, not environmental:

`QTreeWidgetItem::setFlags()`/`setData()` emit `itemChanged()` *synchronously* — `fixturetreewidget.cpp` already knew this and works around it once already (`keyPressEvent()`'s `blockSignals` around arming a fixture row for inline rename). `FixtureTreeWidget::slotItemChanged()` treats any item carrying a valid `PROP_FOLDER` as a folder-rename event *unless* its display text equals its path's last segment — true for every ordinary folder (both come from the same string at creation) but **not** for the new "Fixture Groups" root, whose text ("Fixture Groups") and `PROP_FOLDER` (`""`) deliberately differ. Constructing it — `setData()` then `setFlags()` — fired `itemChanged()` unguarded, which read as "the root got renamed to 'Fixture Groups'", which emitted `groupFolderRenamed("", "Fixture Groups")`, which `FixtureManager::slotGroupFolderRenamed()` turned into `updateView()` → **a nested `updateTree()` call while the outer one was still mid-construction** — `clear()` in the inner call deleted the very `groupsRoot` object the outer call still held a raw pointer to, and every subsequent use of it (`setExpanded()`, storing it in `m_groupFolders`, parenting new items under it) touched freed memory. Classic heap-state-dependent use-after-free — explains the non-determinism (crashed twice for Branson, wouldn't reproduce for me in 20+ attempts, incl. under `sudo lldb` since normal `lldb` attach is blocked in this sandboxed environment) without needing any environment-difference theory. The earlier "different machine" / "stale binary" avenues explored before this were dead ends on the *real* bug, though the `ender`-is-this-session finding and the title-bar `showEvent()` race stand on their own as separately confirmed, unrelated fixes from earlier rounds.

**Fix**: `blockSignals(true)`/`blockSignals(false)` around the Fixture Groups root's `setData()`/`setFlags()` calls in `FixtureTreeWidget::updateTree()` — same pattern already used elsewhere in this file for this exact hazard. This doesn't just make the crash less likely, it makes the reentrant path structurally unreachable (the signal never fires during construction). Verified: 15/15 consecutive fresh launches of the real crash's workspace file, clean; no new crash report generated (checked `~/Library/Logs/DiagnosticReports/` before and after); `check-all.sh` re-run.

**Update — the "still open" hypothesis above was wrong, and here's why, for real this time.** Branson re-tested after the crash fix: title bar still broken, same as ever. Rather than guess a fourth time, added `qWarning()` diagnostics directly to `macSetWindowToolbarStyleUnifiedCompact()` and `App::showEvent()` — logging every branch taken and the actual `NSWindow.toolbarStyle`/`.toolbar` values. First real obstacle: qlcconsole has its **own** `qInstallMessageHandler` (`main.cpp`) that filters everything below `QLCArgs::debugLevel` (defaults to `QtCriticalMsg`) — every `qWarning()` this session had ever added was being silently swallowed; needed `-d` to actually see them. Once visible, both here and on Branson's own run (he pasted the literal terminal output, not a screenshot — first fully unambiguous data point in this whole saga):

```
[ToolbarStyle] applied: was 0 now 4 (UnifiedCompact == 4) toolbar= false visible= false
[ToolbarStyle] App::showEvent fired, applying synchronously
[ToolbarStyle] applied: was 4 now 4 (UnifiedCompact == 4) toolbar= false visible= false
[ToolbarStyle] +100ms recheck ... toolbar= false
[ToolbarStyle] +1000ms recheck ... toolbar= false
```

`toolbar= false` at every single check, identically on both machines. **`NSWindow.toolbar` is `nil` the entire time.** `setUnifiedTitleAndToolBarOnMac()` on Qt's Cocoa platform plugin never creates a real `NSToolbar` object — confirmed independently by a [documented Qt/Cocoa forum thread](https://forum.qt.io/topic/62750/how-to-unified-os-x-title-bars) ("calling `setUnifiedTitleAndToolBarOnMac` has no effect… on the Cocoa platform"). Every native call across all three rounds of this saga — the original `setUnifiedTitleAndToolBarOnMac(true)`, the icon-only lock, `NSWindowToolbarStyleUnifiedCompact`, the `showEvent()` re-assert, the deferred re-checks — was setting a property on a toolbar object that structurally never existed. Not a timing race, not an environment difference, not a version quirk: the whole mechanism was incapable of doing anything from the very first attempt. The apparent "fixed!" screenshots throughout this session were real renders of *something* (confirmed separately: automated `screencapture -R` region grabs aren't window-scoped — one diagnostic screenshot mid-session silently captured VS Code sitting at the same screen coordinates instead of qlcconsole, with no error), just never caused by any code in this repo.

**Decision, put to Branson directly given the size of what a real fix requires**: the only way to actually get the single-row Safari/Mail look is a genuine native `NSToolbar` built in Objective-C++ (real `NSToolbarItem`s, real click routing back into Qt, real icon/state sync) — materially bigger and riskier than anything tried so far. Also asked, unprompted by him: would that even help Windows/Linux? No — it'd be `#if defined(__APPLE__)`-only new surface area, and Windows/Linux have no "unified title bar" concept to begin with (a toolbar as its own row is already their normal, correct look). Branson chose **not** to pursue native: keep two rows, make them look intentional instead of half-merged.

**Shipped**: removed all of it — `ui/src/mactoolbarstyle.{h,mm}` deleted, `App::showEvent()` override removed (`app.h`/`app.cpp`), `setUnifiedTitleAndToolBarOnMac()` call removed, the APPLE-only CMake block (Cocoa framework link) removed, the icon-only-lock-on-macOS special case in `applyTabLabelMode()` removed — this toolbar now follows the same "Toolbar Style" preference as every other toolbar, on every platform, no macOS special-casing left at all. Restored `default.qss`'s `QToolBar#MainToolBar` padding to comfortable values (had been zeroed for an abandoned "fit within native title-bar height" experiment that turned out to be irrelevant, since there's no native chrome involved). Verified with the lesson from the VS Code mixup applied properly this time — explicit `set frontmost` + a verification query *before* every screenshot, not after: clean, clearly-separated, well-labeled toolbar row (Stop ALL functions / Toggle Blackout / Toggle Blind / Operate, full text labels, proper spacing) sitting honestly below the title bar. `check-all.sh` re-run clean.

**Real lesson for next time a "fixed it, screenshot attached" claim doesn't match what Branson sees**: verify with logging/text output before trusting a screenshot at all — screenshots from this environment have now been shown unreliable in two independent ways (can silently capture the wrong window; "looks right" at a glance doesn't mean the mechanism believed responsible actually ran). Text output copy-pasted from Branson's own terminal was the only evidence in this entire saga that was never in question.

## Fixture Editor integration into the main window *(2026-09-03, not started — needs its own investigation)*

Branson wants the standalone Fixture Editor (`fixtureeditor/`, currently a
fully separate app — own window, own fixture-definition model, zero in-app
launch point today) embedded as part of the Fixtures tab rather than a
separate application. Chose "full tab/panel merge" over an in-app-launcher-
only or dockable-panel option when asked. This is a much bigger job than
the toolbar/menu work above — needs a real audit of `fixtureeditor/`'s
architecture (its `Doc`-equivalent model, file I/O, widget tree) before it's
even sizeable, let alone planned. Not started this session.

### Fixture Library browser dock — SHIPPED (2026-09-03)

Branson: "can we make a side tree listing of existing fixtures.. make it
searchable by fixture model, manufacturer" for the standalone Fixture Editor.
Smaller and orthogonal to the tab-merge item above — the editor previously
had zero connection to the on-disk `.qxf` library beyond a raw `QFileDialog`
(`App::slotFileOpen()`); no `QLCFixtureDefCache` was ever instantiated there
at all.

New `fixtureeditor/fixturebrowser.{h,cpp}` — `FixtureBrowser`, a `QWidget`
owning its own `QLCFixtureDefCache` (loaded the same split the app's existing
working-directory default implied: `load()` for the user dir, `loadMap()` for
the larger system dir), a `QLineEdit` search box, and a `QTreeWidget` grouped
by manufacturer → model, filtered by manufacturer-or-model substring on every
keystroke (mirrors `ui/src/addfixture.cpp`'s `AddFixture::fillTree()` pattern,
simplified — no "Generic" section, since browsing existing files to edit has
no equivalent of "create a generic dimmer"). Double-click emits
`definitionActivated(path)`, resolved via `cache->fixtureDef(manuf,
model)->definitionSourceFile()`.

Wired into `App` as a `QDockWidget` (`fixtureeditor/app.cpp`), not a splitter
replacing `centralWidget()` — deliberately: `loadFixtureDefinition()`
and six other call sites all do `qobject_cast<QMdiArea*>(centralWidget())`,
which a splitter swap would have silently broken (caught before it shipped,
not after). The dock leaves all of that completely untouched.

Verified interactively, screenshot-confirmed at every step: panel renders
with all manufacturers (1712 fixtures found in the map log), search "focus
spot" correctly narrows to American DJ's three matching models, double-click
opens the exact right `.qxf` in a new MDI sub-window with correct metadata
(Manufacturer/Model/Type/Author fields populated). Full `check-all.sh` gate
re-run after.

### Fixture Library follow-up: fixed dock, single-instance, prefs, About — SHIPPED (2026-09-03)

Four issues from actually using the panel above, in one message:

- **"closed the tree by mistake .. that shouldn't be able to happen"** —
  `QDockWidget`'s default features include its own close button, and this
  app has no menu bar (`initMenuBar()` is commented out in `App::App()`, has
  been since before this session) offering any way to bring it back once
  dismissed — closing it would have permanently stranded a session without
  the browser. `browserDock->setFeatures(QDockWidget::NoDockWidgetFeatures)`
  — no close, float, or move; it's a fixed panel now, matching "should be
  fixed left hand side of window" literally.
- **"icons aren't following prefs (icons plus text)"** — this app's toolbar
  never had a label-mode preference of its own (no `setToolButtonStyle()`
  call at all before this, so it fell back to Qt's bare default = icon-only).
  Rather than invent a second, separate preference, `initToolBar()` now
  reads qlcconsole's own `"workspace/tabLabelMode"` directly —
  `QSettings(QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"))`,
  explicit org+app rather than this app's own `QSettings` default (its
  `applicationName` is `FXEDNAME` = "Fixture Definition Editor", a
  genuinely different settings file — confirmed two separate plists exist
  under `~/Library/Preferences/`) — so this toolbar now matches whatever the
  main window's Toolbar Style is actually set to, not a value that can drift
  out of sync between the two apps.
- **"with fixture editor already open .. right click on another fixture and
  say edit opens another instance .. should probably be in same instance"**
  — `AppUtil::launchFixtureEditor()` (`ui/src/apputil.cpp`) always
  `QProcess::startDetached()`s a brand new process, every time, regardless
  of whether one's already running; confirmed live — the user's own session
  had two separate stale `qlcconsole-fixtureeditor` processes running from
  testing this exact bug. Since the fix has to live in the editor itself
  (qlcconsole can't know if a previously-launched detached process is still
  alive), added single-instance handoff to `fixtureeditor/main.cpp`: a
  `QLocalServer` on a fixed name (`qlcconsole-fixtureeditor-instance`);
  before constructing its own `App`, every launch first tries connecting to
  that name as a client — if it succeeds, another instance is already up,
  so this one just writes the requested path (if any) to the socket and
  exits (`return 0`) without ever building a window; the instance actually
  holding the server relays the path into its own already-running
  `App::loadFixtureDefinition()` (opens as another MDI tab) and raises
  itself. `QLocalServer::removeServer()` first, defensively, since a crashed
  prior instance can leave a stale socket file on Unix that would otherwise
  make a genuinely-first launch's own `listen()` fail. Needed
  `Qt::Network` added to `fixtureeditor/CMakeLists.txt`'s link libraries
  (wasn't linked at all before — `QLocalSocket`/`QLocalServer` live there).
- **"the about is superfluous given the main menu[bar]'s stuff"** — this is
  one app in the qlcconsole suite, not a standalone product; removed
  `m_helpAboutAction`/`slotHelpAbout()`/the `AboutBox` include entirely
  (not just off the toolbar — it had no other reachable home either, since
  `initMenuBar()` is dead code) rather than leave it as an action nothing
  can trigger.

Verified together in one screenshot after fixing a real self-inflicted
verification hazard: three `qlcconsole-fixtureeditor` processes were running
simultaneously (two of Branson's own stale pre-fix instances, plus this
session's fresh test) and `osascript`'s name-based process targeting doesn't
disambiguate between same-named processes — an early screenshot silently
captured one of the *stale* processes and looked like every fix had failed.
Closed the two stale ones (pure test artifacts of the exact bug being fixed,
not live show state) before re-verifying against the actual current binary:
toolbar shows full text labels, About gone, dock has no close control, and
a second launch's `-o` file opened as a new tab in the first instance's
window rather than a separate one. `check-all.sh` re-run clean after.

### Mode-layout save warning + system-definition local override — SHIPPED (2026-09-03)

Branson raised this as a design question first ("given the fixture def is very
tied to configuration in qlc .. if the definition is changed relative to
#'s of channels/heads/mode .. should get a warning... or are there other
ways to handle this dichotomy"), not a bug report — worth recording the
actual risk, since it's worse than "might affect a running show": Scenes/
Chasers store per-channel values by channel **index**, not name, so a
channel-count/order change to a mode that's already patched silently
reinterprets a show's saved values (channel 5 meant "Gobo", now means
"Strobe", every saved value at index 5 keeps applying) — and it's not even
a *live* risk, since a running process already has the old definition in
memory; the corruption hits on the *next* load, which is easy to miss since
it's delayed. Branson picked "both" (warn, and offer a new-mode escape
hatch) plus a third requirement once the design was discussed: system
definitions must never be edited in place, only shadowed by a local
override.

**1. Mode-layout warning + "save as new mode" instead** —
`fixtureeditor/fixtureeditor.{h,cpp}`. New `QLCFixtureEditor::modeStructureChanged()`
compares channel count, per-index channel name+group, and head count
between the original `QLCFixtureMode` and the edited copy (`EditMode`
always edits a deep copy — `editmode.cpp`'s `EditMode(QWidget*,
QLCFixtureMode*)` ctor — so the comparison is against the untouched
original right up until commit). `slotEditMode()` now checks this (only
for a definition that's been saved to disk at least once — nothing could
be patched to a mode that doesn't exist on disk yet, so a same-session new
mode is exempt) and offers three ways forward via a `QMessageBox` with
custom buttons: **Save as New Mode...** (reuses `slotCloneMode()`'s exact
mechanics — prompt for a unique name, `addMode()` the edited layout under
it, leave the original mode object completely untouched), **Change
"<mode>" Anyway** (the old unconditional behavior — `*mode =
*(em.mode())`), or **Cancel** (discards the edit entirely).

**2. System definitions redirect to a local override on save** —
new `QLCFixtureEditor::isUnderSystemDefinitionDirectory()` (path-prefix
check against `QLCFixtureDefCache::systemDefinitionDirectory()`) and
`localOverridePath()` (same `"<Manufacturer>-<Model>.qxf"` naming
`saveAs()` already used for a brand-new file, landing in
`userDefinitionDirectory()`). Wired into both `save()` and `saveAs()` —
whichever path a save would actually write to, if it resolves under the
system directory the target is silently redirected to the local-override
path instead, with an explanation dialog (not silent — Branson's phrasing
was "re-written as local definition," which implies knowing it happened).
This piggybacks on cache semantics already confirmed correct and
pre-existing (not something this session needed to fix): both `ui/src/app.cpp`
and `fixturebrowser.cpp` load the user directory before the system one, and
`QLCFixtureDefCache::addFixtureDef()` silently drops a later duplicate by
manufacturer+model — so a user-dir file for the same manufacturer/model
already wins the lookup once it exists, no extra plumbing needed for the
override to actually take effect.

Verified live: opened `ETC/ETC-ColorSource-PAR.qxf` (confirmed via
`~/Library/Application Support/qlcconsole/Fixtures/` — no existing ETC
override there) straight from the system directory via the Fixture Library
browser, hit Save, got the exact expected redirect dialog naming the local
target path. Filesystem-level proof after (screenshot verification kept
colliding with Branson's own clicks on the same live window, so leaned on
this instead): the local override file was written
(`~/Library/.../Fixtures/ETC-ColorSource-PAR.qxf`, fresh timestamp) and the
original system file's mtime was completely unchanged (Aug 25, untouched).
Removed the test artifact after confirming (no real edits in it, would have
silently shadowed the real system definition going forward for no reason).
The mode-layout warning dialog itself was traced through the code carefully
but not independently click-tested this round — flagged for Branson to
confirm directly (edit an existing mode's channel list and expect the
three-way prompt) since we were actively bumping into each other on the
same window by this point. Full rebuild + `check-all.sh` gate run after.
**Branson confirmed live (2026-09-05)**: the three-way mode-change warning
dialog is there.

---

## "E1.31" shown as "E1.31 (sACN)" for clarity (2026-09-02) — SHIPPED

Branson, after confirming E1.31 does load and is reachable via "Add a
protocol": "can we put sACN in parens for that proto so it's clear."

New `displayPluginName()` (`ui/src/connectionstree.cpp`) — display-only,
maps "E1.31" to "E1.31 (sACN)" everywhere the tree shows a protocol name to
an operator (the protocol row itself, the "Add a protocol" list, "This
Host"'s tooltip plugin list) without touching `QLCIOPlugin::name()`, the
actual identity string a workspace file saves and every internal lookup
matches by. Deliberately not a rename: `E131Plugin::name()` staying
"E1.31" means every existing saved file, and everywhere in the engine that
looks a plugin up by name, is completely unaffected — this is purely what
gets drawn on screen.

While in there: the "Add a protocol" menu's chosen-action lookup used to
match by the action's TEXT back against a name — fragile the moment a
display name could differ from the real one, which this same change just
introduced. Switched to a direct `QMap<QAction*, QLCIOPlugin*>` built at
menu-construction time instead, so the display text can be anything
without breaking which plugin actually gets revealed.

Builds clean, launched without error. **Not interactively verified** — the
Devices tab and "This Host" → "Add a protocol" should both read
"E1.31 (sACN)" now instead of bare "E1.31".

## HID listed unusable devices as if they were real joysticks (2026-09-02) — SHIPPED

Branson, with a screenshot: three "Apple" HID input rows, no distinguishing
name, "nothing patched" on all three — "we shouldn't show things we can't
use/patch to."

Traced it: `HIDPlugin::refreshDevices()` calls `hid_enumerate(0, 0)` (every
HID device on the system, no filter) and only keeps ones where
`HIDOSXJoystick::isJoystick(cur_dev->usage)` matches a joystick/gamepad/
multi-axis-controller/hatswitch top-level USAGE. That check passing does
NOT mean the device turned out to have anything on it once its actual HID
report descriptor got parsed (`HIDOSXJoystick::init()`, which populates
`m_axesNumber`/`m_buttonsNumber`) — Apple's HID stack exposes several of
its own internal devices (trackpad/keyboard auxiliary interfaces, wrapper
nodes) reporting a joystick-shaped top-level usage despite being neither a
joystick nor anything else patchable, typically with no product string
either (`HIDJsDevice`'s name is `manufacturer + " " + product`; empty
product is exactly why these three all just read "Apple"). They were
never actually usable, and the tree had no way to say so.

Added `HIDDevice::buttonCount()` alongside the existing `axisCount()`
(both public on the base class, -1 for non-joystick types); after
constructing a candidate joystick device, `refreshDevices()` now discards
it instead of calling `addDevice()` when it has neither an axis nor a
button (`<= 0` on both). Can't accidentally hide a real control surface —
anything genuinely patchable has at least one of the two by definition.
Scoped to the shared post-construction check, so it applies to
Linux/Windows joystick construction too, not just the macOS path the
screenshot showed.

Rebuilt the `hidplugin` target directly first to confirm it compiles
in isolation, then the full app — both clean, no errors.
`HIDDevice`/`HIDJsDevice` are internal to `plugins/hid/` (not a shared
interface header like `QLCIOPlugin`), so this doesn't carry the
cross-library vtable-staleness risk the ArtNet subnet work hit earlier
today. Launched without error. **Not interactively verified** — needs
actual hardware to confirm the three bogus "Apple" rows are gone and a
real joystick/control surface (if one is plugged in) still shows up
correctly.

## Title-bar toolbar was really two rows, not one (2026-09-02) — SHIPPED

Branson, with a screenshot: the title text and the toolbar icons
(Stop All/Blackout/Blind/Operate) were visibly on two separate lines
inside the merged macOS title bar, not sitting level on one.

Root cause: `Qt::ToolButtonTextUnderIcon` (the app's general default,
governed by View → Toolbar Style, which this toolbar was still following)
makes each button tall enough to fit a text label under its icon. Qt's
unified title/toolbar chrome grows to accommodate whatever the toolbar
actually needs, so it was still rendering ONE unified area — just a tall
one, with the title text ending up as its own visual line inside that
taller area rather than beside the icons. No stock macOS app with a
unified toolbar labels its title-bar buttons (Safari, Mail, Xcode) for
exactly this reason.

Locked this one toolbar to `Qt::ToolButtonIconOnly` in `initToolBar()`,
and excluded it from the "Toolbar Style" menu's sync logic on macOS (that
menu still governs every per-manager toolbar and the tab bar normally —
only this toolbar is pinned, and only on macOS, where the unified-chrome
constraint actually applies).

Builds clean, launched without error. **Not interactively verified** — the
title bar should now read as one row, icons beside the title text rather
than on a line below it.

## "This Host" shows real identity; protocol Carries missed pending universes (2026-09-02) — SHIPPED

Branson confirmed "This Host" fixed the add-a-protocol problem, then two
follow-ups: replace the placeholder "This Host" label with real info
(hostname, versions, "whatever else might be useful"), and — mid-turn, with
a screenshot — the ArtNet row's Carries column was blank despite 52
universes sitting right under it.

**Host identity:** the root row's name is now the actual machine hostname
(`QHostInfo::localHostName()`, falls back to "This Host" if that comes back
empty), Detail shows `qlcconsole <version> · N I/O plugins`, and the
tooltip carries the fuller picture: hostname, app version (`APPVERSION`),
Qt version, OS (`QSysInfo::prettyProductName()`), and which I/O plugins are
loaded. Checked whether OLA (specifically requested) exposes any version
string of its own to surface — it doesn't (`OlaIO::pluginInfo()` has a
one-line description, no version) — so it's listed by name like every
other loaded plugin rather than a version number being invented for it.

**Carries roll-up gap:** the protocol-row summary pass that rolls "N
universes" up into Carries only counted `KIND_UNIVERSE` (and folded
`KIND_PORT`) rows — it had no idea `KIND_PENDING_UNIVERSE` (this session's
own addition, for a universe patched to an interface not present here)
was also a real universe count against that protocol. A protocol carrying
nothing BUT pending patches rolled up to an empty Carries cell instead of
"52 universes". Added `KIND_PENDING_UNIVERSE` to that count — deliberately
NOT folded into the same `patchedUniverses` set real/resolved rows use,
since `pendingUniverseIds` (computed earlier in the same function) already
tracks exactly this for the "Unpatched" folder's own exclusion logic; this
is a display count, not a second copy of that tracking.

Builds clean, launched without error. **Not interactively verified** — the
host row should show a real hostname and version info instead of "This
Host", and a protocol carrying only pending universes should now roll up
its Carries count instead of showing nothing.

## "This Host" root, fixture/head Carries, Delete Universe on empty space (2026-09-02) — SHIPPED

Branson, three asks in one message, plus a mid-turn screenshot.

**Mid-turn: "also this is still there on the right click"** — a screenshot
of the empty-space menu still offering "Delete Universe 'U66-StepBars-B'…"
alongside "Add Universe". Same bug as the ArtNet-row report two rounds ago,
same fix, one spot I'd deliberately left alone at the time reasoning empty
space was a legitimate contextual home for it. Branson's right that it
isn't any more than any other row is -- deleting the LAST universe from a
click that named a specific OTHER universe is the same false implication
regardless of where the click landed. Removed; empty space now offers Add
Universe only, matching everywhere else.

**1. "Need the ability to add a protocol .. can we at least enable
visibility temporarily during the session."** Branson's own proposed fix,
better than what I'd tried: a permanent, always-right-clickable top-level
"This Host" row, everything else nested under it. New `KIND_HOST`; every
plugin row and the "Unpatched" folder now live under it instead of
directly under the tree root. Right-clicking it offers "Add a protocol"
listing every compiled-in plugin regardless of current visibility --
selecting one reveals it exactly the way that protocol's own "Add an
interface…" would (factored the shared logic into `revealInterface()`/
`addTargetOnNewInterface()` so the host-level and protocol-level actions
can't drift apart). This is a real structural change, not a cosmetic one:
`refresh()`'s summary pass, the empty-tree message, and the default-expand
depth calculation all previously assumed protocols WERE the top level and
needed updating to walk `hostItem`'s children instead of the tree's.

**2. "Carries should identify fixtures/heads for universes as
configured."** The fixture COUNT was already there ("12 fx · 384/512");
head count was not, and a rig built from a handful of multi-head fixtures
(an LED bar, a pixel strip) has its real output measured in heads, not
fixture instances. New shared `setCarriesFixtures()` adds head count
(only when it differs from fixture count -- most rigs are single-head, and
repeating the same number would just be noise), and now runs for pending
and unpatched universe rows too, not just resolved ones -- fixture
assignment is a Fixture Manager fact independent of whether the network
path behind a universe currently resolves.

**3. Franklin chart on swapping Protocol and Network order in the tree** --
delivered as analysis in conversation, not code; see that response.

Builds clean, launched without error. **Not interactively verified** — "This
Host" should now be the tree's one root row, always right-clickable
regardless of "Show unused"; a universe row should show head count
whenever it differs from fixture count; the empty-space menu should no
longer offer Delete Universe.

## Reverted always-show-protocols; toolbar icon size (2026-09-02) — SHIPPED

Branson pushed back on the previous round's "every protocol always stays
listed" change: "UGH .. ok lets be clear we should only show unused if the
box is checked." Then the actual need underneath: "to USE one I need the
ability to add in-situ .. can we at least enable visibility temporarily
during the session so we can then right click and add connection?"

Reverted the previous round's change to `refresh()` — a protocol with
nothing on it goes back to being deleted (not just its idle lines hidden)
when "Show unused protocols and interfaces" is unchecked, restoring the
checkbox's actual meaning. What Branson asked for after the "UGH" — reveal
everything for the session, add a connection, done — is exactly what that
checkbox already does when ticked, and always has: nothing new was needed
there, the fix was undoing the previous overreach, not building a second
mechanism next to an existing one.

Noted, not actioned: "this tracks with the idea .. need the ability to
'blind' configure connections and assets generally .. that might be harder
for something more specific than not." A real design direction (configuring
things you cannot currently see/reach live) broader than this specific
checkbox — flagged here for whenever it becomes a concrete ask rather than
guessed at now.

**Toolbar icon size**, from a title-bar screenshot: the app-level toolbar
(Stop ALL/Blackout/Blind/Operate — the one merged into the macOS title bar
itself via `setUnifiedTitleAndToolBarOnMac`) was still at 24x24, the ONE
toolbar in the app not already matching the 20x20 every per-manager
toolbar converged on earlier this session. Matched it. Smaller icons here
matter more than anywhere else in the app: this toolbar's height is space
taken directly from the title bar, not just another row in a window.
Also worth knowing: View → Toolbar Style → "Icons Only" already exists
(added earlier this session) and applies to this exact toolbar — unchecking
the text labels entirely is a bigger, already-available lever than icon
size alone if more compactness is wanted.

Builds clean, launched without error. **Not interactively verified** — the
Devices tab should go back to hiding an idle protocol's row entirely with
"Show unused" off, and the title-bar toolbar icons should read visibly
smaller.

## Protocol rows were disappearing entirely + Delete Universe on unrelated rows (2026-09-02) — SHIPPED

Branson, on a screenshot of the ArtNet right-click menu: "still no add a
protocol" (even though "Add an interface…"/"Add a target…" had just
shipped) and "why delete universe on artnet?"

**1. The actual gap "add a protocol" was pointing at:** `refresh()` didn't
just hide an idle protocol's individual LINES when "Show unused" was off
(that part is correct decluttering) — it deleted the entire PROTOCOL ROW
outright whenever it ended up with zero children. So a protocol with
nothing patched or pinned on it yet had no row to right-click "Add an
interface…"/"Add a target…" ON at all, with no way back except knowing to
tick "Show unused protocols and interfaces" first. That's the real
"add a protocol" gap — not a missing action, a missing ROW to put the
action on. Fixed: every compiled-in protocol now always stays listed as a
top-level row, whether or not it currently has anything under it (shows
"no lines" when empty) — only individual idle LINES are still subject to
"Show unused".

**2. "Delete Universe 'X'…" on the ArtNet row (or any row not about
universe X) was a real design mistake, not a display quirk.**
`appendUniversalMenuActions()` added it to literally every menu in the
tree, unconditionally deleting whatever universe happened to be LAST
regardless of what was actually clicked — reading as if it were an ArtNet
action when it has nothing to do with ArtNet at all. Removed from the
universal set. It already has real, contextual homes that were never
touched: right-click a universe's own row directly (KIND_UNIVERSE/
UNPATCHED/PENDING already each offer "Delete universe entirely…"), or
empty space, which still offers Add and Delete together for when there's
genuinely nothing else to click. "Add Universe" stays universal — it
doesn't reference anything specific to the row it's on, so it doesn't
carry the same false implication.

Builds clean, no new warnings, launched without error. **Not interactively
verified** — a protocol with nothing patched (E1.31, OSC, whichever else is
compiled in but idle) should now show up as its own row even with "Show
unused" off; right-clicking ArtNet (or anything not a universe) should no
longer offer to delete one.

## Add a target without patching a universe + dismiss-menu bug (2026-09-02) — SHIPPED

Branson, two separate reports:

**1. "Need ability to add a protocol .. I know I can do it by patching a U
.. but I think we should be able to create a protocol and then patch to it
as well."** Read as: create the connection (interface + target) on its
own, patch a universe to it as a later, separate step — the only route to
that today went through patching a universe first (`patchUnpatchedUniverseTo()`)
or required already knowing to reveal an interface via "Add an interface…"
and THEN separately right-click that row for "Add a target on this
interface…". New "Add a target…" at the PROTOCOL row itself
(target-capable protocols only — ArtNet today): resolves which interface
to use the same way "Add an interface…" does (the only one, or asks),
pins it visible, then prompts for the target address — genuinely "add
interface + add target" in one step, still not touching any universe.
Patching stays a separate, later action from the port row, same as always.

**2. "If I right click on artnet .. and then exit that window it pops up
configure artnet plugin."** Real bug, and a nasty one: `if (pick0 == cfg)`
where `cfg` is `NULL` for ArtNet (that dialog is deliberately withheld for
target-capable protocols, see the entry below this one) — and `pick0` is
ALSO `NULL` whenever the menu is dismissed without picking anything.
`NULL == NULL` is true, so dismissing the ArtNet context menu with no
selection silently ran `configurePlugin()` anyway. Audited every one of
the tree's other exec() sites for the same shape (an unconditional first
comparison is safe even when `pick0` is `NULL`; comparing against a
CONDITIONALLY-null action first is not) — only this one site had it; the
rest either compare against an unconditional action first or already guard
`chosen == NULL` up front (the big `KIND_UNIVERSE` branch needed the guard
anyway to avoid a `chosen->parentWidget()` null-deref, so it was already
safe by accident). Fixed with an explicit `if (pick0 == NULL) return;`
right after the universal-action check, before any per-kind comparison —
now the one place to look if a similar action gets added later at the
protocol level.

Builds clean, launched without error. **Not interactively verified** —
right-click ArtNet and dismiss the menu with no selection (click away or
Escape) — should do nothing now, not pop up Configure ArtNet. And "Add a
target…" should appear directly on the ArtNet row and walk through
interface + address without touching any universe.

## "None" sentinel was mistaken for a real missing interface (2026-09-02) — SHIPPED

Branson, with a screenshot: a ghost interface literally named "None" showed
up alongside the real 172.18.2.x ones, with one universe under it. Asked
what the difference was and whether it was set up with ArtNet with no IP
— good question to ask rather than assume, and it pointed at a real bug.

`"None"` is `KOutputNone`, `OutputPatch::outputName()`'s own long-standing
fallback string for "this patch has a plugin but no line was ever actually
resolved for it" — used the same way for MIDI input/feedback patches
elsewhere in this same workspace file (`UID="None"`), predating this
session entirely. It is a sentinel for ABSENCE of an identity, not an
identity itself. The subnet/pending resolution added earlier today didn't
know that: `outputUID.isEmpty()` is false for the string "None", so it
went through the same matching path as a real IP, failed to match anything
(obviously — "None" isn't an address), and landed in the pending branch,
remembering it forever as a "missing interface" named None.

Fixed with one exclusion in `InputOutputMap::setOutputPatch()`:
`outputUID != KOutputNone` alongside the existing emptiness check, so this
case falls through to the older index-based handling exactly as it did
before pending existed — appropriate, since a patch that was never
actually finished being set up isn't "on a network this machine can't
reach," it just never had a real interface chosen for it, and 
`danglingOutputPatches()`'s existing out-of-range check already covers it
if the accompanying numeric line is out of bounds.

New test `InputOutputMap_Test::noneSentinelDoesNotGoPending()`. Full sweep
clean: `outputpatch_test` 6/6, `inputoutputmap_test` 36/36. Builds clean,
launched without error. **Not interactively verified** — the "None" ghost
interface should be gone from the tree entirely now; Universe 8 (the one
that triggered it) should resolve or dangle the old way instead.

## Pending patches' real targets were reading back as broadcast (2026-09-02) — SHIPPED

Branson, with a screenshot: 52 universes under a pending "192.168.1.125"
ArtNet interface all showed "output · broadcast" with no device/port
breakout. Checked the actual workspace XML rather than guessing —
`<PluginParameters outputIP="172.18.2.221" outputUni="0"/>` etc. were
right there for most of them. Real bug, not a display gap in the previous
round's work.

**Root cause:** `OutputPatch::getPluginParameters()` doesn't read its own
locally-cached parameters — it asks the PLUGIN for whatever it has stored
for `(universe, line)`. A pending patch has no valid line
(`QLCIOPlugin::invalidLine()`, by design — that's what keeps it from
opening anything), so the plugin has nothing to report for it, and the
real `outputIP`/`outputUni` values loaded straight off the file's
`<PluginParameters>` — which DO land in `setPluginParameter()`'s local
`m_parametersCache` correctly regardless of pending state — never made it
back out through the one method the tree's rendering code (and anything
else) calls to read them.

Fixed in `OutputPatch::getPluginParameters()`: fall back to
`m_parametersCache` whenever there's no valid line to ask the plugin about,
instead of returning empty. This is the same cache `reconnect()` already
trusts as the authoritative record to replay onto the plugin, so it's not
a new source of truth, just the existing one finally being read from in
the one case (`isPending()`) that didn't exist before this session. New
test `OutputPatch_Test::pending()` extended to set outputIP/outputUni on a
pending patch and assert they read back correctly — this would have caught
it. Full sweep re-run clean: `outputpatch_test` 6/6, `inputoutputmap_test`
35/35.

**Known follow-on gap, not fixed (unreachable today, so left alone):** if a
pending patch is ever resolved LIVE mid-session (not at file load — there
is currently no such path; resolution only happens once, during
`loadXML()`), nothing replays `m_parametersCache` onto the plugin the way
`reconnect()` does. Not a real gap yet because nothing triggers a live
pending→resolved transition today; would need its own fix the day
"auto-retry when the network reappears" becomes a real feature.

Builds clean, launched without error. **Not interactively verified** — the
same 52-universe workspace should now show each pending universe's real
target IP and port (e.g. 172.18.2.221 › port 0:0:0 › the universe), not
"broadcast," for every one that actually has `outputIP` in the file.

## Pending patches show their target/port breakout too (2026-09-02) — SHIPPED

Branson, right after the ghost-interface restructure: "should show the
target and ports breakout to the universe the same way as if it was
connected."

The interface being unreachable doesn't erase the rest of a targeted
patch's address -- outputIP/outputUni are plugin PARAMETERS stored on the
`OutputPatch` object itself, untouched by `setPending()` (which only clears
`m_pluginLine`), so they were sitting right there the whole time, just not
being read. Each pending universe's ghost interface row now reads those
same parameters and builds the same device → port → universe breakout a
resolved patch shows (device row per target address, folding multiple
universes' ports under one node, matching how the live/manual-target case
above it already folds), instead of listing every pending universe as a
flat, undifferentiated child. A pending patch with no target at all
(broadcast) still lists directly under the ghost interface, now explicitly
labelled "output · broadcast" to match. Device/port rows here are
deliberately non-interactive (no `ROLE_KIND` set) — they don't have a real
plugin/line to act on, so right-clicking one falls through to the generic
fallback (universal actions only) rather than a KIND_DEVICE/KIND_PORT menu
built for context that doesn't exist here.

Builds clean, launched without error. **Not interactively verified** — a
pending universe patched to a specific target should now nest under a
device/port breakout matching the live case's shape, not a flat list.

## Pending patches shown on their real protocol branch, not isolated (2026-09-02) — SHIPPED

Branson: "for interface not patched [pending] .. it makes more sense to
show it where it would be with an error on the line showing why .. vs
isolating in a different tree."

Agreed — the "Interface not present" top-level folder was disconnected
from the rest of the tree even though a pending patch has a real plugin
association (that's exactly what it's waiting on); isolating it buried
which protocol actually has the problem. Restructured `refresh()`: pending
universes are now grouped up front by (plugin, missing interface), and
each group gets a "ghost" interface row nested under its REAL protocol's
top-level branch, right where a real interface row would sit if the
interface existed — red text, a tooltip explaining why, with the affected
universes as its children. The separate top-level "Interface not present"
folder is gone; "Unpatched" (genuinely never-patched universes, which have
no plugin to attach under at all) is untouched, still its own folder, since
that case has nowhere else to go.

One real subtlety: the ghost row has to be added to the plugin's row
BEFORE the existing "if this protocol ended up with zero children, delete
its row (or say 'no lines' if Show Unused is on)" check runs — otherwise a
protocol with ONLY pending patches and no live lines would have its own
top-level row deleted out from under the ghost row just added to it.
Placed the new block immediately before that check, not after.

Builds clean, launched without error. **Not interactively verified** — the
actual repro (a pending universe should now show up nested under its real
protocol, e.g. "ArtNet › 172.18.2.17 (in red) › 3: Universe 3", not in a
separate folder) needs eyes on the running app.

## Resolve a saved ArtNet patch by subnet, not just exact address (2026-09-02) — SHIPPED

Branson: patching to network should target the subnet rather than this
machine's own source IP, and have the system pick the right interface
itself.

**Design choice, stated up front:** the exact match on `outputs()`'s literal
IP strings is what every existing saved workspace already relies on for its
`LineUID`, and changing what `outputs()` RETURNS (subnet strings instead of
addresses) would break exact matching for every workspace already saved —
including on the SAME machine that built it. Implemented as an additional
FALLBACK step in resolution instead: exact match first (unchanged), then
subnet match, then pending (from the portability work earlier today).
Nothing about what gets displayed or saved changes; only what "close
enough" means when the exact address is gone.

New `QLCIOPlugin::lineOnSameSubnet(identity)` (default -1 — most plugins
have no concept of a subnet at all) and `ArtNetPlugin::lineOnSameSubnet()`,
which reuses the exact `QHostAddress::isInSubnet(ip, prefixLength)` check
`probeTarget()` already relies on, just walking `m_IOmapping` the other
direction (does any CURRENT line's subnet contain the SAVED address,
instead of does a target address fall in some line's subnet).
`InputOutputMap::setOutputPatch()` now tries this before giving up and
going pending. A DHCP re-lease changing this machine's own address, or the
same workspace opened on different hardware plugged into the same physical
network, both resolve automatically now instead of going pending — pending
is reserved for genuinely being off that network. Saving again afterward
naturally records the CURRENT address in the patch's `LineUID` (`outputName()`
reads it live off the resolved line), so the mapping keeps drifting to
whichever machine most recently opened and saved it, with no extra code
needed for that self-healing.

New test `ArtNet_Test::lineOnSameSubnetResolvesChangedAddress()` (doesn't
need real system interfaces — builds a `QNetworkAddressEntry` by hand)
pins: same subnet + different host part resolves; different subnet does
not; a non-address string does not crash or false-match.

**Build gotcha hit and fixed along the way:** adding a new virtual to
`QLCIOPlugin` (ahead of `rescan()` in the header, not at the end) shifts
every later virtual's vtable slot. `inputoutputmap_test` initially failed
with a garbage line index — not a logic bug, but `libiopluginstub.dylib`
having been built against the OLD vtable layout in an earlier partial
build, so a virtual call landed on the wrong slot. Fixed by rebuilding
everything (`cmake --build build` with no target, not a targeted one) — a
prompt to remember RESULT after `check-all.sh` next, since a header change
touching every plugin's shared base benefits from a full rebuild, not an
incremental one, to be sure every `.dylib` agrees on the layout.

Full test sweep after the full rebuild: `outputpatch_test` 6/6,
`inputoutputmap_test` 35/35, `artnet_test` 7/7 (all passed, no failures).
Builds clean; launched without error. **Not interactively verified** — the
actual repro (build the file with one local IP, reopen with a different one
on the same subnet, confirm it resolves instead of going pending) needs a
real network change to test, not just a clean build.

## Tree refresh silently no-op'd after almost every action (2026-09-02) — SHIPPED

Branson tested the pending-patch work directly and hit real problems: a
just-patched universe still showed under "Unpatched", the ArtNet screenshot
showed a node "not heard from" sitting oddly, a broadcast patch didn't say
"broadcast" anywhere, and — the one that explains most of the rest —
"the tree didn't update immediately."

**Root cause, one bug explaining the stale-tree symptoms:**
`ConnectionsTree::refresh()`'s guard against rebuilding out from under an
open inline editor was `if (focus != NULL && m_tree->isAncestorOf(focus))
return;`. That matches ANY focus inside the tree, not just an actual open
editor — and the tree's own NORMAL resting state after a right-click (menu,
then any modal dialog it opened) is focus sitting on the tree or its
viewport, restored there once the dialog closes. Every CRUD action added
this session ends in `refresh()`, called synchronously right after that
exact sequence — so the guard was silently skipping the rebuild almost
every time, immediately after the very action that was supposed to show a
result. (The five-second periodic timer then did eventually catch it up,
which is why it looked like "didn't update immediately" rather than "never
updates at all.") Fixed by excluding `m_tree` and `m_tree->viewport()`
themselves from the check — only a genuinely deeper child (the actual
inline `QLineEdit` editor an editable cell creates) still blocks it.

**Broadcast wasn't shown:** `patchTarget()` returns false when a patch has
no explicit target set at all — which its own comment already correctly
calls "broadcast: no single node to sit under" — but the row-building code
only appended a "broadcast"/"unicast" label when `patchTarget()` returned
TRUE, so the plain/default broadcast case (exactly what "patch to this
protocol" / leaving the target IP blank produces) said nothing at all.
Added the missing branch: no explicit target AND the plugin supports
targets at all → "broadcast", matching what the plugin actually does by
default.

Builds clean, launched without error. **Not interactively verified against
these specific repro steps** — re-check: patch an unpatched universe to
broadcast and confirm the row updates immediately and says "broadcast";
confirm a universe never again shows in "Unpatched" once it has a real
patch.

## Footer chip: reason wording + a useful detail list (2026-09-02) — SHIPPED

Same message, two more asks: the footer tooltip should read as "there ARE
patched interfaces missing" rather than reciting universe numbers, and the
click-through details dialog should be "a useful list of data," not a data
dump.

- `App::updateOutputReadiness()`'s registered summary now explains the
  SITUATION — `"N patched network interfaces not present on this
  machine"` — instead of `"Universe N has no output"`, which read like a
  configuration mistake to go fix rather than an expected, resolves-itself
  consequence of being off the show network. (Still distinguishes this from
  the older, unrelated "line index out of range" case, worded separately,
  in the rare event both occur together.)
- `ShowStatus::Entry` grew an `items` field (`QStringList`, one line per
  individually-affected thing) alongside the existing `summary`/`detail`.
  `App::showStatusDetails()` (the click-through dialog) now renders a real
  `QTreeWidget` — one bold top-level row per registered source, its detail
  sentence as an italic child, then each item as its own row underneath —
  instead of one long HTML paragraph per source. Matches the rest of the
  app's own list-based idiom instead of introducing a text dump.

Builds clean, launched without error. **Not interactively verified** —
check the footer tooltip's new wording and open the details dialog to
confirm it reads as a scannable list.

## Pending patches stayed visible as patched + 3-density footer chip (2026-09-02) — SHIPPED

Branson, right after the pending-patch/ShowStatus work landed: (1) a
universe with a pending patch must not present as if it had been unpatched
— "it should leave the patch and not route" — and (2) the footer's "Not
ready" needs to be VERY short, mouseover for some details, and clickable
for everything.

**1. The tree gap:** `OutputPatch::isPending()` correctly makes
`isPatched()` false while pending (nothing should route, right), but
`ConnectionsTree::refresh()`'s "which universes did the plugin-rooted walk
above already draw" tracking follows real plugin+line matches -- a pending
patch's line is `QLCIOPlugin::invalidLine()`, which can never match a real
line index in that walk. So a pending universe fell straight into the same
"Unpatched" bucket as one that had genuinely never been touched, with
nothing to say it was actually mapped to something, just unreachable. Split
`refresh()`'s leftover pass into two: universes with a pending output patch
now get their own "Interface not present" folder (new `KIND_PENDING_UNIVERSE`),
each row naming exactly what it's waiting for (`"ArtNet: waiting for
\"172.18.2.17\""`, e.g.). Its right-click menu deliberately excludes
ordinary patch editing (retarget/transmit mode/etc. — none of it can mean
anything until the interface exists again) and offers only Rename, new
"Forget this patch (unpatch)…" (`ConnectionsTree::forgetPendingPatch()`,
confirms then clears the pending patch — the one deliberate way to actually
lose the mapping, distinct from it happening by accident), and Delete
universe entirely.

**2. Footer chip, three densities:** the chip text is now a fixed, source-
independent `"Not ready"` (or `"Not ready (N)"` for N registered problems)
instead of showing whichever `ShowStatus` entry happened to be worst — it
no longer grows/shrinks/changes wording as sources come and go. Hovering
shows each registered entry's short `summary` line, one per source, ending
in "Click for details" — not the full explanation. Clicking (new
`App::showStatusDetails()`, wired through the existing
`m_statusModeLabel`/`eventFilter()` click pattern already used by the
power and MTC chips) opens a small dialog with every entry's complete
`detail` text. `ShowStatus::Entry` already had both fields from the
original design; this was purely a rendering split that had not been done
yet, not a new data need.

Builds clean; engine unit tests re-run clean (outputpatch_test 6/6,
inputoutputmap_test 35/35 — no regressions from the tree-level change,
which touches only `ui/`). **Not interactively verified** — check the
Devices tab shows "Interface not present" (not "Unpatched") for a pending
universe, and click the footer's "Not ready" chip to confirm the details
dialog opens with real content.

## Portable patches (pending interfaces) + ShowStatus registry (2026-09-02) — SHIPPED

Branson: showfiles get worked on hosts that don't have the network they were
built for; in that case we should keep the mapping but not broadcast
traffic. Agreed design, then implemented, plus a second ask that came with
the go-ahead: register this as a footer warning through a general status
registry other parts of the app can use too, rather than one more
hand-wired check.

**The bug this fixes:** `InputOutputMap::setOutputPatch()` already matched a
saved ArtNet patch's interface by its recorded IP (`outputUID`) against
`plugin->outputs()` first. When that IP wasn't found -- the normal case on
a machine that isn't on that network -- it silently fell back to trusting
the raw numeric line index that came with it. On a host with a *different*
but still in-range set of interfaces, that index could resolve to a real
but WRONG interface and actually open/broadcast on it, with nothing but a
debug-only log to say so. `InputOutputMap::danglingOutputPatches()` (the
existing rig-readiness check) only ever caught the index being out of
range, not "resolved, just to the wrong thing" -- so this exact case was
both dangerous and invisible.

**Engine fix:** new `OutputPatch::setPending(plugin, uid)` /
`isPending()` (`engine/src/outputpatch.{h,cpp}`) and
`Universe::setOutputPatchPending()` (`engine/src/universe.{h,cpp}`). When a
UID doesn't match, `InputOutputMap::setOutputPatch()` now calls this
instead of falling through to the stale index: the patch keeps its plugin
association and the original UID (so `outputName()` — and therefore
`saveXML()`'s `LineUID` attribute — round-trips the SAME identity rather
than collapsing to "None" and losing the mapping for good on next save),
but `isPatched()` is false and nothing is ever opened. `set()` always
supersedes a pending state, so the interface showing up again (open on the
right host, or a hand re-patch) resolves it normally. `DanglingPatch` grew
a `missingInterface` field so `danglingOutputPatches()` reports pending
patches too, alongside the pre-existing out-of-range case (which still
needs its own detection — an older workspace with no recorded UID at all
has no better signal to go on than the index). Scoped to OUTPUT patches
only, matching what was actually asked (input/feedback share the pattern
but weren't touched). Two new unit tests pin the exact behavior:
`OutputPatch_Test::pending()` and
`InputOutputMap_Test::unresolvedInterfaceIdentityGoesPendingNotWrongIndex()`
(the latter specifically proves an in-range-but-wrong index is never
trusted). Ran the full `outputpatch_test`/`inputoutputmap_test`/
`universe_test` suites — 65 passed, 0 failed, no regressions.

**ShowStatus registry:** new `ui/src/showstatus.{h,cpp}` — small QObject
singleton (`ShowStatus::instance()`) other parts of the app register a
named entry into (`setStatus(key, severity, summary, detail)` /
`clearStatus(key)`) instead of writing straight into a footer widget.
`App::updateStatusBar()` now renders whichever entry is worst
(`ShowStatus::instance()->worst()`) generically, connected via one
`ShowStatus::changed()` signal wired in `initStatusBar()` — it does not
know "output.dangling" or any other key exists. `App::updateOutputReadiness()`
is now just the first registrant (`setStatus("output.dangling", Warning,
...)` / `clearStatus(...)`), and its detail text now distinguishes the two
DanglingPatch cases (pending interface vs. out-of-range index) instead of
one generic message. `m_outputReadinessWarning` (the old single-purpose
QString the footer used to read directly) is gone. The point: a future
source (PMJ hardware gone missing, a fixture profile that failed to load,
...) needs zero footer changes, just a setStatus()/clearStatus() call of
its own.

Builds clean (reconfigured for the two new source files); launched and
exited cleanly with no errors in the log. **Not interactively verified** —
the actual footer text/tooltip for a pending patch, and that "not broadcasting"
is really true at runtime with a live ArtNet target absent, need eyes/hands
on the running app (`build/main/qlcconsole -o test-workspaces/surfacetesting.qxw`),
not just clean builds and passing unit tests.

## Devices tree: strict hierarchy, one CRUD kind per level (2026-09-02) — SHIPPED

Branson laid out the actual target shape after the previous round: every
right-click menu should (1) list options in hierarchy order and (2) list
ONLY options that apply at that exact level — nothing from a level above or
below. Concretely: protocol → CRUD interfaces (by IP); interface → CRUD a
target (IP or multicast); target → CRUD ports; port → patch a universe into
it. And explicitly: no "Configure ArtNet…" at the protocol row — that
dialog is the overall config this whole tree is replacing.

This directly reversed the last round's "shortcut" additions (patch a
universe / add a connection straight from the protocol row) — those reached
past the interface level, exactly the violation being called out. Removed.

**Per level, now:**
- **Protocol** (`KIND_PLUGIN`): "Add an interface…" only. A network
  interface is a real host NIC (`QNetworkInterface::allInterfaces()`,
  confirmed in `ArtNetPlugin::outputs()`) that exists whether or not this
  tree currently shows it — `refresh()`'s liveness filter hides an idle one.
  There is nothing to fabricate, only something to stop hiding, so "Add"
  here picks a line (`pickPluginLine()`) and adds it to a new session-
  persistent `m_pinnedLines` set the liveness filter also checks. Honest
  framing, not literal interface creation. "Configure %1…" is now shown
  ONLY for plugins with no per-patch equivalent at all — confirmed
  DMX-USB/MIDI widget settings (speed, mode, …) are genuinely per PHYSICAL
  WIDGET with no tree row to live on — and hidden for target-capable
  protocols (ArtNet), where `ConfigureArtNet`'s dialog turned out to be
  nothing but a second, differently-shaped editor for the exact same target
  IP / ArtNet universe number / transmit mode a target or port row already
  edits directly.
- **Interface** (`KIND_LINE`): "Add a target on this interface…" for
  target-capable protocols — and, new, "Stop showing this interface" (the
  symmetric D, offered only once idle) to unpin one added above. "Patch a
  universe here" and "Patch a universe to a new target…" are GONE for
  target-capable protocols (ArtNet) — patching now always goes interface →
  target → port, never skipping a level. Kept as-is for lines with no
  target concept at all (DMX-USB, MIDI): for those the line already IS the
  endpoint, so this is the bottom of their hierarchy, not a skip.
  "Reroute…" (previous round) stays here too — it is a line-level concern.
- **Target** (`KIND_DEVICE`) and **port** (`KIND_PORT`): unchanged — already
  matched the spec (rename/add-port/forget-target; patch-into-port), no
  hierarchy violation found there.
- **Unpatched universe**: new "Patch to…" — the explicit reverse-direction
  entry point Branson asked for ("for unpatched .. right click .. patch to
  ip/port"). New `patchUnpatchedUniverseTo()` asks protocol → interface →
  (for a target-capable protocol) address/port, i.e. everything a normal
  interface → target → port click-through would have supplied, since there
  is no such row to click yet for an orphan universe.

Also fixed the menu-ORDER half of the ask: the universal actions (Collapse/
Expand, Add/Delete Universe) were being added to the shared `QMenu` before
any per-kind branch added its own items, so they always rendered first
regardless of what was clicked. Restructured so per-kind branches build
their own items first and a new `appendUniversalMenuActions()` appends the
generic ones immediately before whichever of the 9 `menu.exec()` call sites
actually fires — every menu now reads specific-to-this-row first, generic
last.

**Known gap, called out rather than silently built around:** the user's
model names "IP or multicast" as target kinds; this engine's ArtNet target
is a single address field with no separate multicast-specific handling —
typing a multicast group address into that field mechanically works (it's
just an IP), but there's no dedicated multicast UI/validation. Not built,
since inventing one wasn't asked for this round and would need its own look
at what ArtNet 4 multicast actually requires here.

Builds clean; launched and exited cleanly with no errors in the log
(3-second smoke check, not left running — see prior note on why a
background-launched instance doesn't persist for interactive use). **Not
interactively verified** — run it yourself with
`build/main/qlcconsole -o test-workspaces/surfacetesting.qxw` and check:
right-click ArtNet → only "Add an interface…" (+ generic actions after a
separator, no Configure); right-click an interface → only target CRUD, no
direct patch; right-click an Unpatched universe → "Patch to…" is there.

## Devices tree: menu order, IP-first patching, reroute (2026-09-02) — SHIPPED

Branson, on the entry point fix below: "better .." then three more asks in
one message.

**1. Menu ordering should be by hierarchy.** The universal actions (Collapse/
Expand, Add/Delete Universe) were being added to the shared `menu` object
BEFORE any per-kind branch added its own row-specific items, so they always
rendered first — a right-click on "ArtNet" led with "Add Universe" ahead of
anything actually about ArtNet. Restructured so per-kind branches build
their own items first, and a new `appendUniversalMenuActions()` appends the
generic ones right before whichever `menu.exec()` fires (all 9 call sites
now call it there instead of the actions being pre-built once at the top).
Every menu now reads specific-to-this-row first, generic/root-level last.

**2. "When I add an interface, shouldn't be asking for target IP — that
should be by universe, right?"** Correct, and this was a real design
mistake in the previous round: the protocol-level "add a connection" action
went straight to `patchToNewTarget()`, which demands an IP before anything
else — appropriate for aiming at one specific remote node (unicast), wrong
as the default flow for "just start using ArtNet," which normally broadcasts
and needs no address at all. "Patch a universe to this protocol…" (asks
which universe first, same as patching from a line row, no IP) is now
offered for EVERY protocol with any output/input line, including ArtNet —
previously it was withheld from ArtNet on the assumption "Add a connection"
covered it. The IP-first flow is still there as a clearly separate, now
better-labelled "Add a connection to a specific target (by IP)…", for when
unicast to one node is actually the goal.

**3. Reroute.** "if it bound against lo0 .. reroute all bound universes to a
different interface." Genuinely didn't exist — `retargetPatch()`/
`retargetSelection()` only re-aim a universe's TARGET ADDRESS on the SAME
line; nothing moved universes off a line entirely. New: right-click a line
row that has universes patched to it → "Reroute N universe(s) to a different
interface…", `ConnectionsTree::rerouteLine()`. Picks a new line (via the
same `pickPluginLine()` from the entry-point fix), confirms, then re-patches
every affected universe in one pass. Each patch's own settings (ArtNet
target IP, transmit mode, ...) live on the `OutputPatch` object itself and
survive untouched — confirmed by reading `Universe::setOutputPatch()` /
`OutputPatch::set()`, which reuse the existing patch object and only update
plugin+line — so this is a pure "which wire does it leave by" change, not a
re-patch from scratch. Output patches only for now; input/feedback reroute
was not asked for and `InputOutputMap::setInputPatch()` replaces the whole
patch object rather than reusing it, so it would need its own look at
whether the profile survives before doing the same trick there.

Builds clean, smoke-tested. **Not interactively verified** — try right-
clicking ArtNet with nothing patched (should lead with protocol actions,
"Patch a universe to this protocol…" should NOT ask for an IP), and
right-clicking a line with universes on it for the new Reroute action.

## Devices tree had no way in for a protocol with nothing patched yet (2026-09-02) — SHIPPED

Branson: "this doesn't allow to add an interface under artnet for instance
which it should," and laid out the model he wants: root = protocol (CRUD),
under a protocol = connection (CRUD, plus type-specific config), under a
connection = patch universe (CRUD). Most of that already existed in the
engine and the per-row menus (see the two entries below this one) — the
actual bug was narrower and explains "can't add an interface" literally:
`refresh()`'s liveness filter hides a LINE row (ArtNet's NIC, e.g.) until
something is already patched or heard on it, and "Show unused" defaults
off. So a protocol with nothing patched anywhere had every one of its lines
invisible, and every "add a target" / "patch a universe" action lived on a
line row — there was no way to reach them without first ticking "Show
unused", which nothing prompted anyone to know to do. Chicken, egg.

Two things fixed this without touching the visibility filter itself
(reintroducing every idle NIC by default was the exact clutter it exists to
avoid): the protocol (plugin) row's own context menu now offers "Add a
connection (target)…" for target-capable protocols (ArtNet today) and
"Patch a universe to this protocol…" for the rest (E1.31, OSC, and anything
future that patches directly without a per-node target) — both reachable
with zero visible lines. Neither needed a pre-existing line row: new
`ConnectionsTree::pickPluginLine()` resolves which line to use itself (the
only one if there's just one, otherwise a picker matching the same "alias
(NIC name)" labelling every line row already uses), then hands off to the
SAME `patchToNewTarget()` / `patchUniverseTo()` these actions already call
from a line row — no new patch logic, just a new way to reach it.

Root-level "CRUD a protocol" itself was not built as literal add/delete:
protocols are fixed compiled-in plugins (ArtNet, MIDI, DMX-USB, …), not
something a workspace can create or remove — "Configure %1…" (already
existed) is the U, the tree row is the R. Worth saying explicitly since it
diverges from the requested shape rather than silently doing something
different.

Builds clean, smoke-tested. **Not interactively verified** — try right-
clicking "Art-Net" (or another protocol) in Devices with nothing patched
yet; "Add a connection (target)…" should be there and should not require
ticking "Show unused" first.

## Devices tree hid every unpatched universe (2026-09-01) — SHIPPED

Branson, right after the round-2 right-click fix landed: "universe1 not
showing up in there.. neither is the one i added in the rigth click." Root
cause was a step deeper than the right-click gap itself: `ConnectionsTree`
(the "Devices" sub-tab) is rooted at `cache->plugins()` — protocol → line →
device → port → universe — so a universe only ever gets a row as a *child of
its patch*. A universe with nothing patched to it has no plugin/line/port to
attach under, so it never rendered at all, regardless of whether it existed
in the engine. That is true of Universe 1 on a fresh workspace (never
patched to anything) and of literally every universe `addUniverse()` can
ever create (a bare universe has no patch by definition) — so the brand-new
right-click "Add Universe" always produced a universe that then looked like
it had failed to appear.

Fixed in `ConnectionsTree::refresh()`: the existing per-plugin summary pass
already walks every row once to count devices/universes for the interface
detail text, so it now also collects every universe id it sees
(`patchedUniverses`) as it goes. After that pass, anything in
`m_doc->inputOutputMap()->universes()` NOT in that set gets a synthetic
top-level "Unpatched" folder with one child row per leftover universe (new
`KIND_UNPATCHED_UNIVERSE`). Right-click on one of those rows offers Rename
and Delete (reusing the existing `renameUniverse()`/`deleteUniverse()` —
`deleteUniverse()` already self-guards "only the last universe can go", so
no new guard logic needed); patching a row belongs to Overview/Detailed as
before, so no patch UI was added here.

Follow-up, same session: "no crud when right click cause every row is
used... how to fix." The universe visibility fix above didn't reach the
actual complaint from round 2 — the empty-space-only "Add Universe"/"Delete
Universe" menu (`item == NULL` case) is only reachable when the tree has
visible empty space below its rows, which a normally-patched rig (the usual
case, and now literally every row given the fix above) doesn't have. Empty
space was never a real route to Add/Delete Universe, just a theoretical one.

Fixed by offering Add/Delete Universe on every row's context menu, not only
empty space. `slotContextMenu()`'s per-kind branches (KIND_PLUGIN/DEVICE/
PORT ×2/LINE/UNIVERSE/UNPATCHED_UNIVERSE) each build and `exec()` their own
`QMenu`, but all reuse the SAME shared `menu` object built once at the top
of the function (already the case for the pre-existing "Collapse/Expand
everything below" actions) — so `addUniv`/`delUniv` actions added to that
shared object right after collapse/expand now show up in all of them. New
private helper `handleUniversalMenuAction()` centralizes the post-exec
handling (collapse/expand + add/delete universe) so each of the 8 exec()
call sites is a single delegating line instead of duplicated logic — same
shape as the existing collapse/expand check it replaces, at every site that
already had one.

Builds clean, smoke-tested. **Not interactively verified** — right-click any
row in Devices next time in the app; "Add Universe" (and "Delete Universe
[last]…" once one exists) should appear at the top of every menu, not just
on empty space.

Follow-up regression, caught by Branson actually clicking around: "can't add
device again on the devices page .. we had fixed that?" The "Unpatched"
folder heading added by the fix above (see the entry below this one) has no
`ROLE_KIND` — it's a label, not a plugin/line/device/universe — so it
matched none of `slotContextMenu()`'s `if (kind == KIND_X)` branches, each
of which builds AND execs the menu itself. Falling through all of them meant
the menu was built (with Add/Delete Universe on it) but never shown —
right-clicking the folder header did nothing, at all, silently. Fixed with a
fallback `menu.exec()` at the end of the function for any row that matches
no specific kind. Builds clean, smoke-tested; not re-verified interactively.

## Connections right-click, round 2 (2026-08-18) — SHIPPED

Branson: "still cannot right click to add/manage connections" after the
earlier fix — that fix only reached the "Detailed" sub-tab's universe
list. Connections actually has three sub-tabs (Devices/Overview/Detailed),
each a structurally separate widget; right-click needed checking on all
three, not assumed from one.

- **Overview** (`UniversePatchGrid`) — had Add/Remove Universe on its own
  toolbar already (`onAddUniverse()`/`onRemoveUniverse()`), but no
  right-click at all. New `onContextMenu()`, wired to the table's
  `customContextMenuRequested`, reuses those same slots.
- **Devices** (`ConnectionsTree`) — Branson overrode the "read-only on
  purpose" comment directly: "I DO want to be able to manage adding
  devices in there." That comment is gone (was stale anyway — the tree
  already had rich per-row editing via right-click: rename/patch/unpatch/
  retarget/delete a specific universe; the only real gap was that
  right-clicking *empty space* did nothing at all, `if (item == NULL)
  return;`, since "add a universe" isn't a property of any existing row).
  New `ConnectionsTree::addUniverse()` (mirrors the existing
  `deleteUniverse()`'s undo-capture pattern) wired into that empty-space
  case: right-click empty space in Devices now offers "Add Universe" and
  "Delete Universe [last one]…" (reusing `deleteUniverse()`'s own already-
  solid "only the last can go" guard/confirm/undo logic, not duplicated).
  Deliberately did NOT thread these into the existing per-kind branches
  (KIND_PLUGIN/DEVICE/PORT/LINE/UNIVERSE each build and `exec()` their own
  menu separately) — too much surface to touch safely in one pass; the
  empty-space case was the actual gap and the lowest-risk fix for it.
- **Detailed** — already had it from the earlier round, unchanged.

Right-click now works on all three Connections sub-tabs. Builds clean,
smoke-tested. **Not interactively verified** (no GUI-automation path for
right-click context menus in this session) — try right-clicking empty
space in Devices, the Overview grid, and the Detailed universe list next
time in the app.

---

## Footer consolidation round 2 (2026-08-18) — SHIPPED

Branson, from a footer screenshot: the rig-readiness warning sat far to
the right (past DESIGN), disconnected from "Ready" on the far left even
though a readiness problem is exactly what that slot should mean; and
Saved/Unsaved/Autosave were three separate chips that could be one.

- **Output-readiness warning moved into `m_statusModeLabel`'s own slot**
  (far left) instead of a separate right-side chip (`m_statusRigLabel`,
  removed). New `m_outputReadinessWarning` (QString, empty when fine) —
  `updateOutputReadiness()` sets it and calls `updateStatusBar()`, which
  now shows it (red, bold) in priority over both the transient
  `m_statusMessage` and the idle "Ready" text. The detailed per-universe
  tooltip moved with it onto `m_statusModeLabel`.
- **Unsaved/Autosaved/Saved consolidated into one chip** (`m_statusDirtyLabel`;
  the separate always-visible `m_statusAutosaveLabel` "Autosave: Enabled"/
  "Last autosave: HH:MM:SS" chip is gone). Driven by three existing signal
  points, no new tracking state needed: `slotDocModified(true)` →
  "● Unsaved changes" (orange), the autosave-completion point in
  `saveXML()`'s autosave path → "Autosaved HH:MM:SS" (gray),
  `slotDocModified(false)` (a real manual save) → "✓ Saved" (gray).
  Autosave deliberately does NOT clear `Doc::isModified()` (confirmed in
  `saveXML()` — only a real save does, via `resetModified()`), and
  `Doc::setModified()` emits `modified(true)` unconditionally on every
  edit, not just the dirty transition — so the very next edit after an
  autosave re-fires `slotDocModified(true)` and flips the chip back to
  "Unsaved changes" on its own, correctly, with no extra bookkeeping.
  "Autosave: Disabled" as a persistent chip is gone too — matches
  Branson's explicit 3-state ask; still configurable via Preferences, just
  no longer occupying constant footer space.

Builds clean, smoke-tested (stable, no crash). **Not visually confirmed** —
tried reading label text via `osascript`'s accessibility bridge this round
specifically (not just resizing/window-existence checks like earlier
rounds), but Qt's static-text elements didn't expose readable values that
way. Worth an eyeball check of the actual chip text/colors next time in
the app, especially the readiness-warning priority-over-"Ready" logic and
the autosave→edit→"Unsaved changes" flip.

**GM/Blackout — resolved differently than proposed, SHIPPED**: recommended
consolidating the footer presentation (one combined "is output actually
happening" chip); Branson redirected instead to "add preference to show GM
in bottom as option" — GM's footer presence is now a View menu toggle
(`m_showFooterGMAction`/`m_showFooterGM`, persisted
`workspace/showFooterGM`, defaults on), same pattern as the existing Load/
Power chip toggles. GM and Blackout stay fully separate elements; the
"consolidate" question was answered by making GM optional rather than
merging it with anything.

**Readiness text simplified**: dropped "NOT READY -" from the warning —
Branson: it "doesn't need to say NOT READY, just needs to be either
'ready' or 'why not' in red." The red bold styling already says "this is a
problem"; the text is now just the reason ("Universe %1 has no output"),
matching "Ready"'s own plain phrasing in the fine case.

Builds clean, smoke-tested. **Not visually confirmed**, same caveat as
everything else in this round.

---

## Toolbar real-estate + title bar (2026-08-18) — SHIPPED

Branson: a Fixture Manager toolbar screenshot showed icon+text spacing so
wide it was "untenable." Two asks: make toolbars more space-efficient, and
consider moving controls into the title bar.

- **Manager toolbar icon size, unified at 20x20** — `fixturemanager.cpp`,
  `functionmanager.cpp`, `showmanager.cpp` (both its main and bottom
  toolbars) never called `setIconSize` at all, falling back to a large
  platform default; `inputoutputmanager.cpp` was 32x32, `virtualconsole.cpp`
  was 26x26. All six now match `showtimelineeditor.cpp`'s existing 20x20
  rather than inventing a new size. This alone shrinks every manager
  toolbar meaningfully, especially combined with long action labels
  ("Channels Fade Configuration") under the existing default
  icon-above-text label mode (itself already a user-toggleable View-menu
  setting, unchanged).
- **App-level toolbar merged into the title bar** — `setUnifiedTitleAndToolBarOnMac(true)`
  in `App::initToolBar()`, macOS-only. Only reaches the small app-level
  toolbar (Panic/Blackout/Blind/Operate); the per-manager toolbars (like
  the one in the screenshot) are embedded inside each tab's own widget via
  `layout()->setMenuBar()`/`addWidget()`, not QMainWindow toolbars, so this
  API structurally can't reach them — confirmed with Branson before
  building, scoped to just the app-level one.

**A real puzzle surfaced while verifying this, worth recording honestly
rather than glossing over**: re-checking the window-minimum-width fix from
earlier today (`QSizePolicy::Ignored` on tab pages), the minimum had grown
from the previously-confirmed 791px to 1111px. Bisected hard — reverted the
title-bar merge (call removed entirely, not just set false), reverted all
six icon-size changes, and cleared the persisted `workspace.geometry`
setting (`defaults delete org.qlcplus.qlcconsole "workspace.geometry"`,
confirmed via a *different* initial window size afterward that this wasn't
just a stale restore) — none of it moved the number. Every specific
hypothesis from today's own changes came back negative. What matters:
**1111px is still comfortably under the 1512px screen and the window
resizes freely** — the actual bug (wider than the screen, unresizable) is
confirmed gone; the 791→1111 shift is unexplained but not a regression of
the resizability fix itself. Mid-investigation, `osascript`/System Events
stopped being able to query windows for *any* app (Finder, Safari, the
frontmost process itself all failed identically) — a macOS Accessibility
session issue unrelated to qlcconsole, not something fixable from in here.
If this is worth chasing further, it needs a real interactive session
(Instruments/Qt's own layout debugging, not blind bisection from outside).

Builds clean throughout. **Not visually confirmed** — same caveat as
everything today, compounded by losing GUI-automation access partway
through; worth a real look and a real resize-by-hand next time in the app.

---

## 🎨 Visual consistency review (2026-08-18) — SHIPPED (the confirmed bugs)

Full findings: [VISUAL_CONSISTENCY_REVIEW.md](VISUAL_CONSISTENCY_REVIEW.md).
Separate from the workflow UX review above — spacing/icons/color/theming,
requested directly ("did you also evaluate look and feel for consistency").
Key fact: `App::applyTheme()` does a real app-wide `QPalette` swap, so ANY
hardcoded hex color is a deliberate theme opt-out, not just style — and two
footer chips added earlier today (mode chip, Show Lock) had exactly that
bug, confirmed via computed contrast against all 3 dark themes. **Fixed**:
mode chip now uses `palette(text)`/a higher-contrast green; Show Lock moved
off Blackout's identical red onto the app's existing `#a06000` amber
convention, so it no longer reads as the same alarm tier as Blackout.

**Still open, ranked in the doc's own "order of attack"**: toolbar icon
size unification (24/26/32/platform-default/none across 5 managers — the
single most visible inconsistency found, since every tab-switch shows it);
LookEditor's Color-page sliders missing numeric value labels (a genuine
loose end from this session's own slider work, Dimmer page has them,
Color doesn't); Delete/Remove icon consolidation (3 different icons, one
concept); one more isolated dark-theme legibility bug in
`timecodecalibrationdialog.cpp`; everything else (icon family drift,
semantic-red consolidation, section-header convention, dialog margins,
Save button's one-off arm/confirm pattern, tooltip voice) — lower
urgency/broader surface, deferred to a dedicated pass.

---

## Main window unresizable / off-screen — real bug, found and fixed
(2026-08-18) — SHIPPED

Branson reported the window couldn't be resized narrower and was hanging
off the edge of the display. First hypothesis was today's own footer work
(GM fader, mode chip) — reasonable given recency, but WRONG: trimming the
GM slider's `setFixedWidth(80)` to a `setMaximumWidth`, and
`m_statusModeLabel`'s hard `setMinimumWidth(300)` down to 160, had zero
measurable effect on the window's minimum width. Verified this empirically
throughout rather than trusting code-reading alone — `osascript`/System
Events driving the real built app, resizing the actual window and reading
back its real size, since there's no screenshot tool available here. That
discipline is what caught two dead-end fixes before landing the real one:
a speculative `ElideRight` swap for the tab bar's `ElideNone` (a 2021
upstream macOS workaround, `ff84d8047`, "try to workaround TabWidget
icon+text issue") also didn't move the number.

**Root cause**: `QTabWidget`'s internal `QStackedWidget` sizes itself to
the LARGEST of ALL its pages by default — documented Qt behavior — not
just the currently-visible one. So whichever of the app's 8 tabs
(Connections, Fixtures, Lighting Studio, Functions, Programming, Shows,
Virtual Console, Simple Desk) happens to need the most width sets the
floor for the WHOLE WINDOW, regardless of which tab is active. Confirmed
by switching the active tab (Connections ↔ Fixtures) and seeing the
minimum stay bit-for-bit identical at 1525px — on a 1512px-wide display,
2px over what even a maximally-shrunk window could fit, so the app opened
wider than the screen and couldn't be pulled back in.

**Fix**: `app.cpp`'s shared `addTab` lambda (the single choke point all 8
tabs already funnel through) now sets `QSizePolicy::Ignored` on both axes
for every tab page right before adding it — the standard, documented
pattern for excluding a stacked page from that max-of-all-pages
aggregation while hidden; it still lays out and renders completely
normally once it IS the current page. Verified: window now opens at
1510px (fits the screen), resizes freely down to 500×400 and back up to
1400×900, and tab-switching (tested Virtual Console/Simple Desk/
Connections) still works with no crash. The speculative `ElideRight`
change was reverted back to `ElideNone` once the real fix was confirmed —
it turned out unnecessary (the remaining tab-bar-driven minimum with
`ElideNone` restored is ~791px, comfortably inside the screen), so there
was no reason to risk reintroducing whatever 2021 rendering bug
`ElideNone` exists to work around. The GM-slider and mode-label width
trims were kept regardless — real improvements (letting the layout
actually compress under pressure instead of a hard, unshrinkable floor),
just not the fix for this specific bug.

Builds clean; this one got an actual behavioral check via `osascript`; the
tab-switch/crash check above ran against the real binary too — more
verification than most of today's changes got.

**Follow-up**: Branson noticed moving the GM fader reflowed every chip to
its right, since the value label's width floated with its text ("1%" vs
"100%"). Fixed to the widest possible reading, same reasoning
`m_statusLoadLabel` already used elsewhere in the footer. Reverified the
resize fix still holds afterward (791px minimum, unaffected by 2 extra
pixels) — no regression.

---

## Footer chip consistency pass (2026-08-18) — SHIPPED

Started as two follow-up asks after the workflow UX review's GM/mode-chip
work: move GM to the left of the footer (next to the mode-message label,
`addWidget` not `addPermanentWidget` — non-permanent, like
`m_statusModeLabel`, since this app doesn't use `statusBar()->showMessage()`
so there's no flicker risk), and make the Design/Operate chip itself
clickable (`installEventFilter`, triggers the same `m_modeToggleAction` the
toolbar button does — one control, not two).

That led to a real design question from Branson: the footer had accumulated
several different "how do we show state" idioms with no shared logic —
Blackout used a `●` dot, Show Lock a 🔒 emoji, Timecode mixed an emoji AND a
dot, Power had an emoji with no dot, the new Mode chip had neither. Landed
on a two-tier answer: **alarm-tier states that are meant to interrupt**
(Blackout, Blind) keep their distinct, deliberately-loud treatment — Blind's
whole-footer-blue and Blackout's dot are staying different from each other
and from everything else on purpose, that's the point. **Passive-readout
chips** (Mode, Timecode, Power, Show Lock, engine Load) converge on one
plain style: bold text + a meaningful color, no decorative emoji icon.
Dropped 🔒 (Show Lock), ⏱︎ (Timecode, 4 call sites), ⚡︎ (Power), ⚙︎/⏱ (Load).
**Deliberately kept**: Blackout/Show Lock's own `●`-in-Blackout and
Timecode's internal `◌`/`●`/`❚❚` state glyphs, Dangle's `⚠︎`, Dirty's `✓`,
Timeline-suspended's `❚❚` — Branson's own distinction, confirmed:
these carry real at-a-glance state meaning (same job as Blackout's dot),
not decorative branding icons, so they're a different thing from the emoji
prefixes that got removed. Show Lock's text also moved from an inline HTML
`<span style=...>` to a `setStyleSheet()` call at construction, matching
every other chip's convention, and its display text changed from "Show
locked" to "SHOW LOCKED" to match the all-caps convention Blackout/Mode
already use.

Builds clean, smoke-tested each step. **Not yet visually confirmed** —
this was a rapid design-question → implementation cycle without a
screenshot checkpoint; worth a look next time the app's open.

---

## 🔍 Workflow UX review (2026-08-18)

Full findings: [WORKFLOW_UX_REVIEW.md](WORKFLOW_UX_REVIEW.md). Requested
while away from PMJ hardware: a discoverability/simplicity review across
fixture setup, Programming-tab look-building, Function Manager/Show-Timeline,
and Virtual Console/live-operating. Core pattern found: the app already has
the right answer in several places (empty-state hints, footer safety chips,
content-aware canvas tiles) — it's just not applied everywhere it's needed
yet. **A separate look-and-feel/visual-consistency pass (spacing, icon sets,
color usage, theme compliance) was explicitly NOT part of this review and is
still open** — Branson asked directly, flagging it rather than letting it
slide.

**Shipped (2026-08-18), the pure-mechanical items — no design decision
needed:**
- Programming tab's Save button renamed "Save Positions" + clarifying
  tooltip (was mislabeled as general Save; ordinary look edits are already
  tracked by the existing app-wide footer "● Unsaved changes" indicator,
  `programmingmanager.cpp`).
- Tab tooltips distinguishing Fixture Groups vs. Channel Groups
  (`fixturemanager.cpp`); clarified tooltips on the shared Add-group action
  and the dynamic Add-action tooltip ("Add channel group..." vs. "Add
  fixture...").
- Head-layout-grid tooltip on `CreateFixtureGroup`'s size fields
  (`createfixturegroup.ui`).
- Palette-type tooltips in the Programming tab's "New…" menu for all nine
  types, with Pan/Tilt vs. Aim spelled out explicitly since they look like
  synonyms and aren't (`programmingmanager.cpp`;
  `QMenu::setToolTipsVisible(true)` was needed too — off by default on most
  platforms including macOS).
- Programming tab's empty-palette-tree guidance folded into the existing
  canvas placeholder text (both the constructor default and the runtime
  `setText()` call) rather than a new separate hint.
- Also: builds now use performance cores only, not `hw.ncpu`
  (`CLAUDE.md`, `check-all.sh`) — Branson asked for this directly mid-review;
  see memory `build-core-moderation`.

**Shipped (2026-08-18), design decisions made:**
- **Grand Master promoted to a compact footer fader** — Branson chose "always
  visible/adjustable" over a readout+popup. New `m_statusGrandMasterSlider`/
  `m_statusGrandMasterValueLabel` in `app.cpp`'s footer, wired to the same
  `InputOutputMap::setGrandMasterValue()`/`grandMasterValueChanged()` hooks
  the VC-embedded `GrandMasterSlider` uses, so both stay in sync. Deliberately
  a new compact widget rather than reusing `GrandMasterSlider` itself — that
  class is a tall vertical sidebar widget (min. 100px), not built for a
  ~20px status-bar strip.
- **Persistent Design/Operate mode chip** — new `m_statusModeChipLabel`,
  updated in `slotModeChanged()` alongside the existing destination-mode
  toggle button (left untouched). Green "OPERATE" / gray "DESIGN".
- **Chaser-step and Show-timeline content swatches** — Branson chose a color
  swatch over a mini-preview thumbnail. New `AppUtil::sceneSwatchColor(doc,
  scene)` (`apputil.{h,cpp}`) — the RGB of the first Color-type palette a
  scene references, invalid `QColor` (no swatch, falls back to the generic
  icon) if it references none, e.g. a pure pan/tilt or dimmer-only scene.
  Wired into `ChaserEditor::updateItem()` (replaces the generic per-type
  icon with a swatch dot when available) and `SceneItem::paint()` on the
  Show timeline (a small dot drawn top-left, deliberately separate from
  `ShowItem`'s existing top-right type badge and from the block's own
  `m_color`, which is a user-assigned organizational color unrelated to
  scene content — conflating the two would have been a real bug). Sequences
  (chaser-driven timeline items) were NOT touched — a sequence has multiple
  per-step scenes with potentially different colors, no single obvious
  swatch, and its lack of inline editing is already a separately-flagged
  open gap (see item 1 below).
- **Naming-convention nudge** — Branson chose placeholder-text-only, but
  there's no dialog/QLineEdit at scene/chaser creation to put placeholder
  text on (creation is silent auto-name + tree-inline-rename, and Qt's
  built-in tree item editor doesn't support placeholder text without a
  custom delegate — real extra engineering, out of scope for "cheap").
  Landed the same-spirit, actually-cheap equivalent instead: tooltips on
  the "New scene"/"New chaser" toolbar actions
  (`functionmanager.cpp`) spelling out the convention with a concrete
  example. Also **deliberately did not hardcode a "learn from siblings"
  auto-namer** — CLAUDE.md is explicit the convention should be learned
  from sibling names, not hardcoded, and no such inference logic exists
  anywhere in the codebase to reuse; building one from scratch is a real
  feature, not a nudge, and wasn't what was asked for here.
- **Connections-tab onboarding hint** — the actual first tab a new
  workspace opens on (not Fixture Manager). A fresh `Doc` always has 4
  default universes (`Doc::Doc`'s default arg), so "count == 0" never
  applies here the way it does for Fixture Manager's fixture tree — the
  hint instead shows whenever none of them have any input/output patch
  yet, hiding once the first one does (`InputOutputManager::updateList()`,
  new `m_onboardingHint`).

**Shipped (2026-08-18) — SceneItem double-click:**
- **Timeline Scene clips now have an edit action** — double-click a bare
  Scene block on the Show timeline (`SceneItem::mouseDoubleClickEvent()`,
  new) and it jumps to the Programming tab with that scene loaded, reusing
  `ProgrammingManager::showFunction(fid)` — an existing public entry point
  ("exactly as clicking it in the nav tree would") that just had nothing
  wired to call it from the timeline before. New
  `App::switchToTabContaining(QWidget*)` does the actual tab-switch
  (`m_tab->indexOf()`/`setCurrentIndex()`), reached from the
  QGraphicsItem via the same `qApp->topLevelWidgets()` →
  `qobject_cast<App*>` → `findChild<ProgrammingManager*>()` pattern already
  used elsewhere (`monitor.cpp`'s Quit handling; `pmjoverlay.cpp`'s own
  reach-pattern this session). Chose "jump to Programming tab" over a
  separate Scene Editor dialog — that's where this fork's actual editing
  workflow lives (CLAUDE.md), a legacy raw-channel Scene Editor would be
  the wrong destination for this fork specifically.
- **Sequences deliberately NOT touched in this pass** — `SequenceItem` has
  real per-cue complexity already (`cueAt()`, `selectCue()`, per-cue drag
  state) that a double-click handler needs to account for (which CUE was
  double-clicked, not just "the sequence"), and DONE.md's existing
  "Sequences: edit in Functions tab hint" note is really the same
  underlying gap — deserves its own focused pass rather than bolting on a
  rushed version here.
- Builds clean, smoke-tested (starts/runs fine). **Not interactively
  verified** — double-clicking a `QGraphicsScene` item can't be driven via
  `osascript`/System Events (scene items aren't separate accessibility
  elements the way native widgets are), so this needs a real click-test in
  the app: open a Show with a Scene clip on the timeline, double-click it,
  confirm it lands in the Programming tab with that scene loaded.

**Item 2 (smaller/lower-urgency batch) — shipped (2026-08-18):**
- **Cue list next-cue indicator — investigated, no fix needed.**
  `VCCueList::setFaderInfo()`'s orange `#FF8000` highlight
  (`vccuelist.cpp:1164`) has no gating on crossfade-panel visibility at
  all — traced its only caller, `slotCurrentStepChanged()`, and the
  `FaderMode::None` branch (no crossfade fader configured, the common
  case) calls it unconditionally too. The original review finding was
  imprecise; didn't force a speculative change against a check that
  already appears correct on inspection — flagging instead of guessing.
- **Design→Operate checkpoint** — mirrors `slotModeDesign()`'s existing
  "there's something you might lose" warning, but gated specifically on
  `Doc::isProgrammerDirty()` (uncommitted pad/Programming-tab edits — colors,
  positions, etc. never saved into a scene), not `isModified()`, which
  is true almost continuously while actively building a show and would
  have reintroduced exactly the "friction on every toggle" the design
  call was meant to avoid.
- **SceneGroupLooks drop-zone precision** — `dropEvent()` now checks which
  column (`m_targetList` vs `m_lookList`, both direct children of
  `SceneGroupLooks` so their `geometry()` is directly comparable to the
  drop position) a palette/fixture-group actually landed on, rejecting
  (with a `QToolTip` explaining why — the original finding noted there
  was no rejection/hint path at all) only the CLEAR wrong-column cases;
  drops landing between/outside both lists still fall through to the
  previous permissive behavior.
- Builds clean, smoke-tested (starts/runs). **Not interactively verified**
  — same caveat as item 1: needs a real click-test (try dropping a palette
  on Targets, a fixture group on Looks, toggle to Operate with an
  uncommitted pad edit) next time in the app.

**Still open:**

1. Timeline vocabulary (Track/Cue/handles) never explained in-app — no
   glossary, first-run hint, or "?" affordance anywhere in the Show
   timeline. Lowest urgency of what's left; not yet designed.

---

## 🔌 Connections tab — remaining

*(Shipped work moved to [DONE.md](DONE.md), 2026-08-28.)*

- **Hardware verification — status.** Use `build/tools/artnetprobe/artnetprobe`
  (`--selftest` / `--poll <addr>` / `--listen`).
  - ✅ **Arrival-interface reporting**: verified 2026-08-28 against live
    traffic. Real ArtPollReplies from 172.18.2.218 and 172.18.2.10 were both
    correctly attributed to `vlan0`, 5/5 datagrams carrying an index. This is
    the routing fix confirmed on real gear, not a simulation.
  - ✅ **USB devices appear**: DMXKing ultraDMX Micro and OpenDeck PMJ_BLACK_1
    both listed under DMX USB / MIDI.
  - ⚠️ **Off-segment unicast probe**: unproven. Every node reachable from this
    host is on a subnet it has an interface on, so it answers broadcast polls
    too and the reply is indistinguishable. Needs a node on a subnet with no
    local interface.
  - ⚠️ **Hand-declared target actually passing DMX**: unproven, and NOT
    testable from one host. Console and probe both bind 6454 with
    `ShareAddress`; broadcast reaches every bound socket but unicast reaches
    exactly one, so the probe cannot see the console's own unicast output.
    Run the probe on a different machine from the console.
- **Devices/Overview parity — done.** Bulk multi-row retarget with port
  auto-increment, feedback IP/port, ArtNet `inputUni`, MIDI output mode, MIDI
  input channel and the MIDI Out-vs-Feedback role swap have all landed on the
  Devices tab. Universe passthrough and the plugin
  description/status have since moved onto Devices too. Still Detailed-only
  and staying there deliberately: the Audio tab, input-profile
  creation/editing (`InputProfileEditor`), and the USB hotplug toggle (an app
  preference more than a patching control). The split now reads as
  "Devices = the rig and its wiring, Detailed = audio and profile editing".
- **Patch undo — remaining limits.** `PatchUndo` (engine/src/patchundo.{h,cpp})
  is ONE step deep: a second change discards the first, deliberately, since a
  state two changes stale would restore onto a rig that has moved under it.
  It covers patches and the universe list (add/delete/name/passthrough) from
  all three tabs, but nothing outside the patch — fixtures, functions, scenes.
  Ctrl+Z is scoped to the Connections widget (`Qt::WidgetWithChildrenShortcut`)
  so it cannot promise general undo elsewhere in the app.
- **`InputOutputMap` universe id vs index — not a live trap after all.**
  `universe(id)` resolves by ID while `outputPatch()`/`setOutputPatch()`/
  `setInputPatch()` take an ARRAY INDEX, but id == index is enforced
  deliberately at both mutation points, not merely coincidental:
  `addUniverse()` assigns the next index as the id, refuses an id already
  present, and fills gaps so a higher id still lands at its own index;
  `removeUniverse()` refuses anything but the last entry. The contract was
  written nowhere, so it is now pinned by
  `InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex()` — relax either
  rule and that test fails instead of the app repatching the wrong universe.
- **Reachability probing — verify on `ender`.** Rescan now unicasts an ArtPoll
  at every address believed in but not heard from (hand-declared targets and
  addresses named by `outputIP`), so a node on another subnet can answer. Never
  tested against a real off-segment node. Note it probes only on an explicit
  Rescan, not on the 5 s tick — a live "is it up" indicator would need a
  cadence decision, and continuous unicast polling of every configured node is
  exactly the traffic this has been avoiding.
- **Probing is Art-Net only.** `QLCIOPlugin::probeTarget()` defaults to a
  no-op; nothing else implements it. Fine today, since Art-Net is the only
  plugin with addressable targets at all.

---

## ✈️ Travel / offline work — no show rig or control surface needed

Buildable + testable on a laptop (offscreen QTest / node for JS effects; the app
runs headless via `QT_QPA_PLATFORM=offscreen`). Good picks while away from the rig:

- **More one-shot effects** — now that the lifecycle exists (`EFFECT_LIFECYCLE_DESIGN.md`),
  author bursts / reveals / sweeps as `oneshot` scripts (pure JS, node-testable).
  A `wand.js` is the template.
- **Effect-lifecycle follow-ups** — `span` sync on the **Show timeline** (today falls
  back to naturalDuration); fold the RGBScript `Once` path into the lifecycle;
  per-look `syncTo`/`onFinish` override UI.
- **Audio effects** — bump the FFT band count / raise the 5 kHz cap for finer Hz
  targeting (node-testable); more audio-reactive scripts.
- **Rebrand → qlcconsole** — titles / About / launcher / macOS bundle names. No hardware.
- **Design-doc work** — "Look" as first-class assembly unit; unified object editor.
- **More stage objects** — flats / drapes / set pieces (2D monitor, offscreen-testable).
- **Small polish** — MTC-chip already done; any bugs found reviewing the code.

**Parked until back at the rig:** the whole **control-surface** effort (PMJ / APC40
mk2 / Xbox — needs the boards) and the **`RIG_TEST_PLAN.md`** verification pass
(needs movers / pixel panel / MIDI keyboard / audio in). The move-in-black + note-
effect timing items also want the rig to confirm.

---

## Recently shipped (verify on rig, then move to DONE.md)

- **PMJ Blackout LED bug — SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}`.
  Branson: "the blackout button still doesn't light on the board." Root
  cause: `PMJOverlay` was never listening for `InputOutputMap::blackoutChanged`
  at all — only Blind's `outputInhibitedChanged` and grand master were wired,
  despite a stale doc comment claiming blackout was covered too. Pressing `O`
  correctly toggled blackout every time; nothing ever told the engine to
  repaint LEDs afterward, so the LED just sat at whatever it was on connect.
  Added `slotBlackoutChanged`, mirroring the existing Blind/GM pattern
  exactly. Builds clean, smoke-tested. **Not yet re-verified on the real
  board.**

- **P2 slice 1 — selection mode: Select/Load wiring + per-fixture intensity
  faders — SHIPPED (2026-08-18).** `engine/src/programmercontroller.{h,cpp}`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/programmingmanager.{h,cpp}`. First
  buildable piece of the P2 design (`CONTROL_SURFACE_DESIGN.md`) agreed
  after Branson asked for "a cohesive think" on tying a whole SCENE to the
  faders, not just one focused palette.
  - **New engine primitive**: `ProgrammerController::writeChannelLive
    (fixtureId, channel, value)` — general-purpose, palette-agnostic raw DMX
    write, mirroring `VCSlider::writeDMXLevel`'s mechanism exactly (grabs/
    reuses a `GenericFader` via `Universe::requestFader()`, sets the
    `FadeChannel`'s target). Verified via code audit that this is safe as a
    one-shot call rather than needing MasterTimer registration:
    `Universe::processFaders()` (`engine/src/universe.cpp:333`) writes every
    outstanding requested fader on its own, every tick, regardless of who
    last touched it — so the target persists without re-invoking this method
    every frame. Also routes the edit for Save-bookkeeping the same way
    VCSlider does (`Doc::routeProgrammerEdit()`, falling back to
    `Doc::setProgrammerValue()`). This was the one deliberately-deferred item
    from the P2 design write-up ("needs a ProgrammerController method audit
    to find the right entry point") — audited, then built.
  - **Selection substrate**: reuses `ProgrammerController::
    setProgrammerSelection()`/`programmerSelection()` directly (not
    `programmerSubSelection()`, which keeps its original narrower "deviate
    individual fixtures out of a group palette" purpose, untouched by PMJ
    for now). New `PMJOverlay::sceneTargetFixtures()` defines "canvas order"
    concretely: the scene shown in the Programming canvas
    (`ProgrammingManager::currentSceneId()`, new getter, same pattern as
    `currentPaletteId()`)'s fixture-group members (expanded, group order)
    then its individually-fixed fixtures, deduped. **Not yet paged past
    10** — a scene with more targets only exposes the first 10 for now
    (flagged, not silently pretended-away).
  - **Select(N)**: toggles that target fixture into/out of
    `programmerSelection()` — multi-select, confirmed with Branson.
    **Load(N)**: replaces the selection with just that one target, matching
    the Role's own doc comment ("load item N into the programmer").
  - **Faders 1-6, Selection mode**: when nothing's focused in the Look
    Editor (`faderInUse()` false — Look-edit mode still wins when it
    applies), fader N writes the intensity channel of the Nth selected
    fixture live via `writeChannelLive()`.
  - **LED highlighting**: Select/Load(N) lit (`Valid`) when strip N has a
    real target in the open scene, brighter (`Selected`) when that fixture
    is actually in the current selection — refreshes live via the existing
    `programmerSelectionChanged` signal (already built, previously unused by
    any UI).
  - **Deferred to a follow-up slice** (per the design doc's own list):
    faders 7-10 (RGBW of selection), Enc 3/4 (Focus/Zoom of selection),
    Reset(N) zeroing a selection-mode fader (currently Reset only covers
    Look-edit mode's `faderInUse()` faders), and the >10-target paging via
    the `Page` button.
  - Builds clean, smoke-tested. **Not yet verified against the real board —
    this is genuinely new, first-time-tested DMX-write plumbing, unlike P1's
    palette-refresh-based writes**, so worth an attentive first test rather
    than an "it built, ship it" pass.

- **Blind/Blackout footer indicators — SHIPPED (2026-08-18).** `ui/src/app.{h,cpp}`.
  Branson: "we don't need two bars for blind, the footer being blue is enuff" —
  removed the redundant `m_statusBlindLabel` text chip (Blind already turns
  the whole footer blue, unmistakable on its own); the toolbar button's
  checked state is untouched. Added a Blackout counterpart Branson asked for
  ideas on — chose the smaller of two options (a compact chip, not a
  whole-footer-red treatment like Blind's): new `m_statusBlackoutLabel`, a
  red "● BLACKOUT" chip shown/hidden in `slotBlackoutChanged()`. Builds clean.

- **P1 slice 8 — numbered fader labels + Up-button reset/highlight —
  SHIPPED (2026-08-18).** `engine/src/controlsurface.h`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/lookeditor.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`. Two asks from Branson after slice 7
  landed: (1) "it'd be nice to see what faders we're moving... number them
  for clarity," (2) "we can also use the fader up button to reset and
  highlight the faders in use."
  - **Numbered labels**: the Color page's R/G/B were previously *inside*
    `QColorDialog` (opaque, can't inject labels) — pulled them out into the
    same numbered-vertical-slider pattern the White/Amber/UV sliders already
    used, so all six read "1 R" "2 G" "3 B" "4 W" "5 A" "6 UV," matching
    `PMJOverlay`'s fader-index mapping exactly. Bidirectional sync with the
    dialog's own picker (`slotColorChanged`/new `slotRgbSliderChanged`, both
    `blockSignals`-guarded to avoid feedback loops); `commitColor()` now
    reads RGB from the numbered sliders (the canonical source) rather than
    `m_colorDialog->currentColor()`.
  - **Reset + highlight**: new `ControlSurface::RoleType::Reset` (index =
    strip N) in the P0 engine core — device-agnostic, so APC40/Xbox overlays
    can reuse it later. PMJ's 10 "N-Up" buttons (previously `CS::Role()`,
    completely unbound) now carry it. New shared `PMJOverlay::faderInUse(int)`
    is the single source of truth for "does fader N do anything right now,"
    used by all three of: the Level write path (refactored to call it instead
    of duplicating the Color/Dimmer type check inline), the new Reset write
    path (zeroes that channel via the same `setDesignColorChannel`/
    `setDesignDimmerValue` from slice 7, value=0 — no new engine write method
    needed), and `stateFor()`'s new `Reset` case (lights the Up button
    `State::Valid` exactly when `faderInUse()` is true).
  - **Live LED refresh on focus change** (needed for the highlight half to
    update the instant a different look is clicked, not just on the next
    unrelated interaction): new `LookEditor::lookFocusChanged(quint32)`
    signal, emitted at the end of every `setPalette()` call (both the
    empty-palette and normal paths) — re-emitted by `ProgrammingManager` as
    `currentPaletteIdChanged`. `PMJOverlay` couldn't connect to this at
    construction time (`ProgrammingManager` isn't built yet when
    `App::initDoc()` constructs `PMJOverlay` — same ordering constraint
    slice 4 hit) — solved with new `programmingManager()`, a `const` helper
    that lazily finds-and-connects-once on first use (`mutable` cache member),
    reused at all four call sites that previously each did their own
    `m_app->findChild<ProgrammingManager*>()`.
  - Builds clean, smoke-tested. **Not yet verified against the real board.**

- **PMJ hardware fix (not code) — OpenDeck LED channel misconfig, 8 buttons
  affected — WRITTEN TO THE BOARD (2026-08-18).** Root-caused via
  `qlcplus-midi-profiler`'s `opendeck dump`/direct SysEx reads (no
  interactive `identify` needed — the LED's own `activation_id` was already
  correct, only its `channel` field was wrong): `Set`, `6-Up`, `6-Load`,
  `Left`, plus 4 currently-unmapped notes all had their paired LED listening
  on MIDI channel 1 (raw) instead of channel 9, so QLC+'s feedback (always
  sent on ch9) never reached them — explains "Set toggles but never lights."
  Backed up first (`qlcplus-midi-profiler/backups/pre-led-channel-fix.json`,
  restorable via `qlc-midi opendeck restore`), then wrote the correct
  channel (raw 9) to all 8 LED slots directly via `OpenDeck.write_checked`,
  verified via read-back. **Confirm live**: Set/6-Up/6-Load should now
  light like `O`/Master already do.

- **P1 slice 7 — context-aware faders (Level 1-10) — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.cpp`. Per
  Branson: faders should mean whatever's relevant to the palette focused in
  the Look Editor, not a fixed submaster/per-fixture role — "if we're
  selecting a [color] feature we'd do sliders for R G B W A U." Reuses the
  same `ProgrammingManager::currentPaletteId()` ground-truth built for the
  pan/tilt encoder work (slice 4). New `ProgrammerController::
  setDesignColorChannel(paletteId, channelIndex, value)` (0=R..5=UV, writes
  via `QLCPalette::colorToString` matching `LookEditor::commitColor()`'s own
  write pattern) and `setDesignDimmerValue(paletteId, value)` — both
  absolute writes (faders aren't relative like the encoders), same
  explicit-paletteId/refresh-every-referencing-scene/`designPositionWritten()`
  contract as `nudgeDesignPanTilt()`. `PMJOverlay`'s `Level` case now checks
  the focused palette's type: Color → faders 1-6 = R/G/B/White/Amber/UV,
  Dimmer → fader 1 = intensity, anything else (PanTilt/Aim/nothing focused)
  → faders stay inert (confirmed with Branson: leave dark/idle when nothing
  applies, don't fall back to a permanent submaster baseline — an open
  question from slice 1 is now resolved this way). Live redraw confirmed
  free: `LookEditor::setPalette()`'s Color case uses real Qt widgets
  (`QColorDialog::setCurrentColor`, `QSlider::setValue`) which repaint
  themselves on state change — unlike the custom `VCXYPadArea` from slice 5,
  no extra `update()` call needed. Builds clean, smoke-tested. **Not yet
  verified against the real board.**

- **P1 slice 6 — encoder direction, confirmed working on the rig —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`. After slice 5 the pin
  finally moved, but both encoders' raw MIDI delta turned out to be
  physically inverted relative to their on-screen effect — confirmed live:
  turning Enc 1 (pan) clockwise moved the dot left, not right. First pass
  negated the wrong encoder (tilt) on a guess; Branson clarified "it's enc1
  thats backwards," so negated pan instead — fixed pan, but left tilt's
  sign inconsistent (undiscussed/untested but same hardware, so likely the
  same physical inversion). Branson called this out directly: negate both,
  so "clockwise increases" is the one consistent rule for both axes rather
  than an asymmetric fix. **Confirmed working on the real PMJ — pan/tilt
  encoder nudge is done.**

- **P1 slice 5 — the actual root cause: `VCXYPadArea::setPosition()` never
  repaints itself — SHIPPED (2026-08-18).** `ui/src/lookeditor.cpp`,
  `ui/src/pmjoverlay.cpp`. Slice 4 was real but not sufficient — Branson
  turned the encoders again after that build, still nothing ("STILL NOT
  MOVING THE PIN"). Added logging inside the full chain (PMJOverlay's
  `slotRoleActivated` Param case) and it proved every single precondition
  was green on every turn: `ProgrammingManager` found, a valid palette id,
  a real palette object, `type() == PanTilt` exactly, `Doc::programmer()`
  non-null — `nudgeDesignPanTilt()` was being called correctly every time.
  So the bug was never upstream at all; traced `nudgeDesignPanTilt()`'s own
  body (`engine/src/programmercontroller.cpp`) and the write side
  (`QLCPalette::setValue()`/`intValue1()`/`intValue2()`) — both correct.
  Root cause was purely on the redraw side: `VCXYPadArea::setPosition()`
  (`ui/src/virtualconsole/vcxypadarea.cpp`) only updates internal state and
  emits `positionChanged()` — it never calls `update()`/`repaint()` itself.
  Every other call site pairs it with an explicit `update()` right after
  (`mousePressEvent`/`mouseMoveEvent`, and the joystick-drag redraw path
  already in `LookEditor` at line ~429) — except `LookEditor::setPalette()`'s
  `PanTilt` case (line ~714), which called `setPosition()` alone. So the
  palette value and the widget's internal `m_dmxPos` really were updating
  correctly on every encoder click; the dot just never got painted. Added
  the missing `m_xyPad->update()` after `setPosition()` there. Also removed
  the now-resolved chain debug logging from `pmjoverlay.cpp`. Builds clean,
  smoke-tested (no crash). **Not yet re-verified against the real board.**

- **P1 slice 4 — nudgeDesignPanTilt() no longer depends on stale
  focused-scene tracking — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`, `ui/src/app.cpp`. Slice 3's fixes
  (value-scaling, XY-pad redraw) turned out to be correct but incomplete —
  Branson dragged the XY pad to re-center, turned Enc 1/2 again, still no
  movement. Re-added debug logging (encoder-side decode now confirmed
  perfect: raw `255`/`2` → `-5°`/`+10°`, exactly as intended) — but the
  SECOND log line, inside `nudgeDesignPanTilt()` itself, never printed at
  all. It was returning at the very first check: `m_focusedSceneId` was
  invalid. Root cause: that tracking (`ProgrammerController::
  m_focusedSceneId`/`m_focusedPaletteId`) only gets set when a scene is
  freshly opened in the Programming tab's canvas — it doesn't persist
  across an app relaunch, and dragging the XY pad by hand never needed it
  in the first place (`LookEditor::slotPanTiltChanged()` only needs its own
  `m_paletteId`, tracked independently). So the encoder path had a real
  dependency the mouse path never had — not a fluke, a design gap.
  - **`nudgeDesignPanTilt()` re-signatured** to take an explicit
    `paletteId` instead of reading `m_focusedSceneId`/`m_focusedPaletteId`
    at all. Refreshes every scene that actually references the palette
    (`scene->palettes().contains(paletteId)`, walking `m_doc->functions()`)
    instead of one assumed "focused" scene — more correct anyway, since a
    palette can legitimately be shared by more than one scene.
  - **New `ProgrammingManager::currentPaletteId()`** — the ground truth for
    "what's on screen right now" (`m_lookEditor->paletteId()`, itself made
    reachable via a new `LookEditor::paletteId()` getter in slice 3).
    `PMJOverlay` now takes an `App*` at construction (`app->findChild<
    ProgrammingManager*>()`, the same reach-pattern already established
    elsewhere in this codebase) to get this instead of going through
    `ProgrammerController`'s tracking.
  - **Found and fixed a real, unrelated header bug along the way**:
    `programmingmanager.h` uses `QDoubleSpinBox*` but never forward-declares
    it (`QSpinBox` is declared, `QDoubleSpinBox` isn't) — worked by
    accident everywhere it was previously included transitively-first;
    broke the moment `pmjoverlay.cpp` included it directly. Fixed the
    header itself (added the missing forward declaration) rather than
    working around it locally — a real latent bug, not a workaround target.
  - Builds clean, smoke-tested (board disconnected, no crash). **Not yet
    re-verified against the real board** — fourth round on this exact
    feature; this one at least has hard evidence (the debug log) behind the
    diagnosis rather than another guess, but still needs a real turn of
    the encoder to confirm.

- **P1 slice 3 — two real bugs found via debug logging, both fixed —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `ui/src/lookeditor.{h,cpp}`, `ui/src/programmingmanager.cpp`. Branson's
  slice-2 test ("still no movement") got temporary file-based debug logging
  added to `nudgeDesignPanTilt()` and `PMJOverlay::slotInputValueChanged()`
  (matching this session's established fallback when guessing plateaus) —
  the resulting log conclusively showed two real, independent bugs rather
  than a workflow mistake:
  1. **Value-scaling bug.** The encoder's raw MIDI twos-complement delta
     (1 = +1, 127 = -1, confirmed weeks earlier via `qlc-midi monitor`) is
     NOT what `inputValueChanged()` actually delivers — QLC+'s MIDI plugin
     already scales it into its internal 0-255 space first (MIDI2DMX,
     ~x<<1, 127→255 special-cased). The log showed raw values `2` and `255`
     arriving, not `1`/`127` — my decode threshold (64/128, correct for raw
     7-bit MIDI) was silently turning a -1 click into a +127 click, which
     instantly clamped pan/tilt to its range boundary on the very first
     turn and then correctly did nothing on every subsequent one (already
     at the clamp) — indistinguishable from "not working" without the log.
     Fixed: threshold is 128/256, matching the space the value is actually
     in by the time it reaches this code.
  2. **The XY pad widget never repaints.** The SAME log also proved
     `nudgeDesignPanTilt()` WAS finding a focused scene (id 0 — a real,
     valid id; only `4294967295` means unset) and, once the palette was
     added to the scene, WAS finding and would have modified it — so the
     underlying engine data was correct the whole time. What was missing:
     nothing told `LookEditor`'s on-screen XY pad to redraw after an
     engine-side value change — only the user dragging it by hand ever
     triggered that before, since `applyDesignJoystick()` (the only prior
     caller of the `designPositionWritten()` signal this reuses) never
     touches a PanTilt-type palette's displayed values at all (it only
     drives an Aim-target, a different widget). Fixed with a new
     `LookEditor::paletteId()` getter and a redraw
     (`m_lookEditor->setPalette(m_lookEditor->paletteId())`) added to
     `ProgrammingManager::slotDesignPositionWritten()` — harmless/no-op
     for the Aim-look case, the only thing that repaints a PanTilt palette
     after a nudge.
  - All temporary debug logging removed after diagnosis, per this
    session's established pattern. Builds clean, smoke-tested (board
    disconnected, no crash). **Not yet re-verified against the real
    board/encoders** — this is the third round of "build, hand back for a
    real test" on this exact feature; worth a clean pass before assuming
    it's actually done.

- **P1 slice 2 — LEDs only light for what's real; Enc 1/2 nudge pan/tilt —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `engine/src/programmercontroller.{h,cpp}`. Follow-on from Branson's first
  real hardware test of slice 1: `Set` correctly toggled Blind (input
  pipeline genuinely works), but Blackout/Blind LEDs didn't light at all,
  and `1-10`/`1-10 Load`/`Go`/`Back` all lit for no reason — confusing,
  and backwards from "highlight what's useful, leave the rest dark."
  - **LED semantics fixed**: `stateFor()` now returns `Empty` (dark) for
    every role that doesn't drive real behavior yet (Page/Select/Load/
    Param/Transport) instead of defaulting them to `Valid` (dim). Only
    Blackout/Blind (dim idle, bright when engaged) light at all now — an
    honest reflection of what this slice actually does.
  - **Real finding, not covered by the LED fix**: Branson tried creating a
    pan/tilt look and turning the encoders — nothing moved. Turned out
    `ProgrammerController::applyDesignJoystick()` (the existing, working
    HID-joystick pan/tilt path) explicitly no-ops for a "Standard Pan/Tilt"
    look — raw palette-value authoring for that case doesn't exist ANYWHERE
    in the app yet, not just missing for the PMJ; only an Aim-palette look
    (which drags a floor-space target) has ever had a working live-nudge
    path. Confirmed with Branson before building rather than guessing at
    Look-authoring semantics unilaterally.
  - **New engine method**: `ProgrammerController::nudgeDesignPanTilt(float
    dPanDeg, float dTiltDeg)` — finds the focused scene's controlling
    PanTilt palette (same precedence `applyDesignJoystick()` uses: the
    explicitly focused palette, else the last PanTilt-type one on the
    scene), adjusts its stored `intValue1()`/`intValue2()` (raw degrees,
    same 540°/270° space `LookEditor`'s XY pad already authors it in — see
    `lookeditor.cpp`'s `PAN_DEG`/`TILT_DEG`), and refreshes the running
    scene via `markSceneEdited()` — the exact same live-refresh path a Look
    Editor slider drag already uses (`Scene::requestPaletteRefresh()`, not
    a full `resetRuntime()` teardown, so repeated nudges don't restart the
    scene's fade-in or flash other channels). No-op for an Aim-look scene
    (nothing to nudge — the joystick's existing path already owns that
    case) or when nothing's focused.
  - **Wired**: `Enc 1` → pan, `Enc 2` → tilt, 2°/click, unconditional (no
    paging yet — Enc 3/4 stay unbound, pending the same page-targeting
    model as the rest of this slice). Had to hand-decode the encoder's raw
    twos-complement value myself (`value<64 → +value, else value-128`) —
    confirmed QLC+ doesn't do this upstream of `inputValueChanged` even for
    an `Encoder`-typed channel (only `QLCInputSource::decodeRelativeDelta()`,
    a *bound-widget* config object, does — not applicable here), matching
    the same raw values `qlc-midi monitor` showed during the original
    encoder classification work.
  - Builds clean, smoke-tested (board disconnected, no crash/regression).
    **Not yet re-verified against the real board** — worth checking: LEDs
    stay dark except Blackout/Blind now, and turning Enc 1/2 with a
    PanTilt-palette look focused actually moves pan/tilt live.

- **P1 slice 1 — PMJ Black 1 overlay onto the control-surface engine —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}` (new), `ui/src/app.{h,cpp}`,
  `ui/src/CMakeLists.txt`. First real device overlay on top of the P0 engine
  from an earlier session (`ControlSurfaceEngine`/`ControlSurface` — device-
  agnostic role/state vocabulary + a generic LED-repaint loop, see
  `CONTROL_SURFACE_DESIGN.md`). Deliberately scoped down from the full
  design-doc Phase 1 vision to what has clean, unambiguous integration
  points today — flagging the scope honestly rather than half-guessing the
  rest:
  - **Full role table registered** — every one of the PMJ's 69 real MIDI
    channels (hardcoded from `resources/inputprofiles/PMJ-Black-1.qxi`, not
    re-parsed at runtime, so a hand-edited profile can't silently desync the
    binding) gets a `ControlSurface::Control` + `Role`, matching the design
    doc's table: `Master`→GM, `Ch 1-10`→per-strip `Level`, `1-10`→`Select`,
    `N-Load`→`Load`, `Enc 1-4`→`Param`, `Groups/Looks/Effects/Macros/Fix
    Cont`→`Page`, `Go/Back/Left/Right/Pre Page/Next Page`→`Transport`,
    `O`/`Set`→`Blackout`/`Blind` (the design doc's proposal, confirmed).
    `N-Up`/`N-Down` and `Favorites` (proposed Tap) are registered with no
    Role — still-open per the design doc's discussion point and the "the
    only Tap in the codebase is Programming-tab-local, not global" finding.
  - **Only 3 are wired to real behaviour so far**: Master fader →
    `InputOutputMap::setGrandMasterValue()`, `O` → `toggleBlackout()`, `Set`
    → `setOutputInhibited()` — all engine-level Doc APIs, no App-level
    plumbing needed. Select/Load/Param/Transport/Page-switching are received
    by the engine and logged, not yet driving real selection/navigation —
    that needs the UI-side "what's currently selected/active" concept this
    session established doesn't cleanly exist yet for fixture groups in the
    Programming tab. Next slice's real dependency, not a small gap.
  - **LED feedback**: `ledSink` sends real Note-On via the existing generic
    `InputOutputMap::sendFeedBack()` (channel 9, matching the profile) —
    reused, not reinvented. Brightness snaps to the OpenDeck "steady levels"
    set (15/31/47/…/127) found via `qlcplus-midi-profiler`'s README, so a
    state change can never accidentally set a control blinking. `stateFor()`
    reflects real state for Blackout/Blind (dim when idle, bright when
    engaged) and the active page (selected vs. valid); Select/Load default
    to a flat "present" dim state pending the same selection-model gap above.
  - **Registered on every startup**, PMJ connected or not — `App::initDoc()`
    constructs the engine + overlay unconditionally, right after
    `startUniverses()`. No crash/error either way; smoke-tested via a full
    launch + screenshot with the board disconnected.
  - Building this slice surfaced one real bug in my own code, not the
    engine: `using CS = ControlSurface;` is invalid C++ (type-alias syntax
    can't name a namespace) — direct compile confirmed the fix
    (`namespace CS = ControlSurface;`).
  - **Partially live-verified, one real bug found and fixed.** Branson
    reconnected the board: `Set` correctly toggled Blind (confirms the input
    pipeline works end-to-end for real), but no LEDs lit at all, when at
    minimum every registered/`Valid` control should have shown a dim glow.
    Root cause: `sendLed()` hardcoded `sendFeedBack(0, ...)` — assumed the
    PMJ's output would be on universe 0, with no actual basis for that
    guess. If that's wrong, feedback silently goes nowhere. Fixed two ways:
    (1) `PMJOverlay` now scans every universe's `InputPatch::profileName()`
    for one matching "PMJ Black 1" at construction time
    (`findKnownUniverse()`), so LEDs light on connect/launch without
    requiring a press first; (2) `slotInputValueChanged()` also re-learns
    the universe from real traffic on every event (not just once), so a
    board patched *after* startup, or re-patched to a different universe
    mid-session, self-corrects instead of staying dark. Builds clean,
    smoke-tested with the board disconnected (no crash/regression) — the
    universe-discovery fix itself still needs a real check with the board
    connected, since that's exactly the scenario it fixes.

- **Three more backstage color themes — SHIPPED (2026-08-18).**
  `ui/src/app.h`, `ui/src/app.cpp`. Turned out this fork already had a whole
  theme system (`App::Theme` enum, `applyTheme()` building a `QPalette`,
  View → Theme menu, persisted to `QSettings`) from an earlier session —
  Default/Tan/Blue. Branson asked for a few more, name-checking a "QLC+
  original" look and a red-shifted night-vision theme, and to look at VS
  Code's dark themes for ideas. Scoped to chrome only (menus/toolbars/tabs/
  dialogs) per Branson's choice — the 2D canvas (trusses/fixtures/grid) is
  hand-painted with ~200 hardcoded `QColor` literals across 18 files and
  doesn't follow the palette; making it theme-aware too is a much bigger
  follow-on project if ever wanted. Added: **QLC+ Original** — corrected
  after Branson called out that my first pass (a dark charcoal grey) was
  wrong: he pulled a fresh copy of upstream QLC+ (`github.com/mcallegari/
  qlcplus`, now checked out at `/Users/branson/git/qlcplus`) to check
  against, which confirmed upstream ships NO custom palette or stylesheet
  at all — it's plain native Qt Fusion light grey. Redone to match that
  (`#efefef` window, white base, black text, `#4a90d9` highlight) — unlike
  "Default" (which just follows whatever the OS's current light/dark
  setting is), this stays that same classic light look on demand regardless
  of OS mode; **Red Shift** (blue channel kept
  near-zero throughout, the same principle as a red stage torch or
  astronomy light, for working backstage in the dark without wrecking night
  vision); **VS Code Dark** (VS Code's "Dark+" editor greys plus its iconic
  `#007acc` accent blue). Same mechanism as the existing two themes — just
  new `QPalette` color blocks in `applyTheme()`'s switch and three new
  entries in the View → Theme menu's data-driven choice list. **Verified
  live**, carefully: with two `qlcconsole` processes running (Branson's real
  session plus my own isolated scratch-file test), AppleScript's `process
  whose unix id is N` turned out to silently match the WRONG process by
  name regardless of the id filter — an accidental click opened Branson's
  real View → Theme menu once (no theme was actually changed; the
  submenu-item click failed before selecting anything, and Escape closed
  it). Switched to a safer verification method with zero click risk:
  pre-set `workspace.theme` via `defaults write org.qlcplus.qlcconsole`
  before launching the scratch instance fresh (it reads the setting once at
  startup), screenshotted Red Shift applied correctly (warm orange-red
  layer-row highlight and tab underlines), then restored the setting to
  Branson's original value (`Blue`) afterward.

- **Stage centre lines now have their own show/hide toggle — SHIPPED
  (2026-08-18).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `ui/src/monitor/monitor.cpp`. The teal crosshair marking the stage centre
  was previously bundled into the same `m_gridItems` list as the grid lines
  themselves, so there was no way to hide it independent of the grid. Gave
  it dedicated `m_centerLineV`/`m_centerLineH` members (rebuilt alongside
  the grid in `updateGrid()`, since their position depends on the same
  `m_cellPixels`/offsets, but no longer added to `m_gridItems`) and a new
  `setCenterLinesVisible()`/`centerLinesVisible()` pair. New "Center" footer
  toggle button next to Rulers/Labels, persisted via `QSettings`
  (`monitor/centerlines`, default on) the same way Rulers/Grid already are.
  Built clean, not yet verified live.

- **Truss tether line: no-line threshold now matches the truss's visual
  width — SHIPPED (2026-08-18).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `updateTrussAnchorLines()`. Branson reported still seeing the line even
  when a fixture looked like it was sitting right on top of the truss. The
  no-line threshold was a fixed 4px around the exact mathematical
  centreline — too tight, since the truss itself is drawn with real width,
  so anywhere within that drawn thickness reads as "on the truss" to the
  eye without being at cross=0 exactly. Threshold is now `max(4px, half the
  truss's own drawn width)`, so the line stays hidden across the whole
  visual footprint of the truss bar, not just its mathematical centre.
  Built clean, not yet verified live.

- **Truss tether line now terminates at the truss's near EDGE, not its
  centreline — SHIPPED (2026-08-18).**
  `ui/src/monitor/monitorgraphicsview.cpp`, `updateTrussAnchorLines()`.
  Follow-on to the entry above: the anchor end of the line was always
  `Truss::positionAt(trussOffset)` — dead centre of the truss's width —
  so the line visually ran INTO the truss body before disappearing under
  it, rather than stopping where the fixture actually meets the truss.
  Reworked the skip-check and the anchor point separately: first decide
  whether to draw at all by comparing `|trussCross|` against the truss's
  half-width (plus ~2px of slack) in world units — this is the same
  "is the fixture still within the truss's visual footprint" test as the
  entry above, just computed before picking an edge, so it isn't skewed by
  which side the line would terminate on. Then, only once drawing, offset
  the anchor point from the centreline by exactly the truss's half-width,
  signed toward whichever side the fixture's `trussCross` is on — so the
  line always starts right at the truss's surface on the fixture's side,
  never inside it. **Verified live** on the isolated scratch copy: cropped
  in on the fixture/truss boundary and confirmed the dashed line now
  stops right at the truss edge instead of running through it.

- **Real bug, finally cornered: Lighting Studio can come up completely empty
  on a fresh launch — trusses, layers, and every fixture missing — even
  though the save was perfectly intact — FIXED (2026-08-18).**
  `ui/src/app.cpp`, `App::loadXML(const QString&)`. This is the bug behind
  Branson's "add a fixture, save, close, reopen — fixture not there at all"
  report, and it very nearly got written off as our earlier file-collision
  false alarm — glad we kept pushing. Root cause, found by reproducing on an
  isolated scratch copy (never touching Branson's real file) with temporary
  file-based tracing at three levels: (1) `MonitorProperties::loadXML()`
  parses every single element correctly — Layer 1, both trusses, all three
  `FixtureRig` entries, confirmed via a log of every XML element it visits;
  (2) yet `Monitor::fillGraphicsView()` — the function that actually
  populates the 2D canvas from that loaded data — ran with `docFixtureCount
  = 0`, i.e. `Doc` had ZERO patched fixtures at the moment it ran; (3) but
  the Fixture Manager (Hardware tab), opened moments later in the SAME
  running instance, correctly listed all three fixtures. So the doc data was
  never actually lost — `Monitor`'s graphics view had just been built once,
  too early, from an empty doc, and never told to rebuild. The mechanism:
  `main.cpp` calls `app.startup()` (constructs all tabs, including
  whichever one is the workspace's saved `CurrentWindow` — shown
  immediately, against whatever `Doc` holds at that instant) BEFORE calling
  `app.loadXML(QLCArgs::workspace)` for a `-o`/`--open` command-line file.
  Fixture Manager's tab tree happens to rebuild itself constantly from many
  UI interactions, so it self-heals; `Monitor::fillGraphicsView()` has
  almost no other trigger and stayed stale. It only ever surfaced visibly
  once a save carried `CurrentWindow="Monitor"` — i.e. once Lighting Studio
  became the tab a workspace reopens directly into, which is exactly the
  workflow this whole session has been exercising. The "open recent file"
  path already knew to call `Monitor::instance()->updateView()` +
  `FixtureManager::instance()->updateView()` after loading (see the existing
  code a few lines above in the same file) — the command-line/`-o` load
  path just never got the same treatment. Fixed by adding those same two
  calls at the end of `App::loadXML(const QString&)`, the shared function
  underneath both paths, so every caller benefits and the "recent file" path
  just does one harmless extra refresh. **Verified live**: reproduced the
  exact empty-canvas symptom on an isolated scratch copy of Branson's file,
  confirmed the fix resolves it (Layer 1, both trusses, all three fixtures,
  cyan rings, and the tether line all render correctly on a fresh launch),
  screenshot-confirmed both before and after. All temporary debug logging
  removed.

- **A truss-bound fixture that isn't sitting right on the truss now gets a
  thin tether line back to its anchor point — SHIPPED (2026-08-17).**
  `ui/src/monitor/monitorgraphicsview.{h,cpp}`, new `updateTrussAnchorLines()`
  + `m_trussAnchorLines`. Direct follow-on to the cross-offset drag fix:
  Branson asked whether an off-truss fixture should get a visual connector
  back to the truss so the binding reads clearly instead of relying on
  proximity, and whether anything else was worth doing. Implemented a
  dashed line from the fixture's current center to its projected point on
  the truss centerline (`Truss::positionAt(trussOffset)`, ignoring cross —
  i.e. the point it would sit at with no offset), skipped when the fixture
  is within 4px of that point so a normally-centered rig stays clutter-free.
  Color was a follow-up design question Branson raised: rather than a fixed
  color, the tether now matches the truss's OWN palette — its neutral
  unselected chord grey (160,163,172) at rest, switching to the truss's own
  selected amber (255,180,0) when the truss (and therefore its extended
  group of bound fixtures, per the anchor-selection fix above) is the
  current selection. This deliberately leaves the fixture's own existing
  selection color (yellow) and bound-to-truss ring (cyan) untouched — those
  are separate, already-established indicators; only the tether reflects the
  truss's selection state. Rebuilt on every truss/fixture move
  (`slotFixtureMoved()`, `slotTrussMoved()`, including its elevation-drag
  branch), on any general item-state refresh (`refreshItemLayerState()`,
  which covers load, POV switches, and lock toggles), and on selection
  change (`extendSelectionToGroups()`) so the color follows selection
  immediately without requiring a move. Vertical trusses (towers) are
  skipped — same reasoning as the cross-offset fix, no "across" concept
  there. **Verification note:** while testing this live, launching
  `surfacetesting.qxw.before-padfix` myself collided with Branson's own live
  session against the same file — each save raced the other's, which briefly
  looked like a real persistence bug (a fixture attach not surviving a
  restart) before we tracked it down: Branson closed his instance, reopened
  cleanly with me not touching the file, and confirmed the attachment DID
  survive a real restart — so that was purely our concurrent access, not a
  product bug. Lesson: don't launch the app against a workspace file the
  user might have open live. Branson then confirmed live: the tether line
  renders correctly and turns amber when the truss is selected — but the
  FIXTURE's own outline stayed yellow instead of following along, which he
  expected to change too. Fixed: `MonitorFixtureItem` gained
  `setTrussGroupSelected()`/`isTrussGroupSelected()` (new bool, distinct
  from the existing `m_isolated`/`isSelected()` selection-color logic in
  `paint()`) — when set, a selected fixture's outline draws in the truss's
  amber instead of the generic yellow. `updateTrussAnchorLines()` now sets
  this flag for every truss-bound fixture on every call (not just the ones
  that get a drawn tether line — a fixture sitting exactly on the centerline
  still needs its outline to follow the truss's selection even though it
  has no line to color). Deliberately narrow: only a fixture whose OWN truss
  is currently selected gets amber — solo-selecting just that fixture (the
  anchor-selection click fix from earlier) still shows plain yellow, since
  that's "just this fixture," not "the assembly." Built clean; not yet
  re-confirmed live (Branson needs to relaunch to pick up the new binary).
  **Follow-up (2026-08-17, same day):** Branson caught a real bug from a
  screenshot — the tether into a vertically-running truss was visibly a
  couple degrees off perpendicular (fine for a horizontal-running truss,
  off for a vertical one). Cause: the line's fixture-end read the icon's
  actual on-screen center while the truss-end was independently recomputed
  from stored `trussOffset` — any drift between a fixture's rendered
  position and its stored offset/cross (rounding, or hand-set data that was
  never perfectly self-consistent) tilted the line. Fixed by deriving BOTH
  endpoints from the same stored `trussOffset`/`trussCross` via the truss's
  own direction vector, so the line is perpendicular by construction
  regardless of the truss's on-screen orientation — no longer reads the
  item's rendered position at all. Also thickened per request (1.2–1.6px →
  2.4–3.0px) with a round cap. Built clean, not yet re-verified live.

- **Clicking a fixture right after clicking its truss now solo-selects the
  fixture instead of keeping the truss along for the drag — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `mousePressEvent()`. Last piece of the anchor-selection fix above: with
  that fix alone, Branson found a single-click on a lone fixture correctly
  moved just the fixture, and selecting the truss correctly moved
  truss+fixtures together — but click the truss FIRST, then click one of
  its fixtures, and both still moved together. Root cause is a Qt default,
  not a leftover bug in our logic: `QGraphicsScene`'s built-in press
  handling only clears the rest of the selection when the clicked item
  *isn't already selected* — if it is, it assumes you're grabbing the whole
  group to drag it. Since `extendSelectionToGroups()` had already pulled the
  fixture into the selection when the truss was clicked, the fixture counted
  as "already selected" by the time it was clicked next, so Qt left the
  truss selected too. Branson was offered two ways to resolve the ambiguity
  (highlight the fixtures to make the shared-selection state visible, or
  make a fixture click always break out solo) and picked solo-select, to
  keep a fixture click meaning "just this fixture" everywhere, consistent
  with the fix above. Implemented by intercepting a plain (no Shift/Ctrl)
  left-click on a `MonitorFixtureItem`: if it's selected AND its bound
  truss's `TrussItem` is also currently selected, force
  `m_scene->clearSelection()` + reselect solely the fixture *before* handing
  off to `QGraphicsView::mousePressEvent()` — so Qt's default handling then
  sees a solo, already-correct selection and just starts the single-fixture
  drag. Shift/Ctrl-click (explicit multi-select) and clicking the truss
  itself are untouched. **Not verified live** — same canvas-click/drag
  limitation as the other entries here; worth confirming at the rig:
  select truss (everything highlights) → click one of its fixtures → only
  that fixture should stay selected and move solo.

- **A truss-bound fixture can be dropped anywhere across the truss (not
  forced onto the centreline) while still touching it — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`, `snapToTruss()`
  (inside `slotFixtureMoved()`) + `slotTrussMoved()`. Follow-on to the
  anchor-selection fix right below: once Branson could drag a bound fixture
  without it dragging the truss, the next ask was that dragging it
  perpendicular to a horizontal truss ("vertically away" on screen) always
  snapped it back onto the centreline, when it should be free to land
  anywhere still touching — with the existing red-border "pull too far and
  it detaches" behavior (already working, driven by `MonitorFixtureItem`'s
  `escapeMode()`/`setEscapeMode()`, set live during the drag in
  `mouseMoveEvent()`) as the actual boundary. `snapToTruss()` used to force
  the perpendicular component to zero by projecting the drop position onto
  the truss's direction vector and discarding everything else; it now splits
  the drop into an ALONG component (still projected/grid-snapped, as before)
  and a CROSS component (perpendicular offset, preserved), clamping cross to
  the same `pxWid() * 2` threshold the escape-mode red border already uses —
  so the allowed range matches exactly where it would otherwise go red, no
  surprise snap-back at the boundary. The cross value is stored in
  `FixtureRigProps::trussCross`, a field that already existed for the
  Fixture Properties dialog's discrete Left/Centered/Right "Across truss"
  selector (`ui/src/monitor/monitor.cpp`) and was already read by
  `MonitorProperties::fixtureRigPosition()` — the authoritative derived
  position `aimsolver.cpp`/`effectinstance.cpp` use for actual pan/tilt
  aiming and effects — so this reuses existing, already-correct engine math
  rather than inventing a parallel one; the top-view canvas (which renders a
  truss-bound fixture from its raw stored XY, not from `fixtureRigPosition()`
  — see `updateFixture()`) is kept in sync by writing the same along+cross
  point into both places. `slotTrussMoved()`'s "truss carries its fixtures"
  logic was also switched from the bare `Truss::positionAt(trussOffset)`
  (centreline only) to `MonitorProperties::fixtureRigPosition()`, so a
  fixture's cross offset (and its mount-side Z nudge) now survives the truss
  itself being moved — previously any fixture with a non-zero cross offset
  would have silently re-centred the next time its truss moved, since
  `positionAt()` never included that term. Vertical trusses (towers) are
  unchanged — the user's ask was specifically about horizontal trusses, and
  a tower's radial/yoke mounting doesn't have the same "across" concept.
  **Not verified live** — same canvas-drag limitation as the other entries
  here; worth a real drag test (drop off-centre but touching → stays there;
  drop across → snaps within bounds; drop too far → red + detach; move the
  truss afterward → fixture keeps its offset) at the rig.

- **Moving a truss-bound fixture no longer drags the truss along with it —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `extendSelectionToGroups()` + new `isGroupAnchorItem()`. Third bug in this
  same area, found immediately after the drag-to-attach fix landed: once a
  fixture is bound to a truss, clicking/dragging *the fixture* also moved
  the whole truss. Root cause was `extendSelectionToGroups()` (wired to
  `QGraphicsScene::selectionChanged`) — it treats every member of a group as
  a peer, so selecting any one member (the fixture) pulled in the whole
  group's `topLevelGroup()`, truss included, and Qt's native multi-select
  drag then moved everything together. But a truss's auto-maintained group
  (`ensureTrussGroup()`/`ensurePlatformGroup()`, `anchorKind == "truss"` /
  `"platform"`) isn't a peer relationship — it's parent→child: the truss
  already carries its bound fixtures correctly when *it* moves, via
  `slotTrussMoved()`'s dedicated position-following logic, which runs
  independent of selection state and needed no changes. Fix: only extend a
  *dedicated* structural group (anchor set) when the anchor item itself —
  the truss/platform, not a rigged member — is what got selected; a manually
  built-out group (anchor cleared once it holds >1 structural item, see
  `structuralMembersOf()`) keeps the old symmetric behavior. New helper
  `isGroupAnchorItem(QGraphicsItem*, anchorKind, anchorId)` takes the anchor
  kind/id rather than the `MonitorProperties::MonitorGroup` struct directly
  — `monitorproperties.h` is only forward-declared in the header, so a
  nested-type parameter there fails to compile (confirmed by trying it
  first). **Not verified live** — same canvas-drag limitation as the entry
  below; needs a real "drag the fixture, does the truss stay put" /
  "drag the truss, do its fixtures follow" check at the rig. Also
  unaddressed: Branson's "not so far they don't touch" phrasing implies a
  bound fixture dragged far enough away should auto-detach — there's an
  existing "escape mode" detach branch in `slotFixtureMoved()` built for
  exactly this, but nothing in the codebase ever calls `setEscapeMode(true)`
  to trigger it, so it's dead code today. Left alone pending confirmation
  this is actually wanted, since it wasn't explicitly asked for as a
  separate feature.

- **Drag-to-attach a fixture onto a truss brought back, scoped correctly
  this time — SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `slotFixtureMoved()`. Direct continuation of the truss-group bug above:
  once that fix landed, Branson immediately hit the OTHER side of the same
  area — dragging an unbound fixture onto a truss did nothing at all. That
  turned out to be pre-existing, deliberate: a prior fix had removed
  auto-attach-on-drop *entirely* to kill a bug where a truss would "grab"
  any fixture whose bounding box merely overlapped it. That's also exactly
  what Branson had asked for earlier in this session (attach should trigger
  on *touching*, not require being centered on the truss) — so the right
  fix was to bring it back with tighter scope, not leave it removed.
  Re-added using `QGraphicsItem::collidesWithItem()` (precise shape overlap,
  not just a bounding-box guess) between the fixture and every `TrussItem`,
  but critically **only inside `slotFixtureMoved()`, which only runs once
  per completed drag (on drop)** — never per mouse-move — so passing a
  fixture over a truss en route somewhere else can't trigger it the way the
  original bug did. Mirrors the already-working, already-accepted "auto
  deck-mount onto a platform" pattern right below it in the same function,
  just for trusses. The explicit right-click "Attach to Truss…" and
  Layers-tree-drag paths are untouched and still work as a fallback for
  precise placement. **Not verified live** — this environment has no way to
  synthesize a real click-drag-release sequence on a `QGraphicsView` canvas
  (no `cliclick` or equivalent installed), so this shipped on code review
  plus direct parity with the platform auto-mount logic it's modeled on,
  not a screenshot. Worth a real drag-and-drop check at the rig before
  trusting it.

- **Build Focus removed; "clicking a fixture selects the truss too" —
  correctly root-caused this time (a real data bug, not Build Focus) and
  fixed — SHIPPED (2026-08-17).** Follow-on to the entry below, which
  turned out to have the WRONG diagnosis for a related-looking but distinct
  symptom Branson hit next.
  - **Build Focus removed entirely**
    (`ui/src/monitor/{monitor,monitorgraphicsview,monitorfixtureitem}.{h,cpp}`),
    per Branson's challenge: it duplicated what a locked Layer already does
    (both clear `ItemIsSelectable`/`ItemIsMovable`, which is what actually
    causes the click-through). `MonitorGraphicsView::setBuildFocus()`/
    `buildFocus()`/`m_buildFocus`, `Monitor::m_buildAction`, the footer
    "Focus:" combo, and the `updateModeIndicator()` BUILD branch are all
    gone. The one part of Build Focus worth keeping — Branson explicitly
    asked for it — was the faint "ghosted" visual as a general indicator of
    "what's currently clickable." `MonitorFixtureItem::setGhosted()` is kept
    but now driven by the fixture's REAL state
    (`refreshItemLayerState()`: `setGhosted(lyr.locked)`) instead of a
    separate mode, so a locked fixture is visibly faint everywhere, all the
    time, not just in one special mode.
  - **The actual "truss steals the selection" bug**, found only after two
    wrong turns (z-order/click-target-size, then Build Focus) — this
    session's Show Manager investigation habit of demanding hard evidence
    over plausible-sounding theories paid off: Branson's screenshots proved
    single-click selection genuinely pulled in the truss, and a live
    reproduction (`surfacetesting.qxw.before-padfix`, temporary debug
    logging in `MonitorProperties::setFixtureGroup()` and
    `extendSelectionToGroups()`) traced it to real saved data — 3 fresh,
    never-truss-bound fixtures (`FixtureRig Truss="4294967295"`, i.e.
    `Truss::invalidId()`) carrying the SAME `GroupId` as an existing truss,
    ~0.58 m away with no position link at all. Root cause:
    `detachFixtureFromTruss()` and the drag-escape auto-detach path in
    `slotFixtureMoved()` (`monitorgraphicsview.cpp`) both clear a fixture's
    `trussId` on unbind but — per an explicit, deliberate old comment,
    *"detaching is about the rig binding, not the spatial grouping"* —
    always left it in the truss's auto-created group. Once a fixture had
    ever been bound-then-unbound from ANY truss, it would select/move
    together with that truss forever after, with zero visual or positional
    relationship. Fixed with a new symmetric helper,
    `MonitorGraphicsView::leaveDedicatedTrussGroup(fid, trussId)`: on
    detach, if the fixture's current group is still the truss's own
    dedicated auto-group (`MonitorGroup.anchorKind == "truss"` and
    `anchorId == trussId` — i.e. nothing the user built out further
    manually), take it back out. Called from both detach paths. Also
    hand-fixed the ALREADY-corrupted stale data in
    `surfacetesting.qxw.before-padfix` (stripped the stray `GroupId="1"`
    from the 3 affected `FxItem` entries) — the code fix only prevents this
    going forward, it doesn't retroactively repair a workspace already
    carrying the orphaned membership.
  - Still open, deferred at Branson's request: a general "Add {thing} to
    {what's under the cursor}, or just 'Add {thing} here' with no attach if
    right-clicking empty space / a locked item" convention across every
    attachable type (truss/platform/pipe/stand/tower) — worth doing, but a
    separate pass from this bug fix.
  - Verified: full engine+UI build clean. NOT re-verified live in-app after
    the final fix (this session's synthetic-click/menu-popup limitations
    made reliably reproducing the exact add-fixture-and-click sequence
    impractical) — confirmed instead via the saved-XML evidence trail above
    plus direct code review of both detach call sites. Worth a real
    at-the-rig check: detach a truss-bound fixture, then single-click it —
    the truss should no longer come along.

- **"Clicking a fixture on a truss selects the truss" — root-caused and
  fixed the real problem (a discoverability gap, not a hit-test bug) —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitor.{h,cpp}`. Branson's report
  led down a wrong first path (I initially suspected z-order/click-target
  size in `trussitem.cpp`/`monitorfixtureitem.cpp` — both checked out fine:
  fixtures are explicitly `zValue(2)` above trusses' `zValue(-0.5)`, and
  `MonitorFixtureItem::shape()` tightly hugs the fixture body). Branson's
  follow-up ("I can click anywhere in the blue box, every time") ruled that
  out and pointed at the real mechanism: **Build focus**
  (`MonitorGraphicsView::setBuildFocus()`), a pre-existing checkable mode
  that intentionally ghosts fixtures (`setGhosted(true)`, `ItemIsSelectable`
  cleared, `setMovable(false)`) so structural items become the click target
  for laying out trusses/platforms — a Qt item with neither flag set
  ignores its own mouse-press, which falls through to whatever's
  underneath. Working exactly as designed, but **undiscoverable**: the
  toggle lived only inside the "More" popup menu, and its own intended
  on-screen indicator — `m_modeLabel`, a "prominent current-mode chip" —
  was declared in the header and fully wired up in
  `updateModeIndicator()`, but **never actually constructed or added to any
  layout**, so it silently did nothing. Fixed by making the toggle itself
  visible: initially tried a dedicated toolbar button next to "Edit Plot"
  plus a full-width colored banner above the canvas (`m_modeLabel`,
  finally instantiated) — Branson asked for a lighter touch instead, so
  landed as a plain checkable toolbar button in the **footer** bar, right
  before Overlay/View (`initGraphicsFooter()`), matching the style of the
  other view-mode selectors already there (Grid/Snap/Rulers/Labels). The
  banner and the dedicated top-toolbar button were both removed again per
  that feedback — `m_modeLabel` goes back to being unconstructed (a
  no-op, matching its original pre-existing state) rather than half-used.
  Verified live at each step via rebuild + relaunch + screenshot, including
  actually toggling Build focus on to confirm fixtures visibly ghost and
  the footer button state reflects it.

- **Lighting Studio's tab icon changed from `:/monitor.png` (a generic
  system-monitor/pulse icon, a poor fit) to `:/grid.png` — SHIPPED
  (2026-08-17).** `ui/src/app.cpp` (both the tab icon and
  `m_controlMonitorAction`'s icon, kept in sync). Chose from the PNG set
  already compiled into this target rather than the fork's parallel SVG set
  (`resources/icons/svg/`, 157 icons including a much more literal
  `2dview.svg`) — that SVG set turned out to only be wired into
  `qmlui`'s CMakeLists (off for this fork per CLAUDE.md), not
  `ui/src`'s; `Qt::Svg` isn't even linked into the `qlcplusui` target, so
  referencing an `.svg` resource there would've silently rendered a blank
  icon. Using it for real would mean adding the `Svg` component to
  `ui/src/CMakeLists.txt`'s `target_link_libraries` and a CMake
  *reconfigure* (not just a rebuild) — real but avoidable extra risk for an
  icon swap, so stuck to the zero-risk PNG option. `grid.png` was picked
  after visually comparing several PNG candidates (`target.png`,
  `position.png`, `tabview.png`, `global.png`, `diptool.png`, `square.png`)
  — it's the one that actually reads as "2D plotted layout," matching what
  the tab shows (a grid-ruled canvas with Grid/Subdiv/Snap controls). Minor
  known tradeoff: also used for Shows' Snap-to-Grid action, but that's a
  toolbar button in a different tab, not the same visual context, so low
  real confusion risk. If a closer-fitting purpose-built icon matters more
  than the CMake risk, the SVG route is the way to get one — flagging it
  rather than deciding unilaterally.

- **Main tab strip reordered to follow the build workflow — SHIPPED
  (2026-08-17).** `App::init()`'s `addTab()` sequence
  (`ui/src/app.cpp`): was Hardware/Functions/Programming/Shows/Virtual
  Console/Simple Desk/Inputs/Outputs/Lighting Studio (construction-history
  order); now **Hardware → Inputs/Outputs → Lighting Studio → Functions →
  Programming → Shows → Virtual Console → Simple Desk** — rig/setup, then
  build content, then run it, per Branson's requested grouping. Purely a
  reordering of the existing `addTab()` calls (all `setActiveWindow()`/
  `indexOf()`-based lookups elsewhere are by class name or widget pointer,
  never a hardcoded index, so nothing else needed to change); confirmed via
  a live relaunch that all 8 tabs still construct without error in the new
  order and the tab strip reads correctly left to right.

- **Lighting Studio is now a real tab; window-title/detach correctness
  fixes; Shows-tab follow-up fixes — SHIPPED (2026-08-17).**
  - **Lighting Studio (`Monitor`) converted from a lazily-created standalone
    `Qt::Window` into a permanent tab**, constructed once in `App::init()`
    alongside every other tab (`ui/src/app.cpp`, `ui/src/monitor/monitor.{h,cpp}`).
    Prompted by Branson noticing it was the one surface that didn't pick up
    the app's title-bar conventions, and pushing back on my initial "keep it
    floating" recommendation — correctly: since ANY tab can already be
    double-click-detached into its own window via the existing
    `App::slotDetachContext`/`DetachedContext` machinery, there was no real
    functional loss from making it a tab, only redundant special-casing.
    - `Monitor`'s constructor moved from `protected` to `public` (with
      `Q_ASSERT(s_instance == NULL); s_instance = this;` moved into the
      constructor body), matching the exact convention every other
      tab-hosted singleton already uses (`FunctionManager`, `ShowManager`,
      etc.) — no longer a special case.
    - `Monitor::createAndShow()` (6 call sites, all left unchanged) no
      longer constructs anything; it now walks up the parent chain to find
      either the `QTabWidget` (switch to the tab) or a `QMainWindow`
      (currently detached — raise that window instead). All 6 existing
      callers keep working with zero call-site changes.
    - Removed `WA_DeleteOnClose` and the old create-time geometry
      restore/first-run centering logic (moot — the tab is never destroyed
      until app shutdown, exactly like its siblings). Removed the
      now-dead `SETTINGS_GEOMETRY` write in `saveSettings()`.
    - Workspace XML: the old per-Monitor `MonitorWindow open="1"/geometry`
      `AppState` element (its own separate persistence, predating the
      generic `DetachedWindow` mechanism) is retired on the save side —
      Monitor detaching now falls through the same generic `DetachedWindow`
      path as any other tab, since its className is just `"Monitor"`. Old
      saved files with a legacy `MonitorWindow` element are read and
      silently ignored (no crash, no bogus popup) rather than crashing or
      double-opening.
    - Verified live: screenshot confirms no floating window appears at
      startup anymore, "Lighting Studio" renders correctly as a selected
      tab (toolbar/layers panel/grid controls all intact), and the title
      bar reads `qlcconsole - <file> - Lighting Studio` — consistent with
      every other tab, which was the actual ask.
  - **Removed the resulting duplicate View-menu entry**: the tab-jump loop
    now provides "Lighting Studio" (Ctrl+Shift+8) automatically, so the old
    dedicated `m_controlMonitorAction` menu item was pulled from the View
    menu (`ui/src/app.cpp`) to avoid listing it twice. The action and its
    Ctrl+Shift+M shortcut still exist and still work
    (`slotControlMonitor()` → `Monitor::createAndShow()`), just not
    re-added to the menu.
  - **Fixed: main window title didn't update when a tab was double-click-
    detached, and detaching left a dangling tab-bar entry.**
    `App::slotDetachContext()` never called `m_tab->removeTab(index)` (only
    the reattach path's matching `insertTab()` existed) — added it, plus an
    explicit `updateWindowTitle()` call after both detach and reattach for
    safety. Also fixed `m_tabOriginals`' positional-indexing fragility: it's
    built once at startup in construction order and is never reordered when
    tabs are removed/reinserted, so a title/label lookup by index would
    silently point at the wrong tab after any detach. Replaced with
    `QTabBar::tabData()` (set once per tab in the `addTab` lambda, and
    re-set after `insertTab()` on reattach) — this travels correctly with
    each tab through remove/insert, unlike a parallel array indexed by
    position. Same fix applied to the workspace-file `DetachedWindow`
    XML-restore loader, which had the identical missing-`removeTab()` bug.
  - **Shows tab: Follow MTC toggle made authoritative and discoverable
    from within the tab itself** (`ui/src/showmanager/showmanager.{h,cpp}`).
    Prompted by Branson asking where Timer-vs-MTC is actually selected —
    turned out my round-2/3 redesign showed a read-only "● FOLLOWING MTC"
    indicator with no way to toggle it from the Shows tab at all (the only
    control was the Control-menu / footer-MTC-chip toggle, both
    `App`-level). Replaced the static label with `m_followMtcButton`, a
    `QToolButton` bound via `setDefaultAction(m_followMtcAction)` — the
    *same* `QAction` already driving the Control menu and footer chip, so
    all three stay in sync for free. Placed in the always-visible part of
    the transport cluster (not gated by mode) so it's reachable to arm
    *or* disarm follow from either state.
    - **Regression caught and fixed**: `updateMultiTrackView()`'s
      `blockSignals()`-wrapped sync of `m_followMtcAction`'s checked state
      (run on every show switch) updates the checked flag correctly, but —
      because signals are blocked on purpose — never runs
      `slotFollowMtcToggled()`, which is where the button's *text* ("Follow
      MTC (off)" vs "● FOLLOWING MTC") got set. Result: button showed stale
      text out of sync with the actually-correct widget visibility. Fixed
      by moving the text update into the shared `updateTransportVisibility()`
      helper (already called from both paths for the widget-swap fix
      earlier this session), so text and visibility can no longer drift
      apart again.
  - **Three Shows-tab track-header bugs fixed** (`ui/src/showmanager/trackitem.cpp`),
    all found and root-caused by Branson during review:
    1. *Dimmer/intensity bar drawn over the green active-indicator bar*:
       `m_intensityRegion` started at x=8, overlapping the active-indicator
       rect at x:1-11. Moved to start at x=14 (same right edge as before).
    2. *Double-clicking Mute/Solo/Lock opened the track-rename dialog*:
       `mouseDoubleClickEvent()` correctly excluded the name area but still
       fell through to `emit itemDoubleClicked()` for the button row, which
       `ShowManager::slotTrackDoubleClicked()` wires straight to a rename
       `QInputDialog`. Now returns early or the M/S/L/intensity regions
       without emitting anything — a double-click there is just two
       ordinary toggle clicks (already handled by `mousePressEvent`).
    3. *Intensity bar couldn't be dragged, only click-set*: `mousePressEvent()`
       calls the base `QGraphicsItem::mousePressEvent()` first, whose
       default implementation ignores the event for an item that's neither
       movable nor selectable (neither flag is set on `TrackItem`) — an
       ignored press means the scene never grabs the mouse for this item,
       so `mouseMoveEvent()` (which already had correct intensity-drag
       logic) was simply never delivered. Fixed with an explicit
       `event->accept()` right after the base-class call.
  - Everything above verified live via rebuild + relaunch + screenshot
    (title bar, tab strip, Follow MTC button text/visibility together) —
    not code-review-only. The three `trackitem.cpp` mouse-handling fixes
    are code-review-verified only (this environment can't synthesize a
    real double-click or a mouse-drag, the same standing limitation noted
    elsewhere in this file) — worth a real check next time at the rig.

- **Show Editor toolbar redesign (Show/Edit split, centered context-aware
  transport, window-title fix) — SHIPPED (2026-08-17).**
  `ui/src/showmanager/showmanager.{h,cpp}`, `ui/src/app.{h,cpp}`. Follow-on
  to the round-2 toolbar work below, going further specifically on the Show
  Editor per Branson's detailed spec:
  - **"Show ▾" split from "Edit ▾".** The single "Add" dropdown from round 2
    got split in two: **Show** (New Show / Rename show / Delete show — which
    show container you're working on) and **Edit** (Add Track/Sequence/
    Audio/Video + Undo/Copy/Paste/Delete/Change Color/Lock/Timings —
    everything that touches the open show's contents). Matches the user's
    explicit split ("show: add/rename/delete" vs. "edit functions can go
    under Edit").
  - **Snap to Grid moved to a new bottom toolbar row** (`m_bottomToolbar`),
    mirroring the 2D view's own Grid/Subdiv/Snap row at the bottom of its
    canvas.
  - **Play/Stop merged into one button.** `m_playStopButton`
    (`QToolButton::MenuButtonPopup` + `setDefaultAction(m_playAction)`):
    primary click keeps the existing Play/Pause toggle behavior unchanged
    (pause-in-place while running, resume on click, existing MTC-follow
    safety no-op); Stop (full stop + rewind) moved to the button's dropdown
    rather than staying a separate always-present toolbar button. Confirmed
    with Branson before implementing (AskUserQuestion) — Pause behavior
    stays, nothing was dropped.
  - **Transport cluster centered, DAW-style, and context-aware.** Flanking
    expanding-stretch spacers center the whole cluster regardless of what's
    docked left/right (`leftStretch`/`rightStretch`, matching how Logic
    centers its transport). Time position + Length stay always visible
    (position readout doubles as "show position" in either mode). Exactly
    one of two widgets shows depending on the show's actual
    `timecodeFollow()` state: **manual mode** — Play/Stop, Time division,
    BPM (`m_transportManualWidget`); **MTC mode** — a read-only "●
    FOLLOWING MTC" indicator + the TC-offset config button
    (`m_transportMtcWidget`), since there's nothing for local playback
    controls to do while an external code is driving the timeline.
  - **Real bug hit and fixed along the way**: the manual/MTC widgets were
    both showing simultaneously. Root cause #1 — `updateMultiTrackView()`
    (runs whenever the active show changes) syncs `m_followMtcAction`'s
    checked state via `blockSignals()` so switching shows doesn't
    re-arm/disarm anything, but that silently bypassed the
    `toggled()`-driven `slotFollowMtcToggled()` where the visibility swap
    lived — fixed by factoring the swap into `updateTransportVisibility()`
    and calling it from both places. Root cause #2, found only after
    root cause #1's fix still didn't work: `QToolBar::addWidget()`
    implicitly wraps a widget in a `QWidgetAction`; a later toolbar layout
    recompute (`applyToolbarLabelMode()`'s `setToolButtonStyle()` call)
    re-syncs the widget's visibility **from that wrapping action**, which
    was never touched — silently undoing a plain `widget->setVisible()`
    call. Fixed by toggling visibility through the `QAction*` that
    `addWidget()` returns (`m_transportManualAction`/`m_transportMtcAction`)
    instead of the widget directly. Root-caused via a scratch debug build
    (temporary `QFile`/`QTextStream` logging, since `qDebug()` output
    wasn't reaching any log this environment could see) rather than guessing
    — removed before shipping.
  - **"Is Length superfluous?" — answered, kept.** Not superfluous: it
    exists specifically because the timeline's own draggable end-handle can
    sit off-screen on a long show, so `m_lengthButton` stays as an
    always-reachable way to set/inspect it regardless of transport source.
  - **Window title now always shows the showfile name and the active tab**
    (`App::updateWindowTitle()`, replacing the title-building half of
    `slotDocModified()`; new `App::slotTabChanged()` wired to
    `m_tab`'s `currentChanged`). Format:
    `qlcconsole - <file or "New Workspace">[ *] - <Tab Name>`, confirmed
    live via `AXRaise` window-name checks (title flips from "…-
    Inputs/Outputs" to "…- Shows" the instant the tab changes). Uses
    `m_tabOriginals` rather than `m_tab->tabText()` for the tab name since
    the latter goes blank under Icons-Only tab-label mode (the toggle added
    in round 2) — fixed the same latent bug in `slotDetachContext()`'s own
    `tabLabel` capture while touching this code. Detached windows
    (`DetachedContext`, previously titleless) now get a one-time title at
    detach time (`<app> - <file> - <tab label>`) — not live-updated
    afterward if the doc's modified state changes while detached, which
    would need tracking every currently-open detached window; scoped out
    as more than what was asked.
  - Verified live in the running app throughout (screenshots + `AXRaise`
    window-name checks), including catching and fixing the visibility bug
    before calling this done rather than shipping on code-review alone.

- **Toolbar-consolidation, round 2: VC's duplicate mode toggle, a VC "Edit"
  dropdown, Shows tab's Add cluster, and a View-menu toolbar-style switch —
  SHIPPED (2026-08-17).** Direct follow-on to the round below, from a fresh
  pass over what still looked cluttered/inconsistent:
  - **VC's redundant Run/Stop button removed**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`). VC had its own
    always-visible checkable "Run"/"Stop" `QToolButton` (top-right,
    `m_runButton`) toggling `Doc::mode()` directly, *in addition to* the
    app-global "Operate"/"Design" toggle (`App::m_modeToggleAction`, on
    App's own top bar, confirmed always-visible across every tab/mode via
    screenshot). Both drove the exact same single piece of state. Confirmed
    safe to remove the VC-local one: `App::slotModeChanged` is wired to
    `Doc::modeChanged` (`app.cpp:688`) and already keeps the global button's
    icon/text/tooltip in sync regardless of what triggered the mode change.
    The VC-local button's original justification (VC's *own* toolbar
    `m_toolbar->hide()`s itself in Operate mode, per `disableEdit()`,
    `virtualconsole.cpp:~1890` — "there's nothing usable there in operate
    mode") is moot since the global button lives outside that toolbar and
    was never affected by it. Removed the button, its `runBar` row, its
    `slotModeChanged()` sync block, and the member/init-list entries.
  - **VC "Edit" dropdown**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`): the toolbar's twelve
    remaining per-widget actions (Cut/Copy/Paste/Delete/Widget
    Properties/Rename Widget/Bring to front/Send to back/Background
    Color/Background Image/Font Colour/Font) collapsed into one
    `m_editButton` ("Edit ▾", `:/edit.png`), right after the "Add" button.
    Deliberately a *fresh* small `QMenu` built locally in `initMenuBar()`
    (not a reuse of `m_editMenu`, unlike how Add reuses `m_addMenu`) — using
    `m_editMenu` would have also dragged in the dynamically-appended custom
    "Add" submenu (`updateCustomMenu()`), duplicating what the new Add
    button already offers. Entries stay individually enabled/disabled by
    the *existing* `VirtualConsole::updateActions()` selection logic,
    unchanged — with nothing selected, Paste/Background/Font remain
    clickable (they legitimately target the canvas) while
    Cut/Copy/Delete/Properties/Rename/stacking show up grayed rather than
    vanishing, matching the user's "only edit with sub selections" ask
    without needing new gating code. Keyboard shortcuts (Ctrl+X/C/V,
    Delete, Ctrl+E) are untouched — they live on the `QAction` objects
    themselves via `setShortcut()`, independent of which menu/toolbar the
    action is currently displayed in, so moving these into a dropdown had
    zero effect on them.
  - **Shows tab's add-element icons folded into the same paradigm**
    (`ui/src/showmanager/showmanager.{h,cpp}`): New Show + Add Track/New
    Sequence/New Audio/New Video (five separate toolbar buttons) collapsed
    into one `m_addButton` ("Add ▾", `:/edit_add.png`), placed first —
    same `QAction`s, same shortcuts (Ctrl+H/N/E/A/D). `ShowManager` also
    gained the `applyToolbarLabelMode()` method every other manager already
    had (it was missing entirely — the toolbar was stuck on Qt's default
    icons-only regardless of the app setting).
  - **Programming tab's "Add" button retrofitted onto the same shared
    setting** (`ui/src/programmingmanager.{h,cpp}`): previously hardcoded
    to `Qt::ToolButtonTextBesideIcon`; now has its own
    `applyToolbarLabelMode()` reading `workspace/tabLabelMode` like the
    others (found via `App::findChild<ProgrammingManager*>()`, the same
    idiom `App` already uses elsewhere for this tab, since Programming has
    no singleton `instance()` of its own).
  - **New View → Toolbar Style submenu** (`ui/src/app.cpp`,
    `initMenuBar()`/View menu, right after the existing Theme submenu):
    Icons & Text / Icons Only / Text Only, a checkable `QActionGroup`
    calling `App::setTabLabelMode()`. This setting (`workspace/tabLabelMode`,
    `App::m_tabLabelMode`) and its full propagation
    (`App::applyTabLabelMode()` → tab strip + main toolbar + every
    manager's own toolbar) already existed and was already being read
    correctly by every manager — there was simply **no UI control to change
    it**, only a raw QSettings value to hand-edit. `App::applyTabLabelMode()`
    extended to also call `ShowManager::instance()->applyToolbarLabelMode()`
    and the `ProgrammingManager` `findChild` lookup, so the new menu now
    covers all seven tabs' toolbars in one switch.
  - Verified live in the running app: VC toolbar screenshot confirmed
    "Add | Edit | VC Fixture Widget Wizard | Virtual Console Settings" with
    no Run button; Shows tab screenshot confirmed "Add | full show ▾ |
    Rename show | Delete show | Undo | Copy | Paste | Delete | …"; View
    menu screenshot confirmed the Toolbar Style submenu with a checkmark on
    the current ("Text Only") setting, and clicking "Icons & Text" updated
    the global toolbar, the Shows toolbar, *and* the bottom tab strip
    simultaneously in the same screenshot — confirming the single shared
    setting really does propagate everywhere. Reverted the live setting
    back to "Text Only" afterward so this testing didn't leave the user's
    persisted preference changed.

- **Function Manager / Virtual Console: same "Add" dropdown consolidation,
  plus a selection-aware VC context menu — SHIPPED (2026-08-17).**
  Follow-on to the Programming tab change below, extended to the other two
  tabs Branson flagged as still cluttered:
  - **Function Manager** (`ui/src/functionmanager.{h,cpp}`): the toolbar's
    nine individual "New scene/chaser/sequence/EFX/collection/RGB Matrix/
    script/audio/video" buttons + Folder collapsed into one `m_addButton`
    ("Add ▾", `:/edit_add.png`, `QToolButton` + popup `QMenu`) built from the
    *same* `QAction*` objects (`initToolbar()`), so shortcuts (Ctrl+1..9),
    slots, and the tree's own right-click menu (which already reused these
    actions) are untouched — purely a toolbar-layout change. Placed first
    (leftmost). `applyToolbarLabelMode()` extended to also style
    `m_addButton` (a toolbar-added `QToolButton` doesn't auto-follow
    `QToolBar::setToolButtonStyle()` the way `addAction()`-created buttons
    do).
  - **Virtual Console** (`ui/src/virtualconsole/virtualconsole.{h,cpp}`):
    same pattern — the toolbar's 14 individual "New Button/Button Matrix/
    Slider/Slider Matrix/Knob/Speed Dial/XY pad/Cue list/Frame/Solo frame/
    Label/Audio Triggers/Clock/Animation" buttons collapsed into one
    `m_addButton`, reusing the *already-built* `m_addMenu` (`initMenuBar()`)
    as-is — same object also feeds the menu-bar's own "&Add" entry and
    `VCFrame::customMenu()`'s right-click submenu, so no new action list was
    authored. Bonus: `m_addMenu` has 16 entries (also Programmer Frame /
    Show control, previously toolbar-omitted), so the dropdown now exposes
    two actions the old toolbar didn't. Placed first.
  - **VC empty-canvas right-click menu made selection-aware**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`,
    `ui/src/virtualconsole/vcwidget.cpp`): right-clicking empty VC canvas
    (nothing selected) used to always show the full `m_editMenu` — Cut/Copy/
    Delete/Rename/Widget Properties included, always enabled=false and
    grayed out but still visually present, with the "Add" submenu buried at
    the very bottom after Background/Foreground/Font/Frame/Stacking
    submenus. New `VirtualConsole::buildEmptyCanvasMenu()` builds a small
    purpose-built menu instead for exactly this case (called from
    `VCWidget::invokeMenu()`, gated on `vc->selectedWidgets().isEmpty()`):
    **Add first**, then Paste (if the clipboard has something), then
    Background/Foreground/Font (these legitimately apply to the bottom
    frame itself with nothing selected — confirmed via
    `VirtualConsole::updateActions()`'s existing enable/disable logic, which
    already special-cased them). Cut/Copy/Delete/Rename/Properties/Frame/
    Stacking are omitted entirely rather than shown disabled — exactly the
    set `updateActions()` already flags as meaningless with an empty
    selection, just now *absent* instead of merely grayed out. The
    when-something-**is**-selected right-click path is untouched (still
    `editMenu()`, unchanged) — this was scoped to the specific "empty
    space" complaint, not a redesign of the selected-widget menu.
    `m_bgMenu`/`m_fgMenu`/`m_fontMenu` (previously `initMenuBar()` locals)
    were promoted to members so both menus can reference the same QMenu
    objects. Deliberately did NOT touch the shared Cut/Copy/Delete/Rename/
    Properties `QAction`s' `setVisible()` — they're also on the toolbar, and
    hiding a shared `QAction` hides it everywhere it's added, which would've
    made toolbar buttons blink in and out during every right-click.
  - Both toolbar changes confirmed via screenshot in the running app (Add
    button present, correctly leftmost, old buttons gone). The VC
    empty-canvas menu content itself is code-review-verified only, not
    screenshotted — same synthetic-input gap as elsewhere in this file:
    there's no `cliclick`-equivalent for a real right-click in this
    environment, and the accessibility tree doesn't expose the VC canvas
    granularly enough to fake one via `AXShowMenu`. Worth a real look next
    time at the rig/laptop.

- **Programming tab: tab-local "Add" menu for New Scene/Chaser/… — SHIPPED
  (2026-08-17).** Branson's ask: find a middle ground between "no icons
  anywhere" and Function Manager's full icon toolbar, and figure out where
  per-function "Add" actions should live given the app has a genuinely
  global menu bar (File/View/Control/Help — confirmed scoped correctly,
  no change needed) plus tabs that get double-click-detached into their own
  bare `QMainWindow` (`App::slotDetachContext`, `app.cpp:1985-2003`) with no
  menu bar of their own. Landed as a small `QToolButton` ("Add ▾",
  `:/edit_add.png`) next to the func-tree filter box in
  `ProgrammingManager`'s nav panel (`ui/src/programmingmanager.cpp`,
  constructor, right after `m_funcTree` is built) — popup `QMenu` with one
  icon'd entry per creatable type (`:/scene.png`/`:/chaser.png`/
  `:/collection.png`/`:/efx.png`/`:/rgbmatrix.png`/`:/show.png`/
  `:/folder.png`, same icons Function Manager already uses), replacing the
  redundant idea of spelled-out "New Scene"/"New Chaser" buttons that just
  duplicated what right-click already offered. Creation logic extracted
  into one shared `ProgrammingManager::addNewFunction(Function::Type,
  const QString &folder)` (`programmingmanager.{h,cpp}`) so the toolbar menu
  and the func-tree's right-click menu (`slotFuncTreeMenu`) call the exact
  same path — no duplicated create/name/select/open logic. The right-click
  menu's own "New …" entries also picked up the same icons while touching
  this code, so the two menus now look and behave identically. Architecture
  point that drove the placement: this had to be a widget owned by
  `ProgrammingManager` itself (not a second app-level menu bar) — only
  widget-owned UI survives `setCentralWidget(context)` when a tab detaches;
  a second `QMenuBar` living on `App` would not follow the tab out.
  Confirmed via the existing Function Manager toolbar, which already proves
  the pattern (it's a child widget in the manager's own layout, so it
  already survives detach today). Built clean
  (`cmake --build build -j --target qlcplusui`, then the full app build);
  screenshotted in the running app and the "+ Add" button is present and
  correctly placed next to the filter box. *Menu contents not
  click-verified in-app* — same synthetic-input gap noted elsewhere in this
  file: `System Events` can invoke the button's `AXPress` (confirms the
  button exists/is enabled) but the resulting `QMenu` popup doesn't render
  for a screenshot to capture, so the actual dropdown items were verified by
  code review, not an eyeballed screenshot. Worth a real look next time at
  the rig/laptop. Not yet extended to other tabs (Function Manager already
  has its own working toolbar; nothing else currently has bare text "New …"
  actions to consolidate) — revisit if that changes.

- **"Look" as the assembly unit (Scene/Collection rethink) — BOTH SLICES
  RESOLVED (2026-08-19)**. Branson shower-thought, worked all the way
  through. Deliberately did NOT rename Scene/Collection (breaks traditional
  QLC users) — instead made the fork's **Look** first-class as two
  independent slices (per the show-lifecycle doc: (1) mainly serves
  Construction, (2) mainly serves Production):
  - **Slice 1 — explicit fixture scope, SHIPPED.** `Scene` gained a
    `LookScope` (`ScopeUnset` / `ScopeWholeStage` / `ScopeGroup` + a
    `FixtureGroup` id), a dedicated typed field following the existing
    `PaletteFade` precedent rather than a generic tag bag (no such bag
    exists on `Function`/`Scene`) — `engine/src/scene.{h,cpp}`:
    `setLookScope()`/`lookScope()`/`lookScopeGroupId()`, XML round-trip as
    `<Function>` attributes (absent = unset, old workspaces unaffected),
    carried through `copyFrom()`. Deliberately a 3-state enum, not "no
    group = whole stage" — unset and explicitly-whole-stage need to stay
    distinguishable. Unit-tested (`engine/test/scene`,
    `Scene_Test::lookScope()`, 21/21 passing). UI: a **"Scope:"** combo in
    `SceneGroupLooks` (the Looks editor, embedded in both the classic Scene
    Editor and the Programming tab canvas) — `ui/src/
    scenegrouplooks.{h,cpp}`, next to the Targets panel, populated from
    every `FixtureGroup` in the doc, kept separate from Targets (declared
    *intent* vs. what's actually painted). Purely organisational metadata —
    doesn't affect playback. *Not yet click-verified in-app* — this
    environment's synthetic-click automation doesn't register on tree/list
    rows (menu-item clicks work, raw clicks don't; same gap noted earlier
    for headful automation), so this shipped on code review + the engine
    test, not an eyeballed screenshot. Worth a real look next time at the
    rig/laptop.
  - **Slice 2 — palette/fixture state as the BASE that effects/RGBScripts
    consume — DONE, by finding + decision, not new code.**
    `EffectInstance::buildPalettesObject()`
    (`engine/src/effectinstance.cpp:894-1001`) already feeds a look's
    painted COLOUR into every running effect every tick
    (`palettes.look.colors`/`palettes.look.dimmer`, read by e.g.
    `resources/rgbscripts/lines.js` via `RGB_HOST_WRAPPER`), already
    falling back to the look's full painted base colour when nothing is
    explicitly nested after the effect (line 953) — "effects respect the
    look's master Dimmer" (shipped earlier) is the same mechanism for
    intensity. **Decided**: this is the correct and complete scope for
    "state as base" — colour/intensity are it, deliberately, not a partial
    build. Everything else a script exposes (`lines.js`'s
    `linesMovement`/`linesType`/`linesPattern`/`linesDistribution`, and the
    equivalent `algo.properties` across the other
    `resources/rgbscripts/*.js` files) stays 100% owned by the
    effect/script itself (`ui/src/lookeditor.cpp`'s properties dialog),
    **on purpose**: colour is already an *external input* in stock QLC+'s
    own RGBScript API (`rgbMap()` scripts are written expecting a colour
    handed in — upstream RGBMatrix always worked this way; deriving it from
    a Look is just substituting *where* that input comes from).
    `algo.properties` are declared and owned *by the script* as its own
    configuration contract — nothing upstream expects those externally
    driven, and forcing it would invent behavior with no basis in how stock
    scripts are authored, breaking compatibility/portability of scripts
    brought in from upstream QLC+.
  - **Terminology settled** (for consistent discussion going forward): see
    the vocabulary table worked out this session — Scene (engine
    primitive) vs. Look (a Scene with Palettes attached, built via the
    Programming tab) vs. Target (what a Look actually paints) vs. Scope
    (what a Look is declared *for*) vs. Palette/Base/Effect/effect-scoped
    palette. Audited the UI against it: no tab-level renames needed —
    "Scene Editor" and the "Scene" type-folder in Function Manager
    correctly refer to the engine primitive, not the workflow, and
    shouldn't become "Look." One real drift found and fixed:
    `SceneGroupLooks`' Targets label said "Fixtures in Scene" in the UI
    while the header's own doc-comment already called it "Targets"
    (`ui/src/scenegrouplooks.h:124`) — code and label now agree
    (`ui/src/scenegrouplooks.cpp`: the label text, its live count update,
    and the intro paragraph's "fixtures in this scene"/"scene fixtures"
    phrasing all read "Targets" now).

- **Release-gate scoping, phases 1+2, + all 3 surfaced failures fixed
  (2026-08-17, BUILT)** — first concrete work off `SHOW_LIFECYCLE_DESIGN.md`'s
  "good gates" thread.
  - **Phase 1 — the macOS `make check` gate was silently broken**, and my
    first fix attempt misdiagnosed *how* it's invoked. There are actually
    **two** `unittest.sh` files: a root-level staging wrapper (already
    correct, already copies every `test.sh` + needed resources into `build/`
    and `cd`s there) that then runs the *copy* of `platforms/linux/
    unittest.sh` it just placed in the build dir. The real, narrower bugs
    were only in `platforms/linux/unittest.sh`: `RUN_UI_TESTS` never got set
    to `1` on darwin at all (`ui/test/*` was unconditionally skipped), and
    the UI-test loop bypassed each test's own `test.sh` (which already knew
    how to resolve a macOS `.app`-bundled QTest binary) in favour of a bare
    `./${test}_test` that doesn't exist for bundle-style tests. Fixed both,
    keeping the script's existing plain-relative-path assumptions intact
    (an earlier pass added `$2`/build-dir-prefixing logic on a wrong model
    of the invocation chain — reverted). Also filled a structural gap the
    now-working gate immediately hit: `engine/test/markplanner` had no
    `test.sh` at all (a real test, just missing its runner).
  - **Phase 2 — model-layer add/remove coverage.** `MonitorProperties` had
    zero test coverage for `addPipe/removePipe` (Boom/Bar/Electric),
    `addStand/removeStand`, `addTower/removeTower`,
    `addStageTarget/removeStageTarget`, and `removeTruss` (add was tested,
    remove wasn't) — added `stageStructureAddRemove()` +
    `stageStructuresXmlRoundTrip()` to the existing
    `engine/test/monitorproperties` suite. `PowerDistribution` (sources,
    circuits, fixture assignment, the direct-source auto-create-circuit-0
    behavior, XML round-trip) had **no test dir at all** — added
    `engine/test/powerdistribution` from scratch.
  - **All 3 pre-existing failures the gate surfaced are now fixed:**
    - `inputoutputmap::profileDirectories()` and
      `qlcfixturedefcache::defDirectories()` both failed on
      `QCoreApplication::applicationDirPath: Please instantiate the
      QApplication object first` — both binaries used `QTEST_APPLESS_MAIN`
      (no `QCoreApplication` instance at all), but the code they exercise
      (`QLCFile::systemDirectory()`) calls `applicationDirPath()` on macOS.
      Switched both to `QTEST_GUILESS_MAIN` (constructs a `QCoreApplication`,
      no widgets/GUI needed). That fixed the warning but exposed the real
      bug underneath: `systemDirectory()` resolves paths relative to the
      app-bundle executable (`Contents/MacOS/<app>` → `../Resources/...`),
      but each test's own expected-path construction assumed a flat
      CWD-relative path — true on Linux (where `systemDirectory()` doesn't
      consult `applicationDirPath()` at all) but not on macOS. Fixed both
      tests' expected-path construction to mirror the same
      `applicationDirPath()/../<dir>` relationship on Apple platforms.
    - `rgbscript`'s "Lines" script failed because its `linesMovement` and
      `linesLifecycle` list properties' declared defaults
      (`algo.linesMovement = 0` / `algo.linesLifecycle = 0`) are dead code —
      `getMovement()`/`getLifecycle()` actually read `algo.linesSlide`/
      `algo.linesRollover`/`algo.linesSizeBehavior`, never given a top-level
      default, so the very first (pre-`setMovement()`/`setLifecycle()`) read
      returned `""` — not one of the property's own declared list values.
      Harmless at runtime (undefined behaved like the intended default, 0),
      but broke property introspection. Fixed by initializing all three
      backing variables at the top of `resources/rgbscripts/lines.js`.
    - Verified clean with **two full, independent `cmake --build build
      --target check` runs** (not just the individual binaries) — fixture
      validation, all `engine/test/*`, all `ui/test/*`, and the enttecwing/
      midi/artnet plugin tests all pass end-to-end.
  - **`mastertimer_test` segfault — found + fixed (2026-08-17).** Root cause
    was a real, deterministic bug, not pure flakiness: `interval()`'s cleanup
    (`fs.stop()`, `mt->unregisterDMXSource(&dss)`) sat *after* a `QVERIFY` on
    a razor-thin real-time tick-count window (49–51 ticks/sec — upstream's
    own `SKIP_TEST` escape hatch for Travis CI is an acknowledgment this was
    always too tight under real scheduling load). `QVERIFY` returns
    immediately on failure, so a timing miss under load skipped cleanup
    entirely — `fs`/`dss` (stack locals) got destroyed while still registered
    with `MasterTimer`, leaving dangling pointers that crashed the *next*
    test method's `timerTick()`. Fixed by moving cleanup before the timing
    assertions (always runs now, regardless of outcome) and widening the
    tolerance to 40–60 (still catches a genuinely broken timer, far less
    sensitive to scheduler jitter). Verified clean on **2 more full `make
    check` pipeline runs** (4 total now, back to back). Not caused by
    anything built in this session (confirmed: `mastertimer` runs and
    completes before
    `ui/test` even starts, so the new `monitor_test` below isn't a factor).
  - **Phase 3 — headful dialog-driven pilot (2026-08-17, BUILT, proven
    viable).** New `ui/test/monitor` — the open question was whether a QTest
    UI test can drive a real, *blocking* `QDialog::exec()` call (as
    `Monitor::slotAddTruss()` uses) under `QT_QPA_PLATFORM=offscreen`.
    It can: schedule the interaction via `QTimer::singleShot(0, ...)` *before*
    calling the slot — `exec()`'s own nested event loop processes it,
    `QApplication::activeModalWidget()` finds the live dialog, `findChild<>()`
    reaches its fields/buttons. `addTrussAccepted()` fills the name field and
    clicks OK, asserts the truss landed in `MonitorProperties` with the right
    name; `addTrussCancelled()` clicks Cancel, asserts nothing was added.
    Both pass, standalone and through the full `make check` pipeline. Power
    Source turned out not to need this pattern at all — its add path
    (`PowerDistributionWidget::slotAddSource()`) is a direct model mutation
    with no dialog, already covered by Phase 2's `powerdistribution` tests —
    so the pilot narrowed to just Truss, which was the one open technique.
    Setup is cheap to replicate (`#define protected public` to reach the
    slot + a bare `Doc`/`Monitor` pair, no plugin/patch wiring needed).
  - **Phase 4 — expanded coverage + `slotRemoveSelected()` (2026-08-17,
    BUILT).** Added to `ui/test/monitor`:
    - `addTargetAccepted()`/`addTargetEditCancelled()` (`Monitor::
      slotAddTarget()`) — same simple-form-dialog pattern as Truss, plus a
      real behavioral difference worth proving: unlike Truss (object created
      only on Accept), StageTarget/Platform/Pipe/Stand/Tower are all created
      *immediately* with defaults, and the dialog that follows is an *edit*
      of the just-created object — so even Cancel leaves it added. Also
      asserts the accept path's bonus effect: a linked PanTilt palette gets
      auto-created.
    - `addPlatformEditCancelled()` (`Monitor::slotAddPlatform()`) — proves
      the same add-then-cancel path through a **heavier** edit dialog (one
      that embeds a full `StructureStudioView` canvas/tree/inspector via
      `makeStudioPane()`, unlike Truss/Target's plain `QFormLayout`).
    - `removeSelectedTruss()`/`removeSelectedCancelled()` — the other open
      half of Phase 3: select a `TrussItem` in the `QGraphicsScene`
      (`item->setSelected(true)`), call `slotRemoveSelected()`, which drives
      a **second, different kind of modal** — `confirmFeatureDelete()`'s
      `QMessageBox`, found the same way via `activeModalWidget()` — Accept
      removes it, Cancel doesn't.
    - **Two real bugs found writing these, both fixed in the tests
      themselves (not app code):**
      1. `QMessageBox::windowTitle()` reads back **empty** on macOS — native
         alert-style message boxes don't surface a title bar, even though
         `confirmFeatureDelete()` does call `setWindowTitle()`. Asserting on
         it left the confirm dialog's `exec()` with nothing ever clicked —
         a genuine **hang**, not a fast failure, and it corrupted whichever
         test ran next. Fixed by asserting on `QMessageBox::text()` instead
         (the actual message content), which *is* reliable.
      2. Blind `findChild<QLineEdit*>()` (no name filter) is ambiguous
         once a dialog embeds `StructureStudioView` — it contains its own
         QLineEdits, so the lookup can silently grab the wrong one instead
         of the name field. Rather than paper over it, **descoped**: no
         "accept with a custom name" test for Platform (or, by the same
         reasoning, Pipe/Stand/Tower — never attempted). The cancel-path
         test for Platform only drives the unambiguous button box, so it
         stays real coverage without the fragile lookup. Reliably testing
         the accept path for these four would need the production dialogs
         to tag their name field with `setObjectName()` first — not done.
    - Verified: 9/9 pass standalone, and **2 more full `make check` pipeline
      runs**, clean both times.
  - **Phase 5 — `release.sh` gate (2026-08-17, BUILT).** New step **1/6**
    (renumbered the existing 5 steps to 2/6–6/6): `cmake --build build
    --target check`, against the standard dev `build/` dir (Debug, reused
    as-is — a correctness gate on the codebase, not a rebuild of the exact
    Release bits `package-local.sh` ships from its own separate
    `build-package/`). `set -euo pipefail` (already at the top of
    `release.sh`) means a failing gate aborts the *entire* release right
    there — before `package-local.sh`, signing/notarizing, tagging, or
    publishing ever run. Verified both directions: the real gate command
    passes clean against this session's actual `build/`; a synthetic
    reproduction of `release.sh`'s exact structure with a deliberately
    failing stand-in step confirmed `set -e` genuinely halts before any
    later step's `step "2/6 ..."` banner even prints (did **not** run the
    real `release.sh` itself — that pushes a git tag and publishes a public
    GitHub Release, real external side effects, not something to fire off
    to validate a shell-flow change). `RELEASE.md`'s step list updated to
    match.
  - **Release-gate arc (Phases 1-5) is now complete end to end.** Remaining
    known gap: full add/remove dialog coverage for Pipe/Stand/Tower, gated
    on tagging their production dialogs' name fields with `setObjectName()`
    first (see Phase 4) — not chased further, no immediate need driving it.

- **Hardware tab: Power tree + universe usage grid (2026-08-12 → 08-13,
  BUILT)** — the former "Fixtures" tab is renamed **"Hardware"** (`app.cpp`,
  one-line tab-label change). Its tree already had a lazily-built
  "Universes" folder (`FixtureTreeWidget::updateTree()`); selecting a
  universe node now swaps the right-hand pane to `UniverseUsageWidget`
  (`ui/src/universeusagewidget.{h,cpp}` — embedded in the splitter like the
  group-layout editor and power view, *not* a popup dialog; started as one,
  corrected after eyeballing it) — a 512-cell address grid coloured per
  occupying fixture (deterministic hue from fixture ID) built on
  `Doc::fixtureForAddress()`, with a tooltip per cell and a fixture legend
  below. A new **"Power"** folder (peer to Fixture Groups/Universes, gated
  behind a new `FixtureTreeWidget::ShowPower` flag so the shared tree widget
  doesn't pick it up in picker dialogs, and *always present* even with zero
  sources so it can be selected/added-to) lists `PowerDistribution` sources →
  circuits → assigned fixtures, mirroring the existing group-folder nesting.
  Right-click the Power folder → **"Add power source…"**; right-click a
  source/circuit → **"Add circuit…"** (same defaults as the Power pane's own
  buttons). Dragging a fixture from anywhere in the tree onto a circuit *or
  a bare source* (lands on its first circuit, auto-created if needed) assigns
  it via `PowerDistribution::assignFixture()` — the same call the existing
  right-click "Add to power circuit" menu already used, now with visual/drag
  entry points too. `slotSelectionChanged()` was rewritten to route
  explicitly by selection type (group → layout, universe → usage grid, any
  Power-tree node → power view, single fixture → its info via the
  previously-dead `fixtureSelected()`, else → generic info) instead of
  defaulting everything-but-groups to the power view. The in-canvas
  Programming-tab power footer (dead code — `ProgrammingManager::
  m_powerFooter` was force-hidden since the readout moved to the app
  status-bar chip) is fully removed; its "Circuits…" button is replaced by
  making the status-bar Power chip itself clickable
  (`ProgrammingManager::openCircuitsDialog()`, wired the same way the MTC
  chip's click-to-bind menu already works).

- **Footer chip polish (2026-08-12)** — Power/Dangle chips (`⚡`/`⚠`) were
  rendering as full-size color emoji next to plain-text chips (Ready/Autosave/
  Saved), reading as a font-size mismatch though the point size was identical
  the whole time; root cause was Unicode emoji-presentation glyphs, not a
  QFont issue. Fixed by appending the text-presentation variation selector
  (U+FE0E) to `⚡`/`⚠`, and gave MTC (`⏱`) and Load (`⚙`) their own leading
  icon in the same style, so all four global status-bar chips read at one
  consistent visual size. Also corrected `platforms/macos/Info.plist.qmlui`
  (only installed when `qmlui` is built) — it still had the pre-rebrand
  `qlcplus-qml` executable/name and `qlcplus.icns` icon refs with no
  `CFBundleIdentifier` at all; now matches the widgets build's
  `com.bransonmatheson.qlcconsole` identity.

- **Note-effect calibration + MIDI plumbing** — per-universe MIDI source scoping
  (`data.midi.universes[n]` + a device-name dropdown, not a slider); switching a
  look's effect script now reloads the live preview; **live param edits reach the
  running effect** (`reloadParamsFromPalette`/`updateEffectParams` — was the root
  cause of "axis does nothing / Learn won't lock"); persistent **Learn range**
  button (writes noteLow/noteHigh, flips to Manual). `QLC_EFFECT_DEBUG=<path>`
  env trace kept (grid/range/held/lit-cols + col0/col64 fixture+DMX+base).
- **Looks: effect-vs-fixture colour separation + tree UI** — a Colour/Dimmer
  palette *nested under* an Effect in the Looks tree feeds that effect and is NOT
  painted as a static base; top-level looks light the fixtures. Fixes same-colour
  strike-on-base washout (RGB-only fixtures). Mechanism = ordering
  (`QLCPalette::isEffectScoped`); tree is derived from/rewrites the flat order.
  Right-click → "Move to fixtures (base)" / "Feed effect ▸ <name>" for explicit
  re-homing; Up/Down moves across nesting boundaries (flat-order move).
  *Open polish:* dropping an external palette onto a specific effect item should
  nest under THAT effect (currently appends). Future: the richer per-item
  assignment could grow beyond order (explicit bind) if multi-effect looks get
  fiddly.
- **Effect perf + release fade-out** — input-reactive effects (midi/audio/
  joystick) now WAIT for input instead of polling: an idle effect whose last
  frame drove nothing is skipped until fresh input (`m_inputDirty` +
  `lastFrameEmpty()`), killing the 50 Hz baseline load of a loaded note effect on
  a big pixel grid. And effect looks have a **release fade-out** set via the Looks
  Fade Out cell — on stop the effect decays over that time instead of snapping
  (EffectInstance fade envelope; runner keeps it ticking then reaps).

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Build host: Raspberry Pi 4 (TODO, 2026-08-25)** — the highest-value box to
  add, because it answers an open question rather than just adding a build:
  1. **Tick punctuality.** 51 universes at `transmitMode="Full"` on a 14-core
     M-series logged 2 `MasterTimer is running late` events in 61 s at only
     50% of one core (see the open item above). Four weak ARM cores is where
     that either holds or doesn't. This is the measurement still outstanding.
  2. **Different ABI.** ARM makes plain `char` *unsigned* where x86 makes it
     signed, plus stricter alignment. DMX values are compared as `char` in
     places (e.g. `QCOMPARE(ua.preGMValues()[0], char(255))` in
     `ui/test/vcxypad/` — `-1` on x86, `255` on ARM). No amount of x86 testing
     finds that class.
  3. **Real target.** Upstream QLC+ ships Raspberry Pi builds; Pi-based
     lighting controllers are common.
  Build 32-bit (armhf) and you also get `qsizetype` as 32 bits — the width
  assumption behind the `%d` format bug fixed in `mastertimer.cpp` this session.
  Setup mirrors 192.168.1.245: cmake, Qt dev packages, xvfb, python3-lxml.

- **Build host: Debian 12 guest (TODO, 2026-08-25)** — a *newer* toolchain
  bound. CI is Ubuntu 22.04 (GCC 11, glibc 2.35, Qt 6.8); 192.168.1.245 is
  Debian 11 (GCC 10, glibc 2.31, Qt5). Bookworm gives GCC 12 + `qt6-base-dev`,
  which is the only combination not currently covered anywhere: Qt6 **and** a
  compiler newer than CI's. Deliberately NOT another Ubuntu 22.04 box — that
  duplicates CI exactly and would never find anything. The value of
  192.168.1.245 was proven the day it was built: its older glibc exposed a
  pthread linkage bug (`4a360911c`) that Ubuntu 22.04 structurally cannot see.

- **Decide whether qlcconsole supports Intel Macs (TODO, 2026-08-25)** — a
  product question, not a coverage gap, and worth settling before building
  hardware for it. The shipped binary is **arm64 only** (`file` on both the
  build output and the packaged app); nothing sets `CMAKE_OSX_ARCHITECTURES`,
  so it targets whatever host builds it. An Intel Mac therefore cannot run the
  current DMG at all. Note this is NOT a Rosetta problem — Rosetta translates
  x86 -> arm, not the reverse, so its sunset doesn't threaten an arm64 build.
  If the answer is "yes, support Intel", the cheap route is probably a
  universal binary (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`; Qt from
  install-qt-action ships universal for macOS) rather than a second build
  machine — one CI config change instead of another host to maintain. Branson
  has an Intel Mac available if a real second machine turns out to be needed.


- **MasterTimer misses ticks at full output load (2026-08-25, 2026-09-02) —
  fixed on macOS, ROUGHED IN elsewhere, NOT YET VERIFIED on Linux/Windows.**
  With 51 universes forced to `transmitMode="Full"` on `ender` (~2484 pkt/s,
  ~49 Hz per universe) a 61 s run logged **2** `MasterTimer is running late`
  events while a 91 s run at the same settings logged **0**. CPU was only 50%
  avg / 64% peak of one core, so this is not throughput saturation — it's
  scheduling jitter, and it's the same wall-clock sensitivity that made
  `MasterTimer_Test::interval()` and `VCCueList_Test::functionRemoved()` flaky
  on CI. Reproduce with `/tmp/throughput.sh` on ender (see DONE.md 2026-08-25
  for the harness).

  **macOS/iOS — fixed, and previously validated** (`feat/realtime-timer-thread`,
  ported from the `rt-test` branch, `8487dbf71`): the DMX timer thread now
  requests Mach `THREAD_TIME_CONSTRAINT_POLICY` (what CoreAudio uses for the
  same problem) in `engine/src/mastertimer-unix.cpp`. Advisory — warns and
  falls back to normal priority if the kernel refuses. This is the run
  referenced above ("real-time-policy run showed zero events of either
  shape").

  **Linux — same file, `SCHED_FIFO` via `pthread_setschedparam`, ROUGHED IN,
  UNVERIFIED** (no Linux box in that session; CI will give a real compile/link
  result on push). Priority is `sched_get_priority_max(SCHED_FIFO) - 10`, not
  max, matching the JACK/PipeWire convention of leaving headroom above this
  thread. **Known limitation, not solved here:** `SCHED_FIFO` normally needs
  `CAP_SYS_NICE` or an rtprio limit, which a stock install will not have — so
  on most Linux installs this falls back to a warning + normal priority
  exactly like an mac refusal would. Packaging a fix (setcap in a postinst,
  or a bundled rtprio limits.d rule) is a separate, bigger decision — not
  done, flagged as its own backlog item below.

  **Windows — MMCSS ("Pro Audio" thread characteristics), ROUGHED IN,
  UNVERIFIED, NO TEST PATH AVAILABLE.** Architecturally different from the
  unix path: `mastertimer-win32.cpp`'s timer callback runs on a Windows
  thread-pool worker (`CreateTimerQueueTimer`), not a thread this code owns,
  so the boost happens inside the callback itself, `thread_local`-guarded to
  run once per actual OS thread. No revert on stop (would need to run on the
  same pool thread that acquired it, which nothing here tracks) — released
  when that thread exits at process end. This one has had zero real-world
  testing of any kind; treat as a rough sketch, not a working feature, until
  someone with a Windows box confirms it.

- **Linux: package a way to actually grant SCHED_FIFO (2026-09-02, follow-on
  to the item above, not started)** — the DMX timer thread's real-time
  priority request will silently fall back to normal scheduling on a stock
  Linux install (no `CAP_SYS_NICE`, no rtprio limit). Options to weigh:
  `setcap cap_sys_nice=eip` on the installed binary from a `.deb`
  postinst/AppImage hook, vs. documenting a manual `/etc/security/
  limits.d/qlcconsole.conf` rtprio rule (the traditional pro-audio-Linux
  convention, needs the user in a group like `audio`). Needs a real Linux
  box to even confirm the roughed-in `SCHED_FIFO` call links/behaves as
  written before this is worth deciding.

- **`check-all.sh`'s Qt6-Werror leg fails at CMake configure, not code (OPEN,
  2026-09-03)** — found running the full gate to verify the Fixture Manager
  menu change (see DONE.md): `run_gate "Qt6-Werror" ...` dies immediately
  with `CMake Error: The warning category "dangling-else" is not known.`,
  before touching a single source file. Confirmed NOT a code regression —
  the Qt6 and Qt6-Release legs (which actually build/link/test) both pass
  clean, and manually invoking `clang++ -Werror=dangling-else` directly
  compiles fine (Apple clang 21.0.0/Xcode, confirmed working). `cmake
  --version` on this host is **4.4.0** — a very recent major version;
  likely candidate is CMake 4.x's own compiler-diagnostics/warning-category
  validation (added for IDE integration in recent CMake releases) rejecting
  a `-Werror=<category>` flag it doesn't recognize by name, at configure
  time, independent of whether the compiler itself supports it. Not
  investigated further tonight — this is `check-all.sh` itself drifting out
  of sync with a CMake upgrade, same *class* of problem as the Linux CI
  `-Werror` break fixed 2026-09-02, but a different gate and needs its own
  look at what CMake 4.4 actually changed about `-Werror=` flag validation
  before guessing at a fix.

- **Gate hole: on headless Linux `make check` silently skips ALL UI tests and
  still prints "Unit tests passed" (OPEN, 2026-08-26)** —
  `platforms/linux/unittest.sh:44` decides whether to run `ui/test` from
  `pidof X` / `pidof Xorg`, unless `whoami` is `runner`/`buildbot`/`abuild`.
  On a headless box under a normal login both checks fail, so `RUN_UI_TESTS=0`
  and **32 UI test binaries never run** — a third of the suite — while the gate
  still reports success. The same branch also leaves `TESTPREFIX` empty, so the
  engine tests run with no platform plugin and `genericdmxsource_test` aborts
  with "could not connect to display". Note `xvfb-run` does NOT rescue it: the
  process is named `Xvfb`, which `pidof X` does not match. Fix is to key off
  "can Qt start a platform plugin" (or just always set
  `QT_QPA_PLATFORM=offscreen`) rather than probing for an X server.

- **The ARM char-signedness tripwire is commented out (OPEN, 2026-08-26)** —
  the assertion cited as the reason to want an ARM build host,
  `QCOMPARE(ua.preGMValues()[0], char(255))` at
  `ui/test/vcxypad/vcxypad_test.cpp:474-483`, sits inside a `/* FIXME !! */`
  block and runs nowhere. The live `char(...)` comparisons in
  `vcxypadfixture_test.cpp` are signedness-*insensitive* (both sides go through
  the same conversion) so they pass identically on x86 and ARM. The Pi 5
  confirms the ABI is there — plain `char` is unsigned, `__CHAR_UNSIGNED__`
  defined — but nothing in the suite exercises it. **Un-commenting that block
  is the experiment that cashes in on the ARM host existing**; until then the
  host proves nothing about this bug class.

- **Debian 12 setup quirk: Qt6 `lrelease` is not on PATH (2026-08-26)** —
  Debian's `qt6-l10n-tools` installs `lrelease`/`lupdate` to `/usr/lib/qt6/bin/`
  with no `/usr/bin/lrelease-qt6` symlink (unlike the Qt5 packages), and
  `translate.sh`'s `which_qt()` probes only `lrelease`, `lrelease-qt6`,
  `lrelease-qt5` on PATH. Build dies at 1% with "lrelease not found". Workaround
  `export PATH=/usr/lib/qt6/bin:$PATH`; durable fix is to fall back to
  `qmake6 -query QT_HOST_LIBEXECS`/`QT_HOST_BINS` or have CMake pass the
  `Qt6::lrelease` target location down.

- **buildhost-qlcplus is missing 10 custom fixture definitions (2026-08-26)** —
  headed runs there open with an error dialog: no definition found for Branson
  LED Movinghead+Circle, Branson LED SPOT, Betopper LM70, ADJ Focus Spot Three
  Z, Oppsk Wall Washer Light Bar, Warmdance XL-450, Junman Two Arm LED Beam,
  PHS Chorus Step Row 64 Heads, WLED Effect Mode. Same cause as the
  `/tmp/*.qxf` load failures seen on that host. Sync the `.qxf` files to
  `~/.qlcconsole/fixtures/` there before using it for real UI validation.

- **~~`-p`/`--operate` does not take effect on headless Linux~~ WRONG — it was
  missing fixture definitions (RESOLVED, 2026-08-26)** — the claim does not
  survive testing, and the reasoning behind it was faulty twice over:
  1. The check grepped the log for `"Starting startup function"`, which only
     prints when a startup function **exists**. Stock `surfacetesting.qxw` has
     `<Engine>` with no `Autostart` attribute, so it measured the absence of a
     startup function, not the absence of Operate mode.
  2. With a valid `Autostart` injected, `-p` works on Linux — proven twice:
     `drift-test.sh` (minimal generated workspace) runs its chaser to
     completion on Debian 12/aarch64, and `surfacetesting.qxw` + Autostart
     enters Operate on Debian 11 as well.
  **Actual root cause:** the custom `.qxf` fixture definitions were absent on
  both Linux hosts. Without them the fixtures fail to load, the functions that
  target them do not load either (`Function start` count 0), so there is no
  startup function, nothing changes universe data, `Universe::dumpOutput`
  short-circuits and the ArtNet plugin is never reached. That is the whole
  reason both Linux boxes transmitted zero packets and could not produce a
  punctuality measurement. Syncing the 99 `.qxf` files to
  `~/.qlcconsole/fixtures/` on each host fixes it.
  **Lesson worth keeping:** a grep for a success message is not a test for the
  condition — a missing log line had two possible causes and the wrong one was
  assumed. Assert the positive precondition first (does this workspace even
  define a startup function?).
- **Note: lateness magnitude was already logged before the `late_us` diag**
  — `mastertimer-unix.cpp:67,74` has a pre-existing
  `qDebug() << "Time is late by" << ... << "nanoseconds"` inside `compareTime()`.
  The new line's genuinely new information is **`compute_us` + `budget_us`**
  alongside it, which is what separates jitter from load; the raw lateness
  figure was obtainable already.

- **TRAP: on Linux, a build-dir run loads NO I/O plugins and silently outputs
  nothing (2026-08-25)** — cost a full 10-minute soak that looked like a clean
  pass. `IOPluginCache::load` resolves via `QLCFile::systemDirectory(PLUGINDIR)`
  (`engine/src/qlcfile.cpp:181`), and on Linux that branch is just
  `dir.setPath(path)` with `PLUGINDIR` a **compile-time absolute**
  (`/usr/lib/qt6/plugins/qlcconsole`, or `qt5` on the Debian 11 box). There is
  no env override. macOS is fine because its branch resolves
  `applicationDirPath()/../PlugIns`, which the build tree mirrors — so this
  bites only on Linux and only when running from `build/` without installing.
  The failure is **silent and looks like success**: every universe logs
  `setOutputPatch - plugin: "None"`, the app runs happily, CPU sits at ~0%, and
  a punctuality soak reports **zero** late events because nothing was ever
  transmitted. Check `grep -c artnet` in the run log, or CPU > 0, before
  believing any Linux throughput/punctuality number.
  Workaround for a build-dir run:
  `sudo mkdir -p $PLUGINDIR && sudo ln -sf ~/git/qlcconsole/build/plugins/*.so $PLUGINDIR/`.
  Worth considering a `QLC_PLUGIN_PATH` env override so test rigs stop needing
  root to run what they just built.

- **ArtNet failure reporting still flaps (OPEN, 2026-08-25)** — `e3905a2a1`
  replaced 2850 identical `sendDmx failed` lines with one report per universe
  per state change, which is an ~87% reduction but not the "one line" first
  claimed. A universe whose target never answers ARP oscillates fail →
  succeed → fail (ARP cache expiry), so the live soak still produced 200
  "is failing" + 150 "recovered" lines across 50 universes in 120 s. Wants
  hysteresis: don't report recovery until it has held for N sends. Deliberately
  NOT built on speculation, because the flapping was caused by absent hardware
  (rig in storage) rather than a real-world configuration.

- **`VCCueList_Test::functionRemoved()` flake — watch, don't assume fixed
  (OPEN, 2026-08-25)** — failed on 2 of ~9 macOS CI runs asserting a tree row
  count right after a 100 ms deferred refresh. `6d3757bb2` switched it to
  `QTRY_COMPARE`, which polls instead of sleeping a guessed interval. NOT
  proven: it passed 7 of 9 runs *before* the change too, and the failure could
  not be reproduced locally even under six spinning cores. Sustained green
  across many runs is the only evidence that counts. If it returns, the fix was
  insufficient rather than wrong. Five other UI test files still use fixed
  `QTest::qWait` and were left alone — none has failed.

- **Windows CI removed, not repaired (OPEN, 2026-08-25)** — `fb9c5fd74` deleted
  the `build-windows` job. Both legs failed at "Fix build" seding
  `platforms/windows/qlcplus4Qt6.nsi`, which the rebrand renamed, and
  RELEASE.md already lists Windows packaging as out of scope and stale. Removed
  rather than left permanently red, because a job that cannot pass trains
  people to ignore CI — which is exactly how this repo ended up with a workflow
  nobody noticed had never run. Re-add when Windows is a real target and the
  NSIS scripts have had their rebrand pass.

- **Warning backlog outside the `-Werror` set (OPEN, 2026-08-25)** — a
  tree-wide `-Wall -Wextra` sweep reports ~1100
  `-Wunnecessary-virtual-specifier`, 92 `-Wdeprecated-declarations` (all in
  Homebrew's OLA headers, not ours), 38 `-Wnon-c-typedef-for-linkage` (e.g.
  `PreviewItem`) and 20 `-Winconsistent-missing-override`. None is in the set
  CI enforces and most are third-party. Noted so the next person doesn't
  rediscover them and assume they're new.

- **Rig fidelity test not done (OPEN, 2026-08-25)** — throughput was measured
  at the NIC with tcpdump, which proves what the console *transmits* but not
  that a node *accepts* it. Branson has a single 4-universe node available;
  the full rig is in storage. That's the test that would confirm packet
  well-formedness end to end. `RIG_TEST_PLAN.md` as a whole remains
  unexercised.


- **Output ENDPOINT reachability check (BACK BURNER, 2026-08-25)** — the
  readiness indicator added in `daeeb97d7` catches one failure mode: the
  workspace names a plugin *line* (for the network plugins, an index into the
  local interface list) that doesn't exist on this machine. It does **not**
  catch the other one: the line is fine, but the ArtNet *node* at the far end
  isn't answering. Found during the live soak on `ender`, where 50 of ~53
  patched universes were failing to send to 13 node addresses
  (`172.18.2.201`–`.230`) — in that instance correctly, because the devices
  were in the trailer, which is exactly why this needs care rather than a
  naive "ping the target" test:
  - a broadcast/subnet target legitimately has nothing to answer it, so
    unreachable ≠ misconfigured;
  - nodes are routinely powered down between calls, and a desk that cries
    wolf every load gets ignored (the same failure mode as the 2850-line
    `sendDmx` spam this replaced);
  - ARP/ping liveness is a poor proxy — an ArtNet node can answer ARP and
    still not be listening on 6454.
  **Shape agreed with Branson (2026-08-25):** the rig is mostly **unicast**, so
  a per-target reachability check IS meaningful and is the main case worth
  building — for a unicast destination, "is that node there?" is a real,
  answerable question. For a **broadcast** target there is nothing to answer,
  so the check degrades to "is the interface/subnet present and up?".
  Critically: validate **directly-attached subnets only**. ArtNet nodes are
  normally on a directly-attached segment; anything routed should be left
  alone rather than guessed at, which also sidesteps the "pingable via the
  default gateway but not actually the show network" false-positive seen on
  ender (192.168.21.214 answered ping while being the wrong interface
  entirely).

  Still informational rather than a blocking NOT READY, and probably ArtPoll
  rather than ICMP — an ArtNet node can answer ARP and not be listening on
  6454.

  **Promoted off the back burner (2026-08-25, Branson):** warn the operator at
  *config* time when a patched unicast target is down, rather than only when
  someone is already chasing a dark universe. The scope is the same as above
  (unicast targets on directly-attached subnets; broadcast degrades to
  "interface up"), but the driver is now setup ergonomics, not debugging.

  **The constraint that makes this non-trivial — poll load must scale with the
  patch, not with the universe count.** This desk routinely patches 50+
  universes (the `ender` soak ran ~53 across 13 node addresses at
  `172.18.2.201`–`.230`). A naive per-universe probe would emit 50+ ArtPolls
  where 13 would do, on a segment already carrying ~2484 pkt/s of DMX — and
  the item above ("MasterTimer misses ticks at full output load") is an open
  question about *scheduling jitter* on exactly that path. Probe traffic that
  perturbs the thing being measured is worse than no probe. So the heuristics
  are load-bearing, not polish:
  - **Coalesce by destination IP, not by universe.** 13 nodes = 13 probes,
    regardless of how many universes each carries. Fan the result back out to
    every universe sharing that target.
  - **Probe off the MasterTimer thread**, and never inside a tick. This must
    not be able to cost a DMX frame.
  - **Config-time and on-demand, not a continuous poller.** On workspace load,
    on patch change, and on an explicit operator "check outputs" — not a
    background heartbeat. A permanent poll across 50 universes is precisely
    the "desk that cries wolf" / spam failure mode this whole area already
    burned itself on once (the 2850-line `sendDmx` flood).
  - **Backoff + hysteresis, shared with the ArtNet flapping item above.** A
    node that is down stays down between probes; don't re-report. The
    "don't announce recovery until it has held for N" rule wanted there is
    the same rule wanted here — build it once.
  - **Bounded concurrency + a short overall deadline**, so a rig with every
    node in the trailer still finishes the check promptly instead of hanging
    the load path on 13 timeouts.
  Open: whether the result surfaces as a per-universe column in the I/O map,
  the existing readiness indicator (`daeeb97d7`), or a footer chip. Probably
  the first — it is a per-patch-row fact.


- **App icon does not reach the runtime on Linux (OPEN, 2026-08-27)** — the
  asset is fine; the delivery is not. `resources/icons/png/qlcconsole.png` is a
  purpose-made fork icon (not the old QLC+ logo), `app.cpp:314` does
  `setWindowIcon(QIcon(":/qlcconsole.png"))`, and `ui/src/qlcui.qrc` aliases it
  correctly. What is wrong:
  1. **One size only.** A single **96×96** PNG is shipped, and it is installed
     to the legacy `share/pixmaps/` rather than the freedesktop icon theme
     (`share/icons/hicolor/<size>/apps/qlcconsole.png`) --
     `platforms/linux/CMakeLists.txt:9-13`. Modern shells prefer hicolor, and
     scaling one 96px source to 128/256 looks soft. `.desktop` says
     `Icon=qlcconsole`, which resolves through the theme first.
  2. **The SVG is never installed.** `resources/icons/svg/qlcconsole.svg`
     exists and would give every size for free via
     `share/icons/hicolor/scalable/apps/`.
  3. **Nothing is installed at all when running from `build/`**, which is how
     every test run works, so the desktop falls back to a generic icon. Only
     `_NET_WM_ICON` from `setWindowIcon` applies, and shells vary in whether
     they use it for the task list.
  4. **macOS dev builds are a bare binary**, not a `.app`, so
     `CFBundleIconFile=qlcconsole.icns` only takes effect in a packaged DMG --
     the icon is never seen during development.
  Fix: install the SVG to `hicolor/scalable/apps/` plus rendered PNGs at
  16/22/24/32/48/64/128/256 to `hicolor/<size>/apps/`, keep `share/pixmaps` for
  compatibility, and run `gtk-update-icon-cache` on install. The `.icns`
  (1024×1024) and `.ico` are already right for macOS/Windows packaging.

- **Art-Net node configuration (ArtAddress) — send path built and proven, but
  no node here applies it (2026-08-26)** — `artnet-config.py` at the repo root
  builds and sends ArtAddress (OpCode 0x6000); the plugin defines
  `ARTNET_ADDRESS` and has never used it. Phase 1 (read/browse) shipped; this
  is the write half.
  Packet correctness is established two ways: the CR041R **acknowledges** a
  short-name write by moving its NodeReport to `0006` (`RcShNameOk`), and OLA
  logs `ArtNet got unknown packet 6000`, i.e. it read the header and opcode and
  simply does not implement ArtAddress.
  **But nothing applies.** Confirmed across a real power cycle of the CR041R:
  the name came back `CR041R_001`, not the value written. This node acks and
  discards -- it is front-panel configured, and its Status1 port-address
  programming-authority bits never say "set by network". OLA has no ArtAddress
  support at all. **Validating that changes stick needs a node that implements
  network programming**; until then the write path stays out of the UI.
  Two protocol notes worth keeping:
  - The **NodeReport code is how a node acknowledges a config write** -- the
    advertised fields may not change even on success, so diffing them is not a
    test. Watch the code (`0006` RcShNameOk, `0007` RcLoNameOk).
  - The NodeReport **counter does not reset on reboot** on this node (8943 →
    31425 → 31733 across a confirmed restart), so it is useless as a power-cycle
    indicator. The report *code* is the reliable signal.
  Also: `NetSwitch`/`SubSwitch`/`SwIn`/`SwOut` are only acted on when bit 7 is
  high (program 7 as `0x87`); `0x00` means "reset to zero" and `0x7f` means
  "leave alone", so `0x7f` -- not `0x00` -- is the safe default for a field you
  do not intend to change.

- **RDM configuration tooling — DMX-Workshop-class rig setup (NEW, 2026-08-25,
  Branson)** — make the desk the thing you use to *set up* the rig, not just
  drive it: discover devices, read what they are, and set DMX address /
  personality / inverts from the console instead of climbing to every fixture's
  menu or carrying a laptop with DMX-Workshop on it.

  **This is an extension, not a green field — check what's already here first.**
  Upstream QLC+ shipped "Preliminary RDM support" (`9050a7f13`) and it is still
  wired in:
  - `plugins/interfaces/rdmprotocol.{h,cpp}` — a real E1.20 packetizer/parser
    with the standard PID table already defined (`PID_DEVICE_INFO`,
    `PID_DMX_START_ADDRESS`, `PID_DMX_PERSONALITY`(+`_DESCRIPTION`),
    `PID_IDENTIFY_DEVICE`, `PID_DEVICE_LABEL`, `PID_PAN_INVERT` /
    `PID_TILT_INVERT` / `PID_PAN_TILT_SWAP`, `PID_LAMP_HOURS`,
    `PID_SENSOR_VALUE`, `PID_SUPPORTED_PARAMETERS`, …). The protocol layer is
    NOT the missing piece.
  - `QLCIOPlugin::RDM` capability + `sendRDMCommand()` / `rdmValueChanged()`,
    implemented by **both** transports this rig actually uses: `artnet`
    (`ArtNetController::sendRDMCommand`) and `dmxusb` (Enttec DMX USB Pro).
  - `ui/src/rdmmanager.{h,cpp,ui}` (~900 lines) — a discovery worker thread +
    UID list + raw get/set-a-PID panel, reachable from Fixture Manager
    (`fixturemanager.cpp:457`).
  So the gap is **workflow, not plumbing**. Today it is a protocol inspector:
  it can talk to a device if you already know which PID you want. DMX-Workshop
  parity means turning it into a rig-setup tool.

  Wanted (roughly in value order — needs its own design doc + a decision on
  where it lives, since Fixture Manager is already crowded):
  1. **A device table, not a UID list** — one row per discovered device with
     manufacturer / model / label / current address / footprint /
     personality, populated by auto-`GET`ting the obvious PIDs on discovery
     rather than making the operator issue each one.
  2. **Editable address + personality in place**, with **overlap detection**
     across the discovered set (the actual reason people open DMX-Workshop:
     "who is sitting on top of whom"). Changing personality changes footprint,
     so re-check overlaps after a personality set.
  3. **Identify** as a first-class button per row (`PID_IDENTIFY_DEVICE`) —
     the "which physical unit is this?" loop, and the single most-used RDM
     feature on a call.
  4. **Reconcile against the patch** — match discovered devices to patched
     `Fixture`s and flag the three mismatches that ruin a focus session:
     patched-but-not-present, present-but-not-patched, and
     address-differs-from-patch. Offer "push patch → rig" and "pull rig →
     patch". This is where it stops being a generic RDM tool and becomes
     *this* console's, and it ties into `RIG_TEST_PLAN.md` /
     `SHOW_LIFECYCLE_DESIGN.md`'s Test-Validate phase.
  5. **Sensors + lamp hours** (`PID_SENSOR_VALUE`, `PID_LAMP_HOURS`,
     `PID_DEVICE_HOURS`) — maintenance readout; low priority, easy once (1)
     exists.

  **Hardware finding (2026-08-25) — the ArtNet node on hand does NOT answer
  RDM, so this is currently BLOCKED on hardware.** Probed `172.18.2.10`
  directly from the show VLAN (this Mac is on `vlan0` as `172.18.2.17`):
  - Node identifies as **`CR041R_001`** / `CR041R` — a **4-port** ArtNet→DMX
    gateway, ArtNet 3, `net/sub 0/0`, `swOut 0,1,2,3`, OEM `0x0022`,
    ESTA `0x707A`.
  - It answers **`ArtPoll` → `ArtPollReply`** immediately and cleanly
    (broadcast to `.255:6454`, not unicast back to the requester — relevant to
    the endpoint-reachability item above if that ends up ArtPoll-based).
  - It **ignores `ArtTodRequest`**: four sent (universes 0-3), full wire
    capture of everything from that host, **zero `ArtTodData`** back. Its own
    `goodOut` bits claim RDM is *not* disabled (bit 3 clear on all 4 ports),
    i.e. it advertises a capability it does not deliver.
  - The request packet was checked field-by-field against the Art-Net 4 spec
    before concluding (56 bytes; Net@21, Command=TodFull@22, AddCount@23,
    Address@24) — the silence is the node's, not a malformed probe.
  - **First probe was invalid — corrected same day.** The initial run used
    opcode `0x8080` for ArtTodRequest; the correct value is **`0x8000`**
    (`plugins/artnet/src/artnetpacketizer.h:36`). The node was right to ignore
    an undefined opcode. Re-probed with the correct opcode **and** an
    `ArtTodControl`/**AtcFlush** (`0x8200`, command 1) first — AtcFlush is the
    step that forces a node to actually *re-run* discovery, where ArtTodRequest
    only asks for the TOD it already holds. Result unchanged: 16 `ArtPollReply`,
    **zero `ArtTodData`**. The conclusion stands, but only after the correct
    test. Note QLC+'s own `sendRDMCommand` sends ArtTodRequest with **no
    preceding AtcFlush** — worth revisiting if a real node ever returns a
    stale/empty TOD.
  - **CONFIRMED by DMX-Workshop (2026-08-26).** Artistic Licence's own tool --
    the Art-Net reference implementation -- connects to the node fine and
    reports outright: **"RDM: Node is not RDM capable (Unidirectional DMX)"**,
    with `Rdm Devices: 0 Detected of which 0 active` on all four outputs
    (firmware V0.14, MAC 02:4D:48:12:02:0A, static, LLRP not supported).
    So the probe's verdict was right, independently confirmed.
    The reason is **hardware, not configuration**: RDM needs bidirectional
    RS-485 (talk, then turn the line around to listen), and this node only
    transmits. No firmware setting or update can add it.
    **Correction (2026-08-26): the node was never lying; the wrong bit was
    being read.** GoodOutput bit 3 means "RDM is *disabled* on this port" -- a
    per-port setting, meaningless on a node with no RDM at all. The capability
    flag is **Status1 bit 1**, and across 80 sampled ArtPollReplies it reads 0
    ("not RDM capable") every time. The node reports itself correctly.
    Separately, its Status1 does flap between `0x00` and `0x74` (ROM-booted +
    port-address programming authority + indicator state) roughly 60/40, which
    is almost certainly why DMX-Workshop's displayed lines appear to change on
    their own -- but bit 1 stays 0 throughout, so the RDM verdict never
    actually flips.
    For capability, read Status1 bit 1; GoodOutput bit 3 answers a different
    question. Still worth confirming with a TOD request, since capability bits
    describe intent and a TOD reply is evidence.
    Consequence: RDM must come via the **DMXKing/Enttec Pro USB dongle**
    (wired, bidirectional; `EnttecDMXUSBPro::sendRDMCommand` already exists) or
    via **OLA** acting as an Art-Net→RDM gateway. The CR041R stays a pure
    output path.
    Worth copying from DMX-Workshop's UI: it presents node config as a tree and
    reports RDM device counts **per DMX output**, not per node -- matching the
    per-port finding above.
  - **Control case found, which resolves the earlier ambiguity.** The Pi 5
    build host (`192.168.20.119`) runs an **OLA Art-Net node**, and OLA answers
    the identical probe with `ArtTodData rdmVer=0x01 uidTotal=0 uidCount=0` —
    i.e. a node with an *empty* TOD and nothing attached still **replies**. So
    "silent" is not how an empty TOD presents. The CR041R's total silence is
    therefore best explained by it **not implementing Art-Net RDM at all**, and
    that no longer needs an RDM fixture to establish.
  - **Consequence:** an RDM-capable fixture on a CR041R DMX port will not be
    discoverable *over Art-Net* regardless of the fixture's own RDM support —
    the gateway will not relay it. (Branson has an **ADJ 3Z** on the rig and is
    ordering a known-RDM fixture, 2026-08-25.) Viable transports are therefore
    the **Enttec DMX USB Pro** (wired), or **OLA** as an Art-Net→RDM gateway.
  - **OLA is a zero-hardware development target.** It implements Art-Net RDM
    correctly, and OLA's dummy/simulated-RDM devices would let the entire
    device-table / reconcile workflow be built and tested with no rig at all —
    which would move this item off the ✈️-blocked list. Worth confirming
    before assuming this feature needs hardware.
  - **SUSPICION, not yet proven — QLC+'s `ArtTodRequest` is 25 bytes; the
    Art-Net spec's is 56.** `ArtNetPacketizer::setupArtNetTodRequest`
    (`artnetpacketizer.cpp:161`) builds a 12-byte common header + 9 filler/spare
    + Net + Command + AddCount + **one** Address byte = 25 bytes. The spec
    defines `Address` as a fixed `[32]` array, making the packet 56 bytes.
    Permissive nodes will not care; a strict one would drop it — which is
    exactly the shape of bug that makes RDM "mysteriously not work" against
    real hardware while the code reads fine.
    **NOT verified.** The comparison was attempted against the OLA node on the
    Pi and was inconclusive: OLA answered the 56-byte probe earlier in the day
    but answered neither length while the Pi was under a parallel build
    (load 4.25), so the negative result says more about OLA being starved than
    about packet length. **The test to run**, on an idle box with a known-good
    RDM node: send the byte-exact 25-byte QLC+ layout and the 56-byte spec
    layout to the same node and compare `ArtTodData` replies. Do this before
    concluding anything about QLC+'s RDM working or not working.
  - Develop against **`rdm-sim.py`** (repo root) when no node is available —
    it answers ArtPoll / ArtTodRequest / ArtTodControl with a synthetic TOD and
    ArtRdm GET for the device-table PIDs. Verified working end to end against
    `artnet-probe.py` (3 simulated UIDs). Note it is deliberately *permissive*
    about request length, so it cannot settle the 25-vs-56 question above.
  - Reproduce any of this with **`artnet-probe.py`** at the repo root.

  **Paradigm answers (2026-08-25 discussion) — no new U→node binding needed.**
  `ArtNetController::sendRDMCommand` (`artnetcontroller.cpp:374`) already looks
  up `m_universeMap[universe]` and sends the TOD request to that universe's
  patched `info.outputAddress` / `info.outputUniverse`. **RDM rides the
  existing output patch**; Inputs/Outputs is already the mapping UI. Likewise
  `handleArtNetTodData` (`:515`) → `rdmValueChanged` → `RDMManager` already
  carries replies back, so "self-populate what it finds" is plumbed — the gap
  is auto-`GET`ting the descriptive PIDs per UID to fill a table.
  - Nuance: the binding is **U → (node IP, node *port*)**, not U → node. The
    CR041R is one IP carrying four universes, so discovery is **per-port** —
    a 4-port node needs 4 TOD requests. Note this is the *opposite* of the
    coalesce-by-IP rule in the endpoint-reachability item above (13 IPs, not
    53 universes): reachability coalesces, RDM cannot. Don't share that code
    path by accident.

  **Reconcile semantics — only one of four cases is an error:**
  | Discovered | Patched | Verdict |
  |---|---|---|
  | yes | yes, same address | matched |
  | yes | nothing at that address | **not an error** — offer to patch it |
  | no | yes | **must NOT be an error by default** |
  | yes | yes, *different* address | the real, actionable conflict |
  Row 3 is the trap: most conventionals and much LED gear have no RDM at all,
  so "patched but not discovered" is the *normal* case on a mixed rig. Flagging
  it would cry wolf on first use — the same failure mode as the 2850-line
  `sendDmx` flood.

  **Blocking design decision — `Fixture` must persist an RDM UID.** Confirmed
  `engine/src/fixture.h` has no `uid`/`rdm` field today. Without a stored
  UID↔Fixture binding, reconciliation can only match on address, which is
  circular: "this fixture moved address" is indistinguishable from "a different
  fixture is now at this address". Row 4 above is unbuildable until this is
  settled. It is a `.qxw` schema addition on `Fixture` — decide before writing
  any UI. Also: there are **no RDM tests anywhere in the tree**.

  **Cautions.** RDM is in-band with DMX on a wired line, so discovery
  interleaves with output — do not run it from the MasterTimer thread, and
  expect it to be visibly disruptive on a live wired universe (fine in
  Construction phase, dangerous mid-show; probably gate it the same way Blind
  is gated). ArtNet RDM is a different animal from wired RDM and node support
  is uneven, so verify per-node rather than assuming. Discovery is a binary
  search over the UID space and is *slow* — the existing worker is already a
  `QThread` for that reason; keep it cancellable. **Needs the rig** to be
  worth anything: none of this is verifiable offscreen beyond parser unit
  tests, and there are no RDM tests today.

- **Simple Desk sliders have no write-back-to-scene path (PARKED, 2026-08-12)**
  — found while triaging the old `live-edit-4.x` branch: Virtual Console
  sliders/XY-pads already write manual adjustments back into the running
  Scene via `CaptureManager::recordOverride()` (wired from `vcslider.cpp`/
  `vcxypadfixture.cpp`, with the existing capture/undo/diff/store workflow —
  see `LiveCaptureDialog`), but `SimpleDesk` (`ui/src/simpledesk.cpp`) has
  zero `CaptureManager` references — moving a Simple Desk fader doesn't
  persist anywhere. Better path is wiring Simple Desk's
  `slotUniverseSliderValueChanged` into `CaptureManager` rather than
  reviving `live-edit-4.x`'s standalone `LiveEditManager` (undo-less
  direct-write) — confirmed `CaptureManager` isn't just equal plumbing,
  it's strictly *better* than that reference implementation:
  - `LiveEditManager::findMostSignificantScene()` (the "which running Scene
    actually wins when several touch this channel" question) was left as an
    explicit `// TODO: Implement HTP/LTP priority logic` stub —
    `return scenes.first();`, i.e. never solved. `CaptureManager::buildPlan()`
    already solves exactly this: it walks running functions in **start
    order**, so the most-recently-started Scene wins LTP per (fxi, channel) —
    a real, reasoned answer, inherited for free.
  - `CaptureManager::buildPlan()` also already flags **chaser-driven
    channels** (a channel currently under a running Chaser step's Scene) and
    excludes them from capture by default — `LiveEditManager`'s plain
    fader-walk has no equivalent, so a Simple Desk tweak there could point-edit
    a Scene the Chaser immediately overwrites on its next step. Preserve this
    exclusion when wiring Simple Desk in.
  - One piece of `live-edit-4.x` *is* directly reusable regardless: unlike VC
    widgets (pre-bound to a specific function), a Simple Desk slider only
    knows an absolute `(universe, address)` — `LiveEditManager::
    findScenesForChannel()`'s address -> `(fixtureId, channel)` resolution
    via `Doc::fixtureForAddress()` is the correct, already-written way to get
    from "which slider moved" to the `(fxi, channel)` pair
    `CaptureManager::recordOverride()` actually wants.
  - Net effect: Simple Desk gains MORE than `live-edit-4.x` ever had (undo,
    conflict/chaser-driven visibility, save-as-new) for less new code than
    reviving `LiveEditManager` would take.
  Needs its own design/implementation pass, not a quick cherry-pick.

- **Cue transition model — hold-on-miss + release-on-transition (4b; DEFERRED,
  needs rig)** — DECIDED framing: a *missed/skipped* cue is a non-event → hold
  last look (no blackout); a cue that *fires* releases what it replaces (outgoing
  look + its effects fade out) → no dangling. Split confirmed: **fade intensity,
  hold position/colour**. Risky part = the intensity-latch core-mixer work; needs
  a live rig to test, so deferred until Branson has rig time. First verify whether
  the current timeline transition already releases the outgoing look or holds it.
- **Pre-positioning / mark cues + ghost visual + dangle detector** — how big
  consoles do move-in-black. See `MOVEINBLACK_DESIGN.md`. Slices **1 + 2 SHIPPED**
  (all rig/visualiser-verify):
  - **1** — `MarkEffect` DMXSource (holds non-intensity, auto-releases on reveal,
    `<Mark>` XML) + Mark/Unmark buttons + dashed-violet monitor ghost.
  - **2a** — `CueOutput`: offline "what would this cue output" (unit-tested).
  - **2b** — `CueLookahead`: next cue + lead time for Chaser & Show.
  - **2c** — `MarkPlanner` + **Auto MIB** toolbar toggle: look-ahead → pre-set
    dark→lit→moving movers, dark-gap gated.
  - **2 persist** — Auto-MIB toggle + dark-gap round-trip (`<MoveInBlack>`) + a
    "s lead" toolbar spinbox. DONE.
  - **3 — dangle detector** — `MarkPlanner::dangleFixtures()` + `dangleFixturesChanged`
    signal, forwarded `ProgrammerController` → App footer chip. DONE (unit-tested,
    engine/test/markplanner). Runs regardless of Auto-MIB being on (manual marks
    dangle too). *Needs rig eyeball to confirm the chip reads right live.*
  Still open: **verify CueLookahead timing on a rig** (the dark-gap depends on it);
  **force-live / force-mark** per-cue overrides + dark-move fade. Also: mark "to a
  chosen look"; a monitor context-menu Mark action. *(from the cue-policy discussion)*

## Build-season freeze list *(LOCKED with Branson — ship before feature freeze)*

- 🔴 **Rig test pass** — run `RIG_TEST_PLAN.md`, fix any ❌ (the gate).
- 🔴 ~~Auto-MIB persist + dark-gap setting~~ **DONE** (b38795658).
- 🔴 **PMJ (OpenDeck) mapping** — the operating surface for the show (free knobs =
  the relative encoders already supported). APC40 map de-scoped. *Best built with
  the PMJ in hand (LED feedback / knob verify).*
- 🟡 ~~Power/circuits → footer bar~~ **DONE** (a9aab77db) — ⚡ status-bar chip.
- 🟡 ~~Native single-shot~~ **DONE — became the effect-lifecycle work** (4c7c9621c,
  `EFFECT_LIFECYCLE_DESIGN.md`): effects declare loop/reactive/**oneshot**;
  one-shots run on `inputs.phase`, duration resolves Look→cue/chase→default,
  hold/release on finish; Wand example + per-look length UI. *Follow-ups (post-
  freeze OK): span on the Show timeline (falls back to naturalDuration today);
  fold RGBScript `Once` into the lifecycle; per-look syncTo/onFinish UI.*
- 🟡 ~~Small polish batch~~ **DONE** — drop-onto-specific-effect nesting (5e-ish),
  End-at-SMPTE (751ce1b5a); MTC-chip glyph was already in place.
- ⚪ ~~dangle detector~~ **DONE** (2026-08-12) — see slice 3 above.
- ⚪ Post-freeze: cue-transition 4b; "Look" as assembly unit;
  unified object editor; more stage objects; rebrand to qlcconsole.
- **Effects respect the look's master Dimmer — SHIPPED** (563c4b3b1): colour
  output scales by the look's Dimmer on dimmerless fixtures; dimmered fixtures
  carry it on the master channel. Move to DONE.md next pass.
- **Move circuits / power usage to the footer bar** — the power/amperage &
  circuit load estimator currently lives in the Programming space; move it to the
  footer bar (like the timecode/load chips) so it's an always-visible status
  readout and reclaims programming canvas. *(Branson request)* See memory: power
  estimation feature.
- **New control surface — integrate (TBD)** — a new hardware control surface to
  bring in (details TBD). Fold into the MIDI-mapping work below once specced.
- **Control-surface engine (PMJ + APC40 mk2 + Xbox) — IN PROGRESS, at the rig.**
  See `CONTROL_SURFACE_DESIGN.md`. Device-agnostic engine: surface model +
  role/page vocabulary + context-aware LED loop; boards are overlays. **P0
  CORE — DONE** (81eaab311, unit-tested `engine/test/controlsurface/`).
  Decisions locked: static core = **GM, Blackout, Blind, Tap, Go, Back**
  (identify more via workflow); **faders follow the page** (optional
  submaster page). All 4 original P1-blocking board facts are resolved
  (encoder CCs 11-14, LED velocity/steady-level scale, output channel 9,
  static-core placement `O`→Blackout/`Set`→Blind). **P1 PMJ overlay + LED is
  underway** (`ui/src/pmjoverlay.{h,cpp}`, slices 1-6 in "Recently shipped"
  above) — LEDs light only for wired controls, Master fader → Grand Master,
  `O`/`Set` → Blackout/Blind, and Enc 1/2 now nudge a focused PanTilt
  palette's XY pad live, direction-confirmed on the real board. **Still
  open in P1**: Select/Load/Transport wiring (needs the app-side
  selection/paging model — not built yet), Favorites/Tap binding (only a
  Programming-tab-local tap-tempo exists today, not the global static-core
  action the design doc means), per-strip fader Level(1-10) semantics
  (submaster vs per-fixture — open design question for Branson), Enc 3/4
  targeting (colour/beam — needs the same selection model as Select/Load),
  and the `Set` button's LED specifically (toggles correctly but doesn't
  light — likely a hardware button/LED address pairing issue, try
  `qlc-midi opendeck identify`/`align`). Then: P2 runtime page → P3 APC40
  mk2 overlay → P4 Xbox roles. Plumbing found: buttons are MIDI notes
  (offset-128; LED note == button note on ch9); faders/enc-push are CC
  (offset<128); LED via `InputOutputMap::sendFeedBack`→`feedbackToMidi`.
  Relative encoders already built.
- **Timecode slice 3 — auto-fill internal latency** — the packet→DMX figure needs
  plugin-side timestamping; once measured it folds into the offset. *(from Timecode
  calibration in DONE.md)*
- **Audio calibration — active loopback self-test** — play a click on the audio
  output, time the round-trip to detection = true audio latency. Needs an audio-emit
  path (AudioRenderer wants a decoder) and is unverifiable offscreen; the
  editable/seeded detection-latency + ±10 ms nudge cover it meanwhile.
- **GUI headful automation** — `screencapture` + `cliclick` driver so Claude can
  drive AND validate real UI (moving this to the spare machine). First task there:
  a `gui-drive.sh` wrapper, then drive the end-handle drag with eyes on real pixels.
- **Clickable Load chip → per-function breakdown (2026-08-12 idea)** — today
  `MasterTimer` only times the whole tick as one number (`engine/src/
  mastertimer.cpp`, the `computeTimer` around `timerTickFunctions()` +
  `timerTickDMXSources()`); no per-Function/per-DMXSource granularity exists.
  Would need wrapping each `function->write()` call in the tick loop
  individually, accumulating per-function-id compute time, and exposing a
  top-N query — real engine instrumentation, not a UI-only change. The
  existing single-number Load chip itself is already cheap (a single
  `QElapsedTimer` read per tick, always-on regardless of the chip; the footer
  just polls an atomic every 500ms) — no separate background load meter
  needed for that part.

---

## Done — real "Import" for .qxw, distinct from Open *(2026-08-12, BUILT)*

File > Import: merge fixtures/fixture groups/functions from a second .qxw
into the CURRENTLY OPEN document, unlike Open (which replaces everything).
Resolved the open design questions from this item's original design-first
note:
- **Scope**: fixtures, fixture groups, AND functions/shows — not fixtures-only.
  Picking a function auto-expands to its full dependency closure (member
  functions, the fixtures/groups they touch) via new `Doc::functionFunctions()`
  (mirrors the existing `Doc::functionFixtures()`) — no manual dependency
  picking needed.
- **ID collisions**: real remap-on-import, not a blind merge. Three separate
  ID spaces (fixture/function/fixture-group) each keep the source ID when
  it's free in the target, else get a fresh one; every cross-reference
  (`SceneValue::fxi`, `ChaserStep::fid`, `RGBMatrix`'s fixture group,
  `FixtureGroup` head assignments, `Show`/`Track` function refs) gets
  rewritten afterward to match. Also handles a DMX-address-space collision
  (a fixture ID can be free while its address range still overlaps something
  already patched) by relocating within the same universe, reported
  separately from ID remaps.
- **UI**: dedicated `ImportSelectionDialog` (browse + pick), not drag-and-drop
  of a file — matches the existing `FixtureSelection`/`FunctionSelection`
  picker convention rather than inventing a new one.
- New engine module `engine/src/qxwimporter.{h,cpp}` (`QxwImporter::import()`)
  does the actual closure/remap/clone/rewrite work, engine-side and
  UI-independent; loads the source file into a throwaway scratch `Doc`
  (`App::loadScratchDoc()`) rather than ever touching the live one mid-merge.
- Verified end-to-end against real workspace files (not just code review):
  clean imports, heavy-collision imports (96/96 IDs correctly remapped, exact
  fixture/group/function count arithmetic), and an adversarial dense-universe
  case (correctly reports "no free DMX address" per fixture rather than
  corrupting anything) — all with zero dangling fixture references across
  every function in the target doc afterward.
- Caught and avoided reusing `Function::createCopy()`/`copyFrom()` for this —
  `Show::copyFrom()` has a latent same-doc assumption (looks up member
  functions via the *target* doc, not the source) that would have silently
  dropped every child function of an imported Show. Cloning goes through each
  object's own `saveXML()`/`loadXML()` round-trip instead (doc-agnostic,
  same proven code path real file Open/Save already uses).
- Not rewritten: function/fixture IDs referenced as literal numbers inside
  Script text (can't safely parse arbitrary JS for this) — scripts import
  verbatim. Audio/Video functions' referenced media files don't travel with
  the import (same as moving a workspace to another machine today).

---

## Done — rebrand the fork to "qlcconsole" *(all 3 phases shipped)*

The fork is now firmly a **desktop console** (mouse+keyboard, MIDI, multi-window),
well past the tablet/Android QML flavour — rebrand from QLC+ to **qlcconsole**.
Distinct from the *Lighting Studio* rename (that was just the 2D tool; shipped).
Keep upstream attribution/license — this is a fork identity, not a takeover.

Phase 1 (2026-08-10, commit 7bf4f6daa) — display strings: window/About titles,
log filename, `.qlcc` workspace extension (`.qxw` still read/imported).

Phase 2 (2026-08-10) — build/binary names: top-level CMake project name and
`CPACK_PACKAGE_NAME` → `qlcconsole`; executable targets `qlcplus` →
`qlcconsole`, `qlcplus-launcher` → `qlcconsole-launcher`,
`qlcplus-fixtureeditor` → `qlcconsole-fixtureeditor` (main/CMakeLists.txt,
launcher/CMakeLists.txt, fixtureeditor/CMakeLists.txt, launcher.cpp's
hardcoded spawn paths, macOS Info.plist CFBundleExecutable, Linux .desktop
Exec= lines, CLAUDE.md/RIG_TEST_PLAN.md/testing_st.md run commands).

Phase 3 (2026-08-12, discovered already-shipped via commit 22fde51d1 + this
pass) — icon/asset filenames (`qlcconsole.icns`, `qlcconsole-fixtureeditor.icns`
etc.) and the macOS `CFBundleIdentifier` (`com.bransonmatheson.qlcconsole`)
were already renamed; only `resources/doxygen/qlcplus.dox` (a docs-generation
config, not app-facing) was still qlcplus-named — renamed to
`qlcconsole.dox` + updated its `PROJECT_NAME` and the `CMakeLists.txt`
doxygen target reference.

UI polish pass (2026-08-11) — main toolbar + global actions (Blackout/Blind/
Operate) + bottom tab bar: repadded/gloss-rendered the 4 mismatched action
icons to match the classic Crystal/Oxygen set's shading (`resources/icons/png/
blackout,blind,operate,design.png`), unified toolbar+tab-bar icon size to
24x24 (`App::initToolBar()`), added a bundled default chrome QSS
(`resources/qss/default.qss`, cascades under the existing user
`~/.qlcconsole/qlcplusStyle.qss` override via `AppUtil::getStyleSheet`).
Deliberately deferred: per-manager toolbar icon-size unification (Virtual
Console 26px / I-O Manager 32px / Show timeline 20px / Monitor 16px all stay
as-is), no dark/light theme switcher *(superseded — see backstage color
themes below, 2026-08-11)*, no SVG-icon migration, Fixture/Function
Manager's own toolbars and the Programming tab's button styling untouched.

Backstage color themes (2026-08-11) — View menu > Theme: Default/Tan/Blue,
picked from `App::Theme` (`ui/src/app.h`), persisted via `workspace/theme`
(`QSettings`), applied live by `App::applyTheme()` (`ui/src/app.cpp`). Whole-
app-surface theming via a `QPalette` swap (`qApp->setPalette()`) rather than
hardcoded per-theme stylesheets — pays off the earlier chrome QSS work
directly, since `resources/qss/default.qss` already reads colors via
`palette(...)` functions, so it follows any theme with zero further changes.
Extended `default.qss` slightly (QGroupBox/QMenuBar/QMenu, still
palette()-only) so more chrome reaches along. Verified all 3 themes
end-to-end via the offscreen snapshot harness (real screenshots, not just
code review) — Default exactly restores the pre-theme look, Tan/Blue both
tint the full window (toolbar, tabs, tables, panels) convincingly. Known
platform caveat, not a bug: this app doesn't force the Fusion widget style
app-wide (only a few `ConsoleChannel` sub-widgets use it), so a handful of
plain native-style buttons/menus elsewhere may follow the palette less
completely than content areas do — full uniform recoloring would mean
switching the app's global QStyle to Fusion, a much bigger, separate
look-and-feel decision, not done here.

Window couldn't be resized smaller / again (2026-08-11) — regression report
led to finding `QTabWidget`/`QStackedWidget` compute minimum size as the max
over ALL pages, not just the visible one, so any one oversized page pins the
whole window (or sub-widget)'s minimum regardless of what's showing. Three
real instances fixed with the same `QSizePolicy::Ignored` + explicit-floor
pattern: `SimpleDesk::initSliderView()`'s unwrapped 32-slider row
(simpledesk.cpp), `LookEditor`'s internal `QStackedWidget` where the pan/tilt
page's 200x200 XY pad bloated every other page (lookeditor.cpp), and
`ProgrammingManager`'s per-fixture console scroll area (proactive — same
pattern, was hidden by default so not yet visibly triggered). Window minimum
width dropped 1296px -> 942px. Not yet investigated: Simple Desk's "Cue
Stack" tab (938px, now the binding constraint) looks like legitimate
toolbar+cue-list content rather than a bug — stopped there per Branson's call.

Per-manager toolbar text/icon label mode (2026-08-11) — the "Icons only /
Text only / Icon+text" workspace setting (`App::applyTabLabelMode()`) only
ever touched the main tab bar and the main window's own toolbar (Panic/
Blackout/Blind/Operate); every per-manager toolbar (Function Manager, Fixture
Manager, Input/Output Manager, Virtual Console) was hardcoded to Qt's
icon-only default, deaf to the setting. Added a `applyToolbarLabelMode()`
method to each (mirrors the existing `Monitor::applyToolbarLabelMode()`
pattern — reads `workspace/tabLabelMode` directly via QSettings), called from
`App::applyTabLabelMode()`. Simple Desk has no comparable QToolBar (its view
controls are standalone QToolButtons), so it's not part of this.

Release readiness (2026-08-11) — v0.1.0 prep landed: `VERSION` file +
`CHANGELOG.md` (SemVer release tags, independent of the git-derived
`APPVERSION` build string); `README.md`/`CONTRIBUTING.md`/`SUPPORT.md`
rewritten with correct qlcconsole branding, fork-of-QLC+ attribution, and
links back to this repo instead of upstream; `RELEASE.md` + `release.sh`
wiring up the existing local build/sign/notarize scripts
(`platforms/macos/package-local.sh`, `sign-notarize.sh`) to tag + publish a
signed macOS DMG as a GitHub Release. Still open before actually cutting
v0.1.0: write the first real `CHANGELOG.md` entry against what's shipped by
then, and run `release.sh` for real.

---

## Now — unify the object editor's fixture management *(design-first; options pending)*

Follow-on to the embedded object-editor canvas (Lighting Studio). Combine the
fixture-properties "edit" dialog (double-click a fixture) and the "Edit Fixtures in
Studio" layout window into ONE object editor. Wants: a left tree of fixtures BY
FIXTURE GROUP as assigned to this object; right-click to add fixtures/groups;
multi-select → create a fixture group (opens the Fixtures-tab head-layout mapping);
drag-drop tree→face to add; distribute/put-on-face via popup or right-click.
**Design options being drafted — pick a direction before building.**

---

---

## Now — more stage-feature objects *(active; design-first)*

Extend the discrete map-object model (truss / platform / target / power source /
image / studio group) with the common rigging structures below. Shared needs:
placeable, movable, lockable, layerable/groupable, XML round-trip, a fixture-host
role (fixtures mount to them like they do to a truss), and a 2D + derived-3D
representation. Likely a small **StageStructure** base + parametric shapes, reusing
the truss geometry funnel (`fixtureRigPosition`) and the bar-on-truss / studio-group
patterns. Write a short design doc first (à la FIXTURESTUDIO_DESIGN.md).

SHIPPED (2026-07-31, see DONE.md) — the unified **Pipe** + **Stand** + **Tower**
object set, all fixture-hosting, 2D + elevation, editors, XML:
- [x] **Boom** = a vertical **Pipe** (on a stand, hung from a truss, or free);
  fixtures mount up the pipe at height + facing angle.
- [x] **House electric** = a horizontal **Pipe** (same object, orientation flag +
  run angle). Booms/electrics are ONE object now (resolved the bar/boom/pipe
  overlap).
- [x] **Stand** = a distinct placeable base; a pipe stands on it (base derived,
  follows the stand). (Trusses/towers on stands = future.)
- [x] **Trusses with booms** — a pipe parented to a truss (drop-arm), base derived.
- [x] **Tower** (16" sq × 8') with **shelves** at heights; fixtures mount on a
  shelf (towerU/V), derived from the tower.
- Demo: `test-workspaces/stage-structures-demo.qxw` (one of each + a fixture on the tower).

Still open (lower priority — non-fixture-hosting scenery): flats, drapes/legs,
set pieces. And: a Stand should also support a truss/tower (only pipes today);
a Tower editor "Cancel" still applies (live-edit); horizontal-pipe fixtures at
per-fixture offsets along the run.

---

## Backlog — not started

### App-wide UI zoom, Cmd+/Cmd- (2026-09-03, long-term, not prioritized) — not started
Branson: VS Code-style zoom — Cmd+/Cmd- (and presumably Cmd+0 to reset)
scaling the whole app's fonts *and* icon sizes together, not just one view.
Explicitly deferred — filed for later, not asked for now.

Checked before filing, so the scope is accurate rather than guessed: **no
zoom infrastructure exists app-wide today.** Two views already have their
*own*, local zoom (Show Manager's timeline — a slider + Ctrl+scroll driving
a `timeScale` variable, `ui/src/showmanager/multitrackview.cpp`; Lighting
Studio's 2D view — Shift+scroll + trackpad pinch,
`ui/src/monitor/monitorgraphicsview.cpp`), but neither is wired to Cmd+/
Cmd-, and neither is what "app-wide" means here — a real implementation is
a different, bigger thing than extending either of those.

**Why this is more than "add a keyboard shortcut + scale the font,"
concretely:** `QApplication::setFont()` cascades point size to most widgets
via font-metric-based sizing, which covers a lot for free — but toolbar/
tree icon sizes are hardcoded pixel values scattered across many call
sites (`toolbar->setIconSize(QSize(20, 20))` appears independently in
Fixture Manager, Connections, and other manager toolbars — see this
session's own work touching several of them), not derived from font size
at all, so they would not scale along with text unless each site is
touched (or icon sizing is centralized first — its own small refactor)
and multiplied by the same zoom factor. `resources/qss/default.qss` also
has fixed-pixel padding/margin values that a pure font-size change
wouldn't touch, so some chrome would look increasingly cramped or loose
relative to the scaled text as the zoom level moves away from 100%.

Not scoped further than this — no design doc yet, no decision on whether
to centralize icon-size constants first or handle it site-by-site, no
persisted-setting design (a `workspace/uiZoom` `QSettings` key alongside
the existing theme/tab-label-mode pattern would be the obvious fit). Pick
up with a real audit of every `setIconSize()`/fixed-pixel QSS rule before
estimating effort for real.

### Show lifecycle: Construction / Test-Validate / Production *(2026-08-15, design doc written)*
See `SHOW_LIFECYCLE_DESIGN.md` — names the three phases a show moves through
(building off-rig → matching Lighting Studio to the real rig → running on the
road) and settles that this is **not** a third `Doc::mode()`; it's the
existing Design/Operate axis crossed with a "connected to real hardware vs.
fully simulated" axis (real, non-Dummy plugin patched **and** not Blind —
Blind turned out to already BE the "disable real output" switch: it silences
every protocol at once and is already Design-only/force-off-in-Operate, see
the doc's 2026-08-15 finding). Concrete follow-ons, none started:
1. Compute + expose the rolled-up connected/simulated state
   (patched-and-not-Blind).
2. Surface it as a footer chip alongside MTC/Load/Power.
3. Decide + build whether Design mode should auto-engage Blind when it
   detects real hardware patched, rather than leaving it opt-in.
4. Turn `RIG_TEST_PLAN.md`'s manual checklist into an actual in-app
   Test/Validate workflow (bigger; own design pass on where it lives).
5. Retroactively tag the rest of this backlog by which phase it serves, to
   sharpen prioritization instead of treating it as one flat list.

### ~~Tiny: mark the MTC-chip section label as show-sourced~~ **DONE** (f91c91992, 2026-07-29)
Already shipped — `App::slotTimecodeStatusChanged` prefixes the section label with
`▸` so it reads as show-content, not as text arriving from the MTC stream. This
entry was just stale; confirmed via `git log` (2026-08-19) and removed.
