# Show Tree — design

*Status: proposal for review (2026-10-16; updated the same day with Beam & gobo, line targets, sub-cues, the Library panel, the cue runner and the end-to-end workflow). Branch `design/show-tree`. Docs only.*

One tree for writing a show, top to bottom: **Production → Cue list → Cue →
Layer → looks**. A *layer* (a target plus its looks) is the unit of work;
scenes and collections become what the tree generates, not what you file.

Why (Branson, 2026-10-16, writing DanceHysteria in the Programming tab):

- "I find myself having to create subfolders in each look type, scene and
  collections of the same thing just to build stuff out." The show file bears
  it out: `Palettes/Color/01-Everybody Scream`, `Show/00-Preshow` (collections)
  and `Show/0-Preshow` / `Show/1-Everbody Scream` (scenes): one song, three
  trees, two spellings.
- "Is a scene really the right atomic place to collect work into
  collections?" His sketch: chase → step is a collection → collection holds
  scenes *or fixture groups with looks applied*; looks at each level apply to
  everything below it.
- He carries looks forward with collections of collections (0.0.5-WHO ARE WE
  = 0.0.4-SHOUT + StepTops-Bright; 0.0.6 = 0.0.5 + FlashWashes). That works
  today only by luck: both StepTops scenes run at once and intensity is HTP, so
  the *brighter* one wins. Going from Bright back to Dim the same way would
  not work. The tree resolves what wins instead of running both.

---

## 0. Decisions for Branson

Each has a recommendation; the rest of the doc is written to it.

**Your call:** `ok` takes the recommendation; otherwise write the option you want (its words from Options), a note, or `?` to talk it over. Blank = not reviewed.

| # | Decision | Options | Recommend | Your call |
|---|---|---|---|---|
| D1 | **Root name** | Production / Show set | **Production.** "Show" is the timeline function, "set" is taken by *group set* (and means a setlist to a band). Note SHOW_LIFECYCLE_DESIGN.md uses "Production" for the *phase* (Construction → Test → Production); that word isn't on screen anywhere, so rename the phase "Run" in that doc. | ok |

| D2 | **Where it lives** | A view inside Programming / its own top tab | **Its own tab, "Show Tree"**, beside Programming. Shares the Look Editor and the source trees (palettes, fixtures & groups). Programming stays for one-off scenes and palette work. | tab |
| D3 | **Cascade resolved when** | Baked at edit time into generated scenes / resolved by the engine at playback | **Baked** (§4). Bake palette *references*, not DMX, so palette edits and group membership stay live with no re-bake. Engine resolution is a later option (§5), not a prerequisite. | baked |
| D4 | **Layer editor shape** | Today's ordered look *stack* / one row per *attribute* | **Rows per attribute** (Colour, Dimmer, Position, Beam, Shutter, Effect; Beam's sub-attributes and a line aim's parameters as child rows, §3A/§3B). A cascade is per attribute, so the editor has to be; the generator writes the stack in the canonical order the stack rules already define. Stack order only mattered for "lower wins" between two looks of one kind, which a row makes impossible. | ok |
| D5 | **Precedence** | — | **This cue beats earlier cues; inside a cue the nearer scope wins.** Tracked < included < cue looks < layer looks < effect's own looks. One sentence the operator can hold. | ok |
| D6 | **What cue looks reach** | The cue's own layers / also layers tracked into it | **Own layers only.** A tracked layer shows its value and where it came from; one click ("Bring into this cue") makes it an own layer, and the cue's looks then reach it. Otherwise a "Blue" on cue 12 silently repaints everything still on from cue 3. | ok  |
| D7 | **What Release goes to** | Dark / home / the list's base cue | **The base cue if the list has one, else dark.** A cue list may mark one cue as its *base* (DanceHysteria's `Show/Base` scenes are exactly this). Released fixtures return to what the base gives them; ones the base doesn't cover go to intensity 0 and *hold* their other attributes (no swing while fading, and move-in-black can preset them). | ok  |
| D8 | **Include + tracking** | — | **Include brings the included cue's resolved output (as if you stood in that cue), live-linked.** Order: tracked state → includes (in order) → this cue's own. Included values track onward like any other. No cycles. The included cue's own Cue-only/Block don't affect the includer. | ok |
| D9 | **Sub-cues** | Track through sub-cues into the next main cue / sub-cue changes are cue-only | **Cue-only.** Cue 5 tracks from cue 4's own state, not from wherever Sub GO got to (it can't be known at bake time, and SUBCUES_DESIGN.md already drops the sub-cues on GO). Inside the sub-list, 4.2 tracks from 4.1. | ok  |
| D10 | **An effect at cue level** | Copied into each layer / one effect across all the cue's layers | **One effect across them**, so a Sinelon runs across the stage as one (the multi-group stage layout already does this). Layers that set that effect kind themselves — or set it to **None** — are left out of it. |  ok |
| D11 | **One-off values** | Must be a named palette / can be inline | **Inline allowed**: an unnamed palette owned by the layer, hidden from the palette tree. **Make reusable** names it and files it. This is what stops `Palettes/Color/01-Everybody Scream`. | ok |
| D12 | **Import of existing shows** | Convert in place / import as a copy | **Import a chaser as a cue list, as a new production; originals untouched.** Imported list starts **Non-tracking**, so it plays exactly as before; **Convert to tracking…** then offers to strip what repeats. (§7) |  ok |
| D13 | **Hand-editing a generated scene** | Allowed (overwritten on re-bake) / locked | **Locked**, with **Detach to plain scene** (the tree then points at that scene as an opaque function layer). | ok |
| D14 | **Cue-list-level looks** | Yes / no | **No.** A base cue + tracking does that job and shows up in the tree as a cue. |ok  |
| D15 | **Beam family** | Gobo as its own attribute / Beam with sub-attributes | **Beam with sub-attributes** Gobo, Gobo rotation, Prism, Zoom, Focus, Iris, Frost — **each cascades and tracks on its own** (a cue changes Gobo while Zoom tracks). Gobo palettes are *named* ("Breakup", "Dots", "Open") and map per fixture definition to that type's wheel slot; fixtures without the capability ignore it, and the editor says **"applies to 4 of 8 fixtures"**. (§3A) | ok  |
| D16 | **Which look parameters inherit one by one** | Every palette parameter / only aim parameters + Beam sub-attributes | **Only the line-aim parameters (Spread, Grouping, Cross) and the Beam sub-attributes.** They are what changes cue to cue on the same look. An effect stays one value with its child looks (D10): changing an effect's speed per cue = another effect palette, or later a speed master. Revisit if effect parameters turn out to be cued as often. | ok  |
| D17 | **Cue-level aim at a line** | Each layer spreads on its own / one formation | **One formation over the union of the cue's own layers' fixtures** (as D10 for effects). Fixtures are numbered **by hang position along the line across the whole union** (the line design's Q3), not group by group, so a Towers + AT-Spots formation interleaves. A layer that sets its own Position — or its own Spread/Grouping/Cross — is left out and spreads on its own. A tracked formation stays one formation until a later cue changes some of its members. (§3B) | ok |
| D18 | **Cue-level Spread / Grouping / Cross** | Reach every layer / only layers aiming at a line | **Only layers whose resolved Position is a line aim.** "Spread +5" on a cue then means "spread whatever aims at a line here", and a wash layer doesn't silently carry a meaningless Spread forward. |ok  |
| D19 | **A spread change between cues** | Snap / fade | **Fade over the cue's fade time** (pan/tilt fade, so the beams sweep — straight in pan/tilt space, close to but not exactly along the line, as the line design's Q8). The plan view shows where the beams were in the previous cue. | ok |
| D20 | **Library panel** | One tree of everything / sections | **Recent · In this cue list · Pinned · All** (with one filter box over all four). What a song uses is in the first two; All is for finding a thing the first time. (§6A) | ok |
| D21 | **One-off values** (extends D11) | — | Set inline from the picker (a colour swatch, a % slider) → an **unnamed palette owned by the layer**, shown at once in **Recent** as "Green-ish · 1.0.1 Towers". Dropping it on a second layer reuses it and **offers to name it** (Make reusable); unnamed it never appears in All. | ok |
| D22 | **How edits land** | Straight into the selected cue / an Eos-style programmer with Update / Record | **Straight into the selected cue**, one undo step per action. Two explicit extras: in the picker, a **tracked** value offers *Set in this cue* or **Update cue 1.0.0** (where it was set, so the change tracks on from there); and **Record as new cue** freezes everything on stage into a new Block cue. A staged programmer can come later if wanted; it doubles every edit's steps. | ok |
| D23 | **Numbering an inserted cue** | Renumber / next free / point cue | **Next whole number in the last place if free, else a point cue** (`0.0.4` + `0.0.5` taken → `0.0.4.5`), skipping numbers its sub-cues use (`0.0.4.1`…). **Never renumber automatically**: QLab and show control address cues by number. A new list's first cue takes its number from the list's leading digits (`01-Everybody Scream` → `1.0.0`). |ok  |
| D24 | **Command line** | No / P1 / later | **Yes, after P1 (with P2), optional.** It calls the same operations as the mouse (one op layer, so it's cheap); tiny grammar, Tab completion from the Library, hint line under it. Not in P1, which must prove the mouse path first. (§6C) | ok |
| D25 | **Sub-cues in the tree** | — | Per SUBCUES_DESIGN.md and D9: a cue's children are its layers **and** its sub-cues (4.1, 4.2…), each with its own layers, fade, wait and **Auto-follow / Sub GO**. 4.2 starts from 4.1; sub-cue changes are cue-only, so cue 5 starts from cue 4's own state wherever Sub GO got to. Origin label "from sub-cue 4.1". (§3C) | ok |
| D26 | **Cue runner: where it lives** | Replace the VC Cue List widget / beside it | **One runner component with two homes**: a full-screen **Run** tab (operator view: big targets, keyboard GO, nothing else to bump) and a **Cue Runner** Virtual Console widget placed on VC pages beside faders/buttons/speed masters, in two sizes — **compact** (current, next, big GO + Sub GO) and **list** (the full runner with sub-cues and the "what changes" line). Several widgets may each follow a different cue list or production. Both mirror the same state; footer GO, keyboard and QLab/OSC/MSC drive the same runner. The existing **VC Cue List widget stays** for plain chasers (old shows unchanged); for Show Tree productions the Cue Runner widget replaces it, with **Swap widget** on a page. (§6B) |ok  |
| D27 | **Back in the runner** | Previous sub-cue / previous main cue | **Follow sub-cues P2 as shipped:** **Back** (⌘⇧↩) = the previous main cue (its sub-cues dropped); **Sub Back** (⌥⌘⇧↩) = the previous sub-cue. Click a cue = **LOAD** (it becomes NEXT/STANDBY); double-click = jump now. |ok |

---

## 1. The model

```
Show file
├── Library  (shared by every production)
│   ├── Fixtures, Groups               (as today)
│   ├── Group sets                     new: named list of groups, e.g. Towers = T1..T4
│   ├── Targets                        points, areas, lines (Lighting Studio)
│   ├── Palettes                       (as today; + Beam: gobo, zoom, … palettes)
│   ├── Reusable layers                new: a layer promoted with Make reusable
│   └── Reusable cues                  new
└── Productions
    ├── DanceHysteria
    │   ├── settings: tracking default, timecode source, base cue
    │   ├── Cue list "Full Show"
    │   │   ├── Cue 0.0.0-base       [Block, base]
    │   │   │   ├── cue looks: Shutter Open, Dimmer Base, Colour White
    │   │   │   ├── Layer USTs:      Position UST-0
    │   │   │   └── Layer Washes:    Colour Blue, Position Spots-0
    │   │   ├── Cue 0.0.3-Announcer  [Track]
    │   │   │   ├── Layer AT-Spots:  Dimmer Full, Aim Announcer,
    │   │   │   │                    Effect Follow Spot
    │   │   │   └── Layer ⟲ BackStage   (reusable, used in 3 cues)
    │   │   ├── Cue 0.0.4-SHOUT …
    │   │   └── Cue 0.0.9-AnnouncerFinish [Block] includes 0.0.3
    │   └── Cue list "01-Everybody Scream"
    └── Corporate Gig …
```

| Node | Holds |
|---|---|
| **Production** | Name; cue lists; tracking default; timecode source; folder for its generated functions. VC / footer GO / show control act on the **current production**. |
| **Cue list** | Name; mode **Tracking** / **Non-tracking**; optional **base cue**; the Chaser's playback settings (run order, tempo, GO on the beat). |
| **Cue** | Number (`0.0.4`), name (`SHOUT`), note, fade/hold (the ChaserStep's), **mode** (List default / Track / Block / Cue-only), **includes** (cues), **cue looks**, **layers**, optional **sub-cues**. |
| **Layer** | **Target** (group, group set, or fixtures), **looks** by attribute, **Carry** or **Release** when the cue is left, optional child layers (§3.5), optional link to a reusable layer. Or an **opaque function layer**: any existing function (EFX, RGB Matrix, a raw-value scene) played as-is. |
| **Look** | A palette (named, or inline/unnamed) on one attribute; an **Effect** look can hold child Colour/Dimmer/Gobo looks (today's rule: they belong to that effect only). A look can be **None** — "this attribute is off here", to stop an inherited value. A Position look can be **Aim at <target>**; aimed at a line it carries **Spread / Grouping / Cross**, each set and inherited on its own (D16). |

**Reusable**: *Make reusable* on a layer or a cue moves it into the Library and
leaves a reference behind (⟲ badge, "used in N cues"). Edit it anywhere →
everywhere. *Detach* turns one reference back into a local copy (badge
"detached from BackStage" until edited). Palettes stay the reusable *values*;
reusable layers/cues are reusable *arrangements*.

**Group sets** expand to their member groups as separate targets of one
generated scene, so effects lay out across the stage as they do today for a
multi-group look, and each member keeps its own head layout.

---

## 2. Cascade and tracking rules

**Attributes:** Colour (an ordered list — colour 1, 2, 3 as today), Dimmer,
Position (Pan/Tilt or Aim at a target), **Beam** (sub-attributes Gobo, Gobo
rotation, Prism, Zoom, Focus, Iris, Frost), Shutter (incl. strobe), and Effects
keyed by kind (colour / dimmer / movement / beam effect). Each resolves on its
own — and so does **each Beam sub-attribute** and **each line-aim parameter**
(Spread, Grouping, Cross): the cascade below runs once per key, not once per
look (D15, D16). Cue-level aim parameters only reach layers that aim at a line
(D18).

**Inside one cue** (D5): for each layer and attribute, the first that sets it:

1. the effect's own child looks (inside that effect only)
2. the layer's looks (child layer before parent layer)
3. the cue's looks
4. included cues' output, last include first
5. what tracked in from the previous cue

**Between cues** — what cue *n* starts from:

| Cue mode | Starts from | Passes on to *n+1* |
|---|---|---|
| **Track** | output of *n−1* | its own output |
| **Block** | nothing (its includes and own content only) | its own output |
| **Cue-only** | output of *n−1* | what it *started* from (its changes don't pass on) |
| *List default* | Track in a Tracking list; Block in a Non-tracking list | as that mode |

Then, leaving a cue, each layer marked **Release** is dropped from what passes
on and goes to the base cue's values, else dark (D7).

**Overlap between layers** (Steps, and US inside it, in the same cue): the
lower layer in the cue wins on what both set, the same "lower wins" as the look
stack; tracked layers sit under all of the cue's own.

The editor marks every value with its origin: own values plain; inherited ones
greyed with the source — *"Blue — from Cue 0.1"*, *"45% — tracked from
0.0.3"*, *"Red — included from 0.0.3"*, *"White — Confetti's own"*,
*"Spread +5 — from Cue 1.0.1"*, *"Aim at DS-Edge — tracked from 1.0.0"*,
*"Spread +5 — from sub-cue 1.0.3.2"*.

---

## 3. Worked examples

### 3.1 Branson's example: global → layer → effect

```
Cue 1.0.0-PreCurtain        cue looks: Colour Blue, Dimmer Full
├── Layer TrussC-RGB        (nothing)
├── Layer Towers (set)      Effect Confetti ─ child: Colour Red
└── Layer USTs              Dimmer 45%, Position UST-0
                            Effect Sinelon ─ child: Colour White
```

| Layer | Colour | Dimmer | Position | Effect |
|---|---|---|---|---|
| TrussC-RGB | Blue *(cue)* | Full *(cue)* | — | — |
| Towers | Blue *(cue)* — base under the effect | Full *(cue)* | — | Confetti in **Red** *(its own)* |
| USTs | Blue *(cue)* | **45%** *(layer)* | **UST-0** | Sinelon in **White** *(its own)*, over Blue |

The Red reaches only Confetti's heads while it plays; the cue's Blue is the base
everywhere else.

### 3.2 Tracking across three cues (Tracking list)

```
0.0.3-Announcer  Track   AT-Spots: Full, White, Aim Announcer, Follow Spot   [Release]
                         BackStage (TrussW-W + TrussC-RGB): Blue, 45%
0.0.4-SHOUT      Track   StepTops: White, Base
0.0.5-WHO ARE WE Track   StepTops: Dimmer 45%
```

| In cue | AT-Spots | BackStage | StepTops |
|---|---|---|---|
| 0.0.3 | Full, White, Announcer, Follow Spot *(own)* | Blue, 45% *(own)* | — |
| 0.0.4 | released → base (Base, White, Spots-0) | Blue, 45% *tracked from 0.0.3* | White, Base *(own)* |
| 0.0.5 | base | Blue, 45% *tracked from 0.0.3* | White *tracked from 0.0.4*, **45%** *(own)* |

0.0.5 holds only one value. Today it is a collection running all of 0.0.4 plus
a second StepTops scene; here it is "StepTops dimmer 45%", and going *darker*
works as well as brighter.

### 3.3 A block cue

Make 0.0.4 **Block**: it starts from nothing, so BackStage is dark in 0.0.4 and
StepTops is the only thing on. 0.0.5 tracks from 0.0.4 as before. Block is the
"hard start" for the top of a song (1.0.0-PreCurtain), where nothing from the
preshow should leak in.

### 3.4 Cue-only, and an include

- 0.0.6-STOMP **Cue-only** with FlashWashes (AT-W-ALL: White, Full): the flash
  plays in 0.0.6; 0.0.7 starts from 0.0.5's output, so the washes are back to
  what they were without a Release or a "washes off" cue.
- 0.0.9-AnnouncerFinish **Block, includes 0.0.3**: the whole announcer look
  back, as it was in 0.0.3 (its BackStage included). Add one layer —
  AT-Spots Dimmer 45% — and that's the cue. Change 0.0.3 later and 0.0.9
  follows. (Today: a collection holding the 0.0.3 collection.)

### 3.5 Child layers

A layer's children target part of its target (a member of its set, a
sub-group): `Steps: White, Base` with a child `US: Red`. US gets Red and
inherits Base; CS and DS keep White. This is Branson's "just under is global to
the entire FG".

---

## 3A. Beam & gobo

**Beam** is one attribute family in the tree with seven sub-attributes —
**Gobo, Gobo rotation, Prism, Zoom, Focus, Iris, Frost** — shown as child rows
under Beam in the layer editor (like an effect's own colour). Each is a key of
its own in the cascade: set, inherited, tracked and released separately.

- **Gobo palettes are named, not numbered.** "Breakup", "Dots", "Open" — the
  palette stores, per fixture definition, which wheel and slot that means
  (Focus Spot Three Z: wheel 1 slot 4; LED Movinghead+Circle: slot 6). A new
  fixture type gets its slot picked once in the palette editor.
- **Mixed targets.** A layer on a group or set that mixes fixtures with and
  without the capability applies the look only to those that have it; the
  rest ignore it (no error, no DMX). The value shows **"applies to 4 of 8
  fixtures"** with a hover listing which group has it (AT-All = AT-Spots ×4
  Focus Spot ✓, AT-W-ALL ×4 XL-450 ✗). Same for zoom, prism, etc.
- **An effect can own a gobo.** A beam effect (DanceHysteria's "Gobo Chase")
  holds its own child Gobo look, which applies inside the effect only; the
  layer's Gobo stays underneath (D5's "effect's own looks" rule, unchanged).
- **Tracking, worked through** (01-Everybody Scream):

| Cue | Towers Gobo | Towers Zoom |
|---|---|---|
| 1.0.0 | **Open** *(own)* | **Wide** *(own)* |
| 1.0.1 | **Breakup** *(own)* | Wide *tracked from 1.0.0* |
| 1.0.2 | Breakup *tracked from 1.0.1* | **Narrow** *(own)* |

  The tracking sheet has a **Show: Summary / Aim / Spread / Gobo / Zoom /
  Colour** switch, so one sub-attribute can be read across the whole list.
- **Engine:** one palette per sub-attribute value (as today's gobo/beam
  palettes), written into the generated scene in canonical stack order. No
  new engine work beyond what Beam palettes need for P1 attributes.

## 3B. Targets & lines in the tree

The Library has a **Targets** section: points (Announcer), areas (Main
Drape), lines ("DS-Edge" along the downstage edge, "Drape" along the main
drape) — drawn in the Lighting Studio per LINE_TARGET_DESIGN.md, or added
here with + Target. A layer's (or a cue's) Position can be **Aim at
<target>**. Aimed at a **line**, the aim look has three parameters:

| Parameter | Values | Default |
|---|---|---|
| **Spread** | −5 … +5, step 0.1. 0 = all on the line's centre; +5 = the full even spread, ends included; negative = the same distances crossed through the centre | +5 |
| **Grouping** | Each light / Pairs / Threes / Halves | Each light |
| **Cross** | Groups swap sides (pairs move as blocks: `5 6 3 4 1 2` at −5) / Each group flips in place (mirror: `2 1 4 3 6 5`) | Groups swap sides |

Distribution is the line design's: centripetal Catmull-Rom through the line's
points, even arc-length slots, lights ordered by hang position along the line.

**Cascade rules**

1. **Parameter-level inheritance (D16).** Position, Spread, Grouping and Cross
   are four keys. A layer can inherit *Aim at DS-Edge* from the cue (or from
   an earlier cue) and set only its own Spread; the editor shows each with its
   origin: *"Aim at DS-Edge — tracked from 1.0.0"*, *"Spread +5 — from Cue
   1.0.1"*, *"Grouping Pairs — tracked from 1.0.0"*, or *"Cross swap sides —
   default"*.
2. **Cue-level aim at a line = one formation (D17).** The union of the cue's
   own layers' fixtures is spread along the line as one formation, numbered by
   hang position along the line across the union. 1.0.5-Finale aims at
   DS-Edge, Halves, +5: Towers (6) and AT-Spots (4) become one row of ten,
   `T1 T1 AT1 T2 AT2 AT3 T3 AT4 T4 T4` from end A to end B. USTs sets its own
   Position (UST-0), so it is left out; a layer that set its own Spread would
   leave too and spread on its own along the same line. Baked like D10: one
   extra generated scene with one Aim look over the union's fixtures.
3. **Cue-level Spread / Grouping / Cross reach only line aims (D18).**
4. **Tracking.** The four keys track independently. A formation made at cue
   level tracks as one formation until a later cue changes some of its
   members (they then split off). **Spread changes fade over the cue's fade
   time** (D19), so on GO the beams sweep along the line; the plan view draws
   the previous cue's points as hollow rings.

**Worked example** (01-Everybody Scream, Towers = 6 movers on T1–T4):

| Cue | Sets | Towers resolve to | Order along DS-Edge |
|---|---|---|---|
| 1.0.0 PreCurtain | Towers: Aim at DS-Edge, Spread 0, Pairs | aim *own*, 0 *own*, pairs *own* | `1+2+3+4+5+6` (all at centre) |
| 1.0.1 Verse | cue look Spread +5 | aim *tracked from 1.0.0*, **+5 from Cue 1.0.1** | `1 2 3 4 5 6` |
| 1.0.2 Build | Towers: Spread −5, Cross swap | aim *tracked*, **−5 own**, pairs *tracked* | `5 6 3 4 1 2` (pairs cross as blocks) |
| 1.0.3.2 Sweep (sub-cue) | Towers: Spread +5, Cross mirror | **+5 from sub-cue** | `1 2 3 4 5 6` |
| 1.0.4 Outro | — | −5 *tracked from 1.0.2* (sub-cues are cue-only) | `5 6 3 4 1 2` |

**UI.** The cue and layer inspectors show a **plan view** (from above,
audience at the bottom): the curve with its end/mid handles, each fixture's
hang position as a numbered dot, and a beam line to its computed point for the
current Spread/Grouping/Cross; dragging the **Spread** slider redraws it live.
Under it, the order strip (`A ▸ 5 6 3 4 1 2 ◂ B`) and the formation's members.

**Phase.** After the line-target P1 ships (line target + even distribution),
and its P2/P3 (Spread slider, Grouping/Cross) — in the Show Tree alongside
**P3** (group sets, cue-level effects), which already brings the union rule.

## 3C. Sub-cues in the tree

A cue's children are its **layers and its sub-cues** (4.1, 4.2…, numbered per
SUBCUES_DESIGN.md: `<cue>.<n>` unless a step has its own number; a main cue
numbered 4.1 wins). Each sub-cue has its own layers and cue looks, and its own
**fade**, **wait** and **Auto-follow / Sub GO** (the step's `SubCues` setting).
+ Sub-cue adds one; the tree badges show "Sub GO" or "auto 0.5s".

- **Tracking (D9, D25):** 4.1 starts from cue 4's output, 4.2 from 4.1's. A
  value set in 4.1 shows in 4.2 as *"from sub-cue 4.1"*. Sub-cue changes are
  **cue-only**: cue 5 starts from cue 4's own state, however far Sub GO got.
- **Tracking sheet:** sub-cue columns sit right after their cue, shaded and
  labelled `↳ .1 Hit / Sub GO`, `↳ .2 Sweep / auto 0.5s`.
- **Example** (1.0.3-Chorus): .1 *Hit* (Sub GO — the first plays with GO):
  BackWash White Full; .2 *Sweep* (Auto-follow 0.5 s, fade 1.5 s): Towers
  Spread +5, mirror; .3 *Strobe* (Sub GO): Towers shutter Strobe fast. GO to
  1.0.4 then shows Towers back at −5 and BackWash back to Blue.
- **Engine:** a cue with sub-cues bakes to a Collection whose step function is
  a generated inner Chaser of sub-cue Collections, `SubCues="SubGo"` (per
  SUBCUES P1–P3, shipped). Auto-follow sub-cues get the inner step's own
  timing; the runner and footer behave exactly as SUBCUES §2–3 say.

---

## 4. Mapping onto the engine (baked)

| Tree | Engine object | Notes |
|---|---|---|
| Production | a function folder `Productions/<name>/`, hidden from the Functions tab by default | plus a `<Production>` element for its settings |
| Cue list | **Chaser** (cue list) | its steps carry cue number, note, fade/hold — what the Chaser already has |
| Cue | **Collection** = one step | holds the cue's complete output: its own layers *and* everything tracked/included |
| Layer (resolved) | generated **Scene**: targets + palette refs in canonical stack order | one per distinct resolved layer, **shared** by every cue whose resolved layer is identical |
| Inline look | QLCPalette with no name in the production's hidden palette folder | |
| Effect + child looks | Effect palette followed by its child palettes in the scene's list | exactly today's "what belongs to an effect" rule |
| Cue-level effect (D10) | one extra generated scene over the union of the cue's layer targets | |
| Beam sub-attribute | its palette (gobo, zoom, …) in the layer's scene | gobo palettes map per fixture definition; fixtures without it skip it |
| Line aim + Spread/Grouping/Cross | an inline Aim palette per distinct (target, spread, grouping, cross) | the params are the line design's `AimParam`s |
| Cue-level line formation (D17) | one extra generated scene: one Aim look over the union of the formation's fixtures | same as a cue-level effect |
| Sub-cues | the cue's step function is a Chaser of sub-cue Collections, `SubCues="SubGo"` | per SUBCUES_DESIGN.md |
| Opaque function layer | the function itself, in the cue's Collection | |
| Cue number | `ChaserStep::cueNumber` | so show control fires it (SHOW_CONTROL_DESIGN.md) |

Generated names follow the cue list's own pattern, learned from its siblings
(e.g. `0.0.4-SHOUT` for the collection, `0.0.4-SHOUT · StepTops` for a layer
scene), filed under `Productions/DanceHysteria/Full Show/`. Generated functions
carry `Generated="ShowTree"` and their node id.

**Why baked complete cues work well on this engine**

- Every Collection is the cue's full state, so **jumping to any cue** (show
  control, timecode, a click) gives the tracked look with no replay.
- A layer that tracks through cues 3–7 is **the same Scene** in five
  Collections. `Function::stop()` only stops a function when its last parent
  lets go, so if the next cue starts it before the old cue lets go, it **keeps
  running**: no re-fade, effects keep their phase. *Needs a P1 spike*:
  `ChaserRunner` stops the old step and starts the new one in the same tick;
  if the stop lands first, a small hand-over change (start new, then stop old)
  is the only engine change P1 needs.

  **Spike result (2026-10-16, `engine/test/cuehandover`):** it did *not*
  work as it was. On GO the old cue's Collection was stopped first; its
  `postRun()` let go of the shared Scene before the new Collection's
  `preRun()` (which runs from the MasterTimer's start queue, *after* the stop
  loop of the same tick) could claim it, so the Scene stopped and started
  again: `stopped` emitted, `elapsed()` back to 0, a fresh fade-in from zero,
  and the MasterTimer's `functionStopped`/`functionStarted` — what the
  EffectScriptRunner tears effect instances down and builds them up on — for
  it. Swapping the order inside `ChaserRunner::write()` alone would not have
  fixed it, for the same reason: the new Collection only claims its members
  at the end of the tick. **The fix (in `ChaserRunner`, as pre-approved):**
  on a step change (GO, Back, a jump, auto-follow) a leaving step whose
  function is a Collection is held until the new step has started; if the
  two Collections share a member, the old one is stopped on the *next*
  write (one tick, 20 ms, later), when the new Collection already holds the
  shared Scene, so `Function::stop()` only removes one of two parents. A
  leaving Collection that shares nothing stops at once, as before. "Shares"
  counts nesting: a cue that *is* a member of the next one (DanceHysteria's
  0.0.5 = Collection of the 0.0.4 Collection + StepTops-Bright) hands over
  too, so today's collections-of-collections stop re-fading on GO as a side
  benefit. Scene,
  EFX and chase steps take exactly the old path. Also in `Collection::postRun()`:
  a member another parent still runs gets this Collection's intensity
  override released (else a cue played at 50 % would hold the shared Scene at
  half for as long as it ran on). With the fix the spike shows: no
  `stopped`/`running` signal, `elapsed()` strictly increasing every tick, the
  shared channel at 255 on every tick through GO, Back, a jump and an
  auto-follow, and the old cue's own scenes stopping. All chaser,
  chaserrunner, collection, sub-cues, show, showrunner, cuestack and show
  control tests unchanged and green. Cost: the old cue's own scenes start
  their fade-out one tick later when it shares something with the new cue.
- A changed layer is a new scene crossfading against the old one: colour LTP,
  intensity HTP — identical dimmers don't dip.
- VC, footer GO, timecode, sub-cues, move-in-black look-ahead, show control and
  the stock Function Manager all see ordinary functions. A file opened in a
  build without the Show Tree still plays.

**What re-bakes, and what doesn't**

| Edit | Re-bake |
|---|---|
| A palette's value (colour, aim, effect params) | **none** — scenes reference the palette, and `requestPaletteRefresh()` already updates running ones |
| A group's members / head layout | **none** for whole-group targets (dynamic) |
| A look added/removed/changed on a layer or cue | that cue, then each following cue until a Block, plus every cue that includes any of them |
| Tracking mode, Release, includes, layer order | the same |
| A group set's members, a reusable layer/cue | every cue using it, and their followers |
| A child layer or an overlap (§3.5) | the parent's scene needs the children's fixtures **excluded** → see below |

A re-bake is cheap (look lists, no DMX) and runs on edit, debounced; scenes no
longer referenced are deleted, running ones are swapped via
`requestPaletteRefresh()`/`resetRuntime()` as the Programming tab does.

**What breaks or needs care with baking**

- Hand edits to a generated scene would be lost → locked (D13).
- Overlap and child layers on part of a group need "this group *minus* these
  fixtures". Without it, intensity HTP lets the parent's level show through.
  **One small engine addition: `Scene::excludedFixtures`** (generated scenes
  only, honoured in the fader-map build under `m_valueListMutex`). Until then
  (P1), child layers are limited to whole members of a set.
- Function count: one scene per *distinct* layer state, not per cue × layer;
  DanceHysteria's preshow would be ~25 scenes for 10 cues.
- Delete/usage dialogs must name cues, not generated scene names.
- Undo is per tree edit (re-bake is deterministic from the tree).

## 5. Resolving at playback instead (later, optional)

A `CueListRunner` in the engine computes each cue's per-channel state on GO
from the tree. Gains: true per-channel tracking, **move fades** (only channels
that change fade), no generated functions, no exclusion tricks. Costs:
re-implementing what the Chaser gives for free (VC cue list, sub-cues, show
control, timecode, beat-quantised GO, move-in-black look-ahead) and a file the
stock engine can't play. Worth it only if baked tracking hits a wall — the
likely one is part-of-group overlap at scale or wanting move fades.

---

## 6. UI

```
┌ Show Tree ──────────────────────────────────────────────────────────────────┐
│ Production: [DanceHysteria ▾]  [Corporate Gig]   + Production     ▶ GO 0.0.4 │
├───────────────────────┬──────────────────────────────────────┬──────────────┤
│ ▾ Full Show (Tracking)│ Cue 0.0.5-WHO ARE WE                 │ Palettes     │
│   ▸ 0.0.0-base  ■base │ Mode [List·Track] [Block] [Cue-only] │ Groups       │
│   ▸ 0.0.3-Announcer   │ Includes: —            Fade 2s Hold ∞│ Group sets   │
│   ▾ 0.0.4-SHOUT       │ Cue looks: —                         │ Reusable     │
│       StepTops        │ ─ Layers ─────────────────────────── │  layers/cues │
│       ⟲ BackStage ⤳   │ StepTops   White ·tracked 0.0.4      │              │
│   ▾ 0.0.5-WHO ARE WE  │            45%                       │ (drag onto   │
│       StepTops        │ BackStage  Blue 45% ·tracked 0.0.3   │  a cue, a    │
│   ▸ 0.0.6-STOMP  ◐    │ ─ Next cue receives ──────────────── │  layer or an │
│ ▸ 01-Everybody Scream │ StepTops, BackStage                  │  attribute)  │
├───────────────────────┴──────────────────────────────────────┴──────────────┤
│ Look Editor (today's, for the selected attribute)                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **Left**: the tree; cues show their mode (■ base, ⊘ block, ◐ cue-only) and
  tracked layers under a cue in grey with ⤳. Drag a cue to reorder; drag a
  group/set onto a cue to add a layer; drag a cue onto a cue to include it.
- **Centre**: the selected node. A cue: mode, includes, timing, its looks, all
  layers (own and tracked), and **Next cue receives**. A layer: one row per
  attribute — own value, or the inherited one greyed with its source; click a
  greyed row to override it here.
- **Right**: the Library panel (§6A).
- **Bottom**: the existing inline Look Editor, unchanged.
- **Header**: Author / Inspect / Run (§6B), Undo, an optional command line
  (§6C).
- Later: a **tracking sheet** (cues × layers grid) for reading a whole list.

A clickable mockup of this view is published alongside this doc: the tree with
inherited values and their source, Track/Block/Cue-only, Release, an include,
Make reusable/Detach, Beam rows and gobo palettes, line aims with a live plan
view, sub-cues, the tracking sheet, the Library panel, real authoring (drag &
drop, pickers, + Cue / + Layer, the command line, a guided "Show me"
walkthrough with action counts) and the cue runner (Run tab + VC widget
preview).

## 6A. Library panel

The right panel is **everything that can be dropped**, split so the short
lists come first (D20):

| Section | Holds | Why |
|---|---|---|
| **Recent** | the last ~10 things set, dragged or typed, newest first — including one-off values ("Green-ish · 1.0.1 Towers") with **Make reusable** | what you just used is what you use next |
| **In this cue list** | palettes, groups, sets and targets already used in the selected cue list, most-used first, with counts (Towers ×6, Full ×3…) | a song mostly reuses its own vocabulary |
| **Pinned** | anything starred | the house White/Base/Open |
| **All** | everything, by kind (Groups 56, Group sets, Targets, Colour/Dimmer/Position/Gobo/Beam/Shutter/Effect palettes, Reusable layers), your folders shown one level deep, collapsed | finding a thing the first time |

One **filter box** at the top narrows all four. Every item is draggable onto a
cue (cue look / new layer), a layer, or an effect row, and the drop hint says
what it will do before you let go.

**How things get here** (a help card in the panel, and its hovers): groups
from the Fixture Manager; targets from the Lighting Studio (or + Target);
palettes from **+ Palette** or **Make reusable** on a one-off; group sets
from ticking groups in All → Groups → **Make set**; reusable layers and cues
from **Make reusable** in the tree. With DanceHysteria's real library (56
groups, ~60 palettes, 28 effects in 10 folders) All is long; Recent and In
this cue list stay around ten items each.

## 6B. Cue runner

One runner component, two homes (D26): the full-screen **Run** tab, and a
**Cue Runner** VC widget (compact or list size). Both show the same state.

- **Header:** production · cue list (switchable), **timecode** readout (the
  production's source, e.g. MTC from QLab), and **LIVE / DESIGN** (⌘F12).
- **Cue list:** each cue and its sub-cues (indented) with its state — *done*,
  **▶ running** (with a progress bar over its fade), **▷ STANDBY** (loaded),
  **NEXT**, **SUB NEXT** — its timing (fade; for sub-cues Sub GO or auto
  + wait) and its note.
- **What changes:** NEXT (and STANDBY, SUB NEXT) carry one line computed from
  the resolved tracking — only the attributes that differ between the stage
  now and that cue: *"Towers → Red, aim spread +5, gobo Breakup; BackStage in
  → Green, Intensity 45%"*. Unchanged attributes (Towers' zoom) aren't listed.
- **Buttons:** **GO** (⌘↩), **Sub GO** (⌥⌘↩), **Sub Back** (⌥⌘⇧↩), **Back**
  (⌘⇧↩), **Pause/Resume**, **Stop** (⌘⇧Esc stops all, as today). QLab, OSC
  and MSC send the same commands (`/go`, `/subgo`, `/subback`, BACK, STOP,
  LOAD); the footer GO drives the same runner.
- **Rules** (SUBCUES §2, D27): GO plays the loaded cue, else the next; into a
  cue with sub-cues it plays the first sub-cue and holds; Auto-follow
  sub-cues fire on their own after their wait (Pause holds the timer and the
  fade); GO from a sub-cue goes to the next *main* cue; Back = previous main
  cue; click = LOAD; double-click = jump (parent first for a sub-cue).
- **Stage now:** a strip of small swatches, one per layer, its resolved
  colour faded by its dimmer, updating on GO / Sub GO.
- **VC widget:** *compact* (on now, next, big GO + Sub GO) and *list* (the
  whole runner); several widgets may follow different lists or productions;
  the old VC Cue List widget stays for plain chasers, and **Swap widget**
  replaces one with the other on a page.

## 6C. Workflow end to end

**Authoring operations** — every one is a single undo step and is shared by
the mouse, drag & drop, the command line and the walkthrough:

| Operation | Mouse | Command line |
|---|---|---|
| New cue list | + Cue list, type name | `list "01-Everybody Scream"` |
| New cue (auto-numbered, D23; tracks by default) | + Cue (after the selected), type name in place | `cue 1.0.1 "Verse"` · `cue + "Verse"` |
| Mode / base | Track · Block · Cue-only; Base cue | `block base` · `cueonly` |
| Add layers | drag a group/set/target onto a cue; or + Layer → tick several → one layer each, or **Make one set layer** | `Towers AT-Spots Steps` |
| Set a look | click a value → picker (searchable, **recent first**), or a one-off swatch / % slider; or drag a palette onto a layer or onto the cue's looks (the hint says *"Blue → cue look: reaches 3 of 3 layers"*) | `Towers colour Blue dimmer 45` · `cue look colour Red` · `Steps colour #33aa55` |
| Effect + its own look | drop the effect on a layer, then a colour/gobo on the effect row | `Steps effect Sinelon colour Red` |
| Aim along a line | drop a target on a layer/cue; Spread slider, Grouping, Cross | `AT-Spots aim DS-Edge spread 0 group pairs` |
| Change a tracked value | edit it in this cue (brings the layer in) **or Update cue X** where it was set (D22) | — |
| Copy / move | Duplicate cue; drag a layer to another cue (⌥ copies); **Record as new cue** | `dup` · `record` |
| Sub-cue | + Sub-cue; fade / wait / Auto-follow · Sub GO | `sub + "Hit"` |

**Effort, measured.** The same three cues — 1.0.0 *base* (Block: White, Base,
Open on Towers, AT-Spots, Steps), 1.0.1 *Verse* (tracks; Towers Blue +
Sinelon in Red; AT-Spots aimed at DS-Edge, Spread 0), 1.0.2 *Wide* (Spread
+5) — built three ways. The Show Tree counts come from the mockup's
walkthrough and command script (run and checked in node: both produce the
identical resolved show); today's from walking through the code
(`programmingmanager.cpp`, `scenegrouplooks.cpp:1054-1125`, `lookeditor.cpp`,
`chasereditor.cpp`).

| | Programming tab today | Show Tree · mouse | Show Tree · command line |
|---|---|---|---|
| New list | (chaser, below) | 2 actions (1 click + name) | 1 command, 26 keys |
| 1.0.0 | — | 12 (8 clicks, 3 drags, name) | 3 commands, 74 keys |
| 1.0.1 | — | 11 (3 clicks, 7 drags, name) | 3 commands, 68 keys |
| 1.0.2 | — | 4 (2 clicks, 1 drag, name) | 2 commands, 28 keys |
| **Total** | **≈165 actions** (150–180) | **29 actions** + 36 keys of names | **9 commands, 196 keys** (252 without Tab) |
| …if palettes & the target already exist | ≈107 | 29 | 9 |
| Objects you create | ≈19: 8 palettes, 4 scenes, 2 collections, 1 chaser, 1 extra stage target (for the spread variant — Aim has no Spread today), 3–4 folders | 1 list, 3 cues, 6 layers; scenes, collections, chaser generated | same |
| Places you go | 6: Palettes tab, Look Editor, functions tree, scene canvas, Collection/Chaser editors, Lighting Studio (+ 2 dialogs) | 2: tree/inspector, Library | 1 (+ the tree to look) |
| What you must remember | open a scene before dropping into it; drop Red on the effect row; which scene sets which group and how they stack in a collection (is Base's White or Towers' Blue on top?); set the chaser's holds (default 0); make the variant target first; rename every "New X N" | Track vs Block; cue look vs layer look; drop on the effect row for its own colour | the grammar (hint line) and names (Tab) |

**Where the tree is NOT simpler.** Building the Library the first time costs
what it costs today (palettes in the Look Editor, targets in the Lighting
Studio) — the ≈58-action difference between 165 and 107 is all Library. A
group set (Towers) has to be made once. Tracking has to be understood before
a greyed, tracked value makes sense. A one-off look that is one scene today is
still a cue + a layer here. And the command line is not *fewer keystrokes*
than the mouse is actions — it wins on staying in one place, not on count.
Genuinely easy today and kept: palettes file themselves by type; Duplicate
auto-increments; multi-select drags; edits are live with no commit step.

**Intuitiveness review** — what a first-time user stumbles on, and the answer:

| Stumble | What the UI does |
|---|---|
| "Why is Towers still blue? I didn't set it here." (tracking) | Greyed value with *"tracked from 1.0.1"*; the tree marks it ⤳ tracked; the list's mode and each cue's mode badge say Track/Block; hover help on every badge; Block is one click |
| Cue look vs layer look | The drop hint says where it lands and **what it reaches** ("reaches 2 of 3 layers (1 sets its own)") before release; values show "from Cue 1.0.1" |
| Where does a one-off colour go? | Into the layer as an unnamed palette, and into Recent; reuse offers a name. No folder to choose |
| Changing a tracked value — here or where it came from? | The picker asks: *Set in this cue* / *Update cue 1.0.0* |
| A gobo that "doesn't work" on half the rig | "applies to 4 of 8 fixtures", with which group lacks it |
| Spread numbers | Live plan view + order strip while dragging the slider; defaults match "evenly spread" |
| Sub-cues vs cues | Indented under their cue everywhere (tree, sheet, runner) with Sub GO / auto badges; the hint says "cue-only: 1.0.4 starts from 1.0.3's own state" |
| Fear of breaking it | Every action is one undo; Inspect mode is read-only; the Run tab has nothing to bump |

## 7. Migration / import

Nothing changes in an existing show until it's imported. **Import chaser as
cue list…** (right-click a chaser in Programming or the Function Manager):

| Chaser content | Becomes |
|---|---|
| Step → Collection | a Cue (number from `cueNumber`, else parsed from a `0.0.4-SHOUT` name, else its position; note and timing kept) |
| Collection → Scene with targets + palettes | a Layer: target = its groups/fixtures, looks = its palettes by attribute |
| Collection → Collection | an **include** (0.0.5 includes 0.0.4) |
| Collection → Chaser | sub-cues |
| Scene with raw values, EFX, RGB Matrix, Show | an opaque function layer (the original, referenced) |
| Scene used in many cues | offered as one reusable layer |

The new list is **Non-tracking** (every cue Block), so it plays as the chaser
did — except where two included scenes fought over HTP intensity, which the
importer lists. **Convert to tracking…** then removes values that equal what
would track in, and turns "includes the previous cue" into plain tracking.
Originals stay; VC buttons are offered a switch to the new list.

## 8. Phases

| Phase | Slice | Engine change |
|---|---|---|
| **P1** | **Show Tree tab, one production, Non-tracking lists.** Cue list = Chaser, cue = Collection, layer = generated Scene. Cue looks cascade to layers; layer attribute rows with inherited values greyed — **including the Beam family with gobo palettes per fixture definition and "applies to N of M"** (§3A); inline looks; effects with child looks (colour, gobo); opaque function layers; **Import chaser as cue list**; authoring by mouse (+ Cue / + Layer, picker, drag & drop with drop hints, undo); Library panel Recent / Pinned / All. Spike: shared-scene hand-over across a step change. | none, or the hand-over fix |
| P2 | **Tracking**: list mode, cue Track/Block/Cue-only, layer Release, base cue, **includes**, origin marks, "Next cue receives", re-bake chain; "Update cue X" for tracked values; Library "In this cue list"; **optional command line** (D24). | none |
| P3 | **Group sets** (+ Make set in the Library), child layers, overlap; cue-level effects across layers; **line aims in the tree** (§3B: aim params per key, cue-level formations, plan view) once the line-target P1–P3 have shipped. | `Scene::excludedFixtures` |
| P4 | **Make reusable / Detach** for layers and cues; Reusable panel; Convert-to-tracking. | none |
| P5 | **Multiple productions**: switcher, current production for VC / footer GO / show control, per-production timecode. | small: "current production" in Doc |
| P6 | Sub-cues in the tree (§3C); tracking sheet with sub-cue columns and the attribute switch. | none (SUBCUES) |
| P7 | **Cue runner** (§6B): Run tab, then the Cue Runner VC widget (compact / list) with Swap widget; "what changes" line. | none (drives ChaserRunner as the footer does) |
| later | Engine-resolved cue lists, move fades (§5) — only if baking hits a wall. | large |

P1 alone removes the triple filing: a song is one branch of one tree, and the
scenes and collections file themselves.

## 9. Not in scope

Replacing the Programming tab; channel-level tracking in P1–P6; a separate
palette scope per production (palettes are shared; inline looks cover
one-offs); cue-list-level looks (D14).

---

## 10. Implementation notes (P1 build, 2026-10-16 overnight)

Decisions taken to build P1. Where the choice was not obvious the
conservative option was taken and is listed under **Open for Branson** at
the end.

### 10.1 Where the code lives

| Piece | File | |
|---|---|---|
| Tree model, cascade, numbering, XML | `engine/src/showtreemodel.{h,cpp}` (namespace `ShowTree`) | pure values, no Doc: the §3 tables run on it directly |
| Bake, edits, undo, import, inline palettes | `engine/src/showtreecontroller.{h,cpp}` (`ShowTreeController`) | owned by Doc; `Doc::showTree()` + load/save/clear forwarders only |
| Hand-over | `ChaserRunner`, `Collection` (§4 spike) | |
| Tests | `engine/test/cuehandover`, `engine/test/showtree` | offscreen, no DMX out |

### 10.2 File format

```xml
<Engine>
  … fixtures, groups, palettes, functions as today …
  <ShowTree NextId="42">
    <Production ID="1" Name="DanceHysteria">
      <CueList ID="2" Name="Full Show" Mode="NonTracking" Base="5" Chaser="120">
        <Cue ID="3" Number="0.0.3" Name="Announcer" Note="…" Mode="Block"
             FadeIn="1260" FadeOut="0" Hold="4294967294" Includes="7" Collection="121">
          <Look Key="Colour" Palettes="4"/>                      <!-- a cue look -->
          <Layer ID="4" Groups="19" Release="1">
            <Look Key="Dimmer" Palettes="2"/>
            <Look Key="Effect.Position" Palettes="50" Own="1"/>  <!-- an effect and its own looks -->
            <Look Key="Beam.Gobo" None="1"/>                     <!-- off here -->
          </Layer>
          <Layer ID="6" Function="33"/>                          <!-- an opaque function layer -->
        </Cue>
      </CueList>
    </Production>
    <Generated Scenes="122,123" Collections="121" Chasers="120"/>
    <InlinePalettes IDs="88"/>
  </ShowTree>
</Engine>
```

- **Where:** one `<ShowTree>` element in `<Engine>`, after the functions;
  written only when the show has a tree, so a show without one saves
  byte-for-byte as before (tested).
- **Node ids:** one counter per show (`NextId`) for productions, lists, cues
  and layers — unique across the whole tree, stable across saves, separate
  from function and palette ids. On load the counter is raised above every
  id found, whatever the file says.
- **Attribute keys** (`Look Key`): `Colour` (an ordered list: colour 1, 2…),
  `Dimmer`, `Position` (Pan/Tilt/PanTilt/Aim), `Beam.Gobo`, `Beam.Zoom`,
  `Beam.Beam` (today's Beam palette: focus/frost/iris together), `Shutter`
  (Shutter and Strobe), `Effect.<kind>` with kind `Color`/`Dimmer`/
  `Position`/`Beam`/`Other` from the script's fixture types
  (`EffectScriptCache::categoryForTypes`). Prism and Gobo rotation have no
  palette type yet, so they are not keys yet.
- **`Generated` mark — a deviation from §4.** The design put
  `Generated="ShowTree"` and the node id on each `<Function>`. Built instead:
  the tree lists what it generated (`<Generated …/>`) and
  `ShowTreeController::isGenerated()` / `describeGenerated()` answer for any
  function ("Cue 0.0.4 SHOUT in Full Show"). Reason: no change to
  `function.cpp`'s common XML (an upstream file every function type goes
  through), and an older build drops the whole `<ShowTree>` either way.
  Generated scenes are found again by *what they play* (target + palette
  list), so they need no node id.
- **`Productions/` folder:** generated scenes and collections get the stock
  `Hidden="True"` flag (the Function Manager and the Programming trees already
  skip hidden functions) and the path `Productions/<production>/<list>`; the
  cue list's Chaser is **visible**, at `Productions/<production>`, so a VC Cue
  List, the footer GO and show control can pick it as they pick any chaser.
- **Inline (unnamed) palettes** (D11, D21) are ordinary `<Palette>` elements,
  named for where they were made ("30% · 1.0.1 Towers"), path
  `Palettes/Show Tree`, and listed in `<InlinePalettes>`; the palette trees
  must hide them (UI step). One no cue uses any more is deleted at the next
  bake. **Make reusable** names and files it and drops it from the list.
- **An older build** (or upstream QLC+) logs "Unknown engine tag: ShowTree",
  skips it, and plays the generated Chaser / Collections / Scenes as
  ordinary functions — the show runs. Saving there loses the tree; the
  generated functions stay (hidden scenes and collections, a visible chaser),
  and inline palettes show as normal palettes in a "Show Tree" folder.
- **No bake on load:** the saved functions are what plays; the next edit
  bakes. A bake straight after load changes nothing (tested).

### 10.3 Baking

- `resolve(list)` gives each cue's resolved layers (§2: tracked → includes →
  cue looks → layer looks; Release to the base cue, else dark; Cue-only
  passes on what it started from). Tracking, Block, Cue-only, Release, base
  and includes are all in the resolver already — P1's UI only has to expose
  them; P2 is mostly UI now.
- A resolved layer bakes to a Scene: its groups/fixtures + palettes in the
  look-stack order (Colour, Dimmer, Position, Gobo, Zoom, Beam, Shutter, then
  each effect followed by its own Colours/Dimmers). Identical (target,
  palettes) = **the same Scene** in every cue (shared; hand-over keeps it
  running). A layer that resolves to nothing (only None) bakes to no scene.
- Cue → Collection (`<number>-<name>`, the DanceHysteria pattern), list →
  Chaser (PerStep fades/holds, SingleShot; step = fade in/out, hold, note,
  `cueNumber`). An infinite hold stays the duration (as
  `ChaserStep::loadXML`).
- Deterministic: baking twice changes nothing (tested on the function XML).

### 10.4 Re-bake safety

- **Synchronous, on the GUI thread, once per edit.** No debounce: palette
  value edits (the Look Editor's sliders) never re-bake — scenes reference
  palettes — so a bake only follows a discrete tree edit, and it is look lists
  only (no DMX).
- **A generated scene's content never changes:** an edit that changes a
  layer bakes a *different* scene (a new signature) and the cue's Collection
  is pointed at it. Nothing the DMX thread reads in a running scene's fader
  map is mutated by a bake, so there is no new `m_valueListMutex` exposure;
  the bake only renames scenes, edits Collection member lists and Chaser steps
  (in place with `replaceStep`, so a running list keeps its step) under their
  own locks.
- **Orphans** (scenes, collections, chasers nothing uses any more) are deleted
  only when not running; a running one is left for a later bake, so an edit
  never pulls a look out from under the show (tested).
- **Limitation:** an edit to the cue that is *on stage* shows the next time
  that cue plays — the running Collection keeps the members it started with.
  Safe; whether to re-take the cue live is open (below).

### 10.5 Undo

- `ShowTreeController::edit(label, change)` is the one door: snapshot (the
  tree's XML plus every inline palette's full XML), apply, bake. Undo/redo
  restore a snapshot and bake (200 steps). Every operation the UI offers is
  one step; an import is one step.
- Inline palettes come back with undo/redo exactly as they were, and so
  does a cue list's chaser with its playback settings (run order, speed
  modes, GO on the beat) — an undo snapshot carries both (tested: redo of an
  import).
- **Not shared with Programming** in P1: the Programming tab has its own
  Revert snapshots and edits functions, not the tree.
- **Palette value edits are not tree edits**, so tree undo doesn't undo a
  colour dragged in the Look Editor (as today in Programming).

### 10.6 Import (§7)

- New production named after the chaser; Non-tracking list; every step a
  cue: number from the step's `cueNumber`, else parsed from the function's
  name (`0.0.4-SHOUT` → `0.0.4` "SHOUT"), else its position; note, fades and
  hold kept.
- In a step's Collection: a Scene of groups/fixtures + palettes → a **layer**;
  a Collection that is another step's → an **include**; anything else (raw
  channel values, own fade times, per-look fades, a transition, a blend mode,
  a speed master, a group-scoped look, two effects of one kind, effects in an
  order the tree doesn't keep, EFX, RGB Matrix, a Show, a Chaser…) → an
  **opaque function layer** playing the original, with the reason in the
  report. A scene's static looks may be reordered (each is the only one of
  its kind; an Aim/Gobo is the look's wherever it sits), but colours keep
  their numbering and effects their order with their own looks
  (`playsTheSame()`).
- The generated chaser starts as a **copy** of the original (run order, speed
  modes, tempo, GO on the beat), its steps replaced by the bake.
- **HTP conflicts:** a cue where the same target is set by two sources (an
  include and its own scene, or two scenes) — the chaser ran both (HTP
  intensity, LTP the rest), the list keeps the later one's values per
  attribute. Listed in the report.
- Not carried yet (noted in the report): a step's skip flag, its own speed
  master, Sub GO steps (P6).

**DanceHysteria-0.5 dry run** (a copy in the scratchpad, in memory only;
`QLC_SHOWTREE_IMPORT=<copy> showtree_test importARealShow`):

| Chaser | Cues | Layers | Opaque | Includes | Generated scenes | HTP conflicts |
|---|---|---|---|---|---|---|
| Full Show | 6 | 9 | 0 | 3 (0.0.5 ⊃ 0.0.4, 0.0.6 ⊃ 0.0.5, 0.0.7 ⊃ 0.0.6) | 7 | 1: 0.0.5 WHO ARE WE — StepTops set by 0.0.4's StepTops-Dim and its own StepTops-Bright (Bright is the brighter, so nothing changes on stage) |
| New Chaser 8 | 2 | 3 | 2 (Wash-Red, Show: scenes with no looks) | 0 | 2 | 0 |

Played side by side, cue by cue, every channel of every universe (static
looks; effects run on their own timer and were not compared): **0 channels
differ** in all 8 cues (6 146 lit in 0.0.2, 282 in 0.0.3, 1 696 in 0.0.4/0.0.5,
6 152 in 0.0.6/0.0.7). Note 0.0.4–0.0.7 have Hold 0, so they auto-follow as
one sequence and back to 0.0.2 (Loop) — the import keeps that.

### 10.7 Gobo mapping (§3A)

- **P1 as built:** today's Gobo palette is used unchanged (one DMX value on
  every fixture's gobo channel), and **"applies to N of M"** counts the
  target's fixtures that have the attribute's channels
  (`ShowTreeController::lookReach`, tested on a Sharpy + dimmers).
- **Approach for the per-definition mapping (both, manual wins):** a gobo
  palette stores a *name* ("Breakup"); per fixture definition it resolves by
  (1) a manual map kept in the palette (manufacturer/model → wheel value, set
  once in the palette editor for a new type), else (2) a name match against
  the definition's Gobo capabilities (capability name or gobo resource file
  name, case- and space-insensitive). No match: not applied, and counted out
  of "applies to". Not built tonight.
- **An effect's own Gobo:** today's stack rule says a Gobo is never an
  effect's, wherever it sits. P1 supports an effect's own Colour/Dimmer only;
  a Gobo of its own needs the engine rule changed (open).

### Open for Branson

1. **`Generated` mark on `<Function>`** as well as in `<ShowTree>`? (10.2;
   built without, to leave function.cpp alone.)
2. **Editing the cue on stage:** show the change at once (re-take the cue
   through the hand-over, or teach Collection to sync its running members) or
   on its next GO (built: next GO)?
3. **Undo shared with Programming?** (built: the tree's own.) And should tree
   undo cover palette value edits made from the tree's Look Editor?
4. **Gobo mapping:** manual map + name match as in 10.7 — agree before it is
   built? And an effect's own Gobo (engine rule change)?
5. **Hold 0 in imported lists:** DanceHysteria's 0.0.4–0.0.7 auto-follow
   (Hold 0). Intended? The import keeps it either way.
