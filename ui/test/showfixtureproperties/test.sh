#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showfixtureproperties_test.app/Contents/MacOS/showfixtureproperties_test ]; then
    exec ./showfixtureproperties_test.app/Contents/MacOS/showfixtureproperties_test
else
    exec ./showfixtureproperties_test
fi
