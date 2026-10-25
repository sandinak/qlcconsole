# Focus calls

Focusing the rig when the show moves into a new room, call by call, with
the controller in your hand on the stage.

A **focus call** is some fixture groups and the point they focus to —
"DH Tspots → DSC", then "AT spots → Soloist DS". The show keeps its calls,
in order, so every venue is focused the same way. Running them is Focus
mode (see [Focus mode](lighting-studio.md#focus-mode)) a call at a time:
the lights of the call come up aimed at their point, you step through
them and nudge each until it lands, and **A** saves the correction **for
this venue** — never into the show.

## Setting up the calls

**Lighting Studio → Focus calls**, or **Run → Focus calls…**, opens the
list.

- **+ Call** adds one; **Up** / **Down** set the order they are run in;
  **Remove** takes one out.
- **Groups** — tick the fixture groups the call focuses. Their fixtures
  are worked in stage order (front to back, then house left to right).
- **Focus to** — a stage target (see [Stage targets](stage-targets.md)),
  or **Where they land** for fixed lights, which are focused by moving
  where their beam lands (see
  [Fixed lights](lighting-studio.md#fixed-lights-pars-and-fixed-spots)).
- **Name** — optional; without one the call is named from its groups and
  target ("DH Tspots → DSC").
- **Note** — a word for the crew, shown on the panel while the call runs
  ("cut off the proscenium").

**Save calls** keeps them in the show (they are saved with it).

![The Focus calls dialog: the calls in order, a call's groups and point, the venue](../images/focus-calls-dialog.png)

1. The calls, in the order they are run.
2. The groups the selected call focuses.
3. The point they focus to.
4. The venue the corrections are saved for (see below).
5. Start runs them.

## Venues: corrections that stay with the room

What you correct in a venue belongs to that room, not to the show. Choose
the **Venue** before you start:

- **New venue…** — type its name ("Lincoln HS"), or leave it empty and it
  is named with the date and time ("Venue 2026-10-23 19:04").
- **An existing venue** — its corrections are put back on the rig, and you
  carry on from them.
- **None** — saves go into the show's own calibration, as Focus mode
  always did.

A venue's corrections are kept in a file **next to the show**, named after
it: `Spring Show.qlcc` keeps them in `Spring Show.venues.xml`. Every **A**
writes that file at once, so quitting or a crash loses nothing. The show
file is never changed by them: while a venue is applied the rig shows its
corrections, and **File → Save** still writes the show's own calibration.
When the show is opened again, the venue it was last focused for is
applied again.

**Any calibration change is the venue's while it is applied.** A pan or
tilt offset typed in the fixture's properties, a fixed light's aim or a
lens set by hand goes into the venue and its file at once, not into the
show. (A Focus mode nudge goes in when it is saved with A.) Typed back to
the show's own value, the venue's correction for it goes. With **None**
applied, those edits change the show as always. Choosing another venue (or None) takes one venue's
corrections off and puts the other's on.

The venue file holds, per fixture: a moving head's pan and tilt offsets,
a fixed light's aim (its pan and how far down it points), a lens zoomed
by hand, and which fixture type and mode it was. It also holds the room's
**truss trims** and **target positions** (below).

**Still at the venue?** When you open a show whose applied venue was last
changed more than about a day ago, the laptop asks "Still at Lincoln
HS?":

- **New venue** (the default): name it, or leave it empty for the date.
  Its corrections start from the show's own; Lincoln HS's are kept.
- **Still at Lincoln HS**: carry on with it.
- **None**: the show's own calibration.

The footer shows **Venue: Lincoln HS** while a venue is applied. The
Focus calls dialog does not pick an old venue for you either: until it is
confirmed, **New venue…** is chosen, with a note naming the old one.

**A fixture that changed.** A correction remembers the fixture's type and
mode. If the light is now another fixture (a lamp swapped for a mover,
another mode), its correction is not applied: the light **needs focus**.
Focus mode's panel says so on it, the plot shows an orange **!** on it,
and the focus calls say "Needs focus: the fixture changed." Focus it and
save: the venue then keeps it as it is now. A correction for a fixture
deleted from the show is kept (it may come back) and listed. A venue
file from before types were kept is applied as it is, with a note once.

**Trims and targets.** A room's pipes hang at other heights and its marks
sit elsewhere. With a venue applied, trimming a truss or moving a stage
target (in the Lighting Studio, or with the target page below) goes into
the venue's file, never the show's. **None** puts the show's own back.

**Seeing and clearing one.** **Corrections…** beside the venue lists the
applied venue's corrections: each light (needs focus, or no longer in the
show, marked), each trim and target. **Clear** puts the one you pick back
to the show's own.

**The file travels.** **File → Collect for Transfer…** copies the venue
file with the show. When you open a show that has no venue file of its
own, but one beside it belongs to the same show under another name (an
earlier version: `DanceHysteria-0.5.venues.xml` beside
`DanceHysteria-0.6.qlcc`, or a Finder copy), the laptop offers to use its
venues. The other file is not changed.

## Running them

**Start focus calls** brings up the first call: its fixtures aimed at
their point, the first one lit and the rest of the call dimmed. The panel
says the call ("Call 2 of 5: AT spots → Soloist DS") and its note, which
fixture, the scope, and where saves go.

![Focus calls running: the call and its note, the scope, the controller](../images/focus-calls-panel.png)

1. The call being focused, and its note for the crew.
2. One fixture or the whole call (Y), and where saves go.
3. The controller, labelled with what each control does here.

| Do | Controller | Keyboard |
|---|---|---|
| Next / previous focus call | RT / LT | PgDn / PgUp |
| Next / previous fixture | RB / LB | ] / [ |
| One fixture / the whole call (scope) | Y | G |
| Nudge (coarse, fine) | Left stick, right stick | Arrows (⇧: 1°) |
| Zoom in / out | D-pad up / down | , / . |
| Focus tighter / looser | D-pad right / left | ' / ; |
| Save for this venue | A | Return |
| Back to what was saved | B | Backspace |
| Zero | X | 0 |
| Undo the last save | View (on release) | ⌘Z |
| Target page: move this call's target | LS (left stick click) | T |
| How many are not saved | View + RS | N |
| Say the light | RS (right stick click) | W |
| Feedback: lights / voice / both | RS held | ⇧W |
| Save every light not saved | A held | ⇧Return |
| This light back to its last save | B held | ⇧Backspace |
| The next light not saved | LB + RB together | U |
| Leave | Menu | Esc |

Held, a D-pad arm keeps zooming or focusing until you let go; the
bumpers and triggers step once a press. **A** must be held for a full
second to save every light, and a hold never answers a warning: only a
fresh press does. **View** undoes when you let it go, so View + RS (how
many are not saved) does not undo.

**X** puts a light back to the show's own calibration while a venue is
applied ("Back to the show's calibration."), not to zero.

**The target page.** Press the left stick (or T) and the stick moves the
call's **target** instead of the light: right is across the stage, up is
upstage. Every head aimed at it follows, like a focus palette on a
lighting desk. **A** keeps the target (for the venue applied, else in the
show), **B** puts it back. Leaving the page or stepping calls with a move
not kept is refused until you choose.

**While you nudge,** the panel says what has changed since the light was
last saved ("Since the save: +2.5° pan, −1.0° tilt") and the plot shows a
dashed ghost where the saved focus lands. **A save** says the light
and where it went ("Spot 3 kept for Lincoln HS.") and makes an Xbox pad
buzz — twice if another light is still unsaved. **Leaving** with lights unsaved says how many ("2 lights not
saved.") before the panel asks.

**The lamp** is colour-blind safe: **blue** with a tick, all saved;
**yellow** with "!", this light; **orange**, another light. The number not
saved is on its rim. While the light you are on is not saved, it
"breathes" slowly on the rig (with lights feedback).

**The pad's light bar** follows the panel's lamp. On a Mac this goes to pads the app reads through
Apple's GameController framework; today that is Xbox pads, which have no
coloured light, so you see it only on the panel (see FOCUS_REFINE_DESIGN.md
§11).

**The whole call** (Y): a nudge moves every fixture of the call by the
same amount — the truss came in a little low — and **A** saves them all,
one undo step. A bumper picks one fixture again.

## A focus-call cue (pre-show)

Put the focus at the top of a pre-show cue list, so the list and the focus
are one checklist:

1. Make a cue for it (its looks are the preset: work light, or the first
   call's lights parked at their point) and tick **Focus call** on its
   cue page. The F column says **focus**.
2. **GO** onto it in Operate. The cue's looks play and the focus calls are
   **armed**: nothing moves yet. The laptop says "Focus calls ready.
   Press Start to begin." and the cue page, the Run tab and the footer say
   **Focus calls: waiting for the controller**.
3. When the crew is ready, press **Start** (Menu) on the controller — or
   **Start focus calls** on the Run tab, or **⌘↩**. The first call comes up,
   blinking, as the voice says it. First the laptop always confirms the
   venue: "Still at Lincoln HS? Answer on the laptop, or wait for a new
   venue." (with none applied: "Name this venue on the laptop, or wait
   for today's date."). Press **Still at Lincoln HS**, or type a name and
   press Return; after 8 seconds a new venue is made, named with today's
   date, so an old room is never corrected by mistake. Typing holds the
   clock.
4. Work the calls as below. **Menu**, or **RT** on the last call, ends
   them (asking about anything unsaved) and the list goes on to the next
   cue — only if it is still on the focus cue. Ended any other way (say,
   Refine started), the voice says "Focus calls ended." and the list stays
   where it is.

Starting Focus mode or Refine while another of them has work not saved is
refused, saying what to do first; nothing is thrown away.

## Telling the operator where they are

Whoever holds the controller is on the stage, looking at the rig, not the
screen. Two things tell them where they are:

- **The lights.** On every change — a call, a fixture, the scope — what
  is picked blinks three times (120 ms on, 120 ms off) and then stays
  lit; the rest of the call stays dim.
- **The laptop's voice.** It says where you are, briefly. Feed the
  laptop's output to the crew's headsets and it is their cue: a line is
  never cut off or spoken over, and when you step quickly only the latest
  "where" line waits (the fixtures you passed are not read out).
  Confirmations ("Spot 3 kept.") and warnings are queued and never
  dropped. **Voice to:** in
  the Focus calls dialog sends it to a chosen output (a Mac's audio
  interface for the headsets, say).
  - stepping to a call: "Call 2 of 5, AT spots to Soloist DS. Fixture 1 of 4."
  - a fixture: "Fixture 3 of 8."
  - the whole call: "All 4, group."
  - A, B, X, View: "Saved." "Reverted." "Zeroed." "Undone."
  - **Say the light** (right stick click, or W) says the light's name
    first and then where you are — "Spot 3. Call 2 of 5, AT spots to
    DSC. Fixture 3 of 8." — and blinks it.

**Feedback: lights, voice or both.** The **Feedback** setting on the panel
(Focus mode, Focus calls and Refine share it; it is this computer's, not
the show's) picks how you are told:

- **Lights and voice** (the default): the blinks and the voice.
- **Lights only**: the blinks (and the panel's lamp); nothing is said.
- **Voice only**: the voice; the rig is not blinked.

**Say the light** speaks whichever it is — you asked. Hold the right stick
click (or press ⇧W) to switch; the voice says the new one ("Feedback:
lights only.").

**Important lines are always spoken,** in Lights only too, and the rig
gives a slow long blink and the pad one long buzz (a save is a quick
double blink and a short buzz):

- warnings and refusals ("Spot 3 not saved.", "This call has no target
  to move.");
- the venue asked;
- **the controller lost**: if the pad drops out (asleep, unplugged,
  flat), everything it held is let go — the sticks, a held button — so no
  light drifts and no hold fires: "Controller lost. Lights held." When it
  is back: "Controller back.";
- **nothing reaching the rig**: Blackout on, the Grand Master at zero, or
  the light's universe with no output ("Spot 3's universe has no output:
  it shows nothing.");
- **another pad**: with two pads, the first to press Start (or, for a
  tool started on the laptop, any button) has the controls; the other is
  ignored and told "Another controller is in use." The panel names the
  pad that has it.

The voice is the Mac's own (`say`); on Linux `spd-say`, elsewhere Qt's
speech, if the computer has one. [Refine](refine.md) speaks the same way.
