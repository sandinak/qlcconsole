#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./eosimportwizard_test.app/Contents/MacOS/eosimportwizard_test ]; then
    exec ./eosimportwizard_test.app/Contents/MacOS/eosimportwizard_test
else
    exec ./eosimportwizard_test
fi
