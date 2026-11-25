# Control surfaces

A control surface is anything with buttons, faders or encoders that is not
the mouse: a MIDI desk (an APC, a Launchpad, an X-Touch Mini, the PMJ), an
OSC app on a tablet, a Stream Deck through Companion, a game pad. This page
says what can hear a control, how to set each kind up, and the rules that
keep a brushed button from changing the show. Everything here is saved with
the show except what is said to be the computer's.

## What hears a control

**All of them are listed in one place**: **Connections ▸ Devices ▸ Show
control &amp; sync ▸ Control surfaces**. Each line says what it is and what is
set up now (*3 triggers learned*, *Emergency stop: not learned*, *1 pad
connected and set up*, *Companion: web access off*), and its **Set Up…** button takes you
to the place in the table below. It reads the show as it is and keeps no
settings of its own, and it follows a device plugged in or pulled out at
once. A controller that is plugged in but on no universe's input is named:
a pad on the **Game pads** line (*DualSense: connected, not set up*), any
other MIDI or USB device on **Other connected devices** (*USB MIDI Device:
not set up*); **Set Up…** there goes to the patch. The row is there only while one is waiting, and software ports (Midi Through, IAC Driver, PipeWire and JACK bridges, loopMIDI) are not counted. The **PMJ Black 1** line
counts only an input with exactly that profile, and says whether its
feedback line is patched (without it the board's lamps stay dark). The
**Grand Master** line's **Set Up…** opens the Virtual Console's Settings on
the Grand Master page, where its input is chosen. With **Control Surfaces** off for the show (see the
emergency stop below) the triggers and the encoder bank lines say so and
their buttons are off; the emergency stop line stays.

**One job per control.** A control (a MIDI note or CC, an OSC message, a
computer key) that one part of the program already listens to is refused
when you learn it for another, naming the owner: *Note 60 already does
Button "House lights" (Virtual Console). One job per control.* Press
**Move It Here** to take it from there and give it to what you are
learning (the old owner stops reacting at once), or **Cancel** to leave it
where it is -- Cancel changes nothing, on either side. Move It Here takes
the control from every owner or from none: if one of them cannot let go,
each keeps it. Every Learn says this the same way: Assign to Control…, the
emergency stop, the encoder bank (a Learn, a preset, Custom turned back on
or another universe: each hands the bank a run of controls, and each
taken one is named with its own control), a widget's External Input
(auto-detect or Choose…), a speed master's Inputs…, the Grand Master's
input and a channel group's input. In a window with **OK** (a widget's
Properties, a speed master's Inputs…, the Virtual Console's Properties, a
channel group) Move It Here takes the control when you press OK: Cancel on
that window leaves it with its old owner. In a speed master's Inputs… its
Tap, Rate and Pause are asked of each other as they stand in the window,
so two of them never end up on one control. Move It Here is not on Undo:
to put a control back, learn it there again. Three exceptions, with no
Move It Here: the PMJ Black 1's controls that do something come with its
profile; the encoder bank's encoders, pushes and bank buttons are set up
as runs (one taken would take the rest of its run with it: change the
bank's controls first -- its Next and Previous buttons can be moved); and
nothing takes the **emergency stop's** control (the stop's own Learn can
take a control from anything except the PMJ's controls and the encoder
bank's runs). Two Virtual Console widgets on one input are Show Review's
*Same input twice* (VC-06), not refused here.

**This computer's own controls.** Two more owners are the computer's, not
the show's, and can never be moved. A **game pad's** buttons and triggers
are its Run map: a Control Map trigger or the emergency stop learned on
one is refused, naming its job in the pad's own words -- *Cross on the
pad: GO with L2 held (the pad's Run map)* -- because a trigger there would
fire a cue on one press, without the arm and the safety. Hits from the
pad are the right trigger's, by their place. A **computer key** the app
already uses (a menu shortcut, an edit key, a Virtual Console key) is
named the same way: *the shortcut for Save Show*.

**A trigger has a place.** A Control Map trigger works where you learned
it, as a hit does: **in a cue** (only while that cue is live), **in a cue
list** (while it runs, or while it is the current production's main list
and nothing else runs), **in a production** (while it is current) or
**everywhere**. So one control may have several triggers at different
places -- *Note 60: Scene: Warm everywhere, except Hit: Stab in cue 2.3* --
and only the **same place** counts as a second job. More in [Where a
trigger works](#where-a-trigger-works) below.

A show saved before this, or with two jobs on a control for any other
reason, loads as it is. The **Who has it now** line at the bottom of
Control surfaces (there only while there is one) lists every such control with what listens to it and where
to fix it; the Devices line adds *1 control with two jobs*.

| What | Where you set it (or **Set Up…** in Control surfaces) | What it does |
|---|---|---|
| **Control Map** | Right-click a cue list, cue or hit (a scene or effect in the Classic QLC+ view) ▸ **Assign to Control…** ([the window](#assign-to-control)); the table is **Connections ▸ Devices ▸ Show control &amp; sync ▸ Control Map** | One button fires one thing. See [Control Map](other-improvements.md#control-map-midi-and-osc-triggers). |
| **This computer's desk** | The top of Control surfaces: **Learn…** per role | STOP, Grand Master, GO and Back for every show on this computer ([below](#this-computers-desk)). |
| **Emergency stop** | The **Emergency Stop…** button under the Control Map table | One button is ALL OFF (below). |
| **Encoder bank** | The section under the Control Map table | Endless encoders turn the Workbench selection in Design. See [Build](build.md#encoder-banks-a-midi-controller-turns-the-selection). |
| **Virtual Console input** | A widget's **External Input** | A button, a slider (the one to use for a fader that sets a level) or a cue list, as in QLC+. |
| **Speed master inputs** | The master's **Inputs…** | A fader or button for a speed master. |
| **Grand Master input** | Virtual Console ▸ Settings ▸ **Grand Master** (the Grand Master line's **Set Up…**) | One fader for the whole show's level. |
| **PMJ overlay** | Automatic, for the PMJ Black 1 only | The PMJ's faders, encoders and buttons (below). |
| **Companion** | The qlcconsole module in Bitfocus Companion | A Stream Deck over the web access socket. See [Stream Deck](show-control.md#stream-deck-companion). |
| **Game pads** | **Connections**, right-click the pad | Pad Run and the focus tools. See [Show control](show-control.md#go-from-the-game-controller-pad-run). |

## A fader is picked up before it follows

An external fader that is not where the control is does nothing until you
move it to, or through, the current value; then it follows. This stops a
fader that was left at the bottom from dropping a control to zero the
moment you touch it. It holds for a **speed master's Rate** fader, a
**channel group's** fader and a **Virtual Console slider** with **Catch
values** on, in Playback, Level or Speed master mode (every new slider or
knob has it on, however it is made: added, from a wizard, an Eos import or
a copy of one that has it; one in a saved show keeps its setting). An
encoder (an input in **Encoder** or **Relative** mode) has no position, so
it is never made to wait.

**Masters follow at once.** The **Grand Master** (footer and Simple Desk)
and a Virtual Console slider set to **Grand Master**, **Submaster** or
**Group master** do not wait: they follow their fader, up and down, from the
first value, whatever Catch values says. They stay that way until their
surface type declares them.

A fader that is waiting says so in the tooltip of the control, and a speed
master's footer button gets an arrow, the way to move the fader:
*"Speed 1 fader: move it to 50% to take over"*. The percent is where on the
fader's travel: a speed master's Rate fader has 100% rate at its middle, so
it reads *"move it to 50%"*. It clears when the fader gets there (or when
you move the control on screen to where the fader is).

Right after the show opens nobody knows where a fader is. A fader that is
already at the control is taken at once; one anywhere else waits, whichever
way you move it, so a control never jumps to a fader that was somewhere
else. A resting fader's jitter is no move.

A Virtual Console slider with **Inverted appearance** reads its fader
upside down, and so does its pick-up: a slider at 0 is picked up by a
fader at the top. A level slider with limits is picked up where its value
sits on the fader's travel, and its hint says that place.

Giving a control another fader (Learn again) starts over: the new fader
waits until it reaches the control.

## The rules for a surface

These are the same rules the pad and the phone page follow, written so you
can check a desk against them.

1. **A press that starts a scene or an effect works in Design and in
   Operate.** Build tries them in Design.
2. **A cue list or a cue** is started by a press in Operate. In Design it is
   started only when it is **armed**: the list the Run tab plays, or what is
   selected in the Show Tree.
3. **A hit** is fired by a press only in Operate and only where its place is
   **live** (the narrowest live place, as a Virtual Console button and the
   phone page do). In Design a hit fires only when it is armed, as above.
   **A computer key follows the same rule** as a pad: a key bound to a hit
   fires in Operate only where the place is live, and a cue list or cue as in
   rule 2. A scene or an effect on a key fires in Operate only (a key is
   also typing). A key held back says why on the footer, as a pad does.
4. **Letting go is always allowed.** A press that stops something, or the
   release of a button held down, is never refused, even when the place
   has ended since.
5. **A refused press says so.** The footer reads *Not fired: Hit: Stab. Its
   place is not live.* (or *In Design a control fires only what is armed…*),
   and the Control Map's last-fired line shows it. A refused press has
   nothing to release: letting go of it stops nothing.
6. **A dedicated GO button needs no arm.** A GO on a desk is a button you
   mean to press, and the double-GO guard holds a second GO that comes too
   soon ([Two GOs at once](show-control.md#two-gos-at-once)). The pad has its
   two-control arm because a pad is held in the hand.
7. **A controller that goes away lets go.** Unplug a desk with a Momentary
   button held and what it held stops; plug it back and the same button
   works at the first press.
8. **Editing never happens in Operate from a surface.** The Encoder bank and
   the PMJ's encoders say *Encoders are off in Operate -- switch to Design to
   edit.*; the PMJ's faders say *Faders are off in Operate -- switch to Design
   to edit.* A master (the Grand Master) is not an edit and works.
9. **A release lets go of what its press did**, as the control was bound
   when it was pressed. Changing the mode or binding the control to something
   else while it is held does not turn the release into a Stop of something
   else.

After an emergency stop, one press of a GO button on a desk starts the list
again. That is by design (rule 6); the pad needs two controls to come back,
a desk does not.

## This computer's desk

**STOP, the Grand Master, GO and Back belong to this computer, not to the
show.** Learn them once and they work in every show you open here: a lab
computer, the show-day laptop and the bench Mac each keep their own. They
sit at the top of **Connections ▸ Devices ▸ Show control &amp; sync ▸
Control surfaces**, under **This computer's desk**, one row each: what the
role is on now (*APC MINI · Note 82 · on Universe 1*), an **On** tick and
**Learn…**.

- **Four learns, each for its own kind of control.** **STOP** takes a
  button pressed, released and pressed again (the panel asks for the second
  press). **GO** and **Back** take a button: one press and its release. The
  **Grand Master** takes a fader: move it. The wrong kind is refused, in
  words: a GO on a fader (*… sends a range of values: it is a fader or knob,
  not a button*), the Grand Master on a button (*… is a button. The Grand
  Master takes a fader.*). A control that is already another role's is
  refused, naming it, and still does its own job meanwhile (a GO pressed
  while Back is learning still GOes). A control refused once stays refused
  for that learn, so a fader pulled down to 0 never turns into a button.
  GO, Back and the Grand Master refuse the show's emergency stop control
  (*… is the show's emergency stop*): the show's stop would be blocked
  there. **Cancel** stops a learn and keeps what was there; a learn also
  ends by itself after 30 seconds with nothing pressed, when you leave the
  panel, when the show goes to Operate and when another show loads. While
  a learn listens, the presses it hears fire nothing of the show's: no
  trigger, Virtual Console button or slider, only the stops.
- **Kept on this computer, never in the show.** The record is in this
  computer's settings, by the device's identity: a MIDI port by its name on
  a Mac, by the *2-* Windows puts before a second unit of one name, by its
  ALSA port (*20:0*) on Linux. Saving a show writes nothing of it, a show
  from another computer does not change it, and **File ▸ New** keeps it.
- **Through the show's own patch.** A role works where a universe's input
  has its device. Move the device to another universe and the role follows
  it; the universe number never matters. Two devices of one name that
  nothing tells apart (two APC minis on a Mac) are said: the desk uses the
  first one patched. Unplug one, or use another port. A universe with
  **passthrough** on sends its input straight to DMX, so the desk can't
  hear a device patched there: the row says so, and the role is not live
  until passthrough is off.
- **The desk wins its controls.** A show listener never hears them: a
  Virtual Console button or slider, the show's Grand Master input, a channel
  group, a speed master and the encoder bank all skip a control the desk
  has. A show's Control Map trigger on one is **kept in the show, not armed
  here**, and listed under **Who has it now** as *blocked on this computer*
  with the reason; on a computer whose desk does not use that control it
  works as before. Assign to Control… on a desk control is refused, naming the
  role.
- **The show keeps its own stop, Grand Master and GO** (board Q314), so a
  computer with no desk still has them. The show's Grand Master input and its
  GO of the Run tab's list are fallbacks: they work only while this computer
  has no Grand Master or GO of its own. **Both stops are live** (Q315): the
  show's emergency stop and the desk's STOP each stop the show, unless the
  show's sits on a control the desk uses for something else.
- **STOP is ALL OFF, always**: in a show with no triggers, with Control
  Surfaces off for the show, with a Learn open, in Design and Operate,
  behind a dialog and while a show loads. Every press after a release is
  ALL OFF; a held button that keeps sending stops the show about once a
  second.
- **GO is one press**, on the Run tab's list, with the double-GO guard (no
  arm; the pad keeps its own). **Back** goes back on the same list. The
  footer chip's tooltip says what GO plays now: *GO → Act 2 main*.
- **The Grand Master waits to pick up** after a show loads, after its
  device is plugged in again, after its port is patched again (by hand or
  by Patch It) and after the computer wakes (board Q316). While a show
  loads it hears nothing of the desk at all. What
  the device sends in the first half second is its own state dump and moves
  nothing; then the fader does nothing until it reaches the Grand Master
  (the row and the chip say *GM waiting: bring the fader to 100 %*), and
  follows from there. A pull to 0 always applies at once. A Grand Master
  learned again on another fader, ticked **On** again, imported or
  repatched waits the same way, so the stage does not jump to where the
  fader sits. Another device coming back (a pad, a second box) does not
  make it wait.
- **On** turns one role off on this computer (Q317): its control is the
  show's again. The tick is kept with the computer.
- **The show's PIN guards the desk** (board Q319): with a PIN set, every
  change -- a learn, a tick, an import -- asks for it first. One PIN opens
  one change: the next one asks again, and so does any change after the
  PIN itself is set or changed.
- **Export…** saves the record to a small file (*.qlcdesk*); **Import…** on
  another computer sets that computer's desk from it, for a room of laptops
  set up the same way.

**The footer chip.** While a role is learned the status bar shows
**Desk**: green when every role that is on works, amber when one is off,
its device is not plugged in, two devices of one name can't be told apart
or the Grand Master waits to pick up, and **grey** when its device is here
but this show's patch does not have it yet -- not set up, not a fault: *Desk:
APC MINI not patched · Patch It*. A click on grey patches the device to the first universe with a
free input (never one with passthrough on) and says so (*Patched APC MINI
to Universe 3's input.*); the chip then reads *Undo* for a moment, and a
click takes it off the patch again. Opening another show ends the Undo:
it never changes a show you did not patch. A click on green or amber opens Control surfaces.

**Show Review** has a **This computer** section: each role, and whether
it reaches the program now. It is not a check of the file -- the roles are
this computer's -- so nothing there is accepted or saved with the show.

## The emergency stop

A control surface can have **one emergency stop**: **Emergency Stop…**, the
button under the Control Map table.

- It is **ALL OFF** through **the one path** every stop takes: the screen's
  **Stop All** (its button, Control ▸ Stop ALL Functions! and its key), a
  Virtual Console **Stop All** button with no fade, the pad's four-together
  stop, the phone page, Companion's ALL OFF and QLab's or MSC's ALL OFF.
  Each does the same: every function stops, every Show-only fixture is cut
  (but its pan and tilt, fine channels too, stay where they are: a
  Show-only head never moves when you stop), every fixture goes to rest, and the Simple Desk and Virtual Console
  **Level** sliders let go of what they hold -- all but pan and tilt, which
  stay where they are, as Stop All leaves every head, so no head swings
  across the stage. None can drift from the others.
- **Stop All is never greyed.** A level held with nothing running (a Simple
  Desk channel, a Level slider) is what it is for too, so it is always
  there. A level let go of stays out, through a GO, until you move its
  fader again (or pick a level or colour from its Click & Go); a fader left
  up on the screen does not light anything until then. (A master, Grand or group, only scales what is lit: it lights
  nothing of its own.)
- Its key is **⌘⇧Esc** on a Mac and **Ctrl+Shift+Backspace** on Windows and
  Linux (Ctrl+Shift+Esc opens the Task Manager on Windows, which would take
  the press first).
- It works in **Design and Operate**, at any moment, on Windows too, and
  needs no arm and no PIN (from the Control Map). It is never held by the double-GO guard.
- **Every press is ALL OFF.** It does not toggle, and holding it down does
  nothing more. The release does nothing.
- It is **learned on a button**, with a second press to confirm: press the
  button, release it, press it again. The dialog says so while you do. It
  refuses a control its input profile calls a fader, knob or encoder, and a
  control that sends two different values with no release between (a fader
  moving); a CC fader with **no profile** that only ever sends 0 and 127
  cannot be told from a button. A computer key is refused (a key works only
  in Operate).
- **It beats any learn.** A learn left waiting (an Encoder learn whose panel
  was closed) or an Assign to Control window that is open never eats a press of
  the stop. Learn dialogs refuse the stop's own control.
- **A stop is never lost for more than one press.** An extra ALL OFF is
  better than a missed one, so nothing marks a bound stop dead for good. Two
  rules: **(a)** a value above 0 that follows a 0, or the first value after
  the show loads or the controller is plugged in, is a press and fires;
  **(b)** any other value above 0 fires when a second has passed since the
  last ALL OFF that control sent. So a held button that keeps resending, or a
  fader that keeps moving, sends ALL OFF about once a second. The second's
  wait starts only when an ALL OFF actually ran.

  | The control sends | The stop does |
  |---|---|
  | 127 held, repeating at once | one ALL OFF |
  | 127 every 0.6 s for 3 s, never a 0 | one about every second (3 or more) |
  | 127, 0, 90, 0, 60, 0 (a velocity pad) | three |
  | 100, then 80 a moment later (the release was lost), then 0, then 100 | one, then one (the 80 is the same hold); flagged |
  | 100, then 80 five seconds later | two; not flagged |
  | 100, then 100 after a second | two |
  | 1, 1, 1, 127, 1 (a relative encoder, never 0) | one in the first second, flagged |
  | a fader swept quickly | one, then about one a second while it moves; flagged |
  | a fader moved once a second or slower | one for each move, **not** flagged |
  | a fader that reaches 0 between moves | one for each press after the 0; flagged |

- **A stop on the wrong kind of control is flagged loudly, not disarmed.**
  When different values arrive with no 0 between and less than a second apart, the footer says once
  *The emergency stop is bound to …, which moves like a fader: learn it again
  on a button.*, and the Emergency Stop… button and the Control Map row go red
  and say *moves like a fader*. The flag stays until the stop is learned
  again (an unrelated learn or removal keeps it) and the stop still fires as
  the table shows. A fader moved slowly enough (a step a second or slower) is
  never recognised, because it looks like a button pressed again; it stops
  the show on each move, which is the safe side. If the input profile calls the control a fader or knob, the stop
  **still fires** under the same press rule, and is flagged the same way: the
  footer says once per hold that it is a fader or knob, and the button and row
  say *is a fader or knob*; learn it on a button. A computer key (a show file
  can put it there) cannot be the stop: the press is refused in words on the
  footer and the button and row say *a computer key cannot be the stop*.
- **A button that keeps resending its value while held** fires one ALL OFF at
  the moment you learn it. That is deliberate: an extra ALL OFF beats a
  missed one.
- **A lost release at learn time** does not hold the first real press: the
  press that taught the stop is ignored until its release, until a second value arrives, or for one second.
- **A device that comes back sends its whole state**, and that is not a press.
  For half a second after a desk is plugged back in (or found again after a
  sleep), a stop whose first value is 0 is at rest, and the one press value
  that follows it in that half second is the desk's state dump: no ALL OFF,
  and the notice says *Ignored STOP's first value after replug (state dump)*.
  Only one value is ever ignored: a release and a press after it (a double
  tap on STOP right after the replug) fire ALL OFF. A first value above 0 is
  a press: one ALL OFF, however often the dump repeats it. After the half
  second every press is a press again.
- **Every other button is the other way round.** A GO, a Back, a hit, any
  other trigger, a Virtual Console button, this computer's GO and Back and
  the PMJ's Go and Back ignore every press value in the first quarter
  second after the desk comes back (that is the desk's burst, never a hand:
  some desks dump a key as release, press, release). For the rest of the
  half second they act only on a press that follows a release (0) from the
  same button: a "pressed" in the desk's dump is ignored, and the notice
  says so (*Ignored GO of Main's first value after replug (state dump)*).
  An unwanted GO is worse than a missed one: if nothing happened, press
  again. This computer's STOP keeps the stop's rule above. Faders are not
  affected: they pick up where they are, as before.
- **When the stop's desk is missing, you are told.** The Control surfaces
  hub's stop line turns amber and says *its device '…' is not connected, so it stops nothing
  until it is back* (or that its universe has no input patched), and Show
  Review's **CTL-01** says the same before the show. The screen's Stop All
  still works.
- It is saved with the show. A QLC+ 4 build opens the show and skips it.
- **No switch hides it.** With **Control Surfaces** turned off for the show
  (View ▸ Workspace ▸ Tabs & Features…, see
  [Features](getting-started.md#features)) the triggers and the encoder bank
  are put away, but the **Emergency Stop…** button stays and the stop still
  fires.

On **Companion**, the **ALL OFF** preset needs the key **held for one
second**, so a brush on the Stream Deck does not stop the show. It is not on
any dial, and it works with *Allow GO from the phone Run page* off and
**needs no PIN** (board Q323): ALL OFF and Stop from Companion or the phone
page never wait on the show's PIN; everything else does. That switch (File ▸
Show Settings…) is Companion's too: GO, Back, Stop and every other run
command from a Stream Deck follow it as the phone does; only ALL OFF does
not.

## The Control Map

How to learn a control and how a press acts is in [Control
Map](other-improvements.md#control-map-midi-and-osc-triggers). Two things
that matter to a desk:

- **Momentary** (on while held) lets go at the release, even if you changed
  mode or armed something else in between.
- A fader or a CC that goes on sending values is **one press** until it goes
  back to 0. Use a Virtual Console slider for a fader that sets a level.
- **GO and Back on one list.** A cue list's **Assign to Control…** learns its
  GO or, with **When pressed** set to **Back**, its Back: two controls, one
  list. Back goes back one cue on each press (the release does nothing),
  through the same path and the same double-GO guard as the Run tab's Back,
  and it is armed where the list's GO is. **Stop the list** stops it from
  either control: chosen with the window on the Back, the Back becomes a
  stop button (as the Control Map table's Back row allows) and stays one.
- **A GO, a Back or a hit is never learned on a fader.** Learn hears what
  the control sends: a fader or knob (its profile says so, or it sends
  different values with no 0 between, the emergency stop's own test) is
  refused, *CC 7 is a fader or knob (it sends a range of values): a GO fires
  on a press, so it takes a button, pad or key.* A scene may still go on a
  fader (one press until it is back at 0).
- **A latching button is flagged.** A button that sends on at one press and
  0 at the next (a toggle switch, a pad set to latch) reaches the show only
  every other press. Learn hears it when you press it a few times (on, then
  0 a press later, then on again; a momentary button let go within a
  second and a half is never taken for one) and turns **Toggle** into
  **Start only**, saying so. A mode you chose yourself is kept and the
  note says so: **Stop only** still stops, **Momentary** follows the
  latch (on, then off), and a list's **GO** or **Stop the list** stays
  what it is. Set the button to momentary on the controller if it can be.

## Assign to Control

Right-click a cue list, a cue or a hit and choose **Assign to Control…**
(it was *Learn Trigger…*; one item per thing, and once assigned it reads
*Trigger: Note 60 (C4), MIDI ch 1…*). A cue list's window assigns its GO
or, with **When pressed** set to **Back**, its Back. In the qlcconsole and
Everything views that is what a control fires: a cue, a list or a hit. A
raw scene or effect is offered it only in the **Classic QLC+** view; an
existing trigger on one keeps working and stays in the Control Map table.

The window is your desk as a list:

- **Who has each control, before you press.** Every control something
  already listens to is listed with its owner: *Scene: Warm (Control
  Map)*, *Button "House lights" (Virtual Console)*, the encoder bank, a
  master. This computer's desk roles read *this computer's GO
  (Connections ▸ Control surfaces) · can't be taken*, greyed: a show never
  takes them. The controls of each patched profile are listed too, free
  ones as **Free**.
- **Press or move a control to find it.** The press selects its row
  (added if it was not listed) and **fires nothing**, in Design and in
  Operate: no trigger, Virtual Console widget, master, channel group,
  game pad's Run button or desk GO hears a press while the window is open,
  and a fader pulled to the bottom stays where the show had it while the
  window is open. When you close it (OK or Cancel) each control you moved
  is heard once where you left it: a submaster or the Grand Master follows
  its fader to where it now is, and a button held down before you opened
  the window is let go of. A button pressed and let go in the window fires
  nothing. The **emergency stop**
  (the show's and this computer's) still stops, and this computer's Grand
  Master still follows its fader. Clicking a row only finds it: the press
  on the device chooses.
- **A GO, a Back or a hit is refused on a fader**, said in the window and
  on the fader's row (*a fader: a GO or a hit takes a button*).
- **A latching button is flagged** on its row (*latching: set it to
  momentary*), with the note below.
- **A Stream Deck through Companion.** While a phone or Companion is on
  this show's Run page, the window says a Stream Deck key is set in
  Companion, not pressed here: a key there sends GO, Back, ALL OFF or a hit
  through the qlcconsole module's presets.

**Control surfaces** has the same finder for any control: the line
**Press or move any control** under this computer's desk names who has
the control you touched last, or says it is free. Unlike the window, the
press there still does its job.

## Where a trigger works

Every trigger has a place, chosen in **Works** when you learn it and
changed later in the **Works** column of the Control Map table:

| Works | Armed while |
|---|---|
| **In cue 2.3 …** | that cue is the live cue of its list (the GO that leaves it disarms it) |
| **In cue list Act 1** | that list runs, or it is the current production's main list and nothing of that production runs |
| **In production …** | that production is current (or none is) |
| **Everywhere** | always -- what every trigger was before places, and what every older show loads as |

This is the hits' own rule: a trigger in cue 2.3 is armed exactly when a
hit living under cue 2.3 is. In **Design** a place is also armed while you
are looking at it: the cue selected in the Show Tree, or the list selected
there or chosen in the Run tab. The Operate / Design rules above still
apply on top.

**Assign to Control… offers the place.** From a hit's right-click it starts
at the hit's own place, and a hit is offered only its own place and the
ones inside it: a hit fires only where its place is live, so a trigger
wider than that would hold the control and fire nothing. (One left wider
in an older show does not hold it: where the hit cannot fire, the next
wider trigger on that control fires.) A cue list's GO and a jump to a cue start at
**Everywhere**: their job is to bring that list or cue live, and a trigger
that works only once it is already live could never start the list or jump
to the cue. The cue or list you right-clicked is offered in the list, with
the places around it (the cue's list, its production) and the cue live
now. Learned from a scene or effect in Build it starts at Everywhere. Under the control, a line says what the control will do where:
*Key · F6: Scene: Warm everywhere, except Scene: Strobe in cue 2 Chorus.*

**Several triggers on one control.**

- At **different places**, the **narrowest armed** one fires: cue, then
  cue list, then production, then everywhere. Learn says so in that line,
  and Learn does not refuse it. Two places of one kind live at once (two
  running lists, two cues of two lists) never step through each other:
  the one in the Run tab's list fires, else the first in the table.
- At the **same place**, Learn refuses it, naming the trigger that is
  there, with **Move It Here** -- and, on a button or key, **Step Through
  Them**: both stay and each press fires the next one (as a VC button with
  two hits at one place does). A pair you stepped through is not listed
  under **Who has it now**: it is your choice, not two jobs.
- The table's **Works** does not offer a place another trigger on that
  control already has: moving there would take that trigger. Learn the
  control again to replace it or step through them.
- Another listener (a VC widget, the encoder bank, a master, the PMJ) is
  refused whatever the place: those listen everywhere.

**Letting go.** A trigger whose place is no longer live can still stop
what it started (a Toggle press, a Momentary release): only a start is held
back. A computer key keeps the key rule there too: in Design a scene or
effect key does nothing, in its place or out of it. Its light stays bright while that runs, then goes dark.

**A place that goes.** Delete the cue, cue list or production a trigger
works in and the trigger moves out, as a hit does: to its production, or
everywhere when that is gone too (never onto a place another trigger on
that control has while a wider one is free). When every place it could
move to already has another trigger on that control, it stays where it
was and never fires -- it is not made to step through with that one --
and the note says so: choose a place in **Works**, or remove it. A hit's
trigger follows the hit when you move the hit to another place, with the
same rule. **Control surfaces ▸ Triggers
by place** and the trigger's **Works** say what happened: *Scene: Strobe
worked only in cue 2 Chorus, which was deleted: it works in production
Show now.* Undo the delete and it goes back.

**A place from a newer version.** A show saved by a later qlcconsole may
have a trigger at a place this one does not know (a layer, an effect). It
is kept, saved back exactly as it came, and never fires here: **Works**
says *In a place from a newer version*, **Its light** *Dark (place from a
newer version)*, and Triggers by place says so. Choose a place in Works
to use it here.

**Lights and the hub follow what is armed now.** A control's light is the
one of the trigger that would fire: when the GO leaves cue 2.3, a control
learned there goes dark (or shows the everywhere one's light). The table's
**Its light** says *Dark (not live here)*, *Dark (its hit is not live
here)* (a hit's trigger whose hit's own place is not live) or *Dark (a
narrower place has it)*. **Control surfaces ▸ Triggers by place** lists each control with a
trigger at a place, what it does where and which trigger has it now
(*Now: Scene: Strobe (in cue 2 Chorus)*).

The **emergency stop** has no place: it is everywhere, alone on its
control, and comes before every trigger and every place.

## What a control's light says

A desk with a feedback line gets its lights from the show, so a button says
what it would do **before** you press it. Every light is read from the one
place that owns the answer -- the same place the Run tab, the footer and the
pad read -- so no two surfaces can disagree.

| The light | What it means |
|---|---|
| **dark** | Pressing it does nothing here, now: a hit whose place is not live, a cue list or cue that is not armed in Design (rules 2 and 3 above), a *Stop only* control with nothing to stop, a cue list with every cue cut, or the thing it fired is gone. The footer says which when you press it. |
| **dim** | Ready: a press would fire it. |
| **bright** | What it fires is running. |
| **two blinks** | The double-GO guard held a command on that cue list ([Two GOs at once](show-control.md#two-gos-at-once)). The light then goes back to what it was saying. |
| **the emergency stop's button** | Dim at every other moment -- it always works, so it is always findable on a dark stage -- and **lit** from an ALL OFF until the next GO. Every other light is dark while it is lit: after an ALL OFF nothing is ready until the show runs again. |

The same words are in the **Its light** column of the Control Map table, so
a desk's lights can be read from the laptop with no desk plugged in.

**This computer's STOP, GO and Back light the same way.** On a desk with a
feedback line, this computer's STOP has the emergency stop's light, and its
GO and Back the light of the show's GO and Back of the Run tab's list
(they are always armed, in Design and Operate). A board's own overlay (the
PMJ's) leaves those buttons to them.

**A desk that glows dim by itself: the PMJ Black 1.** The PMJ lights every
button at a low level (15) when it is idle. There a dim light could not be
told from a button that does nothing, so on the PMJ the show uses its
profile's two levels instead: **lit** (127) when a press would fire it now
(a hit only while its place is live) or what it fired is running, **dim** at
every other moment. The emergency stop's button is **lit at every moment**
-- it always works -- and after an ALL OFF it **blinks lit and dim** until
the next GO. Two blinks are still two blinks. Pads whose low level is a
colour (the APC mini mk2, the Launchpad Mini MK3) keep the table above.

**A pad whose lights are colours, not brightnesses.** Some pads (the APCs)
read the velocity of a light message as a *colour index*, so a dim level
there is another colour, or nothing at all. Switch **Dim a control that is
ready** off, under the Control Map table: a ready control then stays dark,
and running, GO held and stopped are unchanged. The setting is saved with
the show.

A computer key has no light, and nor has a control on a universe with no
feedback line: the table says so on that row.

## The PMJ Black 1

The PMJ's faders, encoders and buttons work only on an input patched with
the **PMJ Black 1** input profile, and only there. Patch it as an input on
**any MIDI channel**, with the feedback line, and the profile **PMJ Black 1**.
No restart: the board is found as soon as it is patched (or its profile is
set), and its lamps light then.
Any other controller that happens to send the same MIDI channel 9 notes or
CCs does nothing to it, and the PMJ's lights are sent only to the universe
the profile is patched on.

- **Master** is the Grand Master. **O** is Blackout (a toggle, not a stop),
  **Set** is Blind.
- **Ch 1-10** edit the open palette (Colour: R G B White Amber UV; Dimmer:
  intensity) or, with fixtures selected, their intensity. **1-Up…10-Up**
  zero that fader's target. **Enc 1 and 2** nudge pan and tilt. These are
  **Design only**: in Operate they are refused with the sentence above.
- **Select 1-10** and **Load 1-10** change the selection and work in either
  mode.
- **Two boards.** Only the first universe patched with the PMJ Black 1
  profile is heard. If a second is patched, the footer says *A second PMJ
  Black 1 is patched on universe N: only universe M is heard. Unpatch one.*
- **One lamp writer.** A free button you learn for the show (**Go**,
  **Favorites**, a Down button: a GO, a hit, the stop) is lit by the Control
  Map only. Moving the Master or any other change on the board never sends
  that lamp dark.
- **Go** and **Back** are the Run tab's GO and Back, on the list the Run
  tab plays, in either mode: in Design they do what the Run tab's GO does
  there. Two GOs inside the guard's window are held, as from the laptop
  ([Two GOs at once](show-control.md#two-gos-at-once)). Learn a trigger on
  either in the Control Map and that trigger is its job instead.
- **Left**, **Right**, **Pre Page** and **Next Page** do nothing yet. The
  mode buttons (**Effects**, **Groups**, **Looks**, **Macros**, **Fix Cont**)
  choose the page; for now every page gives the strips the same job.

### What the lamps mean

| Lit | What it means |
|---|---|
| **Go** | The Run tab's GO would play a cue now. Dim: there is a cue list, but nothing left to GO. Dark: no cue list. |
| **Back** | The list is running, so Back has a cue to go back to. Dim: the list is stopped. |
| **One mode button** | The page in use: **Effects** until you press another (Groups, Looks, Macros, Fix Cont). |
| **1-10**, **1-Load…10-Load** | That strip has a fixture of the scene open in Build (strip 1 the first). Brighter: that fixture is selected. Dark: the scene has no fixture for that strip. |
| **1-Up…10-Up** | That fader edits the open palette now. |
| **O**, **Set** | Dim: ready. Bright: Blackout or Blind is on. |
| **A key you learned** | The Control Map's light: lit when a press would fire it or it is running, dim otherwise ([What a control's light says](#what-a-controls-light-says)). |
| **The emergency stop's key** | Lit at every moment: it always works. Blinking lit and dim: the show is stopped (ALL OFF) until the next GO. |

The **PMJ Black 1** row of **Connections ▸ Control surfaces** says the same.

## Other surfaces

- **Akai APC40 mk2**: the input profile **Akai-APC40-mkII** gives the pads
  their LED feedback. Patch it on MIDI channel **Omni**: each track's fader
  and buttons send on that track's own MIDI channel (1-8, the master on 9),
  so on one channel only track 1 is heard. For the pads to take the lights
  and stop toggling their own, set the device's **Initialization message**
  to **APC40 Mk2 Ableton mode** (the MIDI plugin's **Configure** in
  Connections ▸ Devices): it sends the SysEx that puts the APC in Ableton
  mode each time the device is opened. Learn a clip pad on a cue, a hit or a scene; the
  encoder bank has an **APC40 (knobs)** preset.
- **Behringer X-Touch Mini**: no input profile yet. Set the unit to
  *Standard* mode, layer A, encoders *Relative 1*; patch it as an input; the
  encoder bank has an **X-Touch Mini** preset.
- **OSC** (TouchOSC, QLab): the QLab listener is off until you switch it on;
  `/go` and `/alloff` work when it is on. A sender on an OSC port patched
  into the Control Map is heard by anyone who can reach that port.
- **Windows**: a Control Map press, the encoder bank and Virtual Console
  inputs work on Windows. A pad's arm and MIDI timecode in are refused there
  (decided for the pad). Control surfaces on Windows are **not yet checked
  on a real Windows machine**.

## Unplugged, replugged, asleep

A cable pulled or a desk asleep mid-show never leaves anything stuck on,
and never surprises you when it comes back.

- **Whatever was held lets go.** A device that goes away never sends its
  releases, so everything that listens behaves as if each control were let
  go: a Control Map trigger held down (a Momentary) lets go, a Virtual Console
  button **flashing** goes off, a slider's **flash** button goes off, and a
  slider forgets where its fader was. The level stays where it is. When the
  desk is back, a slider that picks up (**Catch values** on, in Playback,
  Level or Speed master mode; see above) waits for its fader before it
  follows, so a fader moved meanwhile, or the state the desk sends when it
  is plugged in, does not jump the level. A slider with Catch values off,
  and every master (Grand Master, Submaster, Group master), follows the
  desk's first value at once, as it always does: set the fader where the
  level is before you plug the desk back in. A flash held with the mouse is
  not touched.
- **The unplug says what stopped working.** The notice and the footer name
  the jobs the device had: *MIDI device 'APC MINI' disconnected — universe 2.
  Not working until it is back: Emergency Stop, Grand Master, GO of Main.*
- **After a sleep**, the notice line says *Slept 40 s, finding devices…*
  while the devices come back; each one back, or still missing, is said as
  usual once it has settled.
- **A repatch says what moves.** In Inputs/Outputs, changing which device a
  universe's input is patched to (or unpatching it) while the show has
  controls on it asks first: *Universe 2's controls move from 'APC MINI' to
  'nanoKONTROL2': Emergency Stop, Grand Master, GO of Main. 'APC MINI' will
  do none of them.* **Cancel** leaves the patch as it was.

## Check it on the real device

These steps need the hardware. Do them once on the desk you will use,
after any build that changes a surface, and say which build.

**PMJ Black 1**

1. Patch the PMJ as an input on any MIDI channel with the feedback line and
   the profile PMJ Black 1 (no restart). *Expect:* **O** and **Set** glow
   dim at once and **Effects** is lit (the page in use); **Go** is lit when
   the show has a cue list. The strip buttons light only with a scene open
   in Build.
2. In Design, open a Colour look in Build and move the faders. *Expect:* the
   palette changes (the faders in use are lit).
3. Switch to **Operate** and move the same faders and turn Enc 1.
   *Expect:* nothing changes, and the footer says *Faders are off in
   Operate -- switch to Design to edit.* or *Encoders are off in
   Operate…* once a second at most.
4. In Operate move the **Master** fader. *Expect:* the Grand Master follows.
5. Patch a second MIDI controller that can send channel 9 (or send CC 0 on
   channel 9 from a MIDI tool) on another universe. *Expect:* the Grand
   Master does not move.
6. Without learning anything, press **Go**. *Expect:* the Run tab's list
   GOes (in Design too, as the Run tab's GO); press twice within a second
   and the second is held (*Second GO held*). **Back** steps back. Then
   learn **Go** on the main cue list (Control Map) and press it: *Expect:*
   one GO a press, not two.
7. Learn **Emergency Stop…** on a PMJ button: press, release, press. *Expect:*
   the dialog asks you to press it again, then shows the control. (A PMJ
   button that does something itself, such as **O** or a Select button, is
   refused: *already does the PMJ Black 1's O (PMJ overlay)*. Use **Go**,
   **Favorites** or a Down button.) Start the
   list and a scene, press the button in Design and then in Operate.
   *Expect:* everything stops each time, with *ALL OFF* in the Control
   Map's last-fired line.
8. Try to learn the stop on a PMJ **fader**. *Expect:* refused, saying a
   fader cannot be the stop; learning goes on.
9. Unplug the PMJ with a Momentary button held, then plug it back.
   *Expect:* what it held stopped; the lights come back; the next press of
   that button works.
10. Patch a second input on the PMJ Black 1 profile and move a PMJ control on
    it. *Expect:* the footer names the second universe and says only the
    first is heard.
11. **The lights.** First with nothing learned on **Go**: *Expect:* with a
    cue list in the show, **Go** is **lit** and **Back** dim; after a GO,
    both lit; on the last cue Go goes dim; Stop the list and Back goes dim.
    The lit mode button is **Effects**; press **Looks** and Looks is the
    lit one. Then with **Go** learned on the main cue list (step 6) and the
    stop learned (step 7): *Expect:* Go **lit** while a GO would play a
    cue; it **blinks twice** when you press Go twice inside a second, then
    lit again; the stop's button **lit** all along (it always works) and
    **blinking lit and dim** when the show is stopped, and while it blinks
    **Go is dim**; one GO after that, Go is lit again and the stop lit and
    still. The Control Map's
    **Its light** column says the same words throughout. Move the
    **Master** fader: *Expect:* Go and the stop's button keep their light.
12. **A hit's light.** Learn a hit that lives in one song on a free PMJ
    button (a Down button or **Favorites**).
    *Expect:* dim while that song is not running, **lit** while it is (and
    while the hit plays), dim again after the list moves past it -- without
    touching anything.
13. **One job per control.** Assign to Control… on a hit and press **O**.
    *Expect:* refused, *… already does the PMJ Black 1's O (PMJ overlay).
    One job per control.*, the reason it can't be moved, and no **Move It
    Here**. Then give a Virtual Console button's External Input the
    **1-Down** button, and Assign to Control… on a hit with the same button.
    *Expect:* refused naming the VC button; **Move It Here** binds it to the
    hit, and pressing it in Operate fires the hit and not the VC button.
14. **A trigger at a cue.** Right-click cue 2 ▸ **Assign to Control…** on a
    hit (Works: *In cue 2 …*) and press **1-Down**. Learn a scene on the
    same button from Build (Works: *Everywhere*). *Expect:* Learn says
    *… everywhere, except … in cue 2*. In Operate: before cue 2, the button
    fires the scene; GO to cue 2 -- it fires the hit; GO past it -- the
    scene again. The light follows each GO without a press.

**Akai APC40 mk2**

1. Patch on MIDI channel **Omni** with the **Akai-APC40-mkII** profile and
   the feedback line, and the Initialization message **APC40 Mk2 Ableton
   mode**. *Expect:* the pads light when idle, and all eight track faders
   are heard.
2. Learn a clip pad on a **hit** whose place is idle. Press it in Operate.
   *Expect:* nothing starts and the footer says *Its place is not live.*
   Start the list that is its place and press again: *Expect:* it fires.
3. In Design press a pad learned on a cue in a list that is not the Run
   tab's. *Expect:* *Not fired…armed.* Select that list in the Show Tree and
   press again: *Expect:* it fires.
4. Learn **Emergency Stop…** on a pad (press, release, press) and press it
   while a scene runs, in Design and in Operate. *Expect:* ALL OFF each
   time. Try the track fader: *Expect:* refused.
5. Unplug and replug with a pad held. *Expect:* the lights come back and the
   next press fires.
6. **Does this pad dim?** With *Dim a control that is ready* on (the
   default), look at a pad learned on a scene that is not running.
   *Expect, if the APC dims:* it glows faintly, and brightly while the scene
   runs. *If instead it is dark, or a surprising colour:* this pad reads the
   velocity as a colour -- switch **Dim a control that is ready** off and
   check that a ready pad is dark and a running one is bright. **Write down
   which it was**: it decides whether the APC gets the dim light by default.

**Behringer X-Touch Mini**

1. Patch it, set the encoder bank to the **X-Touch Mini** preset, select a
   layer in Build and turn an encoder in Design. *Expect:* the value moves.
2. Switch to Operate and turn an encoder. *Expect:* *Encoders are off in
   Operate -- switch to Design to edit.*
3. Learn **Emergency Stop…** on a bottom-row button. Try an encoder or the
   fader instead. *Expect:* the button learns after the second press; the
   fader and the encoder are refused.

**Stream Deck with Companion**

1. Add the **ALL OFF (hold 1 s)** preset from **Run the show** to a key.
2. With *Allow GO from the phone Run page* **off** (it covers Companion too)
   and a scene running, tap
   the key quickly. *Expect:* nothing stops.
3. Hold the key for one second. *Expect:* everything stops, even with Allow
   GO off. Set a PIN in the show and leave it **out** of the connection (or
   type a wrong one): *Expect:* ALL OFF and **Stop** still work; GO, Back and
   the hit keys do nothing and Companion's log says the show has a PIN.
4. Fire a hit key in Design, then in Operate with the hit's place live and
   not live. *Expect:* it fires only in Operate with the place live.

**Every stop, one path (phase F)**

1. **Windows:** with a scene running, press **Ctrl+Shift+Backspace**.
   *Expect:* everything stops, and the Task Manager does not open. Press
   Ctrl+Shift+Esc: the Task Manager opens and the show is untouched (that
   key is not Stop All on Windows). Control ▸ Stop ALL Functions! shows
   Ctrl+Shift+Backspace. **Mac:** ⌘⇧Esc stops everything.
2. **Held levels only:** stop every function, then raise a Simple Desk
   channel on an ordinary dimmer (not Show only), or a Virtual Console
   Level slider, so a light is up with nothing running. *Expect:* **Stop
   All** is not greyed; pressing it takes the light out, and it stays out
   through a GO until you move that fader again. With a moving head's pan
   held on the desk or a slider too: the head does not move.
3. **Desk STOP with the switch off:** learn **Emergency Stop…** on a desk
   button, then turn **Control Surfaces** off (View ▸ Workspace ▸ Tabs &
   Features…). Start a scene and press the button in Design, with a Learn
   Trigger… dialog open, and in Operate. *Expect:* ALL OFF each time.
4. **A locked phone:** set a show PIN, open the Run page on the phone and do
   **not** unlock it. Start the list from the laptop. *Expect:* the PIN
   screen shows **STOP**; two taps stop the list. Connect a pad to the
   phone: LB + RB + LT + RT stops everything without the PIN. GO stays
   greyed until the PIN is given.
5. **Every route, the same stop:** for each of the screen's Stop All, a VC
   Stop All button, the desk's stop, the pad and the phone, press it with a
   Show-only fogger at full. *Expect:* the fogger cuts at once every time
   and the footer says *held: emergency stop* until the next GO.

## See also

- [Control Map](other-improvements.md#control-map-midi-and-osc-triggers),
  [Keys on the computer keyboard](other-improvements.md#keys-on-the-computer-keyboard)
- [Encoder banks](build.md#encoder-banks-a-midi-controller-turns-the-selection)
- [Show control (QLab)](show-control.md): Pad Run, the emergency stop on the
  pad, Two GOs at once, MIDI Show Control and OSC
- [Steam Deck and gamepad remote](gamepad-remote.md)
