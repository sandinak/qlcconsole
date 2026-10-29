#!/bin/bash
#
# To measure unit test coverage, perform these steps:
# 0. export CCACHE_DISABLE=1        # (only if you use compiler cache)
# 1. rm -rf build && mkdir build    # Remove original build directory and recreate it. You can skip this step if you want to preserve the build directory.
# 2. cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Dcoverage=ON
# 3. cmake --build build -j8
# 4. cmake --build build --target lcov     (or: ./coverage.sh ui)
#
# Human-readable HTML results are written under coverage/html, the combined
# tracefile to coverage/coverage.info, and a per-file line + branch table to
# coverage/per-file.tsv. COVERAGE_MEASURE_FAILED=1 measures even when a test
# failed (the failures are still listed and the exit status still says so).
# The numbers and how they were taken: docs/TEST_MATRIX.md, "Coverage".
#

ARCH=$(uname)
THISCMD=`basename "$0"`

TARGET=${1:-}

if [ "$TARGET" != "ui" ] && [ "$TARGET" != "qmlui" ]; then
  echo >&2 "Usage: $THISCMD ui|qmlui"
  exit 1
fi

DEST_DIR="build" # Do NOT change to "./build" or "build/"

#############################################################################
# lcov options
#############################################################################

# Check if lcov is installed
if [ -z "$(which lcov)" ] || [ -z "$(which genhtml)" ]; then
    echo >&2 "Unable to produce coverage results; can't find lcov/genhtml."
    exit 1
fi

# Branch coverage too. lcov 2.x names the switch branch_coverage and stops on
# gcov/geninfo mismatches lcov 1.x passed over (a header seen by two
# compilers, a counter a thread raced: see coverage.cmake); report and go on.
LCOV_MAJOR=$(lcov --version | sed -n 's/.*version \([0-9]*\).*/\1/p')
if [ "${LCOV_MAJOR:-1}" -ge 2 ]; then
    LCOV_OPTS="--rc branch_coverage=1 --keep-going --ignore-errors mismatch,negative,inconsistent,unused,empty,source,gcov,format,range,count"
    GENHTML_OPTS="--rc branch_coverage=1 --keep-going --ignore-errors mismatch,negative,inconsistent,unused,empty,source,format,range,count"
else
    LCOV_OPTS="--rc lcov_branch_coverage=1"
    GENHTML_OPTS="--rc genhtml_branch_coverage=1"
fi
lcov_() { lcov $LCOV_OPTS "$@"; }

#############################################################################
# Preparation
#############################################################################

# The whole build tree, not a list of directories: the list this script used
# to keep named engine/src, ui/src and three plugin dirs, so every plugin
# library a test links (artnet, E1.31, OSC, MIDI, WLED: their controllers),
# the shared plugins/interfaces sources compiled into the plugin tests
# (rdmprotocol.cpp) and webaccess were never measured -- 0%, or missing.
rm -rf coverage
mkdir -p coverage/html

lcov_ -d $DEST_DIR -z || exit $?
lcov_ -d $DEST_DIR -c -i -o coverage/base.info || exit $?

#############################################################################
# Run unit tests
#############################################################################

# Qt's per-function watchdog is 300 s. A gcov build runs several times
# slower (TrackBack_Test::courseShows: ~60 s in a Debug build on an M4,
# 357 s under gcov on the CI runner, killed by the watchdog with signal 6).
# Scaled for coverage builds only, so a real hang in a normal build still
# fails at 300 s; set QTEST_FUNCTION_TIMEOUT (ms) yourself to override.
export QTEST_FUNCTION_TIMEOUT=${QTEST_FUNCTION_TIMEOUT:-1500000}

# One program at a time: every test writes the shared libraries' .gcda
# counters on exit, and coverage is not where the gate's speed matters.
QLC_TEST_JOBS=${QLC_TEST_JOBS:-1} ./unittest.sh $TARGET
FAILED=$?
if [ ${FAILED} != 0 ]; then
    if [ "${COVERAGE_MEASURE_FAILED:-0}" != 1 ]; then
        echo "Will not measure coverage because ${FAILED} unit tests failed."
        exit ${FAILED}
    fi
    echo "${FAILED} unit tests failed; measuring anyway (COVERAGE_MEASURE_FAILED=1)."
fi

#############################################################################
# Gather results, combined and HTMLized
#############################################################################

lcov_ -d $DEST_DIR -c -o coverage/test.info || exit $?
lcov_ -a coverage/base.info -a coverage/test.info -o coverage/coverage.info || exit $?

# Remove stuff that isn't part of QLC sources
lcov_ -r coverage/coverage.info '*.h' -o coverage/coverage.info # Q_OBJECT etc.
lcov_ -r coverage/coverage.info '*moc_*' -o coverage/coverage.info
lcov_ -r coverage/coverage.info '/usr/*' '/opt/*' -o coverage/coverage.info
lcov_ -r coverage/coverage.info '*_test*' '*/test/*' '*/tests/*' -o coverage/coverage.info
lcov_ -r coverage/coverage.info '*/ui_*' -o coverage/coverage.info
lcov_ -r coverage/coverage.info "*/$DEST_DIR/*" -o coverage/coverage.info
lcov_ -r coverage/coverage.info '*Library*' -o coverage/coverage.info # OSX

# Generate HTML report
genhtml $GENHTML_OPTS -o coverage/html coverage/coverage.info

# Per file: lines found/hit, branches found/hit (LF/LH/BRF/BRH records).
awk -v root="$PWD/" '
    /^SF:/  { f = substr($0, 4); sub("^" root, "", f); lf = lh = bf = bh = 0 }
    /^LF:/  { lf = substr($0, 4) }
    /^LH:/  { lh = substr($0, 4) }
    /^BRF:/ { bf = substr($0, 5) }
    /^BRH:/ { bh = substr($0, 5) }
    /^end_of_record/ { print f "\t" lf "\t" lh "\t" bf "\t" bh }
' coverage/coverage.info | sort > coverage/per-file.tsv.body
{ printf 'file\tlines\tlines_hit\tbranches\tbranches_hit\n'; cat coverage/per-file.tsv.body; } > coverage/per-file.tsv
rm -f coverage/per-file.tsv.body

exit ${FAILED}
