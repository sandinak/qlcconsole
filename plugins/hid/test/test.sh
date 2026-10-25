#!/bin/sh
# Test executables build flat into build/plugins/ (see plugins/CMakeLists.txt).
if [ -x ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test ]; then
    exec ../../hidgamecontroller_test.app/Contents/MacOS/hidgamecontroller_test
else
    exec ../../hidgamecontroller_test
fi
