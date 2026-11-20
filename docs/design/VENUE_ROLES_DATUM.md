# Lining up a room and using its lights: the datum and house roles — design

2026-11-19. Board Q285 (c) and Q286 (a). Builds on [VENUES.md](VENUES.md)
(house rig, fills, not ours, the clash check) and
[VENUE_FROM_SHOW.md](VENUE_FROM_SHOW.md) (a venue made from the room's own
show file). For Dance Hysteria (DH) at the PMS Auditeria: the on-site visit
is early December, the first major show early February 2027.

Lengths in this note are in feet and inches. The app shows and reads every
length in **Settings ▸ Units** (`MeasurementUnits`, `LengthSpinBox`,
`formatLength()`), and new values are saved in whole millimetres.

Branson: *"just having the ability to select venue is nice .. but it needs
to be useful as well .. stage layout will be helpful .. but we also need
effective use of the lighting assets w/o having to do a full reconfig"*,
and *"we need to be able to say.. dim house lights .. or raise the stage
lights .. regardless of venue on a consistent manner"*.

Contents:

1. What is wrong today
2. The datum
3. Roles
4. PMS, worked through
5. Phases and ROI
6. Tests, help, course, menu count
7. Questions for Branson

---

## 1. What is wrong today

- **The room is not lined up with the tour.** New Venue from a Show places
  each house fixture at the world position it had in the other file
  (VENUE_FROM_SHOW.md §4). World 0,0 is the corner of each file's own stage
  grid (`MonitorProperties::stageCentre()`), so the two rigs only line up
  by luck. PMS's grid is 70 ft wide, DH's is 50 ft: PMS's FOH truss lands
  10 ft stage left and 6' 9" downstage of where it belongs relative to DH.
- **Cues can't reach the house rig.** Fills work by group (VENUES.md §4),
  but name matching found 0 of PMS's 16 groups in DH, so all 16 wait as
  room groups and every house light needs hand work.

## 2. The datum

### 2.1 The model

Q285 (c): **the centre line (CL) gives left and right; the upstage drape
gives up and downstage.** The tour rig is measured from those two lines.

- **The show's frame is the tour's frame.** The tour rig, its trusses, its
  stage targets and its aims never move when a venue is applied. Each room
  is placed under them: the room's centre line on the show's, the room's
  drape line on the show's upstage line.
- **Why the room moves and not the tour.** Moving the tour would rewrite
  every tour position on every venue switch, mark the show modified, and
  drift with rounding. Moving the room is one offset per venue, applied
  where the house rig is put in (`VenueRig` apply) and taken back out
  when a house fixture is moved on the plot. The show file stays
  byte-identical.
- **What "the tour follows" means here.** Targets and aims are defined in
  the show's frame, so they keep their place relative to the drape and the
  centre line in every room, by construction. Aim palettes on house movers
  re-solve against the same targets, because house and tour now share one
  frame.

| What | In a new room |
|---|---|
| Tour fixtures, trusses, platforms, scenery | Unchanged (show frame) |
| Stage targets, blockings | Unchanged; a venue's own target moves (Q128) still apply on top |
| Aim palettes | Re-solve as always; nothing to do |
| Raw pan/tilt palettes on tour heads | Unchanged, and still land: the rig and the marks keep their geometry |
| Truss trims (Q128) | As today, per venue |
| House fixtures from a room file | Moved by the venue's offset (2.3) |
| House fixtures added by hand at the venue | Placed in the show frame already; offset 0 |

### 2.2 Where it is stored

**Show** (`<Monitor>`):

- **Centre line**: the middle of the stage grid, as today. The plot's
  rulers, the number line and `stageOffset()` already count from it, and
  the centre line can't be dragged (lighting-studio.md). No change.
- **Upstage line**, new: a stage piece beside the curtain line, built the
  same way (`curtainLineY()` / `autoCurtainLineY()` / `setCurtainLineY()`
  have the pattern). `upstageLineY()`, `autoUpstageLineY()`,
  `setUpstageLineY()`, `upstageLineIsAuto()`; a fifth `StagePiece`,
  `UpstagePiece`, with its row in Plot layers, its eye and lock, a drag on
  the plot and **Properties…** ("Auto", or how far upstage of the curtain
  line, in a `LengthSpinBox`). **Auto**, in order:
  1. the most upstage cloth across the centre line: a **Cyclorama**, a
     **Traveler**, a **Scrim** (`Scenery::Kind`);
  2. else a **Back wall** across the centre line (its downstage face);
  3. else the upstage edge of the main deck (`mainDeck()`);
  4. else the upstage edge of the stage grid (Y 0).
  Saved as `UpstageLine="1237"` (mm) only when set.

**Venue**: a new `<Datum>` element inside `<Venue>`, beside `<Room>`. Not
attributes on `<Room>`: a build that knows `<Room>` drops attributes it
doesn't know when it saves, but keeps an unknown element inside `<Venue>`
whole (VENUES.md §7).

```xml
<Venue Name="PMS Auditeria" …>
 …
 <Room Universe="4" From="PMS Stage.qlcc">…</Room>
 <Datum CentreX="10668" Drape="3284">
  <Stage Curtain="5708" Opening="10973" OpeningHeight="5499"
         WingSR="2150" WingSL="2010" Trim="6002"/>     <!-- phase 3 -->
 </Datum>
</Venue>
```

- `CentreX`, `Drape` (mm): where the room's centre line and drape are in
  the room file's own coordinates, the ones its house rig's plot positions
  are saved in. No `<Datum>`: the venue is not lined up (2.5).
- `<Stage>` (phase 3): the room's measurements, from the drape and the
  centre line: the curtain line (how far downstage of the drape), the
  proscenium opening, the wings, the electrics' trim.
- A venue with no `<Room>` (made by hand, house fixtures patched and placed
  on site) needs no datum: what is placed at the venue is placed in the
  show's frame.

### 2.3 The transform

One translation per venue, worked out when it is applied:

```
offset = ( show.centreX  - room.CentreX ,
           show.upstageY - room.Drape   ,
           0 )
show position = room position + offset
```

- Applied to each house fixture's plot position as the house rig is put
  in (free-placed X/Y are mm, `fixtureRigPosition()`), and to the room's
  scenery when phase 3 brings it in.
- **Heights are not moved**: Z is above the stage floor in both files.
- **Aim directions and calibration are not moved**: a translation does not
  turn anything.
- **Moving a house fixture on the plot** with the venue applied saves
  `position - offset`, so the venue file stays in the room's coordinates
  and lining up again never piles one offset on another.
- **The show's upstage line moved** (Branson finds DH's drape hangs a foot
  further upstage than drawn): every venue's offset follows at once, the
  house rigs move with the line, the tour stays where it is.
- No rotation and no mirror. The PMS file is square to its centre line and
  stage left is +X (its SL wall is at X 18.3 m, SR at 3.1 m), the
  convention `scenery.h` gives. A rotated or mirrored room file is handled
  when one is met.

### 2.4 Entering it: read first, one click

**From the room's file** (New Venue from a Show, and **Line Up** for a
venue made before this):

- **Centre line**: the room file's own centre line (the middle of its
  grid), which is what its rulers counted from when it was drawn. When its
  proscenium or main drape is centred more than 3" off it, the venue page
  says so, with **Use the Proscenium's Centre** (PMS: the proscenium is
  4" stage left of the file's centre line).
- **Drape**: the room file's automatic upstage line (the same rule as the
  show's, 2.2), shown as a choice of each cloth across its centre line,
  upstage first: "Cyclorama 2 (18' 9" upstage of the main drape)". Beside
  it, **Downstage of it by** (`LengthSpinBox`, 0 by default) for when the
  tour's drape hangs on a pipe in front of the room's cyc.
- New Venue from a Show takes both without asking (the dialog's **What
  comes in** says "Lined up: the centre line, and the drape at Cyclorama 2").
  The venue page's new **Room** box shows them with **Line Up the Room**
  for a change.

**On the plot** (phase 3, a file with no scenery): venue page ▸ Room ▸
**Line Up on the Plot**: click a point on the room's centre line (a house
light known to hang on centre, a DSC spike), then a point on its drape
line; the house rig jumps under the show's lines. Each click snaps to a
house fixture or a room piece. Two clicks, no typing; **Downstage of it
by** stays for the odd case.

**On site** the check is to look: with the venue applied, the plot draws
the room's lights against the tour's (the Owner overlay already colours
them), and **Flash** on a fixture shows it is the light it says it is.

### 2.5 Old shows and old venues

- A show with no `UpstageLine`: Auto (2.2). Nothing is written, nothing
  moves, the file stays byte-identical.
- A venue with no `<Datum>` applies exactly as today (offset 0).
  When it has a `From` file, the venue page's Room box says **Not lined up
  with this show** and offers **Line Up the Room**, reading the room file
  again if it is still where it was (else **Browse…**). That is the path
  for a PMS venue made before this.
- An older build reading a venue with a `<Datum>`: kept whole and written
  back; the house rig applies without the offset, as it does today.

## 3. Roles

### 3.1 The model: a role is a tag on one group

Q286 (a): a fixed set of roles on every show. They are **ordinary fixture
groups with a role tag**. Nothing new plays them:

- **Fills already do the work.** A venue fills a show group with its house
  heads, and a cue on a group plays on whoever is in it now (VENUES.md §4,
  `aGroupCuePlaysOnEachVenuesHouseRig`). A role group filled by PMS plays on
  PMS's lights, filled by the next room on that room's.
- **One group per role**, `FixtureGroup::role()` (new
  `engine/src/houserole.{h,cpp}`, like `rigowner.h`), saved as
  `<Role>Front</Role>` in `<FixtureGroup>` only when set.
- **Made on first use**, in a **Roles** folder (`FixtureGroup::path()`):
  the first **Fill Roles** on the venue page (3.5) or New Venue from a
  Show makes all eight and the standard cues. A show that never uses roles
  stays byte-identical.
- **Any group can take a role.** When the show already has a group by that
  name ("Cyc"), Fill Roles asks once: **Use your group "Cyc"** or **Make
  "Cyc (role)"**. His own groups are never renamed or changed otherwise.
- **Owner stays Not set.** A role group can hold tour heads too (DH's own
  front light in Stage Wash Front, if he wants Stage up to use it); setting the
  group's owner to House would move those into the venue (VENUES.md §2),
  so a role group refuses an owner.
- **Renaming** keeps the role. **Deleting** says which standard cues use it;
  the next Fill Roles makes it again.
- `PROGRAMMING_ROLE_DESIGN.md` is about the Build tab's job, not lighting
  roles; the closest existing idea is `EosCircuit::Role` (Relay, House,
  Work, Backstage), which the suggestions reuse (3.4).

### 3.2 The set

The proposal, refined by PMS's rig and by practice:

| Role (group name) | What it is | Change |
|---|---|---|
| **House Lights** | the audience's light | as proposed |
| **Aisle** | aisle and step lights | kept separate: House out leaves them on, for getting people in and out in the dark |
| **Stage Wash Front** | light on the stage from the house side: FOH, box booms, balcony rail | as proposed |
| **Stage Wash Top** | down light from the overhead electrics | as proposed |
| **Side** | booms, ladders, shins in the wings | as proposed |
| **Back** | from upstage, aimed toward the house | as proposed |
| **Cyc** | lights on the cyc or backdrop | **new**: PMS's 3rd Electric is five ColorSource CYCs; a cyc is lit for colour, not as stage wash |
| **Work** | the crew's light | as proposed |

The names are the ones board Q288 approved (2026-11-19).

Not roles: **power relays** (PMS's "Relay FOH" and the rest: Switch + On
when not in use already keep them on), **backstage running lights and
blues** (Blackout and the GM already leave them alone), moving lights.

### 3.3 The standard cues

Made with the roles, as **reusable cues** (Show Tree P4,
`ShowTree::ReusableCue`): one definition, any number of uses, an edit to
one is an edit to all.

| Cue | Layers (Dimmer + Open White, 3.4) | Fade |
|---|---|---|
| **House to half** | House Lights 50% | 5 s |
| **House out** | House Lights out (Aisle not in the cue: it stays) | 5 s |
| **House full** | House Lights full, Aisle full | 5 s |
| **Stage up** | Stage Wash Front, Stage Wash Top, Side, Back full | 3 s |
| **Stage out** | Stage Wash Front, Stage Wash Top, Side, Back and Cyc out; house and work untouched | 3 s |
| **Work lights** | Work full | 0 s |

Board Q288 approved these six (2026-11-19). House full brings the house
back for intermission and bows; letting the Work lights hit go puts them
out.

- **In a cue list**: dragged in from the Library as any reusable cue
  (`insertReusableCue()`), numbered by the list's scheme; the preshow list
  becomes "0.0.2 House to half, 0.0.3 House out". They track as any cue.
- **Fired any time**: six **Global hits** (`HitScope::Global`) in a
  **House** folder, each a use of its reusable cue, **Override** on
  (`Cue::override`: a hit that only mixed would leave a full house at full
  under HTP). **Make VC Page** on the folder (P-D7) gives six buttons; the
  pad and the Run tab fire them as any hit. If a Hits list does not take a
  reusable use today, phase 2 makes it (a hit is a cue of its production's
  Hits list).
- **Levels and fades are his to change**; the cues stay the standard ones
  by name. Fill Roles adds back any he deleted, never overwrites one he
  edited.
- **The palettes**: Dimmer full / 50% / out and a Colour "Open White"
  (R = G = B = full, white channel full), filed in a **Roles** folder;
  an existing palette with exactly those values is reused (as Capture does,
  P-D9).

### 3.4 How a role cue plays

**Intensity, and white where white is the only way to make light.** Each
role layer has two looks: Dimmer at the level, and Colour Open White.

- PMS's ColorSources are patched in 3-channel RGB: no intensity channel. A
  Dimmer palette alone writes nothing on such a head
  (`QLCPalette` Dimmer, qlcpalette.cpp: master and head intensity channels
  only), so "Stage up" would leave them dark. With Open White in the same
  look, `dimScaleWithoutDimmer()` scales the white by the look's dimmer:
  Stage up is 255/255/255, House to half on an RGB house light 128/128/128.
- On a head with a dimmer and colour mixing, Open White is set too: house,
  work and "stage up" light is white light. (Keeping the show's colour is
  question 1.)
- **Relays** (`Fixture::Switch`: on at 50% and over): House to half is
  **on**; the role table says "Ch 15: relay, half is on". A 1-channel house
  circuit named "Relay…" without the Switch rule gets a **Treat as Relay**
  tick in the role table (it sets the rule on that house fixture, in the
  venue).
- **Positions are never touched**: a mover in a role keeps where it
  points.
- **Blackout and the GM**: house, work and aisle circuits from an Eos show
  already ignore both (`EosCircuit::rulesText`); stage roles follow them
  as any light.

**A role the room lacks is empty, and the cue skips it.** A layer on an
empty group sends nothing. It says so where the operator looks: the
layer's row in the tree and the hit's button read **none in PMS
Auditeria**, and Show Review does not report an empty role layer as a
mistake (it does report a standard cue whose every role is empty).

**The house desk.** When the room keeps its house lights (Not ours,
VENUES.md §5):

- Fill Roles never suggests a fixture on Not ours channels.
- House Lights is then empty, so the cue skips it, and the cue's note says
  **Call the house desk: house to half** (Run tab and the tree show it at
  standby). Nothing of ours can reach the desk's channels anyway
  (`Universe::dumpOutput()` zeroes them).
- At PMS the house lights are relays on the room's own DMX line, which DH
  drives whole while the venue is applied: no desk, the cue drives them.

### 3.5 Suggesting fills, and the one click

`HouseRole::suggest(doc, venue)` gives each house fixture a role, or none,
with a reason. The first rule that applies wins:

1. **Words**, in the fixture's name, else its room groups' names, by
   `EosCircuit::classify()` and a few stage words: House, Aisle, Work →
   those roles; Hall, Blue, Backstage, a relay with no other word, Spare →
   **none**; FOH, Front, Box boom → Front; Top, Down → Top; Side, Boom,
   Shin, Ladder → Side; Back → Back; Cyc → Cyc.
2. **Moving lights** (pan and tilt) → **none**, "a moving light: add it by
   hand".
3. **Cyc**: the definition's model says CYC, or its beam lands on a
   Cyclorama or backdrop (line of sight, `BeamOcclusion`) → Cyc.
4. **Where it hangs**, in the show's frame once the room is lined up,
   against the room's curtain line (its main drape or proscenium) and
   opening:
   - downstage of the curtain line by more than 3 ft → **Front**;
   - over the stage (between the drape and the curtain line, inside the
     opening, above 10 ft): aimed within 30° of straight down → **Top**;
     tipped toward the house more than 30° → **Back**; tipped upstage more
     than 30° → **Front**; no aim → **Top**;
   - outside the opening, below 10 ft, or aimed mostly across → **Side**.
5. Otherwise **none**.

Never suggested: a fixture on Not ours channels, an unpatched one (a clash),
one already in a role by hand.

**The confirm UI**: the venue page gets a **Roles** box (and New Venue
from a Show shows the same table as its last step):

```
Role           Lights from PMS Auditeria           Why                       Use
House Lights   2  Ch 15, Ch 24 (relays: half=on)   "House" in its name       [x]  Flash
Stage Wash Front 12 Ch 51–62                       FOH, 14' 1" DS of curtain [x]  Flash
Stage Wash Top 25 Ch 101–113, 201–212              over the stage, aimed down[x]  Flash
Side           none in this room                                                  
…
                              [ Fill Roles ]   Edit…   (3 not used: Ch 114, …)
```

- **Fill Roles** is the one click: every ticked row becomes that role
  group's fill in this venue (the existing `Fill`, `<House><Fill>`), and
  the roles and standard cues are made the first time.
- **Flash** lights that row's fixtures in the room (the Stage bar's Flash),
  which is how a suggestion is confirmed on site: by looking.
- **Edit…** opens the role group in Fixture Manager with the venue applied
  (editing a fill is editing the group, VENUES.md §4).
- **Not used** lists the house fixtures in no role, each with its reason.
- Every count, every name and every distance in the table goes through
  `MeasurementUnits`.

## 4. PMS, worked through

PMS Stage.qlcc as it is: 69 fixtures, 16 groups, 4 electrics at 19' 8",
a proscenium 36' wide, a main drape, a cyclorama, a back wall, no decks.

**The datum.** The file's centre line is the middle of its 70 ft grid;
its proscenium is 4" stage left of that (said, not used). Its drape is
**Cyclorama 2**, 18' 9" upstage of the main drape. DH's centre line is the
middle of its 50 ft grid and its upstage line is its **Cyclorama 1**. The
offset is 10' 0" toward stage right and 6' 9" upstage; every PMS light
moves by it, nothing of DH's moves.

**The roles** (fixture names and models from the file):

| PMS group | Lights | Role suggested | Rule |
|---|---|---|---|
| FOH (DSR, DSC, DSL are parts of it) | Ch 51–62, ColorSource Spot | **Stage Wash Front** | word FOH; 14' 1" DS of the curtain line |
| 1st Elect, Top Lights | Ch 101–113: Spot jr and PAR, 1' 6" US of the curtain line | **Stage Wash Top** ("Top Lights" by word; the Spot jrs by place, unless their aims tip them) | word / place |
| 2nd Elect | Ch 201–212: Spot jr and PAR, 10' 3" DS of the drape | **Stage Wash Top** | place |
| 3rd Elect | Ch 301–305, ColorSource CYC, 3' 9" DS of the cyc | **Cyc** | model |
| (no group) | Ch 114, Focus Spot Three Z | none: moving light | |
| House | Ch 15 Linear, Ch 24 Down (relays) | **House Lights** | word |
| House & Work | Ch 15, 24; Ch 17 Stage Work Lights | **House Lights**; Ch 17 **Work** | word, per fixture |
| Relays, Power relays | Ch 1–14, 16, 18–21, 23 | none: power and spares | word Relay / Spare |
| Backstage, Hallway, Blue Dome | Ch 22, 25, 26 | none: running lights | word Hall / Blue |
| Group 100 | 47 of the above | none of its own | (each fixture by its own row) |

Side, Back and Aisle are empty at PMS: Stage up plays
Front and Top, and its row says **Side, Back: none in PMS Auditeria**.
PMS's DSR / DSC / DSL stay as room groups for phase 3's area fills (DH has
area groups of its own: DS1f, DS1t …).

## 5. Phases and ROI

ROI as STRATEGY §3: Value × Fit ÷ Effort (days) × risk.

| Phase | What | When | ROI |
|---|---|---|---|
| **1. Datum** | Upstage line stage piece (auto, drag, Properties); `<Datum>`; the offset on apply and on a house drag; New Venue from a Show lines up by itself; the venue page's Room box with **Line Up the Room** (re-lines a venue made before this); round trips | Nov 20–23 | 5 × 5 ÷ 2.5 × 0.8 = **8** |
| **2. Roles** | Role tag, the eight groups on first use; suggestions (words, Cyc, place); the Roles box and the table in New Venue from a Show; Fill Roles, Flash, Edit; six reusable cues and Global hits, Dimmer + Open White; "none in …" and the house-desk call on screen; Show Review rule | Nov 23–27, in the RC for the visit | 5 × 5 ÷ 4 × 0.8 = **5** |
| **3. Room layout** | `<Stage>` room measurements (read from the file, else typed on site); while applied they set the curtain line, apron and number line; the room's scenery on a locked **Room** plot layer; a **Fit** line in the clash check (a tour tower outside the room's opening or walls); a venue's truss position up/downstage (beside trims, bound targets follow); Line Up on the Plot (two clicks); area fills (PMS DSR/DSC/DSL to DH's area groups) | after the visit, ready mid-January for February | 4 × 5 ÷ 6 × 0.8 = **2.7** |

**Before the PMS visit (~Dec 1): phases 1 and 2**, in the RC of Nov 27,
so he can line PMS up, fill the roles with Flash in the room, and run
House to half / Stage up on the real rig. Phase 3 is what the visit
measures for: the room's real numbers, typed or confirmed on site, feed
it.

## 6. Tests, help, course, menu count

**Tests that fail on today's code** (engine/test/venuerig unless said):

- `importedRoomLinesUpOnTheDrape`: a PMS-like room file (70 ft grid, cyc
  at a known Y) into a 50 ft show: a house light lands at the show's
  centre line and upstage line plus its distances in the room. Today it
  lands at its raw position.
- `lineUpMovesHouseNotTour`: Line Up the Room moves every house fixture by
  the offset; every tour fixture, truss and target is unchanged.
- `houseDragSavesRoomCoordinates`: a house light moved with the venue
  applied is saved minus the offset; apply again lands it where dropped.
- `upstageLineFollowsMovedShowLine`: moving the show's upstage line moves
  the house rig, not the tour.
- `oldVenueAppliesWithoutOffset` and `showWithoutDatumByteIdentical` (pass
  today; kept as guards).
- monitorproperties: `upstageLineAuto` (cyc, then back wall, then deck,
  then grid), `upstageLineRoundTrip` (mm, only when set).
- houserole (new): `suggestPmsLike` (the table of section 4 on a trimmed
  PMS-like file), `notOursNeverSuggested`, `movingLightNotSuggested`,
  `roleTagRoundTrip`, `rolesMadeOnFirstUseOnly`, `roleGroupRefusesOwner`.
- showtree: `stageUpLightsRgbOnlyHead` (a 3-channel RGB fill reads
  255/255/255; today a Dimmer look on it writes nothing),
  `houseToHalfOnRelayIsOn`, `emptyRoleLayerSendsNothingAndSaysSo`,
  `roleCueFollowsVenueSwitch` (two venues, one cue, each room's lights),
  `standardCuesNotOverwritten`.
- ui/test/venuerigui: the Room box (detected values, Line Up), the Roles box
  (rows, Fill Roles, the "none in" text, Flash enabled), shots at 1024×640,
  1440×900, 1920×1080; layoutsanity_test; `units_test` sees the new
  `LengthSpinBox` fields.

**Help**: docs/features/venues.md gains **Lining up the room** and
**Roles: house lights and stage light in any room**, and the on-site
checklist gains numbered steps (Line Up, Flash each role row, House to
half, Stage up, Work lights, each with what should happen). lighting-studio.md:
the upstage line piece. show-tree.md: the standard cues and their hits.
A CHANGELOG line per phase.

**Course**: Level 3 lesson 6 (stage marks and the classroom venue) adds the
upstage line beside the curtain line, and one paragraph on roles: "House to
half works in any room because it is written on House Lights, not on the
room's lights".

**Menu count: no change.** New surface: the Upstage line row in Plot
layers (a stage piece, like the curtain line), the Room and Roles boxes on
the venue page, a Roles group folder, a House folder of hits.

## 7. Questions for Branson

**Q-1. Stage up: white, or the show's colour?**
(a) Dimmer + Open White on every role light; (b) dimmer only, keeping the
colour the show has (RGB-only lights would need a new "scale what's
playing" rule, and are dark when nothing set a colour).
**Recommendation: (a).** Stage up, house and work light are white light,
and it is the only way PMS's ColorSources light at all.

**Q-2. The set: eight roles and six cues?** Cyc added, Aisle kept apart
from House, House full and Stage out added (Q288).
**Recommendation: yes**, as section 3.2 and 3.3.

**Q-3. Do DH's own lights belong in roles?** Stage up in a room with no
front light does nothing unless DH's front light is in Stage Wash Front.
(a) Roles start with house lights only; he adds tour lights by hand.
(b) Fill Roles suggests tour lights too, by the same rules.
**Recommendation: (a)** for December (DH's rig is a dance rig, not a
wash); (b) is a small add in phase 3 if he finds himself doing it.

**Q-4. Where does DH's drape hang at PMS?** The default lines PMS's cyc up
with DH's upstage line. If DH hangs its own drape on a pipe in front of the
cyc, the distance goes in **Downstage of it by**.
**Recommendation:** start with the cyc and measure on site.
