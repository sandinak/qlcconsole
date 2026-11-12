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
| LB + X | Stop the list |
| LB + RB | Fire the armed hit |
| Menu | Controller test |

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

Try this on the Deck and report back, with a screenshot of the controller
test:

1. Join the Deck to the show network's Wi-Fi.
2. Open **/run** in Desktop Mode. Check the cues show, and that the dot by
   the name is green.
3. Open **Controller test**. Press every button, both triggers, both stick
   clicks, the D-pad and View/Menu, and move both sticks. Each box should
   light. Take a screenshot.
4. On the console, start a Venue focus. Open **/focus**. Tap a light; it
   should blink on the rig.
5. Move the left stick, then the right. The light should follow with no
   lag you can feel. Note the round trip the test shows.
6. Press A to save, and RT to go to the next call.
7. In Steam, bind L4, R4, L5 and R5 to Page Up, Page Down, [ and ]. Check
   them in the controller test (Last key) and on the focus page.
8. Add the browser as a non-Steam game with the focus address. Launch it
   from Game Mode and repeat step 5.
9. With the Deck's controller seen by the console directly (USB, or
   Steam's virtual pad on a Linux console), read the VID:PID the console's
   controller list shows for it and report it.
