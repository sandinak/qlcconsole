# Roadmap 0.4.6 → 0.5.0: the Dance Hysteria milestone

Written 2026-11-14 on the 0.4.5 train (`train/045a`, b296f13e7). A plan,
not a decision: the questions in §4 are his to answer. Read on a phone:
every section starts with the short version.

**Sources.** The script (*Dance Hysteria – Script*, Google Doc, last edited
2026-11-12; the same text as `scratchpad/roi/show/script.txt`, cited
below as `script:LINE`), the show file `DanceHysteria-0.6.qlcc` and its
`.venues.xml` (saved 2026-11-14 12:42), the music folder
(`~/Crew/Current/sandsite/music`), STRATEGY.md, TODO.md, CHANGELOG
[0.4.5]/[0.4.4]/[0.4.3], docs/features, UX_RULES.md, TEST_MAP.md, the
roadmap export (147 items, 120 not done) and the board (229 answered
questions). **Read** means it is in one of those. **Inferred** means I
pieced it together; **guess** means I could not check it.

**The one-paragraph answer.** The tool already covers most of what the
script asks for; what it lacks for this show is small and specific: a
colour that *spreads from the witch* (the spell, the blood on the steps,
the steps lit as the cast is bespelled), a visible "what RT fires now" for
the staff hits, a safe way to run with two GO sources, a resume after a
crash, a one-page pre-show check, and faster cue writing for the ~60 cues
still to build. The biggest risk is not code: Pad Run, the WLED staff,
the IR candles, Fire Spread and the lanterns have never run on his real
hardware. So 0.4.6 builds the witch's magic and puts it on the bench,
0.4.7 speeds up writing the songs, 0.4.8 hardens tech week, 0.4.9 is
show day and the classroom (last feature release), and 0.5.0 is hardening
only: soak, real-pad sweep, dress-rehearsal checklist.

---

## 1. The show, as the tool must serve it

**Short version.** 42 moments: 15 covered, 12 built but never run on his
hardware, 8 partly covered, 4 missing (three are "a colour spreads from
a point"; the fourth is fog, which the script doesn't ask for), 3 sound,
house or undecided. Cues exist only for the preshow and the first third of
*Everybody Scream* (25 cues, last is 1.1.3 Solo End, plus 2.0.1, 2.9.0
and 3.0.1); everything after `script:90` has no cue yet. That is
expected: he has just started building in the tool.

**Running order.** Read from the script's headings; the section numbers
are the show file's for 0-3 (Startup, Everybody Scream, Lets Dance,
Abracadabra) and **inferred** for 4-6 from the music folder (01, 02, 03,
03.5-Waltz, 05, 06; there is no 04 track because Life and Death is
a cappella).

| S | Section | Script lines |
|---|---|---|
| 0 | Opening / preshow (EVO chant, announcer, credits) | 17-51 |
| 1 | Everybody Scream | 53-178 |
| – | Time skip (TOWN1 lights the lanterns) | 180-191 |
| 2 | Let's Dance (day ↔ night cycle) | 193-315 |
| – | Interlude (the witch's spell; show file numbers it 3.0.x) | 317-321 |
| 3 | Abracadabra | 323-402 |
| 4 | Life and Death a cappella + Waltz | 410-532 |
| 5 | Heads Will Roll | 534-652 |
| 6 | Life and Death Reprise, curtain close | 654-704 |

*Thriller* is in the music folder but not in the script: assumed cut or an
encore; nothing planned. The Oct 20 `.docx` in Downloads is a different
event (Masked Singer).

**Status words.** **Covered**: built and tested in the gate.
**Unverified**: built and tested offscreen, never on his hardware.
**Partly**: something is missing (said). **Missing**: no feature.
**Not ours**: sound or people, not lighting.

| # | Script | Moment | Cue in show file | Status | Gap → item |
|---|---|---|---|---|---|
| 1 | 23 | Curtain closed; light on curtain and announcer | 0.0.3 Announcer (follow spot on Announcer target, AT-W-ALL on Main Drape) | Unverified | follow spot framing on real zoom heads never checked (TODO "Follow spot size") |
| 2 | 37-42 | EVO chant "what time is it", STOMPs, "Let's go" | 0.0.4-0.0.10 | Covered | – |
| 3 | 45 | Lanterns light, front gets creepy, organ | 0.0.11 Announcer (Lanterns Candlelight, USTs) | Unverified | WLED lanterns on real devices (`wled-real-device`) |
| 4 | 51 | Front lights out, **curtain slowly opens** | 1.0.1 Curtain (Curtain Open layer) | Partly | curtain fixture is on U04-Control, which has **no output in the show or either venue** → Q-A |
| 5 | 55-56 | Night square, backlit, blue/purple/yellow | 1.0.2 Ahhs | Covered | – |
| 6 | 61 | Uplights per set of AHHs, last to all | 1.0.2.1-1.0.2.4 | Covered | – |
| 7 | 63-141 | Solo, choruses, dance break | 1.1.1-1.1.3 (solo only) | Covered | cues not written yet |
| 8 | 144 | Witch USC, cast clumps round her | none | Covered | Aim at ES-Witch + Blocking per cue exist |
| 9 | 156 | Witch falls; **blood flows down the steps** | none | **Missing** | a red front moving downstage on the step pixels → `spell-spread` |
| 10 | 174 | Blackout, witch slips off | none | Covered | Block cue (CUE-04 allows it) |
| 11 | 182 | TOWN1 enters DSR with the matchstick | none | Partly | matchstick = held prop, being built (TODO "Props a person holds") |
| 12 | 187-189 | **Lantern 1 ignites** (flare SFX, SR tower), then the rest | none | Unverified | Fire Spread from SR over the Lanterns group (0.4.5, offscreen; WLED Effect-mode lanterns untested) → `spread-on-show-kinds`; SFX sync → Q-B |
| 13 | 195-263 | Day → sunset → night → morning → midday → night; cyc for time of day | 2.0.1 (empty) | Covered | long cue fades, per-look fades |
| 14 | 196 | Spot as **sun and moon** across the sky | none | Unverified | slow position fade + named loaded gobo (0.4.3, "the moon in AT-2 3"); real gobo slot not checked |
| 15 | 197 | **Laser stars** at night | none | Unverified | Starry lasers patched only in the venue (U14/U15); PWR-09 checks built; band zone question open → Q-D |
| 16 | 294 | Witch appears USC, hooded, staff hidden | 3.0.1 Witch Appears (NeedsWork, staff Starburst) | Partly | numbered in S3 although the script puts it at the end of S2 (inferred: he means the interlude); fine either way |
| 17 | 313 | Throws off cloak, **stamps staff**, song ends, blackout | none | Unverified | staff = WLED over Wi-Fi JSON (U13); script says 2 staffs, show patches 1 (Staffs group = 1); latency in the room never measured |
| 18 | 318-319 | **Spell sweeps the stage**: fountains and lanterns bleed red | none | **Missing** | colour spreading from the witch → `spell-spread` |
| 19 | 319 | Fire rises (fountain flames) | none | Covered | Silk Flame relays, fountain fire height (TODO "Fountain fire") |
| 20 | 320 | Drone F + SFX | – | Not ours | sound; sync → Q-B |
| 21 | 325 | Dark colours, witch spells bright | none | Covered | – |
| 22 | 329-330 | Witch casts the spell: **steps light up as they're bespelled** | none | **Missing** | spread from the witch over the Steps group → `spell-spread` |
| 23 | 335-341, 362 | **Staff slams on the vocal hit** (×2) | none | Unverified | cue-scoped hits fire on RT; two at one place take turns (show-control.md "Fire a hit"); nothing on screen says which hit is armed → `run-armed-hit-shown` |
| 24 | 383 | "The floor's on fire" | none | Covered | Firelight on FARTs/steps |
| 25 | 386 | "Tribute to Phantom?" | – | – | undecided in the script (his) |
| 26 | 412-414 | Minimal, reds/yellows, witch CS | none | Covered | – |
| 27 | 420-421 | Witch gestures, **25 hand-held candles light** | none | Partly | IR blaster fixture + drawing being built (TODO; `ir-artnet-controller` DMX-CHART) → `candle-safety` |
| 28 | 520 | Waltz, **candles off**, handed off | none | Partly | same |
| 29 | 527-532 | Click starts; "STOP!" staff slam + BOOM; back to square lighting; cast drops | none | Unverified | a GO on the beat with the staff in the cue; tight SFX sync → Q-B |
| 30 | 536 | Orange, green | none | Covered | – |
| 31 | 641-650 | Witch USC, all inside the main drape; staff slam, cast drops | none | Partly | ending undecided in the script; "inside main drape" may mean the curtain moves → Q-A |
| 32 | 656 | White for players, red/yellow for scenery | none | Covered | – |
| 33 | 659-665 | Staff resurrects the cast to bow; "ACK" for **band** and **crew** | none | Partly | band targets exist (Horns 1, Guitars, Drums); **Horns 2 sits at the plot origin** (0,0,0); no crew target (his data) |
| 34 | 667 | **Stage progressively catches fire** through the song | none | Unverified | Fire Spread with Spread Over = the cue's time (0.4.5, offscreen) |
| 35 | 678-688 | Staff drops them; fade to just witch and fire | none | Covered | tracking + Release |
| 36 | 696 | Fade out witch, stage on fire | none | Covered | – |
| 37 | 704 | Waltz as **curtains close** | none | Partly | as #4 → Q-A |
| 38 | – | GO, Back, Sub GO from the DualSense / Xbox pad | – | Unverified | Pad Run rebuilt today on his 5 rules (M1000-M1008, offscreen only) |
| 39 | – | Follow spot op (DualSense, SL preset, read) | FollowSpotOperators in show | Unverified | operator feel at dink still open (roadmap `fspot-operator-view`) |
| 40 | – | Fog / haze | – | Missing (inferred) | not in the script; an IR fogger remote is captured (`remotes/fogger.ir`); one SELECT value on the blaster, no build |
| 41 | – | 79 universes, 225 fixtures, 70 Art-Net outputs, 6,144 step pixels | – | Unverified | 0.4.1 soak on kronk passed (memory); never on 0.4.5+ |
| 42 | – | House lights, preshow music | – | Not ours (guess) | not patched; the venue's own board (guess) |

---

## 2. Workflow walk

**Short version.** Patching, the plot and groups are done and mature for
this show. The friction now is in **writing ~60 cues fast**, **effects the
script names that the tool can't make yet**, and **running safely with a
pad, a laptop and a phone that can all GO**. Teaching has one live bug:
lesson 3.5 teaches the old pad map.

In the order the show is built:

1. **Patch the rig and network.** Done for this show (79 universes).
   Friction:
   - Two patches: the show's own leaves 9 universes with no output (T2/T4
     towers, both lasers, U16) and the venue fills them (`.venues.xml`).
     Correct by design, but a student reading Connections can't tell which
     is live. PAT-06 notes it; no build.
   - **U04-Control (the curtain) has no output anywhere** (show + both
     venues). Cue 1.0.1 does nothing on the rig. → Q-A.
   - Both venues are named by timestamp ("Venue 2026-10-25 06:35",
     "Venue 2026-10-30 14:22"): a new venue doesn't ask for a name →
     `venue-name-on-create`.
   - WLED on real devices, ArtAddress on a real node: never checked
     (`wled-real-device`, `artnet-node-config-verify`).
2. **Place it on the plot.** Mature (0.4.3 handles, 0.4.5 in flight: box
   prop, facing-arrow drag, 45° outlines). Data only: two "Chorus copy"
   targets on one spot, "Person 3" a drum kit, "Horns 2" at the origin.
3. **Groups.** 53 groups, folders for step tops/fronts. No friction found.
4. **Focus and venue.** One empty focus call (Q191 builds calls from
   targets when none have groups, so it still works). Laser zone open
   (`lasers-clear-zone`, needs his call → Q-D).
5. **Build looks and cues per song.** The real cost from here: the script
   has ~45 SCENE/LIGHTING directions and the show has 25 cues, all before
   `script:90`. Friction:
   - Colour and level edits in the Workbench are not undo steps, and
     several look rows of one type don't edit together
     (`workbench-undo-multi`, verify: true).
   - Bring Into This Cue on a child layer takes two steps
     (`bring-into-child`).
   - One-off palettes copied by Duplicate keep the old cue's number in
     their name: palette 72 "44% · 1.1.1 Music Start AT-W-ALL" is used in
     cue 1.1.3; 56/71 "60% · 0.0.5 Who Are We 2" sit in a cue now called
     "what time is it?" (show file; `cloneOneOff`,
     engine/src/showtreecontroller.cpp ~5432) → `palette-name-follow`.
   - The command line makes production hits only; the ~9 staff slams want
     cue hits (`hit-for-cue-command`).
   - The directions are already typed, in the script: the calling script
     can import the text but can't turn its SCENE lines into cues
     (show-tree.md "The calling script") → `script-to-cues`, Q-G.
   - UX flows F-04 and F-07 still walk Build, hidden in the qlcconsole
     view since 0.4.4 (Q228): the walker checks a path he can't see →
     `uxflows-show`.
6. **Effects.** Fire Spread (0.4.5) is fire only; the script asks four
   times for *a colour that spreads from the witch or runs down the
   steps* (#9, #18, #22, and the lantern ignite #12) → `spell-spread`.
   Fire Spread runs per pixel through a script; over the 96 step bars
   (6,144 heads) plus the FARTs its tick cost is unmeasured (guess: the
   biggest per-tick risk in the show) → `spread-on-show-kinds`. RGB
   effects don't run on the colour-wheel T-Spots (Known issue); static
   wheel colours are enough for this script → deferred.
7. **Props and blocking.** Blocking per cue is built and used (ES-Witch has
   2 sets). Held props and the candle blaster are being built today. The
   candles are a single 2-channel IR fixture for the whole room, and
   SELECT 5/6 (Dim/Brighten) are *presses*: a tracked cue, a Back or a
   re-sent window would dim again (DMX-CHART.md) → `candle-safety`.
8. **Follow spots.** Built and refined (0.4.3 framing, 0.4.4 operator
   view). Only hardware feel is open.
9. **Rehearse and tech with notes.** Notes pane, needs-work flag (used on
   3.0.1), cue sheet with every note (merged in train/043j), calling
   script with Follow. He watches rehearsal clips in a separate player
   and stops it constantly (REHEARSAL_VIDEO.md "Why"): a real cost, but
   7 days for rungs 1-4 → Q-C.
10. **Run the show from the pad and phone.** Pad Run follows his five
    rules (0.4.5). Friction:
    - RT fires "the armed hit", but the Run tab, the phone page and the
      Pad chip don't say which; the name is spoken only after it fires
      (show-control.md). Watching the witch, he needs it before
      → `run-armed-hit-shown`.
    - The laptop (⌘↩, Run tab), the pad and the phone page can all GO;
      `App::runCommand` (ui/src/app.cpp:6055) has no guard against two
      GOs from two sources a moment apart: one cue skipped
      → `go-double-guard`, Q-B.
    - The Workbench still plays what it loads live in Operate
      (`workbench-live-operate`, needs his call) → Q-E.
    - After a crash, nothing reopens at the live cue (no resume in the
      docs) → `crash-resume-cue`.
    - Previews, Locate and Solo in Design reach the curtain, lasers and
      flame relays like any light; Output rules has no "show only" rule
      (fixture-manager.md "Output rules") → `output-rule-show-only`, Q-A.
    - The script credits him as **technical and show band director**
      (`script:47-48`): he may be on stage with the band during the show
      (inferred), so a student may hold GO. → Q-B.
11. **Teach students.** Lesson 3.5's student page and handout still say
    "hold LB, press A" (`docs/course/level-3/lesson-05/student.md:19,100`,
    `handout.md:29,97`); the glossary has the new map. The course audit
    ranks a blocking student page first (`course-catchup-04x`); L3
    pictures are missing. → `course-35-padmap`, `course-show-pages`.

---

## 3. Re-scored ROI

**Formula** (one for every row):
`ROI = (2 × Show + Teach) × Freq × Risk ÷ (15 × days)`.
Show = value to building/running *this* show (1-5), counted twice; Teach
= teaching / ease of use (1-5); Freq = how often it bites (1-5); Risk = how
bad its absence is at show time (1-5); days = agent-build days with help,
tests and CHANGELOG. 1.0 = one day spent on a mid-value fix. Sizes: S ≤ 1
day, M 1-3, L 3-6, XL > 6. NEW = found in §1-2.

### Built in 0.4.6-0.4.9

| Item | id | Show | Teach | Freq | Risk | Size | ROI | Rel. |
|---|---|---|---|---|---|---|---|---|
| Lesson 3.5 teaches the old pad map | NEW course-35-padmap | 2 | 5 | 3 | 3 | S 0.25 | **21.6** | 0.4.6 |
| Run tab / phone / Pad chip say what RT fires now | NEW run-armed-hit-shown | 4 | 4 | 4 | 4 | S 1 | **12.8** | 0.4.6 |
| IR candles: only state commands track | NEW candle-safety | 4 | 2 | 2 | 4 | S 0.5 | **10.7** | 0.4.6 |
| Before the house opens: one check page | NEW preshow-check | 5 | 4 | 3 | 5 | M 1.5 | **9.3** | 0.4.9 |
| Spell Spread: a colour from a point or direction | NEW spell-spread | 5 | 3 | 4 | 4 | M 1.5 | **9.2** | 0.4.6 |
| Spread on lanterns, relays, steps + its tick cost | NEW spread-on-show-kinds | 5 | 1 | 3 | 4 | S 1 | **8.8** | 0.4.6 |
| Second GO from another source held | NEW go-double-guard | 4 | 3 | 2 | 5 | S 1 | **7.3** | 0.4.8 |
| Output rule "Show only" | NEW output-rule-show-only | 4 | 4 | 3 | 4 | M 1.5 | **6.4** | 0.4.8 |
| Workbench in Operate loads Blind | workbench-live-operate | 4 | 3 | 2 | 5 | S 1.5 | **4.9** | 0.4.8 |
| UX flows in the qlcconsole view + pad flows | NEW uxflows-show | 2 | 4 | 3 | 3 | S 1 | **4.8** | 0.4.6 |
| blockedDimmedHeads allocations per tick | blockeddimmed-allocs | 3 | 1 | 5 | 2 | S 1 | **4.7** | 0.4.9 |
| Resume at the live cue after a restart | NEW crash-resume-cue | 5 | 2 | 1 | 5 | S 1 | **4.0** | 0.4.8 |
| Make cues from the script's directions | NEW script-to-cues | 5 | 4 | 4 | 2 | M 2 | **3.7** | 0.4.7 |
| Held props + candle blaster (in flight) | held-props (TODO) | 3 | 3 | 3 | 2 | M 1 | **3.6** | 0.4.5/6 |
| Bring Into on a child layer | bring-into-child | 3 | 3 | 3 | 2 | S 1 | **3.6** | 0.4.7 |
| One-off palette names follow their cue | NEW palette-name-follow | 3 | 3 | 3 | 2 | S 1 | **3.6** | 0.4.7 |
| Laser inside the band zone | lasers-clear-zone | 4 | 2 | 1 | 5 | S 1 | **3.3** | 0.4.8 |
| Workbench edits undo + several rows | workbench-undo-multi | 4 | 4 | 4 | 3 | M 3 | **3.2** | 0.4.7 |
| `hit … for cue` on the command line | hit-for-cue-command | 3 | 2 | 3 | 2 | S 1 | **3.2** | 0.4.7 |
| PWR-01 counts colour palettes | review-pwr01-palettes | 4 | 1 | 1 | 4 | S 1 | **2.4** | 0.4.9 |
| macOS memory growth: close or explain | mac-memory-growth | 4 | 1 | 2 | 4 | M 2 | **2.4** | 0.4.9 |
| A new venue asks for its name | NEW venue-name-on-create | 2 | 3 | 1 | 1 | S 0.25 | **1.9** | 0.4.7 |
| Course: blocking, props, Pad Run pages, L3 pictures | course-catchup-04x + blocking-lesson + course-l3-pictures | 1 | 5 | 2 | 2 | M 2 | **0.9** | 0.4.9 |

The course batch scores low by the formula (it weights the show) but stays:
STRATEGY guardrail 2 makes a course note part of shipping, and the
teaching half of the milestone needs the pad and blocking pages right.

### Scored and deferred past 0.5.0

| Item | id | ROI | Why it waits |
|---|---|---|---|
| Several hits on the pad (RT + X/Y/A/B) | padrun-multi-hit (was padrun-hits-focus-confirm's Hits page) | 1.6 | cue hits take turns on RT; covers every slam in the script → Q-F |
| RGB effects on wheel/CMY movers | rgb-effects-wheel-cmy | 1.2 | wheel colours are enough here; hot path, needs perf numbers |
| Fixtures duplicate/group undo | fixtures-dup-undo | 1.2 | the patch is done |
| Group masters on the pad | group-master-pad-map | 1.1 | not in the script |
| Calling script per list | calling-script-per-list | 0.9 | the show has one list |
| Walk-the-show leftovers | padrun-walk-leftovers | 0.8 | MVP covers the stage walk |
| Load chip breakdown | load-chip-breakdown | 0.8 | `spread-on-show-kinds` measures what matters |
| Blocking leftovers | blocking-leftovers | 0.7 | Blocking already used; held props go first |
| Rehearsal video rungs 1-2 | (REHEARSAL_VIDEO.md) | 0.6 | 3.5 days; risk at show time is low → Q-C |
| Workbench row drag | workbench-row-drag | 0.6 | arrows work |
| Signalling / curtain controller | signalling | 0.5 | only if the curtain is DMX → Q-A |
| Notes on reusables | notes-marks-reusables | 0.5 | – |
| Aim/Formation split (Q206, decided) | aim-formation-split-q206 | 0.4 | decided, but L and risky this close to the show; after 0.5.0 |
| LTC polish | ltc-polish | 0.4 | GO by hand recommended → Q-B |
| VC Cue List sections | song-sections-vc | 0.3 | he runs from the Run tab |

### Close on the roadmap (already shipped)

`fspot-live-size`, `fspot-framing-presets`, `rig-handles`,
`course-l3-missing` (3.1/3.2/3.4), `newhere-palettes-studio`,
`course-mov08-presets`, `uxwalk-findings-1`, `ux-review-findings-1`
(all 0.4.3); `cue-sheet-all-notes` (train/043j); `fspot-operator-view`,
focus confirm of `padrun-hits-focus-confirm` (0.4.4; hardware feel still
open); `wfaudit-silent-failures` (0.4.5, three of four; Show Review's
Fix ▸ on a gone cue is the fourth: fold into `preshow-check`);
`showtree-cues-subcues`.

### Bench checks (his hardware, not builds): in the release that needs them

`wled-real-device`, `dualsense-touch-test`, `controller-slots-hw`,
`steamdeck-hw-check` (only if the Deck or phone is used on show night).

### Every release (recurring)

`upstream-intake`, `library-sync-release`, `lupdate-each-release`;
`make-perf-release` + `perf-baseline-showbuilder` once, at 0.5.0. Tooling
already in flight keeps going: `ux-tooling`, `menuwords-check`.

### DEFER past 0.5.0 (one reason per group)

- **GO by hand for this show (Q-B):** `timeline-p1-cue-clip`,
  `timeline-p2-clock-source`, `timeline-p3-markers-sections`,
  `timeline-p4-ruler`, `timeline-mtc-out` (Q220: built when a show needs
  it), `tl-snap-beat`, `tc-latency-calibration`, `tc-per-version-maps`,
  `show-manager-g1`.
- **New surface or XL, not in the script:** `sound-reactive-design`
  (Q227 LATER), `spatial-effects-backlog` (Spell Spread takes the show's
  part), `tablet-ipad`, `ai-headless-mcp`, `ui-zoom`, `native-gdtf`,
  `eos-import-leftovers`, `stage-inventory-across-shows`,
  `rdm-find-address`, `power-by-intensity`, `focus-shutters`,
  `w7-conversions`, `upgrade-collection-busk`,
  `starfield-coverage-outline`, `cue-sheet-fixture-rows`,
  `line-target-p4`.
- **Playback polish the script doesn't call for:**
  `playback-p2-fade-shapes`, `playback-p3-step-transitions`,
  `playback-p4-swop`, `subcue-step-effects`, `cue-transition-4b`.
- **Plot and Studio polish (the plot is done for this show):**
  `area-cover-leftovers`, `audience-zone-q160`, `beam-masking`,
  `drumkit-draw`, `fountain-flat-planes`, `one-selection-leftovers`,
  `studio-select-similar`, `buried-fixture-scenery`,
  `cyc-backwash-leftovers`.
- **Build / Show Review polish:** `build-palettes-tree` (Build hidden),
  `effect-designer-leftovers`, `vc-context-menu`, `fixture-defs-gobos`,
  `fixture-editor-updates`, `connections-leftovers`,
  `review-missing-checks`, `review-undo-accept`,
  `review-gate-courseshows` (1.1).
- **Platforms and devices this show doesn't use (a Mac runs it):**
  `pi-release`, `linux-fifo-pkg`, `web-tls`, `win-releasecheck-ssh`,
  `win11-matrix`, `win-code-signing`, `rttune-windows`, `win-heavy-tick`,
  `updater-real-release`, `warning-backlog`, `perfprec-leftovers`,
  `rig-offload`, `controlsurface-s4`, `encoder-controller-check`,
  `streamdeck-publish`, `padrun-arm-key`, `artnet-node-config-verify`,
  `focus-refine-open`, `course-clips`.
- **His calls, any time, no build:** `send-defs-upstream`,
  `upstream-uaf-report`, `clean-clones`.

### DROP

- `steamdeck-native`: already in the drop lane (back burner, XL).
- `qlc5-import`: STRATEGY's NOT-built list has "QLC+ 5 parity work"; the
  roadmap note already flags it. Reopen only on his explicit yes.

---

## 4. The release plan 0.4.6 → 0.5.0

**Short version.** Four feature releases, each about one day of parallel
agent build plus a gate, each testable in one bench session, then a
hardening-only 0.5.0. **0.4.9 is the last feature release.** Questions are
lettered Q-A … Q-G (put them on the board as the next Q numbers); each has
my recommended answer.

0.4.5 (in flight, not part of this plan): Pad Run on his rules, Controller
Test picture, Connections By universe, WLED set up on patch, Add Fixture
gaps, search keeps selection, Fire Spread; still building: held props, IR
candles, box prop, facing-arrow drag, 45° outlines.

### 0.4.6 — The witch's magic, on the real rig

Why together: every item is a script moment (#9, #12, #18, #22, #23,
#27-28) or how the operator fires one, and they share one bench session
on the real steps, lanterns, staff and candles.

| id | Size | Notes |
|---|---|---|
| spell-spread | M 1.5 | startNow |
| spread-on-show-kinds | S 1 | startNow |
| run-armed-hit-shown | S 1 | startNow |
| course-35-padmap | S 0.25 | startNow |
| uxflows-show | S 1 | startNow |
| candle-safety | S 0.5 | after the candle fixture lands (feat/045-held-props) |
| held props / candle blaster | carry-over | only if not in 0.4.5 |

**He checks on hardware:** Pad Run sweep on the DualSense and the Xbox pad
(every control, armed and not: rules 1-5); a staff hit on RT in the room
(how late is it over Wi-Fi?); the lanterns igniting from SR; a spell
spreading from the witch over the real step pixels; candles On / Flicker /
Off through the tower blaster on its real universe (never Art-Net 0:0:0).
**Depends on:** 0.4.5's Fire Spread (on `train/045a`).
**Question:** Q-F.

### 0.4.7 — Write the songs fast

Why together: all of it is the cue-writing loop he is in now (~60 cues to
go), in the Show Tree and Workbench; one bench session = write *Let's
Dance* (the day/night cycle) end to end and time it.

| id | Size | Notes |
|---|---|---|
| workbench-undo-multi | M 3 | – |
| bring-into-child | S 1 | – |
| hit-for-cue-command | S 1 | the staff slams |
| palette-name-follow | S 1 | startNow |
| script-to-cues | M 2 | Q-G |
| venue-name-on-create | S 0.25 | – |

**He checks:** a song written from its script lines; ⌘Z after every colour
and level change; duplicated cues read their own numbers.
**Questions:** Q-G, Q-C.

### 0.4.8 — Tech week: safe to run

Why together: the things that go wrong in a run with an audience: two GO
sources, a crash, a preview reaching a curtain or a laser, the Workbench
live in Operate. One bench session = a full run with laptop, pad and phone
together, the app killed mid-song.

| id | Size | Notes |
|---|---|---|
| go-double-guard | S 1 | Q-B; rules first, his yes before building (memory: operator-control-rules) |
| crash-resume-cue | S 1 | loads, never plays (rule: nothing moves without GO) |
| output-rule-show-only | M 1.5 | Q-A |
| workbench-live-operate | S 1.5 | Q-E |
| lasers-clear-zone | S 1 | Q-D |

**He checks:** GO from the laptop and the pad at once (one cue, said);
force-quit and reopen (offered the live cue, nothing plays); Design
preview of 1.0.1 and a laser cue (held); Show Review on the laser zone.
**Questions:** Q-A, Q-B, Q-D, Q-E.

### 0.4.9 — Show day and the classroom (last feature release)

Why together: what the crew does before the house opens, and what the
students read. One bench session = the pre-show check on kronk, on the
show network, with the real show.

| id | Size | Notes |
|---|---|---|
| preshow-check | M 1.5 | + the dress-rehearsal checklist (help page) |
| review-pwr01-palettes | S 1 | breakers: APC 3000 VA, 20 A circuits in the show |
| blockeddimmed-allocs | S 1 | hot path; hotpath_test numbers |
| mac-memory-growth | M 2 | close or explain with the soak data |
| course batch | M 2 | blocking + held props + Pad Run student pages, L3 pictures |

**He checks:** the pre-show page on kronk names nothing he doesn't
already know; a student follows the Pad Run page without help.
**No questions.** After 0.4.9: feature freeze (bugs, docs, course only).

### 0.5.0 — The show milestone (hardening only)

No new features. In order:
1. Every bug from the 0.4.6-0.4.9 benches, fixed and gated.
2. Full gate (gate=0, linux=0), Windows CI green, the release matrix
   (RELEASE.md), `make perf` once on a quiet ender.
3. **Soak:** 3 × 8 h on kronk with the final DanceHysteria (real /
   scaled / rough, as the 0.4.1 run), Art-Net leaving en5 (check pps),
   no crash, footprint flat.
4. **Real-pad sweep:** every control on each pad (DualSense, the Edge
   clone, Xbox), armed and not; recorded.
5. **Dress-rehearsal checklist** (from `preshow-check`), run in the room:
   every universe reaching its node, staff ×2, candles, lanterns,
   fountains, lasers (zone), follow spot framing, Local Network allowed,
   power, battery, screen awake, autosave on.
6. Known issues lists every offscreen-only item still left.

### Questions he must answer (each with my recommendation)

- **Q-A Curtain.** Is the main drape moved by DMX at the venue (U04-Control
  has no output in the show or either venue) or by a person on the rail?
  *Recommended:* a person on the rail, called from the calling script;
  leave the Curtain layer as a drawing. Build `output-rule-show-only`
  either way, for the lasers, flame relays and any fogger (and the
  curtain, if it is DMX). Blocks: output-rule-show-only, signalling.
- **Q-B Show-night roles.** Who presses GO (you on the pad from the band,
  a student at the laptop, or both), who plays SFX and the click, and does
  any song run on timecode from Logic? *Recommended:* a student at the
  laptop owns GO with the calling script on Follow; you keep the pad armed
  for the staff hits; every song GO by hand (timecode stays built,
  unused); the sound op plays SFX, called from the same script; add the
  double-GO guard. Blocks: go-double-guard; keeps the timecode items
  deferred.
- **Q-C Rehearsal video before the show?** *Recommended:* after 0.5.0
  (ROI 0.6 here: 3.5+ days, low show-time risk). If you are designing from
  phone clips every day this month, the 0.5-day seek spike goes in 0.4.7
  and rungs 1-2 in 0.4.8.
- **Q-D Lasers inside the band's no-light zone** (`lasers-clear-zone`).
  *Recommended:* move the band zone to start upstage of the lasers (a plot
  edit you make) and have Show Review say when a laser sits inside a zone;
  no other build.
- **Q-E The Workbench in Operate** (`workbench-live-operate`).
  *Recommended:* it loads Blind in Operate; **Play Live** is one click and
  said in the footer.
- **Q-F One hit on RT for the show?** *Recommended:* yes. Cue hits that
  share a cue take turns on RT, which covers every staff slam in the
  script; RT + X/Y/A/B waits until after 0.5.0.
- **Q-G Cues from the script.** Should the calling script make proposed
  cues from its SCENE / LIGHTING lines? *Recommended:* yes: in the calling
  script window, previewed, numbered by the show's scheme (S.P.II), each
  on its line with the direction as its note, one undo step; text comes in
  as plain text (Google Docs ▸ Download ▸ Plain text).

---

## 5. Risks and blockers to the show milestone

**Hardware never run (the biggest risk; STRATEGY risk 6).**
- Pad Run on his 5 rules: gate only (M1000-M1008). One bench round with
  each pad before 0.4.7 builds on it.
- The staff: WLED over Wi-Fi JSON (U13), one patched of the two the script
  needs; latency with an audience's phones in the room is unknown.
- The candles: one IR fixture for 25 candles through one Pi in the tower;
  Dim/Brighten are stateful presses.
- Fire Spread / Spell Spread on real pixels, on Intel kronk, over 6,144
  step heads: tick cost unmeasured (guess: the heaviest thing in the show).
- WLED lanterns, fountain LEDs on a real device (`wled-real-device`).
- Follow spot framing on real zoom heads; the moon gobo slot.

**Single points of failure.**
- One show laptop (kronk, Intel MBP, battery at 808 cycles); no hot spare
  (multi-console is on the NOT-built list). Mitigation: `crash-resume-cue`,
  "Open the last show at start" (0.4.4), a second Mac with the same build
  and file, ready, not following.
- One IR blaster for every candle; one Wi-Fi path for the staffs.
- macOS Local Network permission: kronk silently refused Art-Net until he
  allowed it at the console (memory, 2026-11-09). Settings ▸ Performance
  checks it since 0.4.4; it goes on the dress-rehearsal checklist.
- Him: designer, console builder and (inferred, `script:47`) band
  director on the night.

**Only he can decide or do.** Q-A … Q-G; patch the second staff; add a
crew target and move Horns 2 off the origin; decide the Heads Will Roll
ending and the Phantom tribute (the script leaves both open); give the
candle blaster's universe; the bench rounds; re-store the notary profile
before 0.5.0 is published (memory: it vanished three times).

**Process.** One build on the bench at a time; the next one built ahead
(memory: build ahead). Never say "fixed" for anything a hand or device
touches until his pad or a recorded real trace confirms it.

---

## 6. What I'd do first tonight

Six items with no answer needed. All branch from **`train/045a`** (Fire
Spread and Pad Run's rules live there), one clone each under
`/Volumes/Ext/git`. **Builds run on the pool, not ender**: commit a WIP
and run `./check-all.sh --quick --remote auto --rev <SHA>`; one test dir
with `QLC_TEST_ONLY=...`. Each ships help, a CHANGELOG line, a TEST_MAP
row and a course note where it applies; UI changes need before/after shots
and the ux-reviewer's sign-off.

1. **spell-spread: a colour arrives from a point or a direction.**
   The script needs a look that *reaches* each light by its distance from
   the witch (the spell, the steps bespelled) or from a line (the blood
   running downstage). Fire Spread already gets each head's floor distance
   from the look's Aim target (`effect.targetDistance`, `_fromTargetFar`,
   `_cueTime`: engine/src/effectscript.h ~165, effectinstance.cpp;
   resources/effectscripts/fire-spread.js; docs/dev/EFFECTSCRIPT_API.md).
   Add a sibling effect, **Spell Spread** (Colour group), that reveals the
   layer's own colour/level instead of fire: settings Spread Over / Spread
   Time / Edge as Fire Spread, plus **From** = the Aim target (default) or
   a compass direction (downstage = "flows down the steps"), and **Flash**
   (a bright edge as it arrives). Lights not yet reached keep what the cue
   under them gives. Accept: EffectInstance tests like
   `fireSpread*` (each pixel by its own distance; direction mode orders
   heads by their projection; no Aim = centre; loop never restarts), the
   effect in the fitted picker for RGB groups, effects.md section, and a
   hotpath_test case over a 64-head bar.

2. **spread-on-show-kinds: what Fire/Spell Spread do to this show's
   lights, and what they cost.** Build a test show from
   DanceHysteria-0.6's kinds (a copy in the test's data, never his file):
   a WLED Lantern in Effect (15ch), a Silk Flame relay, a Step Row 64
   Heads bar, a PL-4003 bar. Assert what each gets (a relay switches at
   the flare, a WLED Effect-mode lantern takes colour + level and its
   effect channel is left alone, pixels burn one by one) and fix what
   doesn't. Measure the tick cost of Fire Spread over 96 step bars
   (6,144 heads) in the Release leg; if it is over 2 ms a tick, cache the
   per-head distances and the flame tables (the commit already caches
   distances, check it holds at this size). Accept: tests per kind, the
   number in the CHANGELOG/perf note, `hotpath_test` within budget.

3. **run-armed-hit-shown: say what RT fires before it fires.**
   `PadRun::armedHits()` (engine/src/padrun.h ~351) knows the hits at the
   narrowest armed place; the Cue Runner already has a hidden
   `crHitsNow` label (ui/src/cuerunner.cpp ~609). Show **Hit ▸ Staff
   Slam** (and "1 of 2" when two take turns) on the Run tab, in the
   footer Pad chip's text while armed, and on the phone Run page
   (webaccess/src/webaccessrunpage.cpp), all from one source and one
   signal (rule 2: one state on every surface). Nothing about firing
   changes. Accept: a test that arms the pad on a cue with two cue hits
   and checks all three surfaces agree before and after each RT pull, and
   after the cue moves on; the page-script test in webaccess/test.

4. **course-35-padmap: Lesson 3.5 teaches the old pad map.**
   `docs/course/level-3/lesson-05/student.md` lines 19 and 100-101 and
   `handout.md` lines 29 and 97 say "hold LB, press A"; the rules since
   0.4.5 are hold LB + RB to arm, hold LT + A for GO, RT the hit, all four
   = emergency stop (docs/features/show-control.md "The rules"). Rewrite
   those lines and the teacher guide's matching step in the course voice
   (crew-level, no cheerleading), check COURSE_PLAN.md row 3.5. Accept:
   `tools/check-course-ui-names.py` passes; a grep for "Hold \*\*LB\*\*"
   in docs/course finds only Walk-mode text.

5. **uxflows-show: the walker walks the view he uses.**
   UX_RULES.md flows F-04 and F-07 go through the Build tab, hidden in
   the qlcconsole view since 0.4.4 (Q228). Re-route them through the Show
   Tree's Workbench (the programmer strip, ⇧⌘N), and add **F-13 Run the
   show from the pad** (arm, LT + A, the Pad chip) and **F-14 Fire a staff
   hit** (a cue hit, armed, RT) with click budgets. Change ux_flows.json
   with the doc (check_rules.py keeps them in step); the pad steps are
   *manual* where the walker can't load the HID plugin. Accept:
   check_rules.py and the walker pass on lesson-3-8-start.qlcc.

6. **palette-name-follow: a copied one-off says its own cue.**
   `ShowTreeController::cloneOneOff` (engine/src/showtreecontroller.cpp
   ~5432) copies a one-off palette with its name, so cue 1.1.3 plays
   "44% · 1.1.1 Music Start AT-W-ALL" and a cue now called "what time is
   it?" plays "60% · 0.0.5 Who Are We 2" (DanceHysteria-0.6). First find
   where a person reads that name (Look Editor row, Why is this on?, the
   Library's This show, tooltips); if nowhere, close it as a no-op and
   say so. Otherwise: a copy takes its new cue's label, and Renumber /
   rename of a cue relabels its one-offs (one undo step with the
   renumber). Accept: ShowTree tests for duplicate, renumber, rename; an
   old show opens unchanged until edited.
