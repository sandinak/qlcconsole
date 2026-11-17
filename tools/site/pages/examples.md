---
slug: examples
title: Examples
menu_order: 3
parent: features
---
Real show files to open, poke at and take apart. They are all **safe on any
network**: no universe has an output and no light has a network address, so
opening one never sends DMX to anything. Download a file, then use
**File ▸ Open** in qlcconsole.

Most of them come from the course for teachers, where each lesson's finished
show is the next lesson's starting point. You can follow the course from any
of them: [Lighting a show choir with the Show Tree](https://sandinak.github.io/qlcconsole/).

## The teaching stage

![The teaching stage, dark](docs/course/images/lesson-1-1/rig-dark.png)

A show choir stage built from a real production and scaled down for class:
risers, trusses, audience towers and a cyc, 68 lights grouped and ready, a
few stage targets, and base colours and levels. There's no show on it yet,
so it's the place to start your own. It's also built into the app as
**File ▸ New from Template ▸ Show-choir stage (classroom)**.

[Download ShowChoir-Classroom.qlcc](https://github.com/sandinak/qlcconsole/raw/main/docs/course/shows/ShowChoir-Classroom.qlcc)
· [What's on the stage](https://sandinak.github.io/qlcconsole/rig.html)

## A first song in three cues

![The verse cue on the teaching stage](docs/course/images/lesson-1-1/rig-verse.png)

*Spring Show / Everybody Scream*: three cues (Pose, Verse, Chorus) written
in the Show Tree with the mouse. Small enough to read end to end, and a good
first look at how cues track.

[Download lesson-1-1-finish.qlcc](https://github.com/sandinak/qlcconsole/raw/main/docs/course/shows/lesson-1-1-finish.qlcc)

## Busking hits on the Virtual Console

![Three hit buttons on a Virtual Console page](docs/course/images/lesson-2-6/vc-hits.png)

The song grown into a full number, with effects, a fan of moving lights, and
a *Hits* section (Red Stab, Glow, Faces) that fires from Virtual Console
buttons on top of whatever cue is running.

[Download lesson-2-6-finish.qlcc](https://github.com/sandinak/qlcconsole/raw/main/docs/course/shows/lesson-2-6-finish.qlcc)

## Two songs, ready to run

![The cue sheet for two songs](docs/course/images/lesson-3-8/cue-sheet.png)

The most complete example: two songs (*Everybody Scream* and *Slow Dance*)
numbered Show.Part.Iteration, sub-cues, cue times that follow timecode, a
group master and an override hit with its own button. Open the **Run** tab
and press GO.

[Download lesson-3-8-finish.qlcc](https://github.com/sandinak/qlcconsole/raw/main/docs/course/shows/lesson-3-8-finish.qlcc)

## Importing an Eos show

![Special circuits found in an imported Eos show](docs/course/images/lesson-3-10/special-circuits.png)

A small hand-written Eos show in USITT ASCII: ten dimmer channels, house,
work and relay circuits (one labelled so the import guesses it wrong, on
purpose), four cues and a sub. Use **File ▸ Import ▸ ETC Eos Show (ASCII)**
to see how qlcconsole brings an Eos show over.

[Download classroom-eos.asc](https://github.com/sandinak/qlcconsole/raw/main/docs/course/shows/classroom-eos.asc)

## The QLC+ sample

![The Classic QLC+ view](docs/images/views-classic.png)

QLC+'s own sample workspace, as it ships with the app. It opens in the
**Classic QLC+** view, so it's a quick way to check that a QLC+ 4 show
carries over unchanged.

[Download Sample.qxw](https://github.com/sandinak/qlcconsole/raw/main/resources/samples/Sample.qxw)

All the course show files are in
[docs/course/shows](https://github.com/sandinak/qlcconsole/tree/main/docs/course/shows),
one start file and one finish file per lesson.
