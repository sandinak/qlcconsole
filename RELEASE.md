# Release process

qlcconsole ships macOS releases, built and signed locally, published to
[GitHub Releases](https://github.com/sandinak/qlcconsole/releases).

## Two independent version numbers

This repo tracks two separate version numbers on purpose — don't confuse
them:

1. **`VERSION` (repo root)** — the qlcconsole release number, e.g. `0.1.0`.
   Bumped manually, once per release. This is what gets tagged (`v0.1.0`)
   and what shows up in the GitHub Release title and the DMG volume label.
2. **`APPVERSION`** (computed in [`variables.cmake`](variables.cmake), shown
   in the About box and `--version`) — `<QLC_BASE_VERSION>+qlcconsole.<N>`,
   where `N` is the git commit count since the `QLC+_4.14.2` fork point.
   This is fully automatic; you never edit it directly. `QLC_BASE_VERSION`
   only changes when the fork is rebased onto a newer upstream QLC+ tag —
   that's a deliberate, separate decision from cutting a qlcconsole release,
   so it's not tied to bumping `VERSION`.

In short: `VERSION` answers "what qlcconsole release is this," `APPVERSION`
answers "what's actually in the binary and what QLC+ base is it built on."
A release's notes should mention both.

## Cutting a release

Prerequisites (one-time, already done on this machine):
- A **Developer ID Application** certificate in your login keychain.
- A `notarytool` keychain profile named `qlc-notary` (see
  `platforms/macos/sign-notarize.sh` for how it's used;
  `xcrun notarytool store-credentials qlc-notary` to (re)create it).
- `gh` (GitHub CLI), authenticated (`gh auth status`).

Steps:

0. Run `./check-all.sh` — the full test gate against **both** Qt majors.
   `release.sh`'s own gate builds only whichever Qt cmake picks up, and this
   fork ships Qt6 DMGs while development happens against whatever is
   installed. Three defects in one week came from that split (see the header
   comment in `check-all.sh`). CI's macOS job builds against Qt6 only and has
   no Qt5 leg, so this script is the only cross-Qt coverage there is.
1. Bump `VERSION` to the new release number.
2. Add a `## [X.Y.Z] - YYYY-MM-DD` section to [CHANGELOG.md](CHANGELOG.md)
   summarizing what shipped since the last release (pull the user-facing
   highlights from [DONE.md](DONE.md)).
3. Commit those two changes.
4. Run `./release.sh`. This:
   - runs the test gate (`cmake --build build --target check` — engine +
     UI unit tests, fixture-definition validation) and **stops here** if
     anything fails, before any of the slower/external steps below run,
   - builds + bundles via `platforms/macos/package-local.sh`,
   - Developer ID signs, notarizes, and staples via
     `platforms/macos/sign-notarize.sh`,
   - tags `vX.Y.Z` and pushes the tag,
   - creates a GitHub Release with the signed DMG attached and release notes
     pulled straight from that version's `CHANGELOG.md` section.
5. Sanity-check the release page, then announce it.

`release.sh` pushes a tag and creates a public GitHub Release — real,
visible actions. Run it deliberately, not as part of routine iteration.

## Signing: local, not CI

Release builds are signed and notarized **on this machine**, not in GitHub
Actions. The Developer ID private key stays in the local keychain and is
never exported as a CI secret. `.github/workflows/build.yml` remains
plain CI (build + test on push/PR) and is unrelated to publishing releases.

## Out of scope for now

- **Linux / Windows packaging** (`platforms/linux/qlcconsole.spec`,
  `debian/changelog`, the NSIS scripts under `platforms/windows/`) — these
  are inherited from upstream QLC+ and are stale (e.g. the `.spec` file
  still reads a `QLCPLUS_VERSION` env var from the old build). qlcconsole's
  releases are macOS-only until one of those platforms becomes an actual
  target; at that point these files need their own version/branding pass
  before they're usable.
- **CI-based release builds** — could move build+sign+notarize into GitHub
  Actions later by exporting the cert as a secret, but that trades local key
  custody for CI convenience; not worth it while releases are infrequent and
  built by one person.
