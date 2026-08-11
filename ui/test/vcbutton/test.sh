#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcbutton_test.app/Contents/MacOS/vcbutton_test ]; then
    exec ./vcbutton_test.app/Contents/MacOS/vcbutton_test
else
    exec ./vcbutton_test
fi
