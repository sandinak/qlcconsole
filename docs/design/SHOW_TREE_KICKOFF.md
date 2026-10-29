# Show Tree — planning kickoff (for a fresh session)

Branch `design/show-tree` (this worktree, not pushed). Read `SHOW_TREE_DESIGN.md`
first; the clickable mockup is https://claude.ai/artifact/EqJ8iZqutBSRZJ6aBR5jSS
(source: the 2026-10-16 session's scratchpad `showtree-mockup/index.html`).

Goal of this session: settle the open planning items below, update the design
doc, and end with a P1 build plan ready to hand to an implementation run.
No code on `main`/`release/*` from here; 0.1.0 ships first.

## Open planning items

1. **Decisions D1–D27** (§0): none reviewed by Branson yet. Go through them;
   record overrides in the doc.
2. **Hand-over spike (blocking).** Baking only works if a Scene shared by two
   consecutive cue Collections keeps running across GO (§4). Check
   `ChaserRunner` step change order: if the old step stops before the new one
   starts, the shared scene re-fades and effects lose phase. Write a small
   engine test before anything else; it decides "no engine change" vs. "start
   new, then stop old".
3. **File format.** `<Production>` element: where the tree lives in the
   .qxw/.qlcc, node ids, `Generated="ShowTree"` marks, the hidden
   `Productions/` folder, how inline (unnamed) palettes are stored, and what an
   older build does with the file.
4. **Undo.** The app has no general undo stack; decide the model (tree-edit
   commands with deterministic re-bake, §4) and whether Programming shares it.
5. **Re-bake cost and safety.** Debounce, orphan-scene deletion while running,
   swapping running scenes (`requestPaletteRefresh()` / `resetRuntime()`), and
   the `m_valueListMutex` rule for anything live preview touches.
6. **Import on a real show.** Dry-run "Import chaser as cue list" against
   `test-workspaces/DanceHysteria-0.5.qlcc`: what becomes layers vs. opaque
   function layers, the HTP-conflict list, generated-scene count.
7. **Gobo mapping (§3A).** How a named gobo palette ("Breakup") finds the right
   wheel slot per fixture definition — name match, manual map, or both.
8. **Cross-doc consistency.** SUBCUES, SHOW_CONTROL, MOVEINBLACK,
   SHOW_LIFECYCLE ("Production" phase → "Run", D1), and the line-target design
   (`feat/line-targets`, needed before P3).
9. **Test strategy.** Offscreen QTest only (no live screen, no DMX out):
   bake determinism, imported list vs. original chaser output, cascade rules
   from the §3 worked examples as table tests.
10. **P1 cut.** Confirm the P1 slice in §8 is the smallest useful one, and
    split it into mergeable steps.
