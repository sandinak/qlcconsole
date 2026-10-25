#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./venuerigui_test.app/Contents/MacOS/venuerigui_test ]; then
    exec ./venuerigui_test.app/Contents/MacOS/venuerigui_test
else
    exec ./venuerigui_test
fi
