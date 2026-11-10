---
layout: student
lesson: "Lesson 3.1"
title: "DMX, universes and patching"
permalink: /class/3-1/
nav_exclude: true
search_exclude: true
today: "Work out where lights fit in a universe, read the classroom show's patch, add a light and move a block with the Patch dialog, and make the patch list and labels."
files: [ShowChoir-Classroom.qlcc]
app: true
words:
  - term: Universe
    means: "One DMX line of 512 channels."
  - term: Address
    means: "The channel where a light's channels start in its universe."
  - term: Footprint
    means: "How many channels a light takes in its mode."
  - term: Patch
    means: "The list of which light is at which universe and address."
  - term: Art-Net / sACN
    means: "DMX sent over a network cable: one stream per universe."
  - term: Patch list
    means: "Every light, its patch, where it hangs and where its universe goes: the paperwork."
---

## Why

The board op calls GO and the cue plays
([Lesson 0.7](../0-7/)), but only if the console and the light agree on
one number: the address. A light patched one channel off does the wrong
thing, or nothing.

## On paper

Patch in order from channel 1: a dimmer (1 channel), two RGB pars (3 each),
two moving heads (12 each). Write each start and end. What happens if two
lights share an address? How many 12-channel heads fit in one universe?

Roles: Driver, Navigator, Designer. Switch after each part.

## Steps

1. **File → Open** → `ShowChoir-Classroom.qlcc`.
2. Click the **Fixtures** tab (⇧⌘2). Open **Universes** and click the
   first one, **Spots, washes, beams, lanterns** (Universe 1): the grid and
   **357 / 512 addresses used (69%)**.
3. Click **SR Truss Spot 1**: **Address Range** 214 - 225, **Channels** 12,
   **Binary Address (DIP)**. Click **Spot 2**: why does it start at 226?
4. Click **Truss pixel boxes 1** (Universe 2): 495 of 512. Why isn't a fourth XL-450 in it?
5. **Add ▾ → Add fixture…**: **WLED → Lantern**, **Mode** **Effect (15ch)**,
   **Name** `Lantern 7`. The app picks Universe 1, address 358. **OK**.
6. **Add ▾ → Add fixture…** again, the same Lantern, **Address** `365`:
   the red **ERROR: Address already used!** **Cancel**.
7. Click **Lantern 1**, ⇧-click **Lantern 6**, right-click → **Patch 6
   Fixtures…**. Set **Place selected at universe** to **Step fronts 1**
   (Universe 5), **address** 385, click **Place**: each **Status** reads *moves from 1.n*. Click **Apply**.
8. Press ⌘Z on the Fixtures tab: the lanterns are back on Universe 1.
9. **Fixtures → Patch List…**: grouped by **Location**; change **Group by**
   to **Universe** and **Type**.
10. **Save XLSX…** into **Documents → qlcconsole** and open it.
11. **Print Labels…**: read the preview (names and "D1.214"), **Save PDF…**.
12. **File → Save As** → `Lesson3-1-yourname.qlcc`.

**Stuck?** **Add ▾** greyed out: click **Design**. Red error stays: pick the
next free address. **Apply** greyed: read the summary line.

## Exit ticket

1. What is a universe, and what is an address?
2. A new light takes 12 channels and the last light ends at 357. Where
   does it start and end?
3. What does Add fixture say if two lights overlap, and what goes wrong on
   stage if it is ignored?
4. Who uses the patch list and labels, and for what?

## At home

Do one:

- **App:** add four WLED Lanterns with **Quantity** 4 and an **Address gap**
  of 5; save the patch list as `Lesson3-1-Home-yourname.xlsx`.
- **Paper:** patch six 7-channel pars, two 16-channel movers and a
  4-channel dimmer pack on a 512 grid; write the label for the second
  mover's cable end.
