# TODO — Programmer / Programming-tab work

Tracking future work for the fork's programming/looks workflow. Done items
move to the bottom or get deleted. See also the session memory under
`~/.claude/.../memory/palette_tree_integration.md`.

## In progress / next
- [ ] **Fixture group folders** — `FixtureGroup` gains a `path` (+ XML), the
      Fixtures & Groups tree nests groups by folder; distinct group vs folder
      icons. (Programming-tab tree first; Fixture Manager later.)
- [ ] **Strobe region bands on the intensity gradient** — paint capability
      regions (e.g. strobe ranges) on the intensity bar, not just the textual
      capability name.

## Backlog
- [ ] **Fixture-channel editor in the canvas** — when a single fixture is
      selected in a scene, show that fixture's DMX channel editor (per-scene
      values) in the bottom of the center panel.
- [ ] **Matrix head-layout: move a sub-group as a unit** — when placing a
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
