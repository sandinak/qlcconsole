#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
# Test executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# beside the plugin hidrescan_test links.
export LD_LIBRARY_PATH=../..
export DYLD_FALLBACK_LIBRARY_PATH=../..

# The scan and open threads, everywhere.
if [ -x ../../hidrescan_test.app/Contents/MacOS/hidrescan_test ]; then
    ../../hidrescan_test.app/Contents/MacOS/hidrescan_test || exit $?
else
    ../../hidrescan_test || exit $?
fi

# The joystick reader when its pad goes and comes back: Linux's only.
if [ -x ../../hidlinuxjoystick_test ]; then
    ../../hidlinuxjoystick_test || exit $?
fi

# The GameController framework side: a Mac's only.
[ "$(uname)" = Darwin ] || exit 0
if [ -x ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test ]; then
    exec ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test
else
    exec ../../hidgamecontroller_test
fi
