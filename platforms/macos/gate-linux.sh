#!/bin/bash
# The gate's Linux leg: compile and test one commit on the Linux build host,
# beside the Mac gate (platforms/macos/gate-pool.sh starts this next to the
# Mac's check-all.sh, so the legs overlap and Linux adds nothing to the
# gate's time unless it is the slower side).
#
#   platforms/macos/gate-linux.sh --rev SHA --out FILE [--host user@ip]
#
# Why: GCC 14 / Qt 6.8 on Linux broke things the Mac-only gate never saw (a
# missing <climits>, a QPointer of a forward-declared type) and went red in
# CI. This is that compiler and Qt, before the push.
#
# Like a Mac host: the host's lock first (~/gates/.gatelock, gate-lock.sh --
# one gate per host; it WAITS for another), `git archive SHA` into
# ~/gates/<id> there (its own checkout, ~/qlcconsole, is never touched), runs
# detached (survives an ssh drop), removes the copy, keeps
# ~/gates/<id>.summary.txt (+ .logs/ on a failure). Configure and build as
# CI's Linux job does (Ninja, Release, ccache; Linux always builds -Werror),
# then unittest.sh ui, offscreen, both phases, and the same "every test
# program ran" count as the Mac legs.
#
# FILE gets the result, first line
#     linux=N linux_qt=6.8.2 linux_host=ADDR
# (N: 0 passed; 1 failed -- configure, build, tests or count; 99 the host is
# unreachable or the copy failed; 98 lost touch), then its timings and, on a
# failure, what failed. The exit status is N.
#
# Hosts: gate-linux-hosts.conf beside this script (name, ssh destination;
# GATE_LINUX_HOSTS names another file). The leg takes the FIRST host in the
# file that is reachable, has the toolchain (cmake ninja ccache g++
# qtpaths6|qmake6) and a free lock; if every one is busy it waits, and the
# gates that wait are served first come, first served: a ticket in
# ${GATE_LINUX_QUEUE:-~/gates/.linux-queue} (on this Mac) keeps the
# longest-waiting gate at the head, and only the head tries the hosts.
# GATE_LINUX_HOST=user@ip uses that one host and nothing else. linux_host= in
# the result line is the host's name from the file.
#
#   gate-linux.sh --dry-run     say which host it would pick (or "would wait"),
#                               take nothing; GATE_DRYRUN=1 does the same
#   gate-linux.sh --self-test   the host choice and the queue, on fake hosts
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
LOCKLIB="$HERE/gate-lock.sh"
CONF="${GATE_LINUX_HOSTS:-$HERE/gate-linux-hosts.conf}"
QDIR="${GATE_LINUX_QUEUE:-$HOME/gates/.linux-queue}"
NEED="${GATE_LINUX_NEED:-cmake ninja ccache g++ qtpaths6|qmake6}"
HOST=""; NAME=""
PATIENCE="${GATE_POOL_PATIENCE:-1800}"
POLL="${GATE_POOL_POLL:-30}"
SSHOPT=(-o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=15 -o ServerAliveCountMax=4)

on() { ssh "${SSHOPT[@]}" "$@" "$HOST" bash -s; }
onlock() { { cat "$LOCKLIB"; echo "GL_NEED='$NEED'"; cat; } | on "$@"; }
say() { echo "linux: $*"; }

# The hosts, in the order they are tried: HN[] names, HA[] ssh destinations.
HN=(); HA=()
lx_hosts() {
    HN=(); HA=()
    if [ -n "${GATE_LINUX_HOST:-}" ]; then
        HN=("${GATE_LINUX_HOST#*@}"); HA=("$GATE_LINUX_HOST")
        return 0
    fi
    [ -r "$CONF" ] || { echo >&2 "gate-linux.sh: no host list $CONF (or set GATE_LINUX_HOST)"; return 1; }
    local n a
    while read -r n a _; do
        case "$n" in ''|'#'*) continue ;; esac
        [ -n "$a" ] || continue
        HN+=("$n"); HA+=("$a")
    done < "$CONF"
    [ ${#HN[@]} -gt 0 ] || { echo >&2 "gate-linux.sh: $CONF lists no host"; return 1; }
}

# One pass over the hosts, first to last. Sets HOST/NAME and returns 0 on the
# first one that is up, has the toolchain and gives up its lock (dry: that is
# free -- nothing taken). Else returns 1 with LX_HELD (what holds each busy
# host) and LX_UP (how many answered with a lock state) for the wait message.
LX_HELD=""; LX_UP=0
lx_try_all() { # <id> <sha> [dry]
    local id=$1 sha=$2 dry=${3:-} i out last
    LX_HELD=""; LX_UP=0
    for ((i = 0; i < ${#HN[@]}; i++)); do
        HOST=${HA[i]}; NAME=${HN[i]}
        if [ -n "$dry" ]; then
            out=$(onlock <<<"gl_need && { h=\$(GL_CLEAN=0 gl_holder | head -n 1); [ -z \"\$h\" ] && echo @@ok || echo \"@@busy \$h\"; }" 2>/dev/null)
        else
            out=$(onlock <<<"gl_need && gl_try '$id' '$sha' '$(hostname -s)'" 2>/dev/null)
        fi
        last=$(printf '%s\n' "$out" | tail -n 1)
        [ "${last#@@}" != "$last" ] || continue    # unreachable
        printf '%s\n' "$out" | sed -n "s/^note: /linux: $NAME: /p"
        case "$last" in
            @@ok) LX_UP=$((LX_UP + 1)); return 0 ;;
            @@busy*) LX_UP=$((LX_UP + 1)); LX_HELD="${LX_HELD:+$LX_HELD; }$NAME: ${last#@@busy }" ;;
            @@notool*) LX_HELD="${LX_HELD:+$LX_HELD; }$NAME: ${last#@@}" ;;
        esac
    done
    return 1
}

# The queue (this Mac): one ticket a waiting gate, named <epoch>-<pid>, whose
# content is that pid. The head is the oldest ticket with a live pid; dead
# tickets are swept. Only the head tries the hosts, so the gate that has
# waited longest takes the next free one.
lq_tickets() { ls "${QDIR:?}" 2>/dev/null | grep -E '^[0-9]+-[0-9]+$' | sort; }
lq_join() { # <name>
    mkdir -p "${QDIR:?}"
    printf '%s\n' "$$" > "${QDIR:?}/${1:?}.tmp" && mv "${QDIR:?}/${1:?}.tmp" "${QDIR:?}/${1:?}"
}
lq_leave() { [ -n "${1:-}" ] && rm -f "${QDIR:?}/${1:?}" "${QDIR:?}/${1:?}.tmp"; return 0; }
lq_live() { local p; p=$(cat "${QDIR:?}/${1:?}" 2>/dev/null); [ -n "$p" ] && kill -0 "$p" 2>/dev/null; }
lq_head() { # prints the head ticket's name
    local f
    for f in $(lq_tickets); do
        if lq_live "$f"; then echo "$f"; return 0; fi
        rm -f "${QDIR:?}/${f:?}"
    done
    return 1
}
lq_ahead() { # <name>: how many live tickets are older
    local f n=0
    for f in $(lq_tickets); do
        [ "$f" = "$1" ] && break
        lq_live "$f" && n=$((n + 1))
    done
    echo "$n"
}

# --self-test: the host choice and the queue, on fake hosts (a fake ssh runs
# the host's side in a scratch $HOME each). Seconds.
lx_selftest() {
    local fails=0 T rc out sleeper p1 p2 p3
    T=$(mktemp -d "${TMPDIR:-/tmp}/gate-linux-test.XXXXXX") || return 1
    check() { local what=$1; shift; if "$@"; then :; else echo "FAIL: $what"; fails=$((fails + 1)); fi; }
    mkdir -p "$T/bin" "$T/ha/gates" "$T/hb/gates" "$T/hc/gates"
    cat > "$T/bin/ssh" <<'FAKE'
#!/bin/bash
# fake ssh: the first word that is not "-o VALUE" is the host; it maps to a scratch HOME.
while [ "${1:-}" = -o ]; do shift 2; done
h=$1; shift
case "$h" in
    fa|*@fa) export HOME="$FAKE_ROOT/ha" ;;
    fb|*@fb) export HOME="$FAKE_ROOT/hb" ;;
    fc|*@fc) export HOME="$FAKE_ROOT/hc" ;;
    *) exit 255 ;;
esac
exec "$@"
FAKE
    chmod +x "$T/bin/ssh"
    export FAKE_ROOT="$T" PATH="$T/bin:$PATH"
    SSHOPT=(-o BatchMode=yes)
    printf '# test\nalpha fa\nbeta  fb\ngamma fc\ndead  fx\n' > "$T/hosts.conf"
    CONF="$T/hosts.conf"; NEED="bash"
    lock() { # <home> <id>: a held lock, its gate alive
        mkdir -p "$1/gates/.gatelock"
        printf 'id=%s\nsha=abcdefabcdef\nsince=%s\nfrom=x\n' "$2" "$(date +%s)" > "$1/gates/.gatelock/info"
        echo "$sleeper" > "$1/gates/$2.pid"
    }
    unlock() { rm -rf "${1:?}/gates/.gatelock"; }
    sleep 120 & sleeper=$!

    lx_hosts
    check "the file lists four hosts" [ "${#HN[@]}" = 4 ]
    out=$(lx_try_all x y dry; echo "$NAME")
    check "all free: the first host" [ "$out" = alpha ]
    check "a dry run takes nothing" [ ! -d "$T/ha/gates/.gatelock" ]
    lock "$T/ha" ga
    out=$(lx_try_all x y dry; echo "$NAME")
    check "first locked: the second" [ "$out" = beta ]
    lock "$T/hb" gb
    out=$(lx_try_all x y dry; echo "$NAME")
    check "two locked: the third" [ "$out" = gamma ]
    lock "$T/hc" gc
    lx_try_all x y dry; rc=$?
    check "all locked: none (rc 1)" [ "$rc" = 1 ]
    check "the dead host is not counted up" [ "$LX_UP" = 3 ]
    check "the wait message names the busy hosts" [ "${LX_HELD#alpha: gate abcdefabcdef running}" != "$LX_HELD" ]
    # A gate that died leaves a stale lock: free.
    echo 99999999 > "$T/ha/gates/ga.pid"
    out=$(lx_try_all x y dry; echo "$NAME")
    check "a dead gate's lock is free: the first host" [ "$out" = alpha ]
    unlock "$T/ha"; lock "$T/ha" ga
    # Real takes: the free host is taken once, the next caller goes elsewhere.
    unlock "$T/hb"; unlock "$T/hc"
    lx_try_all id1 sha1 >/dev/null; check "a real take gets beta" [ "$NAME" = beta ]
    check "its lock is there" grep -q '^id=id1$' "$T/hb/gates/.gatelock/info"
    lx_try_all id2 sha2 >/dev/null; check "the next gets gamma" [ "$NAME" = gamma ]
    lx_try_all id3 sha3 >/dev/null; rc=$?
    check "no free host: busy, nothing taken" [ "$rc" = 1 -a "$(sed -n 's/^id=//p' "$T/hc/gates/.gatelock/info")" = id2 ]
    # The toolchain check.
    unlock "$T/ha"; unlock "$T/hb"; unlock "$T/hc"
    NEED="no-such-tool-xyz"
    lx_try_all x y dry; rc=$?
    check "no toolchain on any host: none" [ "$rc" = 1 -a "$LX_UP" = 0 ]
    check "...and it says which" [ "${LX_HELD#alpha: notool}" != "$LX_HELD" ]
    NEED="bash"
    # GATE_LINUX_HOST: one host only.
    out=$(GATE_LINUX_HOST=user@fc lx_hosts; echo "${HN[*]}/${HA[*]}")
    check "GATE_LINUX_HOST overrides the file" [ "$out" = "fc/user@fc" ]
    # The queue: oldest live first; a dead head is skipped.
    QDIR="$T/q"
    sleep 120 & p1=$!; sleep 120 & p2=$!; sleep 120 & p3=$!
    mkdir -p "$QDIR"
    echo $p2 > "$QDIR/1000-2"; echo $p1 > "$QDIR/1000-1"; echo $p3 > "$QDIR/1005-3"
    check "the oldest ticket is the head" [ "$(lq_head)" = 1000-1 ]
    check "two are ahead of the third" [ "$(lq_ahead 1005-3)" = 2 ]
    kill $p1 2>/dev/null; wait $p1 2>/dev/null
    check "a dead head is skipped" [ "$(lq_head)" = 1000-2 ]
    check "...and swept" [ ! -e "$QDIR/1000-1" ]
    lq_leave 1000-2
    check "leaving passes the head on" [ "$(lq_head)" = 1005-3 ]
    kill $p2 $p3 $sleeper 2>/dev/null; wait 2>/dev/null
    rm -rf "${T:?}"
    if [ "$fails" -eq 0 ]; then echo "gate-linux.sh self-test: ok"; return 0; fi
    echo "gate-linux.sh self-test: $fails failed"; return 1
}

REV=HEAD; OUT=""; DRY="${GATE_DRYRUN:+1}"
while [ $# -gt 0 ]; do
    case "$1" in
        --rev)  shift; REV="${1:?--rev needs a commit}" ;;
        --out)  shift; OUT="${1:?--out needs a file}" ;;
        --host) shift; export GATE_LINUX_HOST="${1:?--host needs user@ip}" ;;
        --dry-run) DRY=1 ;;
        --self-test) lx_selftest; exit $? ;;
        -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo >&2 "gate-linux.sh: unknown option '$1'"; exit 2 ;;
    esac
    shift
done
[ -n "$OUT" ] || [ -n "$DRY" ] || { echo >&2 "gate-linux.sh: --out FILE is required"; exit 2; }
lx_hosts || exit 2

SHA=$(git -C "$REPO" rev-parse --verify "$REV^{commit}" 2>/dev/null) \
    || { echo >&2 "gate-linux.sh: '$REV' is not a commit here"; exit 2; }
ID="lx-${SHA:0:12}-$(date +%Y%m%d-%H%M%S)-$$"
T0=$(date +%s)
RUN=""


fail() { # <code> <why>: the result file, then out
    say "$2"
    { echo "linux=$1 linux_qt=? linux_host=${NAME:-${HOST#*@}}"; echo "$2"; } > "$OUT"
    exit "$1"
}
# Dry run: which host would it be?
if [ -n "$DRY" ]; then
    if lx_try_all "$ID" "$SHA" dry; then
        say "would pick $NAME ($HOST)"; exit 0
    fi
    say "would wait: ${LX_HELD:-no host answers}"
    [ "$LX_UP" -gt 0 ] && exit 0
    exit 1
fi

# The host's lock dropped on the way out, whatever happens here -- unless the
# leg is still running there (lost touch): it drops it itself when done.
TICKET="$(date +%s)-$$"
trap '[ -n "$RUN" ] && rm -f "$RUN"; lq_leave "$TICKET"; [ -n "$HOST" ] && onlock <<<"gl_release $ID" >/dev/null 2>&1' EXIT

# A host: the first free one, once this gate is the head of the queue.
WAITED=0; DOWN=0; SAID=""
trap 'fail 130 "stopped while waiting for the Linux host"' INT TERM
lq_join "$TICKET"
while :; do
    if [ "$(lq_head)" = "$TICKET" ]; then
        if lx_try_all "$ID" "$SHA"; then lq_leave "$TICKET"; break; fi
        if [ "$LX_UP" -gt 0 ]; then
            DOWN=0
            if [ "$LX_HELD" != "$SAID" ] || [ $((WAITED % 300)) -lt "$POLL" ]; then
                say "waiting for a Linux host: $LX_HELD (${WAITED}s so far)"
                SAID=$LX_HELD
            fi
        else
            [ "$WAITED" -eq 0 ] && fail 99 "no Linux host is usable (${LX_HELD:-all unreachable}; ALLOW_SKIP=1 to gate without it)"
            DOWN=$((DOWN + POLL))
            [ "$DOWN" -ge "$PATIENCE" ] && fail 99 "no Linux host was usable for ${DOWN}s while waiting (${LX_HELD:-all unreachable})"
        fi
    elif [ $((WAITED % 300)) -lt "$POLL" ]; then
        say "queued: $(lq_ahead "$TICKET") gate(s) ahead for a Linux host (${WAITED}s so far)"
    fi
    sleep "$POLL"
    WAITED=$((WAITED + POLL))
done
trap - INT TERM

say "${SHA:0:12} on $NAME ($HOST), ~/gates/$ID"
on <<EOF || fail 99 "the Linux host $HOST is unreachable (ALLOW_SKIP=1 to gate without it)"
mkdir -p "\$HOME/gates/$ID" && touch "\$HOME/gates/$ID.running"
EOF
git -C "$REPO" archive "$SHA" | ssh "${SSHOPT[@]}" "$HOST" "tar -x -C \"\$HOME/gates/$ID\"" \
    || { on <<<"rm -rf \"\$HOME/gates/$ID\" \"\$HOME/gates/$ID.running\"" >/dev/null 2>&1; fail 99 "copy to the Linux host failed"; }

# The run script, on the host: configure, build, test, summary, clean up.
RUN="$(mktemp "${TMPDIR:-/tmp}/gate-linux.XXXXXX")"
cat > "$RUN" <<RUNEOF
G="\$HOME/gates"
ID="$ID"
cd "\$G/\$ID" || exit 1
export QT_QPA_PLATFORM=offscreen CCACHE_BASEDIR="\$PWD" CCACHE_NOHASHDIR=1 QT_RCC_SOURCE_DATE_OVERRIDE=1
L=build-gate-logs; mkdir -p \$L
now() { date +%s; }
nt() { echo \$(( \$(now) - \$1 )); }
QTV=\$(qtpaths6 --qt-version 2>/dev/null || qmake6 -query QT_VERSION 2>/dev/null || echo ?)
res=PASS; rc=0; tc=0; tb=0; tt=0; ta=0; ran=0; exp=?
t0=\$(now)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_AUTOMOC_PATH_PREFIX=ON \\
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > \$L/configure.log 2>&1 \\
    || { res="FAIL(configure)"; rc=1; }
tc=\$(nt \$t0)
if [ \$rc -eq 0 ]; then
    t0=\$(now)
    cmake --build build -- -k 0 > \$L/build.log 2>&1 || { res="FAIL(build)"; rc=1; }
    tb=\$(nt \$t0)
fi
if [ \$rc -eq 0 ]; then
    t0=\$(now)
    QLC_TEST_PHASE=parallel ./unittest.sh ui "\$PWD/build" > \$L/tests.log 2>&1 || { res="FAIL(check)"; rc=1; }
    tt=\$(nt \$t0); t0=\$(now)
    echo "==================== tests (serial) ====================" >> \$L/tests.log
    QLC_TEST_PHASE=serial ./unittest.sh ui "\$PWD/build" >> \$L/tests.log 2>&1 || { res="FAIL(check)"; rc=1; }
    ta=\$(nt \$t0)
    ran=\$(grep -E "^Test programs run:" \$L/tests.log | awk '{ n += \$4 + \$6 + \$10 } END { print n + 0 }')
    exp=\$({ ls engine/test/*/test.sh ui/test/*/test.sh ai/test/*/test.sh
        ls plugins/{enttecwing,midi,artnet,E1.31,wled}/test/test.sh plugins/velleman/test/test.sh; } 2>/dev/null | wc -l)
    if [ "\$ran" -lt "\$exp" ]; then
        res="FAIL(count)"; rc=1
        echo "ran \$ran test programs of the \$exp in the tree. This is not a pass." >> \$L/tests.log
    fi
fi
{
    echo "linux=\$rc linux_qt=\$QTV linux_host=$NAME"
    echo "  Linux        gcc \$(gcc -dumpfullversion 2>/dev/null), Release, Ninja: configure \${tc}s   build \${tb}s   test \${tt}s   alone \${ta}s   tests \${ran}/\${exp}   \$res"
    if [ \$rc -ne 0 ]; then
        echo "FAILED: Linux \$res"
        case "\$res" in
            "FAIL(configure)") tail -n 40 \$L/configure.log ;;
            "FAIL(build)") grep -nE "error:|FAILED:" -A 3 \$L/build.log | head -n 80; tail -n 20 \$L/build.log ;;
            *) awk '/^==================== FAILED:/ { on = 1 } on { print } /^Unit tests failed/ { on = 0 }' \$L/tests.log | head -n 300
               grep -nE "ran [0-9]+ test programs of|not valid|Traceback" \$L/tests.log | head; tail -n 15 \$L/tests.log ;;
        esac
    fi
} > "\$G/\$ID.summary.txt"
[ \$rc -eq 0 ] || cp -R \$L "\$G/\$ID.logs" 2>/dev/null
cd "\$G" && rm -rf "./\$ID" "./\$ID.sh"
echo \$rc > "\$ID.rc"; rm -f "\$ID.running"
[ "\$(sed -n 's/^id=//p' .gatelock/info 2>/dev/null)" = "\$ID" ] && rm -rf .gatelock
exit 0
RUNEOF
ssh "${SSHOPT[@]}" "$HOST" "cat > \"\$HOME/gates/$ID.sh\"" < "$RUN" || fail 99 "could not start on the Linux host"
# Detached (nohup: an ssh drop doesn't end it) in its own process group.
on <<EOF || fail 99 "could not start on the Linux host"
set -m
nohup bash "\$HOME/gates/$ID.sh" > /dev/null 2>&1 < /dev/null &
echo \$! > "\$HOME/gates/$ID.pid"
EOF

stop() {
    trap - INT TERM
    on <<EOF
G="\$HOME/gates"
p=\$(cat "\$G/$ID.pid" 2>/dev/null) && [ -n "\$p" ] && kill -TERM -- "-\$p" 2>/dev/null
sleep 2; rm -rf "\$G/$ID" "\$G/$ID.sh" "\$G/$ID.running" "\$G/$ID.pid"
EOF
    fail 130 "stopped"
}
trap stop INT TERM

LOST=0
while :; do
    sleep 20
    if out=$(on 2>/dev/null <<EOF
G="\$HOME/gates"
if [ -e "\$G/$ID.rc" ]; then echo "@@done \$(cat "\$G/$ID.rc")"
elif [ -e "\$G/$ID.running" ]; then echo "@@running"
else echo "@@gone"; fi
EOF
    ); then
        LOST=0
        case "$out" in
            "@@done "*) break ;;
            @@running) ;;
            *) fail 98 "the gate's copy on the Linux host went away" ;;
        esac
    else
        LOST=$((LOST + 20))
        [ $LOST -le 20 ] && say "the Linux host is not answering; waiting up to ${PATIENCE}s"
        [ $LOST -ge "$PATIENCE" ] && fail 98 "lost touch with the Linux host for ${LOST}s; ~/gates/$ID.summary.txt there may still appear"
    fi
done
trap - INT TERM

SUMMARY=$(on 2>/dev/null <<<"cat \"\$HOME/gates/$ID.summary.txt\"; rm -f \"\$HOME/gates/$ID.rc\" \"\$HOME/gates/$ID.pid\"")
[ -n "$SUMMARY" ] || fail 98 "no summary from the Linux host; see ~/gates/$ID.summary.txt there"
printf '%s\n' "$SUMMARY" > "$OUT"
rc=$(head -n 1 "$OUT" | sed -n 's/^linux=\([0-9]*\) .*/\1/p')
say "done in $(( $(date +%s) - T0 ))s: $(head -n 1 "$OUT")"
exit "${rc:-98}"
