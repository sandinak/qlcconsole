# Build, then schedule — one workflow from an old QLC+ show

*Status: proposal, 2026-10-22. Design only, nothing built. Builds on
PROGRAMMING_REDESIGN.md (option A, the outline), PROGRAMMING_ROLE_DESIGN.md
(P-D1…P-D16) and SHOW_TREE_DESIGN.md. Mockups (private):
https://claude.ai/artifact/P5VQ1n2pnrnr2xJvokxDbv*

Branson, 2026-10-22:

> *"this tool needs to be familiar and comfortable for qlc users with the
> 'old' ways of doing things and have a smooth transition to the 'newer'
> ways of doing things. The old programming tab kinda bridged that but
> inefficiently. Holistically I should be able to take an old showfile ..
> import it .. feel comfortable in the old tools with how it was generated
> .. and convert it to the new tools .. ( chaser -> Show tree ) kinda thing
> .. and have a clean work flow from there."*
>
> *"the show tree tab doesnt handle building effects well... if I have a
> collection in a chase .. I should be able to load it .. work on it ..
> add/remove efx, scenes, rgbscripts ..etc.. much like the programming tab
> did .. and/or .. if I have a cue in the show tree .. load it .. and work on
> the layers .. generate effects for a layer in detail etc."*

Earlier: *"we need a tab to build effects .. and then a tab to schedule
everything"*, leaning to **option 3**: one **Build** tab with sections
**Palettes | Effects | Scenes** (the outline only for scenes), and the Show
Tree as the schedule.

---

## 0. The proposal in brief

1. **Two working tabs, two jobs.** **Build** makes things: palettes,
   effects, scenes, collections. **Show Tree** puts them in order and runs
   them. Patch (Connections, Fixtures, Lighting Studio) and Run (Run,
   Virtual Console, Simple Desk) stay as they are.
2. **One editor for "a stack of things that play together": the
   Workbench.** It loads a scene, a collection, a chaser step, a Show Tree
   cue or a busk look and shows it the same way every time: layers you can
   open, add, remove and reorder, an editor for the selected row, and a
   live preview. A ↩ button takes you back to where you came from. Today
   the same job is spread over the Scene editor, the Collection editor, the
   Programming canvas and the Show Tree's layer page.
3. **Old objects stay old objects, and get a first-class home.** Build ▸
   Scenes has a **Classic** arrangement that lists the show by function
   type, as QLC+'s Function Manager does. An old show opens there, nothing
   converted, and every chaser, collection, EFX and RGB Matrix opens in
   the Workbench.
4. **Conversion is explicit, goes both ways, and you can check it.** Chaser
   → cue list (Import, then Convert to tracking) and cue list → chaser
   (Detach) exist already, and both play the same. What's new is the part
   around them:
   - a **show converter** that tells cue-list chasers from effect chasers;
   - **Import in place**, which files the originals away and re-points VC
     and show control;
   - a link from each list back to the chaser it came from;
   - per-object conversions (raw scene → palettes, EFX / RGB Matrix →
     effect palette), each offered only where it plays the same, or
     clearly marked as approximate.
5. **Effects get room.** Build ▸ Effects is where effects are designed:
   effect palettes, presets, `.qxfx`, WLED, and, listed beside them, the
   classic EFX and RGB Matrix functions. The Workbench's **Design effect**
   on any layer opens the same designer on just that layer's fixtures, so
   you can work on a cue's layer in detail.

---

## 1. Inventory: what is where today

Main at 178a2894a. Tabs are added in `ui/src/app.cpp:489-634` in this order.

### 1.1 Tabs and editors

| Place | Class / file | What it's for | Makes / edits |
|---|---|---|---|
| **Connections** | `InputOutputManager` | universes, outputs, inputs (upstream's Inputs/Outputs, moved first) | patch I/O |
| **Fixtures** | `FixtureManager` | patch, groups, group folders, head layout, inventory | fixtures, groups |
| **Lighting Studio** | `Monitor` | 2D/3D stage, targets; makes an **Aim** palette for a new target, but a **PanTilt** palette for a pasted one | targets, Aim/PanTilt palettes |
| **Functions** (no tab: Window menu, its own window — *"I keep hitting it by mistake instead of Programming"*, `parkContext`, app.cpp:634) | `FunctionManager` | the stock tree of all 9 types; Add, Wizard, Clone, Palettes… | every function type via the stock editors (below); palettes via `PaletteEditDialog` (6 types) |
| — stock editors | `SceneEditor`, `ChaserEditor` (+ cue no., sub-cues, speed master, GO lands, locked mode), `CollectionEditor` (+ speed master), `EFXEditor`, `RGBMatrixEditor`, `ScriptEditor`, `ShowEditor`, Audio/Video | the QLC+ way: raw channel scenes, step tables, member lists | functions |
| — Function Wizard | `functionwizard.cpp`, `palettegenerator.cpp` | scenes, chasers, RGB matrices from fixture capabilities (also from VC) | functions |
| — Live edit | `FunctionLiveEditDialog` (Live menu) | Scene / Chaser / EFX / RGB Matrix editor on a running function | functions |
| **Programming** | `ProgrammingManager`, `SceneGroupLooks`, `LookEditor` | scenes as targets + palette stacks; palettes, Bundles; stage tools toolbar (Highlight, Park, MIB, Beat, Snapshot) | scenes; and, via `loadFunctionEditor`, Collection / Chaser / EFX / RGB Matrix / Show (`ShowTimelineEditor`) in the canvas; 9 palette types (no Zoom) |
| **Show Tree** | `ShowTreeView`, `ShowTreeController` (+ `showtree*.cpp`) | productions → cue lists → cues → layers; tracking, includes, sub-cues, reusables, busk Looks; Library; command line; Import chaser…, Convert to tracking, Detach | cue lists, cues, layers, looks; 10 palette types (with Zoom); bakes hidden scenes/collections + a visible chaser |
| **Shows** | `ShowManager` | timelines (tracks, markers, timecode); hosts its own Scene and right-side editors | Show, Sequence(+Scene), Audio, Video |
| **Virtual Console** | `VirtualConsole` | buttons, faders, Cue List, Cue Runner widget, programmer buttons (SelectFixtures / SaveProgrammer) | VC; **SaveProgrammer** saves scenes (group scenes with palettes) |
| **Simple Desk** | `SimpleDesk` | channel faders; its own cue stacks of raw DMX (`CueStack`, not functions, not the tree's cues) | cue stacks |
| **Run** | `CueRunner` (Full) | the operator's GO view of the current production | — |

### 1.2 The same job in two or more places

| Job | Places today |
|---|---|
| **Edit a scene** | `SceneEditor` (Functions, Shows, Live edit) · Programming canvas (`SceneGroupLooks` + `LookEditor`) · Show Tree layer page (bakes to a hidden scene) |
| **Edit a collection** | `CollectionEditor` (Functions, Programming canvas) · a Show Tree cue *is* a collection, edited as layers |
| **Edit a chaser / cue list** | `ChaserEditor` (Functions, Programming, Shows) · Show Tree list · Simple Desk cue stacks |
| **Make an effect** | **three systems**: EFX functions (`EFXEditor`), RGB Matrix functions incl. RGB scripts (`RGBMatrixEditor`), effect palettes (JS effect scripts, `LookEditor` + `EffectBrowser` + `EffectScriptEditor`). Effect palettes can be made in both Programming and the Show Tree, but the Show Tree's tooltip sends you to Programming to design one |
| **Make / edit a palette** | Programming palette tree (9 types) · Show Tree Library (10 types) · `PaletteEditDialog` (6 types) · Lighting Studio (Aim or PanTilt) |
| **Capture the stage** | Programming Snapshot · Show Tree Record · Live → Capture (`LiveCaptureDialog`) · VC SaveProgrammer · Simple Desk Record cue |
| **Busk looks** | Programming scenes on VC buttons · Show Tree Looks section (Adopt as look between them) |
| **Show timelines** | Shows tab · `ShowTimelineEditor` inside Programming |
| **Undo** | Programming (scene snapshots + the shared history) · Show Tree controller · Shows |
| **The "is it generated?" lock** | repeated in `FunctionManager::editFunction`, `ProgrammingManager::loadFunctionEditor`, `ShowManager` (:802) |

### 1.3 Where the seams hurt

1. **An old QLC+ user can't find their show.** Functions has no tab. An
   old show's chasers, collections, EFX and RGB matrices are reachable
   only through Window → Functions or the Programming tree. The outline
   (feat/programming-outline) greys them out, with *Open in Functions*.
   Programming's placeholder still says *"open it in the Functions tab"*,
   but that tab no longer exists.
2. **There's no "open this and work on it".** A Show Tree cue edits on a
   narrow page, one layer at a time. An opaque layer (an EFX, an RGB
   Matrix, a raw scene, a nested collection) shows only *"Plays the
   function "X" as it is."*, with no way to open it (`showtreeview.cpp:3240`).
   A collection in a chaser opens in the stock `CollectionEditor`: a list
   of names, with nothing about what each member does on stage.
3. **Effects are scattered.** You design an effect palette in
   Programming, attach it in the Show Tree, and an EFX or RGB Matrix in
   Functions. There's no place to try an effect on one layer of a cue,
   in context, and tune it.
4. **Import copies, then the copies drift apart.** Import builds new
   layers and a new chaser, and the original chaser, collections and
   scenes stay with no link (`CueList` has no source id). Edit
   *BackStage* in Programming afterwards and the cue list doesn't change
   (palette edits still reach it; added groups and new rows don't). The
   show now has two "Full Show"s. The importer doesn't offer to move VC
   buttons over, although SHOW_TREE_DESIGN §7 planned it.
5. **Every chaser is treated as a cue list.** In QLC+ a chaser is used two
   ways: as a cue list (manual steps, infinite hold, notes) or as an
   effect (a looping strobe, a par chase). The importer lists both kinds
   the same, and a looping par chase makes no sense as a cue list.
6. **A stock show converts to opaque layers almost entirely** (§2.2).
   Stock scenes are raw channel values, so the tree can't open them up.
   Nothing offers to turn them into palettes, although the capture engine
   for that exists (`CaptureManager`, `buildPaletteFromBucket`).
7. **Three palette-type lists, two Look Editor instances, three undo
   stacks.** The Programming redesign's shared Library (P2) and the one
   history cover part of this.

---

## 2. The old-show journey, traced

Two real files, dry-run imported in memory on copies (the
`importARealShow` harness, `QLC_SHOWTREE_IMPORT`). The files were not
changed and no crash reports were written.

- **DanceHysteria-0.1.qlcc**: Branson's show before the Show Tree. Its
  "Full Show" chaser plays collections `0.0.1-Startup` … `0.0.7-STOMP`.
  Each holds an empty `0.0.0-base` collection plus scenes. The scenes are
  mostly groups + palettes; *Wash-Red*, *Wash-Base* and *Show* are raw
  values.
- **resources/samples/Sample.qxw**: stock QLC+ 4.0.2, Massimo's sample. It
  has 108 raw-value scenes, 13 looping chasers (*LST White strobo*, *Par
  chaser 1*, *Group chaser 1*…), 5 EFX (*SC Circle 1*…) and no groups.

### 2.1 Step by step (DanceHysteria-0.1)

| # | Step | What happens today | Feels |
|---|---|---|---|
| 1 | **File → Open** the `.qlcc` (or a `.qxw`) | Same loader for both (`App::loadXML`). Nothing converts. Old per-type palette folders (`Palettes/Color/…`) merge into one folder per name (`migrateLegacyPaths`). A plain Save writes back to a `.qxw` if you opened one, and the file then gains `<ShowTree>` once a tree exists. | fine |
| 2 | **Look at it the old way** | The show opens on whatever tab was current. **Functions isn't a tab.** Programming lists scenes and functions by type, and the outline branch greys out everything but scenes. Window → Functions shows the familiar tree: Chaser *Full Show*, Collections *0.0.x*, scenes in *Show*, *Spots*, *Washes*. | **foreign**: your show's spine is hidden |
| 3 | **Open a step to see what it is** | Double-click *0.0.4-SHOUT* in Functions: `CollectionEditor` lists *0.0.0-base, Spot-Red, Wash-Red*. To see what *Wash-Red* does you open it on its own (SceneEditor, raw channels) and lose the collection. | the old way, as it was |
| 4 | **Show Tree → Import chaser… → Full Show** | A new production *Full Show* with a Non-tracking list. **7 cues, 6 layers, 15 opaque layers**, 0 includes, 3 generated scenes. The opaque layers are: *0.0.0-base* ×7 ("played as it is (Collection)": it isn't a step itself, so it can't be an include); *Wash-Red* ×2, *Wash-Base* ×3 and *Show* ×3 ("has channel values of its own"). Plays the same: **0 channels differ** in any cue. The report is a message box with the detail folded away. | works; reads as a wall of ▣ |
| 5 | **Mix marking** | *New Chaser 8*'s cue *Red-Base*: two layers on overlapping head groups both set Colour/Dimmer/Position, so the import sets **Overlap: Mix** (`Cue::mixOverlap`, badge **≈ mix**). On the cue page: "Overlapping layers mix (as a chaser)". | correct, but the words are new to a QLC+ user |
| 6 | **Convert to tracking…** | Full Show: 6 cues track, 0 stay Block, 10 values and 2 layers taken out, 3 releases. For example *0.0.4 SHOUT: Wash-Red released (0.0.5 WHO ARE WE doesn't light it)*. **0 channels differ.** One undo step. | good: the plan reads well |
| 7 | **Keep working** | Cues are good to edit in the tree. **Opaque layers can't be opened.** *Wash-Base* still has to be found in Functions. The original *Full Show*, its collections and scenes sit beside the new list with no link, and VC buttons still fire the old chaser. Edit *Spot-Base* in Programming and the cue list doesn't change. | **breaks**: two copies of the show |
| 8 | **Go back to the old view** | **Detach to plain chaser…** hands the list back as an ordinary chaser + collections + scenes under `Productions/…`, one undo step. The Functions window shows the tree's chaser locked (steps and name), and other generated functions only as a banner with *Show in Show Tree*. | works, but it's a one-way door each time, and the original is a third copy |

### 2.2 The stock show (Sample.qxw)

- Every chaser imports, and **every layer is opaque**: 47 cues in 13 lists,
  **0 layers, 47 opaque layers**.
- None of the 13 is a cue list. All are `Loop` chasers used as effects
  (strobes, pan/tilt sweeps, par chases), so turning them into productions
  is the wrong move.
- *Group chaser 1–3* and *Par chaser 1–4* step through scenes with their
  own 1 s fade, and the chaser's Fade In mode is **Default** ("the
  function's own").
- **Finding:** the dry run reports **201 channels differ** at its sampling
  instant (chaser `u1/59: 245` vs list `108`). The list fades those
  scenes differently from how the chaser does. It needs a look (W0)
  before we tell stock users "it plays the same".

### 2.3 What the journey needs

- The old tree on show, as a place you work in rather than a window you
  dig for.
- An **Open** on anything opaque.
- Import that **knows which chasers are cue lists**, and that **takes
  over** from the original rather than copying it.
- A path for raw scenes into palettes, offered and checked, never forced.
- The round trip kept: Detach stays, and the classic view of a tree list
  is always there, read-only, with *Edit in Show Tree*.

---

## 3. The proposed model

### 3.1 Four places, four jobs

| Group | Tabs | Job | Changes |
|---|---|---|---|
| **Patch** | Connections · Fixtures · Lighting Studio | the rig | none here (Aim/PanTilt inconsistency is a separate fix) |
| **Build** | **Build** (today's Programming, renamed and widened) | make the vocabulary: palettes, effects, scenes, collections; the **Workbench** | absorbs the Functions window's editing and the Programming tab |
| **Schedule** | **Show Tree** · Shows | put things in order: cue lists, cues, tracking, busk looks; timelines | gains *Open in Workbench* everywhere; its page stays light |
| **Run** | Run · Virtual Console · Simple Desk | play it | none here |

The tab bar keeps its order. Programming becomes **Build**, with a thin
separator before Show Tree and after Shows, so the groups show. The
Functions window stays in the Window menu while Build ▸ Scenes ▸ Classic
grows to cover it, and retires after that (open question 4).

### 3.2 One vocabulary, both worlds

Branson asked whether old→new is one-way, two-way, or two views of one
model. **Answer: both ways, always explicit, with one editor across both
worlds.**

- The Show Tree already *stores* its work as classic functions: a chaser,
  hidden collections and hidden scenes, baked after every edit. So every
  tree list already has a classic view: read-only (D13), with *Edit in
  Show Tree*.
- What we add is a **single editor** (the Workbench), so working on a
  collection in a chaser feels the same as working on a cue in the tree.
  Then *which world it lives in* matters less day to day.
- Turning one world into the other stays a deliberate act, Import or
  Detach, each checked to play the same.

Live two-way sync is ruled out, for example editing a generated
collection in the Collection editor and having the tree absorb it.
Reverse-mapping arbitrary function edits into cues and layers can't be
done reliably, and it is what D13's lock protects.

| QLC+ object | Show Tree object | How they relate |
|---|---|---|
| Fixture group | Group | the same object |
| Palette | Palette | the same object, show-wide |
| **Scene of groups + palettes** | **Layer** (of a cue or a look) | the same shape. A layer bakes to exactly such a scene. Import reads one into a layer; Detach writes one back |
| Scene of raw channel values | **Raw layer** ▣ | referenced, played as it is. **Capture into palettes** on request turns it into a layer (palettes reused or made per the show's names), checked channel for channel; the original is kept |
| **Collection** | **Cue** (in a list) or **Look** (busk) | Import (as a step) / Adopt as look / Detach |
| Collection inside a step's collection | **Include** (when it is itself a step) | Import. When it isn't a step (DH-0.1's `0.0.0-base`): an opaque layer today. Proposed: **flatten** it into the cue's layers when it holds only layer-able scenes, and drop it when it's empty |
| **Chaser used as a cue list** (manual: infinite holds, notes, cue numbers, the main cue list, a VC Cue List) | **Cue list** | Import (+ Convert to tracking) / Detach |
| **Chaser used as an effect** (Loop, short or Common duration, raw steps, fired from VC buttons) | stays a **function**, played as a ▣ layer wherever used | not imported as a list. Later, if wanted: a *Step chase* effect palette |
| **EFX** | ▣ layer, or a **movement effect palette** (Orbit, Figure 8, Ballyhoo…) | *Convert to effect…* on request, marked **approximate** (the shapes differ); the original is kept |
| **RGB Matrix** (built-in algorithm or RGB script) | ▣ layer, or a **matrix effect palette** where one matches (Matrix Bars, Wipe, Sparkle…) | as EFX: on request, approximate, original kept |
| Show (timeline), Sequence, Script, Audio, Video | stay in Shows / as functions | none |
| Simple Desk cue stack | — | out of scope |

### 3.3 Build: Palettes | Effects | Scenes (option 3)

```
┌ Build ─ [ Palettes | Effects | Scenes ]  + New ▾  Capture ▾  ⟲ ⟳ │ show controls … DESIGN ┐
├ LIBRARY / NAVIGATOR ─┬ WORKBENCH ──────────────────────────────┬ EDITOR ───────────────┤
│ (by section)         │ ↩ Show Tree · DH › 00-Preshow › 0.0.3   │ the selected row:     │
│ Palettes: folders    │ ◀ 0.0.2   0.0.3 Announcer   0.0.4 ▶     │ Look Editor / effect  │
│ Effects: effect pals,│ fade 1.0s · hold ∞ · note "Listen for…" │ designer / EFX editor │
│  presets, EFX, RGB M │ ▾ ◆ AT-Spots      Colour White · Dim 100│ / RGB Matrix editor / │
│ Scenes: the outline  │ ▾ ◆ AT-W-Bottom   Blue · Main Drape aim │ channel console       │
│  (Folders | Classic) │   ✦ Stage shape -circle  (own: 45%)    │                       │
│ Groups · Fixtures    │ ▸ ▣ New RGB Matrix 17   (opaque, Open)  │ ▶ On the rig: 0.0.3 … │
│                      │ + Layer  + Scene  + EFX  + RGB Matrix   │                       │
├──────────────────────┴─────────────────────────────────────────┴───────────────────────┤
│ Command  AT-Spots effect Ballyhoo · Tab completes                                        │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

- **The section switch** changes only the left-hand navigator and what is
  listed. The Workbench and the editor are the same in every section.
- **Palettes.** Folders of any type, as today and as in the Show Tree's
  Library: one class shared by both tabs (PROGRAMMING_REDESIGN P2). Select
  a palette and the Workbench loads the **bench**: the palette playing on
  *Try on* groups (P-D4), with *Used by* under it.
- **Effects.** Three lists in one place:
  - effect palettes, in their folders;
  - **presets** (browser categories, Recent, WLED);
  - **classic effects**: EFX and RGB Matrix functions, each marked as one.

  Select one and it plays on the bench in the designer. **+ New ▾** offers
  Effect palette, EFX and RGB Matrix, and says which world each belongs
  to.
- **Scenes.** The outline from feat/programming-outline is the navigator,
  in two arrangements:
  - **Folders** (the default for a show with a Show Tree): your scene
    folders, scenes, and now **collections and chasers as full rows**,
    not greyed;
  - **Classic** (the default for a show without one): by type, Scene ·
    Chaser · Collection · EFX · RGB Matrix · Show · Script…, folders under
    each, as QLC+'s Function Manager lists them.

  Clicking a row loads it in the Workbench. Show, Script, Audio and Video
  open their stock editor in the editor pane (Show: *Open in Shows*).
- **Groups / Fixtures** sit at the bottom of the navigator in every
  section, as drag sources.
- **Command bar** and **one undo history**, as in the Show Tree.

### 3.4 The Workbench

**What it loads, and what you see:**

| Loaded | Header | Rows (layers) | Notes |
|---|---|---|---|
| a **Scene** (groups + palettes) | name, folder, fades, transition, speed master, *Fired by* | one layer: its targets and palette rows | today's canvas, as one layer |
| a raw **Scene** | as above | one ▣ raw layer: the channel console | *Capture into palettes…* |
| a **Collection** | name, *Used in* (chasers/steps, VC) | one layer per member: a group scene opens up into targets + palettes; a raw scene, EFX, RGB Matrix, chaser or nested collection is a ▣ row with **Open** | **+ Scene / + EFX / + RGB Matrix / + Collection / remove / reorder** |
| a **chaser step** | the chaser, **◀ prev · step N · next ▶**, fade in / hold / fade out / note / cue no. | the step function's layers (a collection as above) | walk a chaser the way the Chaser editor lists it, with each step opened up |
| a whole **Chaser** | the step table (the Chaser editor's columns) | the selected step's layers under it | *Import as cue list…* (or *Edit in Show Tree* for a generated one) |
| a **Show Tree cue** | cue no., name, mode, fades, includes, ≈ mix | its own layers, plus **grey tracked-in rows** (*⤳ from 0.0.2*), cue looks | edits go through `ShowTreeController`, one undo step each; ↩ Show Tree |
| a **Show Tree look** | name, Fire as, *Fired by* | its layers | as a cue without tracking |
| a **reusable layer** | *used in N places* | its rows | an edit is an edit to all |

**Behaviour, the same for every kind:**

- **Rows.** A layer row expands into target rows (◆ group, ◇ fixture) and
  attribute rows (the Show Tree's: Colour, Dimmer, Position, Beam family,
  Shutter, one per effect kind, with an effect's own colour/dimmer nested
  under it). Grey rows say where a value comes from, as in the tree.
- **The editor pane** edits the selected row, at full height:
  - Look Editor for a palette;
  - **effect designer** for an effect;
  - `EFXEditor` / `RGBMatrixEditor` for those functions;
  - the channel console for raw values;
  - the member list for a group.
- **Add / remove / reorder.** Drag from the Library or use the **+** row:
  + Layer (groups), + Scene (existing), + EFX / + RGB Matrix (**new**,
  made on the selected layer's groups and opened in the editor), + Effect
  (an effect palette on the selected layer). ⌫ removes a row. Removing a
  member from a collection doesn't delete the function, as *Remove from*
  doesn't today.
- **Shared members.** A scene in several collections (*BackStage* is in
  0.0.3 and 0.0.4), or a reusable layer, carries a badge: **also in 0.0.3
  Announcer**. An edit changes every place that uses it. **Make this
  step's own copy** splits it off, as the tree's *Unlink here*. (Open
  question 7.)
- **Preview.**
  - In Design, what's loaded plays: a cue **in context** (what tracks in
    plays too, and the selected layer is highlighted), or **Solo** (only
    the selected layer).
  - In Operate nothing previews, as everywhere else.
  - The *On the rig* strip says what's playing, with Stop.
- **Return.** **↩** goes back to where you came from (the cue in the Show
  Tree, the step in the chaser), with its selection kept. Edits are live
  and on the one history, so there is no Save / Cancel to remember.
- **Opaque rows open.** **Open** on a ▣ row loads that function into the
  Workbench, or its stock editor into the editor pane. That covers every
  ▣ layer in the tree.

**Designing an effect for one layer** (*"generate effects for a layer in
detail"*):

1. In the Workbench, select a layer (*AT-W-Bottom* in 0.0.3 Announcer) and
   choose **Design effect**, or select its effect row.
2. The editor pane becomes the **effect designer**. It's the same one as
   Build ▸ Effects, with:
   - the browser (presets, Recent, WLED);
   - parameters, Spread, speed master, on the beat;
   - the effect's own colour and dimmer.

   It previews on **just that layer's fixtures**, over the rest of the
   cue (in context) or alone (Solo).
3. Keep it **as a one-off on this layer** (an inline palette, as the tree
   makes for a picked value) or **Save as palette…** (named per the show,
   filed, reusable). *Save as preset* makes it a starting point for other
   shows.
4. ↩ to the Show Tree. The cue's Effect row now names it.

**Under the hood: one widget, two back ends.**

- `FunctionStack` edits real functions: a Scene / Collection / Chaser
  step, through `Doc`, `ProgrammerController` and `PaletteOps`.
- `CueStack` edits the tree's model through `ShowTreeController`. Every
  edit is one tree step, then a re-bake, so generated functions stay
  locked (D13).
- The rows, editor pane, preview and history are shared.
- The row model already exists in two shapes: the outline's
  `programmingoutline.cpp` and the tree's `showLayerPage` attribute rows.
  The Workbench's row model is the merge of the two.

### 3.5 The bridge: an old show, start to finish (proposed)

1. **Open** the `.qxw` / `.qlcc`. Build opens on **Scenes ▸ Classic**: the
   show by function type, as QLC+ listed it. Nothing is converted.
2. A one-line **banner**: *"Made in QLC+ 4.x: 2 chasers look like cue lists
   (Full Show, New Chaser 8). Review…"*. You can dismiss it, and it comes
   back from Build's ⋯ menu.
3. **Work the old way, better.** Click *Full Show* and the Workbench walks
   its steps, each collection opened into layers. Add an EFX to
   *0.0.4-SHOUT*, remove *Wash-Red*, retime the fade. Everything stays a
   chaser of collections, and plays and saves as QLC+ would.
4. **Show converter** (Review…). One sheet, one row per chaser:
   - **Cue list** (manual holds, notes, cue numbers, main cue list, a VC
     Cue List): *Import as cue list*;
   - **Effect** (loop, short holds, raw steps, fired from VC buttons):
     *keep as a function*. It plays as a ▣ layer wherever you drop it.

   Per row: the import plan (cues, layers, includes, mixes, opaque rows
   and why), and **"plays the same: 0 channels differ"** from the same
   check the dry run uses, or the cues that don't.
5. **Import in place** (the default; open question 5) does three things:
   - makes the list as today, and **links it to its source** (`Source=`
     on `<CueList>`);
   - **re-points** VC buttons, VC Cue Lists and show control from the
     chaser to the list's chaser;
   - **files the originals** (the chaser, plus collections and scenes
     only it used) into `Imported/Full Show (originals)`, each marked
     *"Imported into Show Tree — editing here doesn't change the cue
     list"*.

   It is one undo step. *Keep both* (today's behaviour) stays as an
   option.
6. **Raw scenes.** The import's opaque rows say what to do next:
   - *Wash-Red: raw values* → **Capture into palettes…**: the plan lists
     palettes reused / made and any leftovers that stay raw, checked
     channel for channel, one undo step.
   - Empty or layer-only nested collections (*0.0.0-base*) are flattened
     or dropped at import, so DH-0.1's 15 opaque layers become 8 that
     really are raw scenes.
7. **Convert to tracking…**: unchanged, already good. The **≈ mix** badge
   gains words a QLC+ user knows: *"plays both scenes, as the chaser did
   (highest intensity, last colour)"*.
8. **Keep working.**
   - The Show Tree for order, tracking, cues and busk looks.
   - ✎ / double-click a cue or layer → **Open in Workbench** (Build),
     detailed work, ↩ back.
   - The tree's page stays as it is for quick edits.
9. **Going back** stays possible:
   - the classic view of the list's chaser (read-only, *Edit in Show
     Tree*);
   - **Detach to plain chaser** for good;
   - **Restore original** from the import link, which re-points VC back,
     for an import you regret.

### 3.7 QLC+ power users: their two habits, kept and carried forward

The old ways stay first-class. Nothing nags a user to convert, nothing
converts on load, and a show built only the old way keeps working forever.
The new tools are a path forward, not a gate.

**Habit 1: set the rig, save a scene, chase the scenes, run.**

| Step | Today | Proposed |
|---|---|---|
| Set the rig | fixture console, Simple Desk, Programming stage tools | same, plus the Workbench's channel console |
| Save the look | five captures: Programming Snapshot, Show Tree Record, Live → Capture, VC SaveProgrammer, Simple Desk Record | **one Capture ▾** in Build, with a destination: *New scene* (raw values, exactly as QLC+ saves it), *Update scene*, *Palettes*, *Cue in the Show Tree* |
| Chase the scenes | Function Manager → new chaser → add steps | Build ▸ Scenes ▸ Classic: select scenes → **Chaser from selection** (order = selection), or drag them onto a chaser; the Workbench walks its steps |
| Run | VC button / cue list, Run | unchanged |
| Step forward | — | **Send to Show Tree** on the chaser (Import in place) when ready |

The new-world twin of habit 1 is the Show Tree's **Record**: set the rig,
press Record, get the next cue. Same gesture, but the cue tracks and its
layers stay editable.

**Habit 2: build scenes, gather them into collections with EFX and RGB
Matrix, run.**

| Step | Today | Proposed |
|---|---|---|
| Make scenes | Scene editor / Programming canvas | Build ▸ Scenes, the Workbench |
| Assemble | Collection editor, a list of function names | load the collection in the Workbench: each member is a layer, **+ Scene / + EFX / + RGB Matrix**, reorder, live preview of the whole |
| Effects | EFX editor, RGB Matrix editor | the same stock editors in the Workbench's editor pane, or **Design effect** on a layer |
| Run | VC buttons, chasers of collections | unchanged |
| Step forward | — | a collection becomes a **cue** (members → layers) or a **busk look** (a VC button that fires it); a chaser of collections becomes a cue list |

**Taking existing work forward** (a selling point; can wait): an
**Upgrade this show…** wizard over the W5 pieces. It runs the show
converter, lists each chaser and collection with its plan (cue list,
effect chaser kept as a function, look, leave alone), offers Capture into
palettes for raw scenes, runs the same-output check, and applies the plan
as one undo step with Restore original. Every part is optional and the
show still plays the old way at every point.

### 3.6 Option 3, option A and the outline: what to keep, change, drop

| Piece | Verdict |
|---|---|
| **Option 3** (Build = Palettes \| Effects \| Scenes; Show Tree = schedule) | **Taken.** The sections are navigator modes over one Workbench, not three layouts. "Outline only for scenes" holds: Palettes and Effects use a plain filtered tree |
| **Option A's Library \| Outline \| Page** | **Kept, re-cast**: the Library and the outline become the left navigator by section; the Page becomes the Workbench + editor pane. The shared Library (P2), command bar (P3), bench (P4), Stage bar (P5), timing section (P6) and Capture (P7) all still apply |
| **Outline branch: navigator tree** (folders → scenes → rows, ⊞/⊟, ⌥-click subtree, remembered expansion, badges ⚠ VC ▣ ⇄ ⏱ ◷, drag rows between scenes, move / ⌥ copy) | **Keep.** It becomes Build ▸ Scenes ▸ Folders |
| **Outline branch: greyed chasers / collections with *Open in Functions*** | **Drop.** They become full rows that load in the Workbench (reverses P-D12, open question 3) |
| **Outline branch: the page** (scene page, look page at full height, group page with channels) | **Change**: it's the first cut of the Workbench + editor pane for one scene. Grow it to collections, steps and cues rather than rewrite it |
| **Outline branch: `PaletteOps` additions, tests** | **Keep** |
| **Right-hand Palettes / Bundles / Fixtures & groups panes** | **Drop** in favour of the shared Library (Bundles as a Library section) |
| **feat/expand-all-resize** (`splitterhandles.cpp`, Expand / Collapse all in Show Tree and Programming) | **Merge now** for the Show Tree half and the splitter handles, which Build reuses. Its Programming half is superseded when W1 lands |
| **PROGRAMMING_REDESIGN's open question 1** (rows named Targets / Palettes, so "Look" means only a busk look) | Still stands. In the Workbench the rows are **layers**, **targets** and **attribute rows**, the Show Tree's words, for collections too |

---

## 4. Phases

Sizes: **S** ≈ a day, **M** ≈ 2–4 days, **L** ≈ a week. Each ships on its
own and leaves the app working.

| Phase | Slice | Size | Reuses |
|---|---|---|---|
| **W0** | **Seam fixes, no new layout.** **Open** on an opaque layer (loads the function's stock editor; `FunctionManager::editFunction`). The import report as a readable sheet. Offer to re-point VC on import. Stale "Functions tab" text. Look into Sample.qxw's 201-channel difference (Default fade mode under a chaser). | S–M | existing editors |
| **W1** | **Build tab + Workbench (scenes and collections).** Rename Programming → Build. Section switch (Scenes first). The outline as the navigator. Workbench with the `FunctionStack` back end: scene and collection, add / remove / reorder members, + EFX / + RGB Matrix, ▣ Open, editor pane hosting LookEditor / channel console / EFX / RGB Matrix editors, preview, ↩. | L | outline branch (navigator, rows, drags, badges, tests), `SceneGroupLooks` model, `LookEditor`, splitter handles |
| **W2** | **Chasers in the Workbench + Classic arrangement.** Step walker (◀ ▶), step header (fades, hold, note, cue no.), the step table for a whole chaser; shared-member badge + *Make own copy*. Scenes ▸ Classic (by type) as the default for shows without a tree. | M | `ChaserEditor` columns / delegates |
| **W3** | **Show Tree ↔ Workbench.** `CueStack` back end; *Open in Workbench* on cue / look / layer (✎, double-click, `wb` command); grey tracked rows; in-context / Solo preview; ↩ with selection kept. | M–L | `showLayerPage` attribute rows, tree preview rules |
| **W4** | **Effects section + layer effect designer.** The bench (Try on), the designer at full height, one-off vs *Save as palette*, *Save as preset*. EFX and RGB Matrix listed as classic effects, made from + New. | M | `LookEditor` effect parts, `EffectBrowser` |
| **W5** | **The bridge.** Open banner. Show converter (chaser classification, per-chaser plan + same-output check). Import in place (source link, re-point VC / show control, file originals, Restore original). Flatten nested non-step collections. *Capture into palettes* for a raw scene. | M–L | `importChaser`, `dryRunImport` check, `CaptureManager`, `adoptAsLook` re-point code |
| **W6** | **Shared Library + command bar in Build** (= PROGRAMMING_REDESIGN P2 + P3); Palettes section on the shared Library. | M | Show Tree Library, `ShowTreeCommand` |
| **W7** | **Conversions and tidy.** EFX → movement effect, RGB Matrix → matrix effect (approximate, on request). Retire or keep the Functions window (open question 4). Help: `build.md` replaces `programming-tab.md` (anchors redirected); show-tree.md gains the Workbench; new help shots; `helpbrowser_test`. | M | — |

Order: **W0 → W1 → W3 → W2 → W4 → W5 → W6 → W7.**

- W0 helps today's users at once.
- W1 + W3 give Branson both halves of "load it and work on it": a
  collection in a chase, and a cue in the tree.
- W5 is the full old-show bridge, and it lands once the Workbench exists
  for the bridge to land in.

Total ≈ 5–6 weeks of build time.

---

## 5. File format and migration risk

- **W1–W4 change no file format.** The Workbench edits existing objects:
  scenes, collections, chaser steps, and the tree's cues through the
  controller. Build ▸ Scenes ▸ Classic / Folders is a view setting
  (QSettings, per show).
- **W5 adds:**
  - `Source="<chaser id>"` on `<CueList>`;
  - an `Imported/…` folder path on the originals;
  - an `ImportedInto=` mark on them.

  Older qlcconsole builds ignore all three. Stock QLC+ 4.14 already can't
  play palettes or the tree, as today.
- **Re-pointing VC and show control** rewrites function ids in VC widgets.
  It is one undo step, and *Restore original* re-points back. Risk: a VC
  widget that fires the chaser by a path the walker doesn't know
  (`adoptAsLook` already walks VC for this; reuse it).
- **Flattening** a nested non-step collection changes the cue's structure,
  not its output. The same `same()` check as Convert to tracking guards
  it, and the plan lists it.
- **Capture into palettes** makes new palettes. It's checked channel for
  channel, and leftovers stay raw. The original scene is kept, so this is
  safe by design.
- **EFX / RGB Matrix → effect palette** can't be proven the same (different
  engines). It is marked approximate, offered only on request, and keeps
  the original. Nothing ever converts on load.
- **Sample.qxw's 201 channels** must be understood before the converter
  says "plays the same" to stock users (W0).
- **Generated functions stay locked (D13).** The Workbench edits a tree cue
  only through the controller, so there is no path that edits a
  generated function directly.
- **Tests.**
  - `programmingmanager_test` → `build_test` (outline branch's
    `programmingoutline_test` carries over).
  - A new `workbench_test` for both back ends.
  - `showtree_test` gains import-in-place / flatten / restore cases.
  - `importARealShow` runs on DanceHysteria-0.1 and Sample.qxw copies in
    the gate, behind its variable, as now.

---

## 6. Not in scope

- Live two-way sync between a generated function and the tree (§3.2).
- Simple Desk cue stacks.
- Shows timelines (they stay in Shows).
- Blind (P-D15).
- Converting anything on load.
- A new VC widget.

---

## 7. Open questions for Branson

Each has a recommended default: `ok` takes it.

1. **Tab name.** **Build** (default), or keep *Programming*, or
   *Palettes* (P-D1)? With collections and chasers in it, *Palettes* is
   too narrow.
2. **Where the Workbench opens from the Show Tree.** **Switch to Build with
   the cue loaded and ↩ back** (default: room to work, one editor), or dock
   the Workbench inside the Show Tree's page area?
3. **Chasers and collections in Build ▸ Scenes** as full rows that load in
   the Workbench. This reverses P-D12 (*Scenes only*). **Yes** (default),
   or keep them greyed with *Open in Functions*?
4. **The Functions window.** **Keep it in the Window menu until Scenes ▸
   Classic covers it (W2), then retire it** (default); or keep it for
   good; or bring it back as a tab?
5. **Import in place.** **File the originals under `Imported/…`, re-point
   VC and show control, link the list to its source** (default); or keep
   both side by side as today; or delete the originals after import?
6. **Effect-style chasers** (loops, strobes, par chases). **Keep them as
   functions, played as ▣ layers; the converter doesn't offer them as cue
   lists** (default); or import them anyway?
7. **A shared member edited in the Workbench** (*BackStage* in 0.0.3 and
   0.0.4). **Edit it everywhere, with an "also in 0.0.3" badge and *Make
   this step's own copy*** (default); or always copy on first edit?
8. **Preview of a cue in the Workbench.** **In context** (what tracks in
   plays too, the selected layer highlighted), with a **Solo** toggle
   (default); or Solo by default?
9. **An effect designed for one layer.** **Kept as a one-off on the layer,
   with *Save as palette…*** (default); or always a named palette?
10. **EFX / RGB Matrix → effect palette.** **Offer only where a matching
    effect exists, marked approximate, keep the original** (default); or
    never convert, only list them under Effects?
11. **Raw scenes.** **Capture into palettes on request, from the opaque row
    or the converter** (default); or offer it for every raw scene during
    import?
12. **The outline branch.** **Don't merge as is; carry its navigator and
    page into W1** (default). **feat/expand-all-resize: merge now** (Show
    Tree half and the splitter handles). OK?
13. **One vocabulary.** A collection's members are shown as **layers**,
    with the Show Tree's attribute rows (default), or as *members*, the
    QLC+ word?
14. **Opening an old show.** Build ▸ Scenes ▸ **Classic** by default for a
    show with no Show Tree, and Folders once it has one (default); or
    always Folders?
15. **Order.** **W0 → W1 → W3 → W2 → W4 → W5 → W6 → W7** (default), or the
    bridge (W5) before the Effects section (W4)?

---

## 8. Mockups

Interactive mockups (DanceHysteria's real names, Blue theme):

- today's scattered state vs the proposed model;
- the old-show journey;
- the Build tab by section;
- the Workbench with a collection from *Full Show* loaded;
- the Workbench with a Show Tree cue's layer and its effect designer.

https://claude.ai/artifact/P5VQ1n2pnrnr2xJvokxDbv (private)
