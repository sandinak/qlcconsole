#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./networkreportdialog_test.app/Contents/MacOS/networkreportdialog_test ]; then
    exec ./networkreportdialog_test.app/Contents/MacOS/networkreportdialog_test
else
    exec ./networkreportdialog_test
fi
