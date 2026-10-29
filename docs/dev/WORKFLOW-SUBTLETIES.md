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
| [3](#3-mousepresseventss-hit-test-always-prefers-a-fixture-over-a-bar) | Fixture hit-test always wins over a bar underneath it | Truss/bar | Accepted |
| [4](#4-structurestudioviews-bar-drag-arm-is-gated-by-the-views-own-m_locked-separately-from-the-bar-objects-own-locked-flag) | View lock vs. object lock are two different checks | Truss/bar | Accepted |
| [5](#5-addtargetaccepteds-own-comment-says-pantilt-palette-the-code-links-an-aim-palette) | Stale comment: says PanTilt, code links Aim | Target/aim | Fixed |
| [6](#6-assigning-a-fixture-to-aim-at-a-target-has-no-test-for-the-real-drag-drop-path) | No test for the real palette-onto-Look drag | Target/aim | Accepted |
| [7](#7-a-failed-qverifyqcompare-inside-the-dialog-driving-lambda-hangs-the-whole-test-binary-not-just-that-one-test) | A failed assertion inside a dialog lambda hangs the binary | Test technique (all dialogs) | Fixed |
| [8](#8-findchildqlineedit-can-silently-grab-a-spin-box-own-internal-editor) | `findChild<QLineEdit*>()` can grab a spin box's own editor | Test technique (all dialogs) | Accepted |
| [9](#9-deck-mounted-and-riser-mounted-fixtures-follow-a-moved-platform-through-two-different-mechanisms) | Deck vs. riser fixtures follow a moved platform two different ways | Platform/riser | Accepted |
| [10](#10-a-dropped-items-screen-centre-means-two-different-things-depending-on-which-attach-check-reads-it) | Two attach paths disagree on what an item's "centre" is | Platform/riser | Fixed |
| [11](#11-a-crossbar-has-none-of-a-truss-bars-drag-machinery----and-the-main-plot-has-no-pipe-attach-at-all) | Crossbars have no drag machinery; main plot has no pipe attach | Stand/boom | Fixed |
| [12](#12-a-sub-group-move-through-the-real-ui-is-a-drag----and-a-drag-here-is-a-genuine-internal-qdragqdropevent-cycle-not-just-mouse-events) | Untested: the real internal `QDrag` cycle for moving a sub-group | Fixture Manager | Fixed |
| [13](#13-assignheads-collision-behavior-at-an-already-occupied-cell-is-untested-here) | Untested: pasting a block onto an occupied cell | Fixture Manager | Fixed |
| [14](#14-a-pipe-crossbar-has-no-standoff-field-so-it-is-always-exactly-coincident-with-its-parent-boom-in-top-view) | A pipe crossbar has no standoff field, unlike a truss bar | Stand/boom | Fixed |
| [15](#15-a-fresh-drop-nested-onto-an-effect-can-silently-fail-to-render-as-nested) | A fresh drop nested onto an Effect can silently fail to render as nested | Programming tab | Fixed |
| [16](#16-a-group-drop-folds-away-an-already-covered-fixed-target-even-with-zero-covering-looks) | A group drop folds away an already-covered fixed target, even with zero covering looks | Programming tab | Accepted |
| [17](#17-reconcileafterpaletteapply-defers-past-dropevent-and-its-dialog-has-no-ok-button) | `reconcileAfterPaletteApply()` defers past `dropEvent()`, and its dialog has no Ok button | Programming tab (test technique) | Accepted |
| [18](#18-an-unshown-top-level-widgets-own-nested-widgets-dont-get-real-geometry-from-resize-alone) | An unshown top-level widget's own nested widgets don't get real geometry from resize() alone | Programming tab (test technique) | Accepted |
| [19](#19-savebucketasscenes-name-and-path-arguments-look-symmetric-but-only-one-falls-back) | `saveBucketAsScene()`'s name and path arguments look symmetric but only one falls back | Programmer/Save | Accepted |
| [20](#20-saveprogrammerasscene-is-dead-code-with-a-stale-doc-comment-pointing-at-it-as-the-real-save-path) | `saveProgrammerAsScene()` is dead code with a stale doc comment pointing at it as the real Save path | Programmer/Save | Fixed |

**All 20 entries are resolved.** #1, #2, #5, #7, #10, #11, #12, #13, #14,
#15 and #20 were fixed (see their entries for what changed and what each
deliberately left out); #3, #4, #6, #8, #9, #16, #17, #18 and #19 were
reviewed and accepted as-is — deliberate behavior, a documented test-technique trap,
or a speculative coverage gap with no evidence it's ever caused a real
problem. Nothing on this list is open work.

**#15–18** came from a second pass surveying the Programming tab's own
drag-construction gestures (drag palettes→looks, groups→dynamic targets,
fixtures→fixed targets — CLAUDE.md's description of the fork's headline
feature), closing #6 above by actually writing the test it called for. Test:
`ProgrammingManager_Test::buildingALookThroughRealProgrammingTabDragAndDropStaysConsistent()`
and its two siblings (`ui/test/programmingmanager/programmingmanager_test.cpp`).

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

*Status:* **Accepted.** Matches ordinary graphics-editor conventions (the
topmost/most-specific object wins a click); no evidence it's ever been a
real complaint. Revisit with a modifier-click or click-priority nudge only
if "can't select the bar something is mounted on" actually comes up.

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

*Status:* **Accepted.** Not a bug — `m_locked` reads as "read-only editor
instance" (e.g. a preview panel embedded elsewhere), separate from "this
specific object is locked against edits". The two locks sharing a name
pattern (`locked()`/`setLocked()`) on two different objects (`Truss` and
`StructureStudioView`) is worth remembering, which this entry now does; no
code change warranted.

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

*Status:* **Accepted.** A real gap in UI-level coverage for
`SceneGroupLooks`'s drop handling, but speculative — no evidence that drop
logic has ever been a source of bugs. Worth a dedicated test file
(`ui/test/scenegrouplooks/`?) if it ever is; not pursued otherwise.

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

*Status:* **Accepted.** Not a bug in the app, a trap in the TEST technique —
now written down so the next dialog test doesn't rediscover it the same way:
filter for a `QLineEdit` whose `parentWidget()` is not itself a
`QAbstractSpinBox`, or scope the search to the specific sub-container known
to hold the real field, rather than a bare `findChild<QLineEdit*>()` on the
whole dialog once more than one text-editing widget might exist. (This
session's `findRealLineEdit()` helper, added for #7, already does the
filtering.)

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

*Status:* **Accepted.** Not obviously a problem — both end up correct, which
the workflow test already checks explicitly, in one test that covers both
mount kinds at once rather than two that could each pass while the
underlying "both mounts always follow" assumption quietly stops holding for
one of them. No code change; the existing coverage is the mitigation.

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

## Programming tab: SceneGroupLooks drag-and-drop workflow

Test: `ProgrammingManager_Test::buildingALookThroughRealProgrammingTabDragAndDropStaysConsistent()`,
`droppingOnTheWrongColumnIsRejected()`, `droppingAGroupFoldsAwayAnAlreadyCoveredFixedTarget()`
(`ui/test/programmingmanager/programmingmanager_test.cpp`) — the Programming
tab's own construction gestures (`ui/src/programmingmanager.cpp`,
`ui/src/scenegrouplooks.cpp`), driven for the first time through real
`QMimeData` built the same way the real source trees
(`FunctionsTreeWidget`, `FixtureGroupSource`) build it, delivered via a
synthetic `QDropEvent` straight to `SceneGroupLooks::dropEvent()`. This
closes #6 above.

### 15. A fresh drop nested onto an Effect can silently fail to render as nested

`SceneGroupLooks::moveLooksToTarget()` — the function a nested drop onto an
Effect row calls — only called `reload()` (which rebuilds the Looks tree)
*inside* an `if (m_scene->reorderPalettes(order))` guard. `reorderPalettes()`
itself returns `false` whenever the computed order is identical to the
scene's current palette order, a legitimate no-op guard for this function's
*other* caller (the right-click "Feed effect ▸" context menu, where
re-choosing the effect a look already feeds really is nothing to do).

But `dropEvent()`'s own nested-drop path calls `m_scene->addPalette(pid)`
*before* `moveLooksToTarget()` — which always **appends** the fresh palette
to the end of the list. "Nest right after this effect" and "already at the
end" coincide exactly whenever the drop lands on the last (or only) Effect
in the scene — the ordinary case of dropping a colour onto your one and
only effect. When they coincide, `reorderPalettes()` correctly reports no
data change, `moveLooksToTarget()` skips its `reload()`, and the just-dropped
palette sits in the scene's palette list (the DATA is correct) but was never
rendered as a tree item at all — not even as a top-level one — because the
tree was never told to rebuild since the palette was added. Nothing crashes
or errors; the drop just visually appears to do nothing until some *unrelated*
later action happens to trigger another `reload()`.

Found by the workflow test itself: dropping a Dimmer directly onto the
scene's one Effect palette (row position computed from the real,
laid-out tree, mapped through the same coordinate space `dropEvent()`
itself reads) added the palette to `scene->palettes()` but produced no row
in `m_lookList` for it whatsoever — confirmed by dumping every row in the
tree immediately after the drop and finding only the two palettes dropped
in earlier steps, not the new one.

*Status:* **Fixed.** `moveLooksToTarget()` now calls `reload()`
unconditionally; `setModified()`/`sceneModified()` stay gated on
`reorderPalettes()` actually reporting a change (no spurious "modified"
flag or signal for a genuine no-op reorder from the context menu). A
redundant `reload()` after an already-`reload()`-ing `reorderPalettes()`
success is a cheap, safe no-op — the function is already called after every
other mutation in this file. Full `programmingmanager_test`/`looksdrop_test`/`monitor_test`
suites rerun green.

### 16. A group drop folds away an already-covered fixed target, even with zero covering looks

`dropEvent()`'s fixture-group branch strips a fixed target's baked
`SceneValue`s that the group's own (non-Effect) looks now cover, and drops
the fixed target entirely once nothing uncovered is left on it. The check
is `keep.isEmpty()`, built *only* from iterating the fixture's *existing*
baked values (`foreach (const SceneValue &scv, m_scene->values()) ...`). A
freshly-added fixed target with **zero** baked values has nothing for that
loop to iterate at all, so `keep` never gets anything appended to it and
stays empty — `keep.isEmpty()` is trivially true — regardless of whether
the group's looks cover anything whatsoever. A fixture added as an
individual target and never given a single value disappears from Targets
the instant its group is dropped in, even if the scene has no looks applied
to either yet.

*Status:* **Accepted**, and verified rather than left as a guess —
`droppingAGroupFoldsAwayAnAlreadyCoveredFixedTarget()` drops a fixture
individually first (confirming via `scene->values()` that it truly has no
baked value), then drops a group sharing that fixture as a member, and
asserts the fixed target is gone while the group is now present. This reads
as intentional once stated plainly: an individual fixed target with
nothing of its own baked onto it is trivially "fully covered" by anything
that supersedes it (a group containing the same fixture is exactly that),
so folding it away rather than leaving a redundant, value-less target
behind is arguably the right call. Flagged here so a future reader who
notices a fixed target vanish on a group drop knows it's this, not data
loss.

### 17. `reconcileAfterPaletteApply()` defers past `dropEvent()`, and its dialog has no Ok button

Whenever a palette drop happens, `dropEvent()` schedules
`reconcileAfterPaletteApply()` via `QTimer::singleShot(0, this, ...)` rather
than calling it inline — deliberately, so its potential modal prompt
doesn't spin a nested event loop inside the drop event itself, before the
drop has been accepted or the (real) drag source has cleaned up. That
prompt — `QMessageBox::question(..., QMessageBox::Yes | QMessageBox::No,
QMessageBox::No)` — only appears if the scene has baked `SceneValue`s no
*currently applied* look covers; a scene built purely by drops with nothing
pre-baked never triggers it, which is why none of this session's three new
tests needed to drive it.

Two traps for a future test that does need to exercise this path:
  - A test that drops a palette and asserts immediately, without at least
    one `QCoreApplication::processEvents()` afterward, will observe the
    scene *before* `reconcileAfterPaletteApply()` has run at all — this
    session's own tests all include a `processEvents()` after every
    palette drop specifically because of this.
  - The `driveDialog()` helper from `ui/test/monitor/monitor_test.cpp` /
    `ui/test/fixturemanager/fixturemanager_test.cpp` looks up
    `QDialogButtonBox::Ok` and falls back to `modal->close()` (then asserts
    the button was found) if it can't find one — but this dialog only has
    Yes/No buttons, so that lookup fails, the fallback close still works,
    but the helper's own assertion about finding the button would then
    incorrectly fail the test. A dedicated Yes/No variant (`QMessageBox`
    still builds its buttons into a real `QDialogButtonBox`, so
    `findChild<QDialogButtonBox*>()` still finds it) would be needed.

*Status:* **Accepted** — a documented test-technique trap, same category as
#8, not a product bug. Nothing to fix until a test actually needs to drive
this specific dialog.

### 18. An unshown top-level widget's own nested widgets don't get real geometry from `resize()` alone

Every prior workflow test in this file (Monitor, Fixture Manager) gets real,
non-degenerate widget geometry under offscreen QTest from `mgr.resize(...)`
+ `QCoreApplication::processEvents()` (Fixture Manager also calls
`mgr.m_splitter->setSizes(...)` explicitly, since it exposes its splitter as
a member). The Programming tab's `ProgrammingManager` does the same
`resize()`/`processEvents()` dance, but its top-level `QSplitter` is a
*local variable* in the constructor, not a member — reachable in a test only
via `findChild<QSplitter*>()`. Even after finding it and calling
`setSizes()`, every descendant (the splitter itself, and everything inside
it) stayed stuck at Qt's default unconfigured `QRect(0,0 100x30)` — the
resize event posted by `resize()` was never actually delivered/cascaded
without an explicit `mgr.layout()->activate()` call first.

A second, separate instance of the *same* gap: `SceneGroupLooks` (`m_canvas`)
is created fresh *inside* `loadCanvas()`, **after** the splitter above
already had real geometry forced into it — as a brand-new child inserted
into an already-laid-out ancestor's layout, it needed its *own* explicit
`resize()` + `layout()->activate()` too; several more
`QCoreApplication::processEvents()` calls alone were not enough to make its
inserted-widget LayoutRequest actually land.

*Status:* **Accepted** — a test-technique lesson now captured in
`ProgrammingManager_Test`'s shared `openCanvas()` helper (with its own
explaining comment), not a product bug: none of this ever matters for the
real, shown application window, where the normal show/resize event pipeline
handles it without any of this. Worth remembering for any future test that
constructs a `ProgrammingManager` (or any other widget whose splitter isn't
exposed as a member) without showing it.

---

## Programmer/Save workflow

Test: `ProgrammerController_Test` (`engine/test/programmercontroller/programmercontroller_test.cpp`)
— `engine/src/programmercontroller.{h,cpp}`, per CLAUDE.md "all programmer-mode
logic (palettes, group scenes, Save workflow)". This file had **zero test
coverage of any kind** before this pass — no `engine/test/programmercontroller/`
directory existed. Driven entirely through `Doc`'s public forwarders (the real
entry point every caller, including the UI, actually uses — `ProgrammerController`
itself is reached only via `doc->programmer()`, which nothing outside `Doc`'s own
constructor wiring calls directly), covering the smart-save bucketing
(`proposedSaveBuckets()`'s channel categorization and largest/smallest
fixture-group selection), plain scene baking (`saveBucketAsScene()`), the
exact-match lookup (`findMatchingScene()`), and dynamic group-scene building
with its palette/per-fixture-bake fallback (`saveBucketAsGroupScene()`). This
is engine-level, dialog-independent logic — the real UI Save gesture
(`VCButton::saveProgrammer()`, wired to a `SaveProgrammer`-action button press)
is orchestration glue on top (which combo box picked which row's action) that
doesn't compute anything this list needs to protect on its own.

### 19. `saveBucketAsScene()`'s name and path arguments look symmetric, but only one falls back

```cpp
scene->setName(name.isEmpty() ? bucket.defaultName : name);
if (!path.isEmpty())
    scene->setPath(path);
```
An empty `name` argument falls back to `bucket.defaultName`. An empty `path`
argument does **not** get the same treatment — `setPath()` is simply never
called at all, so the new `Scene` keeps whatever its own ordinary constructor
default is (the empty string, which `Function::path()`'s default
non-simplified form renders as just `"Scene/"`), not `bucket.defaultPath`.
Discovered because a test written expecting the two arguments to behave the
same way (pass both empty, expect both to fall back to the bucket's own
defaults) failed on the path half only.

*Status:* **Accepted.** The real UI caller (`VCButton::saveProgrammer()`)
never hits this in practice — its Save dialog always pre-fills the Path field
from `bucket.defaultPath` before the user could see or submit an empty one,
so every real call passes a genuine non-empty path. But any other or future
caller reasonably assuming "blank means use the sensible default" for both
arguments, the way `name` actually works, would silently misfile the scene
into the default folder with no error. `saveBucketAsSceneEmptyPathDoesNotFallBackToTheBucketsDefaultPath()`
pins the current (asymmetric) behavior down explicitly, with a comment
pointing at its sibling test to update if this is ever made consistent.

### 20. `saveProgrammerAsScene()` is dead code with a stale doc comment pointing at it as the real Save path

`ProgrammerController::saveProgrammerAsScene(name)` / `Doc::saveProgrammerAsScene(name)`
is public API, documented, and forwarded by `Doc` — a simple path that always
creates one brand-new `Scene` from every raw programmer value, with no
categorization, no fixture-group awareness, no group-scene option. But it has
**no real callers anywhere in the codebase.** The actual Save pipeline
(`VCButton::saveProgrammer()`) exclusively uses the bucket-based pipeline
(`proposedSaveBuckets()` → `saveBucketAsScene()`/`saveBucketAsGroupScene()`
per row, driven by a real dialog) — yet `vcbutton.h`'s own doc comment on
`saveProgrammer()` claimed it "build[s] a Scene from `Doc::saveProgrammerAsScene()`",
which stopped being true once the bucket pipeline replaced it.

*Status:* **Fixed** — the stale comment. `VCButton::saveProgrammer()`'s doc
comment now describes the actual edited-scenes/bucket-rows dialog flow and
explicitly notes `saveProgrammerAsScene()` is dead code from this button's
perspective. `saveProgrammerAsScene()` itself is left in place, untouched and
untested by this pass — it's still real, working, forwarded public API (just
apparently unused); removing it or deciding its fate is a separate, deliberate
call this pass didn't make.

---
