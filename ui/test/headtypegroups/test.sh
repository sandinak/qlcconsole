#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./headtypegroups_test.app/Contents/MacOS/headtypegroups_test ]; then
    exec ./headtypegroups_test.app/Contents/MacOS/headtypegroups_test
else
    exec ./headtypegroups_test
fi
