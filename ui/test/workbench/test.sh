#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./workbench_test.app/Contents/MacOS/workbench_test ]; then
    exec ./workbench_test.app/Contents/MacOS/workbench_test
else
    exec ./workbench_test
fi
