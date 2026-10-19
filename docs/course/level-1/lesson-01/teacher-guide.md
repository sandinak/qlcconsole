---
title: "1.1 Teacher guide"
parent: "Lesson 1.1 — From a song to cues"
grand_parent: "Level 1 — Basics"
nav_order: 1
---

# Lesson 1.1 — From a song to cues: your first three looks

**Teacher guide** · Level 1, lesson 1 · 50 minutes

## Objectives

Students can:

1. split a song into moments and say *why* the light changes at each one;
2. build a cue list of three cues in the Show Tree, each a group with a
   colour and a level;
3. play the cue list with GO in the Run tab.

## Before class

- [ ] qlcconsole installed on the projector computer and every class laptop
  ([Install](../../install.md)).
- [ ] `lesson-01-start.qlcc` and `lesson-01-finish.qlcc` from
  [`docs/course/shows/`](../../shows/) copied to each laptop (or a shared
  folder).
- [ ] On the projector computer: open `lesson-01-start.qlcc`, Show Tree tab,
  and **View → Rig Overview** placed beside it. Bigger text helps: the
  Rig Overview window can be made full height.
- [ ] Print the [handout](handout.md) (one per student; Part 1 is
  done on paper by everyone) and the [exit ticket](exit-ticket.md).
- [ ] Optional: a 60-second clip of a show choir opener to play during the
  design idea.

## Vocabulary (introduce in this order)

**cue**, **cue list**, **production**, **group**, **layer**, **look**,
**palette**, **intensity**, **cyc**, **tracked**, **GO**. Each is defined
once in the handout's word table and in the [Glossary](../../GLOSSARY.md).
Say the word, point at it on the projector, use it in a sentence.

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Reading a song |
| 10–20 | **Teacher demo** (projector) | Build cue 1.0.0 live; the class calls the steps |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–14 |
| 40–45 | **Check / share** | Two groups GO their cues for the class |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Play the clip (or describe "Everybody Scream"). Ask: *where did the
   feeling change?* Collect answers on the board: "before they sang",
   "when they started", "the chorus — it got big".
2. Name them: those moments are **cues**. The whole song's cues in order is
   a **cue list**. All our songs together are the **production**.
3. For each moment ask *what should the audience feel?* and *which lights
   could say that?* Point at the Rig Overview: the cyc (the back wall), the
   risers, the step-top lights.
   - Pose — mystery, waiting → a coloured cyc behind dark singers
     (silhouette).
   - Verse — close, personal → warm light on the singers from the risers.
   - Chorus — big, joyful → strong colour on the risers.
4. Students fill in the **Why** column of the handout's cue sheet.

**Key idea to land:** *every cue has a reason.* We never add light just
because we can.

## 2. Teacher demo (10 min)

On the projector, with the class telling you what to click next (have the
handout up so they can read ahead):

1. **+ Production** → `Spring Show`; **+ Cue list** → `01-Everybody Scream`. Point
   out "Tracking" beside it and that the `01` numbers the cues 1.0.0, 1.0.1…
2. **+ Cue** → type `Pose`, Return.
3. Library → **All → Groups → Trusses → TrussC-RGB**: drag it onto the
   dashed box under **Layers**. Read the top-right hint aloud while dragging
   (*"a new layer on TrussC-RGB in 1.0.0 Pose"*). "A **layer** is one group
   doing one thing in this cue."
4. **Blue** then **Full** onto the row. Watch the cyc turn blue.
5. **Why a level too?** Undo (⌘Z) the Full: the cyc stays blue — these
   LED boxes have no dimmer, so a colour alone plays at full. Say: *on a
   light with a dimmer (the AT-W-ALL front washes, Lesson 1.3) a colour alone is dark,
   and the level is a design choice anyway — always set both.* Redo
   (⇧⌘Z).
6. **+ Cue** → `Verse`. Point at the grey row: **⤳ TrussC-RGB … tracked
   from 1.0.0 Pose**. "Lights stay on until you change them. That's
   **tracking**. We'll learn much more about it in Level 2."

Stop there; the students build the rest.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each cue (handout says when).
- Students without a laptop: the **design station** — the cue sheet for
  extension cue 1.0.3 *Button* (what is the final hit?), then swap.
- Walk the room with the finish picture in mind
  (`images/lesson-01/rig-chorus.png`).

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| **+ Cue list** asks nothing / does nothing | The production isn't selected | "Click Spring Show first." |
| A layer in the wrong cue | Dropped while another cue was selected | "Undo, click the right cue, drop again." |
| Stage stays dark | No cue selected (the stage shows the selected cue in Design), or the Rig Overview's **Live** is off | "Click your cue in the tree." |
| Cue numbers 1.0.0.5 or similar | + Cue pressed with an earlier cue selected | Fine for now; cue numbers never renumber. Delete and redo if it bothers them. |
| GO does nothing in the Run tab | The production was un-set (someone clicked its **▶ Current** button) | "Click Spring Show, then its Current button once." |
| Footer says **Not ready** in red | Expected: the show has no outputs | "That's because it isn't plugged into real lights — on purpose." |

## 4. Check for understanding (while circulating)

- "Point to a **layer** in your cue. What group is it? What's its look?"
- "Why is the cyc still blue in the verse? You didn't put it there."
  (*It tracked from Pose.*)
- "If the director says *I can't see faces in the verse*, which cue do
  you change?" (*1.0.1 Verse.*)

## 5. Share (5 min)

Two groups run their Run tab on the projector: three GOs. The class says
what each cue made them feel. Did it match the **Why** column?

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — four questions, answers below.

## Extension (fast groups)

From the handout: set the chorus cyc to Magenta *in this cue* (⋯ → Set in
this cue), type the whole lesson on the command line, design a fourth cue.

## At home

[At home](at-home.md): rebuild the three cues on their own computer, or
the paper version (design cue sheet for a song of their choice).

## Answer key

- **Exit ticket:** 1 — a moment where the light changes. 2 — TrussC-RGB:
  Blue, Full (one group, one colour, one level). 3 — It **tracked**: lights
  stay on until a later cue changes them. 4 — Any reasoned answer, e.g.
  "Verse: warm light on the singers, because it's close and personal."
- **Typed version** (extension 2) ends exactly like the mouse version.

## How this lesson was checked

Every step of the handout is run by the course test
(`ui/test/courseshow`, `lessonOneWithTheMouse`, `lessonOneSetInThisCue`,
`lessonOneTyped`, `lessonOnePlaysInTheRunTab`): the same buttons,
dialogs, drops and the same hint text, offscreen on the real app. The
pictures are taken by the same test.
