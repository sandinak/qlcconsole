#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./positiontool_test.app/Contents/MacOS/positiontool_test ]; then
    exec ./positiontool_test.app/Contents/MacOS/positiontool_test
else
    exec ./positiontool_test
fi
