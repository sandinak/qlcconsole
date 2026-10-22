#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./workbenchchaser_test.app/Contents/MacOS/workbenchchaser_test ]; then
    exec ./workbenchchaser_test.app/Contents/MacOS/workbenchchaser_test
else
    exec ./workbenchchaser_test
fi
