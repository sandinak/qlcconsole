# Show Review — design (post-0.3, not built)

**Status:** design only, 2026-10-25. Nothing here is built; the app is
feature-frozen for 0.3 (memory: feature freeze). Tracked in
[TODO.md](../TODO.md) under *Show Review*.

Branson, 2026-10-25: *"show review. When designing perhaps it's worth having
an auditor that looks for problems like this that haven't been explicitly
accepted ... identify which cases you're going to check for. Be
comprehensive (spots that might sweep when they shouldn't, conflicting
palettes, etc.). Can also include warnings from the footer if they've not
been resolved and they're not RUNTIME issues, but config/design issues."*

The problems that prompted it, all found by hand in the last weeks: matrix
scripts too slow for a Pi (commit `ead57dc60`: four scripts at 12.6–14 ms a
tick against a 12 ms budget on a Pi 5); hit buttons colliding in the same
scope and overriding across scopes (board Q154/Q157); venue clashes (house
and tour on the same addresses, "not ours" channels); fixtures not on the
plot; unaimable lights in Quick focus; missing fixture definitions; moving
heads that would sweep in light; palettes that conflict; tracking surprises;
Show Tree originals being edited instead of the list's generated functions.
Each was caught late, by a person, once. Show Review catches them every time,
before the show leaves the laptop.

Contents:

1. [Purpose and principles](#1-purpose-and-principles)
2. [What already flags problems](#2-what-already-flags-problems)
3. [The check catalog](#3-the-check-catalog)
4. [UX sketch](#4-ux-sketch)
5. [Architecture](#5-architecture)
6. [Testing plan](#6-testing-plan)
7. [Phasing](#7-phasing)
8. [Open questions](#8-open-questions)

---

## 1. Purpose and principles

Show Review reads the show — patch, plot, palettes, cues, hits, Virtual
Console, venues, power, files — and lists what is likely wrong with its
**design**, each with the place to fix it. It is the show-file audit a
programmer runs before tech, and the checklist a student runs before
handing a show in.

**Design time only.** A finding is about what is *saved in the show* (or
its venue sidecar), true on any computer that opens it. What is true only
*now, on this machine* — a controller asleep, an output held off the rig
network, an Art-Net node not answering, the engine running late, Blackout
on, Park holding — stays in the footer, where it already is. The test: *would
this still be a problem if the show were opened on the show laptop in the
venue, with every device plugged in?* If yes, it's Show Review's; if no,
it's the footer's. A few checks straddle (a universe with fixtures but no
output patch is design; the patched device being absent is runtime); §2
sorts the existing ones.

**Fixed, or accepted with a note.** Every finding ends one of two ways:

- **Fixed** — the show changes and the finding goes away on the next run.
- **Accepted** — someone says *this is on purpose* and why. The acceptance
  is saved **with the show** (who, when, the note), so the next person to
  open it sees the decision, not a fresh warning. *"Live move in the
  ballad: the spots drift with the soloist on purpose — BM 10-25."*

There is no third way: no "ignore", no "don't show again" on this
computer. A check that never applies to a show is turned off **for that
show**, which is itself an acceptance with a note (*"No moving heads in
this rig"*).

**Accepted findings stay quiet until what they refer to changes.** An
acceptance records a fingerprint of what the check looked at (§4.3). If the
cue, fixture, palette or button changes so that fingerprint changes, the
finding comes back — marked *was accepted*, showing the old note — because
the decision was about something that is no longer there.

**Three severities.**

| Severity | Means | Examples |
|---|---|---|
| **Blocker** | The show will do something wrong or unsafe, or won't open or play elsewhere. Fix it before the show runs. | two fixtures on the same addresses; a missing definition; a circuit over its breaker; media that isn't there |
| **Warning** | Probably wrong; look at it. Often fine once someone has decided. | a head moving while lit; two hits fighting over the same lights; a matrix too heavy for a Pi |
| **Note** | Tidy-up, paperwork, teaching. Never blocks anything. | an unused palette; "New Scene 4"; a cue with no name |

A blocker *can* be accepted (the show is the designer's), but the summary
always counts accepted blockers separately ("0 blockers · 1 accepted"), and
the course's hand-in can require none (§4.2).

**Every finding links to the place to fix it.** *Fix* opens the cue, the
fixture's properties, the palette in Build, the VC button, the Patch dialog
row, the venue page — the same jump-to the app already uses (Show in Show
Tree, Fixture Properties, Connections). A finding without a place to go is
a bad finding.

**Plain crew language.** Messages name things the way the show names them —
fixtures by name and number, cues by number and label, groups and palettes
by name — and say what happens on stage, not what the data says: *"Spot 3
pans from DSL to USC while lit at 60 % (cue 11 → 12)"*, not *"Fixture 7 ch
0..1 delta with intensity > 0"*. The same voice as the focus tools and the
course (memory: course voice).

**It fits teaching.** A student runs **Review** before handing a show in;
the teacher reads the same list, with each acceptance's note as the
student's reasoning. The CLI (§5.4) lets a teacher review a folder of
hand-ins at once. Findings teach: each has a one-paragraph *Why this
matters* the student can open.

**Not a linter for taste.** It never says a look is ugly, a cue is too long
or a colour is wrong. It flags what is *inconsistent, unsafe, unreachable,
unportable or unfinished*.

---

## 2. What already flags problems

The app already finds many of these, each in its own place, at its own
moment. Show Review does **not** replace them: it calls the same code
(one source of truth) and lists the design-time results together, with
acceptance. The runtime ones stay where they are.

### 2.1 The footer's readiness list (`ShowStatus`)

One shared registry, `ShowStatus` (`ui/src/showstatus.h`), feeds the
footer's *Ready / Not ready (n)* chip, its tooltip and `ShowStatusDialog`.
Most entries are filled by `AppUtil::updateFixtureReadiness()`
(`ui/src/apputil.cpp`), debounced 200 ms after fixture changes and after
load. **D** = design-time (Show Review lists it), **R** = runtime or this
computer only (stays in the footer only).

| Footer entry | What it says | D/R | In Show Review as |
|---|---|---|---|
| `fixtures.unpatched` | fixtures not patched | D | PAT-03 (used ones W; unused N) |
| `fixtures.clash` | fixtures loaded unpatched because their address overlapped (`Doc::addressClashes()`) | D | PAT-01 |
| `fixtures.duplicates` | two fixtures with one name (`Doc::duplicateFixtureNames()`) | D | FIX-14 |
| `fixtures.output` | a light whose mode has no lumens or power (`FixtureDefCheck::expectsLight()`) | D | FIX-07 |
| `fixtures.nodefinition` | a fixture standing in as a dimmer: definition missing (`Fixture::definitionUnavailable`) | D | FIX-01 |
| `definitions.failed` | a .qxf that won't read (`FixtureDefCheck::diagnose()`) | D (the show's) / R (this computer's library) | FIX-01 when the show uses it; else footer only |
| `definitions.duplicates` | two local files define one fixture | R (this computer) | footer only |
| `definitions.outofdate` | user definitions older than the shipped one (`FixtureDefUpdates`) | D | FIX-03 |
| `embedded.showonly` / `embedded.differ` / `embedded.problems` | definitions only the .qlcc carries, differing from this computer's, or unusable (`ShowEmbedding`) | D | FILE-03 (note / warning) |
| `universes.nooutput` | a universe with fixtures and no output patch | D | PAT-06 |
| `effects.failed` | an effect palette with no script, a missing script, or one that failed (`Doc::effectFailures()`) | D (no script, missing) / R (failed while running) | FX-07 for the first two |
| `show.load` | the load log: mode not found, channels or address out of range, unsupported sequences, video or audio file not found | D | FIX-04, PAT-02, FILE-02 (each line mapped to its check) |
| `output.dangling` | a patch whose line, interface or plugin isn't on this machine; output held by the rig-network gate | R | footer only (PAT-08 covers the show's rig networks) |
| `outputs.problems` | sends failing, nodes not answering | R | footer only |
| `wled.targets` | WLED not answering, weak Wi-Fi | R | footer only |
| `wled.config` | a WLED's own config disagrees with the show (`WledConfigCheck`) | R (polled from the device) | PAT-10 reads the last saved comparison only |
| `input.devices` | patched controllers not connected | R | footer only (VC-07 covers an input universe with no patch at all) |
| `storage.onlineonly` | cloud placeholders skipped on load | R | footer only (FILE-06 is the design cousin) |
| `simpledesk.held` | Simple Desk holding channels | R | footer only |

The other footer chips (`App::initStatusBar()`, `ui/src/app.cpp`) are all
runtime and stay as they are: the rig-network chip (`transmitGateStatus`),
Engine Load (tick time against `MasterTimer`'s budget — the *measured* side
of FX-01), the Overrides chip (Park, Highlight, Hold at end, Quick/Venue
focus, Blind), the move-in-black dangle chip (`MarkPlanner::
updateDangleDetection()`; MOV-09 is its design-time twin), Blackout,
timecode, Unsaved. The **power** chip is a live estimate of the look in
Design (OVERLOAD over a circuit's derated limit); PWR-01 runs the same sums
over every cue and hit instead of what's up now.

### 2.2 Checks outside the footer

| Existing check | Where | Finds | D/R | In Show Review as |
|---|---|---|---|---|
| `FixtureDefCheck::check()` | `engine/src/fixturedefcheck.cpp` | definition errors and warnings; shown only in the fixture editor's problems panel | D | FIX-02, FIX-07 |
| `FixtureDefUpdates::impact()` / `modeFit()` | `engine/src/fixturedefupdates.cpp` | what updating a definition would break | D | FIX-03's detail |
| `VenueRig::clashes()` | `engine/src/venuerig.cpp` (kinds Overlap, NotOurs, Slot) | house rig vs tour package, not-ours, slots; in the venue page, Apply, Patch dialog, the open notice | D | PAT-04, PAT-05 |
| `Doc::canPatchFixtures()` / `isOnNotOurs()` | `engine/src/doc.cpp` | refuses overlaps, past 512, not ours | D (refusal at edit time) | PAT-01/02/04 for files that got past it |
| `VenueCorrections` `needsFocus` / `isSwapped` / `orphaned` / `isDead` / checks | `engine/src/venuecorrections.*`, shown by `FocusReport` | swapped lights, orphan corrections, dead lights, checked-by | D (per venue) | VEN-05, VEN-06, VEN-07, VEN-08, FIX-08 |
| `ShowTreeController::whyNotSend()` | `engine/src/showtreebridge.cpp` | why a chaser can't be sent to the Show Tree (made by the tree, upgraded, a locked original) | D | CUE-12's wording |
| `ImportReport` / `ConvertPlan` / `CapturePlan` | `engine/src/showtreecontroller.h` | HTP conflicts, mixed includes, plays-differently cues, why a list can't convert | D, but only inside those dialogs | not repeated; CUE-12 covers their aftermath |
| `checkCueNumber()` | `ShowTreeController` | refuses a taken or bad number while typing | D (edit time) | CUE-06 for files that got past it |
| `lookReach()` / `lookMisses()` / `lookAmbiguities()` | `ShowTreeController`, shown as "applies to n of m" on a layer | a look reaching some of its fixtures; an ambiguous gobo name | D | PAL-03 |
| `LookStackCheck::check()` | `engine/src/lookstackcheck.cpp`, badges in Build's outline | covered looks, unmatched effects, partial dimmers | D | PAL-02 (1:1) |
| `ProgrammingOutline::decorateScene()`, `SceneGroupLooks::missingLooks()` | `ui/src/programmingoutline.cpp`, `scenegrouplooks.cpp` | a group with no look, "not patched", LookStackCheck issues, ⚠ in Build | D | PAL-02, PAT-03 |
| `SceneGroupLooks::headsThatCannotReach()` | `AimSolver::sceneAims()` + `BeamOcclusion` | heads that can't reach or are blocked from their aim target; red ring on the plot | D | VEN-04, MOV-10 |
| `AimSolver::aimable()` | `engine/src/aimsolver.cpp` | silently drops heads that can't point or aren't on the plot | D, unflagged | VEN-02, FIX-05 |
| Look Editor warnings | `ui/src/lookeditor.cpp` | no gobo/shutter/strobe channel on the targets; a colour too dark to read | D, inline only | PAL-03 |
| Power dialog scene check | `PowerDistributionWidget::slotSceneCheck()` | scenes and collections over a circuit's derated limit | D, on demand | PWR-01 (same sums, every cue and hit) |
| `NetworkTopology::warnings()` / `NetworkReport` | `engine/src/networktopology.cpp` | a wireless port with no receiver, transport ≠ patch, a universe on two ports | D | PAT-09 |
| Rig map address-clash tint | `Monitor::applyMapView()` | overlapping fixtures tinted red on the plot | D | PAT-01 |
| Add Fixture, Patch dialog, universe patcher checks | `addfixture.cpp`, `patchinventorydialog.cpp` (`refreshConflicts`), `universepatcher.cpp` | address taken, not ours, past 512, name taken — refused or CONFLICT while editing | D (edit time) | PAT-01/02/04 |
| `StructureStudioView::rebuildOverlapWarnings()` | Studio | fixtures overlapping on a structure | D | FIX-06's neighbour (a note, v3) |
| `ShowManager::checkOverlapping()` | Shows timeline | refuses overlapping items on a track | D (edit time) | SC-03 |
| WLED devices / LED layout problems | `wleddevicesdialog.cpp`, `ledlayouteditor.cpp` | device and layout config | D, in their dialogs | PAT-10, FIX-11 |
| Monitor "Not on the plot", Inventory panel | `monitor.cpp`, `monitorinventorypanel.cpp` | the selected fixture has no plot position; unplaced fixtures listed | D, not a warning | FIX-05 |
| VC duplicate widget id | `virtualconsole.cpp` on load | gives a duplicate id a new one, in the log only | D | §4.3 (acceptance keys of VC widgets) |
| `ProgrammerController::rigOutputProblem()` | focus tools | Blackout, GM at 0, no output, said to the operator | R | not ours |
| `RefineSession` shared-palette warning | Focus this cue | editing a palette other cues share | R (said while editing) | not ours |
| `TimecodeSource::health()`, MTC chip | timecode | no source, unstable | R | SC-02 covers the design half |

### 2.3 Nothing checks these today

From the inventory: **hit collisions and scopes** (board Q154/Q157 — no
code yet; hits are in `engine/src/showtreelooks.cpp`); **matrix and script
cost** on a target machine (only the live Engine Load chip and the
`QLC_TICKLOG` log; script errors go to the log); **strobe rates**;
**output-rule oddities** (Ignore Blackout on a stage light — only a tooltip
and an icon); **naming** (only duplicate fixture names); **absolute paths
and media outside the folder** (`Doc::normalizeComponentPath()` rewrites,
never reports; missing media is a load-log line); **dead lights in cues**
(dead is per venue, in the focus report only); **follow-spot binding**;
**moves in light** and every other tracking check (the Show Tree shows S/T/R
counts and MIB overrides, but flags nothing); every **VC** check.

---

## 3. The check catalog

Each check has an ID (`AREA-nn`), stable forever: acceptances are keyed by
it. Columns in each entry:

- **Detects / why** — what it finds and why that matters on stage.
- **Sev** — B (blocker), W (warning), N (note). Some checks raise a
  different severity per case; that is said.
- **Data** — what makes it feasible: the class or field it reads, or
  **new:** what is missing.
- **Cost** — *cheap* (a pass over data in memory), *moderate* (resolving
  every cue, or geometry per fixture), *new data* (needs something the show
  or the app doesn't have yet).
- **Says** — an example message, as the student would read it.
- **v1** — ✓ when in the first release (§7).

The *target* of a finding (what Fix jumps to and what acceptance is keyed
by) is named in brackets after the message.

### 3.1 Movement (MOV)

**MOV-01 Moves in light.** · W · moderate · v1 ✓
Detects: a head whose position changes between one played cue and the next
while it is lit in both (intensity above zero in the cue before, and the
position fade isn't hidden in a dark gap). Why: the audience watches the
beam slide across the stage; it's the most common thing a move-in-black
system exists to stop. Often deliberate (a drift with the soloist) — hence
accept. Data: `ShowTreeModel::resolve()` per list gives each cue's
`ResolvedLayer::values` by attribute ("Position", "Dimmer") with their
`Origin`; `playedBefore()` skips skipped cues; fixture positions from
`QLCPalette` Position values through `ShowTreeLooks`. Classic chasers: each
step's `Scene` values on channels of `QLCChannel::Pan`/`Tilt` group against
the intensity channels. Says: *"Spot 3 pans from DSL to USC while lit at
60 % — cue 11 → 12 (Ballad). Move it in the cue before, or accept a live
move."* [cue 12, fixture]

**MOV-02 Can't move in black.** · W · cheap · v1 ✓
Detects: a head that is dark in the cue before and repositioned in this
cue, but move-in-black can't hide it: MIB is off for the show, the list
(`CueList::moveInBlack` Off) or the cue (*Don't move in black into this
cue*), or the dark gap is shorter than the lead time
(`MarkPlanner::darkGapMs()`, default 800 ms) — e.g. the cue before is an
auto-follow with a 0.3 s hold. Why: it fades up while still travelling.
Data: the same resolve; `MarkPlanner::wantsMoveInBlack()` logic (lift its
rule into a pure function); cue fade/hold/follow times. Says: *"Wash 2–5
move into cue 23 with 0.3 s of dark — too short to move in black (needs
0.8 s). They'll be seen arriving."* [cue 23]

**MOV-03 Arrives late.** · W · cheap
Detects: a head that fades up in a cue whose position time (a layer's or
attribute's own fade, or a delay) is longer than its intensity fade in: it
reaches full while still moving. Why: same as MOV-01, caused by timing, not
by the cue before. Data: cue `fadeIn`, layer/attribute timing where the Show
Tree keeps it, `Scene` per-channel fades for classic functions. Says:
*"Spots 1–4 fade up in 1 s but take 3 s to reach the soloist — cue 8."*
[cue 8]

**MOV-04 Sweeps across the audience.** · W · moderate, new data
Detects: a position fade whose path, sampled through the fade (pan and tilt
interpolated as the engine fades DMX, not as the crow flies), lands in the
**house** — downstage of the apron front and below eye height plus a margin
— while lit. Also a fade through the house while dark is a Note (motors and
fans are heard; some designers care). Why: a beam across the audience's
eyes, mid-ballad. Data: plot geometry exists — fixture position and
orientation (`MonitorProperties::fixturePosition()`), `AimSolver` for
pan/tilt → world ray, the apron front (`apronPlatform()`, stage pieces).
**new:** a *house* zone. Derive it by default (everything downstage of the
deck front, floor to 2 m), optionally drawn as an **Audience** area on the
plot's Stage layer. Says: *"Mover 2's beam crosses rows A–F on the way from
cue 14 to 15 (lit, 2 s fade)."* [cue 15, fixture]

**MOV-05 Aimed into the house.** · W · moderate, new data
Detects: a lit head whose resting position in a cue lands in the house zone
(MOV-04) or at audience eye level — not a sweep, a parked beam. Deliberate
audience lights exist (a blinder, a "light the crowd" look): accept. Data:
as MOV-04. Says: *"Spot 6 sits in the audience at full in cue 30 (Finale) —
audience lights on purpose?"* [cue 30, fixture]

**MOV-06 The long way round.** · W · cheap
Detects: a pan or tilt fade that travels more than 180° of real rotation
when the same aim is reachable with a much shorter move (a 540° pan has two
DMX values for most directions). Why: QLC+ fades DMX values, so DMX 5 → 250
spins the head nearly all the way round — a sweep the designer never drew.
Data: `QLCPhysical` pan/tilt range (`focusPanMax()`, `focusTiltMax()`), the
two DMX values. Says: *"Spot 3 turns 300° from cue 4 to cue 5; the same
spot is 60° away the other way."* [cue 5, fixture]

**MOV-07 Fades that cross.** · N · moderate
Detects: two heads whose beams' paths cross mid-fade while both are lit
(a "scissor" nobody designed), or one head's position fade and another
cue's effect fighting over the same channels within a fade. Why: usually
harmless, occasionally the ugliest moment of a show. Data: MOV-04's sampled
paths. Says: *"Spots 1 and 2 cross centre stage during the fade into cue 9."*
[cue 9]

**MOV-08 Not marked before its first cue.** · W · cheap · v1 ✓
Detects: a head lit in the first played cue of a list (or the first cue
after a Block cue) whose position differs from where the cue before leaves
it — or, for the first cue, from the head's home — with no preshow cue to
put it there. Why: the first GO of the night sweeps every mover from home
to the opener, lit. Data: resolve; the list's first played cue. Says:
*"Cue 1 (Opener) lights Spots 1–8 but nothing sets their position before
it — they'll swing in from home. Add a preset cue 0.5, or mark them."*
[cue 1]

**MOV-09 Positioned in the dark, never revealed.** · N · cheap
Detects: a cue repositions a dark head and no later cue lights it before
its position changes again. Why: a dead move (wasted motor noise), or a cue
someone meant to light. The design-time twin of the runtime
`MarkPlanner::dangleFixtures()`. Says: *"Cue 17 aims Spot 5 at the piano;
nothing lights it there."* [cue 17]

**MOV-10 Beam blocked.** · N · moderate
Detects: a lit head in a cue whose beam stops on scenery, a drape, a truss
or a deck before its aim (`BeamOcclusion`), or tilts into the rig. Why: a
look that lights a border. Data: `BeamOcclusion::Model` (drapes, walls,
platforms, towers, trusses, pipes, stands) — exists. Says: *"Spot 4 in cue
22 hits the main drape 2 m from the light."* [cue 22, fixture]

**MOV-11 Follow spot not bound.** · W · cheap
Detects: fixtures with the follow-spot look (`Fixture::FollowspotLook`) or a
cue/hit that uses the follow spot (an Aim target that is the follow spot's
pin), when the show has no stick binding (`ProgrammerController::
isPanBound()` / `isTiltBound()`) and no input profile with pan/tilt slots on
any input universe (`autoBindFromProfile()`'s scan). The binding is the
design part; the pad being asleep is the footer's. Says: *"Spot 9 is the
follow spot but nothing drives it: no controller stick is bound."*
[fixture; Settings ▸ controller]

**MOV-12 Coarse without fine.** · N · cheap
Detects: a scene or cue that sets pan or tilt coarse but leaves the fine
channel to whatever was there, on fixtures that have one. Why: a few-degree
jitter between cues and between identical heads. Data: `QLCChannel`
`ControlByte` Fine. Says: *"Scene 03-02.01 sets Spot 2's pan but not its
pan fine."* [scene]

### 3.2 Palettes and looks (PAL)

**PAL-01 Two palettes, one channel.** · W · moderate · v1 ✓
Detects: in one scene, cue or hit, two palettes (or a palette and raw
values) set the same attribute on the same fixture, and which wins depends
on layer order rather than a deliberate child-layer exception. In the Show
Tree, two non-child layers on overlapping targets (a fixture in both
*Truss-Spots* and *All Spots*) with different values for the same
attribute; in a scene, `LookStackCheck`'s rules. Why: change the order (or
the group) and the look changes. Data: `ShowTreeModel::stageOf()` per cue
(unit → attribute → winning value) plus the layer targets;
`LookStackCheck::check()` for Build scenes; `FixtureGroup` membership.
Says: *"Cue 6: Spot 3 is in Truss-Spots (Red) and All Spots (Blue); Blue
wins because it's lower in the cue."* [cue 6, both layers]

**PAL-02 A look that never shows.** · W · cheap · v1 ✓
Detects: what `LookStackCheck` already reports — a look the one below it
always covers, an effect that matches none of its targets, a dimmer that
only reaches some heads. Why: the student thinks it's there. Data:
`LookStackCheck::Issue` (`Note`, `Warning`) — exists, mapped 1:1. Says:
*"Scene Verse wash: the Amber palette is always covered by Red below it."*
[scene, palette]

**PAL-03 A palette its targets can't play.** · W · cheap · v1 ✓
Detects: a Colour palette on fixtures with no colour channels, Gobo on
fixtures with no wheel, Position on fixed pars (no pan/tilt and not
fixed-aimable), Beam/zoom on fixtures without zoom — on every fixture the
layer reaches (all: warning; some: note, "plays on 4 of 6"). Data:
`QLCPalette::type()` against `QLCFixtureMode` channel groups. Says: *"Cue 3
puts Gobo Breakup on Pars 1–6, which have no gobo wheel."* [cue 3, layer]

**PAL-04 Unused palettes.** · N · cheap
Detects: a palette no scene, cue, hit, reusable layer, effect or other
palette uses. Why: the palette list is the show's vocabulary; dead words
confuse the next programmer. Data: `Doc::palettes()`; references from
`Scene`, `ShowTreeModel` looks, reusables (*Used by* in the Show Tree
already counts them). Says: *"Palette 'Teal (old)' isn't used anywhere."*
[palette]

**PAL-05 Palettes pointing at nothing.** · W · cheap · v1 ✓
Detects: a palette (or a look in a cue) that names a fixture, group or head
that no longer exists, or a group now empty; a Position palette aimed at a
stage target that was deleted. Why: it plays on nothing, silently. Data:
palette fixture/group values, `Doc::fixtureGroup()`, `StageTarget` ids.
Says: *"Position palette 'Soloist DS' aims at target 'Soloist' — deleted."*
[palette]

**PAL-06 Same group twice.** · N · cheap
Detects: two fixture groups with the same members (or one a renamed copy of
the other), and layers using both. Why: the "which group do I edit"
problem. Data: `FixtureGroup::fixtureList()`. Says: *"Groups 'Truss' and
'Truss spots' have the same 8 fixtures."* [groups]

**PAL-07 Hard values where a palette exists.** · N · moderate
Detects: raw channel values in a scene or layer that equal an existing
palette's value on those fixtures (Eos's "absolute data" audit). Why: when
the palette is updated for the venue, this cue won't follow. Data: scene
values vs `QLCPalette` values. Says: *"Cue 14 sets Wash 1–6 to the same
blue as palette 'Ballad Blue', by value. Use the palette so it updates
with it."* [cue 14, layer]

**PAL-08 Name says one colour, value another.** · N · cheap
Detects: a palette whose name's value word (`PaletteNaming`) no longer
matches its value — a name typed by hand, or a palette from before
names followed values. Data: `PaletteNaming::follow*()` returning a new
name. Says: *"Palette 'Red' is a dark blue."* [palette]

**PAL-09 Empty palette.** · N · cheap
Detects: a palette with no value, or a group set / palette folder with
nothing in it. Says: *"Palette 'Gobo 3' has no value."* [palette]

**PAL-10 Group-set ambiguity.** · W · cheap
Detects: a group set dropped on a scene or cue whose groups overlap (one
fixture in two groups of the set), so the set's own order decides which
look a fixture gets (board Q86 added a set's groups to a scene). Data:
`FixtureGroup` membership, the set's groups. Says: *"Group set 'Stage
areas' has DSL and Left in it; Spot 2 is in both."* [group set]

### 3.3 Tracking and cues (CUE)

All of these resolve each list once with `ShowTreeModel::resolve()` (pure,
no Doc) and read the result; they cost one resolve per list, shared.

**CUE-01 Tracks further than meant.** · W · moderate · v1 ✓
Detects: a value set in one cue that is still carried (`Origin::Tracked`,
same `fromCue`) across a song boundary, or more than *N* played cues (show
setting, default 15). A song boundary is a cue whose whole-number part
changes in a list that uses point cues, a cue marked *Block*, a
reusable "Blackout" cue, or — new — an explicit *Song starts here* flag
(§8). Why: the classic tracking surprise — a special from song 2 still up
in song 5. Data: `ResolvedValue::fromCue`, `Origin`. Says: *"Spot 4's
gobo from cue 12 (Ballad) is still in cue 31 (Finale) — 19 cues later,
across 2 songs."* [cue 12, layer]

**CUE-02 No block at the top of a song.** · W · cheap · v1 ✓
Detects: the first cue of a song (as CUE-01) that tracks
(`effectiveMode()` Track) and receives layers from the song before. Why:
editing the end of song 2 changes the start of song 3. Says: *"Cue 20
(top of Ballad) tracks 6 layers in from song 2. Make it a Block cue?"*
[cue 20]

**CUE-03 A cue that changes nothing.** · N · cheap · v1 ✓
Detects: a played cue whose stage (`stageOf()`) is the same as the cue
before's and whose timing, effects and sub-cues add nothing. Why: the SM
calls a GO and nothing happens — usually a cue whose content moved. Says:
*"Cue 7.5 looks exactly like cue 7."* [cue 7.5]

**CUE-04 Empty cue.** · N · cheap · v1 ✓
Detects: a cue with no layers, looks or includes. In a Block cue it's a
blackout — fine if called that, so the note says which. Says: *"Cue 15 is
empty — a blackout. Name it 'Blackout' or give it a look."* [cue 15]

**CUE-05 Auto-follow with no time.** · W · cheap · v1 ✓
Detects: an auto-follow whose follow, hold and fade are all zero (the cue
before is never seen); a chain of auto-follows that loops to its own start
with no hold (runs forever); an auto-follow off the last cue. Data: cue
`fadeIn`/`hold`/follow; chaser step times for classic chasers. Says:
*"Cue 9 follows on at once with no fade — cue 8 is never seen."* [cue 9]

**CUE-06 Numbers: duplicates and gaps.** · W / N · cheap · v1 ✓
Detects: two cues with the same number in a list (typing refuses it;
imports and hand-edited files can still have it) — W; a gap in whole
numbers ("1, 2, 5") or point cues out of order with their position — N.
Data: `ShowTreeModel` numbering (D23), `CueList::cues`. Says: *"Two cues
are numbered 14 in Act 1."* [list]

**CUE-07 Skipped cues left skipped.** · W · cheap · v1 ✓
Detects: every skipped cue (`Cue::skip`), and worse cases — a skipped cue
that is the list's base cue, that another cue includes, that a VC button or
show control jumps to, or the only Block between two songs. Why: skip is a
rehearsal tool; a skip left on is a missing cue on opening night. Says:
*"3 cues are skipped in Act 1: 4.5, 7, 12. Cue 12 is the base cue."*
[cues]

**CUE-08 Base cue problems.** · W · cheap · v1 ✓
Detects: a list whose layers Release but which has no base cue
(`CueList::baseCue` invalid); a base cue deleted, skipped, or after the
first cue that releases to it. Says: *"Cue 18 releases the side wash to the
base cue, but Act 2 has none — it goes dark."* [list]

**CUE-09 Never released.** · N · cheap
Detects: a layer set early in a Track list that is carried to the last cue
and never released or blocked. Why: the end of the show still has the
preshow's haze or the walk-in special. Says: *"The house-left special from
cue 2 is still on in the last cue."* [cue 2, layer]

**CUE-10 Release with nothing to release.** · N · cheap
Detects: a layer marked Release in a cue where nothing was tracking on that
target. Says: *"Cue 11 releases Truss-Spots, which nothing set."* [cue 11]

**CUE-11 Include loops and dangling references.** · W / B · cheap · v1 ✓
Detects: `ResolvedCue::skippedIncludes` (an include that would loop) — W;
an include of a deleted cue, a reusable layer or cue that no longer exists,
a sub-cue list whose parent is gone — B. Data: resolve, `ReusableLayer`,
`ReusableCue`. Says: *"Cue 4 includes cue 9, which includes cue 4 — left
out."* [cue 4]

**CUE-12 Generated functions edited, originals still used.** · W · cheap ·
v1 ✓
Detects: (a) an original kept for Restore original
(`CueList::originals`, `ShowTreeController::originalFor()`) that is still
fired by a VC button, a timeline track or show control instead of the list;
(b) an original edited after the import (its content no longer what was
filed); (c) a generated function (`ShowTreeController::isLocked()`) whose
content differs from what the tree would bake — edited outside the tree in
an older build, or by hand in the XML. Why: edits that change nothing that
plays, or a show that plays the old chaser. Data: the controller's locked
and original queries; a rebake to compare. Says: *"VC button 'Act 1' still
runs the original chaser 'Act 1', not the Show Tree list made from it."*
[VC button; Show in Show Tree]

**CUE-13 A list nothing can play.** · W · cheap · v1 ✓
Detects: a cue list that is not its production's main list, is followed by
no Cue Runner, is on no VC Cue List, and has no show-control or timecode
route — it can't be GOne in Operate. Data: `Production::mainList`,
`VCCueRunner::follow()`, VC Cue Lists' chaser ids, `ShowControl`. Says:
*"List 'Encore' can't be played: it isn't the main list and nothing on the
Virtual Console runs it."* [list]

**CUE-14 Odd timing.** · N · cheap
Detects: a fade over 60 s (a typo for 6.0?), a fade out longer than the
next cue's follow, a hold that ends a list on an auto-follow to nothing, a
delay longer than the fade. Says: *"Cue 3's fade is 300 s. Meant 3.00?"*
[cue 3]

**CUE-15 Sub GO with no Sub GO.** · N · cheap
Detects: a cue with sub-cues on Sub GO (`SubCues="SubGo"`) when nothing
offers Sub GO — no Cue Runner shows it, no key or input is bound to it.
Says: *"Cue 10 has 3 sub-cues on Sub GO, but no button or key sends Sub
GO."* [cue 10]

**CUE-16 Venue focus cue without calls.** · W · cheap
Detects: a cue marked *Make this a venue focus cue* (`FocusCallCue`) when
the show has no focus calls, or none whose fixtures are in this production.
Says: *"Cue 0.1 is a venue focus cue, but the show has no focus calls."*
[cue 0.1]

### 3.4 Hits and Virtual Console (VC)

The Virtual Console is UI-only (`ui/src/virtualconsole/`); see §5.3 for how
a headless run reads it.

**VC-01 Hits that fight in the same scope.** · W · moderate · v1 ✓
Detects: two hits in the same scope — the same hit folder, or hits on one
VC button or in one VC frame that is not a solo frame — that set the same
attribute on the same fixtures with different values, and can be on
together (Toggle buttons, not exclusive). Why: "last started wins" for
colour and position, so the stage depends on the order they were pressed
(board Q154/Q157: hits are scoped by where they live in the Show Tree; a
VC button can fire several hits; no code for this yet). Data: each hit's `ResolvedCue`
(`ShowTreeModel::CueList::looks`), `stageOf()`; the VC button → function
map. Says: *"Hits 'Red stab' and 'Blue stab' in folder 'Chorus' both set
the colour of All Spots and can be on at once — whichever was pressed last
wins."* [both hits]

**VC-02 A hit that overrides a wider one.** · N · moderate
Detects: a hit in a narrower scope that sets what a hit in a wider scope
(or the running list) sets, per the Q157 scope rule — reported as
information, so the student knows which one wins. Says: *"Hit 'Solo spot'
(folder Ballad) overrides 'Warm wash' (all hits) on Spot 3's colour."*
[hit]

**VC-03 Hits nobody can fire.** · W · cheap · v1 ✓
Detects: a hit whose collection no VC button, key, input or show control
fires (*Fired by* is empty). Says: *"Hit 'Confetti' isn't on any button.
Make VC page?"* [hit]

**VC-04 Buttons with no function.** · W · cheap · v1 ✓
Detects: a VC button with no function, or one whose function was deleted;
a slider in Level mode on channels of fixtures that are gone; a Cue List
widget on a deleted chaser. Says: *"Button 'Haze on' (page Busk) does
nothing: its function was deleted."* [VC widget]

**VC-05 Same key twice.** · W · cheap · v1 ✓
Detects: two VC widgets (buttons, Cue Runner actions, Cue List actions)
with the same key on the same page or always active; a VC key that is also
an app shortcut in Operate (⌘↩ GO, Space). Data: `VCButton::keySequence()`,
`VCCueRunner::keySequence(Action)`, the app's shortcut table. Says: *"Key
B is on both 'Blackout' and 'Blue stab' (page Busk)."* [both widgets]

**VC-06 Same MIDI or input twice.** · W · cheap · v1 ✓
Detects: two widgets bound to the same input universe and channel (a fader
or pad) with different jobs. Data: `VCWidget` input sources
(`QLCInputSource` universe, channel). Says: *"MIDI note 36 on the APC
fires both 'Strobe' and 'Cue 1 GO'."* [both widgets]

**VC-07 Inputs from a universe with no input.** · W · cheap
Detects: widgets bound to an input universe that has no input patch or
profile in the show (the patch is design; the device asleep is runtime).
Says: *"8 buttons listen to universe 3, which has no input patched."*
[Connections ▸ universe 3]

**VC-08 Cue Runner on no list.** · W · cheap · v1 ✓
Detects: a Cue Runner following a deleted list (`VCCueRunner::follow()`
invalid), or following "the current production" in a show with no
production. Says: *"Cue Runner 'Main GO' follows a cue list that was
deleted."* [VC widget]

**VC-09 A VC Cue List on an upgraded chaser.** · W · cheap
Detects: a VC Cue List widget still on a chaser the Show Tree took over
(Import in place moves them; a widget added later doesn't). Overlaps CUE-12
(a); kept separate so the message can offer *Point it at the list*. Says:
*"Cue List 'Act 1' runs the old chaser. Point it at the Show Tree list?"*
[VC widget]

**VC-10 Duplicate hits.** · N · cheap
Detects: two hits that put exactly the same thing on stage
(`cueContent()`). Says: *"Hits 'Wash 1' and 'Wash copy' are the same."*

**VC-11 Hard to reach.** · N · cheap
Detects: a widget off its frame or page, zero-size, hidden under another,
or on a page with no way to get to it. Says: *"Button 'Fog' is outside
frame 'Effects' and can't be clicked."* [VC widget]

**VC-12 A speed control with nothing on it.** · N · cheap
Detects: a Speed Dial or tap bound to a speed master (`SpeedMasters`) that
no effect, look, chase or collection uses. Says: *"Tap 'Speed 3' drives
nothing."* [VC widget]

### 3.5 Show control and timecode (SC)

**SC-01 Remote cues that don't exist.** · W · cheap
Detects: MSC/OSC routes (`ShowControl`) naming cue numbers or lists that
aren't in the show; QLab cue numbers duplicated across lists the same
route can reach. Says: *"QLab fires 'GO 14.5' on Act 1, which has no cue
14.5."* [Show control]

**SC-02 Timecode with no source.** · W · cheap
Detects: a production set to follow timecode (`Production::timecode`), or a
timeline following it, when the show picks no timecode source
(`TimecodeSource`). Says: *"Act 2 follows timecode but none is set up."*
[Connections ▸ Devices]

**SC-03 Timeline pointing at nothing.** · W · cheap
Detects: a Shows timeline track or item on a deleted function; two tracks
playing the same function overlapping; an item past the show's end. Data:
`Show`, `Track`, `ShowFunction`. Says: *"Show 'Preshow' plays a scene that
was deleted (track 2, 00:01:30)."* [Shows tab]

### 3.6 Patch and outputs (PAT)

**PAT-01 Two fixtures on the same channels.** · B · cheap · v1 ✓
Detects: overlapping addresses in a universe among patched fixtures. Data:
`Fixture::universe()`, `address()`, `channels()`; the same overlap test the
patch list and readiness use. Says: *"Spot 3 (1/101–116) and Wash 1
(1/110–123) share channels 110–116."* [Patch dialog, both rows]

**PAT-02 Past the end of the universe.** · B · cheap · v1 ✓
Detects: a fixture whose footprint runs past 512 (only an import or a hand-
edited file can make it). Says: *"LED bar 4 starts at 1/500 and needs 30
channels."* [Patch]

**PAT-03 Unpatched, but used.** · W · cheap · v1 ✓
Detects: a staged (inventory) or unpatched fixture that cues, hits or
scenes light. Unused and unpatched is fine (inventory-first) — a Note at
most. Says: *"Spot 9 is in 12 cues but isn't patched."* [Patch dialog]

**PAT-04 On channels the venue keeps.** · B · cheap · v1 ✓
Detects: a fixture on not-ours channels (`Doc::isOnNotOurs()`,
`Universe::notOurs`) — refused when patching now, possible in an older file
or a venue changed after. Says: *"Wash 3 is on 1/401–412, which Lincoln HS
keeps (not ours)."* [Patch, venue page]

**PAT-05 Venue clashes.** · B / W · cheap · v1 ✓
Detects: `VenueRig::clashes()` for the applied venue and, as a dry run, for
every venue in the sidecar: Overlap (B), Not ours (B), Slot — a house
fixture in a Tour slot or a tour fixture in a House slot (W). One finding
per clash, per venue. Says: *"At Lincoln HS, house Par 12 and tour Spot 3
share 2/1–8."* [venue page]

**PAT-06 A universe with fixtures and no output.** · W · cheap · v1 ✓
Detects: a universe with patched fixtures and no output patch in the show
(nor in the applied venue's patch), when the show patches any output at
all. With no outputs anywhere the show is "preview only" and that's a Note.
Data: `Universe::outputPatch()`, `setShowOutputBase()`. Says: *"Universe 3
has 14 fixtures and goes nowhere."* [Connections ▸ universe 3]

**PAT-07 Two universes on one network universe.** · W · cheap
Detects: two universes patched to the same Art-Net/sACN universe on the
same interface (or the same node), so one overwrites the other. Data:
`OutputPatch` parameters (Art-Net universe, sACN universe, IP). Says:
*"Universes 2 and 5 both send Art-Net 0:0:1 to 172.18.2.40."*
[Connections]

**PAT-08 Network output with no rig network.** · N · cheap
Detects: network outputs in a show with no rig networks set (`RigNetwork`),
or an output patch whose address is outside the show's rig networks — it
will always be held. The hold itself is runtime. Says: *"This show sends
Art-Net but names no rig network; on a school network it would go out
there too."* [Settings ▸ General ▸ Network]

**PAT-09 Network cabling gaps.** · W · cheap
Detects: what the Network Report (`NetworkReport`, `NetworkTopology`)
already derives as design problems: a fixture on a node port that carries
no universe, a port carrying more universes than it can, a run longer than
its cable allows, a universe no node carries. Says: *"Node 2 port B
carries universe 4, but its fixtures are on universe 5."* [Lighting Studio
▸ network]

**PAT-10 WLED set up differently from the show.** · W · cheap
Detects: the WLED config check's design results (`WledConfigCheck`): LED
count, segments or start universe in the show differing from what the
device was last read as. Only the saved comparison; a device not answering
is runtime. Says: *"WLED 'Choir riser' has 120 LEDs in the show, 144 on the
device when last read."* [Connections]

**PAT-11 Input and feedback loops.** · N · cheap
Detects: an input universe whose feedback goes out on the same line it
listens to, or an output and input on the same MIDI port for unrelated
jobs. Says: *"Universe 6's MIDI feedback goes back to its own input."*

### 3.7 Fixtures and definitions (FIX)

**FIX-01 Missing definition.** · B · cheap · v1 ✓
Detects: a fixture whose definition wasn't found (loaded as a placeholder
/ generic), in the show or in the venue's house rig. Data: the same test
the footer uses; `ShowEmbedding` says whether the show carries a copy.
Says: *"No definition for 'Warmdance XL-450' (4 fixtures). Install it or
embed it."* [Fixtures ▸ Definition Updates]

**FIX-02 Definition problems.** · W · cheap · v1 ✓
Detects: `FixtureDefCheck::check()` Issues on definitions the show uses —
Error as W, Warning as N — and a definition that loads but loses parts of
itself (`ParseProblem::loads`). Says: *"'Branson LED SPOT' mode 'Basic'
names a channel that doesn't exist."* [Fixture editor]

**FIX-03 Older definition than the library's.** · N · cheap
Detects: what Definition Updates lists (`FixtureDefUpdates`) for the show:
the show's or user folder's copy is older than the bundled or newer one.
Says: *"3 fixtures use an older 'ColorSource Spot jr' definition."*
[Definition Updates]

**FIX-04 Mode or type mismatch.** · W · cheap
Detects: a fixture's mode not present in its definition (fallen back); a
venue correction recorded for another definition (the fixture-type check
in `VenueCorrections`: a swapped light needs focus); a house fixture
filling a group that the cues use with a mode lacking the attributes the
cues set (no colour mixing on an RGB-cued group). Says: *"Lincoln HS's
house Par 4 fills 'Front wash' in 8-bit dimmer mode; the cues set colour
there."* [fixture, venue page]

**FIX-05 Not on the plot.** · W / N · cheap · v1 ✓
Detects: a patched fixture with no place on the plot
(`MonitorProperties::containsFixture()` false). W when it's in a focus call,
an aim (Position palette on a target) or the power plan — those need where
it is; N otherwise. Says: *"Spots 7 and 8 aren't on the plot; focus call
'Truss spots' can't aim them."* [Lighting Studio ▸ Inventory]

**FIX-06 On the plot but not hung.** · N · cheap
Detects: an overhead-type fixture on the plot with no mount (not on a
truss, pipe, boom, stand or tower) at floor height. Says: *"Spot 4 is on
the floor at DSC, not on any truss."* [plot]

**FIX-07 No lens or beam data.** · N · cheap
Detects: a light (`FixtureDefCheck`'s *is a light at all* test) with no
beam angle or lens range: the Studio can't draw its beam and Quick focus
can't zoom it. Says: *"'Generic RGB par' has no beam angle; the plot
guesses 25°."* [Fixture editor]

**FIX-08 Dead lights still used.** · W · cheap · v1 ✓
Detects: a light marked dead in the applied venue
(`VenueCorrections::isDead()`, board Q140) that a cue or hit lights —
worst when it's the only light on a layer. Says: *"Spot 3 is marked dead at
Lincoln HS; cues 4 and 7 light only it on the soloist."* [cues, venue]

**FIX-09 Fixture used nowhere.** · N · cheap
Detects: a patched fixture no scene, cue, hit, effect, group or palette
reaches (LightWright's "unused" list). Says: *"Par 17 is patched but
nothing uses it."* [fixture]

**FIX-10 In no group.** · N · cheap
Detects: a fixture in no fixture group, in a show whose cues are written on
groups (the Show Tree's way). Teaching: groups are the contract (VENUES.md
§4). Says: *"Wash 9 isn't in any group, so no group cue can reach it."*
[Fixture Manager]

**FIX-11 Head layout holes.** · W · cheap
Detects: a group used by a matrix effect with members not placed in its
head layout, or cells for heads of fixtures no longer in the group. Data:
`FixtureGroup` head layout, `GroupHead`. Says: *"Group 'Riser pixels' has
2 bars not in its layout; the matrix skips them."* [Fixture Manager ▸
layout]

**FIX-12 Owner not said.** · N · cheap
Detects: in a show with venues or House slots, fixtures whose owner
(`RigOwner`) is unset, so the clash check and house swap can't tell. Says:
*"12 fixtures have no owner; with venues in use, say Tour or House."*
[Fixture Properties]

**FIX-13 Paperwork gaps.** · N · cheap
Detects: what a LightWright audit flags: a fixture with no position
(truss/pipe name), unit number, purpose or colour note, when the show has
started filling those (patch labels, `PatchLabels`). Says: *"4 fixtures
have no unit number; the patch list will be blank there."* [Patch labels]

**FIX-14 Duplicate fixture names.** · N · cheap
Detects: two fixtures with one name (refused now; old shows have them).
Says: *"Two fixtures are called 'Spot 1'."* [fixtures]

### 3.8 Venues and focus (VEN)

**VEN-01 Focus calls with no target.** · W · cheap · v1 ✓
Detects: a focus call with no stage target or a deleted one, or with no
groups or fixtures (`FocusCall::groups`, `fixtures`, its target
`FocusSession::Reference`). Says: *"Focus call 'Truss spots → ?' has no
target."* [Edit focus calls]

**VEN-02 Unaimable lights in a focus call.** · W · cheap · v1 ✓
Detects: lights in a focus call that can't be aimed — no pan/tilt and not
a fixed light Quick focus can place (Q102), or not on the plot (FIX-05).
Quick focus excludes them at run time and says so; this says it before.
Says: *"Focus call 'AT-W-ALL' has 2 pars that can't be aimed; they'll be
left out."* [focus call]

**VEN-03 Targets off the stage.** · W · cheap
Detects: a stage target (`StageTarget`) outside every deck and platform
and the apron, or below the floor; a target a focus call uses that sits
inside scenery. Data: target position against `StagePlatform`s and the
apron. Says: *"Target 'Soloist DS' is 1.2 m downstage of the apron."*
[plot]

**VEN-04 A target the light can't reach.** · W · moderate
Detects: a focus call's target outside a head's pan/tilt range from where
it hangs (`AimSolver`), or blocked on the way (`BeamOcclusion`). Says:
*"Spot 6 can't tilt far enough to reach 'Piano' from the FOH truss."*
[focus call, fixture]

**VEN-05 Swapped fixtures need focus.** · W · cheap · v1 ✓
Detects: corrections in the applied venue recorded for a different
definition than the fixture now has (the existing fixture-type check). Says:
*"Spot 2 was a Betopper LM70 when Lincoln HS was focused; it's an ADJ
Focus Spot now. Refocus it."* [Venue focus]

**VEN-06 Lights never focused here.** · N · cheap
Detects: moving heads in focus calls with no correction and no check
(`VenueCorrections::Check`) in the applied venue, when others have one.
Says: *"6 of 8 truss spots are focused for Lincoln HS; Spots 7 and 8
aren't."* [Venue focus]

**VEN-07 An old focus.** · N · cheap
Detects: the applied venue's newest check older than a show setting
(default 60 days), or older than changes to the rig (a fixture moved on the
plot since). Says: *"Lincoln HS was last focused 4 months ago, before
Spots 1–4 moved to the FOH truss."* [venue manager]

**VEN-08 Corrections for lights that are gone.** · N · cheap
Detects: venue corrections, checks or dead marks for fixture ids no longer
in the show. Says: *"Lincoln HS has corrections for 3 fixtures no longer
in the show."* [venue manager]

**VEN-09 Venue sidecar missing.** · W · cheap · v1 ✓
Detects: the show says a venue is applied (or has venues) but the sidecar
file beside it isn't there or can't be read. Says: *"The show uses venue
'Lincoln HS' but its venue file isn't beside the show."* [File]

**VEN-10 Practice venue left on.** · W · cheap · v1 ✓
Detects: a scramble-practice venue applied, or a `<FixtureRig>` entry a
practice added (TEST_MAP finding) with no real venue. Says: *"The practice
venue is still applied; the lights are scrambled."* [venue manager]

**VEN-11 Unnamed or placeholder venue.** · N · cheap
Detects: venues named "New venue", "Venue 2", "Copy of …", or two venues
with names differing only in case. Says: *"Venue 'New venue' — give it the
room's name."* [venue manager]

**VEN-12 A group the house can't fill.** · W · cheap
Detects: a group the cues use whose house members the venue's `VenueRig`
doesn't fill (a group fill left empty), so cues on it play only the tour
part — or nothing — here. Says: *"At Lincoln HS nothing fills 'Front
wash'; cues 1–30 light nothing from the front."* [venue page]

**VEN-13 Moving heads in no focus call.** · N · cheap
Detects: moving heads on the plot that no focus call includes, in a show
that has focus calls. Says: *"Spots 9–10 aren't in any focus call."*
[Edit focus calls]

### 3.9 Effects and performance (FX)

**FX-01 Too heavy for the machine it plays on.** · W / B · new data · v1 ✓
Detects: the per-tick cost of what plays at once — at its heaviest cue or
hit combination — against the tick budget of the show's **target
machine**. Cost = Σ over running matrix/script effects of (the script's
cost per head per tick on that machine × heads × scope factor for *Each
box, staggered*), plus a per-universe output cost. W over 75 % of the
budget, B over 100 %. Why: the Pi 5 missed its 12 ms budget on four
scripts on eight XL-450s (`ead57dc60`); the operator sees stutter, not an
error. Data: `EffectInstance` (what each look plays and on how many
heads), `RGBMatrix`, `EffectScript`; the 12 ms budget from
`EffectInstance_Test::matrixEffectsKeepTimeOnEightBoxes`. **new:** (1) a
*plays on* setting per show (This computer, Raspberry Pi 5, Pi 4, a
Windows laptop) kept in `<ShowReview>`; (2) a cost table per script per
platform, produced by that timing test in a *measure* mode
(`QLC_MATRIX_MEASURE=path`) on each target and shipped as
`resources/effectscripts/costs.json`. Unknown scripts (the user's own) are
costed by a short dry run on this machine, scaled. Says: *"Cue 27 (Megamix)
runs Fire, Starfield and Kaleidoscope on 8 XL-450s: about 15 ms a tick on a
Raspberry Pi 5, over its 12 ms. Expect stutter there."* [cue 27]

**FX-02 RGB matrix on non-RGB lights.** · W · cheap · v1 ✓
Detects: a matrix effect on a group whose heads have no RGB, CMY or colour
mixing (dimmer-only heads get intensity only; heads with neither, nothing).
Data: group heads' channels (`QLCFixtureHead`). Says: *"Matrix 'Rainbow' on
'Truss spots': 6 of 8 heads have no colour mixing."* [effect]

**FX-03 Effects that don't move.** · W · cheap · v1 ✓
Detects: an EFX with zero width and height or zero speed, a matrix or
script with speed or duration zero, an effect on a speed master that is
paused or at 0 BPM with no tap. Says: *"EFX 'Circle' has size 0; the
lights just sit there."* [effect]

**FX-04 EFX on lights that can't move.** · W · cheap
Detects: an EFX on fixtures without pan/tilt (LookStackCheck's "effect
matches none" for the position case, also for classic EFX functions).
Says: *"EFX 'Wave' runs on Pars 1–6, which can't move."* [effect]

**FX-05 Too many effects on one light.** · N · cheap
Detects: more than *N* (default 3) effects at once on one fixture's
channels in a cue or hit, or two effects driving the same attribute (only
one shows). Says: *"Spot 2 runs 4 effects at once in cue 14; only the last
colour effect shows."* [cue 14]

**FX-06 Audio effects with no audio input.** · W · cheap
Detects: an audio-reactive effect (Matrix EQ, WLED sound-reactive) in a
show with no audio input configured. Says: *"'Matrix EQ' follows audio but
no audio input is set — it'll play simulated beats."* [Inputs/Outputs]

**FX-07 Script that won't load.** · B · cheap · v1 ✓
Detects: an RGB or effect script a function uses that fails to load or
isn't present (`RGBScriptsCache`, `EffectScriptCache`; the design half of
the footer's `effects.failed`, `Doc::effectFailures()`), or a property
value the script no longer offers. Says: *"Matrix 'Sparkle' uses script
'my-sparkle.js', which isn't on this computer or in the show."* [effect]

**FX-08 More universes than the machine sends well.** · N · new data
Detects: universes × output rate over what the target machine sustained in
the stress tests (memory: stress-test results). Data: **new:** a small
per-platform table beside FX-01's. Says: *"This show sends 24 universes;
a Pi 4 has been measured to 16 at 44 Hz."* [Connections]

**FX-09 Media a Pi can't play.** · N · cheap
Detects: Video functions, or audio formats the target platform's
backend doesn't decode. Says: *"Video 'Intro.mov' on a Raspberry Pi:
H.265 isn't hardware-decoded there."* [function]

### 3.10 Power and safety (PWR)

**PWR-01 A circuit over its breaker.** · B / W · moderate · v1 ✓
Detects: for each cue and hit (and their worst combination with the
running list), the draw per circuit: `PowerDistribution::
functionFixtureWatts()` per fixture (scenes resolve values and palettes;
collections HTP-merge) plus standby, summed per `PowerCircuit` against its
rating. B over 100 %, W over 80 % (the continuous-load rule; the
derated limit the Power dialog's scene check, `PowerDistributionWidget::
slotSceneCheck()`, already uses — lift it to the engine). Says:
*"Cue 31 (Finale) draws 18.4 A on circuit 2A (20 A breaker) — over 80 %."*
[cue 31, Power distribution]

**PWR-02 Everything at full is over the supply.** · N · cheap
Detects: the whole rig at full (Highlight, Park at full, a work light test)
over a source's capacity. Says: *"All lights at full is 52 A on the 40 A
company switch. No cue does that, but Highlight on everything would."*
[Power distribution]

**PWR-03 Lights on no circuit.** · W · cheap · v1 ✓
Detects: once power is set up (any source), fixtures assigned to no
circuit (`PowerDistribution::circuitOf()` −1). Says: *"9 fixtures are on no
circuit, so the power check can't count them."* [Power distribution]

**PWR-04 Capped below the look.** · W · cheap
Detects: a cue that asks a fixture for more current than its cap
(`FixturePower` max amps) — the limiter will dim it, so the look plays at a
fraction. Says: *"Cue 5 asks LED bar 3 for 6 A; its cap is 4 A, so it
plays at about 70 %."* [cue 5, fixture]

**PWR-05 Flashing faster than is safe.** · W · moderate · v1 ✓
Detects: anything that flashes intensity more than **3 times a second**
(the photosensitive-epilepsy guidance used by broadcasters and WCAG) over a
large part of the stage: a strobe channel at a capability with a known
frequency over 3 Hz (`QLCCapability` presets `StrobeFrequency`,
`StrobeFreqRange` with Hz in its resources); a chase or matrix alternating
bright and dark with steps under 333 ms; an effect strobing faster than
that. A strobe with no Hz in its definition (`StrobeSlowToFast`) above
half its range is reported as *may be*. Why: schools and show choirs play
to families; many venues require a posted warning. Accept with the note
*"Strobe warning posted in the lobby"*. Says: *"Cue 40 strobes Wash 1–6 at
about 10 Hz (definition: 1–20 Hz at this value). Post a strobe warning, or
slow it under 3 Hz."* [cue 40]

**PWR-06 Lights that ignore Blackout.** · W · cheap · v1 ✓
Detects: output rules (`OutputRuleKeeper`) *Ignore Blackout* or *Ignore
the Grand Master* on a fixture that is a light (not a relay, work light
or house light by its plot symbol or definition type). Why: Blackout is a
safety control; a stage light that ignores it surprises the operator. Says:
*"Spot 1 ignores Blackout and the Grand Master. Work light on purpose?"*
[Output Rules]

**PWR-07 A relay in a fade or an effect.** · W · cheap
Detects: a fixture with *Switch (relay)* in a cue with a fade crossing
50 %, a chase, or a matrix — the relay clatters or switches mid-fade. Says:
*"Relay 'Fog power' is in chase 'Lightning'; it'll click on and off with
each step."* [chase]

**PWR-08 Haze or fog with no off.** · N · cheap
Detects: a hazer or fog machine (definition type Hazer/Smoke) set on in a
cue and never set off by the list's last cue. Says: *"The hazer comes on
in cue 2 and is never turned off."* [cue 2]

### 3.11 Files and portability (FILE)

**FILE-01 Media outside the show folder.** · W · cheap · v1 ✓
Detects: audio, video, plot pictures and scenery images, and any other
component path that resolves outside the show's folder or is absolute
(`Doc::normalizeComponentPath()` would leave it absolute). Why: it works
on this laptop and nowhere else. Says: *"Audio 'Overture.wav' is in
/Users/branson/Downloads — Collect for Transfer copies it into the show
folder."* [File ▸ Collect for Transfer]

**FILE-02 Media that isn't there.** · B · cheap · v1 ✓
Detects: a component path that doesn't exist (resolved with
`Doc::denormalizeComponentPath()`). Says: *"Video 'Intro.mov' is
missing."* [function]

**FILE-03 Definitions not carried with the show.** · W · cheap · v1 ✓
Detects: a `.qxw` show (no embedding) using definitions, scripts or input
profiles from this computer's user folder — it opens elsewhere without
them. A `.qlcc` embeds them (`ShowEmbedding`); there, a note when the
embedded copy and this computer's differ (`ComputerNewer`). Says: *"This
.qxw uses 3 fixture definitions only this computer has. Save as .qlcc to
carry them."* [File ▸ Save As]

**FILE-04 Input profiles not carried.** · W · cheap
Detects: input patches using a user input or HID profile the show doesn't
carry. Says: *"Universe 6 uses input profile 'APC mini (Branson)', only on
this computer."* [Connections]

**FILE-05 qlcconsole features in a .qxw.** · N · cheap
Detects: a `.qxw` show using fork-only features (Show Tree, venues,
stage pieces) that stock QLC+ will drop if it saves it. Says: *"This show
has a Show Tree; opened and saved in QLC+, it would be lost."*

**FILE-06 Saved in a synced folder.** · N · cheap
Detects: a show file in a cloud-synced folder (the cloud-storage safety
work knows these folders; `storage.onlineonly` in the footer is its
runtime side). It's about where the file lives, not the moment. Says:
*"The show is in iCloud Drive; keep a local copy for the show."*

### 3.12 Naming and teaching (NAME)

**NAME-01 Names off the show's pattern.** · N · cheap · v1 ✓
Detects: a scene or collection whose siblings in its folder follow a
pattern (`SS-PP.II-Description`, learned from the folder as the duplicate
naming already does in `FunctionManager`) and it doesn't. Says: *"'verse
wash' sits among 03-01.01-…, 03-01.02-…; name it 03-01.03-Verse wash?"*
[function]

**NAME-02 Default and copy names.** · N · cheap · v1 ✓
Detects: "New Scene", "Scene 12", "Copy of …", "Group 1", "Palette 3",
"New hit", unnamed cues in a main list. Says: *"4 functions still have
default names: New Scene 3, New Scene 4, …"* [functions]

**NAME-03 Same name, same folder.** · N / W · cheap
Detects: two functions with one name in one folder (N); W when both are on
VC buttons or in pickers where only the name tells them apart. Says:
*"Two scenes in 'Act 1' are called 'Wash'."* [functions]

**NAME-04 Unused functions.** · N · cheap
Detects: scenes, chasers, EFX and matrices no cue, collection, chaser, VC
widget, timeline or show control uses (the Show Tree's own and kept
originals excluded). MA's "unused objects". Says: *"12 scenes aren't used
anywhere."* [Functions]

**NAME-05 Notes still saying to-do.** · N · cheap
Detects: cue, hit and step notes containing *TODO*, *FIX*, *??*, *TBD*.
Says: *"Cue 12's note says 'FIX gobo'."* [cue 12]

**NAME-06 Empty folders.** · N · cheap
Detects: empty function, palette or hit folders. Says: *"Folder 'Old' is
empty."*

### 3.13 Footer readiness, folded in (FTR)

The footer's readiness list mixes design and runtime items. The design ones
are already checks above; Show Review reads them through the same code and
lists them under their catalog ID, so a student doesn't see the same
problem twice under two names. The runtime ones stay in the footer only.
The full mapping, per footer entry, is in §2.

### 3.14 Summary

| Group | Checks | v1 |
|---|---|---|
| Movement (MOV) | 12 | 3 |
| Palettes and looks (PAL) | 10 | 4 |
| Tracking and cues (CUE) | 16 | 11 |
| Hits and VC (VC) | 12 | 6 |
| Show control and timecode (SC) | 3 | 0 |
| Patch and outputs (PAT) | 11 | 6 |
| Fixtures and definitions (FIX) | 14 | 4 |
| Venues and focus (VEN) | 13 | 5 |
| Effects and performance (FX) | 9 | 4 |
| Power and safety (PWR) | 8 | 4 |
| Files and portability (FILE) | 6 | 3 |
| Naming and teaching (NAME) | 6 | 2 |
| **Total** | **120** | **52** |

Checks that need **new data** (everything else reads what the show
already has):

- **MOV-04, MOV-05** (and MOV-07's crossing test): a *house* zone —
  derived from the deck front by default, optionally an Audience area on
  the Stage layer.
- **FX-01, FX-08**: a *plays on* machine per show and a per-platform cost
  table for every bundled script, measured by the existing timing test in
  a measure mode on each target (Pi 5, Pi 4, Intel Mac, Windows).
- **CUE-01, CUE-02**: song boundaries. Heuristics work without data; an
  explicit *song starts here* cue flag would make them exact.
- **Acceptance** itself: the `<ShowReview>` element (§4.3), and a stable
  key for focus calls, which today are identified only by their position in
  the list (`FocusCalls::calls()`) — give each an id when acceptance lands.

---

## 4. UX sketch

### 4.1 Where it lives

- **A Review chip in the footer**, beside Ready: *Review · 2 ⛔ 5 ⚠* (blockers
  and warnings not fixed or accepted; notes aren't counted on the chip). Grey
  *Reviewed* when nothing is open. It shows the last result; it doesn't run
  on its own unless live review is on (§4.2). One of the footer chips
  Settings ▸ Appearance can hide.
- **File ▸ Review Show… (⇧⌘R)** and a **Review** button on the Show Tree
  header and the Fixtures tab toolbar open the panel.
- **The Review panel**: a dock (like Level… and Colour… in the Show Tree),
  or a window in Classic QLC+ view.

```
┌ Show Review ─────────────────────────────── Re-check ⟳ ┐
│ Plays on: [Raspberry Pi 5 ▾]   Last run 14:02 · 0.4 s  │
│ [All areas ▾] [Open ▾] [Search…            ]           │
│ ⛔ Blockers (2)                                         │
│   PAT-01  Spot 3 and Wash 1 share 1/110–116      Fix ▸ │
│   FILE-02 Video 'Intro.mov' is missing           Fix ▸ │
│ ⚠ Warnings (5)                                          │
│ ▸ Movement (2)                                          │
│   MOV-01  Spot 3 pans DSL→USC lit at 60 %, 11→12        │
│           Fix ▸  Accept…  Why?                          │
│ ▸ Hits (1) …                                            │
│ ⓘ Notes (14)                                  [show ▾]  │
│ ✓ Accepted (3)                                [show ▾]  │
│   MOV-01  Ballad drift, cue 18 — "on purpose: follows   │
│           the soloist" · Branson · 10-25                │
│   ↺ PWR-05 Cue 40 strobe — accepted, CHANGED since      │
└─────────────────────────────────────────────────────────┘
```

- **Grouped** by severity, then area; **filter** by area, severity, state
  (Open, Accepted, Changed since accepted, All) and text; the selection
  filters to findings about it (*only this cue*), a toggle.
- **Fix ▸** jumps: the Show Tree cue (and the layer), Fixture Properties,
  the Patch dialog on the row, Build on the palette or scene, the VC widget
  (Virtual Console in Design, the widget selected), the venue page, Power
  distribution, Edit focus calls, Connections on the universe. Some checks
  offer a one-click fix beside it where it is unambiguous and undoable
  (*Make Block cue*, *Point it at the list*, *Collect for Transfer*,
  *Rename to 03-01.03-…*), always one undo step.
- **Accept…** asks for a note (required, one line is fine), shows who
  (§4.3), and stores it. **Accept all like this** for a selection of the
  same check. **Turn this check off for this show…** with a note.
  **Reopen** on an accepted finding.
- **Why?** opens a paragraph per check: what it means on stage, how to fix
  it — the same text the help page lists (`docs/features/show-review.md`,
  F1).
- **Re-check** runs everything; the panel otherwise keeps its list current
  as edits come in (§5.5).

### 4.2 When it runs

- **On demand**: the panel, ⇧⌘R, the chip.
- **Before a hand-off** — each asks only when something is open, and never
  blocks except as said:
  - **File ▸ Collect for Transfer** runs the FILE, FIX-01/03 and VEN-09
    checks first and lists what would not travel.
  - **Switching to Operate** (the first time per session, like *GO in
    Design offers Operate*): *2 blockers open — Review / Go to Operate
    anyway*. Never in Operate itself; a running show is never interrupted.
  - **Saving** never asks (saves must stay instant and silent); the chip
    updates.
  - **Course hand-in** (new: **File ▸ Hand In…** for course shows, or the
    course's instructions): runs Review; the hand-in copy carries the
    findings and acceptances, and the course can require *no open
    blockers or warnings* — accepted is fine, that is the student's
    reasoning, which is the point.
- **Live** (Settings ▸ Show Review ▸ *Check as I work*, off by default):
  the cheap checks re-run on the parts that changed, debounced 1.5 s after
  the last edit, and the chip follows. Moderate checks run on Re-check and
  before hand-offs only, unless live review is set to *everything*.
- **On open**: no. A show opens quietly; the chip shows the result saved
  with the show (counts only) until a run.

### 4.3 Acceptance: stored with the show

In the show XML, an optional engine element, written only when it has
something in it (like `OutputRules` and `FocusCalls`); an older build skips
it (`Doc::loadXML()` warns and skips an unknown engine tag):

```xml
<ShowReview PlaysOn="pi5" LastRun="2026-11-02T14:02:11" Open="2,5,14">
  <Accepted Check="MOV-01" Target="list:3/cue:118/fx:7"
            Hash="b1946ac9" By="Branson Matheson" When="2026-10-25T21:14:00">
    Live drift with the soloist, on purpose.</Accepted>
  <Accepted Check="PWR-05" Target="list:3/cue:140"
            Hash="7d5e0a11" By="Ava R." When="2026-11-01T16:40:00">
    Strobe warning posted in the lobby and the program.</Accepted>
  <Off Check="MOV-04" By="Branson Matheson" When="2026-10-25T21:20:00">
    Thrust stage; the house zone isn't drawn yet.</Off>
  <Setting Check="CUE-01" Name="maxCues" Value="20"/>
</ShowReview>
```

- **Check**: the catalog ID, never reused.
- **Target**: the finding's subject by **stable ids**, never by names or
  positions: `Fixture::id()`, `FixtureGroup::id()`, `Function::id()`,
  `QLCPalette::id()`, the Show Tree's production, list, cue and layer ids
  (`ShowTreeModel`, one saved counter `NextId` — kept across edits and
  saves), `VCWidget::id()` (saved; a duplicate found on load is silently
  given a new id, so that load should log the remap and move the
  acceptance with it), venue
  by name (venues are keyed by name in `VenueCorrections`; a rename moves
  its acceptances with it), focus calls by a new id (§3.14). A finding about
  two things (a collision) keys both, sorted.
- **Hash**: a short hash of exactly what the check read to decide — for
  MOV-01 the two cues' resolved Position and Dimmer for that fixture and the
  cue timing; for PAT-01 the two fixtures' universe, address and footprint;
  for PWR-05 the strobe value and the definition's capability. Each check
  defines its own `fingerprint()` (§5.1). Not a hash of the whole cue: a
  change elsewhere in the cue doesn't reopen the decision; a change to what
  the decision was about does.
- **When the hash differs**, the finding is open again, shown as *Changed
  since accepted* with the old note and who wrote it, and **Accept again**
  keeps the note for editing. When the target is gone, the acceptance is
  dropped at the next save (and said in the panel once).
- **By**: the name in Settings ▸ General ▸ *Your name* (new; the course
  already asks students for one), else the computer's user's full name.
- Acceptances are part of the show's undo history (an Accept is one step)
  and travel with every copy of the show — Save As, Collect for Transfer,
  the hand-in.
- **Settings** per check that the show can tune (CUE-01's *N*, VEN-07's
  days, FX-05's limit) go in the same element.

---

## 5. Architecture

### 5.1 A check registry in the engine

```
engine/src/showreview/
  reviewcheck.h        // class ReviewCheck (abstract)
  reviewfinding.h      // struct ReviewFinding
  reviewcontext.h/.cpp // what checks share: one resolve per list, indexes
  showreview.h/.cpp    // ShowReview: registry, run, acceptance, XML
  checks/mov*.cpp, pal*.cpp, cue*.cpp, … // one small class per check
```

```cpp
struct ReviewFinding
{
    QString check;            // "MOV-01"
    Severity severity;        // Blocker, Warning, Note
    QString message;          // plain crew language, translated
    QStringList targets;      // stable keys: "fx:7", "list:3/cue:118"
    QByteArray fingerprint;   // what the check read (hashed for storage)
    ReviewJump jump;          // where Fix goes (a kind + ids; the UI maps it)
};

class ReviewCheck
{
public:
    virtual QString id() const = 0;            // "MOV-01"
    virtual QString title() const = 0;         // "Moves in light"
    virtual Area area() const = 0;
    virtual Cost cost() const = 0;             // Cheap, Moderate
    virtual Inputs inputs() const = 0;         // what it reads (§5.5)
    virtual QList<ReviewFinding> run(const ReviewContext &ctx) const = 0;
};
```

- **Pure reads.** A check never changes the Doc. It reads through a
  `ReviewContext` that builds shared things once per run: each list's
  `ShowTreeModel::resolve()` and `stageOf()`, fixture → groups, function →
  users (who plays what), the VC index (§5.3), the venue's clashes. Many
  checks share one resolve.
- **Reuse, don't re-derive.** Where a check exists (§2) the review check is
  a thin adapter over it: `LookStackCheck::check()`, `VenueRig::clashes()`,
  `FixtureDefCheck::check()`, `FixtureDefUpdates`, the readiness model's
  design items, `NetworkReport`, `PowerDistribution`. Where the existing code
  lives in the UI (the readiness list in `ShowStatus`, MIB rules in
  `MarkPlanner`, the naming pattern in `FunctionManager`), the pure part
  moves to the engine first, and both callers use it — a refactor with no
  behaviour change, covered by the existing tests.
- **ShowReview** owns the registry (static registration, one line per
  check), runs a selection, applies acceptances (match by check + targets,
  compare fingerprint), and loads/saves `<ShowReview>`. It lives on the
  `ProgrammerController` side of the house (memory: programmer-controller)
  — `Doc` gets a thin forwarder for the XML, nothing more.
- **Translation**: messages are `tr()` strings with arguments, so a
  finding's text is the student's language, and the CLI's too.

### 5.2 UI on top

- `ui/src/showreviewpanel.{h,cpp}`: the dock/window, model, filters,
  Accept dialog. `ReviewJump` → the existing jump-to code (Show in Show
  Tree, Fixture Properties, the Patch dialog, VC selection, Connections).
- The footer chip joins `ShowStatus` as one more entry, with its own
  colour rules, hideable in Settings ▸ Appearance with the others.
- Fix-it actions are commands the UI already has (make Block, point a
  widget at a list, rename, Collect for Transfer), invoked as one undo
  step.

### 5.3 The Virtual Console without a UI

VC widgets live in `ui/src/virtualconsole/`. The engine can't see them, but
the VC checks need only a few facts per widget: id, type, caption, page and
frame, function id(s), key sequences, input sources, a Cue Runner's
follow, solo-frame membership, geometry. So:

- An engine-side **`VcIndex`** (plain structs) that both sides fill: the
  UI from the live `VirtualConsole` (a small exporter walking widgets), the
  CLI by reading the `<VirtualConsole>` element of the file with a
  stream reader that keeps only those facts and skips everything else.
- The two fillers are tested against each other on the test shows: the
  index from the live VC and from the saved file must be equal.

### 5.4 The CLI

```
qlcconsole --review show.qlcc [--format text|json] [--min warning]
           [--plays-on pi5] [--check MOV-01,PAT-*] [--include-accepted]
```

- Runs **before** the `QApplication` is made (as `-style` is handled in
  `main/main.cpp` today), on a `QCoreApplication`: load the definition
  caches, scripts and the Doc with `loadIO = false` (no plugins, nothing
  sent, no devices opened), load the venue sidecar read-only, run, print,
  exit. Never writes the show — not even the acceptances' cleanup.
- **Text** output for people: grouped like the panel, one line a finding,
  `ID  severity  message  [target]`. **JSON** for CI and the course's
  tooling: the findings, the accepted ones with notes when asked, counts,
  version.
- **Exit code**: 0 nothing open at or above `--min`; 1 open warnings; 2
  open blockers; 3 the show couldn't be read. A course folder:
  `for f in handins/*.qlcc; do qlcconsole --review "$f"; done`.
- The repo's own `test-workspaces/` and the course shows
  (`docs/course/shows/`) are reviewed in the gate with a checked-in
  expected result, so a change that makes a course show fail its review is
  caught (§6).

### 5.5 Performance and incremental re-check

- **Budget.** A full run on the largest test show (the stress-test rig,
  70–80 universes, about 15k channels; and DanceHysteria-0.6's cue lists)
  under **0.5 s** on the dev Mac and **3 s** on a Pi 5; cheap checks alone
  under **50 ms** on the dev Mac, so live review is unnoticeable. Each check
  is timed; the panel shows the slowest in a tooltip; a check over 200 ms is
  a bug.
- **Expensive parts are shared and cached**: one resolve per list
  (already pure, and `rebakeChain()` says which cues an edit can change);
  sampled beam paths for MOV-04/05/07 cached per (fixture, from, to)
  position pair; `BeamOcclusion::Cache` exists already.
- **Incremental.** Each check declares its `Inputs` (Patch, Fixtures,
  Groups, Palettes, Functions, ShowTree, VC, Plot, Venue, Power, Files,
  Settings). Doc and controller signals the app already has
  (`fixtureChanged`, `functionChanged`, the Show Tree's change signals, the
  venue's, the VC's) mark inputs dirty; after the debounce only checks with
  a dirty input run, and for Show Tree checks only the lists touched
  (finer: only the cues in the edit's `rebakeChain()`). Findings from
  untouched checks are kept.
- **Thread.** Runs on the GUI thread in slices (a check at a time, yielding
  to the event loop), not a worker: the Doc is not safe to read from
  another thread while the user edits, and a snapshot would cost more than
  the run. Never during Operate's GO handling; live review pauses in
  Operate.

---

## 6. Testing plan

- **One trigger show per check.** `engine/test/showreview/shows/` holds a
  small `.qxw` per check (or per group, with one cue per case) that
  triggers it, plus the near-miss that must *not* trigger it (a move in the
  dark beside the move in light; two hits in different solo frames beside
  two in the same scope). Built by code in the test where that's shorter
  than a file. Every check ID in the registry must have a test that makes it
  fire and one that keeps it quiet — a registry test fails if a check has
  neither.
- **`ShowReview_Test`** (engine, headless): per check, the finding's ID,
  severity, target keys and message text; the jump kind.
- **Acceptance round trips**: accept → save → load → the finding is
  accepted (quiet); change what it read → it reopens as *Changed since
  accepted* with the note; change something it didn't read (another
  layer of the cue) → still accepted; delete the target → the acceptance
  goes at the next save; an older build's load (the element skipped) and
  save drop nothing else (`ShowFiles_Test` style); a newer build's unknown
  attributes in `<ShowReview>` survive a save.
- **Fingerprint stability**: the same show loaded twice, and saved and
  reloaded, gives identical fingerprints (no pointer order, no hash-map
  order).
- **VcIndex parity**: index from the live VC (offscreen, `QT_QPA_PLATFORM=
  offscreen`, per memory) equals the index read from the file, on every
  test workspace.
- **CLI**: `qlcconsole --review` on the trigger shows — exit codes, text and
  JSON output (golden files), no write to the show (mtime and bytes
  unchanged), no plugin loaded.
- **Course and sample shows** in the gate: `docs/course/shows/*` and
  `test-workspaces/*` reviewed with checked-in expected findings (most
  should be clean or accepted, with notes saying why), so a regression in a
  check or a show shows up as a diff.
- **Performance**: a timing test on the stress-test show asserting the
  budget in optimised builds (serial list, as it measures wall time —
  `platforms/linux/unittest-serial.txt`).
- **UI** (`ui/test/showreview`, offscreen): the panel lists, filters,
  Fix jumps to the right place for each jump kind, Accept stores the note
  and is one undo step, the chip counts.

---

## 7. Phasing

Effort is rough, in focused sessions (an evening of agent work with
Branson's review), after the freeze lifts.

**v1 — the frame and the checks with the data in hand (≈ 6–8 sessions).**

- Engine: registry, context, findings, acceptance with fingerprints,
  `<ShowReview>` XML; the CLI (`--review`, text and JSON); VcIndex from
  both sides. (2 sessions)
- UI: the panel, chip, Fix jumps, Accept, Re-check; Review before Operate
  and Collect for Transfer. (1–2 sessions)
- The v1 checks (2–3 sessions), 52 of them:

| ID | One line |
|---|---|
| MOV-01 | A head moves while lit between cues |
| MOV-02 | Move in black can't hide a move (off, or the dark gap too short) |
| MOV-08 | Movers lit in the first cue with nothing presetting their position |
| PAL-01 | Two palettes or layers set the same attribute on a fixture; order decides |
| PAL-02 | LookStackCheck's covered looks and unmatched effects |
| PAL-03 | A palette type its fixtures can't play |
| PAL-05 | A palette or look pointing at a deleted fixture, group or target |
| CUE-01 | A value tracks across a song or past N cues |
| CUE-02 | The top of a song tracks in from the song before |
| CUE-03 | A cue that changes nothing |
| CUE-04 | An empty cue |
| CUE-05 | Auto-follow with no time, or a loop with no hold |
| CUE-06 | Duplicate cue numbers (and gaps, as notes) |
| CUE-07 | Skipped cues, worst when base, included or jumped to |
| CUE-08 | Releases with no base cue, or a bad base cue |
| CUE-11 | Include loops; includes and reusables pointing at nothing |
| CUE-12 | Originals still played or edited; generated functions edited outside the tree |
| CUE-13 | A cue list nothing can play |
| VC-01 | Hits in the same scope fighting over the same lights |
| VC-03 | Hits no button, key or input fires |
| VC-04 | Buttons and widgets with no function, or a deleted one |
| VC-05 | The same key on two widgets, or on an app shortcut |
| VC-06 | The same MIDI/input on two widgets |
| VC-08 | A Cue Runner following no list |
| PAT-01 | Overlapping addresses |
| PAT-02 | A footprint past 512 |
| PAT-03 | Unpatched fixtures that cues use |
| PAT-04 | Fixtures on not-ours channels |
| PAT-05 | Venue clashes, for every venue (overlap, not ours, slot) |
| PAT-06 | A universe with fixtures and no output |
| FIX-01 | Missing definitions |
| FIX-02 | Definition problems (FixtureDefCheck) |
| FIX-05 | Fixtures not on the plot (warning when focus, aim or power needs it) |
| FIX-08 | Dead-marked lights still used |
| VEN-01 | Focus calls with no target or no lights |
| VEN-02 | Unaimable lights in focus calls |
| VEN-05 | Swapped fixtures needing focus |
| VEN-09 | Venue sidecar missing |
| VEN-10 | Practice venue left applied |
| FX-01 | Effects too heavy for the target machine (with the Pi 5 cost table only) |
| FX-02 | RGB matrices on lights with no colour mixing |
| FX-03 | Effects of zero size or speed, or on a paused speed master |
| FX-07 | Scripts that won't load |
| PWR-01 | A circuit over its breaker in a cue or hit |
| PWR-03 | Lights on no circuit, once power is set up |
| PWR-05 | Flashing over 3 Hz |
| PWR-06 | Stage lights that ignore Blackout or the Grand Master |
| FILE-01 | Media outside the show folder or absolute |
| FILE-02 | Missing media |
| FILE-03 | Definitions and scripts the show doesn't carry |
| NAME-01 | Names off the folder's SS-PP.II pattern |
| NAME-02 | Default and "Copy of" names |
| FTR | Every design-time footer item, under its catalog ID (§2) |

**v2 — geometry and the rest of the cheap ones (≈ 4–5 sessions).**
MOV-03/04/05/06/09/10/11, the house zone (derived, then drawable);
VEN-03/04/06/07/08/12/13; PAL-04/06/08/09/10; CUE-09/10/14/15/16;
VC-02/07/09; PAT-07/08/09/10; FIX-03/04/06/07/09/10/11/12; PWR-04/07;
FILE-04; SC-01/02/03; live review; Hand In for the course; course lesson
text and a lesson exercise ("review your show and hand in with no open
warnings").

**v3 — the long tail (≈ 3 sessions).** MOV-07/12, PAL-07, VC-10/11/12,
PAT-11, FIX-13/14, VEN-11, FX-04/05/06/08/09 (with cost tables for every
target), PWR-02/08, FILE-05/06, NAME-03/04/05/06; one-click fixes for the
common ones; the review in CI on every test workspace and course show.

Per group, roughly: MOV 2.5 sessions (the geometry is most of it), CUE 1.5,
VC 1.5 (half of it the VcIndex), PAL 1, PAT 0.5, FIX 0.5, VEN 1, FX 1.5
(the cost tables and measure mode), PWR 1, FILE 0.5, NAME 0.5, SC 0.5;
framework, CLI and panel 3–4.

---

## 8. Open questions

1. **Song boundaries** (CUE-01/02): heuristics (whole-number changes, Block
   cues, the `SS` of `SS-PP.II` names), or a *Song starts here* flag on a
   cue? The flag is one bit in the file and makes the check exact.
2. **The house zone** (MOV-04/05): derived from the deck front only, or a
   drawable Audience area on the Stage layer (a thrust or in-the-round
   stage needs the drawable one)?
3. **Who** on an acceptance: a *Your name* setting on this computer, or
   the course's student name, or both?
4. **Blockers and Operate**: ask once per session (proposed), or never ask
   and leave it to the chip?
5. **Hand In**: a File menu entry for course shows, or only the CLI for
   teachers?
6. **Plays on** (FX-01): per show (proposed: the show knows where it
   runs), or per computer?
7. **Photosensitivity** (PWR-05): 3 Hz over "a large part of the stage" —
   how large? Proposed: any group of 4+ lights, or any light whose beam
   covers a quarter of the deck on the plot.
