# Shows that carry their own definitions — design

*Status: proposal for review (2026-10-16). Branch `feat/show-embedding`.*

A `.qlcc` opens complete on any computer: the custom fixture definitions,
effect scripts, RGB scripts and head layouts it uses travel inside it. When
the show's copy and the computer's copy of something differ, the show keeps
its own and the footer says so — nothing changes under a patched rig without
the operator choosing it.

Why (2026-10-15/16): DanceHysteria opened on the Windows test machine with
most of its rig missing — ~100 of Branson's own definitions live only in his
user folder, and four it needs (the PHS step rows among them) only in the Mac's
show cache. The rig drew as red stand-ins and nothing lit.

Decisions taken:

- **Embedded in the show file** (option A), not a show folder or a zip. One
  plain XML file, easy to mail and sync; **Collect for Transfer…** writes a
  folder when one is wanted.
- **A difference warns; the show keeps its own version** until the operator
  chooses (below).

---

## 1. What travels

Saving a `.qlcc` embeds every item the show uses that is **not part of the
app**:

| Kind | Embedded when | Matched by |
|---|---|---|
| Fixture definition (`.qxf`) | a fixture uses it and it didn't come from the app's own library (user folder, legacy QLC+ folder, show cache, the show's folder) | manufacturer + model |
| Effect script (`.js`) | a look uses it and it isn't one the app ships | script file name |
| RGB script (`.js`) | an RGB matrix uses it and it isn't shipped | script name |
| Head layout | already stored in the show (MonitorProperties) — unchanged, but part of the comparison below | layout key |

Shipped items stay out, so the file grows only by what's yours (a few KB per
definition). `.qxw` is written as before, without the section, so QLC+ can
still open it.

## 2. Format

One section in the workspace XML, before `<Engine>` so it's read first:

```xml
<Embedded>
  <FixtureDefinition Manufacturer="PHS Chorus" Model="Step Row 64 Heads"
      SHA256="9f2c…" From="user" FromModified="2026-10-11T07:08:00"
      Embedded="2026-10-16T09:12:00"><![CDATA[<?xml …the .qxf, byte for byte…]]></FixtureDefinition>
  <EffectScript File="stage-shape.js" SHA256="…" …><![CDATA[…]]></EffectScript>
  <RGBScript File="…" …><![CDATA[…]]></RGBScript>
</Embedded>
```

- The file's bytes, unchanged (base64 instead of CDATA only if the text holds
  `]]>`), so the checksum is exact and a copy can be written back out as the
  same file.
- `SHA256` of those bytes; `From` (user / legacy / showcache / showfolder),
  `FromModified` (the source file's time when it was embedded) and `Embedded`
  (when) — what the warnings below use to say which side looks newer.

## 3. Opening a show

For each item the show uses:

| Case | Result |
|---|---|
| Only the show has it | Used from the show. Footer (info): **"N definitions came with this show"** — **Install on this computer** writes them to the user folder. |
| Only the computer has it | Used; embedded at the next save. |
| Both, identical (same SHA-256) | Nothing to say. |
| **Both, different** | The **show's** version is used. Footer (**warning**): **"N items in this show differ from this computer's"** — **Review…** |

The show's version is put in the fixture-definition cache (or script cache)
for as long as this show is open; opening another show starts from the
computer's again.

## 4. Review… — one row per difference

- **What it is** — "PHS Chorus Step Row 64 Heads (fixture definition)".
- **Which side looks newer** — the computer's file changed after it was
  embedded (`FromModified` older than the file's time now): *"Updated on this
  computer since the show was saved"*; or the show's copy is newer than the
  computer's file: *"This computer has an older copy"*; or can't tell.
- **Safe to switch?** checked against what the show actually uses:
  - *Fixture definitions*: every mode the show's fixtures use still exists in
    the other version, with the same channel count and the same channel kinds
    in the same order → **compatible** ("names and sizes differ; channels the
    same"). Otherwise **not compatible**, listing the fixtures and what
    changed ("16-channel mode now 18 channels: 32 fixtures would need
    re-patching").
  - *Scripts*: every parameter the show's looks set still exists with the same
    type → compatible; otherwise the looks affected are named.
- **Actions** (per row, and for all compatible rows at once):
  - **Use this computer's** — the show switches now (asks first when not
    compatible), and embeds it at the next save.
  - **Keep the show's** — the warning stops for that item (remembered in the
    show).
  - **Install the show's** — writes the show's version over the computer's file
    (asks first; the old file goes to the Trash).

## 5. Saving

Save embeds **the version the show is using** — never quietly the computer's.
An updated definition reaches the show only through **Use this computer's**.
A first save of a show that has never embedded anything embeds what it's using
now (the computer's, as today).

## 6. Collect for Transfer…

**File → Collect for Transfer…** writes a folder:

```
DanceHysteria/
  DanceHysteria.qlcc
  Fixtures/*.qxf
  EffectScripts/*.js
  RGBScripts/*.js
  HeadLayouts/*.json
```

And opening a show also looks in its own folder's `Fixtures/`,
`EffectScripts/` and `RGBScripts/` (today only a `Manufacturer-Model.qxf`
next to the show, and `.qxi` input profiles, are found there), so a collected
folder opens complete even as a `.qxw`, and someone who keeps definitions with
a show can.

## 7. Before any of this: a path bug

The user folders for **effect scripts** and **head layouts** are hard-coded to
the Mac's `Library/Application Support/qlcconsole/…`
(`engine/src/effectscriptcache.cpp:20`, `engine/src/headlayout.cpp:28`), so on
Windows and Linux your own effect scripts and saved head layouts are never
found. They move to the same per-platform user folder as fixture definitions.

## 8. Phases

| Phase | Scope |
|---|---|
| P0 | The path bug (§7) |
| P1 | Embed on save (`.qlcc`); read on open; "only the show has it" + Install on this computer |
| P2 | Differences: SHA compare, show's version wins, footer warning, Review… with newer-side and compatibility, the three actions; save embeds what's in use |
| P3 | Collect for Transfer…; the show folder's `Fixtures/` `EffectScripts/` `RGBScripts/` searched on open |

## 9. Tests

- Round trip: a `.qlcc` with custom definitions and scripts saves the
  `<Embedded>` section (exact bytes, right SHA-256); a `.qxw` doesn't.
- A clean "computer" (empty user folder, test override) opens the show
  complete from the embedded copies; Install writes the files.
- Identical copies: no warning. Different copies: the show's used, the
  warning raised; newer-side text from the times; compatible vs not (mode
  removed, channel count changed, channel kind changed; script parameter
  removed) with the fixtures/looks named.
- Use this computer's / Keep / Install, and that Save never swaps versions.
- Collect for Transfer writes the folder; that folder opens complete as
  `.qxw` on a clean computer.
- Shipped definitions and scripts are never embedded.

## 10. Not in scope

- Fetching definitions from the internet (the QLC+ online library).
- Embedding audio/video media (already stored relative to the show).
- Merging two versions of a definition.
