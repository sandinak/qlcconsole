#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../src:../../ui/src:../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../src:../../ui/src:../../engine/src
if [ -x ./webaccess_test.app/Contents/MacOS/webaccess_test ]; then
    exec ./webaccess_test.app/Contents/MacOS/webaccess_test
else
    exec ./webaccess_test
fi
