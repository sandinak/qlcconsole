---
title: "Level 2 check"
parent: "Lesson 2.7 — Sub-cues: stomps inside a cue"
grand_parent: "Level 2 — Intermediate"
nav_order: 6
---

# Level 2 check — design and program one full song

*Two or three class periods after Lesson 2.7. Groups of 2–3.*

## The task

Design, program and run the lighting for **one whole song**, from the
first note to the button.

1. **Choose the song.** Your teacher assigns one from the show, or your
   group proposes one for approval. Get a recording you can play in
   class.
2. **Map it** (Lesson 0.5–0.6): the sections, what the audience should
   feel at each change, the builds, the hits and the button.
3. **Design it on paper** as a cue sheet, about **8 to 15 cues**:

   | Cue | When (words or time) | What should the audience feel? | Lights (group: colour, level, aim, effect) | Mode / time |
   |---|---|---|---|---|
   | 1.0.0 | | | | |
   | | | | | |

   Plus a short list of **hits** for busking, each with what it's for.
4. **Program it** in qlcconsole from
   [`lesson-1-1-start.qlcc`](../../shows/lesson-1-1-start.qlcc) (the
   empty classroom show): **+ Production** (in the header, or under **More ▾**) named for your
   group, **+ Cue list** named for the song, then the cues (a second song is a **+ Section**
   of that list, not another list). It must use:
   - **tracking on purpose**: at least one **Block** cue (a hard start),
     and **Release** or **Cue-only** where something must not carry on
     (Lesson 2.1);
   - **at least one palette of the song's own**, named for its job, in a
     folder for the song (2.2);
   - **at least one effect** (2.4);
   - **at least one hit** on a Virtual Console button (2.6).

   Child layers or group sets (2.3), a line (2.5) and sub-cues (2.7)
   score higher where the song calls for them. Save as
   `Level2-check-group.qlcc`.
5. **Run it live** while the song plays, one of you calling as SM and
   another on the board, with a hit fired at least once.
6. **A two-minute talk:** what you wanted the audience to feel, and how
   the light did it, cue by cue for the big moments.

## Rubric

| | Beginning (1) | Developing (2) | Proficient (3) | Advanced (4) |
|---|---|---|---|---|
| **Design** (the map and the sheet) | Cues with no reasons | Some cues have a reason | Every cue has a reason that fits the music; a clear build to the peak and a button | A story across the song: contrast between sections, held back for the peak, every quality of light chosen on purpose |
| **Programming** (the show file) | It doesn't play, or a required item is missing | Plays, but differs from the sheet, or tracking carries things on by accident | Plays the sheet; all four required items, used where they belong | Plays the sheet cleanly; tracking, palettes, effects and hits each doing a job; faces lit throughout |
| **Operation** (the live run) | Cues missed or very late | Some cues early or late | Every cue on its call; the hit fired on the music | Clean calls, GOs and Sub GOs; a mistake recovered calmly |
| **Teamwork** (and the talk) | One person did it | Roles shared unevenly | Everyone had a role | Everyone can explain every cue and why |

**Pass:** 3 or more in every row. A group that scores below 3 in a row
gets specific notes on what to work on, then runs it again.

## Teacher notes

- **Time:** one period to map and design (the at-home of 2.7 starts it),
  one to program, one to run and talk. Groups that finish early add
  sub-cues or a line.
- **Checking the file:** open it and click through the cues in Design.
  The tree shows tracking at a glance: **B** and **C** in the **M**
  column, **R** counts, grey rows; the Library shows the song folder;
  **Hits** shows **VC ×1** where a button fires a hit.
- **Common problems:** a colour from the verse still on in the bridge
  (tracking: ask where it's released); an effect on the front light
  (eye candy over faces); hits named "Hit 1" (ask what each is for).
