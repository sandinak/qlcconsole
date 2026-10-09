#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./wledconfigcheck_test.app/Contents/MacOS/wledconfigcheck_test ]; then
    exec ./wledconfigcheck_test.app/Contents/MacOS/wledconfigcheck_test
else
    exec ./wledconfigcheck_test
fi
