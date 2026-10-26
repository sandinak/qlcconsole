---
title: For volunteers
nav_order: 11
---

# Welcome! Getting started on the course

Thank you for helping with the Show Tree course. This page is everything you
need for your first day: the app, the teaching show, where the files are,
how a lesson is laid out, and how to send your changes. **Ask Branson about
anything at all.** No question is too small.

## 1. Install qlcconsole 0.3.0 on the Mac

Branson will give you a Mac with qlcconsole on it. If it isn't installed
yet, or you want it on another Mac:

1. Open the qlcconsole 0.3.0 disk image: a file whose name starts with
   `qlcconsole` and ends in **`.dmg`** (from Branson, or from the
   [Releases page](https://github.com/sandinak/qlcconsole/releases)).
2. In the window that opens, drag **qlcconsole** onto **Applications**.
3. Open **Applications → qlcconsole**. If macOS asks whether to open it,
   click **Open**.
4. Check the version: **qlcconsole → About qlcconsole** says **0.3.0**.

Good to know:

- The app checks GitHub for updates once a day. If the footer shows
  **Update available**, you can ignore it. Ask Branson before you update,
  so your app matches the lessons.
- Keys in the course are Mac keys: ⌘ is Command, ⇧ is Shift, ⌥ is Option.

## 2. Open the teaching show

The course has its own show: a virtual show choir stage with 68 lights,
the groups, some colours and levels, and no cues yet. It is **never
connected to real lights**, so you can't break anything. Try everything.

1. Get the show file. On GitHub, open
   [`docs/course/shows/ShowChoir-Classroom.qlcc`](shows/ShowChoir-Classroom.qlcc)
   and click **Download raw file** (the ↓ button). Each lesson's start
   and finish files are in the same folder.
2. Make a folder **Documents → qlcconsole** and keep your files there.
3. In qlcconsole: **File → Open**, then pick the file.
4. Click the **Show Tree** tab at the bottom (⇧⌘5).
5. **View → Rig Overview** (⇧⌘R) opens the stage in its own window.
6. The footer says **No outputs: preview only**, in grey. That's expected:
   the show has no outputs on purpose. Red in the footer means a real
   problem.

Save your own work with **File → Save As** and a new name, so the course
file stays as it is.

The app's own help (**F1**, or the [user guide](https://github.com/sandinak/qlcconsole/blob/main/docs/features/README.md)
in the repo) explains each tool in full; the lessons link to its pages
where it helps.

Then read [The teaching show](rig.md) (what's on the stage) and do
[Lesson 1.1](level-1/lesson-01/handout.md) as a student would. It takes
about 20 minutes and is the best way to learn the app.

## 3. How the course is laid out

Everything is in the `docs/course/` folder of the repo, on the **`main`**
branch:
[browse it on GitHub](https://github.com/sandinak/qlcconsole/tree/main/docs/course).

| Where | What |
|---|---|
| [`README.md`](README.md) | The course home page |
| [`COURSE_PLAN.md`](COURSE_PLAN.md) | The plan: who it's for, every level and lesson, the design ideas, classroom logistics, decisions, and where the app got in the way (§8) |
| [`GLOSSARY.md`](GLOSSARY.md) | Every word the course uses, defined once |
| [`install.md`](install.md), [`rig.md`](rig.md) | Student pages: install, and the stage |
| `level-0/` … `level-3/` | One folder per level. Each lesson has its own folder, like `level-1/lesson-01/` |
| `shows/` | The teaching show and each lesson's start and finish files |
| `images/` | The pictures, one folder per lesson (`images/lesson-1-1/`) |
| [`_template/`](https://github.com/sandinak/qlcconsole/tree/main/docs/course/_template) | Blank lesson files to copy for a new lesson |
| `tools/` | The script that makes the teaching show (you won't need it) |
| `_config.yml` | Settings for the course website (you won't need it) |

The app's own help, which explains every button, is in
[`docs/features/show-tree.md`](https://github.com/sandinak/qlcconsole/blob/main/docs/features/show-tree.md)
(and in the app: press **F1**).

## 4. Read and preview the pages

**On GitHub (easiest).** Click any `.md` file and GitHub shows it formatted,
with pictures and tables. The links between pages work too. This is how
most people will read the course until the website is switched on.

**As the website (optional).** Later, the same pages become a website with
a sidebar and search. To see that on the Mac before it's live, you need
Ruby and a few minutes in Terminal; ask Branson to set it up with you. The
commands are in the [course README](README.md#reading-it).

## 5. What a lesson looks like

Every lesson has the same parts, so any teacher can pick it up. One folder
per lesson, with five pages and a small index:

| File | Who it's for | What's in it |
|---|---|---|
| `index.md` | everyone | one line about the lesson, links to the other pages, one picture |
| `teacher-guide.md` | the teacher | objectives, before class, words, a 50-minute timing table, the design idea, the demo, what to watch for, questions to ask, answer key |
| `handout.md` | students | today's goals, a word table, Part 1 on paper (design), Part 2 at the laptop (numbered steps), *Stuck?*, *Done early?* |
| `slides.md` | the teacher | an outline, one heading per slide |
| `exit-ticket.md` | students | 3–4 short questions on paper |
| `at-home.md` | students | **A.** the same task on a home computer, **B.** a paper version (no computer, or a Chromebook) |

Copy the [`_template/`](https://github.com/sandinak/qlcconsole/tree/main/docs/course/_template) folder to start a new lesson.

### Style rules

- **Voice: talk to students as crew.** Write the way an experienced
  designer talks to a new member of the lighting crew: respectful,
  peer-to-peer, and assuming they're capable. Use real theatre
  vocabulary (cyc, wash, board op, block cue), not stand-in words for
  it. Don't over-explain the obvious, don't cheerlead ("Great job!",
  "Just enjoy it"), and don't reassure students about things they
  weren't worried about.
- **Clear, with a normal rhythm.** Plain and precise, but in full
  sentences that connect, not strings of three-word fragments. One
  action per numbered step.
- **Define a word once.** A new word goes in the lesson's word table,
  with a short, exact definition, **and** in the
  [Glossary](GLOSSARY.md) with the lesson number. After that, just use
  it.
- **Keep style talk out of student pages.** Notes about tone, sentence
  length or "keeping it simple" belong on this page, in `_template/` and
  in teacher guides, never in a handout, at-home page, exit ticket or
  the glossary.
- **Design first, then the tool.** Every lesson starts from a decision a
  designer makes ("the verse should feel close and warm"), and every cue
  has a reason.
- **Name things exactly as the app shows them**, in bold: **+ Cue**,
  **Rig Overview**, **Warm White**. Menus with arrows: **View → Rig
  Overview**.
- **A "You should see" line after each step** that changes something, so
  the navigator can check it. Write it as a normal sentence about the
  stage or the screen: *You should see: the cyc turns blue, brightest at
  the top.*
- **Mac keys** (⌘), with one line saying "on Windows or Linux, Ctrl".
- **Every step must really work in the app.** Branson's course test does
  each step on the real app (see §6). If a step doesn't work the way you'd
  like, write down what happened. It might be an app problem worth fixing.
- **Always a paper path.** Some students only have a Chromebook or no
  computer, so every at-home page has a paper version.
- Spell **colour** as the app does. Our song is **"Everybody Scream"**.
- Use the show's names: groups **Steps, US, CS, DS, StepTops, Trusses,
  Truss-Spots, TrussW-RGB, TrussC-RGB, USTs, AT-Spots, AT-W-ALL,
  Lanterns**; targets **Soloist DS, Center, Riser Line**.

## 6. Pictures: you don't make them by hand

All the app pictures in the course are taken **by the course test**
(`ui/test/courseshow`), from the real app, with numbered callouts. That
keeps them exactly right, and when the app changes, they can all be taken
again in one go.

So when you want a picture:

1. Write where it goes and what it should show, as a note in the page:

   ```
   <!-- PICTURE: the Run tab after the second GO, with GO and the
        STANDBY row marked -->
   ```

2. Tell Branson (or mention it in your pull request). He adds it to the
   test and the picture appears in `images/lesson-N-N/`.

Your own screenshots (⇧⌘4 on the Mac) are welcome as **sketches** in a
pull request or a message, to show what you mean.

Drawings that aren't app screens (a stage diagram, a cue sheet) can be made
any way you like; put them in the lesson's `images/` folder.

## 7. How to send your changes

You don't need git or Terminal. Everything works on the GitHub website:

1. Make a free [GitHub](https://github.com) account and send Branson your
   user name. He'll add you to the project.
2. Open the page you want to change on the
   [`main` branch](https://github.com/sandinak/qlcconsole/tree/main/docs/course)
   (check the branch name at the top left says **main**).
3. Click the **pencil** (Edit this file). Make your change. **Preview**
   shows it formatted.
4. Click **Commit changes…**. Write one line saying what you changed.
   Choose **Create a new branch … and start a pull request**, and click
   **Propose changes**.
5. On the next page, check that **base** is **main**, then click **Create
   pull request**.

Branson reviews it, checks the steps on the app, and merges it.

- **A new file:** in the folder on GitHub, **Add file → Create new file**
  (type `lesson-06/handout.md` to make a folder too). For pictures, **Add
  file → Upload files**.
- **Just an idea or a problem?** Open an
  [issue](https://github.com/sandinak/qlcconsole/issues/new) and start the
  title with **Course:**. Or tell Branson.
- **Unsure?** Write it in a Google Doc and share it. Ideas are what
  matter; Branson can move them into the files.

## 8. What's written, and what's next

| Part | Status |
|---|---|
| [Course plan](COURSE_PLAN.md), [teaching show](rig.md), [install](install.md), [glossary](GLOSSARY.md) | written |
| [Level 0](level-0/index.md): 0.1 *What light does*, 0.2 *The virtual stage* (with the Level 0 check) | written |
| [Level 1](level-1/index.md): 1.1–1.5, the [Level 1 check and rubric](level-1/lesson-05/level-check.md) | written |
| [Level 3](level-3/index.md): 3.3 *Reading the plot: symbols*, 3.6 *Focus and calibration* (with pictures), 3.7 *House lights, work lights and power*, 3.10 *Coming from Eos* (optional) | written; 3.3, 3.7 and 3.10 have no pictures yet |
| Level 2 (7 lessons), the rest of Level 3 | planned: outlines in the [course plan](COURSE_PLAN.md#2-levels-and-lessons) |
| Slide decks made from the slide outlines | to make |

Good first jobs for you:

- **Do Level 0 and Level 1 as a student**, start to finish, and note
  anything confusing, too long for 50 minutes, or too hard for a ninth
  grader.
- Suggest **video clips** for Lesson 0.1 (a 2-minute show choir number with
  clear light changes; YouTube links are fine).
- Try the **Level 1 rubric** (in the Level 1 check) on a made-up group:
  is it fair and clear?
- Draft the **Level 2** lessons from the plan, starting with 2.1.

## 9. Who to ask

**Branson**, for anything: the app, the course, the show, GitHub, or "is
this a good idea?". If something in the app looks broken, tell him what
you clicked and what happened. A screenshot helps.
