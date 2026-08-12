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
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO"

VERSION="${1:-$(cat VERSION)}"
TAG="v${VERSION}"

step() { echo; echo "===== $* ====="; }

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

step "1/5 Build + bundle (platforms/macos/package-local.sh)"
platforms/macos/package-local.sh

step "2/5 Sign + notarize + staple (platforms/macos/sign-notarize.sh)"
platforms/macos/sign-notarize.sh

DMG=$(ls -t dist/qlcconsole-*.dmg 2>/dev/null | head -1)
[ -n "$DMG" ] || { echo "No DMG found in dist/ after sign-notarize.sh. Aborting."; exit 1; }
echo "Release artifact: $DMG"

step "3/5 Extract release notes from CHANGELOG.md"
NOTES_FILE="$(mktemp)"
awk -v ver="$VERSION" '
  $0 ~ "^## \\[" ver "\\]" { found=1; next }
  found && /^## \[/ { exit }
  found { print }
' CHANGELOG.md > "$NOTES_FILE"

step "4/5 Tag and push"
git tag -a "$TAG" -m "qlcconsole $TAG"
git push origin "$TAG"

step "5/5 Publish GitHub Release"
gh release create "$TAG" "$DMG" \
  --title "qlcconsole $TAG" \
  --notes-file "$NOTES_FILE"

rm -f "$NOTES_FILE"

echo
echo "Released $TAG: $DMG"
gh release view "$TAG" --web
