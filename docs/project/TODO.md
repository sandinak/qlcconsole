# TODO — open work

Only what is still to do. Shipped work — with its build notes, follow-ons
and "not yet verified on the rig" caveats — is in [DONE.md](DONE.md); the
sections moved there on 2026-10-11 are under "Moved from TODO.md". When
something ships, move its entry to DONE.md. See also the session memory
under `~/.claude/.../memory/`.

---

## Song sections — leftovers (Q190, 2026-10-29)

Built: numbering scheme per production, section rows in the Show Tree and
Run, Block at section start, + Section, Merge lists into one…
(docs/features/show-tree.md "Songs and sections"). Open, each "decided (can
change)":
- The scheme lives on the production (the show); folds are per session,
  not saved.
- The merged list takes the production's name and keeps the first list's
  id, settings and chaser; VC widgets that fired a merged-away list fire
  the one list from its top (not from that song).
- A later list's base cue or move-in-black On can't be kept per cue; the
  merge says so instead.
- The VC Cue List widget doesn't show section rows; the tree, the Run tab
  and the VC Cue Runner's list size do.
- Course: done (crs): Lesson 0.7, 1.1 (words, "In the app" box), the Level
  1/2 checks and the 3.8 outline say a show is one list, a song a section
  (+ Section). No lesson builds a second song yet.
## Effect origins, Build search, runs for any effect — leftovers (efn, 2026-10-29)

Built (DONE.md 2026-10-29). Open, each "decided (can change)":
- The browser's origin filters: none on lists everything; some on lists
  only those, from every tab (as a search does).
- Picking one of the show's effects in the browser copies it; the copy is
  "based on" what that one was based on (no new link to the palette).
- Build's search keeps each section's own Filter…; a click on a result
  selects it in its section (doesn't open it) and clears the search.
- "N times" for a looping script ends after N lengths; only a one-shot
  script restarts its state each run. EFX / RGB Matrix get Loop / Once
  only (their Run order); no Hold, no N times.
- The *one-off* mark is on effect rows only, not on every one-off value.
- Runs on a Show Tree effect row changes the palette (everywhere it plays
  when it is the show's); it is not an undo step yet.
## Show Tree tidy — leftovers (2026-10-29, DONE.md)

- The command line's `release` takes groups only; a ▣ (function) layer
  is released from its page or the right-click menus.

## Live and blind — leftovers (docs/design/LIVE_BLIND.md, 2026-10-29)

Built (DONE.md 2026-10-29). Open, each "decided (can change)" in the note:
- Build's Workbench in Operate still plays what it loads live; make it
  blind too if "only the run is live" should cover Build.
- The Blind view draws levels, not effects' motion.
- Reordering looks *within* one Show Tree layer (they play in kind order;
  a layer holds one effect per kind, so order changes nothing on stage
  today). Would need an order on Layer, saved, resolved and undone.

## Forum quick wins (Q180, 0.3.1)

From the forum survey's quick wins (STRATEGY.md 3.2). **Built:** Art-Net
TimeCode in (the MTC follow path), the patch list's Gel column, help
answers to common forum questions (Coming from QLC+ 4), File ▸ New from
Template (the classroom stage), File ▸ Show Settings (open in Operate, an
Operate PIN). **Still open:**
- **Tonight's running order** (reorder productions for Run): Show Tree
  code, left while another agent is in it.
- **Fade in/out on hits**: Show Tree (hit bake), same reason.
- **Gel colour in the Rig Overview** (the 3D view): the rig view, same
  reason. The patch list half is built.
- **Momentary / start-only / stop-only on Control Map bindings**: built
  with the Control Map's return (cm, feat/031-control-map), not here.
- **Teaching mode**: DONE (t, feat/031-teaching): must-haves + lesson link;
  see docs/features/teaching.md. Also DONE (tch2, feat/031-teaching2): reset
  to handout (state embedded in the handout, Show Settings, behind the PIN),
  confirm before patch changes (Fixture Manager, Patch dialog), USB/hardware
  output held. Still later: a per-GO "what just happened" log, glossary
  popovers, a Show Review "Teaching mode is on" check (when Show Review
  exists), the course shows rebuilt with Teaching on (they ship off; Lesson
  3.6 says to tick it), and the patch confirm in the Lighting Studio's
  Fixture Properties (monitor files were another agent's).

## Lighting Studio alignment — leftovers (0.3.1 d2, 2026-10-29)

The audit's rows are fixed (DONE.md 2026-10-29, ui/test/plotalignment).
Still open from it:
- The grid is drawn in whole cells (a 15.24 m stage as 15): the outline,
  centring and the off-grid count use whole units (ALIGNMENT.md #18).
- The cursor readout samples depth half a pixel off where it solves
  (A9); a 0.5–1 px bias on a slope.
- decided (can change): a structure editor's ruler still counts from the
  structure's middle; only the Rig Overview's counts from CL / ⌖, and the
  H / V / Z readout counts from CL / ⌖ everywhere.
- decided (can change): a lantern's glow at 45° doesn't pick it; its glass
  and cage do. A click on a ruler band picks nothing under it.

## Focus calls and refining a cue from the stage (design, 2026-10-23)

Two jobs, being designed on the board (Q105-Q108), built after the freeze:
- **Focus calls** (pre-show alignment on a new stage): focus groups saved
  with the show, in order, each with a target; run from a list cue or Run;
  triggers step groups, bumpers step fixtures, stick nudges, A saves the
  fixture's calibration -- so every cue lands right on this stage.
- **Refining a cue in place**: with a cue live, the controller edits the
  cue's own values (aim, zoom, focus, gobo, colour) at a chosen scope
  (layer / group / fixture), with feedback to the operator on stage (the
  selection flashing, spoken status from the laptop, rumble).

**Status (2026-10-24):** built and merged to `main` (board Q105-Q136):
focus calls, venues, Refine in the Show Tree and Build, the focus-call
cue, the venue manager, feedback and voice, the gaps-review fixes. See
[FOCUS_REFINE_DESIGN.md](../design/FOCUS_REFINE_DESIGN.md) section 12 for what is
stubbed and next: Branson's "Try this" with a real pad, then a real-rig
check on the show VLAN.

## House rig, tour package and universes per venue (board Q137-Q138, 2026-10-24)

**Status:** built on `main` (docs/design/VENUES.md): owners on fixtures and groups,
house rigs kept in the venue sidecar and swapped with the venue, group
fills, Tour/House slots, the venue's output patch, "not ours", the clash
check, the venue page, the plot's Owner overlay. Open: Branson confirms
docs/design/VENUES.md section 11; the Rig Overview (3D) owner filter; the focus
report's owner column; a real-rig check that a not-ours universe stays
silent on Art-Net/sACN.

## People on the stage: follow-ons (performer lines shipped 2026-10-29, DONE.md)

Open: a set's costume/height changed in one place (now each line's
dialog); keep performers out of aim-target pickers if they crowd them;
people facing along a turned riser (everyone faces the house); a line
converted from a plain line keeps its own heights rather than chest height.

## Lasers: orientation, look, starfield coverage (board Q172, 2026-10-29)

Design study of 2026-10-26 (parts A, B, C). **Part A
done** (0.3.1): one `RigHeads::fixedThrow` for the 3D view, the plot's
footprint, Quick focus and the orientation diagram; Face for fixed lights
(it aims them), a fixed light dropped on a truss chord faces that side;
Tilt in yoke worded up/down; a strip's Tilt tips up on any upright face,
the file keeping the old sense. Decided at night (can change): the Face
row reads the side the light throws nearest, blank in between; a yoke
tilt set by hand survives a Face change between upright sides; only a
fixed light with no face of its own takes a chord's face on a drop; a
bare typed number in Tilt in yoke means up. **Part B done** (0.3.1): a
laser in the Rig Overview is a box in a U yoke with its aperture window
toward its throw (`projectorParts()`, `FixtureVisualTraits::projector`),
its beams leave the window; `Fixture::LaserLook`, a Laser's Auto plot
symbol, under a new Effects section. Decided (can change): the yoke turns
to the bearing and stays square to its clamp, the box tilts in it; the
housing stays dark and only the apertures light; on a truss the box is
centred on its rig point (as a mover is, and the elevations), seated only
on a deck, riser or tower shelf; a Laser given another symbol draws as
that; the plot symbol turns with the icon rotation like every symbol (not
with the throw). **Open:** part
C (H×V window, density by throw, coverage outline, the audience-zone
safety check for Show Review), the Type=Laser emission inference, and
FixtureDefCheck's laser checks.

## Show Review: an auditor for the show's design (post-0.3, asked 2026-10-25)

Design in [docs/design/SHOW_REVIEW.md](../design/SHOW_REVIEW.md); nothing built, after
the freeze. Reads the saved show (patch, plot, palettes, cues, hits, VC,
venues, power, files) and lists design problems by severity (blocker /
warning / note), each fixed or **accepted with a note saved in the show**
(who, when, why; reopens when what it was about changes). Runtime problems
stay in the footer; the footer's design-time items are folded in under
catalog IDs. 120 checks in 12 groups (moves in light and sweeps, conflicting
palettes, tracking across songs, hit collisions in a scope, venue clashes,
not on the plot, unaimable focus lights, missing definitions, matrices too
heavy for a Pi, circuit overload, strobe over 3 Hz, media outside the
folder, naming); a v1 of 52. An engine registry runnable headless, a
`qlcconsole --review show.qlcc` CLI for CI and the course, a Review panel
and footer chip on top. Open questions in its section 8.

## Focus mode: shutters on profiles (future, asked 2026-10-23)

Framing shutters on a profile/ellipsoidal (and a par with barn doors): how
to define and set them in Focus mode is open. An idea to start from: the
triggers pick a side (top, bottom, left, right), then a stick moves that
shutter in and out and turns its angle. Needs: a model for four cuts per
light (in/out, angle), drawing them on the beam's footprint and in 3D, and
what a definition can tell us. Hard; after the freeze.

## Lighting Studio: handles on the plot (future, asked 2026-10-23)

With the plot unlocked, objects get handles instead of only the editor:
drag a wall's, truss's or pipe's **ends** to lengthen it, **⌥-drag an end**
to rotate it about the other end, and so on for decks and scenery. After
the feature freeze; needs a design pass (which objects, which handles, snap,
undo as one step, how it reads in Front/Side).

## Eos import (docs/design/EOS_IMPORT_DESIGN.md)

- **Phases 1–4 + magic-sheet placement, special circuits and subs as VC
  faders: in the 0.2.2 release candidate (trial/batch7, 2026-10-22).** File ▸ Import takes an Eos USITT ASCII
  export (.asc): the wizard (files, fixture types), the usual picker and
  merge, the cue lists into the Show Tree as tracking lists (one undo
  step). Tried on Branson's Element 2 show (01-30-23 .asc + Magic Sheet
  1, outside the repo: `QLC_EOS_SAMPLE_DIR` runs it in eosimporter_test).
  Left:
  - The magic-sheet calibration dialog (two reference channels → real
    coordinates); today a sheet lands as an uncalibrated layer.
  - BP values, effects, curves, subs' fader/bump behaviour, cue delays and
    links, per-part timing, multi-part fixtures' extra parts: reported,
    not carried.
  - ~~Library gaps: ColorSource Spot / Spot Jr / Cyc RGB modes~~ — done
    (ETC ColorSource Spot jr + CYC definitions, all Spot modes; they match
    Exact now).
  - Augment3d (.esf3d `working.a3d`): his file's is the empty default
    scene; nothing to read until a show has a model.

## Build + Workbench (docs/design/WORKFLOW_DESIGN.md, phases W0–W7)

- **W0–W5 are in the 0.2.2 release candidate (trial/batch7,
  2026-10-22)**: W1 Build tab + Workbench, W2 chasers + Classic, W3 Show
  Tree ↔ Workbench, W4 effect designer (hosted in Build ▸ Effects and the
  docked Workbench), W5 bridge + Upgrade This Show…, view modes. The help
  and the course describe them (docs/rc-022-sweep). Leftovers below; then
  W6 → W7.
- **W1 Build tab + Workbench — leftovers.** It
  carries the P1 outline (feat/programming-outline, never merged) as
  Build ▸ Scenes. Left over / open from W1:
  - A lone scene takes no + Scene / + EFX / + RGB Matrix (a scene is one
    layer); "make a collection of this scene and…" could come later.
  - (Chasers: done in W2, below.)
  - Palettes and Effects sections are today's lists behind the switch; the
    shared Library (P2/W6), the bench (W4) and the effect designer on a
    layer (W4) are to come. No command bar yet (W6).
  - Look Editor *value* edits (a colour, a level) are not on the history
    yet — only scene and collection edits are. Deleting a function is still
    not undoable.
  - Several look rows of one type selected don't edit together yet.
  - The Workbench's expansion is remembered per show folder and loaded
    function id.
- **W3 Show Tree ↔ Workbench — leftovers.** `TreeCueStack` (the engine's
  Simple Desk owns the name `CueStack`), docked in place of the tree's
  page; the tree's cue / layer pages lent to the editor pane. Left over:
  - Rows of a cue don't drag (reorder with ▲ ▼; layers between cues: in
    the tree). A layer row's own menu is short (Solo, Release, Remove,
    Show in the tree); the tree's full layer menu (split, make reusable,
    detach…) stays in the tree.
  - + Scene / + EFX put the new layer last (the tree's order), not after
    the selected one.
  - Cue looks (D6) show in the header count, not as rows of their own;
    edit them on the cue's page in the pane.
  - The dock is one per tree; a detached Show Tree window docks the
    Workbench there too (untested on a second monitor).
- **W2 Chasers in the Workbench + Classic — leftovers.** Step walker (`ChaserWalker`,
  `FunctionStack` chaser step), step header + step table, Chaser from
  selection, drops on a chaser, Make own copy (layer / step), Scenes ▸
  Folders | Classic. Left over / open from W2:
  - The chaser's own settings (run order, direction, speed modes) are still
    edited in the stock Chaser editor in the editor pane, and its edits are
    not on the shared history (as in W1).
  - No ⌥↑ / ⌥↓ shortcut for ◀ ▶ yet (a window-wide one would clash with
    the Show Tree); the step table takes ↑ ↓ when it has focus.
  - A scene step takes no + Scene / + EFX (a step plays one function);
    "make this step a collection" could come later.
  - Preview plays the step's function alone, not the chaser running
    through its steps (Play the chaser from VC).
  - Classic folder rename renames that folder for every function type
    filed in it (paths are shared, as in Folders).
  - ~~Retire the Functions window now Classic covers it~~ — done
    (board Q32, feat/retire-functions-window): Window → Functions opens
    Build ▸ Scenes ▸ Classic; every function type, the wizard, the
    startup function and Delete folder are on + Scene ▾ / the outline's
    right-click; sequences edit in the Workbench; Show Tree Open and Edit
    Chase… load there. The Functions window is hidden, not deleted:
    App::setFunctionsWindowShown(bool) is the one switch, for the coming
    view modes (Classic QLC+ shows it).
  - ~~View modes~~ — done (board Q35/Q36, feat/view-modes): View ▸
    Workspace ▸ Classic QLC+ / qlcconsole / Everything, saved per show
    (`View=` on `<Workspace>`); old QLC+ shows open in Classic, the rest
    and new shows in qlcconsole; a hidden tab's jump shows Everything
    with a note; Upgrade This Show… offers qlcconsole once.
- Then W6 → W7.

## Show Manager parity (review 2026-10-29, docs/dev/SHOW_MANAGER_PARITY.md)

Five fixed; the rest, by the doc's numbers:
- **G1 (decide):** chasers on a show run on the show clock -- loop,
  ping-pong, random, backward, beat tempo and infinite holds play unlike
  4.14.4. Trivial: show-clock only Time/SingleShot/Forward chasers (loses
  scrub sync for the rest); medium: honour every mode. Hot path: perf.
- **G2:** an Edit… action / double-click that opens the scene and chaser
  editors (and after ⌘E New sequence); revives `showRightEditor`.
- G3 undo of drag-move/resize (snapshot before the change); G4 fade-band
  press rewrites hold/fade modes; G5 timings tool scales all cues; G6
  master lock on every edit; G9 back-to-back clips of one function; G10
  per-ShowFunction track settings; G11 Sequence on a wrong-scene track
  deleted on reload; G12 missing Undo snapshots; G13 mute marks modified.
- Keep unless asked: G7 Delete with nothing selected, G8 M/S order.

## Small fixes and leftovers

- ~~**VC effect controls**~~ — done (feat/031-vc-controls; DONE.md
  2026-10-29). Open: a Speed Dial on an effect parameter; overrides that
  survive a stop; Store as an undo step (palette param edits have none yet).

- ~~**Perf coverage gaps** (asked 2026-10-29: "did you do moving heads ..
  rgb matrixes .. fixture groups across the stage .. power management?")~~
  — done (perf/031-coverage, (pg2); DONE.md 2026-10-29): the mixed rig,
  Show Tree, power, drawing and web clients in `make perf` and the gate.
  Open, found by it:
  - **Show Tree edit at scale:** 155 ms CPU per edit on 200 tracking cues
    over stage groups, nearly all `planCue()` re-planning every cue on
    every bake. Incremental bake (only cues whose resolved state changed).
  - **Rig view frame cost:** angled with beams, 62 ms CPU a frame for 240
    fixtures (plan 2.2 ms); with Live on in a running show up to 25 frames
    a second, ~1.5 cores. A frame budget (rate or supersample down while
    values change fast).
  - The Workbench preview is not drawn by itself (it runs functions); no
    point for it.

- ~~**Web access saved with the show** (asked 2026-10-29: "enable
  webserver for a showfile in config and port")~~ — done
  (feat/031-show-webserver; DONE.md 2026-10-29). Open: a real-rig check
  from a tablet on the show VLAN.

- **Left from the RC8 security batch (red team, 2026-10-27):**
  - The QML UI's web access (`webaccess-qml.cpp`) still has the old
    `m_auth && level` checks; qmlui is off, harden it if it's ever built.
  - Web access is plain http: TLS (or a note to put it behind one) if
    anyone asks for it off the show network.

- **Found in the 0.2.2 docs sweep (2026-10-22, docs/rc-022-sweep):**
  - The help shots show Build ▸ Scenes as **Classic**: help-demo.qxw has
    no Show Tree, so it opens on Classic. Give the demo a Show Tree, or
    switch the outline to Folders in `HelpShots`. The Workbench bar's
    **Own copy** button has no number on the Build picture.
  - File ▸ Import's section header is `tr("Fixtures && patch")`: check
    how a section title shows `&&` (and whether macOS menus show section
    titles at all).
  - Course Level 3 lessons 3.3, 3.7, 3.10 have no pictures yet (the
    `<!-- PICTURE -->` notes say what goes where).
  - Not checked: whether the stock Functions tab keeps its own ⌘1–9 keys
    in the Classic and Everything views.

- **Rename a group in place in the Fixtures tree (2026-10-22,
  feat/group-rename-enter): done.** Return/F2 on a group row edits its
  name in the tree, Esc cancels; empty/unchanged names are ignored, as in
  the group editor. Esc now also disarms a fixture/universe row (it used
  to stay editable). Leftover: like every group edit, not on the ⌘Z
  history.

- **⇧⌘N New here (2026-10-22, feat/new-next-thing): leftovers.** Built in
  the Show Tree, Build (+ Workbench) and Fixtures, Edit ▸ New … names it.
  Open: making a group in Fixtures isn't an undo step (New Group… never
  was); a layer with nothing picked in the Library goes to the filter
  rather than asking; the Palettes section and the Lighting Studio have
  none yet; the Edit item shows the key in its text, not as a bound
  shortcut (a binding there would clash with the VC's ⇧⌘N).

- **W4 effect designer: follow-ons (2026-10-22)** (feat/w4-effect-designer,
  docs/design/WORKFLOW_DESIGN.md §3.3/§3.4):
  - ~~Host it in the Workbench's editor pane and Build ▸ Effects~~ — done
    (Build ▸ Effects, the docked Workbench's Effect row). Window ▸ Effects
    stays as a shortcut.
  - In context, the layer's old effect keeps playing under the draft. On
    intensity, highest-takes-precedence (HTP) can let it show through;
    Solo shows the draft alone. Muting just that layer's scene while
    designing would need a preview hook in `ShowTreeController`.
  - `pickEffect()` (a command, a test) doesn't add to the browser's
    Recent tab or switch to the effect's tab.
  - The bench's Try on is a menu, not a drop target; the design has groups
    dropped on it.
  - Not yet seen on the real rig (ender / show VLAN): the preview on
    AT-W-ALL and on the XL-450 boxes was checked offscreen only.
- **Workflow W5 leftovers (2026-10-22)** (feat/w5-bridge; design:
  docs/design/WORKFLOW_DESIGN.md §3.5):
  - **Upgrade this show…** built (feat/upgrade-show-menu): File menu +
    banner, chasers and collections with plans, Capture page, one undo
    step, Restore original. Not in it: a collection → **busk look** (§3.7
    lists it; there is no collection-to-look conversion yet), and a
    stand-alone chaser of collections becoming a cue list is what the
    cue-list row already does. The banner's ✕ still lasts only until the
    next show is opened.
  - The open banner is in the Show Tree tab (Build is W1's).
  - Capture into palettes reads Dimmer, Colour (RGB/W/A/UV, CMY, a wheel
    slot), Position (whole degrees) and an unnamed Gobo value. Shutter,
    zoom, prism and effect-mode channels stay in the "(rest)" scene. A
    scene with its own fades isn't offered (most of Sample.qxw's).
  - The originals' "Imported into Show Tree" mark is the list's record
    (`Original` entries) and the folder name, not an attribute on each
    function; the Functions window shows no banner on them yet.
  - Restore original re-points everything firing the list back to the
    chaser, including widgets pointed at the list after the import.

- **Workflow W0 leftovers (2026-10-22)** (feat/w0-seam-fixes, DONE.md;
  design: docs/design/WORKFLOW_DESIGN.md on design/programming-redesign):
  - The blend between scene steps (`ChaserRunner::blendScene()`) covers a
    cue that plays one scene **as it is**. A scene step read into a layer
    (groups + palettes) bakes to a tree scene, which doesn't blend: a
    chaser of such scenes stepping faster than their own fades would still
    differ mid-fade. None of the real shows does this; the dry run would
    show it.
  - The import re-point moves VC buttons, VC Cue Lists, VC sliders
    (playback), speed dials, clocks, audio triggers, Show timelines, the
    main cue list and the cue-list number (W5). XY pads and matrices
    can't hold a chaser. Not covered: input profiles' MSC/OSC by name,
    Scripts that start the chaser by id.
  - The sheet's check is static (as the cues stand); fades and effects
    aren't compared. The dry run (`importARealShow`) still plays both.

- **Definition updates: leftovers (2026-10-21)** (feat/definition-updates,
  DONE.md):
  - On ender, `~/Library/Application Support/QLC+` links to
    `~/Crew/SynologyDrive/QLC+`, which is gone (the folder is now
    `~/Crew/QLC+`). 51 of the 52 legacy links in the user folder lead
    nowhere, so those definitions don't load. Re-point the link (Branson's
    call), then run Fixtures → Definition Updates…: 12 stale copies are
    waiting in the QLC+ folder, including both 36LED copies at 4.12.6.
  - The standalone Fixture Definition Editor doesn't show "older than
    current" yet. It is a separate app; it could reuse
    `FixtureDefUpdates::scan()` for the open file.
  - Undo goes back one update at a time. Older backups stay in
    `.backup/` and are never pruned.

- **Software update: first real-release checks (2026-10-20)**: the updater
  (feat/update-check, docs/features/updates.md) is unit-tested against a
  local fake GitHub and fake apps only. Still to do:
  - When the first release with `SHA256SUMS` is out, update a signed
    /Applications copy end to end, from the previous release.
  - Test a standard (non-admin) account, which should fall back to Open
    Download.
  - Test the Windows silent install (`/S /D=`) and reopen on the Win10 VM.
  - Versions stay plain X.Y.Z; a build learns it is a pre-release from its
    own release's GitHub flag at its first check. Until that check, a
    pre-release build behaves as Stable. A SemVer suffix would also work, but
    it would break NSIS `VIProductVersion "${VERSION}.0"`.
  - `hdiutil attach -mountpoint` warns that it is deprecated on macOS 27, in
    favour of `diskutil image attach`. It still works.

- **MasterTimer stop/start race (2026-10-16)** — `MasterTimerPrivate::run()`
  sets `m_run = true` inside the thread, so a `stop()` that comes right after
  `start()` (before the thread runs) is undone and `wait()` never returns:
  two `App::clearDocument()` in a row hung appviews_test. `m_run` is a plain
  bool too. Fix: set it in `start()` before `QThread::start()`, make it
  atomic. Found on feat/unit-aware-dimensions; worked round in the test.

- **Layers panel: a row the plot can't select loses its selection on any
  rebuild (2026-10-15)** — clicking a fixture the plot can't select (stacked
  inside a tower in plan view) now keeps its row selected, but the next
  `MonitorLayersPanel::reload()` re-selects from the plot's (empty)
  selection and clears it. Rebuilds are frequent (a rename, any
  `fixtureChanged`), so renaming T1-W from the list may still drop the row
  afterwards. Found when monitor_test's
  `aLayersRowStaysSelectedWhenThePlotCannotFollow` failed under QPA minimal:
  a queued rebuild landed after the click (the test now settles first).
  Fix: carry the list's own selection across a rebuild for items the plot
  can't hold.

- **macOS dev builds show no app icon (2026-10-15)** — `build/main/qlcconsole`
  is a bare binary, so the bundle's `.icns` never applies, and
  `AppUtil::setApplicationIdentity` deliberately skips the app-wide icon on
  macOS (it would override the `.icns` in a packaged Dock). Could set it only
  when not running from a bundle, if it matters.

## Line targets — leftovers (2026-10-17)

P1–P3 built on `feat/line-targets` (DONE.md); not merged yet. If wanted
later (docs/design/LINE_TARGET_DESIGN.md P4): Spread on a control-surface fader; a
"breathe" effect sweeping Spread on a speed master; dragging line handles in
the Rig Overview; effects that chase along a line. Also: the target menu's
"Lock Target" is undone by the next plot refresh (locks mirror the layer's).

## Release 0.1.0 (in progress, 2026-10-16)

On `release/0.1.0`. macOS universal (Apple silicon + Intel, 12+), Windows
installer and Linux AppImage from CI. CHANGELOG.md's 0.1.0 section is
written, known issues included.

- **First 24-hour soak (kronk) — reviewed, fixes merged** (DONE.md
  2026-10-16). **Second soak done** (2026-10-16 → 17, 4 hosts, DONE.md):
  0 crashes or hangs on Linux x86_64, Linux arm64 (podman) and macOS in all
  three phases; Windows 10 VM clean in the real phase, the scaled phase cut
  after 15 min by a Windows Update reboot. Left: a short Windows re-soak
  (scaled + rough), and diff kronk's `--heap-track` snapshots.
- **Rig test pass** — `docs/dev/RIG_TEST_PLAN.md`, section K first (the 0.1.0
  work), on the office rig with kronk.
- Then: merge `release/0.1.0` to main, `make release`, `make attach` the Windows
  installer and Linux AppImage from CI (docs/dev/RELEASE.md).

## Platforms and soak findings — open (2026-10-16)

- **Windows: engine ticks slow on heavy shows** — on the scaled soak show
  (94 universes, 2560 pixel fixtures) the tick can't keep its rate on
  Windows. `fix/win-tick-perf` (merged): the universe threads' per-tick
  qDebug formatting held their fader locks (68 ms mean tick, 660% CPU on the
  VM), and MinGW's std::recursive_mutex leaked a kernel Event per fader
  (~5000 handles/s). GenericFader uses QRecursiveMutex on Windows now: STRESS
  show on the VM at 8.1 ms / 0.2% over / 287% CPU, handles flat at ~1600
  (Linux on ender: 2.3 ms / ~165%). What's left is ordinary engine work
  (Scene::processValue, GenericFader::write). Still in CHANGELOG known issues
  until a heavy show is soaked on Windows.
- **The STRESS show is too heavy for the Windows 10 VM** — soak the VM
  with a lighter show; heavy-show Windows numbers want real hardware.
- **macOS: the on-screen app's memory grows** — about 50 MB/h in the
  first soak's real-show phase (RSS 201 → 1116 MB, no crash, ticks fine);
  neither the engine nor the App driven offscreen reproduces it. The
  second soak runs with `--heap-track` (hourly `heap -s` / `vmmap
  --summary`): diff two snapshots for what grew.
- **WLED config check: log noise** — reported from the soak logs (where
  it logs from is not yet pinned down); find what logs it and how often,
  and quiet it.
- **Windows CI test step: fails the job (2026-10-26)** — the earlier
  "all 142 test binaries pass" (`fix/win-tests-and-nits`, run 38938082831)
  went stale: by 2026-10-24, 16 of 190 programs failed there unnoticed
  because the step was informational (TEST_MATRIX W1–W3: braced lists of
  `&Doc::signal`, unexported `Fixture`/`FixtureGroup`/UI classes, plugin
  signals by member pointer). Fixed (`fix/windows-rc4`); the step now fails
  the job, a program that can't run on the runner goes in its `SKIP` list
  with a reason, and `dllsignals_test` catches the pattern on every platform.
- **Windows leftovers** — not code-signed (SmartScreen warns); ~95 MB
  installer, mostly ffmpeg codec DLLs from Qt Multimedia (trim if it
  matters); `QLCIOPlugin`'s moc lives in each plugin DLL as well as the
  engine, so outside a plugin its signals are connected by name
  (`dllsignals_test` enforces it); no `-Werror` on MinGW, by design. Still to confirm on the VM:
  the main window's outer edges resizing, audio, DMX USB/D2XX, the file
  associations, and opening a .qlcc whose definitions the VM lacks.

## Show Tree (feat/show-tree, P1 built 2026-10-16 overnight, P2–P4 2026-10-17, R1–R3 2026-10-18 — not merged)

[SHOW_TREE_DESIGN.md](../design/SHOW_TREE_DESIGN.md) (D1–D27 approved; §10 has the
implementation notes and the questions below). Built: the hand-over fix,
the engine model / cascade / bake / undo / import, the Show Tree tab and its
help (DONE.md 2026-10-16). Handoff: SHOW_TREE_OVERNIGHT.md.

- **Colour rings 2026-10-18 done** (DONE.md). Branson to do: install
  `engine/test/common/fixtures/Branson-LED-Movinghead+Circle-2heads.qxf`
  beside his own definition (qlcconsole fixtures folder), then select the
  10 movers → **Change Definition…** → "LED Movinghead+Circle (2 heads)",
  then Create Group(s) from Selection by head type. Try on the rig: does
  the ring take the spot's dimmer (drawn as not)?; the chase step rate.
- **Fix pass 2026-10-17 done** (DONE.md): preview of the selected cue in
  Design mode, several groups in one layer, Library group folders, gobos by
  name per fixture type.
- **Follow-up 2026-10-17 done** (DONE.md, §10.9): in Design the edited cue
  wins over any list running for real (Perform: the list owns the stage);
  gobo names match in tiers, ambiguous → not guessed, pick offered.
- **Open for Branson** (§10): `Generated` mark on `<Function>` too?; undo
  shared with Programming?; an effect's own Gobo (out of scope so far);
  DanceHysteria's Hold-0 cues 0.0.4–0.0.7 auto-follow — intended?; **Blind
  and the preview — to discuss**.
- **Try on the rig:** the Design-mode preview while editing (and that Blind
  mutes it), also over a list running for real (edit a dimmer down under a
  running cue at full; stop the preview: the cue comes back); GO through an imported list with effects running (the
  hand-over keeps a shared scene's effect phase — tested on engine signals,
  not seen on stage).
- **P2 done 2026-10-17** (DONE.md, §10.10): tracking controls in the tab
  (list mode, base cue, cue modes, Release on any layer, includes with loop
  refusal, "Next cue receives" with values), the re-bake chain (tested
  against what a bake really changes), "Update cue X" and Record as new cue
  (D22/D23), Library "In this cue list" (D20), the optional command line
  (D24). **Try:** import a show's chaser on a copy, make the list
  Tracking, release / base / remove repeated layers by hand (P4 brings
  Convert to tracking), and GO through it; the command line (Command).
- **P2 answers done 2026-10-17** (DONE.md, §10.11; open 9–11 resolved):
  Record asks selected / running / everything lit when they differ, with
  per-mode defaults in Settings → Show Tree; several groups = one layer
  everywhere, a layer each with ⌥ or a comma, Split / Merge layers; a typed
  or picked value reuses the palette playing it, the command line makes a
  named one per the show's pattern, the picker's one-off has one-click Make
  reusable. **Try:** Record while a list runs (Perform and Design), merge
  two layers and GO through the cues after.
- **Gaps from the help done 2026-10-17** (DONE.md, §10.12): Merge by mouse
  (multi-select), D13 enforced outside the tab, one list rule (new =
  Tracking), one undo step per action audited. **Open:** Detach to plain
  *chaser*. **Try:** ⌘-click two layers → Merge; open the list's chaser in
  the Functions window (locked, Show in Show Tree).
- **P3 done 2026-10-17** (DONE.md, §10.13): group sets (Make set, by
  name everywhere, laid out by position), child layers and overlap with
  `Scene::excludedFixtures` (no HTP show-through; an import keeps mixing),
  cue-level effects as one across layers (D10), line aims in the tree with
  Spread / Grouping / Cross rows, one formation by hang position (D16–D19);
  plus rename in place (Return/F2), ⌘C/⌘V/⌘D cues and layers, `help` in the
  command line. **Open (§10 "Open for Branson" 12–16):** D10/D17 built as a
  shared layout span rather than one extra scene; the plan view (top view
  with the previous cue's rings) not built — the order strip is; a Mix
  cue's overlap in the Design preview; a child's Dimmer on no-dimmer RGB
  fixtures; sub-cue numbering for paste waits for P6. **Try:** make Towers
  from two groups, drop it with ⌥; ⇧-drop a group on a layer, set its
  dimmer down; drop a line target on a cue with two layers and slide Spread
  from the cue's Line ⋯; type `help`.
- **P5–P7 done 2026-10-18 on the build host** (DONE.md, §10.17–10.19;
  handoff SHOW_TREE_OVERNIGHT.md "P5-P7 on the build host"): several
  productions with a current one (what GO, the footer, show control and the
  runner play; its main list, its timecode source); sub-cues in the tree
  (Sub GO / Auto-follow, cue-only, `SubGoFollow`); the cue runner (Run tab
  ⇧⌘0, Cue Runner VC widget, Swap widget, "what changes", OSC
  `/subgo` `/subback`). **Open (§10, 27–35):** switching production while a
  list runs; no current production by default; a sub-cue's wait from fade-in
  or start; the cue's own fade with sub-cues; Run-tab keys; ~~VC widget inputs~~
  (done 2026-10-21, feat/vc-cuerunner-layout: External Input per action, info-first
  layout, Design-mode handles);
  Swap's follow rule; Convert/Detach/import with sub-cues; the tracking
  sheet. **Mac:** `./check-all.sh` legs not run yet (Linux only). **Try:**
  two productions, switch, ⌘↩; + Sub-cue, Sub GO, auto-follow; the Run tab;
  a Cue Runner on a VC page, Swap Widget.
- **Show Tree command line: plain-language prompts** (Branson, 2026-10-17;
  not built). Two layers, phrase layer first. (1) Offline phrase layer
  (~2–3 days): synonyms/filler words mapped onto the existing grammar —
  'make the towers blue at half', 'all the movers', colour names, 'at
  half'→50 — always available at a show, deterministic. (2) Optional AI
  layer when online (~3–5 days): prompt + the show's names (groups,
  palettes, cues) sent to Claude, which returns ordinary command lines;
  shown for confirmation, Return runs them through the same op layer, one
  undo step each; API key in Settings; handles 'warmer', 'like cue 3
  but…'. Not show-time (needs internet).
- **P4 done 2026-10-17** (DONE.md, §10.14): reusable layers and cues
  (Make reusable, uses live-linked by syncing after every edit, Unlink
  here, Library → Reusable with Used by incl. VC buttons, rename, delete
  that asks), Detach to plain chaser, Convert to tracking… (plan first,
  every cue checked to play the same; DanceHysteria-0.5's three lists: 0
  channels differ). **Open (§10, 17–20):** what a reusable cue shares
  (mode yes, timing no); Convert releases rather than Cue-only and leaves
  Mix cues mixing (ties to 13); a detached list's shared scenes stay
  locked until the tree lets go; "detached from X" badge not built.
  **Try:** make a layer reusable, drop it in another list's cue, change it
  there; Convert to tracking on an imported list and GO through it.
- **R1–R3 (docs/design/PROGRAMMING_ROLE_DESIGN.md) done 2026-10-18** (DONE.md,
  §10.15): PaletteOps + one undo history (tree + palette steps, the
  Programming tab's palette list on it too) + one Used by incl. VC;
  Library palettes (+ Palette, F2, folders of any type, drag to file,
  delete naming every use); the Looks section (looks as cues that never
  track, a Collection each fired from VC, Make VC page, Copy to look, Adopt
  as look). **Left of R1:** Look Editor value edits on the palette undo;
  Programming's tree folder rename/delete/drag through PaletteOps; power
  estimate to the app; Stage bar
  (P-D10). **Left of R2:** Open in Palettes. **Left of R3:** per-look fades /
  transition / speed master (and on cue layers), Fire as, a look's MIDI
  trigger in the tab. **Open (§10): none.** **Try:** + Look on a
  production, drop groups and palettes on it, Make VC page, press the
  button over a running list; + Palette in a Library folder, F2, drag,
  delete one a cue uses and ⌘Z.
- **Answers 12–25 done 2026-10-18** (DONE.md, §10.16; open 12–25
  resolved, 26 still open): one precedence rule (Convert puts Mix cues on
  it where every channel plays the same, ≈ mix marks the rest); the plan
  view of a line aim; the Design preview = GO (also in a Mix cue); a
  Dimmer dims the colour it doesn't own on no-dimmer fixtures; Convert
  tries Cue-only; Detach copies shared scenes; "Unlinked from" tooltip;
  Make VC page updates its frame; Programming's ⌘Z / ⇧⌘Z on the one
  history + Edit menu; feat/palette-folders merged with the legacy-layout
  detection fixed. **Try:** Convert on DanceHysteria's New Chaser 8 (its
  cue 1 stays ≈ mix); a line aim's plan view while sliding Spread; Make VC
  page twice after adding a look; Programming → New palette, ⌘Z there;
  open an older show and check its palette folders.
- **Answer 26 done 2026-10-18** (DONE.md, §10.20; Branson: "delete"):
  deleting a used palette clears it from every cue, layer, look and
  reusable (a look left empty goes), as well as scenes and effect slots;
  one Undo puts back the palette and every use. **Try:** delete a palette
  three cues and a look use (Library, Programming, `palette X delete
  anyway`), check the cues, ⌘Z.
- **Course fixes 2026-10-18 done** (DONE.md, fix/course-app-issues): crash
  on quit, new production current, drop-area text, coloured light on the
  cyc. **Open for Branson:** should a coarse Pan/Tilt channel whose
  definition declares no Default rest at centre (128) instead of 0, as
  most consoles home a mover? It changes real DMX for every such head (all
  but ~50 of the 1,639 shipped Pan/Tilt channels declare none), and a
  floor-standing head at centre points straight up, so it would not make
  the classroom's front washes light the stage either. Not changed.
  Branson's "36LED Moving Head" definition marks **Pan** as the fine byte
  (`<Group Byte="1">`): the app finds no coarse pan, so Aim can't pan
  those heads (DanceHysteria's AT washes too) -- fix it in the definition.
- **P5 next:** multiple productions (switcher, current production for VC /
  footer GO / show control, per-production timecode).

## Needs the real rig / hardware

- **PlayStation pads: names and the touchpad (built 2026-10-29, board
  Q170, feat/031-pads-voice)** -- tested offscreen with the stub plugin's
  device kind and simulated touch channels only. On ender with the DS4
  (Bluetooth): the map says Cross/Share/Options and draws the touchpad and
  light bar; the Controller test window shows "buttonA · Cross" and the
  touchpadPrimary x/y rows; `ds4probe 20` while dragging one then two
  fingers confirms a lifted finger reads exactly (0, 0) (readTouch()
  takes that as a lift); a one-finger drag across the pad nudges ~4°, two
  fingers up spot the beam down; a firm drag that clicks only says the
  light. Tune touchDegrees()/touchBeamDegrees()/touchFocusSpan() and
  TouchpadGesture::slop()/jitter() on the hardware. A DualSense: "Create".
  Not built (Q170): the headphone jack (part 3, USB-only audio), SF
  Symbols on the map.

- **The voice waits for the selection (built 2026-10-29, board Q171)** --
  tested offscreen with SilentVoice (settle, cut, preempt, the real timer)
  only. On ender with the voice to the headphones: hold RB through 8
  lights and hear only the one you stop on, ~0.4 s after; switch the pad
  off mid-sentence and hear "Controller lost" at once. `say` is killed to
  cut a line: check it stops cleanly, with no click or half-word left on
  the headset feed. Not built (Q171 #12): a haptic tick per step while the
  voice waits.

- **Control Map with a real MIDI controller and OSC (built 2026-10-29,
  feat/031-control-map)** -- tested through the stub plugin only (learn,
  each mode, feedback values sent, save/load). On a controller with LEDs:
  RIG_TEST_PLAN.md section L (Note On/Off per profile, LEDs after reload
  and replug, OSC learn, the show in stock QLC+ 4.14.4).

- **Rig network gate on real networks (built 2026-10-19, feat/rig-network-gate)**
  — tested with a modelled machine (NetworkRoute::setResolverForTest) and
  Art-Net's test sink only; no packet was sent. On ender: with the show's
  rig network 172.18.0.0/20, unplug vlan0 / join another Wi-Fi and confirm
  with tcpdump on en0 that no Art-Net/sACN/ArtPoll leaves (and the footer
  goes red within ~5 s); plug back in and it resumes. Check WLED on the
  show Wi-Fi vs. a laptop whose default route is elsewhere. Also: a VPN
  route to the show network (source on utunN, outside the rig CIDR) is
  *held* by design — decide whether the VPN's own subnet should be listed.

- **Focus mode with a real controller (built 2026-10-18, feat/focus-alignment)**
  — docs/design/FOCUS_MODE_DESIGN.md. Tested offscreen with a stub controller and the
  keyboard only. On the rig with the Xbox pad: View + Menu opens it; the
  bumpers, A/B/X/Y, View/Menu and the D-pad (macOS raw mode now reports it
  as buttons 11–14 -- check the bits on a Series pad, and on an element-mode
  / 360-driver pad that 11–14 really are the D-pad); the right stick and
  triggers after the raw reader's axis reorder (LX LY LT RX RY RT, as the
  profile names them); stick right/up feel on hung, standing and inverted
  heads; 8°/s and 1°/s rates feel right; the offsets land the light on a
  real mark from two references across the stage.
  **Controller map and Use: Controller | Keyboard (2026-10-21,
  feat/focus-controller-map):** that each control on the panel's map lights
  as the real button is pressed (and the D-pad arms the right way round),
  that the stick caps follow the sticks the right way (up is up); that
  unplugging the pad mid-session switches to the keyboard with the note
  and plugging it back offers it (does HID's rescan emit
  configurationChanged on unplug on macOS, and how quickly?); that on
  Keyboard a resting / knocked pad moves nothing.
  **Background input and mappings (2026-10-21,
  fix/controller-background-mapping):** built and tested offscreen only.
  With the real pad, on USB and on Bluetooth: open **Controller test…**,
  press everything once, Save the log -- every row's *Maps to* must be the
  control pressed (it says whether GameController or which report layout
  read it); then in Focus mode switch to another app, let the display
  sleep, lock the screen, sleep the Mac, and let the pad switch itself off
  (15 min) and back on: the stick must keep moving the head behind other
  apps, and after each the pad must work again with no restart (or the
  panel says **Controller paused:** why). Check `pmset -g assertions` shows
  qlcconsole's activity only while Focus mode / a bound follow spot is on.

- **Matrix effects on the XL-450s (built 2026-10-18, feat/matrix-effects)**
  — the fourteen Matrix effects were checked offscreen only (no DMX from
  the laptop). On the rig: each on one box, on all eight as one canvas, in
  the 135ch and 30ch modes, and the white setting's five choices; that the
  head map's RGB/W/RGB/W/RGB rows are the box as built (Grid Probe); and
  how a look on the split TrussC-RGB + TrussC-W groups lays out against
  one group of all 75 heads per box. The second twelve (built 2026-10-21,
  feat/matrix-effects-2: Fire, Starfield, Spiral, Lissajous, Kaleidoscope,
  Bouncing Balls, Fireworks, Scanner, Dissolve, Gradient Sweep, Box VU,
  Image) and the **Each box, staggered** scope too: a GIF on the eight
  boxes (Fit / Fill / Each box), Box VU from the real audio input, and a
  staggered ripple across the rig.

- **Re-check the Oppsk wall-washer readings (2026-10-16)** — the 24 bars in
  DanceHysteria-0.4 carry Full 12.3 A / Half 2.7 / Standby 0.4 (White 6.6,
  R/G/B 2.3) against a definition rated 100 W: at 120 V that is 1.48 kW a
  bar at full and 48 W dark, 9.6 A for the 24 doing nothing. A DC-side or
  whole-chain reading would explain it; re-measure one bar on its mains lead.
  The V-Show PL-4003 definition has no power at all (now guessed at 50 W).

- **Rig test pass** — run `docs/dev/RIG_TEST_PLAN.md`, fix any ❌ (the gate on the
  freeze list).
- **Output trouble reporting, on the rig** (built 2026-10-15, DONE.md)
  — (1) unicast to an address with nothing on it for a few minutes — one
  "not reachable" line per target in the footer and log, no "recovered";
  (2) unplug a node that was answering — "stopped answering" within
  ~9-12 s, back ~6 s after it is plugged in, fixtures respond at once
  (reporting never holds output back); (3) short a DMX run on a node port
  that reports it (or pull the terminator if that trips it) — "DMX output
  short" on that universe's row, gone 10 s after it's cleared; (4) pull
  the console's own cable — "not reachable" within 1-2 s, gone ~30 s after
  it's back; (5) a node on another subnet: confirm it now appears and is
  watched (unicast polls), and that nothing on the show segment minds a
  poll every 3 s; (6) a healthy rig over a long run shows and logs
  nothing; (7) check a merging port no longer reads SHORT DETECTED.
- **WLED on a real device** — Branson tested some 2026-10-11: hit or miss,
  probably the WLED controller rather than the interface; needs more
  testing. Still to do: the live view included;
  effect / palette names read from the device (`/json/eff`, `/json/pal`);
  160 WLED effects ported, written from behaviour (WLED is EUPL -- never
  from FX.cpp): every 1D effect, the 23 sound-reactive 1D ones on the audio
  input; not ported: the 2D ones, Copy Segment, Image, Slow Transition.
  Compare the ports side by side with a real device -- Percent's share
  (intensity/255, from the docs' "% of fill") especially.
- **Control surfaces** — PMJ (OpenDeck) mapping as the show's operating
  surface; the control-surface engine (PMJ + APC40 mk2 + Xbox) was last
  "in progress, at the rig".
- **Fixture Test on an ADJ Focus Spot Three Z** — reported not lighting;
  the code looked right, root cause never confirmed (DONE.md, 2026-09-05).
- **Verify what shipped without a rig check** — DONE.md marks each such
  entry "not yet Branson-verified"; the lists from 2026-09-05 to 2026-09-26
  in particular. **Verified on the rig 2026-10-10/11:** a real step, front
  and top fixtures, with the power management; two-arm moving heads. The
  rest isn't rigged yet.
- **Hardware checks from the Connections work** — see "Connections tab —
  remaining" below (off-segment unicast probe, a hand-declared target
  actually passing DMX, reachability on `ender`).

## Backlog — asked for, not started

- **Signalling system (cue lights / stand-by / go signals to crew, house
  curtain controller)** — Branson, 2026-10-17: "eventually we'll need a
  signaling system". Drapes can now be driven by a curtain fixture
  (feat/drape-curtain; Lighting Studio "Curtains that move"): patching that
  fixture to a curtain motor controller or a DMX relay box is how a real
  curtain gets cued today. Still to come: stand-by/go cue lights for the
  fly floor and the curtain operator, warn/go timing in the show, and a
  house-curtain controller that is more than a channel.

Effects and playback (from Branson, 2026-10-11, in this order; scope each
before building):

1. **The spread layer, with speed masters and Link** — S1 beat clock, S2
   speed masters, S3 spread layer **done** (DONE.md 2026-10-11; demo in
   test-workspaces/speed-spread-demo.qxw). **S4** control-surface
   automapping (a Speed page: masters on faders, taps, beat LEDs; PMJ
   `Favorites` → Tap) is **backlogged until Branson is at the surface**
   (2026-10-12) -- but each master's Tap / Rate / Pause can already be
   mapped by hand (Speed Masters → Inputs..., 2026-10-12), and the masters
   sit in the footer; S4 is the automatic page on top of that. **S5** Ableton Link deferred (GPL licence).
   [SPEED_SPREAD_DESIGN.md](../design/SPEED_SPREAD_DESIGN.md).
2. **Field generators sampled at positions, plus fire** — generators that
   are a field over the stage, each fixture taking its value from where it
   stands on the plot; fire as one of them.
3. **Layers, blend modes and masks** — effects stacked in layers, with
   blend modes between them and masks to limit where one applies.
4. **Beat-quantized Swop** — GO on the beat is done (a chase or cue list
   holds a GO to the next beat or bar); Swop still lands the instant it's
   pressed.
5. **Keyframe shapes and spatial transitions** — the shape of a move
   between keyframes, and transitions that sweep across the rig spatially
   instead of everything changing at once.

- **Agreed next (2026-10-13/15), after 0.1.0:**
  - **Sub-cues** ([SUBCUES_DESIGN.md](../design/SUBCUES_DESIGN.md)) — P1 (engine)
    and P2 (editor, VC cue list, footer, ⌥⌘↩) shipped in 0.1.0 (DONE.md);
    P3 show control (`/cue/4.2/go`, `/go 4.2`, MSC GO 4.2, `/subgo`,
    `/subback`, LOAD 4.2 on Next) is in 0.2.0. Still to do: P4 step-based
    effects on Sub GO.
  - **Sending to QLab** — OSC / MSC out. Its home is ready: the "QLab out"
    section of Connections → Show control & sync (`ShowControlSyncPage`).
  - **Duplicate a structure's setup** (Branson, 2026-10-15; replaces the
    one-off "tower patch rule") — from one structure (T1) and the fixtures
    on it, set up its likes (T2, T3…): the same fixtures placed and mounted
    the same way, matched by their labels, and addressed. The addressing
    half exists (right-click → Patch Others Like This / the Patch dialog's
    Match); the placement half is the new part. Design first.
  - **Playback timing** (docs/design/PLAYBACK_TIMING_DESIGN.md): P2 fade shapes, P3
    per-chase-step transitions, P4 Swop, P5 clickable Load chip.
  - **Rig Overview frame rate** — maybe automatic (30, lower when the
    engine is busy): the overview now draws on every core, which competes
    with the DMX engine on a 6-core machine. Decide from the soak (the
    first one's worst tick on kronk, 6 cores, was 40-43 ms).
- **Stage and inventory across show files** — keep a stage layout
  (structures, platforms, scenery) and the hardware inventory (fixtures,
  network gear, their patch) outside any one show, and bring them into a
  new show instead of copying a .qlcc and stripping it.
- **Stress test across Mac / Linux / Windows 11** — the 24-hour soak
  runner (`stresstest/soak24/`) now runs on all three (DONE.md 2026-10-16)
  and the second soak ran on 4 hosts (DONE.md). Still missing from the matrix: a real
  Windows 11 machine (only the Windows 10 VM so far), and the new paths at
  full rig (WLED/JSON output, effect runner) with outputs live rather than
  stripped.
- **One object editor** — merge Fixture Properties and "Edit Fixtures in
  Studio" into one: a tree of the object's fixtures by group, add by
  right-click or drag, multi-select → fixture group, distribute / put on
  face. Design options first.
- **Fixture Editor inside the main window** — not started; needs its own
  investigation (DONE.md, "Fixture Editor integration into the main window").
- **Post-freeze:** cue transition 4b (hold-on-miss + release-on-transition);
  "Look" as an assembly unit; pre-positioning / mark cues with a ghost
  visual.
- **Timecode slice 3** — auto-fill the internal latency; **audio
  calibration** — an active loopback self-test; **clickable Load chip** — a
  per-function breakdown.

## Ideas — not started

### no-light zones -- warn when a look lights the audience

Branson, 2026-09-17: "if we create a scene with lights that point an area that
the light SHOULDN'T be in .. audience faces .. that should be a warning. The
latter thing can be time gated.. so sweeps or strobes for blinders can be ok."

**Shape.** A zone is an AREA of the room rather than a surface: a footprint on
the plot plus an eye height (1.2 m sitting, 1.7 m standing). Drawn like an Area
target -- outline from above, a band in the elevations -- on its own layer.

**The warning.** A scene warns when a beam RESTS on the zone. Everything that
crosses it in passing is fine: a sweep, a strobe, a blinder hit. The proposed
rule is a dwell time -- a beam on the zone for more than about half a second
warns -- which needs the aim to be evaluated over the look's own movement
(effects included) rather than at a single instant. Where it lands lives with
the reach work: AimSolver::sceneAims() already resolves aim per fixture, and
StructureStudioView::beamFootprint() already cuts a cone against a surface.

**Open questions.** The dwell threshold (0.5 s a guess, not measured against a
real cue). Whether a zone is a new object or StageTarget::Kind::Area with a
"no light" flag. Whether the warning belongs on the scene row in Programming
(next to "can't reach"), on the plot, or both. Whether house lights and
audience blinders are exempt by fixture group.

---

### people as a stage feature -- whole choirs on a layer

Branson, 2026-09-17: "multiple people on the stage .. can we create "people" as
a feature type so that we can have a layer with say the entire choir in
different positions on the stage for visualization."

**What exists.** A StageTarget of kind Person already draws a lofted figure in
the rig view (lit by the beams on it) and a silhouette in the plot, stands on
the deck under it, and is sized by the follow-spot subject height. But a target
is an AIM POINT: making forty of them to show a choir clutters the target list,
the Aim palette picker and the aim warnings.

**Shape.**
  - A "people" feature separate from targets: position, height (or child /
    adult / seated presets), optional name/part (S/A/T/B), facing.
  - Placed in bulk: a grid or arc across a riser row, snapping to decks the way
    fixtures deck-mount; drag as a block.
  - On layers, so "Choir -- concert", "Choir -- processional" and "Band" can be
    swapped for visualization without touching the rig.
  - Any person can be promoted to (or linked with) a target when a follow-spot
    needs to track them.
  - Lit like the Person figure is today; worth measuring rig-view frame time
    with 40+ figures (each is ~300 shaded faces).

---

### an MCP server for qlcconsole

Branson, 2026-09-10: "shower thought .. can we consider an MCP for qlcconsole ..
so that we can do this kinda thing inline?"

**The case for it, from this session.** Three rounds of bulk edits were made to
`stage-structures-demo.qxw` by parsing and rewriting XML with regex, and the
schema was guessed wrong in two of them:
  - `<Head Fixture="36" Head="0"/>` looked entirely plausible and loaded as an
    EMPTY group. The real format places every HEAD on an X/Y grid with the head
    index as the element's text -- `US1t` is 16x2 = 32 entries for 2 fixtures.
  - A verification pass compared fixture heights to an absolute 0.204 m and
    reported 16 false failures, because the steps are STACKED and each has its
    own `platformBaseZ()`.
Both were guesses at a schema the engine already knows exactly.

**Shape.** A small `tools/qlcmcp` binary linking `libqlcplusengine`, speaking
JSON-RPC over stdio. It loads a workspace through the REAL engine and exposes
thin wrappers -- `list_fixtures`, `list_groups`, `list_structures`,
`create_group`, `delete_group`, `set_fixture_placement`, `assign_to_group`,
`save`. Because saving goes through `Doc::saveXML()`, there is no format to
guess and nothing to verify afterwards: that whole class of error disappears.

Feasibility is already demonstrated -- `monitor_test` loads real workspaces
headlessly through `Doc::loadXML()` with the fixture-definition cache, which is
most of what the server needs.

**Caveats:** a real project, not an afternoon. Needs care about editing a file
the running app has open (or a "read-only unless told otherwise" default). Check
whether the existing `webaccess/` module is a better host than a new binary
before starting. Useful to Branson directly too, not just to tooling: scripted
rig edits, bulk repatching, cross-show consistency checks.

---

## 🔌 Connections tab — remaining

*(Shipped work moved to [DONE.md](DONE.md), 2026-08-28.)*

- **Hardware verification — status.** Use `build/tools/artnetprobe/artnetprobe`
  (`--selftest` / `--poll <addr>` / `--listen`).
  - ✅ **Arrival-interface reporting**: verified 2026-08-28 against live
    traffic. Real ArtPollReplies from 172.18.2.218 and 172.18.2.10 were both
    correctly attributed to `vlan0`, 5/5 datagrams carrying an index. This is
    the routing fix confirmed on real gear, not a simulation.
  - ✅ **USB devices appear**: DMXKing ultraDMX Micro and OpenDeck PMJ_BLACK_1
    both listed under DMX USB / MIDI.
  - ⚠️ **Off-segment unicast probe**: unproven. Every node reachable from this
    host is on a subnet it has an interface on, so it answers broadcast polls
    too and the reply is indistinguishable. Needs a node on a subnet with no
    local interface.
  - ⚠️ **Hand-declared target actually passing DMX**: unproven, and NOT
    testable from one host. Console and probe both bind 6454 with
    `ShareAddress`; broadcast reaches every bound socket but unicast reaches
    exactly one, so the probe cannot see the console's own unicast output.
    Run the probe on a different machine from the console.
- **Devices/Overview parity — done.** Bulk multi-row retarget with port
  auto-increment, feedback IP/port, ArtNet `inputUni`, MIDI output mode, MIDI
  input channel and the MIDI Out-vs-Feedback role swap have all landed on the
  Devices tab. Universe passthrough and the plugin
  description/status have since moved onto Devices too. Still Detailed-only
  and staying there deliberately: the Audio tab, input-profile
  creation/editing (`InputProfileEditor`), and the USB hotplug toggle (an app
  preference more than a patching control). The split now reads as
  "Devices = the rig and its wiring, Detailed = audio and profile editing".
- **Patch undo — remaining limits.** `PatchUndo` (engine/src/patchundo.{h,cpp})
  is ONE step deep: a second change discards the first, deliberately, since a
  state two changes stale would restore onto a rig that has moved under it.
  It covers patches and the universe list (add/delete/name/passthrough) from
  all three tabs, but nothing outside the patch — fixtures, functions, scenes.
  Ctrl+Z is scoped to the Connections widget (`Qt::WidgetWithChildrenShortcut`)
  so it cannot promise general undo elsewhere in the app.
- **`InputOutputMap` universe id vs index — not a live trap after all.**
  `universe(id)` resolves by ID while `outputPatch()`/`setOutputPatch()`/
  `setInputPatch()` take an ARRAY INDEX, but id == index is enforced
  deliberately at both mutation points, not merely coincidental:
  `addUniverse()` assigns the next index as the id, refuses an id already
  present, and fills gaps so a higher id still lands at its own index;
  `removeUniverse()` refuses anything but the last entry. The contract was
  written nowhere, so it is now pinned by
  `InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex()` — relax either
  rule and that test fails instead of the app repatching the wrong universe.
- **Reachability probing — verify on `ender`.** Rescan now unicasts an ArtPoll
  at every address believed in but not heard from (hand-declared targets and
  addresses named by `outputIP`), so a node on another subnet can answer. Never
  tested against a real off-segment node. Note it probes only on an explicit
  Rescan, not on the 5 s tick — a live "is it up" indicator would need a
  cadence decision, and continuous unicast polling of every configured node is
  exactly the traffic this has been avoiding.
- **Probing is Art-Net only.** `QLCIOPlugin::probeTarget()` defaults to a
  no-op; nothing else implements it. Fine today, since Art-Net is the only
  plugin with addressable targets at all.

---

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Build host: Raspberry Pi 4 (TODO, 2026-08-25)** — the highest-value box to
  add, because it answers an open question rather than just adding a build:
  1. **Tick punctuality.** 51 universes at `transmitMode="Full"` on a 14-core
     M-series logged 2 `MasterTimer is running late` events in 61 s at only
     50% of one core (see the open item above). Four weak ARM cores is where
     that either holds or doesn't. This is the measurement still outstanding.
  2. **Different ABI.** ARM makes plain `char` *unsigned* where x86 makes it
     signed, plus stricter alignment. DMX values are compared as `char` in
     places (e.g. `QCOMPARE(ua.preGMValues()[0], char(255))` in
     `ui/test/vcxypad/` — `-1` on x86, `255` on ARM). No amount of x86 testing
     finds that class.
  3. **Real target.** Upstream QLC+ ships Raspberry Pi builds; Pi-based
     lighting controllers are common.
  Build 32-bit (armhf) and you also get `qsizetype` as 32 bits — the width
  assumption behind the `%d` format bug fixed in `mastertimer.cpp` this session.
  Setup mirrors 192.168.1.245: cmake, Qt dev packages, xvfb, python3-lxml.

- **Build host: Debian 12 guest (TODO, 2026-08-25)** — a *newer* toolchain
  bound. CI is Ubuntu 22.04 (GCC 11, glibc 2.35, Qt 6.8); 192.168.1.245 is
  Debian 11 (GCC 10, glibc 2.31, Qt5). Bookworm gives GCC 12 + `qt6-base-dev`,
  which is the only combination not currently covered anywhere: Qt6 **and** a
  compiler newer than CI's. Deliberately NOT another Ubuntu 22.04 box — that
  duplicates CI exactly and would never find anything. The value of
  192.168.1.245 was proven the day it was built: its older glibc exposed a
  pthread linkage bug (`4a360911c`) that Ubuntu 22.04 structurally cannot see.

- **MasterTimer misses ticks at full output load (2026-08-25, 2026-09-02) —
  fixed on macOS, own thread on Windows (2026-10-16), ROUGHED IN and NOT
  YET VERIFIED on Linux.**
  With 51 universes forced to `transmitMode="Full"` on `ender` (~2484 pkt/s,
  ~49 Hz per universe) a 61 s run logged **2** `MasterTimer is running late`
  events while a 91 s run at the same settings logged **0**. CPU was only 50%
  avg / 64% peak of one core, so this is not throughput saturation — it's
  scheduling jitter, and it's the same wall-clock sensitivity that made
  `MasterTimer_Test::interval()` and `VCCueList_Test::functionRemoved()` flaky
  on CI. Reproduce with `/tmp/throughput.sh` on ender (see DONE.md 2026-08-25
  for the harness).

  **macOS/iOS — fixed, and previously validated** (`feat/realtime-timer-thread`,
  ported from the `rt-test` branch, `8487dbf71`): the DMX timer thread now
  requests Mach `THREAD_TIME_CONSTRAINT_POLICY` (what CoreAudio uses for the
  same problem) in `engine/src/mastertimer-unix.cpp`. Advisory — warns and
  falls back to normal priority if the kernel refuses. This is the run
  referenced above ("real-time-policy run showed zero events of either
  shape").

  **Linux — same file, `SCHED_FIFO` via `pthread_setschedparam`, ROUGHED IN,
  UNVERIFIED** (no Linux box in that session; CI will give a real compile/link
  result on push). Priority is `sched_get_priority_max(SCHED_FIFO) - 10`, not
  max, matching the JACK/PipeWire convention of leaving headroom above this
  thread. **Known limitation, not solved here:** `SCHED_FIFO` normally needs
  `CAP_SYS_NICE` or an rtprio limit, which a stock install will not have — so
  on most Linux installs this falls back to a warning + normal priority
  exactly like an mac refusal would. Packaging a fix (setcap in a postinst,
  or a bundled rtprio limits.d rule) is a separate, bigger decision — not
  done, flagged as its own backlog item below.

  **Windows — done 2026-10-16** (`fix/win-mastertimer`): one dedicated
  timer thread with MMCSS instead of a thread-pool callback per tick
  (DONE.md 2026-10-16). Ticks too slow on heavy shows is a separate,
  open item ("Platforms and soak findings" above).

- **Linux: package a way to actually grant SCHED_FIFO (2026-09-02, follow-on
  to the item above, not started)** — the DMX timer thread's real-time
  priority request will silently fall back to normal scheduling on a stock
  Linux install (no `CAP_SYS_NICE`, no rtprio limit). Options to weigh:
  `setcap cap_sys_nice=eip` on the installed binary from a `.deb`
  postinst/AppImage hook, vs. documenting a manual `/etc/security/
  limits.d/qlcconsole.conf` rtprio rule (the traditional pro-audio-Linux
  convention, needs the user in a group like `audio`). Needs a real Linux
  box to even confirm the roughed-in `SCHED_FIFO` call links/behaves as
  written before this is worth deciding.

- **The ARM char-signedness tripwire is commented out (OPEN, 2026-08-26)** —
  the assertion cited as the reason to want an ARM build host,
  `QCOMPARE(ua.preGMValues()[0], char(255))` at
  `ui/test/vcxypad/vcxypad_test.cpp:474-483`, sits inside a `/* FIXME !! */`
  block and runs nowhere. The live `char(...)` comparisons in
  `vcxypadfixture_test.cpp` are signedness-*insensitive* (both sides go through
  the same conversion) so they pass identically on x86 and ARM. The Pi 5
  confirms the ABI is there — plain `char` is unsigned, `__CHAR_UNSIGNED__`
  defined — but nothing in the suite exercises it. **Un-commenting that block
  is the experiment that cashes in on the ARM host existing**; until then the
  host proves nothing about this bug class.

- **Debian 12 setup quirk: Qt6 `lrelease` is not on PATH (2026-08-26)** —
  Debian's `qt6-l10n-tools` installs `lrelease`/`lupdate` to `/usr/lib/qt6/bin/`
  with no `/usr/bin/lrelease-qt6` symlink (unlike the Qt5 packages), and
  `translate.sh`'s `which_qt()` probes only `lrelease`, `lrelease-qt6`,
  `lrelease-qt5` on PATH. Build dies at 1% with "lrelease not found". Workaround
  `export PATH=/usr/lib/qt6/bin:$PATH`; durable fix is to fall back to
  `qmake6 -query QT_HOST_LIBEXECS`/`QT_HOST_BINS` or have CMake pass the
  `Qt6::lrelease` target location down.

- **buildhost-qlcplus is missing 10 custom fixture definitions (2026-08-26)** —
  headed runs there open with an error dialog: no definition found for Branson
  LED Movinghead+Circle, Branson LED SPOT, Betopper LM70, ADJ Focus Spot Three
  Z, Oppsk Wall Washer Light Bar, Warmdance XL-450, Junman Two Arm LED Beam,
  PHS Chorus Step Row 64 Heads, WLED Effect Mode. Same cause as the
  `/tmp/*.qxf` load failures seen on that host. Sync the `.qxf` files to
  `~/.qlcconsole/fixtures/` there before using it for real UI validation.

- **Note: lateness magnitude was already logged before the `late_us` diag**
  — `mastertimer-unix.cpp:67,74` has a pre-existing
  `qDebug() << "Time is late by" << ... << "nanoseconds"` inside `compareTime()`.
  The new line's genuinely new information is **`compute_us` + `budget_us`**
  alongside it, which is what separates jitter from load; the raw lateness
  figure was obtainable already.

- **TRAP: on Linux, a build-dir run loads NO I/O plugins and silently outputs
  nothing (2026-08-25)** — cost a full 10-minute soak that looked like a clean
  pass. `IOPluginCache::load` resolves via `QLCFile::systemDirectory(PLUGINDIR)`
  (`engine/src/qlcfile.cpp:181`), and on Linux that branch is just
  `dir.setPath(path)` with `PLUGINDIR` a **compile-time absolute**
  (`/usr/lib/qt6/plugins/qlcconsole`, or `qt5` on the Debian 11 box). There is
  no env override. macOS is fine because its branch resolves
  `applicationDirPath()/../PlugIns`, which the build tree mirrors — so this
  bites only on Linux and only when running from `build/` without installing.
  The failure is **silent and looks like success**: every universe logs
  `setOutputPatch - plugin: "None"`, the app runs happily, CPU sits at ~0%, and
  a punctuality soak reports **zero** late events because nothing was ever
  transmitted. Check `grep -c artnet` in the run log, or CPU > 0, before
  believing any Linux throughput/punctuality number.
  Workaround for a build-dir run:
  `sudo mkdir -p $PLUGINDIR && sudo ln -sf ~/git/qlcconsole/build/plugins/*.so $PLUGINDIR/`.
  Worth considering a `QLC_PLUGIN_PATH` env override so test rigs stop needing
  root to run what they just built.

- **`VCCueList_Test::functionRemoved()` flake — watch, don't assume fixed
  (OPEN, 2026-08-25)** — failed on 2 of ~9 macOS CI runs asserting a tree row
  count right after a 100 ms deferred refresh. `6d3757bb2` switched it to
  `QTRY_COMPARE`, which polls instead of sleeping a guessed interval. NOT
  proven: it passed 7 of 9 runs *before* the change too, and the failure could
  not be reproduced locally even under six spinning cores. Sustained green
  across many runs is the only evidence that counts. If it returns, the fix was
  insufficient rather than wrong. Five other UI test files still use fixed
  `QTest::qWait` and were left alone — none has failed.

- **Warning backlog outside the `-Werror` set (OPEN, 2026-08-25)** — a
  tree-wide `-Wall -Wextra` sweep reports ~1100
  `-Wunnecessary-virtual-specifier`, 92 `-Wdeprecated-declarations` (all in
  Homebrew's OLA headers, not ours), 38 `-Wnon-c-typedef-for-linkage` (e.g.
  `PreviewItem`) and 20 `-Winconsistent-missing-override`. None is in the set
  CI enforces and most are third-party. Noted so the next person doesn't
  rediscover them and assume they're new.

- **Rig fidelity test not done (OPEN, 2026-08-25)** — throughput was measured
  at the NIC with tcpdump, which proves what the console *transmits* but not
  that a node *accepts* it. Branson has a single 4-universe node available;
  the full rig is in storage. That's the test that would confirm packet
  well-formedness end to end. `docs/dev/RIG_TEST_PLAN.md` as a whole remains
  unexercised.


- **Output ENDPOINT reachability check (MOSTLY BUILT 2026-10-15, see "Output
  trouble reporting" in DONE.md)** — node liveness is now watched continuously
  from the ArtPoll the plugin already ran every 3 s, rather than the
  config-time probe sketched below; only nodes heard this session are judged,
  which answers the cry-wolf concern. Still open from this item: a check at
  load/patch time for a unicast target that has NEVER answered. Original
  notes: the
  readiness indicator added in `daeeb97d7` catches one failure mode: the
  workspace names a plugin *line* (for the network plugins, an index into the
  local interface list) that doesn't exist on this machine. It does **not**
  catch the other one: the line is fine, but the ArtNet *node* at the far end
  isn't answering. Found during the live soak on `ender`, where 50 of ~53
  patched universes were failing to send to 13 node addresses
  (`172.18.2.201`–`.230`) — in that instance correctly, because the devices
  were in the trailer, which is exactly why this needs care rather than a
  naive "ping the target" test:
  - a broadcast/subnet target legitimately has nothing to answer it, so
    unreachable ≠ misconfigured;
  - nodes are routinely powered down between calls, and a desk that cries
    wolf every load gets ignored (the same failure mode as the 2850-line
    `sendDmx` spam this replaced);
  - ARP/ping liveness is a poor proxy — an ArtNet node can answer ARP and
    still not be listening on 6454.
  **Shape agreed with Branson (2026-08-25):** the rig is mostly **unicast**, so
  a per-target reachability check IS meaningful and is the main case worth
  building — for a unicast destination, "is that node there?" is a real,
  answerable question. For a **broadcast** target there is nothing to answer,
  so the check degrades to "is the interface/subnet present and up?".
  Critically: validate **directly-attached subnets only**. ArtNet nodes are
  normally on a directly-attached segment; anything routed should be left
  alone rather than guessed at, which also sidesteps the "pingable via the
  default gateway but not actually the show network" false-positive seen on
  ender (192.168.21.214 answered ping while being the wrong interface
  entirely).

  Still informational rather than a blocking NOT READY, and probably ArtPoll
  rather than ICMP — an ArtNet node can answer ARP and not be listening on
  6454.

  **Promoted off the back burner (2026-08-25, Branson):** warn the operator at
  *config* time when a patched unicast target is down, rather than only when
  someone is already chasing a dark universe. The scope is the same as above
  (unicast targets on directly-attached subnets; broadcast degrades to
  "interface up"), but the driver is now setup ergonomics, not debugging.

  **The constraint that makes this non-trivial — poll load must scale with the
  patch, not with the universe count.** This desk routinely patches 50+
  universes (the `ender` soak ran ~53 across 13 node addresses at
  `172.18.2.201`–`.230`). A naive per-universe probe would emit 50+ ArtPolls
  where 13 would do, on a segment already carrying ~2484 pkt/s of DMX — and
  the item above ("MasterTimer misses ticks at full output load") is an open
  question about *scheduling jitter* on exactly that path. Probe traffic that
  perturbs the thing being measured is worse than no probe. So the heuristics
  are load-bearing, not polish:
  - **Coalesce by destination IP, not by universe.** 13 nodes = 13 probes,
    regardless of how many universes each carries. Fan the result back out to
    every universe sharing that target.
  - **Probe off the MasterTimer thread**, and never inside a tick. This must
    not be able to cost a DMX frame.
  - **Config-time and on-demand, not a continuous poller.** On workspace load,
    on patch change, and on an explicit operator "check outputs" — not a
    background heartbeat. A permanent poll across 50 universes is precisely
    the "desk that cries wolf" / spam failure mode this whole area already
    burned itself on once (the 2850-line `sendDmx` flood).
  - **Backoff + hysteresis, as in send health** (output trouble reporting,
    DONE.md 2026-10-15 — built there, reuse it). A
    node that is down stays down between probes; don't re-report. The
    "don't announce recovery until it has held for N" rule wanted there is
    the same rule wanted here — build it once.
  - **Bounded concurrency + a short overall deadline**, so a rig with every
    node in the trailer still finishes the check promptly instead of hanging
    the load path on 13 timeouts.
  Open: whether the result surfaces as a per-universe column in the I/O map,
  the existing readiness indicator (`daeeb97d7`), or a footer chip. Probably
  the first — it is a per-patch-row fact.


- **Verify node IP setting (ArtIpProg) on a real node (2026-10-16)** — built on
  `fix/artnet-unconfigured-nodes` (see DONE.md 2026-10-16), tested only against
  a sink. On the rig: a factory-fresh node, Connections → **Rescan** (does it
  appear amber? if not, add the alias the docs give and Rescan again) →
  right-click → **Set node IP address…** → Ask the node, then Apply. Watch
  with `tools/artnetprobe --listen` from another machine if it misbehaves.
  Open: whether the limited-broadcast poll really leaves by the named
  interface on macOS/Windows (IP_PKTINFO), and whether nodes answer
  ArtIpProg from the old or new address (both are accepted).

- **Art-Net node configuration (ArtAddress) — send path built and proven, but
  no node here applies it (2026-08-26)** — `tools/artnet-config.py`
  builds and sends ArtAddress (OpCode 0x6000); the plugin defines
  `ARTNET_ADDRESS` and has never used it. Phase 1 (read/browse) shipped; this
  is the write half.
  Packet correctness is established two ways: the CR041R **acknowledges** a
  short-name write by moving its NodeReport to `0006` (`RcShNameOk`), and OLA
  logs `ArtNet got unknown packet 6000`, i.e. it read the header and opcode and
  simply does not implement ArtAddress.
  **But nothing applies.** Confirmed across a real power cycle of the CR041R:
  the name came back `CR041R_001`, not the value written. This node acks and
  discards -- it is front-panel configured, and its Status1 port-address
  programming-authority bits never say "set by network". OLA has no ArtAddress
  support at all. **Validating that changes stick needs a node that implements
  network programming**; until then the write path stays out of the UI.
  Two protocol notes worth keeping:
  - The **NodeReport code is how a node acknowledges a config write** -- the
    advertised fields may not change even on success, so diffing them is not a
    test. Watch the code (`0006` RcShNameOk, `0007` RcLoNameOk).
  - The NodeReport **counter does not reset on reboot** on this node (8943 →
    31425 → 31733 across a confirmed restart), so it is useless as a power-cycle
    indicator. The report *code* is the reliable signal.
  Also: `NetSwitch`/`SubSwitch`/`SwIn`/`SwOut` are only acted on when bit 7 is
  high (program 7 as `0x87`); `0x00` means "reset to zero" and `0x7f` means
  "leave alone", so `0x7f` -- not `0x00` -- is the safe default for a field you
  do not intend to change.
  **Re-checked 2026-10-26 (Branson cleared writes to the office node): no
  change.** The office converter is still the CR041R (`172.18.2.10`, firmware
  14, MAC 02:4D:48:12:02:0A). A short-name ArtAddress write was acked with
  NodeReport `0006` (RcShNameOk) and discarded -- the name read back
  `CR041R_001`, exactly as in August. Long-name and port-address writes were
  not run this time (the session's tool permissions stopped further writes),
  so nothing needed restoring. Verdict unchanged: **this node cannot validate
  node configuration; keep ArtAddress out of the UI** (it still is -- the
  plugin only defines `ARTNET_ADDRESS`; the UI has ArtIpProg "Set node IP
  address…" only). `tools/artnet-config.py` fixed the same day: Net/SubSwitch
  defaulted to `0x00` ("reset to physical switches") instead of `0x7f`, and its
  before/after diff compared raw name bytes, so the CR041R's stale buffer
  bytes (below) read as "CONFIG CHANGED".
  **CR041R reply quirk found:** its ArtPollReply reuses an uncleared buffer.
  Byte 23 (Status1) is garbage -- 40 polls gave `0x6F` x11, `0x74` x16,
  `0x00` x11, `0xA9` x3 -- and the bytes after the long name's terminator carry
  other packets' text (OLA's NodeReport, `tcp://…` strings, even our own
  ArtAddress `0x7f` bytes). So Status1 on this node means nothing, including
  the RDM bit (set in ~27% of replies, `0x6F`), and the August "flaps between
  0x00 and 0x74" note was the same bug.
  **Steadied in RC4 (Q165-a):** `ArtNetRdmFlag` (plugins/artnet/src) trusts
  the bit only after five agreeing replies with the same Status1, treats a
  Status1 that changes 4+ times in 16 replies as junk ("no RDM" until 64
  calm replies), and takes an ArtTodData reply as proof. A fresh 47-reply
  capture the same day (`00`/`74`/`AD` only, no RDM bit) is in ArtNet_Test.

- **RDM configuration tooling — DMX-Workshop-class rig setup (NEW, 2026-08-25,
  Branson)** — make the desk the thing you use to *set up* the rig, not just
  drive it: discover devices, read what they are, and set DMX address /
  personality / inverts from the console instead of climbing to every fixture's
  menu or carrying a laptop with DMX-Workshop on it.

  **This is an extension, not a green field — check what's already here first.**
  Upstream QLC+ shipped "Preliminary RDM support" (`9050a7f13`) and it is still
  wired in:
  - `plugins/interfaces/rdmprotocol.{h,cpp}` — a real E1.20 packetizer/parser
    with the standard PID table already defined (`PID_DEVICE_INFO`,
    `PID_DMX_START_ADDRESS`, `PID_DMX_PERSONALITY`(+`_DESCRIPTION`),
    `PID_IDENTIFY_DEVICE`, `PID_DEVICE_LABEL`, `PID_PAN_INVERT` /
    `PID_TILT_INVERT` / `PID_PAN_TILT_SWAP`, `PID_LAMP_HOURS`,
    `PID_SENSOR_VALUE`, `PID_SUPPORTED_PARAMETERS`, …). The protocol layer is
    NOT the missing piece.
  - `QLCIOPlugin::RDM` capability + `sendRDMCommand()` / `rdmValueChanged()`,
    implemented by **both** transports this rig actually uses: `artnet`
    (`ArtNetController::sendRDMCommand`) and `dmxusb` (Enttec DMX USB Pro).
  - `ui/src/rdmmanager.{h,cpp,ui}` (~900 lines) — a discovery worker thread +
    UID list + raw get/set-a-PID panel. NOT reachable: its Fixture
    Manager tab is commented out upstream and here (see 2026-10-26 below).
  So the gap is **workflow, not plumbing**. Today it is a protocol inspector:
  it can talk to a device if you already know which PID you want. DMX-Workshop
  parity means turning it into a rig-setup tool.

  Wanted (roughly in value order — needs its own design doc + a decision on
  where it lives, since Fixture Manager is already crowded):
  1. **A device table, not a UID list** — one row per discovered device with
     manufacturer / model / label / current address / footprint /
     personality, populated by auto-`GET`ting the obvious PIDs on discovery
     rather than making the operator issue each one.
  2. **Editable address + personality in place**, with **overlap detection**
     across the discovered set (the actual reason people open DMX-Workshop:
     "who is sitting on top of whom"). Changing personality changes footprint,
     so re-check overlaps after a personality set.
  3. **Identify** as a first-class button per row (`PID_IDENTIFY_DEVICE`) —
     the "which physical unit is this?" loop, and the single most-used RDM
     feature on a call.
  4. **Reconcile against the patch** — match discovered devices to patched
     `Fixture`s and flag the three mismatches that ruin a focus session:
     patched-but-not-present, present-but-not-patched, and
     address-differs-from-patch. Offer "push patch → rig" and "pull rig →
     patch". This is where it stops being a generic RDM tool and becomes
     *this* console's, and it ties into `docs/dev/RIG_TEST_PLAN.md` /
     `docs/design/SHOW_LIFECYCLE_DESIGN.md`'s Test-Validate phase.
  5. **Sensors + lamp hours** (`PID_SENSOR_VALUE`, `PID_LAMP_HOURS`,
     `PID_DEVICE_HOURS`) — maintenance readout; low priority, easy once (1)
     exists.

  **Hardware finding (2026-08-25) — the ArtNet node on hand does NOT answer
  RDM, so this is currently BLOCKED on hardware.** Probed `172.18.2.10`
  directly from the show VLAN (this Mac is on `vlan0` as `172.18.2.17`):
  - Node identifies as **`CR041R_001`** / `CR041R` — a **4-port** ArtNet→DMX
    gateway, ArtNet 3, `net/sub 0/0`, `swOut 0,1,2,3`, OEM `0x0022`,
    ESTA `0x707A`.
  - It answers **`ArtPoll` → `ArtPollReply`** immediately and cleanly
    (broadcast to `.255:6454`, not unicast back to the requester — relevant to
    the endpoint-reachability item above if that ends up ArtPoll-based).
  - It **ignores `ArtTodRequest`**: four sent (universes 0-3), full wire
    capture of everything from that host, **zero `ArtTodData`** back. Its own
    `goodOut` bits claim RDM is *not* disabled (bit 3 clear on all 4 ports),
    i.e. it advertises a capability it does not deliver.
  - The request packet was checked field-by-field against the Art-Net 4 spec
    before concluding (56 bytes; Net@21, Command=TodFull@22, AddCount@23,
    Address@24) — the silence is the node's, not a malformed probe.
  - **First probe was invalid — corrected same day.** The initial run used
    opcode `0x8080` for ArtTodRequest; the correct value is **`0x8000`**
    (`plugins/artnet/src/artnetpacketizer.h:36`). The node was right to ignore
    an undefined opcode. Re-probed with the correct opcode **and** an
    `ArtTodControl`/**AtcFlush** (`0x8200`, command 1) first — AtcFlush is the
    step that forces a node to actually *re-run* discovery, where ArtTodRequest
    only asks for the TOD it already holds. Result unchanged: 16 `ArtPollReply`,
    **zero `ArtTodData`**. The conclusion stands, but only after the correct
    test. Note QLC+'s own `sendRDMCommand` sends ArtTodRequest with **no
    preceding AtcFlush** — worth revisiting if a real node ever returns a
    stale/empty TOD.
  - **CONFIRMED by DMX-Workshop (2026-08-26).** Artistic Licence's own tool --
    the Art-Net reference implementation -- connects to the node fine and
    reports outright: **"RDM: Node is not RDM capable (Unidirectional DMX)"**,
    with `Rdm Devices: 0 Detected of which 0 active` on all four outputs
    (firmware V0.14, MAC 02:4D:48:12:02:0A, static, LLRP not supported).
    So the probe's verdict was right, independently confirmed.
    The reason is **hardware, not configuration**: RDM needs bidirectional
    RS-485 (talk, then turn the line around to listen), and this node only
    transmits. No firmware setting or update can add it.
    **Correction (2026-08-26): the node was never lying; the wrong bit was
    being read.** GoodOutput bit 3 means "RDM is *disabled* on this port" -- a
    per-port setting, meaningless on a node with no RDM at all. The capability
    flag is **Status1 bit 1**, and across 80 sampled ArtPollReplies it reads 0
    ("not RDM capable") every time. The node reports itself correctly.
    Separately, its Status1 does flap between `0x00` and `0x74` (ROM-booted +
    port-address programming authority + indicator state) roughly 60/40, which
    is almost certainly why DMX-Workshop's displayed lines appear to change on
    their own -- but bit 1 stays 0 throughout, so the RDM verdict never
    actually flips.
    For capability, read Status1 bit 1; GoodOutput bit 3 answers a different
    question. Still worth confirming with a TOD request, since capability bits
    describe intent and a TOD reply is evidence.
    Consequence: RDM must come via the **DMXKing/Enttec Pro USB dongle**
    (wired, bidirectional; `EnttecDMXUSBPro::sendRDMCommand` already exists) or
    via **OLA** acting as an Art-Net→RDM gateway. The CR041R stays a pure
    output path.
    Worth copying from DMX-Workshop's UI: it presents node config as a tree and
    reports RDM device counts **per DMX output**, not per node -- matching the
    per-port finding above.
  - **Control case found, which resolves the earlier ambiguity.** The Pi 5
    build host (`192.168.20.119`) runs an **OLA Art-Net node**, and OLA answers
    the identical probe with `ArtTodData rdmVer=0x01 uidTotal=0 uidCount=0` —
    i.e. a node with an *empty* TOD and nothing attached still **replies**. So
    "silent" is not how an empty TOD presents. The CR041R's total silence is
    therefore best explained by it **not implementing Art-Net RDM at all**, and
    that no longer needs an RDM fixture to establish.
  - **Consequence:** an RDM-capable fixture on a CR041R DMX port will not be
    discoverable *over Art-Net* regardless of the fixture's own RDM support —
    the gateway will not relay it. (Branson has an **ADJ 3Z** on the rig and is
    ordering a known-RDM fixture, 2026-08-25.) Viable transports are therefore
    the **Enttec DMX USB Pro** (wired), or **OLA** as an Art-Net→RDM gateway.
  - **OLA is a zero-hardware development target.** It implements Art-Net RDM
    correctly, and OLA's dummy/simulated-RDM devices would let the entire
    device-table / reconcile workflow be built and tested with no rig at all —
    which would move this item off the ✈️-blocked list. Worth confirming
    before assuming this feature needs hardware.
  - **SUSPICION, not yet proven — QLC+'s `ArtTodRequest` is 25 bytes; the
    Art-Net spec's is 56.** `ArtNetPacketizer::setupArtNetTodRequest`
    (`artnetpacketizer.cpp:161`) builds a 12-byte common header + 9 filler/spare
    + Net + Command + AddCount + **one** Address byte = 25 bytes. The spec
    defines `Address` as a fixed `[32]` array, making the packet 56 bytes.
    Permissive nodes will not care; a strict one would drop it — which is
    exactly the shape of bug that makes RDM "mysteriously not work" against
    real hardware while the code reads fine.
    **NOT verified.** The comparison was attempted against the OLA node on the
    Pi and was inconclusive: OLA answered the 56-byte probe earlier in the day
    but answered neither length while the Pi was under a parallel build
    (load 4.25), so the negative result says more about OLA being starved than
    about packet length. **The test to run**, on an idle box with a known-good
    RDM node: send the byte-exact 25-byte QLC+ layout and the 56-byte spec
    layout to the same node and compare `ArtTodData` replies. Do this before
    concluding anything about QLC+'s RDM working or not working.
  - Develop against **`tools/rdm-sim.py`** when no node is available —
    it answers ArtPoll / ArtTodRequest / ArtTodControl with a synthetic TOD and
    ArtRdm GET for the device-table PIDs. Verified working end to end against
    `tools/artnet-probe.py` (3 simulated UIDs). Note it is deliberately *permissive*
    about request length, so it cannot settle the 25-vs-56 question above.
  - Reproduce any of this with **`tools/artnet-probe.py`**.
  - **Re-checked 2026-10-26 after the Connections rework (RC2/RC3,
    d6662f0b6): capability intact, nothing regressed.**
    Network (from this Mac, `vlan0` 172.18.2.219): three Art-Net nodes answer
    -- the **CR041R** (still the office converter, 4 ports, swOut 0-3, ports 1-2
    transmitting), **OLA on the Pi** (`172.18.2.218`, Status1 `0xD2`, RDM bit
    set) and this console. AtcFlush + ArtTodRequest: CR041R **silent** again;
    OLA answers `ArtTodData uidTotal=0` (TOD empty: its Dummy RDM device is
    not patched to a universe). So **Branson's RDM light on the CR041R is not
    reachable over Art-Net** -- the node is transmit-only hardware. Routes that
    work: a DMXKing/Enttec USB Pro on that DMX line (`EnttecDMXUSBPro::
    sendRDMCommand`; none was plugged in here today), an RDM-capable Art-Net
    node in place of the CR041R, or OLA with an RDM-capable USB widget on that
    line acting as the gateway.
    **App path proven headless:** a QPluginLoader harness on the built
    `libartnet.dylib` (no UI) lists both nodes through `discoveredDevices()`
    with every ArtPollReply field (names, MAC, firmware, OEM/ESTA, RDM, per-
    port address and "transmitting", NodeReport), and `sendRDMCommand(
    DISCOVERY_COMMAND)` to OLA comes back as `rdmValueChanged
    {DISCOVERY_COUNT: 0}`. **That settles the 25-vs-56-byte suspicion above
    for OLA: it answers QLC+'s 25-byte ArtTodRequest** (idle Pi, three runs).
    A strict node is still untested.
    Gotcha for anyone writing such a harness: `ArtNetPlugin` re-declares
    `rdmValueChanged` (upstream does too), shadowing `QLCIOPlugin`'s. The app
    connects by string and gets the right one; a pointer-to-member connect to
    `&QLCIOPlugin::rdmValueChanged` silently never fires.
    **The RDM Manager has never been reachable in this fork** -- upstream
    commented out its Fixture Manager tab (`fixturemanager.cpp`, the `/* … */`
    around `new RDMManager`) and it still is; the "reachable from Fixture
    Manager" line above was wrong. Not a regression; the device-table work
    (1-5) needs its own entry point anyway.
    Next step without hardware: patch OLA's Dummy RDM port to universe 0 on
    the Pi (`ola_patch`) -- then the Art-Net TOD returns real UIDs and the
    GET PIDs (DEVICE_INFO, labels, DMX_START_ADDRESS, personality, software
    version) can be exercised end to end through the plugin.
    Devices shows "RDM" / "no RDM" from Status1 bit 1, so with the CR041R's
    garbage Status1 (see the ArtAddress item) it reads "RDM" in roughly one
    refresh in four. Worth a fix: believe the bit only when it is stable across
    replies, or confirm it with a TOD request.

  **Paradigm answers (2026-08-25 discussion) — no new U→node binding needed.**
  `ArtNetController::sendRDMCommand` (`artnetcontroller.cpp:374`) already looks
  up `m_universeMap[universe]` and sends the TOD request to that universe's
  patched `info.outputAddress` / `info.outputUniverse`. **RDM rides the
  existing output patch**; Inputs/Outputs is already the mapping UI. Likewise
  `handleArtNetTodData` (`:515`) → `rdmValueChanged` → `RDMManager` already
  carries replies back, so "self-populate what it finds" is plumbed — the gap
  is auto-`GET`ting the descriptive PIDs per UID to fill a table.
  - Nuance: the binding is **U → (node IP, node *port*)**, not U → node. The
    CR041R is one IP carrying four universes, so discovery is **per-port** —
    a 4-port node needs 4 TOD requests. Note this is the *opposite* of the
    coalesce-by-IP rule in the endpoint-reachability item above (13 IPs, not
    53 universes): reachability coalesces, RDM cannot. Don't share that code
    path by accident.

  **Reconcile semantics — only one of four cases is an error:**
  | Discovered | Patched | Verdict |
  |---|---|---|
  | yes | yes, same address | matched |
  | yes | nothing at that address | **not an error** — offer to patch it |
  | no | yes | **must NOT be an error by default** |
  | yes | yes, *different* address | the real, actionable conflict |
  Row 3 is the trap: most conventionals and much LED gear have no RDM at all,
  so "patched but not discovered" is the *normal* case on a mixed rig. Flagging
  it would cry wolf on first use — the same failure mode as the 2850-line
  `sendDmx` flood.

  **Blocking design decision — `Fixture` must persist an RDM UID.** Confirmed
  `engine/src/fixture.h` has no `uid`/`rdm` field today. Without a stored
  UID↔Fixture binding, reconciliation can only match on address, which is
  circular: "this fixture moved address" is indistinguishable from "a different
  fixture is now at this address". Row 4 above is unbuildable until this is
  settled. It is a `.qxw` schema addition on `Fixture` — decide before writing
  any UI. Also: there are **no RDM tests anywhere in the tree**.

  **Cautions.** RDM is in-band with DMX on a wired line, so discovery
  interleaves with output — do not run it from the MasterTimer thread, and
  expect it to be visibly disruptive on a live wired universe (fine in
  Construction phase, dangerous mid-show; probably gate it the same way Blind
  is gated). ArtNet RDM is a different animal from wired RDM and node support
  is uneven, so verify per-node rather than assuming. Discovery is a binary
  search over the UID space and is *slow* — the existing worker is already a
  `QThread` for that reason; keep it cancellable. **Needs the rig** to be
  worth anything: none of this is verifiable offscreen beyond parser unit
  tests, and there are no RDM tests today.

- **Simple Desk sliders have no write-back-to-scene path (PARKED, 2026-08-12)**
  — found while triaging the old `live-edit-4.x` branch: Virtual Console
  sliders/XY-pads already write manual adjustments back into the running
  Scene via `CaptureManager::recordOverride()` (wired from `vcslider.cpp`/
  `vcxypadfixture.cpp`, with the existing capture/undo/diff/store workflow —
  see `LiveCaptureDialog`), but `SimpleDesk` (`ui/src/simpledesk.cpp`) has
  zero `CaptureManager` references — moving a Simple Desk fader doesn't
  persist anywhere. Better path is wiring Simple Desk's
  `slotUniverseSliderValueChanged` into `CaptureManager` rather than
  reviving `live-edit-4.x`'s standalone `LiveEditManager` (undo-less
  direct-write) — confirmed `CaptureManager` isn't just equal plumbing,
  it's strictly *better* than that reference implementation:
  - `LiveEditManager::findMostSignificantScene()` (the "which running Scene
    actually wins when several touch this channel" question) was left as an
    explicit `// TODO: Implement HTP/LTP priority logic` stub —
    `return scenes.first();`, i.e. never solved. `CaptureManager::buildPlan()`
    already solves exactly this: it walks running functions in **start
    order**, so the most-recently-started Scene wins LTP per (fxi, channel) —
    a real, reasoned answer, inherited for free.
  - `CaptureManager::buildPlan()` also already flags **chaser-driven
    channels** (a channel currently under a running Chaser step's Scene) and
    excludes them from capture by default — `LiveEditManager`'s plain
    fader-walk has no equivalent, so a Simple Desk tweak there could point-edit
    a Scene the Chaser immediately overwrites on its next step. Preserve this
    exclusion when wiring Simple Desk in.
  - One piece of `live-edit-4.x` *is* directly reusable regardless: unlike VC
    widgets (pre-bound to a specific function), a Simple Desk slider only
    knows an absolute `(universe, address)` — `LiveEditManager::
    findScenesForChannel()`'s address -> `(fixtureId, channel)` resolution
    via `Doc::fixtureForAddress()` is the correct, already-written way to get
    from "which slider moved" to the `(fxi, channel)` pair
    `CaptureManager::recordOverride()` actually wants.
  - Net effect: Simple Desk gains MORE than `live-edit-4.x` ever had (undo,
    conflict/chaser-driven visibility, save-as-new) for less new code than
    reviving `LiveEditManager` would take.
  Needs its own design/implementation pass, not a quick cherry-pick.

- **Cue transition model — hold-on-miss + release-on-transition (4b; DEFERRED,
  needs rig)** — DECIDED framing: a *missed/skipped* cue is a non-event → hold
  last look (no blackout); a cue that *fires* releases what it replaces (outgoing
  look + its effects fade out) → no dangling. Split confirmed: **fade intensity,
  hold position/colour**. Risky part = the intensity-latch core-mixer work; needs
  a live rig to test, so deferred until Branson has rig time. First verify whether
  the current timeline transition already releases the outgoing look or holds it.
- **Pre-positioning / mark cues + ghost visual + dangle detector** — how big
  consoles do move-in-black. See `docs/design/MOVEINBLACK_DESIGN.md`. Slices **1 + 2 SHIPPED**
  (all rig/visualiser-verify):
  - **1** — `MarkEffect` DMXSource (holds non-intensity, auto-releases on reveal,
    `<Mark>` XML) + Mark/Unmark buttons + dashed-violet monitor ghost.
  - **2a** — `CueOutput`: offline "what would this cue output" (unit-tested).
  - **2b** — `CueLookahead`: next cue + lead time for Chaser & Show.
  - **2c** — `MarkPlanner` + **Auto MIB** toolbar toggle: look-ahead → pre-set
    dark→lit→moving movers, dark-gap gated.
  - **2 persist** — Auto-MIB toggle + dark-gap round-trip (`<MoveInBlack>`) + a
    "s lead" toolbar spinbox. DONE.
  - **3 — dangle detector** — `MarkPlanner::dangleFixtures()` + `dangleFixturesChanged`
    signal, forwarded `ProgrammerController` → App footer chip. DONE (unit-tested,
    engine/test/markplanner). Runs regardless of Auto-MIB being on (manual marks
    dangle too). *Needs rig eyeball to confirm the chip reads right live.*
  - **Q38 — per list / per cue** — show default + cue list Use show's / On /
    Off + per-cue "Don't move in black into this cue" (`mib`, `nomib`,
    `allowmib`; badges; MIB tooltip). DONE (feat/mib-overrides). Lead time
    stays show-wide.
  Still open: **verify CueLookahead timing on a rig** (the dark-gap depends on it);
  **force-mark** per-cue override + dark-move fade. Also: mark "to a
  chosen look"; a monitor context-menu Mark action. *(from the cue-policy discussion)*

## Backlog — not started

### Power by intensity, per circuit (Branson, 2026-10-11, long term) — not started
A fixture can now be held to a maximum draw (its measured profile, the
limiter scaling its output). Take it a step further: control the power on
a whole **circuit** by intensity — keep everything on a circuit under its
breaker by scaling the fixtures on it together, rather than capping each
one on its own. Builds on PowerProfile/PowerModel and the power
distribution (circuits) already in the show.

### App-wide UI zoom, Cmd+/Cmd- (2026-09-03, long-term, not prioritized) — not started
Branson: VS Code-style zoom — Cmd+/Cmd- (and presumably Cmd+0 to reset)
scaling the whole app's fonts *and* icon sizes together, not just one view.
Explicitly deferred — filed for later, not asked for now.

Checked before filing, so the scope is accurate rather than guessed: **no
zoom infrastructure exists app-wide today.** Two views already have their
*own*, local zoom (Show Manager's timeline — a slider + Ctrl+scroll driving
a `timeScale` variable, `ui/src/showmanager/multitrackview.cpp`; Lighting
Studio's 2D view — Shift+scroll + trackpad pinch,
`ui/src/monitor/monitorgraphicsview.cpp`), but neither is wired to Cmd+/
Cmd-, and neither is what "app-wide" means here — a real implementation is
a different, bigger thing than extending either of those.

**Why this is more than "add a keyboard shortcut + scale the font,"
concretely:** `QApplication::setFont()` cascades point size to most widgets
via font-metric-based sizing, which covers a lot for free — but toolbar/
tree icon sizes are hardcoded pixel values scattered across many call
sites (`toolbar->setIconSize(QSize(20, 20))` appears independently in
Fixture Manager, Connections, and other manager toolbars — see this
session's own work touching several of them), not derived from font size
at all, so they would not scale along with text unless each site is
touched (or icon sizing is centralized first — its own small refactor)
and multiplied by the same zoom factor. `resources/qss/default.qss` also
has fixed-pixel padding/margin values that a pure font-size change
wouldn't touch, so some chrome would look increasingly cramped or loose
relative to the scaled text as the zoom level moves away from 100%.

Not scoped further than this — no design doc yet, no decision on whether
to centralize icon-size constants first or handle it site-by-site, no
persisted-setting design (a `workspace/uiZoom` `QSettings` key alongside
the existing theme/tab-label-mode pattern would be the obvious fit). Pick
up with a real audit of every `setIconSize()`/fixed-pixel QSS rule before
estimating effort for real.

### Show lifecycle: Construction / Test-Validate / Run *(2026-08-15, design doc written; "Production" renamed Run 2026-10-16, D1)*
See `docs/design/SHOW_LIFECYCLE_DESIGN.md` — names the three phases a show moves through
(building off-rig → matching Lighting Studio to the real rig → running on the
road) and settles that this is **not** a third `Doc::mode()`; it's the
existing Design/Operate axis crossed with a "connected to real hardware vs.
fully simulated" axis (real, non-Dummy plugin patched **and** not Blind —
Blind turned out to already BE the "disable real output" switch: it silences
every protocol at once and is already Design-only/force-off-in-Operate, see
the doc's 2026-08-15 finding). Concrete follow-ons, none started:
1. Compute + expose the rolled-up connected/simulated state
   (patched-and-not-Blind).
2. Surface it as a footer chip alongside MTC/Load/Power.
3. Decide + build whether Design mode should auto-engage Blind when it
   detects real hardware patched, rather than leaving it opt-in.
4. Turn `docs/dev/RIG_TEST_PLAN.md`'s manual checklist into an actual in-app
   Test/Validate workflow (bigger; own design pass on where it lives).
5. Retroactively tag the rest of this backlog by which phase it serves, to
   sharpen prioritization instead of treating it as one flat list.
