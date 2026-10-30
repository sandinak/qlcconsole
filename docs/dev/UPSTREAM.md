# Upstream intake

qlcconsole stands on [QLC+](https://github.com/mcallegari/qlcplus). Each
release, upstream's fixes and fixture definitions are reviewed and taken
in, with the upstream commit recorded, and the review is logged here:
what was taken, what was adapted by hand, what was skipped and why.

Upstream is read-only from here: nothing is pushed, committed or opened
against `mcallegari/qlcplus`. Fixes worth sending back go through the
user, as ordinary upstream contributions.

## Where the fork stands

| | |
|---|---|
| QLC+ base | **4.14.4** (`QLC+_4.14.4` = 027e6d4fd, 2026-03-15), since qlcconsole 0.3.1 |
| Forked from | `QLC+_4.14.2` (2026, the original fork point) |
| Upstream master merged | 9a0deb5cf (2026-02-01), in 1f8d28a5f / d875a4313 |
| Code reviewed up to | upstream master 4cd37df23 (2026-10-29), 2026-10-30 |
| Fixture definitions synced to | upstream master 4cd37df23 (2026-10-29), by `tools/sync-fixtures.py` |

The build string is `<QLC_BASE_VERSION>+qlcconsole.<N>`
([variables.cmake](../../variables.cmake)): the base is 4.14.4, and `N` is
still counted from the original fork point (`QLC+_4.14.2..HEAD`), so
build numbers never go backwards when the base moves (0.3.0 was build
2347). 4.14.4 is not an ancestor of the fork's history: its fixes came in
as cherry-picks, so `git rev-list QLC+_4.14.4..HEAD` would count the
February sync's upstream commits out and drop `N` by about 550.

## Intake log

### 2026-10-30: upstream master after 4.14.4 (qlcconsole 0.3.1)

`QLC+_4.14.4..4cd37df23` is 411 non-merge commits; 247 touch only qmlui,
fixtures, CI, packaging or translations. The other 164 were triaged by
area. Master is QLC+ 5.x with 4.x fixes mixed in, so what shares code
with the fork's rewritten engine (Doc patching, ChaserRunner, ShowRunner,
EFX, RGBMatrix, MasterTimer, Universe) mostly doesn't cherry-pick.
**35 taken** with `git cherry-pick -x`, **2 hand-ported**, the rest
skipped or deferred below.

**Taken** (upstream sha in each commit message):

- Engine: SceneValues of deleted fixtures leave a ChaserStep (0f693838b);
  macro divide by zero (3de28fd48); audio fade-out start level
  (0cd8d2685) and end of stream (fdb9f1636); EFX that froze when fade in
  was not shorter than its duration (521fb3b6d); a Collection lasts as
  long as its longest member (83b673453); RGBMatrix::setTotalDuration
  divide by zero (70bd2e970); paused faders resume when they fade out
  (e683c05b5); a fixture's channels kept when another moves over them
  (78906fddb, e72f6283c); Script duration cached (8bc125f7a); RGBMatrix
  dimmer mode on generic dimmers (6be96e80c); RGB to grey precision
  (b2d87e5fd); v5 scripts terminate (65ac80c9c, 94648f4ae); fixture
  definition live update (c3d96465f).
- Widget UI: DMX dump past four universes (63096cb44); test unit rounding
  (f0463d793, 95b340a7a); audio editor saves fade edits on focus loss
  (4a9481440); Click & Go scaling (aa4b53ffb); VC cue list resets the
  cached submaster on stop (7090cf32a).
- Plugins: OSC feedback path (b40b7e917); Peperoni USBDMX21 detection and
  GCC 16 builds (05f7c4c18, 4214bac7f, 79f91001d, 4a4c05113).
- Web access: keypad parsing (30581dd38).
- Resources: Behringer X-Touch Compact, M-VAVE SMC-PAD / SMC-Mixer
  profiles, prev/next page assignments and the swapped-icon fix
  (1330b6079, 5cedb03f1, 528387870, c21248510, a5fd62ec8); TwinkleFox
  script test fix (1ccdab841).
- Tests: Qt 6.11 font string in RGBText_Test (6ebc60d20). Test hunks of the
  picks above were adapted to the fork's one-argument `Universe::processFaders()`
  and `GenericFader::write()`.

**Hand-ported**: 107e773ae (fixture channels beyond the universe are
skipped when patching) and 2f0c123fb (an EFX entry whose fixture is
missing has no universe) as one-guard changes.

**Skipped, the fork already covers it or the code is gone:**

- Web access hardening 33d41a4e8, 823ff74bb, 724fc4ae8, 448935e15,
  26af840b4: RC8's origin check, `permits()`, `htmlEscaped()`, argument
  counts and `sanitizedFixtureFileName()` already do this.
- 6bf05505b hotspot on Trixie: the fork's `hotspotCommands()` replaced that
  code.
- 4c13ab364 (iOS translate env), 707970034 (Qt5 builds), 9859f9d4b (iOS):
  not our platforms; Qt5 is dropped.
- a124abebe, 4db9faa57: definitions without modes; the fork's cache reloads
  in place and the fixture editor has a Problems check.
- 0169f6bcd encoder input: the fork rewrote relative-encoder decoding.
- rgbscripts 23979ee02, 5c697b330, e5c4a59db: the fork already has these
  scripts (7d724bd29), in its own versions.

**Deferred** (fork code diverged; each needs a hand port with its own test,
none is a known crash on the fork's code):

- ChaserRunner: 26757151d (step advanced in the write that starts it),
  1734a65c9 (submaster on every crossfade step), 3bd4c6c9b (beat rework).
- 43828817d (functions using deleted groups/fixtures; EFX and RGBMatrix),
  8e383d52d (Show items sharing a Function), f57bf8b90 (Doc::saveXML NULL
  checks), 7e2f4ae84 (threading races on reload: MasterTimer/Universe),
  ae1047152 (universe channel setup follows patching: the fork's
  `patchFixtures` replaced the code), bb2746a75 / 0508555b9 (RGBMatrix
  update and phase), b3e82d587 (audio band levels on top of the log bands),
  eb14a184b (scriptrunner), 82d358517 (universe composition; a hot-path
  rewrite touching ten files: a `make perf` job of its own).

**Skipped as QLC+ 5 / new features, not fixes:** qmlui (all), plugin UIDs
(cbad3b03d, 695413170, f6e60c3be, 3785e78bc), Peperoni libusb rework
(2b447118e), beat tracker (c9b014248, fa000cf74, 61d192923,
50c10ae2d), EFX dimmer control (2ad86c3de, 57f6fe851, 6e918aa5c),
RGBMatrix attributes and pattern override (e671868c6, 4ebdfbb7a),
universe split for RGB panels (98c3cb6c4, 6f7f290da), ServiceLoader
(d470ee28a), LightEmitter rename (ff8f746ba), Cue List play on
double-click (afe2c68a9, a behaviour change), `Rename qlcplus-qml` and
packaging, upstream CI, version bumps and translations (the catalogues are
taken with the next 4.x release's).

### 2026-10-29: fixture definitions (qlcconsole 0.3.1)

The first run of `tools/sync-fixtures.py`, from upstream master
a089223b9 to 4cd37df23 (2026-10-29). Master, not a 4.x tag: QLC+ 4 is
in maintenance and new definitions land on master only (`QLC+_4.14.4`
has 1,710; master 1,781); the `.qxf` format and `fixture.xsd` are
unchanged between them. **44 new**, **4 upstream updates** taken (ETC
ColorSource Fresnel V, Pro-Lights CromoWash100, Rockville Motionstrip
RGBW, Stairville CLB2.4), 1 upstream rename followed
(`Stairville-TRI-LED-Bundle-Complete.qxf` →
`Stairville-Stage-TRI-LED-Bundle-Complete.qxf`, same model), the fork's
edited ETC ColorSource Spot and its 10 fork-only files kept, no
conflicts. 1,791 definitions, `scripts/check` clean.

### 2026-10-28: QLC+ 4.14.3 and 4.14.4 (qlcconsole 0.3.1)

`QLC+_4.14.2..QLC+_4.14.4` is 654 commits. 479 (+67 merges) were already
here by ancestry: the February sync merged upstream master at 9a0deb5cf,
which already contains 4.14.3. The 100 newer commits (+8 merges) were
triaged one by one: **21 taken, 3 adapted, 6 already in, 70 skipped**.

**Taken** (`git cherry-pick -x`; partial picks drop only qmlui hunks):

- Engine: startup function reset on New/Open (9756f8c27, #1963); a
  deleted function no longer reached in Collection::totalDuration and
  ShowRunner::write, and the Show function-ID counter fix (6776dbb55);
  Dimmer palettes with a fractional value (822bdffee); Function setters
  that emit only on a change (018bfce76); picture z-index and
  Video::stopFromUI (38f16b4f3); logarithmic audio spectrum bands with a
  smoothed level (b41a1fbe3) -- audio triggers, RGB Audio and effect
  scripts now see 40 Hz-5 kHz in log steps, as stock 4.14.4 does.
- Widget UI: VC slider reset-button fixes (6776dbb55), nested submasters
  (6ad4b7250), speed dial "(Not Sent)" translatable (1aeb13421).
- Plugins: DMX USB udev rules for more FTDI chips (12d47451b); GPIO on
  libgpiod 2, input level and chipset fixes (22913325d, ff5d4340a,
  cbfb28734; the GPIO plugin isn't built by default); SPI string fix
  (a5570c560).
- Resources: Novation Launchpad MK2 input profile and MIDI template
  (fe92ca895, b79dd63aa).
- Translations: ui, webaccess, plugin and fixture-editor catalogues
  (f54ff36b5, 2090bae21, ceb083736, 100d8a89b, bcffa5f94, 7b5f3dec9);
  upstream's `qlcplus_*.ts` map onto our renamed `qlcconsole_*.ts`.

**Adapted** (hand-ported, "Ported from upstream <sha>" in the message):

- 639b26f62 web access null guards: one guard ahead of RC8's origin
  check, which reads the request.
- a7f6bbc63 web pages: valid HTML/CSS, button tooltips, the cue list's
  `</tr>`, upload type filters, the keypad page rewrite; every value
  still goes through RC8's `htmlEscaped()`, and the slider's top label
  is now escaped too (upstream escaped it in 6776dbb55; RC8 had missed
  it).
- e4d8a4a8c, two parts: the web-socket connection's pointers start null,
  clear when the socket goes, the ping timer stops on close, and writes
  go only to a connected socket; and InputOutputMap carries stock
  4.14.4's `<NetworkServer>` settings through a save (TEST_MATRIX C6),
  reset on every load.

**Already in**: c32bf4a87 and f5854a11e (patch-identical commits on main);
the 4.14.4 fixture commits 8bee6aa32, 9301eb2ae, 54d837fd4, 8b84fd34a
(imported in 8fb614a39).

**Skipped**:

- 45 qmlui-only commits and the qmlui hunks of mixed ones (qmlui is off).
- webaccess-v5 / webaccess-qml (built only with qmlui): e8d7a1c81,
  eb3859abd, 0eb6d57f4, d0a0d9c6b, d1e06b654.
- 7aff5af36 (#1974) and 13dfd52a0: const/enum/override refactors with no
  behaviour change, conflicting across the fork's engine and RC8's web
  access; f04a1a023 only undoes part of 7aff5af36.
- QLC+ 5 packaging (22844d210, c224fa1ae, c22a593d2), upstream CI (10
  `.github` commits) and version bumps (10f50c0f6, cfd388ce4, d27206728,
  027e6d4fd).
- 6776dbb55's other webaccess escaping: RC8 (c04eb39ba) already escapes
  those values.

**Deferred**: e4d8a4a8c's web-access ownership change (connections
parented to the TCP server, the web-socket server to the connection, the
TCP socket handed to it, `WebAccessBase::closeServer()`). RC8 fixed the
quit-time crash its own way (`m_socket` a `QPointer`, `~WebAccessBase`
taking its list first); mixing the two models risks a double delete. The
cost of not taking it: one `QWebSocketServer` and `QWebSocket` per
browser session stay allocated until quit. Worth a focused change with
ASan and a browser test.

**Fixture definitions**: 4.14.4's ten new ones were already in
(8fb614a39). Taken now from upstream master (a089223b9): 28 new
definitions and 3 upstream updates (American DJ Jolt 300, Cameo ROOT PAR
6, beamZ LCB244); the fork's own ETC ColorSource Spot and fork-only files
are kept. 1748 definitions, `resources/fixtures/scripts/check` clean.

<details>
<summary>Every commit in 9a0deb5cf..QLC+_4.14.4 (100), with its decision</summary>

| Upstream | Date | Subject | Area | Decision | Reason | Risk |
|---|---|---|---|---|---|---|
| 12d47451b | 2026-02-02 | plugins/dmxusb: update udev rules for more FTDI chipsets | plugins/dmxusb | **TAKE** | udev rules for more FTDI chips (Linux DMX USB) | low |
| 7fd2e30ae | 2026-02-03 | qmlui: fix network client item width | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| e8d7a1c81 | 2026-02-03 | qmlui: comment unused debug function | webaccess | **SKIP** | webaccess-qml.cpp only (built only with qmlui) | n/a |
| 6c97cf054 | 2026-02-03 | qmlui: make simple desk fixture selection persistent | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 8d618997f | 2026-02-04 | qmlui: apply channel modifiers immediately | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 7b5f3dec9 | 2026-02-04 | [Translation] refine German translation (#1945) | fixtureeditor, plugins/E1.31, plugins/artnet, plugins/dmxusb, plugins/enttecwing, plugins/ola, plugins/osc, plugins/udmx | **TAKE** | German plugin / fixture-editor translations | low |
| f54ff36b5 | 2026-02-05 | Update translations (ui, qmlui, webaccess) | qmlui, ui, webaccess | **TAKE** | ui + webaccess catalogues refreshed (applied onto our renamed qlcconsole_*.ts); qmlui catalogues dropped | low |
| a5570c560 | 2025-12-07 | plugins/spi: fix frequency string spelling | plugins/spi | **TAKE** | SPI plugin string fix + catalogues (Linux-only plugin) | low |
| 10f50c0f6 | 2026-02-05 | Enter 5.2.0 release | CMakeLists.txt, platforms, variables.cmake | **SKIP** | upstream version bump (5.2.0); our base bump replaces it | n/a |
| c224fa1ae | 2026-02-05 | windows: remove the QML cache on uninstall | platforms | **SKIP** | QLC+ 5 Windows installer QML cache | n/a |
| 2090bae21 | 2026-02-05 | Updated Catalan and Spanish translations (qmlui, ui and webaccess). | qmlui, ui, webaccess | **TAKE** | Catalan/Spanish ui + webaccess; qmlui dropped | low |
| 1aeb13421 | 2025-12-07 | ui/vcspeeddialfunction: allow translation of speedMultiplierNames | ui | **TAKE** | speed dial "(Not Sent)" translatable; catalogues mapped onto our renamed qlcconsole_*.ts | low |
| cfd388ce4 | 2026-02-06 | Back to 5.2.1 debug | CMakeLists.txt, platforms, variables.cmake | **SKIP** | upstream version bump (5.2.1 debug) | n/a |
| eb3859abd | 2026-02-07 | webaccess: a few improvements to widget styles | webaccess | **SKIP** | webaccess-v5 resources (installed only with qmlui) | n/a |
| 2679a5954 | 2026-02-07 | qmlui: increase the BPM limit to 1000 | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| c32bf4a87 | 2026-02-07 | engine: fix crash on playing cue list with missing media | engine | **ALREADY-IN** | patch-identical commit already on main (git cherry: "-") | none |
| ceb083736 | 2026-02-07 | update German translation | qmlui, ui, webaccess | **TAKE** | German ui + webaccess; qmlui dropped | low |
| 0acfa7ed4 | 2026-02-07 | qmlui: register FontAwesome properly to avoid system conflicts | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| e410c0f11 | 2026-02-07 | actions: add git revision to builds | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| b705af0f0 | 2026-02-07 | actions: use env variable | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| cd638eb70 | 2026-02-08 | actions: move git revision set before build | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| 234bdfd84 | 2026-02-08 | qmlui: fix warnings about non existing font and items | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 71236c8e6 | 2026-02-08 | qmlui: implement video pause | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 13dfd52a0 | 2026-02-06 | webaccess: make more functions const | qmlui, ui, webaccess | **SKIP** | const-only refactor, mostly webaccess-qml (not built); conflicts with RC8 webaccessbase for no behaviour change | n/a |
| 639b26f62 | 2026-02-06 | webaccess: add more nullptr checks | webaccess | **ADAPT** | web access null guards, merged under RC8: one guard before RC8's origin check (which reads the request); RC8 model unchanged | low |
| 0eb6d57f4 | 2026-02-07 | webaccess/webaccess-v5: add support for sliders in percentage and inverted mode | webaccess | **SKIP** | webaccess-v5 resources (qmlui only) | n/a |
| d0a0d9c6b | 2026-02-07 | webaccess/webaccess-v5: immediately update bottomLabel when starting a cue list | webaccess | **SKIP** | webaccess-qml.cpp (qmlui only) | n/a |
| d1e06b654 | 2026-02-07 | webaccess/webaccess-v5: fix simple desk slider override flickering | webaccess | **SKIP** | simpledesk-v5.js (qmlui only) | n/a |
| 22844d210 | 2026-02-08 | windows: fix deploy of web interface resources | platforms | **SKIP** | QLC+ 5 Windows installer (qlcplus5Qt6.nsi); not ours | n/a |
| f8944352c | 2026-02-10 | qmlui: completed input profile editor | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| fe92ca895 | 2026-02-10 | resources: updated Novation Launchpad MK2 input profile with color table and channels | resources/inputprofiles | **TAKE** | input profile update (Launchpad MK2 colours/channels) | low |
| 41848f1b1 | 2026-02-11 | qmlui: fix 2D view selection offset when zommed in | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| ad5a4dda4 | 2026-02-11 | qmlui: improve spliview divider thickness | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| b79dd63aa | 2026-02-11 | resources: add Novation LaunchPad MK2 welcome message | resources/miditemplates | **TAKE** | MIDI template (Launchpad MK2 welcome message) | low |
| c231e0b33 | 2026-02-11 | qmlui: improve Chaser Editor step insertion and scrollbar | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 01315836a | 2026-02-12 | qmlui: improve command line open option to accept no filename | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| c22a593d2 | 2026-02-12 | linux: use dedicated desktop and xml file for v5 | platforms | **SKIP** | QLC+ 5 Linux desktop/metainfo files | n/a |
| 7a8cf3572 | 2026-02-13 | qmlui: improve last item click reset | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| b2cc34fbe | 2026-02-13 | qmlui: implement Show item snapping | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| ca164891b | 2026-02-13 | qmlui: fix VC Speed dial tap with an external controller | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 77c7ea7c7 | 2026-02-13 | qmlui: fix input profile editor wizard on Windows | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| f5854a11e | 2026-02-13 | fix crash on script runner shutdown | engine | **ALREADY-IN** | patch-identical commit already on main (git cherry: "-") | none |
| 6776dbb55 | 2026-02-13 | engine/ui/qmlui: straight fixes from #1955 | engine, qmlui, ui, webaccess | **TAKE** | engine null-function guards in Collection::totalDuration and ShowRunner::write (crash on a deleted function), Show function-ID counter fix, VCSlider reset-button fixes; webaccess hunk already covered by RC8 escaping except the slider top label (taken in the a7f6bbc63 port); qmlui hunks dropped | low |
| 38f16b4f3 | 2026-02-14 | qmlui: fix picture z-index and content stopping | engine, qmlui | **TAKE** | engine hunk only: Video picture z-index, Video::stopFromUI(); qmlui hunks dropped | low |
| 018bfce76 | 2026-02-14 | qmlui: stop all functions when exiting without hanging | engine, qmlui | **TAKE** | engine hunk only: Function fade/duration setters emit changed() only on a change; qmlui hunks dropped | low |
| 82d1cf28d | 2026-02-14 | qmlui: fix import from project | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 1930d4615 | 2026-02-16 | qmlui: fix VC page deletion not dimming out | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 822bdffee | 2026-02-17 | engine: fix dimmer palette as percentage | engine | **TAKE** | Dimmer palette value read as a number with fraction (forum t=19298); one-line conflict beside the fork's Aim case | low |
| 722511e9f | 2026-02-17 | qmlui: add auto-set duration in Chaser Editor & improved selection | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| a7f6bbc63 | 2026-02-18 | improve static webaccess code/resources (#1954) | webaccess | **ADAPT** | valid HTML/CSS, button tooltips, </tr> fix, upload type filters, keypad rewrite; merged into RC8 escaping (htmlEscaped/esc kept, slider top label newly escaped) | low |
| e4d8a4a8c | 2026-02-19 | qmlui: save network server settings in InputOutputMap (fix #1942) | engine, qmlui, webaccess | **ADAPT** | qhttpserver part only: web-socket pointers initialised, cleared on destroy, ping timer stopped on close, write only when connected. Network-server settings in InputOutputMap skipped (qmlui feature). Ownership re-parenting DEFERRED (RC8 has its own model) | medium |
| 6ad4b7250 | 2026-02-20 | qmlui,ui: improve nested submasters | qmlui, ui | **TAKE** | nested VC submasters: frame passes intensity down and re-emits (widget UI); qmlui hunks dropped | low |
| 9e001dc04 | 2026-02-20 | actions: attempt to sign macOS builds | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| 6ca42ee18 | 2026-02-20 | actions: attempt to sign macOS build | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| f0ef5ddd5 | 2026-02-20 | actions: attempt to sign macOS build | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| d0109d3e8 | 2026-02-21 | actions: attempt to sign macOS builds | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| d381067e1 | 2026-02-21 | actions: give up with signing macOS builds | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| f7926ae8e | 2026-02-21 | qmlui: protect VideoProvider against invalid pointers | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 0a415e5b0 | 2026-02-21 | qmlui: fix VC Slider adjust mode | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 50de47e23 | 2026-02-21 | actions: fix build workflow | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| 44229b336 | 2026-02-21 | qmlui: apply submaster value on project load | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 9756f8c27 | 2026-02-21 | engine: reset startup Function on project change (fix #1963) | engine | **TAKE** | startup Function reset on New/Open (#1963) -- a stale startup function from the last show | low |
| 22913325d | 2026-02-22 | plugins/gpio: fix build with newer library version | plugins/gpio | **TAKE** | GPIO plugin builds against libgpiod 2.x (plugin not built by default here; kept in step) | low |
| 31fe58fa6 | 2026-02-22 | qmlui: implemented video fade and close on Esc keypress | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 2feb38cd6 | 2026-02-22 | qmlui: fix also windowed playback destroy | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 4fc5e8288 | 2026-02-25 | qmlui: preserve universe filter across views | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| f4ce52df5 | 2026-02-25 | qmlui: restored audio triggers VC widgets control | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| af0283ac1 | 2026-02-26 | qmlui: preserve VC slider custom range on attribute change | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| b41a1fbe3 | 2026-02-27 | engine: switch to logarithmic audio spectrum bands | engine, qmlui, ui | **TAKE** | logarithmic audio spectrum bands + smoothed power (forum t=19189); behaviour change for audio triggers / RGB Audio / effect scripts, as in stock 4.14.4 | medium |
| 95384afab | 2026-02-27 | qmlui: add CTRL-E shortcut to toggle VC editing | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| d62c64f96 | 2026-03-01 | qmlui: hook 3d support command line | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 8237ec651 | 2026-03-01 | qmlui: fix typo | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| f1875a775 | 2026-03-01 | qmlui: restore the --nowm command line option | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 6515a1495 | 2026-03-01 | qmlui: sort folders and functions alphabetically | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 891ec8921 | 2026-03-03 | qmlui: fix fixture manager node expansion regression | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| b9d312653 | 2026-03-03 | qmlui: fix editing from web (fix #1971) | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 19233c75c | 2026-03-04 | qmlui: mapped more Tardis actions | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| bfa14ac35 | 2026-03-06 | qmlui: fix VC audio triggers DMX output type | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 003f3fed4 | 2026-03-06 | qmlui: fix VC Slider value persistence | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 5330a1a7e | 2026-03-07 | qmlui: fix file save on Android | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 3ca6000f9 | 2026-03-07 | qmlui: fix initial app geometry on Android | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| a0095709d | 2026-03-08 | actions: bump Qt version to 6.10.2 | .github | **SKIP** | upstream CI workflow; the fork runs its own .github/workflows/build.yml | n/a |
| 3c331c57c | 2026-03-08 | qmlui: fix Doc signal in NetworkManager | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| b399c6226 | 2026-03-08 | qmlui: restored VC XYPad presets | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 8bee6aa32 | 2026-03-09 | resources: 5 new fixtures (see changelog) | debian, resources/fixtures | **ALREADY-IN** | the 5 definitions came in with 8fb614a39 (fixtures: import 10 new profiles from QLC+ 4.14.4); debian/changelog not used | none |
| 8b84fd34a | 2026-03-10 | resources: improve the schema validation script | resources/fixtures | **ALREADY-IN** | scripts/check already taken in 8fb614a39 | none |
| 54d837fd4 | 2026-03-10 | resources: fix ADJ fixture | resources/fixtures | **ALREADY-IN** | fixed file already in via 8fb614a39 | none |
| a344d2973 | 2026-03-10 | qmlui: fix crash when submaster is set to 0 (fix #1976) | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 100d8a89b | 2026-03-13 | Update translations (qmlui, webaccess) | qmlui, webaccess | **TAKE** | webaccess catalogues; qmlui dropped | low |
| bcffa5f94 | 2026-03-13 | Updated qmlui and webaccess Catalan and Spanish translations. | qmlui, webaccess | **TAKE** | Catalan/Spanish webaccess; qmlui dropped | low |
| 7aff5af36 | 2026-03-14 | more code improvements (#1974) | engine, plugins/dmxusb, plugins/ola, plugins/peperoni, qmlui, ui, webaccess | **SKIP** | 206-file const/enum/override refactor (#1974), no behaviour change; conflicts across the fork's engine (chaser, doc, scene, palette, fixturegroup, show...). Not worth the merge risk | n/a |
| 14ec662e3 | 2026-03-14 | qmlui: handle channel editor color tool close event | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| e485f0ae2 | 2026-03-14 | qmlui: fix channel editor capability float values | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| 9301eb2ae | 2026-03-14 | resources: 5 new fixtures (see changelog) | debian, resources/fixtures | **ALREADY-IN** | the 5 definitions came in with 8fb614a39; debian/changelog not used | none |
| f04a1a023 | 2026-03-14 | plugins/ola: fix override | plugins/ola | **SKIP** | removes the override 7aff5af36 put on OlaOutThread::start/stop; our header never had it | n/a |
| ff5d4340a | 2026-03-14 | plugins/gpio: fix input level comparison | plugins/gpio | **TAKE** | GPIO input level compared as 0/1 (forum t=19415) | low |
| d27206728 | 2026-03-14 | Enter 5.2.1 release | CMakeLists.txt, debian, variables.cmake | **SKIP** | upstream version bump (5.2.1) | n/a |
| 987112908 | 2026-03-15 | qmlui: remove const from simple desk getFader | qmlui | **SKIP** | qmlui-only (qmlui is off in this fork) | n/a |
| cbfb28734 | 2026-03-15 | plugins/gpio: avoid chipset duplication | plugins/gpio | **TAKE** | GPIO: no duplicated chipsets | low |
| 027e6d4fd | 2026-03-15 | Enter 4.14.4 release | variables.cmake | **SKIP** | upstream version bump (4.14.4): replaced by our own base bump to 4.14.4 | n/a |

</details>

## How to do the next intake

1. **Fetch upstream** into a scratch clone (never into a checkout someone
   is working in):
   ```sh
   git clone --no-hardlinks <qlcconsole> intake && cd intake
   git remote add upstream https://github.com/mcallegari/qlcplus.git
   git fetch upstream --tags
   git switch -c upstream/intake-<version> origin/main
   ```
2. **The range.** Code: from the last reviewed point (`QLC+_4.14.4` now)
   to the new tag, or to upstream master if there's no new 4.x tag:
   `git log --no-merges --reverse QLC+_4.14.4..<target>`. Anything already
   here shows as `-` in `git cherry HEAD <target> QLC+_4.14.4`; a commit
   whose patch `git apply --check -R`s cleanly is in too.
3. **Triage** every commit: ALREADY-IN, TAKE, ADAPT or SKIP, each with a
   reason and a risk. Skip qmlui-only changes (qmlui is off), QLC+ 5
   packaging, upstream CI and version bumps. Look hardest at engine
   races and crashes (compare with the fork's RC4/RC5/RC8 TSan, ASan and
   fuzz fixes so nothing is fixed twice), the network plugins (Art-Net,
   E1.31, OSC, DMX USB: RC4/RC8 hardened them), and web access (RC8's
   model -- local by default, authenticated, origin-checked, escaped --
   must not get weaker).
4. **Apply** in small commits grouped by area (fixtures, plugins, engine,
   UI, translations): `git cherry-pick -x` for TAKE (drop qmlui hunks
   with `git checkout HEAD -- qmlui` before committing); a hand port for
   ADAPT with "Ported from upstream <sha>" in the message. Upstream's
   `ui/src/qlcplus_*.ts` apply to our `qlcconsole_*.ts` by rename
   detection. Build and run the affected tests offscreen after each
   group.
5. **Fixture definitions**: `tools/sync-fixtures.py` (`--dry-run`
   first). It fetches `~/git/qlcplus`, merges each upstream `.qxf` three
   ways against the last sync (`tools/sync-fixtures.base`): new files
   are added, upstream updates of files the fork hasn't changed are
   taken, fork-only and fork-edited files are kept, a clash is reported
   and the fork's file kept. It rebuilds `FixturesMap.xml`, runs
   `resources/fixtures/scripts/check`, and moves the base only when the
   check passes.
6. **Base bump** (only when moving to a new upstream release):
   `QLC_BASE_VERSION` in variables.cmake and `project(... VERSION)` in
   CMakeLists.txt; keep the build count anchored at `QLC+_4.14.2`.
   Update the "Based on QLC+" lines (About box, `--version`, README,
   docs/README.md, docs/features/README.md, SUPPORT.md, RELEASE.md,
   CLAUDE.md) and the CHANGELOG.
7. **Log it** here: a new section under *Intake log* and the table at
   the top.
8. Gate the merge commit (`./check-all.sh --remote ... --rev <sha>`) and
   push only that SHA on `gate=0`.
