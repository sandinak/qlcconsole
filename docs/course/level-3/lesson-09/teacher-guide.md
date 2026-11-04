---
title: "3.9 Teacher guide"
parent: "Lesson 3.9 — Troubleshooting, and the Level 3 check"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.9 — Troubleshooting, and the Level 3 check

**Teacher guide** · Level 3, lesson 9 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-9/> (the show file, the steps, the exit ticket and the at-home task; nothing else), and for the Level 3 check <https://sandinak.github.io/qlcconsole/class/3-check/>

<img src="../../images/qr/class-3-check.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-check/">

<img src="../../images/qr/class-3-9.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-9/">

**Builds on:** Lessons [3.7](../lesson-07/teacher-guide.md) and [3.8](../lesson-08/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. separate what is wrong in the saved show (Show Review) from what is wrong now (Not ready);
2. fix a finding where it is, or accept it with a note;
3. use "Why is this on?" and Solo to find a cause, and say what they cost (nothing).

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] [`lesson-3-9-start.qlcc`](../../shows/lesson-3-9-start.qlcc) on each laptop (Lesson 3.8's finish with three faults put in).
- [ ] On the projector: the start file open, the **Show Tree** tab, the **Rig Overview** beside it.
- [ ] For the check: a day on the real rig with Branson (see the [Level 3 check](level-check.md)).
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**Show Review**, **finding**, **blocker / warning / note**, **accept**, **Why is this on?**, **Not ready**, **Solo**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | 7:20, a dark light: what do you check, in what order? |
| 10–20 | **Teacher demo** (projector) | Review chip, Fix ▸, Not a Problem…; Why isn't it on?; Solo |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | Swap laptops: each pair names the three faults the other pair's file has |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Put the problem on the board with the clock: "7:20. The lanterns are dark in song 2. The house opens at 7:30." Let the class say what they would check, and write it in the order they say it. Then sort the list: what the show says (saved, true on every computer), what is true now (an output, Blackout), what this one light is doing. Land that the app has a place for each: Review, Not ready, Why is this on?

**Key idea to land:** read before you change: the show, then the light.

## 2. Teacher demo (10 min)

1. Open the start file. Click **Review**: read two rows aloud, and hover for why it matters. Show that a warning answers where it shows: hover a **⚠ cyc** mark on a cue, right-click it (**I Meant It** turns it green **cyc ✓**, **Not a Problem…** hides it, **Not Meant: Show Me the Layer**, **Open in Show Review**). A closed main curtain now stops its cyc light being flagged.
2. CUE-02: **Fix ▸**, Block at section start, **Re-check**: five findings gone.
3. VC-03: **Not a Problem…**; show the note on the Not a Problem filter.
4. Right-click a lantern → **Why is this on?** Read *Why it isn't on* top to bottom.
5. The Run tab's fader name, double-click: full. Then Solo on a layer.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching at each numbered step.
- Students without a laptop: the **design station** — Part 1 of the handout.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| No Review chip | The show only just opened | "Wait two seconds; it checks after a change." |
| **Not a Problem…** won't go | No note | "The note is required: it is the point." |
| "It's still dark" | Blackout or the GM is down | "Ask the light again: it names them." |
| They fix every note | Notes are not faults | "Notes never block anything. Choose what matters tonight." |

## 4. Check for understanding (while circulating)

- "Which of the faults was in the cue, which in the show, which in the room?" (*Block: the cue; the spare hit: the show; the group master: the rig*)
- "Why did four CUE-01s go with one Block?" (*they were one carry-over seen four times*)
- "Does Solo change the show?" (*no, it is a preview: leaving the cue turns it off*)

## 5. Share (5 min)

Pairs swap the take-home laptops from the last class's faults, if any, or name the three faults in each other's start file.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

Turn a check off for the show with a note (**Turn Check Off…**), for a rig that has no moving heads. Ask: "When is that honest and when is it hiding a problem?" Then **Follow selection** in the Why window (click along a row of lights and read each answer).

## Part 3 — On the real rig (with Branson)

The [Level 3 check](level-check.md) is here: the preshow check and a song on the real rig, with Branson. Students open the show, run Review, read the Not ready list, and fix or accept; then run the song.

## At home

[At home](at-home.md): a fault of their own in a copy, swapped, or a troubleshooting card on paper.

## Answer key

- **Exit ticket:** 1 — Show Review reads the saved show, so it is true on any computer. Not ready is about now, on this computer: an output down, a controller asleep. 2 — Accept it with a note and your name. The note is saved with the show, so the next person reads the decision and not a new warning. 3 — "Why is this on?" on the light. Under *Why it isn't on* it names the reason that blocks first: Blind, Blackout, the Grand Master, a group master at 0, a closed shutter. 4 — Solo on one layer at a time: only that layer plays. Play puts the cue back whole.

## How this lesson was checked

The course test (`ui/test/courseshow`, `levelThreeWithTheMouse` and `lessonThreeNineFindsTheFaults`) does the steps offscreen on the real app: Show Review on the start file finds CUE-02, CUE-01, VC-03 and the rest, **Block at section start** ticked, VC-03 marked Not a Problem with a note, the re-check with neither CUE-02 nor VC-03 open; "Why is this on?" on a lantern naming the group master at 0 %; the master restored and the lanterns lit; and Solo on the cyc layer (the lanterns go out) and off. It doesn't drive the Review panel's buttons, the Run tab's fader, or the ⌘I window. `levelThreePictures` (with `COURSE_SHOTS=docs/course/images`) takes this lesson's pictures, in `images/lesson-3-9/`: the Review panel, why the Lanterns aren't on, and Solo on a layer.
