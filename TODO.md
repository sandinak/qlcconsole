# TODO — Programmer / Programming-tab work

Tracking future work for the fork's programming/looks workflow. Done items
move to the bottom or get deleted. See also the session memory under
`~/.claude/.../memory/palette_tree_integration.md`.

## In progress / next
(nothing active)

## Next
(nothing active — pick from Ideas/Backlog)

## Ideas / bigger bets (shower-thoughts)
- [ ] **Shareable effect "looks"/groupings** — design a full set of effect
      groupings (e.g. a rolling sweep with colour patterns) that can be
      stored and **shared with other users** as a portable bundle.
- [ ] **More movement shapes / draw-your-own** — beyond the built-in EFX
      shapes, let users define custom movement paths (draw on an XY canvas).
- [ ] **Capability plugins (RGBScript-like) for position / intensity /
      colour / etc.** — extensible generators, tied to the sharing idea so
      community-made effects can be imported.

## Backlog — when placing a
      fixture group onto a larger group's head layout, drag the whole
      sub-group as a block and keep sub-groups visually distinct (Fixture
      Manager head-layout grid editor; meatier change).
- [ ] **Blind vs live preview toggle** in the Programming tab (currently
      live preview runs the scene in Design mode only).
- [ ] **Sequence editing** in the Programming canvas (currently Sequences
      show the "edit in Functions tab" hint).
- [ ] **Tree filtering** (search box) for very large palette/function trees,
      if folders alone get unwieldy.
- [ ] Remove the now-unused `ui/src/groupselection.{h,cpp}` (backed the
      removed "Select groups…" dialog) — or repurpose.
- [ ] Decide whether to remove the Functions tab's own Cmd-C/Cmd-V duplicate
      (stock QLC) now that the Programming tab has duplicate.
- [ ] Fixture Manager should also respect fixture-group folders once added.

## Done
- [x] Palettes in the Function Manager tree with folders.
- [x] Palette drag/drop between folders + clone.
- [x] Dedicated **Programming** top-level tab (scenes/looks canvas).
- [x] Searchable/foldered palette + function source trees.
- [x] Drag palettes→looks, groups→dynamic targets, fixtures→fixed targets.
- [x] Inline look editor (color / dimmer-gradient / XY pan-tilt / gobo-shutter)
      with named gobo/shutter capability values + intensity capability hint.
- [x] Live DMX/2D preview of the edited scene (Design mode).
- [x] Removed duplicate group-look UI from the Functions view.
- [x] Collections / chasers / EFX / matrices open in the Programming canvas
      with drag-in of members/steps.
- [x] Cmd-C / Cmd-V duplicate; "New palette…"; type icons + paths on looks.
- [x] Open Programming canvas on double-click (single-click drag works).
- [x] **Fixture group folders** (FixtureGroup.path + tree folders + icons).
- [x] **Strobe region bands** on the intensity bar (CapabilityBar widget).
- [x] Fix: removing a fixed-fixture target clears its baked values (look
      no longer overridden when converting a static scene to a group scene).
- [x] Right-click create menus (function tree: new scene/chaser/…/folder;
      palette tree: new palette / move to folder); removed the +buttons.
- [x] Usage counts: scene "used in N places", look "used by N scenes".
- [x] Nest a collection/chaser's members under its node in the tree.
- [x] Palette right-click lists types (Color/Dimmer/Pan-Tilt/Gobo/Shutter).
- [x] Per-fixture DMX channel editor in the canvas (select a fixed fixture).
- [x] Live-preview any canvas function (collection auto-goes-live); keep
      the collection context/subtree when drilling into a member scene.
- [x] Recursive nesting/context: Chaser -> Collections -> Scenes.
- [x] Fix segfault from rapid preview start/stop during edits (refreshPreview).
- [x] Engine: guard Scene::write fader-map build with m_valueListMutex
      (race vs resetRuntime from live preview edits).
- [x] Fixtures & Groups tree organized by Universe (default folder).
- [x] Fixture group editor: "Add group as block".
- [x] Fixture console kept in-frame (height-bounded); multi-fixture editing
      (edit several fixed fixtures together; mismatched values untouched
      until changed).
- [x] Engine: fixture-group sub-group tagging (+XML) + colour blocks in the
      group editor.
- [x] Programming tab: bottom editors min-height (faders no longer squished).
- [x] Fixture Manager: double-click to open; drag fixtures from the tree
      onto the group editor grid at a cell.
- [x] Create empty fixture groups (group action always enabled in Design).
- [x] Move a sub-group as a unit (right-click grid → Move sub-group …).
- [x] Folders for fixture groups in the Fixture Manager (+ Move to folder).
