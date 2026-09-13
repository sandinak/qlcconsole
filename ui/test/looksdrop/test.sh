#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./looksdrop_test.app/Contents/MacOS/looksdrop_test ]; then
    exec ./looksdrop_test.app/Contents/MacOS/looksdrop_test
else
    exec ./looksdrop_test
fi
