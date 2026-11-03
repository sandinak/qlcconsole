#!/bin/bash
# Build a shareable, self-contained QLC+ (fork) .app + .dmg for macOS.
# Mirrors the upstream build-v4 CI recipe (.github/workflows/build.yml) but
# runs locally in a dedicated build dir. The app carries the repo's resources
# only: nothing from the builder's home (~/Library/Application Support/QLC+
# held personal fixtures, Finder duplicates and test scripts that shipped in
# 0.2.x DMGs). release-check.sh fails a DMG with any resource file that is not
# byte-identical to the repo's.
#
# Result: ~/qlcconsole.app  and
#         <repo>/dist/qlcconsole-<release>-<universal|arm64>-<date>-<rev>.dmg
#
# ~/qlcconsole.app is replaced as a side effect: `cmake --install` writes the
# bundle there (variables.cmake, QLC_MACOS_BUNDLE). The one it replaces is
# kept as ~/qlcconsole.app.prev (one generation; an extension Finder and
# LaunchServices don't take for an app). Restore with
#   rm -rf ~/qlcconsole.app && mv ~/qlcconsole.app.prev ~/qlcconsole.app
# It refuses to run while that app is open.
#
# The DMG is made without driving Finder (create-dmg --skip-jenkins): it holds
# the app and an Applications link, in a plain window. DMG_FINDER=1 restores
# the laid-out window with the background -- that runs an AppleScript that
# opens the volume in Finder on the real screen.
#
# Fast, because it builds only what ships (-DQLC_NO_TESTS=ON: none of the ~245
# test programs) through ccache, in a build dir that lasts between runs:
# /Volumes/Ext/build/<checkout>-<flavour> while ender's work drive is mounted,
# else <repo>/build-<flavour>. A second run re-compiles what the commits
# since changed, and re-links. QLC_ARCH=native (`make dmg ARCH=native`)
# builds arm64 only, for installing on our own Apple silicon Macs: the DMG is
# named ...-arm64-native-... and publish.sh refuses it. BUILD_DIR= overrides
# the build dir; NO_CCACHE=1 turns ccache off.
#
# UNIVERSAL=1 (the default) builds one app for Apple silicon AND Intel Macs:
# against the official, universal Qt (aqt; QTDIR, default ~/Qt-aqt/6.9.3/macos)
# and the plugins' libraries built universal by universal-deps.sh
# (UNIDEPS, default ~/unideps). UNIVERSAL=0 is the old Homebrew build: this
# Mac's arch only.
set -euo pipefail

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# The checkout this script is in (a worktree builds itself, not the main tree).
REPO="${REPO:-$(cd "$SCRIPTDIR/../.." && pwd)}"
APP="$HOME/qlcconsole.app"
UNIVERSAL="${UNIVERSAL:-1}"
UNIDEPS="${UNIDEPS:-$HOME/unideps}"
QLC_ARCH="${QLC_ARCH:-}"
case "$QLC_ARCH" in ""|native) ;; *) echo "ERROR: QLC_ARCH must be empty (universal) or native"; exit 1 ;; esac
JOBS="$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"

cd "$REPO"

step() { echo; echo "============================================================"; echo ">>> $*"; echo "============================================================"; }

if [ "$UNIVERSAL" = 1 ]; then
  export QTDIR="${QTDIR:-$HOME/Qt-aqt/6.9.3/macos}"
  OSXARCHS="arm64;x86_64"; FLAVOUR=universal
  if [ "$QLC_ARCH" = native ]; then
    [ "$(uname -m)" = arm64 ] || { echo "ERROR: ARCH=native is arm64 only; this Mac is $(uname -m)"; exit 1; }
    OSXARCHS=arm64; FLAVOUR=arm64-native
  fi
  BUILD_NAME="build-$FLAVOUR"
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
  ARCHFLAGS=(-DCMAKE_OSX_ARCHITECTURES="$OSXARCHS"
             -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake;$UNIDEPS"
             -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local"
             -DCMAKE_IGNORE_PATH="/opt/homebrew/lib;/opt/homebrew/include;/usr/local/lib;/usr/local/include;/opt/local/lib;/opt/local/include")
else
  export QTDIR="/opt/homebrew/opt/qt6"
  BUILD_NAME="build-package"
  FLAVOUR="$(uname -m)"
  ARCHFLAGS=(-DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake")
fi

# A build dir that lasts: on the work drive when it is mounted (named for the
# checkout, as a CMake build dir belongs to one source dir).
if [ -n "${BUILD_DIR:-}" ]; then BUILD="$BUILD_DIR"
elif [ -d /Volumes/Ext ] && [ -w /Volumes/Ext ] && mkdir -p /Volumes/Ext/build 2>/dev/null; then
  BUILD="/Volumes/Ext/build/$(basename "$REPO")-${BUILD_NAME#build-}"
else BUILD="$REPO/$BUILD_NAME"; fi

# ccache for every slice (the compiler sees both -arch flags in one call).
. "$SCRIPTDIR/ccache-env.sh"
LAUNCHER=()
if [ -z "${NO_CCACHE:-}" ] && command -v ccache >/dev/null 2>&1; then
  LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
            -DCMAKE_AUTOMOC_PATH_PREFIX=ON)
  # The same settings as check-all.sh: paths relative to the checkout, the
  # build dir out of the hash, rcc's qrc timestamp fixed.
  export CCACHE_BASEDIR="$REPO" CCACHE_NOHASHDIR=1 QT_RCC_SOURCE_DATE_OVERRIDE=1
  CC_BEFORE="$(ccache --print-stats 2>/dev/null | awk -F'\t' '/^direct_cache_hit|^preprocessed_cache_hit|^cache_miss/{s[$1]=$2} END{print s["direct_cache_hit"]+s["preprocessed_cache_hit"], s["cache_miss"]+0}')"
else
  LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER=)
fi

if pgrep -f "^$APP/Contents/MacOS/" >/dev/null; then
  echo "ERROR: $APP is running -- quit it first (this replaces it)."
  exit 1
fi

step "1/7 Configure (Release, $FLAVOUR, no tests, build dir: $BUILD)"
cmake -S . -B "$BUILD" "${ARCHFLAGS[@]}" "${LAUNCHER[@]}" \
  -DCMAKE_BUILD_TYPE=Release -DQLC_NO_TESTS=ON \
  -DQLC_MACOS_BUNDLE="$APP" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0   # what Qt 6.9 itself needs

step "2/7 Build (parallel)"
cmake --build "$BUILD" -j"$JOBS"
if [ -n "${CC_BEFORE:-}" ]; then
  read -r h0 m0 <<< "$CC_BEFORE"
  read -r h1 m1 <<< "$(ccache --print-stats 2>/dev/null | awk -F'\t' '/^direct_cache_hit|^preprocessed_cache_hit|^cache_miss/{s[$1]=$2} END{print s["direct_cache_hit"]+s["preprocessed_cache_hit"], s["cache_miss"]+0}')"
  echo "  ccache over this build: $((h1 - h0)) hits, $((m1 - m0)) misses (cache: ${CCACHE_DIR:-default})"
fi

step "3/7 Install into ~/qlcconsole.app (fresh; the old one -> qlcconsole.app.prev)"
if [ -d "$APP" ]; then
  rm -rf "$APP.prev"
  mv "$APP" "$APP.prev"
  echo "  kept the previous app as $APP.prev"
fi
cmake --install "$BUILD"

step "4/7 Resources: the repo's only (cmake --install put them there)"
# No user content: whatever is in this Mac's ~/Library/Application Support
# stays there. Anything a show needs ships from resources/ in the repo.
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
# The offscreen platform plugin: `qlcconsole -platform offscreen` runs headless
# (scripted checks, release-check.sh) without a window. Signed with the rest.
mkdir -p "$APP/Contents/PlugIns/platforms"
cp -f "$QTPLUGINS/platforms/libqoffscreen.dylib" "$APP/Contents/PlugIns/platforms/"
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
  if [ "$UNIVERSAL" = 1 ] && [ "$FLAVOUR" = universal ]; then
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
for need in PlugIns/platforms/libqoffscreen.dylib Resources/licenses/LGPL-2.1.txt Resources/licenses/THIRD-PARTY.txt; do
  [ -f "$APP/Contents/$need" ] || { echo "ERROR: $APP has no $need"; exit 1; }
done

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
DMG_STYLE=(--skip-jenkins); [ "${DMG_FINDER:-0}" = 1 ] && DMG_STYLE=()
VOLNAME="qlcconsole ${RELEASE}"; [ "$QLC_ARCH" = native ] && VOLNAME="$VOLNAME (arm64 only)"
./create-dmg "${DMG_STYLE[@]}" --volname "$VOLNAME" \
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
