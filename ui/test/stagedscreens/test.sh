#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./stagedscreens_test.app/Contents/MacOS/stagedscreens_test ]; then
    exec ./stagedscreens_test.app/Contents/MacOS/stagedscreens_test
else
    exec ./stagedscreens_test
fi
