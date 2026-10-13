#!/bin/bash
#
# Takes the pictures in the in-app help (docs/images) from the real app,
# offscreen, on test-workspaces/help-demo.qxw. Rerun it when the UI changes.
# Nothing is drawn on the screen and no setting of yours is touched.
#
#   ./help-shots.sh [build-dir] [only]    # only = a picture name substring
#
# A picture that's new must still be added to ui/src/help.qrc and used in a
# guide; run helpbrowser_test after.

set -e
cd "$(dirname "$0")"

BUILD=${1:-build}
ONLY=$2
APP=$BUILD/main/qlcconsole
[ -x "$APP" ] || { echo "No app at $APP -- build first."; exit 1; }

# A copy of the demo, so the app's autosave and recovery files stay out of the repo.
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp test-workspaces/help-demo.qxw "$WORK/help-demo.qxw"

QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 \
QLC_HELP_SHOTS="$PWD/docs/images" QLC_HELP_SHOTS_ONLY="$ONLY" \
    "$APP" -o "$WORK/help-demo.qxw" -platform offscreen 2>&1 | grep '^\[HELPSHOT\]'
exit "${PIPESTATUS[0]}"
