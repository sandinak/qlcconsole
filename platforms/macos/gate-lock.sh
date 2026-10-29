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
#   gl_try ID SHA        take the lock: last line "@@ok", or "@@busy WHAT"
#   gl_release ID        drop the lock if ID holds it and its gate is not
#                        running (a gate still running keeps it; it drops
#                        it itself when done: gl_done)
#   gl_done ID           drop the lock if ID holds it (the gate's own end)
#
#   platforms/macos/gate-lock.sh --self-test    the lock's own test, on a
#                                               scratch ~/gates (seconds)

GL_MAX="${GL_MAX:-7200}"
GL_GRACE="${GL_GRACE:-900}"
GL_CLEAN="${GL_CLEAN:-1}"

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

gl_holder() {
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

gl_try() {
    local id=$1 sha=$2 G held
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
        printf 'id=%s\nsha=%s\nsince=%s\nfrom=%s\n' "$id" "$sha" "$(gl_now)" "$(hostname -s 2>/dev/null)" \
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
