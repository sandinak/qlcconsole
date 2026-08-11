#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./monitorfixture_test.app/Contents/MacOS/monitorfixture_test ]; then
    exec ./monitorfixture_test.app/Contents/MacOS/monitorfixture_test
else
    exec ./monitorfixture_test
fi
