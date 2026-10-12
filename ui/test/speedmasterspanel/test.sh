#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./speedmasterspanel_test.app/Contents/MacOS/speedmasterspanel_test ]; then
    exec ./speedmasterspanel_test.app/Contents/MacOS/speedmasterspanel_test
else
    exec ./speedmasterspanel_test
fi
