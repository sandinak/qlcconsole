#!/bin/bash
#
# Regenerate the application icons in this freedesktop hicolor tree from their
# SVG sources. The PNGs are checked in so a build needs no SVG renderer; rerun
# this (it needs rsvg-convert, from librsvg) whenever a source SVG changes.
#
# The tree is laid out exactly as it installs under share/icons/hicolor, and
# appicons.qrc puts the same files in the binaries for the window icon.

set -e
cd "$(dirname "$0")"

SVGDIR=../svg
SIZES="16 22 24 32 48 64 128 256"

# installed name   source SVG
ICONS="qlcconsole:qlcconsole.svg qlcconsole-fixtureeditor:fixture-editor.svg"

for icon in $ICONS; do
    name=${icon%%:*}
    svg=${icon#*:}
    for size in $SIZES; do
        mkdir -p ${size}x${size}/apps
        rsvg-convert -w $size -h $size "$SVGDIR/$svg" -o ${size}x${size}/apps/$name.png
    done
done
