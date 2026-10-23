# Focus calls and Refine — design (board Q105–Q108)

Branch `feat/focus-calls-refine`, 2026-10-23. A design plus a first working
version. Not merged.

**The idea.** Kids already know game controllers, so the controller is the
star here. Two jobs on the stage use one controller layout:

- **Focus calls** align the rig in a new venue. This is calibration, saved
  per venue.
- **Refine** changes a cue's own values from the stage. This is
  programming, saved in the show.

The layout is the same in both, and the same as plain Focus mode. A student
learns it once.

Contents:

1. Decisions this builds on
2. The controller, one map for both tools
3. Telling the operator where they are (lights and voice)
4. Focus calls: data model
5. Per-venue corrections: the sidecar file
6. Refine: scopes, and how each edit is stored in the cue
7. Entry points
8. Mockups
9. Code map
10. Out of scope
11. Open questions for the owner
12. Status and next steps

---

## 1. Decisions this builds on

| Q | Decision | Where it lands |
|---|---|---|
| Q105 | Corrections are saved for this run **and** to disk, so a restart keeps them. They never overwrite the show file. They are kept **per venue** (a named venue, or a date when unnamed). | §5 |
| Q106 | Refine's scope cycles **Layer → Group → Fixture** on one button. Layer edits the layer's look. Fixture makes a per-fixture exception. | §6 |
| Q107 | Feedback to the operator on stage comes from **the lights** and **the laptop's voice**. On a change, what is selected flashes. Only the current scope stays at full. | §3 |
| Q108 | Design both now and build them together. The controller layout is **the same** in both. | §2 |

Already on main: Q100–Q104. These cover the fixed-light Pan/Tilt aim, Focus
mode for fixed lights, beam/zoom/focus in Focus mode, and the Show Tree's
S/T/R/M/F columns.

---

## 2. The controller: one map for both tools

`engine/src/controllerscheme.{h,cpp}` is the single source of the map:

- `ControllerScheme::verbFor(role)` maps each control to a **verb**.
- `label(tool, verb)` gives each tool's words for that verb.

Focus mode's `FocusSession::controlFor()`, Refine's `RefineSession::apply()`
and the on-screen map (`FocusControlMap::setTool()`) all read it. A control
does the same **kind** of thing in every tool:

- triggers step the big unit;
- bumpers step the small unit;
- Y sets how much you are working on;
- A keeps, B goes back.

| Control | Verb | Focus mode (as before) | **Focus calls** | **Refine** |
|---|---|---|---|---|
| **RT / LT** (triggers) | Next / previous group | — | Next / previous **focus call** | Next / previous **layer** (always Layer scope) |
| **RB / LB** (bumpers) | Next / previous item | Next / previous fixture | Next / previous **fixture** in the call | Next / previous **fixture**, or **group** at Group scope. At Layer scope a bumper picks the first fixture. |
| **Y** | Cycle scope | One head ↔ all heads | **Fixture ↔ whole call** | **Layer → Group → Fixture** (Group only when the layer has more than one group) |
| **Left stick** | Move / nudge (coarse) | Calibration pan/tilt, 8°/s | Calibration pan/tilt, 8°/s. A fixed light moves where it lands. | The cue's **position**, 8°/s |
| **Right stick** | Fine | 1°/s | 1°/s | 1°/s |
| **D-pad ↑ / ↓** | Zoom wider / narrower | Lens (hand zoom: saved) or zoom channel (live only) | same | The cue's **zoom** (on the colour/gobo pages: nothing) |
| **D-pad ← / →** | Focus nearer / farther | Focus channel, live only | same | The cue's **focus**. On the colour/gobo pages: previous / next palette. |
| **A** | Keep | Save the head's offsets | **Save for this venue** (sidecar, at once) | **Keep in the cue** (one Show Tree step) |
| **B** | Revert | Back to saved | Back to saved | **Back to the cue** (drops pending) |
| **X** | Reset | Zero the offsets | Zero the offsets | **Clear the exception** (Group/Fixture). At Layer scope it is the same as B. |
| **View / Back** | Undo | Undo last save (a group save is one step) | same | Show Tree undo |
| **Menu / Start** | Exit | Ask about unsaved, leave | same | Ask "Keep what is pending?", leave |
| **LS click** | Alternate | Second reference | Second reference (rarely needed: each call has its own point) | **D-pad page**: beam → colour → gobo |
| **RS click** | Where am I? | Say it, flash it | Say it, flash it | Say it, flash it |
| View + Menu held | — | Opens Focus mode (as before) | — | — |
| Guide | — | unused | unused | unused |

**Triggers are read as buttons.** They press past 0.6 and release below
0.35, so a trigger resting slightly in does not repeat
(`ControllerScheme::TriggerButton`). On the Xbox profile the triggers are
axes 2 and 5 (`role="lt"` and `role="rt"`). They were already declared;
nothing in the profiles changed.

**One change to plain Focus mode.** Y used to toggle the second reference.
Y is now the scope everywhere, so the second reference moved to the left
stick click, and the keyboard keeps **R**. Plain Focus mode's Y now nudges
and saves *every* head in the list at once. That is useful when a whole
truss trims a little low. A bumper goes back to one head.

**Keyboard equivalents** (the panels and the help list them all):

| Action | Key |
|---|---|
| Calls / layers | PgDn / PgUp |
| Fixtures | ] / [ |
| Scope | G |
| Where am I? | W |
| Page (Refine) | P |
| Keep | Return |
| Back | Backspace |
| Reset | 0 |
| Zoom | . / , |
| Focus | ; / ' |
| Exit | Esc |

**Not used: rumble.** The TODO item mentioned rumble. The HID plugin has no
output path to a pad's motors yet, so it is left as an open question (§11).

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

**Only the current scope stays at full:**

- **Focus calls.**
  - Fixture scope: the fixture being worked is at the focus level (80%),
    and the rest of the call is dimmed (15%: Focus calls start with
    "others: dim").
  - Group scope: the whole call is at the focus level.
  - Fixtures of other calls are not driven.
- **Refine.**
  - Layer scope: nothing is dimmed; the cue looks as it will.
  - Group or Fixture scope: the selected fixtures keep the **cue's own
    level**, and the rest of *that layer* is held at 15%.
  - Other layers are left as the cue has them.
  - The flash darkens the selected fixtures in the "off" phase.

### Voice

The laptop says where the controller is: briefly, and only the latest phrase
(a new phrase cuts off the old one, so stepping quickly through eight
fixtures says only where you stopped). The engine emits text
(`ProgrammerController::operatorAnnounce`). The UI speaks it
(`ui/src/operatorvoice.{h,cpp}`).

**Exact phrases:**

| When | Focus calls | Refine |
|---|---|---|
| Start | "Call 1 of 5, DH Tspots to DSC. Fixture 1 of 4." | "Refine, cue 4. AT spots, layer." |
| Next call / layer | "Call 2 of 5, AT spots to Soloist DS. Fixture 1 of 3." | "DS wash, layer." |
| Next fixture | "Fixture 3 of 8." (with the call prefix) | "AT spots, fixture 3 of 8." |
| Scope to group | "… All 4, group." | "AT spots SL, group 1 of 2." |
| Where am I? (RS click) | The full phrase again | "Cue 4, AT spots, fixture 3 of 8." |
| A / B / X / View | "Saved." / "Saved 4." / "Reverted." / "Zeroed." / "Undone." / "Nothing to undo." | "Kept." / "Back to the cue." / "Exception cleared." / "Undone." |
| Refusals | — | "This layer is aimed at a target. Refine a fixture, or move the target." / "No zoom here." / "No focus here." |
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

## 4. Focus calls: data model

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

The values are absolute (this venue's value), not deltas. See §11.

**Naming a venue.** The Focus calls dialog offers:

- the existing venues;
- **New venue…**, which you name or leave empty to get "Venue
  yyyy-MM-dd HH:mm", made unique;
- **None**, which saves into the show's own calibration as Focus mode always
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
- Focus mode no longer marks the show modified when it saves to a venue.

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

## 6. Refine: scopes, and how each edit is stored in the cue

Refine lives in `engine/src/refinesession.{h,cpp}`. The Show Tree has no
raw DMX: a value is a **palette** on a layer's **look** (`ShowTree::Look`,
by key: Position, Zoom, Beam.Beam, Colour, Beam.Gobo). A per-fixture value
in a cue is a **child layer** (§3.5 of SHOW_TREE_DESIGN.md):

- it has a target inside its parent's;
- its own looks win on its fixtures;
- the parent lights the rest of it.

Refine uses exactly those. It adds nothing to the cue model.

| Scope | What a change edits | Stored as |
|---|---|---|
| **Layer** | The layer's own look, for all its fixtures. | A new **inline palette** (one-off, owned by the tree) with the new value, set on the layer by `setLayerLook()`. For position, the old palette's degrees plus the nudge, **fanning copied**, so the whole fan moves. A named palette that other cues use is never edited in place. |
| **Group** | One of the layer's groups (`Layer::pieces()`). | A **child layer** on that group (made with `addChildLayer()` the first time, reused after), with the value as its own look. |
| **Fixture** | One fixture. | A **child layer** on `{fixture}`, as above: the per-fixture exception. |

**Pending until A.** Every nudge is pending and shown on the rig through an
`OutputLayer` (Universe::Focus, Override), so the operator sees it at once.
**A** keeps everything pending as **one** Show Tree step
(`beginBatch("Refine")` … `endBatch`). The order is Layer, then Group, then
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

**Mode.** Refine is Design mode only in v1. It previews the cue
(`ShowTreeController::previewCue`), and a kept edit re-bakes and shows at
once. Going to Operate ends it. Operate is an open question (§11): a
structural edit there only lands on the next GO.

**Layers listed.** The cue's **own top-level** layers on fixtures. Opaque
function layers, and layers that are only tracked in, are not listed (§12).

---

## 7. Entry points

| Where | What | v1 |
|---|---|---|
| Lighting Studio toolbar → **Focus calls** | Edit the calls, choose a venue, Start | ✅ |
| **Run** tab header → **Focus calls…** | The same dialog, pre-show | ✅ |
| Controller **View + Menu** | Plain Focus mode (as before) | ✅ (unchanged) |
| Show Tree cue page → **Refine on the controller…** | Refine that cue, first layer | ✅ |
| Show Tree layer right-click → **Refine on the controller…** | Refine starting on that layer | ✅ |
| Build Workbench (a cue on loan from the Show Tree) | A Refine button on the Workbench header | ✗ (next) |
| A **focus-call cue** in a cue list | See below | ✗ (designed) |

**A focus-call cue (designed, not built).** This is a cue flag
`focusCall="all"` or `focusCall="2-4"` on an ordinary Show Tree cue. GO on
that cue would:

1. leave the list's lights as they are;
2. open the Focus calls panel on those calls, with the venue applied (or
   ask for one);
3. let **Menu** end the focus and continue: the next GO plays the next cue.

It belongs at the top of a "Pre-show" list, so the stage manager's checklist
and the focus are one list. It needs Operate-mode Focus, because the list
runs in Operate (see §11).

---

## 8. Mockups

### Focus calls panel (Focus mode's panel, running calls)

```
┌ Focus Mode ─────────────────────────────────────────────────────────┐
│ Focus calls — fixture 2 of 4                                        │
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
│ Zoom ↕ Focus ↔ ──────┤  ✚       ○A ───┼── Save for this venue      │
│                      └──────( R )──────┼── Fine nudge               │
│                                 (RS) ──┴── Where am I?              │
│                                                            [ Exit ] │
└─────────────────────────────────────────────────────────────────────┘
```

### Refine panel

```
┌ Refine ─────────────────────────────────────────────────────────────┐
│ Refine — cue 4 Ballad  ·  layer 1 of 3                              │
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
│ Zoom ↕ Focus ↔ (or   ┤  ✚       ○A ───┼── Keep in the cue          │
│  colour, gobo) ──────┘  ────( R )──────┼── Fine move                │
│                                 (RS) ──┴── Where am I?              │
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
     │   ✚ zoom ↕ focus ↔          A  KEEP                      │
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
| `engine/src/focussession.{h,cpp}` | + calls, scope, flash, announce, venue saves, batch undo |
| `engine/src/refinesession.{h,cpp}` | Refine |
| `engine/src/truss.h` | `RigCalibration` |
| `engine/src/monitorproperties.{h,cpp}` | Calibration base, written by `saveXML()` |
| `engine/src/programmercontroller.{h,cpp}` | Owns them, routes the controller (triggers too), `operatorAnnounce` |
| `engine/src/doc.cpp` | `<FocusCalls>` load/save forwarders |

UI (thin):

| File | What |
|---|---|
| `ui/src/monitor/focuscallsdialog.{h,cpp}` | The calls and the venue, Start |
| `ui/src/monitor/focusmodepanel.cpp` | Call, note, scope and venue lines, new keys |
| `ui/src/monitor/focuscontrolmap.{h,cpp}` | `setTool()`, labels from ControllerScheme, LT/RT/LS/RS callouts |
| `ui/src/refinepanel.{h,cpp}` | Refine's panel |
| `ui/src/operatorvoice.{h,cpp}` | Voice interface and back ends, announcer |
| `ui/src/app.cpp`, `monitor.cpp`, `cuerunner.cpp`, `showtreeview.cpp` | Wiring and entry points |

Tests:

- `engine/test/focuscalls` (12): scheme, triggers, XML, stepping, group
  scope, phrases and flash, venue sidecar and restart, switching, names.
- `engine/test/refinesession` (11): refusals, DMX→degrees, scopes and
  steps, layer edit and undo, fixture exception and clear, zoom/focus/colour,
  Aim refusal, dimming and phrases, one tool at a time.
- `engine/test/focussession`: updated for Y → scope.
- `ui/test/focuscallsui` (7): dialog, venue, panel lines, Refine panel keys,
  voice.
- `ui/test/focusmode`: updated labels.

Help:

- `docs/features/focus-calls.md` and `refine.md`, both registered in
  `help.qrc` and `kTopics`;
- `lighting-studio.md` Focus mode;
- `whats-new.md`.

---

## 10. Out of scope (deliberately)

- **Framing shutters** on profiles, and barn doors. This is the separate
  future item in TODO.md. Its starting idea ("the triggers pick a side")
  **conflicts** with triggers stepping calls. See §11.
- Rumble or vibration feedback (the HID plugin has no output path to the
  pad).
- Refine in Operate (live during a show).
- Refine of tracked-in layers (only the cue's own layers), of effects, and
  of intensity. The dimmer is deliberately not on the controller: the
  flash and dim feedback use it.
- Editing a reusable layer through Refine. `setLayerLook` on a use edits
  the reusable everywhere. v1 does not warn about this.
- Multiple controllers (two operators, one per side).
- Remote (phone) controllers. The verb layer would make this easy.

---

## 11. Open questions for the owner

1. **Y moved in plain Focus mode.** The second reference is now the left
   stick click, so that Y means "scope" everywhere. Is that OK, or should
   plain Focus mode keep Y?
2. **Absolute or delta corrections?** A venue stores absolute offsets
   ("here, this head is +1.25°"). If the show's own calibration is
   re-done later, the venue still wins for the fixtures it corrected.
   Deltas (relative to the show's) would follow such a change, but are
   harder to read. Which one?
3. **Unnamed venues.** They default to "Venue 2026-10-23 19:04". Should the
   dialog insist on a name instead? Teachers may prefer "Lincoln HS".
4. **Corrections made outside Focus calls while a venue is applied.** For
   example, an offset typed in the fixture dialog. They are held out of the
   show file but are *not* recorded to the venue, so they are lost on
   reopen. Should they record into the venue, or should the fixture dialog
   warn?
5. **Refine in Operate.** In Operate, a Show Tree edit that changes a
   layer's structure lands on the next GO. Live-refining a running cue
   could hold the pending overlay after A until the cue is re-run. Is
   Operate wanted for v2? A focus-call cue in a running list needs the
   same.
6. **Shutters vs triggers.** If shutters come to Focus mode, which control
   picks the side? Suggestion: the **D-pad on a "shutter page"** (LS click),
   matching Refine's pages, so the triggers stay "big step".
7. **Refine's dim level.** Out-of-scope fixtures in the layer go to an
   absolute 15%. That can *raise* a fixture the cue has below 15%. Fine as
   is, or scale the cue's level instead?
8. **Group scope position in Refine.** A group exception takes the group's
   first fixture's position for the whole group, which collapses a fan
   inside the group. Keep it, or give a group exception a fanned copy?
9. **Voice.** Is the rate right (`say -r 210`)? Which voice? Should it also
   speak the fixture's name ("fixture 3 of 8, AT Spot 3")? Should the voice
   be off by default in a classroom?
10. **Rumble.** Is it worth adding HID output for rumble (one buzz on Keep,
    two on refuse)?

---

## 12. Status and next steps

**Works (on this branch):**

- The shared controller scheme, used by Focus mode, Focus calls and Refine.
- The on-screen map labelled per tool.
- Triggers routed as buttons.
- Focus calls:
  - saved in the show;
  - edited in a dialog;
  - run with call and fixture stepping, group scope (nudge, save and undo
    as one), flash and voice.
- Per-venue corrections:
  - the sidecar beside the show, written on every save;
  - re-applied on open;
  - switchable;
  - the show file never takes them (tested).
- Refine:
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

**Stubbed or not built:**

- A focus-call cue in a list (designed in §7).
- A Refine button on the Build Workbench.
- Refine in Operate.
- Refine of tracked-in layers.
- A venue manager UI (rename and delete exist in the engine only).
- Help screenshots for the new panels (`./help-shots.sh` entries).
- Rumble.

**Next, in order:**

1. Owner review of §11.
2. A Refine button on the Workbench header when a cue is on loan.
3. The focus-call cue (needs Operate-mode Focus).
4. A venue manager (rename/delete) in the dialog.
5. Help shots.
6. A real-rig check on `ender`/the show VLAN: flash timing visible at
   distance, `say` audible over a hall, trigger thresholds on a worn
   controller.
