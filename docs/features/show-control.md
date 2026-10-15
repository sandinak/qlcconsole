# Show control (QLab)

QLab — or anything else that speaks **MIDI Show Control** (MSC) or **OSC**
— can fire qlcconsole's cues by number: QLab runs the sound and video, and
its cues GO the lights.

Everything that controls or clocks the show from outside is set up in one
place: **Connections → Show control &amp; sync** — OSC in, MIDI Show Control
in, and MIDI timecode, each with what it last heard. (It used to be a group
in the Settings dialog; that group now just has an **Open** button that
takes you there. Your settings carried over unchanged.)

## Cues and cue lists

A **cue list** is a chase (a chaser). Its steps are the **cues**.

- **Cue numbers.** The chaser editor has a **Cue** column. A step with no
  number of its own answers to its position — 1, 2, 3… — shown in grey, so
  a chase works with no setup. Type a number to give a step its own ("12.5",
  "101"); it keeps it when steps around it move. **Renumber Cues** numbers
  every step 1, 2, 3… in the order they are now.
  Numbers match as numbers: "12.5" is "12.50".
- **The main cue list.** Tick **Main cue list** in one chase's editor: a
  command that doesn't name a list goes there. A show with only one chase
  needs no main list — that one is used.
- **Numbered lists.** Give a chase a **Cue list** number to drive it by
  name: MSC's cue-list field, or `/list/2/…` in OSC.
- **Skipping a step.** Untick a step in the chaser editor's **Step** column
  (or right-click → **Skip This Step**; several at once work too) to leave
  it out without deleting it: GO, Next, Back and the steps that run on by
  themselves pass over it. It shows greyed and struck through, in the
  editor and in the Virtual Console cue list. Asked for by number — GO cue
  4, or picked in the cue list — it still plays. Tick it to put it back.

## What's next, and GO from anywhere

- The footer's **Next:** says what the main cue list's GO plays next — its
  cue number and name. Click it for **GO**, **Back**, **Stop**, **Load ▸**
  (any cue, onto standby) and **Open the Cue List**.
- **GO** is ⌘↩ (Ctrl+Enter) and **Back** ⌘⇧↩, in every window, and in
  **Control** → GO / Back (Main Cue List).
- A **Virtual Console cue list** shows a **Cue** column beside the step
  number and a **Next:** line under its list — the cue, its name and its
  note.

## What a command does

| Command | What happens |
|---|---|
| **GO** | With a cue: that cue. Without: the cue on standby (LOAD) if there is one, else the next not skipped — the first, if the list isn't playing. Starts the list if it's stopped. A chase set to **GO on the beat** still waits for its beat. |
| **LOAD** | Puts a cue on standby: the next GO without a cue goes there. Nothing changes on stage. |
| **STOP** | Stops the list. With a cue, only if that cue is the one playing. |
| **BACK** | The cue before the one playing (past skipped ones). |
| **PAUSE** / **RESUME** | Holds the list where it is, and carries on. |
| **RESET** | Stops the list and forgets what's on standby. |
| **ALL OFF** | Stops everything that's running — the panic button. |

## OSC — QLab's Network cue

1. **Connections → Show control &amp; sync → OSC in**: tick **Listen for OSC
   cue commands** and pick a port (53100 by default). It's off until you
   turn it on, because anyone on the network could fire a cue. It takes
   effect at once, and the page shows this computer's address to send to.
2. In QLab, point a network patch at that address and port. If QLab runs on
   the same computer, use 127.0.0.1 — and a port other than 53000 and
   53001, which QLab uses itself.
3. Add a **Network cue** with an OSC message:

| Message | Does |
|---|---|
| `/go` | GO the next cue (or the one on standby) |
| `/go 12.5`, `/go/12.5` | GO cue 12.5 |
| `/cue/12.5/start`, `/cue/12.5/go` | GO cue 12.5 |
| `/cue/12.5/load`, `/load 12.5` | LOAD cue 12.5 |
| `/cue/12.5/stop` | STOP, if cue 12.5 is playing |
| `/stop`, `/pause`, `/resume`, `/reset`, `/back` | as named |
| `/panic`, `/alloff` | ALL OFF |

Put `/list/2` in front for cue list 2 (`/list/2/cue/5/start`), and
`/qlcconsole` in front of either if you like (`/qlcconsole/go`). A cue
given as an argument can be a number or text.

## MIDI Show Control — QLab's MIDI cue

1. **Connections**: patch the MIDI port QLab sends on as an **input** of any
   universe (the same as for MIDI timecode).
2. **Connections → Show control &amp; sync → MIDI Show Control in**: **Answer
   MIDI Show Control** is on; set the **device** number QLab sends to (1 by
   default). A message to the all-call device (127) is always answered.
   **Listen on** is **All MIDI inputs** (the default) or the one port QLab
   sends on. The choice names the port, not a universe, and belongs to
   this computer, so it holds for every show. Under it the page says where
   MSC can come in — the universes with that input patched — or, if the
   chosen port isn't patched in this show, says so in red: MSC isn't
   answered then (it doesn't quietly fall back to every input). **Patch a
   MIDI input…** goes to where inputs are patched.
3. In QLab, a **MIDI cue** sending MSC with the command format **Lighting**
   (or Moving Lights, or All Types), the command, and the cue number — and
   the cue list, for a list other than the main one.

MSC's **STOP** holds the cue in progress and **RESUME** carries it on, so in
qlcconsole they are PAUSE and RESUME; **GO_OFF** stops the list. GO, LOAD,
ALL_OFF, RESET and TIMED_GO (as GO) are answered too.

## Checking the hookup

**Connections → Show control &amp; sync** says whether OSC is listening —
green, or in red why it can't, such as "Can't listen for OSC on port 53100"
when another program has the port (pick another) — and, for OSC and MSC
each, the **last command** that arrived, when, and what came of it: "GO
cue 12.5 — Main", or why not: "no such cue in Main", "no main cue list
(mark one in its chaser)". An OSC message that isn't a cue command shows
its address ("/qlab/hello — not a cue command"), so a typo in QLab shows
up too; an MSC command says which universe's input it came in on. Fire a
cue from QLab and look there first.

The same page's **MIDI timecode** section picks which universe's MIDI input
timecode is taken from (the same choice as the footer's **MTC** chip) and
saves that choice with the show (it's a universe of the show's patch; a
show that names a universe it doesn't have follows any), and shows the
code as it arrives with its **sync health**: healthy, usable or
unstable, from how fast it runs against real time, its jitter and how
often it updates.

**QLab out** — firing QLab's cues from here — isn't built yet; the page
keeps a place for it.

## See also

- [Shows (the timeline)](shows.md) — following MIDI timecode instead.
- [Getting started](getting-started.md#running-the-show).
