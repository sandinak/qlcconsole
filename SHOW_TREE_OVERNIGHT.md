# Show Tree — overnight build handoff (2026-10-16 → 17)

Branch **`feat/show-tree`** (worktree `~/git/qlcconsole-showtree`), pushed to
origin, **not merged**. Design: SHOW_TREE_DESIGN.md — §4 has the spike
result, **§10 the implementation notes and "Open for Branson"**.

## Spike result (D3) — baking works, with one engine fix

As it was, a Scene shared by two consecutive cue Collections **restarted on
GO** (stopped, `elapsed()` back to 0, a fresh fade-in, effect instances torn
down): the old Collection's `postRun()` let go of it before the new one's
`preRun()` (MasterTimer start queue, after the stop loop) could claim it —
reordering inside `ChaserRunner::write()` alone could not fix that.
**Fix (ChaserRunner, pre-approved):** a leaving Collection step that shares a
member with the new step (at any depth, or one holds the other) is stopped on
the next write, one tick later; everything else stops as before.
`Collection` gives back its intensity override to a member that runs on, and
reads its member list under its lock. Proven in `engine/test/cuehandover`
(GO, Back, jump, auto-follow, nested collections: no stop/start signal,
elapsed climbing, channel at full every tick). Side benefit: today's
DanceHysteria collections-of-collections stop re-fading too.

## What was built (commits, oldest first)

| Commit | What |
|---|---|
| 6cc06e27a | Cue hand-over fix + `engine/test/cuehandover` (the spike) |
| ea8c14e0b | Engine: `ShowTree` model/cascade/numbering/XML, `ShowTreeController` (bake, undo, inline palettes, import), `Doc::showTree()`, §10 notes, `engine/test/showtree` |
| 83dc8cd5b | Show Tree tab (`ui/src/showtreeview`), help `show-tree.md`, what's new, Window shortcuts, `ui/test/showtreeview` |
| e454b1508 | Effect's own looks, move cues; TODO/DONE |
| b34443257 | Undo/redo keeps a list's chaser settings |
| b1ef3e695 | Drag a cue to move it, ⌥ to include |
| 8495b701d | Setting a value on a carried-in layer is one step; safer row rebuild |
| 2df3de2c4 | `Collection::components()/contains()` read under the lock |
| c2ab0a290 | SHOW_LIFECYCLE: phase "Production" → "Run" (D1) |
| 805db90bc | Look Editor docked under the tree |
| 3aa52d609 | "applies to N of M" hover lists the fixtures that ignore it |

P1 per §8: tab, Non-tracking lists, cue = Collection, layer = shared Scene,
cue looks cascading, attribute rows with inherited values greyed + source,
Beam family rows + "applies to N of M", inline looks, effects with own
looks, opaque function layers, **Import chaser**, + Cue / + Layer, picker,
drag & drop with hints, undo, Library Recent / Pinned / All. Beyond P1 for
free: the resolver already does tracking, Block / Cue-only, Release to the
base cue and includes, and the tab exposes them.

## DanceHysteria-0.5 dry run (a copy, in memory only)

Full Show: 6 cues, 9 layers, **0 opaque**, 3 includes, **7 generated
scenes**, 1 HTP conflict (0.0.5: StepTops set by 0.0.4's Dim and its own
Bright — Bright is brighter, so nothing changes on stage). Played side by
side cue by cue, every channel of every universe: **0 channels differ** in
all 6 cues (static looks; effects not compared). New Chaser 8: 2 cues, 3
layers, 2 opaque (scenes with no looks). Details in §10.6.

## Test / gate status

- **`./check-all.sh`** (Qt6 / Qt6-Release / Qt6-Werror, run with JOBS=5 on
  a `git archive` of the commit in the scratchpad so the worktree could keep
  moving):
  - on **ea8c14e0b** (spike + engine a–c): **all three legs PASS**
    (396 s / 412 s / 428 s).
  - on **e9567719c** (everything through the docked Look Editor and the
    reach hover; only this file changed since): **all three legs PASS**
    (whole gate 1295 s).
- New tests: `cuehandover_test` 11/11, `showtree_test` 26 + 1 opt-in skip,
  `showtreeview_test` 18 + 1 opt-in skip; chaser, chaserrunner, collection,
  sub-cues, show control, programmingmanager, appviews, helpbrowser all green.
- **No crash reports** (`~/Library/Logs/DiagnosticReports`, nothing new
  since the run began). One environmental stall: `qlcfixturedefcache_test
  storeDef()` sat in `sync()` for minutes while Time Machine was backing up
  over SMB; it finished on its own.
- **CI** (sandinak/qlcconsole, push builds): spike commit **all green**;
  engine and later commits — macOS, Linux and Linux-coverage **green**,
  Windows still queued/running when this was written (check
  `gh run list -R sandinak/qlcconsole --branch feat/show-tree`).

## What's left of P1

- **Gobo palettes by name per fixture definition** (§3A) — approach written
  (§10.7: manual map + name match), not built; waiting on you.
- An edit to the cue **on stage** shows on its next GO (open question).

## Open for Branson (also §10)

1. `Generated` mark on `<Function>` too (built: listed in `<ShowTree>` only)?
2. Editing the cue on stage: live, or next GO (built: next GO)?
3. Undo shared with Programming? Should tree undo cover Look Editor edits?
4. Gobo mapping approach, and an effect's own Gobo (needs the stack rule changed).
5. DanceHysteria 0.0.4–0.0.7 have Hold 0, so they auto-follow as one
   sequence and loop back to 0.0.2 — intended?

## How to try it

Offscreen tests (from the worktree, after `cmake --build build -j5`):

```sh
cd build/engine/test/cuehandover && DYLD_FALLBACK_LIBRARY_PATH=../../src ./cuehandover_test
cd build/engine/test/showtree    && DYLD_FALLBACK_LIBRARY_PATH=../../src ./showtree_test
cd build/ui/test/showtreeview && QT_QPA_PLATFORM=offscreen \
  DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
  ./showtreeview_test.app/Contents/MacOS/showtreeview_test -platform offscreen
# dry-run the import on a COPY of a show:
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-copy.qlcc
cd build/engine/test/showtree && QLC_SHOWTREE_IMPORT=/tmp/dh-copy.qlcc \
  DYLD_FALLBACK_LIBRARY_PATH=../../src ./showtree_test importARealShow -maxwarnings 0
# a picture of the tab on that copy:
cd build/ui/test/showtreeview && QLC_SHOWTREE_SHOW=/tmp/dh-copy.qlcc QLC_SHOWTREE_SHOT=/tmp/st.png \
  QT_QPA_PLATFORM=offscreen DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
  ./showtreeview_test.app/Contents/MacOS/showtreeview_test pictureOfARealShow -platform offscreen
```

The app, on a **copy** (it patches the show's outputs as usual, so DMX goes
out if the rig is on the network — nothing tonight launched it):

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-showtree.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-showtree.qlcc
```

Then **Show Tree** tab (⇧⌘6) → **Import chaser… → Full Show**, open
0.0.4 SHOUT, click a layer; ✎ on a row opens the Look Editor, ⋯ sets values.

---

## Fix pass (2026-10-17)

Branson tried P1 and asked for four fixes, all approved. Same branch
(`feat/show-tree`), pushed, **not merged**. Notes: SHOW_TREE_DESIGN.md
§10.8; §10 "Open for Branson" updated (2 resolved, 2 new).

| Commit | What |
|---|---|
| 4327af954 | **Selecting a cue plays it on the rig** (Design mode): preview of the selected cue (or a layer in it), re-taken after every bake so edits show at once; switches with the selection; stops on leaving the tab / selecting a list or nothing / going Live. Live (Operate) is blind until GO. Not previewed while its own list runs for real; gives way when the list starts, comes back when it stops. Header label says what's on the rig. |
| 102afcb19 | **Several groups in one layer**: a multi-group drop is one layer (⌥ = a layer each), groups dropped on a layer join it, "Remove X from layer"; hints say which. A layer stays one layer when its groups change (`Layer.continues`, XML `ContinuesGroups/ContinuesFixtures`). |
| a1ee9581d | **Library groups in the Fixture Manager's folders**, nested; open folders kept by path; filter works at any depth and on folder names. |
| f13f79ff4 | **Gobo palettes by name per fixture type** (§10.7): `GoboName` + per-type picked slots; Look Editor names the look from a pick and lists what each type plays; "applies to N of M" counts it. Unnamed gobo looks play as before. |
| 253bc2632 | Help (show-tree.md, what's new), TODO/DONE, design §10. |

How the preview is built (the part to read): it runs the cue Collection's
*members* under its own FunctionParent, not the Collection, so a scene two
cues share keeps running as the selection moves (no re-fade), a re-bake is
a start/stop diff (no restart), and stopping it never stops what a chaser
or VC widget started. The power chip already reads what's lit in Design on
any tab, so it covers the preview with no change; Blind mutes it.

### Gate and CI

- **`./check-all.sh`** (JOBS=5, on a `git archive` copy in the scratchpad):
  - on **4327af954** (fix 1): Qt6 **PASS** (402 s), Qt6-Release **PASS**
    (369 s), Qt6-Werror **PASS** (399 s); whole gate 1170 s.
  - on **253bc2632** (all four fixes + docs): Qt6 **PASS** (384 s),
    Qt6-Release **PASS** (351 s), Qt6-Werror **PASS** (392 s); whole gate
    1127 s.
- New/changed tests: `showtree_test` 35 + 1 opt-in skip (preview ×5, layer
  targets ×3, gobo by name), `showtreeview_test` 22 + 1 opt-in skip (preview,
  drops on a layer and ⌥, Library folders, gobo in the Look Editor);
  qlcpalette, programmercontroller, scene, lookeditor, programmingmanager,
  helpbrowser green. DanceHysteria-0.5 import dry run (a copy): still **0
  channels differ** in all 8 cues.
- **No crash reports** in `~/Library/Logs/DiagnosticReports` from these runs.
- **CI** run 39024589457 (253bc2632): macOS and Linux **green**; Linux
  coverage and Windows were still running when this was written — check
  `gh run list -R sandinak/qlcconsole --branch feat/show-tree`.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-fix.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-fix.qlcc
```

(It patches the show's outputs as usual: DMX goes out if the rig is on the
network. Use Blind if you only want the 2D view.)

1. **Preview:** Show Tree tab (⇧⌘6) → Import chaser… → Full Show. In
   Design mode click 0.0.3, then 0.0.4: the rig (or the 2D monitor) follows,
   the header says "▶ On the rig: 0.0.4 SHOUT". Drop a palette on a layer:
   it shows at once. Click the list row: preview stops. Switch to another
   tab: stops. Go to Operate and click cues: nothing changes on stage, the
   header says Live is blind. GO the list from the footer in Design and
   click its cues: "Full Show is running: not previewed".
2. **Several groups:** select two groups in the Library and drag them onto
   a cue (one layer "A + B"); again with ⌥ (one layer each); drag a group
   onto a layer row (it joins); right-click that layer → Remove … from layer.
   Watch the drop hint top right before letting go.
3. **Folders:** Library → All → Groups now mirrors the Fixture Manager's
   folders; type in the filter (a group name, or a folder name), clear it.
4. **Gobos by name:** on a layer whose group mixes fixture types, ✎ on the
   Gobo row (or set a gobo palette first via ⋯) → in the Look Editor pick a
   gobo on one type's wheel ("Wheel of" chooses the type): the look takes
   its name, the list under it says what each type plays, and the row's
   "applies to N of M" counts only the types that have it.

### Open for Branson (also §10)

1. `Generated` mark on `<Function>` too? (unchanged)
2. ~~Editing the cue on stage~~ — resolved: Design shows it at once, Live on GO.
3. Undo shared with Programming? (unchanged)
4. ~~Gobo mapping~~ — resolved and built; an effect's own Gobo still open
   (out of scope).
5. DanceHysteria Hold-0 auto-follow — intended? (unchanged)
6. New: a preview *on top of a different* list running for real is allowed
   (as Programming's preview is); or block the preview while any list runs?
7. New: gobo names match exactly (ignoring case/space/-/_): "Dot" ≠ "Dots";
   a per-type pick covers differences. Fuzzier would risk "Dots Shake".

Also worth knowing: editing a gobo palette in the Look Editor (either tab)
now makes it a by-name look; a type with no gobo of that name then ignores
it rather than getting the old raw value.

---

## Follow-up (2026-10-17)

Branson answered §10 questions 6 and 7; built on `feat/show-tree`, pushed,
**not merged**. Notes: SHOW_TREE_DESIGN.md §10.9.

| Commit | What |
|---|---|
| 4973a8d43 | **In Design, whatever is in edit is visible.** The selected cue previews also over its own list or any other running for real (header "…, over Full Show running"). Stopping the preview leaves the stage to what really runs, which kept going underneath. Perform: the list owns the stage, as before. The preview's scenes take precedence through `Scene::setEditOverride()` (Override-priority faders, LTP on every channel, switched in place on the DMX thread, cleared before a stop so the fade-out is HTP into what runs). Engine fixes: `Universe::requestFaderPriority()` couldn't lower a priority; a 16-bit `FadeChannel::Override` write sent one byte. **Gobo names in tiers:** per-type pick → exact → singular/plural ("gobo" and numbers ignored) → unique prefix/contains; ambiguous → not guessed, the row says "ambiguous: pick per type…" and links to the Look Editor. |
| bd8f64a2e | Help (show-tree.md), design §10.9 (+ 6 and 7 resolved, 8 "Blind: to discuss"), TODO/DONE. |

- **Gate** (`./check-all.sh`, JOBS=5, `git archive` copy of bd8f64a2e):
  Qt6 **PASS** (413 s), Qt6-Release **PASS** (377 s), Qt6-Werror **PASS**
  (406 s); whole gate 1196 s.
- New tests: `showtree_test::inDesignTheEditWinsOverARunningList`,
  `showtreeview_test::inDesignTheEditShowsOverItsRunningList`, gobo
  ambiguity in `goboLooksMatchByNamePerType`,
  `qlcpalette_test::goboNamesMatchInTiers` (table),
  `universe_test::faderPriorityMovesBothWays`. No new crash reports.
- **To try on the rig:** GO a list, then in Design select its running cue
  and pull a dimmer down: the rig should show the lower level; click the
  list row: back to the running cue. Gobos: name a look "Dot" on a mixed
  group and read each type's line in the Look Editor.
- **Open:** Blind and the preview (to discuss); the others in §10 unchanged.

---

## P2 (2026-10-17)

Tracking in the tab, Update cue / Record, Library "In this cue list" and
the optional command line (§8 P2). Built on `feat/show-tree`, pushed,
**not merged**. Notes: SHOW_TREE_DESIGN.md **§10.10**; open questions 9–11
added to §10.

| Commit | What |
|---|---|
| bab2b58a0 | **Engine:** `dependsOn/canInclude` (includes can't loop; the resolver leaves a looping include/base out instead of reading a half-resolved cue: a latent P1 bug); `addInclude/removeInclude/moveInclude`; `rebakeChain()` + `lastBakeChanged()` (what a bake really changed); D22 `valueSource/updateSource`; `recordCue` (Block, D23 number), `duplicateCue`, `setCueMode`, `setTargetRelease`, `addCueNumbered`; `usageInList`; `ShowTreeCommand` with `beginBatch/endBatch` (one undo per command); tests |
| 4ddb1a102 | **Tab:** list page base cue; cue page base check box, includes (+ menu greys loops with why, −, ↑↓), "Next cue receives" with values / back to base / released dark; Release on a carried-in layer; tree menu Mode / Record / Duplicate; header Record + "Changed: …"; ⋯ "Set in this cue" / "Update cue X"; Library "In this cue list"; command line (Command toggle, Tab, ↑↓, hint line); tests |
| fe1f9b68c | Cue page scrolls; the real-show picture test can show tracking |
| 62f9e9b91 | Help `show-tree.md`, design §10.10, TODO/DONE |
| d36c7ffc8 | A failed command also drops a one-off value made before any tree change |

- **Gate** (`./check-all.sh`, JOBS=4, `git archive` copies in the scratchpad):
  - after item 1, on **4ddb1a102** (all P2 code): Qt6 **PASS** (478 s),
    Qt6-Release **PASS** (417 s), Qt6-Werror **PASS** (455 s); 1350 s.
  - final, on **d36c7ffc8**: Qt6 **PASS** (447 s), Qt6-Release **PASS**
    (448 s), Qt6-Werror **PASS** (458 s); 1353 s.
- **Tests:** `showtree_test` 47 + 1 opt-in skip (+12: re-bake chain on the
  model and as baked, includes, D22, record, dup, usage, tracked preview,
  **`convertAnImportToTracking`** — a DanceHysteria-shaped chaser imported,
  converted by hand, plays §3.2 / §3.3 / §3.4 on the rig — command line ×3);
  `showtreeview_test` 31 + 1 opt-in skip (+8); `helpbrowser_test` green.
- **DanceHysteria-0.5 import dry run** (a copy in the scratchpad, unchanged
  after): **0 channels differ** in all 8 cues (Full Show 6, New Chaser 8 2).
- **Crash reports:** six `showtreeview_test-2026-10-17-0748..0749*.ips`
  — mine, from the picture test I extended (it held a `CueList *` across an
  edit; fixed in fe1f9b68c before commit-time runs). None from the gates.
- **CI** run 39030378130 (d36c7ffc8): macOS, Linux, Windows **green**;
  Linux coverage still running when this was written.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-p2.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-p2.qlcc
```

(It patches the show's outputs as usual; use Blind for the 2D view only.)

1. Show Tree → Import chaser… → Full Show. Select the list: set **Tracking**
   and a **Base cue**. Watch the grey ⤳ rows appear and "Changed: …" top right.
2. Convert by hand: on 0.0.3 tick **Release** on AT-Spots; delete layers in
   0.0.4 that only repeat 0.0.3; on 0.0.5–0.0.7 remove the include of the
   cue before (cue page, Includes, −). Read **Next cue receives** on each.
3. On a grey value, **⋯ → Update cue 0.0.3 …**: the change shows in every
   cue it tracks into. Try **Set in this cue** for the difference.
4. **Record** on 0.0.5: a Block cue 0.0.5.5 with everything on stage.
5. **Command**: `cue + "Test" block`, `StepTops dimmer 45`, Tab on `Ste`,
   `include 0.0.3`, `undo`.
6. Library → **In this cue list** while a cue of the list is selected.

### Open for Branson (also §10)

- 9. Record as new cue in Perform: the selected cue (built) or the running
  list's current cue / everything lit?
- 10. Command line `Towers USTs …` = a layer each (mockup), `Towers+USTs` =
  one; the mouse defaults the other way. Swap?
- 11. `dimmer 45` always makes a one-off (no reuse of a palette at 45 %).
- Unchanged: `Generated` on `<Function>`; undo shared with Programming;
  effect's own Gobo; DanceHysteria Hold-0; Blind (to discuss).

## P2 answers (2026-10-17)

Branson's answers to open 9–11, built on `feat/show-tree`, pushed, **not
merged**. Notes: SHOW_TREE_DESIGN.md **§10.11** (D11/D21 changed; open 9–11
marked resolved).

| Commit | What |
|---|---|
| 3310a9f59 | **Record** selected / running / everything lit (asks only when they differ; Settings → Show Tree default per Design / Perform, Ask by default; `record selected\|running\|lit`). **One layer or each**, one rule: several = one layer (drop, + Layer, `Towers USTs` = `Towers+USTs`), each = ⌥-drop, ⌥-click / + Layer menu, `Towers, USTs`. **Split layer / Merge layers** keeping tracking (`Layer::continuesAlso`, re-set cues split/merged too), refused with a reason on conflicting looks or if any cue's stage would change (`ShowTree::stageDiffers`). **Values reuse palettes**: exact named palette (In this cue list → Recent → any); command line otherwise makes one named per the show's pattern (`Intensity 30%` beside `Intensity 45%`); picker one-off gets one-click **Make reusable** on its row. Tests |
| 5034675ed | Help `show-tree.md` (+ What's new), design §10.11, TODO/DONE |
| d631f35ca | Unused helper removed (the first gate's Werror leg caught it) |

- **Gate** (`./check-all.sh`, JOBS=6, `git archive` copy in the scratchpad):
  on 5034675ed Qt6 PASS, Qt6-Release PASS, **Qt6-Werror FAIL** (build:
  unused `layerText` in showtreecommand.cpp) → fixed in d631f35ca; on
  **d631f35ca**: Qt6 **PASS** (780 s), Qt6-Release **PASS** (427 s),
  Qt6-Werror **PASS** (439 s); 1646 s.
- **Tests:** `showtree_test` 52 + 1 opt-in skip (+5: record sources,
  split and merge across tracked cues with refusals and XML round trip,
  palette reuse order + editing the palette changes both cues on the rig,
  naming patterns); `showtreeview_test` 34 + 1 skip (+3: the Record
  setting / Don't ask again / dialog / command line, one-or-each + split +
  merge from a tree selection, picked value reuse + Make reusable);
  `helpbrowser_test` green.
- **DanceHysteria-0.5 import dry run** (a copy, unchanged after): passes
  (0 channels differ).
- **Crash reports:** one new, `showtreeview_test-2026-10-17-090513.ips`
  (SIGABRT): mine, the QTest watchdog on a first draft of
  `severalGroupsOneLayerOrEach` that sat in the "select groups first"
  message box (a Library rebuild had cleared its selection). Fixed before
  commit; none from the gates.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-p2a.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-p2a.qlcc
```

1. Import chaser… → Full Show; make it Tracking. GO it from the footer,
   then select another cue and press **Record**: it asks *selected / running*
   (and *everything lit* if something else is on). Tick **Don't ask again**;
   Settings → **Show Tree** shows the default per mode. Try it in Operate.
2. Select two groups in the Library → **+ Layer**: one layer; ⌥-click (or
   the arrow) for one each. Right-click a two-group layer → **Split layer**;
   ⌘-click two layers of a cue → right-click → **Merge 2 layers** (set one's
   colour differently first to see the refusal). GO through the cues after.
3. **Command**: `StepTops dimmer 45` (says *reused "Intensity 45%"*),
   `StepTops dimmer 30` (*new palette "Intensity 30%" in Palettes/Dimmer*),
   `Towers AT-Spots dimmer Full` (one layer) vs `Towers, AT-Spots …` (each),
   `record running`.
4. ⋯ → **Level…** 45 on a row: uses Intensity 45%. 31: a one-off with
   **Make reusable** beside it.

### Still open (§10)

`Generated` on `<Function>`; undo shared with Programming; an effect's own
Gobo; DanceHysteria Hold-0; Blind (to discuss).

## Drop on the page (2026-10-17)

Branson's report ("drag to the main window and it adds layers; how do I add
to the same layer"), two added items (command line always open, Tab for
every word) and a priority bug (truss bars not blue on the Rig Overview).
On `feat/show-tree`, pushed, **not merged**.

| Commit | What |
|---|---|
| d8664080a | **Rig Overview bug** — see below. `fixturevisualtraits.cpp` + monitor_test and showtreeview_test regressions |
| f23a1eb6a | **Drop on the page**: the cue page takes every drop the tree does — groups / fixtures on the page = one new layer (⌥ one each, now one undo step), onto a layer's row = added to it, onto a grey carried-in row = brought into the cue with them (one undo step, values kept), palettes = cue look / that row's look, functions = opaque layer. Real rows (click opens the layer), dashed drop area, page / row lights up, the tree's hint text; the cue stays selected. Layer page: hint + multi-group drops; tree: drops on carried-in rows. One `dropMime()` for all three. **Command line** always at the bottom (toggle gone), ClickFocus, **:** in the tree jumps there (⌘L is Show Lock), Esc back. **Tab** completes from a parse of the line: keywords, cue numbers, lists, groups + folder paths, fixtures, attributes, palettes by kind (+ none/off/clear), functions, modes, record choices; shared part + options, Tab again cycles; names with spaces whole, begun quotes closed, `,`/`+` names quoted. Grammar: fixtures and `Movers/Front/Spots` as targets, `function A, B`, `list X` selects an existing list. Help: Adding layers, command line, shortcuts, what's new |

**The truss-bar bug, root cause.** Not the Show Tree: on a copy of
Branson's own file (`/tmp/dh-showtree.qlcc`, the XL-450 bars loaded from
its embedded definitions), the cue's layer on TrussC-RGB + T TrussW-RGB with
Blue + Base (48 = 19%) was previewed (Design) and every head of all 8 bars
went out (0, 1, 48) in universes 4/6 — blue at 19%, correct: the bars have
no dimmer channel, so the Dimmer look scales their colour. The **Rig
Overview** read it as colour (0,1,48) *and*, with no dimmer channel, a level
of max(R,G,B) = 48, then drew colour × level = 9/255: black. Any dimmed RGB
fixture without a dimmer channel showed it; at full it looked fine. Fixed in
`channelSetLiveState()`: with no dimmer channel the colour comes back at full
hue and the level once. Fixtures with a dimmer channel unchanged; all 287
monitor tests unchanged. One group or two made no difference; in Operate the
preview doesn't run and the header says *"Live: editing is blind"*, never
"On the rig".

- **Tests:** showtreeview_test +3 (`dropsOnTheCuePage`,
  `dropsOnTheLayerPage`, `aDimmedColourOnPixelBarsReachesTheUniverse`) and
  the command-line test (bottom, focus, `:`/Esc, cycling); showtree_test
  `commandLineCompletesEveryWord` (44-row table, every word kind) +
  `commandLineTabCyclesAndRuns`; monitor_test dimmed pixel-bar check;
  helpbrowser_test green.
- **Gate** (`./check-all.sh`, JOBS=6, `git archive` copy of f23a1eb6a in
  the scratchpad): Qt6 **PASS** (427 s), Qt6-Release **PASS** (420 s),
  Qt6-Werror **PASS** (407 s); 1254 s; 87 engine + 56 UI test programs.
- **Crash reports:** none from tests. One new
  `qlcconsole-2026-10-17-153638.ips` is Branson's own app session (launched
  15:16, crashed 15:36): a mouse press in the Rig Overview
  (`StructureStudioView::mousePressEvent` → `setCursor(Qt::ClosedHandCursor)`)
  died inside Qt's Cocoa cursor code (`QImage::toCGImage` → `CGImageCreate`,
  bad colour space) — Qt / macOS, not ours; worth watching if it repeats.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-drop.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-drop.qlcc
```

1. Select a cue; drag two groups from the Library onto the dashed area in
   the middle: one layer (⌥: one each). Drag another group onto that
   layer's row: it joins. Drag one onto a grey ⤳ row: it comes into the cue.
2. Open the Rig Overview (⇧⌘R), Live on: a layer of RGB bars at Blue with a
   low Dimmer now shows dim blue (it drew black).
3. Command line at the bottom: press **:** in the tree, type `Tow`⇥,
   `col`⇥, `B`⇥⇥⇥ (cycles), `function Ha`⇥, `list 0`⇥.

## The four limits from the help (2026-10-17, late)

Found while writing docs/features/show-tree.md (7cf0c70c5); design §10.12.

- **Merge by mouse.** ⌘-click two layers of one cue → **Merge** (in the header or the
  context menu). ⇧-click takes a range. Delete takes several cues/layers in one step.
- **D13 lock.** Open the list's chaser in the Functions window or the Programming tab:
  steps and name are locked under a bar saying *"Made by the Show Tree — edit it
  there."*, with **Show in Show Tree**. Run order and speed modes stay editable.
  Delete/rename/move of it is refused. Not built: *Detach to plain chaser*.
- **+ Cue list** now makes a **Tracking** list, as `list` does. An import stays
  Non-tracking.
- **Undo:** Make reusable, Detach, several functions dropped together, and a colour +
  dimmer dropped together are each one step now. `everyActionIsOneUndoStep` walks 47
  actions.
- **Gate** (`git archive` copy of f92add51b, JOBS=4): Qt6 PASS 461 s, Qt6-Release PASS
  399 s, Qt6-Werror PASS 438 s (1298 s).
- **Crash reports:** `showtreeview_test-2026-10-17-163315.ips` / `-163354.ips` are
  QTest watchdog aborts from my own runs while writing the undo walk. A modal "Delete
  cue list?" opened because the selection bug above toggled rows; both are fixed, and
  the test now closes any modal box. None since.

**Try** (on a COPY): select a cue, ⌘-click two of its layers, Merge. Open Functions
(Window menu) → Productions → the list's chaser: it is locked, and Show in Show Tree
works.

---

## P3 (2026-10-17)

Group sets, child layers and overlap, cue-level effects, line aims (§8 P3),
plus what Branson asked for while it was being built. On `feat/show-tree`,
pushed, **not merged**. Notes: SHOW_TREE_DESIGN.md **§10.13**; open
questions **12–16** in §10.

| Commit | What |
|---|---|
| 979a29253 | Merge `feat/line-targets` (P1–P3). Conflicts: `QLCPalette::loadXML` (Gobo's and Aim's children both kept), DONE / what's new (both sides). monitor_test's Add-menu test now expects "Target Here" and "Target Line Here". |
| 00ec68dcd | **Engine:** `Scene::setExcludedFixtures` / `setLayoutSpan` per look — fader map (under `m_valueListMutex`), re-aim path, CueOutput, line aims (`AimSolver::aimHeads(scene, palette)`), effects (cells over the span, writes its own). XML `<Exclude>` / `<Span>`. Thread test. |
| a528f4891 | **Group sets** (model, undo, XML), **child layers** (`Layer::parent`), **overlap** in the bake (per attribute and fixture, the nearer source wins, the rest leave that fixture out — no HTP show-through), `Cue::mixOverlap` for imports. |
| 3027802ec | Tab: Library → Group sets, Make set; ⇧-drop child layers, Add child layer; nested rows; "from Steps (parent)"; Overlap check box; **rename in place** (Return/F2); command words. |
| 6dab53671 | **D10** cue effect as one across layers; **line aims**: Spread / Grouping / Cross keys, D18, D17 formation by hang position, inline Aim palettes for a layer's own settings, D19; Library → Targets, rows, order strip, Line ⋯. |
| 477bcf711 | **`help`** from the command table (did-you-mean, examples clickable); **⌘C / ⌘V / ⌘D** and `copy` / `paste` for cues and layers. |
| 6f08c4a54 | Help, design §10.13, TODO (incl. the plain-language command line, not built) / DONE; an Aim palette edit re-bakes. |
| 0df1d15f7 | **Library \| Tree \| Editor** (Look Editor under the page); Return / double-click in the Library and ⌘F or / to add without dragging; a drop below the tree's rows. |
| 2cc1eb612 | A named layer says what it holds (page "Contains:" with links and ✕; command line). |
| 6ccb01dc0 | **Several layers edited together** (shared / mixed values, tri-state Release, one undo step, right-click, command line); a named layer is just its name in the tree (Branson's correction). |

- **Gate** (`./check-all.sh` with JOBS=4, on `git archive` copies in the
  scratchpad — the script's `JOBS` line made to read the environment in
  the copy only):
  - after item 2, on **3027802ec**: Qt6 **PASS** (494 s), Qt6-Release
    **PASS** (626 s), Qt6-Werror **PASS** (710 s);
  - final, on **6ccb01dc0** (everything; only this file after it): Qt6
    **PASS** (2263 s — the machine was at load 220–300 from other checks),
    Qt6-Release **PASS** (438 s), Qt6-Werror **PASS** (466 s).
- **Tests:** showtree_test 114 + 1 opt-in skip (+17), showtreeview_test
  50 + 1 skip (+9; the undo walk covers 13 more actions), helpbrowser_test
  green.
- **DanceHysteria-0.5 dry run** (a fresh copy, unchanged after; the file
  now has a third chaser): **0 channels differ** in all 11 cues. It found a
  real issue first: two overlapping StepTops scenes in New Chaser 8 mix RGB
  highest-wins, which the tree's overlap rule changed (1056 channels) — so
  imported cues keep mixing (`Overlap: Mix`, said in the report).
- **Crash reports**, all from my test drafts, none from the gates:
  `showtree_test-…-180653.ips` (QTest watchdog: my first D10 test ticked the
  MasterTimer on the main thread while previewing a scripted effect —
  EffectScriptRunner's direct `functionStopped` slot re-claimed the
  universes the tick held; a test-only deadlock, the real timer runs on its
  own thread; the test no longer does it); `showtreeview_test-…-193322` /
  `-193330.ips` (my test looked up the layer page as a `QLabel`, null).
- `build/main/qlcconsole` rebuilt.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-p3.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-p3.qlcc
```

(It patches the show's outputs as usual; use Blind for the 2D view only.)

1. Show Tree (⇧⌘6): the Library is on the left now. ⌘-click two tower
   groups → right-click → **Make set…** "Towers". Select a cue, select the
   set, press **Return** (or drag it): one layer. ⌥Return: one each.
2. **⇧-drag** a group that is part of a layer onto it: a child layer. Set
   its Dimmer low — the parent's full doesn't show through.
3. Drop an effect on the **cue**: one effect across its layers.
4. Draw a line target in the Lighting Studio (Add → Target Line), then
   Library → Targets → drop it on a cue with two layers of movers: one
   formation; the cue page's **Line ⋯** → Spread 0, then +5 in the next
   cue with a fade: the beams sweep. The layer page's order strip numbers
   the lights across both layers.
5. ⌘-click three layers: Release, a colour, a dimmer — all at once, one
   undo each. Type `dimmer 45` with them selected.
6. Return on a layer row renames it; ⌘C / ⌘V a cue; type `help`, `help
   spread`.

### Open for Branson (§10, 12–16)

- 12. D10/D17 built as a shared layout span (each layer's own scene keeps
  its own base under the effect), not one extra scene. OK?
- 13. Overlap: a layer's own value beats a cue look reaching a lower layer
  (D5's nearer scope) — OK? Imports keep mixing; should Convert to tracking
  (P4) clear it?
- 14. The plan view (top view, beams, the previous cue's rings) is not
  built; the order strip is. Now or later?
- 15. A Mix cue in the Design preview shows the lower layer where they
  overlap (the preview's edit-override writes on top).
- 16. A child's Dimmer on fixtures with no dimmer channel doesn't dim the
  colour its parent gives them.
- Unchanged: `Generated` on `<Function>`; undo shared with Programming; an
  effect's own Gobo; DanceHysteria Hold-0; Blind; Detach to plain chaser.
- Not built, recorded: the plain-language command line (TODO.md).

---

## P4 (2026-10-17)

Make reusable / Detach for layers, cues and lists; the Library's Reusable
section; Convert to tracking (§8 P4). On `feat/show-tree`, pushed, **not
merged**. Notes: SHOW_TREE_DESIGN.md **§10.14**; open questions **17–20**
in §10.

| Commit | What |
|---|---|
| 26ab4f82e | **Engine:** `ReusableLayer` / `ReusableCue` (`<ReusableLayer>`, `<ReusableCue>`, `Reusable=`); a use is a plain layer/cue with a link, and `syncReusables()` after every edit makes the use the edit changed the reusable's content and pushes it to every other use — so every operation, mouse or typed, edits all uses in one undo step. Make reusable, use, Unlink here, rename, uses, delete (refused / unlinking). `detachList()` (list leaves the tree; chaser, collections, scenes only it plays handed over; a still-shared scene handed over when the tree lets go, never deleted; undo takes them back on the same ids). `planConvertToTracking()` / `convertToTracking()`. Commands `reusable`, `use`, `unlink`, `detach [list]`, `convert [apply]`. Tests. |
| b09d64864 | **Tab:** ⟲ on uses (with count), Reuse line on cue/layer pages (Make reusable…, Used by…, Unlink here), right-click entries, list page **Convert to tracking…** (the plan, then one step) and **Detach to plain chaser…**; Library → All → **Reusable** (drag / Return = a use; Rename, Used by incl. VC buttons, Delete asks). Tests. |
| 3fad44da1 | Help `show-tree.md` (+ What's new), design §10.14 + open 17–20, TODO/DONE |
| ad5ea5c70 | Convert's button enabled by a cheap check (no plan on every list click) |
| 3d05b94a5 | Unused variable in a test (the first gate's Werror leg) |

**Decided here (Detach to plain chaser, §10 open):** the list **leaves the
tree** rather than staying as an opaque layer — its chaser's steps *are*
its cues, so nothing of the tree would be left to edit.

**Convert to tracking** keeps a change only if every cue still has the same
stage *and* the same generated scenes (target, palettes, exclusions, spans)
and opaque functions — equal scenes play equal DMX. DanceHysteria-0.5
(fresh copy, unchanged after): Full Show 5 cues track, 3 includes made
tracking, 2 releases, 6 values + 2 layers taken out; 00-Preshow 2 track, 5
releases, 6 values + 2 layers; New Chaser 8 1 tracks, 2 releases, 3 values
+ 1 layer. **0 channels differ** in all 11 cues (imported vs converted,
every universe), and the import itself still 0 vs the chasers.

- **Gate** (`./check-all.sh`, JOBS=4, `git archive` copies in the
  scratchpad, JOBS line made to read the environment in the copy only):
  - after items 1–2, on **b09d64864**: Qt6 **PASS** (465 s), Qt6-Release
    **PASS** (441 s), **Qt6-Werror FAIL** (build: unused `a2` in
    showtree_test) → fixed in 3d05b94a5;
  - final, on **3d05b94a5**: Qt6 **PASS** (480 s), Qt6-Release **PASS**
    (425 s), Qt6-Werror **PASS** (455 s); 1360 s.
- **Tests:** showtree_test 121 + 1 opt-in skip (+7: reusable layers, reusable
  cues in two lists, save/load/undo/batch rollback, detach list, convert on
  DanceHysteria's shape and on synthetic lists, typed); showtreeview_test
  52 + 1 skip (+2; the undo walk +13 actions, ≥70); helpbrowser_test green.
- **Crash reports:** none new in `~/Library/Logs/DiagnosticReports` during
  this work.
- **R1** (PROGRAMMING_ROLE_DESIGN.md): nothing of it was needed by P4; left
  for R1 (noted in TODO).
- `build/main/qlcconsole` rebuilt. A running app keeps its old binary until
  it is relaunched.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-p4.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-p4.qlcc
```

1. Show Tree → Import chaser… → Full Show; select the list → **Convert to
   tracking…**: read the plan, Convert; GO through it (it should look the
   same); ⌘Z puts the import back.
2. Right-click a layer → **Make reusable…** "BackStage"; select a cue of
   another list, Library → All → Reusable → drag it in (or Return). Change
   its colour there: the first list's changes too. **Unlink here** on one.
3. Right-click a cue → **Make cue reusable…** "Walk-in"; drop it on a cue of
   another list: a new cue after it, same layers, its own timing.
4. Right-click a list → **Detach to plain chaser…**: it leaves the tree; the
   chaser in Functions → Productions is unlocked; ⌘Z brings it back.
5. Type `reusable`, `use BackStage`, `unlink`, `convert`, `convert apply`,
   `detach list`, `help use`, `help convert`.

### Open for Branson (§10, 17–20)

- 17. What a reusable cue shares: mode yes, timing no — OK?
- 18. Convert releases rather than making Cue-only cues, leaves cue looks
  and Mix cues alone (ties to 13) — OK?
- 19. A detached list's scene shared with another list stays locked until
  the tree lets go of it — or copy it at once?
- 20. A "detached from BackStage" badge after Unlink here — wanted?
- Unchanged: 1, 3, 4 (effect's own Gobo), 5, 8, 12–16.

---

## Looks + Library palettes (2026-10-18)

PROGRAMMING_ROLE_DESIGN.md **R1** (the parts R2/R3 need), **R2** (palettes
in the Library) and **R3** (the Looks section), after Branson's "yes" to
*Looks/busking + palettes in the Library next*. On `feat/show-tree`,
pushed, **not merged**. Notes: SHOW_TREE_DESIGN.md **§10.15**; open
questions **21–26** in §10.

| Commit | What |
|---|---|
| 45ad2e89f | Merge `design/programming-role` (PROGRAMMING_ROLE_DESIGN.md, P-D1…P-D16 approved, and the release/0.1.0 work it sits on; conflicts in appsettings, structurestudioview includes, TODO/DONE — both sides kept) |
| 22bf9f9a6 | **Engine:** `PaletteOps` (Doc-owned: create by type, rename, duplicate, file, folders of any type, delete; one palette undo; `usage()` naming cues/looks/reusables/scenes/slots); **one history** (tree + palette steps stamped; `undoShared`/`redoShared`); the **Looks section** (`CueList::looks`, `<Looks>`, folders, never tracking, a Collection per look whose id survives edits, Adopt as look, Copy to look, Record lit takes a running look's layers); commands `palette …`, `look …`, `vcpage`, `adopt`; tests |
| 474f2983b | **Tab:** `UsedBy` (VC buttons), Look Editor Used by line; Programming's palette New/Duplicate/Delete/Move through PaletteOps; Library palettes (by type / folders first, + Palette, F2, folders, drag, Used by, Delete asks); the Looks section in the tree (folders, + Look, Fired by, VC ×N, Make VC page, Copy to look, Adopt with VC re-point); `VirtualConsole::placeWidget`; tests |
| 19be654cb | Help (`show-tree.md` Looks + Library palettes + one Undo, `programming-tab.md`, What's new), design §10.15 + open 21–26, PROGRAMMING_ROLE_DESIGN.md status, TODO/DONE |

**Decided here (deviations, §10.15):**
- A look is a **cue of a flagged list**, not a new node type — every cue
  operation works on it for free; `edit()` keeps looks free of mode,
  includes and base.
- A look **always bakes to a Collection** (the design: a Scene if one
  layer): a layer's scene changes id when the look changes, the cue's
  Collection doesn't, so a VC button keeps firing it. Shown, under
  `Productions/<p>/Looks/<folder>`, locked (open 21).
- `PaletteOps` is owned by **Doc** (the design: beside
  ProgrammerController), a thin accessor, as the tree is.
- feat/palette-folders is **not merged**: the Library reads type-first
  folders as one folder (its rule, read only) and writes type-independent
  paths (open 25).

**R1 left for later:** Look Editor value edits on the palette undo;
Programming's tree folder rename/delete/drag via PaletteOps (they set paths
directly, off the history); Programming's ⌘Z reaching the palette history
(open 24); power estimate to the app; Stage bar (P-D10); the Used by line in
Programming's editor. **R2 left:** Open in Palettes. **R3 left:** per-look
fades / transition / speed master (and on cue layers), Fire as, a look's
MIDI trigger in the tab.

- **Gate** (`./check-all.sh`, JOBS=6, `git archive` copies in the
  scratchpad, the JOBS line made to read the environment in the copy only):
  - after R2 (R3 included), on **474f2983b**: Qt6 **PASS** (373 s),
    Qt6-Release **PASS** (331 s), Qt6-Werror **PASS** (381 s); 1085 s;
  - final, on **19be654cb**: Qt6 **PASS** (385 s), Qt6-Release **PASS**
    (352 s), Qt6-Werror **PASS** (401 s); 1138 s.
- **Tests:** showtree_test 133 + 1 opt-in skip (+12); showtreeview_test 58
  + 1 skip (+6: palette and look actions each one undo step on the shared
  history, the Programming tab's New palette undone from the Show Tree);
  programmingmanager_test and helpbrowser_test green.
- **DanceHysteria-0.5 dry run** (a fresh copy in the scratchpad, unchanged
  after, by checksum): New Chaser 8 2 cues, Full Show 6, 00-Preshow 3 —
  **0 channels differ** in all 11 cues (imported vs the chasers), and 0
  after Convert to tracking; the plans as P4's (Full Show: 5 cues track, 3
  includes, 2 releases, 6 values + 2 layers).
- **Crash reports:** one new, `showtreeview_test-2026-10-18-080734.ips` —
  `VirtualConsole::VirtualConsole` asserting a second instance: a new test
  failed before its `delete VirtualConsole::instance()` and the next test
  made another. Test harness only; fixed with a scoped VC (`ScopedVc`). None
  since.
- `build/main/qlcconsole` rebuilt. A running app keeps its old binary until
  it is relaunched.

### What to try in the app (on a COPY of a show)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-looks.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-looks.qlcc
```

1. Show Tree → select the production (import a chaser first if there is
   none) → **+ Look**. Name it "Hit 1"; drag Towers and a colour onto it.
   It plays on the rig while selected (Design).
2. Right-click **Looks** → New folder… "Hits"; drag Hit 1 onto it; **Make
   VC page**. In the Virtual Console: a frame "Looks — Hits", a red button.
   GO a cue list, press the button: it plays over the list (highest
   intensity wins); press again — the list's look is back.
3. Library → All → Palettes: **+ Palette → Colour** in a folder; **F2**
   rename; drag a palette onto a folder; right-click → Group by type (off:
   folders first). Delete a palette a cue uses: the question names the cue
   (and a VC button firing it); ⌘Z brings it back everywhere.
4. Programming tab → New Color in its palette list; back in the Show Tree,
   ⌘Z takes it back (one palette history).
5. Library → Functions → right-click an old busking scene → **Adopt as
   look**; with a VC button on it, say yes to re-point.
6. Type: `palette colour "Amber 2" #ffb000 in "01-Song"`, `palette Amber 2
   rename "Amber"`, `look "Hit 2" in "Hits"`, `Towers dimmer Full`,
   `vcpage`, `adopt "Old Scene"`, `help palette`, `help look`.

### Open for Branson (§10, 21–26)

- 21. A look is a Collection always (stable for VC) — OK?
- 22. Make VC page: a new frame each time, Toggle, six a row, colour from the
  first colour — or update the frame it made; Flash for some?
- 23. Library palettes: by type by default (as built) or folders first?
- 24. Should Programming's ⌘Z reach the palette history too?
- 25. Merge feat/palette-folders? (a folder named like a type would be
  stripped by its migration unless PaletteOps writes `Folders="1"`.)
- 26. Deleting a palette cues use: leave "(missing)" + Undo (as built), or
  also take it off the cues?
- Unchanged: 1, 3 (now partly answered by the shared history), 4, 5, 8,
  12–20.

---

## Answers 12–20 (2026-10-18)

Branson's answers to §10 open 12–20, and then 21–25 in the same pass (26
is still open). Notes in SHOW_TREE_DESIGN.md §10.16; help updated
(show-tree.md "Who wins", the plan view, Convert, Detach, Make VC page,
Undo; programming-tab.md).

| # | Answer | What changed |
|---|---|---|
| 12 | ok | D10/D17 stay a shared layout span |
| 13 | "consistent in precedence up and down the chain" | One rule: most specific source (tracked < included < cue look < layer < child < effect's own), then deeper, then lower. Convert puts Mix cues on it where every channel is the same; the rest stay **≈ mix** (tree marker + tooltip) and the plan lists them |
| 14 | build it now | Plan view under the order strip: line from above, beams, previous cue's points as rings |
| 15 | preview must match playback | Preview scenes HTP among themselves, over everything else; tested equal to GO |
| 16 | it should | The Dimmer's layer plays the winning colour on no-dimmer fixtures |
| 17 | each use | Timing per use, as built |
| 18 | yes | Convert tries Cue-only (and clears Mix, 13) |
| 19 | copy | Detach copies scenes another list plays |
| 20 | IDK → no badge | Tooltip "Unlinked from <name>" |
| 21 | ok | A look stays a Collection |
| 22 | ok → update | Make VC page updates its own frame (adds/removes its buttons, leaves the user's) |
| 23 | ok | Library palettes by type by default |
| 24 | yes | Programming ⌘Z / ⇧⌘Z on the one history; Edit menu names the step |
| 25 | yes | feat/palette-folders merged (--no-ff); migration strips only the genuine old layout |

**DanceHysteria-0.5 dry run (a COPY):** Full Show 0.0.6 / 0.0.7 Mix → the
rule; New Chaser 8 cue 1 stays Mix ("would change 1056 channel(s) on 22
fixture(s)"); 0 channels differ in all 11 cues, chaser vs list and
converted vs imported.

### Try in the app (on a COPY)

```sh
cp ~/git/qlcconsole/test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-answers.qlcc
~/git/qlcconsole-showtree/build/main/qlcconsole -o /tmp/dh-answers.qlcc
```

1. Import New Chaser 8 → Convert to tracking: the plan says cue 1 stays
   Mix; the tree shows **≈ mix** on it (hover). Import Full Show: its
   0.0.6 / 0.0.7 follow the rule after Convert.
2. A layer aimed at a line: under the order strip, the plan view; set
   Spread in the next cue — the rings are the cue before.
3. A child layer on RGB bars with a lower Dimmer than its parent: the
   bars dim.
4. Looks → Make VC page; add a look, delete one, move a button; Make VC
   page again — the same frame, updated.
5. Programming → New palette, then a Show Tree change, then ⌘Z twice in
   Programming; Edit menu shows what Undo will take back.
6. Open an older show: palette folders per type become one folder per
   name; a folder named "Colour" stays.

### Open for Branson

- 26 (deleting a palette cues use) — unanswered.
- Unchanged: 1, 3, 4, 5, 8.

---

## P5-P7 on the build host (2026-10-18)

Built unattended on **buildhost-qlcplus** (Debian 13, x86_64, Qt 6.8.2, GCC
14, Ninja, `-Werror`), on `feat/show-tree` from 451ef02bf. **Not pushed**
(no GitHub from that host): fetch the commits over SSH. Notes in
SHOW_TREE_DESIGN.md **§10.17-10.19**, open questions **27-35**; help in
`show-tree.md` (*Productions*, *Sub-cues*, *The cue runner*),
`show-control.md`, `shortcuts.md`, `whats-new.md`.

| Commit | What |
|---|---|
| 7d16d5a6d | **P5 productions:** the current production (= the show's main cue list seen from the tree, kept as one both ways, never on undo); per production a main list and a timecode source (`TimecodeSource::setProductionUniverse`); header switcher, production page, ▶ current / · main, footer "Dance ▸ Next:" + Production menu; `production … [current]`, `main` |
| 132108f2f | **Fix (pre-existing):** `~Chaser` deletes a running runner before its steps -- a use-after-free on teardown that glibc reported as "double free or corruption" in showtreeview_test (intermittent, Linux only); found with an ASan build. Plus a test reading a Scene after undo deleted it |
| 1b18f75bb | **P6 sub-cues:** a flagged list per cue (`CueList::subOf`, `<SubCues>`), tracking from the cue then each other, cue-only; baked as a held inner chaser (`SubGo`, new **`SubGoFollow`** for Auto-follow); the hand-over reaches through them (no re-fade); tree rows, + Sub-cue, Advance; `sub`, `follow`, `subgo`, `cue 4.2` |
| cc4a510fd | **P7 cue runner:** `CueRunner` (Full / Compact / List, no state of its own: everything through `ShowControl::execute`); the **Run** tab (⇧⌘0, Space = GO); `VCCueRunner` + **Swap Widget**; `changeLine()` "what changes"; ShowControl SUB GO / SUB BACK (`/subgo`, `/subback`), sub-cues by number, LOAD of a sub-cue |
| 652d901e6 | **Fix (P6 regression the P6 gate caught):** the same chase on two consecutive steps is stopped before it restarts (`subcues_test::consecutiveCuesOnOneChaseStartWhereAsked`) |
| 88dbb3455 | Help, design §10.17-10.19 + open 27-35, TODO/DONE |
| 043676070 | Test only: a list pointer held across an edit in showtreeplay_test (ASan) |

**Decided here (deviations, §10):**
- The current production is not a second "what GO plays": it *is* the
  main cue list (ShowControl), seen from the tree. No current production =
  exactly the behaviour before P5.
- Sub-cues are a **flagged list** (as the Looks section), not a new node:
  every cue operation works on them for free.
- **Engine change in P6** (the design said none): `ChaserStep::SubGoFollow`,
  needed for per-sub-cue Auto-follow; and the hand-over into / out of a
  sub-cue chase waits one more write.
- The runner keeps no state; LOAD of a sub-cue is ShowControl's
  (`loadedSubCue`).

### Gate results (real)

Gate = fresh Release and Debug builds of a `git archive` of the commit in
separate dirs, then `QT_QPA_PLATFORM=minimal xvfb-run -a ./unittest.sh ui
<release>` and the DanceHysteria dry run.

- **P5 (7d16d5a6d):** Release and Debug builds clean (the only warning is
  the old `-Werror=reorder` note for C files). **unittest.sh: crashed** in
  showtreeview_test (`double free or corruption` at
  `inDesignTheEditShowsOverItsRunningList`, then hung in QtTest's stack
  dump). Not P5's: an ASan build of that snapshot traced it to the
  pre-existing Chaser teardown bug, fixed in 132108f2f; the whole
  showtreeview_test was ASan-clean after it.
- **P6 (1b18f75bb):** builds clean; **unittest.sh failed** at
  `subcues_test::consecutiveCuesOnOneChaseStartWhereAsked` (my regression,
  fixed in 652d901e6); dry run 0 channels differ.
- **Final (88dbb3455, P5+P6+P7+fixes+docs): PASS** -- Release and Debug
  clean; unittest.sh **90 engine, 59 of 59 UI test programs, all passed**
  (623 s for the whole gate); dry run **0 channels differ** in all 18
  compares; the show copy unchanged (sha256).
- **Debug-build Show Tree tests** (HEAD 652d901e6+): showtree 136 + 1
  opt-in skip, showtreeplay 23, cuehandover 11, chaser 24, chaserrunner 22,
  showcontrol 21, subcues 14, showtreeview 62 + 1 skip, cuerunner 11,
  appviews 14, helpbrowser 9 -- all pass.
- **ASan** (Debug, `-fsanitize=address`): showtreeview_test, cuerunner_test,
  subcues_test, cuehandover_test clean; showtreeplay_test found the test's
  own dangling pointer (043676070); rerun with it: showtreeplay_test 23
  and showtree_test 136 (+1 skip) **ASan-clean**.
- **DanceHysteria:** `~/soak24` has no 0.5, so the newest there,
  **DanceHysteria-0.3.qlcc**, copied to /tmp each run: New Chaser 8 (2 cues)
  and Full Show (7 cues) imported, played against the chasers and converted
  to tracking -- **0 channels differ** in every cue, both ways.
- **No DMX out:** every test reads the universes in memory.
- **Not run here:** the macOS `./check-all.sh` legs (Qt6, Qt6-Release,
  Qt6-Werror). **Run them on the Mac before merging.**

### What's left
- Tracking sheet (and its sub-cue columns) -- it doesn't exist yet (35).
- "Stage now" swatch strip in the runner (§6B).
- Cue Runner VC widget's own MIDI / key inputs (32); Run-tab keys beyond
  Space (31).
- Import of a chaser's Sub GO steps as tree sub-cues; Convert to tracking
  and Detach to plain chaser on a list with sub-cues (34).

### Open for Branson (§10, 27-35)
- 27. Switching the current production while a list runs: keep it running
  (as built), ask, or stop it?
- 28. No current production until one is chosen -- or the first made /
  imported becomes current?
- 29. A sub-cue's wait counts from the one before having **faded in** (its
  hold); QLab counts from its start. Which?
- 30. With sub-cues, GO plays 4.1 with 4.1's fade; the cue's own fade is
  unused. Make the first sub-cue take the cue's fade?
- 31. Run tab keys (↑/↓ STANDBY, Esc, a Sub GO key)?
- 32. Inputs on the Cue Runner VC widget?
- 33. Swap: a Cue List on the current main list becomes a runner following
  "the current production" -- or pinned to that list?
- 34. Convert / Detach / import with sub-cues -- build them through?
- 35. Build the tracking sheet next?
- Unchanged: 1, 3, 4, 5, 8; 26 (palette delete) is being built elsewhere.

### How to try it on the Mac (on a COPY of a show)

```sh
cd ~/git/qlcconsole
git fetch ssh://branson@192.168.1.245/home/branson/git/qlcconsole-showtree feat/show-tree:feat/show-tree
git checkout feat/show-tree
./check-all.sh                                   # the three macOS legs
cp test-workspaces/DanceHysteria-0.5.qlcc /tmp/dh-p7.qlcc
build/main/qlcconsole -o /tmp/dh-p7.qlcc
```

1. **Productions.** Import a chaser (a new production), make a second one
   (+ Production, a list, two cues). Header **Production:** → pick one; ⌘↩
   plays its main list; the footer says "<production> ▸ Next: …". Its page:
   Main list, Timecode. Right-click a list → Play on GO.
2. **Sub-cues.** Select a cue → **+ Sub-cue** twice; give 4.1 and 4.2 layers;
   4.2's page → Advance: Auto-follow after 0.5 s. GO into the cue (4.1
   plays), ⌥⌘↩ (or wait) for 4.2, GO on: the next cue starts from the cue's
   own state. Type `sub "Strobe" subgo`, `cue 4.2`, `help sub`.
3. **Run tab** (⇧⌘0): GO / Sub GO / Back / Sub Back / Pause / Stop, Space
   = GO; click a cue (STANDBY), double-click (jump); the NEXT and SUB NEXT
   lines say what changes; LIVE / DESIGN and the timecode in the header.
4. **VC:** Add → New Cue Runner (properties: plays, Compact / List); put a
   Cue List on a tree list → Edit → **Swap Widget**. GO from the footer and
   from QLab (`/go`, `/subgo`, `/go 4.2`): every runner follows.
