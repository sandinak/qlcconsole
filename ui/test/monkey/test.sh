#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
# The gate and CI run a 20 s monkey (the serial list runs it alone); by hand
# the test's own default is 60 s, and QLC_MONKEY_MS sets either.
export QLC_MONKEY_MS="${QLC_MONKEY_MS:-20000}"
if [ -x ./monkey_test.app/Contents/MacOS/monkey_test ]; then
    exec ./monkey_test.app/Contents/MacOS/monkey_test
else
    exec ./monkey_test
fi
