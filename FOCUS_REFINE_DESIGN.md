# Venue focus and Focus this cue — design (board Q105–Q108; was "Focus calls and Refine")

> **Names (board Q149, 2026-10-25).** In the app Focus mode is now **Quick
> focus**, running the focus calls is **Venue focus**, and Refine is
> **Focus this cue** (on a Build scene, **Focus this look**); a focus-call
> cue is a **venue focus cue**. The classes and files keep their names
> (`FocusSession`, `FocusCallsDialog`, `RefineSession`, `refine.md`...).
> The individual steps are still **focus calls**. Every focus tool is on
> the Lighting Studio's **Focus ▾** menu (section 14).

Branch `feat/focus-calls-refine`, 2026-10-23 (decisions Q109–Q113 built the same night; Q116–Q127 and
the gaps review, Q128–Q136, on 2026-10-24), merged to main. The focus report (Q134), hold B and PS
pads (Q139), the small ideas (Q140) and scramble practice (§13) on main on the night of 2026-10-24.
**Feature freeze** for this area after Q140: bugs and the trial's findings only.

## Try this (about 10 minutes)

Build and open a show with moving heads on the plot, one fixture group of
them, and a stage target. `test-workspaces/help-demo.qxw` has Movers, Pars
and a target called Centre:

```
build/main/qlcconsole -o test-workspaces/help-demo.qxw
```

Save it once under a new name, because a venue's file lives beside the show.
Keys are given in brackets for running without a controller. Plug in an Xbox
pad if you have one.

1. **Set up the focus calls.** Lighting Studio → **Venue focus**. Click
   **+ Call**, tick **Movers**, and set Focus to **Centre**. Add another
   call for **Pars**, which focuses to "where they land". Venue: leave
   **New venue…** with the date. Click **Start focus calls**.
2. **Step and nudge.**
   - **RT / LT** (PgDn / PgUp): step calls.
   - **RB / LB** (] / [): step fixtures. The picked fixture blinks three
     times and the laptop says "Fixture 2 of 8."
   - **Left stick** (arrows): nudge.
   - **D-pad**: up zooms in, down out (, and .); right tightens the focus,
     left loosens it (' and ;).
   - **A** (Return): save.
   - **Y** (G): the whole call. Nudge once, and every head moves.
   - **RS click** (W): **Say the light** — "Mover 2. Call 1 of 2, Movers
     to Centre. Fixture 2 of 8." Hold RS (⇧W) to switch the feedback
     between lights only, voice only, and both; the setting is also on the
     panel.
3. **Venue, not show.** Leave the calls with **Menu** (Esc). In Finder,
   look for `<show>.venues.xml` next to the show: it holds your offsets. The
   show is not marked modified. Reopen Venue focus: **Rename…**, **Apply
   now** (None puts the show's own back), and **Delete** manage venues. Type
   a pan offset in a fixture's properties while a venue is applied: it goes
   into the venue too (Q111).
4. **Focus this cue a cue.**
   - In the Show Tree, add a cue with a layer on **Movers** and a position.
   - On its cue page, click **Focus this cue on the controller…**.
   - Move with the stick (arrows) and the lights follow. **A** (Return)
     keeps the change: one undo step. **Y** (G) goes to one fixture; a move
     there is an exception, a child layer in the tree. **X** (0) clears it.
   - **D-pad**: up zooms in, down out; right tightens the focus, left
     loosens it. **LS click** (P) changes the D-pad to colour and then gobo
     palettes (right: the next, left: the one before).
5. **A tracked layer.** Add a cue after it that sets nothing and Focus this cue
   that. The voice says "Movers, tracked from 1. Keeps go into this cue."
   **S** on the panel switches to keeping in cue 1.
6. **From Build.** Load a scene in Build's Workbench and click **Focus this cue…**
   in its header, or right-click a group or fixture row → **Focus this cue on the
   controller…**. The controller is the same; an exception is that fixture
   out of the palette, with the scene's own values.
7. **A focus-call cue.**
   - Make a "Pre-show" list with a cue that sets the work light. Tick
     **Focus call** on its cue page (the F column says **focus**).
   - Go to Operate and **GO** to it. The light comes up, the voice says
     "Venue focus ready. Press Start to begin.", and the Run tab and footer
     say it is waiting.
   - Press **Start/Menu** (⌘↩) to begin the calls. **Menu** (Esc) ends them
     and the list GOes to the next cue.
8. **The voice on the headsets.** In Venue focus, **Voice to:** sends the
   voice to the output that feeds them. Step quickly through the fixtures:
   the voice never talks over itself, and only the fixture you stop on is
   read out. Hold a D-pad arm: zoom or focus keeps going until you let go.
   Nudge a light: the panel says "Since the save: +2.5° pan" and the plot
   shows a dashed ghost where the saved one lands. Press both bumpers to
   jump to the next light not saved; hold A to save them all, hold B to put
   this light back. A Bluetooth Xbox pad buzzes on a save, twice if another
   light is still unsaved.
   Set **Feedback** on the panel (or hold RS / ⇧W) to *Lights only* (no
   voice), *Voice only* (no blinks) or both. **Say the light** (RS / W)
   speaks either way.
9. **The gaps review (Q128–Q136).**
   - Set a venue's file date back a few days (edit `Modified=` in the
     `.venues.xml`) and reopen: "Still at …?", New venue the default. The
     footer says **Venue: …**. GO a focus-call cue with a venue applied:
     the laptop asks "Still at …?" too, and waiting 8 s makes a new one.
   - In focus calls press **LS** (T): the **target page**. The stick
     moves the call's target and every head follows; **A** keeps it for
     the venue (`<Target>` in the sidecar), **B** puts it back.
   - Switch the pad off mid-move with A held: "Controller lost. Lights
     held." and nothing moves; on again: "Controller back."
   - Set Feedback to Lights only and press Menu with a light unsaved: the
     warning is still spoken, with a slow blink and one long buzz.
   - Hold A: a full second to save all. In Build, a shared palette's
     warning waits for a fresh A, never a hold.
   - Focus this cue in Operate a cue that is not running: refused. On the running
     one: no blink, no dim; GO lets go of what was held.
   - The lamp: blue ✓ / yellow ! / orange, with a count; the Focus this cue panel
     has it too. View + RS (N) says "2 not saved."
   - Swap a corrected fixture's definition: it is flagged **needs focus**
     (panel, plot "!", voice). **Corrections…** lists and clears one.
   - Save the show as `…-0.2`, delete its new `.venues.xml`, reopen: the
     earlier version's venues are offered. Collect for Transfer copies
     the venue file.
10. **The report, practice and the last pad moves (Q134, Q139, Q140).**
   - Type your name in **Focused by:**, save a few lights, then
     **Export focus report…**: a PDF, a web page or a CSV (by the file's
     ending). Each light: call, target and "SL 2.4 · US 3.1", the
     correction, who checked it and when, and the flags.
   - Nudge three lights without saving and **hold B**: "3 lights back to
     their last save." **View**: all three back, still not saved.
   - **View + X** (D): "Spot 3 marked dead."; the plot and Corrections…
     show it, and so does the report.
   - **Click both sticks** (H): "Holding." Nothing on the pad moves
     anything. Again: "Released."
   - The D-pad says "Spot down." / "Flood out." / "Sharper." / "Softer.";
     entering a call says where its target is ("…to DSC, CL, US 3.").
   - Click a light on the plot: the calls go to it.
   - A new venue with **Start from …** ticked starts with the last venue's
     corrections.
   - **Practice (scramble)…**, Medium: every head is off by 2–6°. With no
     rig, watch the beams on the plot land off their marks; focus them back
     and save; press Menu: the score, and the real venue back.
   - With a **DualShock 4 or DualSense** paired to the Mac: it drives the
     tools like an Xbox pad, and its light bar shows the lamp's colour
     (blue saved, yellow this light, orange another), buzzing on a save.

---

**The idea.** Kids already know game controllers, so the controller is the
star here. Two jobs on the stage use one controller layout:

- **Venue focus** align the rig in a new venue. This is calibration, saved
  per venue.
- **Focus this cue** changes a cue's own values from the stage. This is
  programming, saved in the show.

The layout is the same in both, and the same as plain Quick focus. A student
learns it once.

Contents:

1. Decisions this builds on
2. The controller, one map for both tools
3. Telling the operator where they are (lights and voice)
4. Venue focus: data model
5. Per-venue corrections: the sidecar file
6. Focus this cue: scopes, and how each edit is stored in the cue
7. Entry points
8. Mockups
9. Code map
10. Out of scope
11. Decisions (Q109–Q113, Q116–Q140)
12. Status and next steps
13. Scramble practice

---

## 1. Decisions this builds on

| Q | Decision | Where it lands |
|---|---|---|
| Q105 | Corrections are saved for this run **and** to disk, so a restart keeps them. They never overwrite the show file. They are kept **per venue** (a named venue, or a date when unnamed). | §5 |
| Q106 | Focus this cue's scope cycles **Layer → Group → Fixture** on one button. Layer edits the layer's look. Fixture makes a per-fixture exception. | §6 |
| Q107 | Feedback to the operator on stage comes from **the lights** and **the laptop's voice**. On a change, what is selected flashes. Only the current scope stays at full. | §3 |
| Q108 | Design both now and build them together. The controller layout is **the same** in both. | §2 |

Already on main: Q100–Q104. These cover the fixed-light Pan/Tilt aim, Focus
mode for fixed lights, beam/zoom/focus in Quick focus, and the Show Tree's
S/T/R/M/F columns.

---

## 2. The controller: one map for both tools

`engine/src/controllerscheme.{h,cpp}` is the single source of the map:

- `ControllerScheme::verbFor(role)` maps each control to a **verb**.
- `label(tool, verb)` gives each tool's words for that verb.

Quick focus's `FocusSession::controlFor()`, Focus this cue's `RefineSession::apply()`
and the on-screen map (`FocusControlMap::setTool()`) all read it. A control
does the same **kind** of thing in every tool:

- triggers step the big unit;
- bumpers step the small unit;
- Y sets how much you are working on;
- A keeps, B goes back.

| Control | Verb | Quick focus (as before) | **Venue focus** | **Focus this cue** |
|---|---|---|---|---|
| **RT / LT** (triggers) | Next / previous group | — | Next / previous **focus call** | Next / previous **layer** (always Layer scope) |
| **RB / LB** (bumpers) | Next / previous item | Next / previous fixture | Next / previous **fixture** in the call | Next / previous **fixture**, or **group** at Group scope. At Layer scope a bumper picks the first fixture. |
| **Y** | Cycle scope | One head ↔ all heads | **Fixture ↔ whole call** | **Layer → Group → Fixture** (Group only when the layer has more than one group) |
| **Left stick** | Move / nudge (coarse) | Calibration pan/tilt, 8°/s | Calibration pan/tilt, 8°/s. A fixed light moves where it lands. | The cue's **position**, 8°/s |
| **Right stick** | Fine | 1°/s | 1°/s | 1°/s |
| **D-pad ↑ / ↓** | **Spot down** (zoom narrower) / **flood out** (wider): up is in, like a camera. Said once a press (Q140) | Lens (hand zoom: saved) or zoom channel (live only) | same | The cue's **zoom** (on the colour/gobo pages: nothing) |
| **D-pad → / ←** | **Sharper** (focus value up) / **softer** (value down): right is always the value up. Said once a press | Focus channel, live only | same | The cue's **focus**. On the colour/gobo pages: next / previous palette. |
| **A** | Keep | Save the head's offsets ("Spot 3 kept."; in a venue "Spot 3 kept for Lincoln HS."; the pad buzzes, twice if another light is still unsaved) | **Save for this venue** (sidecar, at once); on the target page: keep the target | **Keep in the cue** ("AT spots kept in cue 4."; one Show Tree step; a shared palette or a reusable layer asks first, Q116) |
| **A held (1.0 s, Q132)** | Keep all | Every unsaved head saved ("3 lights kept.") | same | All pending kept ("All kept."). Never answers a warning: only a fresh press does |
| **B** | Revert | Back to saved | Back to saved | **Back to the cue** (drops pending) |
| **B held (0.6 s)** | Revert all (Q139) | **Every** unsaved light back to its last save, in any call, as one undo step (View brings them back, unsaved): "3 lights back to their last save." | same | Back to the cue |
| **LB + RB** | Next unsaved | The next light not saved ("Spot 7, not saved."; "All saved.") | same, in any call | The next change not kept ("All kept.") |
| **X** | Reset | Zero the offsets (in a venue: back to the show's calibration) | same | **Clear the exception** (Group/Fixture). At Layer scope it is the same as B. |
| **View / Back** | Undo | Undo last save (a group save is one step) -- **on View's release** in every tool, and not after a chord | same | Show Tree (or Build) undo, on View's release |
| **View + A** | Keep at the source | — | — | A tracked layer's change kept in the cue it tracks from (Q118); ⇧Return |
| **View + RS** | How many | "2 not saved." (N) | same | "1 not kept." |
| **Menu / Start** | Exit | Ask about unsaved, leave | same. **On an armed focus-call cue (no session yet): begins the calls** (§7) | Ask "Keep what is pending?", leave |
| **LS click** | Alternate, **on release** (so it can chord with RS) | Second reference | **Target page** (Q128): the stick moves the call's target, its heads follow; A keeps (venue), B back. T | **D-pad page**: beam → colour → gobo |
| **RS click** | Say the light | The light's name first, then where; flashed. Spoken whatever the feedback setting | same | same |
| **RS held (0.6 s)** | Feedback | Lights → Voice → Both, said ("Feedback: voice only.") | same | same |
| **LS + RS** (both stick clicks) | Hold (Q140) | Every light holds where it is: nothing on the pad moves anything, no breathing or blinks, "Holding."; again: "Released." H | same | same |
| **View + X** | Mark dead (Q140) | The light being worked marked dead in the venue applied (again: not dead); said, on the panel, the plot's needs-focus marks, Corrections… and the report. Needs a venue. D | same | "Dead lights are marked in the focus calls." |
| View + Menu held | — | Opens Quick focus (as before) | — | — |
| Guide | — | unused | unused | unused |

**A held D-pad arm repeats** (Branson: "when I hold a button on the dpad
it doesn't repeat"):

- The zoom and focus arms step once on the press, again after 0.4 s, and
  then every 80 ms until released. This is `ControllerScheme::HoldRepeat`,
  driven by each tool's controller tick (`press(verb, down)` + `tick()`),
  so Quick focus, Venue focus and Focus this cue (Show Tree and Build) all behave
  the same.
- The bumpers, triggers and buttons step once a press.
- On Focus this cue's colour and gobo pages, left/right step palettes, so they do
  not repeat either.

**Triggers are read as buttons.** They press past 0.6 and release below
0.35, so a trigger resting slightly in does not repeat
(`ControllerScheme::TriggerButton`). On the Xbox profile the triggers are
axes 2 and 5 (`role="lt"` and `role="rt"`). They were already declared;
nothing in the profiles changed.

**One change to plain Quick focus.** Y used to toggle the second reference.
Y is now the scope everywhere, so the second reference moved to the left
stick click, and the keyboard keeps **R**. Plain Quick focus's Y now nudges
and saves *every* head in the list at once. That is useful when a whole
truss trims a little low. A bumper goes back to one head.

**Keyboard equivalents** (the panels and the help list them all):

| Action | Key |
|---|---|
| Calls / layers | PgDn / PgUp |
| Fixtures | ] / [ |
| Scope | G |
| Say the light | W |
| Feedback (cycle) | ⇧W |
| Save all (A held) | ⇧Return (Quick focus) |
| Every unsaved light back to its last save (B held) | ⇧Backspace |
| Hold every light / release (both stick clicks) | H |
| Mark dead (View + X) | D |
| Next unsaved (both bumpers) | U |
| Keep at the source (View + A, Focus this cue) | ⇧Return (Focus this cue) |
| Page (Focus this cue) | P |
| Target page (focus calls) | T (R too) |
| How many not saved (View + RS) | N |
| Keep | Return |
| Back | Backspace |
| Reset | 0 |
| Zoom | . / , |
| Focus | ; / ' |
| Exit | Esc |

**Rumble.** Built (Q126): a short buzz on a keep (two when another light is
still unsaved) and one long buzz (700 ms) for something important (Q131).
On a Mac it reaches pads read through GameController; see §11.

**Two pads (Q135).** The first pad to press Start (on an armed focus-call
cue), or any button for a tool started on the laptop, owns the tools
until they end. Others are ignored, sticks too, and told "Another
controller is in use." The panels name the owner.

**The controller lost (safety).** When the pad the tools read goes away
(main's `inputDeviceDisconnected`, `focusControllerChanged`),
`ProgrammerController::freezeController()` centres both sticks, resets
the A/B/RS holds and a held D-pad repeat and clears the held buttons, so
nothing moves and no keep-all fires: "Controller lost. Lights held."; on
its return "Controller back."

---

## 3. Telling the operator where they are

The operator holding the controller is on stage and should not need to look
back at the laptop.

### Lights

**Flash pattern.** On any change of call, layer, scope or fixture, and on
**Where am I?**, the selected fixtures **blink three times**: 120 ms lit,
120 ms dark, 720 ms in all (`FocusSession::flashLit()` and
`flashDurationMs()`). They then stay steady. A 30 ms timer drives the blink
through the same OutputLayer the tool already writes, so nothing else in
the show is touched.

**A warning's blink (Q131).** Warnings, refusals, the controller lost and
the venue asked get one slow long blink of the selection (dark 800 ms,
then lit), unlike the selection's three quick ones and a save's double.

**The unsaved light breathes (gap 10).** While the light being focused is
not saved it breathes slowly on the rig (2.4 s, 45–100 % of its focus
level), Lights/Both only.

**Only the current scope stays at full:**

- **Venue focus.**
  - Fixture scope: the fixture being worked is at the focus level (80%),
    and the rest of the call is dimmed (15%: Venue focus start with
    "others: dim").
  - Group scope: the whole call is at the focus level.
  - Fixtures of other calls are not driven.
- **Focus this cue.**
  - Layer scope: nothing is dimmed; the cue looks as it will.
  - Group or Fixture scope: the selected fixtures keep the **cue's own
    level**, and the rest of *that layer* is dimmed to 15% — only the
    fixtures the cue has brighter than that; one already lower is never
    raised (Q112b). The cue's level is read from its Dimmer look, else
    from the output before Focus this cue touched it.
  - Other layers are left as the cue has them.
  - The flash darkens the selected fixtures in the "off" phase.

### Feedback: lights, voice or both

Branson's ask: one setting, **Lights / Voice / Both** (default Both), shared
by Quick focus, Venue focus and Focus this cue. It lives in
`ProgrammerController::operatorFeedback()`; the UI keeps it in Settings
(`controllerTools/feedback`), not in the show. Each panel has it as a
**Feedback:** choice.

- **Lights:** the rig's blinks (selection flashes, the save's double
  blink, Focus this cue's dimming) plus the panel's status lamp (main 241e51c1c);
  no voice.
- **Voice:** spoken lines only; the rig is not blinked
  (`FocusSession::setRigFeedback(false)`, `setLightsFeedback(false)`). The
  lamp stays on screen, since it is harmless.
- **Both:** lights and voice.

**The console's setting is the authority** (Q127: "default to what's set
on the console and follow that thru the entire operation"). It applies
everywhere, the armed focus-call cue included: in Lights only that cue is
silent, and in Voice or Both it says "Venue focus ready". Hold RS (or ⇧W)
is a deliberate override that changes that same setting, so there is
always one current value.

**Say the light** (RS tap, or W) speaks in every mode, because the operator
asked for it. It gives the light's name first and briefly, then where it is.
Holding RS for 0.6 s (or pressing ⇧W) cycles the setting and says the new one
(`ControllerScheme::LongPress`, ticked by the controller tick).

### Voice

The laptop says where the controller is, briefly. The laptop's output feeds
the crew's **headsets** on stage, so the voice is the crew's cue and works
together with the blink (owner, 2026-10-23):

- **A line is never cut off or spoken over.** A line that arrives while
  another is being said waits.
- **Two priorities (gap 9, `ControllerScheme::Priority`).** "Where am I"
  lines go stale: only the newest waits, so stepping quickly through eight
  fixtures says the one you stopped on. Confirmations ("Spot 3 kept.") and
  important lines are queued and never dropped.
- **Important lines always speak (Q131)**, in Lights only too: warnings,
  refusals, the controller lost/back, the venue ask, nothing reaching the
  rig (Blackout, Grand Master at 0, a universe with no output where others
  have one), another pad.
- **The voice is in step with the blink.** A line is spoken at the same
  moment as the blink it describes (`OperatorVoice::say`, `waiting()`).

The engine emits text (`ProgrammerController::operatorAnnounce`). The UI
speaks it (`ui/src/operatorvoice.{h,cpp}`).

**Output device.** The Venue focus dialog's **Voice to:** picks the audio
output, so the voice can go to the interface that feeds the headsets. On a
Mac this is `say -a <device>`, and the device is kept in the setting
`operatorVoice/device`. Linux and Windows: follow-up (spd-say has no
device choice; Qt TextToSpeech uses the system output).

**Exact phrases:**

| When | Venue focus | Focus this cue |
|---|---|---|
| Start | "Call 1 of 5, DH Tspots to DSC. Fixture 1 of 4." | "Focus this cue, cue 4. AT spots, layer." |
| Next call / layer | "Call 2 of 5, AT spots to Soloist DS. Fixture 1 of 3." | "DS wash, layer." |
| Next fixture | "Fixture 3 of 8." (with the call prefix) | "AT spots, fixture 3 of 8." |
| Scope to group | "… All 4, group." | "AT spots SL, group 1 of 2." |
| Say the light (RS click) | "Spot 3. Call 2 of 5, AT spots to Soloist DS. Fixture 3 of 8." | "Mover 3. Cue 4, AT spots, fixture 3 of 8." |
| A / B / X / View | "Saved." / "Saved 4." / "Reverted." / "Zeroed." / "Undone." / "Nothing to undo." | "Kept." / "Back to the cue." / "Exception cleared." / "Undone." |
| Refusals | — | "This layer is aimed at a target. Focus this cue a fixture, or move the target." / "No zoom here." / "No focus here." |
| Page / palette | — | "Colour page." / "Gobo page." / "Beam page: zoom and focus." / the palette's name |

**Voice back ends.** `OperatorVoice` is a small interface:

- **macOS:** `say -r 210` through `QProcess`. It is on every Mac and needs
  nothing deployed.
- **Linux:** `spd-say` when it is on the PATH.
- **Elsewhere:** Qt6 **TextToSpeech** when that Qt has it. Homebrew's Qt
  6.11 does have it. It is linked on non-Apple platforms only, so the
  signed Mac bundle gains no framework.
- **Fallback:** `SilentVoice`. It records phrases for tests.
- **Off switch:** setting `operatorVoice/enabled` (on by default).
  `QLC_NO_VOICE` and the offscreen/minimal platforms force silence, so test
  runs are never heard.

Windows would be a `CommandVoice` around PowerShell's `SpeechSynthesizer`,
or QtTextToSpeech's SAPI engine.

---

## 4. Venue focus: data model

A focus call is defined in `engine/src/focuscalls.{h,cpp}`:

```cpp
struct FocusCall {
    QString name;                   // empty: "DH Tspots → DSC" from groups + target
    QList<quint32> groups;          // FixtureGroup ids, worked in stage order
    QList<quint32> fixtures;        // loose fixtures besides the groups'
    FocusSession::Reference target; // a StageTarget, or a scene's Aim; None = fixed lights land where they are
    QString note;                   // shown on the panel: "cut off the pros"
};
```

**Owner and order.** `FocusCalls` (owned by `ProgrammerController`, as the
CLAUDE.md says programmer logic should be) holds the calls **in run order**.

**Where they are saved.** They are saved **in the show**, inside `<Engine>`,
only when there are calls, so older shows are byte-identical:

```xml
<FocusCalls>
  <Call Name="" TargetKind="Target" TargetID="5" Note="Cut off the pros">
    <Group ID="3"/>
  </Call>
  <Call Name="AT spots -> Soloist" TargetKind="Target" TargetID="7">
    <Group ID="4"/><Fixture ID="22"/>
  </Call>
</FocusCalls>
```

The calls are the show's: they describe *how* this show is focused, which is
the same in every venue. Only the corrections are per venue.

**Running.** `FocusCalls::sessionCalls()` turns the calls into
`FocusSession::Options::calls`, and `FocusSession` runs them:

- `m_order` is the current call's heads.
- `m_all` is every head in every call.
- Entering a call resolves its target into each head's point. A line target
  spreads the call's heads along it.
- A head may be in more than one call. Its correction is one value (it is
  calibration), and each call re-aims it at that call's point to check it.

---

## 5. Per-venue corrections: the sidecar file

> What a venue *owns* besides corrections (its house rig, the patch of every
> universe in the room, the channels that are not ours, board Q137–Q138) is
> designed in [VENUES.md](VENUES.md); it is kept in this same sidecar.

The sidecar is handled by `engine/src/venuecorrections.{h,cpp}`
(`VenueCorrections`, owned by `ProgrammerController`).

**Name.** It sits beside the show, with the same base name:

```
Spring Show.qlcc  →  Spring Show.venues.xml
```

**Format.** It holds several venues, plus which one is applied:

```xml
<QlcVenues Version="1" Show="Spring Show.qlcc" Active="Lincoln HS">
 <Venue Name="Lincoln HS" Created="2026-10-23T19:04:00" Modified="2026-10-23T19:31:12">
  <Fixture ID="12" Name="AT Spot 3" PanOfs="1.25" TiltOfs="-0.40" PanZero="0.0"/>
  <Fixture ID="20" Name="DS Par 2" PanOfs="0.00" TiltOfs="0.00" AimPan="12.5" AimDown="47.0" Lens="19.0"/>
 </Venue>
 <Venue Name="Venue 2026-11-02 18:10" …/>
</QlcVenues>
```

Each fixture holds a `RigCalibration` (new, in `truss.h`):

- a moving head's pan and tilt offsets;
- a fixed light's `fixedAim`, `panZeroDir` and `aimDown`;
- a hand-zoomed `lensDeg`.

The values are absolute (this venue's value), not deltas. Decided: absolute (Q110, §11).

**Naming a venue.** The Venue focus dialog offers:

- the existing venues;
- **New venue…**, which you name or leave empty to get "Venue
  yyyy-MM-dd HH:mm", made unique;
- **None**, which saves into the show's own calibration as Quick focus always
  did.

**How the show file is kept clean.**

- `MonitorProperties` now has a **calibration base** per fixture: the show's
  own calibration, held aside while a venue is applied.
- `MonitorProperties::saveXML()` writes the base in place of the rig's
  current calibration. So **File → Save** never writes a venue's
  corrections, and neither does the Lighting Studio's undo snapshot, which
  uses the same XML.
- Applying a venue holds the base for **every** fixture, so nothing done to
  calibration while it is applied reaches the show.
- Quick focus no longer marks the show modified when it saves to a venue.

**Load and apply.** `App::loadXML(file)` calls
`venueCorrections()->setShowFile(file, true)`. That reads the sidecar and
re-applies the venue it names as active. **Save As** carries the venues to
the new name.

**Switching venues.** `setActiveVenue(name)`:

1. Restores every base (the rig is the show's own again).
2. Applies the new venue.
3. Writes `Active=`.

`None` just restores.

**Restart safety.** Every **A** (and every undo of a save) calls `record()`,
which writes the whole sidecar through `QSaveFile`. The write is atomic: a
crash mid-write leaves the old file. A restart opens the show, which reads
the sidecar and re-applies it. The test `correctionsGoToTheVenueNotTheShow`
does exactly this.

**Never touching the show file.** That test also checks that, after
corrections, the show's `FixtureRig` attributes are what they were before.

---

## 6. Focus this cue: scopes, and how each edit is stored in the cue

Focus this cue lives in `engine/src/refinesession.{h,cpp}`. The Show Tree has no
raw DMX: a value is a **palette** on a layer's **look** (`ShowTree::Look`,
by key: Position, Zoom, Beam.Beam, Colour, Beam.Gobo). A per-fixture value
in a cue is a **child layer** (§3.5 of SHOW_TREE_DESIGN.md):

- it has a target inside its parent's;
- its own looks win on its fixtures;
- the parent lights the rest of it.

Focus this cue uses exactly those. It adds nothing to the cue model.

| Scope | What a change edits | Stored as |
|---|---|---|
| **Layer** | The layer's own look, for all its fixtures. | A new **inline palette** (one-off, owned by the tree) with the new value, set on the layer by `setLayerLook()`. For position, the old palette's degrees plus the nudge, **fanning copied**, so the whole fan moves. A named palette that other cues use is never edited in place. |
| **Group** | One of the layer's groups (`Layer::pieces()`). | **Position** (Q112c): an **offset** from each fixture's own position, kept as a position exception (child layer) on **each** fixture of the group, so a fan inside the group stays a fan. Zoom, focus, colour, gobo: one **child layer** on the group (made with `addChildLayer()` the first time, reused after). X at Group scope clears both. |
| **Fixture** | One fixture. | A **child layer** on `{fixture}`, as above: the per-fixture exception. |

**Pending until A.** Every nudge is pending and shown on the rig through an
`OutputLayer` (Universe::Focus, Override), so the operator sees it at once.
**A** keeps everything pending as **one** Show Tree step
(`beginBatch("Focus this cue")` … `endBatch`). The order is Layer, then Group, then
Fixture, so the most specific wins. **B** drops it.

**X** clears the exception at Group or Fixture scope:

- it removes this page's keys from the child layer (Position, Zoom and Beam
  on the beam page; Colour; Gobo);
- it removes the child if nothing is left on it.

**Values:**

- **Position** is degrees. The start point is exact from a flat
  PanTilt/Pan+Tilt palette. Otherwise it comes from the palette expanded
  for the fixture (fanned and Aim positions), or from the output read back
  when the cue says nothing (a tracked value). It is kept in **whole
  degrees** (PanTilt palettes store ints). The preview rounds the same way,
  so what you see is what is kept.
- **An Aim layer at Layer scope is refused, and the voice says why.** Moving
  it by degrees would stop it following its target. Picking a fixture (Y)
  works: its exception is a fixed PanTilt.
- **Zoom** is degrees, held to the definition's lens range. It is stored as
  a Zoom palette.
- **Focus** is 0–255. It is stored as a Beam palette, keeping that look's
  frost and iris.
- **Colour / gobo** (LS-click page) step through the show's named Colour /
  Gobo palettes, in name order. Keeping sets that palette on the look.

**Mode (Q112a).** Focus this cue works in Design and in Operate, and switching
between them does not end it.

- **Design:** it previews the cue (`ShowTreeController::previewCue`), and
  a kept edit re-bakes and shows at once.
- **Operate:** the cue list runs on; Focus this cue does not preview. A kept edit
  is in the tree at once, but the running cue's generated scene only
  changes at its next GO. Until then Focus this cue **holds what was kept** on the
  rig through its OutputLayer, and says "Kept. It plays from the next GO."
  Leaving Focus this cue lets go. Going back to Design drops the hold and
  previews the cue with the edit.

**Layers listed.** The cue's **own top-level** layers on fixtures, then the
layers it only tracks in (§6C). Opaque function layers are not listed.

### 6C. A layer the cue only tracks

Some layers a cue only inherits (tracked in from an earlier cue, not in
`Cue::layers`). Focus this cue now lists them after the cue's own layers:

- **How they are listed.** `ShowTreeController::resolved()` gives every
  `ResolvedLayer` with `own == false` at depth 0. Each one gets a stand-in
  layer holding its target and the looks it plays here, so the controller,
  the preview and the start values work exactly as on an own layer.
  `valueSource()` gives the cue its values come from.
- **What the voice says.** "AT spots, tracked from 2. Keeps go into this
  cue."
- **Where A keeps the change.** It can go either to this cue or to the
  source cue:
  - **This cue: the default.** `setTargetLook()` makes the layer this cue's
    own; the source cue is untouched. Focus this cue then steps on from that own
    layer.
  - **The source cue: the alternative.** `updateSource()` makes the change
    there, so every cue that tracks it plays it.
- **Switching between them.** On the panel's checkbox or the **S** key. The
  choice is spoken: "Keeps go into cue 2." On the controller, **View + A**
  keeps once at the source (Q118).
- **Layer scope only.** An exception on a tracked layer needs this cue's own
  layer first, so Y says so and stays at Layer scope until A keeps it here.

The default is "this cue" because what the operator is looking at is this
cue. Changing the source silently would also change every cue between there
and here. This is Q-C1.

Test: `refinesession_test aTrackedLayerKeepsHereOrAtItsSource`.

### 6B. Focus this cue on a Build scene (from the Workbench)

The owner asked for Focus this cue "in the show-tree/builder" with the controller
"very consistent between the two". It is the same `RefineSession`, the same
controller table, panel, flash and voice. Only what an edit is stored as
changes (`engine/src/refinescene.cpp`). A Build scene has no layers, so each
part of a cue maps to its nearest equivalent:

| Cue part | Build scene equivalent |
|---|---|
| Layer (triggers) | **The scene.** The triggers step the scenes in the Workbench stack (a collection's members); with one scene they stay on it. |
| Group (Group scope) | **A group row** of the scene (`Scene::fixtureGroups()`), plus its loose fixtures as one piece. |
| Layer's look | **The scene's palettes.** In a scene they play on everything it lights, minus each palette's excluded fixtures. |
| Layer-scope edit | The scene's palette for the attribute has its **value moved in place**: `PaletteOps::valueStep`, the Look Editor's step, so a shared palette changes everywhere it is used. With no palette, the scene's raw values on all its fixtures. Colour/gobo: the palette picked replaces the scene's palette of that type. |
| Exception (child layer) | **Out of the attribute's palettes** (`Scene::setExcludedFixtures`), with the value as the scene's own **raw values** on those fixtures (added as fixture rows if they are only in a group). This is what the Show Tree's child layers bake to. One `PaletteOps::sceneStep`. **X** reverses it. |
| Show Tree undo | Build's undo (`PaletteOps::undo`). |

Entry points:

- the Workbench header's **Focus this cue…**, for a scene or a cue on loan from the
  Show Tree;
- **Focus this cue on the controller…** on the right-click menu of a scene, group
  or fixture row (Stack mode), and of a cue's layer row (`TreeCueStack`).

**Not clean:**

- Editing a shared palette in place is Build's rule, but it differs from
  the Show Tree, where Focus this cue makes a one-off. This is question Q-B1 in §11.
  (Q116 made it warn first; a reusable Show Tree layer warns the same way.)
- A Layer-scope keep that changes a palette and raw values is **one** undo
  step (Q117, `PaletteOps::sceneValueStep()`).

---

## 7. Entry points

| Where | What | v1 |
|---|---|---|
| Lighting Studio toolbar → **Venue focus** | Edit the calls, choose a venue, Start | ✅ |
| **Run** tab header → **Venue focus…** | The same dialog, pre-show | ✅ |
| Controller **View + Menu** | Plain Quick focus (as before) | ✅ (unchanged) |
| Show Tree cue page → **Focus this cue on the controller…** | Focus this cue that cue, first layer | ✅ |
| Show Tree layer right-click → **Focus this cue on the controller…** | Focus this cue starting on that layer | ✅ |
| Build Workbench header → **Focus this cue…** (a scene, or a cue on loan) | Focus this cue what is loaded (§6B) | ✅ |
| Build Workbench right-click on a scene, group, fixture or cue-layer row | Focus this cue starting there | ✅ |
| A **focus-call cue** in a cue list (cue page: **Focus call**; F column: **focus**) | Armed on GO; the controller's **Start** begins (below) | ✅ |

### A focus-call cue

The owner's direction: "it should wait for input from the joystick before it
actually starts so that light can be preset in a certain condition and then
when ready somebody can just hit the button", with the voice through the
headsets "in concert with the lights".

**The flag.** A focus-call cue is an ordinary Show Tree cue with
`Cue::focusCall` set:

- set on the cue page with the **Focus call** checkbox;
- saved in the show as `FocusCall="1"`, which an older build ignores, so the
  cue plays there as an ordinary cue;
- shown as **focus** in the F column, with a sentence in the tooltip;
- one undo step (`setCuesFocusCall`).

It belongs at the top of a "Pre-show" list.

**GO arms it; it does not start.** `FocusCallCue` (engine) watches the tree's
lists as they play: their chasers' current step, through `runningCue()`. GO
onto a focus-call cue:

- plays the **cue's own looks as a preset**: the work light, or the first
  call's fixtures parked at their point;
- **arms** the focus calls. Nothing steps.

The voice says once: **"Venue focus ready. Press Start to begin."** The cue
page, the Run tab and the footer all say **"Venue focus: waiting for the
controller"**. The Run tab also has a **Start focus calls** button.

**Start begins them.**

- **Controller:** **Start (Menu)**. While armed, Start is free: no session
  is running, so it doesn't mean Exit yet. Once running, Menu means Exit
  again. The same button starts and ends, which is easy to say on headset.
  A was rejected because it is Save in the session, so a double-press could
  save the first fixture by accident.
- **Keyboard:** **⌘↩** (Ctrl+Return), application-wide while armed.

Starting:

- runs Quick focus on the show's calls **in Operate** (`Options::allowOperate`),
  dimming the others;
- uses a venue: the one applied. With none applied (and a show file to
  keep one beside), the laptop asks for its name first (Q120, state
  `NamingVenue`): the voice says "Name this venue on the laptop, or wait for
  today's date."; after 8 s with no answer it is the dated name ("Venue
  named for today."). Typing a name holds the clock;
- speaks the first call ("Call 1 of 5, DH Tspots to DSC. Fixture 1 of 4.")
  as its blink begins.

**Leaving continues the list.** Either of these asks about unsaved work, as
Quick focus always does, then stops the session:

- **Menu**;
- **RT on the last call**, which says "Last call done." first
  (`Options::endAfterLastCall`).

`FocusCallCue` then sends **GO** to that list's chaser, which plays the next
cue. A list that moves off the cue before Start disarms it.

Tests:

- `focuscalls_test aFocusCallCueArmsThenStartsOnTheController` (armed → start
  → step → exit → GO);
- `focuscallsui_test aFocusCallCueWaitsForTheController` (the Run tab, its
  button, the panel, Menu).

---

## 8. Mockups

### Venue focus panel (Quick focus's panel, running calls)

```
┌ Quick Focus ─────────────────────────────────────────────────────────┐
│ Venue focus — fixture 2 of 4                                        │
│ Call 2 of 5: AT spots → Soloist DS                                  │
│ Cut off the proscenium                                              │
│ AT Spot 3  (ID 12)                                                  │
│ Aimed at: Soloist DS                                                │
│ Pan +1.25°    Tilt −0.40°                                           │
│ Beam 19.0° (lens 15–35°, set by hand)   D-pad ↕ or , .              │
│ Unsaved — saved is pan +0.00°, tilt +0.00° (A to save, B to revert) │
│ One fixture — bumpers step fixtures. Y switches.                    │
│   Saves to venue "Lincoln HS" (not the show file)                   │
│ Step 0.1°                                         3 of 11 checked   │
│ Use: [Controller] [Keyboard]   Xbox Wireless Controller             │
│                                                                     │
│ Previous focus call ─┐  ┌───────────┐  ┌─ Next focus call           │
│ Previous fixture ────┤ (LT)       (RT) ├── Next fixture             │
│ Undo save ───────────┤  [LB]     [RB]  ├── Exit                     │
│ Nudge ───────────────┤ ( L )   ○ Y  ───┼── Scope: group ↔ fixture   │
│ Second reference ────┤  (LS)  ○X ○B ───┼── Revert                   │
│ Zoom ▲in▼out Focus ◀▶┤  ✚       ○A ───┼── Save for this venue      │
│                      └──────( R )──────┼── Fine nudge               │
│                                 (RS) ──┴── Say the light (hold: fb) │
│                                                            [ Exit ] │
└─────────────────────────────────────────────────────────────────────┘
```

### Focus this cue panel

```
┌ Focus this cue ─────────────────────────────────────────────────────────────┐
│ Focus this cue — cue 4 Ballad  ·  layer 1 of 3                              │
│ AT spots, fixture 3 of 8.                                           │
│ Fixture: an exception for AT Spot 3 alone (a child layer).          │
│   Y changes the scope.  D-pad: beam (zoom, focus) (LS click …)      │
│ Pending (A keeps it in the cue, B goes back): pan 273° tilt 133°,   │
│   zoom 22.0°                                                        │
│ Controller: Xbox Wireless Controller                                │
│                                                                     │
│ Previous layer ──────┐  ┌───────────┐  ┌─ Next layer                │
│ Previous fixture/grp ┤ (LT)       (RT) ├── Next fixture / group     │
│ Undo ────────────────┤  [LB]     [RB]  ├── Exit                     │
│ Move ────────────────┤ ( L )   ○ Y  ───┼── Scope: layer→group→fix.  │
│ D-pad page ──────────┤  (LS)  ○X ○B ───┼── Back to the cue          │
│ Zoom ▲in▼out Focus ◀▶┤  ✚       ○A ───┼── Keep in the cue          │
│  colour, gobo) ──────┘  ────( R )──────┼── Fine move                │
│                                 (RS) ──┴── Say the light (hold: fb) │
│ [Keep (A)]  [Back to the cue (B)]                          [ Exit ] │
└─────────────────────────────────────────────────────────────────────┘
```

### The controller, side by side (what a student learns)

```
              LT  previous call / layer          RT  next call / layer
              LB  previous fixture               RB  next fixture
     ┌──────────────────────────────────────────────────────────┐
     │  (L) nudge / move         Y  scope                       │
     │  click: 2nd ref / page  X  reset      B  back            │
     │   ✚ ▲in▼out, ◀looser tighter▶  A  KEEP                   │
     │            (R) fine; click: WHERE AM I?                  │
     │      View: undo                Menu: exit                │
     └──────────────────────────────────────────────────────────┘
```

---

## 9. Code map

Engine (all unit-tested):

| File | What |
|---|---|
| `engine/src/controllerscheme.{h,cpp}` | The one map: verbs, labels per tool, trigger-as-button |
| `engine/src/focuscalls.{h,cpp}` | `FocusCall` / `FocusCalls`, XML, names |
| `engine/src/venuecorrections.{h,cpp}` | Venues, sidecar, apply/unapply, record |
| `engine/src/focusreport.{h,cpp}` | The focus report: rows, HTML, Lightwright CSV (Q134) |
| `engine/src/focussession.{h,cpp}` | + calls, scope, flash, announce, venue saves, batch undo |
| `engine/src/refinesession.{h,cpp}` | Focus this cue |
| `engine/src/truss.h` | `RigCalibration` |
| `engine/src/monitorproperties.{h,cpp}` | Calibration base, written by `saveXML()` |
| `engine/src/programmercontroller.{h,cpp}` | Owns them, routes the controller (triggers too), `operatorAnnounce` |
| `engine/src/doc.cpp` | `<FocusCalls>` load/save forwarders |

UI (thin):

| File | What |
|---|---|
| `ui/src/monitor/focuscallsdialog.{h,cpp}` | The calls and the venue, Start, the report (PDF), practice |
| `plugins/hid/gamepadlayout.*`, `plugins/hid/macx/hidgamecontroller.mm` | The pad layout, PlayStation pads through GameController (Q139) |
| `ui/src/monitor/focusmodepanel.cpp` | Call, note, scope and venue lines, new keys |
| `ui/src/monitor/focuscontrolmap.{h,cpp}` | `setTool()`, labels from ControllerScheme, LT/RT/LS/RS callouts |
| `ui/src/refinepanel.{h,cpp}` | Focus this cue's panel |
| `ui/src/operatorvoice.{h,cpp}` | Voice interface and back ends, announcer |
| `ui/src/app.cpp`, `monitor.cpp`, `cuerunner.cpp`, `showtreeview.cpp` | Wiring and entry points |

Tests:

- `engine/test/focuscalls` (12): scheme, triggers, XML, stepping, group
  scope, phrases and flash, venue sidecar and restart, switching, names.
- `engine/test/refinesession` (11): refusals, DMX→degrees, scopes and
  steps, layer edit and undo, fixture exception and clear, zoom/focus/colour,
  Aim refusal, dimming and phrases, one tool at a time.
- `engine/test/focussession`: updated for Y → scope.
- `ui/test/focuscallsui` (7): dialog, venue, panel lines, Focus this cue panel keys,
  voice.
- `ui/test/focusmode`: updated labels.

Help:

- `docs/features/focus-calls.md` and `refine.md`, both registered in
  `help.qrc` and `kTopics`;
- `lighting-studio.md` Quick focus;
- `whats-new.md`.

---

## 10. Out of scope (deliberately)

- **Framing shutters** on profiles, and barn doors. This is the separate
  future item in TODO.md. Its starting idea ("the triggers pick a side")
  **conflicts** with triggers stepping calls. See §11.
- Rumble on pads not read through GameController (Linux, Windows, PS pads
  over raw HID): see §11.
- Focus this cue of effects, and of intensity. The dimmer is deliberately not on the controller: the
  flash and dim feedback use it.
- Two operators at once (one per side). Two pads are handled: the first
  to press Start owns it (Q135).
- Ideas from the gaps review kept for later: a focus report page/CSV, a
  scramble practice mode, spoken stage positions, a freeze chord, crew
  words.
- Remote (phone) controllers. The verb layer would make this easy.

---

## 11. Decisions (board Q109–Q113, 2026-10-23)

The open questions were answered on the board after the first version:

| Q | Question | Decision | Done |
|---|---|---|---|
| Q109 | Y moved to scope in plain Quick focus (second reference on the left stick click)? | **Y is scope everywhere.** Keep as built. | As built |
| Q110 | Absolute or delta corrections? Must a venue be named? | **Absolute** values; a **date-and-time** name is fine. Keep as built. | As built |
| Q111 | A calibration typed in the fixture dialog while a venue is applied? | It goes **into the venue** and its `Show.venues.xml`, not the show. | `VenueCorrections` records any calibration change from `MonitorProperties::rigPropsChanged` while a venue is applied (only the calibration fields; a change of place or mount is not one). Quick focus's heads are *held* while it runs, so a live nudge is recorded only when saved with A, and a discarded one never. Typed back to the show's value, the correction goes. Test: `focuscalls_test anEditElsewhereGoesToTheVenue`. |
| Q112a | Focus this cue in Operate? | **Allowed.** | §6 Mode. Test: `refinesession_test operateHoldsWhatIsKept`. |
| Q112b | Focus this cue's dim raises a fixture below 15%? | **Dim only what is brighter** than 15%; never raise one the cue has lower. | `RefineSession::cueLevel()`. Test: `dimsOnlyWhatIsBrighter`. |
| Q112c | A group exception collapses a fan? | **Keep the fan:** the change is an offset to each fixture's own position. | §6 table. Test: `groupScopeKeepsTheFan`. |
| Q113 | Voice (rate, which voice, names, default), rumble, shutters? | **Wait** until he has tried it. Nothing changed. | — |

Still open from Q113, for after the trial: the voice's rate and voice,
whether it says fixture names, whether it is off by default in class;
rumble; and which control picks a shutter side (suggestion: a D-pad
"shutter page" on the left stick click, so the triggers keep meaning "big
step").

### Decisions Q116–Q127 (2026-10-24)

| Q | Decision | Built |
|---|---|---|
| Q116 | Focus this cue in Build, Layer scope, a shared palette: edit in place, but **warn first**, naming the other scenes or cues by voice and on the panel; a second A (or the panel's **Change them all**) applies it. | `RefineSession::paletteUsers()`, `sharedWarning()`, `confirmShared()`; B drops it. |
| Q117 | A Build Layer-scope keep that changes a palette and raw values is **one** undo step. | `PaletteOps::sceneValueStep()`: scenes and palette values in one step; a whole Build keep is that one step. |
| Q118 | Tracked layer: default stays "this cue"; **View + A** keeps at the source. | Focus this cue's View undoes on release unless A came while held; ⇧Return. |
| Q119 | Start/Menu begins an armed focus-call cue. | As built. |
| Q120 | No venue applied: **ask** on the laptop; 8 s without an answer, the dated name; said. | `FocusCallCue` state `NamingVenue`, `FocusCallsDialog::askVenue()`; typing holds the clock. |
| Q121 | Finishing the calls GOes on automatically. | As built. |
| Q122 | "Voice to:" on Mac only. | As built. |
| Q126 | All six, in every controller tool: the pad's light bar, rumble, "Spot 3 kept" / "2 lights not saved", the saved position's ghost and the change in degrees, A held = save all / B held = back to the last save, both bumpers = next unsaved. | See §2's table; the pad's output is below. |
| Q127 | The console's Feedback setting is the authority throughout, the armed cue included; RS held / ⇧W changes that same setting. | As built (tested). |

**The pad's light bar and rumble (Q126.1, Q126.2): what works, what is missing.**

- **The engine side is complete and tested.**
  - `ProgrammerController::setPadLight()` sends the lamp's colour (green /
    yellow / red; none to give the pad back its own) and `padRumble()` sends
    1 or 2 buzzes. Both go to the controller's own input plugin as
    `QLCIOPlugin::sendFeedBack(... params {"gamepad": "light", "rgb":
    "#rrggbb"} / {"gamepad": "rumble", "pulses": n})`.
  - A plugin that does nothing with it is a no-op. The stub plugin records
    what it is sent, for the tests.
- **macOS, the HID plugin (`HIDPlugin::sendFeedBack`).**
  - It hands these to pads read through Apple's GameController framework
    (`HIDGameController::setLight()`, `rumble()`):
    - rumble through `GCController.haptics` and Core Haptics (macOS 11+);
    - the light through `GCController.light` (macOS 11+).
  - Built, but **not tried on a real pad.**
- **The light bar: PlayStation pads (Q139, built).** Option 1 below is
  done: a DualShock 4 / DualSense (Sony 054C: 05C4, 09CC, 0BA0, 0CE6,
  0DF2) is a `GamepadLayout::PlayStation` pad, claimed through
  GameController by kind (`HIDGameController::claim(name,
  PlayStationPad)`; an Xbox line never takes one), read into the Xbox
  layout (Cross A, Circle B, Square X, Triangle Y, Create View, Options
  Menu, PS guide, touchpad click Share), so `GCDeviceLight` takes the
  lamp's colour and `GCDeviceHaptics` the buzzes. Without the framework
  (QLC_HID_NO_GAMECONTROLLER, macOS before 11.3) its HID elements are
  mapped by usage (buttons 1–14, Z/Rz the right stick, Rx/Ry the
  triggers). `playstation.qxhid` (installed on macOS only) names the
  controls with the Xbox profile's roles. **Not yet tried on a real pad**:
  check the light bar's colour, the buzz, and every button's role in
  Controller test. An Xbox pad still has no coloured light. The other way,
  kept for Linux/Windows or a pad the framework does not claim:
  1. (done) **Claim PS pads through GameController as well.**
  2. **Write the pad's output report over IOKit**
     (`IOHIDDeviceSetReport`).
     - DualShock 4: USB report 0x05, rumble bytes 4–5, RGB bytes 6–8;
       Bluetooth report 0x11 with a CRC-32.
     - DualSense: USB 0x02, Bluetooth 0x31 with a CRC-32.
- **What is missing for rumble on other pads.** A pad read through raw HID
  (not claimed) does not buzz: that is the same IOKit output report as
  above.
- **Linux / Windows.** No-op.
  - Linux: evdev `FF_RUMBLE`, or hidraw output reports.
  - Windows: XInput `XInputSetState` for Xbox, or HID output reports for
    PS pads.

**Smaller choices made on the way:**

- **Hold B** was, until Q139, the light being worked only; it is now every
  unsaved light (§2's table). B's press, which comes first, has already
  put this light back: the hold counts it and takes it into its one undo
  step.
- **Both bumpers** jump from where the *first* bumper of the pair stepped
  from, so pressing LB then RB is not "previous, then next unsaved".
- **The ghost** is drawn where the head would land with its *saved*
  calibration, given that the live calibration lands it on the reference.
  For a mover that is the saved−live difference applied to the intended
  angles, met at the reference's height. For a fixed light it is the saved
  landing point. The label reads "saved (+2.5° pan)".

### Decisions from the gaps review, Q128–Q136 (2026-10-24)

| Q / gap | Decision | Built |
|---|---|---|
| Q128 | A venue also overrides truss trims and stage target positions; a pad **target page** moves the current call's target, its heads following (like Eos focus palettes). | `<Trim>`/`<Target>` in the sidecar; `Truss::setBaseTrim()`, `StageTarget::setBasePosition()` keep the show's own in the show file; `syncStructure()` records moves while a venue is applied. Target page = **LS click** in focus calls (T), A keeps, B back. Child bars follow their parent. |
| Q129 | Confirm the venue: on open when last changed over ~a day ago ("Still at …?", New venue default); the focus-call cue ALWAYS confirms (voice + laptop, 8 s → new dated venue); footer "Venue: …"; the dialog no longer defaults to an old venue. | `needsConfirm()` (20 h), `FocusCallsDialog::confirmVenue()`, `askVenue(…, current)`, `App::updateVenueFooter()`. |
| Q130 | Quiet Focus this cue in Operate: the running cue only, no blink, no dim; the hold is released on GO; "It plays next time cue N runs". | `RefineSession::cueIsRunning()`, `isBlind()`; others refused. |
| Q131 | Important lines always speak (Lights only too) with a slow long blink and one long buzz. | `ControllerScheme::Priority`, `ProgrammerController::warnOperator()`, `flashWarning()`, `padRumble(1, 700)`. |
| Q132 | Hold A is 1.0 s; a hold never confirms a warning. | `keepAllHoldSeconds()`, `RefineSession::hasWarningPending()`. |
| Q133 | Corrections store the fixture's definition and mode; a mismatch is skipped and flagged "needs focus" (panel, plot badge, calls); a deleted fixture's correction is kept, listed as orphaned; old files apply with a one-time note. | `VenueCorrections::needsFocus()`, `orphaned()`, `note()`; plot `markNeedsFocusFixtures()`. |
| Q134 | Both (as answered on the board). | — see the board; built as items above. |
| Q135 | Two pads: the first to press Start owns the session; others ignored and told; the panel names the owner. | `controllerOwner()`, `controllerOwnerName()`. |
| Q136 | As recommended on the board. | — |
| Safety | Controller lost: freeze everything, say so; "Controller back." | `freezeController()`, `controllerLost()`. |
| Gap 5 | GO only if the list is still on the focus cue; a start never discards unsaved work; "Venue focus ended" when stopped from outside. | `FocusSession::finish()`, start guards. |
| Gap 8 | The venue file travels: Collect for Transfer copies it; a sibling's (versioned Save As, Finder copy) is offered on open. | `writeSidecarFor()`, `importCandidates()`, `importFrom()`. |
| Gap 9 | The voice never drops confirmations. | `OperatorVoice` queue with priorities. |
| Gap 10 | Accessible lamp: blue/yellow/orange, ✓ / !, the count; on the Focus this cue panel; the unsaved light breathes; View + RS says how many. | `FocusStatusLamp`, `ControllerScheme::savedColour()` etc. |
| Review | One word per tool saying where a change lands; X in a venue → the show's calibration; View timing the same everywhere (on release); a reusable layer warns first; nothing reaching the rig is said; see and clear one correction. | As listed in §2, §3; `rigOutputProblem()`, `FocusCallsDialog::showCorrections()`. |

### Decisions Q134, Q139, Q140 (2026-10-24, built that night)

| Q | Decision | Built |
|---|---|---|
| Q134 | The focus report, both forms: a printable page per venue and a Lightwright-style CSV, from the venue manager. It is the hand-in for lesson 3.6. | `FocusReport::rows()/html()/csv()`; `VenueCorrections::Check` (when, by, dead) written with every save in a venue (`markChecked()`, `<Check>` in the sidecar); "Focused by:" in the dialog (Settings `focusCalls/checkedBy`); **Export focus report…**: PDF (`QPdfWriter`, landscape Letter), .html or .csv by the file's ending (`FocusCallsDialog::writeReport()`). Flags: dead; swapped (`isSwapped()`); needs focus = not checked there, or swapped. Tests: `focusreport_test`, `focuscallsui_test theFocusReportIsExported`. |
| Q139a | Hold B = revert **all** unsaved lights (mirrors hold A), said, one undo step. | `FocusSession::revertAllHeld()`; undo entries with `revert`; "Undone: the changes are back, not saved." Test: `holdBPutsEveryLightBack`. |
| Q139b | Read PlayStation pads through GameController on macOS so the light bar follows the lamp; the HID path for other pads unchanged. | §11 "The light bar". Tests: `gamepadlayout_test playStationElements`, `matchesThePlayStationProfile`. Needs a real pad. |
| Q139c | Rumble: keep as is. | Unchanged. |
| Q140 | The small ideas, then freeze. | Spoken positions (`callPositionPhrase()`, SpokenOffset, on entering a call and Say the light); crew words (`ControllerScheme::crewWord()`, labels); freeze chord (`ProgrammerController::setHolding()`, LS on release, H); the focus look clears gobo/prism/frost (`FocusSession::openBeamValue()`); mark dead (**View + X**, chosen because View chords are the "about the light" layer and X alone is Reset; D); click on the plot (`FocusSession::jumpTo()` from `Monitor::slotMapSelectionChanged()`); new venue from the last (`addVenueFrom()`, `lastVenue()`, "Start from …" ticked). Test: `theSmallIdeas`, `aNewVenueStartsFromTheLast`. |
| Practice | Scramble practice for teaching. | §13. |

**Choices made on the way:**

- A new venue from the **dialog** starts from the last venue by default
  (ticked); the focus-call cue's spoken ask and the "Still at …?" dialog
  still start from the show's own, as their text says. One or the other
  could follow the dialog after the trial.
- Positions are said on **entering** a call and with **Say the light**, not
  on every fixture step, so stepping stays short.
- Hold is safety, so "Holding." / "Released." are important lines (always
  spoken) with one buzz; a tool that ends while holding lets go.
- Crew words are said once a press, never on a held arm's repeats, and
  only where the light has a zoom or focus to move.
- The focus look's "open" value comes from the definition: a capability
  named Open / Off / No …, or PrismEffectOff; a frost channel with none is
  sent 0. Gobo index and prism rotation channels are left alone.

### Open questions (after Q116–Q127)

| # | Question | What is built now |
|---|---|---|
| Q-F1 | Hold B: this light, or all unsaved? | **Answered (Q139a): all**, built. |
| Q-F2 | Light bar on PS pads through GameController? | **Answered (Q139b): yes**, built; untried on a real pad. |
| Q-F3 | Rumble strength and length (0.12 s at full, 0.22 s apart). | As stated; untested on a real pad. |

---

## 12. Status and next steps

**Works (on this branch):**

- The shared controller scheme, used by Quick focus, Venue focus and Focus this cue.
- The on-screen map labelled per tool.
- Triggers routed as buttons.
- Venue focus:
  - saved in the show;
  - edited in a dialog;
  - run with call and fixture stepping, group scope (nudge, save and undo
    as one), flash and voice.
- Per-venue corrections:
  - the sidecar beside the show, written on every save;
  - re-applied on open;
  - switchable;
  - the show file never takes them (tested).
- Focus this cue:
  - Layer, Group and Fixture scopes;
  - position, zoom, focus, colour and gobo palettes;
  - pending preview on the rig;
  - A keeps as one Show Tree step (inline palette on the layer, or a child
    layer exception);
  - X clears an exception;
  - Aim layers refused at Layer scope;
  - dim and flash feedback, voice.
- Voice: `say` on macOS, `spd-say` on Linux, Qt TextToSpeech elsewhere.
- Help pages and tests.

Built on the night of 2026-10-23, at the owner's request:

- Focus this cue from Build's Workbench, on scenes and on cues (§6B).
- A focus-call cue that arms on GO and starts on the controller (§7).
- Focus this cue of a layer that is only tracked in (§6C).
- A venue manager: apply now, rename, delete.
- A voice queue and output device.
- Help pictures for the new panels.

Built on 2026-10-24 from the gaps review (§11, Q128–Q136): venue
confirm, the controller-lost freeze, quiet Focus this cue in Operate, important
lines always spoken, hold A 1.0 s, the focus-call cue's robustness, the
fixture-type check, per-venue trims and targets with the target page,
the venue file travelling, the voice's priorities, the accessible lamp,
two pads, the review's consistency items, and "nothing reaching the rig".

**Stubbed or not built:**

- Rumble and the light bar beyond GameController pads (§11): Linux and
  Windows; a PS pad on a Mac without the framework reads but neither lights
  nor buzzes.
- Practice scrambles moving heads only; a fixed light's aim is not
  scrambled.
- A voice output device on Linux and Windows.
- A Lighting Studio undo snapshot restored while a venue is applied puts
  the show's own trims and targets back on the rig until the venue is
  applied again.

**Built on the night of 2026-10-24:** the focus report (Q134), hold B for
all and PlayStation pads (Q139), the small ideas (Q140) and scramble
practice (§13). Then the freeze: nothing new here until the trial.

**Next, in order:**

1. Branson tries it ("Try this" at the top, step 10 for tonight's), with a
   real PS pad for the light bar; then voice, rumble and shutters (Q113),
   and the new questions in §11.
2. A real-rig check on `ender`/the show VLAN: flash timing visible at
   distance, `say` audible over a hall, trigger thresholds on a worn
   controller.

---

## 13. Scramble practice

For teaching (lesson 3.6): the crew learns to focus on a rig that is
really off, and is scored, without anyone's real venue changing.

- **Start.** Venue focus → **Practice (scramble)…** → how far off: easy
  1–3°, medium 2–6°, hard 4–10° (pan and tilt each, either way).
  `FocusCallsDialog::startPractice()` stores the calls and calls
  `VenueCorrections::startPractice(movingHeads, seed, min, max)`.
- **The practice venue.** A venue of its own, "Practice 2026-10-24 19:30",
  flagged in the sidecar (`<Practice From= Started= LastSave= Saves=>` with
  a `<Truth>` per light: its right calibration, which is the venue applied
  when it started, else the show's). Each light's correction is the truth
  plus the scramble; trims and targets are copied from the venue applied.
  It is applied like any venue, so the real rig is really off, and the
  venue it came from is untouched.
- **The visualiser.** Quick focus normally draws the light being worked at
  its intent (on its mark). With a practice applied the views draw every
  scrambled light from its DMX with its **right** offsets
  (`practiceViewProps()` in `fixtureAimDirection()` and the plot's gauges;
  `focusIntentDirection()` answers false), so the beam lands where the
  real light would, off its mark until focused back.
- **Saves** go into the practice venue (`record()`), each one counted
  (`markChecked()`: `saves`, `lastSave`).
- **The end.** When the calls stop, `showPracticeResult()` shows the score
  (`practiceScore()`: lights within 1°, mean and worst error in degrees,
  lights saved, saves, seconds from the start to the last save) and
  `endPractice()` puts the venue it came from back. A show opened with a
  practice left applied opens on that venue instead.
- **The report** of a practice venue has the score and an **Off by**
  column (the CSV: "practice: 1.2° off" in the notes): the hand-in.
- **Not done:** fixed lights are not scrambled; a class leaderboard (more
  than one result at once) is the report files side by side.



## 14. The Focus menu, the reasons, the names (board Q143, Q146, Q148, Q149)

Built for 0.3.0 RC2 (2026-10-25).

- **One Focus ▾** on the Lighting Studio toolbar (`FocusMenu::fill()`,
  `ui/src/monitor/focusmenu.cpp`), filled as it opens: **Quick focus…**,
  **Venue focus…** (with a venue applied the calls run for it at once;
  with none, the list opens to name the room first:
  `FocusCallsDialog::startVenueFocus()`), **Edit focus calls…**,
  **Venues ▸** (Apply ▸ each venue or None, New venue…, Rig and
  universes…), **Practice (scramble)…** and **Focus report…** (the
  applied venue's; with none, the only one, else asked).
- **Greyed with the reason** (Q146-2): each `FocusMenu::*Blocked()` is
  empty when the entry can be used, else the sentence shown as its
  tooltip — no lights placed on the plot, no focus calls yet, none with a
  light on the plot, no moving head to scramble, a show not saved, no
  venue. Used on the menu, the Run tab's Venue focus, the Rig Overview's
  Quick focus, the Show Tree cue page (its Focus ▾ header, the tick box,
  Focus this cue) and the Workbench header.
- **Quick focus's setup says why** (Q146-1): under the form, "No moving
  heads are placed on the plot. Place them in the Lighting Studio, or
  choose Fixed lights." or "No stage target to aim at. Add one with Add ▸
  Add Target Position." (the Add menu's own wording). Aim at is never
  blank: "No stage target yet", and **Add a target…** places one in the
  middle of the plot view through the Add Target dialog (with its Aim
  palette). With no moving heads but fixed lights, the fixed lights are
  chosen.
- **Names** (Q149): see the note at the top. The cue page's tick box is
  **Make this a venue focus cue** (the F column still says *focus*); the
  cue page and the Workbench have a **Focus ▾** in their header (Focus
  this cue / look, Make this a venue focus cue), and a layer's right-click
  says **Focus this cue…**.
- **Practice says what a scramble is** (Q143):
  `FocusCallsDialog::practiceExplanation()` on the level picker and the
  button's tooltip, and in focus-calls.md.
- Help anchors renamed (`#quick-focus`, `#running-venue-focus`,
  `#a-venue-focus-cue-pre-show`, `#focus-this-look-on-a-build-scene`,
  `#focusing-during-the-show`); the old ones still land on the new
  headings (`HelpBrowser`'s alias table).
- Tests: `FocusCallsUi_Test::quickFocusSaysWhyStartIsGreyed`,
  `theFocusMenuGreysWhatCannotBeUsed`, `focusEntriesElsewhereSayWhy`,
  `practiceExplainsTheScramble`.

### Voice detail, and Branson's other test findings (2026-10-25)

- **Voice detail** (`ControllerScheme::VoiceDetail`: Terse, Standard —
  the default — Verbose), one setting beside Feedback on every panel
  (`VoiceDetailCombo`, QSettings `controllerTools/voiceDetail`), set on
  both sessions by `ProgrammerController::setVoiceDetail()`. Every line
  that changes with it comes from one phrase table in
  `controllerscheme.cpp`: `stepPhrase()` ("F1 Spot 3." / "Fixture 1, Spot
  3." / "Fixture 1 of 8, Spot 3."), `callPhrase()` ("Call 2." / "Call 2,
  AT spots to Soloist DS." / "Call 2 of 5, AT spots to Soloist DS, SL 2,
  US 1."), `savedPhrase()` ("Saved." / "Saved fixture 1 to Lincoln HS."
  / "Saved Spot 3 to Lincoln HS."), `savedSomePhrase()`,
  `savedAllPhrase()` ("Saved all." / "Saved all, 6 lights."),
  `revertedPhrase()` and `revertedAllPhrase()`. At Terse and Standard the
  call is said on entering it; at Verbose on every step, as before. Say
  the light is always the name first (Terse: only the name). Warnings and
  safety lines are not in the table and are never shortened. There is no
  free controller gesture for it, so it is a panel setting only. The
  tables in sections 2 and 3 above show the Verbose wording.
- **Hold A confirms**: `FocusSession::saveAllHeld()` always says
  `savedAllPhrase()` (every light of the session now saved), and the
  panel shows the last confirmation (`focusConfirmed`).
- **Lights that cannot be aimed**: Quick focus's setup keeps what was
  selected but cannot be focused (`FocusModeSetupDialog::unfocusable()`)
  and, at Start, lists them with **Exclude them** (the default) or
  Cancel.
- **Renaming a venue** renames it inside the one sidecar: corrections,
  trims, targets and house rig go with it; a practice scrambled from it
  now follows the new name back (`VenueCorrections::renameVenue()`).
- Tests: `FocusCalls_Test::theVoiceSaysTheLightsName` (each level),
  `holdingAOnThePadSaysAllSaved`, `renamingAVenueKeepsEverything`,
  `RefineSession_Test::theOperatorIsToldWhereTheyAre`,
  `FocusCallsUi_Test::voiceDetailIsOneSettingOnEveryPanel`,
  `lightsThatCannotBeAimedAreListed`.
