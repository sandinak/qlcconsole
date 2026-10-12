#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./lookeditor_test.app/Contents/MacOS/lookeditor_test ]; then
    exec ./lookeditor_test.app/Contents/MacOS/lookeditor_test
else
    exec ./lookeditor_test
fi
