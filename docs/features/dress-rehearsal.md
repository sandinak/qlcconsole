# Before the house opens

The last half hour before the doors: is everything that has to work going
to work? **Before the House Opens** is one page of what the app already
knows about that, and this guide is the checklist for the rest, the
things only a person in the room can see.

## The page

Open it from **Show Review ▸ Before the House Opens…** (the footer's
Review chip opens Show Review). Going to Operate offers it too, once a
session, as a quiet line above the tabs: **Check Now**.

Each line says how it stands in a word and a symbol, never colour alone:

| Says | Means |
|---|---|
| **✓ Ready** | Nothing to do. |
| **⚠ Check** | The show can run, but look at it. |
| **✕ Not ready** | Fix this before the house opens. |
| **– Not used** | Not part of this show, or this computer doesn't say. |
| **○ By hand** | Only a person can check it: the page says how. |

Under each line's name is what is wrong, in plain words, and the things
it is about. Where the app has its own fix, the button is on the line:
it opens the place the fix is, the same as the footer or Show Review
would. **Check Again** runs Show Review and looks at the computer again;
the lines also follow the footer as things change.

| Line | What it reads | The fix |
|---|---|---|
| **Every universe reaches its node** | The footer's output entries: a universe with fixtures and no output, an output kept offline, a node that stopped answering, a WLED on the wrong universe ([When an output is in trouble](other-improvements.md#when-an-output-is-in-trouble)) | **Open Connections** (or the entry's own fix) |
| **Local Network allowed** | macOS only, with Art-Net or sACN patched: every network output failing means macOS has most likely refused the app the network ([macOS](system-tuning.md#macos)) | **Settings ▸ Performance…** |
| **Controllers connected** | Every pad or MIDI surface the show is patched to | **Show in Connections** |
| **Show Review** | Its blockers and warnings ([Show Review](show-review.md#the-review-chip-and-the-panel)) | **Open Show Review**, on the first blocker |
| **The footer's readiness list** | Whatever else the footer lists as not ready ([Readiness footer](other-improvements.md#readiness-footer)) | **Show Details…** |
| **Mains power** | On battery or plugged in, and how full | Plug it in |
| **Screen stays awake** | Whether the screen sleeps or locks during the show | **Settings ▸ Performance…** |
| **DMX off Wi-Fi** | Whether Art-Net or sACN goes out over Wi-Fi | **Settings ▸ Performance…** |
| **This computer, for the show** | The rest of [Settings ▸ Performance](system-tuning.md#settings--performance) marked important: the timer, full speed, heat, backups | **Settings ▸ Performance…** |
| **Autosave on** | Autosave on, and whether the last one failed ([Show files](getting-started.md#show-files)) | **Turn It On** |
| **Channel check** | By hand: see below | **Quick Focus…** |

The page reads; it doesn't prove a light came on. "Reaches its node"
means the app is sending and no node has stopped answering, as far as
Art-Net can tell: look at each node's own lamp too.

## The channel check

Walk every light before the house opens. [Quick focus](lighting-studio.md#quick-focus)
does it from the pad: choose every moving head (or every fixed light),
the other heads **dark**, and each light comes up alone; **RB** steps to
the next, **LB** back. Watch each one come up and point where it should.
Nothing is saved unless you press **A**. Quick focus runs in Design, so
do the walk before going to Operate.

## The dress-rehearsal checklist

Run it in the room at the dress rehearsal, then again before each show.
Each line says where the app helps.

- **Every universe reaching its node.** The page's first line, then each
  node's lamp. Connections shows each node and its universes.
- **The staffs.** Each WLED staff answers and shows its look; they go over
  Wi-Fi, so try them with the room full of phones if you can
  ([WLED output](other-improvements.md#wled-output)).
- **Candles through the IR blaster.** Fire the candle cue: every candle
  lights, Dim and Brighten step them all. The blaster needs a clear line
  to the candles ([Candles](stage-targets.md#candles)).
- **Lanterns.** Each lantern lights and its colour is right
  ([Lanterns](lighting-studio.md#lanterns)).
- **Fountains.** The fountain lights come up with their cue
  ([Fountains](lighting-studio.md#fountains)).
- **Lasers' zone.** Nothing lands in a no-light zone; Show Review says when
  a light does ([No-light zones](lighting-studio.md#no-light-zones),
  [Lasers and star projectors](lighting-studio.md#lasers-and-star-projectors)).
- **Follow spot framing.** The spot frames the person at each preset
  ([How big: framing and the iris](stage-targets.md#how-big-framing-and-the-iris)).
- **Local Network allowed.** The page's line. macOS asks the first time
  the app reaches the network; if it was refused (or nobody was there to
  answer), System Settings ▸ Privacy &amp; Security ▸ Local Network, turn
  on qlcconsole, then quit and open the app again.
- **Power.** Each dimmer and node on the circuit it was planned on, the
  laptop on mains ([Power distribution](other-improvements.md#power-distribution)).
- **Battery.** The page's **Mains power** line; a pad charged, and its
  spare.
- **Screen awake.** The page's **Screen stays awake** line.
- **Autosave on.** The page's **Autosave on** line, and the show saved.
- **The pads.** Each pad connected (the page's line) and every control
  doing what it should ([The controller test](gamepad-remote.md#the-controller-test)).
