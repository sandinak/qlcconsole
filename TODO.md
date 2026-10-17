# TODO — open work

Only what is still to do. Shipped work — with its build notes, follow-ons
and "not yet verified on the rig" caveats — is in [DONE.md](DONE.md); the
sections moved there on 2026-10-11 are under "Moved from TODO.md". When
something ships, move its entry to DONE.md. See also the session memory
under `~/.claude/.../memory/`.

---

## Small fixes and leftovers

- **Layers panel: a row the plot can't select loses its selection on any
  rebuild (2026-10-15)** — clicking a fixture the plot can't select (stacked
  inside a tower in plan view) now keeps its row selected, but the next
  `MonitorLayersPanel::reload()` re-selects from the plot's (empty)
  selection and clears it. Rebuilds are frequent (a rename, any
  `fixtureChanged`), so renaming T1-W from the list may still drop the row
  afterwards. Found when monitor_test's
  `aLayersRowStaysSelectedWhenThePlotCannotFollow` failed under QPA minimal:
  a queued rebuild landed after the click (the test now settles first).
  Fix: carry the list's own selection across a rebuild for items the plot
  can't hold.

- **macOS dev builds show no app icon (2026-10-15)** — `build/main/qlcconsole`
  is a bare binary, so the bundle's `.icns` never applies, and
  `AppUtil::setApplicationIdentity` deliberately skips the app-wide icon on
  macOS (it would override the `.icns` in a packaged Dock). Could set it only
  when not running from a bundle, if it matters.

## Line targets — leftovers (2026-10-17)

P1–P3 built on `feat/line-targets` (DONE.md); not merged yet. If wanted
later (LINE_TARGET_DESIGN.md P4): Spread on a control-surface fader; a
"breathe" effect sweeping Spread on a speed master; dragging line handles in
the Rig Overview; effects that chase along a line. Also: the target menu's
"Lock Target" is undone by the next plot refresh (locks mirror the layer's).

## Release 0.1.0 (in progress, 2026-10-15)

On `release/0.1.0`. Universal (Apple silicon + Intel), macOS 12+.

- **24-hour soak on kronk** (the Intel show machine, i7-9750H) — running
  since 2026-10-15 09:56: the real show, the show plus a synthetic block,
  then rough handling (`stresstest/soak24/`, results in
  `~/soak24/run-20261015-0956/` on kronk). Review before releasing.
- **Rig test pass** — `RIG_TEST_PLAN.md`, section K first (the 0.1.0
  work), on the office rig with kronk.
- Then: merge `release/0.1.0` to main, `./release.sh`.

## Show Tree (feat/show-tree, P1 built 2026-10-16 overnight, P2 and P3 2026-10-17 — not merged)

[SHOW_TREE_DESIGN.md](SHOW_TREE_DESIGN.md) (D1–D27 approved; §10 has the
implementation notes and the questions below). Built: the hand-over fix,
the engine model / cascade / bake / undo / import, the Show Tree tab and its
help (DONE.md 2026-10-16). Handoff: SHOW_TREE_OVERNIGHT.md.

- **Fix pass 2026-10-17 done** (DONE.md): preview of the selected cue in
  Design mode, several groups in one layer, Library group folders, gobos by
  name per fixture type.
- **Follow-up 2026-10-17 done** (DONE.md, §10.9): in Design the edited cue
  wins over any list running for real (Perform: the list owns the stage);
  gobo names match in tiers, ambiguous → not guessed, pick offered.
- **Open for Branson** (§10): `Generated` mark on `<Function>` too?; undo
  shared with Programming?; an effect's own Gobo (out of scope so far);
  DanceHysteria's Hold-0 cues 0.0.4–0.0.7 auto-follow — intended?; **Blind
  and the preview — to discuss**.
- **Try on the rig:** the Design-mode preview while editing (and that Blind
  mutes it), also over a list running for real (edit a dimmer down under a
  running cue at full; stop the preview: the cue comes back); GO through an imported list with effects running (the
  hand-over keeps a shared scene's effect phase — tested on engine signals,
  not seen on stage).
- **P2 done 2026-10-17** (DONE.md, §10.10): tracking controls in the tab
  (list mode, base cue, cue modes, Release on any layer, includes with loop
  refusal, "Next cue receives" with values), the re-bake chain (tested
  against what a bake really changes), "Update cue X" and Record as new cue
  (D22/D23), Library "In this cue list" (D20), the optional command line
  (D24). **Try:** import a show's chaser on a copy, make the list
  Tracking, release / base / remove repeated layers by hand (P4 brings
  Convert to tracking), and GO through it; the command line (Command).
- **P2 answers done 2026-10-17** (DONE.md, §10.11; open 9–11 resolved):
  Record asks selected / running / everything lit when they differ, with
  per-mode defaults in Settings → Show Tree; several groups = one layer
  everywhere, a layer each with ⌥ or a comma, Split / Merge layers; a typed
  or picked value reuses the palette playing it, the command line makes a
  named one per the show's pattern, the picker's one-off has one-click Make
  reusable. **Try:** Record while a list runs (Perform and Design), merge
  two layers and GO through the cues after.
- **Gaps from the help done 2026-10-17** (DONE.md, §10.12): Merge by mouse
  (multi-select), D13 enforced outside the tab, one list rule (new =
  Tracking), one undo step per action audited. **Open:** Detach to plain
  *chaser*. **Try:** ⌘-click two layers → Merge; open the list's chaser in
  the Functions window (locked, Show in Show Tree).
- **P3 done 2026-10-17** (DONE.md, §10.13): group sets (Make set, by
  name everywhere, laid out by position), child layers and overlap with
  `Scene::excludedFixtures` (no HTP show-through; an import keeps mixing),
  cue-level effects as one across layers (D10), line aims in the tree with
  Spread / Grouping / Cross rows, one formation by hang position (D16–D19);
  plus rename in place (Return/F2), ⌘C/⌘V/⌘D cues and layers, `help` in the
  command line. **Open (§10 "Open for Branson" 12–16):** D10/D17 built as a
  shared layout span rather than one extra scene; the plan view (top view
  with the previous cue's rings) not built — the order strip is; a Mix
  cue's overlap in the Design preview; a child's Dimmer on no-dimmer RGB
  fixtures; sub-cue numbering for paste waits for P6. **Try:** make Towers
  from two groups, drop it with ⌥; ⇧-drop a group on a layer, set its
  dimmer down; drop a line target on a cue with two layers and slide Spread
  from the cue's Line ⋯; type `help`.
- **Show Tree command line: plain-language prompts** (Branson, 2026-10-17;
  not built). Two layers, phrase layer first. (1) Offline phrase layer
  (~2–3 days): synonyms/filler words mapped onto the existing grammar —
  'make the towers blue at half', 'all the movers', colour names, 'at
  half'→50 — always available at a show, deterministic. (2) Optional AI
  layer when online (~3–5 days): prompt + the show's names (groups,
  palettes, cues) sent to Claude, which returns ordinary command lines;
  shown for confirmation, Return runs them through the same op layer, one
  undo step each; API key in Settings; handles 'warmer', 'like cue 3
  but…'. Not show-time (needs internet).
- **P4 next:** Detach to plain chaser, Convert to tracking, Make reusable
  layers/cues.

## Needs the real rig / hardware

- **Re-check the Oppsk wall-washer readings (2026-10-16)** — the 24 bars in
  DanceHysteria-0.4 carry Full 12.3 A / Half 2.7 / Standby 0.4 (White 6.6,
  R/G/B 2.3) against a definition rated 100 W: at 120 V that is 1.48 kW a
  bar at full and 48 W dark, 9.6 A for the 24 doing nothing. A DC-side or
  whole-chain reading would explain it; re-measure one bar on its mains lead.
  The V-Show PL-4003 definition has no power at all (now guessed at 50 W).

- **Rig test pass** — run `RIG_TEST_PLAN.md`, fix any ❌ (the gate on the
  freeze list).
- **WLED on a real device** — Branson tested some 2026-10-11: hit or miss,
  probably the WLED controller rather than the interface; needs more
  testing. Still to do: the live view included;
  effect / palette names read from the device (`/json/eff`, `/json/pal`);
  160 WLED effects ported, written from behaviour (WLED is EUPL -- never
  from FX.cpp): every 1D effect, the 23 sound-reactive 1D ones on the audio
  input; not ported: the 2D ones, Copy Segment, Image, Slow Transition.
  Compare the ports side by side with a real device -- Percent's share
  (intensity/255, from the docs' "% of fill") especially.
- **Control surfaces** — PMJ (OpenDeck) mapping as the show's operating
  surface; the control-surface engine (PMJ + APC40 mk2 + Xbox) was last
  "in progress, at the rig".
- **Fixture Test on an ADJ Focus Spot Three Z** — reported not lighting;
  the code looked right, root cause never confirmed (DONE.md, 2026-09-05).
- **Verify what shipped without a rig check** — DONE.md marks each such
  entry "not yet Branson-verified"; the lists from 2026-09-05 to 2026-09-26
  in particular. **Verified on the rig 2026-10-10/11:** a real step, front
  and top fixtures, with the power management; two-arm moving heads. The
  rest isn't rigged yet.
- **Hardware checks from the Connections work** — see "Connections tab —
  remaining" below (off-segment unicast probe, a hand-declared target
  actually passing DMX, reachability on `ender`).

## Backlog — asked for, not started

Effects and playback (from Branson, 2026-10-11, in this order; scope each
before building):

1. **The spread layer, with speed masters and Link** — S1 beat clock, S2
   speed masters, S3 spread layer **done** (DONE.md 2026-10-11; demo in
   test-workspaces/speed-spread-demo.qxw). **S4** control-surface
   automapping (a Speed page: masters on faders, taps, beat LEDs; PMJ
   `Favorites` → Tap) is **backlogged until Branson is at the surface**
   (2026-10-12) -- but each master's Tap / Rate / Pause can already be
   mapped by hand (Speed Masters → Inputs..., 2026-10-12), and the masters
   sit in the footer; S4 is the automatic page on top of that. **S5** Ableton Link deferred (GPL licence).
   [SPEED_SPREAD_DESIGN.md](SPEED_SPREAD_DESIGN.md).
2. **Field generators sampled at positions, plus fire** — generators that
   are a field over the stage, each fixture taking its value from where it
   stands on the plot; fire as one of them.
3. **Layers, blend modes and masks** — effects stacked in layers, with
   blend modes between them and masks to limit where one applies.
4. **Beat-quantized Swop** — GO on the beat is done (a chase or cue list
   holds a GO to the next beat or bar); Swop still lands the instant it's
   pressed.
5. **Keyframe shapes and spatial transitions** — the shape of a move
   between keyframes, and transitions that sweep across the rig spatially
   instead of everything changing at once.

- **Agreed next (2026-10-13/15), after 0.1.0:**
  - **Sub-cues** ([SUBCUES_DESIGN.md](SUBCUES_DESIGN.md)) — P1 (engine)
    and P2 (editor, VC cue list, footer, ⌥⌘↩) built on `feat/sub-cues`
    (DONE.md). Still to do: P3 show control (`/cue/4.2/go`, `/subgo`,
    `/subback`, MSC GO 4.2, LOAD 4.2 — after `feat/connections-showcontrol`
    merges; the engine API is there: `Chaser::findCueOrSubCue()`,
    `jumpToSubCue()`, `subGo()`, `subBack()`), P4 step-based effects on Sub
    GO.
  - **Sending to QLab** — OSC / MSC out. Its home is ready: the "QLab out"
    section of Connections → Show control & sync (`ShowControlSyncPage`).
  - **Duplicate a structure's setup** (Branson, 2026-10-15; replaces the
    one-off "tower patch rule") — from one structure (T1) and the fixtures
    on it, set up its likes (T2, T3…): the same fixtures placed and mounted
    the same way, matched by their labels, and addressed. The addressing
    half exists (right-click → Patch Others Like This / the Patch dialog's
    Match); the placement half is the new part. Design first.
  - **Windows build** — port upstream QLC+'s `build-windows` CI job
    (MSYS2/MinGW) with qlcconsole names; installer `.exe` as an artifact.
  - **WLED** — the rest of the 1D effects (Fireworks Starburst, Percent,
    Noise 1–4, Perlin Move, Meteor Smooth…), from behaviour only (WLED is
    EUPL).
  - **Playback timing** (PLAYBACK_TIMING_DESIGN.md): P2 fade shapes, P3
    per-chase-step transitions, P4 Swop, P5 clickable Load chip.
  - **Rig Overview frame rate** — maybe automatic (30, lower when the
    engine is busy): the overview now draws on every core, which competes
    with the DMX engine on a 6-core machine. Decide from the soak.
- **Stage and inventory across show files** — keep a stage layout
  (structures, platforms, scenery) and the hardware inventory (fixtures,
  network gear, their patch) outside any one show, and bring them into a
  new show instead of copying a .qlcc and stripping it.
- **Stress test across Mac / Linux / Windows 11** — Branson can lend the
  machines; plan the matrix first: 70–80 universes, a soak with the machine
  kept awake (`caffeinate`), and the new paths (effect runner, WLED/JSON,
  rig view at full rig). The 2026-10-10 overnight run was cut short when
  the Mac slept.
- **One object editor** — merge Fixture Properties and "Edit Fixtures in
  Studio" into one: a tree of the object's fixtures by group, add by
  right-click or drag, multi-select → fixture group, distribute / put on
  face. Design options first.
- **Fixture Editor inside the main window** — not started; needs its own
  investigation (DONE.md, "Fixture Editor integration into the main window").
- **Post-freeze:** cue transition 4b (hold-on-miss + release-on-transition);
  "Look" as an assembly unit; pre-positioning / mark cues with a ghost
  visual.
- **Timecode slice 3** — auto-fill the internal latency; **audio
  calibration** — an active loopback self-test; **clickable Load chip** — a
  per-function breakdown.

## Ideas — not started

### no-light zones -- warn when a look lights the audience

Branson, 2026-09-17: "if we create a scene with lights that point an area that
the light SHOULDN'T be in .. audience faces .. that should be a warning. The
latter thing can be time gated.. so sweeps or strobes for blinders can be ok."

**Shape.** A zone is an AREA of the room rather than a surface: a footprint on
the plot plus an eye height (1.2 m sitting, 1.7 m standing). Drawn like an Area
target -- outline from above, a band in the elevations -- on its own layer.

**The warning.** A scene warns when a beam RESTS on the zone. Everything that
crosses it in passing is fine: a sweep, a strobe, a blinder hit. The proposed
rule is a dwell time -- a beam on the zone for more than about half a second
warns -- which needs the aim to be evaluated over the look's own movement
(effects included) rather than at a single instant. Where it lands lives with
the reach work: AimSolver::sceneAims() already resolves aim per fixture, and
StructureStudioView::beamFootprint() already cuts a cone against a surface.

**Open questions.** The dwell threshold (0.5 s a guess, not measured against a
real cue). Whether a zone is a new object or StageTarget::Kind::Area with a
"no light" flag. Whether the warning belongs on the scene row in Programming
(next to "can't reach"), on the plot, or both. Whether house lights and
audience blinders are exempt by fixture group.

---

### people as a stage feature -- whole choirs on a layer

Branson, 2026-09-17: "multiple people on the stage .. can we create "people" as
a feature type so that we can have a layer with say the entire choir in
different positions on the stage for visualization."

**What exists.** A StageTarget of kind Person already draws a lofted figure in
the rig view (lit by the beams on it) and a silhouette in the plot, stands on
the deck under it, and is sized by the follow-spot subject height. But a target
is an AIM POINT: making forty of them to show a choir clutters the target list,
the Aim palette picker and the aim warnings.

**Shape.**
  - A "people" feature separate from targets: position, height (or child /
    adult / seated presets), optional name/part (S/A/T/B), facing.
  - Placed in bulk: a grid or arc across a riser row, snapping to decks the way
    fixtures deck-mount; drag as a block.
  - On layers, so "Choir -- concert", "Choir -- processional" and "Band" can be
    swapped for visualization without touching the rig.
  - Any person can be promoted to (or linked with) a target when a follow-spot
    needs to track them.
  - Lit like the Person figure is today; worth measuring rig-view frame time
    with 40+ figures (each is ~300 shaded faces).

---

### an MCP server for qlcconsole

Branson, 2026-09-10: "shower thought .. can we consider an MCP for qlcconsole ..
so that we can do this kinda thing inline?"

**The case for it, from this session.** Three rounds of bulk edits were made to
`stage-structures-demo.qxw` by parsing and rewriting XML with regex, and the
schema was guessed wrong in two of them:
  - `<Head Fixture="36" Head="0"/>` looked entirely plausible and loaded as an
    EMPTY group. The real format places every HEAD on an X/Y grid with the head
    index as the element's text -- `US1t` is 16x2 = 32 entries for 2 fixtures.
  - A verification pass compared fixture heights to an absolute 0.204 m and
    reported 16 false failures, because the steps are STACKED and each has its
    own `platformBaseZ()`.
Both were guesses at a schema the engine already knows exactly.

**Shape.** A small `tools/qlcmcp` binary linking `libqlcplusengine`, speaking
JSON-RPC over stdio. It loads a workspace through the REAL engine and exposes
thin wrappers -- `list_fixtures`, `list_groups`, `list_structures`,
`create_group`, `delete_group`, `set_fixture_placement`, `assign_to_group`,
`save`. Because saving goes through `Doc::saveXML()`, there is no format to
guess and nothing to verify afterwards: that whole class of error disappears.

Feasibility is already demonstrated -- `monitor_test` loads real workspaces
headlessly through `Doc::loadXML()` with the fixture-definition cache, which is
most of what the server needs.

**Caveats:** a real project, not an afternoon. Needs care about editing a file
the running app has open (or a "read-only unless told otherwise" default). Check
whether the existing `webaccess/` module is a better host than a new binary
before starting. Useful to Branson directly too, not just to tooling: scripted
rig edits, bulk repatching, cross-show consistency checks.

---

## 🔌 Connections tab — remaining

*(Shipped work moved to [DONE.md](DONE.md), 2026-08-28.)*

- **Hardware verification — status.** Use `build/tools/artnetprobe/artnetprobe`
  (`--selftest` / `--poll <addr>` / `--listen`).
  - ✅ **Arrival-interface reporting**: verified 2026-08-28 against live
    traffic. Real ArtPollReplies from 172.18.2.218 and 172.18.2.10 were both
    correctly attributed to `vlan0`, 5/5 datagrams carrying an index. This is
    the routing fix confirmed on real gear, not a simulation.
  - ✅ **USB devices appear**: DMXKing ultraDMX Micro and OpenDeck PMJ_BLACK_1
    both listed under DMX USB / MIDI.
  - ⚠️ **Off-segment unicast probe**: unproven. Every node reachable from this
    host is on a subnet it has an interface on, so it answers broadcast polls
    too and the reply is indistinguishable. Needs a node on a subnet with no
    local interface.
  - ⚠️ **Hand-declared target actually passing DMX**: unproven, and NOT
    testable from one host. Console and probe both bind 6454 with
    `ShareAddress`; broadcast reaches every bound socket but unicast reaches
    exactly one, so the probe cannot see the console's own unicast output.
    Run the probe on a different machine from the console.
- **Devices/Overview parity — done.** Bulk multi-row retarget with port
  auto-increment, feedback IP/port, ArtNet `inputUni`, MIDI output mode, MIDI
  input channel and the MIDI Out-vs-Feedback role swap have all landed on the
  Devices tab. Universe passthrough and the plugin
  description/status have since moved onto Devices too. Still Detailed-only
  and staying there deliberately: the Audio tab, input-profile
  creation/editing (`InputProfileEditor`), and the USB hotplug toggle (an app
  preference more than a patching control). The split now reads as
  "Devices = the rig and its wiring, Detailed = audio and profile editing".
- **Patch undo — remaining limits.** `PatchUndo` (engine/src/patchundo.{h,cpp})
  is ONE step deep: a second change discards the first, deliberately, since a
  state two changes stale would restore onto a rig that has moved under it.
  It covers patches and the universe list (add/delete/name/passthrough) from
  all three tabs, but nothing outside the patch — fixtures, functions, scenes.
  Ctrl+Z is scoped to the Connections widget (`Qt::WidgetWithChildrenShortcut`)
  so it cannot promise general undo elsewhere in the app.
- **`InputOutputMap` universe id vs index — not a live trap after all.**
  `universe(id)` resolves by ID while `outputPatch()`/`setOutputPatch()`/
  `setInputPatch()` take an ARRAY INDEX, but id == index is enforced
  deliberately at both mutation points, not merely coincidental:
  `addUniverse()` assigns the next index as the id, refuses an id already
  present, and fills gaps so a higher id still lands at its own index;
  `removeUniverse()` refuses anything but the last entry. The contract was
  written nowhere, so it is now pinned by
  `InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex()` — relax either
  rule and that test fails instead of the app repatching the wrong universe.
- **Reachability probing — verify on `ender`.** Rescan now unicasts an ArtPoll
  at every address believed in but not heard from (hand-declared targets and
  addresses named by `outputIP`), so a node on another subnet can answer. Never
  tested against a real off-segment node. Note it probes only on an explicit
  Rescan, not on the 5 s tick — a live "is it up" indicator would need a
  cadence decision, and continuous unicast polling of every configured node is
  exactly the traffic this has been avoiding.
- **Probing is Art-Net only.** `QLCIOPlugin::probeTarget()` defaults to a
  no-op; nothing else implements it. Fine today, since Art-Net is the only
  plugin with addressable targets at all.

---

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Build host: Raspberry Pi 4 (TODO, 2026-08-25)** — the highest-value box to
  add, because it answers an open question rather than just adding a build:
  1. **Tick punctuality.** 51 universes at `transmitMode="Full"` on a 14-core
     M-series logged 2 `MasterTimer is running late` events in 61 s at only
     50% of one core (see the open item above). Four weak ARM cores is where
     that either holds or doesn't. This is the measurement still outstanding.
  2. **Different ABI.** ARM makes plain `char` *unsigned* where x86 makes it
     signed, plus stricter alignment. DMX values are compared as `char` in
     places (e.g. `QCOMPARE(ua.preGMValues()[0], char(255))` in
     `ui/test/vcxypad/` — `-1` on x86, `255` on ARM). No amount of x86 testing
     finds that class.
  3. **Real target.** Upstream QLC+ ships Raspberry Pi builds; Pi-based
     lighting controllers are common.
  Build 32-bit (armhf) and you also get `qsizetype` as 32 bits — the width
  assumption behind the `%d` format bug fixed in `mastertimer.cpp` this session.
  Setup mirrors 192.168.1.245: cmake, Qt dev packages, xvfb, python3-lxml.

- **Build host: Debian 12 guest (TODO, 2026-08-25)** — a *newer* toolchain
  bound. CI is Ubuntu 22.04 (GCC 11, glibc 2.35, Qt 6.8); 192.168.1.245 is
  Debian 11 (GCC 10, glibc 2.31, Qt5). Bookworm gives GCC 12 + `qt6-base-dev`,
  which is the only combination not currently covered anywhere: Qt6 **and** a
  compiler newer than CI's. Deliberately NOT another Ubuntu 22.04 box — that
  duplicates CI exactly and would never find anything. The value of
  192.168.1.245 was proven the day it was built: its older glibc exposed a
  pthread linkage bug (`4a360911c`) that Ubuntu 22.04 structurally cannot see.

- **MasterTimer misses ticks at full output load (2026-08-25, 2026-09-02) —
  fixed on macOS, ROUGHED IN elsewhere, NOT YET VERIFIED on Linux/Windows.**
  With 51 universes forced to `transmitMode="Full"` on `ender` (~2484 pkt/s,
  ~49 Hz per universe) a 61 s run logged **2** `MasterTimer is running late`
  events while a 91 s run at the same settings logged **0**. CPU was only 50%
  avg / 64% peak of one core, so this is not throughput saturation — it's
  scheduling jitter, and it's the same wall-clock sensitivity that made
  `MasterTimer_Test::interval()` and `VCCueList_Test::functionRemoved()` flaky
  on CI. Reproduce with `/tmp/throughput.sh` on ender (see DONE.md 2026-08-25
  for the harness).

  **macOS/iOS — fixed, and previously validated** (`feat/realtime-timer-thread`,
  ported from the `rt-test` branch, `8487dbf71`): the DMX timer thread now
  requests Mach `THREAD_TIME_CONSTRAINT_POLICY` (what CoreAudio uses for the
  same problem) in `engine/src/mastertimer-unix.cpp`. Advisory — warns and
  falls back to normal priority if the kernel refuses. This is the run
  referenced above ("real-time-policy run showed zero events of either
  shape").

  **Linux — same file, `SCHED_FIFO` via `pthread_setschedparam`, ROUGHED IN,
  UNVERIFIED** (no Linux box in that session; CI will give a real compile/link
  result on push). Priority is `sched_get_priority_max(SCHED_FIFO) - 10`, not
  max, matching the JACK/PipeWire convention of leaving headroom above this
  thread. **Known limitation, not solved here:** `SCHED_FIFO` normally needs
  `CAP_SYS_NICE` or an rtprio limit, which a stock install will not have — so
  on most Linux installs this falls back to a warning + normal priority
  exactly like an mac refusal would. Packaging a fix (setcap in a postinst,
  or a bundled rtprio limits.d rule) is a separate, bigger decision — not
  done, flagged as its own backlog item below.

  **Windows — MMCSS ("Pro Audio" thread characteristics), on its own timer
  thread (2026-10-16).** `mastertimer-win32.cpp` used to tick from a
  `CreateTimerQueueTimer` thread-pool callback; once ticks overran (Windows
  soak dry run, STRESS show) the pool started a worker per overlapping tick:
  111 -> 872 threads, 803 MB private, 40 s ticks, and MMCSS refused each new
  worker (565, too many threads). Now one dedicated thread on a
  high-resolution waitable timer (timeBeginPeriod(1) fallback), absolute
  deadlines, re-anchor on overrun like the unix loop, MMCSS asked once and
  reverted at stop. mastertimer_test covers an overrunning tick (one thread,
  one tick at a time, no catch-up burst) and stop() during a tick.

- **Linux: package a way to actually grant SCHED_FIFO (2026-09-02, follow-on
  to the item above, not started)** — the DMX timer thread's real-time
  priority request will silently fall back to normal scheduling on a stock
  Linux install (no `CAP_SYS_NICE`, no rtprio limit). Options to weigh:
  `setcap cap_sys_nice=eip` on the installed binary from a `.deb`
  postinst/AppImage hook, vs. documenting a manual `/etc/security/
  limits.d/qlcconsole.conf` rtprio rule (the traditional pro-audio-Linux
  convention, needs the user in a group like `audio`). Needs a real Linux
  box to even confirm the roughed-in `SCHED_FIFO` call links/behaves as
  written before this is worth deciding.

- **The ARM char-signedness tripwire is commented out (OPEN, 2026-08-26)** —
  the assertion cited as the reason to want an ARM build host,
  `QCOMPARE(ua.preGMValues()[0], char(255))` at
  `ui/test/vcxypad/vcxypad_test.cpp:474-483`, sits inside a `/* FIXME !! */`
  block and runs nowhere. The live `char(...)` comparisons in
  `vcxypadfixture_test.cpp` are signedness-*insensitive* (both sides go through
  the same conversion) so they pass identically on x86 and ARM. The Pi 5
  confirms the ABI is there — plain `char` is unsigned, `__CHAR_UNSIGNED__`
  defined — but nothing in the suite exercises it. **Un-commenting that block
  is the experiment that cashes in on the ARM host existing**; until then the
  host proves nothing about this bug class.

- **Debian 12 setup quirk: Qt6 `lrelease` is not on PATH (2026-08-26)** —
  Debian's `qt6-l10n-tools` installs `lrelease`/`lupdate` to `/usr/lib/qt6/bin/`
  with no `/usr/bin/lrelease-qt6` symlink (unlike the Qt5 packages), and
  `translate.sh`'s `which_qt()` probes only `lrelease`, `lrelease-qt6`,
  `lrelease-qt5` on PATH. Build dies at 1% with "lrelease not found". Workaround
  `export PATH=/usr/lib/qt6/bin:$PATH`; durable fix is to fall back to
  `qmake6 -query QT_HOST_LIBEXECS`/`QT_HOST_BINS` or have CMake pass the
  `Qt6::lrelease` target location down.

- **buildhost-qlcplus is missing 10 custom fixture definitions (2026-08-26)** —
  headed runs there open with an error dialog: no definition found for Branson
  LED Movinghead+Circle, Branson LED SPOT, Betopper LM70, ADJ Focus Spot Three
  Z, Oppsk Wall Washer Light Bar, Warmdance XL-450, Junman Two Arm LED Beam,
  PHS Chorus Step Row 64 Heads, WLED Effect Mode. Same cause as the
  `/tmp/*.qxf` load failures seen on that host. Sync the `.qxf` files to
  `~/.qlcconsole/fixtures/` there before using it for real UI validation.

- **Note: lateness magnitude was already logged before the `late_us` diag**
  — `mastertimer-unix.cpp:67,74` has a pre-existing
  `qDebug() << "Time is late by" << ... << "nanoseconds"` inside `compareTime()`.
  The new line's genuinely new information is **`compute_us` + `budget_us`**
  alongside it, which is what separates jitter from load; the raw lateness
  figure was obtainable already.

- **TRAP: on Linux, a build-dir run loads NO I/O plugins and silently outputs
  nothing (2026-08-25)** — cost a full 10-minute soak that looked like a clean
  pass. `IOPluginCache::load` resolves via `QLCFile::systemDirectory(PLUGINDIR)`
  (`engine/src/qlcfile.cpp:181`), and on Linux that branch is just
  `dir.setPath(path)` with `PLUGINDIR` a **compile-time absolute**
  (`/usr/lib/qt6/plugins/qlcconsole`, or `qt5` on the Debian 11 box). There is
  no env override. macOS is fine because its branch resolves
  `applicationDirPath()/../PlugIns`, which the build tree mirrors — so this
  bites only on Linux and only when running from `build/` without installing.
  The failure is **silent and looks like success**: every universe logs
  `setOutputPatch - plugin: "None"`, the app runs happily, CPU sits at ~0%, and
  a punctuality soak reports **zero** late events because nothing was ever
  transmitted. Check `grep -c artnet` in the run log, or CPU > 0, before
  believing any Linux throughput/punctuality number.
  Workaround for a build-dir run:
  `sudo mkdir -p $PLUGINDIR && sudo ln -sf ~/git/qlcconsole/build/plugins/*.so $PLUGINDIR/`.
  Worth considering a `QLC_PLUGIN_PATH` env override so test rigs stop needing
  root to run what they just built.

- **Output trouble reporting: send health, node health, UI (BUILT 2026-10-15
  — needs the rig)** — started as "ArtNet failure reporting still flaps":
  `e3905a2a1` logged once per state change, and a unicast target that never
  answers ARP flips that state itself (macOS: ~5 s of queued "successes"
  while ARP is out, then 20 s of "host is down"), so the soak still produced
  200 "is failing" + 150 "recovered" lines across 50 universes in 120 s.
  Built on `feat/artnet-status`:
  - **Send health** (`plugins/interfaces/sendhealth.{h,cpp}`, shared by
    Art-Net and E1.31): failing once failures persist 1 s (isolated successes
    don't reset it), at once for a hard error; recovered only after 30 s of
    uninterrupted successes. E1.31's per-packet `qDebug` spam is gone too.
  - **Node health** (`plugins/artnet/src/artnetnodewatch.{h,cpp}`), from the
    ArtPollReplies the plugin already asks for: a node heard this session
    that misses 3 polls in a row is "stopped answering" (back after 2
    answered); the port carrying our universe reporting a DMX short
    (GoodOutputA bit 2), or no longer outputting after it was, is a port
    fault (2 replies in a row; clears after 10 s clean). Broadcast universes
    watch the nodes that output that Port-Address; unicast ones the node at
    that address.
  - **Traffic:** unchanged on-subnet (the existing 3 s broadcast ArtPoll per
    interface with an output). New: one unicast ArtPoll per 3 s to each
    distinct unicast output target OFF the interface's subnet (coalesced by
    address, GUI thread, never in a DMX tick), since those never hear the
    broadcast.
  - **UI:** new `QLCIOPlugin::outputProblems(output, universe)`; footer entry
    `outputs.problems` (one line per problem with the universes it hits,
    **Open Connections**); the universe's Connections row gets the problem in
    red with a tooltip, in both views, refreshed on `targetProblemsChanged()`.
    The footer now listens to every plugin's `targetProblemsChanged()`, not
    only WLED's.
  - **Bug found on the way:** Connections had GoodOutput's short (0x04) and
    merging (0x08) bits swapped, so a merging port showed SHORT DETECTED.
    Fixed per the Art-Net 4 spec / libartnet's `PORT_STATUS_*` — **worth
    confirming against a node that really is merging**.
  - Tests: `artnet_test` 7 → 24, new `e131_test` (5), `connectionstree_test`
    +2 (row + footer), all on injected results/times and fabricated replies;
    nothing is sent.
  **To confirm on the rig:** (1) unicast to an address with nothing on it for
  a few minutes — one "not reachable" line per target in the footer and log,
  no "recovered"; (2) unplug a node that was answering — "stopped answering"
  within ~9-12 s, back ~6 s after it is plugged in, fixtures respond at once
  (reporting never holds output back); (3) short a DMX run on a node port that
  reports it (or pull the terminator if that trips it) — "DMX output short"
  on that universe's row, gone 10 s after it's cleared; (4) pull the
  console's own cable — "not reachable" within 1-2 s, gone ~30 s after it's
  back; (5) a node on another subnet: confirm it now appears and is watched
  (unicast polls), and that nothing on the show segment minds a poll every
  3 s; (6) a healthy rig over a long run shows and logs nothing; (7) check
  a merging port no longer reads SHORT DETECTED.
  Not built: E1.31 has no per-receiver reply in the base protocol. E1.31
  Universe Discovery packets only list the universes a *source* sends, so
  they say nothing about receivers either -- noted, not worth building.

- **`VCCueList_Test::functionRemoved()` flake — watch, don't assume fixed
  (OPEN, 2026-08-25)** — failed on 2 of ~9 macOS CI runs asserting a tree row
  count right after a 100 ms deferred refresh. `6d3757bb2` switched it to
  `QTRY_COMPARE`, which polls instead of sleeping a guessed interval. NOT
  proven: it passed 7 of 9 runs *before* the change too, and the failure could
  not be reproduced locally even under six spinning cores. Sustained green
  across many runs is the only evidence that counts. If it returns, the fix was
  insufficient rather than wrong. Five other UI test files still use fixed
  `QTest::qWait` and were left alone — none has failed.

- **Windows build back in CI (OPEN, 2026-10-15)** — `fb9c5fd74` had deleted
  `build-windows` (it died seding the pre-rebrand `qlcplus4Qt6.nsi`).
  Re-added: MSYS2 UCRT64 + MSYS2's own Qt 6 packages (6.11 at the time; GCC
  16), NSIS installer `qlcconsole-<VERSION>-<date>-<rev>-win64-setup.exe`
  uploaded as an artifact, `--version` smoke test run from the deploy dir with
  MSYS2 off PATH. Fixed on the way: avrt.h include order, `near` (a windows.h
  macro) as a variable name, PreviewItem typedefs, and -- the important one --
  signals named by member pointer across the engine/UI DLL boundary silently
  never connecting under MinGW (`qlcengine_export.h` / `qlcui_export.h`; a new
  class whose signal another module connects by `&Class::signal` needs
  `QLC_ENGINE_EXPORT` / `QLC_UI_EXPORT` too). Left open:
  1. Nobody has installed it on a real Windows machine -- rig view on D3D11,
     audio, DMX USB/D2XX and the per-user file associations are unverified.
  2. ~95 MB installer, mostly ffmpeg codec DLLs pulled in by Qt Multimedia's
     ffmpeg backend; trim if it matters.
  3. Tests: all 142 binaries pass on Windows CI since fix/win-tests-and-nits
     (run 38938082831); the step is still informational
     (`continue-on-error`) -- make it fail the job once a few more runs stay
     green. What the failures were: UI tests' QSettings() had no
     organization name, and Qt's registry backend keeps nothing without one
     (addfixture, functionselection, monitor, programmingguide,
     speedmasterspanel) -- every UI test now gets an INI file of its own
     (ui/test/common/testsettings.cpp); USERPROFILE not set alongside HOME
     (showembedding, showdifferences); mixed '\' separators in system
     fixture paths (qlcfixturedefcache) and a test splitting on them
     (rgbscript); a read-only folder Windows ignores (networkreportdialog).
     lookeditor's segfault (exit 139) went away with the settings change;
     its exact cause was not pinned down.
  4. Not code-signed (SmartScreen will warn). No `-Werror` on MinGW, by design.
  5. `QLCIOPlugin` signals are connected by member pointer from the UI too, but
     its moc lives in each plugin DLL as well as the engine; not annotated.
  6. First install on a Windows 10 VM (2026-10-15), Branson's report and what
     was done: unreadable buttons/combos/spin boxes on a dark theme (the
     native windowsvista style ignores the palette's Button colour) -> Fusion
     off macOS (main.cpp) plus theme-derived shade roles (App::applyTheme);
     invisible splitter handles -> same; "Win+Shift+R does nothing" -> it is
     Ctrl+Shift+R, now also in the Window menu, and the help spells keys as
     Ctrl/Alt/Shift off a Mac (HelpBrowser::keysForPlatform); sluggishness ->
     Art-Net/E1.31/OSC walked every network interface (~20 ms each on
     Windows) on every outputs()/inputs() call, now at most once a second;
     also fewer repolishes/registry reads on the status-bar timers, NoProxy
     for WLED, and App::nativeEvent fixed for Qt 6's qintptr* (USB hotplug
     never reached HotPlugMonitor). Still to confirm on the VM: the main
     window's outer edges resizing, and whether it still feels slow.
  7. "Rig Overview wireframed in red, no light" on the VM (GPU and CPU alike):
     not Direct3D -- the VM had none of DanceHysteria's own definitions, so
     those fixtures stood in as generic dimmers, 300 mm per channel: a
     64-channel bar was a 19 m wall of heads in the overlap-warning red. A
     stand-in now draws as one small box with no overlap warning; the footer's
     "no definition on this computer" error names the folder to copy the
     .qxf files into, and Fetch Definitions rereads it. Windows CI now runs
     the rig view on D3D11 (WARP): GPU-vs-CPU matches as on Metal/GL, and the
     pictures are kept (rig-shots-windows artifact).
  8. Windows "slow ticks, 640% CPU" on the STRESS show (2026-10-16, VM, 8
     vCPU i7-10700K, headless, outputs stripped). Stack-sampled (a ctypes
     SuspendThread + dbghelp StackWalk64 sampler; nothing installed). The
     build measured, da40149e5, predated d5f527e35: the 16 busy universe
     threads spent ~40% of a core each formatting Universe::writeBlended's
     per-tick qDebug (QTextStream/QLocale/toLocal8Bit -- dear on MinGW),
     holding their fader locks, so the timer thread sat in
     Universe::requestFader's mutex: 68 ms mean tick (94% over), 660% CPU.
     release/0.1.0 (d7fe56924) already ran it at 7.9 ms / 0.3% over / 300%
     CPU -- but leaked ~5000 kernel handles a second (3 M handles, +300 MB in
     10 min): MinGW's std::recursive_mutex is never destroyed and winpthreads
     gives each one an Event. GenericFader's lock is QRecursiveMutex on
     Windows now (fix/win-tick-perf): handles flat at ~1600, private bytes
     flat, 8.1 ms / 0.2% over / 287% CPU. What's left is ordinary engine work
     (Scene::processValue, the universe threads' GenericFader::write); Linux
     on ender does the same show at 2.3 ms / ~165%.

- **Warning backlog outside the `-Werror` set (OPEN, 2026-08-25)** — a
  tree-wide `-Wall -Wextra` sweep reports ~1100
  `-Wunnecessary-virtual-specifier`, 92 `-Wdeprecated-declarations` (all in
  Homebrew's OLA headers, not ours), 38 `-Wnon-c-typedef-for-linkage` (e.g.
  `PreviewItem`) and 20 `-Winconsistent-missing-override`. None is in the set
  CI enforces and most are third-party. Noted so the next person doesn't
  rediscover them and assume they're new.

- **Rig fidelity test not done (OPEN, 2026-08-25)** — throughput was measured
  at the NIC with tcpdump, which proves what the console *transmits* but not
  that a node *accepts* it. Branson has a single 4-universe node available;
  the full rig is in storage. That's the test that would confirm packet
  well-formedness end to end. `RIG_TEST_PLAN.md` as a whole remains
  unexercised.


- **Output ENDPOINT reachability check (MOSTLY BUILT 2026-10-15, see "Output
  trouble reporting" above)** — node liveness is now watched continuously
  from the ArtPoll the plugin already ran every 3 s, rather than the
  config-time probe sketched below; only nodes heard this session are judged,
  which answers the cry-wolf concern. Still open from this item: a check at
  load/patch time for a unicast target that has NEVER answered. Original
  notes: the
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
  6454.

  **Promoted off the back burner (2026-08-25, Branson):** warn the operator at
  *config* time when a patched unicast target is down, rather than only when
  someone is already chasing a dark universe. The scope is the same as above
  (unicast targets on directly-attached subnets; broadcast degrades to
  "interface up"), but the driver is now setup ergonomics, not debugging.

  **The constraint that makes this non-trivial — poll load must scale with the
  patch, not with the universe count.** This desk routinely patches 50+
  universes (the `ender` soak ran ~53 across 13 node addresses at
  `172.18.2.201`–`.230`). A naive per-universe probe would emit 50+ ArtPolls
  where 13 would do, on a segment already carrying ~2484 pkt/s of DMX — and
  the item above ("MasterTimer misses ticks at full output load") is an open
  question about *scheduling jitter* on exactly that path. Probe traffic that
  perturbs the thing being measured is worse than no probe. So the heuristics
  are load-bearing, not polish:
  - **Coalesce by destination IP, not by universe.** 13 nodes = 13 probes,
    regardless of how many universes each carries. Fan the result back out to
    every universe sharing that target.
  - **Probe off the MasterTimer thread**, and never inside a tick. This must
    not be able to cost a DMX frame.
  - **Config-time and on-demand, not a continuous poller.** On workspace load,
    on patch change, and on an explicit operator "check outputs" — not a
    background heartbeat. A permanent poll across 50 universes is precisely
    the "desk that cries wolf" / spam failure mode this whole area already
    burned itself on once (the 2850-line `sendDmx` flood).
  - **Backoff + hysteresis, shared with the ArtNet flapping item above.** A
    node that is down stays down between probes; don't re-report. The
    "don't announce recovery until it has held for N" rule wanted there is
    the same rule wanted here — build it once.
  - **Bounded concurrency + a short overall deadline**, so a rig with every
    node in the trailer still finishes the check promptly instead of hanging
    the load path on 13 timeouts.
  Open: whether the result surfaces as a per-universe column in the I/O map,
  the existing readiness indicator (`daeeb97d7`), or a footer chip. Probably
  the first — it is a per-patch-row fact.


- **Verify node IP setting (ArtIpProg) on a real node (2026-10-16)** — built on
  `fix/artnet-unconfigured-nodes` (see DONE.md 2026-10-16), tested only against
  a sink. On the rig: a factory-fresh node, Connections → **Rescan** (does it
  appear amber? if not, add the alias the docs give and Rescan again) →
  right-click → **Set node IP address…** → Ask the node, then Apply. Watch
  with `tools/artnetprobe --listen` from another machine if it misbehaves.
  Open: whether the limited-broadcast poll really leaves by the named
  interface on macOS/Windows (IP_PKTINFO), and whether nodes answer
  ArtIpProg from the old or new address (both are accepted).

- **Art-Net node configuration (ArtAddress) — send path built and proven, but
  no node here applies it (2026-08-26)** — `artnet-config.py` at the repo root
  builds and sends ArtAddress (OpCode 0x6000); the plugin defines
  `ARTNET_ADDRESS` and has never used it. Phase 1 (read/browse) shipped; this
  is the write half.
  Packet correctness is established two ways: the CR041R **acknowledges** a
  short-name write by moving its NodeReport to `0006` (`RcShNameOk`), and OLA
  logs `ArtNet got unknown packet 6000`, i.e. it read the header and opcode and
  simply does not implement ArtAddress.
  **But nothing applies.** Confirmed across a real power cycle of the CR041R:
  the name came back `CR041R_001`, not the value written. This node acks and
  discards -- it is front-panel configured, and its Status1 port-address
  programming-authority bits never say "set by network". OLA has no ArtAddress
  support at all. **Validating that changes stick needs a node that implements
  network programming**; until then the write path stays out of the UI.
  Two protocol notes worth keeping:
  - The **NodeReport code is how a node acknowledges a config write** -- the
    advertised fields may not change even on success, so diffing them is not a
    test. Watch the code (`0006` RcShNameOk, `0007` RcLoNameOk).
  - The NodeReport **counter does not reset on reboot** on this node (8943 →
    31425 → 31733 across a confirmed restart), so it is useless as a power-cycle
    indicator. The report *code* is the reliable signal.
  Also: `NetSwitch`/`SubSwitch`/`SwIn`/`SwOut` are only acted on when bit 7 is
  high (program 7 as `0x87`); `0x00` means "reset to zero" and `0x7f` means
  "leave alone", so `0x7f` -- not `0x00` -- is the safe default for a field you
  do not intend to change.

- **RDM configuration tooling — DMX-Workshop-class rig setup (NEW, 2026-08-25,
  Branson)** — make the desk the thing you use to *set up* the rig, not just
  drive it: discover devices, read what they are, and set DMX address /
  personality / inverts from the console instead of climbing to every fixture's
  menu or carrying a laptop with DMX-Workshop on it.

  **This is an extension, not a green field — check what's already here first.**
  Upstream QLC+ shipped "Preliminary RDM support" (`9050a7f13`) and it is still
  wired in:
  - `plugins/interfaces/rdmprotocol.{h,cpp}` — a real E1.20 packetizer/parser
    with the standard PID table already defined (`PID_DEVICE_INFO`,
    `PID_DMX_START_ADDRESS`, `PID_DMX_PERSONALITY`(+`_DESCRIPTION`),
    `PID_IDENTIFY_DEVICE`, `PID_DEVICE_LABEL`, `PID_PAN_INVERT` /
    `PID_TILT_INVERT` / `PID_PAN_TILT_SWAP`, `PID_LAMP_HOURS`,
    `PID_SENSOR_VALUE`, `PID_SUPPORTED_PARAMETERS`, …). The protocol layer is
    NOT the missing piece.
  - `QLCIOPlugin::RDM` capability + `sendRDMCommand()` / `rdmValueChanged()`,
    implemented by **both** transports this rig actually uses: `artnet`
    (`ArtNetController::sendRDMCommand`) and `dmxusb` (Enttec DMX USB Pro).
  - `ui/src/rdmmanager.{h,cpp,ui}` (~900 lines) — a discovery worker thread +
    UID list + raw get/set-a-PID panel, reachable from Fixture Manager
    (`fixturemanager.cpp:457`).
  So the gap is **workflow, not plumbing**. Today it is a protocol inspector:
  it can talk to a device if you already know which PID you want. DMX-Workshop
  parity means turning it into a rig-setup tool.

  Wanted (roughly in value order — needs its own design doc + a decision on
  where it lives, since Fixture Manager is already crowded):
  1. **A device table, not a UID list** — one row per discovered device with
     manufacturer / model / label / current address / footprint /
     personality, populated by auto-`GET`ting the obvious PIDs on discovery
     rather than making the operator issue each one.
  2. **Editable address + personality in place**, with **overlap detection**
     across the discovered set (the actual reason people open DMX-Workshop:
     "who is sitting on top of whom"). Changing personality changes footprint,
     so re-check overlaps after a personality set.
  3. **Identify** as a first-class button per row (`PID_IDENTIFY_DEVICE`) —
     the "which physical unit is this?" loop, and the single most-used RDM
     feature on a call.
  4. **Reconcile against the patch** — match discovered devices to patched
     `Fixture`s and flag the three mismatches that ruin a focus session:
     patched-but-not-present, present-but-not-patched, and
     address-differs-from-patch. Offer "push patch → rig" and "pull rig →
     patch". This is where it stops being a generic RDM tool and becomes
     *this* console's, and it ties into `RIG_TEST_PLAN.md` /
     `SHOW_LIFECYCLE_DESIGN.md`'s Test-Validate phase.
  5. **Sensors + lamp hours** (`PID_SENSOR_VALUE`, `PID_LAMP_HOURS`,
     `PID_DEVICE_HOURS`) — maintenance readout; low priority, easy once (1)
     exists.

  **Hardware finding (2026-08-25) — the ArtNet node on hand does NOT answer
  RDM, so this is currently BLOCKED on hardware.** Probed `172.18.2.10`
  directly from the show VLAN (this Mac is on `vlan0` as `172.18.2.17`):
  - Node identifies as **`CR041R_001`** / `CR041R` — a **4-port** ArtNet→DMX
    gateway, ArtNet 3, `net/sub 0/0`, `swOut 0,1,2,3`, OEM `0x0022`,
    ESTA `0x707A`.
  - It answers **`ArtPoll` → `ArtPollReply`** immediately and cleanly
    (broadcast to `.255:6454`, not unicast back to the requester — relevant to
    the endpoint-reachability item above if that ends up ArtPoll-based).
  - It **ignores `ArtTodRequest`**: four sent (universes 0-3), full wire
    capture of everything from that host, **zero `ArtTodData`** back. Its own
    `goodOut` bits claim RDM is *not* disabled (bit 3 clear on all 4 ports),
    i.e. it advertises a capability it does not deliver.
  - The request packet was checked field-by-field against the Art-Net 4 spec
    before concluding (56 bytes; Net@21, Command=TodFull@22, AddCount@23,
    Address@24) — the silence is the node's, not a malformed probe.
  - **First probe was invalid — corrected same day.** The initial run used
    opcode `0x8080` for ArtTodRequest; the correct value is **`0x8000`**
    (`plugins/artnet/src/artnetpacketizer.h:36`). The node was right to ignore
    an undefined opcode. Re-probed with the correct opcode **and** an
    `ArtTodControl`/**AtcFlush** (`0x8200`, command 1) first — AtcFlush is the
    step that forces a node to actually *re-run* discovery, where ArtTodRequest
    only asks for the TOD it already holds. Result unchanged: 16 `ArtPollReply`,
    **zero `ArtTodData`**. The conclusion stands, but only after the correct
    test. Note QLC+'s own `sendRDMCommand` sends ArtTodRequest with **no
    preceding AtcFlush** — worth revisiting if a real node ever returns a
    stale/empty TOD.
  - **CONFIRMED by DMX-Workshop (2026-08-26).** Artistic Licence's own tool --
    the Art-Net reference implementation -- connects to the node fine and
    reports outright: **"RDM: Node is not RDM capable (Unidirectional DMX)"**,
    with `Rdm Devices: 0 Detected of which 0 active` on all four outputs
    (firmware V0.14, MAC 02:4D:48:12:02:0A, static, LLRP not supported).
    So the probe's verdict was right, independently confirmed.
    The reason is **hardware, not configuration**: RDM needs bidirectional
    RS-485 (talk, then turn the line around to listen), and this node only
    transmits. No firmware setting or update can add it.
    **Correction (2026-08-26): the node was never lying; the wrong bit was
    being read.** GoodOutput bit 3 means "RDM is *disabled* on this port" -- a
    per-port setting, meaningless on a node with no RDM at all. The capability
    flag is **Status1 bit 1**, and across 80 sampled ArtPollReplies it reads 0
    ("not RDM capable") every time. The node reports itself correctly.
    Separately, its Status1 does flap between `0x00` and `0x74` (ROM-booted +
    port-address programming authority + indicator state) roughly 60/40, which
    is almost certainly why DMX-Workshop's displayed lines appear to change on
    their own -- but bit 1 stays 0 throughout, so the RDM verdict never
    actually flips.
    For capability, read Status1 bit 1; GoodOutput bit 3 answers a different
    question. Still worth confirming with a TOD request, since capability bits
    describe intent and a TOD reply is evidence.
    Consequence: RDM must come via the **DMXKing/Enttec Pro USB dongle**
    (wired, bidirectional; `EnttecDMXUSBPro::sendRDMCommand` already exists) or
    via **OLA** acting as an Art-Net→RDM gateway. The CR041R stays a pure
    output path.
    Worth copying from DMX-Workshop's UI: it presents node config as a tree and
    reports RDM device counts **per DMX output**, not per node -- matching the
    per-port finding above.
  - **Control case found, which resolves the earlier ambiguity.** The Pi 5
    build host (`192.168.20.119`) runs an **OLA Art-Net node**, and OLA answers
    the identical probe with `ArtTodData rdmVer=0x01 uidTotal=0 uidCount=0` —
    i.e. a node with an *empty* TOD and nothing attached still **replies**. So
    "silent" is not how an empty TOD presents. The CR041R's total silence is
    therefore best explained by it **not implementing Art-Net RDM at all**, and
    that no longer needs an RDM fixture to establish.
  - **Consequence:** an RDM-capable fixture on a CR041R DMX port will not be
    discoverable *over Art-Net* regardless of the fixture's own RDM support —
    the gateway will not relay it. (Branson has an **ADJ 3Z** on the rig and is
    ordering a known-RDM fixture, 2026-08-25.) Viable transports are therefore
    the **Enttec DMX USB Pro** (wired), or **OLA** as an Art-Net→RDM gateway.
  - **OLA is a zero-hardware development target.** It implements Art-Net RDM
    correctly, and OLA's dummy/simulated-RDM devices would let the entire
    device-table / reconcile workflow be built and tested with no rig at all —
    which would move this item off the ✈️-blocked list. Worth confirming
    before assuming this feature needs hardware.
  - **SUSPICION, not yet proven — QLC+'s `ArtTodRequest` is 25 bytes; the
    Art-Net spec's is 56.** `ArtNetPacketizer::setupArtNetTodRequest`
    (`artnetpacketizer.cpp:161`) builds a 12-byte common header + 9 filler/spare
    + Net + Command + AddCount + **one** Address byte = 25 bytes. The spec
    defines `Address` as a fixed `[32]` array, making the packet 56 bytes.
    Permissive nodes will not care; a strict one would drop it — which is
    exactly the shape of bug that makes RDM "mysteriously not work" against
    real hardware while the code reads fine.
    **NOT verified.** The comparison was attempted against the OLA node on the
    Pi and was inconclusive: OLA answered the 56-byte probe earlier in the day
    but answered neither length while the Pi was under a parallel build
    (load 4.25), so the negative result says more about OLA being starved than
    about packet length. **The test to run**, on an idle box with a known-good
    RDM node: send the byte-exact 25-byte QLC+ layout and the 56-byte spec
    layout to the same node and compare `ArtTodData` replies. Do this before
    concluding anything about QLC+'s RDM working or not working.
  - Develop against **`rdm-sim.py`** (repo root) when no node is available —
    it answers ArtPoll / ArtTodRequest / ArtTodControl with a synthetic TOD and
    ArtRdm GET for the device-table PIDs. Verified working end to end against
    `artnet-probe.py` (3 simulated UIDs). Note it is deliberately *permissive*
    about request length, so it cannot settle the 25-vs-56 question above.
  - Reproduce any of this with **`artnet-probe.py`** at the repo root.

  **Paradigm answers (2026-08-25 discussion) — no new U→node binding needed.**
  `ArtNetController::sendRDMCommand` (`artnetcontroller.cpp:374`) already looks
  up `m_universeMap[universe]` and sends the TOD request to that universe's
  patched `info.outputAddress` / `info.outputUniverse`. **RDM rides the
  existing output patch**; Inputs/Outputs is already the mapping UI. Likewise
  `handleArtNetTodData` (`:515`) → `rdmValueChanged` → `RDMManager` already
  carries replies back, so "self-populate what it finds" is plumbed — the gap
  is auto-`GET`ting the descriptive PIDs per UID to fill a table.
  - Nuance: the binding is **U → (node IP, node *port*)**, not U → node. The
    CR041R is one IP carrying four universes, so discovery is **per-port** —
    a 4-port node needs 4 TOD requests. Note this is the *opposite* of the
    coalesce-by-IP rule in the endpoint-reachability item above (13 IPs, not
    53 universes): reachability coalesces, RDM cannot. Don't share that code
    path by accident.

  **Reconcile semantics — only one of four cases is an error:**
  | Discovered | Patched | Verdict |
  |---|---|---|
  | yes | yes, same address | matched |
  | yes | nothing at that address | **not an error** — offer to patch it |
  | no | yes | **must NOT be an error by default** |
  | yes | yes, *different* address | the real, actionable conflict |
  Row 3 is the trap: most conventionals and much LED gear have no RDM at all,
  so "patched but not discovered" is the *normal* case on a mixed rig. Flagging
  it would cry wolf on first use — the same failure mode as the 2850-line
  `sendDmx` flood.

  **Blocking design decision — `Fixture` must persist an RDM UID.** Confirmed
  `engine/src/fixture.h` has no `uid`/`rdm` field today. Without a stored
  UID↔Fixture binding, reconciliation can only match on address, which is
  circular: "this fixture moved address" is indistinguishable from "a different
  fixture is now at this address". Row 4 above is unbuildable until this is
  settled. It is a `.qxw` schema addition on `Fixture` — decide before writing
  any UI. Also: there are **no RDM tests anywhere in the tree**.

  **Cautions.** RDM is in-band with DMX on a wired line, so discovery
  interleaves with output — do not run it from the MasterTimer thread, and
  expect it to be visibly disruptive on a live wired universe (fine in
  Construction phase, dangerous mid-show; probably gate it the same way Blind
  is gated). ArtNet RDM is a different animal from wired RDM and node support
  is uneven, so verify per-node rather than assuming. Discovery is a binary
  search over the UID space and is *slow* — the existing worker is already a
  `QThread` for that reason; keep it cancellable. **Needs the rig** to be
  worth anything: none of this is verifiable offscreen beyond parser unit
  tests, and there are no RDM tests today.

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

## Backlog — not started

### Power by intensity, per circuit (Branson, 2026-10-11, long term) — not started
A fixture can now be held to a maximum draw (its measured profile, the
limiter scaling its output). Take it a step further: control the power on
a whole **circuit** by intensity — keep everything on a circuit under its
breaker by scaling the fixtures on it together, rather than capping each
one on its own. Builds on PowerProfile/PowerModel and the power
distribution (circuits) already in the show.

### App-wide UI zoom, Cmd+/Cmd- (2026-09-03, long-term, not prioritized) — not started
Branson: VS Code-style zoom — Cmd+/Cmd- (and presumably Cmd+0 to reset)
scaling the whole app's fonts *and* icon sizes together, not just one view.
Explicitly deferred — filed for later, not asked for now.

Checked before filing, so the scope is accurate rather than guessed: **no
zoom infrastructure exists app-wide today.** Two views already have their
*own*, local zoom (Show Manager's timeline — a slider + Ctrl+scroll driving
a `timeScale` variable, `ui/src/showmanager/multitrackview.cpp`; Lighting
Studio's 2D view — Shift+scroll + trackpad pinch,
`ui/src/monitor/monitorgraphicsview.cpp`), but neither is wired to Cmd+/
Cmd-, and neither is what "app-wide" means here — a real implementation is
a different, bigger thing than extending either of those.

**Why this is more than "add a keyboard shortcut + scale the font,"
concretely:** `QApplication::setFont()` cascades point size to most widgets
via font-metric-based sizing, which covers a lot for free — but toolbar/
tree icon sizes are hardcoded pixel values scattered across many call
sites (`toolbar->setIconSize(QSize(20, 20))` appears independently in
Fixture Manager, Connections, and other manager toolbars — see this
session's own work touching several of them), not derived from font size
at all, so they would not scale along with text unless each site is
touched (or icon sizing is centralized first — its own small refactor)
and multiplied by the same zoom factor. `resources/qss/default.qss` also
has fixed-pixel padding/margin values that a pure font-size change
wouldn't touch, so some chrome would look increasingly cramped or loose
relative to the scaled text as the zoom level moves away from 100%.

Not scoped further than this — no design doc yet, no decision on whether
to centralize icon-size constants first or handle it site-by-site, no
persisted-setting design (a `workspace/uiZoom` `QSettings` key alongside
the existing theme/tab-label-mode pattern would be the obvious fit). Pick
up with a real audit of every `setIconSize()`/fixed-pixel QSS rule before
estimating effort for real.

### Show lifecycle: Construction / Test-Validate / Run *(2026-08-15, design doc written; "Production" renamed Run 2026-10-16, D1)*
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
