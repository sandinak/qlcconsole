# Offload: GUI-thread work that could move to workers

Status: an audit, not a plan (rigperf, 2026-11-03). Nothing here is built;
the user picks. Each entry says what it costs now, what moving it could gain,
what it risks for the picture (fidelity) and for the DMX (jitter), and the
effort.

## Rules any move follows

- **The DMX comes first.** Workers run at `QThread::LowPriority`, are
  capped to `SystemProbe::performanceCores()` (fewer in Operate) and never
  take a lock the MasterTimer or a universe thread takes. They read a
  snapshot taken once per frame on the GUI thread, never the live engine.
- **The same picture.** A threaded path draws exactly what the
  single-threaded one did. A test compares the two pixel for pixel, as
  `RenderPerf_Test::beamShortcutsDrawTheSamePicture` does for beam landings.
- **Measured before and after** on Dance Hysteria 0.6 (210 fixtures),
  1280×800, with the DMX tick interval p99 recorded alongside. A worse DMX
  p99 is a failure, whatever the frame gains.

## Already off the GUI thread

| Work | Where | Notes |
|---|---|---|
| Fixture geometry, beams and pools for the 3D/rig view | `drawFixturesInParallel()` | runs of fixtures on the view's pool, merged in order |
| CPU rasterising (no graphics card, or GPU off) | `flushOps()`, bands of rows | each band runs every op in the same order |
| GPU rasterising | `RhiRaster` (Metal on macOS; Vulkan, D3D or OpenGL by Qt's QRhi elsewhere) | the CPU path is the fallback |

The user's "lots of CPUs but only using 1" was the frame budget, not
missing threads: it held the view to 40% of one core whatever the machine
(fixed: a share of the performance cores less two; Operate keeps the old
share and 15 fps).

## Candidates

Costs are CPU ms per frame or per run on his show on an M4 Max under load
(ender) unless said; "frame" is a frame of the rig view while values change.

| # | Work on the GUI thread | Cost now | Gain if moved | Fidelity risk | Jitter risk | Effort |
|---|---|---|---|---|---|---|
| 1 | GPU frame preparation: vertex lists from the ops and the CPU light compose (`gpuRender()`, `ZRaster::composeLight()`) | 10-13 ms a frame (the largest serial part left) | most of it: vertices can be built per op batch in parallel; the compose is per row | low: same vertices, same order | low (pool is low priority) | medium |
| 2 | Structures (`drawOneStructure()` for decks, trusses, scenery) | 1-2 ms | small | low | none | low |
| 3 | The live snapshot and overlap warnings | ~1 ms (overlap: twice a second) | small | none | none | low |
| 4 | Lighting Studio 2D plot redraw (`MonitorGraphicsView`) | 23 ms a frame on the 240-fixture rig (`gui_plot2d_frame_240fx`) | large while the plot is open; QGraphicsView items paint on the GUI thread, so it means caching item pixmaps per value or a custom painter on a pool | medium: item stacking order and selection must match | low | high |
| 5 | Gobo mask preparation (`BeamOptics::opticsFor()`, a 64×64 mask per fixture per frame) | well under 1 ms a frame (now once per fixture, not per head) | small now; a cache per (fixture, gobo values) would remove it | none | none | low |
| 6 | Show Review checks | not measured on his show | runs on edits, not per frame; a worker would keep a big show's edit responsive | low (results are data) | none | medium |
| 7 | AreaCover coverage sampling | not measured; on aim edits | as 6 | low | none | medium |
| 8 | Show loading and parsing | 2.1 s on dink, 9-18 s on ender under load | the window could paint while it loads; parsing itself is serial XML | medium: Doc must not be seen half-built | none (no show running) | high |
| 9 | Layout and plot overlays (rulers, labels, target readouts) | ~0.3 ms | none worth it | — | — | — |
| 10 | Web server JSON (`webaccess`) | not measured | per request; already off the timer | low | low | medium |

Recommended order if any: 1 (the biggest serial cost of a GPU frame), then 5
(trivial), then 4 if the 2D plot is used during shows.

## Measured for this audit (2026-11-03)

His show, 1280×800, Smooth, GPU, beams on, playing cue 1.1.2. Per frame,
wall / process CPU, and the interval between frames while values change:

| Machine | Before (6a4fa9ed3) | After |
|---|---|---|
| dink (M4, 4 performance cores) | 48 / 78 ms, a frame every 245 ms | 23 / 45 ms, every 56 ms |
| ender (M4 Max, under load) | 70 / 107 ms, every 635 ms | 57 / 27 ms, every 134 ms |

The DMX tick interval p99 stayed at 20.1-20.4 ms in every phase, before and
after, with the view open, hovered, and closed, in Design and in Operate.
