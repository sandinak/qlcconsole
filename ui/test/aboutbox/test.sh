#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./aboutbox_test.app/Contents/MacOS/aboutbox_test ]; then
    exec ./aboutbox_test.app/Contents/MacOS/aboutbox_test
else
    exec ./aboutbox_test
fi
