---
title: "3.9 Student handout"
parent: "Lesson 3.9 — Troubleshooting, and the Level 3 check"
grand_parent: "Level 3 — Advanced"
nav_order: 2
---

# Lesson 3.9 — Troubleshooting, and the Level 3 check

**Student handout**

## Why this matters

Things go wrong at the worst time, and in two kinds. Something saved in the show is wrong (a song that carries into the next, a button that fires nothing), and it is wrong on every computer. Or something is wrong now, here (Blackout is on, an output is down). Read the show first, ask the light second, and change the least you can. What is deliberate gets a note, so the next person who opens the show reads the decision and not a fresh warning.

## Today you will

- read Show Review's findings, fix one at its source and accept one with a note;
- ask a dark light why it isn't on, and read the answer;
- find the layer that makes a cue look wrong with Solo.

## Words for today

| Word | Means |
|---|---|
| **Show Review** | A read of the saved show that lists what is probably wrong with its design, each finding with where to fix it. |
| **Finding** | One thing Show Review reports: a check's id (CUE-02), how bad it is, and what it says in the show's own names. |
| **Accept** | Say a finding is on purpose, with a note and your name. The note is saved with the show. |
| **Why is this on?** | Right-click a light, or select it and press ⌘I: what sets it now and, when it is dark, what holds it down. |
| **Not ready** | The footer's list of what is wrong on this computer right now (a controller asleep, no output), not in the show. |

## Part 1 — Plan it (on paper, everyone)

A light won't come on at 7:20. Write the order you would check things in. Which of the checks take ten seconds? Which can wait until after the house opens?

## Part 2 — Build it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks *You should see*), **Designer** (holds the list from the page above). Switch every few steps.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

1. **File ▸ Open** → `lesson-3-9-start.qlcc`: the show from Lesson 3.8, left in a
   state. Open the **Show Tree** tab (⇧⌘5) and **View ▸ Rig Overview** (⇧⌘R).
2. If the footer says **Not ready**, click it and read. The classroom
   show has no outputs, so one line there is expected. That list is about
   now, on this computer.
3. Click **Review** in the footer.
   - *You should see:* findings, worst first. Hover a row for why it
     matters. Look for **CUE-02**, and for four **CUE-01** under it.
4. Select **CUE-02** and click **Fix ▸**. It takes you to the second song.
   Click the header **2 — Slow Dance** and tick **Block at section start**.
   Click **Re-check**.
   - *You should see:* CUE-02 and the four CUE-01 gone. They were one
     fault: the Button's spread still on in Slow Dance.
5. Select **VC-03**, a hit called *Spare* that no button fires. **Accept…**,
   write `Kept for the encore`, your name, **OK**. Show **Accepted**.
   - *You should see:* the finding moves to Accepted, with your note.
6. Leave the two **MOV-08** warnings and the notes. They are about the
   real rig's moving lights, and about the classroom having no outputs.
   Tell your partner which you would fix first on the real rig, and why.
7. Click **2.0.1 Slow Dance**. The Lanterns should be amber at full, and in
   the Rig Overview they are dark. Right-click a lantern → **Why is this
   on?** (or select it and press ⌘I).
   - *You should see:* under **Why it isn't on**, a line *Group master
     "Lanterns" is at 0 %*. The line about the universe having no output is the
     classroom.
8. Open the **Run** tab (⇧⌘8). Double-click the name under the **Lanterns**
   fader to put it back to full. Look at the Rig Overview again.
9. Back in the Show Tree, click the **TrussC-RGB** layer of 2.0.1 and press
   **Solo**. Only that layer plays: the lanterns go out. Press **Solo**
   again, and the cue is whole.
10. **File ▸ Save As** → `Lesson3-9-yourname.qlcc`.

**Stuck?** No **Review** chip: it appears a couple of seconds after the show
opens. **Fix ▸** greyed: select a finding first. **Accept…** needs a note.
The lanterns still dark: the Grand Master (**GM** in the footer) or Blackout.
Ask the light again.

## If it's wrong and the house is opening

Undo is ⌘Z, in the order you did things. A show that crashed leaves a
recovery copy beside it (*Show*.qlcc.autosave); Open it with **All Files**
and Save As under a new name. Change the least you can, and write down
what you changed.

## Done early? (Extension)

Turn a check off for the show with a note (**Turn Check Off…**), for a rig that has no moving heads. Ask: "When is that honest and when is it hiding a problem?" Then **Follow selection** in the Why window (click along a row of lights and read each answer).
