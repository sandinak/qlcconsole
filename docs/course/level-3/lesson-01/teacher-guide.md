---
title: "3.1 Teacher guide"
parent: "Lesson 3.1 — DMX, universes and patching"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.1 — DMX, universes and patching

**Teacher guide** · Level 3, lesson 1 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-1/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-1.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-1/">

**Builds on:** Lesson [0.7](../../level-0/lesson-07/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say what a universe, a channel, an address and a footprint are, and why two lights can't share channels;
2. find any light's universe, address and channels in the Fixtures tab, and say where the next light would go;
3. re-address a block of lights with the Patch dialog, and make the patch list and labels the crew works from.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] `ShowChoir-Classroom.qlcc` on each laptop (from `docs/course/shows/`).
- [ ] On the projector: the show open, the **Fixtures** tab, **Universes** open.
- [ ] A spreadsheet program on the laptops, for the XLSX.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] For Part 3: the rig's network node and a light with DIP switches (with Branson).

## Vocabulary (introduce in this order)

**DMX**, **channel**, **universe**, **address**, **footprint**, **patch**, **DIP switches**, **Art-Net / sACN**, **node**, **patch list**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Handout Part 1: patch five lights on paper; the clash |
| 10–20 | **Teacher demo** (projector) | Universe 1's grid; one light's info; the Patch dialog |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | Each group says where the next 165-channel box would go |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Draw a row of 512 boxes on the board and let the class patch the five
lights of Part 1 into it in order. Then give two lights the same start
address and ask what the audience sees when the SM calls a cue for one of
them. Ask: "The electrician is on a ladder and the light isn't answering.
What did she check first?" (*the address on the light*; many fixtures set
it with DIP switches or a small display).

**Key idea to land:** a patch is a promise that the console and the light
agree on one number, and it has to be true on every one of the rig's lights.

## 2. Teacher demo (10 min)

1. **Fixtures** tab, **Universes ▸ Spots, washes, beams, lanterns** (Universe 1): the grid. "The coloured
   cells are used, the pale ones free. 357 of 512."
2. **SR Truss Spot 1**: **Address Range** 214 - 225 and the **Binary
   Address (DIP)** pattern. "That is what is set on the light itself."
3. **Add ▾ → Add fixture…** with the Lantern: the app picks 358. Type 365:
   the red error.
4. Select Lanterns 1 to 6 → right-click → **Patch 6 Fixtures…**: read the
   columns and the **Status**; cancel.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching every part.
- Students without a laptop: the **design station**: patch the lights of
  Part 1 and then the classroom show's Universe 11, **Step tops 1**, on a printed 512 grid
  (eight 64-channel bars, full).

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| Add ▾ is greyed | They are in Operate | "Click **Design** at the top right." |
| They can't find **Lantern** | They are looking under Generic | "It's under **WLED**." |
| They look for "Universe 5" in the Patch dialog | Its list is by name | "Universes are named for what is on them: **Step fronts 1** is Universe 5." |
| "I patched Lantern 7 and now there are seven" | Expected | "Delete it or leave it: it's your copy." |
| The Patch dialog's **Apply** is greyed | A conflict | "Read the summary line: which fixture, and what it clashes with." |
| Undo does nothing | Another tab is in front | "⌘Z acts on the tab you're on. Click the **Fixtures** tab." |
| The labels read "D1.214", not "D214" | Expected | "The rig has several universes: universe first." |

## 4. Check for understanding (while circulating)

- "Why does SR Truss Spot 2 start at 226?" (*Spot 1 takes 214 to 225, and the next light starts right after*)
- "**Truss pixel boxes 1** has 495 of 512 used. Why can't a fourth XL-450 go in the last 17?" (*it needs 165 channels, and a light can't be split across two universes*)
- "Why does the app say *moves from 1.1*?" (*universe 1, address 1: where it is now*)

## 5. Share (5 min)

Each group says the universe and address the next 165-channel box would take in the classroom show, and why (*Universe 4, **Truss pixel boxes 3**, address 331: the first universe with 165 free channels*).

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

The Patch dialog's **one universe each, counting up**: the first lantern on one universe and the next on the universe after … each at the address typed. Ask: "When does a rig want that?" (*a row of lights, each on its own node port*).

## Part 3 — On the real rig (with Branson)

The classroom show sends nothing. On the rig:

1. **Connections ▸ Devices**: the nodes the console hears, and their ports. Point at a node on the truss and at its **port 1**.
2. Pick one universe in the rig's show: **Edit connection…** (right-click, or double-click). Read protocol, node and port; **Cancel**. Don't change anything unless the teacher asks.
3. At a light on a ladder: read its address from its display or DIP switches, and against the Fixtures tab's **Binary Address (DIP)**.
4. Print labels for a few lights from **Fixtures → Patch List…** and put them on the cable ends.

<!-- PICTURE (real hardware): a light's DIP switches set to a patched address; a node's front panel with the ports numbered -->

Leave Art-Net and sACN settings to Branson: a wrong universe on a live network can drive another room's lights.

## At home

[At home](at-home.md): patch a small rig on paper, or find a real light's address.

## Answer key

- **Handout Part 1:** dimmer 1 to 1; par A 2 to 4; par B 5 to 7; mover A 8 to 19; mover B 20 to 31. Mover B on par B's start: both follow the same channels, so a cue for one drives the other. 42 movers (504 channels). A light that doesn't fit in the free channels goes in another universe whole; a light can't be split.
- **Exit ticket:** 1: a universe is one DMX line of 512 channels; an address is the channel a light starts at. 2: it starts at 358, the first free channel after 357, and ends at 369 (12 channels). 3: the red **ERROR: Address already used!**, or Apply greyed in the Patch dialog; two lights following the same channels. 4: the crew hanging and labelling the rig; the electrician at the light.

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonDmxAndPatching`) runs it offscreen on the real app: Universe 1's usage (357 of 512), SR Truss Spot 1's address range and DIP pattern, the address the app offers a new Lantern (Universe 1, 358), the clash message on 365, the Patch dialog placing Lanterns 1 to 6 on **Step fronts 1** (Universe 5) from 385 (0 conflicts), Apply and Undo, the patch list's three groupings, the XLSX it saves, and the labels it makes ("D1.214"). The real network (Part 3) is not testable here.
