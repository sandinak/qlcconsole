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
- **Move-in-black slice 3 — dangle detector** (positioned-but-dark fixture matching
  no upcoming cue = warn). Pure logic on `CueOutput`/`CueLookahead`, unit-testable.
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
  Still open: **verify CueLookahead timing on a rig** (the dark-gap depends on it);
  **force-live / force-mark** per-cue overrides + dark-move fade; **(3) dangle
  detector** (positioned-but-dark fixture matching no upcoming cue = warn — falls
  out of the plan). Also: mark "to a chosen look"; a monitor context-menu Mark
  action. *(from the cue-policy discussion)*

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
- ⚪ Post-freeze: cue-transition 4b; dangle detector; "Look" as assembly unit;
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
- **Show-length polish** — "End at SMPTE hh:mm:ss" convenience (needs the offset in
  the host menu); bar/beat snapping for the end handle when a BPM is set; clean up
  the end-handle label collision with a marker at the same position (found on the
  spare-machine pass).
- **GUI headful automation** — `screencapture` + `cliclick` driver so Claude can
  drive AND validate real UI (moving this to the spare machine). First task there:
  a `gui-drive.sh` wrapper, then drive the end-handle drag with eyes on real pixels.

---

## Now — real "Import" for .qxw, distinct from Open *(design-first; not started)*

Today `-o`/File > Open and `--open` all *replace* the current document with
another `.qxw`. There's no way to pull content FROM one workspace INTO the
one you're already working in (merge in another show's fixtures/functions/
palettes without starting over). Want a real **File > Import** workflow.
Needs a design pass before building — at minimum:
- What's importable: whole workspace vs. a scoped subset (fixtures only?
  a function/show subtree? palettes/bundles?).
- ID collision handling — imported fixtures/functions/universes will very
  likely collide with IDs already in use; needs a remap-on-import strategy
  (and probably a preview/picker UI, not a blind merge).
- Where it fits: a dedicated import dialog (browse + pick what to bring in)
  vs. drag-and-drop a second .qxw onto a tree (Fixtures tab, Function
  Manager) the same way fixtures/groups already drag onto targets elsewhere
  in this fork's UI.
- Relationship to the `.qlcc` vs `.qxw` split from the rebrand (Phase 1) --
  does Import read both extensions the same as Open does today?

---

## Now — rebrand the fork to "qlcconsole" *(Phase 1 + 2 done; icons/bundle ID pending)*

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

Deliberately left alone: icon/asset filenames (still `qlcplus.icns` etc. —
asset rename is a separate pass) and the macOS `CFBundleIdentifier`
(`com.bransonmatheson.qlcplus` — changing it resets TCC permission grants and
app preferences, so leave unless there's a reason to force that reset).

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

## Now — Fixtures tab: power tree + universe usage grid *(not started)*

- **Power systems in the tree** — a `Power` folder in the Fixtures-tab tree
  (peer to Fixture Groups); add devices under `Power → <circuit>` the same
  drag/right-click workflow as adding fixtures to a group. Gives power
  planning a real home in the tree instead of living only as a 2D stage-object
  (see "more stage-feature objects" below — the tree entry and the map object
  should probably reference the same underlying circuit).
- **Rename the Fixtures tab → "Physical Groups"** — once it hosts fixture
  groups AND power circuits (and maybe other physical groupings later),
  "Fixtures" undersells what's actually in there.
- **Double-click a universe folder → channel-usage grid** — same drill-down
  gesture already used elsewhere in the tree; opens a grid showing what's
  patched/consuming each channel in that universe.

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
- Demo: `stage-structures-demo.qxw` (one of each + a fixture on the tower).

Still open (lower priority — non-fixture-hosting scenery): flats, drapes/legs,
set pieces. And: a Stand should also support a truss/tower (only pipes today);
a Tower editor "Cancel" still applies (live-edit); horizontal-pipe fixtures at
per-fixture offsets along the run.

---

## Backlog — not started

### "Look" as the assembly unit (Scene/Collection rethink) *(Branson shower-thought; design-doc-first)*
Don't rename Scene/Collection (breaks traditional QLC users). Instead make the
fork's **Look** first-class: give it (1) an explicit **fixture scope** (the
"subpart vs whole stage" idea, as metadata) and (2) **palette/fixture state as the
BASE that effects/RGBScripts consume** (parameterise effects by the Look's state
vs discrete per-fixture config; old way still works). Continuous with palette-fed
looks + dimmer-as-multiplier scene-base already built. Write a design doc first
(à la FIXTURESTUDIO_DESIGN.md) before code.

### Tiny: mark the MTC-chip section label as show-sourced *(Branson shower-thought)*
The MTC footer chip appends the current show SECTION under the playhead
(`MTC ● …:12:03 · Startup`). An operator could misread the section name as arriving
FROM the MTC stream — MTC carries only the SMPTE clock; the section is a LOCAL show
marker looked up at that clock. Cheap typographic fix: scope the label so it reads
as show-content (e.g. `▸ Startup` section glyph, or `→ Startup` implying "resolves
to"). 1-line change in `App::slotTimecodeStatusChanged` (ui/src/app.cpp, the `ctx`
string). Not behavioral; deferred pending decision.
