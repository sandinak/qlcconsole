# Controller jobs

Status: invariants written 2026-11-15, before the build (the operator's
rule: the safety rules first, then the code, then sweeps). Decided by him
2026-11-15 (memory `feedback_operator-control-rules`, "Controller jobs").
Code: `ControllerJobs` (engine/src/controllerjobs.*), the jobs section of
the Controller Test (ui/src/controllertestdialog.*), the checks in
`PadRun` and `ProgrammerController`. Tests: engine/test/controllerjobs,
engine/test/padrunrules, ui/test/padbenchshow, ui/test/connectionstree.

## Why

One game controller does everything: it runs cues, steers the follow
spot and works venue focus. With two or three pads on a show ("we can get
multiple controllers and will likely have them") every pad does all of
it, so a follow-spot op's pad can arm the show and a focus walker's pad
can steer the spot. A job is what one pad is for.

## The jobs

| Job | What it covers |
|---|---|
| **Run cues** | Pad Run's map: the arm chord, GO / Back / Sub GO / Sub Back, the hit, RS (say where we are). (The View + Menu hold switched the Run role until Q251; Run / Tools is now the computer's, the Run tab's Pad: button, and the hold only says so, on any pad) |
| **Follow spot** | the follow spot's sticks (bound from the profile or by hand), its fine stick, its D-pad speed and size |
| **Focus** | venue focus (LT + Menu in and out of FOCUS in Run, Menu in Tools, its calls, "still at <venue>?"), Focus mode (View + Menu tap in Tools), Refine and the walk (Menu, or a touchpad swipe, in Run) |

Each job works on its own (board Q250, 2026-11-16: "assign roles
discretely per pad: focus, followspot, cueing"): nothing a job grants
needs a second job on the same pad. Invariant 11 and the sweep below.

Outside every job, on every pad: the emergency stop (the four together)
and Centre Sticks (View + LS, moves nothing). A Virtual Console widget
patched to the pad's channels is the show's own input mapping and hears
the pad as it hears any input: jobs are about the pad tools, not the
universe.

## Who a controller is

A job is saved for one physical pad, so the identity must survive the
pad being switched off, a rescan, and the pad landing on another input
line or universe. In order:

1. **`mac:aa:bb:cc:dd:ee:ff`**: a Bluetooth address in the device's
   name. The HID plugin names a device "<maker> <product> (<serial>)"
   (`assembleDevicesName()`, plugins/hid/hidjsdevice.cpp); macOS gives a
   Bluetooth pad's address as its serial ("DE-4C-09-77-77-93" or with
   colons). Six hex pairs split by `:` or `-`, folded to lower case and
   colons.
2. **`port:<plugin>:<location>`**: no address. The plugin's
   `inputDeviceLocation(input)` (new, `QLCIOPlugin`; empty by default):
   the HID plugin on macOS gives the transport and IOKit location ID from
   the hidapi path ("USB 14200000"; the path is
   `<transport>_<vid>_<pid>_<location>`), on Linux the joystick's
   `/sys/class/input/jsN/device/phys` ("usb-0000:00:14.0-2/input0"), on
   Windows the HID device path (it carries the port's instance ID). It
   is the USB port: the same pad in another port is another controller.
3. **`name:<plugin>:<name>`**: neither. Two such pads of one model share
   their jobs; the Controller Test says so ("can't be told apart").

decided: the Bluetooth address beats the port, because a pad on Bluetooth
has no fixed port. The same pad on a cable has another identity (its
address is not in a wired pad's name): its jobs are set once per way it
connects. (can change: a pad's serial over USB, when one says it.)

## Invariants

1. **Defaults.** A controller with nothing saved has all three jobs. One
   pad, nothing saved: every control, armed or not, in every tool, does
   exactly what it did before jobs existed (a sweep against the old
   map, not an example).
2. **Per computer.** Jobs are this computer's settings (`QSettings`,
   group `controllerJobs`, one key per identity), never the show's: a
   saved show carries none, and a show opened on another computer gets
   that computer's jobs. A controller set back to all three jobs leaves
   no key.
3. **Identity.** Two pads with different addresses, or no address and
   different ports, are different controllers, whatever their names.
4. **The emergency stop is everyone's.** The four together stop the show
   from ANY connected controller, whatever its jobs, armed or not, a tool
   holding the pad or not, in FOCUS or not, a venue focus waiting or
   owned by another pad, the Controller Test open or not (Pad Run rule 5;
   in the Run role, as before: the Tools role has no stop). It is read
   first, before any owner, tool, job, role or test check
   (`PadRun::watchStop()` at the top of
   `ProgrammerController::slotControllerInputChanged()`), so nothing can
   swallow it (review 0.4.10, H1 and M2). One pad on two universes
   stops once a chord (one ALL OFF, one rumble, one message). A controller with no jobs
   does nothing else: no command, no arm, no light change, no follow
   spot move, no tool step.
5. **Run cues off.** Every Run-map control on that pad does nothing but
   say **This controller doesn't run cues** (the notice, so the footer's
   chip shows it, and aloud): the arm chord, GO, Back, Sub GO, Sub Back,
   the hit, RS. (The role hold is no pad's since Q251.) Nothing on any other pad changes.
6. **One runner.** The first controller to arm holds Run until it
   disarms (its chord, the stop, the pad lost, its Run cues job taken
   away, the role switched, a tool taking the pad); a venue focus it started
   keeps its arm aside, and no other pad arms meanwhile. While it holds
   it, another pad's arm chord, GO family, hit and LT + Menu are refused
   with **<name> is running cues** (the name short: "DualSense (…77:93)";
   aloud and as `notice()`; the footer chip, which already names the
   runner, says "another controller is running cues": `chipNotice()`), and nothing on the runner changes: still
   armed, its held safety still held, its chord timing untouched.
7. **Follow spot off.** That pad's sticks never move a follow spot: the
   follow spot's binding skips it (the next pad with the job, else none),
   a stick bound to it by hand stops moving the spot, its fine stick,
   D-pad speed and size do nothing, and a profile's follow spot button is
   refused. The follow spot's set-up in the Look Editor says which pads
   steer it and where that is set, and when no pad has the job.
8. **Focus off.** That pad cannot start or end venue focus (LT + Menu,
   or Menu in Tools), Focus mode, Refine or the walk (Menu, or a swipe),
   and cannot become the pad that owns a tool (Q135); it is told **This
   controller doesn't do focus**. The pad the focus tools read and name
   (`focusController()`) is one with the job whenever one is here. Its presses while a tool runs are ignored (but the
   stop). Among the pads with Focus, the first to press Start still owns
   the tool; another is told "Another controller is in use."
9. **One status source.** `PadRun::state()`, `stateColour()`,
   `roleText()` and `stateChanged()` stay the only source. With more than
   one pad connected, `roleText()` armed (and in FOCUS) names the runner
   ("Pad: ARMED · DualSense (…77:93)"); the footer chip and the Run tab
   read it, nothing derives its own. Each pad's light shows its own state
   (`PadRun::lightUniverse()`): the runner the state's light, every other
   pad its own colour, every pad red after the stop. One pad: as before.
   The same pad seen twice (one identity on two universes) is one
   controller: counted once (no runner's name on the chip), and told one
   light, the same colour on both lines (review 0.4.10, M1).
10. **Jobs changed live.** Taking Run cues from the runner disarms it (no
   button: rule 3's "with no button at all"); taking Focus from the pad
   that owns a tool lets the ownership go; taking Follow spot from the
   bound pad rebinds. A change is saved at once. Whatever a pad left
   pushed on the follow spot (its stick, fine stick, a facing push) is
   let go when it loses the job or the binding changes for any reason
   (job, rebind, Bind by hand, unplug): its later events are dropped, so
   a push it left would drive the spot on by itself (review 0.4.10, H2).
   The fine stick is kept per pad and per axis; what the follow spot uses
   is the sum of the pushes of the pads with the job, clamped to a full
   push, so a pad letting go takes only its own share away and one pad's
   resting noise never hides another's held push (verify, H-A).
   A venue focus start given up or refused lets its owner go too.

11. **Each job on its own (Q250, option a).** A pad with Focus and no
   arm of its own to give -- no Run cues, or another pad runs cues --
   starts a waiting venue focus in Run with **LT + Menu**, no arm (still
   two controls; Menu alone only says "To start the venue focus, hold LT
   and press Menu."), and ends it with LT + Menu; only the pad that owns
   the focus ends it (Q135). The runner on another pad **stays armed**:
   its GO, Back, Sub GO, Sub Back and new hits are refused with
   **Venue focus is running on <pad>: nothing plays until it ends.**
   (the notice and aloud; the chip, which already names that pad, says
   "nothing plays until the venue focus ends"), its arm chord changes
   nothing and says the same, and when the focus ends it is still armed:
   its GO works with no re-arm (two short buzzes on its pad say so). A
   hit already playing is not cut by the focus start: a one-shot plays
   to its end and lets go by itself, a toggle stays on until the
   runner's own pull lets it go (that pull starts nothing). FOCUS on the
   chip, the Run tab, `roleText()` and the light names the pad the focus
   is on (`focusOwnerUniverse()`: the owner, else the pad whose arm it
   put by); only that pad's light is blue. One pad with Run cues and
   Focus is as before: armed, LT + Menu, the arm put by
   (`m_focusArm`) and back at the end; not armed, refused saying to arm.
   How it starts is said from one source (`PadRun::focusStart()`): the
   voice's "Venue focus ready…", the Run tab and the footer say "armed"
   only when the pads that do focus must arm first, and say when no pad
   does focus. The emergency stop stays first on every pad (4).

## Every gate, swept (2026-11-16)

Every check of the arm (`m_armed`, the arm universe, `isArmed()`), the
runner, the focus owner (`controllerOwner()`) or a job (`hasJob`,
`ControllerJobs::has`), with its job and whether it needed another.

| Where | Gate | Job | Needed another job? |
|---|---|---|---|
| `PadRun::watchStop()` (top of `slotControllerInputChanged()`) | the four, first | none (everyone's) | no |
| `PadRun::handle()`, the four | arm chord, the hit | Run cues | no |
| `PadRun::handle()`, the map | Menu is Focus's, the rest Run cues' | both, apart | no |
| `PadRun::handle()`, FOCUS | GO family, hit, RS, Menu while focusing | Run cues (RS, refusals), Focus (Menu) | **was**: the runner's presses never reached it once the calls ran (they were told "doesn't do focus"). Fixed: routed here, refused with the focus's pad named; a playing hit can be let go |
| `PadRun::focusStartPress()` | LT + Menu with a focus cue waiting | Focus | **yes, fixed**: needed the pad armed, and only Run cues arms. A pad with no arm to give starts it with LT + Menu |
| `PadRun::startFocus()` | puts the arm by | Focus | **was**: put the runner's arm by from any pad. Now only from the runner's own pad |
| `PadRun::toolChanged()`, `focusCueChanged()`, `ProgrammerController::routeFocusInput()` (`isArmed()` → `reset()`) | a tool taking the pad drops the arm | none (the tool) | **was**: another pad's venue focus disarmed the runner. Fixed: not in FOCUS |
| `PadRun::armChordUpdate()` / `armChordFired()` / `setArmed()` | no arm change in FOCUS, one runner | Run cues | no (now says why to the runner) |
| `PadRun::roleText()`, `lightUniverse()`, `chipNotice()` | FOCUS names a pad | (status) | **was**: named the runner. Now the focus's pad |
| `PadRun::triggerState()` | the triggers' feel | (status) | **was**: armed feel on every pad while another pad focused. Now Armed only |
| `PadRun::handle()` → `startWalk()` | Menu walks, not while armed | Focus | not a job, the one runner. Q252: a Focus pad walks beside an armed runner in Design (the runner keeps its arm); in Operate it is refused naming the runner, never "disarm first" |
| `PadRun::swipe()` (`routeFocusInput()` feeds the touchpad) | a swipe starts or steps the walk | Focus | **was missing**: any pad's swipe walked. Now only a pad with Focus is read |
| `PadRun::focusMenu()` | ends the focus | Focus + the owner (Q135) | no |
| `PadRun::jobsChanged()` | a job taken away | each its own | no |
| `PadRun::fireHit()`, `trigger()` | reached from the map only | Run cues | no |
| `ProgrammerController` jobs lambda | owner, follow spot let go | Focus, Follow spot | no |
| `routeFocusInput()`, tool + no Focus | a tool's presses | Focus | no |
| `routeFocusInput()`, Q135 owner claim | first press owns a tool | Focus | no (in Run the pad's start claims it) |
| `routeFocusInput()`, Menu in Tools | starts the focus cue | Focus | no |
| `routeFocusInput()`, "Still at…?" A held | answers yes | Focus + owner | no |
| `routeFocusInput()`, View + Menu | the hold / the tap | none (the hold switches nothing, Q251), Focus (the tap: Focus mode, Tools) | no: the role is the computer's for every pad now; a Focus pad without Run cues tapping in Run is told on the screen where Focus mode is |
| `focusController()` | the pad the focus tools read and name | Focus | **was**: could pick a pad without Focus. Now one with the job first |
| `followSpotTakes()`, the bound sticks, a profile's follow spot button, `refreshStickBinding()`, `combineFineSticks()` | the follow spot | Follow spot | no: never the arm or the runner. **Was**: while any tool ran the follow spot held, every pad's (the tick fed the tool and returned first). Q253 (built, below): beside a tool the follow spot is steered by a Follow spot pad that is not the tool's (`isToolsPad()`), and holds only while the tool works on its own lights (`toolOnSpotLights()`); a twin line's fine stick is one share (`fineSlot()`) |
| `sendPadFeedback()`, `padRumble()`, `setPadTriggers()`, `padFamily()` | which pad is told | (feedback) | **was**: the tools' pad (`focusController()`, a Focus pad first) for every buzz, trigger feel and control name -- the runner's armed triggers and GO buzzes went to the focus pad. Now the acting pad, else the state's pad (review 1) |
| `ui/src/controllertestdialog.cpp` | the test takes the pad, disarms the runner | none | no (documented: arm again on close); **was**: not beside a venue focus -- now in every case (review 9) |
| `ui/src/app.cpp` ⌘↩ with a focus cue armed | the laptop starts it | none | no |
| `ui/src/monitor/focusmodepanel.cpp`, `refinepanel.cpp` | name the owner | (status) | no |

### Rules added by the review of a430351ef (2026-11-16)

Written before the fixes, every caller swept against them:

- **Feedback goes to the pad whose action it is.** A buzz or a control's
  name while a pad's input is handled is that pad's
  (`ProgrammerController::feedbackPad()`: the acting pad); otherwise the
  pad the state is on (`statePad()`: the runner armed, the focus's pad in
  FOCUS, a tool's owner -- the light and the triggers always follow it);
  otherwise the tools' pad, as with one pad. The arm's firm triggers are
  the runner's, every other pad's off. The emergency stop buzzes every
  pad (`kAllPads`, once per physical pad). The re-arm at a focus end
  buzzes the runner, once. `focusController()` keeps its Focus-job
  preference: it is who the focus tools read, not who gets feedback.
- **A tool taking the pad drops the runner's arm, whoever started the
  focus.** Only the venue focus itself (or a walk beside the runner,
  Q252) leaves the runner its arm; the laptop's Focus this cue / look,
  Quick focus and the Controller Test disarm it, during a focus or a walk
  or as it ends (`PadRun::toolOverFocus()`, `focusCueChanged()`,
  `walkEnded()`), and it is never told GO works again.
- **The four's bits are never forgotten while the stop reads them.** A
  tool, the test or the walk taking or giving back the pad forgets what
  is held but LB, RB, LT and RT (`PadRun::forgetHeld()`: the arm chord
  spent until both bumpers are up); only a role switch, where the four
  were not read, clears them.
- **Who has the venue focus.** A focus begun on the laptop puts the
  runner's arm by only when the runner can work it (Focus); otherwise
  the runner stays armed beside it and FOCUS is unnamed until a pad with
  Focus takes it with a press (Q135), then names that pad (its light,
  `focusOwnerChanged()`). If the focus's pad loses the Focus job or is
  unplugged mid-focus, the focus runs on with no pad and says so ("Venue
  focus has no controller now: a controller with the Focus job takes it
  with a press, or finish it on the laptop."); only a pad with Focus
  takes it, and the runner's arm is put by then only if the runner can
  work it (a runner with Focus: at once, as for a laptop start). A pad
  that held View as the focus began and let it go meanwhile has let go
  (the held buttons are kept before any early return).
- **After the emergency stop** a pad's LT + Menu starts no venue focus:
  "Emergency stop: arm a pad (or GO on the computer) before the venue
  focus." (decided as the safe default, can change). The laptop still
  can.
- The words of a press refused beside the focus (or a walk) go when it
  ends; the Run tab's focus-start line follows a job edit or a pad
  plugged in or out as the footer does.

### A walk beside the runner (board Q252, option a)

A pad with Focus presses Menu while another pad is armed: in **Design**
it walks (Quiet Refine's Design side) and the runner **stays armed**, as
beside a venue focus -- its GO family and new hits refused with
"<walker> is walking the show: nothing plays until the walk ends." (the
chip, naming the walker: "Pad: Walk · <walker> · nothing plays until the
walk ends"), its arm chord changes nothing, a hit left on is its own to
let go, and when the walk ends its GO works with no re-arm (two short
buzzes on its pad). In **Operate** the walk is refused: "<runner> is
running cues: in Operate the walk waits until it disarms." The walker
owns the walk's tool from its start (Q135); the walk's triggers and
light are its own. The emergency stop is live from both. The Controller
Test or the laptop's Focus this cue mid-walk disarms the runner. A swipe
still starts no walk while a pad is armed (unchanged). Hits fire in
Operate only, so a walk beside the runner never meets a new hit; one
already on plays on (the walk stops nothing).

A walk beside the runner ends by itself, the runner's GO live again at
once and the reason on the screen (and any change not kept put back,
said), when the show goes into **Operate** ("Walk ended: the show is in
Operate and <runner> runs cues."), the **role** is switched, the walker
**loses its Focus job** ("Walk ended: <walker> no longer does focus.") or
the walker **goes away** (unplugged, or its input moved: "Walk ended: the
controller walking it went away.") -- as a focus left with no pad is said
(second review of 46cfdd708). A runner's swipe beside a walk or a focus
steps nothing and is refused as its other presses are.

A runner's arm put by is not ownership: any pad with Focus may take an
ownerless venue focus (a pad unplugged and back takes its focus again),
and FOCUS names no pad until one does.

Already true before the build: the stop from either pad (`watchStop()`
first); a playing hit not cut (a walk starts or stops no function).

### The follow spot beside a tool (board Q253, option a)

Decided by the owner 2026-11-16: "the follow spot keeps steering during
another pad's tool unless that tool works on the spot's own lights; then
it holds, and the chip says why."

- **The tool's pad and the others.** While a tool runs (Quick Focus /
  Focus mode, Venue Focus, Focus This Cue, Focus This Look, the walk), the
  tool's pad is its owner (Q135; the walk's pad owns it from its start, a
  Menu or a swipe), or the owner's twin line (`isToolsPad()`). A tool
  begun on the laptop is no pad's until a pad with Focus presses: with
  one controller here that pad is the tool's at once, as before; with
  more, the follow spot's pad keeps steering until a press takes the
  tool. Its sticks are the tool's (`m_toolPanNorm`..., apart from the
  follow spot's). A walk's cue step (RT, LT, a swipe) restarts Focus this
  cue on the next cue: that stop is no end, so the walker stays the
  owner (`PadRun::isRestarting()`; review of f16599314, blocker 1).
- **Another pad with Follow spot steers on**: its bound stick, its fine
  stick, D-pad speed and size and the facing push, as with no tool
  (`routeFocusInput()`, before the beside-the-runner and owner checks);
  the controller tick runs the follow spot after the tool's
  (`applyDesignJoystick()`). Its other buttons are as before beside a
  tool (the Run map's refusals, "Another controller is in use.", "This
  controller doesn't do focus.").
- **Unless the tool works on the spot's own lights**: the fixtures of the
  follow spot's scene (its groups too) meet the tool's live set -- the
  venue focus's current call, Quick Focus's list, the walk's current
  layer, every layer of the cue (or every scene) Focus This Cue / Look
  steps through (`toolOnSpotLights()`; both sets cached, the tool's
  worked out again only when its call, cue, layer or session changes, the
  spot's when its scene changes, and both once a second for an edit
  meanwhile). **Its lights include the ones tied to its subject**: when
  the follow spot follows a stage target and the tool's live looks aim
  at that target -- a focus reference or call target, an Aim look of the
  cue, its layer or the scene -- moving the subject would re-aim those
  lights under the operator, so the spot holds for them too (decided, the
  conservative reading of the owner's answer: hold whenever moving the
  spot would move the tool's lights). Then the spot
  **holds**: what was pushed on it is let go and it stops at once (no
  drift, no velocity kept for later), nothing a pad sends moves it, and
  its one status (`FollowSpotStatus::heldBy`) says why. The mover is
  stopped before the data channel is told, so a free-fly script hears no
  speed through the hold (no stale `vx` / `vy`); the channel is told even
  when the sticks were already centred and the spot only gliding -- the footer's
  follow spot chip, the Look Editor's line and the rig view's tip read
  "Follow spot holds: <tool> is working on its lights" (no second
  source).
- **No jump after.** When the tool ends or moves off those lights (another
  call, another layer), the spot steers again from zeroed sticks: an axis
  that was deflected when the spot stopped taking it (the hold, or its pad
  becoming the tool's) moves nothing until it has read centre once
  (`m_panLatched`, `m_tiltLatched`, `m_fineLatched`). A stick held through
  the change never moves the spot.
- **One pad with both jobs** (or its twin line): the tool has its sticks,
  as before; what it had pushed on the spot is let go when the tool
  begins or the pad takes it, and the same no-jump rule applies at the
  tool's end. A tool starting (or a walk's cue step) touches only the
  tool's sticks: another pad's fine stick held beside it stays.
- **One pad seen twice** steers once: a twin line's fine stick is that
  pad's one share (`fineSlot()`), never added to itself; the owner's twin
  line is the owner (the owner check uses `sameController()`, as
  `isToolsPad()` does).
- **The tick beside a tool.** The controller tick now runs the follow
  spot's own part while a tool runs: `joystickScene()` (a running cue's
  follow spot found, `endLiveFollow()` and its restore glide when that
  cue ends), `advanceRestoreGlide()`, `updateFollowSpotStatus()` and the
  once-a-second stick search. Before, all of it waited for the tool's
  end; a glide or a cue change during a tool now happens on time.
- **Another device behind a patch is a pad lost (rule 3: "the pad lost
  disarms").** Each input patch's device is known by an id its plugin
  never gives another device (`QLCIOPlugin::inputDeviceId()`; the HID
  plugin's, kept per device: the same whatever line it moves to). When a
  plugin's configuration changes, after the patches have followed their
  devices to new lines, a patch whose device id changed -- a pad
  re-enumerated at its path, or another pad shifted onto its line when
  one before it went (Linux joysticks with no location) -- is treated
  exactly as an unplug followed by a plug (`ProgrammerController::
  checkPadDevices()`, `padLost()`): what the old device read and the
  input patch had not delivered yet goes out first, in order
  (`InputPatch::deliverNow()`: a stop press of the old device counts);
  then an arm made on it drops, saying "Disarmed: the controller went
  away." on the chip; a tool it owned runs on with no owner; what it
  pushed on the follow spot lets go; and nothing it held is held -- the
  Run role's four and the stop's latch (on its twin lines too), the
  chords and buttons, the walk's modifier and level. The device there
  now starts from nothing: it arms with its own two controls, and GOs
  with its own LT + A. A pad that only moved lines (the same device id)
  is told nothing: it keeps its arm, its held safety, a chord or a stop
  squeeze in progress, and every reading. A plugin that can't tell a
  device (id 0) is concluded nothing about, as before. A patch follows
  its device by id (`InputOutputMap::followInputDevices()`), and never
  takes a line whose device another patch follows: two pads of one name
  each keep their own universe. A patch whose device is gone follows the
  device that replaced it in place, as its plugin says
  (`QLCIOPlugin::inputDeviceReplacement()`: the HID plugin's same-path
  re-enumeration) -- a pad lost and a new one, on its own universe; never
  a device by its name (a spare of that name on charge is not the pad that
  went). Otherwise it waits offline, and the pad plugged back reopens it:
  a line of its name that no other patch follows and whose device was not
  already here when it went, nor re-enumerated from one that was
  (`retryOfflinePatches()`, `Universe::OfflinePatch::notTheDevice`). A pad
  already connected never takes over by itself (it may not be the same
  pad): the notice says the universe waits for its own device, and --
  only when that other pad is no universe's (`InputOutputMap::
  hasFreeInputDevice()`), and only for the first universe in order that
  waits for that name (the one `retryOfflinePatches()` gives a replugged
  spare) -- that it takes over once unplugged and plugged back in, or
  patched in Inputs/Outputs. Another universe's pad is never offered. An
  "any game controller" slot follows its pad's device id to whatever line
  it moved to (never a spare that moved onto its old line), and a line is
  another universe's by the device that universe's patch follows, not by
  the line index it was patched to (`slotDeviceLine()`): two "any" slots
  keep their own pads when a pad before both goes, though the later one is
  looked at after the earlier has moved. A plugin with no ids keeps the
  line-and-name rule, as before. Every value is read with the roles its device had
  (`ProgrammerController::controllerRole()` learns a universe's roles while
  its device is known, and again at once after a patch, device, profile,
  slot or configuration change, once what was read has been delivered,
  never later from whatever device is there then): a press
  delivered after its pad was unplugged, or after another model took its
  line, is never read with another layout. Before any patch is set again or let
  go (a re-patch, an unplug, a controller slot's change), what it read and
  has not delivered goes out first, with the patch it was read on
  (`InputOutputMap::setInputPatch()` calls `InputPatch::deliverNow()`
  with no lock held), and a frame flushed on a universe thread moves from
  the buffer to the delivery queue in one locked step. In the HID plugin
  each value carries its device's id, so a value read before its device
  moved to another line is delivered on its line then, and a gone
  device's value goes nowhere (`HIDPlugin::slotDeviceValue()`). A button's
  release with no press seen since its pad was new is ignored (never a
  View undo or a stick click). decided (inherent): three of the four
  held on the pad whose device changed are gone with it -- the new
  device must squeeze the stop afresh (a full squeeze held across the
  change on that pad has stopped already, and the next full squeeze stops
  again). Its twin lines' latches are cleared too, so a twin line still
  reading all four fires the stop again on its next reading -- a second
  stop, which errs the safe way. Known limits (round-5 review): an "any
  game controller" slot (`InputOutputMap::applyInputSlots()`) keeps its
  own pad by device id while that pad is here, but once its pad is gone
  it takes the first free line of any game controller -- a pad by its
  kind, not the one it had (as a pad lost and a new one: disarmed); and a
  named slot device always takes the first line of its name, so it never
  reaches the second of two pads of one name. Not built here. Fixed here:
  an "any" slot no longer keeps a line another universe's patch is on (a
  manual patch and an "any" slot on one pad); a slot moving to another
  device is a pad lost (by device id, never "the same name"); and two
  "any" slots no longer swap pads when a pad before both is unplugged
  (lines taken by device id, `PadTraces_Test::twoAnySlotsKeepTheirOwnPads`). Known
  limits with no stable identity for a pad (Linux joysticks and
  same-model macOS pads without one): a spare plugged in while B is away
  and before B returns is taken for B; and when every pad sleeps at once
  the pads of one name come back in the order they wake, each universe
  taking the next. The way forward is a device serial
  (`HIDFound::serial`) as the identity where the pad gives one -- not
  built here.
- **The restore glide waits for a tool.** A cue's follow spot ending
  glides its subject back where the show has it; while a tool's lights
  aim at that subject the glide waits where it is, and goes on when the
  tool ends or moves off them, from where the subject is then, over the
  whole fade (`advanceRestoreGlide()`).
- Unchanged: the emergency stop is read first on every pad (4), the arm
  takes two controls, the runner's GO family rules beside a focus or a
  walk (11, Q252), and one status source per surface (9).

decided (by the builder, can change): the live set of Focus This Cue /
Look started on the laptop is the whole cue (every layer it can step to),
the walk's only its current layer; with one controller a laptop tool has
the sticks at once (invariant 1); a beside pad's D-pad is the spot's, so a
press that is to take an ownerless tool is any other button.

## Where it is set

Connections ▸ right-click the controller ▸ **Test Controller and
Jobs…** (it was Test Controller…; no new item). The Controller Test shows,
under the picture of the controller picked, **Jobs on this computer**:
three switches (Run cues, Follow spot, Focus) and **Known as** (the
Bluetooth address or USB port). The picker lists each controller with
that identity, so two identical pads read "DUALSHOCK 4 · Bluetooth
…77:93" and "DUALSHOCK 4 · USB port 14200000"; pressing a control lights
the picture of the pad in your hand only. While the test is open the pad
runs nothing but the emergency stop, which still stops the show from
any pad. Opening it takes the pad as a tool does, so the runner is
disarmed; the window says so ("arm it again with LB + RB when you
close"). A job applies on each tick of a switch, at once, with the
window still open.

## Tests (sweeps, not examples)

- Two pads, every job mix of the second (eight), every control alone and
  in the chords on it, the first armed and not: what it produces is
  exactly its jobs' (engine/test/padrunrules
  `jobsEveryControlOnASecondPad`).
- One runner: pad A arms, pad B's every control; then A disarms and B
  arms.
- The stop from a pad with no jobs, armed elsewhere and not.
- Identity for identical pads (same name, two addresses; same name, two
  ports; neither).
- One pad, nothing saved: the sweep equals the pre-jobs sweep.
- Settings: written, read back by a new store, absent from a saved show.
- Each job on its own (Q250): the runner with Run cues only, the other
  pad each of the eight mixes on either line -- the focus started and
  ended with LT + Menu and no arm, Menu alone refused, the runner armed
  throughout and its GO family refused then working
  (`jobsVenueFocusFromAFocusPad`); the stop from either pad in the other's
  FOCUS; one pad with both jobs as before; a toggle hit across the focus
  start (engine/test/padrunrules) and a one-shot (ui/test/padhit); the
  chip, button and lights (ui/test/padrunrules
  `focusBesideTheRunnerOnEverySurface`).
- The follow spot beside a tool (Q253): every tool x its lights inside the
  spot's or not x the spot on the tool's pad or another, on the real
  follow spot path (the subject's motion): steers or holds, the status
  words, the sticks zeroed on hold, no jump at the tool's start or end or
  from a stick held through it (engine/test/followspotfeel
  `q253TheSpotBesideEveryTool`); one pad seen twice steers once
  (`q253ATwinPadDoesNotDoubleSteer`); lights aimed at the spot's subject
  hold it, every tool (`q253TargetTiedLightsHoldTheSpot`); a free-fly
  spot's script hears no speed through the hold
  (`q253AFreeFlySpotHoldsStill`); a walk's cue steps keep its owner and a
  beside pad's fine stick (`q253AWalksCueStepsKeepItsOwnerAndTheSpot`);
  the footer's chip as drawn, at three sizes
  (ui/test/padrunrules `q253TheFollowChipSaysWhyItHolds`); another
  pad's hot-plug keeps what a pad holds (engine/test/padrunrules
  `aHotPlugKeepsTheOtherPadsHeld`); the
  restore glide waits for a tool aimed at the subject
  (`q253TheRestoreGlideWaitsForTheTool`); another device behind a patch is
  a pad lost (`aChangedDeviceIsALostPad`, `noStopOnAnArmAfterADeviceChange`,
  `aBufferedStopPressCountsBeforeTheReset`,
  `aReleaseWithNoPressSeenDoesNothing`; the chip as drawn,
  ui/test/padrunrules `aChangedDeviceDisarmsOnTheChip`); a device's id
  follows it (plugins/hid/test `aDevicesIdFollowsTheDevice`); through the
  real HID plugin, a pad shifted onto another's line disarms it and
  another pad's drop leaves the runner as it was, with and without
  locations (engine/test/padtraces `aShiftedLineIsNoSafety`,
  `anotherPadsDropLeavesTheRunnerAsItWas`, `aPadReenumeratedKeepsItsUniverse`,
  `aPadUnpluggedAndBackTakesNoOtherPad`); a queued value follows its
  device (plugins/hid/test `aQueuedValueFollowsItsDevice`).
