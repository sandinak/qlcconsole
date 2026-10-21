# Release process

qlcconsole releases are published to
[GitHub Releases](https://github.com/sandinak/qlcconsole/releases):

- **macOS** — a universal DMG (Apple silicon + Intel, macOS 12+), built,
  Developer ID signed and notarized **locally** by `./release.sh`.
- **Windows** — the NSIS installer (`…-win64-setup.exe`) built by CI's
  `build-windows` job. Not code-signed.
- **Linux** — the x86_64 AppImage built by CI's `build-linux` job
  (`build-v4`).

The Windows and Linux files are downloaded from the CI run of the release
commit and attached to the release `release.sh` created (step 5 below).

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

## make test / make release

From the repo root (`make help` for the details):

- `make test` — builds the checkout (`build-dev/`, Homebrew Qt) and installs
  `~/Applications/qlcconsole-dev.app`, ad-hoc signed, for running on this Mac.
  Not a release, not distributable, and it runs no unit tests (`make check`).
- `make release` — steps 2–3 of `./release.sh` without the tag or the
  GitHub Release: `package-local.sh` + `sign-notarize.sh` on a committed HEAD,
  then `dist/SHA256SUMS` and a Gatekeeper check. It **replaces
  `~/qlcconsole.app`**, keeping the old one as `~/qlcconsole.app.prev`, and
  leaves `/Applications` alone (open the DMG and drag to update that one).
  It doesn't run the test gate; `make check` first. The DMG is made without
  driving Finder (`create-dmg --skip-jenkins`: a plain window, no background
  layout); `DMG_FINDER=1` brings back the laid-out window, at the cost of an
  AppleScript opening the volume on the screen.
- `make publish` — prints what `./release.sh` will do, asks you to type
  `publish`, then runs it (it rebuilds; it does not reuse `make release`'s DMG).

The scripts build the checkout they're in, so a worktree releases itself.

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
   highlights from [DONE.md](DONE.md)), with its Known issues. A section
   written ahead as `- Unreleased` gets its date now. The section is the
   release notes, verbatim.
3. Commit those two changes, and merge the release branch (e.g.
   `release/0.1.0`) to `main`; release from `main`. Push it, so CI builds
   the Windows installer and the AppImage from that commit.
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
5. Attach the Windows installer and the Linux AppImage from CI. Wait for
   the CI run of the release commit to finish (`build-windows` and the
   `build-v4` Linux job green), then:

   ```sh
   RUN=$(gh run list --workflow build.yml --commit "$(git rev-parse HEAD)" \
         --limit 1 --json databaseId -q '.[0].databaseId')
   gh run download "$RUN" -D dist/ci \
      -p 'qlcconsole-*-win64-setup.exe' -p 'build-v4-AppImage'
   cp dist/ci/qlcconsole-*-win64-setup.exe/*.exe dist/qlcconsole-X.Y.Z-win64-setup.exe
   cp dist/ci/build-v4-AppImage/*.AppImage dist/qlcconsole-X.Y.Z-x86_64.AppImage
   ./release.sh attach dist/qlcconsole-X.Y.Z-win64-setup.exe \
      dist/qlcconsole-X.Y.Z-x86_64.AppImage
   ```

   `attach` uploads the files and adds their lines to the release's
   `SHA256SUMS` (re-uploaded). The in-app updater refuses any download
   whose checksum is not listed there, so attach CI's files this way, not
   with a bare `gh release upload`.

   CI's names carry the build date and commit (and, for the AppImage, the
   build id rather than the release number); the copies above give users
   plain names. Check the installer's `--version` says X.Y.Z (CI's smoke
   test already does) and, if the Windows VM is free, install it there.
6. Sanity-check the release page, then announce it.

`release.sh` pushes a tag and creates a public GitHub Release — real,
visible actions. Run it deliberately, not as part of routine iteration.

## What the in-app updater needs from a release

The updater (`ui/src/updatechecker.cpp`, docs/features/updates.md) reads
`api.github.com/repos/sandinak/qlcconsole/releases`, and it needs these
things from each release:

- **Tag** `vX.Y.Z`, a SemVer version. A tag that is not a version is
  ignored.
- **Pre-release flag** for builds meant only for the Pre-release channel,
  such as Show Tree for students. VERSION stays plain X.Y.Z; a build whose
  own release carries the flag follows the Pre-release channel by itself:
  `PRERELEASE=1 TITLE="Show Tree X.Y.Z" ./release.sh`.
- **Asset names**, which is how it picks its download:
  - macOS: `qlcconsole-<v>-universal-<date>-<rev>.dmg`
  - Windows: `qlcconsole-<v>-win64-setup.exe`
  - Linux (linked, not installed): `qlcconsole-<v>-x86_64.AppImage`
- **`SHA256SUMS`**, in `sha256sum` format, with one line for every asset
  above. `release.sh` writes it for the DMG, and `release.sh attach` adds
  the rest.
- On macOS, the DMG's app must be signed with the same Developer ID Team
  (B4N7HJ7VZ6) and bundle id as the installed one, and must pass
  Gatekeeper. That is what sign-notarize.sh produces.

The release body is shown as the update's notes, rendered as Markdown.

## Signing: local, not CI

macOS release builds are signed and notarized **on this machine**, not in
GitHub Actions. The Developer ID private key stays in the local keychain and is
never exported as a CI secret. `.github/workflows/build.yml` remains
plain CI (build + test on push/PR) and is unrelated to publishing releases.

## Out of scope for now

- **Linux distribution packages** (`platforms/linux/qlcconsole.spec`,
  `debian/changelog`) — inherited from upstream QLC+ and stale (the `.spec`
  still reads a `QLCPLUS_VERSION` env var; `debian/changelog` is upstream's
  4.14.x history). Linux ships as CI's AppImage only; these files need their
  own version/branding pass before an .rpm or .deb is offered.
- **Windows code signing** — the installer is unsigned, so SmartScreen
  warns on first run. It has been installed and used on a Windows 10 VM
  (2026-10-15/16), not yet on a real Windows 11 machine (TODO.md).
- **CI-based release builds** — could move build+sign+notarize into GitHub
  Actions later by exporting the cert as a secret, but that trades local key
  custody for CI convenience; not worth it while releases are infrequent and
  built by one person.
