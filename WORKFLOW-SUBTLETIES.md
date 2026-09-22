# Workflow subtleties — a review list

Found while building end-to-end integration tests that drive real UI code
paths (real dialogs, real `QMouseEvent` press/move/release sequences, real
drag-drop) instead of calling the model API directly, and check exact
expected results rather than "something changed". Each test lives in
`ui/test/monitor/monitor_test.cpp` unless noted; each entry below is
something the test work surfaced that seemed worth a second look — a design
decision that may be intentional but is easy to trip over, a naming/behavior
mismatch, or a rough edge in how two features interact.

**How to use this:** each entry gets a status once reviewed —
**Accept** (working as intended, no change), **Fix** (real bug/rough edge,
worth a follow-up), or **Note** (informational, not actionable). Unresolved
entries stay unmarked.

## Index

| # | Entry | Domain |
|---|---|---|
| # | Entry | Domain | Status |
|---|---|---|---|
| [1](#1-a-horizontal-trusss-default-crossbar-is-visually-coincident-with-the-truss-itself) | Horizontal truss's default bar is visually coincident with the truss | Truss/bar | Fixed |
| [2](#2-a-plain-drag-drop-attach-never-forms-a-group--only-the-explicit-attach-to-truss-action-or-a-file-reload-does) | Plain drag-drop attach never forms a group | Truss/bar | Fixed |
| [3](#3-mousepresseventss-hit-test-always-prefers-a-fixture-over-a-bar) | Fixture hit-test always wins over a bar underneath it | Truss/bar | |
| [4](#4-structurestudioviews-bar-drag-arm-is-gated-by-the-views-own-m_locked-separately-from-the-bar-objects-own-locked-flag) | View lock vs. object lock are two different checks | Truss/bar | |
| [5](#5-addtargetaccepteds-own-comment-says-pantilt-palette-the-code-links-an-aim-palette) | Stale comment: says PanTilt, code links Aim | Target/aim | Fixed |
| [6](#6-assigning-a-fixture-to-aim-at-a-target-has-no-test-for-the-real-drag-drop-path) | No test for the real palette-onto-Look drag | Target/aim | |
| [7](#7-a-failed-qverifyqcompare-inside-the-dialog-driving-lambda-hangs-the-whole-test-binary-not-just-that-one-test) | A failed assertion inside a dialog lambda hangs the binary | Test technique (all dialogs) | Fixed |
| [8](#8-findchildqlineedit-can-silently-grab-a-spin-box-own-internal-editor) | `findChild<QLineEdit*>()` can grab a spin box's own editor | Test technique (all dialogs) | |
| [9](#9-deck-mounted-and-riser-mounted-fixtures-follow-a-moved-platform-through-two-different-mechanisms) | Deck vs. riser fixtures follow a moved platform two different ways | Platform/riser | |
| [10](#10-a-dropped-items-screen-centre-means-two-different-things-depending-on-which-attach-check-reads-it) | Two attach paths disagree on what an item's "centre" is | Platform/riser | Fixed |
| [11](#11-a-crossbar-has-none-of-a-truss-bars-drag-machinery----and-the-main-plot-has-no-pipe-attach-at-all) | Crossbars have no drag machinery; main plot has no pipe attach | Stand/boom | Fixed |
| [12](#12-a-sub-group-move-through-the-real-ui-is-a-drag----and-a-drag-here-is-a-genuine-internal-qdragqdropevent-cycle-not-just-mouse-events) | Untested: the real internal `QDrag` cycle for moving a sub-group | Fixture Manager | Fixed |
| [13](#13-assignheads-collision-behavior-at-an-already-occupied-cell-is-untested-here) | Untested: pasting a block onto an occupied cell | Fixture Manager | Fixed |
| [14](#14-a-pipe-crossbar-has-no-standoff-field-so-it-is-always-exactly-coincident-with-its-parent-boom-in-top-view) | A pipe crossbar has no standoff field, unlike a truss bar | Stand/boom | Fixed |

**#1, #2, #5, #7, #10, #11, #12, #13 and #14 are fixed** (see their entries
for what changed and what each deliberately left out). Of what's left,
**#3**/**#4** are real product/test-technique decisions worth a deliberate
look if they're ever worth revisiting; **#6**, **#8** and **#9** are Notes,
not bugs — informational only, no action expected.

---

## Truss / bar / fixture workflow

Test: `Monitor_Test::buildingARigStepByStepThroughRealUiStaysConsistent()`
(`ui/test/monitor/monitor_test.cpp`).

### 1. A Horizontal truss's default crossbar is visually coincident with the truss itself

`createBarOnTruss()`'s defaults for a **Horizontal** parent are
`barFace = FaceBottom`, `barRun = RunAlong`, `barStandoff = 0`. In Top view,
that means the bar's line renders in the *same screen position* as the
parent truss's own line — there is no pixel a user (or a test) could click
that unambiguously means "the bar" rather than "the truss". `trussUnderFixture()`'s
tie-break (whichever `TrussItem` happens to come first out of an unordered
`QHash`) picks one arbitrarily.

For a **Vertical** parent the defaults (`FaceDownstage`/`RunAcross`) instead
put the crossbar visibly off to the side, so this only affects the
Horizontal-truss case — which is also the more common one (an overhead
truss with a video/LED bar hung under it).

*Status:* **Fixed**, with a fix aimed specifically at "the standoff" per the
user's steer, while keeping clear of the pre-existing, unrelated `Pipe`
"stand"/`standOffset` concept (a boom's mount height up a *Stand* post — see
#14's doc comment on `crossbarStandoff()` for the explicit disambiguation
this raised).

`barStandoff` alone cannot separate a Horizontal truss's default bar from its
parent in Top view: `recomputeChildTrusses()`'s `base = attach + faceN *
(half + barStandoff)` only has an effect on the axis `faceN` actually points
along, and `barFaceVector(FaceBottom)` — the old Horizontal default — is a
pure Z-axis vector, so no standoff value, however large, moves the bar's X/Y
at all. The real fix is the same one that already makes the Vertical-parent
case work: default a Horizontal truss's bar to `FaceDownstage` (a real
Y-component) instead of `FaceBottom`, `RunAlong` (unchanged), with a nonzero
`barStandoff` (0.15) so it renders visibly off the truss's own line instead
of merely being *capable* of doing so. `createBarOnTruss()`'s Vertical branch
is untouched — it already worked.

A new test, `aBarOnAHorizontalTrussDefaultsClearOfItInTopView()`, creates a
Horizontal truss via `createBarOnTruss()` (the real production call, not a
hand-built `Truss`), asserts the new defaults (`barFace()==FaceDownstage`,
`barRun()==RunAlong`, `barStandoff()>0`), and proves the fix at the pixel
level using `StructureStudioView::hitTestBar()`: a hit-test at the truss's
own centreline now finds nothing, while a hit-test at the bar's actual
(now-offset) position finds it. Full suite reruns green.

### 2. A plain drag-drop attach never forms a group — only the explicit "Attach to Truss" action or a file reload does

`MonitorGraphicsView::attachFixtureToTrussAt()` (the function a real mouse
drop onto a truss/bar actually calls, via `trussUnderFixture()`) writes
`FixtureRigProps` only. It does **not** call `ensureTrussGroup()`. The
*different*, explicitly-named `attachFixtureToTruss()` (singular call site,
presumably the right-click "Attach to Truss" context-menu action) does call
it. `ensureGroupRegistry()` (the pass that runs on file load) also forms/
re-anchors groups for anything it finds already rigged.

So: drag a fixture onto a truss by hand → no group exists until you save and
reload, or until you happen to use the other, separately-named attach path.
The Layers list's grouped/anchored display (the "on truss" / container
rows this session's earlier work fixed) will look *sparse* — flat items, no
group folder — right after a plain drag, then reorganize itself once the
file round-trips.

*Status:* **Fixed.** `attachFixtureToTrussAt()` now calls `ensureTrussGroup()`
and `props->setFixtureGroup()` too, matching `attachFixtureToTruss()`
exactly (plus `mapStructureChanged()`/`refreshItemLayerState()` so the
Layers list and lock/visibility state pick it up immediately, the same as
the explicit action already did). A plain drag-drop attach now forms/joins
a group the moment it happens, not just on the next file load.
`buildingARigStepByStepThroughRealUiStaysConsistent()`'s step 4 now asserts
the bar's group exists and is anchored to the bar right after the drop,
where it used to assert the opposite. Full suite reruns green (136/136).

### 3. `mousePressEvent()`'s hit test always prefers a fixture over a bar

In `StructureStudioView::mousePressEvent()`, `hitTestFixture()` runs before
`hitTestBar()`. A fixture mounted at a bar's midpoint therefore "shadows"
the bar underneath it for mouse purposes — pressing there grabs and drags
the *fixture* (sliding it along the bar), never arms a bar-drag, no matter
how the user intended the click. Discovered because the workflow test's
first attempt pressed at the bar's exact midpoint (right where the fixture
had just been dropped in the previous step) and the bar simply never moved
— no error, no crash, just silently the wrong object dragged.

*Status:* unmarked. This matches ordinary graphics-editor conventions
(the topmost/most-specific object wins a click) and is very likely correct
as-is. Flagging only because there's no visual affordance in the UI itself
that tells a user "you're about to grab the fixture, not the bar you're
aiming for" — if this has ever come up as a real complaint (can't select
the bar something is mounted on), a modifier-click or a small
click-priority nudge could help.

### 4. `StructureStudioView`'s bar-drag arm is gated by the view's own `m_locked`, separately from the bar object's own `locked()` flag

`dragBarTo()` (called directly, bypassing raw mouse events, as in
`draggingABarChangesItsPositionAlongTheParent()`) only checks the *bar's*
own lock. But `mousePressEvent()`'s bar-hit branch — the code path a real
mouse click actually goes through — additionally requires
`setLocked(false)` on the *view* itself, or the press is accepted (selects/
highlights the bar) but the subsequent drag in `mouseMoveEvent()` is
refused. Every existing bar-drag test that uses raw mouse events
(`clickingABarSelectsAndHighlightsIt`, `doubleClickingABarActivatesIt`)
already calls `setLocked(false)` explicitly; the ones that call `dragBarTo()`
directly don't need to. This is easy to miss if you write a *new* raw-mouse-
event test by copying a `dragBarTo()`-based one instead of a
`mousePressEvent()`-based one (which is what happened building this test).

*Status:* Note. Not a bug — `m_locked` reads as "read-only editor instance"
(e.g. a preview panel embedded elsewhere), separate from "this specific
object is locked against edits" — but the two locks having the same name
pattern (`locked()`/`setLocked()`) on two different objects (`Truss` and
`StructureStudioView`) makes it easy to assume they're the same check.

---

## Target / aim workflow

Test: `Monitor_Test::aimingAFixtureAtATargetFollowsItThroughARealSession()`.

### 5. `addTargetAccepted()`'s own comment says "PanTilt palette"; the code links an "Aim" palette

The code (`Monitor::slotAddTarget()`) was deliberately changed from
auto-linking a `PanTilt` palette to an `Aim` palette — the comment right
above it explains why (a `PanTilt` palette carries fixed values and doesn't
follow the target if it moves; `Aim` resolves per fixture from geometry).
But the *existing test* `addTargetAccepted()`'s own comment
(`ui/test/monitor/monitor_test.cpp` around line 162) still says "Accepting
here also auto-links a PanTilt palette to it" — stale from before that
change. The test's actual assertion doesn't check palette *type*, so it
still passes; only the prose is wrong.

*Status:* **Fixed.** Both comments in `addTargetAccepted()` now say `Aim`
where they said `PanTilt`, matching the production code and its own comment
in `slotAddTarget()`. No behavior or assertion change — the test's checks
never depended on palette type, only the prose was wrong.

### 6. Assigning a fixture to aim at a target has no test for the real drag-drop path

The actual user gesture — drag a palette onto a "Look" in `SceneGroupLooks`
(`ui/src/scenegrouplooks.cpp`), drag a fixture/group onto its target column —
is real production `QDropEvent` handling, but no test anywhere (not
`looksdrop_test.cpp`, not this new workflow test) drives that drop through a
synthetic `QDropEvent`/`QMimeData`. Every aim-related test, including this
new one, builds the `Scene` + `QLCPalette` + membership directly
(`scene->addFixture()`/`scene->addPalette()`), which is the *data* a real
drag would produce but not the interaction itself.

*Status:* Note. Same shape as "no AddFixture-dialog test" in the truss/bar
section — a real gap in UI-level coverage for `SceneGroupLooks`'s drop
handling specifically, worth its own dedicated test file
(`ui/test/scenegrouplooks/`?) if that widget's drop logic has ever been a
source of bugs. Not attempted here; scope was the Monitor-side workflow.

---

## A hang risk in every dialog-driving test in this file (not new to this session)

Test: surfaced while building
`buildingAPlatformStepByStepThroughRealUiStaysConsistent()`, but applies to
every existing test that drives a modal dialog via
`QTimer::singleShot(0, ...)` + `QApplication::activeModalWidget()`
(`addTrussAccepted`, `addTargetAccepted`, `studioEditorDialogDragDoesNotCrash`,
and now three more in this session's new tests).

### 7. A failed `QVERIFY`/`QCOMPARE` inside the dialog-driving lambda hangs the whole test binary, not just that one test

The pattern (established well before this session) is: schedule a
zero-delay `QTimer::singleShot` lambda *before* calling the slot that opens
a blocking `QDialog::exec()`; the lambda runs inside `exec()`'s own nested
event loop, finds the dialog's widgets, and clicks a button to close it.
If a `QVERIFY`/`QCOMPARE` *inside that lambda* fails, the macro's normal
"return out of the enclosing function" behavior returns out of the
**lambda**, before it reaches the button click. Nothing else will ever
close that dialog under offscreen QTest (there's no user to click Cancel),
so `dlg.exec()` blocks forever — not a test failure, a hung process that
has to be killed. This happened three times in a row while building the
platform/riser dialog interaction (a wrong widget lookup each time), and
each one silently hung the binary rather than reporting a failure.

*Status:* **Fixed.** Both test files now have a shared `driveDialog()` (plus
`driveMessageBox()` for a `QMessageBox`-style confirm, `findRealLineEdit()`
and `findSpinByLabel()` for the two lookup traps in #8/#10) that schedules
the `QTimer::singleShot`, runs a caller-supplied `fill(modal) -> bool`, and
—no matter what `fill` returned or asserted internally— clicks the dialog's
button (or closes it) *before* reporting success/failure. Every
dialog-driving call site in `ui/test/monitor/monitor_test.cpp` and
`ui/test/fixturemanager/fixturemanager_test.cpp` (18 in total, including
the pre-existing `addTrussAccepted`/`addTargetAccepted`/
`removeSelectedTruss`/etc. that predate this session, and the bespoke
multi-dialog search in `studioEditorDialogDragDoesNotCrash`) now goes
through one of these, so a wrong widget lookup fails that one test instead
of hanging the binary. Full suite reruns green (136/136 in `monitor_test`,
5/5 in `fixturemanager_test`).

### 8. `findChild<QLineEdit*>()` can silently grab a spin box's own internal editor

`QAbstractSpinBox` (the base of `QDoubleSpinBox`, `FeetInchesSpinBox`, etc.)
owns its OWN internal `QLineEdit` as its text-editing widget. A dialog with
any spin box constructed/parented such that it precedes the real "Name:"
field in the widget tree will have `dlg.findChild<QLineEdit*>()` return that
spin box's internal editor instead — invisibly, since it's still a valid,
non-null `QLineEdit*`, just the wrong one. The "Add Truss" and "Edit Target"
dialogs happen to add their name field before any spin box exists at all,
so this never bit them; the platform dialog's more complex layout (a
geometry form embedded inside a larger Structure Studio canvas pane, itself
full of its own spin boxes) hit it immediately.

*Status:* Note — not a bug in the app, a trap in the TEST technique. Worth
writing down once so the next dialog test doesn't rediscover it the same
way: filter for a `QLineEdit` whose `parentWidget()` is not itself a
`QAbstractSpinBox`, or scope the search to the specific sub-container known
to hold the real field, rather than a bare `findChild<QLineEdit*>()` on the
whole dialog once more than one text-editing widget might exist.

---

## Platform / riser workflow

Test: `Monitor_Test::buildingAPlatformStepByStepThroughRealUiStaysConsistent()`.

### 9. Deck-mounted and riser-mounted fixtures follow a moved platform through two different mechanisms

A deck-mounted fixture's XY is stored as an ABSOLUTE position (mm); when the
platform moves, `MonitorGraphicsView::slotPlatformMoved()` has explicit code
that walks every fixture with `deckPlatformId == p->id()` and shifts its
stored XY by the platform's own origin delta. A riser-mounted fixture's
position is never stored at all — `MonitorProperties::fixtureRigPosition()`
computes it fresh every time from `platform->originX()/originY() + riserU/
riserV`, so it comes out correct automatically the instant the platform's
origin changes, with no carry code involved.

*Status:* Note, not obviously a problem (both end up correct, which the new
test checks explicitly) — but it means "does a fixture follow its
platform when the platform moves" is actually two independently-maintained
code paths that happen to agree today. A future change to one (e.g.
optimizing away the deck carry-along loop, or changing what triggers it) 
could silently break only ONE of the two mount kinds, and nothing about the
riser path would catch it. Worth keeping the two mounts' "does it follow"
behavior covered by a test that checks both at once, the way this one does,
rather than as two separate tests that could each pass while the underlying
assumption (both mounts always follow) quietly stops holding for one of them.

### 10. A dropped item's screen "centre" means two different things depending on which attach check reads it

The truss/bar drop path (`trussUnderFixture()`, `attachFixtureToTrussAt()`)
reads a dropped item's centre as `sceneBoundingRect().center()`. The
deck-mount auto-attach path (in `slotFixtureMoved()`) reads it as
`pos() + halfIcon()`, where `halfIcon()` is half of the item's `cellSize()`
— NOT its bounding rect. The two differ because the item's drawn bounding
rect also includes its name label (drawn below the icon), which pulls the
bounding box's centre down without moving the icon itself. A test (or a
real drag that happens to rely on bounding-rect maths) computed against the
wrong one lands the "centre" about a label's height away from where the
truss/tower code would put it — which is small compared to a truss's width
tolerance, but was enough to miss a platform's footprint entirely at the
margin this test first tried.

*Status:* **Fixed** — the two paths now agree. There was no good reason for
the difference: both checks want "the fixture's own visual centre", and the
label-inclusive `sceneBoundingRect().center()` was the odd one out (its
result moves depending on whether the label happens to be drawn, which
`cellSize()`/`halfIcon()` never do) — the deck-mount path's plain
`pos() + halfIcon()` was the one already doing the right thing.

Added `MonitorFixtureItem::iconCentreScene()` — a single canonical accessor
(`mapToScene(iconRect().center())`) — validated by an existing precedent
already solving the same class of problem elsewhere
(`mapRectToScene(iconRect())` at `monitorgraphicsview.cpp:2306`). Replaced
all 7 `sceneBoundingRect().center()` call sites that meant "the fixture's own
position" with it: `trussUnderFixture()`, `pipeUnderFixture()`, both sites in
`attachFixtureToTrussAt()`, the aim-trace line, `attachFixtureToTruss()`
(this one had an internal read/write inconsistency, also fixed), and the
"nothing lands" note anchor. Left the *unrelated* `sceneBoundingRect()` uses
alone (selection-viewport rect, structure-item `centreOf`, the group badge
position) — those are legitimately about the rendered extent, not "where is
this fixture".

Updated 6 test call sites in `monitor_test.cpp` to use `iconCentreScene()`
too, which let the platform test's manual `cellHalf` workaround (added when
#10 was first discovered) be deleted — the two paths agreeing means the
workaround is no longer needed. Full suite reruns green with no tolerance
changes required.

---

## Stand / boom / crossbar workflow

Test: `Monitor_Test::buildingAStandStepByStepThroughRealUiStaysConsistent()`.

### 11. A crossbar has none of a truss bar's drag machinery -- and the main plot has no pipe attach at all

Trusses and bars share one code family: `hitTestBar()`, `dragBarTo()`,
`m_selectedBarId`/`m_dragBarId`, and a real mouse drag on a truss's own
Structure Studio editor all work for a bar. A stand's crossbar is a
different class entirely -- a `Pipe` with `isBarOnPipe()` true, not a
`Truss` -- and **none** of that machinery applies to it:
`hitTestBar()` returns `UINT_MAX` immediately whenever the view isn't
`TrussKind`, and `dragBarTo()` only ever looks up `props->truss(barId)`.
A crossbar's *only* real-UI edit is the "Height up boom" spin box in its
own "Edit Pipe" dialog -- there is no drag gesture for it at all, on any
view.

Separately, and more surprising: **the main 2D plot has no pipe/boom
support whatsoever**. `MonitorGraphicsView::slotFixtureMoved()` has real
branches for a truss attach and a deck-mount auto-attach, but grep confirms
zero pipe branches, and no `pipeUnderFixture()` function exists at all. The
only ways to mount a fixture on a boom are the context-menu "Mount on >
Boom > name" action (`attachFixtureToPipe()`) or dropping it on the stand's
own Structure Studio canvas -- never a plain drag on the main plot the way
a fixture auto-decks onto a platform or auto-attaches to a truss just by
being dropped near one.

*Status:* **Fixed**, both halves:

- `MonitorGraphicsView::pipeUnderFixture()` (mirrors `trussUnderFixture()`)
  plus a matching auto-attach branch in `slotFixtureMoved()` give the main
  plot a real drop-to-attach path for a pipe, the same as a truss or a
  platform. A dropped fixture attaches to whichever boom/crossbar it lands
  on; a horizontal pipe additionally derives its ALONG position from the
  drop point (a vertical boom's height can't be read from a Top-view point
  at all, the same reason a Vertical truss's own drag-attach leaves its
  offset alone too — see `attachFixtureToTruss()`'s own comment on that).
  The deck-mount auto-attach's guard now also excludes an already
  pipe-mounted fixture, so the two auto-attach paths can't fight over the
  same drop.
- `StructureStudioView` gained a parallel bar-drag mechanism for a Pipe
  crossbar — `hitTestPipeBar()`/`dragPipeBarTo()`,
  `m_dragPipeBarId`/`m_selectedPipeBarId`, `setPipeBarHighlight()`, and
  `pipeBarSelected`/`pipeBarActivated`/`pipeBarMoved` signals — wired into
  `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`/
  `mouseDoubleClickEvent` alongside the existing truss-bar branches, plus
  the same amber "selected" colour `drawPipe()` now applies to a crossbar.
  A crossbar's own lock is respected the same way a truss bar's is; a pipe
  crossbar has no cross-shift concept (`recomputePipeAnchors()` derives its
  origin from `parentPipeOffset` alone), so `dragPipeBarTo()` is a single
  degree of freedom, unlike `dragBarTo()`.

**Scope not attempted here** (disclosed, not silently dropped): the new
`pipeBarSelected`/`pipeBarMoved` signals are emitted but not wired into a
per-object inspector panel the way `barSelected`/`barMoved` already are in
`makeStudioPane()` — a crossbar's "Height up boom" field only exists in its
own "Edit Pipe" dialog, unaffected by this fix, not in an inline strip next
to the canvas the way a truss bar's "Along truss" field is. That's a
separate, appreciably sized feature (a whole second inspector layout), not
literally "give a crossbar the drag capability a truss bar already has,"
which is what this entry asked for.

`buildingAStandStepByStepThroughRealUiStaysConsistent()` now exercises both
through real UI: a plot drop attaches the fixture to the boom, and a real
mouse drag moves the crossbar. Building the test surfaced a THIRD, related
finding (logged separately below as #14): a crossbar's origin is always
exactly coincident with its parent boom's own point in Top view, with no
way to separate them, unlike a truss bar's `barStandoff`.

### 14. A pipe crossbar has no standoff field, so it is always exactly coincident with its parent boom in Top view

`MonitorProperties::recomputePipeAnchors()`'s bar-on-pipe pass derives a
crossbar's origin as `parent->positionAt(parentPipeOffset)` with nothing
added — no analogue of a `Truss` bar's `barStandoff`, which is what keeps
a truss bar visually separated from its own parent (see #1, which is the
*absence* of a nonzero default for that field; this is the absence of the
field's entire concept for a `Pipe`). A vertical boom draws as a single
"disc" point in Top view regardless of height (there is no way to pick
"where up the boom" from directly above), so a crossbar hung anywhere on
it — at the default top offset or anywhere else — always starts at that
exact same point. `buildingAStandStepByStepThroughRealUiStaysConsistent()`
originally tried to attach a fixture to the boom AFTER the crossbar already
existed and got the crossbar instead, a tie broken only by `QHash`
iteration order over `m_pipeItems` — silently wrong roughly half the time.
The test now creates the crossbar AFTER attaching to the boom, sidestepping
the ambiguity rather than resolving it.

*Status:* **Fixed.** `Pipe` gained its own `crossbarStandoff()`/
`setCrossbarStandoff()` field (default 0, back-compatible with every
existing `.qxw`) — offset perpendicular to the crossbar's own `runAngle()`,
so it stays correct if the crossbar is later reoriented, rather than a fixed
axis. Its doc comment in `pipe.h` explicitly calls out that this is **not**
the same thing as the pre-existing `standOffset()`/`hasStandOffset()` pair
(height a *Pipe* mounts UP a *Stand*'s post) — the two are unrelated
concepts that happen to share the word "stand[off]", which is exactly the
collision the user flagged up front. `recomputePipeAnchors()`'s bar-on-pipe
pass now applies this offset before setting the crossbar's origin, mirroring
`recomputeChildTrusses()`'s `faceN * (half + barStandoff)` term for a Truss
bar. All three crossbar-creation sites (`studioAddBar()`'s two branches,
`slotAddBarToPipe()`) default it to 0.15, matching #1's truss-bar default.
XML load/save added (`CrossbarStandoff` attribute, omitted when zero). The
"Edit Pipe" dialog's boom-mounted-crossbar section gained a "Standoff from
boom" spin box (`slotEditPipe()`), read back into `setCrossbarStandoff()` on
accept, alongside the existing "Height up boom" field — so it's editable
through the real UI, not just a fixed default.

`buildingAStandStepByStepThroughRealUiStaysConsistent()`'s step ordering,
which #14's *discovery* had forced into an unnatural "attach to boom before
creating the crossbar" order to dodge the coincident-origin ambiguity, is now
reverted to the natural order (boom → crossbar → attach via plot drop), with
an explicit `QVERIFY2(crossbar->crossbarStandoff() > 0.0f, ...)` proving the
separation exists. Full suite (monitor_test 137/137, fixturemanager_test
5/5) reruns green.

---

## Fixture Manager: fixture-group workflow

Test: `FixtureManager_Test::populatingAndMovingAFixtureGroupBlockThroughRealUi()`
(`ui/test/fixturemanager/fixturemanager_test.cpp`) — a different subsystem
from everything above (`ui/src/fixturemanager.cpp`,
`ui/src/fixturegroupeditor.cpp`), tested for the first time with any real
UI interaction: the file's two existing tests were purely API-driven before
this.

### 12. A sub-group move through the real UI is a drag -- and a drag here is a genuine internal `QDrag`/`QDropEvent` cycle, not just mouse events

Every other "drag" tested in this session's other workflow tests (truss,
bar, platform, boom) is driven by feeding raw `QMouseEvent`s straight to a
widget's own `mousePressEvent()`/`mouseMoveEvent()`/`mouseReleaseEvent()`
overrides or its `eventFilter()` — no actual Qt drag-and-drop machinery
involved anywhere. The fixture-group grid is different: past the drag
threshold, `FixtureGroupEditor::eventFilter()`'s `MouseMove` branch calls
`startCellDrag()`, which builds a real `QMimeData`/`QDrag` (with its own
internal mime type, `"application/x-qlc-groupcells"`) and almost certainly
calls `QDrag::exec()` — a genuine OS-mediated drag session, not just event
delivery. That's a materially bigger, riskier thing to fake under offscreen
QTest than synthetic mouse events (no user, no window manager, no real
drop target to negotiate with).

This test does not attempt it. It calls `FixtureGroupEditor::moveSubGroup()`
directly instead — a real, exact, single-purpose function, but reached in
the actual app via the right-click "Move sub-group" context-menu action,
not via a drag. The mouse-driven internal drag path itself (press → move
past threshold → `startCellDrag()` → `QDrag::exec()` → the `Drop` branch of
`eventFilter()` computing `dx`/`dy` from the drop position → `moveHeads()`)
remains completely untested by anything in this file.

*Status:* **Fixed**, via the first of the two paths this entry laid out:
test it without a real drag session. New test
`draggingASubGroupBlockMovesItThroughTheRealDropPath()` pastes a block the
same real way `populatingAndMovingAFixtureGroupBlockThroughRealUi()` does,
then sets `editor->m_dragCells`/`m_dragAnchor` directly — exactly what
`startCellDrag()` itself would have captured from a real press on an
already-selected block — and delivers a synthetic `QDropEvent` (tagged with
the internal `"application/x-qlc-groupcells"` mime type, empty payload,
since the Drop handler reads move geometry from the drop position and
`m_dragCells`/`m_dragAnchor`, never from the `QMimeData`'s own bytes)
straight to `eventFilter()`. `QDrag::exec()` itself is still skipped (no
window manager to negotiate with under offscreen QTest), but everything
downstream of it now runs for real: the `Drop` branch's dx/dy-from-position
math, its deferred `QTimer::singleShot(0, ...)` call into `moveHeads()`
(drained with `QTest::qWait(30)`, the established pattern for a coalesced
event-loop callback elsewhere in this suite), and `canMoveHeads()`'s guard.

The test proves both directions of the guard: a drop that would collide with
the target group's own untouched member is refused (nothing moves), and a
drop that fits actually carries both of the block's fixtures to their new
cells as a unit, sub-group tag intact. `moveSubGroup()` — the context-menu
path the sibling workflow test already covers — remains untested by *this*
test on purpose; the two together now cover both real ways a sub-group
actually moves.

### 13. `assignHead()`'s collision behavior at an already-occupied cell is untested here

`slotAddGroupBlock()`'s own paste loop only skips a source head if
`existing.contains(gh)` — i.e. if that exact fixture/head is ALREADY a
member of the target group somewhere, not if the DESTINATION CELL happens
to be occupied by something else. This test deliberately anchored its
paste on an empty cell to sidestep the question (see the comment at the
click step) rather than find out what `assignHead()` actually does when
asked to write into a cell something else already occupies — overwrite
silently, refuse, or something else.

*Status:* **Fixed** — answered, not changed: the behavior turned out to
already be intentional-looking, just undocumented and unverified. New test
`pastingABlockOntoAnOccupiedCellOverwritesTheOccupant()` plants an occupant
fixture at the paste's destination cell, then pastes a block anchored right
on top of it. `FixtureGroup::assignHead()` writes `m_heads[pt] = head`
unconditionally for an explicit point — a plain `QMap` assignment — so the
occupant is silently overwritten: no refusal, no relocation, no trace of it
left anywhere else in the group. The test asserts exactly that (and that the
paste's *other* cell and the group's untouched original member are
unaffected), with a comment pointing at both this entry and the assertion to
update together if that overwrite-on-collision behavior is ever deliberately
changed.

---
