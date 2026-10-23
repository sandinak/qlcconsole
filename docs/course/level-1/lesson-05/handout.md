---
title: "1.5 Student handout"
parent: "Lesson 1.5 — Calling and running the show"
grand_parent: "Level 1 — Basics"
nav_order: 2
---

# Lesson 1.5 — Calling and running the show

**Student handout**

## Today you will

- call cues as a stage manager and take them as a board operator;
- run the show from the Run tab with GO, Back, Standby and Stop;
- run the whole song live, with the music.

## Words for today

| Word | Means |
|---|---|
| **Stage manager (SM)** | Runs the show during performance, watching the stage and **calling** every cue. |
| **Board operator (board op)** | Runs the lighting console and takes each cue when the SM calls it. |
| **Call** | The SM's spoken instruction that makes a cue happen. |
| **Standby** | The warning before a call: the cue is coming up. The op answers "Standing by". |
| **GO** | Plays the next cue. The op presses it **on the word "GO"**, never before. |
| **Back** | Returns to the previous cue. |
| **LOAD (STANDBY)** | Clicking a cue in the Run tab makes it **▷ STANDBY**, the cue the next GO plays, even if it isn't next in the list. |
| **Stop** | Stops the cue list, and the stage goes dark. |
| **Design / Operate** | **Design** is for building the show: clicking a cue shows it on stage. **Operate** is for running it: the Run tab says **LIVE**, and only GO changes the stage. |
| **Point cue** | A cue numbered between two others (1.0.2.5 between 1.0.2 and 1.0.3), so a late cue doesn't change anyone else's number. *(Extension.)* |

## Part 1 — The call (everyone, no computers)

Every cue is called the same way:

| Who | Says |
|---|---|
| SM | "**Standby** lights cue one-oh-two." |
| Op | "**Standing by.**" |
| SM | "Lights cue one-oh-two… **GO**." |
| Op | presses GO on the word **GO** |

**Calling drill.** In pairs, one of you calls and the other claps on the
GO. The caller tries to catch the op jumping early ("Lights cue… *wait
for it*…"). Then swap.

**The cue sheet for the SM:**

| Cue | Call it when… |
|---|---|
| 1.0.0 Pose | the choir is in place, before the first note |
| 1.0.1 Verse | the first words |
| 1.0.2 Chorus | the first chorus, on the hit |
| 1.0.3 Scream | the scream (the Solo follows by itself) |
| 1.0.4 Solo | — (it follows the Scream) |

## Part 2 — Run it (at the laptop)

Roles: **SM** (calls the show from the cue sheet), **Board op** (the
only one touching the laptop), **Spotter** (watches the Rig Overview:
right cue, right time?). Switch after each run.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole, choose **File → Open** and open
   `lesson-1-5-start.qlcc` (where Lesson 1.4 finished).
2. Click the **Run** tab (⇧⌘7) and open **View → Rig Overview** (⇧⌘R)
   beside it.
3. Click **Operate** at the top right of the window.
   - *You should see:* **LIVE** in the Run tab's top right corner. From
     now on, only GO changes the stage.

![The Run tab](../../images/lesson-1-5/run-standby.png)

**(1)** a cue on **STANDBY**, **(2)** what the next GO will play and
what it changes, **(3)** **GO** (or **Space**), **(4)** **Back**, **(5)**
**Stop** and **(6)** **LIVE**.

### A practice run

4. SM: "Standby lights cue one-oh-oh." Op: "Standing by." SM: "Lights cue
   one-oh-oh… GO." Op: **GO**.
   - *You should see:* the cyc fades up blue, **▶** appears beside
     **1.0.0 Pose** in the list, and the line under the list shows
     **NEXT**: *1.0.1 Verse*.
5. Call and take the verse.
   - *You should see:* lavender risers, with faces lit from the front.
6. Suppose that GO came too early. Press **Back**.
   - *You should see:* the stage back in the Pose, with NEXT on the verse
     again.
7. SM: "Standby lights cue one-oh-two," skipping the verse and going
   straight to the chorus. The op **clicks the 1.0.2 Chorus row** in the
   list.
   - *You should see:* **▷ STANDBY** on that row, and the line reading
     *▷ STANDBY 1.0.2 Chorus: …*. This is **LOAD**.
8. SM: "Lights cue one-oh-two… GO." Op: **GO**.
   - *You should see:* the chorus snap in, with magenta on the steps.
9. SM: "Go straight to the solo!" The op **double-clicks the 1.0.4 Solo
   row**.
   - *You should see:* the Solo start immediately, fading up over its 3
     seconds to a dark stage with the spots on the soloist.
10. Press **Stop**.
    - *You should see:* the stage go dark, the line read **Stopped**, and
      NEXT return to *1.0.0 Pose*.

### Show time

11. Play the song and run the whole list from start to finish, called by
    the SM: Pose, Verse, Chorus and Scream, with the Solo following on its
    own. Then press **Stop**, switch roles and run it again.
12. When you've finished, press **Stop** and click **Design** at the top
    right.
    - *You should see:* **DESIGN** in the corner again.

## Stuck?

| Problem | Try |
|---|---|
| GO does nothing | Check that the **Run** tab is showing and you aren't at the end of the list. **Stop**, and the next GO starts again from 1.0.0. |
| The wrong cue played | Someone probably clicked a row. Read the **NEXT / STANDBY** line before every GO. |
| **File → Open** is greyed out | You're in Operate. Press **Stop**, then click **Design**. |
| A box asks "Really stop them and switch back to Design mode?" | Cues are still running. Click **Yes** (or click **No** and press **Stop** first). |
| Clicking cues in the Show Tree doesn't change the stage | That's LIVE working as intended: only GO changes the stage. Click **Design** to build again. |

## Done early? (Extension)

1. **Recovery drill.** The SM calls a GO too early on purpose; the op
   recovers with **Back** and waits for the next call.
2. On paper, design a sixth cue for the **ending** of the song: when it
   happens, why, which lights, and its timing.
3. **A late cue.** At rehearsal the director asks for one more moment, a
   **Hit** on the drum fill right after the chorus. The SM has already
   written 1.0.3 and 1.0.4 into the script, so those numbers must not
   change. Press **Stop**, click **Design**, then the **Show Tree** tab
   (⇧⌘4).
   1. Click **1.0.2 Chorus**, click **+ Cue**, type `Hit` and press
      **Return**.
      - *You should see:* **1.0.2.5 Hit** between the Chorus and the
        Scream. It's a **point cue**: it fits between two cues without
        changing either number.
   2. Give it a look: drag **Lanterns** onto its drop area, then **White**
      and **Full** onto the Lanterns row.
   3. The director changes their mind: the hit goes after the Solo.
      Drag the **1.0.2.5 Hit** row in the tree onto the lower half of
      **1.0.4 Solo**. Before you let go, the note in the top bar says
      where it goes and its new number.
      - *You should see:* **1.0.5 Hit** at the end of the list. A cue's
        number always matches its place in the list, so moving a cue
        gives it a new number; the other cues keep theirs.
   4. **Undo** (⌘Z) puts it back at 1.0.2.5.

   On a list that nobody has written numbers down for yet, right-click a
   cue → **Renumber this level…** numbers the cues 1, 2, 3… in one go. Once
   the SM's script (or QLab) uses the numbers, add point cues instead.
