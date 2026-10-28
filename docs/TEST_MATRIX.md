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
| **M1** | `App::clearDocument()` (`ui/src/app.cpp:1035-1038`) → `Monitor::updateView` → `MonitorGraphicsView::refreshItemLayerState` (`monitorgraphicsview.cpp:2786-2889`), `TrussItem::boundingRect` (`trussitem.cpp:130`); also in `~MonitorGraphicsView` (:595) | **heap-use-after-free.** `Doc::clearContents()` → `MonitorProperties::reset()` (`monitorproperties.cpp:1012`) deletes the stage platforms, stands, towers, scenery and trusses. The rig view's items still point at them and read `layerId()`, `locked()`, `groupId()` and `type()` before they are rebuilt. | Open any show with stage structures (`stage-structures-demo.qxw`, `surfacetesting.qxw`, `Office.qlcc`, the course shows), then File ▸ New or Open. ASan reports it every time. `courseshow_test`, `showfiles_test` and `matrixshows` hit it. | 100% under ASan; silent in normal builds (freed memory still readable) | Real | **High** (UAF on every show change; a crash is possible in Release) | Rig view / Monitor: clear the items before `Doc::clearContents()`, or have `updateView` drop stale items first. **Fixed in `0c2d81d43`:** the model emits `structureAboutToBeDeleted()` before it deletes any structure (reset() and every remove...()), and the rig view drops the item that borrows it (deleted later, since a target is deleted from its own menu); `Doc::clearing()` drops them all. Per-structure Delete had the same use-after-free. `Monitor_Test::aNewShowDropsTheStructureItemsFirst` / `::removingAStructureDropsItsItemFirst` reproduce the trace under ASan without the fix; monitor, courseshow, showfiles tests ASan-clean. |
| **M2** | `ShowTreeView::eventFilter` (`showtreeview.cpp:9546`) during the constructor (`:1184`) | UBSan reads `m_filter` (`QLineEdit *`, `showtreeview.h:908`) before it is set. Its value is `0xbebebebe`. The `QShortcut` parented to `m_tree` sends ChildAdded through the event filter, which is already installed. | Construct an App (every UI test that builds one). | 100% under UBSan | Real (only a compare, but undefined) | Low | Show Tree: `QLineEdit *m_filter = nullptr;`. **Fixed in `ed3af94cc`:** m_filter initialised in the header. |
| **M3** | `VCButton::VCButton` → `setState(Inactive)` (`vcbutton.cpp:104,479`) and `setAction` (`:106,618-708`) | `m_state` and `m_action` are read before they are initialised (UBSan reports `0xbebebebe` and `-1`). If the garbage happens to equal `Inactive`, `stateChanged` is never emitted. This comes from upstream QLC+. `VCSlider`'s `SliderMode(-1)` (`vcslider.cpp:205`) is deliberate and of the same kind. | Create any VC button. | 100% under UBSan | Real (upstream) | Low | VC: initialise in the header. **Fixed in `ed3af94cc`:** m_state and m_action initialised in the header; the Action(-1) assignment removed. |
| **F1** | `Doc::applyFixtureToUniverses` → `InputOutputMap::addUniverse` (`inputoutputmap.cpp:249`) → `universeAdded` → `ShowControlSyncPage::refreshMidiInputs` (`showcontrolsyncpage.cpp:367`) | **A show with one fixture whose `<Universe>` is huge freezes the app on open.** Universes are added one at a time up to that index, and every add rebuilds the MIDI-input list over all of them, which is O(N²). With 65536 the load had not finished after 10 minutes. With `-1` (4294967295) it ends in a qFatal: `QEventDispatcherUNIXPrivate` runs out of descriptors. | In `Office.qlcc`, set one fixture's `<Universe>` to `65536` (`f097`), or a fixture's to `-1` (`f179`), then open it. | 2/200 damaged shows; 100% repeatable | Real | **Medium** (a hand-edited or damaged show hangs and then aborts) | Engine loader: cap or refuse universe indexes above a limit (QLC+ supports far fewer); refresh the sync page once after load, not per universe. **Fixed in `ed3af94cc`:** at most 1024 universes (InputOutputMap::maxUniverses()); a fixture past it, negative or not a number loads unpatched and is said; the Show Control page rebuilds once per run. Tests: Doc_Test::loadUniverseOutOfRange, ShowControlUi_Test::manyUniversesRebuildTheListOnce. |
| **F2** | `App::loadXML(const QString&)` (`ui/src/app.cpp:4807-4815`) | **A truncated show opens as "no error", silently.** After `loadXML(*doc)` nothing checks `doc->hasError()` for a premature end of document. A cut file opens as a smaller show, and a Save or autosave would then overwrite the original with less. Inherited from upstream (`~/git/qlcplus/ui/src/app.cpp:1403`). | `head -c 15000 help-demo.qxw > cut.qxw`, then open it. 37 of 37 truncated variants loaded with 0 dialogs, including ones with only 4 fingerprint lines left. | 37/37 | Real (data safety) | **Medium** | App load: treat `hasError()` after the load as damaged and say so (offer read-only, or refuse). **Fixed in `ed3af94cc`:** Doc::loadXML() false + error log line; App warns "damaged or incomplete (line N)", footer keeps it, Save goes to Save As until saved whole; autosave only ever writes <show>.autosave. Tests: Doc_Test::loadTruncated, ShowFiles_Test::aDamagedShowOpensAndIsNotSavedOver (4 shows x 3 cuts). |
| **F3** | Opening `test-workspaces/Office.qlcc` | **The show is "modified" as soon as it opens.** Closing it asks "save changes?", so the shot harness hangs at `App::closeEvent` → `saveModifiedDoc` (`app.cpp:921`, `:2725`), and the autosave writes `Office.qlcc.autosave`. Other shows quit cleanly. | `QT_QPA_PLATFORM=offscreen QLC_SHOT_DIR=/tmp/s qlcconsole -o Office.qlcc` never exits; `sample` shows `saveModifiedDoc`. | 3/3 Windows, 1/1 macOS | Real (something in Office's load marks the doc dirty: its ArtNet output "kept, plugin not installed", WLED or effect state) | Low–medium (a needless "save?" prompt; an autosave at open) | App load / output-patch "kept" path. Also: the shot harness should never block on a prompt. **Fixed in `ed3af94cc`:** cause: MonitorGraphicsView::ensureTrussGroup() (Monitor::updateView() after the load) grouped the truss-rigged fixtures, which the file had no group for, and set modified after App::loadXML()'s resetModified(); help-demo.qxw too. The reset now comes after the views are built. Test: ShowFiles_Test::aShowOpensUnmodified (every show). |
| **L1** | `ShowTreeView_Test::typedTimesAreKeptWhenFocusLeaves` (`ui/test/showtreeview/showtreeview_test.cpp:7090`) | Under `de_DE.UTF-8` on Linux, typing "1.5" into the fade field gives **15000 ms**. The German spin box takes "." as a group separator. macOS ignores `LANG` for `QLocale` (only C `printf`/`atof` change), so only Linux catches this. | `LANG=LC_ALL=de_DE.UTF-8 QT_QPA_PLATFORM=offscreen showtreeview_test typedTimesAreKeptWhenFocusLeaves` on Linux | 1/1 on Linux de; 0 elsewhere | Test (locale-bound input). Also a UX risk: a German user typing "1.5" gets a 15 s fade with no warning | Low (test); medium (UX) | Show Tree: type `QLocale().decimalPoint()` in the test; consider rejecting group separators in time fields. **Fixed in `ed3af94cc`:** SecondsSpinBox: "." and "," both the decimal point in every time field. Test: ShowTreeView_Test::aTimeTakesEitherDecimalMark (de_DE, en_US, C). |
| **S1** | Real macOS style (`QT_STYLE_OVERRIDE=macos`); the gate's offscreen runs use Fusion | Six UI tests fail only under the style that users actually get on a Mac: `appviews` (main window minimum width 1037 > 1000), `viewlayout` (Lighting Studio's Monitor ends 20 px short of the right and bottom edges at 1280×800 and 1920×1080), `mainbar` (Blackout contrast < 4.5 in theme 0), `programmingoutline` (Option-click expand-all), `connectionstree` (scroll 63/64), `monitor` (classic scroll bar room 16 beside the lock buttons). | `QT_QPA_PLATFORM=offscreen QT_STYLE_OVERRIDE=macos ./unittest.sh ui build` on newaudio (Qt 6.12); the val (6.9.3) result is in Notes | 1/1 on newaudio | Real in part (Qt 6.12; on 6.9.3 the style crashes offscreen, E4): the 20 px gap, the minimum width and the contrast are user-visible if they reproduce in the cocoa app. The macOS style is not fully native under offscreen, so check in the real app | Medium | Layout / main window; consider a macOS-style leg in the gate |
| **T1** | `MonitorProperties_Test::hidingTheApronMovesNothingOnIt` (`monitorproperties_test.cpp:3410`) | The test reads `apron->id()` after `setApronDepth(0)` has deleted the deck. ASan reports a heap-use-after-free; on val (Intel) the freed memory is reused and the compare fails (0 vs 1). **This is RC3 val gate `29a7b787acff`'s failure.** | Full suite on val | 3/4 on val; 0 elsewhere (ASan always) | Test | Medium (it broke an RC gate) — **fixed on main in `84778c937`**; no ASan report from it in the `c371b24ca` rerun | Monitor properties tests |
| **T2** | `monitorproperties_test.cpp:1013-1077` (`stageStructureAddRemove`), `:1379`; `scenery_test.cpp:356`; `fixturemanager_test.cpp:1174` | Test-side use-after-free: `id()` is read on objects the test has just removed or deleted. | ASan suite | 100% under ASan | Test | Low — **fixed in `c371b24ca`** (ids kept first; clean under ASan on dink) | Respective tests: keep the id before removing |
| **T3** | `controllerkeepalive_test.cpp:40-41`; `connectionstree_test.cpp:1139,1228` | stack-use-after-scope: a fake backend outlives its counters, and a `connect(..., [&rebuilds])` lambda has no context object and runs after the test's frame is gone. | ASan suite | 100% under ASan | Test | Low — **fixed in `c371b24ca`** (counters declared first; a local context object; clean under ASan on dink) | Give the lambdas a context object; order the destructors |
| **T4** | `MainBar_Test::fadeOneSecondAndStopFadesTheLightOut` (`ui/test/mainbar/mainbar_test.cpp:468`) | "Fade 1 second and stop" ended after 173 ms, even though the test runs alone in the serial phase (`unittest-serial.txt`). | Full suite on val (Intel, Qt 6.9.3) | 1/7 on val; 0/10 newaudio, 0 elsewhere | Test: a wall-clock assert. The fade advances one tick per frame, and a machine that falls behind sends late frames back to back | Low–medium (gate flake) — **fixed in `c371b24ca`**: the test records every frame sent and asserts the level only falls, through at least 25 of the 50 frames (49 seen); no wall-clock assert | Main bar / fade timing |
| **W1** | Windows (MinGW) DLL boundary: `for (auto sig : { &Doc::a, &Doc::b, ... })` at `ui/src/programmingoutline.cpp:148`, `programmingmanager.cpp:670`, `showtreeview.cpp:2077,2091` | `QObject::connect(Doc, ProgrammingOutline): signal not found` (and `Doc, QTimer`). In a braced list, member pointers become import-thunk addresses, so these connects fail and **the Build outline, Build ▸ Effects and the Show Tree Library never refresh on Windows** when functions, groups, fixtures or palettes change. | CI Windows log, run 40312625172: `programmingmanager_test`, `showtreeview_test`, `itemicons_test` | Every Windows CI run since ≥ 2026-10-24 | Real (Windows only). **Fixed in b586b4af1** (one connect() per signal; ProgrammingOutline_Test::theListsFollowEveryDocSignal) | **High** | Build / Show Tree: one plain `connect()` per signal |
| **W2** | Windows: UI connects to unexported engine classes: `Fixture::valuesChanged` (`monitorgraphicsview.cpp:4797`), `FixtureGroup::changed/nameChanged` (`fixturegroupeditor.cpp:156,208`), `QLCIOPlugin::inputDiagnostic/configurationChanged` (`controllertestdialog.cpp:168,173`); `lanternpixels.cpp:125` has the same pattern | Drapes driven by a curtain fixture never move. The group editor goes stale. The Controller Test dialog lists no inputs. | `monitor_test` (both curtain tests; also on the VM), `venuerigui_test`, `focusmode_test` on CI Windows | Every Windows run | Real (Windows only). **Fixed in 91e1342a0** (Fixture, FixtureGroup exported; QLCIOPlugin signals by name; DllSignals_Test) | Medium | Engine: `QLC_ENGINE_EXPORT` on `Fixture` and `FixtureGroup`; decide about `QLCIOPlugin` |
| **W3** | Windows: tests spy on unexported classes (`InputDeviceNotices`, `EffectDesigner`, `LookEditor`, `LedLayoutEditor`, `CueRunner`, `ProgrammingManager`, `ConnectionsTree`, `StagePieceDialog`, `App`, `ShutterAutoOpen`, `ArtNetPlugin`); `effectdesigner_test.cpp:331` calls `first()` on an empty spy (exit 139); `venuerig_test.cpp:861` keeps the sidecar open across `forgetPatch()` (Windows file lock) | 12 test programs fail on Windows with the product fine; one of them crashes. | CI Windows log for `05534ef92` | Every Windows run | Test. **Fixed in 91e1342a0** (classes exported, guarded spy, sidecar closed) and dbbc578d2 (the step fails the job): 191/191 on Windows CI run 40349870821 | Medium (it hides W1/W2: the step is informational) | Tests and CI: export the classes or use string signals; make the Windows test step fail the job (TODO.md already plans this); add a gate scan for `&X::signal` outside X's library (as `d3f737033` did) |
| **SA1** | `engine/src/monitorproperties.cpp:2998` and `:3931` (cppcheck `uninitStructMember`) | A mounted fixture's rebuilt plot item is a fresh `PreviewItem` whose `m_zoom` and `m_flags` are **uninitialised** (`monitorproperties.h:68-69` have no defaults). `m_flags` holds Hidden / InvertedPan / InvertedTilt / VerticalStrip, so the rebuilt item can come up hidden or inverted, and that gets saved. | Static: a mounted fixture with no `<Item>` on load (`fixtureRigPosition` path) | Latent | Real | Medium | Monitor properties: `int m_zoom = 0; quint32 m_flags = 0;`. **Fixed in `0c2d81d43`:** `int m_zoom = 0; quint32 m_flags = 0;`. The shows had saved it (`FixedZoom="1804114660"` + Hidden / InvertedPan / InvertedTilt in stage-structures-demo, d2d-showfile, help-demo, speed-spread-demo, the course shows): on load an impossible fixed zoom (<0 or >360) is dropped with its flags. Items whose junk zoom was negative (not saved) can't be told apart and keep their flags; nothing in this UI reads them. |
| **SA2** | `engine/src/showembedding.cpp:1021,1023` | `paramNamed(a.paramDefs(), name)` keeps a pointer into the temporary `QList` that `paramDefs()` returns by value. It survives today only because of implicit sharing. | Static (cppcheck `danglingTemporaryLifetime`) | Latent | Real (fragile) | Low | Show embedding: bind `const auto defs = a.paramDefs();` first. **Fixed in `ed3af94cc`:** the two lists are held for the loop. |
| **SA3** | `engine/src/measurementunits.cpp:33` (`APP_SETTINGS`) | On macOS, units are stored in `com.qlcplus.qlcconsole` (org "qlcplus") while the rest of the app uses `org.qlcplus.qlcconsole` (organisation domain `qlcplus.org`). In tests they likewise land in a different INI. Settings resets and exports miss them. | `defaults read com.qlcplus.qlcconsole` shows `units.length` on ender (Oct 18) | Always on macOS | Real | Low | Settings: build the domain the same way as `QSettings()`. **Fixed in `ed3af94cc`:** QLCFile::settingsOrganisation(), as QSettings() picks it; an old macOS value moved once. Test: MeasurementUnits_Test::keptWithEveryOtherSetting. |
| **SA4** | `ui/src/monitor/structurestudioview.h:774,801` | `ThrowKey::operator==` uses `memcmp` on floats: -0.0 ≠ +0.0, NaN == NaN, which is inconsistent with value compare in a hash key. | Static (clang-tidy) | Latent | Real | Low | Studio: compare values. **Fixed in `0c2d81d43`:** keys compare and hash canonical bits (-0.0 = +0.0, one NaN), `Monitor_Test::throwCacheKeysCompareByValue`. |
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
  **Fixed in `471f5f3c8`** (matrix 2, U3 below).
- **New, product:** `channelmodifier.cpp:76`, `m_values[p] = floatVal` converts a
  float above 127 straight into a `char` (QByteArray), which is undefined. Upstream's
  code. It is reached from `ChannelModifier_Test::modifierMap`. Fix: `char(uchar(qRound(floatVal)))`.
  **Fixed in `471f5f3c8`** (matrix 2, U2 below), truncating as before rather than rounding.
- Not in this set, still open: `Monitor_Test::aDoubleClickOnTwoHungOnATrussEditsBoth`
  removes its truss through `MonitorProperties` while the rig view still has the
  `TrussItem`, and `~MonitorGraphicsView` then reads the freed truss
  (`trussitem.cpp:130`). It is M1's shape (the view keeps items for removed
  structures), so it is left to the M1 fix.

The build copy `~/gates-matrix/rc4-testfix` was removed afterwards.

## Matrix 2: fuzzing and the coverage build, fixed in `471f5f3c8`

Matrix 2 (2026-10-26) fuzzed `52a23d27e` on dink with libFuzzer, ASan and UBSan
(Homebrew clang): the show loader, venues, input profiles, fixture definitions
and the Art-Net receive parsers (`~/matrix2/fuzz`), and ran a Debug + gcov
coverage build on the Linux host. Approved for RC4 as Q168 ("Fix all in RC4").
Every one is fixed in `471f5f3c8`. The minimised repros are in the tests as
byte arrays. The fix was checked by replaying all 1056 ArtRdm crash artifacts,
both Art-Net corpora and the known repros, with every prefix of each, under
ASan + UBSan against the fixed parsers (0 reports; the old parsers abort on the
first known repro).

| ID | Where | Symptom | Fix | Test(s) |
|---|---|---|---|---|
| **Z1** | `Chaser::loadXML` (`chaser.cpp:814`), the same loop in `Sequence::loadXML` | A `<Step>` with no `Number`, or a negative one, was `QList::insert`-ed at that index: an abort in Debug, memory corruption in Release. Repro: `<Function Type="Chaser"><Step>` | Out-of-range numbers append. `Collection::addFunction` appends for any out-of-range index too | `Chaser_Test::loadStepNumberOutOfRange`, `Sequence_Test::loadStepNumberOutOfRange` |
| **Z2** | `ArtNetPacketizer::fillDMXdata` (`artnetpacketizer.cpp:415-418`) | An ArtDmx of 12–17 bytes read bytes 14–17 past the end. Network-reachable | Under 18 bytes is dropped; the data is held to 512 channels, because the controller indexes a 512-byte input buffer with it | `ArtNet_Test::artDmxShorterThanItsHeaderIsDropped`, `::artDmxIsClampedToAUniverse` |
| **Z3** | `ArtNetPacketizer::processTODdata` → `RDMProtocol::byteArrayToUID` (`rdmprotocol.cpp:498-503`) | An ArtTodData whose UidCount promises more UIDs than it carries read past the end | Dropped whole; `byteArrayToUID` refuses fewer than 6 bytes | `ArtNet_Test::artTodDataPromisingMoreUidsIsDropped`, `::artTodDataStillParses` |
| **Z4** | `RDMProtocol::parsePacket` (`rdmprotocol.cpp:284, 301, 304, 311, 318, 326, 374-378, 401, 469→568`) | 1056 crashes, 17 signatures: the header, PDL, per-PID fields and checksum were all read on the packet's own word | The 23-byte header, then PDL + checksum, are checked before they are read; each PID needs the PDL it decodes; `calculateChecksum` stays in its buffer. DEVICE_INFO's sensor count now comes from byte 18 (it was the checksum's low byte) | `ArtNet_Test::rdmEveryTruncationIsDropped`, `::rdmFuzzCrashesAreDroppedQuietly` (one per signature), `::rdmValidRepliesStillParse`, `::malformedPacketsThroughTheController` |
| **Z5** | `E131Packetizer::fillDMXdata` (found reviewing Z2) | A 125-byte sACN packet passed `checkPacket` and read its start code past the end; a count of 0 took the rest of the datagram; a long one overran the 512-byte buffer | Under 126 bytes dropped; 0–512 channels | `E131_Test::dmxPacketsAreBoundedByTheDatagramAndAUniverse` |
| **U1** | `artnetpacketizer.cpp:415/436/446/468` (UBSan, thousands of reports) | Left shift of a negative `char` (universe, address); a byte shifted into bit 31 of an `int` in `byteArrayToUID`/`byteArrayToLong` | Through `quint8` / `quint32`; a top-bit universe byte is no longer sign-extended | `ArtNet_Test::artDmxIsClampedToAUniverse`, `::artTodDataStillParses` |
| **U2** | `ChannelModifier::setModifierMap` (`channelmodifier.cpp:76`) | A float above 127 converted straight to `char` | Through `int` and `uchar`, truncated as before (same values) | `ChannelModifier_Test::upperHalfValues` |
| **U3** | `VCSlider::VCSlider` → `setSliderMode` (`vcslider.cpp:205, 670-682`) | Compared against `SliderMode(-1)`, outside the enum | Starts from `Level` (header default too) | `VCFrame_Test::sliderStartsInAValidMode` |
| **H1** | `GenericFader_Test::aChannelBeingSetUpStaysPutWhileTheUniverseWrites` (Linux Debug + gcov) | Hung to the 300 s watchdog. The stack: the test's writer thread in `sched_yield` / `GenericFader::write`, the setter blocked in `getChannelFader`'s lock | **Test.** The writer re-took the lock back to back with only a yield and a mutex isn't fair, so the setter starved; nothing in `GenericFader`/`Universe` holds the lock across calls, and the app's universe thread writes once a tick. The writer now waits for a set-up between writes | the test itself |

## Matrix 2 overnight: fuzzing and mutation on RC5, fixed in RC6

The overnight run (2026-10-26/27) fuzzed for 62.5 k s per harness on dink
(ASan + UBSan) and ran a mutation pass on the Linux host. Every fuzz crash
but two was Z1 or Z4/U1, already fixed in RC4. The mutation scores are not
valid (the harness restored each file with its old mtime, so the previous
mutant stayed compiled in), but the 426 survivors still mark untested code;
the top three were written up as tests first. Approved for RC6 as Q173.

| ID | Where | Symptom | Fix | Test(s) |
|---|---|---|---|---|
| **N1** | `Fixture::loadXML` (`fixture.cpp`, the address check) → `Doc::applyFixtureToUniverses` → `Universe::setChannelDefaultValue` | A fixture with more channels than a universe holds (repro `<!DOCTYPE Workspace><Workspace><Fixture><ID></ID><Channels>926<`) kept them with only its address reset; adding it wrote the defaults past the 512-byte universe buffers. An assert in Debug, an unchecked heap write in Release. Also in upstream | It loads unpatched, said in the error log, as an out-of-range universe is; the address check uses the mode's channel count; `setChannelDefaultValue` ignores a channel past the universe like `setChannelCapability` and `setChannelModifier` (the other per-channel setters were already bounded). **Fixed in `55815692d`** | `Fixture_Test::loadMoreChannelsThanAUniverse`, `Universe_Test::perChannelSettersStopAtTheUniverseEnd` |
| **N2** | `QLCFixtureHead::cacheChannels` (`qlcfixturehead.cpp`) | `(int)i >= size`: a head `<Channel>` of 2^31 or more went negative, passed, and `mode->channels().at(i)` read out of bounds (repro `<!DOCTYPE FixtureDefinition><FixtureDefinition><Mode Name="H"><Head><Channel>2999999999<`). Also in upstream | Unsigned compare; the same signed compare on head channels fixed in `FixturePower`, `FixtureDefCheck`, `FixtureVisualTraits` and `BeamOptics`. The mode and channel loaders had none. **Fixed in `b8e8595bd`** | `QLCFixtureHead_Test::cacheChannelsPastTheIntRange` |
| **Gap 1** | `VenueRig::unapply/forget/sync` (`venuerig.cpp` 527, 544, 661, 663, 679, 680) | Untested: a stale venue id after the venue is off, the show replaced or a house fixture deleted would drop a show fixture from the save (`Doc::saveXML` skips the venue's ids) | **Test gap, not a bug**: the test passed on main and kills all seven mutants. `0225832a9` | `VenueRig_Test::aVenueOffLeavesNoVenueFixtures` |
| **Gap 2** | `ShowTreeController::saveXML` (`showtreecontroller.cpp` 5554) | Untested: a tree of only a production (nothing generated) would not be saved | **Test gap, not a bug**: passed on main, kills the `(A && B) \|\| (C && …)` mutant. A cue list, even empty, generates its chaser at once, so the production alone is the case. `0225832a9` | `ShowTree_Test::saveKeepsAProductionWithNothingBaked` |
| **Gap 3** | `VenueRig` sidecar read of `<NotOurs To=…>` (`venuerig.cpp` 323) | Untested: an end read 2 too far would grow the range on every load and save; the only test range ended at 512, which the clamp hid | **Test gap, not a bug**: a 5–8 range round-trips write, read, write; kills the `+ 1` mutant. `0225832a9` | `VenueRig_Test::sidecarRoundTrip` |

## Red-team review of RC7, fixed in RC8

A red-team pass (2026-10-27) on `3108a5504` went after what the two matrices
had not: the OSC parser (never fuzzed), web access, the Script engine and
whole-program chaos. Static review plus four new offscreen QTest suites
(`webaccess/test`, `plugins/osc/test`, `engine/test/hostileshow`,
`ui/test/monkey`, with `engine/test/common/hostilecase.h` running each
crash-prone case in a child process), built on the Linux host (Qt 6.8.2,
Debug) and dink (ASan + UBSan, Qt 6.11), loopback only. 23 of its tests
failed, each on a real bug. Approved for RC8 as Q175. Policy chosen for
`-w` without `-wa`: **this computer (loopback, under a loopback name) has
full access; anyone else may look, not touch**; `-wa` with no admin yet lets
only this computer create the first admin.

| ID | Where | Symptom | Fix | Test(s) |
|---|---|---|---|---|
| **H1** | `WebAccessBase` level checks (`m_auth && …`) | `-w` without `-wa`: no auth object, so every check was skipped: anyone on the network could load a show, run functions, toggle Operate, set channels, black out | Every state-changing request checks `permits()`; without `-wa` only a loopback request under a loopback name (not DNS rebinding) is admin, anyone else `LOGGED_IN` (look only). **`Fixed in `c04eb39ba``** | `WebAccess_Test::withoutAuthAProjectCannotBeLoaded`, `::withoutAuthAFixtureCannotBeStored`, `::withoutAuthThisComputerStillHasFullAccess`, `::withoutAuthTheNetworkCanLookButNotTouch`, `::aRebindingNameIsNotThisComputer` |
| **H2** | `WebAccessBase` (`listen(QHostAddress::Any…)`) | Reachable from every network the console was on | Listens on 127.0.0.1 unless `--web-bind`/`-wb`; prints the URL, warns when not loopback. **`Fixed in `c04eb39ba``** | `WebAccess_Test::itListensOnlyOnLoopbackByDefault`, `::itListensWhereItIsTold` |
| **H3** | `Script::handleSystemCommand` | `systemcommand` ran any program, detached, from any show, VC button, autostart or web-loaded project | Off unless **Settings ▸ Advanced ▸ Allow scripts to run programs** (QSettings `scripts/allowPrograms`, default off); refused, with a footer message, for a show from web access (`Doc::loadedFromWeb`); a Script started only by a preview (`FunctionParent::preview()`, Workbench / Show Tree preview / Solo, directly or through collections and chasers) runs nothing. **`Fixed in `76af9c636``** | `HostileShow_Test::aSystemCommandIsOffUntilTheOperatorTurnsItOn`, `::aShowFromWebAccessNeverRunsPrograms`, `::aSystemCommandIsNotRunMerelyByLoadingTheShow`, `Workbench_Test::aPreviewNeverRunsAScript` |
| **H4** | `WebAccessAuth::authenticateRequest` | `-wa` with no admin yet returned `NOT_PROVIDED_LEVEL` (= admin): the first visitor owned the console | No admin = nobody; only this computer may create the first admin (an admin, nothing else), `/config` opens for that. **`Fixed in `c04eb39ba``** | `WebAccess_Test::authWithNoUsersIsNotOpenToEveryone`, `::theFirstVisitorCannotMakeThemselvesAdmin`, `::theFirstAdminComesFromThisComputer` |
| **H5** | `WebAccess::slotHandleWebSocketRequest`, `handleCommonWebSocketCommand` | `GM_VALUE`, `CH`, cue/frame/clock/matrix, IO and user commands read `cmdList[n]` unchecked: one short frame aborted | Every index checked against the count. **`Fixed in `c04eb39ba``** | `WebAccess_Test::shortCommandsDoNotCrash` |
| **H6** | `getWidgetType` / `getWidgetSubIdList` | Unknown widget id dereferenced null | Null checked, `VCWidget::typeToString` called statically. **`Fixed in `c04eb39ba``** | `WebAccess_Test::unknownWidgetIdsDoNotCrash` |
| **H7** | `/loadFixture` | Upload name went into the path: `../escaped.qxf` wrote outside the fixture folder | `sanitizedFixtureFileName()`: base name only, `.qxf` only, else 403; `..` never leaves the web folder, outside files only when a VC widget uses them as its picture. **`Fixed in `c04eb39ba``** | `WebAccess_Test::anUploadedFixtureStaysInTheFixtureFolder`, `::uploadNamesAreSanitised`, `::filesOutsideTheWebFolderAreNotServed` |
| **H8** | `QLC+SYS` (Linux/Pi) | Reboot, halt, network, hotspot, autostart reachable without auth (H1) | Admin only (`permits(SUPER_ADMIN_LEVEL)`), refused and logged otherwise; NETWORK's arguments counted. **`Fixed in `c04eb39ba``** | `WebAccess_Test::systemCommandsNeedAnAdmin` |
| **H9** | `OSCPacketizer::parsePacket` | Bundle size 0xFFFFFFFC: the loop stepped back and forth forever on the GUI thread | Unsigned sizes; an element of 0 or past the datagram ends the bundle; the offset only moves forward. **`Fixed in `82b23d7fc``** | `OSC_Test::aBundleWithANegativeSizeEnds` |
| **H10** | `OSCPacketizer::parseMessage` / `parsePacket` | Huge or cut sizes, unterminated tags or strings: signed overflow (UBSan) and reads past the datagram | Every read bounded by the datagram; nested bundles 8 deep at most; blobs skipped by their size. **`Fixed in `82b23d7fc``** | `OSC_Test::aBundleWithAHugeSizeEnds`, `::aBundleCutInsideASizeIsDropped`, `::tagsWithNoEndAreDropped`, `::aStringWithNoEndIsDropped`, `::randomDatagramsNeverCrash` |
| **M1** | WebSocket upgrade, POST | No Origin check: any site the operator visited could drive the show (CSWSH, CSRF) | `originAllowed()`: no Origin, this computer, or the console's own origin; else 403. **`Fixed in `c04eb39ba``** | `WebAccess_Test::aPageOnAnotherSiteCannotDriveTheShow`, `::aFormOnAnotherSiteCannotLoadAProject` |
| **M2** | `WebAccess` HTML builders, configuration and simple desk pages, `virtualconsole.js` | Captions, function names, notes, universe/profile/device/file/user names written as HTML: stored XSS | `htmlEscaped()` / `jsString()` everywhere a name is written; the page's live updates use `textContent`; new usernames limited to `[A-Za-z0-9._@-]`. **`Fixed in `c04eb39ba``** | `WebAccess_Test::namesFromTheShowAreEscapedInThePage` |
| **M3** | `Script::getValueFromString` | `random(1,0)`: modulo by zero (SIGFPE on x86) | Bounds taken either way round, 64-bit span. **`Fixed in `76af9c636``** | `HostileShow_Test::randomWithEqualBoundsDoesNotCrashTheTimer`, `::randomTakesItsBoundsEitherWayRound` |
| **M4** | `Script::handleSetFixture` | Unpatched fixture or one past the show's universes: `universes[universe]` out of range on the timer thread | Refused unless patched and in range. **`Fixed in `76af9c636``** | `HostileShow_Test::setFixtureOnAnUnpatchedFixtureIsSafe` |
| **M5** | `OSCPacketizer` float/double | `(char)(255 * f)` of NaN, inf, out-of-range: UB | Clamped to 0..255, NaN 0. **`Fixed in `82b23d7fc``** | `OSC_Test::outOfRangeNumbersAreClamped` |
| **M6** | `OSCController::getInputSocket` | Input bound `QHostAddress::Any` | Binds the controller's interface address; multipart cache and path hash table at most 4096 paths. **`Fixed in `82b23d7fc``** | `OSC_Test::theInputListensOnlyOnItsInterface`, `::manyPathsDoNotGrowMemoryWithoutBound` |
| **M7** | `PatchList`, `FocusReport`, `NetworkReport` CSV | No formula-injection guard | `CsvField::escape()`: `'` before `= + - @` TAB CR unless a number; XLSX already wrote inline strings, now tested. **`Fixed in `2da8bbc72``** | `PatchList_Test::aFormulaInANameIsNotRunBySpreadsheets` |
| **L1** | `WebAccessBase::~WebAccessBase`, `QHttpConnection` | Quit with a browser open: connections deleted twice; a plain connection's socket deleted after the server | The list is taken and each connection cut off before delete; the socket pointer is a `QPointer`. **`Fixed in `c04eb39ba``** | `WebAccess_Test::shutdownWithALiveClientIsClean`, `::aHugeUploadIsRefusedEarly` |
| **L2** | `QHttpConnection` | No body limit; an unparsable URL asserted | 413 over 64 MB (Content-Length or chunked total), 400 for a bad URL, never an assert. **`Fixed in `c04eb39ba``** | `WebAccess_Test::aHugeUploadIsRefusedEarly`, `::aMalformedUrlDoesNotAbort` |
| **L3** | `/tmp/qlc_showdebug.log` (`QLC_SHOW_DEBUG` only) | Predictable world path | **Open**: debug-only, left for a later pass | — |

The red team's `http()` test helper waited with blocking `waitFor*()` while
the server ran in the same thread, so no test could read a reply; it waits
with the event loop running now, and the suite takes ~35 s instead of ~110 s.
The tests that stand in for a visitor from the network turn off
`WebAccessBase::m_trustLoopback`, since a test can only reach loopback. The
monkey (`88923d58c`) runs 20 s in the gate and CI (`QLC_MONKEY_MS` from its test.sh, 60 s
by hand); it and web access are in the serial list. Not hardened: the QML
UI's web access (`webaccess-qml.cpp`, qmlui is off) and argument splitting
in the Pi's nmcli commands (admin only now).

## Matrix 2: show compatibility, real Art-Net, TSan with an instrumented Qt (items 1, 3, 7)

Run 2026-10-26 on ender (items 1, 3) and val (item 7). Items 1 and 3 used a
`git archive` of `778ee7d21` built RelWithDebInfo against Homebrew Qt 6.11.1 in
the session scratchpad, plus QLC+ `QLC+_4.14.4` (from `~/git/qlcplus`, archived and
built out of tree). The harnesses were a small offscreen program per build: open
through `App::loadXML`, save through `App::saveXML`, open again and save again.
Each step dumped a fingerprint (every fixture, group cell, scene value, chaser step,
EFX/RGB matrix setting, collection member, universe and patch, monitor counts,
palettes, Show Tree, and VC widget id/type/caption/geometry/parent). A
path-keyed XML diff compared the original with the save. The Art-Net run used the
same App with a timeline and `InputOutputMap::universeWritten` logged against
`sudo tcpdump` on vlan0.

**Item 1, compatibility.** The test covered 45 files: every `.qlcc`/`.qxw` under
`~/Crew` (copies, read-only; the `Current`/`dh` symlinks are the same files),
`test-workspaces/` and `resources/samples`. Among them are DanceHysteria 0.1–0.6,
written by qlcconsole builds 1352–1773 across the 0.1 and 0.2 releases, so no old
build was needed. Every file opened with no crash or modal. The fingerprint after
the first save equals the one after the first open for 39/45, and
saving twice gives identical bytes for those 39. The 6 that differ are all C1. The
XML element counts (fixtures, functions, groups, palettes) equal the loaded counts
in every file. Every other change in the XML diff is a deliberate migration:
palette type folders, lock and layer migration, the SA1 junk zoom/flags, feet
units by preference, `MonoColor`→`Color`, the `FadeOout` typo, Qt 6 font strings,
generic dimmer mode names, and patches following the network.

**Stock QLC+ 4.14.4 opening qlcconsole saves.** All 45 current-build saves
opened, round-tripped (stable bytes except C1 again) and closed in stock with no
crash. As expected, stock drops the Show Tree, `Embedded` definitions and scripts,
trusses, stage pieces, layers, `MonitorImage`, the Aim/Effect/Gobo/Shutter
palettes, the qlcconsole VC widget type 12 ("full show"), and `Patched`. It reads
the grid's whole numbers and ignores `Exact*`. The two things it chokes on are
C2 and C1 below.

**Item 3, real Art-Net.** The test ran `Office.qlcc` (a copy) on its real patch
(Art-Net to the CR041R at 172.18.2.10, universe 0, vlan0). The timeline: idle,
then *1-All White Full*, GM 128 / 0 / 255, blackout on/off, then the *Fixture
check* chaser (in memory: loop, fade 300 ms, hold 700 ms), with a blackout and
GM 64 during the chase, then the Blackout collection and stop-all. The run took
58 s and sent 338 frames.

- Every one of the 257 engine frames outside blackout reached the wire. The time
  between the engine frame and the packet was a median 0.03 ms (maximum 6.3 ms).
- There were no sequence breaks (1, 2, 3…), no duplicates, and nothing on the
  wire that the engine did not produce.
- The chase ran at 50 Hz: gaps between 19.83 and 20.05 ms (median 20.00). An
  idle universe is refreshed every 2.000 s, which is Standard mode's keepalive.
- GM 128 put 0x65 (101) on the wire as 0x33 (51), and RGB 0xff as 0x80.
  Blackout zeroes the intensity and colour channels and keeps pan/tilt. That is
  by design, but `universeWritten` still reports the values from before the
  blackout.
- Recovery from blackout was immediate.
- The node's ArtPollReply was stable throughout (CR041R_001, 4 ports,
  GoodOutput 0x80, no merge bit).
- Interference: the user's own qlcconsole (pid 72290, `-o ~/Crew/Current/…/DanceHysteria-0.6.qlcc`,
  running since 25 Oct 21:25) was also on the wire. It sends all-zero ArtDmx to
  the same node and universe every 2 s, which the analysis separated out (N1).

| ID | Where | Symptom | Reproduction | Severity | Likely fix |
|---|---|---|---|---|---|
| **C1** | `RGBMatrix::loadXML` (`engine/src/rgbmatrix.cpp:461-463`, `RGBAlgorithm::loader` returns NULL for an unknown script) and `RGBMatrix::saveXML` (`:542-544`, writes `<Algorithm>` only if non-NULL) | **An RGB matrix whose script this build doesn't have loses its effect on the first save.** The algorithm name and type disappear from the file. Its `<Property>` values are kept but orphaned. The next open falls back to Stripes. No warning at open. Same code in stock 4.14.4, so it also hits a qlcconsole show opened and saved in stock: qlcconsole's embedded scripts (`Lantern`, `Staff Rise`) become NULL there, and the show comes back as Stripes. | `crew` `CORP-DEV-1.0.qxw` RGB matrices 727–729 (`Plasma (Colors)`); `talesuntold-0.5-v5.qxw` (9), `tpihab*.qxw` (4–5); qlcconsole→stock: `lantern-test.qxw`, `staff-test.qxw` | **Medium** (silent loss of show content) | Keep the unresolved `<Algorithm>` (type, name, properties) and write it back unchanged, and say "script X not installed" at open. **Fixed in `3b4b75dda` (RC5):** `RGBMatrix::loadXML` copies the `<Algorithm>` element; when it can't be loaded it is kept (`unknownAlgorithmXML()`), written back verbatim by `saveXML`, carried by `copyFrom`, replaced by `setAlgorithm`, and the load log names the matrix and the script. Test: `RGBMatrix_Test::anUninstalledScriptSurvivesASave`. |
| **C2** | qlcconsole staged fixtures (`<Patched>False</Patched>`, written at U0/A0) read by stock `Doc::addFixture` | **Stock QLC+ deletes every staged fixture.** It doesn't know `Patched`, sees each one overlapping channel 0, and "cannot be created". Saving in stock makes the loss permanent: DanceHysteria 0.1/0.2 lose 21 of 208 fixtures, stage-structures-demo 60 of 202. | `QLC+ 4.14.4 -o` any show with Inventory fixtures; stderr `Fixture "AT2-3z" cannot be created.` | **Medium** for the "a .qxw stays as QLC+ can read it" promise (`App::saveXML`) | Write staged fixtures where stock skips them (an element of their own), or at a free address range |
| **C3** | `App::saveXML` (`ui/src/app.cpp:5093-5134`) → `Doc::saveXML` → `MonitorProperties::keepPatternsWithShow` (`engine/src/monitorproperties.cpp:163`) → `Doc::keepWithShow` (`engine/src/doc.cpp:431`) | **Save As to another folder breaks truss/tower pattern images and writes beside the old show.** `Doc::workspacePath()` is still the previous file's folder during the save, because `setFileName` runs after the rename. The picture is copied into the old folder's `images/`, and the new file says `images/…jpg` relative to a folder that has no `images/`. (Every `normalizeComponentPath` in a Save As is relative to the old folder. That part is upstream.) | Open DanceHysteria-0.1.qlcc (truss `SockPattern` in `~/Downloads`), Save As in another folder: `images/stones-pattern-…jpg` appears beside the original; the saved file's folder has none | **Medium** | Set the target folder for the duration of the save (or pass it to `saveXML`). **Fixed in `b19b58706` (RC5):** `App::saveXML` sets the target's folder as the workspace path for the whole of a non-auto save (restored if it fails); `Doc::normalizeComponentPath` no longer takes a sibling folder with the same prefix for the show's own. (The UI's Save As set the path first already; the route that missed it was any other caller of `App::saveXML`, such as the matrix harness.) Test: `ShowFiles_Test::saveAsToAnotherFolderTakesThePictures`. |
| **C4** | `MonitorProperties` load/save of `MonitorImage` (`monitorproperties.cpp:3594`, `:4502`) and the Background→image migration (`:3866-3880`) | `MonitorImage@Source` is written as an absolute path and read raw (no `normalizeComponentPath`/`denormalizeComponentPath`, unlike `SockPattern`). An old show's relative `<Background>Stage Layout.jpg</Background>` becomes `/abs/path/Stage Layout.jpg` on the first save, so the plot loses its floor image when the show moves. | Any 4.x show with `<Background>`: the saved file has `MonitorImage … Source="/private/…/Stage Layout.jpg"` | Low–medium | Normalise/denormalise `Source`, and keep it with the show like the patterns |
| **C5** | `MonitorProperties::loadXML` ZPos heuristic (`monitorproperties.cpp:3389-3400`) | A QLC+ 5 show (positions in mm; Z is depth there) has every \|Z\| > 100 divided by 1000 and saved back in metres. Reopened in QLC+ 5, the depth axis collapses to about 0. | `talesuntold-0.5-v5.qxw`: `FxItem ZPos="6728.11"` → `"6.72811"` (169 items) | Low (QLC+ 5 files) | Only apply the repair to qlcconsole-written files (Creator), or skip v5 files |
| **C6** | 4.14.4 `<InputOutputMap><NetworkServer Type AutoStart Name Password/>` | qlcconsole (4.14.2 base) drops the stock 4.14.4 network-server settings on save. | `crew` `QLC/wled_test.qxw` | Low | Carry the element through unchanged. **Fixed in the 4.14.4 upstream intake (0.3.1):** `InputOutputMap` reads and writes `<NetworkServer>` as stock 4.14.4 does (ported from upstream e4d8a4a8c), reset on every load. Test: `InputOutputMap_Test::networkServerSurvivesASave`. |
| **C7** | Output patch with `UID="None" Line="4"` (`InputOutputMap::loadPatch` / `NetworkRoute::resolve`, `engine/src/networkroute.cpp:169`) | An unresolved stock patch is bound by line number to whatever interface this Mac has at index 4 (192.168.20.166 on ender), so it starts sending there. Saving writes `Network="192.168.20.0/23"`, which makes the guess permanent. | `test-workspaces/surfacetesting.qxw.autosave`, universe 8 | Low | Leave a `None` UID pending rather than resolving it by index |
| **N1** | `ArtNetPacketizer::m_sequence` (`plugins/artnet/src/artnetpacketizer.cpp:42-45,144-158`), keyed by Art-Net universe per controller | Every QLC universe on one interface that sends Art-Net universe N to *different* nodes shares one sequence counter. In DanceHysteria-0.6, 18 universes send to Art-Net universe 0 at different IPs, so each node sees the sequence go up by 18 per packet. A universe ≥ 4 starts at 0 (Art-Net's "sequence disabled"). Seen live from the user's running console (above). | Capture vlan0 with DanceHysteria-0.6 running: ArtDmx to .10 with seq 73, 91, 109, … every 2.000 s | Low (nodes that reorder by sequence still see it increasing) | Key the counter by (node address, Art-Net universe) |
| **N2** | — | 50 Hz ArtDmx during changes. Art-Net recommends ≤ 44 Hz for a universe; inherited from QLC+. A second console on the same host sending the same node and universe isn't noticed. The node doesn't report merging either, because the source IP is the same. | — | Info | — |

**Item 7, TSan with an instrumented Qt (val).** It had stopped at 13:00 (exit
40): qlcconsole's configure needs `Qt6SerialPort` (dmxusb, uart) and
`Qt6WebSockets` (webaccess), and `~/matrix2/qt-tsan` had neither. `TextToSpeech`
is QUIET and not linked on macOS. qtserialport and qtwebsockets 6.9.3 were added
to `run.sh`, with the same flags as the other modules (`-fsanitize=thread`,
nice 15), and the run resumed from its stamps at 16:53, NJ=10. The build finished
at 17:15, the tests at 17:22, and it ended **DONE exit 0**. The 14 test programs
that "failed" exited 66, which is TSan's report exit code: every QtTest total in
`tests.log` is 0 failed. `summary.md` lists 129 reports, 47 keys. Its "ours"
column says 0, which is wrong: the dedupe's tree match missed `~/matrix2/tsan/src`
paths, and most keys are in-tree. The top keys:

- `InputOutputMap::setGrandMasterValue` (`inputoutputmap.cpp:466/469`) vs `releaseUniverses` (`:394`): 19 + 3 + 2 reports.
- `Function::preRun`/`isRunning` and `stop`/`stopped` (`function.cpp:1040/1095`, `:1212/1219`): 7 + 7 + 5.
- `Universe::postGMValue` vs `updatePostGMValue` (`universe.cpp:772`): 6.
- `MasterTimer::timerTick` (`mastertimer.cpp:198/272`): 5.
- `Universe::preGMValue` vs `writeMultiple` (`universe.cpp:831/1439`): 4.
- `Universe::setInhibitOutput` vs `dumpOutput` (`universe.cpp:204/1090`): 4.
- The `MasterTimer` function list (`stopAllFunctions`/`stop`/`fadeAndStopAll` vs `removeAt`, `mastertimer.cpp:360-438`) and `~Function` vptr vs `timerTickFunctions:438`.

With Qt instrumented these are no longer the E3 false positives and need triage
(`~/matrix2/tsan/{summary.md,reports/,tests.log}`; the source is `52a23d27e`).

Triage (2026-10-26): 13 distinct issues, none high. The two medium and two of
the low ones are fixed for RC5 (IDs are the triage's, not this file's
matrix-1 M/L rows):

| ID | What | Severity | Status |
|---|---|---|---|
| **TSan M1** | `MasterTimer::stopAllFunctions()` polled `m_functionList.size()` unlocked and could return between a tick's stops and its start-queue move; the queued function then ran on for File ▸ New/Open to delete (use-after-free). `m_stopAllFunctions` a plain bool. | Medium | **Fixed in `19b70095b`:** atomic flag, `runningFunctions()` locked, the wait also needs the start queue empty, `stop()` clears both lists (upstream `7e2f4ae84`). Test: `MasterTimer_Test::stopAllTakesTheQueuedFunctionsToo`. |
| **TSan M2** | `Chaser::createRunner` parented the runner to the GUI-thread Chaser from the MasterTimer thread (Qt's child events and the main thread's scope level, from the wrong thread). | Medium-low | **Fixed in `19b70095b`:** no parent; `~Chaser` and `postRun` delete it, nothing reads its parent. Test: `Chaser_Test::theRunnerHasNoParent`. |
| **TSan L1** | `Function` `m_stop`/`m_running`/`m_paused`/`m_preserveAttributes` plain bools across the GUI and timer threads; a stop's preserve choice could be seen after the stop. | Low | **Fixed in `19b70095b`:** atomics; the preserve flag is written before a release-store of `m_stop`, read after an acquire. Test: `Function_Test::aStopFromAnotherThreadCarriesItsPreserveChoice`. |
| **TSan L3** | `Universe::m_inhibitOutput` (Blind) a plain bool, set on the GUI thread and read per frame. | Low | **Fixed in `19b70095b`:** `std::atomic<bool>`. Test: `Universe_Test::blindIsSeenFromTheOutputThread`. |
| TSan L2, L4, L5 | GUI per-byte reads of the universe buffers; `m_beatRequested`; `Function::elapsed()` from EffectInstance. | Low | Open (after 0.3.0). |
| TSan B1–B5, T1, X1 | Benign flags, test-harness reads, and TSan faulting inside QV4's JIT. | — | Open; no product change needed (T1 is test stubs). |

**Side effects to know about.**

- The item 1 harness loaded the Art-Net/E1.31 plugins for the qlcconsole pass
  (17:00–17:03). Each show was open for 2–6 s, so it would have sent its patched
  zero-level ArtDmx and ArtPoll from vlan0 to the IPs in those shows:
  DanceHysteria's 172.18.2.20/.21/.50–.55/.201–.213 and others, beyond the
  permitted node.
- The capture that started at 17:02:57 saw ArtDmx only to .10. Only .10 and the
  Pi (.218) answered polls, so nothing else on the VLAN was listening. Earlier
  traffic was not captured.
- The stock pass and every later run had the network plugins removed.
- No crash reports were caused (`~/Library/Logs/DiagnosticReports` has nothing
  new since 11:37).
- Scratch: `…/scratchpad/m2b` (builds, `shows/` copies, `out-qlc/`, `out-up/`,
  `dmx/run2.{pcap,log,analysis}`, `harness/`).

## Matrix 2: scale, leaks and clean installs on RC4 (items 2, 4)

Run 2026-10-26 21:00 → 2026-10-27 11:10 against RC4, `fbee52ab5`: the DMG
`qlcconsole-0.3.0-universal-20261026-fbee52ab5.dmg` (sha256 `f71c61a9…`) and CI
run 40419087623's Windows installer (attempt 2,
`qlcconsole-0.3.0-20261027-fbee52ab5-win64-setup.exe`, sha256 `3fa5d2a0…`).
The source builds were `git archive` copies (Linux `~/matrix2/rc4`, ender
scratchpad), never a checkout. No network plugin was loaded in any run.
The DMG copies had `libartnet/libe131/libosc/libos2l/libwled` moved out.
Windows had the same DLLs moved out of its temp install. The build trees had
no plugins. WLED `<IPAddress>` and Art-Net `ManualTarget`s were stripped from
the soak shows, so the WLED config check had nothing to poll.

**Item 2, scale (Linux build host, Release, after the mutation job ended at
09:30).**

- `run.py validate`: all seven scenarios ran within budget. Its one "FAIL" is
  `efx-40-16u` at +25.3% (p99 1.17 ms against 0.94 ms). That baseline was
  captured on ender (arm64), so cross-machine numbers aren't comparable.
  Everything else ran 24–65% faster than baseline.
- `capability`: the light ceiling is 180 universes (p99 11.3 ms). The heavy
  ceiling is 40 universes; at 60 the p99 is 24.5 ms.
- Realtime, 75 universes × 67 fixtures (15,075 channels), 600 s: the gap
  p50 was 20.01 ms and p99 21.1 ms. 4 of 29,992 ticks were more than 1.5×
  late, and the worst was 65.8 ms. The harness reports "running funcs=0" in
  this mode, so this is a timer/universe test, not a function-load test.
- Chaos: 120 s, 119,101 ops, survived. Concurrency: 300 s, 5.0 M ops,
  survived. Both ran on Linux, where the harness doesn't measure RSS
  (`RSS 0.0 MB`; the sampler is Mach-only), so their "leak 0.00 MB/hr" means
  nothing there.
- Full app, 80 universes × 64 fixtures, 110 functions, 900 s of WebSocket
  torture:
  - 2,105 rounds, every one answered, no crash.
  - CPU averaged 361%.
  - RSS went 256 → 488 MB within the first ~3 minutes and then stayed flat
    at 488 MB for 12 minutes. The driver's "POSSIBLE LEAK (+232 MB)" verdict
    is the start-vs-end threshold catching warm-up.

**Item 2, leaks.** The harness is `m2leak`, an offscreen program on the real
`App` (`QSettings` in a temp INI, `HOME` in a sandbox). Each cycle does the
following:

1. `clearDocument` + `loadXML`.
2. Build edits: 6 collections opened on the Workbench, each with a layer
   added, a layer moved, and undo / undo / redo / undo; 8 scenes opened in
   the Look Editor; all three sections.
3. Run: Operate mode, 12 collections/chasers/matrices started for 2 s, then
   stopped.
4. `clearDocument`.

The shows alternated between DanceHysteria-0.3 (stripped) and
`Sample.qxw`.

- ender, macOS `leaks` + MallocStackLogging, 20 cycles: footprint stayed
  flat at 146–160 MB. Unreachable memory was 168 KB after cycle 2 and
  2.1 MB after cycle 20, about 110 KB a cycle.
- Linux, ASan + LSan + UBSan Debug, 40 cycles with full teardown: 2.96 MB
  in 20,469 allocations. A 1-cycle control showed 64 KB, so about 74 KB a
  cycle.
- The two agree on three sources (L1–L3 below). `heap` class census on ender
  (cycle 2 vs 16) adds the reachable growth in L4 and the list growth in M1.
- A first harness version never ran `deleteLater()` (no `exec()`), and
  showed ~15 MB/cycle of Workbench stacks and Look Editors. That was the
  harness: with deferred deletes processed, those are freed.

**Item 2, 24 h soak on val** (Intel i7-10700K, macOS 26.7.2).

- Setup:
  - The DMG's own app, copied to `~/matrix2/rc4-soak/qlcconsole.app` with the
    network plugins removed and Qt 6.9.3's `libqoffscreen.dylib` added.
  - Started directly with `QT_QPA_PLATFORM=offscreen` and
    `HOME`/`CFFIXED_USER_HOME` set to a sandbox, at nice 10. It is not on
    screen; the October soaks were.
  - `stresstest/soak24/run_soak.py`, patched in the copy for a direct
    launch, `--strip-outputs`, 3 × 8 h.
  - The same DanceHysteria-0.3 / -STRESS shows as the Linux soaks, with that
    host's 102 fixture definitions in the sandbox.
- Started 2026-10-26 21:50:03 EDT; due to end around 2026-10-27 21:51.
- A first start at 21:04 was stopped at 21:49 because the sandbox lacked
  those definitions: ender's user folder holds symlinks into the legacy QLC+
  folder, which were dangling on val.

| Run | Real: over budget/h, worst | Scaled: over/h, worst | Rough | RSS (real) | Crashes |
|---|---|---|---|---|---|
| **val RC4, 2026-10-27** | **9.9/h (79 in 8 h), 40.3 ms** | interim at 07:09: 47 in 1.3 h (~36/h), 90.9 ms | pending | 156 → 360 peak → 352 MB | 0 so far |
| Linux 2026-10-25 (2.67 h phases) | 11.6/h, 37.4 ms | 5,300/h, 56.2 ms | 19.5/h, 55.4 ms | 340 → 365 MB | 0 |
| Linux 2026-10-16 (8 h phases) | 11.3/h, 44.0 ms | 7,045/h, 101.2 ms | 20.6/h, 57.1 ms | 234 → 291 → 277 MB | 0 |
| kronk 2026-10-16 (on screen, `--heap-track`) | – | – | – | 312 → 492 → 486 MB | 0 |

The final numbers are in `val:~/matrix2/rc4-soak/run24/summary.json`. Scaled
ticks over budget on val's i7 are far below the Linux host's.

**Item 4, clean-machine macOS (EvoAudioMBP).** Intel, macOS 13.7.8, stock
QLC+.app in /Applications, Xcode CLT already present, nothing installed.

- Gatekeeper, with `com.apple.quarantine` set as Safari would:
  - DMG: `spctl -t open` accepted (Notarized Developer ID); `hdiutil verify`
    VALID.
  - App: `spctl -t execute` accepted (Notarized Developer ID);
    `codesign --verify --deep --strict` OK.
  - Ticket stapled on both the app and the DMG.
- `LSMinimumSystemVersion` is 12.0. Every Mach-O in the bundle has `minos`
  ≤ 12.0 in both slices (66 binaries; 4 at 11.0), and nothing links outside
  the bundle or /System and /usr/lib. Below 13.7, so fine.
- The x86_64 slice runs (`vmmap`: Code Type X86-64):
  - `--version` prints `0.3.0 (build 4.14.2+qlcconsole.2302, fbee52ab5)`.
  - A headless `-o Sample.qxw -w`: the web page served 224 VC buttons and
    18 sliders.
  - Sockets: TCP *:9998 listen, and UDP *:3330 (the ENTTEC Wing plugin),
    listening only.
  - This needed the offscreen plugin added and an ad hoc re-sign (see I1).
- First run created:
  - `~/Library/Preferences/org.qlcplus.qlcconsole.plist` (`monitor.rulers`,
    `units.seededFromShow`, `workspace.recent0`).
  - `~/Library/Application Support/qlcconsole/{Bundles,EffectPresets,EffectScripts,Fixtures,InputProfiles,MidiTemplates,ModifiersTemplates,RGBScripts}`.
  - `~/Library/Application Support/QLC+/HIDProfiles` (L5).
  - A CoreMIDI `ByHost/com.apple.MIDI.*.plist`.
- The update check doesn't run offscreen (`backgroundChecksAllowed()`), so it
  can't be seen headless.
- The first-launch dialog itself needs the screen and wasn't exercised.
- RC5 (`38e1963b8`, `…20261027-38e1963b8.dmg`) got a quick check on the same
  Mac: notarized and stapled, `spctl` accepted, `LSMinimumSystemVersion`
  12.0, and `--version` printed `build 4.14.2+qlcconsole.2307, 38e1963b8`.

**Item 4, Windows 10 VM.** The VM was **not clean**:

- qlcconsole 0.3.0 (Matrix 1's `05534ef92`) at `C:\qlcconsole`, with its
  Apps & features entry, file associations and Start Menu.
- Stock QLC+ at `C:\QLC+`.
- `%USERPROFILE%\qlcconsole` (Branson's data) and `%USERPROFILE%\QLC+`
  (empty folders).
- `HKCU\Software\qlcplus\{qlcconsole,Q Light Controller Plus}`.
- VC++ runtimes 2008–2022 (x86 and x64).
- Python 3.9.
- The `qlcsoak-*` scheduled tasks.
- No Qt or MinGW on PATH.

The registry keys and Start Menu folder an install overwrites were exported
first and restored byte-for-byte afterwards.

- Silent install: `/S /D=%TEMP%\qlcrc4\inst` exited 0 in 11.6 s (3,321
  files, 308 MB). The installer is **not Authenticode-signed**, and
  `qlcconsole.exe` has no FileVersion resource.
- Dependencies: a PE import walk of all 176 EXE/DLLs resolves every import
  to the install folder, its own folder or System32. Only `qwindows.dll` is
  shipped as a platform plugin.
- `--version` and `-platform offscreen --version` print `0.3.0 … fbee52ab5`
  with `qoffscreen.dll` from CI's rig bundle of the same run (identical
  `Qt6Core.dll`) via `QT_QPA_PLATFORM_PLUGIN_PATH`. Without it, both hang
  (I1).
- Headless `-o Sample.qxw -w`: the same 224 buttons and 18 sliders. The
  sockets were TCP :9997 and UDP :3330 only, and no module loaded from
  outside the install or Windows.
- The log says `FFmpeg version 9.0.2 GPL version 3 or later` (I2).
- Uninstall: `uninstall.exe /S _?=` exited 0 and removed every file (only
  `uninstall.exe` stays, which `_?=` causes). It also removed the Uninstall
  key, the associations, `HKCU\Software\qlcconsole` and the Start Menu
  folder. **`HKCU\Software\qlcplus\qlcconsole` (the real settings) stays**
  (L6).

| ID | Where | Symptom | Reproduction | Severity |
|---|---|---|---|---|
| **M1** | `InputOutputManager::slotUniverseAdded` (`ui/src/inputoutputmanager.cpp:747-751`) with no reset on close/open; `updateList()` (`:393`) is not called on a load | **Each opened show appends its universes to the Inputs/Outputs list without removing the previous show's rows.** The rows keep the old show's names. Rename uses `currentRow()` as the universe index (`:740`, `:759`), so a stale row renames the wrong universe or none. | `m2leak` counts the `QListWidget` under InputOutputManager after each `clearDocument()`+`loadXML()` (the File ▸ Open path, `app.cpp:2876-2883`): 78 → 156 → 234 for DanceHysteria-0.3 opened 3 times. In the app: open a show, open another, look at Inputs/Outputs. | **Medium.** **Fixed in `04def1125` (RC6):** the list keeps one row per universe (on add, remove, `Doc::cleared`; rebuilt on `Doc::loaded`), and rename, passthrough, slot and delete use the row's universe, not its number. `AppViews_Test::theUniverseListMirrorsEachShowOpened` |
| **M2** | `QLCFile::mergeLegacyUserData()` (`engine/src/qlcfile.cpp:259-275`) on Windows | (a) `newRoot` is `getenv("HOME")/qlcconsole`. With HOME unset (a normal desktop session; OpenSSH and MSYS set it), that is `\qlcconsole` on the **current drive**: `C:\qlcconsole`, the default install folder. (b) `QFile::link` makes Windows shell links (`4c 00 00 00…`) named `*.qxf`, which the loader can't read: "Fixture definition loading from …Legacy-Test.qxf failed: Resource error occurred", at every start. **Old QLC+ user fixtures never reach qlcconsole on Windows.** | VM, isolated: profile with `QLC+\Fixtures\Legacy-Test.qxf`, HOME removed, cwd a `subst` drive: the run wrote `Q:\qlcconsole\Fixtures\Legacy-Test.qxf` (a .lnk) and `Q:\qlcconsole\.legacy-linked`, and nothing under the profile. With HOME = the profile, the link lands right but fails to load. | **Medium** |
| L1 | `Fixture::genericDimmerDef/genericDimmerMode` (`engine/src/fixture.cpp:1090-1135`, from `setChannels:188` and `loadXML:1671/1694`) | Every generic dimmer gets its own `QLCFixtureDef` that nothing deletes (`~Fixture` is empty). `setChannels` after `loadXML` makes a second one. Same in upstream. | LSan: 1,160 channels/capabilities and 200 modes over 40 cycles; `leaks`: the largest group | Low (≈50 KB per DanceHysteria open) |
| L2 | `RGBAudio::~RGBAudio()` (`engine/src/rgbaudio.cpp:45-51`) → `Doc::audioInputCapture()` (`doc.cpp:900`) | Destroying an RGBAudio calls `audioInputCapture()`, which **creates** the Doc's `AudioCaptureQt6` (buffers, FFTW plan, BeatTracker). `RGBAlgorithm::algorithm()` builds and destroys a temporary RGBAudio for every RGB matrix constructed. One capture leaks per loaded show. | LSan: 20 captures, ~1.3 MB (BeatTracker FFT buffers) over 20 DanceHysteria loads | Low |
| L3 | `QLCFixtureDef::loadXML(const QString&)` (`engine/src/qlcfixturedef.cpp:464-469`) | When the file can't be read, it returns `ReadError` without `QLCFile::releaseXMLReader()`, leaking the reader and its QFile. It's hit by `Fixture::loadXML`'s "project local path" fallback (`fixture.cpp:1594-1602`). For a show posted to `/loadProject` (no file name), that fallback reads `"/<Man>-<Model>.qxf"` at the filesystem root (137 log lines per load on val). | LSan: 1,200 objects; `leaks`: 158 | Low |
| L4 | `FixtureManager::updateGroupMenu()` (`ui/src/fixturemanager.cpp:1596-1597`) | `removeAction()` doesn't delete the menu's own actions, so every call leaves them as children of `m_groupMenu`. That is 1,323 `QAction`s (~0.6 MB) per DanceHysteria open, growing for the session. Same in upstream (`m_groupMenu->clear()` would delete them). | `heap` census: QAction +18,522 over 14 opens; `m2leak` counts 1,578 → 2,901 → 4,224 menu-child actions | Low |
| L5 | `plugins/hid/hidplugin.cpp:41` | The user HID profile folder is hard-coded to `Library/Application Support/QLC+/HIDProfiles` under the home folder on every platform. On a clean Mac it creates stock QLC+'s folder, the same on Linux (`~/Library/…` on the build host), and on Windows `%USERPROFILE%\Library\Application Support\QLC+\HIDProfiles`. | First run on EvoAudio and in the Windows temp profile | Low |
| L6 | NSIS uninstaller (`platforms/windows/qlcconsole.nsi.in`, "This will delete all settings") | It deletes `HKCU\Software\qlcconsole` (`Install_Dir`, `StartMenu_Dir`); the app's settings are under `HKCU\Software\qlcplus\qlcconsole` and stay. Installing a second copy also takes over the first one's Apps & features entry, associations and shortcuts with no warning. | VM: present after the uninstall | Low |
| L7 | `OutputLayer::~OutputLayer → clear() → set()` (`engine/src/outputlayer.cpp:25, 53`) from `FocusSession::~FocusSession` (`focussession.cpp:150`) during `~Doc`'s child deletion | UBSan: a member call on a `Doc` whose destructor has run (`doc.cpp:516`). It's safe today only because `~Doc` nulls `m_ioMap`. Also `UpdateManager::eventFilter` compares a `QPointer<QLabel>` while the label is mid-destruction (`updatemanager.cpp:126`). | ASan/UBSan `m2leak` with full teardown (`delete app`) | Low (UB at quit) |
| I1 | DMG, installer | Neither ships an offscreen platform plugin (DMG: `libqcocoa` only; Windows: `qwindows.dll` only). So `qlcconsole --version` builds a `QApplication` first (`main.cpp:313`), needs a display, and fails over SSH. On Windows it hangs on Qt's invisible "no platform plugin" message box. On macOS 13, an offscreen plugin added to the notarized bundle needs an ad hoc re-sign, because a modified bundle stalls in dyld (unquarantined) or gets Gatekeeper's "can't be verified" dialog (quarantined). | `QT_QPA_PLATFORM=offscreen …/qlcconsole --version` | Info (headless checks only) |
| I2 | Windows installer | Was: MSYS2's FFmpeg 9.0.2 built "GPL version 3 or later" while the DMG ships LGPL FFmpeg 7.1.1. Fixed in RC8: CI swaps in BtbN's LGPL build of FFmpeg 9.0.2 (pinned, sha256) and ships the licence texts (`licenses\`); `release-check.ps1` checks `win.ffmpeg.licence`, `win.ffmpeg.licence-text`, `win.run.multimedia`. | CI `release-check-windows` | Fixed (RC8) |
| I3 | CI artifacts | Run 40419087623 has two installers for one SHA: `…-20261026-…` from attempt 1 and `…-20261027-…` from attempt 2. They differ in bytes and in name, because the name carries the UTC build date. | `gh api …/runs/40419087623/artifacts` | Info |
| E1 | `stresstest/` | (a) `run.py validate` compares against an arm64 baseline on any host. (b) RSS is sampled only through Mach, so chaos/concurrency "leak 0.00" on Linux is unmeasured. (c) `--mode realtime` started no functions. (d) `stress_driver.py` calls warm-up a leak. | as above | Test |

**Side effects and clean-up.**

- **EvoAudioMBP:** A Gatekeeper window appeared on its screen at 21:08:
  CoreServicesUIAgent's `CSUIGatekeeperProgressWindow`, then an alert. It was
  caused by running a quarantined, modified copy (quarantine had been copied
  onto the inner files by `ditto` from the quarantined DMG). The process was
  killed. `killall CoreServicesUIAgent` didn't end the agent, so the alert
  may still be on that screen until dismissed. Everything added was removed,
  including:
  - `org.qlcplus.qlcconsole.plist`
  - `~/Library/Application Support/{qlcconsole,QLC+}`
  - the CoreMIDI ByHost plist
  - `com.apple.coreservices.uiagent.plist` and `com.apple.ReportCrash.plist`
    (created by that run)
  - `~/Library/Logs/DiagnosticReports/qlcconsole-2026-10-26-212348.ips` (my
    SIGABRT: offscreen plugin refused)
  - hdiutil's `Application Support/DiskImages` and `Logs/fsck_hfs.log`
  - the LaunchServices registrations of the temp copies (`lsregister -u`)
  - `/tmp/qlcrc4`, `/tmp/qlcrc5`

  syspolicyd's own record of that assessment is root-owned and was left.
- **ender:** At 21:02 a mistyped `--help` (no `QT_QPA_PLATFORM`) started the
  DMG copy with the Cocoa platform on ender's screen for about 2 minutes
  before it was killed. The settings plists' times show they were not
  written.
- **Windows VM:** It ended as it started. `%TEMP%\qlcrc4` and val's
  `~/qlcconsole-windows/rc4-matrix2` were removed.
- **Linux:** `~/matrix2/rc4` keeps only the logs and JSON (18 MB). The
  driver's Release run rewrote `~/.config/qlcplus/qlcconsole.conf`, as the
  earlier soaks there did.
- **val:** The soak writes `~/Library/Preferences/org.qlcplus.qlcconsole.plist`
  (cfprefsd ignores `CFFIXED_USER_HOME`). It is new on val: delete it, and
  `~/matrix2/rc4-soak`, after collecting the results.

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
