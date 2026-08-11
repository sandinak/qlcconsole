#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcframeproperties_test.app/Contents/MacOS/vcframeproperties_test ]; then
    exec ./vcframeproperties_test.app/Contents/MacOS/vcframeproperties_test
else
    exec ./vcframeproperties_test
fi
