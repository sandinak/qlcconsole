---
layout: student
lesson: "Lesson 2.7"
title: "Sub-cues: stomps inside a cue"
permalink: /class/2-7/
nav_exclude: true
search_exclude: true
today: "Put a stomp inside the chorus: sub-cues taken with Sub GO and one that follows by itself, then play them in the Run tab."
files: [lesson-2-7-start.qlcc]
app: true
words:
  - term: Sub-cue
    means: "A cue inside a cue (1.0.2.1, 1.0.2.2…) for hits within a section. What it changes doesn't pass on."
  - term: Sub GO
    means: "Steps to the next sub-cue of the cue playing (⌥⌘↩). GO still goes to the next cue."
  - term: Auto-follow
    means: "A sub-cue that goes on by itself, a set time after the one before."
---

## Why

A chorus has stomps and claps inside it, and the light can land on each,
a sting ([Lesson 0.6](../0-6/)). The chorus goes on, and what comes after
shouldn't inherit the stomp: a sub-cue fires on the beat and doesn't
pass on (cue-only, [Lesson 2.1](../2-1/)).

## The plan

| Sub-cue | When | Advance | Lights |
|---|---|---|---|
| 1.0.2.1 Chorus | Plays with GO | — | nothing new |
| 1.0.2.2 Stomp | The stomp | **Sub GO** | **Lanterns**: White, Full |
| 1.0.2.3 Stomp Out | 0.5 s later | **Auto-follow** | **Lanterns**: Out |

Why is the stomp a Sub GO, and its end automatic?

## Steps

1. **File ▸ Open** → `lesson-2-7-start.qlcc`.
2. Click the **Show Tree** tab (⇧⌘5); open **View ▸ Rig Overview** (⇧⌘R)
   beside it.
3. Click **1.0.2 Chorus**; in its **Sub-cues** row click **+ Sub-cue**.
   Name it `Chorus`, **Return**.
4. **More ▾ → + Sub-cue**: `Stomp`. Drag **Lanterns** onto the dashed
   box; **White** and **Full** onto its row.
5. **More ▾ → + Sub-cue**: `Stomp Out`. Drag **Out** onto its Lanterns
   row.
6. **Advance**: **Auto-follow after**, type `0.5`, **Return**.

   ![Stomp Out](../../images/lesson-2-7/stomp-out.png)

7. **File ▸ Save As** → `Lesson2-7-yourname.qlcc`.
8. **Run** tab (⇧⌘8): click **1.0.2 Chorus**, press **GO**.
9. Press **Sub GO** on the stomp: the lanterns flash, then go out by
   themselves.

   {% include inapp.html do="Sub GO" path="Control ▸ Sub GO (Main Cue List)" mac="⌥⌘↩" win="Ctrl+Alt+Enter" see="Or the Run tab's **Sub GO** button. **GO** still goes on to the next main cue." %}

   ![Sub GO](../../images/lesson-2-7/run-subgo.png)

10. Press **GO**: the Flash, lanterns dark. Then **Stop**.

**Stuck?** Wrong place: a sub-cue goes under the selected cue; undo and
select the chorus. No Advance row: click the sub-cue, not the chorus.
Sub GO does nothing: GO the chorus first.

## Exit ticket

1. What's the difference between **GO** and **Sub GO**?
2. Why did **1.0.2.1 Chorus** play without Sub GO?
3. Stomp Out is **auto 0.5s**. What does that do, and why not a Sub GO?
4. Why don't the lanterns come into the Flash?

## At home

Do one, and start your [Level 2 check](../2-check/) song map:

- **App:** download [`lesson-2-7-finish.qlcc`](../../shows/lesson-2-7-finish.qlcc).
  Load and GO the chorus, press **Sub GO** on the beat of a song you
  know. Change Stomp Out's wait to `0.25`: better or worse? Save as
  `Lesson2-7-Home-yourname.qlcc`.
- **Paper:** write a chorus you know as a cue and its sub-cues: the
  moment, the lights, Sub GO or auto (and how long).
