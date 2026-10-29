---
layout: student
lesson: "Lesson 1.5"
title: "Calling and running the show"
permalink: /class/1-5/
nav_exclude: true
search_exclude: true
today: "Call cues as stage manager and take them as board op, then run the whole song live from the Run tab."
files: [lesson-1-5-start.qlcc]
app: true
words:
  - term: Stage manager (SM)
    means: "Runs the show in performance and **calls** every cue."
  - term: Board op
    means: "Runs the console and takes each cue when the SM calls it."
  - term: Standby
    means: "The warning before a call. The op answers \"Standing by\"."
  - term: GO / Back / Stop
    means: "Play the next cue / return to the previous one / stop the list (the stage goes dark)."
  - term: LOAD (STANDBY)
    means: "Clicking a cue in the Run tab makes it **▷ STANDBY**: the next GO plays it."
  - term: Design / Operate
    means: "Building (clicking a cue shows it) / running: the Run tab says **LIVE** and only GO changes the stage."
---

## Why

A cue only works if it happens on the moment. The stage manager calls it
and the board op takes it on GO ([Lesson 0.7](../0-7/)); today you run
that with the real tool.

## The call

| Who | Says |
|---|---|
| SM | "**Standby** lights cue one-oh-two." |
| Op | "**Standing by.**" |
| SM | "Lights cue one-oh-two… **GO**." |
| Op | presses GO on the word **GO**, never before |

Drill in pairs: one calls, one claps on the GO. The caller tries to catch
the op jumping early. Swap.

| Cue | Call it when… |
|---|---|
| 1.0.0 Pose | the choir is in place, before the first note |
| 1.0.1 Verse | the first words |
| 1.0.2 Chorus | the first chorus, on the hit |
| 1.0.3 Scream | the scream (the Solo follows by itself) |

Roles: **SM**, **Board op** (the only one on the laptop), **Spotter**
(watches the Rig Overview). Switch after each run.

## Steps

1. **File ▸ Open** → `lesson-1-5-start.qlcc`.
2. Click the **Run** tab (⇧⌘8); open **View ▸ Rig Overview** (⇧⌘R)
   beside it.
3. Click **Operate** at the top right. The Run tab shows **LIVE**.

   {% include inapp.html do="Operate and Design" path="Control ▸ Operate" mac="⌘F12" win="Ctrl+F12" see="The same item reads **Design** to switch back. In Operate the app won't open a show or quit." %}

   ![The Run tab](../../images/lesson-1-5/run-standby.png)

4. Call and take **1.0.0 Pose**. The cyc fades up blue; NEXT reads
   *1.0.1 Verse*.
5. Call and take the verse.
6. That GO came too early: press **Back**. The stage is back in Pose.
7. SM: "Standby lights cue one-oh-two." The op **clicks the 1.0.2 Chorus
   row**: **▷ STANDBY** (LOAD).
8. SM: "…GO." Op: **GO**. The chorus snaps in.
9. SM: "Go straight to the solo!" The op **double-clicks the 1.0.4 Solo
   row**: it starts at once.
10. Press **Stop**. The stage goes dark; NEXT is back on 1.0.0.
11. Play the song and run the whole list, called by the SM. **Stop**,
    switch roles, run it again.
12. Press **Stop**, then click **Design** at the top right.

**Stuck?** The wrong cue played: read the **NEXT / STANDBY** line before
every GO. **File ▸ Open** greyed out: **Stop**, then **Design**.

## Exit ticket

1. Who decides **when** a light cue happens?
2. The SM says "Standby lights cue one-oh-three". What do you say, and
   when do you press GO?
3. What happens when you **click** a cue in the Run tab?
4. The Run tab says **LIVE**. What does that mean for the board op?

## At home

This week you start the [Level 1 check](../1-check/). Do one:

- **App:** write the cue sheet for your 30-second clip (3–5 cues: when,
  why, lights, timing). Start programming it from
  [`lesson-1-1-start.qlcc`](../../shows/lesson-1-1-start.qlcc), run it
  with someone at home calling, and bring in
  `Level1-check-yourname.qlcc` with the sheet.
- **Paper:** the same cue sheet, then practise calling it aloud while the
  clip plays.
