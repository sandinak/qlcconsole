#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcxypadarea_test.app/Contents/MacOS/vcxypadarea_test ]; then
    exec ./vcxypadarea_test.app/Contents/MacOS/vcxypadarea_test
else
    exec ./vcxypadarea_test
fi
