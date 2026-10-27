#!/bin/sh
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../osc_test.app/Contents/MacOS/osc_test ]; then
    exec ../../osc_test.app/Contents/MacOS/osc_test
else
    exec ../../osc_test
fi
