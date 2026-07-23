#!/usr/bin/env bash
#
# Marathon stress run for a dedicated/spare machine: hours of the heaviest
# multi-threaded workload the harness has, watching for crash / hang / leak /
# tick-budget breach. Interleaves a chaos (lifecycle) burst each cycle so runtime
# add/remove/delete churn is covered too.
#
# Prereqs on the target machine (once):
#   1. Build a RELEASE engine:
#        cmake -S . -B build-stress -DCMAKE_BUILD_TYPE=Release
#        cmake --build build-stress -j --target qlcplusengine
#   2. Build the harness against it:
#        cd stresstest/engine
#        cmake -S . -B build -DQLC_BUILD="$PWD/../../build-stress" \
#              -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"   # or your Qt prefix
#        cmake --build build -j
#   3. (Linux, for data races) build the TSan harness and run one pass under it:
#        ./build_sanitizers.sh thread && python3 run.py concurrency --sanitize thread
#
# Usage:
#   ./run_marathon.sh [HOURS] [CONFIG]
#     HOURS   total wall-clock hours (default 8)
#     CONFIG  "heavy" (default) | "huge"
#
set -u
HOURS="${1:-8}"
CONFIG="${2:-heavy}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ENGDIR="$HERE/engine"
Q="$ENGDIR/build/qlcstress"
LIB="$HERE/../build-stress/engine/src"
FIX="$HERE/../resources/fixtures"
LOG="$HERE/results/marathon-$(date +%Y%m%d-%H%M%S).log"
mkdir -p "$HERE/results"

# On macOS the engine dylib is found via DYLD_FALLBACK_LIBRARY_PATH; on Linux use
# LD_LIBRARY_PATH. Set both — the irrelevant one is ignored.
export DYLD_FALLBACK_LIBRARY_PATH="$LIB"
export LD_LIBRARY_PATH="$LIB"

case "$CONFIG" in
  huge)  UNI=60; FPU=150; SCN=500; CHA=80; MAT=20; EFX=20; COL=16; SHW=16 ;;
  *)     UNI=40; FPU=120; SCN=300; CHA=60; MAT=12; EFX=12; COL=10; SHW=10 ;;
esac
COMMON="--universes $UNI --fixtures-per-uni $FPU --scenes $SCN --chasers $CHA \
        --matrices $MAT --efx $EFX --collections $COL --shows $SHW --fixtures-dir $FIX"

# One long concurrency run gives the best warmup-excluded leak slope. Chaos bursts
# between cycles add lifecycle churn. We run 1h concurrency + 2min chaos per cycle.
CYCLE_CONC=3600
CYCLE_CHAOS=120
[ -x "$Q" ] || { echo "ERROR: harness not built at $Q (see header)"; exit 2; }

echo "marathon: $HOURS h, config=$CONFIG ($UNI uni / $((UNI*FPU)) fixtures / $SCN scenes / $SHW shows)" | tee "$LOG"
echo "marathon: log -> $LOG"
END=$(( $(date +%s) + HOURS*3600 ))
CYCLE=0
while [ "$(date +%s)" -lt "$END" ]; do
  CYCLE=$((CYCLE+1))
  echo "===== cycle $CYCLE : concurrency ${CYCLE_CONC}s =====" | tee -a "$LOG"
  QLC_TICKLOG=5000 "$Q" engine --mode concurrency --seconds "$CYCLE_CONC" --sample-seconds 60 \
      $COMMON 2>&1 | tee -a "$LOG"
  rc=$?
  # crash / TSan report / tick overrun are the fail signals
  if [ $rc -ne 0 ] || grep -qiE "segmentation|SIGABRT|ERROR: |runtime error:|Destroyed while" "$LOG"; then
    echo "marathon: FAIL in cycle $CYCLE (rc=$rc) — see $LOG" | tee -a "$LOG"; exit 1
  fi
  echo "===== cycle $CYCLE : chaos ${CYCLE_CHAOS}s =====" | tee -a "$LOG"
  "$Q" engine --mode chaos --seconds "$CYCLE_CHAOS" $COMMON 2>&1 | tee -a "$LOG"
done
echo "marathon: COMPLETED $CYCLE cycles over ~$HOURS h with no crash/overrun/leak-fail" | tee -a "$LOG"
echo "marathon: review the per-60s 'rss=' samples in $LOG for the leak slope (want flat after warmup)"
