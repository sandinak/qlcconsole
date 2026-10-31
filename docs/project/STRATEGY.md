# qlcconsole: what it is, and what to build next

**Adopted 2026-10-28.** All thirteen decisions in section 6 were taken as
recommended (the thirteenth, Windows code signing, is "wait"). This is the
project's direction: what qlcconsole is for, what it builds next and in
what order, and what it doesn't build. [CLAUDE.md](../../CLAUDE.md) holds
the short form of the guardrails that every change follows.

Written 2026-10-27 from the repo at 0.3.0 plus the RC8 batch, the help,
the [CHANGELOG](../../CHANGELOG.md), [TODO](TODO.md) and [DONE](DONE.md),
the [design notes](../design/), five design studies (teaching, why-on, pad
Run, DualShock 4, lasers), a survey of the QLC+ forum, the 0.3.1 queue,
upstream QLC+'s source, and a short look at QLC+ 5. Where the plan has
moved since, this copy says so.

Already done in 0.3.1 since it was written: the QLC+ 4.14.4 base and the
first logged upstream intake, the support promise and the QLC+ 5 line in
the README, Save As .qxw, Full Screen, New and Delete for input profiles,
"Start new shows in the Classic view", the DMX Monitor, the note on what a
.qxw save leaves out, and the stock QLC+ 4.14.4 round-trip test in the
gate. The tables below still show them as they were planned.

Effort numbers are the design studies' "one developer, with tests" days.
The project's real throughput with agents is higher than that, but the
ratios between items hold, and ratios are what the ROI column uses.

---

## 0. The short version

- **qlcconsole is a theatre-style lighting console for small crews**: a
  show written as tracked cue lists, focused from the stage with a game
  controller, carried from room to room, and taught to students. It sits
  on the QLC+ 4 engine and keeps QLC+ 4's widgets UI as a first-class
  Classic view. It is not "QLC+ with more features".
- **Its job for QLC+ 4 users** is to be a trustworthy place to keep working:
  their shows open unchanged, the Classic view behaves like 4.14, fixture
  definitions keep arriving, and it runs on the machines they have. Four
  of these are cheap and missing today: a written support promise, a
  "Coming from QLC+ 4" page, a scheduled fixture-library sync, and a gate
  test that stock QLC+ 4.14 opens what qlcconsole saves. The Classic view
  also lost two stock features that matter to some 4.x users: Full
  Screen, and creating or deleting input profiles.
- **0.3.1: Polish and the classroom.** The fix batches, the 4.x home basics,
  Teaching mode, the voice settle and the PlayStation names, and the
  forum quick wins. The bigger studies (pad Run, why-on, the laser
  look and safety) moved out of it: pad Run to 0.4, why-on to 0.5.
- **0.4: Run the show.** One GO path, then pad Run, the phone Run page,
  timecode (LTC and cues at a time), override hits, group masters, effect
  fade-out, the printed cue and tracking sheet, and walking the show from
  the stage with the controller (board Q192).
- **0.5: The show explains itself.** Show Review v1, "Why is this on?" v1
  to v3 with its student mode, the laser and audience safety checks,
  realistic move speed in the Rig Overview, and **AI Assist** (Design mode
  only, off by default, a local model proposing command-line text the
  operator reviews; docs/design/AI_ASSIST.md).
- **The main risk is the one the usability review already found:**
  creeping featurism. Five design studies arrived in one day. The
  guardrails in section 5 are meant to stop the next 102-item menu.

---

## 1. Identity

### What qlcconsole is

qlcconsole is a lighting console for crews who run real shows on modest
rigs: show choir, school theatre, and small touring productions. A show
is written the way theatre writes it, as cue lists that track, with
Build's palettes and the Workbench for the looks. It is planned on a plot
of the actual stage, focused from that stage with a game controller and a
voice telling the person on the deck which light they're moving, and
carried from venue to venue with each room's house rig, patch and focus
corrections kept beside the show. It is built to be taught: a course,
teaching shows, scramble practice and a focus report are part of the
product, not extras. Underneath it is the QLC+ 4 engine and its widgets
UI, kept alive and tested, so people who grew up on QLC+ 4 can open their
shows, work in the Classic view they know, and step into cues, venues and
focus when they want to. It is not a replacement for QLC+ 5, not a
big-console clone, and not a media server.

### Principles that decide what gets built

1. **The cue list is the spine.** New show-running features attach to the
   Show Tree and the Run tab (cues, sub-cues, hits, productions). They
   don't add another way to play a show.
2. **The room is real.** A feature that knows about the stage, the plot,
   the venue or the focus is in character. One that ignores them should
   have a good reason to.
3. **One model per job.** Reuse the Show Tree, Build/Workbench, venues,
   focus calls, Show Status and the controller schemes. No parallel
   systems. If something needs a second path, the first path is wrong.
4. **It has to be teachable.** Each feature ships with help, a course note
   where it fits, and words a 15-year-old crew member can follow without
   being talked down to.
5. **A QLC+ 4 show is safe here.** Open it, run it, save it, and nothing
   changes unless the user asks. The Classic view is a promise, not a
   leftover.
6. **Reliability beats features.** Every change passes the three-leg gate.
   A bug found at a show outranks any roadmap item. Nothing installs
   during a show, and a pad picked up mid-show never moves it.
7. **Small rigs, small machines.** Design for a laptop at a school, a few
   hundred fixtures and Art-Net/sACN nodes. Don't design for 40 universes
   of pixels or a networked multi-console session.
8. **Respect for upstream.** qlcconsole stands on QLC+. It takes fixes and
   fixtures from upstream with credit, sends useful fixes back, and never
   markets itself against QLC+ 5.

### Who it serves

| Audience | What they need from it | Already strong |
|---|---|---|
| Show choir and school theatre crews | Cues that track, a GO button, looks that are quick to build, a stage they can see | Show Tree, Run, Build/Workbench, Lighting Studio, Rig Overview |
| Teachers | A course, safe practice shows, something to grade | Course Levels 0, 1 and part of 3; teaching show; scramble practice; focus report |
| Small touring shows | One show, many rooms; fast focus; paperwork | Venues, house rig, focus calls with pad and voice, patch list and labels |
| QLC+ 4 users who don't want QLC+ 5 | Their shows, their workflow, a maintained app | Classic view, .qxw opens and saves, Upgrade This Show… is optional |

### Who it deliberately doesn't serve

- **People happy on QLC+ 5.** They have a maintained, cross-platform tool
  with a big community. Nothing here should try to move them.
- **Big-console operators** (MA, Eos, Hog) who want an offline editor or a
  backup desk for a large rig. qlcconsole borrows their vocabulary
  (tracking, Block, Cue-only, command line), not their scale.
- **Media servers, pixel mapping at scale and DJ/club automation.** NDI,
  video mapping, Ableton Link and an audio engine are other tools' jobs.
  qlcconsole takes cues from QLab and DAWs; it doesn't become one.
- **Multi-operator networked editing.** The model is one operator and one
  laptop, plus a phone or tablet for GO.

### How it talks about QLC+ 5

- Respectful and factual. QLC+ 5 is the upstream project's current
  version, and its developers carry most of the work qlcconsole builds on.
- No "switch from 5" marketing, no feature-comparison tables, no claims
  that 4.x is abandoned. Upstream released 4.14.4 in March 2026 and its
  notes say the author is concentrating on 5. "QLC+ 4 is in maintenance;
  qlcconsole keeps its widgets UI going" is accurate. "QLC+ 4 is dead" is
  not, and it isn't ours to say.
- Suggested README line, replacing nothing that's there now, added under
  the "not affiliated" paragraph: *"If you use QLC+ 5 and it works for
  you, keep using it. qlcconsole is for people who prefer the QLC+ 4
  desktop UI, and for crews who want a theatre cue list, venues and
  stage-side focus."*

---

## 2. A home for QLC+ 4 users

What a QLC+ 4 user needs to feel at home and to trust the tool for years.
Concrete gaps first, then what to promise.

### 2.1 Opening their shows unchanged

Today:
- `.qxw` opens; Save writes the format it was opened in; an old show opens
  in the Classic view; nothing converts on load
  ([WORKFLOW_DESIGN](../design/WORKFLOW_DESIGN.md) §3.7, §5); Upgrade This
  Show… is optional.
- When this was written the fork stood on upstream master of early
  February 2026, without the 4.14.3 and 4.14.4 releases or the upstream
  engine race fixes since. *Since then:* 0.3.1 took both releases' fixes
  and moved the base to QLC+ 4.14.4; [UPSTREAM.md](../dev/UPSTREAM.md)
  logs what was taken, adapted and skipped.

Gaps:
- **No test proves stock QLC+ 4.14 can open what qlcconsole saves.** The
  docs say stock QLC+ skips the fork's elements, which is the right design,
  but nothing in the gate loads a qlcconsole-saved .qxw with a stock 4.14
  engine. That's the one promise a 4.x user is trusting, and it isn't
  tested. (Section 2.2 has the audit's specific findings.)
- **No upstream intake routine** (*since then:* the first intake is logged
  in [UPSTREAM.md](../dev/UPSTREAM.md), with how to do the next one).
  Upstream engine and plugin fixes arrived only when someone happened to
  merge master, with no list of what was taken, skipped or why.

### 2.2 Classic view parity

Checked against stock 4.14 / upstream `ui/src` (file:line from the fork
unless marked). The two marked *verified* were re-checked by hand.

Blockers for some 4.x users:
- **Full screen is gone** (*verified*). Stock has Control ▸ Toggle Full
  Screen (Ctrl+F11, upstream `ui/src/app.cpp:755`) and `-f/--fullscreen`;
  the fork has neither. On Windows and Linux there's no way to run the
  app full screen, which kiosk and Pi users rely on. ≈0.5 day.
- **Input profiles can't be created or deleted** (*verified*). Stock's
  patch editor has a Profile tab with Add / Remove / Edit
  (`slotAddProfileClicked`, upstream `inputoutputpatcheditor.cpp:691,749`).
  The fork keeps only "Edit" on an existing profile
  (`inputoutputpatcheditor.cpp:270-310`). A MIDI or OSC controller user
  with no bundled profile is stuck. ≈1 day.
- **A .qxw saved here isn't quite the show stock sees.** Venue house
  fixtures go to the venue sidecar, not the .qxw (`engine/src/doc.cpp:2645`),
  so stock QLC+ doesn't see them at all. Fork-only blocks (Show Tree, show
  control, speed masters, output rules, power, timecode, network, three VC
  widgets) are skipped by stock and **dropped if stock saves the file**.
  That's expected, but nothing warns the user, and Show Review's FILE-05
  warning is design only. Fix: a one-time notice on saving a .qxw that
  has fork-only content, listing what stock QLC+ would lose (≈1.5 days),
  plus the round-trip test (section 2.1).
- **No Raspberry Pi package** (section 2.4).

Annoying:
- **Save As offers only .qlcc** (`app.cpp:3372`); a .qxw needs the
  extension typed. Add the .qxw filter. ≈0.25 day.
- **The DMX-values Monitor isn't reachable in Classic.** The Monitor
  button is now the Rig Overview; the stock DMX monitor lives inside the
  Lighting Studio, which switches the view to Everything
  (`app.cpp:4332-4344`). Give Classic a Monitor that opens the DMX view
  (or the Studio in its own window without changing view). ≈1 day.
- **Help is online-only for QLC+'s own pages** (Shift+F1, `app.cpp:2091`);
  stock bundled them. Low cost to leave; mention it in the migration page.
- **Translations are stale.** The fork's strings are in `tr()`, but
  `lupdate` hasn't run since the rebrand (2026-08-10); `qlcconsole_de_DE.ts`
  has 1,565 messages against about 7,440 `tr()` calls. Non-English users
  see a mix. Running `lupdate` each release is cheap; translating is
  community work (LATER).
- **Settings and recent files don't carry over** from QLC+ (different
  QSettings app name). The user folder is linked in
  (`QLCFile::mergeLegacyUserData`), which covers fixtures, profiles and
  scripts. A one-time "import QLC+ settings" would finish the job.
  ≈1 day.
- **A new show opens in the qlcconsole view** (`app.cpp:749-752`). A 4.x
  user who prefers Classic has to set it on every new show. A per-user
  "start new shows in Classic" preference fixes it. ≈0.5 day.
- **Show Manager differs a lot from stock** (3,004 lines against 1,803),
  and no one has checked its parity. Worth a review before calling
  Classic a promise. ≈2 days.

Cosmetic (leave, but list on the migration page): Stop all, Blackout and
Design/Operate moved from the toolbar to the top bar (still in Control);
autosave every 5 minutes; a different footer; Blind; three extra VC
widgets; web access bound to localhost unless `-wb` (a deliberate
security change; say so).

Kept and fine: the per-tab toolbars, the Audio tab, plugin configuration,
hotplug, passthrough, every stock command-line option except `-f`, the
plugin set (plus WLED), the fixture editor and web access.

### 2.3 Fixture definitions now that 4.x is in maintenance

Today:
- The fork ships 1,720 definitions; upstream master has 1,738. 28 are
  upstream-only (new since the last merge), 10 are fork-only, and 4 shared
  files differ.
- Definition Updates (Fixtures ▸ Definition Updates…) is good: it finds
  stale copies, shows what an update changes, never overwrites a
  customised file, and can undo. A .qlcc carries its own copies.
- What's missing is the **supply**. New definitions only arrive when
  upstream master is merged.

Recommendation, in order of ROI:
1. **A library sync script, run every release** (≈2 days to build, ≈1 hour
   a release after). It copies upstream's `resources/fixtures/**.qxf` (and
   the manufacturer index) into the fork, keeps fork-only files, reports
   the 4 kinds of change (new, changed, fork-only, conflicting), and runs
   the fixture-definition check and the Definition Updates logic on the
   result. Upstream's definitions are Apache-2.0 like the fork, and the
   format is the same in 4 and 5 (upstream's engine reads both UIs' files).
   Credit in CHANGELOG: "N definitions from QLC+ upstream".
2. **Send the fork's own definitions upstream** (some are already on
   `fork/fixture-*` branches against upstream). That's the
   community-friendly half of the same pipeline, and it keeps the 10
   fork-only files from drifting.
3. **The Open Fixture Library route for GDTF**, documented, not built:
   OFL converts GDTF to .qxf. One help paragraph covers "my fixture is
   only in GDTF".
4. **Native GDTF import: LATER.** Upstream says it needs funding and about
   a month; the forum demand is real but small for this audience, whose
   rigs are mostly common fixtures. Revisit if a school brings a fixture
   only GDTF describes.

### 2.4 Plugins and operating systems

| Platform | Today | What a 4.x user expects | Gap |
|---|---|---|---|
| macOS 12+ (universal) | Signed, notarized DMG; the gate's home | Same | none |
| Windows 10/11 x64 | CI installer, **not code-signed**; tested on a Win10 VM that's "unreliable"; no Windows 11 machine in the matrix | An installer that school IT will allow | signing; a real Win11 check |
| Linux x86_64 | CI AppImage; a Debian 13 build host | .deb or AppImage | `.spec`/`debian/changelog` inherited and stale ([RELEASE.md](../dev/RELEASE.md)) |
| Raspberry Pi (arm64) | Builds on the Pi 5 (apt Qt 6.8.2, ~34 min); no package; Pi 4 build host still TODO | QLC+ 4 had an official Pi image; Pi users are a real 4.x group | no released build; MasterTimer late ticks on Linux (TODO) |
| Plugins | Art-Net, sACN, DMX USB, uDMX, Velleman, Peperoni, Enttec Wing, HID, MIDI, OSC, OS2L, OLA, loopback, GPIO/SPI/UART (Linux), plus WLED | Same as 4.14 | ShowNet and single-vendor dongles only on demand |

Recommendation: promise macOS, Windows and Linux x86_64 now. Call the Pi
"builds, not packaged" until an arm64 AppImage or .deb exists (0.5).
Price a Windows code-signing certificate when the first school on Windows
appears; school IT is the most likely place an unsigned installer is
refused.

### 2.5 A long-term-support promise

There was no written promise, and SUPPORT.md pointed "general QLC+
questions" to the upstream forum, where 4.x answers are thinning out (and
which isn't fair to send to upstream for a fork's users). *Since then:*
[SUPPORT.md](../../SUPPORT.md) carries the promise, adapted from this text
(short, honest, no dates we can't keep):

> **What qlcconsole promises QLC+ 4 users**
> - Every show QLC+ 4.14 can open, qlcconsole opens, plays and saves
>   without changing it, and stock QLC+ 4.14 can open it again. This is
>   tested on every build.
> - The Classic QLC+ view stays: QLC+ 4's tabs, names and toolbars. New
>   features never require leaving it to keep a show working.
> - Fixture definitions from QLC+ upstream are brought in every release.
> - macOS, Windows and Linux builds, on Qt 6, for as long as the project is
>   maintained. Security and OS-breakage fixes come first.
> - qlcconsole is maintained by one person. If that changes, this page says
>   so, and the last release stays downloadable with its source.

The last bullet matters. A single maintainer can't promise forever, but
can promise to say so.

### 2.6 The migration document

There's no "Coming from QLC+ 4" page. The pieces are scattered: views in
getting-started.md, "Coming from the Functions window" in build.md,
Upgrade This Show… in the Show Tree page. A 4.x user needs one page,
≈1.5 days with pictures:

1. Your show opens as it was (Classic view, nothing converts).
2. What's the same, tab by tab, and what moved (Inputs/Outputs → its
   Universes page in Connections, Functions → also Build ▸ Scenes ▸
   Classic, the Monitor → the Rig Overview).
3. What's new that you can ignore (and how to hide it: the Classic view).
4. When you're ready: Upgrade This Show…, the qlcconsole view, the course
   Level 1 as a self-study path.
5. Going back: what stock QLC+ 4 keeps and drops (palettes, the Show Tree,
   venues are lost; scenes, chasers, VC and the patch are kept).
6. Where to get help (this repo's issues, not upstream's forum).

Put it in the help (help.qrc, kTopics), link it from the README's "QLC+
shows (.qxw) open in qlcconsole" line, and from the Classic view's
footer the first time a .qxw opens.

---

## 3. ROI table

How to read it:
- **Value** to the core audience, 1-5. **Fit** with the identity, 1-5
  (5 = deepens what qlcconsole is; 1 = copies QLC+ 5 or a big console).
- **Effort** in developer-days, from the design studies where one exists.
- **Risk**: L (×1.0), M (×0.8), H (×0.6).
- **ROI** = Value × Fit ÷ Effort × risk factor, rounded. Above 5 is
  excellent, 2-5 good, 1-2 fair, under 1 poor.
- **Maint.** is the ongoing cost: L (none to speak of), M (tests and help
  to keep current), H (moving parts: hardware, protocols, security).
- **Call**: BUILD (0.3.1), BUILD (0.4), BUILD (0.5), LATER, NO.

Sorted by ROI within each group. The calls weigh ROI first, then the
release's story and dependencies; where a call departs from the ROI
order the note says why.

### 3.1 The 4.x home

| Item | Value | Fit | Reuses | Effort | Risk | Maint. | ROI | Call |
|---|---|---|---|---|---|---|---|---|
| LTS promise in SUPPORT.md + README line on QLC+ 5 | 4 | 5 | existing docs | 0.5 | L | L | **40** | BUILD (0.3.1) |
| "Coming from QLC+ 4" help page | 5 | 5 | views, build.md, Upgrade This Show | 1.5 | L | L | **17** | BUILD (0.3.1) |
| Stock round-trip test: stock 4.14 engine loads a qlcconsole-saved .qxw | 5 | 5 | engine tests, QLC+_4.14.2 tag | 2 | L | L | **12.5** | BUILD (0.3.1) |
| Fixture library sync script, every release | 5 | 5 | Definition Updates, fixture check | 2 | L | L | **12.5** | BUILD (0.3.1) |
| Upstream intake: review upstream master each release, take engine/plugin fixes, log taken/skipped | 4 | 5 | git upstream remote | 3 | M | M | **5.3** | BUILD (0.3.1), then every release |
| Save As offers .qxw | 3 | 5 | file dialog | 0.25 | L | L | **60** | BUILD (0.3.1) |
| Restore Full Screen (menu, Ctrl+F11, `-f`) | 4 | 5 | stock code | 0.5 | L | L | **40** | BUILD (0.3.1) |
| Restore creating and deleting input profiles | 4 | 5 | stock patch editor | 1 | L | L | **20** | BUILD (0.3.1) |
| A DMX Monitor reachable in Classic | 3 | 5 | Studio's DMX view | 1 | L | L | **15** | BUILD (0.3.1) |
| "Start new shows in Classic" preference | 3 | 5 | views | 0.5 | L | L | **30** | BUILD (0.3.1) |
| Notice when a .qxw save holds content stock QLC+ will drop | 4 | 5 | Show Review FILE-05 idea | 1.5 | L | L | **13** | DONE (0.3.1: counts by kind, Teaching/Operate/web settings and Effect Control widgets named, "Don't ask again for this show") |
| Import QLC+ settings and recent files once | 2 | 4 | mergeLegacyUserData | 1 | L | L | 8 | BUILD (0.3.1) |
| Notice when a .qxw save holds content stock QLC+ will drop | 4 | 5 | Show Review FILE-05 idea | 1.5 | L | L | **13** | BUILD (0.3.1) |
| Import QLC+ settings and recent files once | 2 | 4 | mergeLegacyUserData | 1 | L | L | 8 | DONE (0.3.1, LegacySettings: asked once, Settings ▸ General ▸ QLC+ 4 later) |
| Run `lupdate` every release | 2 | 3 | Qt .ts | 0.5 | L | L | 12 | BUILD (0.3.1), then every release |
| Show Manager parity review | 3 | 5 | Shows tab | 2 | L | L | 7.5 | BUILD (0.3.1) |
| Send the fork's definitions upstream | 2 | 4 | fork/fixture-* branches | 1 | L | L | 8 | BUILD (0.3.1), user's call |
| Raspberry Pi arm64 package (AppImage or .deb) | 3 | 4 | Pi 5 build recipe | 4 | M | M | 2.4 | BUILD (0.5) |
| Windows code signing | 3 | 3 | CI installer | 2 + a yearly fee | L | M | 4.5 | LATER (when a Windows school appears) |
| Translations of the fork's strings | 2 | 3 | Qt .ts | 10+ | L | H | 0.6 | LATER |
| Native GDTF/MVR import | 3 | 2 | definition cache, plot | 15 | M | H | 0.3 | LATER (OFL route documented instead) |

### 3.2 The 0.3.1 queue

| Item | Value | Fit | Reuses | Effort | Risk | Maint. | ROI | Call |
|---|---|---|---|---|---|---|---|---|
| Help pages answering forum asks (override slider, ⌘D dump, Force LTP, MIDI CC out) | 3 | 4 | the help | 0.5 | L | L | **24** | BUILD (0.3.1) |
| File ▸ New from Template (show-choir stage first) | 4 | 5 | course's classroom show | 1 | L | L | **20** | BUILD (0.3.1) |
| Voice settle | 3 | 5 | OperatorVoice | 1 | L | L | **15** | BUILD (0.3.1) |
| Operate PIN, folded into Teaching's PIN and Show Settings; "open in Operate" setting | 3 | 5 | Design/Operate, Teaching PIN | 1 | L | L | **15** | BUILD (0.3.1), with (t) |
| Art-Net TimeCode receive into the MTC follow path | 4 | 5 | MTC follow, Art-Net plugin | 1.5 | M | M | **11** | BUILD (0.3.1) |
| Plot locking: start locked setting, inherited lock in Layers | 3 | 4 | itemlock, Layers tree | 1 | L | L | **12** | BUILD (0.3.1) |
| Gel colour on fixed lights (Rig Overview, patch list) | 3 | 4 | Rig Overview, patch list | 1 | L | L | **12** | BUILD (0.3.1) |
| Tonight's running order (reorder productions for Run) | 4 | 5 | productions, Run | 2 | L | L | **10** | BUILD (0.3.1) |
| Fade in/out times on hits | 4 | 5 | Hits | 2 | M | M | 8 | BUILD (0.3.1); the override layer is 0.4 |
| Scoped hits: a hit lives under a cue, list or production, or everywhere; a VC button holds several and fires the armed one (Q154); then the button map, same-scope prompt and override marks (Q157) | 4 | 5 | Hits, Show Tree runtime, VC buttons | 4 | M | M | **4** | BUILD (0.3.1): phase 1 places + arming, phase 2 Q157 |
| Momentary / start-only / stop-only on Control Map bindings | 2 | 3 | Control Map | 1 | L | L | 6 | DONE (0.3.1, with the Control Map back: learn in Build and the Show Tree, saved with the show) |
| Back view in the structure editors | 3 | 4 | StructureStudioView planes | 2 | L | M | 6 | BUILD (0.3.1) |
| Fix batch: leaks, TSan, compat C2/C4-C7, Art-Net sequence per node, Windows relink, red-team items | 4 | 5 | the gate | 5 | L | L | 4 | BUILD (0.3.1), first |
| Remaining plot alignment faults + structure-editor coverage | 3 | 4 | plotalignment test | 3 | L | L | 4 | BUILD (0.3.1) |
| Lasers: one shared fixedThrow, Face for fixed units, yoke tilt with migration | 3 | 4 | RigHeads, studio placement | 3 | M | M | 3.2 | BUILD (0.3.1) |
| PlayStation names and glyphs (PadNames table, tokens, map drawing) | 3 | 4 | controller schemes | 3 | L | L | 4 | BUILD (0.3.1); pad Run needs it |
| Controller slots: one input from any of several pads (Xbox here, DualShock 4 there), offer-to-adopt, mapping by gamepad role; DualSense audit (Linux layout, stock input profiles) | 4 | 5 | device-following inputs, controller profiles, PadNames | 2 | M | M | **8** | DONE (0.3.1, user's ask 2026-10-30: a travelling show meets a different pad at each place; a DualSense arrived the same day) |
| Teaching mode, must-haves (Show Settings, chip, Operate warning, safe output, PIN, Make Student Handout) | 5 | 5 | scramble practice, Design/Operate, .qlcc | 7 | M | M | 2.9 | BUILD (0.3.1); see note 1 |
| Touchpad option A | 2 | 3 | HID plugin, focus tools | 3.5 | M | M | 1.4 | LATER (0.4 if pad Run users ask) |
| Laser aperture face, U yoke, LaserLook symbol | 2 | 3 | Rig Overview | 4 | L | M | 1.5 | LATER |
| Star-field coverage + laser audience/eye-height warnings | 3 | 4 | drawStarField, Show Review | 5 | M | M | 1.9 | BUILD (0.5), with Show Review |
| Pad Run role (MVP 6 d, full 10.5 d) | 4 | 5 | ShowControl::execute, focus pad stack | 6 | M | M | 2.7 | BUILD (0.4); see note 2 |
| Walk the show from the stage with the controller (extends Focus this cue) | 4 | 4 | Refine, Layer/Group/Fixture scope, voice and light-bar feedback, cue notes | 5 | M | M | 2.5 | BUILD (0.4); user's call 2026-10-30 (Q192), see TODO.md |
| "Why is this on?" v1 | 4 | 5 | GenericFader, Show Tree tracking | 9 | M | M | 1.8 | BUILD (0.5); see note 3 |
| Teaching later items: lesson link, reset to handout, GO log, glossary popovers | 3 | 5 | Teaching | 4 | L | L | 3.8 | BUILD (0.5) |

Notes:
1. **Teaching mode** has a lower ROI number than its call suggests,
   because it's a week of work. It's still a 0.3.1 item: the course runs
   "through the winter", the Operate PIN and safe output close real
   classroom risks, and why-on's student mode and Show Review's teaching
   checks both depend on it.
2. **Pad Run** is the best single feature for the "run the show" story,
   but it should land after the one-GO-path change (`App::runCommand`)
   and the PS names, and it belongs with the other run features in 0.4.
3. **Why-on v1** is 8-10 days and its value grows with Teaching and Show
   Review. Built in 0.5 alongside them, it becomes the "the show explains
   itself" release instead of a lone inspector.

### 3.3 Forum top 20 and the rest of the design backlog

| Item | Value | Fit | Reuses | Effort | Risk | Maint. | ROI | Call |
|---|---|---|---|---|---|---|---|---|
| AI Assist P0–P3 (docs/design/AI_ASSIST.md): one operation registry under the command line, a Design-only assistant on a local model (Ollama), configuration import, an MCP server | 4 | 4 | Show Tree command line, undo, Design/Operate | 15 | M | M | 1.1 | BUILD (0.5), user's call 2026-10-30 (P2b configuration import also stands alone) |
| One GO path (`App::runCommand`) for keys, Run tab, footer, pad, web | 4 | 5 | ShowControl, RC8 preview hold | 0.5 | L | L | **40** | BUILT 2026-10-31 (0.4, first; docs/design/RUN_COMMANDS.md) |
| Tracking sheet + printable cue sheet (PDF/XLSX) | 5 | 5 | tracking resolve, patch-list exports | 4 | L | L | **6.3** | BUILD (0.4) |
| One GO path (`App::runCommand`) for keys, Run tab, footer, pad, web | 4 | 5 | ShowControl, RC8 preview hold | 0.5 | L | L | **40** | BUILD (0.4), first; prerequisite |
| Tracking sheet + printable cue sheet (PDF/XLSX) | 5 | 5 | tracking resolve, patch-list exports | 4 | L | L | **6.3** | BUILT (0.4, sheet, 2026-10-31): More ▾ → Cue Sheet… |
| Course Level 2 lessons (7), one per release cycle | 5 | 5 | course template, courseshow_test | 2 a lesson | L | M | **12.5** a lesson | BUILD (0.3.1 onward) |
| Realistic move speed in the Rig Overview | 3 | 5 | Rig Overview, definitions' speeds | 3 | L | L | 5 | BUILD (0.5) |
| LTC (audio) timecode in, plus cues at a timecode | 4 | 5 | MTC follow, audio capture | 4 | M | M | 4 | BUILD (0.4): **LTC in built** (feat/04-ltc); cues at a timecode next, after the One GO path |
| Effect fade in / fade out (depth to 0 on release) | 4 | 5 | layered looks | 4 | M | M | 4 | BUILD (0.4) |
| Group / intensity masters on faders | 4 | 5 | groups, speed masters, Control Map | 4 | M | M | 4 | BUILD (0.4) |
| QLab out (OSC / MSC out) | 3 | 4 | Show control page | 3 | L | M | 4 | BUILT (0.4) |
| Override / priority hits (Normal / Override / Inhibit, restore on release) | 5 | 4 | Hits, OutputLayer | 5 | M | M | 3.2 | BUILD (0.4) |
| Phone/tablet Run page (current, next, GO, Back, Hold) | 4 | 5 | secured web access, CueRunner | 5 | M | H | 3.2 | BUILD (0.4) |
| Calling script view (cues on lyric or script lines) | 3 | 4 | cue notes | 4 | L | M | 3 | BUILD (0.5) |
| Encoder banks: MIDI controller drives the selection in Build | 3 | 4 | Focus-this-look mapping | 4 | L | M | 3 | LATER |
| Duplicate a structure's setup (T1 → T2, T3) | 3 | 4 | Patch Others Like This | 4 | L | M | 3 | LATER |
| W6 shared Library + command bar in Build | 3 | 4 | Show Tree Library, ShowTreeCommand | 4 | M | M | 2.4 | LATER |
| Show Review v1 (52 checks, CLI, panel) | 4 | 5 | ShowStatus, LookStackCheck, tracking | 8 | L | M | 2.5 | BUILD (0.5) |
| Plot handles (drag ends, ⌥-rotate) | 3 | 4 | Lighting Studio | 5 | M | M | 1.9 | LATER |
| Stage and inventory across show files | 3 | 4 | inventory, structures | 5 | M | M | 1.9 | LATER |
| RDM find-and-address in the Patch dialog | 4 | 5 | RDM manager, Patch dialog | 6 | H | H | 2 | LATER (needs the rig) |
| Signalling: stand-by/go cue lights, curtain | 3 | 4 | curtain fixture, cues | 6 | M | M | 1.6 | LATER |
| Timeline waveform + markers | 3 | 3 | Shows timeline | 4 | L | M | 2.3 | LATER (copies 5) |
| Stream Deck / Companion module | 3 | 3 | OSC in | 4 | L | M | 2.3 | LATER |
| People on stage as a layer (whole choir) | 2 | 4 | Person target | 4 | L | M | 2 | LATER |
| App-wide UI zoom (⌘+ / ⌘-), useful on a classroom projector | 3 | 3 | none | 5 | M | M | 1.4 | LATER |
| One object editor (merge Fixture Properties and Edit in Studio) | 3 | 4 | both editors | 6 | M | M | 1.6 | LATER |
| Shutters on profiles in Focus | 3 | 4 | focus tools, beams | 10 | M | H | 1 | LATER |
| W7 conversions (EFX → movement effect, RGB Matrix → matrix effect) | 2 | 3 | effect designer | 4 | M | M | 1.2 | LATER |
| Power by intensity per circuit | 2 | 3 | power model | 5 | M | M | 1 | LATER |
| MCP server / plain-language command line | 2 | 2 | engine headless load | 8 | M | H | 0.4 | LATER |
| Relative / multiply layers (tilt over an effect, masks) | 2 | 3 | look stacking | 10 | H | H | 0.4 | NO for now: pro-console semantics, engine risk |
| Fixture Editor inside the main window | 1 | 2 | fixture editor | 5 | M | M | 0.3 | NO: the separate editor works |
| Ableton Link | 1 | 1 | none | 5 | H | H | 0.1 | NO: GPL, not this audience |
| NDI / video pixel mapping | 1 | 1 | none | 20 | H | H | 0.03 | NO: media-server territory |
| Audio engine growth (JACK, surround, per-track volume) | 1 | 1 | none | 10 | M | H | 0.1 | NO: QLab's job |
| OSCQuery | 1 | 2 | OSC | 3 | M | H | 0.5 | NO: attack surface, little gain |
| Concurrent multi-user editing | 1 | 1 | none | 20 | H | H | 0.03 | NO: one operator, one laptop |
| QLC+ 5 features for parity (VC z-index/distribute, RGB Matrix overrides, video fades) | 1 | 1 | 4.x VC | 3-5 each | L | M | under 0.3 | NO: parity for its own sake |

### 3.4 Top 10 by ROI

Leaving out the docs and content items, which are covered after the
list:

1. Save As offers .qxw (60).
2. One GO path, `App::runCommand` (40). It's small, and it unblocks pad
   Run, the phone Run page and override hits. **Built 2026-10-31**
   (docs/design/RUN_COMMANDS.md).
3. Restore Full Screen (40).
4. "Start new shows in Classic" preference (30).
5. Restore creating and deleting input profiles (20) and New from
   Template (20).
6. Voice settle (15), the Operate PIN folded into Teaching (15), and a DMX
   Monitor in Classic (15).
7. Notice when a .qxw save holds content stock QLC+ will drop (13).
8. Stock round-trip test and fixture library sync (12.5 each).
9. Art-Net TimeCode receive (11) and tonight's running order (10).
10. Tracking sheet and printable cue sheet (6.3), the highest-ROI large
    feature. **Built** (2026-10-31).

The docs and content items beat most of these: the LTS promise (40),
the forum help pages (24), "Coming from QLC+ 4" (17) and each Level 2
course lesson (12.5). The first three cost under three days together.
The pattern is clear: the cheapest, highest-return work right now is
making the QLC+ 4 path trustworthy, not adding features.

---

## 4. Roadmap

### 0.3.1: Polish and the classroom (about 4 weeks)

The story: *the 0.3 release, made solid, made safe for a classroom, and
made a clear home for QLC+ 4 users.*

Order:
1. **Fix batch, then the remaining plot alignment faults.** Reliability first; the leaks, TSan and
   compat items are the base everything else is gated on.
2. **The 4.x home basics**, in parallel with 1 because they're mostly docs
   and tooling: LTS promise and README line → stock round-trip test →
   library sync script (+ first sync) → upstream intake review → Classic
   parity fixes (Full Screen, input profiles, .qxw in Save As, the
   stock-loss notice, Monitor, a Classic default, settings import,
   `lupdate`, the Show Manager review) → "Coming from QLC+ 4" page last, so
   it describes the fixed app.
3. **Small items**: plot lock setting, Back view, voice settle, gel
   colour, momentary triggers, forum help pages.
4. **PlayStation names and glyphs** (without the touchpad). Pad Run in
   0.4 depends on it.
5. **Teaching mode must-haves**, with the Operate PIN folded in, then
   **New from Template**, which can ship the classroom show with Teaching
   on.
6. **Run-side quick wins**: Art-Net TimeCode, running order, hit fade
   times.
7. **Lasers part A** (one fixedThrow, Face, yoke tilt), if time allows;
   otherwise first in 0.4.
8. **Course**: Lesson 2.1 (cue modes, Release, base cue), so Level 2 starts
   when the winter does.

Out of 0.3.1, on purpose: pad Run, why-on, laser B and C, touchpad. They
are each a week or more, and each fits a later story better.

### 0.4: Run the show (about 6-8 weeks)

The story: *a student or a stage manager runs the whole show with
confidence, from the console, a game controller or a phone, to a backing
track.*

Order and dependencies:
1. **One GO path** (`App::runCommand`). Everything below goes through it.
   **Built 2026-10-31:** every route enters through `ShowControl::run()`
   with its `Route` (docs/design/RUN_COMMANDS.md).
2. **Pad Run MVP**, after 0.3.1's PS names and the voice settle. Then its
   Hits page, list cycling and the focus confirm.
3. **Phone/tablet Run page** on the secured web access, using the same GO
   path and Teaching's rules (a PIN-guarded "allow remote GO").
4. **Timecode**: LTC in (built: a third source into the same follow path),
   then "cue N at 00:01:23:10" on cue lists, building on 0.3.1's Art-Net
   TimeCode and waiting for the One GO path.
5. **Override hits** (priority layer, restore on release), building on
   0.3.1's hit fade times. Then **group intensity masters**, which share
   the same engine stage.
6. **Effect fade in/out.**
7. **Tracking sheet and printable cue sheet**, the stage manager's paper.
   **Built** (2026-10-31): More ▾ → Cue Sheet… on the Show Tree tab.
8. **QLab out** (OSC/MSC), if the show-control users ask.
8. **QLab out** (OSC/MSC) -- built: a GO sent on, per show, from the Show control page.
9. **Walk the show from the stage** (Q192, extends Focus this cue and
   Refine): cues, layers and "Needs work" tags from the controller.
10. **Course**: Lessons 2.2-2.4, and a Level 3 lesson on running from the
   pad or phone.

### 0.5: The show explains itself (about 6-8 weeks)

The story: *the console tells you what's wrong with the show and why a
light is doing what it's doing, in words a student can learn from.*

Order and dependencies:
1. **Show Review v1** (52 checks, CLI, panel), with Teaching's checks and
   the course shows reviewed in the gate.
2. **"Why is this on?" v1**, then v2 (per-writer provenance, after the
   80-universe stress run), then **v3's student mode**, which needs
   Teaching (0.3.1) and reads best next to Show Review.
3. **Laser part C** (coverage and audience/eye-height warnings) and the
   **no-light zones** idea, both as Show Review checks rather than a
   separate system.
4. **Realistic move speed in the Rig Overview**, which also feeds Show
   Review's "moves in light" checks.
5. **Teaching's later items** (lesson link, reset to handout, GO log,
   glossary).
6. **Calling script view**, if the course's SM lesson wants it.
7. **Raspberry Pi package**, so a school can leave a Pi as the show
   machine.
8. **Course**: the rest of Level 2, and Level 3's remaining lessons.

Dependency chain at a glance:

```
fix batch ─┐
4.x basics ─┼─> 0.3.1
PS names ───┼──────────────> pad Run (0.4)
voice settle┘                   ^
Teaching ─────┬─> phone Run rules (0.4)
              ├─> why-on student mode (0.5)
              └─> Show Review teaching checks (0.5)
one GO path (0.4, built) ─> pad Run, phone Run, override hits
Art-Net TC (0.3.1) ─> LTC + cues at timecode (0.4)
hit fade times (0.3.1) ─> override hits ─> group masters (0.4)
Show Review v1 ─> laser C, no-light zones, move speed checks (0.5)
```

---

## 5. Risks to the identity, and guardrails

### The risks

1. **Scope creep.** The usability review already found overlapping paths,
   102-item menus and 8+ tabs, and a freeze was needed to fix it. On
   2026-10-27 alone, five new design studies were written, with about 70
   days of work in them. The forum survey adds 20 more. If even half land
   without a filter, the 0.3 clean-up is undone by 0.5.
2. **The size of what's maintained.** The fork adds about 450,000 lines
   over QLC+ 4.14.2 in the engine, UI, plugins and tests, on top of the
   inherited 4.x code that upstream no longer develops much. The fork
   now owns the widgets UI, the Qt 6 port and its platform quirks (the
   Qt 6.11 macOS resize bug in RC8 is an example).
3. **Upstream drift.** About 400 upstream commits since the last merge,
   some of them engine race fixes. The longer between intakes, the harder
   each one gets, and engine files are where the fork and upstream both
   change things.
4. **Gate time.** 200 test programs, about 15 minutes a gate on a
   10-core machine, and several long serial tests. Every feature adds to
   it. A gate that takes 30 minutes gets skipped.
5. **One maintainer.** Everything runs through one person's review, one
   person's hardware and one person's shows. Agents scale the typing, not the judgement or the rig checks. "Needs the real rig"
   items in TODO.md are already a long list.
6. **Hardware debt.** Many shipped features are tested offscreen only
   (controllers, WLED, Art-Net reporting, RDM). A school finding a
   hardware bug at a show hurts trust more than a missing feature.
7. **Identity drift by comparison.** Each "QLC+ 5 has X" or "Eos does Y"
   ask is easy to justify alone. Together they make qlcconsole a worse
   copy of both.

### Guardrails

1. **A feature must attach to an existing model**: the Show Tree, Build
   and the Workbench, venues, focus calls, Show Status/Show Review, or the
   controller schemes. A proposal that needs a new top-level model, tab or
   playback path needs an explicit yes from the maintainer, with the reason, recorded.
2. **Every feature ships with help, a course note where it applies, tests
   in the gate, and a CHANGELOG line.** No "help later".
3. **Every candidate gets an ROI line before it's built**, in the format
   of section 3, recorded with the proposal. A design study ends with its ROI line.
4. **New surface area costs something.** A new menu item, tab, panel or
   mode either replaces one or is justified in writing. Count the menu
   items each release and keep the number flat or falling.
5. **Hardware-touching features land with a rig check entry**, and a
   release lists what's still offscreen-only.
6. **Upstream intake every release** (fixtures always, engine fixes after
   review), logged in [UPSTREAM.md](../dev/UPSTREAM.md): taken, skipped, why.
7. **A gate budget.** Hold the full gate at 15 minutes on the reference
   Mac (10 performance cores). A new test
   over 10 seconds needs a reason, and each release looks at the slowest
   ten.
8. **"No" is recorded.** Section 3's NO rows go into a short "Not for
   qlcconsole" list in the docs, so the same asks get the same answer.
9. **Bus factor.** Keep CLAUDE.md, the memory notes, RELEASE.md and the
   design notes current enough that someone else could cut a release.
   Name, in SUPPORT.md, what happens to the project if it stops.

---

## 6. Decisions

Asked as one question, "Adopt this strategy (identity, 4.x home, roadmap,
guardrails)?", with the recommended answer for each part below.
**Adopted 2026-10-28, every part as recommended.**

1. **Identity**: adopt section 1's paragraph and principles; put a short
   version in README and CONTRIBUTING. → **Yes.**
2. **QLC+ 5 wording**: add the "If you use QLC+ 5 and it works for you,
   keep using it" line, and never say 4.x is abandoned. → **Yes.**
3. **Classic parity**: restore Full Screen and input-profile
   create/delete, and add the other section 2.2 fixes to 0.3.1.
   → **Yes.**
4. **LTS promise**: publish section 2.5's text in SUPPORT.md, and stop
   sending fork users to the upstream forum for 4.x questions. → **Yes.**
5. **Fixture library**: sync upstream's definitions every release by
   script, and send the fork's own upstream; document the OFL route for
   GDTF; native GDTF later. → **Yes.**
6. **Upstream intake**: review upstream master each release, taking
   engine and plugin fixes (not qmlui), logged in UPSTREAM.md. → **Yes.**
7. **0.3.1 scope**: the queue minus pad Run, why-on, laser B/C and the
   touchpad, plus the 4.x home basics and Lesson 2.1. → **Yes.**
8. **0.4 = "Run the show"**, starting with the one GO path, in section 4's
   order. → **Yes.**
9. **0.5 = "The show explains itself"**: Show Review, why-on with its
   student mode, safety checks. → **Yes.**
10. **Guardrails**: adopt section 5's list, especially "attach to an
   existing model", "help + course note + tests", and an ROI line per
   candidate. → **Yes.**
11. **The NO list**: Ableton Link, NDI/pixel mapping, an audio engine,
    OSCQuery, multi-user editing, QLC+ 5 parity work, relative/multiply
    layers for now. → **Yes.**
12. **Raspberry Pi**: "builds, not packaged" until 0.5's arm64 package.
    → **Yes.**
13. **Windows code signing**: wait until a Windows school appears, then
    buy the certificate. → **Wait.**
