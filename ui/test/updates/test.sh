#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./updates_test.app/Contents/MacOS/updates_test ]; then
    exec ./updates_test.app/Contents/MacOS/updates_test
else
    exec ./updates_test
fi
