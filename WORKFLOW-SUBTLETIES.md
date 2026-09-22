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
| [1](#1-a-horizontal-trusss-default-crossbar-is-visually-coincident-with-the-truss-itself) | Horizontal truss's default bar is visually coincident with the truss | Truss/bar |
| [2](#2-a-plain-drag-drop-attach-never-forms-a-group--only-the-explicit-attach-to-truss-action-or-a-file-reload-does) | Plain drag-drop attach never forms a group | Truss/bar |
| [3](#3-mousepresseventss-hit-test-always-prefers-a-fixture-over-a-bar) | Fixture hit-test always wins over a bar underneath it | Truss/bar |
| [4](#4-structurestudioviews-bar-drag-arm-is-gated-by-the-views-own-m_locked-separately-from-the-bar-objects-own-locked-flag) | View lock vs. object lock are two different checks | Truss/bar |
| [5](#5-addtargetaccepteds-own-comment-says-pantilt-palette-the-code-links-an-aim-palette) | Stale comment: says PanTilt, code links Aim | Target/aim |
| [6](#6-assigning-a-fixture-to-aim-at-a-target-has-no-test-for-the-real-drag-drop-path) | No test for the real palette-onto-Look drag | Target/aim |
| [7](#7-a-failed-qverifyqcompare-inside-the-dialog-driving-lambda-hangs-the-whole-test-binary-not-just-that-one-test) | A failed assertion inside a dialog lambda hangs the binary | Test technique (all dialogs) |
| [8](#8-findchildqlineedit-can-silently-grab-a-spin-box-own-internal-editor) | `findChild<QLineEdit*>()` can grab a spin box's own editor | Test technique (all dialogs) |
| [9](#9-deck-mounted-and-riser-mounted-fixtures-follow-a-moved-platform-through-two-different-mechanisms) | Deck vs. riser fixtures follow a moved platform two different ways | Platform/riser |
| [10](#10-a-dropped-items-screen-centre-means-two-different-things-depending-on-which-attach-check-reads-it) | Two attach paths disagree on what an item's "centre" is | Platform/riser |
| [11](#11-a-crossbar-has-none-of-a-truss-bars-drag-machinery----and-the-main-plot-has-no-pipe-attach-at-all) | Crossbars have no drag machinery; main plot has no pipe attach | Stand/boom |
| [12](#12-a-sub-group-move-through-the-real-ui-is-a-drag----and-a-drag-here-is-a-genuine-internal-qdragqdropevent-cycle-not-just-mouse-events) | Untested: the real internal `QDrag` cycle for moving a sub-group | Fixture Manager |
| [13](#13-assignheads-collision-behavior-at-an-already-occupied-cell-is-untested-here) | Untested: pasting a block onto an occupied cell | Fixture Manager |

Highest-signal to look at first: **#7** (a real risk to CI/test reliability,
not just app behavior), **#2** and **#11** (both read as real product
decisions worth making on purpose rather than by accident), **#12** (the
biggest actual coverage gap this effort left behind).

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

*Status:* unmarked. Worth asking whether a Horizontal truss's default bar
should get a nonzero standoff (or drop below the truss's Z, matching how it
already renders in *elevation* — see `updateTrusses()`'s Z-offset comment)
so it's clickable in Top view too, or whether Top-view bar interaction on a
Horizontal truss is simply not an expected workflow (elevation views only).

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

*Status:* unmarked. Possibly intentional (grouping is a deliberate,
named action; a casual drag stays ungrouped until you ask for it) — but the
inconsistency between "drag onto a truss" and "right-click → Attach to
Truss" producing different downstream state (one groups, one doesn't) for
what reads as the same user intent is worth a deliberate decision rather
than an accident of two code paths growing independently.

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

*Status:* unmarked, trivially fixable (one-line comment update in
`addTargetAccepted()`). Flagging because a comment like this is exactly what
a future reader would trust over re-deriving the behavior from the code —
worth fixing opportunistically.

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

*Status:* unmarked, and worth a decision either way:
  - **Fix**: give this file a shared helper (e.g. `driveModalDialog(fn)`)
    that wraps the lambda, catches a failed lookup, force-closes the dialog
    via `modal->close()` in every case, and *then* reports the failure —
    turning every one of these into "fails this test" instead of "hangs the
    run". The new platform test's own lambda (see its comment) does this by
    hand; worth lifting into a shared utility if this pattern gets used
    again, which it likely will given how many more dialogs exist
    (stand, pipe, tower, group).
  - **Accept**: the existing tests all happen to look up unambiguous, single
    widgets and have never hit this in practice; treat it as an accepted
    risk of the pattern and only harden it if/when CI actually hangs on one.

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

*Status:* unmarked. Two attach paths disagreeing about what "the item's
centre" means is the kind of inconsistency worth a deliberate look — either
they SHOULD agree (in which case one is arguably using the wrong point,
and a real user could see a fixture that visually looks centered on a
target fail to attach, or attach somewhere visibly off from where it
looks dropped), or there's a reason specific to each (truss attach cares
about the rendered icon+label extent for its "does it touch" check, deck
attach cares about the plain grid cell) that's worth a one-line comment at
each site cross-referencing the other so a future reader doesn't assume
they're interchangeable.

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

*Status:* unmarked -- this reads as a real feature-completeness gap between
the truss/bar/platform family (all of which get main-plot drag-drop and,
for bars, a dedicated drag machinery) and the stand/boom/crossbar family
(neither). Worth a decision: is stand rigging meant to stay a
Structure-Studio-only workflow permanently (booms are less commonly
repositioned once rigged, maybe intentionally more deliberate), or is this
simply the newer/less-developed corner of the structural model and due for
the same main-plot treatment trusses and platforms already have?

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

*Status:* unmarked, and the most open question this whole effort raised.
Two honest paths forward, worth deciding rather than defaulting into:
  - **Test it anyway, without a real drag session**: construct a synthetic
    `QDropEvent` carrying `QMimeData` tagged with the internal mime type,
    with `m_dragCells`/`m_dragAnchor` set directly (mirroring what
    `startCellDrag()` would have captured from a real selection+press), and
    deliver it straight to `eventFilter()` — skipping `QDrag::exec()`
    entirely while still exercising the actual `Drop` branch's own
    dx/dy-from-position math and `canMoveHeads()` guard. More real than
    calling `moveSubGroup()` directly; still not a full end-to-end drag.
  - **Accept the gap**: the context-menu path this test does cover reaches
    the identical mutation (`moveHeads()` under the hood either way), so
    the DATA outcome of "move a sub-group" is covered even though the
    GESTURE isn't. If the internal drag path has never been a source of
    reported bugs, that may be coverage enough.

### 13. `assignHead()`'s collision behavior at an already-occupied cell is untested here

`slotAddGroupBlock()`'s own paste loop only skips a source head if
`existing.contains(gh)` — i.e. if that exact fixture/head is ALREADY a
member of the target group somewhere, not if the DESTINATION CELL happens
to be occupied by something else. This test deliberately anchored its
paste on an empty cell to sidestep the question (see the comment at the
click step) rather than find out what `assignHead()` actually does when
asked to write into a cell something else already occupies — overwrite
silently, refuse, or something else.

*Status:* unmarked. Worth a dedicated (much smaller) test on its own:
paste a block anchored so it overlaps an existing member and assert
whichever behavior is intended. Left out here because it's a different
question from the workflow this test is about, not because it's low-value.

---
