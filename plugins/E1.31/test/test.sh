#!/bin/sh
export LD_LIBRARY_PATH=../..
export DYLD_FALLBACK_LIBRARY_PATH=../..
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../e131_test.app/Contents/MacOS/e131_test ]; then
    exec ../../e131_test.app/Contents/MacOS/e131_test
else
    exec ../../e131_test
fi
