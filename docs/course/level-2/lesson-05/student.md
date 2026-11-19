---
layout: student
lesson: "Lesson 2.5"
title: "Moving lights and lines"
permalink: /class/2-5/
nav_exclude: true
search_exclude: true
today: "Fan the truss spots along a line across the risers, then cross them in red for the button, sweeping in over a second."
files: [lesson-2-5-start.qlcc]
app: true
words:
  - term: Line target
    means: "A target that's a line: each light aimed at it gets its own point along it. Ours: **Riser Line**."
  - term: Spread
    means: "**+5** end to end, **0** all on the centre, **−5** crossed."
  - term: Fan / cross
    means: "Beams spread out / beams crossing over each other."
  - term: Colour wheel / ring
    means: "A spot's named colour slots: a colour picks the nearest."
---

## Why

In haze a beam is scenery ([Lesson 0.3](../0-3/)), and moving light
pulls the eye ([Lesson 0.2](../0-2/)). The button ends the song and asks
for applause ([Lesson 0.6](../0-6/)): beams that fan, then sweep into a
cross on the last note, make an ending nobody misses.

## The plan

| Cue | When | Why | Lights |
|---|---|---|---|
| 1.0.7 Fan *(new)* | The last line | | **Block**. **Truss-Spots**: White, Full, aimed at **Riser Line** |
| 1.0.8 Button *(new)* | The last note | | The same, **Spread −5**, **Red**, fade in 1 s |

A snap or a quick sweep for the button? Why?

## Steps

1. **File ▸ Open** → `lesson-2-5-start.qlcc`.
2. Click the **Show Tree** tab (⇧⌘5); open **View ▸ Rig Overview** (⇧⌘R)
   beside it.
3. Click **1.0.6 Dance Break**, **+ Cue**, type `Fan`, **Return**.
   **Mode**: **Block**.
4. Drag **Truss-Spots** onto the dashed box; **White** and **Full** onto
   its row.
5. Drag **Riser Line** (All → Targets) onto the **Truss-Spots** row: a
   fan along the CS row.

   ![The fan](../../images/lesson-2-5/rig-fan.png)

6. Click the **Truss-Spots** row: the aim, the order strip, the plan view
   and **Spread +5**.

   ![The line on the layer's page](../../images/lesson-2-5/line-layer.png)

7. The Fan is a Block cue, so the spots would swing onto the line lit.
   Mark them in the cue before it: click **1.0.6 Dance Break**, drag
   **Truss-Spots** onto the dashed box, then **Out** and **Riser Line**
   onto the **Truss-Spots** row.
8. Click **1.0.7 Fan**, **+ Cue**, type `Button`, **Return**.
9. Click the grey **Truss-Spots** row; **Spread ⋯ → −5**: the beams cross.
   In the plan view (from above, downstage at the bottom) each beam is a
   line from where its light hangs to its point on the line, so a cross
   shows as lines that cross. Read the caption under it. With Grouping set,
   lights that move together share a colour, and it counts the crossings
   (*Groups swap sides: 24 beam crossings*). **Cross** does something only
   with Spread below 0; at +5 or 0 the plan says so.

   ![The plan at Spread −5: the beams cross](../../images/lesson-2-5/line-cross.png)
10. Click **1.0.8 Button**; drag **Red** onto the **Truss-Spots** row.
11. Type `1` in **Fade in**, **Return**.

    ![The button](../../images/lesson-2-5/rig-button.png)

11. **File ▸ Save As** → `Lesson2-5-yourname.qlcc`.

**Stuck?** The beams didn't move: drop Riser Line on the **row**, not the
dashed box. The fan crossed: undo, set −5 in the **Button**. No Spread
row: the Position must say **Aim Riser Line**.

## Keep a shape: Formation

A Spread and Grouping you like will be wanted again on another line, and
copying the numbers by hand lets them drift apart. **Save as Formation…**
keeps them as one palette.

1. Click the **Truss-Spots** row of **1.0.8 Button**: Spread is −5.
   Set **Grouping** to Pairs.
2. On the **Spread** row, **⋯ → Save as Formation…**, name it
   `Pairs Crossed`, **OK**. The layer now uses the palette and clears its
   own rows; it is one undo step.
3. Change the Formation and every cue that uses it follows. It is in the
   Library under **Formation**.

## Blocking: the same look, wherever the people stand

Theatre marks where a performer stands for a number with spike tape. If
the soloist stands downstage in one song and upstage in the next, you
don't want a second look for the same light; you want the target to move.
**Blocking** is that: a named set of stage target places, and a cue picks
one.

1. Open the Lighting Studio (⇧⌘3) and its **Blocking ▾** button: it says
   which blocking the plot shows. **Default** is where the targets stand
   when nothing says otherwise.
2. Click a cue, then its **Blocking** row on the cue page: **Default**, or
   a set. **Save Positions as Blocking…** makes a set from where the
   targets stand now and uses it from this cue on.
3. Pick the new set in **Blocking ▾**: a blue banner reads *Moving targets
   in …*, and a drag moves the selected targets in that blocking only. A
   dashed ring shows where Default puts each one.
4. Every look aimed at a target follows: the Aim looks, the line spreads,
   the follow spot. A cue keeps its blocking for the cues after it, until
   one sets its own.

Why it matters on the real rig: the focus you did in Lesson 3.6 is for
Default, and a blocking is an offset from it, so a venue's correction
carries into every set.

## An effect that starts at a point: Fire Spread

Everything above aimed light at a place. **Fire Spread** uses a place the
other way round: as the point where something starts. The head nearest the
point catches first, and the fire runs outward at an even speed, so a
head twice as far away catches twice as late. Each head flares, brighter
and whiter, then settles into a flicker. It is a cue for a song about
heat, and a good test of whether the rig is placed correctly on the plot,
because the timing comes from where each light, and each pixel of a pixel
bar, stands in the 2D plan. Heads that aren't placed catch together.

1. In a Block cue, put the group that holds the whole rig on a layer and
   pick **Fire Spread** as its effect.
2. Drag a target onto the layer for its **Aim** look: that is where the
   fire starts. With no Aim look it starts at centre stage.

Fire Spread burns *over* what is already there. Until the fire reaches a
head, the head shows whatever is under the effect, and over black it stays
dark, so give the layer no Colour or Dimmer of its own above the effect if
you want the stage dark before the fire. The spread happens once, when the
cue starts, then the fire burns for as long as the look plays. To spread
once and leave the stage as it ends, set **Runs** to **Once** and **At the
end** to **Hold**.

## Props in a hand: a staff in one cue, gone in a later one

A **Staff** or a **Candle** target can be carried. Who holds it is part of
the blocking, so it is cued the way the places are: the cue whose blocking
puts the staff in Ann's hand is when she gets it, and a later cue whose
blocking says otherwise takes it away. Nobody cues "she picks it up". It
goes wherever she goes, in the plot, the front and side views and the Rig
Overview. Light is aimed at the person, never the prop, so a look on her
lands the same either way.

1. In the Lighting Studio, pick the blocking in **Blocking ▾** (or **Save
   Positions as Blocking…** for a new one) that the staff's cue uses.
2. Right-click the staff ▸ **Held By** ▸ Ann. In another blocking, right-click it again ▸
   **Its Own Place** or **Not on Stage**. Each cue gets the staff, or
   loses it, with its blocking.
3. For a row of singers, select them all, right-click one ▸ **Give Each a
   Prop** ▸ **Candle**. Each gets one in the hand, in one Undo step. Do it
   in a blocking set, and only the cues using that set have them.

The candles are lit by a cue, not by the prop. Patch one **IR Candle
Remote** fixture (every candle hears the same remote); a look in the cue
sets its **Select**: **On**, **Off**, **Candle** (flicker), **Light**
(steady), **Dim** or **Brighten**. **Dim** and **Brighten** are button
presses, one step each, not a level; the blaster repeats a button for the
**Window** you set, so give it long enough for the cast to catch the
beam. Leave Select without a fade: a fade passes through the other
buttons on the way. The plot shows what was last sent, not what a candle
did: one in a pocket missed the button.

## When a head swings the long way round

A 540° head reaches most places two ways, a whole turn apart on its pan
channel. Where it changes from one to the other is its **seam**: a beam
moved across that line swings all the way round the other way, in front
of the audience. A spot hung out front with its pan zero facing away from
the stage has the seam in the middle of the stage, so a follow spot walked
across it whips round.

The turn a head works in is kept with it. **Fixture Properties ▸ Pan works
around** shows where the seam is and how clear of it the stage is; Show
Review warns about a head with its seam on the stage (**MOV-14**), and its
fix, **Centre the Pan on Stage…**, moves the seam off the stage in one
Undo step. Nothing else moves it.

## Exit ticket

1. What does a **line target** do that a point target doesn't?
2. What do Spread **+5**, **0** and **−5** look like?
3. Why does the Button have a one-second fade in?
4. What did the spots' colour wheel and ring do with Red, and why not an
   exact red?
5. A soloist carries a staff in the verse and hands it off before the
   chorus. What do you cue, and what do you not?
6. Why is **Dim** on the candle remote not a level?

## At home

Do one:

- **App:** download [`lesson-2-5-finish.qlcc`](../../shows/lesson-2-5-finish.qlcc).
  In **1.0.7 Fan**, try **Spread ⋯ → 0**, then **+2.5**, then **Grouping
  ⋯ → Pairs**. Keep the one you like for the last line; bring in
  `Lesson2-5-Home-yourname.qlcc` and a sentence on why.
- **Paper:** draw the stage from above with four spots and the line.
  Draw Spread +5, 0 and −5, lights numbered 1–4 from SR. Which ends a
  song best?
- **Reuse it:** save Spread −5 and Grouping Pairs as a Formation (see
  *Keep a shape*), check it in its preview, then change it and watch every
  cue that uses it follow.
