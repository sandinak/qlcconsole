#!/bin/bash
#
# No test may open a window on the real screen, or leave a crash dialog there.
# Two walls, both checked here (the gate fails on either):
#
#  - every test.sh, before it runs anything, has
#        [ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
#  - every test program carries engine/test/common/testguard.cpp (the
#    top-level CMakeLists.txt links it into each one), which does the same
#    before main() and keeps a crash from raising macOS's "quit
#    unexpectedly" dialog. Its mark in a binary: QLC_TEST_OFFSCREEN_GUARD.
#
# QLC_TEST_REAL_SCREEN=1 in the environment lifts both, to watch a test on
# purpose.
#
#   check-test-guard.sh --source REPO          every */test/**/test.sh in the tree
#   check-test-guard.sh --build DIR [TEST...]  in a build dir: those test dirs'
#                                              (default: all) test.sh, and the
#                                              programs they run
#   check-test-guard.sh --self-test            this script's own test
#
# check-all.sh runs --self-test and --source before building;
# platforms/linux/unittest.sh runs --build before the tests.

GUARD='[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen'
MARK=QLC_TEST_OFFSCREEN_GUARD

# The guard must come before anything else the script does: the first line
# that is neither the #! line, a comment nor blank.
check_script()
{
    local f="$1" first
    first=$(sed -e '1{/^#!/d;}' -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' "$f" | head -n 1)
    if [ "$first" != "$GUARD" ]; then
        echo "$f: does not start with the offscreen guard. Its first command must be"
        echo "    $GUARD"
        return 1
    fi
    return 0
}

is_program()
{
    [ -f "$1" ] && [ -x "$1" ] || return 1
    [ "$(head -c 2 "$1" 2>/dev/null)" = "#!" ] && return 1
    return 0
}

# The programs a test dir's test.sh runs: every ./ or ../ path it names that
# is a program (a .exe beside a name counts, for Windows), preferring a
# macOS bundle's binary over a stale plain one of the same name.
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
    local root="$1" f bad=0 n=0
    for f in $(cd "$root" && find . \( -name 'build*' -o -name dist -o -name .git \) -prune \
                   -o -path '*/test/*' -name test.sh -print | sort); do
        n=$((n + 1))
        check_script "$root/$f" || bad=$((bad + 1))
    done
    if [ "$n" -eq 0 ]; then
        echo "check-test-guard: no test.sh under $root"
        return 1
    fi
    if [ "$bad" -gt 0 ]; then
        echo "check-test-guard: $bad of $n test.sh lack the offscreen guard."
        return 1
    fi
    echo "check-test-guard: all $n test.sh start offscreen."
    return 0
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
        check_script "$dir/$t/test.sh" || { bad=$((bad + 1)); continue; }
        progs=$(programs_of "$dir/$t")
        if [ -z "$progs" ]; then
            grep -q "Nothing to test here" "$dir/$t/test.sh" && continue
            echo "$t: found no program its test.sh runs, so can't tell it has the test guard."
            bad=$((bad + 1))
            continue
        fi
        for prog in $progs; do
            np=$((np + 1))
            if ! LC_ALL=C grep -qaF "$MARK" "$prog"; then
                echo "$prog: lacks the test guard (engine/test/common/testguard.cpp; the"
                echo "    top-level CMakeLists.txt adds it to programs under a test/ dir)."
                bad=$((bad + 1))
            fi
        done
    done
    if [ "$bad" -gt 0 ]; then
        echo "check-test-guard: $bad problem(s) in $n test dirs. A test could open a window on the screen."
        return 1
    fi
    echo "check-test-guard: $n test dirs, $np programs, all offscreen-guarded."
    return 0
}

self_test()
{
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/check-test-guard.XXXXXX") || return 1
    # expect <0|1> <description> <command...>
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
    fakeprog() # <path> [mark]
    {
        mkdir -p "$(dirname "$1")"
        printf '\177ELF fake program %s\n' "${2:-}" > "$1"
        chmod +x "$1"
    }

    # Source: a good tree passes; a missing guard, or one after a command, fails.
    mkdir -p "$tmp/src/engine/test/a" "$tmp/src/ui/test/b" "$tmp/src/build/ui/test/c"
    printf '#!/bin/sh\n# comment\n%s\nexec ./a_test\n' "$GUARD" > "$tmp/src/engine/test/a/test.sh"
    printf '#!/bin/bash\n\n%s\n./b_test\n' "$GUARD" > "$tmp/src/ui/test/b/test.sh"
    printf '#!/bin/sh\n./c_test\n' > "$tmp/src/build/ui/test/c/test.sh"   # build dirs are skipped
    expect 0 "source: guarded tree" check_source "$tmp/src"
    printf '#!/bin/sh\nexport LD_LIBRARY_PATH=x\n%s\n./b_test\n' "$GUARD" > "$tmp/src/ui/test/b/test.sh"
    expect 1 "source: guard after a command" check_source "$tmp/src"
    printf '#!/bin/sh\n./b_test\n' > "$tmp/src/ui/test/b/test.sh"
    expect 1 "source: no guard" check_source "$tmp/src"
    printf '#!/bin/sh\nexport QT_QPA_PLATFORM=cocoa\n./b_test\n' > "$tmp/src/ui/test/b/test.sh"
    expect 1 "source: another platform" check_source "$tmp/src"
    expect 1 "source: an empty tree" check_source "$tmp/src/ui/test/b/nothing"

    # Build: each test.sh guarded and each program it runs marked.
    local b="$tmp/build"
    mkdir -p "$b/engine/test/a" "$b/ui/test/w" "$b/plugins/p/test" "$b/engine/test/none"
    printf '#!/bin/sh\n%s\nexport LD_LIBRARY_PATH=../../src\n./a_test\n' "$GUARD" > "$b/engine/test/a/test.sh"
    fakeprog "$b/engine/test/a/a_test" "$MARK"
    printf '#!/bin/sh\n%s\nif [ -x ./w_test.app/Contents/MacOS/w_test ]; then\n  exec ./w_test.app/Contents/MacOS/w_test\nelse\n  exec ./w_test\nfi\n' \
        "$GUARD" > "$b/ui/test/w/test.sh"
    fakeprog "$b/ui/test/w/w_test.app/Contents/MacOS/w_test" "$MARK"
    fakeprog "$b/ui/test/w/w_test"                    # stale plain build: not what runs
    printf '#!/bin/sh\n%s\nexec ../../p_test\n' "$GUARD" > "$b/plugins/p/test/test.sh"
    fakeprog "$b/plugins/p_test" "$MARK"
    printf '#!/bin/bash\n%s\necho "Nothing to test here."\n' "$GUARD" > "$b/engine/test/none/test.sh"
    expect 0 "build: all guarded" check_build "$b"
    expect 0 "build: a selection" check_build "$b" engine/test/a plugins/p/test
    fakeprog "$b/plugins/p_test"
    expect 1 "build: a flat plugin program without the mark" check_build "$b"
    fakeprog "$b/plugins/p_test" "$MARK"
    fakeprog "$b/ui/test/w/w_test.app/Contents/MacOS/w_test"
    expect 1 "build: a bundle without the mark" check_build "$b"
    fakeprog "$b/ui/test/w/w_test.app/Contents/MacOS/w_test" "$MARK"
    rm -f "$b/engine/test/a/a_test"
    fakeprog "$b/engine/test/a/a_test.exe"
    expect 1 "build: a Windows program without the mark" check_build "$b"
    rm -f "$b/engine/test/a/a_test.exe"
    expect 1 "build: no program to check" check_build "$b"
    fakeprog "$b/engine/test/a/a_test" "$MARK"
    printf '#!/bin/sh\n./a_test\n' > "$b/engine/test/a/test.sh"
    expect 1 "build: a test.sh without the guard" check_build "$b"
    expect 1 "build: a test dir that isn't there" check_build "$b" engine/test/missing

    rm -rf "$tmp"
    if [ "$fails" -gt 0 ]; then
        echo "check-test-guard: self-test: $fails case(s) failed."
        return 1
    fi
    echo "check-test-guard: self-test passed."
    return 0
}

case "${1:-}" in
    --source)    [ -n "${2:-}" ] || { echo >&2 "usage: $0 --source REPO"; exit 2; }
                 check_source "$2" ;;
    --build)     [ -n "${2:-}" ] || { echo >&2 "usage: $0 --build DIR [TEST...]"; exit 2; }
                 shift; check_build "$@" ;;
    --self-test) self_test ;;
    *)           echo >&2 "usage: $0 --source REPO | --build DIR [TEST...] | --self-test"; exit 2 ;;
esac
