---
title: "3.1 Slides outline"
parent: "Lesson 3.1 — DMX, universes and patching"
grand_parent: "Level 3 — Advanced"
nav_order: 3
---

# Slides — Lesson 3.1: DMX, universes and patching

*An outline: one heading per slide, with what's on it and what the
teacher says. It can be turned into a deck as it is.*

---

## 1. DMX, universes and patching

- Title, Level 3 · Lesson 1
- Picture: a light's back panel with DIP switches
- *Say:* "The console and the light have to agree on one number."

## 2. A universe is 512 channels

- DMX: each channel 0 to 255. One line of 512 = one universe.
- A light takes as many channels as its mode has: its footprint.

## 3. The address

- The channel a light starts at; the next light starts right after.
- Picture: Universe 1's grid (357 of 512 used).

## 4. Two lights, one address

- Both follow the same cues. *Ask:* "What does the audience see?"

## 5. Patch it in qlcconsole

- Fixtures tab ▸ Universes ▸ a universe: the grid.
- Add ▾ → Add Fixture…: the app picks the next free address.
- Right-click → Patch N Fixtures…: the Patch dialog, reviewed before **Apply**.

## 6. Over the network

- Art-Net and sACN: DMX in a network cable, a universe per stream.
- A node turns universes into DMX ports. Part 3, on the rig.

## 7. The paperwork

- Fixtures → Patch List…: group by Location, Universe or Type; Save XLSX…; Print Labels….

## 8. Your turn

- Roles: Driver · Navigator · Designer. Handout Part 2.

## 9. Exit ticket

- 4 questions on paper.
