#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./programmingoutline_test.app/Contents/MacOS/programmingoutline_test ]; then
    exec ./programmingoutline_test.app/Contents/MacOS/programmingoutline_test
else
    exec ./programmingoutline_test
fi
