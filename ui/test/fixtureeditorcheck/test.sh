#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixtureeditorcheck_test.app/Contents/MacOS/fixtureeditorcheck_test ]; then
    exec ./fixtureeditorcheck_test.app/Contents/MacOS/fixtureeditorcheck_test
else
    exec ./fixtureeditorcheck_test
fi
