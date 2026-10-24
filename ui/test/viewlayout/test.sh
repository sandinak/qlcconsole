#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./viewlayout_test.app/Contents/MacOS/viewlayout_test ]; then
    exec ./viewlayout_test.app/Contents/MacOS/viewlayout_test
else
    exec ./viewlayout_test
fi
