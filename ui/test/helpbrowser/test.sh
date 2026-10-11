#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./helpbrowser_test.app/Contents/MacOS/helpbrowser_test ]; then
    exec ./helpbrowser_test.app/Contents/MacOS/helpbrowser_test
else
    exec ./helpbrowser_test
fi
