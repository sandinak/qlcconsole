#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./editphysical_test.app/Contents/MacOS/editphysical_test ]; then
    exec ./editphysical_test.app/Contents/MacOS/editphysical_test
else
    exec ./editphysical_test
fi
