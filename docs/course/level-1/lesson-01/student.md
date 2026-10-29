---
layout: student
lesson: "Lesson 1.1"
title: "From a song to cues"
permalink: /class/1-1/
nav_exclude: true
search_exclude: true
today: "Program the first three cues of \"Everybody Scream\" in the Show Tree and play them with **GO**."
files: [lesson-1-1-start.qlcc]
app: true
web: true
words:
  - term: Cue
    means: "A planned lighting change, tied to a moment in the song. It has a number (1.0.0) and a name."
  - term: Cue list
    means: "All of one song's cues, in order."
  - term: Production
    means: "The whole show: every cue list in it."
  - term: Group
    means: "A named set of lights used together, like *Steps* or *TrussC-RGB* (the truss pixels that light the cyc)."
  - term: Layer
    means: "One group doing one thing in a cue: \"TrussC-RGB, Blue, Full\"."
  - term: Palette
    means: "A saved colour or level, like *Blue* or *Half*, that you drag onto a layer."
  - term: Cyc
    means: "The white cloth across the back of the stage, lit with colour. Said \"sike\"."
  - term: Tracking
    means: "A light holds its state into the next cues until a cue changes it."
  - term: GO
    means: "Plays the next cue."
---

## Why

A song has sections, and each should feel different
([Lesson 0.5](../0-5/)). A cue is how that change reaches the stage, and
each cue is built in layers, one group doing one job
([Lesson 0.3](../0-3/)). Today's three cues are the first three rows of
a song map.

## The plan

Fill in **Why** on paper first: what should the audience feel?

| Cue | When | Why | Lights |
|---|---|---|---|
| 1.0.0 Pose | Before the first note | | TrussC-RGB: Blue, Full |
| 1.0.1 Verse | First words | | StepTops: Warm White, 75% |
| 1.0.2 Chorus | First chorus | | Steps: Magenta, Full |

Roles: **Driver** (mouse), **Navigator** (reads each step), **Designer**
(watches the stage). Switch after each cue.

## Steps

1. Open qlcconsole: **File → Open** → `lesson-1-1-start.qlcc`.
2. Click the **Show Tree** tab (⇧⌘5).
3. **View → Rig Overview** (⇧⌘R), beside the Show Tree. The footer reads
   *No outputs: preview only*: expected in class.

   ![The Show Tree tab](../../images/lesson-1-1/showtree-tab.png)

4. **+ Production**, type `Spring Show`, **OK**.
5. **+ Cue list**, type `01-Everybody Scream`, **OK**.
6. **+ Cue**, type `Pose`, press **Return**: **1.0.0 Pose**.
7. In the Library, open **All → Groups → Trusses** and drag
   **TrussC-RGB** onto the dashed box under **Layers**. The note in the
   top bar says where the drop lands: read it before you let go.

   ![Dragging a group onto the cue](../../images/lesson-1-1/drop-group.png)

8. From **Palettes → Colour**, drag **Blue** onto the TrussC-RGB row;
   from **Dimmer**, drag **Full** onto it. The cyc turns blue. Always give
   a layer a colour *and* a level.

   ![Colour and level on the layer](../../images/lesson-1-1/drop-palettes.png)

9. **+ Cue**, `Verse`, **Return**. The grey row is the cyc, tracked from
   Pose.
10. From **Groups → Steps**, drag **StepTops** onto the dashed box, then
    **Warm White** and **75%** onto its row.

    ![The verse: a tracked row and a new layer](../../images/lesson-1-1/tracked-row.png)

11. **+ Cue**, `Chorus`, **Return**.
12. Drag **Steps** onto the dashed box, then **Magenta** and **Full** onto
    its row.
13. Click the **Run** tab (⇧⌘8) and press **GO** (or Space) three times.
    If a line offers **Switch to Operate**, close it with **×**.

    ![The Run tab](../../images/lesson-1-1/run-tab.png)

14. **File → Save As** → `Lesson01-yourname.qlcc`.

| Pose | Verse | Chorus |
|---|---|---|
| ![Pose](../../images/lesson-1-1/rig-pose.png) | ![Verse](../../images/lesson-1-1/rig-verse.png) | ![Chorus](../../images/lesson-1-1/rig-chorus.png) |

**Stuck?** Dropped in the wrong cue: **⌘Z**, click the right cue, drag
again. Can't find a palette: type its name in **Filter the Library**.
Stage dark: select a **cue** in the tree.

## Exit ticket

1. Explain what a **cue** is, as you would to someone new to the crew.
2. Cue 1.0.0 Pose has one **layer**. Write it the way the Show Tree shows
   it (group, colour, level).
3. You didn't touch the cyc in 1.0.1 Verse, yet it was still blue. Why?
4. Pick one cue. What should the audience feel, and how does the lighting
   get them there?

## At home

Do one:

- **App:** build the three cues again from `lesson-1-1-start.qlcc`
  without the pictures, then recolour every cue for a different story (a
  ballad, a horror number). Play them with GO and bring in
  `Lesson01-Home-yourname.qlcc`.
- **Browser:** the same in the practice version, then **Save to file** and
  hand the file in.
- **Paper:** a cue sheet of 3–5 cues for the first minute of a song you
  choose: when, what the audience should feel, and the lights (group:
  colour, level).
