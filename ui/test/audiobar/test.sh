#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./audiobar_test.app/Contents/MacOS/audiobar_test ]; then
    exec ./audiobar_test.app/Contents/MacOS/audiobar_test
else
    exec ./audiobar_test
fi
