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

GATE_STATUS_PLACEHOLDER

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
