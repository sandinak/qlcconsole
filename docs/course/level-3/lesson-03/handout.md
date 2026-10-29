---
title: "3.3 Student handout"
parent: "Lesson 3.3 — Reading the plot: symbols"
grand_parent: "Level 3 — Advanced"
nav_order: 2
---

# Lesson 3.3 — Reading the plot: symbols

**Student handout**

## Why this matters

A plot is how a crew agrees what hangs where. A symbol tells you what
kind of light it is, and so which part of a look it can be: a wash, a
spot, a special ([Lesson 0.3](../../level-0/lesson-03/handout.md)).

## Today you will

- read a lighting plot: what each symbol says about the light;
- let qlcconsole draw every light as the symbol it guesses, and correct the
  guesses that are wrong;
- mark our lanterns as **practicals**.

## Words for today

| Word | Means |
|---|---|
| **Plot** | The drawing of the rig from above: every light, where it hangs and what it is. |
| **Symbol key** | The legend of a plot: which symbol means which kind of light. |
| **Plot symbol** | What a light looks like on qlcconsole's plot and in its studio: a moving head, a PAR can, an LED bar, a lantern… **Auto** draws it from its type. |
| **Practical** | A light that is part of the set and is seen as a light by the audience: a hanging lantern, a table lamp, a light-up staff. |

## Part 1 — Read it (on paper, everyone)

On the printed plot, find one of each and write down its symbol:

| Kind of light | Symbol (sketch it) | How many on the plot? |
|---|---|---|
| A moving light | | |
| A wash | | |
| A spot (hard edge) | | |
| A practical | | |

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet, compares the plot with
our rig). Switch roles after each correction.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `ShowChoir-Classroom.qlcc`.
2. Click the **Lighting Studio** tab (⇧⌘3).
   - *You should see:* the plot: the stage from above, with the trusses
     upstage, the risers, and the audience towers out in the house.

### Let the app guess

3. On the plot's bar, click **Show** and tick **Show Auto fixtures as
   their closest plot symbol**.
   - *You should see:* the truss spots, the audience-tower spots and the
     upstage beams drawn as moving heads, the step strips, step tops and
     truss boxes as bar washes, and the lanterns as LED PARs.
4. With the Designer: which of those is wrong for our rig? Write down two.

### Correct the guesses

5. Click the **Fixtures** tab (⇧⌘2). Find the group **AT-W-ALL** (in the
   folder **AT**), right-click it and open **Plot Symbol ▸**.
   - *You should see:* the first item reads **Auto (Moving head spot)**:
     that's the app's guess. The rest of the menu is in five sections:
     **Moving**, **Fixed conventional**, **Fixed LED**, **Practicals** and
     **Scenery**.
6. Choose **Moving head round wash**, then go back to the **Lighting
   Studio** tab.
   - *You should see:* the four audience-tower washes drawn with a round
     base and lens instead of a spot's.
7. In the **Fixtures** tab, right-click the **Lanterns** group →
   **Plot Symbol ▸** → under **Practicals**, **Lantern**.
   - *You should see:* on the plot, the six lanterns drawn as lanterns
     (from above, their base). A lantern throws no beam: when it is lit,
     its glow spreads round it. Their icons in the Fixtures tab changed
     too.
8. Click the **Show Tree** tab (⇧⌘5) and open the Library's groups.
   - *You should see:* the same icons there: the app uses one icon per
     fixture everywhere.

A plot symbol is only the picture. The lights send exactly what they did
before; their definitions don't change.

### Save

9. **File → Save As** → `Lesson3-3-yourname.qlcc` in **Documents →
   qlcconsole**.

## Stuck?

| Problem | Try |
|---|---|
| A symbol went on the wrong lights | **Undo** (⌘Z) in the **Fixtures** tab or the **Lighting Studio** takes it back: both use the rig's history. |
| Only one lantern changed | Right-click the **Lanterns** group, not one lantern. |
| There is no **Show** button | Click the **Lighting Studio** tab; the Show button is on the plot's own bar. |

## Done early? (Extension)

1. The **TrussC-RGB** boxes light the cyc. Which symbol says that best?
   Set it, and say why.
2. Untick **Show Auto fixtures as their closest plot symbol**. Which lights
   keep their new symbol, and why?
