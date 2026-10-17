#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./showtreeview_test.app/Contents/MacOS/showtreeview_test ]; then
    exec ./showtreeview_test.app/Contents/MacOS/showtreeview_test
else
    exec ./showtreeview_test
fi
