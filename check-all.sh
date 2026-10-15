#!/bin/bash
# Run the full test gate: Qt6 Debug, Release, and Debug with warnings as
# errors.
#
# Qt6 only, since the Qt6-only conversion (2026-10;
# the tag qt5-last is the last main that also built on Qt5). The Qt5 leg had
# earned its place -- three defects in one week came from the Qt5/Qt6 split
# (QTreeWidget::mimeData() by value vs const-ref; Homebrew's plugin paths;
# the Qt5 script engine lacking ES6 Array.prototype.fill()) -- and went with
# the split itself.
#
# The Release leg: CMAKE_BUILD_TYPE changes optimisation, and at least one
# shipped bug (undefined narrowing in VCXYPadFixture::writeDMX) only
# manifested under -O2 -- invisible to every Debug-ish local build.
#
# CI (.github/workflows/build.yml) builds and tests macOS and Linux on every
# push; this is the local gate to run before pushing.
set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Performance cores only, not hw.ncpu -- on Apple Silicon that also counts
# efficiency cores, which saturating slows down everything else on the machine.
JOBS="$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || nproc)"
FAILED=""

run_gate() { # <label> <build-dir> <qt-prefix-or-empty>
    local label="$1" dir="$2" prefix="$3" extra="${4:-}"
    echo
    echo "==================== $label ===================="
    if [ ! -d "$prefix" ] && [ -n "$prefix" ]; then
        echo "SKIP $label: $prefix not installed"
        return 0
    fi

    local args=(-S "$REPO" -B "$REPO/$dir")
    [ -n "$prefix" ] && args+=(-DCMAKE_PREFIX_PATH="$prefix/lib/cmake")
    # QUOTED. Unquoted, the shell split "-DCMAKE_CXX_FLAGS=-Werror=a -Werror=b"
    # into separate words and cmake saw "-Werror=b" as its OWN option -- a -W
    # category it does not know. Harmless until CMake 4.4 started treating an
    # unknown -W category as an error, at which point the Werror leg stopped
    # configuring at all. The whole string is one argument by design.
    [ -n "$extra" ] && args+=("$extra")

    local t0 t1 t2 t3
    t0=$(date +%s)
    if ! QTDIR="$prefix" cmake "${args[@]}"; then
        FAILED="$FAILED $label(configure)"; return 1
    fi
    t1=$(date +%s)
    # Build first, then test, so each is timed on its own: compiling scales
    # with cores, the tests (run one after another, many waiting on timers)
    # mostly don't -- which is the number that says whether more machines or
    # a faster one would help.
    if ! QTDIR="$prefix" cmake --build "$REPO/$dir" -j"$JOBS"; then
        FAILED="$FAILED $label(build)"; return 1
    fi
    t2=$(date +%s)
    if ! QTDIR="$prefix" cmake --build "$REPO/$dir" -j"$JOBS" --target check; then
        FAILED="$FAILED $label(check)"; return 1
    fi
    t3=$(date +%s)
    TIMINGS="$TIMINGS
$(printf '  %-12s configure %4ss   build %5ss   test %5ss   total %5ss' \
        "$label" $((t1 - t0)) $((t2 - t1)) $((t3 - t2)) $((t3 - t0)))"
    echo "PASS $label"
}
TIMINGS=""
GATE_START=$(date +%s)

run_gate "Qt6" build-qt6 /opt/homebrew/opt/qt6  "" || true

# Release matters on its own, not as a variant: VCXYPadFixture::writeDMX()
# narrowed an unclamped double to ushort, which is undefined behaviour that
# Debug happened to wrap and -O2 did not. Every local gate passed; only the
# macOS CI job, which configures Release, caught it. One Release leg keeps that
# class in reach locally instead of waiting for CI to find it.
run_gate "Qt6-Release" build-qt6-rel /opt/homebrew/opt/qt6 "-DCMAKE_BUILD_TYPE=Release" || true

# The Linux CI job builds with -Werror, and several of the classes it enforces
# are invisible to a default local build -- -Wreorder in particular has now
# broken CI twice from changes that passed every leg above. clang can see most
# of them; turn them on here so the feedback is a minute away instead of a
# push-and-wait. NOT -Werror across the board: the tree carries ~1150 warnings
# outside CI's set (mostly third-party headers), so this enforces only the
# classes CI actually fails on.
WERROR_SET="-Werror=reorder-ctor -Werror=dangling-else -Werror=misleading-indentation"
WERROR_SET="$WERROR_SET -Werror=format -Werror=macro-redefined -Werror=unused-function"
WERROR_SET="$WERROR_SET -Werror=unused-variable -Werror=deprecated-declarations"
# CI's Linux job is gcc with -Wall -Wextra -Werror. These are the classes it
# shares with clang that slipped through the set above and broke it
# (2026-10-12: an unused parameter, an ignored [[nodiscard]], a switch
# missing an enum value, a set-but-unused variable, a sign-compare).
WERROR_SET="$WERROR_SET -Werror=unused-parameter -Werror=unused-result -Werror=switch"
WERROR_SET="$WERROR_SET -Werror=unused-but-set-variable -Werror=sign-compare"
run_gate "Qt6-Werror" build-qt6-werror /opt/homebrew/opt/qt6 "-DCMAKE_CXX_FLAGS=$WERROR_SET" || true

echo
echo "Timings:$TIMINGS"
echo "  whole gate $(( $(date +%s) - GATE_START ))s"
echo
if [ -n "$FAILED" ]; then
    echo "FAILED:$FAILED"
    exit 1
fi
echo "All configurations passed."
