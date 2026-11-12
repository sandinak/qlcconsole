#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../ui/src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../ui/src:../../../engine/src
if [ -x ./pagescript_test.app/Contents/MacOS/pagescript_test ]; then
    exec ./pagescript_test.app/Contents/MacOS/pagescript_test
else
    exec ./pagescript_test
fi
