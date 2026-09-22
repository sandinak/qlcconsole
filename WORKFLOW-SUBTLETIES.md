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

*(A section for Fixture Manager grouping and pipe/stand + crossbar to follow
as those workflow tests are built.)*
