#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./inputdevicenotices_test.app/Contents/MacOS/inputdevicenotices_test ]; then
    exec ./inputdevicenotices_test.app/Contents/MacOS/inputdevicenotices_test
else
    exec ./inputdevicenotices_test
fi
