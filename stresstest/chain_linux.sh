#!/bin/sh
# Perf campaign p1, one Linux host (buildhost x86_64 or the Pi 5), after the
# scaling sweep: Art-Net on the wire, the full app under input load (both in
# a loopback-only network namespace), [LSan], then the long soak (also in
# the namespace). Each step logs to ~/perf/data/chain.log.
#
#   sh chain_linux.sh WIRE_UNIVERSES SOAK_HOURS_PER_PHASE [lsan]
P=$HOME/perf; S=$P/src/stresstest; D=$P/data
WIRE_U=${1:-8,32,64,128}; SOAK_H=${2:-4}; LSAN=${3:-}
log() { echo "$(date '+%F %T %Z') $*" >> "$D/chain.log"; }
log "chain start: wire=$WIRE_U soak=${SOAK_H}h/phase lsan=$LSAN"
while pgrep -f "[p]erf_sweep.py" >/dev/null; do sleep 30; done
log "sweep finished"

if [ ! -f "$D/wire/wire_sweep.json" ]; then
    sh "$S/wire/run_netns.sh" "$P/home" python3 "$S/wire/wire_sweep.py" --qlcstress "$P/stress-build/qlcstress" \
        --resources "$P/src/resources" --plugins "$P/plug-artnet" --out "$D/wire" --capture linux \
        --universes "$WIRE_U" > "$D/wire.log" 2>&1
    log "wire sweep rc=$?"
fi

if [ ! -f "$D/io/io_result.json" ]; then
    sh "$S/wire/run_netns.sh" "$P/home" python3 "$S/wire/io_load.py" --exe "$P/build-rel/main/qlcconsole" \
        --qlcstress "$P/stress-build/qlcstress" --resources "$P/src/resources" --plugins "$P/plug-io" \
        --home "$P/home" --out "$D/io" --capture linux > "$D/io.log" 2>&1
    log "io load rc=$?"
    python3 "$S/wire/wire_stats.py" "$D/io/wire.pcap" --windows "$D/io/phases.json" --universes 0-31 \
        --json "$D/io/wire.json" >> "$D/io.log" 2>&1 && rm -f "$D/io/wire.pcap"
    log "io wire stats rc=$?"
fi

if [ "$LSAN" = "lsan" ] && [ ! -f "$D/lsan/lsan.json" ]; then
    B=$P/build-asan
    F="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
    export CCACHE_BASEDIR=$P CCACHE_NOHASHDIR=1
    if [ ! -x "$B/ui/test/m2leak/m2leak" ]; then
        cmake -S "$P/src" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=$F" \
            "-DCMAKE_CXX_FLAGS=$F -Wno-error" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined" \
            "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined" \
            "-DCMAKE_MODULE_LINKER_FLAGS=-fsanitize=address,undefined" > "$D/asan-cfg.log" 2>&1
        ninja -C "$B" -j8 m2leak > "$D/asan-build.log" 2>&1
        log "asan build rc=$?"
    fi
    M=$B/ui/test/m2leak/m2leak
    mkdir -p "$D/lsan"
    for n in 1 10 40; do
        R=$D/lsan/n$n; mkdir -p "$R"
        ( cd "$R" && env HOME="$P/home" QT_QPA_PLATFORM=offscreen QLC_IOPLUGIN_DIR="$P/plug-none" \
            QLC_NO_RECOVERY_PROMPT=1 QLC_LEAK_TEARDOWN=1 \
            ASAN_OPTIONS=detect_leaks=1:halt_on_error=0:log_path=$R/asan:fast_unwind_on_malloc=0:malloc_context_size=30 \
            LSAN_OPTIONS=report_objects=0:max_leaks=0 \
            UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:log_path=$R/ubsan \
            timeout 10800 "$M" "$n" "$P/shows/DanceHysteria-0.3.noip.qlcc" "$P/src/resources/samples/Sample.qxw" \
            > "$R/session.log" 2>&1; echo "rc=$?" > "$R/rc" ) &
    done
    wait
    log "lsan runs done: $(cat $D/lsan/n*/rc | tr '\n' ' ')"
    python3 "$S/m2leak/lsan_parse.py" "$D/lsan/lsan.json" "$D/lsan/n1:1" "$D/lsan/n10:10" "$D/lsan/n40:40" \
        > "$D/lsan/summary.txt" 2>&1
    log "lsan parse rc=$?"
fi

if [ ! -d "$D/soak" ]; then
    log "soak start: real,scaled ${SOAK_H} h each"
    cd "$S/soak24" && nice -n 5 sh "$S/wire/run_netns.sh" "$P/home" python3 "$S/soak24/run_soak.py" \
        --app-exe "$P/build-rel/main/qlcconsole" --linux-exe "$P/build-rel/main/qlcconsole" \
        --home "$P/home" --ioplugin-dir "$P/plug-none" \
        --show "$P/shows/DanceHysteria-0.3.noip.qlcc" --scaled-show "$P/shows/DanceHysteria-0.3-STRESS.noip.qlcc" \
        --phases real,scaled --hours "$SOAK_H" --out "$D/soak" > "$D/soak.log" 2>&1
    log "soak finished rc=$?"
fi
log "chain done"
