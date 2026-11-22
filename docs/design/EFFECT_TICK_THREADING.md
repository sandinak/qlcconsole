# Moving the effect tick off the GUI thread

Status: a design note, not a plan (perf/053-effect-host, 2026-11-22).
**Nothing here is built.** The branch that carries this note cut the host's
per-tick cost where it stood instead (see [the decision](#the-decision));
this says what a thread move would take, what stands in the way, and the
one slice worth doing first.

Companion notes: [OFFLOAD.md](OFFLOAD.md) (the same question for the rig
view, and the rules any move follows), [performance.md](../performance.md),
[EFFECT_LIFECYCLE_DESIGN.md](EFFECT_LIFECYCLE_DESIGN.md).

## What runs where now

| Thread | What it does | Lock |
|---|---|---|
| GUI | `EffectScriptRunner::slotTick()` on a 20 ms `QTimer`: every live `EffectInstance::runTick()`, then `publishWrites()` | `m_instanceMutex` while ticking, `m_writesMutex` to publish |
| MasterTimer | `EffectScriptRunner::writeDMX()`: takes a copy of the published writes, composes them and fills the universes' faders | `m_writesMutex`, briefly |
| universe threads | `Universe::processFaders()` on what the faders hold | the universe's own |

The DMX side is **already** decoupled: `writeDMX()` never touches an
instance or a `QJSEngine`; it reads `m_publishedWrites` under a mutex that
is never held across script evaluation. That is the part a thread move
would normally have to invent, and it exists.

What is not decoupled is everything on the way *in*. `runTick()` reads the
live `Doc` — directly, with no lock, because `Doc` has none:

- `Doc::function()` → `Scene`: `transitionIn/Out()`, `isRunning()`,
  `elapsed()`, `transitionFixtures()`, `runtimePalettes()`, `layoutSpan()`,
  `writtenFixtures()`, `values()`, `fixtures()`, `fixtureGroups()`,
  `fadeInSpeed()`, `effectiveSpeedMaster()`
- `Doc::palette()` → `QLCPalette`: `effectParamValues()`,
  `effectTargetBindings()`, `wledEffect()`, `isEffectScoped()`,
  `valuesFromFixtures/FixtureGroups()`, `lookDimmerValue()`
- `Doc::fixture()` → `Fixture`: its mode, heads, channels, `address()`,
  `universe()`, `outputRules()`, `forcedHTP/LTPChannels()`,
  `positionToValues()`, `masterIntensityChannel()`
- `Doc::fixtureGroup()` → size and head map
- `Doc::monitorProperties()`: `fixtureRigPosition()`, `stageCentre()`,
  `stageTarget()`, `targetPosition()`, `targetAimPoint()`,
  `targetFigureHeight()`, `headLayoutFor()`, `occlusionModel(doc)`
- `Doc::masterTimer()` for the beat and the speed masters
- `AimSolver::*` and `RigHeads::headPositions()`, both of which take the
  `Doc` and walk it again

and it writes back into the engine on two paths:

- `Scene::requestReaim()` (the follow spot's beam re-aim, `syncFollowBeam()`)
- `QLCPalette::setFollowSizesBeamLive()` (the footer's live beam size), via
  the runner

## What stands in the way

1. **`Doc` is GUI-thread property.** It has no mutex and the UI edits it
   under the operator's hands: a fixture patched, a look's colour dragged,
   a group's head layout redrawn, a cue's palette list reordered. A worker
   reading `Scene::values()` while the Look Editor rewrites it is a
   use-after-free, not a wrong colour. `Scene` guards its own value list
   with `m_valueListMutex` (added for live preview: `Scene::write`'s
   fader-map build vs `resetRuntime`), and that is the *only* piece of the
   list above that is guarded at all. The rest — `Fixture`, `QLCPalette`,
   `FixtureGroup`, `MonitorProperties`' targets — is bare.
2. **A `QJSEngine` belongs to one thread.** Each instance owns one
   (`EffectScript`). Moving the tick means creating, ticking, garbage
   collecting *and destroying* every live instance on the worker thread —
   the destruction matters: V4 teardown blocks for seconds, which is why
   `slotPrepareQuit()` exists.
3. **Three other callers tick instances on the GUI thread**, each with its
   own engines: the Blind view (`BlindEffects`, "the same effect runtime
   the stage uses"), the cue-output peek (`CueOutput` →
   `wledDeviceWrites()`) and the Look Editor's spread preview
   (`SpreadPreview` → `spreadPreview()`). They can stay where they are —
   separate instances, separate engines — but every `EffectInstance` member
   then has to be correct under both regimes, and the class is 3,200 lines
   with `mutable` caches throughout (`m_stageCells`, `m_imageParams`,
   `m_lastIntentDeg`, `m_cueLenMs`, `m_cycleStart`, `m_ownMs`).
4. **Input arrives on the GUI thread.** `slotInputValueChanged()`,
   `setDataChannels()`, `setOverride()`, `reloadParamsFromPalette()`,
   `setScenesOffStage()`, `syncScene()` and the whole one-shot API
   (`restartOneShots()`, `oneShotState()`) all poke a live instance from the
   GUI thread today and are documented "main thread". Each becomes a queued
   call with an ordering question ("did the override land before or after
   the tick that read it?").
5. **The writes back are UI-visible.** A re-aim request and the footer's
   live beam size have to cross back to the GUI thread, which means a
   frame's latency on the follow spot's beam — the one effect where a frame
   is felt.
6. **The DMXSource contract is fine.** `writeDMX()` would not change: it
   already reads a published list. A worker would publish into the same
   `m_writesMutex`-guarded slot. (It must keep publishing *whole frames*: a
   half-published frame would put a torn colour on the wire.)
7. **Jitter, not throughput, is the risk.** Per OFFLOAD.md's rules a worker
   runs at `QThread::LowPriority` and must never take a lock the MasterTimer
   or a universe thread takes. `m_writesMutex` is taken by both — briefly,
   to copy — so a low-priority worker holding it while descheduled would
   stall the DMX thread. Publishing has to swap a shared-pointer frame, not
   copy a list under the lock.

## The smallest safe first slice

**A frozen argument snapshot, with no threads at all.**

Make explicit what `runTick()` is already half-doing. The 0.4.13–0.5.2 work
on this class moved most of the per-head arguments into caches that are
rebuilt only when their inputs change (`m_facts`, `m_cellObjects`,
`m_spreadBuilt`, `m_distBuilt`, `m_stageCells`), and perf/053-effect-host
finished that job for the rest (the per-member aim objects, the look-aim
fractions, the cells, the look's base values). What is left is a tick that
reads, per frame, only:

- its own clock, envelope and depth (no `Doc`)
- a **`EffectRigSnapshot`**: the cells, the per-fixture facts and places,
  the look's base values and colours, the aim targets' geometry, the
  scene's transitions and elapsed time — a plain value type, built on the
  GUI thread and **refreshed only when the `Doc` changes**, handed over as
  a `QSharedPointer<const EffectRigSnapshot>`

Done as a refactor it is testable with no new concurrency: the same DMX
bytes out, proven by the existing effect tests, and
`SpreadEffects_Test::theHostTickCostOverTheStepBars` watching the cost. It
pays for itself immediately — the snapshot build moves off the 50 Hz path
onto the edit path — and it is the precondition for every later step.
Only then is the thread move a mechanical change: give the runner a
`QThread`, build the instances there, post the snapshot in and the frame
out.

What it does **not** solve, and what the slice after it has to answer: the
`Doc` writes back (5), input ordering (4), and instance lifetime across the
two threads (2).

## The decision

perf/053-effect-host did **not** move the tick. The note above is why: the
move is not small (a snapshot type the whole class has to be rewritten
against, queued input, cross-thread lifetime for a `QJSEngine`) and not
safe while `Doc` has no lock and the UI edits it live. The branch instead
removed the per-head work that made the tick expensive — the per-cell named
property stores, the per-cell head-channel copies, the look-aim lookups and
the per-tick cell and base-value rebuilds — which took the host's own cost
over 6,144 heads from about 2.4 ms a tick to under 0.6 ms (nothing drawn)
and from 6.4 ms to about 1 ms (every head lit) on an Intel Mac in Release.
At 50 Hz that is 3 % of one core instead of 32 %, on the thread the window
paints on, which is what the operator was feeling.

Measured, that is most of the gain a thread move could offer, for a
fraction of the risk. The move is worth revisiting when a single effect's
*script* (not the host) is the cost — a matrix effect on a very large
canvas — because a script cannot be cached away.

## ROI

ROI (first guess): reach = shows with a big pixel rig running an effect
over it (his step bars; one show now, more as pixel rigs spread) = small
but growing; value = the GUI thread's remaining effect cost, now about
1 ms a tick for one lit effect over 6,144 heads, so **at most 5 % of one
core per effect** — below what the rig view itself costs a frame
(OFFLOAD.md items 1 and 4 are 10-23 ms); cost = **large**: the snapshot
refactor of a 3,200-line class, queued input with ordering rules,
cross-thread `QJSEngine` lifetime, and the Blind/preview callers kept
working, plus a before/after DMX p99 comparison per OFFLOAD.md's rules;
risk = **high** on the one path where a race is a crash, not a wrong
pixel. Rough ROI 1 (do the snapshot slice if the host ever climbs again;
do not thread it): after 0.6 unless a script-bound effect is measured as
the cost.
