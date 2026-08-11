#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./speeddial_test.app/Contents/MacOS/speeddial_test ]; then
    exec ./speeddial_test.app/Contents/MacOS/speeddial_test
else
    exec ./speeddial_test
fi
