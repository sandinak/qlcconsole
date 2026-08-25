#!/bin/bash

# Define the source and destination directories
SOURCE_DIR="."
DEST_DIR="./build"
if [ -d "$2" ]; then
  DEST_DIR="$2"
fi

echo "Using the destination directory $DEST_DIR"

if [ ! -d $DEST_DIR/resources/ ]; then
  mkdir -p $DEST_DIR/resources/
fi

# Copy resources directories necessary for unittest
cp -r $SOURCE_DIR/resources/colorfilters $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/fixtures $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/gobos $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/icons $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/inputprofiles $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/rgbscripts $DEST_DIR/resources
cp -r $SOURCE_DIR/resources/schemas $DEST_DIR/resources

# Find all files necessary for tests recursively in the source directory and copy
# to destination directory.
#
# Prune EVERY build directory, not just $DEST_DIR. The old version pruned only
# the destination, so a second build dir alongside it (a Qt5/Qt6 pair, a Release
# tree, anything) got walked and copied: with a handful of them present this
# find matched 462292 files instead of 124, and the copy loop below -- one
# mkdir -p and one cp per file -- ground for over fifteen minutes before anyone
# noticed it wasn't hung. .gitignore already treats build* as build output.
for file in $(find $SOURCE_DIR \( -path "$SOURCE_DIR/build*" -o -path "$DEST_DIR" \) -prune -o \( -name "test.sh" -o -name "*.xml*" \) -print); do

    # Get the directory of the file (excluding the "./" prefix)
    dir=$(dirname ${file#./})

    # Create the destination directory if it doesn't exist
    mkdir -p $DEST_DIR/$dir

    # Move the file to the new destination
    cp $file $DEST_DIR/$dir/
done

cp $SOURCE_DIR/platforms/linux/unittest.sh $DEST_DIR/

# Propagate the runner's exit status. Without capturing it, this script's exit
# status is popd's -- always 0 -- so "make check" reported failures in its
# output and still succeeded, which is exactly what a release gate must not do.
pushd $DEST_DIR
./unittest.sh $1
RESULT=$?
popd

exit $RESULT
