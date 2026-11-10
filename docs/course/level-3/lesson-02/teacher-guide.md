---
title: "3.2 Teacher guide"
parent: "Lesson 3.2 — Fixtures and definitions"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.2 — Fixtures and definitions

**Teacher guide** · Level 3, lesson 2 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-2/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-2.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-2/">

**Builds on:** Lesson [0.2](../../level-0/lesson-02/teacher-guide.md) and Lesson [3.1](../lesson-01/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say what a fixture definition, a mode and a head are, and where the console gets them;
2. read a light's channels, mode and channel count in the Fixtures tab, and say what a different mode would cost in channels;
3. find the heads a group is made of, and open a definition in the Fixture Editor without changing it.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] `ShowChoir-Classroom.qlcc` on each laptop (from `docs/course/shows/`).
- [ ] On the projector: the show open, the **Fixtures** tab, **Fixture Groups ▸ Trusses** open.
- [ ] The Fixture Editor installed with the app (it opens from the Fixtures tab; check once before class).
- [ ] The manual page of any real light with a DMX chart and several modes.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**fixture definition**, **mode**, **head**, **Included / Local**, **Fixture Editor**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | A manual's DMX chart; Part 1's three modes |
| 10–20 | **Teacher demo** (projector) | XL-450's info; Definition & Mode…; a group's heads |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | Each group names the mode of one real light and what it costs |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Hand out the DMX chart from a real manual. Ask the class to find the
channel that dims the light, and the channel that sets its colour. Then
ask: "The console has never seen this light. How does it know that channel
4 is a dimmer?" (*a definition says so*). Then Part 1's three modes: the
same light, three different footprints.

**Key idea to land:** the console doesn't know lights; it knows
definitions. If the definition (or the mode) is wrong, every cue that
touches the light is wrong.

## 2. Teacher demo (10 min)

1. **Fixtures** tab, **SR Truss Cyc 1**: Manufacturer, Model, **Mode**
   165ch, **Type**, **Definition** Local, and the channel table.
2. Right-click → **Definition & Mode…**, open **Mode**, step through the
   three modes, then **Cancel**.
3. **Fixture Groups ▸ Trusses ▸ TrussC-RGB** then **TrussC-W**: a cell's
   tooltip, tagged **[RGB]** and **[W]**.
4. Right-click **SR Truss Cyc 1** → **Edit Fixture Definition…**: **Modes**
   tab, then close.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching every part.
- Students without a laptop: the **design station**: three more DMX
  charts (a dimmer, an RGB par, a mover) and the table of Part 2 step 6 for
  each.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| **Definition & Mode…** missing | A group is selected | "Right-click one light." |
| The footprint changed | They pressed **OK** in Definition & Mode… | "⌘Z on the Fixtures tab, or leave it: it's your copy." |
| The Fixture Editor is slow to appear | It's its own application | "Give it a few seconds; it may open behind." |
| "Why do the cyc boxes show Local?" | The file isn't shipped with the app | "The show carries its own copy, so it opens complete anywhere." |
| A student wants to edit the definition | | "Reading is the point. Close without saving: the file is used by the show." |

## 4. Check for understanding (while circulating)

- "Which channel of the spot dims it, and what dims the cyc box?" (*the spot's Dimmer channel; the box has no dimmer channel, so the console dims its colour*)
- "What does 135ch RGB give up against 165ch?" (*the 30 white pixels*)
- "Why does the group TrussC-RGB not light the white pixels?" (*its heads are only the colour ones*)

## 5. Share (5 min)

Each group names one light from the rig, its mode, its channel count, and how many would fit in a universe (Lesson 3.1).

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

**SR AT Spot** and **SL AT Spot** are the same model in different modes
(18 and 21 channels). Open **Definition & Mode…** on each and read the
modes. Ask: "What would change if both were patched in the larger mode?"
(*three more channels each; the cues are the same*). Handout extension 2: a UST uses the Junman Two Arm LED Beam: **9 Channel** has 1 head, **15 Channel** (ours) has 2 heads.

## Part 3 — On the real rig (with Branson)

At the rig, with the Fixtures tab open beside a real light:

1. Find a light's model on its label and the same model in the Fixtures tab. Read its **Mode**.
2. On the light's own menu, read its mode. It has to be the console's mode, channel for channel. Change nothing unless Branson says to.
3. A light with a mode the show doesn't use: look at its definition's **Modes** tab for the one the console should be set to.

<!-- PICTURE (real hardware): a pixel box's menu showing its DMX mode, beside the Fixtures tab -->

## At home

[At home](at-home.md): read a definition on your own computer, or work out a patch on paper.

## Answer key

- **Handout Part 1:** modes 5ch and 8ch have a Dimmer channel. 3ch: 170 per universe (510 channels); 5ch: 102; 8ch: 64. Light set to 5ch, console patched as 3ch: the light waits on channel 4 for the dimmer. The console patched it as 3 channels, so channel 4 carries nothing, or whatever the next light's first channel carries: the dimmer sits at zero or follows another light.
- **Handout Part 2 step 6:** 165ch: 3; 135ch RGB: 3 (405 channels; four would be 540); 30ch White: 17.
- **Exit ticket:** 1: the file that tells the console what each channel of a light does. 2: the list of channels a light is set to; the mode set on the light and on the console have to be the same, and the mode decides how many channels the light takes. 3: heads: a box's 45 colour pixels and 30 white pixels; the group holds the colour heads only, so the white ones stay dark. 4: Local: the file isn't one the app ships; the show carries its own copy so it opens complete on another computer.

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonFixturesAndDefinitions`) runs it offscreen on the real app: the XL-450's info (Warmdance, 165ch, LED Bar (Pixels), Local, Red 1 …) and the spot's, the XL-450's modes by name, channels and heads (165ch/165/75, 135ch RGB/135/45, 30ch White/30/30), the **Definition & Mode…** dialog's mode list and channel counts, the groups' heads (TrussC-RGB 180, TrussC-W 120), that **Edit Fixture Definition…** has a file to open, **File → Show Definitions…** and its five rows, and the Fixtures menu's **Fixture Editor…**. It doesn't open the Fixture Editor window itself, which is a separate program.
