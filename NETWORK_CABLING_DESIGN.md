# Network / Cabling Truth — Design

Companion to the 2D-monitor overlays. Describes how to model the PHYSICAL data
network — ArtNet/sACN nodes, their DMX output ports, and the runs to fixtures —
so the 2D map can answer real cabling questions: how many nodes, where they sit,
how many universes each carries, and which fixtures hang off each DMX output.

Status: **DESIGN ONLY**. The overlays below already ship:
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
     ├─ transport            // "ArtNet" | "sACN" | "<interface>"
     ├─ ipHint (optional)    // for ArtNet/sACN, informational
     └─ NodePort[]           // physical 5-pin outputs
         ├─ index (1..N)
         └─ universe          // the universe this port breaks out (0 = unused)
```

- A **NetworkNode** is a placeable map item (drag onto the plot, lockable,
  layerable) — reuse the power-source item scaffolding.
- A **NodePort** ties a physical output to a **universe**. Universes are already
  the join key to fixtures (`Fixture::universe()`), so a fixture is "on port P of
  node N" iff its universe == that port's universe.
- The **transport** is cross-checked against the output patch: if a node claims
  ArtNet for universe 3 but the patch says sACN, flag a mismatch.

Cabling is then fully derived:
- **fixtures on a port** = fixtures whose universe == port.universe.
- **run per port** = the polyline node → fixtures in address order (chain).
- **port loading** = channel sum / fixture count per port (spot over-loaded runs).

---

## UI

1. **Network pane** (like the Power pane): add/name nodes, set transport, add
   ports, assign a universe per port. Live conflict/mismatch warnings.
2. **Place nodes on the map**: drag a node onto the plot; it renders as a labelled
   gateway icon (port count shown).
3. **Network overlay, upgraded**: with nodes present, the overlay draws, per
   universe/port, a **run polyline** from the node to its fixtures in address
   order — the actual cable path. Colour by node (or by universe within a node).
   Hovering a run shows `Node · Port p · Universe u · N fixtures · ~L m`.
4. **Reports**: a small summary — nodes, universes/node, longest run, ports free —
   for prepping the data package.

---

## Phasing

| Phase | Scope | Est. |
|---|---|---|
| **N1 — model** | `NetworkTopology` + `NetworkNode`/`NodePort`, Doc ownership, XML. Unit-test the join (fixtures↔port by universe) + transport mismatch. | 3–5 days |
| **N2 — map item** | Node as a placeable/lockable/layerable map item (clone power-source item). | 3–4 days |
| **N3 — Network pane** | Add/edit nodes + ports + universe assignment; mismatch/conflict warnings. | 3–5 days |
| **N4 — run lines** | Upgrade the Network overlay to draw node→fixtures run polylines in address order; hover stats; run-length estimate. | 3–5 days |
| **N5 — reports** | Node/universe/run/port summary for the data package. | 1–2 days |

**Usable "see my nodes + runs on the map"** = N1–N4 ≈ **2–3 weeks**.

**Risk notes**: keep it *derivation-first* — the only new authored data is the
node placement + port→universe map; everything else (which fixtures, transport,
address order) is derived, so it stays correct as the patch changes. Reuse the
PowerDistribution + power-source-item patterns wholesale to keep it cheap. The
overlay run-lines need a small canvas overlay layer (same as the deferred
Fixture-Studio "daisy-chain" idea — build once, reuse for both).

---

## Relationship to Power

Power (circuits/sources) and Network (universes/nodes) are the **two physical
infrastructures** the rig needs. They share the exact same shape: a source/node
placed on the map, owning outputs, with fixtures joined by an attribute
(circuit / universe), overlaid as coloured runs. Building Network as a near-copy
of Power keeps them consistent and lets a future "infrastructure" overlay show
both power and data drops together for a full prep view.
