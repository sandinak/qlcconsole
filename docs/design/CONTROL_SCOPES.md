# Control scopes: static controls per computer, assigned controls per show

Status: **proposal, revision 2** (2026-11-24), for Branson's board. No code
for revision 2 yet. Revision 1 (places, collisions, pick-up; decisions Q295 to
Q303 in §9) is built in part: phase 1 (`ui/src/controlowners.*`), phase 3
(places on triggers, `ControlMap::Place`), the Control surfaces hub
(train/053i) and paused pick-up (feat/053-fader-pickup).

His refinement (2026-11-24): *overall settings enable the whole surface and
map the static controls (grand master, e-stop, cueing); enabling it shows a
map of static and configurable controls; static controls are the same for
each surface type; configurable controls are assigned at the thing (effect,
look...) by right-click ▸ assign to control ▸ learn; configurable controls
are per show file; the static mapping is per computer.*

---

## 1. Two layers

**Static** controls are the desk's furniture: the Grand Master, the stop, GO
and Back. They belong to *this computer and this surface*, mean the same in
every show you open, and come from the surface's **type**. **Assigned**
controls are the show's: *this* fader rides *this* effect in *this* song.
They are learned from the thing itself, saved in the show, and have a place
(Q295).

| | Static | Assigned (configurable) |
|---|---|---|
| Belongs to | this computer (the runtime instance) | this show file |
| Decided by | the surface type (its input profile) | the person, per thing |
| Set where | Connections ▸ Show control & sync ▸ **Control surfaces**: Enable | right-click the thing ▸ **Assign to Control…** ▸ press it |
| Saved in | QSettings, keyed by the device's identity (as `ControllerJobs`) | the show: `<ControlMap>`, VC widget inputs, channel groups |
| Live | always, by its own rules (Operate/Design, rule 8) | while its place is live (Q295) |
| Can a show take it? | no: refused, naming it | yes, if no static role and no other owner has it |
| Examples | GM fader, E-STOP, GO, Back, Blackout, Speed master 1 rate/tap, Design's encoder pages, the PMJ's look faders | Fader 5 = Truss Wave speed in song 3; pad 12 = hit "Stab"; button 3 = scene Warm wash; a VC slider's input |

Where today's listeners land:

| Listener today | Saved today | Becomes |
|---|---|---|
| Emergency stop (`ControlMap`, `EmergencyStopTarget`) | show `<ControlMap>` | static: the type's STOP role |
| Grand Master input (`VCProperties::grandMasterInputUniverse/Channel`) | show, VC Properties | static: GRAND MASTER role |
| Encoder bank (`engine/src/encoderbank.*`) | show, inside `<ControlMap>` | static: the type's encoder roles (Design pages) |
| Speed masters' Tap/Rate/Pause inputs (`speedmasters.cpp:710`) | show `<SpeedMasters>` | static: SPEED n roles (the masters are numbered in every show) |
| PMJ overlay (`ui/src/pmjoverlay.*`, roles hard-coded) | its profile | static: the first surface type, roles moved into its `.qxi` |
| Pad Run, focus tools (`padrun.h`, `ControllerScheme`, `ControllerJobs`) | this computer already | static: already this model (the "game pad" type) |
| Control Map triggers, hits, keys | show | assigned (unchanged, with places) |
| VC widget inputs, channel-group inputs, group masters | show | assigned (unchanged) |

A trigger on "GO of list Main" stays assigned: it names one list of one show.
The static **GO** means *the list the Run tab has*, in any show (no arm: rule 6;
the pad keeps its two-control arm).

## 2. Surface types

A surface type **is a QLC+ input profile** (`.qxi`, `engine/src/qlcinputprofile.*`,
`engine/src/qlcinputchannel.*`) with three optional additions. QLC+ 4 skips
unknown tags with a warning (`qlcinputchannel.cpp:374`), so the files stay
readable there.

```xml
<Surface Version="1" Layout="grid" Columns="9"/>          <!-- profile level: how to draw it -->
<Channel Number="7">
  <Name>Fader 9</Name><Type>Slider</Type>
  <Role>GrandMaster</Role>                                <!-- static role, or none -->
  <Facts Motorised="false" Feedback="none" Place="row 2 col 9"/>
</Channel>
```

- **Role**: the `ControlSurface` vocabulary already in `engine/src/controlsurface.h`
  (`Static { Blackout, GrandMaster, Blind, Tap, ... }`, `Transport { Go, Back, ... }`,
  `Level`, `Param`, `Page`, `Select`), plus `EmergencyStop` and `SpeedRate n` /
  `SpeedTap n`. A channel with no role is **free**: the show may assign it.
- **Facts**: motorised (a fader), relative (already: `QLCInputChannel::MovementType`
  and `RelativeEncoding`), feedback kind (none / LED / ring / RGB), and where
  it sits on the drawing.
- **Rules every type keeps**: STOP only on a button (the stop's Learn rule,
  `control-surfaces.md` "The emergency stop"); at most one STOP and one GRAND
  MASTER per type; the PMJ's role table in `pmjoverlay.cpp` becomes data in
  `PMJ-Black-1.qxi` and the overlay a generic surface overlay.

First shipped types (static roles; everything else free):

| Type (profile) | Static roles | Free for shows |
|---|---|---|
| PMJ Black 1 | GM (Master), Blackout (O), Blind (Set), Design look faders and encoders, pages | its grid |
| APC40 / APC40 mk2 | GM (Master), GO / Back (Play / Stop), STOP (one red-lit key we pick), Speed 1 (Cue level), encoder pages (Device knobs) | 8 track faders, clip grid, track knobs |
| APC mini (mk1 / mk2) | GM (fader 9), STOP (a Shift-row key), GO / Back (two scene keys) | faders 1–8, the 8x8 grid |
| X-Touch Mini / Compact | GM (main fader), GO / Back, STOP, encoder pages | the rest |
| Launchpad (Mini MK3, Pro, MK2) | STOP, GO, Back, Blackout on the top row | the grid |
| Generic MIDI fader box (BCF2000, nanoKONTROL, PMJ 9 Faders) | GM = last fader; nothing else | all others |
| Game pad | the Pad Run and focus maps (already static: `ControllerScheme`, `PadRun`) | the RT hit = the show's hits, by place |
| Stream Deck (via Companion) | none in qlcconsole: Companion's pages are its layout (open Q-Q) | — |

**An unknown device.** Connections ▸ Control surfaces ▸ **Make a Surface…**
opens the input profile editor (`ui/src/inputprofileeditor.*`; its existing
wizard learns the channels) with one added page: *"Press the button for GO /
Back / STOP; move the Grand Master fader; skip any you don't want"* — the
stop's press–release–press confirm included. It saves to the user profile
folder (`USERINPUTPROFILEDIR`), so it is a type like any other. A shipped type
is never edited in place: **Copy to Edit** (Q-L).

## 3. Enabling a surface and the surface map

The hub (`ui/src/controlsurfacespanel.*`, train/053i) splits into the two
layers: **On this computer** (surfaces, their static roles, pads) above **In
this show** (triggers by place, VC inputs, channel groups, who has it now).
No new tab, window or menu item: it is the hub's own page.

Enabling: a device seen on any MIDI / OSC / HID input whose type has static
roles is listed; **Enable** ties it to this computer by its identity (`mac:` /
`port:` / `name:`, as `ControllerJobs` does) — never by universe. Its controls
reach the engine on a **computer-owned line** outside the show's universes (as
computer keys have `ControlMap::KeyboardUniverse`), so a show with a different
patch, or none, still hears the stop (Q-J).

```
Connections ▸ Show control & sync ▸ Control surfaces
┌ On this computer ───────────────────────────────────────────────────────┐
│ ● APC40 mk2  (USB, port 2)  Enabled ☑   Type: Akai APC40 mk2 ▾  [Map]     │
│ ● PMJ Black 1 (USB, port 1) Enabled ☑   Type: PMJ Black 1      [Map]     │
│ ○ nanoKONTROL2              Enabled ☐   Type: (none)  [Make a Surface…]   │
│ ● Pad: DualSense (mac:…)    Run cues · Focus                    [Jobs…]   │
├ APC40 mk2 ──────────────────────────────────────────────────────────────┤
│  [K1][K2][K3][K4][K5][K6][K7][K8]    ▓ static (this computer)             │
│  ┌──┬──┬──┬──┬──┬──┬──┬──┐ [S1]     ▒ this show    · free                │
│  │▒ │▒ │· │· │· │· │· │· │ [S2]                                         │
│  │  ...  clip grid (40)   │ ...    Pad 1 ▒ Hit "Stab" · in song 3          │
│  └──┴──┴──┴──┴──┴──┴──┴──┘          Pad 2 ▒ Scene Warm wash · everywhere   │
│  |▒| |▒| |·| |·| |·| |·| |·| |·|  |▓|   F1 ▒ Truss Wave speed · cue 2.3     │
│  F1  F2  F3  F4  F5  F6  F7  F8   MST   MST ▓ Grand Master                 │
│  [▓STOP] [▓GO] [▓BACK]  [▓Cue lvl = Speed 1]   STOP ▓ Emergency stop        │
│  Move a control: it lights here and says who has it now.                  │
├ In this show ───────────────────────────────────────────────────────────┤
│ Triggers by place (6) · VC inputs (3) · Who has it now (0)  [Set Up…]     │
│ ⚠ This show brings its own GM input and stop.   [Use Here] [Not Here]     │
└─────────────────────────────────────────────────────────────────────────┘
```

- Each control is drawn from the profile's `<Surface>` layout (a plain list
  when a type has none), marked **static** / **this show** (what, and the
  place in words: `ControlOwners::placeName`) / **free**. A control with
  several places lists each, narrowest first (Q296's sentence).
- Moving a control highlights it (the existing input feed; the panel only
  reads, `controlsurfacespanel.h`). Click a free control: *"Assign it from
  the thing: right-click a scene, effect, cue or hit ▸ Assign to Control…"*.
- One status source: the map, the lamps, the footer and Companion read the
  same owner answer (`ControlOwners::owners`).
- Teaching / Student level: the map is read-only; Enable needs the PIN.

## 4. Assign to Control…

**Learn Trigger…** (`controllearndialog.cpp:244`) is renamed **Assign to
Control…** and offered on the same things (scene, effect, cue list, cue, hit;
later a layer and an effect in a cue): one item per object, menu count flat
(UX-41, UX-80). The dialog is today's: *"Press or move the control"*, the
place picker (phase 3: everywhere / production / list / cue), the one-line
result (*"Pad 1: Hit Stab in song 3, except Strobe in cue 2.3"*). A fader
assignment (a ride of an effect's speed or a layer's level, Q298) is the same
item; the dialog offers what a fader can do for this thing.

Collisions (phase 1's `ControlOwners`, plus one owner kind):

- **A static control → refused, no Move It Here**: *"That is the APC40 mk2's
  GO on this computer. Pick a free control, or change the surface in
  Connections ▸ Control surfaces."* (UX-28). A show can never take one.
- **Another show listener** (VC, channel group, a trigger at the same place)
  → refused naming it, with **Move It Here** (Q297, built).
- **A trigger at another place** → shared, narrowest armed wins (Q296, built).
- **A show opened on a computer whose static roles cover one of its
  triggers**: the trigger is kept in the file, never armed here, and listed
  in Who has it now: *"Button 3: Scene Warm wash — blocked on this computer:
  APC mini's STOP"* (Q-N).

## 5. Shows that hold static bindings today

Every show saved by 0.5.3 or earlier may hold a GM input, an emergency stop,
an encoder bank and speed-master inputs. Nothing is dropped:

1. **Read as today.** The loaders keep reading `<ControlMap>`'s stop and
   `<EncoderBank>`, VC Properties' GM input, `<SpeedMasters>` inputs, and
   **save them back unchanged** (QLC+ 4 and older qlcconsole still work).
2. **On load, once per show per computer**, when the show holds one this
   computer has no static role for: the hub row and a footer line (not a
   modal) say *"This show brings its own GM input and stop. Use them on this
   computer?"* — **Use Here** makes them this computer's (a "Custom" type
   built from them), **Not Here** leaves them inert (remembered per show).
3. **The stop never goes quiet.** Until this computer has a STOP role, a
   show's own stop stays live whatever was answered.
4. **The computer wins** where both exist; the show's copy is listed as
   *"in this show, not used on this computer"*.
5. New shows write none of these. The help, a course note and a CHANGELOG
   line say where they went.

## 6. Pick-up with declared facts (replaces Q304)

feat/053-fader-pickup guesses: *"whether a device's fader has a motor cannot
be told from its universe"* (`faderpickup.h`), so every fader waits and
feedback is sent to all. With facts in the type:

| Declared | Rule |
|---|---|
| Relative (encoder) | never waits; turns from the current value |
| Motorised | sent the value when the owner changes (a GO, a page, a new selection) and taken when it arrives; the travel window (`TravelMs`) applies only here |
| Absolute, not motorised | waits until it passes the value (catch, Q299); the GM and lowering masters take a pull down at once (built) |
| Not declared (no type, VC inputs on raw MIDI) | the branch's rule, unchanged |

The pick-up hint (*"move up to 62 % to pick up"*) shows on the surface map
beside the fader as well as on the GM chip.

## 7. The in-flight branches

| Branch | Verdict | Concretely |
|---|---|---|
| feat/053-control-collisions (phase 1, `ControlOwners`) | **Keep**, merge in the next train | It is the owner query both layers need. In S3: a `Static` owner kind (surface, role, "on this computer", `movable=false`) replaces `Pmj`; `EmergencyStop`, `Encoder`, `GrandMaster`, `SpeedMaster` become `Static` as they migrate (§5) |
| feat/053-control-button-places (phase 3, this branch) | **Keep**, merge after phase 1 | Places are the assigned layer as he described it. In S1 the menu text becomes Assign to Control… |
| feat/053-fader-pickup (phase 2, paused) | **Adapt, unpause** | `FaderPickup` stays; in S2 add a declared fact (Relative / Motorised / Absolute / Unknown) read from the type; the motor-travel guess runs only for Motorised and Unknown. Its default is safe to ship first (Q-R) |
| train/053i Control surfaces hub | **Keep, restructure** | In S1 its rows split under **On this computer** (pmj, pads; estop, encoder, masters as they migrate) and **In this show** (map, vc, places, collisions, web); the surface map is added to it |

## 8. Questions for Branson

**Q-J. How does a static surface reach the engine when a show's patch does not have it?**
A: a computer-owned input line outside the show's universes, found by the device's identity (as keys use `KeyboardUniverse`). B: on load the computer patches it into a free universe of the show, not saved. C: the show's patch stays the way in; the computer only names roles, and the hub offers "Patch it" when it is missing.
*Recommend A*: a show can't lose the stop by its patch, and nothing in the show changes. ROI: high (static means it works in every show). Size: M–L (an input line in `InputOutputMap` that is not a universe; owner keys already allow a pseudo-universe).

**Q-K. Which static roles exist?**
A: the minimum — GM, STOP, GO, Back, Blackout. B: A plus Speed master 1–4 rate/tap, Blind, cue next/prev. C: B plus Design editing (encoder pages, look faders, as the PMJ and encoder bank do today).
*Recommend C, built A → B → C*: the PMJ and the encoder bank are already C. ROI: high for A, medium for B/C. Size: A S, B S, C M (move PMJ and encoder bank onto roles).

**Q-L. Can a computer change a type's static layout?**
A: no — a type is fixed; a different layout is a new type (Copy to Edit). B: per-computer overrides on top of a type. C: fully free per computer.
*Recommend A*: "consistent for each surface type" holds, and a copied type is one file to carry to another computer. ROI: medium. Size: S (A), M (B).

**Q-M. Old shows' GM, stop, encoder bank and speed-master inputs?**
A: ask once per show per computer (Use Here / Not Here); the stop stays live until the computer has one; the file never changes. B: adopt silently when this computer has none. C: ignore them (kept in the file, inert).
*Recommend A*. ROI: high (no show breaks quietly). Size: M.

**Q-N. A show's trigger lands on a control that is static on this computer.**
A: static wins; the trigger is kept, not armed, listed as blocked with the reason. B: the show wins for that control while the show is open. C: ask at load.
*Recommend A*: the GO is the GO on this desk in any show. ROI: high (safety, one status source). Size: S on phase 1.

**Q-O. Rename Learn Trigger… to Assign to Control… and use it for faders too?**
A: yes, one item on every assignable thing. B: keep Learn Trigger… and add Assign Fader…. C: keep as is.
*Recommend A* (his words; menu count flat). ROI: medium (findability). Size: S (text) + M (fader rides).

**Q-P. Where does the surface map live?**
A: in the Control surfaces hub only. B: A, plus a footer chip per enabled surface that opens it at that surface. C: also an Operate overlay (a key shows it over the Run tab).
*Recommend B*. ROI: medium–high (findable from where you work, UX-61). Size: M.

**Q-Q. Stream Deck through Companion: static, assigned, or its own?**
A: Companion's pages are its own static layout on that computer; qlcconsole assigns nothing to it. B: Companion exposes free keys ("qlcconsole: Control 1–32") that a show assigns like any control. C: both.
*Recommend A now, B later*: Companion already lays out keys; B needs a Companion module change. ROI: low–medium. Size: A none, B M.

**Q-R. Pick-up: what ships before surfaces declare their faders?**
A: pick-up for non-master faders only (VC playback/level sliders, speed-master Rate, channel groups); the Grand Master and VC Grand/Sub/Group master sliders keep today's behaviour (follow at once, both ways) until their surface type declares them; motorised "send the value" waits for declared facts. B: every fader waits, masters too (an emergency pull-down of a master would wait). C: no pick-up until S2.
*Recommend A*: four review rounds showed a master's "lower at once but not on jitter" can't be told from MIDI traffic alone (board Q304); with declared facts in S2 masters can join. ROI: high. Size: S (narrow the paused branch + one review).

## 9. Decisions (2026-11-24, revision 1) — status under revision 2

| Board | Here (rev. 1) | Decision | Status |
|---|---|---|---|
| Q295 | Q-A | Mappings get a place (cue, layer, effect, everywhere), armed only while it is live: the rule hits follow. | **Stands**: the assigned layer. Cue / list / production / everywhere built (phase 3); layer and effect not yet. |
| Q296 | Q-B | Two mappings on one control at different places are allowed; the narrowest armed one wins (cue > layer > list > production > effect > everywhere > this computer), said at Learn. | **Stands for assigned controls**; the "this computer" rung is **superseded**: static controls are not on the ladder, a show can't share them. |
| Q297 | Q-C | A control another listener already uses is refused at Learn, naming the owner, with **Move It Here**. | **Stands**, amended: a static control is refused with no Move It Here. |
| Q298 | Q-D | A scoped fader's value drops when its place ends (the GO); the cue plays as written; Store keeps it into the cue. | **Stands** (not built). |
| Q299 | Q-E | Pick-up on for every scoped fader; motorised faders are sent the value. | **Stands**; "motorised" is now **declared** by the type (§6), not guessed. |
| Q300 | Q-F | RGBW / pan / tilt faders are refused in Operate for now; group masters for levels. A ride bound to a fixed layer may come later. | **Stands**. |
| Q301 | Q-G | Both: the device setup per computer, the mappings per show. | **Stands, made concrete** by revision 2: device setup = static roles from the type. |
| Q302 | Q-H | A control on an effect is armed wherever the effect plays; learn it on the cue to narrow it. | **Stands** (not built). |
| Q303 | Q-I | Two things at the same place on one button step through (as hits); refused for faders. | **Stands**. |
| Q304 | — | Pick-up and motor faders (feat/053-fader-pickup paused on it). | **Superseded** by §6 and Q-R. |

## 10. Phased plan (smallest valuable first)

0. **Land what's built**: phase 1 + phase 3 + the hub (a train), pick-up as
   is (Q-R A). No model change.
1. **S1 — the hub's two layers and the map, read-only.** Split the hub; draw
   each patched device from its profile (a list when there's no layout) with
   today's owners: static (PMJ, the stop, GM, encoder bank, speed masters) vs
   this show vs free. Rename to Assign to Control… (Q-O). Size M. Value: the
   whole desk visible at once, today.
2. **S2 — surface types.** `<Role>` / `<Facts>` / `<Surface>` in the profile
   loader and editor; the PMJ's roles move into its `.qxi`; APC mini and the
   generic fader box get roles; `FaderPickup` reads Facts. Size M.
3. **S3 — static per computer.** Enable by identity, the computer-owned line
   (Q-J), static roles live in every show; the `Static` owner and its refusal
   (Q-N); migration of old shows (Q-M). Size L.
4. **S4 — more types and Make a Surface…** (APC40, X-Touch, Launchpad; the
   static-role wizard page; Copy to Edit). Size M.
5. **S5 — fader rides and the remaining places** (layer, effect: Q298, Q302)
   on Assign to Control…. Size M–L.

Each phase: help in `docs/features/control-surfaces.md`, a course note, gate
tests that fail on today's code (a static control refused for a show; an old
show's stop still live; a motorised fader not waiting), shots at 3 sizes for
the hub, and a CHANGELOG line.
