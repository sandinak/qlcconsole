#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
# Stock QLC+ 4.14.4 reads a .qxw qlcconsole saved (CMakeLists.txt).
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./stockroundtrip_test.app/Contents/MacOS/stockroundtrip_test ]; then
    exec ./stockroundtrip_test.app/Contents/MacOS/stockroundtrip_test
elif [ -x ./stockroundtrip_test ]; then
    exec ./stockroundtrip_test
else
    # Not built on this platform (Windows): nothing to run.
    echo "stockroundtrip: not built here; skipped"
    exit 0
fi
