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

### The follow-spot stick

Decided (can change): the stick moves the followed target **in the live
cue's blocking**, as a Studio drag in that set would
(`MonitorProperties::setTargetPositionIn()`): in Default it moves the
target, in "Apron number" only the target's offset there. When the cue
ends it glides back to where that blocking has it. A wand held by a
person (`PixelPath::heldCentre`) follows the person's blocked place.

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

## Props someone holds (2026-11-14)

Asked for: "for props like staff and others .. can we assign to a person
to 'hold' and would move with them .. should be cued as to when they get
the prop and when it's taken from them" and hand-held IR candles, "just
that they have them on cue and they lose them on cue". Decided with the
user: who holds a prop is part of Blocking; a held prop changes nothing
an Aim covers ("given it's lit it won't matter"); a candle starts full on
every On and can be dimmed.

- **Which kinds.** `StageTarget::isHandPropKind()`: **Staff** and
  **Candle**, two new target kinds (no generic "prop" kind existed; the
  stage props -- Steps, platforms, scenery, the Box, the fountain -- are
  structures, not targets, and nobody carries them). A Person is never one,
  and only a Person holds, so a hold can never loop. The wand stays a pixel
  fixture with its own `heldBy` (FOUNTAIN_WAND.md); it now reads its holder
  through the same `MonitorProperties::holderFeet()`.
- **Model.** `StageTarget::Blocked` gains `hold` (Unset, OwnPlace, HeldBy,
  OffStage) and `holder`. Default's hold is kept as `m_blocked[0]` (only a
  hold: Default's place is the target itself). `StageTarget::holdIn(set)`:
  the set's own hold, else Default's, else OwnPlace -- so a set that says
  nothing about a prop does what Default does, as with places.
- **One reader.** `targetPosition()` of a held prop is
  `handPoint(holder, blocking)`: the holder's place in that blocking, feet
  on the deck under them, a hand to their right (-X, 0.25 m) and 0.1 m
  in front, at 55% of their height. `targetHolder()` and `targetOnStage()`
  say who has it and whether it is drawn. The check is a kind compare, so
  no other target's read changes; nothing new runs per frame.
- **Cue tracking** is the blocking's: a cue that sets a blocking whose hold
  says "held by Ann" is where she gets it; a later cue with a blocking that
  says otherwise is where it is taken (put down, handed on, or off stage).
- **Light.** Aims, area cover and the solvers read the person, never the
  prop: nothing in them looks at a hand prop (tested).
- **UI.** The plot's right-click: Held By ▸ (Its Own Place, Not on Stage,
  each person), Give Each a Prop ▸ (Staff, Candle: one new prop in each
  selected person's hand, one undo step), Lit By ▸ for candles. A prop
  dropped on a person (within 0.6 m of where they stand) goes into their
  hand; dropped elsewhere it is put down there. All in the blocking the
  plot shows. Edit Target's Subject offers Staff and Candle; a staff's
  Height row is its length.
- **Candles** (`CandleRemote`, engine/src/candleremote.h): one Generic >
  IR Candle Remote fixture (Select + Window, the blaster's chart). What a
  candle shows comes from the Select channel as the plot already reads
  fixture output (`Fixture::valuesChanged`, UI thread), a change of Select
  or Window being one button press: On lit full, Off dark, Candle flicker,
  Light steady, Dim one step down, Brighten full. Unknown until the first
  button of the session (or Select's value when the Studio first opens).
  The plot animates the flicker only while it paints the candle; the rig
  view marks it with two sparks. A candle's `litBy` names its remote;
  unset = the only one in the show.
- **File.** `<Blocked Set="2" Hold="held" HeldBy="7"/>`, `Set="0"` for
  Default's, `Kind="staff"`/`"candle"`, `LitBy="12"`. An older build reads
  the kind as a plain mark and skips a Blocked with only a hold (and every
  Set="0"): the prop is a mark at its own Default place.

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
