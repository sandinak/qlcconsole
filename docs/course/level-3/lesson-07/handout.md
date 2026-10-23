---
title: "3.7 Student handout"
parent: "Lesson 3.7 — House lights, work lights and power"
grand_parent: "Level 3 — Advanced"
nav_order: 2
---

# Lesson 3.7 — House lights, work lights and power

**Student handout**

## Today you will

- add the house lights, the work lights and a power relay to the show;
- give each the **output rules** it needs, so Blackout and the Grand
  Master leave them alone;
- run a preshow sequence: house up, house to half, house out.

## Words for today

| Word | Means |
|---|---|
| **House lights** | The lights over the audience. Up before the show and at intermission, out when the show starts. |
| **Work lights** | Plain bright lights for working on stage when there's no show: rigging, set-up, cleaning. |
| **Relay** | A switch the console controls, for power: on or off, never dimmed. A relay often powers a whole truss of LED fixtures. |
| **Blackout** | A switch that takes every light to zero at once, whatever is running. Press it again to bring them back. |
| **Grand Master (GM)** | The one fader over everything: at 50 % every light is at half its level. It's the **GM** slider in the footer. |
| **Blind** | In Design: the real lights are muted while the screen keeps showing the looks, so you can build without the stage seeing it. |
| **Output rules** | Rules for one fixture: **Ignore Blackout**, **Ignore the Grand Master**, **Switch (relay)**, **On when not in use**. |

## Part 1 — Plan it (on paper, everyone)

A show day, from load-in to strike. For each moment, who is in the room
and which lights should be on?

| Moment | Who's there | House | Work | Stage power |
|---|---|---|---|---|
| Load-in and focus | | | | |
| House opens | | | | |
| The show | | | | |
| Intermission | | | | |
| Strike | | | | |

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet, watches the stage).
Switch roles after each fixture.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `ShowChoir-Classroom.qlcc`.
2. Click the **Fixtures** tab (⇧⌘2). Open **View → Rig Overview** (⇧⌘R).

### Add the three circuits

3. Click **Add ▾ → Add fixture…**. In **Fixture Model**, open **Generic**
   and pick **Generic** (a plain dimmer). Type the **Name** `House Lights`,
   leave **Channels** at 1 and click **OK**.
   - *You should see:* **House Lights** in the fixture list, at a free
     address the app picked.
4. Do the same for `Work Lights` and `Stage Power`.

### Give them their rules

5. Right-click **House Lights** → **Output Rules…**. Tick **Ignore
   Blackout** and **Ignore the Grand Master**, then **OK**.
   - *You should see:* a small amber badge on its icon. Hover it to read
     its rules.
6. The same for **Work Lights**.
7. Right-click **Stage Power** → **Output Rules…**. Tick **Switch
   (relay)**, **Ignore Blackout**, **Ignore the Grand Master** and **On
   when not in use**, then **OK**.
8. Open **Settings** (⌘,) → **Advanced**. Under **Output (saved with this
   show)**, read **"On when not in use" fixtures come on:** — leave it at
   **When you switch to Operate**, and close Settings.

   Stage Power is now off while you build, and on by itself from the
   moment someone switches to Operate. You won't see it on the virtual
   stage; on the real rig you'd hear the relay click.

### The preshow, typed

9. Click the **Show Tree** tab (⇧⌘4). Click the **Command** line at the
   bottom and type each line, then **Return**:

   ```
   production "Spring Show"
   list "00-Preshow"
   cue + "House up"
   "House Lights" dimmer Full
   TrussC-RGB colour Blue dimmer Full
   cue + "House to half"
   "House Lights" dimmer Half
   cue + "House out"
   "House Lights" dimmer Out
   ```

   - *You should see:* a cue list **00-Preshow** with **0.0.0 House up**,
     **0.0.1 House to half** and **0.0.2 House out**. The cyc preset is in
     the first cue, so the audience walks in to a blue cyc.

### Try the masters

10. Click **0.0.0 House up**. The cyc is blue in the Rig Overview, and the
    house lights are at full.
11. Click **Blackout** at the top right.
    - *You should see:* the cyc go dark. The house lights stay at full:
      they ignore Blackout. Click **Blackout** again to bring the stage
      back.
12. In the footer, drag the **GM** slider all the way down, then back up.
    - *You should see:* the cyc go dark and come back. The house lights
      and the relay don't move.

### Save

13. **File → Save As** → `Lesson3-7-yourname.qlcc` in **Documents →
    qlcconsole**.

## Stuck?

| Problem | Try |
|---|---|
| **Add ▾** is greyed out | You're in Operate. Click **Design** at the top right. |
| The command line says it can't find *House* | Put names with spaces in quotes: `"House Lights"`. |
| The cyc stays dark | **Blackout** is still on (it's lit red), or the **GM** slider is down. |
| Something went wrong | **Undo** (⌘Z) takes back the last step. |

## Done early? (Extension)

1. Turn on **Blind** (top right) in Design and click the cues of the
   preshow. What does the screen show, and what would the real lights do?
   Which of our three fixtures stay as they are in Blind?
2. A work light that the show also uses in a scene (a "work light look"
   for a song set in a garage): should it ignore Blackout? Argue both
   sides.
