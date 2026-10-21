#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./focusmode_test.app/Contents/MacOS/focusmode_test ]; then
    exec ./focusmode_test.app/Contents/MacOS/focusmode_test
else
    exec ./focusmode_test
fi
