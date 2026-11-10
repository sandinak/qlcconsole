---
layout: student
lesson: "Lesson 3.2"
title: "Fixtures and definitions"
permalink: /class/3-2/
nav_exclude: true
search_exclude: true
today: "Read the definitions behind the rig: channels, modes and heads. Compare a pixel box's three modes, find the colour and white heads, and open the Fixture Editor."
files: [ShowChoir-Classroom.qlcc]
app: true
words:
  - term: Fixture definition
    means: "The file that tells the app what each channel of a light does."
  - term: Mode
    means: "One way a light can be set up: a list of its channels in order. It decides how many channels the light takes."
  - term: Head
    means: "One lit part of a fixture: a pixel box has many, a mover has one."
  - term: Included / Local
    means: "Where a definition file comes from: shipped with the app, or not (yours, or the show's)."
  - term: Fixture Editor
    means: "The tool for reading and writing definitions."
---

## Why

A look is colour, intensity and position
([Lesson 0.2](../0-2/)), and the console can only set them if it knows
which channel is which. A light's definition says so. Pick the wrong mode
and Blue comes out green, or the light waits for a dimmer nobody sends.

## On paper

A par has modes **3ch** (Red, Green, Blue), **5ch** (Red, Green, Blue,
Dimmer, Strobe) and **8ch** (Dimmer, Red, Green, Blue, White, Strobe,
Macro, Speed). Which modes can dim the whole colour from the light? How
many of each fit in one universe? The light is set to 5ch but patched as
3ch: what happens on channel 4?

Roles: Driver, Navigator, Designer. Switch after each part.

## Steps

1. **File → Open** → `ShowChoir-Classroom.qlcc`. Click the **Fixtures** tab (⇧⌘2).
2. Open **Fixture Groups ▸ Trusses** and click **SR Truss Cyc 1**:
   **Manufacturer** Warmdance, **Model** XL-450, **Mode** 165ch, **Type** LED
   Bar (Pixels), **Definition** Local, **Channels** 165.
3. Click **SR Truss Spot 1**: a Moving Head, 12 Channel. Which channel dims
   it? What dims the cyc box instead?
4. Right-click **SR Truss Cyc 1** → **Definition & Mode…**. Pick each **Mode**
   and read **Channels**: 165, 135, 30. **Cancel**.
5. How many boxes fit in a universe in each mode (165ch, 135ch RGB, 30ch White)?
6. Click **TrussC-RGB**: each cell is a head, with the box's name, **H:**,
   **A:**, **U:** and the tag **[RGB]**. Click **TrussC-W**: the tag is **[W]**.
7. Right-click **SR Truss Cyc 1** → **Edit Fixture Definition…**. In the Fixture
   Editor click **Modes**: **Name**, **Channels**, **Heads** (75, 45 and 30
   heads). Close it **without saving**.
8. **File → Show Definitions…**: the five definitions this show carries.
9. **File → Save As** → `Lesson3-2-yourname.qlcc`.

**Stuck?** **Definition & Mode…** missing: right-click one light, not a group.
Pressed **OK** in it: ⌘Z on the Fixtures tab, or leave it. The Fixture Editor
is slow: it is its own program and may open behind.

## Exit ticket

1. What does a fixture definition tell the console?
2. What is a mode, and what two things does it decide?
3. Which of our groups holds the XL-450's colour pixels, and what does a
   colour on it do to the white ones?
4. The XL-450 reads Local. What does that mean, and why does the show still
   open on another computer?

## At home

Do one:

- **App:** open **Fixtures → Fixture Editor…**, look up a light you know,
  and write down its modes, channels, heads and dimmer channel.
- **Paper:** copy a light's DMX chart as a table and work out how many fit
  in a universe in each mode.
