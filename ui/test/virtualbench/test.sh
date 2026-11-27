#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src:../../../webaccess/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src:../../../webaccess/src
# The whole app's qDebug() would use up QtTest's warning budget before the
# bench's own findings ([VirtualBench] lines) are printed.
export QT_LOGGING_RULES="default.debug=false"
if [ -x ./virtualbench_test.app/Contents/MacOS/virtualbench_test ]; then
    exec ./virtualbench_test.app/Contents/MacOS/virtualbench_test -maxwarnings 0
else
    exec ./virtualbench_test -maxwarnings 0
fi
