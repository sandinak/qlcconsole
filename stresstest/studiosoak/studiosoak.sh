#!/bin/sh
# studiosoak.sh BINARY SHOW MINUTES OUTDIR [SNAP_MIN]
# Runs studiosoak offscreen at nice 20 under MallocStackLogging, and every
# SNAP_MIN minutes (default 10) saves heap -s, vmmap --summary and
# leaks --nocontext of it into OUTDIR. Never touches the real screen.
# SOAK_LIBDIR: the engine/ui dylibs to run with (a copy, so a rebuild of
# the tree under it can't swap them mid-run). Passed through env(1) after
# nice: SIP drops DYLD_* on the way through /bin/sh and /usr/bin/nice.
set -u
BIN=$1; SHOW=$2; MIN=$3; OUT=$4; SNAP=${5:-10}
mkdir -p "$OUT"
QT_QPA_PLATFORM=offscreen QLC_NO_RECOVERY_PROMPT=1 MallocStackLogging=1 \
    nice -n 20 env DYLD_LIBRARY_PATH="${SOAK_LIBDIR:-}" "$BIN" "$((MIN + 3))" "$SHOW" 2> "$OUT/soak.log" &
PID=$!
echo "pid $PID" > "$OUT/pid"
snap() {
    T=$1
    heap -s "$PID" > "$OUT/heap-$T.txt" 2>&1
    vmmap --summary "$PID" > "$OUT/vmmap-$T.txt" 2>&1
    leaks --nocontext "$PID" > "$OUT/leaks-$T.txt" 2>&1
}
sleep 120; snap 002
m=0
while kill -0 "$PID" 2>/dev/null; do
    sleep 60; m=$((m + 1))
    [ $((m % SNAP)) -eq 0 ] && kill -0 "$PID" 2>/dev/null && snap "$(printf %03d $((m + 2)))"
done
wait "$PID"
echo "exit $?" >> "$OUT/pid"
