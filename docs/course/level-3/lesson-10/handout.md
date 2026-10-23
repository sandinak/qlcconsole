---
title: "3.10 Student handout"
parent: "Lesson 3.10 — Coming from Eos (optional)"
grand_parent: "Level 3 — Advanced"
nav_order: 2
---

# Lesson 3.10 — Coming from Eos

**Student handout**

## Today you will

- bring an Eos show into qlcconsole;
- check which channels the import reads as house lights, work lights,
  relays and backstage lights, and fix the one it gets wrong;
- find each Eos idea you know in qlcconsole.

## Words for today

| Word | Means |
|---|---|
| **USITT ASCII** | The plain-text show file an Eos desk exports (**File ▸ Export ▸ USITT ASCII**): patch, groups, palettes, presets, subs and cue lists. |
| **Special circuit** | A channel that isn't a stage light: a relay, house lights, work lights, backstage blues. |
| **Sub (submaster)** | On Eos, a fader that brings up a set of levels by hand. |
| **Output rules** | From Lesson 3.7: **Ignore Blackout**, **Ignore the Grand Master**, **Switch (relay)**, **On when not in use**. |

## Part 1 — Translate it (on paper, everyone)

Fill in the right-hand column as you go through Part 2.

| On Eos | In qlcconsole |
|---|---|
| A channel (*11 House Lights*) | |
| A group (*Front*) | |
| A cue list, with tracking | |
| A sub on a fader | |
| A channel marked as a relay or house light | |

## Part 2 — Import it (at the laptop)

Roles: **Driver** (mouse), **Navigator** (reads each step aloud and checks
*You should see*), **Designer** (holds this sheet and fills in the table).
Switch roles at each page of the wizard.

**On Windows or Linux:** where it says ⌘, press **Ctrl**.

### Get ready

1. Open qlcconsole and choose **File → New**: the import goes into the
   show that is open.

### The import

2. **File → Import → ETC Eos Show (ASCII .asc)…** and pick
   `classroom-eos.asc`.
   - *You should see:* **Import an Eos show**, on the page **The show's
     files**, with the file filled in. Click **Next**.
3. **Fixture types.** Every channel of this show is a plain dimmer, so
   each is patched as a generic dimmer. Leave **Bring the cue lists into
   the Show Tree as tracking cue lists** ticked and click **Next**.
4. **Special circuits.** The import read each channel's Eos label.
   - *You should see:* **11 House Lights** as **House**, **12 Work
     Lights** as **Work**, **13 Relay Stage Power** as **Relay (power)**
     and **14 Backstage Blue** as **Backstage / hall**, and also **5 Cyc
     Blue** as **Backstage / hall**.
5. Channel 5 is the cyc's blue: a stage light. In its row, change
   **Role** to **Normal**. Then **Finish**.
6. The next window lists everything that comes in, all ticked. Click
   **OK**.
   - *You should see:* **Import complete**, with what was read and what
     landed. **Show Details** lists the output rules each kind of circuit
     got.

### Find it all

7. **Fixtures** tab (⇧⌘2).
   - *You should see:* ten fixtures named after their Eos channels (*Ch 1
     FOH Left* … *Ch 14 Backstage Blue*), and groups in a folder **Eos**:
     **Front**, **House &amp; Work**, **Power relays** and **Backstage**.
     The house, work, relay and backstage fixtures carry the amber
     output-rules badge.
8. **Show Tree** tab (⇧⌘4).
   - *You should see:* the cue list with the Eos cue numbers: **1
     Preshow**, **2 House to half**, **3 House out**, **4 Opening**. It is
     a tracking list, as on Eos.
9. **Virtual Console** tab.
   - *You should see:* a frame **Eos subs** with one fader, **Sub 1
     Work**.

### Save

10. **File → Save As** → `Lesson3-10-yourname.qlcc` in **Documents →
    qlcconsole**.

## Stuck?

| Problem | Try |
|---|---|
| **Import** is greyed out | You're in Operate. Click **Design** at the top right. |
| You finished the wizard with *Cyc Blue* still a backstage circuit | Right-click **Ch 5 Cyc Blue** in the Fixtures tab → **Output Rules…** and untick the rules. |
| No cue list in the Show Tree | The tick on the **Fixture types** page was off. **File → New** and import again. |

## Done early? (Extension)

1. If you brought your own Eos show (exported as USITT ASCII), import it
   into a new show. Check its special circuits row by row, then read
   **Show Details** in the report: what didn't come across?
2. In the Show Tree, compare cue 3 with what you'd expect on Eos: which
   channels track into it?
