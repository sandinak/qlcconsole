#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./ledlayout_test.app/Contents/MacOS/ledlayout_test ]; then
    exec ./ledlayout_test.app/Contents/MacOS/ledlayout_test
else
    exec ./ledlayout_test
fi
