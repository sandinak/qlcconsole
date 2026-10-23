---
title: "1.5 Teacher guide"
parent: "Lesson 1.5 — Calling and running the show"
grand_parent: "Level 1 — Basics"
nav_order: 1
---

# Lesson 1.5 — Calling and running the show

**Teacher guide** · Level 1, lesson 5 · 50 minutes · then the
[Level 1 check](level-check.md)

## Objectives

Students can:

1. call cues as a stage manager ("Standby lights cue…", "GO") and answer
   as a board operator ("Standing by");
2. run a cue list in the Run tab: **GO**, **Back**, **LOAD** a cue
   (STANDBY), jump to a cue, **Stop**;
3. switch to **Operate** for the show (LIVE) and back to **Design**.

## Before class

- [ ] [`lesson-1-5-start.qlcc`](../../shows/lesson-1-5-start.qlcc) on each
  laptop (Lesson 1.4's finish).
- [ ] On the projector: the start file open, the **Run** tab, the **Rig
  Overview** beside it.
- [ ] The song, with sound, and a **printed cue sheet** for the stage
  manager (the handout has one).
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] Plan the [Level 1 check](level-check.md): next lesson (or two).

## Vocabulary (introduce in this order)

**stage manager (SM)**, **board operator (board op)**, **call**,
**standby**, **GO**, **Back**, **LOAD**, **Stop**, **Design / Operate
(LIVE)**. Each is in the handout's word table and the
[Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Who calls, who presses; the calling drill |
| 10–20 | **Teacher demo** (projector) | The Run tab; Operate; a called run |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, then called runs with the song |
| 40–45 | **Share** | One group runs the song for the class |
| 45–50 | **Exit ticket** | On paper; hand out the Level 1 check |

## 1. Design idea (10 min)

1. In a real show, the person pressing GO doesn't decide when. The
   **stage manager** watches the stage and the script, and **calls** each
   cue; the **board operator** answers and presses GO **on the word
   "GO"**, never before.
2. The call, every time:
   - SM: **"Standby lights cue one-oh-two."** (Get ready.)
   - Op: **"Standing by."** (I'm ready.)
   - SM: **"Lights cue one-oh-two… GO."**
   - Op presses GO on "GO".
3. **The calling drill** (no computers): pairs face each other; one calls,
   one "presses" (claps) on GO. The SM tries to trick them ("Lights
   cue… *wait*…"): the op must only move on the word **GO**.

**Key idea to land:** the SM decides *when*, the op makes it *happen*.
A cue that goes early or late is as wrong as the wrong colour.

## 2. Teacher demo (10 min)

1. Open the start file. Click **Operate** at the top right: the Run tab's
   corner says **LIVE**. "In LIVE, clicking cues in the Show Tree doesn't
   change the stage any more: only GO does."
2. The Run tab, from the picture on the handout: the cue list, the
   **NEXT** line (what the next GO will change), **GO**, **Back**,
   **Stop**.
3. Have a student call three cues; you press GO on each "GO".
4. **Click** cue 1.0.4 Solo: it says **▷ STANDBY**: "that's LOAD: the
   next GO plays it, even though it isn't next." GO.
5. **Stop**. Click **Design** (top right) to go back to building.

## 3. Hands-on (20 min)

- Roles: **SM** (calls, with the cue sheet and the song),
  **Board op** (hands on the laptop), **Spotter** (watches the Rig
  Overview: did the right cue happen, on time?). Switch after each run.
- Students without a laptop: the **design station** — the calling drill,
  or start the Level 1 check's cue sheet.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| GO pressed on "Standby" | Nerves | "Only on the word GO." |
| GO does nothing | The Run tab isn't showing, or the list has ended | "Click the Run tab. At the end? **Stop**, then GO starts again." |
| "The wrong cue went" | A cue row was clicked (LOAD) by mistake | "Read NEXT / STANDBY before you GO." |
| File → Open is greyed | They're in Operate | "Click **Design** at the top right first (Stop first)." |
| A box asks "Really stop them…" | Going to Design while cues still run | "Yes is fine in class; Stop first next time." |

## 4. Check for understanding (while circulating)

- "What do you say when the SM says *standby*?" (*Standing by.*)
- "What does clicking a cue in the Run tab do?" (*LOAD: it's STANDBY, the
  next GO plays it.*)
- "Why does the Run tab say LIVE?" (*We're in Operate: the show is
  running for real.*)

## 5. Share (5 min)

One group runs the whole song for the class, called by its SM, with the
music.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below. Hand out the
[Level 1 check](level-check.md): groups bring a draft cue sheet next
time.

## Extension (fast groups)

From the handout: call the whole song with a "hold" (the SM stops a cue
with **Back**), and design a sixth cue for the ending.

**A late cue** (handout, extension 3), in Design: + Cue with 1.0.2 Chorus
selected makes **1.0.2.5 Hit**, a point cue, so the SM's 1.0.3 and 1.0.4
keep their numbers. Dragged onto the lower half of 1.0.4 Solo it becomes
**1.0.5**: a list plays in number order, so a cue's place and its number
move together. Worth saying out loud: real shows are full of point cues,
because once the SM's script or QLab has the numbers, nobody renumbers.
**Renumber this level…** (right-click a cue) is for tidying a list before
that. It lists every number that changes before you press OK; *Start* is
where the count begins (Start 1 makes 1.0.0 into 1.0.1).

## At home

[At home](at-home.md): start the Level 1 check's cue sheet.

## Answer key

- **Exit ticket:** 1 — the stage manager. 2 — "Standing by", then press
  GO on the word "GO". 3 — LOAD: it becomes **STANDBY**, the cue the next
  GO plays. 4 — LIVE (Operate): clicking cues in the Show Tree doesn't
  change the stage; only GO does.

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`lessonFiveRunsTheShow` and `lessonFiveOperate`): Operate (the Run tab
says LIVE, File → Open is greyed), GO twice, Back, a click that LOADs
1.0.2 (STANDBY), GO, a double-click that jumps to 1.0.4, Stop (the stage
goes dark, NEXT is 1.0.0 again), Design again — and that in LIVE a click
on a cue in the Show Tree leaves the stage alone. The late-cue extension is
`lessonFiveLateCue`: + Cue after the Chorus gives 1.0.2.5 Hit, the drop
note names 1.0.5, the drop makes it 1.0.5 at the end, and Undo puts it
back. The picture is taken by the same test.
