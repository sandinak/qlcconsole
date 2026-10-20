---
title: "1.1 Student handout"
parent: "Lesson 1.1 — From a song to cues"
grand_parent: "Level 1 — Basics"
nav_order: 2
---

# Lesson 1.1 — From a song to cues: your first three looks

**Student handout**

## Today you will

- turn a song into a list of moments (**cues**);
- build three cues in the Show Tree, each with a group of lights in a
  colour at a level;
- play your cues with **GO**.

## Words for today

| Word | Means |
|---|---|
| **Cue** | A moment in the song where the light changes. Each cue has a number (1.0.0, 1.0.1…) and a name. |
| **Cue list** | All the cues for one song, in order. |
| **Production** | The whole show: all its cue lists. |
| **Group** | A set of lights we always use together, like *Steps* or *TrussC-RGB*. |
| **Our group names** | **Trusses** are the four upright trusses across the back. **TrussC-RGB** = the colour (RGB) pixels of the boxes on the trusses that light the **C**yc; **TrussW-RGB** = the ones that **W**ash out over the stage. **Steps** = the LED strips on the front of each riser step (rows **US**, **CS**, **DS**); **StepTops** = the bars on top of the steps. |
| **Layer** | One group doing one thing in a cue: "the TrussC-RGB, blue, full". |
| **Look** | What a layer does: its **colour** and its **intensity** (level). |
| **Palette** | A saved colour or level with a name, like *Blue* or *Half*. You drag it onto a layer. |
| **Intensity** | How bright. *Full* is all the way up; *Out* is off. |
| **Cyc** | The big white cloth at the back of the stage (say "sike"). Light paints it. |
| **Track / tracked** | A light stays on in the next cue until you change it. |
| **GO** | The button that plays the next cue. |

## Part 1 — Design it (on paper, everyone)

Our song: **"Everybody Scream"**. We light its first three moments: the
**pose** before the first note (the choir frozen on the risers), the first
**verse**, and the first **chorus**.

Fill in the **Why** column — what should the audience feel?

| Cue | When | Why (feel) | Lights |
|---|---|---|---|
| 1.0.0 Pose | Before the first note | | TrussC-RGB: Blue, Full |
| 1.0.1 Verse | First words | | StepTops: Warm White, 75% |
| 1.0.2 Chorus | First chorus | | Steps: Magenta, Full |

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet, watches the stage).
Switch roles after each cue.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `lesson-1-1-start.qlcc`.
2. Click the **Show Tree** tab at the bottom (or press ⇧⌘6).
3. Open the stage: **View → Rig Overview** (⇧⌘R). Move it so you can see
   it and the Show Tree together.
   - *You should see:* a dark stage with three rows of risers, a white cyc
     at the back, trusses across the back on both sides.
   - The footer says **Not ready** in red. That's normal in class: this
     show isn't connected to real lights.

![The Show Tree tab](../../images/lesson-1-1/showtree-tab.png)

The tab has: **(1)** the buttons that make things, **(2)** the
**Library** — groups and palettes to drag, **(3)** the **tree** of your
show, **(4)** the **page** for what you clicked, **(5)** the command line
(for later), **(6)** the tabs.

### Make the song

4. Click **+ Production**. Type `Spring Show`, click **OK**.
   - *You should see:* **Spring Show** in the tree, selected.
5. Click **+ Cue list**. Type `01-Everybody Scream`, click **OK**.
   - *You should see:* **01-Everybody Scream** under Spring Show, marked *Tracking*.
   - The `01` means its cues start at **1.0.0**.

### Cue 1.0.0 — Pose

6. Click **+ Cue**. The **Name** box on the page is ready: type `Pose`
   and press **Return**.
   - *You should see:* **1.0.0 Pose** in the tree.

![A new cue](../../images/lesson-1-1/new-cue.png)

   **(1)** + Cue, **(2)** the Name box, **(3)** the dashed box under
   **Layers**, *"Drag groups or fixtures here to add a layer"* — the drop area, **(4)** your cue in the tree.

7. In the Library, open **All → Groups → Trusses**. Drag **TrussC-RGB**
   onto the dashed box under **Layers** (3).
   - While you drag, the top right says *"a new layer on TrussC-RGB in
     1.0.0 Pose"*.
   - *You should see:* a row **TrussC-RGB** under Layers.

![Dragging a group onto the cue](../../images/lesson-1-1/drop-group.png)

8. Open **All → Palettes → Colour**. Drag **Blue** onto the
   **TrussC-RGB** row. Then open **Dimmer** and drag **Full** onto the same row.
   - *You should see:* the row says **TrussC-RGB   Blue, Full**, and in the
     Rig Overview the cyc turns a clear blue, brightest at the top (the cyc
     boxes aim upward).

![Colour and level on the layer](../../images/lesson-1-1/drop-palettes.png)

   **(1)** the palette in the Library, **(2)** the layer it lands on,
   **(3)** the top right says what changed.

   **Always give a layer a colour *and* a level.** These LED lights show
   a colour at full by themselves, but lights with a dimmer (like the
   AT-W-ALL front washes, in a later lesson) stay dark with only a colour — and a level
   you chose is part of your design.

### Cue 1.0.1 — Verse

9. Click **+ Cue**, type `Verse`, press **Return**.
   - *You should see:* **1.0.1 Verse**. On its page a grey row
     **⤳ TrussC-RGB … tracked from 1.0.0 Pose** — the cyc *stays* blue.
     Lights stay on until you change them.
10. From **Groups → Steps**, drag **StepTops** onto the drop area. Drag
    **Warm White** and **75%** onto the **StepTops** row.
    - *You should see:* the risers lit warm from the step tops, the cyc
      still blue.

![The verse: a grey tracked row and a new layer](../../images/lesson-1-1/tracked-row.png)

   **(1)** the cyc, carried on from Pose (grey), **(2)** your new layer,
   **(3)** the same grey row in the tree.

### Cue 1.0.2 — Chorus

11. Click **+ Cue**, type `Chorus`, press **Return**.
12. From **Groups → Steps**, drag **Steps** onto the drop area. Drag
    **Magenta** and **Full** onto the **Steps** row.
    - *You should see:* bright magenta lines on the front of every riser,
      warm risers, blue cyc.

### Play it

Your production is already the one GO plays: the tree marks it
**▶ current**. (Don't click its **Current** button — that would turn it
off.)

13. Click the **Run** tab (⇧⌘0). Press **GO** (or **Space**) three times.
    - *You should see:* NEXT moves down the list; the stage goes Pose →
      Verse → Chorus.

![The Run tab](../../images/lesson-1-1/run-tab.png)

   **(1)** GO, **(2)** your cues, **(3)** what the next GO will change.

14. **Save** with your name: **File → Save As** → `Lesson01-yourname.qlcc`.

## What it should look like

| Pose | Verse | Chorus |
|---|---|---|
| ![Pose](../../images/lesson-1-1/rig-pose.png) | ![Verse](../../images/lesson-1-1/rig-verse.png) | ![Chorus](../../images/lesson-1-1/rig-chorus.png) |

## Stuck?

| Problem | Try |
|---|---|
| A dialog asked something you didn't expect | Cancel, then click the right row in the tree first (the production for + Cue list). |
| The group landed in the wrong cue | **Undo** (⌘Z) and click the right cue first. |
| GO does nothing | Is **Spring Show** marked **▶ current** in the tree? If not, click it and click its **Current** button once. |
| Nothing lights in the Rig Overview | Is a **cue** selected in the tree (the stage shows the selected cue)? Is **Live** on in the Rig Overview? |
| You can't find a palette | Type its name in **Filter the Library** at the top left. |

## Done early? (Extension)

1. Change the chorus so the **cyc goes magenta too**: click **1.0.2
   Chorus**, click the grey **TrussC-RGB** row, and on its Colour row use
   **⋯ → Set in this cue → Magenta**. Watch the tree: TrussC-RGB is now
   the chorus's own layer.
2. **Type it.** Open `lesson-1-1-start.qlcc` again and build the same three
   cues by typing in the **Command** line at the bottom (click it, type a
   line, press Enter):

   ```
   production "Spring Show"
   list "01-Everybody Scream"
   cue + "Pose"
   TrussC-RGB colour Blue dimmer Full
   cue + "Verse"
   StepTops colour "Warm White" dimmer 75
   cue + "Chorus"
   Steps colour Magenta dimmer Full
   ```

   Names with spaces go in quotes. `dimmer 75` finds the palette **75%**.
3. Add a fourth cue, **1.0.3 Button** — the big final hit. What would you
   light?
