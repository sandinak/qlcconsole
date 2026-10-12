#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./programmingmanager_test.app/Contents/MacOS/programmingmanager_test ]; then
    exec ./programmingmanager_test.app/Contents/MacOS/programmingmanager_test
else
    exec ./programmingmanager_test
fi
