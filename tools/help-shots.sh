#!/bin/bash
#
# Takes the pictures in the in-app help (docs/images) from the real app,
# offscreen, on test-workspaces/help-demo.qxw. Rerun it when the UI changes.
# Nothing is drawn on the screen and no setting of yours is touched.
#
#   tools/help-shots.sh [build-dir] [only]    # only = a picture name substring
#
# A picture that's new must still be added to ui/src/help.qrc and used in a
# guide; run helpbrowser_test after.

set -e
cd "$(dirname "$0")/.."

BUILD=${1:-build}
ONLY=$2
APP=$BUILD/main/qlcconsole
[ -x "$APP" ] || { echo "No app at $APP -- build first."; exit 1; }

# A copy of the demo, so the app's autosave and recovery files stay out of the repo.
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp test-workspaces/help-demo.qxw "$WORK/help-demo.qxw"

# Only the Loopback output plugin: the pictures must never show this
# machine's network interfaces, Art-Net nodes (MAC addresses) or MIDI ports.
mkdir "$WORK/plugins"
LOOP=$(ls "$BUILD"/plugins/libloopback.* "$BUILD"/plugins/loopback/libloopback.* 2>/dev/null | head -1)
[ -n "$LOOP" ] || { echo "No Loopback plugin in $BUILD/plugins -- build it first."; exit 1; }
cp "$LOOP" "$WORK/plugins/"

QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 QLC_IOPLUGIN_DIR="$WORK/plugins" \
QLC_HELP_SHOTS="$PWD/docs/images" QLC_HELP_SHOTS_ONLY="$ONLY" \
    "$APP" -o "$WORK/help-demo.qxw" -platform offscreen 2>&1 | grep '^\[HELPSHOT\]'
RESULT=${PIPESTATUS[0]}

# Qt writes no colour profile into these, but a picture with one would put the
# QTBUG-147602 crash back in reach (see tools/png-colour-chunks.py) -- fail loudly.
python3 tools/png-colour-chunks.py docs/images || exit 1
exit "$RESULT"
