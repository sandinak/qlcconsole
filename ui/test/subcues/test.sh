#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./subcues_ui_test.app/Contents/MacOS/subcues_ui_test ]; then
    exec ./subcues_ui_test.app/Contents/MacOS/subcues_ui_test
else
    exec ./subcues_ui_test
fi
