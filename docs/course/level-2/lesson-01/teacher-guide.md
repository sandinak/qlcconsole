---
title: "2.1 Teacher guide"
parent: "Lesson 2.1 — Tracking and Block"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.1 — Tracking and Block

**Teacher guide** · Level 2, lesson 1 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-1/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-2-1.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-1/">

**Builds on:** Lessons [0.5](../../level-0/lesson-05/teacher-guide.md), [0.6](../../level-0/lesson-06/teacher-guide.md), [0.7](../../level-0/lesson-07/teacher-guide.md), and Lesson [1.3](../../level-1/lesson-03/teacher-guide.md)'s Block cue. Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say what a cue plays in a tracking list (its own layers and what
   tracks in), and read it in the tree (grey rows, **S / T / R / M**) and
   on the cue's page (**Tracked in**, **Next cue receives**);
2. let a layer go when its cue ends with **Release**, and say where it
   goes (the base cue, else dark);
3. choose a cue's mode: **Track**, **Block** (a hard start) or
   **Cue-only** (a moment that doesn't carry on).

## Before class

- [ ] [`lesson-2-1-start.qlcc`](../../shows/lesson-2-1-start.qlcc) on each
  laptop (Lesson 1.4's finish: the five cues of "Everybody Scream").
- [ ] On the projector: the start file open, Show Tree tab, **View → Rig
  Overview** beside it.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] Three index cards per group for the design idea (or one set for the
  board).

## Vocabulary (introduce in this order)

**tracking** (review), **tracked in**, **S / T / R / M**, **Next cue
receives**, **Release**, **hard start / Block** (review), **Cue-only**,
**base cue** (extension). Each is in the handout's word table and the
[Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Cue cards: what's on in cue 3? Two director's notes |
| 10–20 | **Teacher demo** (projector) | Read the chorus; Release the verse's step tops |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–13 |
| 40–45 | **Check / share** | Two groups play Chorus → Flash → Scream |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Write three cue cards, as a programmer would: *1: cyc blue.* *2: step
   tops lavender, front light warm.* *3: steps magenta.* Ask: *"In cue 3,
   what's on stage?"* Most say "the steps". The answer is everything:
   nothing turned the cyc, the step tops or the front light off. That's
   **tracking**: a cue list writes only the changes, and a light keeps
   its state until a cue changes it. Eos, grandMA and our Show Tree all
   work this way.
2. Why it's good: change the cyc colour in cue 1 and every cue after it
   follows, with nothing to copy. Why it's dangerous: things stay on that
   you forgot about.
3. Read out the director's two notes from the handout. For each, ask:
   *what should carry on, and what shouldn't?*
   - The lavender is the verse's idea (one look per idea, Lesson 0.5):
     it should **leave** when the verse ends.
   - The flash is a sting (Lesson 0.6): it should be **gone** on the next
     beat, whatever comes next.
4. And the Solo (Lesson 1.3) is the third case: a **hard start** from a
   dark stage, whatever came before.

**Key idea to land:** tracking is the default; Release, Cue-only and Block
are how a designer says *not this one*.

## 2. Teacher demo (10 min)

On the projector:

1. Click **1.0.2 Chorus**. Point at the grey rows under it and the
   **S 1 / T 3** on its row; hover the **T** header to show its name.
   Scroll the page down to **Tracked in** and **Next cue receives**.
2. Click **1.0.1 Verse** → its **StepTops** row → tick **Release**. Read
   the top bar aloud: *"Changed: 1.0.2 Chorus, 1.0.3 Scream"*. Click the
   chorus: the step tops are dark on the Rig Overview.
3. Leave the flash for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout.
- Students without a laptop: the **design station**, the paper at-home
  task (B): which layers are on in each cue, and which mode each needs.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| No Release box on the page | They clicked the grey StepTops row in the chorus | "Release goes on the cue that owns the layer. Click the verse, then its own StepTops row." |
| The new cue is 1.0.3.5 | The Scream was selected when they pressed + Cue | "+ Cue goes after the selected cue. Undo, select the chorus, try again." |
| The scream has white step tops | The Flash is still Track | "What mode stops a cue's changes passing on?" |
| The flash's step tops are dark | White and Full dropped on the dashed box (cue looks) with no StepTops layer, or on the wrong row | "Drop the group on the box first, then the colour and level on its row." |

## 4. Check for understanding (while circulating)

- "What does T 2 on the chorus mean?" (*Two layers track into it: the cyc
  from the Pose and the front light from the verse.*)
- "Where does a released layer go?" (*Back to the base cue, or dark if
  the base cue doesn't light it.*)
- "Why not make the Flash a Block cue?" (*Block starts from nothing: the
  chorus would go dark for the flash.*)

## 5. Share (5 min)

Two groups click **1.0.2 Chorus → 1.0.2.5 Flash → 1.0.3 Scream** on the
projector while the class calls what changes. Does the flash read as a
sting? (*Yes: it's there for one cue, and the scream starts clean of it.*)

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: the Pose as the list's **base cue**, with the step tops
at Blue, 10%, so the released step tops glow dim blue in the chorus
instead of going dark.

## At home

[At home](at-home.md): a **Final Chorus** after the Solo, made a Block
cue and lit again; or, on paper, a five-cue sheet to read for tracking
and modes.

## Answer key

- **Exit ticket:** 1 — everything that tracks in: layers set by earlier
  cues that no cue since has changed or released (unless the cue is a
  Block cue). 2 — **S** counts the layers the chorus sets itself (1: the
  Steps); **T** the layers tracking in (2: TrussC-RGB from the Pose and
  AT-W-ALL from the verse). 3 — the step tops leave when the verse ends,
  so they're dark in the chorus; the lavender is the verse's colour and
  muddied the chorus's magenta (one look per idea). 4 — (a) Block: the
  solo starts from a dark stage, a hard start, so the eye goes to the
  soloist; (b) Cue-only: the flash shows, but the next cue starts from
  what the flash started from, so it's gone on the next beat.
- **Part 1 question:** without it: the flash is a sting, there and gone,
  and the scream's red reads better without white under it.
- **At home B:** 1 — cue 3: cyc, step tops (lavender), front, steps; cue
  5: only the spot if cue 5 is Block. 2 — Cue-only; otherwise the white
  step tops track into cue 5. 3 — cue 5, the solo: a hard start. 4 — on
  the step tops in cue 2.
- **Extension:** either is a fair answer with a reason: dark gives the
  magenta steps the most contrast; a dim glow keeps the risers' shape.

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoOneExtension`) on the real app,
offscreen: the counts in the tree's S, T, R and M columns before and
after each step, the grey rows, *Next cue receives*, the top bar's
*Changed:* line, the Release and Mode boxes and the base cue, and what
the rig sends (the step tops lavender, then dark in the chorus; white in
the flash; dark again in the scream; dim blue with the base cue). The
finish file is what the steps make (`buildLevelTwoShows`), and the
pictures are taken by the same test (`levelTwoPictures`).
