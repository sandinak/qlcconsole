#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./itemicons_test.app/Contents/MacOS/itemicons_test ]; then
    exec ./itemicons_test.app/Contents/MacOS/itemicons_test
else
    exec ./itemicons_test
fi
