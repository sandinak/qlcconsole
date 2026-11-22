# Scoped controls and the surface slices after 0.5.2

Board Q269-Q273, decided 2026-11-18. Written against
`release/0.5.2` (`a33678e1c`), 2026-11-22, as the plan for the 0.5.3
control-surface work. The ranked plan it follows is
[CONTROL_SURFACES_AUDIT_2026-11-17.md](CONTROL_SURFACES_AUDIT_2026-11-17.md);
this note says what of it has shipped, what the scoped-assignment model is
when it is built, and which slice is being built now.

---

## 0. Which slice is first, and why it is not the scoped one

The audit's suggested order is **CS1 + CS2 -> CS3 -> CS4 -> (bench day: CS5,
CS9) -> CS6 -> CS7 (scoped buttons) -> CS8, CS10**, and board Q268 made CS1
"the first 0.5.x item". CS1 and CS2 shipped in **0.5.1** (§2 below), so the
first unbuilt slice by the audit's own ranking is **CS3, the run lamp from
the one status source** (ROI 13 against CS7's 5.3).

Two reasons not to jump the queue to CS7:

1. CS7's own LED rule (audit §5.3) is *"each bound control shows its
   **resolved** binding: dark = nothing here, dim = assigned and idle,
   bright = running"*. Today a Control Map light is one bit: `sendFeedback(b,
   bool on)` with `on` = "the bound function is running"
   (`engine/src/controlmap.cpp:1194`). There is no dim, no blink and no
   "nothing here now", so a scoped binding would have no way to say that
   its place is not live -- the first thing a student needs to see from a
   bank of scoped pads. CS3 builds that vocabulary; CS7 then adds one more
   reason for a lamp to be dark.
2. CS3 is the slice that makes **today's** desk honest about the rules 0.5.1
   shipped. A press is now refused when its place is not live (Q269) and the
   refusal is said on the footer -- but the LED on the desk still says
   nothing, so the operator finds out by pressing.

So: **CS3 is built on `feat/053-scoped-controls`** (§3), and the scoped model
below is written out in full for whoever picks up CS7 next. Nothing of CS7 is
built here.

---

## 1. The scoped-assignment model (Q272), as it would be built

Owner, verbatim (TODO.md): *"a control can be assigned to a setting tied to a
layer, a cue, a song, a show, etc. -- the mapping belongs to a scope and is
live when that scope is (falling back to the wider one)"*. First slice:
**buttons only**.

### 1.1 Where the mapping is stored

No new model and no new file. A `ControlMap::Binding` gains two fields, named
and stored exactly as a hit's place already is (`ShowTree::Cue::scope` /
`scopeOf`, `engine/src/showtreemodel.h:310-316`):

```cpp
/** Where a binding lives, and so when it is live. From the widest. */
enum class Place { Computer, Show, Production, List, Section, Cue, Layer };

struct Binding
{
    ...
    Place place = Place::Show;      //!< Show: as every binding is today
    quint32 placeOf = 0xFFFFFFFF;   //!< the production, list, section, cue or layer
};
```

Saved on the existing `<Trigger>` element as two attributes an older build
ignores (the same trick `Scope=` / `ScopeOf=` uses, and the same trick the
`Key=` attribute uses):

```xml
<ControlMap RunList="12">
  <Trigger Universe="0" Channel="188" Target="Hit" ID="41"
           Place="Section" PlaceOf="77" Mode="Momentary"/>
</ControlMap>
```

- No attribute = `Place::Show`, so **every show saved before this loads
  unchanged** and a 0.5.2 build reading a 0.5.3 show keeps firing the
  binding show-wide (it becomes wider, never narrower: a control never
  silently gains a narrower job on an older build).
- `Place::Computer` is **not** in the show: it goes in `QSettings`, beside
  the controller jobs (Q250), under `controlmap/computer/<universe>/<channel>`.
  That is Q5's answer -- the GO button of this desk belongs to the desk, so
  any show opened on it has a GO.
- `Place::Layer` is listed for completeness and is **not** in slice 1
  (audit §5.6: a Design-only slice 3; an Operate layer fader needs an
  intensity-override path on the layer's baked scene, which does not exist).

Invariant to keep: today `setBinding()` and `loadXML()` enforce *one control,
one target*. It becomes **one control, one target per place**. A second
binding of the same control at the same place still replaces the first.

### 1.2 How scopes resolve

One function, used by everything that needs an answer (audit §5.4 rule 7):

```cpp
/** The binding a control fires now: of the bindings on it, the live one
 *  with the narrowest place. Null: none is live. */
const Binding *resolvedBinding(quint32 universe, quint32 channel) const;
```

Order, narrowest first: **Layer > Cue > Section > List > Production > Show >
Computer**. A place is live when:

| Place | Live in Operate | Live in Design |
|---|---|---|
| Cue | that cue is live (`isHitPlaceLive(HitScope::Cue, id)`) | the cue is selected in the Show Tree (`isArmed()`) |
| Section | a cue of that song section is live | a cue of it, or the section, is selected |
| List | that cue list is running, or (nothing of its production running) it is the current production's main list | that list is selected, or it is the Run tab's list |
| Production | it is the current production | the same |
| Show | always | always |
| Computer | always | always |

Section is the one new place: `HitScope` has no Section, so the enum above is
a parallel `Place` with the same order, and `isHitPlaceLive` is extended with
a Section case rather than duplicated (`ShowTree::Numbering` already says
what a show's sections are, `showtreemodel.h:186-205`). Q12's answer: in
Design "live" is the **Show Tree selection**, as a computer key already is
(`ControlMap::isArmed()`), so a song's buttons can be tried while it is being
built.

**When it is worked out.** Never per frame, never per press-latency path.
`resolvedBinding()` is a lookup in a per-control list that is rebuilt
(`rebuild()`) on exactly the events that already rebuild the hit arming:
`ShowTreeController::hitArmingChanged` (a list started, stopped or moved on,
the production changed), `Doc::modeChanged`, a Show Tree selection change,
and a binding added or removed. That is audit §5.4 rule 2: **a scope change
never fires anything**, it only changes what the next press will do.

### 1.3 What a scope change does to a held control

Already true, and it is the one piece of CS7 that is **built today**: a press
remembers the binding it pressed. `ControlMap::m_pressed` is a
`QHash<quint64, Binding>` -- *"the binding each held control pressed: its
release acts on it, not on what the control is bound to by then"*
(`controlmap.h`), shipped in 0.5.1 as rule 9 of
[control-surfaces.md](../features/control-surfaces.md) and tested by
`ControlMap_Test::aReleaseActsOnWhatWasPressed`.

So for scoped buttons (audit §5.4 rule 3) the rule holds with no new work: a
Momentary button held while the cue changes under it **releases what it
fired**, not what its control resolves to by then. The slice's job is to
prove it with a scope change in the sweep, not to build it.

Three cases the sweep must cover, none of which fires anything:

| While a button is held, the scope... | What happens |
|---|---|
| narrows (a cue of its list goes live, and a narrower binding now wins) | nothing fires; the held press still owns its old binding; the release stops what it started; the **next** press fires the narrower one |
| widens (the cue leaves the stage) | the same, the other way |
| ends altogether (the list stops) | the release still acts (rule 4, "letting go is always allowed", `surfaceMayFire()` is not asked on a release) |

Faders are rules 4-5 of the audit (catch / soft takeover) and are **not** in
slice 1, because the Control Map has no fader target at all yet (CS6).

### 1.4 LED feedback

Slice 1 adds no new lamp vocabulary: it reuses CS3's (§3 below), with one
more reason for a lamp to be dark.

| What the operator sees | What it means with places |
|---|---|
| dark | no binding of this control is live here (the narrowest live place has none, and no wider one either), or the resolved binding cannot fire now |
| dim | the resolved binding would fire if pressed |
| bright | what the resolved binding fires is running |
| two blinks | the double-GO guard held a command on the resolved binding's list |

The lamp is pushed only when its value changes (CS3 keeps the last value
sent per control), so a production switch that re-resolves 40 pads sends only
the pads that actually changed -- a scope change must not flood a MIDI line.

### 1.5 What GO and the emergency stop are exempt from

Audit §5.4 rule 1, as the learn must enforce it:

1. **The emergency stop can only be bound at `Place::Computer` or
   `Place::Show`.** The Emergency Stop… learn offers no Where combo. ALL OFF
   is never scoped, so no place change can take it away. (Today it is
   show-wide and there is one of it: `removeBinding`/`setBinding` keep a
   single `EmergencyStopTarget`.)
2. **A control holding GO, Back or ALL OFF at any place refuses a narrower
   binding.** The Learn Trigger… dialog refuses it in words -- *"Note 60 is
   GO for this show. A control that GOes cannot also do something else in one
   song."* -- and the refusal is a test, not a comment.
3. **A narrower binding never shadows one of those three.** `resolvedBinding()`
   would not even be asked, because (2) stops the pair from existing; the
   sweep still asserts it from the loaded XML, where a hand-edited file could
   hold both. A file that does hold both keeps the GO and drops the narrower
   one on load, the same way a duplicate control is dropped today.
4. **The e-stop lamp is never scoped dark.** It shows the stop's own state
   (lit while the show is stopped), at every place, in both modes.

GO itself needs no arm on a desk (Q269, shipped): the double-GO guard and the
CS3 lamp cover it. That is unchanged by places.

### 1.6 What slice 1 is, concretely

Buttons only; places Show / Production / List / Section / Cue; one combo
(**Where**) added to Learn Trigger…, defaulting to the narrowest place of the
row it was opened on (Q13: that cue's song section if the list has sections,
else the list); a **Place** column in the Control Map panel, grouped by
place; "Controls here" (read-only, with Learn…) in the Show Tree inspector of
a cue, list or section. No new tab, no new menu item.

Tests that must fail on the code before it: the place x live/not x
Design/Operate x mode sweep; GO/Back/ALL OFF refused a narrower binding (and
dropped from a hand-written file); a held press across a scope change
releasing to its own binding; a counter proving resolution is recomputed on a
GO and not per frame; one control with three bindings firing the narrowest
live one.

---

## 2. What 0.5.1 already shipped for Q270 (and Q269)

Read before building anything in this area. The whole of the audit's CS1 and
CS2 is in 0.5.1 (CHANGELOG [0.5.1], "Control surfaces are safer").

| Q270 / CS1 piece | Shipped? | Where |
|---|---|---|
| One **learned button** that is ALL OFF | **Yes** | `ControlMap::EmergencyStopTarget`, `emergencyStop()`; the **Emergency Stop…** button under the Control Map table (`ui/src/controlmappanel.cpp`) |
| A **second press confirms** at learn time | **Yes** | `listenForButton()`, `confirming()` -- press, release, press again; `ControlMap_Test::theStopIsLearnedOnAButtonWithASecondPress` |
| **Never a fader, knob or encoder** | **Yes** | `isButtonControl()` (by input profile, else by what the MIDI channel number is); a control that has been *seen* moving like a fader is flagged loudly and still fires (`stopProblem()`, `m_estopFlagged`); a computer key is refused outright |
| **ALL OFF through the one path** | **Yes** | `ShowControl::run(AllOff, RouteControlMap)` |
| **Never held** (double-GO guard) | **Yes** | `ShowControl::isGuarded()` excludes `AllOff`; `ControlMap_Test::theStopIsNotHeldByTheDoubleGoGuard` |
| **Never a toggle, never "held"** | **Yes** | every press is ALL OFF, the release does nothing, repeats inside `m_estopDebounceMs` are one press |
| Works **while a learn is listening**, after a replug, after a lost release | **Yes** | the independent review's findings, all tested (`theStopWorksWhileALearnIsListening`, `theStopSurvivesAReplugAndRepeats`, `aLostReleaseAtLearnTimeDoesNotSwallowTheStop`, `aDeviceThatDropsItsZerosStillStops`) |
| **Companion ALL OFF, hold 1 s** | **Yes** | `tools/companion-module/`, the ALL OFF preset; works with *Allow GO from the phone Run page* off |
| Said **on the screen** when it cannot work | **Yes** | `stopProblem()` in red on the stop's own row, one footer line |
| Help + hardware checklist | **Yes** | [docs/features/control-surfaces.md](../features/control-surfaces.md) (CS2), "Check it on the real device" |

And of Q269 / CS1a: a MIDI or OSC press now follows the mode and the hit's
place (`surfaceMayFire()`, `refusalFor()`, `blocked()`), a dedicated GO needs
no arm, a release acts on what it pressed, a controller that goes away lets
go, and the PMJ overlay hears only a universe patched with the PMJ Black 1
profile and refuses edits in Operate (CS1c).

**What is left of Q270: nothing in software.** What is left is the bench:
RIG_TEST_PLAN §L and the checklist in control-surfaces.md have never been run
on a real desk (audit F9), so every claim above is "built and tested
offscreen, not yet pressed on hardware".

---

## 3. CS3, the slice built here: the run lamp from the one status source

### 3.1 What it was, what it is

Before: `sendFeedback(b, bool on)`, `on` = the bound function is running.
One bit. The audit's F7: *"a Control Map cue-list LED is lit while the
list's chaser runs. It never shows standby, a double-GO hold, the e-stop
(`Stopped`) or Pad ARMED."*

After, five states, each read from the one place that already owns it --
never worked out in the Control Map:

| Lamp | What it means | The one source |
|---|---|---|
| **Off** (0) | pressing does nothing now: the target is gone, the press would be refused (not armed in Design, a hit's place not live, a Stop-only on something idle), a cue list with nothing left to GO, or the show is stopped | `targetExists()`, `surfaceMayFire()` (the Q269 rule itself), `ShowControl::standbyStep()`, `OutputRuleKeeper::emergencyStopped()` |
| **Standby** (dim, 30 -> MIDI velocity 15) | a press would fire it | `surfaceMayFire()` + `standbyStep()` |
| **Running** (bright, 255 -> velocity 127) | what it fires is running | `Function::isRunning()` (as before) |
| **Held** (two blinks, then back) | the double-GO guard held a command on this binding's cue list | `ShowControl::commandHeld` |
| **Stopped** (lit) | the emergency stop's own lamp, after an ALL OFF until the next GO | `OutputRuleKeeper::emergencyStopped()` |

Dim 30 is velocity 15, which is the level the PMJ's own idle template lights
its 54 LEDs at (`resources/miditemplates/PMJ-Black-1-idle.qxm`), so a PMJ
reads "ready" the same way its board does when QLC+ has said nothing yet.

### 3.2 Devices whose LEDs are colour-coded, not dimmable

An APC's clip pads take a *colour index* in the velocity (1 green, 3 red, 5
yellow on an mk1), not a brightness, so a dim level there is not dim -- it is
another colour, or nothing. Nobody has pressed one of these boards since the
Build era (audit F9), so the lamp has an out that needs no hardware to use:
**Lights ▸ "Dim a control that is ready"**, a checkbox under the Control Map
table, saved with the show (`<ControlMap Lights="onoff">`). Off, Standby
sends 0 -- exactly what 0.5.2 sends -- and Running, Stopped and the held
blink are unchanged. The default is on.

### 3.3 Cost

No new timer on the DMX path and nothing per frame. The lamp is recomputed
on the events that already exist (a function started or stopped, a GO, a list
stopped, the arming changed, the mode changed, an e-stop struck or ended, a
device plugged in) and pushed **only when the value changed** (one `QHash`
of the last value per control), so a mode switch with 40 bindings sends only
what moved. The held blink is a two-blink `QTimer` chain owned by the
Control Map, armed only by `commandHeld`.

Every one of those signals can arrive on a plugin's or the MasterTimer's
thread, so each is connected `Qt::QueuedConnection`: the class stays
GUI-thread only, as its header says.

---

## 4. ROI for the slices still to build

ROI = Value x Fit / Effort (days) x risk, as the audit and STRATEGY §3 do.
CS1 and CS2 are shipped and dropped from the table.

| Rank | Slice | V | F | Effort | Risk | ROI | Note |
|---|---|---|---|---|---|---|---|
| 1 | **CS3 run lamp from the one status source** | 4 | 5 | 1.5 | L | **13** | **built here.** Standby / running / held / stopped on the desk; a dark lamp now means "pressing does nothing" |
| 2 | **CS9 Windows surfaces** (Q7) | 2 | 4 | 0.5 | L | **16** | bench-gated: one Win10 VM pass with a USB MIDI desk, then a help line or a refusal. Cheapest thing on the list |
| 3 | **CS4 who hears this control** | 4 | 5 | 2.5 | L | **8** | Show Review clash check over Control Map / encoder bank / speed masters / PMJ; the learn dialog saying "CC 11 is also Encoder bank 1"; Test Surface… log. Needed before the PMJ retires (two consumers on Enc 1-2 today, F6) |
| 4 | **CS5 encoder presets made true** | 3 | 4 | 1 | L | **12** | hardware-gated (Q273's boards): fix the X-Touch / APC40 numbers from a trace, add a PMJ preset and an X-Touch Mini input profile |
| 5 | **CS10 retire the VC Programmer Frame wizard** (Q10) | 2 | 4 | 0.5 | L | **16** | one menu item fewer; waits on bench step A5 (does it still work at all) |
| 6 | **CS6 faders on the Control Map** | 4 | 4 | 3 | L | **5.3** | GM, group master, speed master Rate, with catch (Q6). The first slice of scoped *faders* needs this first |
| 7 | **CS7 scoped controls, slice 1** (Q272, §1) | 4 | 5 | 3 | M | **5.3** | buttons only; wants CS3's lamp vocabulary and CS4's clash warning under it |
| 8 | **CS8 PMJ onto the Control Map** (Q271) | 3 | 4 | 2.5 | M | **3.8** | ~800 lines of overlay + ControlSurfaceEngine retire onto default bindings + the CS5 preset. Wants CS4 (two consumers) and CS5 (the preset) first |
| 9 | CS7 slice 2: scoped faders (rules 4-5) | 3 | 5 | 2.5 | M | **4.8** | catch on a scope change; needs CS6 |
| 10 | CS7 slice 3: layer scope (Design only) | 2 | 4 | 2 | M | **3.2** | an Operate layer fader needs an intensity-override path on the baked scene: its own ROI line, not this one |

Suggested order after CS3: **CS9 and CS10 on the next bench day** (both are
half a day and both are gated on one real-hardware answer), then **CS4**,
then **CS5 + CS8** together on the PMJ, then **CS6 -> CS7**.

---

## 5. Decisions wanted

| # | Question | Recommendation |
|---|---|---|
| D1 | CS3 before CS7, as the audit ranks, with the scoped model written out here? (This note's §0.) | **Yes.** CS7 without CS3's lamp has no way to show a scoped pad that its place is not live |
| D2 | Dim = MIDI velocity 15 for "ready", with a per-show *Dim a control that is ready* switch for colour-coded pads? (§3.2) | **Yes**, and bench step 11 settles it on the PMJ and the APC |
| D3 | Does a bound control's lamp going **dark** when a press would be refused read as "broken" to a student, or as "not now"? | Keep dark. It is the only thing that teaches the Q269 rule without a press, and the footer says why on the press |
| D4 | `Place::Computer` in QSettings (Q5), so this desk's GO follows the desk and not the show | **Yes**, when CS7 is built; nothing stored for it yet |
| D5 | Section as a place needs `isHitPlaceLive` extended, which is also the hit resolver. Extend it, or a parallel `Place` resolver? (§1.2) | **Extend it.** Two resolvers would disagree the first time a section moved |
