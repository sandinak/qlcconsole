#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showdifferences_test.app/Contents/MacOS/showdifferences_test ]; then
    exec ./showdifferences_test.app/Contents/MacOS/showdifferences_test
else
    exec ./showdifferences_test
fi
