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
flame's middle). The base's editor lists them under "Fixtures on this
object", as a truss does. Platform moves already carry deck fixtures; the
group carries the rest.

Repair on load: a flame (or any fixture) standing on a fountain's base but
not in its group joins the group.

## Pixel paths

`FixtureRigProps::pixelPath` (`PixelPath::Shape`): None, Inner wall top,
Outer wall top, Base ring, Up the flame (a spiral of `pixelTurns` turns),
Wand. `PixelPath::headPositions()` lays the fixture's LEDs along that path in
head order; the generic helpers are `PixelPath::along()` (N points evenly by
length along a polyline) and `PixelPath::helix()`. The plot and the rig view
draw each LED where the path puts it, in its live colour, with a glow scaled
by its level -- the same colours the fixture's heads (or a WLED effect's
LEDs) already have. Pixels light no stage.

## The wand

Pixel path **Wand**: a staff `wandLength` long (1.2 m), its strip a helix of
`pixelTurns` turns (0 = from the LED count at 2 cm spacing round a 25 mm
staff). Drawn as a thin dark staff with the LEDs on it; in the plot a short
line with coloured dots. No beam.

**Held by** (`heldBy`, a Person target): its middle is at the person's
position, `holdHeight` (1.0 m) up and a hand's reach (0.25 m) downstage of
them, leaning `holdTilt` (20 degrees) forward; it moves with the person.
Unheld it stands upright where it was placed. Per-cue blocking positions do
not exist on main yet; when they land, the held position should read the
target through the same resolver.

Decided (can change): the wand is a pixel path, not a new stage look, so any
pixel fixture can be one without a new menu item.
