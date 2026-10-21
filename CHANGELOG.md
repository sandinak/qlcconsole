# Changelog

All notable user-facing changes to qlcconsole are documented here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This is the fork's own changelog — for the detailed development log (with
per-feature build notes and deferred follow-ups), see [DONE.md](DONE.md) and
active work in [TODO.md](TODO.md).

Versions here are independent of the QLC+ base version embedded in the
in-app build string (see [RELEASE.md](RELEASE.md) for how the two relate).

## [Unreleased]

### Added
- **Skip cues in the Show Tree.** A cue can be skipped without deleting it
  and put back later: untick the box on its row, right-click → **Skip cue**
  / **Unskip cue**, **⌘/** on the selected cues, **Skip** on the cue page,
  or type `skip 0.0.3`, `skip 0.0.3 thru 0.0.5`, `skip 0.0.3, 0.0.7`,
  `unskip …`. Several selected cues change together; each change is one
  undo step and is saved with the show (`Skip="1"` on the cue, and on its
  chaser step). A skipped cue is grey and struck through with a ⦸ badge,
  its layers greyed. GO, auto-follow and Back pass over it, and NEXT (Run
  tab, footer, Cue Runner) names the next cue that plays. It tracks as if
  it weren't in the list: nothing of it passes on, so the cue after starts
  from the cue before (a skipped Block cue blocks nothing). A jump to it
  (a double-click, show control, picking it in Design) still plays it,
  from the cue before it; GO from there restacks to what the list played
  from the top shows. Import keeps a chaser step's skip flag (it used to be
  dropped with a note), and Convert to tracking keeps it. Engine:
  `ShowTree::Cue::skip`, `playedBefore()` / `playedAfter()`,
  `ShowTreeController::setCuesSkipped()`; `ChaserRunner` restacks when
  leaving a skipped step.
- **Definition updates.** Fixtures → **Definition Updates…** lists the
  user folder's fixture definitions that are older than the current version
  of the same Manufacturer/Model (the shipped one, or a newer user copy,
  including stale originals left in the synced QLC+ folder that no link
  leads to), split into used by the open show / not used, with a diff
  (modes and channels added, removed, changed; a mode whose channel layout
  changes is flagged) and **Update from Current**, **Update All**, **Keep
  Mine**, **Ignore This Version**, **Undo Last Update**. "Older" is
  decided conservatively: same bytes → nothing; another author, or a
  channel/mode the current one lacks → customised, never "old"; otherwise
  the Creator version (a qlcconsole build vs a later QLC+ release is
  "can't tell"), then — between the user's own files only — the file time.
  Every update backs the old files up to `Fixtures/.backup/<date>/` with a
  manifest, and undo restores them (dates included) unless changed since.
  Links into the QLC+ folder: update the linked (synced) original, or
  replace the link with a local file; shipped files are never written. A
  definition the open show uses is swapped in place (fixtures keep their
  mode by name); a missing mode or a changed channel count is refused, a
  changed channel kind needs confirming with the fixtures and functions
  listed. Footer entry `definitions.outofdate` with **Review…**.
  `FixtureDefUpdates` (engine), `DefinitionUpdatesDialog`.
- **File → Show Definitions…** lists everything a .qlcc carries and how it
  compares with this computer's, with a per-definition diff. "Which looks
  newer" now compares the Creator versions before the file times. **Undo**
  after Use This Computer's (one step, however many rows), and the
  compatibility lines name the functions using the affected fixtures.
  `ShowEmbedding::clearUndo()` / `undoChoices()` / `computerFileChanged()`.
- **Controller test** (Focus mode's setup, next to **Use**; Settings →
  Advanced → Game controllers). Lists every raw input each game controller
  sends, patched or not, as it arrives: the device, its connection (USB /
  Bluetooth / Bluetooth LE, model, product ID, and how it is read), the
  element (GameController name, HID usage, or USB report byte/bit), the raw
  value and range seen, the channel and value it feeds, and what that maps
  to (profile name, gamepad role, action, Focus mode control). **Copy** and
  **Save…** give the whole log. Plugin interface:
  `QLCIOPlugin::setInputDiagnostics()` / `inputDiagnostic()`,
  `inputConnectionInfo()`, `inputProblem()`.
- **Focus mode says when the controller is paused**, and why --
  "Controller paused: the Mac is asleep / no controller is connected /
  finding the controller again after the Mac slept / macOS is not letting
  qlcconsole read the controller…" -- instead of going quiet
  (`ControllerKeepAlive`, `ProgrammerController::controllerKeepAlive()`).

### Changed
- **The Show Tree shows tracking as badges, with the details on hover.** A
  cue's tracked-in layers used to say *tracked from 0.0.1 Setup* in a column
  that grew to fit, covering the layer names. Now a cue row counts what it
  sets (● 2), releases (○ 1) and what tracks in (⤳ 5); a grey row shows only
  the number of the cue it came from. Hold the pointer over either for the
  group, each attribute's value and its cue, and what the next cue
  receives; the cue page lists the same under **Tracked in**. The badge
  column keeps its width as the tree is resized, can be dragged wider or
  narrower, and is remembered; long text ends in "…".
- **The boot window shows the version.** The window shown while qlcconsole
  starts names the release ("qlcconsole 0.2.2") and, smaller below it, the
  build and the commit it was built from, as `--version` and the About box
  give them.
- The Lighting Studio plot now draws a pixel fixture's heads in the cells
  its LED layout gives (it used head order on the declared grid), and the
  plot, Rig Overview/Angled views and beam origins use the layout's grid
  size when it has one (a strip laid out in rows by hand). All of them
  redraw as soon as a layout is saved.

### Fixed
- **Back, then GO, keeps the colours the cues set.** Going Back (or jumping
  to a cue) played the cue's own layers but left everything else as the
  cue played before it had it: a fixture the cue leaves dark, or lights with
  only a dimmer, kept the later cue's colour wheel, position, gobo or
  shutter, and GO on from there carried it forward. For example: cue 3 sets
  the Sharpys green, cue 4 is a Block cue lighting them at half with no
  colour of its own, cue 6 turns them blue -- played from the top they are
  green in cue 4, but GO to cue 6, Back, Back left them blue in cue 4, and
  GO to cue 5 kept them blue. Now Back and a jump walk back up the list and
  put back what the cues before set last (or, for a setting only later cues
  make, what the stage had when the list started), so every cue looks the
  same however you get to it. In a cue that mixes, a layer that stayed on
  from the cue before could also win over one the cue lists after it; the
  cue's own order now decides, however you got there.
- **No "declares no light output" warning for a curtain.** Opening a show
  with a Generic Stage Curtain listed it in the footer's Not ready list as
  a fixture type that declares no light output. Only a light is asked for
  one now: a fixture whose definition says it gives no light (Emission
  None: a curtain, a motor) and a fan, hazer or smoke machine with no
  intensity channel are left out, in the footer and in the fixture
  editor's check alike. A light with no output declared is still listed.
- **Focus mode draws where the light should point.** While you nudge a
  head, the Rig Overview (CPU and GPU), the Studio's beams and aim traces,
  the plot's footprints, line-of-sight rays and pan/tilt gauges keep the
  focused head's beam on its reference instead of reading back the nudged
  DMX, which made the drawn beam creep and jump with every nudge (rounded to
  DMX steps and a frame behind the offset). This holds through Save,
  Revert, Zero, Undo and leaving without saving; only the panel's numbers
  show the nudge. A Rig Overview opened during a session now rings the
  head being focused, and a selected scene's aim no longer overrides the
  focused head there or on the plot.
- **The controller kept working only while qlcconsole was in front.**
  While Focus mode or the follow spot has a controller, the app now holds
  an `NSProcessInfo` activity (user-initiated, latency-critical): App Nap
  had been slowing the 50 Hz joystick tick to seconds apart whenever the
  app was behind another window or the display slept. The Focus mode panel
  (a tool window) no longer hides when another app is in front. The Xbox
  controller is read through Apple's GameController framework with
  `shouldMonitorBackgroundEvents`. Focus mode's stick integrates the real
  time since the last tick (capped at 0.1 s).
- **A controller switched off and on again, or back after the Mac slept,
  was never picked up again** until a restart: the app's hotplug monitor
  sees USB only, and the HID plugin kept reading the device that had gone.
  The plugin now watches IOKit game controllers on any transport, the
  GameController framework and the Mac waking, rescans, and replaces a
  device that went and came back under the same path in place (so its patch
  is reopened).
- **HID input lines could send their input under a number no patch listens
  to.** A device skipped while scanning (Apple's internal HID nodes) still
  used up a line number, and a rescan numbered new devices from 0 again; a
  device's line is now always its place in the plugin's list.
- **Xbox controller mappings on macOS.** The USB report's buttons were read
  from the wrong bytes and bits (A/B/X/Y came through as View/Menu/stick
  clicks, the D-pad as A/B/X/Y), and its sticks' Y axes were upside down
  for the profile; Bluetooth controllers were read in whatever order their
  elements came, without their triggers (Brake/Accelerator) or D-pad (the
  hat switch), and the Xbox profile did not list any wireless product ID.
  Every Xbox One, Series X|S, Elite and Adaptive model is now identified
  by product ID and connection and read into the Xbox profile's layout --
  through the GameController framework when it has the pad, else the USB
  report (GIP) or the Bluetooth HID usages per firmware (`GamepadLayout`).
  The profile lists the Bluetooth / Bluetooth LE IDs and Share (button 15).
- The HID plugin no longer opens every HID device on the Mac to find its
  joysticks (that included the keyboards, which is what macOS asks Input
  Monitoring permission for) or Apple's internal HID nodes, and no longer
  writes `/tmp/qlc_hid_debug.txt` on every scan.

- **LED Layout editor.** A direct **LED Layout…** action for a multi-head
  fixture — on its right-click menu in the Fixture Manager, the Lighting
  Studio plot and Layers tree and the Rig Overview, and as a button in
  Fixture Properties — edits where each head of its model and mode really
  is, without making a group. The grid shows every head in its cell, both
  kinds at once (colour kinds as a gradient in head order, white, amber,
  UV), with a typed row pattern (`RGB, W, RGB, W, RGB`), click-to-swap and
  move-to-empty-cell, flips, grid size, Head Order, and an order preview
  that can light each head on the real fixture. **Save to Show** keeps it
  in the show; **Save as Default for This Model** also writes it to the
  user data folder's `HeadLayouts/` (one JSON file per model and mode) for
  new shows and shows without their own, which still win. The Split by Head
  Type wizard uses the same editor, with its save-as-default now a checkbox.
- The show's LED layouts are now guarded against being read by a running
  matrix effect while they are edited.

## [0.2.1] - 2026-10-21

A point release of the Show Tree beta: one top bar, auto-open shutters,
a controller map in Focus mode, sub-cues from QLab, and fixes to Import
chaser, confetti on two-arm beams and the update installer.

### Added
- **Focus mode controller map and device choice.** The Focus mode panel
  draws the controller (an Xbox-style layout, vector, following the theme)
  with each control labelled by its Focus mode action, taken from the
  profile's gamepad roles rather than fixed text: a profile with other names
  (Cross, R1, Options…) shows them, a control it lacks is greyed, unused
  ones (triggers, stick clicks, Guide) are grey. Buttons light while held
  and the stick caps follow the sticks; a click on a control does it.
  **Use: Controller | Keyboard** in the setup dialog and the panel picks
  the device that drives the nudges and the map shown (the keys, labelled
  and lit as pressed, in Keyboard). Default: the controller when one Focus
  mode can use is connected, else the keyboard; the user's choice is kept
  per machine (`focusmode/input`). On the keyboard every controller input
  is swallowed and ignored -- sticks centred, nothing applied -- so a
  resting pad cannot drift a head; keyboard shortcuts work in both. A
  controller disconnecting mid-session switches to the keyboard with a
  note, and returning offers **Use the controller**.
  `ProgrammerController::focusController()` / `focusControllerChanged()` /
  `focusControllerInput()`, `FocusSession::setInputDevice()`.
- **Auto-open shutters** (Settings → Advanced, saved with the show, on by
  default). Like a console's auto open: a head given a level (its dimmer,
  or a colour when it has no dimmer) by any look, layer, cue, slider or
  programmer edit, whose shutter nothing sets, has its shutter opened on
  output, at the definition's open value (an open default, else the
  ShutterOpen / "Shutter open" capability; a strobe preset channel at 0;
  never an iris). An explicit shutter value -- Closed, Strobe, anything --
  always wins, including one a slider or programmer edit left behind; a
  head with no level, or a dimmer only resting at its default, is left
  alone. Each universe applies it after its faders on its own thread, on
  the post-GM values only, so DMX, the 2D view and the Rig Overview agree
  and turning it off restores exactly what the show sets. Shows that never
  turned it off save as before (`<ShutterAutoOpen Enabled="0"/>` only
  when off). The AT-Spots (Focus Spot Three Z, shutter resting at Off) now
  light from White, Full alone. The Programming tab stops warning "no look
  for: shutter" while it is on.
- **Sub-cues from QLab.** OSC `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play
  a sub-cue (its cue first); `/subgo` and `/subback` say why when they
  can't; LOAD 4.2 puts a sub-cue on standby and **Next:** in the footer
  and the cue list shows it ("Next: 4.2 — Wash"); STOP 4.2 stops the list
  only while 4.2 is the one playing. ⌥⌘↩ / ⌥⌘⇧↩ go through show control, so their result is
  reported with the rest.
- **Two new look warnings.** The Looks list marks a Colour look that is all
  zero (**Black**), and a position look on fixtures whose definition marks
  every Pan or Tilt channel Fine (**Can't move**: the head stays at 0).

### Changed
- **Settings has General and Advanced tabs.** Advanced holds the show's
  output behaviour (auto-open shutters) and two settings for experienced
  users moved off the one page: the effect script editor, and the Show
  control (QLab) pointer to Connections. Everything else stays on General.
- **One top bar instead of two.** The show controls and the open tab's
  own tools share one row: the tab's tools on the left, **Stop all**,
  **Blackout**, **Blind** and a **Design | Operate** switch (the mode you
  are in lit) on the right, as small icon-and-word buttons. About 57 px
  more for content on every tab (94 px of bars above it before, 37 px
  now). Blackout lights red and Blind blue while on, readable in all six
  themes. A narrow window folds the tab's tools into **»**; the show
  controls never fold. **Settings → Show Controls** can put them in the
  status bar instead, at its right end, leaving the top to the tab's own
  tools; it switches at once, no restart. Same actions and shortcuts
  either way; a tab in a window of its own keeps its tools at its top;
  kiosk mode still has neither. The Show Tree header and the Programming
  tab's row are toolbars now (they fold into » rather than being cut off
  or scrolled), and the Lighting Studio's toolbar uses the same 20 px
  icons as the others.

### Fixed
- **The Show Tree's drop hint stays in sight in the shared top bar.** The
  note that says where a drag will land (and, after a change, what it
  changed) sat at the end of the Show Tree's toolbar, so once the toolbar
  shared the top bar with the show controls it folded into **»** at any
  window narrower than about 1900 px — every laptop. It now stands beside
  the toolbar, just left of **Stop all**, and keeps up to 420 px; the
  buttons fold first.
- **Import chaser keeps an include brighter than the cue's own level.** A
  cue that includes another at full and sets the same fixtures lower itself
  (DanceHysteria's Full Show 0.0.5: 0.0.4's StepTops at full, its own at
  45%) played the 45% after import; the chaser had run both and the full
  level won. The importer now makes such a cue **mix** (Overlap: Mix, *≈
  mix* in the tree) — only where the tree's rule would change its output,
  checked channel by channel — and in a cue that mixes an include plays
  beside the cue's own layers on the same group instead of giving way to
  them, so it plays as the chaser did. The report says which cues and how
  many channels; Convert to tracking keeps them mixing. On DanceHysteria-0.6
  the import dry run is 0 channels different in every cue (Full Show 0.0.5
  marked; 0.0.6 and 0.0.7 already mixed and now match too). The dry run
  (`showtree_test importARealShow`) reads the show's embedded fixture
  definitions, as opening it does, with the user folder kept out, so it
  compares the same on a machine without the author's fixture files, starts
  each playback from the idle rig with channel defaults put back, and fails
  unless 0 channels differ.
- **A dimmer effect on a two-arm beam went black.** One master dimmer
  serving several heads got one scaled write per head, multiplied together:
  confetti's lit arm times its dark one was 0. The brightest head now sets
  it.
- **GO by number on a paused list resumed it and stayed put.** It now plays
  that cue.
- **An OSC float cue number matched nothing.** 2.2 sent as a float read as
  2.200000048; floats now read to the digits they hold.
- **The update installer copes with a stale disk image.** Something
  scanning a fresh download can leave the DMG attached with nothing
  mounted, and every later attach then failed ("the disk image would not
  open"). The macOS update helper now detaches such leftovers first,
  retries a transient attach failure (not a bad image), waits until the
  old mount is really gone, and cleans up if it is stopped part-way; the
  app asks it to stop before killing it, so that cleanup runs. Its error
  says what hdiutil said.
- **Two flaky tests steadied** (Updates_Test's disk images, built now
  without attaching anything; ShowControlUi_Test's OSC port, now checked
  and retried), and the course and office show tests build with GCC's
  -Werror.

### Tooling
- **`make dmg`, `make beta`, `make release` and `make attach`** build the
  signed universal DMG and publish to GitHub Releases through the REST API
  only (`platforms/macos/publish.sh`; RELEASE.md). `make test` builds the
  dev app.
- **Show Tree course** (docs/course): Levels 0 and 1, lessons 0.1-1.5 with
  the Level 1 check, in the course voice, the teaching show-choir rig and
  its shows, a browser practice version of Lesson 1.1, and a test that
  drives every lesson by the mouse.
- **Office show**: a generator for `Office.qlcc` (the office rig test
  show) and its test, run by the gate.

## [0.2.0] - 2026-10-21

The Show Tree release. A show is written as one tree — productions → cue
lists → cues → layers → looks — with tracking, sub-cues and a cue runner
for playback; line targets, line of sight and Focus mode for aiming;
curtains a fixture opens and flies; colour rings on movers; Matrix effects
for pixel boxes; backup WLED lanterns; and output held off the rig
network. Everything in 0.1.0 is in it too.

### Added
- **Software update check and install** (GitHub Releases). A background
  check after start-up and at most once a day, plus **Help → Check for
  Updates…**. It sends one unauthenticated GET of the releases list, with
  ETag / If-None-Match, waits out GitHub's rate-limit reset, and gives up
  quietly offline. Channels are Stable or Pre-release, defaulting to the
  build's own. Versions are compared SemVer-style, with `+qlcconsole.N`
  breaking ties. **Skip This Version** is available. A footer chip shows
  "Update available: X", and a non-modal dialog shows the release notes as
  Markdown. Downloads are verified against the release's `SHA256SUMS`, and
  anything that does not match is refused. On macOS the new app is staged
  beside the old one, after its code signature, Gatekeeper, Team ID and
  bundle id are checked. A detached helper swaps it in at quit, keeps the
  old app until the new one has started, and reopens the show on **Restart
  and Install**. A read-only /Applications falls back to opening the DMG.
  Windows runs the NSIS installer silently after quit. Linux and the Pi are
  notified only. Nothing installs in Perform mode, in kiosk mode, or while a
  function runs. "Install updates automatically" downloads in the
  background and installs at the next quit. `QLCCONSOLE_NO_UPDATES=1` or
  the system-wide `updates/disabled` turns it all off. `release.sh` now
  publishes `SHA256SUMS`, and `./release.sh attach` adds CI's files and
  their checksums.
- **Only transmit on the rig network.** A show can name its rig networks
  (Settings → Network, e.g. `172.18.0.0/20`, saved with the show as
  `<RigNetwork>` in the IO map). With **Only transmit on rig networks** on
  (this computer's setting, default on), Art-Net, E1.31/sACN, OSC and WLED
  output from any interface outside them is **held** — patch kept and saved
  unchanged, line not opened, nothing sent (no default-route unicast, no
  broadcast, no ArtPoll/ArtPollReply/discovery) — and resumes by itself
  within ~5 s on the rig network. Loopback and non-network outputs are not
  affected. A red footer chip ("Output held — not on the rig network
  (172.18.0.0/20). Connected: en0 10.4.12.7") opens the setting;
  **Transmit anyway, this session…** asks first and is never saved. A show
  with none set transmits as before, with an amber footer note and a
  suggestion taken from its patches' saved networks (offered, not written).
- **Focus mode** for aligning the rig before a show: step through the moving
  heads with the controller's bumpers, each aimed at its reference (a
  scene's aim, a stage target, a line target's own point) and lit, nudge its
  pan/tilt calibration offsets with the sticks (the follow spot's stick
  path) or D-pad, A to save (one undo step), B revert, X zero, Y a second
  reference, Menu to leave (asks about unsaved heads). Keyboard fallback for
  all of it. Lighting Studio / Rig Overview **Focus**, View → Focus Mode…
  (⌘⌥F), or View + Menu. Controller profiles gain optional gamepad
  `role=` attributes; the Xbox profile has them and the D-pad.
- **Show Tree** (new tab, ⇧⌘6): productions → cue lists → cues → layers →
  looks, with tracking (Block / Cue-only / Release), includes, one
  precedence rule up and down the chain, a plan view for line targets, and
  preview that matches GO. **Import chaser…** turns an existing chaser into
  a tree (0 channels differ on playback); **Convert to tracking** /
  **Detach to plain chaser** go the other way. A **Looks** section for
  busking (with **Make VC page**), the **Library** with palettes, Recent and
  Pinned, one undo history shared with the Programming tab, and a typed
  command line. Deleting a palette takes it off every look that uses it
  (one Undo puts it all back).
- **Productions, sub-cues and the cue runner.** Several productions per
  show, each with its own main list and timecode; sub-cues 4.1, 4.2… with
  Sub GO or Auto-follow; the **Run** tab (⇧⌘0, Space = GO) and the **Cue
  Runner** VC widget (and **Swap Widget** from a Cue List); OSC `/subgo`,
  `/subback`.
- **Line targets.** A stage target can be a line (or curve): an Aim look
  spreads its heads along it, crosses them, groups them in pairs or halves,
  in hanging or look order.
- **Colour rings on movers.** A mover's LED ring is a head type of its own
  (group by head type makes Movers and Ring groups); ring chases by name;
  drawn live in the 2D plot and Rig Overview; **Change Definition…** moves
  fixtures to a drop-in definition.
- **Create Group(s) from Selection** — one group, a group per head type, or
  a group per head type for each fixture, previewed before anything is made.
- **Palette folders hold any type** — folders first, or Group by type.
- **Matrix effects for pixel boxes.** Fourteen effects in a new Matrix tab
  (bars, chevrons, ripple, rain, checker, sparkle, radar, iris, equaliser,
  snakes, wipes, colour noise, box chase, strobe), across a run of boxes or
  in each box, with white-pixel options (the XL-450's).
- **Curtains that move.** A main drape or traveler can be driven by a
  curtain fixture (bundled **Generic › Stage Curtain**: Open, Open fine,
  Fly): it opens and flies as the channel fades, on the plot and in the Rig
  Overview, and blocks beams where it is (see Line of sight).
- **Backup WLED lanterns.** A WLED universe can drive several devices: a
  backup lantern mirrors the primary without programming anything twice.
  Right-click the universe in Connections (or the Fixture Manager) →
  **WLED Devices / Backup Lanterns…** to add, remove and reorder devices
  (the top one is the primary), each with its own address and channel count
  if it needs one; one undo step. Every device is sent to on its own, so a
  dead or hanging one never holds up the others, and one that comes back is
  sent the whole look again. Connections says "1 of 2 answering — … not
  answering", the footer names the silent backup, and the rig view shows
  the first device that answers. Saved as `outputIP2`, `outputIP3`… beside
  `outputIP`; older versions ignore them and drive the primary alone.
- **Line of sight for follow spots and aiming.** The stage's solid features
  — drapes and travelers as they hang (closed blocks, the open gap doesn't,
  flown is out of the way), walls and the proscenium, platform and riser
  sides, towers, and trusses/pipes/stands at least 20 cm thick — are checked
  between each aimed head and its point; a head's own mount never counts.
  The Followspot effect's **Fade When Blocked** (ms; 0 = off) fades a
  blocked head out and back; an Aim look's **Dim lights that can't see the
  target** (off by default) does the same for any aimed look. Reach
  warnings name the blocker ("blocked by Main drape"), the Lighting Studio
  plot draws blocked aim lines red to the blocker, and **Show → Line of
  sight** sets which kinds block and whether the 3D views stop beams at
  them. Effect scripts read `sceneTarget.blocked` / `blockedBy` /
  `blockedKind` / `blockedAt` / `clearance` (also on `aimAt` slots). A
  line target checks each head's own point on the line; a curtain driven by
  a fixture blocks where that fixture has it now.

### Changed
- **A new production is current when nothing else has GO.** In the Show
  Tree, a production made while no production is current and the main cue
  list isn't some other chaser becomes the current one, so a first
  production plays from the Run tab without **Make current**. Adding a
  production never takes GO from one the operator chose.
- **Coloured light reads as its colour in the rig views.** Light landing on
  a grey or white surface (a cyc, the deck) now takes it over in the
  light's colour: a full blue wash on the cyc draws blue, not white with a
  lavender tint. White light looks as before, and coloured surfaces (LED
  strips, drapes) keep their own colour.

### Fixed
- **Beams stop at risers and the floor in the rig views.** A wash thrown
  across a riser went through it, lighting the air behind it, and its cone
  carried on below the stage floor (the cone's edge was followed to the
  surface the middle of the beam landed on, a drape far beyond). A beam now
  stops at a platform's side as well as its top, each edge of the cone ends
  at what it meets, and a side or front elevation cuts the beam round the
  risers it meets and shows the wash on their tops and faces as a band.
- **Calibration offsets no longer show in the rig views.** A head's pan/tilt
  offsets correct the physical fixture: the DMX keeps them, but the plot,
  its gauges, the Studio, the Rig Overview, beams and footprints now draw
  the intended aim. Fixture Properties' **Test orientation → Aim at target**
  uses the aim solver (it was 180° out and solved standing heads as hung).
- macOS Xbox controller (raw mode): axes now arrive in the order the Xbox
  profile names them (the right stick was reported as the left trigger),
  and the D-pad is reported.
- **Crash on quit with the Show Tree open.** Closing the app while its
  window was still up could crash: the Show Tree tab stopped its preview on
  a show that had already been freed.
- **The cue page's drop area is readable in every theme.** "Drag groups or
  fixtures here…" was nearly invisible on the dark themes (Blue especially).
- **Chaser teardown use-after-free.** Deleting a chaser while its runner was
  still running freed the steps the runner reads (an intermittent "double
  free" on Linux).

## [0.1.0] - Unreleased

First public release. qlcconsole is a fork of
[QLC+ 4.14.2](https://github.com/mcallegari/qlcplus), rebuilt around a
console-first programmer-mode workflow.

**Platforms:**
- **macOS** — one app for Apple silicon and Intel Macs (macOS 12 Monterey
  or later), signed and notarized, as a DMG.
- **Windows** — a 64-bit installer (Windows 10 or later). Not code-signed
  yet, so SmartScreen asks before it runs.
- **Linux** — an x86_64 AppImage.

**Soak-tested** (2026-10-16 → 17, the DanceHysteria show, three 8-hour
phases — the real show, a scaled-up show, rough handling with reloads —
with DMX output stripped): no crashes or hangs on Linux x86_64 (Debian 13),
Linux arm64 or macOS in any phase. On Windows 10 (a VM) the real-show phase
ran clean; the scaled phase was cut short by a Windows Update reboot, and
a shorter Windows re-soak is planned.

### Added
- **QLab and show control.** Chase steps carry cue numbers (a Cue column in
  the chaser editor, with Renumber); a chase can be the main cue list or
  have a list number; **MIDI Show Control** and **OSC** can GO, LOAD, STOP,
  PAUSE, RESUME, RESET and ALL OFF them. The footer's **Next:** shows what
  the main list's GO plays next; GO (⌘↩) and Back (⌘⇧↩) work from any
  window; the Virtual Console cue list shows cue numbers and what's next.
  **Skip** a step without deleting it — GO, Next, Back and auto-follow pass
  over it.
- **Connections → Show control & sync** — one page for what controls the
  show from outside and what it syncs to: OSC in (port, listening or not,
  the last command), MSC in (device number, **Listen on** all MIDI inputs
  or just one, the last command), and the MIDI timecode source with its
  sync health. The timecode source is saved with the show.
- **Sub-cues** — a chase used as a cue lists its own steps under it as
  4.1, 4.2…; set to **Sub GO** they wait for Sub GO (⌥⌘↩, Sub Back ⌥⌘⇧↩)
  while GO moves on to the next cue. Shown in the chaser editor, the
  Virtual Console cue list (with Sub GO / Sub Back buttons) and the footer
  ("Sub: 4.2 · Next: 5").
- **Output trouble is reported** — an Art-Net or E1.31 output that can't
  send, an Art-Net node that stops answering, or a node port reporting a
  DMX short shows in the footer (with **Open Connections**) and in red on
  the universe's row in Connections. It settles before it warns, so a
  brief hiccup doesn't flicker on and off.
- **Shows carry their own definitions** — a `.qlcc` saves the fixture
  definitions and effect scripts it uses. Opened on another computer, the
  show's own copies are used (**Install on This Computer** keeps the ones
  it lacks); where they differ from that computer's the footer says so and
  **Review…** shows the differences. **File → Collect for Transfer…** gathers a
  show and everything it needs into one folder.
- **One beat clock and speed masters.** Every effect and beat chase reads
  one clock (effects started apart are on the same beat); a Tap on the
  Programming toolbar. Up to sixteen **speed masters**, each its own BPM and
  tap, rate and pause, or following the main clock or another master by a
  ratio; a look, chase, step or collection runs on one, and the footer
  shows each master's beat. **GO on the beat** holds a GO to the next beat
  or bar.
- **Spread** — an effect's phase spread across its heads: by order,
  columns, rows or across the stage, forward, reversed, from the centre or
  the ends, mirrored or in blocks, or out from / round the look's aim; a
  preview in the look editor.
- **The stack checker** — the Looks list marks a look the one below always
  covers, an effect another covers, a dimmer only some heads get, and says
  why.
- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, as CSV or a printable PDF (Fixtures → Patch List…).
  **Labels…** prints a label per fixture (all, or the selected rows) on
  Avery 5160 sheets or a 62 mm roll: name, universe and address, mode,
  and a network fixture's name (its IP on mouseover in the list).
- **Units** — Settings → Units picks mm, cm, m, inches or feet (kg or lb
  to match); fixture sizes and weights show in it and take any unit typed
  with the number (`7.5"`, `6' 3"`, `26.5 lb`), and the Studio's grid
  follows it. Files still store millimetres and kilograms.
- **In-app pictures** — the guides show the main window, footer,
  Programming tab, effect browser, Patch dialog and more with numbered
  marks; click one for full size.
- **The Rig Overview on the graphics card** (Metal on a Mac, OpenGL
  elsewhere) with its fixtures worked out on every core: a fully lit
  208-fixture show from about a second a frame to 15 ms.
  A **GPU** box turns it off. Its own **Layers** menu shows and hides
  layers — the same switch as the Studio's eye.
- **Washes look like washes** — a pool fades past its beam angle for a wash
  and stays crisp for a profile; PC and Fresnel lenses are softest. Pools
  show on decks and steps.
- **Programming tab** — a dedicated top-level tab for building scenes/"looks":
  searchable/foldered palette and function trees, drag-and-drop palettes onto
  looks, groups onto dynamic targets, fixtures onto fixed targets. Inline look
  editor for color, dimmer gradient, XY pan-tilt, and gobo/shutter. Live
  DMX/2D preview while editing (Design mode).
- **Group scenes** — scenes that target a fixture group dynamically, so a
  look can be reused across different fixture sets.
- **Fixture Manager enhancements** — fixture-group folders, empty-group
  creation, and a drag-and-drop head-layout grid editor (place fixtures on
  cells, add a whole group as a block, move sub-groups as a unit).
- **Show timeline** — explicit show length with a Logic-style end handle and
  park-at-end behavior, so a show doesn't scroll off into nothing once MTC
  runs past its end.
- **MIDI timecode sync-health** — live rate/jitter/latency readout with a
  healthy/unstable verdict, to help diagnose timing issues against real
  hardware.
- **Effect scripting system** — a scripted effect lifecycle (one-shot bursts,
  sweeps, reveals) with audio- and MIDI-reactive effects; see
  [EFFECTSCRIPT_API.md](EFFECTSCRIPT_API.md) and
  [EFFECT_LIFECYCLE_DESIGN.md](EFFECT_LIFECYCLE_DESIGN.md).
- **Effect presets** — named, reusable configurations layered over effect
  scripts; see [EFFECTPRESET_DESIGN.md](EFFECTPRESET_DESIGN.md).
- **Lighting Studio** — a rigging tab for the physical rig: place and rig
  trusses, towers, platforms and steps, pipes, stands, hangers, a
  cyclorama and power sources; drag a fixture onto a structure to mount it,
  with select/move-together group behavior; a Layers panel for organizing
  the plot; Power/DMX/Network/Stage-only overlays. The plot is seen from
  Top, Front or Side, and each structure's own editor adds an **Angled** 3D
  view; **View → Rig Overview…** (⇧⌘R) opens the whole rig in 3D in a window
  of its own. See [the feature guide](docs/features/lighting-studio.md).
- **The rig view is lit by the show** — in Design mode the selected look
  (or live output) lights the 3D rig: beams per head that carry past their
  target and land as pools on the deck, platforms or cyc; brightness from
  the fixture's declared lumens or watts; a Haze setting (None / Light /
  Medium / Heavy) with a glow round lit LEDs; gobos, gobo rotation, prisms
  and your own gobo images per fixture; pixel bars lit section by section;
  truss socks and tower faces (with printed patterns) glowing with the
  bars inside; people targets lit in the colour that reaches them. A moving
  head's footprint shows everywhere its pan and tilt can reach.
- **Lanterns and hangers** — hangers are rigging, like a truss: clamped to
  a truss or tower with an arm and a drop to one hook, edited in the rig
  view and copied to every tower in one step. A lantern body glows all
  round, with its LEDs (a hanging cluster or tape) seen through clear or
  diffusing glass.
- **Fixtures tab** — a Power folder for modeling power sources/circuits and
  assigning fixtures to them, plus a per-universe DMX address usage grid.
- **Connections tab** — the I/O manager rebuilt around a strict tree
  hierarchy (host → protocol → interface → target → port → universe), with
  right-click menus scoped to exactly what applies at each level, IP-first
  patching, rerouting a line's universes to a different interface in one
  step, and "portable" patches: opening a workspace on a machine missing the
  network it was built for keeps the mapping intact (instead of losing it)
  without broadcasting to the wrong place. Ctrl+Z undoes the last
  patch/universe change made here.
- **Control-surface engine** — a device-agnostic engine for hardware
  control surfaces (role/page vocabulary, context-aware LED feedback), with
  an OpenDeck PMJ Black 1 overlay underway; APC40 mk2 and Xbox controller
  support planned on the same engine.
- **Consistent startup window** — app icon/name/version, a scrolling log of
  what's loading, and a progress bar cover the whole boot sequence (plugin/
  fixture/profile loading, opening a workspace passed on the command line,
  autosave recovery) on every platform, replacing the old indeterminate,
  macOS-only progress dialog. The main window now only ever appears once
  everything is actually ready.
- **Real-time DMX output timing** — on macOS the DMX timer thread asks the
  OS for real-time scheduling, fixing missed output ticks on a busy
  machine. On Windows it has a thread of its own with "Pro Audio"
  priority; on Linux it asks for real-time priority, which a stock install
  doesn't grant (it then runs at normal priority).
- **Backstage color themes** — six selectable app-wide color themes (View →
  Theme): Default, Tan, Blue, QLC+ Original, Red Shift (keeps the blue
  channel near-zero, for working in the dark without wrecking night vision),
  and VS Code Dark.
- **Move-in-black / pre-positioning** — Mark cues to silently pre-position
  movers before a reveal, with a live dangle-detector footer chip flagging
  any fixture left un-marked.
- **Look scope** — a Scene/Look can record whether it is meant for the
  whole stage or a specific fixture group. It is kept in the show file but
  no longer shown in the Looks editor: it changes nothing that lights, and
  next to Targets it read as a second place to choose fixtures.
- Real **File → Import** for `.qxw` — merge fixtures, fixture groups, and
  functions (with their full dependency closure) from another workspace
  file into the one currently open, with automatic ID-collision remapping.
- Rebrand throughout: application, launcher, and fixture-editor binaries,
  macOS app bundle, and About box now identify as qlcconsole, with clear
  in-app attribution to QLC+ and its original authors.
- **Inventory-first workflow** — fixtures can be added to an inventory with
  no address, then placed, grouped and programmed before they are patched
  (they send no DMX until then). A docked Inventory panel in Lighting Studio
  drags them onto trusses, risers and towers; the **Patch** dialog patches
  or re-addresses a selection in one go (keeping their spacing or packed),
  and fixtures can be returned to the inventory in Design mode. Drop
  fixtures or a group on a universe in Fixture Manager to patch them there.
- **Network design** — place Art-Net/sACN nodes and wireless DMX
  transmitters/receivers on the plot (from a catalog of common gear, or
  your own saved models); the Network overlay draws each port's cable run
  through its fixtures, and **More → Network Report…** lists nodes, ports,
  universes and run lengths, with Copy and Save CSV. Unpatched fixtures can
  be placed on the nearest node port with room.
- **WLED output** — Wi-Fi LED devices running WLED get one universe each,
  by IP, sent as WLED's own JSON state only when the look changes (about ten
  times a second at most). Devices that don't answer, or are on weak Wi-Fi,
  are reported in the footer; while the rig view is open it shows what each
  device's LEDs are actually doing. Looks can pick WLED's own effects for
  the device to run, and **160 WLED effects** play in qlcconsole itself —
  every 1D effect, the 23 sound-reactive ones on the audio input. Tried on
  real devices only briefly so far.
- **Scene transitions** — a scene can have a Transition in and out that
  reaches each fixture by where it stands: wipes across, up/down or
  front-to-back, from/to the centre, dissolve, or along each bar, set on
  the scene's canvas in the Programming tab.
- **Effect browser** — the look editor's effect picker is a searchable
  browser with categories, a Recent list and a live 16-light preview of
  each effect. The RGB Matrix effects hidden behind same-named scripts are
  listed too ("Plasma (RGB Matrix)"), including the bottom-up Fire.
- **Readiness footer** — the "Ready" chip becomes "Not ready (N)" and lists
  unpatched fixtures, fixture types with no light output declared, address
  clashes, duplicate names, unreadable or duplicate definition files,
  universes with fixtures but no output, WLED devices not answering, files
  in cloud storage not on this computer, and fixtures with no definition
  here. Select rows (shift-click for a range) and fix them from the list.
  See [the feature guide](docs/features/other-improvements.md#readiness-footer).
- **Cloud-storage safety** — online-only files in a synced folder
  (Synology Drive, iCloud, OneDrive, Dropbox) are skipped at startup
  instead of waited on, with a footer warning and **Download Now**. A
  **show cache** on this computer keeps every non-shipped definition a show
  uses; before a show loads, each comes from the library, the cache, or a
  cloud fetch you can Skip.
- **Fixture definition repair** — a definition that won't load opens in
  the Fixture Editor's repair view, showing where it breaks and offering
  tested repairs; a Problems panel checks definitions as you edit, and each
  definition shows whether it is Included or your own (Local).
- **Stage targets** gain kinds (Person, Drum kit, Area, Structure), can
  follow the truss, platform or tower they're bound to, and scenes say
  "can't reach" when a head's pan/tilt range can't get there.
- **Layers panel** — rename any row with Enter, Select All Similar, fixture
  groups listed under the structures their fixtures are on, and plot
  selection mirrored in the list.
- **Plot** — Select All (⌘A), one undo per drag, and one lock per layer
  (everything locked is outlined red).
- Fixture names are unique: renaming to a taken name is refused, a
  duplicate is numbered on add or import, and older shows' duplicates can
  be numbered from the footer or Fixture Manager.
- A batch of network fixtures gets one IP each, counting up from the first.
- The look editor warns when a colour look is nearly dark (a hue picked
  from black), and a palette being edited is tried on the open look so the
  rig follows.
- Programming tab: search over Fixtures & groups, and "← Back to <scene>"
  from a fixture group's layout view.

- **In-app help** — F1 opens the guide for whatever is in front (each tab,
  the Rig Overview, Fixture Properties, the Patch dialog, the look editor,
  the power dialogs); the Help menu lists Getting Started, What's New,
  Keyboard and Mouse, Effects and Troubleshooting, with search across every
  guide. The guides are bundled into the app, and a test checks every link
  in them.
- New guides: Getting Started (startup and the ways a show comes together),
  What's New, Keyboard and Mouse, Effects, Troubleshooting.
- **Text** effect — words on a pixel grid, scrolling or still, in the look's
  colours; effect scripts can declare a `"text"` setting (a text box in the
  look editor). The RGB Matrix Text algorithm gains a **Centered** style.
- **Step Test Pattern** effect — strips counted, steps numbered and named,
  and a spinner that stops if output freezes.
- **Patch Others Like This** / the Patch dialog's **Match** — like
  structures (four towers, four steps) patched alike: each fixture at its
  counterpart's address, in its own structure's universe.
- **Mount on → Deck** — the selection stood on the deck under each (or a
  named one), upright, on its top, nothing floating.
- The Rig Overview follows the Lighting Studio's layers.

### Changed
- The version you see is the release: the About box, `--version` and
  Finder say **0.1.0**, with the build (`4.14.2+qlcconsole.<N>`, what a saved
  show records) and the commit beside it (see [RELEASE.md](RELEASE.md)).
- **Qt 6.8 or later only**; Qt 5 is no longer supported (the last tree that
  built on it is tagged `qt5-last`).
- Opening the app goes straight into qlcconsole — no window asking whether
  to open the Fixture Editor (Fixtures → Fixture Editor…; a double-clicked
  definition still opens in it).
- Showing and hiding layers, and their labels, work in Operate mode; locking
  stays a Design-mode change.
- A look stacks one way: top to bottom, the lower wins; an effect's colour
  replaces the base; dimmer effects scale and multiply; a Colour or Dimmer
  under an effect is that effect's only.
- Main tab order regrouped to follow the build workflow: Hardware →
  Inputs/Outputs → Lighting Studio → Functions → Programming → Shows →
  Virtual Console → Simple Desk.
- Function Manager, Virtual Console, the Programming tab, and the Show
  Editor each replaced their long rows of individual "New X" buttons with a
  compact "Add ▾"/"Edit ▾" dropdown; a View → Toolbar Style menu switches
  every toolbar at once between icons+text, icons-only, and text-only.
- Lock is now the layer's lock: locking any object locks its layer. Older
  shows' per-object locks move onto a locked layer named "Locked" on open.
- When two local definition files describe the same fixture, the most
  recently changed one is used.
- View → Rig Overview… opens its window without switching to the Lighting
  Studio tab.
- The effect script called "Fire" is now "Firelight" (looks using
  it keep it), so the RGB Matrix Fire is reachable again.

- A drag in the Lighting Studio only moves a fixture; **Shift** held as you
  drop hangs it on the truss or pipe, or stands it on the deck, it's over
  (the outline turns green while it would). Inventory drops still place.
  A mounted fixture is only offered a truss or pipe at its own height.
- **Functions** has no tab — Window → Functions (⇧⌘4) opens it in a window
  of its own.
- ⇧⌘R closes the Rig Overview when it's in front; ⌘W closes it too.
- Orientation is offered for anything that pans or tilts, bar-shaped or not,
  and Test orientation names its centred pose (straight up / down / out).
- Fixtures with a structure above them draw faded and dashed in the Top
  view; the rulers band any selection and mark the middle of what's dragged.
- The Connections Devices header is one row.
- Opening a show with many universes is much faster: a 94-universe show
  loads in about 9 s (was 34 s), in a tenth of the memory.
- A fixture whose definition isn't on this computer draws in the rig view
  as one small box (not a wall of red heads); the footer says which folder
  to copy the definition into, and **Fetch Definitions** looks there again.
- Off a Mac: the themes are readable (buttons, lists and splitter handles
  follow the theme), and the help spells keys Ctrl / Alt / Shift.
- Linux: the app icon shows at every size, and on Wayland.
- Effects on big pixel rigs: about 8 ms a tick for 96 × 64 pixels (was 68);
  the rig view about 9 ms a frame on a 200-fixture show (was 51).

### Fixed
- **Lights behind a closed curtain glowed through it in the Rig Overview.**
  Steps just upstage of the closed main drape showed their haze glow on the
  drape and on the floor in front of it, and the pools their light made on
  the back of the cloth. Opaque scenery and platforms now hide a light's
  glow, and pools on the far side of a drape stay there; where the drape is
  open the glow shows through the gap, and a scrim is still see-through.
- **Saving a fixture definition could stall for minutes on a busy Mac:**
  it flushed every disk instead of the one file. It now writes the file
  whole (or not at all) and flushes only that file.
- **Universe patches were lost when a show was opened away from its
  network.** Opening a show on a computer without the interface it was
  patched to (a laptop off the show VLAN) and saving it wrote the show
  without those patches — every Art-Net universe on that interface gone,
  however often it was re-patched. Patches that can't be opened are now
  kept, shown as offline ("not present — kept, will reconnect"), saved back
  unchanged, and reconnect by themselves when the network appears; inputs,
  feedback and patches to plugins not installed are kept the same way.
  Art-Net, E1.31 and OSC patches also **follow the network** rather than
  one interface address: the same show opens patched on any console on the
  show network, through whichever interface (or route) reaches it.
- A workspace whose last-active tab was Lighting Studio could come up with
  the whole 2D plot empty on a fresh launch — trusses, layers, and every
  fixture missing — even though the save was completely intact.
- Selecting a truss no longer drags along a fixture you click immediately
  afterward, and dragging a fixture bound to a truss no longer drags the
  truss along with it.
- A fixture dropped near (not exactly centered on) a truss can now be
  placed anywhere across the truss's width while still touching it,
  matching how fixtures are actually rigged, instead of always snapping to
  dead center.
- A show file with two fixtures on overlapping addresses dropped the
  second on load, with every group, scene and cue that used it. It now
  loads unpatched and flagged, and is saved back as it was.
- A fixture whose definition was missing was saved as "Generic", losing
  the real fixture for good. It now keeps and saves its manufacturer, model
  and mode, saving warns first, and Fetch Definitions restores it.
- Startup could hang for as long as cloud storage took to answer, or for
  good when it couldn't be reached.
- A steady effect could flash: the output sometimes caught it between
  frames and sent a blank one.
- An unpatched fixture in a running scene could crash the app.
- A gobo look sent nothing, and a new gobo look had no wheel to pick from.
- A look dropped on the canvas now lands where it was aimed, and a colour
  change to a look reaches every scene that uses it.
- Creating a palette or function in a new folder renamed the workspace's
  first look to the folder's name.
- Add Fixture gave every fixture in a network batch the same IP.
- Lanterns on hangers that kept an earlier mount's face drew on their side;
  a fixture on a hanger now always hangs straight.
- A light at the foot of a tower, moved under the bar on top of it, was
  hung on the bar.
- A copy of a fixture's physical data recompiled a regular expression and
  wrote a guessed power figure into the copy.
- **Don't Save** left the unsaved show's autosave behind, so every start
  offered the discarded work back.
- Two static looks on one channel kept the higher value (Blue then Red came
  out magenta-ish); a second Colour look moved a colour wheel; an Aim or a
  Dimmer after a movement effect did nothing; Breathe did nothing on movers
  with only a head dimmer.
- The speed master's beat blinked in the footer with nothing playing.
- The Xbox controller profile was installed where the HID plugin never
  looked.
- A crash under heavy load: with a busy show running, scenes and effects
  starting and stopping could corrupt memory and bring the app down (found
  in a 24-hour soak). Writes past the end of a universe are refused too.
- Lighting Studio's **Add Target**, **Add Person**, **Copy** and **Paste**
  did nothing.
- The patch grid didn't follow universes being added or removed, and the
  Function Manager's buttons didn't follow the selection.
- Windows: a slow tick could pile up hundreds of timer threads until the
  app stopped responding.
- Windows: the app felt sluggish — every output check walked every network
  interface; and USB devices plugged in while running weren't noticed.
- Art-Net send-failure warnings flickered on and off for a target that
  never answered; and Connections read a merging node port as "SHORT
  DETECTED".
- Engine: a fader's priority could be raised but never lowered, and an
  overriding 16-bit channel wrote only its low byte, to the coarse address.
  Nothing in 0.1.0 hit either yet; fixed ahead of the features that will.
- Rig Overview: an RGB fixture with no dimmer channel (pixel bars, many
  par cans) drew far too dark, or black, when dimmed: its level was counted
  twice.
- macOS: pressing the empty canvas in Structure Studio's Angled view to orbit
  crashed the app. Qt 6.11 draws its move cursor (and the wait/busy ones)
  from a colour-profiled picture that it frees too early (QTBUG-147602); the
  move cursor is now the open hand (closed while dragging) on macOS, and
  wait/busy the arrow. Linux and Windows are unchanged.
- macOS: the same Qt 6.11 fault (QTBUG-147602) could crash the app wherever
  one of its own pictures -- an icon, a gobo, a help picture -- was handed to
  AppKit, since 508 of the 660 bundled PNGs carried a colour profile. The
  profiles are removed (the pixels are unchanged, and the app never used
  them), and the test gate now fails if one comes back.

### Known issues
- **Windows:** on heavy shows (around 90 universes with thousands of
  pixel fixtures) the engine couldn't keep its tick rate. A fix is in
  (the test VM now ticks that show at about 8 ms on average) but it hasn't
  had a long soak on a heavy show yet. Moderate shows run normally.
- **macOS:** over a long run the on-screen app's memory grows with no crash
  or slowdown — about 50 MB an hour in the first 24-hour soak; in the
  second, 312 → 492 MB over the 8-hour real-show phase. Being tracked
  down.
- **Linux:** DMX output timing runs at normal priority unless the system
  grants real-time priority (CAP_SYS_NICE or an rtprio limit).
- The universal Mac app has no OLA plugin.
- Not yet tried against the real thing: QLab / MIDI Show Control end to
  end, Art-Net trouble reporting on a failing node, and opening a show's
  carried definitions on a machine without them.

[0.2.1]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.1
[0.2.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.0
[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
