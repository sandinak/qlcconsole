# Workflow UX review (2026-08-18)

A design review of qlcconsole's core workflows, requested while PMJ hardware
wasn't available: fixture setup → building looks (Programming tab) →
building shows (Function Manager / Show-Timeline) → running a show
(Virtual Console, live safety controls). Scope was "gaps in workflow, usage,
simplicity, and inherent understanding of how to do things" — discoverability
and first-time/infrequent-user comprehension, not a correctness or code-style
review. Method: four parallel passes over the actual UI code (labels,
tooltips, empty states, drag/drop handlers, menu structure), each
cross-checked against `TODO.md`/`DONE.md` so already-flagged issues are
marked as confirmations, not new findings.

## The pattern underneath the findings

The app already contains the *right answer* to most of what's below — it's
just applied in one place and not the others. Three examples:

- **Empty-state onboarding hints exist and work well** — Fixture Manager's
  "No fixtures, click [icon] to add fixtures" and the Show timeline's
  empty-state hint are both good, specific, low-friction. Neither the
  overall blank workspace nor the Programming tab's empty palette tree gets
  the same treatment.
- **Footer/toolbar safety chips exist and work well** — Blackout, Blind, and
  Show Lock are all instantly visible from every tab, no menu-diving. Grand
  Master, arguably the *first* fader an operator reaches for to save a bad
  look live, is siloed inside the Virtual Console tab instead.
- **Content-aware visual summaries exist and work well** — the Programming
  canvas's look tiles, target-coverage warnings (⚠ no look for gobo/shutter),
  and per-look fade tooltips all show *what a thing actually is* at a glance.
  Chaser steps and Show-timeline blocks don't: both show only a name, so
  "what does step 4 actually paint" requires opening something else to find
  out.

None of this needs a new design language — it needs the existing one applied
where it's currently missing. That reframes most of what follows from "add a
feature" to "extend a pattern that's already proven in this codebase."

A second, smaller theme: **the same word means different things in
different tabs, with nothing on screen to tell them apart** — "Group"
(fixture-group vs. channel-group), "Pan/Tilt" vs. "Aim" (raw XY vs.
rig-geometry-computed), and undefined timeline vocabulary (Track/Cue/Show)
all cost a user real confusion at the exact moment they're choosing between
two similar-looking options.

---

## 1. Fixture setup (Fixture Manager, groups)

1. **No onboarding for a brand-new workspace.** `App::clearDocument()`
   (`ui/src/app.cpp:750`) resets engine state with nothing pointing a
   first-time user toward Fixture Manager — no welcome panel, no "start
   here." The empty-state hint pattern exists elsewhere in the app (Show
   timeline) but wasn't applied to the very first screen a user sees.
2. **"Group" means two unrelated things, unlabeled, in adjacent tabs.**
   "Fixture Groups" (`fixturemanager.cpp:439`, spatial head-layout groups
   for the 2D grid/XY-pad) and "Channel Groups" (`fixturemanager.cpp:455`,
   DMX-channel grouping for Simple Desk) sit next to each other with no tab
   tooltip distinguishing them. Worse: the single `m_addAction` toolbar
   button silently changes meaning ("Add fixture..." vs. "Add group...")
   depending on which tab is active (`slotTabChanged`,
   `fixturemanager.cpp:1222-1240`) — same icon, same position, different
   action.
3. **The head-layout grid's purpose is never explained.** `CreateFixtureGroup`
   (`ui/src/createfixturegroup.ui`) has "Width"/"Height" fields with zero
   tooltips — nothing in the dialog itself tells a user this grid is what
   later powers XY-pad/per-head effects in the Programming tab.
4. **Group creation is one level deeper and mislabeled vs. fixture
   creation.** Adding a fixture is a single clearly-labeled toolbar button.
   Adding a group requires opening the "Add fixture to group..." dropdown
   and picking "New Group..." from inside it — a button whose own tooltip
   doesn't read as "create a group."

**Confirmed good, not a gap:** the Add Fixture dialog itself (single page,
full tooltips, inline duplicate/collision errors); Fixture Manager's
empty-fixtures hint; folder organization (drag-to-folder, right-click move)
is implemented and working per `DONE.md`.

**Already flagged, still open:** RDM Manager as a bolt-on protocol inspector
with no onboarding of its own (`TODO.md` ~line 1958, "Fixture Manager is
already crowded").

## 2. Building looks (Programming tab)

1. **The Save button doesn't do what its name implies.** `m_saveBtn`
   (`programmingmanager.cpp:276-279`) is the *only* Save-labeled control in
   the tab, but its tooltip is "Save joystick position edits to the
   workspace file" and it stays disabled until a joystick pan/tilt drag
   happens. Every ordinary look edit (palette drag, color change, gradient
   adjustment — ~30 call sites in `lookeditor.cpp`) calls
   `m_doc->setModified()` directly and is immediately live in memory, with
   nothing in this tab telling the user their scene work exists but isn't
   yet on disk. A user could reasonably believe "Save" is what commits their
   look edits and be confused when it stays greyed out.
2. **Palette-type choice is unexplained at the moment of choosing.** The
   palette tree's right-click "New…" menu lists nine bare `QAction`s with no
   tooltips (`programmingmanager.cpp:2104-2114`). "Pan/Tilt" and "Aim" look
   like near-synonyms but do very different things (raw XY-pad values vs.
   rig-geometry-computed aim at a named stage target) — nothing at creation
   time helps a user pick the right one.
3. **Empty-palette-tree guidance is easy to miss.** The canvas placeholder
   text presumes palettes already exist; the actual "how to create one" hint
   ("Right-click to add a palette") is a small label in a different pane the
   user may not be looking at.
4. **Target drop zones are cosmetic, not functional.** `SceneGroupLooks
   ::dropEvent` (`scenegrouplooks.cpp:643-796`) dispatches purely by MIME
   type, not drop position — a palette dropped on the "Targets" column still
   becomes a look. Forgiving rather than broken, but inconsistent with the
   strong two-column "drag here" visual language, and worth confirming this
   is the intended design rather than an oversight.

**Confirmed good, not a gap:** SceneGroupLooks header text, per-look fade
tooltips, target-coverage warnings, and the look-editor's type-to-page
switching are all clear on screen. The Scene/Look/Target/Scope terminology
audit from `TODO.md` is confirmed complete — no drift found.

## 3. Building shows (Function Manager, Chaser, Show/Timeline)

1. **Chaser steps and timeline blocks show only a name — never content.**
   `ChaserEditor::updateItem` (`chasereditor.cpp:1364-1432`) and the
   timeline's `SceneItem`/`ShowItem` all render name + a generic per-type
   icon, never anything derived from what the scene actually paints. Combined
   with default names like "New Scene 7" (#4 below), "what does step 4
   actually do" is invisible without opening the Scene Editor separately.
2. **Timeline clips have no edit action at all.** Neither
   `SceneItem::contextMenuEvent` nor the base `ShowItem::contextMenuEvent`
   offers "Edit function" or responds to double-click — only "Align to
   cursor" and "Lock/Unlock." Double-click is wired at the *track* level
   only, and that just opens a track-rename dialog. This is true in both the
   standalone Show Manager and the embedded Programming-tab timeline.
3. **Consequently, there's no assisted path from a timeline clip to the look
   it plays.** With no edit action (#2) and nothing wiring a timeline
   selection to the function tree, the only route is: read the block's
   (possibly generic, #1) name, tab-switch to Function Manager or
   Programming, then manually search the tree for a matching name — easy to
   get wrong in a show with many similarly-named scenes.
4. **The `SS-PP.II-Description` naming convention has zero UI support.** No
   placeholder, validator, or auto-fill anywhere implements it. New
   scenes/chasers get hardcoded generic names (`"New Scene %1"`,
   `functionmanager.cpp:433-450`); the Programming tab's own palette-driven
   naming follows a completely different, unrelated scheme. Consistency with
   the fork's own naming convention is a pure human-discipline problem today.
5. **Timeline vocabulary is never explained in-app.** "Track," "Cue,"
   stretch/fade handle nubs — fewer than 10 tooltip/status-tip/whatsThis
   calls exist across the three largest show-manager files combined. The UI
   assumes prior DAW/console experience (fade wedges, "Logic-style" end
   handles per the code's own comment).

**Confirmed good / already flagged:** the Show/Edit toolbar split is shipped
and working (`TODO.md`). Sequences' lack of inline editing (a narrower
instance of #2/#3) is already flagged in `DONE.md` as a known open gap, not
new.

## 4. Running a show (Virtual Console, live safety controls)

1. **Grand Master is siloed inside Virtual Console; its safety-tier siblings
   are global.** Blackout, Blind, and Show Lock all live on the main toolbar,
   visible from every tab. Grand Master is a `VCDockArea` child mounted
   inside the VC tab (`vcdockarea.cpp`), with visibility further gated by an
   optional workspace property. An operator working anywhere else during a
   live show has instant Blackout/Blind but has to tab-switch to reach the
   fader that's often the first thing reached for to save a bad look.
2. **The Design/Operate toggle always shows the destination mode, not the
   current one.** Standard play/pause-style affordance
   (`app.cpp:1020-1046`), but there's no separate always-on "you are here"
   indicator — a stressed operator glancing at a button reading "Design"
   mid-show could misread it as the current state. Neither the window title
   nor the status bar carries a persistent mode readout.
3. **The cue list's "next cue" indicator is hidden behind an optional
   panel.** The orange next-step highlight is only computed in
   `vccuelist.cpp`'s `setFaderInfo()`, which only runs when the optional
   crossfade side-fader UI is enabled. Without it, "what fires when I hit
   Go" is invisible until it happens.
4. **Design→Operate is a silent transition; Operate→Design isn't.**
   Leaving a running show (Operate→Design) correctly warns and requires
   confirmation. Going live (Design→Operate) does nothing extra beyond
   force-disarming Blind (itself correct) — no "you're about to go live"
   checkpoint, asymmetric with the care taken on the other direction.

**Confirmed good / already fixed:** the VC-internal Run/Stop toggle that
used to duplicate the app-global Design/Operate action by a confusingly
similar name was already identified and removed (`DONE.md`,
"Toolbar-consolidation round 2"). The Blackout/Blind/Show-Lock footer chips
shipped this cycle are genuinely fast and discoverable. Blackout is
correctly zero-friction (no confirmation dialog); Blind is correctly forced
off entering Operate, preventing a muted rig from silently surviving into a
live show.

---

## Suggested order of attack

Roughly ranked by (impact on a new/infrequent user) ÷ (size of fix), not by
workflow area — several of these are genuinely small:

1. **Rename/rescope the Programming tab's Save button** (2.1) — either
   rename it to "Save Positions" or add a general unsaved-changes indicator.
   Actively misleading today, cheap to fix.
2. **Tab tooltips distinguishing Fixture Groups vs. Channel Groups** (1.2)
   and **palette-type tooltips in the New… menu** (2.2) — both are a few
   lines of `setToolTip`/`setStatusTip`, directly prevent a wrong pick.
3. **Promote Grand Master to the global toolbar/footer** (4.1) — matches a
   pattern already built for Blackout/Blind/Show Lock; mechanical to extend.
4. **Persistent Design/Operate mode chip** (4.2) — same footer-chip pattern
   used for Blackout/Blind, reused rather than invented.
5. **Blank-workspace and empty-palette-tree onboarding hints** (1.1, 2.3) —
   same empty-state-hint pattern already proven in Fixture Manager and Show
   timeline, just not applied to these two spots yet.
6. **Chaser-step / timeline-block content indicators** (3.1) — a color
   swatch or similar sampled from the scene, matching the "content-aware
   summary" pattern already used in the Programming canvas's look tiles.
7. **Timeline clip edit action (double-click / context menu)** (3.2) — the
   highest-effort item here, but it's the root cause of #3.3
   (unassisted cross-navigation) too, so fixing it resolves two findings.
8. **Naming-convention support** (3.4) — at minimum a placeholder showing
   the `SS-PP.II-Description` pattern in the New Scene/Chaser name field.
9. Everything else (head-layout tooltip, drop-zone precision, next-cue
   visibility, Design→Operate checkpoint, timeline vocabulary) — lower
   urgency, worth a pass once the above land.
