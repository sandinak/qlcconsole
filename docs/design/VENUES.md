# House rig, tour package and universes per venue — design (board Q137–Q138)

2026-10-24. Builds on the venue sidecar of FOCUS_REFINE_DESIGN.md §5 (a
venue's focus corrections, trims and targets beside the show in
`<show>.venues.xml`). This page adds what a venue *owns*: its house
fixtures, how they fill the show's groups, the network patch of the room,
and the channels that are not ours.

Branson's words (Q137): *"this way I can define venue/house and as I move
the touring show between the locations I can just load the extras."*

Contents:

1. The words
2. Owner: on fixtures and groups
3. Where a fixture lives
4. Groups are the contract: cues on whatever rig the venue has
5. Universes: slots, the venue's patch, "not ours"
6. The clash check
7. The sidecar, extended
8. Old shows and old venue files
9. The surfaces (no new tabs)
10. Code map and tests
11. Choices to confirm
12. Not done

---

## 1. The words

| Word | Meaning |
|---|---|
| **Tour package** | The fixtures that travel with the show. Owner **Tour** (the default) or **Rental** (hired for the run; travels like Tour). Saved in the show. |
| **House rig** | The fixtures a room already has. Owner **House**. Saved in that room's venue, not in the show. |
| **Slot** | One of the show's universes, named and marked **Tour** or **House** ("U1–U4 Tour, U5–U6 House"). Saved in the show. |
| **Venue patch** | Where each slot goes in this room: the Art-Net node and IP, the sACN universe, the USB device. Saved in the venue. |
| **Not ours** | A universe, or a range of addresses in one, that the room keeps for itself (the house desk's house lights). Nothing of ours is sent there. Saved in the venue. |

## 2. Owner: on fixtures and groups

`RigOwner::Owner` (engine/src/rigowner.h): `Unset`, `Tour`, `House`,
`Rental`.

- **A fixture group has an owner** (`FixtureGroup::owner()`), `Unset` by
  default. Set a group's owner and all its fixtures follow.
- **A fixture can override it** (`Fixture::owner()`), `Unset` by default,
  which means "from its group".
- **The effective owner** (`RigOwner::effective(doc, fid)`):
  1. the fixture's own, when set;
  2. else the owner of the first group (lowest id) that has one and holds
     the fixture;
  3. else **Tour**.

  A fixture in two groups that disagree takes the first; Fixture
  Properties says which ("From FOH wash: House").
- **XML.** `<Owner>House</Owner>` inside `<Fixture>`, `<Owner>` inside
  `<FixtureGroup>`, written only when set, so a show that never uses it is
  byte-identical.

## 3. Where a fixture lives

The rule is the one calibration already follows: **with a venue applied,
what is the room's goes into the room's file; with None, everything is the
show's.**

- **A venue applied, a fixture becomes House** (its own owner, or its
  group's): it moves into that venue's **house rig**. From then on the show
  file does not have it: not the fixture, not its place on the plot, not its
  heads in the show's groups. The venue file has all three. The show is
  marked modified (it lost a fixture).
- **A venue applied, a house fixture becomes Tour or Rental**: it moves out
  of the venue into the show ("this one comes on tour now").
- **A fixture added while a venue is applied**, into a House group or set to
  House: the venue's.
- **No venue applied** (None): House is a label. The fixture stays in the
  show, as today. When a venue is applied later, the venue page offers
  **Move the show's house fixtures into <venue>**; nothing moves by itself,
  so an old show never changes under you.
- **Switching venue**: the old venue's house rig is written to its file and
  taken out of the rig; the new venue's is put in. The tour package is not
  touched: same ids, same addresses, same everything.
- **Applying and switching never mark the show modified**, as with
  corrections.

**Ids.** A house fixture keeps the id it had. The ids every venue's house
rig uses are **reserved**: a new show fixture skips them, so a house light
of one venue never shares an id with a tour light. If one does all the same
(the show was edited somewhere without its venue file), the house fixture
is renumbered when it is put in, and its group heads and plot place follow.

**Its calibration** is part of its house-rig entry (it *is* the room's), so
a house fixture never has a separate correction.

## 4. Groups are the contract: cues on whatever rig the venue has

Cues are written against groups: "FOH wash", "Side L". The show has the
group; each venue says which of its house fixtures **fill** it.

- The group is the show's (its name, folder, owner, its tour heads).
- A venue's **fill** of a group is the house heads in it, with their cells
  in the group's head layout. While the venue is applied the group holds
  both; the show file writes only the tour heads, the venue file only its
  fill.
- **Editing a fill** is editing the group as always (Fixture Manager, drag a
  house fixture in, the head-layout grid) with the venue applied.
- **Playback follows.** A Show Tree layer and a palette on a group play on
  whoever is in the group *now* (`Scene` follows its groups live, build.md
  "dynamic"). So a cue programmed at home, with home's house pars in
  "FOH wash", plays on the next room's house pars in "FOH wash".
- **What does not follow:** a raw channel value set on one house fixture
  (a classic Scene, an EFX fixture, a VC slider on that fixture). It names
  the fixture, so it plays only in that venue, and QLC+ drops values on a
  fixture that is gone when the venue is switched. Program house lights
  through groups. (Tested: `aGroupCuePlaysOnEachVenuesHouseRig`.)
- **A group fill whose cell is taken** (the show put a tour head there
  since): the house head goes in the next free cell.

## 5. Universes: slots, the venue's patch, "not ours"

**Slots (show).** A universe has a name (as in QLC+) and now an owner,
`Universe::slotOwner()`: Tour (default) or House. Set on the Connections
toolbar beside the name. XML: `Owner="House"` on `<Universe>`, only when
House. Slots are a statement of intent, used by the clash check (a tour
fixture in a House slot, a house fixture in a Tour slot) and shown on the
venue page; they change no output.

**The venue patch.** A venue can carry the output patch for every slot:
plugin, line, network and parameters (an Art-Net node's IP, an sACN
universe), exactly as `<Universe><Output>` stores them.

- **Recording it.** With a venue applied, change an output patch on the
  Connections tab and it goes into the venue (like Q111 for calibration);
  the venue then holds the patch of **every** slot, as it is now. The venue
  page also has **Keep the patch for <venue>** and **Use the show's patch**.
- **Applying it.** The show's own patch is held aside per universe
  (`Universe::setShowOutputBase()`), the venue's put on. A universe the
  venue has no patch for (added to the show since) keeps the show's.
- **The show file** always writes the show's own patch (the held base), so
  saving at a venue never moves the show's home patch.
- **None / switching** puts the show's patch back first.
- A venue without a patch behaves as today.

**Not ours.** A venue lists universes, or ranges of addresses in one, that
the room keeps:

- **A whole universe**: nothing is sent at all — the output plugins are not
  written for it, Blackout and Blind included.
- **A range** (e.g. U1 401–512): DMX and Art-Net/sACN frames have no gaps,
  so our frames carry **zero** there, whatever any fixture, Simple Desk or
  park asks. A desk owning those channels on the same line needs an HTP
  merge at the node; marking the whole universe is the clean way.
- **Patching into it is refused** (`Doc::canPatchFixtures()`), the Patch
  dialog marks such rows CONFLICT, and the clash check lists any fixture
  already there.
- Enforced in `Universe::dumpOutput()`, the one place every frame passes.

## 6. The clash check

`VenueRig::clashes(doc, venueName)` lists, for a venue applied or not
(a dry run: the venue's house rig against the show's tour package):

| Kind | What |
|---|---|
| **Overlap** | A house fixture and a tour/rental fixture (or two house fixtures) on the same addresses in a universe. |
| **Not ours** | Any fixture on addresses the venue marks not ours. |
| **Slot** | A house fixture in a Tour slot, or a tour fixture in a House slot. Only when the show has House slots at all. |

Where it runs:

- **Apply now** in the venue manager: the list first, then **Apply anyway**
  or **Cancel**.
- **Opening a show** whose venue is applied: the list as the venue note.
- **The venue page** lists them live.
- **The Patch dialog** marks not-ours rows CONFLICT and shows the venue's
  clashes under the table.

A house fixture that clashes when it is put in goes in **unpatched**, keeping
the address it wants (as a fixture that clashes on load always has), so it
never drives a tour fixture's channels. That holds for every kind of clash
(decided 2026-10-25, following Q138): an overlap, channels the venue keeps
(not ours), or a Tour slot when the show has House slots. A tour fixture
that clashes stays patched as the show has it: the venue never edits the
show, and nothing is sent on not-ours channels anyway. Adding or moving a
fixture onto not-ours channels is refused (`Doc::isOnNotOurs()`, used by
`canPatchFixtures()`, `addFixture()` and a patched fixture's move).

## 7. The sidecar, extended

```xml
<QlcVenues Version="2" Show="Spring Show.qlcc" Active="Lincoln HS" HouseIds="1000,1001">
 <Venue Name="Lincoln HS" Created="…" Modified="…">
  <Fixture ID="12" … PanOfs="1.25"/>            <!-- corrections, as before -->
  <Trim …/> <Target …/>                          <!-- as before -->
  <House>
   <Fixture> … the fixture as the show file writes it … </Fixture>
   <Monitor …> … its plot item and rig props … </Monitor>
   <Fill Group="3" Name="FOH wash">
    <Head Fixture="1000" Head="0" X="0" Y="0"/>
   </Fill>
  </House>
  <Patch Universe="0"><Output Plugin="ArtNet" UID="…" Line="1" Network="10.0.0.0/8">
   <PluginParameters outputIP="10.0.0.20"/></Output></Patch>
  <NotOurs Universe="5"/>
  <NotOurs Universe="0" From="401" To="512"/>
 </Venue>
</QlcVenues>
```

- `<House>` holds the fixtures in the show file's own `<Fixture>` format
  and the plot data in the show file's own `<Monitor>` format, so nothing
  about a fixture is lost or translated.
- `NotOurs` addresses are 1-based, as on screen.
- Version 2 only because there is more in it; version 1 files read as
  before (no house rig, no patch, nothing not ours).
- A file from a newer build reads for what this build knows. Elements it
  doesn't know -- between the venues, inside a `<Venue>`, `<House>` or
  `<Practice>` -- are kept whole and written back on the next save (at the
  end of their parent); a house `<Fixture>` is kept whole anyway. Unknown
  attributes on known elements are not kept.

## 8. Old shows and old venue files

- Every fixture and group without an owner is Tour; every universe is a Tour
  slot; nothing is written for them.
- A venue file without `<House>`, `<Patch>` or `<NotOurs>` applies exactly as
  today.
- A House label set with no venue applied changes nothing about where the
  fixture is saved.

## 9. The surfaces (no new tabs)

| Where | What |
|---|---|
| Fixture Manager, a group's editor | **Owner:** Not set / Tour / House / Rental, beside the name. |
| Fixture Properties (one or many) | **Owner:** From its group (…) / Tour / House / Rental. |
| Connections toolbar | **Slot:** Tour / House, beside the universe name. |
| Focus calls ▸ venue manager | **Rig and universes…** opens the venue page: house rig (count, groups filled, Move the show's house fixtures in), the slots with this venue's patch and Not ours, Keep / Use the show's patch, the clash list. |
| Lighting Studio plot | Overlay **Owner**: Tour blue, House amber, Rental violet; tooltip says which, and from where. |
| Inventory panel, patch list | The owner beside each fixture that is not Tour (the patch list gains an Owner column only when some fixture is not Tour). |

## 10. Code map and tests

| File | What |
|---|---|
| `engine/src/rigowner.{h,cpp}` | The enum, names, `effective()`, `effectiveAll()` |
| `engine/src/fixture.*`, `fixturegroup.*` | `owner()`, XML |
| `engine/src/universe.*` | `slotOwner()`, show output base, not-ours mask in `dumpOutput()` |
| `engine/src/venuerig.{h,cpp}` | House rig, fills, moves, venue patch, not ours, clashes |
| `engine/src/venuecorrections.*` | `Venue` gains the rig data; apply/unapply call `VenueRig` |
| `engine/src/doc.*` | Thin forwarders: `isVenueFixture()`, reserved ids, not-ours refusal; save skips venue fixtures |
| `engine/src/monitorproperties.cpp` | The show file skips venue fixtures |
| `ui/src/monitor/venuerigdialog.{h,cpp}` | The venue page |
| `ui/src/fixturegroupeditor.cpp`, `monitor/fixturefields.cpp`, `inputoutputmanager.cpp`, `monitor/monitor.cpp`, `patchinventorydialog.cpp` | The surfaces of §9 |

Tests: `engine/test/venuerig` (round trips, inheritance and override, the
switch, a group cue on two rigs, the clash check, not ours on the wire, the
venue patch applied and restored, the show file clean) and
`ui/test/venuerigui` (a group's owner in its editor, the venue page).

## 11. Choices to confirm

1. **Rental lives in the show**, like Tour (it travels for the run). Only
   House lives in a venue.
2. **House with no venue applied is a label**; the move into a venue is the
   button, never automatic on apply.
3. **Two groups disagreeing**: the first group (lowest id) wins.
4. **A range not ours is zeroed**, not skipped (DMX has no gaps); the whole
   universe is the only way to send nothing at all.
5. **Changing a patch with a venue applied records the venue's patch for
   every slot**, not only the one changed.
6. **Raw values on a house fixture** (classic scenes, EFX) are not carried to
   another venue's rig; only groups are.
7. **Any venue applied holds the show's patch aside**, so an output changed
   on the Connections tab while *any* venue is applied (even one made only
   for focus corrections) is that venue's from then on. A venue that never
   had an output changed plays the show's patch, as before.
8. **A focus practice** (scramble) started in a venue takes that venue's
   house rig with it, so the room's lights stay up; the practice's house
   movers are scrambled and scored like the tour's.

## 12. Not done

- The Rig Overview (3D) has no owner filter yet; the plot overlay has it.
- (Done: the focus report has an Owner column, `FocusReport::Row::owner`.)
- House structure (a room's own trusses) is still the show's structure with
  venue trims (Q128); a house fixture hangs on the show's structure.
