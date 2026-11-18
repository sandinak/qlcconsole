# Control surfaces audit and plan before 0.6.0

Read-only audit of `/Users/branson/git/qlcconsole-048` (train/050b, HEAD `0af215303`), 2026-11-17.
Subject: MIDI, OSC and hardware surfaces (faders, encoders, buttons, LEDs). Game pads are context only.
No code was changed and nothing was built.

Already on the board: [TODO.md](/Users/branson/git/qlcconsole-048/docs/project/TODO.md) lines 13-22 ("Control surfaces before 0.6.0, asked 2026-11-17") says this audit is being written and that slices follow from it.

---

## 0. Findings in one table

| # | Finding | Evidence | Severity |
|---|---|---|---|
| F1 | A MIDI/OSC Control Map press ignores arming, mode and the hit's place. Computer keys check `keyMayFire()` (Operate, or Design with the target armed). MIDI/OSC presses go straight to `press()`. A bound hit fires anywhere, at any time, in Design or Operate. The phone page fires only an armed hit, and only in Operate. | `engine/src/controlmap.cpp:486-541` (no mode or arming check), `:544-560` (HitTarget calls `actOnFunction` directly), `:704-707` (`keyMayFire`, keys only); `webaccess/src/webaccessrun.h:99-104` | **High** |
| F2 | A surface has no emergency stop. Control Map targets are Function, CueList, Cue and Hit only, so there is no ALL OFF. Companion has Stop (stop the list) but no ALL OFF, although the Run socket takes `alloff`. The PMJ's `O` is a Blackout *toggle*, which is not a run command. A VC "Stop all" button is QLC+'s `FixtureRest::stopAll`, not `ShowControl::run(AllOff)`. | `engine/src/controlmap.h` TargetType; `tools/companion-module/src/actions.js:17-23` (no alloff) vs `webaccess/src/webaccessrun.cpp:172-174`; `ui/src/pmjoverlay.cpp:369`; `ui/src/virtualconsole/vcbutton.cpp:1760-1763`, `engine/src/fixturerest.cpp:136-148` | **High** |
| F3 | One MIDI press can undo a pad e-stop. After ALL OFF, PadRun is `Stopped` "until armed again or the show runs". A Control Map GO or function press is a fresh operator action: in Operate it plays, and it re-stamps Show-only fixtures. The pad rules need two controls to come back from a stop; a surface needs one press. | `engine/src/padrun.h:325-326`; `engine/src/outputrulekeeper.cpp:354-361` (new session); `engine/src/function.cpp:1291-1331` (operator stamp) | **High** (ask Q1/Q3) |
| F4 | The PMJ overlay is always on and listens by channel number only. It is built at every startup whether or not a PMJ is plugged in. It takes any universe's input whose channel is in its table, then moves its LED universe to that universe. In an Omni patch that means MIDI ch 9: CC 0 is the Grand Master, CC 1-14 are faders and encoders, notes 0-53 are buttons (Blackout, Blind). Any controller sending those on ch 9 drives the PMJ actions. | `ui/src/app.cpp:1984-1985`; `ui/src/pmjoverlay.cpp:337-353` (filter and universe re-bind), `:40-117` (table 32768-32949 = ch 9) | Medium |
| F5 | The PMJ overlay is stale against Build-era rules. Its faders and encoders edit palettes **in Operate**, with **no undo step** (the Encoder bank refuses in Operate and takes one undo step per gesture). Go/Back/Left/Right do nothing (`Transport: break`) and stay dark. It has no help page and no test of its own (one Blind toggle in appviews_test). | `ui/src/pmjoverlay.cpp:378-412,465-474,511,561`; `engine/src/programmercontroller.cpp:4613-4710` (no mode or undo); `engine/src/encoderbank.cpp:496-507`; `ui/test/appviews/appviews_test.cpp:2566-2577` | Medium |
| F6 | Several consumers can hear one control, and nothing warns. VC widgets, the Control Map, the Encoder bank, Speed master Inputs…, the PMJ overlay, the footer GM input and effect scripts each connect to `inputValueChanged` on their own. Show Review VC-06 ("same input twice") checks VC widgets only. Example: the PMJ's Enc 1-4 (CC 11-14 on ch 9), set up as an Encoder bank, would nudge a palette through PMJOverlay *and* turn the selection through the Encoder bank. | `grep inputValueChanged`: `engine/src/{controlmap,encoderbank,effectscriptrunner,programmercontroller}.cpp`, `engine/src/speedmasters.h:166`, `ui/src/{pmjoverlay,grandmasterslider}.cpp`, VC; `engine/src/showreviewvcchecks.cpp:298-330` | Medium |
| F7 | "One status source" does not reach surface LEDs. A Control Map cue-list LED is lit while the list's chaser runs. It never shows standby, a double-GO hold, the e-stop (`Stopped`) or Pad ARMED. The control-surface engine has a single `setStateProvider()`, and PMJOverlay owns it, so a second overlay would overwrite it. | `engine/src/controlmap.cpp:801-821`; `engine/src/controlsurfaceengine.h:50-51`; `ui/src/pmjoverlay.cpp:310` | Medium |
| F8 | Windows (Q257) treats surfaces differently from pads. Windows refuses the pad's arm, MSC cue commands and MIDI timecode in. Control Map MIDI/OSC GO, the PMJ overlay, the Encoder bank, VC inputs and OSC show control all work on Windows, and none was checked there. | `engine/src/padrun.cpp:631-655`; `engine/src/showcontrol.cpp:781`; `engine/src/doc.cpp:243`; no `padRunSupported` in controlmap/encoderbank/pmjoverlay | Low (decide, Q7) |
| F9 | No MIDI/OSC surface has been verified on hardware since the Build era. The Control Map (RIG_TEST_PLAN §L) is unticked. The encoder presets are "none checked against a unit". The Stream Deck + has never run in a real Companion. The PMJ §H checks were "each confirmed once, in isolation", and the whole set has never been run in one sitting. | `docs/dev/RIG_TEST_PLAN.md:204-220,120-139,232-235`; `engine/src/encoderbank.cpp:62-65`; `CHANGELOG.md:373-378,2764-2768` | Process |
| F10 | Faders reach a group master, speed master or the GM only through a VC slider or the per-master Inputs… dialog. The Control Map takes presses only. Speed masters send no beat LED back. | TODO.md:1436-1438; `engine/src/speedmasters.h:155-167` (no feedback in speedmasters.cpp) | Gap |
| F11 | Surfaces have no Test Controller. Rule 7 ("a controller's response is testable") is met for pads only: Test Controller and Jobs… appears only on game-controller slot devices. | `ui/src/connectionstree.cpp:3100-3120`; `ui/src/controllertestdialog.h` | Gap |
| F12 | Help is scattered. The Control Map is in other-improvements.md §"Control Map", the Encoder bank in build.md, the Stream Deck in show-control.md, and the PMJ is nowhere. No page is called "control surfaces". | `docs/features/*.md` | Findability |

---

## 1. Inventory of what exists today

### 1.1 Capabilities

| Capability | What it does | Where the user finds it | Designed for / tested on | Built | Hardware-verified? | Wired to Show Tree / Run / One GO path / guard / Show-only / e-stop / one status source |
|---|---|---|---|---|---|---|
| **Control Map** (MIDI note/CC, OSC, computer key → target) | Fires a function, GOes a Show Tree list, GOes one cue (sub-cue aware), fires a hit. Modes: Toggle, Momentary, Start only, Stop only (on lists: GO, GO while held, Stop). LED follows "running". Saved with the show. | Right-click → **Learn Trigger…** on a Show Tree list, cue or hit, a Build navigator row, a Workbench layer or a Build ▸ Effects row. The list is at Connections ▸ Devices ▸ Show control & sync ▸ Control Map. | Generic; stub plugin only | 2026-10-29 (DONE.md:650-690); keys 0.4.2 | **No.** RIG_TEST_PLAN §L blank; TODO.md:2396-2400 | Show Tree: yes (targets). One GO path: lists and cues yes (`controlmap.cpp:663`, `RouteControlMap`), so the double-GO guard applies. **Functions and hits bypass `run()`** (`:599-615`). Arming and Operate: keys only (F1). Show-only: a MIDI press is an operator stamp; held in Design, live in Operate. E-stop: no target (F2). Status: LED = running only (F7). |
| **Encoder bank** (in the Control Map) | Eight relative encoders × five banks (Intensity, Colour, Position, Beam, Gobo/prism) turn Build's selection or the opened look. Push held = fine, tap = home. Rings and bank lamps. One undo step per gesture. **Refuses in Operate.** | Connections ▸ Devices ▸ Show control & sync ▸ Control Map ▸ Encoder bank (Learn more…). The Workbench must be showing. | Presets: X-Touch Mini, Midi Fighter Twister, APC40 knobs, three generic | 2026-11-01 (enc), enc2 | **No.** Presets "none checked against a unit" (`encoderbank.cpp:62-65`); CHANGELOG known issue | Attached to Build/Workbench. Safe: Operate refuses (`encoderbank.cpp:496-507`). Show Tree layer rows refuse ("made from its looks"). |
| **Stream Deck / Companion module** | GO, Back, Stop, Sub GO/Back, Hold/Resume, GO cue N, Move standby with "GO if armed", hit fire/release, Blackout, GM / group master set, nudge and toggle; now/next variables and feedbacks. | `tools/companion-module/`, loaded from Companion's Developer modules path; web access with a PIN. Help: show-control.md "Stream Deck (Companion)". | Stream Deck + (dials, touch strip); node tests with a mock server | 2026-11-01 (sd) | **No** (CHANGELOG:373-378; TODO.md:1302). Not published. | Everything goes through the Run socket and `ShowControl::run(RouteWeb)`: remote GO + PIN (Q256), hits only when armed and in Operate. **No ALL OFF action** (F2). |
| **Phone/Deck Run page** (the Run socket) | Same commands; masters, GM, Blind/Solo in Design, `alloff` always allowed (PIN only). | Web access | Phones, Steam Deck | 0.4.x | Steam Deck partly (CHANGELOG:373) | Fully on the One GO path, Q256 rules, `PadRun::playedList()`. **Reference model for surfaces.** |
| **OSC / MSC show control (QLab path)** | `/go`, `/back`, `/subgo`, `/stop`, `/alloff` or `/panic`, `/cue/N/go`…; MSC GO, ALL OFF… | Connections ▸ Devices ▸ Show control & sync. OSC in is **off by default** (`showcontrollistener.cpp:54-58`), bound to Any (`:163`). | QLab | 0.4 | QLab OSC/MSC partly on the bench | One GO path (`RouteOsc`/`RouteMsc`). ALL OFF is never held. On Windows MSC GO is refused, MSC ALL OFF/Stop/Pause run (`showcontrol.cpp:781`). |
| **Speed master Inputs…** | Tap, Rate (fader 0-200 %) and Pause per master, from any input | Footer speed masters ▸ Inputs… (effects.md:746) | Generic | 2026-10-12 | No | Independent consumer. No feedback (beat LED). Not in the Control Map list. |
| **Virtual Console external input** (stock QLC+ 4) | Every widget: button, slider (Level, Playback, Submaster, GM, Speed master), Cue List, Speed Dial, XY pad, frame pages (next/prev input), Solo frame. Slider **catch values** (soft takeover). Fork widgets: Cue Runner (`RouteVcRunner`), Show Control, VC effect controls. | Virtual Console tab (in every view, including the qlcconsole view: `app.cpp:5709-5718`) | Any profile | Stock + fork | Stock; fork widgets offscreen | VC Cue List and Cue Runner go through `run()` (`RouteVcCueList`/`RouteVcRunner`); a plain chaser uses `admitDirect()`. Buttons start functions directly. Show-only follows the same operator stamp. VC-06 catches the same input twice *among VC widgets only*. |
| **VC "New Programmer Frame from Input Profile…"** (ProgrammerMap) | Generates a VC frame of parameter sliders, select buttons, pad-mode buttons and chaser step buttons from a `.qxpm` | Virtual Console ▸ Add menu (`virtualconsole.cpp:395-400,590`) | APC40 mk2 only (`resources/programmermaps/Akai-APC40-mkII.qxpm`) | **2026-05** (programmer-mode era, `b62773023`) | Was used on the APC40 in May | **Stale.** Built for Doc::PadMode and the programmer-mode Save flow, before Build and the Show Tree. No help page found. |
| **Control-surface engine + PMJ overlay** | Roles/pages/LED-state engine (P0) and a hard-coded PMJ Black 1 overlay (P1 slices 1-8): Master → GM, `O` → Blackout toggle, `Set` → Blind toggle, Enc 1/2 → nudge the focused Pan/tilt look, faders 1-6/1 → the focused Colour/Dimmer look, N-Up → zero, Select/Load 1-10 → programmer selection of the Workbench's scene, faders 1-6 → live intensity of the selected fixtures. Go/Back/Left/Right/Page/Favorites/N-Down: nothing. | Automatic: no UI, no help | PMJ Black 1 (OpenDeck) | 2026-08-18 (DONE.md:11278-11660) | **Partly.** Enc 1/2 direction "confirmed working on the real PMJ" (DONE.md:11441); §H never run as a set (RIG_TEST_PLAN:232-235). | **Not** on the One GO path (transport unwired). **Not** Operate-aware (F5). No undo. No e-stop. One-button Blackout toggle. LED state is its own (F7). Hears any universe (F4). |
| **Input profiles** | 47 bundled (`resources/inputprofiles/`), plus New/Delete beside Edit (restored, CHANGELOG:2208-2211). LED init templates in `resources/miditemplates/` (PMJ-Black-1-idle, APC40MK2, APC40, APCmini(MK2), APC20, Launchpad MK2/Mini MK3). | Connections ▸ a universe's patch editor | n/a | stock + fork | n/a | Used by VC feedback and the Control Map's feedback parameters. |
| **Effect scripts reading MIDI** | Note-reactive effects ("midi" data channel); Look Editor ▸ Learn range for a keyboard range | Look Editor on a note effect | MIDI keyboard | 2026-08 | RIG_TEST_PLAN A3-A5 | A look-scoped input that already exists (relevant to §5) |

### 1.2 Profiles and presets in the repo, per device he named

| Device | Input profile | LED init | Encoder bank preset | Other | State |
|---|---|---|---|---|---|
| PMJ Black 1 (OpenDeck) | `PMJ-Black-1.qxi` (69 ch: 11 sliders, 4 encoders, 54 buttons with feedback 15-127) | `miditemplates/PMJ-Black-1-idle.qxm` | **none** (its Enc 1-4 are CC 11-14 on ch 9, two's complement: a 4-encoder preset is a one-liner) | PMJOverlay hard-codes it | Input line must be "any MIDI channel" (CONTROL_SURFACE_DESIGN P1 plan) |
| Akai APC40 mk2 | `Akai-APC40-mkII.qxi` | `APC40MK2.qxm` | "Akai APC40 (knobs)": CC 16-23 absolute, rings CC 24-31, notes 58/59 step. It does **not** say mk1 or mk2, and none of it was checked. | `programmermaps/Akai-APC40-mkII.qxpm` (May, stale) | |
| Behringer X-Touch Mini | **none** (only X-Touch Compact and Extender) | none | "Behringer X-Touch Mini": standard mode, layer A, Relative 1; encoders CC 1-8, pushes notes 0-7, banks notes 8-12, step 13/14 | | Control Map names its controls "Note n / CC n" with no profile |
| Midi Fighter Twister | none | none | "twister" (Omni patch, ch 1/2/4) | | |
| Stream Deck + | n/a (not MIDI) | n/a | n/a | Companion module | Needs Companion + web access |
| TouchOSC | `TouchOSC-Automat5.qxi`, `TouchOSC-Mix16.qxi` (stock) | | | | |

### 1.3 Stale against the 0.4/0.5 rules: summary

| Rule (source) | Control Map | Encoder bank | PMJ overlay | Companion | VC input | Programmer Frame |
|---|---|---|---|---|---|---|
| One GO path, `ShowControl::run()` (RUN_COMMANDS.md) | lists/cues ✅, functions/hits ✗ | n/a | ✗ (transport unwired) | ✅ | Cue List/Runner ✅, buttons ✗ (stock) | ✗ (chaser step direct) |
| Double-GO guard (`showcontrol.h:150-200`) | ✅ lists/cues | n/a | n/a | ✅ | ✅ (`admitDirect`) | ? |
| Hits fire only at a live place, in Operate (Q154, web rule) | ✗ (F1) | n/a | n/a | ✅ | ✅ (`armedHit`) | n/a |
| Edits refused in Operate | n/a | ✅ | ✗ (F5) | ✅ (Design-only words) | stock | ✗ |
| Show only: no Show-only fixture without a GO in Operate | held in Design ✅; one press in Operate fires it | ✅ | programmer fader holds Show-only ✅ | ✅ | same as Control Map | ? |
| E-stop always reachable, never held, never suppressed | no stop target ✗; one press restarts (F3) | n/a | Blackout toggle only ✗ | no `alloff` ✗ | Stop All ≠ ALL OFF ✗ | ✗ |
| One status source drives every surface (pad rule 2) | LED = running ✗ | rings = values ✅ | own state ✗ | reads the Run socket ✅ | stock ✗ | ✗ |
| Help + tests + CHANGELOG (STRATEGY guardrail 2) | ✅ | ✅ | ✗ | ✅ | stock | ✗ |

---

## 2. What an operator needs, and the gaps

### 2.1 Needs against what we have

| Operator need (show choir / school theatre, 2026) | Covered today by | Real gap? |
|---|---|---|
| **GO / Back on a hardware button, with feedback** | Control Map GO on a list (guarded); Companion GO/Back; pad; Run tab | **Partly a gap.** The LED says "list running", not "standby ready / held / stopped" (F7). Back is not a Control Map target; a cue-list binding can only GO or Stop. |
| **Cue list and hit pages** (a bank of hits per song) | Control Map per hit (one control, one hit); VC buttons hold several hits and fire the narrowest armed one (Q157); Companion hit slots | **Gap for MIDI.** No paging. The narrowest-armed rule exists for VC buttons and not for Control Map buttons (F1, §5). |
| **Grand Master fader** | Footer GM's own input (stock); VC slider in GM mode; PMJ Master; Companion GM | Covered (VC / footer input). Nothing in the Control Map list. |
| **Sub-masters / faders on looks or layers** | VC sliders (Submaster/Playback); group masters by VC slider only (TODO.md:1436) | **Gap:** no fader target on the Control Map (CONTROLMAP.md Phase 3 never built); no layer or look fader in the Show Tree. |
| **Speed / rate masters** | Speed master Inputs… (Tap, Rate, Pause); VC speed slider/dial | Covered for input. **Gap:** no beat LED (SPEED_SPREAD S4 backlogged "until Branson is at the surface"). |
| **Selection + encoders for Build / Refine** | Encoder bank (Design); PMJ Enc 1/2; pads for Focus/Refine | Covered **if** the presets are right (F9). The PMJ is not a preset. Refine is pads only. |
| **LED feedback** | Control Map running LED; Encoder bank rings and lamps; VC feedback; PMJ | Partly (F7). |
| **Bank / page switching** | Encoder bank banks; VC frame pages (manual) | **Gap** for Control Map buttons and faders. This is the scoped-assignment ask (§5). |
| **A safe e-stop** | Pad 4-control chord, phone/Deck `alloff`, OSC `/alloff`, MSC ALL OFF, a key? | **Gap** for MIDI buttons and Stream Deck keys (F2). |
| **Knowing which control does what** | Control Map table; VC-06 for VC | **Gap** (F6, F11). |

### 2.2 How other consoles handle surfaces

From general knowledge of those consoles; not checked against their current manuals.

| | Eos | grandMA3 | ChamSys MagicQ | QLC+ 4 | qlcconsole today |
|---|---|---|---|---|---|
| GO/Back | Dedicated hardware GO / Stop-Back, per playback | Executor buttons, master GO/Back | Playback GO/Back + main GO | VC Cue List next/prev input | Control Map GO, Companion, pad, Run tab |
| Faders | Fader wings; fader pages; per-page fader configuration (master, sub, playback) | Executor pages; faders bound to sequences/masters | Playback pages | VC sliders; frame pages | VC sliders, footer GM |
| Encoders | Category-paged encoders (Intensity/Focus/Colour/Image/Form/Shutter) | Encoder bar pages per feature | Encoder wheels per attribute | VC knob sliders | **Encoder bank** (same category-paged idea) |
| Generic MIDI/OSC | OSC API; MIDI via Show Control "MIDI Raw" → macros | MIDI remotes and DMX remotes → executors/commands | MIDI notes/CCs mapped to playbacks and keys | Per-widget input profile | Control Map, VC, Encoder bank, Speed master inputs |
| Paging tied to the show | Macros fired from cues can change the fader page | Cue command field can run `Page n` | Cue stack macros can change the page | Manual frame pages | None (VC pages manual) |
| Fader takeover on a page change | Motor faders on big desks; non-motor faders must be picked up | Motor faders; pick-up on non-motor wings | Optional fader matching | Slider **catch values** | VC slider catch only |
| E-stop | No single e-stop; Release/Off commands | `Off` / `Kill` commands | Release all | Stop all functions | Pad chord, web/OSC/MSC ALL OFF |

What qlcconsole already has that these don't: one run path with a double-GO guard across every route, Show-only output rules, a hit scope model (cue > list > production > global), and remote rules (Q256). Surfaces should plug into those, not copy desk paging wholesale.

---

## 3. Safety review

### 3.1 What a MIDI/OSC surface can do that the pad rules forbid

| Pad rule (memory `feedback_operator-control-rules`; CONTROLLER_JOBS invariants) | MIDI/OSC surface today | Verdict |
|---|---|---|
| 1. Never change cue lists from the controller | A Control Map binding GOes a *named* list (the one it was learned on), not the Run tab's played list. It cannot switch the played list. | OK (not the same thing) |
| 2. One status source on every surface | A surface LED shows "chaser running" (`controlmap.cpp:801-821`). PMJ LEDs show their own state. Neither shows ARMED, Stopped or held. | **Violates** (F7) |
| 3. Arm/disarm takes two controls | Surfaces have no arm: one press GOes, in Design too (`controlmap.cpp:544-576`, no mode check) | Different by design; needs a decision (Q1) |
| 4. One safety (LT) for triggering cues | None on a surface | Q1 |
| 5. E-stop always works, from any controller | No e-stop target on MIDI or Companion (F2) | **Gap** |
| After an e-stop, coming back takes the arm (two controls) | One MIDI GO or function press restarts the show and re-stamps Show-only fixtures (F3) | **Violates the intent** |
| Hits: only the armed hit at the narrowest live place (Q154/Q157; web: Operate only) | A MIDI-bound hit fires any time and anywhere (`controlmap.cpp:549-552`) | **Violates** (F1) |
| Show only: nothing moves a Show-only fixture without a GO in Operate | In Design, held (OutputRuleKeeper `HeldByDesign`, `outputrulekeeper.cpp:431`). In Operate, a MIDI press on a function or hit bound to a laser look fires it at once, with no arm. A MIDI press counts as an operator action (`function.cpp:1325-1331`). | Within the letter of the Show-only rule, which says Operate plus an operator action. It is looser than the pad (arm + LT + RT) and the web (remote GO + PIN + armed place). |
| Double-GO guard on every route | ✅ Control Map lists/cues (`RouteControlMap`), OSC, MSC, Companion, VC Cue List. ✗ PMJ Go (unwired, so nothing to guard). Function targets on a chaser are started directly, not through `run()`: verify whether a Show Tree list's chaser can be learned as a FunctionTarget (Build rows). | Mostly OK; one item to verify |
| No edit in Operate (Encoder bank's rule) | PMJ faders and encoders edit palettes in Operate, no undo (F5) | **Violates** |
| Unknown device must not drive the show | PMJ overlay takes any device's ch 9 messages (F4) | **Violates** |
| OSC exposure | QLab listener off by default ✅, binds Any, no PIN (QLab convention). OSC through the OSC *plugin* into the Control Map: any sender on the patched port. | Document it; not a 0.5.x blocker |

### 3.2 Windows after Q257

| Path | Windows today | Same as pads? |
|---|---|---|
| Pad arm, Pad GO | Refused (`padrun.cpp:631-655,668-672`) | n/a |
| Pad e-stop | Works | n/a |
| MSC GO / MSC ALL OFF | GO refused, ALL OFF/Stop/Pause run (`showcontrol.cpp:781`) | Like pads |
| MIDI timecode in | Refused (`doc.cpp:243`) | Like pads |
| **Control Map MIDI note/CC GO, functions, hits** | **Works** | **No** |
| Encoder bank, PMJ overlay, Speed master inputs, VC inputs | Work | No |
| OSC (QLab listener, Control Map OSC) | Works | n/a (not MIDI) |

Q257 was about the HID plugin's hotplug and the SysEx input path. Note/CC input on the Windows MIDI plugin is a different path, so refusing it is not obviously required, but **nothing on it was checked on Windows** (TEST_MAP M1249 covers the pad, MSC and MTC only). See Q7.

---

## 4. Ranked plan for 0.5.x (before 0.6.0)

ROI = Value × Fit ÷ Effort (days) × risk (L 1.0, M 0.8), as in STRATEGY §3. Every slice attaches to an existing model: the Control Map (Connections ▸ Show control & sync), the Show Tree, Show Review, the Run socket, or the help. **No new tab, no new menu item.** Each slice needs a CHANGELOG line, a help update and a RIG_TEST_PLAN entry (guardrail 5).

| Rank | Slice | Attaches to | Value | Fit | Effort | Risk | ROI | Tests (sweeps, must fail on today's code) |
|---|---|---|---|---|---|---|---|---|
| 1 | **CS1 Surface safety floor.** (a) A Control Map MIDI/OSC press follows the hit rule: a hit fires only when its place is live, in Operate; in Design only when armed (`isArmed()`, as keys). Cue/list targets in Design follow `isArmed()` (Q2). (b) New Control Map target **Emergency stop (ALL OFF)** → `ShowControl::run(AllOff, RouteControlMap)`: never toggles, never held, learnable only on a Button-type control, and the learn asks for a second press to confirm. (c) PMJOverlay hears only universes whose input profile is "PMJ Black 1" (it already has `findKnownUniverse()`), never re-binds to another, and refuses palette edits in Operate with the Encoder bank's words. (d) Companion: an **ALL OFF** action and preset (hold 1 s), sent as `alloff`. | Control Map, Run socket | 5 | 5 | 2 | L | **12.5** | controlmap_test: sweep every target type × {Design, Operate} × {place live, not live, selected}; ALL OFF target sweep (held window, toggle, a fader at 0/127, after a GO within 1 s); appviews/pmj: a non-PMJ universe sending ch 9 CC 0 leaves the GM alone; PMJ fader in Operate changes no palette; companion node test for `alloff` |
| 2 | **CS2 Help: one "Control surfaces" page** (docs/features/control-surfaces.md, in help.qrc + kTopics): what hears a control, the Control Map, the Encoder bank, VC input, Speed master inputs, Companion, the safety rules above, Windows status, per-device setup | Help | 4 | 5 | 0.5 | L | **40** (do it with CS1) | helpbrowser_test |
| 3 | **CS3 Run lamp from the one status source.** A Control Map GO/list binding's LED: dim = a cue on standby, bright = a cue fading in or playing, two blinks = a GO held by the guard (`ShowControl::commandHeld`), off after ALL OFF until the next GO. The e-stop binding is lit after a stop. Read from ShowControl and `PadRun::state()`, never derived on its own. | Control Map feedback | 4 | 5 | 1.5 | L | **13** | controlmap_test: feedback values sent per state (stub plugin), every state × binding mode; held-GO blink; resend on reconnect |
| 4 | **CS4 Who hears this control.** (a) Show Review VC-06 extended to Control Map, Encoder bank, Speed master inputs and PMJ (one input, two consumers = a warning naming both). (b) Learn Trigger… says "CC 11 is also Encoder bank 1". (c) Connections ▸ right-click a MIDI/OSC input universe → **Test Surface…**: a log of raw input → which consumer took it (the pad's Test Controller for surfaces, rule 7). | Show Review, Learn dialog, Connections context menu (no new menu-bar item) | 4 | 5 | 2.5 | L | **8** | showreview_test clash sweep over every consumer pair; controlmapui learn-clash text; a test-surface dialog test with the stub plugin |
| 5 | **CS5 Encoder presets made true**, at his bench: fix X-Touch Mini/APC40 numbers from a recorded trace; add a **PMJ Black 1** preset (4 encoders, CC 11-14 ch 9 Omni = 32779-32782, two's complement); add an X-Touch Mini input profile from the trace | Encoder bank | 3 | 4 | 1 | L | **12** (hardware-gated) | encoderbank_test per preset: decoded detents from recorded bytes |
| 6 | **CS6 Faders on the Control Map** (CONTROLMAP.md Phase 3): GM, group master, speed master Rate (and Tap/Pause listed here too), with **catch** (soft takeover, as VC slider catch values) and the speed master beat on a learned lamp (S4's LED part) | Control Map; group/speed masters | 4 | 4 | 3 | L | **5.3** | controlmap_test: catch sweep (fader above/below/at the level, page change mid-move); playing, not editing (no modified flag); beat lamp timing at the tick |
| 7 | **CS7 Scoped assignments, slice 1** (§5): buttons only, place = Show / Production / Cue list / Song section / Cue, narrowest live wins | Control Map + Show Tree places (HitScope) | 4 | 5 | 3 | M | **5.3** | sweep: every place × live/not × Design/Operate; GO/Back/e-stop never shadowed; a held press releases to what it pressed after a scope change |
| 8 | **CS8 PMJ onto the Control Map** (Q4): replace PMJOverlay + ControlSurfaceEngine (~800 lines, no help) with a "PMJ Black 1" set of default bindings (Master → GM fader, Go/Back → GO/Back of the Run tab's list, a chosen button → ALL OFF) + the CS5 encoder preset + its LED init template. Remove the overlay. | Control Map, Encoder bank | 3 | 4 | 2.5 | M | **3.8** | the CS1/CS3 sweeps run on the PMJ table; `controlsurface_test` retired |
| 9 | **CS9 Windows surfaces** (Q7): a Win10 VM bench check of a USB MIDI controller (Control Map GO, LEDs, replug), then either a note in the help or a refusal like Q257 | — | 2 | 4 | 0.5 | L | **16** (bench-gated) | M1249-style test if refused |
| 10 | **CS10 Retire the VC Programmer Frame wizard** or re-test it (May 2026, programmer-mode era): one menu item fewer if retired | VC Add menu | 2 | 4 | 0.5 | L | **16** | none, or VC test of the frame load |

**Suggested order:** CS1 + CS2 → CS3 → CS4 → (bench day: CS5, CS9, the checklists below) → CS6 → CS7 → CS8, CS10. CS1-CS4 are about 7 days and need no hardware to build, but each needs a bench pass before it is called fixed (feedback_validate-real-path). The ROI column ranks CS2, CS9 and CS10 higher, but CS9 waits on a bench day and CS10 on Q10.

### 4.1 Per-device hands-on checklists

Before the bench, tell me which of these he still owns (Q8). Record each run as a trace in the Control Map's "last fired" plus a raw MIDI log, so presets and profiles can be fixed without the device in hand.

**PMJ Black 1** (profile ✅, LED template ✅, overlay ✅, encoder preset ✗)

| # | Step | Expect today | Note |
|---|---|---|---|
| P1 | Patch as input on "any MIDI channel" + feedback line, profile PMJ Black 1; restart | `O`/`Set` dim, the rest dark (RIG H1) | |
| P2 | Run RIG_TEST_PLAN §H1-H15 in one sitting | as written | never done as a set |
| P3 | Control Map: Learn Trigger on the main cue list → press `Go`; twice within 1 s | GO; second "Second GO held" on the Run tab; `Go` LED lit while the list runs | `Go` does nothing via the overlay |
| P4 | Learn Trigger on a hit → press it in Design with the hit's cue not live | **Fires today** (F1); after CS1: refused | |
| P5 | Pad e-stop, then press the learned `Go` | **Show runs again on one press** (F3) | decide Q1/Q3 |
| P6 | Switch to Operate, move faders 1-6 with a Colour look open | **Palette changes, no undo** (F5); after CS1: refused | |
| P7 | Encoder bank, generic two's complement, first encoder 32779, 4 encoders | **Both** the bank and the overlay act on Enc 1/2 (F6) | |
| P8 | Unplug/replug mid-session | LEDs come back (H15) | |

**Akai APC40 mk2** (profile ✅, LED template ✅, encoder preset "APC40 (knobs)" unverified, Programmer Frame map May 2026)

| # | Step | Expect |
|---|---|---|
| A1 | Patch with Akai-APC40-mkII.qxi + feedback; send APC40MK2.qxm if the pads stay dark | pads light idle |
| A2 | Encoder bank ▸ preset APC40 (knobs): with a Workbench selection turn the 8 Device knobs in each bank; Device ◀ ▶ step banks | values move; rings follow; at a knob's end the turn stops (absolute). Record CC numbers if not 16-23 / rings 24-31 / notes 58-59 |
| A3 | Learn a clip pad on a cue, a hit, a scene (RIG L1-L8) | fires; LED on while running; replug restores |
| A4 | VC slider (Submaster) on track fader 1 with catch values; master fader → VC GM slider | no jump until caught |
| A5 | VC ▸ Add ▸ New Programmer Frame from Input Profile… | record whether it still builds and works (for CS10) |

**Behringer X-Touch Mini** (no profile, preset "X-Touch Mini" unverified)

| # | Step | Expect |
|---|---|---|
| X1 | X-Touch Editor: Standard mode, layer A, encoders Relative 1; note the global MIDI channel | |
| X2 | Patch (Omni), Encoder bank ▸ preset X-Touch Mini; Workbench selection; turn each encoder in all five banks | values move one detent per click; rings follow; push-hold = fine, tap = home; top-row buttons pick banks, lamp shows which; 13/14 step |
| X3 | Switch to Operate, turn | "Encoders are off in Operate -- switch to Design to edit." |
| X4 | Learn a bottom-row button on a hit; LED | fires; LED follows |
| X5 | The fader on a VC GM slider | GM follows |
| X6 | Save a raw log of every control | builds the missing input profile (CS5) |

**Stream Deck + with Companion** (module ✅, unpublished; the scratchpad checklist from 2026-11-01 is the base)

| # | Step | Expect |
|---|---|---|
| S1 | Web access on, PIN set; module from the Developer modules path; connect | variables show now/next |
| S2 | Remote GO **off**: press GO | refused, "remote GO is off" |
| S3 | Remote GO on: GO, GO twice fast, Back, Sub GO | GO; second held; same text as the Run tab |
| S4 | Dials: GM nudge, group master nudge/press, Move standby + "GO if armed" | GM/master move; GO only within the arm window |
| S5 | Hit key in Design, then Operate with the hit's place live/not live | fires only Operate + live |
| S6 | Wrong PIN 5× | locked 30 s |
| S7 | Look for an ALL OFF key | **none today** (F2); after CS1d: hold 1 s → everything stops, even with remote GO off |

**OSC (TouchOSC or QLab)**: RIG L9 plus `/go`, `/alloff` with the listener on, and confirm nothing is heard with it off (the default).

### 4.2 Owner questions (recommended answers)

| Q | Question | Recommended |
|---|---|---|
| Q1 | Does a MIDI/Stream Deck GO need an arm like the pad (two controls, a safety)? | **No**, for a dedicated GO button on a desk surface, as on every console: the double-GO guard plus the CS3 lamp cover it. **But** hits and functions from a surface fire only in Operate at a live place (CS1a). Revisit if a surface is ever hand-held. |
| Q2 | In Design, should a MIDI/OSC press follow the computer-key rule (fires only what is armed: the Run tab's list or the Show Tree selection)? | **Yes for cue lists, cues and hits; no for scenes and effects** (they keep firing in Design so Build can try them; Show-only is held in Design anyway). |
| Q3 | E-stop on a surface: one learned button? | **Yes, one Button-type control**, learned with a confirming second press. ALL OFF through `run()`, never held. Companion's key needs a 1 s hold. Not on faders or encoders. |
| Q4 | PMJ: retire PMJOverlay + ControlSurfaceEngine onto Control Map defaults + an Encoder bank preset (CS8), or finish P2 (transport, pages) in the overlay? | **Retire onto the Control Map.** One input path to keep safe, help already exists, and APC40 and X-Touch get the same treatment. |
| Q5 | Should run controls (GO, Back, ALL OFF, GM) be saved per **computer** (like controller jobs) rather than per show, so any show opened on this desk has its GO button? | **Yes:** a computer layer under the show's bindings, the widest scope in §5. |
| Q6 | Fader takeover after a page/scope change: catch (pick up), or jump? | **Catch**, as VC slider catch values: the fader does nothing until it crosses the target's level; its lamp blinks and the footer names it until caught. |
| Q7 | Windows: leave Control Map MIDI GO allowed? | **Yes, allowed**, with a "checked / not yet checked" line in the help after one Win10 bench run (CS9). Note/CC does not go through the SysEx path Q257 was about. |
| Q8 | Which surfaces do you still own: PMJ Black 1, APC40 mk2, X-Touch Mini, Midi Fighter Twister, Stream Deck + (arrived?) | — (needed to schedule CS5 and the bench) |
| Q9 | Publish the Companion module (npm / Bitfocus store)? | **After** CS1d and one S1-S7 pass on the real Stream Deck +. |
| Q10 | Retire VC ▸ "New Programmer Frame from Input Profile…" (May 2026, programmer mode)? | **Retire** unless A5 shows it still earns its menu item (menu count falls by one). |

---

## 5. Scoped control assignments

Owner, verbatim: "with surfaces .. we will want to be able to assign controls to different settings that can be tied into a layer .. a cue .. a song .. a show .. etc."

### 5.1 What exists toward it

| Piece | What it already does | Scope today |
|---|---|---|
| **Hit places** (Q154/Q157) | `ShowTree::HitScope { Global, Production, List, Cue }` (`engine/src/showtreemodel.h:170-178`). `isHitPlaceLive()`, `armedHit()`: the narrowest armed place wins (cue > list > production > global), worked out per GO, cue change, list start/stop and production switch, never per frame (`engine/src/showtreecontroller.h:1125-1170`). A VC button holding several hits fires the narrowest armed one. | **This is the resolver to reuse.** |
| Control Map | One control, one target, saved with the show; `isArmed()` already decides "live" from the Run tab's list and the Show Tree selection (`controlmap.cpp:744-797`) | Show-wide only |
| Encoder bank | Banks (Intensity…Gobo) follow the Build selection; refuses on baked layer rows | Selection-scoped (Design) |
| VC external input | Per widget; frames have manual pages; hidden pages of a multi-page frame | Page-scoped, manual |
| Song sections (Q190) | Numbering scheme → a section per song in a cue list (`showtreemodel.h:186-205`) | A place that exists in the model |
| Cue layers | Layers of a cue (baked into its collection; Solo per layer) | A place in Design; baked in Operate |
| Controller jobs (Q250) | Per pad, per **computer** (QSettings), not the show | Computer-scoped |
| ControllerScheme | One verb map across Focus/Refine tools (`engine/src/controllerscheme.h`) | Tool-scoped (pads) |
| Effect "Learn range" | A note range bound inside a look's effect | Look-scoped input |
| Venues | Venue sidecar data (house fixtures) | Venue scope exists for data |

### 5.2 How others do it

Again from general knowledge, not checked against current manuals.

| Console | Mechanism | Lesson |
|---|---|---|
| Eos | Fader pages hold the fader configuration; a macro run from a cue can change the page | Paging follows the show only if someone programs a macro per cue |
| grandMA3 | Executor pages; a cue's command field can run `Page n`; motor faders follow the page | The same, but the change is in the cue data; motor faders solve takeover |
| ChamSys MagicQ | Playback pages; cue stack macros change pages; optional fader matching | Takeover is an option because most wings have no motors |
| QLC+ 4 | VC frame pages, manual; slider catch values | No link to the show at all |

None of them resolve scope **implicitly** from where the show is. qlcconsole can, because the hit-place resolver already does exactly that for buttons. That is simpler for students: no page macros to program.

### 5.3 Model options

| Question | Option A: explicit pages (MA/Eos style) | **Option B: places, narrowest live wins (recommended)** | Option C: per-cue copies |
|---|---|---|---|
| Stored where | New page objects + page-change commands in cues | `ControlMap::Binding` gains `place` + `placeOf` (Computer / Venue / Show / Production / Cue list / Song section / Cue / Layer). Saved in `<ControlMap>` as attributes an older build ignores; Computer in QSettings (as controller jobs); Venue in the venue sidecar. | Each cue carries a full map |
| Resolved how | Current page | The binding whose place is live and narrowest (Layer > Cue > Section > List > Production > Show > Venue > Computer). Live = what is running in Operate (as `isHitPlaceLive`), the Show Tree selection in Design (as `isArmed`). Ties: refused at learn, one control one thing **per place**. | Current cue |
| New model? | **Yes** (pages): needs an explicit yes | **No**: places are the Show Tree's own | No, but duplicates data everywhere |
| Who changes it | The operator or a macro | Nobody: it follows the show | Nobody |
| LEDs | Per page | Each bound control shows its *resolved* binding: dark = nothing here, dim = assigned and idle, bright = running, blink = fader not caught | per cue |

### 5.4 Rules (write these first, then sweep)

1. **GO, Back and ALL OFF are never scoped away.** A control holding one of them at any place cannot get a narrower binding (the learn refuses and says why), and ALL OFF can only be bound at Computer or Show scope. The double-GO guard applies to every GO family command from any place (they all go through `ShowControl::run()`).
2. **A scope change never fires anything.** It is worked out on a GO, a cue change, a list start or stop, a production switch, or a selection change, never per frame (the hit resolver's rule).
3. **A press belongs to what it pressed.** A button held across a scope change releases to the target it fired: store the pressed binding in the down-set, not just the control (today `m_down` keys the control and `release()` looks the binding up again).
4. **A fader belongs to its target until it is let go, then catches.** When the binding under a fader changes (fader held at 70 % when the cue changes), the old target keeps its level and is no longer driven. The new target is untouched until the fader crosses its current level (catch). The lamp blinks and the footer says "Fader 3: bring to 45 % to take Front wash". Encoders are relative: a gesture's coalesced turns are flushed to the old target before the switch.
5. **What a scoped fader set is kept when its place ends**, except a *layer level*, which belongs to that cue and goes when the cue leaves the stage (Q14).
6. **Hits and functions from any place fire only in Operate at a live place** (CS1a), and Show-only fixtures follow the output rules unchanged.
7. **One source for the resolved map**: the Control Map panel, the Learn dialog, the LEDs, the Test Surface log and Show Review all read one `resolvedBinding(universe, channel)`.

### 5.5 How it attaches without a new model

- **Learn Trigger…** gains one combo, **Where**: *Everywhere on this computer / This show / This production / This cue list / This song / This cue / This layer*. It defaults to the narrowest place of the row it was opened on. No new menu.
- The **Control Map table** gains a **Place** column and groups by it.
- The Show Tree **inspector** of a cue, list or section lists "Controls here" (read-only, with Learn…). No new tab.
- The engine reuses `ShowTreeController::isHitPlaceLive()` and extends `HitScope` with Section and Layer, or adds a parallel `Place` enum with the same order.

### 5.6 Recommended first slice (CS7 above)

Buttons only, places Show / Production / Cue list / Song section / Cue, resolver = the hit resolver, rules 1-3, 6 and 7. That gives "a bank of hits per song on the same eight pads" with no paging work and no takeover problem. Faders with catch come in CS6 at Show/Computer scope, then scoped faders (rules 4-5) as slice 2. Layer scope is a Design-only slice 3: a fader on the Workbench's selected layer = that layer's intensity. Operate layer faders need an engine path (an intensity override on the layer's baked scene) and get their own ROI line.

ROI (slice 1): Value 4 × Fit 5 ÷ 3 days × 0.8 = **5.3**. Tests: a sweep of every place × live/not live × Design/Operate × binding mode; GO/Back/ALL OFF never shadowed (try to learn narrower: refused); a held press across a GO releases to its target; resolution recomputed on a GO, not per frame (counter).

### 5.7 Owner questions for scope

| Q | Question | Recommended |
|---|---|---|
| Q11 | Should scope follow the show by itself (implicit), or by explicit page buttons? | **Implicit** (narrowest live place), with no page buttons in slice 1. Explicit paging can come later as a Computer-scope "page" control if he finds he wants it. |
| Q12 | In Design, is "live" the Show Tree selection or the Run tab's list? | **The selection** (as computer keys today), so he can try a song's buttons while building it. |
| Q13 | The default place when learning from a cue row | **That cue's song section** if the list has sections, else the cue list. A cue-only binding is a deliberate choice. |
| Q14 | A layer fader's level when its cue leaves the stage | **Released** (it belongs to that cue). Group and speed master levels set from a scoped fader are kept. |
| Q15 | Venue scope: house-light faders bound per venue (PMS) | **Yes, later:** a venue-sidecar place, after slice 1. |
