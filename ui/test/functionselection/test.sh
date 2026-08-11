#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./functionselection_test.app/Contents/MacOS/functionselection_test ]; then
    exec ./functionselection_test.app/Contents/MacOS/functionselection_test
else
    exec ./functionselection_test
fi
