#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./cuerunner_test.app/Contents/MacOS/cuerunner_test ]; then
    exec ./cuerunner_test.app/Contents/MacOS/cuerunner_test
else
    exec ./cuerunner_test
fi
