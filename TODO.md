# TODO — Programmer / Programming-tab work

Active + not-yet-built work for the fork's programming/looks workflow.
**Shipped work has moved to [DONE.md](DONE.md)** (the completed log, with
per-feature detail + deferred/eyeball notes). Add new work here; move an entry
to DONE.md when it ships. See also the session memory under
`~/.claude/.../memory/`.

---

## ✈️ Travel / offline work — no show rig or control surface needed

Buildable + testable on a laptop (offscreen QTest / node for JS effects; the app
runs headless via `QT_QPA_PLATFORM=offscreen`). Good picks while away from the rig:

- **More one-shot effects** — now that the lifecycle exists (`EFFECT_LIFECYCLE_DESIGN.md`),
  author bursts / reveals / sweeps as `oneshot` scripts (pure JS, node-testable).
  A `wand.js` is the template.
- **Effect-lifecycle follow-ups** — `span` sync on the **Show timeline** (today falls
  back to naturalDuration); fold the RGBScript `Once` path into the lifecycle;
  per-look `syncTo`/`onFinish` override UI.
- **Audio effects** — bump the FFT band count / raise the 5 kHz cap for finer Hz
  targeting (node-testable); more audio-reactive scripts.
- **Rebrand → qlcconsole** — titles / About / launcher / macOS bundle names. No hardware.
- **Design-doc work** — "Look" as first-class assembly unit; unified object editor.
- **More stage objects** — flats / drapes / set pieces (2D monitor, offscreen-testable).
- **Small polish** — MTC-chip already done; any bugs found reviewing the code.

**Parked until back at the rig:** the whole **control-surface** effort (PMJ / APC40
mk2 / Xbox — needs the boards) and the **`RIG_TEST_PLAN.md`** verification pass
(needs movers / pixel panel / MIDI keyboard / audio in). The move-in-black + note-
effect timing items also want the rig to confirm.

---

## Recently shipped (verify on rig, then move to DONE.md)

- **PMJ Blackout LED bug — SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}`.
  Branson: "the blackout button still doesn't light on the board." Root
  cause: `PMJOverlay` was never listening for `InputOutputMap::blackoutChanged`
  at all — only Blind's `outputInhibitedChanged` and grand master were wired,
  despite a stale doc comment claiming blackout was covered too. Pressing `O`
  correctly toggled blackout every time; nothing ever told the engine to
  repaint LEDs afterward, so the LED just sat at whatever it was on connect.
  Added `slotBlackoutChanged`, mirroring the existing Blind/GM pattern
  exactly. Builds clean, smoke-tested. **Not yet re-verified on the real
  board.**

- **P2 slice 1 — selection mode: Select/Load wiring + per-fixture intensity
  faders — SHIPPED (2026-08-18).** `engine/src/programmercontroller.{h,cpp}`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/programmingmanager.{h,cpp}`. First
  buildable piece of the P2 design (`CONTROL_SURFACE_DESIGN.md`) agreed
  after Branson asked for "a cohesive think" on tying a whole SCENE to the
  faders, not just one focused palette.
  - **New engine primitive**: `ProgrammerController::writeChannelLive
    (fixtureId, channel, value)` — general-purpose, palette-agnostic raw DMX
    write, mirroring `VCSlider::writeDMXLevel`'s mechanism exactly (grabs/
    reuses a `GenericFader` via `Universe::requestFader()`, sets the
    `FadeChannel`'s target). Verified via code audit that this is safe as a
    one-shot call rather than needing MasterTimer registration:
    `Universe::processFaders()` (`engine/src/universe.cpp:333`) writes every
    outstanding requested fader on its own, every tick, regardless of who
    last touched it — so the target persists without re-invoking this method
    every frame. Also routes the edit for Save-bookkeeping the same way
    VCSlider does (`Doc::routeProgrammerEdit()`, falling back to
    `Doc::setProgrammerValue()`). This was the one deliberately-deferred item
    from the P2 design write-up ("needs a ProgrammerController method audit
    to find the right entry point") — audited, then built.
  - **Selection substrate**: reuses `ProgrammerController::
    setProgrammerSelection()`/`programmerSelection()` directly (not
    `programmerSubSelection()`, which keeps its original narrower "deviate
    individual fixtures out of a group palette" purpose, untouched by PMJ
    for now). New `PMJOverlay::sceneTargetFixtures()` defines "canvas order"
    concretely: the scene shown in the Programming canvas
    (`ProgrammingManager::currentSceneId()`, new getter, same pattern as
    `currentPaletteId()`)'s fixture-group members (expanded, group order)
    then its individually-fixed fixtures, deduped. **Not yet paged past
    10** — a scene with more targets only exposes the first 10 for now
    (flagged, not silently pretended-away).
  - **Select(N)**: toggles that target fixture into/out of
    `programmerSelection()` — multi-select, confirmed with Branson.
    **Load(N)**: replaces the selection with just that one target, matching
    the Role's own doc comment ("load item N into the programmer").
  - **Faders 1-6, Selection mode**: when nothing's focused in the Look
    Editor (`faderInUse()` false — Look-edit mode still wins when it
    applies), fader N writes the intensity channel of the Nth selected
    fixture live via `writeChannelLive()`.
  - **LED highlighting**: Select/Load(N) lit (`Valid`) when strip N has a
    real target in the open scene, brighter (`Selected`) when that fixture
    is actually in the current selection — refreshes live via the existing
    `programmerSelectionChanged` signal (already built, previously unused by
    any UI).
  - **Deferred to a follow-up slice** (per the design doc's own list):
    faders 7-10 (RGBW of selection), Enc 3/4 (Focus/Zoom of selection),
    Reset(N) zeroing a selection-mode fader (currently Reset only covers
    Look-edit mode's `faderInUse()` faders), and the >10-target paging via
    the `Page` button.
  - Builds clean, smoke-tested. **Not yet verified against the real board —
    this is genuinely new, first-time-tested DMX-write plumbing, unlike P1's
    palette-refresh-based writes**, so worth an attentive first test rather
    than an "it built, ship it" pass.

- **Blind/Blackout footer indicators — SHIPPED (2026-08-18).** `ui/src/app.{h,cpp}`.
  Branson: "we don't need two bars for blind, the footer being blue is enuff" —
  removed the redundant `m_statusBlindLabel` text chip (Blind already turns
  the whole footer blue, unmistakable on its own); the toolbar button's
  checked state is untouched. Added a Blackout counterpart Branson asked for
  ideas on — chose the smaller of two options (a compact chip, not a
  whole-footer-red treatment like Blind's): new `m_statusBlackoutLabel`, a
  red "● BLACKOUT" chip shown/hidden in `slotBlackoutChanged()`. Builds clean.

- **P1 slice 8 — numbered fader labels + Up-button reset/highlight —
  SHIPPED (2026-08-18).** `engine/src/controlsurface.h`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/lookeditor.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`. Two asks from Branson after slice 7
  landed: (1) "it'd be nice to see what faders we're moving... number them
  for clarity," (2) "we can also use the fader up button to reset and
  highlight the faders in use."
  - **Numbered labels**: the Color page's R/G/B were previously *inside*
    `QColorDialog` (opaque, can't inject labels) — pulled them out into the
    same numbered-vertical-slider pattern the White/Amber/UV sliders already
    used, so all six read "1 R" "2 G" "3 B" "4 W" "5 A" "6 UV," matching
    `PMJOverlay`'s fader-index mapping exactly. Bidirectional sync with the
    dialog's own picker (`slotColorChanged`/new `slotRgbSliderChanged`, both
    `blockSignals`-guarded to avoid feedback loops); `commitColor()` now
    reads RGB from the numbered sliders (the canonical source) rather than
    `m_colorDialog->currentColor()`.
  - **Reset + highlight**: new `ControlSurface::RoleType::Reset` (index =
    strip N) in the P0 engine core — device-agnostic, so APC40/Xbox overlays
    can reuse it later. PMJ's 10 "N-Up" buttons (previously `CS::Role()`,
    completely unbound) now carry it. New shared `PMJOverlay::faderInUse(int)`
    is the single source of truth for "does fader N do anything right now,"
    used by all three of: the Level write path (refactored to call it instead
    of duplicating the Color/Dimmer type check inline), the new Reset write
    path (zeroes that channel via the same `setDesignColorChannel`/
    `setDesignDimmerValue` from slice 7, value=0 — no new engine write method
    needed), and `stateFor()`'s new `Reset` case (lights the Up button
    `State::Valid` exactly when `faderInUse()` is true).
  - **Live LED refresh on focus change** (needed for the highlight half to
    update the instant a different look is clicked, not just on the next
    unrelated interaction): new `LookEditor::lookFocusChanged(quint32)`
    signal, emitted at the end of every `setPalette()` call (both the
    empty-palette and normal paths) — re-emitted by `ProgrammingManager` as
    `currentPaletteIdChanged`. `PMJOverlay` couldn't connect to this at
    construction time (`ProgrammingManager` isn't built yet when
    `App::initDoc()` constructs `PMJOverlay` — same ordering constraint
    slice 4 hit) — solved with new `programmingManager()`, a `const` helper
    that lazily finds-and-connects-once on first use (`mutable` cache member),
    reused at all four call sites that previously each did their own
    `m_app->findChild<ProgrammingManager*>()`.
  - Builds clean, smoke-tested. **Not yet verified against the real board.**

- **PMJ hardware fix (not code) — OpenDeck LED channel misconfig, 8 buttons
  affected — WRITTEN TO THE BOARD (2026-08-18).** Root-caused via
  `qlcplus-midi-profiler`'s `opendeck dump`/direct SysEx reads (no
  interactive `identify` needed — the LED's own `activation_id` was already
  correct, only its `channel` field was wrong): `Set`, `6-Up`, `6-Load`,
  `Left`, plus 4 currently-unmapped notes all had their paired LED listening
  on MIDI channel 1 (raw) instead of channel 9, so QLC+'s feedback (always
  sent on ch9) never reached them — explains "Set toggles but never lights."
  Backed up first (`qlcplus-midi-profiler/backups/pre-led-channel-fix.json`,
  restorable via `qlc-midi opendeck restore`), then wrote the correct
  channel (raw 9) to all 8 LED slots directly via `OpenDeck.write_checked`,
  verified via read-back. **Confirm live**: Set/6-Up/6-Load should now
  light like `O`/Master already do.

- **P1 slice 7 — context-aware faders (Level 1-10) — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.cpp`. Per
  Branson: faders should mean whatever's relevant to the palette focused in
  the Look Editor, not a fixed submaster/per-fixture role — "if we're
  selecting a [color] feature we'd do sliders for R G B W A U." Reuses the
  same `ProgrammingManager::currentPaletteId()` ground-truth built for the
  pan/tilt encoder work (slice 4). New `ProgrammerController::
  setDesignColorChannel(paletteId, channelIndex, value)` (0=R..5=UV, writes
  via `QLCPalette::colorToString` matching `LookEditor::commitColor()`'s own
  write pattern) and `setDesignDimmerValue(paletteId, value)` — both
  absolute writes (faders aren't relative like the encoders), same
  explicit-paletteId/refresh-every-referencing-scene/`designPositionWritten()`
  contract as `nudgeDesignPanTilt()`. `PMJOverlay`'s `Level` case now checks
  the focused palette's type: Color → faders 1-6 = R/G/B/White/Amber/UV,
  Dimmer → fader 1 = intensity, anything else (PanTilt/Aim/nothing focused)
  → faders stay inert (confirmed with Branson: leave dark/idle when nothing
  applies, don't fall back to a permanent submaster baseline — an open
  question from slice 1 is now resolved this way). Live redraw confirmed
  free: `LookEditor::setPalette()`'s Color case uses real Qt widgets
  (`QColorDialog::setCurrentColor`, `QSlider::setValue`) which repaint
  themselves on state change — unlike the custom `VCXYPadArea` from slice 5,
  no extra `update()` call needed. Builds clean, smoke-tested. **Not yet
  verified against the real board.**

- **P1 slice 6 — encoder direction, confirmed working on the rig —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`. After slice 5 the pin
  finally moved, but both encoders' raw MIDI delta turned out to be
  physically inverted relative to their on-screen effect — confirmed live:
  turning Enc 1 (pan) clockwise moved the dot left, not right. First pass
  negated the wrong encoder (tilt) on a guess; Branson clarified "it's enc1
  thats backwards," so negated pan instead — fixed pan, but left tilt's
  sign inconsistent (undiscussed/untested but same hardware, so likely the
  same physical inversion). Branson called this out directly: negate both,
  so "clockwise increases" is the one consistent rule for both axes rather
  than an asymmetric fix. **Confirmed working on the real PMJ — pan/tilt
  encoder nudge is done.**

- **P1 slice 5 — the actual root cause: `VCXYPadArea::setPosition()` never
  repaints itself — SHIPPED (2026-08-18).** `ui/src/lookeditor.cpp`,
  `ui/src/pmjoverlay.cpp`. Slice 4 was real but not sufficient — Branson
  turned the encoders again after that build, still nothing ("STILL NOT
  MOVING THE PIN"). Added logging inside the full chain (PMJOverlay's
  `slotRoleActivated` Param case) and it proved every single precondition
  was green on every turn: `ProgrammingManager` found, a valid palette id,
  a real palette object, `type() == PanTilt` exactly, `Doc::programmer()`
  non-null — `nudgeDesignPanTilt()` was being called correctly every time.
  So the bug was never upstream at all; traced `nudgeDesignPanTilt()`'s own
  body (`engine/src/programmercontroller.cpp`) and the write side
  (`QLCPalette::setValue()`/`intValue1()`/`intValue2()`) — both correct.
  Root cause was purely on the redraw side: `VCXYPadArea::setPosition()`
  (`ui/src/virtualconsole/vcxypadarea.cpp`) only updates internal state and
  emits `positionChanged()` — it never calls `update()`/`repaint()` itself.
  Every other call site pairs it with an explicit `update()` right after
  (`mousePressEvent`/`mouseMoveEvent`, and the joystick-drag redraw path
  already in `LookEditor` at line ~429) — except `LookEditor::setPalette()`'s
  `PanTilt` case (line ~714), which called `setPosition()` alone. So the
  palette value and the widget's internal `m_dmxPos` really were updating
  correctly on every encoder click; the dot just never got painted. Added
  the missing `m_xyPad->update()` after `setPosition()` there. Also removed
  the now-resolved chain debug logging from `pmjoverlay.cpp`. Builds clean,
  smoke-tested (no crash). **Not yet re-verified against the real board.**

- **P1 slice 4 — nudgeDesignPanTilt() no longer depends on stale
  focused-scene tracking — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`, `ui/src/app.cpp`. Slice 3's fixes
  (value-scaling, XY-pad redraw) turned out to be correct but incomplete —
  Branson dragged the XY pad to re-center, turned Enc 1/2 again, still no
  movement. Re-added debug logging (encoder-side decode now confirmed
  perfect: raw `255`/`2` → `-5°`/`+10°`, exactly as intended) — but the
  SECOND log line, inside `nudgeDesignPanTilt()` itself, never printed at
  all. It was returning at the very first check: `m_focusedSceneId` was
  invalid. Root cause: that tracking (`ProgrammerController::
  m_focusedSceneId`/`m_focusedPaletteId`) only gets set when a scene is
  freshly opened in the Programming tab's canvas — it doesn't persist
  across an app relaunch, and dragging the XY pad by hand never needed it
  in the first place (`LookEditor::slotPanTiltChanged()` only needs its own
  `m_paletteId`, tracked independently). So the encoder path had a real
  dependency the mouse path never had — not a fluke, a design gap.
  - **`nudgeDesignPanTilt()` re-signatured** to take an explicit
    `paletteId` instead of reading `m_focusedSceneId`/`m_focusedPaletteId`
    at all. Refreshes every scene that actually references the palette
    (`scene->palettes().contains(paletteId)`, walking `m_doc->functions()`)
    instead of one assumed "focused" scene — more correct anyway, since a
    palette can legitimately be shared by more than one scene.
  - **New `ProgrammingManager::currentPaletteId()`** — the ground truth for
    "what's on screen right now" (`m_lookEditor->paletteId()`, itself made
    reachable via a new `LookEditor::paletteId()` getter in slice 3).
    `PMJOverlay` now takes an `App*` at construction (`app->findChild<
    ProgrammingManager*>()`, the same reach-pattern already established
    elsewhere in this codebase) to get this instead of going through
    `ProgrammerController`'s tracking.
  - **Found and fixed a real, unrelated header bug along the way**:
    `programmingmanager.h` uses `QDoubleSpinBox*` but never forward-declares
    it (`QSpinBox` is declared, `QDoubleSpinBox` isn't) — worked by
    accident everywhere it was previously included transitively-first;
    broke the moment `pmjoverlay.cpp` included it directly. Fixed the
    header itself (added the missing forward declaration) rather than
    working around it locally — a real latent bug, not a workaround target.
  - Builds clean, smoke-tested (board disconnected, no crash). **Not yet
    re-verified against the real board** — fourth round on this exact
    feature; this one at least has hard evidence (the debug log) behind the
    diagnosis rather than another guess, but still needs a real turn of
    the encoder to confirm.

- **P1 slice 3 — two real bugs found via debug logging, both fixed —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `ui/src/lookeditor.{h,cpp}`, `ui/src/programmingmanager.cpp`. Branson's
  slice-2 test ("still no movement") got temporary file-based debug logging
  added to `nudgeDesignPanTilt()` and `PMJOverlay::slotInputValueChanged()`
  (matching this session's established fallback when guessing plateaus) —
  the resulting log conclusively showed two real, independent bugs rather
  than a workflow mistake:
  1. **Value-scaling bug.** The encoder's raw MIDI twos-complement delta
     (1 = +1, 127 = -1, confirmed weeks earlier via `qlc-midi monitor`) is
     NOT what `inputValueChanged()` actually delivers — QLC+'s MIDI plugin
     already scales it into its internal 0-255 space first (MIDI2DMX,
     ~x<<1, 127→255 special-cased). The log showed raw values `2` and `255`
     arriving, not `1`/`127` — my decode threshold (64/128, correct for raw
     7-bit MIDI) was silently turning a -1 click into a +127 click, which
     instantly clamped pan/tilt to its range boundary on the very first
     turn and then correctly did nothing on every subsequent one (already
     at the clamp) — indistinguishable from "not working" without the log.
     Fixed: threshold is 128/256, matching the space the value is actually
     in by the time it reaches this code.
  2. **The XY pad widget never repaints.** The SAME log also proved
     `nudgeDesignPanTilt()` WAS finding a focused scene (id 0 — a real,
     valid id; only `4294967295` means unset) and, once the palette was
     added to the scene, WAS finding and would have modified it — so the
     underlying engine data was correct the whole time. What was missing:
     nothing told `LookEditor`'s on-screen XY pad to redraw after an
     engine-side value change — only the user dragging it by hand ever
     triggered that before, since `applyDesignJoystick()` (the only prior
     caller of the `designPositionWritten()` signal this reuses) never
     touches a PanTilt-type palette's displayed values at all (it only
     drives an Aim-target, a different widget). Fixed with a new
     `LookEditor::paletteId()` getter and a redraw
     (`m_lookEditor->setPalette(m_lookEditor->paletteId())`) added to
     `ProgrammingManager::slotDesignPositionWritten()` — harmless/no-op
     for the Aim-look case, the only thing that repaints a PanTilt palette
     after a nudge.
  - All temporary debug logging removed after diagnosis, per this
    session's established pattern. Builds clean, smoke-tested (board
    disconnected, no crash). **Not yet re-verified against the real
    board/encoders** — this is the third round of "build, hand back for a
    real test" on this exact feature; worth a clean pass before assuming
    it's actually done.

- **P1 slice 2 — LEDs only light for what's real; Enc 1/2 nudge pan/tilt —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `engine/src/programmercontroller.{h,cpp}`. Follow-on from Branson's first
  real hardware test of slice 1: `Set` correctly toggled Blind (input
  pipeline genuinely works), but Blackout/Blind LEDs didn't light at all,
  and `1-10`/`1-10 Load`/`Go`/`Back` all lit for no reason — confusing,
  and backwards from "highlight what's useful, leave the rest dark."
  - **LED semantics fixed**: `stateFor()` now returns `Empty` (dark) for
    every role that doesn't drive real behavior yet (Page/Select/Load/
    Param/Transport) instead of defaulting them to `Valid` (dim). Only
    Blackout/Blind (dim idle, bright when engaged) light at all now — an
    honest reflection of what this slice actually does.
  - **Real finding, not covered by the LED fix**: Branson tried creating a
    pan/tilt look and turning the encoders — nothing moved. Turned out
    `ProgrammerController::applyDesignJoystick()` (the existing, working
    HID-joystick pan/tilt path) explicitly no-ops for a "Standard Pan/Tilt"
    look — raw palette-value authoring for that case doesn't exist ANYWHERE
    in the app yet, not just missing for the PMJ; only an Aim-palette look
    (which drags a floor-space target) has ever had a working live-nudge
    path. Confirmed with Branson before building rather than guessing at
    Look-authoring semantics unilaterally.
  - **New engine method**: `ProgrammerController::nudgeDesignPanTilt(float
    dPanDeg, float dTiltDeg)` — finds the focused scene's controlling
    PanTilt palette (same precedence `applyDesignJoystick()` uses: the
    explicitly focused palette, else the last PanTilt-type one on the
    scene), adjusts its stored `intValue1()`/`intValue2()` (raw degrees,
    same 540°/270° space `LookEditor`'s XY pad already authors it in — see
    `lookeditor.cpp`'s `PAN_DEG`/`TILT_DEG`), and refreshes the running
    scene via `markSceneEdited()` — the exact same live-refresh path a Look
    Editor slider drag already uses (`Scene::requestPaletteRefresh()`, not
    a full `resetRuntime()` teardown, so repeated nudges don't restart the
    scene's fade-in or flash other channels). No-op for an Aim-look scene
    (nothing to nudge — the joystick's existing path already owns that
    case) or when nothing's focused.
  - **Wired**: `Enc 1` → pan, `Enc 2` → tilt, 2°/click, unconditional (no
    paging yet — Enc 3/4 stay unbound, pending the same page-targeting
    model as the rest of this slice). Had to hand-decode the encoder's raw
    twos-complement value myself (`value<64 → +value, else value-128`) —
    confirmed QLC+ doesn't do this upstream of `inputValueChanged` even for
    an `Encoder`-typed channel (only `QLCInputSource::decodeRelativeDelta()`,
    a *bound-widget* config object, does — not applicable here), matching
    the same raw values `qlc-midi monitor` showed during the original
    encoder classification work.
  - Builds clean, smoke-tested (board disconnected, no crash/regression).
    **Not yet re-verified against the real board** — worth checking: LEDs
    stay dark except Blackout/Blind now, and turning Enc 1/2 with a
    PanTilt-palette look focused actually moves pan/tilt live.

- **P1 slice 1 — PMJ Black 1 overlay onto the control-surface engine —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}` (new), `ui/src/app.{h,cpp}`,
  `ui/src/CMakeLists.txt`. First real device overlay on top of the P0 engine
  from an earlier session (`ControlSurfaceEngine`/`ControlSurface` — device-
  agnostic role/state vocabulary + a generic LED-repaint loop, see
  `CONTROL_SURFACE_DESIGN.md`). Deliberately scoped down from the full
  design-doc Phase 1 vision to what has clean, unambiguous integration
  points today — flagging the scope honestly rather than half-guessing the
  rest:
  - **Full role table registered** — every one of the PMJ's 69 real MIDI
    channels (hardcoded from `resources/inputprofiles/PMJ-Black-1.qxi`, not
    re-parsed at runtime, so a hand-edited profile can't silently desync the
    binding) gets a `ControlSurface::Control` + `Role`, matching the design
    doc's table: `Master`→GM, `Ch 1-10`→per-strip `Level`, `1-10`→`Select`,
    `N-Load`→`Load`, `Enc 1-4`→`Param`, `Groups/Looks/Effects/Macros/Fix
    Cont`→`Page`, `Go/Back/Left/Right/Pre Page/Next Page`→`Transport`,
    `O`/`Set`→`Blackout`/`Blind` (the design doc's proposal, confirmed).
    `N-Up`/`N-Down` and `Favorites` (proposed Tap) are registered with no
    Role — still-open per the design doc's discussion point and the "the
    only Tap in the codebase is Programming-tab-local, not global" finding.
  - **Only 3 are wired to real behaviour so far**: Master fader →
    `InputOutputMap::setGrandMasterValue()`, `O` → `toggleBlackout()`, `Set`
    → `setOutputInhibited()` — all engine-level Doc APIs, no App-level
    plumbing needed. Select/Load/Param/Transport/Page-switching are received
    by the engine and logged, not yet driving real selection/navigation —
    that needs the UI-side "what's currently selected/active" concept this
    session established doesn't cleanly exist yet for fixture groups in the
    Programming tab. Next slice's real dependency, not a small gap.
  - **LED feedback**: `ledSink` sends real Note-On via the existing generic
    `InputOutputMap::sendFeedBack()` (channel 9, matching the profile) —
    reused, not reinvented. Brightness snaps to the OpenDeck "steady levels"
    set (15/31/47/…/127) found via `qlcplus-midi-profiler`'s README, so a
    state change can never accidentally set a control blinking. `stateFor()`
    reflects real state for Blackout/Blind (dim when idle, bright when
    engaged) and the active page (selected vs. valid); Select/Load default
    to a flat "present" dim state pending the same selection-model gap above.
  - **Registered on every startup**, PMJ connected or not — `App::initDoc()`
    constructs the engine + overlay unconditionally, right after
    `startUniverses()`. No crash/error either way; smoke-tested via a full
    launch + screenshot with the board disconnected.
  - Building this slice surfaced one real bug in my own code, not the
    engine: `using CS = ControlSurface;` is invalid C++ (type-alias syntax
    can't name a namespace) — direct compile confirmed the fix
    (`namespace CS = ControlSurface;`).
  - **Partially live-verified, one real bug found and fixed.** Branson
    reconnected the board: `Set` correctly toggled Blind (confirms the input
    pipeline works end-to-end for real), but no LEDs lit at all, when at
    minimum every registered/`Valid` control should have shown a dim glow.
    Root cause: `sendLed()` hardcoded `sendFeedBack(0, ...)` — assumed the
    PMJ's output would be on universe 0, with no actual basis for that
    guess. If that's wrong, feedback silently goes nowhere. Fixed two ways:
    (1) `PMJOverlay` now scans every universe's `InputPatch::profileName()`
    for one matching "PMJ Black 1" at construction time
    (`findKnownUniverse()`), so LEDs light on connect/launch without
    requiring a press first; (2) `slotInputValueChanged()` also re-learns
    the universe from real traffic on every event (not just once), so a
    board patched *after* startup, or re-patched to a different universe
    mid-session, self-corrects instead of staying dark. Builds clean,
    smoke-tested with the board disconnected (no crash/regression) — the
    universe-discovery fix itself still needs a real check with the board
    connected, since that's exactly the scenario it fixes.

- **Three more backstage color themes — SHIPPED (2026-08-18).**
  `ui/src/app.h`, `ui/src/app.cpp`. Turned out this fork already had a whole
  theme system (`App::Theme` enum, `applyTheme()` building a `QPalette`,
  View → Theme menu, persisted to `QSettings`) from an earlier session —
  Default/Tan/Blue. Branson asked for a few more, name-checking a "QLC+
  original" look and a red-shifted night-vision theme, and to look at VS
  Code's dark themes for ideas. Scoped to chrome only (menus/toolbars/tabs/
  dialogs) per Branson's choice — the 2D canvas (trusses/fixtures/grid) is
  hand-painted with ~200 hardcoded `QColor` literals across 18 files and
  doesn't follow the palette; making it theme-aware too is a much bigger
  follow-on project if ever wanted. Added: **QLC+ Original** — corrected
  after Branson called out that my first pass (a dark charcoal grey) was
  wrong: he pulled a fresh copy of upstream QLC+ (`github.com/mcallegari/
  qlcplus`, now checked out at `/Users/branson/git/qlcplus`) to check
  against, which confirmed upstream ships NO custom palette or stylesheet
  at all — it's plain native Qt Fusion light grey. Redone to match that
  (`#efefef` window, white base, black text, `#4a90d9` highlight) — unlike
  "Default" (which just follows whatever the OS's current light/dark
  setting is), this stays that same classic light look on demand regardless
  of OS mode; **Red Shift** (blue channel kept
  near-zero throughout, the same principle as a red stage torch or
  astronomy light, for working backstage in the dark without wrecking night
  vision); **VS Code Dark** (VS Code's "Dark+" editor greys plus its iconic
  `#007acc` accent blue). Same mechanism as the existing two themes — just
  new `QPalette` color blocks in `applyTheme()`'s switch and three new
  entries in the View → Theme menu's data-driven choice list. **Verified
  live**, carefully: with two `qlcconsole` processes running (Branson's real
  session plus my own isolated scratch-file test), AppleScript's `process
  whose unix id is N` turned out to silently match the WRONG process by
  name regardless of the id filter — an accidental click opened Branson's
  real View → Theme menu once (no theme was actually changed; the
  submenu-item click failed before selecting anything, and Escape closed
  it). Switched to a safer verification method with zero click risk:
  pre-set `workspace.theme` via `defaults write org.qlcplus.qlcconsole`
  before launching the scratch instance fresh (it reads the setting once at
  startup), screenshotted Red Shift applied correctly (warm orange-red
  layer-row highlight and tab underlines), then restored the setting to
  Branson's original value (`Blue`) afterward.

- **Stage centre lines now have their own show/hide toggle — SHIPPED
  (2026-08-18).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `ui/src/monitor/monitor.cpp`. The teal crosshair marking the stage centre
  was previously bundled into the same `m_gridItems` list as the grid lines
  themselves, so there was no way to hide it independent of the grid. Gave
  it dedicated `m_centerLineV`/`m_centerLineH` members (rebuilt alongside
  the grid in `updateGrid()`, since their position depends on the same
  `m_cellPixels`/offsets, but no longer added to `m_gridItems`) and a new
  `setCenterLinesVisible()`/`centerLinesVisible()` pair. New "Center" footer
  toggle button next to Rulers/Labels, persisted via `QSettings`
  (`monitor/centerlines`, default on) the same way Rulers/Grid already are.
  Built clean, not yet verified live.

- **Truss tether line: no-line threshold now matches the truss's visual
  width — SHIPPED (2026-08-18).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `updateTrussAnchorLines()`. Branson reported still seeing the line even
  when a fixture looked like it was sitting right on top of the truss. The
  no-line threshold was a fixed 4px around the exact mathematical
  centreline — too tight, since the truss itself is drawn with real width,
  so anywhere within that drawn thickness reads as "on the truss" to the
  eye without being at cross=0 exactly. Threshold is now `max(4px, half the
  truss's own drawn width)`, so the line stays hidden across the whole
  visual footprint of the truss bar, not just its mathematical centre.
  Built clean, not yet verified live.

- **Truss tether line now terminates at the truss's near EDGE, not its
  centreline — SHIPPED (2026-08-18).**
  `ui/src/monitor/monitorgraphicsview.cpp`, `updateTrussAnchorLines()`.
  Follow-on to the entry above: the anchor end of the line was always
  `Truss::positionAt(trussOffset)` — dead centre of the truss's width —
  so the line visually ran INTO the truss body before disappearing under
  it, rather than stopping where the fixture actually meets the truss.
  Reworked the skip-check and the anchor point separately: first decide
  whether to draw at all by comparing `|trussCross|` against the truss's
  half-width (plus ~2px of slack) in world units — this is the same
  "is the fixture still within the truss's visual footprint" test as the
  entry above, just computed before picking an edge, so it isn't skewed by
  which side the line would terminate on. Then, only once drawing, offset
  the anchor point from the centreline by exactly the truss's half-width,
  signed toward whichever side the fixture's `trussCross` is on — so the
  line always starts right at the truss's surface on the fixture's side,
  never inside it. **Verified live** on the isolated scratch copy: cropped
  in on the fixture/truss boundary and confirmed the dashed line now
  stops right at the truss edge instead of running through it.

- **Real bug, finally cornered: Lighting Studio can come up completely empty
  on a fresh launch — trusses, layers, and every fixture missing — even
  though the save was perfectly intact — FIXED (2026-08-18).**
  `ui/src/app.cpp`, `App::loadXML(const QString&)`. This is the bug behind
  Branson's "add a fixture, save, close, reopen — fixture not there at all"
  report, and it very nearly got written off as our earlier file-collision
  false alarm — glad we kept pushing. Root cause, found by reproducing on an
  isolated scratch copy (never touching Branson's real file) with temporary
  file-based tracing at three levels: (1) `MonitorProperties::loadXML()`
  parses every single element correctly — Layer 1, both trusses, all three
  `FixtureRig` entries, confirmed via a log of every XML element it visits;
  (2) yet `Monitor::fillGraphicsView()` — the function that actually
  populates the 2D canvas from that loaded data — ran with `docFixtureCount
  = 0`, i.e. `Doc` had ZERO patched fixtures at the moment it ran; (3) but
  the Fixture Manager (Hardware tab), opened moments later in the SAME
  running instance, correctly listed all three fixtures. So the doc data was
  never actually lost — `Monitor`'s graphics view had just been built once,
  too early, from an empty doc, and never told to rebuild. The mechanism:
  `main.cpp` calls `app.startup()` (constructs all tabs, including
  whichever one is the workspace's saved `CurrentWindow` — shown
  immediately, against whatever `Doc` holds at that instant) BEFORE calling
  `app.loadXML(QLCArgs::workspace)` for a `-o`/`--open` command-line file.
  Fixture Manager's tab tree happens to rebuild itself constantly from many
  UI interactions, so it self-heals; `Monitor::fillGraphicsView()` has
  almost no other trigger and stayed stale. It only ever surfaced visibly
  once a save carried `CurrentWindow="Monitor"` — i.e. once Lighting Studio
  became the tab a workspace reopens directly into, which is exactly the
  workflow this whole session has been exercising. The "open recent file"
  path already knew to call `Monitor::instance()->updateView()` +
  `FixtureManager::instance()->updateView()` after loading (see the existing
  code a few lines above in the same file) — the command-line/`-o` load
  path just never got the same treatment. Fixed by adding those same two
  calls at the end of `App::loadXML(const QString&)`, the shared function
  underneath both paths, so every caller benefits and the "recent file" path
  just does one harmless extra refresh. **Verified live**: reproduced the
  exact empty-canvas symptom on an isolated scratch copy of Branson's file,
  confirmed the fix resolves it (Layer 1, both trusses, all three fixtures,
  cyan rings, and the tether line all render correctly on a fresh launch),
  screenshot-confirmed both before and after. All temporary debug logging
  removed.

- **A truss-bound fixture that isn't sitting right on the truss now gets a
  thin tether line back to its anchor point — SHIPPED (2026-08-17).**
  `ui/src/monitor/monitorgraphicsview.{h,cpp}`, new `updateTrussAnchorLines()`
  + `m_trussAnchorLines`. Direct follow-on to the cross-offset drag fix:
  Branson asked whether an off-truss fixture should get a visual connector
  back to the truss so the binding reads clearly instead of relying on
  proximity, and whether anything else was worth doing. Implemented a
  dashed line from the fixture's current center to its projected point on
  the truss centerline (`Truss::positionAt(trussOffset)`, ignoring cross —
  i.e. the point it would sit at with no offset), skipped when the fixture
  is within 4px of that point so a normally-centered rig stays clutter-free.
  Color was a follow-up design question Branson raised: rather than a fixed
  color, the tether now matches the truss's OWN palette — its neutral
  unselected chord grey (160,163,172) at rest, switching to the truss's own
  selected amber (255,180,0) when the truss (and therefore its extended
  group of bound fixtures, per the anchor-selection fix above) is the
  current selection. This deliberately leaves the fixture's own existing
  selection color (yellow) and bound-to-truss ring (cyan) untouched — those
  are separate, already-established indicators; only the tether reflects the
  truss's selection state. Rebuilt on every truss/fixture move
  (`slotFixtureMoved()`, `slotTrussMoved()`, including its elevation-drag
  branch), on any general item-state refresh (`refreshItemLayerState()`,
  which covers load, POV switches, and lock toggles), and on selection
  change (`extendSelectionToGroups()`) so the color follows selection
  immediately without requiring a move. Vertical trusses (towers) are
  skipped — same reasoning as the cross-offset fix, no "across" concept
  there. **Verification note:** while testing this live, launching
  `surfacetesting.qxw.before-padfix` myself collided with Branson's own live
  session against the same file — each save raced the other's, which briefly
  looked like a real persistence bug (a fixture attach not surviving a
  restart) before we tracked it down: Branson closed his instance, reopened
  cleanly with me not touching the file, and confirmed the attachment DID
  survive a real restart — so that was purely our concurrent access, not a
  product bug. Lesson: don't launch the app against a workspace file the
  user might have open live. Branson then confirmed live: the tether line
  renders correctly and turns amber when the truss is selected — but the
  FIXTURE's own outline stayed yellow instead of following along, which he
  expected to change too. Fixed: `MonitorFixtureItem` gained
  `setTrussGroupSelected()`/`isTrussGroupSelected()` (new bool, distinct
  from the existing `m_isolated`/`isSelected()` selection-color logic in
  `paint()`) — when set, a selected fixture's outline draws in the truss's
  amber instead of the generic yellow. `updateTrussAnchorLines()` now sets
  this flag for every truss-bound fixture on every call (not just the ones
  that get a drawn tether line — a fixture sitting exactly on the centerline
  still needs its outline to follow the truss's selection even though it
  has no line to color). Deliberately narrow: only a fixture whose OWN truss
  is currently selected gets amber — solo-selecting just that fixture (the
  anchor-selection click fix from earlier) still shows plain yellow, since
  that's "just this fixture," not "the assembly." Built clean; not yet
  re-confirmed live (Branson needs to relaunch to pick up the new binary).
  **Follow-up (2026-08-17, same day):** Branson caught a real bug from a
  screenshot — the tether into a vertically-running truss was visibly a
  couple degrees off perpendicular (fine for a horizontal-running truss,
  off for a vertical one). Cause: the line's fixture-end read the icon's
  actual on-screen center while the truss-end was independently recomputed
  from stored `trussOffset` — any drift between a fixture's rendered
  position and its stored offset/cross (rounding, or hand-set data that was
  never perfectly self-consistent) tilted the line. Fixed by deriving BOTH
  endpoints from the same stored `trussOffset`/`trussCross` via the truss's
  own direction vector, so the line is perpendicular by construction
  regardless of the truss's on-screen orientation — no longer reads the
  item's rendered position at all. Also thickened per request (1.2–1.6px →
  2.4–3.0px) with a round cap. Built clean, not yet re-verified live.

- **Clicking a fixture right after clicking its truss now solo-selects the
  fixture instead of keeping the truss along for the drag — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `mousePressEvent()`. Last piece of the anchor-selection fix above: with
  that fix alone, Branson found a single-click on a lone fixture correctly
  moved just the fixture, and selecting the truss correctly moved
  truss+fixtures together — but click the truss FIRST, then click one of
  its fixtures, and both still moved together. Root cause is a Qt default,
  not a leftover bug in our logic: `QGraphicsScene`'s built-in press
  handling only clears the rest of the selection when the clicked item
  *isn't already selected* — if it is, it assumes you're grabbing the whole
  group to drag it. Since `extendSelectionToGroups()` had already pulled the
  fixture into the selection when the truss was clicked, the fixture counted
  as "already selected" by the time it was clicked next, so Qt left the
  truss selected too. Branson was offered two ways to resolve the ambiguity
  (highlight the fixtures to make the shared-selection state visible, or
  make a fixture click always break out solo) and picked solo-select, to
  keep a fixture click meaning "just this fixture" everywhere, consistent
  with the fix above. Implemented by intercepting a plain (no Shift/Ctrl)
  left-click on a `MonitorFixtureItem`: if it's selected AND its bound
  truss's `TrussItem` is also currently selected, force
  `m_scene->clearSelection()` + reselect solely the fixture *before* handing
  off to `QGraphicsView::mousePressEvent()` — so Qt's default handling then
  sees a solo, already-correct selection and just starts the single-fixture
  drag. Shift/Ctrl-click (explicit multi-select) and clicking the truss
  itself are untouched. **Not verified live** — same canvas-click/drag
  limitation as the other entries here; worth confirming at the rig:
  select truss (everything highlights) → click one of its fixtures → only
  that fixture should stay selected and move solo.

- **A truss-bound fixture can be dropped anywhere across the truss (not
  forced onto the centreline) while still touching it — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`, `snapToTruss()`
  (inside `slotFixtureMoved()`) + `slotTrussMoved()`. Follow-on to the
  anchor-selection fix right below: once Branson could drag a bound fixture
  without it dragging the truss, the next ask was that dragging it
  perpendicular to a horizontal truss ("vertically away" on screen) always
  snapped it back onto the centreline, when it should be free to land
  anywhere still touching — with the existing red-border "pull too far and
  it detaches" behavior (already working, driven by `MonitorFixtureItem`'s
  `escapeMode()`/`setEscapeMode()`, set live during the drag in
  `mouseMoveEvent()`) as the actual boundary. `snapToTruss()` used to force
  the perpendicular component to zero by projecting the drop position onto
  the truss's direction vector and discarding everything else; it now splits
  the drop into an ALONG component (still projected/grid-snapped, as before)
  and a CROSS component (perpendicular offset, preserved), clamping cross to
  the same `pxWid() * 2` threshold the escape-mode red border already uses —
  so the allowed range matches exactly where it would otherwise go red, no
  surprise snap-back at the boundary. The cross value is stored in
  `FixtureRigProps::trussCross`, a field that already existed for the
  Fixture Properties dialog's discrete Left/Centered/Right "Across truss"
  selector (`ui/src/monitor/monitor.cpp`) and was already read by
  `MonitorProperties::fixtureRigPosition()` — the authoritative derived
  position `aimsolver.cpp`/`effectinstance.cpp` use for actual pan/tilt
  aiming and effects — so this reuses existing, already-correct engine math
  rather than inventing a parallel one; the top-view canvas (which renders a
  truss-bound fixture from its raw stored XY, not from `fixtureRigPosition()`
  — see `updateFixture()`) is kept in sync by writing the same along+cross
  point into both places. `slotTrussMoved()`'s "truss carries its fixtures"
  logic was also switched from the bare `Truss::positionAt(trussOffset)`
  (centreline only) to `MonitorProperties::fixtureRigPosition()`, so a
  fixture's cross offset (and its mount-side Z nudge) now survives the truss
  itself being moved — previously any fixture with a non-zero cross offset
  would have silently re-centred the next time its truss moved, since
  `positionAt()` never included that term. Vertical trusses (towers) are
  unchanged — the user's ask was specifically about horizontal trusses, and
  a tower's radial/yoke mounting doesn't have the same "across" concept.
  **Not verified live** — same canvas-drag limitation as the other entries
  here; worth a real drag test (drop off-centre but touching → stays there;
  drop across → snaps within bounds; drop too far → red + detach; move the
  truss afterward → fixture keeps its offset) at the rig.

- **Moving a truss-bound fixture no longer drags the truss along with it —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `extendSelectionToGroups()` + new `isGroupAnchorItem()`. Third bug in this
  same area, found immediately after the drag-to-attach fix landed: once a
  fixture is bound to a truss, clicking/dragging *the fixture* also moved
  the whole truss. Root cause was `extendSelectionToGroups()` (wired to
  `QGraphicsScene::selectionChanged`) — it treats every member of a group as
  a peer, so selecting any one member (the fixture) pulled in the whole
  group's `topLevelGroup()`, truss included, and Qt's native multi-select
  drag then moved everything together. But a truss's auto-maintained group
  (`ensureTrussGroup()`/`ensurePlatformGroup()`, `anchorKind == "truss"` /
  `"platform"`) isn't a peer relationship — it's parent→child: the truss
  already carries its bound fixtures correctly when *it* moves, via
  `slotTrussMoved()`'s dedicated position-following logic, which runs
  independent of selection state and needed no changes. Fix: only extend a
  *dedicated* structural group (anchor set) when the anchor item itself —
  the truss/platform, not a rigged member — is what got selected; a manually
  built-out group (anchor cleared once it holds >1 structural item, see
  `structuralMembersOf()`) keeps the old symmetric behavior. New helper
  `isGroupAnchorItem(QGraphicsItem*, anchorKind, anchorId)` takes the anchor
  kind/id rather than the `MonitorProperties::MonitorGroup` struct directly
  — `monitorproperties.h` is only forward-declared in the header, so a
  nested-type parameter there fails to compile (confirmed by trying it
  first). **Not verified live** — same canvas-drag limitation as the entry
  below; needs a real "drag the fixture, does the truss stay put" /
  "drag the truss, do its fixtures follow" check at the rig. Also
  unaddressed: Branson's "not so far they don't touch" phrasing implies a
  bound fixture dragged far enough away should auto-detach — there's an
  existing "escape mode" detach branch in `slotFixtureMoved()` built for
  exactly this, but nothing in the codebase ever calls `setEscapeMode(true)`
  to trigger it, so it's dead code today. Left alone pending confirmation
  this is actually wanted, since it wasn't explicitly asked for as a
  separate feature.

- **Drag-to-attach a fixture onto a truss brought back, scoped correctly
  this time — SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `slotFixtureMoved()`. Direct continuation of the truss-group bug above:
  once that fix landed, Branson immediately hit the OTHER side of the same
  area — dragging an unbound fixture onto a truss did nothing at all. That
  turned out to be pre-existing, deliberate: a prior fix had removed
  auto-attach-on-drop *entirely* to kill a bug where a truss would "grab"
  any fixture whose bounding box merely overlapped it. That's also exactly
  what Branson had asked for earlier in this session (attach should trigger
  on *touching*, not require being centered on the truss) — so the right
  fix was to bring it back with tighter scope, not leave it removed.
  Re-added using `QGraphicsItem::collidesWithItem()` (precise shape overlap,
  not just a bounding-box guess) between the fixture and every `TrussItem`,
  but critically **only inside `slotFixtureMoved()`, which only runs once
  per completed drag (on drop)** — never per mouse-move — so passing a
  fixture over a truss en route somewhere else can't trigger it the way the
  original bug did. Mirrors the already-working, already-accepted "auto
  deck-mount onto a platform" pattern right below it in the same function,
  just for trusses. The explicit right-click "Attach to Truss…" and
  Layers-tree-drag paths are untouched and still work as a fallback for
  precise placement. **Not verified live** — this environment has no way to
  synthesize a real click-drag-release sequence on a `QGraphicsView` canvas
  (no `cliclick` or equivalent installed), so this shipped on code review
  plus direct parity with the platform auto-mount logic it's modeled on,
  not a screenshot. Worth a real drag-and-drop check at the rig before
  trusting it.

- **Build Focus removed; "clicking a fixture selects the truss too" —
  correctly root-caused this time (a real data bug, not Build Focus) and
  fixed — SHIPPED (2026-08-17).** Follow-on to the entry below, which
  turned out to have the WRONG diagnosis for a related-looking but distinct
  symptom Branson hit next.
  - **Build Focus removed entirely**
    (`ui/src/monitor/{monitor,monitorgraphicsview,monitorfixtureitem}.{h,cpp}`),
    per Branson's challenge: it duplicated what a locked Layer already does
    (both clear `ItemIsSelectable`/`ItemIsMovable`, which is what actually
    causes the click-through). `MonitorGraphicsView::setBuildFocus()`/
    `buildFocus()`/`m_buildFocus`, `Monitor::m_buildAction`, the footer
    "Focus:" combo, and the `updateModeIndicator()` BUILD branch are all
    gone. The one part of Build Focus worth keeping — Branson explicitly
    asked for it — was the faint "ghosted" visual as a general indicator of
    "what's currently clickable." `MonitorFixtureItem::setGhosted()` is kept
    but now driven by the fixture's REAL state
    (`refreshItemLayerState()`: `setGhosted(lyr.locked)`) instead of a
    separate mode, so a locked fixture is visibly faint everywhere, all the
    time, not just in one special mode.
  - **The actual "truss steals the selection" bug**, found only after two
    wrong turns (z-order/click-target-size, then Build Focus) — this
    session's Show Manager investigation habit of demanding hard evidence
    over plausible-sounding theories paid off: Branson's screenshots proved
    single-click selection genuinely pulled in the truss, and a live
    reproduction (`surfacetesting.qxw.before-padfix`, temporary debug
    logging in `MonitorProperties::setFixtureGroup()` and
    `extendSelectionToGroups()`) traced it to real saved data — 3 fresh,
    never-truss-bound fixtures (`FixtureRig Truss="4294967295"`, i.e.
    `Truss::invalidId()`) carrying the SAME `GroupId` as an existing truss,
    ~0.58 m away with no position link at all. Root cause:
    `detachFixtureFromTruss()` and the drag-escape auto-detach path in
    `slotFixtureMoved()` (`monitorgraphicsview.cpp`) both clear a fixture's
    `trussId` on unbind but — per an explicit, deliberate old comment,
    *"detaching is about the rig binding, not the spatial grouping"* —
    always left it in the truss's auto-created group. Once a fixture had
    ever been bound-then-unbound from ANY truss, it would select/move
    together with that truss forever after, with zero visual or positional
    relationship. Fixed with a new symmetric helper,
    `MonitorGraphicsView::leaveDedicatedTrussGroup(fid, trussId)`: on
    detach, if the fixture's current group is still the truss's own
    dedicated auto-group (`MonitorGroup.anchorKind == "truss"` and
    `anchorId == trussId` — i.e. nothing the user built out further
    manually), take it back out. Called from both detach paths. Also
    hand-fixed the ALREADY-corrupted stale data in
    `surfacetesting.qxw.before-padfix` (stripped the stray `GroupId="1"`
    from the 3 affected `FxItem` entries) — the code fix only prevents this
    going forward, it doesn't retroactively repair a workspace already
    carrying the orphaned membership.
  - Still open, deferred at Branson's request: a general "Add {thing} to
    {what's under the cursor}, or just 'Add {thing} here' with no attach if
    right-clicking empty space / a locked item" convention across every
    attachable type (truss/platform/pipe/stand/tower) — worth doing, but a
    separate pass from this bug fix.
  - Verified: full engine+UI build clean. NOT re-verified live in-app after
    the final fix (this session's synthetic-click/menu-popup limitations
    made reliably reproducing the exact add-fixture-and-click sequence
    impractical) — confirmed instead via the saved-XML evidence trail above
    plus direct code review of both detach call sites. Worth a real
    at-the-rig check: detach a truss-bound fixture, then single-click it —
    the truss should no longer come along.

- **"Clicking a fixture on a truss selects the truss" — root-caused and
  fixed the real problem (a discoverability gap, not a hit-test bug) —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitor.{h,cpp}`. Branson's report
  led down a wrong first path (I initially suspected z-order/click-target
  size in `trussitem.cpp`/`monitorfixtureitem.cpp` — both checked out fine:
  fixtures are explicitly `zValue(2)` above trusses' `zValue(-0.5)`, and
  `MonitorFixtureItem::shape()` tightly hugs the fixture body). Branson's
  follow-up ("I can click anywhere in the blue box, every time") ruled that
  out and pointed at the real mechanism: **Build focus**
  (`MonitorGraphicsView::setBuildFocus()`), a pre-existing checkable mode
  that intentionally ghosts fixtures (`setGhosted(true)`, `ItemIsSelectable`
  cleared, `setMovable(false)`) so structural items become the click target
  for laying out trusses/platforms — a Qt item with neither flag set
  ignores its own mouse-press, which falls through to whatever's
  underneath. Working exactly as designed, but **undiscoverable**: the
  toggle lived only inside the "More" popup menu, and its own intended
  on-screen indicator — `m_modeLabel`, a "prominent current-mode chip" —
  was declared in the header and fully wired up in
  `updateModeIndicator()`, but **never actually constructed or added to any
  layout**, so it silently did nothing. Fixed by making the toggle itself
  visible: initially tried a dedicated toolbar button next to "Edit Plot"
  plus a full-width colored banner above the canvas (`m_modeLabel`,
  finally instantiated) — Branson asked for a lighter touch instead, so
  landed as a plain checkable toolbar button in the **footer** bar, right
  before Overlay/View (`initGraphicsFooter()`), matching the style of the
  other view-mode selectors already there (Grid/Snap/Rulers/Labels). The
  banner and the dedicated top-toolbar button were both removed again per
  that feedback — `m_modeLabel` goes back to being unconstructed (a
  no-op, matching its original pre-existing state) rather than half-used.
  Verified live at each step via rebuild + relaunch + screenshot, including
  actually toggling Build focus on to confirm fixtures visibly ghost and
  the footer button state reflects it.

- **Lighting Studio's tab icon changed from `:/monitor.png` (a generic
  system-monitor/pulse icon, a poor fit) to `:/grid.png` — SHIPPED
  (2026-08-17).** `ui/src/app.cpp` (both the tab icon and
  `m_controlMonitorAction`'s icon, kept in sync). Chose from the PNG set
  already compiled into this target rather than the fork's parallel SVG set
  (`resources/icons/svg/`, 157 icons including a much more literal
  `2dview.svg`) — that SVG set turned out to only be wired into
  `qmlui`'s CMakeLists (off for this fork per CLAUDE.md), not
  `ui/src`'s; `Qt::Svg` isn't even linked into the `qlcplusui` target, so
  referencing an `.svg` resource there would've silently rendered a blank
  icon. Using it for real would mean adding the `Svg` component to
  `ui/src/CMakeLists.txt`'s `target_link_libraries` and a CMake
  *reconfigure* (not just a rebuild) — real but avoidable extra risk for an
  icon swap, so stuck to the zero-risk PNG option. `grid.png` was picked
  after visually comparing several PNG candidates (`target.png`,
  `position.png`, `tabview.png`, `global.png`, `diptool.png`, `square.png`)
  — it's the one that actually reads as "2D plotted layout," matching what
  the tab shows (a grid-ruled canvas with Grid/Subdiv/Snap controls). Minor
  known tradeoff: also used for Shows' Snap-to-Grid action, but that's a
  toolbar button in a different tab, not the same visual context, so low
  real confusion risk. If a closer-fitting purpose-built icon matters more
  than the CMake risk, the SVG route is the way to get one — flagging it
  rather than deciding unilaterally.

- **Main tab strip reordered to follow the build workflow — SHIPPED
  (2026-08-17).** `App::init()`'s `addTab()` sequence
  (`ui/src/app.cpp`): was Hardware/Functions/Programming/Shows/Virtual
  Console/Simple Desk/Inputs/Outputs/Lighting Studio (construction-history
  order); now **Hardware → Inputs/Outputs → Lighting Studio → Functions →
  Programming → Shows → Virtual Console → Simple Desk** — rig/setup, then
  build content, then run it, per Branson's requested grouping. Purely a
  reordering of the existing `addTab()` calls (all `setActiveWindow()`/
  `indexOf()`-based lookups elsewhere are by class name or widget pointer,
  never a hardcoded index, so nothing else needed to change); confirmed via
  a live relaunch that all 8 tabs still construct without error in the new
  order and the tab strip reads correctly left to right.

- **Lighting Studio is now a real tab; window-title/detach correctness
  fixes; Shows-tab follow-up fixes — SHIPPED (2026-08-17).**
  - **Lighting Studio (`Monitor`) converted from a lazily-created standalone
    `Qt::Window` into a permanent tab**, constructed once in `App::init()`
    alongside every other tab (`ui/src/app.cpp`, `ui/src/monitor/monitor.{h,cpp}`).
    Prompted by Branson noticing it was the one surface that didn't pick up
    the app's title-bar conventions, and pushing back on my initial "keep it
    floating" recommendation — correctly: since ANY tab can already be
    double-click-detached into its own window via the existing
    `App::slotDetachContext`/`DetachedContext` machinery, there was no real
    functional loss from making it a tab, only redundant special-casing.
    - `Monitor`'s constructor moved from `protected` to `public` (with
      `Q_ASSERT(s_instance == NULL); s_instance = this;` moved into the
      constructor body), matching the exact convention every other
      tab-hosted singleton already uses (`FunctionManager`, `ShowManager`,
      etc.) — no longer a special case.
    - `Monitor::createAndShow()` (6 call sites, all left unchanged) no
      longer constructs anything; it now walks up the parent chain to find
      either the `QTabWidget` (switch to the tab) or a `QMainWindow`
      (currently detached — raise that window instead). All 6 existing
      callers keep working with zero call-site changes.
    - Removed `WA_DeleteOnClose` and the old create-time geometry
      restore/first-run centering logic (moot — the tab is never destroyed
      until app shutdown, exactly like its siblings). Removed the
      now-dead `SETTINGS_GEOMETRY` write in `saveSettings()`.
    - Workspace XML: the old per-Monitor `MonitorWindow open="1"/geometry`
      `AppState` element (its own separate persistence, predating the
      generic `DetachedWindow` mechanism) is retired on the save side —
      Monitor detaching now falls through the same generic `DetachedWindow`
      path as any other tab, since its className is just `"Monitor"`. Old
      saved files with a legacy `MonitorWindow` element are read and
      silently ignored (no crash, no bogus popup) rather than crashing or
      double-opening.
    - Verified live: screenshot confirms no floating window appears at
      startup anymore, "Lighting Studio" renders correctly as a selected
      tab (toolbar/layers panel/grid controls all intact), and the title
      bar reads `qlcconsole - <file> - Lighting Studio` — consistent with
      every other tab, which was the actual ask.
  - **Removed the resulting duplicate View-menu entry**: the tab-jump loop
    now provides "Lighting Studio" (Ctrl+Shift+8) automatically, so the old
    dedicated `m_controlMonitorAction` menu item was pulled from the View
    menu (`ui/src/app.cpp`) to avoid listing it twice. The action and its
    Ctrl+Shift+M shortcut still exist and still work
    (`slotControlMonitor()` → `Monitor::createAndShow()`), just not
    re-added to the menu.
  - **Fixed: main window title didn't update when a tab was double-click-
    detached, and detaching left a dangling tab-bar entry.**
    `App::slotDetachContext()` never called `m_tab->removeTab(index)` (only
    the reattach path's matching `insertTab()` existed) — added it, plus an
    explicit `updateWindowTitle()` call after both detach and reattach for
    safety. Also fixed `m_tabOriginals`' positional-indexing fragility: it's
    built once at startup in construction order and is never reordered when
    tabs are removed/reinserted, so a title/label lookup by index would
    silently point at the wrong tab after any detach. Replaced with
    `QTabBar::tabData()` (set once per tab in the `addTab` lambda, and
    re-set after `insertTab()` on reattach) — this travels correctly with
    each tab through remove/insert, unlike a parallel array indexed by
    position. Same fix applied to the workspace-file `DetachedWindow`
    XML-restore loader, which had the identical missing-`removeTab()` bug.
  - **Shows tab: Follow MTC toggle made authoritative and discoverable
    from within the tab itself** (`ui/src/showmanager/showmanager.{h,cpp}`).
    Prompted by Branson asking where Timer-vs-MTC is actually selected —
    turned out my round-2/3 redesign showed a read-only "● FOLLOWING MTC"
    indicator with no way to toggle it from the Shows tab at all (the only
    control was the Control-menu / footer-MTC-chip toggle, both
    `App`-level). Replaced the static label with `m_followMtcButton`, a
    `QToolButton` bound via `setDefaultAction(m_followMtcAction)` — the
    *same* `QAction` already driving the Control menu and footer chip, so
    all three stay in sync for free. Placed in the always-visible part of
    the transport cluster (not gated by mode) so it's reachable to arm
    *or* disarm follow from either state.
    - **Regression caught and fixed**: `updateMultiTrackView()`'s
      `blockSignals()`-wrapped sync of `m_followMtcAction`'s checked state
      (run on every show switch) updates the checked flag correctly, but —
      because signals are blocked on purpose — never runs
      `slotFollowMtcToggled()`, which is where the button's *text* ("Follow
      MTC (off)" vs "● FOLLOWING MTC") got set. Result: button showed stale
      text out of sync with the actually-correct widget visibility. Fixed
      by moving the text update into the shared `updateTransportVisibility()`
      helper (already called from both paths for the widget-swap fix
      earlier this session), so text and visibility can no longer drift
      apart again.
  - **Three Shows-tab track-header bugs fixed** (`ui/src/showmanager/trackitem.cpp`),
    all found and root-caused by Branson during review:
    1. *Dimmer/intensity bar drawn over the green active-indicator bar*:
       `m_intensityRegion` started at x=8, overlapping the active-indicator
       rect at x:1-11. Moved to start at x=14 (same right edge as before).
    2. *Double-clicking Mute/Solo/Lock opened the track-rename dialog*:
       `mouseDoubleClickEvent()` correctly excluded the name area but still
       fell through to `emit itemDoubleClicked()` for the button row, which
       `ShowManager::slotTrackDoubleClicked()` wires straight to a rename
       `QInputDialog`. Now returns early or the M/S/L/intensity regions
       without emitting anything — a double-click there is just two
       ordinary toggle clicks (already handled by `mousePressEvent`).
    3. *Intensity bar couldn't be dragged, only click-set*: `mousePressEvent()`
       calls the base `QGraphicsItem::mousePressEvent()` first, whose
       default implementation ignores the event for an item that's neither
       movable nor selectable (neither flag is set on `TrackItem`) — an
       ignored press means the scene never grabs the mouse for this item,
       so `mouseMoveEvent()` (which already had correct intensity-drag
       logic) was simply never delivered. Fixed with an explicit
       `event->accept()` right after the base-class call.
  - Everything above verified live via rebuild + relaunch + screenshot
    (title bar, tab strip, Follow MTC button text/visibility together) —
    not code-review-only. The three `trackitem.cpp` mouse-handling fixes
    are code-review-verified only (this environment can't synthesize a
    real double-click or a mouse-drag, the same standing limitation noted
    elsewhere in this file) — worth a real check next time at the rig.

- **Show Editor toolbar redesign (Show/Edit split, centered context-aware
  transport, window-title fix) — SHIPPED (2026-08-17).**
  `ui/src/showmanager/showmanager.{h,cpp}`, `ui/src/app.{h,cpp}`. Follow-on
  to the round-2 toolbar work below, going further specifically on the Show
  Editor per Branson's detailed spec:
  - **"Show ▾" split from "Edit ▾".** The single "Add" dropdown from round 2
    got split in two: **Show** (New Show / Rename show / Delete show — which
    show container you're working on) and **Edit** (Add Track/Sequence/
    Audio/Video + Undo/Copy/Paste/Delete/Change Color/Lock/Timings —
    everything that touches the open show's contents). Matches the user's
    explicit split ("show: add/rename/delete" vs. "edit functions can go
    under Edit").
  - **Snap to Grid moved to a new bottom toolbar row** (`m_bottomToolbar`),
    mirroring the 2D view's own Grid/Subdiv/Snap row at the bottom of its
    canvas.
  - **Play/Stop merged into one button.** `m_playStopButton`
    (`QToolButton::MenuButtonPopup` + `setDefaultAction(m_playAction)`):
    primary click keeps the existing Play/Pause toggle behavior unchanged
    (pause-in-place while running, resume on click, existing MTC-follow
    safety no-op); Stop (full stop + rewind) moved to the button's dropdown
    rather than staying a separate always-present toolbar button. Confirmed
    with Branson before implementing (AskUserQuestion) — Pause behavior
    stays, nothing was dropped.
  - **Transport cluster centered, DAW-style, and context-aware.** Flanking
    expanding-stretch spacers center the whole cluster regardless of what's
    docked left/right (`leftStretch`/`rightStretch`, matching how Logic
    centers its transport). Time position + Length stay always visible
    (position readout doubles as "show position" in either mode). Exactly
    one of two widgets shows depending on the show's actual
    `timecodeFollow()` state: **manual mode** — Play/Stop, Time division,
    BPM (`m_transportManualWidget`); **MTC mode** — a read-only "●
    FOLLOWING MTC" indicator + the TC-offset config button
    (`m_transportMtcWidget`), since there's nothing for local playback
    controls to do while an external code is driving the timeline.
  - **Real bug hit and fixed along the way**: the manual/MTC widgets were
    both showing simultaneously. Root cause #1 — `updateMultiTrackView()`
    (runs whenever the active show changes) syncs `m_followMtcAction`'s
    checked state via `blockSignals()` so switching shows doesn't
    re-arm/disarm anything, but that silently bypassed the
    `toggled()`-driven `slotFollowMtcToggled()` where the visibility swap
    lived — fixed by factoring the swap into `updateTransportVisibility()`
    and calling it from both places. Root cause #2, found only after
    root cause #1's fix still didn't work: `QToolBar::addWidget()`
    implicitly wraps a widget in a `QWidgetAction`; a later toolbar layout
    recompute (`applyToolbarLabelMode()`'s `setToolButtonStyle()` call)
    re-syncs the widget's visibility **from that wrapping action**, which
    was never touched — silently undoing a plain `widget->setVisible()`
    call. Fixed by toggling visibility through the `QAction*` that
    `addWidget()` returns (`m_transportManualAction`/`m_transportMtcAction`)
    instead of the widget directly. Root-caused via a scratch debug build
    (temporary `QFile`/`QTextStream` logging, since `qDebug()` output
    wasn't reaching any log this environment could see) rather than guessing
    — removed before shipping.
  - **"Is Length superfluous?" — answered, kept.** Not superfluous: it
    exists specifically because the timeline's own draggable end-handle can
    sit off-screen on a long show, so `m_lengthButton` stays as an
    always-reachable way to set/inspect it regardless of transport source.
  - **Window title now always shows the showfile name and the active tab**
    (`App::updateWindowTitle()`, replacing the title-building half of
    `slotDocModified()`; new `App::slotTabChanged()` wired to
    `m_tab`'s `currentChanged`). Format:
    `qlcconsole - <file or "New Workspace">[ *] - <Tab Name>`, confirmed
    live via `AXRaise` window-name checks (title flips from "…-
    Inputs/Outputs" to "…- Shows" the instant the tab changes). Uses
    `m_tabOriginals` rather than `m_tab->tabText()` for the tab name since
    the latter goes blank under Icons-Only tab-label mode (the toggle added
    in round 2) — fixed the same latent bug in `slotDetachContext()`'s own
    `tabLabel` capture while touching this code. Detached windows
    (`DetachedContext`, previously titleless) now get a one-time title at
    detach time (`<app> - <file> - <tab label>`) — not live-updated
    afterward if the doc's modified state changes while detached, which
    would need tracking every currently-open detached window; scoped out
    as more than what was asked.
  - Verified live in the running app throughout (screenshots + `AXRaise`
    window-name checks), including catching and fixing the visibility bug
    before calling this done rather than shipping on code-review alone.

- **Toolbar-consolidation, round 2: VC's duplicate mode toggle, a VC "Edit"
  dropdown, Shows tab's Add cluster, and a View-menu toolbar-style switch —
  SHIPPED (2026-08-17).** Direct follow-on to the round below, from a fresh
  pass over what still looked cluttered/inconsistent:
  - **VC's redundant Run/Stop button removed**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`). VC had its own
    always-visible checkable "Run"/"Stop" `QToolButton` (top-right,
    `m_runButton`) toggling `Doc::mode()` directly, *in addition to* the
    app-global "Operate"/"Design" toggle (`App::m_modeToggleAction`, on
    App's own top bar, confirmed always-visible across every tab/mode via
    screenshot). Both drove the exact same single piece of state. Confirmed
    safe to remove the VC-local one: `App::slotModeChanged` is wired to
    `Doc::modeChanged` (`app.cpp:688`) and already keeps the global button's
    icon/text/tooltip in sync regardless of what triggered the mode change.
    The VC-local button's original justification (VC's *own* toolbar
    `m_toolbar->hide()`s itself in Operate mode, per `disableEdit()`,
    `virtualconsole.cpp:~1890` — "there's nothing usable there in operate
    mode") is moot since the global button lives outside that toolbar and
    was never affected by it. Removed the button, its `runBar` row, its
    `slotModeChanged()` sync block, and the member/init-list entries.
  - **VC "Edit" dropdown**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`): the toolbar's twelve
    remaining per-widget actions (Cut/Copy/Paste/Delete/Widget
    Properties/Rename Widget/Bring to front/Send to back/Background
    Color/Background Image/Font Colour/Font) collapsed into one
    `m_editButton` ("Edit ▾", `:/edit.png`), right after the "Add" button.
    Deliberately a *fresh* small `QMenu` built locally in `initMenuBar()`
    (not a reuse of `m_editMenu`, unlike how Add reuses `m_addMenu`) — using
    `m_editMenu` would have also dragged in the dynamically-appended custom
    "Add" submenu (`updateCustomMenu()`), duplicating what the new Add
    button already offers. Entries stay individually enabled/disabled by
    the *existing* `VirtualConsole::updateActions()` selection logic,
    unchanged — with nothing selected, Paste/Background/Font remain
    clickable (they legitimately target the canvas) while
    Cut/Copy/Delete/Properties/Rename/stacking show up grayed rather than
    vanishing, matching the user's "only edit with sub selections" ask
    without needing new gating code. Keyboard shortcuts (Ctrl+X/C/V,
    Delete, Ctrl+E) are untouched — they live on the `QAction` objects
    themselves via `setShortcut()`, independent of which menu/toolbar the
    action is currently displayed in, so moving these into a dropdown had
    zero effect on them.
  - **Shows tab's add-element icons folded into the same paradigm**
    (`ui/src/showmanager/showmanager.{h,cpp}`): New Show + Add Track/New
    Sequence/New Audio/New Video (five separate toolbar buttons) collapsed
    into one `m_addButton` ("Add ▾", `:/edit_add.png`), placed first —
    same `QAction`s, same shortcuts (Ctrl+H/N/E/A/D). `ShowManager` also
    gained the `applyToolbarLabelMode()` method every other manager already
    had (it was missing entirely — the toolbar was stuck on Qt's default
    icons-only regardless of the app setting).
  - **Programming tab's "Add" button retrofitted onto the same shared
    setting** (`ui/src/programmingmanager.{h,cpp}`): previously hardcoded
    to `Qt::ToolButtonTextBesideIcon`; now has its own
    `applyToolbarLabelMode()` reading `workspace/tabLabelMode` like the
    others (found via `App::findChild<ProgrammingManager*>()`, the same
    idiom `App` already uses elsewhere for this tab, since Programming has
    no singleton `instance()` of its own).
  - **New View → Toolbar Style submenu** (`ui/src/app.cpp`,
    `initMenuBar()`/View menu, right after the existing Theme submenu):
    Icons & Text / Icons Only / Text Only, a checkable `QActionGroup`
    calling `App::setTabLabelMode()`. This setting (`workspace/tabLabelMode`,
    `App::m_tabLabelMode`) and its full propagation
    (`App::applyTabLabelMode()` → tab strip + main toolbar + every
    manager's own toolbar) already existed and was already being read
    correctly by every manager — there was simply **no UI control to change
    it**, only a raw QSettings value to hand-edit. `App::applyTabLabelMode()`
    extended to also call `ShowManager::instance()->applyToolbarLabelMode()`
    and the `ProgrammingManager` `findChild` lookup, so the new menu now
    covers all seven tabs' toolbars in one switch.
  - Verified live in the running app: VC toolbar screenshot confirmed
    "Add | Edit | VC Fixture Widget Wizard | Virtual Console Settings" with
    no Run button; Shows tab screenshot confirmed "Add | full show ▾ |
    Rename show | Delete show | Undo | Copy | Paste | Delete | …"; View
    menu screenshot confirmed the Toolbar Style submenu with a checkmark on
    the current ("Text Only") setting, and clicking "Icons & Text" updated
    the global toolbar, the Shows toolbar, *and* the bottom tab strip
    simultaneously in the same screenshot — confirming the single shared
    setting really does propagate everywhere. Reverted the live setting
    back to "Text Only" afterward so this testing didn't leave the user's
    persisted preference changed.

- **Function Manager / Virtual Console: same "Add" dropdown consolidation,
  plus a selection-aware VC context menu — SHIPPED (2026-08-17).**
  Follow-on to the Programming tab change below, extended to the other two
  tabs Branson flagged as still cluttered:
  - **Function Manager** (`ui/src/functionmanager.{h,cpp}`): the toolbar's
    nine individual "New scene/chaser/sequence/EFX/collection/RGB Matrix/
    script/audio/video" buttons + Folder collapsed into one `m_addButton`
    ("Add ▾", `:/edit_add.png`, `QToolButton` + popup `QMenu`) built from the
    *same* `QAction*` objects (`initToolbar()`), so shortcuts (Ctrl+1..9),
    slots, and the tree's own right-click menu (which already reused these
    actions) are untouched — purely a toolbar-layout change. Placed first
    (leftmost). `applyToolbarLabelMode()` extended to also style
    `m_addButton` (a toolbar-added `QToolButton` doesn't auto-follow
    `QToolBar::setToolButtonStyle()` the way `addAction()`-created buttons
    do).
  - **Virtual Console** (`ui/src/virtualconsole/virtualconsole.{h,cpp}`):
    same pattern — the toolbar's 14 individual "New Button/Button Matrix/
    Slider/Slider Matrix/Knob/Speed Dial/XY pad/Cue list/Frame/Solo frame/
    Label/Audio Triggers/Clock/Animation" buttons collapsed into one
    `m_addButton`, reusing the *already-built* `m_addMenu` (`initMenuBar()`)
    as-is — same object also feeds the menu-bar's own "&Add" entry and
    `VCFrame::customMenu()`'s right-click submenu, so no new action list was
    authored. Bonus: `m_addMenu` has 16 entries (also Programmer Frame /
    Show control, previously toolbar-omitted), so the dropdown now exposes
    two actions the old toolbar didn't. Placed first.
  - **VC empty-canvas right-click menu made selection-aware**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`,
    `ui/src/virtualconsole/vcwidget.cpp`): right-clicking empty VC canvas
    (nothing selected) used to always show the full `m_editMenu` — Cut/Copy/
    Delete/Rename/Widget Properties included, always enabled=false and
    grayed out but still visually present, with the "Add" submenu buried at
    the very bottom after Background/Foreground/Font/Frame/Stacking
    submenus. New `VirtualConsole::buildEmptyCanvasMenu()` builds a small
    purpose-built menu instead for exactly this case (called from
    `VCWidget::invokeMenu()`, gated on `vc->selectedWidgets().isEmpty()`):
    **Add first**, then Paste (if the clipboard has something), then
    Background/Foreground/Font (these legitimately apply to the bottom
    frame itself with nothing selected — confirmed via
    `VirtualConsole::updateActions()`'s existing enable/disable logic, which
    already special-cased them). Cut/Copy/Delete/Rename/Properties/Frame/
    Stacking are omitted entirely rather than shown disabled — exactly the
    set `updateActions()` already flags as meaningless with an empty
    selection, just now *absent* instead of merely grayed out. The
    when-something-**is**-selected right-click path is untouched (still
    `editMenu()`, unchanged) — this was scoped to the specific "empty
    space" complaint, not a redesign of the selected-widget menu.
    `m_bgMenu`/`m_fgMenu`/`m_fontMenu` (previously `initMenuBar()` locals)
    were promoted to members so both menus can reference the same QMenu
    objects. Deliberately did NOT touch the shared Cut/Copy/Delete/Rename/
    Properties `QAction`s' `setVisible()` — they're also on the toolbar, and
    hiding a shared `QAction` hides it everywhere it's added, which would've
    made toolbar buttons blink in and out during every right-click.
  - Both toolbar changes confirmed via screenshot in the running app (Add
    button present, correctly leftmost, old buttons gone). The VC
    empty-canvas menu content itself is code-review-verified only, not
    screenshotted — same synthetic-input gap as elsewhere in this file:
    there's no `cliclick`-equivalent for a real right-click in this
    environment, and the accessibility tree doesn't expose the VC canvas
    granularly enough to fake one via `AXShowMenu`. Worth a real look next
    time at the rig/laptop.

- **Programming tab: tab-local "Add" menu for New Scene/Chaser/… — SHIPPED
  (2026-08-17).** Branson's ask: find a middle ground between "no icons
  anywhere" and Function Manager's full icon toolbar, and figure out where
  per-function "Add" actions should live given the app has a genuinely
  global menu bar (File/View/Control/Help — confirmed scoped correctly,
  no change needed) plus tabs that get double-click-detached into their own
  bare `QMainWindow` (`App::slotDetachContext`, `app.cpp:1985-2003`) with no
  menu bar of their own. Landed as a small `QToolButton` ("Add ▾",
  `:/edit_add.png`) next to the func-tree filter box in
  `ProgrammingManager`'s nav panel (`ui/src/programmingmanager.cpp`,
  constructor, right after `m_funcTree` is built) — popup `QMenu` with one
  icon'd entry per creatable type (`:/scene.png`/`:/chaser.png`/
  `:/collection.png`/`:/efx.png`/`:/rgbmatrix.png`/`:/show.png`/
  `:/folder.png`, same icons Function Manager already uses), replacing the
  redundant idea of spelled-out "New Scene"/"New Chaser" buttons that just
  duplicated what right-click already offered. Creation logic extracted
  into one shared `ProgrammingManager::addNewFunction(Function::Type,
  const QString &folder)` (`programmingmanager.{h,cpp}`) so the toolbar menu
  and the func-tree's right-click menu (`slotFuncTreeMenu`) call the exact
  same path — no duplicated create/name/select/open logic. The right-click
  menu's own "New …" entries also picked up the same icons while touching
  this code, so the two menus now look and behave identically. Architecture
  point that drove the placement: this had to be a widget owned by
  `ProgrammingManager` itself (not a second app-level menu bar) — only
  widget-owned UI survives `setCentralWidget(context)` when a tab detaches;
  a second `QMenuBar` living on `App` would not follow the tab out.
  Confirmed via the existing Function Manager toolbar, which already proves
  the pattern (it's a child widget in the manager's own layout, so it
  already survives detach today). Built clean
  (`cmake --build build -j --target qlcplusui`, then the full app build);
  screenshotted in the running app and the "+ Add" button is present and
  correctly placed next to the filter box. *Menu contents not
  click-verified in-app* — same synthetic-input gap noted elsewhere in this
  file: `System Events` can invoke the button's `AXPress` (confirms the
  button exists/is enabled) but the resulting `QMenu` popup doesn't render
  for a screenshot to capture, so the actual dropdown items were verified by
  code review, not an eyeballed screenshot. Worth a real look next time at
  the rig/laptop. Not yet extended to other tabs (Function Manager already
  has its own working toolbar; nothing else currently has bare text "New …"
  actions to consolidate) — revisit if that changes.

- **"Look" as the assembly unit (Scene/Collection rethink) — BOTH SLICES
  RESOLVED (2026-08-19)**. Branson shower-thought, worked all the way
  through. Deliberately did NOT rename Scene/Collection (breaks traditional
  QLC users) — instead made the fork's **Look** first-class as two
  independent slices (per the show-lifecycle doc: (1) mainly serves
  Construction, (2) mainly serves Production):
  - **Slice 1 — explicit fixture scope, SHIPPED.** `Scene` gained a
    `LookScope` (`ScopeUnset` / `ScopeWholeStage` / `ScopeGroup` + a
    `FixtureGroup` id), a dedicated typed field following the existing
    `PaletteFade` precedent rather than a generic tag bag (no such bag
    exists on `Function`/`Scene`) — `engine/src/scene.{h,cpp}`:
    `setLookScope()`/`lookScope()`/`lookScopeGroupId()`, XML round-trip as
    `<Function>` attributes (absent = unset, old workspaces unaffected),
    carried through `copyFrom()`. Deliberately a 3-state enum, not "no
    group = whole stage" — unset and explicitly-whole-stage need to stay
    distinguishable. Unit-tested (`engine/test/scene`,
    `Scene_Test::lookScope()`, 21/21 passing). UI: a **"Scope:"** combo in
    `SceneGroupLooks` (the Looks editor, embedded in both the classic Scene
    Editor and the Programming tab canvas) — `ui/src/
    scenegrouplooks.{h,cpp}`, next to the Targets panel, populated from
    every `FixtureGroup` in the doc, kept separate from Targets (declared
    *intent* vs. what's actually painted). Purely organisational metadata —
    doesn't affect playback. *Not yet click-verified in-app* — this
    environment's synthetic-click automation doesn't register on tree/list
    rows (menu-item clicks work, raw clicks don't; same gap noted earlier
    for headful automation), so this shipped on code review + the engine
    test, not an eyeballed screenshot. Worth a real look next time at the
    rig/laptop.
  - **Slice 2 — palette/fixture state as the BASE that effects/RGBScripts
    consume — DONE, by finding + decision, not new code.**
    `EffectInstance::buildPalettesObject()`
    (`engine/src/effectinstance.cpp:894-1001`) already feeds a look's
    painted COLOUR into every running effect every tick
    (`palettes.look.colors`/`palettes.look.dimmer`, read by e.g.
    `resources/rgbscripts/lines.js` via `RGB_HOST_WRAPPER`), already
    falling back to the look's full painted base colour when nothing is
    explicitly nested after the effect (line 953) — "effects respect the
    look's master Dimmer" (shipped earlier) is the same mechanism for
    intensity. **Decided**: this is the correct and complete scope for
    "state as base" — colour/intensity are it, deliberately, not a partial
    build. Everything else a script exposes (`lines.js`'s
    `linesMovement`/`linesType`/`linesPattern`/`linesDistribution`, and the
    equivalent `algo.properties` across the other
    `resources/rgbscripts/*.js` files) stays 100% owned by the
    effect/script itself (`ui/src/lookeditor.cpp`'s properties dialog),
    **on purpose**: colour is already an *external input* in stock QLC+'s
    own RGBScript API (`rgbMap()` scripts are written expecting a colour
    handed in — upstream RGBMatrix always worked this way; deriving it from
    a Look is just substituting *where* that input comes from).
    `algo.properties` are declared and owned *by the script* as its own
    configuration contract — nothing upstream expects those externally
    driven, and forcing it would invent behavior with no basis in how stock
    scripts are authored, breaking compatibility/portability of scripts
    brought in from upstream QLC+.
  - **Terminology settled** (for consistent discussion going forward): see
    the vocabulary table worked out this session — Scene (engine
    primitive) vs. Look (a Scene with Palettes attached, built via the
    Programming tab) vs. Target (what a Look actually paints) vs. Scope
    (what a Look is declared *for*) vs. Palette/Base/Effect/effect-scoped
    palette. Audited the UI against it: no tab-level renames needed —
    "Scene Editor" and the "Scene" type-folder in Function Manager
    correctly refer to the engine primitive, not the workflow, and
    shouldn't become "Look." One real drift found and fixed:
    `SceneGroupLooks`' Targets label said "Fixtures in Scene" in the UI
    while the header's own doc-comment already called it "Targets"
    (`ui/src/scenegrouplooks.h:124`) — code and label now agree
    (`ui/src/scenegrouplooks.cpp`: the label text, its live count update,
    and the intro paragraph's "fixtures in this scene"/"scene fixtures"
    phrasing all read "Targets" now).

- **Release-gate scoping, phases 1+2, + all 3 surfaced failures fixed
  (2026-08-17, BUILT)** — first concrete work off `SHOW_LIFECYCLE_DESIGN.md`'s
  "good gates" thread.
  - **Phase 1 — the macOS `make check` gate was silently broken**, and my
    first fix attempt misdiagnosed *how* it's invoked. There are actually
    **two** `unittest.sh` files: a root-level staging wrapper (already
    correct, already copies every `test.sh` + needed resources into `build/`
    and `cd`s there) that then runs the *copy* of `platforms/linux/
    unittest.sh` it just placed in the build dir. The real, narrower bugs
    were only in `platforms/linux/unittest.sh`: `RUN_UI_TESTS` never got set
    to `1` on darwin at all (`ui/test/*` was unconditionally skipped), and
    the UI-test loop bypassed each test's own `test.sh` (which already knew
    how to resolve a macOS `.app`-bundled QTest binary) in favour of a bare
    `./${test}_test` that doesn't exist for bundle-style tests. Fixed both,
    keeping the script's existing plain-relative-path assumptions intact
    (an earlier pass added `$2`/build-dir-prefixing logic on a wrong model
    of the invocation chain — reverted). Also filled a structural gap the
    now-working gate immediately hit: `engine/test/markplanner` had no
    `test.sh` at all (a real test, just missing its runner).
  - **Phase 2 — model-layer add/remove coverage.** `MonitorProperties` had
    zero test coverage for `addPipe/removePipe` (Boom/Bar/Electric),
    `addStand/removeStand`, `addTower/removeTower`,
    `addStageTarget/removeStageTarget`, and `removeTruss` (add was tested,
    remove wasn't) — added `stageStructureAddRemove()` +
    `stageStructuresXmlRoundTrip()` to the existing
    `engine/test/monitorproperties` suite. `PowerDistribution` (sources,
    circuits, fixture assignment, the direct-source auto-create-circuit-0
    behavior, XML round-trip) had **no test dir at all** — added
    `engine/test/powerdistribution` from scratch.
  - **All 3 pre-existing failures the gate surfaced are now fixed:**
    - `inputoutputmap::profileDirectories()` and
      `qlcfixturedefcache::defDirectories()` both failed on
      `QCoreApplication::applicationDirPath: Please instantiate the
      QApplication object first` — both binaries used `QTEST_APPLESS_MAIN`
      (no `QCoreApplication` instance at all), but the code they exercise
      (`QLCFile::systemDirectory()`) calls `applicationDirPath()` on macOS.
      Switched both to `QTEST_GUILESS_MAIN` (constructs a `QCoreApplication`,
      no widgets/GUI needed). That fixed the warning but exposed the real
      bug underneath: `systemDirectory()` resolves paths relative to the
      app-bundle executable (`Contents/MacOS/<app>` → `../Resources/...`),
      but each test's own expected-path construction assumed a flat
      CWD-relative path — true on Linux (where `systemDirectory()` doesn't
      consult `applicationDirPath()` at all) but not on macOS. Fixed both
      tests' expected-path construction to mirror the same
      `applicationDirPath()/../<dir>` relationship on Apple platforms.
    - `rgbscript`'s "Lines" script failed because its `linesMovement` and
      `linesLifecycle` list properties' declared defaults
      (`algo.linesMovement = 0` / `algo.linesLifecycle = 0`) are dead code —
      `getMovement()`/`getLifecycle()` actually read `algo.linesSlide`/
      `algo.linesRollover`/`algo.linesSizeBehavior`, never given a top-level
      default, so the very first (pre-`setMovement()`/`setLifecycle()`) read
      returned `""` — not one of the property's own declared list values.
      Harmless at runtime (undefined behaved like the intended default, 0),
      but broke property introspection. Fixed by initializing all three
      backing variables at the top of `resources/rgbscripts/lines.js`.
    - Verified clean with **two full, independent `cmake --build build
      --target check` runs** (not just the individual binaries) — fixture
      validation, all `engine/test/*`, all `ui/test/*`, and the enttecwing/
      midi/artnet plugin tests all pass end-to-end.
  - **`mastertimer_test` segfault — found + fixed (2026-08-17).** Root cause
    was a real, deterministic bug, not pure flakiness: `interval()`'s cleanup
    (`fs.stop()`, `mt->unregisterDMXSource(&dss)`) sat *after* a `QVERIFY` on
    a razor-thin real-time tick-count window (49–51 ticks/sec — upstream's
    own `SKIP_TEST` escape hatch for Travis CI is an acknowledgment this was
    always too tight under real scheduling load). `QVERIFY` returns
    immediately on failure, so a timing miss under load skipped cleanup
    entirely — `fs`/`dss` (stack locals) got destroyed while still registered
    with `MasterTimer`, leaving dangling pointers that crashed the *next*
    test method's `timerTick()`. Fixed by moving cleanup before the timing
    assertions (always runs now, regardless of outcome) and widening the
    tolerance to 40–60 (still catches a genuinely broken timer, far less
    sensitive to scheduler jitter). Verified clean on **2 more full `make
    check` pipeline runs** (4 total now, back to back). Not caused by
    anything built in this session (confirmed: `mastertimer` runs and
    completes before
    `ui/test` even starts, so the new `monitor_test` below isn't a factor).
  - **Phase 3 — headful dialog-driven pilot (2026-08-17, BUILT, proven
    viable).** New `ui/test/monitor` — the open question was whether a QTest
    UI test can drive a real, *blocking* `QDialog::exec()` call (as
    `Monitor::slotAddTruss()` uses) under `QT_QPA_PLATFORM=offscreen`.
    It can: schedule the interaction via `QTimer::singleShot(0, ...)` *before*
    calling the slot — `exec()`'s own nested event loop processes it,
    `QApplication::activeModalWidget()` finds the live dialog, `findChild<>()`
    reaches its fields/buttons. `addTrussAccepted()` fills the name field and
    clicks OK, asserts the truss landed in `MonitorProperties` with the right
    name; `addTrussCancelled()` clicks Cancel, asserts nothing was added.
    Both pass, standalone and through the full `make check` pipeline. Power
    Source turned out not to need this pattern at all — its add path
    (`PowerDistributionWidget::slotAddSource()`) is a direct model mutation
    with no dialog, already covered by Phase 2's `powerdistribution` tests —
    so the pilot narrowed to just Truss, which was the one open technique.
    Setup is cheap to replicate (`#define protected public` to reach the
    slot + a bare `Doc`/`Monitor` pair, no plugin/patch wiring needed).
  - **Phase 4 — expanded coverage + `slotRemoveSelected()` (2026-08-17,
    BUILT).** Added to `ui/test/monitor`:
    - `addTargetAccepted()`/`addTargetEditCancelled()` (`Monitor::
      slotAddTarget()`) — same simple-form-dialog pattern as Truss, plus a
      real behavioral difference worth proving: unlike Truss (object created
      only on Accept), StageTarget/Platform/Pipe/Stand/Tower are all created
      *immediately* with defaults, and the dialog that follows is an *edit*
      of the just-created object — so even Cancel leaves it added. Also
      asserts the accept path's bonus effect: a linked PanTilt palette gets
      auto-created.
    - `addPlatformEditCancelled()` (`Monitor::slotAddPlatform()`) — proves
      the same add-then-cancel path through a **heavier** edit dialog (one
      that embeds a full `StructureStudioView` canvas/tree/inspector via
      `makeStudioPane()`, unlike Truss/Target's plain `QFormLayout`).
    - `removeSelectedTruss()`/`removeSelectedCancelled()` — the other open
      half of Phase 3: select a `TrussItem` in the `QGraphicsScene`
      (`item->setSelected(true)`), call `slotRemoveSelected()`, which drives
      a **second, different kind of modal** — `confirmFeatureDelete()`'s
      `QMessageBox`, found the same way via `activeModalWidget()` — Accept
      removes it, Cancel doesn't.
    - **Two real bugs found writing these, both fixed in the tests
      themselves (not app code):**
      1. `QMessageBox::windowTitle()` reads back **empty** on macOS — native
         alert-style message boxes don't surface a title bar, even though
         `confirmFeatureDelete()` does call `setWindowTitle()`. Asserting on
         it left the confirm dialog's `exec()` with nothing ever clicked —
         a genuine **hang**, not a fast failure, and it corrupted whichever
         test ran next. Fixed by asserting on `QMessageBox::text()` instead
         (the actual message content), which *is* reliable.
      2. Blind `findChild<QLineEdit*>()` (no name filter) is ambiguous
         once a dialog embeds `StructureStudioView` — it contains its own
         QLineEdits, so the lookup can silently grab the wrong one instead
         of the name field. Rather than paper over it, **descoped**: no
         "accept with a custom name" test for Platform (or, by the same
         reasoning, Pipe/Stand/Tower — never attempted). The cancel-path
         test for Platform only drives the unambiguous button box, so it
         stays real coverage without the fragile lookup. Reliably testing
         the accept path for these four would need the production dialogs
         to tag their name field with `setObjectName()` first — not done.
    - Verified: 9/9 pass standalone, and **2 more full `make check` pipeline
      runs**, clean both times.
  - **Phase 5 — `release.sh` gate (2026-08-17, BUILT).** New step **1/6**
    (renumbered the existing 5 steps to 2/6–6/6): `cmake --build build
    --target check`, against the standard dev `build/` dir (Debug, reused
    as-is — a correctness gate on the codebase, not a rebuild of the exact
    Release bits `package-local.sh` ships from its own separate
    `build-package/`). `set -euo pipefail` (already at the top of
    `release.sh`) means a failing gate aborts the *entire* release right
    there — before `package-local.sh`, signing/notarizing, tagging, or
    publishing ever run. Verified both directions: the real gate command
    passes clean against this session's actual `build/`; a synthetic
    reproduction of `release.sh`'s exact structure with a deliberately
    failing stand-in step confirmed `set -e` genuinely halts before any
    later step's `step "2/6 ..."` banner even prints (did **not** run the
    real `release.sh` itself — that pushes a git tag and publishes a public
    GitHub Release, real external side effects, not something to fire off
    to validate a shell-flow change). `RELEASE.md`'s step list updated to
    match.
  - **Release-gate arc (Phases 1-5) is now complete end to end.** Remaining
    known gap: full add/remove dialog coverage for Pipe/Stand/Tower, gated
    on tagging their production dialogs' name fields with `setObjectName()`
    first (see Phase 4) — not chased further, no immediate need driving it.

- **Hardware tab: Power tree + universe usage grid (2026-08-12 → 08-13,
  BUILT)** — the former "Fixtures" tab is renamed **"Hardware"** (`app.cpp`,
  one-line tab-label change). Its tree already had a lazily-built
  "Universes" folder (`FixtureTreeWidget::updateTree()`); selecting a
  universe node now swaps the right-hand pane to `UniverseUsageWidget`
  (`ui/src/universeusagewidget.{h,cpp}` — embedded in the splitter like the
  group-layout editor and power view, *not* a popup dialog; started as one,
  corrected after eyeballing it) — a 512-cell address grid coloured per
  occupying fixture (deterministic hue from fixture ID) built on
  `Doc::fixtureForAddress()`, with a tooltip per cell and a fixture legend
  below. A new **"Power"** folder (peer to Fixture Groups/Universes, gated
  behind a new `FixtureTreeWidget::ShowPower` flag so the shared tree widget
  doesn't pick it up in picker dialogs, and *always present* even with zero
  sources so it can be selected/added-to) lists `PowerDistribution` sources →
  circuits → assigned fixtures, mirroring the existing group-folder nesting.
  Right-click the Power folder → **"Add power source…"**; right-click a
  source/circuit → **"Add circuit…"** (same defaults as the Power pane's own
  buttons). Dragging a fixture from anywhere in the tree onto a circuit *or
  a bare source* (lands on its first circuit, auto-created if needed) assigns
  it via `PowerDistribution::assignFixture()` — the same call the existing
  right-click "Add to power circuit" menu already used, now with visual/drag
  entry points too. `slotSelectionChanged()` was rewritten to route
  explicitly by selection type (group → layout, universe → usage grid, any
  Power-tree node → power view, single fixture → its info via the
  previously-dead `fixtureSelected()`, else → generic info) instead of
  defaulting everything-but-groups to the power view. The in-canvas
  Programming-tab power footer (dead code — `ProgrammingManager::
  m_powerFooter` was force-hidden since the readout moved to the app
  status-bar chip) is fully removed; its "Circuits…" button is replaced by
  making the status-bar Power chip itself clickable
  (`ProgrammingManager::openCircuitsDialog()`, wired the same way the MTC
  chip's click-to-bind menu already works).

- **Footer chip polish (2026-08-12)** — Power/Dangle chips (`⚡`/`⚠`) were
  rendering as full-size color emoji next to plain-text chips (Ready/Autosave/
  Saved), reading as a font-size mismatch though the point size was identical
  the whole time; root cause was Unicode emoji-presentation glyphs, not a
  QFont issue. Fixed by appending the text-presentation variation selector
  (U+FE0E) to `⚡`/`⚠`, and gave MTC (`⏱`) and Load (`⚙`) their own leading
  icon in the same style, so all four global status-bar chips read at one
  consistent visual size. Also corrected `platforms/macos/Info.plist.qmlui`
  (only installed when `qmlui` is built) — it still had the pre-rebrand
  `qlcplus-qml` executable/name and `qlcplus.icns` icon refs with no
  `CFBundleIdentifier` at all; now matches the widgets build's
  `com.bransonmatheson.qlcconsole` identity.

- **Note-effect calibration + MIDI plumbing** — per-universe MIDI source scoping
  (`data.midi.universes[n]` + a device-name dropdown, not a slider); switching a
  look's effect script now reloads the live preview; **live param edits reach the
  running effect** (`reloadParamsFromPalette`/`updateEffectParams` — was the root
  cause of "axis does nothing / Learn won't lock"); persistent **Learn range**
  button (writes noteLow/noteHigh, flips to Manual). `QLC_EFFECT_DEBUG=<path>`
  env trace kept (grid/range/held/lit-cols + col0/col64 fixture+DMX+base).
- **Looks: effect-vs-fixture colour separation + tree UI** — a Colour/Dimmer
  palette *nested under* an Effect in the Looks tree feeds that effect and is NOT
  painted as a static base; top-level looks light the fixtures. Fixes same-colour
  strike-on-base washout (RGB-only fixtures). Mechanism = ordering
  (`QLCPalette::isEffectScoped`); tree is derived from/rewrites the flat order.
  Right-click → "Move to fixtures (base)" / "Feed effect ▸ <name>" for explicit
  re-homing; Up/Down moves across nesting boundaries (flat-order move).
  *Open polish:* dropping an external palette onto a specific effect item should
  nest under THAT effect (currently appends). Future: the richer per-item
  assignment could grow beyond order (explicit bind) if multi-effect looks get
  fiddly.
- **Effect perf + release fade-out** — input-reactive effects (midi/audio/
  joystick) now WAIT for input instead of polling: an idle effect whose last
  frame drove nothing is skipped until fresh input (`m_inputDirty` +
  `lastFrameEmpty()`), killing the 50 Hz baseline load of a loaded note effect on
  a big pixel grid. And effect looks have a **release fade-out** set via the Looks
  Fade Out cell — on stop the effect decays over that time instead of snapping
  (EffectInstance fade envelope; runner keeps it ticking then reaps).

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Output ENDPOINT reachability check (BACK BURNER, 2026-08-25)** — the
  readiness indicator added in `daeeb97d7` catches one failure mode: the
  workspace names a plugin *line* (for the network plugins, an index into the
  local interface list) that doesn't exist on this machine. It does **not**
  catch the other one: the line is fine, but the ArtNet *node* at the far end
  isn't answering. Found during the live soak on `ender`, where 50 of ~53
  patched universes were failing to send to 13 node addresses
  (`172.18.2.201`–`.230`) — in that instance correctly, because the devices
  were in the trailer, which is exactly why this needs care rather than a
  naive "ping the target" test:
  - a broadcast/subnet target legitimately has nothing to answer it, so
    unreachable ≠ misconfigured;
  - nodes are routinely powered down between calls, and a desk that cries
    wolf every load gets ignored (the same failure mode as the 2850-line
    `sendDmx` spam this replaced);
  - ARP/ping liveness is a poor proxy — an ArtNet node can answer ARP and
    still not be listening on 6454.
  **Shape agreed with Branson (2026-08-25):** the rig is mostly **unicast**, so
  a per-target reachability check IS meaningful and is the main case worth
  building — for a unicast destination, "is that node there?" is a real,
  answerable question. For a **broadcast** target there is nothing to answer,
  so the check degrades to "is the interface/subnet present and up?".
  Critically: validate **directly-attached subnets only**. ArtNet nodes are
  normally on a directly-attached segment; anything routed should be left
  alone rather than guessed at, which also sidesteps the "pingable via the
  default gateway but not actually the show network" false-positive seen on
  ender (192.168.21.214 answered ping while being the wrong interface
  entirely).

  Still informational rather than a blocking NOT READY, and probably ArtPoll
  rather than ICMP — an ArtNet node can answer ARP and not be listening on
  6454. Only worth building when someone is actually chasing a dark universe.


- **Simple Desk sliders have no write-back-to-scene path (PARKED, 2026-08-12)**
  — found while triaging the old `live-edit-4.x` branch: Virtual Console
  sliders/XY-pads already write manual adjustments back into the running
  Scene via `CaptureManager::recordOverride()` (wired from `vcslider.cpp`/
  `vcxypadfixture.cpp`, with the existing capture/undo/diff/store workflow —
  see `LiveCaptureDialog`), but `SimpleDesk` (`ui/src/simpledesk.cpp`) has
  zero `CaptureManager` references — moving a Simple Desk fader doesn't
  persist anywhere. Better path is wiring Simple Desk's
  `slotUniverseSliderValueChanged` into `CaptureManager` rather than
  reviving `live-edit-4.x`'s standalone `LiveEditManager` (undo-less
  direct-write) — confirmed `CaptureManager` isn't just equal plumbing,
  it's strictly *better* than that reference implementation:
  - `LiveEditManager::findMostSignificantScene()` (the "which running Scene
    actually wins when several touch this channel" question) was left as an
    explicit `// TODO: Implement HTP/LTP priority logic` stub —
    `return scenes.first();`, i.e. never solved. `CaptureManager::buildPlan()`
    already solves exactly this: it walks running functions in **start
    order**, so the most-recently-started Scene wins LTP per (fxi, channel) —
    a real, reasoned answer, inherited for free.
  - `CaptureManager::buildPlan()` also already flags **chaser-driven
    channels** (a channel currently under a running Chaser step's Scene) and
    excludes them from capture by default — `LiveEditManager`'s plain
    fader-walk has no equivalent, so a Simple Desk tweak there could point-edit
    a Scene the Chaser immediately overwrites on its next step. Preserve this
    exclusion when wiring Simple Desk in.
  - One piece of `live-edit-4.x` *is* directly reusable regardless: unlike VC
    widgets (pre-bound to a specific function), a Simple Desk slider only
    knows an absolute `(universe, address)` — `LiveEditManager::
    findScenesForChannel()`'s address -> `(fixtureId, channel)` resolution
    via `Doc::fixtureForAddress()` is the correct, already-written way to get
    from "which slider moved" to the `(fxi, channel)` pair
    `CaptureManager::recordOverride()` actually wants.
  - Net effect: Simple Desk gains MORE than `live-edit-4.x` ever had (undo,
    conflict/chaser-driven visibility, save-as-new) for less new code than
    reviving `LiveEditManager` would take.
  Needs its own design/implementation pass, not a quick cherry-pick.

- **Cue transition model — hold-on-miss + release-on-transition (4b; DEFERRED,
  needs rig)** — DECIDED framing: a *missed/skipped* cue is a non-event → hold
  last look (no blackout); a cue that *fires* releases what it replaces (outgoing
  look + its effects fade out) → no dangling. Split confirmed: **fade intensity,
  hold position/colour**. Risky part = the intensity-latch core-mixer work; needs
  a live rig to test, so deferred until Branson has rig time. First verify whether
  the current timeline transition already releases the outgoing look or holds it.
- **Pre-positioning / mark cues + ghost visual + dangle detector** — how big
  consoles do move-in-black. See `MOVEINBLACK_DESIGN.md`. Slices **1 + 2 SHIPPED**
  (all rig/visualiser-verify):
  - **1** — `MarkEffect` DMXSource (holds non-intensity, auto-releases on reveal,
    `<Mark>` XML) + Mark/Unmark buttons + dashed-violet monitor ghost.
  - **2a** — `CueOutput`: offline "what would this cue output" (unit-tested).
  - **2b** — `CueLookahead`: next cue + lead time for Chaser & Show.
  - **2c** — `MarkPlanner` + **Auto MIB** toolbar toggle: look-ahead → pre-set
    dark→lit→moving movers, dark-gap gated.
  - **2 persist** — Auto-MIB toggle + dark-gap round-trip (`<MoveInBlack>`) + a
    "s lead" toolbar spinbox. DONE.
  - **3 — dangle detector** — `MarkPlanner::dangleFixtures()` + `dangleFixturesChanged`
    signal, forwarded `ProgrammerController` → App footer chip. DONE (unit-tested,
    engine/test/markplanner). Runs regardless of Auto-MIB being on (manual marks
    dangle too). *Needs rig eyeball to confirm the chip reads right live.*
  Still open: **verify CueLookahead timing on a rig** (the dark-gap depends on it);
  **force-live / force-mark** per-cue overrides + dark-move fade. Also: mark "to a
  chosen look"; a monitor context-menu Mark action. *(from the cue-policy discussion)*

## Build-season freeze list *(LOCKED with Branson — ship before feature freeze)*

- 🔴 **Rig test pass** — run `RIG_TEST_PLAN.md`, fix any ❌ (the gate).
- 🔴 ~~Auto-MIB persist + dark-gap setting~~ **DONE** (b38795658).
- 🔴 **PMJ (OpenDeck) mapping** — the operating surface for the show (free knobs =
  the relative encoders already supported). APC40 map de-scoped. *Best built with
  the PMJ in hand (LED feedback / knob verify).*
- 🟡 ~~Power/circuits → footer bar~~ **DONE** (a9aab77db) — ⚡ status-bar chip.
- 🟡 ~~Native single-shot~~ **DONE — became the effect-lifecycle work** (4c7c9621c,
  `EFFECT_LIFECYCLE_DESIGN.md`): effects declare loop/reactive/**oneshot**;
  one-shots run on `inputs.phase`, duration resolves Look→cue/chase→default,
  hold/release on finish; Wand example + per-look length UI. *Follow-ups (post-
  freeze OK): span on the Show timeline (falls back to naturalDuration today);
  fold RGBScript `Once` into the lifecycle; per-look syncTo/onFinish UI.*
- 🟡 ~~Small polish batch~~ **DONE** — drop-onto-specific-effect nesting (5e-ish),
  End-at-SMPTE (751ce1b5a); MTC-chip glyph was already in place.
- ⚪ ~~dangle detector~~ **DONE** (2026-08-12) — see slice 3 above.
- ⚪ Post-freeze: cue-transition 4b; "Look" as assembly unit;
  unified object editor; more stage objects; rebrand to qlcconsole.
- **Effects respect the look's master Dimmer — SHIPPED** (563c4b3b1): colour
  output scales by the look's Dimmer on dimmerless fixtures; dimmered fixtures
  carry it on the master channel. Move to DONE.md next pass.
- **Move circuits / power usage to the footer bar** — the power/amperage &
  circuit load estimator currently lives in the Programming space; move it to the
  footer bar (like the timecode/load chips) so it's an always-visible status
  readout and reclaims programming canvas. *(Branson request)* See memory: power
  estimation feature.
- **New control surface — integrate (TBD)** — a new hardware control surface to
  bring in (details TBD). Fold into the MIDI-mapping work below once specced.
- **Control-surface engine (PMJ + APC40 mk2 + Xbox) — IN PROGRESS, at the rig.**
  See `CONTROL_SURFACE_DESIGN.md`. Device-agnostic engine: surface model +
  role/page vocabulary + context-aware LED loop; boards are overlays. **P0
  CORE — DONE** (81eaab311, unit-tested `engine/test/controlsurface/`).
  Decisions locked: static core = **GM, Blackout, Blind, Tap, Go, Back**
  (identify more via workflow); **faders follow the page** (optional
  submaster page). All 4 original P1-blocking board facts are resolved
  (encoder CCs 11-14, LED velocity/steady-level scale, output channel 9,
  static-core placement `O`→Blackout/`Set`→Blind). **P1 PMJ overlay + LED is
  underway** (`ui/src/pmjoverlay.{h,cpp}`, slices 1-6 in "Recently shipped"
  above) — LEDs light only for wired controls, Master fader → Grand Master,
  `O`/`Set` → Blackout/Blind, and Enc 1/2 now nudge a focused PanTilt
  palette's XY pad live, direction-confirmed on the real board. **Still
  open in P1**: Select/Load/Transport wiring (needs the app-side
  selection/paging model — not built yet), Favorites/Tap binding (only a
  Programming-tab-local tap-tempo exists today, not the global static-core
  action the design doc means), per-strip fader Level(1-10) semantics
  (submaster vs per-fixture — open design question for Branson), Enc 3/4
  targeting (colour/beam — needs the same selection model as Select/Load),
  and the `Set` button's LED specifically (toggles correctly but doesn't
  light — likely a hardware button/LED address pairing issue, try
  `qlc-midi opendeck identify`/`align`). Then: P2 runtime page → P3 APC40
  mk2 overlay → P4 Xbox roles. Plumbing found: buttons are MIDI notes
  (offset-128; LED note == button note on ch9); faders/enc-push are CC
  (offset<128); LED via `InputOutputMap::sendFeedBack`→`feedbackToMidi`.
  Relative encoders already built.
- **Timecode slice 3 — auto-fill internal latency** — the packet→DMX figure needs
  plugin-side timestamping; once measured it folds into the offset. *(from Timecode
  calibration in DONE.md)*
- **Audio calibration — active loopback self-test** — play a click on the audio
  output, time the round-trip to detection = true audio latency. Needs an audio-emit
  path (AudioRenderer wants a decoder) and is unverifiable offscreen; the
  editable/seeded detection-latency + ±10 ms nudge cover it meanwhile.
- **Show-length polish** *(2026-08-12: end-handle label collision confirmed
  already fixed — commit dcc620241 moved the length chip to the ruler strip
  above the marker lane specifically to stop it colliding with a section
  marker's own label. Genuinely still open:)* "End at SMPTE hh:mm:ss"
  convenience (needs the offset in the host menu); bar/beat snapping for the
  end handle when a BPM is set.
- **GUI headful automation** — `screencapture` + `cliclick` driver so Claude can
  drive AND validate real UI (moving this to the spare machine). First task there:
  a `gui-drive.sh` wrapper, then drive the end-handle drag with eyes on real pixels.
- **Clickable Load chip → per-function breakdown (2026-08-12 idea)** — today
  `MasterTimer` only times the whole tick as one number (`engine/src/
  mastertimer.cpp`, the `computeTimer` around `timerTickFunctions()` +
  `timerTickDMXSources()`); no per-Function/per-DMXSource granularity exists.
  Would need wrapping each `function->write()` call in the tick loop
  individually, accumulating per-function-id compute time, and exposing a
  top-N query — real engine instrumentation, not a UI-only change. The
  existing single-number Load chip itself is already cheap (a single
  `QElapsedTimer` read per tick, always-on regardless of the chip; the footer
  just polls an atomic every 500ms) — no separate background load meter
  needed for that part.

---

## Done — real "Import" for .qxw, distinct from Open *(2026-08-12, BUILT)*

File > Import: merge fixtures/fixture groups/functions from a second .qxw
into the CURRENTLY OPEN document, unlike Open (which replaces everything).
Resolved the open design questions from this item's original design-first
note:
- **Scope**: fixtures, fixture groups, AND functions/shows — not fixtures-only.
  Picking a function auto-expands to its full dependency closure (member
  functions, the fixtures/groups they touch) via new `Doc::functionFunctions()`
  (mirrors the existing `Doc::functionFixtures()`) — no manual dependency
  picking needed.
- **ID collisions**: real remap-on-import, not a blind merge. Three separate
  ID spaces (fixture/function/fixture-group) each keep the source ID when
  it's free in the target, else get a fresh one; every cross-reference
  (`SceneValue::fxi`, `ChaserStep::fid`, `RGBMatrix`'s fixture group,
  `FixtureGroup` head assignments, `Show`/`Track` function refs) gets
  rewritten afterward to match. Also handles a DMX-address-space collision
  (a fixture ID can be free while its address range still overlaps something
  already patched) by relocating within the same universe, reported
  separately from ID remaps.
- **UI**: dedicated `ImportSelectionDialog` (browse + pick), not drag-and-drop
  of a file — matches the existing `FixtureSelection`/`FunctionSelection`
  picker convention rather than inventing a new one.
- New engine module `engine/src/qxwimporter.{h,cpp}` (`QxwImporter::import()`)
  does the actual closure/remap/clone/rewrite work, engine-side and
  UI-independent; loads the source file into a throwaway scratch `Doc`
  (`App::loadScratchDoc()`) rather than ever touching the live one mid-merge.
- Verified end-to-end against real workspace files (not just code review):
  clean imports, heavy-collision imports (96/96 IDs correctly remapped, exact
  fixture/group/function count arithmetic), and an adversarial dense-universe
  case (correctly reports "no free DMX address" per fixture rather than
  corrupting anything) — all with zero dangling fixture references across
  every function in the target doc afterward.
- Caught and avoided reusing `Function::createCopy()`/`copyFrom()` for this —
  `Show::copyFrom()` has a latent same-doc assumption (looks up member
  functions via the *target* doc, not the source) that would have silently
  dropped every child function of an imported Show. Cloning goes through each
  object's own `saveXML()`/`loadXML()` round-trip instead (doc-agnostic,
  same proven code path real file Open/Save already uses).
- Not rewritten: function/fixture IDs referenced as literal numbers inside
  Script text (can't safely parse arbitrary JS for this) — scripts import
  verbatim. Audio/Video functions' referenced media files don't travel with
  the import (same as moving a workspace to another machine today).

---

## Done — rebrand the fork to "qlcconsole" *(all 3 phases shipped)*

The fork is now firmly a **desktop console** (mouse+keyboard, MIDI, multi-window),
well past the tablet/Android QML flavour — rebrand from QLC+ to **qlcconsole**.
Distinct from the *Lighting Studio* rename (that was just the 2D tool; shipped).
Keep upstream attribution/license — this is a fork identity, not a takeover.

Phase 1 (2026-08-10, commit 7bf4f6daa) — display strings: window/About titles,
log filename, `.qlcc` workspace extension (`.qxw` still read/imported).

Phase 2 (2026-08-10) — build/binary names: top-level CMake project name and
`CPACK_PACKAGE_NAME` → `qlcconsole`; executable targets `qlcplus` →
`qlcconsole`, `qlcplus-launcher` → `qlcconsole-launcher`,
`qlcplus-fixtureeditor` → `qlcconsole-fixtureeditor` (main/CMakeLists.txt,
launcher/CMakeLists.txt, fixtureeditor/CMakeLists.txt, launcher.cpp's
hardcoded spawn paths, macOS Info.plist CFBundleExecutable, Linux .desktop
Exec= lines, CLAUDE.md/RIG_TEST_PLAN.md/testing_st.md run commands).

Phase 3 (2026-08-12, discovered already-shipped via commit 22fde51d1 + this
pass) — icon/asset filenames (`qlcconsole.icns`, `qlcconsole-fixtureeditor.icns`
etc.) and the macOS `CFBundleIdentifier` (`com.bransonmatheson.qlcconsole`)
were already renamed; only `resources/doxygen/qlcplus.dox` (a docs-generation
config, not app-facing) was still qlcplus-named — renamed to
`qlcconsole.dox` + updated its `PROJECT_NAME` and the `CMakeLists.txt`
doxygen target reference.

UI polish pass (2026-08-11) — main toolbar + global actions (Blackout/Blind/
Operate) + bottom tab bar: repadded/gloss-rendered the 4 mismatched action
icons to match the classic Crystal/Oxygen set's shading (`resources/icons/png/
blackout,blind,operate,design.png`), unified toolbar+tab-bar icon size to
24x24 (`App::initToolBar()`), added a bundled default chrome QSS
(`resources/qss/default.qss`, cascades under the existing user
`~/.qlcconsole/qlcplusStyle.qss` override via `AppUtil::getStyleSheet`).
Deliberately deferred: per-manager toolbar icon-size unification (Virtual
Console 26px / I-O Manager 32px / Show timeline 20px / Monitor 16px all stay
as-is), no dark/light theme switcher *(superseded — see backstage color
themes below, 2026-08-11)*, no SVG-icon migration, Fixture/Function
Manager's own toolbars and the Programming tab's button styling untouched.

Backstage color themes (2026-08-11) — View menu > Theme: Default/Tan/Blue,
picked from `App::Theme` (`ui/src/app.h`), persisted via `workspace/theme`
(`QSettings`), applied live by `App::applyTheme()` (`ui/src/app.cpp`). Whole-
app-surface theming via a `QPalette` swap (`qApp->setPalette()`) rather than
hardcoded per-theme stylesheets — pays off the earlier chrome QSS work
directly, since `resources/qss/default.qss` already reads colors via
`palette(...)` functions, so it follows any theme with zero further changes.
Extended `default.qss` slightly (QGroupBox/QMenuBar/QMenu, still
palette()-only) so more chrome reaches along. Verified all 3 themes
end-to-end via the offscreen snapshot harness (real screenshots, not just
code review) — Default exactly restores the pre-theme look, Tan/Blue both
tint the full window (toolbar, tabs, tables, panels) convincingly. Known
platform caveat, not a bug: this app doesn't force the Fusion widget style
app-wide (only a few `ConsoleChannel` sub-widgets use it), so a handful of
plain native-style buttons/menus elsewhere may follow the palette less
completely than content areas do — full uniform recoloring would mean
switching the app's global QStyle to Fusion, a much bigger, separate
look-and-feel decision, not done here.

Window couldn't be resized smaller / again (2026-08-11) — regression report
led to finding `QTabWidget`/`QStackedWidget` compute minimum size as the max
over ALL pages, not just the visible one, so any one oversized page pins the
whole window (or sub-widget)'s minimum regardless of what's showing. Three
real instances fixed with the same `QSizePolicy::Ignored` + explicit-floor
pattern: `SimpleDesk::initSliderView()`'s unwrapped 32-slider row
(simpledesk.cpp), `LookEditor`'s internal `QStackedWidget` where the pan/tilt
page's 200x200 XY pad bloated every other page (lookeditor.cpp), and
`ProgrammingManager`'s per-fixture console scroll area (proactive — same
pattern, was hidden by default so not yet visibly triggered). Window minimum
width dropped 1296px -> 942px. Not yet investigated: Simple Desk's "Cue
Stack" tab (938px, now the binding constraint) looks like legitimate
toolbar+cue-list content rather than a bug — stopped there per Branson's call.

Per-manager toolbar text/icon label mode (2026-08-11) — the "Icons only /
Text only / Icon+text" workspace setting (`App::applyTabLabelMode()`) only
ever touched the main tab bar and the main window's own toolbar (Panic/
Blackout/Blind/Operate); every per-manager toolbar (Function Manager, Fixture
Manager, Input/Output Manager, Virtual Console) was hardcoded to Qt's
icon-only default, deaf to the setting. Added a `applyToolbarLabelMode()`
method to each (mirrors the existing `Monitor::applyToolbarLabelMode()`
pattern — reads `workspace/tabLabelMode` directly via QSettings), called from
`App::applyTabLabelMode()`. Simple Desk has no comparable QToolBar (its view
controls are standalone QToolButtons), so it's not part of this.

Release readiness (2026-08-11) — v0.1.0 prep landed: `VERSION` file +
`CHANGELOG.md` (SemVer release tags, independent of the git-derived
`APPVERSION` build string); `README.md`/`CONTRIBUTING.md`/`SUPPORT.md`
rewritten with correct qlcconsole branding, fork-of-QLC+ attribution, and
links back to this repo instead of upstream; `RELEASE.md` + `release.sh`
wiring up the existing local build/sign/notarize scripts
(`platforms/macos/package-local.sh`, `sign-notarize.sh`) to tag + publish a
signed macOS DMG as a GitHub Release. Still open before actually cutting
v0.1.0: write the first real `CHANGELOG.md` entry against what's shipped by
then, and run `release.sh` for real.

---

## Now — unify the object editor's fixture management *(design-first; options pending)*

Follow-on to the embedded object-editor canvas (Lighting Studio). Combine the
fixture-properties "edit" dialog (double-click a fixture) and the "Edit Fixtures in
Studio" layout window into ONE object editor. Wants: a left tree of fixtures BY
FIXTURE GROUP as assigned to this object; right-click to add fixtures/groups;
multi-select → create a fixture group (opens the Fixtures-tab head-layout mapping);
drag-drop tree→face to add; distribute/put-on-face via popup or right-click.
**Design options being drafted — pick a direction before building.**

---

---

## Now — more stage-feature objects *(active; design-first)*

Extend the discrete map-object model (truss / platform / target / power source /
image / studio group) with the common rigging structures below. Shared needs:
placeable, movable, lockable, layerable/groupable, XML round-trip, a fixture-host
role (fixtures mount to them like they do to a truss), and a 2D + derived-3D
representation. Likely a small **StageStructure** base + parametric shapes, reusing
the truss geometry funnel (`fixtureRigPosition`) and the bar-on-truss / studio-group
patterns. Write a short design doc first (à la FIXTURESTUDIO_DESIGN.md).

SHIPPED (2026-07-31, see DONE.md) — the unified **Pipe** + **Stand** + **Tower**
object set, all fixture-hosting, 2D + elevation, editors, XML:
- [x] **Boom** = a vertical **Pipe** (on a stand, hung from a truss, or free);
  fixtures mount up the pipe at height + facing angle.
- [x] **House electric** = a horizontal **Pipe** (same object, orientation flag +
  run angle). Booms/electrics are ONE object now (resolved the bar/boom/pipe
  overlap).
- [x] **Stand** = a distinct placeable base; a pipe stands on it (base derived,
  follows the stand). (Trusses/towers on stands = future.)
- [x] **Trusses with booms** — a pipe parented to a truss (drop-arm), base derived.
- [x] **Tower** (16" sq × 8') with **shelves** at heights; fixtures mount on a
  shelf (towerU/V), derived from the tower.
- Demo: `test-workspaces/stage-structures-demo.qxw` (one of each + a fixture on the tower).

Still open (lower priority — non-fixture-hosting scenery): flats, drapes/legs,
set pieces. And: a Stand should also support a truss/tower (only pipes today);
a Tower editor "Cancel" still applies (live-edit); horizontal-pipe fixtures at
per-fixture offsets along the run.

---

## Backlog — not started

### Show lifecycle: Construction / Test-Validate / Production *(2026-08-15, design doc written)*
See `SHOW_LIFECYCLE_DESIGN.md` — names the three phases a show moves through
(building off-rig → matching Lighting Studio to the real rig → running on the
road) and settles that this is **not** a third `Doc::mode()`; it's the
existing Design/Operate axis crossed with a "connected to real hardware vs.
fully simulated" axis (real, non-Dummy plugin patched **and** not Blind —
Blind turned out to already BE the "disable real output" switch: it silences
every protocol at once and is already Design-only/force-off-in-Operate, see
the doc's 2026-08-15 finding). Concrete follow-ons, none started:
1. Compute + expose the rolled-up connected/simulated state
   (patched-and-not-Blind).
2. Surface it as a footer chip alongside MTC/Load/Power.
3. Decide + build whether Design mode should auto-engage Blind when it
   detects real hardware patched, rather than leaving it opt-in.
4. Turn `RIG_TEST_PLAN.md`'s manual checklist into an actual in-app
   Test/Validate workflow (bigger; own design pass on where it lives).
5. Retroactively tag the rest of this backlog by which phase it serves, to
   sharpen prioritization instead of treating it as one flat list.

### ~~Tiny: mark the MTC-chip section label as show-sourced~~ **DONE** (f91c91992, 2026-07-29)
Already shipped — `App::slotTimecodeStatusChanged` prefixes the section label with
`▸` so it reads as show-content, not as text arriving from the MTC stream. This
entry was just stale; confirmed via `git log` (2026-08-19) and removed.
