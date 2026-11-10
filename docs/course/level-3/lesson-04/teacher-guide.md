---
title: "3.4 Teacher guide"
parent: "Lesson 3.4 — Building the rig"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.4 — Building the rig

**Teacher guide** · Level 3, lesson 4 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-4/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-4.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-4/">

**Builds on:** Lesson [0.3](../../level-0/lesson-03/teacher-guide.md), Lesson [0.8](../../level-0/lesson-08/teacher-guide.md) and Lesson [3.3](../lesson-03/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. add a boom and a wall to the plot, and mount a light on the boom;
2. point a fixed light with **Face**, **Points** and **Tilt in yoke**, and say which way each goes;
3. read Plot layers (what hangs on what), use a layer's lock and an object's own lock, and say why the plot has to match the room.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] `ShowChoir-Classroom.qlcc` on each laptop (from `docs/course/shows/`).
- [ ] On the projector: the show open, the **Lighting Studio** tab (⇧⌘3), Plot Layers open.
- [ ] A tape measure for the design station.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] For Part 3: the rig's trusses and a boom to measure (with Branson).

## Vocabulary (introduce in this order)

**boom**, **mount**, **face**, **points**, **tilt in yoke**, **plot layer**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Handout Part 1: a truss 2' lower than the plot says |
| 10–20 | **Teacher demo** (projector) | Add Here ▸ Boom; the layer question; Mount on; Face |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | Each group says which way its par points, and why |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Give the numbers of Part 1: a spot 18' up, aimed at a mark 12' away on the
floor. Let them find 56° (tangent 1.5). Then hang it 16' up: the console
still sends 56° down, and the beam lands 10' 8" away: 1' 4" short of the
mark: the pool sits in front of the singer's toes and the singer stands in its far edge.
Ask: "What did the console get wrong?" (*nothing: the plot did*).

**Key idea to land:** the console is only as right as the plot. Every aim
is arithmetic from the plot's numbers.

## 2. Teacher demo (10 min)

1. **Lighting Studio** tab, **Plot Layers**: the first three layers
   (House, Stage, Stage Lights) are locked.
2. Right-click the wing → **Add Here ▸ Boom**. "Place on which layer?":
   the active layer is locked. Pick **Towers**.
3. Name it, **Pipe height** `14'`, OK. Add the par (**Add Here ▸
   Fixture…**), then right-click it → **Mount on ▸ Boom ▸ SR Boom**.
4. Double-click the par: **Face ▸ Stage left**, then **Aim & Mount**:
   **Points (° from DS)** reads 270; type `20 down` in **Tilt in yoke**.
5. Right-click the boom → **Lock Layer “Towers”**; drag it: the status line
   says why it didn't move.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching every part.
- Students without a laptop: the **design station**: draw the SR wing on a
  paper plan, put the boom about 4' off the stage edge with a par on it, and
  write its Face, Points and Tilt in yoke.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| "Place on which layer?" | The active layer (House) is locked | "That's on purpose: pick **Towers**." |
| Nothing happens on **Add Here** | The plot is locked (**Plot Locked**) | "Click **Edit Plot**." |
| **Mount on ▸ Boom** is greyed | No boom yet | "Add the boom first." |
| The par points the wrong way | Face is the side of the mount, not the target | "Face says which way it throws: Stage left throws toward stage left." |
| They can't drag the boom | A lock | "Read the status line: layer, or the object's own?" |
| A length reads in metres | Settings ▸ Units | "The fields follow Settings ▸ Units: set feet and inches." |

## 4. Check for understanding (while circulating)

- "Points reads 270. Which way is that?" (*toward stage left*)
- "What is the difference between a layer's lock and an object's own?" (*the layer holds everything on it; the object's lock holds one thing, even when its layer is open*)
- "Why does the boom need to be where the real one is?" (*the console aims every light on it from that position*)

## 5. Share (5 min)

Each group says its par's **Points** and **Tilt in yoke** and what the beam does on stage.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

Copy and paste the boom to the other wing; **Show Light Footprint** with two different **Tilt in yoke** values. Ask: "Which tilt lights faces, and which lights the floor?" (*steeper: the floor; shallow: faces and the back wall*): a steep beam lands near the light's own feet; a shallow one reaches the people.

## Part 3 — On the real rig (with Branson)

At the rig with a tape measure and a laser measure:

1. The height of each truss, the position of the boom along and across the stage, and the stage's width and depth. Write them down against the plot's.
2. Where a number differs, double-click the structure on the plot and type the real one.
3. Fire an aim at a mark (the **Soloist DS** aim from Level 1) and see whether the beam lands on it. If it doesn't, check the plot's numbers again before touching the light.

<!-- PICTURE (real hardware): a tape on a real truss next to the plot's height for it; the beam of an aimed light landing on a spiked mark -->

## At home

[At home](at-home.md): measure a room, or sketch a rig on paper.

## Answer key

- **Handout Part 1:** the beam lands at 16 ÷ 1.5 = 10' 8" (10.67') from the light, 1' 4" short of the mark. The truss height, and how far the mark is from the truss: a tape or a laser measure.
- **Exit ticket:** 1: the plot's numbers decide the angles; wrong numbers aim every light on that structure wrong. 2: toward stage left, 20 degrees below level. 3: a layer's lock holds everything on that layer; an object's lock holds that one object. 4: Face; Points; Tilt in yoke.

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonBuildingTheRig`) runs it offscreen on the real app: the plot's toolbar (**Add**, **Plot Layers**, **Edit Plot**), the scenery list's **Wall**, the classroom's layers (House, Stage and Stage Lights locked, so a new object asks where it goes; **Towers** open), a boom named SR Boom and a wall with a 3' door, a Chauvet COREpar 40 USB with its 8-Channel mode mounted on the boom, **Face ▸ Stage left** giving **Points** 270 and level, the par listed under SR Boom in Plot layers, and a layer's lock. It doesn't drive the Add fixture dialog or the editors, and it can't measure a real room (Part 3).
