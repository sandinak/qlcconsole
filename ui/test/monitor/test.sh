#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./monitor_test.app/Contents/MacOS/monitor_test ]; then
    exec ./monitor_test.app/Contents/MacOS/monitor_test
else
    exec ./monitor_test
fi
