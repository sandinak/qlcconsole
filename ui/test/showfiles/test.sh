#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showfiles_test.app/Contents/MacOS/showfiles_test ]; then
    exec ./showfiles_test.app/Contents/MacOS/showfiles_test
else
    exec ./showfiles_test
fi
