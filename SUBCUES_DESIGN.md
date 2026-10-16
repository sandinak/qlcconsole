# Sub-cues — design

*Status: proposal for review (2026-10-15). Branch `feat/sub-cues`.*

A cue that has cues inside it: cue 4 brings up a look, and **4.1, 4.2, 4.3**
step through changes within it — on a second button, **Sub GO** — while the
main **GO** still goes to cue 5. Today the same thing takes a separate chase,
switched on at cue 4 and off at cue 5, stepped from its own Virtual Console
widget, with nothing in the cue list to show it is there.

Decisions taken (2026-10-15):

- **Any chase used as a step gives sub-cues.** A step whose function is a
  chase lists that chase's steps under it. No new kind of step, and a chase
  can still be used elsewhere.
- **The main GO goes to the next main cue.** Sub-cues left in cue 4 are
  dropped when GO goes to 5. Sub GO is the only way through 4.1, 4.2… (as
  QLab's group cues: two buttons, each doing one thing).
- **Sub-cues are numbered `<parent>.<n>`** (4.1, 4.2) unless the inner step
  has a cue number of its own. Show control can fire them by number; a main
  cue that is itself numbered 4.1 wins over sub-cue 1 of cue 4.

---

## 1. The model

`ChaserStep` gets one setting, **how its sub-cues advance**:

| `subCues` | Means | Default for |
|---|---|---|
| `Auto` | The inner chase runs on its own timing, as today | every step in an existing show (nothing changes when one is opened) |
| `SubGo` | The inner chase holds each step until **Sub GO** | a chase step the user switches to it |

Only meaningful when the step's function is a chase (or a sequence); ignored
otherwise. Saved as an attribute on `<Step>` (`SubCues="SubGo"`), absent for
`Auto`, so older files read and write unchanged.

One level only: a sub-cue's own chase steps (4.1.1) play on their own timing.
Deeper nesting is out of scope (§8).

## 2. Playback

With cue 4 a `SubGo` chase step:

- **GO to cue 4**: the inner chase starts on its first playable step (skipped
  steps passed over, as GO does) and **holds** there — its holds are not
  counted while it is a sub-cue list. Fades are the inner steps' own.
- **Sub GO**: the inner chase's next playable step, with that step's fade.
  At the last sub-cue, Sub GO does nothing (the footer says *end of 4*).
- **Sub Back**: the previous sub-cue.
- **GO** (main): cue 5, with cue 5's transition; the inner chase stops with
  cue 4 as any step's function does.
- **Back** (main): cue 3; the inner chase stops.
- **Auto-follow on cue 4** (its hold runs out): goes on to cue 5, sub-cues
  left or not — the hold is the operator's decision about cue 4 as a whole.
- **GO on the beat** (quantize) applies to Sub GO as to GO, from the same
  chase's setting.
- **Speed masters**: the inner chase keeps its own assignment, inherited from
  the step as today.
- **Jumping to a sub-cue** (by number, §4, or a click in the cue list): plays
  its parent cue if that isn't the one playing, then that sub-cue.

The engine side: `ChaserRunner` already starts a step's function; for a
`SubGo` step it starts the inner chase *held* and keeps a handle to it. Sub
GO / Sub Back become `ChaserAction`s on the **outer** chase (`ChaserSubNext`,
`ChaserSubPrevious`, `ChaserSubSetStep`) that the outer runner passes to the
inner chase's runner. The inner chase never needs to know it is nested,
apart from "hold, don't count".

## 3. What you see

**Chaser editor.** A step with a chase as its function gets a disclosure
triangle; open, its steps are listed indented under it — cue number (derived
ones greyed), name, fades, hold — read-only, with **Edit chase…** to change
them where they live. The step's **Sub-cues** column: *Own timing* / *Sub GO*.

**Virtual Console cue list.** The same nesting; the playing sub-cue is
highlighted inside its parent. A **Sub GO** button (and Sub Back) appears on
a cue list that has any `SubGo` step; a sub-cue can be clicked to jump to it.

**Footer.** With sub-cues left in the cue playing, "Next:" reads
**Sub: 4.2 · Next: 5**; its click menu gains **Sub GO** and **Sub Back**.

**Shortcuts.** GO is ⌘↩ and Back ⌘⇧↩ today; **Sub GO ⌥⌘↩** and
**Sub Back ⌥⌘⇧↩**, app-wide like GO.

## 4. Show control (QLab, MSC, OSC)

- **By number**: `/cue/4.2/go`, `/go 4.2`, MSC GO 4.2 — looked up in the list
  as now (a main cue numbered "4.2" wins); if none, `4.2` is read as sub-cue
  2 of cue 4 (or the sub-cue whose own number is "4.2"), and played as in §2.
- **Sub GO without a number**: OSC `/subgo` and `/subback` (our own addresses,
  documented; QLab sends them as plain Network cues). MSC has no sub-GO
  command; a numbered MSC GO covers it.
- `LOAD 4.2` puts the sub-cue on standby; Next shows it.

## 5. Step-based effects (later phase)

The same Sub GO for effects that move in steps (a chase effect, a step
pattern): a look's effect can be set to **step on Sub GO** instead of its
rate, and Sub GO advances it one step. Separate phase (P4), once sub-cues
work for chases; needs an `inputs.step` signal in the effect-script API and a
flag on the effect palette.

## 6. Phases

| Phase | Scope | Touches |
|---|---|---|
| P1 | `subCues` on the step (XML), held inner chase, Sub GO / Sub Back actions, numbering (`stepCue` for sub-cues), jump to a sub-cue | `chaserstep`, `chaser`, `chaserrunner` + engine tests |
| P2 | Chaser editor nesting and the Sub-cues column; VC cue list nesting + Sub GO button; footer; shortcuts | `chasereditor`, `vccuelist`, `app` + UI tests |
| P3 | Show control: number lookup into sub-cues, `/subgo`, `/subback`, LOAD | `showcontrol` + tests; docs `show-control.md` |
| P4 | Step-based effects on Sub GO | effect engine, effect palette, scripts |

P1 and P2 can be one branch; P3 waits for the Connections show-control work
(`feat/connections-showcontrol`, which touches the show-control UI) to merge
first, to keep the two apart.

## 7. Tests (P1–P3)

- Engine: GO into a `SubGo` step holds its first sub-cue; Sub GO / Sub Back
  step and stop at the ends; skipped sub-cues are passed over; GO to the next
  main cue stops the inner chase; auto-follow leaves with sub-cues left; an
  `Auto` step behaves exactly as today (regression); XML round trip, absent
  attribute = `Auto`.
- Numbering: derived `4.1` and own numbers; a main cue "4.1" wins; jump by
  number plays the parent first.
- UI: editor nesting and the column; VC cue list highlight and Sub GO button;
  footer text; shortcuts.
- Show control: `/cue/4.2/go`, `/subgo`, MSC GO 4.2.

## 8. Not in scope

- Sub-cues of sub-cues (4.1.1).
- Editing an inner chase's steps inline in the outer editor (Edit chase…
  opens it).
- A chase used as sub-cues in two places playing at once (a function runs
  once; the second start takes it over, as today).
- Sending sub-cue numbers out to QLab (that is "QLab out", after this).
