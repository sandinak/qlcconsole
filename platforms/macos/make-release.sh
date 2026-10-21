#!/bin/bash
# `make release`: the signed, notarized, stapled universal DMG of HEAD.
#
#   package-local.sh   universal Release build -> ~/qlcconsole.app (the old
#                      one kept as ~/qlcconsole.app.prev) + an ad-hoc DMG
#   sign-notarize.sh   Developer ID sign, notarize (qlc-notary), staple the
#                      app; rebuild, sign, notarize and staple the DMG
#   then               dist/SHA256SUMS for that DMG, and a Gatekeeper check
#
# It builds the checkout it's in, which has to be committed: the DMG is named
# after HEAD's rev, so local edits would ship under a rev that doesn't hold
# them. (test-workspaces/ isn't part of the app and is ignored here.)
# ALLOW_DIRTY=1 builds anyway, for a DMG that is not for release.
#
# It tags and publishes nothing; that's ./release.sh (RELEASE.md).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"
VERSION="$(cat VERSION)"
REV="$(git rev-parse --short HEAD)"
SIGN_ID="${SIGN_ID:-Developer ID Application: Branson Matheson (B4N7HJ7VZ6)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-qlc-notary}"
APP="$HOME/qlcconsole.app"

step() { echo; echo "######## $* ########"; }

step "Pre-flight: $(git rev-parse --abbrev-ref HEAD) @ $REV, qlcconsole $VERSION"
if ! git diff --quiet HEAD -- . ':(exclude)test-workspaces'; then
  if [ "${ALLOW_DIRTY:-0}" = 1 ]; then
    echo "WARNING: building uncommitted changes (ALLOW_DIRTY=1) -- not a release."
  else
    echo "ERROR: uncommitted changes to tracked files:"
    git status --short --untracked-files=no -- . ':(exclude)test-workspaces'
    echo "Commit them (or ALLOW_DIRTY=1 for a throwaway DMG)."
    exit 1
  fi
fi
untracked="$(git ls-files --others --exclude-standard -- . ':(exclude)test-workspaces' | head -5)"
[ -z "$untracked" ] || { echo "Note: untracked files (not in the build unless a glob picks them up):"; echo "$untracked"; }
security find-identity -v -p codesigning | grep -qF "$SIGN_ID" ||
  { echo "ERROR: no '$SIGN_ID' signing identity in the keychain (RELEASE.md, prerequisites)."; exit 1; }
xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1 ||
  { echo "ERROR: notarytool profile '$NOTARY_PROFILE' missing or not working:"
    echo "  xcrun notarytool store-credentials $NOTARY_PROFILE --apple-id <Apple ID> --team-id B4N7HJ7VZ6"; exit 1; }
echo "  signing identity and notary profile OK"
echo "  ~/qlcconsole.app will be replaced (backup: ~/qlcconsole.app.prev)"

step "Build + bundle (package-local.sh)"
REPO="$REPO" "$REPO/platforms/macos/package-local.sh"

step "Sign + notarize + staple (sign-notarize.sh)"
REPO="$REPO" APP="$APP" SIGN_ID="$SIGN_ID" NOTARY_PROFILE="$NOTARY_PROFILE" \
  "$REPO/platforms/macos/sign-notarize.sh"

DMG="$(ls -t "$REPO"/dist/qlcconsole-"$VERSION"-universal-*-"$REV".dmg 2>/dev/null | head -1)"
[ -n "$DMG" ] || { echo "ERROR: no dist/qlcconsole-$VERSION-universal-*-$REV.dmg after signing."; exit 1; }

step "Checksums + verification"
# The format release.sh uploads, and the in-app updater checks against.
( cd "$REPO/dist" && shasum -a 256 "$(basename "$DMG")" ) > "$REPO/dist/SHA256SUMS"
cat "$REPO/dist/SHA256SUMS"
xcrun stapler validate "$DMG"
spctl -a -t open --context context:primary-signature -v "$DMG"
spctl -a -t exec -v "$APP"
echo "  app version: $("$APP/Contents/MacOS/qlcconsole" --version 2>/dev/null | grep -m1 '^qlcconsole ' || echo '?')"

echo
echo "DMG:        $DMG"
echo "SHA256SUMS: $REPO/dist/SHA256SUMS"
echo "Installed:  $APP  (previous: $APP.prev)"
echo "/Applications/qlcconsole.app is untouched. To update it: quit qlcconsole, then"
echo "  open \"$DMG\"   and drag qlcconsole onto Applications (Replace), or"
echo "  rm -rf /Applications/qlcconsole.app && ditto \"$APP\" /Applications/qlcconsole.app"
echo "Publishing is separate: make publish / ./release.sh (RELEASE.md)."
