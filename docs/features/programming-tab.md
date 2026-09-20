# Programming Tab

*"Scenes hold looks. Looks are made of palettes."* — the app's own guide text.

## Why

Stock QLC+ builds a Scene by setting raw per-channel DMX values, fixture by
fixture. Want "all the blue washes a little dimmer"? Re-touch every fixture's
channels by hand. Change your mind about the exact shade of blue? Re-touch
every scene that used it.

The Programming tab replaces that with an indirection layer. A Scene stops
being a flat bag of baked DMX values and becomes a declaration of:

- **Targets** — which fixtures or groups this scene affects
- **Looks** — which reusable **palettes** (Color, Dimmer, Pan/Tilt, Aim, Beam,
  Gobo, Shutter, Strobe, Effect) apply to them

Edit a palette once — say, nudge "Deep Blue" a little warmer — and every
scene that uses it updates. It's still an ordinary QLC+ `Scene` function
under the hood, so workspace files stay compatible with the classic Function
Manager / Scene Editor.

## Layout

![Programming tab with a scene's Targets and Looks open](../images/programming-tab-targets-looks.png)

Three panes:

- **Left — Scenes / functions.** A filterable tree of every function
  (Scene/Chaser/Collection/EFX/RGB Matrix/Show) in folders. Single-click
  selects; double-click opens it in the canvas.
- **Center — canvas.** A toolbar (Highlight/Flash/Park/Mark/Auto‑MIB, BPM,
  Snapshot), then either an empty-state placeholder or the loaded editor. For
  a Scene, that's the **Targets** panel (left half) and **Looks** panel
  (right half) shown above. Other function types (Chaser, Collection, EFX,
  RGB Matrix, Show) open their own stock editors here instead. Below the
  canvas, the inline **Look Editor** appears whenever a look is selected.
- **Right — sources.** A **Palettes** tab (filterable, foldered, drag
  source) and a **Bundles** tab, with a permanent **Fixtures &amp; groups**
  tree underneath for dragging Targets.

## Building a look, step by step

1. Open the **Programming** tab.
2. If you don't have any palettes yet, right-click the **Palettes** tab and
   pick a type — Color, Dimmer, Pan/Tilt, Aim, Beam, Gobo, Shutter, Strobe,
   or Effect. It opens straight into the Look Editor for you to set its
   value; drag it into a folder afterward to file it.
3. On the left, pick an existing Scene (double-click) or click **Add** → New
   Scene.
4. **Drag a fixture group onto Targets** to add it as a **dynamic** target —
   its membership is followed live, so adding a fixture to that group later
   automatically pulls it into every scene using it. **Drag an individual
   fixture onto Targets** instead for a **fixed** target with static
   membership.
5. **Drag a palette from the right onto the Looks list.** Dropped on empty
   space, it becomes a base look (painted directly onto the targets).
   Dropped onto an Effect look, a Color/Dimmer palette instead nests
   *under* it, feeding that effect's gradient or master level.
6. Click a look to edit it live in the Look Editor at the bottom — see
   below. Every edit there applies immediately; there's no separate "apply"
   step.
7. Reorder looks with the up/down buttons (list order is paint precedence —
   later entries win on shared channels); remove a look or target with its
   panel's **Remove** button.
8. Save the workspace normally (⌘S / Ctrl+S).

A **Bundle** is a related, separate shortcut: a portable, shareable snapshot
of an ordered palette stack (no fixture targets attached). "Stamping" one
from the Bundles tab onto the open scene replaces its whole look stack in one
action — handy for re-using a "look" across shows without re-dragging every
palette. One level of undo is kept for a stamp.

## The inline Look Editor

![The Color look editor: basic-color swatches, a wheel picker, and R/G/B/W sliders](../images/programming-tab-look-editor-color.png)

The editor shown depends on the selected look's palette type:

| Type | What you get |
|---|---|
| **Color** | A color-wheel picker, a grid of basic-color swatches, and six numbered sliders (1 R · 2 G · 3 B · 4 White · 5 Amber · 6 UV — matching a physical-fader numbering convention). Amber/UV only appear if your target fixtures actually have those emitters. |
| **Dimmer** | A gradient bar marked with the fixture's actual intensity capability regions (e.g. strobe bands), with a live capability-name readout. |
| **Pan/Tilt** | An XY pad plus Home/Down/Front/Left/Right preset buttons — the same raw pan/tilt pair applied to every target, regardless of where it's physically rigged. |
| **Aim** | A combo box listing your **Stage Targets** instead of raw angles — each fixture's actual pan/tilt is computed from its rig position, so fixtures in different spots converge on the same point. Needs fixtures placed in Lighting Studio first. See [Stage Targets](stage-targets.md). |
| **Beam** | Focus / Frost / Iris sliders. |
| **Gobo / Shutter** | A slider plus a picker of the target fixture's actual named capabilities, with thumbnail previews. |
| **Strobe** | A single rate slider (0–100%). |
| **Effect** | A picker of built-in presets or raw `.js` scripts, a dynamically-built parameter panel, and Save/Export/Import for sharing `.qxfx` effect files. |

Every type except Effect also shows a per-look **Fade in / Fade out**
override (leave at "step" to inherit the scene or chaser's own timing).

Red warning text appears if a look's type has no matching channel on any of
its targets (e.g. a Gobo look applied to fixtures with no gobo wheel), and —
for Aim looks specifically — the Targets panel flags in red which fixtures
physically **can't reach** the point they're aimed at (see
[Stage Targets](stage-targets.md#reachability)).

## Design vs. Operate mode

There's no preview toggle local to this tab — it follows the app's global
Design/Operate mode:

- **Design mode**: the open scene plays live so you can see what you're
  building, but edits don't disturb a running show elsewhere.
- **Operate mode**: the scene runs as the actual show output.

The 2D Monitor plot, if open, stays in sync with whatever scene is open here.

## Saving your work

Ordinary look edits (color, dimmer, gradient, gobo, targets…) apply live —
there's nothing to "commit," just save the workspace as usual.

The one exception: dragging an Aim target's position or nudging a Pan/Tilt
palette with a joystick/HID controller in Design mode needs an explicit
**Save Positions** button (a two-click "arm, then confirm" action) before it
persists to the file — because that's the one edit path that isn't already
fully live.

**Snapshot** (toolbar button) is a different thing entirely: it bakes the
scene's *current live DMX output* into it as static values — useful for
capturing a look you built live on an external console.

## Terminology

| Term | Meaning |
|---|---|
| **Look** | A palette as applied inside a specific scene. |
| **Palette** | A reusable, named preset (Color, Dimmer, Pan/Tilt, Aim, Beam, Gobo, Shutter, Strobe, Effect) that expands per-fixture at run time. Lives in its own tree, independent of any scene. |
| **Target** | A fixture or group a scene's looks apply to — **dynamic** (a group, membership tracked live) or **fixed** (an individual fixture, static). |
| **Base look** | A look positioned before any Effect in the list — paints fixtures directly. |
| **Nested look** | A Color/Dimmer look placed *under* an Effect — feeds that effect's gradient/master instead of painting fixtures itself. |
| **Bundle** | A portable, target-free snapshot of an ordered palette stack, "stampable" onto any scene. |
| **Snapshot** | Bakes a scene's current live output into it as static values. |
