# DONE — shipped programmer / programming-tab work

Completed work for the fork's programming/looks workflow, newest first. Each
entry keeps its per-feature detail + any deferred/eyeball notes. Active and
not-yet-built work lives in [TODO.md](TODO.md); move an entry here when it ships.

---

### 2026-07-27 — Show length: Logic-style end handle + park-at-end *(BUILT — needs hardware/eyeball)*
Fixes "MTC past show end → cursor scrolls off and disappears." A show now has an
EXPLICIT length (Logic-style end-of-project marker) that the cursor parks at.
- [x] **P1 engine** — `Show::configuredDuration` (0=auto), `totalDuration()` =
      configured?:content, `contentDuration()`; XML `<TimeDivision Duration=>`;
      `ShowRunner` m_endTime = effective end → non-follow finish + **clamp**
      (cues authored at/after the end are never started). Unit-tested (Show 11/11).
- [x] **P2 UI** — draggable end handle in `MultiTrackView` (blue=auto/rides
      content, red=configured; grip+label; drag=grid-snap; right-click Fit/Set
      length; past-end shading + content-past-end warning; scene headroom). Wired
      in BOTH hosts via `showLengthChangeRequested` → `setConfiguredDuration`.
- [x] **P3 cursor** — `moveCursor()` clamps display to the end (parks, no runaway);
      footer MTC chip "✓ complete +Xs past end".
- [x] **Eyeball/hardware**: drag feel + snapping ✓ (2026-07-27); live park under real MTC past the
      end ✓ (2026-07-28); clamp (content past handle doesn't fire) during a timecode run ✓.
- [ ] **Deferred**: "End at SMPTE hh:mm:ss" convenience (needs offset — host menu);
      bar/beat snapping when a BPM is set; cursor tint when past-end.

### 2026-07-27 — Dev tooling: automated GUI verification *(BUILT)*
So Claude can self-check UI work instead of relying solely on hand screenshots.
- [x] **Offscreen snapshot harness** — `App::captureScenarioIfRequested()` (main.cpp
      calls it post-load). Gated by `QLC_SHOT_DIR`; optional `QLC_SHOT_TAB`
      (substring), `QLC_SHOT_FUNC` (default first Show), `QLC_SHOT_CALIBRATE=1`,
      `QLC_SHOT_STAY=1`. Navigates, expands nav trees, `QWidget::grab()` → PNG
      (works under `QT_QPA_PLATFORM=offscreen`), then quits. `[SHOT] <path>` to
      stderr. Added `ProgrammingManager::showFunction(fid)` public nav entry.
- [x] **QTest driver test** — `ui/test/functionstreewidget` (5/5): typeSortRank
      table + role-order vs A–Z category ordering. Guards the type-order feature.
- Deferred: headful `screencapture`/video for pixel/motion bugs (intrusive; stays
      Branson's domain), and QTest input-driven tests for the calibrate dialog.

### FUTURE — Programming tab: Show→tracks→functions subtree + type ordering *(Branson shower-thought; not started)*
- Expand a **Show** in the Programming function tree into its **tracks**, each track
  into the functions assigned to it (mirror the Show Manager timeline hierarchy).
- Option to order top-level function types **hierarchically** (Show · Chaser ·
  Collection · RGBMatrix · EFX · Scene) instead of alphanumeric.
- Remove the "Remove NN" hack from the function-tree right-click menu.
- Tree lives in `FunctionTreeWidget`; ordering = sort-key change, subtree =
  tree-build change.

### FUTURE — Timecode calibration / sync-health tool (Branson shower-thought)
The per-show MTC `timecodeOffset` is manual today. Latency splits in two:
- **Internal (measurable):** packet→process→DMX (GUI hop + tick), plus RATE
  (×real-time), JITTER (inter-packet / phase-lock drift). QLC can timestamp and
  report all of this per configuration and AUTO-FILL the internal portion of the
  offset. Read-only measurement → low risk.
- **External (NOT self-measurable):** RTP/USB transport delay + fixture response —
  outside the software, no shared reference/feedback path. Needs calibration
  against the real show.
Build: (1) live **sync-health** readout — rate, jitter, internal latency,
healthy/unstable verdict; **[x] SLICE 1 BUILT** (rate/jitter/interval + verdict
in the MTC chip's bind menu; measured in `TimecodeSource`). Still open: internal
packet→DMX latency figure (needs plugin-side timestamping). (2) **assisted offset
calibration** — tap on the beat a few times against the music, average →
suggested offset; **[x] BUILT** (`TimecodeCalibrationDialog`, opened from the MTC
chip menu "Calibrate offset…"; tap vs. a chosen reference — timeline 0 or a
section marker — averages taps, shows mean/spread + live feed health, Apply
writes `Show::setTimecodeOffset`, ±10 ms trim). (3) auto-fill the internal-latency
portion. Bottom
line: the fixed offset can't be
fully auto-computed (physics is external), but everything internal is measurable
and the rest can be assisted+repeatable instead of blind. Related:
[[perf_load_indicator_idea]] (MasterTimer tick timing is the internal-latency source).

### Timecode: audio click-track AUTO-calibration (Branson idea, 2026-07-27) *(#1 + #2-partial BUILT)*
- [x] **#1 auto-tap** — `TimecodeCalibrationDialog` "Listen (auto-tap)" mode:
      drives the beat generator from the audio input (`setBeatGeneratorType(Audio)`,
      restored on stop/close), samples the offset on each `InputOutputMap::beat()`
      via the grid model (Click-BPM field + coarse-offset disambiguation), pooled
      with the manual taps. Compile + layout verified (screenshot); AUDIO PATH
      needs a hardware pass (offscreen can't feed a click).
- [x] **#2 detection-latency correction (measured floor)** — editable "Detection
      latency (ms)" seeded from the LIVE capture block via new
      `InputOutputMap::audioInputBlockMs()` (= `AudioCapture::captureSize/sampleRate`,
      ×1.5), subtracted from each auto-tap.
- [ ] **#2 active loopback self-test** — DEFERRED: play a click on the audio output,
      time round-trip to detection = true audio latency. Needs an audio-emit path
      (AudioRenderer wants a decoder — rabbit hole) and is UNVERIFIABLE offscreen;
      the editable/seeded latency + ±10 ms nudge cover it meanwhile.
Original design notes below:

### FUTURE(design) — Timecode: audio click-track AUTO-calibration (Branson idea, 2026-07-27)
Replace the human finger in the assisted tap calibration with the **machine ear**:
detect a click track on the AUDIO INPUT and auto-capture taps against the timeline.
Infra already exists and is wired: `AudioCapture::beatDetected()`
(engine/audio/src/audiocapture.h) fires per spectral-flux onset from
`BeatTracker::processAudio()`, already connected in `InputOutputMap`
(inputoutputmap.cpp ~1111). So the work is a UI "Listen (auto-tap)" mode on
`TimecodeCalibrationDialog` that, on each `beatDetected()`, does what the spacebar
does (capture `tc->positionMs()`, offset vs the expected beat) — hands-free, at
every beat → hundreds of samples per pass.
Two catches that ARE the design:
- **Which beat? (phase).** A manual tap hits ONE known reference; auto-detect fires
  on every beat, so an onset pins the offset only MODULO one beat period (±½ beat).
  → auto-tap is a REFINEMENT after a coarse offset (one manual tap / the hour-snap)
  resolves which beat; then averaging hundreds of onsets nails it tight + low-jitter.
- **Audio-in latency is a NEW systematic term.** Onset is delayed by driver/OS input
  latency + capture buffering (frameSize 2048 @ 44.1k ≈ 46 ms/block) + flux group
  delay ≈ 50–100 ms, but CONSISTENT. Auto-tap trades the human's ~100 ms variable
  reaction for the machine's ~50–100 ms consistent latency → jitter collapses, but
  the systematic offset is RELOCATED not removed. Characterize it once per rig via
  a **loopback** (QLC plays a click on its internal beat generator, hears it back,
  delta = round-trip audio latency — this is TODO slice 3, "auto-fill internal
  latency"), or fold the residual into the ±10 ms nudge.
Verdict: build as an OPT-IN precision mode ON TOP of the manual tap (baseline stays
the always-works path; audio mode is the power feature when a clean click bus is
patched to QLC's input). Continuous with the assisted-calibration + sync-health
work already built above. Effort ~1 wk for the honest version (auto-tap mode +
loopback latency characterization); ~2–3 days for just the beatDetected→auto-sample
slice against a coarse offset.

### 2026-07-25 — Fixture Studio: 2D-map groups as editable studio objects *(Phases 1–4 BUILT — needs eyeball; see FIXTURESTUDIO_DESIGN.md)*
Foundational implementation landed across 5 commits (388958ab7 → ea9cad209).
Engine core is unit-tested (monitorproperties_test studioFrameDerivation +
studioFrameXmlRoundTrip, 16/16 pass); builds clean; app loads workspace with no
regression. **Still needs interactive eyeball** — the UI flows below were built
and compile but have not been exercised in the running app.
- [x] **P1 engine** — `MonitorGroup` frame (hasFrame/origin/rotation), members'
      `FixtureRigProps::groupLocal`, derived world pos via a new branch in
      `fixtureRigPosition()` (the truss funnel), XML round-trip. **Tested.**
- [x] **P1 UI** — `createStudioGroupFromSelection()` (origin=centroid, adopt
      locals); member top-view drag edits `groupLocal`; `StudioGroupEditor`
      (name/origin/rotation/per-member local X/Y/Z/re-center, live, snapshot-
      revert); right-click Create/Edit. *(eyeball)*
- [x] **P2** — bind a studio group to a FixtureGroup; **Seed from cells**
      ((cell−centre)·pitch) / **Adopt current positions** (world→local);
      `boundFxGroup`+pitch persisted; cell matrix left untouched. *(eyeball)*
- [x] **P3** — `ProgrammerController::rebuildCompositeGroup()` re-pulls sources
      from per-head provenance tags (already written by `copySelectionIntoGroup`),
      re-blocks horizontally ordered by studio X; Fixture Manager “Rebuild
      composite from members”. *(eyeball)*
- [x] **P4** — component templates: `StudioTemplate::saveGroup()`/`stamp()` (JSON,
      role/order arrangement); “Save as Template…” in the editor, “Stamp Studio
      Template…” on a map selection. *(eyeball)*
- [x] **Graphical 3-plane (Top/Front/Side) canvas editor** — BUILT and now
      eyeballed. `StudioPlaneView` (ui/src/monitor/studioplaneview.*) draws the
      group's members as draggable LED bars in Top/Front/Side orthographic
      projections of the local frame; grid, pan (drag empty), wheel-zoom, per-face
      Distribute/Put-on-face, and the anchor feature (platform footprint/face, or
      truss line) drawn as reference geometry transformed into the frame. Embedded
      in `StudioGroupEditor` (view combo + selection strip stay in sync). Verified
      2026-07-29 via the offscreen harness (`QLC_SHOT_STUDIO=<id|name>`): all three
      planes render correctly for both a platform-anchored group (US-1) and a
      truss-anchored group (T-2) — no crash, reference geometry projects right.
- [x] **Browsable component library** (2026-07-29) — components are now saved to a
      shared library folder (`~/Library/Application Support/QLC+/StudioComponents/`,
      via `QLCFile::userDirectory` off USERQLCPLUSDIR) and browsed in a new
      `StudioComponentBrowser` dialog (ui/src/monitor/studiocomponentbrowser.*):
      list + plan-view thumbnail + details, Stamp-onto-selection (double-click too),
      Rename, Delete, Import a one-off .json. `StudioTemplate` grew library helpers
      (`libraryPath/library/info/saveToLibrary/removeFile/renameInLibrary` + an
      `Info` struct with per-role locals for the preview). Entry points: 2D-map
      fixture right-click "Stamp Studio Component…", empty-canvas "Browse Studio
      Components…", and `StudioGroupEditor` "Save as Component…" (name → library,
      replacing the old raw file-dialog "Save as Template…"). Verified via the
      offscreen harness (`QLC_SHOT_COMPONENTS=1`, seeds→grabs→cleans up).
- [x] **Add fixtures inside the Studio editor** (2026-07-29) — `StudioGroupEditor`
      now has an "Add fixtures…" button (left column) that pops a multi-select
      picker of every patched map fixture not already in the group; chosen ones are
      adopted in at their current position. Previously the only ways in were
      Create-from-Selection (2D map) or a FixtureGroup Seed/Adopt/Put-on-face.
      Revert-safe (reuses rememberFixture snapshot).
- [x] **Double-click a group → opens the Studio** (2026-07-29) — in the Layers
      panel, double-clicking a group node now opens the Fixture Studio editor
      (mirrors the right-click "Edit…", promoting a plain group in place); rename
      moved fully to the right-click "Rename group…" / F2. (Layer nodes still rename
      on double-click.) Was: double-click only started an inline rename, so there
      was no double-click path to the studio at all.
- [ ] **Deferred by decision — live composition.** Snapshot chosen; provenance +
      the single `rebuildCompositeGroup` entry point leave the door open. Enable
      by re-running it on `FixtureGroup::changed(id)` when wanted.
- [ ] **Eyeball checklist**: select 2+ fixtures → Create Studio Group → drag a
      member (moves within frame) → Edit dialog: change origin/rotation (whole
      unit moves/rotates), edit a local, Re-center; Bind a FixtureGroup → Seed
      then Adopt; Save as Template → Stamp onto another selection; build a
      composite FG then Rebuild-from-members; save/reload round-trips.

### 2026-07-25 — Fixture Studio: DESIGN *(superseded by the BUILT entry above)*
Promote `MonitorGroup` to a first-class, editable **studio object**: a rigid body
with a local frame (origin+rotation), members in group-local coords, world
position **derived** (the truss/`fixtureRigPosition` pattern). Click a group →
**3-plane (Top/Front/Side) editor**. FixtureGroup cell layouts **inform** (seed or
adopt, never lock) the physical layout; cell space stays the script's coordinate
system untouched. Decisions locked (2026-07-25 exploration): **(A) bind &
reference**, not merge; **snapshot** FG-into-FG composition **with source-FG
provenance** so live is a later flip; studio geometry orders composed cells.
Motivating case: 8″×8″×8′ steps anchored on platforms, front/top FGs informed by
the layout, composed into "all step fronts" / "all step tops".
- [ ] **Phase 1** — studio-group core: `MonitorGroup` origin+rotation (model+XML+
      full ABI rebuild); group-local coords + derived world pos; rigid move/rotate;
      3-plane editor (reuse `monitorgraphicsview` + `riserfaceeditor`). *(~1.5–2.5 wk)*
- [ ] **Phase 2** — FG binding: link + seed(cell×pitch)/adopt(current pos);
      decoupled cell space; cell-index overlay. *(~2–4 days)*
- [ ] **Phase 3** — functional composition: block-inclusion + **source-FG
      provenance**; "Rebuild from members"; order composed cells by studio geometry.
      *(~3–5 days)*
- [ ] **Phase 4** — template/instance component library: template file (slots/
      roles/layout/anchor); stamp flow; library browser. *(~1.5–2 wk)*
- [ ] **Later** — live composition: rebuild on `FixtureGroup::changed(id)`. *(~1–2 days)*

### 2026-07-21 — 2D Monitor: bar-on-truss REBUILT on a truss-local model *(BUILT — needs eyeball)*
Reworked start-to-finish (the parentDrop/direction model kept surprising us).
A bar is still a child truss, but its world geometry is DERIVED from five
truss-LOCAL params, so placement reads the way you rig:
- [x] **Model** — `Truss`: `parentOffset` (**Along**), `barFace`
      (Bottom/Top/Downstage/Upstage/StageRight/StageLeft), `barStandoff`,
      `barRun` (Along/Across/Drop). XML `BarFace/BarStandoff/BarRun` + migration
      of old `ParentDrop`. Unit-tested `childBarFollowsParent` (face/run/standoff
      derivation + follow + XML).
- [x] **Derivation** — `recomputeChildTrusses` computes origin/direction/type:
      `base = parent.positionAt(Along) + faceVector(Face)·(halfThick+Standoff)`;
      Along→parallel, Across→boom toward the face, Drop→Vertical hanging; centred
      on the attach point. "Across the front facing downstage" = Face=Downstage,
      Run=Along.
- [x] **Editor** — bar rows: Bar-on-truss · Along · Face · Stand-off · Run;
      raw Type/Origin/Direction hidden for bars (derived). **Live preview** on the
      canvas as controls change; **Cancel reverts** the mount + removes bars added
      via the strip.
- [x] **Strip** — extent is run-based (Along bar on a flat truss / Drop bar on a
      tower = a segment; everything else = a point marker at its Along position).
- [ ] **Eyeball**: Add Bar → set Face=Downstage → boom sits on the downstage face;
      try Across / Drop; slide Along on the strip; move the parent → follows;
      Cancel reverts; save/reload round-trips.

### 2026-07-21 — 2D Monitor: horizontal bar on a truss (child-truss attach) *(superseded by the truss-local rebuild above)*
A "bar" is modelled as a child truss hung on a parent truss, reusing the whole
fixture-on-truss stack (binding, rendering, grouping, delete-cleanup).
- [x] **Engine** — `Truss` gains `parentTrussId` / `parentOffset` / `parentDrop`
      (+ `isChildBar`); XML `ParentTruss/ParentOffset/ParentDrop`.
      `MonitorProperties::recomputeChildTrusses()` derives each bar's origin =
      `parent.positionAt(parentOffset)` dropped in Z, iterated a few passes
      (bar-on-bar) — called at end of `loadXML` and after any truss move/edit.
      `removeTruss` detaches child bars (they become free trusses in place).
      Unit-tested `childBarFollowsParent` (derive + move-follow + XML).
- [x] **UI** — Truss editor: "Bar on truss" parent combo + "Position along
      parent" + "Drop below attach"; on OK the origin is derived and the bar
      joins the parent's group. `MonitorGraphicsView::followParentTrusses()`
      re-derives + redraws bars + repositions their fixtures; called after edit
      and (deferred via singleShot, to survive the drop) when a parent truss is
      dragged.
- [x] **Horizontal OR vertical bar run** — a child bar of type Horizontal is a
      cross-bar/drop bar (runs in XY); type Vertical is a HANGING DROP that
      extends DOWNWARD (`Truss::positionAt` + elevation render special-case
      `isChildBar()`; free towers still extend up).
- [x] **Place attach point on the Fixture-Placement strip** — the parent truss
      editor's strip now shows child bars as amber draggable markers (offset =
      the bar's parentOffset); dragging sets where the bar attaches. On the SIDE
      elevation strip for vertical parent trusses. `TrussStripWidget::Slot`
      gained `barTrussId`; the OK handler writes bar slots back to parentOffset +
      recomputes.
- [ ] **Eyeball**: attach a bar to a horizontal truss (drop below) and to a
      vertical tower (boom); make a vertical hanging-drop bar; hang fixtures on
      the bar; drag the bar along the parent's placement strip; move the parent →
      bar + fixtures follow; delete the parent → bar detaches; save/reload.

### 2026-07-21 — 2D Monitor: background image → placeable Image objects *(BUILT — needs eyeball)*
Promoted the single global background image to first-class **Image objects** that
live in layers/groups, with a per-image plane so each shows in the right view.
- [x] **Engine** — `MonitorProperties::MonitorImage` registry (id/name/source/
      plane/originX,Y,Z/width/height/layerId/groupId/locked) + `addImage`/
      `setImage`/`removeImage`/`nextImageId`. XML `<MonitorImage>` round-trip
      (unit-tested `imagesXmlRoundTrip`). Migration: an old global
      `commonBackgroundImage` is converted to a Floor image spanning the grid on
      load, then cleared.
- [x] **Plane → view** — Floor shows in 2D Top (lies flat), FrontBackdrop in
      Front elevation (X×Z at a Z height), SideBackdrop in Side (Y×Z). Only the
      matching plane renders per POV (`updateImages`).
- [x] **Item** — `MonitorImageItem` (ui/src/monitor) draws the pixmap scaled to
      its rect; movable in-plane, selectable, lock tint; drop persists the new
      origin (`slotImageMoved`, plane-aware inverse). z=0.5 (behind features,
      above grid).
- [x] **Integration** — images join the heterogeneous helpers (group/layer/
      select/edit/itemFor), `refreshItemLayerState` (same lock-selectability
      rule), Layers-tree leaves (kind "image", :/image.png icon, inline rename),
      right-click Edit/Delete/Lock, double-click editor (name/plane/source/size/
      X/Y/Z).
- [x] **UI** — Add ▸ **Add Image** (file picker → editor). Removed the toolbar
      "Set background image" button (colour button kept).
- [ ] **Eyeball**: add a floor image (Top), a front backdrop (switch to Front),
      move/resize/lock, group it, reload + save survive; confirm an old
      workspace's background migrates to a floor image.

### 2026-07-20 — 2D Monitor: explicit truss binding + fixture/truss context menus *(BUILT — needs eyeball)*
Reworked truss↔fixture interaction so a truss no longer "grabs" overlapping
fixtures, and made per-fixture operations reachable.
- [x] **No more 2D drop-to-bind** — dropping an unbound fixture on a truss no
      longer auto-binds it (removed the auto-grab in `slotFixtureMoved` + the
      drop-target hover highlight). Already-bound fixtures still snap along the
      truss and detach on escape (drag far off).
- [x] **Explicit attach** — fixture right-click → **Attach to Truss ▶** (lists
      trusses, checks the current one) OR drag the fixture onto a truss-anchored
      group node in the Layers tree. Both route to
      `MonitorGraphicsView::attachFixtureToTruss` (snaps to nearest point, joins
      the truss group).
- [x] **Detach** — fixture right-click → **Remove from Truss**
      (`detachFixtureFromTruss`).
- [x] **Double-click a truss-bound (or grouped) fixture** drills in (isolated
      cyan highlight) so it can be slid along the truss or dragged clear to
      detach. (Removed the old vertical-truss→open-editor shortcut.)
- [x] **Truss right-click** → **Edit Truss…** / **Delete Truss** (new
      `trussRemoveRequested` signal → `Monitor::slotTrussRemoveRequested`,
      confirm + `removeTruss`).
- [x] **Centre on truss** — a bound fixture's icon centres on the truss line
      (not its top-left), across display / drag-snap / attach / editor
      (`MonitorGraphicsView::halfIcon`).
- [x] **2nd double-click opens the editor** (drill-in → act); right-click
      fixture → **Edit Fixture… / Remove from View**; **Remove from Truss**
      button in the editor's Rig Assignment.
- [x] **Lock Position** on the truss & platform right-click menus (restored the
      per-item lock the Edit/Delete menu had shadowed; OR-folds with layer lock).
- [x] **Truss side** (under-hung / top-mounted / centred) — `FixtureRigProps::
      trussMountSide` offsets the fixture's Z by ±half the truss thickness in
      elevation views. XML `TrussSide` (written when ≠ under-hung).
- [x] **Deck mount** — `FixtureRigProps::deckPlatformId` + `deckHeightOffset`: a
      fixture stands ON a platform, keeping its free XY, Z = platform top +
      offset (follows the platform). Editor "On platform (deck)" combo + height;
      mutually exclusive with truss. XML `Deck`/`DeckH`; `removePlatform`
      un-decks. `refreshRiserFixtures` repositions deck fixtures too.
- [ ] **Eyeball**: no auto-bind on drop; attach via menu + tree; slide/detach;
      right-click edit/delete/lock; centred-on-truss; truss-side Z in Front/Side;
      deck-mount a light on a platform → sits at deck height.

### 2026-07-20 — 2D Monitor: rulers + footer bar + settable 0,0 origin *(BUILT — needs eyeball)*
Consolidate grid/measure controls into a **footer bar** in the 2D view and add
fixed, toggleable **rulers** with a live coordinate readout and a user-settable
0,0 origin.
- [x] **Engine** — `MonitorProperties` stage origin (`stageOrigin()` QPointF in
      metres) + `OriginX/OriginY` XML round-trip (written only when non-zero),
      reset in `reset()`.
- [x] **View** — `MonitorGraphicsView` ruler/readout API: `rulerTicks(horizontal)`
      (viewport-pixel ticks, POV-aware Top X/Y, Front X/Z, Side Y/Z, origin-
      offset), `viewportToReadout`, `axisName`, `unitSuffix`, `beginPickOrigin`
      (click-to-place), `setStageOriginMetres`. `rulersChanged`/`cursorReadout`/
      `originPicked` signals emitted on zoom/pan/resize/POV/metrics/move; mouse
      tracking on; scrollbar pan wired.
- [x] **UI** — `MonitorRuler` widget (top + left, dumb painter querying the view);
      graphics view wrapped in a 2×2 grid (corner + rulers + canvas), `NoFrame`
      so ticks line up with the viewport. Footer bar (`initGraphicsFooter`) holds
      the moved **Size / Units / Subdiv / Snap** controls plus a **Rulers** toggle
      (persisted), a **0,0** menu (click-to-place + Stage-centre / Downstage-centre
      / Top-left presets), and a right-aligned live X/Y readout. Ruler cursor
      markers via a viewport event filter.
- [ ] **Eyeball**: open Monitor → 2D view; confirm rulers frame the canvas and
      track zoom/pan, footer controls still drive the grid, live readout follows
      the cursor, click-to-place + presets move 0,0, and origin survives a save.

### 2026-07-20 — 2D Monitor: organizational Layers (OmniGraffle-style) *(BUILT — needs eyeball in Monitor window)*
Layer approach for the 2D map: create layers, assign items to them, and
hide/lock a whole layer at once ("generate and lock as we go"). New items land
on the **active** layer.
- [x] **Engine** — `MonitorProperties::MonitorLayer` registry (add/remove/
      rename/reorder, visible/locked, active-layer; permanent Default id 0;
      unknown ids resolve to Default). Per-item `layerId` on fixtures, trusses,
      platforms, targets, power sources — all XML round-tripped, written only
      when non-default. Unit-tested (`layers`, `layersXmlRoundTrip`).
- [x] **View** — `MonitorGraphicsView::refreshItemLayerState()` folds the three
      lock sources (global layout lock + per-item lock + layer lock) and layer
      visibility onto every item; called from all rebuild paths + the global
      lock toggle. `setSelectedItemsLayer` / `reassignLayerItems` helpers.
- [x] **UI** — `MonitorLayersPanel` docked as the 2nd splitter pane; eye/lock/
      rename/reorder rows, active = selected row, "Move selection here".
      Toolbar toggle (hideable) with visibility persisted.
- [ ] **Eyeball**: open Monitor → 2D view → toggle **Layers**; add a layer,
      hide/lock it, confirm items on it hide/freeze and reload survives a save.

### 2026-07-20 — 2D Monitor: group / ungroup + Layers TREE *(BUILT — needs eyeball)*
Second half of the ask, then extended: groups are **named folders under their
layer** in a full object tree, and **group-of-groups** (nesting) is supported.
**Select-together, spatial-only.**
- [x] **Engine** — per-item `groupId` (0=ungrouped) on all 5 item types +
      `MonitorGroup` registry {id,name,layerId,parentGroupId} in
      MonitorProperties (createGroup/ensureGroup/removeGroup/childGroups/
      nextGroupId + `<MonitorGroup>` XML). Unit-tested (`groupRegistry`,
      `groupsXmlRoundTrip` incl. nesting).
- [x] **View** — nesting-aware: `topLevelGroup`, `groupIsUnder`,
      `itemsUnderGroup` (recursive), select-together selects the whole outermost
      subtree. `groupSelectedItems` combines ≥2 units (loose items + existing
      top-level groups nest under a new parent), adopts the selection's DOMINANT
      layer. `ungroupById`/`ungroupSelectedItems` dissolve one level (promote
      children to parent). `ensureGroupRegistry` migrates anonymous first-cut
      groups on load. Non-fixture move slots persist the whole selection.
- [x] **UI** — Layers panel is now a **QTreeWidget**: Layer (eye/lock/rename) →
      nested group folders → item leaves (fixtures/trusses/…). Click group =
      select its items on map; double-click = rename; right-click = rename/
      ungroup. Toolbar Group/Ungroup + Cmd/Ctrl+G / +Shift+G still there.
      `mapStructureChanged` rebuilds the tree after canvas grouping.
- [x] **Panel v2** (2026-07-20): left-docked, min-width 200, non-collapsible,
      default-visible (key bump). 2-column tree (name | eye/lock). Inline rename
      of layer/group via **Enter / F2 / double-click**. **Multi-select**
      (ExtendedSelection) → right-click **Group selected** / **Move to layer →**.
      Right-click **blank → New layer**. **Operate mode disables all editing**
      (buttons/edit/eye/lock; `Doc::modeChanged`). In-panel **× hide** button
      (unchecks the toolbar toggle).
- [ ] **Eyeball**: rubber-band 2+ fixtures → Ctrl+G → new folder appears under
      their layer; group two groups → nested folder; rename with Enter; multi-
      select leaves → right-click Group/Move; save+reload holds.
- [x] **Drag-to-reparent** (2026-07-20): drag item leaves / group folders onto a
      layer or group in the tree to move/nest them (viewport drop intercepted,
      cycle-guarded). View: `reparentToLayer/reparentToGroup/reparentGroupTo
      Layer/reparentGroupToGroup` + `itemFor(kind,id)`.
- [x] **Read-only elevation views** (2026-07-20 — NEEDS EYEBALL): toolbar
      **View: Top / Front / Side**. `MonitorGraphicsView::ViewPOV` + `projectMm`
      (Top=(X,Y); Front=(X,Z↑); Side=(Y,Z↑) off `floorPixelY()`). Every render
      path branches only for non-Top so Top stays byte-identical: fixtures placed
      by 3-D `fixtureRigPosition`; trusses drawn as projected end-to-end lines
      (`TrussItem::setElevationMode` makes a vertical tower a segment, not the
      top-down bullseye); platforms as front/side box faces; targets/power
      projected. Elevation is read-only (`isElevation()` forces non-movable);
      plot-lock + snap disabled. POV not persisted (always opens in Top).
- [ ] **Eyeball elevation**: switch to Front — vertical trusses stand up with
      fixtures at their heights; check platforms/targets/power placement. Likely
      tuning: grid vertical extent uses `gridSize.height()` cells as ceiling;
      aim-lines/FS-pin still computed top-view (may look off in elevation);
      end-on trusses render as short stubs; h=0 platforms draw thin.
- [ ] **Z-precedence in Top** (still open): now that `projectMm` exists, deriving
      top-view `setZValue` from height is the natural follow-up.

### 2026-07-20 — Truss auto-group + move-with-truss; fixture mounting *(NEEDS EYEBALL)*
- [x] **#1a move-with-truss**: `slotTrussMoved` now repositions rigged fixtures
      from `Truss::positionAt(offset)` (moveFixtureTo + emit fixtureMoved to
      persist). Dragging a truss carries its fixtures.
- [x] **#1b truss auto-group**: `ensureTrussGroup(trussId)` creates a group named
      for the truss (on its layer) and pulls in still-ungrouped bound fixtures;
      called from `ensureGroupRegistry` (load migration) and on drop-onto-truss
      in slotFixtureMoved. KNOWN: ungrouping a truss then reloading re-groups it.
- [x] **#2 fixture mounting**: right-click a fixture → **Set mounting height…**
      (Z, metres) and **Heads: stack vertically (riser strip)**. Z now persists
      in the widgets build (was `#ifdef QMLUI`-gated); vertical flag =
      `MonitorProperties::VerticalStripFlag` (XML `VertStrip`), swaps the box
      aspect in `updateFixture` so heads lay out in a column. Elevation places
      the fixture at its mounting height. Unit-tested (Z + flag round-trip).
- [x] **Layer "disappears" on move — FIXED**: `reload()` preserved scroll pos;
      `setCurrentItem(active)` was scrolling the target layer out of view.
- [x] **Fix (drag fixtures onto truss didn't group)**: already-grouped fixtures
      weren't pulled into the truss group. Now newly-bound fixtures are FORCED
      into the truss's group (tracked via `newlyBoundFx` in slotFixtureMoved),
      overriding any prior group.
- [x] **Targets excluded from the tree/grouping**: they're dynamic aim points
      that move across levels during a show — omitted from `gatherItems`.
- [x] **Fix (layer reorder made it disappear)**: `reorderLayer` renumbers ALL
      layer orders 0..n-1 (no ties) + `selectLayerNode` scrolls the moved layer
      back into view (setCurrentItem(active) had scrolled it off).
- [ ] **Eyeball**: drag ungrouped/grouped fixtures onto a truss → they join the
      truss folder; reorder layers → stays visible; Front view riser strips.
- [ ] **#3 native beams** — deferred at Branson's request ("not yet").
- [ ] **Known limits**: MIXED-type group drag persists only the grabbed type +
      all fixtures; no group bbox outline on canvas; tree re-expands on reload
      (no collapse memory); no drag-drop reparent yet.

### 2026-07-20 — Control Map: static MIDI mapping (no VC widget) *(Phase 0 BUILT — needs hardware test)*
Bind hardware MIDI controls straight to actions with **no Virtual Console
widget** taking up screen space. Full design + phased plan in `CONTROLMAP.md`.
- [x] **Phase 0 — vertical slice** *(in `ProgrammerController`, in-memory, not
      persisted; validates the hardware loop before locking the schema):*
      `learnFunctionTrigger`/`clearFunctionTrigger`/`hasFunctionTrigger`/
      `functionTriggerLabel`; learn captures the next press in
      `slotControllerInputChanged`; a press toggles `Function::start/stop`;
      LED follows `running`/`stopped` via `InputOutputMap::sendFeedBack`.
      UI: right-click a function in the Programming-tab function tree →
      **Learn / Clear MIDI Trigger**. *Needs a physical wing to confirm
      press→toggle→LED end to end.*
- [ ] **Phase 1** — `engine/src/controlmap.{h,cpp}` model owned by `Doc`,
      `<ControlMap>` persistence, dedicated dispatcher + `FunctionParent`.
- [ ] **Phase 2** — Control Map grid dialog (rows of `InputSelectionWidget`
      + target picker + mode), reached off a menu — not a VC widget.
- [ ] **Phase 3** — fader/intensity + submaster targets.
- [ ] **Phase 4** — look/palette (via ProgrammerController) + global actions
      (blackout / GM / blind / park / chaser next-prev / timeline transport).
      *This is the old "MIDI-mapped look recall" backlog item.*
- [ ] **Phase 5** — feedback polish (resend-on-connect, Note-Off semantics).
- [ ] **Phase 6** — banking/pages *(decision-gated; reserve `page` in Phase 1)*.

### 2026-07-20 — Smart name increment + fix Enter-to-rename *(BUILT — TESTED 2026-07-28)*
- [x] **Fix in-place fixture rename** — Return/Enter/F2 on a selected fixture now
      opens the inline editor. Bug: `QTreeWidgetItem::setFlags()` emits
      `itemChanged()` synchronously, so arming `ItemIsEditable` re-entered
      `slotItemChanged`, disarmed the row, and made `editItem()` a no-op. Fixed by
      `blockSignals()` around the arming (`ui/src/fixturetreewidget.cpp`).
- [x] **Reusable unique-name helper** — `Doc::nextUniqueName(name, isTaken)`
      (engine/src/doc.cpp): keeps `name` if free, else bumps the rightmost digit run
      preserving zero-pad (`US #3`→`US #4`, `0.9.1-Startup`→`0.9.2-Startup`), falls
      back to `" 2"`. `nextDuplicateName()` now delegates to it.
- [x] **Fixture multi-add** continues past existing names and never collides
      (accumulating used-name set; `ui/src/fixturemanager.cpp`). Per decision:
      single typed names kept verbatim; only multi-add + duplicate auto-number.
- [x] **Add Fixture dialog** prepopulates the Name field with a non-colliding
      default (`nextUniqueName` vs existing fixtures) and shows a live orange
      "name already exists" warning under the field for a single add (hidden for
      multi-add, which auto-numbers). `ui/src/addfixture.{ui,h,cpp}`.
- [x] **Test infra**: `ui/test/CMakeLists.txt` now defines `QLC_TEST_RESOURCE_ROOT`
      (mirrors engine/test) so fixture-dependent UI tests run out-of-tree instead of
      aborting at `loadMap()`. addfixture_test now 9/0.
- [x] **Multi-add numbered base**: typing a name that already ends in a number
      ("US #4") now INCREMENTS it (US #4, US #5, …) instead of bolting on a second
      counter ("US #4 #1"). Unnumbered names keep the "#NN" suffix. Collision alert
      now also fires for multi-add (informational: "numbered past it").

### 2026-07-20 — 2D Monitor: per-item lock for stage features *(BUILT — needs GUI test)*
Right-click → Lock/Unlock, mirroring the existing Truss lock. Locked items can't be
dragged and draw with a red border. Persisted as a `Locked="true"` XML attribute.
- [x] **StagePlatform** — `locked()`/`setLocked()` + XML; `PlatformItem` context menu,
      movability gate, red border tint.
- [x] **StageTarget** — same, `TargetItem` (red crosshair when locked).
- [x] **PowerSource** — `PowerSource::locked` + XML; `PowerSourceItem` emits
      `lockToggleRequested`, `MonitorGraphicsView::slotPowerSourceLockToggled`
      toggles the model + rebuilds (item has no Doc/model pointer, so it routes
      through the view like `itemDropped`).
- [ ] Fixtures themselves are NOT per-item lockable yet (only the global layout
      lock). Add if wanted, mirroring this pattern on MonitorFixtureItem.

### 2026-07-19 — Effect engine: sharing, authoring, EFX-parity scripts *(BUILT — needs GUI test)*
Clarification: the scriptable effect engine was already built (EffectScriptRunner /
EffectInstance / 30+ scripts / intent boundary / palette-fed looks / drawn-path
canvas). These fill the remaining gaps additively — EFX and core untouched.
- [x] **EFX-parity scripts** — `lissajous.js` (arbitrary X/Y freq + phase = full
      stock-EFX Lissajous; Circle/Fig-8 are special cases) and `diamond.js`
      (rotated-square path). Registered in `resources/effectscripts/CMakeLists.txt`.
      Answers "absorb old EFX?": no migration — scripts already cover EFX patterns
      and go beyond (colour/dimmer/gobo/live-input); these add the two shapes that
      had no script. Keep EFX (core Function type) as-is.
- [x] **Export / Import effects** — Look-Editor effect page buttons. Export writes a
      portable `.qxfx` (JSON: preset name/script/category/description + pinned
      params + **embedded script source**). Import installs the script into the user
      dir if missing (then rescans) + saves the preset — so a recipient without the
      script can still use it. (`LookEditor::slotExportEffect/slotImportEffect`.)
- [x] **In-app + external authoring** — "New script…" (template → user dir → open)
      + "Edit script…" (current Generator's .js). Opens in either the new in-app
      `EffectScriptEditor` (mono QPlainTextEdit + Save-rescans + Reload) or the OS
      default editor, per a new **App Settings → Effects → "Effect script editor"**
      preference (`QSettings effectscript/editor`). Both wired.
- Scripts node-eval verified (lissajous/diamond produce sane pan/tilt). Full build
      clean. See `testing_st.md` §J.

### 2026-07-19 — Per-track flags: intensity / solo-safe / hold-last / priority *(BUILT — TESTED 2026-07-28)*
Four new per-track controls on the Show timeline (model + XML + UI + runtime).
- [x] **Intensity submaster** — a draggable **fader bar** on each track header
      (amber below 100%) scales that track's dimmer output live. `Track::m_intensity`
      (0..1, XML `Intensity=`); `Show::setTrackIntensity(id, v)` sets it + calls
      `ShowRunner::adjustIntensity` live; the runner also inits `m_intensityMap`
      from `track->intensity()` at start. Persists.
- [x] **Solo-safe / mute-exempt** — track flag (context menu): solo + "any-solo
      silences others" never touch it (house/work light/safety). `Track::m_soloSafe`.
- [x] **Solo made model-backed + non-destructive** — `Track::m_isSolo` (was UI-only,
      implemented by baking mute into other tracks). `ShowRunner` queue-build now
      skips a track if muted OR (any solo & not soloed & not solo-safe). Solos are
      additive. `slotTrackSoloFlagChanged` no longer writes mute into siblings.
- [x] **Hold-last** — track flag: when the track's cue ends, its look is snapshotted
      into the last-look holder (append) instead of releasing to black; the track's
      next cue clears it (per-fixture yield). Captured in ShowRunner Phase-2 stop
      using the threaded `universes`; Operate + `lastLookEnabled` gated. New
      `LastLookEffect::addHold` (accumulate) + `Doc::captureLastLook(..., append)`.
- [x] **Priority / role** — track enum Background/Normal/Override (context menu →
      submenu). `ShowRunner` sets each started child's `setFadePriority`
      (Background=-1 / Auto=0 / Override) so a base-wash track yields and an accent
      track wins LTP. `Track::m_priority` (XML `Priority=`).
- UI: TrackItem context menu (solo-safe/hold-last/priority) + the intensity bar
      (drag). New signals `itemPropertiesChanged`/`itemIntensityChanged` →
      `MultiTrackView::trackIntensityChanged` → `Show::setTrackIntensity` in both
      hosts. Engine tests green: track 8, show 11, showrunner 6, lastlookeffect 6.
      Note: intensity is live. See `testing_st.md` §I.
- [x] **Mute/solo/solo-safe now LIVE mid-run** — the runner builds its queue from
      ALL tracks and enforces silence per frame (`enforceLiveMuteSolo`): it stops
      the children of tracks that just went silent and starts the active cues of
      tracks that just became audible — targeted (only changed tracks, no full
      re-seek, so other tracks don't blip). `startChild` factored + shared with the
      normal start phase; the start gate reads `m_lastSilent`. UI already writes
      `Track::setMute/setSolo`, so no extra plumbing. Priority still applies at
      child start (not re-applied live). showrunner 6/6.

### 2026-07-19 — Timeline: frozen track-header column *(BUILT — TESTED 2026-07-28)*
- [x] The **track-header column** (names/mute/solo/lock), the **zoom slider corner**
      and the **vertical divider** now stay pinned at the viewport's left edge when
      the timeline scrolls horizontally, instead of scrolling off. `MultiTrackView`:
      `scrollContentsBy` override + `pinLeftColumn()` sets each `TrackItem` (and the
      divider/slider proxy) x to the current scroll offset (`mapToScene(0,0).x()`)
      and floats them above clips (`TRACK_HEADER_Z=800`, below cursor/drag). Re-pinned
      on add-track / divider rebuild. `drawForeground` marker bands/lines now clamp
      to `leftX+TRACK_WIDTH` so they don't paint over the frozen column (the pinned
      "MARKERS" label already used `leftX`). Shared widget → both the embedded
      Programming-tab timeline and the full Show Manager tab benefit.

### 2026-07-19 — Suspend-hold + operator arming/visibility + canvas undo *(BUILT — needs test)*
- [x] **Suspend = HOLD, not release** — Exit Timeline Control (VC takeover) now
      captures the current look into the last-look holder BEFORE releasing the
      show's children, so handing to the VC no longer blacks out channels the VC
      isn't driving. New `Doc::captureLastLook(fid, universes)` (shared by
      `Show::postRun` + the suspend path); `ShowRunner::write` now takes universes
      (threaded from `Show::write`) so the suspend block can snapshot preGM. On
      resume the restarted cues reclaim their fixtures (per-fixture yield). Gated
      on `lastLookEnabled`.
- [x] **Auto-arm the linked manual cue list at a freeze** — when MTC stops mid-show
      in a section that links a cue list (the seam), `ShowManager` STARTS that
      chaser (edge-triggered on the freeze): its first cue goes live and it becomes
      the current chaser so a mapped GO / APC step drives it. Skips if already
      running. `VCShowControl` shows it **green "▶ ARMED — <name> · next GO: <cue>"**.
- [x] **Footer run-of-show readout** — the app's MTC chip now appends the current
      **section** + linked **manual cue** (armed/next) under the playhead, driven by
      the per-position `timeChanged` update — visible from any tab, not just the VC
      Show Control widget.
- [x] **General canvas Undo (Ctrl+Z)** — per-scene undo in the Programming canvas:
      detached `Scene` copies (`copyFrom`, full state incl. values/targets/looks/
      fades) snapshotted at load + before each edit (`slotCanvasModified` →
      `pushUndoSnapshot`, 25-deep). `slotUndo` restores the pre-edit state (keeps
      the current name so a tree rename isn't reverted), reloads the canvas +
      preview; falls back to the old bundle-stamp undo when there's no scene-edit
      history. Cleared when leaving to a func/group editor.
- Engine touched-tests green: show 11, showrunner 6, lastlookeffect 6, track 8,
      showfunction 5. See `testing_st.md` §G.

### 2026-07-19 — Batch: audit quick-wins *(GUI tested 2026-07-28: Snapshot guard ✓, Circuits dialog ✓, palette Delete ✓, group rename ✓, group delete ✓)*
- [x] **Palette Delete** — Programming-tab palette-tree right-click → "Delete"
      (single/multi). Confirms, counts scenes that reference each palette (warns
      the looks will lose them), then `Doc::deletePalette` (which already detaches
      from scenes). Closes the CRUD hole.
- [x] **Fixture group rename + delete in the panel** — `FixtureGroupSource`
      right-click on group(s): **Rename group…** (single) + **Delete group(s)**
      (confirms; fixtures kept). No more round-trip to Fixture Manager for these.
- [x] **Live-DMX snapshot → scene** — Programming-tab **Snapshot** toolbar button
      bakes the current pre-GM output of every outputting fixture into the open
      scene as static `SceneValue`s (`checkHTP=false`, exact values). Reads under
      `claimUniverses`, collects, releases, THEN confirms + applies (no dialog
      under the lock). Captures a look built on an external console. Canvas
      reloads to show the baked values.
- [x] **Freeze-threshold reconcile** — `TIMECODE_WATCHDOG_MS` 600 → **200** so the
      footer "holding" chip flips ~when the rig actually freezes (ShowRunner's
      `m_msSinceFresh > 150`), not up to ~450 ms later. Safe: the watchdog's
      running=false only drives the chip + the Play "followingLive" check; show
      auto-start is on the rising edge (immediate) and the freeze is independent.
      Cross-ref comments added in both files.
- [x] **Stale test fixes** — `ShowFunction_Test`/`Track_Test` expected the old grey
      SceneType default colour; updated to the fork's blue `(70,110,150)`. Both
      green now (engine touched-tests: track 8/8, showfunction 5/5, show 11/11,
      lastlookeffect 6/6).

### 2026-07-19 — Finish embedded show timeline + close timecode↔manual seam *(GUI-tested 2026-07-28)*
GUI results (CGEvent + screencapture): Play/Stop ✓; section-marker add/rename/recolour/delete/link ✓
(full CRUD menu confirmed); "+" Add Track → immediate row ✓; right-click empty slot →
"Add function here..." ✓; track rename via double-click ✓. Per-clip delete not exercised
(requires a clip; the Delete-key handler is built — not a gap).
Two audit gaps closed together.
- [x] **Embedded timeline is now a full editor** (`ShowTimelineEditor`): transport
      **Play/Pause + Stop** toolbar (owns the show's playback — no more auto-play
      on open; `ProgrammingManager::loadFunctionEditor` skips `startPreview` for
      ShowType and the editor stops the show on teardown); **per-clip delete**
      (Delete key + toolbar, multi-select confirm); **track rename** (double-click),
      **reorder** (drag/menu → `moveTrack`), **colour**; full **section-marker CRUD**
      (add/rename/recolour/delete/relabel/move) wired from `MultiTrackView`;
      right-click empty slot → **Add here** (FunctionSelection). Playhead cursor +
      play icon track the show's running state.
- [x] **Marker → manual cue list link (the seam)** — `ShowMarker` gains
      `cueListId` (engine/src/show.{h,cpp}); `setMarker` PRESERVES it across
      relabel/recolour, `setMarkerCueList`/`markerCueList` set/read it, move carries
      it, XML round-trips as `CueList=` attr (back-compat). New MultiTrackView
      marker menu **"Link manual cue list…"** → `markerSetCueListRequested` →
      handler (Chaser-only FunctionSelection) in BOTH `ShowTimelineEditor` and
      `ShowManager`. Linked markers draw a **⏵ glyph** in the timeline lane.
- [x] **Runtime surfacing** — `VCShowControl` now shows the current section's
      **linked manual cue list + its next GO** (amber `m_manualCueLabel`), so when
      the show freezes in a spoken/break section the operator sees which stack to
      GO and what's next — no more hunting. Driven by `updateSection` off the
      section marker under the playhead.
- Engine `Show_Test` 11/11 pass. (Pre-existing unrelated failures in
      `Track_Test`/`ShowFunction_Test` = stale default-colour asserts, not touched
      here.) Still compact vs Show Manager: no undo, no MTC source combo, no footer
      chips in the embed (those stay in the full tab). See `testing_st.md` §D/§E.

### 2026-07-19 — Last-look persistence (Change B, phase 1) *(BUILT — needs live test)*
A stopped/finished Show no longer blacks out the rig in Operate — it HOLDS its
final look until the next cue. Closes the long-standing "Change B (intensity
persistence)" gap for shows (was "not built — risky core-mixer work").
- [x] **`LastLookEffect` holder** (`engine/src/lastlookeffect.{h,cpp}`) — a
      DMXSource that re-asserts captured per-channel values every tick via
      `uni->write(forceLTP=true)` (survives `zeroIntensityChannels`). Same proven
      pattern as `ParkEffect`/`HighlightEffect` (which both hold intensity), so no
      HTP-mixer surgery. Runtime-only, not persisted.
- [x] **Capture** in `Show::postRun` (single choke point: any stop reason, still
      has universes, runs before the runner releases children). Operate-only +
      `Doc::lastLookEnabled()`. Walks the show's fixtures (recursive
      `collectShowFixtures`: Scene/EFX/RGBMatrix = leaves→fixtures; Chaser/
      Collection/Show = containers→recurse), snapshots each channel's current
      `preGMValue`, installs the hold.
- [x] **Per-fixture (per-channel) yield** — a newly started function drops only the
      held channels on the fixtures IT drives; the rest keep holding
      (`Doc::slotClearLastLookOnStart` → `functionFixtures(fid)` →
      `LastLookEffect::releaseFixtures`). A full new state covering every held
      fixture clears it entirely (= the old whole-look replace). Also cleared on
      →Design (`setMode`) and `clearContents`. Fixture collector shared:
      `Doc::functionFixtures` (recurses containers; Scene/EFX/RGBMatrix = leaves).
- [x] **Owned by Doc** (`m_lastLook`, `lastLook()` accessor, `setLastLookEnabled`);
      default ON in Operate.
- [x] **Toolbar controls** (main toolbar, next to Follow-MTC/Suspend): **Hold Last
      Look** checkable toggle (`:/star.png`, `Doc::setLastLookEnabled`) + **Clear
      Held Last Look** momentary (`:/fileclose.png`, `lastLook()->clear()`).
- [x] **MIDI-mappable VCButton action** `ClearLastLook` ("Timeline: Clear held last
      look") — enum + string + actionToString/stringToAction + dispatch + a radio
      in vcbuttonproperties.ui + load/save. APC40 can drop a held look.
- [x] **Unit test** `LastLookEffect_Test` (hold asserts forceLTP + persists; clear
      releases; empty = inactive; per-fixture release yields only that fixture) —
      6/6 pass.
- Scope: SHOW stops only (not bare scene/chaser stops). Does NOT change the
      running-show VC-grab-persistence case (separate symmetric-LTP concern).
      Needs live confirm — see `testing_st.md` §B.

### 2026-07-19 — Programming tab: Shows editable + create-group from fixtures *(GUI tested 2026-07-28)*
Two asks: build Shows additively in the Programming canvas, and make fixture
groups from the lower-right Fixtures & Groups panel.
- [x] **Shows in the Programming canvas** — selecting a Show in the left tree now
      hosts an embedded timeline in the canvas instead of the "edit in Functions"
      placeholder. New `ui/src/showmanager/showtimelineeditor.{h,cpp}`
      (`ShowTimelineEditor`) wraps a `MultiTrackView` + the minimal show-side
      orchestration (populate from the show, create tracks, place dropped
      functions via a ported `addFunctionToTrack`, persist moves, delete track,
      playhead cursor on preview). Hosted in
      `ProgrammingManager::loadFunctionEditor` (`case ShowType`, `dragIn = true`);
      **"New Show"** added to the func-tree create menu. Drag-in needed **zero**
      new plumbing — the Programming func tree's external-drag payload is
      byte-identical to the timeline drop handler (both MIME
      `application/x-qlcplus-functions`, a `quint32` fid stream). A Show is a
      Function, so the tab's existing `startPreview` runs it live.
      Deliberately compact (phase 1): the full Show Manager tab keeps the shows
      combo, MTC source, footer chips, markers, undo stack and cue-level tools;
      this editor is for additive building. Follow-ups: share ONE orchestration
      core between this and ShowManager (currently `addFunctionToTrack` /
      populate loop are ported, not shared); undo; markers; whether a Show should
      auto-play on open (it does, matching collection/chaser preview).
- [x] **Compact timeline sizing** — the embedded timeline was rendering the
      fixed 6-row / 600px minimum (empty phantom tracks + a scrollbar). Added
      `MultiTrackView::setCompact(bool)`: in compact mode `updateViewSize` sizes
      the scene height to the actual track count (min 1) and `updateTracksDividers`
      draws one divider per existing track. `ShowTimelineEditor` turns it on; the
      full Show Manager tab keeps its roomy empty canvas.
- [x] **Double-click a group → visualize in the canvas** — double-clicking a
      group in the lower-right Fixtures & Groups source now hosts a
      `FixtureGroupEditor` (head-layout grid) in the central pane.
      `FixtureGroupSource::groupDoubleClicked(quint32)` →
      `ProgrammingManager::loadGroupEditor` (reuses the func-editor canvas slot;
      title shows group name + head count). It's the real editor, so the layout
      is editable there too, not just visualized.
- [x] **Drop-to-timeline landed offset to the right (FIXED)** — in external-drag
      mode the func tree used Qt's default full-row drag pixmap, whose hotspot is
      the grab point (mid-row), so blocks landed ~grab-offset px right of the
      ghost. Added `FunctionsTreeWidget::startDrag` override routing external
      drags through `startExternalDrag()` with a small icon ghost + explicit
      `setHotSpot(0,0)`, so the ghost tracks the cursor and the block lands under
      the pointer. Affects every external-drag consumer (Collection/Chaser/Show
      editors + the show timeline). Needs GUI confirm.
- [x] **Right-click → Create fixture group from fixtures** — the lower-right
      `FixtureGroupSource` panel's context menu now offers "Create fixture group
      from N fixtures…" on selected fixture rows (was group-nodes-only). Reuses
      the Fixture Manager path: `CreateFixtureGroup` dialog (name + auto-sized
      grid from head count) → `new FixtureGroup` → `addFixtureGroup` →
      `assignFixture` per fixture. New group appears in the tree ready to drag
      onto a scene. No engine changes.

### 2026-07-19 — Show timeline: crash fix + cue multi-select *(BUILT — needs GUI test)*
- [x] **Crash fix (thread safety)** — `Chaser::steps/stepsCount/stepAt` now lock
      `m_stepListMutex` (made recursive+mutable). Unlocked reads raced the
      timer-thread MTC show-drive and crashed `SequenceItem::paint`
      (EXC_BAD_ACCESS). `ShowRunner` uses a locked `steps()` copy, never
      `stepAt()` pointers. See memory `chaser_steps_thread_safety`.
- [x] **Cue multi-select** in `SequenceItem`: `m_selectedSteps` set; plain click =
      single, **Shift-click** = range from anchor, **Ctrl/Cmd-click** = toggle;
      all selected cues wash yellow (anchor brighter).
- [x] **Delete** — right-click → "Delete N cues" (confirmed; not undoable — chaser
      step timing isn't in the timeline undo stack).
- [x] **Scale** — right-click → "Scale cue timing…" → percent dialog
      (`scaleSelectedCues`).
- [x] **Move** — plain-drag a cue inside a contiguous **interior** multi-selection
      slides the group as a unit (`CueGroupMove`; neighbours a-1/b+1 absorb the
      slide, group timing preserved). Needs a give-cue each side.
- Note: cue editing needs the block **unlocked** (Full Show block is `Locked=1`)
      only for whole-block move/stretch; interior divider/slip edits work locked.
      Editability gated by `isShowLocked()` + track lock, not mode. Backlog: Delete
      key shortcut; rubber-band cue select; undo for cue edits.

### 2026-07-19 — MTC show = pure timecode cue drive *(BUILT — needs live-MTC test)*
A show is now purely timecode-driven — **every cue fires on the clock, none wait**
(the earlier "barrier / manual-GO inside the show" attempt was reverted). Spoken
cues belong in a SEPARATE hand cue list, not the show.
- [x] **External-clock chaser** — `ChaserRunner::setExternalClock` (via
      `Chaser::setShowClocked`); a show-clocked chaser never self-advances on its
      own MasterTimer time, only on an explicit SetStepIndex from ShowRunner.
      Cleared in `releaseChild` so standalone runs self-advance again.
- [x] **Position drive** — `ShowRunner::stepAtLocalMs` picks the cue whose
      cumulative on-timeline span contains `localMs = elapsed − blockStart`;
      `stepTimelineMs` = ChaserStep duration, else nominal `SHOW_NOMINAL_CUE_MS`
      (3000, matches SequenceItem) for a 0/∞ cue. Lockstep drives
      `if (target != cur)` — bidirectional, so a Logic locate/scrub jumps the cue.
- [x] **Freeze on MTC-stop** — chasers pause again with the other children so
      in-progress fades freeze; the frozen clock also issues no step change.
- [x] **Symmetric LTP** — removed the show's `Background` priority demotion; show
      cues write at Auto, last-writer-wins vs the VC (see LTP notes below).
- Diagnostics: `QLC_SHOW_DEBUG=1` → `/tmp/qlc_showdebug.log` logs `[GATE]`,
      `[START]`, and per-chaser `[RUNNER]` (cur/localMs/tgt/curFnRunning).
- Authoring: drag sub-cues in Show Manager to position each cue on the clock;
      un-positioned 0/∞ cues default to 3 s.
- **Still open — Change B (intensity persistence)**: a stopped show's last look
      still drops intensity to 0 (HTP zeroed each frame); "last cue persists until
      overwritten" needs core-mixer work. Not built.

### 2026-07-16 — Show Manager timeline usability batch *(BUILT — partial GUI test 2026-07-28)*
Punch-list from Branson while driving the timeline. All on `programmer-mode`.
- [x] **Empty-state hint + Show CRUD** — the timeline shows a centred hint when
      there's no show ("click New Show…") or an empty show ("drop a scene/chaser/
      collection, or right-click to add a track"). Rename show… / Delete show…
      toolbar actions next to the shows combo (delete confirms; keeps referenced
      functions). Show-scoped actions disabled when no show exists.
- [x] **Coarse whole-timeline Undo (Ctrl-Z)** — 25-deep snapshot stack of tracks
      (name/mute/colour/scene) + placed ShowFunctions + markers; restore rebuilds
      the timeline. Pushed before item move/resize/drop/delete, track add/delete/
      reorder, and every marker edit. Per-show (cleared on show switch).
      Limitation: does NOT capture a Chaser's internal step timing (that lives on
      the function — see chase-cue retiming below).
- [x] **Multi-select + safe multi-delete** — Shift/Ctrl-click + rubber-band
      marquee; Delete removes the selected item(s) (one straight away, >1 with a
      confirm) and is undoable. Delete no longer nukes a track by surprise when
      nothing is selected (tracks go via the header right-click, which confirms).
- [x] **Track header drag-to-reorder** — vertical drag on a track header reorders
      (multi-row honoured), in addition to the right-click Move up/down; undoable.
- [x] **Bare Scene = simple timed clip** — dropping a Scene no longer wraps it in
      a hidden 1-step Sequence; it's a SceneItem clip the ShowRunner runs for its
      duration (default 5 s). Existing Sequence-wrapped scenes still load.
- [x] **Chase cue retiming** — drag a chaser block's interior step dividers to set
      each cue's hold against the timeline (split cursor on hover; min 0.1 s;
      Common-mode chasers convert to PerStep on first drag). Outer edges still
      stretch the whole item.
- [x] **Fixture Manager: in-place rename** — Return / F2 on a fixture row edits its
      name inline (like folders); commit renames the fixture (2D monitor follows).
- [x] **Timeline beachball (FIXED)** — clicking a timeline item synchronously
      built a full SceneEditor console (every fixture + channel) AND a ChaserEditor
      on the right splitter; on a real rig that beach-balled on nearly every click /
      chase selection. Removed the open-editor-on-click behaviour entirely (the
      timeline is for arranging/timing; content is edited in Programming/Functions).
      Clicking now just selects + activates the track.
- [x] **Chase "one big block" / no sub-cues (FIXED)** — manual-GO steps (infinite
      hold) were clamped to 10,000,000 ms so one step filled the view. Added
      SequenceItem::stepDisplayMs() (nominal 3 s for infinite/0) used by paint +
      width + divider hit-test, so every cue shows as a distinct draggable
      sub-block.
- [ ] **Editor-to-the-right removed from the Show tab** — if arranging vs. content
      editing causes workflow friction with the Programming tab, revisit (Branson,
      2026-07-16). Right/bottom SceneEditor+ChaserEditor panels now stay hidden.

### Decided 2026-07-16 — show / VC / timeline coordination
Context: in Operate the Show timeline and the Virtual Console both drive the rig
and needed an arbitration + visualisation model. Decisions (Branson, 2026-07-16):
global Follow-MTC on the main toolbar; exit = suspend-and-keep-position; VC
overrides the timeline per-channel (timeline = low-priority base). Built in three
milestones on `programmer-mode` (commits 36ca4ee42, c96eb2411, dd44eacaa).

- [x] **Footer "under timeline control" + MIDI-mappable exit** *(BUILT — needs GUI test)*
      - `ShowRunner::setSuspended()` — resumable VC takeover: stops the running
        child functions (rig → VC) but the elapsed clock keeps tracking timecode,
        so resume = `seekTo(now)` and the right cues restart. Applied on the timer
        thread (request flag under the TC mutex); reset on stop.
        `Show::setTimelineSuspended/isTimelineSuspended` forwarders (runtime only).
      - `ShowManager` (singleton) global surface: `timelineControlActive()`,
        `timelineSuspended()`, `setTimelineSuspended()`, `toggleTimelineSuspended()`
        + `timelineControlChanged()` (emitted on start incl. MTC auto-start, stop,
        suspend, show-select).
      - App footer chip: "● UNDER TIMELINE CONTROL" (blue) / "❚❚ TIMELINE SUSPENDED
        — VC control" (amber), Operate-only. Main-toolbar **Exit/Resume Timeline
        Control** action (enabled only while a show runs in Operate).
      - MIDI-mappable **VCButton SuspendTimeline** action (LED = suspended);
        selectable in Button Properties, mapped via the button's input source.
- [x] **Global Follow-MTC toggle** *(BUILT — needs GUI test)* — moved out of the
      Show Manager toolbar onto the **main toolbar** (`App::m_followMtcAction`,
      :/clock.png); drives `ShowManager::setFollowTimecode()` on the current show.
      MTC **source** combo + timeline-**offset** button stay in the Show Manager
      (show config). MIDI-mappable **VCButton FollowTimecode** action (LED =
      following). Selecting a different show re-broadcasts its follow state.
- [x] **Operate: VC overrides timeline per-channel** *(BUILT — needs GUI test)* —
      `Universe::Background (-1)` priority below `Auto`; `Function::fadePriority()`
      (default Auto, restored in `postRun`); `Scene` requests its fader at that
      priority; `ShowRunner` lowers started functions to Background in Operate and
      it propagates through `ChaserRunner` (steps) + `Collection` (members). VC at
      Auto therefore wins LTP on touched channels. Intensity stays HTP (VC adds,
      can't dim below the base — full override = Exit Timeline Control).
      **Visualisation is inherent**: the timeline runs the same function objects
      the VC binds to by id, so a timeline-activated scene lights its VCButton
      (Monitoring/amber) and a timeline chase drives the bound VCCueList step.
      Limitation: EFX/RGBMatrix/Audio on the timeline are not yet prioritised
      (Scene is the dominant look case).

      **TESTING PLAN — show/VC/timeline coordination** *(Operate mode, a show on
      the timeline + a VC with buttons/cuelists bound to the SAME functions):*
      - [ ] **Footer chip** — enter Operate + run the show: footer shows blue "●
            UNDER TIMELINE CONTROL". Toolbar **Exit Timeline Control** enables.
      - [ ] **Suspend keeps position** — click Exit Timeline Control: rig hands to
            VC, chip goes amber "SUSPENDED — VC control", playhead keeps moving with
            MTC. Click Resume: the cue active at the CURRENT position restarts (no
            rewind).
      - [ ] **MIDI exit** — map an APC40 button to a VCButton set to *Suspend
            timeline*; its LED lights while suspended; press toggles takeover.
      - [ ] **Global Follow-MTC** — the toggle is on the main toolbar; arming it
            from any tab makes the current show chase Logic; a VCButton set to
            *Follow MIDI Time Code* mirrors + toggles it (LED = following).
      - [ ] **VC overrides per-channel** — timeline runs a wash on all LEDs; hit a
            VC button running a different colour on a subset → that subset follows
            VC, the rest stay on the timeline wash. Release → timeline reclaims them.
      - [ ] **Visualisation** — a scene the timeline activates shows its VC button
            in Monitoring (amber); a chase on the timeline moves the bound VCCueList's
            highlighted step in lock-step.

### Decided 2026-07-15 — show-timeline design session
Context: audience is students + show choir; the fork nails *building* looks but
the *run* side was still stock. Established that stock chaser + VC Cue List +
APC40 already give a GO cue stack, so the gaps are narrower than first thought.

- [x] **Per-look (per-parameter) fade times — IN + OUT** *(GUI tested 2026-07-28: all core UI checks ✓; bundle round-trip + chaser integration not yet exercised)*
      — separate fade-in and fade-out times stored **per look/palette applied to a
      scene**, not just per chaser step. Rule: the chaser step's fade is the
      cue-wide default; a look with its own explicit time **overrides it for that
      look's channels only** (per direction); "step" = fall back to the step fade.
      Lets colour snap in while movers glide, and a look punch in fast / release
      slow (a "pulse": 0 in, slow out).
      Engine: `Scene` gains a `paletteId → {fadeInMs, fadeOutMs}` override map
      (`m_paletteFade`, guarded by `m_bindingsMutex`) with `setPaletteFade(id,
      inMs, outMs)` (each <0 clears that direction; 0 = snap), `paletteFadeIn/Out(id)`
      and `paletteFades()`. `Scene::write()` passes each non-Effect palette's
      fade-IN override (or step `fadeIn`) to `processValue`. Fade-OUT:
      `handleFadersEnd()` resolves each palette carrying an explicit fade-out into
      the channels it drives and calls the new `GenericFader::setFadeOut(enable,
      default, perChannelMap)` (channelHash → ms) so those channels release over
      their own time while the rest use the scene fade-out; a 0 scene fade-out no
      longer dismisses faders instantly when a look has a positive out override.
      Effect palettes excluded (EffectScriptRunner owns their timing). Persisted
      as optional `FadeIn`/`FadeOut` attrs on the scene's `<Palette>` ref (legacy
      single `FadeTime` still read as fade-in); round-trip + copyFrom +
      removePalette/clear cleanup unit-tested (`Scene_Test::paletteFadeTime`).
      **Bundles carry it**: `BundleEntry` gains `fadeIn`/`fadeOut` (JSON
      round-trip); "Save as Bundle" captures the scene's per-look fades,
      stamping restores them, stamp-undo preserves them.
      UI: the Programming-tab **Looks list is now a 3-column tree (Look | In |
      Out)** — bottom Look editor spinners set a per-look time (minimum reads
      "step" = follow the step/scene fade); Effect looks show "—". Editing
      refreshes the live preview. Fixed 2026-07-28: after right-click reset,
      `slotLookSelectionChanged()` now called so bottom editor refreshes. Still
      needs: chaser integration test (pulse out survives 0 scene fade-out);
      bundle round-trip GUI test.

      **TESTING PLAN — per-look fade times** *(run `build/main/qlcplus -o
      surfacetesting.qxw`, Design mode, Programming tab; a scene with ≥2 looks on
      movers+LEDs, e.g. a Colour look and a Pan/Tilt or Aim look on the same
      fixtures):*
      - [x] **Columns read right** — Looks list shows `Look | Fade In | Fade Out`;
            new looks show `step / step`; header + row tooltips explain 0 vs step.
      - [x] **Set an in-fade** — bottom look-editor spinbox, set 2 s. Cell shows
            `2 s`. (Tree-cell delegate editor doesn't open reliably via CGEvent
            double-click; bottom spinbox works. CGEvent Up arrows and osascript
            keystroke both commit; typeStr/unicode CGEvent does not.)
      - [x] **Colour-snaps-while-movers-glide** — Colour look Fade In = 0 (snap),
            Pan/Tilt look Fade In = 3 s. Both visible in tree simultaneously.
            (The headline use-case.)
      - [x] **Pulse (fast in / slow out)** — Shutter look Fade In = 0, Fade Out =
            4 s; tree cell shows `0 s | 4 s`; bottom editor shows `0.00 s / 4.00 s`.
      - [x] **Reset to step** — right-click a look → context menu shows *Reset Fade
            In to step / Reset Fade Out to step / Reset both to step*; cell returns
            to `step`. Fixed: after reset, bottom look editor now refreshes
            (slotLookSelectionChanged() call added to the reset handler).
      - [x] **0 ≠ step** — XML: explicit `FadeIn="0"` attr for snap looks vs no
            attr (step) for un-set looks; tree shows `0 s` vs `step` respectively.
      - [ ] **Precedence intact** — reordering looks (drag / Up-Down) still works
            and doesn't disturb the fade cells; effect looks show `—` (inert).
      - [x] **Persistence** — set in/out on a couple looks, save → XML has
            `<Palette … FadeIn=… FadeOut=…/>`; Palette 2=2000, Palette 3=0/4000,
            Palette 0=3000 confirmed.
      - [ ] **Bundle round-trip** — *Save as Bundle* from a scene carrying per-look
            fades → open the JSON, confirm `fadeIn/fadeOut`; **Stamp** it onto
            another scene → fades restored; **Ctrl-Z** (undo stamp) → prior fades
            restored.
      - [x] **Look editor mirror** — bottom Look editor "Fade in/out" spinners
            show the selected look's fade; editing via spinbox updates tree cell.
            (Refresh-after-reset bug fixed in this session.)
- [x] **Blind / Park / Highlight batch** *(GUI tested 2026-07-28: Blind toggle ✓ blue banner+footer, Park ✓ toolbar visible, Highlight/Flash ✓ toolbar visible)* — classroom-safety
      trio, all as Programming-tab toolbar buttons.
      - **Blind** = build without hitting the stage. Engine adds a per-universe
        output-inhibit flag (`Universe::setInhibitOutput`): `dumpOutput()` returns
        early so the physical plugins get nothing, but `processFaders()` still emits
        `universeWritten()`, so the 2D monitor / preview keeps updating. Toggled via
        `InputOutputMap::setOutputInhibited` (mirrors `setBlackout`, re-dumps on
        release so the rig catches up). Distinct from blackout. It's a **global**
        output state, so the toggle is a **main-toolbar action next to Blackout**
        (`App::m_controlBlindAction`, `:/blind.png` eye-with-slash), NOT a
        Programming-tab button — engine `outputInhibitedChanged` is the single source
        of truth that syncs every indicator. **Blue** (EOS/MA) indicators: the
        toolbar action, a full-width **canvas banner** in the Programming tab, and —
        while armed — the **entire app status-bar footer turns blue** with a white
        "● BLIND — rig muted, preview only" caption (survives tab switches, since the
        footer is always visible). Design-mode only: the toolbar action is disabled
        in Operate and force-cleared on →Operate (`App::slotModeChanged`), so a muted
        rig never survives into a live show. Not persisted.
        (Also fixed a fork wart: the **Show-mode Lock** toolbar button reused
        `:/blackout.png` and looked like a second Blackout — now a padlock,
        `:/lock.png` locked / `:/unlock.png` open.)
      - **Highlight** was already a persistent toggle; added a **Flash** button that
        momentarily flashes the selected fixtures to identify them (wires the existing
        `ProgrammerController::flashFixture`).
      - **Park** = hold fixtures out of cue output. New `ParkEffect` DMXSource (mirror
        of `HighlightEffect`) holds captured per-channel values at `forceLTP` every
        tick. `ProgrammerController::parkFixtures` snapshots each fixture's current
        pre-GM output (claim/preGMValue/release) so a fixture freezes where it sits;
        unpark releases. **Persists** to the `.qxw` as `<Park>` (round-trip
        unit-verified) via thin Doc forwarders; cleared on workspace clear + fixture
        removal. Toolbar **Park** button is context-sensitive (Park ⇄ Unpark on the
        selection) plus **Unpark all**.
      Still needs: GUI confirmation of the three toolbar buttons + that Blind darkens
      the rig while the 2D monitor still shows the look (couldn't drive the GUI
      headless this session — no accessibility permission).
- [x] **Show timeline + MTC follow** *(BUILT — TESTED 2026-07-28)* —
      Decision revised (Branson, 2026-07-15): **expand the existing Show Manager
      in place** rather than lift widgets into the Programming tab or make a new
      tab. The Show engine was already function-generic, so the work was UI +
      timecode plumbing.
      - **Collections are first-class on the timeline.** New `CollectionItem`
        (ui/src/showmanager/, modeled on RGBMatrixItem — a plain block, no
        per-step preview). `MultiTrackView::addCollection` stamps a default 10s
        duration when the collection reports no finite totalDuration (scenes) so
        the block is visible and the ShowRunner doesn't stop it instantly
        (stopTime == startTime). `slotAddItem` (append + new-track) and
        `updateMultiTrackView` (load) dispatch CollectionType; FunctionSelection
        filter now allows it. Distinct violet collection colour.
      - **MTC parse (plugin).** `MidiMtcDecoder` assembles HH:MM:SS:FF from 8
        quarter-frame (0xF1) nibbles (+2 frame offset) and full-frame locate
        SysEx (exact); reports ms + fps. Fed by CoreMIDI/ALSA/Win32 input paths;
        `MidiInputDevice::mtcTimeChanged` → `MidiPlugin` → new
        `QLCIOPlugin::timeCodeChanged`. Unit-tested (midi_test). NOTE: macOS
        CoreMIDI drops whole-packet SysEx, so full-frame locate isn't delivered
        there — quarter-frames (the running case) are.
      - **Engine follow.** `TimecodeSource` (Doc-owned, lazy) aggregates
        positions; a 200ms watchdog flips running→false when TC stops (freeze =
        manual GO / spoken scene). Auto-detect by default; `setSourceUniverse()`
        = override. Routed: `timeCodeChanged` → InputPatch (line-filtered) →
        InputOutputMap → Doc wires it to the source. `ShowRunner` gains
        `setTimecodeFollow`/`setExternalTime`; write() sets m_elapsedTime from
        the external position (never auto-finishes); a backward move is a
        `seekTo()` locate (stop running, skip past ended funcs, restart active
        ones with correct offset); cross-thread access mutex-guarded. `Show`
        forwards + persists `<TimeDivision FollowTimecode="True">`.
      - **Footer chips** (system-health row in the Show Manager): timecode chip
        grey/amber/green + live `HH:MM:SS:FF @fps`; load chip = MasterTimer tick
        compute vs budget (green/amber ≥60%/red ≥100%). `MasterTimer` now always
        measures per-tick compute into an atomic (`tickComputeMs()`).
      - Toolbar: **Follow MIDI Time Code** toggle (per-show) + source combo
        (Auto / lock to a universe).
      Unit tests: MTC decoder, TimecodeSource watchdog+override, ShowRunner
      follow/seek, Show FollowTimecode round-trip — all pass.

      **TESTING PLAN — show timeline + MTC** *(tested 2026-07-28 with Logic Pro → CoreMIDI):*
      - [x] **Collection on a track** — Show Manager → add a Collection; it
            renders as a violet block, drags/resizes, plays back (its scenes
            fire), round-trips through save/reload.
      - [x] **Follow toggle** — enable "Follow MIDI Time Code"; play the show;
            roll MTC from Logic → the cursor/looks chase the timecode; the
            footer timecode chip goes green with live HH:MM:SS:FF.
      - [x] **Freeze / manual GO** — stop MTC (spoken scene) → chip goes amber
            "holding", the show freezes at position; resume MTC → it chases again.
      - [x] **Locate** — jump Logic's playhead backward/forward → the show
            relocates (seekTo) and the right looks are active at the new position.
      - [x] **Source override** — with two MIDI inputs, set the source combo to
            one universe; confirm only that source drives the clock.
      - [x] **Load chip** — under a heavy look, watch the load chip climb; verify
            amber/red thresholds read sensibly against the 20ms budget.
      - [x] **Persistence** — enable Follow, save, reload → toggle restores;
            check `.qxw` for `<TimeDivision … FollowTimecode="True">`.

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

- [ ] **Venue / house-power source type (BACKLOG)** — model building/house power as
      a source whose **circuits are the wall outlets**, with a service rating (main
      breaker), so sockets that share a house circuit are grouped and flagged when
      they'd trip together. Decided (2026-07-15) to keep the current standalone
      **wall-socket source** (`PowerSource::WallSocket`, single implicit circuit) for
      quick one-offs and add **House panel / Venue** as an *additional* source `Type`
      later — not to replace wall sockets. Ties into the `.venue` export (venue-fixed
      infrastructure): house panels would travel with the venue, fixture assignments
      stay with the show. Power pane already supports per-source Type + flat (≤1
      circuit) vs multi-circuit rendering, so this is mostly a new Type + a
      service-rating field + overload roll-up at the panel level.

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
- *(Highlight / Park / Blind — DONE, moved to "In progress / next" batch above.)*
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
- [ ] **Look transition times** — *DECIDED, promoted to "In progress / next" as
      "Per-look (per-parameter) fade times"; see there for the override rule.*

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
- [ ] **Timecode / cue sheet** — *DECIDED & promoted; see "Show timeline in the
      Programming tab + MTC follow" under In progress / next.*
- [ ] **More movement shapes / draw-your-own** — beyond built-in EFX shapes,
      let users draw custom movement paths on an XY canvas.
- [ ] **Dimmer curve per fixture / channel** — override the default linear
      dimmer response with square, cubic, etc. curves, per fixture or group.
      Stored in Scene or FixtureGroup; applied at writeDMX time.
- [ ] **MIDI-mapped look recall** — *PROMOTED into the Control Map feature
      (Phase 4); see "Control Map: static MIDI mapping" under In progress / next
      and `CONTROLMAP.md`.*

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
