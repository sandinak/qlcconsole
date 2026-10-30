# VC effect controls — Design

Asked 2026-10-29: "Is there a way we can map effect options to VC widgets
(knobs, sliders, etc)? You'd want that somewhat reusable so that the current
effect can use the slider for its setting." Built on feat/031-vc-controls
(part 3, after hits on VC buttons and VC speed-master sliders).

## Shape

A **VC slider** (slider or knob style) gets a new mode, **Effect**: it
controls one **parameter** of one **target** effect. External input
(MIDI/OSC/encoder in absolute mode) feeds it as any slider. No new widget
type, no new tab, no menu item.

**Target** — which effect:

| Target | Means |
|---|---|
| A palette | that effect palette, wherever it plays |
| On a group | the effect playing on fixture group X (its look's groups) |
| Current | the most recently **started or selected** effect; scope: all, or one group |

*Current* is what makes one fader reusable across songs: start the next
song's look and the same knob now drives its effect. **Started** = an effect
palette's engine starting (EffectScriptRunner), or an EFX / RGB Matrix
function starting; **selected** = an effect palette shown in Build's look
editor (`ProgrammingManager::currentPaletteIdChanged`). A target that has
stopped falls back to the one before it that still plays.

**Parameter** — by role, mapped to each effect's own names (first match):

| Role | Effect scripts (params) | EFX | RGB Matrix |
|---|---|---|---|
| Speed | speed, rate, motion, spin | duration (×¼..×4, middle as set) | duration (same) |
| Size / Width | size, width, scale, length, panSize/panRadius | width + height | n/a |
| Rate | rate, density, frequency | n/a | n/a |
| Offset / Spread | spread, phaseSpread, offset; else the host Spread amount (0..8 cycles) | n/a | n/a |
| Direction | direction (its list), reverse | Forward / Backward | Forward / Backward |
| Intensity | intensity, level, gain, dimmer | the function's intensity | the function's intensity |
| Colour 1 hue | the look's first colour, hue turned | n/a | colour 1, hue turned |

A script param's range is its own min..max; a list param (`values:`) is
stepped through. An effect without the role shows **n/a**.

## Live override, plus Store

Decided (can change): **a value is a live override** of the running
instance — the palette (or function) is untouched, and the override goes
when the effect stops; started again, it plays as stored. A **Store**
button on the widget (shown in this mode) writes the overridden values into
the palette (or keeps them in the EFX/RGB Matrix), as the look editor's
parameter edits do -- like those, not an undo step yet. Colour 1 hue is
never stored: the colour is the look's, not the effect's.

Engine path: the same as the look editor's live edits (lb, 2355e32ab:
`EffectScriptRunner::updateEffectParams()` → `EffectInstance::
reloadParamsFromPalette()`), with an override layer on top:
`EffectScriptRunner::setParamOverride(palette, param, value)` sets it on the
running instance (script params into its param values; the host spread
amount and colour 1 hue as instance fields), keeps it across a palette
reload, and drops it when the instance goes. EFX / RGB Matrix: the VC layer
remembers the original value on first change and puts it back when the
function stops (Store forgets the original).

## Display

The slider's top label is the value ("1.50", "Clockwise", "×2.00", "n/a",
"not playing"); the bottom label is the target ("Truss Wave · Speed").

## Cost

The slider is not a DMX source in this mode; it acts on moves and on
effect start/stop/selection signals only. The engine checks one empty hash
per tick for overrides (spread, hue) — no per-tick cost when idle.

## Save / load

`<EffectControl Target="Palette|Group|Current" Palette=".." Group=".."
Role="Speed"/>` inside `<Slider>`, written only in Effect mode; older shows
load as they were.

## Not built

- A Speed Dial for effect parameters (its time is a duration; a Speed Dial
  on the effect's **speed master** covers tempo).
- Relative (endless) encoders beyond what an input profile turns into
  absolute values.
- Overrides surviving a stop; per-head parameters.
