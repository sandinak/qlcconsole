#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./endhandle_test.app/Contents/MacOS/endhandle_test ]; then
    exec ./endhandle_test.app/Contents/MacOS/endhandle_test
else
    exec ./endhandle_test
fi
