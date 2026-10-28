#!/bin/sh
# Linux: the MasterTimer asks for SCHED_FIFO and, without CAP_SYS_NICE or an
# rtprio limit, is refused and runs at normal priority, competing with one
# thread per universe. A/B at the loads around this host's limits: the same
# 'mixed' (per-step chasers) configuration with the default limits ("nort")
# and with RLIMIT_RTPRIO raised by prlimit ("rt", SCHED_FIFO granted). The
# scheduling class of every thread is read with ps mid-run (.policy: the
# threads in FF = SCHED_FIFO).
#   sh rt_ab.sh "128 192 256"      -> ~/perf/data/rtab/{rt,nort}-uNNNN.{json,timeline.csv,log,policy}
P=$HOME/perf; D=$P/data/rtab; R=$P/src/resources
mkdir -p "$D"
for u in $1; do
    q=$(( (u + 3) / 4 )); c=$(( (u + 7) / 8 ))
    for mode in nort rt; do
        name=$(printf "%s-u%04d" $mode $u)
        [ -f "$D/$name.json" ] && continue
        set -- "$P/stress-build/qlcstress" engine --mode realtime --json --seconds 40 --warmup-seconds 10 \
            --universes $u --fixtures-per-uni 170 --scenes $u --chasers $q --matrices $q --efx $q \
            --collections 0 --cuelists $c --scene-fixtures 170 --chaser-perstep \
            --fixtures-dir "$R/fixtures" --scripts-dir "$R/rgbscripts" --scenario "rtab/$name" --out "$D/$name"
        if [ $mode = rt ]; then
            sudo -n prlimit --rtprio=95:95 setpriv --reuid="$(id -u)" --regid="$(id -g)" --init-groups \
                env HOME="$P/home" "$@" > "$D/$name.log" 2>&1 &
        else
            env HOME="$P/home" "$@" > "$D/$name.log" 2>&1 &
        fi
        # the run's own qlcstress (not the sudo/setpriv wrapper)
        pid=""
        for i in $(seq 1 600); do
            sleep 1
            grep -q "^realtime: .* s at " "$D/$name.log" 2>/dev/null || continue
            pid=$(pgrep -n -x qlcstress)
            break
        done
        sleep 5
        [ -n "$pid" ] && ps -L -o tid=,cls=,rtprio=,comm= -p "$pid" | awk '{c[$2]++} END {for (k in c) printf "%s:%d ", k, c[k]; print ""}' > "$D/$name.policy"
        wait
        echo "$name: threads by class $(cat "$D/$name.policy" 2>/dev/null)"
    done
done
