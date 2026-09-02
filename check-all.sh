#!/bin/bash
# Run the full test gate against BOTH Qt majors.
#
# Why: this fork builds against whichever Qt cmake finds, but ships macOS DMGs
# built against Qt6 while day-to-day development happens against whatever is
# installed (qt@5 here). Three separate defects in one week came from that
# split and none were caught by building one configuration:
#   - QTreeWidget::mimeData() takes the item list by value in Qt5, by
#     const-ref in Qt6 -- the widgets UI stopped compiling under Qt5.
#   - Homebrew's Qt6 keeps plugins under share/qt/, qt@5 directly under the
#     prefix -- bundles shipped with no platform plugin and could not launch.
#   - Array.prototype.fill() is ES6; the Qt5 script engine lacks it, so the
#     Lines RGB script returned an empty map on Qt5 builds.
#
# Also runs one Release leg: CMAKE_BUILD_TYPE changes optimisation, and at
# least one shipped bug (undefined narrowing in VCXYPadFixture::writeDMX) only
# manifested under -O2 -- invisible to every Debug-ish local build.
#
# CI does not cover this: .github/workflows/build.yml has a build-macos job,
# but it installs Qt6 only (no Qt5 leg), and as of this writing the workflow
# has never actually run on this repo -- `gh run list` is empty and the
# actions/workflows API reports total_count 0. Until CI is demonstrably
# running, this script is the only cross-Qt coverage there is.
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
    [ -n "$extra" ] && args+=($extra)

    if ! QTDIR="$prefix" cmake "${args[@]}"; then
        FAILED="$FAILED $label(configure)"; return 1
    fi
    if ! QTDIR="$prefix" cmake --build "$REPO/$dir" -j"$JOBS" --target check; then
        FAILED="$FAILED $label(check)"; return 1
    fi
    echo "PASS $label"
}

run_gate "Qt5" build-qt5 /opt/homebrew/opt/qt@5 "" || true
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
run_gate "Qt6-Werror" build-qt6-werror /opt/homebrew/opt/qt6 "-DCMAKE_CXX_FLAGS=$WERROR_SET" || true

echo
if [ -n "$FAILED" ]; then
    echo "FAILED:$FAILED"
    exit 1
fi
echo "All configurations passed."
