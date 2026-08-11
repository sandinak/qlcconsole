#!/bin/bash
export LD_LIBRARY_PATH=../src:$LD_LIBRARY_PATH
export DYLD_FALLBACK_LIBRARY_PATH=../src:$DYLD_FALLBACK_LIBRARY_PATH
# Plugin executables build flat into build/plugins/ (see plugins/CMakeLists.txt),
# not alongside this test.sh, so look one level up from the plugin source dir.
if [ -x ../../enttecwing_test.app/Contents/MacOS/enttecwing_test ]; then
    exec ../../enttecwing_test.app/Contents/MacOS/enttecwing_test
else
    exec ../../enttecwing_test
fi
