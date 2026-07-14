# TODO — Programmer / Programming-tab work

Tracking future work for the fork's programming/looks workflow. Done items
move to the bottom or get deleted. See also the session memory under
`~/.claude/.../memory/palette_tree_integration.md`.

---

## In progress / next
*(pick from Backlog)*

- [ ] **Power/amperage estimate + power-distribution model (NEW, needs GUI test)** —
      Design-mode-only estimate of the previewed look's electrical load, shown in a
      Programming-tab **footer** (`Estimated load: X.X A | Y.YY kW | OVERLOAD`), plus a
      full power-distribution model. Engine: `engine/src/powerdistribution.{h,cpp}` —
      `PowerSource`(name, voltage) → `PowerCircuit`(name, ratedAmps, deratePercent,
      fixtures) → fixtures; owned by `Doc` (lazy `powerDistribution()`, mirrors
      `MonitorProperties`), persisted in the `.qxw` as `<PowerDistribution>`.
      `PowerEstimator` namespace estimates per-fixture watts = rated
      (`physical().powerConsumption()`) × intensityFraction (dimmer × colour); movers
      add a fixed always-on base (`MoverBaseWatts=60`); rated==0 falls back to
      `#intensityChannels × FallbackWattsPerChannel(40)`. Amps = Σwatts/voltage.
      `recomputePower()` reads `Universe::preGMValues()` (designed peak, ignores
      GM/blackout) via `claimUniverses/releaseUniverses(false)` on a 500 ms timer that
      is **started only in Design mode + tab visible** (gated in
      showEvent/hideEvent/slotModeChanged) so Run mode pays nothing; footer hidden in
      Operate. Circuit-assignment dialog: `ui/src/powerdistributiondialog.{h,cpp}`
      (Circuits… button) — editable sources/circuits tree (rows red when over derated
      limit), fixture list with assignment + watts, and greedy **auto-assign**
      bin-packing into circuits up to `deratedLimit()`. Default voltage/derate/breaker
      from QSettings (`power/defaultVoltage`=120, `deratePercent`=80,
      `defaultBreakerAmps`=20). Circuits carry **per-circuit voltage** (0=inherit source,
      for mixed 120/208 distros) and a derived **connector hint** (Edison/L6-20…).
      **UPS/battery sources**: set a VA rating to make a source a UPS — carries a
      datasheet runtime point (min @ W), estimates runtime = (min×W)/loadW, flags
      overload when load VA (=W/PF, `power/powerFactor`=0.9) exceeds the VA rating
      (footer + source row). Edited in the dialog's "Source power (UPS / battery)" panel.
      XML round-trip (incl. circuit voltage + UPS fields) + runtime scaling unit-verified;
      live compute path runs without hang. Still needs: visual confirmation of the
      footer/dialog and tuning of the Mover/fallback watt constants against real fixtures.

- [ ] **2D Monitor: fixture facing arrow + set-facing (DRAFT, needs GUI test)** —
      moving heads now draw a green facing arrow showing `FixtureRigProps.panZeroDir`
      (0=downstage/screen-down, 90=SR, 180=US, 270=SL; matches the aim solver's
      azimuth). Set it by **Alt-drag** to rotate (snaps 15°, Shift=fine) or
      **right-click → Facing (pan-zero)** quick-presets. Persists to
      `panZeroDir`, shares the one source of truth with the test dialog's
      Pan-zero spinbox (dialog edits refresh the arrow live via
      `fixtureItemForId`). The pan/tilt movement arcs now also rotate with the
      facing (Qt_zero = 270° + panZeroDir), so the head's depicted home/centre
      pointing swings with the base — matching the aim solver, which already
      folds panZeroDir into the world→pan computation. Files:
      monitorfixtureitem.{h,cpp}, monitor.cpp.
      The facing arrow + Alt-drag + facing/truss context menu are **editing
      affordances**, shown only while the plot is unlocked. The existing layout
      lock toggle was renamed to the **Edit Plot ⇄ Plot Locked** paradigm
      (padlock label + tooltip via `updatePlotLockAppearance`): "Edit Plot" =
      arrange & aim (arrows on); "Plot Locked" = rig frozen for the show (arrows
      off, fixtures not movable, aim targets still adjustable). Pan/tilt arcs
      stay visible in both modes.
      Fixture Properties dialog: "Position & Orientation" block moved above "Rig
      Assignment" and given a live **facing preview** (FacingPreviewWidget) —
      body + green arrow + pan-range wedge over DS/US/SR/SL labels, updating in
      lock-step with the pan-zero spinbox and icon Rotation so you can confirm
      orientation before saving without looking at the canvas.
      Follow-ups: true side-mount (boom) kinematics is still Tier-2
      (yaw+pitch+roll base orientation, pan/tilt swap roles); reverse live-sync
      (Alt-drag → open dialog spinbox) not wired.

---

## Backlog  *(roughly priority order)*

### Low complexity — quick wins
- [ ] **Decide Cmd-C/Cmd-V in Functions tab** — stock QLC has its own
      duplicate there; now that Programming tab has it, remove one or
      leave both?

### Medium complexity — clear value
- [ ] **Highlight mode (identify)** — flash a selected fixture/group to
      full-bright white (bypassing programming) so you can identify it in
      the rig. Standard on EOS / MA2. One ProgrammerController call +
      a toolbar button; clears on deselect. *(relatd to park)*
- [ ] **Park / unpark DMX** — hold a channel or fixture at a fixed DMX
      value regardless of cue output. ProgrammerController maintains a
      "parked" overlay; unpark removes it. Useful for stuck fixtures or
      setting a fixture aside during programming.
- [ ] **Blind vs live preview toggle** — Programming tab currently always
      previews live in Design mode. Add a Blind button: edits don't hit
      DMX until you "take" them. Normal EOS workflow.
- [ ] **Scene capture from live DMX** — "Snapshot" action reads current
      universe output and writes it into a new or existing scene as baked
      values. Essential for capturing looks built on an external console.
      MasterTimer or GenericFader has the live values.
- [ ] **Sequence editing in the Programming canvas** — Sequences
      (chasers with per-step scenes) currently show a "edit in Functions
      tab" hint. Inline step editing in the canvas would complete the workflow.
### Higher complexity — phased
- [ ] **Sub-master / intensity master per fixture group** — a per-group
      fader (0–100%) that scales all dimmer output for that group, without
      editing the scene. Overlays on top of palette paramount. Useful for
      live balancing without touching cues.
- [ ] **Look transition times** — each look applied to a scene can carry a
      fade-in / fade-out time, used when the scene is triggered. Scene
      stores the fade alongside the palette ref; runtime applies via
      FadeChannel. *Pairs with blind/live toggle.*

---

## Ideas / bigger bets  *(shower thoughts; decide before building)*

- [ ] **Command line / keypad bar (EOS-style)** — a command prompt at the
      bottom of the window to perform operations by typing (select fixtures,
      apply palettes/intensity, record, etc.), like an ETC EOS command line.
      Would parse a small grammar and drive ProgrammerController / functions.
      *High complexity; transformative for power users.*
- [ ] **Scriptable effect-plugin engine** *(the big one)*. A JS effect like
      RGBScript but intelligent: operates on a static list or a dynamic
      FixtureGroup, handed a fixture-set descriptor
      `[{id, head, pos{x,y,z}, rot, type, hasPanTilt, panRange°, tiltRange°,
      hasRGB,…}]` built from MonitorProperties + Fixture.
      **DECIDED — data boundary = high-level INTENTS, not raw DMX**: scripts
      return per-fixture `{pan:°, tilt:°, dimmer:0-1, color/paletteRef,
      gobo:idx}`; host converts via `Fixture::positionToValues` and
      `QLCPalette::valuesFromFixtures`; palettes passed IN each tick (named
      slots) so live palette edits re-colour the effect. Phasing: P0 pan/tilt
      script bound to a group → P1 palette+color/dimmer/gobo intents →
      P2 live inputs (follow-spot) → P3 sharing/bundling.
      See [[scriptable_effect_engine_design]].
- [ ] **Shareable effect "looks"/groupings** — portable bundles of effect
      groupings (rolling sweep, colour patterns) that users can share.
      *Depends on scriptable effect engine.*
- [ ] **Timecode / cue sheet** — trigger cues from MIDI timecode or OSC
      timestamps, for locked-to-music shows.
- [ ] **More movement shapes / draw-your-own** — beyond built-in EFX shapes,
      let users draw custom movement paths on an XY canvas.
- [ ] **Dimmer curve per fixture / channel** — override the default linear
      dimmer response with square, cubic, etc. curves, per fixture or group.
      Stored in Scene or FixtureGroup; applied at writeDMX time.
- [ ] **MIDI-mapped look recall** — bind a palette/look to a MIDI note or CC
      so a controller can fire looks without Virtual Console. Would reuse the
      InputProfileEditor + ProgrammerController.

---

## Done
- [x] **Persist across restarts (RGB Matrix + Effect looks)** — one flag, one
      meaning, on both script paradigms: *stopping and restarting does not reset
      the script*. For an **RGBMatrix** the identity is the matrix function, so
      `postRun()` remembers the step index and `preRun()` restores it after
      `initializeDirection()` has reset it (the `RGBScript` object itself is never
      recreated — `m_runAlgorithm == m_algorithm` — so any per-pixel buffer the
      script keeps in its module scope survives already). For an **Effect look**
      the identity is the palette, so `EffectScriptRunner` *parks* the whole
      `EffectInstance` (QJSEngine and all) when the last scene using it stops, and
      re-adopts it in the next scene that carries the same palette. Purpose: long,
      slowly-evolving effects (a trickle down the front cloth) survive a chaser
      step from one collection to the next instead of snapping back to frame 0.
      The effect ends naturally when it is no longer part of the running
      collection. Editing a look (`syncScene`) discards the parked engine — a
      change to the effect must not resume the pre-edit script; a copy of a matrix
      copies the flag but not the resume point. Both persist to XML.
- [x] **Followspot: Design-mode inert + lastPosition/snapToTarget handoff** — (1)
      the JS effect engine no longer ticks in Design mode (`EffectScriptRunner::
      slotTick` returns early when `Doc::Design`), so followspot does NOT move and
      does NOT accumulate any setpoint while building scenes — only output was
      suppressed before, the script still advanced state. (2) `followspot.js` gains
      a `followMode` dropdown: `lastPosition` (default) resumes from the beam's
      last spot so heads don't jump on scene transitions; `snapToTarget` re-aims to
      the bound `followTarget` each time. Last position is persisted host-side:
      `EffectInstance` reports per-fixture pan/tilt degrees (`lastIntentDegrees()`),
      `EffectScriptRunner` accumulates them in `m_lastSpotDeg` (survives instance
      teardown) and re-injects via `setLastSpotPositions()` → exposed to scripts as
      `fixture.lastSpot`. Falls back to target, then movement-seeded centre, when no
      position is recorded yet. The `followMode` dropdown appears in the **Look
      Editor** Parameters section (per-effect-palette), not the Followspot panel.
      (3) Follow-spot **pin** is now Operate-only: `ProgrammingManager::
      slotFollowSpotPinChanged` gates `visible` on `Doc::Operate` (single choke
      point), `slotModeChanged` hides it on →Design and re-seeds via
      `seedStageAimFromScene` on →Operate. The draggable **StageTarget** marker
      stays visible in Design as the aim handle. (4) **Shortest-path pan** on
      target transitions: `followspot.js` `nearestPan()` picks the pan
      representation (pan±360 within range) closest to the beam's last position
      when seeding `snapToTarget`, so a >360° head takes the short way instead of
      flipping/sweeping the long way. (5) Effect scripts now copy into the build
      tree at BUILD time (`add_custom_command` + `effectscripts_build` target in
      `resources/effectscripts/CMakeLists.txt`) so script edits propagate with a
      plain `cmake --build` — no reconfigure. (6) Removed [AIM2]/[FSOUT]
      fx9 debug logging. (7) **JS effect owns the followspot** (decided over the
      C++ Aim path): root cause of "snap to target on stick move" + "can't move
      after guard" was that the EffectScriptRunner's fader used `Universe::Auto`,
      same priority as the scene's Aim/PanTilt palette fader — so the effect
      winning LTP depended on fragile insertion order and the scene's pan/tilt
      could override it. Fixed by requesting the effect fader at
      `Universe::Override` (effectscriptrunner.cpp) so it reliably wins LTP
      (pan/tilt); HTP intensity unchanged (max is order-independent). With the
      effect now actually steering, `applyDesignJoystick` yields (returns early)
      when the focused scene has a `followspot` Effect palette — this also stops
      the C++ path from dragging the Aim stage target around / `resetRuntime()`
      churn. Net: in lastPosition the beam holds across transitions and moves
      from where it sits; per-fixture seeding handles mixed-fixture scenes (same
      fixture holds, new fixture seeds from its bound target).
- [x] **Split Pan/Tilt channels: indexed-position band** — fixtures whose
      Pan/Tilt channel maps only a sub-range to absolute positioning (rest =
      continuous rotation, e.g. Junman 2 Head: DMX 0-127 index, 128-255 spin)
      now convert correctly. `Fixture::positionToValues` honours a
      `RotationIndexed` capability on the MSB channel and scales the physical
      degree range across that band only, clamping so an out-of-range angle can
      never spill into the rotation zone. No-band fixtures are byte-identical to
      stock. Added `Junman/Junman-2-Head.qxf` (+ FixturesMap entry) and a
      `Fixture_Test::indexedPosition` unit test. Continuous spin would be a
      separate "rate" intent writing raw values into 128-255 — not built yet.
- [x] **2D Monitor: stage-feature copy/paste** — Ctrl+C / Ctrl+V (toolbar +
      "Copy/Paste features here" context menu) duplicate selected trusses,
      platforms and targets. Keyboard paste cascades 0.5 m per repeat; "Paste
      here" anchors the group to the cursor. Target copies get their own linked
      PanTilt palette (mirrors Add Target). Ctrl+Z undoes a paste (removes the
      copies + any palettes), interleaved correctly with move-undo.
- [x] **2D Monitor: platform polish** — targets stay movable when layout is
      locked; platform placement snaps to grid on drop; platform W/D/H accept
      feet-and-inches (5' 6") or decimal feet; platform name label bigger/bold
      and centered.
- [x] **2D Monitor: grid/drag/snap/lock + zoom/pan/undo/center** —
      multi-select + rubber-band, grid subdivisions, layout LOCK (magic-sheet
      style selection surface, persisted), Shift+wheel zoom (cursor-anchored),
      Shift-drag pan (sceneRect matched to viewport for scroll range), Cmd/Ctrl+Z
      undo of moves, teal centre H/V axes. SNAP fix: snaps the whole move as a
      group on drop (was snapping each fixture independently and scrambling the
      layout). Files: monitor/monitorgraphicsview.*, monitorfixtureitem.*,
      monitor.*, engine/monitorproperties.*.
- [x] **Beam palette** — new `QLCPalette::Beam` type: Focus/Frost/Iris sliders
      (0-255 raw DMX each). Engine matches BeamFocusNearFar/FarNear presets,
      Beam-group channels named "frost", and ShutterIrisMinToMax/MaxToMin
      presets. Look editor has a dedicated 3-slider page. XML save/load as
      "focus,frost,iris". "New Beam" in the palette creation menu.
- [x] **Position presets on the PanTilt look editor page** — Home / Front /
      Down / Left / Right quick-dial buttons fill the XY pad to named degree
      positions (no new palette type; PanTilt already stores degrees and
      uses positionToValues()).
- [x] **Tree search/filter in Programming tab** — QLineEdit with clear button
      above both the scene/function tree and the palette tree; filterByText()
      on FunctionsTreeWidget shows/hides items (case-insensitive, auto-expands
      matching folders).
- [x] **Removed dead `ui/src/groupselection.{h,cpp}`** — backed the removed
      "Select groups…" dialog; nothing referenced it.
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
      ellipses still read as circles-with-gaps.
- [x] **2D Monitor: thin fixtures now visible** — flat/step fixtures rendered a
      few px tall; clamp each fixture to a 14px minimum in updateFixture; heads
      lay out and light up. True scale preserved above the minimum.
- [x] **2D Monitor colour rendering at zoom** — head/body outlines made
      cosmetic (width 0) so they stay 1px at any zoom instead of growing and
      washing out the colour fill. Colour-wheel heads now derive their colour
      from the capability NAME (e.g. "Blue") when the definition carries no
      explicit resource colour. Shift+wheel zoom direction fixed for macOS
      (delta arrives on the X axis when Shift is held).
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
      default to checked; the Programming console now unchecks all after
      setFixture so only the scene's actual baked values + applied-look channels
      are selected. Keeps static scenes narrow for layering (color/intensity/
      position/gobo as separate scenes on one fixture).
- [x] **Fix: status-bar unsaved indicator alignment** — matched the other
      permanent labels' AlignRight; was AlignVCenter so it sat lower than
      "Autosave".
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
