---
title: "1.4 Student handout"
parent: "Lesson 1.4 — Timing: fades and holds"
grand_parent: "Level 1 — Basics"
nav_order: 2
---

# Lesson 1.4 — Timing: fades and holds

**Student handout**

## Today you will

- choose a timing for each cue, from the music;
- set fades and a hold on the cue's page;
- play it with GO and watch the timing.

## Words for today

| Word | Means |
|---|---|
| **Snap** | A change with no fade (0 seconds): a hit, a surprise. |
| **Fade** | A change that takes time: a breath. |
| **Fade in** | How long a cue takes to arrive. |
| **Fade out** | How long a cue's lights take to go away when the next cue takes over. |
| **Hold** | How long a cue waits before the next one plays **by itself**. **∞** means it waits for GO. |
| **Follow** | A cue that plays by itself after a hold. The Run tab says *follow 2s*. |

## Part 1 — Design it (on paper, everyone)

Listen to the song. For each cue, choose **snap** or a **fade** and how
many seconds, and say why.

| Cue | When | Time | Why |
|---|---|---|---|
| 1.0.0 Pose | Before the first note | **Fade in 3 s** | |
| 1.0.1 Verse | First words | **Fade in 2 s** | |
| 1.0.2 Chorus | First chorus, on the hit | **Snap** (0 s) | |
| 1.0.3 Scream | The scream | **Snap**, **hold 2 s**, **fade out 3 s** | |
| 1.0.4 Solo | After the scream | **Fade in 3 s** | |

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet and calls the times).

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `lesson-1-4-start.qlcc`. (Lesson
   1.3's finish.)
2. Click the **Show Tree** tab (⇧⌘6). Open **View → Rig Overview** (⇧⌘R)
   beside it.

### Type the times

To type a time: click in the box, select what's there (⌘A), type the
number, press **Return**.

3. Click **1.0.0 Pose**. In **Fade in**, type **3** and press **Return**.
   - *You should see:* **Fade in 3.00 s**.
4. Click **1.0.1 Verse**. **Fade in**: **2**, Return.
5. Leave **1.0.2 Chorus** at **0.00 s**: a snap.
6. Click **1.0.3 Scream**. **Hold**: **2**, Return. **Fade out**: **3**,
   Return.
   - *You should see:* **Fade in 0.00 s**, **Fade out 3.00 s**, **Hold
     2.00 s** (it said *∞ — waits for GO* before).

![The Scream's timing](../../images/lesson-1-4/cue-timing.png)

   **(1)** Fade in, **(2)** Fade out, **(3)** Hold.

7. Click **1.0.4 Solo**. **Fade in**: **3**, Return.

Clicking cues in the tree shows each one **at once**. The fades play when
you press **GO**.

### Play it

8. Click **Spring Show** in the tree (so nothing is previewed), then the
   **Run** tab (⇧⌘0). Put the Rig Overview where you can see it.
   - *You should see:* the **Timing** column: *fade 3s*, *fade 2s*, the
     chorus empty (a snap), *follow 2s*, *fade 3s*.
9. Press **GO**. Watch the cyc.
   - *You should see:* the cyc fades up to blue over three seconds. The
     line above NEXT says how far it has got: *▶ 1.0.0 Pose 42%*.

![The Run tab during a fade](../../images/lesson-1-4/run-fading.png)

   **(1)** the cue playing, and how far through its fade; **(2)** the
   Timing column.

10. **GO** again: the verse fades in over two seconds. **GO** again: the
    chorus **snaps** in.
11. **GO** for the Scream, and take your hands off.
    - *You should see:* the stage snaps red. Two seconds later the **Solo
      plays by itself**: the red melts away while the spots fade up on the
      soloist.
12. Press **Stop**. **File → Save As** → `Lesson1-4-yourname.qlcc` in
    **Documents → qlcconsole**.

## Stuck?

| Problem | Try |
|---|---|
| The box went back to 0.00 | Type the number, then press **Return** before clicking anywhere else. |
| Nothing fades when I click cues | That's right: fades play with **GO** in the Run tab. |
| GO starts in the middle | Press **Stop**, then **GO** starts from 1.0.0 again. |
| The Solo never comes by itself | Is the Scream's **Hold** 2.00 s (not ∞)? |

## Done early? (Extension)

1. Give the **chorus** a 1-second fade instead of a snap. Play it. Which is
   better with the music? Keep the one you like.
2. Play the song and press **GO** on each moment. Change the times until
   the light breathes with the music.
3. What would happen if the **Pose** had a hold of 5 seconds?
