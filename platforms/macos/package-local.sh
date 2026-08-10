#!/bin/bash
# Build a shareable, self-contained QLC+ (fork) .app + .dmg for macOS.
# Mirrors the upstream build-v4 CI recipe (.github/workflows/build.yml) but
# runs locally against Homebrew qt@5, in a dedicated build dir, and folds in
# Branson's custom user content (RGB scripts / fixtures / templates / bundle)
# so the effects work on a fresh machine.
#
# Result: ~/qlcconsole.app  and  <repo>/dist/qlcconsole-<ver>-<date>-<rev>.dmg
set -euo pipefail

REPO="/Users/branson/git/qlcconsole"
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$REPO/build-package"
USERDATA="$HOME/Library/Application Support/QLC+"
APP="$HOME/qlcconsole.app"
export QTDIR="/opt/homebrew/opt/qt6"

cd "$REPO"

step() { echo; echo "============================================================"; echo ">>> $*"; echo "============================================================"; }

step "1/7 Configure (Release, separate build dir: build-package)"
cmake -S . -B "$BUILD" \
  -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

step "2/7 Build (parallel)"
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"

step "3/7 Install into ~/qlcconsole.app (fresh)"
rm -rf "$APP"
cmake --install "$BUILD"

step "4/7 Fold in custom user content (faithful clone)"
RES="$APP/Contents/Resources"
copy_into() { # <src-glob-dir> <dest-subdir>
  local src="$1" dest="$RES/$2"
  mkdir -p "$dest"
  if compgen -G "$src/"'*' >/dev/null; then
    cp -f "$src/"* "$dest/" 2>/dev/null || true
    echo "  + $(ls -1 "$src" | wc -l | tr -d ' ') file(s) -> Resources/$2"
  fi
}
copy_into "$USERDATA/RGBScripts"         RGBScripts
copy_into "$USERDATA/Fixtures"           Fixtures
copy_into "$USERDATA/MidiTemplates"      MidiTemplates
copy_into "$USERDATA/ModifiersTemplates" ModifiersTemplates
copy_into "$USERDATA/Bundles"            Bundles

# Files copied from the Homebrew Cellar keep their read-only (0444) perms,
# which makes macdeployqt's strip pass and the final codesign fail with
# "not writable". Make the whole bundle user-writable before we touch it.
chmod -R u+w "$APP"

step "5/7 Bundle & rewrite ALL dependencies (deterministic, framework-aware)"
# We do NOT use macdeployqt or platforms/macos/fix_dylib_deps.sh here. Both
# assume official Qt (which uses @rpath install names). Homebrew's Qt5 uses
# ABSOLUTE install names (/opt/homebrew/opt/qt@5/...), so macdeployqt leaves
# absolute refs in the executables AND inside the Qt framework binaries, and
# misses transitive deps (harfbuzz/glib/pcre2/md4c, the libsndfile codec
# chain). Instead bundlefix.py walks the whole dependency graph: it copies
# every non-system dep into Frameworks (frameworks + flat libs), rewrites all
# absolute refs to @rpath, adds the executables' rpath, and verifies zero
# absolute references remain (non-zero exit fails the build).
# Add the SVG icon-engine plugin the CMake install step doesn't place.
if [ -d "$QTDIR/plugins/iconengines" ]; then
  mkdir -p "$APP/Contents/PlugIns/iconengines"
  cp -f "$QTDIR/plugins/iconengines/"*.dylib "$APP/Contents/PlugIns/iconengines/" 2>/dev/null || true
fi
python3 "$SCRIPTDIR/bundlefix.py" "$APP"

step "6/7 Ad-hoc codesign"
set +e   # codesign loops hit benign non-Mach-O files; gate explicitly below
find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" -type f 2>/dev/null | while IFS= read -r f; do
  file "$f" | grep -q 'Mach-O' && codesign --force --sign - "$f" 2>/dev/null
done
for b in "$APP/Contents/MacOS/"*; do
  file "$b" | grep -q 'Mach-O' && codesign --force --sign - "$b" 2>/dev/null
done
codesign --force --deep --sign - "$APP" 2>/dev/null
csok=1; codesign --verify --deep --strict "$APP" 2>/dev/null || csok=0
set -e
[ "$csok" = 1 ] && echo "  codesign verify OK (ad-hoc)" || { echo "ERROR: codesign verify failed"; exit 1; }

step "7/7 Create DMG"
APPVERSION=$(awk -F\" '/^#define APPVERSION/ {gsub(/ /,"-",$2); print $2}' "$REPO/engine/src/qlcconfig.h")
BUILD_DATE=$(date -u '+%Y%m%d')
GIT_REV=$(git -C "$REPO" rev-parse --short HEAD)
DIST="$REPO/dist"; mkdir -p "$DIST"
DMG="$DIST/qlcconsole-${APPVERSION}-${BUILD_DATE}-${GIT_REV}.dmg"
rm -f "$DMG"
cd "$REPO/platforms/macos/dmg"
./create-dmg --volname "qlcconsole ${APPVERSION}" \
  --volicon "$REPO/resources/icons/qlcconsole.icns" \
  --background background.png \
  --window-size 400 300 \
  --window-pos 200 100 \
  --icon-size 64 \
  --icon "qlcconsole" 0 150 \
  --app-drop-link 200 150 \
  "$DMG" \
  "$APP"

echo; echo "DONE."
echo "App: $APP"
echo "DMG: $DMG"
ls -lh "$DMG"
