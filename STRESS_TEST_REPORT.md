# Overnight Stress-Test Report — 2026-07-19 → 20

Branch `programmer-mode`, commit `278f9b937` (session work) + follow-up stress fixes
(uncommitted at time of writing — see "Commit" at the end).

## TL;DR
- **No crashes** across the whole engine test suite (59 suites) or the headless app load.
- **9 confirmed issues found by 4 adversarial-review passes**; **8 fully fixed** (2
  crash-capable races, 1 UAF crash, 1 that silently defeated the last-look feature, 1
  growing leak, 1 **security** path-traversal), **1 mitigated** in the embedded editor +
  flagged for the Show Manager tab (E1).
- All fixes covered by **new/updated unit tests**, including a **3-thread concurrency
  stress test** (60k+ ops) for the last-look holder that passed 6× with no flakiness.
- Remaining items are **"evaluate," not "core-failure"** — see §4.

---

## 1. What was exercised

| Area | Method | Result |
|---|---|---|
| Full engine suite (59 test binaries) | build + run all | **0 crashes**; 35 run-and-pass, 24 can't run (see note) |
| My touched suites | targeted run | track 10/10 · show 11/11 · showrunner 6/6 · lastlookeffect 9/9 · showfunction 5/5 · universe 26/26 · function 36/36 · genericdmxsource 4/4 · collection 16/16 · scenevalue 9/9 |
| Timer-thread engine code (last-look, live mute/solo, ShowRunner) | 3× parallel adversarial code review | 9 findings (§2) |
| Programming-tab undo + UI memory safety | adversarial review | verified safe (§2) |
| Integration surface (app/showmanager/vcshowcontrol/lookeditor) | adversarial review | round 2 (§2) |
| Concurrency of `LastLookEffect` | new 3-thread stress test, 6 runs | pass, no deadlock/crash |
| App startup + workspace load/teardown | headless (offscreen) run on a COPY | no crash markers |
| Compiler warnings on all changed files | -Wall build scan | none new (2 pre-existing benign) |

**Note on the 24 "failing" suites:** every one fails at `initTestCase()` on
`fixtureDefCache()->loadMap(dir) == false` (or an empty rgbscripts/i18n dir). That's
the compile-time `INTERNAL_FIXTUREDIR` pointing at an install path absent in a dev
build — it hits **stock, unmodified** suites (chaser/scene/sequence/efx/doc/…)
identically and is an environment limitation, **not a regression**. The count and set
are byte-identical before and after my fixes. Suites that don't need fixture defs (and
exercise my code) all pass.

---

## 2. Bugs found and FIXED

Ranked by severity. All confirmed by adversarial review, then fixed + tested.

### F1 — Use-after-free: `ShowTimelineEditor` destructor touched a freed Show — **CRASH, fixed**
The embedded timeline is torn down via `deleteLater()`, which defers past the engine's
*synchronous* `delete func`. The destructor then did `m_show->isRunning()` on freed
memory. **Repro:** open a Show in the Programming tab, then delete it (Functions tab) or
open another workspace → crash next event-loop turn.
**Fix:** the editor now nulls `m_show` on `Doc::functionRemoved`/`Doc::clearing`
(emitted *before* the delete). `showtimelineeditor.cpp`.

### F2 — Data race → crash: track-intensity fader vs. the timer thread — **CRASH, fixed**
`ShowRunner::adjustIntensity` (UI thread, from the submaster fader) mutated
`m_intensityMap` and iterated `m_runningQueue` while the timer thread appended/removed
from both **every frame**. Concurrent QList/QMap mutation is UB → crash. **Repro:** drag
a track intensity fader during playback.
**Fix:** `adjustIntensity` now only records a request under `m_tcMutex`; the timer thread
applies it via `applyPendingIntensity()` in `write()`. `showrunner.*`.

### F3 — Data race defeating the feature: `LastLookEffect::m_registered` — **CORRECTNESS, fixed**
`m_entries` was mutex-guarded but the register/unregister *decision* was a check-then-act
on an unguarded bool, touched from both threads. Interleaving `addHold` (timer) with
`clear` (UI) could leave **entries non-empty but the DMX source unregistered** → the held
last look silently drops to black — the exact thing the feature prevents.
**Fix:** a dedicated `m_regMutex` serializes reconciliation and re-reads the live entry
count, so registration always matches entries. Validated by the 3-thread stress test.
`lastlookeffect.*`.

### F4 — Use-after-free: editing a running/previewing show's structure — **CRASH, fixed (mitigated)**
The timer thread now walks `Show::tracks()`/`Track::showFunctions()` **every frame**
(live mute/solo, hold-last). Deleting/moving a track or clip on the UI thread mid-run
freed objects it was dereferencing. (Pre-existing in stock at cue boundaries; my
per-frame walks widened the window.) **Repro:** Play a show in the embedded editor, then
delete a clip/track while it plays.
**Fix:** the embedded editor's structural-edit slots (drop, add/delete/move track, delete
clip) now `stop()+stopAndWait()` the runner before mutating. `showtimelineeditor.cpp`.
See §4 for the broader (Show-Manager-tab) exposure still to evaluate.

### F5 — Logic bug: same cue started twice — **SERIOUS, fixed**
After a suspend→resume re-seek, `enforceLiveMuteSolo()` and the Phase-1 start loop could
both start the same cue in one frame → duplicated in the running queue, double
`start()`/attribute-override, double-release.
**Fix:** `startChild()` now refuses to start a function already in the running queue
(covers all start paths). `showrunner.cpp`.

### F6 — Memory leak: `MultiTrackView` scene + items — **LEAK, fixed**
`resetView()` removed items from the scene without deleting them, and the view had no
destructor to free its unparented `QGraphicsScene`. Benign upstream (one persistent
view); the fork rebuilds a fresh view per Show-open and on **every** timeline edit, so it
grew steadily.
**Fix:** `resetView()` deletes removed items; added `~MultiTrackView` (deletes the scene)
and `~TrackItem` (frees its region rects). `multitrackview.cpp`, `trackitem.cpp`.

### F7 — Robustness: `Track::loadXML` intensity parse — **MINOR, fixed**
`Intensity="abc"` silently parsed to `0.0` → track loads dark. Now checks the parse `ok`
flag and keeps the default `1.0` on garbage. `track.cpp` (+ test).

### F8 — Latent null-deref: `TrackItem::paint` — **MINOR, fixed**
One `m_track->getSceneID()` lacked the NULL guard the rest of the function uses. Guarded.
`trackitem.cpp`.

### F9 — Path traversal / arbitrary file write on Effect **import** — **SECURITY, fixed**
`LookEditor::slotImportEffect` used the imported `.qxfx` JSON's `"script"` field
**unsanitized** to build the install path: a crafted file with `"script":
"../../../../tmp/pwned"` (or an absolute path) made `udir.filePath(base + ".js")` escape
the user scripts dir and write attacker-controlled content (`.js`-suffixed) anywhere —
including clobbering an existing file. The sibling write paths (New-script, savePreset)
already sanitized; only import missed it. Since Import's whole purpose is "open an effect
someone shared," this is a realistic vector. **Fix:** sanitize `base` with
`[^A-Za-z0-9_-] → -` before it's used as a filename. `lookeditor.cpp`.
(Found by the 2nd-round review, which otherwise confirmed the integration surface —
timecode-signal threading, auto-arm, VCShowControl bounds, drag, editor rescan,
VCButton round-trip — all SAFE.)

---

## 3. New tests added (lock the fixes)
- `LastLookEffect_Test`: `registrationCycle` (F3), `addHoldAccumulates`,
  **`concurrentStress`** (3 threads × 20k ops — F2/F3 under contention). 9/9.
- `Track_Test`: `newFlagsRoundTrip` (all new flags survive save/load),
  `garbageAttrsAreSafe` (F7 + bad Priority → Normal). 10/10.
- `Show_Test::markers`: extended to cover the marker→cue-list link — preserved across
  relabel/recolour and round-tripped through XML (the timecode↔manual seam). 11/11.
- `ShowRunner_Test::intensity`: updated for the now-deferred `adjustIntensity` (F2).

---

## 4. Follow-up items — NOW FIXED (2026-07-20)

### E1 — Edit-during-run UAF (core fix) — **FIXED**
Both halves done: (a) **core** — `Show::m_tracks` and `Track::m_functions` are now
guarded by recursive mutexes (mirrors the shipped `Chaser::m_stepListMutex` pattern), so
the timer thread's per-frame walks and the UI's edits can't corrupt the lists; lock order
is always `tracksMutex → functionsMutex` (verified, no AB-BA). Benefits stock too. (b) the
**stop-before-edit** guard is now also on the **Show Manager tab** structural slots
(drop / add / delete / move track + delete clip), guaranteeing no track/ShowFunction is
*freed* while the runner references it. Full-suite regression: 0 new failures, 0 crashes.

### E2 — `Show::m_runner` lifetime — **FIXED**
New `Show::m_runnerMutex` guards `m_runner` across threads: the UI accessors
(`setTrackIntensity`, `adjustAttribute`) and the timer thread's create/delete
(`preRun`/`postRun`) all lock it. `write()` shares the timer thread with pre/postRun so it
needs no lock. Order `runnerMutex → tracksMutex` (consistent).

### E3 — Freeze threshold unified — **FIXED**
The ShowRunner freeze and the watchdog chip now derive from **one** shared constant
`SHOW_TC_HOLD_MS` (180, in `timecodesource.h`), so they can never drift. (Still worth a
live-MTC confirm that 180ms doesn't false-trigger on your source — ~22 missed
quarter-frames at 30fps, should be safe.)

### E4 — MTC drive now has automated coverage — **PARTLY ADDRESSED**
Added `ShowRunner_Test::timecodeDriveAndFreeze` — drives the real `write()`-based chase →
freeze (after `SHOW_TC_HOLD_MS`) → thaw, plus the existing seek/latch test. This exercises
the load-bearing timecode logic without a rig. A **live Logic-MTC** run is still the final
sign-off (`testing_st.md` §C/§G) — that can only be done on-site.

### E5 — Pre-existing benign warnings
`fixturegroupsource.h:58` / `functionstreewidget.h:216` — `mimeData` overload missing
`override` (the Qt drag trick). Harmless; predates this work. Could add `override` for tidiness.

---

## 5. Confirmed SAFE (checked, no action)
- Programming-tab **undo** detached-Scene snapshots: no double-free (QObject-child of Doc
  frees once; PM's deletes remove from Doc's child list first), no dangling ids, capped at 25.
- `captureLastLook` preGM reads: bounds-checked; timer-thread only.
- `collectFunctionFixtures` recursion: cycle-safe (`visited` + depth guard).
- `Track` XML back-compat: new fields default correctly for old files; written only when
  non-default; garbage Priority → Normal.
- Marker cue-list: survives move/relabel; dangling id resolved null-safely everywhere.
- MasterTimer DMX-source register/unregister: correctly list-mutex-guarded; no deadlock
  with the new `m_regMutex` (verified by the concurrency test + lock-order analysis).
