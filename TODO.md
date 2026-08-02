# TODO — Programmer / Programming-tab work

Active + not-yet-built work for the fork's programming/looks workflow.
**Shipped work has moved to [DONE.md](DONE.md)** (the completed log, with
per-feature detail + deferred/eyeball notes). Add new work here; move an entry
to DONE.md when it ships. See also the session memory under
`~/.claude/.../memory/`.

---

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Timecode slice 3 — auto-fill internal latency** — the packet→DMX figure needs
  plugin-side timestamping; once measured it folds into the offset. *(from Timecode
  calibration in DONE.md)*
- **Audio calibration — active loopback self-test** — play a click on the audio
  output, time the round-trip to detection = true audio latency. Needs an audio-emit
  path (AudioRenderer wants a decoder) and is unverifiable offscreen; the
  editable/seeded detection-latency + ±10 ms nudge cover it meanwhile.
- **Show-length polish** — "End at SMPTE hh:mm:ss" convenience (needs the offset in
  the host menu); bar/beat snapping for the end handle when a BPM is set; clean up
  the end-handle label collision with a marker at the same position (found on the
  spare-machine pass).
- **GUI headful automation** — `screencapture` + `cliclick` driver so Claude can
  drive AND validate real UI (moving this to the spare machine). First task there:
  a `gui-drive.sh` wrapper, then drive the end-handle drag with eyes on real pixels.

---

## Now — rebrand the fork to "qlcconsole" *(app-wide; not started)*

The fork is now firmly a **desktop console** (mouse+keyboard, MIDI, multi-window),
well past the tablet/Android QML flavour — rebrand from QLC+ to **qlcconsole**.
Distinct from the *Lighting Studio* rename (that was just the 2D tool; shipped).
Scope: app/window title, About box, launcher, macOS bundle/package names
(platforms/macos), and decide whether the `qlcplus` CLI binary name changes too.
Keep upstream attribution/license — this is a fork identity, not a takeover.

---

## Now — unify the object editor's fixture management *(design-first; options pending)*

Follow-on to the embedded object-editor canvas (Lighting Studio). Combine the
fixture-properties "edit" dialog (double-click a fixture) and the "Edit Fixtures in
Studio" layout window into ONE object editor. Wants: a left tree of fixtures BY
FIXTURE GROUP as assigned to this object; right-click to add fixtures/groups;
multi-select → create a fixture group (opens the Fixtures-tab head-layout mapping);
drag-drop tree→face to add; distribute/put-on-face via popup or right-click.
**Design options being drafted — pick a direction before building.**

---

## Now — more stage-feature objects *(active; design-first)*

Extend the discrete map-object model (truss / platform / target / power source /
image / studio group) with the common rigging structures below. Shared needs:
placeable, movable, lockable, layerable/groupable, XML round-trip, a fixture-host
role (fixtures mount to them like they do to a truss), and a 2D + derived-3D
representation. Likely a small **StageStructure** base + parametric shapes, reusing
the truss geometry funnel (`fixtureRigPosition`) and the bar-on-truss / studio-group
patterns. Write a short design doc first (à la FIXTURESTUDIO_DESIGN.md).

SHIPPED (2026-07-31, see DONE.md) — the unified **Pipe** + **Stand** + **Tower**
object set, all fixture-hosting, 2D + elevation, editors, XML:
- [x] **Boom** = a vertical **Pipe** (on a stand, hung from a truss, or free);
  fixtures mount up the pipe at height + facing angle.
- [x] **House electric** = a horizontal **Pipe** (same object, orientation flag +
  run angle). Booms/electrics are ONE object now (resolved the bar/boom/pipe
  overlap).
- [x] **Stand** = a distinct placeable base; a pipe stands on it (base derived,
  follows the stand). (Trusses/towers on stands = future.)
- [x] **Trusses with booms** — a pipe parented to a truss (drop-arm), base derived.
- [x] **Tower** (16" sq × 8') with **shelves** at heights; fixtures mount on a
  shelf (towerU/V), derived from the tower.
- Demo: `stage-structures-demo.qxw` (one of each + a fixture on the tower).

Still open (lower priority — non-fixture-hosting scenery): flats, drapes/legs,
set pieces. And: a Stand should also support a truss/tower (only pipes today);
a Tower editor "Cancel" still applies (live-edit); horizontal-pipe fixtures at
per-fixture offsets along the run.

---

## Backlog — not started

### "Look" as the assembly unit (Scene/Collection rethink) *(Branson shower-thought; design-doc-first)*
Don't rename Scene/Collection (breaks traditional QLC users). Instead make the
fork's **Look** first-class: give it (1) an explicit **fixture scope** (the
"subpart vs whole stage" idea, as metadata) and (2) **palette/fixture state as the
BASE that effects/RGBScripts consume** (parameterise effects by the Look's state
vs discrete per-fixture config; old way still works). Continuous with palette-fed
looks + dimmer-as-multiplier scene-base already built. Write a design doc first
(à la FIXTURESTUDIO_DESIGN.md) before code.

### Tiny: mark the MTC-chip section label as show-sourced *(Branson shower-thought)*
The MTC footer chip appends the current show SECTION under the playhead
(`MTC ● …:12:03 · Startup`). An operator could misread the section name as arriving
FROM the MTC stream — MTC carries only the SMPTE clock; the section is a LOCAL show
marker looked up at that clock. Cheap typographic fix: scope the label so it reads
as show-content (e.g. `▸ Startup` section glyph, or `→ Startup` implying "resolves
to"). 1-line change in `App::slotTimecodeStatusChanged` (ui/src/app.cpp, the `ctx`
string). Not behavioral; deferred pending decision.
