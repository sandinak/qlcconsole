#!/bin/sh
# Working hours (08:00-22:00 America/New_York): every process of this user
# whose command line names ~/perf/ -- the perf campaign's harnesses, the app
# under test, their runners -- goes to nice 20, every thread of it (on Linux
# a renice -p only reaches one thread). Night: left as started. Logs each
# change to ~/perf/daynice.log. Ends when ~/perf/DAYNICE_STOP exists, or
# after 2 h with nothing to watch.
LOG=$HOME/perf/daynice.log
idle=0
echo "$(date '+%F %T %Z') daynice start (pid $$)" >> "$LOG"
while [ ! -e "$HOME/perf/DAYNICE_STOP" ]; do
    h=$(TZ=America/New_York date +%H)
    pids=$(pgrep -u "$(id -u)" -f "$HOME/perf/" | grep -v "^$$\$")
    if [ -z "$pids" ]; then idle=$((idle + 1)); else idle=0; fi
    [ $idle -ge 120 ] && break
    if [ "$h" -ge 8 ] && [ "$h" -lt 22 ]; then
        for p in $pids; do
            if [ -d /proc/$p/task ]; then
                for tn in $(ps -L -o tid=,ni= -p "$p" 2>/dev/null | awk '$2 < 19 {print $1 ":" $2}'); do
                    t=${tn%%:*}; n=${tn##*:}
                    renice 20 -p "$t" >/dev/null 2>&1 && \
                        echo "$(date '+%F %T %Z') nice 20: pid $p tid $t (was $n)" >> "$LOG"
                done
            else
                n=$(ps -o nice= -p "$p" 2>/dev/null | tr -d ' ')
                if [ -n "$n" ] && [ "$n" -lt 20 ]; then
                    renice 20 -p "$p" >/dev/null 2>&1 && \
                        echo "$(date '+%F %T %Z') nice 20: pid $p (was $n)" >> "$LOG"
                fi
            fi
        done
    fi
    sleep 60
done
echo "$(date '+%F %T %Z') daynice end" >> "$LOG"
