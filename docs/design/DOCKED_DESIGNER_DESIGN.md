# The docked effect designer (board Q263–Q266)

*Design note, 2026-11-22, for board Q263–Q266 (decided 2026-11-18; see
[docs/project/TODO.md](../project/TODO.md) "Decided 2026-11-18" and
[WORKFLOW_EVAL_2026-11-17.md](WORKFLOW_EVAL_2026-11-17.md) R1, R3, R7 and
Q-E1–Q-E4). It finishes [WORKFLOW_DESIGN.md](WORKFLOW_DESIGN.md) §3.4 W4
for cues: "the editor pane becomes the effect designer … ↩ to the Show
Tree". Nothing here is a new tab, model or playback path.*

**ROI:** reach = everyone who puts an effect on a cue (every class, every
show: effects are lesson 2.4 and the busking Hits); value = one designer
instead of four surfaces, the effect seen on its own heads before it is
named, and no modal window hiding the tree and the Library; cost = M
(2–4 days: one factory branch, four entry points, the modal's logic moved
into a non-visual host, the course's steps 5–7); risk = the draft's
lifetime (what happens when you click away mid-design) and the preview
(two things playing at once), both answered in §4 and §5 and tested.
Rough ROI 1.

---

## 1. What opens the modal today, and what opens instead

`EffectLayerDialog` (ui/src/effectdesigner.h:343, .cpp:1627–1790) is a
`QDialog` that owns an `EffectDesigner`, works out the layer's targets and
base looks, and writes the result onto the layer in one Show Tree batch.
`EffectLayerDialog::design()` runs it with `exec()`.

| # | Entry point | Code today | Q263/Q264 opens instead |
|---|---|---|---|
| E1 | Layer page's **Effect** row ⋯ → **Design a New Effect…** / **Design Effect…** (`stDesignEffect`) | showtreeview.cpp:6379–6382 → `EffectLayerDialog::design(…, key)` | the docked Workbench on that cue and layer, the designer in the editor pane, started on that row's effect |
| E2 | Layer right-click → **Design Effect…** (`stDesignEffectLayer`) | showtreeview.cpp:9254–9256 → `EffectLayerDialog::design(…)` (no key) | the same, started fresh (a new kind goes beside the layer's effects) |
| E3 | Layer page's Effect row **✎** | showtreeview.cpp:5844 → `editPalette(pid)` → the Look Editor under the page | the same docked designer, on that effect (every other attribute's ✎ keeps the Look Editor) |
| E4 | Library ▸ **+ Palette ▸ Effect** | showtreeview.cpp:11619 → `newPaletteFromLibrary(Effect)`: an empty palette, named "New…", in the Look Editor | the docked designer on the **bench**: pick it, watch it, then **Save in This Show…** names and files it. No palette exists until then |
| E5 | Library right-click an effect palette → **Edit Effect** | showtreeview.cpp:11451 `place.edit` → `editPalette(pid)` → the Look Editor | the docked designer started from that palette (**Update "x"** writes it back). D-3 closed: "Edit Effect" means the same thing in both tabs |
| E6 | Docked Workbench: click a layer's **Effect** row | workbench.cpp:2743 asks the factory, which declines, so the Look Editor shows the palette | the designer, on that layer (the ask already there starts working) |
| E7 | Docked Workbench: **+ Effect ▾ → Effect Palette…** | workbench.cpp:580 → `designEffect(EffectRequest())` → the bench, after `clear()` unloads the cue | on a cue layer: the designer on that layer (what the tooltip has always said); the cue stays loaded |
| E8 | Build ▸ Effects **+ New** / click an effect / right-click **Edit Effect** | programmingmanager.cpp:880, 894, 923, 1238 → `ProgrammingManager::designEffect()` → the bench pane | unchanged (this is the surface the others now match) |

Everything else that shows an effect palette — the Look Editor's browser,
presets, WLED, Import / Export — stays where it is; the designer hosts the
same `LookEditor` inside itself (effectdesigner.h:211).

After this, `EffectLayerDialog` has no callers. It goes, and with it the
only modal in the effect path (D-2).

## 2. Where the docked designer lives

Three files, and one new non-visual class:

- **ui/src/workbench.\*** — `Workbench::EffectRequest` (workbench.h:200–214)
  already carries everything a cue layer's design needs: `cueId`,
  `layerId`, `targetSignature`, `key`, `palette`. `Workbench::designEffect()`
  (workbench.cpp:2766) builds one from the selected row and asks the static
  `EffectPaneFactory` for a widget, then `hostEffectPane()` puts it in the
  editor pane under "**Design effect** — <layer>". *The plumbing is
  finished; only the factory declines.* Three changes:
  1. `EffectRequest` gains `tryOn` (groups the bench should try on), for E4.
  2. a public `designEffectOn(cueId, layerId, signature, key)` so a caller
     that knows the layer (E1–E3) does not depend on which row happens to
     be selected; it selects that layer's row first, then designs.
  3. the bench branch (`designEffect(const EffectRequest &)`) stops calling
     `clear()` when a `TreeCueStack` is loaded: docked, the cue stays
     loaded and plays in context behind the bench (§5), and ↩ goes back to
     its page instead of nowhere. With a Build scene or collection loaded
     it behaves as it does today.
- **ui/src/effectdesigner.\*** — `EffectDesigner` itself needs **no**
  change: it already takes targets, base looks, a one-off label, the
  In Context / Solo row, and reports with `effectChosen()`. What changes is
  `plugIntoWorkbench()` (effectdesigner.cpp:429): instead of
  `req.cueId != invalid → nullptr`, a cue request builds a designer **and**
  an `EffectLayerHost` that configures and applies it. `EffectLayerHost` is
  `EffectLayerDialog` with the window taken off: the same target / base /
  previous-look reading (`.cpp:1627–1736`), the same `apply()` batch
  (`.cpp:1748–1782`), no `QDialog`, no `exec()`. The dialog's two jobs that
  were really the window's — Cancel, and restoring the tree's previous
  preview — move to §4 and §5.
- **ui/src/programmingmanager.\*** — Build's half is the model and does not
  move. `ProgrammingManager::designEffect(paletteId)` (programmingmanager.cpp:1136)
  switches to Build ▸ Effects, asks the Workbench for the bench pane, and
  wires "saved → listed, selected, on the bench again". The Show Tree's new
  entry points do the Show-Tree-shaped equivalent (dock, then design), and
  `lendWorkbench()` (programmingmanager.cpp:3337) keeps working untouched:
  the Workbench that gets lent is the one that hosts the pane, so the
  designer travels with it.

Help, course and the Library's **Effects** heading are the same words in
both tabs after this; §7 lists the text.

## 3. One designer, two shapes

| | A cue layer (E1–E3, E6, E7) | The bench (E4, E5, E8) |
|---|---|---|
| Plays on | the layer's groups / fixtures (`setTargets`) | the **Try on ▾** groups |
| Under it | the layer's other looks (`setBaseLooks`) | nothing |
| Keeps as | **Keep on This Layer** (one-off), **Save in This Show…**, **Update "x"** | **Save in This Show…**, **Update "x"**, **Make a Layer ▾** |
| Context row | **In Context / Solo** (§5) | hidden (the cue, when one is loaded, plays in context) |
| Heading | "**Design effect** — 1.0.6 Dance Break · Steps" | "**Design effect** — <name>, on the bench" |
| ↩ / Esc | the layer's page (§4) | the cue's page, or the scene it came from |

## 4. A design that is mid-edit when the user goes somewhere else

The draft is a hidden one-off palette played by a hidden scene; nothing in
the show changes until a button is pressed, and `cancel()` leaves no trace
(effectdesigner.h:46–52). `hasUnkeptChanges()` says whether leaving would
drop work. The modal answered this by being modal. The pane answers it the
way the bench already does (workbench.cpp:920–975, `backOut()`):

| What the user does | What happens |
|---|---|
| **↩ Show Tree**, **Esc**, or ← out of the pane | asked once — *"Leave the effect designer? This effect hasn't been kept."* **Keep designing** (default) / **Leave** — then the layer's page comes back. Unkept: nothing kept, as Cancel |
| clicks **another row** in the layers column (including another layer) | the same question; **Keep designing** puts the selection back on the layer being designed |
| clicks **another cue** in the tree beside the dock | the same question (the tree's selection is put back on **Keep designing**) |
| **leaves the tab** (`suspendWorkbench()`) or the cue is deleted / undone away | the draft is dropped silently, as closing the modal window did. Coming back shows the layer's page, not the designer |
| a **GO** takes the stage | the preview is held (`m_heldByRun`, RC8) and the draft stays: the designer keeps working, nothing plays |
| **Save in This Show…** / **Keep on This Layer** / **Update** | kept (one Show Tree batch, `EffectLayerHost::apply()`), and the pane stays open on what was kept — `markKept()` means the next click away asks nothing |

A re-entrant question is the one trap here: the ask happens in the row
handler, so the guard sets a flag while the box is up and ignores the
selection change it causes itself. Tests drive it through
`Workbench::setConfirm()` (workbench.h:120), which already exists for the
bench's question, so no message box runs in the gate.

## 5. Preview: In Context and Solo without two Solos

The modal did its own previewing: `previewCue()` on open, the tree's
previous preview restored in the destructor, and **Solo** = stop the
cue's preview, leaving only the draft's (effectdesigner.cpp:1707–1746).

Docked, the Workbench is **already** previewing the cue: `TreeCueStack`
`playsItself()`, and `startOwnPreview()` either previews the cue (in
context) or plays the selected layer's scene alone (Solo)
(treecuestack.cpp:990–1053). It also already has a **Solo** button in the
header (`Workbench::setSolo`, workbench.cpp:2849).

So the pane does **not** preview on its own:

- the designer's **In Context / Solo** row stays visible (Q263 asks for
  it) and drives `Workbench::setSolo()`; the stack is the single source of
  truth, so the header's Solo button and the row always agree (both read
  `WorkbenchStack::solo()`; `setSolo()` returns early when nothing
  changes, so no loop).
- **In Context** = the cue plays, the draft's preview scene over it on the
  layer's heads — what the modal did.
- **Solo** = only that layer's scene plays, with the draft over it. That is
  a change of meaning from the modal's "only the effect", and a better one:
  the layer's own colour and dimmer are what the effect is designed over
  (`setBaseLooks`), so Solo now shows exactly the layer, alone.
- nothing is restored on the way out: the tree's preview was never taken
  away. The modal's destructor dance goes.

## 6. What the Show Tree's docked Workbench needs

`showtreeworkbench.cpp` (W3) needs **no new mechanism** — the dock, the
lender, the tree-follow and the lent pages all stay. Four small things:

1. **A door that docks.** E1–E3 and E4–E5 call
   `openInWorkbench(cueId, layerId, signature)` (showtreeworkbench.cpp:200)
   or `dockWorkbench()` (:116) first, then design. Both are there already;
   the new `ShowTreeView::designEffectOnLayer()` and
   `designEffectPalette()` are the two thin wrappers.
2. **`workbenchFollowTree()` must not steal the pane.** It re-opens the
   Workbench on the tree's current cue whenever the tree's selection
   changes (:325). With a designer up, it asks §4's question first and puts
   the selection back when the answer is Keep designing.
3. **`closeWorkbench()` / `suspendWorkbench()`** call `bench->clear()`,
   which retires the editor pane, so the draft dies with it. That is the
   wanted behaviour (§4, "leaves the tab"); it needs no code, only the
   test that proves the draft left no palette behind.
4. **The hint line.** "…is in the Workbench: ↩ Show Tree puts the page
   back" becomes, while designing, "Designing the effect for <layer>: Keep
   on This Layer, or Save in This Show" — the sentence the modal's buttons
   used to carry.

`ProgrammingManager::lendWorkbench()` is unaffected: it is the same
`Workbench` instance either side, and it keeps what was loaded in
`m_lentFrom`. A designer pane open when Build asks for its Workbench back
cannot happen — Build only gets it back through `closeWorkbench()`, which
is case 3.

## 7. Q265 (a Build scene dropped on a cue) and Q266 (the name)

**Q265** is independent of the designer and lands in the drop path, not
the designer: `ShowTreeView::dropFunctions()` (showtreeview.cpp:7487) makes
every function an opaque ▣ layer. Decided: a **Build scene of groups and
looks** lands as **layers** (its groups and looks copied as the cue's own,
palettes shared), ⌥ keeps it ▣, and a ▣ look-scene's menu gains **Make It
Layers…** in the slot beside **Capture into Palettes…**
(showtreeview.cpp:9230). The reader that already does this is Upgrade This
Show's step-scene reader ("A scene of groups and palettes became a
layer"); the `same()` check tells a look-scene from a raw one. The original
scene is kept, and **Used in** must say that the cue now has a copy, so a
later edit in Build no longer reaches it.

**Q266** keeps the name **Show Tree** and pays the findability cost in
three strings: the tab's tooltip, the **Window** menu entry's tooltip and
the help title (`# Show Tree` in docs/features/show-tree.md, which is what
`HelpBrowser::titleOf()` reads, helpbrowser.cpp:81). Each says *cue lists,
cues and hits*. No rename, no new menu item, so the menu count is flat.

## 8. Order of work, and what each step proves

1. **Q266** (strings + test): the tooltip, the Window entry, the help
   title. Its own commit.
2. **`EffectLayerHost`**: the modal's logic with the window taken off, the
   existing designer tests ported onto it (same assertions, no `QDialog`).
3. **The factory accepts cue requests**, `designEffectOn()`, the row /
   back-out guard, the Solo wiring. E6 and E7 start working for free; the
   `+ Effect ▾ → Effect Palette…` tooltip becomes true and its test checks
   *where the effect landed*, not just the tooltip's words
   (workbench_test.cpp:510).
4. **E1–E3**: the Show Tree's doors dock and design; `EffectLayerDialog`
   deleted.
5. **E4–E5**: the Library's **+ Palette ▸ Effect** and **Edit Effect**;
   `showLibraryEffects()`'s hint wired (D-5).
6. **Help and course**: effects.md "Open it from any of three places" → two
   shapes (§3), show-tree.md's Effects and Workbench paragraphs, build.md's
   stale + Effect lines (D-4), lesson 2.4 steps 5–7 ("pick it, watch it on
   the steps, name it, Save in This Show"), the glossary's *Effect
   designer* and *One-off effect*.
7. **Q265** last: it shares no code with the designer, so it can slip to
   its own branch without blocking anything above.

Shots for the UX review: the docked designer on a cue layer and on the
bench, at 1024×640, 1440×900 and 1920×1080, beside the same cue's page
before the change.

## 9. What was built (2026-11-22, feat/053-docked-designer)

Q266 and Q263 + Q264 as above, with four things worth recording because
they differ from, or add to, the plan:

- **No Cancel in the pane.** The designer's Cancel stays hidden either
  side (as the bench's always was): **↩** and **Esc** leave, and they ask
  first when nothing has been kept (§4). One way out, not two.
- **Save in This Show… files where the Library stands.** `+ Palette ▸
  Effect` always filed the new palette in the folder selected in the
  Library (the song's folder, which lesson 2.4's exit ticket asks about),
  so the request carries it: `EffectRequest::folder` →
  `EffectDesigner::setSaveFolder()`. Without this the effect would have
  landed in the show's effects folder and the lesson's claim would be
  false.
- **What the designer keeps shows in the Library at once**, and a new
  palette's name is opened to type there, as Build ▸ Effects opens it
  (`ShowTreeView::followDesignerIntoTheLibrary`). The course's "name it"
  step needs it.
- **A latent crash, fixed:** a child going down can still change the show
  — the designer drops its hidden preview scene as it is destroyed — and
  `Doc::functionRemoved` reached `ShowTreeView::updateBridgeBanner` with
  the banner's widgets already freed. Both `~ShowTreeView` and
  `~Workbench` now let go of the show's signals first. Reachable before
  this work through `+ Effect ▾ → Effect Palette…` in a Workbench the
  tree owns (no lender).

**Left open: Q265** (§7, a Build scene dropped on a cue lands as layers,
⌥ keeps it ▣, a ▣ block gets **Make It Layers…**). It shares no code with
the designer: the drop path (`ShowTreeView::dropFunctions`) and the menu
slot beside **Capture into Palettes…**, reusing Upgrade This Show's
step-scene reader. Its own branch.
