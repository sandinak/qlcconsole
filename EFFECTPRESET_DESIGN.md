# Generator / Effect / Bundle / Look — Design

Companion to `EFFECTSCRIPT_API.md`. Describes the Effect preset system and
the Bundle system for portable, shareable look configurations.

---

## Vocabulary

```
GENERATOR  →  EFFECT  →  BUNDLE  →  LOOK
 (the .js)    (preset)   (stack)    (scene)
```

- **Generator** — a `.js` file that computes live DMX output. Lives in
  `resources/effectscripts/`. Few of them; built by developers or power-users.
- **Effect** — a named capability = Generator + pinned params, stored as
  `.json`. Many of them; what designers browse. Created by "Save as Effect…"
  with no JS required.
- **Bundle** — a portable, shareable palette stack (colours + dimmer + effects),
  fixtures not included. One JSON file captures an entire look configuration;
  stamping it onto a scene recreates the full palette stack. Searchable by
  keyword, category, mood, tempo.
- **Look** — the assembled scene: Colour/Dimmer/Effect palettes stacked in
  LTP precedence order.

One-liner: **an Effect is a Generator with settings; a Bundle is a complete
Look without fixtures; you build Looks by stamping Bundles and tweaking.**

---

## Part 1 — Effects

### 1.1 What an Effect file is

A small JSON file. Only `name` and `script` are required.

```json
{
  "name": "Breathe",
  "script": "dimmer-phaser",
  "category": "Dimmer",
  "description": "Slow sine dimmer pulse",
  "params": { "waveform": 0, "rate": 0.2, "minDim": 0.05, "maxDim": 1.0 }
}
```

| Field | Req | Meaning |
|---|---|---|
| `name` | ✓ | Display name. Unique — user Effects override shipped by name. |
| `script` | ✓ | Generator basename (e.g. `dimmer-phaser`). Resolved via `EffectScriptCache`. |
| `category` | – | Picker group (`Color`/`Dimmer`/`Position`/`Beam`). Defaults to Generator's. |
| `description` | – | One-line tooltip. |
| `params` | – | Map of `param-name → value`. Unset params use Generator's `default`. |
| `inputs` | – | Default values for declared input slots (0..1). |

An Effect stamps an Effect palette: sets `scriptPath`, pre-fills
`effectParamValues`. After stamping it's a normal editable palette. The `.qxw`
stores script+params so a look loads correctly even if the Effect JSON is gone.

### 1.2 Versioning & compatibility

- **Add a param** → additive; old Effects don't set it, Generator's `default` applies.
- **Add an enum option** → append only — never reorder (breaks stored indices).
- **Rename a param** → declare `aliases` on the Generator. `EffectInstance` maps old names at init.
- **Remove a param** → stale key silently dropped.
- **Semantic change** → bump `effect.version` on the Generator.

### 1.3 Shipped Effects

**Dimmer** (`dimmer-phaser`, `native-strobe`): Breathe, Strobe, Ramp Chase, Shutter Strobe, Stagger Strobe

**Color** (`warm-cool`, `uv-pulse`, `amber-pulse`): Warm Wave, Cool Wave, Temperature Chase, UV Wash, UV Chase, UV Strobe, Candlelight, Amber Fire, Amber Chase

**Beam** (`zoom-pulse`, `iris-pulse`, `gobo-chase`): Zoom Wave, Zoom Burst, Iris Pulse, Iris Pop, Gobo Chase, Gobo Spin

### 1.4 Picker layout

Grouped combo: Category → Generator separator → Effect bullets (indented `■`).
Selecting an Effect stamps params + records `<EffectPreset>` for picker identity.
Selecting a raw Generator starts from defaults.

Auto-path: `Palettes/Effect/<Category>/<Generator>/` mirrors the picker.

### 1.5 Where Effect files live

```
~/Library/Application Support/QLC+/EffectPresets/   — user (wins on name collision)
<app>/Resources/EffectPresets/                       — shipped
```

---

## Part 2 — Bundles

### 2.1 What a Bundle is

A Bundle is an ordered snapshot of palette values — colours, dimmer, effects —
that fully describes a look configuration without fixture targets. It is:

- **Portable** — contains resolved values, not palette IDs. Works in any workspace.
- **Self-contained** — a single JSON file is everything you need to share a look.
- **Fixture-agnostic** — stamping creates palettes targeted at the scene's existing fixtures.
- **Versioning-free** — values are baked, so no parameter migration needed. Effect entries inherit the Generator's alias compatibility transitively: if a Generator renames a param, `EffectInstance`'s alias resolution handles it at stamp time, same as `.qxw` loading.

```json
{
  "name": "Sunset Gradient",
  "description": "Warm orange-to-blue spatial gradient with slow amber breath",
  "author": "Branson",
  "created": "2026-06-30",
  "version": 1,
  "keywords": ["sunset", "warm", "outdoor", "evening", "gradient"],
  "attributes": {
    "category": "Color",
    "contains": ["color", "effect"],
    "tempo": "slow"
  },
  "palettes": [
    { "type": "Color", "name": "Sunset Orange", "color": "#ff5e2c" },
    { "type": "Color", "name": "Night Blue",    "color": "#1b2a6b" },
    { "type": "Effect", "name": "Spatial Gradient",
      "script": "spatial-gradient", "params": { "angle": 0 } }
  ]
}
```

### 2.2 Bundle metadata fields

| Field | Req | Meaning |
|---|---|---|
| `name` | ✓ | Display name. User bundles override shipped by name. |
| `description` | – | One-paragraph explanation shown in the browser. |
| `author` | – | Creator attribution. |
| `created` | – | ISO date (`YYYY-MM-DD`) when first saved. |
| `version` | – | Incrementing int; bump when re-saving an existing bundle. |
| `keywords` | – | Free-form array of strings. Full-text searched in the browser. |
| `attributes.category` | – | `Color`/`Dimmer`/`Position`/`Beam`/`Mixed` — primary character. |
| `attributes.contains` | – | Array: `["color","dimmer","effect","position","beam"]` — what palette types are included. |
| `attributes.tempo` | – | `slow`/`medium`/`fast` — motion/pulse speed character. |
| `attributes.mood` | – | Free text: `warm`/`dramatic`/`subtle` etc. |

### 2.3 Palette snapshot types

| type | Fields |
|---|---|
| `Color` | `name`, `color` (hex `#rrggbb`), optional `wauv` (hex for white/amber/uv) |
| `Dimmer` | `name`, `value` (0–255) |
| `Effect` | `name`, `script` (Generator basename), optional `effect` (preset name), `params` (map) |
| `PanTilt` | `name`, `pan` (degrees), `tilt` (degrees) |
| `Beam` | `name`, `focus` (0–255), `frost` (0–255), `iris` (0–255) |

### 2.4 Stamp operation

When a Bundle is dragged onto a look group (or "Stamp to Look" is invoked):

1. For each palette entry in order:
   - **De-dup**: search Doc for a matching existing palette (same type + same values). Reuse if found.
   - **Create**: if none found, create a new palette with the entry's values, targeted at the scene's fixtures.
2. Append the palette IDs (reused or new) to the scene's palette stack in Bundle order.
3. Emit `sceneModified`.

De-dup avoids spawning "Blue #3" when "Blue" already exists with the same colour.
Effect palettes are never de-duped (params may differ even for the same script).

### 2.5 CRUD

| Operation | How |
|---|---|
| **Create** | "Save as Bundle…" in the look list toolbar → `BundleEditor` dialog captures metadata, lists constituent palettes from the current look. Writes JSON to user bundles dir. |
| **Read** | `BundleCache` scans dirs; `BundleBrowser` panel in ProgrammingManager shows list with live search. |
| **Update** | Right-click bundle → "Edit…" → `BundleEditor` pre-populated; re-saves to same file. |
| **Delete** | Right-click → "Delete Bundle" → removes JSON from user dir (shipped bundles cannot be deleted, only overridden). |
| **Duplicate** | Right-click → "Duplicate" → opens editor pre-filled with " Copy" suffix. |
| **Export** | Right-click → "Show in Finder" — the JSON is the export. |

### 2.6 Search & filtering in BundleBrowser

- **Text search**: matches name, description, author, keywords (case-insensitive substring).
- **Category filter**: combo `All / Color / Dimmer / Position / Beam / Mixed`.
- **Contains filter**: checkboxes `Color / Dimmer / Effect / Position / Beam`.
- **Tempo filter**: combo `Any / Slow / Medium / Fast`.
- Results update live as filters change.

### 2.7 Where Bundle files live

```
~/Library/Application Support/QLC+/Bundles/   — user-authored (wins on name collision)
<app>/Resources/Bundles/                       — shipped examples
```

---

## Part 3 — Implementation status

| Feature | Status |
|---|---|
| `EffectScriptCache` — scan, displayName dedup | ✅ |
| `EffectPresetCache` — scan, JSON, user-override | ✅ |
| Grouped picker (Category → Generator → Effects) | ✅ |
| Stamp on Effect selection; raw Generator fallback | ✅ |
| `<EffectPreset>` XML persistence | ✅ |
| "Save as Effect…" button + dialog | ✅ |
| `maybeAutoPath()` + `organizeEffectPalettes()` | ✅ |
| Generator `effect.version` + param `aliases` | ✅ |
| 20 shipped Effects across Color/Dimmer/Beam | ✅ |
| Intent keys: `w`/`a`/`uv`/`shutterOpen`/`strobeRate`/`focus`/`frost`/`zoom`/`iris`/`gobo` | ✅ |
| `QLCBundle` data model + JSON load/save | ✅ |
| `BundleCache` — scan dirs, user-override, save/delete | ✅ |
| `BundleEditor` dialog — create/edit with metadata | ✅ |
| `BundleBrowser` panel — search, filter, preview | ✅ |
| Stamp-to-look with de-dup | ✅ |
| "Save as Bundle…" in look list toolbar | ✅ |
| Shipped example Bundles | ✅ |
| `lock`/`expose` param visibility in Effect JSON | 🔲 future |
| Version-keyed migration maps for semantic breaks | 🔲 future |
| Per-head expansion for pixel/matrix fixtures | 🔲 future |
