---
layout: student
lesson: "Lesson 1.4"
title: "Timing: fades and holds"
permalink: /class/1-4/
nav_exclude: true
search_exclude: true
today: "Give each cue its timing from the music, then play the song with **GO** and watch the fades happen."
files: [lesson-1-4-start.qlcc]
app: true
words:
  - term: Snap
    means: "A change with no fade (0 seconds), for a hit or a surprise."
  - term: Fade in / fade out
    means: "How long a cue takes to arrive / how long its lights take to go when the next cue takes over."
  - term: Hold
    means: "How long a cue waits before the next one plays **on its own**. **∞** waits for GO."
  - term: Follow
    means: "A cue that plays on its own after the previous cue's hold: *follow 2s* in the Run tab."
---

## Why

Timing is part of the look. A snap lands a hit, a slow fade is a breath,
and a cue a beat late feels wrong ([Lesson 0.6](../0-6/)).

## The plan

Listen to the song. Write **why** each time fits.

| Cue | Time | Why |
|---|---|---|
| 1.0.0 Pose | Fade in 3 s | |
| 1.0.1 Verse | Fade in 2 s | |
| 1.0.2 Chorus | Snap (0 s) | |
| 1.0.3 Scream | Snap, hold 2 s, fade out 3 s | |
| 1.0.4 Solo | Fade in 3 s | |

To enter a time: click the box, **⌘A**, type the number, press
**Return**. **Esc** puts the old time back.

## Steps

1. **File → Open** → `lesson-1-4-start.qlcc`.
2. Click the **Show Tree** tab (⇧⌘5); open **View → Rig Overview** (⇧⌘R)
   beside it.
3. Click **1.0.0 Pose**; set **Fade in** to **3**.
4. Click **1.0.1 Verse**; set **Fade in** to **2**.
5. Leave **1.0.2 Chorus** at 0.00 s, so it snaps.
6. Click **1.0.3 Scream**; set **Hold** to **2** and **Fade out** to
   **3**.

   ![The Scream's timing](../../images/lesson-1-4/cue-timing.png)

7. Click **1.0.4 Solo**; set **Fade in** to **3**.
8. Click **Spring Show** in the tree, then the **Run** tab (⇧⌘8). The
   **Timing** column reads *fade 3s*, *fade 2s*, nothing, *follow 2s*,
   *fade 3s*. (Clicking cues shows them instantly; fades play only with
   GO.)
9. Press **GO**. The cyc fades up over three seconds. Close any **Switch
   to Operate** line with **×**.

   ![The Run tab during a fade](../../images/lesson-1-4/run-fading.png)

10. **GO** again: the verse fades in. **GO** once more: the chorus snaps.
11. **GO** for the Scream, then hands off. Two seconds later the Solo
    follows on its own.
12. Press **Stop**.
13. **File → Save As** → `Lesson1-4-yourname.qlcc`.

**Stuck?** GO starts in the middle: **Stop**, then GO starts from 1.0.0.
The Solo never follows: the Scream's **Hold** must be 2.00 s, not ∞.

## Exit ticket

1. What is the difference between a **snap** and a **fade**?
2. What does **Fade in 3** mean on a cue?
3. The Scream has **Hold 2**. What happens two seconds after its GO?
4. Your song opens with a slow piano introduction. What timing would you
   give the first cue, and why?

## At home

Do one:

- **App:** download [`lesson-1-4-finish.qlcc`](../../shows/lesson-1-4-finish.qlcc).
  Run it with GO along with the song, and adjust the times until the
  lighting moves with the music. Bring in `Lesson1-4-Home-yourname.qlcc`
  and a note of what you changed and why.
- **Paper:** time the first minute of a song. Pick four moments: the time
  in the song, snap or fade (how many seconds), and why.
