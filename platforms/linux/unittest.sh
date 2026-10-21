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

# Which Qt platform the tests get. This decides HOW the UI tests run, never
# WHETHER: every branch below runs them. An earlier version only ran ui/test
# when it could find an X server (`pidof X` / `pidof Xorg`) or the login was a
# known CI user, so on a headless Linux box under a normal login all the UI
# test programs -- a third of the suite -- were silently skipped while the gate
# still printed "Unit tests passed". The run is now also counted (see the end
# of this script), so a skip can't pass quietly again.
if [ -n "${QT_QPA_PLATFORM:-}" ]; then
  # The caller chose a platform (cocoa to watch the tests on macOS, xcb to
  # watch them on a Linux desktop, offscreen from the top-level unittest.sh).
  echo "Using the caller's QT_QPA_PLATFORM='$QT_QPA_PLATFORM'"
  TESTPREFIX=""
elif [[ "$OSTYPE" == "linux-gnu"* ]] && [ "$IS_CI" -eq 1 ]; then
  # CI: offscreen, as macOS, Windows and headless Linux run. It used to be
  # the minimal platform under Xvfb, which has no real fonts: every width
  # taken from text is wrong there (a 1280 px window's top bar asked for
  # 1133 px, a label for 768), so tests of what folds or fits, and clicks
  # aimed at a row's rectangle, failed with nothing wrong -- MainBar_Test,
  # ShowTreeView_Test, ProgrammingManager_Test. The whole suite passes on
  # offscreen on Linux (Debian 13, Qt 6.8.2, 2026-10-21).
  TESTPREFIX="QT_QPA_PLATFORM=offscreen"
elif [[ "$OSTYPE" == "linux-gnu"* ]] && [ -n "${DISPLAY:-}" ] \
    && command -v xdpyinfo >/dev/null 2>&1 && xdpyinfo >/dev/null 2>&1; then
  # A real, reachable X session: use it, as headed runs always have.
  TESTPREFIX=""
else
  # Headless Linux, macOS, anything else: Qt's offscreen platform needs no
  # display at all. (xvfb-run is no substitute for this check: its server is
  # named Xvfb, which `pidof X` never matched.)
  TESTPREFIX="QT_QPA_PLATFORM=offscreen"
fi

# Windows (MSYS2 bash, as CI runs it): no display needed for offscreen, and
# no rpath -- test.sh's LD_LIBRARY_PATH means nothing to the Windows loader,
# which finds DLLs on PATH. Put every build dir holding one there.
# QT_FORCE_STDERR_LOGGING: a Windows process without a console (CI's piped
# output) has QTest send its whole log to OutputDebugString instead of stdout,
# so every test, failing ones included, ran silently. A caller's
# QT_QPA_PLATFORM still wins, as above.
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
  if [ -n "${QT_QPA_PLATFORM:-}" ]; then
    TESTPREFIX="QT_FORCE_STDERR_LOGGING=1"
  else
    TESTPREFIX="QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1"
  fi
  DLLDIRS=$(find "$PWD" -name '*.dll' -not -path '*/CMakeFiles/*' -exec dirname {} \; | sort -u | tr '\n' ':')
  export PATH="${DLLDIRS}${PATH}"
fi

# How many test programs actually ran, per group -- checked at the end.
# Run one test's test.sh. A connection to a slot or signal that doesn't
# exist only logs "No such slot"/"No such signal" and the feature it was for
# silently does nothing (the Studio's Add Target/Copy/Paste, the patch grid,
# the Function Manager's actions all did, for weeks): such a log fails the test.
run_test()
{
    local out rc
    out=$(mktemp)
    eval $TESTPREFIX ./test.sh 2>&1 | tee "$out"
    rc=${PIPESTATUS[0]}
    if grep -qE "No such (slot|signal)" "$out"; then
        echo "*** A broken signal/slot connection was logged:"
        grep -E "No such (slot|signal)" "$out" | sort | uniq -c
        [ "$rc" -eq 0 ] && rc=1
    fi
    rm -f "$out"
    return $rc
}

ENGINE_RAN=0
UI_RAN=0
UI_EXPECTED=0

#############################################################################
# Fixture definitions check with xmllint
#############################################################################

pushd resources/fixtures/scripts
./check
RET=$?
popd
if [ $RET -ne 0 ]; then
    echo "Fixture definitions are not valid. Please fix before commit."
    exit $RET
fi

#############################################################################
# Engine tests
#############################################################################

TESTDIR=engine/test
TESTS=$(find ${TESTDIR} -maxdepth 1 -mindepth 1 -type d)
for test in ${TESTS}
do
    # Ignore .git
    if [ $(echo ${test} | grep ".git") ]; then
        continue
    fi

    # Ignore CMakeFiles
    if [ $(echo ${test} | grep "CMakeFiles") ]; then
        continue
    fi

    # Isolate just the test name
    test=$(echo ${test} | sed 's/engine\/test\///')

    $SLEEPCMD
    # Execute the test
    pushd ${TESTDIR}/${test}
    echo "$TESTPREFIX ./test.sh"
    run_test
    RESULT=${?}
    popd
    if [ ${RESULT} != 0 ]; then
        echo "${RESULT} Engine unit tests failed. Please fix before commit."
        exit ${RESULT}
    fi
    ENGINE_RAN=$((ENGINE_RAN + 1))
done

#############################################################################
# UI tests
#############################################################################

# Skip ui in qmlui mode
if [ "$TARGET" != "qmlui" ]; then

TESTDIR=ui/test
TESTS=$(find ${TESTDIR} -maxdepth 1 -mindepth 1 -type d)
for test in ${TESTS}
do
    # Ignore .git
    if [ $(echo ${test} | grep ".git") ]; then
        continue
    fi

    # Ignore CMakeFiles
    if [ $(echo ${test} | grep "CMakeFiles") ]; then
        continue
    fi

    # Isolate just the test name
    test=$(echo ${test} | sed 's/ui\/test\///')

    UI_EXPECTED=$((UI_EXPECTED + 1))
    if [ ! -f ${TESTDIR}/${test}/test.sh ]; then
        echo "UI test '${test}' has no test.sh in the build dir, so it can't be run. Please fix before commit."
        exit 1
    fi

    $SLEEPCMD
    # Execute the test via its checked-in test.sh (sets LD_LIBRARY_PATH/
    # DYLD_FALLBACK_LIBRARY_PATH itself, and already resolves the macOS
    # <name>_test.app/Contents/MacOS/<name>_test bundle path vs. a plain
    # executable — the old bare "./${test}_test" here didn't).
    pushd ${TESTDIR}/${test}
    run_test
    RESULT=${?}
    popd
    if [ ${RESULT} != 0 ]; then
        echo "${RESULT} UI unit tests failed. Please fix before commit."
        exit ${RESULT}
    fi
    UI_RAN=$((UI_RAN + 1))
done

fi

#############################################################################
# Enttec wing tests
#############################################################################

$SLEEPCMD
pushd plugins/enttecwing/test
run_test
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} Enttec wing unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# Velleman test
#############################################################################

if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Skip Velleman test (not supported on OSX)"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
  echo "Skip Velleman test (plugin not built on Windows)"
else
  $SLEEPCMD
  pushd plugins/velleman/test
  run_test
  RESULT=$?
  if [ $RESULT != 0 ]; then
    echo "Velleman unit test failed ($RESULT). Please fix before commit."
	exit $RESULT
  fi
  popd
fi

#############################################################################
# MIDI tests
#############################################################################

$SLEEPCMD
pushd plugins/midi/test
run_test
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} MIDI unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# ArtNet tests
#############################################################################

$SLEEPCMD
pushd plugins/artnet/test
run_test
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} ArtNet unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# E1.31 tests
#############################################################################

$SLEEPCMD
pushd plugins/E1.31/test
run_test
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} E1.31 unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# WLED tests
#############################################################################

$SLEEPCMD
pushd plugins/wled/test
run_test
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} WLED unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# Final judgment
#############################################################################

# A gate that ran nothing must not pass. Count what actually ran, say it, and
# fail if a whole group came up empty -- that is how the UI tests used to go
# missing for a third of the suite without anyone noticing.
echo "Test programs run: ${ENGINE_RAN} engine, ${UI_RAN} of ${UI_EXPECTED} UI" \
     "(platform: ${TESTPREFIX:-QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-default}})"
if [ ${ENGINE_RAN} -eq 0 ]; then
    echo "No engine test programs were run. This is not a pass."
    exit 1
fi
if [ "$TARGET" != "qmlui" ] && [ ${UI_RAN} -eq 0 ]; then
    echo "No UI test programs were run (is ui/test in the build dir?). This is not a pass."
    exit 1
fi

echo "Unit tests passed."
