---
title: Home
nav_order: 1
permalink: /
---

# Lighting a show choir with the Show Tree

A course for high-school show choir tech crews: from "what does lighting
do?" to programming and running a real show in **qlcconsole**, on a
virtual copy of a show choir stage.

> **Status:** first pass for review. The [course plan](COURSE_PLAN.md),
> the [teaching show](rig.md) and **Lesson 1.1** are written; the rest is
> planned.

## Start here

| I am… | Go to |
|---|---|
| A student | [Install qlcconsole](install.md), then your lesson's **handout** |
| The teacher | [Course plan](COURSE_PLAN.md) · [Lesson 1.1 teacher guide](level-1/lesson-01/teacher-guide.md) |
| Anyone | [Glossary](GLOSSARY.md) · [The teaching show](rig.md) |

## The levels

| Level | What you learn | Lessons |
|---|---|---|
| [0 — First look](level-0/index.md) | What light does; the virtual stage | 2 |
| [1 — Basics](level-1/index.md) | A song → cues: colour, intensity, direction, timing, GO | 5 (1.1 written) |
| [2 — Intermediate](level-2/index.md) | Tracking, palettes, effects, moving lights, busking | 7 |
| [3 — Advanced](level-3/index.md) | Patching, fixtures, building the rig, show control, running a real show | 7 |

## What's in this folder

| Path | What |
|---|---|
| `COURSE_PLAN.md` | The plan: audience, levels, design strand, lesson template, logistics, online plan, assessment, open questions |
| `install.md`, `GLOSSARY.md`, `rig.md` | Student pages: install, words, the stage |
| `level-N/` | A level; `level-1/lesson-01/` is a complete lesson (teacher guide, handout, slides outline, exit ticket, at home) |
| `shows/` | The teaching show and each lesson's start / finish files (see [shows/README.md](shows/README.md)) |
| `images/` | Lesson pictures, taken from the real app by the course test |
| `tools/` | How the teaching show is made; the GitHub Pages workflow (not turned on) |
| `_config.yml` | The website's settings (GitHub Pages, just-the-docs theme) |

## Reading it

- **On GitHub:** every page reads as it is — click through from here.
- **As a website:** with GitHub Pages turned on (see the plan, §6), the
  same pages become a site with a sidebar and search.
- **Preview the site on your computer** (needs Ruby):

  ```sh
  cd docs/course
  gem install bundler jekyll
  bundle init && bundle add jekyll just-the-docs jekyll-remote-theme jekyll-relative-links
  bundle exec jekyll serve      # then open http://127.0.0.1:4000
  ```

## Keeping it right

The lessons are checked against the app: `ui/test/courseshow` opens the
teaching show offscreen, does every step of Lesson 1.1 with the mouse and
typed, plays it with GO, and checks the show can't send DMX. It runs with
the rest of the app's tests. If the app changes so a lesson step stops
working, that test fails. The pictures are taken by the same test (see
[shows/README.md](shows/README.md)).

## Licence

qlcconsole is Apache-2.0. The course content's licence is still to be
chosen (suggested: CC BY-SA 4.0).
