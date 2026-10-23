---
title: "3.10 Teacher guide"
parent: "Lesson 3.10 — Coming from Eos (optional)"
grand_parent: "Level 3 — Advanced"
nav_order: 1
---

# Lesson 3.10 — Coming from Eos (optional)

**Teacher guide** · Level 3, lesson 10 · 50 minutes · for students who
already know an Eos desk

## Objectives

Students can:

1. bring an Eos show into qlcconsole with **File ▸ Import ▸ ETC Eos Show**;
2. check the import's **special circuits** and correct a wrong guess;
3. say where each Eos idea they know lands here: channels, groups, cue
   lists, subs, tracking.

## Before class

- [ ] qlcconsole on the projector computer and the class laptops.
- [ ] `classroom-eos.asc` on each laptop (from `docs/course/shows/`). It is
  a small USITT ASCII show written for this lesson: ten dimmer channels
  (front light, back light, a cyc, specials, and the house, work,
  relay and backstage circuits), one cue list of four cues and one sub.
- [ ] If the students have their own Eos show, ask them to export it
  (**File ▸ Export ▸ USITT ASCII** on the desk) and bring it.
- [ ] Print the [handout](handout.md) and the [exit ticket](exit-ticket.md).

## Vocabulary (introduce in this order)

**USITT ASCII**, **special circuit**, **sub (submaster)**, **output
rules** (from Lesson 3.7). Each is in the handout's word table and the
[Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | What an Eos show file holds, and the words that differ |
| 10–20 | **Teacher demo** (projector) | The import, up to the Special circuits page |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout Part 2 |
| 40–45 | **Check / share** | The translation table, filled in together |
| 45–50 | **Exit ticket** | On paper |

## 1. Design idea (10 min)

Ask the Eos students to explain to the rest of the class how their desk
thinks: channels, not fixtures; a tracking cue list; subs on faders;
palettes and presets. Then put the translation table from the handout on
the board and leave the right-hand column empty for now.

**Key idea to land:** the show is the same design in either tool; what
changes is the words and where things live.

## 2. Teacher demo (10 min)

1. **File ▸ New**, then **File ▸ Import ▸ ETC Eos Show (ASCII .asc)…**
   and pick `classroom-eos.asc`.
2. **The show's files**: the `.asc` is filled in. **Next**.
3. **Fixture types**: every channel is a plain dimmer, so each is a
   generic dimmer. The tick to bring the cue lists into the Show Tree is
   on. **Next**.
4. **Special circuits**: the app read each channel's Eos label. Ask:
   "Which of these is wrong?" Channel 5, *Cyc Blue*, is marked
   **Backstage / hall** because of the word *Blue*: it's a cyc light. Set
   it back to **Normal**.

## 3. Hands-on (20 min)

- Roles: **Driver / Navigator / Designer**, switching at each page.
- Students without a laptop: the **design station** — fill in the
  translation table from what the demo showed.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The import is greyed out | They are in Operate | "Click **Design** first." |
| *Cyc Blue* stays on through Blackout later | Channel 5 was left as Backstage | "Look at its output rules: right-click it → **Output Rules…**, untick them." |
| They can't find the sub | It's on the Virtual Console | "**Virtual Console** tab, the frame **Eos subs**." |

## 4. Check for understanding (while circulating)

- "Why did *Cyc Blue* come in as a backstage circuit?" (*the import
  guesses from the label; "blue" on a single dimmer usually means a
  backstage blue light*)
- "Where did the Eos sub go?" (*a fader on the Virtual Console*)
- "Does this cue list track?" (*yes: Eos tracks, and the import makes a
  tracking cue list*)

## 5. Share (5 min)

Fill in the translation table on the board together.

## 6. Exit ticket (5 min)

[Exit ticket](exit-ticket.md) — answers below.

## Extension (fast groups)

Import the student's own Eos show, if they brought one, into a new show,
and read the import report's notes (**Show Details**) on what didn't come
across.

## At home

[At home](at-home.md): the translation table, finished.

## Answer key

- **Exit ticket:** 1 — the house lights, the work lights, a power relay,
  backstage blues: channels that aren't stage lights. 2 — its Eos label;
  check every row, because a label can mislead (*Cyc Blue*). 3 — a
  Playback fader in the Virtual Console frame **Eos subs**, named *Sub 1
  Work*. 4 — any two of: channel → a fixture (*Ch 11 House Lights*);
  group → a group in the **Eos** folder; cue list → a tracking cue list in
  the Show Tree with the same cue numbers; sub → a Virtual Console fader;
  special circuits → output rules.

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonFromEos`) runs it offscreen
on the real app with `classroom-eos.asc`: the special-circuit guesses
(House, Work, Relay (power), Backstage / hall for channels 11–14, and
Backstage / hall for *Cyc Blue*), *Cyc Blue* set back to Normal, ten
fixtures in the new show with the rules each role gives, the groups
**Eos/Front**, **Eos/House & Work**, **Eos/Power relays** and
**Eos/Backstage**, the cue list's four cues in the Show Tree, and *Sub 1
Work* as a fader in **Eos subs**. It drives the wizard's pages through
their code rather than by clicks, and takes no pictures yet.
