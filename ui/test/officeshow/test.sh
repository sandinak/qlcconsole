#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./officeshow_test.app/Contents/MacOS/officeshow_test ]; then
    exec ./officeshow_test.app/Contents/MacOS/officeshow_test
else
    exec ./officeshow_test
fi
