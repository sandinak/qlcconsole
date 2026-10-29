---
layout: student
lesson: "Lesson 3.6"
title: "Focus and calibration"
permalink: /class/3-6/
nav_exclude: true
search_exclude: true
today: "Read target places from the stage marks, write and run the show's focus calls, focus a scrambled rig for a score, and change a cue with **Focus this cue**."
files: [ShowChoir-Classroom.qlcc]
app: true
words:
  - term: Focus (the rig)
    means: "Pointing, sizing and sharpening each light so it lands where the design says. Once per room."
  - term: Focus call
    means: "A step of the focus: some lights and their point (\"Truss-Spots → Center\")."
  - term: Hot spot
    means: "The brightest part of a beam, usually its centre."
  - term: Spot down / flood out
    means: "Make the beam narrower / wider (zoom)."
  - term: Sharp / soft
    means: "A crisp / blurred beam edge."
  - term: Correction
    means: "How far a light is off in this room (pan, tilt in degrees), kept so cues land right."
  - term: Venue
    means: "One room the show plays in. Its corrections live in a file beside the show."
  - term: Curtain line / number line / CL
    means: "Where the main curtain hangs / a line across the front, numbered in feet out from the centre line (**CL**)."
  - term: Scramble
    means: "Practice: every mover is knocked off its target and you focus them back, scored."
  - term: Focus this cue
    means: "Changing a cue's own values from the stage. Venue focus fixes the rig; this changes the cue."
---

## Why

A cue is only as good as where its light lands. Aimed a foot off, a
special misses the soloist and the look loses its focus
([Lesson 0.4](../0-4/)). Focus puts every light back on its mark in each
room.

## A focus call (no computer)

The **electrician** is at the light and moves it. The **designer** stands
where it should land, **back to the light**, and reads the shadow of
their head: never look into the beam. The designer calls in short, fixed
words: "Hot spot on me." "Up a little." "Spot down." "Sharper." "Lock it."
The electrician answers "Locked". On our rig, "lock it" is **A**.

Roles: **Electrician** (keys or controller), **Designer** (calls the
moves), **Scribe** (reads the steps). Switch at each part.

## Keys

| Do | Controller | Keys |
|---|---|---|
| Next / previous light | RB / LB | ] / [ |
| Nudge | left stick (right: fine) | arrows (⇧: 1°) |
| Lock it | A | Return |
| Back to the last save | B | Backspace |
| Whole call at once | Y | G |
| Next / previous call | RT / LT | PgDn / PgUp |
| Spot down / flood out | D-pad up / down | , / . |
| Sharper / softer | D-pad right / left | ' / ; |
| Leave | Menu | Esc |

## Steps

1. **File → Open** → `ShowChoir-Classroom.qlcc`.
2. **File → Save As** → `Lesson3-6-yourname.qlcc` **first**: the venue's
   file goes beside your show.
3. Click the **Lighting Studio** tab (⇧⌘3); open **View → Rig Overview**
   (⇧⌘R) beside it.
4. In **Plot layers** → **Stage**, click the eye on **Centre line / DSC
   mark**, **Curtain line** and **Number line**.

   ![The stage marks](../../images/lesson-3-6/stage-marks.png)

5. Copy down where **Soloist DS** and **Center** are. "SL 1' · DS 1' 1""
   is 1 foot stage left of CL and just over a foot downstage of the number
   line.
6. **Focus ▾ → Edit focus calls…**.
7. **+ Call**: tick **Truss-Spots**, **Focus to** **Center**.
8. **+ Call**: **AT-Spots** only, to **Soloist DS**; **Note**
   `Off the proscenium`.
9. **+ Call**: **AT-W-ALL** only, to **Center**.

   ![Three calls](../../images/lesson-3-6/focus-calls.png)

10. **Venue:** **New venue…**, type `Classroom`. **Focused by:** your
    name.
11. Click **Save calls**.
12. Click **Start venue focus**. The panel says **Call 1 of 3**; the first
    spot blinks.
13. Press **]**, then **⇧→**, then **Return**: the second spot is kept for
    Classroom.
14. Press **G** (whole call), **⇧→**, then **Backspace** to put it back.
15. **PgDn** to call 2. Press **,** twice and **'** twice: spot down,
    sharper.
16. **Esc**. If asked about lights not saved, **Discard**.
17. **Focus ▾ → Practice (scramble)…**, choose **Easy: 1–3° off**, **OK**.
18. Light by light, call by call: nudge until the beam lands on the
    target figure, then lock it.
19. When every light is locked, **Esc** for the score.

    ![The score](../../images/lesson-3-6/practice-score.png)

20. **Export the report…** → **PDF** → `Lesson3-6-yourname focus
    report.pdf`. **This is your hand-in.**
21. **Show Tree** tab (⇧⌘5). In the **Command** line, type each line,
    then **Return**:

    ```
    production "Spring Show"
    list "Solo"
    cue + "Solo"
    AT-Spots colour White dimmer Full aim "Soloist DS"
    ```

22. On the cue's page: **Focus ▾ → Focus this cue…**.
23. Press **,** twice, then **Return**: the cue gets a **Zoom** row; the
    venue focus is unchanged.
24. Press **G** until the panel says **Fixture**, **→** twice, **Return**:
    one spot gets a position of its own.
25. **Esc**, then **File → Save** (⌘S).

**Stuck?** "Save the show first": step 2. Nothing moves: **Blackout** is
on or the **GM** is down. Keys do nothing: click the Focus panel first.

## Exit ticket

1. On a focus call, what does the electrician do, and what does the
   designer do?
2. What is the difference between "spot down" and "sharper"?
3. A target reads **SL 1' · DS 1' 1"**. Where is it?
4. You lock a light with A in the venue Classroom. Where is that
   correction kept, and why not in the show?
5. How is Focus this cue different from a focus call?

## At home

Do one:

- **App:** in your `Lesson3-6-yourname.qlcc`, run **Practice (scramble)**
  on **Medium: 2–6° off**, export the report as PDF and bring it with your
  Easy one. Under the score: which call took longest, and why?
- **Paper:** draw a room at home with a curtain line, a centre line and a
  number line in feet. Place three targets and write where each is
  ("SL 3' · US 4'"), then a focus chart for two lights aimed at them.
