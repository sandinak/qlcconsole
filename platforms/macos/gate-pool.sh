#!/bin/bash
# The gate pool: run ./check-all.sh for one commit on whichever Mac is idle
# (or the one you name), in a throwaway copy there.
#
#   platforms/macos/gate-pool.sh [--host NAME|auto] [--rev REV] [-- ARGS...]
#   platforms/macos/gate-pool.sh --status      what auto would see, and pick
#   platforms/macos/gate-pool.sh --self-test   the run script and the shot
#                                              fetch-back, on a scratch ~/gates
#   ./check-all.sh --remote NAME|auto [--rev REV] [--leg ... | --quick | --strict]
#
# Screenshots: --shots DIR (check-all.sh --shots DIR) gives the tests
# QLC_SHOT_DIR=~/gates/<id>.shots on the host, and when the gate ends --
# passed or failed -- copies what they wrote into DIR here and removes it
# there. (The tests' HOME is private and the copy is deleted, so a path in
# the test, a dot-file in the host's home or an scp afterwards is never
# needed.) GATE_ENV="NAME ..." sends those variables' values along too (e.g.
# COURSE_RIG=1), for the tests that run only with them; not PATH, HOME or
# the gate's own.
#
# REV (default HEAD) must be committed: the copy is `git archive REV`, so
# what is gated is exactly that SHA -- push it with `git push origin
# <sha>:main`, never the branch name. ARGS go to that copy's check-all.sh.
#
# Hosts: gate-hosts.conf beside this script. auto asks every host at once
# (5 s to answer) for its load average over its performance cores and
# whether a gate is already running there (a check-all.sh process, or
# another gate's lock or claim in ~/gates), skips busy and unreachable ones
# and those without their Qt, and picks the smallest
#     minutes * (1 + load / performance cores)
# -- an idle fast Mac first, a loaded one only when it still beats an idle
# slow one. Nothing free: this Mac ("local" in the file).
#
# One gate per host: before it copies anything there, the pool takes the
# host's lock (~/gates/.gatelock; gate-lock.sh beside this script). While
# another gate holds the host -- that lock, a gate's ~/gates/*.running, or
# an agent's ~/gates/.vallock -- it WAITS, polling every GATE_POOL_POLL
# seconds (default 30) and saying so: "waiting for val: gate 3c423d721a5f
# running since 11:51". A holder whose gate is dead (or never started), or
# that is over 2 h old, is stale and cleared. The gate drops the lock when
# it ends, even with this end gone.
#
# The copy runs detached on the host (nohup), so an ssh drop -- dink's VPN
# blips -- doesn't kill it; this script polls it every 20 s, prints its
# progress, and waits out an unreachable stretch of up to GATE_POOL_PATIENCE
# seconds (default 1800). Ctrl-C stops the gate there and removes the copy.
#
# The Linux leg (platforms/macos/gate-linux.sh): next to the Mac's gate, from
# here, the pool compiles and tests the same commit on the Linux build host
# (GCC 14, Qt 6.8 -- what CI's Linux job breaks on). The hosts are listed in
# gate-linux-hosts.conf (GATE_LINUX_HOST=user@ip overrides it with one): the
# leg takes the first one that is up, has the toolchain and no gate running,
# else waits for whichever frees first, longest-waiting gate first
# (gate-linux.sh; --status says which it would pick). It has the host's lock
# like any gate, runs while the Mac gate does, and its result joins the
# summary: gate=0 needs it green.
# Host unreachable: gate=1, "linux=99", unless ALLOW_SKIP=1 (then
# "linux=skipped", like a skipped Mac leg). Not run for --quick or --leg
# (partial runs, not gates), or with GATE_LINUX=off.
#
# The last block is the gate's summary; its first line is the result:
#     gate=0 host=newaudio qt=6.12.0 rev=3c423d721a5f linux=0 linux_qt=6.8.2
# (gate=99: the host couldn't be reached or the copy failed; 98: lost
# touch with it mid-gate.) The exit status is N. Kept on the host, small:
# ~/gates/<id>.log (check-all's output) and ~/gates/<id>.summary.txt, plus
# ~/gates/<id>.logs/ (every leg's full log) when the gate failed; this Mac
# keeps a copy of the summary as build-gate-logs/pool-<id>.txt.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
CONF="${GATE_HOSTS:-$HERE/gate-hosts.conf}"
PATIENCE="${GATE_POOL_PATIENCE:-1800}"
POLL="${GATE_POOL_POLL:-30}"
LOCKLIB="$HERE/gate-lock.sh"
SSHOPT=(-o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=15 -o ServerAliveCountMax=4)

WANT=auto
REV=HEAD
STATUS=0
SHOTS=""
SELFTEST=0
while [ $# -gt 0 ]; do
    case "$1" in
        --host)   shift; WANT="${1:?--host needs a name or auto}" ;;
        --rev)    shift; REV="${1:?--rev needs a commit}" ;;
        --status) STATUS=1 ;;
        --shots)  shift; SHOTS="${1:?--shots needs a directory}" ;;
        --self-test) SELFTEST=1 ;;
        --)       shift; break ;;
        -h|--help) sed -n '2,59p' "$0"; exit 0 ;;
        *) echo >&2 "gate-pool.sh: unknown option '$1' (try --help)"; exit 2 ;;
    esac
    shift
done
ARGS=("$@")

# Run a script (stdin) on host i: through ssh, or here for "local".
on() { # <i> [ssh options...]
    local i=$1; shift
    if [ "${DEST[i]}" = local ]; then bash -s; else ssh "${SSHOPT[@]}" "$@" "${DEST[i]}" bash -s; fi
}
# A path from the conf as that host's shell sees it: ~ is its $HOME.
hostpath() { case "$1" in '~'|'~/'*) echo "\$HOME${1#\~}" ;; *) echo "$1" ;; esac; }

# The variables GATE_ENV names, as export lines for the run script.
gate_env_lines() {
    local v
    for v in ${GATE_ENV:-}; do
        case "$v" in
            PATH|HOME|TMPDIR|QT6_PREFIX|GATE_*|QLC_SHOT_DIR|QUICK_*) echo >&2 "gate-pool.sh: GATE_ENV may not set $v"; return 1 ;;
            [A-Za-z_]*) ;;
            *) echo >&2 "gate-pool.sh: GATE_ENV: '$v' is not a variable name"; return 1 ;;
        esac
        case "$v" in *[!A-Za-z0-9_]*) echo >&2 "gate-pool.sh: GATE_ENV: '$v' is not a variable name"; return 1 ;; esac
        [ -n "${!v+x}" ] || { echo >&2 "gate-pool.sh: GATE_ENV names $v, which is not set here"; return 1; }
        printf 'export %s=%q\n' "$v" "${!v}"
    done
}

# The run script for gate <id> on the host (stdout): check-all.sh in the
# copy, then the small logs kept and the copy removed, whatever this end
# does meanwhile. Uses SHA, ARGS, SHOTS.
write_run() { # <id> <host name> <qt> <path prefix> <extra export, or "">
    local id=$1 name=$2 qt=$3 xp=$4 qenv=$5 a
    echo 'G="$HOME/gates"'
    echo "cd \"\$G/$id\" || exit 1"
    echo "export PATH=\"$xp\$PATH\" QT6_PREFIX=\"$qt\" GATE_HOST=$name GATE_REV=$SHA"
    [ -z "$qenv" ] || echo "export $qenv"
    gate_env_lines || return 1
    # Screenshots: kept beside the copy (not in it: the copy goes at the
    # end), fetched and removed by the pool.
    [ -z "$SHOTS" ] || echo "mkdir -p \"\$G/$id.shots\" && export QLC_SHOT_DIR=\"\$G/$id.shots\""
    printf './check-all.sh'; for a in ${ARGS[@]+"${ARGS[@]}"}; do printf ' %q' "$a"; done
    echo " > \"\$G/$id.log\" 2>&1"
    echo 'rc=$?'
    echo "cp build-gate-logs/summary.txt \"\$G/$id.summary.txt\" 2>/dev/null"
    echo "[ \$rc -eq 0 ] || cp -R build-gate-logs \"\$G/$id.logs\" 2>/dev/null"
    echo "cd \"\$G\" && rm -rf \"./$id\" \"./$id.sh\""
    echo "echo \$rc > \"$id.rc\"; rm -f \"$id.running\""
    # The host's lock, dropped by the gate itself: this end may be gone.
    echo "[ \"\$(sed -n 's/^id=//p' .gatelock/info 2>/dev/null)\" = \"$id\" ] && rm -rf .gatelock"
}

# The screenshots of gate <id> from host i into <dir> here; removed there.
# Prints how many files came back.
fetch_shots() { # <i> <id> <dir>
    local i=$1 id=$2 dir=$3 rc
    mkdir -p "$dir" || return 1
    if [ "${DEST[i]}" = local ]; then
        ( mkdir -p "$HOME/gates/$id.shots" && cd "$HOME/gates/$id.shots" && tar -cf - . ) | tar -xf - -C "$dir"
    else
        ssh "${SSHOPT[@]}" "${DEST[i]}" "mkdir -p \"\$HOME/gates/$id.shots\" && cd \"\$HOME/gates/$id.shots\" && tar -cf - ." | tar -xf - -C "$dir"
    fi
    rc=$?
    [ "$rc" -eq 0 ] && on "$i" <<<"rm -rf \"\$HOME/gates/$id.shots\"" >/dev/null 2>&1
    find "$dir" -type f | wc -l | tr -d ' '
    return "$rc"
}

# --self-test: write_run's lines and fetch_shots, with a "local" host on a
# scratch HOME. Nothing is built.
gp_selftest() {
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/gate-pool-test.XXXXXX") || return 1
    ok() { echo "ok - $1"; }
    no() { echo "not ok - $1"; fails=$((fails + 1)); }
    SHA=0123456789abcdef0123456789abcdef01234567; ARGS=(--quick --strict)
    SHOTS=""; GATE_ENV=""
    out=$(write_run id1 dink /q /x: "QLC_TEST_ONLY=ui/test/a")
    case "$out" in *QLC_SHOT_DIR*) no "no --shots: no QLC_SHOT_DIR" ;; *) ok "no --shots: no QLC_SHOT_DIR" ;; esac
    printf '%s\n' "$out" | grep -qx "./check-all.sh --quick --strict > \"\$G/id1.log\" 2>&1" \
        && ok "the arguments reach check-all.sh" || no "the arguments reach check-all.sh"
    printf '%s\n' "$out" | grep -qx 'export QLC_TEST_ONLY=ui/test/a' && ok "the quick selection rides" || no "the quick selection rides"
    SHOTS="$tmp/out"
    out=$(write_run id1 dink /q /x: "")
    printf '%s\n' "$out" | grep -qx 'mkdir -p "$G/id1.shots" && export QLC_SHOT_DIR="$G/id1.shots"' \
        && ok "--shots: QLC_SHOT_DIR beside the copy" || no "--shots: QLC_SHOT_DIR beside the copy"
    COURSE_RIG="a b"; export COURSE_RIG; GATE_ENV="COURSE_RIG"
    out=$(write_run id1 dink /q /x: "")
    printf '%s\n' "$out" | grep -qx 'export COURSE_RIG=a\\ b' && ok "GATE_ENV sends a value, quoted" || no "GATE_ENV sends a value, quoted"
    GATE_ENV="HOME"
    write_run id1 dink /q /x: "" >/dev/null 2>&1 && no "GATE_ENV refuses HOME" || ok "GATE_ENV refuses HOME"
    GATE_ENV="NOT_SET_ANYWHERE_X"
    write_run id1 dink /q /x: "" >/dev/null 2>&1 && no "GATE_ENV refuses an unset name" || ok "GATE_ENV refuses an unset name"
    GATE_ENV=""
    # The run script itself, on a fake copy whose check-all.sh writes a shot.
    local H="$tmp/home"
    mkdir -p "$H/gates/id2/build-gate-logs"
    printf '#!/bin/bash\n[ -d "$QLC_SHOT_DIR" ] && echo png > "$QLC_SHOT_DIR/a.png"; echo gate=0 > build-gate-logs/summary.txt; exit 0\n' > "$H/gates/id2/check-all.sh"
    chmod +x "$H/gates/id2/check-all.sh"
    write_run id2 local /q "" "" > "$H/gates/id2.sh" && HOME="$H" bash "$H/gates/id2.sh"
    [ "$(cat "$H/gates/id2.rc" 2>/dev/null)" = 0 ] && [ ! -d "$H/gates/id2" ] && ok "the run script runs, keeps rc, removes the copy" \
        || no "the run script runs, keeps rc, removes the copy"
    [ -f "$H/gates/id2.shots/a.png" ] && ok "a test's shot lands beside the copy" || no "a test's shot lands beside the copy"
    DEST=(local)
    out=$(HOME="$H" fetch_shots 0 id2 "$tmp/out")
    [ "$out" = 1 ] && [ -f "$tmp/out/a.png" ] && ok "fetch_shots brings it back" || no "fetch_shots brings it back ($out)"
    [ ! -d "$H/gates/id2.shots" ] && ok "and removes it on the host" || no "and removes it on the host"
    out=$(HOME="$H" fetch_shots 0 nosuch "$tmp/out2")
    [ "$out" = 0 ] && ok "no shots: nothing, no error" || no "no shots: nothing, no error ($out)"
    rm -rf "$tmp"
    if [ "$fails" -eq 0 ]; then echo "gate-pool.sh self-test: ok"; return 0; fi
    echo "gate-pool.sh self-test: $fails failed"; return 1
}
if [ "$SELFTEST" = 1 ]; then gp_selftest; exit $?; fi

# The hosts, in file order.
N=(); DEST=(); QTP=(); XPATH=(); MINS=()
while read -r name dest qt xpath mins _; do
    case "$name" in ''|'#'*) continue ;; esac
    N+=("$name"); DEST+=("$dest"); QTP+=("$qt"); XPATH+=("$xpath"); MINS+=("${mins:-30}")
done < "$CONF"
[ ${#N[@]} -gt 0 ] || { echo >&2 "gate-pool.sh: no hosts in $CONF"; exit 2; }


#############################################################################
# Pick the host
#############################################################################

PROBES="${TMPDIR:-/tmp}/gate-pool-$$"
mkdir -p "$PROBES"
trap 'rm -rf "${PROBES:?}"' EXIT

# Host i runs the lock's functions (gate-lock.sh), then the script (stdin).
onlock() { # <i> [ssh options...]
    local i=$1; shift
    { cat "$LOCKLIB"; cat; } | on "$i" "$@"
}

probe() { # <i>: "perfcores load1 gates claims qt-ok"
    onlock "$1" <<EOF
gl_reap >/dev/null 2>&1
p=\$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)
l=\$(sysctl -n vm.loadavg | awk '{ print \$2 }')
g=\$(pgrep -f '[c]heck-all\.sh' | wc -l | tr -d ' ')
c=\$(GL_CLEAN=0 gl_holder | grep -vc '^note:')
q=no; [ -f "$(hostpath "${QTP[$1]}")/lib/cmake/Qt6/Qt6Config.cmake" ] && q=yes
echo "\$p \$l \$g \$c \$q"
EOF
}

if [ "$WANT" = auto ] || [ "$STATUS" = 1 ]; then
    for ((i = 0; i < ${#N[@]}; i++)); do
        ( probe $i > "$PROBES/$i" 2>/dev/null || echo "down" > "$PROBES/$i" ) &
    done
    wait
    BEST=-1; BESTSCORE=""; LOCAL=-1
    printf '%-10s %-24s %s\n' host ssh state >&2
    for ((i = 0; i < ${#N[@]}; i++)); do
        [ "${DEST[i]}" = local ] && LOCAL=$i
        read -r p l g c q < "$PROBES/$i"
        if [ "$p" = down ] || [ -z "${q:-}" ]; then
            state="unreachable"
        elif [ "$q" != yes ]; then
            state="no Qt at ${QTP[i]}"
        else
            score=$(awk -v m="${MINS[i]}" -v l="$l" -v p="$p" 'BEGIN { printf "%.1f", m * (1 + l / p) }')
            state="load $l on $p perf cores, score $score"
            if [ "$g" -gt 0 ] || [ "$c" -gt 0 ]; then
                state="BUSY: a gate is running ($g check-all, $c gate locks/claims); $state"
            elif [ -z "$BESTSCORE" ] || awk -v a="$score" -v b="$BESTSCORE" 'BEGIN { exit !(a < b) }'; then
                BEST=$i; BESTSCORE=$score
            fi
        fi
        printf '%-10s %-24s %s\n' "${N[i]}" "${DEST[i]}" "$state" >&2
    done
    if [ "$BEST" -lt 0 ]; then
        [ "$LOCAL" -ge 0 ] || { echo >&2 "gate-pool.sh: nothing free and no local host in $CONF"; exit 2; }
        BEST=$LOCAL
        echo >&2 "nothing free: falling back to ${N[BEST]} (this Mac)"
    fi
    echo >&2 "auto: ${N[BEST]}"
    if [ "$STATUS" = 1 ]; then
        [ "${GATE_LINUX:-on}" = off ] || "$HERE/gate-linux.sh" --dry-run 2>&1 | sed 's/^/  /' >&2
        exit 0
    fi
    H=$BEST
else
    H=-1
    for ((i = 0; i < ${#N[@]}; i++)); do [ "${N[i]}" = "$WANT" ] && H=$i; done
    [ "$H" -ge 0 ] || { echo >&2 "gate-pool.sh: no host '$WANT' in $CONF (${N[*]}, or auto)"; exit 2; }
fi

#############################################################################
# Copy, start, follow
#############################################################################

SHA=$(git -C "$REPO" rev-parse --verify "$REV^{commit}" 2>/dev/null) \
    || { echo >&2 "gate-pool.sh: '$REV' is not a commit here"; exit 2; }
ID="${SHA:0:12}-$(date +%Y%m%d-%H%M%S)-$$"
NAME="${N[H]}"
QT="$(hostpath "${QTP[H]}")"
XP=""; [ "${XPATH[H]}" != - ] && XP="$(hostpath "${XPATH[H]}"):"
T0=$(date +%s)
mkdir -p "$REPO/build-gate-logs"

fail() { # <code> <why>
    echo "gate=$1 host=$NAME qt=? rev=${SHA:0:12} ($2)"
    exit "$1"
}

# The Linux leg, beside the Mac's gate (gate-linux.sh): started now, it waits
# for its own host's lock. Only for a whole gate.
LXPID=""; LXOUT="$PROBES/linux.txt"; LXWANT=0
case " ${ARGS[*]-} " in *" --quick "*|*" --leg "*) ;; *) [ "${GATE_LINUX:-on}" = off ] || LXWANT=1 ;; esac
lxstop() { [ -n "$LXPID" ] && kill -TERM "$LXPID" 2>/dev/null; return 0; }
if [ "$LXWANT" = 1 ]; then
    "$HERE/gate-linux.sh" --rev "$SHA" --out "$LXOUT" > >(sed -u 's/^/  /') 2>&1 &
    LXPID=$!
fi

# The host's lock: taken, or waited for (gate-lock.sh).
WAITED=0; DOWN=0; SAID=""
trap 'echo; lxstop; fail 130 "stopped while waiting for $NAME"' INT TERM
while :; do
    out=$(onlock $H <<<"gl_try '$ID' '$SHA' '$(hostname -s)'" 2>/dev/null)
    last=$(printf '%s\n' "$out" | tail -n 1)
    if [ "${last#@@}" != "$last" ]; then
        DOWN=0
        printf '%s\n' "$out" | sed -n "s/^note: /gate pool: $NAME: /p"
        [ "$last" = "@@ok" ] && break
        held=${last#@@busy }
        # When the holder changes, and every 5 min while it doesn't.
        if [ "$held" != "$SAID" ] || [ $((WAITED % 300)) -lt "$POLL" ]; then
            echo "waiting for $NAME: $held (${WAITED}s so far)"
            SAID=$held
        fi
    else
        [ "$WAITED" -eq 0 ] && fail 99 "unreachable"
        DOWN=$((DOWN + POLL))
        [ "$DOWN" -ge "$PATIENCE" ] && fail 99 "unreachable for ${DOWN}s while waiting for its lock"
    fi
    sleep "$POLL"
    WAITED=$((WAITED + POLL))
done
trap - INT TERM
# Dropped on the way out, whatever happens here -- unless the gate is still
# running there (lost touch): it keeps it, and drops it itself when done.
trap 'lxstop; onlock $H <<<"gl_release $ID" >/dev/null 2>&1; rm -rf "${PROBES:?}"' EXIT
[ "$WAITED" -gt 0 ] && echo "gate pool: $NAME is free after ${WAITED}s"

echo "gate pool: ${SHA:0:12} on $NAME (${DEST[H]}), ~/gates/$ID"
on $H <<EOF || fail 99 "unreachable"
mkdir -p "\$HOME/gates/$ID" && touch "\$HOME/gates/$ID.running"
EOF
if [ "${DEST[H]}" = local ]; then
    git -C "$REPO" archive "$SHA" | tar -x -C "$HOME/gates/$ID"
else
    git -C "$REPO" archive "$SHA" | ssh "${SSHOPT[@]}" "${DEST[H]}" "tar -x -C \"\$HOME/gates/$ID\""
fi || { on $H <<<"rm -rf \"\$HOME/gates/$ID\" \"\$HOME/gates/$ID.running\"" >/dev/null 2>&1; fail 99 "copy failed"; }

# --quick picks its tests from what changed since QUICK_BASE, but the copy
# has no .git: work the list out here and send it along (without it every
# remote quick ran ALL of the Qt6 leg's tests). QLC_TEST_ONLY rides too.
QENV=""
case " ${ARGS[*]-} " in *" --quick "*)
    if [ -n "${QLC_TEST_ONLY:-}" ]; then
        QENV="QLC_TEST_ONLY=$(printf '%q' "$QLC_TEST_ONLY")"
    elif QMB=$(git -C "$REPO" merge-base "${QUICK_BASE:-origin/main}" "$SHA" 2>/dev/null); then
        git -C "$REPO" diff --name-only "$QMB" "$SHA" > "$PROBES/quick-changed"
        if [ "${DEST[H]}" = local ]; then
            cp "$PROBES/quick-changed" "$HOME/gates/$ID/.quick-changed"
        else
            ssh "${SSHOPT[@]}" "${DEST[H]}" "cat > \"\$HOME/gates/$ID/.quick-changed\"" < "$PROBES/quick-changed"
        fi || fail 99 "copy failed"
        QENV="QUICK_CHANGED_FILE=.quick-changed"
    fi ;;
esac

# The run script, with this end's values written in (write_run).
RUN="$PROBES/run.sh"
write_run "$ID" "$NAME" "$QT" "$XP" "$QENV" > "$RUN" || fail 99 "could not write the run script"
if [ "${DEST[H]}" = local ]; then
    cp "$RUN" "$HOME/gates/$ID.sh"
else
    ssh "${SSHOPT[@]}" "${DEST[H]}" "cat > \"\$HOME/gates/$ID.sh\"" < "$RUN"
fi || fail 99 "could not start"
# Detached (nohup: an ssh drop doesn't end it) in its own process group
# (set -m), so Ctrl-C here stops all of it and nothing else.
on $H <<EOF || fail 99 "could not start"
set -m
nohup bash "\$HOME/gates/$ID.sh" > /dev/null 2>&1 < /dev/null &
echo \$! > "\$HOME/gates/$ID.pid"
EOF

stop() {
    trap - INT TERM
    echo; echo "gate pool: stopping the gate on $NAME"
    lxstop
    on $H <<EOF
G="\$HOME/gates"
p=\$(cat "\$G/$ID.pid" 2>/dev/null) && [ -n "\$p" ] && kill -TERM -- "-\$p" 2>/dev/null
sleep 2; rm -rf "\$G/$ID" "\$G/$ID.sh" "\$G/$ID.running" "\$G/$ID.pid" "\$G/$ID.shots"
EOF
    fail 130 "stopped"
}
trap stop INT TERM

SHOWN=0; LOST=0
while :; do
    sleep 20
    # New lines of its progress; then whether it's done.
    if out=$(on $H <<EOF 2>/dev/null
G="\$HOME/gates"
# Whole lines only: a line still being written waits for the next poll.
if [ -f "\$G/$ID.log" ]; then
    n=\$(wc -l < "\$G/$ID.log"); tail -n +$((SHOWN + 1)) "\$G/$ID.log" | head -n \$((n - $SHOWN))
fi
if [ -e "\$G/$ID.rc" ]; then echo "@@done \$(cat "\$G/$ID.rc")"
elif [ -e "\$G/$ID.running" ]; then echo "@@running"
else echo "@@gone"; fi
EOF
    ); then
        LOST=0
        body=$(printf '%s\n' "$out" | sed '$d')
        last=$(printf '%s\n' "$out" | tail -n 1)
        if [ -n "$body" ]; then
            printf '%s\n' "$body"
            SHOWN=$((SHOWN + $(printf '%s\n' "$body" | wc -l)))
        fi
        case "$last" in
            "@@done "*) RC=${last#@@done }; break ;;
            @@running) ;;
            *) fail 98 "the gate's copy on $NAME went away" ;;
        esac
    else
        LOST=$((LOST + 20))
        [ $LOST -le 20 ] && echo "gate pool: $NAME not answering; waiting up to ${PATIENCE}s"
        [ $LOST -ge "$PATIENCE" ] && fail 98 "lost touch with $NAME for ${LOST}s; the gate may still finish there: ~/gates/$ID.summary.txt"
    fi
done
trap - INT TERM

# The screenshots, passed or failed: they are what a review needs.
SHOTLINE=""
if [ -n "$SHOTS" ]; then
    if NSHOTS=$(fetch_shots "$H" "$ID" "$SHOTS"); then
        SHOTLINE="shots: $NSHOTS file(s) in $SHOTS"
    else
        SHOTLINE="shots: could not copy them back from $NAME:~/gates/$ID.shots (left there)"
    fi
    echo "gate pool: $SHOTLINE"
fi

SUMMARY=$(on $H <<<"cat \"\$HOME/gates/$ID.summary.txt\"; rm -f \"\$HOME/gates/$ID.rc\" \"\$HOME/gates/$ID.pid\"" 2>/dev/null)
[ -n "$SUMMARY" ] || fail "${RC:-98}" "no summary; see ~/gates/$ID.log on $NAME"

# The Linux leg's result joins the summary: gate=0 needs it green too.
if [ -n "$LXPID" ]; then
    if kill -0 "$LXPID" 2>/dev/null; then echo "gate pool: $NAME is done; waiting for the Linux leg"; fi
    wait "$LXPID" 2>/dev/null
    LXPID=""
    LXHEAD=$(head -n 1 "$LXOUT" 2>/dev/null)
    LXRC=$(printf '%s\n' "$LXHEAD" | sed -n 's/^linux=\([0-9]*\) .*/\1/p')
    [ -n "$LXRC" ] || { LXRC=98; LXHEAD="linux=98 linux_qt=? (no result from the Linux leg)"; }
    LXTAIL=$(tail -n +2 "$LXOUT" 2>/dev/null)
    LXFIRST=$(printf '%s\n' "$SUMMARY" | head -n 1)
    LXREST=$(printf '%s\n' "$SUMMARY" | tail -n +2)
    if [ "$LXRC" = 99 ] && [ -n "${ALLOW_SKIP:-}" ]; then
        LXHEAD="linux=skipped"
        LXTAIL="SKIPPED: Linux -- ${LXTAIL:-the Linux host is unreachable} -- allowed by ALLOW_SKIP"
        LXRC=0
    elif [ "$LXRC" -ne 0 ]; then
        LXTAIL="$LXTAIL
Linux leg did not pass: the gate fails.$([ "$LXRC" = 99 ] && echo " (Host unreachable: ALLOW_SKIP=1 accepts that.)")"
    fi
    if [ "$LXRC" -ne 0 ] && [ "$RC" -eq 0 ]; then
        RC=1
        LXFIRST=$(printf '%s\n' "$LXFIRST" | sed 's/^gate=0/gate=1/')
    fi
    SUMMARY="$LXFIRST ${LXHEAD}
$LXREST
$LXTAIL"
fi
[ -n "$SHOTLINE" ] && SUMMARY="$SUMMARY
$SHOTLINE"
printf '%s\n' "$SUMMARY" > "$REPO/build-gate-logs/pool-$ID.txt"
echo
echo "gate pool: $NAME, $(( $(date +%s) - T0 ))s from copy to result$([ "$RC" -ne 0 ] && echo "; full logs on $NAME: ~/gates/$ID.logs/")"
printf '%s\n' "$SUMMARY"
exit "$RC"
