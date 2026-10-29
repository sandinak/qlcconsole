---
title: "2.3 Teacher guide"
parent: "Lesson 2.3 — Parts of a group"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.3 — Parts of a group

**Teacher guide** · Level 2, lesson 3 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-3/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-2-3.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-3/">

**Builds on:** Lessons [0.3](../../level-0/lesson-03/teacher-guide.md), [0.4](../../level-0/lesson-04/teacher-guide.md), [0.5](../../level-0/lesson-05/teacher-guide.md), and Lesson [2.1](../lesson-01/teacher-guide.md)'s Block cue. Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say why a call and response is lit in parts, and what symmetry and
   breaking it do for the audience;
2. make a **group set** and light it as one layer;
3. light part of a layer with a **child layer**, from the tree's
   right-click menu or with ⇧ as they drop.

## Before class

- [ ] [`lesson-2-3-start.qlcc`](../../shows/lesson-2-3-start.qlcc) on each
  laptop (Lesson 2.2's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View → Rig
  Overview** beside it.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] Optional: a clip of a call and response in a show choir number (one
  side sings, the other answers). Check it plays in the room first.

## Vocabulary (introduce in this order)

**symmetry**, **call and response**, **group set**, **child layer**.
Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Half the room sings, half answers; who do you watch? |
| 10–20 | **Teacher demo** (projector) | The bridge; the set; the SR child |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–12 |
| 40–45 | **Check / share** | Two groups show their bridge |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Split the class down the middle. The left half says a line ("Who's
   out there?"), the right half answers ("We are!"). Do it twice. Ask
   the front row: *"Where did your eyes go?"* (To whoever was speaking.)
2. On stage, the light does that job: it lights the half that's singing
   and holds the other back (Lesson 0.4: the eye goes to contrast). That
   means lighting **part of a group**: one side of the trusses, one row
   of the risers.
3. **Symmetry.** Both sides the same reads as balanced, formal, settled.
   Break it on purpose and the eye goes to the difference. A bridge often
   starts symmetric and then breaks it.
4. Two tools for parts:
   - a **group set** keeps both sides as one layer: one look, set once;
   - a **child layer** lights part of its parent and leaves the rest to
     it.

**Key idea to land:** build the whole first (both sides, all the steps),
then pick out the part with a child. The rest stays as the parent says.

## 2. Teacher demo (10 min)

On the projector:

1. Click **1.0.4 Solo** → **+ Cue** → `Bridge`, **Mode: Block**.
2. Library → **All → Groups → Trusses**: click **Trusses SR**, ⌘-click
   **Trusses SL**, right-click → **Make set…** → `Truss Sides`. Drag it
   onto the dashed box; **Blue**, **Full**.
3. Right-click the layer in the tree: point at **Split** greyed out (a set
   is one layer). **Add child layer → Trusses SR**, then **Amber** on it.
4. Leave the Steps and the top row for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout.
- Students without a laptop: the **design station**, the paper at-home
  task (B): a four-line bridge on a stage plan.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The set holds only one group | Only one group was selected for Make set… | "⌘-click the second group first, then right-click." |
| US is a layer of its own below Steps | ⇧ wasn't held through the drop | "Undo, and hold ⇧ until you let go. Read the top bar first." |
| The whole truss is amber | Amber dropped on Truss Sides, not the child | "Which row is the SR side?" |
| The Solo's spots are in the bridge | Mode not set to Block | "What mode is a hard start?" |

## 4. Check for understanding (while circulating)

- "What does the US child set, and what does it get from Steps?" (*It
  sets White and Full; it would get anything else from Steps, the
  parent.*)
- "Why can't you split Truss Sides?" (*A set is one layer: the set is
  what the layer is. To change one side, add a child.*)

## 5. Share (5 min)

Two groups show 1.0.4 Solo → 1.0.5 Bridge on the projector. The class
says where they looked first, second and third.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: CS and DS dropped together are one layer; **Split
layer** makes a layer each (nothing on stage changes); **More ▾ →
Merge** puts them back.

## At home

[At home](at-home.md): **1.0.6 Response**, the mirror (SL amber, the
front row picked out); or, on paper, a four-line bridge on a stage plan.

## Answer key

- **Exit ticket:** 1 — a layer on part of its parent (one member of its
  set, or a group inside it); it sets only what it sets, and the parent
  lights the rest of it (a child at 20% under a parent at full is at
  20%). 2 — one look on both sides, set once and tracking as one layer;
  the parts can still be picked out with child layers; change the set's
  members and every cue that uses it follows. 3 — both blue: balanced,
  the choir as one; SR amber: the call stands out, the eye goes to the
  side that's singing. 4 — the top row: it's the brightest and the only
  white, against dim blue (and the SR side is the warm contrast on the
  trusses).
- **Part 1 question:** the US row first (brightest, white), then the
  amber SR side.
- **Extension:** two layers when the parts will need different looks
  later; one when they always move together (less to change, and one
  thing to read in the tree).

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoThreeExtension`) on the real app,
offscreen: the Library's **Make set…** on two selected groups, the set's
drop and its hint, the tree's right-click menu (**Add child layer →
Trusses SR**; **Split** greyed on a set's layer), the ⇧-drop of US onto
the Steps row and its hint, and what the rig sends (both sides blue; SR
amber and SL blue; the top row white at full, the others dim blue). The
extension (a layer on two groups, Split, Merge) and the at-home cue
(1.0.6 Response, the mirror) are checked the same way. The finish file
is what the steps make, and the pictures are taken by the same test.
