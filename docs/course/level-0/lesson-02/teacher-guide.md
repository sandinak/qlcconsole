---
title: "0.2 Teacher guide"
parent: "Lesson 0.2 — The virtual stage"
grand_parent: "Level 0 — First look"
nav_order: 1
---

# Lesson 0.2 — The virtual stage

**Teacher guide** · Level 0, lesson 2 · 50 minutes · **Level 0 check**

## Objectives

Students can:

1. use the stage directions: upstage, downstage, stage left, stage right;
2. find the parts of our stage in the Rig Overview: risers, cyc, trusses,
   audience towers, the house;
3. open a show in qlcconsole and play its cues with **GO**.

## Before class

- [ ] qlcconsole installed on the projector computer and every class laptop
  ([Install](../../install.md)). This is the first lesson on the
  computers: allow time, or install before class.
- [ ] [`lesson-1-1-finish.qlcc`](../../shows/lesson-1-1-finish.qlcc) on
  each laptop, in **Documents → qlcconsole**.
- [ ] On the projector: the show open, **View → Rig Overview** (⇧⌘R) made
  big.
- [ ] Print the [handout](handout.md) and the
  [stage plan to label](exit-ticket.md) (one each).
- [ ] Optional: tape "STAGE LEFT" and "STAGE RIGHT" signs on the walls of
  your room, as if the class were the choir facing the audience.

## Vocabulary (introduce in this order)

**upstage / downstage**, **stage left / stage right**, **risers**,
**cyc**, **truss**, **audience tower (AT)**, **house**, **rig**, **Rig
Overview**, **GO**. Each is in the handout's word table and the
[Glossary](../../GLOSSARY.md).

## Timing

| Min | Part | What happens |
|---|---|---|
| 0–10 | **Design idea** (no computers) | Stage directions, standing up |
| 10–20 | **Teacher demo** (projector) | Open the show, the Rig Overview, its views |
| 20–40 | **Hands-on** (pairs/triads, roles) | Handout: find the parts, then GO |
| 40–50 | **Level check** | Label the stage plan, on paper |

## 1. Design idea (10 min)

Get everyone standing, facing the "audience" (the board).

1. "You are the choir. Take one step toward the audience: that's
   **downstage**. A step back: **upstage**." (Old stages sloped up toward
   the back, so the back was *up*.)
2. "Raise your right hand: that's **stage right**. Your left: **stage
   left**." Then turn them round to face the class from the board: "Now
   you're the audience. Where is stage right?" (*On your left.*) Stage
   directions are always **the performers' left and right**.
3. Quick game: call "downstage left!", "upstage right!", and they move.

**Key idea to land:** lighting people and directors use these words so
everyone means the same place.

## 2. Teacher demo (10 min)

On the projector:

1. Open qlcconsole. **File → Open** → `lesson-1-1-finish.qlcc`.
2. Point at the grey **No outputs: preview only** in the footer: "It
   isn't connected to real lights, on purpose. We can't break anything.
   If that corner ever turns red, something is really wrong."
3. **View → Rig Overview** (⇧⌘R). The stage, at 45°. Drag in the picture
   to swing round; the mouse wheel zooms.
4. **Room: Work light**: the stage parts show clearly. Walk through them,
   front to back: the **audience towers** (AT1 on stage right, AT2 on
   stage left) in the **house**; the **proscenium** (the frame) and the
   red **drapes**; the **risers** in three rows (DS, CS, US); the
   **trusses** (four tall frames, two each side); the **cyc** at the back.
5. **View: Top**: "This is the plan, seen from above. Stage right is on the
   left of the picture, downstage at the bottom." **View: Front**, then
   back to **45°**.
6. **Room: House**. Click the **Run** tab (⇧⌘8) and press **GO** once:
   the cyc turns blue. "That's a **cue**. You'll make this in Lesson 1.1."

## 3. Hands-on (20 min)

- Groups of 2–3, **Driver / Navigator / Designer**; switch roles after
  each part of the handout.
- Students without a laptop: the **design station** — start labelling the
  stage plan from the projected picture, then swap.

### What to watch for

| You see | Likely cause | Say |
|---|---|---|
| The Rig Overview is gone | It was closed, or is behind the main window (it is a window of its own) | "**View → Rig Overview** (⇧⌘R) opens it again." |
| Everything looks dark | **Room** is on Blackout or Dim | "Set **Room: Work light**." |
| The view is upside down or far away | They dragged or zoomed a lot | "Pick **View: 45°** again." |
| GO does nothing | The Run tab isn't the one showing, or the show isn't open | "Click the **Run** tab. Is the show open?" |
| "It says No outputs" | Expected | "Not connected to real lights, on purpose: it's a preview." |
| A line at the top: *GO played in Design…* **Switch to Operate** | The first GO in Design, once a session | "Close it with ×. Operate is for running a show; we'll use it in Lesson 1.5." |

## 4. Check for understanding (while circulating)

- "Point to stage left on the screen." (*The right of the picture, from
  the audience side.*)
- "Which riser row is nearest the audience?" (*DS.*)
- "Where are the lights that light faces from the front?" (*The audience
  towers, in the house.*)

## 5. Level 0 check (10 min)

[Label the stage](exit-ticket.md) on paper, alone. Collect it. Students who
get 12 or more of the 15 labels right pass Level 0; go over the rest next
lesson.

## Extension (fast groups)

From the handout: **Room: Blackout** and GO again: the light only. Which
cue lights the risers from below? What else can they find (the lanterns,
the upstage beams)?

## At home

[At home](at-home.md): install qlcconsole and explore the stage, or the
paper stage-directions game.

## Answer key

**The stage plan:** 1 cyc · 2 trusses SR · 3 trusses SL · 4 US riser row ·
5 CS riser row · 6 DS riser row · 7 a soloist downstage centre (DS) ·
8 proscenium (the frame, with the main drape) · 9 the house (audience) ·
10 AT1, the stage-right audience tower · 11 AT2, the stage-left audience
tower · A upstage · B downstage · C stage right · D stage left.

**Questions:** 1 — the performers' (singers') left. 2 — upstage. 3 — the
Rig Overview (View → Rig Overview, ⇧⌘R).

## How this lesson was checked

The course test (`ui/test/courseshow`, `lessonZeroTwoTheVirtualStage`)
opens the show, opens the Rig Overview and checks its **View** (Top,
Front, Side, 45°) and **Room** (Work light, House, Dim, Blackout) choices
and **Live**, then presses GO three times in the Run tab. The pictures
are taken by the same test.
