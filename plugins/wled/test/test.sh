#!/bin/sh
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../wled_test.app/Contents/MacOS/wled_test ]; then
    exec ../../wled_test.app/Contents/MacOS/wled_test
else
    exec ../../wled_test
fi
