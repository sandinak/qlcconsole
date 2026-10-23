#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./importmenu_test.app/Contents/MacOS/importmenu_test ]; then
    exec ./importmenu_test.app/Contents/MacOS/importmenu_test
else
    exec ./importmenu_test
fi
