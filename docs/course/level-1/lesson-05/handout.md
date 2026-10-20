---
title: "1.5 Student handout"
parent: "Lesson 1.5 — Calling and running the show"
grand_parent: "Level 1 — Basics"
nav_order: 2
---

# Lesson 1.5 — Calling and running the show

**Student handout**

## Today you will

- call cues like a stage manager, and run them like a board operator;
- use the Run tab: GO, Back, Standby, Stop;
- run the whole song, live, with the music.

## Words for today

| Word | Means |
|---|---|
| **Stage manager (SM)** | Runs the show from backstage. Watches the stage and **calls** every cue. |
| **Board operator (board op)** | Runs the lighting console. Presses GO when the SM says so. |
| **Call** | What the SM says to make a cue happen. |
| **Standby** | "Get ready": the next call is coming. The op answers "Standing by". |
| **GO** | Plays the next cue. Press it **on the word GO**, never before. |
| **Back** | Goes back to the cue before. |
| **LOAD (STANDBY)** | Click a cue in the Run tab: it becomes **▷ STANDBY**, the cue the next GO plays, even if it isn't next. |
| **Stop** | Stops the cue list: the stage goes dark. |
| **Design / Operate** | **Design**: building the show; clicking a cue shows it. **Operate**: running the show (the Run tab says **LIVE**); only GO changes the stage. |

## Part 1 — The call (everyone, no computers)

Every cue is called the same way:

| Who | Says |
|---|---|
| SM | "**Standby** lights cue one-oh-two." |
| Op | "**Standing by.**" |
| SM | "Lights cue one-oh-two… **GO**." |
| Op | presses GO on the word **GO** |

**The calling game.** In pairs: one calls, one claps on GO. The caller
tries to catch the op out ("Lights cue… *wait for it*…"). Then swap.

**The cue sheet for the SM:**

| Cue | Call it when… |
|---|---|
| 1.0.0 Pose | the choir is in place, before the first note |
| 1.0.1 Verse | the first words |
| 1.0.2 Chorus | the first chorus, on the hit |
| 1.0.3 Scream | the scream (the Solo follows by itself) |
| 1.0.4 Solo | — (it follows the Scream) |

## Part 2 — Run it (at the laptop)

Roles: **SM** (calls, with the cue sheet), **Board op** (the only one
touching the laptop), **Spotter** (watches the Rig Overview: right cue,
right time?). Switch after each run.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole. **File → Open** → `lesson-1-5-start.qlcc`. (Lesson
   1.4's finish.)
2. Click the **Run** tab (⇧⌘0). Open **View → Rig Overview** (⇧⌘R) beside
   it.
3. Click **Operate** at the top right of the window.
   - *You should see:* the Run tab's top right corner says **LIVE**. Now
     only GO changes the stage.

![The Run tab](../../images/lesson-1-5/run-standby.png)

**(1)** a cue on **STANDBY**, **(2)** what the next GO will play and what
it changes, **(3)** **GO** (or **Space**), **(4)** **Back**, **(5)**
**Stop**, **(6)** **LIVE**.

### A practice run

4. SM: "Standby lights cue one-oh-oh." Op: "Standing by." SM: "Lights cue
   one-oh-oh… GO." Op: **GO**.
   - *You should see:* the cyc fades up blue. In the list, **▶** is beside
     **1.0.0 Pose**, and the line under the list says what's **NEXT**:
     *1.0.1 Verse*.
5. Call and GO the verse.
   - *You should see:* the risers lavender, faces lit from the front.
6. Oops, too early! Press **Back**.
   - *You should see:* back to the Pose; NEXT is the verse again.
7. SM: "Standby lights cue one-oh-two" — the chorus, skipping the verse.
   The op **clicks the 1.0.2 Chorus row** in the list.
   - *You should see:* the row says **▷ STANDBY**, and the line says
     *▷ STANDBY 1.0.2 Chorus: …*. That's **LOAD**.
8. SM: "Lights cue one-oh-two… GO." Op: **GO**.
   - *You should see:* the chorus snaps in: magenta steps.
9. SM: "Go straight to the solo!" The op **double-clicks the 1.0.4 Solo
   row**.
   - *You should see:* the Solo starts at once (it fades up over its 3
     seconds): a dark stage, the spots on the soloist.
10. Press **Stop**.
    - *You should see:* the stage goes dark; the line says **Stopped**, and
      NEXT is *1.0.0 Pose* again.

### Show time

11. Play the song. Run the whole list, start to finish, called by the SM:
    Pose, Verse, Chorus, Scream (the Solo follows by itself). Then
    **Stop**. Switch roles and run it again.
12. When you're done: **Stop**, then click **Design** at the top right.
    - *You should see:* the corner says **DESIGN** again.

## Stuck?

| Problem | Try |
|---|---|
| GO does nothing | Is the **Run** tab showing? At the end of the list? **Stop**, then GO starts again from 1.0.0. |
| The wrong cue went | Did someone click a row? Read the **NEXT / STANDBY** line before GO. |
| **File → Open** is greyed out | You're in Operate: **Stop**, then click **Design**. |
| A box asks "Really stop them and switch back to Design mode?" | Cues are still running. Click **Yes** (or **No**, then **Stop** first). |
| Clicking cues in the Show Tree doesn't change the stage | That's LIVE: only GO does. Click **Design** to build again. |

## Done early? (Extension)

1. **The hold.** The SM calls a GO too early on purpose; the op fixes it
   with **Back** and waits for the next call.
2. Design a sixth cue for the **ending** of the song on paper: when, why,
   which lights, what timing. Next lesson you can build it.
