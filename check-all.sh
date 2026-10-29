#!/bin/bash
# Run the full test gate: Qt6 Debug, Release, and Debug with warnings as
# errors.
#
#   ./check-all.sh            the gate: all three legs
#   ./check-all.sh --quick    iterate: the Qt6 leg only, testing just the test
#                             dirs your change touches (since origin/main, or
#                             QUICK_BASE) -- not a gate, never push on it
#   ./check-all.sh --leg Qt6-Release [--leg ...]   just those legs
#   ./check-all.sh --spread   run the Release and Werror legs on other Macs
#                             over SSH (GATE_SPREAD) while this one runs Qt6
#   ./check-all.sh --remote auto|NAME [--rev REV]   the whole gate of commit
#                             REV (default HEAD) on whichever Mac of the pool
#                             is idle, or that one (platforms/macos/
#                             gate-pool.sh, hosts in gate-hosts.conf there)
#
# The last lines are the summary, and its first line is the result,
#     gate=N host=NAME qt=6.x.y rev=SHA
# (N = 0 passed; host: GATE_HOST or this Mac's name; rev: GATE_REV, or HEAD
# with -dirty for uncommitted changes); the same block is kept in build-gate-logs/summary.txt with each leg's full
# log beside it. Environment: QT6_PREFIX (where Qt 6 is; default: brew
# --prefix qt6, then /opt/homebrew/opt/qt6), ALLOW_SKIP=1 (a leg that can't
# run -- no Qt there -- doesn't fail the gate), NO_CCACHE=1, GATE_JOBS (the
# CPU budget, default the performance cores), QLC_TEST_JOBS (see
# platforms/linux/unittest.sh).
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
# How it runs (2026-10-24; it took 32 minutes, one leg after another, each
# leg's tests one program at a time): the legs are configured, then built
# together under one make jobserver with the performance cores as slots, so
# one leg's link steps overlap another's compiles; then all three legs'
# tests run at once, each its share of the cores (unittest.sh runs a leg's
# programs concurrently); then, with nothing else of the gate running, each
# leg's timing-sensitive tests (platforms/linux/unittest-serial.txt) alone.
# ccache, when installed, makes a rebuild of mostly unchanged sources cheap,
# also across the throwaway git-archive copies gates run on.
#
# CI (.github/workflows/build.yml) builds and tests macOS and Linux on every
# push; this is the local gate to run before pushing.
set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# cmake, brew and ccache live in Homebrew's bin, which a non-interactive SSH
# session's PATH lacks (ender, dink). Appended: a PATH that has them wins.
for d in /opt/homebrew/bin /usr/local/bin; do
    if [ -d "$d" ]; then
        case ":$PATH:" in *":$d:"*) ;; *) PATH="$PATH:$d" ;; esac
    fi
done
export PATH

QUICK=0
SPREAD=0
ONLY_LEGS=""
REMOTE=""
POOL_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --quick)  QUICK=1; POOL_ARGS+=(--quick) ;;
        --spread) SPREAD=1 ;;
        --leg)    shift; ONLY_LEGS="$ONLY_LEGS ${1:-}"; POOL_ARGS+=(--leg "${1:-}") ;;
        --remote) shift; REMOTE="${1:-auto}" ;;
        --rev)    shift; REMOTE_REV="${1:?--rev needs a commit}" ;;
        -h|--help) sed -n '2,25p' "$0"; exit 0 ;;
        *) echo >&2 "check-all.sh: unknown option '$1' (try --help)"; exit 2 ;;
    esac
    shift
done

# --remote: the pool runs this script on the host it picks, on a git archive
# of the commit; exec, so this process is the pool's (a check-all.sh still
# running here would mark this Mac busy to the pool's own probe).
if [ -n "$REMOTE" ]; then
    [ "$SPREAD" = 1 ] && { echo >&2 "check-all.sh: --remote runs the whole gate elsewhere; not with --spread"; exit 2; }
    exec "$REPO/platforms/macos/gate-pool.sh" --host "$REMOTE" --rev "${REMOTE_REV:-HEAD}" -- ${POOL_ARGS[@]+"${POOL_ARGS[@]}"}
fi
[ -n "${REMOTE_REV:-}" ] && { echo >&2 "check-all.sh: --rev goes with --remote (here, the gate tests the tree as it is)"; exit 2; }

# Performance cores only, not hw.ncpu -- on Apple Silicon that also counts
# efficiency cores, which saturating slows down everything else on the
# machine. Intel Macs have no hw.perflevel0 and use hw.ncpu; Linux, nproc.
JOBS="${GATE_JOBS:-$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || nproc)}"

# Where Qt 6 is: QT6_PREFIX, else Homebrew's (/opt/homebrew on Apple
# Silicon, /usr/local on Intel), else the Apple Silicon default -- which, if
# missing, skips the legs, and a skipped leg FAILS the gate (below).
# (brew --prefix qt6 names the opt/qt alias; opt/qt6 is the same Qt and what
# existing build dirs were configured with, so prefer it when it's there.)
QT6="${QT6_PREFIX:-}"
if [ -z "$QT6" ] && command -v brew >/dev/null 2>&1; then
    QT6="$(brew --prefix 2>/dev/null)/opt/qt6"
    [ -d "$QT6" ] || QT6="$(brew --prefix qt6 2>/dev/null)"
fi
[ -n "$QT6" ] || QT6=/opt/homebrew/opt/qt6
export QTDIR="$QT6"
QT_VERSION=$(sed -n 's/^set(PACKAGE_VERSION "\([0-9.]*\)")$/\1/p' \
    "$QT6/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake" "$QT6/lib/cmake/Qt6/Qt6ConfigVersion.cmake" 2>/dev/null | head -n 1)
QT_VERSION="${QT_VERSION:-none}"

# What the result line names: the pool's name for this Mac, else its own; the
# commit, else HEAD here (-dirty: tracked files differ from it).
GATE_HOST="${GATE_HOST:-$(hostname -s 2>/dev/null || hostname)}"
if [ -z "${GATE_REV:-}" ] && [ "$(git -C "$REPO" rev-parse --show-toplevel 2>/dev/null)" = "$REPO" ] \
   && GATE_REV=$(git -C "$REPO" rev-parse HEAD 2>/dev/null); then
    git -C "$REPO" diff --quiet HEAD -- 2>/dev/null || GATE_REV="$GATE_REV-dirty"
fi
# Short hashes, as git shows them: twelve digits, plus -dirty.
GATE_REV=$(echo "${GATE_REV:-unknown}" | sed -E 's/^([0-9a-f]{12})[0-9a-f]*/\1/')

LAUNCHER=()
if [ -z "${NO_CCACHE:-}" ] && command -v ccache >/dev/null 2>&1; then
    # AUTOMOC_PATH_PREFIX: moc output #includes its header by include-dir
    # relative path, not absolute, so a moc file compiled in another copy
    # is the same file to ccache.
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
              -DCMAKE_AUTOMOC_PATH_PREFIX=ON)
    # Paths relative to the checkout, and the build dir left out of the
    # hash, so a git-archive copy in a new folder hits what the last one
    # compiled (its debug info then names the first copy's paths: harmless
    # for a gate).
    export CCACHE_BASEDIR="$REPO" CCACHE_NOHASHDIR=1
    # rcc writes each resource's modification time into its qrc_*.cpp, and
    # a git-archive copy's files are as new as their commit, or as the
    # build that generated them: every qrc file missed, every gate. One
    # fixed date (the gate's builds are never shipped) makes them the same.
    export QT_RCC_SOURCE_DATE_OVERRIDE=1
    CCACHE_STATE="ccache $(ccache --version | head -n 1 | awk '{print $3}')"
else
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER=)
    CCACHE_STATE="no ccache"
fi

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

# The legs. Release matters on its own, not as a variant:
# VCXYPadFixture::writeDMX() narrowed an unclamped double to ushort, which is
# undefined behaviour that Debug happened to wrap and -O2 did not. Every local
# gate passed; only the macOS CI job, which configures Release, caught it. One
# Release leg keeps that class in reach locally instead of waiting for CI.
LEG_NAME=(Qt6 Qt6-Release Qt6-Werror)
LEG_DIR=(build-qt6 build-qt6-rel build-qt6-werror)
# QUOTED, one element each. Unquoted, the shell split
# "-DCMAKE_CXX_FLAGS=-Werror=a -Werror=b" into separate words and cmake saw
# "-Werror=b" as its OWN option -- a -W category it does not know. Harmless
# until CMake 4.4 started treating an unknown -W category as an error, at
# which point the Werror leg stopped configuring at all.
LEG_ARG=("" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_CXX_FLAGS=$WERROR_SET")
NLEGS=${#LEG_NAME[@]}

# --spread: which leg runs where (ssh destinations with ~/gates writable).
GATE_SPREAD="${GATE_SPREAD:-Qt6-Release=val Qt6-Werror=192.168.1.17}"

# The gate tests everything: a selection left in the environment by quick
# iterating must not quietly narrow it.
[ "$QUICK" = 1 ] || unset QLC_TEST_ONLY

LOGS="$REPO/build-gate-logs"
rm -rf "${LOGS:?}"
mkdir -p "$LOGS"
GATE_START=$(date +%s)

# Per leg: what to do (local / remote host / off), how it went, its times.
STATE=(); HOST=(); RESULT=(); T_CONF=(); T_BUILD=(); T_TEST=(); T_ALONE=(); NTESTS=()
for ((i = 0; i < NLEGS; i++)); do
    STATE[i]=local; HOST[i]=""; RESULT[i]=""
    T_CONF[i]=0; T_BUILD[i]=0; T_TEST[i]=0; T_ALONE[i]=0; NTESTS[i]=""
    if [ "$QUICK" = 1 ] && [ "${LEG_NAME[i]}" != Qt6 ]; then STATE[i]=off; fi
    if [ -n "$ONLY_LEGS" ] && ! echo " $ONLY_LEGS " | grep -q " ${LEG_NAME[i]} "; then STATE[i]=off; fi
    if [ "$SPREAD" = 1 ] && [ "${STATE[i]}" = local ]; then
        for m in $GATE_SPREAD; do
            [ "${m%%=*}" = "${LEG_NAME[i]}" ] && { STATE[i]=remote; HOST[i]="${m#*=}"; }
        done
    fi
done
for w in $ONLY_LEGS; do
    echo " ${LEG_NAME[*]} " | grep -q " $w " || { echo >&2 "check-all.sh: no leg '$w' (${LEG_NAME[*]})"; exit 2; }
done

log_of() { echo "$LOGS/${LEG_NAME[$1]}.log"; }
say() { echo "[$(( $(date +%s) - GATE_START ))s] $*"; }

if [ ! -d "$QT6" ]; then
    for ((i = 0; i < NLEGS; i++)); do
        if [ "${STATE[i]}" = local ]; then
            STATE[i]=off; RESULT[i]=SKIP
            say "SKIP ${LEG_NAME[i]}: $QT6 not installed (set QT6_PREFIX)"
        fi
    done
fi

# The tests' staging checks every fixture definition with xmllint and with a
# python3 that has lxml (resources/fixtures/scripts/). Without one, every leg
# builds for minutes and then runs no tests at all -- say so now. (macOS's
# /usr/bin/python3 has no lxml; an ssh session's PATH can find that one.)
if ! command -v xmllint >/dev/null 2>&1 || ! python3 -c 'import lxml.etree' >/dev/null 2>&1; then
    echo >&2 "check-all.sh: the fixture check needs xmllint and a python3 with lxml; here python3 is"
    echo >&2 "  $(command -v python3 || echo missing)$(python3 -c 'import lxml' >/dev/null 2>&1 || echo ', without lxml') -- put one first on PATH (or: python3 -m pip install --user lxml)"
    { echo "gate=1 host=$GATE_HOST qt=$QT_VERSION rev=$GATE_REV"
      echo "No python3 with lxml (or no xmllint) on PATH: nothing built or tested."; } | tee "$LOGS/summary.txt"
    exit 1
fi

# No test may open a window on the real screen: every test.sh starts
# offscreen (each leg's unittest.sh then checks the built programs for the
# test guard too). The check's own test first.
if ! GUARD_OUT=$("$REPO/platforms/linux/check-test-guard.sh" --self-test 2>&1 \
        && "$REPO/platforms/linux/check-test-guard.sh" --source "$REPO" 2>&1); then
    echo >&2 "$GUARD_OUT"
    { echo "gate=1 host=$GATE_HOST qt=$QT_VERSION rev=$GATE_REV"
      echo "check-test-guard.sh failed: a test could open a window on the screen. Nothing built or tested."; } | tee "$LOGS/summary.txt"
    exit 1
fi

# No relative link in a Markdown file (docs, README, the help, the course)
# may point at a file that isn't there.
if ! LINKS_OUT=$(python3 "$REPO/tools/check-doc-links.py" "$REPO" 2>&1); then
    echo >&2 "$LINKS_OUT"
    { echo "gate=1 host=$GATE_HOST qt=$QT_VERSION rev=$GATE_REV"
      echo "tools/check-doc-links.py failed: a dead link in the docs. Nothing built or tested."; } | tee "$LOGS/summary.txt"
    exit 1
fi

echo "Gate in $REPO on $GATE_HOST: $JOBS cores, Qt $QT_VERSION at $QT6, $CCACHE_STATE; logs in $LOGS"

#############################################################################
# --spread: the remote legs start first and run on their own
#############################################################################

REMOTE_PIDS=""
for ((i = 0; i < NLEGS; i++)); do
    [ "${STATE[i]}" = remote ] || continue
    id="spread-$(date +%Y%m%d-%H%M%S)-$$-${LEG_NAME[i]}"
    say "${LEG_NAME[i]}: on ${HOST[i]} (~/gates/$id)"
    (
        t0=$(date +%s)
        # The tree as it is here (an archive copy has no git), minus builds.
        ( cd "$REPO" && tar --exclude './build*' --exclude './.git' --exclude './dist' -czf - . ) \
          | ssh -o BatchMode=yes -o ConnectTimeout=15 "${HOST[i]}" \
              "d=\$HOME/gates/$id; mkdir -p \$d && tar -xzf - -C \$d && cd \$d && ./check-all.sh --leg ${LEG_NAME[i]}; rc=\$?; cd; rm -rf \$d; exit \$rc" \
          > "$(log_of $i)" 2>&1
        echo "$? $(( $(date +%s) - t0 ))" > "$LOGS/${LEG_NAME[i]}.remote"
    ) &
    REMOTE_PIDS="$REMOTE_PIDS $!"
done

#############################################################################
# Configure
#############################################################################

for ((i = 0; i < NLEGS; i++)); do
    [ "${STATE[i]}" = local ] || continue
    args=(-S "$REPO" -B "$REPO/${LEG_DIR[i]}" -DCMAKE_PREFIX_PATH="$QT6/lib/cmake" "${LAUNCHER[@]}")
    [ -n "${LEG_ARG[i]}" ] && args+=("${LEG_ARG[i]}")
    t0=$(date +%s)
    if cmake "${args[@]}" > "$(log_of $i)" 2>&1; then
        T_CONF[i]=$(( $(date +%s) - t0 ))
        say "${LEG_NAME[i]}: configured (${T_CONF[i]}s)"
    else
        STATE[i]=off; RESULT[i]="FAIL(configure)"
        say "${LEG_NAME[i]}: CONFIGURE FAILED"
    fi
done

#############################################################################
# Build: every local leg at once, sharing $JOBS make slots
#############################################################################

MK="$LOGS/build.mk"
{
    printf 'all:'
    for ((i = 0; i < NLEGS; i++)); do [ "${STATE[i]}" = local ] && printf ' leg%d' $i; done
    printf '\n'
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = local ] || continue
        # $(MAKE), not cmake --build: cmake doesn't pass the jobserver's file
        # descriptors on, and each leg's make then ran one job at a time.
        # (A Ninja tree -- CMAKE_GENERATOR -- gets its own third of the cores.)
        if [ -f "$REPO/${LEG_DIR[i]}/Makefile" ]; then
            cmd='$(MAKE) -C "%s" all'
        else
            cmd="cmake --build \"%s\" -j$(( (JOBS + 2) / 3 ))"
        fi
        printf "leg%d:\n\t@t0=\$\$(date +%%s); $cmd >> \"%s\" 2>&1; echo \$\$? \$\$(( \$\$(date +%%s) - t0 )) > \"%s\"\n" \
            $i "$REPO/${LEG_DIR[i]}" "$(log_of $i)" "$LOGS/${LEG_NAME[i]}.build"
    done
} > "$MK"
# ccache's hits and misses over the build (the whole Mac's: a build next
# door counts too), for the summary: a warm rebuild should be nearly all hits.
cc_stats() { ccache --print-stats 2>/dev/null | awk -F'\t' '
    $1 == "direct_cache_hit" || $1 == "preprocessed_cache_hit" { h += $2 }
    $1 == "cache_miss" { m += $2 } END { print h + 0, m + 0 }'; }
CC_BEFORE=""
[ "${#LAUNCHER[@]}" -gt 0 ] && [ "$CCACHE_STATE" != "no ccache" ] && CC_BEFORE=$(cc_stats)
if grep -q '^leg' "$MK"; then
    say "building: $(for ((i = 0; i < NLEGS; i++)); do [ "${STATE[i]}" = local ] && printf '%s ' "${LEG_NAME[i]}"; done)(-j$JOBS shared)"
    make -s -j"$JOBS" -f "$MK" all
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = local ] || continue
        read rc secs < "$LOGS/${LEG_NAME[i]}.build" || { rc=1; secs=0; }
        T_BUILD[i]=$secs
        if [ "$rc" -eq 0 ]; then
            say "${LEG_NAME[i]}: built (${secs}s)"
        else
            STATE[i]=off; RESULT[i]="FAIL(build)"
            say "${LEG_NAME[i]}: BUILD FAILED"
        fi
    done
fi

CC_LINE=""
if [ -n "$CC_BEFORE" ]; then
    read h0 m0 <<< "$CC_BEFORE"; read h1 m1 <<< "$(cc_stats)"
    h=$((h1 - h0)); m=$((m1 - m0))
    [ $((h + m)) -gt 0 ] && CC_LINE="ccache: $h hits, $m misses ($(( 100 * h / (h + m) ))% hits) over the build"
    [ -n "$CC_LINE" ] && say "$CC_LINE"
fi

#############################################################################
# Test: the legs at once, each its share of the cores; then the
# timing-sensitive tests alone
#############################################################################

QUICK_TESTS=""
if [ "$QUICK" = 1 ] && [ -n "${QLC_TEST_ONLY:-}" ]; then
    QUICK_TESTS="$QLC_TEST_ONLY"            # the caller picked them
    say "quick: testing $QUICK_TESTS"
elif [ "$QUICK" = 1 ]; then
    # The test dirs a change since QUICK_BASE (default origin/main) touches:
    # a test's own files; a test that #includes a changed source's header; a
    # plugin's test for its sources; everything when the build or the test
    # runner changed. Committed, staged, unstaged and untracked all count.
    BASE="${QUICK_BASE:-origin/main}"
    if ! MB=$(git -C "$REPO" merge-base "$BASE" HEAD 2>/dev/null); then
        echo "--quick needs a git checkout with $BASE (set QUICK_BASE); testing everything"
        QUICK_TESTS=ALL
    else
        CHANGED=$( (git -C "$REPO" diff --name-only "$MB"; git -C "$REPO" ls-files --others --exclude-standard) | sort -u)
        for f in $CHANGED; do
            case "$f" in
                CMakeLists.txt|*.cmake|unittest.sh|platforms/linux/unittest*|*/test/CMakeLists.txt|engine/test/common/*|ui/test/common/*)
                    QUICK_TESTS=ALL ;;
                engine/test/*/*|ui/test/*/*)
                    QUICK_TESTS="$QUICK_TESTS $(echo "$f" | cut -d/ -f1-3)" ;;
                plugins/*/test/*)
                    QUICK_TESTS="$QUICK_TESTS $(echo "$f" | cut -d/ -f1-3)" ;;
            esac
            case "$f" in
                plugins/*/test/*) ;;
                plugins/*)
                    p=$(echo "$f" | cut -d/ -f2)
                    [ -f "$REPO/plugins/$p/test/test.sh" ] && QUICK_TESTS="$QUICK_TESTS plugins/$p/test" ;;
            esac
            case "$f" in
                *.h|*.cpp)
                    stem=$(basename "$f"); stem="${stem%.*}"
                    for d in $(cd "$REPO" && grep -rlE "#include [\"<]$stem\.h[\">]" engine/test ui/test plugins/*/test 2>/dev/null \
                               | cut -d/ -f1-3 | sort -u); do
                        case "$d" in engine/test/common|ui/test/common) continue ;; esac
                        QUICK_TESTS="$QUICK_TESTS $d"
                    done ;;
            esac
        done
        case " $QUICK_TESTS " in *" ALL "*) QUICK_TESTS=ALL ;; esac
        [ "$QUICK_TESTS" = ALL ] || QUICK_TESTS=$(printf '%s\n' $QUICK_TESTS | sort -u | tr '\n' ' ')
    fi
    if [ "$QUICK_TESTS" = ALL ]; then
        say "quick: the build or the test runner changed -- all of the Qt6 leg's tests"
    elif [ -z "$QUICK_TESTS" ]; then
        say "quick: no test dir touched since $BASE -- built, nothing to test"
    else
        say "quick: testing $QUICK_TESTS"
    fi
fi

NLOCAL=0
for ((i = 0; i < NLEGS; i++)); do [ "${STATE[i]}" = local ] && NLOCAL=$((NLOCAL + 1)); done
if [ "$NLOCAL" -gt 0 ]; then
    # The test programs' share: the cores less two, split over the legs.
    PER=$(( (JOBS - 2 + NLOCAL - 1) / NLOCAL ))
    [ "$PER" -lt 1 ] && PER=1
    [ -n "${QLC_TEST_JOBS:-}" ] && PER="$QLC_TEST_JOBS"
fi

run_check() { # <leg> <phase>
    local i=$1
    echo "==================== tests ($2) ====================" >> "$(log_of $i)"
    # What the 'check' target runs, called directly: the tree was just built,
    # and make's walk of every target's AUTOMOC step to find nothing to do
    # cost about 40 s a leg, twice.
    ( cd "$REPO" && QLC_TEST_PHASE="$2" QLC_TEST_JOBS="$PER" ./unittest.sh ui "$REPO/${LEG_DIR[i]}" ) >> "$(log_of $i)" 2>&1
}

if [ "$QUICK" = 1 ] && [ -z "$QUICK_TESTS" ]; then
    for ((i = 0; i < NLEGS; i++)); do [ "${STATE[i]}" = local ] && STATE[i]=done; done
elif [ "$NLOCAL" -gt 0 ]; then
    [ "$QUICK" = 1 ] && [ "$QUICK_TESTS" != ALL ] && export QLC_TEST_ONLY="$QUICK_TESTS"
    say "testing: $NLOCAL leg(s), $PER test programs at a time each"
    TEST_PIDS=""
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = local ] || continue
        ( t0=$(date +%s); run_check $i parallel; echo "$? $(( $(date +%s) - t0 ))" > "$LOGS/${LEG_NAME[i]}.test" ) &
        TEST_PIDS="$TEST_PIDS $!"
    done
    wait $TEST_PIDS
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = local ] || continue
        read rc secs < "$LOGS/${LEG_NAME[i]}.test" || { rc=1; secs=0; }
        T_TEST[i]=$secs
        [ "$rc" -eq 0 ] || RESULT[i]="FAIL(check)"
        say "${LEG_NAME[i]}: tests $([ "$rc" -eq 0 ] && echo passed || echo FAILED) (${secs}s)"
    done
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = local ] || continue
        t0=$(date +%s)
        if run_check $i serial; then ok=passed; else ok=FAILED; RESULT[i]="FAIL(check)"; fi
        T_ALONE[i]=$(( $(date +%s) - t0 ))
        say "${LEG_NAME[i]}: timing-sensitive tests alone $ok (${T_ALONE[i]}s)"
        STATE[i]=done
    done
fi
# How many test programs a leg must run: one per test.sh in the tree
# (engine, UI, and the plugins unittest.sh runs; Velleman not on macOS). A
# leg that runs fewer -- none found, a group gone missing -- fails, however
# its programs did: "the tests passed" means nothing about tests not run.
EXPECTED=$(cd "$REPO" && { ls engine/test/*/test.sh ui/test/*/test.sh
    ls plugins/{enttecwing,midi,artnet,E1.31,wled}/test/test.sh
    [ "$(uname)" = Darwin ] && ls plugins/hid/test/test.sh
    [ "$(uname)" = Darwin ] || ls plugins/velleman/test/test.sh; } 2>/dev/null | wc -l | tr -d ' ')
for ((i = 0; i < NLEGS; i++)); do
    if [ "${STATE[i]}" = done ]; then
        # "Test programs run: 99 engine, 77 of 78 UI, 5 plugin" per phase, summed.
        NTESTS[i]=$(grep -E "^Test programs run:" "$(log_of $i)" \
            | awk '{ n += $4 + $6 + $10 } END { print n + 0 }')
        if [ "$QUICK" != 1 ] && [ "${NTESTS[i]}" -lt "$EXPECTED" ]; then
            say "${LEG_NAME[i]}: ran ${NTESTS[i]} test programs of the $EXPECTED in the tree. This is not a pass."
            [ -z "${RESULT[i]}" ] && RESULT[i]="FAIL(count)"
        fi
    fi
    if [ "${STATE[i]}" = done ] && [ -z "${RESULT[i]}" ]; then RESULT[i]=PASS; fi
done

#############################################################################
# --spread: wait for the remote legs
#############################################################################

if [ -n "$REMOTE_PIDS" ]; then
    say "waiting for the remote legs"
    wait $REMOTE_PIDS
    for ((i = 0; i < NLEGS; i++)); do
        [ "${STATE[i]}" = remote ] || continue
        read rc secs < "$LOGS/${LEG_NAME[i]}.remote" || { rc=255; secs=0; }
        T_TEST[i]=$secs
        # The remote gate's own summary line for the leg: "... tests  184 PASS".
        NTESTS[i]=$(grep -E "^  ${LEG_NAME[i]} " "$(log_of $i)" | tail -n 1 | sed -n 's/.* tests *\([0-9]*\)[/ ].*/\1/p')
        if [ "$rc" -eq 0 ]; then RESULT[i]=PASS
        elif [ "$rc" -eq 255 ]; then RESULT[i]="FAIL(ssh ${HOST[i]})"
        else RESULT[i]="FAIL(on ${HOST[i]})"; fi
        say "${LEG_NAME[i]} on ${HOST[i]}: ${RESULT[i]} (${secs}s)"
    done
fi

#############################################################################
# Summary
#############################################################################

FAILED=""
SKIPPED=""
for ((i = 0; i < NLEGS; i++)); do
    case "${RESULT[i]}" in
        PASS|"") ;;
        SKIP) SKIPPED="$SKIPPED ${LEG_NAME[i]}" ;;
        FAIL*) FAILED="$FAILED ${LEG_NAME[i]}${RESULT[i]#FAIL}" ;;
    esac
done

# What failed, from each failing leg's log: the compiler's errors, or the
# test runner's failure report (each failed program's log).
for ((i = 0; i < NLEGS; i++)); do
    case "${RESULT[i]}" in FAIL*) ;; *) continue ;; esac
    log="$(log_of $i)"
    echo
    echo "==================== ${LEG_NAME[i]}: ${RESULT[i]} -- $log"
    case "${RESULT[i]}" in
        "FAIL(build)"|"FAIL(configure)")
            grep -nE "error:|Error [0-9]+|CMake Error" -A 3 "$log" | head -n 120
            echo "[... last 30 lines ...]"; tail -n 30 "$log" ;;
        *)
            if grep -qE "^==================== FAILED:|^FAILED test programs" "$log"; then
                awk '/^==================== FAILED:/ { on = 1 } on { print } /^Unit tests failed/ { on = 0 }' "$log" | head -n 600
            fi
            # What stopped a run before (or instead of) its tests: the
            # fixture check, staging, a count that came up short.
            grep -nE "not valid|not a pass|Traceback|Error:|Could not copy|No test programs|ran [0-9]+ test programs of" -B 3 -A 1 "$log" | head -n 60
            grep -qE "^==================== FAILED:" "$log" || { echo "[... last 20 lines ...]"; tail -n 20 "$log"; } ;;
    esac
done

GATE=0
[ -n "$FAILED" ] && GATE=1
if [ -n "$SKIPPED" ] && [ -z "${ALLOW_SKIP:-}" ]; then GATE=1; fi
NRUN=0
for ((i = 0; i < NLEGS; i++)); do
    case "${RESULT[i]}" in PASS|FAIL*) NRUN=$((NRUN + 1)) ;; esac
done
# Nothing ran, nothing passed -- ALLOW_SKIP or not.
[ "$NRUN" -eq 0 ] && GATE=1

{
    echo "gate=$GATE host=$GATE_HOST qt=$QT_VERSION rev=$GATE_REV"
    echo "Timings ($JOBS cores, $CCACHE_STATE$([ "$QUICK" = 1 ] && echo ", QUICK"); the legs overlap, so the columns don't add up):"
    for ((i = 0; i < NLEGS; i++)); do
        [ -n "${RESULT[i]}" ] || continue
        if [ "${RESULT[i]}" = SKIP ]; then
            printf '  %-12s skipped\n' "${LEG_NAME[i]}"
        elif [ "${STATE[i]}" = remote ]; then
            printf '  %-12s on %-14s %5ss   tests %4s   %s\n' "${LEG_NAME[i]}" "${HOST[i]}" "${T_TEST[i]}" "${NTESTS[i]:-?}" "${RESULT[i]}"
        else
            printf '  %-12s configure %3ss   build %4ss   test %4ss   alone %3ss   tests %4s/%s   %s\n' \
                "${LEG_NAME[i]}" "${T_CONF[i]}" "${T_BUILD[i]}" "${T_TEST[i]}" "${T_ALONE[i]}" "${NTESTS[i]:-0}" "$EXPECTED" "${RESULT[i]}"
        fi
    done
    echo "  whole gate $(( $(date +%s) - GATE_START ))s"
    [ -n "$CC_LINE" ] && echo "  $CC_LINE"
    [ "$QUICK" = 1 ] && echo "QUICK: the Qt6 leg only$([ -n "$QUICK_TESTS" ] && [ "$QUICK_TESTS" != ALL ] && echo ", tests: $QUICK_TESTS") -- not the gate."
    [ -n "$SKIPPED" ] && echo "SKIPPED:$SKIPPED (Qt not at $QT6)$([ -n "${ALLOW_SKIP:-}" ] && echo " -- allowed by ALLOW_SKIP" || echo " -- a skipped leg fails the gate; ALLOW_SKIP=1 to accept")"
    if [ -n "$FAILED" ]; then
        echo "FAILED:$FAILED"
    elif [ "$NRUN" -eq 0 ]; then
        echo "No leg ran: this is not a pass."
    elif [ "$GATE" -eq 0 ] && [ -n "$SKIPPED" ]; then
        echo "All configurations that ran passed."
    elif [ "$GATE" -eq 0 ]; then
        echo "All configurations passed."
    fi
} > "$LOGS/summary.txt"
echo
cat "$LOGS/summary.txt"
exit $GATE
