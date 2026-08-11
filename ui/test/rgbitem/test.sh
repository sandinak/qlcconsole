#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./rgbitem_test.app/Contents/MacOS/rgbitem_test ]; then
    exec ./rgbitem_test.app/Contents/MacOS/rgbitem_test
else
    exec ./rgbitem_test
fi
