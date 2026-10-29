# Programming tab — a redesign that flows like the Show Tree

*Status: proposal, 2026-10-21. Design only, nothing built. Companion to
PROGRAMMING_ROLE_DESIGN.md (P-D1…P-D16, all agreed) and SHOW_TREE_DESIGN.md.
Interactive mockups of the options, with today's tab rendered offscreen
for comparison: https://claude.ai/artifact/VrfTgmrVMzDMAdnw31KH1X (private).*

Branson, 2026-10-21: *"perhaps a refactor of the programming page generally
to make it flow better.. like we did with show tree"*.

What he has hit in the tab:

1. **He can't expand all looks to move things between them.** The canvas
   shows one scene at a time; to move a palette or a group from one scene to
   another you open one, remember, open the other, drop, go back, remove.
   Inside a scene the look list folds effects (nested Colour/Dimmer) one by
   one.
2. **No handle between looks and fixtures/groups.** Targets and Looks sit
   side by side in a plain layout (`scenegrouplooks.cpp`, `QHBoxLayout`
   `cols`), so the half you need can't take the room.
3. **It feels cramped and doesn't flow.** Four regions compete (function
   tree, canvas, Look Editor under it, sources on the right with Fixtures &
   groups under them), the Look Editor gets what is left under the canvas,
   and the stage tools still crowd the tab's part of the top bar.

`feat/expand-all-resize` (in progress) adds Expand all / Collapse all and
splitters to today's tab as a stop-gap. This proposal supersedes it in P1,
but it is worth merging: it costs nothing to drop later.

---

## 1. The tab's job in the Show Tree era

The Show Tree is now the day-to-day surface: cue lists, tracking, busk looks
(the Looks section, R3), everyday palette housekeeping in its Library (R2),
one undo history (R1). PROGRAMMING_ROLE_DESIGN §2 already says what is left:

> Palettes is where the show's vocabulary is made and kept: the colours,
> levels, positions, beams, gobos and above all the **effects** that cues and
> busk looks are written in.

So the tab builds **things cues use**, and the plain scenes the tree doesn't
do:

| Stays here (the workshop) | Lives in the Show Tree | Leaves both |
|---|---|---|
| Palette design on a **bench** of real fixtures, effects above all (browser, presets, `.qxfx`, WLED) | cue lists, cues, layers, tracking, includes, sub-cues | Chaser / Collection / EFX / RGB Matrix / Show editors → **Functions** tab (P-D12) |
| Palette housekeeping (the same `PaletteOps` and Library widget as the tree) | busk **Looks** fired from VC (P-D5) | group layout editor → Fixture Manager |
| **Scenes**: classic group-and-palette scenes and raw-value scenes (hazer, practicals), transitions, speed master | capture into a cue / look (R4) | Scope combo (kept in the file, ignored) |
| Effects as palettes; Bundles (P-D11) | Record, the cue runner | the hidden followspot panel |
| Capture into palettes, raw Snapshot into a scene | | Stage tools → app-wide **Stage bar** (P-D10) |
| Where-used for a palette (Used by) | | |

Two words clash today and this design fixes them (open question 1):

- In the Programming tab a **look** is a *palette row inside a scene*
  (`SceneGroupLooks`); the help says "Scenes hold looks".
- In the Show Tree a **Look** is a *busking look* in a production's Looks
  section.

Here, a scene's children become **Targets** and **Palettes** (rows), the
same words the Show Tree uses for a layer's group and its values. "Look"
then means one thing in the app: the Show Tree's busk look.

---

## 2. The proposed layout (recommended: Option A, "the Show Tree's twin")

```
┌ one top bar: [+ Scene] [+ Palette ▾] [Capture ▾]  ⟲ ⟳ │ show controls … DESIGN ┐
├ Stage bar (app-wide): Highlight · Flash · Park ▾ · MIB ▾ · ♩ 128 Tap · Masters ┤
├──────────────┬────────────────────────────────┬─────────────────────────────────┤
│ LIBRARY      │ OUTLINE   [⊞ expand] [⊟ fold]  │ PAGE: Palette "Blue" (Colour)   │
│ Filter…      │ ▾ ◎ Bench   Try on: Steps, USTs│ ┌ Look Editor, full height ──┐  │
│ ▸ Recent     │ ▾ ▢ Show/Base                  │ │ wheel · swatches · 1–6     │  │
│ ▸ Pinned     │   ▾ Base -Washes     ⚠ 1       │ │ sliders · wheel/ring       │  │
│ ▾ Palettes   │      ◆ TrussC-RGB              │ └────────────────────────────┘  │
│   ▸ 01-Every…│      ◆ T TrussW-RGB            │ Fade in 0s · out 0s             │
│   ● Blue     │      ◆ AT-W-ALL                │ Plays on: Base -Washes (3 grps) │
│   ● White    │      ● Blue            ←sel    │ applies to 26 of 26             │
│ ▸ Effects    │      ● Base 60%                │ Used by: 4 scenes, 3 cues ·     │
│ ▾ Groups     │      ● Spots-0                 │   1 VC button   [which]         │
│   Steps      │   ▸ Base-Steps                 │                                 │
│   Trusses ▸  │   ▸ Base-UST                   │                                 │
│ ▸ Fixtures   │ ▸ ▢ Show/0-Preshow (6)         │                                 │
│ ▸ Bundles    │ ▸ ▢ Show/1-Everbody Scream (2) │                                 │
├──────────────┴────────────────────────────────┴─────────────────────────────────┤
│ > Base -Washes colour Blue dimmer Base_              Tab: completes · ? help    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

Left to right, as in the Show Tree: **Library | Outline | Page**, the
**command bar** always open at the bottom, everything in **one top bar**.
The dividers drag and are remembered (and the outline's name/badge column,
as the tree's).

### 2.1 Library (left) — the Show Tree's own widget

The same class the Show Tree uses (`ShowTreeView`'s Library, lifted into a
reusable widget), so search, folders, + Palette, F2, drag-to-file, Delete
with Used by, Pinned and Recent behave identically in both tabs. Sections:

- **Recent** and **Pinned** (shared with the tree: what you pinned there is
  pinned here);
- **Palettes**, folders first (any type in a folder, as today), or by type;
- **Effects**: effect palettes and the effect **presets** (browser
  categories, Recent, WLED), so "new effect from preset" is a drag;
- **Groups** (with their Fixture Manager folders) and **Fixtures**;
- **Targets** (stage targets, for Aim);
- **Bundles** (P-D11, unchanged: drag a bundle onto a scene to stamp it).

The Show Tree's "In this cue list" section becomes **In this folder** here:
the palettes and groups the selected scene's folder uses, most-used first.

### 2.2 Outline (middle) — what you are building

A tree, as the Show Tree is: **Bench**, then the scene folders.

- **◎ Bench** (P-D4's test bench, made a node): its targets are the groups
  you try palettes on ("Try on: Steps, USTs"). It is never a function and
  never plays outside Design. Select a palette in the Library and it plays
  on the bench; drop groups on the bench to change what it plays on.
- **Scene folders** (`Show/Base`, `Show/0-Preshow`, …) → **scenes** →
  their **targets** (◆ group, ◇ fixture) and **palette rows** (● with a
  swatch or a type glyph; an effect row ✦ holds its own Colour/Dimmer, one
  level deeper, as the stack rules say).
- **Expand all / Collapse all** (buttons and ⌘→ / ⌘←, ⌥-click a triangle
  for its whole subtree, as Qt trees and Finder do). Expansion is
  remembered per show.
- Badges in a right column, as the tree's: **⚠ n** (no look for a target,
  can't reach, unpatched), **VC ×n** (buttons firing the scene), **▣** a
  raw-value scene, **⇄** a transition, **⏱** a speed master.
- Hover for the details (the stack notes `LookStackCheck` writes today,
  which group a palette doesn't reach).

### 2.3 Page (right) — the selected thing, with room

What the Show Tree calls the page; here it is wider, because editing is the
tab's main work:

| Selected | Page |
|---|---|
| a palette (Library, or a row in a scene) | **the Look Editor at full height** for its type (Colour, Dimmer, Position (Pan/Tilt, Aim + jog + Save Positions), Beam / Gobo / Shutter / Strobe, Effect with the browser inline), its **Fade in / out**, **Plays on** (the bench, or the scene it was picked in), *applies to N of M*, the **Used by** line (R1's) |
| a scene | name, folder, fades, **transitions**, **speed master**, warnings, *Fired by* (VC), **Snapshot**, Duplicate, **Adopt as look** (P-D16) |
| a raw-value scene or a fixed fixture row | the fixture console, as today, full height |
| a group / fixture row | its members, *Open in Fixture Manager* |
| the bench | its targets, and which palette type it remembers them for |
| a folder | its scenes, Expand all, Make VC page (as the tree's Looks folder) |

### 2.4 Command bar (bottom) — always open, Tab completes

One grammar with the Show Tree's (`ShowTreeCommand`), so a palette verb
learned there works here. New scene verbs on top:

```
scene "Base-Lanterns" in Show/Base          new scene, selected
Steps TrussC-RGB                           add targets to the selected scene
colour Blue dimmer Base                    add / replace palette rows
Steps colour #33aa55                       a new palette, named per the show
move Blue to "BackWash"                    move a row (copy: dup … to)
palette colour "Amber 2" #ffb000 in 01-Everybody Scream   (R2's, unchanged)
bench Steps USTs                           what the bench plays on
expand all · fold all · undo · redo · ?
```

Tab completes names (groups, palettes, scenes, folders) as it does in the
tree; the hint line above says what Return will do.

### 2.5 Stage bar and live preview

- **Stage bar** (P-D10): Highlight, Flash, Park ▾, Move in black ▾, Beat
  (BPM, Tap), Speed masters. One app-level widget, shown under the top bar in
  Programming, Show Tree and Fixture Manager. The tab's own part of the top
  bar keeps only + Scene, + Palette ▾, Capture ▾ and Undo / Redo, so it
  stops folding into **»** at laptop widths.
- **Preview rule** (§5 of the role doc, one helper both tabs call): in
  **Design** the thing selected is visible: a palette plays on the bench or
  on its scene; a scene previews; the 2D Monitor and Lighting Studio follow.
  In **Operate** nothing previews (the edits still save; open question 7).
  A thin **On the rig** strip at the top of the page shows what is playing
  ("▶ Preview: Base -Washes · Blue on 26 heads"), with *Stop*.
- **Power estimate** leaves the tab (R1 leftover): the status-bar chip is
  owned by the app, so it works whatever tab exists.

---

## 3. Interactions

### Drag and drop (one rule set; the hint says what a drop does before you let go)

| Drag | Onto | Does |
|---|---|---|
| palette (Library) | a scene, or its Palettes rows | adds a palette row (same type already there: replaces it, ⌥ adds a second, e.g. colour 2) |
| palette | an effect row | the effect's own Colour/Dimmer |
| palette | the bench | plays it on the bench (doesn't change any scene) |
| group / fixture | a scene | a dynamic / fixed target |
| group / fixture | the bench | Try on |
| a row (target or palette) | another scene | **moves** it; ⌥ copies (as layers between cues in the tree) |
| a row | another place in its own scene | reorders the stack (the lower wins) |
| scene | a folder | files it |
| scene | another scene | nothing (no includes here; the hint says "use the Show Tree") |
| bundle | a scene | stamps it (replaces its palette rows; one undo step) |

### Selection and editing

- **Multi-select** (⇧/⌘) anywhere in the outline: rows across scenes move
  or delete together; several scenes take one dropped palette each ("Blue →
  3 scenes"); several palette rows of one type edit together on the page
  (as the tree's *Editing several layers*).
- **Inline rename**: F2 or a slow second click, on scenes, folders, palettes
  (in the Library or a row). Duplicate learns the `SS-PP.II-Description`
  pattern from siblings, as now.
- **Keyboard**: ↑/↓ moves, → / ← open / close, ⌘→ / ⌘← all, Return opens
  the page (focus to the editor), ⌫ deletes with the Used by question, **:**
  in the outline goes to the command bar (as in the tree), Esc comes back.
- **Undo**: everything goes on the **one history** (R1's shared stamp):
  outline edits, palette housekeeping, the tree's steps, and (the R1
  leftover) Look Editor value edits, a slider drag coalesced into one step.
  Today's canvas snapshots with no redo go; ⇧⌘Z works for scene edits too.

### Design / Operate

Design: edit and preview as §2.5. Operate: the outline and page still edit
(a programmer touching a palette mid-show is real), but nothing previews and
a banner on the page says edits go straight to whatever is playing them.
Blind (P-D15) stays a separate discussion.

---

## 4. One source of truth with the Show Tree

| Piece | Rule |
|---|---|
| Palettes | show-wide `QLCPalette`s in the Doc; both tabs edit the same object through `PaletteOps` (R1). A palette edited here re-bakes every cue and look using it (the tree already re-bakes on palette change). |
| Library | one widget class, two instances; Pinned and Recent shared. |
| Look Editor | one class (`LookEditor`); the Programming instance moves from "under the canvas" to "the page". Per-palette fades, the Used by line, *applies to N of M* live in the class. |
| Used by | `PaletteOps::usage()` + `usedby.cpp`, unchanged; the page shows it, links jump to a cue (Show Tree) or a scene (outline). **Open in Palettes** (R2 leftover) from the Library/tree selects the palette here, on the bench. |
| Per-look fades, transition, speed master | one **Timing** section of the page, used by a scene here and (R3 remainder) by a Show Tree look and a cue layer, stored in the generated scene's existing fields. |
| Capture | one engine (`CaptureManager` + `buildPaletteFromBucket`); targets differ: *into palettes* / *new scene* here, *cue / look* in the tree (R4). |
| Command grammar | one parser; verbs that need a cue only work in the tree, scene verbs only here; palette verbs in both. |
| Stage bar, preview rule, power | app-level, as §2.5. |

---

## 5. Migration — existing shows keep working

- **No file format change** for P1–P3. The outline is a view of what is
  already saved: a `Scene` with `<FixtureGroup>` / fixture targets and an
  ordered `<Palette>` list (`SceneGroupLooks`' model). Order, effect nesting,
  transitions, speed masters, fades: read and written as now.
- Non-scene functions are not in the outline. A scene folder that holds
  chasers or collections shows them greyed with *Open in Functions*, once,
  so nothing seems to vanish (open question 5).
- Generated (Show Tree) functions stay hidden, as today; "Show generated"
  for inspection (P-D12).
- **The bench** is new: saved as `<ProgrammingBench>` in the workspace
  (groups per palette type). Older builds ignore it.
- `ProgrammingManager::currentSceneId()` / `currentPaletteId()` (control
  surfaces) keep their meaning: the scene selected in the outline, the
  palette on the page.
- The VC controller programmer (SelectFixtures / SaveProgrammer) is
  untouched; its saved group scenes appear in the outline like any other.
- Help: `programming-tab.md` is rewritten for the new layout (anchors kept
  where the sections survive), new help shots, `kTopics` and `help.qrc`.
- `programmingmanager_test` is rewritten alongside P1 (the canvas widgets it
  finds by name go); `showtreeview_test` gains the shared Library cases.

---

## 6. Phases

Sizes as the role doc: **S** ≈ a day, **M** ≈ 2–4 days, **L** ≈ a week.
Each phase ships on its own and leaves the tab working.

| Phase | Slice | Size |
|---|---|---|
| **P1** | **The outline.** Scenes outline replaces the function tree + the Targets/Looks canvas: folders → scenes → targets + palette rows (effects nested), Expand / Collapse all, ⌥-click subtree, multi-select, drag rows between scenes (move, ⌥ copy) and to reorder, F2, badges + hover. The Look Editor moves to the **page** (full height). Scene edits on the one history with redo. Supersedes feat/expand-all-resize. | L |
| **P2** | **The Library.** The Show Tree's Library lifted into a shared widget and used here (left), replacing the Palettes / Bundles / Fixtures & groups panes; Effects presets and Bundles as sections; layout flips to Library \| Outline \| Page. | M |
| **P3** | **Command bar.** Always open, shared grammar + scene and bench verbs, Tab completion, hint line. | M |
| **P4** | **Bench + palette page.** The bench node (P-D4), *Plays on*, *applies to N of M*, Used by on the page, **Open in Palettes** from the Show Tree (R2 leftover). | M |
| **P5** | **Stage bar + power** (R1 leftovers, P-D10): one app-level bar in Programming, Show Tree and Fixture Manager; the power chip owned by the app. The tab's top-bar part shrinks to four buttons. | M |
| **P6** | **Timing section**: per-palette fades, transition, speed master as one page section, reused by Show Tree looks and cue layers (R3 remainder), plus Fire as and a look's MIDI trigger there. | M |
| **P7** | **Capture ▾** (R4): into palettes (named per the show, filed in a folder), as a new scene (today's Snapshot), and in the tree as cue / look; the controller programmer's Save gains the target. | M |
| **P8** | **Tidy and rename**: retire the Scope combo, stock editors, group layout editor, followspot panel; the tab's name (P-D1, open question 2); help rewrite and shots; value-edit undo coalescing if not done in P1. | S–M |

Order: P1 → P2 → P3 → P4 can ship as the "flows like the tree" release;
P5–P7 close the role doc's remaining R-phases and slot wherever they are
wanted. Total ≈ 4–5 weeks of build time.

---

## 7. Options considered (mockups in the artifact)

- **Today** (with the stop-gap): one scene at a time on a canvas, function
  tree left, sources right, Look Editor under the canvas.
- **A — The Show Tree's twin** (recommended): §2. Fixes all three pain
  points, reuses the Library and command line, one way of working in both
  tabs.
- **B — Palette-first workshop** (PROGRAMMING_ROLE_DESIGN §3 as drawn):
  palette tree left, Look Editor centre, Used by + Try on right; scenes are
  a second **mode** with today's canvas. Strong for palette design, but
  scenes stay one-at-a-time behind a mode switch, so pain point 1 remains
  and the tab has two layouts to learn.
- **C — Scenes into the Show Tree**: the tab shrinks to Library + Look
  Editor; classic scenes become a "Scenes" section of each production in the
  tree. One place for everything, but it puts raw-value scenes into a tool
  built for tracking cue lists (P-D12 said no), makes scenes per-production
  when they are show-wide, and is the largest migration.

---

## 8. Open questions for Branson

1. **Names inside a scene.** Rows become **Targets** and **Palettes** (so
   "Look" means only the Show Tree's busk look). OK, or keep "Looks"?
2. **Tab name.** P-D1 said **Palettes**. With scenes in the outline too,
   keep *Palettes*, or *Programming* after all?
3. **"Looks" in your pain point:** whole scenes (expand several scenes and
   move palettes/groups between them) or the rows in one scene (open every
   effect's Colour/Dimmer)? The design does both; this sets which P1 polishes
   first.
4. **Drag between scenes moves, ⌥ copies** (as layers between cues). Or copy
   by default, since scenes are often siblings of one look?
5. **Non-scene functions** in a scene folder: greyed with *Open in
   Functions* (proposed), or hidden entirely?
6. **The bench**: saved with the show (proposed) or per session? One bench,
   or one remembered per palette type (movers for positions, washes for
   colours)?
7. **Operate mode**: editable without preview, with a banner (proposed), or
   read-only like the tree's Inspect?
8. **Stage bar placement**: its own strip under the top bar (proposed), in
   the top bar's right group, or in the status bar?
9. **Look Editor**: on the page at the right (proposed, more room), or
   under the page as in the Show Tree, for sameness?
10. **Library sharing**: one Pinned / Recent across both tabs (proposed), or
    each tab its own?
11. **Busk looks**: show the production's Show Tree Looks in the outline as
    jump-only rows, or keep them out entirely?
12. **feat/expand-all-resize**: merge as the stop-gap now (proposed), given
    P1 replaces it?
