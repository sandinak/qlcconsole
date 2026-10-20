#!/bin/bash
# Cut a qlcconsole release: build + bundle, Developer ID sign + notarize,
# tag, and publish a GitHub Release with the signed DMG attached.
#
# See RELEASE.md for the full process (version bump policy, prerequisites).
# This script performs real, externally-visible actions (git tag push,
# public GitHub Release) — run it deliberately, not as part of iteration.
#
#   ./release.sh            # uses the version in VERSION
#   ./release.sh 0.2.0      # override
#   PRERELEASE=1 TITLE="Show Tree 0.2.0" ./release.sh
#                           # a GitHub pre-release (the in-app updater's
#                           # Pre-release channel), with its own title
#   ./release.sh attach dist/qlcconsole-0.2.0-win64-setup.exe \
#                       dist/qlcconsole-0.2.0-x86_64.AppImage
#                           # add CI's files to the release made from
#                           # VERSION, and their lines to its SHA256SUMS
#
# Every release carries SHA256SUMS (sha256sum format, one line per asset).
# The in-app updater (ui/src/updatechecker.cpp) refuses to install a
# download whose checksum is not listed there, and picks its download by
# name: qlcconsole-<v>-universal-*.dmg (macOS), qlcconsole-<v>-win64-setup.exe
# (Windows), qlcconsole-<v>-x86_64.AppImage (Linux, link only).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO"

step() { echo; echo "===== $* ====="; }

# SHA-256 lines for the given files, by base name (what the asset is called).
sums_for() {
  local f
  for f in "$@"; do
    printf '%s  %s\n' "$(shasum -a 256 "$f" | awk '{print $1}')" "$(basename "$f")"
  done
}

if [ "${1:-}" = "attach" ]; then
  shift
  [ $# -gt 0 ] || { echo "usage: $0 attach FILE..."; exit 1; }
  VERSION="$(cat VERSION)"
  TAG="v${VERSION}"
  command -v gh >/dev/null || { echo "gh (GitHub CLI) not found."; exit 1; }
  for f in "$@"; do [ -f "$f" ] || { echo "No such file: $f"; exit 1; }; done
  WORK="$(mktemp -d)"
  gh release download "$TAG" -p SHA256SUMS -D "$WORK" 2>/dev/null || touch "$WORK/SHA256SUMS"
  # Replace any earlier line for the same name, then add the new ones.
  for f in "$@"; do
    grep -v "  $(basename "$f")\$" "$WORK/SHA256SUMS" > "$WORK/SUMS.new" || true
    mv "$WORK/SUMS.new" "$WORK/SHA256SUMS"
  done
  sums_for "$@" >> "$WORK/SHA256SUMS"
  echo "SHA256SUMS:"; cat "$WORK/SHA256SUMS"
  gh release upload "$TAG" "$@" --clobber
  gh release upload "$TAG" "$WORK/SHA256SUMS" --clobber
  rm -rf "$WORK"
  echo "Attached to $TAG: $*"
  exit 0
fi

VERSION="${1:-$(cat VERSION)}"
TAG="v${VERSION}"
PRERELEASE="${PRERELEASE:-0}"
TITLE="${TITLE:-qlcconsole $TAG}"

command -v gh >/dev/null || { echo "gh (GitHub CLI) not found. Install it and run 'gh auth login' first."; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "gh is not authenticated. Run 'gh auth login' first."; exit 1; }

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag $TAG already exists. Bump VERSION (or pass a version override) first."
  exit 1
fi

if ! grep -q "^## \[${VERSION}\]" CHANGELOG.md; then
  echo "CHANGELOG.md has no '## [${VERSION}]' section. Add one before releasing."
  exit 1
fi

step "1/6 Test gate (cmake --build build --target check)"
# Fast pre-flight against the standard dev build dir (Debug, iterated on all
# session) rather than package-local.sh's separate Release build-package/ —
# this is a correctness gate on the codebase, not a rebuild of the exact
# shipping bits, so it should stay cheap and reuse whatever's already built.
# set -e means a non-zero exit here (a real test failure) aborts the whole
# release right here, before any of the expensive/external steps below.
[ -d build ] || cmake -S . -B build
cmake --build build -j"$(sysctl -n hw.ncpu)"
cmake --build build --target check

step "2/6 Build + bundle (platforms/macos/package-local.sh)"
platforms/macos/package-local.sh

step "3/6 Sign + notarize + staple (platforms/macos/sign-notarize.sh)"
platforms/macos/sign-notarize.sh

DMG=$(ls -t dist/qlcconsole-*.dmg 2>/dev/null | head -1)
[ -n "$DMG" ] || { echo "No DMG found in dist/ after sign-notarize.sh. Aborting."; exit 1; }
echo "Release artifact: $DMG"

step "4/6 Extract release notes from CHANGELOG.md"
NOTES_FILE="$(mktemp)"
awk -v ver="$VERSION" '
  $0 ~ "^## \\[" ver "\\]" { found=1; next }
  found && /^## \[/ { exit }
  found { print }
' CHANGELOG.md > "$NOTES_FILE"

step "5/6 Tag and push"
git tag -a "$TAG" -m "qlcconsole $TAG"
git push origin "$TAG"

step "6/6 Publish GitHub Release (with SHA256SUMS)"
# The checksum file the in-app updater verifies every download against.
# CI's Windows installer and AppImage are added later with
# "./release.sh attach ...", which appends their lines.
SUMS_DIR="$(mktemp -d)"
sums_for "$DMG" > "$SUMS_DIR/SHA256SUMS"
cat "$SUMS_DIR/SHA256SUMS"
PRE_FLAG=()
[ "$PRERELEASE" = 1 ] && PRE_FLAG=(--prerelease)
gh release create "$TAG" "$DMG" "$SUMS_DIR/SHA256SUMS" \
  --title "$TITLE" \
  --notes-file "$NOTES_FILE" \
  ${PRE_FLAG[@]+"${PRE_FLAG[@]}"}

rm -f "$NOTES_FILE"
rm -rf "$SUMS_DIR"

echo
echo "Released $TAG: $DMG"
gh release view "$TAG" --web
