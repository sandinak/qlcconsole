#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./autosave_test.app/Contents/MacOS/autosave_test ]; then
    exec ./autosave_test.app/Contents/MacOS/autosave_test
else
    exec ./autosave_test
fi
