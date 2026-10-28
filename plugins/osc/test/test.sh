#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../osc_test.app/Contents/MacOS/osc_test ]; then
    exec ../../osc_test.app/Contents/MacOS/osc_test
else
    exec ../../osc_test
fi
