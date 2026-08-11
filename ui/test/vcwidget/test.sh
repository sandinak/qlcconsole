#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcwidget_test.app/Contents/MacOS/vcwidget_test ]; then
    exec ./vcwidget_test.app/Contents/MacOS/vcwidget_test
else
    exec ./vcwidget_test
fi
