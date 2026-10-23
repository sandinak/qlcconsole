#!/bin/bash
# `make test`: build the checked-out tree and install it as a launchable
# ~/Applications/qlcconsole-dev.app -- fast, unsigned (ad-hoc), no DMG.
#
# The dev app runs on Homebrew's Qt in place: Homebrew names its libraries
# with absolute paths, so the bundle holds only qlcconsole's own binaries,
# plugins and resources, plus the Qt plugins (the app only looks for those
# inside its bundle). It runs on this Mac, with Homebrew's Qt installed; it is
# not something to hand to anyone else -- that's `make dmg`.
#
# It has its own bundle id and name ("qlcconsole (dev)"), so macOS keeps it
# apart from the release app. Its show-file types rank "Alternate": a
# double-clicked .qlcc still opens in the release app.
#
#   platforms/macos/dev-app.sh            # build + install; prints the version
#   platforms/macos/dev-app.sh --run ARGS # ...then launch it with ARGS
#
# Env: BUILD (default <repo>/build-dev), DEV_APP, QTDIR (default Homebrew's).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD:-$REPO/build-dev}"
APP="${DEV_APP:-$HOME/Applications/qlcconsole-dev.app}"
DEV_ID="com.bransonmatheson.qlcconsole.dev"
DEV_NAME="qlcconsole (dev)"
export QTDIR="${QTDIR:-/opt/homebrew/opt/qt6}"
JOBS="$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"
PB=/usr/libexec/PlistBuddy
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister

step() { echo; echo ">>> $*"; }

QTPLUGINS="$QTDIR/share/qt/plugins"            # Homebrew's Qt6
[ -d "$QTDIR/plugins/platforms" ] && QTPLUGINS="$QTDIR/plugins"
[ -d "$QTPLUGINS/platforms" ] || { echo "ERROR: no Qt plugins under $QTDIR (set QTDIR)"; exit 1; }

# Configure once, and again only if the bundle path changed (a plain
# `cmake -S -B` would re-run configure on every call).
if ! grep -qxF "QLC_MACOS_BUNDLE:PATH=$APP" "$BUILD/CMakeCache.txt" 2>/dev/null; then
  step "Configure $(basename "$BUILD") (RelWithDebInfo, Homebrew Qt)"
  cmake -S "$REPO" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake" \
    -DQLC_MACOS_BUNDLE="$APP" \
    -DQLC_MACOS_DEV_BUNDLE=ON
else
  # Configured already: refresh it anyway (a couple of seconds), so the build
  # number and commit in the app's version are this commit's, not the last
  # configure's. QTDIR is exported above, so the Qt plugin paths stay right.
  cmake "$BUILD" >/dev/null
fi

step "Build (-j$JOBS)"
cmake --build "$BUILD" -j"$JOBS"

if pgrep -f "^$APP/Contents/MacOS/" >/dev/null; then
  echo "ERROR: $DEV_NAME is running -- quit it, then run make test again."
  echo "       (the build above is done; only the install was skipped)"
  exit 1
fi

step "Install $APP"
mkdir -p "$(dirname "$APP")"
rm -rf "$APP"
LOG="$BUILD/dev-app-install.log"
cmake --install "$BUILD" >"$LOG" 2>&1 || { tail -20 "$LOG"; echo "ERROR: install failed (log: $LOG)"; exit 1; }
[ -x "$APP/Contents/MacOS/qlcconsole" ] || { echo "ERROR: install left no $APP/Contents/MacOS/qlcconsole"; exit 1; }

# The app looks for Qt plugins only inside the bundle (main.cpp). The install
# rules copy the few a release needs; the dev app takes the whole set it
# might load, from Homebrew: offscreen (for QT_QPA_PLATFORM=offscreen runs),
# SVG icons, TLS (the update check) and permissions (the microphone prompt).
for d in platforms imageformats iconengines styles multimedia tls permissions networkinformation; do
  compgen -G "$QTPLUGINS/$d/*.dylib" >/dev/null || continue
  # Homebrew's plugin files are relative symlinks into its Cellar, which
  # `cmake --install` copies as links (dangling here): copy the real files.
  rm -rf "$APP/Contents/PlugIns/$d"
  mkdir -p "$APP/Contents/PlugIns/$d"
  cp -fL "$QTPLUGINS/$d/"*.dylib "$APP/Contents/PlugIns/$d/"
done
if [ -n "$(find "$APP" -type l -print -quit)" ]; then
  echo "ERROR: symlinks left in the bundle:"; find "$APP" -type l; exit 1
fi
chmod -R u+w "$APP"     # Homebrew's files arrive read-only

# Its own identity, so Finder, Spotlight and the Dock don't take it for the
# release app, and the release app stays the default for show files.
PLIST="$APP/Contents/Info.plist"
$PB -c "Set :CFBundleIdentifier $DEV_ID" "$PLIST"
$PB -c "Set :CFBundleName $DEV_NAME" "$PLIST"
$PB -c "Delete :CFBundleDisplayName" "$PLIST" 2>/dev/null || true
$PB -c "Add :CFBundleDisplayName string $DEV_NAME" "$PLIST"
i=0
while $PB -c "Print :CFBundleDocumentTypes:$i" "$PLIST" >/dev/null 2>&1; do
  $PB -c "Delete :CFBundleDocumentTypes:$i:LSHandlerRank" "$PLIST" 2>/dev/null || true
  $PB -c "Add :CFBundleDocumentTypes:$i:LSHandlerRank string Alternate" "$PLIST"
  i=$((i + 1))
done

step "Ad-hoc codesign"
find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" "$APP/Contents/MacOS" -type f 2>/dev/null |
  while IFS= read -r f; do
    file -b "$f" | grep -q 'Mach-O' && codesign --force --sign - "$f" 2>/dev/null || true
  done
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
"$LSREGISTER" -f "$APP" 2>/dev/null || true

VERSION_LINE="$(QT_QPA_PLATFORM=offscreen "$APP/Contents/MacOS/qlcconsole" --version 2>/dev/null | grep -m1 '^qlcconsole ' || true)"
echo
echo "Built:   $(git -C "$REPO" rev-parse --abbrev-ref HEAD) @ $(git -C "$REPO" rev-parse --short HEAD)$(git -C "$REPO" diff --quiet HEAD || echo ' (+ local changes)')"
echo "Version: ${VERSION_LINE:-(unknown -- '$APP/Contents/MacOS/qlcconsole --version' failed)}"
echo "App:     $APP"
echo "Launch:  open \"$APP\"   (or Spotlight: \"$DEV_NAME\")"
echo "         \"$APP/Contents/MacOS/qlcconsole\" -o show.qlcc"

if [ "${1:-}" = "--run" ]; then
  shift
  step "Run: qlcconsole $*"
  exec "$APP/Contents/MacOS/qlcconsole" "$@"
fi
