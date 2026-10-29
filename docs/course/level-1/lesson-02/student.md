---
layout: student
lesson: "Lesson 1.2"
title: "Colour and intensity tell the mood"
permalink: /class/1-2/
nav_exclude: true
search_exclude: true
today: "Make the verse cold and eerie by changing its look, and add a red **Scream** cue with a cue look."
files: [lesson-1-2-start.qlcc]
app: true
words:
  - term: Warm / cool
    means: "Warm colours (amber, warm white, red) read as friendly or hot; cool ones (blue, lavender, cyan) as calm, cold or eerie."
  - term: Saturated / pale
    means: "Deep and strong (Red, Blue) / white mixed in, softer (Lavender)."
  - term: Contrast
    means: "How different two moments are. A dark verse makes the chorus feel bigger."
  - term: ⋯ (more)
    means: "The button at the end of each row on a layer's page, with the ways to change that row."
  - term: One-off value
    means: "A colour or level that isn't a palette, stored on one layer only."
  - term: Make reusable
    means: "Turns a one-off value into a palette you can use anywhere."
  - term: Cue look
    means: "A colour or level for the **whole cue**: it reaches every layer that doesn't set its own."
---

## Why

Colour and level are how the audience reads a moment's mood
([Lesson 0.4](../0-4/)). The director's note is about how the verse
feels; the fix is a change of colour and intensity, and the scream is a
hit ([Lesson 0.6](../0-6/)).

## The plan

The director: *"The verse is too cosy. Make it cold and a little eerie.
And when they scream, the whole stage goes red."* Fill in **Why** first.

| Cue | When | Why | Lights |
|---|---|---|---|
| 1.0.1 Verse *(change)* | First words | | StepTops: **Lavender, 40%** (was Warm White, 75%) |
| 1.0.3 Scream *(new)* | The first scream | | TrussW-RGB and Lanterns, whole cue: **Red, Full** |

Roles: Driver, Navigator, Designer. Switch after each part.

## Steps

1. **File ▸ Open** → `lesson-1-2-start.qlcc` (Pose, Verse and Chorus from
   Lesson 1.1).
2. Click the **Show Tree** tab (⇧⌘5); open **View ▸ Rig Overview** (⇧⌘R)
   beside it.
3. In the tree, click **1.0.1 Verse**.
4. On the page, click the **StepTops** row under **Layers**. Its own page
   opens, with a **Colour** row and a **Dimmer** row.

   ![The layer's page](../../images/lesson-1-2/layer-page.png)

5. On the **Colour** row: **⋯ → Set to → Lavender**. The top bar reads
   *Changed: 1.0.1 Verse, 1.0.2 Chorus*: StepTops tracks into the chorus.
6. On the **Dimmer** row: **⋯ → Level…**, type **40**, click **Apply**.
   The row reads *40% · 1.0.1 Verse StepTops*: a one-off value.
7. Click **Make reusable**. 40% is now a palette in the Library.

   ![The level and Make reusable](../../images/lesson-1-2/make-reusable.png)

8. Press **⌘Z** (Edit ▸ Undo) three times (warm again), then **⇧⌘Z**
   (Edit ▸ Redo) three times (cool again).

   ![The cool verse](../../images/lesson-1-2/rig-verse-lavender.png)

9. Click **1.0.2 Chorus**, then **+ Cue**, type `Scream`, press **Return**.
10. Drag **TrussW-RGB** (Groups → Trusses) onto the dashed box under
    **Layers**, then **Lanterns** (Groups → Lanterns) onto the dashed box.
11. Drag **Red** onto the **dashed box itself**, not a row. The top bar
    reads *reaches 2 of 2 layers*.
12. Drag **Full** onto the dashed box the same way. Both rows read **Red,
    Full**; the stage floods red.

    ![The Scream's cue looks](../../images/lesson-1-2/cue-looks.png)

13. Click Pose, Verse, Chorus and Scream in turn. Is there real contrast
    between the verse and the chorus?
14. **File ▸ Save As** → `Lesson1-2-yourname.qlcc`.

**Stuck?** No ⋯ buttons: click the StepTops row first. Red went onto one
layer only: ⌘Z and drop it on the dashed box. Anything else: the app's
help covers the tab you're in.

{% include inapp.html do="Help on what you're looking at" path="Help ▸ qlcconsole Help" mac="F1" win="F1" see="It opens on the page for what you're looking at. On a Mac laptop, hold **fn** and press F1." %}

## Exit ticket

1. You changed the verse to **Lavender at 40%**. How does it feel now,
   and what makes it feel that way?
2. How do you change the colour of a layer you've already programmed?
3. After **Level… 40**, the row read *40% · 1.0.1 Verse StepTops*. What
   does that mean, and what does **Make reusable** do?
4. What is a **cue look**?

## At home

Do one:

- **App:** download [`lesson-1-2-finish.qlcc`](../../shows/lesson-1-2-finish.qlcc).
  New note from the director: *"Make the whole song happy and warm, like a
  party."* Rework every cue with **⋯ → Set to**, **⋯ → Level…** and cue
  looks; keep contrast between verse and chorus. Bring in
  `Lesson1-2-Home-yourname.qlcc`.
- **Paper:** design the verse twice, *eerie* and *happy*: for each, a
  group, a colour, a level and one sentence of why.
