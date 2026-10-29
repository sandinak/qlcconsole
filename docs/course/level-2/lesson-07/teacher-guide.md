---
title: "2.7 Teacher guide"
parent: "Lesson 2.7 — Sub-cues: stomps inside a cue"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.7 — Sub-cues: stomps inside a cue

**Teacher guide** · Level 2, lesson 7 · 50 minutes · then the
[Level 2 check](level-check.md)

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-7/> (the show file, the steps, the exit ticket and the at-home task; nothing else), and for the Level 2 check <https://sandinak.github.io/qlcconsole/class/2-check/>

<img src="../../images/qr/class-2-7.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-7/"> <img src="../../images/qr/class-2-check.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-check/">

**Builds on:** Lesson [0.6](../../level-0/lesson-06/teacher-guide.md) (stings), Lesson [2.1](../lesson-01/teacher-guide.md) (cue-only) and [1.5](../../level-1/lesson-05/teacher-guide.md) (the Run tab). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say why a sting inside a section is a sub-cue, not a cue of its own;
2. add **sub-cues** to a cue and set each one's **Advance** (**Sub GO**,
   or **auto-follow** after a wait);
3. play a cue's sub-cues with **GO** and **Sub GO**, and say why the next
   cue doesn't inherit them.

## Before class

- [ ] [`lesson-2-7-start.qlcc`](../../shows/lesson-2-7-start.qlcc) on each
  laptop (Lesson 2.6's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View → Rig
  Overview** beside it.
- [ ] Print the [handout](handout.md), the [exit ticket](exit-ticket.md)
  and the [Level 2 check](level-check.md).
- [ ] A song clip with a clear stomp or clap in its chorus.
- [ ] Plan the Level 2 check: the next two or three periods; songs to
  assign.

## Vocabulary (introduce in this order)

**sub-cue**, **Sub GO**, **auto-follow**, **advance**. Each is in the
handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Stomp along: a sting inside the chorus |
| 10–20 | **Teacher demo** (projector) | The first sub-cue and Advance |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–13 |
| 40–45 | **Level 2 check** | Hand out the task and rubric; groups choose songs |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Play the clip; the class stomps on the stomp. Ask: *"Is the stomp a
   new section?"* (No: the chorus goes on.) *"Is it a lighting moment?"*
   (Yes: a sting, Lesson 0.6.)
2. Three ways to light it, and what's wrong with two of them:
   - a cue of its own after the chorus: the chorus's look would have to
     be rebuilt after it, and what it sets tracks on (Lesson 2.1);
   - a hit (Lesson 2.6): fine for the unplanned, but the stomp is in the
     score every night;
   - a **sub-cue**: inside the chorus, fired with **Sub GO** on the beat,
     and what it changes doesn't pass on.
3. The end of a stomp doesn't need the op: it can **follow by itself**
   half a second later.

**Key idea to land:** GO moves the song on; Sub GO lands the accents
inside a section.

## 2. Teacher demo (10 min)

On the projector:

1. Click **1.0.2 Chorus** → **Sub-cues: + Sub-cue**. Name it `Chorus`;
   point out it has no layers: it's the chorus as it is, and it plays
   with GO.
2. Show the **Advance** row on a sub-cue page.
3. Leave the stomp and the playing for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout. In Play it, the designer stomps and the
  driver presses Sub GO on it.
- Students without a laptop: the **design station**, the paper at-home
  task (B): a chorus written as a cue and its sub-cues.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| A sub-cue under the wrong cue | Another cue selected for + Sub-cue | "It goes under the selected cue. Undo and select the chorus." |
| Sub GO does nothing | The chorus isn't playing | "GO the chorus first." |
| The lanterns stay on after the stomp | Stomp Out has no Out, or no auto-follow | "What ends the stomp?" |

## 4. Check for understanding (while circulating)

- "Why don't the lanterns come into the Flash?" (*Sub-cues are
  cue-only: the Flash starts from the chorus's own state.*)
- "Why is 1.0.2.1 empty?" (*The first sub-cue plays with GO; the stomp
  must wait for its beat.*)

## 5. The Level 2 check (5 min)

Hand out the [Level 2 check](level-check.md) and its rubric. Groups pick
or are given a song; the at-home task starts the song map.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: a second stomp, **1.0.2.4**, Red, on Sub GO.

## At home

[At home](at-home.md): Sub GO on the beat of a song, and Stomp Out's wait
tried at 0.25 s; or, on paper, a chorus and its sub-cues. Plus the Level
2 check's song map.

## Answer key

- **Exit ticket:** 1 — GO goes on to the next main cue (the song moves
  on); Sub GO steps to the next sub-cue of the cue that's playing (an
  accent inside the section). 2 — the first sub-cue plays with GO, so
  GO into the chorus played it; that's why it holds nothing new. 3 — it
  goes on by itself half a second after the stomp has faded in: the
  stomp's end needs no one's timing, so the op's hands are free for the
  next one. 4 — what the sub-cues change doesn't pass on: the next cue
  starts from the chorus's own state (cue-only, Lesson 2.1).
- **Part 1 question:** the stomp has to land on the beat, which only the
  op can do live; its end is a fixed time later, so it can follow by
  itself.

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoSevenPlaysTheStomps`,
`lessonTwoSevenExtension`) on the real app, offscreen: the **+ Sub-cue**
link on the chorus's page and **More ▾ → + Sub-cue**, the numbers
1.0.2.1–1.0.2.3 (and 1.0.2.4 for the extension), the names, the drops,
**Advance: Auto-follow after 0.5 s**; then in the Run tab: the chorus
loaded and GO (the lanterns dark), **Sub GO** (Lantern 1 at full), Stomp
Out by itself (dark again), and GO to 1.0.2.5 Flash (still dark). The
Level 2 check uses only steps checked in Lessons 1.1–2.7.
