# Show Tree — design

*Status: proposal for review (2026-10-16). Branch `design/show-tree`. Docs only.*

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

| # | Decision | Options | Recommend |
|---|---|---|---|
| D1 | **Root name** | Production / Show set | **Production.** "Show" is the timeline function, "set" is taken by *group set* (and means a setlist to a band). Note SHOW_LIFECYCLE_DESIGN.md uses "Production" for the *phase* (Construction → Test → Production); that word isn't on screen anywhere, so rename the phase "Run" in that doc. |
| D2 | **Where it lives** | A view inside Programming / its own top tab | **Its own tab, "Show Tree"**, beside Programming. Shares the Look Editor and the source trees (palettes, fixtures & groups). Programming stays for one-off scenes and palette work. |
| D3 | **Cascade resolved when** | Baked at edit time into generated scenes / resolved by the engine at playback | **Baked** (§4). Bake palette *references*, not DMX, so palette edits and group membership stay live with no re-bake. Engine resolution is a later option (§5), not a prerequisite. |
| D4 | **Layer editor shape** | Today's ordered look *stack* / one row per *attribute* | **Rows per attribute** (Colour, Dimmer, Position, Beam, Gobo, Shutter, Strobe, Effects). A cascade is per attribute, so the editor has to be; the generator writes the stack in the canonical order the stack rules already define. Stack order only mattered for "lower wins" between two looks of one kind, which a row makes impossible. |
| D5 | **Precedence** | — | **This cue beats earlier cues; inside a cue the nearer scope wins.** Tracked < included < cue looks < layer looks < effect's own looks. One sentence the operator can hold. |
| D6 | **What cue looks reach** | The cue's own layers / also layers tracked into it | **Own layers only.** A tracked layer shows its value and where it came from; one click ("Bring into this cue") makes it an own layer, and the cue's looks then reach it. Otherwise a "Blue" on cue 12 silently repaints everything still on from cue 3. |
| D7 | **What Release goes to** | Dark / home / the list's base cue | **The base cue if the list has one, else dark.** A cue list may mark one cue as its *base* (DanceHysteria's `Show/Base` scenes are exactly this). Released fixtures return to what the base gives them; ones the base doesn't cover go to intensity 0 and *hold* their other attributes (no swing while fading, and move-in-black can preset them). |
| D8 | **Include + tracking** | — | **Include brings the included cue's resolved output (as if you stood in that cue), live-linked.** Order: tracked state → includes (in order) → this cue's own. Included values track onward like any other. No cycles. The included cue's own Cue-only/Block don't affect the includer. |
| D9 | **Sub-cues** | Track through sub-cues into the next main cue / sub-cue changes are cue-only | **Cue-only.** Cue 5 tracks from cue 4's own state, not from wherever Sub GO got to (it can't be known at bake time, and SUBCUES_DESIGN.md already drops the sub-cues on GO). Inside the sub-list, 4.2 tracks from 4.1. |
| D10 | **An effect at cue level** | Copied into each layer / one effect across all the cue's layers | **One effect across them**, so a Sinelon runs across the stage as one (the multi-group stage layout already does this). Layers that set that effect kind themselves — or set it to **None** — are left out of it. |
| D11 | **One-off values** | Must be a named palette / can be inline | **Inline allowed**: an unnamed palette owned by the layer, hidden from the palette tree. **Make reusable** names it and files it. This is what stops `Palettes/Color/01-Everybody Scream`. |
| D12 | **Import of existing shows** | Convert in place / import as a copy | **Import a chaser as a cue list, as a new production; originals untouched.** Imported list starts **Non-tracking**, so it plays exactly as before; **Convert to tracking…** then offers to strip what repeats. (§7) |
| D13 | **Hand-editing a generated scene** | Allowed (overwritten on re-bake) / locked | **Locked**, with **Detach to plain scene** (the tree then points at that scene as an opaque function layer). |
| D14 | **Cue-list-level looks** | Yes / no | **No.** A base cue + tracking does that job and shows up in the tree as a cue. |

---

## 1. The model

```
Show file
├── Library  (shared by every production)
│   ├── Fixtures, Groups               (as today)
│   ├── Group sets                     new: named list of groups, e.g. Towers = T1..T4
│   ├── Palettes                       (as today; the reusable values)
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
| **Look** | A palette (named, or inline/unnamed) on one attribute; an **Effect** look can hold child Colour/Dimmer looks (today's rule: they belong to that effect only). A look can be **None** — "this attribute is off here", to stop an inherited value. |

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
Position (Pan/Tilt or Aim), Beam, Gobo, Shutter, Strobe, and Effects keyed by
kind (colour / dimmer / movement / beam effect). Each resolves on its own.

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
0.0.3"*, *"Red — included from 0.0.3"*, *"White — Confetti's own"*.

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
- **Right**: the Programming tab's sources plus group sets and reusables.
- **Bottom**: the existing inline Look Editor, unchanged.
- Later: a **tracking sheet** (cues × layers grid) for reading a whole list.

A clickable mockup of this view (tree, inherited values with their source,
Track/Block/Cue-only, Release, an include, Make reusable/Detach, and a
tracking sheet per cue list) is published alongside this doc.

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
| **P1** | **Show Tree tab, one production, Non-tracking lists.** Cue list = Chaser, cue = Collection, layer = generated Scene. Cue looks cascade to layers; layer attribute rows with inherited values greyed; inline looks; effects with child looks; opaque function layers; **Import chaser as cue list**. Spike: shared-scene hand-over across a step change. | none, or the hand-over fix |
| P2 | **Tracking**: list mode, cue Track/Block/Cue-only, layer Release, base cue, **includes**, origin marks, "Next cue receives", re-bake chain. | none |
| P3 | **Group sets**, child layers, overlap; cue-level effects across layers. | `Scene::excludedFixtures` |
| P4 | **Make reusable / Detach** for layers and cues; Reusable panel; Convert-to-tracking. | none |
| P5 | **Multiple productions**: switcher, current production for VC / footer GO / show control, per-production timecode. | small: "current production" in Doc |
| P6 | Sub-cues in the tree; tracking sheet. | none (SUBCUES) |
| later | Engine-resolved cue lists, move fades (§5) — only if baking hits a wall. | large |

P1 alone removes the triple filing: a song is one branch of one tree, and the
scenes and collections file themselves.

## 9. Not in scope

Replacing the Programming tab; channel-level tracking in P1–P6; a separate
palette scope per production (palettes are shared; inline looks cover
one-offs); cue-list-level looks (D14).
