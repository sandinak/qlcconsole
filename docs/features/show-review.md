# Show Review

Show Review reads the show and lists what is likely wrong with its
**design**, each with the place to fix it: two fixtures on the same
channels, a missing fixture definition, a special from one song still up
three songs later, two hits fighting over the same lights, a cue that
strobes faster than 3 times a second, a sound file outside the show's
folder. It is the audit a programmer runs before tech, and the checklist a
student runs before handing a show in.

It looks only at what is **saved in the show**, so a finding is true on any
computer that opens it. What is true only now, on this computer (a
controller asleep, an Art-Net node not answering, Blackout on) stays in the
footer's readiness list.

## The Review chip and the panel

The footer has a **Review** chip beside the readiness chip:

- **Review · 2 ⛔ 5 ⚠**: 2 blockers and 5 warnings are open (red with a
  blocker, amber with only warnings). Notes are not counted on the chip.
- **Reviewed** (grey): nothing open at warning or above.

Show Review checks the show again a couple of seconds after you change it,
in Design. Click the chip to open the **Show Review** panel. The **Show
readiness** dialog (click the readiness chip) also has a **Show Review…**
button.

### Answering a warning where it shows

A warning reads the same wherever it shows — the chip, a **⚠ cyc** mark on
a Show Tree cue, a row's check id in the panel. **Hover** it for the detail:
the check, what it found (which fixtures, how much, on what, in which cue),
every cue it marks, and what you can do. **Right-click** it to answer it
there, without opening the panel:

- **I Meant It** — light on the cyc kept on the layer that lights it (only
  PWR-12 can be meant).
- **Not a Problem…** — it doesn't matter here: say why (a note is required).
- **Not Meant** — a meant warning goes back to open.
- **Reopen** — a Not a Problem goes back to open.
- **Not Meant: Show Me the Layer** — the layer to aim off, shutter, zoom or
  darken (any other check: **Show Me**, where to fix it).
- **Open in Show Review** — the panel, on that row.

The chip's right-click lists every open warning, each in a submenu of its
own. These are the panel's own answers, so a warning cleared from a mark is
cleared everywhere. I Meant It and Not Meant are one undo step each.

#### Three states

A warning is in one of three states, and the cue says which:

| State | On the cue | In Show Review | Footer chip |
|---|---|---|---|
| **Open** | amber **⚠ cyc** | under Warnings | counted |
| **Meant** | green **cyc ✓**; hover says who marked it, when and the note | under **Meant** | not counted |
| **Not a Problem** | nothing | under **Not a Problem**, with the note | not counted |

Only PWR-12 (light on the cyc) can be **Meant**: there the light is on
purpose and stays in the show as such. Every other check is open or Not a
Problem. Right-click any of them to move between states. The chip's tooltip
sums them up ("2 open · 1 meant · 3 not a problem"); the chip itself counts
open blockers and warnings only. Who and when come from your name on this
computer; both are saved with the show, and an older show opens with its
marks as before (meant, nobody named).

The panel lists findings by severity:

| Severity | Means |
|---|---|
| ⛔ **Blocker** | The show will do something wrong or unsafe, or won't open or play elsewhere. Fix it before the show runs. |
| ⚠ **Warning** | Probably wrong. Often fine once someone has decided. |
| **Note** | Tidy-up and paperwork. Never blocks anything. |

Each row shows its check's id (PAT-01, CUE-07...) and what it found, in the
show's own names. Hover a row for **why it matters**. Show **Open**,
**Not a Problem** or **All**, and type in **Search** to narrow the list.
**Re-check** runs every check again.

- **Fix ▸** (or double-click) goes to the place to fix it: the cue in the
  Show Tree, the function, the Patch dialog on the fixtures, Connections on
  the universe, the Lighting Studio.
- **Not a Problem…** (it was **Not a Problem…**) says the finding doesn't matter
  here. Write a note (it is required) and your name. See [Accepting a finding](#accepting-a-finding).
- **Reopen** takes an acceptance back.
- **Why?** opens the check's paragraph.
- **Turn Check Off…** turns a check off for this show, with a note ("No
  moving heads in this rig").

## Accepting a finding

Every finding ends one of two ways: you **fix** the show, or you **accept**
it with a note. There is no "ignore": an acceptance is saved **with the
show** (who, when and why), so the next person to open it reads the
decision, not a fresh warning.

An acceptance is tied to what the check looked at. When that changes (the
fixture moves, the cue's strobe gets faster), the finding comes back
marked **↺ Changed since marked Not a Problem**, with the old note, because the decision
was about something that is no longer there. A change elsewhere in the show
doesn't reopen it. When the thing it was about is deleted, the acceptance is
dropped at the next save.

Your name comes from the Not a Problem dialog; it remembers the last one typed on
this computer and starts from your account name.

A blocker marked Not a Problem still counts in the summary as such ("0 blockers
open · 1 marked Not a Problem").

## What it checks

The checks built so far, by area. The full catalog, and what comes next, is
in the design (docs/design/SHOW_REVIEW.md in the source).

**Movement** (the Show Tree; heads that can move, with a dimmer)

- **MOV-01** Moves in light: a head's position changes between cues while it
  is lit in both. Often deliberate (a drift with the soloist); otherwise move
  it in the cue before. The message says how far it turns and how long that
  takes at the head's real speed.
- **MOV-02** Can't move in black: a head dark in the cue before and
  repositioned in this one, but move in black can't hide it: it is off for
  the show, the list or the cue, or the dark before the cue is shorter than
  the lead time (or than the head needs to travel, if that is longer). A
  cue that waits for GO has all the time it needs.
- **MOV-08** Not marked before its first cue: heads lit in the first cue of a
  list (or a Block cue that nothing positions them for) away from the middle
  of their range, with nothing before it to put them there.
- **MOV-13** An area left partly dark: a cue whose Aim look at an
  [area](stage-targets.md#area-targets) leaves more than a tenth of it unlit
  at the width the lights' zoom has now. It names the cue, the area, how much
  is lit and which part is missing: lit at head height and on the deck
  both count, so it says "deck dark upstage left" or "heads dark downstage
  centre" ("lights only 72% of it, at head height and on the deck (deck dark
  upstage left)"). Zoom wider, add a light, tilt some in so the beams reach
  the deck, or make the area smaller.

How long a move takes comes from the head's type: a moving head turns 180°
in 0.4 s, a scanner in 0.1 s, pan and tilt together (the slower of the two).
Definitions don't carry a speed, so this is a default; the Rig Overview draws
the same speed (see [Lighting Studio](lighting-studio.md#heads-travel-at-their-real-speed)).

**Patch and outputs**

- **PAT-01** Two fixtures on the same channels (blocker).
- **PAT-02** A fixture running past channel 512 (blocker).
- **PAT-03** Unpatched fixtures: a warning when cues or scenes light them, a
  note when they only wait in the inventory.
- **PAT-04** A fixture on channels the venue keeps (not ours) (blocker).
- **PAT-05** A venue's clash with the show: house fixtures overlapping the
  tour's channels, channels the venue keeps (not ours), a house fixture in a
  Tour slot. Checked for every venue, applied or not (a blocker; a slot
  clash is a warning). The applied venue's not-ours is PAT-04's.
- **PAT-06** A universe with fixtures and no output (a note when the show
  patches no output at all: it only previews).

**Fixtures and definitions**

- **FIX-01** Missing fixture definition: the fixture stands in as a dimmer
  (blocker).
- **FIX-02** A fixture definition with errors (a head on a channel the mode
  doesn't have, overlapping capabilities, no model): it plays wrong or
  loses parts of itself. The editor's smaller warnings stay in the editor.
- **FIX-05** Not on the plot: a warning when a focus call needs to aim it.
  Only asked of a show that uses the plot.
- **FIX-07** A definition with no lumens or power.
- **FIX-08** A light marked dead at the applied venue that a cue, hit or
  scene still lights.
- **FIX-14** Two fixtures with one name.
- **FIX-15** A light buried in the stage: its body (the Physical Dimensions
  box the rig view draws) is inside a deck, riser or step with a solid top.
  Says *"UST-2 is inside Deck SR. Sit it on Deck SR (0.61 m)."* The same
  problem is an **error in the footer** (`fixtures.buried`: "UST-2 is inside
  Deck SR", with **Sit on Top** and **Show**), and the light is ringed red
  with a "!" on the plot. Sit on Top makes it stand on the deck (a deck
  mount, so it follows the deck when the deck moves or grows); a light that
  overhangs an edge with its middle off the deck is told to move clear
  instead. Not flagged: a light clamped to a truss, pipe, tower or hanger,
  or in a studio group (the structure places it); a light standing on or
  bolted to that deck; a deck with a Clear or Open top (lights go inside
  those on purpose); and a body that only touches or sinks less than 3 cm.
  It re-reads a moment after any edit.
- **FIX-18** A definition with no beam angle or no pan/tilt range: *"Test
  Spot: no beam angle and no pan/tilt range in its definition (3
  fixture(s)); the rig view and aims assume 15° / 540°×270°. Fix the
  definition so they use the real figures."* One note per definition used in
  the show, however many fixtures share it. The rig view and the aim solver
  carry on with a default by type (see
  [Lighting Studio](lighting-studio.md)) so the light still draws and aims;
  this note says the figure is a guess. A **note**; **Edit Definition…**
  opens the definition in the Fixture Editor (Physical tab). Quiet once the
  definition declares them.
- **FIX-16** A light on the floor whose row is on a deck: *"UST-2 is on the
  floor; UST-1, UST-3…UST-8 are on Deck USP1 (0.61 m). Sit it on Deck USP1
  (0.61 m)."* The row-mates are the other lights of the **same fixture type**
  in a fixture group it belongs to (smallest group first), else with the
  same name series ("UST-" for UST-2), else in the same line (Y within
  0.3 m). It fires when more than half of them, and at least two, stand on a
  deck (a deck mount, or a free light at a solid top's height) and this
  light is within a body length (0.5 m at least) of the edge of one of those
  decks. A **warning** in the footer (`fixtures.offrow`, **Sit on Top** /
  **Show**) and an amber ring with a triangle on the plot, since a floor
  light beside a riser can be deliberate: **Accept with a note** as for any
  check. Sit on Top is FIX-15's deck mount on the nearest deck the row uses.
  A light buried in a deck is FIX-15's, not this one's.

**Palettes and looks**

- **PAL-01** Two layers of a cue set the same attribute on one fixture,
  differently: the lower one wins, so the order decides the look.
- **PAL-02** A look that never shows (what Build's ⚠ badge says).
- **PAL-03** A palette its fixtures can't play: colour on lights with no
  colour, a gobo on lights with no wheel.
- **PAL-05** A cue, scene or palette pointing at a palette, group, fixture
  or stage target that was deleted, or a group now empty.

**Tracking and cues** (the Show Tree)

- **CUE-01** A value from one song still on in a later song. Songs are the
  song sections of the production's numbering scheme
  ([Reordering and numbering](show-tree.md#reordering-and-numbering)).
- **CUE-02** The first cue of a song tracks in from the song before: make it
  a Block cue.
- **CUE-03** A cue that looks exactly like the cue before it: GO changes
  nothing (a note). Cues with sub-cues and focus calls are left alone.
- **CUE-04** An empty cue (fine in a Block cue called a blackout).
- **CUE-05** An auto-follow with no fade and no hold: the cue is never seen.
- **CUE-06** Two cues with one number in a list.
- **CUE-07** Skipped cues left skipped, worst when it's the base cue or
  another cue includes it.
- **CUE-08** Released layers with no base cue (a note: they go dark), or a
  base cue that is deleted, skipped or comes after.
- **CUE-12** After Import in place, a chaser, collection or timeline that
  still plays the original chaser the Show Tree list was made from, or a
  Virtual Console button or slider that runs it (a Cue List widget is VC-09's).
- **CUE-11** An include that would loop, or an include, reusable or sub-cue
  list pointing at what was deleted.
- **CUE-13** A cue list nothing can play: it isn't the production's main
  list, no Cue Runner follows it, no VC Cue List or button runs it, and no
  timecode or control-map route reaches it.

- **CUE-15** A cue with sub-cues that wait for Sub GO (a note), when no Cue
  Runner shows a Sub GO button and no Cue Runner key or input sends it. The
  keyboard shortcut (Ctrl+Alt+Return) still works, but nothing on the
  console says so.

**Hits**

- **VC-01** Two hits in the same place that set the same attribute on the
  same lights: whichever was pressed last wins. Accept it when one button
  steps through them.
- **VC-02** A hit that overrides a wider one (a note): on a button holding
  hits from several places, the narrowest place that is live fires, so a
  hit in a cue wins over the same button's hit in the list or the
  production when both set the same things. It tells you which one fires.
- **VC-03** A hit no VC button, key or input fires.
- **VC-04** A button, matrix or playback slider with no function (or a
  deleted one), a level slider on fixtures that were deleted, a Cue List on
  a deleted list.
- **VC-05** The same key on two widgets that can be live together (not on
  different pages of one frame), or a key Operate already answers to (the
  GO keys, Space).
- **VC-06** Two widgets with different jobs on one input control.
- **VC-07** Widgets listening to an input universe the show has no input
  patched on (or no controller slot): a controller can never reach them.
  Counted per universe. From the command line, the file's input patches
  are what is read.
- **VC-08** A Cue Runner that follows a deleted cue list, or the current
  production in a show with no cue list.
- **VC-09** A VC Cue List still on the old chaser after Import in place: it
  plays a show that no longer matches the Show Tree list. Point it at the
  list.

The Virtual Console checks read what the console holds, not the console: in
the app that is the open console, and `--review` reads it from the file, so
both give the same answer.

**Venues and focus**

- **VEN-01** A focus call with no target, a stage target or scene that was
  deleted, or no lights left (its groups are gone): Venue focus has nothing
  to do for it.
- **VEN-02** Lights in a focus call that can't be aimed (no pan or tilt, and
  not a light Quick focus can place on the plot): they're left out when it
  runs. A light off the plot is FIX-05's.
- **VEN-05** A light the applied venue corrected as another fixture (a lamp
  swapped for a mover, a new mode): the old correction is left off until
  it's focused again.
- **VEN-09** The venue file beside the show (`Show.venues.xml`) can't be read,
  or is missing while the show has venues or house universes. The show
  opens without the venues' corrections. From the command line the venue
  file is read, never applied.
- **VEN-10** A scramble-practice venue still applied (a warning: the lights
  are off on purpose), or practice venues and no real one (a note).
- **BLK-01** A [blocking](stage-targets.md#blocking) puts a target where a
  light aimed at it reaches in Default but can't reach there.

**Effects, power and safety**

- **FX-02** An RGB matrix on a group with heads that have no RGB or CMY
  mixing: they get intensity at most.
- **FX-03** An EFX of size 0 or speed 0, or a matrix of speed 0: the lights
  just sit there.
- **FX-07** An effect with no script, or whose script isn't on this
  computer (blocker).
- **PWR-01** A cue, hit or scene that can draw more than a circuit's breaker
  (blocker) or over its derated limit (warning). Only once the power is set
  up. A colour set by a palette counts as white, the most it can draw.
- **PWR-03** Lights on no circuit, once the power is set up.
- **PWR-05** Light flashing faster than 3 times a second (the
  photosensitivity guidance): a warning for four lights or more at once, a
  note for fewer. Accept it with a note that the strobe warning is posted.
- **PWR-06** A stage light that ignores Blackout or the Grand Master.
- **PWR-09** A laser, or a star projector, whose beam or star field can
  reach the audience or eye height. The plot has no audience area, so the
  rule is a fixed unit whose beam, followed 30 m, enters the space downstage
  of the stage's front edge below 3 m. Aim it up or away, or accept it with a
  note saying who approved it (the venue's laser safety officer, the
  variance). A laser's beam is its **scan window**, H × V degrees (the
  definition's widest lens angle across, its narrowest up and down when that
  is declared and narrower, else a square; 40 degrees when it declares none),
  not a round cone. Lasers on pan and tilt aren't read here: their aim is by
  cue (PWR-11 reads it). Tape on the unit (**Masking**) is taken off its
  window first. When the unit has a **power class**, 1, 2 and 3R reaching the
  audience is a **Note** that names the class; 3B and 4 stay a Warning, and
  so does a laser with no class set.
- **PWR-10** A laser whose scan window is a **static pencil** (under 1 degree
  either way) or **tight** (under 10 × 10 degrees' worth), which puts all its
  power on a few spots. A note; a warning when a fixed laser's window also
  reaches the audience or a no-light zone. Wide, moving patterns spread the
  power. The pattern itself is the laser's own and isn't in the show, so the
  window is what is read.
- **PWR-11** A **no-light zone** on the plot (the orchestra pit, the
  conductor, a camera, the audience's eyes) that a lit beam's cone crosses in
  a cue. The finding names the cue, the lights and the zone, in the first cue
  the beam starts to cross it (not again in the cues that track it on). Each
  lit light's cone is followed 30 m from where it is in the rig: a mover from
  its position in the cue, a fixed light from where it throws, a laser as its
  window. A beam stops at the stage floor, so a pit's zone reaches up to it.
  A laser's window is read after any **Masking** on it, and a ray stopped by
  solid scenery (the cyc, a closed drape) does not reach a zone behind it:
  only past its edge or over its top. For a laser the finding says where the
  bottom of its window passes through the zone, and how many degrees of tape
  on the bottom clear it (**Apply This Cut** puts it in the unit's Masking).
  Aim it off, darken it in that cue, tape it, or move the zone. Zones are drawn and
  edited in the [Lighting Studio](lighting-studio.md#no-light-zones).
- **PWR-12** Light on the cyc nobody meant (*backwash*): a lit beam whose
  cone reaches a cyc or a scrim (or any scenery with **Light here only on
  purpose**) and isn't meant. Any part of the cone counts, followed from the
  plot as PWR-11 follows it, stopping at the floor and at anything solid; the
  finding names the cue, the fixtures and roughly how much of each beam lands
  there, once, in the cue the light starts landing. Meant is only what the
  show says: a **Cyc / strip light** or **Uplight wash** fixture, a look aimed
  at a target on the cyc, or **I Meant It**. On a PWR-12 row **Not a Problem…** reads
  **I Meant It** (no note needed: the mark is kept on the layer that lights it, with who and
  when, one undo step, and the row moves to **Meant**; right-click for Not a Problem…), **Fix** reads **Show Me** (the
  layer in the Show Tree), and on a meant row **Reopen** reads **Not Meant**.
  A closed curtain in front of the cyc (where that cue leaves it) takes the
  light. Every cue it lands in is marked **⚠ cyc** in the Show Tree (**cyc ✓** in green on the cue where it is meant). See [Light on
  the cyc only on purpose](lighting-studio.md#light-on-the-cyc-only-on-purpose).

**Files and names**

- **FILE-01** A sound or video outside the show's folder: it works on this
  computer only. File ▸ Collect for Transfer copies it in.
- **FILE-02** A sound or video that isn't there (blocker).
- **FILE-03** A definition or script the .qlcc carries that couldn't be read
  or was altered (a warning), or that differs from this computer's (a note).
  Definitions only the show has are normal and stay information.
- **NAME-01** A scene named off its folder's SS-PP.II-Description pattern.
- **NAME-02** Default and copy names ("New Scene 4", "… (Copy)").

**Teaching**

- **TEACH-01** Teaching mode is on (a note). Turn it off in File ▸ Show
  Settings before handing the show to a crew, or accept it with a note such
  as "class copy".

The footer's design problems are the same problems: the Show readiness
dialog shows their id ("PAT-01 · 1 fixture clashes with another's
address"), and a design entry no check reads yet (definitions out of date,
definitions only the show carries) is listed in the panel under its id.

## From the command line

`qlcconsole --review show.qlcc` reviews a show with no window: for CI, or a
teacher checking a folder of hand-ins. It never writes the show, loads no
plugins and sends nothing.

```
qlcconsole --review show.qlcc [--format text|json] [--min note|warning|blocker]
           [--check PAT-01,CUE-*] [--include-accepted]
```

It exits **0** when nothing is open at or above `--min` (default: warning),
**1** with open warnings, **2** with open blockers and **3** when the show
couldn't be read. A folder of hand-ins:

```
for f in handins/*.qlcc; do qlcconsole --review "$f"; done
```

On macOS the program is inside the app:
`/Applications/qlcconsole.app/Contents/MacOS/qlcconsole --review show.qlcc`.
