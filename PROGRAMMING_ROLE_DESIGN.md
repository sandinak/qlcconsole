# Programming tab — its role next to the Show Tree

*Status: proposal for review (2026-10-17). Branch `design/programming-role`,
cut from `release/0.1.0`. Docs only. Companion to SHOW_TREE_DESIGN.md (on
`feat/show-tree`).*

The Show Tree is becoming the day-to-day programming surface: Branson built
DanceHysteria's preshow in it and it "went well". SHOW_TREE_DESIGN.md §9 kept
"replacing the Programming tab" out of scope, and its help says *"You build
palettes, groups, one-off scenes and effects there."* This doc decides what
that sentence should mean from here on.

Branson, 2026-10-17: *"considering the longer term of programming tab .. I
think yea .. it can be a more specific tool .. but may need some refinement to
make it more tied to that role rather than day-to-day role."*

The split this doc is written to:

- **Show Tree** owns anything cue-list shaped, and gains what day-to-day work
  still sends you to Programming for: busking looks, palette housekeeping,
  capture into a cue.
- **Programming** becomes the **palette & look workshop**: designing palettes
  (effects above all), organising them, seeing where they're used, capture
  into palettes, and raw-value / classic scenes. It stays, visible.

---

## 0. Decisions for Branson

**Your call:** `ok` takes the recommendation; otherwise write the option you
want (its words from Options), a note, or `?` to talk it over. Blank = not
reviewed.

| # | Decision | Options | Recommend | Your call |
|---|---|---|---|---|
| P-D1 | **Tab name** | Programming (keep) / **Palettes** / Looks Workshop / Workshop | **Palettes.** It names what the tab is for, the way an Eos user says "go to the palette display". "Looks" is taken by the Show Tree's Looks section (P-D5) and "Workshop" says nothing. Rename lands with R5, when the layout matches the name. | |
| P-D2 | **Visibility** | Always shown / hidden by default (Settings → show it) | **Always shown, moved after Show Tree** in the tab order (and ⌘ shortcuts). Revisit hiding after a season of shows run only from the tree. | |
| P-D3 | **Primary object of the tab** | The scene (today) / **the palette** | **The palette.** Palette tree on the left, the Look Editor full-size in the centre, usage + test targets on the right (§3). The scene canvas becomes the tab's second mode, **Scenes**. | |
| P-D4 | **Previewing a palette with no scene open** | Must be in an open look (today: "Not in a look") / a **test bench** | **Test bench:** pick groups/fixtures once ("Try on: Towers, USTs"); the palette under edit plays on them in Design mode. Today's "trying it on" an open look stays in Scenes mode. | |
| P-D5 | **Home of busking looks** (scenes fired from VC, not cues) | Programming scenes (today) / **a Looks section in a production** | **Show Tree Looks section** (§4.1): looks are built with layers like cues but never track; each bakes to one generated function a VC button fires. Programming keeps classic scenes for what doesn't fit. | |
| P-D6 | **A busk look over a running cue list** | Engine HTP/LTP as today / looks always win / looks always lose | **As today:** intensity HTP, the rest LTP, last started wins. That is how a Programming scene on a VC button behaves now, so nothing about a show's feel changes. A per-look "on top" priority can come later if a show needs it. | |
| P-D7 | **Firing a look from VC** | Existing VC button on the generated function / a new VC "Look" widget | **Existing VC button** (Toggle / Flash), bound to the look's generated function, plus **Make VC page** on a Looks folder: one button per look, laid out. No new widget. | |
| P-D8 | **Palette housekeeping in the Show Tree Library** | Pin only (today) / **the everyday operations** / everything | **Everyday operations:** + Palette (by type), rename, duplicate, move to folder, delete with usage, Open in Palettes. Effect design, `.qxfx` import/export and Bundles stay in Palettes. Both call one shared palette-operations layer (§5). | |
| P-D9 | **Capture** | Snapshot raw DMX into a scene (today) / **capture into palettes** / both | **Both, palettes first:** Capture reads live output, reuses palettes that play exactly those values, makes new ones named per the show's pattern for the rest (`buildPaletteFromBucket` exists), and leaves only what no palette can hold as raw values. Targets: a new look/cue/layer (Show Tree) or palettes only (Palettes tab). Raw Snapshot stays in Scenes mode. | |
| P-D10 | **Stage tools on the Programming toolbar** (Highlight, Flash, Park, Move in black, Beat, speed masters) | Stay / **app-wide Stage bar** | **App-wide Stage bar**, shown in Show Tree, Palettes and Fixture Manager. They act on the rig, not on a scene, and today they exist **only** on the Programming toolbar, so the Show Tree has no Highlight or Park. Jog speed and Save Positions stay with the position editor. | |
| P-D11 | **Bundles** | Keep / retire / fold into reusable layers | **Keep in Palettes, no new work.** They are cross-show (JSON in the user dir) and target-free; reusable layers (Show Tree P4) are per-show and targeted. Revisit if P4's export makes them redundant. | |
| P-D12 | **Non-scene functions in the tab** (Chaser, Collection, EFX, RGB Matrix, Show: create, stock editors, members, add to collection) | Keep / **Scenes only** | **Scenes only.** The other types open in the Functions tab ("Open in Functions"); cue-list work is the Show Tree's. Generated functions are hidden from Scenes mode (a "Show generated" toggle for inspection). | |
| P-D13 | **Where-used** | Delete dialog only (today) / **a Used by panel** | **Used by panel** beside the editor: cues (named "Cue 1.0.1 Verse — Towers", via `describeGenerated`), busk looks, classic scenes, effect slots, and the VC widgets that fire those. Click jumps there. | |
| P-D14 | **Undo of palette value edits** | Each tab its own (today: Programming per-scene, Show Tree per-tree) / **one palette undo shared by both** | **One palette undo**, owned by the shared palette layer: a colour dragged in either tab's Look Editor undoes in either. Scene and tree undo stay as they are. Answers Show Tree open question 3. | |
| P-D15 | **Blind** | Output mute (today) / Eos-style blind editing / to discuss | **To discuss** (§4.4 outlines one shape). Not built in any phase here until talked through. | |
| P-D16 | **Existing Programming scenes** | Leave / **adopt on request** / convert all | **Adopt on request:** "Adopt as look" on a scene makes a Looks-section look (layers from targets + palettes, raw values as an opaque layer), offers to re-point VC buttons, and leaves the scene. Nothing converts on load. | |

---

## 1. Today: what the Programming tab does

Taken from `ui/src/programmingmanager.cpp`, `scenegrouplooks.cpp`,
`lookeditor.cpp` and `engine/src/programmercontroller.*` on `release/0.1.0`,
plus what `feat/show-tree` already shares. **Fate:** *Stays* (in Palettes),
*Show Tree* (moves there / already there), *Shared* (one component used by
both), *Global* (out of any one tab), *Retire* (from this tab; it lives on
elsewhere).

### Left pane — Scenes / functions

| Capability | Code | Fate |
|---|---|---|
| Function tree of all six types, folders, filter, drag-to-file, folder rename/delete, "Group types by role" | `m_funcTree`, `slotFuncTreeMenu` | **Stays for Scenes only** (P-D12); other types → Functions tab |
| **Add** menu: New Scene / Chaser / Collection / EFX / RGB Matrix / Show / Folder | `addNewFunction` | **Stays: Scene + Folder**; the rest **Retire** |
| Members nested under an open collection/chaser/show; Remove from "…" | `syncMemberNodes`, `addMemberChildren` | **Retire** (Functions tab; Show Tree for cue lists) |
| Drop a scene on a collection row to add it | `addFunctionsToCollection` | **Retire** (Show Tree layers / includes) |
| Duplicate, ⌘C/⌘V, Delete / Delete Everywhere with usage text | `duplicateFunctions`, `deleteFunctions`, `functionUsage` | **Stays**; usage text **Shared** |
| Learn / Clear MIDI trigger on a function | `learnFunctionTrigger` | **Stays** for scenes; **Show Tree** for looks (§4.1) |
| Generated (Show Tree) functions: locked, not renamed/moved | `isGenerated` checks (feat/show-tree) | Already done; P-D12 hides them |

### Centre — scene canvas

| Capability | Code | Fate |
|---|---|---|
| Targets: dynamic (group) / fixed (fixture); warnings "no look for", "can't reach", unpatched | `SceneGroupLooks` | **Show Tree** for day-to-day (layers already do this); **Stays** in Scenes mode |
| Looks stack, effect with nested Colour/Dimmer, Move to base / Feed effect, up/down order | `SceneGroupLooks` | Same as above. The stack rules are the tree's bake target, so **Shared** model |
| Per-look Fade in / out (or "scene") | `LookColIn/Out` | **Stays**; Show Tree has cue fades only (`Cue::fadeIn/Out`) — gap, §4.1 |
| Scene Fade, **Transitions** (wipe/centre/dissolve/along bar, edge), per-scene **speed master** | `SceneGroupLooks` header row | **Stays**; **Show Tree gap**: no transition or speed master on a layer (`showtreemodel.h` has neither). §4.1 adds them |
| **Scope** (not set / Whole Stage / a group) | `m_scopeCombo` | **Retire** — a statement of intent the tree's layer target already is |
| Fixed-fixture raw values via the fixture console (mixed-type note) | `m_fixtureConsole` | **Stays** (raw-value scenes are Programming's) |
| Stock editors for Chaser/Collection/EFX/RGB Matrix/Show in the canvas | `loadFunctionEditor` | **Retire** (Functions tab) |
| Fixture-group layout editor (double-click a group, ← Back) | `loadGroupEditor` | **Retire** here → "Open in Fixture Manager" |
| Per-scene undo (scene snapshots) | `m_undoStack` | **Stays** for scenes; palette edits → P-D14 |
| Live preview in Design, the scene running in Operate, 2D Monitor follows | `startPreview`, `startOperateScene` | **Shared** preview rules (Show Tree has its own, §10.9 there) |
| "How this works" guide | `showProgrammingGuide` | **Stays**, rewritten for the palette role |

### Toolbar

| Capability | Code | Fate |
|---|---|---|
| Highlight, Flash | `setHighlightActive`, `flashFixture` | **Global** Stage bar (P-D10) |
| Park / Unpark all | `parkFixtures` (saved in the show) | **Global** |
| Mark, Unmark, **Auto move-in-black** + lead time | `markFixtures`, `setAutoMoveInBlack` | **Global** (a show setting; the runner relies on it) |
| Beat on, BPM, Tap, Speed masters… | `slotBpm*`, `SpeedMastersPanel` | **Global** |
| **Snapshot** live output into the open scene (raw, every lit patched fixture) | `slotSnapshotLive` | **Stays** (Scenes mode); capture-to-palettes is new (P-D9) |
| Jog speed; **Save Positions** (arm, confirm) for joystick / Aim drags | `m_speedSpin`, `commitDesignJoystick` | **Stays** with the position editor; **Shared** if the tree's Look Editor gets joystick nudging |
| Followspot binding panel | `m_followSpotPanel` (built, always hidden) | **Retire** the hidden panel; bindings live in Inputs |

### Right pane — sources

| Capability | Code | Fate |
|---|---|---|
| **Palettes**: filterable, foldered; new by type (Colour, Dimmer, Pan/Tilt, Aim, Beam, Gobo, Shutter, Strobe, Effect); new folder; rename; duplicate; move to folder; delete with usage; Save as Bundle; double-click to edit | `slotPaletteTreeMenu`, `createPalette`, `duplicatePalette` | **Stays, promoted** (§3); everyday ops **Shared** with the Library (P-D8). Show Tree's one-off palettes are already hidden here |
| **Bundles**: stamp onto the open scene (one-level undo), save from a look or palettes, user-dir JSON | `BundleBrowser`, `BundleCache` | **Stays** (P-D11) |
| Fixtures & groups tree, search, drag to Targets | `m_fixGroupTree` | **Shared** idea; the Library has its own. Used in Palettes for the test bench (P-D4) |

### Bottom — Look Editor and services

| Capability | Code | Fate |
|---|---|---|
| Look Editor: per-type editors, effect browser (live previews, Recent, WLED), `.qxfx` import/export, gobo by name, line-aim Spread/Grouping/Cross (feat/show-tree) | `LookEditor` | **Shared**: already one class, two instances (Programming, `stLookEditor`) |
| "Trying it on" a palette that isn't in the open look | `updateTrialPalette` | **Stays** in Scenes mode; test bench replaces it in palette mode |
| Power estimate (status-bar chip) and the circuits dialog | `initPowerTimer`, `openCircuitsDialog` | **Global** service: it is app-wide but its timer is owned by `ProgrammingManager` |
| Usage queries (`paletteUsage`, `functionUsage`, `usageLines`) | `ProgrammerController` | **Shared**; misses VC widgets and busk looks today |

### Not the tab, but "programming": the controller programmer

VC buttons with **SelectFixtures / SaveProgrammer / RevertProgrammer / pad
modes** (`vcbutton.h`) paint raw values and save them as scenes, smart-bucketed
into group scenes with palettes (`proposedSaveBuckets`,
`saveBucketAsGroupScene`). **Stays** as is; its Save dialog gains a "into a
look / cue" target once §4.3 exists.

---

## 2. The refined role

**Name** (P-D1): **Palettes**. Alternatives: keep *Programming*; *Looks
Workshop*; *Workshop*.

**What it's for.** Palettes is where the show's vocabulary is made and kept:
the colours, levels, positions, beams, gobos and above all the **effects**
that cues and busk looks are written in. You design a palette on a test
bench of real fixtures, file it, see everywhere it's used, and change it once
for the whole show. It also keeps the two jobs the tree doesn't do: scenes of
raw channel values (a hazer, a practical, a fixture with no palette-able
channels) and capturing a look someone built on another console.

**Who uses it, when.**

| When | Who | Does |
|---|---|---|
| Before the show is written (prep, off-rig) | the programmer | builds the house palettes and effect library; files folders; imports `.qxfx` |
| Focus / tech | programmer + LD | refines positions (Aim, joystick nudges, Save Positions), checks "applies to N of M", tunes effects on the rig |
| During writing | programmer, from the Show Tree | jumps in via ✎ / **Open in Palettes** for an effect that needs real design time; everyday tweaks stay in the tree |
| Guest console / festival | programmer | **Capture** what the guest console outputs into palettes |
| Show time | nobody | the tab has nothing an operator needs |

---

## 3. Refinements to the tab

```
┌ Palettes ─────────────[ Palettes | Scenes ]──────────────────────────────────┐
│ Filter…            + ▾ │ Effect: Sinelon "Steps run"     ⟲ undo │ Used by (4) │
│ ▾ Colour (14)          │ ┌ Look Editor, full width ──────────┐ │ Cue 1.0.1   │
│   ▸ House              │ │ effect settings, browser, spread, │ │  Verse·Steps│
│   ▸ 01-Everybody…      │ │ speed …                           │ │ Cue 1.0.3   │
│ ▾ Effect (28)          │ └───────────────────────────────────┘ │ Look "Hit 1"│
│   ▸ Movement           │ Try on: [Steps ×] [USTs ×]  + group   │  VC: Busk/7 │
│   ● Sinelon "Steps run"│ applies to 12 of 12 · Design: live    │ Scene "Old" │
│ ▸ Position (9)         │                                       │ ─────────── │
│ ▸ Gobo (6)  …          │ Capture ▾   Bundles ▾                 │ Fixtures &  │
│                        │                                       │ groups      │
└────────────────────────┴───────────────────────────────────────┴─────────────┘
```

**Promote**

1. **Palette tree on the left, all types**, folders of any type (the tree
   already supports it; the left pane becomes it). Counts per type and per
   folder. "Unused" filter (no usage at all) for clean-up.
2. **Look Editor in the centre, full width**, not a bottom pane sized to
   content. The effect editor gets the space the effect browser was moved
   into its own window for.
3. **Test bench** (P-D4): a "Try on" row of groups/fixtures, remembered per
   palette type (positions on movers, colours on washes). In Design mode the
   palette under edit plays on them, as the tree's preview does: edit wins
   over any running list in Design, nothing in Perform (Show Tree §10.9's
   rule, reused, not re-invented).
4. **Used by panel** (P-D13) on the right: cues (by cue number and layer),
   busk looks, classic scenes, effect slots, VC widgets. Each row jumps: a
   cue opens the Show Tree on that cue. The delete question reads from the
   same list.
5. **Capture** menu (P-D9): *Capture into palettes…* (palettes only, filed in
   a folder you name) and, in Scenes mode, today's raw *Snapshot*.
6. **Effect design** keeps everything it has (browser, presets, `.qxfx`,
   WLED) and gains **Save as preset** next to Export, so a tuned effect is
   one click to reuse in other palettes.

**Demote / remove**

1. The function tree → **Scenes mode** (top switch), scenes only (P-D12).
   Add menu: New Scene, New Folder.
2. Stock editors for other function types, members under containers,
   add-to-collection drop → gone from this tab ("Open in Functions").
3. Toolbar stage tools → app-wide **Stage bar** (P-D10). Jog speed and Save
   Positions move into the Pan/Tilt and Aim editors.
4. Scope combo → removed (existing values kept in the file, ignored).
5. Group layout editor → "Open in Fixture Manager".
6. Hidden followspot panel → deleted.

Scenes mode keeps today's canvas for classic and raw-value scenes: Targets,
Looks, fixture console, transitions, Snapshot, Bundles stamp, per-scene undo.

---

## 4. What the Show Tree gains

### 4.1 Looks section (busking)

**Model.** A production gets a second kind of child beside cue lists:

```
Production DanceHysteria
├── Cue list "Full Show"
└── Looks                         new
    ├── ▸ Hits                    folder
    │   ├── Look "Hit 1"          layers: Steps Strobe Fast, Washes White Full
    │   └── Look "Blackout Hit"   layers: All Dimmer 0  [Flash]
    └── Look "Crowd Wash"         cue looks: Colour Amber; layer AudienceBlinders 60%
```

A **look** has what a cue has, minus what only makes sense in a list: cue
looks, layers (child layers, group sets, effects across layers, line aims —
all of P3), **no** number, mode, tracking, includes, Release or sub-cues. It
always resolves as a Block cue would. It adds:

- **Fire as:** Toggle / Flash (the VC button's action, set when the button is
  made) and the MIDI trigger (`learnFunctionTrigger`, today a Programming
  function-menu item).
- **Fade in / out** and, closing today's gap, a **transition** (shape, time,
  edge) and **speed master** — on the look and on cue layers alike, baked
  into the generated scene's existing fields, so they cost no engine change.

**Bake.** One look = one generated function: a Scene if it has one layer, a
Collection of the layer scenes otherwise (as a cue). Generated, locked (D13),
named after the look, filed under `Productions/<name>/Looks/`.

**Fired from VC** (P-D7): a VC button points at the generated function, as
any button does. **Make VC page** on the Looks node or a folder lays out one
button per look on a new VC page (Toggle, caption = look name, colour from the
look's first Colour). The VC function picker lists looks by name ("Look: Hit
1"), not their generated scene.

**With a running cue list** (P-D6): the engine's normal merge — intensity
HTP, everything else LTP, last started wins. The runner's "what changes" line
and "Next cue receives" ignore looks (they aren't the list's state); the Stage
bar's Park still beats both. **Record as new cue** with "everything lit"
already takes in "anything else on"; R3 makes it record a running look as the
look's layers rather than its generated function.

**Relation to cues.** *Copy to look* on a cue (its resolved output, frozen,
as a look) and *Add to cue* on a look (its layers into the selected cue, own
layers) cover the "I busked it, now I want it in the list" path. No live
link: a look is not an include.

### 4.2 Palette management in the Library

The Library's **All → Palettes** gains the everyday operations (P-D8), from
the shared layer (§5): **+ Palette ▾** (type), Rename (F2, as layers),
Duplicate, Move to folder (drag, as in Programming), Delete (the usage
question names cues), **Open in Palettes** (the tab, that palette selected),
and folder New / Rename / Delete. The ✎ on a value already opens the shared
Look Editor; it gains the **Used by** line ("used in 6 cues, 2 looks" — click
for the list). Effect browser and `.qxfx` stay reachable through ✎ since the
editor is the same class.

### 4.3 Capture into a cue or look

**Capture ▾** in the tree's header (beside Record): *as new cue*, *into this
cue*, *as new look*. It reads live output (as Snapshot does: pre-Grand-Master,
patched fixtures only), groups fixtures by the show's groups (the
`proposedSaveBuckets` / `splitBucketByGroup` logic), then per group and
attribute:

1. a palette that plays exactly that value → reused (as the picker does,
   §10.11 there);
2. else a new palette named per the show's pattern, in a `Captured/<date>`
   folder (`buildPaletteFromBucket`);
3. what no palette type holds → one raw-value scene as an opaque layer.

A preview sheet lists the layers it will make, the palettes reused / made, and
the raw leftovers before anything is written. One undo step. Fixtures driven
by a running cue list are left out by default (as `CaptureManager` already
excludes chaser-driven channels).

### 4.4 Blind (outline only — to discuss, P-D15)

Today Blind is the global output mute: every protocol silenced, Design-only,
forced off going to Operate; the tree's own preview obeys it. An Eos-style
blind would instead be **edit without output**: a cue or palette changed
while the stage keeps playing what it played. Pieces it would need, for the
discussion:

- a per-tab or per-edit **Blind** switch (the tree's Live mode is already
  blind until GO — §10.4 there — so this may be one rule, made explicit);
- the **Lighting Studio / 2D Monitor showing the blind state** while DMX shows
  the live one;
- palettes: an edit in blind marks the palette "changed, not on stage" and
  every running cue using it holds the old values until **Update**;
- how it relates to Design vs Perform, and whether the mute keeps its name.

---

## 5. Shared pieces — one source of truth

| Piece | Today | Plan |
|---|---|---|
| **Look Editor** | one class, two instances (Programming, `stLookEditor`); both edit the palette in the Doc | keep two instances, one class; editor-only features (Used by, test bench hook) go in the class, host-specific ones behind a small host interface |
| **Palette operations** (create by type, rename, duplicate, move to folder, delete with usage, folders) | private methods of `ProgrammingManager` (`createPalette`, `duplicatePalette`, folder menu) | move to an engine/ui-neutral **`PaletteOps`** (beside `ProgrammerController`), called by the Palettes tree, the Library and the command line |
| **Palette undo** (P-D14) | none for value edits in either tab | one stack in `PaletteOps`; both editors push to it |
| **Usage** | `paletteUsage` / `functionUsage`; tree names generated scenes via `describeGenerated` | one `usage(pid)` returning cues, looks, scenes, effect slots and **VC widgets** (new: walk VC for functions that use it) |
| **Preview** | Programming: open scene in Design, stops on tab hide; Show Tree: selected cue, wins over running lists in Design | one rule ("in Design the thing being edited is visible; in Perform nothing") and one helper both tabs call |
| **Power estimate / circuits** | timer owned by `ProgrammingManager`, app-wide chip | move to the app (or `Doc`) so it never depends on a tab existing (needed before P-D2 could ever hide the tab) |
| **Stage bar** (Highlight, Flash, Park, MIB, Beat, masters) | Programming toolbar only | one app-level widget, hosted by each tab that wants it |
| **Command line** | Show Tree only | add palette verbs (`palette colour "Amber 2" #ffb000`, `rename`, `move`) to the same grammar; it then works from the Library. No command line in Palettes |
| **Fixtures & groups source** | Programming tree; Library's own | keep two views; one search behaviour (match row or child, open folders on the way) |

---

## 6. Migration — nothing breaks

- **Every existing scene stays a scene.** Programming-built scenes (targets +
  look stacks) keep playing from VC buttons, chasers and shows exactly as now.
  Scenes mode still opens and edits them.
- **Cue lists:** *Import chaser as cue list* (built, Show Tree P1) already
  turns a scene of targets + palettes into a layer and anything else into an
  opaque function layer. Unchanged.
- **Busk scenes:** *Adopt as look* (P-D16) on a scene in Scenes mode or the
  Library: targets + looks → a look's layers (an effect's nested looks → its
  own looks; transitions, fades, speed master carried); raw values → an
  opaque layer. Offers to re-point VC buttons that fire the scene; the scene
  stays until you delete it.
- **What can't adopt cleanly** (listed before it runs): two looks of one kind
  where "lower wins" mattered, Scope, per-look fades that differ inside one
  attribute.
- **Opaque layers** are the safety net throughout: anything the tree can't
  express is referenced, not converted.
- **File format:** Looks are a new child of `<Production>` in `<ShowTree>`;
  an older build ignores the element and still plays the generated functions.
- **Help:** `programming-tab.md` → `palettes.md` (old anchors redirected),
  `show-tree.md` gains Looks, Library palettes and Capture sections.

---

## 7. Phases

Show Tree P1–P3 are built; P4 (Make reusable / Detach), P5 (productions),
P6 (sub-cues in the tree) and P7 (cue runner) are planned. Sizes: S ≈ a day,
M ≈ 2–4 days, L ≈ a week.

| Phase | Slice | Size | Slots |
|---|---|---|---|
| **R1** | **Shared pieces**: `PaletteOps` out of `ProgrammingManager`, palette undo, one `usage()` incl. VC, power estimate to the app, **Stage bar** (P-D10) | M | alongside **P4** (no overlap in code) |
| **R2** | **Library palette management** (§4.2) + Used by line in the Look Editor; palette verbs in the command line | M | right after R1, before P5 |
| **R3** | **Looks section** (§4.1): model, bake, VC binding and Make VC page, transitions / speed master on looks *and* cue layers, Copy to look / Add to cue, **Adopt as look** (§6) | L | after **P4** (a look can use reusable layers), with or just before **P5** (a looks section belongs to a production) |
| **R4** | **Capture** into palettes, cue, layer or look (§4.3); controller-programmer Save gains the target | M | after R3 |
| **R5** | **Palettes tab refinement** (§3): layout flip, test bench, Used by panel, Scenes mode, removals, rename (P-D1), help rewrite | M–L | after R2 (needs `PaletteOps`); before or with **P7** so the runner ships with a tab set that makes sense |
| **R6** | **Blind** — after the discussion (P-D15); depends on how P7's Live/Design split feels on the rig | ? | after P7 |
| later | Hide Palettes by default (P-D2), if a season shows it's not missed | S | — |

Order with the tree: **P4 + R1 → R2 → R3 → P5 → R4 → P6 → R5 → P7 → R6.**
R1–R3 are what makes "never needs Programming day to day" true; R5 is
polish of a tab used less, so it can wait behind the runner if the runner is
wanted sooner.

---

## 8. Not in scope

- Removing the Programming / Palettes tab, or the classic Scene editor.
- Converting existing scenes on load; editing generated functions (D13
  stands).
- A per-production palette scope (palettes stay show-wide, as SHOW_TREE §9).
- A new VC widget for looks (P-D7: buttons are enough); busk-look priority
  levels beyond HTP/LTP (P-D6).
- Engine-resolved playback for looks (as SHOW_TREE §5: only if baking hits a
  wall).
- The VC controller programmer (SelectFixtures / SaveProgrammer / pads)
  beyond giving its Save a look/cue target.
- Bundles redesign (P-D11).
- Blind beyond the outline in §4.4 until it's discussed.
