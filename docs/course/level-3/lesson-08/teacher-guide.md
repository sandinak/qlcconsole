---
title: "3.8 Teacher guide"
parent: "Lesson 3.8 — Running a real show"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.8 — Running a real show

**Teacher guide** · Level 3, lesson 8 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-8/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-8.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-8/">

**Builds on:** Lessons [1.5](../../level-1/lesson-05/teacher-guide.md) (calling and running), [2.1](../../level-2/lesson-01/teacher-guide.md) (Block), [2.6](../../level-2/lesson-06/teacher-guide.md) (hits) and [3.5](../lesson-05/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. number a show by song, name its sections and make the second a Block start;
2. say when a group master, a plain hit and an override hit is the right tool;
3. make the cue sheet and read the tracking sheet.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] [`lesson-3-8-start.qlcc`](../../shows/lesson-3-8-start.qlcc) on each laptop (Lesson 3.5's finish).
- [ ] On the projector: the start file open, the **Show Tree** tab, the **Rig Overview** beside it.
- [ ] A printer or a PDF viewer for the cue sheet.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**section**, **Block at section start**, **group master**, **override hit**, **cue sheet**, **tracking sheet**, **Blackout**, **Stop all**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | One show, many songs, one GO; what the night asks of the op |
| 10–20 | **Teacher demo** (projector) | Numbering, + Section, Block; the Lanterns master; the Hush hit; the cue sheet |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | One group runs song 1 into song 2 while the SM calls it; the op uses the master and Hush |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Draw a show's night on the board: songs 1 to 4 across, the preshow before, an intermission. Ask: "The last cue of song 1 leaves red on the risers. The next song starts. Whose job is it to take the red off?" Then the two answers: the song 2 programmer, in every cue (and one cue will miss it), or the one setting that starts song 2 from nothing. Then the night: a room that makes the lanterns too hot, a soloist who forgets the words. Land that these are the operator's tools and not edits to the design.

**Key idea to land:** the show is built so the op can run it without editing it.

## 2. Teacher demo (10 min)

1. On the start file: click **Spring Show**, pick **Show.Part.Iteration**. Point at the header the cues fold under.
2. **+ Section**, `Slow Dance`; type the lines; tick **Block at section start**. In the Rig Overview, click the Button, then 2.0.1: the red is gone.
3. Fixtures tab: **Lanterns**, tick **Group master**. In Operate, drag the fader to half and back.
4. Type the Hush hit and `override`; fire it from the Virtual Console over Slow Dance.
5. **More ▾ → Cue Sheet…**: both sheets. Point at a grey `(Red)`: tracked in, not set here.
6. Optional, with a controller: the Run tab's **Pad:** button, **Menu** to walk the show from the stage: next and previous cue with the triggers, levels with LB and a trigger. The pad can walk the show while someone else at the laptop sees where you are.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching at each numbered step.
- Students without a laptop: the **design station** — Part 1 of the handout.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| **+ Section** is greyed | The production has no numbering | "Pick Show.Part.Iteration on the production first." |
| No fader beside the cues | **Group master** isn't ticked on the group | "It's on the group, in the Fixtures tab." |
| The Hush button does nothing | They are in Design | "Operate first." |
| Hush changes nothing on the lanterns | They forgot `override` | "Without override a hit only adds: the brighter one wins. Type `override`." |

## 4. Check for understanding (while circulating)

- "What would song 2 inherit without Block?" (*the Button's red, the Fan's beams: anything song 1 left tracking*)
- "Group master at half and Grand Master at half?" (*a quarter: they multiply*)
- "What does Stop all do to Hush?" (*lets it go, with the cues*)

## 5. Share (5 min)

A group's SM calls the end of song 1 and the start of song 2; the op uses the Lanterns master and Hush once each, on a call.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

**Running order…** (on the production): drag the songs into tonight's order, or untick one to leave it out; GO passes over it. Then ask why the song you moved should be Block. (*It tracks from the song before it by number, not by running order.*) And the **phone Run page** has a slider for each group master, under the cues; ask who should be allowed to touch it.

## Part 3 — On the real rig (with Branson)

In the theatre: a preshow list as section 0 of the show (the cues of Lesson 3.7), the first GO at the house-out call, and the pad's Run role for the SM backstage. Walk through the checklist aloud, with the person who does each item.

## At home

[At home](at-home.md): a third song and a second override hit, or the night's checklist on paper.

## Answer key

- **Exit ticket:** 1 — Otherwise the colours, positions and beams the last cue of song 1 left on stage track into song 2. Block starts the song from nothing. 2 — A cue's level is the design: it goes in the show for every night. A group master is tonight's adjustment for the room, and no cue is edited. 3 — It wins over what the cues play on the channels it sets, brightness included, so it can pull a light down. A plain hit only adds: the brightest wins. 4 — Every layer across the cues, with what each cue sets (plain) and what only carries in (grey, in brackets). The programmer, to see what a cue inherits.

## How this lesson was checked

The course test (`ui/test/courseshow`, `levelThreeWithTheMouse` and `lessonThreeEightRunsTheShow`) does the steps offscreen on the real app: the numbering picked on the production, the first song named on its header, **+ Section**, the typed cues, Block at section start, the Group master box in the group's editor, the Hush hit typed with `override` and its Virtual Console page. Then in Operate: a jump to the Button and GO into song 2 (nothing of the Button's red carried in), the Lanterns master at half and back by the command line, Hush pulling the lanterns to a quarter from its Virtual Console button and letting go, and the cue sheet's two song headings and the tracking sheet. It doesn't save the PDF or drive a controller. `levelThreePictures` (with `COURSE_SHOTS=docs/course/images`) takes this lesson's pictures, in `images/lesson-3-8/`: Numbering, Block at section start, the Lanterns master on the Run tab, and the cue sheet and tracking sheet.
