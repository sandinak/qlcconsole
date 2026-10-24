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
a fixed light's aim (its pan and how far down it points), and a lens
zoomed by hand. Copy it along with the show to take a venue's focus to
another computer.

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
| Undo the last save | View | ⌘Z |
| Say the light | RS (right stick click) | W |
| Feedback: lights / voice / both | RS held | ⇧W |
| Save every light not saved | A held | ⇧Return |
| This light back to its last save | B held | ⇧Backspace |
| The next light not saved | LB + RB together | U |
| Leave | Menu | Esc |

Held, a D-pad arm keeps zooming or focusing until you let go; the
bumpers and triggers step once a press.

**While you nudge,** the panel says what has changed since the light was
last saved ("Since the save: +2.5° pan, −1.0° tilt") and the plot shows a
dashed ghost where the saved focus lands. **A save** says the light
("Spot 3 kept.") and makes an Xbox pad buzz — twice if another light is
still unsaved. **Leaving** with lights unsaved says how many ("2 lights not
saved.") before the panel asks.

**The pad's light bar** follows the panel's lamp — green all saved, yellow
this light, red another. On a Mac this goes to pads the app reads through
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
   blinking, as the voice says it. If no venue is applied, the laptop first
   asks for the venue's name ("Name this venue on the laptop, or wait for
   today's date."); type it and press Return, or after 8 seconds it is
   named with today's date. Typing holds the clock.
4. Work the calls as below. **Menu**, or **RT** on the last call, ends
   them (asking about anything unsaved) and the list goes on to the next
   cue.

## Telling the operator where they are

Whoever holds the controller is on the stage, looking at the rig, not the
screen. Two things tell them where they are:

- **The lights.** On every change — a call, a fixture, the scope — what
  is picked blinks three times (120 ms on, 120 ms off) and then stays
  lit; the rest of the call stays dim.
- **The laptop's voice.** It says where you are, briefly. Feed the
  laptop's output to the crew's headsets and it is their cue: a line is
  never cut off or spoken over, and when you step quickly only the latest
  one waits (the fixtures you passed are not read out). **Voice to:** in
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

The voice is the Mac's own (`say`); on Linux `spd-say`, elsewhere Qt's
speech, if the computer has one. [Refine](refine.md) speaks the same way.
