#!/bin/sh
# The nightly performance run: origin/main, built Release, through the
# performance guard (tools/perf/guard.py), with a history kept on this host.
#
#   tools/perf/nightly.sh            # what cron / launchd runs
#
# Everything lives under $PERF_NIGHTLY (default ~/perf-nightly):
#   qlcconsole/      a clone of the repository, moved to origin/main each night
#   build-perf/      its Release build (incremental: a few minutes a night)
#   runs/<date>/     each night's results (the last 30 kept)
#   trend.csv        the history: one row per metric per night (format in
#                    docs/performance.md, "Keeping it fast"); copied into
#                    docs/perf-data/trend.csv at a release, not nightly
#   latest.txt       the last night's summary: its first line is
#                    "perf=N host=CLASS rev=SHA date=..." (N: 0 within
#                    tolerance, 1 regression, 2 build/run error, 3 skipped
#                    because the machine was busy)
#
# Install (nothing installs it for you; see docs/performance.md):
#   Linux, crontab -e:   15 3 * * * $HOME/perf-nightly/qlcconsole/tools/perf/nightly.sh >/dev/null 2>&1
#   macOS: a LaunchAgent with StartCalendarInterval 03:15 running this script.
# The first run needs the clone:
#   git clone https://github.com/sandinak/qlcconsole.git ~/perf-nightly/qlcconsole
#
# The machine needs a committed baseline for its host class
# (docs/perf-data/baseline-<class>.csv); without one the first night
# records it into the clone and says so in latest.txt -- commit it from there.
set -u
PERF_NIGHTLY=${PERF_NIGHTLY:-$HOME/perf-nightly}
SRC=$PERF_NIGHTLY/qlcconsole
BRANCH=${PERF_BRANCH:-main}
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
mkdir -p "$PERF_NIGHTLY/runs"
STAMP=$(date +%Y-%m-%dT%H%M)

# One at a time.
LOCK=$PERF_NIGHTLY/.lock
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "perf=3 skipped: another nightly run holds $LOCK ($STAMP)" > "$PERF_NIGHTLY/latest.txt"
    exit 3
fi
trap 'rmdir "$LOCK"' EXIT

# Move the clone to origin/main, then run the script as it is THERE (this
# file may just have changed under the running shell).
if [ -z "${NIGHTLY_UPDATED:-}" ]; then
    if [ ! -d "$SRC/.git" ]; then
        echo "perf=2 no clone at $SRC: git clone https://github.com/sandinak/qlcconsole.git $SRC" \
            > "$PERF_NIGHTLY/latest.txt"
        exit 2
    fi
    git -C "$SRC" fetch -q origin "$BRANCH" && git -C "$SRC" checkout -q --detach FETCH_HEAD || {
        echo "perf=2 git fetch of origin/$BRANCH failed ($STAMP)" > "$PERF_NIGHTLY/latest.txt"; exit 2; }
    rmdir "$LOCK"; trap - EXIT
    NIGHTLY_UPDATED=1 exec sh "$SRC/tools/perf/nightly.sh" "$@"
fi

REV=$(git -C "$SRC" rev-parse --short=12 HEAD)
OUT=$PERF_NIGHTLY/runs/$STAMP
mkdir -p "$OUT"

# A gate or another benchmark on this machine would be measured too.
if pgrep -f "check-all[.]sh" >/dev/null 2>&1 || pgrep -f "perf_sweep[.]py|run_soak[.]py" >/dev/null 2>&1; then
    RC=3
    echo "a gate or a perf run is running" > "$OUT/guard.log"
else
    python3 "$SRC/tools/perf/guard.py" --rev "$REV" --build-dir "$PERF_NIGHTLY/build-perf" \
        --out "$OUT" --trend "$PERF_NIGHTLY/trend.csv" > "$OUT/guard.log" 2>&1
    RC=$?
fi

HOSTCLASS=$(python3 -c "import sys; sys.path.insert(0, '$SRC/tools/perf'); import guard; print(guard.host_class(''))" 2>/dev/null)
{
    echo "perf=$RC host=$HOSTCLASS rev=$REV date=$STAMP"
    case $RC in
        0) grep -E "^perf=0|^Recorded" "$OUT/guard.log" ;;
        1) grep -E "REGRESSED|^perf=1" "$OUT/guard.log" ;;
        3) echo "skipped: the machine was busy"; tail -n 2 "$OUT/guard.log" ;;
        *) echo "error:"; tail -n 15 "$OUT/guard.log" ;;
    esac
    echo "full log: $OUT/guard.log"
} > "$PERF_NIGHTLY/latest.txt"
cp "$PERF_NIGHTLY/latest.txt" "$OUT/summary.txt"

# Keep the last 30 nights.
N=$(ls -1d "$PERF_NIGHTLY"/runs/*/ 2>/dev/null | wc -l)
if [ "$N" -gt 30 ]; then
    ls -1d "$PERF_NIGHTLY"/runs/*/ | sort | head -n $((N - 30)) | while read -r d; do rm -rf "$d"; done
fi
exit $RC
