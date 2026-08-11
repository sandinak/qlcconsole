#!/bin/sh
export LD_LIBRARY_PATH=../../src
export DYLD_FALLBACK_LIBRARY_PATH=../../src
if [ -x ./palettegenerator_test.app/Contents/MacOS/palettegenerator_test ]; then
    exec ./palettegenerator_test.app/Contents/MacOS/palettegenerator_test
else
    exec ./palettegenerator_test
fi
