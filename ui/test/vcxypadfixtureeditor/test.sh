#!/bin/sh
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./vcxypadfixtureeditor_test.app/Contents/MacOS/vcxypadfixtureeditor_test ]; then
    exec ./vcxypadfixtureeditor_test.app/Contents/MacOS/vcxypadfixtureeditor_test
else
    exec ./vcxypadfixtureeditor_test
fi
