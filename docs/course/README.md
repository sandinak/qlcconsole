---
title: Home
nav_order: 1
permalink: /
---

# Lighting a show choir with the Show Tree

A course for high-school show choir tech crews: from "what does lighting
do?" to programming and running a real show in **qlcconsole**, on a
virtual copy of a show choir stage.

> **Status (2026-10-20):** **Level 0** (2 lessons) and **Level 1** (5
> lessons and the Level 1 check) are written, each step checked on
> qlcconsole 0.2.0. Levels 2 and 3 are planned in the
> [course plan](COURSE_PLAN.md).

## Start here

| I am… | Go to |
|---|---|
| A student | [Install qlcconsole](install.md), then your lesson's **handout** |
| A student on a Chromebook, or without the app | The [Show Tree practice version](web/index.html) in your browser (Lesson 1.1) |
| The teacher | [Course plan](COURSE_PLAN.md) · start with [Lesson 0.1](level-0/lesson-01/teacher-guide.md) |
| Anyone | [Glossary](GLOSSARY.md) · [The teaching show](rig.md) |
| Helping write the course | [For volunteers](VOLUNTEER.md) — start here |

## The levels

| Level | What you learn | Lessons |
|---|---|---|
| [0 — First look](level-0/index.md) | What light does; the virtual stage | 2 (written) |
| [1 — Basics](level-1/index.md) | A song → cues: colour, intensity, direction, timing, GO ("Everybody Scream") | 5 (written) |
| [2 — Intermediate](level-2/index.md) | Tracking, palettes, effects, moving lights, busking | 7 |
| [3 — Advanced](level-3/index.md) | Patching, fixtures, reading and building the rig, show control, house lights and power, running a real show — on the real rig from December/January; an optional lesson for students coming from Eos | 10 (3 written) |

## What's in this folder

| Path | What |
|---|---|
| `COURSE_PLAN.md` | The plan: audience, levels, design strand, lesson template, logistics, online plan, assessment, open questions |
| `install.md`, `GLOSSARY.md`, `rig.md` | Student pages: install, words, the stage |
| `level-N/` | A level; each lesson has its own folder (`level-1/lesson-01/` …): teacher guide, handout, slides outline, exit ticket, at home |
| `shows/` | The teaching show and each lesson's start / finish files (see [shows/README.md](shows/README.md)) |
| `images/` | Lesson pictures, taken from the real app by the course test |
| `web/` | The Show Tree practice version: a browser page for students who can't run the app (plain HTML/JS, no install) |
| `tools/` | How the teaching show and the practice version's show data are made |
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
3's written lessons (3.3, 3.7, 3.10) through the app's own code, and checks
the shows can't send DMX. It runs with
the rest of the app's tests. If the app changes so a lesson step stops
working, that test fails. The pictures are taken by the same test (see
[shows/README.md](shows/README.md)).

## Licence

See [LICENSE.md](LICENSE.md):

- course text and pictures: **CC BY 4.0**;
- the teaching show files, scripts and the course test: **Apache-2.0**, as
  qlcconsole is.
