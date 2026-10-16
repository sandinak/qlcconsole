#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./fixtureuniversepatch_test.app/Contents/MacOS/fixtureuniversepatch_test ]; then
    exec ./fixtureuniversepatch_test.app/Contents/MacOS/fixtureuniversepatch_test
else
    exec ./fixtureuniversepatch_test
fi
