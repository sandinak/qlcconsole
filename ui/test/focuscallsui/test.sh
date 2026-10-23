#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./focuscallsui_test.app/Contents/MacOS/focuscallsui_test ]; then
    exec ./focuscallsui_test.app/Contents/MacOS/focuscallsui_test
else
    exec ./focuscallsui_test
fi
