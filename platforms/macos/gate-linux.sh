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
# GATE_LINUX_HOST (default branson@192.168.1.245) names the host.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
LOCKLIB="$HERE/gate-lock.sh"
HOST="${GATE_LINUX_HOST:-branson@192.168.1.245}"
PATIENCE="${GATE_POOL_PATIENCE:-1800}"
POLL="${GATE_POOL_POLL:-30}"
SSHOPT=(-o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=15 -o ServerAliveCountMax=4)

REV=HEAD; OUT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --rev)  shift; REV="${1:?--rev needs a commit}" ;;
        --out)  shift; OUT="${1:?--out needs a file}" ;;
        --host) shift; HOST="${1:?--host needs user@ip}" ;;
        -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
        *) echo >&2 "gate-linux.sh: unknown option '$1'"; exit 2 ;;
    esac
    shift
done
[ -n "$OUT" ] || { echo >&2 "gate-linux.sh: --out FILE is required"; exit 2; }

SHA=$(git -C "$REPO" rev-parse --verify "$REV^{commit}" 2>/dev/null) \
    || { echo >&2 "gate-linux.sh: '$REV' is not a commit here"; exit 2; }
ID="lx-${SHA:0:12}-$(date +%Y%m%d-%H%M%S)-$$"
T0=$(date +%s)
RUN=""

on() { ssh "${SSHOPT[@]}" "$@" "$HOST" bash -s; }
onlock() { { cat "$LOCKLIB"; cat; } | on "$@"; }
say() { echo "linux: $*"; }

fail() { # <code> <why>: the result file, then out
    say "$2"
    { echo "linux=$1 linux_qt=? linux_host=${HOST#*@}"; echo "$2"; } > "$OUT"
    exit "$1"
}
# The host's lock dropped on the way out, whatever happens here -- unless the
# leg is still running there (lost touch): it drops it itself when done.
trap '[ -n "$RUN" ] && rm -f "$RUN"; onlock <<<"gl_release $ID" >/dev/null 2>&1' EXIT

# The host's lock: taken, or waited for.
WAITED=0; DOWN=0; SAID=""
trap 'fail 130 "stopped while waiting for the Linux host"' INT TERM
while :; do
    out=$(onlock <<<"gl_try '$ID' '$SHA' '$(hostname -s)'" 2>/dev/null)
    last=$(printf '%s\n' "$out" | tail -n 1)
    if [ "${last#@@}" != "$last" ]; then
        DOWN=0
        printf '%s\n' "$out" | sed -n "s/^note: /linux: /p"
        [ "$last" = "@@ok" ] && break
        held=${last#@@busy }
        if [ "$held" != "$SAID" ] || [ $((WAITED % 300)) -lt "$POLL" ]; then
            say "waiting for the Linux host: $held (${WAITED}s so far)"
            SAID=$held
        fi
    else
        [ "$WAITED" -eq 0 ] && fail 99 "the Linux host $HOST is unreachable (ALLOW_SKIP=1 to gate without it)"
        DOWN=$((DOWN + POLL))
        [ "$DOWN" -ge "$PATIENCE" ] && fail 99 "the Linux host $HOST was unreachable for ${DOWN}s while waiting for its lock"
    fi
    sleep "$POLL"
    WAITED=$((WAITED + POLL))
done
trap - INT TERM

say "${SHA:0:12} on $HOST, ~/gates/$ID"
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
    echo "linux=\$rc linux_qt=\$QTV linux_host=\$(hostname -s)"
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
