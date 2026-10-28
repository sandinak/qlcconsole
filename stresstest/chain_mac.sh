#!/bin/sh
# Perf campaign p1 on a Mac (dink), after the scaling sweep: Art-Net on the
# wire, the DMG's app under input load, then the long soak. Loopback only;
# a guard capture on every other interface proves nothing of ours left.
# The app's QSettings go to the user's real preferences (cfprefsd ignores a
# sandbox HOME): they were exported first and are put back after each run
# (~/perf/restore-prefs.sh).
#
#   sh chain_mac.sh WIRE_UNIVERSES SOAK_HOURS_PER_PHASE
P=$HOME/perf; S=$P/src/stresstest; D=$P/data
A=$P/qlcconsole.app/Contents
WIRE_U=${1:-8,32,64,128,256,512}; SOAK_H=${2:-12}
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
log() { echo "$(date '+%F %T %Z') $*" >> "$D/chain.log"; }
caffeinate -i -m -s -w $$ &
log "chain start: wire=$WIRE_U soak=${SOAK_H}h/phase"
while pgrep -f "[p]erf_sweep.py" >/dev/null; do sleep 30; done
log "sweep finished"

GUARD=$D/guard; mkdir -p "$GUARD"
GP=""
for i in en0 en1 vlan0 vlan1; do
    sudo -n tcpdump -i "$i" -n -w "$GUARD/$i.pcap" "udp and (port 6454 or portrange 7700-7799 or port 9871)" \
        > /dev/null 2> "$GUARD/$i.log" &
    GP="$GP $!"
done
sleep 2
log "guard captures up on en0 en1 vlan0 vlan1"

if [ ! -f "$D/wire/wire_sweep.json" ]; then
    env HOME="$P/home" CFFIXED_USER_HOME="$P/home" QT_PLUGIN_PATH="$A/PlugIns" \
        python3 "$S/wire/wire_sweep.py" --qlcstress "$P/stress-build/qlcstress" --resources "$P/src/resources" \
        --plugins "$A/PlugIns-artnet" --out "$D/wire" --capture mac --universes "$WIRE_U" > "$D/wire.log" 2>&1
    log "wire sweep rc=$?"
fi

if [ ! -f "$D/io/io_result.json" ]; then
    env QT_PLUGIN_PATH="$A/PlugIns" python3 "$S/wire/io_load.py" --exe "$A/MacOS/qlcconsole" \
        --qlcstress "$P/stress-build/qlcstress" --resources "$P/src/resources" --plugins "$A/PlugIns-io" \
        --home "$P/home" --out "$D/io" --capture mac > "$D/io.log" 2>&1
    log "io load rc=$?"
    python3 "$S/wire/wire_stats.py" "$D/io/wire.pcap" --windows "$D/io/phases.json" --universes 0-31 \
        --json "$D/io/wire.json" >> "$D/io.log" 2>&1 && sudo -n rm -f "$D/io/wire.pcap"
    log "io wire stats rc=$?"
    sh "$P/restore-prefs.sh" >> "$D/chain.log" 2>&1
fi

for p in $GP; do sudo -n kill -INT "$p" 2>/dev/null; done
sleep 3
for i in en0 en1 vlan0 vlan1; do
    n=$(tcpdump -r "$GUARD/$i.pcap" -n 2>/dev/null | wc -l | tr -d ' ')
    mine=$(tcpdump -r "$GUARD/$i.pcap" -n 2>/dev/null | grep -c -E "IP (192\.168\.1\.17|172\.18\.2\.17|192\.168\.11\.17|192\.168\.21\.217)\.")
    echo "$i: $n packets on Art-Net/OSC/io ports, $mine sent by this host" >> "$GUARD/guard.txt"
done
log "guard: $(tr '\n' ';' < "$GUARD/guard.txt")"

if [ ! -d "$D/soak" ]; then
    log "soak start: real,scaled ${SOAK_H} h each"
    cd "$S/soak24" && nice -n 5 python3 "$S/soak24/run_soak.py" --app-exe "$A/MacOS/qlcconsole" \
        --home "$P/home" --ioplugin-dir "$A/PlugIns-none" --strip-outputs \
        --show "$P/shows/DanceHysteria-0.3.noip.qlcc" --scaled-show "$P/shows/DanceHysteria-0.3-STRESS.noip.qlcc" \
        --phases real,scaled --hours "$SOAK_H" --out "$D/soak" > "$D/soak.log" 2>&1
    log "soak finished rc=$?"
    sh "$P/restore-prefs.sh" >> "$D/chain.log" 2>&1
fi
log "chain done"
