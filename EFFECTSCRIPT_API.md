# Effect Script API Reference

A guide for writing **effect scripts** for this QLC+ fork — enough detail to
author new scripts (including for an AI). Effect scripts are small JavaScript
files that compute live DMX output for a scene, frame by frame. They are the
**single effect engine** for the programmer/looks workflow (the native C++
effects were retired); a followspot, a colour chase, a position→intensity mapper
are all just scripts.

See `POSITION_ARCH.md` for the position/followspot architecture that several of
these concepts (targets, convergence, followMode) come from.

---

## 1. Where scripts live & how they run

- **Location:** `resources/effectscripts/*.js`. On macOS the build copies them to
  `<build>/Resources/EffectScripts/` (the app reads `applicationDirPath()/../Resources/EffectScripts`).
  A user override dir also exists: `~/Library/Application Support/QLC+/EffectScripts`.
- **Loaded by:** `EffectScript` (`engine/src/effectscript.{h,cpp}`) parses and
  validates one file. `EffectScriptCache` scans the dirs. `EffectScriptRunner`
  ticks every live instance; `EffectInstance` builds the per-tick arguments and
  converts the returned intents to DMX.
- **When:** an instance exists for each **Effect palette** attached to a
  **running scene**. Scenes run in **both Edit (preview) and Operate** modes, so
  scripts tick in both. Tick rate is ~**50 Hz**.
- **Output priority:** script output is written through a GenericFader at
  `Universe::Override`, so it **layers on top of** the scene's own palettes and
  reliably wins LTP channels (pan/tilt). HTP channels (intensity) still max.
- **Editing a script:** re-run `cmake --build build` to refresh the copied file,
  then relaunch (scripts are loaded at startup).

---

## 2. Script shape

A script is an IIFE returning one `effect` object:

```js
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "My Effect";        // shown in the effect picker
    effect.description = "One-line tooltip";
    effect.notes       = "Longer help text shown in the look editor panel";
    effect.author      = "Your name";

    // ... manifest (sections 3) ...

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        return fixtures.map(function(f) {
            return { /* intent — section 6 */ };
        });
    };

    return effect;
})()
```

The file must be a **pure expression** evaluating to the effect object (note the
trailing `()` with no semicolon issues). Standard JS is available; there is **no**
`require`, DOM, filesystem, or wall-clock — see Design Tenets (section 8).

---

## 3. Manifest fields

All manifest fields are optional except `name` and `tick`. Declaring a field is
how it shows up in the look editor and how the host knows what to inject.

| Field | Type | Purpose |
|---|---|---|
| `apiVersion` | int | API version (current: `1`). |
| `name` | string | Display name in the effect picker. |
| `description` | string | One-line tooltip. |
| `notes` | string | Long help shown in the editor panel. |
| `author` | string | Credit. |
| `fixtureTypes` | string[] | Capability tags the effect targets: `"moving"`/`"mover"`/`"pantilt"`, `"rgb"`/`"rgbw"`/`"color"`, `"dimmer"`, `"shutter"`, `"colorwheel"`. The host **filters** the scene's fixtures to those matching at least one tag. Empty/omitted = all fixtures. |
| `dataChannels` | string[] | Real-time host data channels to subscribe to (e.g. `["joystick"]`). Arrive as `data.<name>` each tick. |
| `inputs` | InputDef[] | Named 0..1 control slots (joystick axis, MIDI CC, OSC…). |
| `palettes` | PaletteDef[] | Named palette references the host resolves and injects as `palettes.<name>`. |
| `targets` | TargetDef[] | Named stage-target references; host injects per-fixture aim as `fixture.aimAt[name]`. (Followspot no longer uses this — it reads the scene's look implicitly via `fixture.sceneTarget`.) |
| `parameters` | ParamDef[] | User-editable params shown in the look editor. |

```js
effect.inputs = [
    { name: "x", description: "Pan axis",  defaultValue: 0.5 },  // 0..1, default 0.5
    { name: "y", description: "Tilt axis", defaultValue: 0.5 }
];

effect.palettes = [
    { name: "color",  type: "Color",  optional: true },   // type = a QLCPalette type
    { name: "dimmer", type: "Dimmer", optional: true }
];

effect.targets = [
    { name: "center", description: "Aim point", optional: true }
];

effect.parameters = [
    { name: "speed", description: "Scale", min: 0.1, max: 3.0, defaultValue: 1.0 },
    // 'values' present → integer-index dropdown:
    { name: "mode",  description: "Mode",  defaultValue: 0, values: ["A", "B", "C"] },
    // type:"path" → XY drawn-path widget; value is a JSON array [[x,y],...] of 0..1 pairs:
    { name: "path",  description: "Movement path", type: "path" }
];

effect.dataChannels = ["joystick"];
```

---

## 4. The tick function

```js
effect.tick = function(fixtures, inputs, palettes, params, state, data) { ... }
```

Called every ~50 Hz tick while the scene runs. It is a **pure function** of its
arguments (sections 5) and **returns an array of intents** (section 6), **one
entry per fixture, in the same order as `fixtures[]`**.

---

## 5. Arguments

### `fixtures` — array of fixture descriptors

One object per (filtered) fixture in the scene. Fields:

| Field | Type | Notes |
|---|---|---|
| `id` | int | Fixture id. |
| `head` | int | Head index (0 = single-head). |
| `panRange` | number | Pan travel in **degrees** (from physical, default 360). |
| `tiltRange` | number | Tilt travel in **degrees** (default 270). |
| `hasPanTilt` | bool | Has pan and/or tilt channels. |
| `hasRGB` | bool | Has additive R **and** G **and** B. |
| `hasColorWheel` | bool | Has a colour wheel channel. |
| `hasDimmer` | bool | Has a master/intensity dimmer. |
| `hasGobo` | bool | Has a gobo channel. |
| `hasShutter` | bool | Has a shutter/strobe channel. |
| `pos` | `{x,y,z}` | 3-D rig world position in **metres** (`{0,0,0}` if unplaced). |
| `aimAt` | `{ <slot>: {pan,tilt} }` | Per-fixture aim **degrees** toward each bound `effect.targets` slot. Empty object per slot when unbound/unsolvable. |
| `sceneTarget` | `{pan,tilt}` | Per-fixture aim **degrees** toward the scene's **own Aim look** (implicit target). **Present only when the scene carries an Aim look.** Use it to detect "this scene aims somewhere" (the followspot defers to the host's converged aim when this is present). |
| `lastSpot` | `{pan,tilt}` | Pan/tilt **degrees** where this fixture's beam was left by the previous Operate tick (possibly a different scene). Absent if no prior position. Enables `followMode = lastPosition` handoffs. |

### `inputs` — control values

Each declared input slot as a **0.0–1.0** float (or its `defaultValue` when
unbound). Plus host-injected timing, always present:

| Key | Meaning |
|---|---|
| `inputs.<name>` | declared input slot value, 0..1 |
| `inputs._time` | seconds since the scene started |
| `inputs._beat` | 0.0–1.0 sawtooth phase within the current beat |
| `inputs._bpm` | current BPM (0 = no beat source) |
| `inputs._beatCount` | integer beats since start |

### `palettes` — resolved palette references

For each declared `effect.palettes` slot, a flattened value object (or empty):

| Palette type | Shape |
|---|---|
| `Color` | `{ r, g, b }` (0..255) |
| `Dimmer` | `{ dimmer }` (0..1) |

### `params` — user parameter values

`params.<name>` — scalar number for normal/dropdown params (dropdowns give the
**integer index**). `type:"path"` params arrive as a JS array of `[x,y]` pairs
(0..1).

### `state` — persistence

A plain object that **persists across ticks** for this instance and is **reset
on scene start**. Use it for accumulators (velocity setpoints, phase, strobe
cycle). It is the only mutable carry-over; everything else is recomputed.

### `data` — real-time channels

For each subscribed `effect.dataChannels` name, `data.<name>` is an object. The
host-provided **`joystick`** channel:

| Key | Meaning |
|---|---|
| `data.joystick.pan` | pan axis 0..1 (0.5 = centre) |
| `data.joystick.tilt` | tilt axis 0..1 |
| `data.joystick.sensitivity` | global sensitivity (device/I-O Manager) |
| `data.joystick.deadzone` | global deadzone (fraction; default 0.05) |

---

## 6. Return value — intents

Return **one object per fixture**, same order as `fixtures[]`. Each may contain
any of the keys below. **An omitted key means "don't touch"** — the scene's own
palette retains that channel. Return `{}` to drive nothing for a fixture.

| Key | Units | Effect |
|---|---|---|
| `pan` | degrees (0..panRange) | Pan position. Converted via the fixture's pan range → DMX (16-bit aware). |
| `tilt` | degrees (0..tiltRange) | Tilt position. |
| `dimmer` | 0.0–1.0 | Master intensity channel. |
| `r`,`g`,`b`,`w`,`a`,`uv` | 0–255 | Additive colour channels (Red/Green/Blue/White/Amber/UV). If `r/g/b` are given but the fixture has **no** additive RGB, the host falls back to the **closest colour-wheel slot**. |

> Units recap: **degrees** for angles, **0..1** for normalized levels, **0..255**
> for colour components — symmetric with what you read (`f.sceneTarget.pan` is
> degrees, `palettes.dimmer.dimmer` is 0..1, `palettes.color.r` is 0..255).

> Note: `gobo`/`shutter` appear in the fixture `hasGobo`/`hasShutter` flags but
> are **not yet wired** as intent keys in the host's intent parser. Adding a new
> intent key is a small, backward-compatible host change (`EffectInstance::parseIntents`).

---

## 7. Execution & composition model

- **Per tick:** `inputs`, `params`, `data`, and `state` carry the run-time
  dynamics. (The position/look architecture caches *subscribed look data* where
  possible; see `POSITION_ARCH.md`.)
- **Layering:** output is Override-priority, on top of the scene's palettes.
  Omit a channel and the scene keeps it — this is how a position-only effect
  leaves colour/dimmer alone.
- **Both modes:** scripts tick in Edit (on the previewed scene) and Operate.
- **The "defer" pattern:** when the host can drive something better than the
  script (e.g. multi-head convergence on a moving target), the script returns an
  **empty intent** for that fixture and lets the host/scene drive. The followspot
  does this whenever `f.sceneTarget` is present.
- **Position-only effects** must never emit colour/dimmer; **colour effects**
  must never emit pan/tilt — so they compose.

---

## 8. Design tenets (binding on every script)

- **Pure & testable:** `tick` is a pure function of `(fixtures, inputs,
  palettes, params, state, data)`. No globals, no DMX, no filesystem, and **no
  wall-clock** — `Date.now()`/`Math.random()` are unavailable/forbidden. Drive
  time from `inputs._time`/`inputs._beat`; drive any "randomness" from a seed in
  `state` or the fixture index. This keeps scripts runnable in a harness.
- **High-level intent, not DMX:** emit degrees/levels/colours; the host converts
  to channels (16-bit, colour-wheel fallback, ranges). Never compute DMX
  addresses.
- **Readable & single-purpose:** a small manifest + one `tick`. Prefer several
  small effects over one do-everything effect.
- **Backward-compatible:** adding a new input/param/data-channel/intent key must
  not break existing scripts. Version via `apiVersion`.
- **Consistent units** (section 6), symmetric between read and write.

---

## 9. Worked examples

### A. Position → intensity (subscribe to look domains)

Dim fixtures the further upstage they aim. Reads the scene's aim, writes dimmer
only (leaves colour/pan/tilt to the scene).

```js
(function() {
    var effect = new Object;
    effect.apiVersion = 1;
    effect.name = "Dim by depth";
    effect.description = "Dim fixtures as their aim moves upstage";
    effect.tick = function(fixtures, inputs, palettes, params, state) {
        return fixtures.map(function(f) {
            if (!f.sceneTarget || f.sceneTarget.tilt === undefined) return {};
            // crude proxy: more tilt-from-centre => deeper => dimmer
            var depth = Math.abs(f.sceneTarget.tilt - f.tiltRange / 2) / (f.tiltRange / 2);
            return { dimmer: Math.max(0.1, 1.0 - depth) };  // 0..1, dimmer only
        });
    };
    return effect;
})()
```

### B. Colour strobe (time-driven, state cycle, no globals)

```js
(function() {
    var effect = new Object;
    effect.apiVersion = 1;
    effect.name = "Colour strobe";
    effect.description = "Alternate two colours each strobe cycle";
    effect.parameters = [
        { name: "rate", description: "Strobes/sec", min: 1, max: 20, defaultValue: 8 }
    ];
    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var rate = params.rate || 8;
        var on = (Math.floor(inputs._time * rate) % 2) === 0;  // time from host
        return fixtures.map(function(f) {
            return on ? { r: 255, g: 0, b: 0 } : { r: 0, g: 0, b: 255 };
        });
    };
    return effect;
})()
```

### C. Joystick free-fly (data channel + state setpoint)

```js
effect.dataChannels = ["joystick"];
effect.tick = function(fixtures, inputs, palettes, params, state, data) {
    var js = data && data.joystick;
    var x = js ? js.pan  : 0.5, y = js ? js.tilt : 0.5;
    var dz = (js && js.deadzone) || 0.05, sen = (js && js.sensitivity) || 1.0;
    function vel(v){ var d=v-0.5; return Math.abs(d)<dz?0:(d>0?1:-1)*((Math.abs(d)-dz)/(0.5-dz)); }
    var xv = vel(x), yv = vel(y), dt = 0.02;
    return fixtures.map(function(f) {
        if (!f.hasPanTilt) return {};
        var pk="p_"+f.id, tk="t_"+f.id;
        if (state[pk]===undefined){ state[pk]=0.5*f.panRange; state[tk]=0.5*f.tiltRange; }
        state[pk]=Math.min(f.panRange, Math.max(0, state[pk]+xv*f.panRange*sen*dt));
        state[tk]=Math.min(f.tiltRange,Math.max(0, state[tk]+yv*f.tiltRange*sen*dt));
        return { pan: state[pk], tilt: state[tk] };  // degrees
    });
};
```

See `resources/effectscripts/followspot.js` for the full reference effect
(joystick subscription, implicit scene target, defer pattern) and the other
shipped scripts for colour/position patterns.

---

## 10. Gotchas

- **Order matters:** the returned array must match `fixtures[]` order and length.
  Returning fewer entries silently drops the tail.
- **`fixtureTypes` filters the array:** if you tag `["moving"]`, wash fixtures
  won't appear in `fixtures[]` at all.
- **Stage/azimuth convention** (for position effects): degrees are clockwise from
  downstage — `pan/tilt` are fixture-local degrees the host maps to DMX; floor
  positions in `pos`/aim are metres with `+x` = stage-right, `+y` = upstage.
- **Don't fight the scene:** emit only the channels you own; omit the rest.
- **No `Date`/`Math.random`:** use `inputs._time`, `inputs._beat`, `state`, or
  the fixture index instead.
