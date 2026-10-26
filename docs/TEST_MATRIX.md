# Extended test matrix — 0.3.0 heading for RC3

**Date:** 2026-10-26  
**SHA tested:** `05534ef92` (origin/main when the run started). Every host ran a
`git archive` of this one commit in `~/gates-matrix/m05534/`, never the host's own
checkout. All of those copies have been removed.  
**Purpose:** find inconsistencies that the normal gate (macOS Qt6 Debug, Release and
Werror, offscreen) does not see. The method was to vary the machine, the Qt
version, the sanitizer, the locale, the scale, the style and the theme, and to
open shows that had been damaged on purpose.

The findings table is at the end. Severity runs high, medium, low. "Real" means
the product is wrong. "Test" means the test is wrong. "Env" means the tool or
the setup caused it.

## Hosts and what ran

| Host | Arch / OS | Qt | What ran |
|---|---|---|---|
| dink | arm64 macOS 26.7 | 6.11.2 (Homebrew) | ASan + UBSan Debug build, full offscreen suite; show round trip (37 shows); 200 damaged shows |
| newaudio | arm64 (M4 Pro) macOS 26.3 | 6.12.0 (Homebrew) | TSan build: engine tests + 17 playback/UI tests. Normal build: full suite ×10 in random order (5 at 12 jobs, 5 at 16, alongside another agent's pool gate). Variants: scale 2, de_DE, Fusion, macOS style, themes 1–5 |
| val | Intel i7 macOS 26.7 | 6.9.3 (`~/Qt-aqt/6.9.3`, the DMG's Qt) | Full suite: base, `QT_SCALE_FACTOR=2`, `LANG/LC_ALL=de_DE.UTF-8`, `QT_STYLE_OVERRIDE=Fusion`, themes 5 (dark) and 1 (light), macOS style. Each slice stood aside for RC3 gates (see Notes) |
| buildhost-qlcplus 192.168.1.245 | x86_64 Debian 13, GCC 14.2 | 6.8.2 (apt) | `-Wall -Wextra -Wpedantic` Release build (no `-Werror`); clang-tidy 19 and cppcheck 2.17 over `engine/src` and `ui/src`; full offscreen suite under C.UTF-8 and then under de_DE.UTF-8 |
| rpi5 | aarch64 Debian 13 | 6.8.2 (apt) | Release build (Ninja `-j3`) and the full offscreen suite, with the plugin directory masked |
| Win10 VM (on val) | x86_64 Windows 10 22H2 | CI's MinGW Qt 6 | CI's `05534ef92` installer, installed silently (`/S`). `--version`. 15 shows opened offscreen from a path with spaces. 33 help pictures at scale 2. `monitor_test` from CI's rig bundle. CI's own Windows test log for this SHA |

### Commands (abridged)

```sh
# dink: ASan + UBSan
cmake -S src -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fsanitize-recover=address -fno-omit-frame-pointer" \
  -DCMAKE_{EXE,SHARED,MODULE}_LINKER_FLAGS=-fsanitize=address,undefined ...
MallocNanoZone=0 ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:abort_on_error=0:log_path=... \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:log_path=... \
QT_QPA_PLATFORM=offscreen QLC_TEST_JOBS=3 QTEST_FUNCTION_TIMEOUT=900000 ./unittest.sh ui build-asan

# newaudio: TSan (Qt is not instrumented)
-DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g -O1"
TSAN_OPTIONS="abort_on_error=0 halt_on_error=0 second_deadlock_stack=1 log_path=..." \
QLC_TEST_ONLY="engine/test/* ui/test/{cuerunner,showcontrol,showmanager,showtreeview,showtreeworkbench,
  focusmode,focuscallsui,workbench,workbenchchaser,simpledeskengine,speedmasterspanel,subcues,vccuelist,
  vcbutton,programmingmanager,lookeditor,appviews}" ./unittest.sh ui build-tsan

# newaudio: random order. unittest.sh starts the "longest" tests first, so random times give a random order.
QLC_TEST_TIMES=<file of random times> QLC_TEST_JOBS=12|16 ./unittest.sh ui build-normal   # x10

# variants (val, newaudio)
QT_SCALE_FACTOR=2 | LANG=LC_ALL=de_DE.UTF-8 | QT_STYLE_OVERRIDE=Fusion|macos | QLC_MATRIX_THEME=N

# Linux: warnings
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -Wpedantic -Wno-error" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && ninja -k 0
run-clang-tidy -checks='-*,bugprone-*,performance-*,clang-analyzer-core.*,clang-analyzer-cplusplus.*,
  -bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,...' -header-filter='(engine|ui)/src'
cppcheck --enable=warning,performance,portability --inconclusive --library=qt engine/src ui/src
```

**Harness changes, made only in the throwaway copies and never committed:**

- `ui/test/common/testsettings.cpp`: when `QLC_MATRIX_THEME=N` is set, it writes
  `workspace/theme=N` into the test's temporary INI. Note that on Apple platforms
  App's own `QSettings()` uses the organisation *domain*, so the file is
  `qlcplus.org/qlcconsole.ini`.
- `ui/test/matrixshows/`: a copy of `showfiles_test` that takes its files from
  `QLC_MATRIX_SHOWS` / `QLC_MATRIX_FUZZ`. Its `roundTrip` opens a show, adds a
  venue, saves as A, reopens, and compares the fingerprint, the Show Tree XML,
  the Virtual Console XML and the venue sidecar. It then saves as B and requires
  A and B to be byte-identical. Its `corrupt` test opens a damaged show and must
  not crash; if the show loads, it saves it and reopens it.
- The damaged shows came from an lxml mutator with seed 20261026, built from 8
  seed shows. The 200 files were: 37 truncated (at a random byte or at a `<`),
  30 with attribute values mangled (`-1`, `4294967296`, `NaN`, `1e999`, `1,5`,
  `../../../etc/passwd`, ...), 27 with element text mangled, 25 with an element
  duplicated, 55 with 1–3 elements removed, and 26 with sibling elements swapped.

## Results by host

**dink (ASan + UBSan, Qt 6.11.2).**
- Every test program passed: 105 engine, 82 UI and 6 plugin. The only failure was
  the harness's own `matrixshows` directory, whose `test.sh` was not executable.
  The suite took 10 minutes at 3 jobs.
- Deduplicated by stack, the sanitizers reported the following:
  - **heap-use-after-free in the rig view on every `App::clearDocument()`** (M1).
  - **UBSan: uninitialised members read in constructors** (M2, M3).
  - Test-only use-after-free and stack-use-after-scope reports (T1–T3).
  - About 30 UBSan "invalid enum value" reports. These come from tests that
    deliberately cast 42, 999, 31337 or 0xDEADBEEF to an enum to test the
    `*ToString` and `set*` defaults. They are test-induced, low priority, and a
    UBSan build would need `-fno-sanitize=enum` or a suppression list for these.
    **Fixed in `c371b24ca`:** the tests use an unnamed value the enum can hold, or,
    where every value is named, drop the cast. `KeyPadParser_Test`'s float-to-char
    conversions are fixed as well.
- **Show round trip:** all 37 shows passed. The set was every `docs/course/shows`
  file, every `test-workspaces` show (the untracked DanceHysteria 0.2–0.6 and
  Office included), the autosaves, `Sample.qxw` (identical to upstream's), the
  stresstest shows and a scratchpad backup. For each one the fingerprint, the
  Show Tree XML, the VC XML and the venue sidecar matched, and the second save
  was byte-identical to the first. The venue sidecar travelled with Save As
  every time.
- **200 damaged shows:** there were no memory errors beyond M1 and M2. 198 shows
  loaded and then saved and reopened with no error. Two hung: **F1**. One of
  those, given more time, ended in a `QEventDispatcherUNIXPrivate` qFatal abort.
- **Every truncated file loaded as "no error" with no message at all** (F2).
- Crash reports on dink:
  - Two `matrixshows_test` reports, both from F1's hang (the QtTest watchdog
    abort, then the qFatal).
  - One `updates_test` report. ASan's runtime corrupts its own lock in the child
    of a `fork()` (QProcess). That is an ASan-on-macOS environment issue, not the
    product.

**newaudio (TSan; random order; variants, Qt 6.12.0).**
- TSan reported 731 races on 20 test programs (exit 66). After deduplication,
  almost all are RGBScript's JS-thread `BlockingQueuedConnection` hand-offs and
  QMutex- or QRecursiveMutex-guarded MasterTimer and Universe state. TSan cannot
  see those as synchronised because Qt is not instrumented.
  - The 4 "double lock" reports are `JSThread`'s handshake mutex, locked on one
    thread and released by `wait()` on another.
  - No report was confirmed as real. The candidates worth a run against an
    instrumented Qt are listed under E3.
- **Random order: 10 of 10 runs were green.** Five ran at 12 jobs and five at 16,
  next to another agent's 3-leg gate on the same Mac. **No flaky test was found.**
- Variants: base, scale 2, de_DE, Fusion and themes 1–5 all passed (themes after
  the tweak fix; see Notes). The **macOS style** variant failed 6 UI tests (S1).

**val (Intel, Qt 6.9.3).**
- Base, scale 2, de_DE and Fusion all passed, except for one test, which
  **failed in 3 of 4 runs on val and never elsewhere.** It is
  `MonitorProperties_Test::hidingTheApronMovesNothingOnIt` (T1). This is the
  same test that failed RC3's val gate `29a7b787acff` at 08:05. It is a
  use-after-free in the test, which ASan on dink pinpointed. **It was already
  fixed on main** by `84778c937` ("read the apron's id before no depth deletes
  its deck").
- Themes 5 (dark) and 1 (light), with the fixed tweak: both green apart
  from T1.
- macOS style: crashes in 24 programs and throws in 6. This is E4, Qt 6.9.3's
  `qmacstyle` without cocoa.
- A repeat base run (base2) failed T1 once more, and also
  `MainBar_Test::fadeOneSecondAndStopFadesTheLightOut`, which ended after
  173 ms even though it runs alone in the serial phase (T4).

**Linux 192.168.1.245 (GCC 14, Qt 6.8.2).**
- `-Wall -Wextra -Wpedantic`: 28 warnings, none of them in our code.
  - 25 `-Wpedantic` in `/usr/include/libusb-1.0/libusb.h` (zero-size and
    flexible arrays).
  - 2 `-Wformat=` in `plugins/hid/linux/hidapi.cpp:236,246`, where `%x` is read
    into an `int *`. That code is upstream's from 2014.
  - Nothing new since 2026-10-20. The tree is already `-Wall -Wextra -Werror`
    clean on Linux.
- The suite passed under C.UTF-8. Under de_DE.UTF-8, one test failed (L1).
- No core files were left.
- clang-tidy: 414 unique findings, of which 248 are
  `performance-unnecessary-value-param`. cppcheck: 2071 lines. The ones with real
  signal are SA1–SA6 in the table. The others were checked and are false
  positives: `showtreereusable.cpp:84` (out of bounds), `qlcpalette.cpp:2443`
  (`at(-1)`, guarded), `powerprofiledialog.cpp:68` (dangling temporary, but
  `foreach` copies), and `monitor.cpp:9620` (a lambda capturing a local, used
  modally).

**rpi5 (aarch64, Qt 6.8.2).**
- A clean Release build, then **all 193 test programs passed** (105 engine,
  82 UI, 6 plugin) in 13 minutes at 2 jobs.

**Windows 10 VM (CI's installer and test log).**
- The silent install succeeded to `C:\qlcconsole`. `--version` printed
  `qlcconsole 0.3.0 (build 4.14.2+qlcconsole.2270, 05534ef92)`.
- 14 of 15 shows opened offscreen from `C:\Users\Branson Matheson\qlc matrix
  05534\show files\` (a path with spaces). Each drew its main window and quit by
  itself in 5–16 s; DanceHysteria-0.6 took 16 s.
- **Office.qlcc never quits** (3 of 3 runs). The same thing happens on macOS
  (F3).
- The help pictures (connections, patch labels, venue chip and menu, plot
  layers, Look Editor and others): 33 of 33 were taken at scale 2 with 0
  failures. Fonts and layout match the Mac pictures. The only visible difference
  is cosmetic: the Connections "Devices" tree shows a third column, cut off as
  "Carr…", with "4…" / "1…" values. On the Mac that column is off-screen.
- The venue sidecar `tw-help-demo.venues.xml` was written beside the show in the
  path with spaces. The original show files were never written.
- `monitor_test` from CI's bundle: 347 passed, 8 failed. Two of the failures are
  the real ones CI also sees (W2). The other six need `QLC_TEST_RESOURCE_ROOT`,
  which is CI's `D:/a/...` path, and so are environment failures.
- **CI's informational Windows test step for this SHA: 16 of 190 test programs
  fail.** TODO.md says "all pass". The step has `continue-on-error`, so this has
  gone unnoticed since at least 2026-10-24 (W1–W3).
- Bluetooth/hotplug: `inputdevicenotices_test` fails on Windows, but the engine
  side passes: the disconnect is seen and the patch is removed. Only the test's
  spy on the unexported `InputDeviceNotices::notice` gets nothing, so this is
  **not a hotplug regression** (part of W3).
- The VM's `HKCU\Software\qlcplus` key was exported before the run and restored
  after it.

## Findings

| ID | Where | Symptom | Reproduction | Frequency | Triage | Severity | Owner / area |
|---|---|---|---|---|---|---|---|
| **M1** | `App::clearDocument()` (`ui/src/app.cpp:1035-1038`) → `Monitor::updateView` → `MonitorGraphicsView::refreshItemLayerState` (`monitorgraphicsview.cpp:2786-2889`), `TrussItem::boundingRect` (`trussitem.cpp:130`); also in `~MonitorGraphicsView` (:595) | **heap-use-after-free.** `Doc::clearContents()` → `MonitorProperties::reset()` (`monitorproperties.cpp:1012`) deletes the stage platforms, stands, towers, scenery and trusses. The rig view's items still point at them and read `layerId()`, `locked()`, `groupId()` and `type()` before they are rebuilt. | Open any show with stage structures (`stage-structures-demo.qxw`, `surfacetesting.qxw`, `Office.qlcc`, the course shows), then File ▸ New or Open. ASan reports it every time. `courseshow_test`, `showfiles_test` and `matrixshows` hit it. | 100% under ASan; silent in normal builds (freed memory still readable) | Real | **High** (UAF on every show change; a crash is possible in Release) | Rig view / Monitor: clear the items before `Doc::clearContents()`, or have `updateView` drop stale items first |
| **M2** | `ShowTreeView::eventFilter` (`showtreeview.cpp:9546`) during the constructor (`:1184`) | UBSan reads `m_filter` (`QLineEdit *`, `showtreeview.h:908`) before it is set. Its value is `0xbebebebe`. The `QShortcut` parented to `m_tree` sends ChildAdded through the event filter, which is already installed. | Construct an App (every UI test that builds one). | 100% under UBSan | Real (only a compare, but undefined) | Low | Show Tree: `QLineEdit *m_filter = nullptr;` |
| **M3** | `VCButton::VCButton` → `setState(Inactive)` (`vcbutton.cpp:104,479`) and `setAction` (`:106,618-708`) | `m_state` and `m_action` are read before they are initialised (UBSan reports `0xbebebebe` and `-1`). If the garbage happens to equal `Inactive`, `stateChanged` is never emitted. This comes from upstream QLC+. `VCSlider`'s `SliderMode(-1)` (`vcslider.cpp:205`) is deliberate and of the same kind. | Create any VC button. | 100% under UBSan | Real (upstream) | Low | VC: initialise in the header |
| **F1** | `Doc::applyFixtureToUniverses` → `InputOutputMap::addUniverse` (`inputoutputmap.cpp:249`) → `universeAdded` → `ShowControlSyncPage::refreshMidiInputs` (`showcontrolsyncpage.cpp:367`) | **A show with one fixture whose `<Universe>` is huge freezes the app on open.** Universes are added one at a time up to that index, and every add rebuilds the MIDI-input list over all of them, which is O(N²). With 65536 the load had not finished after 10 minutes. With `-1` (4294967295) it ends in a qFatal: `QEventDispatcherUNIXPrivate` runs out of descriptors. | In `Office.qlcc`, set one fixture's `<Universe>` to `65536` (`f097`), or a fixture's to `-1` (`f179`), then open it. | 2/200 damaged shows; 100% repeatable | Real | **Medium** (a hand-edited or damaged show hangs and then aborts) | Engine loader: cap or refuse universe indexes above a limit (QLC+ supports far fewer); refresh the sync page once after load, not per universe |
| **F2** | `App::loadXML(const QString&)` (`ui/src/app.cpp:4807-4815`) | **A truncated show opens as "no error", silently.** After `loadXML(*doc)` nothing checks `doc->hasError()` for a premature end of document. A cut file opens as a smaller show, and a Save or autosave would then overwrite the original with less. Inherited from upstream (`~/git/qlcplus/ui/src/app.cpp:1403`). | `head -c 15000 help-demo.qxw > cut.qxw`, then open it. 37 of 37 truncated variants loaded with 0 dialogs, including ones with only 4 fingerprint lines left. | 37/37 | Real (data safety) | **Medium** | App load: treat `hasError()` after the load as damaged and say so (offer read-only, or refuse) |
| **F3** | Opening `test-workspaces/Office.qlcc` | **The show is "modified" as soon as it opens.** Closing it asks "save changes?", so the shot harness hangs at `App::closeEvent` → `saveModifiedDoc` (`app.cpp:921`, `:2725`), and the autosave writes `Office.qlcc.autosave`. Other shows quit cleanly. | `QT_QPA_PLATFORM=offscreen QLC_SHOT_DIR=/tmp/s qlcconsole -o Office.qlcc` never exits; `sample` shows `saveModifiedDoc`. | 3/3 Windows, 1/1 macOS | Real (something in Office's load marks the doc dirty: its ArtNet output "kept, plugin not installed", WLED or effect state) | Low–medium (a needless "save?" prompt; an autosave at open) | App load / output-patch "kept" path. Also: the shot harness should never block on a prompt |
| **L1** | `ShowTreeView_Test::typedTimesAreKeptWhenFocusLeaves` (`ui/test/showtreeview/showtreeview_test.cpp:7090`) | Under `de_DE.UTF-8` on Linux, typing "1.5" into the fade field gives **15000 ms**. The German spin box takes "." as a group separator. macOS ignores `LANG` for `QLocale` (only C `printf`/`atof` change), so only Linux catches this. | `LANG=LC_ALL=de_DE.UTF-8 QT_QPA_PLATFORM=offscreen showtreeview_test typedTimesAreKeptWhenFocusLeaves` on Linux | 1/1 on Linux de; 0 elsewhere | Test (locale-bound input). Also a UX risk: a German user typing "1.5" gets a 15 s fade with no warning | Low (test); medium (UX) | Show Tree: type `QLocale().decimalPoint()` in the test; consider rejecting group separators in time fields |
| **S1** | Real macOS style (`QT_STYLE_OVERRIDE=macos`); the gate's offscreen runs use Fusion | Six UI tests fail only under the style that users actually get on a Mac: `appviews` (main window minimum width 1037 > 1000), `viewlayout` (Lighting Studio's Monitor ends 20 px short of the right and bottom edges at 1280×800 and 1920×1080), `mainbar` (Blackout contrast < 4.5 in theme 0), `programmingoutline` (Option-click expand-all), `connectionstree` (scroll 63/64), `monitor` (classic scroll bar room 16 beside the lock buttons). | `QT_QPA_PLATFORM=offscreen QT_STYLE_OVERRIDE=macos ./unittest.sh ui build` on newaudio (Qt 6.12); the val (6.9.3) result is in Notes | 1/1 on newaudio | Real in part (Qt 6.12; on 6.9.3 the style crashes offscreen, E4): the 20 px gap, the minimum width and the contrast are user-visible if they reproduce in the cocoa app. The macOS style is not fully native under offscreen, so check in the real app | Medium | Layout / main window; consider a macOS-style leg in the gate |
| **T1** | `MonitorProperties_Test::hidingTheApronMovesNothingOnIt` (`monitorproperties_test.cpp:3410`) | The test reads `apron->id()` after `setApronDepth(0)` has deleted the deck. ASan reports a heap-use-after-free; on val (Intel) the freed memory is reused and the compare fails (0 vs 1). **This is RC3 val gate `29a7b787acff`'s failure.** | Full suite on val | 3/4 on val; 0 elsewhere (ASan always) | Test | Medium (it broke an RC gate) — **fixed on main in `84778c937`**; no ASan report from it in the `c371b24ca` rerun | Monitor properties tests |
| **T2** | `monitorproperties_test.cpp:1013-1077` (`stageStructureAddRemove`), `:1379`; `scenery_test.cpp:356`; `fixturemanager_test.cpp:1174` | Test-side use-after-free: `id()` is read on objects the test has just removed or deleted. | ASan suite | 100% under ASan | Test | Low — **fixed in `c371b24ca`** (ids kept first; clean under ASan on dink) | Respective tests: keep the id before removing |
| **T3** | `controllerkeepalive_test.cpp:40-41`; `connectionstree_test.cpp:1139,1228` | stack-use-after-scope: a fake backend outlives its counters, and a `connect(..., [&rebuilds])` lambda has no context object and runs after the test's frame is gone. | ASan suite | 100% under ASan | Test | Low — **fixed in `c371b24ca`** (counters declared first; a local context object; clean under ASan on dink) | Give the lambdas a context object; order the destructors |
| **T4** | `MainBar_Test::fadeOneSecondAndStopFadesTheLightOut` (`ui/test/mainbar/mainbar_test.cpp:468`) | "Fade 1 second and stop" ended after 173 ms, even though the test runs alone in the serial phase (`unittest-serial.txt`). | Full suite on val (Intel, Qt 6.9.3) | 1/7 on val; 0/10 newaudio, 0 elsewhere | Test: a wall-clock assert. The fade advances one tick per frame, and a machine that falls behind sends late frames back to back | Low–medium (gate flake) — **fixed in `c371b24ca`**: the test records every frame sent and asserts the level only falls, through at least 25 of the 50 frames (49 seen); no wall-clock assert | Main bar / fade timing |
| **W1** | Windows (MinGW) DLL boundary: `for (auto sig : { &Doc::a, &Doc::b, ... })` at `ui/src/programmingoutline.cpp:148`, `programmingmanager.cpp:670`, `showtreeview.cpp:2077,2091` | `QObject::connect(Doc, ProgrammingOutline): signal not found` (and `Doc, QTimer`). In a braced list, member pointers become import-thunk addresses, so these connects fail and **the Build outline, Build ▸ Effects and the Show Tree Library never refresh on Windows** when functions, groups, fixtures or palettes change. | CI Windows log, run 40312625172: `programmingmanager_test`, `showtreeview_test`, `itemicons_test` | Every Windows CI run since ≥ 2026-10-24 | Real (Windows only) | **High** | Build / Show Tree: one plain `connect()` per signal |
| **W2** | Windows: UI connects to unexported engine classes: `Fixture::valuesChanged` (`monitorgraphicsview.cpp:4797`), `FixtureGroup::changed/nameChanged` (`fixturegroupeditor.cpp:156,208`), `QLCIOPlugin::inputDiagnostic/configurationChanged` (`controllertestdialog.cpp:168,173`); `lanternpixels.cpp:125` has the same pattern | Drapes driven by a curtain fixture never move. The group editor goes stale. The Controller Test dialog lists no inputs. | `monitor_test` (both curtain tests; also on the VM), `venuerigui_test`, `focusmode_test` on CI Windows | Every Windows run | Real (Windows only) | Medium | Engine: `QLC_ENGINE_EXPORT` on `Fixture` and `FixtureGroup`; decide about `QLCIOPlugin` |
| **W3** | Windows: tests spy on unexported classes (`InputDeviceNotices`, `EffectDesigner`, `LookEditor`, `LedLayoutEditor`, `CueRunner`, `ProgrammingManager`, `ConnectionsTree`, `StagePieceDialog`, `App`, `ShutterAutoOpen`, `ArtNetPlugin`); `effectdesigner_test.cpp:331` calls `first()` on an empty spy (exit 139); `venuerig_test.cpp:861` keeps the sidecar open across `forgetPatch()` (Windows file lock) | 12 test programs fail on Windows with the product fine; one of them crashes. | CI Windows log for `05534ef92` | Every Windows run | Test | Medium (it hides W1/W2: the step is informational) | Tests and CI: export the classes or use string signals; make the Windows test step fail the job (TODO.md already plans this); add a gate scan for `&X::signal` outside X's library (as `d3f737033` did) |
| **SA1** | `engine/src/monitorproperties.cpp:2998` and `:3931` (cppcheck `uninitStructMember`) | A mounted fixture's rebuilt plot item is a fresh `PreviewItem` whose `m_zoom` and `m_flags` are **uninitialised** (`monitorproperties.h:68-69` have no defaults). `m_flags` holds Hidden / InvertedPan / InvertedTilt / VerticalStrip, so the rebuilt item can come up hidden or inverted, and that gets saved. | Static: a mounted fixture with no `<Item>` on load (`fixtureRigPosition` path) | Latent | Real | Medium | Monitor properties: `int m_zoom = 0; quint32 m_flags = 0;` |
| **SA2** | `engine/src/showembedding.cpp:1021,1023` | `paramNamed(a.paramDefs(), name)` keeps a pointer into the temporary `QList` that `paramDefs()` returns by value. It survives today only because of implicit sharing. | Static (cppcheck `danglingTemporaryLifetime`) | Latent | Real (fragile) | Low | Show embedding: bind `const auto defs = a.paramDefs();` first |
| **SA3** | `engine/src/measurementunits.cpp:33` (`APP_SETTINGS`) | On macOS, units are stored in `com.qlcplus.qlcconsole` (org "qlcplus") while the rest of the app uses `org.qlcplus.qlcconsole` (organisation domain `qlcplus.org`). In tests they likewise land in a different INI. Settings resets and exports miss them. | `defaults read com.qlcplus.qlcconsole` shows `units.length` on ender (Oct 18) | Always on macOS | Real | Low | Settings: build the domain the same way as `QSettings()` |
| **SA4** | `ui/src/monitor/structurestudioview.h:774,801` | `ThrowKey::operator==` uses `memcmp` on floats: -0.0 ≠ +0.0, NaN == NaN, which is inconsistent with value compare in a hash key. | Static (clang-tidy) | Latent | Real | Low | Studio: compare values |
| **SA5** | `ui/src/virtualconsole/vcxypadfixture.cpp:126`; `sequence.cpp:79`, `vcframe.cpp:1535`, `vcsoloframe.cpp:236`; `rdmmanager.cpp:742` | `operator=` without a self-assignment guard; `Parent::virtual` calls that skip the direct base (`Function::copyFrom` from `Sequence`, `QWidget::paintEvent` from `VCSoloFrame`); a `quint16` loop over a `uint` bound. | Static (clang-tidy) | Latent | Real (minor; mostly upstream) | Low | VC / engine |
| **SA6** | 35 × `bugprone-integer-division` (for example `doc.cpp:1264,1287`, `scene.cpp:1268,1617`, `programmercontroller.cpp:1193`, `monitorfixtureitem.cpp:695,1601,1608`, `vcxypad*.cpp`) | Integer division in a floating-point context. Most are intended (pixels); worth a pass over the engine ones. | Static | — | To review | Low | Engine / UI |
| **E1** | macOS + LANG | `LANG=de_DE` changes only the C runtime on macOS (`atof("1.5")` gives 1, `printf` gives "1,5"); `QLocale::system()` stays en_US. The macOS de runs passed, so no C-runtime number parsing breaks. QLocale coverage needs Linux. | Small Qt probe on newaudio | — | Env | Info | Gate: keep a Linux de_DE run |
| **E2** | ASan + `fork()` on macOS | `updates_test` (QProcess `fork()` → `hdiutil`) dies in the child: `_os_unfair_lock_corruption_abort` in ASan's StackDepot. | ASan suite | 1/1 | Env | Info | — |
| **E3** | TSan with Homebrew Qt | 731 reports; none confirmed. Candidates for a TSan-instrumented Qt run: `Universe::processFaders` (`universe.cpp:360-363`) vs `requestFader` on two worker threads in `LastLookEffect_Test::concurrentStress`; `Function::~Function` vtable vs `MasterTimer::timerTickFunctions:482` (deleting while ticking, which should be excluded by the tick mutex); `ChaserRunner::setAction` vs `Chaser::setAction` (`chaserrunner.cpp:256-315`). | `tsan/` reports | — | Env (probably false) | Info | Engine, if someone builds an instrumented Qt |
| **E4** | `QT_STYLE_OVERRIDE=macos` under offscreen on Qt 6.9.3 (val) | 24 UI programs crash (SIGSEGV in `libqmacstyle` → `objc_msgSend`, painting a `QProgressBar` with no NSView) and 6 throw. On Qt 6.12 the same style runs (S1). This left 16 `*_test` `.ips` crash reports on val, 10:27–10:37. | val macos variant | 1/1 | Env (Qt) | Info | — |

## Rerun after the test fixes (`c371b24ca`)

dink, the same ASan + UBSan Debug build, the 20 programs the fixes touch (16 engine,
4 UI: chaser, controllerkeepalive, efx, fixture, function, grandmaster, keypadparser,
monitorproperties, qlcchannel, qlcfile, qlcinputchannel, rgbtext, scenery, universe,
mastertimer, channelmodifier, connectionstree, fixturemanager, mainbar, vcbutton). All
passed. **ASan: 0 reports** (the baseline full-suite run of the parent commit had 33, 16 of them the tests' own T2/T3).
**UBSan: 0 from the tests.** What remains is product code, reached by these programs:

- `showtreeview.cpp:9546` (M2) and `vcbutton.cpp:479,618-671` (M3), from
  `mainbar_test` and `vcbutton_test`.
- `vcslider.cpp:670-682`: `SliderMode(-1)` read in `setSliderMode` (M3's sibling).
- **New, product:** `channelmodifier.cpp:76`, `m_values[p] = floatVal` converts a
  float above 127 straight into a `char` (QByteArray), which is undefined. Upstream's
  code. It is reached from `ChannelModifier_Test::modifierMap`. Fix: `char(uchar(qRound(floatVal)))`.
- Not in this set, still open: `Monitor_Test::aDoubleClickOnTwoHungOnATrussEditsBoth`
  removes its truss through `MonitorProperties` while the rig view still has the
  `TrussItem`, and `~MonitorGraphicsView` then reads the freed truss
  (`trussitem.cpp:130`). It is M1's shape (the view keeps items for removed
  structures), so it is left to the M1 fix.

The build copy `~/gates-matrix/rc4-testfix` was removed afterwards.

## Notes

- **RC3 and val.** Four RC3 gates ran on val during this matrix (`29a7b787acff`,
  `84778c93775c`, `77092c3c2eeb`, plus a fourth). Every val slice ran under a
  watcher that killed the slice and waited whenever a `check-all.sh` or a fresh
  `~/gates/*.running` appeared. The build gave way three times. The Windows VM
  ran only between gates.
  - `29a7b787acff` failed on T1, which is fixed in `84778c937`.
  - `84778c93775c` failed `ShowFiles_Test::openingAShowNeverWritesIt(stage-structures-demo.qxw)`.
    `77092c3c2` addressed this; it is likely related to F3, the autosave that is
    written at open.
  - `77092c3c2eeb` passed (gate=0).
- **The theme tweak's first version was wrong.** It wrote `workspace/theme`
  through `QSettings("qlcplus","qlcconsole")`. That constructor uses the native
  format, not the test's INI file, so it landed in the real
  `~/Library/Preferences/com.qlcplus.qlcconsole.plist`, a file App never reads.
  The first theme runs on newaudio and val therefore ran in the default theme.
  The fixed version writes the INI that App opens, and those theme runs are the
  ones reported here. The stray plist keys were deleted on both Macs. SA3 was
  found this way.
- **Installed:**
  - Linux host: `clang-tidy` and `cppcheck` (apt, which pulled in clang-19,
    llvm-19, clang-tools-19, libclang-rt-19-dev, libz3-dev and their
    dependencies).
  - Windows VM: qlcconsole 0.3.0 build 2270 (`05534ef92`), which replaced the
    earlier install in `C:\qlcconsole`.
  - Nothing was installed on the Macs or the Pi.
- **Crash reports:**
  - dink: 3, explained above (F1 ×2, E2).
  - val: 16 from the macOS-style variant (E4), all mine.
  - newaudio: none. The Office repro was killed with SIGKILL, which leaves no
    report.
  - Linux and Pi: no core files.
