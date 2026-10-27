# What's new

The latest changes, newest first. Each links to the guide with the detail.

## 0.3.0 (October 2026)

The Programming tab is now **Build**, with a Workbench that edits scenes,
collections and chasers as layers; the Show Tree and Build share it. Old
QLC+ shows open in a **Classic QLC+** view and can be upgraded in one
step. Fixtures get plot symbols and output rules, and Eos shows import with
their house lights, relays and subs.

### The venue chip

- **Which room, always in sight.** The footer says the venue applied —
  **Venue: Lincoln HS**, or **Venue: none** — and turns amber with a
  count when it clashes with the show's own rig. Click it for the venue
  menu: switch venue, new, rename, the venue page, venue focus, the focus
  report and the clashes. **View ▸ Venue…** and the Lighting Studio's
  **Focus ▸ Venues** open the same menu. See [The venue
  chip](venues.md#the-venue-chip).
- **The apron on the floor.** The apron has a **Height** (the main
  deck's until you set it; 0 is the floor), and hiding it no longer moves
  or drops the lights on it. See [The apron](lighting-studio.md#the-apron).
- **A locked layer asks.** Placing something while the active layer is
  locked asks which layer it goes on, or unlocks it for you. See
  [Locks](lighting-studio.md#locks).

### Patch list and fixture labels

- **Grouped by where it hangs.** The patch list groups by **Location** —
  the truss, bar or tower a fixture hangs on, else its fixture group —
  or by universe or type, in address order under each heading.
- **Only what you need.** **Show** discrete fixtures or the RGB Matrix &
  pixel ones, and only the Tour, House or Rental fixtures; the copy, CSV,
  the new **XLSX** export, the PDF and the labels follow.
- **One Mode column.** The channel count is in the Mode when its name
  doesn't say it ("Open + Fly (2 ch)").
- **Labels on tape.** Right-click a row → **Print Label…**: "Spot 3 D101"
  (a template you can change) on 12 or 24 mm tape, printed through the
  system's print dialog — with a preset for the **Brother PT-P710BT** —
  or saved as a PDF. A PT-P710BT set up on the Mac is found and picked
  when you haven't chosen a stock. See [The patch list](fixture-manager.md#the-patch-list).
- **What's missing stands out.** A fixture not patched yet is highlighted
  amber in the list: **— not patched**.

### House rig and tour package

- **Owner: Tour, House or Rental.** Set it on a fixture group (beside its
  name in the Fixture Manager) and its fixtures follow; one fixture can
  say otherwise in Fixture Properties.
- **The house rig lives in the venue.** With a venue applied, House
  fixtures are saved in that venue's file, with their plot places and
  the groups they fill. Applying another venue swaps the house rig; the
  tour package is not touched.
- **Cues on groups play on any room's rig.** A cue on "FOH wash" plays
  on whichever house fixtures fill FOH wash in the venue applied.
- **Universes per room.** Each universe is a **Tour** or **House** slot
  (**Connections ▸ Universes**, on the toolbar beside its name). A venue keeps the room's outputs for
  every universe (an output changed while it is applied is the venue's),
  and can mark universes or ranges **not ours**: nothing is sent there.
- **The clash check.** Apply now lists a house fixture on a tour
  fixture's addresses, anything on channels that are not ours, and
  fixtures in the other kind's slot, before the venue goes live. The
  venue page (**Rig and universes…** in the venue menu) has it all; the plot
  has an **Owner** overlay. See [House rig and tour package](venues.md).

### Quick focus, Venue focus and Focus this cue: the controller on stage

- **One Focus menu.** The Lighting Studio toolbar's **Focus ▾** holds
  every focus tool: Quick focus, Venue focus, Edit focus calls, Venues,
  Practice (scramble) and the Focus report. What cannot be used yet is
  greyed with the reason — no lights on the plot, no focus calls, no
  venue — there, on the Run tab and in the Show Tree. Quick focus says
  why its Start is greyed, and offers **Add a target…** when the show has
  none to aim at. See [The Focus menu](lighting-studio.md#the-focus-menu).
- **Venue focus.** The show keeps its focus in order — "DH Tspots →
  DSC", then "AT spots → Soloist DS" — and runs it on the controller:
  triggers step the calls, bumpers the fixtures, the stick nudges, A
  saves. Corrections are saved **for the venue**, in a file beside the
  show, the moment A is pressed; the show file never takes them. See
  [Venue focus](focus-calls.md).
- **Focus this cue.** The same controller on a Show Tree cue's own values:
  position, zoom, focus, colour, gobo — the layer as a whole, or one
  fixture as an exception (a child layer). See [Focus this cue](refine.md).
  It works on a Build scene too, as **Focus this look** from the
  Workbench's **Focus ▾** (see
  [On a Build scene](refine.md#focus-this-look-on-a-build-scene)), and in
  Operate on the running cue only, quietly (see
  [Focusing during the show](refine.md#focusing-during-the-show)).
- **A venue focus cue.** Tick **Make this a venue focus cue** on a
  pre-show cue (or pick it from the **Focus ▾** in the cue page's or the
  Workbench's header): GO arms the focus calls, and Start on the
  controller begins them. See
  [A venue focus cue](focus-calls.md#a-venue-focus-cue-pre-show).
- **Say the light.** The right stick click says the light's name and
  where you are; **Feedback** picks lights, voice or both (hold it to
  switch). What is picked blinks, and the laptop says it: "AT
  spots, layer", "Fixture 3, Spot 3". **Voice** beside Feedback sets how
  much is said: terse ("F3 Spot 3", "Saved"), standard, or verbose
  ("Fixture 3 of 8, Spot 3", and the call's target and where it is). A
  held A says "Saved all, 6 lights."; a save in a venue names the venue.
- **One layout.** Y is now the scope in Quick focus too (one head, or all
  of them); the second reference moved to the left stick click.
- **Still at the venue?** A show whose venue was last changed more than 20
  hours ago asks on open, New venue being the default; a venue focus cue always
  asks. The footer shows **Venue:** and its name. Venues also keep the
  room's truss trims and target positions, and the **target page** (left
  stick click in Venue focus) moves a call's target with every head on
  it. See [Venues](focus-calls.md#venues-corrections-that-stay-with-the-room).
- **Safe on stage.** A controller that drops out freezes everything it
  held ("Controller lost. Lights held."). Warnings are always spoken,
  with a slow blink and one long buzz. The first pad to press Start owns
  the tools. The save lamp is blue, yellow and orange with a tick or "!"
  and a count, in Focus this cue too. See
  [Telling the operator](focus-calls.md#telling-the-operator-where-they-are).
- **Venue files travel.** Collect for Transfer copies the venues, and a
  show saved under a new version offers the earlier version's venues.
- **The focus report.** **Export focus report…** writes a venue's focus
  light by light — the call, the target and its place from the stage
  marks, the correction, the lens, who checked it and when, and the
  dead, swapped and needs-focus flags — as a PDF or web page to print, or
  a CSV for Lightwright. See [The focus report](focus-calls.md#the-focus-report).
- **Practice.** **Practice (scramble)…** knocks every moving head in the
  calls off its target by a random amount, in a practice venue of its
  own; the crew focuses them back and is scored on degrees off, time and
  saves. The real venues are never touched, and it works on the
  visualiser too. See
  [Practice](focus-calls.md#practice-scramble-and-focus-back).
- **On the pad.** Hold B puts back every light not saved (one undo
  step). Both stick clicks hold every light ("Holding."). View + X marks
  a light dead. The D-pad speaks the crew's words: spot down, flood out,
  sharper, softer. The calls say where the target is ("SL 2, US 1"), a
  light clicked on the plot is the next one worked, the focus look takes
  the gobo, prism and frost out, and a new venue starts from the last one.
  On a Mac a **PlayStation pad** (DualShock 4, DualSense) works like an
  Xbox pad, and its light bar shows the save lamp's colour.

### Build and the Workbench

- **The Programming tab is Build.** Over its left side, **Palettes |
  Effects | Scenes** picks what is listed, with groups and fixtures under
  each. On the right, the **Workbench** shows whatever you load. The tab
  wears a yellow hard hat. See [Build](build.md).
- **A collection as layers.** Click a collection in Build ▸ Scenes and
  each member is a layer: a scene opens up into its groups and looks, an
  EFX or an RGB Matrix plays as it is (▣). **+ Scene** and **+ Effect ▾**
  (EFX, RGB Matrix) add one; ▲ ▼ reorder, ⌫ takes a layer out (the scene stays
  in the show), and scenes drag in from the left. **▣ Open** loads a layer
  on its own and **↩** comes back. A scene used elsewhere says **also in
  2**; **Own copy** splits it off. What is loaded plays on the rig as you
  work (Design mode), and every change is one ⌘Z. See
  [The Workbench](build.md#the-workbench).
- **Every scene in one tree.** Build ▸ Scenes is an outline of folders,
  scenes, collections and chasers, each scene opening into its targets and
  looks. Drag a row to another scene to move it (⌥ copies). Every function by
  type is the stock Functions tab. See [The outline](build.md#the-outline).
- **Walk a chaser.** A chaser loads step by step: **◀ ▶**, the step's
  fades, hold, cue number and note edited in place, the step table above
  its layers. **Chaser from selection** makes a chaser of the scenes you
  pick, in the order you pick them. See
  [Walking a chaser](build.md#walking-a-chaser).
- **Make effects in Build.** Build ▸ Effects has **+ New ▾** (an effect
  palette, an EFX or an RGB Matrix) and a right-click menu. An effect
  palette opens in the **effect designer**: the effect browser at full
  height beside its settings, its own colour and dimmer, playing live on
  the heads it is for. **Make a layer** turns the bench's groups and
  effect into a new layer of the cue or scene you came from. See
  [Making effects](build.md#making-effects),
  [The effect designer](effects.md#the-effect-designer) and
  [Make a layer](effects.md#make-a-layer).
- **A look's name follows its value.** Change a colour look from red to a
  darker red and "Red" becomes "Dark Red"; only that part of the name
  changes, and a name of your own is left alone. Intensity, strobe and
  shutter names follow the same way. See
  [The name follows the value](build.md#the-name-follows-the-value).
- **One way to upgrade.** **Upgrade This Show…** is the one door from
  chasers to cue lists: File menu, the Show Tree's header (it replaces
  Import chaser…), the Run tab's empty state, and Build's **Send to Show
  Tree…**, which opens it with just that chaser ticked. A chaser upgraded
  already shows greyed, with why and **Restore original…** on its row, and
  with nothing left to upgrade the File menu's entry still opens and says
  so. See [Send to Show Tree, from Build](show-tree.md#send-to-show-tree-from-build)
  and [Upgrade this show](show-tree.md#upgrade-this-show).

- **+ Effect ▾ and Script ▾.** The Workbench's **+ Effect ▾** adds an
  **EFX** or an **RGB Matrix** (it was two buttons), and the Look Editor's
  **Script ▾** holds **New script…** and **Edit script…**. The effect
  designer has no **Save as preset…** any more: keep an effect as a
  palette, or **Export…** it to another show. Build ▸ Effects is effects'
  only home: there is no **Window ▸ Effects**. See
  [The Workbench](build.md#the-workbench) and
  [The effect designer](effects.md#the-effect-designer).
- **A group set dropped on a scene adds its groups.** *Towers* onto a
  Build scene adds *SL* and *SR*; it used to be refused without a word.
  See [The outline](build.md#the-outline).

### Views, menus and keys

- **One word per idea.** **Plot Symbol** is what *Stage Look* was
  (what a fixture is drawn as: Fixtures right-click, Fixture Properties,
  the plot's **Show** menu); the Lighting Studio's *Layers* panel is
  **Plot layers**; **Operate** replaces *Perform (Operate)*; and the
  Control menu's *Hold Last Look* is **Hold at End**, with **Clear Hold
  at End** (and a Shows track's **Hold at end**). Only the names changed:
  shows and settings are as they were. See
  [Plot symbols](lighting-studio.md#plot-symbols).
- **Three views.** **View ▸ Workspace** picks the tabs: **qlcconsole**
  (Connections, Fixtures, Lighting Studio, Build, Show Tree, Virtual
  Console, Simple Desk, Run; Shows and Functions in Everything), **Classic QLC+** (QLC+'s own tabs, in its order
  and names) or **Everything**. The view is saved with the show. An old
  QLC+ show opens in Classic QLC+; everything else, and a new show, in
  qlcconsole. Choosing a hidden tab from the Window menu shows Everything,
  with a way back. See [Views](getting-started.md#views).
- **Functions.** "Classic" means stock QLC+ only: Build ▸ Scenes no longer
  has a Classic arrangement, and **Window ▸ Functions** opens the stock
  Functions tab (from the qlcconsole view it switches to Everything, with
  a way back). Build ▸ Effects' EFX and RGB Matrix heading reads "QLC+
  effects". New functions of any type and the Function Wizard are on
  **+ Scene ▾**. See
  [Coming from the Functions window](build.md#coming-from-the-functions-window).
- **⇧⌘1… follow the tabs on screen.** The Window menu numbers the view's
  tabs in the order you see them, so ⇧⌘1 is always the first tab: in
  Classic QLC+ that is Fixtures, in qlcconsole Connections (the Show Tree
  is ⇧⌘5 and Run ⇧⌘8 there). Change the view and they renumber. Tabs the
  view hides come after, with no shortcut. See
  [Keyboard and mouse](shortcuts.md).
- **Settings ▸ Appearance.** The theme, the toolbar style, which chips
  the footer shows and how the speed masters show their beat are in a new
  **Appearance** tab in Settings, out of the View menu; your choices carry
  over. **View** is now only what you look at: **Workspace**, **Address
  Tool**, **Rig Overview…** and **Quick Focus…**. See
  [Appearance settings](other-improvements.md#appearance-settings).
- **File ▸ Import is a menu of file types**: **QLC+ / qlcconsole
  Workspace**, **ETC Eos Show (ASCII .asc)** and **QLC+ Fixtures List
  (.qxfl)**, each with a file dialog for that kind, then **Any Supported
  File…**. See [Importing into a show](getting-started.md#importing-into-a-show).
- **Esc backs out.** Esc takes you back one level from an editor that
  covers a page: a palette opened over Build's rows (it has **↩ Back**
  now), a group's layout, the effect bench (asking first if the effect
  isn't kept), a ▣ opened layer, the Workbench docked in the Show Tree,
  the Show Tree's Look Editor. Edits stay. Esc in a rename still only
  cancels the rename, and plain Esc never stops playback (**Stop all** is
  ⌘⇧Esc). See [Backing out](build.md#backing-out-esc).
- **⇧⌘N makes the next thing** for where you are: a cue list, the next
  cue, a layer on the groups picked in the Library, a scene in Build's
  selected folder, a group in the Fixtures tab. **Edit** names it first
  (*New Cue After 0.0.3*). See [New here](show-tree.md#new-here).
- **The boot window shows the version** that is starting, and the build
  and commit below it.

- **A smaller qlcconsole view.** Eight tabs, ⇧⌘1–⇧⌘8; Shows and
  Functions are in Everything. The **Rig Overview** opens in its own
  window, kept in front of the main one. **Classic QLC+** has QLC+'s own
  toolbars and a footer of just the mode, the Grand Master and Ready. See
  [Views](getting-started.md#views).
- **A quieter footer.** One amber **Overrides** chip ("Overrides: Park (3),
  Hold at end · Clear") lists what holds the rig and clears it in one
  click. **Load** is off unless you turn it on (Settings ▸ Appearance), the
  power estimate shows only for a show with its power set up, and **MTC**
  only once a timecode source is picked or code is heard. **Next:** is
  status only: click it for the Run tab, where GO is. See
  [Overrides chip](other-improvements.md#overrides-chip) and
  [What's next](show-control.md#whats-next-and-go-from-anywhere).
- **Operate is the lock.** Show-mode Lock (and its ⌘L) is gone: Operate
  locks the Virtual Console's programmer faders and the Shows timeline,
  and a console Grand Master fader works in Operate. GO pressed in Design
  offers **Switch to Operate**, once a session. See
  [Running the show](getting-started.md#running-the-show).
- **Edit ▸ Undo undoes the tab in front** — Build, the Show Tree, Fixtures,
  the Lighting Studio, or Connections' **Undo Patch Change** — and
  elsewhere reads *Nothing to undo on this tab*. It no longer undoes a Show Tree step from another tab. See
  [Finding things](troubleshooting.md#finding-things).
- **Connections opens where you'd look**: on **Universes** (it was
  *Detailed*) in Classic QLC+, on **Devices** otherwise. Show control and
  timecode are rows at the bottom of Devices: **OSC in**, **MIDI Show
  Control in** and **MIDI timecode**, each with its state; the separate
  sub-tab is gone. See [Checking the hookup](show-control.md#checking-the-hookup).
- **Pages squash with the window.** A row of buttons too wide for a narrow
  window scrolls sideways instead of holding the window wide.
- **The Show Tree and Run have their own tab icons**: a cue tree and a
  green GO button.
- **Classic QLC+ Functions works as in QLC+.** Click a function and its
  editor opens beside the tree, using the whole window; a scene the Show
  Tree made (an Eos show's cue scenes) opens greyed under a line saying
  where to edit it. Elsewhere a double-click opens one, as before. See
  [Coming from the Functions window](build.md#coming-from-the-functions-window).
- **Classic QLC+ Functions lists your functions, not the Show Tree's.** The
  scenes, collections and chasers the Show Tree makes for its cues are left
  out; right-click → **Show the Show Tree's own functions** lists them.
- **The scene editor has General on top and the sliders below**, with a
  bar between them to drag, and a scene of looks made in Build lists its
  fixture groups and looks instead of looking empty. A long row of sliders
  scrolls sideways with **Shift + wheel** or a drag on the space between
  them. See [The scene editor](build.md#the-scene-editor).
- **Always show scroll bars** in Settings ▸ Appearance, for macOS's
  scroll bars that otherwise show only while scrolling.

### Show Tree

- **Cue order is number order.** Type a new number (the cue's **Number**
  field, or **F2** on its row) and the cue moves to its place; a number
  already taken is refused. Drag one cue or several and each dropped cue
  is numbered for its new place (between 2 and 3 → 2.5); no other cue is
  renumbered. Right-click a cue → **Renumber this level…** numbers its
  level 1, 2, 3… (or from a start, in steps), point cues and sub-cues
  following. Each is one undo step. See
  [Reordering and numbering](show-tree.md#reordering-and-numbering).
- **A cue in the Workbench.** Double-click a cue, a layer or a hit, press
  **✎ Workbench** or type `wb`: the cue opens in the Workbench in place of
  the page, its own layers opened up a row per attribute, what tracks in
  greyed with where it comes from. It plays in context, or **Solo** the
  selected layer. **↩ Show Tree** goes back. See
  [Working on a cue in the Workbench](show-tree.md#working-on-a-cue-in-the-workbench).
- **Skip a cue without deleting it.** Untick its box (or **⌘/**, or
  `skip 0.0.3`): GO, auto-follow and Back pass over it, and the next cue
  plays as if it weren't there. See [Skipping cues](show-tree.md#skipping-cues).
- **Move in black, per list and per cue.** The show's auto move-in-black
  is the **MIB** button in the Show Tree's header (*MIB · 3* while it
  pre-positions; Build's **Move in black ▾** shows and changes it too) —
  one setting, saved with the show; a new show starts with it off. A cue list can say **On** or
  **Off** whatever the show says, and a cue can say **Don't move in black
  into this cue**. See [Move in black](show-tree.md#move-in-black).
- **Cue colours.** Give a cue list or a cue a text colour (right-click →
  **Colour ▸**, or `textcolour red`). It shows in the tree, the Run tab,
  the VC Cue Runner and Cue List and the Workbench header. See
  [Text colours](show-tree.md#text-colours).
- **Typed times are kept.** A fade or hold typed on a cue's page is
  applied when you leave the field (Tab, or a click on another cue), not
  only on Return; **Esc** puts it back. See [The tab](show-tree.md#the-tab).
- **A quieter command line.** Its syntax lines are folded away: the **?**
  button beside it, **?** typed first, or **F1** in the line shows them.
  See [The command line](show-tree.md#the-command-line).
- **Notes open up while you type**, in the cue page and in a chaser's
  steps, up to six lines; **Return** applies, **Shift+Return** starts a
  new line.
- **Group tools in the Library.** Right-click groups for **Select all
  similar**, **Make set…**, **Move to folder ▸** and **Output Rules…**. See
  [The Library](show-tree.md#the-library).
- **The Cue Runner widget** takes a controller input and a key for each
  action in a new **External Input** tab, and gives the cue information
  the room as it grows. See [The cue runner](show-tree.md#the-cue-runner).

- **Hits.** The busking states beside the cue lists are **Hits** now
  (**+ Hit**, the **Hits** section, **Adopt as a hit**, **Copy to a hit**,
  `hit "name"`; `look` still works). A layer's looks are still looks. See
  [Hits (busking)](show-tree.md#hits-busking).
- **A column per flag.** **S**, **T**, **R** (sets, tracks in, releases),
  **M** (Block, Cue-only) and **F** (base, skip, no MIB, includes,
  reusable, mix, VC) replace the one column of symbols; hover a header for
  its name, right-click the header to hide one. See [The tab](show-tree.md#the-tab).
- **One Move in black switch**: the Show Tree's **MIB** button. Control ▸
  Auto Move in Black and its Settings row are gone; Build's **Move in
  black ▾** still shows and changes it. See
  [Move in black](show-tree.md#move-in-black).
- **One production, nothing to choose.** With a single production there
  is no Production switcher or *current* marker: GO plays its list. A
  second production brings them back. See [Productions](show-tree.md#productions).
- **A shorter header and right-click.** **More ▾** holds **+ Production**,
  **+ Sub-cue** and **Merge**; **Upgrade This Show…** shows only while
  there are chasers to upgrade. A cue's right-click keeps what acts on a selection
  (**Duplicate**, ⌘D, stays); its base cue, mode and includes are on its
  page.
- **Layers named by their groups.** Fixtures picked one by one are named
  by the fixture groups they make up (*FOH + 1st Elect*), not fixture by
  fixture.
- **Level… and Colour… are docked** in a strip above the command line,
  not a window that could fall behind.

### Old shows

- **Upgrade This Show…** (File menu, and the line at the top of the Show
  Tree for a show of chasers) lists every chaser and collection with what
  would become of it — a cue list, checked to play the same; an effect
  kept as a function; or left alone — and offers to capture raw scenes
  into palettes. **Upgrade** does the ticked parts as one undo step;
  **Restore original…** puts a chaser back. Nothing happens on its own.
  After an upgrade, a Classic QLC+ show is offered the qlcconsole view.
- **Classic QLC+ says when a show uses the Show Tree.** A show whose
  Virtual Console has a **Cue Runner**, or buttons that fire Show Tree
  cue lists or hits, says so in the footer — *This show uses the Show
  Tree. Open the qlcconsole view to edit it.* — with a button to switch.
  It never switches for you, **×** puts it away until the show is next
  opened, and the content still plays in Classic QLC+. See
  [Views](getting-started.md#views).
  The **Run tab** of a show with no cue list says so and offers the same
  **Upgrade This Show…**; its GO is grey until there is something to play.
  See [Upgrade this show](show-tree.md#upgrade-this-show) and
  [An old show: convert it](show-tree.md#an-old-show-convert-it).
- **Originals say what they are.** After an upgrade in place, the old
  chaser and the scenes and collections only it plays open greyed as
  *Original from the import, kept for Restore original*, naming the cue
  to edit instead, with **Show in Show Tree** going there. A scene the cue
  list plays as it is stays yours to edit. See
  [Restore original](show-tree.md#send-to-show-tree-from-build).
- **A chaser made a cue list shows a sheet**: whether the cue list plays the same
  (checked channel by channel), a row per cue, and what plays as it is and
  why. It moves the VC buttons, VC Cue Lists and show control that fire
  the chaser over to the new list. What the steps repeat is set once and
  tracks. See
  [From a chaser you already have](show-tree.md#from-a-chaser-you-already-have)
  and [What repeats is set once and tracks](show-tree.md#what-repeats-is-set-once-and-tracks).
- **Open what a layer plays.** A ▣ layer (a raw scene, an EFX, an RGB
  Matrix) opens its function in the Workbench; its page says where else it
  plays. See [Adding layers](show-tree.md#adding-layers).
- **No outputs: preview only.** A show with nothing patched to an output
  says so in the footer, in grey, instead of a red **Not ready**: nothing
  reaches a rig, so nothing there is urgent. Red is kept for real problems
  on a rig with outputs. See [Readiness footer](other-improvements.md#readiness-footer).
- **Same show, same output.** A show saved by QLC+ 4.x opens with
  **Auto-open shutters** off, so it sends the DMX it sends in QLC+. Shows
  made in qlcconsole keep it on. Turn it on in Settings ▸ Advanced. See
  [Auto-open shutters](other-improvements.md#auto-open-shutters).
- **The stock sample imports the same.** A chase of scenes with their own
  fades now plays in the cue list as in the chaser (it used to run at
  about half the level).
- **A damaged show file says so.** A show cut short or broken opens as far
  as it can be read, with *This show file is damaged or incomplete (line
  N)*, and Save asks for a new name instead of writing over the original
  with less. A fixture on a universe the show can't have opens unpatched,
  and says so, instead of freezing the app. See
  [Troubleshooting](troubleshooting.md#files-and-fixtures).
- **Opened is not changed.** A show whose trusses had no group yet no
  longer asks to save as soon as it opens.
- **Times take either decimal mark.** In the Show Tree's cue times, the
  Look Editor's fades and the outline's times, "1.5" and "1,5" are both
  one and a half seconds, in any language. A chaser step's Fade and Hold
  still want ".".
- **Save As keeps the pictures.** Saving to another folder copies the
  truss and tower pictures into that folder's **images**. See
  [Truss socks](lighting-studio.md#truss-socks).
- **An RGB matrix whose script isn't here keeps it.** It doesn't run on
  that computer, the footer says which, and saving keeps it as it was.
  See [Troubleshooting](troubleshooting.md#files-and-fixtures).

### Fixtures and the plot

- **The orientation diagram shows the throw.** In Fixture Properties a
  fixed light's green arrow now points where it really throws — its face,
  or its Pan and Tilt aim — shorter the steeper it points, with a dot for
  straight down and a ring for straight up. See
  [Fixture Properties](lighting-studio.md#fixture-properties).
- **The Focus screen holds still.** The Quick focus, Venue focus and
  Focus this cue panels keep their layout as the controller's buttons
  change what they say, so nothing jumps under the operator's eye.
- **The voice survives a missing output.** When the output chosen in
  **Voice to:** is gone (a pad asleep, a headset interface unplugged),
  the voice speaks on the computer's own output and says so, rather than
  losing every line. See
  [Telling the operator](focus-calls.md#telling-the-operator-where-they-are).
- **Plot layers follows the plot.** Pick on the plot and Plot layers picks
  the same rows, opens just their branches and scrolls to them. The
  **Selected only** funnel shows only the selection and what holds it.
  What hangs on what is a tree — Tower ▸ Hanger ▸ Lantern, Truss ▸ Bar ▸
  Spot — and right-click a layer for **New Folder**. See
  [Plot layers](lighting-studio.md#plot-layers).
- **Two locks: an object's and its layer's.** **Lock Position** holds one
  thing (still selectable, a padlock on its row); **Lock Layer** holds the
  layer. A 0.2 show's **"Locked"** layer is taken apart when it opens, its
  objects back on their layers and still locked. New things never land on
  a locked layer. See [Locks](lighting-studio.md#locks).
- **Line targets** draw in their own colour (a red ring when locked), and
  their handles snap to the grid. See
  [Line targets](stage-targets.md#line-targets).

- **Snap takes the whole rig.** With **Snap** on, trusses, towers,
  stands, pipes and scenery land on the grid too, not only fixtures and
  platforms. See [Selecting and undo on the plot](lighting-studio.md#selecting-and-undo-on-the-plot).
- **Plot symbols.** Fixture Properties and the Fixtures tab's right-click
  **Plot Symbol ▸** say what a fixture looks like: Auto, or one of sixteen
  symbols in five sections — moving heads, fixed conventionals (PAR can,
  ellipsoidal, Fresnel, followspot), fixed LEDs, **Practicals**
  (Lantern, Staff) and **Scenery** (Curtain). The plot and the studio draw that shape in the live
  colour, and the fixture lists show its icon. It changes only the
  picture, so it is offered in Operate too. See
  [Plot symbols](lighting-studio.md#plot-symbols).
- **Mark a fixture as a curtain.** Plot Symbol ▸ **Scenery ▸ Curtain**
  makes any fixture — a relay on a dimmer channel — a curtain fixture: a
  drape can be driven by it, the plot draws the drape instead of the
  fixture, and its intensity opens the curtain (0 closed, 255 open). See
  [Plot symbols](lighting-studio.md#plot-symbols).
- **Pasting a truss asks about its fixtures**: patch the copies at the
  next free addresses after the originals, leave them unpatched, or
  cancel. See [Selecting and undo on the
  plot](lighting-studio.md#selecting-and-undo-on-the-plot).
- **Stage marks on the plot, as objects.** The centre line and DSC mark,
  the curtain line, the number line and the apron are **stage pieces** on
  their own **Stage** layer in Plot layers, each with its own eye and lock
  (and the layer's). Click one to select it, drag the curtain line or the
  number line up or downstage, drag the apron's front edge; each drag is one
  undo step. Double-click (or right-click, **Properties…**) for where it is
  as a crew measures it — the curtain line upstage of the front of the deck,
  the number line downstage of the curtain line, the apron's depth and front
  curve, each **Auto** until set — always in the show's unit. The curtain
  line is found from the main drape or the proscenium; the number line is
  ticked in feet or metres out from centre, labelled CL, its long ticks only
  on numbered places; trusses and pipes over the audience say how far out
  they hang (**FOH · 4.2 m DS of curtain line**). The full centre line
  replaces the teal centre cross's line along it. The Rig Overview draws
  the lines on its floor and lists the pieces in its **Plot layers** menu,
  and they're saved with the show. See [Stage marks](lighting-studio.md#stage-marks-centre-line-curtain-line-number-line).
- **The apron is its own deck.** It starts at the curtain line and runs
  downstage by its depth, with its rounded front right at its front edge,
  and it's real deck: drawn as deck on the plot and in the Rig Overview
  (from above, at 45° and in 3D, whatever marks are on), and stood on. It
  never moves or resizes another platform, so a riser butted against the
  front of the deck stays where it is. See [The apron](lighting-studio.md#the-apron).
- **The Rig Overview's controls are in a footer bar** along its foot, like
  the Lighting Studio plot's: **Show** (Target positions) and **Plot
  layers** on the left, **View** on the right, and a **Fit** button to put
  the whole rig back in view after a zoom. See [Angled](lighting-studio.md#angled--the-free-3d-view).
- **Where a target is, in crew terms.** Each stage target on the plot,
  each figure in the Rig Overview, and the person Quick focus stands where a
  fixed light lands has a small readout of where it is against the stage
  marks, for example **SL 2.4 · US 3.1 m**. It follows the target as it
  moves. Untick **Show ▸ Target positions** to hide it. See [Stage marks](lighting-studio.md#stage-marks-centre-line-curtain-line-number-line).
- **Auto fixtures as their closest symbol.** The plot's **Show** menu has
  **Show Auto fixtures as their closest plot symbol**, saved with the show.
- **One icon per fixture, everywhere**, and curtains have a drape. Icons
  say what a row is: a Dimmer look is a pie of its level, a Shutter look
  open, closed or strobing, an aim a bullseye, a fixture a fixed wash,
  moving spot or moving wash. See [Icons](fixture-manager.md#icons) and
  [Stage targets](stage-targets.md).
- **Output rules for relays, house and work lights.** Right-click
  fixtures or a group → **Output Rules…** (Fixtures tab, Build, the Show
  Tree's Library): **Ignore Blackout** (it holds through Blind too),
  **Ignore the Grand Master**, **Switch (relay)** — on at 50 %, off below,
  never faded — and **On when not in use**. When those come on is the
  show's setting in **Settings ▸ Advanced ▸ Output**: when you switch to
  Operate (the default) or when the show opens. Fixtures with rules carry
  an amber badge. See [Output rules](fixture-manager.md#output-rules).
- **Edit one definition from several fixtures.** Select fixtures, or a
  group, that share a definition and right-click → **Edit Fixture
  Definition…**. The toolbar's **Properties** opens several at once. See
  [Context menu](fixture-manager.md#context-menu).
- **Rename a group in the tree** with **Return** or **F2**; **Select All
  Similar**, **Move to Folder ▸** and **Create Group(s) from Selection…**
  work in the group's folder. See
  [Fixture-group folders](fixture-manager.md#fixture-group-folders).
- **LED Layout…** on a multi-head fixture edits where each head of its
  model really is, saved with the show or as the model's default. See
  [LED Layout](fixture-manager.md#led-layout).
- **Definition updates.** **Fixtures ▸ Definition Updates…** lists your
  fixture definitions older than the current version, with what an update
  changes, a dated backup and **Undo Last Update**; **File ▸ Show
  Definitions…** does the same for the copies a show carries. See
  [Definition Updates](definition-updates.md).
- **ETC ColorSource Spot jr and CYC**, and the ColorSource Spot's other
  modes (Direct, 1-Channel, RGB).
- **No false warning for a curtain**, fan, hazer or smoke machine in the
  footer's Not ready list.

- **The Lighting Studio, smoother.** Several fixtures Shift-dropped on a
  truss all hang; **Mount on** and a double-click act on the whole
  selection; a truss pastes without its fixtures (**Truss only**);
  scenery, towers, booms and stands copy and paste; a wall reads
  **Length**, **Width** and **Height** and can have a **door**; the stage
  stays centred; a sock's or tower face's pattern picture is copied into
  an **images** folder next to the show when it is saved. See
  [Mounting a fixture](lighting-studio.md#mounting-a-fixture) and
  [Scenery](lighting-studio.md#scenery-cyc-walls-and-drapes).
- **Fixed lights aimed and focused.** A par or profile has **Pan** and
  **Tilt** in Aim &amp; Mount, and Quick focus steps through fixed lights
  too, with a person standing where each lands. Where the beam can
  change, D-pad up zooms in and down zooms out (, and .), and right
  tightens the focus and left loosens it (' and ;). See [Fixed lights](lighting-studio.md#fixed-lights-pars-and-fixed-spots)
  and [The beam](lighting-studio.md#the-beam).
- **Quick focus shows what is saved.** A big lamp in the panel, to read
  from the stage: **blue** with a tick, all saved; **yellow** with "!",
  the light you're on has unsaved changes; **orange**, another light
  has; an orange ring round a yellow centre for both, with the count on
  its rim and the names beside it ("Unsaved: 2 other lights (Spot 3,
  Spot 7)"). A save flashes it blue, and the real light
  dips to dark and back twice so whoever is on stage sees it was kept.
  Every light with unsaved changes is marked in yellow on the plot and in
  the Rig Overview, and leaving names them. See
  [Saving](lighting-studio.md#saving-and-the-save-status-lamp).

### Importing an Eos show

- **Special circuits and subs.** The Eos import has a **Special circuits**
  page: relay, house, work and backstage / hall channels, read from their
  Eos labels, each changeable. They come in with
  [output rules](fixture-manager.md#output-rules) and a group each under
  **Eos** (**Power relays**, **House &amp; Work**, **Backstage**). Each Eos
  sub becomes a Playback fader in a new Virtual Console frame, **Eos
  subs**. See [Importing an Eos show](getting-started.md#importing-an-eos-show).

### Effects

- **Twelve more Matrix effects** — Fire, Starfield, Spiral, Lissajous,
  Kaleidoscope, Bouncing Balls, Fireworks, Scanner, Dissolve, Gradient
  Sweep, Box VU and Image (a picture or animated GIF kept in the show) —
  and an **Each box, staggered** scope that rolls the same picture from
  box to box. See [Matrix effects](effects.md#matrix-effects) and
  [Pictures and GIFs](effects.md#pictures-and-gifs).
- **Matrix effects are lighter.** Bouncing Balls, Kaleidoscope, Lissajous
  and Starfield take about half the processor they did, with the same
  picture, so eight pixel boxes staggered keep time on a Raspberry Pi 5.
  Lissajous lights heads that sit off its grid instead of going dark.
- **A Closed shutter look closes.** *Closed* used to open the shutter on
  any spot with an Open capability; now *Closed* closes, *Strobe* strobes
  and *Open* opens. See
  [Auto-open shutters](other-improvements.md#auto-open-shutters).

### Controllers

- **Controller test** (Quick focus's setup; Settings ▸ Advanced ▸ Game
  controllers) lists every input a game controller sends as it arrives.
  Quick focus says when the controller is paused, and why. A controller
  keeps working with qlcconsole behind other windows, is found again when
  switched back on or after the Mac sleeps, and every Xbox model is read
  in the right layout. See [Controller test](lighting-studio.md#controller-test).
- **A controller coming back is picked up, and said.** A pad (or MIDI
  surface) patched as an input that is switched off, asleep or unplugged
  keeps its patch, profile and controller roles; Connections ▸ Devices
  shows it greyed, "not connected", and the notice line says **Controller
  '…' disconnected — universe 2** in amber. When it comes back, on whatever
  line, it is patched again by itself — **Controller '…' connected** —
  and Quick focus, the follow spot and focus calls carry on with it. A
  show opened with the pad off picks it up the moment it is switched on.
  Quick focus started with the pad asleep says "No controller connected.
  Using the keyboard.", and the pad takes over by itself when you wake it:
  "Controller back."
  See [Controllers and MIDI devices coming and going](other-improvements.md#controllers-and-midi-devices-coming-and-going).

### Connections

- **One connection window.** Right-click a universe ▸ **Edit
  connection…**, or double-click it: the universe, the protocol and the
  interface, then that protocol's settings right there — Art-Net's node
  port, sACN's universe and priority, MIDI's mode, a USB widget's device —
  and the input, feedback and passthrough below. A patched universe opens
  filled in. One window instead of a question per step, and one entry
  instead of a dozen on the right-click. See
  [Editing a connection](other-improvements.md#editing-a-connection).
- **Overview is part of Devices now.** Tick **By universe** and the rows
  carry the patch as columns — Output, IP, U, Mode, Input, Profile,
  Feedback, PT — chosen from the header's right-click or **Columns**, and
  remembered. Double-click one to change it in the connection window. The
  Overview sub-tab is gone. Several universes selected: **Transmit mode of
  N selected universes** sets them all at once.
- **Patch from the Fixture Manager in the same window.** A universe row's
  **Patch output…** (or **Edit connection…**, once patched) opens the
  connection window on that universe. See
  [Patching a universe's output](fixture-manager.md#patching-a-universes-output).
- **MIDI input listens on the channel you pick.** The connection
  window's input channel was one off ("Channel 1" listened on channel 2,
  "All channels" on channel 1 only); it now matches the MIDI plugin.
- **Fixture Properties opens for every fixture**, on the plot or not; a
  fixture not on the plot has its position rows greyed.
- **A steady RDM flag.** An Art-Net node whose poll replies carry a junk
  status byte no longer flickers between **RDM** and **no RDM**: the row
  changes only once the node says the same thing several replies running.
  See [Connections &amp; patch](other-improvements.md#connections--patch).
- **Universes lists this show's universes.** **Connections ▸ Universes**
  shows exactly the open show's universes, however many shows were opened
  before it, and rename, passthrough, the slot and Delete act on the
  universe picked.
- **A fixture too big for a universe opens unpatched.** A fixture whose
  mode takes more than 512 channels loads unpatched, and the footer says
  why, instead of writing past the end of the universe. See
  [Troubleshooting](troubleshooting.md#files-and-fixtures).

## 0.2.1 (October 2026)

### Quick focus: the controller on screen

- **A map of the controller.** The Quick focus panel now draws the
  controller, each control labelled with what it does there — Next head on
  RB, Save on A, Nudge on the left stick, Fine on the right, the step on the
  D-pad. Buttons light while held and the sticks show where they are, so
  the mapping can be checked on the real pad. The labels come from the
  controller profile's roles: another make of pad shows its own names, and
  what it lacks is greyed. Click a control to do it.
- **Controller or keyboard.** **Use: Controller | Keyboard**, in the setup
  and the panel. It starts on the controller when one is connected,
  otherwise the keyboard, and remembers your choice. On the keyboard the
  map shows the keys and the controller is ignored, so a resting pad can't
  drift a head; the keys work with either. A controller unplugged
  mid-session hands over to the keyboard, and is offered back when it
  returns. See [Controller or keyboard](lighting-studio.md#controller-or-keyboard).

### Shutters open by themselves

- **Auto-open shutters.** A spot whose shutter rests closed now lights
  from a level alone: give it White at Full and it opens its shutter, as a
  lighting console does. A shutter look (Closed, Strobe...) still wins, and
  a light with no level stays as it is. It is on for new shows (0.3.0:
  off for a show saved by QLC+), saved with the show, and turned off in the new **Settings → Advanced** tab,
  which also now holds the effect script editor choice and the Show control
  (QLab) button. See [Auto-open shutters](other-improvements.md#auto-open-shutters).

### The window

- **One top bar instead of two.** The open tab's own tools and the show
  controls share one row: the tab's tools on the left (**»** holds what
  doesn't fit in a narrow window), and on the right **Stop all**,
  **Blackout**, **Blind** and a **Design | Operate** switch with the mode
  you are in lit. Blackout lights red and Blind blue while they are on.
  About 57 px more for the tab on every tab. **Settings → Show Controls**
  can move the show controls to the status bar instead; it changes at
  once. See [Finding your way around](getting-started.md#finding-your-way-around).

### Effects

- **Confetti on a two-arm beam, and two new look warnings.** A dimmer
  effect on a fixture whose one master dimmer serves several heads takes
  its brightest head, so a beam with one lit arm lights (it went black).
  The Looks list now marks a Colour look that is all zero (**Black**), and
  a position look on fixtures that can't move because their definition
  marks every Pan or Tilt channel Fine (**Can't move**). See
  [The stack checker](effects.md#the-stack-checker).

### Show control

- **Sub-cues from QLab.** `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play a
  sub-cue (its cue first); OSC `/subgo` and `/subback` step the cue
  playing; LOAD 4.2 puts one on standby and **Next:** shows it. A GO by
  number on a paused list now plays that cue (it used to resume where it
  was). See [Sub-cues from QLab](show-control.md#sub-cues-from-qlab).

## 0.2.0 — Show Tree (October 2026)

### Software updates

- **Update check and install.** qlcconsole checks GitHub Releases once a day
  in the background, and from **Help → Check for Updates…**. A newer
  version shows as a footer chip, **Update available: 0.2.0**, never as a
  pop-up. Its window shows the release notes, with **Install**, **Later**
  and **Skip This Version**. On macOS and Windows, Install downloads the
  update and checks it against the release's SHA-256 list (and, on macOS,
  its signature). It then installs when you quit, or on **Restart and
  Install**, reopening the same show. It never installs in Perform mode or
  while anything is running. Settings → Software Update has the channel
  (Stable, or Pre-releases such as Show Tree) and **Install updates
  automatically**. IT can turn it all off. On Linux and the Pi, it only
  tells you. See [Software updates](updates.md).

### Show Tree

- **Movers with a colour ring.** A ring of LEDs round a mover's lens is a
  head type of its own: group by head type makes **Movers** and **Ring**
  groups, and a Colour look on each sets only the wheel or only the ring
  (on both, each its nearest slot: black is the ring's off, any colour the
  nearest hue). The ring's chases are picked by name — the Look Editor's
  **Wheel / ring** list, or `Ring colour Red/Yellow-Chase`. The 2D plot
  and the Rig Overview draw the ring, live, chases stepping round.
  **Change Definition…** moves fixtures to a drop-in definition with the
  ring as its own head. See [Colour rings](show-tree.md#colour-rings).
- **Several productions, sub-cues in the tree, and the cue runner.** A show
  can hold several **productions**; the **current** one (the header's
  **Production:** switcher, or the footer's GO menu) is what the footer GO,
  ⌘↩, QLab / OSC / MSC and the runner play -- its **main list**, with a
  **timecode** source of its own if it needs one. A cue gets **sub-cues**
  4.1, 4.2… with **+ Sub-cue**: each with its own layers, fade and **Sub GO**
  or **Auto-follow**; they track from one another and leave the next cue
  alone. The new **Run** tab (⇧⌘0) is the operator's view: the cue list
  with ▶ / STANDBY / NEXT / SUB NEXT, a *what changes* line from the
  tracking ("Towers → Red; BackStage in → Green"), the timecode, LIVE /
  DESIGN and big GO, Sub GO, Back, Sub Back, Pause and Stop buttons (Space
  is GO); click a cue to LOAD it, double-click to jump. The **Cue Runner**
  VC widget is the same runner on a page (compact or list), and **Swap
  Widget** turns a Cue List into one. OSC gains `/subgo` and `/subback`, and
  sub-cues answer to their numbers. Typed: `production …`, `main`, `sub …`,
  `follow`, `subgo`. See [Productions](show-tree.md#productions),
  [Sub-cues](show-tree.md#sub-cues) and
  [The cue runner](show-tree.md#the-cue-runner).
- **Looks (busking) and palettes in the Library.** A production now has a
  **Looks** section beside its cue lists: looks built with the same layers
  and rows as cues, which never track and play from VC buttons over
  whatever runs. **Make VC page** puts a button for each on the Virtual
  Console; **Adopt as look** turns an existing scene into one; **Copy to
  look** freezes a cue. The Library's **Palettes** gain the everyday work —
  **+ Palette** by type, rename with **F2**, folders that hold any type,
  drag to file, duplicate, delete that names every cue, look and VC button
  using it — on one **Undo** with the tree (and with the Programming tab's
  palette list). Typed: `palette …`, `look …`, `vcpage`, `adopt`. See
  [Looks](show-tree.md#hits-busking) and
  [Palettes in the Library](show-tree.md#palettes-in-the-library).
- **Show Tree P4: reusable layers and cues, Convert to tracking, Detach a
  list.** Right-click a layer or a cue → **Make reusable…**: it goes into
  the Library (All → Reusable) and can be dropped into any cue of any list
  — a "BackStage" wash in ten cues, a "Walk-in" state in every song. Every
  use stays linked: change one and they all change; **Unlink here** makes a
  copy of its own. **Convert to tracking…** on an imported list takes out
  what only repeats the cue before, turns "includes the cue before" into
  tracking and shows you the changes first — every cue plays exactly as
  before. **Detach to plain chaser…** hands a list back as an ordinary
  chaser. See [Reusable layers and cues](show-tree.md#reusable-layers-and-cues),
  [Convert to tracking](show-tree.md#convert-to-tracking) and
  [Detach to plain chaser](show-tree.md#detach-to-plain-chaser).
- **Show Tree P3: group sets, child layers, cue-wide effects, line aims.**
  Make a **group set** (Towers = SL + SR) once in the Library and use it by
  name anywhere; **⇧-drop** a group onto a layer for a **child layer** that
  sets only what it sets — the parent never shows through it. An effect on
  the cue runs as **one effect across** its layers; a line target on the cue
  aims them all as **one formation**, numbered by where the lights hang, with
  **Spread, Grouping and Cross** rows of their own. Rename a row in place
  with **Return**, copy and paste cues with **⌘C / ⌘V**, and type **`help`**
  in the command line for every word with examples. See
  [Group sets](show-tree.md#group-sets), [Child layers](show-tree.md#child-layers),
  [Line aims](show-tree.md#line-aims) and [Help](show-tree.md#help).
- **Show Tree: Library | Tree | Editor, and adding without dragging.** The
  Library is on the left now, beside the tree. **Return** or a double-click
  in the Library adds what is selected to the tree's selected cue or layer;
  **⌘F** (or **/**) jumps to the filter, type and press Return. A drop below
  the tree's last row is a new layer in the selected cue. A named layer
  is just its name in the tree; its page says what it holds. Select several
  layers and edit them together — Release, a colour, a dimmer, all in one
  step. See [Adding layers](show-tree.md#adding-layers) and
  [Editing several layers](show-tree.md#editing-several-layers).
- **Write a show as one tree.** A new **Show Tree** tab beside Programming:
  productions, cue lists, cues, and in each cue the layers — a group and the
  looks it plays. Drag groups onto a cue for layers and palettes onto a layer
  (or onto the cue, for every layer in it); values that come from the cue or
  an earlier cue show grey with where they came from. The scenes, collections
  and the chaser that play it are made and kept up to date for you, and a
  layer that stays the same from one cue to the next keeps running through
  GO (no re-fade, effects keep their place). **Import chaser…** turns a
  chaser you have into a cue list that plays exactly as it did. See
  [Show Tree](show-tree.md).
- **See the cue you're editing.** In Design mode, selecting a cue in the
  Show Tree plays it on the rig and every edit shows at once; in Live it
  stays blind until GO. Several groups dropped together make one layer (⌥ for
  a layer each), a group dropped on a layer joins it, and the Library shows
  groups in the Fixture Manager's folders. See
  [Seeing it on the rig](show-tree.md#seeing-it-on-the-rig).
- **Show Tree: Record which stage, split / merge, values that reuse
  palettes.** Record asks whether to record the selected cue, the running
  cue or everything lit when they differ (or does what Settings → Show Tree
  says). Several groups make one layer everywhere — drop, + Layer, command
  line — and a layer each with ⌥ or a comma (`Towers, USTs`); right-click
  to **Split** a layer per group or **Merge** layers. A level or colour you
  type or pick reuses the palette that already plays it; a typed one that
  doesn't exist yet becomes a palette named like yours. See
  [Record and duplicate](show-tree.md#record-and-duplicate) and
  [Values you pick or type](show-tree.md#values-you-pick-or-type).
- **Show Tree: drop on the page, a command line that's always there.**
  The cue page takes drops as the tree does — groups on it make a layer
  (several at once: one layer, ⌥ for one each), onto a layer's row join it,
  even a grey carried-in one — and the drop area and the line at the top
  say what will happen. The command line now sits at the bottom of the tab
  (**:** from the tree), and **Tab** completes every word it takes —
  fixtures, folder paths, functions, cue lists, cue numbers, palettes by
  kind — Tab again for the next. A dimmed RGB bar now shows its colour on
  the Rig Overview (it drew black). See
  [Adding layers](show-tree.md#adding-layers) and
  [The command line](show-tree.md#the-command-line).
- **Gobo looks by name.** A gobo look picked on one fixture type's wheel
  plays the gobo of the same name on every other type's own wheel; types
  without it ignore it. See [Gobos by name](show-tree.md#gobos-by-name).
- **A scene two cues share no longer re-fades on GO.** A cue list stepping
  from one collection to another that holds the same scene (or the collection
  before it) now keeps that scene running instead of stopping and starting
  it.

### Stage targets

- **Line targets.** A new kind of aim target: a curve in space that spreads
  the lights aimed at it evenly along it, stage-left light to the stage-left
  end. Drag its ends to stretch or swing it, double-click it to add a point
  and drag that to bend it, drag the line to move it all. An Aim look on
  a line has a **Spread** slider (-5 crossed .. 0 all on the centre .. +5
  end to end) and **Group / Cross** for halves or pairs that swap sides or
  mirror. See [Line targets](stage-targets.md#line-targets).

### Fixture Manager

- **One menu item for one group or many.** Select fixtures and/or groups
  → **Group → Create Group(s) from Selection…**: one group of every head
  (made as the old *Create Group from Selection…* made it — groups
  selected go in as blocks keeping their layout, fixtures off the stage
  fill a grid you can size), a group per head type across them ("Bar · RGB
  pixels" with all 128 RGB pixels of eight bars, "Bar · White pixels" with
  their 32 whites; a par's single head stays "Par · RGB"), or a group per
  head type for each fixture, in a folder. The groups are listed — names,
  head counts, layout — before any is made. It replaces the two separate
  items. See
  [Create Group(s) from Selection](fixture-manager.md#create-groups-from-selection).

### Effects

- **Matrix effects for pixel boxes.** Fourteen effects in a new **Matrix**
  tab — bars, chevrons, ripples, rain, a checkerboard, sparkle, radar, an
  iris, an equaliser, snakes, wipes, colour noise, a box-by-box chase and a
  strobe — drawn across a whole run of boxes as one picture or in each box,
  on the beat where it counts. Every one has a **white** setting for boxes
  with white pixels (the Warmdance XL-450's): the white part of the colour
  plus accents, follow brightness, accents only, RGB only or white only.
  A pixel box in a look with no group is now drawn on as its own grid.
  See [Matrix effects](effects.md#matrix-effects).

### Lighting Studio

- **Beams stop at risers and the floor.** A wash thrown across the stage —
  the TrussW bars over DanceHysteria's steps — ran straight through the
  risers and on below the stage floor in a side elevation. Now a beam stops
  at the first riser, step or platform it meets (its side as well as its
  top), washes the top and face it reaches, and nothing of it shows behind
  a riser or below the floor, in the Studio and the Rig Overview alike. See
  [Beams](lighting-studio.md#beams-footprint-and-coverage).
- **Curtains that open in the show.** A main drape or traveler can be
  **Driven by** a curtain fixture (**Add curtain fixture** in its editor):
  as the fixture's **Open** channel goes from 0 to 255 the drape opens, and
  **Fly** takes a main drape up and out. The plot and the Rig Overview follow
  it while it fades. Use it in scenes, chasers and the Show Tree like any
  fixture; the fade time is how long the move takes. The curtain fixture
  isn't a light, so it draws no power and doesn't show in Inventory. Patch it
  to a curtain controller or relay box to move the real one. See
  [Curtains that move](lighting-studio.md#curtains-that-move).

### Follow spot and aiming

- **Quick focus: align the rig with the controller.** Step through the moving
  heads with the bumpers — each aimed at its reference (the scene's aim, a
  target, a line target's own point), lit and ringed in the views — nudge
  its calibration offsets with the sticks or D-pad until the light lands,
  and save with A (B reverts, X zeroes, Y checks a second reference). The
  keyboard does it all too. From the Lighting Studio, the Rig Overview,
  View → Quick Focus… (⌘⌥F) or View + Menu on the controller. See
  [Quick focus](lighting-studio.md#quick-focus).
- **Calibration offsets stay out of the picture.** A head's pan/tilt
  offsets are in its DMX but no longer in the views: its beam is drawn on
  the target it is aimed at. See
  [Calibration offsets](lighting-studio.md#calibration-offsets).
- **Line of sight: lights go out when they can't see the target.** A follow
  spot's new **Fade When Blocked** fades a head while a closed drape, a
  wall, a riser or a tower stands between it and the subject, and brings it
  back when the way is clear; an Aim look's **Dim lights that can't see the
  target** does the same for any aimed look. Both are off unless you turn
  them on. The Targets panel names what's in the way, the plot draws a
  blocked aim line red to its blocker, and **Show → Line of sight** picks
  what counts and can stop the 3D views' beams at it. See
  [Line of sight](stage-targets.md#line-of-sight).

### Programming

- **Palette folders hold any type.** A song's folder can hold its colours,
  aims and effects together; the Palettes tab shows folders first, or
  **Group by type** (right-click) for type first with the same folders under
  each. Older shows' per-type folders merge on open. See
  [Filing into folders](build.md#filing-into-folders).

### Outputs

- **Only transmit on the rig network.** A show can name the networks its
  output belongs on (**Settings → Network**, for example 172.18.0.0/20,
  saved with the show). Art-Net, sACN, OSC and WLED output on any other
  network — a laptop at school — is held, kept and sent nothing, and
  resumes by itself on the rig network. The footer says it in red: "Output
  held — not on the rig network (172.18.0.0/20). Connected: en0
  10.4.12.7"; click it for the setting. **Transmit anyway, this session…**
  asks first and is never saved. See
  [Only transmit on the rig network](other-improvements.md#only-transmit-on-the-rig-network).
- **Backup WLED lanterns.** A WLED universe can drive several devices: a
  backup lantern mirrors the primary, nothing programmed twice. Right-click
  the universe → **WLED Devices / Backup Lanterns…** → **Add Backup**. Each
  device is sent to on its own, so a dead one never holds up the others,
  and one that comes back is sent the look again. Connections says "1 of 2
  answering" while one is down. See
  [Backup lanterns](other-improvements.md#backup-lanterns).

## 0.1.0 (October 2026)

### Programming

- **Drag scenes and palettes into folders.** In the Programming tab, a scene
  (or several, or a whole folder) dragged onto a folder moves into it, and
  onto the type's heading or empty space moves back out; pressing on a row
  and moving now always drags it, where before it could rubber-band select
  instead. Right-click a folder to rename or delete it — deleting keeps what
  was in it. See [Filing into folders](build.md#filing-into-folders).
- **Take things out of a collection, chaser or show from the side tree.**
  Right-click rows listed under one → **Remove from “…”** (the functions
  stay). Delete on those rows now reads **Delete Function Everywhere…** and
  says where else the function is used. See
  [Taking something out](build.md#taking-something-out-of-a-collection-chaser-or-show).
- **Deleting says where it's used.** Deleting a palette or a function in the
  Programming tab (menu or ⌫) lists every scene look, effect-look slot,
  collection, chaser step and show track using it before you confirm. See
  [Deleting palettes and functions](build.md#deleting-palettes-and-functions).
- **A look keeps its Speed and Spread when you try another effect.**
  Clicking or double-clicking another effect's tile in the look editor put
  the look back on *As the look* with no spread; now both stay. See
  [Speed masters](effects.md#speed-masters). Closing the app with a speed
  master's name still being typed no longer crashes.

### Fixture definitions

- **Sizes and weights in your own units.** **Settings → Units** picks mm,
  cm, m, inches or feet (weight in kg or lb to match); the fixture
  editor's Physical tab shows width, height, depth and weight in it, and
  takes any unit typed with the number — `7.5"`, `6' 3"`, `19.05 cm`,
  `26.5 lbs`. Files still keep millimetres and kilograms. See
  [The fixture editor](fixture-manager.md#the-fixture-editor).
- **The Studio's grid follows it too** — rulers, readouts, labels and
  length fields in metres (mm/cm/m) or feet (in/ft); the grid's own
  Meters/Feet choice is gone. Shows open in your units and are saved in
  them; if you'd never chosen, the first show you open in feet makes it
  feet. See [Selecting and undo on the plot](lighting-studio.md#selecting-and-undo-on-the-plot).

### Sub-cues

- **Cues inside a cue.** A chase used as a step of a cue list lists its
  steps under it as **4.1, 4.2…**. Set the step's **Sub-cues** to **Sub
  GO** and they wait for **Sub GO** (⌥⌘↩; Sub Back ⌥⌘⇧↩) while GO still
  goes on to cue 5. The chaser editor and the Virtual Console cue list show
  them nested, the cue list highlights the one playing and has Sub GO /
  Sub Back buttons, and the footer says **Sub: 4.2 · Next: 5**. Older
  shows play as before. See [Sub-cues](show-control.md#sub-cues).

### Outputs

- **Patch a universe from the Fixture Manager.** Right-click a universe row
  in the fixtures list: **Patch output…**, or on a patched one the menu says
  where it goes and offers **Change interface / target…** and **Unpatch
  output**; with several selected, **Patch N selected universes to…** /
  **Unpatch N selected universes**, in the order you clicked them. The same
  questions and undo as Connections. See
  [Patching a universe's output](fixture-manager.md#patching-a-universes-output).
- **Patch or unpatch several universes at once.** In Connections, select
  universes and right-click: **Patch N selected universes to…** puts them on
  a device's ports in the order you clicked them (more universes than ports:
  the rest are named and left alone), and **Unpatch N selected universes**
  removes them all in one go — each one undo step. See
  [Connections &amp; patch](other-improvements.md#connections--patch).
- **Pick the port from a list.** Patching a universe to Art-Net (or WLED)
  lists every port of every device on that interface, free ones first and
  held ones saying by whom; a held port is refused. **Change interface /
  target…** asks the interface again, so a universe on the wrong network
  card can move without unpatching. "Port" is now the node's socket and
  "universe" its Art-Net address. See
  [Connections &amp; patch](other-improvements.md#connections--patch).
- **The power estimate is believable.** A fixture whose definition gives no
  power is guessed at 150 W for a mover and 50 W for anything else (it was
  40 W per channel, so a pixel bar read kilowatts), and hovering the footer
  figure explains it: the biggest draws, each measured, rated or guessed.
  See [Power distribution](other-improvements.md#power-distribution).

- **New Art-Net nodes can be found and given an address.** A node still on
  its factory address (2.x.x.x and the like) shows in Connections in amber
  with "set its IP"; **Rescan** now also polls each interface's limited
  broadcast so such a node can hear it; and right-click → **Set node IP
  address…** sets its IP, mask, gateway or DHCP (ArtIpProg), after a
  confirmation. A node no network here can reach gets the command for a
  temporary address instead. See
  [Setting up a new Art-Net node](other-improvements.md#setting-up-a-new-art-net-node).
- **A node's DHCP state reads right.** Connections had "DHCP capable" from
  the bit that says the node is *using* DHCP.
- **Art-Net and E1.31 say when an output is in trouble** — in the footer
  and in red on the universe's row in Connections: an address that can't
  be reached, an Art-Net node that stopped answering, a node port reporting
  a DMX short or no longer outputting. Judged with a delay each way, so they
  don't flicker, and they clear by themselves. See
  [When an output is in trouble](other-improvements.md#when-an-output-is-in-trouble).
- **The log no longer flaps** "is failing" / "recovered" for a node that
  isn't there: one line when it starts, one when it has really recovered.
- **A node port's DMX short shows as a short.** Connections had the
  "short" and "merging" flags of a node's ports the wrong way round.

### Shows that travel

- **A .qlcc carries its own definitions and scripts.** Saving embeds every
  fixture definition, effect script and RGB script the show uses that isn't
  part of the app, so it opens complete on another computer — no more red
  stand-ins for your own fixtures. A .qxw is saved as before.
- **Differences warn; the show keeps its own.** Where this computer's copy
  differs, the show's is used and the footer says so; **Review…** shows
  which looks newer and whether switching is safe for the modes and
  settings the show uses, with **Use This Computer's**, **Keep the Show's**
  and **Install the Show's**. Ones only the show has are a footer note with
  **Install on This Computer**. Save never swaps versions behind your back.
- **File → Collect for Transfer…** writes the show and those files into one
  folder; a show's folder's Fixtures/, EffectScripts/ and RGBScripts/ are
  searched when it opens.
- On **Windows and Linux**, your own effect scripts, LED maps, presets,
  bundles, gobos and network gear are now found: they live in the same user
  folder as your fixture definitions (they were looked for at the Mac's
  path).
- See [Opening a show on another computer](fixture-manager.md#opening-a-show-on-another-computer).

### Show control

- **QLab can fire the lights' cues.** A chase's steps have **cue numbers**
  (a Cue column in the chaser editor; a step without one answers to its
  position), a chase can be the **main cue list** or have a list number,
  and **MIDI Show Control** (on a patched MIDI input) or **OSC** (on a port
  of its own) can GO, LOAD, STOP, PAUSE, RESUME, RESET or ALL OFF them.
  See [Show control (QLab)](show-control.md).
- **Connections → Show control &amp; sync**: OSC in, MIDI Show Control in
  and MIDI timecode in one place, each live — listening or why not, the
  last command and when, the timecode's source and sync health. It replaces
  the Settings dialog's Show control group (which now points there); the
  settings carry over. MSC can be kept to **one MIDI port** (by name, for
  every show) and the **timecode source is saved with the show**.
- **Next:** in the footer says what the main cue list's GO plays next; GO
  (⌘↩) and Back (⌘⇧↩) work from any window, and the Virtual Console cue
  list shows each cue's number and what's next.
- **Skip a step** without deleting it: untick it in the chaser editor (or
  right-click → Skip); GO, Next, Back and auto-follow pass over it.

### Help

- **In-app help.** **F1** opens the guide for whatever is in front — the
  tab, the Rig Overview, Fixture Properties, the Patch dialog, the look
  editor, the power dialogs. **Help** lists the guides: Getting Started,
  What's New, Keyboard and Mouse, Effects, Troubleshooting, and all of
  them; search finds a word in every guide. The stock QLC+ reference is
  still there, online, as **QLC+ Reference** (⇧F1).
- New guides: [Shows (the timeline)](shows.md) — what tracks, items,
  cues and the show end are; [Getting started](getting-started.md) (starting the app and
  the ways a show comes together), [Keyboard and mouse](shortcuts.md),
  [Effects](effects.md) and [Troubleshooting](troubleshooting.md).
- **Pictures with numbered marks** on the parts each guide talks about:
  the main window and the footer ([Getting started](getting-started.md#finding-your-way-around)),
  the [Build tab](build.md#layout), the effect look editor
  and the [effect browser](build.md#the-effect-browser), the
  [stack checker](effects.md#the-stack-checker),
  [Connections by universe](other-improvements.md#connections--patch) and the
  [Patch dialog](fixture-manager.md#patching-the-patch-dialog).

### Lighting Studio

- **A closed curtain hides what is behind it.** Lit steps just upstage of
  the closed main drape glowed through it in the Rig Overview — their haze
  glow, and the pools their light made on the back of the cloth. Now the
  drape (or a wall, or a platform) hides them, and the glow shows only
  where the curtain is open. See [Haze](lighting-studio.md#haze).
- **A mover hung on a truss sits where the truss editor shows it** in the
  Rig Overview and the Angled view — just under the bar — instead of half
  a body lower.
- **Washes look like washes.** A pool now fades out past its beam angle
  for a wash (pars, pixel bars, wash movers) and stays crisp for a profile
  (a gobo, focus or iris channel, or a beam bar); PC and Fresnel lenses
  are softest. See
  [Washes and profiles](lighting-studio.md#washes-and-profiles-the-edge-of-a-pool).
- **A lit rig draws about fifteen times faster**: the decks are worked
  out once a frame, where each ray lands is kept while the rig stays put,
  and a pool split across a deck's edge is drawn only as finely as it
  shows (a 208-fixture show fully lit: from about a second a frame to 70
  ms).
- **The Rig Overview draws on the graphics card** (Metal on a Mac,
  OpenGL elsewhere), and works out its fixtures on every core at once:
  the same picture, and the lit 208-fixture show from about 70 ms a frame
  to 15 (from a stutter to smooth). The **GPU** box in its toolbar turns it off
  if a driver draws it wrongly. See
  [Angled — the free 3D view](lighting-studio.md#angled--the-free-3d-view).
- **Show or hide layers from the Rig Overview** — its **Plot layers** menu, the
  same switch as the Studio's eye. The eye and the labels toggle now work in
  Operate mode as well.
- **Pools show on decks and steps.** A beam landing on a platform was drawn
  only where it reached the bare floor; the part on the deck was lost.

- **The Rig Overview stands on centre stage** — on the floor, in the
  middle of the stage grid — and turns about it, instead of centring
  whatever the rig's bounding box was (which floated the floor half a
  truss-height below the middle).
- **The Lighting Studio and the Rig Overview show a running position effect on an Aim look.** A head
  under a look with an Aim target was always drawn pointing at the target —
  so Stage Shape, or an orbit round a target, never showed. Selected, a
  look still shows where its target is; running, the heads are drawn where
  they're actually sent.
- **A drag only moves; Shift-drop attaches.** Dropping a fixture over a
  truss, pipe or deck no longer hangs it there unless you hold **Shift** as
  you let go; the outline turns green while it would, and updates the
  moment Shift is pressed or released. Coming off still needs no Shift, and
  fixtures dragged in from the Inventory are placed where they land.
  ([Mounting](lighting-studio.md#mounting-a-fixture))
- **Height counts when attaching.** A fixture mounted on something (the
  foot of a tower, a deck) is only offered a truss or pipe within half a
  metre of its own height — no more lights at a tower's base hung on the
  bar on top.
- **Mount on → Deck.** Stands every selected fixture on the deck under it
  (or a named one): on its top, upright, nothing floating, nudged inside a
  deck it sat on the edge of.
- **Fixtures under structures** draw faded with a dashed outline in the
  Top view, still clickable.
- **Rulers mark the middle** of whatever you drag — a drape's centre line,
  not where you grabbed it — and band any selection, not only fixtures.
- **Orientation for anything that pans or tilts**, even bar-shaped (a
  two-arm beam bar standing on a step); **Test orientation** names its
  centred pose for it — straight up, down or out.
- **Lanterns on hangers hang straight.** A face left over from an earlier
  mount drew some on their side; it's cleared when the show loads.
- **Rig Overview** (⇧⌘R) **follows the Studio's layers** — hide a layer,
  it leaves the overview — and the same shortcut, or ⌘W, closes it.
- **Faster drawing of the rig**: a 200-fixture show draws in about 9 ms a frame (it
  was 51).

- **A truss between two stands** — the truss editor's **Other end on
  stand**: it runs from one stand's top to the other's and follows them.
- **Hangers on the 2D plot** — arm and hook, double-click to edit.
- **A patch can be undone** — the Patch dialog's Apply is one ⌘Z, on the
  Fixtures tab or in the Studio.
- A **house electric** (horizontal pipe) lists its fixtures by distance
  **along** it, up to its length, rather than a boom's height.
- The **Network Topology** dialog works like every rig dialog: the plot
  follows as you edit, Cancel puts the network back, OK is one undo.
- The Virtual Console's **button matrix** and **animation** keys moved to
  ⇧⌘G and ⇧⌘N, off the app's Lighting Studio and Rig Overview keys.

### Patching

- **Patching fits fixtures into what's free, in one order.** The Patch
  dialog's Place now defaults to **into the free space**: the fixtures are
  addressed one after another around whatever else is in the universe —
  no more conflicts from moving a patched fixture's block onto its
  neighbours — in one **addressing order** you choose: by name, movers
  first then by kind, or as they hang. The same group always lands the
  same way. Any
  with no room are flagged and left where they were, with **Carry on into
  the next universe** or **Fit by changing modes** to finish the patch.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))
- **Change a mode in the Patch dialog**, patched fixture or not; Apply
  changes it, and ⌘Z puts it back. A different head count is allowed
  unless a fixture group lays out heads the new mode doesn't have.

- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, to save as **CSV** or a printable **PDF** for the crew.
  **Fixtures → Patch List…**, or the Studio's **More** menu.
  ([The patch list](fixture-manager.md#the-patch-list))
- **A label per fixture** from the patch list: **Labels…** saves a PDF —
  the name and the address (U3 · 101) large, the universe's name, the mode
  and channels, a network fixture's name and where it hangs small — on Avery 5160 sheets (30 to a Letter page) or a 62 mm
  roll, for all the fixtures or the selected rows.
  ([Labels](fixture-manager.md#labels))
- **Patch Others Like This.** Right-click a structure: the other
  structures of its kind with the same fixtures are patched as it is — each
  fixture at the address of its counterpart, in its own structure's
  universe. The Patch dialog offers the same **Match** whenever its fixtures
  span several structures.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))

### Programming

- **160 WLED effects run on any fixture** — every 1D effect of WLED's: Android, Aurora, Blends, Blink, Blink Rainbow, Bouncing Balls, Bpm, Breathe, Candle, Candle Multi, Chase, Chase 2, Chase 3, Chase Flash, Chase Flash Rnd, Chase Rainbow, Chase Random, Chunchun, Color Clouds, Colorful, Colorloop, Colortwinkles, Colorwaves, Dancing Shadows, Dissolve, Dissolve Rnd, Drip, Dynamic, Dynamic Smooth, Fade, Fairy, Fairytwinkle, Fill Noise, Fire 2012, Fire Flicker, Fireworks, Fireworks 1D, Fireworks Starburst, Flow, Flow Stripe, Glitter, Gradient, Halloween Eyes, Heartbeat, Hyper Sparkle, ICU, Juggle, Lake, Lighthouse, Lightning, Loading, Meteor, Meteor Smooth, Multi Comet, Noise 1, Noise 2, Noise 3, Noise 4, Noise Pal, Oscillate, Pacifica, PacMan, Palette, Percent, Perlin Move, Phased, Phased Noise, Plasma, Police, Popcorn, Pride 2015, PS 1D Balance, PS Chase, PS Dancing Shadows, PS DripDrop, PS Fire 1D, PS Fireworks 1D, PS Hourglass, PS Pinball, PS Sparkler, PS Spray 1D, PS Springy, PS Starburst, Railway, Rain, Rainbow, Rainbow Runner, Random Colors, Ripple, Ripple Rainbow, Rolling Balls, Running, Running Dual, Saw, Scan, Scan Dual, Scanner, Scanner Dual, Shimmer, Sine, Sinelon, Sinelon Dual, Sinelon Rainbow, Solid, Solid Glitter, Solid Pattern, Solid Pattern Tri, Sparkle, Sparkle Dark, Sparkle+, Spots, Spots Fade, Stream, Stream 2, Strobe, Strobe Mega, Strobe Rainbow, Sunrise, Sweep, Sweep Random, Tetrix, Theater, Theater Rainbow, Traffic Light, Tri Fade, Tri Wipe, TV Simulator, Twinkle, Twinklecat, Twinklefox, Twinkleup, Two Areas, Two Dots, Washing Machine, Wavesins, Wipe, Wipe Random — and the sound-reactive DJ Light, Freqmap, Freqpixels, Freqwave, Gravcenter, Gravcentric, Gravfreq, Gravimeter, Juggles, Matripix, Midnoise, Noisefire, Noisemeter, Noisemove, Plasmoid, PS GEQ 1D, PS Sonic Boom, PS Sonic Stream, Puddlepeak, Puddles, Ripple Peak, Rocktaves, Waterfall, which follow the audio input (Inputs/Outputs) as the device follows its microphone — so a look with one runs it on its WLED
  devices and its other fixtures alike.
- **Effect previews show what the effect does.** A position effect draws
  the shape its heads trace, with the heads along it, instead of a strip
  of lights; gobo, iris and zoom effects show a row of beams; amber and UV
  pulses light in their own colours. ([The effect browser](build.md#the-effect-browser))
- **WLED effects only where they can run.** A WLED effect that only runs on
  a WLED device is offered only when the look's targets include one;
  without, the WLED tab lists the ones that run on any fixture.
- **A Programming toolbar that fits.** Highlight, Flash, Tap and Save
  Positions stay buttons; Park, Move in black, Beat (it reads the tempo)
  and More are drop-downs — one row, no scrolling.
- **The effect browser is a window of its own.** Once an effect is picked,
  its settings have the look editor's full width; **Change effect…** opens
  the browser, where every pick (and every arrow-key step) plays at once,
  and **Cancel** or Esc puts back the effect you had.
  ([The effect browser](build.md#the-effect-browser))

### Playback

- **GO on the beat.** A chase or cue list can hold a GO to the next beat
  or bar — of its speed master, or the main beat — so a cue pressed a
  little early lands on the music. The Next button shows a GO waiting.
  ([GO on the beat](getting-started.md#go-on-the-beat))

### Programming tab

- **The Looks list checks the stack** — a look the one below always covers,
  an effect another covers, a dimmer only some heads get, an effect for
  fixtures the look hasn't… are marked in the list with why, and counted
  under it. ([The stack checker](effects.md#the-stack-checker))

- **The handles move the edges you drag** — no pane snaps shut, and the
  canvas can be narrowed (its fades-and-transitions line wraps; it was 1300
  px at its narrowest, so a handle squeezed the panes beside it instead).
  A new handle sits **between the looks and the Look Editor**, which
  scrolls when made short. Positions are remembered.
  ([Layout](build.md#layout))

### Effects

- **A look stacks the same way every time** — top to bottom, the lower
  wins; colours are colour 1, 2, 3; an effect's colour replaces the base
  instead of mixing with it; dimmer effects scale and multiply; a Colour or
  Dimmer under an effect is that effect's only, and the ones above are the
  look's own, for every effect. Fixed along the way: two static looks on a
  channel kept the *higher* value (Blue then Red came out magenta-ish, two
  Pan/Tilts kept the bigger pan); a second Colour look moved a colour
  wheel; an Aim or a Dimmer placed after a movement effect did nothing;
  a Dimmer look didn't dim a fixture with no dimmer; two effects on one
  channel went by which started first, not their order in the list.
  ([How a look stacks](effects.md#how-a-look-stacks))

- **Breathe and the other dimmer effects work on movers with a head
  dimmer** (no master dimmer in their definition), and a colour-wheel
  fixture's wheel holds its colour while they breathe instead of hopping
  slots.
- **Movement effects run round the look's aim.** With an Aim or Pan/Tilt
  look in it, a ballyhoo, orbit, figure 8 and the rest move over that point
  instead of the middle of the heads' range (straight down, on a hung
  mover). ([Position](effects.md#position-moving-heads))

- **How the footer shows a master's beat** — a blink, a bar across each
  beat, or nothing; always, or only while something on that master plays
  (**View → Speed Master Beat in Footer**).
- **Speed masters in the footer** — a button for each master something is
  on (an unused one neither shows nor flashes, there or in its window),
  reading its tempo and lit on its beat, with a green level bar while its
  rate is moved; a click opens the Speed Masters window at it
  (**View → Show Speed Masters in Footer**). Each master's **Inputs…**
  maps its Tap, Rate and Pause to a MIDI controller.
  ([Speed masters](effects.md#speed-masters))
- **Effects layer as on a desk.** A colour effect replaces the look's
  colour on its heads instead of adding to it (Cyan under a rainbow washed
  every hue pale); a dimmer effect dims what's there, the colour effect's
  included. ([Effects](effects.md))
- **Rainbow with a colour in the look sweeps that colour** — a wave of
  Cyan across the heads, no rainbow; with two or more, it sweeps through
  them.
- **A rainbow on a wash with no dimmer is a rainbow.** On an RGBW head with
  no master dimmer (a Cobra Wash in 13-channel mode, say) an effect's own
  dimmer put red, green, blue and white all at full under its colour — a
  rainbow came out white.
- **Spread.** One Spread row for every chasing effect — amount in cycles,
  in look order or across columns, rows or the stage, **out from or round
  the look's target**, forward, reverse, centre out or ends in, with wings
  and blocks — and a preview of the heads coloured by phase. Every
  movement effect takes it too (orbit, figure 8, ballyhoo, wave, sweep,
  wander, fan, spotlight chase…), and **Stage Shape** uses it for which
  head goes where on the line or shape.
  ([Spread](effects.md#spread))
- **Speed masters.** 4 to 16 tempo masters (Programming toolbar →
  **Beat → Speed masters…**), one of which can follow the **show timeline**
  (and so timecode): each its own tempo, tap, rate and pause, or following the
  main clock or another master at ×¼ … ×4. Put an effect, a look, a chase
  step, a chase or a collection on one; on a master an effect's rate counts
  per beat; a chase counting in beats steps on its master's beats.
  ([Speed masters](effects.md#speed-masters))
- **Effects keep the beat together.** Every effect that follows the beat
  reads one beat clock — two started at different moments used to stay
  that far apart for good. **Tap** now averages your last four taps and
  puts the beat on the last one, pulling every beat effect onto it.
  ([On the beat](effects.md#on-the-beat))
- **Text** — words on a pixel grid, scrolling or still in the middle, in
  the look's colours; settings that are words get a text box.
  ([Text](build.md#text))
- **Step Test Pattern** — a diagnostic card for LED-strip steps: strips
  counted, steps numbered and named, a spinner that stops if output
  freezes. ([Step Test Pattern](build.md#step-test-pattern))
- The RGB Matrix **Text** algorithm has a **Centered** style.
- **Much faster on big pixel rigs**: 96 strips × 64 pixels ticks in about
  8.5 ms (was 68), leaving the screen free to keep up.
- The **effect list sits beside its settings** in the look editor, so both
  fit on an ordinary screen.

### Views and windows

- **Connections → By universe lists the universes in use** — fixtures
  patched in, an output or an input — and says how many it hides; **Show
  unused universes** shows them all.

- **Connections holds still.** The list refreshes every few seconds to
  catch nodes coming and going; it no longer drops the universe you picked
  or scrolls back to the top when it does, and it waits while a right-click
  menu or dialog is open, so **Patch…** acts on the row you chose.
- **Right-click a tab → Open in Its Own Window.** Works on the first
  click, even when the app isn't in front (a Mac spends that click
  bringing the window forward, so a Shift-click there did nothing).
- **Shift-click a tab** to put it in a window of its own — a double-click
  no longer does it by accident. **Window → Move … to Its Own Window** does
  the same for the view in front, and the Window menu can turn the
  Shift-click off. ([Window menu](other-improvements.md#window-menu))
- **Functions has no tab** — **Window → Functions** (⇧⌘4) opens it
  (since 0.3.0, the stock Functions tab). ([Window menu](other-improvements.md#window-menu))
- The **Window** menu lists every view, wherever it is, and puts
  broken-out ones back.
- **Connections**' Devices header is one row; the list gets the height.

### Earlier this month

- **Power**: a fixture's **maximum draw** — amps, proportional, measured
  state by state, set on many at once and copied to the same fixtures —
  holds a hungry fixture under its circuit.
  ([Maximum draw](other-improvements.md#a-fixtures-maximum-draw))
- **Connections**: only a universe's name is edited in place; select a
  universe to see what it's sending, live.
- A look on a fixture **group** sets only that group's own heads.
- A function deleted while running lets go of its channels.
- **Scenery** (cyc, walls, drapes with an opening, a proscenium, a rounded
  front), **fountains** (with a silk-flame fixture and a labelled diagram),
  **lasers and star projectors**, a beam split across the surfaces it
  crosses, group rotation, and count badges on towers.
  ([Lighting Studio](lighting-studio.md))
