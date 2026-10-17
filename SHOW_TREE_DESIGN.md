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
| D11 | **One-off values** | Must be a named palette / can be inline | **Inline allowed**: an unnamed palette owned by the layer, hidden from the palette tree. **Make reusable** names it and files it. This is what stops `Palettes/Color/01-Everybody Scream`. **Changed 2026-10-17 (Branson: "reuse, so we can adjust globally by updating the palette"):** a value typed or picked first **reuses a named palette playing exactly it** (In this cue list → Recent → any); only when none does is a one-off made (picker) or a **named palette per the show's own pattern** (command line). (§10.11) | ok |
| D12 | **Import of existing shows** | Convert in place / import as a copy | **Import a chaser as a cue list, as a new production; originals untouched.** Imported list starts **Non-tracking**, so it plays exactly as before; **Convert to tracking…** then offers to strip what repeats. (§7) |  ok |
| D13 | **Hand-editing a generated scene** | Allowed (overwritten on re-bake) / locked | **Locked**, with **Detach to plain scene** (the tree then points at that scene as an opaque function layer). | ok |
| D14 | **Cue-list-level looks** | Yes / no | **No.** A base cue + tracking does that job and shows up in the tree as a cue. |ok  |
| D15 | **Beam family** | Gobo as its own attribute / Beam with sub-attributes | **Beam with sub-attributes** Gobo, Gobo rotation, Prism, Zoom, Focus, Iris, Frost — **each cascades and tracks on its own** (a cue changes Gobo while Zoom tracks). Gobo palettes are *named* ("Breakup", "Dots", "Open") and map per fixture definition to that type's wheel slot; fixtures without the capability ignore it, and the editor says **"applies to 4 of 8 fixtures"**. (§3A) | ok  |
| D16 | **Which look parameters inherit one by one** | Every palette parameter / only aim parameters + Beam sub-attributes | **Only the line-aim parameters (Spread, Grouping, Cross) and the Beam sub-attributes.** They are what changes cue to cue on the same look. An effect stays one value with its child looks (D10): changing an effect's speed per cue = another effect palette, or later a speed master. Revisit if effect parameters turn out to be cued as often. | ok  |
| D17 | **Cue-level aim at a line** | Each layer spreads on its own / one formation | **One formation over the union of the cue's own layers' fixtures** (as D10 for effects). Fixtures are numbered **by hang position along the line across the whole union** (the line design's Q3), not group by group, so a Towers + AT-Spots formation interleaves. A layer that sets its own Position — or its own Spread/Grouping/Cross — is left out and spreads on its own. A tracked formation stays one formation until a later cue changes some of its members. (§3B) | ok |
| D18 | **Cue-level Spread / Grouping / Cross** | Reach every layer / only layers aiming at a line | **Only layers whose resolved Position is a line aim.** "Spread +5" on a cue then means "spread whatever aims at a line here", and a wash layer doesn't silently carry a meaningless Spread forward. |ok  |
| D19 | **A spread change between cues** | Snap / fade | **Fade over the cue's fade time** (pan/tilt fade, so the beams sweep — straight in pan/tilt space, close to but not exactly along the line, as the line design's Q8). The plan view shows where the beams were in the previous cue. | ok |
| D20 | **Library panel** | One tree of everything / sections | **Recent · In this cue list · Pinned · All** (with one filter box over all four). What a song uses is in the first two; All is for finding a thing the first time. (§6A) | ok |
| D21 | **One-off values** (extends D11) | — | Set inline from the picker (a colour swatch, a % slider) → an **unnamed palette owned by the layer**, shown at once in **Recent** as "Green-ish · 1.0.1 Towers". Dropping it on a second layer reuses it and **offers to name it** (Make reusable); unnamed it never appears in All. **Changed 2026-10-17:** the picker first reuses an exact named palette; a one-off's row has **Make reusable** right by it, one click, named per the show's pattern ("Intensity 31%"). (§10.11) | ok |
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
| Cue-level effect (D10) | one extra generated scene over the union of the cue's layer targets | *as built (§10.13): each member's own scene plays it, with a layout span over the union* |
| Beam sub-attribute | its palette (gobo, zoom, …) in the layer's scene | gobo palettes map per fixture definition; fixtures without it skip it |
| Line aim + Spread/Grouping/Cross | an inline Aim palette per distinct (target, spread, grouping, cross) | the params are the line design's `AimParam`s |
| Cue-level line formation (D17) | one extra generated scene: one Aim look over the union of the formation's fixtures | same as a cue-level effect *(as built: a layout span, §10.13)* |
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
| Add layers | drag a group/set/target onto a cue (several: **one layer**; ⌥: one each); or + Layer with several selected → one layer (⌥-click or its menu: one each). *Changed 2026-10-17: one rule everywhere, §10.11* | `Towers AT-Spots Steps` (one layer) · `Towers, AT-Spots, Steps` (one each) |
| Set a look | click a value → picker (searchable, **recent first**), or a swatch / % slider (reuses a palette playing exactly that, else a one-off); or drag a palette onto a layer or onto the cue's looks (the hint says *"Blue → cue look: reaches 3 of 3 layers"*) | `Towers colour Blue dimmer 45` · `cue look colour Red` · `Steps colour #33aa55` (reused, or a new named palette) |
| Effect + its own look | drop the effect on a layer, then a colour/gobo on the effect row | `Steps effect Sinelon colour Red` |
| Aim along a line | drop a target on a layer/cue; Spread slider, Grouping, Cross | `AT-Spots aim DS-Edge spread 0 group pairs` |
| Change a tracked value | edit it in this cue (brings the layer in) **or Update cue X** where it was set (D22) | — |
| Copy / move | Duplicate cue; drag a layer to another cue (⌥ copies); **Record as new cue** (asks which stage when they differ, §10.11) | `dup` · `record [selected\|running\|lit]` |
| Split / merge layers | right-click → Split layer (one per group) · select several → Merge layers | — |
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
| Where does a one-off colour go? | A palette already playing it is reused; else into the layer as an unnamed palette, and into Recent, with **Make reusable** (one click, named and filed as the show's) on its row. No folder to choose |
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
| P2 *(built 2026-10-17, §10.10)* | **Tracking**: list mode, cue Track/Block/Cue-only, layer Release, base cue, **includes**, origin marks, "Next cue receives", re-bake chain; "Update cue X" for tracked values; Library "In this cue list"; **optional command line** (D24). | none |
| P3 *(built 2026-10-17, §10.13)* | **Group sets** (+ Make set in the Library), child layers, overlap; cue-level effects across layers; **line aims in the tree** (§3B: aim params per key, cue-level formations, plan view) once the line-target P1–P3 have shipped. | `Scene::excludedFixtures` (+ a per-look layout span) |
| P4 | **Make reusable / Detach** for layers and cues; Reusable panel; Convert-to-tracking. | none |
| P5 | **Multiple productions**: switcher, current production for VC / footer GO / show control, per-production timecode. | small: "current production" in Doc |
| P6 | Sub-cues in the tree (§3C); tracking sheet with sub-cue columns and the attribute switch. | none (SUBCUES) |
| P7 | **Cue runner** (§6B): Run tab, then the Cue Runner VC widget (compact / list) with Swap widget; "what changes" line. | none (drives ChaserRunner as the footer does) |
| later | Engine-resolved cue lists, move fades (§5) — only if baking hits a wall. | large |
| later | **Plain-language command line** (Branson, 2026-10-17): an offline phrase layer first (synonyms and filler words onto the existing grammar — "make the towers blue at half"; deterministic, always there at a show), then an optional online layer (the prompt and the show's names to Claude, which returns ordinary command lines, shown for confirmation, one undo step each; API key in Settings; not for show time). TODO.md. | none |

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
          <Layer ID="8" Groups="19,20" ContinuesGroups="19"/>    <!-- two groups; carries on from the tracked 19 -->
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
- **The hand-over reads Collection members on the DMX thread**
  (`ChaserRunner::sharesMembers`), so `Collection::components()` and
  `contains()` now read the member list under its lock (a copy), as
  `functions()` already did.
- **Orphans** (scenes, collections, chasers nothing uses any more) are deleted
  only when not running; a running one is left for a later bake, so an edit
  never pulls a look out from under the show (tested).
- **An edit to the cue on stage** (resolved 2026-10-17, §10.8): in
  **Design** mode the selected cue plays as a preview and every bake re-takes
  it, so an edit shows at once — also while its list runs for real (the
  preview takes precedence, §10.9). In **Live** (Operate) it shows the next
  time the cue plays (GO): the running Collection keeps the members it
  started with — editing live is blind.

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
  of "applies to". **Built 2026-10-17** (§10.8); the name match made
  looser, in tiers, the same day (§10.9).
- **An effect's own Gobo:** today's stack rule says a Gobo is never an
  effect's, wherever it sits. P1 supports an effect's own Colour/Dimmer only;
  a Gobo of its own needs the engine rule changed (open).

### 10.8 Fix pass (2026-10-17)

Branson tried P1; four fixes, all approved.

- **Preview (Design mode).** `ShowTreeController::previewCue()/stopPreview()`;
  the view calls it on selection (a cue, or a layer / tracked layer in one)
  while the tab is on screen, and stops it on leaving the tab, selecting a
  list, a production or nothing. The preview runs the cue Collection's
  **members** (generated scenes, opaque functions), not the Collection, under
  its own parent (`FunctionParent(AutoVCWidget, 0xFFFFFF53)`):
  - a scene two previewed cues share is never let go of, so moving the
    preview is the hand-over (no re-fade, effects keep their phase);
  - after every bake the preview is synced as a start/stop diff (start new
    members first, then stop the ones gone) — no stop/start of what stays,
    so none of the MasterTimer start/stop race Programming's
    `refreshPreview()` warns about. Bakes still never change a running
    scene's content (10.4), so `m_valueListMutex` exposure is unchanged;
  - stopping removes only that parent: a chaser's collection, a VC button or
    Programming holding the same scene keeps it (a Master parent would clear
    them all).
  Scenes the preview let go of are still running for a tick, so a bake 250 ms
  later sweeps them (a QTimer, stopped on reset/load).
  ~~Not previewed while its own list runs for real (`ListRunning`)~~ —
  **changed 2026-10-17 (§10.9): in Design the edit wins**, over its own list
  or any other. **Live (Operate):** state `NotDesign`, nothing plays;
  into Design, the app has stopped everything, so the preview starts afresh.
  Blind is the global output mute and covers the preview; the status-bar
  power estimate reads what is lit in Design mode on any tab, so it covers
  the preview with no change.
- **Several groups in one layer.** A drop of several groups (and fixtures)
  on a cue is one layer; ⌥ a layer per group; groups on a layer join its
  target; "Remove X from layer" (never the last). `setLayerTarget()` keeps
  the layer one layer: if in that cue it took a carried-in layer's place,
  it remembers that target (`Layer.continues`, XML `ContinuesGroups` /
  `ContinuesFixtures`) and the resolver lets it take that layer's place and
  values under the new target; the same layer set again in the cues it
  tracks into (an own layer that took the tracked one's place) is
  retargeted in the same undo step. A Block cue's own layer is a fresh layer
  and is left alone. Tests: two groups track across cues in any order;
  a group added to a tracked layer; added where the layer starts.
- **Library groups in folders** — FixtureGroup `path`, nested as in the
  Fixture Manager; open folders remembered by path; the filter matches a
  group at any depth or a group folder by name, and clearing it restores
  the folders' open state from before it was typed.
- **Gobo by name** (10.7). `QLCPalette::goboName()` (XML `GoboName`) and
  `goboValues()` (per "Manufacturer/Model", XML `<GoboValue Fixture Value>`);
  `goboValueFor(fixture)`: picked slot → the gobo capability of that name on
  the channel the look writes to (the first gobo wheel), its middle value;
  a picture counts as `Folder/file` (`ClayPaky/gobo00006`: Chauvet's own
  `gobo00006` is other glass, so the file name alone is not enough) → else
  not applied. Names compare ignoring case, spaces, `-` and `_`; exact
  otherwise (so "Dots" never picked "Dots Shake"; made looser, in tiers,
  by the follow-up, §10.9). **An unnamed gobo look
  plays its one value everywhere, as before** — existing shows unchanged
  until the look is edited. The Look Editor names the look from the first
  pick (or from a change of the same type's pick), keeps every pick per type,
  has a Gobo name field and a per-type "what it plays" list. The
  programmer's back-derive of a named gobo sets the slot for that fixture's
  type. `lookReach(target, look)` counts a gobo by name per fixture.
  Not done: matching a swapped glass (MonitorProperties `goboSwap`, a
  per-unit property not safe to read on the DMX path), a second gobo wheel,
  and an effect's own Gobo (out of scope).

### 10.9 Follow-up (2026-10-17): Branson's answers to 6 and 7

**Preview rule — "In Design, whatever is in edit should be visible. In
Perform, the cue list overrides. Blind we talk about later."**

- **Design:** the cue being edited (selected cue, or a layer in it) is
  always on the rig, also while its own list or any other runs for real.
  `ListRunning` is gone; the header adds "over <list> running"
  (`ShowTreeController::listsRunning()`). The real list keeps going
  underneath; stopping the preview (deselect, select a list, leave the tab,
  switch mode) leaves the stage to it — its current cue as it started
  (Live edits land on GO). Nothing a chaser or VC widget started is stopped
  (the preview still only removes its own parent).
- **Precedence — the mechanism.** Plain stacking can't do it: a dimmer is
  HTP, so a running cue at full hides an edit down (Programming's preview,
  checked, is plain stacking too and has the same limit). So the preview's
  **scenes** are marked `Scene::setEditOverride(true)`:
  - their faders go to `Universe::Override` priority (after every Auto
    fader in `processFaders`), and every channel carries
    `FadeChannel::Override`, which writes LTP (`Universe::write(…, forceLTP)`):
    the edited value is what goes out on the channels the cue sets; the
    others stay what runs underneath;
  - the flag is atomic; the DMX thread applies it at the next `Scene::write`
    (`applyEditOverride`: `requestFaderPriority` + `GenericFader::
    setChannelsFlag`, under `m_valueListMutex`), so a scene the running cue
    and the preview share is moved in place, never restarted. A re-aim /
    repalette keeps the flag on the channels it replaces;
  - let go of, the flag is cleared *before* the stop: a scene the real list
    still holds goes back to Auto/HTP in place; one that stops fades out
    HTP (`handleFadersEnd` drops `Override` first), so an intensity fades
    into what runs underneath instead of being forced to 0, and LTP
    channels fade to the value under them (`SetTarget`; the fader keeps its
    Override place for the fade);
  - opaque functions in the cue (EFX, RGB matrix, scripts) mix as usual
    (Effect palettes already run at Override in `EffectScriptRunner`).
  - Engine fixes on the way: `Universe::requestFaderPriority()` never
    lowered a fader's priority (it searched with the fader still in the
    list and only set the priority when it found a place) — now re-inserted
    as `requestFader()` would (`universe_test::faderPriorityMovesBothWays`);
    `GenericFader::write()`'s `Override` path wrote only one byte of a
    16-bit channel — now every byte, MSB first (no existing user had
    16-bit Override channels).
- **Perform (Live):** unchanged — the running list owns the stage, no
  preview (`NotDesign`), edits on GO.
- **Blind:** unchanged (global output mute, covers the preview) — **to
  discuss**.
- Tests: `showtree_test::inDesignTheEditWinsOverARunningList` (edit down
  under a running cue at full → 127 not 255; stop → 255; another cue on
  top; the list starting mid-preview keeps it), `showtreeview_test::
  inDesignTheEditShowsOverItsRunningList` (Design + own list running → edit
  visible, header; stop by selecting the list / leaving the tab → running
  cue back; Perform → no preview, blind until GO).

**Gobo name matching — looser, in tiers** (`QLCPalette::matchGoboName`;
`resolveGobo()` gives value + how + candidates; `goboValueFor()` is its
value):

1. the slot **picked** for the type (unchanged, first);
2. **exact**, ignoring case, spaces, `-`, `_` (a slot's name, or its picture
   `Folder/file`);
3. **singular/plural**, the word "gobo" and numbers ignored ("Dot" = "Dots"
   = "Gobo 3 Dots"; a trailing plural `s` dropped, not `ss`);
4. a **unique prefix or contains** (3+ letters; slot names only).

The first step that finds anything decides. Several *different* names in it
→ **ambiguous**: not guessed, that type ignores the look; the row reads
"applies to N of M — ambiguous: pick per type…" (link → Look Editor;
tooltip lists "Model: A / B" per type, `lookAmbiguities()`), and the Look
Editor's per-type list says "ambiguous (A / B)". The same name on several
slots of one wheel (an "Open", a glass repeated in its shake range — 206 of
968 gobo channels in the bundled definitions do this) is one gobo, its
first slot, so exact matching behaves as before. Pictures match exactly
only, and a wanted picture name (with a `/`) tries nothing looser. Known
edge: step 3 ignores numbers, so "Gobo 3 Shake" can find a wheel's only
"Gobo 2 Shake" (with several, ambiguous). Table test:
`qlcpalette_test::goboNamesMatchInTiers`; Look Editor and row in
`showtreeview_test::goboLooksMatchByNamePerType`.

### 10.10 P2 (2026-10-17): tracking in the tab, D22/D23, the command line

Most of the cascade was in P1's resolver (10.3); P2 exposes it and fills
the gaps.

- **Dependencies and loops (D8).** `ShowTree::dependsOn(list, a, b)`: cue
  *a* needs *b* at any depth — it tracks from the cue before (Track,
  Cue-only), includes, or releases a layer to the base (not from a
  Cue-only cue). `canInclude()` refuses self, twice and any include whose
  cue already depends on the includer; `addInclude/removeInclude/
  moveInclude` use it and `setCue()` drops such includes. The resolver
  also leaves out a looping include or base (base → dark) **before**
  resolving it: P1 only caught a cue still on the stack, so an include of
  a later cue that tracks from this one gave that later cue an empty start
  and kept it (a latent bug; the UI could make it). A loop in a file is now
  left out on both sides, whichever cue resolves first (it was: the first
  one read won).
- **Re-bake chain.** `ShowTree::rebakeChain(list, cue)`: the cue; the
  following cues while they start from it (up to a Block; a Cue-only cue
  passes on only what it started from, so a change *in* it stops there,
  one *before* it goes through); every includer; and when one of them is
  the base, the cue after each release to it — to a fixed point. The bake
  itself stays whole and deterministic (cheap, 10.4); it now records the
  cues whose Collection it changed (`lastBakeChanged()`), and the tests
  check the two agree on §3 edits (the chain is a superset: a cue in it
  whose layer sets the value itself doesn't change). The tab shows
  "Changed: …" after each edit.
- **Update cue X (D22).** `valueSource()` walks a value back — Tracked and
  Base by `fromCue`, Included into the included cue, and on through
  that cue's own origin — to the cue whose layer (or cue look) sets it;
  `updateSource()` sets it there, one step. A cue look there is changed as
  a cue look (it reaches that cue's own layers that don't set it), as the
  mockup's `opUpdateSource`. The ⋯ menu then says **Set in this cue** /
  **Update cue 1.0.0 base**.
- **Record as new cue (D22, D23).** The selected cue's resolved stage
  (tracked, included, own; layers playing nothing left out) as a **Block**
  cue after it, every layer its own with its played looks, timing copied,
  "<name> (recorded)", numbered per D23. It plays the same generated
  scenes (same target + palettes). "What's on stage" is the selected cue
  — what Design previews — not a mix of lists running for real (open, 9).
  **Duplicate** copies a cue's own content, never the base flag.
- **Release on a carried-in layer** (`setTargetRelease`) makes it the cue's
  own first, as setting a value does.
- **Library "In this cue list" (D20):** `usageInList()` counts per layer
  (groups, fixtures, opaque functions) and per look (palettes, an effect's
  own); one-off values included (reusing one on a second layer is what
  "Make reusable" is offered for). Sorted by count, then groups before
  values, then name; only shown with a list selected.
- **Command line (D24, §6C).** `engine/src/showtreecommand.{h,cpp}`: a
  word reader (names with spaces matched longest-first, quoted; ~~`+`
  joins two into one layer~~ — names in a row are one layer, `,` starts
  another since §10.11) calling the controller. Grammar per the mockup
  minus what later phases bring (`sub`, `aim`/`spread`/`group`/`cross`):
  `cue <n> "name"`, `cue + "name"`, `cue look …`, `track|block|cueonly|
  default|base`, `<groups> <attr> <value>…`, `effect X colour Y` (its own),
  `none`/`clear`, a level `45`/`45%` or `#rrggbb` → ~~one-off value~~
  a reused or new named palette (§10.11),
  `release`, `include` (toggles), `record`, `dup`, `undo`, `redo`,
  `list "name"` (a Tracking list, as the mockup's). **One undo step per
  command:** `beginBatch()/endBatch(keep)` — edits inside apply at once
  (later words see earlier ones) but bake and snapshot once; a failure
  puts the tree back and the bake drops a one-off value made on the way.
  Tab completes the last 1–3 words from what fits there (keywords and
  groups first; palettes of the attribute just typed; cue numbers after
  `cue`/`include`). Hidden by default; **Command** shows it (kept in the
  settings).
- **Tracked preview:** unchanged mechanism — a cue's Collection is its full
  tracked state, so previewing cue 5 plays everything tracked into it, and
  an Update cue X re-takes it (tested).
- Tests: `showtree_test::theRebakeChain`, `aBakeChangesOnlyTheChain`,
  `includesAreCycleProof`, `updateCueWhereATrackedValueWasSet`,
  `recordAsNewCueFreezesTheStage`, `duplicateACue`, `usageInAList`,
  `previewShowsTheTrackedState`, **`convertAnImportToTracking`** (a
  DanceHysteria-shaped chaser imported, then converted by hand — Tracking,
  base, Release, repeated layers and includes removed — plays §3.2 (going
  *darker* works), §3.4 (cue-only flash gone in the next cue; 0.0.9
  includes 0.0.3) and §3.3 (0.0.4 Block; the bake changes exactly 0.0.4 →
  0.0.7) on the rig), `commandLine`, `commandLineFailsAsAWhole`,
  `commandLineCompletes`; `showtreeview_test` ×8 for the tab.

### 10.11 P2 answers (2026-10-17): Record which stage, one layer or each, reused values

Branson answered open questions 9–11.

**Record — "subjective; should prompt to ask how to handle it, and/or have
config options that default this in the settings."**

- `ShowTreeController::RecordSource` **Selected** (the selected cue's
  resolved stage, as before), **Running** (`runningCue(list)`: the cue the
  selected cue's list's chaser is on, matched by its step's Collection),
  **Lit** (every list running for real in tree order, the Design preview on
  top — later wins attribute by attribute on the same target — then every
  other running Scene / EFX / RGB Matrix not generated by the tree as an
  opaque function layer; chasers, collections and shows are skipped, their
  steps are what is lit). Programmer / Simple Desk values are not captured.
- `recordChoices(cue)`: the sources that are available, put something on
  stage, and differ from an earlier one (by targets + played looks). Zero or
  one → no question. `recordCue(cue, source)` puts the new Block cue after
  the selected cue in every case, named "<cue> (recorded)" (Running: the
  running cue's name and timing; Lit: "on stage (recorded)"); one undo step
  labelled with the source.
- Settings: `showtree/recordDesign`, `showtree/recordPerform` =
  `ask|selected|running|lit` (default `ask`), in **Settings → Show Tree**
  (`AppSettings`, the dialog the app opens). A default naming something
  that can't be recorded now (running, nothing runs) asks instead. The
  dialog (radio per choice with its layer count; preselected Selected in
  Design, Running in Perform) has **Don't ask again in <mode>**, which
  writes that mode's key. The command line's bare `record` goes through the
  same chooser (`ShowTreeCommand::setRecordChooser`); `record selected|
  running|lit` overrides it (refused with a reason when that isn't on stage).

**Several groups — "need both behaviours, and they must be consistent."**

- One rule: **several = one layer by default; one each is explicit.**
  Drop (⌥ = each, unchanged), **+ Layer** (several selected: one layer;
  ⌥-click or the button's menu: one each), command line (`Towers USTs` and
  `Towers+USTs`: one layer; `Towers, USTs`: one each — `release` follows
  it too). Hint line, Tab completion (after `,`/`+` it completes the next
  group) and help updated; the drop hint already said which.
- **Split layer (one per group)** — `splitLayer()`: a piece per group and
  one on the fixtures (`Layer::pieces()`), each a copy of the looks and
  release; the first keeps the layer's id. Tracking identity:
  - a piece **carries on** from what the layer carried on from (the tracked
    or included layer it took over, its `continues`): each piece whose
    units overlap it. The resolver now lets **several own layers of one
    cue carry on from the same carried-in layer** (the first takes it out
    of the stack, the others start from the same copy);
  - onwards, as `setLayerTarget()`: where the same layer is set again
    (own, took over that target) it is split there too; a layer carrying on
    from it under another target carries on from the pieces.
- **Merge layers** — `mergeLayers()`: own, non-opaque layers of one cue →
  one layer on the union, at the first one's place and id. **The safe
  behaviour: refuse, with the reason, never guess** — when two set an
  attribute differently ("Towers sets Colour to Blue, USTs to Red"), or one
  is released and another isn't. The merged layer carries on from
  everything they carried on from: new **`Layer::continuesAlso`** (XML
  `<Continues Groups Fixtures/>` children of `<Layer>`; an older build
  skips them) — the first stays `continues`, the extras fill in values the
  first doesn't have. Onwards: set again together → merged there too (same
  conflict rule, "in cue 3 …"); carried on from under another target → from
  the merge.
- **The guard for both:** after the edit, `ShowTree::stageDiffers(before,
  after)` compares every cue's **stage** (`stageOf`: per group / fixture /
  opaque function and attribute, the winning value — LTP, later layer
  wins — or for Dimmer the set of levels, HTP). Any difference → the edit
  is refused (no undo step) with "would change what cue 0.0.5 plays on USTs
  (Dimmer)". This is what makes "only one of them is set later" (it would
  light twice, HTP) a refusal rather than a silent change. Units are
  groups and fixtures as named (no Doc), so a group's fixtures named
  separately count as another unit.

**`dimmer 45` — "reuse, so we can adjust globally by updating the palette."**

- `findPalette(type, values, list, recent)`: named, non-inline, flat
  (no fanning) palettes playing exactly the value — Dimmer: the same DMX
  value (45 % = 115, as the command and picker round); Colour: the same RGB
  and W/A/UV (missing = black). Order: used in this cue list (most uses,
  then lowest id) → Recent (newest) → any (name, then id). Dimmer and
  Colour only (what can be typed / picked).
- **Command line:** none → `createNamedPalette()`: named by
  `suggestPaletteName()`, filed by `suggestPaletteFolder()` (the type's
  most common folder, trailing `/` ignored; tie → shorter path), made as an
  inline palette in the batch and made reusable only when the command
  succeeds (a failed command leaves nothing). Undo removes the value from
  the cue; the palette stays (it is an ordinary palette now). The message
  says *reused "Intensity 45%"* / *new palette "Intensity 30%" in
  Palettes/Dimmer*.
- **Naming pattern:** a sibling (named palette of that type) whose name
  holds its own value as a whole token — Dimmer its level in %, Colour its
  hex with or without `#` — gives a template (`Intensity {}%`); the most
  common template wins, ties to the oldest. None: Dimmer `Dimmer 30%`;
  Colour the colour's name if it is exactly a named colour ("Red"), else the
  nearest name and the hex ("Sea Green #33aa55"). A name the type already
  has gets " (2)". DanceHysteria: `Intensity 45%` (115) in
  `Palettes/Dimmer` → `dimmer 30` = **Intensity 30%** in Palettes/Dimmer.
- **Picker** (⋯ → Level… / Colour…, `setOneOffDimmer/Colour`): reuses an
  exact palette (and notes it in Recent); else the D21 one-off, whose row
  now has **Make reusable** beside the value: one click,
  `makeReusable(pid)` names and files it by the same pattern. Library →
  Make reusable… pre-fills that name and folder.
- One-offs are never reused by the matcher (they are not in All).

Tests: `showtree_test::recordTheRunningCueOrEverythingLit`,
`splitALayerKeepsTracking`, `mergeLayersKeepsTracking` (both across four
tracked cues, a re-set cue, refusals, XML round trip),
`aTypedValueReusesAPalette` (preference order; editing the reused palette
changes both cues on the rig), `aNewPaletteIsNamedAsTheShowsAre`, the
command-line tests updated for the new grammar; `showtreeview_test::
recordAsksWhenTheChoicesDiffer` (setting, Don't ask again per mode, the
command line, the dialog), `severalGroupsOneLayerOrEach` (+ Layer, hints,
split, merge from a tree multi-selection), `aPickedValueReusesAPalette`.

### 10.12 Gaps closed (2026-10-17): multi-select, D13 lock, one list rule, one undo step

Four limits the help listed, found while documenting it.

- **Merge by mouse.** The tree is `ExtendedSelection`: ⌘-click toggles a
  row, ⇧-click takes a range. Everything that acts on one row (the page,
  the Design preview, + Cue, Record, the context menu's single-row items)
  follows the *current* row, the one clicked last; only Merge and Delete
  read the selection. **Merge** is a header button and the context menu's
  item, enabled when `selectedLayers()` is 2+ own layers of one cue (a
  ⤳ row or another cue's layer empties it). Delete with several rows:
  the cues and own layers in one batch step; a list or production in the
  selection is refused with a message (they are deleted alone, as they
  ask first). Several cues dragged together are refused (hint: *"Drag one
  cue at a time"*); layers never drag. A rebuild keeps the rest of a
  multi-selection. Rows selected from code use `ClearAndSelect`
  explicitly: with `ExtendedSelection` and no event Qt reads the held
  modifiers, so a held ⌘ (⌘Z) would *toggle* the row — found by the test.
- **D13 enforced.** `ShowTreeController::lockedText()` ("Made by the Show
  Tree — edit it there.") and `generatedNode(fid)` (list for a chaser, cue
  for a collection, first cue and own layer for a scene).
  `ShowTreeView::lockedBanner()` (that text, what it plays, **Show in Show
  Tree** → `reveal()`: the tab forward, the node selected; for a layer's
  scene **Detach to plain scene**) and `refuseGenerated()` (a message, the
  same words, Show in Show Tree). Wired into: the Functions window (the
  chaser's editor locked; a collection/scene shows only the bar; Delete
  refused, also of a folder holding one), the functions tree (no in-place
  rename, not moved to another folder, a folder delete skips them), the
  Programming tab (editor locked; delete, add to collection, take out of a
  chaser/collection refused), Shows' chaser editor and a VC widget's live
  edit (locked). **`ChaserEditor::setLocked(banner)`** locks the steps and
  the name (add/remove/raise/lower/shuffle/cut/paste, drops, in-place
  edits, the skip menu, Renumber, the speed dial) and keeps what §10.2 says
  is the user's: run order, direction, the fade/hold modes, the cue-list
  number and main cue list, testing. Running and playing are untouched.
  **Detach to plain chaser** is not built (open): a chaser's steps are the
  tree's cues.
- **One list rule.** `addCueList()` defaults to **Tracking**, and + Cue
  list passes it explicitly, as `list "name"` always did; `importChaser()`
  makes its list Non-tracking (D12). The existing tests that relied on the
  old default now say `ListMode::NonTracking`.
- **One undo step per action (D22), audited.** Every view action and
  command was walked: gaps were **Make reusable** (no step at all),
  **several functions dropped** (a step each), **a colour and a dimmer
  dropped together** (a step each) and **Detach** (undo left the plain
  scene behind). Now: `makeReusable()` pushes a step (`pushUndo`, shared
  with `edit()`/`endBatch()`), and a list version makes a row's one-offs
  reusable in one; the snapshot carries palettes made reusable here
  (`<Reusable>`, so redo names them again; the inline restore now keeps the
  path too) and scenes Detach made (`<Detached>`: restoring a snapshot
  from before a detach deletes its scene once the bake has let go of it;
  redo reloads it as it was). `dropFunctions()` and a multi-kind
  `dropPalettes()` run in a batch. A one-off no cue uses is no longer
  deleted while any scene (not only a generated one) plays it. Still not
  tree undo, by design: Look Editor value edits, and a palette a command
  made (it stays, §10.11).
- Tests: `showtree_test::aNewListIsTrackingAnImportedOneIsNot`,
  `makeReusableIsOneUndoStep`, `detachIsOneUndoStep`,
  `generatedFunctionsAreFoundInTheTree`; `showtreeview_test::
  mergeLayersBySelectingThem` (real ⌘/⇧ clicks, the context menu, a
  refusal), `severalRowsSelectedDeleteAndDrag`, `aNewCueListIsTracking`,
  `generatedFunctionsAreLockedElsewhere` (Functions window, Programming
  tab, live edit, Detach from the bar), **`everyActionIsOneUndoStep`**:
  47 actions (drops, pickers, page fields, context-menu items, Record,
  Delete, Detach, import, 15 commands), each checked that one undo gives
  back the state before (tree, palettes and one-off flags, the show's own
  functions) and redo the state after.

### 10.13 P3 (2026-10-17): group sets, child layers and overlap, cue-level effects, line aims

Merged `feat/line-targets` (P1–P3: LineCurve, LineSpread, the `__line*`
Aim params, the look editor's Spread row and dot strip) first.

**Engine: what a look leaves out, and what it lays out over.**

- `Scene::setExcludedFixtures(palette, fixtures)`: the fixtures a look
  doesn't write. Honoured where the fader map is built (`write()`, under
  `m_valueListMutex`; the exclusions are read under `m_bindingsMutex`, the
  documented lock order), in the re-aim/re-palette path, the fade-out map,
  `CueOutput`, `AimSolver::aimHeads(scene, palette)` and `EffectInstance`.
- `Scene::setLayoutSpan(palette, groups, fixtures)`: what a look lays out
  over when it is one with other scenes — a line numbers its heads over the
  span, an effect lays its cells over it (the multi-group stage layout, by
  where heads stand); the scene still writes only its own.
- Both per look, not per scene (the design said `Scene::excludedFixtures`):
  a child that sets only Colour must leave its parent's Dimmer and effect
  running on its fixtures. Saved as `<Exclude Palette Fixtures>` / `<Span
  Palette Groups Fixtures>` (an older build skips them), copied, part of a
  generated scene's identity (its signature). A generated scene never
  changes after it is made, so the DMX thread never sees one change under
  it; the setters still lock (tested from two threads).
- An effect still reads the scene's own looks as its base (level, colour,
  the look's aim) on fixtures it leaves out of a static look.

**The bake (§2 overlap, §3.5).** Per cue, attribute and fixture, the layer
whose value wins: **this cue over earlier ones** (a layer's own, then a cue
look, then an include, then tracked — a later cue's tracked value over an
earlier's), then the **deeper** layer (a child over its parent), then the
**lower** one. Every other layer covering that fixture leaves it out of that
attribute's palettes. None counts (it wins and lights nothing). A layer left
out everywhere plays no scene. Note "lower wins on what both set" is read as
*set*: a layer's own look beats a cue look reaching a lower layer (D5's
nearer scope), so a cue look never overrides a layer's own on an overlap.

**Overlap in imports.** The DanceHysteria-0.5 dry run found it: New Chaser
8's cue 1 runs two StepTops scenes (White and Red) whose RGB mixed
highest-wins — the tree's rule made it Red (1056 channels). So
`Cue::mixOverlap` (`Overlap="Mix"`): its layers mix as a collection does
(only a child leaves its parent out); the importer sets it where overlapping
scenes set the same attribute and says so in the report; the cue page's
check box turns it off. Dry run: 0 channels differ in all 8 cues.

**Group sets (§1).** `ShowTree::GroupSet` in `<ShowTree>` (`<GroupSet ID
Name Groups Fixtures>`), in the undo snapshot and rolled back with a failed
command. `Target::sets`: the set is the layer's identity (signature
`…|s:<ids>`, written only when there are sets, so old signatures are
unchanged); the bake expands it to its member groups as separate targets of
one scene. Deleting a set in use is refused (the cues named). ⌥-drop of a
set: a layer per member. Command: `set "Towers" SL SR`; a set by name as a
target. **Position, not group order (Branson):** a line formation is ordered
by `AimSolver::orderedLineHeads` (along the line, by where heads hang) over
the whole union; an effect over a span uses the multi-group stage grid (rig
positions). Test: two towers and the spots between them interleave by X.

**Child layers (§3.5).** `Layer::parent` (`Parent=`), `addChildLayer()`
only inside the parent (by fixtures; refusal says why), placed after the
parent's children. The resolver gives a child only its own looks (no cue
looks: they reach the parent, which lights the child's fixtures anyway),
`ResolvedLayer::depth` and `parent` (kept as it tracks). The tab shows what
the parent lights grey, "from Steps (parent)". Removing a parent removes its
children; a parent can't be retargeted past them; split/merge refuse.

**Cue-level effects (D10) — built as a shared layout span, not one extra
scene.** A cue look on an `Effect.*` key carries `ResolvedValue::span` (the
cue), kept as it tracks. The layers in a cue with the same look and span
are one effect: each member's own scene plays it with a layout span over
all their targets, so one Sinelon crosses them, and each keeps its own level
and colour under it. One extra scene (§4) would have lost that: an effect
reads its base from its own scene. They start in the same tick (one
Collection) so the layout's phase is shared; a later cue changing a member
re-makes the others' scenes (a new span), and their effect restarts.

**Line aims (§3B, D16–D19).** `Look::value` and keys `Position.Spread /
.Grouping / .Cross` (values `"-5".."5"`, `each|pairs|threes|fours|halves`,
`swap|mirror`), each cascading and tracking on its own. D18 in the resolver:
a `LineAimTest` (the controller's `resolved()`: the Aim palette's target is a
line) lets the cue's values reach only line aims. D17: a cue-level Position
is a span like an effect; the layers with the same aim, span and cue-level
parameters are one formation (any parameter of its own → on its own). A
layer whose parameters differ from its Aim palette's plays an **inline Aim
palette made for them** (copy of the base with the `__line*` params; found
again by target + params; hidden; dropped when unused; edits to the base
re-bake). D19 needs nothing: pan/tilt fade (tested mid-fade). Library →
Targets (`aimPaletteFor()`: a named Aim palette pointing there with nothing
of its own is reused, else "Aim <target>" is made — like a typed value, it
stays after undo). The order strip (`LineSpreadPreview` over the layer's
scene) numbers the whole formation. **Not built:** the top-view plan with
the previous cue's points as rings (D19's second half).

**Also (Branson, during P3).** *Rename in place* as Programming does it:
Return/F2 in the tree, Return commits, Esc cancels, empty = the automatic
name; cues (name, not number), lists, productions; `name "…"`;
`renameNode()` makes no step for the same name. *Copy/paste*: ⌘C/⌘V/⌘D on
the tree only (`Qt::WidgetShortcut`, so text fields keep theirs),
`pasteCues()` (a run after the selected cue, numbered by D23 in turn,
includes only within the list, child parents remapped) and `pasteLayers()`;
`copy` / `paste` typed. *`help`*: `ShowTreeCommand::words()` is the one
table — syntax, what, examples, related — that `keywords()` and
`attributeWords()` come from; `help`, `help <word>`, `?`, did-you-mean;
shown in a pane over the command line. A test reads every example through
the parser.

*Layout and adding (Branson's feedback during P3):* the columns are now
**Library | Tree | Editor** (the §6 sketch had the Library on the right),
with the Look Editor docked under the editor column only — the page and the
editor are worked on together, and the tree keeps its full height; the
command line stays full width. A drop below the tree's last rows goes to
the selected cue. Return / double-click in the Library acts on the tree's
selection as a drop would (the hint says what); ⌘F or / jumps to the
Library filter, Return adds the first match. A named layer shows only its
name in the tree (Branson: short; contents in the tooltip); the main window
says what it holds (a line under it on the cue page, the layer page's
"Contains:" with links and ✕); its name works on the command line, and the
command line's placeholder says what the selected layer holds. **Several
layers selected are edited together** (any cues): the page shows the shared
value or "mixed", a tri-state Release, sums "applies to"; every set goes
through one funnel (`applyLook`, `setEffectOwn`) that applies it to all of
them in one batch (one undo step); a right-click menu for them; the command
line takes an attribute first for the selection (`dimmer 45`, `release
on`).

Tests: showtree_test `sceneExcludedFixturesAreNotWritten`,
`exclusionIsThreadSafeWithLivePreview`, `groupSetsAreOneTargetAndKeepTracking`,
`childLayersExcludeTheirFixturesFromTheParent`, `overlappingLayersLowerWins`,
`childLayerEditsWhileTheDmxThreadRuns`, `anImportedOverlapMixesAsTheChaserDid`,
`aCueEffectIsOneAcrossItsLayers`, `cueSpreadReachesOnlyLineAims`,
`aCueAimIsOneFormationByPosition`, `aSpreadChangeFadesOverTheCueFade`,
`everyCommandWordHasHelpAndItsExamplesParse`, `copyAndPasteCues`;
showtreeview_test `groupSetsInTheLibrary`, `childLayersByMouse`,
`renameInPlace`, `lineAimsInTheTab`, `copyPasteAndHelp`,
`libraryTreeEditorAndAddingWithoutDragging`, `namedLayersShowWhatTheyHold`,
`editSeveralLayersAtOnce`, and
`everyActionIsOneUndoStep` (+13 actions).

### Open for Branson

1. **`Generated` mark on `<Function>`** as well as in `<ShowTree>`? (10.2;
   built without, to leave function.cpp alone.)
2. ~~**Editing the cue on stage**~~ — **resolved 2026-10-17:** Design mode
   shows it at once (the preview re-takes the cue after every bake); Live is
   blind until GO (10.4, 10.8).
3. **Undo shared with Programming?** (built: the tree's own.) And should tree
   undo cover palette value edits made from the tree's Look Editor?
4. ~~**Gobo mapping**~~ — **resolved 2026-10-17:** manual map + name match,
   built (10.8). Still open: an effect's own Gobo (engine stack rule change;
   out of scope of the fix pass).
5. **Hold 0 in imported lists:** DanceHysteria's 0.0.4–0.0.7 auto-follow
   (Hold 0). Intended? The import keeps it either way.
6. ~~**A preview on top of a list running for real**~~ — **resolved
   2026-10-17:** "In Design, whatever is in edit should be visible. In
   Perform, the cue list overrides." Built (§10.9): in Design the preview
   takes precedence over any running list; in Perform there is none.
7. ~~**Gobo names that differ only by suffix**~~ — **resolved 2026-10-17:**
   looser, in tiers after the per-type pick (exact → singular/plural →
   unique prefix/contains); ambiguous → not guessed, the row says so and
   offers the pick (§10.9).
8. **Blind: to discuss** (Branson, 2026-10-17). Today Blind is the global
   output mute and covers the preview; unchanged.
9. ~~**Record as new cue — which stage?**~~ — **resolved 2026-10-17:**
   "subjective; should prompt to ask how to handle it, and/or have config
   options that default this in the settings." Built (§10.11): it asks
   (selected cue / running cue / everything lit) only when they differ,
   "Don't ask again" writes the default; Settings → Show Tree has one
   default each for Design and Perform (Ask by default); `record
   selected|running|lit` overrides.
10. ~~**Command line: several groups.**~~ — **resolved 2026-10-17:** "need
    both behaviours, and they must be consistent." Built (§10.11): one
    layer is the default everywhere (drop, + Layer, Library, command line:
    `Towers USTs` = `Towers+USTs` = one layer), a layer each is explicit
    (⌥-drop, ⌥-click / menu on + Layer, `Towers, USTs`); Split layer and
    Merge layers in the tree.
11. ~~**Command line: `dimmer 45`**~~ — **resolved 2026-10-17:** "reuse, so
    we can adjust globally by updating the palette." Built (§10.11): an
    exact palette is reused (In this cue list → Recent → any); the command
    line otherwise makes a named one per the show's pattern; the picker a
    one-off with one-click Make reusable. D11/D21 updated.
12. **D10/D17 as a shared layout span** rather than one extra scene over the
    union (§10.13: the effect keeps each layer's own base under it). OK?
13. **Overlap rule:** a layer's own value beats a cue look reaching a lower
    layer on the fixtures they share (D5's "nearer scope"), rather than
    "the lower layer wins" whatever the source. OK? And imported cues keep
    mixing (Overlap: Mix) — fine, or should Convert to tracking (P4) clear
    it?
14. **The plan view** (top view, beams, the previous cue's points as rings)
    is not built; the order strip is. Wanted for P3, or later?
15. **A Mix cue in the Design preview** shows the lower layer where they
    overlap (the preview writes on top to win over a running list). Accept,
    or preview such cues without the edit override?
16. **A child's Dimmer on fixtures without a dimmer channel** doesn't dim
    the colour its parent gives them (the parent's scene scales by its own
    look dimmer). Fill the parent's colour into the child there (more
    scenes), or leave it as a known limit?
