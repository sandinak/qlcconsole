#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcproperties_test.app/Contents/MacOS/vcproperties_test ]; then
    exec ./vcproperties_test.app/Contents/MacOS/vcproperties_test
else
    exec ./vcproperties_test
fi
