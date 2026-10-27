#!/bin/sh
export LD_LIBRARY_PATH=../../src
export DYLD_FALLBACK_LIBRARY_PATH=../../src
if [ -x ./hostileshow_test.app/Contents/MacOS/hostileshow_test ]; then
    exec ./hostileshow_test.app/Contents/MacOS/hostileshow_test
else
    exec ./hostileshow_test
fi
