# Sound-reactive lighting — Design

Branson, 2026-11-04: a down-the-line design for lighting that follows the
music beyond what is there now. **Not built now** (the feature freeze holds);
this is the plan to pick up after it. Written from `origin/main` 90c9ef05d.
Companions: `SPEED_SPREAD_DESIGN.md` (the beat clock, Spread),
`VC_EFFECT_CONTROLS.md` (parameter roles, live overrides),
`LINE_TARGET_DESIGN.md`, `PLAYBACK_TIMING_DESIGN.md` (GO on the beat),
`SHOW_REVIEW.md` (PWR-05, the 3 Hz rule).

**The short version.** One new piece of engine, a **music analyser** on the
audio capture thread that already exists, turns the input into a dozen
numbers (level, bass, mid, highs, kick, snare, hat, energy, silent). Everything
else attaches to models we have: a **Music** row in the Look Editor beside
Spread and Speed; **"Audio · Kick"** as a Control Map trigger that fires a
**hit** (armed by where the hit lives); the **beat clock** steered better and
led by the venue's **mic delay**; a **♪ chip** in the footer with a meter and
the **Audio master**. No new tab, no new playback path, no menu item.

---

## 1. What exists

| What | Where | What it gives |
|---|---|---|
| **AudioCapture** — one shared input, its own thread | `engine/audio/src/audiocapture.{h,cpp}`; `Doc::audioInputCapture()` `engine/src/doc.h:223` | Blocks of **2048 frames** (`audiocapture.h:40`), mono mix, DC removal, Hann window, FFTW (`audiocapture.cpp:229-345`). **Log bands 40–5000 Hz**, up to 32 (`audiocapture.h:42-45`, `audiocapture.cpp:176-227`). **Level** smoothed 40 ms attack / 200 ms release (`audiocapture.cpp:235-247`). Silence gate at −54 dBFS sends zeros (`:274-293`). Starts when a client registers bands, stops when the last leaves (`:105-150`). Also feeds the LTC receiver (`:369-372`). |
| **BeatTracker** | `engine/audio/src/beattracker.{h,cpp}` | Spectral-flux **onset** in one band (set to **40–400 Hz**, `audiocapture.cpp:78-81`), adaptive threshold, 200 ms refractory, a silence gate, `getCurrentBpm()` (mean of intervals, `beattracker.cpp:310`) — computed but **not used** for the clock. Emits `beatDetected()` (`audiocapture.h:161`). |
| **Beat generator = Audio** | `engine/src/inputoutputmap.h:1040-1046`, `inputoutputmap.cpp:2064-2116` | Detected onsets → `slotProcessBeat()` (`:2189-2207`): BPM from the **single last interval**, then `MasterTimer::requestBeat()`. |
| **The beat clock** | `engine/src/mastertimer.h:323-352`, `mastertimer.cpp:653-660` | One running beat position every effect reads; an external beat **nudges** it (`alignBeat(0.5)`), tap snaps it. Internal / External / None sources (`mastertimer.h:273-287`). MIDI clock comes in as a Plugin beat (`inputoutputmap.cpp:2218-2229`). |
| **"audio" data channel** for effect scripts | `engine/src/effectscriptrunner.cpp:493-561` | 16 bands 0..1 (normalised to the loudest), `level`, `peak`, `bandCount`. Subscribed only while a live effect asks for it. Read by **27 scripts**: the **23 WLED** sound-reactive ports (`docs/features/effects.md:797`), Audio Spectrum, Audio VU, Matrix EQ, Matrix Box VU (`effects.md:312,316,436`). |
| **RGB Matrix "Audio"** (stock QLC+) | `engine/src/rgbaudio.{h,cpp}` | Spectrum bars on a matrix. |
| **VC Audio Triggers** (stock QLC+) | `ui/src/virtualconsole/vcaudiotriggers.*`, `ui/src/audiobar.h:54-59,107-108`, `ui/src/audiotriggerwidget.h:29` | Per band or volume: a **level threshold** (min/max, divisor) drives DMX channels, a function or a VC widget. Level-crossing, not onset. |
| **Speed masters** | `engine/src/speedmasters.*`, `docs/features/effects.md:486-560` | 4–16 tempo masters following the main clock / show timeline / another master at a ratio; Tap, Pause, Rate; footer buttons. |
| **On the beat / GO on the beat** | `effects.md:439-449`; `engine/src/chaser.h:216` (`GoNow, GoOnBeat, GoOnBar`) | Effects step on beat divisions; a chase or cue list holds GO to the next beat or bar. |
| **Parameter roles + live override** | `ui/src/virtualconsole/vceffectcontrol.h:50` (`Speed, Size, Rate, Spread, Direction, Intensity, Hue`), `EffectScriptRunner::setParamOverride` `engine/src/effectscriptrunner.h:111` | A VC slider turns one role of the current effect; the override goes when the effect stops. |
| **Timecode calibration**, audio listen mode | `ui/src/timecodecalibrationdialog.{h,cpp}` (`slotListenToggled`, `onAudioBeat`); `InputOutputMap::audioInputBlockMs()` `inputoutputmap.h:1062` | Tap or listen to a click to measure an offset. Block latency is known; the device's is not: `AudioCaptureQt6::latency()` returns 0 (`audiocapture_qt6.cpp:126-128`). |
| **Photosensitivity check** | `engine/src/showreviewchecks.cpp:920`, PWR-05 | Four lights or more over 3 Hz: a warning. |
| **Silence fallback, in places** | `effects.md:312,316` | Matrix EQ and Box VU **simulate on the beat** with no audio; the WLED ports **rest dark** (`build.md:1013`). |

**Latency today**, kick to DMX: up to one block (2048 / 44.1 kHz =
**46 ms**, no overlap) + the OS input buffer (unmeasured) + a queued hop to
the GUI thread (`slotProcessBeat`) + up to one MasterTimer tick (20 ms at
50 Hz) — roughly **60–110 ms**, plus the fixture. Light that trails the
sound by more than ~40–50 ms reads as late. Reactive output can't be made
earlier than the analysis; **beat-clock output can** (it predicts).

**Found while reading** (small, worth fixing in phase 0 whatever happens):

1. `effectscriptrunner.cpp:555-557` tells scripts the bands tile **0..maxHz
   linearly**; `audiocapture.cpp:187-201` makes them **log 40–5000 Hz**. Any
   script mapping a band to Hz is off.
2. `dataProcessed(double*, …)` hands a pointer to a buffer the capture
   thread rewrites; the GUI-thread slots (`effectscriptrunner.cpp:535`,
   `rgbaudio.cpp:71`) read it later. Torn frames are possible — harmless
   for a VU, wrong for an onset.
3. Audio BPM jumps on any missed or extra onset (one interval,
   `inputoutputmap.cpp:2194-2203`); the tracker's averaged BPM sits unused.
4. Nothing above **5 kHz** is analysed (`audiocapture.h:45`): a hi-hat lives
   at 6–16 kHz, so it can't be detected today.
5. `AUDIO_DEFAULT_BUFFER_SIZE` says "bytes per channel"; it is frames.

## 2. The gap

- **One trigger, one band.** There is a kick-ish onset and nothing else: no
  snare, no hat, no "energy", no build or drop.
- **Sound-reactive is a kind of effect, not a property of one.** 27 effects
  hear the music; the other ~300 (every phaser, movement, matrix and WLED
  effect) can't. A designer can't say "this ballyhoo, bigger on the bass".
- **Nothing fires a hit from the music.** The busking model (hits, armed by
  place, Fade out) has no audio source; VC Audio Triggers predate hits and
  work on levels, not onsets.
- **No master, no meter in Operate.** You can't turn the reactivity down
  for a ballad, see that the mic is hearing anything, or know the stage
  will stay lit if the band stops.
- **No latency model.** The mic is 10 m from the PA (≈29 ms of air) plus
  the input buffer; nothing measures or compensates it.

## 3. The design

```
 mic / line in
     │
 AudioCapture thread (exists) ──► FFT 2048, bands ──► "audio" channel: 27 scripts,
     │                                               RGB Audio, VC Audio Triggers
     ├─ LTC (exists)
     └─ MusicAnalyser (new)  hop 512 (11.6 ms), window 1024/2048, to 12 kHz
           level · bass · mid · high (fast + slow envelopes)
           kick · snare · hat onsets · energy short/long · silent · confidence
           └─► MusicState: one 64-byte snapshot, seqlock, no allocation
                 ├─ beat clock: steer (median BPM, alignBeat) + venue lead
                 ├─ Music rows (EffectInstance, per tick) × Audio master
                 ├─ Control Map "Audio · Kick" ─► hit (armed by place, 3 Hz guard)
                 ├─ ♪ footer chip: meter, master, state · Why is this on?
                 └─ (later) energy curve ─► Run: "Drop?" on the next cue
```

### 3.0 The analyser (the enabler)

`MusicAnalyser` in `engine/audio/src/`, fed every block by `AudioCapture::run()`
the way LTC is (`processBlock(int16_t*, frames, channels, rate)`), so it is
testable without a device (`engine/test/ltc` already works this way).

- **Hop 512, window 1024** for onsets (11.6 ms resolution at 44.1 kHz;
  ring buffer, preallocated); the 2048 spectrum stays for the existing users.
- **Three onset detectors**, spectral flux with adaptive threshold (the
  BeatTracker's method, generalised): **kick** 40–150 Hz, **snare** 150 Hz–
  2 kHz with a noise (flatness) test, **hat** 6–12 kHz. Each publishes a
  counter and a timestamp, never a signal, so no event is lost between ticks.
- **Envelopes**: level, bass (<150), mid (150–2k), highs (2k–12k), each
  normalised by a slow auto-gain (an 8 s peak follower with a floor) so a
  quiet room and a loud one land in the same 0..1.
- **Energy**: short (0.5 s) and long (8 s) loudness; their ratio is the
  build/drop signal (3.5).
- **Silent**: under the gate for 2 s. **Confidence**: how regular the kicks
  are (low on speech, high on a four-on-the-floor).
- **Tempo**: the median of the last 8 kick intervals, octave-folded to
  70–180 BPM, feeds `setBpm` only when it moves by more than 2 BPM; each kick
  still nudges the phase (`alignBeat(0.5)`). Fixes finding 3.

Everything downstream reads `MusicState`, never the `dataProcessed` signal.

### 3.1 Follow the music — modulation on any effect parameter

**Where it attaches: a Music row in the Look Editor**, under **Spread** and
**Speed** on an Effect look (`build.md:879-885`). The row follows the look
everywhere it plays — Build, the Workbench, a Show Tree layer, a hit — so
there is nothing new on the layer. One row, up to two lines:

| Setting | Values |
|---|---|
| **Follows** | Off · Level · Bass · Mids · Highs · Kick · Snare · Hat · Energy · **Spectrum across the spread** |
| **Moves** | the effect's roles: Intensity · Size · Speed · Spread · Zoom · Colour (hue / mix) |
| **Depth** | 0–100 % of the parameter's range, around its set value |
| **Attack / Release** | 0–2 s (a kick with 0 / 0.3 s is a bump with a tail) |
| **Floor** | the least it falls to (default 30 % for Intensity: never dark on a quiet bar) |

- **Roles move to the engine.** The role table in `vceffectcontrol.h:50`
  (Speed, Size, Rate, Spread, Direction, Intensity, Hue) becomes an engine
  table both the VC slider and the Music row use; Zoom and Colour mix are
  added. One mapping, two users.
- **Intensity on any layer**, effect or not: a **Music Level** effect in
  the Dimmer tab, stacked on the layer like any dimmer effect
  (`show-tree.md:1810`, "Stack effects on a layer"). No layer flag needed.
- **Speed follows**: only in steps (×½, ×1, ×2 of its master) and only on a
  bar line, so the tempo never wobbles. A continuous speed follow is cut.
- **The Workbench layer row** shows **♪** in the badge column
  (`build.md:540-551`) when a look follows the music; hover says what.
- **Same look, calm verse, wild chorus**: two copies of the palette, or the
  Audio master on a control (3.6). A per-layer depth was considered and
  left out: it is a second place to look.

### 3.2 Kick, snare and hat fire hits

**Where it attaches: the Control Map** (`other-improvements.md:1124`).
**Learn Trigger…** on a hit gains a source list beside MIDI and OSC:
**Audio · Kick / Snare / Hat / Drop** (Drop only once 3.5 exists). How a
press acts is the existing set; for audio the useful ones are **Momentary**
(on with the onset, off at once → the hit's own **Fade out** is the decay)
and **Toggle**.

- **The gate is the hit's place.** A hit is armed only while its cue, list
  or production is live (`show-tree.md:2303-2330`), so "kick blinders only
  in the last chorus" is a hit under that cue. Nothing new to learn.
- **Every Nth** (1, 2, 4, 8) and **on the bar's 1** for "a hit on the
  downbeat"; a **min interval** (default 250 ms).
- **Ready-made hits** in a template folder: *Blinder bump* (audience
  blinders Full, Fade out 0.4 s), *Colour bump* (white 60 % on the wash),
  *Sparkle* (Matrix Sparkle, one burst). They are ordinary hits.
- **VC Audio Triggers stay** as they are for Classic; their help points here.

### 3.3 Spectrum across the stage

A source on the Music row, not a new effect: **Spectrum across the
spread** gives each head the band at its place in the look's **Spread**
order (`effects.md:451-470`: stage left→right, columns, out from the
target, a line target's points). Bass at one end, highs at the other, on
movers (Size or Intensity) or pixel bars. Line targets give the order on a
curved truss for free. Blocks and Wings work as they do for phase.

### 3.4 Movement that breathes

No new mechanism: 3.1 on a Position effect. Shipped as **effect presets**
(`EFFECTPRESET_DESIGN.md`): *Ballyhoo, tilt on the bass* (Size ← Bass,
depth 40 %, release 0.25 s), *Fan opens with the energy* (Spread ← Energy),
*Zoom pumps* (Zoom ← Kick). Movement depth is capped at 50 % of the
effect's size so a mover never slams its stops; the Rig Overview's
realistic move speed (0.5) shows when it can't keep up.

### 3.5 Builds and drops: suggest, don't fire

From `energy short / long` plus onset density: a **build** is rising highs
and snare density over 4+ bars with the bass thinning; a **drop** is the
bass back above its long average within a beat after a build. Output:

- **Run tab**: the cue on standby (or the next) shows **"Drop?"** and the
  GO button pulses on the predicted bar line; the operator decides. Never
  an automatic GO (One GO path, STRATEGY 0.4).
- Optionally a **Drop** hit trigger (3.2), armed by place like any hit.
- A show on timecode already knows its sections (`show-tree.md:1556`);
  this is for live bands and DJ sets, and says so.

### 3.6 Operate: the Audio master and the meter

- **♪ chip** in the footer, beside the speed masters: three mini bars
  (bass/mid/high), a kick dot, the **Audio master** %, and its state:
  *Listening*, *Quiet — on the beat*, *No input*. Absent until the show
  uses a Music row or an audio trigger (as the speed masters are).
- **Audio master** 0–100 %, scaling every Music row's depth and arming
  audio triggers above 0. Click the chip for its slider; map it in the
  Control Map, a VC slider (a new slider mode, like Speed master) or the
  pad. Works in Operate. **0 % = every look at its set value**, not dark.
- The chip's click also opens the existing audio input settings, which
  gain the live meter, per-band sensitivity, and the venue's mic delay.
- **Why is this on?** gains a line: *"Level 64 %: Breathe follows Bass
  (depth 40 %), Audio master 80 %"*. The Rig Overview needs nothing: it
  draws the output.

## 4. Safety and show-readiness

- **Never a dead or frozen stage.** After 2 s of silence (or *No input*)
  the analyser **simulates from the beat clock**: a kick on each beat, the
  envelopes at a rest level — the precedent is Matrix EQ / Box VU
  (`effects.md:312,316`). With no beat clock either, Music rows sit at
  their set value. The Floor keeps intensity up. The 23 WLED ports get the
  same fallback instead of resting dark (one change in `WLED.audio()`).
- **Strobe.** An audio-fired hit that lights four or more fixtures is held
  to **3 a second** (PWR-05's rule, `showreviewchecks.cpp:920`) unless the
  show turns the guard off; **Show Review** warns on audio triggers whose
  hit flashes, and on Music rows driving a strobe channel. Kick → strobe
  channel directly is not offered.
- **Readiness footer**: *Not ready — audio input missing* when the show
  uses audio and the device is gone; the capture error already exists
  (`audiocapture.h` `captureError`).
- **Mic delay per venue**, saved with the venue (the room is real). Measured
  with the timecode calibration's **listen** mode (`TimecodeCalibrationDialog`),
  which shares the roadmap item *tc-latency-calibration*: play a click
  through the PA, it hears it, the delay is the gap. Used two ways:
  the **beat clock leads** by it (clock-locked looks land on the beat),
  and reactive output is labelled with it in the chip's tooltip (it can't
  be undone, only shortened by the hop).
- **Operate primacy.** The analyser never touches DMX; a stalled audio
  device stalls nothing but the meter. Learning a trigger is Design-only.
  The Audio master works in Operate; nothing else audio does.
- **Teaching mode** can lock the Audio master at a ceiling.

## 5. Performance

- **Analysis stays on the capture thread** it already has; the MasterTimer
  thread does nothing new. Cost: one 1024-point FFT every 11.6 ms (≈86/s)
  plus band sums — well under 1 % of a core; measure it.
- **MusicState** is one fixed struct published with a seqlock (writer bumps
  a counter, readers retry on change). **No allocation, no lock, no
  QVariant** per tick. The existing per-block `QVariantMap` for scripts
  (`effectscriptrunner.cpp:552`) is left alone (21/s on the GUI thread).
- **Music rows** apply in `EffectInstance` on the tick where scripts run
  already (`effectscriptrunner.cpp:564+`): one multiply-add and an
  envelope step per row, coefficients precomputed when the row changes.
- **Zero cost when unused**: the analyser runs only while something
  subscribes (the existing register/unregister pattern), and an effect
  with no Music row skips the step.
- **Gate**: `hotpath_test` gains a "16 modulated looks" point; `make perf`
  gains an analyser point (CPU per block). Numbers before/after in the
  commits that touch the effect tick.

## 6. Phasing

ROI = Value × Fit ÷ Effort × risk (STRATEGY §3: L 1.0, M 0.8, H 0.6).
Effort in developer-days with tests and help.

| Phase | What | Value | Fit | Reuses | Effort | Risk | Maint. | ROI | Call |
|---|---|---|---|---|---|---|---|---|---|
| **A. Listen better** | MusicAnalyser (hop 512, kick/snare/hat, envelopes, energy, silent), MusicState, median BPM, findings 1–5, ♪ chip with meter, silence → beat-clock fallback (WLED ports too) | 3 | 4 | AudioCapture, BeatTracker, LTC's processBlock, footer chips | 4 | M | M | **2.4** | First; everything below needs it |
| **B. Hits on the kick** | Control Map source *Audio · Kick/Snare/Hat*, Every Nth / on the 1, min interval, 3 Hz guard, Show Review check, Audio master (arm), template hits, course note | 4 | 4 | Control Map, scoped hits, Fade out, PWR-05 | 3 | M | M | **4.3** | Best ROI; ships with A |
| **C. Mic delay per venue** | Venue field, measured by the calibration's listen mode, beat clock lead | 3 | 5 | venues, TimecodeCalibrationDialog, alignBeat | 1.5 | L | L | **10** | With A/B (small, the room is real) |
| **D. Follow the music** | Music row (roles → engine, source/depth/attack/release/floor), Music Level dimmer effect, Spectrum across the spread, breathing presets, ♪ badge, Why is this on? line | 4 | 3 | Look Editor rows, VC role table, setParamOverride, Spread, presets | 6 | M | M | **1.6** | After B is used at a show |
| **E. Builds and drops** | Energy curve, "Drop?" on the next cue in Run, Drop trigger | 2 | 3 | Run, One GO path, D's energy | 5 | H | M | **0.7** | LATER (wait for a real ask) |

Fit for D is a 3, not a 4: continuous modulation is the club-desk end of
the line STRATEGY draws ("DJ/club automation are other tools' jobs"); a
show-choir chorus is a fair use, but the cue list stays the spine. B is a
4: hits on the accents is Lesson 0.6 and 2.6 already.

**Teaching**: a short note in **Lesson 2.6** (*Busking with Hits*): learn a
hit to the kick, armed under the chorus cue, the Audio master to 0 for the
ballad — and why a flash on every kick is a strobe (0.6's restraint).
D gets a Level 3 extension, not a lesson. Help: `effects.md` (*Following
the music*), `show-tree.md` (Hits), `other-improvements.md` (Control Map).

## 7. What's NOT built

- **An audio engine.** The line: qlcconsole **listens to one input and
  reduces it to a dozen numbers**. It plays, routes, mixes, records and
  processes no audio for output; no second input, no JACK/ASIO, no
  plugins, no stored audio. The analyser is a reader on the capture thread
  the LTC decoder and the spectrum already share. If a feature needs to
  *hear a file*, it gets its numbers from that file's analysis offline,
  never by playing it.
- Ableton Link (NO list), automatic GO from the music, continuous speed
  follow, key/chord/pitch detection, stem separation, ML beat tracking,
  per-layer depth, kick → strobe channel direct, an audio tab or panel.

## 8. Tests (offscreen, in the gate)

- **Synthetic signals**, generated in the test (no files): a kick (55 Hz
  sine, 120 ms decay) at 90/120/174 BPM; a snare (band noise burst); a hat
  (noise above 7 kHz); pink noise; silence; a sweep; a kick + hat pattern.
  Fed straight to `MusicAnalyser::processBlock` at 44.1 and 48 kHz, 1 and 2
  channels.
  - Each kick found within one hop (≤12 ms) of the true onset; no kicks on
    hat-only or pink noise; snare/hat each found and kept apart.
  - Median BPM within 1 of the truth after 8 beats, steady under one
    dropped and one extra onset; octave-folded.
  - Silent in 2 s; fallback simulates on the beat clock; back in 1 block.
- **Recorded spectra**: three short CC0 clips (≤10 s, mono, 22 kHz) —
  four-on-the-floor, a live drum kit, speech — with expected onset lists.
- **Modulation**: envelope attack/release against an injected clock;
  Floor; Audio master 0 → set values bit-for-bit; Spectrum across the
  spread orders heads as the Spread preview does.
- **Triggers**: a kick at 240 BPM on a 6-light hit fires ≤3/s; Every 4th;
  a disarmed hit doesn't fire; Momentary releases into Fade out.
- **Venue delay**: beat clock leads by it; saved and loaded with the venue.
- **Save/load**: Music rows and audio triggers round-trip; the stock QLC+
  4.14 round-trip test opens a show that has them.
- **No device in any test**: a fake `AudioCapture` subclass, or the
  analyser fed directly; `hotpath_test` as in §5.

## 9. Open questions

On the board (questions.json): where Follow the music lives, how an onset
reaches a hit, the silence fallback, the strobe guard, and builds/drops.
Also, smaller, decide when D starts:

- Movement depth cap (50 %) — a setting, or fixed?
- The Audio master on the pad (DualSense) — which control, if any?
- A "rehearse with a track" mode that analyses a file without playing it:
  useful for Design without the band, but close to the line; recommended no.
