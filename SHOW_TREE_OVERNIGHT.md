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
