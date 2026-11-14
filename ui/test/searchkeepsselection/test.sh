#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./searchkeepsselection_test.app/Contents/MacOS/searchkeepsselection_test ]; then
    exec ./searchkeepsselection_test.app/Contents/MacOS/searchkeepsselection_test
else
    exec ./searchkeepsselection_test
fi
