# TODO — Programmer / Programming-tab work

Tracking future work for the fork's programming/looks workflow. Done items
move to the bottom or get deleted. See also the session memory under
`~/.claude/.../memory/palette_tree_integration.md`.

## In progress / next
(nothing active)

## Next
(nothing active — pick from Ideas/Backlog)

## Ideas / bigger bets (shower-thoughts)
- [ ] **Command line / keypad bar (EOS-style)** — a command prompt at the
      bottom of the window to perform operations by typing (select fixtures,
      apply palettes/intensity, record, etc.), like an ETC EOS command line.
      Would parse a small grammar and drive ProgrammerController / functions.
- [ ] **Scriptable effect-plugin engine** (the big one). A JS effect like
      RGBScript but *intelligent*: operates on a static list or a dynamic
      FixtureGroup, and is handed a fixture-set descriptor
      `[{id, head, pos{x,y,z}, rot, type, hasPanTilt, panRange°, tiltRange°,
      hasRGB,…}]` built from MonitorProperties + Fixture.
      **DECIDED — data boundary = high-level INTENTS, not raw DMX**: scripts
      return per-fixture `{pan:°, tilt:°, dimmer:0-1, color/paletteRef,
      gobo:idx}`; host converts via `Fixture::positionToValues` (degrees→DMX)
      and `QLCPalette::valuesFromFixtures` (position-aware fanning), writing
      FadeChannels (no bypass of HTP/LTP). Palettes are passed IN each tick
      (named slots) so live palette edits re-colour the effect. Live inputs
      (e.g. follow-spot target XY) declared as input channels (VC XY / OSC /
      external socket). Engine choice TBD: QScriptEngine (match RGBScript) vs
      QJSEngine (Qt5-current). Phasing: P0 pan/tilt script bound to a group →
      P1 palette+color/dimmer/gobo intents → P2 live inputs (follow-spot) →
      P3 sharing/bundling. Example plugins: moving-head effects, follow-spot
      fed by external position code. See [[scriptable_effect_engine_design]].
- [ ] **Shareable effect "looks"/groupings** — design a full set of effect
      groupings (e.g. a rolling sweep with colour patterns) that can be
      stored and **shared with other users** as a portable bundle. (Ties into
      the scriptable effect-plugin engine above.)
- [ ] **More movement shapes / draw-your-own** — beyond the built-in EFX
      shapes, let users define custom movement paths (draw on an XY canvas).
- [ ] **Beam palette** — generic Beam-group raw 0-255 slider (named caps like
      Gobo/Shutter) targeting the fixture's primary Beam-group channel. ~8
      touch points mirroring the Zoom palette (enum, valuesFromFixtures,
      type↔string, icon, XML i/o, creation menu, look-editor page).
- [ ] **2D Monitor**: multi-select move (RubberBandDrag + emit per selected),
      grid substeps, snap-to-grid with levels (itemChange rounding).

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
- [x] **FIX (crash, from audit): dangling group editor on group delete** —
      deleting a group whose FixtureGroupEditor is open left m_grp dangling;
      any later interaction (Ctrl+Z/drag/menu) was a use-after-free. FixtureMgr
      now tracks m_groupEditorId and closes the editor in slotFixtureGroupRemoved.
- [x] **Hardening (from audit)** — group-editor cell drop + scene look-apply
      reconcile prompt now deferred via singleShot (no rebuild/modal inside a
      drop event). Audit confirmed the earlier teardown/defer/blockSignals
      fixes are sound.
- [x] **FIX (shutdown segfault): stale fixtures() cache during clearContents** —
      Doc deleted each fixture then emitted fixtureRemoved, but the fixtures()
      cache wasn't invalidated until after the loop, so listeners (e.g.
      FixtureGroupSource::reload → Fixture::name) iterated freed pointers. Now
      the cache is invalidated before each emit; FixtureGroupSource also skips
      reload while the Doc is clearing (Doc::clearing/loaded guard).
- [x] **FIX (data corruption): runaway "001. 001. …" in function names** — the
      Programming nav's member nesting set prefixed display text on items that
      carry a function id; that fired itemChanged → FunctionsTreeWidget rename
      handler → fn->setName(prefixed) and saved it, compounding each rebuild.
      Now syncMemberNodes blocks the tree's signals during nesting. Added a
      repair action (func-tree right-click → "Repair: strip leading 'NN. '")
      to clean already-corrupted names.
- [x] **FIX (segfault): heap corruption from rebuilding trees mid-drop** —
      drag-to-folder handlers called setPath()→reload()/updateView() (and the
      palette drop → updateTree) synchronously inside dropEvent, deleting the
      item Qt's drag machinery still referenced (use-after-free; crashed later
      in autosave/FixtureGroup::saveXML and FixtureGroupSource::reload). All
      three drop paths now defer the rebuild via QTimer::singleShot(0).
      FixtureGroup::saveXML also switched to const iteration (no detach).
- [x] **Programming tab: Look pane renders fully, canvas yields** — removed the
      QScrollArea around the look editor; it's now added directly with a
      Maximum vertical policy + 340px cap, so it always shows its full content
      (no scroll) and the canvas (Expanding) shrinks to make room. Capped the
      colour picker (300) and XY pad (280) so the pane stays compact; window
      stays resizable (canvas hits min first).
- [x] **FIX: window grew off-screen / couldn't resize** — the look-editor
      panel min height (440, for the colour page) bloated the Programming tab's
      minimum past short screens. Reduced look/fixture/note panel minimums
      (200/200/120) and the colour picker max (300); the colour page scrolls on
      short windows instead of forcing height. Also clamp the restored window
      geometry to the current screen on startup so an oversized/off-screen saved
      window is pulled back on-screen.
- [x] **2D Monitor: force single line for elongated fixtures** — aspect ≥ 3 →
      one row, aspect ≤ 1/3 → one column; only near-square fixtures use a grid.
      Fixes tall pixel bars that were rendering as 2-column wraps.
- [x] **2D Monitor: linear default for fixtures w/o physical dims** — fixtures
      with no declared physical size were defaulting to square 300×300 → square
      head grid (the "farts" wrapping). Now default to a wide:thin strip
      (width 300, height 300/heads) → single-row layout, matching real LED
      tape/bars. Declared-size fixtures unchanged.
- [x] **2D Monitor: aspect-correct fixture sizing/layout (reworked baseline)** —
      replaced the per-dimension 14px min clamp (which squashed thin fixtures
      toward square → pixel arrays wrapped to ~8 rows) with: (a) keep TRUE
      aspect ratio, enforce only a minimum AREA scaled equally on both axes;
      (b) head grid from head-count × aspect (cols ≈ √(N·w/h)) not pixel area,
      so a tape ≈ one row and a panel ≈ square grid at any size; (c) removed the
      <5px head-layout skip; (d) float cell division (no 0-size/invisible heads).
      Zoom provides per-LED detail. (monitorfixtureitem setSize, graphicsview
      updateFixture.)
- [x] **2D Monitor: dense fixtures tile as RECTANGLES (RGB tape/pixels)** —
      fixtures with >16 heads draw heads as QGraphicsRectItem filling the cell
      (zero-gap pixel array) instead of round QGraphicsEllipseItem; touching
      ellipses still read as circles-with-gaps. FixtureHead m_item/m_back are
      now QAbstractGraphicsShapeItem with setShapeRect/shapeRect helpers.
      (Follow-up option: force 1-row layout for elongated strips.)
- [x] **2D Monitor: thin fixtures now visible** — flat/step fixtures rendered a
      few px tall (setSize even skips head layout <5px), so no colour heads
      existed. Clamp each fixture to a 14px minimum in updateFixture; heads lay
      out and light up. True scale preserved above the minimum.
- [x] **2D Monitor colour rendering at zoom** — head/body outlines made
      cosmetic (width 0) so they stay 1px at any zoom instead of growing and
      washing out the colour fill. Colour-wheel heads now derive their colour
      from the capability NAME (e.g. "Blue") when the definition carries no
      explicit resource colour. Shift+wheel zoom direction fixed for macOS
      (delta arrives on the X axis when Shift is held).
- [x] **2D Monitor: grid/drag/snap/lock + zoom/pan/undo/center** —
      multi-select + rubber-band, grid subdivisions, layout LOCK (magic-sheet
      style selection surface, persisted), Shift+wheel zoom (cursor-anchored),
      Shift-drag pan (sceneRect matched to viewport for scroll range), Cmd/Ctrl+Z
      undo of moves, teal centre H/V axes. SNAP fix: snaps the whole move as a
      group on drop (was snapping each fixture independently and scrambling the
      layout). Files: monitor/monitorgraphicsview.*, monitorfixtureitem.*,
      monitor.*, engine/monitorproperties.*.
- [x] **Chaser member numbering: skip self-numbered names** — the Programming
      nav's step-number prefix is no longer added to members whose name already
      starts with a digit (e.g. "00-01.02-…"), so user SS-PP.II names don't get
      a redundant/confusing leading index. (Data was never affected.)
- [x] **Color look page fits without scrolling** — pinned QColorDialog to a
      fixed height (420) and matched the look panel min height (440), so the
      colour picker + W/A/UV sliders fit on load.
- [x] **Dynamic group drop replaces matching fixed fixtures** — dropping a
      fixture group onto a scene removes any fixed-fixture targets that are
      members of it (and clears their baked values), so the group's looks
      aren't overridden by leftover static values.
- [x] **Scenes colour-coded by composition** — function/programming tree tints
      scenes: dynamic (look/group-driven, blue), static (baked values, grey),
      mixed (purple), with tooltips (FunctionsTreeWidget::updateFunctionItem).
- [x] **Targets grouped by fixture type** — SceneGroupLooks target list is now
      a tree: dynamic groups at top, fixed fixtures under per-type folders
      ("Model (Mode) (N)"); selecting a type folder edits all its fixtures
      together (feeds the same-type console). Remove + selection-preserve handle
      folders.
- [x] **Pivot to look editor from DMX console** — opening a palette
      (double-click in tree / new / duplicate) now shows the look editor even
      when the per-fixture console is up (showLookEditorPanel()).
- [x] **Palette copy/duplicate** — Cmd-C/Cmd-V when the palette tree is focused,
      and right-click "Duplicate" (createCopy → "… (copy)" → open for edit).
- [x] **Color editor W/A/UV as vertical sliders right of the picker** — were
      below the tall colour dialog and scrolled off-screen; White always shown,
      Amber/UV gated on fixture presence.
- [x] **Color palettes drive White/Amber/UV** — (1) engine auto-derives White
      additively (W=min(R,G,B), per fixture) when a colour palette has no
      explicit wauv, so legacy RGB-only colour palettes now light RGBW white;
      Amber/UV default 0. (2) Color look editor gained White/Amber/UV sliders
      (shown only when target fixtures have those emitters); White auto-fills
      from RGB on colour change, all three are manually overridable. Stored via
      QLCPalette wauv (colorToString rgb+wauv).
- [x] **Fix: console scooted right on every selection** — FixtureConsole::
      setFixture deleted old channel widgets but never removed the trailing
      spacer it re-adds each rebuild, so spacers piled up at the front and
      pushed channels right. Now clears all leftover layout items first.
- [x] **Greyed look-channels actually dim** — setEnabled(false) didn't grey
      (channel has its own stylesheet bg); added a 0.45 opacity effect on
      locked channels.
- [x] **Look-controlled channels: selected but greyed/read-only** — the prog
      console now disables (greys) channels an applied look drives
      (FixtureConsole::setChannelEnabled); baked values applied first, look
      values override (paramount), so the displayed value matches runtime.
- [x] **Targets label shows fixture count** — "Targets (N fixtures)" counting
      distinct fixtures across fixed + dynamic-group targets.
- [x] **Clearer "uncovered channels" prompt** — wording now states it's N
      channel values across M fixtures (was a confusing raw channel count).
- [x] **Fix: bottom editor jumped/"moved right" on target add/remove** —
      SceneGroupLooks::reload() now preserves the target selection (was
      cleared, hiding the wide console and shifting the splitter each time).
- [x] **Applied looks are now paramount over baked values** — (1) runtime:
      Scene::writeDMX processes baked values first, palettes last, so a look
      overrides leftover baked values on its channels; (2) apply-time: dropping
      a look onto a scene strips baked values the palette covers, then prompts
      to clear any remaining baked channels no applied look covers
      (SceneGroupLooks::reconcileAfterPaletteApply). Matches the layered-scene
      workflow (color look → only color; add Dimmer look → intensity).
- [x] **Double-click a look loads it** — double-clicking a look in the scene's
      Looks list (re)loads it into the look editor, switching away from the
      per-fixture channel editor (even if already selected).
- [x] **Fix: single vs multi console rendered differently** — channels now
      always built hidden + revealed via showChannels() (showEvent or explicit
      after a rebuild), so fixture→fixture switches render identically.
- [x] **Fix: prog console selected ALL channels** — checkable ConsoleChannels
      default to *checked*; the Programming console now unchecks all after
      setFixture so only the scene's actual baked values + applied-look channels
      are selected. Keeps static scenes narrow for layering (color/intensity/
      position/gobo as separate scenes on one fixture).
- [x] **Fix: status-bar unsaved indicator alignment** — matched the other
      permanent labels' AlignRight (they top-align); was AlignVCenter so it sat
      lower than "Autosave".
- [x] **Fix: blank DMX console on fixture→fixture switch** — FixtureConsole
      built channels hidden and only revealed them in showEvent(); switching
      directly between fixtures (console already visible) left them hidden.
      Now setVisible(isVisible()) at build time.
- [x] **Fix: multi-group "Move to folder" in prog-page source** — moved only
      the clicked group; now applies to every selected group.
- [x] **Fix: applied looks now show in the fixed-fixture DMX console** —
      console prefills resolved scene palettes (valuesFromFixtures/Groups) then
      baked values, matching Scene::writeDMX order (looks were palette refs, not
      baked into values, so the console missed them).
- [x] **Prog-page Fixtures&Groups drag-to-folder** — FixtureGroupSource now
      accepts dropping group(s) onto a folder / sibling group / root to re-file
      them (DragDrop + CopyAction, PathRole on folders); setPath → Doc signal →
      reload. Dragging fixtures/groups out to the canvas still works.
- [x] **Status-bar unsaved indicator** — App status bar now shows
      "● Unsaved changes" / "✓ Saved" driven by Doc::modified (app.cpp,
      m_statusDirtyLabel).
- [x] **New palette folder (Programming tab)** — palette tree right-click →
      "New folder…"; moves any selected palettes in (so it persists), else
      creates an empty folder node to drag into. FunctionsTreeWidget gained
      paletteFolderPathFor() + ensurePaletteFolder().
- [x] **Multi-fixture DMX edit by type (Programming canvas)** — selecting
      several fixed fixtures of the SAME type edits them together; mixed types
      now show a note instead of mirroring by channel index
      (ProgrammingManager::sameFixtureType).
- [x] **Palette quick wins (Programming tab)** — (1) drag palettes onto a
      folder / sibling palette / root to re-file them (external mode now
      DragDrop; FunctionsTreeWidget handles PALETTE-mime drops →
      paletteDroppedToFolder → setPath + updateTree); (2) gobo/shutter look
      editor now shows capability image thumbnails in the picker + a live
      preview swatch (loads QLCCapability Picture-preset resource paths).
- [x] **Natural FG folder manipulation** — (1) "Move to folder…" now moves
      every selected group, not just one; (2) drag group(s) onto a folder/group/
      root to re-folder them (new group-drag MIME + tree drop handling, signal
      groupsDroppedOnFolder → setPath); (3) universes nested under a single
      top-level "Universes" folder (Fixture Manager only; gated on ShowGroups).
      fixtureItem/groupItem lookups made recursive for the deeper tree.
- [x] **Group-editor move: beachball fix + undo + marquee + drop wireframe** —
      (1) bulk edits now block the group's signals and emit one changed() at the
      end (was firing a tree-rebuild storm per head → beachball); (2) Ctrl+Z
      undo stack of full layouts (size+heads+sub-group tags); (3) index-precise
      marquee selection (replaced Qt's pixel rubber-band that over-grabbed a
      row); (4) QRubberBand wireframe + valid/invalid cursor while dragging a
      block. Engine: FixtureGroup::restoreState/notifyChanged/headSubGroupMap.
- [x] **Faster group-editor render for large groups** — debounce the
      sectionResized burst (Stretch emits one per column) into a single font
      relayout; skip per-cell font-fitting past 600 cells; freeze repaints
      during table rebuild. Was ~O(rows×cols²) per relayout.
- [x] **Multi-cell drag in the group editor grid** — ExtendedSelection
      (rubber-band / Shift / Ctrl); press-drag any selection (1 or many cells)
      to a new position. Unified block-move via moveHeads() (replaced Qt's
      InternalMove/swap); refuses if it would collide with non-selected heads.
- [x] **Copy whole fixtures / whole fixture groups into a new or existing
      group** — Fixture Manager tree right-click: "Copy to new group…" and
      "Copy into group ▸ <group>". Groups paste as sub-group blocks (layout
      preserved, tagged); loose fixtures append in a row; grid auto-grows.
- [x] **Move a whole fixture as a unit** in the group editor grid (right-click
      a cell → "Move <fixture> up/down/left/right"; shares moveHeads() with the
      sub-group mover).
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
