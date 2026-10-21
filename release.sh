#!/bin/bash
# Kept so old habits still work: publishing is `make beta` / `make release`
# (platforms/macos/publish.sh), the one way to publish -- see RELEASE.md.
#
#   ./release.sh                      = make release  (full GitHub release)
#   PRERELEASE=1 ./release.sh         = make beta     (GitHub pre-release)
#   ./release.sh attach FILE...       = make attach FILES="FILE..."
#
# TITLE, DRYRUN, SKIP_GATE, REPLACE, DMG ... pass through (publish.sh).
# CI does not use this script.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "${1:-}" = attach ]; then
  shift
  exec "$REPO/platforms/macos/publish.sh" attach "$@"
fi
[ -z "${1:-}" ] || { echo "The version comes from the VERSION file now: bump it and commit."; exit 1; }
if [ "${PRERELEASE:-0}" = 1 ]; then
  exec "$REPO/platforms/macos/publish.sh" beta
fi
exec "$REPO/platforms/macos/publish.sh" release
