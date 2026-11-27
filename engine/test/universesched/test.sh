#!/bin/sh
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src
export DYLD_FALLBACK_LIBRARY_PATH=../../src
# The A/B (QLC_SCHED_AB=1) runs for many minutes: past QTest's 5 minute watchdog
[ -n "${QLC_SCHED_AB:-}" ] && export QTEST_FUNCTION_TIMEOUT=7200000
./universesched_test
