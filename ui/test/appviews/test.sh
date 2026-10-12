#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./appviews_test.app/Contents/MacOS/appviews_test ]; then
    exec ./appviews_test.app/Contents/MacOS/appviews_test
else
    exec ./appviews_test
fi
