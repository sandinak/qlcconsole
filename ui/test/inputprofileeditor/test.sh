#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src:../../../webaccess/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src:../../../webaccess/src
if [ -x ./inputprofileeditor_test.app/Contents/MacOS/inputprofileeditor_test ]; then
    exec ./inputprofileeditor_test.app/Contents/MacOS/inputprofileeditor_test
else
    exec ./inputprofileeditor_test
fi
