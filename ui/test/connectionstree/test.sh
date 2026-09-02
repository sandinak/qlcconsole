#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./connectionstree_test.app/Contents/MacOS/connectionstree_test ]; then
    exec ./connectionstree_test.app/Contents/MacOS/connectionstree_test
else
    exec ./connectionstree_test
fi
