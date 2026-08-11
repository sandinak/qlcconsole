#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixturetreewidget_test.app/Contents/MacOS/fixturetreewidget_test ]; then
    exec ./fixturetreewidget_test.app/Contents/MacOS/fixturetreewidget_test
else
    exec ./fixturetreewidget_test
fi
