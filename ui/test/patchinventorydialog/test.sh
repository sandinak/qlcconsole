#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./patchinventorydialog_test.app/Contents/MacOS/patchinventorydialog_test ]; then
    exec ./patchinventorydialog_test.app/Contents/MacOS/patchinventorydialog_test
else
    exec ./patchinventorydialog_test
fi
