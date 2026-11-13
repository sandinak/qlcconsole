#!/bin/bash
# `make rc HOSTS="dink kronk"`: a test build of HEAD on our own Macs, now --
# built, Developer ID signed, installed as /Applications/qlcconsole.app on each
# HOST. No notarization, no DMG, nothing published.
#
#   package-local.sh        universal Release build -> ~/qlcconsole.app
#                           (NO_DMG=1; ARCH=native: arm64 only, half the compile)
#   sign-notarize.sh        NOTARIZE=0: sign only (steps 0-3)
#   install-test-app.sh     zip, ssh, codesign-verify, install on each HOST
#
# Testing never needed notarization: a copy over ssh has no quarantine flag.
# The DMG to publish is still `make dmg` (notarized; it can run while the
# testing does). On 2026-11-13 the test RC waited on notarytool for an hour.
#
# Pre-flight: committed tree (as make dmg), the signing identity, and HEAD
# holding all of origin/main as GitHub has it now (fetched): an RC was nearly
# built from a main a push behind, because the clone's origin/main was stale.
# ALLOW_DIRTY=1 / ALLOW_BEHIND=1 build anyway.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"
SIGN_ID="${SIGN_ID:-Developer ID Application: Branson Matheson (B4N7HJ7VZ6)}"
APP="$HOME/qlcconsole.app"
[ $# -gt 0 ] || { echo "usage: make rc HOSTS=\"dink kronk local\" [ARCH=native]"; exit 2; }

step() { echo; echo "######## $* ########"; }
step "Pre-flight: $(git rev-parse --abbrev-ref HEAD) @ $(git rev-parse --short HEAD), qlcconsole $(cat VERSION)"
if ! git diff --quiet HEAD -- . ':(exclude)test-workspaces'; then
  if [ "${ALLOW_DIRTY:-0}" = 1 ]; then echo "WARNING: uncommitted changes (ALLOW_DIRTY=1)"
  else echo "ERROR: uncommitted changes to tracked files (commit, or ALLOW_DIRTY=1)"; exit 1; fi
fi
if "$REPO/tools/gitsync.sh" fetch; then
  if ! git merge-base --is-ancestor origin/main HEAD; then
    n=$(git rev-list --count HEAD..origin/main)
    if [ "${ALLOW_BEHIND:-0}" = 1 ]; then echo "WARNING: HEAD lacks $n commit(s) of origin/main (ALLOW_BEHIND=1)"
    else echo "ERROR: HEAD lacks $n commit(s) of origin/main -- merge it first (or ALLOW_BEHIND=1)"; exit 1; fi
  fi
  echo "  HEAD holds origin/main ($(git rev-parse --short origin/main))"
else
  [ "${ALLOW_BEHIND:-0}" = 1 ] || { echo "ERROR: could not fetch origin to check HEAD against main (ALLOW_BEHIND=1 to build anyway)"; exit 1; }
fi
security find-identity -v -p codesigning | grep -qF "$SIGN_ID" ||
  { echo "ERROR: no '$SIGN_ID' signing identity in the keychain"; exit 1; }
if pgrep -f "^$APP/Contents/MacOS/" >/dev/null; then
  echo "ERROR: $APP is running -- quit it first (the build replaces it)"; exit 1
fi

step "Build (package-local.sh, no DMG)"
NO_DMG=1 QLC_ARCH="${QLC_ARCH:-}" REPO="$REPO" "$REPO/platforms/macos/package-local.sh"

step "Sign (sign-notarize.sh, NOTARIZE=0)"
NOTARIZE=0 REPO="$REPO" APP="$APP" SIGN_ID="$SIGN_ID" "$REPO/platforms/macos/sign-notarize.sh"

step "Install on: $*"
"$REPO/platforms/macos/install-test-app.sh" --app "$APP" "$@"
echo
echo "Test build $(git rev-parse --short HEAD) installed. Not notarized (for our own Macs);"
echo "the DMG to publish: make dmg."
