#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
# Test executables build flat into build/plugins/ (see plugins/CMakeLists.txt).
if [ -x ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test ]; then
    exec ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test
else
    exec ../../hidgamecontroller_test
fi
