#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showcontrol_ui_test.app/Contents/MacOS/showcontrol_ui_test ]; then
    exec ./showcontrol_ui_test.app/Contents/MacOS/showcontrol_ui_test
else
    exec ./showcontrol_ui_test
fi
