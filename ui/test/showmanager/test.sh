#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showmanager_test.app/Contents/MacOS/showmanager_test ]; then
    exec ./showmanager_test.app/Contents/MacOS/showmanager_test
else
    exec ./showmanager_test
fi
