#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./courseshow_test.app/Contents/MacOS/courseshow_test ]; then
    exec ./courseshow_test.app/Contents/MacOS/courseshow_test
else
    exec ./courseshow_test
fi
