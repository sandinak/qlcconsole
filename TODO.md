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
- **Control-surface engine (PMJ + APC40 mk2 + Xbox) — ⛔ PARKED (needs the
  hardware; resume at THIS rig, not on travel).** See `CONTROL_SURFACE_DESIGN.md`.
  Device-agnostic engine: surface model + role/page vocabulary + context-aware LED
  loop; boards are overlays. **P0 CORE — DONE** (81eaab311, unit-tested
  `engine/test/controlsurface/`). Decisions locked: static core = **GM, Blackout,
  Blind, Tap, Go, Back** (identify more via workflow); **faders follow the page**
  (optional submaster page). **P1 (PMJ overlay + LED) is blocked on 4 board facts**
  — get these from OpenDeck / the QLC input monitor with the PMJ plugged in:
  1. **Encoder ROTATION CC numbers** (profile only lists the Enc 1-4 *push*, CC11-14).
  2. **LED velocity scale** — board wants brightness 0-15; QLC feedback maps
     0-255→0-127. Which value = full?
  3. **Output MIDI channel** (LEDs are ch 9) so the `sendFeedBack` params offset lands them.
  4. **Static-core button placement** — PMJ has no Blackout/Blind/Tap buttons.
     Proposed: `O`→Blackout, `Set`→Blind, `Favorites`→Tap (confirm/reassign).
  Then: P1 PMJ overlay+LED (Design page) → P2 runtime page → P3 APC40 mk2 overlay →
  P4 Xbox roles. Plumbing found: buttons are MIDI notes (offset-128; LED note ==
  button note on ch9); faders/enc-push are CC (offset<128); LED via
  `InputOutputMap::sendFeedBack`→`feedbackToMidi`. Relative encoders already built.
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

### "Look" as the assembly unit (Scene/Collection rethink) *(Branson shower-thought; design-doc-first)*
Don't rename Scene/Collection (breaks traditional QLC users). Instead make the
fork's **Look** first-class: give it (1) an explicit **fixture scope** (the
"subpart vs whole stage" idea, as metadata) and (2) **palette/fixture state as the
BASE that effects/RGBScripts consume** (parameterise effects by the Look's state
vs discrete per-fixture config; old way still works). Continuous with palette-fed
looks + dimmer-as-multiplier scene-base already built. Per the show-lifecycle doc
above: (1) mainly serves Construction (organizing/filtering), (2) mainly serves
Production (runtime behavior) — worth designing as two separate slices rather
than one. Write a design doc first (à la FIXTURESTUDIO_DESIGN.md) before code.

### Tiny: mark the MTC-chip section label as show-sourced *(Branson shower-thought)*
The MTC footer chip appends the current show SECTION under the playhead
(`MTC ● …:12:03 · Startup`). An operator could misread the section name as arriving
FROM the MTC stream — MTC carries only the SMPTE clock; the section is a LOCAL show
marker looked up at that clock. Cheap typographic fix: scope the label so it reads
as show-content (e.g. `▸ Startup` section glyph, or `→ Startup` implying "resolves
to"). 1-line change in `App::slotTimecodeStatusChanged` (ui/src/app.cpp, the `ctx`
string). Not behavioral; deferred pending decision.
