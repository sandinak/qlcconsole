---
title: Course plan
nav_order: 2
---

# Show Tree course — plan for review

*Status: first pass for Branson to review (2026-10-18). Written: this plan,
the teaching show, Lesson 1.1 complete. Everything else below is an outline.*

The Show Tree is the main tool for this year's show. This course takes a
high-school show choir tech crew from "what does a lighting designer do?"
to programming and running a real show in qlcconsole, using a stage that
looks like ours: risers, towers, a cyc, pixel boxes and lanterns.

The course teaches **design first, then the tool**. Each lesson starts from
something a designer decides ("the verse should feel close and warm") and
ends with the Show Tree action that makes it happen.

---

## 1. Audience and assumptions

| | Assumption (change any of these) |
|---|---|
| **Students** | Grades 9–12, on the show choir tech crew. Most have never run lights. Some have run a board once. |
| **Class period** | **50 minutes.** Each lesson fits one period: 10 min design idea, 10 min teacher demo, 20 min hands-on, 10 min check and exit ticket. A 45-minute period drops the extension. |
| **Pace** | About one lesson a week, between rehearsals. Level 0–1 before the fall show's tech week; Level 2 through the winter; Level 3 for the students who will run the spring show. |
| **Laptops in class** | **Few at first** — one per pair or three. The projector shows the teacher's screen. |
| **Computers at home** | Mixed: Mac, Windows, and **Chromebooks**. A Chromebook runs qlcconsole only if its Linux environment (Crostini) is turned on **and** it has an Intel/AMD chip — the Linux build is x86_64 only. Many school Chromebooks have Linux turned off by the district, and many are ARM. Those students do the paper version of the homework (each lesson has one). |
| **The stage** | The **teaching show** (below): a virtual copy of a show choir stage. It is never connected to real lights. The **Rig Overview** is the stage. |
| **Real lights** | Only in Level 3, on Branson's rig, with an adult. |

### Roles for shared laptops

With one laptop for two or three students, they rotate roles **every
cue** (not every lesson), so everyone drives:

| Role | Does |
|---|---|
| **Driver** | Hands on the mouse and keyboard. Only the driver touches the laptop. |
| **Navigator** | Reads the handout steps aloud and checks each "You should see" line. |
| **Designer** | Holds the cue sheet. Decides colour and level, says *why*, and watches the Rig Overview to say whether it worked. |

A pair skips the designer role; the navigator does both.

---

## 2. Levels and lessons

Four levels. Each level ends with a short practical check. Lesson numbers
are *level.lesson*.

### Level 0 — First look (2 lessons)

*Outcome: students can say what light does for a show choir number, and
can open the app, find the stage, and play a finished cue list.*

| # | Lesson | Design idea | In the tool |
|---|---|---|---|
| 0.1 | **What light does** | The four qualities of light (intensity, colour, direction, movement). The jobs of light: let the audience see, tell them where to look, set the mood, match the energy of the song. | No computer. Watch a 2-minute clip of a show choir number twice: once for the singers, once only for the light. Name each change. |
| 0.2 | **The virtual stage** | The stage words: upstage / downstage, stage left / stage right (the singers' left and right), risers, towers, cyc, front of house. | Install and open qlcconsole. Open `lesson-01-finish.qlcc`. **View → Rig Overview** (⇧⌘R): find each part of the stage. Run tab: press GO three times. Nothing you do can break real lights. |

### Level 1 — Basics (5 lessons)

*Outcome: from a song, students write a short cue sheet and program it as
a cue list of looks (colour, intensity, direction, timing), then run it
with GO.*

| # | Lesson | Design idea | In the tool |
|---|---|---|---|
| **1.1** | **From a song to cues: your first three looks** *(written)* | A song has sections; a cue is a moment where the light changes. A look is a group of lights in a colour at a level. | Production → cue list → cue. A group dropped on a cue is a **layer**; a colour and a level dropped on the layer. Three cues: Pose, Verse, Chorus. Make it current, GO in the Run tab. |
| 1.2 | **Colour and intensity tell the mood** | Warm vs cool; saturated vs pale; bright isn't always better. Contrast between sections. | Change a look in place (⋯ → Set to), Level… and Colour… for values not in the palette list, a **cue look** for the whole cue. Undo. |
| 1.3 | **Direction: aim the light** | Front light shows faces; back and side light shape bodies; uplight is drama. Every number needs faces lit. | Stage targets (Soloist DS, Center). Drop **Aim Center** on the Front Wash; **Aim Soloist DS** on a Front Spot. Why moving lights need a Position. |
| 1.4 | **Timing: fades and holds** | A snap is a hit; a slow fade is a breath. Cue timing follows the music. | Fade in / Fade out / Hold on the cue page. Watch the fades in the Rig Overview. |
| 1.5 | **Calling and running the show** | Standby / GO. The stage manager calls; the board op runs. | Run tab: GO, Back, Stop, LOAD a cue. Students "call" each other's cues. **Level check:** light a 30-second clip (3–5 cues) and run it. |

### Level 2 — Intermediate (7 lessons)

*Outcome: students program a whole song the way a pro does: with
tracking, reusable palettes, effects on the pixel fixtures, moving lights
on lines, and busking looks for the unexpected.*

| # | Lesson | Design idea | In the tool |
|---|---|---|---|
| 2.1 | **Tracking and Block** | What stays on unless you change it. A "hard start" for each song. | Grey ⤳ rows ("tracked from"), **Release**, cue modes Track / Block / Cue-only, the **base cue**, *Next cue receives*. |
| 2.2 | **Palettes: change once, change everywhere** | A designer's colours for a song. The director says "less pink" an hour before the show. | Edit a palette in the Look Editor and watch every cue change. Make your own palette; a folder per song. *Used by*. |
| 2.3 | **Parts of a group** | Highlight one row of singers. Symmetry. | Child layers (`US Risers in Risers`), group sets (Towers = Towers SR + Towers SL), Split / Merge. |
| 2.4 | **Effects: sparkle, chase, matrix** | Energy and rhythm. Eye candy vs. light on people. | Effects on the riser fronts and pixel boxes (Sinelon, chase, sparkle); an effect on the cue across the whole stage; its own colour; speed masters. |
| 2.5 | **Moving lights and lines** | Beams as scenery; fans and crosses on the button of a song. | The Riser Line target, Spread / Grouping / Cross, the plan view. Colour rings on the Movers. |
| 2.6 | **Busking with Looks** | When the band vamps, or the soloist goes long. | The Looks section, **Make VC page**, firing looks from Virtual Console buttons over a running list. |
| 2.7 | **Hits inside a cue** | Hits, stomps and buttons in a chorus. | Sub-cues 4.1, 4.2, Sub GO and auto-follow. **Level check:** design and program one full song from a cue sheet. |

### Level 3 — Advanced (7 lessons)

*Outcome: students can set up the real rig, fix it when it breaks, and run
the show. Patching and everything else that touches real hardware lives
here.*

| # | Lesson | In the tool |
|---|---|---|
| 3.1 | **DMX, universes and patching** | Addresses and channels; universes; Art-Net and sACN; the Patch dialog; Connections. *(First lesson on the real network, with Branson.)* |
| 3.2 | **Fixtures and definitions** | The Fixture Manager; definitions and modes; the Fixture Editor; groups by head type (a pixel box's RGB and white heads; colour rings). Shows that carry their own definitions. |
| 3.3 | **Building the rig** | Lighting Studio: structures, mounting fixtures, layers, stage targets and lines; why the rig must match the room for aims to work. |
| 3.4 | **Show control** | QLab (or a sound op) firing cues by number; OSC / MIDI Show Control; timecode. |
| 3.5 | **Focus and calibration** | Test orientation, Locate, the Step Test Pattern; aiming the real movers at the real targets. |
| 3.6 | **Running a real show** | Operate mode, Show-mode Lock, the panic button, the preshow checklist, power. |
| 3.7 | **Troubleshooting** | The Not ready list, a light that won't come on, a cue that looks wrong, Undo and backups. **Capstone** (below). |

---

## 3. The design-concepts strand

Every lesson opens with a design idea before the tool. These ideas come
back again and again; the table says where each one maps to the Show Tree.

| Concept | What students learn | In the Show Tree |
|---|---|---|
| **Intensity** | How bright. The eye goes to the brightest thing. | The **Dimmer** row; palettes Full, 75%, Half, 25%, 10%, Out. |
| **Colour** | Warm/cool, saturated/pale, what it says (blue = night, cool, calm; red/magenta = energy). | The **Colour** row; the colour palettes. |
| **Direction (position)** | Front light shows faces, back light separates, side light sculpts, uplight is drama. | Which **group** you choose (Front Wash vs Riser Tops vs Cyc Washes), and **Position** / Aim for moving lights. |
| **Movement** | Light that changes while you watch: fades, chases, moving beams. | Fade times, **effects**, moving lights on a **line**. |
| **Focus the audience** | Light where you want them to look; dark where you don't. | Which layers are in a cue; a soloist special. |
| **Mood and story** | What the song is about; what changes in it. | Colour choices per song; palettes per song. |
| **Energy** | Match the music: verse low, chorus high, button big. | Levels and how much of the rig is on; effects; snaps vs fades. |
| **Reading a song** | Intro, verse, pre-chorus, chorus, bridge, dance break, button (the final hit). | One **cue list** per song; a **cue** per section or moment. |

### The cue sheet

Students design on paper before they touch the computer. The same sheet is
used from Lesson 1.1 to the capstone (it is in Lesson 1.1's handout).

| Cue | Where in the song (words or count) | What the audience should feel | Lights (group: colour, level) | Time |
|---|---|---|---|---|
| 1.0.0 | Before the first note, singers in pose | Mystery, waiting | Cyc Washes: Blue, Full | snap |
| … | | | | |

---

## 4. Lesson template

Every lesson has the same parts, so a substitute can teach it:

1. **Objectives** — 2–3 "I can…" statements.
2. **Vocabulary** — the new words, each defined once (and in the
   [Glossary](GLOSSARY.md)).
3. **Design idea (10 min)** — no computers. Picture, clip or discussion.
4. **Teacher demo (10 min)** — on the projector, the exact steps the
   handout will ask for, with the class calling out what to do next.
5. **Hands-on (20 min)** — handout steps in pairs/triads, with roles. Each
   step has a **You should see** line.
6. **Check for understanding** — 2–3 questions while groups work.
7. **Exit ticket (5 min)** — on paper or a form.
8. **Extension** — for groups that finish early.
9. **At home** — the same task on a home computer, or a paper version for
   students who can't install it.

Each lesson's folder holds: `teacher-guide.md`, `handout.md`,
`slides.md` (a slide outline that can become a deck), `exit-ticket.md`,
`at-home.md`, and its pictures in `images/lesson-NN/`.

---

## 5. Classroom logistics

- **Projector demo first, always.** The teacher builds the lesson on the
  projector before anyone touches a laptop. Students without a laptop
  follow on the paper cue sheet.
- **Stations.** With fewer laptops than groups, run two stations and swap
  at half time:
  - **Laptop station** — the hands-on steps.
  - **Design station** (no computer) — the paper cue sheet for the next
    song, a colour-swatch card sort (which colour for which mood), or
    "board op" practice: one student calls "Standby cue 2… GO", another
    flips a printed cue card.
- **Peer review.** Groups show their Run tab to the next group: three
  GOs, the other group says what each cue made them feel.
- **Nothing to break.** The teaching show sends nothing to real lights, so
  "try it and see" is safe. Undo (⌘Z / Ctrl+Z) takes back any step.
- **Save often.** Students save under their own name
  (`Lesson01-Jordan.qlcc`), never over the course file.
- **Keyboard shortcuts** are shown as Mac keys (⌘). On Windows and Linux
  use **Ctrl** for ⌘ and **Alt** for ⌥.

---

## 6. Online: the course on GitHub

### Where it lives

The whole course is Markdown in this repo, in `docs/course/`. That keeps
the course next to the app it teaches: when the app changes, the course
and its test change in the same commit, and the course test
(`courseshow_test`) fails if a lesson step stops working.

### Recommended: GitHub Pages with the just-the-docs theme

- **Why:** students get a clean website (search, a sidebar of levels and
  lessons, works on a phone) with no build step for the teacher: edit a
  `.md` file, push, the site updates. The same `.md` files also read fine
  on GitHub itself, so nothing breaks if Pages is never turned on.
- **How:** `docs/course/_config.yml` is a Jekyll config using the
  just-the-docs remote theme (GitHub's Pages build supports remote themes).
  Because the course is in a sub-folder of `docs/`, Pages needs a small
  GitHub Actions workflow that builds `docs/course` only. That workflow is
  in `docs/course/tools/pages-workflow.yml.example` — **not** in
  `.github/workflows/`, so nothing runs until Branson moves it there and
  turns on Pages (Settings → Pages → Source: GitHub Actions).
- **Simpler alternative:** don't turn on Pages; send students to the
  `docs/course/README.md` page on GitHub. Everything works, just less
  pretty.
- **Nothing is published yet.** The branch is pushed; Pages is off.

### Downloads for students

- [Install qlcconsole](install.md) points at the **GitHub Releases** page
  (macOS dmg, Windows installer, Linux AppImage), with install notes per
  computer.
- Each lesson links its show files (`docs/course/shows/`). On GitHub, a
  student opens the file and clicks **Download raw file**.

### Licence

- qlcconsole is Apache-2.0 (as QLC+ is).
- **Course content:** suggested **CC BY-SA 4.0** — other teachers may use
  and adapt it if they credit it and share alike. Branson decides; nothing
  is marked yet beyond a note in the README.

---

## 7. Assessment

- **Exit tickets** every lesson (quick, low stakes).
- **Level checks:**
  - Level 0: label the stage diagram (US/DS/SL/SR, risers, towers, cyc).
  - Level 1: *Light this 30-second clip* — a cue sheet with 3–5 cues, the
    cue list built, and run live with GO while the clip plays.
  - Level 2: *Design and program one song* — cue sheet, tracking used on
    purpose, at least one palette of their own, one effect, one busking
    look.
  - Level 3: run the real rig through a preshow check and a song.
- **Final project (capstone):** a team designs, programs and runs one song
  of the show. Graded on: the cue sheet (does every cue have a reason?),
  the program (does it play what the sheet says?), running it (clean
  GOs on the calls), and a 2-minute talk: "here's what we wanted the
  audience to feel and how the light did it."
- A simple rubric (4 levels: beginning / developing / proficient /
  advanced) for design, programming, operation and teamwork goes with
  Level 1's check — to be written with the Level 1 lessons.

---

## 8. Where the course and the app disagree (found writing Lesson 1.1)

Nothing in the app was changed in this pass. These are for Branson:

1. **A new production isn't the one GO plays.** After **+ Production**,
   the Run tab shows nothing to GO until the production is made current
   (its page → **Make current**). Lesson 1.1 has that as step 9. With a
   single production in the show, making it current automatically would
   save every beginner the confusion.
2. **The drop area's text is hard to read.** On the cue page, the dashed
   area's "Drag groups or fixtures here…" is very faint in the Blue theme
   (see `images/lesson-01/new-cue.png`). The handout tells students where
   it is instead of relying on the text.
3. **Moving lights have no home.** The Front Wash and Front Spots are
   moving heads; lit with only a colour and a level they point at the floor
   by their tower. Lesson 1.1 uses only non-moving groups; Lesson 1.3
   introduces Aim. A per-fixture home position (or a show base cue the
   course ships) would make a first "Front Wash, Warm White, Full" work.
4. **The footer is red in class.** The teaching show shows **Not ready** in red:
   "13 universe(s) with fixtures but no output" — true and intended (it
   must never send DMX). The handout tells students to expect it. A
   "virtual rig / classroom" setting that silences it would help.
5. **Blue on the cyc looks pale** in the Rig Overview: pure blue (#0000ff)
   on the cyc renders as a faint lavender tint, not a strong blue. Students
   may think the cue didn't work; the handout says "a soft blue glow".
6. **A crash on close, found by the course test.** `App::~App` deletes the
   Doc before the Show Tree tab. If the main window is still showing when
   the App is destroyed, the tab's hide event stops its preview on the
   Doc's (already freed) ShowTreeController → SIGSEGV. The test now hides
   the window first; the app fix is one line in `App::~App`, beside the
   ProgrammingManager one: delete `ShowTreeView::instance()` before
   `m_doc`. (Not changed in this pass.)

---

## 9. Open questions for Branson

1. **Period length and pace** — 50 minutes and about one lesson a week:
   right? How many periods before the fall show's tech week?
2. **Which song** for the running example? Lesson 1.1 uses a made-up
   "Opener" (Pose, Verse, Chorus). A real song from this year's set would
   motivate more — but needs a clip we can play in class.
3. **Groups and names** in the teaching show: *Back Beams* (the two-arm
   beams standing on the top riser — your brief said "Back Truss"),
   *Pixel Boxes* (the tower XL-450s that face out) vs *Cyc Washes* (the ones
   that light the cyc), the folder *Extras*, numbering towers *1 = outside*
   on each side. Keep, or rename to what the crew calls them?
4. **The line target** is called *Riser Line*, not *Risers*: a target and
   a group with the same name clash on the command line. OK?
5. **Fixture count.** The teaching show has 68 fixtures (the real show
   206): one LED strip per riser front instead of eight, one spot per tower,
   no pixel walls between the towers, no pars. Enough of "our stage"?
6. **Chromebooks** — does the district allow the Linux environment on
   student Chromebooks? Are they Intel or ARM? (Decides whether homework
   is on the computer or on paper for those students.)
7. **The first release.** The repo is public, but it has **no GitHub
   Release yet**, so the [install page](install.md)'s download link has
   nothing to download until v0.1.0 (or a class build) is published.
   Which build should students use?
8. **Turn on GitHub Pages?** (Recommended, with the workflow provided.)
9. **Course licence** — CC BY-SA 4.0 OK?
10. **Real lights** — when do students first touch the real rig: Level 3
    only, or a "field trip" at the end of Level 1?
11. **Show file names for students** — save as `Lesson01-<name>.qlcc` in
    a class folder? Is there a shared drive?
12. The app items in §8 — fix any before the course starts?
