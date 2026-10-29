---
title: "1.4 Teacher guide"
parent: "Lesson 1.4 — Timing: fades and holds"
grand_parent: "Level 1 — Basics"
nav_order: 1
---

# Lesson 1.4 — Timing: fades and holds

**Teacher guide** · Level 1, lesson 4 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/1-4/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-1-4.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/1-4/">

**Builds on:** Lesson [0.6](../../level-0/lesson-06/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say when a **snap** works and when a **fade** works, from the music;
2. set a cue's **fade in**, **fade out** and **hold** on its page;
3. play the cue list in the Run tab and watch the timing on the stage.

## Before class

- [ ] [`lesson-1-4-start.qlcc`](../../shows/lesson-1-4-start.qlcc) on each
  laptop (Lesson 1.3's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View → Rig
  Overview** beside it.
- [ ] The song, or any two contrasting clips (a slow ballad opening and a
  big choral hit), with sound.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**snap**, **fade**, **fade in**, **fade out**, **hold**, **follow**. Each
is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Counting the music; a snap vs a fade |
| 10–20 | **Teacher demo** (projector) | Fade in on the Pose; GO in the Run tab |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–12 |
| 40–45 | **Check / share** | Two groups play their list to the music |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. With the lights in the room: switch them off **at once** (a snap), then
   back on. Then dim them slowly, if you can, or turn them off one bank at
   a time. *"Which one felt like a surprise? Which one like a breath?"*
2. **A snap is a hit; a slow fade is a breath.** The light's timing
   follows the music:
   - a **chorus** that starts on a big hit → a snap (0 seconds);
   - a **verse** that eases in → a fade of 2–3 seconds;
   - the very start of a number → a slow fade up while the choir holds its
     pose.
3. Play the song (or the two clips). Students count the seconds of each
   change with their fingers: "how long would this fade be?"
4. The cue sheet on the handout gets a **Time** column.

**Key idea to land:** the timing is part of the design. Listen to the
music, then choose.

## 2. Teacher demo (10 min)

1. Click **1.0.0 Pose**. On its page, click in **Fade in**, select what's
   there, type **3**, press **Return**.
2. "Clicking cues in the tree shows each one at once. Fades play when you
   press **GO**." Click the production, so nothing is previewed.
3. **Run** tab (⇧⌘8), **GO**: the cyc fades up over three seconds. Point
   at the line above NEXT: *▶ 1.0.0 Pose 42%* — how far through the fade
   it is. Point at the **Timing** column: *fade 3s*.
4. **Stop**.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; the
  Designer calls out the times.
- Students without a laptop: the **design station** — time the changes in
  the song with a stopwatch and fill in the Time column for a sixth cue
  of their own; then swap.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The number went back to its old time | They pressed **Esc**, which puts it back | "Type it again; **Return**, Tab or clicking the next cue keeps it." |
| "My fade doesn't fade!" | They are clicking cues in the tree | "Fades play with **GO** in the Run tab." |
| The time went on the wrong cue | Another cue was selected | "Check the **Name** box at the top of the page." |
| The Solo came without GO | Expected: the Scream's **Hold** is 2 s | "That's the hold: it follows on by itself." |
| Hold shows **∞ — waits for GO** | The default | "Type 2 to make it follow on." |

## 4. Check for understanding (while circulating)

- "Why is the chorus a snap?" (*It starts on a big hit.*)
- "What does the Scream's hold do?" (*After two seconds, the next cue
  plays by itself.*)
- "What does the Scream's fade out do?" (*The red takes three seconds to
  go away while the solo fades up.*)

## 5. Share (5 min)

Two groups play their list from the Run tab **with the song**: GO on each
moment. Does the timing feel right with the music?

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: try a 1-second fade on the chorus and decide which is
better; time the whole list to the song.

## At home

[At home](at-home.md): time a song you like, on a computer or on paper.

## Answer key

- **Exit ticket:** 1 — a snap: a sudden change, a hit; a fade: a smooth
  change over time, a breath. 2 — how long the cue takes to arrive. 3 —
  the next cue (the Solo) plays by itself after 2 seconds, without GO.
  4 — any reasoned answer that matches the music (e.g. "a slow fade, it's
  a ballad").

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`lessonsOneTwoToOneFourWithTheMouse` and `lessonFourPlaysTheFades`): the
times typed into each cue's Fade in, Fade out and Hold boxes, then GO in
the Run tab — the cyc half way after a second, full after three; the
Scream following on to the Solo by itself after its hold, the red fading
out as the spots fade up. It also checks that a click on a cue in the
tree shows it at once. The pictures are taken by the same test.
