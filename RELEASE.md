# Release process

qlcconsole releases are published to
[GitHub Releases](https://github.com/sandinak/qlcconsole/releases):

- **macOS** — a universal DMG (Apple silicon + Intel, macOS 12+), built,
  Developer ID signed and notarized **locally** by `make beta` /
  `make release` (`platforms/macos/publish.sh`).
- **Windows** — the NSIS installer (`…-win64-setup.exe`) built by CI's
  `build-windows` job. Not code-signed.
- **Linux** — the x86_64 AppImage built by CI's `build-linux` job
  (`build-v4`).

The Windows and Linux files are downloaded from the CI run of the release
commit and attached afterwards with `make attach` (step 5 below).

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

## make targets

From the repo root (`make help` for the details). The first four stay on
this Mac; **`make beta`, `make release` and `make attach` are public**.

- `make test` — builds the checkout (`build-dev/`, Homebrew Qt) and installs
  `~/Applications/qlcconsole-dev.app`, ad-hoc signed, for running on this Mac.
  Not a release, not distributable, and it runs no unit tests (`make check`).
- `make run SHOW=…` — `make test`, then launch it with `-o SHOW`.
- `make check` — `./check-all.sh`, the unit-test gate.
- `make dmg` — `platforms/macos/make-release.sh`: `package-local.sh` +
  `sign-notarize.sh` on a committed HEAD, then `dist/SHA256SUMS` and a
  Gatekeeper check. It **replaces `~/qlcconsole.app`**, keeping the old one
  as `~/qlcconsole.app.prev`, and leaves `/Applications` alone (open the DMG
  and drag to update that one). No gate, no tag, nothing published. The DMG
  is made without driving Finder (`create-dmg --skip-jenkins`: a plain
  window, no background layout); `DMG_FINDER=1` brings back the laid-out
  window, at the cost of an AppleScript opening the volume on the screen.
  It ends with the pre-release check of the new DMG (below), and fails if
  that fails; `RELEASE_CHECK=0` skips it.
- `make release-check` — the pre-release check of the newest `dist/*.dmg`
  (`DMG=…` for another, `HOST=<ssh-host>` to run it on another Mac,
  `WAIVE=id,…`); see [Pre-release checks](#pre-release-checks). `make dmg`
  runs it at the end by itself.
- `make beta` — **public**: a GitHub **pre-release** (the updater's
  Pre-release channel), titled "Show Tree <VERSION> (beta)".
- `make release` — **public**: a full GitHub release (Stable channel: every
  user is offered it), titled "qlcconsole <VERSION>".
- `make attach FILES="…"` — **public**: add files (CI's Windows installer
  and AppImage) to the v<VERSION> release, and their lines to its
  `SHA256SUMS` (re-uploaded). Asks you to type `attach`.

`make beta` and `make release` run `platforms/macos/publish.sh`:

1. Pre-flight, all read-only, and it refuses on any of: uncommitted
   changes to tracked files (`test-workspaces/` aside); HEAD on no origin
   branch (push first); tag `v<VERSION>` already on origin or with a GitHub
   release (unless `REPLACE=1`); no or an empty `## [<VERSION>]` CHANGELOG
   section; no signing identity / notary profile; `~/qlcconsole.app`
   running. A section still dated `Unreleased` is a warning.
2. Shows the plan and the full release notes — a short install header,
   then the CHANGELOG section verbatim — and asks you to type `publish`
   (with `REPLACE=1`, `replace v<VERSION>` first). `DRYRUN=1` stops here.
3. The test gate: `./check-all.sh` on a `git archive` copy of HEAD in
   `$TMPDIR`, deleted afterwards pass or fail (three full builds; the disk
   has filled before). `SKIP_GATE=1` skips it.
4. `make dmg` (or `DMG=dist/….dmg`: an already-built, stapled DMG of HEAD).
5. With `REPLACE=1`, only now deletes the old release and tag. Tags HEAD
   `v<VERSION>` (annotated, message = the title) and pushes the tag.
6. Creates the release as a **draft** (not public), uploads the DMG and
   `SHA256SUMS`, downloads both back from GitHub and checks the DMG against
   `SHA256SUMS`; then makes it public and prints its URL. (`DRAFT=1` stops
   at the draft — for testing the script.)

If it fails after the tag push, it says what exists and how to clean up;
running again with `REPLACE=1` is the usual fix. `TITLE=`, `INTRO=` (the
notes' first line) and `NOTES_FILE=` override the defaults.

It uses only GitHub's REST API (`gh api`). `gh release create` once failed
with "GraphQL: API rate limit already exceeded" while REST had quota.

`./release.sh` still works and runs the same script (`PRERELEASE=1
./release.sh` = `make beta`; `./release.sh attach …` = `make attach`).

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
4. `DRYRUN=1 make beta` (or `make release`) to read the plan and notes,
   then `make beta` / `make release` (above). It runs the gate, builds,
   signs and notarizes the universal DMG
   (`qlcconsole-<VERSION>-universal-<date>-<rev>.dmg`), tags `vX.Y.Z`,
   and publishes the release with the DMG and `SHA256SUMS`, verified by
   downloading them back.
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
   make attach FILES="dist/qlcconsole-X.Y.Z-win64-setup.exe dist/qlcconsole-X.Y.Z-x86_64.AppImage"
   ```

   `attach` uploads the files (replacing any of the same name), adds their
   lines to the release's `SHA256SUMS` (re-uploaded), checks GitHub's
   copies against it, and rewrites the notes' "Windows and Linux builds:"
   line to say what is now attached. `DRYRUN=1` shows the new `SHA256SUMS`
   only. The in-app updater refuses any download
   whose checksum is not listed there, so attach CI's files this way, not
   with a bare `gh release upload`.

   CI's names carry the build date and commit (and, for the AppImage, the
   build id rather than the release number); the copies above give users
   plain names. Before attaching, read CI's `release-check-windows` and
   `release-check-linux` results (or run the checks yourself, below): any
   FAIL there is a release bug.
6. Sanity-check the release page, then announce it.

`make beta` / `make release` / `make attach` push a tag and publish on
GitHub — real, visible actions, and the in-app updater offers the result
to users. Run them deliberately, not as part of routine iteration.

## Pre-release checks

One command per platform proves an installer, and what it installs, is
right before anything is published. Each prints TAP (and writes
`release-check.tap` + `release-check.json`) with a summary at the end, and
exits non-zero on any failure. All three pass with no waiver as of 0.3.0
(RC8). A failure is a release bug: fix it. If one must ship anyway, waive
it by id (`--waive id,id` / `-Waive id,id`, or `RELEASE_CHECK_WAIVE=id,id`
for `make dmg` / `make release-check`): it is then reported as
`TODO waived` with its reason, never hidden, and the waiver goes in the
release notes. None of them ever opens a window --
every run of the app is `QT_QPA_PLATFORM=offscreen` + `-platform offscreen`
with its own HOME/settings -- or launches a quarantined app.

| Platform | Command | Where |
|---|---|---|
| macOS | `platforms/macos/release-check.sh <dmg> [--host <ssh-host>]` (`make release-check`) | here, and a clean Mac with `--host` (e.g. EvoAudio, Intel macOS 13) |
| Windows | `powershell -ExecutionPolicy Bypass -File platforms\windows\release-check.ps1 <setup.exe>` | the Windows VM (under `~/qlcconsole-windows`), and CI's `build-windows` job |
| Linux | `platforms/linux/release-check.sh <AppImage> [--host <ssh-host>]` | CI's `build-v4` job, and a clean Ubuntu 22.04 / 24.04 with no Qt (below) |

Every installer ships Qt's offscreen platform plugin, so the checks run the
app headless with what it ships. (`--offscreen-plugin` / `-OffscreenDir`
supply one only to check an older installer that lacks it.)

**macOS** (`make dmg` runs it on the DMG it made; `RELEASE_CHECK_HOST=evo`
also runs it on another Mac):
- the DMG: SHA256SUMS line, Developer ID signature and Team, stapled
  ticket, Gatekeeper (`spctl -t open`), its layout;
- the app: `codesign --verify --deep --strict`, Team, timestamp, stapled
  ticket, Gatekeeper, hardened runtime on every Mach-O, entitlements of the
  three executables equal to `qlcconsole.entitlements`;
- every Mach-O (read by a small Perl reader, no Xcode tools needed):
  universal, no absolute library path outside the OS, only `@loader_path` /
  `@executable_path` rpaths, every `@rpath` reference inside the bundle, no
  binary newer than `LSMinimumSystemVersion`;
- Qt plugins (cocoa, offscreen, styles, iconengines, imageformats, tls,
  multimedia; sqldrivers if anything links QtSql), the I/O plugins, the
  audio plugin, FFmpeg's licence (LGPL, not `--enable-gpl`) and its text in
  `Resources/licenses/`;
- Info.plist: bundle id, version = VERSION, build = the binary's, minimum
  macOS, executable, icon, usage strings, every document type with a UTI
  declared for it (`UTExportedTypeDeclarations` for .qlcc,
  `UTImportedTypeDeclarations` for QLC+'s .qxw and .qxf);
- resources: FixturesMap.xml against the files, no definitions loose in
  Fixtures/, no Finder-duplicate files (`* 2.qxf`), and every file in
  Fixtures, RGBScripts, MidiTemplates, ModifiersTemplates, Bundles,
  InputProfiles, EffectScripts, EffectPresets, Gobos, DeviceProfiles,
  HIDProfiles and licenses byte-identical to the repo's (`res.from-repo.*`;
  a manifest of the checkout's `resources/`, sent along with `--host`):
  nothing from the builder's home folder; Sample.qxw, the data folders,
  translations, the help guides compiled into libqlcplusui;
- file types: a copy registered with `lsregister -f`, its claimed types read
  back, the default handler for .qlcc (with `--ls-real`, the default for
  `--host`, the copy is registered outside the temp dirs so it can become
  the handler), then unregistered;
- Gatekeeper on a quarantined copy, with `spctl` only;
- headless: `--version` on each slice this Mac can run and a load of
  Sample.qxw, on a copy (quarantine stripped, re-signed ad hoc) with the
  bundle's own offscreen plugin -- the I/O and audio plugins must load;
- and no crash report, no LaunchServices record of its copies, nothing left
  (the remote temp dir included).

**Windows** (`-SkipTakeover` skips the second install): Authenticode
(unsigned is reported as expected for now), the installer's version, a
silent install (`/S /D=`), the files, every EXE/DLL import resolvable and
x64 (a PE reader, no dumpbin), the offscreen plugin, FFmpeg's configure
line (LGPL, not `--enable-gpl`) and its licence text in `licenses\`, the
Apps & features entry (name, version, publisher,
InstallLocation, UninstallString), the .qlcc/.qxw/.qxf associations (ProgID,
icon, quoted open command), the Start Menu shortcuts' targets, `--version`
and a headless load with QtMultimedia's FFmpeg backend loaded, then
uninstall `/S` and NOTHING left: files, entry, associations, shortcuts,
`HKCU\Software\qlcconsole` and the app's own settings
`HKCU\Software\qlcplus\qlcconsole`; and a second silent install into
another folder must be refused (exit 3) without touching the first. (An
interactive install that finds qlcconsole in another folder asks: install
there instead, or go back.) Every key and Start Menu item it can touch is
exported first and restored after; a qlcconsole already installed on the
machine has its entry hidden for the run and restored.

**Linux**: `--appimage-extract`, every ELF's dependencies with AppRun's
library path (none missing, no Qt from the system, nothing beyond a base
desktop system taken from the machine, RPATH only `$ORIGIN`-relative), the
.desktop file (`desktop-file-validate`, icon, `Exec=qlcconsole %f` with
nothing before the user's arguments, MimeType), the MIME definitions for
.qlcc/.qxw/.qxf, AppStream metainfo, Qt plugins (xcb, offscreen, jpeg, gif,
ico, svg, svgicon, ffmpeg) and I/O plugins, data, FFmpeg's licence and its
text, `--version` both directly and through AppRun, and a headless load of
Sample.qxw through AppRun with the FFmpeg multimedia backend loaded.

What makes the AppImage self-contained is `platforms/linux/appimage-bundle.sh`
(CI runs it after `cmake --install`): the Qt plugins above, then every
library any ELF in the tree needs except the base system of a desktop Linux
(glibc, libstdc++, GL/X11/xcb, fontconfig, freetype, glib, dbus,
ALSA/PulseAudio, libusb, libudev -- the same list the check accepts), then
`$ORIGIN`-relative RUNPATHs. The real test is a machine with none of the
build's libraries: a clean Ubuntu 22.04 (and 24.04) with only that base
system installed -- no Qt, no FFmpeg, no libftdi/FFTW/libmad. On the Linux
build host these are minbase debootstrap chroots, `/srv/clean-jammy` and
`/srv/clean-noble` (podman is not installed there):

    sudo chroot /srv/clean-jammy bash /tmp/rc/release-check.sh /tmp/rc/<AppImage> \
        --version <VERSION> --rev <rev> --out /tmp/rc/out

after copying the AppImage and `release-check.sh` into `/tmp/rc` inside it
(and bind-mounting /proc, /sys and /dev). The chroots hold the base system
the bundler leaves out and nothing else: GL/EGL/OpenGL, X11 and the core
xcb libraries, xkbcommon, fontconfig, freetype, harfbuzz, glib, dbus,
ALSA, PulseAudio (with libsndfile), libusb, libudev, krb5's GSSAPI, plus
`file`, `binutils` and `procps` for the check. `--version`/`--rev` are
needed there because CI's AppImage name carries the build id, not the
release.

In CI the Windows and Linux checks run after the build and upload their
results (`release-check-windows`, `release-check-linux`). They fail the job
only when the repository variable `RELEASE_CHECK_BLOCKING` is `true`
(Settings > Secrets and variables > Actions > Variables); with it unset a
failure shows as a warning on a green job. All three pass since RC8: set it
to `true`.

## What the in-app updater needs from a release

The updater (`ui/src/updatechecker.cpp`, docs/features/updates.md) reads
`api.github.com/repos/sandinak/qlcconsole/releases`, and it needs these
things from each release:

- **Tag** `vX.Y.Z`, a SemVer version. A tag that is not a version is
  ignored.
- **Pre-release flag** for builds meant only for the Pre-release channel,
  such as Show Tree for students. VERSION stays plain X.Y.Z; a build whose
  own release carries the flag follows the Pre-release channel by itself:
  `make beta` (title "Show Tree X.Y.Z (beta)"; the course install page
  looks for a pre-release whose name starts with "Show Tree").
- **Asset names**, which is how it picks its download:
  - macOS: `qlcconsole-<v>-universal-<date>-<rev>.dmg`
  - Windows: `qlcconsole-<v>-win64-setup.exe`
  - Linux (linked, not installed): `qlcconsole-<v>-x86_64.AppImage`
- **`SHA256SUMS`**, in `sha256sum` format, with one line for every asset
  above. `make beta` / `make release` write it for the DMG, and
  `make attach` adds the rest.
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
