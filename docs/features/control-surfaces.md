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
connected*, *Companion: web access off*), and its **Set Up…** button takes you
to the place in the table below. It reads the show as it is and keeps no
settings of its own. With **Control Surfaces** off for the show (see the
emergency stop below) the triggers and the encoder bank lines say so and
their buttons are off; the emergency stop line stays.

**One job per control.** A control (a MIDI note or CC, an OSC message, a
computer key) that one part of the program already listens to is refused
when you learn it for another, naming the owner: *Note 60 already does
Button "House lights" (Virtual Console). One job per control.* Press
**Move It Here** to take it from there and give it to what you are
learning (the old owner stops reacting at once), or **Cancel** to leave it
where it is. Every Learn says this the same way: Learn Trigger…, the
emergency stop, the encoder bank, a widget's External Input (auto-detect or
Choose…), a speed master's Inputs…, the Grand Master's input and a channel
group's input. Move It Here is not on Undo: to put a control back, learn it
there again. Two exceptions, with no Move It Here: the PMJ Black 1's
controls that do something come with its profile, and nothing takes the
**emergency stop's** control (the stop's own Learn can take a control from
anything else). Two Virtual Console widgets on one input are Show Review's
*Same input twice* (VC-06), not refused here.

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
| **Control Map** | Right-click a scene, effect, cue list, cue or hit ▸ **Learn Trigger…**; the table is **Connections ▸ Devices ▸ Show control &amp; sync ▸ Control Map** | One button fires one thing. See [Control Map](other-improvements.md#control-map-midi-and-osc-triggers). |
| **Emergency stop** | The **Emergency Stop…** button under the Control Map table | One button is ALL OFF (below). |
| **Encoder bank** | The section under the Control Map table | Endless encoders turn the Workbench selection in Design. See [Build](build.md#encoder-banks-a-midi-controller-turns-the-selection). |
| **Virtual Console input** | A widget's **External Input** | A button, a slider (the one to use for a fader that sets a level) or a cue list, as in QLC+. |
| **Speed master and Grand Master inputs** | The master's **Inputs…** | A fader or button for a master. |
| **PMJ overlay** | Automatic, for the PMJ Black 1 only | The PMJ's faders, encoders and buttons (below). |
| **Companion** | The qlcconsole module in Bitfocus Companion | A Stream Deck over the web access socket. See [Stream Deck](show-control.md#stream-deck-companion). |
| **Game pads** | **Connections**, right-click the pad | Pad Run and the focus tools. See [Show control](show-control.md#go-from-the-game-controller-pad-run). |

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

## The emergency stop

A control surface can have **one emergency stop**: **Emergency Stop…**, the
button under the Control Map table.

- It is **ALL OFF** through the same path as the pad's four-together stop,
  the phone page's STOP and QLab's ALL OFF: every function stops, every
  Show-only fixture is cut.
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
  was closed) or a Learn Trigger dialog that is open never eats a press of
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
- It is saved with the show. A QLC+ 4 build opens the show and skips it.
- **No switch hides it.** With **Control Surfaces** turned off for the show
  (View ▸ Workspace ▸ Tabs & Features…, see
  [Features](getting-started.md#features)) the triggers and the encoder bank
  are put away, but the **Emergency Stop…** button stays and the stop still
  fires.

On **Companion**, the **ALL OFF** preset needs the key **held for one
second**, so a brush on the Stream Deck does not stop the show. It is not on
any dial, and it works with *Allow GO from the phone Run page* off (it needs
the PIN when the show has one).

## The Control Map

How to learn a control and how a press acts is in [Control
Map](other-improvements.md#control-map-midi-and-osc-triggers). Two things
that matter to a desk:

- **Momentary** (on while held) lets go at the release, even if you changed
  mode or armed something else in between.
- A fader or a CC that goes on sending values is **one press** until it goes
  back to 0. Use a Virtual Console slider for a fader that sets a level.

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

**Learn Trigger… offers the place.** From a hit's right-click it starts
at the hit's own place. A cue list's GO and a jump to a cue start at
**Everywhere**: their job is to bring that list or cue live, and a trigger
that works only once it is already live could never start the list or jump
to the cue. The cue or list you right-clicked is offered in the list, with
the places around it (the cue's list, its production) and the cue live
now. Learned from a scene or effect in Build it starts at Everywhere. Under the control, a line says what the control will do where:
*Key · F6: Scene: Warm everywhere, except Scene: Strobe in cue 2 Chorus.*

**Several triggers on one control.**

- At **different places**, the **narrowest armed** one fires: cue, then
  cue list, then production, then everywhere. Learn says so in that line,
  and Learn does not refuse it.
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
back. Its light stays bright while that runs, then goes dark.

**Lights and the hub follow what is armed now.** A control's light is the
one of the trigger that would fire: when the GO leaves cue 2.3, a control
learned there goes dark (or shows the everywhere one's light). The table's
**Its light** says *Dark (not live here)* or *Dark (a narrower place has
it)*. **Control surfaces ▸ Triggers by place** lists each control with a
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
- Go, Back, Left, Right and the page buttons light nothing and do nothing
  here. Bind **Go** and **Back** in the Control Map to GO and Back the list
  (rule 6).

## Other surfaces

- **Akai APC40 mk2**: the input profile **Akai-APC40-mkII** gives the pads
  their LED feedback. Learn a clip pad on a cue, a hit or a scene; the
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

## Check it on the real device

These steps need the hardware. Do them once on the desk you will use,
after any build that changes a surface, and say which build.

**PMJ Black 1**

1. Patch the PMJ as an input on any MIDI channel with the feedback line and
   the profile PMJ Black 1, then restart. *Expect:* **O** and **Set** glow
   dim; the other buttons stay dark.
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
6. Learn **Go** on the main cue list (Control Map). Press it. *Expect:* the
   list GOes; press twice within a second and the second is held (*Second
   GO held*).
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
11. **The lights.** With **Go** learned on the main cue list (step 6) and
    the stop learned (step 7), look at the board between each of these.
    *Expect:* **Go** glows dim before the first GO (the same level the board
    lights itself at when it is plugged in); bright while a cue runs; it
    **blinks twice** when you press Go twice inside a second, and goes back
    to bright; the stop's button glows dim all along (it always works) and
    goes **lit** when the show is stopped, and while it is lit **Go is
    dark**; one GO after that, Go is dim or bright again and the stop is
    dim. The Control Map's **Its light** column says the
    same words throughout.
12. **A hit's light.** Learn a hit that lives in one song on a free PMJ
    button (a Down button or **Favorites**).
    *Expect:* dark while that song is not running, dim while it is, dark
    again after the list moves past it -- without touching anything.
13. **One job per control.** Learn Trigger… on a scene and press **O**.
    *Expect:* refused, *… already does the PMJ Black 1's O (PMJ overlay).
    One job per control.*, the reason it can't be moved, and no **Move It
    Here**. Then give a Virtual Console button's External Input the
    **1-Down** button, and Learn Trigger… on a scene with the same button.
    *Expect:* refused naming the VC button; **Move It Here** binds it to the
    scene, and pressing it fires the scene and not the VC button.
14. **A trigger at a cue.** Right-click cue 2 ▸ **Learn Trigger…** on a
    hit (Works: *In cue 2 …*) and press **1-Down**. Learn a scene on the
    same button from Build (Works: *Everywhere*). *Expect:* Learn says
    *… everywhere, except … in cue 2*. In Operate: before cue 2, the button
    fires the scene; GO to cue 2 -- it fires the hit; GO past it -- the
    scene again. The light follows each GO without a press.

**Akai APC40 mk2**

1. Patch with the **Akai-APC40-mkII** profile and the feedback line.
   *Expect:* the pads light when idle.
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
2. With *Allow GO from the phone Run page* **off** and a scene running, tap
   the key quickly. *Expect:* nothing stops.
3. Hold the key for one second. *Expect:* everything stops, even with Allow
   GO off. With a PIN set in the show, give it in the connection and expect
   the same.
4. Fire a hit key in Design, then in Operate with the hit's place live and
   not live. *Expect:* it fires only in Operate with the place live.

## See also

- [Control Map](other-improvements.md#control-map-midi-and-osc-triggers),
  [Keys on the computer keyboard](other-improvements.md#keys-on-the-computer-keyboard)
- [Encoder banks](build.md#encoder-banks-a-midi-controller-turns-the-selection)
- [Show control (QLab)](show-control.md): Pad Run, the emergency stop on the
  pad, Two GOs at once, MIDI Show Control and OSC
- [Steam Deck and gamepad remote](gamepad-remote.md)
