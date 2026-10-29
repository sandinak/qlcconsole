#!/bin/bash
# The gate pool: run ./check-all.sh for one commit on whichever Mac is idle
# (or the one you name), in a throwaway copy there.
#
#   platforms/macos/gate-pool.sh [--host NAME|auto] [--rev REV] [-- ARGS...]
#   platforms/macos/gate-pool.sh --status      what auto would see, and pick
#   ./check-all.sh --remote NAME|auto [--rev REV] [--leg ... | --quick]
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
# The last block is the gate's summary; its first line is the result:
#     gate=0 host=newaudio qt=6.12.0 rev=3c423d721a5f
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
while [ $# -gt 0 ]; do
    case "$1" in
        --host)   shift; WANT="${1:?--host needs a name or auto}" ;;
        --rev)    shift; REV="${1:?--rev needs a commit}" ;;
        --status) STATUS=1 ;;
        --)       shift; break ;;
        -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo >&2 "gate-pool.sh: unknown option '$1' (try --help)"; exit 2 ;;
    esac
    shift
done
ARGS=("$@")

# The hosts, in file order.
N=(); DEST=(); QTP=(); XPATH=(); MINS=()
while read -r name dest qt xpath mins _; do
    case "$name" in ''|'#'*) continue ;; esac
    N+=("$name"); DEST+=("$dest"); QTP+=("$qt"); XPATH+=("$xpath"); MINS+=("${mins:-30}")
done < "$CONF"
[ ${#N[@]} -gt 0 ] || { echo >&2 "gate-pool.sh: no hosts in $CONF"; exit 2; }

# Run a script (stdin) on host i: through ssh, or here for "local".
on() { # <i> [ssh options...]
    local i=$1; shift
    if [ "${DEST[i]}" = local ]; then bash -s; else ssh "${SSHOPT[@]}" "$@" "${DEST[i]}" bash -s; fi
}
# A path from the conf as that host's shell sees it: ~ is its $HOME.
hostpath() { case "$1" in '~'|'~/'*) echo "\$HOME${1#\~}" ;; *) echo "$1" ;; esac; }

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
    [ "$STATUS" = 1 ] && exit 0
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

# The host's lock: taken, or waited for (gate-lock.sh).
WAITED=0; DOWN=0; SAID=""
trap 'echo; fail 130 "stopped while waiting for $NAME"' INT TERM
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
trap 'onlock $H <<<"gl_release $ID" >/dev/null 2>&1; rm -rf "${PROBES:?}"' EXIT
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

# The run script, with this end's values written in: check-all.sh in the
# copy, then the small logs kept and the copy removed, whatever this end
# does meanwhile.
RUN="$PROBES/run.sh"
{
    echo 'G="$HOME/gates"'
    echo "cd \"\$G/$ID\" || exit 1"
    echo "export PATH=\"$XP\$PATH\" QT6_PREFIX=\"$QT\" GATE_HOST=$NAME GATE_REV=$SHA"
    printf './check-all.sh'; for a in ${ARGS[@]+"${ARGS[@]}"}; do printf ' %q' "$a"; done
    echo " > \"\$G/$ID.log\" 2>&1"
    echo 'rc=$?'
    echo "cp build-gate-logs/summary.txt \"\$G/$ID.summary.txt\" 2>/dev/null"
    echo "[ \$rc -eq 0 ] || cp -R build-gate-logs \"\$G/$ID.logs\" 2>/dev/null"
    echo "cd \"\$G\" && rm -rf \"./$ID\" \"./$ID.sh\""
    echo "echo \$rc > \"$ID.rc\"; rm -f \"$ID.running\""
    # The host's lock, dropped by the gate itself: this end may be gone.
    echo "[ \"\$(sed -n 's/^id=//p' .gatelock/info 2>/dev/null)\" = \"$ID\" ] && rm -rf .gatelock"
} > "$RUN"
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
    on $H <<EOF
G="\$HOME/gates"
p=\$(cat "\$G/$ID.pid" 2>/dev/null) && [ -n "\$p" ] && kill -TERM -- "-\$p" 2>/dev/null
sleep 2; rm -rf "\$G/$ID" "\$G/$ID.sh" "\$G/$ID.running" "\$G/$ID.pid"
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

SUMMARY=$(on $H <<<"cat \"\$HOME/gates/$ID.summary.txt\"; rm -f \"\$HOME/gates/$ID.rc\" \"\$HOME/gates/$ID.pid\"" 2>/dev/null)
[ -n "$SUMMARY" ] || fail "${RC:-98}" "no summary; see ~/gates/$ID.log on $NAME"
printf '%s\n' "$SUMMARY" > "$REPO/build-gate-logs/pool-$ID.txt"
echo
echo "gate pool: $NAME, $(( $(date +%s) - T0 ))s from copy to result$([ "$RC" -ne 0 ] && echo "; full logs on $NAME: ~/gates/$ID.logs/")"
printf '%s\n' "$SUMMARY"
exit "$RC"
