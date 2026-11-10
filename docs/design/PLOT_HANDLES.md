# Plot handles

Asked 2026-10-23 (TODO "Lighting Studio: handles on the plot"; STRATEGY
"Plot handles (drag ends, ⌥-rotate)"). With the plot unlocked, a selected
object is changed by grabbing it, not only through its editor dialog.

## Where we start

Most of it is built (0.4.2, `ui/src/monitor/shapehandles.*`,
`monitorgraphicsview_shapes.cpp`): a platform, a truss or scenery selected
on its own in Top gets a square on each end and side, a knob to turn it,
a dashed preview and a readout; one drag is one undo; any lock takes the
handles away. Line targets have their own end and mid-point handles, an
image its corner squares, the stage marks their own drags. This pass fills
the gaps the ask names: **pipes**, **re-angling by an end**, **snapping an
end to other ends**, and **saying why a locked object has none**.

## Which objects get which handles (Top view, Design)

| Object | Handles |
|---|---|
| Platform, riser, deck | ends, sides, corners stretch; knob turns (steps: across the treads only) |
| Truss (free, horizontal) | two end squares; knob turns |
| Pipe / electric (free, horizontal) | two end squares; knob turns **(new)** |
| Wall, flat, leg, border, drape (scenery) | two end squares; knob turns |
| Cyc | two end squares only (it hangs square across the back) |
| Several selected / a group | the knob alone, on the box round them |
| Line target | its own end and mid-point handles (unchanged) |
| Image | corner squares, centre held (unchanged) |
| Stage edges, centre line, plaster line | their own drags (unchanged) |
| Boom, stand, tower, a bar on a truss or pipe, a truss on a stand | none: a point on the plan, or it follows what it is on; move by dragging, heights in Front/Side and the Structure Studio |

A truss or pipe hung on something else follows its parent and has no handles.

## The end of a line: drag to stretch or swing

A truss, pipe or piece of scenery is a **line** on the plan. Drag an end
square and the end goes where the pointer is while the other end stays
put, so the line lengthens, shortens and **re-angles** in one drag (it
swings about the far end). The knob still turns it about its middle.

**⌥/Alt has one meaning on every handle: free of the snaps.** On a deck's
squares and the knob that was already so; on a line's end it lifts the
grid, the straight-line detent and the catch on another end. (A first cut
used ⌥ on an end to mean "swing"; the UX review (UX-66) took it back so ⌥
never means two things.)

A cyc's ends only stretch (it hangs square across the back and cannot
turn).

A hover over a handle says what a drag does and its keys: on a line's end
"Drag to stretch or swing · Shift: 15° steps · ⌥: no snap" (UX-61), on a
deck's square "Drag to stretch · ⌥: no snap", on the knob "Drag to turn ·
⌥: no 15° steps".

## Snap

- A deck's **stretch** goes in Snap steps (plot ▸ Snap); off with Snap off
  or with ⌥.
- A **line's end** snaps, first match wins (none of them with ⌥):
  1. **to another end** within about 10 px on screen: the ends of every
     other truss, pipe and piece of scenery and a deck's corners, so a wall
     meets a wall and a pipe meets a truss end exactly;
  2. **Shift**: the angle in 15° steps, the length in Snap steps;
  3. **straight**: within 3° of the way it faced, it keeps its angle and
     stretches along itself in Snap steps (a detent, so a stretch stays a
     stretch);
  4. otherwise **to the grid**: the end on the nearest Snap point (free with
     Snap off).
- The knob turns in 15° steps (⌥ free), as before.

## The readout

While dragging, a readout by the pointer gives the length in **Settings ▸
Units** through `MeasurementUnits::formatMetres()` (`Length 30'`,
`16' × 8'`); a line turned off its own angle adds the way it faces
(`Length 12' 6" · 45°`), and "→ end" when it has caught another end. Never
a hard-coded unit.

## Undo

One drag is one step on the rig undo (`pushRigUndo` with the snapshot taken
at the press), whatever it changed: length, angle, place, and what rides on
it. Riders keep their distance from the end that stayed put: a truss's
lights, bars and hangers and a pipe's lights and bars.

## Locks

The handles are there only while nothing holds the object: its own lock,
its group's, its layer's, Plot Locked, Operate, Front/Side each take them
away (as today).

**A selected but locked object** **(new)** shows its outline dashed in the
lock red, no squares and no knob, and one line under it saying what holds
it and where to lift it: "Layer locked — unlock it in Plot layers", "Plot
locked — Edit Plot to change it". So a click on a locked wall answers "why
can't I drag this?" without a dialog.

## Keyboard nudge

There is none on the plot today (the arrows scroll it). Not added here;
a candidate for later (arrow = one Snap step, Shift = ten).

## Speed

The handles are one item over the plot, drawn only while one object is
selected. Its pens and brushes are made when the shape is set, not in
`paint()`: drawing them allocates nothing.

## Deferred

- Handles in Front/Side (a truss's height, a pipe's trim by its end).
- A boom's or stand's height by a handle on the plan.
- Snapping a plain stretch to another end on its line.
- Keyboard nudge.
