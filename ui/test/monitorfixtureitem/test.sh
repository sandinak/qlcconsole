#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./monitorfixtureitem_test.app/Contents/MacOS/monitorfixtureitem_test ]; then
    exec ./monitorfixtureitem_test.app/Contents/MacOS/monitorfixtureitem_test
else
    exec ./monitorfixtureitem_test
fi
