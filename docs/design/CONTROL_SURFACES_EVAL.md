# Control surfaces: full evaluation of the plan (rev 2)

Date: 2026-11-24. This replaces `EVALUATION.md`, which was written from cut-down
inputs. This version uses **all 8 journeys** (student fader box, pro APC40 mk2,
PMJ desk, Stream Deck, game pad, touring swap, mid-show trouble, teacher
classroom), **all 4 designs with every score**, the safety adversary and the
completeness critic. Code read on `qlcconsole-053-ctlplaces`; nothing built or run.

**What changed from the first report.** The shape is the same: two layers, a
show that keeps its own stop, an always-on keyboard STOP, today's broken bits
first. The full scores for **This Computer's Desk** (missing last time) move the
*February route*. It scores highest overall, safest and cheapest. So the per-
computer desk for February goes through the show's own patch, and rev 2's
computer-owned line, surface types and drawn map move after the show.

---

## 1. Verdict

1. **For February, build "This Computer's Desk"**: one per-computer record (STOP,
   GM, GO, Back) learned by pressing, found through the show's own patch by
   device identity, with shows still writing their own stop, GM and GO.
2. **Graft onto it**: Draw the Desk's Assign window (press selects, fires
   nothing, owner shown before you press), GM pick-up after load/replug/wake, a
   per-computer teacher lock, owner kinds for the pad and keyboard, and one
   dispatch point so a static control can't also fire a show listener.
3. **Why**: it closes the "no stop at all" holes rev 2 opens (lab computers,
   unknown boxes, desk unplugged), avoids opening one MIDI port twice (Windows
   is exclusive), and costs about M instead of L–XL.
4. **Keep rev 2's direction for after February**: the computer-owned line, surface
   types in `.qxi`, the drawn map, `Surface=` on bindings, fader rides. Build each
   only when a journey shows the need (touring and the pro APC are the likely ones).
5. **First**, fix today's broken bits (no Back, PMJ lamps blanked, Stop All greyed,
   Windows panic key, help wrong). They hurt the February show more than any
   new feature helps it.

## 2. The journeys

| Journey | Today: biggest friction | With the recommendation | Blocker left? |
|---|---|---|---|
| Student, cheap 16-fader box, 20 cues | Five set-up places for one box; nothing says it is plugged in but not patched; GM set-up hidden in VC Settings; no Back; a GO learned on a fader by mistake | Footer chip says the box is here and not set up. Four presses set STOP, GM, GO, Back (each refuses the wrong kind of control). Hits via Assign to Control, which refuses GO on a fader and flags latching buttons. The show still carries its stop, so a wiped lab login loses nothing | Looks on faders still need VC sliders (now with pick-up). No blocker |
| Pro, APC40 mk2, 2-hour concert | ~30 learns per show, no user template; Ableton-mode SysEx hidden in Configure MIDI; busk faders only via hand-paged VC sliders | STOP, GM, GO, Back learned once per Mac. Ableton mode and Omni channel in the help and the APC checklist. Pads assigned by cue as today | Busk faders per song wait for fader rides (after Feb); speed masters and encoder bank still per show |
| PMJ desk, open any show | Works only in shows patched with its profile; Back can't be bound (help says it can); Master move blanks learned lamps | Desk GO/Back on the PMJ's free Go/Back keys, a real Back target, one lamp writer. A show without the PMJ patched shows a red chip with Patch It | One click (Patch It) on a foreign show; PMJ faders stay Design-only |
| Stream Deck via Companion | Module not in the store; Assign waits forever for a Companion key; PIN can block ALL OFF; "everywhere" hit can't be let go | Assign shows "Stream Deck keys are set in Companion" when a Run client speaks; ALL OFF never behind the PIN (decision 9); a hit can always be let go; footer names a refused press | Module publishing and stable hit ids still open (after Feb) |
| Game pad, Pad Run | Pad needs a show patch; hub says "No pad connected" while one is; a trigger learned on Cross also fires with every L2+Cross GO | Pad owner kind refuses show triggers on Run-map buttons; hub row sees an unpatched pad and offers it | Pad still patched per show (computer line after Feb) |
| Touring, house desk swap | Repatching universe 1 silently moves the stop and triggers to unlabelled buttons; stop looks learned while its desk is gone | Repatch warns and lists what moves; hub and Show Review say "stop's desk not connected"; house desk gets STOP/GM/GO/Back in four presses | Re-assigning show triggers to another model is still one by one |
| Mid-show trouble | Footer says a device left, not which jobs went; VC flash stays on after unplug; Stop All greyed with only held levels; Ctrl+Shift+Esc opens Task Manager on Windows | Chip names lost jobs and what still works; held controls released on unplug; GM waits to pick up on replug/wake; one ALL OFF path, always enabled; wake notice | No, if the replug and wake checks pass on hardware |
| Teacher, 6 laptops x 2 desks | Everything per show; no class-wide view; swapping desk models fires wrong pads silently | Handout still carries stop and GM, so a laptop without a desk record still has a stop. Teacher lock per computer; export/import of the desk record | Class-wide controls report and model-swap remap after Feb |

## 3. Design comparison

Scores 0–10 from the scorers. Build cost: **higher = cheaper**.

| Journey / axis | Rev-2 hardened | Draw the Desk | Playback Desk | This Computer's Desk |
|---|---|---|---|---|
| Student fader box | 4 | 5 | 4 | **7** |
| Pro APC40 mk2 | 6 | 6 | **7** | 5 |
| PMJ desk | **6** | 5 | 4 | **6** |
| Stream Deck | 2 | **3** | 2 | **3** |
| Game pad | 3 | **6** | 3 | 4 |
| Touring swap | 5 | 5 | **8** | 5 |
| Mid-show trouble | **6** | 4 | 5 | 5 |
| Teacher classroom | 4 | 4 | **6** | 5 |
| **Journeys total (of 80)** | 36 | 38 | 39 | **40** |
| Safety | 6 | 5 | 6 | **7** |
| Findability | 5 | **7** | 5 | 6 |
| Build cost | 3 | 4 | 3 | **8** |
| Fits guardrails | 7 | 6 | 6 | **9** |

What each is best at:

- **Rev-2 hardened**: the plumbing a computer-owned line needs. A counted,
  shared device open (MIDI `openInput` refuses a second open and `closeInput`
  closes for everyone, `midiplugin.cpp:208-240`), one key per physical control,
  `Surface=` on saved bindings, a footer chip that names lost jobs. Best for
  mid-show trouble once a desk line exists.
- **Draw the Desk**: findability. The Assign window *is* the map; free controls
  glow, static ones are greyed with the reason, taken ones name their owner
  before you press; the press is swallowed so nothing fires. Pad and keyboard
  maps from `FocusControlMap`. Best for the game pad (Run-map buttons become
  owners).
- **Playback Desk**: portability. Shows name playback numbers, not MIDI notes,
  so a show moves from an APC to an X-Touch by slot, with leftovers listed
  ("P12: no fader on this desk"). A raised fader keeps its look across GO until
  brought to 0. Best for touring and the classroom; heavy pro vocabulary.
- **This Computer's Desk**: safety per cost. Never retires a show's own stop;
  no double open, no double hearing, no new binding key; desk bindings exist
  only at run time and are never saved. Best for the student box and the
  February timeline. Weak on the assigned layer (no bulk reassign, repatch
  still moves bindings).

## 4. Safety: the adversary's top items

| # | Risk | Severity | How the recommendation handles it |
|---|---|---|---|
| 1 | A show ends up with **no hardware STOP** (rev 2 §5.5 moves STOP off the show; the desk route is silent if its device isn't patched) | blocker | Shows keep writing their own stop. Keyboard and on-screen STOP can never be unbound. Red "No hardware STOP" chip on the Run tab when no desk or show stop is live. Gate test per case |
| 2 | **One device opened twice** (computer line + show patch): Windows second open fails, macOS hears each press twice | blocker | Feb route has no computer line, so no second open. The counted shared open is the first step of any later computer line, with a Windows check |
| 3 | "Static wins" only gates the Control Map; VC widgets, GM slider, channel groups, speed masters, encoder bank listen to raw input and still fire (double GO) | blocker | One dispatch point: show listeners move to a filtered signal that drops controls owned by the desk. A gate test per listener kind. Sized M, not S |
| 4 | Control Map disconnects from input when it has no bindings (`controlmap.cpp:334`), so a desk STOP in an empty show may never be heard | blocker | Desk roles count toward "want input"; better, the desk STOP handler sits outside that connect cycle. Test in an empty show, after File > New, after a load, after a repatch |
| 5 | Universe fallback fires the wrong device when a saved desk isn't there | major | No `Surface=` until after Feb. When it comes: fall back to the universe only for bindings without `Surface=`; others are listed "waiting for APC40", never armed |
| 6 | Identical devices swap after replug (MIDI identity is name only) | major | Add USB location to the MIDI plugin; hub says when only a name tells units apart; Identify blinks one unit; the desk STOP never moves to an unconfirmed unit |
| 7 | **GM jumps** after load, replug or wake (follows at once, Q313) | major | GM waits to pick up after each of those; a pull to 0 always applies; device dumps in the first 500 ms are ignored for masters; chip says "GM waiting: bring fader to 100 %" |
| 8 | Page turn swaps what a raised fader holds (Playback Desk) | major | Not in the Feb build. If slots come later: old content stays until 0, new content waits from 0, amber "(held: old name)" |
| 9 | Desk GO silently follows the Run tab's list | major | Chip names the target ("GO → Act 2 main") and flashes on a change; double-GO guard keyed on the list (option to refuse the first GO after a change: decision 4) |
| 10 | LEDs lie: PMJ overlay blanks Transport LEDs on every GM move | major | One lamp writer per control, from the owner answer; test that a GM move leaves the GO lamp alone. Phase F |
| 11 | Auto-laid-out map tiles don't match the real device | major | Feb uses a list with "press a control to find it"; a click only preselects, the physical press confirms |
| 12 | Pad buttons have no owner kind; a single-press show trigger on a pad bypasses the 2-control arm | major | Pad owner kind refuses GO, Back, STOP, lists and bare one-press triggers on pad buttons (decision 10) |
| 13 | A show moved to another computer silently moves its stop and triggers | major | Repatch and load compare the patched device with the one the show was learned on; a mismatch unarms the stop and lists it |
| 14 | Unplug, sleep or wake leaves held controls stuck | major | Release everything held on a lost line (VC, channel groups, encoder bank as well as Control Map); pick-up on return; fixed reopen order |
| 15 | Mode switch mid-gesture freezes or snaps a Design-only fader | major | A switch releases Design-only holds and puts faders in pick-up; a move in Operate says once why it does nothing |
| 16 | STOP gated by a switch, mode or modal by accident | major | STOP matrix test: feature off, Learn open, Assign open, Design and Operate, teacher lock, modal, show load. Keyboard STOP too; no Ctrl+Shift+Esc on Windows |
| 17 | Two STOPs and unlisted remotes (Companion, web, OSC) | major | Both stops live and listed together; remotes registered as owner kinds so the hub names every owner |
| 18 | Desk record protected only by a show PIN | major | Teacher lock at Settings level, per computer |
| 19 | Patch It leaks the desk into the show file | minor | Patch It names the universe and offers Undo |
| 20 | Run-time-only desk roles invisible to Show Review | minor | Show Review gets a "This computer" section: each desk role and whether it is reachable now |

## 5. What the plan was missing (critic) and what to add

| Missing | Severity | Add |
|---|---|---|
| MIDI has no stable identity: only HID gives a location; Windows renames repeats "2- APC MINI"; ALSA numbers move | blocker | MIDI USB location (CoreMIDI, ALSA path); normalise Windows/ALSA names; "#1/#2, press a key on this one" picker; identity test rows per platform |
| Windows MIDI input is exclusive; pool gate is Mac + Linux | blocker | Feb route avoids a second open; any later computer line needs the shared open and a sandWindows check |
| No single lamp owner across ControlMap, VC feedback, PMJ overlay and desk roles | major | One feedback arbiter keyed like `ControlOwners`; on load, blank then one resend |
| Hardware banks/layers (APC40 bank, X-Touch A/B, Launchpad modes, nanoKONTROL scenes) | major | Hardware checklist item per device now; `<Bank>` in types later; live line names the layer |
| OSC tablets, web page and Companion as owners; two operators GOing at once | major | Register them as owner kinds; a two-operator double-GO test |
| Several desks with overlapping roles (two GMs) | major | One GM owner per computer; others marked "spare"; all GO/STOP sources live |
| QLC+ 4 round-trip and old-show migration undefined | major | Round-trip tests; old shows load with no warning; never cut the minimal old-show path |
| No printed desk sheet | major | Print / Save as PDF from the hub list (no new menu item); doubles as course handout |
| Sleep/wake/hub power-cycle recovery sequence | major | One recovery sequence for every listener; a fake-plugin test with a changed name, same location |
| Teacher distribution and reset of desk records | major | Export/import the desk record; Reset behind the teacher lock; hub says "show made for APC40, this computer has APC mini" |
| Pad and keyboard missing from the owner model | major | Pad and Keyboard owner kinds in phase F, non-movable entries for their stops |
| Scribble strips / device displays | minor | After Feb; reserve `<Facts Display>` in the type format |
| Motor faders under pick-up | minor | Drive to the owner value on load, place and page change, never while touched; BCF2000/X-Touch checklist |
| Accessibility of the map/chips | minor | Words or glyphs as well as colour; `accessibleName` from the owner text; keyboard navigation |
| Translation of composed sentences | minor | Whole `tr()` templates with `%1`, no string glueing |
| Linux and the Pi as show computers | minor | One Pi journey in the hardware list; identity tests in the Linux gate leg |

## 6. Revised build plan

Each phase ships with help, a course note, a CHANGELOG line, gate tests that
fail on old code, and shots at 3 sizes for any UI change. Sizes S / M / L.

| Phase | What | Size | Feb show |
|---|---|---|---|
| 0 | Land what's built: collisions, button places, the hub, fader pick-up for non-master faders (Q313) | S | yes |
| F | **Fix today**: cue-list Back target; PMJ lamp overwrite (one lamp writer); PMJ hub row matches the exact profile and says if feedback is patched; hub GM Set Up opens VC Settings on the GM tab; hub refreshes on connect/disconnect; hub sees an unpatched pad; Stop All enabled with held levels; one ALL OFF path for screen, desk, pad, phone; no Ctrl+Shift+Esc on Windows; Pad and Keyboard owner kinds; help: no PMJ restart, no Control Map Back, APC Omni channel and Ableton mode, Companion names on the Allow GO switch | M | yes |
| D | **This computer's desk**: record by identity (MIDI location added), four typed learns (STOP press-release-press, buttons for GO/Back, fader for GM), run-time-only bindings, Desk owner kind, one dispatch point for show listeners, desk roles count for "want input", footer chip green/amber/red with Patch It (named, undoable) and the GO target list, GM pick-up on load/replug/wake, teacher lock, export/import, Show Review "This computer" section | M–L | yes, the core |
| A | **Assign to Control…** (rename, one item per thing): Draw the Desk's window as a list; press selects and fires nothing; owner shown before the press; GO/hit on a fader refused; latching buttons detected; "press or move any control" line in the hub; Companion hint | M | yes |
| R | **Recovery**: release held controls on unplug for every listener; state-dump guard on replug; wake notice ("slept 40 s, finding devices"); repatch warning listing what moves; stop's missing-device line in the hub and Show Review | S–M | yes |
| P | After Feb: counted shared open, `canonical()` key, `Surface=` bindings with universe fallback only for bindings without it, computer-owned line (if decision 1 says so) | M | no |
| T | Surface types (`<Role>`, `<Facts>`, `<Surface>`, init SysEx resent on replug/wake), PMJ table into its `.qxi` (golden test), APC40 mk2, APC mini, X-Touch, drawn map, Make a Surface on the map | L | no |
| S5 | Fader rides and look submasters (decision 8), pick-up hint in the Run tab | M–L | no |
| S6 | Playback slots for desk swaps, Companion as a surface with stable hit ids, print sheet, OSC tablets, backup computer | M each | no |

**About 5 trains to February** (0, F, D, A, R). If time runs short, trim A to the
rename plus press-selects and the fader refusal; never cut F, D's STOP and GM
pick-up, or R's release-on-unplug.

## 7. Test plan

Offscreen gate tests with a fake MIDI/HID plugin line. Names follow existing
`*_test` classes; `deskrecord_test` and `dispatch_test` are new.

**Phase 0**
- `controlmap_test::momentaryReleasedOnDisconnect` (rule 7; guards today's behaviour)
- `controlmap_test::listGoDoubleGoGuard` (20 GOs 1.5 s apart reach cue 20; a press 0.3 s after a GO is held)
- `vcslider_test::pickupAfterPageChange` (page 2 slider waits until the fader passes its value)

**Phase F**
- `controlmap_test::backTargetGoesBack` (Back on the Run tab's list, guard applied)
- `pmjoverlay_test::gmMoveKeepsLearnedLamp` (stop lamp stays 30 after a GM change)
- `controlsurfacespanel_test::pmjRowExactProfile` ("PMJ 9 Faders" reads Not patched)
- `controlsurfacespanel_test::gmSetUpOpensGrandMasterTab`
- `controlsurfacespanel_test::refreshOnDeviceChange`
- `controlsurfacespanel_test::unpatchedPadListed`
- `app_test::stopAllEnabledWithHeldLevel` (VC Level slider on fog; Stop All enabled and clears it)
- `app_test::screenStopSameAsDeskStop` (same lamp state, same footer line)
- `app_test::panicShortcutNotOsReserved` (per-platform table)
- `controlowners_test::padRunButtonRefused` (Cross, L2 refused with the chord named)
- `controlowners_test::padStopChordNeverStarts` (trigger on LB; four-button stop leaves nothing running)
- `helpbrowser_test::controlSurfacesMatchesCode` (no Control Map Back, no PMJ restart)
- `webaccessrun_test::everywhereHitCanBeReleased`
- `webaccessrun_test::refusalShownOnFooter` (Allow GO off, Design)

**Phase D**
- `deskrecord_test::stopFiresInEmptyShow` (zero bindings, feature switch off)
- `deskrecord_test::stopMatrix` (Learn open, Assign open, Design/Operate, teacher lock, modal, during load)
- `deskrecord_test::neverWrittenToShow` (byte-compare save)
- `deskrecord_test::rebindsByIdentityOnRepatch`
- `deskrecord_test::twoSameNameDevicesWarn`
- `deskrecord_test::goTargetsRunTabListAndNamesIt`
- `deskrecord_test::learnRefusesWrongControlKind` (GO on a fader, GM on a button)
- `deskrecord_test::bothStopsLive` (desk stop and show stop on different controls)
- `deskrecord_test::sameControlDeskWins` (show trigger kept, unarmed, listed blocked)
- `dispatch_test::staticControlSkipsShowListeners` (one row per listener: VC button, VC slider, GM input, channel group, speed master, encoder bank)
- `grandmasterslider_test::pickupAfterLoadReplugWake` (pull to 0 always applies)
- `grandmasterslider_test::deviceDumpIgnored` (first 500 ms)
- `controllerjobs_test::midiLocationIdentity` (macOS, Windows "2- " prefix, ALSA)
- `deskrecord_test::teacherLockFreezesRecord` (File > New can't change it)
- `deskrecord_test::exportImportRoundTrip`
- `showreview_test::thisComputerSection`

**Phase A**
- `controllearndialog_test::pressSelectsFiresNothing` (Design and Operate)
- `controllearndialog_test::ownerShownBeforePress`
- `controllearndialog_test::faderRefusedForGoAndHit`
- `controllearndialog_test::latchingButtonSuggestsMomentary`
- `controllearndialog_test::companionHintOnRunClient`
- `contextmenus_test::oneAssignItemPerThing` (menu count unchanged)
- `controlsurfacespanel_test::pressAnyControlNamesOwners`

**Phase R**
- `vcwidget_test::releasedOnDisconnect` (VC flash off after unplug)
- `controlmap_test::stopStateDumpOnReplug` (first 0 then dumps: no ALL OFF; first 127: one ALL OFF)
- `inputdevicenotices_test::namesLostJobs` ("Emergency stop, Grand Master, GO of Main")
- `inputoutputmanager_test::repatchWarnsAndLists` (Cancel leaves the patch)
- `controllerkeepalive_test::wakeNoticeOnce`
- `showreview_test::stopDeviceMissing`

**After Feb (P, T, S5, S6)**
- `inputoutputmap_test::sharedOpenOnePressOnce` (Windows host too)
- `controlmap_test::surfaceBindingResolvesOnOtherPort`; `::surfaceMissingNeverFallsBack`
- `qlcinputprofile_test::pmjRolesGolden`; `::initSysExResentOnReconnect`
- `xmlroundtrip_test::qlcplus4OpensNewShow`
- `playbackaddr_test::showMovesBySlot` (if slots are chosen)
- `fadersubmaster_test::heldUntilZeroAcrossGo` (if decision 8 is B)

**Hardware checklist** (each on macOS; Windows box and Pi where marked)

- **Generic MIDI box (nanoKONTROL-class, unbranded 8+8)**: device name and identity
  on Mac and Windows; survives a port change and replug; buttons momentary or
  latching; fader jitter at rest; GM parked at 0 then first touch with the show at
  100 %; STOP press-release-press and refused on a fader; 20 GOs with none lost
  or doubled; show moved to a fresh user account: stop and GM work or are reported.
- **APC40 mk2**: Ableton-mode SysEx takes, and is resent after replug, hub blip,
  sleep/wake; pad colours for ready/running/held/stopped with "dim" on and off;
  all 8 track faders heard on Omni; Master to GM first touch; STOP key away from
  Play (brush test); Cue level and Tempo knob encoding; 2-hour soak with lamps
  sampled every 10 min.
- **APC mini**: two identical units on one Mac, swapped ports and reboot: which
  holds the desk roles; STOP on a dedicated key, not Shift.
- **PMJ Black 1**: GO and Favorites lamps stay dim when the Master moves; OpenDeck
  levels steady; Go double-press vs the guard; port identity after replug and on
  Windows; GM with Master low at load; layout check so STOP is findable dark.
- **X-Touch Compact / Mini**: layer A/B channel numbers; motor faders follow
  feedback; motor driven on load and page change, never while touched; Mini's
  fader is not motorised; name on macOS vs Windows.
- **Stream Deck via Companion (4.3 and 5.1)**: GO/Back/Stop/ALL OFF held 1 s, with
  Allow GO on/off and a PIN; network pull mid-run: time to show disconnected,
  ALL OFF key look; per-song hit keys through a real song order; volunteer trial
  of 10 cues noting every glance at the laptop; Companion on another machine.
- **Game pad (DualSense BT and USB, Xbox, clone)**: identity across re-pair and
  sleep; trigger on a bumper then four-button stop on the rig: nothing returns;
  learn with thumbs on sticks takes the D-pad; Create+Options flip; two pads in a
  2-hour run with a battery drop; Windows box shows stop-only.
- **Sleep and unplug, every desk**: unplug with a Momentary held, a VC flash held,
  GM mid-travel; same port, other port, through a hub; lid close 10 s and 2 min in
  Operate and Design; record first working press and the footer text.
- **Windows (sandWindows)**: MIDI exclusive open with show patch + desk record;
  panic shortcut; endpoint names "2- APC MINI".
- **Raspberry Pi**: APC mini identity after reboot; STOP fires; GM picks up.
- **Network**: OLA probe universe 15 on the show VLAN confirms ALL OFF and GO
  reach output with triggers also bound (no added latency).

## 8. Decisions for him

Mapped to the board where a question already exists (Q314–Q321); three are new (1, 9, 10).

1. **(new, re-decides Q305) How does the desk reach the program for February?**
   A: a computer-owned line (rev 2; needs the shared open first, about 2 more
   trains, Windows risk). B: through the show's own patch by identity, red chip
   with Patch It when the show lacks it (This Computer's Desk). C: B now, A after
   Feb if touring or the APC journey needs it. *Recommend C.* Safe and cheap for
   February; one click on a foreign show, and the show's own stop covers it.

2. **(Q314) Does a new show still carry its own STOP, GM and GO?**
   A: no, as §5.5 says. B: yes, written as today. C: only the STOP.
   *Recommend B.* Lab resets, the show-day computer and untyped boxes otherwise
   have no stop; it still opens in QLC+ 4.

3. **(Q315, replaces the Q308 prompt) Two stops, and an old show's controls.**
   A: Use Here / Not Here per show per computer (Q308). B: fixed rule: both stops
   live; on the same control the desk wins and the show's binding is listed
   blocked; no prompt. *Recommend B.* An extra ALL OFF beats a missed one, and no
   student answers for the next one.

4. **(Q317) Desk GO: one press with no arm?**
   A: one press, double-GO guard. B: A, plus the chip names the target list and a
   per-computer off tick. C: B, plus the first GO after a Run-tab list change is
   refused unless the new target showed for 2 s. *Recommend B*; C if a two-list
   show is planned. The pad keeps its two-control arm either way.

5. **(Q316, amends Q313) Grand Master after load, replug or wake.**
   A: follows at once. B: waits to pick up; a pull to 0 always applies; device
   dumps ignored. *Recommend B.* A parked fader blacking out the stage on the
   first brush is the likeliest live failure.

6. **(Q319) Who may change this computer's desk at Student level?**
   A: the show PIN (File > New gets round it). B: a teacher lock in Settings, per
   computer. C: anyone. *Recommend B.*

7. **(Q318) Key tables for the first types.** With decision 1 = C, February keys
   are learned on the bench, so the tables are only needed for the after-Feb
   types. Proposed: PMJ GO = Go, Back = Back, STOP = Favorites (O stays Blackout);
   APC40 mk2 GO = Play, STOP = Stop All Clips, Speed 1 = Tap Tempo; APC mini STOP
   on a dedicated key. *Recommend: pick the February keys on the bench now; the
   tables wait.*

8. **(Q321) A fader on a look: ride that drops at GO, or submaster?**
   A: rides drop at GO everywhere (Q298). B: a cue/list ride drops; a production
   or everywhere ride stays (a submaster); a raised fader keeps its look until
   brought to 0. C: submasters stay VC sliders. *Recommend B*, built after Feb;
   VC sliders with pick-up carry February.

9. **(new) Companion and phone ALL OFF behind the show PIN.**
   A: keep the PIN (today; a lockout can block ALL OFF). B: ALL OFF and STOP skip
   the PIN; everything else keeps it. *Recommend B.* "E-stop always works."

10. **(new) Pad buttons as show triggers.**
    A: allowed as today (one press, no arm). B: refused on every pad button until
    an arm+RT chord is designed; RT hits by place stay the pad's show control.
    *Recommend B.* Matches the 2-control arm rule.

11. **(Q320) A backup computer in February hearing the same desk?**
    A: no; backup mode stays later. B: yes; build "listen only" with D.
    *Recommend A unless a backup is planned* (about S).
