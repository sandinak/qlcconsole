#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./definitionupdates_test.app/Contents/MacOS/definitionupdates_test ]; then
    exec ./definitionupdates_test.app/Contents/MacOS/definitionupdates_test
else
    exec ./definitionupdates_test
fi
