#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src
export DYLD_FALLBACK_LIBRARY_PATH=../../src
if [ -x ./hostileshow_test.app/Contents/MacOS/hostileshow_test ]; then
    exec ./hostileshow_test.app/Contents/MacOS/hostileshow_test
else
    exec ./hostileshow_test
fi
