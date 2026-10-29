---
layout: student
lesson: "Lesson 3.10"
title: "Coming from Eos"
permalink: /class/3-10/
nav_exclude: true
search_exclude: true
today: "Import an Eos show, fix how it reads the special circuits, and find each Eos idea you know in qlcconsole."
files: [classroom-eos.asc]
app: true
words:
  - term: USITT ASCII
    means: "The plain-text show file Eos exports (**File ▸ Export ▸ USITT ASCII**): patch, groups, palettes, subs, cue lists."
  - term: Special circuit
    means: "A channel that isn't a stage light: a relay, house lights, work lights, backstage blues."
  - term: Sub (submaster)
    means: "On Eos, a fader that brings up a set of levels by hand."
  - term: Output rules
    means: "From Lesson 3.7: **Ignore Blackout**, **Ignore the Grand Master**, **Switch (relay)**, **On when not in use**."
---

## Why

The ideas you learnt in Level 0 (cues, looks, calls) don't belong to one
console ([Lesson 0.7](../0-7/)). If you've run an Eos desk, your show
and your habits can come with you.

## On paper

Fill in the right-hand column as you go.

| On Eos | In qlcconsole |
|---|---|
| A channel (*11 House Lights*) | |
| A group (*Front*) | |
| A cue list, with tracking | |
| A sub on a fader | |
| A channel marked as a relay or house light | |

Roles: Driver, Navigator, Designer (fills in the table). Switch at each
page of the wizard.

## Steps

1. Open qlcconsole and choose **File → New**.
2. **File → Import → ETC Eos Show (ASCII .asc)…** → `classroom-eos.asc`,
   then **Next**.
3. **Fixture types:** leave **Bring the cue lists into the Show Tree as
   tracking cue lists** ticked, **Next**.
4. **Special circuits:** check each row. 11 House, 12 Work, 13 Relay
   (power), 14 Backstage, and **5 Cyc Blue** as Backstage too.
5. Channel 5 is a stage light: set its **Role** to **Normal**, then
   **Finish**.
6. In the list of what comes in, click **OK**. **Show Details** lists the
   output rules each circuit got.
7. **Fixtures** tab (⇧⌘2): ten fixtures named after their Eos channels,
   and groups in the folder **Eos**.
8. **Show Tree** tab (⇧⌘5): the cue list with the Eos numbers, 1 to 4,
   tracking.
9. **Virtual Console** tab: the frame **Eos subs**, with **Sub 1 Work**.
10. **File → Save As** → `Lesson3-10-yourname.qlcc`.

**Stuck?** **Import** greyed out: click **Design**. Cyc Blue still a
backstage circuit: Fixtures → right-click **Ch 5 Cyc Blue** → **Output
Rules…** and untick them.

## Exit ticket

1. What is a **special circuit**? Give two examples.
2. How does the import decide a channel is special, and why did you check
   every row?
3. Where did the Eos **sub** end up?
4. Name two Eos ideas and where they live in qlcconsole.

## At home

Do one:

- **App:** import `classroom-eos.asc` into a new show as in class. Add a
  cue between **3 House out** and **4 Opening** for a slow cyc build.
  What number does it get, and why? Bring in
  `Lesson3-10-Home-yourname.qlcc`.
- **Paper:** finish the table, and add three Eos ideas you use that
  aren't in it, with where you think each lives in qlcconsole.
