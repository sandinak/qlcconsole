#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixturecreator_test.app/Contents/MacOS/fixturecreator_test ]; then
    exec ./fixturecreator_test.app/Contents/MacOS/fixturecreator_test
else
    exec ./fixturecreator_test
fi
