# Show control (QLab)

QLab — or anything else that speaks **MIDI Show Control** (MSC) or **OSC**
— can fire qlcconsole's cues by number: QLab runs the sound and video, and
its cues GO the lights.

Everything that controls or clocks the show from outside is set up in one
place: the **Show control & sync** rows at the bottom of **Connections →
Devices** — **OSC in**, **MIDI Show Control in** and **MIDI timecode**, each
with its state beside it ("Port 8000 · listening", "Off"). Click a row to
set it up on the right, with what it last heard. (It used to be in the
Settings dialog, then a sub-tab of its own; your settings carried over
unchanged.)

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
  cue number and name. It is status only: click it for the **Run** tab,
  where GO, Back, Stop and LOAD are.
- **GO** is ⌘↩ and **Back** ⌘⇧↩, in every window, and in
  **Control** → GO / Back (Main Cue List).
- A **Virtual Console cue list** shows a **Cue** column beside the step
  number and a **Next:** line under its list — the cue, its name and its
  note.

## Sub-cues

A cue can have cues inside it: cue 4 brings up a look, and **4.1, 4.2,
4.3** step through changes within it on a second button, **Sub GO**, while
GO still goes on to cue 5.

- **Making one.** Use a chase as a step of the cue list (drag it in like any
  function). Its steps are that cue's sub-cues. In the chaser editor the
  step gets a disclosure triangle: open it to see the steps under it,
  numbered, with their fades and holds. They're read-only there —
  right-click → **Edit Chase…** (or double-click one) opens that chase to
  change them. The chase can still be used anywhere else.
- **Sub-cues column.** On the chase step: **Own timing** (the default, and
  what every older show has) runs the chase on by itself, as ever.
  **Sub GO** holds each of its steps until a Sub GO.
- **Playing.** GO into cue 4 plays its first sub-cue (past skipped ones)
  and holds there — the chase's own holds aren't counted. **Sub GO** plays
  the next sub-cue with its own fade; **Sub Back** the one before. At the
  last (or first) one they do nothing. **GO** goes on to cue 5 whatever
  sub-cues are left, and **Back** to cue 3; the chase stops either way, as
  it would. If cue 4 has a hold of its own, it follows on to 5 when that
  runs out. A chase set to **GO on the beat** holds a Sub GO for the beat
  too.
- **Numbering.** A sub-cue is its cue's number, a dot, and its place in the
  chase — 4.1, 4.2… (grey, in the editor) — unless the step has a cue
  number of its own in that chase, which it keeps.
- **Where you see them.** The Virtual Console cue list shows the same
  nesting, with the sub-cue playing highlighted under its cue; a cue list
  with a Sub GO step gets **Sub GO** and **Sub Back** buttons, and clicking
  a sub-cue plays it (its cue first, if that isn't the one playing). The
  footer reads **Sub: 4.2 · Next: 5** while sub-cues are left, or
  **Sub: (end of 4)**; its menu has Sub GO and Sub Back.
- **Keys.** **Sub GO** is ⌥⌘↩ and **Sub Back** ⌥⌘⇧↩ (Ctrl+Alt+Enter and
  Ctrl+Alt+Shift+Enter), in every window, on the main cue list — beside GO
  ⌘↩ — and in **Control** → Sub GO / Sub Back (Main Cue List).

One level only: a sub-cue's own chase steps run on their own timing.

- **In the Show Tree,** sub-cues are made in the tree (+ Sub-cue) and can
  **auto-follow**: see [Sub-cues](show-tree.md#sub-cues).
- **The current production.** With Show Tree productions, a command with no
  list goes to the **current production's** main list — the same list
  GO (⌘↩) and the Run tab play. See
  [Productions](show-tree.md#productions) and
  [The cue runner](show-tree.md#the-cue-runner).

### Sub-cues from QLab

- **By number.** `/cue/4.2/go`, `/go 4.2` and MSC GO 4.2 play sub-cue 4.2
  — cue 4 first, if another cue is playing or the list is stopped. A main
  cue numbered "4.2" of its own wins; otherwise "4.2" is sub-cue 2 of cue
  4, or a sub-cue whose own number is 4.2.
- **Sub GO and Sub Back** over OSC: `/subgo` and `/subback` (send them from
  a QLab Network cue). They step the cue playing, as ⌥⌘↩ does; an
  argument is ignored. MSC has no Sub GO command — send a numbered GO
  (GO 4.2) instead.
- **LOAD 4.2** (`/cue/4.2/load`, MSC LOAD) puts the sub-cue on standby:
  **Next:** in the footer and in the cue list shows it — "Next: 4.2 —
  Wash" — and the next GO plays it. The Run tab loads a sub-cue too.
- **STOP 4.2** stops the list only when 4.2 is the sub-cue playing.
- When Sub GO can't, **Connections → Devices → Show control & sync** says why:
  "cue 1 has no sub-cues on Sub GO", "end of 4", "4 is at its first
  sub-cue".

## What a command does

| Command | What happens |
|---|---|
| **GO** | With a cue: that cue (or sub-cue, 4.2). Without: the cue on standby (LOAD) if there is one, else the next not skipped — the first, if the list isn't playing. Starts the list if it's stopped. A chase set to **GO on the beat** still waits for its beat. |
| **LOAD** | Puts a cue on standby: the next GO without a cue goes there. Nothing changes on stage. |
| **STOP** | Stops the list. With a cue, only if that cue is the one playing. |
| **BACK** | The cue before the one playing (past skipped ones). |
| **SUB GO** / **SUB BACK** | The next / previous [sub-cue](#sub-cues) of the cue playing; nothing at the ends. |
| **PAUSE** / **RESUME** | Holds the list where it is, and carries on. |
| **RESET** | Stops the list and forgets what's on standby. |
| **ALL OFF** | Stops everything that's running — the panic button. |

## OSC — QLab's Network cue

1. **Connections → Devices → OSC in**: tick **Listen for OSC
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
| `/go 4.2`, `/cue/4.2/go` | GO sub-cue 4.2 (cue 4 first if need be) |
| `/subgo`, `/subback` | Sub GO, Sub Back in the cue playing |
| `/stop`, `/pause`, `/resume`, `/reset`, `/back` | as named |
| `/subgo`, `/subback` | SUB GO / SUB BACK |
| `/go 4.2`, `/load 4.2` | a sub-cue by its number: GO it (its cue first), or LOAD it |
| `/panic`, `/alloff` | ALL OFF |

Put `/list/2` in front for cue list 2 (`/list/2/cue/5/start`), and
`/qlcconsole` in front of either if you like (`/qlcconsole/go`). A cue
given as an argument can be a number or text; a float is read to the
digits it holds (4.2, not 4.199999809).

## MIDI Show Control — QLab's MIDI cue

1. **Connections**: patch the MIDI port QLab sends on as an **input** of any
   universe (the same as for MIDI timecode).
2. **Connections → Devices → MIDI Show Control in**: **Answer
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

## QLab out — sending a GO on

The other direction: when a cue goes on stage from this console (⌘↩, the
Run tab, the footer, the Virtual Console, a controller), qlcconsole can
send it on to QLab, a sound desk or anything else that listens. It is off
until you turn it on, and it is **this show's** setting, saved with the
show: **Connections → Devices → Show control & sync**, **QLab out**.

- **Send OSC to** a host and port (QLab listens on 53000). The address is
  `/cue/{cue}/start` by default, `{cue}` being the cue number that went, so
  QLab starts the cue with the same number; `/go` sends QLab's own GO
  instead. It leaves from the OSC plugin's output on the network the host
  is on (or the interface you pick under **From**). No new socket and no
  patching needed.
- **Send MIDI Show Control GO to device** *n* on a MIDI output: MSC GO,
  lighting format, with the cue number (`4`, `12.5`, `4.2`), through the
  MIDI plugin's output you pick under **On**.
- **Send** every cue, or only cues with a cue number of their own (set in
  the cue list, or by Renumber). A cue without one is then left out; with
  every cue, its position is its number.

Only a GO that actually puts a cue on stage sends: a GO that finds no such
cue or list sends nothing, and neither does a preview or anything in blind.
A command that came in by OSC or MSC is not sent back out, so QLab and
qlcconsole can't GO each other in a loop. **Last sent** on the page shows
what went. Back, Stop and the like are not sent (decided: GO only, can
change).

## Checking the hookup

**Connections → Devices → Show control & sync** says whether OSC is listening —
green, or in red why it can't, such as "Can't listen for OSC on port 53100"
when another program has the port (pick another) — and, for OSC and MSC
each, the **last command** that arrived, when, and what came of it: "GO
cue 12.5 — Main", or why not: "no such cue in Main", "no main cue list
(mark one in its chaser)". An OSC message that isn't a cue command shows
its address ("/qlab/hello — not a cue command"), so a typo in QLab shows
up too; an MSC command says which universe's input it came in on. Fire a
cue from QLab and look there first.

The same page's **MIDI timecode** section picks which universe's input
timecode is taken from: MTC on a MIDI input, or Art-Net TimeCode on an
Art-Net input, or LTC played into the audio input (see
[Following timecode](shows.md#following-timecode)) (the same choice as the footer's **MTC** chip) and
saves that choice with the show (it's a universe of the show's patch; a
show that names a universe it doesn't have follows any), and shows the
code as it arrives with its **sync health**: healthy, usable or
unstable, from how fast it runs against real time, its jitter and how
often it updates.

The page's **Web access** section starts the web server (the Virtual
Console in a browser) whenever this show opens, on the port you pick; see
[Web access saved with the show](other-improvements.md#web-access-saved-with-the-show).

## See also

- [Shows (the timeline)](shows.md) — following MIDI timecode instead.
- [Getting started](getting-started.md#running-the-show).
