#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vccuelist_test.app/Contents/MacOS/vccuelist_test ]; then
    exec ./vccuelist_test.app/Contents/MacOS/vccuelist_test
else
    exec ./vccuelist_test
fi
