---
title: "3.7 Teacher guide"
parent: "Lesson 3.7 — House lights, work lights and power"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.7 — House lights, work lights and power

**Teacher guide** · Level 3, lesson 7 · 50 minutes

**Give students:** <https://sandinak.github.io/qlcconsole/class/3-7/> (the show file, the steps, the exit ticket and the at-home task; nothing else)

<img src="../../images/qr/class-3-7.svg" width="120" alt="QR code for https://sandinak.github.io/qlcconsole/class/3-7/">

**Builds on:** Lesson [0.7](../../level-0/lesson-07/teacher-guide.md). Open with the handout's *Why this matters* before any clicks.

## Objectives

Students can:

1. say what house lights, work lights and a power relay are for, and who
   needs them on when;
2. set **output rules** so that Blackout and the Grand Master leave them
   alone, and a relay switches instead of fading;
3. run a preshow sequence: house up, house to half, house out.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] `ShowChoir-Classroom.qlcc` on each laptop (from `docs/course/shows/`).
- [ ] On the projector: the show open, the **Fixtures** tab, **View → Rig
  Overview** beside it.
- [ ] For Part 3: the theatre's house light and work light circuits on the
  console, and the rig's power relay if it has one (with Branson).
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**house lights**, **work lights**, **relay**, **Blackout**, **Grand
Master**, **Blind**, **output rules**. Each is in the handout's word table
and the [Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Who needs which lights on, from load-in to the bows |
| 10–20 | **Teacher demo** (projector) | Add House Lights; Output Rules…; Blackout leaves it on |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | The preshow sequence, called |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Walk the class through a show day: load-in, focus, rehearsal, house open,
the show, intermission, strike. For each, ask who is in the room and what
they need to see. Then the two failures: the board op hits Blackout in a
song and the house lights go out with the stage, so the audience is
suddenly in the dark; or someone pulls the Grand Master down at the end of
the night and the LED fixtures' power relay drops with it.

**Key idea to land:** some circuits belong to the building, not the show,
and the console must not dim or black them out by accident.

## 2. Teacher demo (10 min)

1. **Fixtures** tab: **Add ▾ → Add fixture…**, **Generic → Generic**,
   Name `House Lights`, OK. "A house light is one dimmer channel."
2. Right-click it → **Output Rules…**: read the four rules aloud. Tick
   **Ignore Blackout** and **Ignore the Grand Master**, OK. Point out the
   amber badge on its icon.
3. Hover the **Blackout** button at the top right: its tooltip says that
   fixtures set to ignore Blackout stay as they are.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching every fixture.
- Students without a laptop: the **design station** — the show-day table
  in Part 1 of the handout.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| Add ▾ is greyed | They are in Operate | "Click **Design** at the top right." |
| The command line can't find *House* | The name has a space and no quotes | "Names with spaces go in quotes: `"House Lights"`." |
| "The house lights don't show on the stage" | The virtual stage has no house lights | "Right: on the real rig in Part 3. Here, watch the cyc." |
| Blackout left on | | "Press **Blackout** again before you go on." |

## 4. Check for understanding (while circulating)

- "Why shouldn't Blackout take the house lights out?" (*the audience
  would be left in the dark; the house belongs to the front of house,
  not the show*)
- "Why is the relay a switch and not a dimmer?" (*a relay is on or off;
  dimming the power to LED fixtures would just drop them*)
- "When does Stage Power come on?" (*when you switch to Operate, the
  show's setting; or when the show opens*)

## 5. Share (5 min)

One group's SM calls the preshow list (house up, house to half, house out)
while their op takes it, with Blackout tried in the middle.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

Blind, in Design: the real rig is muted while the screen keeps showing
the looks, so a look can be built without the stage seeing it. Fixtures
that ignore Blackout stay as they are in Blind too. Ask: "Why does that
matter for the work lights during a rehearsal?"

During a show (Operate) the same button can't mute anything: the
audience is watching. There it is a view: the stage keeps playing the
run, and the plot shows the cue being fixed in the Show Tree, so the next
cue can be checked before GO. Ask: "What does the blue footer tell the
operator?"

## Part 3 — On the real rig (with Branson)

With the theatre's house light and work light circuits on the console:
set their output rules as in the handout, bring the house to full, then
try Blackout and the Grand Master in front of the class. Then the relay:
switch to Operate and listen for it. Leave the show's setting at **When
you switch to Operate** unless Branson says otherwise: power comes on only
when someone has chosen to run the show.

## At home

[At home](at-home.md): a show-day light plan on paper.

## Answer key

- **Exit ticket:** 1 — house: over the audience, up before the show and at
  intermission; work: bright plain light for working on stage when there's
  no show. 2 — Ignore Blackout and Ignore the Grand Master. 3 — a relay is
  on or off; never dimmed or faded, and the Grand Master can't switch it
  off. 4 — on its own (On when not in use) when you switch to Operate,
  which is the show's setting in Settings ▸ Advanced ▸ Output.

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonHouseLights`) runs it
offscreen on the real app: three one-channel generic dimmers added where
Add fixture puts them, the Output Rules dialog's boxes ticked as the
handout says, Stage Power off in Design until the first switch to
Operate and on after it, the preshow list typed as in the handout,
Blackout taking the cyc to zero while House Lights and Stage Power keep
full, the Grand Master at zero doing the same, and the house going to
half and out with the cues. It doesn't drive the Add fixture dialog
itself, and takes no pictures for this lesson yet.
