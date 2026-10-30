# Coming from QLC+ 4

For someone who already runs shows on QLC+ 4.x and wants to know what
moving to qlcconsole involves: what stays, what's better, how to move a
show over, how the old ways of working map to the new ones, and how to
go back.

qlcconsole is built on QLC+ 4.14.4 (Help ▸ About says
`4.14.4+qlcconsole.N`). QLC+ 4.x is in maintenance upstream; qlcconsole
keeps its desktop UI going. If you use QLC+ 5 and it works for you, keep
using it.

## What stays the same

| | In qlcconsole |
|---|---|
| **The engine** | QLC+ 4.14.4's: the same functions (scenes, chasers, sequences, collections, EFX, RGB matrices, shows, scripts, audio, video), the same timing and the same DMX output |
| **Show files** | A **.qxw** opens as it is. Nothing is converted on load, and **Save** writes it back as a .qxw ([Show files](getting-started.md#show-files)) |
| **Fixture definitions** | The QLC+ library (with QLC+ 4.14.4's new and updated ones), your own definitions, and the fixture editor |
| **Plugins** | QLC+ 4's input and output plugins (Art-Net, sACN, DMX USB, MIDI, OSC, HID and the rest), plus a WLED output |
| **Virtual Console** | Every widget, external input on MIDI and OSC, Design and Operate, kiosk mode (**-k**) |
| **Simple Desk, Show Manager** | Simple Desk as it was; Show Manager is the **Shows** tab, with a few additions ([Shows](shows.md)) |
| **Web access** | The same pages, started with **-w** (but see [What's different](#whats-different-or-not-here)) |
| **The QLC+ tabs** | The **Classic QLC+** view: Fixtures, Functions, Shows, Virtual Console, Simple Desk, Inputs/Outputs, in QLC+'s order, names and toolbars |

![The Classic QLC+ view](../images/views-classic.png)

1. QLC+'s main toolbar: New, Open, Save, Save As, the DMX Monitor, the
   Rig Overview, the address tool, Full Screen, help, dump and live edit.
2. The tab's own row of buttons, as in QLC+ (here the Fixtures tab's).
3. QLC+'s tabs, in QLC+'s order and names.
4. The footer: Ready, the Grand Master and the mode.

A view only shows and hides tabs; the show is the same in each. See
[Views](getting-started.md#views).

## What you'll notice that's better

All of it is optional. A show built the QLC+ way keeps working without
any of it.

| | What it does for you | Guide |
|---|---|---|
| **Show Tree** | The show written as cue lists, cues and layers, with tracking (a cue carries on what it doesn't change), Block / Cue-only, cue numbers, move in black, sub-cues and hits for busking. The scenes and chaser behind it are made for you | [Show Tree](show-tree.md) |
| **Run tab** | A full-screen cue runner: GO, Back, Sub GO, STANDBY, what the next cue changes; the same runner as a VC widget (**Cue Runner**). GO is ⌘↩ from any window | [The cue runner](show-tree.md#the-cue-runner) |
| **Build and the Workbench** | Scenes made of looks (palettes on fixture groups) instead of raw channel values; a collection or chaser edited as layers, with a live preview and one undo history | [Build](build.md) |
| **Palettes and effects** | Reusable colour, dimmer, position, beam, gobo and effect palettes; an effect designer and a library of built-in effects that take the look's colours | [Effects](effects.md) |
| **Lighting Studio** | The plot: trusses, towers, risers and scenery, fixtures hung on them, the network and cable runs, a 3D **Rig Overview** (⇧⌘R) lit by the show | [Lighting Studio](lighting-studio.md) |
| **Stage targets** | Marks on stage that moving heads aim at, solved from where each fixture hangs | [Stage targets](stage-targets.md) |
| **Focus tools** | Quick focus and Venue focus from the stage with an Xbox or PlayStation controller, a spoken voice telling the operator which light they have, Focus this cue | [Venue focus](focus-calls.md) · [Focus this cue](refine.md) |
| **Venues** | One show, many rooms: per-venue focus corrections, house rig, patch and outputs | [House rig and tour package](venues.md) |
| **Fixtures** | Group folders, a head-layout grid editor, an inventory of fixtures not yet patched, a Patch dialog, a patch list with labels, Definition Updates | [Fixture Manager](fixture-manager.md) · [Definition updates](definition-updates.md) |
| **Connections** | QLC+'s Inputs/Outputs page plus a Devices view that follows the wiring (protocol, interface, nodes, universes) and controllers picked up again after sleep | [Connections and patch](other-improvements.md#connections--patch) |
| **Show control** | QLab (or anything speaking OSC or MIDI Show Control) fires cues by number | [Show control](show-control.md) |
| **The footer** | Ready / Not ready, with each problem and its fix; the Grand Master, speed masters, what GO plays next | [Readiness footer](other-improvements.md#readiness-footer) |
| **Files** | Autosave and recovery; **.qlcc** shows carry their own fixture definitions and scripts; safe saving in cloud folders; **Collect for Transfer** | [Show files](getting-started.md#show-files) |
| **Sturdier** | Fuzzing (show files, fixture definitions, input profiles, Art-Net and RDM packets) and ASan, UBSan and ThreadSanitizer runs, with what they found fixed; malformed network packets dropped; a damaged show says so and is never saved over; a security review of web access and scripts | [Security](troubleshooting.md#security) |
| **Measured speed** | How much one computer drives, measured: about 80 universes on a Raspberry Pi 5, 256 on a Linux i7, 768 on an Apple M4 | [Choosing a computer](getting-started.md#choosing-a-computer) |
| **Packaging** | Qt 6; a signed and notarized universal Mac app; Windows installer and Linux AppImage; updates that install at quit, never during a show | [Software updates](updates.md) |

## Moving over, step by step

### 1. Install it beside QLC+ 4

qlcconsole is a separate app with its own name, settings and user
folder, so QLC+ 4 stays installed and untouched. Downloads are on
[GitHub Releases](https://github.com/sandinak/qlcconsole/releases).

At every start it brings in what is in your QLC+ user folder that its
own doesn't have yet: fixture definitions, RGB scripts, input profiles,
MIDI and modifier templates and colour filters.

| | Your QLC+ folder | qlcconsole's | How |
|---|---|---|---|
| macOS | ~/Library/Application Support/QLC+ | ~/Library/Application Support/qlcconsole | links to the same files |
| Linux | ~/.qlcplus | ~/.qlcconsole | links to the same files |
| Windows | your user folder\QLC+ | your user folder\qlcconsole | copies |

A file that is already in qlcconsole's folder wins. One you remove from
qlcconsole's folder stays removed; the original in QLC+'s folder is left
alone. Your own game pad profiles live in qlcconsole's folder.

#### Your QLC+ 4 settings

QLC+ 4's settings are a different matter: qlcconsole asks first. The
first time it starts and finds them, it lists what it found and asks
once whether to bring them over:

- **recent shows**, added to **File ▸ Open Recent** after qlcconsole's
  own (shows that no longer exist are left out);
- **I/O and plugin preferences**: the default patch for a new show,
  hotplug, and each plugin's own settings (MIDI channel and mode, the
  Art-Net, sACN and OSC wait, DMX USB and uDMX settings);
- **interface preferences** that work the same here: the Virtual
  Console's default widget sizes, Simple Desk's pages, the slider style
  and similar.

Say no and it doesn't ask again; **Settings ▸ General ▸ QLC+ 4 ▸ Bring
over…** does it whenever you like, and shows what's still to bring.
QLC+ 4 itself is never changed, and a setting qlcconsole already has is
kept. Window sizes and layouts aren't copied: the windows differ. I/O
and plugin preferences take effect at the next start.

### 2. Open a show

**File ▸ Open** a .qxw. It opens in the **Classic QLC+** view, as it
was: nothing is converted, it plays as before, and **Save** writes the
.qxw. Two things to know:

- **.qxw or .qlcc.** **.qlcc** is qlcconsole's own format: the same show
  plus copies of every fixture definition and script it uses that isn't
  shipped with the app, so it opens complete on another computer. Keep a
  .qxw while you still go back and forth with QLC+ 4; **Save As** offers
  both (**QLC+ 4 Workspaces** is the .qxw).
- **What QLC+ 4 can't read.** Once you use qlcconsole's own features, a
  .qxw holds parts QLC+ 4 doesn't know. Saving says which, once for that
  file, after the save, with how many of each and **Save as .qlcc…** to
  keep a full copy (or **Don't ask again for this show**). See
  [Saving for QLC+ 4](getting-started.md#saving-for-qlc-4).

To start every new show in Classic too: **Settings ▸ General ▸ Start new
shows in the Classic view**.

### 3. Try the new parts when you want them

- **View ▸ Workspace ▸ qlcconsole** shows the new tabs (Lighting Studio,
  Build, Show Tree, Run); **Everything** shows all of them. Switching
  changes nothing in the show.
- **File ▸ Upgrade This Show…** turns the chasers you choose into Show
  Tree cue lists. It shows the plan first, checks that each cue puts out
  what its step did (**Plays the same**), moves what fired the chaser
  (VC buttons, cue lists, the main cue list) to the new list, and keeps
  the original for **Restore original…**. One undo step. See
  [Upgrade this show](show-tree.md#upgrade-this-show).
- The course's Level 1 is a self-study path through the Show Tree, on a
  practice show that is never connected to lights:
  [the course](https://github.com/sandinak/qlcconsole/blob/main/docs/course/README.md)
  (on GitHub).

### 4. Going back to QLC+ 4

Save as a .qxw (**Save As ▸ QLC+ 4 Workspaces**) and open it in QLC+ 4.
Every build's tests open a .qxw saved here with QLC+ 4.14.4's own engine
and check that the fixtures, groups, channel groups, palettes and
functions are all there.

| QLC+ 4 keeps | QLC+ 4 skips, and drops if it saves the file |
|---|---|
| Fixtures and the patch, fixture groups, channel groups, palettes, every function (scenes, chasers, collections, EFX, RGB matrices, sequences, shows, scripts), the Virtual Console | The Show Tree (cue lists, cues, hits), speed masters, the power plan, the network map, the timecode source, parked channels, focus calls, the Lighting Studio's rigging, the Cue Runner and Show Control widgets, a venue's house fixtures, fixtures in the inventory |

A Show Tree cue list is saved as a chaser with the collections and
scenes it plays, and QLC+ 4 loads those, but what only qlcconsole does
(effect palettes, aiming at stage targets, the cue list's own rules)
doesn't come with them, and the tests check that QLC+ 4 opens the file,
not how it plays there: try a show on the rig before relying on it in
QLC+ 4. [Detach to plain chaser](show-tree.md#detach-to-plain-chaser)
hands a cue list back as an ordinary chaser first, if you want to keep
working on it the QLC+ way. A fixture definition you added in qlcconsole
lives in qlcconsole's user folder: copy it to QLC+'s for QLC+ 4 to find
it.

## Old workflow, new workflow

Every left-hand column still works, in the Classic QLC+ view.

| In QLC+ 4 you… | In Classic you still can | The qlcconsole way, and why |
|---|---|---|
| make scenes in the **Function Manager** | **Functions** tab, the stock scene editor | **Build ▸ Scenes** and the Workbench: looks (palettes on groups), so a colour changed in a palette changes every scene that uses it ([Coming from the Functions window](build.md#coming-from-the-functions-window)) |
| set channels with the scene editor's sliders | Same | In the Workbench, click a group or fixture row for its channels; the [Look Editor](build.md#the-inline-look-editor) for colour, position, beam and gobo by name |
| build **collections** of scenes, EFX and RGB matrices | Same | Load the collection in the Workbench: its members are layers, **+ Scene** and **+ Effect ▾** add more, and it plays as you build ([The Workbench](build.md#the-workbench)) |
| run a **chaser as a cue list** from a VC Cue List | Same; tick **Main cue list** in the chaser editor and GO (⌘↩) and the **Run** tab play it too | **Upgrade This Show…** makes it a Show Tree cue list: tracking, cue numbers, sub-cues, notes, and the Run tab ([Show Tree](show-tree.md)) |
| set the rig, save a scene, chase the scenes | Same | Build keeps it: **Chaser from selection** ([Two QLC+ habits, kept](build.md#two-qlc-habits-kept)) |
| busk with VC buttons on scenes | Same | **Hits**: busking states on VC buttons, built like cues ([Hits](show-tree.md#hits-busking)) |
| make **EFX** and **RGB matrices** | Stock editors, in Functions or the Workbench | **Build ▸ Effects**: effect palettes that take the look's colours ([Effects](effects.md)) |
| run a **Show Manager** timeline | **Shows** tab | The same timeline, with track flags and timecode follow ([Shows](shows.md)); for cue-by-cue shows, the Show Tree; QLab can fire cues ([Show control](show-control.md)) |
| make **fixture groups** | Fixtures tab | Folders, the head-layout grid, groups laid out from where fixtures hang ([Fixture Manager](fixture-manager.md#fixture-group-folders)) |
| add and address fixtures | **Add fixture** in the Fixtures tab | The inventory and the Patch dialog: place first, address later ([Inventory](fixture-manager.md#inventory-design-the-rig-before-patching)) |
| patch universes in **Inputs/Outputs** | The same page (Connections ▸ Universes) | Connections ▸ Devices: what is wired to what ([Connections and patch](other-improvements.md#connections--patch)) |
| watch output in the **Monitor** | **View ▸ DMX Monitor**, or the toolbar | The Lighting Studio and the 3D Rig Overview ([Lighting Studio](lighting-studio.md)) |
| check channels on the **Simple Desk** | Same | Same |
| map MIDI or OSC controls to VC widgets | Same | Same; or learn a control straight onto a scene, effect, cue list, cue or hit with no VC widget ([Control Map](other-improvements.md#control-map-midi-and-osc-triggers)); game pads for focus ([The controller map](lighting-studio.md#the-controller-map)) |
| aim movers with pan and tilt | Same | Aim at [stage targets](stage-targets.md), or focus from the stage with a controller ([Quick focus](lighting-studio.md#quick-focus)) |
| copy the .qxw and your fixtures to another computer | Same | Save a **.qlcc**, or **Collect for Transfer** ([Opening a show on another computer](fixture-manager.md#opening-a-show-on-another-computer)) |

## What's different or not here

- **Names.** Inputs/Outputs is the **Connections** tab (Classic shows
  QLC+'s name). Build was called Programming before 0.3.0.
- **Show controls.** Stop all, Blackout and Design / Operate sit in the
  top bar (or the footer), and are in the **Control** menu too.
- **Shortcuts.** GO is ⌘↩, Stop all ⌘⇧Esc, Design / Operate ⌘F12; plain
  Esc never stops playback. The VC button matrix and animation keys are
  ⇧⌘G and ⇧⌘N, and a new VC slider is ⇧⌘U (⇧⌘S is Save As). See
  [Keyboard and mouse](shortcuts.md).
- **Web access is safer by default.** **-w** answers on this computer
  only, where QLC+ 4 answered on every network. Add **--web-bind** to
  reach it from a tablet, and **-wa** with accounts to let that tablet
  change anything ([Web access](other-improvements.md#web-access)).
- **Scripts don't run programs** unless **Settings ▸ Advanced ▸ Allow
  scripts to run programs** is on.
- **Autosave** writes a recovery copy beside the show every 5 minutes
  while there are unsaved changes.
- **Help.** **F1** opens these pages; QLC+'s own manual is online
  (**Help ▸ QLC+ Reference (online)**, ⇧F1), not bundled.
- **Not here:** QLC+ 4's experimental QML interface (the start of QLC+
  5); a packaged Raspberry Pi build (it builds from source); a
  code-signed Windows installer (Windows warns before installing). The
  new screens are in English; QLC+'s translations cover only its own
  parts.
- **Always open a show with -o** on the command line. A bare file name
  isn't opened, and saving then can write an empty show over it.

## Questions QLC+ users ask

Answers to questions that come up again and again on the QLC+ forum. The
features are QLC+ 4's own, and they work the same here.

### A fader that takes over channels, then lets go

A Virtual Console slider in **Level** mode sets channels directly. To have
it take over from whatever else is playing them, and then hand them back:

1. In the slider's properties, on **Level**, pick the channels and tick
   **Monitor the selected channels and update the slider level**.
2. The slider now follows what those channels are doing. Move it and it
   **overrides** them: its level wins over the cue list, chasers and other
   faders on those channels.
3. The **reset** button under the slider releases the override, and
   the channels go back to what the show is playing. The reset can have
   its own key or controller input.

Without Monitor, a slider in Level mode joins the mix: the highest level
wins for intensity, and the last one moved wins for everything else.

### ⌘D: save what's on stage as a scene

**Control ▸ Dump DMX values to a function** (⌘D; Ctrl+D on Windows and
Linux) records the DMX values going out now as a scene. It doesn't matter
what made them: the Simple Desk, a fader or a cue. In the dialog:

- **Dump all channels**, or only the channels you pick in the tree.
- **Dump only non-zero values** leaves out channels at 0.
- Give it a **name**, or pick an existing scene to **overwrite**.
- **Add to**: also put the new scene on a chaser, or on a new VC button
  or slider.

In the Show Tree, **Record as new cue** does the same into a cue list,
as palettes and layers rather than raw values. Dump is for when you've
set the stage by hand and want to keep it as it is.

### Flash that wins: Override priority and Force LTP

A VC button set to **Flash** plays its scene while it's held. Two options
in the button's properties (under Flash) decide how hard it plays:

- **Override priority**: the flash wins over sliders and running chasers
  on the same channels, rather than mixing with them. Use it for a
  blinder or a strobe hit that must show whatever else is running.
- **Force LTP**: the scene's intensity channels act as LTP. The flash sets
  its levels even when they're *lower* than what's playing, so a flash to
  50% really is 50%. Without it, the highest level wins and a lower flash
  disappears under a brighter cue.

When you let go, the channels go back to what the show is playing. These
options work on scenes. A Show Tree hit has its own Toggle and Flash, and
these two options don't apply to it (see
[Hits](show-tree.md#hits-busking)).

### Sending MIDI CC out

Two ways, depending on what you want to drive:

- **Values as CC** (a DAW's mixer, a video app): in **Connections**,
  patch a universe's **output** to the MIDI port. DMX channel 1 is CC 0,
  channel 2 is CC 1, and so on up to channel 128, with the value halved
  (0–255 becomes 0–127). The MIDI plugin's configuration sets the MIDI
  channel, and can send **Note** or **Program Change** instead of CC. Put
  the channels in a scene, or on a fader, and they go out as MIDI
  whenever they change.
- **Feedback to a controller's lights and motor faders**: give a VC
  widget an external input from the controller, and patch the same MIDI
  port as that universe's **feedback** output. The widget sends its state
  back on the same CC or note.

## Getting help

- **Help ▸ Troubleshooting ▸ Report a Bug…** opens a GitHub issue with
  the app's version and system details filled in (and nothing from your
  show but counts); see [Reporting a bug](troubleshooting.md#reporting-a-bug).
- **qlcconsole's issues** on
  [GitHub](https://github.com/sandinak/qlcconsole/issues), for anything
  in this app, its QLC+ 4 parts included. Not the upstream forum: its
  developers shouldn't have to answer for a fork.
- **What qlcconsole promises QLC+ 4 users** (your shows stay safe, the
  Classic view stays, definitions keep arriving, macOS, Windows and
  Linux builds) is in
  [SUPPORT.md](https://github.com/sandinak/qlcconsole/blob/main/SUPPORT.md).
- [Troubleshooting](troubleshooting.md) when something isn't doing what
  you expect.
