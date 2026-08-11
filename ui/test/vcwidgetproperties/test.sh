#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcwidgetproperties_test.app/Contents/MacOS/vcwidgetproperties_test ]; then
    exec ./vcwidgetproperties_test.app/Contents/MacOS/vcwidgetproperties_test
else
    exec ./vcwidgetproperties_test
fi
