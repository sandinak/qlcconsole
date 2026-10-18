#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./wleddevices_test.app/Contents/MacOS/wleddevices_test ]; then
    exec ./wleddevices_test.app/Contents/MacOS/wleddevices_test
else
    exec ./wleddevices_test
fi
