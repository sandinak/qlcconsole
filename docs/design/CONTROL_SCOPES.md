# Control scopes: when a mapped control is live

Status: **proposal** (2026-11-24), for Branson's board. No code yet.
Asked: *"We need to future-state manage mapping controls to settings"* — does
a control mapped in a cue work only in that cue, does one on a tracked layer
stay armed while it tracks, what about effects and reuse, collisions, and
common controls (intensity, RGBW...).

Short answer: **a mapping lives where you attach it and works while that thing
is live.** Most of that model is already built for **hits** (a hit's *place*
decides when it fires, and the narrowest place wins). This proposal stretches
the same rule to the Control Map and to faders, instead of adding a new model.

---

## 1. What hears a control today

Seven parts of the program listen to input, each on its own; none knows about
the others.

| Listener | Code | When it acts | Saved |
|---|---|---|---|
| **Control Map** (triggers) | `engine/src/controlmap.{h,cpp}`; connects to the input at `controlmap.cpp:261` | Scene/effect: always, both modes (`controlmap.cpp:1231`). Cue list/cue: Operate, or Design when armed (`:1234`). Hit: only where its place is live (`:1237-1241`). Keys: scene/effect Operate only (`:1117`). | show, `<ControlMap>` |
| **Emergency stop** | `controlmap.cpp:672-697` | Always, before anything, learn included | show |
| **Encoder bank** | `engine/src/encoderbank.{h,cpp}`, `:367` | Turns the Build selection, Design only (rule 8) | show, inside `<ControlMap>` (`controlmap.cpp:1563`) |
| **VC external input** | `ui/src/virtualconsole/*` | As in QLC+; a slider can *catch* (below) | show |
| **Masters** (Speed, Grand, group masters) | `engine/src/speedmasters.cpp`, `doc.h:1259` | Both modes; a master is playing, not editing | show |
| **PMJ overlay** | `ui/src/pmjoverlay.{h,cpp}`, `:173`; roles in `engine/src/controlsurface.h` | Faders follow the palette focused in the Look Editor (`pmjoverlay.h:104-111`); refused in Operate (`pmjoverlay.cpp:218`) | profile, not show |
| **Pad Run / Companion** | `engine/src/padrun.h:53-100`; `docs/features/show-control.md:809-836` | Pad: 2-control arm, LT+button, four-together stop. Companion: Allow GO + PIN, ALL OFF held 1 s | show / computer |

Also listening: programmer bound from a profile (`programmercontroller.cpp:189`)
and channel groups (`channelsgroup.cpp:162`).

### How conflicts are handled today

- **Inside the Control Map: one control, one thing.** `setBinding()` replaces a
  binding on the same control *or* for the same target
  (`controlmap.cpp:115-137`). The Learn dialog warns first: *"It fires … now;
  OK moves it here."* (`ui/src/controllearndialog.cpp:274-277`).
- **The stop beats everything.** Handled before learn and before any binding
  (`controlmap.cpp:672-697`); Learn refuses the stop's control
  (`controlmap.cpp:706-712`).
- **Keys** are refused at Learn when a Virtual Console widget or an app
  shortcut has them (`ui/src/keytriggers.cpp:151`, `:211-220`), and don't fire
  while the VC or an editor has the keyboard (`keytriggers.h`).
- **Across listeners: nothing.** A MIDI note can be a trigger, a VC button and
  an encoder-bank button at once; all three act. The help admits it:
  *"Nothing warns you yet when two of them take the same control, so give each
  control one job."* (`docs/features/control-surfaces.md:21-23`).
- **The one real precedence rule is hits.** A hit lives at a place
  (`ShowTree::HitScope { Global, Production, List, Cue }`,
  `engine/src/showtreemodel.h:170-178`) and is armed while that place is live
  (`ShowTreeController::isHitArmed`, `showtreecontroller.h:1164`). A VC button
  holding several hits fires the **narrowest armed** one — cue > list >
  production > everywhere (`armedHit()`, `showtreecontroller.h:1167-1170`;
  `hitOverrides()`, `:1177-1181`); several at the same place step or fire
  together (Q157, `armedTier()`). Pad Run (`padrun.h:114-119`) and Companion
  follow the same rule, and the button's properties already say it in words:
  *"HIT 1 fires Blackout everywhere, except Strobe in cue 2.3"*
  (`docs/features/show-tree.md:2648-2660`).
- **Soft takeover exists only on VC sliders** ("catch values",
  `vcslider.h:386`, `vcslider.cpp:2439-2453`). Nothing else picks up.
- **Feedback** is one status source per control: `ControlMap::Lamp`
  (off / dim ready / bright running / blink held / stop, `controlmap.h` and
  `lamp()`), `ControlSurface::State` for the PMJ (`controlsurface.h`), Companion
  colours (red on, blue armed).

**What that means for the questions:** "only for that cue" already exists for
*buttons* — as a hit placed under the cue. It does not exist for faders, for
layers, or for Control Map triggers on scenes and effects (those are always
armed). Collisions are handled only inside one listener.

---

## 2. The proposed scope model

A mapping gets a **place**, as a hit has. The place is where you learned it
(right-click the cue, the layer, the effect ▸ Learn Control…), shown and
changeable in the Control Map table. No new top-level model: places are the
Show Tree's own, and the table stays the one list (STRATEGY principle 3).

| Place | Armed when | Disarmed when | Saved | Example |
|---|---|---|---|---|
| **This cue** | its cue is the live cue (or a sub-cue of it) | the next GO leaves it | show, on the binding (as a hit's `ScopeOf=`) | Fader 1 rides the follow spot's level in 2.3 only |
| **This layer** | the layer is on stage: set here or tracked in | a cue Releases it (armed in that cue, then off) or it is Untracked (never armed in that cue) | show | Encoder rides "Truss wash" colour from 1.1 until 3.0 releases it |
| **This list / production** | that list runs / that production is current | another list / production takes over | show | Song-specific busk faders for "Everybody Scream" |
| **This effect** | that effect is playing, wherever it plays | it stops | show | Speed knob on "Truss Wave" — every cue that uses it |
| **This effect in this cue** | the effect plays *and* the cue is live | either ends | show | Same knob, only in 2.3 |
| **Everywhere** (show-wide) | always (subject to Operate/Design rules) | never | show | Grand Master, group masters, GO |
| **This computer** | always, on this computer, any show | never | computer settings | Your own desk layout on your own laptop (open Q7) |

Rules that go with it:

1. **The GO moves the controls, not the fade.** On GO, the leaving cue's
   controls disarm and the new cue's arm at once, so a fader never belongs to
   two cues mid-crossfade. A button held down keeps what it pressed until its
   release (rule 9, `control-surfaces.md:74-77`).
2. **A reusable layer or cue carries its controls.** A control on a reusable
   (`ShowTree::ReusableLayer`, `showtreemodel.h:526`) is on every use, as its
   looks are; "This use only" is the cue place.
3. **An effect reused is one effect.** A control on an effect arms wherever it
   plays (as VC's "This effect" slider does, `docs/features/effects.md:935-967`).
   Want it in one song only? Learn it on the cue (or the effect inside the cue).
4. **What a scoped fader set is live, not stored** — it goes when the control
   disarms, as an effect slider's value goes when the effect stops
   (`effects.md:958-964`). **Store** keeps it into the cue. (open Q4)
5. Released / Untracked use his words (Q222): **Release** = in this cue, then
   lets go; **Untrack** = never reaches this cue (`showtreecontroller.h:775-799`).

---

## 3. Collisions and precedence

**At Learn** (the same dialog everywhere):

- **Same control, same place → refused** with the owner named: *"Fader 1
  already rides Truss wash in cue 2.3. Remove that first, or learn it here at
  a different place."* For buttons only, offer *"Step through them"* (the Q157
  behaviour hits already have).
- **Same control, different place → allowed**, and the dialog says the
  result in one line: *"Fader 1: Grand Master everywhere, except Truss wash
  level in cue 2.3."* — the sentence VC hit buttons already write.
- **Same control, another listener** (a VC widget, the encoder bank, a
  master input, the PMJ overlay) → **refused**, naming it — as keys already are
  against VC (`keytriggers.cpp:211-220`). Closes the gap in
  `control-surfaces.md:21-23`.
- **The stop's control → refused** (already, `controlmap.cpp:706-712`).

**Live — narrowest armed place wins:** cue > layer > list > production >
effect > everywhere > this computer. Layer sits under cue because a tracked
layer spans several cues; an effect sits low because a reused effect spans the
show. The stop and the pad's own rules sit above all of it (section 6).

**"Who has this control now"** — in **Connections ▸ Show control & sync ▸
Control surfaces** (`ui/src/controlsurfacespanel.{h,cpp}`, rows at
`controlsurfacespanel.cpp:69-109`). Add one row, **"Who has it now"**: move a
control and it names its current owner and place (*"Fader 1 → Truss wash level
(cue 2.3; overrides Grand Master)"*), with each binding of it listed under.
The panel keeps reading, not owning (its own rule, `controlsurfacespanel.h`).
The Control Map table gets a **Place** column beside the **Armed** it already
shows for keys (`other-improvements.md:1351`). The footer says a refused press, as it
does now (rule 5).

---

## 4. Common controls: intensity, RGBW, pan/tilt, beam

Two kinds, and the Operate/Design line decides which:

- **Editing the selection** (encoder bank, PMJ faders and encoders): Design
  only. This is an edit, and rule 8 (`control-surfaces.md:67-70`) and UX-49
  *Operate refuses edits from every input* already say no in Operate. These
  **act on what is selected** (Build / Workbench / Look Editor), never on a
  fixed group, and **persist** as the device setup (`encoderbank.h`), saved with
  the show today. The static-core vs. pages split in
  `docs/design/CONTROL_SURFACE_DESIGN.md:21-30` stays.
- **Playing a level** (Grand Master, group masters, speed masters, effect
  sliders): both modes, act on a **fixed** thing, place *everywhere* unless
  learned on a cue/layer. A group-master fader is the safe "intensity of the
  washes" control in Operate.

So, a challenge to the first answer: *"common controls act on the current
selection"* holds in Design only. In Operate there is no selection to edit;
an RGBW fader in Operate has to be bound to a fixed layer or group (a scoped
"ride" of that layer's look, live, not stored), or it is refused.

Persistence: show-wide common controls persist with the show (as now).
Per-computer setup is open Q7.

---

## 5. Pick-up and feedback

- **Pick-up (soft takeover).** When a fader's owner changes (a GO, a bank
  page, a new selection), the fader does nothing until it passes the current
  value — VC's "catch values" (`vcslider.cpp:2439-2453`) made the default for
  every scoped fader. A **motorised** fader (or a ring, `encoderbank.h`
  `rings`) is sent to the new value instead. Encoders need neither (relative).
  The Control Surfaces row shows *"Fader 1: move up to 62 % to pick up"*.
- **Feedback** lights only what is armed now: the existing Lamp states
  (`controlmap.h`: dark = a press would do nothing, dim = ready, bright =
  running, blink = held) extended to scoped controls, so a cue's buttons go
  dark on the GO that leaves it. One status source (operator rule 5,
  `padrun.h:79-80`): the lamp, the table, Companion and the "who has it" row
  all read one answer. Stream Deck keys get the same via Companion feedbacks.

---

## 6. Safety

1. **E-stop above everything**, unscoped, never refused, never part of a
   collision (already: `controlmap.cpp:672-697`, `lamp()` always lit).
2. **The pad keeps its own rules**: 2-control arm, trigger + A/B, no lists
   from the pad, one status source (`padrun.h:78-100`). Scoped controls never
   arm the pad and never pick a list.
3. **Operate refuses edits** from every input (UX-49): scoped *edits* are
   Design-only; scoped *plays* (levels, rides, hits) work in Operate.
4. **Design follows today's arming**: a scoped control fires in Design only
   while its place is armed (the Run tab's list or the Show Tree selection,
   `controlmap.cpp:1172-1224`), so a brushed fader in a rehearsal can't move
   the stage from a cue you aren't looking at.
5. **Letting go is always allowed** (rule 4) and a disarmed fader's live
   value returns to the cue's own — never a jump to the fader's position.
6. **Nothing refused silently** (UX-28): every refusal names the owner.

---

## 7. Questions for Branson

**Q-A. Do mappings get a place (cue, layer, effect, everywhere)?**
A: yes, the place model above. B: no, keep "always armed" and use hits for
per-cue buttons. C: places for buttons only (hits already do it), not faders.
*Recommend A.* ROI: high — the busk-fader-per-song ask; one rule students
already learn for hits. Size: M (place on a binding + arming check; reuses
`isHitPlaceLive`).

**Q-B. Two mappings on one control at different places — allowed?**
A: allowed, narrowest armed wins (as hits). B: refused, one job per control.
C: allowed for buttons, refused for faders. *Recommend A*, with the one-line
sentence shown at Learn. ROI: high for song-specific desks. Size: S once Q-A
exists.

**Q-C. A control already used by another listener (VC, encoder bank, master)
— what happens at Learn?**
A: refused, naming the owner. B: warned, allowed. C: unchanged (both act).
*Recommend A* — the gap the help admits today. ROI: high (prevents a show
bug). Size: S–M (a "who listens to this control" query across listeners).

**Q-D. A scoped fader's value when it disarms: drop or keep?**
A: drops (live), Store keeps it into the cue. B: stays until something else
sets it. C: a per-binding choice. *Recommend A* — matches effect sliders; the
cue plays as written. ROI: medium. Size: S.

**Q-E. Pick-up by default?**
A: pick-up on for every scoped fader, motorised faders jump. B: off by
default, a tick per binding. C: jump always. *Recommend A*. ROI: high
(no jumps on stage). Size: S (VC's code exists).

**Q-F. Common controls in Operate: what do RGBW/pan/tilt faders do?**
A: refused in Operate (as now), group masters for levels. B: a "ride"
bound to a fixed layer, live, not stored. C: act on a selection in Operate too.
*Recommend A now, B later* — C breaks UX-49. ROI: medium. Size: A none, B M.

**Q-G. Where does a desk layout live: the show or this computer?**
A: the show (as now; travels with the file). B: this computer (your desk, any
show). C: both — device setup per computer, mappings per show. *Recommend C*.
ROI: medium (touring with your own desk). Size: M.

**Q-H. Does a control on an effect arm wherever the effect plays?**
A: yes, everywhere it plays; learn on the cue to narrow it. B: only where it
was learned. *Recommend A* — one effect is one effect, like a reusable layer.
ROI: medium. Size: S on top of Q-A.

**Q-I. Same place, two buttons' worth of things on one button?**
A: refused. B: step through (Q157, as hits). C: fire together.
*Recommend B for buttons, A for faders.* ROI: low–medium. Size: S.

---

## 8. Phased plan (smallest valuable first)

1. **Collision check at Learn, across listeners** (Q-C). A "who listens to
   this control" query over the Control Map, encoder bank, VC inputs, masters
   and the PMJ overlay; Learn dialogs refuse with the owner's name; a
   **Who has it now** row in Control Surfaces. No model change. Size S–M.
2. **Pick-up for every fader input** (Q-E): VC's catch logic made shared,
   default on for masters and new bindings; motorised faders sent the value.
   Size S.
3. **Places for Control Map buttons** (Q-A, Q-B, Q-I): a binding gets
   `Scope=`/`ScopeOf=` (as hits; QLC+ 4 skips the element), arming through
   `isHitPlaceLive`, narrowest wins, lamps follow. Size M.
4. **Layer and effect places + scoped faders** (Q-D, Q-H): armed from the
   resolver (layer on stage: Own/Tracked/Included, not Base), GO moves the
   controls, live values, Store. Size M–L.
5. **Per-computer desk setup** (Q-G) and Operate rides (Q-F B). Size M each.

Each phase ships with help in `docs/features/control-surfaces.md`, a course
note, gate tests (a test per rule above that fails on today's code) and a
CHANGELOG line.

## 9. Decisions (2026-11-24)

Branson took the recommendation on every question:

| Board | Here | Decision |
|---|---|---|
| Q295 | Q-A | Mappings get a place (cue, layer, effect, everywhere), armed only while it is live: the rule hits follow. |
| Q296 | Q-B | Two mappings on one control at different places are allowed; the narrowest armed one wins (cue > layer > list > production > effect > everywhere > this computer), said at Learn. |
| Q297 | Q-C | A control another listener already uses is refused at Learn, naming the owner, with **Move It Here**. |
| Q298 | Q-D | A scoped fader's value drops when its place ends (the GO); the cue plays as written; Store keeps it into the cue. |
| Q299 | Q-E | Pick-up on for every scoped fader; motorised faders are sent the value. |
| Q300 | Q-F | RGBW / pan / tilt faders are refused in Operate for now; group masters for levels. A ride bound to a fixed layer may come later. |
| Q301 | Q-G | Both: the device setup per computer, the mappings per show. |
| Q302 | Q-H | A control on an effect is armed wherever the effect plays; learn it on the cue to narrow it. |
| Q303 | Q-I | Two things at the same place on one button step through (as hits); refused for faders. |

Build order: §8, phase 1 first.
