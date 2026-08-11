#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./efxpreviewarea_test.app/Contents/MacOS/efxpreviewarea_test ]; then
    exec ./efxpreviewarea_test.app/Contents/MacOS/efxpreviewarea_test
else
    exec ./efxpreviewarea_test
fi
