---
title: "3.5 Teacher guide"
parent: "Lesson 3.5 — Show control and timecode"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.5 — Show control and timecode

**Teacher guide** · Level 3, lesson 5 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-5/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-5.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-5/">

**Builds on:** Lesson [2.7](../../level-2/lesson-07/teacher-guide.md) (the show) and [1.5](../../level-1/lesson-05/teacher-guide.md) (GO and the Run tab). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. set **Follow timecode** on a cue list and give its cues times, leaving out the ones that belong to the hand;
2. say what QLab's cue numbers promise, and what a clock does that a hand doesn't;
3. name the ways a GO reaches the show (key, Run tab, pad, phone, QLab, clock) and say they are one GO.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] [`lesson-3-5-start.qlcc`](../../shows/lesson-3-5-start.qlcc) on each laptop (Lesson 2.7's finish).
- [ ] On the projector: the start file open, the **Show Tree** tab, the **Run** tab ready.
- [ ] For the demo, if you have one: a timecode source (Branson's QLab laptop sending MTC, or an LTC clip into the audio input), a phone on the same network, and a game controller.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**show control**, **OSC**, **MIDI Show Control**, **timecode**, **Follow timecode**, **pad**, **Run page**. Each is in the handout's word table and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | How a song is run when the music is on a computer: by a hand, by a number, by a clock |
| 10–20 | **Teacher demo** (projector) | Follow timecode and a time; the three show-control rows; the phone page and the pad if you have them |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | One group reads their times against the song map; do they land where the audience feels the change? |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Play the first minute of the song. Ask the class to clap at each change of section, and note the times on the board. "Could you do that every night to a tenth of a second?" Then the two ways of not having to: the sound op's computer calls the cue (QLab fires cue 1.0.2), or both computers read the same clock. Land the difference: a number is an address and a clock is a time. The address breaks if the cues are renumbered; the time breaks if the recording changes.

**Key idea to land:** a show can be run by a hand, by a number or by a clock, and they all play the same GO.

## 2. Teacher demo (10 min)

1. **Connections ▸ Devices ▸ Show control & sync**: read the three rows aloud, then the Web access row. "These are the four ways the outside world reaches the show."
2. On the start file, click the list, tick **Follow timecode**, and type one time. Show the Run tab's **at** on the row.
3. If you have a timecode source (Branson's QLab laptop sending MTC, or an LTC clip into the audio input), start it: the Run tab's header shows the clock, the list goes to each cue as it passes, and under the list it says what is next and in how many seconds. Stop the clock and nothing moves; start it again and it carries on. Jump it back: nothing plays for the jump, and the cues after it play as the clock reaches them.
4. Phone: **Show Settings**, tick **Allow GO from the phone Run page** (leave off afterwards), open **/run** on a phone from the QR beside **Web access**. Tap GO; the show moves. Tap Stop once: it asks again.
5. Pad: the Run tab's **Pad:** button to **Run**. Press A: nothing. Hold LB: the light bar turns orange, **Pad: ARMED**. A: GO.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching at each numbered step.
- Students without a laptop: the **design station** — Part 1 of the handout.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| No **At timecode** box | A list is selected | "Click a cue." |
| The time box goes red | Fewer than four parts | "Hours, minutes, seconds, frames: `00:01:05:00`." |
| Nothing shows on the Run tab | **Follow timecode** isn't ticked | "Tick it on the list first." |
| "My cue didn't go" | No time on it, or it is skipped | "A cue with no time is left to GO." |

## 4. Check for understanding (while circulating)

- "Why would the lights follow a clock?" (*it lands on the beat every night and frees the op*)
- "What does the clock do if it stops mid-song?" (*nothing plays and nothing is forgotten; it carries on when the clock returns*)
- "The clock jumps forward past three cues. How many play?" (*only the latest one passed*)

## 5. Share (5 min)

Each group reads three of their times against the song map. Where would the audience notice a cue two seconds late? Where wouldn't they?

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

Press **Learn** instead of typing: with a timecode source running, the box stamps the time coming in now onto the cue. Ask: "When would you rather learn a time than type it?" (*while the sound op plays the song once and the SM calls the cues*). Then a button on a MIDI controller: right-click the cue list, or a hit, in the Show Tree → **Learn trigger…**, and press the control. Ask why a dedicated button beats a mouse in the dark.

## Part 3 — On the real rig (with Branson)

With Branson's QLab laptop on the show network: turn on **OSC in** or **MIDI Show Control in**, and have the sound op fire cue 1.0.2 from QLab. Then its timecode: the same cues, no hand on GO. Keep the show's **Allow GO from the phone Run page** off unless a phone is part of tonight's plan.

## At home

[At home](at-home.md): a slower recording for the same cues, or eight cues on a time-and-number sheet.

## Answer key

- **Exit ticket:** 1 — It lands on the beat every night and no one has to watch the music; the op is free to watch the stage. A person's GO is a little late and a little different each time. 2 — QLab's cues still say 1.0.2, and 1.0.2 is now a different look. The numbers are an address the sound op has written down. 3 — Timecode leaves it alone. GO still plays it: by hand, or by number from QLab. 4 — In File ▸ Show Settings…, **Allow GO from the phone Run page**. The show's PIN, if it has one, asked once per browser session. The student handout never carries the setting.

## How this lesson was checked

The course test (`ui/test/courseshow`, `levelThreeWithTheMouse` and `lessonThreeFiveFollowsTheTimecode`) does the steps offscreen on the real app: Follow timecode ticked, each cue's time typed in the At timecode box, the times on the Run tab, then Operate with timecode fed in: the list goes to each cue as its time passes, the Flash is left alone, and a GO from the phone page is refused until the show allows it. It doesn't drive the Connections dialog or a real pad. `levelThreePictures` (with `COURSE_SHOTS=docs/course/images`) takes this lesson's pictures, in `images/lesson-3-5/`: Follow timecode, a cue's At timecode box, the times on the Run tab.
