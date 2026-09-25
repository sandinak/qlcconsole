#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./networknodeitem_test.app/Contents/MacOS/networknodeitem_test ]; then
    exec ./networknodeitem_test.app/Contents/MacOS/networknodeitem_test
else
    exec ./networknodeitem_test
fi
