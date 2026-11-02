---
title: "3.8 Student handout"
parent: "Lesson 3.8 — Running a real show"
grand_parent: "Level 3 — Advanced"
nav_order: 2
---

# Lesson 3.8 — Running a real show

**Student handout**

## Why this matters

A show is one GO key and a dozen songs. The op can't stop between numbers to rebuild a list, so every song lives in the same list, and the one before mustn't leave its colours on the next ([Lesson 2.1](../2-1/)). Then the night adds what the plan didn't: the lanterns are too hot for the room, the soloist forgets the words and needs a quieter stage, and the stage manager wants the whole thing on paper ([Lesson 0.7](../0-7/)).

## Today you will

- turn the song into a section of a show and add a second song that starts from nothing;
- give the operator a group master and an override hit, and use them over a running cue;
- read the cue sheet and the tracking sheet the show makes.

## Words for today

| Word | Means |
|---|---|
| **Section** | One song of the show's one cue list: its cues, under a header (**1 — Everybody Scream**). The cue numbers decide where a section starts. |
| **Block at section start** | The song's first cue starts from nothing, so nothing of the song before carries in. |
| **Group master** | A fader for one group's intensity. At half, the group's dimmers give half of what the cues say, all night, with no cue edited. |
| **Override hit** | A hit that wins over everything the show plays, brightness too: it can pull a light down as well as up. Let go and the show is back. |
| **Cue sheet / tracking sheet** | The stage manager's paper, made from the show: every cue in running order, and every layer across the cues with what each cue changes. |

## Part 1 — Plan it (on paper, everyone)

A show of three songs. For each, write what the last cue leaves on stage and what the next song's first cue needs to be. Which light would you want a fader for, on a night the room was small? What one button would you want over a song that goes wrong?

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks *You should see*), **Designer** (holds the sheet). Switch every few steps.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

1. **File ▸ Open** → `lesson-3-8-start.qlcc` (Lesson 3.5's finish). Open
   the **Show Tree** tab (⇧⌘5) and **View ▸ Rig Overview** (⇧⌘R).
2. Click **Spring Show**. In **Numbering**, pick **Show.Part.Iteration**.
   - *You should see:* the song's cues fold under a header, **1**.
3. Click that header. In **Name**, type `Everybody Scream` and press
   **Return**.
4. Click the list **01-Everybody Scream**, then **+ Section**. Name it
   `Slow Dance`, **OK**.
   - *You should see:* a second header, **2 — Slow Dance**, and a new cue
     **2.0.1**.
5. Click the **Command** line and type each line, then **Return**:

   ```
   name "Slow Dance"
   TrussC-RGB colour Blue dimmer Half
   Lanterns colour Amber dimmer Full
   cue + "Slow Dance Out"
   TrussC-RGB dimmer Out
   Lanterns dimmer Out
   ```

6. Click the header **2 — Slow Dance** and tick **Block at section start**
   (it may be ticked already). Click **1.0.8 Button**, then **2.0.1**, in
   the Show Tree. The Button's red and the Fan's beams stay in song 1: the
   Rig Overview shows none of them in 2.0.1.
7. **Fixtures** tab (⇧⌘2): click the group **Lanterns** and tick **Group
   master**.
8. Back in the Show Tree, type these lines. The hit is **Hush**, and
   *override* is the word that makes it one:

   ```
   production "Spring Show"
   hit "Hush"
   Lanterns dimmer 25%
   override
   vcpage
   ```

   - *You should see:* **Hush** marked **override** in the tree, and a
     button for it on the Virtual Console (⇧⌘6).
9. **File ▸ Save As** → `Lesson3-8-yourname.qlcc`.
10. Click **Operate**. In the **Run** tab (⇧⌘8), double-click **1.0.8 Button**: the
    list jumps there. Then **GO**.
    - *You should see:* **2.0.1 Slow Dance**: blue cyc, amber lanterns,
      and none of the Button's red.
11. Beside the cue list is a fader for **Lanterns**. Drag it to half. The
    lanterns drop to half and the cues don't change. Double-click its name
    to put it back to full.
12. **Virtual Console**: click **Hush**. The lanterns drop to a quarter
    whatever the cue says. Click it again: back to full.
13. **Run** tab: **Stop**, then **Design**.
14. In the Show Tree, **More ▾ → Cue Sheet…**. The **Sheet** box has two
    sheets. Read the **Cue sheet** (the cues by song), then the **Tracking
    sheet**. Save one as a PDF with **Save PDF…**.

**Stuck?** No **Numbering** box: click the production, not the list. **+
Section** greyed: pick a **Numbering** first. No Group master on the Run
tab: tick it on the group in the Fixtures tab. **Hush** does nothing:
you are in Design. Click **Operate**.

## The night's checklist

On paper, before the house opens: the show open and saved; **Operate**; the
Run tab on the first cue; Stage Power and the house lights as in
[Lesson 3.7](../3-7/); every group master at full; the pad, phone and QLab
as the plan says; a hand on **Blackout** (top right) and **Stop all**
(⌘⇧Esc). Blackout takes every light that doesn't ignore it to zero, and
**Stop all** lets go of every cue and hit. Neither is an edit.

## Done early? (Extension)

**Running order…** (on the production): drag the songs into tonight's order, or untick one to leave it out; GO passes over it. Then ask why the song you moved should be Block. (*It tracks from the song before it by number, not by running order.*) And the **phone Run page** has a slider for each group master, under the cues; ask who should be allowed to touch it.
