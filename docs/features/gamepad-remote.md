# Steam Deck and gamepad remote

A Steam Deck, a phone or a tablet as a Wi-Fi remote for the console: work
a venue focus from the stage with the Deck's sticks, and keep the cue list
and the show's status on a second screen. Nothing is installed on the
Deck. It opens the console's own web pages in its browser.

The Deck is a remote, not a second console. It sends commands to the one
console the same way the phone's [Run page](other-improvements.md#the-run-page-phone-and-tablet)
does, and it is allowed the same things. DMX stays on the console's wired
network. The Wi-Fi only carries small control messages.

## The two pages

- **Focus remote**: **http://&lt;address&gt;:9999/focus**. It follows the
  [Focus mode or Venue focus](focus-calls.md) running on the console. It
  shows the focus calls (each with how many lights are saved), the lights
  in the call, and the light being worked: its name, the point it focuses
  to, the call's note, its pan and tilt correction, beam and focus, and
  whether it is saved. Below that, how many are saved and how many are
  left. Tap a call or a light to go to it; the light blinks on the rig
  (Locate). The page does not start a focus. You start it on the console
  (**Lighting Studio ▸ Focus**), because the venue it saves to is the
  console's question.
- **Run page**: **http://&lt;address&gt;:9999/run**. The cue on now and the
  next one, each with the entries of its Notes pane (`Note: Call the follow
  spot`, ticked ones struck through), with GO, BACK, HOLD and STOP and the
  group masters. It now also
  lists the **Show status** warnings (the footer chip's lines) and offers
  **Focus ›** while a focus runs on the console. On a screen 1280 pixels
  wide or more held sideways (the Deck), it is in two columns: the cues on
  the left, GO on the right.
  In Design it also has **BLIND**, **SOLO** and the arrows either side of
  SOLO (see [Blind and Solo](#blind-and-solo)). In a Teaching show it
  shows a blue **TEACHING** chip and a line saying what the instructor's
  settings hold (see [How it stays safe](#how-it-stays-safe)).

Both pages work on a phone too. Each has a **Controller test** button
(see below).

## Controls

The controller does what it does on the console. A press goes to the
console as the same button, and the console's [controller
scheme](focus-calls.md) decides what it does. Each button on the focus page
is labelled with its job.

**Focus remote**

| Control | Does |
|---|---|
| Left stick | Pan and tilt, coarse (8° a second at full tilt) |
| Right stick | Pan and tilt, fine (1° a second) |
| LT / RT | Previous / next focus call |
| LB / RB | Previous / next light |
| A | Save the light (for the venue applied) |
| B | Back to its last save |
| X | Zero the correction |
| Y | Scope: this light / the whole call |
| D-pad up / down | Zoom narrower / wider |
| D-pad left / right | Focus softer / sharper |
| LS (click) | Target page: move the call's target |
| RS (click) | Say the light (on the console) |
| View | Undo the last save |
| Menu | Opens and closes the controller test (leaving focus is done on the console) |
| Arrow keys | Nudge 0.1° (Shift: 1°); the on-screen arrows use the step shown between them |
| Page Up / Page Down, [ / ], Enter, Backspace | The console's Focus mode keys: call, light, save, revert |

**Run page** (the pad's Run role, by the console pad's rules: the bumpers
arm and disarm, the left trigger is the safety, the right trigger the hit)

| Control | Does |
|---|---|
| Hold LB + RB | Arm; hold them again to disarm (the page says ARMED until then) |
| Armed, hold LT + A | GO |
| Armed, hold LT + B | Back |
| Armed, hold LT + X | Sub GO |
| Armed, hold LT + Y | Sub Back |
| Armed, pull RT | Fire the armed hit (named under ARMED: **Hit ▸ Breathe**) |
| LB + RB + LT + RT | Emergency stop: everything off (the page says EMERGENCY STOP). The page says SENDING STOP until the console answers that it is done, then EMERGENCY STOP. The stop goes even with **Allow GO from the phone Run page** off, and needs no PIN: a page that has not been given the show's PIN still stops it. With the connection gone or the stop refused it says STOP NOT SENT and why (STOP NOT CONFIRMED when no answer came): stop from the laptop |
| Armed, hold LT + View | Solo: the first layer of the cue in the preview, or the whole cue again |
| Armed, hold LT + D-pad right / left | Solo the next / previous layer |
| Armed, hold LT + LS (click) | Blind on / off |
| Menu | Controller test |

There is no "stop the list" on the controller: the page's STOP button
does that (two taps). The pad never picks the list: the page's buttons
and its controller play the list the console's **Run** tab plays (the
one picked there, else the main list), the same list as the console's
controller, and the page's cue on now and next are that list's. A
Companion button or an OSC message that names a list plays that list.

While the page's controller is armed, the line under **ARMED** names the
hit RT fires next, in the console's words: **Hit ▸ Breathe**, **Hit ▸
Breathe off** when the pull lets it go, **Hit ▸ Red (1 of 2)** when two
take turns (the console's controller and the page's share the turn).
Nothing shows when there is no hit to fire.

The last three are the remote's own. The console's controller does not
use View, the D-pad or LS in its Run role, so nothing clashes.

The console's controller has one more state, **Pad: FOCUS** (blue): a
[venue focus cue](focus-calls.md#a-venue-focus-cue-pre-show) started from
it, armed, with LT + Menu. While it is in FOCUS that controller plays no
cue and LT + Menu again finishes the focus (the console's
[Pad Run](show-control.md#go-from-the-game-controller-pad-run) rules);
the list stays on the focus cue until a GO moves it on, from the
console's controller (LT + A), the laptop or this page.

**On the console's own controller, while a follow spot plays** (see
[Moving a follow spot](stage-targets.md#moving-a-follow-spot))

| Control | Does |
|---|---|
| Left stick | Moves the subject, turned to where the operator stands |
| Right stick | The same, finely |
| D-pad up / down | Speed step |
| D-pad left / right | Close / open the beam |
| Hold LS (click), push a stick toward centre stage, let go | Sets which way the operator faces (15° steps) |
| View + LS | Centre Sticks |

## Blind and Solo

Both do what the console's own Blind and Solo do in Design, and both
appear on the page only in Design.

- **BLIND** mutes the rig and keeps the console's screens live, like the
  Blind button on the console in Design. The page shows a blue **BLIND**
  chip while it is on, whoever turned it on. In Operate the console's
  Blind only changes what its own screens show and never mutes, so the
  remote refuses it there: "Blind and Solo are for Design: the show is
  running."
- **SOLO** works on the cue playing in the Show Tree's preview (pick a
  cue in the Show Tree, or press **Play** on its page). Only one of its
  layers plays, as with **Solo** on the Show Tree's cue page. **SOLO**
  solos the first layer, and pressing it again plays the whole cue.
  **‹** and **›** step through the layers. The button names the soloed
  layer (**SOLO: USTs**). With no cue in the preview, SOLO is greyed.
  Picking another cue on the console ends the solo, as it does there.

## Setting up

1. On the console: **Connections ▸ Devices ▸ Show control &amp; sync ▸ Web
   access**. Tick **Start web access with this show** and **Reachable from
   other devices on the network**. The QR code beside **Open:** is the Run
   page's address.
2. **File ▸ Show Settings…**: tick **Allow GO from the phone Run page**.
   The focus remote follows the same switch. Set a PIN if the show
   network isn't only yours.
3. Put the Deck on the same network as the console.

**Desktop Mode.** Open Firefox or Chrome and type the address with
**/focus** or **/run**. Press any button so the browser sees the
controller.

**Game Mode.** In Desktop Mode, open Steam and choose **Games ▸ Add a
Non-Steam Game to My Library…**. Pick the browser (Chrome works best). In
the new entry's **Properties ▸ Launch options**, add `--kiosk` and the
address at the end of what is already there, for example
`--kiosk http://192.168.1.20:9999/focus` with your console's address.
Back in Game Mode, the entry opens the page full screen.

**The back grips (L4, R4, L5, R5).** A browser does not see them. In the
entry's controller layout (Steam ▸ the controller icon ▸ **Edit
layout**), bind them to keys. The page reads these keys, which are the
console's Focus mode keys:

| Grip | Bind to key | Does on the focus page |
|---|---|---|
| L4 | Page Up | Previous focus call |
| R4 | Page Down | Next focus call |
| L5 | [ | Previous light |
| R5 | ] | Next light |

The right trackpad can stand in for the right stick: set it to **As
Joystick** in the same layout.

## How it stays safe

- **The Run page's rules.** Nothing is sent while **Allow GO from the
  phone Run page** is off -- except the emergency stop (LB + RB + LT + RT),
  which a page can always send. With a PIN, the page asks for it once per
  session, even with GO from the phone off, and every command needs it
  except the two stops: the emergency stop and the list's **STOP** never
  need the PIN, and the PIN screen has its own **STOP** (tap twice). With
  logins (-wa), the account needs **Only Virtual Console**
  or more.
- **Moves, not positions.** The page sends how far a stick moved since
  its last message, at most 40 times a second. If the Deck drops off the
  Wi-Fi, the light stops. No stick is left held. The console adds a
  burst up and applies it at most 50 times a second. One message can move
  a light 5° at most.
- **Hold and Exit stay on the console.** A hold (the freeze chord) stops
  the page as it stops the pad. Leaving focus with unsaved lights is asked
  at the console, so Menu doesn't leave.
- **The controller test sends nothing.** While it is open, the controls
  only light up their boxes.
- **A controller that stops answering is skipped.** Controllers are
  opened in the background, two seconds each at most. One that doesn't
  answer (a USB pad switching mode, held busy by the computer) is left
  out and named in the footer, "... not answering"; the other pads, the
  lights and every pad's emergency stop carry on, and the show keeps
  running. The computer may go on holding that controller, and with it
  the USB port or hub it is on: unplugging it may not free it, and other
  devices on that hub may stop working. After the show, restart the
  computer (on a Mac, restart this Mac to free its USB). Until then
  qlcconsole may not finish quitting: the window closes, but the app can
  stay listed as running.
  Opening or closing a device on another line no longer waits with
  every universe held: a device that does not answer while it is
  opened or closed keeps the other universes' DMX running. A USB DMX
  interface that stops answering while it is sending is not covered:
  it can still stop the output, and the app, until the restart. Do not
  re-patch a USB DMX interface on a hub held that way during a show.
- **Teaching mode holds the remote as it holds the console.** A remote GO
  goes through the same path as the console's GO, so the instructor's
  settings ([Teaching mode](teaching.md#instructor-settings)) apply to it
  in the same way. Real output stays held, a GO's show control out to
  QLab, OSC or MSC is held, and with **Hold show control out while Blind
  is on**, a GO after Blind from the remote is held too. The Run page
  says what is held, and which cue's trigger was not sent. The remote
  cannot change any of these settings. Teaching locks neither Blind nor
  Solo on the console, so it does not lock them on the remote either.

## The controller test

**Controller test** (or Menu) opens a panel that shows every button and
stick live: each box lights up while its button is down, a trigger shows
how far it is pulled, and the four stick bars move. It also names the
controller and its layout, the last key pressed (a back grip bound to a
key shows here), and the round trip to the console in milliseconds. If a
button lights the wrong box, the browser has not given the controller the
standard layout. Use the screenshot to report it.

## Controller jobs: more than one controller

Each game controller plugged into this computer has three jobs: **Run
cues**, **Follow spot** and **Focus**. All three are on until you change
them, so a show with one pad works as it always has. With two or three
pads, give each its job: one runs cues, one steers the follow spot, one
walks the stage for focus.

To set them, go to **Connections**, right-click the controller under its
universe's input and choose **Test Controller and Jobs…**. Under the
picture of the controller, **Jobs** has a switch for each job. A change is
saved, and works, at once, with the window still open. While the window is
open the pad runs nothing else: the pad that runs cues is disarmed (arm it
again with LB + RB when you close), but the emergency stop still stops the
show from any pad.

- **Saved with this computer, not the show.** A show opened on another
  computer uses that computer's jobs.
- **Which pad is which.** **Known as** says how this computer tells the
  pad apart: its Bluetooth address, or the USB port it is plugged into
  when it has no address. Two pads of the same model read apart in the
  **Controller** list by that. Press a control on the pad in your hand:
  only the picture of that pad lights. A pad plugged into another USB
  port is another controller to this computer, and a Bluetooth pad on a
  cable is too, so set its jobs again there.
- **Run cues off.** The pad can't arm. Its arm chord, GO, Back, Sub GO,
  Sub Back and the hit do nothing, and the footer says *This controller
  doesn't run cues*.
- **One controller runs cues at a time.** The first pad to arm holds the
  show until it disarms. Another pad that tries is refused and told
  which pad has it (*DualSense (…77:93) is running cues*); the footer,
  which already names that pad, adds *another controller is running
  cues*. Nothing on the first pad changes. With more than one
  pad connected, the footer's **Pad: ARMED** chip names the pad that
  armed, and only that pad's light shows the arm.
- **Follow spot off.** The pad's sticks never move the follow spot; it
  takes the next pad that has the job. Taken off mid-push, whatever the
  pad was pushing lets go: the follow spot stops, it does not drift on.
  The same happens when the follow spot moves to another pad, is bound by
  hand, or the pad is unplugged. Only that pad's push lets go: with two
  pads on the follow spot, their fine sticks add up (to a full push), and
  the other pad's trim stays. The follow spot's settings in the
  Look Editor say where the jobs are set, and say when no pad has the
  job.
- **Focus off.** The pad can't start or work Venue focus (LT + Menu),
  Focus mode, Focus this cue or the walk (Menu, or a swipe), and the
  footer says *This controller doesn't do focus* (see [Venue
  focus](focus-calls.md)).
- **Each job works on its own.** A pad with Focus and not Run cues (or
  any Focus pad while another pad runs cues) starts a waiting venue focus
  with **LT + Menu**, no arm; Menu alone only says *To start the venue
  focus, hold LT and press Menu.* The pad that runs cues **stays armed**:
  the chip reads **Pad: FOCUS ·** and the focus pad's name, only the focus pad's
  light is blue, and the runner's GO, Back, Sub GO, Sub Back and new hits
  are refused with *Venue focus is running on* (the focus pad)*: nothing
  plays until it ends.* A hit already playing keeps playing (a one-shot
  to its end; a toggle until the runner pulls RT to let it go). The focus
  pad ends it with LT + Menu, and the runner's GO works again at once, no
  re-arm (two short buzzes). One pad with Run cues and Focus works as
  before: armed, LT + Menu. A focus started on the laptop leaves a
  runner without Focus armed too, and **Pad: FOCUS** names the first pad
  with Focus to press a button. If the focus pad loses its Focus job or
  is unplugged, the focus runs on and the chip says so: another pad with
  Focus takes it with a press, or finish it on the laptop. After the
  emergency stop a pad can't start the venue focus until a pad is armed
  again (or GO on the computer). The laptop's Focus this cue, Quick focus
  or the Controller Test still disarm the runner.
- **A walk beside the runner.** In Design, a pad with Focus can walk the
  show (Menu) while another pad is armed: the runner stays armed, the
  chip reads **Pad: Walk ·** and the walking pad's name, and the runner's
  GO family is refused (*… is walking the show: nothing plays until the
  walk ends.*) until the walk ends, then works with no re-arm. In
  Operate the walk waits until the runner disarms, and says who runs
  cues. The walk beside the runner also ends by itself, saying why, if
  the show goes into Operate, the role is switched, or the walking pad
  loses its Focus job or is unplugged (a change not kept goes back).
- **The follow spot beside a tool.** A pad with Follow spot keeps
  steering the spot (its sticks, the fine stick, D-pad speed and size)
  while another pad works a tool: Venue focus, Focus mode, Focus this cue
  or look, or the walk. If the tool is working on the follow spot's own
  lights (the focus call, the walk's layer, or the cue or look being
  focused lights one of them, or aims a light at the subject the follow
  spot is following), the spot **holds** instead: it stops at
  once, the follow spot chip in the footer says *Follow spot holds:
  Venue Focus is working on its lights* (or the tool it is), and nothing
  pushed on its pad moves it. When the tool ends or moves off those
  lights it steers again once its stick has come back to centre: a stick
  held through the change never jumps the spot. One pad with both jobs
  gives its sticks to the tool, as before. A tool started on the laptop
  belongs to the first pad with Focus to press a button (not the D-pad:
  on the follow spot's pad that stays the spot's); until then the follow
  spot's pad steers on, and with only one controller connected the tool
  has its sticks at once.
- Buzzes, trigger feel and the names of buttons in a message go to the
  pad they are about: the runner's armed triggers and GO buzz are on the
  runner, the emergency stop buzzes every pad.
  Focus mode is in the Tools role, and Run /
  Tools is switched on the computer (the Run tab's **Pad:** button) for
  every pad: a View + Menu tap in Run on a Focus-only pad says so.
- **A pad that is replaced is a pad lost.** If the pad that runs cues
  re-enumerates, or (on Linux, two pads of one name) another pad takes its
  place on its line because one before it was unplugged, it is disarmed as
  if unplugged: the chip says *Disarmed: the controller went away.*, and
  nothing it was holding counts. Arm again with LB + RB. Another pad
  being unplugged does not disturb the pads that stay: each keeps its own
  universe, its arm, the LT it holds and a stop it is squeezing.
  A spare of the same model that is already connected (on charge, say)
  never takes over an unplugged pad's universe by itself: the notice says
  the universe waits for its own device. When a spare that no universe
  uses is connected, the notice also says how to use it there: unplug it
  and plug it back in, or patch it in Inputs/Outputs. With two universes
  waiting for that model, only the first (the lower number, the one a
  replugged spare goes to) is told; when notices disagree, follow the
  latest. A pad another universe uses is never suggested.
- **The emergency stop is on every pad.** In the Run role, both bumpers
  and both triggers together stop everything from any connected
  controller, whatever its jobs, whatever the pads are doing: a tool or a
  venue focus on another pad, or the Controller Test open.
- **One pad listed twice is one pad.** macOS sometimes lists one
  controller twice; with the same address (or USB port) it is one
  controller here: the Pad chip names no second runner, both get the same
  light, and one press is one action -- one LT + A is one GO, one RT one
  hit, the four one stop, one Menu one walk, one trigger pull one cue
  step of the walk (even when one listing sends the trigger as a button
  and the other as an axis), one D-pad press one follow spot speed or
  size step -- and its fine stick steers the follow spot once, not twice
  as fast. If one of its two listings drops while it works a tool or
  walks beside the runner, the pad keeps the tool or the walk on the
  other: its sticks stay the tool's, never the follow spot's, and nothing
  it has not kept is put back. Two pads of one model told apart by address or USB port
  stay two pads.
- **Each pad times its own arm chord.** Another pad squeezing LB + RB
  while the runner holds its own never cuts the runner's half second.

### Check jobs with two real pads

Two pads (say a DualSense and an Xbox pad), the show in Operate with a
cue list of at least three cues, a toggle hit on the production, and a
[venue focus cue](focus-calls.md#a-venue-focus-cue-pre-show) on a Pre-show
list. Set pad A to **Run cues** only and pad B to **Focus** only.

1. Arm A (LB + RB). *Expected:* **Pad: ARMED · A**, orange, only A's
   light orange.
2. Put the Pre-show list on its focus cue. *Expected:* the Run tab says
   *waiting for the controller — hold LT and press Menu* (no "armed"),
   and the voice says "Hold LT and press Menu to begin."
3. On B, press Menu alone. *Expected:* nothing starts; the chip says
   *To start the venue focus, hold LT and press Menu.*
4. On A, pull RT (the toggle hit comes on). On B, hold LT and press
   Menu. *Expected:* the calls start; **Pad: FOCUS · B**, blue, only B's
   light blue; A's light its own colour; the hit still on.
5. On A, hold LT and press A, then B, X, Y. *Expected:* nothing plays;
   the chip says *Pad: FOCUS · B · nothing plays until the venue focus
   ends* each time.
6. On A, pull RT. *Expected:* the hit lets go ("… off."). Pull again:
   refused with the same words, nothing comes on.
7. On A, hold LB + RB for a second. *Expected:* nothing changes; same
   words.
8. On B, hold LT and press Menu. *Expected:* the focus ends; **Pad:
   ARMED · A** again, A's light orange, two short buzzes on A. On A, LT +
   A: the next cue plays, with no re-arm.
9. Start the focus on B again, then press all four on A (LB + RB + LT +
   RT). *Expected:* **Pad: STOPPED**, both lights red, everything off. End
   the focus on B (LT + Menu): A is not armed. Repeat with the four on B.
10. With A armed, in Design: on B press Menu. *Expected:* B walks;
    **Pad: Walk · B**; A's LT + A plays nothing (*… is walking the show:
    nothing plays until the walk ends.*). Menu on B again: the walk ends,
    two short buzzes on A, A's LT + A plays. In Operate, Menu on B:
    refused, naming A.
11. Arm A, put the Pre-show list on its focus cue, and press **Start
    Venue Focus** on the Run tab (the laptop). *Expected:* A stays armed;
    the chip says **Pad: FOCUS** with no name. Once the calls run, press Y
    on B: **Pad: FOCUS · B**, B's light blue. LT + Menu on B ends it.
12. On A, hold DualSense L2 + Cross to GO (A a DualSense, B an Xbox pad),
    then press Cross alone. *Expected:* the GO buzz is on A only, A's
    triggers are firm while armed, and the chip says *GO is L2 held and
    Cross.* (A's own names).
13. Set A to every job, B to Follow spot only. Disarmed, on A, LT +
    Menu. *Expected:* refused: *Not armed. To start the venue focus, hold
    LB and RB to arm, then LT + Menu.* Arm A, LT + Menu: **Pad: FOCUS**,
    not armed; LT + Menu again: ARMED (as before jobs).
14. The follow spot beside a walk. Set A to **Focus** only and B to
    **Follow spot** only; the show in Design, a follow spot on (its scene
    open in the Workbench, B named as its stick on the footer chip), and
    a cue whose first layer lights other fixtures and whose second layer
    lights the follow spot's. Pick that cue, turn the Run role on and
    press Menu on A (the walk, on layer 1). Push B's left stick.
    *Expected:* the subject moves; the follow spot chip says *stick
    moving* and nothing about holding. B's right stick trims it, B's
    D-pad Up steps the speed.
15. Keep B's stick pushed and press RB on A (the walk's next layer: the
    follow spot's lights). *Expected:* the spot stops at once; the follow
    spot chip says *Follow spot holds: the walk is working on its lights*;
    B's sticks and D-pad move nothing.
16. Still holding B's stick, press LB on A (back to layer 1).
    *Expected:* the hold words go, but the spot does not move. Let B's
    stick come back to centre, then push it: the spot steers again.
17. On A press Menu (the walk ends) while B steers. *Expected:* the spot
    keeps moving with B's stick throughout; nothing jumps.
18. Walk ended, B steering. Start **Focus This Look** on the laptop on a
    look that lights the follow spot's fixtures, then press Y on A (A
    takes the tool). *Expected:* the follow spot chip says *Follow spot
    holds: Focus This Look is working on its lights*; A's sticks work the
    look; B's stick moves nothing. End the look on the laptop, let B's
    stick come back to centre and push: the spot steers.
19. A walking (Run role, Design), B steering: on A, press RT to step to
    the next cue. *Expected:* B's stick and held right stick keep the spot
    moving; A's sticks stay the walk's; a press on a third pad with Focus
    says *Another controller is in use.*
20. Arm A, hold LT, and switch A off and on (or unplug it and plug it
    back) still holding LT. *Expected:* the chip says *Disarmed: the
    controller went away.*; pressing A plays nothing. Arm again (LB + RB),
    hold LT and press A: GO. With three pads of the same kind, arm the
    last one, hold its LT, and unplug the first: the last stays armed, LT
    + A plays, and plugging the first back in gives it its own universe
    again (not another pad's).
21. Set A to Run cues. Disarmed, on A: hold LB + RB, squeeze LT and let it go,
    let go of RB and press it again. *Expected:* nothing arms; the chip
    says *Let go of both bumpers, then hold LB and RB again.* Let go of
    both and hold LB + RB: it arms.

Write down any step that did not give its expected result, with the pads
and how they were connected.

## Latency

On the console itself (loopback), a stick message moves the light in
about 0.2–0.8 ms, and the page sees the new values about 1 ms later. Over
Wi-Fi, add the network's round trip, which the controller test shows. Expect
a few milliseconds on a quiet 5 GHz network, and more on a busy 2.4 GHz
one.
At 40 messages a second, a stick move is on the rig within about
25 ms plus that round trip.

## Hardware checklist

One sitting with the Deck, the console running this build, and a show
with a cue list, two or more layers on a cue, and a Venue focus to start.
On the console, turn on web access and **Allow GO from the phone Run
page** (Setting up, steps 1 and 2). Take the screenshots the steps ask
for and note each result.

1. **Wi-Fi.** Join the Deck to the show network. In Desktop Mode, open
   Firefox or Chrome at **http://&lt;address&gt;:9999/run**.
   *Expected:* the dot by the list name is green and the cue on now and
   next match the console's Run tab.
2. **Controller test.** Tap **Controller test**, then press every
   button, both triggers, both stick clicks, the D-pad, View and Menu,
   and move both sticks.
   *Expected:* the name line reads "Steam Deck…" with "mapping:
   standard". Each box lights while its control is down. A trigger shows
   how far it is pulled, and the stick bars follow the sticks. Take a
   screenshot. Menu closes the test. To check the pad's mapping on the
   console as well, press **Check Mapping** in its Controller Test
   (see [Controller test](lighting-studio.md#controller-test)) and follow
   the prompts: *Expected:* every control ticks ✓ and the summary says
   "20 of 20 match"; copy the log if one reads wrong.
3. **Round trip.** Open **/focus**. With the controller test open there,
   read the round trip line.
   *Expected:* a number in milliseconds. Note it (a few ms on a quiet
   5 GHz network).
4. **Run controls.** Back on **/run**, hold LB + RB for half a second and
   let go.
   *Expected:* "ARMED: LT + A IS GO" shows and stays. Hold LT and press A:
   the next cue plays on the console. A on its own does nothing. Hold
   LB + RB again: the banner goes.
5. **Blind.** Put the console in Design. On the Deck, arm, then hold LT
   and press LS (click the left stick).
   *Expected:* the rig goes dark, the console's screens stay live, and
   the console's Blind button lights. The page shows **BLIND ON** and the
   blue BLIND chip. LT + LS again: the rig comes back.
6. **Solo.** On the console, pick a cue with two or more layers in the
   Show Tree, so it plays in the preview. On the Deck, armed, hold LT and press View.
   *Expected:* only the first layer lights, and the page reads **SOLO:**
   and that layer's name. LT + D-pad right moves to the next layer. LT +
   View again plays the whole cue.
7. **Blind and Solo in Operate.** Switch the console to Operate.
   *Expected:* the BLIND and SOLO row disappears from the page. LT + LS
   says "Blind and Solo are for Design: the show is running."
8. **Focus remote.** Back in Design, start a Venue focus on the console.
   On **/focus**, tap a light in the list.
   *Expected:* that light blinks on the rig.
9. **Sticks.** Move the left stick, then the right.
   *Expected:* the light follows, the right stick finer, with no lag you
   can feel. Let go: it stops at once.
10. **Save and step.** Press A, then RT.
    *Expected:* the light shows Saved and the page moves to the next
    focus call.
11. **Back grips.** In Steam, bind L4, R4, L5 and R5 to Page Up, Page
    Down, [ and ] (The back grips, above). Press each one with the
    controller test open, then with it closed.
    *Expected:* the test's Last key shows "PageUp = LT", "PageDown = RT",
    "[ = LB" and "] = RB". With the test closed, the grips change the call
    and the light.
12. **Game Mode.** Add the browser as a non-Steam game with
    `--kiosk http://192.168.1.20:9999/focus`, using your console's
    address (Game Mode, above). Launch it from Game Mode and repeat
    step 9.
    *Expected:* the page fills the screen and the sticks work as in
    Desktop Mode.
13. **Teaching.** On the console, open a Teaching show (File ▸ Show
    Settings ▸ Teaching mode, instructor settings left as they are) and
    arm and press LT + A on **/run**.
    *Expected:* the blue TEACHING chip shows, and the line under the
    section reads "Teaching: real output held · show control out held:
    Teaching (cue … not sent)". The cue still plays on the console.
14. **VID:PID.** Connect the Deck's controller to the console directly
    (USB, or Steam's virtual pad on a Linux console). Open the console's
    controller list.
    *Expected:* the Deck's controller is listed, with Xbox button names,
    and its VID:PID starts with 28DE (Valve). Report the whole VID:PID.
15. **A pad that switches mode.** This tests recovery from the clone's own
    mode switch, not a qlcconsole control (no pad chord switches Run / Tools).
    Not the Deck: plug the Xbox 360 clone
    pad (a DualSense clone, 045E:028E once switched) into the console by
    USB, patch it, and start a cue list running with the console's
    output on the rig. Hold View + Menu (Create + Options) on the pad for
    a full second, twice.
    *Expected:* the app stays responsive the whole time: menus open, the
    Run tab's clock keeps going and the lights keep running. If the pad
    had armed Pad Run, the Run tab says "Disarmed: the controller went
    away." If the pad doesn't come back within a few seconds, the footer
    names it ("Xbox 360 Controller for Windows (USB …) — not
    answering"), and its details say the show keeps running and to
    restart this Mac to free its USB. Quit the app: the window closes at
    once. If the pad stayed named, the status line says "may not finish
    quitting" as the window closes, and the app may stay listed as
    running until the Mac restarts.
16. **After a pad gets stuck.** With the pad from step 15 named in the
    footer (reopen the app if you quit), and a second pad patched:
    *Expected:* the console's DMX output still runs (a cue fades on the
    rig, the output monitor moves), and the second pad's emergency stop
    (both bumpers and both triggers) stops the show. Restart the Mac
    afterwards and check the clone pad, and anything else on that USB
    hub, works again.
