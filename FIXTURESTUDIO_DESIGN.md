# Fixture Studio — Design

Companion to the 2D-monitor work (`monitorproperties.*`, `monitorgraphicsview.*`)
and the fixture-group model (`fixturegroup.*`). Describes turning 2D-map **groups**
into first-class, editable **studio objects** — logical units of fixtures you
build once, arrange in three dimensions, and place cohesively on stage — and how
those units are informed by, and compose into, the engine's `FixtureGroup`
matrix layouts that RGB/matrix scripts run across.

Status: **DESIGN ONLY** — nothing here is built yet. Decisions locked in the
2026-07-25 exploration; estimates at the bottom.

---

## Vocabulary

```
FixtureGroup  →  (informs)  →  Studio Group  →  (nests into)  →  Studio Group
 (engine FG)                   (MonitorGroup)                    (group-of-groups)
 cell grid /                   rigid body +                      transforms
 script space                  3-plane geometry                  compose
```

- **FixtureGroup (FG)** — the *engine* group. A `QSize` matrix + `QMap<QLCPoint,
  GroupHead>` of **cells** (unitless, topological). This is **script space**:
  RGB/matrix generators index cells, not metres. Unchanged by this work.
- **Studio Group** — a promoted `MonitorProperties::MonitorGroup`. Gains a
  **local coordinate frame** (origin + rotation) and becomes the thing you click,
  edit in 3 planes, and place/rotate as one rigid body. Members store
  **group-local** coordinates; world position is **derived**.
- **Group-of-groups** — studio groups nest via the existing `parentGroupId`;
  child transforms compose under the parent (a *step* inside *all steps* inside
  the stage).

One-liner: **a FixtureGroup says how heads relate for a script; a Studio Group
says where a logical unit sits in space and how it moves — and a FixtureGroup's
layout can seed the Studio Group, without the two ever being locked together.**

---

## The problem — two grouping systems that never talk

Today the fork has two unrelated notions of "group":

| | `FixtureGroup` (engine) | `MonitorGroup` (2D map) |
|---|---|---|
| Coordinates | integer **cells**, unitless, relative | none — members hold absolute **metres** |
| Purpose | script/matrix space, palettes, functional | spatial select-&-move-together |
| Id space | engine | monitor (disjoint) |
| Nesting | block-inclusion + sub-group **tag** (snapshot) | live `parentGroupId` |
| Geometry of its own | a cell matrix | **none** |

A fixture's "which logical unit am I" (FG) and "where do I sit / who do I move
with" (MonitorGroup) are disjoint facts. The Studio marries them — **without**
merging the id spaces (decision **A**, below).

---

## Core model

### 1. The Studio Group is a rigid body with a local frame

`MonitorGroup` gains geometry: an **origin** (`QVector3D`) and **rotation**.
Members are stored in **group-local** coordinates; their world position is
**derived**:

```
worldPos(member) = groupOrigin + groupRotation × localOffset(member)
```

This is the **exact pattern trusses already use** (`FixtureRigProps` →
`fixtureRigPosition()` derives truss-mounted fixtures from truss geometry +
`trussOffset`; `recomputeChildTrusses()` recurses). A studio group is just
another **derivation source** — a rigid parent whose local frame is the unit's
own space. Nested groups compose transforms the way bar-on-truss already does.

- **Grab the group** → translate/rotate the whole unit; every member follows.
- **Grab a member** → edit just its `localOffset`.
- Anchoring is free: `MonitorGroup.anchorKind` already supports `"truss"` /
  `"platform"` — a step group rides its riser, a pod group rides its truss.

### 2. Click a group → edit it in three dimensions

Promoting the group to a rigid body makes a **3-plane editor** (Top / Front /
Side) fall out — the same orthographic-local-frame idea as the truss editor,
generalized. This is essential, not cosmetic: an 8″×8″×8′ **step** has front
LEDs on the *face* (Front plane) and top lights on the *deck* (Top plane); a
single flat map cannot express it. `ui/src/monitor/riserfaceeditor.*` already
places fixtures on a riser face — that's the germ of the Front-plane editor.

### 3. Two decoupled layouts per group (the RGB-script guarantee)

A group carries **two independent layouts that must never be rigidly coupled**:

- **Cell / logical grid** — `FixtureGroup` space. *What the matrix script runs
  across.* Authoritative, untouched. **The script literally cannot see world
  position**, so nothing the studio does to physical placement can break a
  unified effect.
- **Metric / local frame** — *what the 2D map and beam viz draw.* Physical,
  editable, and free to be **irregular** (stage-spanning RGB rigs are not on a
  clean pitch×cell grid; the local frame holds arbitrary offsets fine).

The studio should **overlay the cell index on the physical layout** so you can
see how the script's path threads through real space, and re-sequence cells if
you want a logical wipe to match the physical sweep.

---

## Decision A — bind & reference (NOT merge)

`FixtureGroup` stays the functional/script/palette unit. A studio group may be
**bound** to a FixtureGroup to pull membership and seed layout. Two objects, one
thin link. No id-space merge, backward compatible, engine untouched. (A true
merge — one object that is both — was considered and rejected as too deep a
change to the working programmer-mode path. The binding can be collapsed toward
a merge later if it proves seamless.)

### Binding is non-destructive: **seed** *or* **adopt**

- **Seed from cells** (fresh group): `localOffset = cell × pitch`. Lay out 12
  heads by inheriting the matrix you already authored, then fine-tune in metres.
- **Adopt current positions** (already-placed rig): derive the local frame from
  where members already sit — do **not** overwrite with a regular grid.

Both are **one-time scaffolds**; afterwards cell-space and metric-space are
edited independently. A fixture can be in FG `All RGB` (script covers it) *and*
in studio sub-group `Pod 3` (placement) — different axes, no conflict. That
orthogonality is the whole reason we chose (A).

---

## Composition

### Spatial — live, native

Studio groups nest via `parentGroupId`; transforms compose; they select/move
together. Already works. A *step* group sits under an *All Steps* group; move
*All Steps* and every step + its lights follow.

### Functional (FG-into-FG) — **traceable snapshot**, door open for live

"All Step Fronts" as one script surface is **FG-level** composition. Today FGs
compose by **block-inclusion** (`fixturemanager` "add a group as a block" +
`headSubGroup` tag), which is a **snapshot**, not a live reference. Decision:
**snapshot to start.**

**But leave the door open for live** by recording **provenance now**: tag each
composed block with its **source FixtureGroup id** (and ideally source cell
coords), not just an opaque sub-group tag. Then:

- **Snapshot (now):** a **"Rebuild from members"** action re-pulls source FGs *on
  command*.
- **Live (later):** the *same* rebuild, triggered by `FixtureGroup::changed(id)`
  (which already exists) instead of a button.

Same resolve path, different trigger. The only unrecoverable mistake is *not*
storing source provenance now.

### Studio geometry feeds composition ordering

The studio group knows each member's real X. So a composed group ("All Step
Fronts") can **order its cell-blocks by physical placement** — the mega-grid's
left-to-right cell order derives from where the steps actually sit, so a wipe
across it sweeps the stage correctly.

---

## Template / instance — the component library (Phase 4)

QLC+ fixtures are **unique instances**, so "the same step" ≠ literally reusing
one group. It means a shared **template**: an abstract module of **slots/roles**
("front-strip", "top-cluster"), their local layout, default hang/facing, and an
anchor kind — stored in a library file (JSON, à la a Bundle in
`EFFECTPRESET_DESIGN.md`), show-independent.

- **Template** — the reusable pattern (the 8″×8″×8′ step: front strip + top
  cluster + platform anchor + local layout).
- **Instance (placed assembly)** — stamping a template binds its slots to real
  fixtures (existing, or auto-created from a fixture profile) and drops one
  rigid studio group on its platform.

The **step is the first template**: define once, stamp per riser.

---

## Worked example — the 8″×8″×8′ steps

- **One step = one studio group**, `anchorKind = "platform"` (rides its riser).
  Built in the **3-plane editor**: front LEDs placed on the **Front** plane
  (riser face), top lights on the **Top** plane (deck).
- **Two FGs per step** — `front-LEDs`, `top-lights` — each a cell grid a script
  runs across, physically *informed by* the studio group's local frame, cell
  space untouched.
- **Stage** = step groups **nested spatially** under *All Steps* (live), plus
  *All Step Fronts* / *All Step Tops* as **functional snapshots** composed from
  each step's FGs, **ordered by studio geometry**.
- **"Same step"** = one **template**, stamped per riser, each stamp wired to that
  step's real fixtures.

---

## Phasing & estimates

Rough **focused-engineering** estimates (this codebase: careful thread-safety,
and adding a member to `MonitorProperties`/`Doc` forces a **full** engine+UI
rebuild — see memory `engine_ui_abi_rebuild`). Ranges, not commitments.

| Phase | Scope | Est. |
|---|---|---|
| **1 — Studio group core** | `MonitorGroup` gains origin+rotation (model + XML + ABI rebuild); members store group-local coords; **derived** world position (clone the truss/`fixtureRigPosition` branch); rigid move/rotate on the map; member drag edits local offset. **3-plane (Top/Front/Side) editor** — the expensive part; reuses `monitorgraphicsview` + `riserfaceeditor`. | **1.5–2.5 wk** |
| **2 — FG binding** | studio-group ↔ FixtureGroup link; **seed** from cell×pitch; **adopt** from current positions; keep cell space decoupled; cell-index overlay on the physical layout. | **2–4 days** |
| **3 — Functional composition** | extend block-inclusion to carry **source-FG provenance**; **"Rebuild from members"** action; order composed cells by studio geometry. (Extends existing "add group as a block".) | **3–5 days** |
| **4 — Template library** | template file format (slots/roles, layout, anchor, rig defaults); **stamp** flow (bind slots → real/auto-created fixtures → placed group); library browser UI in the studio. | **1.5–2 wk** |
| **Later — live composition** | trigger `Rebuild` on `FixtureGroup::changed(id)` instead of on command. Cheap *because* Phase 3 stored provenance. | **1–2 days** |

- **Minimum usable "steps on platforms, fronts/tops composed"** = Phases 1–3 ≈
  **3–4.5 weeks**.
- **Full studio incl. reusable component library** = + Phase 4 ≈ **+2 weeks**.

**Risk notes:** Phase 1's 3-plane editor is the schedule driver and the least
reused (truss editor + `riserfaceeditor` give parts, not the whole). The ABI
rebuild gotcha means the `MonitorGroup`-geometry change must land as a full
build or the UI dylib reads stale offsets and crashes on load. Live preview /
derivation paths touched here run on the render path — mind the
`m_valueListMutex`/MasterTimer concurrency model.
