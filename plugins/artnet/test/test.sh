#!/bin/sh
export LD_LIBRARY_PATH=../src
export DYLD_FALLBACK_LIBRARY_PATH=../src
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../artnet_test.app/Contents/MacOS/artnet_test ]; then
    exec ../../artnet_test.app/Contents/MacOS/artnet_test
else
    exec ../../artnet_test
fi
