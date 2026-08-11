#!/bin/sh
export LD_LIBRARY_PATH=../src
export DYLD_FALLBACK_LIBRARY_PATH=../src
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../velleman_test.app/Contents/MacOS/velleman_test ]; then
    exec ../../velleman_test.app/Contents/MacOS/velleman_test
else
    exec ../../velleman_test
fi
