#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showtreeworkbench_test.app/Contents/MacOS/showtreeworkbench_test ]; then
    exec ./showtreeworkbench_test.app/Contents/MacOS/showtreeworkbench_test
else
    exec ./showtreeworkbench_test
fi
