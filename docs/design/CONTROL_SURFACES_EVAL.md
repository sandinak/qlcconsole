# Control surfaces: independent evaluation of the plan (rev 2)

Date: 2026-11-24. Inputs: the rev-2 plan (`CONTROL_SCOPES-rev2.md`), 8 journeys
walked from a blank show to a running one (3 in full: student fader box, pro
APC40 mk2, PMJ desk; 5 scored: Stream Deck, game pad, touring swap, mid-show
trouble, teacher classroom), 4 designs, a safety adversary and a completeness
critic. Code read on `qlcconsole-053-ctlplaces`; nothing built or run.
Scores for "Playback Desk" past two journeys, and all of "This Computer's
Desk", did not reach this report; where they are used it says so.

Spot-checked in the code for this report: `MidiPlugin::openInput` refuses a
device that is already open and `closeInput` closes it for every user
(`midiplugin.cpp:208-240`); `ControlMap::Mode` has Toggle / Momentary /
StartOnly / StopOnly and no Back (`controlmap.h:123-129`); `KeyboardUniverse =
0xFFFFFFFE` (`controlmap.h:217`); the hub's PMJ row matches any profile name
containing "PMJ" (`controlsurfacespanel.cpp:343`); there is no fader pick-up on
this branch.

---

## 1. Verdict

1. **Build rev 2, hardened** (two layers, the desk named by its identity), with
   **Draw the Desk's Assign window** grafted on, and **three plan changes**.
2. The hardened plumbing goes first and on its own (one shared device open,
   one key per physical control, `Surface=` on saved bindings). Without it,
   enabling a desk the show also patches breaks one side or fires twice.
3. Change 1: **a show still carries a fallback STOP, GM and GO**, armed only
   when this computer has none. As written, §5.5 makes the stop vanish on lab
   machines, the show-day computer and any box with no type.
4. Change 2: **a keyboard and on-screen STOP that nothing can switch off**,
   always beside any desk STOP, and a desk STOP that ignores the feature switch.
5. Change 3: **fix today's broken bits first** (no Back, GM set-up hidden, PMJ
   lamps blanked by the Master, help wrong in two places). They hurt the Feb
   show more than anything new helps it.

## 2. The journeys

| Journey | Today: biggest friction | With the recommendation | Blocker left? |
|---|---|---|---|
| Student, cheap 16-fader box, 20-cue show | Five set-up places for one box (patch, VC Settings for GM, Learn Trigger, Control Map stop, VC sliders); nothing says the box is plugged in but unpatched; no Back | Footer chip "USB MIDI Device: not set up" opens the hub; Make a Surface asks for GO / Back / STOP / GM; hits via Assign to Control, which refuses a GO on a fader; looks still on VC sliders, now with pick-up | Only if the lab wipes settings: the show's fallback covers GO/STOP/GM, so no |
| Pro, APC40 mk2, 2-hour concert | About 30 learns per show, Ableton-mode init hidden in Configure MIDI, busk faders only via hand-paged VC sliders | Enable once per Mac; GM/STOP/GO/Back/Tap live in every show; pads assigned by cue; init SysEx part of the type, resent on replug/wake | Busk faders per song wait for fader rides (after Feb); VC pages until then |
| PMJ desk, open any show | PMJ only works in shows patched with its profile; Back can't be bound; Master move blanks learned lamps | Enable once; PMJ gets GO, Back and STOP roles; one lamp writer; works in a blank show | No, once the PMJ type gains GO/Back/STOP (decision 5) |
| Stream Deck via Companion | No hub row; Assign waits silently for a key that never comes; PIN can block ALL OFF | Hub row says Companion keys are set in Companion; Assign shows that hint; ALL OFF never behind the PIN | Companion free keys stay "later" (Q312) |
| Game pad | Show needs a pad patch for pad GO/stop; single-press pad learns allowed | Pad shown as an all-static map; single-press pad learns refused (2-control arm held) | Pad on a show with no pad patch: needs the HID shared open, not in S3 |
| Touring: venue desk swap | Show's controls tied to a universe; another port or computer goes quiet | `Surface=` bindings find the same model on any port; unresolved ones listed "waiting for an APC mini mk2", never dropped; pre-show list of what's missing | Swapping models (APC to X-Touch) is still a re-assign pass |
| Mid-show trouble (unplug, sleep, wrong press) | Desk STOP dies with the show's patch; no sign which jobs were lost | Footer chip names lost jobs and what still works; desk line survives show loads; held controls released and GM re-picked-up on replug/wake | No, if the wake/replug tests pass on hardware |
| Teacher, 6 laptops x 2 desks | Per-show set-up on every laptop; PIN is per show | Export/import "this computer's desk" file; teacher lock per computer; Not Here for every show | Lab images that reset settings need the import step in the course |

## 3. Design comparison

Scores 0-10 from the scorers (cost: higher means cheaper as scored;
Rev-2 hardened's L-XL got 3). "n/r" = not received by this report.

| Journey / axis | Rev-2 hardened | Draw the Desk | Playback Desk | This Computer's Desk |
|---|---|---|---|---|
| Student fader box | 4 | 5 | 5 | n/r |
| Pro APC40 | 6 | 6 | 8 | n/r |
| PMJ desk | 5 | 4 | n/r | n/r |
| Stream Deck | 2 | 2 | n/r | n/r |
| Game pad | 3 | 6 | n/r | n/r |
| Touring swap | 6 | 5 | n/r | n/r |
| Mid-show trouble | 6 | 4 | n/r | n/r |
| Teacher classroom | 4 | 4 | n/r | n/r |
| Safety | 6 | 5 | n/r | n/r |
| Findability | 5 | 7 | n/r | n/r |
| Cost | 3 | 4 | n/r | n/r |
| Fits guardrails | 6 | 7 | n/r | n/r |

What each is best at:

- **Rev-2 hardened**: the plumbing. A shared, counted device open; one key per
  physical control (`canonical(universe, channel)`); `SurfaceLine`
  pseudo-universes on the `KeyboardUniverse` precedent; `Surface=` on bindings
  with a universe fallback; footer chip per surface that names lost jobs;
  STOP one dedicated key, never Shift; T1-T8 journey tests on a fake MIDI line.
  It is the only design that closes the double-open and double-fire holes.
- **Draw the Desk**: findability. Assign to Control opens the desk picture with
  the thing chosen; free tiles glow, static ones say why, taken ones name the
  owner before the press; a press in that window selects its tile and **fires
  nothing**; tiles typed fader/knob/button so GO on a fader is refused; a
  "not enabled" chip for a new device; tile, lamp, footer and refusal text
  checked equal by a gate test; the pad drawn with the existing
  `FocusControlMap`.
- **Playback Desk**: the pro's busk. Numbered playbacks whose page follows the
  show, held-fader handoff shown in the Run bar, and a level fader that can be
  "everywhere" (a real submaster, which Q298's drop-at-GO is not). Its cost is
  a third address space (`PlaybackUniverse`) and controls that change meaning
  under the hand at each GO; keep the ideas, not the model.
- **This Computer's Desk**: resolves the desk to whatever universe the show
  patched. The adversary's top blocker (no patch, no STOP) rules it out.

## 4. Safety: the adversary's top items

| # | Hazard | Sev. | How the recommendation handles it |
|---|---|---|---|
| 1 | Desk STOP can't fire when the show has not patched the desk | blocker | Desk opens on its own line at startup, before any show (Q305); show load never closes it. Test with a blank show |
| 2 | One MIDI device can't be opened twice; closing either user kills both | blocker | Phase P: counted open per plugin line, close at zero, fan values to each listener. Gate test opens both orders, closes either, STOP still fires |
| 3 | Same press arrives on the show universe and the desk line: double GO skips a cue | blocker | Phase P: one canonical key per device control, one dispatch per physical edge; Q309 checked on that key |
| 4 | Two stops (show and computer) disagree, or one press both stops and GOes | major | A STOP control can hold no other role in any layer; the show's stop stays live only on a control nothing else uses (decision 2); STOP refused on a non-button |
| 5 | GM jumps on show open, replug, wake, or a device's state dump | major | GM picks up after load / replug / wake; a pull to 0 always applies at once (decision 3, amends Q313) |
| 6 | Sleep/wake or replug: dead desk or ghost press | major | On disconnect, connect and wake: release held controls, clear stop-held state, need a fresh 0 before faders arm, chip says gone / back |
| 7 | STOP silenced by the Control Surfaces feature switch | major | Desk STOP goes through `ControlMap::emergencyStop()` before any feature, mode, place, learn or page check, as the show's stop does today |
| 8 | Pages that follow cues change what a fader does under the hand (Playback Desk) | major | Not adopted as a model. Rides get pick-up and a Run-tab hint |
| 9 | `Surface="type#n"` lands on the wrong unit on another computer | major | A binding whose saved desk is known but absent is kept, not armed, listed; no silent universe fallback then |
| 10 | Desk GO fires the live show from Design | minor | GO keeps the double-GO guard; footer says "GO from desk" each time; GO/Back are the only no-arm roles |
| 11 | Brushed speed knob changes all tempos in Operate | minor | Speed roles use pick-up; relative knobs need a detent; footer says it |
| 12 | Auto-laid-out map misplaces controls | minor | Assign by moving the control first, clicking a tile second; map labelled "layout guessed" |

Also from the journeys, not on the adversary list but real today:

- **PMJ lamp overwrite**: `pushDeviceLeds` sends 0 to every role-less LED on each
  GM change, blanking a learned GO, hit or stop lamp (`controlsurfaceengine.cpp:58-72`).
  Two writers for one light. Fix in phase F: the overlay skips controls
  `ControlOwners` gives to the show.
- **A GO or hit learned on a fader** fires on a nudge. Refuse at Assign
  (typed controls, or the "moves like a fader" check the stop already has).
- **Latching buttons** on cheap boxes make a Toggle hit fire on every other
  press. Detect 127/0 alternation at learn and suggest Start only.

## 5. What the plan was missing

Add now (before or with S3):

- **Stop with no desk.** Keyboard and on-screen STOP always live, never
  replaced by a desk STOP, never behind a PIN, level or feature switch.
  When a desk STOP is unreachable, the show's own stop arms again.
- **Shared device open** (all plugins, not only MIDI) and **one owner for
  feedback** per control, so lamps and motor faders don't fight.
- **Portability**: shows carry a fallback STOP/GM/GO (decision 1); export and
  import of "this computer's desk" as a file; a pre-show list per desk of
  unresolved, blocked and missing controls, in Show Status or the hub.
- **Identity per OS**: USB serial where there is one, then port, then name;
  Windows prefixes ("2- APC40 mkII"), Linux ALSA numbers; an Identify press to
  say which of two identical units is A and B. Test on Windows, Pi, Linux.
- **Device init**: an init message in the type (APC40 mk2 Ableton mode),
  resent on open, replug and wake.
- **Back as a target**: cue-list Back in `ControlMap`, so a show can assign one
  and the help stops promising what the code can't do.
- **QLC+ 4 round trip**: never write a pseudo-universe to the file; always a
  real universe beside `Surface=`; test a file saved by QLC+ 4.
- **A fake input/output plugin in the gate** (scripted presses, fader moves,
  feedback capture, unplug/replug) so every journey becomes a test.
- **Undo**: assignments as steps on the shared history; Move It Here locked in
  Operate.

Add later (after the Feb show):

- Device layers and Shift (APC bank, X-Touch A/B, Launchpad modes) as a
  `Layer=` on roles; VC page bits handled in the canonical key.
- OSC tablets: controls named by path, identity by sender, hash collisions
  refused.
- Companion as a surface in `ControlOwners` (lamp and owner feed), and one rule
  for several GMs: last mover wins, the others pick up, motor faders follow.
- Backup computer: "listen only" mode so a tracking backup doesn't double GO.
  (Bring forward if Feb has a backup machine: decision 7.)
- RGB colour tables and a lamp vocabulary (off, owned, active, armed, blocked)
  in `<Facts>`; MCU/HUI listed as unsupported.
- Print desk sheet per surface and show; accessible tile names; whole-sentence
  `tr()` templates for owner and place text.

## 6. Revised build plan

Smallest valuable first. Sizes S / M / L. Each phase: help, course note,
CHANGELOG line, gate tests that fail on old code, shots at 3 sizes for any UI.

| Phase | What | Size | Feb show |
|---|---|---|---|
| 0 | Land what's built: control collisions, button places, the hub, pick-up for non-master faders (Q313) | S | yes |
| F | Fix today: hub GM row opens VC Settings on the GM tab; cue-list **Back** target; PMJ lamp overwrite; PMJ row matches the exact profile and says if feedback is patched; footer line "this show has no stop / GO on this desk"; help: no restart for PMJ, Back, APC Omni channel and Ableton mode; GO/hit on a fader refused at learn | S-M | yes |
| S1 | Hub's two layers; read-only map (a list where there is no layout); rename to **Assign to Control…** with Draw the Desk's window: press selects, fires nothing; typed controls; owner shown before the press; "not set up" chip for a new device | M | yes |
| P | Plumbing alone: counted open per plugin line, `SurfaceLine` slots, `canonical()` in `ControlOwners`, `Surface=` on bindings with universe fallback, fake surface plugin in the gate | M | yes |
| S2 | `<Role>`, `<Facts>`, `<Surface>`, init message in the profile; PMJ's table into its `.qxi` (golden test) **plus GO, Back, STOP**; APC mini and generic box; Make a Surface roles page (moved up from S4, because an untyped box otherwise loses its stop) | M | yes |
| S3a | Static STOP, GM, GO, Back, Blackout per computer: Enable by identity, desk line at startup, `Static` owner, Q309, show fallback roles, always-on keyboard/screen STOP, GM pick-up on load/replug/wake, replug/wake handling, per-surface chip, export/import desk file, teacher lock | L | yes, the core |
| S3b | Old-show migration prompt (Q308) with "Not Here for every show" | S | if time |
| S4 | APC40 mk2, X-Touch, Launchpad types; Copy to Edit; per-role off tick; speed roles (Tap, Rate with relative support) | M | APC40 type only if the Feb rig uses one |
| S5 | Fader rides and layer/effect places (Q298, Q302), with an everywhere ride as a submaster; pick-up hint on the Run tab | M-L | no: VC sliders with pick-up cover Feb |
| S6 | Layers/Shift, OSC tablets, Companion as a surface, backup mode, print sheet | M each | no |

About 7 trains through S3a. If time runs short before Feb, cut S3b and S4 first,
never F or P.

## 7. Test plan

Gate tests (offscreen, fake surface plugin; names follow existing QTest style).
Each must fail on today's code.

**Phase F**

- `ControlSurfacesPanel_Test::grandMasterSetUpOpensGrandMasterTab`
- `ControlMap_Test::cueListBackStepsBack`
- `ControlMap_Test::backAndGoOnOneListCoexist`
- `PMJOverlay_Test::masterMoveKeepsShowLamps` (learned GO/hit/stop lamps survive a GM change)
- `ControlSurfacesPanel_Test::pmjRowMatchesExactProfileOnly`
- `ControlLearnDialog_Test::goOnFaderRefused` (CC 10, 20, 30 arrives)
- `ControlLearnDialog_Test::latchingButtonSuggestsStartOnly`
- `ShowStatus_Test::footerSaysNoStopOnThisDesk`
- `ControlMap_Test::twentyGosReachCueTwenty` (1.5 s apart; a press 0.3 s after a GO is held)

**Phase S1**

- `ControlLearnDialog_Test::pressSelectsAndFiresNothing`
- `ControlLearnDialog_Test::staticControlRefusedWithoutMoveItHere`
- `ControlLearnDialog_Test::controlNamedFromProfile` ("APC40 mk2 · Fader 3")
- `ControlSurfacesPanel_Test::unpatchedDeviceListedWithSetUp`
- `ControlSurfacesPanel_Test::rowsReadCountsAndOwners` (4 widgets, 5 triggers, stop, GM, no collisions)
- `ControlOwners_Test::tileFooterRefusalTextAgree` (one status source)
- `ContextMenus_Test::oneAssignItemPerThing` (no Learn Trigger…, menu count unchanged)
- `LayoutSanity_Test` and `WindowSanity_Test` for the hub and the Assign window

**Phase P**

- `InputOutputMap_Test::sharedOpenBothOrders` (desk line then show patch, and reverse; close either; STOP fires)
- `InputOutputMap_Test::showLoadKeepsSurfaceLine`
- `ControlOwners_Test::canonicalKeyOnePressOneFire` (old show's GO on the desk's GO advances one cue)
- `ControlMap_Test::surfaceBindingResolvesOnOtherUniverse`
- `ControlMap_Test::absentSurfaceBindingKeptNotArmed` ("waiting for an APC mini mk2")
- `ControlMap_Test::qlcplus4RoundTripKeepsUniverseFallback`
- `ControlMap_Test::saveWritesNoPseudoUniverse`

**Phase S2**

- `QLCInputProfile_Test::roleFactsSurfaceLoadAndSkipInQlcplus4`
- `PMJOverlay_Test::qxiRolesMatchOldTable` (golden, every channel)
- `QLCInputProfile_Test::stopOnlyOnButton` and `oneStopOneGrandMasterPerType`
- `FaderPickup_Test::motorisedNeverWaits`, `relativeTurnsFromCurrent`, `absoluteWaitsToCatch`
- `InputProfileEditor_Test::makeSurfaceRolesPage` (press-release-press for STOP)
- `MidiPlugin_Test::initMessageSentOncePerOpen`

**Phase S3a**

- `ControlMap_Test::staticStopFiresWithNoShowPatch` (blank show, then unplug/replug)
- `ControlMap_Test::staticStopIgnoresFeatureSwitch` (switch off, Operate, learn open, page held)
- `ControlMap_Test::keyboardStopAlwaysLive` (surface universe removed)
- `ControlMap_Test::showStopArmsWhenDeskStopUnreachable`
- `ControlMap_Test::showFallbackStopUsedOnBareComputer`
- `GrandMaster_Test::waitsAfterLoadReplugWake` and `pullToZeroAlwaysApplies`
- `ControlMap_Test::replugReleasesHeldControls`
- `ControlOwners_Test::staticWinsShowTriggerBlockedListed` (Q309, saved byte-identical)
- `ControlOwners_Test::vcInputOnStaticControlBlocked`
- `PadRun_Test::deskGoDoesNotBypassPadArm`
- `Teaching_Test::enableFollowsTeacherLock`
- `SurfaceSettings_Test::exportImportRoundTrip`
- `IdentityResolve_Test::twoIdenticalUnitsKeepRoles` (port swap)
- `FooterChip_Test::goneNamesLostJobs`

**Phase S3b / S4 / S5**

- `ShowLoad_Test::oldShowUseHereNotHereOncePerComputer`, `migrationSavesByteIdentical` (on the DanceHysteria and Office test shows)
- `SpeedMasters_Test::relativeRateStepsNeverJump`
- `SurfaceRoles_Test::perRoleOffTick`
- `FaderRide_Test::catchThenFollowThenDropAtGo`, `everywhereRideIsSubmaster`
- `RunTab_Test::pickupHintShownInRunBar`

Hardware checklist (by hand, per device; results in the device checklist in
`control-surfaces.md`):

- **Cheap generic MIDI box (nanoKONTROL-class)**, Mac and Windows: name and
  identity, across a port change and a replug mid-show; Note or CC, momentary
  or latching; fader jitter at rest; GM parked at 0 then touched; STOP
  learned on a button and refused on a fader; 20 GOs, none lost or doubled.
- **APC40 mk2**: Ableton-mode SysEx takes, and is resent after replug, hub
  power blip and sleep/wake; pad colours for ready / running / GO held /
  stopped with "Dim a ready control" on and off; Device knob rings; Cue level
  and Tempo encoding; Tap Tempo; all 8 track faders heard on Omni; Master
  first-touch; STOP key placement and brush test; 2-hour soak with lamp drift
  sampled every 10 min.
- **APC mini mk2**: STOP on a dedicated key (not Shift row); two identical units,
  reboot and port swap.
- **PMJ Black 1**: GO/Back/STOP keys work in a blank show; lamps of learned
  controls survive Master moves; O (Blackout) vs STOP labelled clearly; Windows
  port name.
- **Game pad (DualSense, Xbox)**: arm still needed with a desk GO live; stop on
  Windows; unplug with a held button, replug, first press works.
- **Stream Deck via Companion**: hub row, ALL OFF not behind the PIN, hit
  numbering after a tree edit.
- **Keyboard**: STOP with every level, PIN and switch combination.
- **Show move**: show to a second computer and a fresh account; GO, GM, STOP
  work or are named missing before the first cue.
- **Network**: OLA probe universe 15 on the show VLAN confirms ALL OFF reaches output.

## 8. Decisions for him

1. **Does a new show carry its own fallback STOP, GM and GO?**
   A: no, as §5.5 says (the desk lives only on the computer). B: yes, written in
   the show and armed only for a role this computer lacks. C: only the STOP.
   *Recommend B.* Lab resets, the show-day computer and untyped boxes otherwise
   have no stop; the file still opens in QLC+ 4. Amends Q308 §5.5.

2. **An old show's stop when this computer also has a STOP.**
   A: off once the computer has one (Q308 as written). B: both live, unless the
   show's stop sits on a control the desk uses for anything else. C: both live
   always. *Recommend B.* An extra ALL OFF beats a missed one; a shared control
   must not both stop and GO.

3. **Grand Master on first touch.**
   A: follows at once (Q313). B: waits to pick up after load, replug and wake;
   a pull to 0 always applies; otherwise follows. C: always picks up.
   *Recommend B.* A parked fader blacking out or flashing the stage on the first
   brush is the most likely live failure for a non-motor desk.

4. **Desk GO: one press with no arm?**
   A: one press, double-GO guard (rule 6). B: A, plus a per-computer tick to turn
   a single static role off (no remap; Q307 otherwise stands). C: GO needs Shift.
   *Recommend B.* Keeps the console feel for pros; a busk-only LD can drop GO
   without making a new type. The pad keeps its two-control arm either way.

5. **First-type key table.** PMJ: GO = Go, Back = Back, STOP = Favorites? (O stays
   Blackout.) APC40 mk2: GO = Play, Back = Record or a scene key, STOP = Stop
   All Clips, Speed 1 = Tap Tempo (not Cue level, which is relative). APC mini:
   STOP on a dedicated key, not the Shift row. *Recommend as listed*; he picks
   the physical keys on the bench, the test pins them.

6. **Who may Enable a desk at Student level?**
   A: the show PIN (as planned; File ▸ New gets around it). B: a teacher lock on
   the computer, separate from the show PIN; a student's own laptop without it
   is free. C: anyone. *Recommend B.*

7. **Will the February show have a backup computer hearing the same desk?**
   A: no; backup mode stays later. B: yes; build "listen only" with S3a.
   *Recommend A unless he plans a backup*; it costs about S.

8. **A fader on a look: submaster or ride that drops?**
   A: rides drop at GO (Q298) for every place. B: a ride at cue or list drops; a
   ride at production or everywhere stays (a submaster). C: submasters stay VC
   sliders. *Recommend B* (from Playback Desk), built in S5; VC sliders with
   pick-up carry the Feb show.
