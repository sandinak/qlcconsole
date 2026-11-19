# New Venue from a Show — design

2026-11-18. Builds on [VENUES.md](VENUES.md) (house rig, fills, the venue
patch, not ours, the clash check). Roadmap item as settled 2026-11-15:
Dance Hysteria (DH) plays PMS, whose house rig is already a show file,
`PMS Stage.qlcc` (made from the Eos export). Making the PMS venue from that
file is one dialog, not an evening of patching. Wanted for Branson's on-site
test at PMS in early December; the first major show there is early
February 2027.

Contents:

1. The flow
2. The data model: the venue's existing structures, and one new element
3. Reading the other show
4. Fixtures: House, at their plot places, re-addressed onto the venue's universe
5. Groups: matched by name, each match confirmed
6. The room's output: a USB DMX interface or an Art-Net node
7. The clash check: what is new
8. Undo
9. Surfaces, help, tests
10. Out of scope
11. Questions for Branson

---

## 1. The flow

1. Footer venue chip (or View ▸ Venue…) ▸ **New Venue…**: the same name
   dialog as now, with one more button, **From a Show…**. No new menu item:
   the menu stays as it is (STRATEGY: the menu-item count stays flat).
2. **From a Show…** opens the **New Venue from a Show** dialog, the typed
   name carried over:
   - **Name**: the room (PMS Auditeria).
   - **Show file**: the .qlcc to take the room's rig from (Browse…).
     Reading it fills the rest; nothing is changed yet.
   - **Its fixtures go on**: one universe of this show, the one DH sets
     aside for the room. Suggested: the first House slot with no tour
     fixture on it, else the first universe with no fixture at all.
   - **Output in this room**: a USB DMX interface, an Art-Net node, or
     not yet (section 6).
   - **Groups**: the other show's groups, each with the group of this show
     it fills (section 5).
   - **What comes in**: the count, the addresses ("69 fixtures on U4
     1–315"), and the clash check of the venue as it would be, before
     anything is applied.
3. **Make Venue** makes the venue, writes the sidecar and applies it. The
   show file is not changed; the show is not marked modified.
4. A closing note says what came in, with **Undo** (section 8).

The venue page (**Rig and Universes…**) then shows the room's universe and
lets the output be chosen again per room (section 6), and lists the room's
groups still waiting for a match.

## 2. The data model

Everything goes into what a venue already owns (`VenueRig::Data`,
VENUES.md §7), so applying, switching, None, the reserved ids, the clash
check and the sidecar all work as they do today:

| What | Where |
|---|---|
| The fixtures, as House | `Data::fixtures` (`<House><Fixture>`), each carrying `<Owner>House</Owner>` so following owners never moves it into the show |
| Their plot places | `Data::monitorXml` (`<House><Monitor>`) |
| The matched groups | `Data::fills` (`<House><Fill>`) |
| The room's output | `Data::patch` for the room's universe only (`<Patch>`); every other universe keeps the show's |

New, in one element the older builds already keep whole (VENUES.md §7,
"unknown elements are kept"):

```xml
<Room Universe="4" From="PMS Stage.qlcc">          <!-- 1-based, as on screen -->
 <Imported><Output Plugin="ArtNet" UID="172.18.2.219" Line="1" Network="172.18.2.0/24">
  <PluginParameters outputIP="172.18.2.10"/></Output></Imported>
 <Group Name="FOH"><Head Fixture="1012" Head="0" X="0" Y="0"/>…</Group>
</Room>
```

- `Universe`: the venue's own universe (the one DH sets aside).
- `From`: the file it was made from, for the venue page.
- `<Imported>`: the outputs the other show had for its fixtures' universe,
  kept so the clash check can tell a leftover (section 7).
- `<Group>`: the other show's groups not matched to one of this show's
  (held, by their own name, for a later match; section 5).

## 3. Reading the other show

`VenueImport::read()` (engine/src/venueimport.{h,cpp}) streams the file
once, read-only, and never loads it as a show:

- each `<Fixture>` of `<Engine>`, whole (as the sidecar keeps one);
- each `<FixtureGroup>`: its name and its head cells;
- the `<Monitor>`: loaded into a scratch `MonitorProperties` so each
  fixture's world position can be worked out from its truss;
- each `<Universe>`'s `<Output>`s.

A fixture whose definition this computer lacks is kept in the venue as it
is (as any house fixture is, VENUES.md §3) and named in the warnings.

## 4. Fixtures

- **Ids**: new ones, after every show fixture and every venue's house ids,
  so none is ever renumbered on apply.
- **House**: `<Owner>House</Owner>` written into each.
- **Universe**: every fixture goes on the venue's universe; **its address
  stays as it was** ("re-addressed in place"). PMS Stage.qlcc is one
  universe, so nothing can collide. A file with fixtures in more than one
  universe is still taken, and the overlaps that makes are listed by the
  clash check (a house fixture that clashes goes in unpatched, as always);
  the warning says so.
- **Plot**: each fixture at the world position it had in the other show.
  The other show's trusses, pipes, towers and risers are not brought in (a
  house fixture hangs on the show's structure, VENUES.md §12), so a
  fixture hung on one is placed free where it hung, with its aim, lens and
  calibration kept. Its 2D layer and plot group are the other show's and
  are cleared (the Default layer). Branson's word (2026-11-15): the PMS
  plot "should be reasonably close", so it is taken as it is and drawn over
  DH's own; trimming is for the December visit.

## 5. Groups

Each group of the other show is offered against this show's groups:

| Suggestion | When |
|---|---|
| **Same name** | the names are equal, ignoring case and spaces at the ends |
| **Similar name** | equal once case, spaces, punctuation and "&"/"and" are ignored ("1st Elect" = "1st elect.") |
| **No match** | neither |

**Every suggestion waits for his tick**: a row with a group chosen and not
confirmed holds **Make Venue** back ("Confirm Each Match: 2 Left").
Choosing a group by hand in a row counts as confirming it.

- A confirmed match becomes a **fill** of that group: the house heads in
  it, at their cells from the other show where those are free (the next
  free cell where the show has a head there, as any fill).
- **No match** (or a suggestion left at "Not matched") is held in the
  venue under its own name, as a room group (`<Room><Group>`), and the
  venue page lists it. It fills nothing, so a cue never lands on house
  lights nobody confirmed, and it is not lost: **Match Groups…** on the
  venue page offers the same table again once the show has a group for it.
- The show's groups are never created, renamed or changed in the show file.

With DH 0.6 as it is, no PMS group has a DH group of the same name (DH's
groups are its own rig's: StepTops, FART-SR…), so all 16 come in as room
groups; see question 1.

## 6. The room's output

The venue is **one universe**, routed per room (decided 2026-11-15):

- **USB DMX interface**: the DMX USB plugin's lines present now, by name
  (the patch follows the interface by name, and waits for it when it is
  unplugged). PMS's house DMX is 5-pin at the booth and in the SL wings.
- **Art-Net node**: an IP address and an Art-Net universe; this computer's
  interface that reaches it is found when it is applied (as a show's
  network patch is).
- **Not yet**: the universe has no output in this room at all, so nothing
  of the room's goes out the show's own output for that universe. The
  clash check says so until one is chosen. This is the default when no USB
  DMX interface is plugged in.
- **As the file had it**: offered only when the other show had an output,
  never the default; the clash check flags it (section 7).

It is written as the venue's patch for that universe only. DH's own rig
runs on its own Art-Net nodes, untouched (no Show-only conflict, Q247:
Show-only fixtures stay on network outputs, and none of them is on the
venue's universe). Teaching mode holds USB output as it holds any.

On the venue page the room's universe has an **Output in <venue>** row with
the same choice, applied at once when the venue is applied.

## 7. The clash check

Two new lines, only for a venue with a room universe (every venue made
before has none, so nothing changes for them):

| Kind | What |
|---|---|
| **Output** | The room's universe still sends what the imported show had ("U4 still sends Art-Net to 172.18.2.10 as PMS Stage.qlcc had it: choose the room's output"); or it uses the show's own output for that universe in this room; or it has no output yet. |
| **Slot** | A tour or rental fixture on the venue's universe ("U4 12: Spot 1 (tour) is on PMS Auditeria's universe"). |

Like every clash: listed in the New Venue from a Show dialog before it is
made, on the venue page, behind **Clashes (n)…**, and the chip turns amber.

## 8. Undo

The import is one step: one venue added, then applied. **Undo** in the
closing note (and later, deleting the venue in the venue manager) takes it
off the rig and removes it: the sidecar is as it was apart from that
venue, the show was never changed. The show's undo history is not involved
(venues are not on it; nothing in the show changed).

## 9. Surfaces, help, tests

- `ui/src/monitor/venuefromshowdialog.{h,cpp}`: the dialog;
  `ui/src/monitor/roomoutputpicker.{h,cpp}`: the output choice (dialog and
  venue page); venuemenu.cpp: the **From a Show…** button.
- Help: docs/features/venues.md, "A venue from another show", with an
  on-site checklist (numbered steps, the expected result of each).
- Tests: engine/test/venuerig (the import: house, ids, universe and
  addresses, plot, show file byte-identical, groups, the output and its
  clashes, sidecar round trip, undo, an end-to-end on a trimmed PMS-like
  show) and ui/test/venuerigui (the dialog, the confirm rule, the venue
  page's output row, shots at three sizes).

## 10. Out of scope

- **Target positions from the Ref Point PDFs.** The ten "Ref Point" PDFs
  beside PMS Stage.qlcc are printable Augment3d calibration markers (a
  picture of a fixture in a ringed square, numbered 1–10) with no
  coordinates in them. Their positions are measured in the room: place the
  markers, then record each as a venue target (Venue focus). A follow-up
  for the December visit, not this build.
- The other show's structure (trusses, scenery), its cues and functions,
  its universes' names, and its not-ours ranges.
- Re-addressing to other addresses (packing), or splitting across
  universes: the venue is one universe.
- Creating DH groups from PMS's (question 1).
- Importing an Eos export (.asc) directly: PMS Stage.qlcc already came
  from one.

## 11. Questions for Branson

**Q-A. PMS's groups don't exist in DH. What should the venue do with
them?** None of PMS's 16 groups (FOH, 1st Elect, Top Lights…) has a DH group
of the same name.
- (a) Hold them in the venue as room groups; he makes the DH groups he
  wants to program against (e.g. "House FOH") when he wants them, then
  **Match Groups…** on the venue page.
- (b) Offer "New group in the show" per row: the DH show gains empty groups
  (named after PMS's), the venue fills them. Changes the show file.
- (c) Make venue-only groups that exist only while PMS is applied.
  A new model piece (a group the show doesn't have).
- **Recommendation: (a)**, built. It keeps "the show file is untouched",
  needs no new model, and DH's cues on house lights are best written
  against a few generic groups he chooses (VENUES.md §4), not PMS's 16.

**Q-B. Should choosing the venue's universe mark it as a House slot in the
show?** Slots are the show's (VENUES.md §5).
- (a) No: the venue records its own universe; the clash check flags a tour
  fixture on it anyway (section 7).
- (b) Yes, a tick box, off by default: one change to the show file.
- **Recommendation: (a)**, built. Same protection without touching the
  show.

**Q-C. Default output when no USB DMX interface is plugged in at import
(the office).**
- (a) **Not yet**: no output for that universe in the venue until he
  chooses one at PMS; the chip is amber until then.
- (b) "Any USB DMX interface": the first one found.
- **Recommendation: (a)**, built. Nothing of the room's can reach the
  office rig, and the amber chip is the to-do for the visit. (b) would
  guess wrong when two interfaces are plugged in.
