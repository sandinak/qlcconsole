#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./effectdesigner_test.app/Contents/MacOS/effectdesigner_test ]; then
    exec ./effectdesigner_test.app/Contents/MacOS/effectdesigner_test
else
    exec ./effectdesigner_test
fi
