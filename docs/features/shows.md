# Shows (the timeline)

A **Show** plays lights against time, the way an audio or video editor
lays clips on tracks: a song's cues at the moment they happen, run from
the top, from where you left off, or locked to the band's timecode. Open
it from the **Shows** tab (in the Classic QLC+ and Everything views;
**Window ▸ Shows** switches there from the qlcconsole view); the Build tab
can show the same timeline for the scene you're building.

## The words

| Word | What it is |
|---|---|
| **Show** | One timeline. A workspace can hold several (a show per song, say); the list at the top switches between them. ⌘H makes a new one. |
| **Track** | A row of the timeline. A track holds the items for one part of the rig — usually one scene's fixtures — and has its own name, colour, submaster and flags (below). |
| **Item** | Anything placed on a track at a time: a Sequence, a Chaser, an EFX, an RGB Matrix, an Audio or Video file. Its left edge is when it starts; its length is how long it plays. |
| **Sequence** | Steps of **one** scene's values, in order — the usual timeline item. A new step starts as a copy of the one before, so you change only what differs. It always plays forward, start to end. |
| **Chaser** | Steps that are each **any** function, in any order (forward, backward, ping-pong, random). Put one on the timeline when the steps aren't all one scene. |
| **Cue** | One step of a sequence on the timeline: when it starts, its fade in, its hold, its fade out ("fade in 2 s · hold 4 s · fade out 1 s · starts 1:12.5"). Select several to move or **Scale cue timing** together. |
| **Cursor / playhead** | The vertical line: where playback starts and where it is. The time field shows it. Click the time ruler or an empty part of a track to move it; drag across empty track space to select items. |
| **Show end** | The handle at the end of the timeline: the show's length. Drag it, or right-click it — **Fit end to content**, **End at current playhead**, **End at SMPTE…**, **Set length…**. When the show reaches its end it stops there and **parks** — the last look stays up, as a base under whatever you play next — instead of running on into nothing. |
| **Time markers** | The ruler's units: time, or BPM in 4/4, 3/4 or 2/2 (with the tempo beside it). **Snap to grid** (⌘G) puts items on the marks. |

### A track's flags

Right-click a track's header:

- **Hold at end (stays up between cues)** — a gap between items keeps
  the last look on, rather than going dark. The held look is a base under
  everything else: anything that plays on top of it mixes with it as two
  cues would (the higher level of a dimmer shows; a position or colour
  goes to what plays), and the track's next item, or any cue that drives
  those fixtures, takes them over.
- **Solo-safe (mute-exempt)** — soloing another track doesn't mute this
  one (house light, a haze track).
- **Priority / role** — **Normal** (a dimmer takes the highest level of
  what plays; any other channel, the last item to write it),
  **Override** (this track wins every channel its scenes play, dimmers
  too: a lower level on it pulls a light below a Normal track or a cue at
  full; when its item ends, the levels underneath come back), or
  **Background** (yields to every other track). Parked fixtures, Focus
  mode, a flash and a highlight still beat an Override track, and the
  Grand Master and Blackout still apply to it.
- **Intensity submaster** — the track's fader scales everything on it.
  It's saved with the show, and the level you set last, before Start or
  while the show plays, is the one that plays.
- **Track colour…**, **Track name**, **Delete Track**.

## Doing things

- **Add a track or a function** — ⌘N: a new empty track, or a scene /
  sequence / chaser / EFX / RGB matrix / audio dropped at the cursor
  (dragging one from the **Functions — drag onto a track** list works too).
- **New sequence on a track** — ⌘E; **audio** — ⌘A; **video** — ⌘D.
- **Edit an item** — double-click it (a scene clip opens its scene).
- **Time an item exactly** — ⌘T: its start and length. A sequence or
  chaser stretches its steps to fit; an EFX or RGB matrix can stretch or
  loop.
- **Lock an item** — ⌘K, so it can't be dragged by accident; **colour** —
  ⌘L; **copy / paste** — ⌘C / ⌘V (it checks the paste won't overlap or
  land on an incompatible track); **delete** — Delete (from the show only;
  Build ▸ Scenes deletes the function itself).
- **Play / pause** — Space; **stop** — ⌘Space (twice: back to 0).
- **Undo** — ⌘Z.

## Following timecode

**Follow MIDI Time Code** locks the playhead to the band's MTC: it finds
the source by itself (or you pick one), follows locates and seeks, and
freezes the look when the timecode stops. "Timeline 0 = current timecode"
lines the show's zero up with wherever the clock is now. The footer's
**MTC** chip says what's being followed and how healthy it is (rate,
jitter, latency); **Load**, if you turn it on in Settings ▸ Appearance,
says how much of each DMX tick the show uses.
**Connections → Devices → Show control &amp; sync** shows the same source and health
with the other outside links, and can pick the source too. The source
picked is saved with the show.

**Art-Net TimeCode** works the same way. Many DAWs, QLab add-ons and media
servers send timecode over the network as Art-Net rather than as MIDI.
Patch an Art-Net **input** on any universe (the node's interface, in
Connections), and its timecode is followed exactly as MTC is. It shows in
the source list as *Art-Net TimeCode*. The four rates are all read: film
(24), EBU (25), drop-frame (29.97) and SMPTE (30).

**LTC (audio timecode)** is the third source. Many playback rigs and
recorders print SMPTE timecode as a sound on a spare track; patch that
track to this computer's audio input and pick **Audio input (LTC)** as the
source (the footer chip's menu, or Connections → Devices → Show control &amp;
sync). It is read from the **first channel** of the input chosen in the
audio settings (the one audio triggers use), at 24, 25, 29.97 drop-frame
and 30 fps, either polarity; the rate is found by itself. The chip then
reads **LTC** instead of MTC, and it is followed like the others: locates,
seeks, and the look freezes when the sound stops. A cable pulled or a
song restarted is caught up within two frames of the code coming back.

- Feed it a line-level signal, not a microphone. A hot or distorted square
  is fine; a signal under about half a percent of full scale is taken for
  silence.
- The audio input adds a little delay (a few tens of milliseconds, one
  buffer plus a frame). Set the show's **offset** to take it out.
- Forward play only: LTC read backwards (tape rewinding) is not followed.
- On macOS the first time asks for microphone access, as audio triggers do.

While the show runs, Virtual Console faders and buttons can still take a
channel over live.

## See also

- [Getting started → Running the show](getting-started.md#running-the-show)
- [Keyboard and mouse → Shows](shortcuts.md#shows-timeline)
- The upstream QLC+ reference (Help → QLC+ Reference (online)) for the stock Show
  Manager in depth.
