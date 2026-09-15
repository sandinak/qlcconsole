#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixturemanager_test.app/Contents/MacOS/fixturemanager_test ]; then
    exec ./fixturemanager_test.app/Contents/MacOS/fixturemanager_test
else
    exec ./fixturemanager_test
fi
