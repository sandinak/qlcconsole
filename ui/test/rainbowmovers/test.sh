#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./rainbowmovers_test.app/Contents/MacOS/rainbowmovers_test ]; then
    exec ./rainbowmovers_test.app/Contents/MacOS/rainbowmovers_test
else
    exec ./rainbowmovers_test
fi
