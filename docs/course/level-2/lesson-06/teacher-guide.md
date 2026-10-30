---
title: "2.6 Teacher guide"
parent: "Lesson 2.6 — Busking with Hits"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.6 — Busking with Hits

**Teacher guide** · Level 2, lesson 6 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-6/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-2-6.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-6/">

**Builds on:** Lessons [0.6](../../level-0/lesson-06/teacher-guide.md), [0.7](../../level-0/lesson-07/teacher-guide.md) (the hit cards over a vamp), and Lesson [1.5](../../level-1/lesson-05/teacher-guide.md) (Operate and the Run tab). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say when a board op busks and why hits are built before the show;
2. build **hits** in the production's Hits section and put them on a
   **Virtual Console** page (**Make VC page**);
3. fire a hit over a running cue list in Operate and let it go.

## Before class

- [ ] [`lesson-2-6-start.qlcc`](../../shows/lesson-2-6-start.qlcc) on each
  laptop (Lesson 2.5's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View ▸ Rig
  Overview** beside it.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] A vamp to loop: a few bars of a song's intro (the same one as
  Lesson 0.7's busking practice works), or someone at a piano.

## Vocabulary (introduce in this order)

**busking** (review), **hit**, **Virtual Console**, **toggle button**.
Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Lesson 0.7's hit cards, again: why ready beforehand |
| 10–20 | **Teacher demo** (projector) | One hit; Make VC page |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–13 |
| 40–45 | **Check / share** | One group busks the loop on the projector |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Remind them of Lesson 0.7: the SM called the song map, and over a
   looped vamp the op flipped hit cards on the beat. Ask: *"What made
   the cards work?"* (They were ready before the vamp started; each had
   one clear job; you could find them without looking.)
2. That is a hit: a look built ahead of time, one button away, fired by
   feel. It doesn't go in the cue list (it isn't planned for a moment),
   and it doesn't track: it plays only what it has, on top of whatever
   the list is doing, until you let it go.
3. Name hits for what they do (*Red Stab*, *Glow*, *Faces*), so the op
   finds them in the dark.

**Key idea to land:** the cue list is the plan; hits are the op's
answers to what the plan can't know.

## 2. Teacher demo (10 min)

On the projector:

1. Right-click **Spring Show → + Hit**, name it `Red Stab`, TrussW-RGB
   Red, Full.
2. Right-click **Hits → Make VC page**; show the Virtual Console tab and
   the button. Point out: in Design, clicking it does nothing.
3. Leave the other hits and the live part for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout. In the live part, the designer calls the
  stab ("Stab… GO") and the driver fires it.
- Students without a laptop: the **design station**, the paper at-home
  task (B): hit cards over a vamp.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The VC button does nothing | Still in Design | "What mode is a show in?" |
| Can't save | In Operate | "Design first, then save." |
| The hit's layer is on a cue instead | Dropped with a cue selected | "Click the hit in the tree first." |

## 4. Check for understanding (while circulating)

- "Why doesn't the verse stop when you fire Red Stab?" (*A hit plays on
  top of the list; the list keeps running.*)
- "What happens when you click it again?" (*It goes, and what the list
  plays is back.*)

## 5. Share (5 min)

Loop the vamp. One group runs the verse in Operate and busks their hits
on the projector while the class calls when to fire.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: **Faces** fired over the Solo, and when that's right
(the soloist moved off her mark, a problem) and when it isn't (it kills
the solo's focus).

## At home

[At home](at-home.md): a fourth hit for an encore, **Make VC page** again
(the page gains a button, the others stay), fired over the song; or, on
paper, four hit cards over a vamp.

## Answer key

- **Exit ticket:** 1 — any two of: a vamp (the band repeats until the
  choir is ready), a soloist who goes long, an encore, a problem on stage
  (a missed entrance, someone off their mark). 2 — a cue is in the list
  and runs with GO, in order, and tracks; a hit is fired by hand when you
  choose, plays on top of whatever runs and never tracks. 3 — the verse
  stayed on underneath (its step tops and front light), the truss boxes
  went red; let go, the stab went and the verse was exactly as before.
  4 — one button per hit is one action in the moment, no searching; a
  name for what it does lets the op find it fast in the dark.
- **Part 1 question:** Red Stab on a drum hit or stomp in the vamp; Glow
  while the choir resets or walks; Faces when anything goes wrong or a
  soloist moves off the spot.

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoSixFiresAHit`) on the real app,
offscreen: **+ Hit** from the tree's right-click menu, the Name box, the
drops and the Center aim, **Make VC page** from the Hits row and its
top-bar message, the frame and its three buttons on the Virtual Console,
*Fired by* on the hit's page; then live, in Operate: GO to the Verse in
the Run tab, a click on **Red Stab** (the truss boxes red, the verse's
step tops still lit), a second click (the boxes dark again), Stop and
Design. The at-home fourth hit and **Make VC page** again (four buttons)
are checked the same way.
