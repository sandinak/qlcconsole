#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixtureselection_test.app/Contents/MacOS/fixtureselection_test ]; then
    exec ./fixtureselection_test.app/Contents/MacOS/fixtureselection_test
else
    exec ./fixtureselection_test
fi
