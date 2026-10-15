#!/bin/bash
# Build a shareable, self-contained QLC+ (fork) .app + .dmg for macOS.
# Mirrors the upstream build-v4 CI recipe (.github/workflows/build.yml) but
# runs locally against Homebrew qt@5, in a dedicated build dir, and folds in
# Branson's custom user content (RGB scripts / fixtures / templates / bundle)
# so the effects work on a fresh machine.
#
# Result: ~/qlcconsole.app  and
#         <repo>/dist/qlcconsole-<release>-<universal|arm64>-<date>-<rev>.dmg
#
# UNIVERSAL=1 (the default) builds one app for Apple silicon AND Intel Macs:
# against the official, universal Qt (aqt; QTDIR, default ~/Qt-aqt/6.9.3/macos)
# and the plugins' libraries built universal by universal-deps.sh
# (UNIDEPS, default ~/unideps). UNIVERSAL=0 is the old Homebrew build: this
# Mac's arch only.
set -euo pipefail

REPO="/Users/branson/git/qlcconsole"
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USERDATA="$HOME/Library/Application Support/QLC+"
APP="$HOME/qlcconsole.app"
UNIVERSAL="${UNIVERSAL:-1}"
UNIDEPS="${UNIDEPS:-$HOME/unideps}"
JOBS="$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"

cd "$REPO"

step() { echo; echo "============================================================"; echo ">>> $*"; echo "============================================================"; }

if [ "$UNIVERSAL" = 1 ]; then
  export QTDIR="${QTDIR:-$HOME/Qt-aqt/6.9.3/macos}"
  BUILD="$REPO/build-universal"
  FLAVOUR=universal
  case "$(lipo -archs "$QTDIR/lib/QtCore.framework/QtCore" 2>/dev/null)" in
    *x86_64*arm64*|*arm64*x86_64*) ;;
    *) echo "ERROR: $QTDIR is not a universal Qt. Install one:"
       echo "  python3 -m venv /tmp/aqt && /tmp/aqt/bin/pip install aqtinstall"
       echo "  /tmp/aqt/bin/aqt install-qt mac desktop 6.9.3 clang_64 -O ~/Qt-aqt \\"
       echo "      -m qt3d qtimageformats qtmultimedia qtserialport qtwebsockets qtshadertools"
       exit 1 ;;
  esac
  [ -f "$UNIDEPS/lib/pkgconfig/libftdi1.pc" ] || \
    { echo "ERROR: no universal libraries in $UNIDEPS -- run platforms/macos/universal-deps.sh"; exit 1; }
  # Homebrew's libraries are this Mac's arch only: kept out of sight, so a
  # plugin either finds the universal one or goes without (OLA does).
  export PKG_CONFIG_PATH="$UNIDEPS/lib/pkgconfig" PKG_CONFIG_LIBDIR="$UNIDEPS/lib/pkgconfig"
  ARCHFLAGS=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
             -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake;$UNIDEPS"
             -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local"
             -DCMAKE_IGNORE_PATH="/opt/homebrew/lib;/opt/homebrew/include;/usr/local/lib;/usr/local/include;/opt/local/lib;/opt/local/include")
else
  export QTDIR="/opt/homebrew/opt/qt6"
  BUILD="$REPO/build-package"
  FLAVOUR="$(uname -m)"
  ARCHFLAGS=(-DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake")
fi

step "1/7 Configure (Release, $FLAVOUR, separate build dir: $(basename "$BUILD"))"
cmake -S . -B "$BUILD" "${ARCHFLAGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0   # what Qt 6.9 itself needs

step "2/7 Build (parallel)"
cmake --build "$BUILD" -j"$JOBS"

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
# Homebrew's Qt6 keeps plugins under share/qt/; official Qt/qt@5 don't.
QTPLUGINS="$QTDIR/plugins"
[ -d "$QTPLUGINS/platforms" ] || QTPLUGINS="$QTDIR/share/qt/plugins"
if [ -d "$QTPLUGINS/iconengines" ]; then
  mkdir -p "$APP/Contents/PlugIns/iconengines"
  cp -f "$QTPLUGINS/iconengines/"*.dylib "$APP/Contents/PlugIns/iconengines/" 2>/dev/null || true
fi
if [ "$UNIVERSAL" = 1 ]; then
  # Official Qt names its frameworks @rpath/..., which bundlefix.py (made for
  # Homebrew's absolute names) never follows: macdeployqt brings in what the
  # app, its helpers and the plugins pull in (QtQml for multimedia, ffmpeg).
  "$QTDIR/bin/macdeployqt" "$APP" \
    -executable="$APP/Contents/MacOS/qlcconsole" \
    -executable="$APP/Contents/MacOS/qlcconsole-fixtureeditor" \
    -executable="$APP/Contents/MacOS/qlcconsole-launcher" 2>&1 | grep -v "^Log:\|File exists" || true
  chmod -R u+w "$APP"
fi
python3 "$SCRIPTDIR/bundlefix.py" "$APP"

# Every @rpath reference has to be found in the bundle, on both arches.
missing=0
while IFS= read -r f; do
  file "$f" | grep -q 'Mach-O' || continue
  if [ "$UNIVERSAL" = 1 ]; then
    a=$(lipo -archs "$f" 2>/dev/null)
    case "$a" in *x86_64*arm64*|*arm64*x86_64*) ;;
      *) echo "  NOT UNIVERSAL ($a): ${f#$APP/}"; missing=1 ;; esac
  fi
  # (A dylib lists its own id first, once per arch: not a dependency.)
  id=$(otool -D "$f" | sed -n 2p)
  for ref in $(otool -L "$f" | awk '/@rpath\//{print $1}' | sort -u | grep -vxF "${id:-none}"); do
    [ -e "$APP/Contents/Frameworks/${ref#@rpath/}" ] || { echo "  MISSING ${ref} (from ${f#$APP/})"; missing=1; }
  done
done < <(find "$APP/Contents" -type f \( -perm -u+x -o -name "*.dylib" \))
[ "$missing" = 0 ] || { echo "ERROR: the bundle is incomplete"; exit 1; }
echo "  every reference resolves in the bundle ($FLAVOUR)"

# A bundle with no platform plugin builds and signs fine but cannot launch.
# Check it here rather than discovering it on a fresh machine.
if [ ! -f "$APP/Contents/PlugIns/platforms/libqcocoa.dylib" ]; then
  echo "ERROR: $APP has no PlugIns/platforms/libqcocoa.dylib -- it won't launch."
  exit 1
fi

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
RELEASE=$(cat "$REPO/VERSION")
BUILD_DATE=$(date -u '+%Y%m%d')
GIT_REV=$(git -C "$REPO" rev-parse --short HEAD)
DIST="$REPO/dist"; mkdir -p "$DIST"
DMG="$DIST/qlcconsole-${RELEASE}-${FLAVOUR}-${BUILD_DATE}-${GIT_REV}.dmg"
rm -f "$DMG"
cd "$REPO/platforms/macos/dmg"
./create-dmg --volname "qlcconsole ${RELEASE}" \
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
