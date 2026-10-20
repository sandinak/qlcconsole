---
title: "1.2 Student handout"
parent: "Lesson 1.2 — Colour and intensity tell the mood"
grand_parent: "Level 1 — Basics"
nav_order: 2
---

# Lesson 1.2 — Colour and intensity tell the mood

**Student handout**

## Today you will

- choose colours and levels for a **mood**;
- change a look you already made, without starting again;
- make a cue where the **whole cue** has one colour (a **cue look**).

## Words for today

| Word | Means |
|---|---|
| **Warm / cool** | Warm colours (amber, warm white, red) feel friendly, close or hot. Cool colours (blue, lavender, cyan) feel calm, cold, night-time or eerie. |
| **Saturated / pale** | A saturated colour is deep and strong (Red, Blue). A pale one is soft, with white in it (Lavender). |
| **Contrast** | How different two moments are. A dark verse makes the chorus feel bigger. |
| **Mood** | What a moment feels like. |
| **⋯ (the "more" button)** | On a layer's page, the button at the end of each row. It changes that row. |
| **One-off value** | A colour or level that isn't one of our palettes, kept just for one layer. |
| **Make reusable** | Turns a one-off value into a palette, so you can use it again anywhere. |
| **Cue look** | A colour or level for the **whole cue**: every layer of the cue that doesn't have its own. |
| **Undo / Redo** | ⌘Z takes back the last change; ⇧⌘Z puts it back. |

## Part 1 — Design it (on paper, everyone)

**The director's note:** *"The verse is too cosy for 'Everybody Scream'.
Make it feel cold and a little eerie. And when they scream, I want the
whole stage to go red."*

1. **Mood sort.** Write each colour under the mood it fits best: Amber,
   Warm White, Red, Magenta, Lavender, Blue, Cyan, Green.

   | Calm | Happy | Eerie | Angry / exciting |
   |---|---|---|---|
   | | | | |

2. **The cue sheet.** Fill in **Why**:

   | Cue | When | Why (feel) | Lights |
   |---|---|---|---|
   | 1.0.1 Verse *(change it)* | First words | | StepTops: **Lavender, 40%** (was Warm White, 75%) |
   | 1.0.3 Scream *(new)* | The first scream | | TrussW-RGB and Lanterns, the whole cue: **Red, Full** |

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet, watches the stage).
Switch roles after each part.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `lesson-1-2-start.qlcc`. (It's where
   Lesson 1.1 finished: Pose, Verse, Chorus.)
2. Click the **Show Tree** tab (⇧⌘6). Open **View → Rig Overview** (⇧⌘R)
   and put it beside the main window.

### Make the verse colder

3. In the tree, click **1.0.1 Verse**.
   - *You should see:* the risers lit warm from the step tops, the cyc
     blue.

![The warm verse](../../images/lesson-1-2/rig-verse-warm.png)

4. On the page, click the **StepTops** row (under **Layers**).
   - *You should see:* the layer's own page, *StepTops in cue 1.0.1
     Verse*, with a row for each attribute: **Colour** Warm White,
     **Dimmer** 75%.

![The layer's page](../../images/lesson-1-2/layer-page.png)

   **(1)** the Colour row and **(2)** its **⋯** button; **(3)** the Dimmer
   row and **(4)** its **⋯**.

5. On the **Colour** row, click **⋯ → Set to → Lavender**.
   - *You should see:* the row says **Lavender**, and the top right says
     **Changed: 1.0.1 Verse, 1.0.2 Chorus**. The chorus changed too:
     StepTops carries on (tracks) into it.
6. On the **Dimmer** row, click **⋯ → Level…**. A small window asks for
   the **Level (%)**. Type **40** and click **OK**.
   - *You should see:* the row says **40% · 1.0.1 Verse StepTops**, with
     **Make reusable** beside it. 40% isn't one of our palettes, so it's a
     **one-off** value, just for this layer. On the stage, the risers are
     dimmer and cool.
7. Click **Make reusable**.
   - *You should see:* the row says just **40%**. It's a palette now: in
     the Library under **Palettes → Dimmer → Show Tree → 40%**, ready to
     drag like Half or Full.

![The level and Make reusable](../../images/lesson-1-2/make-reusable.png)

   **(1)** the new colour, **(2)** the level you picked, **(3)** **Make
   reusable**, **(4)** what changed.

8. **Try Undo.** Press ⌘Z three times: the verse is warm again. Press ⇧⌘Z
   three times: it's cool again. Nothing is ever lost.

![The cool verse](../../images/lesson-1-2/rig-verse-lavender.png)

### Cue 1.0.3 — Scream

9. In the tree, click **1.0.2 Chorus**. Click **+ Cue**, type `Scream`,
   press **Return**.
   - *You should see:* **1.0.3 Scream**, with grey rows: what carries on
     from the earlier cues.
10. From **Groups → Trusses**, drag **TrussW-RGB** onto the dashed box
    under **Layers**. Then from **Groups → Lanterns**, drag **Lanterns**
    onto the dashed box too.
    - *You should see:* two new rows, **TrussW-RGB** and **Lanterns**,
      with no colour yet.
11. Drag **Red** (Palettes → Colour) onto the **dashed box** — not onto a
    row. While you drag, the top right says *"Red → cue look on 1.0.3
    Scream: reaches 2 of 2 layers"*. Then drag **Full** (Palettes →
    Dimmer) onto the dashed box the same way.
    - *You should see:* **Cue looks: Colour: Red, Dimmer: Full**. Both
      rows say **Red, Full**, and the stage floods red.

![The Scream's cue looks](../../images/lesson-1-2/cue-looks.png)

   **(1)** the cue looks, **(2)** the two layers they reach.

![The scream](../../images/lesson-1-2/rig-scream.png)

### Compare and save

12. Click **Pose**, **Verse**, **Chorus**, **Scream** in turn and watch the
    stage. Is there **contrast** between the verse and the chorus?
13. **File → Save As** → `Lesson1-2-yourname.qlcc` in **Documents →
    qlcconsole**.

## Stuck?

| Problem | Try |
|---|---|
| There are no ⋯ buttons | Click the **StepTops** row on the cue's page first: the ⋯ are on the layer's page. |
| The small Level window is behind the main window | Click the qlcconsole icon in the Dock, or press ⌘Tab. |
| Red went onto only one layer | You dropped it on a row. Undo (⌘Z) and drop it on the **dashed box**. |
| The rows in Scream say nothing after the drop | Is the cue **1.0.3 Scream** selected in the tree? |
| Something else went wrong | Undo (⌘Z), one step at a time. |

## Done early? (Extension)

1. **Pick any colour.** On the verse's StepTops **Colour** row, click
   **⋯ → Colour…**. A colour window opens: pick a colour and click **OK**.
   The row shows its code, like **#6a0dad-ish**, with **Make reusable**.
   Do you like it better than Lavender? (Undo if not.)
2. **A puzzle.** Click **1.0.0 Pose** and drag **Green** onto its dashed
   box. Read the top right: *"reaches 0 of 1 layers"*. Why didn't the cyc
   turn green? (Undo afterwards.)
3. Give the **chorus** a new mood with **⋯ → Set to** on its Steps row.
   What does it say now?
