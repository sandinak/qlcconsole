# Calibration offsets and Focus mode — design

Two related pieces of work, from Branson:

> "If I set an offset on a fixture, that is to fix actual alignment because of
> the fixture itself, but the fixture aim on the rig view should remain
> consistent with the intended position on the stage. Also long term I'd like
> to be able to use the xbox controller to iterate across lights on the stage
> using the bumpers, where they highlight, and then I can use the joystick to
> update the offsets in place, and then save using the buttons so the show is
> then aligned."

## 1. Calibration offsets are invisible to the views

### The model

A fixture's **pan offset** and **tilt offset** (`FixtureRigProps::panOffsetDeg`
/ `tiltOffsetDeg`, edited in the fixture's Rig box and now in Focus mode) say
how far the *physical* head is out: the yoke was bolted a few degrees round, the
tilt encoder homes a little high. They correct the hardware, not the design:

- **DMX includes them.** `AimSolver::solve()` adds them after the target
  geometry, so the real light lands where the design says.
- **Every view removes them.** Turning live DMX back into "where is it
  pointing" undoes them, so a head with +3° pan offset aimed at the Main Drape
  target draws its beam on the target, not 3° off. What the views show is the
  light on the stage, assuming the offset exactly cancels the fault — which is
  the point of setting it.

That holds for every way DMX reaches a head: an Aim look (solved), a follow
spot, a line target, and a Standard Pan/Tilt look or raw programmer values
(not solved — the view still subtracts the offset, which is where the physical
light actually goes for that DMX).

**Pan zero (Facing) is not calibration.** `panZeroDir` is which way the yoke's
base faces on the rig — geometry, like position and mounting. It is applied
both ways (the solver turns the bearing into pan with it, the view turns pan
back into a bearing with it), and a view showing the beam must use it. It is
not "removed"; it is part of the intended picture. Invert flags and standing /
hanging are the same: geometry, applied both ways.

### One function

`AimSolver::beamDirection(rp, panCentred, tiltCentred)` is the inverse of
`AimSolver::solve()`, step for step: mirrors (invert, standing), then the
calibration offsets, then the mounting. `AimSolver::intendedAngles()` is the
same up to the point where the offsets are gone, for views that draw angles
rather than rays. Everything that turns DMX into a direction goes through
them:

| Where | Before | Now |
|---|---|---|
| `fixtureAimDirection()` (Rig Overview, Studio beams/aim traces, CPU and GPU, plot footprints, occlusion of a live beam) | its own copy of the inverse | calls `AimSolver::beamDirection()` |
| Plot pan/tilt gauges (`MonitorFixtureItem`) | raw DMX degrees: offset and invert shown | `AimSolver::intendedAngles()` |
| Fixture properties' "Test orientation → Aim at target" | a third copy of the forward math, 180° out and ignoring standing heads | `AimSolver::solve()` |
| Reach warnings, line of sight, look editor aim readouts, follow spot targets | computed from target geometry, never from DMX | unchanged (offsets only eat into the reach margin, which is physically right) |

## 2. Focus mode

### On the follow spot's machinery: one joystick path

Branson: *"the offset tool can work like the follow spot if it can push back
updates?"* Two ways to do that were weighed:

- **(a) a "Focus" effect script** with a write-back API
  (`fixture.setCalibrationOffset()` / `commitCalibration()` queued to the main
  thread) plus button events delivered to the script; or
- **(b) a C++ mode that drives the follow spot's own input path.**

**Chosen: (b).** The follow spot's stick handling is not mostly in
`followspot.js`: when a scene has a target (the case that matters on a real
rig) the script *defers*, and the stick is handled host side —
`ProgrammerController` holds the bound axes (the HID profile's `pan`/`tilt`
slots, auto-bound), takes the profile's **deadzone** and **sensitivity**, and
integrates the deflection on its **50 Hz joystick timer** (velocity
proportional to deflection past the deadzone, so a held stick keeps moving and
a released one holds). Focus mode plugs into exactly that: while it is on, the
same timer tick hands the same stick reading — same deadzone, same
sensitivity, the same velocity curve as `followspot.js`'s `axisVelocity()` —
to the focus session instead of to the aim target. There is one joystick
path; Focus mode is a different consumer of it, as the follow spot is.

Why not (a):

- **The script tenets** (POSITION_ARCH.md): `tick` is a pure function of its
  arguments — no host handles, no side effects. A script that writes the rig's
  calibration breaks that, and needs a queued, thread-hopping write API
  (scripts run on the DMX thread) just to make it safe.
- **Ownership**: an effect lives in a scene; focusing is done to the rig. A
  scene would have to carry a Focus effect to focus, and the effect would
  still need host-side selection, undo, a save/discard prompt and the HUD —
  which is all of (b) anyway.
- **Safe, undoable writes come free in (b)**: the session runs on the main
  thread, so a nudge, a save and an undo are ordinary main-thread edits to
  `MonitorProperties`, with nothing to queue.

The DMX is a `DMXSource` like Highlight and Park, so it overrides whatever is
running for the heads it uses, without touching any function.

### What it does

- **Enter** from the Lighting Studio toolbar (Focus), the Rig Overview bar
  (Focus), View → Focus Mode… (⇧⌘F), or by holding **View + Menu** on the
  controller. A short setup asks which fixtures, in what order, what to aim
  at, and what the others do.
- **Design mode only.** In Perform it refuses; in Design it warns when
  functions are running (Focus overrides the heads it uses, nothing else).
- **References.** Each head is aimed at its reference — the point it should
  hit:
  - *the selected scene's aim* (default when the scene has an Aim look): each
    head's own point from `AimSolver::sceneAims()`, so a line target gives
    every head its own place on the line;
  - *a stage target*: point targets aim every head at the point; a line target
    spreads the heads along it in focus order;
  - an optional **second reference** (Y toggles to it) to check the same
    offset at a second place across the stage.
  Heads with no reference (no rig position, not aimable) are left out.
- **The current head** is lit (configurable level, white, shutter open) and
  aimed at its reference with its **live** offset; it is ring-highlighted in
  the Lighting Studio plot and the Rig Overview. The **others** in the list are
  dark, dimmed, or left as they are (setup choice).
- **Order**: by stage position (front to back, then house left to right) or as
  selected (the group's own order).

### Controls

| Action | Controller | Keyboard |
|---|---|---|
| Next / previous head | RB / LB | `]` / `[` (also Tab / Shift+Tab) |
| Nudge offset (coarse, accelerating) | left stick | Shift+arrows (1° steps) |
| Nudge offset (fine) | right stick | — |
| Step offset by the step size (0.1°) | D-pad | arrows |
| Save this head's offset | A | Enter |
| Revert to saved | B | Backspace |
| Zero the offset | X | 0 |
| Toggle reference A / B | Y | R |
| Undo the last save | View (Back) | ⌘Z |
| Exit (asks if unsaved) | Menu (Start) | Esc |
| Enter Focus mode | hold View + Menu | ⇧⌘F |

Sticks: the follow spot's response — the HID profile's deadzone and
sensitivity, velocity in proportion to deflection past the deadzone, integrated
at 50 Hz — scaled for alignment work: full deflection is 8°/s on the left stick
and 1°/s on the right (both × sensitivity), where the follow spot's free fly
covers the whole range in a second.

Stick right = more pan offset, stick up = more tilt offset: a nudge always
turns the head's own pan/tilt the same way, so on the rig "push towards where
the light should go" may feel mirrored on an inverted or standing head. The
HUD shows the numbers either way.

### Saving

- Nudging writes the live offset straight into the rig (so the solver and the
  views both use it immediately) but **does not** mark the show modified.
- **A** makes it the saved offset: one step on Focus mode's undo stack, marks
  the head checked, marks the show modified. File → Save persists it.
- **B** puts back the saved offset, **X** zeroes the live one (unsaved until
  A).
- **Exit** with unsaved heads asks: Save all / Discard / Cancel. Discard puts
  every head back to its saved offset.
- After exit, the whole session is **one** step on the Lighting Studio's own
  undo (⌘Z there puts every offset back as it was before Focus mode).
- HUD: head name and number (*N of M*), reference, live and saved offsets,
  step size, saved/unsaved, *K of M checked*, the button legend.

### Controller mapping

Buttons and axes are matched by **role**, a new optional `role` attribute in
`.qxhid` profiles (`a b x y lb rb back start ls rs guide dpad_up dpad_down
dpad_left dpad_right` for buttons, `lx ly rx ry lt rt` for axes). A profile
without roles still works if its controls are named the usual way ("A",
"LB", "Back"/"View", "Start"/"Menu", "D-pad Up", "Right Stick X"…), so any
gamepad mapped like the Xbox one works. The left stick is the axis pair the
programmer already binds for the joystick (the profile's `pan`/`tilt` slots).
The Xbox profile gains roles and the D-pad (buttons 11–14), and the macOS raw
Xbox reader now reports the D-pad and orders its axes the way the profile
names them.

While Focus mode is active it owns the controller: button actions
(chaser step, follow spot toggle) and the design joystick are suspended.

### Not in scope

- Per-head offsets for multi-head movers: offsets are per fixture, as stored.
- Saving offsets into a separate "calibration" file: they live in the show,
  as they always have.
