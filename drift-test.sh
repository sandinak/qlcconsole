#!/bin/bash
# Free-run timing drift test for the MasterTimer.
#
# Measures whether a function's programmed duration matches wall-clock reality.
# Function::incrementElapsed() advances by a fixed MasterTimer::tick() per tick
# rather than by wall clock, and the timer loop re-anchors its deadline after a
# missed tick, so skipped ticks are gone for good: a chase runs permanently slow
# by the lost time.
#
# Deliberately does NOT use timecode. A timecode-following show phase-locks to
# the incoming position (showrunner.cpp: snap on a big jump, ease 25% otherwise),
# so it corrects exactly the drift we are trying to observe -- a timecode test
# would measure the correction loop and report near-zero drift regardless.
# A free-running chaser has nothing correcting it, which is the point.
#
#   ./drift-test.sh [steps] [step_ms]     default 600 x 1000ms = 10 minutes
set -u
REPO=${REPO:-$(cd "$(dirname "$0")" && pwd)}

# --rate: run two different durations and report the slope between them. This
# is the only honest way to separate real drift from the fixed start-up offset,
# which a single run silently folds into its percentage.
if [ "${1:-}" = "--rate" ]; then
    short=${2:-30}; long=${3:-150}
    a=$("$0" "$short" 1000 | awk -F= '/^DRIFT_MS=/{print $2}')
    b=$("$0" "$long"  1000 | awk -F= '/^DRIFT_MS=/{print $2}')
    python3 - "$a" "$b" "$short" "$long" <<'RATEPY'
import sys
a, b, s, l = float(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
span = (l - s) * 1000.0
rate = (b - a) / span
print(f"DRIFT_SHORT_MS={a:+.0f}  (over {s} s)")
print(f"DRIFT_LONG_MS={b:+.0f}  (over {l} s)")
print(f"DRIFT_FIXED_OFFSET_MS={a - rate*s*1000:+.0f}")
print(f"DRIFT_RATE_PCT={100*rate:+.4f}")
print(f"DRIFT_RATE_S_PER_HOUR={rate*3600:+.2f}")
print("DRIFT_VERDICT=" + ("CLEAN (no accumulating drift)" if abs(rate) < 0.0002
                          else "DRIFTING"))
RATEPY
    exit 0
fi
STEPS=${1:-600}
STEP_MS=${2:-1000}
WS=/tmp/drift-test.qxw
LOG=/tmp/drift-test.log
BIN="$REPO/build/main/qlcconsole"
CHASER_ID=9000

[ -x "$BIN" ] || { echo "DRIFT_ABORT=no binary at $BIN"; exit 1; }

python3 - "$WS" "$STEPS" "$STEP_MS" "$CHASER_ID" <<'PY'
import sys
ws, steps, step_ms, cid = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
# Two empty scenes are enough: we are timing the chaser's step clock, not output.
scenes = "".join(
    f'  <Function ID="{cid+1+i}" Type="Scene" Name="drift-s{i}">\n'
    f'   <Speed FadeIn="0" FadeOut="0" Duration="0"/>\n'
    f'  </Function>\n' for i in range(2))
# SingleShot so it ends by itself; PerStep so each Hold is honoured verbatim.
step_xml = "".join(
    f'   <Step Number="{n}" FadeIn="0" Hold="{step_ms}" FadeOut="0">{cid+1+(n%2)}</Step>\n'
    for n in range(steps))
open(ws, "w").write(f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE Workspace>
<Workspace xmlns="http://www.qlcplus.org/Workspace" CurrentWindow="FunctionManager">
 <Creator>
  <Name>Q Light Controller Plus</Name>
  <Version>4.14.4</Version>
  <Author>drift-test</Author>
 </Creator>
 <Engine Autostart="{cid}">
{scenes}  <Function ID="{cid}" Type="Chaser" Name="drift-chaser">
   <Speed FadeIn="0" FadeOut="0" Duration="0"/>
   <Direction>Forward</Direction>
   <RunOrder>SingleShot</RunOrder>
   <SpeedModes FadeIn="PerStep" FadeOut="PerStep" Duration="PerStep"/>
{step_xml}  </Function>
 </Engine>
</Workspace>
""")
print(f"workspace: {steps} steps x {step_ms} ms = {steps*step_ms/1000.0:.1f} s programmed")
PY

rm -f "$HOME/.qlcconsole/untitled.qxw.autosave" 2>/dev/null
rm -f "$HOME/Library/Application Support/qlcconsole/untitled.qxw.autosave" 2>/dev/null
export QLC_NO_RECOVERY_PROMPT=1

# Timestamps are the measurement instrument: we time from the engine's own
# "Function start()" to its "Function postRun", so app startup and workspace
# loading are excluded from the figure.
#
# QT_MESSAGE_PATTERN does NOT work here -- main.cpp installs qlcMessageHandler,
# which formats messages itself and ignores the pattern. It does
# fprintf(stderr) + fflush(stderr) per message, though, so stamping each line
# as we read it is accurate to the pipe latency rather than to a buffer flush.
QT_QPA_PLATFORM=offscreen "$BIN" -p -d 0 -o "$WS" 2>&1 \
  | python3 -u -c 'import sys,datetime
for line in sys.stdin:
    sys.stdout.write(datetime.datetime.now().isoformat(timespec="milliseconds")+" "+line)' \
  > "$LOG" &
sleep 3
APP=$(pgrep -n -f "qlcconsole -p -d 0 -o $WS")
[ -n "${APP:-}" ] || { echo "DRIFT_ABORT=app did not start"; exit 1; }

PROGRAMMED_MS=$(( STEPS * STEP_MS ))
DEADLINE=$(( $(date +%s) + PROGRAMMED_MS/1000 + 180 ))
while [ "$(date +%s)" -lt "$DEADLINE" ]; do
    kill -0 "$APP" 2>/dev/null || break
    grep -q "Function postRun.*drift-chaser" "$LOG" 2>/dev/null && break
    sleep 2
done
kill -TERM "$APP" 2>/dev/null; wait "$APP" 2>/dev/null

python3 - "$LOG" "$PROGRAMMED_MS" <<'PY'
import re, sys
from datetime import datetime
log, programmed = sys.argv[1], int(sys.argv[2])
txt = open(log, errors="replace").read()
def stamp(pat):
    m = re.search(r'^(\S+)\s.*' + pat, txt, re.M)
    if not m: return None
    return datetime.strptime(m.group(1), "%Y-%m-%dT%H:%M:%S.%f")
t0 = stamp(r'Function start\(\).*drift-chaser')
t1 = stamp(r'Function postRun.*drift-chaser')
print(f"DRIFT_PROGRAMMED_MS={programmed}")
if not t0 or not t1:
    print("DRIFT_VERDICT=INCOMPLETE (chaser did not start and finish; see " + log + ")")
    raise SystemExit(1)
actual = (t1 - t0).total_seconds() * 1000.0
drift = actual - programmed
print(f"DRIFT_ACTUAL_MS={actual:.0f}")
print(f"DRIFT_MS={drift:+.0f}")
print(f"DRIFT_PCT={100.0*drift/programmed:+.3f}")
print(f"DRIFT_PER_HOUR_S={drift/programmed*3600:+.2f}")
late = txt.count("running late")
print(f"DRIFT_LATE_TICKS_LOGGED={late}")
# One lost tick = one tick's worth of drift, so the two should agree.
# NOTE: a single run cannot tell a constant offset from an accumulating rate.
# There is a fixed ~23 ms cost between the "Function start()" log line and the
# first tick actually advancing the chaser, which on a 20 s run looks like
# 0.13%/hour of drift and on a 2 min run looks like 0.02%. Only the SLOPE
# across two different durations is meaningful. Use --rate for that.
print("DRIFT_NOTE=single run: includes a fixed start offset; use --rate for the true slope")
PY
