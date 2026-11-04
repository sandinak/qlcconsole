#!/bin/bash
# One gate at a time per host: the pool's lock (platforms/macos/gate-pool.sh).
#
# Two gates on one Mac share its cores: on val (2026-10-29) pairs of them
# took 15-20 min instead of ~10, at load ~60, and a slow test ran past Qt's
# watchdog -- 8 gates of 10 failed that passed on newaudio. So gate-pool.sh
# takes this lock on the host before it copies anything there, and WAITS
# for it rather than failing.
#
# The functions below are sent to the host (`cat gate-lock.sh; echo gl_try
# ...` into its bash) and run there, on ~/gates:
#
#   ~/gates/.gatelock/info   the lock: id=, sha=, since= (epoch), from=
#                            (mkdir is the atomic take)
#   ~/gates/.vallock         the agents' advisory lock (mkdir by hand):
#                            honoured as held
#   ~/gates/<id>.running     a gate's marker -- also a pool from before this
#                            lock, which never takes it: honoured as held
#   ~/gates/<id>.pid         that gate's detached process
#
# Stale, and so not held: the lock or a marker whose gate's .pid is dead;
# one with no .pid at all after GL_GRACE seconds (default 900: the copy
# step, before the gate starts, has no .pid yet -- a gate that died there
# left one forever); anything older than GL_MAX seconds (default 7200, 2 h).
# A stale lock is broken (under a second mkdir, so two waiters can't both
# break it and the second take the first's new lock); a stale marker is
# removed with its copy (~6 GB) -- its .log and .summary.txt stay. A stale
# .vallock is ignored but left: its agent rmdirs it.
#
#   gl_holder [OWN-ID]   print what holds the host, one line each ("note:"
#                        lines: stale ones cleared); nothing = free
#   gl_try ID SHA [FROM] take the lock (FROM: the Mac that asks): last line
#                        "@@ok", or "@@busy WHAT"
#   gl_need              the host has the Linux leg's toolchain (GL_NEED words)
#   gl_release ID        drop the lock if ID holds it and its gate is not
#                        running (a gate still running keeps it; it drops
#                        it itself when done: gl_done)
#   gl_done ID           drop the lock if ID holds it (the gate's own end)
#
#   gl_reap              kill the orphans of dead gates (below); run by
#                        every gl_holder that cleans, and by the pool's probe
#
#   platforms/macos/gate-lock.sh --self-test    the lock's own test, on a
#                                               scratch ~/gates (seconds)
#
# Orphans (2026-11-01): a gate that was killed leaves its test programs
# running when the kill didn't reach their process group -- a *_test that
# spins at 100% for days, on a host the next gate then shares. So every host
# a pool run touches also gets: kill each *_test process (the user's own)
# whose executable is under ~/gates/<id>/ when gate <id> is dead (its .pid
# not running, or none) and the process is older than GL_REAP_AGE seconds
# (default 3600, 1 h). Nothing else is ever killed: not the app, not a
# running gate's tests, not anything outside ~/gates, not a program whose
# name doesn't end in _test. Each kill is a line in ~/gates/cleanup.log.

GL_MAX="${GL_MAX:-7200}"
GL_GRACE="${GL_GRACE:-900}"
GL_CLEAN="${GL_CLEAN:-1}"
GL_REAP_AGE="${GL_REAP_AGE:-3600}"

gl_g() { echo "$HOME/gates"; }
gl_now() { date +%s; }
gl_mtime() { stat -c %Y "$1" 2>/dev/null || stat -f %m "$1" 2>/dev/null || echo 0; }
gl_clock() { date -r "$1" '+%H:%M' 2>/dev/null || date -d "@$1" '+%H:%M' 2>/dev/null || echo "?"; }

# Whether gate ID's process is alive (its .pid there and running).
gl_alive() {
    local p
    p=$(cat "$(gl_g)/$1.pid" 2>/dev/null) && [ -n "$p" ] && kill -0 "$p" 2>/dev/null
}

# Whether gate ID, held since SINCE (epoch), is stale.
gl_stale() {
    local id=$1 since=$2 age
    age=$(( $(gl_now) - since ))
    [ "$age" -gt "$GL_MAX" ] && return 0
    if [ -e "$(gl_g)/$id.pid" ]; then
        gl_alive "$id" && return 1
        return 0
    fi
    [ "$age" -gt "$GL_GRACE" ]
}

gl_info() { # <key>: from the lock's info
    sed -n "s/^$1=//p" "$(gl_g)/.gatelock/info" 2>/dev/null | head -n 1
}

# ps's elapsed time ([[dd-]hh:]mm:ss) in seconds.
gl_secs() {
    awk -v t="$1" 'BEGIN { d = 0; if (split(t, a, "-") == 2) { d = a[1]; t = a[2] }
        n = split(t, b, ":"); s = 0; for (i = 1; i <= n; i++) s = s * 60 + b[i]; print d * 86400 + s }'
}

# The path of process PID's executable (Linux /proc, else lsof's first txt).
gl_exe() {
    local e
    if e=$(readlink "/proc/$1/exe" 2>/dev/null) && [ -n "$e" ]; then
        printf '%s\n' "${e% (deleted)}"
    else
        lsof -a -p "$1" -d txt -Fn 2>/dev/null | sed -n 's/^n//p' | head -n 1
    fi
}

# Kill the *_test processes of dead gates (see the header); log each.
gl_reap() {
    [ "$GL_CLEAN" = 1 ] || return 0
    local G Gr pid et args exe rel id age
    G=$(gl_g)
    [ -d "$G" ] || return 0
    Gr=$(cd "$G" && pwd -P)
    ps -u "$(id -u)" -o pid= -o etime= -o args= 2>/dev/null | while read -r pid et args; do
        case "${args%% *}" in *_test) ;; *) continue ;; esac
        [ "$pid" = "$$" ] && continue
        age=$(gl_secs "$et")
        [ "$age" -ge "$GL_REAP_AGE" ] || continue
        exe=$(gl_exe "$pid")
        case "$exe" in
            "$G"/*) rel=${exe#"$G"/} ;;
            "$Gr"/*) rel=${exe#"$Gr"/} ;;
            *) continue ;;
        esac
        id=${rel%%/*}
        case "$id" in ''|.*|*/*) continue ;; esac
        gl_alive "$id" && continue
        kill -TERM "$pid" 2>/dev/null
        sleep 1
        kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null
        echo "$(date '+%Y-%m-%d %H:%M:%S') killed pid $pid ${exe##*/} (${age}s old, gate $id is dead): $exe" >> "$G/cleanup.log"
        echo "note: killed ${exe##*/} (pid $pid, ${age}s old) left by dead gate $id"
    done
}

gl_holder() {
    gl_reap
    local own="${1:-}" G id sha since f
    G=$(gl_g)
    if [ -d "$G/.gatelock" ]; then
        id=$(gl_info id); sha=$(gl_info sha); since=$(gl_info since)
        # No info yet: being taken this instant, or a take that died.
        [ -n "$since" ] || since=$(gl_mtime "$G/.gatelock")
        if [ -n "$own" ] && [ "$id" = "$own" ]; then
            :
        elif gl_stale "${id:-none}" "$since"; then
            if [ "$GL_CLEAN" = 1 ]; then
                if [ -d "$G/.gatelock.break" ] && [ $(( $(gl_now) - $(gl_mtime "$G/.gatelock.break") )) -gt 60 ]; then
                    rmdir "$G/.gatelock.break" 2>/dev/null
                fi
                if mkdir "$G/.gatelock.break" 2>/dev/null; then
                    # Still the same stale holder? (Another waiter may have
                    # broken it and taken it meanwhile.)
                    if [ "$(gl_info id)" = "$id" ]; then
                        rm -rf "$G/.gatelock"
                        [ -n "$id" ] && ! gl_alive "$id" && rm -f "$G/$id.pid"
                        echo "note: broke the stale lock of gate ${id:-?} (since $(gl_clock "$since"))"
                    fi
                    rmdir "$G/.gatelock.break"
                else
                    echo "gate lock being broken"
                fi
            fi
        else
            echo "gate ${sha:0:12} running since $(gl_clock "$since")"
        fi
    fi
    if [ -d "$G/.vallock" ]; then
        since=$(gl_mtime "$G/.vallock")
        if [ $(( $(gl_now) - since )) -le "$GL_MAX" ]; then
            echo "~/gates/.vallock held since $(gl_clock "$since")"
        fi
    fi
    for f in "$G"/*.running; do
        [ -e "$f" ] || continue
        id=$(basename "$f" .running)
        [ -n "$own" ] && [ "$id" = "$own" ] && continue
        since=$(gl_mtime "$f")
        if gl_stale "$id" "$since"; then
            if [ "$GL_CLEAN" = 1 ]; then
                rm -rf "${G:?}/$id" "$G/$id.running" "$G/$id.sh" "$G/$id.pid"
                echo "note: removed the stale marker of gate $id (since $(gl_clock "$since"))"
            fi
        else
            echo "gate ${id:0:12} running since $(gl_clock "$since")"
        fi
    done
}

# Whether this host has the toolchain the Linux leg builds with: every word
# of GL_NEED, where a|b means either (default: cmake ninja ccache g++
# qtpaths6|qmake6). Prints "@@notool missing X" and fails when not.
gl_need() {
    local w alt ok
    for w in ${GL_NEED:-cmake ninja ccache g++ qtpaths6|qmake6}; do
        ok=
        for alt in $(printf '%s' "$w" | tr '|' ' '); do
            command -v "$alt" >/dev/null 2>&1 && ok=1
        done
        [ -n "$ok" ] || { echo "@@notool missing $w"; return 1; }
    done
    return 0
}

gl_try() {
    local id=$1 sha=$2 from=${3:-} G held
    G=$(gl_g)
    mkdir -p "$G"
    held=$(gl_holder "$id")
    printf '%s\n' "$held" | grep '^note:'
    held=$(printf '%s\n' "$held" | grep -v '^note:' | grep . | head -n 1)
    if [ -n "$held" ]; then
        echo "@@busy $held"
        return 1
    fi
    if [ "$(gl_info id)" = "$id" ]; then
        echo "@@ok"
        return 0
    fi
    if mkdir "$G/.gatelock" 2>/dev/null; then
        printf 'id=%s\nsha=%s\nsince=%s\nfrom=%s\n' "$id" "$sha" "$(gl_now)" "${from:-$(hostname -s 2>/dev/null)}" \
            > "$G/.gatelock/info.$$" && mv "$G/.gatelock/info.$$" "$G/.gatelock/info"
        echo "@@ok"
        return 0
    fi
    echo "@@busy gate lock just taken"
    return 1
}

gl_release() {
    [ "$(gl_info id)" = "$1" ] || return 0
    gl_alive "$1" && return 0
    rm -rf "$(gl_g)/.gatelock"
}

gl_done() {
    [ "$(gl_info id)" = "$1" ] && rm -rf "$(gl_g)/.gatelock"
    return 0
}

#############################################################################
# --self-test
#############################################################################

gl_selftest() {
    local fails=0 out T
    T=$(mktemp -d "${TMPDIR:-/tmp}/gate-lock-test.XXXXXX") || return 1
    # A scratch home: nothing here touches the real ~/gates.
    local HOME="$T"
    local G="$T/gates"
    check() { # <what> <cmd...>
        local what=$1; shift
        if "$@"; then :; else echo "FAIL: $what"; fails=$((fails + 1)); fi
    }
    last() { printf '%s\n' "$1" | tail -n 1; }

    # Free host: taken; the same gate again: still its own.
    out=$(gl_try a1 aaaaaaaaaaaa11 2>&1)
    check "free host is taken" [ "$(last "$out")" = "@@ok" ]
    check "info records the gate" [ "$(gl_info id)" = a1 ]
    out=$(gl_try a1 aaaaaaaaaaaa11 2>&1)
    check "own lock is not busy" [ "$(last "$out")" = "@@ok" ]

    # A second gate waits while the first runs (live pid).
    sleep 60 & local live=$!
    echo $live > "$G/a1.pid"
    out=$(gl_try b2 bbbbbbbbbbbb22 2>&1)
    check "second gate is busy" [ "${out#@@busy gate aaaaaaaaaaaa running since }" != "$out" ]
    check "busy leaves the lock alone" [ "$(gl_info id)" = a1 ]
    gl_release b2
    check "release by another id keeps it" [ "$(gl_info id)" = a1 ]
    gl_release a1
    check "release while its gate runs keeps it" [ "$(gl_info id)" = a1 ]

    # The gate dies: its lock is stale, the next gate breaks it and takes it.
    kill $live 2>/dev/null; wait $live 2>/dev/null
    out=$(gl_try b2 bbbbbbbbbbbb22 2>&1)
    check "dead gate's lock is broken" [ "$(last "$out")" = "@@ok" ]
    check "breaking it is said" [ "${out#note: broke the stale lock of gate a1}" != "$out" ]
    check "the new gate holds it" [ "$(gl_info id)" = b2 ]
    gl_done b2
    check "gl_done drops it" [ ! -d "$G/.gatelock" ]

    # No .pid (died in the copy step): held within the grace, stale after.
    gl_try c3 cccccccccccc33 >/dev/null
    out=$(gl_try d4 dddddddddddd44 2>&1)
    check "pid-less lock within grace is held" [ "${out#@@busy}" != "$out" ]
    sed -i.bak "s/^since=.*/since=$(( $(gl_now) - 1000 ))/" "$G/.gatelock/info"
    out=$(gl_try d4 dddddddddddd44 2>&1)
    check "pid-less lock past grace is stale" [ "$(last "$out")" = "@@ok" ]
    gl_done d4

    # Older than GL_MAX: stale even with a live pid.
    sleep 60 & live=$!
    gl_try e5 eeeeeeeeeeee55 >/dev/null
    echo $live > "$G/e5.pid"
    sed -i.bak "s/^since=.*/since=$(( $(gl_now) - 8000 ))/" "$G/.gatelock/info"
    out=$(gl_try f6 ffffffffffff66 2>&1)
    check "lock past 2 h is stale" [ "$(last "$out")" = "@@ok" ]
    kill $live 2>/dev/null; wait $live 2>/dev/null
    gl_done f6

    # .vallock: held; past GL_MAX: ignored, and left.
    mkdir "$G/.vallock"
    out=$(gl_try g7 gggggggggggg77 2>&1)
    check ".vallock is held" [ "${out#@@busy ~/gates/.vallock held since}" != "$out" ]
    touch -t 202001010000 "$G/.vallock"
    out=$(gl_try g7 gggggggggggg77 2>&1)
    check "old .vallock is ignored" [ "$(last "$out")" = "@@ok" ]
    check "old .vallock is left" [ -d "$G/.vallock" ]
    rmdir "$G/.vallock"; gl_done g7

    # A .running marker from an older pool (no lock): held while its pid
    # lives; dead (or pid-less past the grace): removed with its copy.
    sleep 60 & live=$!
    mkdir -p "$G/h8"; touch "$G/h8.running"; echo $live > "$G/h8.pid"
    out=$(gl_try i9 iiiiiiiiiiii99 2>&1)
    check "live .running is held" [ "${out#@@busy gate h8 running}" != "$out" ]
    kill $live 2>/dev/null; wait $live 2>/dev/null
    out=$(gl_try i9 iiiiiiiiiiii99 2>&1)
    check "dead .running is cleared" [ "$(last "$out")" = "@@ok" ]
    check "its marker and copy are gone" [ ! -e "$G/h8.running" -a ! -d "$G/h8" ]
    gl_done i9
    mkdir -p "$G/j10"; touch "$G/j10.running"
    out=$(gl_try k11 kkkkkkkkkkkk11 2>&1)
    check "pid-less .running within grace is held" [ "${out#@@busy}" != "$out" ]
    touch -t 202001010000 "$G/j10.running"
    out=$(gl_try k11 kkkkkkkkkkkk11 2>&1)
    check "pid-less .running past grace is cleared" [ "$(last "$out")" = "@@ok" ]
    gl_done k11

    # GL_CLEAN=0 (the pool's --status probe): stale is free but nothing goes.
    mkdir -p "$G/l12"; touch -t 202001010000 "$G/l12.running"
    out=$(GL_CLEAN=0 gl_holder)
    check "status probe sees stale as free" [ -z "$out" ]
    check "status probe removes nothing" [ -e "$G/l12.running" ]

    # Two takers at once: exactly one gets it.
    rm -rf "$G"/*; mkdir -p "$G"
    local n
    for n in 1 2 3 4 5 6; do ( gl_try "race$n" "race$n" > "$T/race$n" 2>&1 ) & done
    wait
    n=$(cat "$T"/race? | grep -c '^@@ok$')
    check "one of six concurrent takers wins (got $n)" [ "$n" = 1 ]

    # Orphans (Q196): a *_test of a dead gate, over GL_REAP_AGE, under
    # ~/gates is killed and logged; nothing else is.
    mkdir -p "$G"
    # A tiny program of our own to copy and run: macOS kills a copy of a
    # system binary (sleep) outright, signed again or not.
    printf '#include <unistd.h>\nint main(void) { sleep(300); return 0; }\n' > "$T/dummy.c"
    local dummybin=""
    cc -o "$T/dummybin" "$T/dummy.c" >/dev/null 2>&1 && dummybin="$T/dummybin"
    dummy() { # <path>: a process that sleeps, named by that path
        mkdir -p "$(dirname "$1")"
        cp "$dummybin" "$1" && chmod +x "$1" || return 1
        "$1" > /dev/null 2>&1 &
        echo $!
    }
    local reapok=1 p_dead p_live p_plain p_out p_young livegate
    if [ -n "$dummybin" ] && p_dead=$(dummy "$G/deadgate/ui/x_test") && [ -n "$p_dead" ]; then
        p_plain=$(dummy "$G/deadgate/ui/plain_prog")
        p_out=$(dummy "$T/outside/z_test")
        sleep 300 & livegate=$!
        echo $livegate > "$G/livegate.pid"
        p_live=$(dummy "$G/livegate/ui/y_test")
        sleep 2
        GL_REAP_AGE=3600 gl_reap >/dev/null
        check "a young orphan is left" kill -0 "$p_dead"
        GL_REAP_AGE=1 GL_CLEAN=0 gl_reap >/dev/null
        check "a status probe kills nothing" kill -0 "$p_dead"
        out=$(GL_REAP_AGE=1 gl_reap)
        check "the dead gate's old _test is killed" [ "${out#note: killed x_test}" != "$out" ]
        check "its process is gone" bash -c "! kill -0 $p_dead 2>/dev/null"
        check "the kill is logged" grep -q "killed pid $p_dead x_test.*gate deadgate is dead" "$G/cleanup.log"
        check "a running gate's _test is left" kill -0 "$p_live"
        check "a program not named *_test is left" kill -0 "$p_plain"
        check "a _test outside ~/gates is left" kill -0 "$p_out"
        check "only one kill is logged" [ "$(wc -l < "$G/cleanup.log" | tr -d ' ')" = 1 ]
        # gl_holder does it too (every pool run); the dead pid-file case.
        p_young=$(dummy "$G/gone2/ui/w_test"); echo 99999999 > "$G/gone2.pid"
        sleep 2
        out=$(GL_REAP_AGE=1 gl_holder)
        check "gl_holder cleans orphans too" [ "${out#note: killed w_test}" != "$out" ]
        kill $p_live $p_plain $p_out $livegate 2>/dev/null
        wait 2>/dev/null
    else
        echo "FAIL: no C compiler (cc) to build the dummy process: orphan cleanup not tested"; fails=$((fails + 1))
    fi

    rm -rf "$T"
    if [ "$fails" -eq 0 ]; then
        echo "gate-lock.sh self-test: ok"
        return 0
    fi
    echo "gate-lock.sh self-test: $fails failed"
    return 1
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    case "${1:-}" in
        --self-test) gl_selftest; exit $? ;;
        *) sed -n '2,40p' "$0"; exit 2 ;;
    esac
fi
