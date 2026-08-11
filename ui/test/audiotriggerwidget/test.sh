#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./audiotriggerwidget_test.app/Contents/MacOS/audiotriggerwidget_test ]; then
    exec ./audiotriggerwidget_test.app/Contents/MacOS/audiotriggerwidget_test
else
    exec ./audiotriggerwidget_test
fi
