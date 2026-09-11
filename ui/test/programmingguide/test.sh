#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./programmingguide_test.app/Contents/MacOS/programmingguide_test ]; then
    exec ./programmingguide_test.app/Contents/MacOS/programmingguide_test
else
    exec ./programmingguide_test
fi
