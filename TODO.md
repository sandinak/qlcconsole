# TODO — Programmer / Programming-tab work

Tracking future work for the fork's programming/looks workflow. Done items
move to the bottom or get deleted. See also the session memory under
`~/.claude/.../memory/palette_tree_integration.md`.

## In progress / next
(nothing active)

## Next
- [ ] **Matrix head-layout: place/move a sub-group as a unit** — in
      `FixtureGroupEditor` (Fixture Manager): (1) add a whole fixture group's
      heads as a block; (2) colour cells by their source sub-group so they're
      visually distinct; (3) move a sub-group block as a unit. Meatier change
      to the QTableWidget grid — own focused turn.

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
