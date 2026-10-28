#!/bin/bash

CURRUSER=$(whoami)
TESTPREFIX=""
SLEEPCMD=""
THISCMD=`basename "$0"`

TARGET=${1:-}
# NOTE: this script is not run in place. The root-level ./unittest.sh stages
# it (plus a fresh copy of every test.sh + the resources/ tests need) into
# the build dir and cd's there BEFORE invoking this copy — so by the time we
# get here, CWD already IS the build dir and plain relative paths
# (${TESTDIR}/${test}) are correct. Don't reintroduce a build-dir prefix here.

if [ "$TARGET" != "ui" ] && [ "$TARGET" != "qmlui" ]; then
  echo >&2 "Usage: $THISCMD ui|qmlui"
  exit 1
fi

# No modal may wait for a click: on a real display (xcb, cocoa) the
# Programming tab's first-run guide and App's recovery offer would. Offscreen
# and minimal skip them anyway; each UI test sets this too
# (ui/test/common/testsettings.cpp), this covers anything run from here.
export QLC_NO_RECOVERY_PROMPT=1

IS_CI=0
if [ "$CURRUSER" == "runner" ] \
    || [ "$CURRUSER" == "buildbot" ] \
    || [ "$CURRUSER" == "abuild" ]; then
  echo "Found build environment with CURRUSER='$CURRUSER' and OSTYPE='$OSTYPE'"
  IS_CI=1
fi

# Which Qt platform the tests get: offscreen, always, on every OS -- no test
# may open a window on the real screen (or, crashing, leave a dialog there).
# This decides HOW the UI tests run, never WHETHER: every branch runs them.
# (An earlier version only ran ui/test when it found an X server, so on a
# headless Linux box all the UI programs were silently skipped; the run is
# now also counted, see the end of this script.) Offscreen needs no display,
# and has real fonts where Xvfb's minimal platform didn't (every width taken
# from text was wrong there). Three walls: this, each test.sh, and
# engine/test/common/testguard.cpp in every program; check-test-guard.sh,
# below, fails the run when one is missing.
#
# QLC_TEST_REAL_SCREEN=1 lifts all three, to watch the tests on purpose:
# then the caller's QT_QPA_PLATFORM (cocoa, xcb), or Qt's default.
if [ "${QLC_TEST_REAL_SCREEN:-}" = 1 ]; then
  echo "QLC_TEST_REAL_SCREEN=1: the tests may use the real screen (QT_QPA_PLATFORM='${QT_QPA_PLATFORM:-}')"
  TESTPREFIX=""
else
  export QT_QPA_PLATFORM=offscreen
  TESTPREFIX="QT_QPA_PLATFORM=offscreen"
fi

# Windows (MSYS2 bash, as CI runs it): no display needed for offscreen, and
# no rpath -- test.sh's LD_LIBRARY_PATH means nothing to the Windows loader,
# which finds DLLs on PATH. Put every build dir holding one there.
# QT_FORCE_STDERR_LOGGING: a Windows process without a console (CI's piped
# output) has QTest send its whole log to OutputDebugString instead of stdout,
# so every test, failing ones included, ran silently.
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
  TESTPREFIX="$TESTPREFIX QT_FORCE_STDERR_LOGGING=1"
  DLLDIRS=$(find "$PWD" -name '*.dll' -not -path '*/CMakeFiles/*' -exec dirname {} \; | sort -u | tr '\n' ':')
  export PATH="${DLLDIRS}${PATH}"
fi

#############################################################################
# How the test programs run
#############################################################################
#
# Concurrently: QLC_TEST_JOBS programs at a time, by default the performance
# cores less two (all cores less two where there is no performance tier),
# never fewer than one. Run one after another, the suite took eight minutes a
# build on a machine with ten idle cores: most programs spend their time
# waiting on timers, not computing.
#
# Each program gets what it would have had to itself: its own build dir as
# its working directory, its own TMPDIR and HOME (fresh folders, removed
# after), so nothing one writes to a temporary file or to "your" folders
# meets another's -- QLCFile::userDirectory() is under $HOME, and three legs'
# QLCFixtureDefCache_Test::storeDef() wrote and removed the same
# ~/Library/Application Support/.../storeTest.qxf at once (and every run
# left things in the real one). QSettings are
# already per program -- the UI tests keep theirs in a QTemporaryDir
# (ui/test/common/testsettings.cpp), which now lands in that TMPDIR, and the
# engine tests that save any point QSettings at their own temporary folder.
#
# Some tests measure wall-clock time (a fade's length, a timer's rate) and
# can't promise their budget on a loaded machine. unittest-serial.txt (next
# to this script; staged with it) lists them: they run after the concurrent
# batch, one at a time. Add a test there, with the reason, rather than
# widening its budget until it means nothing.
#
# Settings (environment):
#   QLC_TEST_JOBS=N     programs at once (1 = the old one-after-another run)
#   QLC_TEST_PHASE      all (default) | parallel | serial -- check-all.sh runs
#                       every leg's parallel batch first, then the serial
#                       lists with nothing else running; stage = only put
#                       the tests' files in place and check the fixtures
#                       (CI's Windows job, which runs the programs itself)
#   QLC_TEST_ONLY="engine/test/scene ui/test/workbench ..."
#                       run only these test dirs (check-all.sh --quick)
#   QLC_TEST_TIMES      where the last run's per-test times are kept, to start
#                       the longest first (default ~/.cache/qlcconsole/)
#
# Every program runs to the end, failing or not, so one run names every
# failure; the output is one line per program, then each failure's log.

PHASE="${QLC_TEST_PHASE:-all}"
case "$PHASE" in
  all|parallel|serial|stage) ;;
  *) echo >&2 "QLC_TEST_PHASE must be all, parallel, serial or stage (got '$PHASE')"; exit 1 ;;
esac

if [ -n "${QLC_TEST_JOBS:-}" ]; then
  JOBS="$QLC_TEST_JOBS"
else
  CORES=$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null \
          || sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 3)
  JOBS=$((CORES - 2))
fi
case "$JOBS" in ''|*[!0-9]*) JOBS=1 ;; esac
[ "$JOBS" -lt 1 ] && JOBS=1

SERIAL_LIST=unittest-serial.txt
TIMES="${QLC_TEST_TIMES:-${HOME:-/tmp}/.cache/qlcconsole/unittest-times}"

LOGDIR="$PWD/test-logs"
TMPBASE=$(mktemp -d "${TMPDIR:-/tmp}/qlctests.XXXXXX") || exit 1
trap 'rm -rf "$TMPBASE"' EXIT

# Run one test's test.sh, in its own dir with its own TMPDIR, output to its
# log; record "<exit> <seconds>" beside it and print one line. A connection
# to a slot or signal that doesn't exist only logs "No such slot"/"No such
# signal" and the feature it was for silently does nothing (the Studio's Add
# Target/Copy/Paste, the patch grid, the Function Manager's actions all did,
# for weeks): such a log fails the test. Always exits 0 -- xargs stops
# handing out work after a child exits 255 -- the verdict is in the file.
run_one()
{
    local t="$1" name log tmp start rc dur done verdict
    name=$(echo "$t" | tr '/' '_')
    log="$LOGDIR/$name.log"
    start=$(date +%s)
    if [ ! -f "$t/test.sh" ]; then
        echo "'$t' has no test.sh in the build dir, so it can't be run." > "$log"
        rc=1
    else
        tmp=$(mktemp -d "$TMPBASE/$name.XXXXXX")
        mkdir -p "$tmp/home"
        ( cd "$t" && export TMPDIR="$tmp/" HOME="$tmp/home" && eval $TESTPREFIX ./test.sh ) \
            > "$log" 2>&1 < /dev/null
        rc=$?
        rm -rf "$tmp"
        if grep -qE "No such (slot|signal)" "$log"; then
            {
                echo "*** A broken signal/slot connection was logged:"
                grep -E "No such (slot|signal)" "$log" | sort | uniq -c
            } >> "$log"
            [ "$rc" -eq 0 ] && rc=1
        fi
    fi
    dur=$(( $(date +%s) - start ))
    echo "$rc $dur" > "$LOGDIR/$name.rc"
    touch "$TMPBASE/done/$name"
    done=$(ls "$TMPBASE/done" | wc -l | tr -d ' ')
    if [ "$rc" -eq 0 ]; then
        verdict="PASS"
    elif [ "$rc" -gt 128 ]; then
        verdict="FAIL (signal $((rc - 128)))"
    else
        verdict="FAIL (exit $rc)"
    fi
    printf '[%3s/%s] %-44s %4ss  %s\n' "$done" "$TOTAL" "$t" "$dur" "$verdict"
    return 0
}

# The test dirs, longest (as last timed) first, so a long one doesn't start
# last and keep the run going alone at the end. Unknown ones go first.
order_by_time()
{
    awk -v f="$TIMES" '
        BEGIN { while ((getline l < f) > 0) { split(l, a, " "); t[a[2]] = a[1] } }
        { print (($0 in t) ? t[$0] : 100000), $0 }' \
    | sort -k1,1nr -k2,2 | cut -d' ' -f2
}

# Run the dirs on stdin, $1 at a time.
run_batch()
{
    # "$BASH", not bash: the exported function must reach the same shell
    # (macOS has bash 3.2 as /bin/bash and often a newer one first on PATH).
    xargs -n 1 -P "$1" "$BASH" -c 'run_one "$1"' _
}

#############################################################################
# Fixture definitions check with xmllint
#############################################################################

if [ "$PHASE" != "serial" ]; then
  pushd resources/fixtures/scripts
  ./check
  RET=$?
  popd
  if [ $RET -ne 0 ]; then
      echo "Fixture definitions are not valid. Please fix before commit."
      exit $RET
  fi
fi

[ "$PHASE" = "stage" ] && exit 0

#############################################################################
# The test programs
#############################################################################

list_tests()
{
    local d
    for d in $(find engine/test -maxdepth 1 -mindepth 1 -type d | sort); do
        case "$d" in *.git*|*CMakeFiles*) continue ;; esac
        echo "$d"
    done
    if [ "$TARGET" != "qmlui" ]; then
        for d in $(find ui/test -maxdepth 1 -mindepth 1 -type d | sort); do
            case "$d" in *.git*|*CMakeFiles*) continue ;; esac
            echo "$d"
        done
    fi
    echo plugins/enttecwing/test
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo >&2 "Skip Velleman test (not supported on OSX)"
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        echo >&2 "Skip Velleman test (plugin not built on Windows)"
    else
        echo plugins/velleman/test
    fi
    echo plugins/midi/test
    echo plugins/artnet/test
    echo plugins/E1.31/test
    echo plugins/osc/test
    echo plugins/wled/test
    if [ "$TARGET" != "qmlui" ]; then
        echo webaccess/test
    fi
    # The HID plugin's GameController side: a Mac's framework only.
    [[ "$OSTYPE" == "darwin"* ]] && echo plugins/hid/test
    return 0
}

ALL_TESTS=$(list_tests)

# Every test.sh starts offscreen and every program has the test guard (see
# the top of this script), or nothing runs. Once per run: not again for the
# serial phase.
if [ "$PHASE" != "serial" ]; then
  if ! ./check-test-guard.sh --build . $ALL_TESTS; then
    echo "A test could open a window on the real screen. This is not a pass."
    exit 1
  fi
fi
if [ -n "${QLC_TEST_ONLY:-}" ]; then
    SELECTED=""
    for t in $ALL_TESTS; do
        for w in $QLC_TEST_ONLY; do
            [ "$t" = "${w%/}" ] && SELECTED="$SELECTED $t"
        done
    done
    for w in $QLC_TEST_ONLY; do
        echo "$ALL_TESTS" | grep -qx "${w%/}" \
            || echo "QLC_TEST_ONLY: no test dir '$w' in this build -- ignored"
    done
else
    SELECTED="$ALL_TESTS"
fi

SERIAL_ALL=""
[ -f "$SERIAL_LIST" ] && SERIAL_ALL=$(sed -e 's/#.*//' -e 's/[[:space:]]*$//' -e '/^$/d' "$SERIAL_LIST")
PAR_TESTS=""
SER_TESTS=""
for t in $SELECTED; do
    if echo "$SERIAL_ALL" | grep -qx "$t"; then
        SER_TESTS="$SER_TESTS $t"
    else
        PAR_TESTS="$PAR_TESTS $t"
    fi
done
case "$PHASE" in
  parallel) SER_TESTS="" ;;
  serial)   PAR_TESTS="" ;;
esac
# Every test the serial list names must be in this build: a run that finds
# fewer than it should is not a pass (see the counts at the end).
if [ -z "${QLC_TEST_ONLY:-}" ]; then
    for t in $SERIAL_ALL; do
        if ! echo "$ALL_TESTS" | grep -qx "$t"; then
            echo "$SERIAL_LIST names $t, which this build dir doesn't have. This is not a pass."
            exit 1
        fi
    done
fi
RUN_TESTS="$PAR_TESTS $SER_TESTS"

mkdir -p "$LOGDIR"
for t in $RUN_TESTS; do
    n=$(echo "$t" | tr '/' '_')
    rm -f "$LOGDIR/$n.log" "$LOGDIR/$n.rc"
done
mkdir -p "$TMPBASE/done"     # the [n/N] counter

TOTAL=$(echo $RUN_TESTS | wc -w | tr -d ' ')
export -f run_one
export LOGDIR TMPBASE TESTPREFIX TOTAL

T0=$(date +%s)
NPAR=$(echo $PAR_TESTS | wc -w | tr -d ' ')
NSER=$(echo $SER_TESTS | wc -w | tr -d ' ')
if [ "$PHASE" = "serial" ]; then
    echo "Running the $NSER test programs in $SERIAL_LIST alone."
else
    echo "Running $NPAR test programs, $JOBS at a time; then $NSER alone (from $SERIAL_LIST)."
fi
if [ "$TOTAL" -eq 0 ] && [ -z "${QLC_TEST_ONLY:-}" ]; then
    echo "No test programs found in $PWD. This is not a pass."
    exit 1
fi
if [ "$NPAR" -gt 0 ]; then
    printf '%s\n' $PAR_TESTS | order_by_time | run_batch "$JOBS"
fi
T1=$(date +%s)
if [ "$NSER" -gt 0 ]; then
    echo "-- alone: the timing-sensitive tests in $SERIAL_LIST --"
    printf '%s\n' $SER_TESTS | run_batch 1
fi
T2=$(date +%s)

#############################################################################
# Final judgment
#############################################################################

ENGINE_RAN=0
UI_RAN=0
UI_EXPECTED=0
PLUGINS_RAN=0
FAILED=""
for t in $RUN_TESTS; do
    n=$(echo "$t" | tr '/' '_')
    case "$t" in ui/test/*) UI_EXPECTED=$((UI_EXPECTED + 1)) ;; esac
    if [ ! -f "$LOGDIR/$n.rc" ]; then
        FAILED="$FAILED $t"
        echo "1 0" > "$LOGDIR/$n.rc"
        echo "(never ran -- the runner was interrupted?)" >> "$LOGDIR/$n.log"
        continue
    fi
    read rc dur < "$LOGDIR/$n.rc"
    [ "$rc" -ne 0 ] && FAILED="$FAILED $t"
    case "$t" in
        engine/test/*) ENGINE_RAN=$((ENGINE_RAN + 1)) ;;
        ui/test/*)     UI_RAN=$((UI_RAN + 1)) ;;
        *)             PLUGINS_RAN=$((PLUGINS_RAN + 1)) ;;
    esac
done

# Keep this run's times for the next run's order (merged, written whole).
mkdir -p "$(dirname "$TIMES")" 2>/dev/null
{
    [ -f "$TIMES" ] && cat "$TIMES"
    for t in $RUN_TESTS; do
        n=$(echo "$t" | tr '/' '_')
        read rc dur < "$LOGDIR/$n.rc"
        echo "$dur $t new"
    done
} | awk '{ if ($3 == "new" || !($2 in t)) t[$2] = $1 } END { for (k in t) print t[k], k }' \
  | sort -k2 > "$TIMES.$$" 2>/dev/null && mv -f "$TIMES.$$" "$TIMES" 2>/dev/null
rm -f "$TIMES.$$" 2>/dev/null

echo
echo "Slowest:$(for t in $RUN_TESTS; do n=$(echo "$t" | tr '/' '_'); read rc dur < "$LOGDIR/$n.rc"; echo "$dur $t"; done \
    | sort -k1,1nr | head -n 5 | awk '{ printf "  %s %ss", $2, $1 }')"
echo "Times: $((T1 - T0))s for $NPAR programs at $JOBS at a time, $((T2 - T1))s for $NSER alone."

# A gate that ran nothing must not pass. Count what actually ran, say it, and
# fail if a whole group came up empty -- that is how the UI tests used to go
# missing for a third of the suite without anyone noticing. (Not for the
# serial phase on its own or a QLC_TEST_ONLY selection: those choose a part.)
echo "Test programs run: ${ENGINE_RAN} engine, ${UI_RAN} of ${UI_EXPECTED} UI, ${PLUGINS_RAN} plugin" \
     "(phase: ${PHASE}; platform: ${TESTPREFIX:-QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-default}})"

if [ -n "$FAILED" ]; then
    for t in $FAILED; do
        n=$(echo "$t" | tr '/' '_')
        log="$LOGDIR/$n.log"
        read rc dur < "$LOGDIR/$n.rc"
        echo
        echo "==================== FAILED: $t (exit $rc) -- $log"
        if [ "$(wc -l < "$log")" -le 300 ]; then
            cat "$log"
        else
            grep -nE "^(FAIL!|XPASS|QFATAL)|Received a fatal|No such (slot|signal)|Totals:" -A 4 "$log" | head -n 200
            echo "[... last 80 lines ...]"
            tail -n 80 "$log"
        fi
    done
    echo
    echo "FAILED test programs ($(echo $FAILED | wc -w | tr -d ' ')):"
    for t in $FAILED; do echo "  $t"; done
    echo "Unit tests failed. Please fix before commit."
    exit 1
fi

if [ -z "${QLC_TEST_ONLY:-}" ] && [ "$PHASE" != "serial" ]; then
    if [ ${ENGINE_RAN} -eq 0 ]; then
        echo "No engine test programs were run. This is not a pass."
        exit 1
    fi
    if [ "$TARGET" != "qmlui" ] && [ ${UI_RAN} -eq 0 ]; then
        echo "No UI test programs were run (is ui/test in the build dir?). This is not a pass."
        exit 1
    fi
fi

echo "Unit tests passed."
