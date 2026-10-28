#!/bin/bash
# Never on the real screen; QLC_TEST_REAL_SCREEN=1 to watch (platforms/linux/check-test-guard.sh).
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:$LD_LIBRARY_PATH
export DYLD_FALLBACK_LIBRARY_PATH=../../src
./channelmodifier_test
