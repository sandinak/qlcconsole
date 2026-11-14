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

**Run page** (the pad's Run role: nothing moves the show unless LB is held)

| Control | Does |
|---|---|
| Hold LB | Arm (the page says LB HELD) |
| LB + A | GO |
| LB + B | Back |
| LB + Y | Sub GO |
| LB + hold X 1 s | Stop the list (a tap does nothing) |
| LB + RB | Fire the armed hit |
| LB + View | Solo: the first layer of the cue in the preview, or the whole cue again |
| LB + D-pad right / left | Solo the next / previous layer |
| LB + LS (click) | Blind on / off |
| Menu | Controller test |

The last three are the remote's own. The console's controller does not
use View, the D-pad or LS in its Run role, so nothing clashes.

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
  phone Run page** is off. With a PIN, the page asks for it once per
  session. With logins (-wa), the account needs **Only Virtual Console**
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
4. **Run controls.** Back on **/run**, hold LB.
   *Expected:* "LB HELD: GO ARMED" shows. LB + A plays the next cue on the
   console. A on its own does nothing.
5. **Blind.** Put the console in Design. On the Deck, press LB + LS
   (click the left stick).
   *Expected:* the rig goes dark, the console's screens stay live, and
   the console's Blind button lights. The page shows **BLIND ON** and the
   blue BLIND chip. Press LB + LS again: the rig comes back.
6. **Solo.** On the console, pick a cue with two or more layers in the
   Show Tree, so it plays in the preview. On the Deck, press LB + View.
   *Expected:* only the first layer lights, and the page reads **SOLO:**
   and that layer's name. LB + D-pad right moves to the next layer. LB +
   View again plays the whole cue.
7. **Blind and Solo in Operate.** Switch the console to Operate.
   *Expected:* the BLIND and SOLO row disappears from the page. LB + LS
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
    press LB + A on **/run**.
    *Expected:* the blue TEACHING chip shows, and the line under the
    section reads "Teaching: real output held · show control out held:
    Teaching (cue … not sent)". The cue still plays on the console.
14. **VID:PID.** Connect the Deck's controller to the console directly
    (USB, or Steam's virtual pad on a Linux console). Open the console's
    controller list.
    *Expected:* the Deck's controller is listed, with Xbox button names,
    and its VID:PID starts with 28DE (Valve). Report the whole VID:PID.
