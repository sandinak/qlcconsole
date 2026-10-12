#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./networktopologydialog_test.app/Contents/MacOS/networktopologydialog_test ]; then
    exec ./networktopologydialog_test.app/Contents/MacOS/networktopologydialog_test
else
    exec ./networktopologydialog_test
fi
