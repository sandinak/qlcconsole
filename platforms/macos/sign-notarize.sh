#!/bin/bash
# Developer ID sign + notarize + staple an already-built ~/qlcconsole.app, then
# rebuild/sign/notarize/staple the DMG. Produces a distributable that opens on
# any (Apple-Silicon) Mac with no Gatekeeper prompt or quarantine step.
#
#   platforms/macos/sign-notarize.sh
#   NOTARIZE=0 platforms/macos/sign-notarize.sh   sign the app only (steps
#              0-3): what `make rc` installs on our own Macs for testing. No
#              Apple round trip, no DMG; never published.
#
# Requires: a "Developer ID Application" identity in the keychain and a
# notarytool keychain profile (default: qlc-notary). Override via env:
#   SIGN_ID, ENT, NOTARY_PROFILE, APP, BUNDLE_ID
# Notarizing is notarize-lib.sh: submit, then our own polling (notarytool's
# --wait crashed twice on 2026-11-13). Stopped while the app's submission was
# in flight: rerun with NZ_SUBMISSION_APP=<id> (printed) to wait on it rather
# than upload again -- its ticket is keyed by the code hash, which signing
# the same build again keeps. The DMG is made anew on every run, so its
# submission always is too.
set -euo pipefail

# The checkout this script is in (VERSION, the DMG's rev and dist/ come from it).
REPO="${REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
APP="${APP:-$HOME/qlcconsole.app}"
SIGN_ID="${SIGN_ID:-Developer ID Application: Branson Matheson (B4N7HJ7VZ6)}"
ENT="${ENT:-$REPO/platforms/macos/qlcconsole.entitlements}"
NOTARY_PROFILE="${NOTARY_PROFILE:-qlc-notary}"
BUNDLE_ID="${BUNDLE_ID:-com.bransonmatheson.qlcconsole}"
SCRATCH="${SCRATCH:-/private/tmp/qlc-notarize}"; mkdir -p "$SCRATCH"

NOTARIZE="${NOTARIZE:-1}"
. "$REPO/platforms/macos/notarize-lib.sh"

step() { echo; echo "===== $* ====="; }

# Before anything is signed or uploaded: the profile, in words.
if [ "$NOTARIZE" = 1 ]; then nz_check_profile || exit 1; fi
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

step "2 Sign the real executables with entitlements (qlcconsole runs the JS/JIT + mic)"
for exe in qlcconsole qlcconsole-fixtureeditor; do
  [ -f "$APP/Contents/MacOS/$exe" ] && codesign "${csflags[@]}" --entitlements "$ENT" "$APP/Contents/MacOS/$exe"
done

step "3 Seal the app bundle (main exec = launcher) with entitlements"
codesign "${csflags[@]}" --entitlements "$ENT" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP" 2>&1 | tail -3
echo "  --- codesign OK ---"

if [ "$NOTARIZE" = 0 ]; then
  step "DONE (NOTARIZE=0: signed, not notarized)"
  echo "App: $APP  (Developer ID signed; not notarized or stapled: for our own Macs,"
  echo "     copied over ssh so no quarantine applies -- never published)"
  exit 0
fi

step "4 Notarize the app"
ZIP="$SCRATCH/QLCplus-app.zip"; rm -f "$ZIP"
/usr/bin/ditto -c -k --keepParent "$APP" "$ZIP"
nz_notarize "$ZIP" app || exit 1

step "5 Staple the app"
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"

step "6 Rebuild the DMG from the stapled app"
# Named as package-local.sh names it: the release, then what it runs on.
RELEASE=$(cat "$REPO/VERSION")
case "$(lipo -archs "$APP/Contents/MacOS/qlcconsole")" in
  *x86_64*arm64*|*arm64*x86_64*) FLAVOUR=universal ;;
  *) FLAVOUR=$(lipo -archs "$APP/Contents/MacOS/qlcconsole")
     # `make dmg ARCH=native`: named so nobody ships it by mistake.
     [ "${QLC_ARCH:-}" = native ] && FLAVOUR="$FLAVOUR-native" ;;
esac
BUILD_DATE=$(date -u '+%Y%m%d'); GIT_REV=$(git -C "$REPO" rev-parse --short HEAD)
DIST="$REPO/dist"; mkdir -p "$DIST"
DMG="$DIST/qlcconsole-${RELEASE}-${FLAVOUR}-${BUILD_DATE}-${GIT_REV}.dmg"
rm -f "$DMG"
# --skip-jenkins: no Finder AppleScript on the real screen (package-local.sh).
DMG_STYLE=(--skip-jenkins); [ "${DMG_FINDER:-0}" = 1 ] && DMG_STYLE=()
( cd "$REPO/platforms/macos/dmg"
  ./create-dmg "${DMG_STYLE[@]}" --volname "qlcconsole ${RELEASE}" \
    --volicon "$REPO/resources/icons/qlcconsole.icns" \
    --background background.png --window-size 400 300 --window-pos 200 100 \
    --icon-size 64 --icon "qlcconsole" 0 150 --app-drop-link 200 150 \
    "$DMG" "$APP" )

step "7 Sign + notarize + staple the DMG"
codesign --force --timestamp --sign "$SIGN_ID" "$DMG"
NZ_SUBMISSION_DMG="" nz_notarize "$DMG" dmg || exit 1
xcrun stapler staple "$DMG"
xcrun stapler validate "$DMG"

step "DONE"
echo "App: $APP  (stapled)"
echo "DMG: $DMG  (signed, notarized, stapled)"
spctl -a -vv -t install "$DMG" 2>&1 | head -3 || true
ls -lh "$DMG"
