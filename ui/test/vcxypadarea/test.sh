#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcxypadarea_test.app/Contents/MacOS/vcxypadarea_test ]; then
    exec ./vcxypadarea_test.app/Contents/MacOS/vcxypadarea_test
else
    exec ./vcxypadarea_test
fi
