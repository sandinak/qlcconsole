#!/bin/bash
# MasterTimer punctuality soak.
#
# Drives every ArtNet universe and mines the instrumented "running late" lines
# for late_us / compute_us, so scheduling jitter can be told apart from engine
# load: if compute is a small fraction of the tick budget while late is large,
# the kernel is not waking the timer thread on time.
#
# The harness ASSERTS ITS OWN LOAD. Three separate times this measurement
# reported a confident "0 late events" on a box that had transmitted nothing at
# all -- once because Linux found no I/O plugins, once because a modal autosave
# dialog blocked startup, once because no function was running so no universe
# ever dumped. A punctuality number from an idle engine is worse than no number,
# because it reads as a pass. So: verify plugins loaded, verify universes
# patched, verify a function is running, and verify packets are actually on the
# wire -- and refuse to print a verdict if any of that fails.
set -u
REPO=${REPO:-$HOME/git/qlcconsole}
SECS=${1:-300}
TAG=${2:-soak}
IFACE_OVERRIDE=${IFACE:-}
WS=/tmp/${TAG}.qxw
OUT=/tmp/${TAG}.log
BIN="$REPO/build/main/qlcconsole"

die() { echo "SOAK_ABORT=$*"; exit 1; }

# Portable packet count: macOS has no coreutils `timeout`, and a preflight that
# silently returns 0 there would abort every run on the very platform the
# original measurement came from.
capture_pkts() { # <iface> <seconds>
    local f; f=$(mktemp)
    sudo -n tcpdump -i "$1" -nn -c 500 'udp port 6454' > "$f" 2>/dev/null &
    local td=$!
    /bin/sleep "$2"
    sudo -n kill -INT "$td" 2>/dev/null; wait "$td" 2>/dev/null
    grep -c . "$f" 2>/dev/null || echo 0
    rm -f "$f"
}
[ -x "$BIN" ] || die "no binary at $BIN"

# --- pick the interface + broadcast target ------------------------------
if command -v ip >/dev/null 2>&1; then
    IFACE=${IFACE_OVERRIDE:-$(ip route show default | awk '/default/{print $5; exit}')}
    IP=$(ip -4 -o addr show dev "$IFACE" | awk '{print $4}' | head -1); IP=${IP%%/*}
    BCAST=$(ip -4 -o addr show dev "$IFACE" | awk '{print $6}' | head -1)
else
    IFACE=${IFACE_OVERRIDE:-$(route -n get default 2>/dev/null | awk '/interface:/{print $2}')}
    IP=$(ipconfig getifaddr "$IFACE" 2>/dev/null)
    BCAST=""
fi
[ -n "${IP:-}" ] || die "no IPv4 address on iface '${IFACE:-?}'"
[ -n "${BCAST:-}" ] || BCAST="${IP%.*}.255"
echo "iface=$IFACE ip=$IP bcast=$BCAST"

# --- build the load workspace -------------------------------------------
# Autostart matters: with no function running, universe data never changes,
# so Universe::dumpOutput short-circuits and the ArtNet plugin is never
# reached -- transmitMode="Full" cannot save you, because the universe stops
# short of the plugin. Pick a real function id from the workspace itself
# rather than hardcoding one that may not exist.
FUNC=${FUNC:-$(grep -o '<Function ID="[0-9]*" Type="RGBMatrix"' \
    "$REPO/test-workspaces/surfacetesting.qxw" | head -1 |
    sed 's/.*ID="\([0-9]*\)".*/\1/')}
[ -n "${FUNC:-}" ] || die "could not find an RGBMatrix function to autostart"
echo "autostart function id=$FUNC"

python3 - "$REPO/test-workspaces/surfacetesting.qxw" "$WS" "$FUNC" "$IP" "$BCAST" <<'PY'
import re, sys
src, dst, func, ip, bcast = sys.argv[1:6]
s = open(src).read()
s = re.sub(r'(Plugin="ArtNet" UID=")[^"]*(")', r'\g<1>%s\g<2>' % ip, s)
s = re.sub(r'outputIP="[^"]*"', 'outputIP="%s"' % bcast, s)
s = re.sub(r'(<PluginParameters\b[^>]*?)\s*transmitMode="[^"]*"', r'\1', s)
s = s.replace('<PluginParameters ', '<PluginParameters transmitMode="Full" ')
s = re.sub(r'<Engine\b[^>]*>', '<Engine Autostart="%s">' % func, s, count=1)
open(dst, 'w').write(s)
print("universes repatched:", s.count('outputIP="%s"' % bcast))
PY

# The recovery prompt blocks startup forever on offscreen platforms; belt and
# braces even though the app now guards it itself.
rm -f "$HOME/.qlcconsole/untitled.qxw.autosave" 2>/dev/null
export QLC_NO_RECOVERY_PROMPT=1

QT_QPA_PLATFORM=offscreen "$BIN" -p -d 0 -o "$WS" > "$OUT" 2>&1 &
APP=$!

# --- preflight: refuse to measure an engine that is not doing the work ---
for _ in $(seq 1 45); do
    kill -0 "$APP" 2>/dev/null || break
    [ "$(grep -c 'Open output on address' "$OUT" 2>/dev/null)" -ge 10 ] && break
    sleep 2
done
OPENED=$(grep -c 'Open output on address' "$OUT" 2>/dev/null)
PATCHED=$(grep -c 'plugin: "ArtNet"' "$OUT" 2>/dev/null)
STARTED=$(grep -c 'Starting startup function' "$OUT" 2>/dev/null)
echo "PRE_OUTPUTS_OPENED=$OPENED"
echo "PRE_UNIVERSES_PATCHED=$PATCHED"
echo "PRE_STARTUP_FUNCTION=$STARTED"
[ "$OPENED" -gt 0 ] || { kill -TERM $APP 2>/dev/null; die "no ArtNet outputs opened (plugins missing? see TODO: Linux PLUGINDIR trap)"; }

PKTS=$(capture_pkts "$IFACE" 6)
echo "PRE_PACKETS_6S=$PKTS"
if [ "${PKTS:-0}" -eq 0 ]; then
    kill -TERM "$APP" 2>/dev/null
    die "engine is not transmitting -- a punctuality verdict here would be meaningless"
fi

# --- measure -------------------------------------------------------------
start=$(date +%s); cpusum=0; cpun=0; peakcpu=0; peakrss=0
while [ "$(date +%s)" -lt $(( start + SECS )) ]; do
    kill -0 "$APP" 2>/dev/null || break
    set -- $(ps -o %cpu=,rss= -p "$APP" 2>/dev/null)
    c=${1:-}; r=${2:-0}
    if [ -n "$c" ]; then
        ci=${c%.*}; cpusum=$(( cpusum + ci )); cpun=$(( cpun + 1 ))
        [ "$ci" -gt "$peakcpu" ] && peakcpu=$ci
        [ "$r" -gt "$peakrss" ] && peakrss=$r
    fi
    /bin/sleep 3
done
elapsed=$(( $(date +%s) - start ))
kill -TERM "$APP" 2>/dev/null; wait "$APP" 2>/dev/null

python3 - "$OUT" "$elapsed" "$cpun" "$cpusum" "$peakcpu" "$peakrss" "$PKTS" <<'PY'
import re, sys
log, elapsed, cpun, cpusum, peakcpu, peakrss, pkts = sys.argv[1:8]
elapsed, cpun, cpusum = int(elapsed), int(cpun), int(cpusum)
txt = open(log, errors='replace').read()
late = [(int(a), int(b), int(c)) for a, b, c in re.findall(
    r'late_us:\s*(-?\d+)\s+compute_us:\s*(\d+)\s+budget_us:\s*(\d+)', txt)]
n_raw = txt.count('running late')
print(f"SOAK_SECONDS={elapsed}")
print(f"SOAK_PACKETS_PER_6S_SAMPLE={pkts}")
print(f"SOAK_LATE_EVENTS={n_raw}")
print(f"SOAK_LATE_PER_MIN={n_raw/(elapsed/60) if elapsed else 0:.2f}")
print(f"SOAK_CPU_AVG_PCT={cpusum//cpun if cpun else 0}")
print(f"SOAK_CPU_PEAK_PCT={peakcpu}")
print(f"SOAK_PEAK_RSS_MB={int(peakrss)//1024}")
if late:
    L = sorted(x[0] for x in late); C = sorted(x[1] for x in late); b = late[0][2]
    q = lambda v, p: v[min(len(v)-1, int(len(v)*p))]
    print(f"SOAK_BUDGET_US={b}")
    print(f"SOAK_LATE_US_MIN={L[0]} MED={q(L,.5)} P95={q(L,.95)} MAX={L[-1]}")
    print(f"SOAK_COMPUTE_US_MIN={C[0]} MED={q(C,.5)} P95={q(C,.95)} MAX={C[-1]}")
    print(f"SOAK_COMPUTE_PCT_OF_BUDGET_MED={100*q(C,.5)/b:.1f}")
    print("SOAK_VERDICT=" + ("JITTER (compute well under budget)"
                             if q(C,.5) < 0.5*b else "LOAD (compute at/over budget)"))
elif n_raw:
    print("SOAK_VERDICT=LATE_BUT_UNINSTRUMENTED (binary predates the late_us diag?)")
else:
    print("SOAK_VERDICT=CLEAN (load verified, no late ticks)")
PY
