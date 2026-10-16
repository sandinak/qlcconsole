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

Prerequisites (one-time, per machine that releases; done on `ender`
2026-10-15 -- a Developer ID certificate made in Xcode → Settings → Accounts →
Manage Certificates, and `xcrun notarytool store-credentials qlc-notary
--apple-id <Apple ID> --team-id B4N7HJ7VZ6` with an app-specific password):
- A **Developer ID Application** certificate in your login keychain.
- A `notarytool` keychain profile named `qlc-notary` (see
  `platforms/macos/sign-notarize.sh` for how it's used;
  `xcrun notarytool store-credentials qlc-notary` to (re)create it).
- `gh` (GitHub CLI), authenticated (`gh auth status`).
- For the **universal** build (Apple silicon + Intel, the default):
  - the official universal Qt, from aqt, at `~/Qt-aqt/6.9.3/macos`
    (`QTDIR` to put it elsewhere):
    `python3 -m venv /tmp/aqt && /tmp/aqt/bin/pip install aqtinstall &&
    /tmp/aqt/bin/aqt install-qt mac desktop 6.9.3 clang_64 -O ~/Qt-aqt -m
    qt3d qtimageformats qtmultimedia qtserialport qtwebsockets qtshadertools`
    -- Homebrew's Qt is this Mac's arch only;
  - the plugins' libraries (libusb, libftdi, fftw, libsndfile) built
    universal into `~/unideps`: `platforms/macos/universal-deps.sh`.

  `UNIVERSAL=0 platforms/macos/package-local.sh` is the old Homebrew build,
  for this Mac's arch only. The universal build has no OLA plugin
  (Homebrew's OLA is single-arch).

Steps:

0. Run `./check-all.sh` — the full test gate: Qt6 Debug, Release (one
   shipped bug only showed at -O2) and warnings-as-errors.
1. Bump `VERSION` to the new release number.
2. Add a `## [X.Y.Z] - YYYY-MM-DD` section to [CHANGELOG.md](CHANGELOG.md)
   summarizing what shipped since the last release (pull the user-facing
   highlights from [DONE.md](DONE.md)).
3. Commit those two changes.
4. Run `./release.sh`. This:
   - runs the test gate (`cmake --build build --target check` — engine +
     UI unit tests, fixture-definition validation) and **stops here** if
     anything fails, before any of the slower/external steps below run,
   - builds + bundles via `platforms/macos/package-local.sh` -- one
     universal app; the DMG is `qlcconsole-<VERSION>-universal-<date>-<rev>.dmg`,
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

- **Linux packaging** (`platforms/linux/qlcconsole.spec`,
  `debian/changelog`) — inherited from upstream QLC+ and stale (e.g. the
  `.spec` file still reads a `QLCPLUS_VERSION` env var from the old build).
  qlcconsole's releases are macOS-only until Linux becomes an actual target;
  at that point these files need their own version/branding pass.
- **Windows** — CI's `build-windows` job builds an NSIS installer
  (`platforms/windows/qlcconsole.nsi.in`, version from `VERSION`) on every
  push and uploads it as an artifact. It is not part of a release: unsigned,
  and not yet tried on a real Windows machine (see TODO.md).
- **CI-based release builds** — could move build+sign+notarize into GitHub
  Actions later by exporting the cert as a secret, but that trades local key
  custody for CI convenience; not worth it while releases are infrequent and
  built by one person.
