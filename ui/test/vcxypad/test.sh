#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcxypad_test.app/Contents/MacOS/vcxypad_test ]; then
    exec ./vcxypad_test.app/Contents/MacOS/vcxypad_test
else
    exec ./vcxypad_test
fi
