---
title: Home
nav_order: 1
permalink: /
---

# Lighting a show choir with the Show Tree

A course for high-school show choir tech crews: from "what does lighting
do?" to programming and running a real show in **qlcconsole**, on a
virtual copy of a show choir stage.

> **Status (2026-10-26):** **Level 0** (2 lessons), **Level 1** (5
> lessons and the Level 1 check) and four **Level 3** lessons (3.3, 3.6,
> 3.7 and the optional 3.10) are written. Every app step of Lessons 0.2,
> 1.1–1.5, 3.3, 3.6, 3.7 and 3.10 is checked by the course test on
> qlcconsole 0.3.0 (release candidate 4), and the pictures of Levels 0
> and 1 and of Lesson 3.6 were taken on it. Level 2 and the rest of
> Level 3 are planned in the [course plan](COURSE_PLAN.md).

## Start here

| I am… | Go to |
|---|---|
| A student | Your lesson's link, from your teacher: one page with the show file, the steps, the exit ticket and the at-home task |
| A student on a Chromebook, or without the app | The [Show Tree practice version](web/index.html) in your browser (Lesson 1.1) |
| The teacher | [Course plan](COURSE_PLAN.md) · start with [Lesson 0.1](level-0/lesson-01/teacher-guide.md) |
| Anyone | [Glossary](GLOSSARY.md) · [The teaching show](rig.md) |
| Helping write the course | [For volunteers](VOLUNTEER.md) — start here |

## Student links

Each written lesson has a student page of its own at
`…/qlcconsole/class/N-N/` (Lesson 1.1:
`https://sandinak.github.io/qlcconsole/class/1-1/`): the show file, the
steps with their pictures, the exit ticket (no answers) and the at-home
task, with no menu and no way into the teacher material. Each teacher
guide starts with its link and a QR code to give students.

## The levels

| Level | What you learn | Lessons |
|---|---|---|
| [0 — First look](level-0/index.md) | What light does; the virtual stage | 2 (written) |
| [1 — Basics](level-1/index.md) | A song → cues: colour, intensity, direction, timing, GO ("Everybody Scream") | 5 (written) |
| [2 — Intermediate](level-2/index.md) | Tracking, palettes, effects, moving lights, busking | 7 |
| [3 — Advanced](level-3/index.md) | Patching, fixtures, reading and building the rig, show control, focus and touring, house lights and power, running a real show — on the real rig from December/January; an optional lesson for students coming from Eos | 10 (4 written) |

## The user guide

The course teaches a show; the app's own help explains every tool in
full. It opens with **F1** in the app, on the page for what you're
looking at, and is in the repo as the [user guide](https://github.com/sandinak/qlcconsole/blob/main/docs/features/README.md).
All of qlcconsole's documentation starts at the [docs hub](https://github.com/sandinak/qlcconsole/blob/main/docs/README.md).

## What's in this folder

| Path | What |
|---|---|
| `COURSE_PLAN.md` | The plan: audience, levels, design strand, lesson template, logistics, online plan, assessment, open questions |
| `install.md`, `GLOSSARY.md`, `rig.md` | Student pages: install, words, the stage |
| `level-N/` | A level; each lesson has its own folder (`level-1/lesson-01/` …): teacher guide, handout, slides outline, exit ticket, at home, and `student.md`, the student page |
| `class/install.md`, `_layouts/student.html` | The student install page, and the plain layout every student page uses (`/class/…` on the site; kept out of the menu and search) |
| `shows/` | The teaching show and each lesson's start / finish files (see [shows/README.md](shows/README.md)) |
| `images/` | Lesson pictures, taken from the real app by the course test |
| `web/` | The Show Tree practice version: a browser page for students who can't run the app (plain HTML/JS, no install) |
| `tools/` | How the teaching show and the practice version's show data are made; `student-pages.py` checks the student pages and makes their QR codes |
| `_config.yml` | The website's settings (GitHub Pages, just-the-docs theme); the site is built by `.github/workflows/pages.yml` from `main` |
| `LICENSE.md` | The course's licences |
| `VOLUNTEER.md` | Getting started for volunteers and co-authors |
| `_template/` | Blank lesson files to copy (not on the website) |

## Reading it

- **On GitHub:** every page reads as it is — click through from here.
- **As a website:** once this is on `main` and Pages is switched on (the
  plan, §6), the same pages are a site with a sidebar and search at
  `https://sandinak.github.io/qlcconsole/`.
- **Preview the site on your computer** (needs Ruby):

  ```sh
  cd docs/course
  gem install bundler jekyll
  bundle init && bundle add jekyll just-the-docs jekyll-remote-theme jekyll-relative-links
  bundle exec jekyll serve      # then open http://127.0.0.1:4000
  ```

## Keeping it right

The lessons are checked against the app: `ui/test/courseshow` opens the
teaching show offscreen, does every step of Lessons 0.2 and 1.1–1.5 with
the mouse (and 1.1 typed), plays them with GO in the Run tab, runs Level
3's written lessons (3.3, 3.6, 3.7, 3.10) through the app's own code, and checks
the shows can't send DMX. It runs with
the rest of the app's tests. If the app changes so a lesson step stops
working, that test fails. The pictures are taken by the same test (see
[shows/README.md](shows/README.md)).

A student page is a short version of its handout: when a handout's steps
change, change its `student.md` to match, then run
`python3 docs/course/tools/student-pages.py` (it checks the pages' links,
pictures and settings, and prints each page's length beside its
handout's).

## Licence

See [LICENSE.md](LICENSE.md):

- course text and pictures: **CC BY 4.0**;
- the teaching show files, scripts and the course test: **Apache-2.0**, as
  qlcconsole is.
