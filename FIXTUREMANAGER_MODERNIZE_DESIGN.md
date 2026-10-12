# Fixture Manager modernization

Branson: "look at fixture manager and see what we can do to modernize it and
maybe integrate it more with the rest of qlcconsole" (2026-09-02), added to
TODO.md as a design-first investigation. This doc records that investigation,
the decisions made from it, and what was deliberately left alone — done
overnight on an isolated branch (`feat/fixturemanager-modernization`, not
merged to `main`) while Branson was asleep, so everything here is reviewable
before anything lands.

## The audit

Full structural audit of `ui/src/fixturemanager.{h,cpp}` (2681/295 lines),
`ui/src/fixturetreewidget.{h,cpp}`, `ui/src/fixturegroupeditor.{h,cpp}`,
`ui/src/rdmmanager.{h,cpp,ui}`, cross-referenced against the reworked
`ui/src/connectionstree.cpp` (this session's Devices/Connections tree
rework) and a line-by-line diff against upstream QLC+ 4.14.2 at
`~/git/qlcplus`. Five findings mattered for scoping this work:

1. **The tree structure itself is fine.** Three independent root sections
   (fixture-group folders, a "Power" root, a "Universes" wrapper), each with
   sensible CRUD already — genuinely fork-original work (folders,
   empty-group creation, the Power section, `UniverseUsageWidget`, the
   tripled-in-size drag-rich group editor), not something inherited or
   broken. **Not touched by this pass.**

2. **The context menu did not follow the Connections/Devices convention**
   ("specific-to-this-row first, generic/universal actions last, appended
   via one shared helper" — `ConnectionsTree::appendUniversalMenuActions()`).
   Fixture Manager's `slotContextMenuRequested()` did the opposite: a fixed
   7-action block (Add/Add RGB/Properties/Test/Remove/Group/Ungroup) was
   added **first and unconditionally**, before any row-kind check, so
   right-clicking the Power root, a universe row, or empty space still
   listed Properties/Test Fixture/Remove/Ungroup — merely disabled, never
   omitted. This is the concrete violation this pass fixes (see "What
   changed" below).

3. **That inverted ordering is inherited from stock upstream QLC+, not a
   fork regression.** Upstream's equivalent function is 9 lines, same
   unconditional block, same shape. The fork only ever added row-aware
   extras (folder/power/composite blocks) *alongside* the untouched
   upstream base, never revisited the base itself. Fixing it is finishing a
   modernization the fork already started on the conditional half, not
   undoing fork work.

4. **RDM is fully separable from this problem, and not part of this pass.**
   The RDM tab is commented out — inherited from an *upstream* "ui: revert
   mistaken change" commit (`5d1e87d338`, 2025-11-29), not a fork decision.
   `rdmmanager.cpp` is otherwise byte-identical to stock upstream
   (whitespace-only diff). It contributes **zero** to Fixture Manager's
   current on-screen crowding — everything actually crowding the tab today
   (Power section, Universe Usage, folders, the three-routes-to-group
   problem below) is fork-original and would exist identically if RDM were
   deleted from the source tree entirely. TODO.md's existing RDM item
   (DMX-Workshop-parity wishlist, needs its own design doc + a hardware-
   blocked UID-persistence decision) is real but is a **separate** design
   problem from this one. **Not touched by this pass** — RDM stays exactly
   as dormant as it already was.

5. **A few smaller, independently-fixable inconsistencies**, not requiring
   any judgment call: `expandAllAction`/`collapseAllAction` were local
   variables (every other action in the class is a stored `m_*` member,
   constructed in `initActions()` — these two were built directly inside
   `initToolBar()` instead, a real construction-pattern crack from organic
   growth); their icons (`:/edit_add.png`/`:/edit_remove.png`) were reused
   from the Add/Delete actions a few buttons over, semantically wrong (this
   is a tree-state toggle, not a create/delete) and inconsistent with
   Connections' own icon-less "Collapse/Expand everything below."

## What changed (this pass)

- **`slotContextMenuRequested()` restructured**: the base action block now
  builds *last*, after every row/selection-specific block (move-to-folder,
  copy-into-group, power-circuit assignment, add-power-source/circuit,
  rebuild-composite) — matching Connections' "specific first, generic last"
  convention, with a leading separator only when the menu already has
  content. Add fixture / Add RGB panel / **Add fixture to group** stay
  unconditional (all three are genuine creation actions usable from any
  row, including empty space — "Add fixture to group"'s dropdown includes
  "New Group…", which is exactly why `slotModeChanged()` force-enables it
  regardless of selection; this was checked carefully — an earlier draft of
  this change mistakenly gated it too, which would have hidden a working
  action, not just decluttered an inapplicable one). Properties / Test
  Fixture / Remove / Ungroup — actions that operate ON an existing fixture-
  or-group selection — are now only added to the menu when one exists,
  instead of always appearing merely disabled.
  - **Verified behaviorally safe, not just "should be fine": every gated
    action's existing enable/disable logic (`slotModeChanged()`,
    `fixturemanager.cpp:248-331`) was read line-by-line before this change.
    In every case where an action is genuinely *enabled* today, my new
    selection condition (`haveFixtureOrGroupSelection`) is also true — this
    only removes the action from the menu in states where it was already
    unconditionally disabled (Power root, a universe row, empty space with
    nothing else selected). No previously-clickable action becomes
    unreachable.**
- **`m_expandAllAction`/`m_collapseAllAction`** are now real `m_*` members,
  constructed in `initActions()` alongside every other action, icon-less
  (matching Connections' `appendUniversalMenuActions()` convention for the
  same concept) instead of borrowing the Add/Delete icons.

Files: `ui/src/fixturemanager.h`, `ui/src/fixturemanager.cpp`.

Build: clean (`cmake --build build`), and ran the full `check-all.sh` gate
per CLAUDE.md's convention for a non-trivial change — Qt5 skipped (not
installed on this host, documented/expected), **Qt6 PASS**, **Qt6-Release
PASS** (both actually build/link/test this change), Qt6-Werror failed at
CMake *configure*, before reaching any source file — confirmed unrelated to
this change (see the new TODO.md entry: CMake 4.4.0 rejecting a
`-Werror=<category>` flag name at configure time, pre-existing environment
drift, not a code regression; Qt6 and Qt6-Release both compile the exact
same warning-sensitive code cleanly). No dedicated `FixtureManager` unit
test exists to extend. Manually launched the built app and confirmed the
Fixtures tab renders and the tree populates correctly (screenshot-verified).

**Interactively verified** (2026-09-03, once you were up and asked to see
it): installed `cliclick` (was absent; TODO.md's own "GUI headful
automation" item tracks this gap generally, still worth building a real
`gui-drive.sh` around it per that item rather than reaching for it ad hoc
each time) and right-clicked for real. Confirmed: the Power root's menu is
now "Add power source…" / "Add fixture…" / "Add RGB panel…" / "Add fixture
to group…" only — Properties/Test Fixture/Delete items/Remove fixture from
group are gone, where they used to appear disabled. A fixture row
("US1aB") still shows the full menu including all four of those, since a
fixture selection is exactly when they apply. Not separately re-checked: a
plain universe row and empty space (same code path as the Power root,
governed by the same `haveFixtureOrGroupSelection` condition, so covered
by the same trace + this same visual confirmation, but not individually
screenshotted).

## What was deliberately investigated and NOT changed

These are real findings from the audit, each a genuine product/UX decision
that benefits from your input rather than an overnight unilateral call:

- **The always-visible 15-action toolbar.** Connections/Devices has *no
  toolbar at all* — pure tree + row-specific context menus. Fixture Manager
  keeps the old upstream shape: one wide toolbar whose actions are
  individually enabled/disabled per tab and per selection rather than not
  shown at all. Removing it (or trimming it) would be a much more visible,
  everyday-use interaction-model change than the menu-ordering fix above —
  genuinely "integrate with the rest of qlcconsole" in spirit, but a bigger
  swing I didn't want to take without you. **Recommendation if you want to
  pursue it**: don't delete the toolbar outright — Connections' toolbar-free
  design works partly *because* its actions are inherently tied to a
  specific row (there's no "act on nothing" concept there). Fixture
  Manager's Add/Add-RGB/Group actions are meant to be reachable even with
  nothing selected (empty-doc bootstrapping), which a pure context-menu
  design would make harder to discover for a first-time user. A middle
  ground worth considering: trim the toolbar to just the creation actions
  (Add/Add RGB/New Group) and move everything selection-dependent
  (Properties/Test/Remove/Ungroup/Move Up/Down/Import/Export/Remap) to
  context-menu-only, closer to Connections' philosophy without losing
  empty-tree discoverability.
- **Three different UI routes to "put fixtures in a group,"** each with
  different semantics and no menu-text distinction: the toolbar/context
  "Add fixture to group" dropdown (*assigns* fixtures to a group,
  membership mutation), "Copy into group" (*copies* whole fixtures/groups
  as a block, preserving layout), "Create group from selection…" (same
  copy semantics, into a brand-new group). A user has no way to tell these
  apart from the menu text alone. Unifying or relabeling these is a real UX
  call (do you want assign and copy to actually behave differently, or was
  that drift?) — not something to guess at unsupervised.
- **RDM's future** — per finding 4 above, genuinely a separate design
  question (TODO.md already tracks it with its own wishlist and open
  blockers). Not sized or scoped further here.

## Why this scope, not more

Given the choice between a large, sweeping redesign made unilaterally
overnight and a smaller, fully-traced, zero-regression-verified fix plus a
clear writeup of the bigger open questions, I chose the latter — the
toolbar and three-routes-to-group findings are both real, but both are
product decisions with more than one reasonable answer, and you weren't
available to weigh in. The menu-ordering fix had exactly one reasonable
answer once the Connections precedent existed, which is why it's the one
thing actually shipped on this branch tonight.
