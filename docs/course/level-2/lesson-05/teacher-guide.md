---
title: "2.5 Teacher guide"
parent: "Lesson 2.5 — Moving lights and lines"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.5 — Moving lights and lines

**Teacher guide** · Level 2, lesson 5 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-5/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-2-5.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-5/">

**Builds on:** Lessons [0.2](../../level-0/lesson-02/teacher-guide.md), [0.3](../../level-0/lesson-03/teacher-guide.md), [0.6](../../level-0/lesson-06/teacher-guide.md), and Lesson [1.3](../../level-1/lesson-03/teacher-guide.md)'s stage targets. Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say why beams are scenery and what makes a strong button;
2. aim moving lights at a **line target** and set its **Spread** (+5,
   0, −5), reading the order strip and the plan view;
3. build a button that tracks from the cue before and sweeps into it
   over a short fade.

## Before class

- [ ] [`lesson-2-5-start.qlcc`](../../shows/lesson-2-5-start.qlcc) on each
  laptop (Lesson 2.4's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View ▸ Rig
  Overview** beside it, **Room: Dim**.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] Optional: four torches, for the design idea.

## Vocabulary (introduce in this order)

**line target**, **spread**, **fan / cross**, **order strip / plan
view**, **colour wheel / colour ring**. Each is in the handout's word
table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Four torches: a fan, a point, a cross |
| 10–20 | **Teacher demo** (projector) | The fan on the Riser Line |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–11 |
| 40–45 | **Check / share** | Two groups play Fan → Button |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Four volunteers stand in a row with torches (the truss spots), the
   room dim, a strip of tape across the floor (the line). Ask them to
   light the tape end to end, evenly: a **fan**, Spread +5.
2. Now all on the middle of the tape: Spread 0, a hot spot. Now each to
   the other side of the middle: the beams **cross**, Spread −5.
3. Ask: *"Which of these says 'the end'?"* Most pick the cross or the
   point. That's the **button** (Lesson 0.6): the last note, a clear
   final picture that tells the audience to applaud.
4. A snap or a sweep? A snap lands on the note; a short sweep (about a
   second) is movement the audience feels as part of the ending. Both
   are fair; say why.

**Key idea to land:** a line target spreads the lights along it; Spread
says how. Beams are scenery, so their shape is a design choice.

## 2. Teacher demo (10 min)

On the projector:

1. **1.0.6 Dance Break → + Cue** `Fan`, **Mode: Block**. Drop
   **Truss-Spots**, **White**, **Full**: the spots point where they rest.
2. Drop **Riser Line** on the row: the fan. Click the row: show the
   order strip and the plan view, then the **Spread** row's **⋯** menu
   without choosing.
3. Leave the button for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout.
- Students without a laptop: the **design station**, the paper at-home
  task (B): the three spreads drawn from above.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The fan crossed | −5 set in the Fan, not the Button | "Which cue is the button?" |
| No Spread row | The aim isn't on a line | "What does the Position row say?" |
| The button is white | Red dropped on the Fan | "Undo; click 1.0.8 first." |

## 4. Check for understanding (while circulating)

- "Why does the Button start with a grey Truss-Spots row?" (*It tracks
  from the Fan: same lights, same aim, same colour, until we change
  them.*)
- "What's different about colour on these spots?" (*No mixing: a wheel
  and a ring of named slots; Red picks the nearest.*)

## 5. Share (5 min)

Two groups play it in the Run tab: click **1.0.7 Fan** to load it, **GO**,
**GO**. Does the ending land?

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: **Spread 0** (a single hot spot) and **Grouping:
Pairs**.

## At home

[At home](at-home.md): Spread 0, +2.5 and Pairs on the fan, and the one
they'd use; or, on paper, the three spreads drawn from above.

## Answer key

- **Exit ticket:** 1 — it spreads them: each light gets its own point
  along the line, in order of where they hang, instead of all meeting
  at one point. 2 — +5: evenly end to end (a fan); 0: all on the centre
  (one hot spot); −5: each on the other side of the centre, so the
  beams cross. 3 — the sweep is part of the ending: the audience sees
  the beams travel into the cross on the last note; a snap would just
  appear. (A snap is also a fair choice, with a reason.) 4 — each turned
  to its slot nearest red: the spots don't mix colour; a wheel and a
  ring have named colours only.
- **Part 1 question:** the movement of the sweep reads as the music's
  last gesture; a snap is the other good choice.

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoFiveExtension`) on the real app,
offscreen: the drops and their hints, the Riser Line drop, the rows under
Position (Aim Riser Line, the order strip, the plan view, *Spread: +5 —
the Aim look's*), the grey row's **Spread ⋯ → −5** and **Changed: 1.0.8
Button**, the Fade in typed, and what the spots send: their pan and tilt
before the aim, on the fan and crossed; their colour wheel and colour
ring moving from White's slots to Red's. The extension (Spread 0, Pairs)
is checked the same way.

**Not in this lesson, and why:** the plan also put the upstage beams
(USTs) on the line. On qlcconsole 0.3.x, aimed at the Riser Line they
send tilt 0 and their beams go out over the house, so the lesson uses
the truss spots. See the
[course plan](https://github.com/sandinak/qlcconsole/blob/main/docs/course/COURSE_PLAN.md#found-writing-level-2-on-030).
