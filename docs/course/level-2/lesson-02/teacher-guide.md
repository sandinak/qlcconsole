---
title: "2.2 Teacher guide"
parent: "Lesson 2.2 — Palettes: change once, change everywhere"
grand_parent: "Level 2 — Intermediate"
nav_order: 1
---

# Lesson 2.2 — Palettes: change once, change everywhere

**Teacher guide** · Level 2, lesson 2 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/2-2/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-2-2.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/2-2/">

**Builds on:** Lessons [0.4](../../level-0/lesson-04/teacher-guide.md), [0.5](../../level-0/lesson-05/teacher-guide.md), and Lesson [1.2](../../level-1/lesson-02/teacher-guide.md) (Make reusable). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. explain why a show is built from palettes: a note is one change, and
   every cue that uses the palette follows;
2. find where a palette is used (**Used by**) before changing it;
3. make a palette of the song's own (**Duplicate**, name it for its job
   in the Look Editor, file it in a song folder) and change it once in
   the **Look Editor**.

## Before class

- [ ] [`lesson-2-2-start.qlcc`](../../shows/lesson-2-2-start.qlcc) on each
  laptop (Lesson 2.1's finish).
- [ ] On the projector: the start file open, Show Tree tab, **View → Rig
  Overview** beside it.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).
- [ ] Optional: paint chips or gel swatches, magenta to violet.

## Vocabulary (introduce in this order)

**palette** (review), **Used by**, **duplicate**, **Look Editor**,
**palette folder**. Each is in the handout's word table and the
[Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | "Less pink", an hour before the show |
| 10–20 | **Teacher demo** (projector) | Used by; Duplicate; the song folder |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2, steps 1–13 |
| 40–45 | **Check / share** | One group takes a new note live |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

1. Set the scene: final dress, an hour before the doors. The director
   says *"the chorus is too pink."* Ask: *"How many cues have that pink
   in them?"* Count them on the board for a real show: every chorus,
   the final chorus, the bows, maybe the cyc too. A dozen places to edit,
   under pressure, and one will be missed.
2. A **palette** is a name for a value. If every one of those places uses
   the palette *Chorus Colour*, the note is one change. That's how
   professional shows are built: the cue list says *which* colour, the
   palette says *what* that colour is.
3. Two habits that make it work:
   - **A palette per song.** The show's own *Magenta* might be in other
     songs; change it and you've changed them too. The song gets copies
     of its own, in a folder named for the song.
   - **Name it for its job.** *Chorus Colour* stays true after the note;
     *Chorus Pink* would be a lie by tonight.
4. Optional: hold up a magenta chip and a violet one. *Which says "big
   hook"? Which says "cooler, more mysterious"?* (Lesson 0.4.)

**Key idea to land:** the cue list holds the plan, the palettes hold the
values. Notes change values.

## 2. Teacher demo (10 min)

On the projector:

1. Library → **All → Palettes → Colour** → right-click **Magenta** →
   **Used by**: one use, the chorus's Steps. Click it.
2. Right-click **Magenta** → **Duplicate**. Point at the Look Editor
   under the page: *Used by: nothing yet*. Type `Chorus Colour` in Name.
3. Right-click it → **Move to folder → New folder…** →
   `01-Everybody Scream`.
4. Leave using it and the note for the students.

## 3. Hands-on (20 min)

- Groups of 2–3 with **Driver / Navigator / Designer** roles; switch after
  each part of the handout.
- Students without a laptop: the **design station**, the paper at-home
  task (B): a palette plan for a song, by job.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| Magenta changed in the Library | They opened Magenta, not the copy | "Undo. Which palette is on the chorus now? Open that one." |
| Only the steps changed | Chorus Colour was dropped on the Steps only | "Where else is the chorus colour? The cyc: drop it on the grey TrussC-RGB row." |
| The copy is still *Magenta (copy)* | Return not pressed in Name | "Type the name and press Return." |
| They can't find Basic colors | The Look Editor is small | "Drag the line above it up to give it room." |

## 4. Check for understanding (while circulating)

- "If you change Chorus Colour, which cues change?" (*The chorus, and
  the cues the cyc tracks into: the flash and the scream.*)
- "Why not just change Magenta?" (*Other songs may use Magenta; the
  song's own palette changes only this song.*)

## 5. Share (5 min)

Give one group a new note live on the projector: *"Warmer in the
chorus."* They open Chorus Colour and pick a warmer colour; the class
reads the **Changed:** line aloud.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

From the handout: **Group by type** off, so the song's folder comes
first with every type in it; then tick it again.

## At home

[At home](at-home.md): a **Verse Colour** made the same way and used on
the verse; or, on paper, a palette plan for a song, by job.

## Answer key

- **Exit ticket:** 1 — one change: open Chorus Colour (✎ on a row that
  uses it, or Edit in the Library) and pick the new colour; the steps
  and the cyc, and every cue they track into, follow. 2 — the show's
  Magenta could be used by other songs or cues; changing it would change
  them too. The song's own copy changes only what the song uses. 3 —
  it's named for its job: the colour can change after a note and the
  name is still true. 4 — every cue and layer that uses the palette (and
  any VC button that fires it); check it before changing a palette, to
  know what else will change.
- **Part 1 question:** the chorus (steps and cyc), and every cue the
  cyc tracks into: 1.0.2.5 Flash and 1.0.3 Scream.
- **Extension:** for example the verse's lavender (*Verse Colour*), the
  verse's 40% (*Verse Level*), the scream's red (*Scream Colour*).

## How this lesson was checked

Every step of the handout is run by the course test (`ui/test/courseshow`,
`levelTwoWithTheMouse`, `lessonTwoTwoExtension`) on the real app,
offscreen: the Library's right-click menus (Used by and its click,
Duplicate, Move to folder → New folder…, Group by type), the Look
Editor's Name box and its Used by line, the drops and the top bar's text,
the click on the Basic colors swatch, and what the rig sends (the steps
and the cyc magenta, then both violet, and the scream's cyc violet too).
The at-home steps (Verse Colour into the existing folder) are checked the
same way. The finish file is what the steps make, and the pictures are
taken by the same test.
