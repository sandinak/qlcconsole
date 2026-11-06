# TODO — open work

Only what is still to do. Shipped work — with its build notes, follow-ons
and "not yet verified on the rig" caveats — is in [DONE.md](DONE.md); the
sections moved there on 2026-10-11 are under "Moved from TODO.md". When
something ships, move its entry to DONE.md. See also the session memory
under `~/.claude/.../memory/`.

**Release numbering (board Q193, 2026-11-01):** 0.3.1 merged into 0.4.0. One release, **0.4.0**, carries the 0.3.1 polish, all of 0.4 and most of 0.5; 0.3.1 never ships separately. "0.3.1", "0.4" and "0.5" in the sections below are roadmap names and stay as written. Releasing is the user's call.

---

## Cue lists on the timeline, the timeline on timecode (design, 2026-11-04) — waiting on Q216–Q221

Design: [docs/design/TIMELINE_SHOWTREE.md](../design/TIMELINE_SHOWTREE.md).
The cue times in the Show Tree are the one truth. A cue list on a Shows track
becomes a cue-list clip that draws and edits those times; the timeline's
playhead is a clock the Show Tree chases (GO by position).
- **P0** (ROI 30): a timeline edit to a cue list's generated chaser is refused,
  with the reason; stale shows.md lines fixed. **In progress** (tlguard).
- **P1** (ROI 4): cue-list clip. It draws sections, cue ticks and anchors from
  the tree, and its edits are Q198 tree steps:
  - drag a tick to set that cue's time;
  - drag a section edge to set the section start (⇧ ripples later sections);
  - drag the clip to shift the whole list.
- **P2** (ROI 5.3): the timeline's clock as a fourth TimecodeCues source. A
  song file runs its cue lists; Learn works with no DAW.
- **P3** (ROI 6): markers bound to sections by name; "Sections from markers";
  "Cue list here…" replaces "Link cue list…" (menu count flat).
- **P4** (ROI 8): a "Timeline 0 =" field and a timecode ruler. The offset
  mapping moves into the engine; the hour snap is announced and can be undone.
- Not built: MTC out (LATER, ROI 1.6). LTC out (NO: needs an audio engine).
- About 11.5 days in all. Starts after the board answers.


## Release / Untrack wording and the tracking span (Q222, untrack, 2026-11-04) -- built

ShowTreeController::untrackPlan / untrackLayers / trackSpan
(showtreetrackops.cpp); grey-row menu (Release After This Cue, Untrack From
This Cue, Go to <cue>) in ShowTreeView::fillLayerMenu; T column "⤳ N" with
the span tooltip; `untrack` command word; glossary + show-tree.md + course.
Decided (can change): Untrack is hidden on a layer's own row (not disabled);
it is refused with the reason when the cue before is Cue-only or a reusable
cue's use, the layer is included or back at the base, or it's the first
sub-cue. The grey row's menu grows by two items (Untrack, Go to), by the ask.

## Duplicate / Copy / Paste on lists, productions, reusable cues (Q215, dup-lists, 2026-11-04) -- built

ShowTreeController::pasteCueList / duplicateCueList / pasteProduction /
duplicateReusableCue (showtreeproductions.cpp); rows' menus + EditKeys in
ShowTreeView. Decided: a copied list shares the reusables its cues use (can
change); a list's Hits are not copied with it, a production's are; list /
production notes (not cue notes) stay with the original. Tests: showtree_test
duplicateAndPasteListProductionReusableCue, showtreeview_test
listsProductionsAndReusableCuesDuplicateCopyAndPaste. Leftover: the command
line has no `copy list`.

## The tree and the rig in step (rigsync, 2026-11-04) — built, leftovers

Branson on 90c9ef05d: "i disabled USTs tracking in a cue .. but the display
didn't change"; "a hit can't be soloed .. should have gone to nothing". Root
cause: in Design the preview played OVER a list running for real (any GO
in Design leaves one running), winning only the channels it set, so
anything an edit took away stayed lit from the run, and an empty hit or a
Solo showed the run. Now the run is held off stage (its cues' faders write
nothing, Universe::setFunctionsHeld; their effects draw nothing) while the
preview or a Solo has the stage (`ShowTreeController::runHeldOffStage()`),
back when the preview stops or on GO. Edits with nothing running already reached the rig in one
frame (measured on DanceHysteria-0.6). Tests: showtree_test
(anEditThatTakesLightAwayShowsOverARunningList,
anEmptyHitShowsNothingAndSoloShowsOnlyIt,
anAutoFollowUnderThePreviewStaysOffStage), showtreeworkbench_test
(theEditIsAllThatShowsOverARunningList), effectinstance_test
(anEffectOfASceneHeldOffStageDrawsNothing). Leftovers:
- Release on a carried-in row (and its docked menu's "Release (Leaving
  This Cue)") still means "gone when this cue is left": the cue itself
  keeps the layer. If "disable tracking here" should take it out of THIS
  cue (grandMA-style release), that is the user's call (would be a new
  meaning for the grey row only).
- Operate stays blind by design (LIVE_BLIND.md): a cue live on stage
  changes on its next GO, not when edited.
- A generated scene a VC-fired hit shares with a cue of the running list
  is held off stage with it while a preview plays (rare: Design, a run,
  a preview and a hit at once).
- "Why is this on / not?" (whyon) doesn't yet name the hold: a light the
  run would light, dark under a Design preview, is not yet explained as
  "held off stage by the preview".

## Timeline guard for a generated cue list (tlguard, 2026-11-04) -- P0 built

A Show Tree cue list's chaser on a Shows track refuses step edits
(SequenceItem::stepsLocked; test showmanager_test). Function Manager, Build
and the live-edit dialog were already locked. Open (design note
TIMELINE_SHOWTREE.md, board Q216-Q221): P1 cue-list clip, P2 timeline clock
for clipped lists, P3 markers and sections, P4 timecode ruler.

## Edit keys the same in every list (WF1-A, W1-27, W2-Q2; wfkeys, 2026-11-04) — built, leftovers

⌫ / F2 / ⌘C ⌘V / ⌘D / ⌘F / Esc in the Show Tree, Build (Palettes,
Effects, Scenes), Fixtures, Plot layers and the plot (ui/src/editkeys.*,
one table in docs/features/shortcuts.md#edit-keys-the-same-in-every-list).
⌘D is focus-scoped (a list with a selection), Dump DMX keeps it elsewhere
until Q214 decides. Leftovers:
- ⌘F in Build and Fixtures comes from wfaudit1 (fix/04-wfaudit1-friction).
- The Show Tree's Library has no ⌘D (palettes duplicate in Build).
- ⌘D in the Fixtures tab is not on ⌘Z (the copy is deleted with ⌫).
- Connections' Universes tab keeps ⌘D = Delete Universe (Q214).

## Words and layout (wfwords, 2026-11-04) -- built

Buttons Title Case (menuwords_test reads code-given button texts; stock `.ui`
dialogs untouched), Show Review "Not a Problem" everywhere, Show Tree default
split 17:42:41 set at the first real width, footer chips drop by priority when
the window is narrow. Tests: menuwords, layoutsanity
(theShowTreeShowsCueNamesWithFreshSettings, theFooterChipsNeverOverlap).
Leftovers:
- Buttons whose text is set through a variable not named ...Btn/...Button/
  ...btn (e.g. `m_encLearn`) are not read by the check; a name outside the
  pattern is found by eye.
- The tab bar still overflows at 1024 wide (Run is cut off).

## Notes beside the Show Tree (Q212, notes, 2026-11-03) — built, leftovers

Notes pane under the cue page (follows the tree row or a Library palette),
dated entries with author, open/done, Snapshot the view (Rig Overview
camera + ~480 px JPEG + cue on stage) and Show me; ✎ marks on rows with the
newest note on hover; All notes… with filters, jump, Print and Export
(PDF / Markdown). Cue note = first entry; Needs work = an open note.
docs/features/show-tree.md#notes. Leftovers:
- Palettes in the Library carry no ✎ mark yet (their notes show in the
  pane and the list).
- No notes on a reusable layer or cue, a group set or a section row.
- The cue sheet and the web remote still show only the cue's own note,
  not the other entries.
- Pictures are kept in memory for the session (undo can bring a snapshot
  back); only those a note uses are saved.

## Track ops: Bring Into This Cue / Start Here Instead (trackops, 2026-11-03) -- built

A grey (tracked) layer's right-click, in the tree and the Workbench: **Bring
Into This Cue** (own, same values, same output; later cues track from here)
and **Start Here Instead** (the layer's start moves to this cue; the cues
from the setter up to here lose it; confirm names them). Engine:
`ShowTreeController::bringIntoCue / startLayerHere` (+ `...Plan`),
`engine/src/showtreetrackops.cpp`, verified by re-resolving the edited list.
Decided: sub-cue lists and Looks refuse both (can change); a layer that is
one effect/aim across layers set by a cue look is refused (can change).
Leftover: Bring Into on a child layer needs its parent brought in first.

## Band instruments as stage targets (bandtargets, 2026-11-03) — built

Keyboard, Grand piano, Upright piano, Equipment rack and Speaker (on wheels)
beside Drum Kit: a default size, aim height, a plan glyph on the plot, a solid
shape in the Rig View, `]` / `[` turn them. Not built (listed for the user): a
Mic / Vocalist stand and a Guitar / Bass amp (a Person plus a Speaker covers
both for now); a facing field in Edit Target (the keys turn it); the Drum Kit
itself still draws as a rectangle, not a kit.

## Fountain fire height, fixtures and WLED in it, wands (fountainwand, 2026-11-03) — built

Fire height (silk from the inner wall's top; old shows keep the flame's
top), fixtures dropped in a fountain mount in it (pars aim up at the
flame), Pixel path (wall top rings, base ring, up the flame, wand), the
wand held by a Person. docs/design/FOUNTAIN_WAND.md. Fixed: the flame on
another layer than its fountain (locked, left behind on a move). Decided
(can change): a wand is a pixel path, not a stage look; the fan unit stays
on the base. Leftovers:
- A held wand follows its performer's plot position; per-cue blocking
  positions, when they land, should feed PixelPath::heldCentre().
- The silk is drawn only while its relay is on (a fan blowing it); off,
  nothing shows above the wall.
- The structure editors' own flat planes (Front/Side in a Lighting Studio
  Editor) draw no flame outline or pixel path yet; the plot's Front and
  Side views do.
- Path LEDs light nothing (by design); a hand-held wand's glow in haze is
  not drawn.
## Studio selection: one rule for every item (studiosel, 2026-11-03) — built

A click on the plot selects exactly the item clicked (a line with its
people, a truss with its own parts); Select All Similar on every kind;
Select Group on a member and the Plot layers row select a group. Groups
stay as data. Built assemblies (MonitorGroup::assembly, set by Add Fountain and
component stamps; older fountains marked on load by Fountain::repair)
select whole; a
double-click enters one. A right-click inside the selection acts on all of
it (Lock/Unlock N Items, Move to Layer, Duplicate, Delete), one undo step.
Targets duplicate (Cmd/Ctrl+D) and copy-paste whole. Decided (can change):
Ctrl+G and Move selection here take a user group along only when all of it
is selected; a fixture has no own lock, so Lock N Items counts the rest;
a duplicate goes half a metre on, as Duplicate Selected always did.
Leftovers:
- The Rig Overview picks fixtures only: no targets or structures to select
  there yet, so nothing to change.
- Select All Similar for a stage piece (centre line, apron) is not offered.
- No per-item Hide on the plot (a layer's eye hides): nothing to apply to a
  selection there.
- Free fixtures are not in the plot's clipboard (a truss's are): Duplicate
  and Cmd/Ctrl+C skip them.

## Blocking: per-cue target places (Q211, blocking, 2026-11-03) — built

Named blocking sets of target offsets (and line/area shapes) on the
targets, a tracked Blocking on the cue, one reader
(MonitorProperties::targetPosition) for every view and solver, the live
blocking set by the cue's Collection as it starts, the Studio's picker,
banner and Default ghosts, BLK-01. docs/design/BLOCKING.md. Course: a
teacher note in level-2 lesson-05 (spike marks). Leftovers:
- No editor for an area's footprint in a blocking (model, file and solver
  have it); the target dialog edits Default only.
- Focus nudges move the Default mark (the venue's), whatever the Studio
  shows.
- Hits leave the blocking alone; a hit with its own blocking is not built.
- Two lists running: the last cue to GO sets the blocking for both.
- Show Review's other checks read Default; only BLK-01 reads each blocking.
- A course lesson on blocking (level 3?) if classes ask for it.

## Gobos with no picture (feat/04-gobo-no-picture, 2026-11-03) -- done

"gobo didn't render in the rigview .. thought we had that." Definitions
that name gobo slots but have no usable picture (no Res, a folder Res,
a missing file) drew a plain beam. Built: `BeamOptics::standInMask`
(deterministic per slot, cached), `GoboLibrary::hasPicture/standInCaps`,
Gobo Wheels suggestions + Fit Suggested Pictures, Show Review FIX-17.
Tests: Monitor_Test::aGoboWithNoPictureDrawsAStandIn,
aGoboWhosePictureIsAFolderDrawsAStandIn,
goboWheelsSuggestsPicturesForBareSlots, ShowReview_Test::fix17.
Move to DONE.md when landed.

## Designer workflow walk (wfaudit1, 2026-11-03 night) — built

"The same action works in one place and not another, things are hard to
find, and failures are silent." Built: drop refusals said (Show Tree, cue
and layer pages, Workbench rows), several-selected menus that act on all
(cues, layers, palettes, Library functions, Plot Layers folders, a truss's
Studio), a save that can't be written whole keeps the show and names the
file, a failed autosave shows in the chip, the web Run page says why a hit
did nothing in Design, the Look Editor warns when a position or zoom look
can't move the scene's lights, ⌘F in Build and Fixtures. On the board
(scratchpad wfaudit/board-questions.md): one set of edit keys everywhere,
Duplicate for cue lists and productions, ⌫ on the plot and in Fixtures
(with wf2's W2-04), lock outside the plot, the audience (Q160). Leftovers:
- Show Review **Fix ▸** on a finding whose cue is gone does nothing, silently.
- Workbench **Add existing…** with nothing new picked says nothing.
- Plot Layers drops onto a locked layer or a vanished group say nothing.
- The Fixtures tab's circuit drop ignores a refused assignment.


## Area cover: an Aim at an Area covers it (Q209, areacover, 2026-11-03) — built

Zones by where the lights hang (least-squares match of x and depth key to
the cells), subject height above the surface under each zone, coverage at
subject height, the plan under an area aim's Position row, MOV-13.
docs/design/AREA_COVER.md. Changes the DMX of existing area aims
(CHANGELOG ▸ Changed). Leftovers:
- The Look Editor and the formation editor show no area plan yet (the Show
  Tree layer page and its pop-out do).
- Coverage reads the zoom from the scene's own values or the fixture's
  current ones, not a Zoom look on another layer of the cue.
- Coverage counts a low beam passing through subject height as lighting
  everyone in its path: no shadowing by the people in front.
- No "point at the centre" option: add one only if a show needs it.

## The follow spot's stick is the patched controller (fix/04-followspot-real-stick, 2026-11-04) -- built

Third report: "◎ Follow: AT-Spots ← ES-Witch · no stick" with his DS4 on
HID, patched to U01-Office with the Sony DualShock 4 profile (dink). The
stick was bound only in `ProgrammingManager::showEvent()`
(`autoBindFromProfile()`): never shown that session, or shown before the
pad connected, and there was no stick. Earlier tests bound by hand
(`bindPanAxis()`) and fed `slotControllerInputChanged()` directly.
`ProgrammerController::refreshStickBinding()` now binds on load, on every
input patch/slot/device change (`InputOutputMap::inputPatchChanged`, new)
and once a second while a follow spot has none; HID profile slots, else
the left stick's roles. Chip and status name the pad
(`followSpotStickName()`). `QLC_FSPOT_PROBE=1` (+`QLC_FSPOT_PROBE_GO=1.1.2`)
logs it all to stderr once a second. Tests drive the real HID plugin with
a virtual DS4 (`QLC_HID_VIRTUAL_PAD`, `HIDVirtualPad`):
`FollowSpotCue_Test::aCueFollowSpotMovesFromEveryRoute`,
`theStickIsWhicheverPadIsPatched`, `hisShow`. Open: confirm live with his
DS4 on dink. Move to DONE.md when landed.

## A cue's follow spot and effects, every time (fix/04-cue-fires-effects, 2026-11-04) -- built

"Followspot STILL NOT WORKING in 1.1.2 ... and no indicator" and "selecting
a cue doesn't always fire its effect". Reproduced on a copy of
DanceHysteria-0.6. Two causes:
1. Any scene open in the Workbench kept the stick (`joystickScene()` only
   looked at running cues with nothing open), and one nearly always is: no
   follow, status Off, so no indicator. Now only a follow spot open there
   takes it; a cue's follow spot wins over any other Workbench scene. The
   subject keeps its height (z) while it moves; the held beam
   (lastPosition) is the follow spot's only.
2. The effect scripts' fader is Override, and a Show Tree preview's scene
   faders are Override too: one requested later went above it, so from the
   second cue selected on (while any effect kept the fader alive) a dimmer
   or position effect was covered. `Universe::requestTopFader()` keeps it
   on top of its priority.
Footer chip **◎ Follow: AT-Spots ← ES-Witch · stick moving** on every tab.
Tests: `FollowSpotCue_Test` (gate: `aCueFollowSpotMovesFromEveryRoute`,
`everyCueSelectedShowsItsEffect`; `QLC_FSPOT_SHOW=<show>`: `hisShow`,
`everyCueFiresItsEffects`), `Universe_Test::aTopFaderStaysAboveLaterFadersOfItsPriority`,
`EffectInstance_Test::followSpotInARunningCueMovesWithTheStick` ("a scene
open in the Workbench"). Open: an effect inside an override hit is still
under the hit's own (HitOverride) faders. Move to DONE.md when landed.

## Follow spot in a cue doesn't move (fix/04-followspot, 2026-11-03) -- done

"1.1.2 has a follow spot on tspots.. and it doesn't move." Cause: the stick
only drove the Workbench's focused scene (`ProgrammerController::applyDesignJoystick`),
so a follow spot in a running Show Tree cue never moved; not a regression
(same on dab9f579a, before arms). Now `joystickScene()` falls back to the
running cue's follow spot; across cues it carries on when tracked, keeps
its subject with Persist, else glides it back over the cue's fade;
`followSpotStatus()` drives a light in the effect editor, a ring and lines
on the rig view and a **follow** flag in the Show Tree. Lengths in effect
params (`unit: "cm"`) and the target editor follow Settings → Units.
Tests: `EffectInstance_Test::followSpotInARunningCueMovesWithTheStick` (one
head, two arms), `followSpotKeepsItsSubjectAcrossCues`, `LookEditor_Test::aLengthShowsAndTakesTheChosenUnit`,
`theFollowSpotEditorSaysWhatItIsDoing`, `ShowTreeView_Test::followSpotFlagInTheTree`,
`Monitor_Test::theFollowSpotIsDrawnOnThePlot`. Open: line-of-sight "lost"
state in the indicators. Move to DONE.md when landed.

## Area cover counts the deck too (Q213, feat/04-area-cover-both, 2026-11-03) -- built

A spot is lit only if lit at head height AND on the deck/riser/step under
it; plan hatches deck dark and heads dark differently with a legend; MOV-13
names which is missing. docs/design/AREA_COVER.md. Aim points unchanged.

## Area cover: ties go to coverage (fix/04-area-tie, 2026-11-04) -- built

Exact least-squares ties (UST arms, mirrored tower washes) are settled by
coverage at both heights: integer costs, tight pairs from the Hungarian
potentials, coarse bitset grid, second assignment + bounded swap search.
His Chorus 57.5% -> 74.9% (best of 384 ties). docs/design/AREA_COVER.md.
Tests: `AimSolver_Test::areaTiesGoToTheBestCover`, Chorus floor back to 0.7.
Zones are cached by their inputs (dim-when-blocked asks every tick):
`HotPath_Test::anAreaAimThatDimsWorksOutItsZonesOnce`.
Move to DONE.md when landed.

## Faster packaging (tools/04-pkgfast, 2026-11-03) -- done

`make dmg`: no tests (`QLC_NO_TESTS`, now also in ai/ and tools/artnetprobe), ccache for both slices, build
dir kept on /Volumes/Ext/build; `make dmg ARCH=native` arm64-only; ender's
ccache on /Volumes/Ext/ccache (`platforms/macos/ccache-env.sh`). Move to
DONE.md when landed.

## Rig view smooth on a busy show (fix/rigperf, 2026-11-03) — done

"dink's visualization has gotten very chunky." It was neither the new
definitions (about +10%) nor a regression (2926 behaves the same): the frame
budget held the view to 40% of one core (interval 1.25 x max(wall,
2 x CPU)), a GO's burst of re-thrown beams held the rate down for seconds,
every ray asked every piece of scenery (`Scenery::rayHit()` walked its
panels, allocating), and the cursor readout re-rasterised the whole frame
on the CPU over GPU frames. Fixed in `StructureStudioView` (share of
`SystemProbe::performanceCores()` less two; Operate 40% of one core and
15 fps; quick recovery; scenery boxes; a moving curtain re-asked alone;
one-row pick; low-priority pool); the picture is pixel-identical
(`RenderPerf_Test::beamShortcutsDrawTheSamePicture`). Leftovers:
- The GUI-thread work that could still move: docs/design/OFFLOAD.md (the
  user picks).
- `stresstest/engine/showbuilder.cpp` hangs the mixed rig at Z = 6000 (m;
  meant mm), so renderperf's beams land on nothing. Fixing it rebases the
  `gui` perf point: do it with a `make perf-record`.
- Measure on Windows (hybrid-core count via efficiency classes, D3D path)
  when the VM is back; Linux big.LITTLE detection is untested on hardware.

## Rig view hover jitter (fix/04-rig-hover-jitter, 2026-11-03) — done

"The rig view moves a very small distance back and forth when the mouse
moves over it." Cause: pointer frames drawn full quality between a running
show's light frames (`StructureStudioView::paintEvent`, `m_hoverFrame`);
on the 2D plot the readout label's width moved the footer scroll bar.
Tests: `RenderPerf_Test::hoveringARunningShowKeepsTheRigStill`,
`Monitor_Test::hoveringThePlotKeepsItStill`. Move to DONE.md when landed.

## A light buried in the stage (FIX-15, place, 2026-11-03)

"UST-2 should throw an error in the footer if it's inside a stage feature."
Built: `PlacementCheck` (engine/src/placementcheck.*; the body is the rig
view's box, the volume each StagePlatform block, separating-axis overlap
over 3 cm), Show Review **FIX-15**, the footer's `fixtures.buried` error
(Sit on Top / Show; re-read 300 ms after any edit or rig change), the red
ring on the plot. Tests: ShowReview_Test::fix15 + fix15Geometry,
ProgrammingManager_Test::aBuriedLightIsAFooterErrorUntilItIsFixed,
Monitor_Test::aBuriedLightIsRingedOnThePlot.

- **Zero lens / zero pan-tilt defaults built (2026-11-03, branch
  fix/04-zero-lens):** `Fixture::effectiveBeam()` (15 degrees moving head or
  scanner, 25 colour changer, 6 effect, 40 LED bar, 20 other; pan 540 / tilt
  270 for a moving head with pan/tilt channels, 0 without) feeds the rig view
  traits, RigHeads, AimSolver and positionToValues; the Fixture Manager and
  plot tooltip say "(assumed: the definition says 0)"; Show Review FIX-18
  once per definition. Tests: Fixture_Test::zeroLensAndRangeGetDefaults,
  AimSolver_Test::zeroRangeDefinitionsStillAim, ShowReview_Test::fix18,
  Monitor_Test::aZeroLensFixtureDrawsABeam.
- **FIX-16 built (Q208, 2026-11-03):** "not on its row's riser" as a
  warning (`PlacementCheck::offRow()`, footer `fixtures.offrow`, amber ring,
  Sit on Top); on his show copy it flags UST-2 only. Row-mates = same type in
  a group, a name series or a line (docs/design/SHOW_REVIEW.md FIX-16).
  Tests: ShowReview_Test::fix16*, ProgrammingManager_Test and Monitor_Test
  below.
- (Earlier note, now superseded:) On the user's show copy nothing is flagged: UST-2 (z 0) abuts USP1's
  upstage edge (its body ends at y 3.760, the deck starts there) instead of
  being inside it, and the other USTs are deck-mounted at 0.61. "Not on its
  row's riser" would be a different check (siblings deck-mounted, this one
  not) -- not built; say if wanted.
- Not yet: scenery (walls, bowed sheets) and tower bodies; a Fix for a
  light that overhangs an edge (it only says "Move it clear").

---

## Steam Deck / gamepad remote — built, leftovers (deck, 2026-11-03)

Built on feat/04-deck-remote: /focus page (WebAccessFocus), gamepad
reading on /focus and /run (WebAccessGamepad, tables shared with the
tests), Run page Show Status + Focus link + 1280×800 landscape layout,
controller test panel, Valve IDs in GamepadLayout::deviceKind(). Docs:
docs/features/gamepad-remote.md.

- [ ] **Hardware check on the user's Deck** (checklist in
  gamepad-remote.md "Hardware checklist"): controller test screenshot,
  Wi-Fi round trip, back grips via Steam Input, Game Mode launch, and the
  VID:PID the console's controller list shows for the Deck.
- [ ] Blind / Solo from the remote: not built. decided: the remote may do
  only what the phone Run page may (can change); Blind/Solo would be new
  remote powers.
- [ ] Teaching instructor flags (q20x): the remote reports `teaching` and
  holds nothing extra; add the check once the flags land on main.
- [ ] Starting a focus from the remote: decided no, the venue choice is
  the console's (can change).
- [ ] A JS test harness for the page scripts (none in the repo): the
  mapping tables are C++ and tested; scripts are only syntax-checked by
  hand (node --check).

## Layout sanity — what layoutsanity_test found (lay, Q207, 2026-11-03)

`ui/test/layoutsanity` opens every screen at its minimum, 1280x800 and
1920x1080. These are in its `knownIssues()` (reported, not failed); fix
one, then delete its row there. At 1280x800 and 1920x1080 only L1 shows.

- **L1** Colour look: the picker's Hue/Sat/Red/HTML rows and its well lie
  under the sliders and the "Wheel / ring" row, at every size (the user's
  find). feat/04-cpick fixes it (checked: clean on that branch); delete the
  two L1 rows when it lands.
- **L2** Workbench at its own minimum (~310x239): the header's title
  squeezed to half its height. The header needs a minimum height that holds
  the 15pt title.
- **L3** Pan/Tilt look at the Workbench minimum: the XY pad over its
  Home/Front/Down/Left/Right column (squeezed to 6 px wide).
- **L4** Aim look at the Workbench minimum: "Dim lights that can't see the
  target" cut short.
- **L5** Build's scene page at the Workbench minimum (~659x326): "How this
  works" cut off at the bottom.
- **L6** Show Settings at its minimum (~298x473): the Teaching rows
  (Lesson, Handout link) squeezed to 5 px; the word-wrapped PIN text takes
  their height. Needs a minimum width (or a heightForWidth-aware layout).
- **L7** Create groups / Head types at their minimum: the group preview cut
  off, the LED map over its "16 × RGB 16 × W" caption (on Linux, its
  order strip cut instead).
- **L8** Scene group looks at its own minimum (~341x184): the Edge/Speed
  row over the Targets/Looks captions and the targets list.

## Aim round trip — leftovers (fix/04-ust-aim, 2026-11-03)

"USTs still pointing SR" (cue 1.1.1, line aim, Spread +5). Built: one DMX
-> beam readback in the engine (`AimSolver::headDirection()`, fine byte
read), band rounding in `Fixture::positionToValues()`, the Blind view's
levels on every fixture (`Fixture::setOwnChannelValues()`), the studio and
Focus line trace through `AimSolver::linePointFor()`, and the table-driven
round trip `AimSolver_Test::aimedHeadsLandOnTheirTarget` (a new rig is one
row). On the user's show the DMX and the live rig view land all 16 UST
beams within 14 cm of their points.

- DONE (feat/04-arms, 2026-11-03): each arm of a multi-arm fixture aims on
  its own (user: "multi arm fixtures can and should be relative ... when
  grouping or crossing we def should take each arm individually"). Members
  are `GroupHead`s (`AimSolver::aimMembers()`, `arms()`, `armOffset()`);
  palette, effects, the studio and the scene-aim checks use them. Decided
  (can change): an arm's origin is the middle of its share of the body's
  Width along the body (the slice the rig view draws it in; definitions
  have no per-head offsets); Focus mode still focuses a fixture as one
  (calibration is per fixture), on its arms' middle point; the 2D plot
  draws one trace per fixture to the arms' middle; line-of-sight dimming
  dims the whole fixture when either arm is blocked. Left for the tvis
  branch: `lineplanview` / `linespreadpreview` still number fixtures, not
  arms -- switch them to `aimMembers()` / `orderedLineMembers()` /
  `lineMemberPoints()`.
- Ask: which app and view showed SR? `/Applications/qlcconsole.app` and
  `~/qlcconsole.app.prev` are build 2825, from before the band readback
  (2848); with the same bundle id, a .qlcc opened from Finder can start one,
  and it draws exactly this (band middle read as a quarter turn, stage right).
- Show notes for the user: cue 0.0.11's function layer "USTs-Opening"
  (scene Opening, PanTilt "45" = pan 375 on a 360 head) tracks into 1.1.1
  beside the USTs layer and drives the same pan/tilt (the layer wins only by
  order); UST-2 is not mounted on its riser (z 0, the others 0.61).

## Colour picker: stage swatches, hover names, resize layout (cpick, 2026-11-03) — built

Done: 48 stage-colour swatches (`PaletteNaming::stageColours()`), hover names
(swatch and field), Blacklight drives UV, Look Editor colour page scrolls /
reflows (tests in lookeditor_test). Leftovers: native/Windows dialogs and the
VC / Monitor / Show Manager `QColorDialog::getColor` call sites still use Qt's
plain dialog; a user-editable swatch list.

## Settings ▸ Performance — built, leftovers (rttune, 2026-11-03)

Built: per-OS system check (SystemTuning, ui/src/systemtuning*), full speed
during Operate (macOS App Nap activity / Windows EcoQoS opt-out + no idle
sleep), Show Status entry "system.tuning" in Operate, MasterTimer reports
its scheduling (all three platforms; Linux SCHED_FIFO path verified on the
Pi 5 by the perf runs). Help: docs/features/system-tuning.md.

- Windows side unmeasured (VM down): EcoQoS opt-out, power-plan probe,
  SetThreadExecutionState. Measure on the Windows host when it is back.
- macOS App Nap hold not measured in the harness (qlcstress is not an app
  bundle, App Nap never applies to it); relies on Apple's documented
  behaviour, as ControllerKeepAlive does.
- Timer slack (PR_SET_TIMERSLACK) measured on Linux: no effect within the
  run-to-run noise, not added.

## Performance page: precise limits and jitter — built, leftovers (perfprec, 2026-11-03)

Built: `perf_sweep.py --refine` bisects every limit after the doubling
ladder (to ~5%, each count decided by two runs of three, noisy band named), qlcstress
splits jitter by cause (timer wake-up lateness against its own loop, stall
inside a tick, late frames as scheduling or load), `extract.py precise`
→ `docs/perf-data/edges.csv` + `jitter.csv`, jitter charts, the gate runs
both self-tests. Pi 5 measured (normal and SCHED_FIFO), i7 container
measured (normal only). Leftovers:
- **M4 Max (ender) night run**: `~/perfprec-runs/night.sh`, waits for
  22:00–08:00 and an idle machine; extract with `extract.py precise m4max
  normal ~/perfprec-runs/normal`, redraw, replace the old M4 (RC8, dink)
  column of the envelope.
- **i7 container can't grant SCHED_FIFO** (unprivileged LXC: `prlimit
  --rtprio` refused) — the RT A/B there needs a privileged container or
  bare metal.
- **Pi 5: `olad` spins a core** (110% CPU, 62 h of CPU in 12 days), left
  running during the runs (stopping it was not ours to do). Rerun the Pi
  after it is fixed or stopped; the numbers are a Pi with a busy daemon.
- Other sweeps (`r`, `bp`) are not refined yet; the Art-Net wire limits are
  still ladder-only.

## 0.4.0 coverage audit — leftovers (tcov, 2026-11-02)

Map: docs/dev/TEST_MAP.md Part 1b. Decide or fix:

- **Engine Show keeps a deleted function's block** (only the Shows tab
  prunes it); `Show_Test::aSceneDeletedWhileTheShowPlaysLeavesTheRestPlaying`
  has the QEXPECT_FAIL ("TODO 0.4 tcov").
- **"Design only" effect fades:** CHANGELOG says Design only; a typed
  `… effect … fade 2` works in Operate (plans are refused). Wording or code?
- **QLab out with Blind on:** a real GO while Blind is on still sends.
  Intended?
- **Teaching's question from the web Design/Operate toggle:** goes through
  `App::slotModeOperate` without `m_unattendedOperate`; may ask on the console.
- **Add Fixture says nothing when the engine refuses** a fixture past
  universe 1024 (`FixtureCreator::addFromDialog`).
- Still untested (med): HID trigger feedback per pad type (plugins/hid),
  Fixture Properties' universe box / truss paste Teaching asks, audio-trigger
  log bands, real LTC audio and the timecode footer chip, Snap-to-section on a
  real drop, Why-on entry points in each view, Show Review CLI warning-only
  exit, Companion module JS tests not in the gate.

## Backwash: light on the cyc only on purpose — built, leftovers (0.4, cyc, 2026-11-02)

Built: Show Review **PWR-12** (engine/src/showreviewzonechecks.cpp, sharing
PWR-11's beams), `Scenery::lightOnPurposeOnly()` (on for Cyclorama and Scrim),
`Layer::meantOnScenery` ("I Meant It", one Show Tree undo step), the panel's
I Meant It / Show Me / Not Meant, the Show Tree's "⚠ cyc" cue mark
(ui/src/showtreebackwash.cpp) and the layer right-click. ROI: one check on
geometry PWR-11 already follows; catches the commonest plot mistake a
student (and a tired LD) makes, before tech, at ~10 ms a review on
DanceHysteria. Each "decided (can change)": scrim guarded by default; a
target "on" the cyc is a mark / area / line within 0.5 m (never a person);
meant carries on while the light keeps landing.
Since (Q210, 2026-11-03): three visible states on the cue -- open amber
"⚠ cyc", meant green "cyc ✓" (Layer::meantBy/When/Note, Review group
"Meant"), Not a Problem (the old Accept…) hidden and under Accepted; one
menu (ShowReviewMarker::fillMenu) in the cue badge, Review rows and the chip;
chip tooltip "2 open · 1 meant · 3 not a problem". Only PWR-12 can be meant.
Not a Problem is an acceptance and not a Show Tree undo step (as before);
Meant → Not a Problem takes the mark off as the one undo step.
Since (cyc2, 2026-11-03): curtains stand where each cue leaves them (a
driven drape from the functions the cue plays; at rest = closed in a list
that moves it, else its set state -- decided, can change); one hover detail
and one right-click menu for every warning mark (ui/src/showreviewmarker.*:
cue mark, footer chip, Review rows); only the warning word in the warning
colour; platforms' Finish (rig) colour, stage black by default, drawn by the
rig views, a warned cyc outlined. Leftovers, not built:
- A driven curtain moved by a look (a dimmer palette on the curtain fixture)
  or a chaser step: only scenes and collections a cue plays are read.
- The 2D plot keeps the plot colour; the Finish is not shown there.
- The Lighting Studio drawing an unmeant beam's pool on the cyc in the
  warning colour (needs the cue the Studio is showing; the Studio draws live
  output, not a cue).
- A live hint in the Workbench while building a layer (nothing live exists
  for zones either; the Show Tree mark follows the re-check a moment later).
- The Cue Sheet's notes column.
- "Keep unmeant light off" per cyc: it would clamp output in the runtime
  (per-frame, per-fixture geometry or a baked mask on the GO path) -- not
  built; the cost is a new playback path and hot-path work.
- "I Meant It" on a layer of a reusable cue's use is not synced to the
  reusable.
## One selection — built, leftovers (Q201, xsel, 2026-11-02)

Built: Studio / plot / Fixture Manager picks set the programmer selection;
Workbench, plot and Studio views follow it in front or not; whole groups
recognised (SelectionShape); near-miss chip (plot band outlines the miss);
Selection ▸ (New Layer from Selection, Add to Current Layer) in the plot,
Studio views, Fixture Manager, Fixtures & Groups and Workbench target rows.
Decided (can change): a group row picks its fixtures; only groups of 2+
count; whole groups → a layer each plus one for the rest; deselecting in the
Fixture Manager doesn't clear the programmer; a structure editor rings only
what is mounted on it; the Fixture Manager tree doesn't follow the selection
back; Create Group(s) from Selection stays under FM's Group ▸.
Leftovers:
- Head-level selections: the programmer selection is fixtures, so "a group's
  heads" is recognised only as its fixtures.
- The Studio views have no rubber band, so the "just outside the box" case
  is the plot's only; the Studio and FM chips use the count rules.
- The Look Editor has no fixture list of its own: it follows through the
  Workbench; ctx (context-menu audit) can slot SelectionMenu into any
  canonical menu still missing it.
- Offscreen tests emit the Studio's pick signal rather than a synthetic
  mouse click on a fixture.
## Context-menu cohesion: built, leftovers (ctx, 2026-11-02)

[CONTEXT_MENUS.md](../design/CONTEXT_MENUS.md) holds the inventory and the
canon (eight sections, the rules for words, one builder per kind).
`ui/src/contextmenus.*` and `ui/test/contextmenus` are the code and its
check.

Fixture groups and fixtures are on the shared builder in the Fixtures tab,
Build ▸ Groups & fixtures, Build scenes, Plot Layers and the Library.
Palette and effect menus in Build and the Library are migrated, and so are
Build ▸ Scenes, the Functions tab and the empty plot. Stage pieces and
no-light zones are renamed, and the dead item menus are deleted.

2026-11-03 (ctx2, the user: "context is a top line, then consistent
options below"): the canon now puts **this place's own items on top**, then
the kind's own menu in a fixed order, the same wherever the thing appears.
One builder per kind: `ContextMenus::rig` (fixture group, fixture — now
with LED Layout…, Gobo Wheels…, Split by Head Type…, Rename…),
`ContextMenus::palette` (Build ▸ Palettes and Effects, the Library, a cue's
look row and the Workbench's, Build scene look rows, the Look Editor's
slots), `ContextMenus::function` (Build ▸ Scenes, Effects, the Library, the
Workbench's layer rows), `ShowTreeView::fillLayerMenu` (tree and docked
Workbench), the tree's cue / hit / list / production menus, the plot's item
menu (fixtures via `rig`, structures and targets with Used By ▸), Structure
Studio and the riser-face editor.

Left:
- [ ] **Virtual Console**: its right-click menu is the menu bar's Edit menu,
  so Delete is fourth and "Widget Properties" has no "…". Give it its own
  menu (§3.4).
- [x] **Connections, power widget, show timeline**: title case,
  Delete last ("Delete universe entirely…" is followed by Add Universe),
  one word for remove ("Unassign", "Forget"). Done 2026-11-04 (wfaudit2):
  every menu label in ui/src is Title Case, checked by
  `ui/test/menuwords` (it reads the sources, so a new menu anywhere is
  held to the canon's words); Connections puts Collapse / Expand All Below
  and Add Universe on top and the row's Delete last ("Delete Universe…",
  "Delete Target…", "Delete Port", "Unpatch…" for "Forget this patch").
  Left: the power widget's "Unassign" (a value, not a destroy: kept).
- [ ] **Line-param rows** (Spread / Grouping / Cross ⋯) and the Look
  Editor's pick menus: no kind block yet (a value, not an object), §3.4.
- [ ] **Functions tab**: QLC+'s own window, hidden by default (Q32); on the
  canon's words but not the function builder.

## Course: Level 3's remaining lessons (0.5 item 9, crs3, 2026-11-01)

Written and checked by `courseshow_test` (CHANGELOG 0.4.0 Teaching):
Level 2 was already complete (2.1-2.7 and the check); it gained
extensions for "Why is this on?" (2.1), Solo (2.3), effect fades (2.4) and
Formation palettes with the line plan in a window (2.5). Level 3 gained 3.5
(show control and timecode: the one GO, pad Run, the phone Run page), 3.8
(sections and Block, group master, override hit, cue sheet and tracking
sheet) and 3.9 (Show Review, "Why is this on?", Solo; the Level 3 check).
Still outlines in COURSE_PLAN.md: 3.1 DMX, universes and patching; 3.2
fixtures and definitions; 3.4 building the rig. Each needs the real network
or the Lighting Studio, so they are the next batch, with Branson on the rig.
Decided (can change): the Level 3 check is on the real rig with Branson, so
it has a page and a rubric but no course show of its own; 3.9's start file
carries its faults (a song that doesn't Block, a hit no button fires, a
group master at 0) rather than students building a broken show.
Candidates, not in the plan: a lesson on the pad's walk-the-show with a
real controller (needs the rig and a pad; it is a demo in 3.8's teacher
guide); a short lesson on Running order for a show whose songs change
night to night (an extension in 3.8); the preshow as section 0 of the
show (3.8 Part 3). No pictures yet for 3.5, 3.8, 3.9.
## Sub-cue ↔ inline cue — built, leftovers (0.4, subx, 2026-11-02)

Built: `ShowTreeController::makeInlineCues()` / `makeSubCues()`
(engine/src/showtreesubcueconvert.cpp), the tree's right-click (Make
Inline Cue, Make Sub-cue of <the cue before>), `inline` and `sub of [n]`
on the command line, help in show-tree.md, tests in showtreeplay_test
(rig output per press, round trips) and showtreeview_test. Each edit is
checked by resolving the production before and after (stage and
cue-wide spans per cue) and carries looks where the rules differ, else
refuses. Decided (can change): Make Inline Cue on 4.2 brings 4.3… along
(a sub-cue plays only after the ones before it); a cue's own sub-cues
come along as following sub-cues (the first auto-following at once);
converted sub-cues are Cue-only and include the one before when the
list's own rule wouldn't play them the same; a cue with no sub-cues gets
a stand-in first sub-cue for its own look (removed again by Make Inline
Cue when it is all that is left); the first sub-cue's "plays with GO"
is the cue's hold 0. Leftovers: the menu offers only the cue before
(`sub of 4` for another); a carried look can't keep a cue-wide effect
whole (refused); looks carried into the next cue stay there after a
round trip (the stage is the same).

## Encoder banks — built, leftovers (0.4, enc, 2026-11-01)

Built: `EncoderBank` (engine/src/encoderbank.*, owned by the Control Map and
saved in its element), `EncoderAttributes` (what a slot is on a fixture
selection), the Look Editor's `encoderAdjust/Home/Value` (lookeditorencoders.cpp),
the Workbench glue (workbenchencoders.cpp, the line under the title) and the
Control Map panel's Encoder bank section. Each "decided (can change)":
presets are three generic encodings plus the X-Touch Mini (standard mode,
layer A, Relative 1), the Midi Fighter Twister (3FH/41H, Omni patch) and the
APC40 knobs (absolute, read as a change); the numbers are the makers'
documented layouts and none was checked on a unit here; push held = fine,
tap = home; in Operate the encoders refuse (banks still step); a channel a
look of the scene drives is left alone and the line says so; the Workbench
must be showing. Open:
- Check the three presets on the real controllers (CC/note numbers, ring
  and lamp lines, relative mode names), and add the ones Branson owns.
- ~~Direct bank buttons: a "Learn" for the bank button and for the
  pushes/rings.~~ Done (enc2): Control Map ▸ Encoder bank ▸ Learn more…
  (pushes, next / previous, bank 1 button with its lamps, rings = the
  control turned, Forget). Decided: the ring base is learned from the
  encoder turned (the same number); a controller with rings elsewhere
  needs its preset.
- ~~Show Tree layer fallback untested.~~ Done (enc2), and it was wrong:
  a cue layer's scene is baked (shared by signature, never saved), so a
  turn there was lost at the next bake and merged into no step. The layer
  row now refuses and says "made from its looks"; an opened look row takes
  the turn (Look Editor path, one undo step). Tests: EncoderBankUi_Test.
  A Build stack of real scenes (collection / chaser) still turns the
  focused layer's scene; no test for that path yet.
- Colour: hue / saturation are for RGB and CMY heads only (not a colour
  wheel); Position steps whole degrees in a Pan / tilt look (the palette
  stores integers); no encoder for effects' parameters.
  ~~Shutter / Strobe looks, Aim looks.~~ Done (enc2): Beam bank slots 5-6
  (Strobe rate; Shutter state, range to range) on Strobe and Shutter
  looks, Position's first encoder is an Aim-at-a-line look's Spread (a
  param step, merged per gesture; decided: there is no target offset in
  the look, the stage target is shared). Strobe / Shutter on a fixture
  selection (a scene's raw channels) are not mapped yet: the line says
  "the selection has no strobe".
- RGB-only fixtures (no dimmer) take Intensity as the strongest colour.
- Hue wraps; everything else clamps. No acceleration beyond the speed the
  controller sends (a fast spin is several detents in one message).

## Line aim visuals: groups in colour, crossings as lines — built, leftovers (tvis, 2026-11-03)

Built: `LineFormation` (ui/src/lineformation.*), one model behind the order
strip and the plan view, from the engine's own rule (orderedLineHeads +
`LineSpread::fractions` / new `LineSpread::groupOf` + `AimSolver::linePoints`).
Groups wear a colour each (7 dataviz hues, validated adjacent + wrap on the
plan surface) and a bracket on the strip; the strip has hang-order squares
with connectors to the points, the plan colours beams; a caption says what
crosses (and that Cross is idle at Spread >= 0); plan names never overlap
(row stacking, leaders). Each "decided (can change)": **the Formation
preview keeps its cue-derived default pick, labelled** ("Showing AT-W-ALL
(aimed at Line 1 in cue 1.1.1 Verse).") with a **Show none** link, rather than
showing nothing until picked; with more than seven groups the colours go
round again (brackets and numbers still tell neighbours apart). Also
built (user ask on 138.png): stored positions (PanTilt, any non-Aim
Position) solved forward from DMX -- `engine/src/beamlanding.*`
(BeamLanding::land / describe / summary) -- shown as a line + plan under the layer's Position row and
on the Look Editor's PanTilt page; the direction is the aim agent's
`AimSolver::headDirection()` readback, so it matches the rig views. Fixed:
the plan view drew (x, height), not (x, y). Open: naming scenery a beam lands
on (only targets and stage areas now); landing on a riser's side.

## Formation palettes — built, leftovers (0.4, form, Q197, 2026-11-01)

Built: `QLCPalette::Formation` (Spread / Grouping / Cross, any subset, as
the `__line*` params), `ShowTree::KeyFormation` resolved per row (layer row
> layer Formation > cue row > cue Formation > Aim's own), Build's
Formation page with its never-saved preview (`FormationEditor`), the
command words and AI Assist's `formation` attribute. Each "decided (can
change)": the preview pick is remembered per palette for the session only
(cleared when another show opens); a Formation on a non-line layer stays
and says it does nothing (not refused); the preview uses the Aim palette a
line aim there reuses as the base. The plan view's pop-out (lpv) works from
the Formation page too, fed by its preview. Open: nothing known.

## Line aim, mixed groups (ust, 2026-11-01) — fixed, leftovers

Branson's "USTs point off stage right" in *T-Spots + USTs*: the resolve was
right (all 16 lights on Line 1 in hang order, the USTs at subject height
above their points, DMX pan 61-67 of the 0-127 band); the views read an
indexed pan band (`RotationIndexed`) over the whole channel and drew the
USTs a quarter turn off. Fixed with `Fixture::positionBand()` /
`positionFraction()` in the rig view, the plot and the readout (CHANGELOG
0.4.0 Fixed). `MoveSpeed`'s travel time and follower now measure a banded head along
its band too (lft2, 2026-11-01), and Show Review's no-light-zone check
(PWR-11) reads a banded head's pan and tilt across it (zpan, 2026-11-02).
Open: nothing known.

## Line aim pop-out (lpv, 2026-11-01) — built, leftovers

Built: `LinePlanWindow` (ui/src/lineplanview.*) shows the order strip and
plan scaled to a resizable, non-modal window; `ShowTreeView::updateInspector`
feeds it on every rebuild of the layer page (no second model, nothing per
frame), dims it when the selection has no line aim, geometry in QSettings
`showtree/linePlanWindow/geometry`. Entry: the ⤢ button on the plan and a
right-click on either view; no menu item. Decided (can change): the window
is a child of the Show Tree view (closes with it); light names show once the
plan is drawn at 1.6x or more. Open: the strip does not name the lights.

## Play / Solo on the Show Tree pages — leftovers (solo, 2026-11-01)

Built (CHANGELOG 0.4.0 Fixed; DONE.md 2026-11-01): Play / Stop and Solo on
the cue and layer pages, greyed in Operate there and in the docked
Workbench. Open:
- The docked Workbench's Solo plays the layer's own scene
  (`TreeCueStack::playSolo`); the pages' Solo is the controller's
  (`setPreviewSolo`, the pad walk's), which takes the layer's child layers
  too. Decided (can change): left as they are; make the Workbench use the
  controller's if the difference shows.
- Decided (can change): Stop holds from layer to layer of a cue; picking
  another cue plays it (the latest action wins). No keyboard shortcut (none
  existed; the shortcut count stays flat).

## Raspberry Pi package — leftovers (pi, 2026-10-31)

Built: `platforms/linux/make-pi-deb.sh` (a .deb; decided, can change),
docs/dev/RASPI.md. Open: build it in CI (an arm64 runner) and attach it to
a release; a Pi 4 build host; a Pi image with the package preinstalled;
`debian/` and the `.spec` are still upstream's stale copies (unused).

## "Why is this on?" v1–v3 — built, leftovers (0.5, why + why2, 2026-11-01)

Built: `WhyOn` (engine/src/whyon.*) answers "why is this light / channel
where it is" from the last frame, on demand, on the UI thread: it copies
the universe's fader list, the one FadeChannel per fader that covers the
channel (`GenericFader::peekAddress`) and the channel's stage values
(`Universe::channelSnapshot`, under the frame lock), replays the merge in
fader priority order and names each source; `WhyOnPanel` (ui/src/
whyonpanel.*) shows it from a right-click in the plot, the Rig Overview,
the Fixture Manager and a Simple Desk channel (docs/features/
troubleshooting.md "Why is this light on?"; course: Lesson 2.1). Nothing
is recorded per frame; the window re-asks at 4 Hz only while shown.
Each "decided (can change)":
- The lines are the engine's stack top-down (the last-written, highest
  layer first), then the stages after the faders in the order the
  universe applies them (group masters, Grand Master, modifier,
  passthrough, Blind,
  Blackout, Teaching's hold). The winner is bold; the headline repeats it.
- A fixture with no channel picked is explained by its master dimmer
  (else its first intensity channel); the window's box picks another.
- The Simple Desk, the programmer's live edits and VC sliders name their
  faders when they make them (`setName`, once, not per frame); a VC
  slider's is its widget id, named by the UI. Other unnamed faders read
  "a source with no name".
- An effect from the effect runner is named by the effect palettes
  playing on that fixture's running scenes; its depth is the fader's Mix.
- A Show Tree scene several cues share is named for the cue running (or
  previewed) now, with the attribute's own tracking origin.
- Wording::Simple (the **Plain words** box, on by default in a Teaching
  show) only drops the merge words today; v3's full student sentences
  build on it.
- Jumps: cue / hit / preview → the Show Tree node; function → reveal (a
  generated one) or open it; Simple Desk, VC, Park / Highlight / Flash /
  Focus → their tab; a group master → the Fixture Manager.
v2 and v3 built (why2, 2026-11-01), each "decided (can change)":
- Who left it there: no per-frame probe, no always-on per-write record.
  Each fader gets a token (Universe::requestFader); the universe notes the
  token and frame per channel when a fader lets go of it (an auto-removed
  channel at its target, a fader dismissed or deleted with channels) and
  keeps who the last 32 faders to leave were (name, function, priority:
  a shared QString, no allocation). Nothing per frame; hotpath_test
  ::whoLetGoAddsNoAllocationPerFrame. A channel written since (a direct
  write) is "kept the last value" again. Not caught: a Show Tree partial
  release at once (GenericFader::release with no fade drops the channel
  with no universe at hand).
- Mark (move in black) is an OutputLayer "Mark" at Universe::Hold, under
  everything, intensity never held (as before: a direct write was zeroed
  by the next frame); its DMXSource only watches for the hand-off.
- Why not: computed when an intensity or shutter channel is asked about
  and the fixture is dark (dimmer ≤ 2 %, a shutter at a Close / "closed"
  capability, every colour emitter ≤ 2 %). Order: no output, not ours,
  Blind, Teaching hold, Blackout, Grand Master 0, group master 0, power
  limiter, what holds the dimmer (Park / released or missing layer in
  the previewed or running cue / nothing / a source at 0), shutter,
  colour. The headline gives the first reason that isn't "no output".
- Actions from a line, Design only: Unpark, Release (a Simple Desk
  channel), Let go of the move in black. ⌘I (free; ⇧⌘I is the VC's) is a
  shortcut on the main window, no menu entry: the light last selected in
  the plot, Rig Overview, Fixture Manager or Workbench.
v3b built (why3, 2026-11-01), each "decided (can change)":
- Why not now also says a **skipped cue** (the nearest skipped cue with a
  layer on the light, behind the running one: GO tracked past it), the
  preview's **Solo** (`ShowTreeController::previewSoloLayer`: the Show
  Tree's and the pad walk's; the Workbench's own Solo plays a plain scene
  and reads as that function) and an **Operate blind edit** (preview state
  NotDesign, the cue open for edit has a layer on the light). They sit
  where "what holds the dimmer" is, Solo first, then the blind edit, then
  released / skipped. A preview is only blamed in Design now (it was named
  in Operate too, wrongly).
- **Stop the function** on a Function line, Design only (`stop` with the
  master as parent: every start of it ends). A cue list's own line is not
  offered it.
- **Follow selection**: a box in the window, off, kept in QSettings
  `whyon/followSelection`; only a shown window moves.
- Locate: it is a flash with no window of its own (Fixture Manager
  toolbar, Monitor), so there is nowhere for a link; its right-click
  already has "Why is this on?". Left.
v3 student mode built (why4, 2026-11-01), each "decided (can change)":
- `WhyOn::explainGroup` / `explainFixtures` (engine/src/whyon.*): two or
  three sentences per **fixture group**, built from each fixture's
  intensity answer (`explain()` with no channel), so no new replay and
  nothing per frame. Fixtures with the same source and state are told
  together; a mixed group leads with the biggest bunch ("Most of the X (4
  of 6 fixtures) ...") and adds one sentence on the rest. A layer is told
  through its fixtures (`explainFixtures`); the window only offers groups.
- Shown by the **Plain words** box (on in Teaching, offered elsewhere), no
  new box or menu entry: a block above the chain for the (up to two)
  groups the light is in, a light in no group on its own; recomputed once a
  second while shown, and put on the clipboard with Copy.
- A sentence names the level before the masters; a second one names the
  group masters and the Grand Master when they scale it. English only
  (`tr()`), like the rest of the window.
Open: pin / several windows, release-at-once provenance, a group name that
is plural by more than a trailing "s" ("Wash" is read as singular).

## Effect fades — leftovers (efo, 2026-10-31)

Built (STRATEGY §4 0.4 item 6; docs/features/show-tree.md "Effects"): a
Show Tree effect's depth comes up over its cue's fade in and goes over
the releasing cue's fade out (`Look::fadeIn/fadeOut`, baked onto the
generated scene as `Scene::setPaletteFade`, `FadeFollow` for the cue's;
`EffectInstance::fadeDepthIn`, `FadeChannel::Mix`). Open, each "decided
(can change)":
- Default: a new effect follows the cue's fades; an effect saved before
  (no FadeIn/FadeOut) or converted from a scene is **At once**, as it
  played.
- Depth is how far its writes go from what is under them (this frame's
  universe value) to its own; a dimmer's swing also scales with depth.
  A release part way in fades out from there over the whole fade out.
- The fade out followed is the one the stopping scene was started with
  (the base look's, the same number), not the next cue's own time.
- EFX and RGB Matrix layers are not changed: QLC+'s own handling (an EFX
  grows over the fade in and stops at once; a matrix fades in and out).
  Making an EFX shrink out on its cue's fade out would change QLC+ 4
  shows; left until asked.
- Classic scenes: an effect look's per-look fade in (Look Editor) now
  starts its depth fade too (it was ignored for effects); a per-look fade
  out alone plays as before.
- ~~No command-line words for effect fades yet~~ built (feat/05-small-items):
  `Steps effect Sinelon fade 2 | in 2 out 0.5 | cue | atonce`, the
  `set_effect_fade` operation (AI plans carry it). `effect X fade …` alone
  keeps the effect's own looks (decided: can change).

## Pad Run — MVP built, leftovers (0.4 item 2, pad, 2026-10-31)

Built: the controller's **Run** role (`PadRun`, engine/src/padrun.*; the
Run tab's **Pad: Tools / Run / ARMED** button; docs/features/show-control.md
"GO from the game controller"). Every command goes through
`ShowControl::run(r, RoutePad)`. Open:
- ~~A Hits page on the pad~~ and ~~a pad chord to switch roles~~: built
  (feat/05-pad-hits): LB + RB fires the armed hit (narrowest place wins,
  through `armedTier()`, started like a VC button, Operate only, name
  spoken); hold View + Menu 1 s switches Tools / Run (a short press in
  Tools is Focus mode, on release; a third button cancels). decided: RB is
  the hit button, several at one place take turns (can change). Open: the
  focus confirm (the rest of "the full role"); not verified on a real pad.
- Q192's walk is built (MVP, below): Menu in the Run role.
- Not verified on hardware: the DualSense light bar / rumble on GO with a
  real pad (the code sends the same feedback the focus tools do).
- decided (can change): GO needs LB held (a modifier, not a double-press);
  Back, Sub GO and Stop need it too; RT / LT and RS need no arm. Tools is
  the default role, kept per computer (QSettings controller/role). Any
  patched pad in the Run role can GO; the arm is per pad. No voice on a GO
  that went (the house hears the laptop): a buzz and the light bar; a GO
  with nothing to play buzzes long and is said, as the Feedback setting
  allows.

## Stream Deck / Companion module — built (sd, 2026-11-01)

Built: `tools/companion-module/` (`@companion-module/base` 2.2, node tests with
a mock server) over the Run socket (webaccess/src/webaccessrun.*; docs/features/show-control.md
"Stream Deck (Companion)"). Open:
- Not published (npm, Companion's module store, a PR to Bitfocus): Branson's call.
  See the ready file for what it takes.
- Hands-on on a Stream Deck + (dials, touch strip level bar): scratchpad checklist.
- A selected layer's dimmer on a dial (Design) is not built: web access has no selection.
- Web access with logins (-wa) is not supported by the module (PIN only).
- Hit and master "numbers" are positions in the show's list; a renamed or reordered list moves them.

## One GO path — built, leftovers (0.4 item 1, go1, 2026-10-31)

Built: every GO, Back, Sub GO, Stop, Load... enters through
`ShowControl::run(request, route)` and is guarded in one place,
`App::runCommand()` (docs/design/RUN_COMMANDS.md; DONE.md 2026-10-31).
Pad Run and the phone Run page call `run()` with their route (override
hits, built, are fired by a VC button's start, not a run command; ALL OFF,
which lets go of them, is one). Open:
- Phone Run page: built 2026-10-31 (feat/04-phone-run): `RouteWeb`, `/run`
  on web access, "Allow GO from the phone Run page" in Show Settings (off by
  default, the show's PIN once per session). ~~a QR code for the
  address~~ (built, feat/05-small-items: ui/src/qrencoder.*, beside "Open:" on
  the Show control page, the first network address's /run; shown only when
  reachable from the network); ~~Hold (Pause/Resume) buttons~~
  and ~~a slider for each group master~~ (built, feat/05-run-leftovers);
  following a list other than the main one (not decided; left open).
- DONE (vcgo, feat/04-vc-go-path): the VC Cue List (and the web page that
  drives it, which calls the same slots) and a VC button's next / previous
  step run a Show Tree main list through `run(request, RouteVcCueList)`;
  a plain QLC+ chaser steps directly as before. Decided (can change): the
  web VC page is not the phone Run page, so the remote-GO rule does not
  apply to it; a routed GO plays at full level with the list's own fades
  (the widget's submaster and crossfade apply to plain chasers only).
  Still stock: pause / resume, a row's sub-cue double-click.
- decided (can change): GO is never refused by Operate's PIN or Teaching
  (they lock Design); a GO that finds nothing to play holds no preview.

## Calling script view — built, leftovers (0.5 item 6, csv, 2026-11-01)

Built (`engine/src/callingscript.*`, `engine/src/showtreescript.cpp`,
`ui/src/callingscriptview.*`, More ▾ → Calling Script… on the Show Tree
tab; help: show-tree.md "The calling script"; course note: Lesson 1.5). Open,
each "decided (can change)":
- Decided (can change): it is a window of its own opened from the Show
  Tree's More menu, not a tab or a Run tab pane (no new top-level tab); the
  menu gains one item (Cue Sheet… and Calling Script… sit together).
  Folded back (fold, Q200): Calling Script… is a button in the Cue Sheet…
  dialog, so More has its four items again.
- Decided (can change): the script and its anchors are part of the
  production (`<CallingScript>` in the Show Tree XML, an older build skips
  it). A cue is anchored by id, so renumbering and moving keep it; deleting
  the cue removes the marker (undo brings it back); a deleted cue's anchor
  is not saved. Text edits move anchors by a prefix/suffix diff; a cue on
  replaced words goes to the start of the change.
- Decided (can change): GO in the view is the Run tab's route
  (`RouteRunTab`), no route of its own; it GOes the production's main list.
- Decided (can change): headings are Markdown `#` lines and lines equal to
  a song section's title or name (Q190); no other detection.
- Not done: one script per production only (no per-list scripts); editing
  keeps no markers in the margin until Done; no drag of an existing marker
  to move it (re-drop or Cue here moves it); a sub-cue is placeable but
  GO there is the cue's, not Sub GO; no per-page print layout (the PDF is
  the cue sheet's landscape Letter); a Show Review check ("cue with no line
  in the script"); web (phone) display of the script; .docx / .pdf import.

## Cue sheet and tracking sheet — leftovers (sheet, 2026-10-31)

Built (`engine/src/cuesheet.*`, `ui/src/cuesheetdialog.*`, More ▾ → Cue
Sheet… on the Show Tree tab; help: show-tree.md). Open, each "decided
(can change)":
- Decided (can change): the entry is the Show Tree's More ▾ menu, so the
  menu-item count is flat; no File menu entry.
- Decided (can change): the cue sheet follows tonight's running order; the
  tracking sheet is always in number order (tracking resolves in list
  order). A tracking cell lists the layer's values, a tracked one in
  brackets (grey in the PDF); a layer is a row, not each fixture.
- Decided (can change): one production at a time, every playable cue list
  of it; sub-cues appear on the cue sheet only; the hits "armed there" are
  those under the cue, the list / production / global ones sit once under
  the list's name.
- Not done: a row per fixture (fixtures x cues) with the fixture's own
  values; a column for a cue's mode (Block / Track); printing straight to a
  printer (the PDF prints); a Show Review check of notes and timing.

## Override hits — built, leftovers (0.4 item 5, ovr, 2026-10-31)

Built (docs/features/show-tree.md "Override hits"; DONE.md 2026-10-31): a
hit's **Override** wins over everything the Show Tree plays on its
channels, intensity too, and fades back over its fade out to what plays
then. Engine: `Universe::HitOverride` (between Override and Flashing),
`GenericFader::startOverrideMix`/`releaseOverrideMix`, `Universe::writeMix`;
`Collection::setHitOverride` raises its members, `Scene` mixes. Open, each
"decided (can change)":
- ~~Next: group intensity masters~~ -- built (gm, below).
- Firing a hit stays a VC button's (or a step's) start, not a run command;
  ALL OFF (a run command) and Stop all let go of overrides.
- The Blind view draws a live override hit over the cue in edit; a hit
  not fired isn't drawn.
- A live override also beats the Design preview (Universe::Override);
  Flashing, Simple Desk, Highlight, Flash, Focus and Park still beat it.
- An override hit fades as a whole over its own fade in / out (per-look
  fade times don't apply to it); its effects play as effects do (Override
  priority, not raised); an opaque function in it (adopted chaser, EFX)
  plays at HitOverride priority but mixes HTP/LTP as before.
- An override hit's scenes are never shared with a cue (the bake keys
  them with an override mark), so a cue never inherits its priority.
- A 16-bit channel is mixed byte by byte during a fade (exact at either
  end). An LTP channel nothing else plays keeps the hit's value after
  release (QLC+'s LTP rule).
- ~~No command-line word yet~~ -- built: `override [on|off]` on the selected
  hit (Operation Registry; one undo step; Design only).

## Group masters — built, leftovers (0.4 item 5, gm, 2026-10-31)

Built (docs/features/show-tree.md "Group masters"; `GroupMasters`,
engine/src/groupmasters.*; `FixtureGroup::setMaster/setMasterLevel`;
`Universe::setGroupMasters`; the Run tab's `GroupMasterStrip`; the VC
slider's Group master mode). Open, each "decided (can change)":
- Where it applies: in the universe's post-fader stage, multiplied just
  before the Grand Master (`Universe::updatePostGMValue`, one byte test per
  written channel, no allocation), not at `writeMix`'s place among the
  faders: scaling the pre-GM values would change what an override hit or
  an effect mixes from, and the post-fader stage is worked out again on
  every path (a frame, a master or GM move). So it scales everything the
  Grand Master does: cues, effects, VC, Simple Desk, override hits,
  Highlight, a flash, Focus. Parked, Ignore-GM and relay channels are not
  scaled.
- Intensity only (the universe's Intensity channels: dimmers and an LED
  fixture's colour mix). A group of some heads of a fixture scales those
  heads; its non-head channels (a master dimmer) only when the group holds
  the whole fixture.
- Overlap: a channel in two mastered groups is scaled by both, multiplied
  (not the lowest).
- Saved: `<Master Level="N"/>` in the group (only when it has one); the
  level is kept, so a show reopens as it was left. The stock QLC+ save
  check names both it and a VC slider in Group master mode (which QLC+ 4
  loads as a submaster).
- Moving a master is playing, not editing: it does not mark the show
  modified (turning a master on or off does).
- Not on the Control Map (presses only) or the pad (Run is GO/Back/Stop):
  a controller fader reaches a group master through a VC slider's
  External Input.
- ~~A command-line word~~ (built: `master Front 50`), ~~a master on the
  phone Run page~~ (built, behind remote GO and the PIN) and ~~masters in
  the Blind view~~ (built: the picture is held back by them; the Grand
  Master stays out, decided, can change). Not done: the "why is this on" answer (v2 provenance should say "held at 50 % by the
  Front master").

## Breathe once — leftovers (brth, 2026-10-30)

Built (build 2593 report: flash at start, end lands anywhere, no typed
values, rate awkward for one shot). Open, each "decided (can change)":
- A wave (script `cycleParam`) on Once / N times runs whole cycles; its
  Length is one cycle, stored as the rate (1 / length). The run Length
  (`__durationMs`, auto = the cue's time) no longer applies to a wave.
  FINAL (Branson, 2026-10-30): Length = one breath.
- Toggle button on a hit with a Once / N-times effect (tgl): once every
  one-shot has finished, a press restarts them (the hit stays on) instead
  of turning it off; while one still runs, a press turns the hit off.
  Decided (can change): a hit whose breath has ended is turned off by
  pressing during the next breath, or with Stop all / the hit's own fade
  out; there is no hold or double-press gesture. Flash is unchanged.
- A scene with an effect holds its output until the effect's first frame,
  at most 250 ms (the Doc's own timer only).
- Dimmer Phaser sends the dimmer alone; the host scales the look's colour
  (white too) on a light with no dimmer.
- A Once / N times run starts afresh on every new start of a scene
  carrying it (a cue's GO, a hit fired again, a chaser step): the user's
  rule. A cue that only tracks the effect through leaves it running or
  done (decided, can change). A tracked effect on a layer whose scene
  changes in that cue (something else on it set) does start again.

## Scoped hits (Q154 / Q157, sch, 2026-10-30)

Built (docs/features/show-tree.md "Hits"): a hit lives under a
cue, a cue list, its production or at the top; armed while its place is
live; a VC button holds several and fires the narrowest armed one; drag
hits between places and onto VC buttons. Open:
- Phase 2 (Q157) built: the map and Hits table, the same-place and
  override questions, ↳ overrides in the tree, on the face and in the Run
  tab, Show checks. Decided (can change): the map shows in the button's
  properties (no pop-up on OK); Step through / Together is one setting per
  button; the tree keeps "VC ×N" with the button names on hover; the Run
  tab's line is in the Run tab only (not the VC Cue Runner sizes); Show
  checks warn only for deleted hits and unanswered shared places.
- Decided (can change): a production's hits are armed while it is current
  or none is (a show with several productions: another production's hits
  are disarmed, a change from before); a list's hits while it runs, or —
  nothing of its production running — while it is the current
  production's main list; a hit for everywhere is kept in a production's
  Hits section (the one it was made or moved from); only a production's
  own hits have folders; a deleted cue or list moves its hits up to the
  production; among equally narrow armed hits a button fires the first
  added; a button with several hits shows the armed one's name instead of
  its caption; drops onto a VC button work in Design and Operate and have
  no undo (the VC has none).
- The command line's `hit` makes production hits only; no `hit … for cue`.

## Controller slots — leftovers (ctl, 2026-10-30)

Built (CHANGELOG 0.3.1 "One input, whichever controller is here";
docs/features/other-improvements.md). Open, each "decided (can change)":
- The show's channels are the first device's (read off it when the slot is
  made with it connected, else the standard gamepad order) and saved with
  the slot; reordering does not change them.
- "Any game controller" takes the first free pad by the plugin's kind or a
  profile with gamepad roles; it keeps the one it has while it stays.
- Patching the universe by hand to a device not in its list replaces the
  list; undo of a patch does not restore a slot.
- The offer names one pad at a time (the notice line holds one); a pad is
  offered once per universe while it stays.
- Not verified on hardware yet: the Linux joystick layout for xpad and
  hid-playstation (codes from the drivers; SDL's mappings agree) and the
  real DualSense checklist (report of ctl).

## Pixel boxes, effects and beams — leftovers (cbug, 2026-10-30)

Built (DONE.md 2026-10-30). Open, each "decided (can change)":
- A Dimmer alone leaves a box with no dimmer dark (white pixels too): the
  colour is the light, as on its RGB pixels.
- One effect over several layers takes its look colours (palettes.look)
  and its timing from the first layer's scene; each layer keeps its own
  base level.
- A standing head that can't reach a line point below it aims at the
  subject height above the point; a hung head aims at the point as before.
- Course: Lessons 2.4/2.5 can now teach the cue-wide effect, the chase
  across the stage and the USTs on the Riser Line (their workarounds are
  the course agent's to lift).

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
  when it is the show's). ~~It is not an undo step yet.~~ Done (lyr): it is
  one step (Runs, At the end, Length).

## Show Tree tidy — leftovers (2026-10-29, DONE.md)

- ~~The command line's `release` takes groups only; a ▣ (function) layer
  is released from its page or the right-click menus.~~ Done (lyr):
  `release <function name>` toggles it.
- Layers move (lyr, 2026-10-30): ⌘X / ⌘C / ⌘V and drag, ⌥ copies. Open,
  each "decided (can change)": cutting cues isn't built (drag them, or
  copy); a layer dropped on a row lands before it in its upper half, after
  it in its lower; a child layer moves only with its parent (moving it
  alone to another cue reparents it when that cue has the parent's target);
  in the docked Workbench a layer row drags to reorder only (to another cue:
  the tree's drag, or Cut and Paste).

## Live and blind — leftovers (docs/design/LIVE_BLIND.md, 2026-10-29)

Built (DONE.md 2026-10-29). Open, each "decided (can change)" in the note:
- Build's Workbench in Operate still plays what it loads live; make it
  blind too if "only the run is live" should cover Build.
- ~~The Blind view draws levels, not effects' motion.~~ Done 2026-10-30:
  its effects move (`engine/src/blindeffects.*`, the stage's EffectInstance
  off the stage on the view's clock).
- Reordering looks *within* one Show Tree layer (they play in kind order;
  a layer holds one effect per kind, so order changes nothing on stage
  today). Would need an order on Layer, saved, resolved and undone.

## Timecode (0.4) — built, leftovers

Built: LTC in (`ltc_test`) and cues at a timecode (`timecodecues_test`), DONE.md 2026-11-01 "Moved from TODO.md". Decided (each can change):
- decided: the first channel of the audio input chosen in the audio settings, no separate LTC device (can change)
- decided: a jump forward fires only the latest cue passed, a jump back re-arms (a step under 250 ms is jitter), a dropout holds, the first position fires only a cue exactly there, cues with no time are GO-only, times are stored in ms and shown at the source's frame rate (can change)

Built (Q198 1–4, q198, 2026-11-01): song sections on the timecode — a
section's start with cue offsets and anchor cues, Section times… for an
older show, Import Logic markers… (MIDI marker meta events, matched by name,
previewed, one undo step), Ripple timecode… / `ripple` / `stretch`, Learn
section starts (`timecodeanchors_test`, songsections_test, hotpath_test).
decided: the marker import adds the file's SMPTE offset, else one hour when
the show's starts are past 01:00:00:00 and the markers aren't (can change);
decided: Later sections follow also moves later cues with a time of their
own; a ripple at a section's first cue moves that section's start (can
change); decided: a section's first start (markers, Learn) keeps its cues in
place and makes them count from it (can change).
**Later (Q198 5):** timing maps per session version (festival cut vs full),
switchable like venues.

**Open:** LTC read backwards, a channel picker, an input level meter beside
the readout, and a time per frame rate (a cue stamped at 25 fps reads the
same milliseconds at 30).
(Left open on purpose in feat/05-run-leftovers: a time per frame rate
changes how a cue's time is stored and read back, which is not small.)

## Timeline waveform + markers — built, leftovers (0.4, tlw, 2026-11-01)

Built: DONE.md 2026-11-01 (`waveformpeaks_test`, `ShowManager_Test`), docs/features/shows.md.
Markers, their undo and the timeline were already in the Shows timeline; this
added the cached background waveform, the DAW marker import and the marker snap.
- decided (can change): the waveform is on the Shows timeline's audio clip and
  in its markers lane; the Show Tree's cue list has no linked audio track and no
  time view, so no waveform there.
- decided (can change): peaks are cached in the user cache, not beside the show.
- Marker import and section anchors moved to the Q198 timing model (q198); snap to a beat and snap while resizing are open.

## Lighting Studio alignment — leftovers (0.3.1 d2, 2026-10-29)

The audit's rows are fixed (DONE.md 2026-10-29, ui/test/plotalignment);
the whole-cell grid and the readout depth bias are fixed too (0.3.1 d3).
Still open from it:
- decided (can change): a structure editor's ruler still counts from the
  structure's middle; only the Rig Overview's counts from CL / ⌖, and the
  H / V / Z readout counts from CL / ⌖ everywhere.
- decided (can change): a lantern's glow at 45° doesn't pick it; its glass
  and cage do. A click on a ruler band picks nothing under it.

## Walk the show from the stage with the controller — MVP built, leftovers (board Q192, 0.4, walk, 2026-10-31)

Built: see DONE.md 2026-10-31 (Walk the show, MVP) and docs/features/show-control.md. Focus calls and Refine (built 2026-10-24, design note moved to DONE.md): Branson's "Try this" with a real pad and a real-rig check on the show VLAN are still open.

Built since (walk2, 2026-11-01, DONE.md): **level** (hold LB + LT / RT)
and **the light itself** on its target (hold LB + sticks, LB + A keeps;
VenueCorrections / the show's calibration, one Show Tree step).

Open:
- Level and the light itself in Focus this cue without walking (no free
  control on Refine's own map yet; `RefineSession::nudgeLevel()` /
  `nudgeLight()` are there for one).
- A **Needs work filter** in the Show Tree, and a column in the cue sheet
  once the sheet (sheet) is on main.
- The cue's note **typed or spoken** from the walk (today: on the cue
  page).
- PlayStation touchpad swipes (cue / layer) and the adaptive triggers:
  DONE (feat/04-dualsense-extras, Q199); a real-DualSense touch test is
  open. Still open: tap = tag, and the trigger click (Weapon mode) tuned
  by hand.
- In Operate, following the running cue as GO moves it (today Refine goes
  blind until it runs again).
- "Layer 2 of 4: Movers, Blue": the layer's colour in the line.
- decided (can change, walk2): hold LB is the modifier for level and the
  light itself, so a tap of LB steps back a layer on its release; the
  sticks without LB stay the cue's (on an aimed layer still refused, now
  saying "pick the light and hold LB"); the light itself is one light at
  Fixture scope on a light the cue aims (an Aim position); A alone keeps
  the light when only it is pending; level is one value for the scope (a
  fan in the old Dimmer look is not kept) and a Show Tree cue only (not a
  Build scene); in Operate the light itself needs the cue running (quiet
  Refine, blind refuses).
- decided (can change): Menu enters and leaves; View solos, so undo is
  View + B; a tap of Y is still the scope and a hold (0.8 s) tags; solo is
  Design only and plays the layer's scene as the cue bakes it (a light an
  upper layer takes stays dark); stepping off a cue with a change not kept
  (or Menu) is refused and said; the walk starts on the cue picked in the
  tree (Design) or the running cue (Operate); no wrap at the list's ends.
- Not verified with a real pad on the rig.

Follow-up notes kept from the design (chat 2026-10-30): targets for layers
are set as for any light (drop a Library > Target on the layer); one button
map for Xbox and PlayStation, the touchpad only adds shortcuts; the button
budget is the walking map + hold LB as a modifier.

## House rig, tour package and universes per venue (board Q137-Q138, 2026-10-24)

**Status:** built on `main` (docs/design/VENUES.md): owners on fixtures and groups,
house rigs kept in the venue sidecar and swapped with the venue, group
fills, Tour/House slots, the venue's output patch, "not ours", the clash
check, the venue page, the plot's Owner overlay. Open: Branson confirms
docs/design/VENUES.md section 11; a real-rig check that a not-ours universe stays
silent on Art-Net/sACN.

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
with the throw). **Part C, the audience-zone check, done** (Show Review PWR-09, rev2
2026-10-31). **Part C's H×V window and the no-light zones, done** (nlz,
2026-11-01): a laser is its H×V scan window (drawn in Top, checked by
PWR-09 and PWR-11); `NoLightZone` on the plot (Add ▸ Add No-light Zone,
saved in the show), Show Review PWR-11 (cue, lights, zone) and PWR-10 (static
or tight window). Decided (each can change): zones are saved in the show, not
the venue file (which carries only the power); H is the definition's widest
lens angle, V its narrowest when declared and narrower, else square (40
degrees when none); a zone keeps out all light, narrow beams (25 degrees or
less) and lasers, or lasers and star projectors; a beam stops at the stage
floor, so a pit's zone must reach up to it; PWR-11 names the first cue a beam
starts to cross; zones are click-through boxes (edit by double-click or the
right-click menu; dragging by the outline or centre handle since lft2,
2026-11-01); one new Add entry. **Open:** a density rule
that reads the laser's pattern (the show doesn't describe patterns), the
coverage outline, the Type=Laser emission inference, and FixtureDefCheck's
laser checks.

## AI Assist — P0–P3 built, leftovers (0.5, docs/design/AI_ASSIST.md; ai, ai2, ai3, mcp, 2026-10-31)

Built: P0 operation registry, P1 settings / providers / read-only ops, P2 write ops with preview and undo, P2b configuration import, P3 MCP server (DONE.md 2026-11-01 "Moved from TODO.md"; help in docs/features/ai-assist.md). Off by default, torn down in Operate, works offline; every apply is one undo step.

Decided (each can change):
- P1: decided: the Show Tree grammar stays one `command` operation in P0; its words become operations of their own in P2, when write operations get schemas and previews (can change).
- P1: decided: `?` alone and `? <one grammar word>` stay the command line's help; any other `? …` is a question, only with AI Assist on (can change).
- P1: decided: the answer shows in the command line's help pane, no new panel or dock (can change).
- P1: decided: a resource error raised in Operate stays in the footer until back in Design (can change).
- P1: decided: Virtual Console widgets aren't checked by `unused` yet (the UI can set `OperationRegistry::setFunctionUseProbe`; the answer says so; since vcx the VcIndex answers it) (can change).
- P2: decided: a typed line still runs as the `command` operation, unchanged; the change operations have no command-line word of their own -- they are the tool forms of the grammar's words, each rendering and running one grammar line, so the grammar stays the one text editor of the tree (can change).
- P2: decided: palette housekeeping (`palette … rename/delete`), `use`/`reusable`, `detach`, `convert`, hits and `vcpage` aren't offered yet: palette steps are a separate undo stack (PaletteOps) and would break "one undo step" in a mixed plan; the others are rarer (can change).
- P2: decided: the proposal applies where it was asked (the selection at the question), not where the tree is now (can change).
- P2: decided: a step refused when applied keeps the proposal on screen with why; Esc drops it (can change).
- P2: decided: the golden prompts are written in the shape Ollama's /v1 sends (P1's recorded call is real); the two live runs above are the real-model check (can change).
- P2b: decided: fixtures, groups and palettes are not part of `import_config`; File ▸ Import ▸ Workspace already brings those in with id and address remapping, and the summary says how many the file has, so the dangling-reference check has nothing to check (can change).
- P2b: decided: a universe has a "collision" when the target universe here has a non-default name or any patch in the selected sections; a fresh show's empty universes are not collisions, and the choice (skip / replace / renumber) must be given, never guessed (can change).
- P2b: decided: an input profile that isn't installed here is a warning, the profile name is kept (can change).
- P2b: decided: the typed command shows the preview and only `apply` makes the change (can change).
- P2b: decided: a universe's controller slot is not imported (a warning says so); only `.qxw` is read (can change).
- P2b: decided: the route checks run when planning/importing; the Operate-switch and route-change re-check of §5b stays with the existing rig-network gate (can change).
- P2b: decided: one new row in File ▸ Import (the design names it), no "Any Supported File" dispatch to it since a `.qxw` is the Workspace importer's (can change).
- P3: decided: transport = stdio through `qlcconsole --mcp` to a QLocalServer in the running app (a Unix socket in a per-user folder, a named pipe on Windows): no TCP port at all, and it works on the show that is open rather than loading a file headless (can change).
- P3: decided: the server needs AI Assist on as well as its own box, and proposals need a second box, both off by default (can change).
- P3: decided: a change call answers at once ("proposed as step N") rather than waiting for the operator; the client checks the result with the read-only tools (can change).
- P3: decided: an MCP proposal is refused while the operator has an in-app question or proposal open, and a new question discards an MCP proposal (can change).
- P3: decided: only one qlcconsole per user serves; a second reports it on the settings page (can change).

- One command line (feat/04-one-command-line, cli1): built -- command → run; typo → local spell-check (`ShowTreeCommand::suggest`, Enter runs / Esc restores); unparsed + AI on + Design → the assistant with the parser error and syntax (`AiAssist::ask(..., parserError)`); `?` forces a question; Operate never asks; the AI button is removed.
- cli1: decided: the correction goes in the line and a second Enter runs it (nothing runs that wasn't read), Esc puts back what was typed (can change).
- cli1: decided: only grammar failures (`Result::unparsed`) go to the AI; a failure the show answers (no such palette) gets the spell-check only (can change).
- cli1: decided: Build's command bar keeps `?` questions but has no plain-words fallback or spell-check yet (can change).

**Next:**
- MCP: a headless `--mcp <show>` mode on a file (the 2026-09-10 idea
  below) if wanted; Windows named-pipe path untested on the VM.
- Small: VC widgets in `unused`; Linux/Windows keychain (QtKeychain);
  Tab completion of the registry's words; palette and authoring operations
  (patch_fixture, create_scene/chaser/group of §4) once they can join a
  plan's one undo step; a ShowTreeView test of the applied selection
  (needs AiAssist in ui/test).

Guardrails: separate `ai/` module behind `QLC_AI_ASSIST`; no AI code on
MasterTimer/universe threads; an ROI line is in STRATEGY.md §3.3.

## Show Review: an auditor for the show's design (0.5; v1 frame built, rev, 2026-10-31)

Design in [docs/design/SHOW_REVIEW.md](../design/SHOW_REVIEW.md); help in
docs/features/show-review.md. Built: the engine registry, Accept with a note, `--review`, the Review chip and panel, 59 checks (DONE.md 2026-11-01 "Moved from TODO.md").

Decided (each can change):
- Songs (Q1) are the production's numbering-scheme song sections (Q190); no
  "song starts here" flag.
- Who (Q3): the name typed in the Accept dialog, remembered on this computer,
  starting from the account name; no new Settings item.
- Blockers and Operate (Q4): no question in v1; the chip says it.
- Hand In (Q5): the CLI only; the Level 2 check asks students to review.
- Strobe (Q7): four lights or more over 3 Hz at once is a warning, fewer a note.
- House zone (Q2) and Plays on (Q6): not needed yet (MOV-04/05, FX-01 not built).
- The chip re-checks 2 s after an edit, in Design only, and stops doing so
  after a run over 500 ms (the design had live review off by default).
- No menu item and no shortcut (menu count flat; ⇧⌘R is the Rig Overview).
- CUE-08 with no base cue is a note (releasing to dark is a way of working).
- PWR-01 doesn't resolve colour palettes (most of a run's time on a big
  show): an unresolved colour counts as white, the most it can draw.
- VC-01: any two hits in one place can be on together (accept when a
  button steps through them).
- PAL-02 reads Build's scenes only; the tree's say it as PAL-01/03.
- Accept is not an undo step yet.

Realistic move speed (mov, 2026-10-31): `MoveSpeed` (engine) gives travel time
from the definition's pan/tilt range; speed itself is a default per type,
decided (can change): moving head 0.4 s per 180 degrees, scanner 0.1 s (no
speed field exists in definitions). The Rig Overview walks each head with a
per-head `MoveSpeed::Follower` (the Rig Overview footer's "Real move speed"
button, on by default). MOV-02 reads the dark gap as the cue before's hold.
Left: MOV-03/04/05 (need cue-level position timing and the house zone); the
2D plot still snaps; a per-fixture speed override.

Leftovers of v1:
- v1 check not built: FX-01 (needs the plays-on setting and a per-script
  cost table per platform, measured on each target; not cheap). Built since
  (rev3, 2026-10-31): VEN-01/02/05/09/10, VC-02/07/09, CUE-15. Decided (can
  change): VEN-09 fires for an unreadable venue file, or a missing one when
  the show has venues or House universe slots (the show file itself never
  says a venue is applied); the CLI loads the venue file beside the show
  without applying it (`VenueCorrections::loadForReview`); CUE-15 doesn't
  count Operate's own Sub GO key (nothing on the console shows it), so it is
  a note; VC-09 owns Cue List widgets on an imported chaser and CUE-12 no
  longer says them twice; VC-02 only reads buttons holding both hits.
- VcIndex (done, vcx, 2026-10-31): one reader of the `<VirtualConsole>`
  element in the engine; the app saves the live console into memory and
  reads it back, `--review` reads the file, so they can't differ (decided,
  can change: not two fillers tested against each other, §5.3). VC-03/04/05/
  06/08 and CUE-13 on it, CUE-12's VC buttons, AI Assist `unused`. Show-control and
  OSC routes don't count for CUE-13 yet.
- Built after v1 (rev2, 2026-10-31): TEACH-01, PWR-09 (lasers and star
  fields into the audience; decided: no audience area is modelled, so the
  house is downstage of the stage edge below 3 m, can change), CUE-03,
  CUE-12 (chasers/collections/timelines and VC widgets still playing an
  original; the edited-original cases wait on a rebake compare), PAT-05, FIX-02 (definition errors only), FIX-08, FX-02, FX-03
  (EFX size/speed 0 and matrix speed 0; a paused speed master isn't read),
  FILE-03 (carried copies that fail or differ; the .qxw "uses the user
  folder" case isn't built).
- Parts of built checks: CUE-01's "past N cues" rule; CUE-05's loop and
  last-cue cases; CUE-06's gaps (notes); CUE-07's "jumped to" cases; PAL-01
  against tracked layers; PWR-01 hits with the running list; PWR-05 chases
  and matrices flashing under 333 ms; FILE-01 plot and scenery pictures.
- Accept all like this; Accept as an undo step; one-click fixes; Review
  before Operate and in Collect for Transfer; the chip hideable in
  Settings ▸ Appearance; counts saved with the show for the chip on open.
- The course and sample shows reviewed in the gate with expected results;
  a timing test on the stress-test show (serial list).

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
    shared Library (P2/W6; **the Library and the command bar are in Build
    since 0.4**, below), the bench (W4) and the effect designer on a
    layer (W4) are to come.
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
- **W6 shared Library + command bar in Build — done (0.4, w6,
  2026-11-01).** The Show Tree's Library is lent to Build (one instance, one
  model, Recent shared); the Command bar runs the registry with a Build
  context (`Towers colour Blue`, `dimmer 50`, `palette …`, `undo`, `?`;
  cue words answer "Show Tree only"). Left over:
  - The Palettes section still shows its own palette tree and Bundles tab
    above the Library; folding them into the Library ("Bundles as a Library
    section") is the rest of P2.
  - A line's new palette (`dimmer 50` made one) is not part of the line's
    undo step; the scene's change is.
  - An effect's `fade` (and its own colour / dimmer) is the cue's: Build
    refuses it and points at the effect designer.
  - The AI Assist `? question` answers here; a plan it proposes needs a cue
    and so is for the Show Tree.
- Then W7.

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
  - **Rig view frame cost:** angled with beams, 62 ms CPU a frame for 240
    fixtures (plan 2.2 ms); with Live on in a running show up to 25 frames
    a second, ~1.5 cores. A frame budget (rate or supersample down while
    values change fast).
  - **Show Tree edit at scale:** 155 ms CPU per edit on 200 tracking cues
    over stage groups, nearly all `planCue()` re-planning every cue on
    every bake. Incremental bake (only cues whose resolved state changed).
  - ~~**Rig view frame cost**~~ — done (feat/rig-cost: a CPU frame budget,
    light frames while values move fast, the people's faces kept).
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
  --summary`): diff two snapshots for what grew. **2026-11-06 (0.4.1):**
  an hour of `stresstest/studiosoak/` -- DanceHysteria-0.6 with the
  Lighting Studio in front, the Rig Overview open and looks cycling,
  offscreen, `MallocStackLogging=1`, snapshots every 10 min -- does not
  grow: footprint 286 MB at 1 min, 273-311 MB after, 302 MB at 60 min (it
  moves with the looks playing; ~65 MB of it is the stack logging), `leaks`
  0 at all six snapshots, the heap 187.0 MB at 12 min and 190.4 MB at
  52 min (the faders' FadeChannel hashes for the looks then running,
  harfbuzz font tables, the Rig Overview's throw cache: all bounded). So what grows is
  what offscreen does not draw: the Cocoa backing store / CoreAnimation
  path. Left: kronk's `--heap-track` snapshots (not on ender) or a
  `vmmap --summary` diff of the real app on kronk's screen.
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
  - **Duplicate a structure's setup** — BUILT (0.4, sdup, 2026-11-01):
    right-click a structure → Duplicate Setup to… (`StructureDuplicate`,
    engine/src/structureduplicate.*; dialog in ui/src/monitor). Decided
    (can change): fixture groups default to adding, plot folders to
    parallel; an occupied target defaults to Skip. Hangers and child
    bars (and the lights on them) are copied too (lft2, 2026-11-01); Replace
    takes the target's own off, undo puts them back.
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

*Built as AI Assist P3 (2026-10-31): an MCP server in the running app over
the Operation Registry (see the AI Assist section). The headless
file-editing server sketched below is not built.*

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
  to the item above; 2026-11-03: Settings ▸ Performance now shows the
  limits.d / setcap commands and the timer reports its policy; the timer
  also takes priority within a smaller rtprio limit. Packaging (postinst
  setcap / bundled limits.d rule) still undecided)** — the DMX timer thread's real-time
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
