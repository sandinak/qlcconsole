#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./patchlistdialog_test.app/Contents/MacOS/patchlistdialog_test ]; then
    exec ./patchlistdialog_test.app/Contents/MacOS/patchlistdialog_test
else
    exec ./patchlistdialog_test
fi
