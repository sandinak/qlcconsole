# Workspace levels: Student, Standard, Advanced

Status: **design, for the board** (2026-11-25). No code yet. It builds on
board Q249 (2026-11-15) and on what is already shipped: the workspace views
(Q35/Q36, `App::WorkspaceView` in [ui/src/app.h](../../ui/src/app.h)), Tabs &
Features (Q262/Q293, [ui/src/showfeatures.h](../../ui/src/showfeatures.h)) and
Teaching mode ([docs/features/teaching.md](../features/teaching.md)).

**Decided (Q249):** a **Level** sits beside the view in **View ▸ Workspace**:
**Student / Standard / Advanced**. The view picks the tabs, and the level
picks how much each tab shows. Student is the Show Tree, Run and focus calls,
with no Build internals, raw channels or advanced settings, and every greyed
item says why. Standard is today's qlcconsole view. Advanced is everything,
plus the "advanced config" from board Q10. The level is saved per show.
Teaching forces Student, and the instructor PIN unlocks the higher levels.
There are three levels. A hidden item always has a findable "switch level"
path, never a dead end.

The bar: *"if I have to explain everything we failed."* A student finds what
the lesson needs without being told where it is. A pro finds everything,
and never runs into a lock without a way past it.

---

## 1. The model

### 1.1 Where the level lives

The level is one attribute on `<Workspace>`, beside `View`, `Tabs` and
`FeaturesOff` (`App::saveXML`, [ui/src/app.cpp](../../ui/src/app.cpp) near
`KXMLQLCWorkspaceView`):

```xml
<Workspace View="qlcconsole" Level="student" Tabs="…" FeaturesOff="…">
```

- **QLC+ 4 safe.** QLC+ 4 and older qlcconsole builds read past an unknown
  attribute, as they do for `View` today. A show saved through QLC+ 4 loses
  the attribute and opens at Standard.
- **Absent means Standard.** Every show made before levels opens exactly as
  it does today. Because nothing is written at Standard, an untouched show's
  file does not change.
- **Unknown value** (written by a newer build): read as Standard, logged,
  and written back unchanged on save (the `FeaturesOff` pattern: drop what
  we don't know, never fail the load).
- Setting the level marks the show modified but is not an undo step. The
  view works the same way.

The level is not a QSettings value. A show handed to a student carries its
level to the student's computer, as it carries its Teaching flag.

### 1.2 How it combines with the view, Tabs & Features and Teaching

An item is **shown** when all four of these agree:

```
shown(key) = view has its tab            (View ▸ Workspace; Tabs & Features' tab list)
          && level >= minLevel(key)      (this design)
          && feature(key) is on          (Tabs & Features' feature list, if the key has one)
          && Teaching's lock allows it   (an effective level, below)
```

- **View × level.** The level applies in the **qlcconsole** and
  **Everything** views. **Classic QLC+** is stock QLC+'s surface, and a
  level would gut it, since almost every Classic tab is a Build internal or
  a raw channel. So in Classic the Level items are greyed ("Classic QLC+
  shows QLC+'s own tabs in full"), and the show behaves as Advanced. When
  Student becomes the effective level of a Classic show (a Teaching show,
  or the person picks Student), the show switches to the qlcconsole view.
  The notice line says so and offers **Back to Classic QLC+** (PIN in
  Teaching). This is question Q-L5.
- **Everything at Student** is the qlcconsole view's tabs plus the
  Everything-only tabs (Build, Functions, Simple Desk) behind their switch
  path. It looks like Student-in-qlcconsole with three more Window-menu
  entries.
- **Tabs & Features** stays the fine-grained switch, and the level is a
  ceiling over it. A feature the level hides shows in that dialog greyed,
  with "Standard level". A feature turned off stays off at every level. The
  same law holds as for features: **hidden, never stopped.** A cue that uses
  an effect still plays it at Student.
- **Never hidden at any level** (the ShowFeatures rule, extended): Stop All,
  Blackout, GO, Back, Sub GO, Sub Back, Pause/Stop, the Grand Master, Blind,
  Design | Operate, the footer's readiness, Help, and anything showing a
  live state that holds the rig (Overrides chip, Park, a venue applied,
  Teaching's "output held"). A student must always be able to stop the
  rig, and see why it is the way it is.
- **Teaching.** A Teaching show's **effective level** is the level saved in
  it. When Teaching is first turned on, and on Make Student Handout, that
  level is set to Student. While Teaching is on, raising the level asks for
  the PIN (Q-L2). An unlock lasts until the show is closed and is never
  saved, so a student copy always reopens at its saved level (Q-L3).
  Lowering the level never asks. With no PIN set, Teaching still resets the
  level to Student but asks for nothing, as Teaching's other switches do.

### 1.3 One central check: the level registry

Today each feature check is an `if` at its call site:
`ShowFeatures::isOn` is called in 17 files. Levels would touch about ten
times as many places, so they get **one registry and a binder**, not
scattered `if`s:

- **`ui/src/showlevel.{h,cpp}`**: `enum Level { Student, Standard,
  Advanced }`, `ShowLevel::instance()`, `level()`, `effectiveLevel()`
  (Teaching applied), `setLevel()`, the `Level` attribute's read and
  write, and a `changed()` signal, as on ShowFeatures.
- **`ui/src/levelregistry.cpp`**: **the one table**, one row per feature
  key:

  ```cpp
  { "file.import",        Standard, Fold, tr("Importing another show or an Eos file") },
  { "tab.simpledesk",     Standard, Tab,  tr("Raw channel faders") },
  { "showtree.cue.timecode", Standard, Grey, tr("Cues at a timecode") },
  { "settings.performance",  Standard, Grey, tr("This computer's tuning") },
  { "settings.engine",       Advanced, Fold, tr("Engine timing and universe threads") },
  ```

  Each row has a **key**, the **lowest level** that shows it, a
  **treatment** (§4: Fold, Grey or Tab), the **words** the switch path
  uses, and optionally the `ShowFeatures::Feature` it also follows.
- **Binding, not branching.** A widget or action declares its key once,
  where it is made: `ShowLevel::bind(action, "file.import")`, or the
  dynamic property `levelKey` on a widget made in a `.ui` file.
  `ShowLevel::apply()` alone decides visibility, enabled state, tooltip and
  the "More at Standard…" lines, on load and on every `changed()`. A
  widget never asks "what level is it?" itself. The one exception is code
  that must refuse an *action* (the command line, a drop, a shortcut). It
  calls `ShowLevel::shown(key)` and, when that says no, returns the
  registry's sentence (UX-28).
- **Rows, not code, at review.** A new feature that should not be at
  Student is one row. The gate fails a key that is bound but has no row,
  a row that nothing binds, and a level-hidden action with no switch path
  (§6).

---

## 2. Tab by tab

Here is what each level shows in the qlcconsole and Everything views.
**Standard is today**, so that column only says where Advanced adds
something. "Grey" means shown, greyed and saying why. "Fold" means taken out
of its menu or toolbar into that menu's one **More at Standard…** line.
"Tab" means the tab is hidden and listed in the Window menu's switch line.
Names come from the UI as built (the files named) and from
[docs/features](../features/).

### Show Tree ([showtreeview.cpp](../../ui/src/showtreeview.cpp), [show-tree.md](../features/show-tree.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Tree: productions, cue lists, cues, layers, ⤳ rows, S T R M F columns | shown | shown | shown |
| Header: + Cue List, + Cue / + Hit, + Layer, Record, Delete, Undo/Redo | shown | shown | shown |
| Header: ✎ Workbench | Grey (the Workbench's stock editors are Build internals) | shown | shown |
| More ▾: + Sub-cue, Merge | shown (course 2.3, 2.7) | shown | shown |
| More ▾: + Production, Production switcher | Fold (one production per class show) | shown | shown |
| MIB (auto move-in-black) | Grey | shown | shown |
| Upgrade This Show… banner | Fold (a handout is already upgraded) | shown | shown |
| Programmer strip: Highlight, Flash, Park ▾, Tap | shown | shown | shown |
| Programmer strip: Move in Black ▾, Beat ▾, Save Positions, More ▾ (Snapshot, Jog speed) | Fold into the strip's » | shown | shown |
| Library: groups, group sets, targets, palettes, effects, + Palette, effect browser | shown (course 1.1 to 2.5) | shown | shown |
| Library: functions (raw scenes, chasers), Ungrouped fixtures | Fold | shown | shown |
| Cue page: number, name, note, fades, hold, mode, base, Skip, sub-cues, looks, layers, *Next cue receives*, Tracked in | shown | shown | shown |
| Cue page: Overlapping layers mix, Don't move in black, Text colour | Grey | shown | shown |
| Cue page: At timecode / Learn; list page: Follow timecode, Timecode row | Grey (Level 3.5) | shown | shown |
| Cue page: Make This a Venue Focus Cue, Focus This Cue… | Grey (Level 3.6) | shown | shown |
| List page: Convert to tracking…, Detach to Plain Chaser… | Fold | shown | shown |
| Layer page: attribute rows, ⋯ Set to / Level… / Colour…, Make reusable, Split / Merge / child layers | shown | shown | shown |
| Layer page: typed raw channel values | Grey ("values by name at Student") | shown | shown |
| Hits: + Hit, Make VC Page, Move to Folder… | shown (course 2.6) | shown | shown |
| Look Editor (✎ on a value or palette), Used by | shown | shown | shown |
| Command line | shown; a command for a hidden item answers with its level and a Switch | shown | shown |
| Cue Sheet… / tracking sheet, Running Order…, Numbering | shown (paper is part of running a show) | shown | shown |

### Run ([cuerunner.cpp](../../ui/src/cuerunner.cpp), [show-tree.md § The cue runner](../features/show-tree.md#the-cue-runner))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| GO, Sub GO, Back, Sub Back, Pause/Resume, Stop, the list, LOAD, jump, What changes | shown, never hidden | shown | shown |
| LIVE / DESIGN, the production/list box | shown | shown | shown |
| Venue Focus…, Start Venue Focus | shown (focus calls are Student) | shown | shown |
| Group master faders | shown (course 3.8 demo; harmless) | shown | shown |
| GO log (Teaching only) | shown | shown | shown |
| Timecode readout | Grey when there is no source | shown | shown |
| Pad button | shown when the pad has a state to say (as today) | shown | shown |
| After a crash: Load as Standby | shown | shown | shown |

### Build and the Workbench ([programmingmanager.cpp](../../ui/src/programmingmanager.cpp), [workbench.cpp](../../ui/src/workbench.cpp), [build.md](../features/build.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Build tab (Everything only since Q228) | Tab | shown (Everything) | shown |
| Functions tab (Classic, Everything) | Tab | shown (Everything) | shown |
| Workbench docked in the Show Tree (✎ Workbench) | Grey | shown | shown |
| Workbench editor pane: channel console, stock scene/EFX/RGB Matrix editors | not reachable (Workbench greyed) | shown | shown |
| Effect script editor, Writing your own (effects.md) | Fold | Fold into Advanced | shown |
| Encoder banks | Fold | shown | shown |
| Dump DMX Values to a Function (Control menu) | Fold | shown | shown |

### Fixtures ([fixturemanager.cpp](../../ui/src/fixturemanager.cpp), [fixture-manager.md](../features/fixture-manager.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| The tab, the fixture/group tree, selection, right-click Selection ▸ New Layer from Selection | shown (course 2.3) | shown | shown |
| Inventory panel, Add, Patch dialog, Remove, Return to inventory | Grey (Teaching already asks before a patch change; Level 3.1) | shown | shown |
| Group folders, Create Fixture Group, head-layout grid editor, LED Layout, Split by Head Type, Lay Out from the Stage | Grey | shown | shown |
| Group master | shown (read; the fader is Run's) | shown | shown |
| Output rules…, Change Definition, Plot Symbol ▸, Power ▸ | Fold | shown | shown |
| Patch list, Print Labels… | Fold | shown | shown |

### Lighting Studio and plot ([monitor/monitor.cpp](../../ui/src/monitor/monitor.cpp), [lighting-studio.md](../features/lighting-studio.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Planes: Top, Front / Side, Angled; Rig Overview (⇧⌘R) | shown (course 0.8) | shown | shown |
| Selecting on the plot, Why is this on? (⌘I), Blocking ▾ | shown (course 2.1, 2.5) | shown | shown |
| Overlay box: Normal, Stage only | shown | shown | shown |
| Overlay box: Power, DMX, Network, Owner | Fold out of the box (one "More views at Standard" item) | shown | shown |
| Focus ▾: Quick Focus, Venue Focus…, Practice (scramble)… (Teaching) | shown | shown | shown |
| Focus ▾: Venues, Focus Report…, Edit Focus Calls… | Focus Report shown (a hand-in, 3.6); Venues and Edit Focus Calls Grey | shown | shown |
| Stage structures, mounting, Inventory panel, Fixture Properties edits, Plot layers' locks, scenery, pixel paths, wand | Grey (Level 3.4); Fixture Properties opens read-only | shown | shown |
| Ambient (Work light…), Haze | shown | shown | shown |

### Connections ([inputoutputmanager.cpp](../../ui/src/inputoutputmanager.cpp), [controlsurfacespanel.cpp](../../ui/src/controlsurfacespanel.cpp), [control-surfaces.md](../features/control-surfaces.md))

Connections is a required tab (`App::tabIsRequired`): a dead output shows
there, so it stays at every level.

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Devices: each device and universe, reachable or not, "Output held — Teaching" | shown, read-only | shown | shown |
| Devices: Edit connection…, patch / unpatch / retarget / delete, + universe | Grey (Level 3.1) | shown | shown |
| Universes page (QLC+'s Inputs/Outputs editor) | Grey (the sub-tab shows, its editor greyed) | shown | shown |
| Show control & sync rows: OSC, MSC, MTC | Grey (Level 3.5) | shown | shown |
| Control surfaces hub: rows, Who has it now, Triggers by place | shown, read-only | shown | shown |
| Control surfaces hub: every Set Up…, Enable (CONTROL_SCOPES §3) | Grey (CONTROL_SCOPES already says read-only at Student; the PIN guards desk changes, Q319) | shown | shown |
| The surface map (S1) and surface pages (feat/053-surface-pages) | shown, read-only: moving a control lights it | shown | shown |
| Learn Trigger… / Assign to Control… on a cue, hit, scene | Fold (Level 3.5 extension) | shown | shown |
| Desk static roles: GM, Stop, GO, Back, Blackout | live, never hidden | live | live |
| Device setup: pad test and jobs, Controller test | shown (a student can test the pad), jobs Grey | shown | shown |
| Web access, the phone Run page's Allow GO | Grey | shown | shown |

### Virtual Console ([virtualconsole/virtualconsole.cpp](../../ui/src/virtualconsole/virtualconsole.cpp))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| The tab, pages, pressing buttons and moving sliders in Operate | shown (course 2.6) | shown | shown |
| Pages made by Make VC Page | shown | shown | shown |
| Design-mode editing: add widget, Properties, External Input | Grey ("built by Make VC Page at Student") | shown | shown |
| Settings (Grand Master page and others) | Grey | shown | shown |
| New Programmer Frame from Input Profile… | Fold | shown (CS10 retires it) | shown |
| Live Edit (Control ▸ Live Edit) | Fold | shown | shown |

### Simple Desk ([simpledesk.cpp](../../ui/src/simpledesk.cpp))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| The tab or the on-demand tab (Window menu) | Tab (raw channels) | shown | shown |
| Channel faders, Cue Stack, Channel groups | not reachable | shown | shown |

### Timeline (Shows tab)

| Item | Student | Standard | Advanced |
|---|---|---|---|
| The tab | Tab (no course lesson before Level 3.5) | shown | shown |
| Control ▸ Follow MTC, Exit Timeline Control, Hold at End | Fold | shown | shown |

### Show Status and Review ([showstatusdialog.cpp](../../ui/src/showstatusdialog.cpp), [show-review.md](../features/show-review.md), [dress-rehearsal.md](../features/dress-rehearsal.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Footer readiness chip, Not ready list, Overrides chip | shown, never hidden | shown | shown |
| Review chip, panel, Fix ▸, Not a Problem… | shown (course 3.9; feature switch still applies) | shown | shown |
| Before the House Opens… | shown | shown | shown |
| Its fixes that open a hidden place (Settings ▸ Performance, Open Connections editing) | the button says "Standard level" and offers the switch | shown | shown |
| Engine Load chip, Power chip | as Tabs & Features says | shown | shown |

### Focus calls ([focus-calls.md](../features/focus-calls.md), [refine.md](../features/refine.md))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Venue Focus…, running the calls on the pad or keys | shown (Q249: focus calls are Student) | shown | shown |
| Practice (scramble) (Teaching), focus report | shown | shown | shown |
| Quick Focus (View menu, Lighting Studio) | shown | shown | shown |
| Edit Focus Calls…, Venues: switch, make, the venue page, clash check | Grey (the teacher sets them; a student runs them) | shown | shown |
| Focus This Cue / Refine | Grey | shown | shown |

### Settings and Show Settings ([appsettings.cpp](../../ui/src/appsettings.cpp), [showsettingsdialog.cpp](../../ui/src/showsettingsdialog.cpp))

| Item | Student | Standard | Advanced |
|---|---|---|---|
| Settings ▸ General, Appearance | shown | shown | shown |
| Settings ▸ Advanced (Output for this show, Auto-open shutters, Game controllers, Scripts, Effects) | Grey page with the switch line; Auto-open shutters' state is read out | shown | shown, plus §2.1 |
| Settings ▸ Performance | Grey page (Before the House Opens still reads it) | shown | shown, plus §2.1 |
| Settings ▸ AI Assist | Grey page | shown | shown |
| File ▸ Show Settings… (Operate, Teaching, PIN) | shown; its Teaching and PIN parts ask the PIN as today | shown | shown |

#### 2.1 What Advanced adds (board Q10)

Q10's "a tab for advanced config" shipped on 2026-10-21 as Settings ▸
Advanced, and today everyone sees it. So that Standard stays today's
surface, Advanced adds **new** expert controls, never ones taken out of
Standard (Q-L4):

- **Settings ▸ Advanced ▸ Engine**: settings that exist only as QSettings or
  environment values today: the engine's frame rate, real-time universe
  threads (the perf/host-leverage branch's A/B), output thread priority,
  autosave interval.
- **Raw values beside names**: the Show Tree's layer rows and the Look
  Editor show the DMX value next to the palette name, and the Run tab's
  What changes line can show channels.
- **The effect script editor** and the script folder (Settings ▸ Advanced
  ▸ Scripts) move up one level, from Standard to Advanced. This is the one
  Standard loss, and it needs Q-L4's yes.
- **Diagnostics**: Copy Layout Report and the engine's per-universe timing,
  in the Help ▸ Troubleshooting submenu that already exists.

### Menus ([ui/src/app.cpp](../../ui/src/app.cpp) `App::initMenuBar`)

| Menu | Student | Standard (today) | Advanced |
|---|---|---|---|
| File | New, New from Template ▸, Open, Open Recent ▸, Save, Save As, Show Settings…, Settings, Quit; Fold: Import ▸, Collect for Transfer…, Show Definitions…, Upgrade This Show… | all 13 | same |
| Edit | Undo, Redo, New | same | same |
| View | Workspace ▸ (views, **levels**, Tabs & Features…), Rig Overview…, Quick Focus…, Venue…, Blind, Full Screen; Fold: Address Tool, DMX Monitor | all 8 | same |
| Fixtures | one line: "Fixture tools are at Standard level…" | Fixture Editor…, Definition Updates…, Import, Export, Remap, Patch List, Channels Fade | same |
| Control | Design/Operate, Blackout, Stop All, GO, Back, Sub GO, Sub Back; Fold: Dump DMX, Live Edit ▸, Follow MTC, Exit Timeline Control, Hold at End, Clear Hold at End | all 13 | same |
| Window | the level's tab jumps, one "More tabs at Standard…", Rig Overview, Put Back ▸, Put All Back, Move to Its Own Window, Shift-Click…, Minimize | all | same |
| Help | all, never folded | all | + Troubleshooting ▸ diagnostics |

---

## 3. The menu count (STRATEGY, UX-44)

The count is the menu bar's **top-level entries**: each menu's own items,
with a submenu counted as one entry and separators not counted. These
numbers are read from `App::initMenuBar` on origin/main 63dafbfa4, with
Window counted in the qlcconsole view (10 tab jumps + 6).

| Menu | Today (= Standard) | Student | Standard with levels |
|---|---:|---:|---:|
| File | 13 | 9 + 1 More = 10 | 13 |
| Edit | 3 | 3 | 3 |
| View | 8 | 6 + 1 More = 7 | 8 |
| Fixtures | 7 | 1 | 7 |
| Control | 13 | 7 + 1 More = 8 | 12 (Hold at End folded) |
| Window | 16 | 6 + 1 More + 6 = 13 | 15 (Shift-Click moved) |
| Help | 10 | 10 | 10 |
| **Total** | **70** | **52** | **68** |

- **Student falls by 18 (−26%).** The five Live Edit sub-items go too.
- **Standard falls by 2 at the top level, and is flat counting every
  item.** The level adds three radio items *inside* View ▸ Workspace
  (4 → 7, under UX-80's 14). They are paid back in the same release by
  three folds already queued:
  1. **Hold at End** becomes a checkable item, and unticking it clears the
     hold. **Clear Hold at End** goes (−1).
  2. **Shift-Click a Tab to Open It in a Window** is a preference, not a
     window, so it moves to Settings ▸ Appearance ▸ Window (−1).
  3. **CS10**: VC ▸ New Programmer Frame from Input Profile… is retired, as
     CONTROL_SURFACES_AUDIT recommends (−1).
- **Advanced** adds only inside Settings and Help ▸ Troubleshooting. No new
  menu, tab or top-level item.

`levels_test::theMenuCountFallsOrStaysFlat` holds these numbers: Student
must be at most 52 and Standard at most 68 at the top level, and counted
item by item (submenus opened out, the VC's Add menu in) Standard at most
98 and Student at most 75, with Advanced equal to Standard. CS10 shipped
with L1's review (the wizard is gone), so the paid-back count holds: the
top-level 68 alone hid that Standard was one item over. A later change that raises
either has to change the test, with its reason in the commit.

---

## 4. Discoverability: never a dead end

There are three treatments. The registry row picks one, and each comes with
its switch path.

1. **Grey** is for a control on a page you are looking at (a cue page
   field, a Settings page, an Edit connection button). It stays where it
   is, greyed. Its tooltip gives the reason in the registry's words and the
   way out: *"Cues at a timecode: Standard level. View ▸ Workspace ▸
   Standard."* (UX-27). Clicking the greyed control (or pressing its key)
   opens the **notice line** above the tabs, which already exists
   (`App::showViewNotice`): *"Cues at a timecode are at the Standard
   level."* **[Switch to Standard]** **[×]**. A whole greyed page (Settings
   ▸ Performance) carries the same line at its top.
2. **Fold** is for menu items, toolbar buttons and combo entries. They leave
   their menu, and the menu ends with one **More at Standard…** line, in
   the menu's own band (UX-80). It opens a small list, *"At Standard this
   menu also has: Import ▸, Collect for Transfer…, …"*, with **Switch to
   Standard**. The menu count falls, and nothing disappears without a
   trace.
3. **Tab** is for a whole tab. It leaves the tab bar. The Window menu,
   which already lists tabs the view hides, gets one line: **More tabs at
   Standard… (Build, Simple Desk, Timeline)**. Its tab shortcut (⇧⌘n), a
   help link or a "show it" button elsewhere lands on the same notice. This
   is how `App::showContext` already handles a tab the view hides. The one
   difference is that a level is never raised silently: it is always the
   notice with its button, never an automatic switch.

**The same path from everywhere:**

- **Help.** F1 on a greyed control opens its help topic, and that topic
  opens with a line "This is at the Standard level: Switch". HelpBrowser
  search finds hidden features. A help link to a hidden place goes to the
  notice.
- **The command line.** A command for a hidden item answers *"`timecode`
  is a Standard-level command."* with Switch, rather than "unknown command".
- **Shortcuts.** A shortcut for a folded action answers in the notice line.
  It never does nothing (UX-28).
- **The footer.** The mode area shows a **STUDENT** chip at Student, and an
  **ADVANCED** chip at Advanced. A click opens a menu with the three levels
  and "What each level shows". Standard has no chip, as today.

**Switch to Standard in a Teaching show** asks for the PIN: *"This is a
Teaching show. The instructor's PIN opens the Standard level until the show
is closed."* A wrong PIN changes nothing. With no PIN set, it switches
without asking. The TEACHING chip's menu gets **Unlock Standard / Advanced…**
in place of a new menu item.

**What Teaching does, in one list:** it holds output (as today), sets the
show to Student on turn-on and on Make Student Handout, asks the PIN to go
up a level, and puts the level in the focus report's Teaching line.
[teaching.md](../features/teaching.md) loses its sentence "Teaching never
changes which tabs you see". The new sentence: "Teaching sets the Student
level; the PIN opens the others."

---

## 5. The course ([docs/course](../course/), COURSE_PLAN §2)

| Course level | Skill level | Why |
|---|---|---|
| Level 0, Lighting concepts (0.8: the virtual stage, Run tab, Rig Overview) | Student | GO three times, find the stage |
| Level 1, Basics (1.1 to 1.5: cues, layers, colour, aim, timing, Operate, Run) | Student | Show Tree basics, Run |
| Level 2, Intermediate (2.1 to 2.7: tracking, palettes, child layers, effects, lines, hits + Make VC Page, sub-cues) | Student | All of it is Show Tree, Library, Lighting Studio viewing and VC pressing (Q-L1) |
| Level 3, 3.1 to 3.5 and 3.7 (patching, definitions, plot symbols, building the rig, show control, output rules) | Standard | Connections editing, Fixtures, the Lighting Studio's structures, Settings ▸ Advanced |
| Level 3, 3.6 (focus and calibration), 3.8, 3.9 | Student is enough for the practice and the running; Standard for Focus This Cue and Venues | the handout sets the level |
| Level 3, 3.10 (Coming from Eos) | Standard | File ▸ Import |

- Each lesson's start file is a Teaching show saved at its level. Level 3
  handouts are made at Standard (Make Student Handout gets a **Level** box,
  Student by default). For this the level must be the one saved in the
  show, not a fixed Student (Q-L2).
- **Course text changes.** Lesson 1.3's troubleshooting line *"Settings →
  Advanced → Auto-open shutters"* becomes *"ask your teacher (it is in
  Settings ▸ Advanced, Standard level)"*. Settings ▸ Advanced reads the
  state out at Student, so the student can see it is off. Lesson 0.8 gets
  one sentence: "the footer's STUDENT chip means the show is set up for
  the class".
- **The course test** (`lessonTwoFiveFormation` and its siblings) runs each
  lesson's steps at the lesson's own level. A lesson that needs an item
  above its level fails the gate.
- A **course note** for the teacher guide: what the three levels are and
  how the PIN unlocks them, written in crew voice, WHY before HOW.

---

## 6. Phasing and tests

ROI line (STRATEGY): the classroom bar (0.3.1/0.5) and a pro surface that
stays whole. The cost is M for the model plus about 150 registry rows. The
payoff: Level 0–2 students never meet an item they cannot use, and no
existing show changes.

| Phase | What | Size | Tests that fail on today's code |
|---|---|---|---|
| **L1 — the model** | `ShowLevel`, the registry, `bind()`/`apply()`, the `Level` attribute, View ▸ Workspace levels, the footer chip, the notice line's Switch, Teaching's reset and PIN unlock, Classic's rule; **menus and tabs only** (§2's Menus rows, Tab rows); the three Standard folds | M | `levels_test::theLevelRoundTripsAndQlc4IgnoresIt`, `::anOldShowOpensAtStandard`, `::teachingSetsStudentAndThePinUnlocksForTheSession`, `::theMenuCountFallsOrStaysFlat`, `::aHiddenTabIsInTheWindowMenuSwitchLine`, `::safetyControlsAreNeverHidden` |
| **L2 — the course path** | Show Tree, Run, Lighting Studio, Fixtures rows; command-line and shortcut answers; Make Student Handout's Level box; course text and start files re-saved | M | `levels_test::everyLevelHiddenItemHasASwitchPath`; the course tests at each lesson's level |
| **L3 — the rest** | Connections and the hub, VC, Simple Desk, Settings pages, Show Review fix buttons; Advanced's §2.1 additions | M | `levels_test::advancedAddsOnlyInsideSettingsAndHelp` |
| **L4 — help and shots** | `docs/features/getting-started.md` § Levels, teaching.md, help.qrc + kTopics, CHANGELOG, a course note; shots at 3 sizes per level for the ux-reviewer | S | `helpbrowser_test`, `menuwords_test` |

**Gate tests, per level (Student, Standard, Advanced) × (qlcconsole,
Everything):**

- **`layoutsanity_test` and `windowsanity_test`** run every case at each
  level. Greyed pages still fit at 1024×640, and a folded toolbar's »
  still works.
- **`levels_test::everyLevelHiddenItemHasASwitchPath`** walks every widget
  and action that has a `levelKey` at Student. Each must be either greyed
  with a tooltip that names its level, or folded with its menu holding a
  More line. Triggering it must raise the notice with a Switch button, and
  pressing Switch (PIN given in a Teaching case) must make the item usable.
  This is Q249's "never a dead end" as a test.
- **`levels_test::theRegistryIsWhole`** checks that every bound key has a
  row and every row is bound. It reads the sources, the way
  `menuwords_test` does.
- **`menuwords_test::noStudentTextSendsYouToAHiddenPlace`** extends
  `noTextSendsYouToAHiddenBuild`. No Teaching glossary line or Student-shown
  tooltip may name a place hidden at Student.
- **`contextmenus_test::everyMenuKeepsTheLaws`** checks that the More line
  sits in its band, that a fold never leaves a submenu with one item, and
  that a fold never leaves an empty menu with no line.
- **The walker / ux-reviewer** gets one packet per level. Its rule book
  gains "a level-hidden item says its level" (UX-27, UX-28).
- **The time budget.** `levels_test` stays under 10 s. Everything it needs
  is built offscreen in one App.

---

## 7. Questions

The decisions and the code do not settle these.

- **Q-L1, how wide Student is.** The course's Level 2 uses palettes
  (Duplicate, Move to Folder), effects, child layers and sets, sub-cues,
  hits and Make VC Page. *Recommended: Student covers course Levels 0–2
  completely.*
- **Q-L2, what "Teaching forces Student" means.** *Recommended: a Teaching
  show opens at its saved level, which Teaching and Make Student Handout set
  to Student by default. Level 3 handouts are saved at Standard, and going
  up needs the PIN.*
- **Q-L3, how long a PIN unlock lasts.** *Recommended: until the show
  closes. It is never saved, so the student copy reopens at its level.*
- **Q-L4, what Advanced adds.** *Recommended: Standard stays exactly
  today's surface, except that the script editor moves up. Advanced adds new
  expert controls (engine timing, universe threads, raw values beside
  names, diagnostics).*
- **Q-L5, levels in Classic QLC+.** *Recommended: levels don't apply in
  Classic. A show that becomes Student there switches to the qlcconsole
  view, with Back to Classic QLC+ offered.*
