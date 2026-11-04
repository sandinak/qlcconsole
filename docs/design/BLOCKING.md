# Blocking (board Q211)

> Blocking: named sets of target positions (and line/area shapes) chosen per
> cue and tracked forward; drag on the plot edits the cue's blocking; every
> view, Focus, coverage and Show Review use it. (user, 2026-11-03)

Some numbers put the performers on the apron. With one position per target
the only way to light them there was a second target and a second look.
With blocking, looks keep aiming at the same targets and the targets move
per cue, so one look ("Front Rank warm") works wherever the people stand.

Not "formation": that word is the line spread (Q197/Q206, the Formation
palette). Theatre calls where people stand *blocking*, and its marks *spike
marks*; the course uses both words.

No new tab, no new playback path: it attaches to the stage targets and adds
one tracked cue property.

## Model

- **Blocking sets** live on `MonitorProperties` (`blockingSets()`: id, name).
  Id 0 is **Default**: today's positions, always there, never stored as a set.
- **Overrides** live on each `StageTarget`, keyed by set id
  (`StageTarget::Blocked`): a position *offset*, and for a line its points,
  for an area or kit its footprint. A target with no override in a set stands
  where Default puts it. Deleting a set drops its overrides; deleting a target
  drops its own.
- **One reader.** `MonitorProperties::targetPosition(t, blocking)`,
  `targetLinePoints()` and `targetFootprint()` are the only way to read where
  a target is. `targetAimPoint()` (and so AimSolver, AreaCover, the follow
  spot, beam landing, the plan views, the rig view, Focus and Show Review) goes
  through them. `blocking` defaults to `MonitorProperties::currentBlocking()`.

### Which blocking

- **Live**: `MonitorProperties::liveBlocking()`, an atomic id. A Show Tree
  cue's Collection carries its cue's resolved blocking (set when it is baked,
  not saved); `Collection::preRun()` sets the live blocking before it starts
  its scenes, so the scenes it starts aim at the new marks on their first
  frame. Running scenes that stay up (a look shared with the cue before) are
  re-aimed over the cue's fade in (`Scene::requestReaim(fadeMs)`). The last
  cue to GO wins when two lists run; a hit leaves the blocking alone unless
  it has one of its own.
- **Everything else** (Blind, the Studio plot, plan views, Show Review):
  `BlockingScope` (engine/src/blocking.h) sets a thread-local blocking for the
  duration of a computation on the UI thread. `currentBlocking()` is the scope
  if one is open, else the live blocking. The MasterTimer thread never opens
  a scope, so live output is untouched by what a view previews.
- **Preview** plays the cue's Collection, so it sets the live blocking to the
  edited cue's, the same way GO does.

### Tracking

`Cue::blocking`: invalid (unset) = what the cue before it in the list has; 0 =
Default, explicitly; n = set n. The first cue of a list starts from Default; a
skipped cue is passed over, like everything else that tracks; a sub-cue's
first starts from its parent cue. `ShowTreeController::cueBlocking(cueId)`
resolves it; the bake caches it on the Collection so nothing walks the list
per frame.

### Hot path

Resolving is a hash lookup under the target's existing position mutex: no
allocation per tick. The cue's resolved set is cached on its Collection at
bake time.

## Venues: offsets, not absolute positions

A blocking stores **offsets from Default** for positions. A venue (Q128)
moves a target's Default position (`m_base` holds the show's, `position()` is
the venue's). With offsets, the apron blocking stays "1.8 m downstage of
where they usually stand" in every venue, which is how the blocking is
directed: if the venue's stage is shallower and the venue pulls the mark
upstage, the apron mark comes with it. Absolute positions would leave the
apron mark where it was in the last room, possibly off the edge. With no
venue applied the two are identical.

Line points and footprints are already relative to the target's centre, so a
blocking stores them as they are (a shape, not a place).

## UI

- **Cue page** (Show Tree): a *Blocking* row: a combo (`Tracks (Apron
  number)` for an unset cue, `Default`, each set), *Save Positions as
  Blocking…* (a copy of where they stand in this cue, used from it on) and
  *Rename…*. The same *Save Positions as Blocking…* is in the Studio's
  Blocking menu and on its empty-plot right-click: the crew's "store a formation".
  A cue that changes blocking shows a small flag in the tree's flags column,
  with the set's name in the tooltip.
- **Lighting Studio plot**: a *Blocking ▾* button in the toolbar (not the
  footer: it is full, and widening it moved the plot) (`Default`,
  `Cue in edit (…)`, each set, *Save Positions as Blocking…*). Dragging
  writes the picked blocking for whatever is selected (one target or many);
  how the plot selects and groups is the Studio's own rule. When it is not Default a banner over the plot reads
  "Moving targets in: Apron number" and each moved target leaves a dashed
  ghost at its Default place. In Default, a drag moves the target itself, as
  before.
- **Show Review**: `BLK-1` "A blocking leaves a target where no light can
  reach": for each set a cue uses, the Aim looks at a target with an override
  are checked with the existing reach logic (`AimSolver::reach`) under that
  blocking.

## File format

```xml
<StageTarget ID="3" Name="Front Rank" X="0.000" Y="3.000" Z="0.000">
  <Blocked Set="1" DX="0.000" DY="1.800" DZ="0.000" W="6.000" D="1.500"/>
  <Blocked Set="2"><LinePoint X=".." Y=".." Z=".."/>...</Blocked>
</StageTarget>
<Blocking ID="1" Name="Apron number"/>
```

and `Blocking="1"` on a `<Cue>` that sets one. An older build skips both
(it sees Default everywhere). An old show loads with Default only, and its
output is byte-for-byte what it was (tested).
