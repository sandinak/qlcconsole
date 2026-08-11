#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./simpledeskengine_test.app/Contents/MacOS/simpledeskengine_test ]; then
    exec ./simpledeskengine_test.app/Contents/MacOS/simpledeskengine_test
else
    exec ./simpledeskengine_test
fi
