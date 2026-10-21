#!/bin/bash
# `make beta` / `make release` / `make attach`: publish qlcconsole to GitHub
# Releases. Everything here is PUBLIC: a pushed tag and a release page that
# the in-app updater (ui/src/updatechecker.cpp) offers to every user on that
# channel. RELEASE.md has the whole process.
#
#   publish.sh beta          GitHub pre-release "Show Tree <VERSION> (beta)"
#                            (the updater's Pre-release channel)
#   publish.sh release       full release "qlcconsole <VERSION>" (Stable)
#   publish.sh attach FILE.. add files (CI's Windows installer and AppImage)
#                            to the release of v<VERSION>, and their lines to
#                            its SHA256SUMS
#
# beta / release, in order:
#   1. pre-flight, read-only: VERSION and its CHANGELOG section, a committed
#      HEAD that is on origin, the tag not on origin yet, gh logged in,
#      signing identity and notary profile (make-release.sh's checks)
#   2. the plan and the release notes, then ONE typed confirmation
#      ("publish"; with REPLACE=1 also "replace v<VERSION>")
#   3. the test gate: ./check-all.sh on a `git archive` copy of HEAD in
#      $TMPDIR, deleted afterwards whatever happens (SKIP_GATE=1 skips it)
#   4. make-release.sh: signed, notarized, stapled universal DMG of HEAD in
#      dist/ + dist/SHA256SUMS; replaces ~/qlcconsole.app (as `make dmg`)
#   5. annotated tag v<VERSION> on HEAD, pushed to origin
#   6. a DRAFT release (not public), the DMG and SHA256SUMS uploaded to it,
#      both downloaded back from GitHub and checked against SHA256SUMS
#   7. the draft made public; its URL printed
#
# Only GitHub's REST API is used (gh api), never GraphQL: `gh release create`
# has failed on "GraphQL: API rate limit already exceeded" with REST quota
# to spare.
#
# Env:
#   DRYRUN=1     pre-flight, plan and notes only: nothing is built or published
#   SKIP_GATE=1  no check-all.sh run
#   REPLACE=1    the tag (and its release) already exist: delete both and
#                publish again -- only after the new DMG is built and verified
#   TITLE=...    the release name (defaults above)
#   INTRO=...    the first line of the notes (beta default: pre-release note)
#   DMG=path     publish this already-built DMG of HEAD (e.g. from `make dmg`)
#                instead of building one; it must be named for VERSION and
#                HEAD's rev and pass stapler validation
#   NOTES_FILE=  notes body instead of the CHANGELOG section
#   DRAFT=1      stop at step 6: leave a draft (for testing this script)
#   GH_REPO      owner/name (default sandinak/qlcconsole)
#   TAG          another tag than v<VERSION> (for testing; VERSION itself
#                must match the VERSION file, which names the DMG)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

GH_REPO="${GH_REPO:-sandinak/qlcconsole}"
VERSION="${VERSION:-$(cat VERSION)}"
TAG="${TAG:-v$VERSION}"
DRYRUN="${DRYRUN:-0}"
SKIP_GATE="${SKIP_GATE:-0}"
REPLACE="${REPLACE:-0}"
DRAFT="${DRAFT:-0}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/qlcconsole-publish.XXXXXX")"
GATE_DIR=""
STAGE=""        # what has been done publicly so far, for the failure message
RELEASE_ID=""

step() { echo; echo "######## $* ########"; }
die()  { echo "ERROR: $*" >&2; exit 1; }

cleanup() {
  local rc=$?
  [ -n "$GATE_DIR" ] && rm -rf "$GATE_DIR"
  rm -rf "$WORK"
  if [ $rc -ne 0 ] && [ -n "$STAGE" ]; then
    echo
    echo "FAILED after: $STAGE"
    [ -n "$RELEASE_ID" ] && echo "  Draft release $RELEASE_ID is still there (not public):" &&
      echo "    gh api -X DELETE repos/$GH_REPO/releases/$RELEASE_ID"
    echo "  Tag $TAG is pushed. Fix the problem and run again with REPLACE=1, or remove it:"
    echo "    git push origin :refs/tags/$TAG && git tag -d $TAG"
  fi
  exit $rc
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# --- GitHub REST helpers ----------------------------------------------------

# The id of the release (draft or not) for a tag, or nothing.
release_id_for() {
  gh api --paginate "repos/$GH_REPO/releases?per_page=100" \
    --jq ".[] | select(.tag_name == \"$1\") | .id"
}

asset_json() { # <release-id> -> "id name" lines
  gh api --paginate "repos/$GH_REPO/releases/$1/assets?per_page=100" --jq '.[] | "\(.id) \(.name)"'
}

ctype_for() {
  case "$1" in
    SHA256SUMS|*.txt) echo "text/plain" ;;
    *)                echo "application/octet-stream" ;;
  esac
}

# Upload (replacing an asset of the same name).
upload_asset() { # <release-id> <file>
  local rid="$1" f="$2" name id n
  name="$(basename "$f")"
  while read -r id n; do
    [ "$n" = "$name" ] || continue
    gh api -X DELETE "repos/$GH_REPO/releases/assets/$id" >/dev/null || die "could not delete the old $name"
    echo "  replaced the old $name"
  done < <(asset_json "$rid")
  echo "  uploading $name ($(du -h "$f" | awk '{print $1}'))"
  gh api -X POST "https://uploads.github.com/repos/$GH_REPO/releases/$rid/assets?name=$name" \
    -H "Content-Type: $(ctype_for "$name")" --input "$f" --jq .browser_download_url >/dev/null
}

download_asset() { # <release-id> <name> <out>
  local id
  id="$(asset_json "$1" | awk -v n="$2" '$2 == n {print $1; exit}')"
  [ -n "$id" ] || { echo "  the release has no asset $2" >&2; return 1; }
  gh api -H "Accept: application/octet-stream" "repos/$GH_REPO/releases/assets/$id" > "$3"
}

# Download the named assets and check them against the release's SHA256SUMS
# (which must also equal <local-sums>, when given).
verify_assets() { # <release-id> <local-sums-or-empty> <name>...
  local rid="$1" sums="$2" d="$WORK/verify"; shift 2
  rm -rf "$d"; mkdir -p "$d"
  download_asset "$rid" SHA256SUMS "$d/SHA256SUMS" || die "no SHA256SUMS on the release"
  [ -z "$sums" ] || cmp -s "$d/SHA256SUMS" "$sums" || die "SHA256SUMS on GitHub differs from the local one"
  for n in "$@"; do
    echo "  downloading $n back from GitHub"
    download_asset "$rid" "$n" "$d/$n" || die "cannot download $n"
    ( cd "$d" && grep "  $n\$" SHA256SUMS | shasum -a 256 -c - ) || die "$n from GitHub fails SHA256SUMS"
    rm -f "$d/$n"
  done
}

# The "Windows and Linux builds:" line of the notes, from what is attached.
platforms_line() { # <asset names...>
  local win="" lin="" a
  for a in "$@"; do
    case "$a" in *-win64-setup.exe) win=1 ;; *.AppImage) lin=1 ;; esac
  done
  if [ -n "$win" ] && [ -n "$lin" ]; then
    echo "Windows and Linux builds: attached below — the Windows installer is not code-signed (SmartScreen warns on first run); the Linux AppImage is x86_64. Raspberry Pi users can build from source (tag $TAG)."
  elif [ -n "$win" ]; then
    echo "Windows and Linux builds: the Windows installer is attached below (not code-signed: SmartScreen warns on first run); Linux not attached yet — Linux / Raspberry Pi users can build from source (tag $TAG)."
  elif [ -n "$lin" ]; then
    echo "Windows and Linux builds: the Linux AppImage (x86_64) is attached below; Windows not attached yet. Raspberry Pi users can build from source (tag $TAG)."
  else
    echo "Windows and Linux builds: not attached yet; Linux / Raspberry Pi users can build from source (tag $TAG)."
  fi
}

# Typed confirmation; anything else stops.
confirm() { # <prompt> <word>
  local a=""
  read -r -p "$1 Type '$2' to go on: " a || true
  [ "$a" = "$2" ] || { echo "Not confirmed -- nothing done."; exit 1; }
}

gh_ready() {
  command -v gh >/dev/null || die "gh (GitHub CLI) not found."
  gh auth status >/dev/null 2>&1 || die "gh is not logged in: gh auth login"
}

# --- attach -----------------------------------------------------------------

if [ "${1:-}" = attach ]; then
  shift
  [ $# -gt 0 ] || die "usage: make attach FILES=\"dist/qlcconsole-$VERSION-win64-setup.exe ...\""
  for f in "$@"; do [ -f "$f" ] || die "no such file: $f"; done
  gh_ready
  rid="$(release_id_for "$TAG")"
  [ -n "$rid" ] || die "no GitHub release for $TAG"
  echo "Release $TAG (id $rid), assets now:"; asset_json "$rid" | awk '{print "  " $2}'
  download_asset "$rid" SHA256SUMS "$WORK/SHA256SUMS" >/dev/null 2>&1 || : > "$WORK/SHA256SUMS"
  for f in "$@"; do  # an earlier line for the same name goes
    grep -v "  $(basename "$f")\$" "$WORK/SHA256SUMS" > "$WORK/s.new" || true
    mv "$WORK/s.new" "$WORK/SHA256SUMS"
  done
  for f in "$@"; do
    printf '%s  %s\n' "$(shasum -a 256 "$f" | awk '{print $1}')" "$(basename "$f")" >> "$WORK/SHA256SUMS"
  done
  echo; echo "New SHA256SUMS:"; cat "$WORK/SHA256SUMS"
  if [ "$DRYRUN" = 1 ]; then echo; echo "DRYRUN: nothing uploaded."; exit 0; fi
  echo
  confirm "Upload $# file(s) and the new SHA256SUMS to the $TAG release ($GH_REPO)?" attach
  for f in "$@"; do upload_asset "$rid" "$f"; done
  upload_asset "$rid" "$WORK/SHA256SUMS"
  names=(); for f in "$@"; do names+=("$(basename "$f")"); done
  verify_assets "$rid" "$WORK/SHA256SUMS" "${names[@]}"
  # Keep the notes' "Windows and Linux builds:" line true.
  gh api "repos/$GH_REPO/releases/$rid" --jq .body > "$WORK/body.md"
  # (bash 3.2: no mapfile; asset names have no spaces)
  line="$(platforms_line $(asset_json "$rid" | awk '{print $2}'))"
  # tag_name goes with every PATCH: GitHub turns a draft patched without it
  # into "untagged-<hash>".
  python3 -I - "$WORK/body.md" "$line" "$TAG" > "$WORK/req.json" <<'PY'
import json, re, sys
body = open(sys.argv[1]).read()
new = re.sub(r'(?m)^Windows and Linux builds:.*$', lambda m: sys.argv[2], body, count=1)
print(json.dumps({"body": new, "tag_name": sys.argv[3]}) if new != body else "{}")
PY
  if [ "$(cat "$WORK/req.json")" != "{}" ]; then
    gh api -X PATCH "repos/$GH_REPO/releases/$rid" --input "$WORK/req.json" >/dev/null
    echo "  notes: $line"
  fi
  echo; echo "Attached to $TAG: $*"
  echo "$(gh api "repos/$GH_REPO/releases/$rid" --jq .html_url)"
  exit 0
fi

# --- beta / release ---------------------------------------------------------

KIND="${1:-}"
case "$KIND" in
  beta)    PRERELEASE=true;  TITLE="${TITLE:-Show Tree $VERSION (beta)}"
           INTRO="${INTRO-**Pre-release** — please report problems.}" ;;
  release) PRERELEASE=false; TITLE="${TITLE:-qlcconsole $VERSION}"
           INTRO="${INTRO-}" ;;
  *) die "usage: $0 beta|release|attach FILE..." ;;
esac
SHA="$(git rev-parse HEAD)"
REV="$(git rev-parse --short HEAD)"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"

step "Pre-flight ($KIND $TAG, $BRANCH @ $REV)"
problems=()
gh_ready
[ "$VERSION" = "$(cat VERSION)" ] ||
  problems+=("VERSION=$VERSION but the VERSION file says $(cat VERSION): the build names the DMG from the file -- bump it and commit")
[[ "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+ ]] || problems+=("$TAG is not vX.Y.Z (the updater ignores it)")

if ! git diff --quiet HEAD -- . ':(exclude)test-workspaces'; then
  problems+=("uncommitted changes to tracked files:")
  while read -r l; do problems+=("    $l"); done < <(git status --short --untracked-files=no -- . ':(exclude)test-workspaces')
fi

git fetch --quiet origin || die "git fetch origin failed"
[ -n "$(git branch -r --contains HEAD 2>/dev/null)" ] ||
  problems+=("HEAD ($REV) is on no origin branch -- push it first (git push origin $BRANCH)")

ON_ORIGIN="$(git ls-remote --tags origin "refs/tags/$TAG" | awk '{print $1}')"
OLD_RID="$(release_id_for "$TAG")"
if [ -n "$ON_ORIGIN" ] || [ -n "$OLD_RID" ]; then
  if [ "$REPLACE" = 1 ]; then
    echo "  REPLACE=1: tag $TAG${ON_ORIGIN:+ (on origin)}${OLD_RID:+ and release $OLD_RID} will be deleted and recreated"
  else
    problems+=("$TAG already exists${ON_ORIGIN:+ on origin}${OLD_RID:+ with GitHub release $OLD_RID} -- bump VERSION, or REPLACE=1")
  fi
fi
if [ -z "$ON_ORIGIN" ] && git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && [ "$REPLACE" != 1 ] &&
   { [ "$(git rev-parse "$TAG^{commit}")" != "$SHA" ] || [ "$(git cat-file -t "$TAG")" != tag ]; }; then
  problems+=("a local tag $TAG exists that is not an annotated tag on HEAD: git tag -d $TAG")
fi

if [ -n "${NOTES_FILE:-}" ]; then
  [ -f "$NOTES_FILE" ] || problems+=("NOTES_FILE $NOTES_FILE not found")
  NOTES_SRC="$NOTES_FILE"
else
  NOTES_SRC="CHANGELOG.md [$VERSION]"
  awk -v ver="$VERSION" '
    index($0, "## [" ver "]") == 1 { found=1; next }
    found && /^## \[/ { exit }
    found { print }
  ' CHANGELOG.md > "$WORK/changelog.md"
  grep -q '[^[:space:]]' "$WORK/changelog.md" || problems+=("CHANGELOG.md has no (or an empty) '## [$VERSION]' section")
  grep -q "^## \[$VERSION\] - Unreleased" CHANGELOG.md &&
    echo "  WARNING: CHANGELOG still says '## [$VERSION] - Unreleased' (RELEASE.md step 2: date it)"
fi

if [ -n "${DMG:-}" ]; then
  [ -f "$DMG" ] || problems+=("DMG $DMG not found")
  case "$(basename "$DMG")" in
    "qlcconsole-$VERSION-universal-"*"-$REV.dmg") ;;
    *) problems+=("DMG must be qlcconsole-$VERSION-universal-<date>-$REV.dmg (HEAD's build)") ;;
  esac
  [ -f "$DMG" ] && { xcrun stapler validate "$DMG" >/dev/null 2>&1 || problems+=("$DMG is not stapled"); }
else
  # (uncommitted changes are reported above; ALLOW_DIRTY keeps them out of this)
  if out="$(ALLOW_DIRTY=1 PREFLIGHT_ONLY=1 platforms/macos/make-release.sh 2>&1)"; then
    echo "  make-release.sh pre-flight OK (signing identity, notary profile)"
  else
    problems+=("make-release.sh pre-flight:")
    while read -r l; do problems+=("    $l"); done < <(echo "$out" | grep -A3 '^ERROR')
  fi
fi

if [ ${#problems[@]} -gt 0 ]; then
  echo; echo "Refusing to publish:"; printf '  %s\n' "${problems[@]}"
  exit 1
fi
echo "  pre-flight OK"

# The notes: a short install header, then the CHANGELOG section verbatim.
{
  [ -n "$INTRO" ] && { echo "$INTRO"; echo; }
  echo "**Install (macOS, Intel or Apple silicon):** open the DMG and drag qlcconsole to Applications. Signed and notarized by Branson Matheson (B4N7HJ7VZ6). Verify with \`shasum -a 256\` against SHA256SUMS."
  echo
  platforms_line
  echo
  if [ -n "${NOTES_FILE:-}" ]; then cat "$NOTES_FILE"; else cat "$WORK/changelog.md"; fi
} > "$WORK/notes.md"

step "Plan"
cat <<EOF
  repo         $GH_REPO
  kind         $([ "$PRERELEASE" = true ] && echo "PRE-RELEASE (updater's Pre-release channel)" || echo "FULL RELEASE (updater's Stable channel: every user is offered it)")
  title        $TITLE
  tag          $TAG (annotated) on $BRANCH @ $REV, pushed to origin
  notes        install header + $NOTES_SRC ($(wc -l < "$WORK/notes.md" | tr -d ' ') lines, below)
  gate         $([ "$SKIP_GATE" = 1 ] && echo "SKIPPED (SKIP_GATE=1)" || echo "./check-all.sh on a git-archive copy of HEAD (deleted after)")
  DMG          $([ -n "${DMG:-}" ] && echo "$DMG (prebuilt)" || echo "built now by make-release.sh -> dist/qlcconsole-$VERSION-universal-$(date -u +%Y%m%d)-$REV.dmg; replaces ~/qlcconsole.app")
  assets       the DMG + SHA256SUMS, downloaded back and verified
  replace      $([ "$REPLACE" = 1 ] && echo "YES: delete ${OLD_RID:+release $OLD_RID and }tag $TAG first (after the build)" || echo no)
  ends as      $([ "$DRAFT" = 1 ] && echo "a DRAFT (DRAFT=1, not public)" || echo "PUBLIC release")
EOF
echo
echo "---------------- release notes ----------------"
cat "$WORK/notes.md"
echo "-----------------------------------------------"

if [ "$DRYRUN" = 1 ]; then
  echo
  echo "DRYRUN: nothing built, tagged or published. Without DRYRUN=1 this asks"
  echo "for 'publish'$([ "$REPLACE" = 1 ] && echo " and 'replace $TAG'"), then runs the steps above."
  exit 0
fi

echo
[ "$REPLACE" = 1 ] && [ -n "$ON_ORIGIN$OLD_RID" ] &&
  confirm "REPLACE deletes the existing $TAG tag${OLD_RID:+ and its GitHub release} (download counts, links to its assets)." "replace $TAG"
confirm "$([ "$DRAFT" = 1 ] && echo "Push tag $TAG and create a DRAFT release '$TITLE'?" || echo "Publish '$TITLE' ($TAG) to $GH_REPO?")" publish

if [ "$SKIP_GATE" != 1 ]; then
  step "Test gate: check-all.sh on an archive copy of HEAD"
  GATE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/qlcconsole-gate.XXXXXX")"
  git archive --format=tar HEAD | tar -x -C "$GATE_DIR"
  echo "  $GATE_DIR"
  ( cd "$GATE_DIR" && ./check-all.sh ) || die "the test gate failed -- nothing published"
  rm -rf "$GATE_DIR"; GATE_DIR=""
  echo "  gate passed; archive copy deleted"
fi

if [ -z "${DMG:-}" ]; then
  step "Build: make-release.sh"
  platforms/macos/make-release.sh
  DMG="$(ls -t dist/qlcconsole-"$VERSION"-universal-*-"$REV".dmg 2>/dev/null | head -1)"
  [ -n "$DMG" ] || die "no dist/qlcconsole-$VERSION-universal-*-$REV.dmg after make-release.sh"
fi
[ "$(git rev-parse HEAD)" = "$SHA" ] || die "HEAD moved during the build -- nothing published"
( cd "$(dirname "$DMG")" && shasum -a 256 "$(basename "$DMG")" ) > "$WORK/SHA256SUMS"
echo "  SHA256SUMS: $(cat "$WORK/SHA256SUMS")"

step "Tag $TAG"
if [ "$REPLACE" = 1 ]; then
  if [ -n "$OLD_RID" ]; then
    gh api -X DELETE "repos/$GH_REPO/releases/$OLD_RID" >/dev/null || die "could not delete release $OLD_RID"
    echo "  deleted release $OLD_RID"
  fi
  if [ -n "$ON_ORIGIN" ]; then
    git push origin ":refs/tags/$TAG" || die "could not delete origin's $TAG"
    echo "  deleted origin's $TAG"
  fi
  git tag -d "$TAG" >/dev/null 2>&1 || true
fi
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  echo "  reusing the local tag $TAG (on HEAD)"
else
  git tag -a "$TAG" -m "$TITLE"
fi
STAGE="tag push"
git push origin "refs/tags/$TAG"

step "Draft release, upload, verify"
python3 -I - "$WORK/notes.md" "$TAG" "$SHA" "$TITLE" "$PRERELEASE" > "$WORK/req.json" <<'PY'
import json, sys
notes, tag, sha, title, pre = sys.argv[1:]
print(json.dumps({"tag_name": tag, "target_commitish": sha, "name": title,
                  "body": open(notes).read(), "draft": True,
                  "prerelease": pre == "true"}))
PY
RELEASE_ID="$(gh api -X POST "repos/$GH_REPO/releases" --input "$WORK/req.json" --jq .id)"
STAGE="tag push, draft release $RELEASE_ID"
echo "  draft release $RELEASE_ID"
upload_asset "$RELEASE_ID" "$DMG"
upload_asset "$RELEASE_ID" "$WORK/SHA256SUMS"
verify_assets "$RELEASE_ID" "$WORK/SHA256SUMS" "$(basename "$DMG")"
echo "  verified: GitHub's copies match SHA256SUMS"

if [ "$DRAFT" = 1 ]; then
  STAGE=""
  echo
  echo "DRAFT=1: left as a draft (not public): $(gh api "repos/$GH_REPO/releases/$RELEASE_ID" --jq .html_url)"
  echo "  delete it:  gh api -X DELETE repos/$GH_REPO/releases/$RELEASE_ID"
  echo "  and the tag: git push origin :refs/tags/$TAG && git tag -d $TAG"
  exit 0
fi

step "Publish"
# tag_name too: a draft PATCHed without it becomes "untagged-<hash>".
gh api -X PATCH "repos/$GH_REPO/releases/$RELEASE_ID" -f tag_name="$TAG" -F draft=false >/dev/null
got="$(gh api "repos/$GH_REPO/releases/$RELEASE_ID" --jq '"\(.tag_name) \(.draft) \(.prerelease)"')"
[ "$got" = "$TAG false $PRERELEASE" ] || die "the release reads back as '$got', not '$TAG false $PRERELEASE'"
STAGE=""
URL="$(gh api "repos/$GH_REPO/releases/$RELEASE_ID" --jq .html_url)"
echo
echo "Published $TITLE ($TAG): $URL"
echo "Next: attach CI's Windows installer and AppImage (RELEASE.md step 5):"
echo "  make attach FILES=\"dist/qlcconsole-$VERSION-win64-setup.exe dist/qlcconsole-$VERSION-x86_64.AppImage\""
