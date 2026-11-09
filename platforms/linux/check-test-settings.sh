#!/bin/bash
#
# No test may read or write the person's real preferences. On macOS a QSettings
# in the native format is ~/Library/Preferences/org.qlcplus.qlcconsole.plist
# whatever HOME says, so a test that set Settings > Units to metres left every
# gate Mac's real app reading metres (2026-11-08). Two walls:
#
#  - every test program carries ui/test/common/testsettings.cpp, which points
#    QSettings at a temporary INI folder before main(). The top-level
#    CMakeLists.txt adds it to every program under a test/ dir. Its mark in a binary: QLC_TEST_SETTINGS_ISOLATED.
#  - platforms/linux/unittest.sh on macOS compares `defaults export` of the
#    app's domain before and after the run (--defaults-before / --defaults-after).
#
#   check-test-settings.sh --source REPO          the CMake wiring is in place
#   check-test-settings.sh --build DIR [TEST...]  the programs each test.sh runs
#                                                 (default: all) carry the mark
#   check-test-settings.sh --defaults-before FILE   snapshot the real prefs
#   check-test-settings.sh --defaults-after FILE    fail with the diff if changed
#   check-test-settings.sh --self-test            this script's own test
#
# check-all.sh runs --self-test and --source before building; unittest.sh runs
# --build and the defaults pair.

MARK=QLC_TEST_SETTINGS_ISOLATED
DOMAINS="${QLC_PREFS_DOMAINS:-org.qlcplus.qlcconsole org.qlcplus.QLCPlus org.qlcplus.qlcplus}"

is_program()
{
    [ -f "$1" ] && [ -x "$1" ] || return 1
    [ "$(head -c 2 "$1" 2>/dev/null)" = "#!" ] && return 1
    return 0
}

programs_of()
{
    local dir="$1" p cand base
    for p in $(sed -e '/^[[:space:]]*#/d' "$dir/test.sh" \
               | grep -oE '\.\.?/[A-Za-z0-9_./+-]+' | sort -u); do
        for cand in "$dir/$p" "$dir/$p.exe"; do
            is_program "$cand" || continue
            base=$(basename "$cand")
            case "$cand" in
                *.app/Contents/MacOS/*) ;;
                *) is_program "$cand.app/Contents/MacOS/$base" && continue ;;
            esac
            echo "$cand"
        done
    done
}

check_source()
{
    local root="$1" bad=0
    [ -f "$root/ui/test/common/testsettings.cpp" ] || { echo "check-test-settings: no ui/test/common/testsettings.cpp in $root"; return 1; }
    grep -q "$MARK" "$root/ui/test/common/testsettings.cpp" \
        || { echo "check-test-settings: testsettings.cpp does not set $MARK"; bad=1; }
    grep -q 'PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui/test/common/testsettings.cpp' "$root/CMakeLists.txt" \
        || { echo "check-test-settings: the top-level CMakeLists.txt no longer adds qlc_test_settings to every test program"; bad=1; }
    [ "$bad" -eq 0 ] && echo "check-test-settings: the CMake wiring is in place."
    return "$bad"
}

check_build()
{
    local dir="$1" t prog progs bad=0 n=0 np=0
    shift
    local tests="$*"
    if [ -z "$tests" ]; then
        tests=$(cd "$dir" && find . -path '*/CMakeFiles' -prune -o -path '*/test/*' -name test.sh -print \
                | sed -e 's|^\./||' -e 's|/test.sh$||' | sort)
    fi
    for t in $tests; do
        n=$((n + 1))
        if [ ! -f "$dir/$t/test.sh" ]; then
            echo "$t: no test.sh in $dir"
            bad=$((bad + 1))
            continue
        fi
        progs=$(programs_of "$dir/$t")
        if [ -z "$progs" ]; then
            grep -q "Nothing to test here" "$dir/$t/test.sh" && continue
            echo "$t: found no program its test.sh runs, so can't tell it has isolated settings."
            bad=$((bad + 1))
            continue
        fi
        for prog in $progs; do
            np=$((np + 1))
            if ! LC_ALL=C grep -qaF "$MARK" "$prog"; then
                echo "$prog: does not isolate QSettings (ui/test/common/testsettings.cpp; the"
                echo "    top-level CMakeLists.txt adds it to programs under a test/ dir)."
                bad=$((bad + 1))
            fi
        done
    done
    if [ "$bad" -gt 0 ]; then
        echo "check-test-settings: $bad problem(s) in $n test dirs. A test could change the real preferences."
        return 1
    fi
    echo "check-test-settings: $n test dirs, $np programs, all on settings of their own."
    return 0
}

# What `defaults` says about each domain; nothing (and a clean skip) where
# there is no `defaults`. A domain with no preferences yet exports as nothing.
snapshot()
{
    local d
    command -v defaults >/dev/null 2>&1 || return 1
    for d in $DOMAINS; do
        echo "## $d"
        defaults export "$d" - 2>/dev/null
    done
    return 0
}

defaults_before()
{
    snapshot > "$1" || { echo "check-test-settings: no 'defaults' here; real-preferences check skipped."; rm -f "$1"; return 0; }
    return 0
}

defaults_after()
{
    local before="$1" now
    [ -f "$before" ] || return 0     # skipped before
    now="$before.after"
    snapshot > "$now" || { rm -f "$now"; return 0; }
    if ! diff -u "$before" "$now" > "$now.diff"; then
        echo "check-test-settings: THE TESTS CHANGED THE REAL PREFERENCES (defaults export, before -> after):"
        cat "$now.diff"
        echo "A test program writes QSettings without ui/test/common/testsettings.cpp, or to a domain of its own."
        rm -f "$now" "$now.diff"
        return 1
    fi
    rm -f "$now" "$now.diff"
    echo "check-test-settings: the real preferences are unchanged by the run."
    return 0
}

self_test()
{
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/check-test-settings.XXXXXX") || return 1
    expect()
    {
        local want="$1" what="$2" rc
        shift 2
        out=$("$@" 2>&1); rc=$?
        [ "$rc" -ne 0 ] && rc=1
        if [ "$rc" -ne "$want" ]; then
            echo "self-test FAILED: $what (exit $rc, wanted $want)"
            echo "$out" | sed 's/^/    /'
            fails=$((fails + 1))
        fi
    }
    fakeprog() { mkdir -p "$(dirname "$1")"; printf '\177ELF fake %s\n' "${2:-}" > "$1"; chmod +x "$1"; }

    mkdir -p "$tmp/src/ui/test/common"
    printf '// %s\n' "$MARK" > "$tmp/src/ui/test/common/testsettings.cpp"
    printf 'target_sources(x PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui/test/common/testsettings.cpp)\n' > "$tmp/src/CMakeLists.txt"
    expect 0 "source: wired" check_source "$tmp/src"
    printf 'nothing\n' > "$tmp/src/CMakeLists.txt"
    expect 1 "source: CMake wiring gone" check_source "$tmp/src"
    printf 'target_sources(x PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui/test/common/testsettings.cpp)\n' > "$tmp/src/CMakeLists.txt"
    printf '// nothing\n' > "$tmp/src/ui/test/common/testsettings.cpp"
    expect 1 "source: mark gone" check_source "$tmp/src"

    local b="$tmp/build"
    mkdir -p "$b/engine/test/a" "$b/plugins/p/test" "$b/engine/test/none"
    printf '#!/bin/sh\n./a_test\n' > "$b/engine/test/a/test.sh"
    fakeprog "$b/engine/test/a/a_test" "$MARK"
    printf '#!/bin/sh\nexec ../../p_test\n' > "$b/plugins/p/test/test.sh"
    fakeprog "$b/plugins/p_test" "$MARK"
    printf '#!/bin/sh\necho "Nothing to test here."\n' > "$b/engine/test/none/test.sh"
    expect 0 "build: all isolated" check_build "$b"
    fakeprog "$b/plugins/p_test"
    expect 1 "build: a plugin test without the mark" check_build "$b"
    expect 0 "build: a selection that skips it" check_build "$b" engine/test/a
    expect 1 "build: a test dir that isn't there" check_build "$b" engine/test/missing

    # Defaults: a fake `defaults` whose answer we change between the two snapshots.
    mkdir -p "$tmp/bin"
    printf '#!/bin/sh\ncat "%s/state" 2>/dev/null\n' "$tmp" > "$tmp/bin/defaults"
    chmod +x "$tmp/bin/defaults"
    echo "units.length=feet" > "$tmp/state"
    (
        export PATH="$tmp/bin:$PATH" QLC_PREFS_DOMAINS="org.test.a"
        defaults_before "$tmp/snap" >/dev/null
        out=$(defaults_after "$tmp/snap" 2>&1) || { echo "self-test FAILED: unchanged prefs flagged"; exit 1; }
        defaults_before "$tmp/snap" >/dev/null
        echo "units.length=metres" > "$tmp/state"
        out=$(defaults_after "$tmp/snap" 2>&1) && { echo "self-test FAILED: changed prefs not caught"; exit 1; }
        echo "$out" | grep -q 'metres' || { echo "self-test FAILED: no diff shown"; exit 1; }
        exit 0
    ) || fails=$((fails + 1))
    # No `defaults` at all: skipped cleanly (an empty PATH dir keeps the shell's builtins only).
    mkdir -p "$tmp/bin2"
    for c in rm diff cat; do ln -s "$(command -v $c)" "$tmp/bin2/$c"; done
    (
        export PATH="$tmp/bin2"
        defaults_before "$tmp/snap2" >/dev/null && defaults_after "$tmp/snap2" >/dev/null
    ) || { echo "self-test FAILED: no-defaults host did not skip cleanly"; fails=$((fails + 1)); }

    rm -rf "$tmp"
    if [ "$fails" -gt 0 ]; then
        echo "check-test-settings: self-test: $fails case(s) failed."
        return 1
    fi
    echo "check-test-settings: self-test passed."
    return 0
}

case "${1:-}" in
    --source)          [ -n "${2:-}" ] || { echo >&2 "usage: $0 --source REPO"; exit 2; }
                       check_source "$2" ;;
    --build)           [ -n "${2:-}" ] || { echo >&2 "usage: $0 --build DIR [TEST...]"; exit 2; }
                       shift; check_build "$@" ;;
    --defaults-before) [ -n "${2:-}" ] || exit 2; defaults_before "$2" ;;
    --defaults-after)  [ -n "${2:-}" ] || exit 2; defaults_after "$2" ;;
    --self-test)       self_test ;;
    *)                 echo >&2 "usage: $0 --source REPO | --build DIR [TEST...] | --defaults-before F | --defaults-after F | --self-test"; exit 2 ;;
esac
