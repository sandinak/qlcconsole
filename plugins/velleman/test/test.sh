#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../src
export DYLD_FALLBACK_LIBRARY_PATH=../src
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../velleman_test.app/Contents/MacOS/velleman_test ]; then
    exec ../../velleman_test.app/Contents/MacOS/velleman_test
else
    exec ../../velleman_test
fi
