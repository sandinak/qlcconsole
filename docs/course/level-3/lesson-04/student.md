---
layout: student
lesson: "Lesson 3.4"
title: "Building the rig"
permalink: /class/3-4/
nav_exclude: true
search_exclude: true
today: "Add a boom and a wall with a door to the plot, mount a par on the boom and point it, read Plot layers, and lock what is done."
files: [ShowChoir-Classroom.qlcc]
app: true
words:
  - term: Boom
    means: "A vertical pipe that lights are mounted up."
  - term: Mount
    means: "What a light hangs or stands on: a truss, boom, tower, riser or the floor."
  - term: Face
    means: "Which side of its mount a light is on, and so which way it throws."
  - term: Points
    means: "Which way a fixed light points, seen from above: 0 downstage, 90 stage right, 180 upstage, 270 stage left."
  - term: Tilt in yoke
    means: "How far a fixed light is tipped up or down from level."
  - term: Plot layer
    means: "A group of things on the plot you can hide or lock together."
---

## Why

Which light is front light and which is side light
([Lesson 0.3](../0-3/)) depends on where it hangs and which way it faces.
The console aims by angles it works out from the plot, so a plot that
doesn't match the room aims every light a little wrong.

## On paper

A spot hangs 18' up and is aimed at a mark 12' away on the floor: about 56°
down (tangent 1.5). The real truss is 16'. The console still sends 56°
down: where does the beam land (16 ÷ 1.5)?

Roles: Driver, Navigator, Designer. Switch after each part.

## Steps

1. **File → Open** → `ShowChoir-Classroom.qlcc`. Click the **Lighting
   Studio** tab (⇧⌘3), then **Plot Layers**. **House**, **Stage** and **Stage
   Lights** are locked.
2. Right-click the wing off the stage-right edge → **Add Here ▸ Boom**. At
   **Place on which layer?** pick **Towers** and **Place on this layer**.
3. In the editor: **Name** `SR Boom`, **Pipe height** `14'`, **OK**.
4. **Add ▸ Add Scenery ▸ Wall**, layer **Towers**: **Name** `Door Wall`,
   **Width** `12'`, **Height** `9'`, **Door width** `3'`, **Door height** `7'`,
   **OK**.
5. Right-click beside the boom → **Add Here ▸ Fixture…**: **Chauvet**,
   **COREpar 40 USB**, **Mode** **8-Channel**, **Name** `SR Boom Par`, **OK**.
6. Right-click **SR Boom Par** → **Mount on ▸ Boom ▸ SR Boom**. In Plot
   layers it hangs under **SR Boom**.
7. Double-click **SR Boom Par**. **Face** → **Stage left**: in **Aim &
   Mount**, **Points (° from DS)** reads 270. In **Tilt in yoke** type
   `20 down`. **OK**.
8. Right-click the boom → **Lock Layer “Towers”** and drag it: **Layer
   locked — unlock it in Layers to move**. Click the **Towers** padlock in
   Plot layers to unlock, then right-click the boom → **Lock Position**.
9. **File → Save As** → `Lesson3-4-yourname.qlcc`.

**Stuck?** Nothing on **Add Here**: click **Edit Plot** to unlock the plot.
**Mount on ▸ Boom** greyed: there is no boom yet. Something moved:
**Undo** (⌘Z).

## Exit ticket

1. A spot hangs 2' lower than the plot says. Why do its beams land in the
   wrong place when the console is working correctly?
2. A par's **Points** reads 270 and its **Tilt in yoke** reads 20 down.
   Which way does it throw, and how steeply?
3. What is the difference between a layer's lock and an object's own lock?
4. Name the three settings that point a fixed light.

## At home

Do one:

- **App:** add a second par on **SR Boom** and point it at the soloist's mark
  with **Face**, **Points** and **Tilt in yoke**; save it as
  `Lesson3-4-Home-yourname.qlcc`.
- **Paper:** measure a room you know, draw it as a plot with a boom and a
  door, and write the Face, Points and Tilt in yoke of a light on the boom.
