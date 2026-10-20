---
title: Course plan
nav_order: 2
---

# Show Tree course — plan for review

*Status (2026-10-20): Level 0 and Level 1 are written — 0.1, 0.2, 1.1–1.5
and the Level 1 check — each with teacher guide, handout, slides outline,
exit ticket and at-home page, every app step checked on qlcconsole 0.2.0
by the course test. Levels 2 and 3 are outlines. Volunteers start at
[For volunteers](VOLUNTEER.md); new lessons start from `_template/`.*

The Show Tree is the main tool for this year's show. This course takes a
high-school show choir tech crew from "what does a lighting designer do?"
to programming and running a real show in qlcconsole, using a stage that
looks like ours: risers, trusses, a cyc, pixel boxes and lanterns.

The course teaches **design first, then the tool**. Each lesson starts from
something a designer decides ("the verse should feel close and warm") and
ends with the Show Tree action that makes it happen.

---

## 1. Audience and assumptions

| | Assumption (change any of these) |
|---|---|
| **Students** | Grades 9–12, on the show choir tech crew. Most have never run lights. Some have run a board once. |
| **Class period** | **50 minutes** (confirmed). Each lesson fits one period: 10 min design idea, 10 min teacher demo, 20 min hands-on, 10 min check and exit ticket. A 45-minute period drops the extension. |
| **Pace** | About one lesson a week (confirmed), between rehearsals. Level 0–1 before the fall show's tech week; Level 2 through the winter; Level 3 for the students who will run the spring show. |
| **Laptops in class** | **Few at first** — one per pair or three. The projector shows the teacher's screen. |
| **Computers at home** | Mixed: Mac, Windows, and **Chromebooks**. A Chromebook runs qlcconsole only if its Linux environment (Crostini) is turned on **and** it has an Intel/AMD chip — the Linux build is x86_64 only. Many school Chromebooks have Linux turned off by the district, and many are ARM. Those students do the paper version of the homework (each lesson has one). |
| **The stage** | The **teaching show** (below): a virtual copy of a show choir stage. It is never connected to real lights. The **Rig Overview** is the stage. |
| **Real lights** | First hands on the real rig in **December or January**, with Branson: Level 3 starts there. Until then, only the virtual stage. |

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
| 0.1 | **What light does** *(written)* | The four qualities of light (intensity, colour, direction, movement). The jobs of light: let the audience see, tell them where to look, set the mood, match the energy of the song. | No computer. Watch a 2-minute clip of a show choir number twice: once for the singers, once only for the light. Name each change. |
| 0.2 | **The virtual stage** *(written)* | The stage words: upstage / downstage, stage left / stage right (the singers' left and right), risers, trusses, cyc, audience towers (front of house). | Install and open qlcconsole. Open `lesson-1-1-finish.qlcc`. **View → Rig Overview** (⇧⌘R): its View (Top, Front, 45°) and Room (Work light…) to find each part of the stage. Run tab: press GO three times. Nothing you do can break real lights. **Level 0 check:** label the stage plan. |

### Level 1 — Basics (5 lessons)

*Outcome: from a song, students write a short cue sheet and program it as
a cue list of looks (colour, intensity, direction, timing), then run it
with GO.*

| # | Lesson | Design idea | In the tool |
|---|---|---|---|
| **1.1** | **From a song to cues: your first three looks** *(written; song: "Everybody Scream")* | A song has sections; a cue is a moment where the light changes. A look is a group of lights in a colour at a level. | Production → cue list → cue. A group dropped on a cue is a **layer**; a colour and a level dropped on the layer. Three cues: Pose, Verse, Chorus. GO in the Run tab. |
| 1.2 | **Colour and intensity tell the mood** *(written)* | Warm vs cool; saturated vs pale; bright isn't always better. Contrast between sections. The director's note: the verse is too cosy. | The verse's StepTops in place: ⋯ → **Set to** Lavender, ⋯ → **Level…** 40, **Make reusable**; Undo / Redo. A new cue 1.0.3 *Scream*: TrussW-RGB and Lanterns with **cue looks** Red, Full. Extension: **Colour…**. |
| 1.3 | **Direction: aim the light** *(written)* | Front light shows faces; back and side light shape bodies; uplight is drama. Every number needs faces lit. | AT-W-ALL Warm White, Full in the verse — it lights the front row as it is — then **Aim Center** (Library → Targets). A **Block** cue 1.0.4 *Solo*: AT-Spots White, Full stay dark (the shutter), **Open** (Palettes → Shutter) lights the floor (no position), then **Aim Soloist DS**. |
| 1.4 | **Timing: fades and holds** *(written)* | A snap is a hit; a slow fade is a breath. Cue timing follows the music. | Fade in / Fade out / Hold typed on the cue page: Pose 3 s, Verse 2 s, Chorus a snap, Scream hold 2 s and fade out 3 s (the Solo, fade in 3 s, follows by itself). Played in the Run tab. |
| 1.5 | **Calling and running the show** *(written)* | Standby / GO. The stage manager calls; the board op runs. | **Operate** (LIVE); Run tab: GO, Back, LOAD a cue (STANDBY), double-click to jump, Stop; Design again. Students call each other's cues. **Level check** (its own page, with the rubric): light a 30-second clip (3–5 cues) and run it. |

### Level 2 — Intermediate (7 lessons)

*Outcome: students program a whole song the way a pro does: with
tracking, reusable palettes, effects on the pixel fixtures, moving lights
on lines, and busking looks for the unexpected.*

| # | Lesson | Design idea | In the tool |
|---|---|---|---|
| 2.1 | **Tracking and Block** | What stays on unless you change it. A "hard start" for each song. | Grey ⤳ rows ("tracked from"), **Release**, cue modes Track / Block / Cue-only, the **base cue**, *Next cue receives*. |
| 2.2 | **Palettes: change once, change everywhere** | A designer's colours for a song. The director says "less pink" an hour before the show. | Edit a palette in the Look Editor and watch every cue change. Make your own palette; a folder per song. *Used by*. |
| 2.3 | **Parts of a group** | Highlight one row of singers. Symmetry. | Child layers (`US in Steps`), group sets (a set of Trusses SR + Trusses SL), Split / Merge. |
| 2.4 | **Effects: sparkle, chase, matrix** | Energy and rhythm. Eye candy vs. light on people. | Effects on the Steps and the truss pixel boxes (Sinelon, chase, sparkle); an effect on the cue across the whole stage; its own colour; speed masters. |
| 2.5 | **Moving lights and lines** | Beams as scenery; fans and crosses on the button of a song. | The Riser Line target, Spread / Grouping / Cross, the plan view, with Truss-Spots and USTs. Colour rings on the Truss-Spots. |
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
| **Direction (position)** | Front light shows faces, back light separates, side light sculpts, uplight is drama. | Which **group** you choose (AT-W-ALL from the front, StepTops from below, TrussC-RGB on the cyc), and **Position** / Aim for moving lights. |
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
| 1.0.0 | Before the first note, singers in pose | Mystery, waiting | TrussC-RGB: Blue, Full | snap |
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

Each lesson's folder holds: `index.md`, `teacher-guide.md`, `handout.md`,
`slides.md` (a slide outline that can become a deck), `exit-ticket.md`,
`at-home.md`, and its pictures in `images/lesson-L-N/` (Lesson 1.2:
`images/lesson-1-2/`); its show files are `shows/lesson-L-N-start.qlcc`
and `…-finish.qlcc`, each lesson starting from the one before's finish.
Blank copies of the six files are in `_template/`.

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
- **Save often, on your own computer.** Students save under their own
  name (`Lesson01-Jordan.qlcc`), never over the course file, in a
  **qlcconsole** folder in their Documents (the app has no folder of its
  own for shows: the first Save offers wherever the computer starts, and
  after that the last folder used). Files come to class on a USB stick, by
  the school's cloud drive, or emailed to themselves — see
  [Install → Saving your work](install.md#saving-your-work).
- **Keyboard shortcuts** are shown as Mac keys (⌘). On Windows and Linux
  use **Ctrl** for ⌘ and **Alt** for ⌥.

---

## 6. Online: the course on GitHub

### Where it lives

The whole course is Markdown in this repo, in `docs/course/`. That keeps
the course next to the app it teaches: when the app changes, the course
and its test change in the same commit, and the course test
(`courseshow_test`) fails if a lesson step stops working.

### The website: GitHub Pages with the just-the-docs theme (approved)

- **Why:** students get a clean website (search, a sidebar of levels and
  lessons, works on a phone) with no build step for the teacher: edit a
  `.md` file, push, the site updates. The same `.md` files also read fine
  on GitHub itself.
- **How:** `docs/course/_config.yml` is a Jekyll config with the
  just-the-docs remote theme. `.github/workflows/pages.yml` builds **only
  `docs/course`** and deploys it, on a push to `main` that touches
  `docs/course/**` (and by hand, *Run workflow*). GitHub runs a workflow
  only from the default branch, so the site goes live when this branch is
  merged to `main` and Pages is switched on:
  1. **Settings → Pages → Build and deployment → Source: GitHub
     Actions.**
  2. **Settings → Environments → github-pages** (made by step 1):
     *Deployment branches and tags* must allow `main` (the default rule,
     "protected branches" or "selected: main", does).
  3. **Settings → Actions → General → Workflow permissions:** the default
     (read) is enough; the workflow asks for `pages: write` and
     `id-token: write` itself.
  4. Merge to `main`, then Actions → **Course site** → *Run workflow*
     once (or push a change under `docs/course`). The site is at
     `https://sandinak.github.io/qlcconsole/`.

### Downloads for students

- [Install qlcconsole](install.md) points at the **show-tree pre-release**
  on GitHub Releases (macOS dmg, Windows installer, Linux AppImage), with
  install notes per computer. That pre-release isn't published yet.
- Each lesson links its show files (`docs/course/shows/`). On GitHub, a
  student opens the file and clicks **Download raw file**; on the site, the
  link downloads it.

### Licence (approved, 2026-10-20)

- qlcconsole is **Apache-2.0** (`COPYING`), as QLC+ is.
- **Course text and pictures: CC BY 4.0.** Other teachers can use and adapt
  it if they credit it; unlike CC BY-SA, it doesn't force their whole
  handout packet under the same licence, and it sits comfortably beside an
  Apache-2.0 app (both only ask for credit).
- **The teaching show files and the course's scripts and test: Apache-2.0**,
  like the app: they are software and data built from and for it.
- Written into `docs/course/LICENSE.md`.

---

## 7. Assessment

- **Exit tickets** every lesson (quick, low stakes).
- **Level checks:**
  - Level 0: label the stage diagram (US/DS/SL/SR, risers, trusses, audience towers, cyc).
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
  Level 1's check: [level-1/lesson-05/level-check.md](level-1/lesson-05/level-check.md).

---

## 8. Where the course and the app disagreed

Fixed in the app (`fix/course-app-issues`, merged into `feat/show-tree`
and into this branch), and the course updated to match:

1. ~~**A new production isn't the one GO plays.**~~ **Done.** A new
   production becomes current by itself when nothing else has GO. The
   handout's "Make current" step and its picture are gone; the handout now
   warns that clicking the **▶ Current** button would *unset* it.
2. ~~**The drop area's text is hard to read.**~~ **Done.** Readable in every
   theme (`images/lesson-1-1/new-cue.png` retaken).
3. ~~**Moving lights have no home.**~~ **Done.** The "Branson 36LED
   Moving Head" definition (AT-W-ALL) has Pan as a coarse channel with
   default 128 and Tilt default 32 (Branson, 2026-10-19); the classroom
   show carries the fixed copy (`derive-classroom-rig.py` patches it, and
   the course test checks a colour-only AT-W-ALL sends pan 128 / tilt 32).
   Tilt 32 is 15° below horizontal: 128 is straight up and the head tilts
   280°, which last season's aim scenes bear out (they reached the singers
   with tilt 45–54, near horizontal, within 2° on average).
   **The facing, worked out from last season's show** (tpihab-1.3 /
   showfiles/out.qxw, scenes *Risers-US/CS/DS-Center*, *Risers*,
   *Apron-Solo*, against the towers where DanceHysteria has them): at pan
   centre, tilted forward, these heads point straight **across** the
   stage, toward stage left — rig pan-zero **270°**, fitted to within
   about 10° rms over 29 aims from both towers — not downstage (0°), which
   is why a bare colour and level used to light the house floor. Both
   towers were rigged the same way round, so the SL heads only reached the
   stage by tilting back over the top (tilt ~208), and no one definition
   default could light the stage from both sides. The classroom rig
   therefore has the washes turned on their shelves to face the middle of
   the front riser row: **SR 252°, SL 108°** (the mirror). With that,
   **"AT-W-ALL, Warm White, Full" on its own lights the front row and the
   soloist** (checked offscreen; facing straight across, 270°/90°, the
   pool fell on the floor in front of the risers), and *Aim Center* still
   lands on Center. Lesson 1.3 can start bare and then aim. For the real
   show (DanceHysteria) the change is Branson's: the per-fixture facing
   and how the SL heads are set on the tower.
4. **The footer is red in class.** Unchanged and intended: the teaching
   show has no outputs, so it says **Not ready** ("13 universe(s) with
   fixtures but no output"). The handout, install and rig pages say to
   expect it.
5. ~~**Blue on the cyc looks pale.**~~ **Done.** Coloured light on the grey
   cyc is clearly coloured — brightest at the top, as the cyc boxes aim
   upward. The handout now says "a clear blue, brightest at the top"
   (`rig-*.png` retaken).
6. ~~**A crash on close with the Show Tree up.**~~ **Done.** The course
   test no longer hides the window before deleting the App.

---

### Found writing Level 0 and Lessons 1.2–1.5 (on 0.2.0)

Not changed in the app (this branch doesn't change app code); each is
worked around in the course and checked by the course test.

7. **The AT-Spots stay dark at Full: their shutter is closed.** The
   *Focus Spot Three Z*'s Strobe/Shutter channel rests at 0 (*Off*), and a
   Dimmer look doesn't open it, so "AT-Spots White, Full" lights nothing.
   **Worked around:** the classroom show now has a Shutter palette
   **Open** (Palettes → Shutter), and Lesson 1.3 teaches it as a step —
   a real thing about spots. **For the app (Branson's call):** a Dimmer
   look could open a closed shutter, as many consoles do, or the
   definition's default could be an *Open* value (8). DanceHysteria has
   the same spots.
8. **Make reusable files a picked level in a folder of its own.** After
   **Level… 40** and **Make reusable**, *40%* lands in **Palettes →
   Dimmer → Show Tree**, though the show's other levels are in no folder
   (the help says it is filed "the way your palettes of that kind are").
   **Worked around:** the handout says where it goes.
9. **The Run tab's production box can keep the last show's name.** Seen in
   the course test: after a show whose production was *Qualities*, opening
   `lesson-1-4-finish.qlcc` left the box at *Current production
   (Qualities)* while the header said *Spring Show*. The box refreshes when
   the current production changes, and both shows' productions had the
   same id. Every course file uses *Spring Show*, so students won't meet
   it; the course test takes the 0.1 pictures (the *Qualities* production)
   last.
10. **The command line's last message carries over to the next show** (the
    bottom right still said *✓ TrussW-RGB colour White dimmer Full* after
    File → Open). Cosmetic.
11. **Not checked: the Mac's own colour panel.** The course test drives
    Qt's colour window for **⋯ → Colour…** (Lesson 1.2's extension); on a
    Mac, Qt may show the system colour panel instead. A volunteer should
    try it on the Mac (VOLUNTEER.md lists it).

Also: the classroom show was rebuilt from DanceHysteria-0.6 as it is now
(the SR tower's shelves were changed after the last build: now 1.0 /
1.524 / 2.438 m, with its spot and washes on different shelves). The bare AT-W-ALL still lights the front row
(Lesson 1.3's first picture), and the aims still land.


## 9. Decisions (Branson, 2026-10-18) and what's still open

| # | Question | Answer |
|---|---|---|
| 1 | Period and pace | 50-minute periods, about one lesson a week. |
| 2 | The song | **"Everybody Scream"** — Lesson 1.1's cue list is `01-Everybody Scream`. |
| 3 | Group names | The real show's style: the **Trusses** across the back, with letters for what's on them: **Truss-Spots**, **TrussW-RGB / -W**, **TrussC-RGB / -W**, **USTs** (the upstage beams), and likewise **Steps**, **StepTops**, **US / CS / DS**, **AT-Spots**, **AT-W-ALL**, **Lanterns**. |
| 4 | Riser Line | Fine. |
| 5 | 68 fixtures | Fine. |
| 7 | What students install | **qlcconsole 0.2.0** from GitHub Releases (the DMG is being made; the lessons are checked on 0.2.0). |
| 8 | GitHub Pages | Yes — the workflow is ready (§6); it needs the merge to `main` and the settings in §6. |
| 9 | Licence | CC BY 4.0 (text, pictures) + Apache-2.0 (show files, scripts): approved (§6). |
| 10 | Real lights | First hands on the real rig in **December or January**. |
| 11 | Where students save | On their own computers (see §5 and the install page). |
| 12 | App issues | Fixed (§8); the moving-head definition fixed too, and its facing worked out — set on the classroom rig, DanceHysteria's is Branson's call. |

**Still open:**

1. **Chromebooks** (6): does the district allow the Linux environment on
   student Chromebooks, and are they Intel or ARM? Probably not Linux — so
   every lesson keeps a full paper *At home* version.
2. **The 0.2.0 release's name and files**: the install page says "the
   newest pre-release whose name starts with **Show Tree**" and "0.2.0 or
   later" — check both when it's published.
3. **The AT-Spots' shutter** (§8, 7): keep teaching **Open**, or change
   the app or the definition?
4. **Video clips** for Lesson 0.1 and the Level 1 check: pick them.
