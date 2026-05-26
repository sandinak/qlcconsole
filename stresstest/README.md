# QLC+ stress / torture test suite

A build-validation system: push QLC+ hard at runtime to **(1) confirm a build
operates correctly under stress** (no crashes, no memory/UB/race bugs, no perf
regressions) and **(2) measure its upper capability** (how big a rig it can
drive), at the scale of a large setup (**70–80 universes, ~15k+ active channels,
RGB-heavy**).

## One command

```sh
cd stresstest
python3 run.py validate      # gate a build: fixed scenarios vs baseline -> PASS/FAIL (exit 0/1)
python3 run.py capability    # measure the ceiling (largest sustainable config)
python3 run.py baseline      # save current results as the new known-good
python3 run.py validate --sanitize address   # run under ASan+UBSan, fail on any report
python3 run.py validate --sanitize thread    # run under TSan (data races)

# production-viability tests (long-runtime / behavioural / robustness)
python3 run.py soak --seconds 1800   # endurance: leak rate (MB/hr) + FD growth -> PASS/FAIL
python3 run.py golden --golden-capture   # capture a DMX-output fingerprint (deterministic)
python3 run.py golden                 # verify output unchanged vs the captured golden -> PASS/FAIL
python3 run.py chaos --seconds 60    # runtime lifecycle churn must survive -> PASS/FAIL
python3 run.py robustness            # .qxw round-trip fidelity + loader fuzzing -> PASS/FAIL
```

`run.py` drives the fixed scenario matrix in `scenarios.json`, parses machine-
readable `RESULT_JSON` from the harness, compares tick cost to `baseline.json`
within a tolerance, and exits non-zero on a regression, a budget breach, a
crash, or a sanitizer report. Drop it into CI / a pre-merge check.

## Layers

| Layer | What it stresses | Where |
|-------|------------------|-------|
| **Orchestrator** (`run.py` + `scenarios.json`) | Build gate + capability measurement + regression baseline. | wraps the harness |
| **Engine torture harness** (`engine/qlcstress`) | The engine in isolation: builds a big `Doc` and drives the `MasterTimer` directly. Deterministic, isolates per-tick CPU cost. | links `libqlcplusengine` |
| **Full-app black-box driver** (`driver/stress_driver.py`) | The *real* `qlcplus` binary headless: XML load, plugins, web access, VC, the whole event loop. Hammered over the WebSocket API. | drives `main/qlcplus` |
| **Sanitizer builds** (`build_sanitizers.sh`) | Same scenarios under ASan+UBSan / TSan to catch use-after-free, overflow, UB and data races in new engine code. | ASan/TSan engine+harness |

The engine the whole suite targets ticks at **50 Hz → a 20 ms per-tick budget**.
If the work for one tick exceeds 20 ms, the engine cannot keep up: that is the
fundamental "breaking point" both layers look for.

## Production-viability tests

Beyond acute failures (crash/budget/UB), these target the chronic and
behavioural issues that decide production readiness. All are fork-portable
(they only use `libqlcplusengine`, the `.qxw` format, and the web API).

| `run.py` mode | What it proves | How it fails |
|---------------|----------------|--------------|
| `soak` | No slow leaks / FD growth / jitter drift over hours | leak-rate (warmup-excluded RSS slope) > threshold, or FD growth > threshold, or crash |
| `golden` | DMX output is **deterministic** and unchanged across builds/forks | output-frame hash diverges from the captured golden (reports first divergent tick) |
| `chaos` | Runtime object lifecycle is safe (add/start/stop/mode churn) | the process crashes/hangs during churn |
| `robustness` | `.qxw` save/load is lossless and the loader survives bad files | round-trip drift, or a mutated/truncated workspace crashes the loader (`fuzz_projects.py`, best run against the ASan build) |

`golden` requires a deterministic workspace (no `Math.random` RGB scripts); the
default golden scenario uses scenes/chasers/EFX/sequences only. `chaos` has an
`--chaos-aggressive` engine flag (via `qlcstress`) that deletes functions/
fixtures *while running* — this exposed a runtime-deletion use-after-free that
is now fixed on `fix/runtime-deletion-uaf` (see FINDINGS); keep it as a
regression check under ASan.

## Workflow for ongoing development

1. Build the engine + harness (below). Run `python3 run.py baseline` once on a
   known-good commit.
2. While extending the system, run `python3 run.py validate` — it fails if your
   change regressed tick cost > tolerance or broke the budget.
3. Periodically (or in CI) run `./build_sanitizers.sh all` then
   `python3 run.py validate --sanitize address` and `--sanitize thread` to catch
   memory/UB/race bugs the perf numbers can't see.
4. Run `python3 run.py capability` to see how the ceiling moves as you optimize.
5. Re-baseline (`run.py baseline`) after an intentional, reviewed perf change.

---

## Prerequisites (already satisfied on this host)

* arm64 build of QLC+ in `build-arm64/` (engine lib + `main/qlcplus`), built
  against Homebrew Qt6. See repo root for the build; configure with:
  ```sh
  cmake -S . -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_OSX_ARCHITECTURES=arm64
  ninja -C build-arm64 qlcplusengine qlcplus
  ```

---

## 1. Engine torture harness — `engine/qlcstress`

Build:
```sh
cd stresstest/engine
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_OSX_ARCHITECTURES=arm64
ninja -C build
```

A single parametric *show builder* (`showbuilder.cpp`) creates N universes of
Generic-RGB fixtures (3 ch each, so RGBMatrix/EFX have real RGB heads) plus a
mix of Scenes, Chasers, RGBMatrices, EFX and Collections. Two subcommands use
it:

### `engine` — drive the engine
```sh
# deterministic per-tick compute benchmark (default mode)
./build/qlcstress engine --universes 80 --fixtures-per-uni 100 \
    --scenes 300 --chasers 80 --matrices 40 --efx 40 --collections 16 \
    --ticks 2000 --fixtures-dir ../../resources/fixtures

# realtime: run the real 50Hz timer + universe threads + event loop, measure
# tick cadence/jitter and RSS growth
./build/qlcstress engine --mode realtime --seconds 30 --universes 40 ...

# ramp: scale the universe count until the tick budget is blown
./build/qlcstress engine --mode ramp --ticks 800 ...
```

Modes:
* **flatout** (default) — synchronously calls `timerTickFunctions()` +
  `Universe::processFaders()` as fast as possible, recording the wall time of
  each tick. Reports min/mean/p50/p95/p99/max and overrun count vs the 20 ms
  budget. Isolates pure engine CPU cost; deterministic.
* **realtime** — starts the real platform `MasterTimer` thread, universe
  threads and a Qt event loop; measures observed inter-tick gap (jitter) and
  resident memory.
* **ramp** — repeats `flatout` while scaling universes to find the largest
  configuration that still fits in budget.

Key options: `--universes --fixtures-per-uni --scenes --chasers --matrices
--efx --collections --ticks --seconds --freq --seed --fixtures-dir`.
`--freq <hz>` overrides the MasterTimer frequency (push beyond 50 Hz).

### `emit` — generate a workspace
```sh
./build/qlcstress emit /tmp/stress80.qxw --universes 80 --fixtures-per-uni 100 \
    --scenes 300 --chasers 80 --matrices 40 --efx 40 --collections 16 \
    --fixtures-dir ../../resources/fixtures
```
Builds the same show and saves it as a **canonical `.qxw`** via the engine's own
`Doc::saveXML`, so the full app is guaranteed to load it. This is the input for
the black-box driver.

---

## 2. Full-app black-box driver — `driver/stress_driver.py`

No third-party deps (stdlib WebSocket client in `wsclient.py`, `ps` for
sampling). Launches the real binary headless (`QT_QPA_PLATFORM=offscreen`,
`-p` operate, `-w` web access), loads a workspace, then cycles torture
patterns while watching for **crash / hang / CPU saturation / RSS growth**.

```sh
cd stresstest/driver
python3 stress_driver.py \
    --qlcplus ../../build-arm64/main/qlcplus \
    --workspace /tmp/stress80.qxw \
    --libdir ../../build-arm64/engine/src \
    --port 9999 --duration 180
```

Torture patterns (cycled): `start-all`, `churn` (rapid random start/stop),
`gm-sweep`, `blackout-flap`, `channel-spray`. After each round it issues a
liveness probe (`getFunctionsNumber`); no response for >15 s ⇒ **HANG**, process
exit ⇒ **CRASH**, RSS growth beyond `--leak-threshold` ⇒ **POSSIBLE LEAK**.

### Engine tick instrumentation (works in the full app too)
The engine has env-gated tick timing: set **`QLC_TICKLOG=1`** (optionally a
report interval) before launching `qlcplus` and the `MasterTimer` logs whenever
a tick's compute exceeds the 20 ms budget, plus periodic mean/max/overrun
stats. Near-zero cost when unset.
```sh
QLC_TICKLOG=100 DYLD_FALLBACK_LIBRARY_PATH=build-arm64/engine/src \
    build-arm64/main/qlcplus -o /tmp/stress80.qxw -p   # watch stderr
```

---

## Sanitizers: known host limitation (macOS 26.2)

On this host (macOS 26.2 beta, arm64) the Apple-clang **ASan runtime
intermittently deadlocks in `libSystem_initializer` during dyld's pre-`main()`
init phase** — the process wedges at ~1.4 MB RSS and never reaches our code. It
is a toolchain/OS race, not a bug in QLC+ or the harness (proven: the same
binary runs fine on a retry). Mitigations baked into `run.py`:

* sets `MallocNanoZone=0` (avoids ASan's self-re-exec, which is where it wedges);
* launches sanitizer scenarios with a timeout and **auto-retries** on a startup
  wedge.

For rock-solid sanitizer runs use **Linux CI** (ASan/TSan are reliable there;
the build wiring in `build_sanitizers.sh` is generic cmake flags) or Homebrew
LLVM clang instead of Apple clang. The perf/crash `validate` and `capability`
modes are unaffected and reliable on this host.

## Findings

_See `FINDINGS.md` for measured breaking points on this host._
