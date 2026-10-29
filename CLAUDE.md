# CLAUDE.md

Guidance for working in this repo. This is a **fork of QLC+** (Q Light
Controller Plus), forked at 4.14.2 and **based on QLC+ 4.14.4** since 0.3.1
(upstream fixes are taken in each release and logged in
[docs/dev/UPSTREAM.md](docs/dev/UPSTREAM.md)) — the Qt6 **widgets** desktop UI, not the QML UI. The fork adds
a *programmer-mode* workflow and a dedicated **Build** tab (was Programming) for building
scenes/"looks".

## Branching

`main` is the trunk (GitHub default branch). Do new work on a feature branch
cut from `main`, then merge it back with a local merge commit (`git merge
--no-ff`) and push directly — no PRs. Only commit/merge when the user asks;
don't merge speculative or in-progress work.

## Build & run

CMake, out-of-tree in `build/` (Unix Makefiles, Debug). **Qt6 only, 6.8 or
later** (CMake stops with a clear message on anything older). Qt5 support was
dropped in 2026-10; the tag **`qt5-last`** is the last `main` that also built on
Qt5. Homebrew's `qt@5` may still be installed here: pass
`-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6/lib/cmake` if CMake ever picks the
wrong one up.

Before a release — and after any non-trivial change — run `./check-all.sh`.
It builds and runs the full gate as **Qt6, Qt6-Release and Qt6-Werror** in
separate build dirs. The Release leg is not redundant: one shipped bug
(undefined narrowing in `VCXYPadFixture::writeDMX`) only misbehaved under
`-O2` and was invisible to every default-build check.

The gate builds the three legs together (one shared `-j` of performance
cores, `GATE_JOBS` to change it; ccache when installed), then runs the
three legs' tests at the same time, splitting the cores between them: each
leg runs (cores − 2) ÷ legs test programs at once, rounded up
(`platforms/linux/unittest.sh`; `QLC_TEST_JOBS` sets the per-leg count), then
runs the wall-clock-sensitive tests listed in
`platforms/linux/unittest-serial.txt` alone at the end. Its last block starts
with `gate=N` (also in `build-gate-logs/summary.txt`, next to each leg's full
log); a skipped leg (no Qt at `QT6_PREFIX` / `brew --prefix qt6`) fails it
unless `ALLOW_SKIP=1`. A test that fails only under load is an isolation bug
to fix; add it to the serial list only if it measures wall-clock time.

For iterating, **`make quick`** (= `./check-all.sh --quick`) builds only the
Qt6 leg and runs only the tests your change touches since `origin/main`
(`QUICK_BASE` to change): the test's own dir, tests that `#include` a changed
header, a plugin's test; everything if CMake files or the runner changed.
It is not the gate — run the full one before pushing. One test dir by hand:
`QLC_TEST_ONLY="ui/test/workbench" ./check-all.sh --quick`.

Tests never touch the real screen: every test.sh and every test program
(`engine/test/common/testguard.cpp`, linked into each by the top-level
CMakeLists) force `QT_QPA_PLATFORM=offscreen`, and on macOS a crashing test
exits with no "quit unexpectedly" dialog; `platforms/linux/check-test-guard.sh`
fails the gate on a test without the guard. Opt out only on purpose:
`QLC_TEST_REAL_SCREEN=1` (+ `QT_QPA_PLATFORM=cocoa`); `QLC_TEST_CRASH_REPORTS=1`
keeps crash reports.

**The gate pool** runs the full gate of a *committed* SHA on whichever Mac is
idle: `./check-all.sh --remote auto [--rev SHA]` (= `make pool`;
`platforms/macos/gate-pool.sh`, hosts in `platforms/macos/gate-hosts.conf`:
ender local, newaudio, dink, val). `auto` skips hosts that are unreachable,
lack their Qt, or already run a gate, and weighs load over perf cores by the
host's typical gate time; nothing free → this Mac. `--remote NAME` picks one;
`make pool-status` shows what auto sees. One gate per host: it takes the
host's `~/gates/.gatelock` first (`platforms/macos/gate-lock.sh`) and
*waits* ("waiting for val: gate <sha> running since 11:51") while another
gate's lock, live `*.running` marker or an agent's `.vallock` holds it; a
holder whose gate is dead or over 2 h old is cleared. It `git archive`s the SHA to
`~/gates/<sha>-<time>` there (never the host's own checkout), runs detached
(survives ssh drops), streams progress, removes the copy, and keeps
`~/gates/<id>.log` + `.summary.txt` (+ `.logs/` on failure). Every gate's
result line names where and with what it ran — **`gate=N host=NAME qt=6.x.y
rev=SHA`** — so read the host and Qt, not just `gate=0`; then push that exact
SHA: `git push origin <sha>:main`. Host notes: memory `remote-gate-hosts`.

CI (`.github/workflows/build.yml`) runs macOS + Linux + Linux-coverage on every
push. It had never executed before 2026-08-25; if runs stop appearing, check
Settings → Actions → General rather than assuming the workflow is wrong.

There is a second machine, **`ender`** (SSH, 14-core Mac, Qt6 only, repo at
`~/git/qlcconsole`), on the `172.18.2.0/24` show VLAN via `vlan0`. It's the
only place DMX output can be verified on real network hardware — see
docs/project/DONE.md 2026-08-25 for the soak/throughput harnesses. Note `cmake` isn't on the
non-interactive `PATH` there; prefix with `export PATH=/opt/homebrew/bin:$PATH`.

```sh
# Reconfigure only when CMakeLists/sources are added or removed:
cmake -S . -B build

# Incremental build (parallel). Run after editing sources: performance cores
# only, not hw.ncpu — on Apple Silicon that also counts efficiency cores,
# which saturating slows down everything else running on the machine.
cmake --build build -j"$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"

# Run. ALWAYS pass -o to open a workspace; positional args are ignored and
# can overwrite the .qxw with an empty file (see memory: qlcplus-cli).
build/main/qlcconsole -o test-workspaces/surfacetesting.qxw
```

`make` shortcuts from the repo root (`make help` lists them):

- **`make test`** — incremental build in `build-dev/` (RelWithDebInfo,
  Homebrew Qt) installed as `~/Applications/qlcconsole-dev.app`: ad-hoc
  signed, its own bundle id (`…qlcconsole.dev`, "qlcconsole (dev)"), opens
  from Finder/Spotlight or `…/Contents/MacOS/qlcconsole -o show.qlcc`. It runs
  **no unit tests** (that's `make check` = `./check-all.sh`). `make run
  SHOW=path` builds and launches with `-o`. Settings are shared with the
  release app (QSettings `org.qlcplus.qlcconsole`).
- **`make dmg`** — `platforms/macos/make-release.sh`: the signed,
  notarized, stapled universal DMG of a committed HEAD into `dist/` (+
  `dist/SHA256SUMS`); replaces `~/qlcconsole.app` (old one →
  `~/qlcconsole.app.prev`). Publishes nothing.
- **`make beta` / `make release` / `make attach` are PUBLIC**
  (`platforms/macos/publish.sh`): gate + `make dmg` + push tag `v<VERSION>` +
  a GitHub pre-release ("Show Tree X (beta)") or full release, DMG +
  SHA256SUMS verified by download; `attach` adds CI's files. Never run them
  unasked; `DRYRUN=1` is safe, `DRAFT=1` leaves a non-public draft. REST
  only (`gh api`), not `gh release` (GraphQL limits). See docs/dev/RELEASE.md.

- App binary: `build/main/qlcconsole` (arm64 Mach-O; renamed from `qlcplus` in
  the qlcconsole rebrand — launcher/fixtureeditor binaries are
  `qlcconsole-launcher` / `qlcconsole-fixtureeditor`).
- Test workspace: `test-workspaces/surfacetesting.qxw` — reuse it across
  iterations; don't rebuild fixtures/scenes/chasers from scratch (see memory:
  test-workspace). Other dev/test `.qxw` files live alongside it in
  `test-workspaces/`; `resources/samples/Sample.qxw` is a separate, real
  bundled app resource and stays where it is.
- `qmlui` is OFF; ignore `qmlui/` for this work.

## Architecture (where things live)

QLC+ splits into an **engine** (no UI) and a Qt widgets **ui** layer.

- `engine/src/` — Doc, Fixture, FixtureGroup, Scene, Chaser, Collection, EFX,
  function model + DMX. Pure logic; serializes to/from `.qxw` XML.
- `ui/src/` — Qt widgets: Fixture Manager, Function Manager, the Build
  tab, consoles, editors.
- `main/`, `launcher/`, `fixtureeditor/`, `plugins/`, `webaccess/` — entry
  points and peripheral apps; rarely touched for this work.

### Fork hotspots

- **`engine/src/programmercontroller.{h,cpp}`** — all programmer-mode logic
  (palettes, group scenes, Save workflow). `Doc` has *thin forwarders* only;
  put new programmer/palette/group logic HERE, not in Doc (memory:
  programmer-controller).
- **`ui/src/programmingmanager.{h,cpp}`** — the **Build** top-level tab (it
  was "Programming"; the class kept its name): the navigator by section
  (Palettes | Effects | Scenes, groups & fixtures under each) and the
  toolbar. Build ▸ Scenes is `ui/src/programmingoutline.{h,cpp}` in
  Navigator mode (docs/design/WORKFLOW_DESIGN.md §3.3).
- **`ui/src/workbench.{h,cpp}`, `ui/src/workbenchstack.{h,cpp}`** — the
  Workbench: a self-contained widget (header, layer rows = the outline in
  Stack mode, + Scene / + EFX / + RGB Matrix, editor pane with the Look
  Editor / channel console / stock editors, live preview, ↩) over a
  `WorkbenchStack` back end (`FunctionStack`: scenes, collections, chasers;
  `TreeCueStack` in `ui/src/treecuestack.*`: a Show Tree cue, every edit a
  tree step -- not `CueStack`, which is the engine's Simple Desk class).
  Collection edits are `PaletteOps::functionStep` steps on the shared
  history. The Show Tree docks Build's Workbench in place of its page
  (`ui/src/showtreeworkbench.cpp`, `ProgrammingManager::lendWorkbench()`).
- **`ui/src/fixturemanager.{h,cpp}`** — Fixture Manager: fixture-group folders,
  empty-group creation, the head-layout grid editor (drag fixtures onto cells,
  add a group as a block, move a sub-group as a unit).
- **`engine/src/fixturegroup.{h,cpp}`** — `FixtureGroup` model: `path` (folders),
  head layout, sub-group tagging (+XML).

### Conventions

- Scene/collection names follow `SS-PP.II-Description`; when duplicating, learn
  the pattern from siblings — don't hard-code (memory: show-naming-convention).
- Match surrounding QLC+ style (it's stock Qt6/C++; existing files set the bar
  for naming, comment density, license header).
- Engine code touched by live preview must be thread-safe: `Scene::write`
  fader-map build is guarded with `m_valueListMutex` (race vs `resetRuntime`).

## Direction

[docs/project/STRATEGY.md](docs/project/STRATEGY.md) (adopted 2026-10-28)
says what qlcconsole is, the roadmap (0.3.1 polish and the classroom, 0.4
run the show, 0.5 the show explains itself) and the guardrails every change
follows:

- A feature attaches to an existing model (Show Tree, Build/Workbench,
  venues, focus calls, Show Status, controller schemes); a new top-level
  model, tab or playback path needs an explicit yes.
- It ships with help, a course note where it applies, tests in the gate and
  a CHANGELOG line; a candidate gets an ROI line before it's built.
- The menu-item count stays flat or falls; upstream intake is logged in
  [docs/dev/UPSTREAM.md](docs/dev/UPSTREAM.md); the full gate holds to its
  time budget (about 15 min; a new test over 10 s needs a reason).
- **Not built:** Ableton Link, NDI/pixel mapping, an audio engine, OSCQuery,
  multi-user editing, QLC+ 5 parity work, relative/multiply layers.
- Never call QLC+ 4.x abandoned: it is in maintenance upstream.

## Performance

Keeping the engine as fast as it is, or faster, is part of every change
([docs/performance.md](docs/performance.md#keeping-it-fast)). A change to a
hot path (MasterTimer, Universe, faders, Scene/Chaser/EFX/RGBMatrix write,
plugin input/output, the Show Tree runtime) carries before/after numbers in
its commit: `make perf` (~5 min, Release engine in `build-perf/` vs this
machine's baseline in `docs/perf-data/`; `perf=1` is a regression).
A measurable slowdown is a bug. A baseline is rebased only on purpose
(`make perf-record`), with the reason in the commit.

## TODO / status

Project tracking lives in `docs/project/`: `TODO.md` (active work at the
top, Done at the bottom), `DONE.md` (the detailed log), `STRATEGY.md`.
Update TODO.md as features land (commits often say "TODO: … done"). Design
notes are in `docs/design/`, developer docs (RELEASE, TEST_MAP,
TEST_MATRIX, UPSTREAM) in `docs/dev/`, fork-only dev scripts in `tools/`.

## Memory

Persistent notes live under
`~/.claude/projects/-Users-branson-git-qlcconsole/memory/` (indexed by `MEMORY.md`).
Check there for workflow conventions before re-deriving them.
