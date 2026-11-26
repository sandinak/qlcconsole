# Edit Live

**Edit Live trims the running show from the board.** In Operate, the
board's page controls (see [Control surfaces ▸ Pages](control-surfaces.md#pages))
move what the running cue puts on the selected fixtures, and **Store**
writes only what you moved into the look that sets it. It is like
recording a look, but smaller: one attribute of the fixtures you touched,
nothing else.

## Turning it on

- **Run tab**: the **Edit Live** switch in the header, beside the pad's
  button.
- **Footer**: the **Edit Live** chip (shown in Operate when a board with
  pages is patched). A click turns it on.

Anyone at this computer can turn it on, but a show with a PIN
(**File ▸ Show Settings**) asks for it first. While it is on, the footer
chip is red, **EDIT LIVE**, with the number of trims, and it never hides.

Edit Live turns itself off, and lets go of its trims, when you go back to
Design, when the show is closed, and at the emergency stop.

## Trimming

1. Select the fixtures (the programmer selection: the 2D view, a group, the
   PMJ's select keys).
2. Move a page control: a knob or encoder turns from where the value is
   on stage; a fader waits until it meets the value on stage (pick-up),
   then follows.

Only the channels you moved sit over the cue (at the Simple Desk's place in
the output, under Highlight, Locate, Focus and Park; the Grand Master and
Blackout still apply). The show file does not change until you Store.

Edit Live trims intensity, colour, position (pan and tilt), zoom, focus,
iris and frost. Gobo and prism wheels and strobe are set in Design.

The Grand Master, STOP, GO and Back are this computer's desk and never
part of a page: they work as always.

## The chip's popup

Click the red chip to see each trimmed setting, as *fixture · attribute ·
from → to* (**Front 1 · Red · 100% → 50%**), and:

- **Store to Cue N**: for each setting, finds the look setting it in the
  running cue -- the cue's own layer, or the earlier cue it tracks from --
  and writes the trimmed value there. Only the touched attributes: the
  colour of a layer changes, its intensity and position do not. Trimmed
  fixtures that are only some of a layer's get their own exception (a
  child layer) for it. All of it is **one undo step**, and the status line
  names what was stored where (*Stored Position of Spot in cue 1 Open (it
  tracks into cue 2 Blue)*). The running cue plays the stored look from
  its next GO, so the trims stay on stage until then.
- **Drop**: the trims go at once; the cue's own values are back.
- **Turn Edit Live Off**.

## GO with trims not stored

GO never waits for Edit Live. The trims drop with the old cue, and the
footer offers **Store to Cue N** for 30 seconds: one click stores them into
the looks that set them in that cue, as Store would have.

## Boards

Every board with surface pages (APC40 mk2, APC64, MaizeDMX). The PMJ's
faders keep their own rules (its overlay) and do not trim in Edit Live.
