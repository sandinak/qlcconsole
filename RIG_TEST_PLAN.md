# Live-Rig Test Plan — recent features

A checkable test set for the fork's recent work, written to be run **on the real
rig by an operator/student** (no source knowledge needed). Work top-to-bottom;
tick each ✅ / ❌ and jot what you saw. Anything ❌ → note the cue/fixture and grab
a phone photo or screenshot.

## Rig prerequisites
- At least a few **moving heads** (pan/tilt + a separate dimmer) and a **pixel
  panel / LED strip** patched and visible (real or in the 2D monitor).
- A **MIDI keyboard** on an input (for the note-effect tests) and, if you have
  one, a **second MIDI controller** (Launchpad/APC) on another input.
- An **audio input** (line-in or mic) for the audio tests.
- Run with: `build/main/qlcplus -o <your show>.qxw`.
- To capture effect internals on a failure, relaunch with
  `QLC_EFFECT_DEBUG=/tmp/effect.log build/main/qlcplus -o <show>.qxw` and attach
  `/tmp/effect.log`.

---

## A · MIDI note effects (Keys / Puddles / Comet)
Use a look that targets the **pixel panel**, with the effect (e.g. MIDI Puddles)
and a colour or two nested under it (see §B).

| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| A1 | Open the look's effect params; open the **MIDI source** dropdown | Lists your inputs **by name** (e.g. "Universe 9 — Launchkey"), not a number slider | |
| A2 | Pick your keyboard as the source; play notes | Panel reacts to the keyboard only | |
| A3 | Click **Learn range…**, play your lowest key then highest, click **Stop & lock** | noteLow/noteHigh snap to the played notes; readout shows them | |
| A4 | Play your lowest key | Lights the **far-left** column | |
| A5 | Play your highest key | Lights the **far-right** column | |
| A6 | Play a single note on a multi-pixel fixture | Lights **individual pixels**, not the whole fixture flat | |
| A7 | Switch the effect script (Keys → Puddles → Comet) | Output **changes live**; note range / source / axis **carry over** (don't reset) | |
| A8 | Change **Axis** to Vertical | Notes spread across **rows** instead of columns | |
| A9 | Drag any effect param slider while playing | Change takes effect **immediately** on the rig | |
| A10 | Stop playing; leave the look up and idle | No flicker; the machine's load/CPU **stays low** while idle (it should wait for input) | |
| A11 | (2 controllers) Set one effect's source to the keyboard, another's to the Launchpad | Each effect reacts only to **its** device | |

---

## B · Looks tree — effect vs fixture colour
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| B1 | On an effect look, drag a **Colour onto the Effect** row | It **nests under** the effect; the panel is NOT flatly pre-lit that colour | |
| B2 | Play the effect | Strikes show in that colour on a dark panel (no wash-out) | |
| B3 | Drag a colour to the **top level** (above the effect) | It now **lights the fixtures** as a static base | |
| B4 | Right-click a nested colour → **Move to fixtures (static base)** | It jumps to the top level | |
| B5 | Right-click a base colour → **Feed effect ▸ <name>** | It nests under that effect | |
| B6 | Select a nested colour, press the **Up** arrow past the effect | It pops **out** to a base | |
| B7 | Look at the fade columns | Effect **Fade In** = "—"; nested colour fades = "—"; a base colour's fades are editable | |

---

## C · Effect behaviours
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| C1 | On an effect look, set its **Fade Out** (e.g. 2 s) in the Looks tree; run then stop the look | The effect **trails off** over 2 s instead of snapping black | |
| C2 | Apply an **RGBScript "Sunset"** look on the panel | **No flashing**; smooth; load is reasonable | |
| C3 | Set Sunset's **Playback = Once (hold last)** | Plays through once and **holds** the final frame (doesn't restart) | |
| C4 | Audio look "Audio VU"; set **Start/End Hz = 40–150**; play bass-heavy music | Reacts to the **kick/bass**, ignores highs | |
| C5 | Same, **Start/End Hz = 3000–5000** | Reacts to **cymbals/hats**, ignores bass | |
| C6 | Create an effect look with **no fixtures** assigned | Drives **nothing** (no random rig output) | |

---

## C2 · One-shot effects (Wand / lifecycle)
Use a look on the pixel panel with the **Wand** effect.
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| C2a | Run the look once | The wand sweeps across the panel **one time** and clears — it does NOT loop | |
| C2b | Put the look on a chaser step and run the chase | The sweep **fits the step** (finishes about when the step ends), landing in time | |
| C2c | In the effect panel set **One-shot length = 3 s**, run again | The sweep now takes ~3 s regardless of the step (per-look override wins) | |
| C2d | Set One-shot length back to **auto** | Length follows the cue/step again | |

## D · 2D monitor / aim targets
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| D1 | Add an Aim target used by a scene with **both movers and a fixed panel** | Aim traces draw **only to the movers**; the fixed panel gets **no trace** | |
| D2 | Remove that target | Traces **disappear immediately** (no need to resize the window) | |

---

## E · Mark / move-in-black — MANUAL
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| E1 | Select some **dark** movers; aim/colour them (apply the next look); click **Mark** | They hold that position **while staying dark**; a **dashed violet outline** appears on them in the 2D monitor | |
| E2 | Bring those fixtures up in a cue/look | They reveal **already in position** (no sweep); the mark **auto-releases** (outline clears) | |
| E3 | Mark some, then click **Unmark all** | All marks clear; outlines gone | |

---

## F · Mark / move-in-black — AUTOMATIC (the big one)
Run your show as you normally would (chaser **or** show timeline).
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| F1 | Turn on **Auto MIB** in the Programming toolbar | Button stays lit | |
| F2 | Step to a cue where a mover is **dark**, and the **next** cue reveals it **pointing somewhere else** | During the dark cue the mover **quietly pre-swings** to the next position; violet outline in the monitor | |
| F3 | Let the next cue fire | The mover comes up **already aimed — no visible sweep** | |
| F4 | Set up a case where two cues are **back-to-back** (little gap) | The planner **skips** the pre-set (not enough dark time) rather than sweeping live | |
| F5 | **TIMING CHECK:** if F2 pre-sets too *late* (still moving when lit) or never | Note it — the **dark-gap** lead time needs tuning (currently ~0.8 s, dev to adjust) | |
| F6 | Turn **Auto MIB** off | Planner's pre-sets release; manual marks (if any) stay | |

---

## G · Odds & ends (quick)
| # | Step | Expect | ✅/❌ |
|---|------|--------|------|
| G1 | Simple Desk → **per-fixture** view with many fixtures | A **horizontal scrollbar** lets you reach them all | |
| G2 | Fixtures page, an ArtNet universe | Shows the **target IP + universe**, not just "ArtNet" | |
| G3 | (If you have an encoder controller) map a **relative knob**; turn it | Smooth, no jumps; **invert** flips direction; live preview tracks | |

---

## Known rig-verify focus (tell the dev)
1. **F5 — Auto-MIB dark-gap timing** is the #1 unknown: the "how soon does the
   next cue fire" figure was never tested against a running chaser/timeline.
2. **E2 / F3 — mark auto-handoff** release point (does it let go exactly as
   intensity comes up, no flash or late snap?).
3. Anything in **A3–A5** if the note-range mapping still feels off on your panel.
