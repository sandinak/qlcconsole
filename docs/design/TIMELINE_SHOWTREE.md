# Cue lists on the timeline, the timeline on timecode

Status: proposal, 2026-11-04. Read against `origin/main` 90c9ef05d.
Answers Branson's ask: where a Show Tree cue list meets the Shows timeline,
working both ways; whether placing a list updates its cue times; anchors on
the timeline; and how the timeline links to SMPTE.

Short answer: there is no merge point today. Both sides read the same
incoming timecode, but separately. The design below makes the **cue times
the one truth**, and makes a cue list on a timeline track a **view and
handle on those times**, not a second copy of them. The timeline becomes
the time view the Show Tree doesn't have. Its own clock can also drive the
cues when no outside timecode is running.


**How it plays (agreed with Branson, 2026-11-04).** The timeline is in effect
GO on a clock, but it works by position, not by counting down to the next cue.
The playhead tells the Show Tree what time it is, and the Show Tree puts in the
cue that should be live at that time, exactly as a GO would:
- **Play from the top:** GO at each cue's time.
- **Jump or scrub:** the cue that should be live goes straight in, tracked as a
  GO from that point would. The cues in between are not replayed.
- **Land mid-fade:** the cue starts part-way through its fade.
- **Pause:** the look holds.
- **Stop:** the list is released.

This is the same rule the Show Tree already uses for incoming timecode; the
timeline is one more clock. The clip has no playback engine of its own.

---

## 1. What exists today

### 1.1 Timecode in (chase only)

| Piece | Where | Since |
|---|---|---|
| One clock for the app: `TimecodeSource` (position ms, fps, running, health; a 400 ms free-run cap, a 1 s "holding" chip) | `engine/src/timecodesource.h:27-45`, `Doc::timecodeSource()` `engine/src/doc.cpp:2529` | 2026-07-15 ff7b2ea83 |
| MTC in (quarter frames + full frame) | `plugins/midi/src/common/midimtcdecoder.*` | 2026-07-15 9458c8058 |
| Art-Net TimeCode in (24/25/29.97df/30) | `plugins/artnet/src/artnetcontroller.cpp:678` | 0.3.1, 95f0e3eeb (Q180) |
| LTC in (first channel of the audio input; forward only) | `engine/src/ltcreceiver.*`, pseudo-universe `TimecodeSource::LtcUniverse` (`timecodesource.h:75`) | 2026-10-31 cb55360fd |
| Per-production source (P5) | `TimecodeSource::setProductionUniverse` | P5 |

**Nothing generates timecode.** No MTC out, no LTC out, no Art-Net TC out
(grep of `engine/`, `ui/`, `plugins/`: only receive paths).

### 1.2 The Shows timeline (stock Show/Track/ShowFunction, forked)

- A `Show` follows timecode when `Show::timecodeFollow()` is on
  (`engine/src/show.h:277-301`). The `ShowRunner` clock then takes
  `setExternalTime()` instead of the tick (`engine/src/showrunner.h:68-80`),
  holds on a dropout and freezes running children's fades.
- **Timeline 0 = timecode offset**: `Show::timecodeOffset()` (ms). The
  mapping is done in the UI, `ShowManager::slotTimecodePosition`
  (`ui/src/showmanager/showmanager.cpp:2276`), which also **auto-snaps the
  offset to the hour** the first time timecode ≥ 01:00:00:00 arrives with no
  offset set (`:2284-2289`). The offset is also set by "Timeline 0 =
  current timecode", the TC 0… menu and the calibration dialog
  (`ui/src/timecodecalibrationdialog.cpp:85`).
- **Section markers**: `ShowMarker {start, end, label, colour, cueListId}`
  (`show.h:35-47`, `:214-235`), since 278f9b937 (2026-07-19). A marker can
  link a **manual** chaser for an off-clock section: when timecode stops
  inside that marker, the linked chaser is started and armed for GO
  (`slotTimecodeRunningChanged`, `showmanager.cpp:2315`; picker at
  `:2165`). The footer prints the section and next GO (`ui/src/app.cpp:8775`).
  This is the only timeline ↔ cue-list link that exists, and it points the
  other way (timeline hands off to a plain chaser by GO).
- **Waveform + marker snap** (tlw, 2026-11-01): the audio clip's waveform in
  the markers lane, snap to a marker edge. TODO.md "Timeline waveform +
  markers" records *decided (can change): the Show Tree's cue list has no
  linked audio track and no time view.*
- **Chaser lockstep**: a chaser block on a track is show-clocked; its step
  is chosen from the show position by walking step durations
  (`showrunner.cpp:807-840`, `Chaser::stepIndexAtLocalMs`,
  `engine/src/chaser.cpp:229`). A step with an infinite hold (a GO cue)
  counts as **3 s** (`SHOW_NOMINAL_CUE_MS`, `showrunner.cpp:297`). This path
  goes straight to `ChaserSetStepIndex`, not through `ShowControl::run()`.

### 1.3 "Collections into the timeline"

aaff1fab3, 2026-07-15, *Collections as first-class Show timeline items*:
UI only (the engine was already function-generic). `CollectionItem`
(`ui/src/showmanager/collectionitem.cpp`) is a plain block; a collection of
scenes has no length, so it gets **10 s** (`COLLECTION_DEFAULT_DURATION`,
`:33`). A Show Tree cue bakes to a Collection, but those are **hidden**
(`coll->setVisible(false)`, `engine/src/showtreecontroller.cpp:4994`), so
a tree cue can't be picked onto a track; only hand-made collections can.

### 1.4 Show Tree cue times (0.4)

- **Cues at a timecode** (9d6a2d91b, 2026-10-31): `Cue::timecode` (ms of the
  *incoming* timecode, absolute, e.g. 01:02:03:10) and
  `CueList::followTimecode` (`engine/src/showtreemodel.h:349-354`,
  `:413-416`). `TimecodeCues` (`engine/src/timecodecues.h`) fires each cue
  through the **One GO path** (`ShowControl::run`, `RouteTimecode`).
  Rules (decided, can change): a jump forward fires only the latest cue
  passed; a jump back re-arms (< 250 ms is jitter); a dropout holds; joining
  part-way fires only a cue exactly there; untimed cues are GO-only; GO by
  hand between timed cues changes nothing.
- **Timecode anchors** (Q198 1–4, 620f53dfb, merged 5d5274dcc 2026-11-02):
  a song section has a `start` (`SectionHead::start`, `showtreemodel.h:205`);
  cues can count from it (`tcFromSection` + `tcOffset`) or from an **anchor
  cue** (`tcAnchor`); `Cue::timecode` stays the *resolved* time so the
  runtime and older builds are unchanged. Operations
  (`engine/src/showtreecontroller.h:557-594`, `showtreetimecode.cpp`):
  `setSectionStart` (+ *later sections follow*), `setSectionStarts`,
  `setCueTimecodeAnchor`, `convertToSectionTimes`, `rippleTimecode`,
  `stretchSection`, `cueTimecodeBase`; Logic marker import from an exported
  MIDI file (`MarkerPlan`, matched by name with `matchKey`, previewed, one
  undo step); `SectionLearn` (tap at each section start). The header
  already says *"For views (the timeline): timecodeAnchors() and
  Section::start, re-read on timecodeAnchorsChanged()"* — nothing in
  `ui/src/showmanager/` reads them yet.
- **Later (Q198 5)**: timing maps per session version (festival cut vs
  full), switchable like venues. **Open**: a time per frame rate (times are
  ms; a cue stamped at 25 fps reads the same ms at 30).

### 1.5 Can a cue list go on a track today? Yes, by accident

A list bakes to a **visible** Chaser under `Productions/<production>`
(`showtreecontroller.cpp:4876-4884`), so Add to track can pick it, and W5's
import-in-place re-points timelines that played the old chaser at the list's
chaser (`engine/src/showtreebridge.cpp:246-250`). On the track it is a
`SequenceItem`, and then:

1. It plays by **step lengths, not cue times**: every GO cue is 3 s
   (lockstep, above). `Cue::timecode` and section starts are ignored.
2. Its step edits (drag a boundary, scale, delete cues, stretch,
   `ui/src/showmanager/sequenceitem.cpp:296`, `:493`, `:907`, `:955`) write
   the generated chaser directly. There is **no `isGenerated()` guard**
   there (only the side editor is locked, `showmanager.cpp:807`), and the
   next bake rewrites the steps (`showtreecontroller.cpp:5096-5108`). Edits
   are lost; a delete changes what plays until then.
3. If the list also follows timecode, `TimecodeCues` GOes the chaser by
   number while the ShowRunner lockstep sets its step from the block: **two
   owners of one chaser**.

So the honest state: the timeline can hold the list's chaser, but nothing
links the timeline to cue times, and the accidental route is unsafe.

### 1.6 Who plays, who owns output

- **Show Tree**: GO / Run tab / pads / OSC / MSC / timecode all go through
  `ShowControl::run()` (`engine/src/showcontrol.h:90-103`), which starts or
  steps the list's baked chaser. Auto-follow and waits are baked into step
  holds (`showtreecontroller.cpp:5045-5056`).
- **Shows timeline**: `ShowRunner` starts each item's function at its time,
  on the show clock (tick, or external timecode).
- Both can run at once. Nothing arbitrates between them: their functions
  meet in the universes like any two functions (HTP intensity, LTP the
  rest). The only takeover is Operate's VC suspend of the timeline
  (`ShowRunner::setSuspended`). Both follow the **same** `TimecodeSource`;
  the timeline maps it through its offset, the tree uses it as is.

### 1.7 Is there a linkable way from timeline to SMPTE?

**Yes, one way: chase.** A Show set to follow timecode locks its playhead to
incoming MTC, Art-Net TimeCode or LTC, with *Timeline 0 = HH:MM:SS:FF* as
the offset (auto-snapped to the hour). There is **no way to send** timecode
from the timeline, and **no link between a timeline position and a cue's
time**: the tree stores absolute timecode, the timeline stores
offset-relative ms, and no code converts between them.

Stale doc to fix with this work: `docs/features/shows.md:85-86` still says
section anchors and marker import are "a separate piece of work" (built in
Q198, in the Show Tree).

---

## 2. Design

### 2.1 The model, in one picture

```
 Show Tree (truth)                      Shows timeline (view + clock)
 ─────────────────                      ─────────────────────────────
 Cue list "Act 1"  followTimecode        Timeline 0 = 01:00:00:00  (Show offset)
  Section Verse 1   start 01:00:12:00 ◄──┐ markers  |Verse 1   |Chorus 1     |
   cue 1.0.1  +0.0 s                     │ audio    ▁▃▅▇▅▃▁▂▃▅▇▇▅▃▂▁▃▅▇▅▃▁▂▃▅
   cue 1.0.2  +4.0 s   anchor ◆          │ Act 1   [▌1.0.1  ▌1.0.2◆ ▌1.0.3 ▌2.0.1 …]
   cue 1.0.3  +2.5 s from ◆              │          ^ clip: a cue-list item
  Section Chorus 1  start 01:00:41:10 ◄──┘          ticks = Cue::timecode − offset
   …                                                  bands = section starts

 drag a tick       → setCueTimecode()        (tree undo step)
 drag a band edge  → setSectionStart()       (⇧: later sections follow)
 drag the clip     → shift every start       (one tree undo step)
 timecode in       → TimecodeCues → ShowControl::run(RouteTimecode)
 timeline playing  → same, its clock as the source (no outside timecode)
```

Rule of thumb: **a cue's time lives in the Show Tree; the timeline only
draws it and edits it through the tree's own operations.** Timeline time =
cue time − the Show's offset. Nothing about a cue's time is stored in the
Show.

### 2.2 Cue list → timeline (the cue-list clip)

**What it is.** A `ShowFunction` on an ordinary track pointing at the list's
chaser, marked `CueTimes="1"` (an older build ignores it and sees the chaser
block it sees today). No new model, no new tab, no new track kind.

**How it's drawn.** From `timecodeAnchors(listId)` and the cues' resolved
times: a band per section (from its start to the next), a tick per timed cue
(number + name on hover), a diamond on anchor cues, a dashed tick for a cue
that counts from a section or anchor. Untimed cues are not on the clip; a
badge says "3 GO-only cues". The clip spans its first time to its last time
plus the last cue's fade (or a section end, if the last section has a
marker end). Skipped cues are drawn greyed. Sub-cues and hits are not drawn.

**What it plays.** Nothing by itself. The ShowRunner does **not** start the
chaser for a `CueTimes` clip and the lockstep never touches it. The list
fires through `TimecodeCues` and the One GO path, exactly as without a
timeline. One playback path, one set of rules (jump, re-arm, dropout,
jitter), one owner of the chaser. (§2.4 adds the timeline's clock as a
source for when there is no outside timecode.)

**Dropping a list on a track.**
- *Its cues have times*: the clip lands where the times say. The drop point
  is ignored, and the status line says so ("Act 1 sits at its cue times,
  01:00:12:00"). Moving it is a separate, deliberate drag (below).
- *Its cues have no times*: the clip lands at the drop point with every cue
  GO-only, and the list is set to follow timecode. The clip's context menu
  offers **Space cues over the clip** (each gets a time, one undo step) and
  **Record GOs** (play the timeline and GO; each GO stamps that cue's time,
  the per-cue Learn run as a pass). Section starts can then come from
  markers (§2.3).

**Editing on the clip** (Design only; Operate draws it read-only).

| Gesture | Tree operation | Undo |
|---|---|---|
| Drag a cue tick | `setCueTimecode` (kept as an offset when its section has a start) | one tree step |
| Drag a section band's left edge | `setSectionStart`; ⇧ = *later sections follow* | one tree step |
| Drag the clip body | every section start and absolute cue time by Δ (`rippleTimecode` at the first cue) | one tree step |
| Drag the clip's right edge | refused, with a reason. Stretch is per section (below) | — |
| Context ▸ Stretch section… | `stretchSection` | one tree step |
| Context ▸ Insert / remove time here | `rippleTimecode` | one tree step |
| Context ▸ Anchor / Un-anchor | `setCueTimecodeAnchor` | one tree step |
| Context ▸ Open in Show Tree | selects the cue there | — |
| Delete the clip | removes the clip only; cue times stay | timeline snapshot |

Snap (⌘G) works as for any item: grid, then marker edges, plus the audio
waveform's peaks if that's built later.

**Conflict rules.**
1. One clip per list per Show. A second drop of the same list is refused
   ("Act 1 is already on track 3 at 00:12").
2. A list may sit in several Shows, but its times are its own: moving it in
   one Show moves it in all. Different positions per Show (a festival cut)
   are Q198 5's timing maps, not this.
3. A clip can't overlap another item on its track (today's rule), and it
   grows as its cues move, so a move that would collide is refused with the
   item it hits named.
4. Locked clips (`ShowFunction::isLocked`) and lists locked elsewhere refuse
   edits, as today.
5. The old chaser block (a cue-list chaser **without** `CueTimes`) keeps
   playing by steps, but `SequenceItem` refuses step edits on a generated
   chaser with the tree's locked banner (P0, a bug fix in its own right).
   Converting it to a cue-list clip is one context-menu item, and the
   default for a list that follows timecode (board question).

### 2.3 Timeline → cues (markers as section anchors)

The timeline already has markers and the tree already has section starts
and name matching (`MarkerPlan::matchKey`). Join them, don't copy them:

- **Bind a marker to a section.** A marker named like a section of a list
  that is clipped in this Show is drawn as that section's start, **read
  from the tree**. Dragging it calls `setSectionStart` (⇧ = later follow).
  The Show keeps only the marker's label and colour. Its start is the
  section's; an older build reads the last start saved. A bound marker
  shows a link glyph. Unbinding leaves a plain marker where it was.
- **Sections from markers** (clip context menu): run `MarkerPlan` with the
  Show's markers as input instead of a MIDI file (offset = the Show's
  timecode offset). Same preview ("Chorus 2 moved +6.0 s, 14 cues follow"),
  one undo step. A DAW's markers can already reach the timeline by hand or
  by the MIDI import, so this closes the loop with no new importer.
- **Markers from sections**: the reverse, for a list built in the tree
  first.
- **Anchors** on the timeline are the tree's anchor cues (diamonds on the
  clip). The timeline adds no anchor type of its own.
- **Ripple from the timeline**: Insert / remove time at the playhead across
  the clip calls `rippleTimecode`. Plain timeline items after it don't move.
  Rippling items is the timeline's own job and not part of this.
- **Learn from a live run**: Learn section starts and per-cue Learn work
  against whichever clock is running: outside timecode, or the timeline's
  own (§2.4). So a designer with only the song file on the timeline can
  learn starts with no DAW running.

The existing **"Link cue list…"** on a marker (a manual chaser armed when
timecode stops) stays and is renamed into one entry, **Cue list here…**,
that offers *bind to a section of a clipped list* or *arm a list for GO
when the clock stops*. The menu count stays flat.

### 2.4 SMPTE / MTC / LTC

**Chase (have).** MTC, Art-Net TC, LTC into one `TimecodeSource`, per
production if wanted. Keep it.

**Mapping.** One number per Show: **Timeline 0 = HH:MM:SS:FF**, i.e. the
existing `timecodeOffset`, shown as a timecode field in the Show's
settings, not only the TC 0… menu. A cue's place on the timeline = its time
− the offset. A cue before 0 is drawn clipped at 0 with a warning. Changes:
- Move the offset mapping out of `ShowManager` into the engine (`Show`), so
  it works and is tested with no Shows tab open (today a VC Show Control
  widget relies on the tab being alive).
- The hour auto-snap stays, but it says so once ("Timeline 0 set to
  01:00:00:00 from the incoming timecode") and is one undo step. Today it
  writes the offset silently.
- Ruler: a **timecode** mode (HH:MM:SS:FF at the source's fps, else the
  Show's display fps) beside time and BPM. Times stay ms. Display fps is a
  Show setting; a per-rate stored time stays the open TODO item.

**The timeline's own clock as a source.** When a Show plays and is **not**
following outside timecode, its position + offset is fed to `TimecodeCues`
as a fourth source (a pseudo-universe like `LtcUniverse`, "Shows timeline").
Only lists clipped in *that* Show listen to it. Then a timeline with the
song's audio clip runs the cue lists on the clip with no DAW, through the
same GO path. If outside timecode is running, it wins and the timeline
clock is not fed (board question). It never feeds the Show itself, so no
loop.

**Dropouts and jumps.** Unchanged and shared: the ShowRunner's 400 ms
free-run then hold, and `TimecodeCues`' rules (forward jump fires the
latest cue, back re-arms, 250 ms jitter, dropout holds, part-way join fires
nothing past). The timeline scrubbing while stopped does not fire cues.
Scrubbing while playing is a jump.

**Generate (not now).** MTC out from the timeline's clock is a small MIDI
plugin addition (quarter frames on a chosen port), but it is a protocol to
keep working on hardware (Maint H) and no current show needs the light desk
to be the master. **LATER**, built only when a show needs it. **LTC out** is
audio generation: **NO** (STRATEGY's no audio engine). Art-Net TC out:
LATER, with MTC out if ever.

**Out of scope** (STRATEGY NO list or not this): audio playback beyond the
stock Audio function already on the timeline; Ableton Link / beat clock;
OSCQuery; chasing reverse LTC (open TODO, separate).

### 2.5 One source of truth

**Recommendation: the Show Tree's cue times (section starts + offsets +
anchors, resolved into `Cue::timecode`) are authoritative. The timeline is
a view, an editor and, when it plays alone, a clock.**

Why:
- The runtime already fires from them through the One GO path. Making the
  timeline authoritative would add a second playback path, which a
  guardrail rules out without an explicit yes.
- A list follows timecode with **no Show at all** (DAW-driven shows). The
  times must live with the list.
- Every Q198 tool (ripple, stretch, Learn, marker import, Section Times…)
  and the cue sheet, Run tab, calling script and older builds
  (`Timecode=`) read the tree.
- The Show's markers and positions are per Show. A list can be in several
  Shows, so they can't hold one list's times without a merge rule.

**Keeping in sync.** The clip holds no times, so there is nothing to sync:
it re-reads on `timecodeAnchorsChanged` / the tree's change signal, and
on the Show's offset changing. Bound markers likewise hold no start.

**Undo.** Edits made on a clip are tree steps (the shared tree+palette
history, `undoShared`). The Shows tab's ⌘Z takes back whichever was last,
the timeline snapshot or a tree step made from the timeline, compared by
stamp. This is how `undoShared` already arbitrates tree vs palette steps.
Placing or deleting the clip is a timeline snapshot. Converting a chaser
block to a clip is one step of each, grouped (`groupStepsSince`).

---

## 3. Phasing, ROI, effort, risk, tests

ROI = Value × Fit ÷ Effort (days) × risk (L 1.0, M 0.8, H 0.6).

| Phase | Item | Value | Fit | Reuses | Effort | Risk | Maint. | ROI | Call |
|---|---|---|---|---|---|---|---|---|---|
| P0 | Guard: no step edits on a generated chaser on a track (locked banner), fix shows.md:85-86 | 3 | 5 | `isGenerated`, `lockedBanner` | 0.5 | L | L | **30** | BUILD (0.4 fix) |
| P1 | Cue-list clip: `CueTimes` ShowFunction, drawn from anchors, tick / band / body drags as tree steps, conflict rules, undo by stamp, "Convert to cue-list clip" | 4 | 5 | Q198 ops, `timecodeAnchors`, SequenceItem, Show offset | 4 | M | M | **4** | BUILD (0.4/0.5) |
| P2 | Timeline clock as a timecode source for clipped lists; offset mapping moved into `Show` (engine); Space cues / Record GOs; Learn against the timeline | 4 | 5 | `TimecodeCues`, `LtcUniverse` pattern, SectionLearn | 3 | M | M | **5.3** | BUILD after P1 |
| P3 | Markers ↔ sections: bound markers, Sections from markers (MarkerPlan on Show markers), Markers from sections, "Cue list here…" merge | 3 | 5 | `MarkerPlan`, `ShowMarker`, `matchKey` | 2.5 | L | M | **6** | BUILD after P1 |
| P4 | Timecode ruler mode, "Timeline 0 =" field in Show settings, announced hour snap, display fps | 3 | 4 | ruler, `TimecodeCues::format/parse` | 1.5 | L | L | **8** | BUILD with P1 |
| — | MTC out from the timeline clock | 2 | 3 | MIDI plugin | 3 | M | H | 1.6 | LATER |
| — | LTC out | 1 | 1 | none | 5 | M | H | 0.2 | NO (audio engine) |
| — | Per-Show clip shift / timing maps | 3 | 4 | venues pattern | 5 | M | M | 1.9 | LATER, with Q198 5 |

Total P0–P4 ≈ 11.5 days. P0 can go now and alone.

**Risks.**
- Hot path: P1 removes a clip's chaser from the lockstep loop, which makes
  that loop cheaper. P2 adds a slot per show tick (50 Hz), so hotpath_test
  needs a case with 3 clipped lists, and `make perf` is due in the release
  that ships it (ShowRunner + TimecodeCues are hot paths).
- Two clocks: P2's "outside timecode wins" must be clear on screen (the
  footer chip names the clock driving the cues).
- Moving the offset mapping out of `ShowManager` touches VC Show Control
  and the calibration dialog. Covered by tests in place today; run them
  before and after.
- The hour auto-snap now creates an undo step. Check it can't fire during
  a show (only while stopped or on first lock).
- Older builds: a `CueTimes` clip plays as a chaser block there (GO cues 3 s
  each). Acceptable, but say it in help.

**Tests (in the gate, all offscreen).**
- engine `timecodeanchors_test` / new `cuelistclip_test`: clip span from
  anchors; body drag = ripple Δ on every start, one undo step; refusal of
  a second clip and of a collision; offset mapping in `Show`, with and
  without the hour snap.
- `showrunner` lockstep skips `CueTimes` clips (the chaser never gets
  `ChaserSetStepIndex` from the show).
- `timecodecues_test`: the timeline clock as a source fires clipped lists
  only, stops when outside timecode starts, jump and dropout rules
  unchanged; no double fire when the Show follows outside timecode.
- `ShowManager_Test`: SequenceItem refuses edits on a generated chaser
  (P0); tick and band drags reach the tree; the Shows tab's ⌘Z undoes a
  tree step made from the timeline, then the timeline snapshot before it;
  bound marker drag moves the section start.
- `layoutsanity_test` + before/after shots at 3 sizes for the clip (Q207).
- Help: shows.md (cue-list clips, timeline clock, timecode ruler),
  show-tree.md (a link to it), course Level 3 lesson 3.5 note; CHANGELOG
  line per phase.

---

## 4. Decisions for Branson

See `questions.json`. In short:

1. Truth: cue times (rec.) / timeline positions / both, with a per-clip
   shift.
2. Dragging a clip: moves the list's times (rec.) / asks each time / is
   refused (move by markers only).
3. Clock: outside timecode wins, timeline clock only without it (rec.) / the
   running Show always drives its clips / chosen per Show.
4. A cue-list chaser already on a track: becomes a clip when its list
   follows timecode (rec.) / stays a chaser block until converted / cue-list
   chasers are refused on tracks.
5. Timecode out: not now (rec.) / MTC out in 0.4 / MTC + Art-Net out.
6. Frame rate: a display rate per Show, times stay ms (rec.) / the source's rate / store frames.
