#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./mainbar_test.app/Contents/MacOS/mainbar_test ]; then
    exec ./mainbar_test.app/Contents/MacOS/mainbar_test
else
    exec ./mainbar_test
fi
