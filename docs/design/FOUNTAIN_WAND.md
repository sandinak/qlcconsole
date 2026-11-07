# Fountain fire height, fixtures and pixels in the fountain, and the wand

2026-11-03, for 0.4. Asked for: "fountain needs a fire height setting .. it
should basically start at top of second wall", "we will be placing fixtures
in it .. and a wled set", "we also need wand as a visual .. a 1.2m staff with
leds wrapped around it driven by wled".

Everything here attaches to models that exist: the fountain stays a plot
group (a platform, two `Scenery::Wall`s, a silk-flame fixture), fixtures in
it are deck mounts on its base, pixel layouts are a property of the fixture
(its rig props), and the wand is a pixel fixture laid out along a staff. No
new tab, no new menu item, no new playback path.

## What a fountain is

`Fountain::Parts` (engine/src/fountain.{h,cpp}) recognises one from a plot
group: one platform (the base) and at least one `Wall` with a rounded-U
outline standing on it. The tallest wall is the inner wall, the lowest the
outer. Nothing new is stored to say "this is a fountain"; an old show's
fountains are found the same way. The group is marked an **assembly**
(`MonitorGroup::assembly`, `setGroupAssembly()`), the flag built assemblies
share so a click can take the whole thing.

## Fire height

`FixtureRigProps::flameHeight` (metres, -1 = not set). A silk flame's silk
starts at `Fountain::flameBottom()` -- the inner wall's top when the flame
is in a fountain, its own top otherwise -- and rises `flameHeight`. The
fan unit itself still stands on the base, hidden by the walls. New
fountains get 3 ft. Editing: **Fire height** in the base's editor (the
fountain's editor), in the plot's units.

Migration (on load, once): a flame with no fire height keeps the top of
the silk where it was drawn (its own top + max(0.45 m, 2 x its width)); the
fire height becomes that top minus the new bottom. A flame not in a
fountain keeps exactly its old height.

## Fixtures in the fountain

A fixture dropped or added inside the base's footprint is deck-mounted on
the base and joins the fountain's group (no Shift needed: the fountain is
one object). Over the inner wall's top it sits on the wall
(`deckHeightOffset` = the wall's top above the base). A fixed light (a par)
put there points up at the flame (`fixedAim`, bearing and elevation to the
flame's middle) -- unless it is in the **gap** (`Fountain::inGap()`: inside
the outer wall's inside face, outside the inner wall's outside face, each U
closed across its open back). There it washes the inner wall: aimed at
`Fountain::gapWashPoint()`, the nearest point of the inner wall's outside
face at half its height (2026-11-07, "pars in the gap"). The rig view's silk
takes its colour only from the pars that are not in the gap. The aim is set
when the light is mounted (dropped in); a move within the fountain keeps it. The base's editor lists them under "Fixtures on this
object", as a truss does. Platform moves already carry deck fixtures; the
group carries the rest.

Repair on load: a flame (or any fixture) standing on a fountain's base but
not in its group joins the group.

## Pixel paths

`FixtureRigProps::pixelPath` (`PixelPath::Shape`): None, Inner wall top,
Outer wall top, Base ring, Up the flame (a spiral of `pixelTurns` turns),
Wand, Sprinkle loops (6, appended so older files keep their numbers). `PixelPath::headPositions()` lays the fixture's LEDs along that path in
head order; the generic helpers are `PixelPath::along()` (N points evenly by
length along a polyline) and `PixelPath::helix()`. The plot and the rig view
draw each LED where the path puts it, in its live colour, with a glow scaled
by its level -- the same colours the fixture's heads (or a WLED effect's
LEDs) already have. Pixels light no stage.

## The wand

Pixel path **Wand**: a staff `wandLength` long (1.2 m), its strip a helix of
`pixelTurns` turns from `kWandGrip` (8%) above the butt to the tip
(`PixelPath::wandTurns()`; 0 = Auto: `wandStripLength` of string, or the LED
count at 2 cm spacing as tape, round a staff `wandDiameter` across, 25 mm).
`wandFromTop` puts LED 1 at the tip (the data end), the string running down. Drawn as a thin dark staff with the LEDs on it; in the plot a short
line with coloured dots. No beam.

**Held by** (`heldBy`, a Person target): its middle is at the person's
position, `holdHeight` (1.0 m) up and a hand's reach (0.25 m) downstage of
them, leaning `holdTilt` (20 degrees) forward; it moves with the person.
Unheld it stands upright where it was placed. Per-cue blocking positions do
not exist on main yet; when they land, the held position should read the
target through the same resolver.

Decided (can change): the wand is a pixel path, not a new stage look, so any
pixel fixture can be one without a new menu item.

## Sprinkle loops

2026-11-07, for 0.4.2. Asked for: a WLED fairy string over the gap -- "at a
point along the U it goes from the inner wall's top across to the outer
wall's top, back to the inner, back and forth N times, then runs along the
wall top to the next point".

Pixel path **Sprinkle loops (over the gap)** (`PixelPath::SprinkleLoops`,
needs a fountain). Data on `FixtureRigProps`:

- `sprinklePoints` (6) and `sprinkleSpansPerPoint` (4): with no table, the
  points are evenly spaced round the U at (i + 0.5) / points of its length
  from the stage-right end, and the LEDs are shared out equally between
  points x spans, no transit LEDs.
- `sprinkleSpans`: a list of `SprinkleSpan { firstLed, lastLed, point }`,
  LEDs 1-based and inclusive (the string's physical numbering), `point`
  0..1 along the U's line from the stage-right end. Laid out in LED order,
  clamped to the string, overlaps trimmed (`PixelPath::sprinkleLayout()`).
  LEDs in no span are transit LEDs.

Show file: attributes on the fixture's rig entry, written only when not the
default -- `SprPts`, `SprPer` and `SprSpans="1-30@0.0833;31-60@0.0833"`
(first-last@point, `;` between spans). An older show has none and reads as
the defaults.

Geometry (`PixelPath::positions()`): span g starts on the inner wall when g
is even and on the outer when odd -- the string goes back and forth, and
carries on from whichever wall the last span ended on. A span's inner end
is the inner wall's outside face at its top; its outer end the nearest
point of the outer wall's inside face, at its top. Between them a parabola
(close enough to a catenary at these sags) whose middle hangs below the
straight line by `PixelPath::sprinkleSag()`: **30% of the straight
distance between the two ends, scaled by the span's LED count over the
string's average span, held to 10%..100%**, and never below the base's top.
LEDs go evenly by length along it. Spans that share a point hang
`kSprinklePitch` (5 cm) apart along the wall, centred on it. Transit LEDs
lie evenly along the top of the wall the string is on, from the last
span's place to the next one's (from the stage-right end before the first
span, to the stage-left end after the last). `positions()` returns LED k at
its place, so a WLED's live pixels and effect ports land right whatever
the segments' Reverse flags are.

Inspector (shown only for this path): **Points**, **Spans per point**,
**Edit Spans…** (`SprinkleSpansDialog`: Span, First LED, Last LED, Point at
(%); `PixelPath::checkSprinkleSpans()` refuses overlaps, a span past the
string, or a point off the wall; an empty table is the even layout) and
**Read Segments**: `GET http://<ip>/json/state` (the IP and NoProxy as
`WledConfigCheck` uses them; 4 s transfer timeout), each `seg[]` entry with
`stop > start` one span (`start` + 1 .. `stop`, as WLED's stop is
exclusive), in start order, `spansPerPoint` to a point; the gaps between
segments become transit LEDs. The note under the rows says how it went, or
why not (not a WLED, no answer, no segments), and which segments' `rev`
doesn't match "every second one reversed". Every edit is one undo step
(the inspector's `aboutToEdit`).

Assumed about WLED: `/json/state` has `seg` as an array of objects with
`start`, `stop` (exclusive) and `rev`; unused slots may appear with
`stop` 0 and are skipped. 2D segments' `startY`/`stopY` are ignored.
