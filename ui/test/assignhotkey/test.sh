#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./assignhotkey_test.app/Contents/MacOS/assignhotkey_test ]; then
    exec ./assignhotkey_test.app/Contents/MacOS/assignhotkey_test
else
    exec ./assignhotkey_test
fi
