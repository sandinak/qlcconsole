# Network / Cabling Truth — Design

Companion to the 2D-monitor overlays. Describes how to model the PHYSICAL data
network — ArtNet/sACN nodes, their DMX output ports, and the runs to fixtures —
so the 2D map can answer real cabling questions: how many nodes, where they sit,
how many universes each carries, and which fixtures hang off each DMX output.

Status: **N1-N6 built** (model, map item, Network Topology dialog, cable runs
in the Network overlay, reports, proximity-guided patching). The overlays below already ship:
- **DMX overlay** — colour by universe + address-clash flags + transport tag.
- **Network overlay** — colour by transport (ArtNet / sACN / interface).

Those are *derivable* from the existing universe→plugin output patch (no new
model). Everything below is the missing piece: **physical placement of the
gateways and the runs**, which QLC does not model today.

---

## The gap

QLC+ patches each **universe** to an output **plugin** (ArtNet, E1.31/sACN, a USB
interface). That tells us the *transport*, but not:
- **Where** the ArtNet→DMX node physically sits on stage.
- **Which of a node's DMX output ports** carries which universe.
- **The run** from a port to the chain of fixtures on it (and its length).

Without that, "cabling truth" (node count, drop locations, run lengths, port
loading) can't be shown. The overlays colour fixtures by universe/transport but
can't draw the node or the cable.

---

## Model

A new Doc-owned model, sibling to `PowerDistribution` (same pattern:
Doc-owned, XML-persisted, edited from a pane, visualised on the 2D map).

```
NetworkTopology (owned by Doc)
 └─ NetworkNode[]          // an ArtNet/sACN gateway (or a DMX interface)
     ├─ id, name
     ├─ position (map item, like a power source)  // world metres
     ├─ transport            // "ArtNet" | "sACN" | "<interface>" | "Wireless DMX"
     ├─ ipHint (optional)    // for ArtNet/sACN, informational
     └─ NodePort[]           // physical 5-pin outputs
         ├─ index (1..N)
         ├─ universe          // the universe this port breaks out (0 = unused)
         └─ NetworkReceiver (optional, wireless only — see below)
```

- A **NetworkNode** is a placeable map item (drag onto the plot, lockable,
  layerable) — reuse the power-source item scaffolding.
- A **NodePort** ties a physical output to a **universe**. Universes are already
  the join key to fixtures (`Fixture::universe()`), so a fixture is "on port P of
  node N" iff its universe == that port's universe.
- The **transport** is cross-checked against the output patch: if a node claims
  ArtNet for universe 3 but the patch says sACN, flag a mismatch.

### Direct-network fixtures -- see "Fixture connection" below

Some fixtures speak ArtNet/sACN natively (fences, steps): no converter box, no
DMX cable run. An earlier draft modelled them as a `Direct` kind of node; that
was removed 2026-09-24 in favour of a per-fixture connection, described next.

### Fixture connection: DMX vs Network (supersedes `Direct`)

Decided 2026-09-24 (Branson): whether a fixture is fed by DMX or speaks
ArtNet/sACN natively is a property of the **fixture**, not a kind of node.
Fixture definitions carry no connection info (`DmxConnector` is empty in every
shipped definition), and today the connection belongs to the universe's output
plugin, so nothing records that a fence or step needs no DMX cable.

- `Fixture::connection` -- `Dmx` (default; omitted from the XML so every
  existing file is unchanged) or `Network`. A both-capable fixture is simply
  whichever the LD picks at patch time.
- A `Network` fixture is its own endpoint: no converter port, no cable run.
  It carries an optional **`ipAddress`**, and the rig uses **unicast** -- each
  network fixture (or each controller behind one IP) is targeted individually.
- That IP is enforced through the universe: the output patch's `outputIP` is
  per universe, so unicast means **one universe per distinct IP**, with
  fixtures behind the same IP sharing it. Patching a network fixture
  allocates (or reuses) a universe whose output is ArtNet/sACN at that IP and
  writes the IP into Connections. This grows the universe count, which the
  66-universe show already tolerates.
- `NetworkNode::Direct` is gone (removed 2026-09-24): a network fixture never
  needed a node. The topology models only gear that carries universes -- converters and
  wireless links. (Old `Kind="1"` nodes load as ordinary nodes; the attribute is ignored.)
- Warnings: a `Dmx` fixture on a universe no node port carries has no physical
  path; a `Network` fixture on a universe patched to a non-network output is a
  mismatch; two IPs on one universe cannot both be honoured.
- N6 Patch suggestions split by connection: `Dmx` fixtures anchor to the
  nearest node port or wireless receiver with free capacity; `Network`
  fixtures need no proximity, only a network-output universe at their IP.

### Wireless DMX (transmitter/receiver split)

A wireless DMX link has two physically separate boxes: a transmitter, and one
or more receivers, each a real unit at its own rig position. The transmitter's
location is frequently *not* useful as a proximity anchor for "which fixtures
does this universe serve" — e.g. a transmitter sitting at FOH or backstage,
relaying to a receiver actually mounted in the rig. Treating the node's own
placed position as that anchor (as every wired `converter`/`direct` node does)
would silently point proximity-based tooling at the wrong physical location.

For `transport == "Wireless DMX"`:
- The `NetworkNode`'s own `position` is the **transmitter's** physical location
  — placed wherever it actually sits, which may be nowhere near the fixtures it
  serves.
- Each `NodePort` that's actually in use carries an optional
  **`NetworkReceiver`** — its own placeable map item, with its own position,
  referencing the parent node + port. A receiver's position is what proximity
  logic (the bulk Patch tool, run/cabling reports) should anchor to; the
  transmitter's position is cosmetic/informational only for a wireless node.
- A wired node has no `NetworkReceiver` — port position == node position, same
  as today.

This means "which universes are physically near fixture F" always resolves
through a receiver when one exists, falling back to the node's own position
only for wired converter/direct nodes.

Cabling is then fully derived:
- **fixtures on a port** = fixtures whose universe == port.universe.
- **run per port** = the polyline node → fixtures in address order (chain) for a
  `converter` node, anchored at the `NetworkReceiver` position instead of the
  node's own position when the port is wireless; no run at all for a `direct`
  node (nothing to draw — see above).
- **port loading** = channel sum / fixture count per port (spot over-loaded runs).
- **proximity anchor** (for a bulk-patch "suggest a universe" tool) = the
  receiver's position when the port is wireless, else the node's own position.

---

## UI

1. **Network pane** (like the Power pane): add/name nodes, set transport,
   add ports, assign a universe per port. For a
   wireless port, place its `NetworkReceiver` (own position, separate from the
   transmitter node). Live conflict/mismatch warnings.
2. **Place nodes on the map**: drag a node onto the plot; a node renders as a
   labelled gateway icon (port count shown), and a wireless node's receivers render as their own icon at their own position,
   linked back to the transmitter by a dashed (not cable) line.
3. **Network overlay, upgraded**: with nodes present, the overlay draws, per
   universe/port, a **run polyline** from the node (or receiver, for wireless)
   to its fixtures in address order — the actual cable path. `direct` nodes
   draw no run. Colour by node (or by universe within a node).
   Hovering a run shows `Node · Port p · Universe u · N fixtures · ~L m`.
4. **Reports**: a small summary — nodes, universes/node, longest run, ports free —
   for prepping the data package.
5. **Bulk "Patch…" suggestions** (feeds the inventory-first workflow's Patch
   view — see [Inventory-first workflow](docs/proposals/inventory-first-workflow.md)):
   for an unpatched fixture already placed in Lighting Studio, find the
   nearest proximity anchor (a receiver if wireless, else the node itself),
   and suggest that node's least-loaded free port/universe as the patch
   target. Purely a suggestion — the LD still confirms via the existing
   collision-safe `AddFixture` edit-mode path.

---

## Phasing

| Phase | Scope | Est. |
|---|---|---|
| **N1 — model** (done) | `NetworkTopology` + `NetworkNode`/`NodePort` incl. `NetworkReceiver` for wireless ports, Doc ownership, XML. Unit-test the join (fixtures↔port by universe), transport mismatch, and the wireless proximity-anchor rule. | 4–6 days |
| **N2 — map item** (done) | Node as a placeable/lockable/layerable map item (clone power-source item). Built for nodes; `NetworkReceiver` as its own placeable item, linked to its node/port, moves to N4 with the overlay layer. | 4–5 days |
| **N3 — Network pane** (done) | Add/edit nodes + ports + universe assignment; place/edit wireless receivers; mismatch/conflict warnings. | 4–6 days |
| **N4 — run lines** (done) | Upgrade the Network overlay to draw node→fixtures (or receiver→fixtures) run polylines in address order; hover stats; run-length estimate; no run for `direct` nodes. | 3–5 days |
| **N5 — reports** (done) | Node/universe/run/port summary for the data package. | 1–2 days |
| **N6 — bulk Patch suggestions** (done) | Proximity-based universe/port suggestion for the inventory-first workflow's bulk "Patch…" view, per the anchor rule above. Depends on N1 (model) + [Inventory-first workflow](docs/proposals/inventory-first-workflow.md) Phase 1 (`isPatched`). | 2–3 days |

**Usable "see my nodes + runs on the map"** = N1–N4: **built.**

**Usable "bulk-patch my inventory sensibly"** = N1 + N6, without needing N2–N5's
visual map UI first (a Network pane with no map placement yet is enough to
define nodes/ports/receivers for the join) ≈ **1–1.5 weeks**.

**Risk notes**: keep it *derivation-first* — the only new authored data is the
node placement + port→universe map; everything else (which fixtures, transport,
address order) is derived, so it stays correct as the patch changes. Reuse the
PowerDistribution + power-source-item patterns wholesale to keep it cheap. The
overlay run-lines need a small canvas overlay layer (same as the deferred
Fixture-Studio "daisy-chain" idea — build once, reuse for both).

---

## What N4 built

`NetworkTopology::runs(doc, fixturePositions)` derives one `NetworkRun` per port
that carries a universe: the patched **DMX** fixtures on that universe in
**address order**, the run's start (the node, or the receiver for a wireless
port), a polyline through the fixtures that are on the plot, a straight-line
length estimate (real cable is longer -- it climbs trusses), and the channels
used. Network fixtures (their own endpoint) and staged fixtures (no address) are
never on a run. `unservedFixtures()` lists patched DMX fixtures whose universe no
port carries. Nothing is stored: it is all derived, so it stays correct as the
patch and the plot change.

In the Lighting Studio's **Network overlay** each run is drawn as a coloured
polyline (colour per node and port) under the fixtures, with a hover band wide
enough to hit. The tooltip reads `Node [via Receiver] - Port p - Universe u - N
fixture(s) - ~L m - used/512 ch`, with "N not on the plot" when some fixtures on
the run have no position yet, and lengths in feet when the plot is in feet. Runs
rebuild (debounced) when a fixture or node moves or a fixture is repatched. A DMX
fixture on a universe no port carries is tinted and says so -- but only once a
topology exists, since with no nodes at all every fixture would be flagged.

## What N5 built

`NetworkReport::build(doc)` derives the data-package summary from the topology,
the patch and the plot -- nothing is stored. It counts nodes, ports (used and
free) and distinct universes carried; per node it lists transport, IP hint,
ports, universes, fixtures and channels; per port the universe, the wireless
receiver it lands at, fixtures, channels and the run length (only when the run
has a start point and at least one fixture on the plot -- the same positions
the Network overlay measures with). It names the longest run, lists fixtures
that are their own network endpoint by IP (with their universe, or "not
patched"), the patched DMX fixtures no port carries, and the topology warnings.

`toText(feet)` is the readable summary; `toCsv(feet)` is two spreadsheet tables
(ports, then network fixtures) separated by a blank line, with fields quoted
where they need it. Lengths follow the plot's units.

In Lighting Studio, **More > Network Report...** opens it read-only with
**Copy** (the text) and **Save CSV...**.

## What N6 built

`PatchPlanner` places a DMX fixture that is on the plot by its nearest node
port: every port with a known location is ranked by the plan distance from the
fixture to where the port's universe lands (the receiver for a wireless port,
else the node), equally near ports least-loaded first. The nearest port with
room wins -- first-fit within the universe it already carries, or, when none of
that node's universes has room, a free port on it claims a new universe, which
the next fixture near that node then joins. Applying the plan writes a claimed
universe onto its port. Fixtures not on the plot, and those with no node near
them that has room (noted on the row), are packed first-fit as before; network
fixtures keep one universe per IP; DMX still never shares a network universe.
Placed fixtures are planned first, so first-fit never takes the room the
fixtures next to a node were going to use.

The Patch dialog shows the proposal in a **Via** column ("SR Node · port 2 ·
3.2 m", "(assigns the port)" when it claims a free one) and a **Place by nearest
node** checkbox, on when any node port is placed. Editing a row's universe by
hand drops the proposed port; Via then names whichever port carries the new
universe, or a dash.

## Relationship to Power

Power (circuits/sources) and Network (universes/nodes) are the **two physical
infrastructures** the rig needs. They share the exact same shape: a source/node
placed on the map, owning outputs, with fixtures joined by an attribute
(circuit / universe), overlaid as coloured runs. Building Network as a near-copy
of Power keeps them consistent and lets a future "infrastructure" overlay show
both power and data drops together for a full prep view.

## Relationship to Inventory-first workflow

[Inventory-first workflow](docs/proposals/inventory-first-workflow.md) adds a
persisted `Fixture::isPatched()` and a bulk "Patch…" view for backfilling
universes/addresses onto fixtures staged before patching. That view's dumbest
form (next-free-address, topology-blind) needs nothing from here. Its useful
form — "patch this fixture into whichever universe its physical neighbourhood
actually uses" — is N6 above, and depends on `NetworkTopology` existing (N1)
so there's a node/port/receiver graph to walk. The two proposals are
independent to build but meant to be used together: stage the rig from
Inventory, place it in Lighting Studio, then let the Patch view use Network's
topology to suggest real addresses instead of guessing from scratch.
