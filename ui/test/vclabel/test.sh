#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vclabel_test.app/Contents/MacOS/vclabel_test ]; then
    exec ./vclabel_test.app/Contents/MacOS/vclabel_test
else
    exec ./vclabel_test
fi
