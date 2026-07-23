# QLC+ stress test — findings

Host: Apple Silicon (arm64, 14 cores), QLC+ 4.14.4 GIT built against Homebrew
Qt6, Release. Engine tick rate **50 Hz → 20 ms per-tick budget**. All engine
numbers are `qlcstress` flatout (deterministic per-tick compute: function
writes + `Universe::processFaders`, dummy/no output patch). Show seed = 1.

## Headline

| Scenario | per-tick (p50) | vs 20 ms budget |
|----------|---------------:|-----------------|
| 80 universes, **476 functions all running** (300 scenes, 80 chasers, 40 matrices, 40 EFX, 16 collections; 8000 fixtures / 24000 ch) | **~225 ms** | **11× over — 100% overruns** |
| 80 universes, **light load** (28 functions) | **8.4 ms** | within budget |
| 120 universes (61k ch), light load | 12.6 ms | within budget |

**The universe/channel count is not the bottleneck. Concurrent running
functions — dominated by Scenes — are.**

## What scales cheaply: universes / channels

Light fixed load (20 scenes, 4 chasers, 2 matrices, 2 EFX), scaling universes:

| Universes | channels | p50 ms |
|----------:|---------:|-------:|
| 10 | 6 120 | 1.07 |
| 20 | 12 240 | 2.07 |
| 40 | 24 480 | 4.13 |
| 60 | 36 720 | 6.10 |
| 80 | 48 960 | 8.38 |
| 120 | 61 440 | 12.59 |

Almost perfectly linear at **~0.105 ms per universe** (per-universe fader
processing + `zeroIntensityChannels` + dump over 512 ch). Extrapolated ceiling
under light load ≈ **~190 universes** before the budget is blown. The target of
**70–80 universes / ~15k+ channels is comfortably within reach** — *as long as
the simultaneously-running function load is modest.*

## What breaks it: concurrent running functions

### Scenes (the dominant cost)
A running Scene re-applies **all** its channel values into faders **every
tick**. Scaling concurrent large scenes (16 universes, ~720 ch/scene):

| Running scenes | p50 ms | |
|---------------:|-------:|--|
| 50 | 6.2 | ok |
| 100 | 14.3 | ok |
| 200 | 33.6 | **over (100% overruns)** |
| 400 | 72.0 | **over** |

Budget blown between **100 and 200 concurrent large scenes** (~0.07 ms/scene,
i.e. cost ∝ scenes × channels-per-scene). This is the single biggest lever.

### RGBMatrix
Scaling concurrent RGBMatrix functions with real script algorithms loaded
(16 universes, groups up to 16×16 heads):

| Matrices | p50 ms |
|---------:|-------:|
| 5 | 0.84 |
| 20 | 1.80 |
| 40 | 3.27 |
| 80 | 6.41 |

≈ **0.08 ms per matrix**; ~250 concurrent before the budget is blown. RGBMatrix
caches its rendered map between steps, so per-tick cost is the map→fader apply.

> ⚠️ Harness gotcha turned finding: if the **RGB script cache is not loaded**
> (`RGBScriptsCache::load`), `RGBAlgorithm::algorithm()` returns null and every
> RGBMatrix silently becomes a **no-op** (≈0 cost). Real shows always load it;
> the harness now loads it explicitly (`--scripts-dir`).

## Realtime behaviour (real 50 Hz timer + universe threads)

40 universes, 104 functions, 12 s realtime run — observed inter-tick gap
(expected 20 ms):

```
mean 21.1 ms, p50 20.1 ms, p95 36.6 ms, p99 55.2 ms, max 312 ms
50% of ticks > 20 ms; 41/583 ticks "late" (>30 ms)
```

It mostly holds 50 Hz at the median but with heavy jitter and occasional
multi-hundred-ms stalls — what a real operator would see as lag/stutter. RSS
grew 21 MB while functions spun up, then held.

## Full-application black-box run

Real `qlcplus` headless (offscreen) + 16-universe / 160-function workspace,
driven over the WebSocket API for 40 s (start-all, churn, GM sweep,
blackout-flap, channel-spray; 97 rounds):

* **No crash, no hang** — stayed responsive (`getFunctionsNumber` answered every round).
* CPU **~1300%** (≈13 cores): the app runs **one thread per universe** + master
  timer + universe + web threads. Thread count scales with universe count — a
  scalability consideration at 80 universes.
* RSS **107 → peak 153 → 140 MB**, +33 MB over the run, no runaway growth → **no
  obvious leak** under this workload.

## Sanitizer findings (macOS 26.5, clang 21)

* **UBSan — real bug, fixed:** `Universe::applyGM` (universe.cpp:514) cast a DMX
  value (0–255) to signed `char` → UB for values >127. Now `uchar`.
* **TSan — one real data race, fixed:** `RGBScript::apiVersion()` read a plain
  `int m_apiVersion` on the run thread (per tick) while `evaluate()` wrote it on
  the JS thread → now `std::atomic<int>`. Init handshake also hardened
  (QMutex/QWaitCondition + mutex-guarded `initEngine`).
* **TSan vs Qt — mostly false positives:** ~100 of the ~105 realtime TSan
  reports are correctly-synchronised code that TSan flags because it does **not
  model Qt6's QMutex / QWaitCondition / QSemaphore / BlockingQueuedConnection**.
  Proof: a reported "race" where *both* accesses hold the same mutex (M0).
  Meaningful TSan against the engine needs a TSan-instrumented Qt build or a
  suppressions file; otherwise rely on ASan/UBSan (which found a real bug) plus
  targeted reasoning. The RGBScript engine design (single shared JS thread, all
  calls marshaled via BlockingQueuedConnection) is sound.

## Production-viability findings

* **Determinism — confirmed.** Same workspace + seed produces a byte-identical
  DMX-frame fingerprint run-to-run (`golden`). Gives a behavioural-regression
  gate and a cross-fork output-equivalence check.
* **Loader robustness — solid.** 200 mutated / truncated / corrupt `.qxw` fed to
  the loader under ASan: **0 crashes** (≈80% tolerated, ≈20% cleanly rejected).
  Save→reload→save is byte-stable (`roundtrip`). The project loader is resilient.
* **Runtime object-deletion was unsafe against the running engine — found by
  `chaos`, now FIXED (commit on `fix/runtime-deletion-uaf`).** Deleting a
  Function or Fixture while the MasterTimer ran was a heap-use-after-free:
  ```
  freed by  Doc::deleteFunction (delete func)      [edit thread]
  read by   MasterTimer::timerTickFunctions        [timer thread]
  ```
  It was a cascade: the function/timer race, plus deleted objects leaving
  dangling refs in Collections (NULL deref via compiled-out `Q_ASSERT`) and a
  Scene's value map racing fixture removal. Fix: a recursive
  `m_functionListMutex` held across the whole tick + `MasterTimer::removeFunction()`
  so deletion (on the Doc thread, preserving QObject affinity) can't race the
  iteration; `Scene` locks `m_values` in `slotFixtureRemoved`/`writeDMX`;
  `Collection` cleans `m_runningChildren` and null-checks its child loops.
  Verified: `chaos --chaos-aggressive` now runs 200k–500k ops across seeds under
  ASan with zero errors and clean shutdown. Reproduce the original crash by
  reverting that commit and running `--chaos-aggressive`.

## Bugs / obstacles surfaced

1. **Traditional UI can't be loaded headless via `-o`.** Under
   `QT_QPA_PLATFORM=offscreen`, `app.loadXML()` pops a **modal dialog** that
   blocks `main()` *before* the web server starts (bare `-w` serves HTTP 200;
   `-o file -w` never binds the port). Workaround in the driver: launch bare,
   then load via the `/loadProject` HTTP API + `QLC+CMD|opMode`. Worth fixing in
   the app (suppress/skip dialogs when no window manager / nogui).
2. **`MasterTimer::stopAllFunctions()` busy-waits forever** if the platform
   timer thread isn't running (spins on `runningFunctions()>0` with nothing to
   advance it). Only bites out-of-process drivers, but it's a latent deadlock.
3. Pre-existing **build breakage on this branch**: the `artnet` plugin and parts
   of `ui/` referenced widgets missing from their `.ui` forms; stale in-source
   generated `ui_*.h`/`moc_*` from an old qmake build shadowed the CMake ones.
   Cleaned the stale artifacts; built `qlcplus` (the `artnet` plugin still fails
   and is not needed for stress testing).
4. **ASan startup deadlock on macOS 26.2 (beta).** Apple-clang ASan binaries
   intermittently wedge in `libSystem_initializer` during dyld's pre-`main()`
   init (1.4 MB RSS, state `UE`, unreapable until reboot). Toolchain/OS race,
   not QLC+ code. Run sanitizers on Linux/CI; locally it's best-effort
   (`MallocNanoZone=0` + timeout/retry mitigations are wired into `run.py`).

## Takeaways for capacity planning

* Channels/universes are cheap — **80 universes / 24k channels is fine**.
* Budget is consumed by **how many functions run at once**, especially Scenes.
  Rough single-core ceilings at 50 Hz on this host: ~130 large scenes, ~250
  matrices, ~190 light universes — but these **combine**, so a dense show hits
  the wall well before any one of them.
* Engine function processing is effectively single-threaded (the MasterTimer
  thread does all `Function::write`s); only `processFaders` is parallel per
  universe. So **function compute is the scaling limit**, not I/O or channels.

---

## Fork thread-safety + real-rig runs (2026-07, programmer-mode)

Added a `concurrency` mode (real MasterTimer thread vs an operator loop hammering
the fork's hardened cross-thread accessors — Scene value/fader readers+setters,
Show MTC/transport, blackout/inhibit, Universe faders — plus external timecode
into shows) and a `--load`/`--start` path to run a **real .qxw** through any mode.
All numbers Release engine, arm64, this host.

### Thread-safety batch — validated clean
* `concurrency` 5 min, synthetic (30 uni / 276 running funcs): **1,276,500 ops,
  0 crashes, 0 asserts, 0 tick overruns, FD flat.** The locks added on the
  Scene/Show/Chaser/VCSlider/IOMap/ProgrammerController paths hold under heavy
  operator-vs-timer racing.
* A monotonic RSS climb seen first was a **harness artifact**, isolated with the
  new `QLC_CONC_MASK` op-mask to `Scene::setValue`: the operator was editing
  fixtures NOT in the scene, and `Scene::setValue` adds an unknown fixture to the
  scene by design. `GenericFader` dedups channels by hash (bounded). With the
  operator restricted to in-scene fixtures, RSS is **flat** (full op-mix settles
  ~77 MB). No engine leak.

### Real show — CampEvo-2.0.qxw (pre-fork content)
Loaded: **65 universes, 184 fixtures¹, 977 scenes, 162 collections, 122 RGB
matrices, 2 chasers.** Run as operated (start the 2 chasers → cascade through
collections). Cascaded runtime load = **~40 functions live at once**.

| run | result | vs 20 ms budget |
|-----|--------|-----------------|
| flatout 2000 ticks | p50 0.09, p95 5.70, **p99 9.89**, max 15.0 ms, 0% overruns | within (½ budget) |
| concurrency 40 s (+ operator race) | **172 M ops, 0 crash**, runningFuncs=40 steady | thread-safe |
| memory | RSS flat ~52 MB, FD flat, leak −23 MB/hr | no leak |

¹ 78 of 262 fixtures need defs absent from this repo's `resources/fixtures`;
universe/function counts loaded fully, so numbers are representative (slightly
optimistic on raw channel count, which is not the bottleneck).

### Effect-script overlay (`--inject-effects N`, realtime 25 s on the real show)
Effect scripts run on `EffectScriptRunner`'s OWN timer/thread, so they do NOT
consume the MasterTimer tick budget:

| injected effects | DMX gap p50 / p99 | late ticks (>1.5×) | RSS peak | user CPU |
|-----------------:|-------------------|--------------------|---------:|--------:|
| 0   | 20.6 / 29.4 ms | 0.4% | 79 MB | 12.3 s |
| 100 | 20.8 / 30.0 ms | 1.0% | 85 MB | 14.2 s |
| 200 | 20.8 / 31.7 ms | 2.2% | 86 MB | 16.0 s |

Cost is main-thread CPU (~+15% at 200 concurrent effects), ~70 KB RSS/instance
(bounded, no leak), and a small rise in DMX-tick jitter. 200 concurrent effects
far exceeds a realistic rig (5–20 looks), so effect scripts fit comfortably.

Note: the `realtime` inter-tick gap carries a fixed ~0.7 ms offscreen-QGuiApp +
`processEvents` overhead (p50 20.6 even at zero load); the engine COMPUTE per
tick is the `flatout` figure (p99 9.9 ms). Judge effect cost by the DELTA
(late-tick %, RSS, CPU), not the absolute gap.
