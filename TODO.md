# TODO — Programmer / Programming-tab work

Active + not-yet-built work for the fork's programming/looks workflow.
**Shipped work has moved to [DONE.md](DONE.md)** (the completed log, with
per-feature detail + deferred/eyeball notes). Add new work here; move an entry
to DONE.md when it ships. See also the session memory under
`~/.claude/.../memory/`.

---

## Deferred / next candidates *(open slices carved out of shipped features)*

- **3-plane (Top/Front/Side) studio editor** — the graphical authoring surface for
  Fixture Studio; the form editor already edits local X/Y/Z, this is the richer
  canvas (place a step's front-face vs top-deck visually). Biggest single chunk —
  the next major block. *(from the Fixture Studio entry in DONE.md)*
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

## Backlog — not started

### "Look" as the assembly unit (Scene/Collection rethink) *(Branson shower-thought; design-doc-first)*
Don't rename Scene/Collection (breaks traditional QLC users). Instead make the
fork's **Look** first-class: give it (1) an explicit **fixture scope** (the
"subpart vs whole stage" idea, as metadata) and (2) **palette/fixture state as the
BASE that effects/RGBScripts consume** (parameterise effects by the Look's state
vs discrete per-fixture config; old way still works). Continuous with palette-fed
looks + dimmer-as-multiplier scene-base already built. Write a design doc first
(à la FIXTURESTUDIO_DESIGN.md) before code.

### More stage-feature objects *(Branson shower-thought)*
Trusses and platforms should be fully discrete, reusable objects; consider adding
other common stage features as first-class placeable map items: **flats**, **stands
with a cross-bar** (T-bar/tripod), booms, drapes/legs, set pieces. Same scaffolding
as truss/platform/power-source items (placeable, lockable, layerable, XML). Could
share a small "stage feature" base + a library of parametric shapes. Ties into the
Fixture-Studio "component template" idea (a stand-with-crossbar is a fixture host).

### Tiny: mark the MTC-chip section label as show-sourced *(Branson shower-thought)*
The MTC footer chip appends the current show SECTION under the playhead
(`MTC ● …:12:03 · Startup`). An operator could misread the section name as arriving
FROM the MTC stream — MTC carries only the SMPTE clock; the section is a LOCAL show
marker looked up at that clock. Cheap typographic fix: scope the label so it reads
as show-content (e.g. `▸ Startup` section glyph, or `→ Startup` implying "resolves
to"). 1-line change in `App::slotTimecodeStatusChanged` (ui/src/app.cpp, the `ctx`
string). Not behavioral; deferred pending decision.
