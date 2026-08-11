#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./functionstreewidget_test.app/Contents/MacOS/functionstreewidget_test ]; then
    exec ./functionstreewidget_test.app/Contents/MacOS/functionstreewidget_test
else
    exec ./functionstreewidget_test
fi
