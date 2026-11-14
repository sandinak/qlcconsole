# Show control (QLab)

QLab — or anything else that speaks **MIDI Show Control** (MSC) or **OSC**
— can fire qlcconsole's cues by number: QLab runs the sound and video, and
its cues GO the lights.

Everything that controls or clocks the show from outside is set up in one
place: the **Show control & sync** rows at the bottom of **Connections →
Devices** — **OSC in**, **MIDI Show Control in** and **MIDI timecode**, each
with its state beside it ("Port 8000 · listening", "Off"). Click a row to
set it up on the right, with what it last heard. (It used to be in the
Settings dialog, then a sub-tab of its own; your settings carried over
unchanged.)

## Cues and cue lists

A **cue list** is a chase (a chaser). Its steps are the **cues**.

- **Cue numbers.** The chaser editor has a **Cue** column. A step with no
  number of its own answers to its position — 1, 2, 3… — shown in grey, so
  a chase works with no setup. Type a number to give a step its own ("12.5",
  "101"); it keeps it when steps around it move. **Renumber Cues** numbers
  every step 1, 2, 3… in the order they are now.
  Numbers match as numbers: "12.5" is "12.50".
- **The main cue list.** Tick **Main cue list** in one chase's editor: a
  command that doesn't name a list goes there. A show with only one chase
  needs no main list — that one is used.
- **Numbered lists.** Give a chase a **Cue list** number to drive it by
  name: MSC's cue-list field, or `/list/2/…` in OSC.
- **Skipping a step.** Untick a step in the chaser editor's **Step** column
  (or right-click → **Skip This Step**; several at once work too) to leave
  it out without deleting it: GO, Next, Back and the steps that run on by
  themselves pass over it. It shows greyed and struck through, in the
  editor and in the Virtual Console cue list. Asked for by number — GO cue
  4, or picked in the cue list — it still plays. Tick it to put it back.

## What's next, and GO from anywhere

- The footer's **Next:** says what the main cue list's GO plays next — its
  cue number and name. It is status only: click it for the **Run** tab,
  where GO, Back, Stop and LOAD are.
- **GO** is ⌘↩ and **Back** ⌘⇧↩, in every window, and in
  **Control** → GO / Back (Main Cue List).
- From a game controller: [Pad Run](#go-from-the-game-controller-pad-run).
- A **Virtual Console cue list** shows a **Cue** column beside the step
  number and a **Next:** line under its list — the cue, its name and its
  note.

## GO from the game controller (Pad Run)

Stand anywhere with an Xbox, DualShock 4 or DualSense controller and run
the show from it. The controller has two **roles**:

- **Tools** (the default): Focus mode, Venue focus, Focus this cue and the
  follow spot, as before. A controller picked up mid-show never moves it.
- **Run**: the controller runs the show.

Switch with the **Pad:** button in the **Run** tab's header (**Pad: Tools**
/ **Pad: Run**). The choice belongs to this computer and is kept for next
time. Its tooltip shows the button map in the words of the pad you're
holding; the footer's **Pad** chip shows the same map as a table when you hover it (it also names a press the pad refused and how to leave Walk), the whole Run tab is framed in orange while the arm is held, and **Settings → Advanced → Game controllers → Controller Test…** lists what each
button does in the Run role (**Run: GO (LB held)**).

While the role is Run the footer says so too, from every tab: **Pad:
Run**, **Pad: ARMED** (orange) or **Pad: Walk**, beside the follow spot's
◎ Follow chip when one plays.

**Nothing moves the show unless LB (L1) is held.** Holding it arms the
pad: the light bar turns orange and the Run tab's button says **Pad:
ARMED**. While it's held:

| Control | Xbox | PlayStation | Does |
|---|---|---|---|
| hold | LB | L1 | arm |
| + | A | Cross | **GO** |
| + | B | Circle | **Back** |
| + | Y | Triangle | **Sub GO** |
| + hold 1 s | X | Square | **Stop** the list (a tap only says "Hold X to stop the list.") |
| — | RT / LT | R2 / L2 | nothing |
| — | RS click | R3 | say the list and what's on standby |
| + | RB | R1 | fire the **armed hit** (below) |
| hold 1 s | View + Menu | Create + Options | switch the role: Tools / Run |

- **GO is confirmed on the pad.** A GO flashes the light bar blue and gives
  a short buzz. A long buzz means there was nothing to play, and the
  laptop says so if the voice is on.
- **An unarmed press does nothing.** The voice says "Hold LB to arm.",
  and the footer's Pad chip shows it for a few seconds, also with
  feedback set to lights only.
- **Stop takes a hold.** X sits next to A, so a slip must not stop the
  list: hold LB, then hold X for a second. A tap of X does nothing but
  say "Hold X to stop the list." (on the footer's Pad chip too). Let go
  of LB during the hold and nothing stops.
- **The list is the Run tab's.** The pad's GO plays the list the Run tab
  plays: the current production's main list, or the one picked in the
  list box in the Run tab's header, beside the list's name (**Current
  production (…)** or *production · list*; it shows when the show has
  more than one list). The pick is saved with the show. The pad cannot
  change it: sight unseen, a wrong list is too easy. RS says which list
  it is.
- **Nothing on the pad leaves Run** but holding View + Menu for a second
  (below). Menu walks the show and Menu again comes back, still in Run.
- **The same buttons on every pad.** The map goes by button, not by
  channel, so a [controller slot](other-improvements.md#one-input-whichever-controller-is-here) that finds a
  DualSense in place of the Xbox pad keeps it.
- **One path.** A pad GO is the same GO as ⌘↩ and the Run tab's button:
  the previews hold for it, and in Design it offers Operate.
- **The tools win.** A Focus mode or Focus this cue started on the laptop
  takes the controller until it ends. The Run tab's button and the footer
  name it meanwhile (**Pad: Venue Focus**, **Pad: Focus This Cue**). In the Run role, View + Menu doesn't
  start Focus mode, and Menu alone walks the show (below). A focus-call
  cue's Start (Menu) still begins its calls.
- The sticks stay the follow spot's in either role. While a follow spot
  plays, the right stick is its fine stick and **D-pad Up / Down** step its
  speed ([How fast, and how fine](stage-targets.md#how-fast-and-how-fine)).

### Fire a hit from the pad

With the arm held, **RB (R1)** fires the hit that is armed right now. A
[scoped hit](show-tree.md) is armed while its place is live: under a cue
that is running, in a cue list, in the current production or everywhere.
Where several are armed the **narrowest** wins, exactly as on a Virtual
Console button holding them all (cue, then list, production, everywhere).
Use it to call a one-shot, say a breathe, off a hit in a cue.

- The voice says its name ("Breathe."). Press again and it lets go
  ("Breathe off."); a one-shot that has finished plays again.
- Two or more hits at the narrowest place take turns, one a press.
- With no hit armed the pad says "No hit armed." and buzzes long. Without
  LB held it says "Hold LB to arm", as for GO.
- Hits fire in **Operate** only, as a button does. It is not a run command:
  it starts the hit's look the way a VC button's press does.

### DualSense extras: triggers and swipes

Two things a PlayStation DualSense adds to the Run role. Neither is needed:
everything has a button, and an Xbox pad simply has no triggers to feel.

- **Adaptive triggers.** In the Run role the triggers do nothing, so they
  stay loose, armed or not. While walking, both
  triggers give a light click at the end of each pull, one click a cue
  stepped. The triggers go back to off when you disarm, leave the walk or
  the Run role, lose the pad, or quit.
- **Touchpad swipes.** One finger across the touchpad steps. One swipe is
  one step, and a swipe never fires a GO: it previews and selects.
  While walking: swipe **right / left** for the next / previous cue and
  **down / up** for the next / previous layer. In the Run map, not walking:
  down / up do nothing (the list is the Run tab's), and right / left start
  a walk from the next / previous cue (Design only; in Operate the cue
  moves with GO). A second finger, a click, a slanted drag and a swipe too soon after
  the last do nothing.

While walking, one finger now swipes, so aiming with the touchpad needs LB
held (two fingers still zoom and focus).

decided (can change): swipes need no arm: they preview and walk, never GO.

### Switch roles from the pad

Hold **View + Menu** (Create + Options) for a second and the controller
changes role: Tools to Run, or Run to Tools. The voice says which
("Run role. Hold LB and press A to GO." / "Tools role.") and the Run tab's
**Pad:** button follows. Neither button does anything alone, and any third
button pressed during the hold cancels it, so a knocked pad cannot take the
show. In Tools, View + Menu released before the second is still Focus mode,
now on its release. Not while a tool (Focus mode, Focus this cue) has the
pad: end it first.

decided (can change): RB with LB is the hit gesture, so the Run map still
has a free D-pad and LS; a hold of 1 s is the role chord.

decided (can change): GO needs a modifier held (LB), not a double-press;
Back, Sub GO and Stop need it too, so there is one rule. Stop also needs
X held for a second, because it sits beside GO.

decided (can change): the pad never picks the cue list; the Run tab's
list box does, and the show keeps it.

### Walk the show from the stage

Stand on stage with the controller in the Run role and press **Menu**
(Options): the pad walks the show. It is **Focus this cue** on the cue in
hand, with its own map for moving around, so you can look at each cue and
each layer where the audience will see it, fix what the pad can fix, and
tag the rest for later.

- **The cue in hand.** In Design, the cue picked in the Show Tree, else
  the list's standby cue. In Operate, the cue the list is running.
- **Focus This Cue** opens on the laptop, so whoever is there sees where
  you are.

While walking:

| Xbox | PlayStation | Does |
|---|---|---|
| RT / LT | R2 / L2 | next / previous cue. In Design it is previewed, never a GO |
| RB / tap LB | R1 / tap L1 | next / previous layer ("Layer 2 of 4: Movers."). LB steps on its release, because it is also the modifier |
| hold LB + LT / RT | hold L1 + L2 / R2 | **level** of the walked layer, group or fixture, down / up. Pull harder to go faster. Let go of the trigger and the voice says the level ("Level 60%.") |
| hold LB + sticks | hold L1 + sticks | **the light itself**: turn the picked light on its target (below) |
| hold LB + A | hold L1 + Cross | keep the light itself |
| View | Create | **solo** the layer, or play the whole cue again |
| View + B | Create + Circle | undo the last keep or tag |
| tap Y | tap Triangle | the scope: layer, group, fixture (the bumpers then step groups or fixtures) |
| hold Y | hold Triangle | tag the cue **Needs work** (hold again to clear it) |
| Menu | Options | stop walking (back to the Run map) |

The sticks, the D-pad, A (keep), B (back to the cue), X, LS and RS do what
they do in [Focus this cue](refine.md). Position, zoom, focus, level, colour and gobo can be changed. Every keep
is one Show Tree step, so undo takes it back here or on the laptop.

#### Level

Hold LB and pull LT to bring the level down, RT to bring it up. A full
pull moves half the range a second. The change starts from what the cue
gives it, shows on the rig at once, and A keeps it in the cue as the
layer's own intensity (at Group or Fixture scope, an exception), the same
as a zoom or a colour. One level for the scope: a fan in the old
intensity is not kept.

#### Zero a light on its target

A light that is twisted on its mount misses its target in every cue that
aims it there. Fix it once, from the stage:

1. Walk to a cue that aims the light at a target (an Aim position).
2. Tap Y until the voice names the light (Fixture scope).
3. Hold LB and move the sticks until the beam sits on the target. The
   left stick is coarse, the right stick fine.
4. Still holding LB, press A to keep it.

This moves the light's own calibration, not the cue. Every cue that aims
it at a target uses the new aim. It goes where [Venue focus](focus-calls.md)
saves its corrections: into the venue that is applied, or into the show's
own calibration when no venue is. Keeping it is one Show Tree step, so
View + B (or undo on the laptop) puts the light back.

- Without LB the sticks still do what Focus this cue does. On a layer
  aimed at a target they are refused, and the voice says to pick the
  light and hold LB.
- At Layer or Group scope the light itself is refused ("Pick the light
  first"), and on a light this cue doesn't aim at a target too.
- B, or leaving the cue, puts a light not kept back where it was.
- **In Operate** it follows the quiet Refine rules: only while the cue
  runs. When GO moves on, it is refused, because nothing is sent.

- **Solo** plays only the walked layer and its exception layers, as the cue
  plays them. A light that a higher layer takes over stays dark. The solo
  follows you to the next layer and the next cue.
- **Needs work** marks the cue in the Show Tree. Write what needs doing in
  the cue's note on the laptop. See [Needs work](show-tree.md#needs-work).
- **Nothing is thrown away.** A cue step or Menu with a change not kept
  is refused, and the voice says to keep it (A) or go back (B) first.
- **In Operate** the walk follows the quiet Refine rules. Only the
  running cue can be walked, GO moves it, nothing blinks or dims, and solo
  is refused.
- **No GO while walking.** Menu first, then hold LB and press A.
- A focus-call cue that is armed still takes Menu to begin its calls.
  When the laptop then asks "Still at *venue*?", **hold A** for a second
  to say yes, same room. A tap does nothing; another room is answered on
  the laptop or by voice.

Everything above is on the buttons, the same on both kinds of pad. The
PlayStation touchpad adds [swipes](#dualsense-extras-triggers-and-swipes).

decided (can change): Menu enters and leaves the walk. View solos, so the
undo moves to View + B. A tap of Y is still the scope and a hold tags.
Solo is for Design only. A cue with nothing to walk is passed over.
Hold LB is the modifier for level and for the light itself, so a tap of LB
steps back a layer when it is let go. The sticks without LB stay the cue's.
The light itself works on one light at a time, at Fixture scope.

### Check it on a real pad

A morning check of Pad Run on each pad you will use: an Xbox One pad, a
PS4 DualShock 4 and a PS5 DualSense, one after the other in one sitting.

**Before you start.** Open a show with:

- a main cue list of at least three cues, and a second cue list (so the
  Run tab shows its list box);
- a [scoped hit](show-tree.md) under the main list's first cue;
- a [focus-call cue](focus-calls.md) at the top of a third list (Pre-show).

Put the show in **Operate**, turn the voice on, and set the Run tab's
button to **Pad: Run**. Connect one pad at a time, and check that
**Settings → Advanced → Game controllers → Controller Test…** sees it.

**For each pad**, in this order (Xbox name first, PlayStation second):

1. Press **A** (Cross) without LB. *Expected:* nothing plays; the
   footer's Pad chip reads **Pad: Run · Hold LB to arm.** (L1 on a
   PlayStation pad) for a few seconds, and the voice says it.
2. Hold **LB** (L1). *Expected:* the Run tab's button and the footer's
   Pad chip say **Pad: ARMED**; on a PlayStation pad the light bar turns
   orange.
3. Still holding LB, press **A** (Cross). *Expected:* the main list goes
   to its first cue; a short buzz; on a PlayStation pad the light bar
   flashes blue, then goes back to orange.
4. Hold LB + **Y** (Triangle). *Expected:* Sub GO runs the cue's next
   sub-cue; with none, a long buzz and the voice says there was nothing
   to play.
5. Hold LB + **A**, then LB + **B** (Circle). *Expected:* GO moves to the
   second cue, Back returns to the first; a short buzz each.
6. Hold LB and tap **X** (Square). *Expected:* nothing stops; the
   footer's Pad chip says **Hold X to stop the list.** (Square on a
   PlayStation pad). Now hold LB and hold X for a second. *Expected:* the
   list stops; a short buzz; no blue flash.
7. Let go of LB. *Expected:* **Pad: Run** again; the light bar goes dark.
8. GO the main list's first cue again (LB + A), then hold LB + **RB**
   (R1). *Expected:* the hit under that cue plays and the voice says its
   name. LB + RB again: it lets go ("… off.").
9. Press **RT** and **LT** (R2, L2), with and without LB. *Expected:*
   nothing happens; the Pad chip still says **Pad: Run** (or **ARMED**).
   Press RS: the voice names the main list.
10. On the laptop, pick the Pre-show list in the Run tab's list box.
    Press RS: the voice names Pre-show. Hold LB + A.
    *Expected:* the focus-call cue arms ("Venue focus ready. Press Start
    to begin."); nothing steps.
11. Press **Menu** (Options). *Expected:* the focus calls begin and the
    laptop asks "Still at *venue*?" and the voice says to hold A. Tap A
    once. *Expected:* nothing is answered (the voice says to hold A).
    Then hold **A** (Cross) for a second. *Expected:* a short buzz, "Still
    at *venue*.", and the first call begins. (With no venue applied, answer
    on the laptop.) Press Menu again to leave the calls.
12. Hold **View + Menu** (Create + Options) for a second. *Expected:* the
    voice says "Tools role." and the button says **Pad: Tools**. Hold them
    again: "Run role. …" and **Pad: Run**.
13. Start the same hold and press A before the second is up. *Expected:*
    the role does not change.
14. In Run, press every other button once, then hold each for two
    seconds: A, B, X, Y, RB, the D-pad, LS, RS, LT, RT, the Xbox (PS)
    button, the sticks. *Expected:* the Run tab's button never reads
    **Pad: Tools**; only View + Menu held leaves Run.
15. Save the show with Pre-show picked in the Run tab's list box, close it and
    open it again. *Expected:* the Run tab plays Pre-show, and LB + A
    GOes it.

**What differs by pad:**

- **Xbox One / Series on a Mac:** no light bar, so steps 2, 3 and 7 are
  checked on the Run tab's button and by the buzz. Before step 1, open
  **Controller Test…**: the pad is listed once (USB: "read via
  GameController framework" or "Xbox USB report (GIP)"). macOS also makes
  a copy of the pad called **GamePad-1** (Microsoft 045E:028E) for other
  apps; it must not be in the list, and no input slot may point at it.
  Press A, B, X and Y one at a time in the test: each lights its own
  box, and D-pad Up lights only D-pad Up.
- **PS4 DualShock 4:** watch the light bar and the buzz: neither has been
  checked on a real pad yet. No adaptive triggers.
- **PS5 DualSense:** as the DualShock 4, plus: R2 stays loose while LB is
  held, and both triggers click once a pull while walking (see
  [DualSense extras](#dualsense-extras-triggers-and-swipes)).

Write down any step that did not give its expected result, with the pad
and how it was connected (USB or Bluetooth).

## Sub-cues

A cue can have cues inside it: cue 4 brings up a look, and **4.1, 4.2,
4.3** step through changes within it on a second button, **Sub GO**, while
GO still goes on to cue 5.

- **Making one.** Use a chase as a step of the cue list (drag it in like any
  function). Its steps are that cue's sub-cues. In the chaser editor the
  step gets a disclosure triangle: open it to see the steps under it,
  numbered, with their fades and holds. They're read-only there —
  right-click → **Edit Chase…** (or double-click one) opens that chase to
  change them. The chase can still be used anywhere else.
- **Sub-cues column.** On the chase step: **Own timing** (the default, and
  what every older show has) runs the chase on by itself, as ever.
  **Sub GO** holds each of its steps until a Sub GO.
- **Playing.** GO into cue 4 plays its first sub-cue (past skipped ones)
  and holds there — the chase's own holds aren't counted. **Sub GO** plays
  the next sub-cue with its own fade; **Sub Back** the one before. At the
  last (or first) one they do nothing. **GO** goes on to cue 5 whatever
  sub-cues are left, and **Back** to cue 3; the chase stops either way, as
  it would. If cue 4 has a hold of its own, it follows on to 5 when that
  runs out. A chase set to **GO on the beat** holds a Sub GO for the beat
  too.
- **Numbering.** A sub-cue is its cue's number, a dot, and its place in the
  chase — 4.1, 4.2… (grey, in the editor) — unless the step has a cue
  number of its own in that chase, which it keeps.
- **Where you see them.** The Virtual Console cue list shows the same
  nesting, with the sub-cue playing highlighted under its cue; a cue list
  with a Sub GO step gets **Sub GO** and **Sub Back** buttons, and clicking
  a sub-cue plays it (its cue first, if that isn't the one playing). The
  footer reads **Sub: 4.2 · Next: 5** while sub-cues are left, or
  **Sub: (end of 4)**; its menu has Sub GO and Sub Back.
- **Keys.** **Sub GO** is ⌥⌘↩ and **Sub Back** ⌥⌘⇧↩ (Ctrl+Alt+Enter and
  Ctrl+Alt+Shift+Enter), in every window, on the main cue list — beside GO
  ⌘↩ — and in **Control** → Sub GO / Sub Back (Main Cue List).

One level only: a sub-cue's own chase steps run on their own timing.

- **In the Show Tree,** sub-cues are made in the tree (+ Sub-cue) and can
  **auto-follow**: see [Sub-cues](show-tree.md#sub-cues).
- **The current production.** With Show Tree productions, a command with no
  list goes to the **current production's** main list — the same list
  GO (⌘↩) and the Run tab play. See
  [Productions](show-tree.md#productions) and
  [The cue runner](show-tree.md#the-cue-runner).

### Sub-cues from QLab

- **By number.** `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play sub-cue 4.2
  — cue 4 first, if another cue is playing or the list is stopped. A main
  cue numbered "4.2" of its own wins; otherwise "4.2" is sub-cue 2 of cue
  4, or a sub-cue whose own number is 4.2.
- **Sub GO and Sub Back** over OSC: `/subgo` and `/subback` (send them from
  a QLab Network cue). They step the cue playing, as ⌥⌘↩ does; an
  argument is ignored. MSC has no Sub GO command — send a numbered GO
  (GO 4.2) instead.
- **LOAD 4.2** (`/cue/4.2/load`, MSC LOAD) puts the sub-cue on standby:
  **Next:** in the footer and in the cue list shows it — "Next: 4.2 —
  Wash" — and the next GO plays it. The Run tab loads a sub-cue too.
- **STOP 4.2** stops the list only when 4.2 is the sub-cue playing.
- When Sub GO can't, **Connections → Devices → Show control & sync** says why:
  "cue 1 has no sub-cues on Sub GO", "end of 4", "4 is at its first
  sub-cue".

## What a command does

| Command | What happens |
|---|---|
| **GO** | With a cue: that cue (or sub-cue, 4.2). Without: the cue on standby (LOAD) if there is one, else the next not skipped — the first, if the list isn't playing. Starts the list if it's stopped. A chase set to **GO on the beat** still waits for its beat. |
| **LOAD** | Puts a cue on standby: the next GO without a cue goes there. Nothing changes on stage. |
| **STOP** | Stops the list. With a cue, only if that cue is the one playing. |
| **BACK** | The cue before the one playing (past skipped ones). |
| **SUB GO** / **SUB BACK** | The next / previous [sub-cue](#sub-cues) of the cue playing; nothing at the ends. |
| **PAUSE** / **RESUME** | Holds the list where it is, and carries on. |
| **RESET** | Stops the list and forgets what's on standby. |
| **ALL OFF** | Stops everything that's running — the panic button. |

## OSC — QLab's Network cue

1. **Connections → Devices → OSC in**: tick **Listen for OSC
   cue commands** and pick a port (53100 by default). It's off until you
   turn it on, because anyone on the network could fire a cue. It takes
   effect at once, and the page shows this computer's address to send to.
2. In QLab, point a network patch at that address and port. If QLab runs on
   the same computer, use 127.0.0.1 — and a port other than 53000 and
   53001, which QLab uses itself.
3. Add a **Network cue** with an OSC message:

| Message | Does |
|---|---|
| `/go` | GO the next cue (or the one on standby) |
| `/go 12.5`, `/go/12.5` | GO cue 12.5 |
| `/cue/12.5/start`, `/cue/12.5/go` | GO cue 12.5 |
| `/cue/12.5/load`, `/load 12.5` | LOAD cue 12.5 |
| `/cue/12.5/stop` | STOP, if cue 12.5 is playing |
| `/go 4.2`, `/cue/4.2/go` | GO sub-cue 4.2 (cue 4 first if need be) |
| `/subgo`, `/subback` | Sub GO, Sub Back in the cue playing |
| `/stop`, `/pause`, `/resume`, `/reset`, `/back` | as named |
| `/subgo`, `/subback` | SUB GO / SUB BACK |
| `/go 4.2`, `/load 4.2` | a sub-cue by its number: GO it (its cue first), or LOAD it |
| `/panic`, `/alloff` | ALL OFF |

Put `/list/2` in front for cue list 2 (`/list/2/cue/5/start`), and
`/qlcconsole` in front of either if you like (`/qlcconsole/go`). A cue
given as an argument can be a number or text; a float is read to the
digits it holds (4.2, not 4.199999809).

## MIDI Show Control — QLab's MIDI cue

1. **Connections**: patch the MIDI port QLab sends on as an **input** of any
   universe (the same as for MIDI timecode).
2. **Connections → Devices → MIDI Show Control in**: **Answer
   MIDI Show Control** is on; set the **device** number QLab sends to (1 by
   default). A message to the all-call device (127) is always answered.
   **Listen on** is **All MIDI inputs** (the default) or the one port QLab
   sends on. The choice names the port, not a universe, and belongs to
   this computer, so it holds for every show. Under it the page says where
   MSC can come in — the universes with that input patched — or, if the
   chosen port isn't patched in this show, says so in red: MSC isn't
   answered then (it doesn't quietly fall back to every input). **Patch a
   MIDI input…** goes to where inputs are patched.
3. In QLab, a **MIDI cue** sending MSC with the command format **Lighting**
   (or Moving Lights, or All Types), the command, and the cue number — and
   the cue list, for a list other than the main one.

MSC's **STOP** holds the cue in progress and **RESUME** carries it on, so in
qlcconsole they are PAUSE and RESUME; **GO_OFF** stops the list. GO, LOAD,
ALL_OFF, RESET and TIMED_GO (as GO) are answered too.

## QLab out — sending a GO on

The other direction: when a cue goes on stage from this console (⌘↩, the
Run tab, the footer, the Virtual Console, a controller), qlcconsole can
send it on to QLab, a sound desk or anything else that listens. It is off
until you turn it on, and it is **this show's** setting, saved with the
show: **Connections → Devices → Show control & sync**, **QLab out**.

- **Send OSC to** a host and port (QLab listens on 53000). The address is
  `/cue/{cue}/start` by default, `{cue}` being the cue number that went, so
  QLab starts the cue with the same number; `/go` sends QLab's own GO
  instead. It leaves from the OSC plugin's output on the network the host
  is on (or the interface you pick under **From**). No new socket and no
  patching needed.
- **Send MIDI Show Control GO to device** *n* on a MIDI output: MSC GO,
  lighting format, with the cue number (`4`, `12.5`, `4.2`), through the
  MIDI plugin's output you pick under **On**.
- **Send** every cue, or only cues with a cue number of their own (set in
  the cue list, or by Renumber). A cue without one is then left out; with
  every cue, its position is its number.

Only a GO that actually puts a cue on stage sends: a GO that finds no such
cue or list sends nothing, and neither does a preview or a Blind edit. A real
GO while Blind is on does send: Blind hides edits from the stage, not a GO.
(A Teaching show can hold show control out, or hold it in Blind; see
[Teaching mode](teaching.md#instructor-settings).)
A command that came in by OSC or MSC is not sent back out, so QLab and
qlcconsole can't GO each other in a loop. **Last sent** on the page shows
what went. Back, Stop and the like are not sent (decided: GO only, can
change).

## Stream Deck (Companion)

Bitfocus Companion can drive a show from a Stream Deck, a Stream Deck +
(with its four dials and touch strip) or any Companion surface, and put the
current and next cue on the keys. The module is `companion-module-qlcconsole`
in the repository's `tools/companion-module/` folder. It talks to the same
web access socket as the phone [Run page](other-improvements.md#the-run-page-phone-and-tablet),
so there is nothing new to switch on in qlcconsole and the same safety rules
apply.

### Set it up

1. In qlcconsole: **Connections ▸ Devices ▸ Show control &amp; sync ▸ Web
   access**, tick **Start web access with this show** and **Reachable from
   other devices on the network** (Companion on the same computer needs only
   the first).
2. **File ▸ Show Settings…**, tick **Allow GO from the phone Run page**.
   This is off in every show until you do. If the show has a PIN, you give it
   to Companion in the connection's settings.
3. In Companion, **Settings ▸ Developer modules path**: the `tools` folder's
   `companion-module` directory (run `npm install` in it once). Add a
   connection of type **qlcconsole**, enter the computer's address and the
   port (9999).
4. Presets: **Run the show** (GO showing the next cue, Back, Stop, Sub GO,
   Hold, Blackout, now/mode displays), **Hits** and **Dials**. Drag them onto
   keys or dials.

It is for the show network: web access is plain http, so keep it off a
shared network. A web access with logins (**-wa**) is not supported by the
module (decided: can change); use it with the show's PIN instead.

### What the keys and dials do

- **GO, Back, Stop, Sub GO, Hold/Resume, GO a cue** (a cue number on the
  main list or a numbered list) are run commands by the web route, so a GO
  holds the Design previews exactly as from the keys. Refused while **Allow
  GO** is off, and until the PIN is given.
- **Hits.** A key fires the hit as a Virtual Console button does: only a hit
  that is armed, the **narrowest** armed place wins, and only in Operate;
  press again to let it go. There are 32 numbered hit keys and one that
  fires "the armed hit". A key lights red while its hit is on and blue
  while it is armed.
- **Blackout** toggles, and shows red while it is on.
- **Dials.** Turn a **group master** dial to move it (5 percent a click, set
  in the connection), press it to go to full, press again to go to 0. The
  **Grand Master** dial is the same. The touch strip shows the name, the
  level and a level bar. Fast spins are added up in the module and sent as
  one message, and the level is held between 0 and 100.
- **Walk the cues dial.** Turning puts the next or previous cue on standby,
  without playing it (the footer's *Next:* and the key show it). **Pressing
  GOes only while the dial is armed**, that is within five seconds of a turn,
  so a knock on the dial does not GO the show. It follows the same Allow GO
  and PIN rules.

### What the keys show

The current and next cue (number and name), the list, production and song
section, Design or Operate, whether remote control is allowed, blackout,
the Grand Master and each group master's level, and each hit's on and armed
state, as Companion variables (`$(qlcconsole:next_num)`,
`$(qlcconsole:next_name)`, `master_1_level`, `hit_1_name`...) and as
feedbacks that colour a key.

### The socket (for other tools)

The module sends one text per command to `ws://host:9999/qlcplusWS`, after
`QLC+RUN|hello` (and `QLC+RUN|pin|…` when the show has a PIN); it hears
`RUN|state|{json}` on every change and `RUN|ack|word|answer` to each
command (`ok`, `nothing`, `refused`, `needpin`, `design`). The words are
`go[|cue[|list]]`, `back`, `stop`, `subgo`, `subback`, `pause`, `resume`,
`standby|next|prev` (a LOAD, no GO), `hit[|id]`, `blackout|on|off|toggle`,
`grand|percent`, `master|group|percent`, `nudge|grand|delta` and
`nudge|master|group|delta` (a signed percent added to the level now, held to
0..100).

decided (can change): nudging the selected layer's dimmer from a dial is not
built; web access has no selection to act on (it lives in the Build tab), and
Design edits are not remote-controlled.

## Checking the hookup

**Connections → Devices → Show control & sync** says whether OSC is listening —
green, or in red why it can't, such as "Can't listen for OSC on port 53100"
when another program has the port (pick another) — and, for OSC and MSC
each, the **last command** that arrived, when, and what came of it: "GO
cue 12.5 — Main", or why not: "no such cue in Main", "no main cue list
(mark one in its chaser)". An OSC message that isn't a cue command shows
its address ("/qlab/hello — not a cue command"), so a typo in QLab shows
up too; an MSC command says which universe's input it came in on. Fire a
cue from QLab and look there first.

The same page's **MIDI timecode** section picks which universe's input
timecode is taken from: MTC on a MIDI input, or Art-Net TimeCode on an
Art-Net input, or LTC played into the audio input (see
[Following timecode](shows.md#following-timecode)) (the same choice as the footer's **MTC** chip) and
saves that choice with the show (it's a universe of the show's patch; a
show that names a universe it doesn't have follows any), and shows the
code as it arrives with its **sync health**: healthy, usable or
unstable, from how fast it runs against real time, its jitter and how
often it updates.

When the music runs from a DAW session, time the cues from their song
sections: a section's start moves its cues with it, Logic's markers (from an
exported MIDI file) or a tap per section set the starts, and **ripple** /
**stretch** shift everything after a cut. See [Song sections on the
timecode](show-tree.md#song-sections-on-the-timecode).

The page's **Web access** section starts the web server (the Virtual
Console in a browser) whenever this show opens, on the port you pick; see
[Web access saved with the show](other-improvements.md#web-access-saved-with-the-show).

## See also

- [Shows (the timeline)](shows.md) — following MIDI timecode instead.
- [Getting started](getting-started.md#running-the-show).
