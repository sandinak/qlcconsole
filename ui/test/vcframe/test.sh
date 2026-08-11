#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcframe_test.app/Contents/MacOS/vcframe_test ]; then
    exec ./vcframe_test.app/Contents/MacOS/vcframe_test
else
    exec ./vcframe_test
fi
