#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcxypadfixture_test.app/Contents/MacOS/vcxypadfixture_test ]; then
    exec ./vcxypadfixture_test.app/Contents/MacOS/vcxypadfixture_test
else
    exec ./vcxypadfixture_test
fi
