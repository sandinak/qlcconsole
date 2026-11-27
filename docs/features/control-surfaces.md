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

**What a control does now.** In the pickers of a widget's External Input,
a speed master's Inputs… and the Grand Master's input, **Choose…** lists
each control with what it does now in a **Does now** column (*free* when
nothing has it), and **Auto Detect** says it under the fields the moment
you move a control, before you press OK: *Fader 10: Grand Master (this
computer's desk)*. The refusal names every job the control has, this
computer's desk roles (STOP, Grand Master, GO, Back) and a surface page's
controls too (*It is also a surface page control: page Move: Pan, on page
Move only*; the page keeps it in Design). In these pickers **Move It Here**
may take a desk role: it is cleared on this computer when you press OK
(set it up again in Connections ▸ Control surfaces). A show's PIN keeps a
locked desk where it is. Assign to Control…'s *press any control* line
names the same jobs.

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
surface type declares them (below).

**What the fader is.** A device's input profile can say what each fader is
(a *surface type*, [below](#surface-types)), and the pick-up follows it:

| The profile says | Pick-up |
|---|---|
| **Endless knob** (Relative) | never waits: it turns from where the value is |
| **Motor fader** | waits as a plain fader, and is sent the value once when it starts to wait, so the motor drives it there and it is picked up when it arrives |
| **Plain fader** (Absolute) | waits until it meets the value, as above |
| nothing (most profiles) | as above, unchanged |

A **master** on a fader its profile calls a **plain fader** waits only to be
raised: below the master and pushed up, or above it, it waits until it
meets it. Pulled down it is taken at once, every time, from the very first
move after the show opens (a move of a step or two is a resting fader's
jitter, not a pull) -- lowering a master is the safe way, so it never
waits. A master on a
motor fader, an endless knob or an undeclared fader follows at once, as
before.

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

## Surface types

A **surface type** is the device's input profile with a few more lines: the
profile says it is one, which controls have a fixed **role** on this kind of
board (the Grand Master, GO, Back, the emergency stop, the PMJ's page keys
and strips), and what each fader **is** (plain, motor, endless). A control
with no role is free for a show to assign. QLC+ 4 reads these profiles and
skips the new lines.

These come as surface types:

| Profile | Roles | Faders |
|---|---|---|
| **PMJ Black 1** | Master = Grand Master; Go, Back, Left, Right, page arrows; O = Blackout, Set = Blind; the five mode keys = pages; strip keys Select / Load / Up (reset); encoders | plain |
| **Akai APC mini** and **APC mini mk2** | Slider 9 = Grand Master; scene keys: **Clip Stop** = GO, **Solo** = Back, **Stop All Clips** = stop | plain |
| **PreSonus FaderPort 2** | Fader = Grand Master; **Play** = GO, **Rewind** = Back, **Shift+Stop** = stop (Shift+Play releases it) | motor |
| **PMJ 9 Faders**, **KORG nanoKONTROL2**, **Behringer BCF2000** | the last fader = Grand Master | plain (BCF2000: motor) |

Today the roles are what the board's own overlay does (the PMJ's) and what
the profile says about its faders; the APC minis' and fader boxes' roles are
written down for the desk to use, which a later version does. Learning a
control for this computer's desk ([This computer's desk](#this-computers-desk))
is unchanged.

**Make one.** Connections ▸ the universe ▸ edit its input profile. On
**General**, tick **This profile is a surface type**; on **Input Mapping**
click a control and pick its **Role** (a strip or page role takes a
number) and, for a fader or knob, what the **Fader** is. A shipped profile
is fixed: save your changes as a copy (a new name). The editor refuses to
save a type that breaks its rules -- the stop on anything but a button, the
Grand Master on anything but a fader, a role on two controls -- and says
which control.

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
  arm; the pad keeps its own): a second GO is held until the cue it
  started has begun (a quarter second at least), never after a **Fast**
  cue ([Two GOs at once](show-control.md#two-gos-at-once)). **Back** goes back on the same list. The
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

## The surface map

**Each board you patch is drawn as it is, every control where it sits,
with what it does now.** It is under **This computer's desk** in
**Connections ▸ Control surfaces**, one map per board, for the **Akai
APC40 mk2**, the **Akai APC64**, the **PMJ Black 1**, the
**MaizeDMX Playback Controller** and the **PreSonus FaderPort 2**. The footer has a chip per drawn board
(**APC40**, **APC64**, **PMJ**, **MaizeDMX**):
a click opens its map.

- **Where GO is, at a glance.** The line over the map says this computer's
  fixed jobs on that board: *GO is PLAY · Back is RECORD · STOP is STOP ALL
  CLIPS · GM is MASTER*. Those controls are drawn in their own colours, the
  role's name on them: **GO** green, **Back** blue, **STOP** red, **GM**
  amber. A role this computer has on another board says so (*GO is on APC
  MINI*). A role it has nowhere says *No STOP on this computer yet.* with
  **Set Up STOP…**, which starts that role's **Learn…** above.
- **Four looks.** **This computer's desk** (STOP, GM, GO, Back, the same in
  every show); **the board's own** (what the PMJ's own controls do: *Level
  1*, *Select 1*, *Blackout*; dashed: they come with the board); **this
  show** (a trigger, a hit by its place, a Virtual Console input, the
  encoder bank, a speed master, the show's Grand Master or emergency stop:
  the name on the control); **free**. A trigger or hit armed only at a
  place that is not live now (another cue, another list) is drawn dimmer;
  hover it for its place. Hover any control for what it does and where it
  is set up.
- **Live.** Press or move a control on the board: it lights on the map, a
  fader or a knob with where it is now. The GM shows **Pick up** while it
  waits for the fader (see [A fader is picked up before it follows](#a-fader-is-picked-up-before-it-follows)).
- **The lights.** Each light on the map is what the app last sent the
  board (its colour on the APC40's pads): if the map and the board differ,
  the board did not take what it was sent.
- **Default roles.** A board's type can come with this computer's roles: the
  first time an **APC40 mk2** is patched here, **PLAY** is GO, **RECORD**
  Back, **STOP ALL CLIPS** STOP and the **MASTER** fader the Grand Master;
  on the **MaizeDMX Playback Controller** its **GO** and **BACK** keys, **X10**
  STOP and the **GM** fader; on the **APC64**, as on the APC40, **PLAY**
  GO, **RECORD** Back, **STOP ALL CLIPS** STOP, and touch strip 8 the Grand
  Master; on the **APC mini mk2** the top scene key **Clip Stop** is GO,
  **Solo** Back, **Shift** STOP and **Slider 9** the Grand Master; **Left** and **Right** page the faders (Colour, Move), lit when there is a page that way.
  A role you have learned already (on any board)
  is never changed, a control the open show already uses keeps its job
  (that role is left for you to learn), and a board is set up once: a role
  you clear stays cleared. The PMJ Black 1's **Go**, **Back** and **Master** are its own
  (the board's overlay does them) and are drawn as GO, Back and GM.
- **The profile comes with the device.** A board patched with no profile
  gets the one made for it, by the name the system gives its port: **APC40**,
  **APC40 mkII**, the **APC64's DAW port**, **APC mini** and **mini mk2**
  (its *Control* port), the **FaderPort 2** (*PreSonus FP2*). A profile you
  pick yourself is kept. The MaizeDMX's port is *USB2.0-MIDI*, a name many
  boxes share, so it is never chosen by name: pick its profile. The
  profile's init follows at once (the APC40 mkII's Ableton mode, the APC64's
  display), so a board that was patched with no profile starts working when
  it is patched or the show is opened.
- **An older copy of a profile in your folder.** A profile of the same name
  in your user folder is used before the built-in one, unless it no longer
  holds to the board's drawing (the old QLC+ APC40 mkII profile, without the
  knobs on the track channels): then the built-in one is used, your file is
  left as it is, and the **Controller setup** line under Control surfaces
  says which file and why. Rename your copy to keep both.
- **Another board takes over.** When the board that holds this computer's
  GO, Back, STOP and Grand Master is gone (unplugged, not here) and a new
  surface with defaults for them is patched or found, the roles move to it
  by themselves; the notice line says so (*GO, Back, STOP and Grand Master
  moved to APC64 from FaderPort 2.*) with **Undo**, which puts them back and
  keeps them there. While the old board is still plugged in nothing moves:
  the new one gets only the roles nobody holds, and the rest when the old
  board lets go of them (cleared, or its Learn replaced).
- **A board with no drawing** (an APC mini, a fader box) is named in one line
  under the maps; what its controls do stays in the rows below.
- **Drawn like the board.** Each map keeps the real board's proportions and
  its parts: round knobs and encoders, wide clip pads, square pads, long
  fader slots, round transport keys, and the groups the panel prints
  (**DEVICE CONTROL**, **SCENE LAUNCH**, **BANK SELECT**) outlined with their
  names. The **PMJ Black 1** has its Master fader and its red and white
  **Go** and **Back** keys on the left under the 3 x 4 grid of keys, four
  big encoders over ten strips (a row of keys, the Load keys, the faders, and
  two rows of keys under them). The map is drawn by the app, not copied from
  a photo.
- **A table of the controls.** Above each map is a table in the look of the
  Connections lists: one row per control, with **Control**, **Page**,
  **Job**, **Lamp** and **Picks up** (a fader that waits to reach the value
  before it moves it). Click a heading to sort (numbers sort as numbers:
  *Clip 1-2* before *Clip 1-10*); type in **Filter the controls** to narrow
  it to a name, a job or a page. A row you select is the control outlined on
  the drawing, and a control you click on the drawing is the selected row.
- **Click a control to give it a job.** A click on a key offers **GO**,
  **Back**, **STOP (all off)**, **Page ◀**, **Page ▶** and a key for each
  page; on a fader, **Grand Master**; on a knob or encoder the page decides.
  Every list ends with **Other… (Assign to Control)** (it tells you how to
  assign a scene, cue or hit to the control) and **No job**. The job ticked is
  the one it has. GO, Back, STOP and the Grand Master are this computer's
  desk (the same as **Learn…**, without the press); the page keys are this
  computer's too. The last change has an **Undo** at the foot of the list. In
  Operate, a show with a PIN asks for it first, as **Assign to Control…** does.
  Jobs such as Sub GO, Blackout, Blind and Flash a group, and a Shift guard
  on STOP, are not in the list yet.
- **Page ◀ and Page ▶.** A board with pages (the APC40 mk2, the APC64, the
  PMJ, the MaizeDMX) has its page keys chosen here: give any key **Page ◀**
  or **Page ▶**, and a key of its own to a page. They are lit while there is
  a page that way, a key has one job (giving it another takes it off the
  first), and they are kept for this computer. The footer's chip and the
  map always say the page (*APC40 · Move*, in Operate too, where the page
  waits for Design); click the page name on the chip to pick one. A page change lights the chip for
  a second, so a board with no lamps and no screen still shows it; the page
  keys' lamps show it on the board, and a board's own screen is written
  with it where its device setup supports one (not built yet).
- **Print Key Labels….** Under the board's name: a sheet of cut-out squares
  at the board's real key sizes with what each key does, a set for each page
  (the desk's and the show's jobs on every page), as an A4 PDF; or one label
  a key on TZe tape, 12 mm (½ in), 18 mm (¾ in) or 24 mm (1 in) wide, for a Brother P-touch CUBE, at its 180
  dpi, saved as a PDF or as pictures, or printed through the system's print
  dialog (the Brother is picked when it is installed; there is no Bluetooth
  setup in qlcconsole: pair it with the computer first). The key sizes follow
  **Settings ▸ Units**. They are measured off each board's published size:
  check one against a rule before you cut a sheet.
- The map is a picture of the board's jobs: assign a scene, cue or hit
  from the thing (right-click a cue list, cue or hit ▸ **Assign to
  Control…**; a scene or effect in the Classic QLC+ view) or from the
  control's click.

The **MaizeDMX Playback Controller** shows up as **USB2.0-MIDI** (a name many
USB MIDI boxes share): patch it with the profile **MaizeDMX Playback
Controller**. The map and its default roles come from the profile, never the
port's name. Its **BO** key is printed Blackout; it has no job yet in
qlcconsole (the map says *Blackout (coming)*). The small circles above the
faders (*LED 1* to *LED 11*) are lights on the board, not knobs: the map
draws them as lamps. **X7** is its **Colour** page and **X9** its **Move**
page (see [Pages](#pages)), each lit while it is on; its lamps need its
feedback line patched.

## The PreSonus FaderPort 2

The 2017 FaderPort 2 (its MIDI port is **PreSonus FP2**) is a one-fader desk
with a motor. It speaks Mackie Control on MIDI channel 1 once it is in its
native mode; patch it on that channel with the profile **PreSonus FaderPort
2**, with its feedback line (the lamps and the motor need it).

- **The fader** is a motor fader (pitch bend). It is the Grand Master, and
  the show moves it: when the Grand Master is somewhere else the fader is
  sent there and picked up when it arrives, as for any motor fader
  ([above](#a-fader-is-picked-up-before-it-follows)). Its **touch**
  sensor is a plain button in the profile, drawn on the map; the desk reads
  it for one job: while a hand is on the fader nothing is sent to the
  motor, and the moment it lets go the motor is sent the Grand Master, so
  the fader stays where you left it instead of driving back to the last
  place the app sent it. A board with no touch sensor does the same a
  moment (150 ms) after the last move. A move still waiting to pick up is
  not taken: the motor goes to the master, not to your hand.
- **The knob** (CC 16) is an endless one, 1 to 63 clockwise and 65 to 127
  counter-clockwise; its push is note 32.
- **Roles** (this computer's desk, seeded the first time it is patched, as
  for the APC40): **Play** is GO, **Rewind** is Back, **Stop** is STOP, the
  fader is the Grand Master. **Play**, **Stop**, **Record** and **Loop**
  have lamps and follow [the lamp rule](#what-a-controls-light-says); the
  other keys are free to assign. The map draws the board; every key is on it.
- **Shift guards the stop.** **Stop** sits where a hand lands by accident,
  so on this board the emergency stop is **Shift+Stop**: Stop alone does
  nothing (its lamp stays dim) and the footer says *FaderPort: Shift+Stop
  stops.* for a moment. After a stop, the GO that releases it is
  **Shift+Play** too: a bare **Play** does not release a stop (the footer
  says so); with no stop active **Play** is GO as ever. **Rewind** (Back)
  is never guarded. The keyboard's, the footer's and every other stop stay
  one press, and the stop is always reachable with Shift held. Both are
  options of the board's map (the **FaderPort** footer chip, one click):
  **Stop needs Shift** and **Releasing a stop needs Shift**, saved with the
  show; untick them for a show that wants the single press. A board's
  layout file says which key is its Shift and what it guards by default
  (`"shift"`, `"guards"`); only the FaderPort 2 asks for it.
- **What PreSonus's MIDI protocol says** (the FaderPort owner's manual,
  appendix *FaderPort Multichannel Controller MIDI Protocol*, for its native
  Studio One mode): the fader is pitch bend on MIDI channel 1, both ways
  (the board sends it when moved; the same message moves the motor); the
  touch sensor is note 104 (0x68), 127 touched, 0 let go; **Shift** is note
  70 (0x46), **Stop** 93 (0x5D), **Play** 94 (0x5E), **Record** 95,
  **Rewind** 91, **Fast Forward** 92, **Loop** 86. The profile matches all
  of these. There is no init message: the mode is chosen on the board, by
  holding **Next** while it powers on and pressing **Solo** (Studio One). A
  lamp is note on: 127 on, 0 off, 1 flashing. The manual gives the knob as
  CC 60 (0x3C); the profile has CC 16, which is what the board sends in its
  Mackie modes. If the knob does nothing, its Board Log says which it is.
- Names are the board's own labels where PreSonus's documents confirm them
  (the Mackie set: Arm, Solo, Mute, Shift, Read, Write, Trim, Touch,
  Marker, Nudge, Click, Loop, Rewind, Fast Forward, Stop, Play, Record).
  **Prev** (note 46) and key **25** are not confirmed by a PreSonus document;
  rename them in the profile once the manual says.

## Pages

**A page is a labelled layer of the desk: colour on the faders, moves on
the knobs.** Each drawn board comes with a page set: named pages, each
giving its controls a job (an attribute of what you are editing: *Red*,
*Pan*, *Pan fine*, *Zoom*). The map labels every control with its job on
the page on now, the colour faders in their colours, and the footer chip
names the page: **APC40 · Move**.

- **Design only.** In Design a page edits what the encoders edit: the look
  open in the Look Editor, else the selected fixtures or group of the
  scene in Build. One move is one undo step. In **Operate** a page does
  nothing and its controls are the show's again: a trigger or a Virtual
  Console control you assigned to one works as you set it up, its light
  too (in Design a page key's light is the page's). While a Learn
  listens, the page rests. In Design
  the page has them, and the map names the show's job as kept from it.
  [Edit Live](edit-live.md) lets a page trim the running cue in Operate.
- **A fader picks up first.** A page's fader does nothing until it meets
  the value it edits, then follows; the Workbench line says where to move
  it (*Red: move it to 50%*). After a page switch or a new selection every
  fader picks up again, so a value never jumps.
- **A knob turns.** It moves the value from where it is. A knob marked
  *fine* turns the same attribute by the small step: on the APC40, knob 1
  is pan, knob 2 pan fine.
- **Never the desk's.** This computer's GO, Back, STOP and Grand Master
  are on no page; a page set that puts a job there is refused.

| Board | Pages | How you switch |
|---|---|---|
| **APC40 mk2** (*Build*) | **Move**: device knobs 1, 2 pan and pan fine; 3, 4 focus and zoom; 5, 6 tilt and tilt fine; 7, 8 gobo and gobo rotation. **Beam**: zoom, focus, iris, frost, strobe, shutter, prism, prism rotation. On every page: faders 1-6 red, green, blue, white, amber, UV, fader 8 intensity; track knobs 1, 2 hue and saturation. | **Pan** (Move) and **Sends** (Beam), lit when on; **Device ◀ ▶** step |
| **APC64** (*Build*) | **Colour**: touch strips 1-7 red, green, blue, white, amber, UV, intensity. **Move**: pan, tilt, zoom, focus, iris, gobo, intensity. The pads stay free; strip 8 is the GM. | Turn the **encoder** |
| **PMJ Black 1** (*Pages*) | **Looks** (on at start): the faders edit the open look (a colour look: 1-6 R G B W A UV; a dimmer look: 1), else the intensity of selected fixture N; encoders pan, tilt, hue, saturation. **Fix Cont**: faders 1-6 colour, 10 intensity, encoders pan, tilt, zoom, focus; press it again for gobo, gobo rotation, prism, iris. **Groups**: fader N is the level of the open scene's group N. **Effects** and **Macros**: nothing yet (their keys dim; the map and the Workbench say *Effects: coming*). Every fader picks up first; **1-Up…10-Up** zero the open look's channel on any page. | The mode keys, the lit one is the page |
| **MaizeDMX** (*Colour*) | **Colour**: faders 1-6 red, green, blue, white, amber, UV; 7, 8 hue and saturation; 10 intensity. **Move**: faders 1-8 pan, tilt, zoom, focus, iris, gobo, gobo rotation, frost; 10 intensity. | **X7** (Colour) and **X9** (Move), lit when on |

**Which set, and sharing one.** The **Pages** row over each map picks the
set this board uses **on this computer** (by the device, like this
computer's desk), names the page on now and how to switch. **Export…**
saves the set as a `.pageset.json` file; **Import…** adds one made for
this board (it is checked first: a set for another board, or one with a
job on the desk's controls, is refused with the reason). The built-in sets
cannot be changed: export one, edit the file, import it under a new name.
A set in your page sets folder that puts a job on the desk's GO, Back,
STOP or GM (edited by hand) is not used, and the Pages row says why.
A show saved with a set other than the board's own remembers it; opened on
a computer that uses another, the row offers it (**Use It** or **Not
Now**). Making pages in the app is the next step.

A show's **encoder bank** on the same board wins: while it is on, that
board's pages rest.

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
  nothing, and so does an encoder turned in it: an encoder sends turns,
  not a place, so nothing it drives moves on the close, and its next turn
  counts from where it is now. The **emergency stop**
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
| **no light** | A trigger learned on another board (the PMJ's channel 9 note 1) on a universe whose drawn board has no such control: it sends no lamp, so an APC pad never blinks for a control it does not have. |
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
- **Ch 1-10** and **Enc 1-4** do the job of the page on now ([Pages](#pages)):
  on **Looks** the faders edit the open palette (Colour: R G B White Amber
  UV; Dimmer: intensity) or, with fixtures selected, their intensity, and
  the encoders pan, tilt, hue and saturation. **1-Up…10-Up** zero that
  fader's target on Looks. These are **Design only**: in Operate they are
  refused with the sentence above.
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
  choose the page (see [Pages](#pages)).

### What the lamps mean

One rule for the whole board:

- **Lit**: a press does something now.
- **Blinking**: what is in use now. The board blinks it itself (every half
  second), so every blinking lamp is in step.
- **Dim**: the key has a job, but nothing to do now.
- **Dark**: the key has no job.

| Key | Lit | Blinking | Dim or dark |
|---|---|---|---|
| **Go** | The Run tab's GO would play a cue now. | | Dim: there is a cue list, but nothing left to GO. Dark: no cue list. |
| **Back** | The list is running, so Back has a cue to go back to. | | Dim: the list is stopped. |
| **Mode buttons** (Effects, Groups, Looks, Macros, Fix Cont) | A page you can switch to. | The page in use: **Looks** until you press another (see [Pages](#pages)). | Dim: a page with no jobs yet (Effects, Macros). |
| **1-10**, **1-Load…10-Load** | That strip has a fixture of the scene open in Build (strip 1 the first). | That fixture is selected. | Dark: no scene open, or the scene has no fixture for that strip. |
| **1-Up…10-Up** | That fader edits the open palette now (the Looks page). | | Dark: it edits nothing now. |
| **O**, **Set** | Ready: a press turns Blackout or Blind on. | Blackout or Blind is on. | |
| **A key you learned** | A press would fire it now. | What it fires (a hit, a scene) is running. | Dim otherwise ([What a control's light says](#what-a-controls-light-says)). |
| **The emergency stop's key** | At every moment: it always works. | Lit and dim in turn: the show is stopped (ALL OFF) until the next GO. | |

The app sends each lamp on its button's own note and MIDI channel (the PMJ
Black 1's buttons are all MIDI channel 9), whatever channel the MIDI
plugin's feedback line is set to.

#### A lamp lights under the wrong button, or not at all

The board's own setup decides which note each LED listens on. Out of the
box an OpenDeck board's LEDs listen on MIDI channel 1 in factory order, so
the app's lamps light nothing, or light the LED under another button (on
one such board the Effects page lamp lit under 1-Load, Set's under
Effects, Blackout's under 10-Load, and Go's under none). The app cannot fix that from
here; the board's setup can. With **qlcplus-midi-profiler**:

1. Read it, writing nothing: `qlc-midi opendeck dump OpenDeck -m maps/pmj-now.json --raw`.
2. If its LEDs don't listen on their buttons' notes, pair them by pressing
   each one (`qlc-midi opendeck identify`, it restores the board after),
   then `qlc-midi opendeck align` (it backs the board up first) so every
   LED answers its own button's note on channel 9.

**Lamp Log…** on the PMJ Black 1 row shows the last lamps the app sent,
each with its note, MIDI channel, velocity and bytes, so you can tell a
lamp that was never sent from one the board did not light. Its **Lamp
Test** lights one button's lamp at every level and blink the board has,
one step every second and a half: say what each step looks like (a colour
means that LED is an RGB one).

The **PMJ Black 1** row of **Connections ▸ Control surfaces** says the same.

## A board is set up when you plug it in

Plug in a board the app knows (APC40 mkII, APC64, APC mini mk2, FaderPort
2, PMJ Black 1, and any other with a built-in profile) and it is set up by
itself: its input is patched with its profile, its feedback goes to the same
device's output, and its init goes out (the APC40 mkII's Ableton mode, the
APC64's display). It goes on the universe it had on this computer last time
when that one is free, else on the first free universe (a new one when
none is). A notice says so, *Akai APC40 mkII ready on universe 5*, with
**Undo**: Undo lets it go, and it is not set up by itself again on this
computer until you patch it by hand. A board the show already has, or
waits for while it is unplugged, is left to the show's own patch.

**A board already patched is put right.** When a board the show has comes
online (plugged in again, the show opened, the app started), its universe
is checked: a feedback that is none, points at another board, at a network
session (*Network RTP Session 1*) or at a port that is gone moves to the
board's own port; a profile that is none, not installed or another board's
becomes its own; then its init goes out. A notice says what was done,
*PMJ Black 1: feedback repaired to its own port*, with **Undo**, and the
Board Log has a *Repaired* line. Undo puts the patch back as it was and
leaves it alone on this computer from then on. A feedback you set on
purpose to another port that is here and is not a board is never touched.

The name the computer gives the board is matched loosely: case, a trailing
*(APC64)*, port words such as *MIDI 1* or *Port 1*, and the maker's name do
not matter, but another model's word does (an *APC40 mkII* is never taken
for an *APC40*). A board with several ports is set up on the one its
profile uses: the APC64 on **APC64 DAW**, the APC mini mk2 on its
**Control** port. Its other ports show under it in **Connections ▸ Control
surfaces** as *part of APC64 (not used by qlcconsole)*; nothing to patch
there.

The **MaizeDMX** shows up as **USB2.0-MIDI Port 1**, a name many cheap
USB MIDI cables share (its chip, USB 1a86:752d, is a generic one), so it
is never set up by itself. The notice asks *USB2.0-MIDI Port 1: is this a
MaizeDMX Playback Controller?* with **Use It**; one click sets it up and
this computer remembers the answer.

**Connections ▸ Control surfaces** lists every MIDI input device plugged
in, patched or not: the boards set up, each with its universe, profile,
feedback line and init; a known board not set up yet (*APC64 DAW (APC64):
Akai APC64, not set up*); an unknown device (*Launchkey MK4 49 MIDI Out:
not set up*). Click one of the last two for its menu: **Use as** its
profile, **Another Profile**, or **Not a Control Surface** (it is hidden on
this computer). A board the show knows that is unplugged stays in the list,
greyed, *not connected*.

## A board's menu and its Board Log

Each board has a chip in the footer (**APC40**, **FaderPort**). A click on
it opens the board's map. The **▾** beside it, or a right-click on the chip,
opens the board's menu:

- **Map**: the board's map.
- **Send Init Again**: its init now (Ableton mode, the APC64's display).
- **Profile**: the profiles that fit this device, the one in use ticked.
  Choosing another patches the board again with it and sends its init.
- **Feedback**: *Paired to* its output, **Pair**, or **None**.
- **Lamp Test**: lights each of the board's lamps in turn, then all of
  them, then gives them back to the show. It needs a feedback line; without
  one the menu says *Feedback ▸ Pair first*.
- **Board Log…**: what the board sent and was sent.
- **Set Up…**: the board's [controller setup](#setting-up-a-controller).

The **Board Log** keeps the last 500 MIDI messages in and out of each
board, newest last: the time to the millisecond, **IN** or **OUT**, the
bytes, and what they are (*Note 93 on, ch 1, velocity 127 (Stop)*, *Pitch
bend, ch 1 = 8192 of 16383 (Fader)*, *SysEx (12 bytes)*). The app's own
steps are in it too, marked **--**: the board recognised and patched, its
profile chosen, each init with the port it went out of (or why it could
not go), the motor fader sent to the Grand Master, a fader move not taken
because the Grand Master waits for it to pick up, a STOP refused because
Shift was not held. **Copy** puts the whole log, with the board's input,
profile and feedback line at the top, on the clipboard for a report.
**Clear** empties it. The map's options row has **Lamp Test…** and **Board
Log…** too.

## Setting up a controller

Some controllers need a setting on the device itself before their profile
works: a mode, or lamps that listen on the notes their buttons send. That
is part of setting the controller up, in **Connections ▸ Control surfaces ▸
Controller setup ▸ Set Up…**. No other tool is needed. The row says what is
known for each controller patched with a profile the app knows (with none,
*All patched controllers are set up*); **Set Up…**
checks the device (a read only) and offers the fix.

- **PMJ Black 1.** As it leaves the factory its lamps are in index order
  (lamp 1 on note 1, MIDI channel 1), not under their buttons, so every lamp
  the app lights is on another key. The check reads the board's lamp table
  and says so plainly: *54 of 54 lamps aren't under their buttons*.
  **Align Lamps** sets each lamp to its own button's note and channel
  (which lamp sits under which button ships with the PMJ Black 1 profile).
  Before anything is written the board's whole setup is backed up to a file,
  and the dialog shows its path. **Undo Align** puts that backup back (what
  the board had just before is backed up too, so the Align can be redone by
  aligning again).
- **Another OpenDeck board** (no pairing shipped): **Identify Lamps…**
  flashes each lamp in turn; press the button under it, or **Skip** when
  nothing flashes. **Finish** puts the board back as it was and keeps the
  pairing on this computer; then **Align Lamps**. While it listens, the
  board's keys do no job (no GO, no Back, no trigger); the emergency stop
  and this computer's STOP still stop. Only Identify lights the board
  meanwhile; when it ends, the show's lamps come back.
- **Akai APC40 mkII.** Its Ableton mode (the pads take the app's lights
  and stop toggling their own) is sent when it is patched, again after a
  replug and after the computer wakes. **Send Again** sends it now. The MIDI
  plugin's **Initialization message** is no longer needed for it.
- **Novation Launchpad Mini MK3.** Its programmer mode is sent the same way.
- **Akai APC64** (on its *APC64 DAW* port). When it is patched, after a
  replug and after a wake the app takes its display and writes *qlc* and
  *console* on it; when the app closes the display is given back. Its pads
  match the profile only in **Session** mode, which no message can set:
  **Check Mode** says *Press SESSION on the APC64, then press any pad*, and
  the pad's MIDI channel tells (channel 7 is Session). That press does no
  job.

Nothing is written to a device's memory unless you press **Align Lamps**,
**Undo Align** or **Identify Lamps…**. In Operate the dialog asks first,
since the board's buttons and lamps pause for a few seconds while it is
written. The backups are in the app's data folder, under *Controller
backups*, in the format of the
[qlcplus-midi-profiler](https://github.com/sandinak/qlcplus-midi-profiler)
tool this is ported from, so either one can restore the other's.

**Rescan**, beside **Set Up…**, looks for a controller just plugged in or
switched on. When it finds none it says so: *No new controller found. The
computer itself doesn't see one: check its power (the APC40 mk2 and many
pads need their own adapter), the cable (charge-only cables carry no data),
and the port.*

## Other surfaces

- **Akai APC40 mk2**: the input profile **Akai-APC40-mkII** gives the pads
  their LED feedback. Patch it on MIDI channel **Omni**: each track's fader
  and buttons send on that track's own MIDI channel (1-8, the master on 9),
  so on one channel only track 1 is heard. For the pads to take the lights
  and stop toggling their own, the APC must be in Ableton mode: the app
  sends it when the APC is patched, after a replug and a wake ([Setting up
  a controller](#setting-up-a-controller)). Learn a clip pad on a cue, a hit or a scene; the
  encoder bank has an **APC40 (knobs)** preset.
- **Akai APC64**: press **SESSION** on the board and patch its **APC64 DAW**
  MIDI port with the input profile **Akai APC64**: everything is on that one
  port. Its 64 pads send on MIDI channel 7 and its buttons on channel 1 (the
  same note numbers, kept apart by channel), its 8 touch strips as pitch bend
  on channels 1-8, its encoder relative. A pad's light takes the Akai colour
  palette by its value. The encoder turns and pushes (one control on the
  map, **Encoder (push)**, free for now); its screen shows three short
  lines (*qlc*, *console*, then the page or role).
- **MaizeDMX Playback Controller**: the input profile **MaizeDMX Playback
  Controller** (ten strips of three keys, an LED and a fader; the master
  column; ten X keys; GO and BACK). Its keys light by the note they send.
  See [The surface map](#the-surface-map) for its default roles.
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

**Plugged in, set up by itself (053 boards)**

Start with the app open and a show loaded, every board unplugged.

1. Plug in the **APC40 mkII**. *Expect:* within a second a notice *Akai
   APC40 mkII ready on universe N* with **Undo**; an **APC40** chip and a
   **▾** in the footer; the pads go dark and light only from the app
   (Ableton mode). Click **▾ ▸ Board Log…**. *Expect:* lines *Recognised
   APC40 mkII as Akai APC40 mkII*, then two **OUT** lines *F0 47 7F 29 60 00
   04 41 09 00 05 F7 … Init (Ableton mode), on patch, to APC40 mkII* and
   *… again, after its port opened*. If the pads are still not in Ableton
   mode: **▾ ▸ Send Init Again**, then **Copy** the log and send it.
2. Click **Undo** on the notice. *Expect:* the chip goes. Unplug and replug
   it. *Expect:* no notice (it is not set up by itself again). Patch it by
   hand in Connections. *Expect:* from then on it is set up by itself again.
3. Plug in the **APC64**. *Expect:* *Akai APC64 ready on universe N*; its
   display says *qlc console*; in **Connections ▸ Control surfaces** one
   row *APC64 DAW (APC64)* with *Notes*, *MIDI* and *Custom* folded under
   it as *part of Akai APC64 (not used by qlcconsole)*, and no *not set
   up* line for them.
4. Plug in the **APC mini mk2**. *Expect:* *Akai APC Mini mk2 ready* on its
   *Control* port; its *Notes* port folded under it. Open its map and move
   fader 1. *Expect:* the fader keeps its name, and its value (*50 %*) is a
   small number under it.
5. Plug in the **MaizeDMX**. *Expect:* a notice *USB2.0-MIDI Port 1: is
   this a MaizeDMX Playback Controller?* with **Use It**. Click it.
   *Expect:* set up with its feedback line; **X7** lit (Colour page). **▾
   ▸ Lamp Test**. *Expect:* each LED and key lamp lights in turn, then all,
   then the show's lamps come back; the Board Log has an **OUT** line per
   lamp. Unplug and replug: *Expect:* set up by itself, no question.
6. Plug in something that is no board (a keyboard, the Launchkey). *Expect:*
   **Connections ▸ Control surfaces** lists it *…: not set up*; click it,
   **Not a Control Surface**. *Expect:* it is gone from the list.
7. **A stale feedback.** In Connections set the PMJ's universe's
   **Feedback** to *Network RTP Session 1* (or any other board), then unplug
   the PMJ and plug it back. *Expect:* *PMJ Black 1: feedback repaired to
   its own port* with **Undo**; the PMJ's lamps light; the Board Log says
   *Repaired PMJ Black 1 … (it was Network RTP Session 1)*. Reopen the show
   with the same stale patch: *Expect:* the same notice.
8. Unplug a board that is set up. *Expect:* its row in Control surfaces is
   grey and says *not connected*; plug it back: it is back, not grey.
9. Click a footer chip's **Design mode** chip and then press a board's
   Stop without Shift, one after the other. *Expect:* one message at a time
   in the footer, never two drawn over each other, and never under another
   chip's words.

**FaderPort 2: what it really sends (053 boards)**

1. Plug in the FaderPort 2. *Expect:* *PreSonus FaderPort 2 ready on
   universe N*, a **FaderPort** chip.
2. **▾ ▸ Board Log…**, then **Clear**.
3. Touch the fader, move it from the bottom to about the middle, let go,
   and wait two seconds. *Expect:* **IN** *Note 104 on … (Fader touch)*, a
   run of **IN** *Pitch bend … (Fader)* lines, *Note 104 off*, then one
   **--** *Motor fader sent to the Grand Master* and an **OUT** *Pitch
   bend* at the place you left it. The fader stays there. If it drives
   back, the log says why (a *not taken … pick-up* line, or the **OUT**
   value).
4. Hold **Shift**, press **Stop**, let go of both. *Expect:* **IN** *Note
   70 on (Shift)*, *Note 93 on (Stop)* and **--** *STOP: the desk stops*;
   the show stops. Press **Stop** alone. *Expect:* **--** *STOP refused:
   Shift … is not held* and the footer says *FaderPort: Shift+Stop stops.*
5. Turn the knob a few clicks each way.
6. **Copy**, and send the log to Claude with the build number.

**APC boards and the MaizeDMX (053 bench)**

1. Patch the **APC40 mkII**'s port with no profile (leave the profile on
   *None*). *Expect:* it is given **Akai APC40 mkII**, the pads go to
   Ableton mode (they light only from the app), an **APC40** chip is in the
   footer. Unplug and replug it: *Expect:* Ableton mode again.
2. Open Control surfaces with the old QLC+ copy of the APC40 mkII profile
   still in your user folder. *Expect:* the Controller setup line names that
   file as older, the knobs on the track channels move their map dials, and
   the file is untouched.
3. Patch the **APC mini mk2** (its *Control* port) with no profile. *Expect:*
   **Akai APC Mini mk2**, a map of 64 pads, the scene keys, the faders and a
   chip; **Clip Stop** is GO, **Solo** Back, **Shift** STOP,
   **Slider 9** the Grand Master; **Left** and **Right** are lit (pages) (when no other board holds them).
4. Patch the **APC64**'s *DAW* port with no profile, in Session mode.
   *Expect:* **Akai APC64**; its display says *qlc console*; **PLAY** is GO,
   **RECORD** Back, **STOP ALL CLIPS** STOP, touch strip 8 the Grand Master.
5. Open a show with a trigger learned on the PMJ, with an APC patched.
   *Expect:* no pad blinks at row 8, column 2 (or anywhere else the APC
   has no control for the trigger).
6. Patch the **MaizeDMX** with its profile and feedback line. *Expect:* the
   LEDs above the faders look like lamps, not knobs; **X7** is lit (Colour),
   press **X9**: it lights, X7 goes dark, faders 1-8 are pan to frost (the
   map says so); press **X7** to go back.
7. With the PMJ or FaderPort holding the roles and an APC64 plugged in too,
   nothing moves. Unplug the FaderPort and patch (or replug) the APC64.
   *Expect:* a notice *GO, Back, STOP and Grand Master moved to APC64 from
   PreSonus FP2* with **Undo**; PLAY is GO at once; **Undo** puts the roles
   back on the FaderPort's controls.

**PreSonus FaderPort 2**

1. Put the FaderPort 2 in native mode (MIDI channel 1) and patch it with the
   profile PreSonus FaderPort 2 and its feedback line. *Expect:* a map of
   the board under Control surfaces, a **FaderPort** chip in the footer,
   *GO is PLAY · Back is REW · STOP is STOP · GM is Fader*.
2. Move the Grand Master on screen. *Expect:* the fader's motor follows it
   to the new place and the Grand Master does not jump.
3. Set the Grand Master to 50%, then pull the fader to the bottom and push
   it up past 50%. *Expect:* a pull down is taken at once; a raise takes
   over where the fader meets the master.
4. Touch the fader without moving it, then let go. *Expect:* the **Fader
   touch** key lights on the map while touched; nothing in the show changes.
5. Start a cue list and look at the board. *Expect:* **Play** is lit, **Stop**
   and **Record** as the show says; **Stop** is lit while ALL OFF holds.
6. Press **Play**, then **Rewind**. *Expect:* GO fires the Run tab's list,
   Back steps back.
7. Press **Stop** alone. *Expect:* nothing stops; the footer says *FaderPort:
   Shift+Stop stops.* and the Stop lamp stays dim.
8. Hold **Shift** and press **Stop**. *Expect:* ALL OFF (the footer says
   *held: emergency stop*); the Stop lamp is lit.
9. Press **Play** alone. *Expect:* the show does not restart; the footer says
   *FaderPort: stopped. Shift+Play releases the stop.* Then hold **Shift**
   and press **Play**. *Expect:* the stop is released and the list GOes.
10. Turn the knob a click each way. *Expect:* its dial on the map turns the
   way the knob does.
11. Put a finger on the fader and move it from 100% down to about 30%, then
   let go. *Expect:* the Grand Master follows your hand to 30% and the
   footer's GM stays at 30%; the fader stays at 30% (it does not drive back
   to 0 or to where it was).
12. In Design, click the footer's **Design** chip. *Expect:* the show stays
   in Design and the footer says where Operate is (Control ▸ Operate).
13. Click the **FaderPort** chip in the footer, at 1024 x 640 too. *Expect:*
   the chip has a small controller glyph; the map of that board alone
   opens (an **All Boards** button brings back the rest) with **Set Up…**, **Stop needs Shift** and **Releasing a
   stop needs Shift** (both ticked) in its header; **STOP** reads
   *Shift+STOP*, **Shift** is drawn as the board's own key. Untick **Stop
   needs Shift**: **Stop** alone now stops.

**Feedback patch (any board with lamps or motors: FaderPort 2, APC, PMJ, MaizeDMX)**

1. Patch the board's input on a universe, then in Connections remove its
   feedback (the universe's right-click, **Remove Feedback**). *Expect:* the
   lamps go dark and Control surfaces says *no feedback line* with a **Patch
   Feedback** button.
2. Unpatch the input and patch it again. *Expect:* the status beside the
   Connections toolbar says *Feedback goes back to* the board, the lamps
   light and the fader follows the Grand Master, without touching feedback.
3. Remove feedback again and press **Patch Feedback** in Control surfaces
   (or in Controller Setup). *Expect:* the lamps light at once.
4. With two outputs of the board's name (or none that matches), press **Patch
   Feedback** under **Boards without feedback**. *Expect:* Connections opens
   with the board's input selected and says *Right-click the board's output,
   Patch a Universe Here (feedback)*; nothing is patched by guess.

**PMJ Black 1**

1. Patch the PMJ as an input on any MIDI channel with the feedback line and
   the profile PMJ Black 1 (no restart). *Expect:* **O** and **Set** glow
   lit at once and **Looks** blinks (the page in use; Groups and Fix Cont
   lit, Effects and Macros dim); **Go** is lit when
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
   GOes (in Design too, as the Run tab's GO); press twice quickly (within
   a quarter second) and the second is held (*Second GO held: cue 1 still
   starting*). **Back** steps back. Then
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
    The blinking mode button is **Looks**; press **Groups** and Groups
    blinks, Looks lit again. Then with **Go** learned on the main cue list (step 6) and the
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

**Surface types**

1. Patch a **Behringer BCF2000** with its profile, put a VC **Level**
   slider (Catch values on) on fader 1 at 80 % and the fader at the
   bottom; move the fader a little. *Expect:* the motor drives the fader to
   80 % and the slider follows from there; it never drops.
2. On the same BCF2000 put a VC **Grand Master** slider on fader 8 and
   move the GM on screen. *Expect:* the motor follows the GM; moving the
   fader moves the GM at once, up and down (a motor master follows).
3. Patch an **APC mini** (mk1 or mk2), put a VC **Grand Master** slider on
   **Slider 9**, GM at full. Pull Slider 9 to the bottom. *Expect:* the GM
   goes down with it at once, from the first move. Push it back up from the
   bottom while the GM sits at 30 % (set on screen). *Expect:* nothing until
   the fader meets 30 %.
4. On the APC mini check the keys its profile names: **Clip Stop** (GO),
   **Solo** (Back), **Stop All Clips** (stop) are the scene column's top,
   second and bottom keys. *Expect:* the names match the board (Q318: the
   keys confirmed on the bench).

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
