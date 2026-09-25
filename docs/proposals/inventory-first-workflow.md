# Inventory-first workflow — design the rig before you patch it

**Status:** Phase 1 (engine) and the Phase 2 UI are built -- see "What shipped"
below. Still open: the equipment catalog for network gear, badges in the
Programming tab, and proximity-guided patching (needs NetworkTopology N6).
**Owner:** Branson / Claude
**Related:** [Lighting Studio](../features/lighting-studio.md),
[Fixture Manager](../features/fixture-manager.md)

## The ask

Branson: *"I'd like to see if there's a way we can do workflow from an
inventory"* — start from a bill-of-materials of lighting the LD actually has,
place it on the set/studio design, group it, and only **then** backfill it
into real DMX universes — instead of today's order, where a fixture doesn't
exist in the show at all until it's already been given a real universe and
address.

This is a research question — "what would it take" — not a build. Below is
what I found reading the actual code, and a recommendation.

## The good news: almost everything downstream is already decoupled

I went looking for where "universe first" is actually enforced as a real
data-model constraint, expecting to find it baked in deep. It isn't. Once a
`Fixture` object exists in `Doc`, **every** downstream fork feature already
treats it as fully independent of what address it holds:

- **Rig placement** (`FixtureRigProps`, `engine/src/monitorproperties.h`) is
  keyed purely by fixture id — mounting a fixture on a truss, riser, tower,
  or pipe, and everything in [Lighting Studio](../features/lighting-studio.md),
  never once looks at `Fixture::universe()`/`address()`.
- **Fixture groups and folders** (`FixtureGroup`, the head-layout grid,
  [Fixture Manager](../features/fixture-manager.md)) are likewise address-blind
  — a group is a grid of fixture *heads*, not DMX addresses.
- **Programming tab Targets** (drag a fixture or group onto a scene) work
  off fixture ids and group membership, not addresses.
- **Studio groups/frames**, **Stage Targets**, **aim solving** — same story.

So the actual gate isn't "the system requires universes before studio
placement" in general — it's narrower and sharper than that:

## The one real gate: a `Fixture` can't exist at all without a valid address

`Doc::addFixture()` (`engine/src/doc.cpp:1020`) rejects a fixture if its
claimed channel range overlaps another already-patched fixture's — there is
no "no address yet" state in the model. The `AddFixture` dialog
(`ui/src/addfixture.cpp`) is built around that: you pick a fixture
definition **and** a universe **and** an address, together, before a
`Fixture` object comes into being at all.

And critically — I checked — **Lighting Studio's own "Add Fixture" flow
doesn't create fixtures.** `Monitor::studioAddFixture()`
(`ui/src/monitor/monitor.cpp:3543`) only ever offers a picker of fixtures
that already exist in `Doc::fixtures()`. There is no path today to originate
a new fixture from inside the rig view — you always have to go to Fixture
Manager first, which always demands a real address. That's the entire
mechanism behind "set up universes, then patch, then place" being the only
possible order today. It's a single choke point, not a systemic one.

## Three things already in the codebase that make this easy to solve safely

1. **An "unpatched" state already exists — for universes.** The Connections
   tab already treats "a universe exists in the show" and "a universe is
   wired to a real interface" as separate facts — the Devices view literally
   shows universes as `not connected to any interface`, and nothing breaks;
   DMX for that universe just doesn't go anywhere. That's the exact safety
   property a staging area for fixtures needs, and it's proven, shipped
   behavior, not a hypothetical.
2. **Re-addressing an existing fixture is already safe and already built.**
   Editing an existing fixture's Properties reopens the same `AddFixture`
   dialog in edit mode (`FixtureManager::editFixtureProperties()`,
   `ui/src/fixturemanager.cpp:2004`) and refuses to apply if
   `af.invalidAddress()` — i.e. moving a fixture to a new universe/address
   later, once it already exists, is already validated collision-safe. A
   "backfill" step needs no new safety-critical code; it reuses this
   directly.
3. **Auto-picking a free address is already a reusable utility.**
   `AddFixture::findAddress()` (`ui/src/addfixture.cpp:375`) already finds
   the next free contiguous channel block across as many universes as you
   give it — used today for "add N fixtures at once." A bulk "patch
   everything I've staged" action can call this once per fixture against the
   *real* universes only.

None of this needed to be built for this proposal — it's already there,
built for other reasons, and it happens to be exactly what's needed.

## Revision: Option A doesn't survive contact with a real rig

The first draft of this proposal recommended reserving a single staging
`Universe` fixtures would land in by default ("Option A"), and treated a
first-class `isPatched()` flag on `Fixture` itself ("Option B") as a "maybe
later, if Option A proves the workflow is wanted." Branson's pushback —
*"we have way more lights than 1 universe .. especially if you consider
pixel mapped things"* — is correct and breaks that recommendation outright.
Worth walking through why, because the reason matters for which option is
actually right.

A universe is 512 channels, full stop. One pixel-mapped fixture alone can
eat a third of that — this repo's own `monitor_test.cpp` builds a synthetic
"Off-axis Tape" test fixture with 64 pixels × 3 channels = **192 channels**,
a single fixture. A handful of those, plus a normal complement of movers
(12-20 channels each), blows past 512 fast. And this isn't a hypothetical
edge case for this app specifically — the real show file this project tests
against (`test-workspaces/d2d-showfile.qxw`) already patches **66
universes**. If "inventory" is meant to hold a real rig's worth of gear
before any of it is patched, one staging universe is nowhere near enough.

The obvious patch — grow more staging universes as needed ("Inventory 1,"
"Inventory 2," …) — mechanically works (`Doc::addFixture()` already
auto-grows `InputOutputMap`'s universe count to whatever index you hand it)
but trades a capacity problem for a clarity one: those staging universes now
sit in the exact same list — Connections, the Fixture Manager's "Universes"
folder, the address-usage grid — that the LD relies on to reason about their
*real* patch. At the 66-universe scale this app already operates at, a pile
of indistinguishable "is this real or is this inventory" universes is a
worse problem than the one we started with, not a better one.

There's a second, sharper issue Option A glossed over, confirmed by tracing
the actual DMX write path: a freshly-constructed `Fixture` defaults to
`m_address = 0` — universe 0, address 0 (`engine/src/fixture.cpp:46`) — which
is not a safe sentinel, it's a **real, live address**. Nothing in the write
path guards against it: `Universe::write()` (`engine/src/universe.cpp:976`)
only has a `Q_ASSERT(address < UNIVERSE_SIZE)`, compiled *out* of Release
builds (the configuration `check-all.sh` and real releases actually ship),
and it wouldn't catch address 0 anyway — that's in bounds. Confining
"inventory" fixtures to a universe nobody's patched to hardware works only
as long as every single one of them is actually, correctly parked there —
one missed reassignment, one copy-paste of `FixtureRigProps` from a template
that carried a real address, and the safety property is gone. A convention
enforced by discipline isn't a safety property.

Both problems point the same direction: **Option B — a first-class,
persisted "unpatched" state on `Fixture` — is the right design, not the
deferred one.** It doesn't have a capacity ceiling (an unpatched fixture
consumes no address space at all, so "way more lights than 1 universe" stops
being a sizing question entirely), and it doesn't rely on convention for
safety (a fixture is either explicitly patched or it isn't, checked at the
one real chokepoint DMX writes already funnel through).

## What Option B actually requires (traced, not guessed)

I traced the live DMX write path to find out how invasive this really is,
expecting it to be scattered. It isn't — every function type (Scene, Chaser,
Script, RGBMatrix, EFX, the fork's own EffectScriptRunner) already funnels
through one shared pair of classes before a byte ever reaches a `Universe`:

- `Scene::processValue()` (`engine/src/scene.cpp:980`) and every other
  Function's write path ask `GenericFader::getChannelFader()`
  (`engine/src/genericfader.cpp:144`) for a fixture+channel's fader.
- On first request, that builds a `FadeChannel` (`engine/src/fadechannel.cpp:63`),
  which calls `autoDetect()` (`fadechannel.cpp:126`) — this reads
  `fixture->universe()`/`address()` **exactly once** and caches the result.
  Every later tick reuses the cached value; nothing re-resolves live.
- `GenericFader::write(Universe*)` (`genericfader.cpp:201`) walks the cached
  channels and calls `universe->write(address, value, …)` — the literal byte
  write, per the note above, essentially unguarded.

That means the property "an unpatched fixture's channels are never written,
no matter what its default universe/address happens to be" needs exactly
**two** guards, both already-central chokepoints every Function type already
shares:

1. `FadeChannel::autoDetect()` — treat an unpatched fixture the same as the
   existing `fixture == NULL` branch already does (no real channel to fade).
2. `GenericFader::write()`'s per-channel skip — there's already an
   `address == QLCChannel::invalid()` skip in this loop; `FadeChannel::address()`
   just needs to actually report invalid for an unpatched channel instead of
   falling back to a bare channel-number value.

That's it for safety-critical engine work. Everything else — the ~8 other
call sites across `scene.cpp`/`cuestack.cpp`/`script.cpp`/`rgbmatrix.cpp`/
`efx.cpp` that compute a target universe index from the fixture's raw
address before ever reaching the fader — is an optional efficiency/hygiene
early-out, not a safety requirement, since the two chokepoints above already
make the outcome correct regardless.

## The rest of the design, corrected

- **`Fixture::isPatched()`** — a persisted bool, default `true` (so every
  existing file's fixtures stay exactly as they are; nothing silently
  becomes unpatched on reload). Set `false` only by a new "Add to
  Inventory" creation path. `universe()`/`address()` keep whatever default
  they hold (0/0 is fine now that it's provably never written) until the
  fixture is actually patched.
- **No staging universe at all.** An unpatched fixture doesn't occupy a
  slot in *any* universe's 512 channels — Connections, the Universes
  folder, and the address-usage grid show exactly the real patch, nothing
  more, at any scale of inventory.
- **Capacity is a non-issue.** Stage 500 pixel-mapped channels or 50,000 —
  it's just `Fixture` objects sitting in `Doc`, the same as any other
  in-memory object the app already handles thousands of.
- **The backfill/patch step is unchanged from the original proposal**:
  reuses `AddFixture` in edit mode (already collision-safe — see above) to
  assign a real universe/address, one at a time or via a bulk "Patch…" view
  wrapping `AddFixture::findAddress()`. The only difference is what
  "patching" flips: previously a universe/address value that was
  *pretending* to be harmless; now an explicit `isPatched` bit, with the
  address only becoming meaningful at that moment.

## What it would actually take, phased

**Phase 1 — the engine primitive:**

1. Add `Fixture::isPatched()`/`setPatched()`, default `true`; persist to XML
   (omit the attribute for `true` so old/normal files don't change on
   save — only an explicit inventory fixture writes `Patched="0"`).
2. Guard `FadeChannel::autoDetect()` and `GenericFader::write()`'s skip
   logic as traced above. Add an engine test: an unpatched fixture at
   address 0/universe 0, alongside a real fixture actually patched there,
   confirms only the real one's values ever reach `Universe::write()`.

**Phase 2 — the creation and backfill UI:**

1. **"Add to Inventory"** in Fixture Manager's Add menu — the existing
   `AddFixture` dialog with universe/address fields hidden and
   `isPatched(false)` set on accept.
2. **"Create fixture…" from inside Lighting Studio** — `Monitor::studioAddFixture()`
   currently only offers a picker of fixtures that already exist
   (`ui/src/monitor/monitor.cpp:3543`); add the same Inventory creation flow
   there so an LD never has to detour through Fixture Manager to originate
   one. This is the change that actually inverts the workflow order, not
   just permits it.
3. **A docked Inventory tree in Lighting Studio itself** (Branson, discussing
   N2): once fixtures can be staged unpatched, dragging them onto the rig one
   at a time via a picker dialog is the wrong interaction for the common case
   — an LD building a rig from a known inventory wants a persistent side panel
   (right-hand dock, alongside the existing Layers panel on the left) listing
   every `isPatched() == false` fixture, dragged straight onto a truss/pipe/
   riser the way a palette is already dragged onto a look in the Programming
   tab. This is the natural UI home for point 4 below (not just a Fixture
   Manager tree row) — the same underlying "Inventory" list, presented where
   the placing actually happens instead of requiring a detour to Fixture
   Manager. Worth designing as one panel serving both.
4. **A real "Inventory" section** in the Fixture Manager tree (a peer of
   "Fixture Groups"/"Power"/"Universes," per `FixtureTreeWidget`'s existing
   pattern of always-present root sections) listing every `isPatched() ==
   false` fixture — a real, queryable list, not an inferred one.
5. **An "unpatched" badge** wherever a fixture is listed elsewhere
   (Lighting Studio labels, Programming tab target rows).
6. **A "Patch…" bulk view** — everything still unpatched, assign each to a
   real universe/address one at a time (reopens edit-mode `AddFixture`) or
   in bulk via `findAddress()`.

## Connection type: DMX vs Network fixtures

A second persisted attribute rides alongside `isPatched`: `Fixture::connection`
(`Dmx` default / `Network`) plus an optional `ipAddress`, specified in
[NETWORK_CABLING_DESIGN.md](../../NETWORK_CABLING_DESIGN.md#fixture-connection-dmx-vs-network-supersedes-direct).
It matters here because the Inventory and the Patch step both branch on it:

- **Add to Inventory** asks for the connection (and IP for network fixtures)
  up front, since an LD knows a fence is ArtNet before any universe exists.
- **Patch...** for a `Dmx` fixture picks a node port by proximity; for a
  `Network` fixture it allocates or reuses a universe at that fixture's IP
  (unicast -- one universe per distinct IP) and sets `outputIP` in Connections.
- Phase 1 gains one more small engine change: the attribute and its XML
  (omitted when `Dmx`), covered by the same "old file opens unchanged" test.

## What shipped (2026-09-24)

**Engine.**
- `Fixture::isPatched()` (default true; `<Patched>False</Patched>` only when
  staged), `Fixture::connection()` (Dmx/Network) and `ipAddress()`. Nothing is
  written to the XML for an ordinary fixture, and a test proves an old file
  round-trips unchanged.
- `Doc::addFixture()` skips the overlap check, the address book and universe
  growth for a staged fixture, so any number can sit at the placeholder 0/0.
  This was the gap the first draft missed: without it only one inventory
  fixture could ever be added. `Doc::slotFixtureChanged()` and
  `replaceFixtures()` honour it too.
- `Doc::patchFixture(id, universe, address)` -- collision-checked, refuses to
  run past a universe, applies the channel capabilities `addFixture()` skipped.
- DMX write path. The doc said two chokepoints; the real audit found more,
  because this fork's highlight/mark/park/flasher effects, the generic DMX
  source, the effect instance, the script runner and the programmer's live
  write all build a universe index from the raw fixture. Every one now skips an
  unpatched fixture. The fader path is covered by an *inert* `FadeChannel`
  (`address()` invalid, `universe()` invalid): the earlier idea of "treat it
  like `fixture == NULL`" would have been wrong, because a NULL-fixture channel
  falls back to the bare channel number and writes universe 1 at that address.
  `GenericFader::write()` skips inert channels silently.
- `PatchPlanner`: DMX packed first-fit around what is patched, spilling into new
  universes; network fixtures unicast, one universe per distinct IP, fixtures
  behind one IP sharing it, DMX never placed in a network universe. `plan()` is
  pure; `apply()` patches and, for network fixtures, points the universe's
  `outputIP` at the fixture (borrowing the plugin and interface of an existing
  ArtNet/sACN universe when the new one has none).

**UI.**
- Fixture Manager: **Add to inventory...** (the Add dialog in inventory mode --
  no universe/address/gap), an **Inventory** root in the tree, and
  **Patch inventory...** (whole inventory, or the selected staged fixtures).
  Staged fixtures are excluded from every per-universe listing (Universes
  folder, Connections, patch grid, channel selection, power estimate), so
  a placeholder 0/0 never makes one look like it sits in universe 1, and
  deleting a universe never sweeps them up.
- Patch dialog: opens on the planner's proposal, every universe and address
  editable, conflicts flagged live, Apply disabled while any remain.
- Lighting Studio: a right-hand **Inventory** panel (toolbar toggle) listing
  staged fixtures not yet on the plot, grouped by model, filterable. Drag one
  onto the plot to place it -- onto a truss, riser or tower to mount it, by the
  same release logic a dragged fixture gets. Placing does not patch it.
- Everything that shows or drives a fixture by universe/address was audited for
  the placeholder 0/0 (a first pass missed most of these, and a staged fixture
  showed up as "universe 1"): the Studio's DMX and Network overlays, tooltips,
  address-clash flags and inspector labels; Locate, Test orientation, Reset and
  the maintenance buttons (which write straight to the universe -- Locate on a
  staged fixture would have lit whatever real fixture sits at universe 1,
  address 1); the Monitor DMX view; Simple Desk's fixture consoles and scene
  sliders and its cue defaults; the audio bar; DMX dump; VC XY pad and level
  slider; the group layout labels.
- The inventory is organised **manufacturer -> model -> fixture**, alphabetical at
  every level with a count on each folder, in both the Fixture Manager tree and
  the Studio panel. Generic dimmers and RGB panels file under "Generic" with a
  model that says what they are ("Dimmer, 4 ch"). Selecting a manufacturer or
  model folder stands for everything beneath it: drag it from the Studio panel,
  or right-click it in the manager to patch it. `PatchPlanner` follows the same
  order, with numbers compared as numbers ("Wash 2" before "Wash 10"), so
  identical fixtures get contiguous addresses.
- **Naming.** A model's own digits are never a counter: adding a second LM70 gives
  "LM70 #2", not "LM71", and a multi-add of Par 64 gives "Par 64 #1, #2" rather
  than Par 65. Only an explicit `#N` suffix in a typed name ("US #4") continues
  as a counter (`Doc::nextUniqueModelName`, and the multi-add rule in
  `FixtureCreator`).
- **Mode and channels are chosen when patching.** The Add dialog in inventory mode
  has no channel count (a generic dimmer starts at one channel; a real fixture
  starts in the mode preselected there). The Patch dialog has a Mode column -- the
  definition's modes, or a channel spin for a generic dimmer -- and the plan is
  made for the chosen footprint. `Doc::canChangeFixtureMode()` guards it: a
  fixture a scene sets channel-by-channel keeps its mode (scenes store raw channel
  indexes), and a mode with a different head count is refused (fixture groups lay
  out heads by number). Disabled choices are shown with the reason. Palettes and
  looks built from them are by capability and are unaffected. Apply is
  transactional: a refused change leaves the fixture unpatched and unchanged.
- The add-fixture flow moved out of `FixtureManager` into `FixtureCreator` so the
  Studio creates fixtures exactly the way the manager does.

**Not done.** The network *equipment* catalog (placing a node or wireless link
from a model definition, with ports prefilled) -- nodes are still created by
hand in the Network Topology dialog. Proximity-guided patching (N6).

Added after the first pass: "Return to inventory" (`Doc::unpatchFixture`, Design
mode only -- a running function holds channels resolved against the old
address), and an "(unpatched)" badge on Programming-tab target rows.

## What deliberately doesn't need to change

- `FixtureRigProps`/`MonitorProperties`, `FixtureGroup`/head-layout grid,
  Programming tab Targets/Looks — all already address-independent, none of
  this proposal touches them.
- `Doc::addFixture()`'s collision check — unchanged; it still only ever
  runs against fixtures that actually carry a real address.
- Every existing workspace file — an omitted `Patched` attribute defaults to
  `true`, so nothing about a normal file's behavior changes.

## Risks / things to get right

- **Migration is the main risk, and it's a small one.** Default `true` on
  load when the attribute is absent means old files need zero conversion.
  Test explicitly that a file saved before this feature existed opens with
  every fixture patched, as before.
- **The two write-path guards have to be *right*, not just present** — this
  is DMX output correctness, the one place a subtle bug is a real fixture
  going dark or a real fixture receiving another one's values. Cover it
  with an engine-level regression test (see below), not just a UI smoke
  test.
- **Don't confuse this with "Remap fixtures..."** (`ui/src/fixtureremap.h`)
  — that's a stock QLC+ tool for mapping fixtures *between two documents*
  (e.g. reassigning which physical fixture in this show plays the role a
  template file's fixture had), a different problem, worth keeping the
  naming distinct from this feature's "Patch…" verb.

## Files most likely to change

- `engine/src/fixture.{h,cpp}` — `isPatched()`/`setPatched()`, XML
  save/load.
- `engine/src/fadechannel.{h,cpp}`, `engine/src/genericfader.cpp` — the two
  safety-critical guards.
- `ui/src/addfixture.{h,cpp}` — Inventory-targeted "quick add" mode.
- `ui/src/fixturemanager.cpp`, `ui/src/fixturetreewidget.cpp` — the
  Inventory tree section, "Add to Inventory" menu entry, badges.
- `ui/src/monitor/monitor.cpp` (`studioAddFixture`) — "Create new fixture…"
  from inside Lighting Studio.
- A new small dialog/widget for the bulk "Patch…" view (natural home:
  alongside `ui/src/universeusagewidget.{h,cpp}`, which already renders a
  per-universe address grid and is the obvious drop target for a
  drag-to-patch interaction later).

## Effort estimate

- Phase 1.1 (`isPatched()` + XML persistence): half a day.
- Phase 1.2 (the two write-path guards + engine regression test): half a
  day — small diff, but this is DMX-correctness code, budget time for the
  test, not just the change.
- Phase 2.1 (Inventory quick-add): half a day.
- Phase 2.2 (create-from-Lighting-Studio): half a day.
- Phase 2.3 (Inventory tree section + badges, three-plus UI surfaces): one
  day.
- Phase 2.4 (bulk Patch view): one day.

Total: roughly 4-4.5 days, essentially the same total as the original
Option-A-then-B estimate — this version just does the small, real engine
work up front instead of deferring it to a "phase 2 maybe," because it
turns out to be small and it's the part that actually solves the problem in
the ask.

## Test approach

- **Engine**: an unpatched fixture at the same default universe/address as
  a real, actually-patched fixture — drive both through a `Scene`, confirm
  only the patched one's values ever reach `Universe::write()`. This is the
  one test that has to be airtight before anything else here matters.
- A workspace built entirely through Inventory (nothing ever patched until
  the end), then bulk-patched, saved, and reopened — confirm every
  placement/group/target survives identically to a normally-patched
  workflow.
- A file saved before this feature existed opens with every fixture's
  `isPatched()` reporting `true`.
- An Inventory fixture dragged into Lighting Studio, mounted, grouped, and
  used as a Programming tab target behaves indistinguishably from an
  already-patched one at every step, until the moment it's patched.
