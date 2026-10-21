#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./nativecursor_test.app/Contents/MacOS/nativecursor_test ]; then
    exec ./nativecursor_test.app/Contents/MacOS/nativecursor_test
else
    exec ./nativecursor_test
fi
