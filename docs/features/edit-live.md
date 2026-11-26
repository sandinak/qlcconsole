# Edit Live

**Edit Live trims the running show from the board.** In Operate, the
board's page controls (see [Control surfaces ▸ Pages](control-surfaces.md#pages))
move what the running cue puts on the selected fixtures, and **Store**
writes only what you moved into the look that sets it. It is like
recording a look, but smaller: one attribute of the fixtures you touched,
nothing else.

## Turning it on

- **Run tab** (Operate): the **Edit Live** switch in the header, beside the pad's
  button.
- **Footer**: the **Edit Live** chip (shown in Operate when a board with
  pages is patched). A click turns it on.

Anyone at this computer can turn it on, but a show with a PIN
(**File ▸ Show Settings**) asks for it first. While it is on, the footer
chip is red, **EDIT LIVE**, with the number of trims, and it never hides.
Edit Live is a **Standard** level tool (**View ▸ Workspace**): in a
Student show, a Teaching show included, the switch and the chip are greyed
and say so.

Edit Live turns itself off, and lets go of its trims, when you go back to
Design, when the show is closed, at the emergency stop, and when the show
goes down to Student. While a controller's setup listens to the board
(**Identify Lamps**, a mode check) or an **Assign to Control…** window is
open, the board's controls trim nothing; the page's key lights come back
when the setup lets go.

## Trimming

1. Select the fixtures (the programmer selection: the 2D view, a group, the
   PMJ's select keys).
2. Move a page control: a knob or encoder turns from where the value is
   on stage; a fader waits until it meets the value on stage (pick-up),
   then follows.

Only the channels you moved sit over the cue (at the Simple Desk's place in
the output, under Highlight, Locate, Focus and Park; the Grand Master and
Blackout still apply). The show file does not change until you Store.

A trim is a move from where things are. On stage it starts from what the
stage shows now (an override hit's colour, a fade half way: the first touch
never jumps), and a fader picks up at what the stage shows. **Store** adds
the same move to the look's own value: with the cue list's master at 50%, a
red taken from 0% to 25% on stage is stored as 50% in the look, which the
list plays at 25%. The trimmed intensities follow the list master, so a
master pulled down takes them with it and what Edit Live puts up is never
brighter than the cue would play. (A list fading out at a Stop is not
followed: the trims go when the cue does.)

Edit Live trims intensity, colour, position (pan and tilt), zoom, focus,
iris and frost. Gobo and prism wheels and strobe are set in Design: their
controls stay the show's while Edit Live is on (a trigger on them still
works).

A cue with sub-cues: the sub-cue playing is the one trimmed, and a Sub GO
drops its trims as GO does.

The Grand Master, STOP, GO and Back are this computer's desk and never
part of a page: they work as always.

## The chip's popup

Click the red chip to see each trimmed setting, as *fixture · attribute ·
from → to* (**Front 1 · Red · 100% → 50%**), and:

- **Store to Cue N**: for each setting, finds the look setting it in the
  running cue -- the cue's own layer, or the earlier cue it tracks from --
  and writes the trimmed value there. Only the touched attributes: the
  colour of a layer changes, its intensity and position do not. It is
  written into the layer that sets that attribute on the fixture (a layer
  that sets only the level is not given a colour). When every head of the
  layer moved by the same amount, the layer's look moves by it, its fan or
  formation kept (movers of different kinds: only when every kind moved
  the same angle; else each kind keeps its own). An attribute the cue does
  not set at all (a focus left at the fixture's default) is stored as what
  the stage shows. A colour look is one colour for all of a fixture's
  heads: a pixel bar whose heads now differ is not stored (the status line
  says so, *heads differ, set colour in Design*) and its trim stays on
  stage, pending, until GO or Drop. Some of the layer's heads with one result get one exception (a
  child layer) on just those heads. Only heads that now truly differ get
  one each, so the next GO plays exactly what was on stage. All of it is
  **one undo step**, and the status line names what was stored where
  (*Stored Position of Spot in cue 1 Open (it tracks into cue 2 Blue)*).
  The running cue plays the stored look from its next GO, so the stored
  values stay on stage until then: the chip says **EDIT LIVE · Held Until
  GO**. **Drop** lets go of them; undoing a Store lets go of what that
  Store held (an earlier one stays), and redoing it holds it again.
- **Drop**: the trims go at once; the cue's own values are back (with
  nothing pending, the stored ones held until GO go).
- **Turn Edit Live Off**.

## GO with trims not stored

GO never waits for Edit Live. The trims drop with the old cue, and the
footer offers **Store to Cue N** for 30 seconds: one click stores them into
the looks that set them in that cue, as Store would have.

## Boards

Every board with surface pages (APC40 mk2, APC64, MaizeDMX). The PMJ's
faders keep their own rules (its overlay) and do not trim in Edit Live.
