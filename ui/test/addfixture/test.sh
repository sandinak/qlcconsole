#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./addfixture_test.app/Contents/MacOS/addfixture_test ]; then
    exec ./addfixture_test.app/Contents/MacOS/addfixture_test
else
    exec ./addfixture_test
fi
