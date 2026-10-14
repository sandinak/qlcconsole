# Show control (QLab) — Design

Branch `feat/qlab-cue-control` (2026-10-13). QLab (or any show controller)
fires lighting cues by number, over MIDI Show Control or OSC. Sending back
to QLab comes in a later branch.

## Where things stand

- **No cue numbers.** A `ChaserStep` is known by its position only; it has a
  free-text `note`. The Simple Desk cue stack's `Cue` has a name, no number.
- **No main cue list.** The nearest thing is "the chaser started last"
  (`ProgrammerController::stepCurrentChaser`), which VC buttons step.
- **MSC can't arrive.** The MIDI plugin drops SysEx: CoreMIDI skips the whole
  packet, ALSA has no SysEx case. MTC works from quarter frames only
  (`MidiMtcDecoder::feedFullFrame` is never called). SysEx *out* works.
- **OSC loses QLab's messages.** The OSC plugin drops a message with no
  arguments (QLab's `/go`), discards string arguments, and squashes values
  to a byte on a hashed channel.
- **Nothing maps an outside message to GO.** VC Cue List inputs are per
  widget; web access is per widget; the control-surface Go role is unwired.

## Decisions (Branson, 2026-10-13)

1. **A Cue column on chaser steps** — text like "12.5". A step with none
   answers to its position (1, 2, 3…), so a chaser works with no setup.
2. **A main cue list, and numbered lists.** One chaser is marked the Main
   cue list; any chaser can have a cue-list number. A command with no list
   goes to the main one.
3. **OSC on a listener of its own** — a setting (on/off, port), not a
   patched universe.
4. **This branch:** cue numbers and lists, then GO / STOP / PAUSE / RESUME /
   LOAD / ALL OFF from MSC and OSC in.

## The model

- `ChaserStep::cueNumber` (QString, XML `Cue`, empty = none). The step's
  **cue** is its number, or its position counting from 1 when it has none.
  Cues match as numbers when both read as numbers ("12.50" is "12.5"),
  else as text.
- `Chaser::cueListNumber` (QString, XML `CueList`, empty = none).
- The **main cue list** is per show: `ShowControl::mainCueList()` (a chaser
  id), saved in the workspace.
- A list is found by number (a chaser whose cueListNumber matches), and a
  command with none goes to the main cue list. No main one set and only
  one chaser in the show: that one.

## Commands (`ShowControl`, engine)

| Command | What it does |
|---|---|
| GO | No cue: the loaded cue if one is, else the next (the first, from stopped). A cue: that cue. Starts the list if it's stopped. Honours GO on the beat. |
| STOP | Stops the list (no cue) — or nothing if that cue isn't the one playing. |
| PAUSE | Pauses the list. |
| RESUME | Un-pauses it. |
| LOAD | Puts a cue on standby: the next GO with no cue goes there. |
| ALL OFF | Stop ALL functions -- the panic button (no fade). |
| RESET | Stops the list and forgets what's loaded. |

Running state stays the chaser's own (`Chaser::setAction`, `start`,
`stop`, `setPause`), as the VC cue list drives it, with the master as
parent. What's loaded is ShowControl's, per list.

## MIDI Show Control in

`F0 7F <device> 02 <format> <command> <data> F7`.

- **Device**: answer our ID (a setting, default 1) and the all-call 7F.
  Groups (70–7E) are taken as all-call.
- **Format**: Lighting (01), Moving Lights (02) and All-types (7F); others
  ignored.
- **Commands**: GO 01, STOP 02, RESUME 03, LOAD 05, ALL_OFF 08, RESET 0A,
  GO_OFF 0B. In MSC, STOP halts the cue in progress and RESUME carries it
  on, so STOP is our PAUSE; GO_OFF is our STOP. TIMED_GO 04 is GO, its time
  ignored. Others ignored.
- **Data**: ASCII cue number, then 00 and the cue list, then 00 and the
  path (ignored).
- **Getting it in**: CoreMIDI and ALSA assemble SysEx (it can span packets)
  and the plugin emits it on a new `QLCIOPlugin::sysExReceived(universe,
  line, QByteArray)`, carried out like timecode (InputPatch →
  InputOutputMap → Doc → ShowControl). A full-frame MTC message, now
  arriving too, goes to the MTC decoder.
- The decoding is ShowControl's (pure, tested); the plugin only assembles.

## OSC in

A UDP listener in the UI (`ShowControlListener`; the engine doesn't link
Qt Network) hands each datagram to `ShowControl::handleOsc()`, which parses
it (bundles too). Settings: **Listen for OSC** (off by default — anyone on
the network could fire cues) and a **port** (default 53100; QLab itself
uses 53000/53001 and may run on the same Mac).

Addresses, with an optional `/qlcconsole` prefix and an optional
`/list/<n>` before the command:

| Address | Command |
|---|---|
| `/go` | GO (an int/float/string argument is a cue) |
| `/go/<cue>`, `/cue/<cue>/go`, `/cue/<cue>/start` | GO that cue |
| `/load/<cue>`, `/cue/<cue>/load` | LOAD |
| `/stop`, `/pause`, `/resume`, `/reset` | as named |
| `/panic`, `/alloff` | ALL OFF |

## UI

- Chaser editor: a **Cue** column; **Renumber** (1, 2, 3… in step order);
  **Cue list** number and **Main cue list**.
- Settings → **Show control**: Listen for OSC + port; MSC device ID; the
  last command received, so a QLab hookup can be checked.
- Help: a Show control guide (QLab: a Network cue or MIDI cue → this).
