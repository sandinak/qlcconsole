# PMS on-site checklist (2026-12-08)

Dance Hysteria (DH) at the PMS Auditeria: the first time the show runs the
room's own rig through **New Venue from a Show**. This is the paper you take
with you. The help page's numbered steps ([venues.md, On site:
checklist](../features/venues.md#on-site-checklist)) say what each button
does; this page says what to bring, the order to do it in, and what to
write down for us.

Lengths are in feet and inches, as the app shows them by default
(**Settings ▸ Units**).

## What was rehearsed at home

`ui/test/pmsrehearsal` runs the whole visit offscreen, on copies of
DanceHysteria-0.6 and PMS Stage.qlcc (2026-11-25). End to end it works:
the venue is made from the menu, all 69 PMS lights come in as House on
one universe, all 16 PMS groups are held for later, the clash check flags
the output, the chip is amber, Fill Roles suggests five roles, all six
house cues reach only PMS's lights, the venue survives a save and reopen,
a USB interface chosen on the venue page clears the clash, and switching
back to the office leaves DH exactly as it was. Nothing was sent to any
network.

What it measured, for the room:

- The room is lined up on its **Cyclorama 2** and the file's centre line:
  PMS stands 10' toward stage right and 6' 4" upstage of where its file
  has it. The proscenium is 4 1/4" stage left of that centre line.
- PMS's main drape lands **18' 9"** downstage of DH's upstage line. DH's
  own main drape is 21' 7" downstage of it at home, so DH's stage is about
  2' 10" deeper than PMS's.
- DH's downstage platforms reach about **1' 10" past PMS's main drape**
  (about 10" past the proscenium line). Check this first on site.
- The proscenium opening is 36'; DH's deck is 32' wide, so it fits across
  with about 2' each side. The main drape's open gap in the file is
  26' 3", narrower than the deck.
- Of DH's 27 stage targets, 3 land outside PMS's stage: **Announcer** and
  **Line 1** (about 8' 4" downstage of the main drape, out on the apron if
  there is one) and **Horns 2** (outside the opening).

## Before leaving

- [ ] **Macs**: ender goes; the show Mac (most likely kronk) goes too. Both
      have the PMS RC installed in /Applications (`make rc HOSTS="kronk
      local"`), opened once, Local Network allowed.
- [ ] **The show**: DanceHysteria-0.6.qlcc **and** DanceHysteria-0.6.venues.xml
      side by side on both Macs. The PMS venue lives in the .venues.xml:
      a show copied without it has no PMS venue.
- [ ] **PMS Stage.qlcc** on both Macs (for **Line Up the Room** or making
      the venue again on site).
- [ ] **Mark DH's spare universe as House** before you make the venue:
      Connections ▸ Universes, select the universe you keep for the room
      (U79, "Universe 79", has nothing on it), **Slot ▸ House**. Without
      it, New Venue from a Show suggests the first empty universe, which in
      DH is **U16 "Effect-Mode"**.
- [ ] **Make the PMS venue at home** (venue chip ▸ New Venue… ▸ From a
      Show…, name it "PMS Auditeria", Output in this room: **Not yet**),
      then **Fill Roles** and save. On site you only choose the output.
- [ ] **USB DMX interface**: the one you will use, tested at home on any
      DMX fixture (it must show under Connections as a DMX USB output when
      plugged in). Take a spare if there is one.
- [ ] **Cables**: two 5-pin DMX cables long enough to reach the booth's
      DMX point and the SL wings point, a 5-pin to 3-pin adapter each way,
      a terminator, a USB extension for the interface.
- [ ] **DH's network**: the switch and Art-Net nodes for DH's own rig, as
      at home. Note the Mac's show-network address (172.18.2.x): DH's
      outputs wait for that network and go out by themselves once the Mac
      is on it.
- [ ] Tape measure, the printed **Ref Point 1–10** markers (Augment3d
      calibration targets: they carry no dimensions, so measure where you
      put them), gaff tape, a flashlight.

## Arriving

1. Plug the USB DMX interface into the Mac and the room's DMX line (booth
   or SL wings) into it. **Expected:** nothing on stage changes.
2. Open the show. **Expected:** the footer chip says **Venue: PMS
   Auditeria · 1 clash**, amber (the output is still Not yet).
3. Venue chip ▸ **Rig and Universes…**. **Expected:** "This venue's
   universe" names the room's universe; "Its output here: none".
4. **Output in this room ▸ USB DMX interface**, pick the interface, **Use
   This Output**. **Expected:** the Output line leaves the clash list and
   the chip goes plain (no amber). If it stays amber, note the remaining
   clash word for word.
5. Clash check clean: the list says **No clashes**. Nothing should say
   "still sends what PMS Stage.qlcc had" (that is the office's Art-Net,
   172.18.2.219): if it does, the output was left at "As the file had it":
   choose the USB interface again.

## The first 15 minutes

1. **Dark test**: Blackout on. **Expected:** stage dark; PMS's house,
   work and hall circuits stay as they are (they ignore Blackout and the
   Grand Master by design). Blackout off.
2. **House to half** (the Global hit). **Expected:** the house lights go
   to half over 5 s. PMS's house circuits are relays (Ch 15, Ch 24) and
   we send them 50%: **note whether they come on, stay off or flicker.**
3. **House out**. **Expected:** the house goes out over 5 s.
4. **Stage up**. **Expected:** FOH (Ch 51–62) and the 1st and 2nd
   electrics (Ch 101–113, 201–212) come up full white over 3 s, the cyc
   lights (Ch 301–305) stay out.
5. **Stage out**. **Expected:** all of those out over 3 s; house and work
   untouched.
6. **Work lights**, then let go. **Expected:** Ch 17 on, then off.
7. **Flash** each Fill Roles row (venue page ▸ Roles ▸ Fill Roles…):
   the lights that come up are that row's. Untick a wrong row.
8. **Walk the targets**: stand on CenterStage, Announcer, Line 1 and
   Horns 2 and look at where the plot puts you against PMS's main drape
   and proscenium. Put a Ref Point marker on DSC and on the plaster line
   and measure each from the cyc and the centre line.
9. **The drape**: measure where DH's upstage edge (its drape or cyc) will
   hang against PMS's Cyclorama 2. Type it in Rig and Universes ▸ Room ▸
   **Drape downstage of Cyclorama 2 by** (a negative number is upstage).
   **Expected:** PMS's lights move on the plot; DH's do not.
10. One DH tour light up through DH's own Art-Net node. **Expected:** it
    comes up as at home; PMS's lights don't move.

## What to note for us

- Which Mac ran it, the build (Help ▸ About), the interface's name.
- Each step above that did something other than **Expected**, in your
  words, with the time.
- The relay answer (step 2): on, off, or flicker at half.
- Measured: cyc to main drape, cyc to plaster line, apron depth (plaster
  line to the stage edge), proscenium opening width, main drape's open
  gap, and where the Ref Point markers sat.
- Any PMS light whose place on the plot is wrong (its channel and where it
  really hangs).
- Anything you had to explain to yourself twice.
