#!/bin/bash

# Never put a test's windows on the real screen. On macOS the UI tests ran on
# the desktop: every dialog a test drives, and every widget one shows, popped
# up in front of whatever was being worked on (CI's Linux job has xvfb, so it
# never showed). Offscreen unless the caller asked for a platform -- set
# QT_QPA_PLATFORM=cocoa to watch them on purpose.
if [ "$(uname)" = "Darwin" ] && [ -z "$QT_QPA_PLATFORM" ]; then
  export QT_QPA_PLATFORM=offscreen
fi

# Define the source and destination directories
SOURCE_DIR="."
DEST_DIR="./build"
if [ -d "$2" ]; then
  DEST_DIR="$2"
fi

echo "Using the destination directory $DEST_DIR"

# No shipped PNG may carry a colour profile (iCCP/sRGB/gAMA/cHRM/cICP): Qt
# 6.11 on macOS can crash converting such an image for AppKit (QTBUG-147602).
# See tools/png-colour-chunks.py.
if ! python3 $SOURCE_DIR/tools/png-colour-chunks.py $SOURCE_DIR; then
  exit 1
fi

if [ ! -d $DEST_DIR/resources/ ]; then
  mkdir -p $DEST_DIR/resources/
fi

# Copy resources directories necessary for unittest -- entry by entry, and
# not what the build dir already has as a link to the very same file. A macOS
# build links resources/<kind>/* to the sources (resources/CMakeLists.txt), so
# copying onto those links wrote each source file over itself; with the gate's
# legs staging at the same time, through their own links to the same sources,
# that failed ("Permission denied", "Not a directory") and left files half
# written while another leg read them.
stage_resources()
{
    local kind="$1" e
    mkdir -p "$DEST_DIR/resources/$kind"
    for e in "$SOURCE_DIR/resources/$kind"/*; do
        [ -e "$e" ] || continue
        [ "$e" -ef "$DEST_DIR/resources/$kind/$(basename "$e")" ] && continue
        cp -R "$e" "$DEST_DIR/resources/$kind/" || return 1
    done
}
for kind in colorfilters fixtures gobos icons inputprofiles rgbscripts schemas; do
    if ! stage_resources $kind; then
        echo "Could not copy resources/$kind into $DEST_DIR for the tests."
        exit 1
    fi
done

# Find all files necessary for tests recursively in the source directory and copy
# to destination directory.
#
# Prune EVERY build directory, not just $DEST_DIR. The old version pruned only
# the destination, so a second build dir alongside it (a Release tree, a Werror
# tree, anything) got walked and copied: with a handful of them present this
# find matched 462292 files instead of 124, and the copy loop below -- one
# mkdir -p and one cp per file -- ground for over fifteen minutes before anyone
# noticed it wasn't hung. .gitignore already treats build* as build output.
for file in $(find $SOURCE_DIR \( -path "$SOURCE_DIR/build*" -o -path "$DEST_DIR" \) -prune -o \( -name "test.sh" -o -name "*.xml*" \) -print); do

    # Get the directory of the file (excluding the "./" prefix)
    dir=$(dirname ${file#./})

    # Create the destination directory if it doesn't exist
    mkdir -p $DEST_DIR/$dir

    # Copy the file there, unless it is there already as a link to itself
    # (see stage_resources above).
    [ "$file" -ef "$DEST_DIR/$dir/$(basename $file)" ] && continue
    if ! cp $file $DEST_DIR/$dir/; then
        echo "Could not copy $file into $DEST_DIR for the tests."
        exit 1
    fi
done

cp $SOURCE_DIR/platforms/linux/unittest.sh $DEST_DIR/
# The tests that must run alone, after the concurrent batch (see there).
cp $SOURCE_DIR/platforms/linux/unittest-serial.txt $DEST_DIR/

# Propagate the runner's exit status. Without capturing it, this script's exit
# status is popd's -- always 0 -- so "make check" reported failures in its
# output and still succeeded, which is exactly what a release gate must not do.
pushd $DEST_DIR
./unittest.sh $1
RESULT=$?
popd

exit $RESULT
