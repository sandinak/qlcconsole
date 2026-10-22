# EOS Import — ETC Eos → qlcconsole

Companion to the `.qxw` import work (`QxwImporter`, `ImportSelectionDialog`,
File → Import). Lets a user bring an existing ETC Eos show — patch, groups,
palette-equivalents, and the magic sheet layout — into qlcconsole so stages
set up on Eos (notably school rigs built on Element/Element 2) don't have to
be rebuilt by hand.

Status: **DESIGN ONLY**. No code yet. Header shapes in
`engine/src/eosimporter.h`, `engine/src/eosasciiparser.h`,
`engine/src/eosmagicsheetparser.h` are the proposed public surface and are
annotated to this doc; nothing else is wired up.

---

## What Eos actually exports (reality check, 2026-10)

| Target | Export path | What it carries |
|---|---|---|
| Patch, groups, cues, subs | **USITT ASCII** (Browser → File → Export → USITT ASCII, `.asc`) | Dimmer-era 1992 standard, cue list w/ times/labels/links/follows/parts, groups (as groups *and* presets, dual-emitted), subs renumbered |
| Magic sheets | **XML export** (Magic Sheet Editor → settings → export icon, `.xml`) | Object placements, target bindings (channel, group, cue, palette, macro…), labels. **Background images are NOT round-tripped** |
| Positions / 3D | **Augment3D** nested JSON (`working.a3d` inside `.esf3d`, which is a ZIP; `Scene/Scene.json`, `Scene/Primitive.json`) | Quaternions, euler, ARGB; readable per community reverse-engineering, not an official export |
| Everything else | — | `.esf3d` outer is proprietary TLV binary; **don't build on it** |

**Explicitly not available:**
- MVR export from Eos (Eos *imports* MVR; it does not export it — this was
  the clean path I'd suggested earlier and it's not actually a thing).
- OSC readback of magic sheet contents (`/eos/get/ms/<n>` returns only
  number/UID/label; placements and bindings are not exposed over OSC).
- Fixture personality export in GDTF or any portable format — a patched
  fixture in `.asc` references a personality by name only.

Prior art worth consulting rather than re-deriving: `Codys-Wright/eos-toolkit`
on GitHub has already mapped the magic-sheet XML attribute set
(`TARGETTYPE` codes 2=Cue, 3=Group, 4=Macro, 6=Palette {sub 1-4 = IP/FP/CP/BP},
9=Preset, 20=Channel, 33=MS, 45=Fader) and the Augment3D nested-ZIP layout.
The USITT ASCII format itself is a published 1992 USITT standard; Eos's
particular dialect (what it emits vs. what it will consume on import) is only
semi-documented and some record types are undocumented.

---

## Scope — v1

Three source files, one scratch Doc, existing merge pipeline:

1. **USITT ASCII** → patch, fixture groups, cues, submasters
2. **Magic sheet XML** → one XML per sheet, user picks which; channel-type
   objects seed Lighting Studio layer placements
3. *(optional, later)* **Augment3D JSON** → true 3D positions if the user
   has an Augment3D model

Out of scope for v1:
- Palette *values* for moving-light parameters beyond intensity — the ASCII
  format doesn't carry them cleanly. Palette *shells* (name + referenced
  channels) land as empty palettes the user fills in.
- Effects. Eos effects don't map 1:1 to QLC chasers/EFX; punt.
- Multi-part cues' detailed timing. Flatten to the cue's base time; note in
  warnings.
- Magic sheet background images.
- Pixel maps.
- Round-trip (QLC → Eos). One-way.

---

## Architecture — mirror QxwImporter

The existing import pipeline is:

```
.qxw file  →  Doc::loadXML() into scratch Doc  →  ImportSelectionDialog
                                                  (user picks)
                                                     ↓
                                           QxwImporter::import()
                                               ↓     ↓     ↓
                                      ID remap  dep closure  2D placement
                                                     ↓
                                              target (live) Doc
```

Eos import becomes the same shape, with one new piece at the front:

```
.asc + {.xml}* [+ .a3d]  →  EosImporter::load()  →  populated scratch Doc
                                                           ↓
                                              ImportSelectionDialog (unchanged)
                                                           ↓
                                            QxwImporter::import() (unchanged)
                                                           ↓
                                                      target Doc
```

So the parsers' job is just **populate a Doc that looks like QLC made it**.
Everything downstream — ID remapping, dependency closure, DMX conflict
relocation, 2D layer/group translation, palette carry — is already written
and tested. This is the whole point of the architecture: an importer is a
`Doc`-producing adapter.

### Classes

- `EosAsciiParser` (`engine/src/eosasciiparser.h`) — streaming USITT ASCII
  reader. Public method `parse(path, Doc*, FixtureMapper*)`.
- `EosMagicSheetParser` (`engine/src/eosmagicsheetparser.h`) — one magic
  sheet XML → a layer-placement plan, keyed by channel number. `parse(path)`
  returns a plain-data result the orchestrator consumes.
- `EosImporter` (`engine/src/eosimporter.h`) — orchestrator. Takes a bundle
  of files (one `.asc`, zero-or-more `.xml`), builds the scratch Doc, hands
  off to the UI layer. Mirrors `QxwImporter`'s header shape; **does not
  mutate the target Doc** — that's still `QxwImporter`'s job.
- `EosFixtureMapper` — resolves Eos personality name → QLC `QLCFixtureDef` +
  `QLCFixtureMode` via `QLCFixtureDefCache::fixtureDef(manufacturer, model)`.
  Interactive side (the "which QLC fixture is this Eos personality?" prompt)
  lives in UI; the mapper itself is engine-side and pure.

### UI entry point

File → Import gets a format-dispatching open: existing `.qxw` behavior is
preserved; a user who picks an `.asc` goes into a two-step wizard —

1. Pick companion files (magic sheet XMLs, optional Augment3D).
2. Reconcile Eos personalities → QLC fixture defs (saved to a mapping file
   per user, so re-imports don't re-prompt).

Then falls through to the usual `ImportSelectionDialog`.

A per-user mapping file lives at `~/.qlcconsole/eos-fixture-map.json`. Keyed
by Eos personality string ("ETC Source 4", "Chauvet Rogue R1 Spot"…) →
`{manufacturer, model, mode}` tuple. First-time import is a dialog; later
imports are silent for known mappings.

---

## USITT ASCII — what to parse

The record types we care about, in rough emit order:

- `Ident`, `Manufacturer`, `Console`, `Clear`, `Version` — metadata, mostly
  for the warnings log.
- `Patch` — fixture address + personality + channel number. The core.
- `$Patch` — Eos-extended patch records (channel label, address, personality
  reference by number). Need to cross-ref against the Eos personality list
  earlier in the file.
- `$FixtureList` — personality definitions by number used in `$Patch`.
- `Group` — channel group. One-to-one with `FixtureGroup` by name.
- `Sub` — submaster. Maps to a QLC Scene (one scene per sub, levels as
  `SceneValue`s). Submaster bump/handle metadata lost.
- `Cue` — cue number, label, up/down time, follow, link, part number, level
  list. Each cue becomes a QLC Scene; a chaser can group them per cue list
  if we feel like reconstructing one, but **v1 just makes scenes** and
  leaves the chaser-building to the user.
- `$Preset` / `$IntensPalette` / `$FocusPalette` / `$ColorPalette` /
  `$BeamPalette` — palette shells. Create `QLCPalette` of the right kind,
  populate with channel references, leave values empty with a warning.

Known Eos quirks per the ETC forum / Blue Room:
- Groups are dual-emitted as **both** `Group` and `$Preset` records; dedupe.
- Submasters are renumbered on export (Eos has no sub pages; ASCII flattens
  them); re-use the Eos sub number only as a label hint, not an identity.
- Cue parts' timings are per-channel-group inside the part — complex;
  flatten and warn.

### Fixture type resolution

Each `$Patch` references a personality by number. Each personality has a
manufacturer + model + mode name. Resolution strategy:

1. Normalise the Eos manufacturer/model to tokens (strip "ETC ", trim
   whitespace, lowercase).
2. Look up in `~/.qlcconsole/eos-fixture-map.json`. Hit? Done.
3. Try an exact match in `QLCFixtureDefCache`. Hit? Save mapping, done.
4. Try a fuzzy match (Levenshtein ≤ 2 on model, exact on manufacturer).
   Offer the top candidate to the user. Save their choice.
5. No candidate → "Generic / Generic Dimmer" fallback, flagged in warnings.
   User can fix the mapping and re-run import.

---

## Magic sheet XML — what it buys you

Per the research pass: `TARGETTYPE` codes we care about —

- `20` Channel — this is the gold. `(x, y)` on a magic sheet is almost
  always a position that reflects the physical plot in school/community
  venues. These become **Lighting Studio layer placements** keyed by
  channel number. The Lighting Studio canvas has a world coordinate system;
  magic sheet coordinates need a one-time per-sheet calibration (two
  reference points — "this fixture is at stage-left wing, this one is at
  centerline") to establish the mapping.
- `6` Palette (sub 1-4 = IP/FP/CP/BP) — target-list ID points at a palette.
  Doesn't carry layout info we need, but useful as a cross-check against
  what `.asc` claimed.
- `3` Group — same.
- `2` Cue, `4` Macro, `9` Preset, `33` nested Magic Sheet, `45` Fader —
  these are *control* elements, not plot elements. Fate:
  - If the user also wants a **Virtual Console** page built, these could
    become VC buttons/sliders. Punt in v1.
  - Otherwise, drop with a warning count.

Image elements and arbitrary shapes: dropped. Documented as a limitation
in the import report, with a suggestion to screenshot the magic sheet in Eos
and bring it into Lighting Studio as a background separately.

### Calibration dialog

After parsing one or more magic sheet XMLs, before placing fixtures, show a
small calibration dialog:

- List each sheet.
- For each: user clicks two channels and types their real-world coords (or
  picks from Lighting Studio's existing structures: "channel 101 is on T-2
  stage left").
- Save calibration per `(showfile fingerprint, sheet UID)` so re-imports are
  silent.

If no calibration is given, channels still import with coordinates relative
to the sheet's own origin, into a dedicated "Imported — <sheet name>"
Lighting Studio layer. User can position/rotate/scale the whole layer later.

---

## Augment3D (optional, v2)

If `working.a3d` is present inside an uploaded `.esf3d`:

- Unzip the outer `.esf3d` (standard ZIP).
- Unzip `working.a3d` (nested ZIP).
- Parse `Scene/Scene.json` for position/orientation per fixture reference.
- Match fixtures by channel number against the already-parsed patch.
- Overrides magic-sheet-derived positions where present.

Per eos-toolkit, this format is reversible and safe to read; **do not** touch
the outer `showdat.dat` TLV binary — their explicit guidance.

Punted to v2 because: (a) most school rigs don't have Augment3D models, (b)
magic sheets already give enough signal for Lighting Studio, (c) parsing a
nested ZIP of JSON isn't hard but testing it needs real `.esf3d` samples.

---

## Reporting / warnings

`EosImportResult` (parallels `QxwImportResult`): counts imported fixtures /
groups / cues / palettes-as-shells, and a `QStringList warnings` for every
item dropped or flattened:

- "Cue 10/5 part 2: per-channel-group timing flattened to base time."
- "Palette CP 1 'Warm Front': channel references preserved, values empty."
- "Magic sheet 'FOH': 4 image elements dropped (not round-tripped by Eos)."
- "Eos personality 'ETC Source 4 LED Lustr+' → no QLC match; patched as
  Generic Dimmer."

Report rendered post-import by the same dialog that renders `QxwImportResult`.

---

## Open questions

- **Fixture library coverage.** QLC's library has broad coverage, but Eos
  installs commonly use ETC's own fixtures (Source 4 LED series, ColorSource,
  Desire) which may be present under slightly different names. First real
  import against a school show will surface what's missing. Mitigation:
  the per-user mapping file; a `tools/eos-fixture-map-helper/` script to
  bulk-edit it.
- **Cue list → chaser or loose scenes?** v1 says loose scenes. If users
  almost always want a chaser, revisit — but then we have to decide what a
  chaser-ful of cues does differently from Eos's cue list semantics
  (follows, links, loops), which is non-trivial.

  **Update 2026-10-22:** settled by the Show Tree. Each Eos cue list comes
  in as a Show Tree production with one cue list, through the tree's own
  import path: the scratch Doc carries a chaser per cue list (one scene per
  cue, its full tracked state; cue number, up/down time, follow/hang), and
  after the merge it goes through Import in place, Capture into palettes
  and Convert to tracking -- one undo step. Eos tracks and the tree tracks,
  so the list ends up tracking; links and delays are reported, not carried.
- **How to represent sub-levels.** A sub in Eos drives channel levels; a
  QLC Scene is the obvious analogue, but a sub also has a handle/bump
  behavior. Scene is fine for the "what levels"; VC fader binding to run it
  is a later tie-in.

  **Update 2026-10-22:** subs now come in as VC faders. Each sub is still a
  Scene ("Eos/Subs", the sub's up/down times as its fades). After the
  merge, `EosImportWizard::addSubFaders()` puts a Playback-mode VC Slider
  on each one, captioned "Sub N label", in a row in a new frame "Eos subs"
  below what the console holds. `$$LTP`, `$$Exclusive`, `$$InhibSubm`,
  `$$EffectSub`, `$$Independent` and `$$IntensityMaster` are parsed into
  `EosSub::options` and reported. Bump buttons and inhibitive behaviour
  aren't carried. A wizard page, **Special circuits**, classifies each
  channel from its label (`EosCircuit::classify`, most specific word wins:
  House > Work > Hall/Blue/Backstage > Relay) and reads `$$Options` X (GM
  exempt). The user's edits travel in `EosImportBundle::circuitRoles`. The
  roles make the groups Eos/Relays, Eos/House & Work and Eos/Backstage ("…
  (special circuits)" when the show has an Eos group of that name), and
  map to fixture output rules in `EosImporter::applyCircuitRoles()`.
  Until `feat/output-rules` merges, that function runs on a marked shim.
  The wiring is one line, `using Rule = OutputRulesShim;` →
  `using Rule = Fixture;`. House and relay cue moves stay in the cues
  (cue 1 still brings the house up).
- **Multiple cue lists.** Element 2 has a single cue list, so a school show
  is a non-issue; a Ti or Gio show might have >1. v1 handles with a prefix
  on scene names ("CL1 cue 10.5"); not elegant but survives.
- **ASCII dialect drift.** The format is 34 years old and Eos extends it
  with `$`-prefixed records. We'll hit records we don't recognise;
  parser must skip-and-warn, never fail-hard on unknown records.

---

## Effort

Rough sizing:

- `EosAsciiParser` — ~2–3 days to get patch + groups + cues solid, another
  1–2 for palette shells. Fuzzing-friendly (lots of real sample files on
  support forums).
- `EosMagicSheetParser` — 1 day for the subset of `TARGETTYPE` codes above;
  eos-toolkit docs save discovery time.
- `EosImporter` orchestrator — 1 day.
- Fixture mapper + UI reconciliation dialog — 2 days.
- Magic sheet → Lighting Studio layer placement + calibration dialog — 2
  days (reuses Lighting Studio's existing layer/placement model).
- Integration into File → Import dispatch — half day.
- Tests, including a captured-from-real-Eos sample set — 2 days.

Call it **two weeks of focused work** for v1 (ASCII + magic sheets).
Augment3D v2 is probably another 2–3 days once real sample files are in
hand. Nothing in here is architecturally hard — the hard problems
(ID remapping, 2D placement carry, palette translation) are already solved
in `QxwImporter`.

---

## Testing

Sample files needed:

1. A reference school-rig `.asc` (one cue list, ~20 fixtures, a mix of
   dimmers + Source 4 LED movers).
2. A corresponding magic sheet XML with channel-type objects laid out
   plot-shaped.
3. *(v2)* A matching Augment3D export.

The user has stages they want to import — those files are the test corpus.
Can live under `test-workspaces/eos-samples/` next to the existing `.qxw`
test workspaces, same convention.

`engine/test/eosimporter/` and `engine/test/eosasciiparser/` tests follow
the shape of `engine/test/qxwimporter/` (QObject + Qt test macros, sample
file assets alongside the test, round-trip assertions on a target Doc).

---

## References

- ETC Eos USITT ASCII import docs:
  https://support.etcconnect.com/ETC/Consoles/Eos_Family/Software_and_Programming/Eos_Family_USITT_ASCII_Import
- ETC Eos Magic Sheet docs:
  https://www.etcconnect.com/WebDocs/Controls/EosFamilyOnlineHelp/en/Content/19_Magic_Sheets/Magic_Sheet_Editor/Magic_Sheet_Settings.htm
- `Codys-Wright/eos-toolkit` — community reverse-engineering of Eos file
  formats, incl. magic sheet XML target codes and Augment3D nested ZIP:
  https://github.com/Codys-Wright/eos-toolkit
- Blue Room forum thread on Eos ASCII dialect limitations:
  https://www.blue-room.org.uk/index.php?showtopic=69400
