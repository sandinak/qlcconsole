#!/bin/sh
export LD_LIBRARY_PATH=../src:../../ui/src:../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../src:../../ui/src:../../engine/src
if [ -x ./webaccess_test.app/Contents/MacOS/webaccess_test ]; then
    exec ./webaccess_test.app/Contents/MacOS/webaccess_test
else
    exec ./webaccess_test
fi
