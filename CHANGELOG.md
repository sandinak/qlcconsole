# Changelog

All notable user-facing changes to qlcconsole are documented here. Format
loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

This is the fork's own changelog — for the detailed development log (with
per-feature build notes and deferred follow-ups), see [DONE.md](DONE.md) and
active work in [TODO.md](TODO.md).

Versions here are independent of the QLC+ base version embedded in the
in-app build string (see [RELEASE.md](RELEASE.md) for how the two relate).

## [0.2.2] - Unreleased

The 0.2.2 release candidate: the Build tab and its Workbench, the
Classic QLC+ / qlcconsole / Everything views, Upgrade This Show, plot
symbols and output rules on fixtures, and Eos shows with their special
circuits and subs.

### Added
- **Build tab and the Workbench** (WORKFLOW_DESIGN.md W1–W4). The
  Programming tab is **Build**: **Palettes | Effects | Scenes** picks what
  the navigator lists, with Groups & fixtures under each, and the
  **Workbench** on the right loads a scene, collection, chaser or any other
  function as layers. On a collection: **+ Scene**, **+ EFX**, **+ RGB
  Matrix**, remove (⌫), ▲ ▼, drag scenes in, **▣ Open** / **↩**, **also in
  N** and **Own copy** for shared members. The editor pane hosts the Look
  Editor, the channel console, timing and the stock editors. Design mode
  previews what is loaded. Every edit is one step on the history the Show
  Tree shares. No file format change; old QLC+ shows open as they are.
- **Build ▸ Scenes outline and Classic.** Folders → scenes, collections
  and chasers, each scene opening into its targets and looks; ⊞ / ⊟,
  badges with details on hover, drag rows between scenes (⌥ copies), F2
  renames. **Scenes ▸ Classic** lists every function by type as the
  Function Manager did; a show without a Show Tree opens on it.
- **Chasers in the Workbench.** A step walker (◀ ▶, the step's fades,
  hold, cue number and note in place), the step table, and the step's
  function as layers. **Chaser from selection**; scenes dropped on a
  chaser become steps; **Make this step's own copy**.
- **Effect designer and Build ▸ Effects.** The effect browser at full
  height beside the effect's settings and its own colour and dimmer,
  previewing live on the target heads. Keep it as a one-off, a new
  palette, an update or a preset. Hosted by **Design effect…** on a Show
  Tree layer and by Build ▸ Effects (**+ New ▾** and a right-click menu),
  effects' one home: **Window ▸ Effects** brings it forward (there is no
  separate Effects window). **Make a layer** turns the bench's groups and
  effect into a new layer of the cue or scene it came from, the selected
  cue, a picked scene or a new one.
- **Show Tree ↔ Workbench (W3).** A cue, look or layer opens in the
  Workbench docked in the Show Tree (double-click, **✎ Workbench**, **Open
  in Workbench**, `wb`): its own layers a row per attribute, tracked-in
  values grey and read-only, Solo, **↩ Show Tree**.
- **Workspace views.** **View ▸ Workspace ▸ Classic QLC+ / qlcconsole /
  Everything**, saved per show (an optional `View=` attribute older
  builds ignore). Classic shows QLC+'s tabs in its order (Connections reads
  *Inputs/Outputs*); qlcconsole shows Connections, Fixtures, Lighting
  Studio, Build, Show Tree, Shows, Virtual Console, Simple Desk and Run
  (Simple Desk, a QLC+ user's channel check, joined after the Virtual
  Console); Everything all ten. A show with no Show Tree not saved by qlcconsole opens in Classic.
  A Window-menu jump to a hidden tab switches to Everything with a *Back
  to …* line.
- **Upgrade This Show…** (File menu and the Show Tree's old-show line;
  W5): every chaser (cue list with its same-output check, effect kept as a
  function, or left alone) and collection, then Capture into palettes for
  raw scenes; one undo step. **Import in place** links the list to its
  chaser, files the originals under `Imported/…` and moves VC widgets,
  timelines and show control to the list; **Restore original…** undoes
  it. Older builds ignore the new XML.
- **Import chaser… shows a sheet**: plays-the-same check, a row per cue,
  what plays as it is and why, and an offer (ticked) to move what fires
  the chaser to the new list. What consecutive steps repeat is set once
  and tracks. **Send to Show Tree…** in Build does the same for one
  chaser. A ▣ layer opens its function, with where else it plays.
- **Show Tree: cue order is number order.** A typed number (Number field,
  or F2 on the row as "number name") moves the cue to its place; a taken
  number, a sub-cue's number or one that would clash with the cue's own
  sub-cues is refused. Dragging one or several cues numbers each dropped
  cue for its new place by + Cue's rule (between 2 and 3 → 2.5) and
  renumbers nothing else; Move up / Move down do the same. **Renumber this
  level…** numbers a level from a start in steps, point cues and sub-cues
  following, and asks first while OSC/MSC is listening. One undo step
  each; numbers save as typed; nothing is re-sorted on load.
- **Skip cues.** Untick a cue, **⌘/**, **Skip cue** or `skip 0.0.3 [thru
  0.0.5]`: GO, auto-follow and Back pass over it and it tracks as if it
  weren't there; a jump still plays it. Saved as `Skip="1"`.
- **Move in black per cue list and per cue.** A list's **Move in black**
  On / Off / as the show, a cue's **Don't move in black into this cue**,
  ⊘ MIB / ▸ MIB badges, `mib on|off|show [list]`, `nomib` / `allowmib`.
  The show's switch is also the Show Tree's **MIB** button (lead time in
  its menu, *MIB · n* while pre-positioning), Build's **Move in black ▾**
  (*Move in black · Auto*), **Control ▸ Auto Move in Black** and Settings
  ▸ General ▸ Show Tree.
- **Cue colours.** A text colour per cue list and cue (**Colour ▸**, the
  page's **Text colour**, `textcolour`), shown in the tree, the Run tab,
  the VC Cue Runner and Cue List and the Workbench header, kept readable
  on every theme.
- **⇧⌘N, New here** in the Show Tree, Build and the Fixtures tab; **Edit ▸
  New …** names what it makes.
- **Notes open up while typed** (cue notes, chaser step notes): wrap and
  grow to six lines; Shift+Return for a new line.
- **Group tools.** *Select All Similar*, *Move to Folder ▸* (folders in
  use, *New Folder…*) and *Create Group(s) from Selection…* in the
  selection's folder, in the Fixtures tab, Build's Groups & fixtures and
  the Show Tree's Library (with *Make set…*). **Return** / **F2** renames a
  group in the Fixtures tree.
- **Plot symbols.** Per fixture: Auto, or sixteen symbols in five sections
  (Moving; Fixed conventional; Fixed LED; Practicals: Lantern and Staff;
  Scenery: Curtain), from Fixture Properties or the Fixtures tab's **Plot Symbol ▸** (also in
  Operate: it changes only the picture). The 2D plot and the studio's
  Top/Front/Side views draw the symbol; a lantern glows all round, a staff
  is a pole wound with LED strip. Saved as `<StageLook>` only when not
  Auto. The plot's **Show** menu adds **Show Auto fixtures as their
  closest plot symbol**, saved with the show (off by default).
- **Curtain plot symbol** (Q51). A **Scenery** section in the plot-symbol
  picker (Fixture Properties, Plot Symbol ▸) with **Curtain**, saved as
  `<StageLook>Curtain</StageLook>`. A fixture marked Curtain — a relay on
  a dimmer channel that runs a traveler — is a curtain fixture: listed as
  a drape's driver, drawn on the plot as the drape (its fixture item
  hidden, its place kept), kept out of Inventory, shown with the curtain
  icon. With no *Open* / *Fly* channel its master intensity (else its
  first channel) opens it, 0 closed, 255 open. Back to Auto restores it;
  ⌘Z undoes it.
- **One icon per fixture, everywhere, and a curtain icon.** Every fixture
  and group list uses the same icon (its plot symbol, a curtain's drape,
  its kind), with the output-rules badge on top. Rows say what they are:
  a Dimmer look's pie, a Shutter look's state, a Strobe look's rate, a
  target's bullseye or line, a fixture's kind.
- **Output rules** for relays, house and work lights: **Ignore
  Blackout** (also through Blind), **Ignore the Grand Master**, **Switch
  (relay)** (on at 50 % and over, never faded, the Grand Master can't
  switch it off) and **On when not in use** (full whenever nothing sets
  it, from the switch to Operate or when the show opens: Settings ▸
  Advanced ▸ Output). Right-click **Output Rules…** in the Fixtures tab,
  Build's Groups & fixtures and the Show Tree's Library; one rig undo
  step; an amber badge; saved as an optional `OutputRules` element.
- **Eos import: special circuits and subs.** A **Special circuits** page
  reads relay, house, work and backstage / hall channels from their Eos
  labels (changeable per row or selection; `$$Options X` marks GM
  exempt), gives them output rules and groups under **Eos** (*Power
  relays*, *House & Work*, *Backstage*). Each sub becomes a Playback fader
  in a Virtual Console frame **Eos subs**.
- **Edit Fixture Definition… for several fixtures** (or a group) that
  share a definition; greyed with a reason when they don't.
- **ETC ColorSource Spot jr and ColorSource CYC** definitions, and the
  ColorSource Spot's Direct, 1-Channel and RGB modes (5-Channel keeps its
  name); Eos `ColorSource_*_RGB` personalities now match exactly.
- **Palette names follow their value** in the Look Editor (Red → Dark
  Red, Intensity 45% → 60%), only the part that names the value; the edit
  and the rename are one undo step.
- **Twelve more Matrix effects** (Fire, Starfield, Spiral, Lissajous,
  Kaleidoscope, Bouncing Balls, Fireworks, Scanner, Dissolve, Gradient
  Sweep, Box VU, Image with GIFs kept in the show) and an **Each box,
  staggered** scope for every scoped Matrix effect.
- **Cue Runner VC widget**: an External Input tab (an input and a key per
  action, with feedback) and an info-first layout with handles for the
  button height and the GO / Sub GO split.
- **Definition updates** (Fixtures ▸ Definition Updates…) and **File ▸
  Show Definitions…**: older definitions in the user folder or inside a
  show, with a diff, backups and undo.
- **LED Layout…** for a multi-head fixture, saved to the show or as the
  model's default.
- **Controller test** (Focus mode's setup; Settings ▸ Advanced ▸ Game
  controllers), and Focus mode saying when the controller is paused and
  why.

### Changed

- **Level… and Colour… are docked** in the Show Tree (board Q61): they open
  in a strip above the command line, inside the window, instead of a
  separate window that could fall behind the main one. Apply / Return sets
  the value, Cancel / Esc leaves it.
- **Help pictures never show this machine's network**: help-shots.sh loads
  only the Loopback plugin (new QLC_IOPLUGIN_DIR override).

- **"Classic" means stock QLC+ only.** Build ▸ Scenes has no Classic
  arrangement any more (the Folders | Classic switch is gone; Scenes lists
  your scene folders). **Window ▸ Functions** opens the stock Functions tab,
  switching to Everything (with a way back) from the qlcconsole view; a show
  saved with Functions in front no longer switches view on open. Build ▸
  Effects' "Classic effects" heading reads "QLC+ effects (EFX, RGB Matrix)".
- **One word per idea** (Q70; labels only — saved shows, settings and
  VC buttons are unchanged). *Stage Look* is now **Plot Symbol**
  (Fixtures right-click, Fixture Properties, the plot's **Show ▸ Show
  Auto fixtures as their closest plot symbol**); the Lighting Studio's
  *Layers* panel is **Plot layers**; *Perform (Operate)* is **Operate**
  everywhere (Settings ▸ Show Tree); *Hold Last Look* / *Clear Held Last
  Look* are **Hold at End** / **Clear Hold at End** (Control menu, a
  Shows track's right-click, the VC button action). Help, the course
  and the help pictures use the new names.
- **The Functions window lives in Build ▸ Scenes ▸ Classic** in the
  qlcconsole view: **Window ▸ Functions** opens it, and new
  functions of every type, the Function Wizard and the startup function
  are on **+ Scene ▾**. Classic QLC+ and Everything keep Functions as a tab.
- **File ▸ Import is a submenu** of source types — *From a show file*:
  QLC+ / qlcconsole Workspace, ETC Eos Show; *Fixtures & patch*: QLC+
  Fixtures List — each with its own file filters, then **Any Supported
  File…** (the old Import).
- **Esc backs out** of in-place editors one level: a palette over Build's
  rows (**↩ Back**), a group's layout, the effect bench (asking first if
  the effect isn't kept), a ▣ opened layer, the docked Workbench, the Show
  Tree's Look Editor. Edits stay; renames keep Esc; plain Esc is no
  playback action (Stop all is ⌘⇧Esc).
- **The Show Tree shows tracking as badges** (● sets, ○ releases, ⤳
  tracks in) with the details on hover, in a resizable column.
- **The Show Tree's command line folds its syntax away.** The line stays;
  the three lines of syntax under it show on the **?** button beside it,
  **?** typed first, or **F1** in the line, and the choice is remembered.
  An empty show's page just says how to start.
- **The Run tab with no main cue list** (a QLC+ 4 show) says *This show
  has no cue list yet. Upgrade turns its chasers into one.* with an
  **Upgrade This Show…** button, and GO and the other buttons are grey
  instead of a live-looking green.
- **The Build tab wears a hard-hat icon.**
- **Fixtures tab:** Plot Symbol ▸ is offered in Operate; the toolbar's
  Properties opens Fixture Properties for several fixtures.
- **A dimmer's Auto plot symbol is Ellipsoidal / profile spot** (a lantern
  is a practical now).
- **The boot window shows the version**, build and commit.
- **The Lighting Studio plot draws a pixel fixture's heads where its LED
  layout puts them.**
- **Tab shortcuts follow the view.** ⇧⌘1, ⇧⌘2… number the tabs the
  current workspace view shows, in its order (⇧⌘0 the tenth), and
  renumber when the view changes; the Window menu lists them so, then the
  hidden tabs with no shortcut. In Classic QLC+, Fixtures is ⇧⌘1; in
  qlcconsole the Show Tree is ⇧⌘5 and Run ⇧⌘9 (they were ⇧⌘6 and ⇧⌘0).
- **Settings ▸ Appearance.** Theme, toolbar style, the four footer chips
  (engine load, power estimate, grand master, speed masters) and the speed
  masters' beat moved out of the View menu into a new Appearance tab in
  Settings, applied on OK. The same settings, so earlier choices carry
  over. View now holds Workspace, Address Tool, Rig Overview and Focus
  Mode.
- **One switch each.** The workspace view is set only in View ▸ Workspace
  (Settings ▸ General no longer has a copy), and the toolbar style only in
  Settings ▸ Appearance.
- **No outputs: preview only.** With nothing patched to any output, the
  footer's readiness chip reads *No outputs: preview only* in grey (with
  *· N to check* when something else is listed; a click opens Connections
  or the list) instead of a red *Not ready*. Red is kept for real problems
  on a rig with outputs. The course no longer tells students to ignore red.
- **Auto-open shutters is off for a QLC+ 4.x show.** A show saved
  without the setting loads with it off unless qlcconsole saved it, so an
  old show sends the same DMX here as in QLC+. New shows keep it on, and a
  show now always saves the setting, on or off.
- **Settings, tooltip and menu text** name the paths the help uses:
  Settings ▸ Advanced ▸ Output in the Output Rules dialog, Build → Move in
  black in the move-in-black tooltips, *Fade 10 / 30 seconds and stop*.

### Fixed
- **A fade or hold typed on a Show Tree cue is kept when you click away.**
  It was applied only on Return: clicking another cue lost it. Tab, a click
  on another cue or anywhere else, and the page moving on now apply it to
  the cue it was typed for; Esc in the field puts it back. A chaser step's
  times in the Workbench go to the step they were typed for when you walk
  to another.
- **A Closed shutter look closes** (it sent Open on spots with an Open
  capability, e.g. the AT-Spots); *Strobe* strobes, and auto-open defers to
  both.
- **Esc in the Look Editor's colour picker** no longer hides the picker
  for good.
- **A playing scene follows its group's members live**, as Build's help
  says. Its group's fixtures were resolved when it started, so a fixture
  put in the group stayed dark (and one taken out kept the look) until the
  scene restarted. A running scene now picks the change up on the next
  frame: a fixture that joins fades in over the look's fade-in, one that
  leaves is let go of over the scene's fade-out (or at once with none), as
  the scene stopping would, and the rest play on untouched.
- **Lighting Studio: a pasted truss brings its fixtures.** ⌘C / ⌘V (and
  Duplicate selected, Paste features here) copied the truss alone, though the
  shortcuts help said structures copy with their fixtures. Each fixture on a
  copied truss now comes too: a new fixture with the same definition, mode
  and settings, a name of its own ("LX1-1 #2"), hung at the same place on
  the copy. They arrive **unpatched** -- which addresses copies take is a
  patching choice (Patch Others Like This) -- and the plot's status line
  says so. One ⌘Z takes the truss and its fixtures away.
- **Pasting a truss with fixtures asks** (Q55): *Paste N fixtures with
  the truss* → **Patch at next free addresses** (after the originals in
  their universe, the Patch dialog's first-fit, spilling into the next
  universe when it is full), **Leave unpatched**, or **Cancel** (nothing
  pasted). Still one ⌘Z for the whole paste; the status line says where
  they went.
- **Hiding the Workbench stops only what it started.** It started and
  stopped what is loaded (the Design preview, the Operate run) with the
  Master parent, and a Master stop clears every source -- so a function a
  VC button, the Run tab or a cue was already running stopped when the
  Workbench hid or let go of it. It now plays under its own parent and
  takes only that away.
- **Build: deleting and duplicating a function can be undone.** Delete,
  Duplicate, ⌘C ⌘V and Build ▸ Effects' Duplicate / Delete of an EFX or RGB
  Matrix went straight to the show with no step on the shared history, so ⌘Z
  couldn't take them back. Each is now one `PaletteOps::functionStep`: Undo
  brings a deleted function back with its id, in its place in each
  collection, chaser and show track (they ride in the step whole), and hands
  it back to the VC buttons, cue lists and main cue list that let go of it;
  Redo deletes it again. A duplicate's Undo takes the copy away, its Redo
  brings it back with the same id.
- **Park, Highlight and a held last look hold their dimmers.** All three
  wrote the universes straight from the MasterTimer tick, before the
  universe thread's `processFaders()` -- which zeroes every intensity
  channel and then lays the running faders on top. So a parked or
  highlighted fixture went dark or showed the running cue, and a timeline
  track's *Hold at end* and the look a show parks on at its end both went
  dark (pan/tilt held; B1, in the fork since each feature came in). They now
  go out through faders of their own (`OutputLayer`), at new
  `Universe::FaderPriority` levels: **Park** on top, Override and past the
  Grand Master (`Universe::writePastGM`, a new `FadeChannel::PastGM`), still
  taken out by Blackout; **Highlight** just under it, Override, with the
  Grand Master and Blackout applying; the **held last look** under
  everything (`Hold`), merged like any running function -- intensity HTP, a
  function takes an LTP channel, and a starting cue still takes its fixtures
  from the hold. `ParkEffect`, `HighlightEffect` and `LastLookEffect` are no
  longer DMX sources. The precedence is written up on
  `Universe::FaderPriority`. **Flash** (`ProgrammerFlasher`) and **Focus
  mode** (`FocusSession`) had the same bug -- a flash or a focused head's
  light lost its dimmer to the frame and to a running cue -- and go out the
  same way, at `Flash` and `Focus` (Override, between Highlight and Park;
  GM and Blackout apply). `FocusSession` is no longer a DMX source;
  `ProgrammerFlasher` still is, only to time its flashes.
- **A Grand Master move no longer drops an On-when-not-in-use relay.**
  Moving the Grand Master recomputes the output at once
  (`Universe::slotGMValueChanged()` -> `updatePostGMValue()`), from the
  stored value -- 0 for an On-when-not-in-use channel nothing sets -- and
  the floor only came back with the next frame's `applyOnAtOpen()`, so a
  relay could drop for a frame. `updatePostGMValue()` now applies the floor
  itself (`onAtOpenValue()`), so every path that works out the output has
  it.
- **Shows timeline: Undo keeps each track's settings.** The timeline's undo
  snapshot (`ShowManager::captureSnapshot`) held a track's name, mute,
  colour and items only, and Undo rebuilds every track from it -- so any
  Undo reset Hold at end, Solo / Solo-safe, priority, intensity and the
  track lock on every track, unlocked locked items, and dropped a marker's
  cue-list link. They are now in the snapshot and put back.
- **Build: the Beat button shows the tempo a Tap set.** It kept the old
  tempo: `slotTapBeat()` set the BPM under a signal blocker the button's
  text follows. Only `slotBpmChanged()` (which would restart the tapped
  beat) is held off now.
- **A Closed shutter look closes.** A Shutter palette resolved to the
  fixture's Open capability whenever it had one (and to 0, open, on a
  strobe-preset channel), whatever the look's value -- so *Closed* (and
  *Strobe*) sent Open on the AT-Spots, and auto-open had nothing to defer
  to. `QLCPalette::shutterIntent()` now reads the look's value on the
  first target whose shutter has a tagged (then a named) Open, Closed or
  Strobe capability there, and `shutterValueFor()` sends each fixture the
  value as set where it means the same, else its own closed / open / a
  strobe at the same rate. No value or 0 (every look made before, the
  course shows' "Open" among them) is still Open; a strobe-preset channel
  never decides it and has no closed, so it stays open. The Shutter icon
  uses the same code, so a Closed look shows closed; the Look Editor shows
  an open look as the type's Open, keeps a Closed picked at 0 as 1, and no
  longer offers a strobe-only channel's single "Strobe" as a Shutter
  source.

- **Move in black no longer carries over to the next show**: a new show,
  or one saved without it, starts off with the default lead time.
- **An imported chaser of raw scenes plays as the chaser did** (Sample.qxw's
  par chasers ran at about half level); its cue-list number is no longer
  copied onto the new list's chaser.
- **The look-ahead's static output** scales a dimmerless fixture's colour
  by the look's dimmer.
- **Back, then GO, keeps the colours the cues set**: Back and a jump put
  back what the cues before set last, so every cue looks the same however
  you get to it.
- **No "declares no light output" warning** for a curtain, a fan, a hazer
  or a smoke machine.
- **Focus mode draws where the light should point** while you nudge, not
  the nudged DMX.
- **Game controllers**: kept working with qlcconsole behind other windows
  (App Nap); found again when switched back on or after sleep; HID lines
  numbered consistently; every Xbox model read in the right layout; the HID
  plugin no longer opens every HID device or writes a debug file.
- **Studio**: target items are dropped before a new or opened show deletes
  the targets.
- The show's LED layouts are guarded against a running matrix effect while
  they are edited.
- **A timeline Override track wins its dimmers too**, as the help says: a
  lower level on it pulls a light below a Normal track or a cue at full
  (intensity used to mix highest-wins). Park, Focus, a flash and a
  highlight still beat it, the Grand Master and Blackout still apply, and
  when its item ends what plays underneath comes back.
- **A track's intensity set before Start plays**: the track's fader and the
  show's attribute for it are one value, so the level set last (and saved
  with the show) is the one that plays. A track saved at 50% used to start
  at full.

### Known issues

- Under very heavy machine load a fade can finish late (seen in the course
  test's 3 s fade taking ~8 s with dozens of builds running); the universe
  threads run at normal priority.

## [0.2.1] - 2026-10-21

A point release of the Show Tree beta: one top bar, auto-open shutters,
a controller map in Focus mode, sub-cues from QLab, and fixes to Import
chaser, confetti on two-arm beams and the update installer.

### Added
- **Focus mode controller map and device choice.** The Focus mode panel
  draws the controller (an Xbox-style layout, vector, following the theme)
  with each control labelled by its Focus mode action, taken from the
  profile's gamepad roles rather than fixed text: a profile with other names
  (Cross, R1, Options…) shows them, a control it lacks is greyed, unused
  ones (triggers, stick clicks, Guide) are grey. Buttons light while held
  and the stick caps follow the sticks; a click on a control does it.
  **Use: Controller | Keyboard** in the setup dialog and the panel picks
  the device that drives the nudges and the map shown (the keys, labelled
  and lit as pressed, in Keyboard). Default: the controller when one Focus
  mode can use is connected, else the keyboard; the user's choice is kept
  per machine (`focusmode/input`). On the keyboard every controller input
  is swallowed and ignored -- sticks centred, nothing applied -- so a
  resting pad cannot drift a head; keyboard shortcuts work in both. A
  controller disconnecting mid-session switches to the keyboard with a
  note, and returning offers **Use the controller**.
  `ProgrammerController::focusController()` / `focusControllerChanged()` /
  `focusControllerInput()`, `FocusSession::setInputDevice()`.
- **Auto-open shutters** (Settings → Advanced, saved with the show, on by
  default). Like a console's auto open: a head given a level (its dimmer,
  or a colour when it has no dimmer) by any look, layer, cue, slider or
  programmer edit, whose shutter nothing sets, has its shutter opened on
  output, at the definition's open value (an open default, else the
  ShutterOpen / "Shutter open" capability; a strobe preset channel at 0;
  never an iris). An explicit shutter value -- Closed, Strobe, anything --
  always wins, including one a slider or programmer edit left behind; a
  head with no level, or a dimmer only resting at its default, is left
  alone. Each universe applies it after its faders on its own thread, on
  the post-GM values only, so DMX, the 2D view and the Rig Overview agree
  and turning it off restores exactly what the show sets. Shows that never
  turned it off save as before (`<ShutterAutoOpen Enabled="0"/>` only
  when off). The AT-Spots (Focus Spot Three Z, shutter resting at Off) now
  light from White, Full alone. The Programming tab stops warning "no look
  for: shutter" while it is on.
- **Sub-cues from QLab.** OSC `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play
  a sub-cue (its cue first); `/subgo` and `/subback` say why when they
  can't; LOAD 4.2 puts a sub-cue on standby and **Next:** in the footer
  and the cue list shows it ("Next: 4.2 — Wash"); STOP 4.2 stops the list
  only while 4.2 is the one playing. ⌥⌘↩ / ⌥⌘⇧↩ go through show control, so their result is
  reported with the rest.
- **Two new look warnings.** The Looks list marks a Colour look that is all
  zero (**Black**), and a position look on fixtures whose definition marks
  every Pan or Tilt channel Fine (**Can't move**: the head stays at 0).

### Changed
- **Settings has General and Advanced tabs.** Advanced holds the show's
  output behaviour (auto-open shutters) and two settings for experienced
  users moved off the one page: the effect script editor, and the Show
  control (QLab) pointer to Connections. Everything else stays on General.
- **One top bar instead of two.** The show controls and the open tab's
  own tools share one row: the tab's tools on the left, **Stop all**,
  **Blackout**, **Blind** and a **Design | Operate** switch (the mode you
  are in lit) on the right, as small icon-and-word buttons. About 57 px
  more for content on every tab (94 px of bars above it before, 37 px
  now). Blackout lights red and Blind blue while on, readable in all six
  themes. A narrow window folds the tab's tools into **»**; the show
  controls never fold. **Settings → Show Controls** can put them in the
  status bar instead, at its right end, leaving the top to the tab's own
  tools; it switches at once, no restart. Same actions and shortcuts
  either way; a tab in a window of its own keeps its tools at its top;
  kiosk mode still has neither. The Show Tree header and the Programming
  tab's row are toolbars now (they fold into » rather than being cut off
  or scrolled), and the Lighting Studio's toolbar uses the same 20 px
  icons as the others.

### Fixed
- **The Show Tree's drop hint stays in sight in the shared top bar.** The
  note that says where a drag will land (and, after a change, what it
  changed) sat at the end of the Show Tree's toolbar, so once the toolbar
  shared the top bar with the show controls it folded into **»** at any
  window narrower than about 1900 px — every laptop. It now stands beside
  the toolbar, just left of **Stop all**, and keeps up to 420 px; the
  buttons fold first.
- **Import chaser keeps an include brighter than the cue's own level.** A
  cue that includes another at full and sets the same fixtures lower itself
  (DanceHysteria's Full Show 0.0.5: 0.0.4's StepTops at full, its own at
  45%) played the 45% after import; the chaser had run both and the full
  level won. The importer now makes such a cue **mix** (Overlap: Mix, *≈
  mix* in the tree) — only where the tree's rule would change its output,
  checked channel by channel — and in a cue that mixes an include plays
  beside the cue's own layers on the same group instead of giving way to
  them, so it plays as the chaser did. The report says which cues and how
  many channels; Convert to tracking keeps them mixing. On DanceHysteria-0.6
  the import dry run is 0 channels different in every cue (Full Show 0.0.5
  marked; 0.0.6 and 0.0.7 already mixed and now match too). The dry run
  (`showtree_test importARealShow`) reads the show's embedded fixture
  definitions, as opening it does, with the user folder kept out, so it
  compares the same on a machine without the author's fixture files, starts
  each playback from the idle rig with channel defaults put back, and fails
  unless 0 channels differ.
- **A dimmer effect on a two-arm beam went black.** One master dimmer
  serving several heads got one scaled write per head, multiplied together:
  confetti's lit arm times its dark one was 0. The brightest head now sets
  it.
- **GO by number on a paused list resumed it and stayed put.** It now plays
  that cue.
- **An OSC float cue number matched nothing.** 2.2 sent as a float read as
  2.200000048; floats now read to the digits they hold.
- **The update installer copes with a stale disk image.** Something
  scanning a fresh download can leave the DMG attached with nothing
  mounted, and every later attach then failed ("the disk image would not
  open"). The macOS update helper now detaches such leftovers first,
  retries a transient attach failure (not a bad image), waits until the
  old mount is really gone, and cleans up if it is stopped part-way; the
  app asks it to stop before killing it, so that cleanup runs. Its error
  says what hdiutil said.
- **Two flaky tests steadied** (Updates_Test's disk images, built now
  without attaching anything; ShowControlUi_Test's OSC port, now checked
  and retried), and the course and office show tests build with GCC's
  -Werror.

### Tooling
- **`make dmg`, `make beta`, `make release` and `make attach`** build the
  signed universal DMG and publish to GitHub Releases through the REST API
  only (`platforms/macos/publish.sh`; RELEASE.md). `make test` builds the
  dev app.
- **Show Tree course** (docs/course): Levels 0 and 1, lessons 0.1-1.5 with
  the Level 1 check, in the course voice, the teaching show-choir rig and
  its shows, a browser practice version of Lesson 1.1, and a test that
  drives every lesson by the mouse.
- **Office show**: a generator for `Office.qlcc` (the office rig test
  show) and its test, run by the gate.

## [0.2.0] - 2026-10-21

The Show Tree release. A show is written as one tree — productions → cue
lists → cues → layers → looks — with tracking, sub-cues and a cue runner
for playback; line targets, line of sight and Focus mode for aiming;
curtains a fixture opens and flies; colour rings on movers; Matrix effects
for pixel boxes; backup WLED lanterns; and output held off the rig
network. Everything in 0.1.0 is in it too.

### Added
- **Software update check and install** (GitHub Releases). A background
  check after start-up and at most once a day, plus **Help → Check for
  Updates…**. It sends one unauthenticated GET of the releases list, with
  ETag / If-None-Match, waits out GitHub's rate-limit reset, and gives up
  quietly offline. Channels are Stable or Pre-release, defaulting to the
  build's own. Versions are compared SemVer-style, with `+qlcconsole.N`
  breaking ties. **Skip This Version** is available. A footer chip shows
  "Update available: X", and a non-modal dialog shows the release notes as
  Markdown. Downloads are verified against the release's `SHA256SUMS`, and
  anything that does not match is refused. On macOS the new app is staged
  beside the old one, after its code signature, Gatekeeper, Team ID and
  bundle id are checked. A detached helper swaps it in at quit, keeps the
  old app until the new one has started, and reopens the show on **Restart
  and Install**. A read-only /Applications falls back to opening the DMG.
  Windows runs the NSIS installer silently after quit. Linux and the Pi are
  notified only. Nothing installs in Perform mode, in kiosk mode, or while a
  function runs. "Install updates automatically" downloads in the
  background and installs at the next quit. `QLCCONSOLE_NO_UPDATES=1` or
  the system-wide `updates/disabled` turns it all off. `release.sh` now
  publishes `SHA256SUMS`, and `./release.sh attach` adds CI's files and
  their checksums.
- **Only transmit on the rig network.** A show can name its rig networks
  (Settings → Network, e.g. `172.18.0.0/20`, saved with the show as
  `<RigNetwork>` in the IO map). With **Only transmit on rig networks** on
  (this computer's setting, default on), Art-Net, E1.31/sACN, OSC and WLED
  output from any interface outside them is **held** — patch kept and saved
  unchanged, line not opened, nothing sent (no default-route unicast, no
  broadcast, no ArtPoll/ArtPollReply/discovery) — and resumes by itself
  within ~5 s on the rig network. Loopback and non-network outputs are not
  affected. A red footer chip ("Output held — not on the rig network
  (172.18.0.0/20). Connected: en0 10.4.12.7") opens the setting;
  **Transmit anyway, this session…** asks first and is never saved. A show
  with none set transmits as before, with an amber footer note and a
  suggestion taken from its patches' saved networks (offered, not written).
- **Focus mode** for aligning the rig before a show: step through the moving
  heads with the controller's bumpers, each aimed at its reference (a
  scene's aim, a stage target, a line target's own point) and lit, nudge its
  pan/tilt calibration offsets with the sticks (the follow spot's stick
  path) or D-pad, A to save (one undo step), B revert, X zero, Y a second
  reference, Menu to leave (asks about unsaved heads). Keyboard fallback for
  all of it. Lighting Studio / Rig Overview **Focus**, View → Focus Mode…
  (⌘⌥F), or View + Menu. Controller profiles gain optional gamepad
  `role=` attributes; the Xbox profile has them and the D-pad.
- **Show Tree** (new tab, ⇧⌘6): productions → cue lists → cues → layers →
  looks, with tracking (Block / Cue-only / Release), includes, one
  precedence rule up and down the chain, a plan view for line targets, and
  preview that matches GO. **Import chaser…** turns an existing chaser into
  a tree (0 channels differ on playback); **Convert to tracking** /
  **Detach to plain chaser** go the other way. A **Looks** section for
  busking (with **Make VC page**), the **Library** with palettes, Recent and
  Pinned, one undo history shared with the Programming tab, and a typed
  command line. Deleting a palette takes it off every look that uses it
  (one Undo puts it all back).
- **Productions, sub-cues and the cue runner.** Several productions per
  show, each with its own main list and timecode; sub-cues 4.1, 4.2… with
  Sub GO or Auto-follow; the **Run** tab (⇧⌘0, Space = GO) and the **Cue
  Runner** VC widget (and **Swap Widget** from a Cue List); OSC `/subgo`,
  `/subback`.
- **Line targets.** A stage target can be a line (or curve): an Aim look
  spreads its heads along it, crosses them, groups them in pairs or halves,
  in hanging or look order.
- **Colour rings on movers.** A mover's LED ring is a head type of its own
  (group by head type makes Movers and Ring groups); ring chases by name;
  drawn live in the 2D plot and Rig Overview; **Change Definition…** moves
  fixtures to a drop-in definition.
- **Create Group(s) from Selection** — one group, a group per head type, or
  a group per head type for each fixture, previewed before anything is made.
- **Palette folders hold any type** — folders first, or Group by type.
- **Matrix effects for pixel boxes.** Fourteen effects in a new Matrix tab
  (bars, chevrons, ripple, rain, checker, sparkle, radar, iris, equaliser,
  snakes, wipes, colour noise, box chase, strobe), across a run of boxes or
  in each box, with white-pixel options (the XL-450's).
- **Curtains that move.** A main drape or traveler can be driven by a
  curtain fixture (bundled **Generic › Stage Curtain**: Open, Open fine,
  Fly): it opens and flies as the channel fades, on the plot and in the Rig
  Overview, and blocks beams where it is (see Line of sight).
- **Backup WLED lanterns.** A WLED universe can drive several devices: a
  backup lantern mirrors the primary without programming anything twice.
  Right-click the universe in Connections (or the Fixture Manager) →
  **WLED Devices / Backup Lanterns…** to add, remove and reorder devices
  (the top one is the primary), each with its own address and channel count
  if it needs one; one undo step. Every device is sent to on its own, so a
  dead or hanging one never holds up the others, and one that comes back is
  sent the whole look again. Connections says "1 of 2 answering — … not
  answering", the footer names the silent backup, and the rig view shows
  the first device that answers. Saved as `outputIP2`, `outputIP3`… beside
  `outputIP`; older versions ignore them and drive the primary alone.
- **Line of sight for follow spots and aiming.** The stage's solid features
  — drapes and travelers as they hang (closed blocks, the open gap doesn't,
  flown is out of the way), walls and the proscenium, platform and riser
  sides, towers, and trusses/pipes/stands at least 20 cm thick — are checked
  between each aimed head and its point; a head's own mount never counts.
  The Followspot effect's **Fade When Blocked** (ms; 0 = off) fades a
  blocked head out and back; an Aim look's **Dim lights that can't see the
  target** (off by default) does the same for any aimed look. Reach
  warnings name the blocker ("blocked by Main drape"), the Lighting Studio
  plot draws blocked aim lines red to the blocker, and **Show → Line of
  sight** sets which kinds block and whether the 3D views stop beams at
  them. Effect scripts read `sceneTarget.blocked` / `blockedBy` /
  `blockedKind` / `blockedAt` / `clearance` (also on `aimAt` slots). A
  line target checks each head's own point on the line; a curtain driven by
  a fixture blocks where that fixture has it now.

### Changed
- **A new production is current when nothing else has GO.** In the Show
  Tree, a production made while no production is current and the main cue
  list isn't some other chaser becomes the current one, so a first
  production plays from the Run tab without **Make current**. Adding a
  production never takes GO from one the operator chose.
- **Coloured light reads as its colour in the rig views.** Light landing on
  a grey or white surface (a cyc, the deck) now takes it over in the
  light's colour: a full blue wash on the cyc draws blue, not white with a
  lavender tint. White light looks as before, and coloured surfaces (LED
  strips, drapes) keep their own colour.

### Fixed
- **Beams stop at risers and the floor in the rig views.** A wash thrown
  across a riser went through it, lighting the air behind it, and its cone
  carried on below the stage floor (the cone's edge was followed to the
  surface the middle of the beam landed on, a drape far beyond). A beam now
  stops at a platform's side as well as its top, each edge of the cone ends
  at what it meets, and a side or front elevation cuts the beam round the
  risers it meets and shows the wash on their tops and faces as a band.
- **Calibration offsets no longer show in the rig views.** A head's pan/tilt
  offsets correct the physical fixture: the DMX keeps them, but the plot,
  its gauges, the Studio, the Rig Overview, beams and footprints now draw
  the intended aim. Fixture Properties' **Test orientation → Aim at target**
  uses the aim solver (it was 180° out and solved standing heads as hung).
- macOS Xbox controller (raw mode): axes now arrive in the order the Xbox
  profile names them (the right stick was reported as the left trigger),
  and the D-pad is reported.
- **Crash on quit with the Show Tree open.** Closing the app while its
  window was still up could crash: the Show Tree tab stopped its preview on
  a show that had already been freed.
- **The cue page's drop area is readable in every theme.** "Drag groups or
  fixtures here…" was nearly invisible on the dark themes (Blue especially).
- **Chaser teardown use-after-free.** Deleting a chaser while its runner was
  still running freed the steps the runner reads (an intermittent "double
  free" on Linux).

## [0.1.0] - Unreleased

First public release. qlcconsole is a fork of
[QLC+ 4.14.2](https://github.com/mcallegari/qlcplus), rebuilt around a
console-first programmer-mode workflow.

**Platforms:**
- **macOS** — one app for Apple silicon and Intel Macs (macOS 12 Monterey
  or later), signed and notarized, as a DMG.
- **Windows** — a 64-bit installer (Windows 10 or later). Not code-signed
  yet, so SmartScreen asks before it runs.
- **Linux** — an x86_64 AppImage.

**Soak-tested** (2026-10-16 → 17, the DanceHysteria show, three 8-hour
phases — the real show, a scaled-up show, rough handling with reloads —
with DMX output stripped): no crashes or hangs on Linux x86_64 (Debian 13),
Linux arm64 or macOS in any phase. On Windows 10 (a VM) the real-show phase
ran clean; the scaled phase was cut short by a Windows Update reboot, and
a shorter Windows re-soak is planned.

### Added
- **QLab and show control.** Chase steps carry cue numbers (a Cue column in
  the chaser editor, with Renumber); a chase can be the main cue list or
  have a list number; **MIDI Show Control** and **OSC** can GO, LOAD, STOP,
  PAUSE, RESUME, RESET and ALL OFF them. The footer's **Next:** shows what
  the main list's GO plays next; GO (⌘↩) and Back (⌘⇧↩) work from any
  window; the Virtual Console cue list shows cue numbers and what's next.
  **Skip** a step without deleting it — GO, Next, Back and auto-follow pass
  over it.
- **Connections → Show control & sync** — one page for what controls the
  show from outside and what it syncs to: OSC in (port, listening or not,
  the last command), MSC in (device number, **Listen on** all MIDI inputs
  or just one, the last command), and the MIDI timecode source with its
  sync health. The timecode source is saved with the show.
- **Sub-cues** — a chase used as a cue lists its own steps under it as
  4.1, 4.2…; set to **Sub GO** they wait for Sub GO (⌥⌘↩, Sub Back ⌥⌘⇧↩)
  while GO moves on to the next cue. Shown in the chaser editor, the
  Virtual Console cue list (with Sub GO / Sub Back buttons) and the footer
  ("Sub: 4.2 · Next: 5").
- **Output trouble is reported** — an Art-Net or E1.31 output that can't
  send, an Art-Net node that stops answering, or a node port reporting a
  DMX short shows in the footer (with **Open Connections**) and in red on
  the universe's row in Connections. It settles before it warns, so a
  brief hiccup doesn't flicker on and off.
- **Shows carry their own definitions** — a `.qlcc` saves the fixture
  definitions and effect scripts it uses. Opened on another computer, the
  show's own copies are used (**Install on This Computer** keeps the ones
  it lacks); where they differ from that computer's the footer says so and
  **Review…** shows the differences. **File → Collect for Transfer…** gathers a
  show and everything it needs into one folder.
- **One beat clock and speed masters.** Every effect and beat chase reads
  one clock (effects started apart are on the same beat); a Tap on the
  Programming toolbar. Up to sixteen **speed masters**, each its own BPM and
  tap, rate and pause, or following the main clock or another master by a
  ratio; a look, chase, step or collection runs on one, and the footer
  shows each master's beat. **GO on the beat** holds a GO to the next beat
  or bar.
- **Spread** — an effect's phase spread across its heads: by order,
  columns, rows or across the stage, forward, reversed, from the centre or
  the ends, mirrored or in blocks, or out from / round the look's aim; a
  preview in the look editor.
- **The stack checker** — the Looks list marks a look the one below always
  covers, an effect another covers, a dimmer only some heads get, and says
  why.
- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, as CSV or a printable PDF (Fixtures → Patch List…).
  **Labels…** prints a label per fixture (all, or the selected rows) on
  Avery 5160 sheets or a 62 mm roll: name, universe and address, mode,
  and a network fixture's name (its IP on mouseover in the list).
- **Units** — Settings → Units picks mm, cm, m, inches or feet (kg or lb
  to match); fixture sizes and weights show in it and take any unit typed
  with the number (`7.5"`, `6' 3"`, `26.5 lb`), and the Studio's grid
  follows it. Files still store millimetres and kilograms.
- **In-app pictures** — the guides show the main window, footer,
  Programming tab, effect browser, Patch dialog and more with numbered
  marks; click one for full size.
- **The Rig Overview on the graphics card** (Metal on a Mac, OpenGL
  elsewhere) with its fixtures worked out on every core: a fully lit
  208-fixture show from about a second a frame to 15 ms.
  A **GPU** box turns it off. Its own **Layers** menu shows and hides
  layers — the same switch as the Studio's eye.
- **Washes look like washes** — a pool fades past its beam angle for a wash
  and stays crisp for a profile; PC and Fresnel lenses are softest. Pools
  show on decks and steps.
- **Programming tab** — a dedicated top-level tab for building scenes/"looks":
  searchable/foldered palette and function trees, drag-and-drop palettes onto
  looks, groups onto dynamic targets, fixtures onto fixed targets. Inline look
  editor for color, dimmer gradient, XY pan-tilt, and gobo/shutter. Live
  DMX/2D preview while editing (Design mode).
- **Group scenes** — scenes that target a fixture group dynamically, so a
  look can be reused across different fixture sets.
- **Fixture Manager enhancements** — fixture-group folders, empty-group
  creation, and a drag-and-drop head-layout grid editor (place fixtures on
  cells, add a whole group as a block, move sub-groups as a unit).
- **Show timeline** — explicit show length with a Logic-style end handle and
  park-at-end behavior, so a show doesn't scroll off into nothing once MTC
  runs past its end.
- **MIDI timecode sync-health** — live rate/jitter/latency readout with a
  healthy/unstable verdict, to help diagnose timing issues against real
  hardware.
- **Effect scripting system** — a scripted effect lifecycle (one-shot bursts,
  sweeps, reveals) with audio- and MIDI-reactive effects; see
  [EFFECTSCRIPT_API.md](EFFECTSCRIPT_API.md) and
  [EFFECT_LIFECYCLE_DESIGN.md](EFFECT_LIFECYCLE_DESIGN.md).
- **Effect presets** — named, reusable configurations layered over effect
  scripts; see [EFFECTPRESET_DESIGN.md](EFFECTPRESET_DESIGN.md).
- **Lighting Studio** — a rigging tab for the physical rig: place and rig
  trusses, towers, platforms and steps, pipes, stands, hangers, a
  cyclorama and power sources; drag a fixture onto a structure to mount it,
  with select/move-together group behavior; a Layers panel for organizing
  the plot; Power/DMX/Network/Stage-only overlays. The plot is seen from
  Top, Front or Side, and each structure's own editor adds an **Angled** 3D
  view; **View → Rig Overview…** (⇧⌘R) opens the whole rig in 3D in a window
  of its own. See [the feature guide](docs/features/lighting-studio.md).
- **The rig view is lit by the show** — in Design mode the selected look
  (or live output) lights the 3D rig: beams per head that carry past their
  target and land as pools on the deck, platforms or cyc; brightness from
  the fixture's declared lumens or watts; a Haze setting (None / Light /
  Medium / Heavy) with a glow round lit LEDs; gobos, gobo rotation, prisms
  and your own gobo images per fixture; pixel bars lit section by section;
  truss socks and tower faces (with printed patterns) glowing with the
  bars inside; people targets lit in the colour that reaches them. A moving
  head's footprint shows everywhere its pan and tilt can reach.
- **Lanterns and hangers** — hangers are rigging, like a truss: clamped to
  a truss or tower with an arm and a drop to one hook, edited in the rig
  view and copied to every tower in one step. A lantern body glows all
  round, with its LEDs (a hanging cluster or tape) seen through clear or
  diffusing glass.
- **Fixtures tab** — a Power folder for modeling power sources/circuits and
  assigning fixtures to them, plus a per-universe DMX address usage grid.
- **Connections tab** — the I/O manager rebuilt around a strict tree
  hierarchy (host → protocol → interface → target → port → universe), with
  right-click menus scoped to exactly what applies at each level, IP-first
  patching, rerouting a line's universes to a different interface in one
  step, and "portable" patches: opening a workspace on a machine missing the
  network it was built for keeps the mapping intact (instead of losing it)
  without broadcasting to the wrong place. Ctrl+Z undoes the last
  patch/universe change made here.
- **Control-surface engine** — a device-agnostic engine for hardware
  control surfaces (role/page vocabulary, context-aware LED feedback), with
  an OpenDeck PMJ Black 1 overlay underway; APC40 mk2 and Xbox controller
  support planned on the same engine.
- **Consistent startup window** — app icon/name/version, a scrolling log of
  what's loading, and a progress bar cover the whole boot sequence (plugin/
  fixture/profile loading, opening a workspace passed on the command line,
  autosave recovery) on every platform, replacing the old indeterminate,
  macOS-only progress dialog. The main window now only ever appears once
  everything is actually ready.
- **Real-time DMX output timing** — on macOS the DMX timer thread asks the
  OS for real-time scheduling, fixing missed output ticks on a busy
  machine. On Windows it has a thread of its own with "Pro Audio"
  priority; on Linux it asks for real-time priority, which a stock install
  doesn't grant (it then runs at normal priority).
- **Backstage color themes** — six selectable app-wide color themes (View →
  Theme): Default, Tan, Blue, QLC+ Original, Red Shift (keeps the blue
  channel near-zero, for working in the dark without wrecking night vision),
  and VS Code Dark.
- **Move-in-black / pre-positioning** — Mark cues to silently pre-position
  movers before a reveal, with a live dangle-detector footer chip flagging
  any fixture left un-marked.
- **Look scope** — a Scene/Look can record whether it is meant for the
  whole stage or a specific fixture group. It is kept in the show file but
  no longer shown in the Looks editor: it changes nothing that lights, and
  next to Targets it read as a second place to choose fixtures.
- Real **File → Import** for `.qxw` — merge fixtures, fixture groups, and
  functions (with their full dependency closure) from another workspace
  file into the one currently open, with automatic ID-collision remapping.
- Rebrand throughout: application, launcher, and fixture-editor binaries,
  macOS app bundle, and About box now identify as qlcconsole, with clear
  in-app attribution to QLC+ and its original authors.
- **Inventory-first workflow** — fixtures can be added to an inventory with
  no address, then placed, grouped and programmed before they are patched
  (they send no DMX until then). A docked Inventory panel in Lighting Studio
  drags them onto trusses, risers and towers; the **Patch** dialog patches
  or re-addresses a selection in one go (keeping their spacing or packed),
  and fixtures can be returned to the inventory in Design mode. Drop
  fixtures or a group on a universe in Fixture Manager to patch them there.
- **Network design** — place Art-Net/sACN nodes and wireless DMX
  transmitters/receivers on the plot (from a catalog of common gear, or
  your own saved models); the Network overlay draws each port's cable run
  through its fixtures, and **More → Network Report…** lists nodes, ports,
  universes and run lengths, with Copy and Save CSV. Unpatched fixtures can
  be placed on the nearest node port with room.
- **WLED output** — Wi-Fi LED devices running WLED get one universe each,
  by IP, sent as WLED's own JSON state only when the look changes (about ten
  times a second at most). Devices that don't answer, or are on weak Wi-Fi,
  are reported in the footer; while the rig view is open it shows what each
  device's LEDs are actually doing. Looks can pick WLED's own effects for
  the device to run, and **160 WLED effects** play in qlcconsole itself —
  every 1D effect, the 23 sound-reactive ones on the audio input. Tried on
  real devices only briefly so far.
- **Scene transitions** — a scene can have a Transition in and out that
  reaches each fixture by where it stands: wipes across, up/down or
  front-to-back, from/to the centre, dissolve, or along each bar, set on
  the scene's canvas in the Programming tab.
- **Effect browser** — the look editor's effect picker is a searchable
  browser with categories, a Recent list and a live 16-light preview of
  each effect. The RGB Matrix effects hidden behind same-named scripts are
  listed too ("Plasma (RGB Matrix)"), including the bottom-up Fire.
- **Readiness footer** — the "Ready" chip becomes "Not ready (N)" and lists
  unpatched fixtures, fixture types with no light output declared, address
  clashes, duplicate names, unreadable or duplicate definition files,
  universes with fixtures but no output, WLED devices not answering, files
  in cloud storage not on this computer, and fixtures with no definition
  here. Select rows (shift-click for a range) and fix them from the list.
  See [the feature guide](docs/features/other-improvements.md#readiness-footer).
- **Cloud-storage safety** — online-only files in a synced folder
  (Synology Drive, iCloud, OneDrive, Dropbox) are skipped at startup
  instead of waited on, with a footer warning and **Download Now**. A
  **show cache** on this computer keeps every non-shipped definition a show
  uses; before a show loads, each comes from the library, the cache, or a
  cloud fetch you can Skip.
- **Fixture definition repair** — a definition that won't load opens in
  the Fixture Editor's repair view, showing where it breaks and offering
  tested repairs; a Problems panel checks definitions as you edit, and each
  definition shows whether it is Included or your own (Local).
- **Stage targets** gain kinds (Person, Drum kit, Area, Structure), can
  follow the truss, platform or tower they're bound to, and scenes say
  "can't reach" when a head's pan/tilt range can't get there.
- **Layers panel** — rename any row with Enter, Select All Similar, fixture
  groups listed under the structures their fixtures are on, and plot
  selection mirrored in the list.
- **Plot** — Select All (⌘A), one undo per drag, and one lock per layer
  (everything locked is outlined red).
- Fixture names are unique: renaming to a taken name is refused, a
  duplicate is numbered on add or import, and older shows' duplicates can
  be numbered from the footer or Fixture Manager.
- A batch of network fixtures gets one IP each, counting up from the first.
- The look editor warns when a colour look is nearly dark (a hue picked
  from black), and a palette being edited is tried on the open look so the
  rig follows.
- Programming tab: search over Fixtures & groups, and "← Back to <scene>"
  from a fixture group's layout view.

- **In-app help** — F1 opens the guide for whatever is in front (each tab,
  the Rig Overview, Fixture Properties, the Patch dialog, the look editor,
  the power dialogs); the Help menu lists Getting Started, What's New,
  Keyboard and Mouse, Effects and Troubleshooting, with search across every
  guide. The guides are bundled into the app, and a test checks every link
  in them.
- New guides: Getting Started (startup and the ways a show comes together),
  What's New, Keyboard and Mouse, Effects, Troubleshooting.
- **Text** effect — words on a pixel grid, scrolling or still, in the look's
  colours; effect scripts can declare a `"text"` setting (a text box in the
  look editor). The RGB Matrix Text algorithm gains a **Centered** style.
- **Step Test Pattern** effect — strips counted, steps numbered and named,
  and a spinner that stops if output freezes.
- **Patch Others Like This** / the Patch dialog's **Match** — like
  structures (four towers, four steps) patched alike: each fixture at its
  counterpart's address, in its own structure's universe.
- **Mount on → Deck** — the selection stood on the deck under each (or a
  named one), upright, on its top, nothing floating.
- The Rig Overview follows the Lighting Studio's layers.

### Changed
- The version you see is the release: the About box, `--version` and
  Finder say **0.1.0**, with the build (`4.14.2+qlcconsole.<N>`, what a saved
  show records) and the commit beside it (see [RELEASE.md](RELEASE.md)).
- **Qt 6.8 or later only**; Qt 5 is no longer supported (the last tree that
  built on it is tagged `qt5-last`).
- Opening the app goes straight into qlcconsole — no window asking whether
  to open the Fixture Editor (Fixtures → Fixture Editor…; a double-clicked
  definition still opens in it).
- Showing and hiding layers, and their labels, work in Operate mode; locking
  stays a Design-mode change.
- A look stacks one way: top to bottom, the lower wins; an effect's colour
  replaces the base; dimmer effects scale and multiply; a Colour or Dimmer
  under an effect is that effect's only.
- Main tab order regrouped to follow the build workflow: Hardware →
  Inputs/Outputs → Lighting Studio → Functions → Programming → Shows →
  Virtual Console → Simple Desk.
- Function Manager, Virtual Console, the Programming tab, and the Show
  Editor each replaced their long rows of individual "New X" buttons with a
  compact "Add ▾"/"Edit ▾" dropdown; a View → Toolbar Style menu switches
  every toolbar at once between icons+text, icons-only, and text-only.
- Lock is now the layer's lock: locking any object locks its layer. Older
  shows' per-object locks move onto a locked layer named "Locked" on open.
- When two local definition files describe the same fixture, the most
  recently changed one is used.
- View → Rig Overview… opens its window without switching to the Lighting
  Studio tab.
- The effect script called "Fire" is now "Firelight" (looks using
  it keep it), so the RGB Matrix Fire is reachable again.

- A drag in the Lighting Studio only moves a fixture; **Shift** held as you
  drop hangs it on the truss or pipe, or stands it on the deck, it's over
  (the outline turns green while it would). Inventory drops still place.
  A mounted fixture is only offered a truss or pipe at its own height.
- **Functions** has no tab — Window → Functions (⇧⌘4) opens it in a window
  of its own.
- ⇧⌘R closes the Rig Overview when it's in front; ⌘W closes it too.
- Orientation is offered for anything that pans or tilts, bar-shaped or not,
  and Test orientation names its centred pose (straight up / down / out).
- Fixtures with a structure above them draw faded and dashed in the Top
  view; the rulers band any selection and mark the middle of what's dragged.
- The Connections Devices header is one row.
- Opening a show with many universes is much faster: a 94-universe show
  loads in about 9 s (was 34 s), in a tenth of the memory.
- A fixture whose definition isn't on this computer draws in the rig view
  as one small box (not a wall of red heads); the footer says which folder
  to copy the definition into, and **Fetch Definitions** looks there again.
- Off a Mac: the themes are readable (buttons, lists and splitter handles
  follow the theme), and the help spells keys Ctrl / Alt / Shift.
- Linux: the app icon shows at every size, and on Wayland.
- Effects on big pixel rigs: about 8 ms a tick for 96 × 64 pixels (was 68);
  the rig view about 9 ms a frame on a 200-fixture show (was 51).

### Fixed
- **Lights behind a closed curtain glowed through it in the Rig Overview.**
  Steps just upstage of the closed main drape showed their haze glow on the
  drape and on the floor in front of it, and the pools their light made on
  the back of the cloth. Opaque scenery and platforms now hide a light's
  glow, and pools on the far side of a drape stay there; where the drape is
  open the glow shows through the gap, and a scrim is still see-through.
- **Saving a fixture definition could stall for minutes on a busy Mac:**
  it flushed every disk instead of the one file. It now writes the file
  whole (or not at all) and flushes only that file.
- **Universe patches were lost when a show was opened away from its
  network.** Opening a show on a computer without the interface it was
  patched to (a laptop off the show VLAN) and saving it wrote the show
  without those patches — every Art-Net universe on that interface gone,
  however often it was re-patched. Patches that can't be opened are now
  kept, shown as offline ("not present — kept, will reconnect"), saved back
  unchanged, and reconnect by themselves when the network appears; inputs,
  feedback and patches to plugins not installed are kept the same way.
  Art-Net, E1.31 and OSC patches also **follow the network** rather than
  one interface address: the same show opens patched on any console on the
  show network, through whichever interface (or route) reaches it.
- A workspace whose last-active tab was Lighting Studio could come up with
  the whole 2D plot empty on a fresh launch — trusses, layers, and every
  fixture missing — even though the save was completely intact.
- Selecting a truss no longer drags along a fixture you click immediately
  afterward, and dragging a fixture bound to a truss no longer drags the
  truss along with it.
- A fixture dropped near (not exactly centered on) a truss can now be
  placed anywhere across the truss's width while still touching it,
  matching how fixtures are actually rigged, instead of always snapping to
  dead center.
- A show file with two fixtures on overlapping addresses dropped the
  second on load, with every group, scene and cue that used it. It now
  loads unpatched and flagged, and is saved back as it was.
- A fixture whose definition was missing was saved as "Generic", losing
  the real fixture for good. It now keeps and saves its manufacturer, model
  and mode, saving warns first, and Fetch Definitions restores it.
- Startup could hang for as long as cloud storage took to answer, or for
  good when it couldn't be reached.
- A steady effect could flash: the output sometimes caught it between
  frames and sent a blank one.
- An unpatched fixture in a running scene could crash the app.
- A gobo look sent nothing, and a new gobo look had no wheel to pick from.
- A look dropped on the canvas now lands where it was aimed, and a colour
  change to a look reaches every scene that uses it.
- Creating a palette or function in a new folder renamed the workspace's
  first look to the folder's name.
- Add Fixture gave every fixture in a network batch the same IP.
- Lanterns on hangers that kept an earlier mount's face drew on their side;
  a fixture on a hanger now always hangs straight.
- A light at the foot of a tower, moved under the bar on top of it, was
  hung on the bar.
- A copy of a fixture's physical data recompiled a regular expression and
  wrote a guessed power figure into the copy.
- **Don't Save** left the unsaved show's autosave behind, so every start
  offered the discarded work back.
- Two static looks on one channel kept the higher value (Blue then Red came
  out magenta-ish); a second Colour look moved a colour wheel; an Aim or a
  Dimmer after a movement effect did nothing; Breathe did nothing on movers
  with only a head dimmer.
- The speed master's beat blinked in the footer with nothing playing.
- The Xbox controller profile was installed where the HID plugin never
  looked.
- A crash under heavy load: with a busy show running, scenes and effects
  starting and stopping could corrupt memory and bring the app down (found
  in a 24-hour soak). Writes past the end of a universe are refused too.
- Lighting Studio's **Add Target**, **Add Person**, **Copy** and **Paste**
  did nothing.
- The patch grid didn't follow universes being added or removed, and the
  Function Manager's buttons didn't follow the selection.
- Windows: a slow tick could pile up hundreds of timer threads until the
  app stopped responding.
- Windows: the app felt sluggish — every output check walked every network
  interface; and USB devices plugged in while running weren't noticed.
- Art-Net send-failure warnings flickered on and off for a target that
  never answered; and Connections read a merging node port as "SHORT
  DETECTED".
- Engine: a fader's priority could be raised but never lowered, and an
  overriding 16-bit channel wrote only its low byte, to the coarse address.
  Nothing in 0.1.0 hit either yet; fixed ahead of the features that will.
- Rig Overview: an RGB fixture with no dimmer channel (pixel bars, many
  par cans) drew far too dark, or black, when dimmed: its level was counted
  twice.
- macOS: pressing the empty canvas in Structure Studio's Angled view to orbit
  crashed the app. Qt 6.11 draws its move cursor (and the wait/busy ones)
  from a colour-profiled picture that it frees too early (QTBUG-147602); the
  move cursor is now the open hand (closed while dragging) on macOS, and
  wait/busy the arrow. Linux and Windows are unchanged.
- macOS: the same Qt 6.11 fault (QTBUG-147602) could crash the app wherever
  one of its own pictures -- an icon, a gobo, a help picture -- was handed to
  AppKit, since 508 of the 660 bundled PNGs carried a colour profile. The
  profiles are removed (the pixels are unchanged, and the app never used
  them), and the test gate now fails if one comes back.

### Known issues
- **Windows:** on heavy shows (around 90 universes with thousands of
  pixel fixtures) the engine couldn't keep its tick rate. A fix is in
  (the test VM now ticks that show at about 8 ms on average) but it hasn't
  had a long soak on a heavy show yet. Moderate shows run normally.
- **macOS:** over a long run the on-screen app's memory grows with no crash
  or slowdown — about 50 MB an hour in the first 24-hour soak; in the
  second, 312 → 492 MB over the 8-hour real-show phase. Being tracked
  down.
- **Linux:** DMX output timing runs at normal priority unless the system
  grants real-time priority (CAP_SYS_NICE or an rtprio limit).
- The universal Mac app has no OLA plugin.
- Not yet tried against the real thing: QLab / MIDI Show Control end to
  end, Art-Net trouble reporting on a failing node, and opening a show's
  carried definitions on a machine without them.

[0.2.1]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.1
[0.2.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.2.0
[0.1.0]: https://github.com/sandinak/qlcconsole/releases/tag/v0.1.0
