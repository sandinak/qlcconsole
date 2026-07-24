#!/bin/bash
# Developer ID sign + notarize + staple an already-built ~/QLC+.app, then
# rebuild/sign/notarize/staple the DMG. Produces a distributable that opens on
# any (Apple-Silicon) Mac with no Gatekeeper prompt or quarantine step.
#
#   platforms/macos/sign-notarize.sh
#
# Requires: a "Developer ID Application" identity in the keychain and a
# notarytool keychain profile (default: qlc-notary). Override via env:
#   SIGN_ID, ENT, NOTARY_PROFILE, APP, BUNDLE_ID
set -euo pipefail

REPO="/Volumes/Ext/git/qlcplus"
APP="${APP:-$HOME/QLC+.app}"
SIGN_ID="${SIGN_ID:-Developer ID Application: Branson Matheson (B4N7HJ7VZ6)}"
ENT="${ENT:-$REPO/platforms/macos/qlcplus.entitlements}"
NOTARY_PROFILE="${NOTARY_PROFILE:-qlc-notary}"
BUNDLE_ID="${BUNDLE_ID:-com.bransonmatheson.qlcplus}"
SCRATCH="${SCRATCH:-/private/tmp/qlc-notarize}"; mkdir -p "$SCRATCH"

step() { echo; echo "===== $* ====="; }
csflags=(--force --options runtime --timestamp --sign "$SIGN_ID")

step "0 Ensure CFBundleIdentifier is set in the built app"
/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$APP/Contents/Info.plist" >/dev/null 2>&1 \
  || /usr/libexec/PlistBuddy -c "Add CFBundleIdentifier string $BUNDLE_ID" "$APP/Contents/Info.plist"
echo "  id: $(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$APP/Contents/Info.plist")"

step "1 Sign nested code (leaf dylibs, plugins, frameworks) — runtime+timestamp"
# loose dylibs in Frameworks (outside any .framework) + all plugins
{ find "$APP/Contents/Frameworks" -type f -name '*.dylib' -not -path '*.framework/*'
  find "$APP/Contents/PlugIns" -type f
} | while IFS= read -r f; do
  file -b "$f" | grep -q 'Mach-O' && codesign "${csflags[@]}" "$f"
done
# framework bundles (codesign signs the versioned binary within)
find "$APP/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0 |
  while IFS= read -r -d '' fw; do codesign "${csflags[@]}" "$fw"; done

step "2 Sign the real executables with entitlements (qlcplus runs the JS/JIT + mic)"
for exe in qlcplus qlcplus-fixtureeditor; do
  [ -f "$APP/Contents/MacOS/$exe" ] && codesign "${csflags[@]}" --entitlements "$ENT" "$APP/Contents/MacOS/$exe"
done

step "3 Seal the app bundle (main exec = launcher) with entitlements"
codesign "${csflags[@]}" --entitlements "$ENT" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP" 2>&1 | tail -3
echo "  --- codesign OK ---"

step "4 Notarize the app"
ZIP="$SCRATCH/QLCplus-app.zip"; rm -f "$ZIP"
/usr/bin/ditto -c -k --keepParent "$APP" "$ZIP"
xcrun notarytool submit "$ZIP" --keychain-profile "$NOTARY_PROFILE" --wait 2>&1 | tee "$SCRATCH/app-submit.log"
grep -q "status: Accepted" "$SCRATCH/app-submit.log" || {
  echo "Notarization NOT accepted; fetching log…"
  sid=$(awk '/id:/{print $2; exit}' "$SCRATCH/app-submit.log")
  xcrun notarytool log "$sid" --keychain-profile "$NOTARY_PROFILE" 2>&1 | tail -40
  exit 1
}

step "5 Staple the app"
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"

step "6 Rebuild the DMG from the stapled app"
APPVERSION=$(awk -F\" '/^#define APPVERSION/ {gsub(/ /,"-",$2); print $2}' "$REPO/engine/src/qlcconfig.h")
BUILD_DATE=$(date -u '+%Y%m%d'); GIT_REV=$(git -C "$REPO" rev-parse --short HEAD)
DIST="$REPO/dist"; mkdir -p "$DIST"
DMG="$DIST/QLC+-${APPVERSION}-${BUILD_DATE}-${GIT_REV}.dmg"
rm -f "$DMG"
( cd "$REPO/platforms/macos/dmg"
  ./create-dmg --volname "Q Light Controller Plus ${APPVERSION}" \
    --volicon "$REPO/resources/icons/qlcplus.icns" \
    --background background.png --window-size 400 300 --window-pos 200 100 \
    --icon-size 64 --icon "QLC+" 0 150 --app-drop-link 200 150 \
    "$DMG" "$APP" )

step "7 Sign + notarize + staple the DMG"
codesign --force --timestamp --sign "$SIGN_ID" "$DMG"
xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait 2>&1 | tee "$SCRATCH/dmg-submit.log"
grep -q "status: Accepted" "$SCRATCH/dmg-submit.log" || { echo "DMG notarization failed"; exit 1; }
xcrun stapler staple "$DMG"
xcrun stapler validate "$DMG"

step "DONE"
echo "App: $APP  (stapled)"
echo "DMG: $DMG  (signed, notarized, stapled)"
spctl -a -vv -t install "$DMG" 2>&1 | head -3 || true
ls -lh "$DMG"
