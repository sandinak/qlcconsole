# Surface pages: labelled, switchable control layouts per surface type

Status: **proposal** (2026-11-25), for Branson's board. Design only, no code.
Builds on [CONTROL_SCOPES.md](CONTROL_SCOPES.md) (Q305-Q324) and
[CONTROL_SURFACES_EVAL.md](CONTROL_SURFACES_EVAL.md) (phases T and S5).

His ask (2026-11-25): *statically mapped things that work during operations:
colours on the faders; H/V on the knobs (1,2 H and H fine, 5,6 V and V fine);
focus and zoom on 3,4, gobos on 5,6... "pages" of use case, labelled, the same
for the APC40, MaizeDMX, the PMJ. A tool to build them in the main app, saveable
and exportable to others, based on the layouts.*

## 0. Decisions proposed (one line each)

1. **Model**: a **page set** is a small JSON file that fits one surface layout.
   It holds named pages; each page maps layout controls to an **operating
   parameter** (colour, position, beam, gobo, intensity, effect speed/size) of
   *the target*. EncoderBank's five banks become the first page set; the PMJ's
   hard-coded pages become the PMJ's page set.
2. **Target and mode**: in Design a page edits what the encoders edit today
   (`Workbench::encoderTarget()`): the look in the Look Editor, else the focused
   scene's selected fixtures, else a stack layer. One undo step per gesture. In
   Operate a page edits nothing (today's rule) and its controls fall back to the
   show's assignments; busking in Operate is a later, opt-in slice (Q-SP2).
3. **Switching**: each page set names its page keys (the PMJ's Effects / Groups
   / Looks / Macros / Fix Cont; the APC40's Device ◀ ▶ and knob-mode row). The
   lit key is the page. The page name is on the map, the footer desk chip and
   the Workbench's encoder line.
4. **Where**: built-in sets ship read-only next to the layouts; **Copy to Edit**
   makes a user set. Which set a surface uses is per computer, by device
   identity (as DeskRecord). Export / Import is the file itself.
5. **Builder**: on the surface map in Control surfaces. A Pages strip above the
   map; click (or move) a control, pick its parameter from a short grouped list.
   No new menu item, tab or window.
6. **First sets**: APC40 mk2 "Build" (faders = colour, device knobs = position
   and beam, track knobs = colour wheel and gobos: no paging needed for his
   list), PMJ Black 1 (its five page keys), X-Touch Mini (eight knobs, so gobos
   are a second page). APC64 and MaizeDMX after his profiler gives their profiles.
7. **Phasing**: slice 1 model + built-in APC40 / PMJ sets + switching + labels;
   slice 2 builder + Copy to Edit + export/import; slice 3 more types and (if
   Q-SP2 says so) Operate busking on S5's fader rides.

**ROI**: Value 5, Fit 5, Reuses EncoderBank/EncoderAttributes, SurfaceLayout,
DeskRecord, the hub map; Effort ~8 days (slices 1-2); Risk M; Maint. M →
**ROI 2.5** (good). Slice 1 alone: ~4 days, ROI 5.

---

## 1. What exists (the parts this reuses)

| Piece | Where | What it gives |
|---|---|---|
| Encoder bank | `engine/src/encoderbank.h:61`, banks `:73`, slot names `encoderbank.cpp:151` | 5 banks × 8 slots (Intensity; Hue, Sat, R, G, B, W, A, UV; Pan, Tilt; Zoom, Focus, Iris, Frost, Strobe, Shutter; Gobo, Gobo rot., Prism, Prism rot.), relative decoding incl. `AbsoluteDelta` for the APC40 knobs (`encoderbank.h:93`), push = fine while held / home on tap, bank lamps, presets incl. "Akai APC40 (knobs)" (`encoderbank.cpp:86`). Saved in the show's `<ControlMap>` today |
| Attribute writer | `engine/src/encoderattributes.h` (`step` `.cpp:327`, `available` `:336`, `adjust` `:436`, `home` `:491`) | The channels of a fixture selection for (bank, slot), 16-bit pairs handled (`.cpp:39`), hue/sat over RGB (`:399`), looks that drive a channel refused. **Relative only**: no absolute set for a fader |
| Workbench target | `ui/src/workbenchencoders.cpp:51` (`encoderTarget()`), wiring `:100` | "The selection": Look Editor's look → scene + selected fixtures → a stack layer; a generated cue layer refused with words |
| Operate refusal | `encoderbank.cpp:496`, `ui/src/pmjoverlay.cpp:304` | In Operate encoders and PMJ faders edit nothing; banks still step |
| PMJ overlay | `ui/src/pmjoverlay.cpp:56-94` (role table), `:374` (`faderInUse`), `:543` (Level), `:603` (Page), `:611` (Param) | Page keys Effects/Groups/Looks/Macros/Fix Cont only light (`m_activePage`, `pmjoverlay.h`); faders follow the focused look's type (Colour → R G B W A UV on 1-6, Dimmer → 1) or else selection intensity; Enc 1/2 = pan/tilt always, Enc 3/4 unused |
| Role vocabulary | `engine/src/controlsurface.h` (`RoleType::Param`, `Page`, `Level`, `Select`, `Load`) | Already has "Param E on the current page" and "Page id" |
| Desk record | `engine/src/deskrecord.h` (roles STOP/GM/GO/Back by device identity, QSettings `settingsGroup()` `:126`, `exportTo`/`importFrom` `:251`) | Per-computer, by identity, exportable: the pattern page sets follow |
| Surface layout | feat/053-surface-map: `engine/src/surfacelayout.h` (`Control` kinds, `Role` with `seed`, **`Page {name, labels}` `:100`, read at `surfacelayout.cpp:182`**), `resources/inputprofiles/layouts/{Akai-APC40-mkII,PMJ-Black-1}.layout.json` | Every control drawn, with kind (fader / knob / encoder / pad), face name and lamp; `selfTest()` against the profile |
| Pick-up | `engine/src/faderpickup.h`, `grandmasterpickup.h` | Catch for absolute, non-motorised faders (Q299, Q313, Q316) |
| Ownership | `ui/src/controlowners.h:77` (`Encoder`, `Pmj` kinds), `InputOutputMap::setStaticControls()` (`inputoutputmap.h:965`) | One answer to "who has this control"; static controls keep show listeners off |

Decisions it must keep: Q306 (static roles first, then speed/tap/blind/next-prev,
**then Design editing: encoder pages, look faders**), Q307 (a type is fixed),
Q309 (static wins over a show's trigger), Q300 (RGBW / pan / tilt faders refused
in Operate for now), Q321 (rides drop at GO), Q311 (map in Control surfaces +
footer chip), STRATEGY guardrails 1 and 4 (attach to existing models; menu count
flat).

## 2. The model

```json
{
  "format": 1,
  "name": "Build",
  "fits": { "profile": "Akai APC40 mkII", "layout": 1 },
  "keys": { "next": 59, "previous": 58, "direct": [87, 88, 89], "hold": [] },
  "always": [
    { "ch": 7,  "param": "red" },  { "ch": 8,  "param": "green" }, { "ch": 9, "param": "blue" },
    { "ch": 10, "param": "white" },{ "ch": 11, "param": "amber" }, { "ch": 12, "param": "uv" },
    { "ch": 14, "role": "gm" }
  ],
  "pages": [
    { "name": "Move", "key": 87, "controls": [
        { "ch": 16, "param": "pan" },  { "ch": 17, "param": "pan", "fine": true },
        { "ch": 20, "param": "tilt" }, { "ch": 21, "param": "tilt", "fine": true },
        { "ch": 18, "param": "focus" },{ "ch": 19, "param": "zoom" },
        { "ch": 52, "param": "gobo" }, { "ch": 53, "param": "gobo-rotation" } ] }
  ]
}
```
*(channel numbers illustrative; the real ones come from the layout file)*

- **Parameter vocabulary** = EncoderBank's 22 slot names flattened to keys
  (`intensity`, `hue`, `saturation`, `red` … `uv`, `pan`, `tilt`, `zoom`,
  `focus`, `iris`, `frost`, `strobe`, `shutter`, `gobo`, `gobo-rotation`,
  `prism`, `prism-rotation`) plus `effect-speed` and `effect-size` (the
  selected effect layer, through the Workbench's editor path) and `none`.
  Labels default from the parameter ("Pan fine"); a page may override one.
- **Absolute vs relative comes from the control, not the file**: the layout
  says knob / encoder / fader (`surfacelayout.h` `Kind`), the profile says
  relative encoding. A fader sets (0-100 % of the attribute, with pick-up);
  a knob or encoder turns by EncoderAttributes' step. One place for the rule.
- **Fine**: `"fine": true` on a turning control = the same 16-bit pair at the
  fine step (`step(..., fine)`, `encoderattributes.cpp:327`). That is his
  "1,2 H and H fine": knob 1 moves pan in coarse steps, knob 2 the same pan in
  fine steps. A push held while turning stays fine on any knob (as today). A
  fader marked fine is refused by the checker (a fader has no "fine" travel
  worth having).
- **`always`**: controls mapped on every page (his "colours on the faders" stay
  put while the knobs page). A page lists only what it adds.
- **Static roles are not page controls**: `role` entries only *draw* a role the
  layout already has (GM, GO, STOP, Back); the checker refuses a page parameter
  on a role channel or on a page key. GM and STOP can never be paged away.

**Code shape.** `EncoderAttributes` gains a parameter-keyed entry point and an
absolute `set(param, norm)`; the (bank, slot) API stays as a thin map over it.
`EncoderBank` keeps its device side (encodings, coalescing, rings, pushes) as
the adapter for relative controls, and its five banks become the built-in
page set "Attributes" (the same eight slots per bank), so an X-Touch or Twister
user sees no change. A new engine `SurfacePages` (one per enabled surface) owns
the current page, the keys and lamps, and emits `adjusted / set / homed(param,
…)` to the one consumer, the Workbench (`workbenchencoders.cpp:100`). The PMJ
overlay's Level / Param / Page cases (`pmjoverlay.cpp:543-640`) become calls
into its `SurfacePages`; `faderInUse()` becomes the "Looks" page's rule.

## 3. What a page acts on

| Mode | Target | Writes | Safety |
|---|---|---|---|
| **Design** | `Workbench::encoderTarget()`: the open look → the focused scene's selected fixtures → a stack layer; with no Workbench, the programmer selection (as the PMJ's selection mode does, `writeChannelLive`) | The look / scene, one undo step per gesture (PaletteOps), as the encoders do today | A parameter no selected fixture has: the control is dark on the map and lamps, a turn says so in the encoder line |
| **Operate** (slices 1-2) | nothing | nothing | Today's refusal, said once a second (`pmjoverlay.cpp:304`). The page's controls are **released** to the show: a fader the show assigned (Assign to Control…) works as the show says. In Design the page owns them (a show trigger on them is listed as blocked, Q309) |
| **Operate busk** (slice 3, only if Q-SP2 = yes) | the selected fixtures on stage | a ride over the playing cue, S5's machinery | drops at the next GO (Q321); Store keeps it into the cue (Q298); a red chip "BUSK: Move page" while any ride holds; only pages ticked "Busk" |

Rules on every page: never the GM, STOP, GO or Back (static, `deskrecord.h`);
a page switch or a new selection never moves a value (absolute faders wait to
pick up: `faderpickup.h`; relative knobs start from where the value is); a
page key is never a show trigger; nothing on a page fires a cue.

## 4. Switching pages, and showing which one

- **Direct keys** (one per page, lit when on): PMJ: Effects, Groups, Looks,
  Macros, Fix Cont (`pmjoverlay.cpp:90-94`, already `RoleType::Page`). APC40
  mk2: the knob-mode row (Pan / Sends / User). Pressing a lit key again steps
  to that key's next page, if it has more than one.
- **Next / previous**: APC40 Device ◀ ▶ (the encoder bank's preset keys
  already, `encoderbank.cpp:86`); X-Touch notes 13/14.
- **Hold**: a key that shows a page only while held (e.g. Shift on the APC40
  for a "fine" or "effects" page), back on release.
- **Shown**: the surface map labels every control with the current page's job
  (`SurfaceLayout::Page::labels` is filled from the set, not hand-written in
  the layout); the footer desk chip (`ui/src/deskchip.h:43`) adds the page:
  "APC40 · Move"; the Workbench's encoder line names the page and the touched
  parameter ("Move · Pan fine 42.1°"). Lamps: page keys lit, controls dark
  when the target lacks their parameter.

## 5. Where page sets live

| What | Where | Edit |
|---|---|---|
| Built-in sets | `resources/inputprofiles/pagesets/<layout>.<name>.pageset.json`, beside the layouts, built in as resources | read-only; **Copy to Edit** |
| User sets | the user profile folder's `Page sets/` (as `USERINPUTPROFILEDIR` for types) | editable |
| Which set a surface uses | this computer's QSettings, by device identity (DeskRecord's group) | Control surfaces row: "Pages: Build ▾" |
| Sharing | Export = save the `.pageset.json`; Import = copy it in after the checker passes | — |

The file names the layout it **fits** (profile + layout format); Import refuses
a set for another type or one whose channels the layout lacks, naming them. A
set is not saved in the show (pages are this desk's furniture, Q301 / Q306), so
a show opens the same on a desk with other pages. A page set does not change
the surface type (Q307 stays: roles and layout are fixed; pages sit on top).

## 6. The builder, on the map

```
Control surfaces ▸ APC40 mk2                 Pages: Build (built in) ▾  [Copy to Edit]
[Move] [Beam] [Effects] [+]                               [Export…] [Import…]
┌ map ──────────────────────────────────────────────┐   Device knob 2
│ K1 K2 K3 K4 K5 K6 K7 K8  ← Hue Sat … Gobo GoboRot  │   Parameter: [Pan ▾]  ☑ Fine
│ device: [Pan][Pan f][Focus][Zoom]                  │   Label: Pan fine
│         [Tilt][Tilt f][Iris][Frost]                │   On: this page | every page
│ |R| |G| |B| |W| |A| |UV| |·| |·|  |GM|             │   [Clear]
└────────────────────────────────────────────────────┘
Move a control on the board, or click it here.
```

- Click a control (or move it on the board: the map already highlights it), and
  the side panel shows one parameter list grouped Intensity / Colour / Position
  / Beam / Gobo / Effect / Nothing; fine and "every page" are ticks; the label
  fills itself. Static roles and page keys are drawn but not pickable, with the
  reason ("GM: this computer's Grand Master").
- Page tabs: rename in place, [+] adds, drag to reorder, the key for a page is
  chosen by pressing it ("Press the key for this page").
- A built-in set is read-only with the one button Copy to Edit. Teaching /
  Student: read-only, the PIN unlocks (Q319).
- No menu item, tab or window: it is the hub page's map (Q311), so the menu
  count stays flat.

## 7. First page sets

**APC40 mk2 "Build"** (his words; both knob rows used, so his list needs no paging):

| Controls | Page "Move" (default) | Page "Beam" | Page "Effects" |
|---|---|---|---|
| Faders 1-8 (always) | R, G, B, W, A, UV, —, Intensity | same | same |
| Device knobs 1 2 / 5 6 | Pan, Pan fine / Tilt, Tilt fine (H above V) | Zoom, Focus / Iris, Frost | Effect speed, size / —, — |
| Device knobs 3 4 / 7 8 | Focus, Zoom / Iris, Frost | Strobe, Shutter / —, — | — |
| Track knobs 1-8 | Hue, Sat, —, —, Gobo, Gobo rot., Prism, Prism rot. | Gobo, Gobo rot., Prism, Prism rot. on 1-4 | — |
| Keys | Pan / Sends / User = Move / Beam / Effects; Device ◀ ▶ step | | |

So "focus and zoom on 3,4" and "5,6 V" sit on the device knobs, and "gobos on
5,6" sit on the track knobs' 5,6, all on one page. Master stays GM; PLAY / STOP
/ RECORD keep their desk roles (`Akai-APC40-mkII.layout.json` roles).

**PMJ Black 1** (its five keys; faders 1-10, Enc 1-4, push-hold = fine):

| Key | Faders | Encoders 1-4 |
|---|---|---|
| Fix Cont | R G B W A UV, —, —, —, Intensity | Pan, Tilt, Zoom, Focus (press again: Gobo, Gobo rot., Prism, Iris) |
| Looks | today's rule: the open look's type (Colour → 1-6, Dimmer → 1) | Pan, Tilt, Hue, Sat |
| Groups | intensity of the focused scene's groups 1-10 | as Fix Cont |
| Effects | — | Effect speed, size, —, — |
| Macros | — (lamp lit, "nothing on this page yet") | — |

Select / Load rows stay as today (selection mode) on every page in slice 1.
The golden test holds today's PMJ behaviour on the Looks page.

**X-Touch Mini "Attributes"**: the five encoder banks as pages (no change for
current users), plus "Move": 1,2 Pan / fine, 3,4 Focus / Zoom, 5,6 Tilt / fine;
"Gobo": 5,6 Gobo / rotation (an eight-knob board must page his gobos).

**APC64, MaizeDMX**: no profile or layout yet. After his profiler
(`~/git/qlcplus-midi-profiler`) gives the `.qxi`, the layout comes next, then a
set copied from the APC40 one (APC64's touch strips act as faders).

## 8. Phasing and tests

| Slice | What | Size |
|---|---|---|
| 1 | `SurfacePages` + file format + checker; EncoderAttributes param API and absolute `set`; EncoderBank banks as the "Attributes" set; built-in APC40 "Build" and PMJ sets; keys, lamps, map labels, chip and encoder-line page name; Design only, Operate releases the controls | M (~4 d) |
| 2 | Builder on the map, Copy to Edit, Export / Import, per-computer choice by identity | M (~4 d) |
| 3 | X-Touch "Move", APC mini, APC64, MaizeDMX sets; Busk pages in Operate on S5 rides if Q-SP2 | M each |

Tests (surface simulator, `engine/test/common/surfacesim/`; each fails on old code):
`surfacepages_test::knob2IsPanFine` (APC40 sim, knob 2 moves pan by the fine
step), `::pageKeySwitchesLabelsAndLamps`, `::faderWaitsAfterPageSwitch`,
`::operateReleasesToShow` (a show's fader assignment works in Operate, refused
in Design as blocked), `::gmNeverOnAPage` (checker), `::importRefusesOtherType`,
`pmjoverlay_test::looksPageGolden` (today's colour/dimmer fader rule unchanged),
`encoderbank_test::banksAsPageSet` (XML of old shows still loads into it).

Help: `docs/features/control-surfaces.md` gets "Pages" (what they are, the
keys per board, making your own, sharing); a course note in the controller
lesson ("a page is a labelled layer of the desk: colour on the faders, moves
on the knobs"); a CHANGELOG line per slice; shots at 3 sizes of the map with
labels.

Old shows: an `<EncoderBank>` saved in a show is read and saved back unchanged
and offered once per show per computer (Q308, Use Here / Not Here); Use Here
picks the "Attributes" set for that device on this computer.

## 9. Questions for Branson (recommendation first)

- **Q-SP1. A page set: its own file on top of a fixed type, or part of the type?**
  (a) own file that fits a layout; the type stays fixed (Q307) · (b) part of the
  type: a different page set is a new type (Copy to Edit the type).
- **Q-SP2. Pages in Operate?** (a) Design only now; in Operate the page's
  controls go back to the show's assignments; opt-in Busk pages later on S5's
  rides, dropping at GO · (b) pages ride the stage in Operate from slice 1,
  dropping at GO · (c) dead in Operate (refused, never released to the show).
- **Q-SP3. Per computer or per show?** (a) per computer by device identity; the
  show carries none · (b) (a), plus a show may name the set it was built with
  and offer it on open · (c) per show.
- **Q-SP4. The APC40 clash (his "5,6 V" and "gobos on 5,6").** (a) both knob
  rows on one page: tilt on device knobs 5,6, gobos on track knobs 5,6 (§7) ·
  (b) device knobs only; gobos on a second page.
- **Q-SP5. Numbered targets ("fader N = group N", "fader N = fixture N").**
  (a) only the PMJ's two built-in rules in slice 1; numbered targets in the
  builder later · (b) in the builder from slice 2.
