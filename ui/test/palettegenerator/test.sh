#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src
export DYLD_FALLBACK_LIBRARY_PATH=../../src
if [ -x ./palettegenerator_test.app/Contents/MacOS/palettegenerator_test ]; then
    exec ./palettegenerator_test.app/Contents/MacOS/palettegenerator_test
else
    exec ./palettegenerator_test
fi
