# TODO — Programmer / Programming-tab work

Active + not-yet-built work for the fork's programming/looks workflow.
**Shipped work has moved to [DONE.md](DONE.md)** (the completed log, with
per-feature detail + deferred/eyeball notes). Add new work here; move an entry
to DONE.md when it ships. See also the session memory under
`~/.claude/.../memory/`.

---

## Fixture Manager — modernize + look at integrating with qlcconsole *(2026-09-02, design-first; investigation not started)*

Branson: look at Fixture Manager and see what can be done to modernize it and
maybe integrate it more with the rest of qlcconsole, rather than leaving it as
the stock QLC+ tab.

Starting context for whoever picks this up: `ui/src/fixturemanager.{h,cpp}`
(~2680/~300 lines) is a listed fork hotspot (CLAUDE.md) — fixture-group
folders, empty-group creation, and the head-layout grid editor (drag fixtures
onto cells, add a group as a block, move a sub-group as a unit) all already
live here, plus it's the entry point into `ui/src/rdmmanager.{h,cpp,ui}`
(`fixturemanager.cpp:457`). That RDM entry already has its own open TODO
entry above noting Fixture Manager is "already crowded" as a place to hang
more UI — worth reading together with this one rather than planning either
in isolation.

Also worth reading alongside: the adjacent **"Now — unify the object editor's
fixture management"** entry below — that one is about the Lighting Studio
per-object editor's fixture picker specifically, a different (if related)
surface from the Fixture Manager tab itself. Since this session separately
did a full hierarchy/workflow rework of the Connections/Devices tree
(strict per-level CRUD, consistent right-click ordering, a real host/protocol/
target/port tree shape — see DONE.md 2026-09-02) and a workflow-UX review
flagged Fixture Manager's empty-state onboarding as one of the *good*
existing patterns not yet applied everywhere else, there may be real value in
holding Fixture Manager up against those same conventions: does its tree
hierarchy, menu structure, and empty/onboarding state now read as
inconsistent with Connections/Devices? Is there UI here that belongs on a
newer tab (Programming, Lighting Studio) instead? **Nothing decided yet** —
start with an audit of the current tab (structure, menus, what's crowded,
what's stock-QLC+-shaped vs already fork-modified) before proposing a
direction, same as the object-editor and stage-feature entries below did.

---

## "E1.31" shown as "E1.31 (sACN)" for clarity (2026-09-02) — SHIPPED

Branson, after confirming E1.31 does load and is reachable via "Add a
protocol": "can we put sACN in parens for that proto so it's clear."

New `displayPluginName()` (`ui/src/connectionstree.cpp`) — display-only,
maps "E1.31" to "E1.31 (sACN)" everywhere the tree shows a protocol name to
an operator (the protocol row itself, the "Add a protocol" list, "This
Host"'s tooltip plugin list) without touching `QLCIOPlugin::name()`, the
actual identity string a workspace file saves and every internal lookup
matches by. Deliberately not a rename: `E131Plugin::name()` staying
"E1.31" means every existing saved file, and everywhere in the engine that
looks a plugin up by name, is completely unaffected — this is purely what
gets drawn on screen.

While in there: the "Add a protocol" menu's chosen-action lookup used to
match by the action's TEXT back against a name — fragile the moment a
display name could differ from the real one, which this same change just
introduced. Switched to a direct `QMap<QAction*, QLCIOPlugin*>` built at
menu-construction time instead, so the display text can be anything
without breaking which plugin actually gets revealed.

Builds clean, launched without error. **Not interactively verified** — the
Devices tab and "This Host" → "Add a protocol" should both read
"E1.31 (sACN)" now instead of bare "E1.31".

## HID listed unusable devices as if they were real joysticks (2026-09-02) — SHIPPED

Branson, with a screenshot: three "Apple" HID input rows, no distinguishing
name, "nothing patched" on all three — "we shouldn't show things we can't
use/patch to."

Traced it: `HIDPlugin::refreshDevices()` calls `hid_enumerate(0, 0)` (every
HID device on the system, no filter) and only keeps ones where
`HIDOSXJoystick::isJoystick(cur_dev->usage)` matches a joystick/gamepad/
multi-axis-controller/hatswitch top-level USAGE. That check passing does
NOT mean the device turned out to have anything on it once its actual HID
report descriptor got parsed (`HIDOSXJoystick::init()`, which populates
`m_axesNumber`/`m_buttonsNumber`) — Apple's HID stack exposes several of
its own internal devices (trackpad/keyboard auxiliary interfaces, wrapper
nodes) reporting a joystick-shaped top-level usage despite being neither a
joystick nor anything else patchable, typically with no product string
either (`HIDJsDevice`'s name is `manufacturer + " " + product`; empty
product is exactly why these three all just read "Apple"). They were
never actually usable, and the tree had no way to say so.

Added `HIDDevice::buttonCount()` alongside the existing `axisCount()`
(both public on the base class, -1 for non-joystick types); after
constructing a candidate joystick device, `refreshDevices()` now discards
it instead of calling `addDevice()` when it has neither an axis nor a
button (`<= 0` on both). Can't accidentally hide a real control surface —
anything genuinely patchable has at least one of the two by definition.
Scoped to the shared post-construction check, so it applies to
Linux/Windows joystick construction too, not just the macOS path the
screenshot showed.

Rebuilt the `hidplugin` target directly first to confirm it compiles
in isolation, then the full app — both clean, no errors.
`HIDDevice`/`HIDJsDevice` are internal to `plugins/hid/` (not a shared
interface header like `QLCIOPlugin`), so this doesn't carry the
cross-library vtable-staleness risk the ArtNet subnet work hit earlier
today. Launched without error. **Not interactively verified** — needs
actual hardware to confirm the three bogus "Apple" rows are gone and a
real joystick/control surface (if one is plugged in) still shows up
correctly.

## Title-bar toolbar was really two rows, not one (2026-09-02) — SHIPPED

Branson, with a screenshot: the title text and the toolbar icons
(Stop All/Blackout/Blind/Operate) were visibly on two separate lines
inside the merged macOS title bar, not sitting level on one.

Root cause: `Qt::ToolButtonTextUnderIcon` (the app's general default,
governed by View → Toolbar Style, which this toolbar was still following)
makes each button tall enough to fit a text label under its icon. Qt's
unified title/toolbar chrome grows to accommodate whatever the toolbar
actually needs, so it was still rendering ONE unified area — just a tall
one, with the title text ending up as its own visual line inside that
taller area rather than beside the icons. No stock macOS app with a
unified toolbar labels its title-bar buttons (Safari, Mail, Xcode) for
exactly this reason.

Locked this one toolbar to `Qt::ToolButtonIconOnly` in `initToolBar()`,
and excluded it from the "Toolbar Style" menu's sync logic on macOS (that
menu still governs every per-manager toolbar and the tab bar normally —
only this toolbar is pinned, and only on macOS, where the unified-chrome
constraint actually applies).

Builds clean, launched without error. **Not interactively verified** — the
title bar should now read as one row, icons beside the title text rather
than on a line below it.

## "This Host" shows real identity; protocol Carries missed pending universes (2026-09-02) — SHIPPED

Branson confirmed "This Host" fixed the add-a-protocol problem, then two
follow-ups: replace the placeholder "This Host" label with real info
(hostname, versions, "whatever else might be useful"), and — mid-turn, with
a screenshot — the ArtNet row's Carries column was blank despite 52
universes sitting right under it.

**Host identity:** the root row's name is now the actual machine hostname
(`QHostInfo::localHostName()`, falls back to "This Host" if that comes back
empty), Detail shows `qlcconsole <version> · N I/O plugins`, and the
tooltip carries the fuller picture: hostname, app version (`APPVERSION`),
Qt version, OS (`QSysInfo::prettyProductName()`), and which I/O plugins are
loaded. Checked whether OLA (specifically requested) exposes any version
string of its own to surface — it doesn't (`OlaIO::pluginInfo()` has a
one-line description, no version) — so it's listed by name like every
other loaded plugin rather than a version number being invented for it.

**Carries roll-up gap:** the protocol-row summary pass that rolls "N
universes" up into Carries only counted `KIND_UNIVERSE` (and folded
`KIND_PORT`) rows — it had no idea `KIND_PENDING_UNIVERSE` (this session's
own addition, for a universe patched to an interface not present here)
was also a real universe count against that protocol. A protocol carrying
nothing BUT pending patches rolled up to an empty Carries cell instead of
"52 universes". Added `KIND_PENDING_UNIVERSE` to that count — deliberately
NOT folded into the same `patchedUniverses` set real/resolved rows use,
since `pendingUniverseIds` (computed earlier in the same function) already
tracks exactly this for the "Unpatched" folder's own exclusion logic; this
is a display count, not a second copy of that tracking.

Builds clean, launched without error. **Not interactively verified** — the
host row should show a real hostname and version info instead of "This
Host", and a protocol carrying only pending universes should now roll up
its Carries count instead of showing nothing.

## "This Host" root, fixture/head Carries, Delete Universe on empty space (2026-09-02) — SHIPPED

Branson, three asks in one message, plus a mid-turn screenshot.

**Mid-turn: "also this is still there on the right click"** — a screenshot
of the empty-space menu still offering "Delete Universe 'U66-StepBars-B'…"
alongside "Add Universe". Same bug as the ArtNet-row report two rounds ago,
same fix, one spot I'd deliberately left alone at the time reasoning empty
space was a legitimate contextual home for it. Branson's right that it
isn't any more than any other row is -- deleting the LAST universe from a
click that named a specific OTHER universe is the same false implication
regardless of where the click landed. Removed; empty space now offers Add
Universe only, matching everywhere else.

**1. "Need the ability to add a protocol .. can we at least enable
visibility temporarily during the session."** Branson's own proposed fix,
better than what I'd tried: a permanent, always-right-clickable top-level
"This Host" row, everything else nested under it. New `KIND_HOST`; every
plugin row and the "Unpatched" folder now live under it instead of
directly under the tree root. Right-clicking it offers "Add a protocol"
listing every compiled-in plugin regardless of current visibility --
selecting one reveals it exactly the way that protocol's own "Add an
interface…" would (factored the shared logic into `revealInterface()`/
`addTargetOnNewInterface()` so the host-level and protocol-level actions
can't drift apart). This is a real structural change, not a cosmetic one:
`refresh()`'s summary pass, the empty-tree message, and the default-expand
depth calculation all previously assumed protocols WERE the top level and
needed updating to walk `hostItem`'s children instead of the tree's.

**2. "Carries should identify fixtures/heads for universes as
configured."** The fixture COUNT was already there ("12 fx · 384/512");
head count was not, and a rig built from a handful of multi-head fixtures
(an LED bar, a pixel strip) has its real output measured in heads, not
fixture instances. New shared `setCarriesFixtures()` adds head count
(only when it differs from fixture count -- most rigs are single-head, and
repeating the same number would just be noise), and now runs for pending
and unpatched universe rows too, not just resolved ones -- fixture
assignment is a Fixture Manager fact independent of whether the network
path behind a universe currently resolves.

**3. Franklin chart on swapping Protocol and Network order in the tree** --
delivered as analysis in conversation, not code; see that response.

Builds clean, launched without error. **Not interactively verified** — "This
Host" should now be the tree's one root row, always right-clickable
regardless of "Show unused"; a universe row should show head count
whenever it differs from fixture count; the empty-space menu should no
longer offer Delete Universe.

## Reverted always-show-protocols; toolbar icon size (2026-09-02) — SHIPPED

Branson pushed back on the previous round's "every protocol always stays
listed" change: "UGH .. ok lets be clear we should only show unused if the
box is checked." Then the actual need underneath: "to USE one I need the
ability to add in-situ .. can we at least enable visibility temporarily
during the session so we can then right click and add connection?"

Reverted the previous round's change to `refresh()` — a protocol with
nothing on it goes back to being deleted (not just its idle lines hidden)
when "Show unused protocols and interfaces" is unchecked, restoring the
checkbox's actual meaning. What Branson asked for after the "UGH" — reveal
everything for the session, add a connection, done — is exactly what that
checkbox already does when ticked, and always has: nothing new was needed
there, the fix was undoing the previous overreach, not building a second
mechanism next to an existing one.

Noted, not actioned: "this tracks with the idea .. need the ability to
'blind' configure connections and assets generally .. that might be harder
for something more specific than not." A real design direction (configuring
things you cannot currently see/reach live) broader than this specific
checkbox — flagged here for whenever it becomes a concrete ask rather than
guessed at now.

**Toolbar icon size**, from a title-bar screenshot: the app-level toolbar
(Stop ALL/Blackout/Blind/Operate — the one merged into the macOS title bar
itself via `setUnifiedTitleAndToolBarOnMac`) was still at 24x24, the ONE
toolbar in the app not already matching the 20x20 every per-manager
toolbar converged on earlier this session. Matched it. Smaller icons here
matter more than anywhere else in the app: this toolbar's height is space
taken directly from the title bar, not just another row in a window.
Also worth knowing: View → Toolbar Style → "Icons Only" already exists
(added earlier this session) and applies to this exact toolbar — unchecking
the text labels entirely is a bigger, already-available lever than icon
size alone if more compactness is wanted.

Builds clean, launched without error. **Not interactively verified** — the
Devices tab should go back to hiding an idle protocol's row entirely with
"Show unused" off, and the title-bar toolbar icons should read visibly
smaller.

## Protocol rows were disappearing entirely + Delete Universe on unrelated rows (2026-09-02) — SHIPPED

Branson, on a screenshot of the ArtNet right-click menu: "still no add a
protocol" (even though "Add an interface…"/"Add a target…" had just
shipped) and "why delete universe on artnet?"

**1. The actual gap "add a protocol" was pointing at:** `refresh()` didn't
just hide an idle protocol's individual LINES when "Show unused" was off
(that part is correct decluttering) — it deleted the entire PROTOCOL ROW
outright whenever it ended up with zero children. So a protocol with
nothing patched or pinned on it yet had no row to right-click "Add an
interface…"/"Add a target…" ON at all, with no way back except knowing to
tick "Show unused protocols and interfaces" first. That's the real
"add a protocol" gap — not a missing action, a missing ROW to put the
action on. Fixed: every compiled-in protocol now always stays listed as a
top-level row, whether or not it currently has anything under it (shows
"no lines" when empty) — only individual idle LINES are still subject to
"Show unused".

**2. "Delete Universe 'X'…" on the ArtNet row (or any row not about
universe X) was a real design mistake, not a display quirk.**
`appendUniversalMenuActions()` added it to literally every menu in the
tree, unconditionally deleting whatever universe happened to be LAST
regardless of what was actually clicked — reading as if it were an ArtNet
action when it has nothing to do with ArtNet at all. Removed from the
universal set. It already has real, contextual homes that were never
touched: right-click a universe's own row directly (KIND_UNIVERSE/
UNPATCHED/PENDING already each offer "Delete universe entirely…"), or
empty space, which still offers Add and Delete together for when there's
genuinely nothing else to click. "Add Universe" stays universal — it
doesn't reference anything specific to the row it's on, so it doesn't
carry the same false implication.

Builds clean, no new warnings, launched without error. **Not interactively
verified** — a protocol with nothing patched (E1.31, OSC, whichever else is
compiled in but idle) should now show up as its own row even with "Show
unused" off; right-clicking ArtNet (or anything not a universe) should no
longer offer to delete one.

## Add a target without patching a universe + dismiss-menu bug (2026-09-02) — SHIPPED

Branson, two separate reports:

**1. "Need ability to add a protocol .. I know I can do it by patching a U
.. but I think we should be able to create a protocol and then patch to it
as well."** Read as: create the connection (interface + target) on its
own, patch a universe to it as a later, separate step — the only route to
that today went through patching a universe first (`patchUnpatchedUniverseTo()`)
or required already knowing to reveal an interface via "Add an interface…"
and THEN separately right-click that row for "Add a target on this
interface…". New "Add a target…" at the PROTOCOL row itself
(target-capable protocols only — ArtNet today): resolves which interface
to use the same way "Add an interface…" does (the only one, or asks),
pins it visible, then prompts for the target address — genuinely "add
interface + add target" in one step, still not touching any universe.
Patching stays a separate, later action from the port row, same as always.

**2. "If I right click on artnet .. and then exit that window it pops up
configure artnet plugin."** Real bug, and a nasty one: `if (pick0 == cfg)`
where `cfg` is `NULL` for ArtNet (that dialog is deliberately withheld for
target-capable protocols, see the entry below this one) — and `pick0` is
ALSO `NULL` whenever the menu is dismissed without picking anything.
`NULL == NULL` is true, so dismissing the ArtNet context menu with no
selection silently ran `configurePlugin()` anyway. Audited every one of
the tree's other exec() sites for the same shape (an unconditional first
comparison is safe even when `pick0` is `NULL`; comparing against a
CONDITIONALLY-null action first is not) — only this one site had it; the
rest either compare against an unconditional action first or already guard
`chosen == NULL` up front (the big `KIND_UNIVERSE` branch needed the guard
anyway to avoid a `chosen->parentWidget()` null-deref, so it was already
safe by accident). Fixed with an explicit `if (pick0 == NULL) return;`
right after the universal-action check, before any per-kind comparison —
now the one place to look if a similar action gets added later at the
protocol level.

Builds clean, launched without error. **Not interactively verified** —
right-click ArtNet and dismiss the menu with no selection (click away or
Escape) — should do nothing now, not pop up Configure ArtNet. And "Add a
target…" should appear directly on the ArtNet row and walk through
interface + address without touching any universe.

## "None" sentinel was mistaken for a real missing interface (2026-09-02) — SHIPPED

Branson, with a screenshot: a ghost interface literally named "None" showed
up alongside the real 172.18.2.x ones, with one universe under it. Asked
what the difference was and whether it was set up with ArtNet with no IP
— good question to ask rather than assume, and it pointed at a real bug.

`"None"` is `KOutputNone`, `OutputPatch::outputName()`'s own long-standing
fallback string for "this patch has a plugin but no line was ever actually
resolved for it" — used the same way for MIDI input/feedback patches
elsewhere in this same workspace file (`UID="None"`), predating this
session entirely. It is a sentinel for ABSENCE of an identity, not an
identity itself. The subnet/pending resolution added earlier today didn't
know that: `outputUID.isEmpty()` is false for the string "None", so it
went through the same matching path as a real IP, failed to match anything
(obviously — "None" isn't an address), and landed in the pending branch,
remembering it forever as a "missing interface" named None.

Fixed with one exclusion in `InputOutputMap::setOutputPatch()`:
`outputUID != KOutputNone` alongside the existing emptiness check, so this
case falls through to the older index-based handling exactly as it did
before pending existed — appropriate, since a patch that was never
actually finished being set up isn't "on a network this machine can't
reach," it just never had a real interface chosen for it, and 
`danglingOutputPatches()`'s existing out-of-range check already covers it
if the accompanying numeric line is out of bounds.

New test `InputOutputMap_Test::noneSentinelDoesNotGoPending()`. Full sweep
clean: `outputpatch_test` 6/6, `inputoutputmap_test` 36/36. Builds clean,
launched without error. **Not interactively verified** — the "None" ghost
interface should be gone from the tree entirely now; Universe 8 (the one
that triggered it) should resolve or dangle the old way instead.

## Pending patches' real targets were reading back as broadcast (2026-09-02) — SHIPPED

Branson, with a screenshot: 52 universes under a pending "192.168.1.125"
ArtNet interface all showed "output · broadcast" with no device/port
breakout. Checked the actual workspace XML rather than guessing —
`<PluginParameters outputIP="172.18.2.221" outputUni="0"/>` etc. were
right there for most of them. Real bug, not a display gap in the previous
round's work.

**Root cause:** `OutputPatch::getPluginParameters()` doesn't read its own
locally-cached parameters — it asks the PLUGIN for whatever it has stored
for `(universe, line)`. A pending patch has no valid line
(`QLCIOPlugin::invalidLine()`, by design — that's what keeps it from
opening anything), so the plugin has nothing to report for it, and the
real `outputIP`/`outputUni` values loaded straight off the file's
`<PluginParameters>` — which DO land in `setPluginParameter()`'s local
`m_parametersCache` correctly regardless of pending state — never made it
back out through the one method the tree's rendering code (and anything
else) calls to read them.

Fixed in `OutputPatch::getPluginParameters()`: fall back to
`m_parametersCache` whenever there's no valid line to ask the plugin about,
instead of returning empty. This is the same cache `reconnect()` already
trusts as the authoritative record to replay onto the plugin, so it's not
a new source of truth, just the existing one finally being read from in
the one case (`isPending()`) that didn't exist before this session. New
test `OutputPatch_Test::pending()` extended to set outputIP/outputUni on a
pending patch and assert they read back correctly — this would have caught
it. Full sweep re-run clean: `outputpatch_test` 6/6, `inputoutputmap_test`
35/35.

**Known follow-on gap, not fixed (unreachable today, so left alone):** if a
pending patch is ever resolved LIVE mid-session (not at file load — there
is currently no such path; resolution only happens once, during
`loadXML()`), nothing replays `m_parametersCache` onto the plugin the way
`reconnect()` does. Not a real gap yet because nothing triggers a live
pending→resolved transition today; would need its own fix the day
"auto-retry when the network reappears" becomes a real feature.

Builds clean, launched without error. **Not interactively verified** — the
same 52-universe workspace should now show each pending universe's real
target IP and port (e.g. 172.18.2.221 › port 0:0:0 › the universe), not
"broadcast," for every one that actually has `outputIP` in the file.

## Pending patches show their target/port breakout too (2026-09-02) — SHIPPED

Branson, right after the ghost-interface restructure: "should show the
target and ports breakout to the universe the same way as if it was
connected."

The interface being unreachable doesn't erase the rest of a targeted
patch's address -- outputIP/outputUni are plugin PARAMETERS stored on the
`OutputPatch` object itself, untouched by `setPending()` (which only clears
`m_pluginLine`), so they were sitting right there the whole time, just not
being read. Each pending universe's ghost interface row now reads those
same parameters and builds the same device → port → universe breakout a
resolved patch shows (device row per target address, folding multiple
universes' ports under one node, matching how the live/manual-target case
above it already folds), instead of listing every pending universe as a
flat, undifferentiated child. A pending patch with no target at all
(broadcast) still lists directly under the ghost interface, now explicitly
labelled "output · broadcast" to match. Device/port rows here are
deliberately non-interactive (no `ROLE_KIND` set) — they don't have a real
plugin/line to act on, so right-clicking one falls through to the generic
fallback (universal actions only) rather than a KIND_DEVICE/KIND_PORT menu
built for context that doesn't exist here.

Builds clean, launched without error. **Not interactively verified** — a
pending universe patched to a specific target should now nest under a
device/port breakout matching the live case's shape, not a flat list.

## Pending patches shown on their real protocol branch, not isolated (2026-09-02) — SHIPPED

Branson: "for interface not patched [pending] .. it makes more sense to
show it where it would be with an error on the line showing why .. vs
isolating in a different tree."

Agreed — the "Interface not present" top-level folder was disconnected
from the rest of the tree even though a pending patch has a real plugin
association (that's exactly what it's waiting on); isolating it buried
which protocol actually has the problem. Restructured `refresh()`: pending
universes are now grouped up front by (plugin, missing interface), and
each group gets a "ghost" interface row nested under its REAL protocol's
top-level branch, right where a real interface row would sit if the
interface existed — red text, a tooltip explaining why, with the affected
universes as its children. The separate top-level "Interface not present"
folder is gone; "Unpatched" (genuinely never-patched universes, which have
no plugin to attach under at all) is untouched, still its own folder, since
that case has nowhere else to go.

One real subtlety: the ghost row has to be added to the plugin's row
BEFORE the existing "if this protocol ended up with zero children, delete
its row (or say 'no lines' if Show Unused is on)" check runs — otherwise a
protocol with ONLY pending patches and no live lines would have its own
top-level row deleted out from under the ghost row just added to it.
Placed the new block immediately before that check, not after.

Builds clean, launched without error. **Not interactively verified** — the
actual repro (a pending universe should now show up nested under its real
protocol, e.g. "ArtNet › 172.18.2.17 (in red) › 3: Universe 3", not in a
separate folder) needs eyes on the running app.

## Resolve a saved ArtNet patch by subnet, not just exact address (2026-09-02) — SHIPPED

Branson: patching to network should target the subnet rather than this
machine's own source IP, and have the system pick the right interface
itself.

**Design choice, stated up front:** the exact match on `outputs()`'s literal
IP strings is what every existing saved workspace already relies on for its
`LineUID`, and changing what `outputs()` RETURNS (subnet strings instead of
addresses) would break exact matching for every workspace already saved —
including on the SAME machine that built it. Implemented as an additional
FALLBACK step in resolution instead: exact match first (unchanged), then
subnet match, then pending (from the portability work earlier today).
Nothing about what gets displayed or saved changes; only what "close
enough" means when the exact address is gone.

New `QLCIOPlugin::lineOnSameSubnet(identity)` (default -1 — most plugins
have no concept of a subnet at all) and `ArtNetPlugin::lineOnSameSubnet()`,
which reuses the exact `QHostAddress::isInSubnet(ip, prefixLength)` check
`probeTarget()` already relies on, just walking `m_IOmapping` the other
direction (does any CURRENT line's subnet contain the SAVED address,
instead of does a target address fall in some line's subnet).
`InputOutputMap::setOutputPatch()` now tries this before giving up and
going pending. A DHCP re-lease changing this machine's own address, or the
same workspace opened on different hardware plugged into the same physical
network, both resolve automatically now instead of going pending — pending
is reserved for genuinely being off that network. Saving again afterward
naturally records the CURRENT address in the patch's `LineUID` (`outputName()`
reads it live off the resolved line), so the mapping keeps drifting to
whichever machine most recently opened and saved it, with no extra code
needed for that self-healing.

New test `ArtNet_Test::lineOnSameSubnetResolvesChangedAddress()` (doesn't
need real system interfaces — builds a `QNetworkAddressEntry` by hand)
pins: same subnet + different host part resolves; different subnet does
not; a non-address string does not crash or false-match.

**Build gotcha hit and fixed along the way:** adding a new virtual to
`QLCIOPlugin` (ahead of `rescan()` in the header, not at the end) shifts
every later virtual's vtable slot. `inputoutputmap_test` initially failed
with a garbage line index — not a logic bug, but `libiopluginstub.dylib`
having been built against the OLD vtable layout in an earlier partial
build, so a virtual call landed on the wrong slot. Fixed by rebuilding
everything (`cmake --build build` with no target, not a targeted one) — a
prompt to remember RESULT after `check-all.sh` next, since a header change
touching every plugin's shared base benefits from a full rebuild, not an
incremental one, to be sure every `.dylib` agrees on the layout.

Full test sweep after the full rebuild: `outputpatch_test` 6/6,
`inputoutputmap_test` 35/35, `artnet_test` 7/7 (all passed, no failures).
Builds clean; launched without error. **Not interactively verified** — the
actual repro (build the file with one local IP, reopen with a different one
on the same subnet, confirm it resolves instead of going pending) needs a
real network change to test, not just a clean build.

## Tree refresh silently no-op'd after almost every action (2026-09-02) — SHIPPED

Branson tested the pending-patch work directly and hit real problems: a
just-patched universe still showed under "Unpatched", the ArtNet screenshot
showed a node "not heard from" sitting oddly, a broadcast patch didn't say
"broadcast" anywhere, and — the one that explains most of the rest —
"the tree didn't update immediately."

**Root cause, one bug explaining the stale-tree symptoms:**
`ConnectionsTree::refresh()`'s guard against rebuilding out from under an
open inline editor was `if (focus != NULL && m_tree->isAncestorOf(focus))
return;`. That matches ANY focus inside the tree, not just an actual open
editor — and the tree's own NORMAL resting state after a right-click (menu,
then any modal dialog it opened) is focus sitting on the tree or its
viewport, restored there once the dialog closes. Every CRUD action added
this session ends in `refresh()`, called synchronously right after that
exact sequence — so the guard was silently skipping the rebuild almost
every time, immediately after the very action that was supposed to show a
result. (The five-second periodic timer then did eventually catch it up,
which is why it looked like "didn't update immediately" rather than "never
updates at all.") Fixed by excluding `m_tree` and `m_tree->viewport()`
themselves from the check — only a genuinely deeper child (the actual
inline `QLineEdit` editor an editable cell creates) still blocks it.

**Broadcast wasn't shown:** `patchTarget()` returns false when a patch has
no explicit target set at all — which its own comment already correctly
calls "broadcast: no single node to sit under" — but the row-building code
only appended a "broadcast"/"unicast" label when `patchTarget()` returned
TRUE, so the plain/default broadcast case (exactly what "patch to this
protocol" / leaving the target IP blank produces) said nothing at all.
Added the missing branch: no explicit target AND the plugin supports
targets at all → "broadcast", matching what the plugin actually does by
default.

Builds clean, launched without error. **Not interactively verified against
these specific repro steps** — re-check: patch an unpatched universe to
broadcast and confirm the row updates immediately and says "broadcast";
confirm a universe never again shows in "Unpatched" once it has a real
patch.

## Footer chip: reason wording + a useful detail list (2026-09-02) — SHIPPED

Same message, two more asks: the footer tooltip should read as "there ARE
patched interfaces missing" rather than reciting universe numbers, and the
click-through details dialog should be "a useful list of data," not a data
dump.

- `App::updateOutputReadiness()`'s registered summary now explains the
  SITUATION — `"N patched network interfaces not present on this
  machine"` — instead of `"Universe N has no output"`, which read like a
  configuration mistake to go fix rather than an expected, resolves-itself
  consequence of being off the show network. (Still distinguishes this from
  the older, unrelated "line index out of range" case, worded separately,
  in the rare event both occur together.)
- `ShowStatus::Entry` grew an `items` field (`QStringList`, one line per
  individually-affected thing) alongside the existing `summary`/`detail`.
  `App::showStatusDetails()` (the click-through dialog) now renders a real
  `QTreeWidget` — one bold top-level row per registered source, its detail
  sentence as an italic child, then each item as its own row underneath —
  instead of one long HTML paragraph per source. Matches the rest of the
  app's own list-based idiom instead of introducing a text dump.

Builds clean, launched without error. **Not interactively verified** —
check the footer tooltip's new wording and open the details dialog to
confirm it reads as a scannable list.

## Pending patches stayed visible as patched + 3-density footer chip (2026-09-02) — SHIPPED

Branson, right after the pending-patch/ShowStatus work landed: (1) a
universe with a pending patch must not present as if it had been unpatched
— "it should leave the patch and not route" — and (2) the footer's "Not
ready" needs to be VERY short, mouseover for some details, and clickable
for everything.

**1. The tree gap:** `OutputPatch::isPending()` correctly makes
`isPatched()` false while pending (nothing should route, right), but
`ConnectionsTree::refresh()`'s "which universes did the plugin-rooted walk
above already draw" tracking follows real plugin+line matches -- a pending
patch's line is `QLCIOPlugin::invalidLine()`, which can never match a real
line index in that walk. So a pending universe fell straight into the same
"Unpatched" bucket as one that had genuinely never been touched, with
nothing to say it was actually mapped to something, just unreachable. Split
`refresh()`'s leftover pass into two: universes with a pending output patch
now get their own "Interface not present" folder (new `KIND_PENDING_UNIVERSE`),
each row naming exactly what it's waiting for (`"ArtNet: waiting for
\"172.18.2.17\""`, e.g.). Its right-click menu deliberately excludes
ordinary patch editing (retarget/transmit mode/etc. — none of it can mean
anything until the interface exists again) and offers only Rename, new
"Forget this patch (unpatch)…" (`ConnectionsTree::forgetPendingPatch()`,
confirms then clears the pending patch — the one deliberate way to actually
lose the mapping, distinct from it happening by accident), and Delete
universe entirely.

**2. Footer chip, three densities:** the chip text is now a fixed, source-
independent `"Not ready"` (or `"Not ready (N)"` for N registered problems)
instead of showing whichever `ShowStatus` entry happened to be worst — it
no longer grows/shrinks/changes wording as sources come and go. Hovering
shows each registered entry's short `summary` line, one per source, ending
in "Click for details" — not the full explanation. Clicking (new
`App::showStatusDetails()`, wired through the existing
`m_statusModeLabel`/`eventFilter()` click pattern already used by the
power and MTC chips) opens a small dialog with every entry's complete
`detail` text. `ShowStatus::Entry` already had both fields from the
original design; this was purely a rendering split that had not been done
yet, not a new data need.

Builds clean; engine unit tests re-run clean (outputpatch_test 6/6,
inputoutputmap_test 35/35 — no regressions from the tree-level change,
which touches only `ui/`). **Not interactively verified** — check the
Devices tab shows "Interface not present" (not "Unpatched") for a pending
universe, and click the footer's "Not ready" chip to confirm the details
dialog opens with real content.

## Portable patches (pending interfaces) + ShowStatus registry (2026-09-02) — SHIPPED

Branson: showfiles get worked on hosts that don't have the network they were
built for; in that case we should keep the mapping but not broadcast
traffic. Agreed design, then implemented, plus a second ask that came with
the go-ahead: register this as a footer warning through a general status
registry other parts of the app can use too, rather than one more
hand-wired check.

**The bug this fixes:** `InputOutputMap::setOutputPatch()` already matched a
saved ArtNet patch's interface by its recorded IP (`outputUID`) against
`plugin->outputs()` first. When that IP wasn't found -- the normal case on
a machine that isn't on that network -- it silently fell back to trusting
the raw numeric line index that came with it. On a host with a *different*
but still in-range set of interfaces, that index could resolve to a real
but WRONG interface and actually open/broadcast on it, with nothing but a
debug-only log to say so. `InputOutputMap::danglingOutputPatches()` (the
existing rig-readiness check) only ever caught the index being out of
range, not "resolved, just to the wrong thing" -- so this exact case was
both dangerous and invisible.

**Engine fix:** new `OutputPatch::setPending(plugin, uid)` /
`isPending()` (`engine/src/outputpatch.{h,cpp}`) and
`Universe::setOutputPatchPending()` (`engine/src/universe.{h,cpp}`). When a
UID doesn't match, `InputOutputMap::setOutputPatch()` now calls this
instead of falling through to the stale index: the patch keeps its plugin
association and the original UID (so `outputName()` — and therefore
`saveXML()`'s `LineUID` attribute — round-trips the SAME identity rather
than collapsing to "None" and losing the mapping for good on next save),
but `isPatched()` is false and nothing is ever opened. `set()` always
supersedes a pending state, so the interface showing up again (open on the
right host, or a hand re-patch) resolves it normally. `DanglingPatch` grew
a `missingInterface` field so `danglingOutputPatches()` reports pending
patches too, alongside the pre-existing out-of-range case (which still
needs its own detection — an older workspace with no recorded UID at all
has no better signal to go on than the index). Scoped to OUTPUT patches
only, matching what was actually asked (input/feedback share the pattern
but weren't touched). Two new unit tests pin the exact behavior:
`OutputPatch_Test::pending()` and
`InputOutputMap_Test::unresolvedInterfaceIdentityGoesPendingNotWrongIndex()`
(the latter specifically proves an in-range-but-wrong index is never
trusted). Ran the full `outputpatch_test`/`inputoutputmap_test`/
`universe_test` suites — 65 passed, 0 failed, no regressions.

**ShowStatus registry:** new `ui/src/showstatus.{h,cpp}` — small QObject
singleton (`ShowStatus::instance()`) other parts of the app register a
named entry into (`setStatus(key, severity, summary, detail)` /
`clearStatus(key)`) instead of writing straight into a footer widget.
`App::updateStatusBar()` now renders whichever entry is worst
(`ShowStatus::instance()->worst()`) generically, connected via one
`ShowStatus::changed()` signal wired in `initStatusBar()` — it does not
know "output.dangling" or any other key exists. `App::updateOutputReadiness()`
is now just the first registrant (`setStatus("output.dangling", Warning,
...)` / `clearStatus(...)`), and its detail text now distinguishes the two
DanglingPatch cases (pending interface vs. out-of-range index) instead of
one generic message. `m_outputReadinessWarning` (the old single-purpose
QString the footer used to read directly) is gone. The point: a future
source (PMJ hardware gone missing, a fixture profile that failed to load,
...) needs zero footer changes, just a setStatus()/clearStatus() call of
its own.

Builds clean (reconfigured for the two new source files); launched and
exited cleanly with no errors in the log. **Not interactively verified** —
the actual footer text/tooltip for a pending patch, and that "not broadcasting"
is really true at runtime with a live ArtNet target absent, need eyes/hands
on the running app (`build/main/qlcconsole -o test-workspaces/surfacetesting.qxw`),
not just clean builds and passing unit tests.

## Devices tree: strict hierarchy, one CRUD kind per level (2026-09-02) — SHIPPED

Branson laid out the actual target shape after the previous round: every
right-click menu should (1) list options in hierarchy order and (2) list
ONLY options that apply at that exact level — nothing from a level above or
below. Concretely: protocol → CRUD interfaces (by IP); interface → CRUD a
target (IP or multicast); target → CRUD ports; port → patch a universe into
it. And explicitly: no "Configure ArtNet…" at the protocol row — that
dialog is the overall config this whole tree is replacing.

This directly reversed the last round's "shortcut" additions (patch a
universe / add a connection straight from the protocol row) — those reached
past the interface level, exactly the violation being called out. Removed.

**Per level, now:**
- **Protocol** (`KIND_PLUGIN`): "Add an interface…" only. A network
  interface is a real host NIC (`QNetworkInterface::allInterfaces()`,
  confirmed in `ArtNetPlugin::outputs()`) that exists whether or not this
  tree currently shows it — `refresh()`'s liveness filter hides an idle one.
  There is nothing to fabricate, only something to stop hiding, so "Add"
  here picks a line (`pickPluginLine()`) and adds it to a new session-
  persistent `m_pinnedLines` set the liveness filter also checks. Honest
  framing, not literal interface creation. "Configure %1…" is now shown
  ONLY for plugins with no per-patch equivalent at all — confirmed
  DMX-USB/MIDI widget settings (speed, mode, …) are genuinely per PHYSICAL
  WIDGET with no tree row to live on — and hidden for target-capable
  protocols (ArtNet), where `ConfigureArtNet`'s dialog turned out to be
  nothing but a second, differently-shaped editor for the exact same target
  IP / ArtNet universe number / transmit mode a target or port row already
  edits directly.
- **Interface** (`KIND_LINE`): "Add a target on this interface…" for
  target-capable protocols — and, new, "Stop showing this interface" (the
  symmetric D, offered only once idle) to unpin one added above. "Patch a
  universe here" and "Patch a universe to a new target…" are GONE for
  target-capable protocols (ArtNet) — patching now always goes interface →
  target → port, never skipping a level. Kept as-is for lines with no
  target concept at all (DMX-USB, MIDI): for those the line already IS the
  endpoint, so this is the bottom of their hierarchy, not a skip.
  "Reroute…" (previous round) stays here too — it is a line-level concern.
- **Target** (`KIND_DEVICE`) and **port** (`KIND_PORT`): unchanged — already
  matched the spec (rename/add-port/forget-target; patch-into-port), no
  hierarchy violation found there.
- **Unpatched universe**: new "Patch to…" — the explicit reverse-direction
  entry point Branson asked for ("for unpatched .. right click .. patch to
  ip/port"). New `patchUnpatchedUniverseTo()` asks protocol → interface →
  (for a target-capable protocol) address/port, i.e. everything a normal
  interface → target → port click-through would have supplied, since there
  is no such row to click yet for an orphan universe.

Also fixed the menu-ORDER half of the ask: the universal actions (Collapse/
Expand, Add/Delete Universe) were being added to the shared `QMenu` before
any per-kind branch added its own items, so they always rendered first
regardless of what was clicked. Restructured so per-kind branches build
their own items first and a new `appendUniversalMenuActions()` appends the
generic ones immediately before whichever of the 9 `menu.exec()` call sites
actually fires — every menu now reads specific-to-this-row first, generic
last.

**Known gap, called out rather than silently built around:** the user's
model names "IP or multicast" as target kinds; this engine's ArtNet target
is a single address field with no separate multicast-specific handling —
typing a multicast group address into that field mechanically works (it's
just an IP), but there's no dedicated multicast UI/validation. Not built,
since inventing one wasn't asked for this round and would need its own look
at what ArtNet 4 multicast actually requires here.

Builds clean; launched and exited cleanly with no errors in the log
(3-second smoke check, not left running — see prior note on why a
background-launched instance doesn't persist for interactive use). **Not
interactively verified** — run it yourself with
`build/main/qlcconsole -o test-workspaces/surfacetesting.qxw` and check:
right-click ArtNet → only "Add an interface…" (+ generic actions after a
separator, no Configure); right-click an interface → only target CRUD, no
direct patch; right-click an Unpatched universe → "Patch to…" is there.

## Devices tree: menu order, IP-first patching, reroute (2026-09-02) — SHIPPED

Branson, on the entry point fix below: "better .." then three more asks in
one message.

**1. Menu ordering should be by hierarchy.** The universal actions (Collapse/
Expand, Add/Delete Universe) were being added to the shared `menu` object
BEFORE any per-kind branch added its own row-specific items, so they always
rendered first — a right-click on "ArtNet" led with "Add Universe" ahead of
anything actually about ArtNet. Restructured so per-kind branches build
their own items first, and a new `appendUniversalMenuActions()` appends the
generic ones right before whichever `menu.exec()` fires (all 9 call sites
now call it there instead of the actions being pre-built once at the top).
Every menu now reads specific-to-this-row first, generic/root-level last.

**2. "When I add an interface, shouldn't be asking for target IP — that
should be by universe, right?"** Correct, and this was a real design
mistake in the previous round: the protocol-level "add a connection" action
went straight to `patchToNewTarget()`, which demands an IP before anything
else — appropriate for aiming at one specific remote node (unicast), wrong
as the default flow for "just start using ArtNet," which normally broadcasts
and needs no address at all. "Patch a universe to this protocol…" (asks
which universe first, same as patching from a line row, no IP) is now
offered for EVERY protocol with any output/input line, including ArtNet —
previously it was withheld from ArtNet on the assumption "Add a connection"
covered it. The IP-first flow is still there as a clearly separate, now
better-labelled "Add a connection to a specific target (by IP)…", for when
unicast to one node is actually the goal.

**3. Reroute.** "if it bound against lo0 .. reroute all bound universes to a
different interface." Genuinely didn't exist — `retargetPatch()`/
`retargetSelection()` only re-aim a universe's TARGET ADDRESS on the SAME
line; nothing moved universes off a line entirely. New: right-click a line
row that has universes patched to it → "Reroute N universe(s) to a different
interface…", `ConnectionsTree::rerouteLine()`. Picks a new line (via the
same `pickPluginLine()` from the entry-point fix), confirms, then re-patches
every affected universe in one pass. Each patch's own settings (ArtNet
target IP, transmit mode, ...) live on the `OutputPatch` object itself and
survive untouched — confirmed by reading `Universe::setOutputPatch()` /
`OutputPatch::set()`, which reuse the existing patch object and only update
plugin+line — so this is a pure "which wire does it leave by" change, not a
re-patch from scratch. Output patches only for now; input/feedback reroute
was not asked for and `InputOutputMap::setInputPatch()` replaces the whole
patch object rather than reusing it, so it would need its own look at
whether the profile survives before doing the same trick there.

Builds clean, smoke-tested. **Not interactively verified** — try right-
clicking ArtNet with nothing patched (should lead with protocol actions,
"Patch a universe to this protocol…" should NOT ask for an IP), and
right-clicking a line with universes on it for the new Reroute action.

## Devices tree had no way in for a protocol with nothing patched yet (2026-09-02) — SHIPPED

Branson: "this doesn't allow to add an interface under artnet for instance
which it should," and laid out the model he wants: root = protocol (CRUD),
under a protocol = connection (CRUD, plus type-specific config), under a
connection = patch universe (CRUD). Most of that already existed in the
engine and the per-row menus (see the two entries below this one) — the
actual bug was narrower and explains "can't add an interface" literally:
`refresh()`'s liveness filter hides a LINE row (ArtNet's NIC, e.g.) until
something is already patched or heard on it, and "Show unused" defaults
off. So a protocol with nothing patched anywhere had every one of its lines
invisible, and every "add a target" / "patch a universe" action lived on a
line row — there was no way to reach them without first ticking "Show
unused", which nothing prompted anyone to know to do. Chicken, egg.

Two things fixed this without touching the visibility filter itself
(reintroducing every idle NIC by default was the exact clutter it exists to
avoid): the protocol (plugin) row's own context menu now offers "Add a
connection (target)…" for target-capable protocols (ArtNet today) and
"Patch a universe to this protocol…" for the rest (E1.31, OSC, and anything
future that patches directly without a per-node target) — both reachable
with zero visible lines. Neither needed a pre-existing line row: new
`ConnectionsTree::pickPluginLine()` resolves which line to use itself (the
only one if there's just one, otherwise a picker matching the same "alias
(NIC name)" labelling every line row already uses), then hands off to the
SAME `patchToNewTarget()` / `patchUniverseTo()` these actions already call
from a line row — no new patch logic, just a new way to reach it.

Root-level "CRUD a protocol" itself was not built as literal add/delete:
protocols are fixed compiled-in plugins (ArtNet, MIDI, DMX-USB, …), not
something a workspace can create or remove — "Configure %1…" (already
existed) is the U, the tree row is the R. Worth saying explicitly since it
diverges from the requested shape rather than silently doing something
different.

Builds clean, smoke-tested. **Not interactively verified** — try right-
clicking "Art-Net" (or another protocol) in Devices with nothing patched
yet; "Add a connection (target)…" should be there and should not require
ticking "Show unused" first.

## Devices tree hid every unpatched universe (2026-09-01) — SHIPPED

Branson, right after the round-2 right-click fix landed: "universe1 not
showing up in there.. neither is the one i added in the rigth click." Root
cause was a step deeper than the right-click gap itself: `ConnectionsTree`
(the "Devices" sub-tab) is rooted at `cache->plugins()` — protocol → line →
device → port → universe — so a universe only ever gets a row as a *child of
its patch*. A universe with nothing patched to it has no plugin/line/port to
attach under, so it never rendered at all, regardless of whether it existed
in the engine. That is true of Universe 1 on a fresh workspace (never
patched to anything) and of literally every universe `addUniverse()` can
ever create (a bare universe has no patch by definition) — so the brand-new
right-click "Add Universe" always produced a universe that then looked like
it had failed to appear.

Fixed in `ConnectionsTree::refresh()`: the existing per-plugin summary pass
already walks every row once to count devices/universes for the interface
detail text, so it now also collects every universe id it sees
(`patchedUniverses`) as it goes. After that pass, anything in
`m_doc->inputOutputMap()->universes()` NOT in that set gets a synthetic
top-level "Unpatched" folder with one child row per leftover universe (new
`KIND_UNPATCHED_UNIVERSE`). Right-click on one of those rows offers Rename
and Delete (reusing the existing `renameUniverse()`/`deleteUniverse()` —
`deleteUniverse()` already self-guards "only the last universe can go", so
no new guard logic needed); patching a row belongs to Overview/Detailed as
before, so no patch UI was added here.

Follow-up, same session: "no crud when right click cause every row is
used... how to fix." The universe visibility fix above didn't reach the
actual complaint from round 2 — the empty-space-only "Add Universe"/"Delete
Universe" menu (`item == NULL` case) is only reachable when the tree has
visible empty space below its rows, which a normally-patched rig (the usual
case, and now literally every row given the fix above) doesn't have. Empty
space was never a real route to Add/Delete Universe, just a theoretical one.

Fixed by offering Add/Delete Universe on every row's context menu, not only
empty space. `slotContextMenu()`'s per-kind branches (KIND_PLUGIN/DEVICE/
PORT ×2/LINE/UNIVERSE/UNPATCHED_UNIVERSE) each build and `exec()` their own
`QMenu`, but all reuse the SAME shared `menu` object built once at the top
of the function (already the case for the pre-existing "Collapse/Expand
everything below" actions) — so `addUniv`/`delUniv` actions added to that
shared object right after collapse/expand now show up in all of them. New
private helper `handleUniversalMenuAction()` centralizes the post-exec
handling (collapse/expand + add/delete universe) so each of the 8 exec()
call sites is a single delegating line instead of duplicated logic — same
shape as the existing collapse/expand check it replaces, at every site that
already had one.

Builds clean, smoke-tested. **Not interactively verified** — right-click any
row in Devices next time in the app; "Add Universe" (and "Delete Universe
[last]…" once one exists) should appear at the top of every menu, not just
on empty space.

Follow-up regression, caught by Branson actually clicking around: "can't add
device again on the devices page .. we had fixed that?" The "Unpatched"
folder heading added by the fix above (see the entry below this one) has no
`ROLE_KIND` — it's a label, not a plugin/line/device/universe — so it
matched none of `slotContextMenu()`'s `if (kind == KIND_X)` branches, each
of which builds AND execs the menu itself. Falling through all of them meant
the menu was built (with Add/Delete Universe on it) but never shown —
right-clicking the folder header did nothing, at all, silently. Fixed with a
fallback `menu.exec()` at the end of the function for any row that matches
no specific kind. Builds clean, smoke-tested; not re-verified interactively.

## Connections right-click, round 2 (2026-08-18) — SHIPPED

Branson: "still cannot right click to add/manage connections" after the
earlier fix — that fix only reached the "Detailed" sub-tab's universe
list. Connections actually has three sub-tabs (Devices/Overview/Detailed),
each a structurally separate widget; right-click needed checking on all
three, not assumed from one.

- **Overview** (`UniversePatchGrid`) — had Add/Remove Universe on its own
  toolbar already (`onAddUniverse()`/`onRemoveUniverse()`), but no
  right-click at all. New `onContextMenu()`, wired to the table's
  `customContextMenuRequested`, reuses those same slots.
- **Devices** (`ConnectionsTree`) — Branson overrode the "read-only on
  purpose" comment directly: "I DO want to be able to manage adding
  devices in there." That comment is gone (was stale anyway — the tree
  already had rich per-row editing via right-click: rename/patch/unpatch/
  retarget/delete a specific universe; the only real gap was that
  right-clicking *empty space* did nothing at all, `if (item == NULL)
  return;`, since "add a universe" isn't a property of any existing row).
  New `ConnectionsTree::addUniverse()` (mirrors the existing
  `deleteUniverse()`'s undo-capture pattern) wired into that empty-space
  case: right-click empty space in Devices now offers "Add Universe" and
  "Delete Universe [last one]…" (reusing `deleteUniverse()`'s own already-
  solid "only the last can go" guard/confirm/undo logic, not duplicated).
  Deliberately did NOT thread these into the existing per-kind branches
  (KIND_PLUGIN/DEVICE/PORT/LINE/UNIVERSE each build and `exec()` their own
  menu separately) — too much surface to touch safely in one pass; the
  empty-space case was the actual gap and the lowest-risk fix for it.
- **Detailed** — already had it from the earlier round, unchanged.

Right-click now works on all three Connections sub-tabs. Builds clean,
smoke-tested. **Not interactively verified** (no GUI-automation path for
right-click context menus in this session) — try right-clicking empty
space in Devices, the Overview grid, and the Detailed universe list next
time in the app.

---

## Footer consolidation round 2 (2026-08-18) — SHIPPED

Branson, from a footer screenshot: the rig-readiness warning sat far to
the right (past DESIGN), disconnected from "Ready" on the far left even
though a readiness problem is exactly what that slot should mean; and
Saved/Unsaved/Autosave were three separate chips that could be one.

- **Output-readiness warning moved into `m_statusModeLabel`'s own slot**
  (far left) instead of a separate right-side chip (`m_statusRigLabel`,
  removed). New `m_outputReadinessWarning` (QString, empty when fine) —
  `updateOutputReadiness()` sets it and calls `updateStatusBar()`, which
  now shows it (red, bold) in priority over both the transient
  `m_statusMessage` and the idle "Ready" text. The detailed per-universe
  tooltip moved with it onto `m_statusModeLabel`.
- **Unsaved/Autosaved/Saved consolidated into one chip** (`m_statusDirtyLabel`;
  the separate always-visible `m_statusAutosaveLabel` "Autosave: Enabled"/
  "Last autosave: HH:MM:SS" chip is gone). Driven by three existing signal
  points, no new tracking state needed: `slotDocModified(true)` →
  "● Unsaved changes" (orange), the autosave-completion point in
  `saveXML()`'s autosave path → "Autosaved HH:MM:SS" (gray),
  `slotDocModified(false)` (a real manual save) → "✓ Saved" (gray).
  Autosave deliberately does NOT clear `Doc::isModified()` (confirmed in
  `saveXML()` — only a real save does, via `resetModified()`), and
  `Doc::setModified()` emits `modified(true)` unconditionally on every
  edit, not just the dirty transition — so the very next edit after an
  autosave re-fires `slotDocModified(true)` and flips the chip back to
  "Unsaved changes" on its own, correctly, with no extra bookkeeping.
  "Autosave: Disabled" as a persistent chip is gone too — matches
  Branson's explicit 3-state ask; still configurable via Preferences, just
  no longer occupying constant footer space.

Builds clean, smoke-tested (stable, no crash). **Not visually confirmed** —
tried reading label text via `osascript`'s accessibility bridge this round
specifically (not just resizing/window-existence checks like earlier
rounds), but Qt's static-text elements didn't expose readable values that
way. Worth an eyeball check of the actual chip text/colors next time in
the app, especially the readiness-warning priority-over-"Ready" logic and
the autosave→edit→"Unsaved changes" flip.

**GM/Blackout — resolved differently than proposed, SHIPPED**: recommended
consolidating the footer presentation (one combined "is output actually
happening" chip); Branson redirected instead to "add preference to show GM
in bottom as option" — GM's footer presence is now a View menu toggle
(`m_showFooterGMAction`/`m_showFooterGM`, persisted
`workspace/showFooterGM`, defaults on), same pattern as the existing Load/
Power chip toggles. GM and Blackout stay fully separate elements; the
"consolidate" question was answered by making GM optional rather than
merging it with anything.

**Readiness text simplified**: dropped "NOT READY -" from the warning —
Branson: it "doesn't need to say NOT READY, just needs to be either
'ready' or 'why not' in red." The red bold styling already says "this is a
problem"; the text is now just the reason ("Universe %1 has no output"),
matching "Ready"'s own plain phrasing in the fine case.

Builds clean, smoke-tested. **Not visually confirmed**, same caveat as
everything else in this round.

---

## Toolbar real-estate + title bar (2026-08-18) — SHIPPED

Branson: a Fixture Manager toolbar screenshot showed icon+text spacing so
wide it was "untenable." Two asks: make toolbars more space-efficient, and
consider moving controls into the title bar.

- **Manager toolbar icon size, unified at 20x20** — `fixturemanager.cpp`,
  `functionmanager.cpp`, `showmanager.cpp` (both its main and bottom
  toolbars) never called `setIconSize` at all, falling back to a large
  platform default; `inputoutputmanager.cpp` was 32x32, `virtualconsole.cpp`
  was 26x26. All six now match `showtimelineeditor.cpp`'s existing 20x20
  rather than inventing a new size. This alone shrinks every manager
  toolbar meaningfully, especially combined with long action labels
  ("Channels Fade Configuration") under the existing default
  icon-above-text label mode (itself already a user-toggleable View-menu
  setting, unchanged).
- **App-level toolbar merged into the title bar** — `setUnifiedTitleAndToolBarOnMac(true)`
  in `App::initToolBar()`, macOS-only. Only reaches the small app-level
  toolbar (Panic/Blackout/Blind/Operate); the per-manager toolbars (like
  the one in the screenshot) are embedded inside each tab's own widget via
  `layout()->setMenuBar()`/`addWidget()`, not QMainWindow toolbars, so this
  API structurally can't reach them — confirmed with Branson before
  building, scoped to just the app-level one.

**A real puzzle surfaced while verifying this, worth recording honestly
rather than glossing over**: re-checking the window-minimum-width fix from
earlier today (`QSizePolicy::Ignored` on tab pages), the minimum had grown
from the previously-confirmed 791px to 1111px. Bisected hard — reverted the
title-bar merge (call removed entirely, not just set false), reverted all
six icon-size changes, and cleared the persisted `workspace.geometry`
setting (`defaults delete org.qlcplus.qlcconsole "workspace.geometry"`,
confirmed via a *different* initial window size afterward that this wasn't
just a stale restore) — none of it moved the number. Every specific
hypothesis from today's own changes came back negative. What matters:
**1111px is still comfortably under the 1512px screen and the window
resizes freely** — the actual bug (wider than the screen, unresizable) is
confirmed gone; the 791→1111 shift is unexplained but not a regression of
the resizability fix itself. Mid-investigation, `osascript`/System Events
stopped being able to query windows for *any* app (Finder, Safari, the
frontmost process itself all failed identically) — a macOS Accessibility
session issue unrelated to qlcconsole, not something fixable from in here.
If this is worth chasing further, it needs a real interactive session
(Instruments/Qt's own layout debugging, not blind bisection from outside).

Builds clean throughout. **Not visually confirmed** — same caveat as
everything today, compounded by losing GUI-automation access partway
through; worth a real look and a real resize-by-hand next time in the app.

---

## 🎨 Visual consistency review (2026-08-18) — SHIPPED (the confirmed bugs)

Full findings: [VISUAL_CONSISTENCY_REVIEW.md](VISUAL_CONSISTENCY_REVIEW.md).
Separate from the workflow UX review above — spacing/icons/color/theming,
requested directly ("did you also evaluate look and feel for consistency").
Key fact: `App::applyTheme()` does a real app-wide `QPalette` swap, so ANY
hardcoded hex color is a deliberate theme opt-out, not just style — and two
footer chips added earlier today (mode chip, Show Lock) had exactly that
bug, confirmed via computed contrast against all 3 dark themes. **Fixed**:
mode chip now uses `palette(text)`/a higher-contrast green; Show Lock moved
off Blackout's identical red onto the app's existing `#a06000` amber
convention, so it no longer reads as the same alarm tier as Blackout.

**Still open, ranked in the doc's own "order of attack"**: toolbar icon
size unification (24/26/32/platform-default/none across 5 managers — the
single most visible inconsistency found, since every tab-switch shows it);
LookEditor's Color-page sliders missing numeric value labels (a genuine
loose end from this session's own slider work, Dimmer page has them,
Color doesn't); Delete/Remove icon consolidation (3 different icons, one
concept); one more isolated dark-theme legibility bug in
`timecodecalibrationdialog.cpp`; everything else (icon family drift,
semantic-red consolidation, section-header convention, dialog margins,
Save button's one-off arm/confirm pattern, tooltip voice) — lower
urgency/broader surface, deferred to a dedicated pass.

---

## Main window unresizable / off-screen — real bug, found and fixed
(2026-08-18) — SHIPPED

Branson reported the window couldn't be resized narrower and was hanging
off the edge of the display. First hypothesis was today's own footer work
(GM fader, mode chip) — reasonable given recency, but WRONG: trimming the
GM slider's `setFixedWidth(80)` to a `setMaximumWidth`, and
`m_statusModeLabel`'s hard `setMinimumWidth(300)` down to 160, had zero
measurable effect on the window's minimum width. Verified this empirically
throughout rather than trusting code-reading alone — `osascript`/System
Events driving the real built app, resizing the actual window and reading
back its real size, since there's no screenshot tool available here. That
discipline is what caught two dead-end fixes before landing the real one:
a speculative `ElideRight` swap for the tab bar's `ElideNone` (a 2021
upstream macOS workaround, `ff84d8047`, "try to workaround TabWidget
icon+text issue") also didn't move the number.

**Root cause**: `QTabWidget`'s internal `QStackedWidget` sizes itself to
the LARGEST of ALL its pages by default — documented Qt behavior — not
just the currently-visible one. So whichever of the app's 8 tabs
(Connections, Fixtures, Lighting Studio, Functions, Programming, Shows,
Virtual Console, Simple Desk) happens to need the most width sets the
floor for the WHOLE WINDOW, regardless of which tab is active. Confirmed
by switching the active tab (Connections ↔ Fixtures) and seeing the
minimum stay bit-for-bit identical at 1525px — on a 1512px-wide display,
2px over what even a maximally-shrunk window could fit, so the app opened
wider than the screen and couldn't be pulled back in.

**Fix**: `app.cpp`'s shared `addTab` lambda (the single choke point all 8
tabs already funnel through) now sets `QSizePolicy::Ignored` on both axes
for every tab page right before adding it — the standard, documented
pattern for excluding a stacked page from that max-of-all-pages
aggregation while hidden; it still lays out and renders completely
normally once it IS the current page. Verified: window now opens at
1510px (fits the screen), resizes freely down to 500×400 and back up to
1400×900, and tab-switching (tested Virtual Console/Simple Desk/
Connections) still works with no crash. The speculative `ElideRight`
change was reverted back to `ElideNone` once the real fix was confirmed —
it turned out unnecessary (the remaining tab-bar-driven minimum with
`ElideNone` restored is ~791px, comfortably inside the screen), so there
was no reason to risk reintroducing whatever 2021 rendering bug
`ElideNone` exists to work around. The GM-slider and mode-label width
trims were kept regardless — real improvements (letting the layout
actually compress under pressure instead of a hard, unshrinkable floor),
just not the fix for this specific bug.

Builds clean; this one got an actual behavioral check via `osascript`; the
tab-switch/crash check above ran against the real binary too — more
verification than most of today's changes got.

**Follow-up**: Branson noticed moving the GM fader reflowed every chip to
its right, since the value label's width floated with its text ("1%" vs
"100%"). Fixed to the widest possible reading, same reasoning
`m_statusLoadLabel` already used elsewhere in the footer. Reverified the
resize fix still holds afterward (791px minimum, unaffected by 2 extra
pixels) — no regression.

---

## Footer chip consistency pass (2026-08-18) — SHIPPED

Started as two follow-up asks after the workflow UX review's GM/mode-chip
work: move GM to the left of the footer (next to the mode-message label,
`addWidget` not `addPermanentWidget` — non-permanent, like
`m_statusModeLabel`, since this app doesn't use `statusBar()->showMessage()`
so there's no flicker risk), and make the Design/Operate chip itself
clickable (`installEventFilter`, triggers the same `m_modeToggleAction` the
toolbar button does — one control, not two).

That led to a real design question from Branson: the footer had accumulated
several different "how do we show state" idioms with no shared logic —
Blackout used a `●` dot, Show Lock a 🔒 emoji, Timecode mixed an emoji AND a
dot, Power had an emoji with no dot, the new Mode chip had neither. Landed
on a two-tier answer: **alarm-tier states that are meant to interrupt**
(Blackout, Blind) keep their distinct, deliberately-loud treatment — Blind's
whole-footer-blue and Blackout's dot are staying different from each other
and from everything else on purpose, that's the point. **Passive-readout
chips** (Mode, Timecode, Power, Show Lock, engine Load) converge on one
plain style: bold text + a meaningful color, no decorative emoji icon.
Dropped 🔒 (Show Lock), ⏱︎ (Timecode, 4 call sites), ⚡︎ (Power), ⚙︎/⏱ (Load).
**Deliberately kept**: Blackout/Show Lock's own `●`-in-Blackout and
Timecode's internal `◌`/`●`/`❚❚` state glyphs, Dangle's `⚠︎`, Dirty's `✓`,
Timeline-suspended's `❚❚` — Branson's own distinction, confirmed:
these carry real at-a-glance state meaning (same job as Blackout's dot),
not decorative branding icons, so they're a different thing from the emoji
prefixes that got removed. Show Lock's text also moved from an inline HTML
`<span style=...>` to a `setStyleSheet()` call at construction, matching
every other chip's convention, and its display text changed from "Show
locked" to "SHOW LOCKED" to match the all-caps convention Blackout/Mode
already use.

Builds clean, smoke-tested each step. **Not yet visually confirmed** —
this was a rapid design-question → implementation cycle without a
screenshot checkpoint; worth a look next time the app's open.

---

## 🔍 Workflow UX review (2026-08-18)

Full findings: [WORKFLOW_UX_REVIEW.md](WORKFLOW_UX_REVIEW.md). Requested
while away from PMJ hardware: a discoverability/simplicity review across
fixture setup, Programming-tab look-building, Function Manager/Show-Timeline,
and Virtual Console/live-operating. Core pattern found: the app already has
the right answer in several places (empty-state hints, footer safety chips,
content-aware canvas tiles) — it's just not applied everywhere it's needed
yet. **A separate look-and-feel/visual-consistency pass (spacing, icon sets,
color usage, theme compliance) was explicitly NOT part of this review and is
still open** — Branson asked directly, flagging it rather than letting it
slide.

**Shipped (2026-08-18), the pure-mechanical items — no design decision
needed:**
- Programming tab's Save button renamed "Save Positions" + clarifying
  tooltip (was mislabeled as general Save; ordinary look edits are already
  tracked by the existing app-wide footer "● Unsaved changes" indicator,
  `programmingmanager.cpp`).
- Tab tooltips distinguishing Fixture Groups vs. Channel Groups
  (`fixturemanager.cpp`); clarified tooltips on the shared Add-group action
  and the dynamic Add-action tooltip ("Add channel group..." vs. "Add
  fixture...").
- Head-layout-grid tooltip on `CreateFixtureGroup`'s size fields
  (`createfixturegroup.ui`).
- Palette-type tooltips in the Programming tab's "New…" menu for all nine
  types, with Pan/Tilt vs. Aim spelled out explicitly since they look like
  synonyms and aren't (`programmingmanager.cpp`;
  `QMenu::setToolTipsVisible(true)` was needed too — off by default on most
  platforms including macOS).
- Programming tab's empty-palette-tree guidance folded into the existing
  canvas placeholder text (both the constructor default and the runtime
  `setText()` call) rather than a new separate hint.
- Also: builds now use performance cores only, not `hw.ncpu`
  (`CLAUDE.md`, `check-all.sh`) — Branson asked for this directly mid-review;
  see memory `build-core-moderation`.

**Shipped (2026-08-18), design decisions made:**
- **Grand Master promoted to a compact footer fader** — Branson chose "always
  visible/adjustable" over a readout+popup. New `m_statusGrandMasterSlider`/
  `m_statusGrandMasterValueLabel` in `app.cpp`'s footer, wired to the same
  `InputOutputMap::setGrandMasterValue()`/`grandMasterValueChanged()` hooks
  the VC-embedded `GrandMasterSlider` uses, so both stay in sync. Deliberately
  a new compact widget rather than reusing `GrandMasterSlider` itself — that
  class is a tall vertical sidebar widget (min. 100px), not built for a
  ~20px status-bar strip.
- **Persistent Design/Operate mode chip** — new `m_statusModeChipLabel`,
  updated in `slotModeChanged()` alongside the existing destination-mode
  toggle button (left untouched). Green "OPERATE" / gray "DESIGN".
- **Chaser-step and Show-timeline content swatches** — Branson chose a color
  swatch over a mini-preview thumbnail. New `AppUtil::sceneSwatchColor(doc,
  scene)` (`apputil.{h,cpp}`) — the RGB of the first Color-type palette a
  scene references, invalid `QColor` (no swatch, falls back to the generic
  icon) if it references none, e.g. a pure pan/tilt or dimmer-only scene.
  Wired into `ChaserEditor::updateItem()` (replaces the generic per-type
  icon with a swatch dot when available) and `SceneItem::paint()` on the
  Show timeline (a small dot drawn top-left, deliberately separate from
  `ShowItem`'s existing top-right type badge and from the block's own
  `m_color`, which is a user-assigned organizational color unrelated to
  scene content — conflating the two would have been a real bug). Sequences
  (chaser-driven timeline items) were NOT touched — a sequence has multiple
  per-step scenes with potentially different colors, no single obvious
  swatch, and its lack of inline editing is already a separately-flagged
  open gap (see item 1 below).
- **Naming-convention nudge** — Branson chose placeholder-text-only, but
  there's no dialog/QLineEdit at scene/chaser creation to put placeholder
  text on (creation is silent auto-name + tree-inline-rename, and Qt's
  built-in tree item editor doesn't support placeholder text without a
  custom delegate — real extra engineering, out of scope for "cheap").
  Landed the same-spirit, actually-cheap equivalent instead: tooltips on
  the "New scene"/"New chaser" toolbar actions
  (`functionmanager.cpp`) spelling out the convention with a concrete
  example. Also **deliberately did not hardcode a "learn from siblings"
  auto-namer** — CLAUDE.md is explicit the convention should be learned
  from sibling names, not hardcoded, and no such inference logic exists
  anywhere in the codebase to reuse; building one from scratch is a real
  feature, not a nudge, and wasn't what was asked for here.
- **Connections-tab onboarding hint** — the actual first tab a new
  workspace opens on (not Fixture Manager). A fresh `Doc` always has 4
  default universes (`Doc::Doc`'s default arg), so "count == 0" never
  applies here the way it does for Fixture Manager's fixture tree — the
  hint instead shows whenever none of them have any input/output patch
  yet, hiding once the first one does (`InputOutputManager::updateList()`,
  new `m_onboardingHint`).

**Shipped (2026-08-18) — SceneItem double-click:**
- **Timeline Scene clips now have an edit action** — double-click a bare
  Scene block on the Show timeline (`SceneItem::mouseDoubleClickEvent()`,
  new) and it jumps to the Programming tab with that scene loaded, reusing
  `ProgrammingManager::showFunction(fid)` — an existing public entry point
  ("exactly as clicking it in the nav tree would") that just had nothing
  wired to call it from the timeline before. New
  `App::switchToTabContaining(QWidget*)` does the actual tab-switch
  (`m_tab->indexOf()`/`setCurrentIndex()`), reached from the
  QGraphicsItem via the same `qApp->topLevelWidgets()` →
  `qobject_cast<App*>` → `findChild<ProgrammingManager*>()` pattern already
  used elsewhere (`monitor.cpp`'s Quit handling; `pmjoverlay.cpp`'s own
  reach-pattern this session). Chose "jump to Programming tab" over a
  separate Scene Editor dialog — that's where this fork's actual editing
  workflow lives (CLAUDE.md), a legacy raw-channel Scene Editor would be
  the wrong destination for this fork specifically.
- **Sequences deliberately NOT touched in this pass** — `SequenceItem` has
  real per-cue complexity already (`cueAt()`, `selectCue()`, per-cue drag
  state) that a double-click handler needs to account for (which CUE was
  double-clicked, not just "the sequence"), and DONE.md's existing
  "Sequences: edit in Functions tab hint" note is really the same
  underlying gap — deserves its own focused pass rather than bolting on a
  rushed version here.
- Builds clean, smoke-tested (starts/runs fine). **Not interactively
  verified** — double-clicking a `QGraphicsScene` item can't be driven via
  `osascript`/System Events (scene items aren't separate accessibility
  elements the way native widgets are), so this needs a real click-test in
  the app: open a Show with a Scene clip on the timeline, double-click it,
  confirm it lands in the Programming tab with that scene loaded.

**Item 2 (smaller/lower-urgency batch) — shipped (2026-08-18):**
- **Cue list next-cue indicator — investigated, no fix needed.**
  `VCCueList::setFaderInfo()`'s orange `#FF8000` highlight
  (`vccuelist.cpp:1164`) has no gating on crossfade-panel visibility at
  all — traced its only caller, `slotCurrentStepChanged()`, and the
  `FaderMode::None` branch (no crossfade fader configured, the common
  case) calls it unconditionally too. The original review finding was
  imprecise; didn't force a speculative change against a check that
  already appears correct on inspection — flagging instead of guessing.
- **Design→Operate checkpoint** — mirrors `slotModeDesign()`'s existing
  "there's something you might lose" warning, but gated specifically on
  `Doc::isProgrammerDirty()` (uncommitted pad/Programming-tab edits — colors,
  positions, etc. never saved into a scene), not `isModified()`, which
  is true almost continuously while actively building a show and would
  have reintroduced exactly the "friction on every toggle" the design
  call was meant to avoid.
- **SceneGroupLooks drop-zone precision** — `dropEvent()` now checks which
  column (`m_targetList` vs `m_lookList`, both direct children of
  `SceneGroupLooks` so their `geometry()` is directly comparable to the
  drop position) a palette/fixture-group actually landed on, rejecting
  (with a `QToolTip` explaining why — the original finding noted there
  was no rejection/hint path at all) only the CLEAR wrong-column cases;
  drops landing between/outside both lists still fall through to the
  previous permissive behavior.
- Builds clean, smoke-tested (starts/runs). **Not interactively verified**
  — same caveat as item 1: needs a real click-test (try dropping a palette
  on Targets, a fixture group on Looks, toggle to Operate with an
  uncommitted pad edit) next time in the app.

**Still open:**

1. Timeline vocabulary (Track/Cue/handles) never explained in-app — no
   glossary, first-run hint, or "?" affordance anywhere in the Show
   timeline. Lowest urgency of what's left; not yet designed.

---

## 🔌 Connections tab — remaining

*(Shipped work moved to [DONE.md](DONE.md), 2026-08-28.)*

- **Hardware verification — status.** Use `build/tools/artnetprobe/artnetprobe`
  (`--selftest` / `--poll <addr>` / `--listen`).
  - ✅ **Arrival-interface reporting**: verified 2026-08-28 against live
    traffic. Real ArtPollReplies from 172.18.2.218 and 172.18.2.10 were both
    correctly attributed to `vlan0`, 5/5 datagrams carrying an index. This is
    the routing fix confirmed on real gear, not a simulation.
  - ✅ **USB devices appear**: DMXKing ultraDMX Micro and OpenDeck PMJ_BLACK_1
    both listed under DMX USB / MIDI.
  - ⚠️ **Off-segment unicast probe**: unproven. Every node reachable from this
    host is on a subnet it has an interface on, so it answers broadcast polls
    too and the reply is indistinguishable. Needs a node on a subnet with no
    local interface.
  - ⚠️ **Hand-declared target actually passing DMX**: unproven, and NOT
    testable from one host. Console and probe both bind 6454 with
    `ShareAddress`; broadcast reaches every bound socket but unicast reaches
    exactly one, so the probe cannot see the console's own unicast output.
    Run the probe on a different machine from the console.
- **Devices/Overview parity — done.** Bulk multi-row retarget with port
  auto-increment, feedback IP/port, ArtNet `inputUni`, MIDI output mode, MIDI
  input channel and the MIDI Out-vs-Feedback role swap have all landed on the
  Devices tab. Universe passthrough and the plugin
  description/status have since moved onto Devices too. Still Detailed-only
  and staying there deliberately: the Audio tab, input-profile
  creation/editing (`InputProfileEditor`), and the USB hotplug toggle (an app
  preference more than a patching control). The split now reads as
  "Devices = the rig and its wiring, Detailed = audio and profile editing".
- **Patch undo — remaining limits.** `PatchUndo` (engine/src/patchundo.{h,cpp})
  is ONE step deep: a second change discards the first, deliberately, since a
  state two changes stale would restore onto a rig that has moved under it.
  It covers patches and the universe list (add/delete/name/passthrough) from
  all three tabs, but nothing outside the patch — fixtures, functions, scenes.
  Ctrl+Z is scoped to the Connections widget (`Qt::WidgetWithChildrenShortcut`)
  so it cannot promise general undo elsewhere in the app.
- **`InputOutputMap` universe id vs index — not a live trap after all.**
  `universe(id)` resolves by ID while `outputPatch()`/`setOutputPatch()`/
  `setInputPatch()` take an ARRAY INDEX, but id == index is enforced
  deliberately at both mutation points, not merely coincidental:
  `addUniverse()` assigns the next index as the id, refuses an id already
  present, and fills gaps so a higher id still lands at its own index;
  `removeUniverse()` refuses anything but the last entry. The contract was
  written nowhere, so it is now pinned by
  `InputOutputMap_Test::universeIdAlwaysEqualsItsArrayIndex()` — relax either
  rule and that test fails instead of the app repatching the wrong universe.
- **Reachability probing — verify on `ender`.** Rescan now unicasts an ArtPoll
  at every address believed in but not heard from (hand-declared targets and
  addresses named by `outputIP`), so a node on another subnet can answer. Never
  tested against a real off-segment node. Note it probes only on an explicit
  Rescan, not on the 5 s tick — a live "is it up" indicator would need a
  cadence decision, and continuous unicast polling of every configured node is
  exactly the traffic this has been avoiding.
- **Probing is Art-Net only.** `QLCIOPlugin::probeTarget()` defaults to a
  no-op; nothing else implements it. Fine today, since Art-Net is the only
  plugin with addressable targets at all.

---

## ✈️ Travel / offline work — no show rig or control surface needed

Buildable + testable on a laptop (offscreen QTest / node for JS effects; the app
runs headless via `QT_QPA_PLATFORM=offscreen`). Good picks while away from the rig:

- **More one-shot effects** — now that the lifecycle exists (`EFFECT_LIFECYCLE_DESIGN.md`),
  author bursts / reveals / sweeps as `oneshot` scripts (pure JS, node-testable).
  A `wand.js` is the template.
- **Effect-lifecycle follow-ups** — `span` sync on the **Show timeline** (today falls
  back to naturalDuration); fold the RGBScript `Once` path into the lifecycle;
  per-look `syncTo`/`onFinish` override UI.
- **Audio effects** — bump the FFT band count / raise the 5 kHz cap for finer Hz
  targeting (node-testable); more audio-reactive scripts.
- **Rebrand → qlcconsole** — titles / About / launcher / macOS bundle names. No hardware.
- **Design-doc work** — "Look" as first-class assembly unit; unified object editor.
- **More stage objects** — flats / drapes / set pieces (2D monitor, offscreen-testable).
- **Small polish** — MTC-chip already done; any bugs found reviewing the code.

**Parked until back at the rig:** the whole **control-surface** effort (PMJ / APC40
mk2 / Xbox — needs the boards) and the **`RIG_TEST_PLAN.md`** verification pass
(needs movers / pixel panel / MIDI keyboard / audio in). The move-in-black + note-
effect timing items also want the rig to confirm.

---

## Recently shipped (verify on rig, then move to DONE.md)

- **PMJ Blackout LED bug — SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}`.
  Branson: "the blackout button still doesn't light on the board." Root
  cause: `PMJOverlay` was never listening for `InputOutputMap::blackoutChanged`
  at all — only Blind's `outputInhibitedChanged` and grand master were wired,
  despite a stale doc comment claiming blackout was covered too. Pressing `O`
  correctly toggled blackout every time; nothing ever told the engine to
  repaint LEDs afterward, so the LED just sat at whatever it was on connect.
  Added `slotBlackoutChanged`, mirroring the existing Blind/GM pattern
  exactly. Builds clean, smoke-tested. **Not yet re-verified on the real
  board.**

- **P2 slice 1 — selection mode: Select/Load wiring + per-fixture intensity
  faders — SHIPPED (2026-08-18).** `engine/src/programmercontroller.{h,cpp}`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/programmingmanager.{h,cpp}`. First
  buildable piece of the P2 design (`CONTROL_SURFACE_DESIGN.md`) agreed
  after Branson asked for "a cohesive think" on tying a whole SCENE to the
  faders, not just one focused palette.
  - **New engine primitive**: `ProgrammerController::writeChannelLive
    (fixtureId, channel, value)` — general-purpose, palette-agnostic raw DMX
    write, mirroring `VCSlider::writeDMXLevel`'s mechanism exactly (grabs/
    reuses a `GenericFader` via `Universe::requestFader()`, sets the
    `FadeChannel`'s target). Verified via code audit that this is safe as a
    one-shot call rather than needing MasterTimer registration:
    `Universe::processFaders()` (`engine/src/universe.cpp:333`) writes every
    outstanding requested fader on its own, every tick, regardless of who
    last touched it — so the target persists without re-invoking this method
    every frame. Also routes the edit for Save-bookkeeping the same way
    VCSlider does (`Doc::routeProgrammerEdit()`, falling back to
    `Doc::setProgrammerValue()`). This was the one deliberately-deferred item
    from the P2 design write-up ("needs a ProgrammerController method audit
    to find the right entry point") — audited, then built.
  - **Selection substrate**: reuses `ProgrammerController::
    setProgrammerSelection()`/`programmerSelection()` directly (not
    `programmerSubSelection()`, which keeps its original narrower "deviate
    individual fixtures out of a group palette" purpose, untouched by PMJ
    for now). New `PMJOverlay::sceneTargetFixtures()` defines "canvas order"
    concretely: the scene shown in the Programming canvas
    (`ProgrammingManager::currentSceneId()`, new getter, same pattern as
    `currentPaletteId()`)'s fixture-group members (expanded, group order)
    then its individually-fixed fixtures, deduped. **Not yet paged past
    10** — a scene with more targets only exposes the first 10 for now
    (flagged, not silently pretended-away).
  - **Select(N)**: toggles that target fixture into/out of
    `programmerSelection()` — multi-select, confirmed with Branson.
    **Load(N)**: replaces the selection with just that one target, matching
    the Role's own doc comment ("load item N into the programmer").
  - **Faders 1-6, Selection mode**: when nothing's focused in the Look
    Editor (`faderInUse()` false — Look-edit mode still wins when it
    applies), fader N writes the intensity channel of the Nth selected
    fixture live via `writeChannelLive()`.
  - **LED highlighting**: Select/Load(N) lit (`Valid`) when strip N has a
    real target in the open scene, brighter (`Selected`) when that fixture
    is actually in the current selection — refreshes live via the existing
    `programmerSelectionChanged` signal (already built, previously unused by
    any UI).
  - **Deferred to a follow-up slice** (per the design doc's own list):
    faders 7-10 (RGBW of selection), Enc 3/4 (Focus/Zoom of selection),
    Reset(N) zeroing a selection-mode fader (currently Reset only covers
    Look-edit mode's `faderInUse()` faders), and the >10-target paging via
    the `Page` button.
  - Builds clean, smoke-tested. **Not yet verified against the real board —
    this is genuinely new, first-time-tested DMX-write plumbing, unlike P1's
    palette-refresh-based writes**, so worth an attentive first test rather
    than an "it built, ship it" pass.

- **Blind/Blackout footer indicators — SHIPPED (2026-08-18).** `ui/src/app.{h,cpp}`.
  Branson: "we don't need two bars for blind, the footer being blue is enuff" —
  removed the redundant `m_statusBlindLabel` text chip (Blind already turns
  the whole footer blue, unmistakable on its own); the toolbar button's
  checked state is untouched. Added a Blackout counterpart Branson asked for
  ideas on — chose the smaller of two options (a compact chip, not a
  whole-footer-red treatment like Blind's): new `m_statusBlackoutLabel`, a
  red "● BLACKOUT" chip shown/hidden in `slotBlackoutChanged()`. Builds clean.

- **P1 slice 8 — numbered fader labels + Up-button reset/highlight —
  SHIPPED (2026-08-18).** `engine/src/controlsurface.h`,
  `ui/src/pmjoverlay.{h,cpp}`, `ui/src/lookeditor.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`. Two asks from Branson after slice 7
  landed: (1) "it'd be nice to see what faders we're moving... number them
  for clarity," (2) "we can also use the fader up button to reset and
  highlight the faders in use."
  - **Numbered labels**: the Color page's R/G/B were previously *inside*
    `QColorDialog` (opaque, can't inject labels) — pulled them out into the
    same numbered-vertical-slider pattern the White/Amber/UV sliders already
    used, so all six read "1 R" "2 G" "3 B" "4 W" "5 A" "6 UV," matching
    `PMJOverlay`'s fader-index mapping exactly. Bidirectional sync with the
    dialog's own picker (`slotColorChanged`/new `slotRgbSliderChanged`, both
    `blockSignals`-guarded to avoid feedback loops); `commitColor()` now
    reads RGB from the numbered sliders (the canonical source) rather than
    `m_colorDialog->currentColor()`.
  - **Reset + highlight**: new `ControlSurface::RoleType::Reset` (index =
    strip N) in the P0 engine core — device-agnostic, so APC40/Xbox overlays
    can reuse it later. PMJ's 10 "N-Up" buttons (previously `CS::Role()`,
    completely unbound) now carry it. New shared `PMJOverlay::faderInUse(int)`
    is the single source of truth for "does fader N do anything right now,"
    used by all three of: the Level write path (refactored to call it instead
    of duplicating the Color/Dimmer type check inline), the new Reset write
    path (zeroes that channel via the same `setDesignColorChannel`/
    `setDesignDimmerValue` from slice 7, value=0 — no new engine write method
    needed), and `stateFor()`'s new `Reset` case (lights the Up button
    `State::Valid` exactly when `faderInUse()` is true).
  - **Live LED refresh on focus change** (needed for the highlight half to
    update the instant a different look is clicked, not just on the next
    unrelated interaction): new `LookEditor::lookFocusChanged(quint32)`
    signal, emitted at the end of every `setPalette()` call (both the
    empty-palette and normal paths) — re-emitted by `ProgrammingManager` as
    `currentPaletteIdChanged`. `PMJOverlay` couldn't connect to this at
    construction time (`ProgrammingManager` isn't built yet when
    `App::initDoc()` constructs `PMJOverlay` — same ordering constraint
    slice 4 hit) — solved with new `programmingManager()`, a `const` helper
    that lazily finds-and-connects-once on first use (`mutable` cache member),
    reused at all four call sites that previously each did their own
    `m_app->findChild<ProgrammingManager*>()`.
  - Builds clean, smoke-tested. **Not yet verified against the real board.**

- **PMJ hardware fix (not code) — OpenDeck LED channel misconfig, 8 buttons
  affected — WRITTEN TO THE BOARD (2026-08-18).** Root-caused via
  `qlcplus-midi-profiler`'s `opendeck dump`/direct SysEx reads (no
  interactive `identify` needed — the LED's own `activation_id` was already
  correct, only its `channel` field was wrong): `Set`, `6-Up`, `6-Load`,
  `Left`, plus 4 currently-unmapped notes all had their paired LED listening
  on MIDI channel 1 (raw) instead of channel 9, so QLC+'s feedback (always
  sent on ch9) never reached them — explains "Set toggles but never lights."
  Backed up first (`qlcplus-midi-profiler/backups/pre-led-channel-fix.json`,
  restorable via `qlc-midi opendeck restore`), then wrote the correct
  channel (raw 9) to all 8 LED slots directly via `OpenDeck.write_checked`,
  verified via read-back. **Confirm live**: Set/6-Up/6-Load should now
  light like `O`/Master already do.

- **P1 slice 7 — context-aware faders (Level 1-10) — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.cpp`. Per
  Branson: faders should mean whatever's relevant to the palette focused in
  the Look Editor, not a fixed submaster/per-fixture role — "if we're
  selecting a [color] feature we'd do sliders for R G B W A U." Reuses the
  same `ProgrammingManager::currentPaletteId()` ground-truth built for the
  pan/tilt encoder work (slice 4). New `ProgrammerController::
  setDesignColorChannel(paletteId, channelIndex, value)` (0=R..5=UV, writes
  via `QLCPalette::colorToString` matching `LookEditor::commitColor()`'s own
  write pattern) and `setDesignDimmerValue(paletteId, value)` — both
  absolute writes (faders aren't relative like the encoders), same
  explicit-paletteId/refresh-every-referencing-scene/`designPositionWritten()`
  contract as `nudgeDesignPanTilt()`. `PMJOverlay`'s `Level` case now checks
  the focused palette's type: Color → faders 1-6 = R/G/B/White/Amber/UV,
  Dimmer → fader 1 = intensity, anything else (PanTilt/Aim/nothing focused)
  → faders stay inert (confirmed with Branson: leave dark/idle when nothing
  applies, don't fall back to a permanent submaster baseline — an open
  question from slice 1 is now resolved this way). Live redraw confirmed
  free: `LookEditor::setPalette()`'s Color case uses real Qt widgets
  (`QColorDialog::setCurrentColor`, `QSlider::setValue`) which repaint
  themselves on state change — unlike the custom `VCXYPadArea` from slice 5,
  no extra `update()` call needed. Builds clean, smoke-tested. **Not yet
  verified against the real board.**

- **P1 slice 6 — encoder direction, confirmed working on the rig —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`. After slice 5 the pin
  finally moved, but both encoders' raw MIDI delta turned out to be
  physically inverted relative to their on-screen effect — confirmed live:
  turning Enc 1 (pan) clockwise moved the dot left, not right. First pass
  negated the wrong encoder (tilt) on a guess; Branson clarified "it's enc1
  thats backwards," so negated pan instead — fixed pan, but left tilt's
  sign inconsistent (undiscussed/untested but same hardware, so likely the
  same physical inversion). Branson called this out directly: negate both,
  so "clockwise increases" is the one consistent rule for both axes rather
  than an asymmetric fix. **Confirmed working on the real PMJ — pan/tilt
  encoder nudge is done.**

- **P1 slice 5 — the actual root cause: `VCXYPadArea::setPosition()` never
  repaints itself — SHIPPED (2026-08-18).** `ui/src/lookeditor.cpp`,
  `ui/src/pmjoverlay.cpp`. Slice 4 was real but not sufficient — Branson
  turned the encoders again after that build, still nothing ("STILL NOT
  MOVING THE PIN"). Added logging inside the full chain (PMJOverlay's
  `slotRoleActivated` Param case) and it proved every single precondition
  was green on every turn: `ProgrammingManager` found, a valid palette id,
  a real palette object, `type() == PanTilt` exactly, `Doc::programmer()`
  non-null — `nudgeDesignPanTilt()` was being called correctly every time.
  So the bug was never upstream at all; traced `nudgeDesignPanTilt()`'s own
  body (`engine/src/programmercontroller.cpp`) and the write side
  (`QLCPalette::setValue()`/`intValue1()`/`intValue2()`) — both correct.
  Root cause was purely on the redraw side: `VCXYPadArea::setPosition()`
  (`ui/src/virtualconsole/vcxypadarea.cpp`) only updates internal state and
  emits `positionChanged()` — it never calls `update()`/`repaint()` itself.
  Every other call site pairs it with an explicit `update()` right after
  (`mousePressEvent`/`mouseMoveEvent`, and the joystick-drag redraw path
  already in `LookEditor` at line ~429) — except `LookEditor::setPalette()`'s
  `PanTilt` case (line ~714), which called `setPosition()` alone. So the
  palette value and the widget's internal `m_dmxPos` really were updating
  correctly on every encoder click; the dot just never got painted. Added
  the missing `m_xyPad->update()` after `setPosition()` there. Also removed
  the now-resolved chain debug logging from `pmjoverlay.cpp`. Builds clean,
  smoke-tested (no crash). **Not yet re-verified against the real board.**

- **P1 slice 4 — nudgeDesignPanTilt() no longer depends on stale
  focused-scene tracking — SHIPPED (2026-08-18).**
  `engine/src/programmercontroller.{h,cpp}`, `ui/src/pmjoverlay.{h,cpp}`,
  `ui/src/programmingmanager.{h,cpp}`, `ui/src/app.cpp`. Slice 3's fixes
  (value-scaling, XY-pad redraw) turned out to be correct but incomplete —
  Branson dragged the XY pad to re-center, turned Enc 1/2 again, still no
  movement. Re-added debug logging (encoder-side decode now confirmed
  perfect: raw `255`/`2` → `-5°`/`+10°`, exactly as intended) — but the
  SECOND log line, inside `nudgeDesignPanTilt()` itself, never printed at
  all. It was returning at the very first check: `m_focusedSceneId` was
  invalid. Root cause: that tracking (`ProgrammerController::
  m_focusedSceneId`/`m_focusedPaletteId`) only gets set when a scene is
  freshly opened in the Programming tab's canvas — it doesn't persist
  across an app relaunch, and dragging the XY pad by hand never needed it
  in the first place (`LookEditor::slotPanTiltChanged()` only needs its own
  `m_paletteId`, tracked independently). So the encoder path had a real
  dependency the mouse path never had — not a fluke, a design gap.
  - **`nudgeDesignPanTilt()` re-signatured** to take an explicit
    `paletteId` instead of reading `m_focusedSceneId`/`m_focusedPaletteId`
    at all. Refreshes every scene that actually references the palette
    (`scene->palettes().contains(paletteId)`, walking `m_doc->functions()`)
    instead of one assumed "focused" scene — more correct anyway, since a
    palette can legitimately be shared by more than one scene.
  - **New `ProgrammingManager::currentPaletteId()`** — the ground truth for
    "what's on screen right now" (`m_lookEditor->paletteId()`, itself made
    reachable via a new `LookEditor::paletteId()` getter in slice 3).
    `PMJOverlay` now takes an `App*` at construction (`app->findChild<
    ProgrammingManager*>()`, the same reach-pattern already established
    elsewhere in this codebase) to get this instead of going through
    `ProgrammerController`'s tracking.
  - **Found and fixed a real, unrelated header bug along the way**:
    `programmingmanager.h` uses `QDoubleSpinBox*` but never forward-declares
    it (`QSpinBox` is declared, `QDoubleSpinBox` isn't) — worked by
    accident everywhere it was previously included transitively-first;
    broke the moment `pmjoverlay.cpp` included it directly. Fixed the
    header itself (added the missing forward declaration) rather than
    working around it locally — a real latent bug, not a workaround target.
  - Builds clean, smoke-tested (board disconnected, no crash). **Not yet
    re-verified against the real board** — fourth round on this exact
    feature; this one at least has hard evidence (the debug log) behind the
    diagnosis rather than another guess, but still needs a real turn of
    the encoder to confirm.

- **P1 slice 3 — two real bugs found via debug logging, both fixed —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `ui/src/lookeditor.{h,cpp}`, `ui/src/programmingmanager.cpp`. Branson's
  slice-2 test ("still no movement") got temporary file-based debug logging
  added to `nudgeDesignPanTilt()` and `PMJOverlay::slotInputValueChanged()`
  (matching this session's established fallback when guessing plateaus) —
  the resulting log conclusively showed two real, independent bugs rather
  than a workflow mistake:
  1. **Value-scaling bug.** The encoder's raw MIDI twos-complement delta
     (1 = +1, 127 = -1, confirmed weeks earlier via `qlc-midi monitor`) is
     NOT what `inputValueChanged()` actually delivers — QLC+'s MIDI plugin
     already scales it into its internal 0-255 space first (MIDI2DMX,
     ~x<<1, 127→255 special-cased). The log showed raw values `2` and `255`
     arriving, not `1`/`127` — my decode threshold (64/128, correct for raw
     7-bit MIDI) was silently turning a -1 click into a +127 click, which
     instantly clamped pan/tilt to its range boundary on the very first
     turn and then correctly did nothing on every subsequent one (already
     at the clamp) — indistinguishable from "not working" without the log.
     Fixed: threshold is 128/256, matching the space the value is actually
     in by the time it reaches this code.
  2. **The XY pad widget never repaints.** The SAME log also proved
     `nudgeDesignPanTilt()` WAS finding a focused scene (id 0 — a real,
     valid id; only `4294967295` means unset) and, once the palette was
     added to the scene, WAS finding and would have modified it — so the
     underlying engine data was correct the whole time. What was missing:
     nothing told `LookEditor`'s on-screen XY pad to redraw after an
     engine-side value change — only the user dragging it by hand ever
     triggered that before, since `applyDesignJoystick()` (the only prior
     caller of the `designPositionWritten()` signal this reuses) never
     touches a PanTilt-type palette's displayed values at all (it only
     drives an Aim-target, a different widget). Fixed with a new
     `LookEditor::paletteId()` getter and a redraw
     (`m_lookEditor->setPalette(m_lookEditor->paletteId())`) added to
     `ProgrammingManager::slotDesignPositionWritten()` — harmless/no-op
     for the Aim-look case, the only thing that repaints a PanTilt palette
     after a nudge.
  - All temporary debug logging removed after diagnosis, per this
    session's established pattern. Builds clean, smoke-tested (board
    disconnected, no crash). **Not yet re-verified against the real
    board/encoders** — this is the third round of "build, hand back for a
    real test" on this exact feature; worth a clean pass before assuming
    it's actually done.

- **P1 slice 2 — LEDs only light for what's real; Enc 1/2 nudge pan/tilt —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.cpp`,
  `engine/src/programmercontroller.{h,cpp}`. Follow-on from Branson's first
  real hardware test of slice 1: `Set` correctly toggled Blind (input
  pipeline genuinely works), but Blackout/Blind LEDs didn't light at all,
  and `1-10`/`1-10 Load`/`Go`/`Back` all lit for no reason — confusing,
  and backwards from "highlight what's useful, leave the rest dark."
  - **LED semantics fixed**: `stateFor()` now returns `Empty` (dark) for
    every role that doesn't drive real behavior yet (Page/Select/Load/
    Param/Transport) instead of defaulting them to `Valid` (dim). Only
    Blackout/Blind (dim idle, bright when engaged) light at all now — an
    honest reflection of what this slice actually does.
  - **Real finding, not covered by the LED fix**: Branson tried creating a
    pan/tilt look and turning the encoders — nothing moved. Turned out
    `ProgrammerController::applyDesignJoystick()` (the existing, working
    HID-joystick pan/tilt path) explicitly no-ops for a "Standard Pan/Tilt"
    look — raw palette-value authoring for that case doesn't exist ANYWHERE
    in the app yet, not just missing for the PMJ; only an Aim-palette look
    (which drags a floor-space target) has ever had a working live-nudge
    path. Confirmed with Branson before building rather than guessing at
    Look-authoring semantics unilaterally.
  - **New engine method**: `ProgrammerController::nudgeDesignPanTilt(float
    dPanDeg, float dTiltDeg)` — finds the focused scene's controlling
    PanTilt palette (same precedence `applyDesignJoystick()` uses: the
    explicitly focused palette, else the last PanTilt-type one on the
    scene), adjusts its stored `intValue1()`/`intValue2()` (raw degrees,
    same 540°/270° space `LookEditor`'s XY pad already authors it in — see
    `lookeditor.cpp`'s `PAN_DEG`/`TILT_DEG`), and refreshes the running
    scene via `markSceneEdited()` — the exact same live-refresh path a Look
    Editor slider drag already uses (`Scene::requestPaletteRefresh()`, not
    a full `resetRuntime()` teardown, so repeated nudges don't restart the
    scene's fade-in or flash other channels). No-op for an Aim-look scene
    (nothing to nudge — the joystick's existing path already owns that
    case) or when nothing's focused.
  - **Wired**: `Enc 1` → pan, `Enc 2` → tilt, 2°/click, unconditional (no
    paging yet — Enc 3/4 stay unbound, pending the same page-targeting
    model as the rest of this slice). Had to hand-decode the encoder's raw
    twos-complement value myself (`value<64 → +value, else value-128`) —
    confirmed QLC+ doesn't do this upstream of `inputValueChanged` even for
    an `Encoder`-typed channel (only `QLCInputSource::decodeRelativeDelta()`,
    a *bound-widget* config object, does — not applicable here), matching
    the same raw values `qlc-midi monitor` showed during the original
    encoder classification work.
  - Builds clean, smoke-tested (board disconnected, no crash/regression).
    **Not yet re-verified against the real board** — worth checking: LEDs
    stay dark except Blackout/Blind now, and turning Enc 1/2 with a
    PanTilt-palette look focused actually moves pan/tilt live.

- **P1 slice 1 — PMJ Black 1 overlay onto the control-surface engine —
  SHIPPED (2026-08-18).** `ui/src/pmjoverlay.{h,cpp}` (new), `ui/src/app.{h,cpp}`,
  `ui/src/CMakeLists.txt`. First real device overlay on top of the P0 engine
  from an earlier session (`ControlSurfaceEngine`/`ControlSurface` — device-
  agnostic role/state vocabulary + a generic LED-repaint loop, see
  `CONTROL_SURFACE_DESIGN.md`). Deliberately scoped down from the full
  design-doc Phase 1 vision to what has clean, unambiguous integration
  points today — flagging the scope honestly rather than half-guessing the
  rest:
  - **Full role table registered** — every one of the PMJ's 69 real MIDI
    channels (hardcoded from `resources/inputprofiles/PMJ-Black-1.qxi`, not
    re-parsed at runtime, so a hand-edited profile can't silently desync the
    binding) gets a `ControlSurface::Control` + `Role`, matching the design
    doc's table: `Master`→GM, `Ch 1-10`→per-strip `Level`, `1-10`→`Select`,
    `N-Load`→`Load`, `Enc 1-4`→`Param`, `Groups/Looks/Effects/Macros/Fix
    Cont`→`Page`, `Go/Back/Left/Right/Pre Page/Next Page`→`Transport`,
    `O`/`Set`→`Blackout`/`Blind` (the design doc's proposal, confirmed).
    `N-Up`/`N-Down` and `Favorites` (proposed Tap) are registered with no
    Role — still-open per the design doc's discussion point and the "the
    only Tap in the codebase is Programming-tab-local, not global" finding.
  - **Only 3 are wired to real behaviour so far**: Master fader →
    `InputOutputMap::setGrandMasterValue()`, `O` → `toggleBlackout()`, `Set`
    → `setOutputInhibited()` — all engine-level Doc APIs, no App-level
    plumbing needed. Select/Load/Param/Transport/Page-switching are received
    by the engine and logged, not yet driving real selection/navigation —
    that needs the UI-side "what's currently selected/active" concept this
    session established doesn't cleanly exist yet for fixture groups in the
    Programming tab. Next slice's real dependency, not a small gap.
  - **LED feedback**: `ledSink` sends real Note-On via the existing generic
    `InputOutputMap::sendFeedBack()` (channel 9, matching the profile) —
    reused, not reinvented. Brightness snaps to the OpenDeck "steady levels"
    set (15/31/47/…/127) found via `qlcplus-midi-profiler`'s README, so a
    state change can never accidentally set a control blinking. `stateFor()`
    reflects real state for Blackout/Blind (dim when idle, bright when
    engaged) and the active page (selected vs. valid); Select/Load default
    to a flat "present" dim state pending the same selection-model gap above.
  - **Registered on every startup**, PMJ connected or not — `App::initDoc()`
    constructs the engine + overlay unconditionally, right after
    `startUniverses()`. No crash/error either way; smoke-tested via a full
    launch + screenshot with the board disconnected.
  - Building this slice surfaced one real bug in my own code, not the
    engine: `using CS = ControlSurface;` is invalid C++ (type-alias syntax
    can't name a namespace) — direct compile confirmed the fix
    (`namespace CS = ControlSurface;`).
  - **Partially live-verified, one real bug found and fixed.** Branson
    reconnected the board: `Set` correctly toggled Blind (confirms the input
    pipeline works end-to-end for real), but no LEDs lit at all, when at
    minimum every registered/`Valid` control should have shown a dim glow.
    Root cause: `sendLed()` hardcoded `sendFeedBack(0, ...)` — assumed the
    PMJ's output would be on universe 0, with no actual basis for that
    guess. If that's wrong, feedback silently goes nowhere. Fixed two ways:
    (1) `PMJOverlay` now scans every universe's `InputPatch::profileName()`
    for one matching "PMJ Black 1" at construction time
    (`findKnownUniverse()`), so LEDs light on connect/launch without
    requiring a press first; (2) `slotInputValueChanged()` also re-learns
    the universe from real traffic on every event (not just once), so a
    board patched *after* startup, or re-patched to a different universe
    mid-session, self-corrects instead of staying dark. Builds clean,
    smoke-tested with the board disconnected (no crash/regression) — the
    universe-discovery fix itself still needs a real check with the board
    connected, since that's exactly the scenario it fixes.

- **Three more backstage color themes — SHIPPED (2026-08-18).**
  `ui/src/app.h`, `ui/src/app.cpp`. Turned out this fork already had a whole
  theme system (`App::Theme` enum, `applyTheme()` building a `QPalette`,
  View → Theme menu, persisted to `QSettings`) from an earlier session —
  Default/Tan/Blue. Branson asked for a few more, name-checking a "QLC+
  original" look and a red-shifted night-vision theme, and to look at VS
  Code's dark themes for ideas. Scoped to chrome only (menus/toolbars/tabs/
  dialogs) per Branson's choice — the 2D canvas (trusses/fixtures/grid) is
  hand-painted with ~200 hardcoded `QColor` literals across 18 files and
  doesn't follow the palette; making it theme-aware too is a much bigger
  follow-on project if ever wanted. Added: **QLC+ Original** — corrected
  after Branson called out that my first pass (a dark charcoal grey) was
  wrong: he pulled a fresh copy of upstream QLC+ (`github.com/mcallegari/
  qlcplus`, now checked out at `/Users/branson/git/qlcplus`) to check
  against, which confirmed upstream ships NO custom palette or stylesheet
  at all — it's plain native Qt Fusion light grey. Redone to match that
  (`#efefef` window, white base, black text, `#4a90d9` highlight) — unlike
  "Default" (which just follows whatever the OS's current light/dark
  setting is), this stays that same classic light look on demand regardless
  of OS mode; **Red Shift** (blue channel kept
  near-zero throughout, the same principle as a red stage torch or
  astronomy light, for working backstage in the dark without wrecking night
  vision); **VS Code Dark** (VS Code's "Dark+" editor greys plus its iconic
  `#007acc` accent blue). Same mechanism as the existing two themes — just
  new `QPalette` color blocks in `applyTheme()`'s switch and three new
  entries in the View → Theme menu's data-driven choice list. **Verified
  live**, carefully: with two `qlcconsole` processes running (Branson's real
  session plus my own isolated scratch-file test), AppleScript's `process
  whose unix id is N` turned out to silently match the WRONG process by
  name regardless of the id filter — an accidental click opened Branson's
  real View → Theme menu once (no theme was actually changed; the
  submenu-item click failed before selecting anything, and Escape closed
  it). Switched to a safer verification method with zero click risk:
  pre-set `workspace.theme` via `defaults write org.qlcplus.qlcconsole`
  before launching the scratch instance fresh (it reads the setting once at
  startup), screenshotted Red Shift applied correctly (warm orange-red
  layer-row highlight and tab underlines), then restored the setting to
  Branson's original value (`Blue`) afterward.

- **Stage centre lines now have their own show/hide toggle — SHIPPED
  (2026-08-18).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `ui/src/monitor/monitor.cpp`. The teal crosshair marking the stage centre
  was previously bundled into the same `m_gridItems` list as the grid lines
  themselves, so there was no way to hide it independent of the grid. Gave
  it dedicated `m_centerLineV`/`m_centerLineH` members (rebuilt alongside
  the grid in `updateGrid()`, since their position depends on the same
  `m_cellPixels`/offsets, but no longer added to `m_gridItems`) and a new
  `setCenterLinesVisible()`/`centerLinesVisible()` pair. New "Center" footer
  toggle button next to Rulers/Labels, persisted via `QSettings`
  (`monitor/centerlines`, default on) the same way Rulers/Grid already are.
  Built clean, not yet verified live.

- **Truss tether line: no-line threshold now matches the truss's visual
  width — SHIPPED (2026-08-18).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `updateTrussAnchorLines()`. Branson reported still seeing the line even
  when a fixture looked like it was sitting right on top of the truss. The
  no-line threshold was a fixed 4px around the exact mathematical
  centreline — too tight, since the truss itself is drawn with real width,
  so anywhere within that drawn thickness reads as "on the truss" to the
  eye without being at cross=0 exactly. Threshold is now `max(4px, half the
  truss's own drawn width)`, so the line stays hidden across the whole
  visual footprint of the truss bar, not just its mathematical centre.
  Built clean, not yet verified live.

- **Truss tether line now terminates at the truss's near EDGE, not its
  centreline — SHIPPED (2026-08-18).**
  `ui/src/monitor/monitorgraphicsview.cpp`, `updateTrussAnchorLines()`.
  Follow-on to the entry above: the anchor end of the line was always
  `Truss::positionAt(trussOffset)` — dead centre of the truss's width —
  so the line visually ran INTO the truss body before disappearing under
  it, rather than stopping where the fixture actually meets the truss.
  Reworked the skip-check and the anchor point separately: first decide
  whether to draw at all by comparing `|trussCross|` against the truss's
  half-width (plus ~2px of slack) in world units — this is the same
  "is the fixture still within the truss's visual footprint" test as the
  entry above, just computed before picking an edge, so it isn't skewed by
  which side the line would terminate on. Then, only once drawing, offset
  the anchor point from the centreline by exactly the truss's half-width,
  signed toward whichever side the fixture's `trussCross` is on — so the
  line always starts right at the truss's surface on the fixture's side,
  never inside it. **Verified live** on the isolated scratch copy: cropped
  in on the fixture/truss boundary and confirmed the dashed line now
  stops right at the truss edge instead of running through it.

- **Real bug, finally cornered: Lighting Studio can come up completely empty
  on a fresh launch — trusses, layers, and every fixture missing — even
  though the save was perfectly intact — FIXED (2026-08-18).**
  `ui/src/app.cpp`, `App::loadXML(const QString&)`. This is the bug behind
  Branson's "add a fixture, save, close, reopen — fixture not there at all"
  report, and it very nearly got written off as our earlier file-collision
  false alarm — glad we kept pushing. Root cause, found by reproducing on an
  isolated scratch copy (never touching Branson's real file) with temporary
  file-based tracing at three levels: (1) `MonitorProperties::loadXML()`
  parses every single element correctly — Layer 1, both trusses, all three
  `FixtureRig` entries, confirmed via a log of every XML element it visits;
  (2) yet `Monitor::fillGraphicsView()` — the function that actually
  populates the 2D canvas from that loaded data — ran with `docFixtureCount
  = 0`, i.e. `Doc` had ZERO patched fixtures at the moment it ran; (3) but
  the Fixture Manager (Hardware tab), opened moments later in the SAME
  running instance, correctly listed all three fixtures. So the doc data was
  never actually lost — `Monitor`'s graphics view had just been built once,
  too early, from an empty doc, and never told to rebuild. The mechanism:
  `main.cpp` calls `app.startup()` (constructs all tabs, including
  whichever one is the workspace's saved `CurrentWindow` — shown
  immediately, against whatever `Doc` holds at that instant) BEFORE calling
  `app.loadXML(QLCArgs::workspace)` for a `-o`/`--open` command-line file.
  Fixture Manager's tab tree happens to rebuild itself constantly from many
  UI interactions, so it self-heals; `Monitor::fillGraphicsView()` has
  almost no other trigger and stayed stale. It only ever surfaced visibly
  once a save carried `CurrentWindow="Monitor"` — i.e. once Lighting Studio
  became the tab a workspace reopens directly into, which is exactly the
  workflow this whole session has been exercising. The "open recent file"
  path already knew to call `Monitor::instance()->updateView()` +
  `FixtureManager::instance()->updateView()` after loading (see the existing
  code a few lines above in the same file) — the command-line/`-o` load
  path just never got the same treatment. Fixed by adding those same two
  calls at the end of `App::loadXML(const QString&)`, the shared function
  underneath both paths, so every caller benefits and the "recent file" path
  just does one harmless extra refresh. **Verified live**: reproduced the
  exact empty-canvas symptom on an isolated scratch copy of Branson's file,
  confirmed the fix resolves it (Layer 1, both trusses, all three fixtures,
  cyan rings, and the tether line all render correctly on a fresh launch),
  screenshot-confirmed both before and after. All temporary debug logging
  removed.

- **A truss-bound fixture that isn't sitting right on the truss now gets a
  thin tether line back to its anchor point — SHIPPED (2026-08-17).**
  `ui/src/monitor/monitorgraphicsview.{h,cpp}`, new `updateTrussAnchorLines()`
  + `m_trussAnchorLines`. Direct follow-on to the cross-offset drag fix:
  Branson asked whether an off-truss fixture should get a visual connector
  back to the truss so the binding reads clearly instead of relying on
  proximity, and whether anything else was worth doing. Implemented a
  dashed line from the fixture's current center to its projected point on
  the truss centerline (`Truss::positionAt(trussOffset)`, ignoring cross —
  i.e. the point it would sit at with no offset), skipped when the fixture
  is within 4px of that point so a normally-centered rig stays clutter-free.
  Color was a follow-up design question Branson raised: rather than a fixed
  color, the tether now matches the truss's OWN palette — its neutral
  unselected chord grey (160,163,172) at rest, switching to the truss's own
  selected amber (255,180,0) when the truss (and therefore its extended
  group of bound fixtures, per the anchor-selection fix above) is the
  current selection. This deliberately leaves the fixture's own existing
  selection color (yellow) and bound-to-truss ring (cyan) untouched — those
  are separate, already-established indicators; only the tether reflects the
  truss's selection state. Rebuilt on every truss/fixture move
  (`slotFixtureMoved()`, `slotTrussMoved()`, including its elevation-drag
  branch), on any general item-state refresh (`refreshItemLayerState()`,
  which covers load, POV switches, and lock toggles), and on selection
  change (`extendSelectionToGroups()`) so the color follows selection
  immediately without requiring a move. Vertical trusses (towers) are
  skipped — same reasoning as the cross-offset fix, no "across" concept
  there. **Verification note:** while testing this live, launching
  `surfacetesting.qxw.before-padfix` myself collided with Branson's own live
  session against the same file — each save raced the other's, which briefly
  looked like a real persistence bug (a fixture attach not surviving a
  restart) before we tracked it down: Branson closed his instance, reopened
  cleanly with me not touching the file, and confirmed the attachment DID
  survive a real restart — so that was purely our concurrent access, not a
  product bug. Lesson: don't launch the app against a workspace file the
  user might have open live. Branson then confirmed live: the tether line
  renders correctly and turns amber when the truss is selected — but the
  FIXTURE's own outline stayed yellow instead of following along, which he
  expected to change too. Fixed: `MonitorFixtureItem` gained
  `setTrussGroupSelected()`/`isTrussGroupSelected()` (new bool, distinct
  from the existing `m_isolated`/`isSelected()` selection-color logic in
  `paint()`) — when set, a selected fixture's outline draws in the truss's
  amber instead of the generic yellow. `updateTrussAnchorLines()` now sets
  this flag for every truss-bound fixture on every call (not just the ones
  that get a drawn tether line — a fixture sitting exactly on the centerline
  still needs its outline to follow the truss's selection even though it
  has no line to color). Deliberately narrow: only a fixture whose OWN truss
  is currently selected gets amber — solo-selecting just that fixture (the
  anchor-selection click fix from earlier) still shows plain yellow, since
  that's "just this fixture," not "the assembly." Built clean; not yet
  re-confirmed live (Branson needs to relaunch to pick up the new binary).
  **Follow-up (2026-08-17, same day):** Branson caught a real bug from a
  screenshot — the tether into a vertically-running truss was visibly a
  couple degrees off perpendicular (fine for a horizontal-running truss,
  off for a vertical one). Cause: the line's fixture-end read the icon's
  actual on-screen center while the truss-end was independently recomputed
  from stored `trussOffset` — any drift between a fixture's rendered
  position and its stored offset/cross (rounding, or hand-set data that was
  never perfectly self-consistent) tilted the line. Fixed by deriving BOTH
  endpoints from the same stored `trussOffset`/`trussCross` via the truss's
  own direction vector, so the line is perpendicular by construction
  regardless of the truss's on-screen orientation — no longer reads the
  item's rendered position at all. Also thickened per request (1.2–1.6px →
  2.4–3.0px) with a round cap. Built clean, not yet re-verified live.

- **Clicking a fixture right after clicking its truss now solo-selects the
  fixture instead of keeping the truss along for the drag — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `mousePressEvent()`. Last piece of the anchor-selection fix above: with
  that fix alone, Branson found a single-click on a lone fixture correctly
  moved just the fixture, and selecting the truss correctly moved
  truss+fixtures together — but click the truss FIRST, then click one of
  its fixtures, and both still moved together. Root cause is a Qt default,
  not a leftover bug in our logic: `QGraphicsScene`'s built-in press
  handling only clears the rest of the selection when the clicked item
  *isn't already selected* — if it is, it assumes you're grabbing the whole
  group to drag it. Since `extendSelectionToGroups()` had already pulled the
  fixture into the selection when the truss was clicked, the fixture counted
  as "already selected" by the time it was clicked next, so Qt left the
  truss selected too. Branson was offered two ways to resolve the ambiguity
  (highlight the fixtures to make the shared-selection state visible, or
  make a fixture click always break out solo) and picked solo-select, to
  keep a fixture click meaning "just this fixture" everywhere, consistent
  with the fix above. Implemented by intercepting a plain (no Shift/Ctrl)
  left-click on a `MonitorFixtureItem`: if it's selected AND its bound
  truss's `TrussItem` is also currently selected, force
  `m_scene->clearSelection()` + reselect solely the fixture *before* handing
  off to `QGraphicsView::mousePressEvent()` — so Qt's default handling then
  sees a solo, already-correct selection and just starts the single-fixture
  drag. Shift/Ctrl-click (explicit multi-select) and clicking the truss
  itself are untouched. **Not verified live** — same canvas-click/drag
  limitation as the other entries here; worth confirming at the rig:
  select truss (everything highlights) → click one of its fixtures → only
  that fixture should stay selected and move solo.

- **A truss-bound fixture can be dropped anywhere across the truss (not
  forced onto the centreline) while still touching it — SHIPPED
  (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`, `snapToTruss()`
  (inside `slotFixtureMoved()`) + `slotTrussMoved()`. Follow-on to the
  anchor-selection fix right below: once Branson could drag a bound fixture
  without it dragging the truss, the next ask was that dragging it
  perpendicular to a horizontal truss ("vertically away" on screen) always
  snapped it back onto the centreline, when it should be free to land
  anywhere still touching — with the existing red-border "pull too far and
  it detaches" behavior (already working, driven by `MonitorFixtureItem`'s
  `escapeMode()`/`setEscapeMode()`, set live during the drag in
  `mouseMoveEvent()`) as the actual boundary. `snapToTruss()` used to force
  the perpendicular component to zero by projecting the drop position onto
  the truss's direction vector and discarding everything else; it now splits
  the drop into an ALONG component (still projected/grid-snapped, as before)
  and a CROSS component (perpendicular offset, preserved), clamping cross to
  the same `pxWid() * 2` threshold the escape-mode red border already uses —
  so the allowed range matches exactly where it would otherwise go red, no
  surprise snap-back at the boundary. The cross value is stored in
  `FixtureRigProps::trussCross`, a field that already existed for the
  Fixture Properties dialog's discrete Left/Centered/Right "Across truss"
  selector (`ui/src/monitor/monitor.cpp`) and was already read by
  `MonitorProperties::fixtureRigPosition()` — the authoritative derived
  position `aimsolver.cpp`/`effectinstance.cpp` use for actual pan/tilt
  aiming and effects — so this reuses existing, already-correct engine math
  rather than inventing a parallel one; the top-view canvas (which renders a
  truss-bound fixture from its raw stored XY, not from `fixtureRigPosition()`
  — see `updateFixture()`) is kept in sync by writing the same along+cross
  point into both places. `slotTrussMoved()`'s "truss carries its fixtures"
  logic was also switched from the bare `Truss::positionAt(trussOffset)`
  (centreline only) to `MonitorProperties::fixtureRigPosition()`, so a
  fixture's cross offset (and its mount-side Z nudge) now survives the truss
  itself being moved — previously any fixture with a non-zero cross offset
  would have silently re-centred the next time its truss moved, since
  `positionAt()` never included that term. Vertical trusses (towers) are
  unchanged — the user's ask was specifically about horizontal trusses, and
  a tower's radial/yoke mounting doesn't have the same "across" concept.
  **Not verified live** — same canvas-drag limitation as the other entries
  here; worth a real drag test (drop off-centre but touching → stays there;
  drop across → snaps within bounds; drop too far → red + detach; move the
  truss afterward → fixture keeps its offset) at the rig.

- **Moving a truss-bound fixture no longer drags the truss along with it —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.{h,cpp}`,
  `extendSelectionToGroups()` + new `isGroupAnchorItem()`. Third bug in this
  same area, found immediately after the drag-to-attach fix landed: once a
  fixture is bound to a truss, clicking/dragging *the fixture* also moved
  the whole truss. Root cause was `extendSelectionToGroups()` (wired to
  `QGraphicsScene::selectionChanged`) — it treats every member of a group as
  a peer, so selecting any one member (the fixture) pulled in the whole
  group's `topLevelGroup()`, truss included, and Qt's native multi-select
  drag then moved everything together. But a truss's auto-maintained group
  (`ensureTrussGroup()`/`ensurePlatformGroup()`, `anchorKind == "truss"` /
  `"platform"`) isn't a peer relationship — it's parent→child: the truss
  already carries its bound fixtures correctly when *it* moves, via
  `slotTrussMoved()`'s dedicated position-following logic, which runs
  independent of selection state and needed no changes. Fix: only extend a
  *dedicated* structural group (anchor set) when the anchor item itself —
  the truss/platform, not a rigged member — is what got selected; a manually
  built-out group (anchor cleared once it holds >1 structural item, see
  `structuralMembersOf()`) keeps the old symmetric behavior. New helper
  `isGroupAnchorItem(QGraphicsItem*, anchorKind, anchorId)` takes the anchor
  kind/id rather than the `MonitorProperties::MonitorGroup` struct directly
  — `monitorproperties.h` is only forward-declared in the header, so a
  nested-type parameter there fails to compile (confirmed by trying it
  first). **Not verified live** — same canvas-drag limitation as the entry
  below; needs a real "drag the fixture, does the truss stay put" /
  "drag the truss, do its fixtures follow" check at the rig. Also
  unaddressed: Branson's "not so far they don't touch" phrasing implies a
  bound fixture dragged far enough away should auto-detach — there's an
  existing "escape mode" detach branch in `slotFixtureMoved()` built for
  exactly this, but nothing in the codebase ever calls `setEscapeMode(true)`
  to trigger it, so it's dead code today. Left alone pending confirmation
  this is actually wanted, since it wasn't explicitly asked for as a
  separate feature.

- **Drag-to-attach a fixture onto a truss brought back, scoped correctly
  this time — SHIPPED (2026-08-17).** `ui/src/monitor/monitorgraphicsview.cpp`,
  `slotFixtureMoved()`. Direct continuation of the truss-group bug above:
  once that fix landed, Branson immediately hit the OTHER side of the same
  area — dragging an unbound fixture onto a truss did nothing at all. That
  turned out to be pre-existing, deliberate: a prior fix had removed
  auto-attach-on-drop *entirely* to kill a bug where a truss would "grab"
  any fixture whose bounding box merely overlapped it. That's also exactly
  what Branson had asked for earlier in this session (attach should trigger
  on *touching*, not require being centered on the truss) — so the right
  fix was to bring it back with tighter scope, not leave it removed.
  Re-added using `QGraphicsItem::collidesWithItem()` (precise shape overlap,
  not just a bounding-box guess) between the fixture and every `TrussItem`,
  but critically **only inside `slotFixtureMoved()`, which only runs once
  per completed drag (on drop)** — never per mouse-move — so passing a
  fixture over a truss en route somewhere else can't trigger it the way the
  original bug did. Mirrors the already-working, already-accepted "auto
  deck-mount onto a platform" pattern right below it in the same function,
  just for trusses. The explicit right-click "Attach to Truss…" and
  Layers-tree-drag paths are untouched and still work as a fallback for
  precise placement. **Not verified live** — this environment has no way to
  synthesize a real click-drag-release sequence on a `QGraphicsView` canvas
  (no `cliclick` or equivalent installed), so this shipped on code review
  plus direct parity with the platform auto-mount logic it's modeled on,
  not a screenshot. Worth a real drag-and-drop check at the rig before
  trusting it.

- **Build Focus removed; "clicking a fixture selects the truss too" —
  correctly root-caused this time (a real data bug, not Build Focus) and
  fixed — SHIPPED (2026-08-17).** Follow-on to the entry below, which
  turned out to have the WRONG diagnosis for a related-looking but distinct
  symptom Branson hit next.
  - **Build Focus removed entirely**
    (`ui/src/monitor/{monitor,monitorgraphicsview,monitorfixtureitem}.{h,cpp}`),
    per Branson's challenge: it duplicated what a locked Layer already does
    (both clear `ItemIsSelectable`/`ItemIsMovable`, which is what actually
    causes the click-through). `MonitorGraphicsView::setBuildFocus()`/
    `buildFocus()`/`m_buildFocus`, `Monitor::m_buildAction`, the footer
    "Focus:" combo, and the `updateModeIndicator()` BUILD branch are all
    gone. The one part of Build Focus worth keeping — Branson explicitly
    asked for it — was the faint "ghosted" visual as a general indicator of
    "what's currently clickable." `MonitorFixtureItem::setGhosted()` is kept
    but now driven by the fixture's REAL state
    (`refreshItemLayerState()`: `setGhosted(lyr.locked)`) instead of a
    separate mode, so a locked fixture is visibly faint everywhere, all the
    time, not just in one special mode.
  - **The actual "truss steals the selection" bug**, found only after two
    wrong turns (z-order/click-target-size, then Build Focus) — this
    session's Show Manager investigation habit of demanding hard evidence
    over plausible-sounding theories paid off: Branson's screenshots proved
    single-click selection genuinely pulled in the truss, and a live
    reproduction (`surfacetesting.qxw.before-padfix`, temporary debug
    logging in `MonitorProperties::setFixtureGroup()` and
    `extendSelectionToGroups()`) traced it to real saved data — 3 fresh,
    never-truss-bound fixtures (`FixtureRig Truss="4294967295"`, i.e.
    `Truss::invalidId()`) carrying the SAME `GroupId` as an existing truss,
    ~0.58 m away with no position link at all. Root cause:
    `detachFixtureFromTruss()` and the drag-escape auto-detach path in
    `slotFixtureMoved()` (`monitorgraphicsview.cpp`) both clear a fixture's
    `trussId` on unbind but — per an explicit, deliberate old comment,
    *"detaching is about the rig binding, not the spatial grouping"* —
    always left it in the truss's auto-created group. Once a fixture had
    ever been bound-then-unbound from ANY truss, it would select/move
    together with that truss forever after, with zero visual or positional
    relationship. Fixed with a new symmetric helper,
    `MonitorGraphicsView::leaveDedicatedTrussGroup(fid, trussId)`: on
    detach, if the fixture's current group is still the truss's own
    dedicated auto-group (`MonitorGroup.anchorKind == "truss"` and
    `anchorId == trussId` — i.e. nothing the user built out further
    manually), take it back out. Called from both detach paths. Also
    hand-fixed the ALREADY-corrupted stale data in
    `surfacetesting.qxw.before-padfix` (stripped the stray `GroupId="1"`
    from the 3 affected `FxItem` entries) — the code fix only prevents this
    going forward, it doesn't retroactively repair a workspace already
    carrying the orphaned membership.
  - Still open, deferred at Branson's request: a general "Add {thing} to
    {what's under the cursor}, or just 'Add {thing} here' with no attach if
    right-clicking empty space / a locked item" convention across every
    attachable type (truss/platform/pipe/stand/tower) — worth doing, but a
    separate pass from this bug fix.
  - Verified: full engine+UI build clean. NOT re-verified live in-app after
    the final fix (this session's synthetic-click/menu-popup limitations
    made reliably reproducing the exact add-fixture-and-click sequence
    impractical) — confirmed instead via the saved-XML evidence trail above
    plus direct code review of both detach call sites. Worth a real
    at-the-rig check: detach a truss-bound fixture, then single-click it —
    the truss should no longer come along.

- **"Clicking a fixture on a truss selects the truss" — root-caused and
  fixed the real problem (a discoverability gap, not a hit-test bug) —
  SHIPPED (2026-08-17).** `ui/src/monitor/monitor.{h,cpp}`. Branson's report
  led down a wrong first path (I initially suspected z-order/click-target
  size in `trussitem.cpp`/`monitorfixtureitem.cpp` — both checked out fine:
  fixtures are explicitly `zValue(2)` above trusses' `zValue(-0.5)`, and
  `MonitorFixtureItem::shape()` tightly hugs the fixture body). Branson's
  follow-up ("I can click anywhere in the blue box, every time") ruled that
  out and pointed at the real mechanism: **Build focus**
  (`MonitorGraphicsView::setBuildFocus()`), a pre-existing checkable mode
  that intentionally ghosts fixtures (`setGhosted(true)`, `ItemIsSelectable`
  cleared, `setMovable(false)`) so structural items become the click target
  for laying out trusses/platforms — a Qt item with neither flag set
  ignores its own mouse-press, which falls through to whatever's
  underneath. Working exactly as designed, but **undiscoverable**: the
  toggle lived only inside the "More" popup menu, and its own intended
  on-screen indicator — `m_modeLabel`, a "prominent current-mode chip" —
  was declared in the header and fully wired up in
  `updateModeIndicator()`, but **never actually constructed or added to any
  layout**, so it silently did nothing. Fixed by making the toggle itself
  visible: initially tried a dedicated toolbar button next to "Edit Plot"
  plus a full-width colored banner above the canvas (`m_modeLabel`,
  finally instantiated) — Branson asked for a lighter touch instead, so
  landed as a plain checkable toolbar button in the **footer** bar, right
  before Overlay/View (`initGraphicsFooter()`), matching the style of the
  other view-mode selectors already there (Grid/Snap/Rulers/Labels). The
  banner and the dedicated top-toolbar button were both removed again per
  that feedback — `m_modeLabel` goes back to being unconstructed (a
  no-op, matching its original pre-existing state) rather than half-used.
  Verified live at each step via rebuild + relaunch + screenshot, including
  actually toggling Build focus on to confirm fixtures visibly ghost and
  the footer button state reflects it.

- **Lighting Studio's tab icon changed from `:/monitor.png` (a generic
  system-monitor/pulse icon, a poor fit) to `:/grid.png` — SHIPPED
  (2026-08-17).** `ui/src/app.cpp` (both the tab icon and
  `m_controlMonitorAction`'s icon, kept in sync). Chose from the PNG set
  already compiled into this target rather than the fork's parallel SVG set
  (`resources/icons/svg/`, 157 icons including a much more literal
  `2dview.svg`) — that SVG set turned out to only be wired into
  `qmlui`'s CMakeLists (off for this fork per CLAUDE.md), not
  `ui/src`'s; `Qt::Svg` isn't even linked into the `qlcplusui` target, so
  referencing an `.svg` resource there would've silently rendered a blank
  icon. Using it for real would mean adding the `Svg` component to
  `ui/src/CMakeLists.txt`'s `target_link_libraries` and a CMake
  *reconfigure* (not just a rebuild) — real but avoidable extra risk for an
  icon swap, so stuck to the zero-risk PNG option. `grid.png` was picked
  after visually comparing several PNG candidates (`target.png`,
  `position.png`, `tabview.png`, `global.png`, `diptool.png`, `square.png`)
  — it's the one that actually reads as "2D plotted layout," matching what
  the tab shows (a grid-ruled canvas with Grid/Subdiv/Snap controls). Minor
  known tradeoff: also used for Shows' Snap-to-Grid action, but that's a
  toolbar button in a different tab, not the same visual context, so low
  real confusion risk. If a closer-fitting purpose-built icon matters more
  than the CMake risk, the SVG route is the way to get one — flagging it
  rather than deciding unilaterally.

- **Main tab strip reordered to follow the build workflow — SHIPPED
  (2026-08-17).** `App::init()`'s `addTab()` sequence
  (`ui/src/app.cpp`): was Hardware/Functions/Programming/Shows/Virtual
  Console/Simple Desk/Inputs/Outputs/Lighting Studio (construction-history
  order); now **Hardware → Inputs/Outputs → Lighting Studio → Functions →
  Programming → Shows → Virtual Console → Simple Desk** — rig/setup, then
  build content, then run it, per Branson's requested grouping. Purely a
  reordering of the existing `addTab()` calls (all `setActiveWindow()`/
  `indexOf()`-based lookups elsewhere are by class name or widget pointer,
  never a hardcoded index, so nothing else needed to change); confirmed via
  a live relaunch that all 8 tabs still construct without error in the new
  order and the tab strip reads correctly left to right.

- **Lighting Studio is now a real tab; window-title/detach correctness
  fixes; Shows-tab follow-up fixes — SHIPPED (2026-08-17).**
  - **Lighting Studio (`Monitor`) converted from a lazily-created standalone
    `Qt::Window` into a permanent tab**, constructed once in `App::init()`
    alongside every other tab (`ui/src/app.cpp`, `ui/src/monitor/monitor.{h,cpp}`).
    Prompted by Branson noticing it was the one surface that didn't pick up
    the app's title-bar conventions, and pushing back on my initial "keep it
    floating" recommendation — correctly: since ANY tab can already be
    double-click-detached into its own window via the existing
    `App::slotDetachContext`/`DetachedContext` machinery, there was no real
    functional loss from making it a tab, only redundant special-casing.
    - `Monitor`'s constructor moved from `protected` to `public` (with
      `Q_ASSERT(s_instance == NULL); s_instance = this;` moved into the
      constructor body), matching the exact convention every other
      tab-hosted singleton already uses (`FunctionManager`, `ShowManager`,
      etc.) — no longer a special case.
    - `Monitor::createAndShow()` (6 call sites, all left unchanged) no
      longer constructs anything; it now walks up the parent chain to find
      either the `QTabWidget` (switch to the tab) or a `QMainWindow`
      (currently detached — raise that window instead). All 6 existing
      callers keep working with zero call-site changes.
    - Removed `WA_DeleteOnClose` and the old create-time geometry
      restore/first-run centering logic (moot — the tab is never destroyed
      until app shutdown, exactly like its siblings). Removed the
      now-dead `SETTINGS_GEOMETRY` write in `saveSettings()`.
    - Workspace XML: the old per-Monitor `MonitorWindow open="1"/geometry`
      `AppState` element (its own separate persistence, predating the
      generic `DetachedWindow` mechanism) is retired on the save side —
      Monitor detaching now falls through the same generic `DetachedWindow`
      path as any other tab, since its className is just `"Monitor"`. Old
      saved files with a legacy `MonitorWindow` element are read and
      silently ignored (no crash, no bogus popup) rather than crashing or
      double-opening.
    - Verified live: screenshot confirms no floating window appears at
      startup anymore, "Lighting Studio" renders correctly as a selected
      tab (toolbar/layers panel/grid controls all intact), and the title
      bar reads `qlcconsole - <file> - Lighting Studio` — consistent with
      every other tab, which was the actual ask.
  - **Removed the resulting duplicate View-menu entry**: the tab-jump loop
    now provides "Lighting Studio" (Ctrl+Shift+8) automatically, so the old
    dedicated `m_controlMonitorAction` menu item was pulled from the View
    menu (`ui/src/app.cpp`) to avoid listing it twice. The action and its
    Ctrl+Shift+M shortcut still exist and still work
    (`slotControlMonitor()` → `Monitor::createAndShow()`), just not
    re-added to the menu.
  - **Fixed: main window title didn't update when a tab was double-click-
    detached, and detaching left a dangling tab-bar entry.**
    `App::slotDetachContext()` never called `m_tab->removeTab(index)` (only
    the reattach path's matching `insertTab()` existed) — added it, plus an
    explicit `updateWindowTitle()` call after both detach and reattach for
    safety. Also fixed `m_tabOriginals`' positional-indexing fragility: it's
    built once at startup in construction order and is never reordered when
    tabs are removed/reinserted, so a title/label lookup by index would
    silently point at the wrong tab after any detach. Replaced with
    `QTabBar::tabData()` (set once per tab in the `addTab` lambda, and
    re-set after `insertTab()` on reattach) — this travels correctly with
    each tab through remove/insert, unlike a parallel array indexed by
    position. Same fix applied to the workspace-file `DetachedWindow`
    XML-restore loader, which had the identical missing-`removeTab()` bug.
  - **Shows tab: Follow MTC toggle made authoritative and discoverable
    from within the tab itself** (`ui/src/showmanager/showmanager.{h,cpp}`).
    Prompted by Branson asking where Timer-vs-MTC is actually selected —
    turned out my round-2/3 redesign showed a read-only "● FOLLOWING MTC"
    indicator with no way to toggle it from the Shows tab at all (the only
    control was the Control-menu / footer-MTC-chip toggle, both
    `App`-level). Replaced the static label with `m_followMtcButton`, a
    `QToolButton` bound via `setDefaultAction(m_followMtcAction)` — the
    *same* `QAction` already driving the Control menu and footer chip, so
    all three stay in sync for free. Placed in the always-visible part of
    the transport cluster (not gated by mode) so it's reachable to arm
    *or* disarm follow from either state.
    - **Regression caught and fixed**: `updateMultiTrackView()`'s
      `blockSignals()`-wrapped sync of `m_followMtcAction`'s checked state
      (run on every show switch) updates the checked flag correctly, but —
      because signals are blocked on purpose — never runs
      `slotFollowMtcToggled()`, which is where the button's *text* ("Follow
      MTC (off)" vs "● FOLLOWING MTC") got set. Result: button showed stale
      text out of sync with the actually-correct widget visibility. Fixed
      by moving the text update into the shared `updateTransportVisibility()`
      helper (already called from both paths for the widget-swap fix
      earlier this session), so text and visibility can no longer drift
      apart again.
  - **Three Shows-tab track-header bugs fixed** (`ui/src/showmanager/trackitem.cpp`),
    all found and root-caused by Branson during review:
    1. *Dimmer/intensity bar drawn over the green active-indicator bar*:
       `m_intensityRegion` started at x=8, overlapping the active-indicator
       rect at x:1-11. Moved to start at x=14 (same right edge as before).
    2. *Double-clicking Mute/Solo/Lock opened the track-rename dialog*:
       `mouseDoubleClickEvent()` correctly excluded the name area but still
       fell through to `emit itemDoubleClicked()` for the button row, which
       `ShowManager::slotTrackDoubleClicked()` wires straight to a rename
       `QInputDialog`. Now returns early or the M/S/L/intensity regions
       without emitting anything — a double-click there is just two
       ordinary toggle clicks (already handled by `mousePressEvent`).
    3. *Intensity bar couldn't be dragged, only click-set*: `mousePressEvent()`
       calls the base `QGraphicsItem::mousePressEvent()` first, whose
       default implementation ignores the event for an item that's neither
       movable nor selectable (neither flag is set on `TrackItem`) — an
       ignored press means the scene never grabs the mouse for this item,
       so `mouseMoveEvent()` (which already had correct intensity-drag
       logic) was simply never delivered. Fixed with an explicit
       `event->accept()` right after the base-class call.
  - Everything above verified live via rebuild + relaunch + screenshot
    (title bar, tab strip, Follow MTC button text/visibility together) —
    not code-review-only. The three `trackitem.cpp` mouse-handling fixes
    are code-review-verified only (this environment can't synthesize a
    real double-click or a mouse-drag, the same standing limitation noted
    elsewhere in this file) — worth a real check next time at the rig.

- **Show Editor toolbar redesign (Show/Edit split, centered context-aware
  transport, window-title fix) — SHIPPED (2026-08-17).**
  `ui/src/showmanager/showmanager.{h,cpp}`, `ui/src/app.{h,cpp}`. Follow-on
  to the round-2 toolbar work below, going further specifically on the Show
  Editor per Branson's detailed spec:
  - **"Show ▾" split from "Edit ▾".** The single "Add" dropdown from round 2
    got split in two: **Show** (New Show / Rename show / Delete show — which
    show container you're working on) and **Edit** (Add Track/Sequence/
    Audio/Video + Undo/Copy/Paste/Delete/Change Color/Lock/Timings —
    everything that touches the open show's contents). Matches the user's
    explicit split ("show: add/rename/delete" vs. "edit functions can go
    under Edit").
  - **Snap to Grid moved to a new bottom toolbar row** (`m_bottomToolbar`),
    mirroring the 2D view's own Grid/Subdiv/Snap row at the bottom of its
    canvas.
  - **Play/Stop merged into one button.** `m_playStopButton`
    (`QToolButton::MenuButtonPopup` + `setDefaultAction(m_playAction)`):
    primary click keeps the existing Play/Pause toggle behavior unchanged
    (pause-in-place while running, resume on click, existing MTC-follow
    safety no-op); Stop (full stop + rewind) moved to the button's dropdown
    rather than staying a separate always-present toolbar button. Confirmed
    with Branson before implementing (AskUserQuestion) — Pause behavior
    stays, nothing was dropped.
  - **Transport cluster centered, DAW-style, and context-aware.** Flanking
    expanding-stretch spacers center the whole cluster regardless of what's
    docked left/right (`leftStretch`/`rightStretch`, matching how Logic
    centers its transport). Time position + Length stay always visible
    (position readout doubles as "show position" in either mode). Exactly
    one of two widgets shows depending on the show's actual
    `timecodeFollow()` state: **manual mode** — Play/Stop, Time division,
    BPM (`m_transportManualWidget`); **MTC mode** — a read-only "●
    FOLLOWING MTC" indicator + the TC-offset config button
    (`m_transportMtcWidget`), since there's nothing for local playback
    controls to do while an external code is driving the timeline.
  - **Real bug hit and fixed along the way**: the manual/MTC widgets were
    both showing simultaneously. Root cause #1 — `updateMultiTrackView()`
    (runs whenever the active show changes) syncs `m_followMtcAction`'s
    checked state via `blockSignals()` so switching shows doesn't
    re-arm/disarm anything, but that silently bypassed the
    `toggled()`-driven `slotFollowMtcToggled()` where the visibility swap
    lived — fixed by factoring the swap into `updateTransportVisibility()`
    and calling it from both places. Root cause #2, found only after
    root cause #1's fix still didn't work: `QToolBar::addWidget()`
    implicitly wraps a widget in a `QWidgetAction`; a later toolbar layout
    recompute (`applyToolbarLabelMode()`'s `setToolButtonStyle()` call)
    re-syncs the widget's visibility **from that wrapping action**, which
    was never touched — silently undoing a plain `widget->setVisible()`
    call. Fixed by toggling visibility through the `QAction*` that
    `addWidget()` returns (`m_transportManualAction`/`m_transportMtcAction`)
    instead of the widget directly. Root-caused via a scratch debug build
    (temporary `QFile`/`QTextStream` logging, since `qDebug()` output
    wasn't reaching any log this environment could see) rather than guessing
    — removed before shipping.
  - **"Is Length superfluous?" — answered, kept.** Not superfluous: it
    exists specifically because the timeline's own draggable end-handle can
    sit off-screen on a long show, so `m_lengthButton` stays as an
    always-reachable way to set/inspect it regardless of transport source.
  - **Window title now always shows the showfile name and the active tab**
    (`App::updateWindowTitle()`, replacing the title-building half of
    `slotDocModified()`; new `App::slotTabChanged()` wired to
    `m_tab`'s `currentChanged`). Format:
    `qlcconsole - <file or "New Workspace">[ *] - <Tab Name>`, confirmed
    live via `AXRaise` window-name checks (title flips from "…-
    Inputs/Outputs" to "…- Shows" the instant the tab changes). Uses
    `m_tabOriginals` rather than `m_tab->tabText()` for the tab name since
    the latter goes blank under Icons-Only tab-label mode (the toggle added
    in round 2) — fixed the same latent bug in `slotDetachContext()`'s own
    `tabLabel` capture while touching this code. Detached windows
    (`DetachedContext`, previously titleless) now get a one-time title at
    detach time (`<app> - <file> - <tab label>`) — not live-updated
    afterward if the doc's modified state changes while detached, which
    would need tracking every currently-open detached window; scoped out
    as more than what was asked.
  - Verified live in the running app throughout (screenshots + `AXRaise`
    window-name checks), including catching and fixing the visibility bug
    before calling this done rather than shipping on code-review alone.

- **Toolbar-consolidation, round 2: VC's duplicate mode toggle, a VC "Edit"
  dropdown, Shows tab's Add cluster, and a View-menu toolbar-style switch —
  SHIPPED (2026-08-17).** Direct follow-on to the round below, from a fresh
  pass over what still looked cluttered/inconsistent:
  - **VC's redundant Run/Stop button removed**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`). VC had its own
    always-visible checkable "Run"/"Stop" `QToolButton` (top-right,
    `m_runButton`) toggling `Doc::mode()` directly, *in addition to* the
    app-global "Operate"/"Design" toggle (`App::m_modeToggleAction`, on
    App's own top bar, confirmed always-visible across every tab/mode via
    screenshot). Both drove the exact same single piece of state. Confirmed
    safe to remove the VC-local one: `App::slotModeChanged` is wired to
    `Doc::modeChanged` (`app.cpp:688`) and already keeps the global button's
    icon/text/tooltip in sync regardless of what triggered the mode change.
    The VC-local button's original justification (VC's *own* toolbar
    `m_toolbar->hide()`s itself in Operate mode, per `disableEdit()`,
    `virtualconsole.cpp:~1890` — "there's nothing usable there in operate
    mode") is moot since the global button lives outside that toolbar and
    was never affected by it. Removed the button, its `runBar` row, its
    `slotModeChanged()` sync block, and the member/init-list entries.
  - **VC "Edit" dropdown**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`): the toolbar's twelve
    remaining per-widget actions (Cut/Copy/Paste/Delete/Widget
    Properties/Rename Widget/Bring to front/Send to back/Background
    Color/Background Image/Font Colour/Font) collapsed into one
    `m_editButton` ("Edit ▾", `:/edit.png`), right after the "Add" button.
    Deliberately a *fresh* small `QMenu` built locally in `initMenuBar()`
    (not a reuse of `m_editMenu`, unlike how Add reuses `m_addMenu`) — using
    `m_editMenu` would have also dragged in the dynamically-appended custom
    "Add" submenu (`updateCustomMenu()`), duplicating what the new Add
    button already offers. Entries stay individually enabled/disabled by
    the *existing* `VirtualConsole::updateActions()` selection logic,
    unchanged — with nothing selected, Paste/Background/Font remain
    clickable (they legitimately target the canvas) while
    Cut/Copy/Delete/Properties/Rename/stacking show up grayed rather than
    vanishing, matching the user's "only edit with sub selections" ask
    without needing new gating code. Keyboard shortcuts (Ctrl+X/C/V,
    Delete, Ctrl+E) are untouched — they live on the `QAction` objects
    themselves via `setShortcut()`, independent of which menu/toolbar the
    action is currently displayed in, so moving these into a dropdown had
    zero effect on them.
  - **Shows tab's add-element icons folded into the same paradigm**
    (`ui/src/showmanager/showmanager.{h,cpp}`): New Show + Add Track/New
    Sequence/New Audio/New Video (five separate toolbar buttons) collapsed
    into one `m_addButton` ("Add ▾", `:/edit_add.png`), placed first —
    same `QAction`s, same shortcuts (Ctrl+H/N/E/A/D). `ShowManager` also
    gained the `applyToolbarLabelMode()` method every other manager already
    had (it was missing entirely — the toolbar was stuck on Qt's default
    icons-only regardless of the app setting).
  - **Programming tab's "Add" button retrofitted onto the same shared
    setting** (`ui/src/programmingmanager.{h,cpp}`): previously hardcoded
    to `Qt::ToolButtonTextBesideIcon`; now has its own
    `applyToolbarLabelMode()` reading `workspace/tabLabelMode` like the
    others (found via `App::findChild<ProgrammingManager*>()`, the same
    idiom `App` already uses elsewhere for this tab, since Programming has
    no singleton `instance()` of its own).
  - **New View → Toolbar Style submenu** (`ui/src/app.cpp`,
    `initMenuBar()`/View menu, right after the existing Theme submenu):
    Icons & Text / Icons Only / Text Only, a checkable `QActionGroup`
    calling `App::setTabLabelMode()`. This setting (`workspace/tabLabelMode`,
    `App::m_tabLabelMode`) and its full propagation
    (`App::applyTabLabelMode()` → tab strip + main toolbar + every
    manager's own toolbar) already existed and was already being read
    correctly by every manager — there was simply **no UI control to change
    it**, only a raw QSettings value to hand-edit. `App::applyTabLabelMode()`
    extended to also call `ShowManager::instance()->applyToolbarLabelMode()`
    and the `ProgrammingManager` `findChild` lookup, so the new menu now
    covers all seven tabs' toolbars in one switch.
  - Verified live in the running app: VC toolbar screenshot confirmed
    "Add | Edit | VC Fixture Widget Wizard | Virtual Console Settings" with
    no Run button; Shows tab screenshot confirmed "Add | full show ▾ |
    Rename show | Delete show | Undo | Copy | Paste | Delete | …"; View
    menu screenshot confirmed the Toolbar Style submenu with a checkmark on
    the current ("Text Only") setting, and clicking "Icons & Text" updated
    the global toolbar, the Shows toolbar, *and* the bottom tab strip
    simultaneously in the same screenshot — confirming the single shared
    setting really does propagate everywhere. Reverted the live setting
    back to "Text Only" afterward so this testing didn't leave the user's
    persisted preference changed.

- **Function Manager / Virtual Console: same "Add" dropdown consolidation,
  plus a selection-aware VC context menu — SHIPPED (2026-08-17).**
  Follow-on to the Programming tab change below, extended to the other two
  tabs Branson flagged as still cluttered:
  - **Function Manager** (`ui/src/functionmanager.{h,cpp}`): the toolbar's
    nine individual "New scene/chaser/sequence/EFX/collection/RGB Matrix/
    script/audio/video" buttons + Folder collapsed into one `m_addButton`
    ("Add ▾", `:/edit_add.png`, `QToolButton` + popup `QMenu`) built from the
    *same* `QAction*` objects (`initToolbar()`), so shortcuts (Ctrl+1..9),
    slots, and the tree's own right-click menu (which already reused these
    actions) are untouched — purely a toolbar-layout change. Placed first
    (leftmost). `applyToolbarLabelMode()` extended to also style
    `m_addButton` (a toolbar-added `QToolButton` doesn't auto-follow
    `QToolBar::setToolButtonStyle()` the way `addAction()`-created buttons
    do).
  - **Virtual Console** (`ui/src/virtualconsole/virtualconsole.{h,cpp}`):
    same pattern — the toolbar's 14 individual "New Button/Button Matrix/
    Slider/Slider Matrix/Knob/Speed Dial/XY pad/Cue list/Frame/Solo frame/
    Label/Audio Triggers/Clock/Animation" buttons collapsed into one
    `m_addButton`, reusing the *already-built* `m_addMenu` (`initMenuBar()`)
    as-is — same object also feeds the menu-bar's own "&Add" entry and
    `VCFrame::customMenu()`'s right-click submenu, so no new action list was
    authored. Bonus: `m_addMenu` has 16 entries (also Programmer Frame /
    Show control, previously toolbar-omitted), so the dropdown now exposes
    two actions the old toolbar didn't. Placed first.
  - **VC empty-canvas right-click menu made selection-aware**
    (`ui/src/virtualconsole/virtualconsole.{h,cpp}`,
    `ui/src/virtualconsole/vcwidget.cpp`): right-clicking empty VC canvas
    (nothing selected) used to always show the full `m_editMenu` — Cut/Copy/
    Delete/Rename/Widget Properties included, always enabled=false and
    grayed out but still visually present, with the "Add" submenu buried at
    the very bottom after Background/Foreground/Font/Frame/Stacking
    submenus. New `VirtualConsole::buildEmptyCanvasMenu()` builds a small
    purpose-built menu instead for exactly this case (called from
    `VCWidget::invokeMenu()`, gated on `vc->selectedWidgets().isEmpty()`):
    **Add first**, then Paste (if the clipboard has something), then
    Background/Foreground/Font (these legitimately apply to the bottom
    frame itself with nothing selected — confirmed via
    `VirtualConsole::updateActions()`'s existing enable/disable logic, which
    already special-cased them). Cut/Copy/Delete/Rename/Properties/Frame/
    Stacking are omitted entirely rather than shown disabled — exactly the
    set `updateActions()` already flags as meaningless with an empty
    selection, just now *absent* instead of merely grayed out. The
    when-something-**is**-selected right-click path is untouched (still
    `editMenu()`, unchanged) — this was scoped to the specific "empty
    space" complaint, not a redesign of the selected-widget menu.
    `m_bgMenu`/`m_fgMenu`/`m_fontMenu` (previously `initMenuBar()` locals)
    were promoted to members so both menus can reference the same QMenu
    objects. Deliberately did NOT touch the shared Cut/Copy/Delete/Rename/
    Properties `QAction`s' `setVisible()` — they're also on the toolbar, and
    hiding a shared `QAction` hides it everywhere it's added, which would've
    made toolbar buttons blink in and out during every right-click.
  - Both toolbar changes confirmed via screenshot in the running app (Add
    button present, correctly leftmost, old buttons gone). The VC
    empty-canvas menu content itself is code-review-verified only, not
    screenshotted — same synthetic-input gap as elsewhere in this file:
    there's no `cliclick`-equivalent for a real right-click in this
    environment, and the accessibility tree doesn't expose the VC canvas
    granularly enough to fake one via `AXShowMenu`. Worth a real look next
    time at the rig/laptop.

- **Programming tab: tab-local "Add" menu for New Scene/Chaser/… — SHIPPED
  (2026-08-17).** Branson's ask: find a middle ground between "no icons
  anywhere" and Function Manager's full icon toolbar, and figure out where
  per-function "Add" actions should live given the app has a genuinely
  global menu bar (File/View/Control/Help — confirmed scoped correctly,
  no change needed) plus tabs that get double-click-detached into their own
  bare `QMainWindow` (`App::slotDetachContext`, `app.cpp:1985-2003`) with no
  menu bar of their own. Landed as a small `QToolButton` ("Add ▾",
  `:/edit_add.png`) next to the func-tree filter box in
  `ProgrammingManager`'s nav panel (`ui/src/programmingmanager.cpp`,
  constructor, right after `m_funcTree` is built) — popup `QMenu` with one
  icon'd entry per creatable type (`:/scene.png`/`:/chaser.png`/
  `:/collection.png`/`:/efx.png`/`:/rgbmatrix.png`/`:/show.png`/
  `:/folder.png`, same icons Function Manager already uses), replacing the
  redundant idea of spelled-out "New Scene"/"New Chaser" buttons that just
  duplicated what right-click already offered. Creation logic extracted
  into one shared `ProgrammingManager::addNewFunction(Function::Type,
  const QString &folder)` (`programmingmanager.{h,cpp}`) so the toolbar menu
  and the func-tree's right-click menu (`slotFuncTreeMenu`) call the exact
  same path — no duplicated create/name/select/open logic. The right-click
  menu's own "New …" entries also picked up the same icons while touching
  this code, so the two menus now look and behave identically. Architecture
  point that drove the placement: this had to be a widget owned by
  `ProgrammingManager` itself (not a second app-level menu bar) — only
  widget-owned UI survives `setCentralWidget(context)` when a tab detaches;
  a second `QMenuBar` living on `App` would not follow the tab out.
  Confirmed via the existing Function Manager toolbar, which already proves
  the pattern (it's a child widget in the manager's own layout, so it
  already survives detach today). Built clean
  (`cmake --build build -j --target qlcplusui`, then the full app build);
  screenshotted in the running app and the "+ Add" button is present and
  correctly placed next to the filter box. *Menu contents not
  click-verified in-app* — same synthetic-input gap noted elsewhere in this
  file: `System Events` can invoke the button's `AXPress` (confirms the
  button exists/is enabled) but the resulting `QMenu` popup doesn't render
  for a screenshot to capture, so the actual dropdown items were verified by
  code review, not an eyeballed screenshot. Worth a real look next time at
  the rig/laptop. Not yet extended to other tabs (Function Manager already
  has its own working toolbar; nothing else currently has bare text "New …"
  actions to consolidate) — revisit if that changes.

- **"Look" as the assembly unit (Scene/Collection rethink) — BOTH SLICES
  RESOLVED (2026-08-19)**. Branson shower-thought, worked all the way
  through. Deliberately did NOT rename Scene/Collection (breaks traditional
  QLC users) — instead made the fork's **Look** first-class as two
  independent slices (per the show-lifecycle doc: (1) mainly serves
  Construction, (2) mainly serves Production):
  - **Slice 1 — explicit fixture scope, SHIPPED.** `Scene` gained a
    `LookScope` (`ScopeUnset` / `ScopeWholeStage` / `ScopeGroup` + a
    `FixtureGroup` id), a dedicated typed field following the existing
    `PaletteFade` precedent rather than a generic tag bag (no such bag
    exists on `Function`/`Scene`) — `engine/src/scene.{h,cpp}`:
    `setLookScope()`/`lookScope()`/`lookScopeGroupId()`, XML round-trip as
    `<Function>` attributes (absent = unset, old workspaces unaffected),
    carried through `copyFrom()`. Deliberately a 3-state enum, not "no
    group = whole stage" — unset and explicitly-whole-stage need to stay
    distinguishable. Unit-tested (`engine/test/scene`,
    `Scene_Test::lookScope()`, 21/21 passing). UI: a **"Scope:"** combo in
    `SceneGroupLooks` (the Looks editor, embedded in both the classic Scene
    Editor and the Programming tab canvas) — `ui/src/
    scenegrouplooks.{h,cpp}`, next to the Targets panel, populated from
    every `FixtureGroup` in the doc, kept separate from Targets (declared
    *intent* vs. what's actually painted). Purely organisational metadata —
    doesn't affect playback. *Not yet click-verified in-app* — this
    environment's synthetic-click automation doesn't register on tree/list
    rows (menu-item clicks work, raw clicks don't; same gap noted earlier
    for headful automation), so this shipped on code review + the engine
    test, not an eyeballed screenshot. Worth a real look next time at the
    rig/laptop.
  - **Slice 2 — palette/fixture state as the BASE that effects/RGBScripts
    consume — DONE, by finding + decision, not new code.**
    `EffectInstance::buildPalettesObject()`
    (`engine/src/effectinstance.cpp:894-1001`) already feeds a look's
    painted COLOUR into every running effect every tick
    (`palettes.look.colors`/`palettes.look.dimmer`, read by e.g.
    `resources/rgbscripts/lines.js` via `RGB_HOST_WRAPPER`), already
    falling back to the look's full painted base colour when nothing is
    explicitly nested after the effect (line 953) — "effects respect the
    look's master Dimmer" (shipped earlier) is the same mechanism for
    intensity. **Decided**: this is the correct and complete scope for
    "state as base" — colour/intensity are it, deliberately, not a partial
    build. Everything else a script exposes (`lines.js`'s
    `linesMovement`/`linesType`/`linesPattern`/`linesDistribution`, and the
    equivalent `algo.properties` across the other
    `resources/rgbscripts/*.js` files) stays 100% owned by the
    effect/script itself (`ui/src/lookeditor.cpp`'s properties dialog),
    **on purpose**: colour is already an *external input* in stock QLC+'s
    own RGBScript API (`rgbMap()` scripts are written expecting a colour
    handed in — upstream RGBMatrix always worked this way; deriving it from
    a Look is just substituting *where* that input comes from).
    `algo.properties` are declared and owned *by the script* as its own
    configuration contract — nothing upstream expects those externally
    driven, and forcing it would invent behavior with no basis in how stock
    scripts are authored, breaking compatibility/portability of scripts
    brought in from upstream QLC+.
  - **Terminology settled** (for consistent discussion going forward): see
    the vocabulary table worked out this session — Scene (engine
    primitive) vs. Look (a Scene with Palettes attached, built via the
    Programming tab) vs. Target (what a Look actually paints) vs. Scope
    (what a Look is declared *for*) vs. Palette/Base/Effect/effect-scoped
    palette. Audited the UI against it: no tab-level renames needed —
    "Scene Editor" and the "Scene" type-folder in Function Manager
    correctly refer to the engine primitive, not the workflow, and
    shouldn't become "Look." One real drift found and fixed:
    `SceneGroupLooks`' Targets label said "Fixtures in Scene" in the UI
    while the header's own doc-comment already called it "Targets"
    (`ui/src/scenegrouplooks.h:124`) — code and label now agree
    (`ui/src/scenegrouplooks.cpp`: the label text, its live count update,
    and the intro paragraph's "fixtures in this scene"/"scene fixtures"
    phrasing all read "Targets" now).

- **Release-gate scoping, phases 1+2, + all 3 surfaced failures fixed
  (2026-08-17, BUILT)** — first concrete work off `SHOW_LIFECYCLE_DESIGN.md`'s
  "good gates" thread.
  - **Phase 1 — the macOS `make check` gate was silently broken**, and my
    first fix attempt misdiagnosed *how* it's invoked. There are actually
    **two** `unittest.sh` files: a root-level staging wrapper (already
    correct, already copies every `test.sh` + needed resources into `build/`
    and `cd`s there) that then runs the *copy* of `platforms/linux/
    unittest.sh` it just placed in the build dir. The real, narrower bugs
    were only in `platforms/linux/unittest.sh`: `RUN_UI_TESTS` never got set
    to `1` on darwin at all (`ui/test/*` was unconditionally skipped), and
    the UI-test loop bypassed each test's own `test.sh` (which already knew
    how to resolve a macOS `.app`-bundled QTest binary) in favour of a bare
    `./${test}_test` that doesn't exist for bundle-style tests. Fixed both,
    keeping the script's existing plain-relative-path assumptions intact
    (an earlier pass added `$2`/build-dir-prefixing logic on a wrong model
    of the invocation chain — reverted). Also filled a structural gap the
    now-working gate immediately hit: `engine/test/markplanner` had no
    `test.sh` at all (a real test, just missing its runner).
  - **Phase 2 — model-layer add/remove coverage.** `MonitorProperties` had
    zero test coverage for `addPipe/removePipe` (Boom/Bar/Electric),
    `addStand/removeStand`, `addTower/removeTower`,
    `addStageTarget/removeStageTarget`, and `removeTruss` (add was tested,
    remove wasn't) — added `stageStructureAddRemove()` +
    `stageStructuresXmlRoundTrip()` to the existing
    `engine/test/monitorproperties` suite. `PowerDistribution` (sources,
    circuits, fixture assignment, the direct-source auto-create-circuit-0
    behavior, XML round-trip) had **no test dir at all** — added
    `engine/test/powerdistribution` from scratch.
  - **All 3 pre-existing failures the gate surfaced are now fixed:**
    - `inputoutputmap::profileDirectories()` and
      `qlcfixturedefcache::defDirectories()` both failed on
      `QCoreApplication::applicationDirPath: Please instantiate the
      QApplication object first` — both binaries used `QTEST_APPLESS_MAIN`
      (no `QCoreApplication` instance at all), but the code they exercise
      (`QLCFile::systemDirectory()`) calls `applicationDirPath()` on macOS.
      Switched both to `QTEST_GUILESS_MAIN` (constructs a `QCoreApplication`,
      no widgets/GUI needed). That fixed the warning but exposed the real
      bug underneath: `systemDirectory()` resolves paths relative to the
      app-bundle executable (`Contents/MacOS/<app>` → `../Resources/...`),
      but each test's own expected-path construction assumed a flat
      CWD-relative path — true on Linux (where `systemDirectory()` doesn't
      consult `applicationDirPath()` at all) but not on macOS. Fixed both
      tests' expected-path construction to mirror the same
      `applicationDirPath()/../<dir>` relationship on Apple platforms.
    - `rgbscript`'s "Lines" script failed because its `linesMovement` and
      `linesLifecycle` list properties' declared defaults
      (`algo.linesMovement = 0` / `algo.linesLifecycle = 0`) are dead code —
      `getMovement()`/`getLifecycle()` actually read `algo.linesSlide`/
      `algo.linesRollover`/`algo.linesSizeBehavior`, never given a top-level
      default, so the very first (pre-`setMovement()`/`setLifecycle()`) read
      returned `""` — not one of the property's own declared list values.
      Harmless at runtime (undefined behaved like the intended default, 0),
      but broke property introspection. Fixed by initializing all three
      backing variables at the top of `resources/rgbscripts/lines.js`.
    - Verified clean with **two full, independent `cmake --build build
      --target check` runs** (not just the individual binaries) — fixture
      validation, all `engine/test/*`, all `ui/test/*`, and the enttecwing/
      midi/artnet plugin tests all pass end-to-end.
  - **`mastertimer_test` segfault — found + fixed (2026-08-17).** Root cause
    was a real, deterministic bug, not pure flakiness: `interval()`'s cleanup
    (`fs.stop()`, `mt->unregisterDMXSource(&dss)`) sat *after* a `QVERIFY` on
    a razor-thin real-time tick-count window (49–51 ticks/sec — upstream's
    own `SKIP_TEST` escape hatch for Travis CI is an acknowledgment this was
    always too tight under real scheduling load). `QVERIFY` returns
    immediately on failure, so a timing miss under load skipped cleanup
    entirely — `fs`/`dss` (stack locals) got destroyed while still registered
    with `MasterTimer`, leaving dangling pointers that crashed the *next*
    test method's `timerTick()`. Fixed by moving cleanup before the timing
    assertions (always runs now, regardless of outcome) and widening the
    tolerance to 40–60 (still catches a genuinely broken timer, far less
    sensitive to scheduler jitter). Verified clean on **2 more full `make
    check` pipeline runs** (4 total now, back to back). Not caused by
    anything built in this session (confirmed: `mastertimer` runs and
    completes before
    `ui/test` even starts, so the new `monitor_test` below isn't a factor).
  - **Phase 3 — headful dialog-driven pilot (2026-08-17, BUILT, proven
    viable).** New `ui/test/monitor` — the open question was whether a QTest
    UI test can drive a real, *blocking* `QDialog::exec()` call (as
    `Monitor::slotAddTruss()` uses) under `QT_QPA_PLATFORM=offscreen`.
    It can: schedule the interaction via `QTimer::singleShot(0, ...)` *before*
    calling the slot — `exec()`'s own nested event loop processes it,
    `QApplication::activeModalWidget()` finds the live dialog, `findChild<>()`
    reaches its fields/buttons. `addTrussAccepted()` fills the name field and
    clicks OK, asserts the truss landed in `MonitorProperties` with the right
    name; `addTrussCancelled()` clicks Cancel, asserts nothing was added.
    Both pass, standalone and through the full `make check` pipeline. Power
    Source turned out not to need this pattern at all — its add path
    (`PowerDistributionWidget::slotAddSource()`) is a direct model mutation
    with no dialog, already covered by Phase 2's `powerdistribution` tests —
    so the pilot narrowed to just Truss, which was the one open technique.
    Setup is cheap to replicate (`#define protected public` to reach the
    slot + a bare `Doc`/`Monitor` pair, no plugin/patch wiring needed).
  - **Phase 4 — expanded coverage + `slotRemoveSelected()` (2026-08-17,
    BUILT).** Added to `ui/test/monitor`:
    - `addTargetAccepted()`/`addTargetEditCancelled()` (`Monitor::
      slotAddTarget()`) — same simple-form-dialog pattern as Truss, plus a
      real behavioral difference worth proving: unlike Truss (object created
      only on Accept), StageTarget/Platform/Pipe/Stand/Tower are all created
      *immediately* with defaults, and the dialog that follows is an *edit*
      of the just-created object — so even Cancel leaves it added. Also
      asserts the accept path's bonus effect: a linked PanTilt palette gets
      auto-created.
    - `addPlatformEditCancelled()` (`Monitor::slotAddPlatform()`) — proves
      the same add-then-cancel path through a **heavier** edit dialog (one
      that embeds a full `StructureStudioView` canvas/tree/inspector via
      `makeStudioPane()`, unlike Truss/Target's plain `QFormLayout`).
    - `removeSelectedTruss()`/`removeSelectedCancelled()` — the other open
      half of Phase 3: select a `TrussItem` in the `QGraphicsScene`
      (`item->setSelected(true)`), call `slotRemoveSelected()`, which drives
      a **second, different kind of modal** — `confirmFeatureDelete()`'s
      `QMessageBox`, found the same way via `activeModalWidget()` — Accept
      removes it, Cancel doesn't.
    - **Two real bugs found writing these, both fixed in the tests
      themselves (not app code):**
      1. `QMessageBox::windowTitle()` reads back **empty** on macOS — native
         alert-style message boxes don't surface a title bar, even though
         `confirmFeatureDelete()` does call `setWindowTitle()`. Asserting on
         it left the confirm dialog's `exec()` with nothing ever clicked —
         a genuine **hang**, not a fast failure, and it corrupted whichever
         test ran next. Fixed by asserting on `QMessageBox::text()` instead
         (the actual message content), which *is* reliable.
      2. Blind `findChild<QLineEdit*>()` (no name filter) is ambiguous
         once a dialog embeds `StructureStudioView` — it contains its own
         QLineEdits, so the lookup can silently grab the wrong one instead
         of the name field. Rather than paper over it, **descoped**: no
         "accept with a custom name" test for Platform (or, by the same
         reasoning, Pipe/Stand/Tower — never attempted). The cancel-path
         test for Platform only drives the unambiguous button box, so it
         stays real coverage without the fragile lookup. Reliably testing
         the accept path for these four would need the production dialogs
         to tag their name field with `setObjectName()` first — not done.
    - Verified: 9/9 pass standalone, and **2 more full `make check` pipeline
      runs**, clean both times.
  - **Phase 5 — `release.sh` gate (2026-08-17, BUILT).** New step **1/6**
    (renumbered the existing 5 steps to 2/6–6/6): `cmake --build build
    --target check`, against the standard dev `build/` dir (Debug, reused
    as-is — a correctness gate on the codebase, not a rebuild of the exact
    Release bits `package-local.sh` ships from its own separate
    `build-package/`). `set -euo pipefail` (already at the top of
    `release.sh`) means a failing gate aborts the *entire* release right
    there — before `package-local.sh`, signing/notarizing, tagging, or
    publishing ever run. Verified both directions: the real gate command
    passes clean against this session's actual `build/`; a synthetic
    reproduction of `release.sh`'s exact structure with a deliberately
    failing stand-in step confirmed `set -e` genuinely halts before any
    later step's `step "2/6 ..."` banner even prints (did **not** run the
    real `release.sh` itself — that pushes a git tag and publishes a public
    GitHub Release, real external side effects, not something to fire off
    to validate a shell-flow change). `RELEASE.md`'s step list updated to
    match.
  - **Release-gate arc (Phases 1-5) is now complete end to end.** Remaining
    known gap: full add/remove dialog coverage for Pipe/Stand/Tower, gated
    on tagging their production dialogs' name fields with `setObjectName()`
    first (see Phase 4) — not chased further, no immediate need driving it.

- **Hardware tab: Power tree + universe usage grid (2026-08-12 → 08-13,
  BUILT)** — the former "Fixtures" tab is renamed **"Hardware"** (`app.cpp`,
  one-line tab-label change). Its tree already had a lazily-built
  "Universes" folder (`FixtureTreeWidget::updateTree()`); selecting a
  universe node now swaps the right-hand pane to `UniverseUsageWidget`
  (`ui/src/universeusagewidget.{h,cpp}` — embedded in the splitter like the
  group-layout editor and power view, *not* a popup dialog; started as one,
  corrected after eyeballing it) — a 512-cell address grid coloured per
  occupying fixture (deterministic hue from fixture ID) built on
  `Doc::fixtureForAddress()`, with a tooltip per cell and a fixture legend
  below. A new **"Power"** folder (peer to Fixture Groups/Universes, gated
  behind a new `FixtureTreeWidget::ShowPower` flag so the shared tree widget
  doesn't pick it up in picker dialogs, and *always present* even with zero
  sources so it can be selected/added-to) lists `PowerDistribution` sources →
  circuits → assigned fixtures, mirroring the existing group-folder nesting.
  Right-click the Power folder → **"Add power source…"**; right-click a
  source/circuit → **"Add circuit…"** (same defaults as the Power pane's own
  buttons). Dragging a fixture from anywhere in the tree onto a circuit *or
  a bare source* (lands on its first circuit, auto-created if needed) assigns
  it via `PowerDistribution::assignFixture()` — the same call the existing
  right-click "Add to power circuit" menu already used, now with visual/drag
  entry points too. `slotSelectionChanged()` was rewritten to route
  explicitly by selection type (group → layout, universe → usage grid, any
  Power-tree node → power view, single fixture → its info via the
  previously-dead `fixtureSelected()`, else → generic info) instead of
  defaulting everything-but-groups to the power view. The in-canvas
  Programming-tab power footer (dead code — `ProgrammingManager::
  m_powerFooter` was force-hidden since the readout moved to the app
  status-bar chip) is fully removed; its "Circuits…" button is replaced by
  making the status-bar Power chip itself clickable
  (`ProgrammingManager::openCircuitsDialog()`, wired the same way the MTC
  chip's click-to-bind menu already works).

- **Footer chip polish (2026-08-12)** — Power/Dangle chips (`⚡`/`⚠`) were
  rendering as full-size color emoji next to plain-text chips (Ready/Autosave/
  Saved), reading as a font-size mismatch though the point size was identical
  the whole time; root cause was Unicode emoji-presentation glyphs, not a
  QFont issue. Fixed by appending the text-presentation variation selector
  (U+FE0E) to `⚡`/`⚠`, and gave MTC (`⏱`) and Load (`⚙`) their own leading
  icon in the same style, so all four global status-bar chips read at one
  consistent visual size. Also corrected `platforms/macos/Info.plist.qmlui`
  (only installed when `qmlui` is built) — it still had the pre-rebrand
  `qlcplus-qml` executable/name and `qlcplus.icns` icon refs with no
  `CFBundleIdentifier` at all; now matches the widgets build's
  `com.bransonmatheson.qlcconsole` identity.

- **Note-effect calibration + MIDI plumbing** — per-universe MIDI source scoping
  (`data.midi.universes[n]` + a device-name dropdown, not a slider); switching a
  look's effect script now reloads the live preview; **live param edits reach the
  running effect** (`reloadParamsFromPalette`/`updateEffectParams` — was the root
  cause of "axis does nothing / Learn won't lock"); persistent **Learn range**
  button (writes noteLow/noteHigh, flips to Manual). `QLC_EFFECT_DEBUG=<path>`
  env trace kept (grid/range/held/lit-cols + col0/col64 fixture+DMX+base).
- **Looks: effect-vs-fixture colour separation + tree UI** — a Colour/Dimmer
  palette *nested under* an Effect in the Looks tree feeds that effect and is NOT
  painted as a static base; top-level looks light the fixtures. Fixes same-colour
  strike-on-base washout (RGB-only fixtures). Mechanism = ordering
  (`QLCPalette::isEffectScoped`); tree is derived from/rewrites the flat order.
  Right-click → "Move to fixtures (base)" / "Feed effect ▸ <name>" for explicit
  re-homing; Up/Down moves across nesting boundaries (flat-order move).
  *Open polish:* dropping an external palette onto a specific effect item should
  nest under THAT effect (currently appends). Future: the richer per-item
  assignment could grow beyond order (explicit bind) if multi-effect looks get
  fiddly.
- **Effect perf + release fade-out** — input-reactive effects (midi/audio/
  joystick) now WAIT for input instead of polling: an idle effect whose last
  frame drove nothing is skipped until fresh input (`m_inputDirty` +
  `lastFrameEmpty()`), killing the 50 Hz baseline load of a loaded note effect on
  a big pixel grid. And effect looks have a **release fade-out** set via the Looks
  Fade Out cell — on stop the effect decays over that time instead of snapping
  (EffectInstance fade envelope; runner keeps it ticking then reaps).

## Deferred / next candidates *(open slices carved out of shipped features)*

- **Build host: Raspberry Pi 4 (TODO, 2026-08-25)** — the highest-value box to
  add, because it answers an open question rather than just adding a build:
  1. **Tick punctuality.** 51 universes at `transmitMode="Full"` on a 14-core
     M-series logged 2 `MasterTimer is running late` events in 61 s at only
     50% of one core (see the open item above). Four weak ARM cores is where
     that either holds or doesn't. This is the measurement still outstanding.
  2. **Different ABI.** ARM makes plain `char` *unsigned* where x86 makes it
     signed, plus stricter alignment. DMX values are compared as `char` in
     places (e.g. `QCOMPARE(ua.preGMValues()[0], char(255))` in
     `ui/test/vcxypad/` — `-1` on x86, `255` on ARM). No amount of x86 testing
     finds that class.
  3. **Real target.** Upstream QLC+ ships Raspberry Pi builds; Pi-based
     lighting controllers are common.
  Build 32-bit (armhf) and you also get `qsizetype` as 32 bits — the width
  assumption behind the `%d` format bug fixed in `mastertimer.cpp` this session.
  Setup mirrors 192.168.1.245: cmake, Qt dev packages, xvfb, python3-lxml.

- **Build host: Debian 12 guest (TODO, 2026-08-25)** — a *newer* toolchain
  bound. CI is Ubuntu 22.04 (GCC 11, glibc 2.35, Qt 6.8); 192.168.1.245 is
  Debian 11 (GCC 10, glibc 2.31, Qt5). Bookworm gives GCC 12 + `qt6-base-dev`,
  which is the only combination not currently covered anywhere: Qt6 **and** a
  compiler newer than CI's. Deliberately NOT another Ubuntu 22.04 box — that
  duplicates CI exactly and would never find anything. The value of
  192.168.1.245 was proven the day it was built: its older glibc exposed a
  pthread linkage bug (`4a360911c`) that Ubuntu 22.04 structurally cannot see.

- **Decide whether qlcconsole supports Intel Macs (TODO, 2026-08-25)** — a
  product question, not a coverage gap, and worth settling before building
  hardware for it. The shipped binary is **arm64 only** (`file` on both the
  build output and the packaged app); nothing sets `CMAKE_OSX_ARCHITECTURES`,
  so it targets whatever host builds it. An Intel Mac therefore cannot run the
  current DMG at all. Note this is NOT a Rosetta problem — Rosetta translates
  x86 -> arm, not the reverse, so its sunset doesn't threaten an arm64 build.
  If the answer is "yes, support Intel", the cheap route is probably a
  universal binary (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`; Qt from
  install-qt-action ships universal for macOS) rather than a second build
  machine — one CI config change instead of another host to maintain. Branson
  has an Intel Mac available if a real second machine turns out to be needed.


- **MasterTimer misses ticks at full output load (2026-08-25, 2026-09-02) —
  fixed on macOS, ROUGHED IN elsewhere, NOT YET VERIFIED on Linux/Windows.**
  With 51 universes forced to `transmitMode="Full"` on `ender` (~2484 pkt/s,
  ~49 Hz per universe) a 61 s run logged **2** `MasterTimer is running late`
  events while a 91 s run at the same settings logged **0**. CPU was only 50%
  avg / 64% peak of one core, so this is not throughput saturation — it's
  scheduling jitter, and it's the same wall-clock sensitivity that made
  `MasterTimer_Test::interval()` and `VCCueList_Test::functionRemoved()` flaky
  on CI. Reproduce with `/tmp/throughput.sh` on ender (see DONE.md 2026-08-25
  for the harness).

  **macOS/iOS — fixed, and previously validated** (`feat/realtime-timer-thread`,
  ported from the `rt-test` branch, `8487dbf71`): the DMX timer thread now
  requests Mach `THREAD_TIME_CONSTRAINT_POLICY` (what CoreAudio uses for the
  same problem) in `engine/src/mastertimer-unix.cpp`. Advisory — warns and
  falls back to normal priority if the kernel refuses. This is the run
  referenced above ("real-time-policy run showed zero events of either
  shape").

  **Linux — same file, `SCHED_FIFO` via `pthread_setschedparam`, ROUGHED IN,
  UNVERIFIED** (no Linux box in that session; CI will give a real compile/link
  result on push). Priority is `sched_get_priority_max(SCHED_FIFO) - 10`, not
  max, matching the JACK/PipeWire convention of leaving headroom above this
  thread. **Known limitation, not solved here:** `SCHED_FIFO` normally needs
  `CAP_SYS_NICE` or an rtprio limit, which a stock install will not have — so
  on most Linux installs this falls back to a warning + normal priority
  exactly like an mac refusal would. Packaging a fix (setcap in a postinst,
  or a bundled rtprio limits.d rule) is a separate, bigger decision — not
  done, flagged as its own backlog item below.

  **Windows — MMCSS ("Pro Audio" thread characteristics), ROUGHED IN,
  UNVERIFIED, NO TEST PATH AVAILABLE.** Architecturally different from the
  unix path: `mastertimer-win32.cpp`'s timer callback runs on a Windows
  thread-pool worker (`CreateTimerQueueTimer`), not a thread this code owns,
  so the boost happens inside the callback itself, `thread_local`-guarded to
  run once per actual OS thread. No revert on stop (would need to run on the
  same pool thread that acquired it, which nothing here tracks) — released
  when that thread exits at process end. This one has had zero real-world
  testing of any kind; treat as a rough sketch, not a working feature, until
  someone with a Windows box confirms it.

- **Linux: package a way to actually grant SCHED_FIFO (2026-09-02, follow-on
  to the item above, not started)** — the DMX timer thread's real-time
  priority request will silently fall back to normal scheduling on a stock
  Linux install (no `CAP_SYS_NICE`, no rtprio limit). Options to weigh:
  `setcap cap_sys_nice=eip` on the installed binary from a `.deb`
  postinst/AppImage hook, vs. documenting a manual `/etc/security/
  limits.d/qlcconsole.conf` rtprio rule (the traditional pro-audio-Linux
  convention, needs the user in a group like `audio`). Needs a real Linux
  box to even confirm the roughed-in `SCHED_FIFO` call links/behaves as
  written before this is worth deciding.

- **Gate hole: on headless Linux `make check` silently skips ALL UI tests and
  still prints "Unit tests passed" (OPEN, 2026-08-26)** —
  `platforms/linux/unittest.sh:44` decides whether to run `ui/test` from
  `pidof X` / `pidof Xorg`, unless `whoami` is `runner`/`buildbot`/`abuild`.
  On a headless box under a normal login both checks fail, so `RUN_UI_TESTS=0`
  and **32 UI test binaries never run** — a third of the suite — while the gate
  still reports success. The same branch also leaves `TESTPREFIX` empty, so the
  engine tests run with no platform plugin and `genericdmxsource_test` aborts
  with "could not connect to display". Note `xvfb-run` does NOT rescue it: the
  process is named `Xvfb`, which `pidof X` does not match. Fix is to key off
  "can Qt start a platform plugin" (or just always set
  `QT_QPA_PLATFORM=offscreen`) rather than probing for an X server.

- **The ARM char-signedness tripwire is commented out (OPEN, 2026-08-26)** —
  the assertion cited as the reason to want an ARM build host,
  `QCOMPARE(ua.preGMValues()[0], char(255))` at
  `ui/test/vcxypad/vcxypad_test.cpp:474-483`, sits inside a `/* FIXME !! */`
  block and runs nowhere. The live `char(...)` comparisons in
  `vcxypadfixture_test.cpp` are signedness-*insensitive* (both sides go through
  the same conversion) so they pass identically on x86 and ARM. The Pi 5
  confirms the ABI is there — plain `char` is unsigned, `__CHAR_UNSIGNED__`
  defined — but nothing in the suite exercises it. **Un-commenting that block
  is the experiment that cashes in on the ARM host existing**; until then the
  host proves nothing about this bug class.

- **Debian 12 setup quirk: Qt6 `lrelease` is not on PATH (2026-08-26)** —
  Debian's `qt6-l10n-tools` installs `lrelease`/`lupdate` to `/usr/lib/qt6/bin/`
  with no `/usr/bin/lrelease-qt6` symlink (unlike the Qt5 packages), and
  `translate.sh`'s `which_qt()` probes only `lrelease`, `lrelease-qt6`,
  `lrelease-qt5` on PATH. Build dies at 1% with "lrelease not found". Workaround
  `export PATH=/usr/lib/qt6/bin:$PATH`; durable fix is to fall back to
  `qmake6 -query QT_HOST_LIBEXECS`/`QT_HOST_BINS` or have CMake pass the
  `Qt6::lrelease` target location down.

- **buildhost-qlcplus is missing 10 custom fixture definitions (2026-08-26)** —
  headed runs there open with an error dialog: no definition found for Branson
  LED Movinghead+Circle, Branson LED SPOT, Betopper LM70, ADJ Focus Spot Three
  Z, Oppsk Wall Washer Light Bar, Warmdance XL-450, Junman Two Arm LED Beam,
  PHS Chorus Step Row 64 Heads, WLED Effect Mode. Same cause as the
  `/tmp/*.qxf` load failures seen on that host. Sync the `.qxf` files to
  `~/.qlcconsole/fixtures/` there before using it for real UI validation.

- **~~`-p`/`--operate` does not take effect on headless Linux~~ WRONG — it was
  missing fixture definitions (RESOLVED, 2026-08-26)** — the claim does not
  survive testing, and the reasoning behind it was faulty twice over:
  1. The check grepped the log for `"Starting startup function"`, which only
     prints when a startup function **exists**. Stock `surfacetesting.qxw` has
     `<Engine>` with no `Autostart` attribute, so it measured the absence of a
     startup function, not the absence of Operate mode.
  2. With a valid `Autostart` injected, `-p` works on Linux — proven twice:
     `drift-test.sh` (minimal generated workspace) runs its chaser to
     completion on Debian 12/aarch64, and `surfacetesting.qxw` + Autostart
     enters Operate on Debian 11 as well.
  **Actual root cause:** the custom `.qxf` fixture definitions were absent on
  both Linux hosts. Without them the fixtures fail to load, the functions that
  target them do not load either (`Function start` count 0), so there is no
  startup function, nothing changes universe data, `Universe::dumpOutput`
  short-circuits and the ArtNet plugin is never reached. That is the whole
  reason both Linux boxes transmitted zero packets and could not produce a
  punctuality measurement. Syncing the 99 `.qxf` files to
  `~/.qlcconsole/fixtures/` on each host fixes it.
  **Lesson worth keeping:** a grep for a success message is not a test for the
  condition — a missing log line had two possible causes and the wrong one was
  assumed. Assert the positive precondition first (does this workspace even
  define a startup function?).
- **Note: lateness magnitude was already logged before the `late_us` diag**
  — `mastertimer-unix.cpp:67,74` has a pre-existing
  `qDebug() << "Time is late by" << ... << "nanoseconds"` inside `compareTime()`.
  The new line's genuinely new information is **`compute_us` + `budget_us`**
  alongside it, which is what separates jitter from load; the raw lateness
  figure was obtainable already.

- **TRAP: on Linux, a build-dir run loads NO I/O plugins and silently outputs
  nothing (2026-08-25)** — cost a full 10-minute soak that looked like a clean
  pass. `IOPluginCache::load` resolves via `QLCFile::systemDirectory(PLUGINDIR)`
  (`engine/src/qlcfile.cpp:181`), and on Linux that branch is just
  `dir.setPath(path)` with `PLUGINDIR` a **compile-time absolute**
  (`/usr/lib/qt6/plugins/qlcconsole`, or `qt5` on the Debian 11 box). There is
  no env override. macOS is fine because its branch resolves
  `applicationDirPath()/../PlugIns`, which the build tree mirrors — so this
  bites only on Linux and only when running from `build/` without installing.
  The failure is **silent and looks like success**: every universe logs
  `setOutputPatch - plugin: "None"`, the app runs happily, CPU sits at ~0%, and
  a punctuality soak reports **zero** late events because nothing was ever
  transmitted. Check `grep -c artnet` in the run log, or CPU > 0, before
  believing any Linux throughput/punctuality number.
  Workaround for a build-dir run:
  `sudo mkdir -p $PLUGINDIR && sudo ln -sf ~/git/qlcconsole/build/plugins/*.so $PLUGINDIR/`.
  Worth considering a `QLC_PLUGIN_PATH` env override so test rigs stop needing
  root to run what they just built.

- **ArtNet failure reporting still flaps (OPEN, 2026-08-25)** — `e3905a2a1`
  replaced 2850 identical `sendDmx failed` lines with one report per universe
  per state change, which is an ~87% reduction but not the "one line" first
  claimed. A universe whose target never answers ARP oscillates fail →
  succeed → fail (ARP cache expiry), so the live soak still produced 200
  "is failing" + 150 "recovered" lines across 50 universes in 120 s. Wants
  hysteresis: don't report recovery until it has held for N sends. Deliberately
  NOT built on speculation, because the flapping was caused by absent hardware
  (rig in storage) rather than a real-world configuration.

- **`VCCueList_Test::functionRemoved()` flake — watch, don't assume fixed
  (OPEN, 2026-08-25)** — failed on 2 of ~9 macOS CI runs asserting a tree row
  count right after a 100 ms deferred refresh. `6d3757bb2` switched it to
  `QTRY_COMPARE`, which polls instead of sleeping a guessed interval. NOT
  proven: it passed 7 of 9 runs *before* the change too, and the failure could
  not be reproduced locally even under six spinning cores. Sustained green
  across many runs is the only evidence that counts. If it returns, the fix was
  insufficient rather than wrong. Five other UI test files still use fixed
  `QTest::qWait` and were left alone — none has failed.

- **Windows CI removed, not repaired (OPEN, 2026-08-25)** — `fb9c5fd74` deleted
  the `build-windows` job. Both legs failed at "Fix build" seding
  `platforms/windows/qlcplus4Qt6.nsi`, which the rebrand renamed, and
  RELEASE.md already lists Windows packaging as out of scope and stale. Removed
  rather than left permanently red, because a job that cannot pass trains
  people to ignore CI — which is exactly how this repo ended up with a workflow
  nobody noticed had never run. Re-add when Windows is a real target and the
  NSIS scripts have had their rebrand pass.

- **Warning backlog outside the `-Werror` set (OPEN, 2026-08-25)** — a
  tree-wide `-Wall -Wextra` sweep reports ~1100
  `-Wunnecessary-virtual-specifier`, 92 `-Wdeprecated-declarations` (all in
  Homebrew's OLA headers, not ours), 38 `-Wnon-c-typedef-for-linkage` (e.g.
  `PreviewItem`) and 20 `-Winconsistent-missing-override`. None is in the set
  CI enforces and most are third-party. Noted so the next person doesn't
  rediscover them and assume they're new.

- **Rig fidelity test not done (OPEN, 2026-08-25)** — throughput was measured
  at the NIC with tcpdump, which proves what the console *transmits* but not
  that a node *accepts* it. Branson has a single 4-universe node available;
  the full rig is in storage. That's the test that would confirm packet
  well-formedness end to end. `RIG_TEST_PLAN.md` as a whole remains
  unexercised.


- **Output ENDPOINT reachability check (BACK BURNER, 2026-08-25)** — the
  readiness indicator added in `daeeb97d7` catches one failure mode: the
  workspace names a plugin *line* (for the network plugins, an index into the
  local interface list) that doesn't exist on this machine. It does **not**
  catch the other one: the line is fine, but the ArtNet *node* at the far end
  isn't answering. Found during the live soak on `ender`, where 50 of ~53
  patched universes were failing to send to 13 node addresses
  (`172.18.2.201`–`.230`) — in that instance correctly, because the devices
  were in the trailer, which is exactly why this needs care rather than a
  naive "ping the target" test:
  - a broadcast/subnet target legitimately has nothing to answer it, so
    unreachable ≠ misconfigured;
  - nodes are routinely powered down between calls, and a desk that cries
    wolf every load gets ignored (the same failure mode as the 2850-line
    `sendDmx` spam this replaced);
  - ARP/ping liveness is a poor proxy — an ArtNet node can answer ARP and
    still not be listening on 6454.
  **Shape agreed with Branson (2026-08-25):** the rig is mostly **unicast**, so
  a per-target reachability check IS meaningful and is the main case worth
  building — for a unicast destination, "is that node there?" is a real,
  answerable question. For a **broadcast** target there is nothing to answer,
  so the check degrades to "is the interface/subnet present and up?".
  Critically: validate **directly-attached subnets only**. ArtNet nodes are
  normally on a directly-attached segment; anything routed should be left
  alone rather than guessed at, which also sidesteps the "pingable via the
  default gateway but not actually the show network" false-positive seen on
  ender (192.168.21.214 answered ping while being the wrong interface
  entirely).

  Still informational rather than a blocking NOT READY, and probably ArtPoll
  rather than ICMP — an ArtNet node can answer ARP and not be listening on
  6454.

  **Promoted off the back burner (2026-08-25, Branson):** warn the operator at
  *config* time when a patched unicast target is down, rather than only when
  someone is already chasing a dark universe. The scope is the same as above
  (unicast targets on directly-attached subnets; broadcast degrades to
  "interface up"), but the driver is now setup ergonomics, not debugging.

  **The constraint that makes this non-trivial — poll load must scale with the
  patch, not with the universe count.** This desk routinely patches 50+
  universes (the `ender` soak ran ~53 across 13 node addresses at
  `172.18.2.201`–`.230`). A naive per-universe probe would emit 50+ ArtPolls
  where 13 would do, on a segment already carrying ~2484 pkt/s of DMX — and
  the item above ("MasterTimer misses ticks at full output load") is an open
  question about *scheduling jitter* on exactly that path. Probe traffic that
  perturbs the thing being measured is worse than no probe. So the heuristics
  are load-bearing, not polish:
  - **Coalesce by destination IP, not by universe.** 13 nodes = 13 probes,
    regardless of how many universes each carries. Fan the result back out to
    every universe sharing that target.
  - **Probe off the MasterTimer thread**, and never inside a tick. This must
    not be able to cost a DMX frame.
  - **Config-time and on-demand, not a continuous poller.** On workspace load,
    on patch change, and on an explicit operator "check outputs" — not a
    background heartbeat. A permanent poll across 50 universes is precisely
    the "desk that cries wolf" / spam failure mode this whole area already
    burned itself on once (the 2850-line `sendDmx` flood).
  - **Backoff + hysteresis, shared with the ArtNet flapping item above.** A
    node that is down stays down between probes; don't re-report. The
    "don't announce recovery until it has held for N" rule wanted there is
    the same rule wanted here — build it once.
  - **Bounded concurrency + a short overall deadline**, so a rig with every
    node in the trailer still finishes the check promptly instead of hanging
    the load path on 13 timeouts.
  Open: whether the result surfaces as a per-universe column in the I/O map,
  the existing readiness indicator (`daeeb97d7`), or a footer chip. Probably
  the first — it is a per-patch-row fact.


- **App icon does not reach the runtime on Linux (OPEN, 2026-08-27)** — the
  asset is fine; the delivery is not. `resources/icons/png/qlcconsole.png` is a
  purpose-made fork icon (not the old QLC+ logo), `app.cpp:314` does
  `setWindowIcon(QIcon(":/qlcconsole.png"))`, and `ui/src/qlcui.qrc` aliases it
  correctly. What is wrong:
  1. **One size only.** A single **96×96** PNG is shipped, and it is installed
     to the legacy `share/pixmaps/` rather than the freedesktop icon theme
     (`share/icons/hicolor/<size>/apps/qlcconsole.png`) --
     `platforms/linux/CMakeLists.txt:9-13`. Modern shells prefer hicolor, and
     scaling one 96px source to 128/256 looks soft. `.desktop` says
     `Icon=qlcconsole`, which resolves through the theme first.
  2. **The SVG is never installed.** `resources/icons/svg/qlcconsole.svg`
     exists and would give every size for free via
     `share/icons/hicolor/scalable/apps/`.
  3. **Nothing is installed at all when running from `build/`**, which is how
     every test run works, so the desktop falls back to a generic icon. Only
     `_NET_WM_ICON` from `setWindowIcon` applies, and shells vary in whether
     they use it for the task list.
  4. **macOS dev builds are a bare binary**, not a `.app`, so
     `CFBundleIconFile=qlcconsole.icns` only takes effect in a packaged DMG --
     the icon is never seen during development.
  Fix: install the SVG to `hicolor/scalable/apps/` plus rendered PNGs at
  16/22/24/32/48/64/128/256 to `hicolor/<size>/apps/`, keep `share/pixmaps` for
  compatibility, and run `gtk-update-icon-cache` on install. The `.icns`
  (1024×1024) and `.ico` are already right for macOS/Windows packaging.

- **Art-Net node configuration (ArtAddress) — send path built and proven, but
  no node here applies it (2026-08-26)** — `artnet-config.py` at the repo root
  builds and sends ArtAddress (OpCode 0x6000); the plugin defines
  `ARTNET_ADDRESS` and has never used it. Phase 1 (read/browse) shipped; this
  is the write half.
  Packet correctness is established two ways: the CR041R **acknowledges** a
  short-name write by moving its NodeReport to `0006` (`RcShNameOk`), and OLA
  logs `ArtNet got unknown packet 6000`, i.e. it read the header and opcode and
  simply does not implement ArtAddress.
  **But nothing applies.** Confirmed across a real power cycle of the CR041R:
  the name came back `CR041R_001`, not the value written. This node acks and
  discards -- it is front-panel configured, and its Status1 port-address
  programming-authority bits never say "set by network". OLA has no ArtAddress
  support at all. **Validating that changes stick needs a node that implements
  network programming**; until then the write path stays out of the UI.
  Two protocol notes worth keeping:
  - The **NodeReport code is how a node acknowledges a config write** -- the
    advertised fields may not change even on success, so diffing them is not a
    test. Watch the code (`0006` RcShNameOk, `0007` RcLoNameOk).
  - The NodeReport **counter does not reset on reboot** on this node (8943 →
    31425 → 31733 across a confirmed restart), so it is useless as a power-cycle
    indicator. The report *code* is the reliable signal.
  Also: `NetSwitch`/`SubSwitch`/`SwIn`/`SwOut` are only acted on when bit 7 is
  high (program 7 as `0x87`); `0x00` means "reset to zero" and `0x7f` means
  "leave alone", so `0x7f` -- not `0x00` -- is the safe default for a field you
  do not intend to change.

- **RDM configuration tooling — DMX-Workshop-class rig setup (NEW, 2026-08-25,
  Branson)** — make the desk the thing you use to *set up* the rig, not just
  drive it: discover devices, read what they are, and set DMX address /
  personality / inverts from the console instead of climbing to every fixture's
  menu or carrying a laptop with DMX-Workshop on it.

  **This is an extension, not a green field — check what's already here first.**
  Upstream QLC+ shipped "Preliminary RDM support" (`9050a7f13`) and it is still
  wired in:
  - `plugins/interfaces/rdmprotocol.{h,cpp}` — a real E1.20 packetizer/parser
    with the standard PID table already defined (`PID_DEVICE_INFO`,
    `PID_DMX_START_ADDRESS`, `PID_DMX_PERSONALITY`(+`_DESCRIPTION`),
    `PID_IDENTIFY_DEVICE`, `PID_DEVICE_LABEL`, `PID_PAN_INVERT` /
    `PID_TILT_INVERT` / `PID_PAN_TILT_SWAP`, `PID_LAMP_HOURS`,
    `PID_SENSOR_VALUE`, `PID_SUPPORTED_PARAMETERS`, …). The protocol layer is
    NOT the missing piece.
  - `QLCIOPlugin::RDM` capability + `sendRDMCommand()` / `rdmValueChanged()`,
    implemented by **both** transports this rig actually uses: `artnet`
    (`ArtNetController::sendRDMCommand`) and `dmxusb` (Enttec DMX USB Pro).
  - `ui/src/rdmmanager.{h,cpp,ui}` (~900 lines) — a discovery worker thread +
    UID list + raw get/set-a-PID panel, reachable from Fixture Manager
    (`fixturemanager.cpp:457`).
  So the gap is **workflow, not plumbing**. Today it is a protocol inspector:
  it can talk to a device if you already know which PID you want. DMX-Workshop
  parity means turning it into a rig-setup tool.

  Wanted (roughly in value order — needs its own design doc + a decision on
  where it lives, since Fixture Manager is already crowded):
  1. **A device table, not a UID list** — one row per discovered device with
     manufacturer / model / label / current address / footprint /
     personality, populated by auto-`GET`ting the obvious PIDs on discovery
     rather than making the operator issue each one.
  2. **Editable address + personality in place**, with **overlap detection**
     across the discovered set (the actual reason people open DMX-Workshop:
     "who is sitting on top of whom"). Changing personality changes footprint,
     so re-check overlaps after a personality set.
  3. **Identify** as a first-class button per row (`PID_IDENTIFY_DEVICE`) —
     the "which physical unit is this?" loop, and the single most-used RDM
     feature on a call.
  4. **Reconcile against the patch** — match discovered devices to patched
     `Fixture`s and flag the three mismatches that ruin a focus session:
     patched-but-not-present, present-but-not-patched, and
     address-differs-from-patch. Offer "push patch → rig" and "pull rig →
     patch". This is where it stops being a generic RDM tool and becomes
     *this* console's, and it ties into `RIG_TEST_PLAN.md` /
     `SHOW_LIFECYCLE_DESIGN.md`'s Test-Validate phase.
  5. **Sensors + lamp hours** (`PID_SENSOR_VALUE`, `PID_LAMP_HOURS`,
     `PID_DEVICE_HOURS`) — maintenance readout; low priority, easy once (1)
     exists.

  **Hardware finding (2026-08-25) — the ArtNet node on hand does NOT answer
  RDM, so this is currently BLOCKED on hardware.** Probed `172.18.2.10`
  directly from the show VLAN (this Mac is on `vlan0` as `172.18.2.17`):
  - Node identifies as **`CR041R_001`** / `CR041R` — a **4-port** ArtNet→DMX
    gateway, ArtNet 3, `net/sub 0/0`, `swOut 0,1,2,3`, OEM `0x0022`,
    ESTA `0x707A`.
  - It answers **`ArtPoll` → `ArtPollReply`** immediately and cleanly
    (broadcast to `.255:6454`, not unicast back to the requester — relevant to
    the endpoint-reachability item above if that ends up ArtPoll-based).
  - It **ignores `ArtTodRequest`**: four sent (universes 0-3), full wire
    capture of everything from that host, **zero `ArtTodData`** back. Its own
    `goodOut` bits claim RDM is *not* disabled (bit 3 clear on all 4 ports),
    i.e. it advertises a capability it does not deliver.
  - The request packet was checked field-by-field against the Art-Net 4 spec
    before concluding (56 bytes; Net@21, Command=TodFull@22, AddCount@23,
    Address@24) — the silence is the node's, not a malformed probe.
  - **First probe was invalid — corrected same day.** The initial run used
    opcode `0x8080` for ArtTodRequest; the correct value is **`0x8000`**
    (`plugins/artnet/src/artnetpacketizer.h:36`). The node was right to ignore
    an undefined opcode. Re-probed with the correct opcode **and** an
    `ArtTodControl`/**AtcFlush** (`0x8200`, command 1) first — AtcFlush is the
    step that forces a node to actually *re-run* discovery, where ArtTodRequest
    only asks for the TOD it already holds. Result unchanged: 16 `ArtPollReply`,
    **zero `ArtTodData`**. The conclusion stands, but only after the correct
    test. Note QLC+'s own `sendRDMCommand` sends ArtTodRequest with **no
    preceding AtcFlush** — worth revisiting if a real node ever returns a
    stale/empty TOD.
  - **CONFIRMED by DMX-Workshop (2026-08-26).** Artistic Licence's own tool --
    the Art-Net reference implementation -- connects to the node fine and
    reports outright: **"RDM: Node is not RDM capable (Unidirectional DMX)"**,
    with `Rdm Devices: 0 Detected of which 0 active` on all four outputs
    (firmware V0.14, MAC 02:4D:48:12:02:0A, static, LLRP not supported).
    So the probe's verdict was right, independently confirmed.
    The reason is **hardware, not configuration**: RDM needs bidirectional
    RS-485 (talk, then turn the line around to listen), and this node only
    transmits. No firmware setting or update can add it.
    **Correction (2026-08-26): the node was never lying; the wrong bit was
    being read.** GoodOutput bit 3 means "RDM is *disabled* on this port" -- a
    per-port setting, meaningless on a node with no RDM at all. The capability
    flag is **Status1 bit 1**, and across 80 sampled ArtPollReplies it reads 0
    ("not RDM capable") every time. The node reports itself correctly.
    Separately, its Status1 does flap between `0x00` and `0x74` (ROM-booted +
    port-address programming authority + indicator state) roughly 60/40, which
    is almost certainly why DMX-Workshop's displayed lines appear to change on
    their own -- but bit 1 stays 0 throughout, so the RDM verdict never
    actually flips.
    For capability, read Status1 bit 1; GoodOutput bit 3 answers a different
    question. Still worth confirming with a TOD request, since capability bits
    describe intent and a TOD reply is evidence.
    Consequence: RDM must come via the **DMXKing/Enttec Pro USB dongle**
    (wired, bidirectional; `EnttecDMXUSBPro::sendRDMCommand` already exists) or
    via **OLA** acting as an Art-Net→RDM gateway. The CR041R stays a pure
    output path.
    Worth copying from DMX-Workshop's UI: it presents node config as a tree and
    reports RDM device counts **per DMX output**, not per node -- matching the
    per-port finding above.
  - **Control case found, which resolves the earlier ambiguity.** The Pi 5
    build host (`192.168.20.119`) runs an **OLA Art-Net node**, and OLA answers
    the identical probe with `ArtTodData rdmVer=0x01 uidTotal=0 uidCount=0` —
    i.e. a node with an *empty* TOD and nothing attached still **replies**. So
    "silent" is not how an empty TOD presents. The CR041R's total silence is
    therefore best explained by it **not implementing Art-Net RDM at all**, and
    that no longer needs an RDM fixture to establish.
  - **Consequence:** an RDM-capable fixture on a CR041R DMX port will not be
    discoverable *over Art-Net* regardless of the fixture's own RDM support —
    the gateway will not relay it. (Branson has an **ADJ 3Z** on the rig and is
    ordering a known-RDM fixture, 2026-08-25.) Viable transports are therefore
    the **Enttec DMX USB Pro** (wired), or **OLA** as an Art-Net→RDM gateway.
  - **OLA is a zero-hardware development target.** It implements Art-Net RDM
    correctly, and OLA's dummy/simulated-RDM devices would let the entire
    device-table / reconcile workflow be built and tested with no rig at all —
    which would move this item off the ✈️-blocked list. Worth confirming
    before assuming this feature needs hardware.
  - **SUSPICION, not yet proven — QLC+'s `ArtTodRequest` is 25 bytes; the
    Art-Net spec's is 56.** `ArtNetPacketizer::setupArtNetTodRequest`
    (`artnetpacketizer.cpp:161`) builds a 12-byte common header + 9 filler/spare
    + Net + Command + AddCount + **one** Address byte = 25 bytes. The spec
    defines `Address` as a fixed `[32]` array, making the packet 56 bytes.
    Permissive nodes will not care; a strict one would drop it — which is
    exactly the shape of bug that makes RDM "mysteriously not work" against
    real hardware while the code reads fine.
    **NOT verified.** The comparison was attempted against the OLA node on the
    Pi and was inconclusive: OLA answered the 56-byte probe earlier in the day
    but answered neither length while the Pi was under a parallel build
    (load 4.25), so the negative result says more about OLA being starved than
    about packet length. **The test to run**, on an idle box with a known-good
    RDM node: send the byte-exact 25-byte QLC+ layout and the 56-byte spec
    layout to the same node and compare `ArtTodData` replies. Do this before
    concluding anything about QLC+'s RDM working or not working.
  - Develop against **`rdm-sim.py`** (repo root) when no node is available —
    it answers ArtPoll / ArtTodRequest / ArtTodControl with a synthetic TOD and
    ArtRdm GET for the device-table PIDs. Verified working end to end against
    `artnet-probe.py` (3 simulated UIDs). Note it is deliberately *permissive*
    about request length, so it cannot settle the 25-vs-56 question above.
  - Reproduce any of this with **`artnet-probe.py`** at the repo root.

  **Paradigm answers (2026-08-25 discussion) — no new U→node binding needed.**
  `ArtNetController::sendRDMCommand` (`artnetcontroller.cpp:374`) already looks
  up `m_universeMap[universe]` and sends the TOD request to that universe's
  patched `info.outputAddress` / `info.outputUniverse`. **RDM rides the
  existing output patch**; Inputs/Outputs is already the mapping UI. Likewise
  `handleArtNetTodData` (`:515`) → `rdmValueChanged` → `RDMManager` already
  carries replies back, so "self-populate what it finds" is plumbed — the gap
  is auto-`GET`ting the descriptive PIDs per UID to fill a table.
  - Nuance: the binding is **U → (node IP, node *port*)**, not U → node. The
    CR041R is one IP carrying four universes, so discovery is **per-port** —
    a 4-port node needs 4 TOD requests. Note this is the *opposite* of the
    coalesce-by-IP rule in the endpoint-reachability item above (13 IPs, not
    53 universes): reachability coalesces, RDM cannot. Don't share that code
    path by accident.

  **Reconcile semantics — only one of four cases is an error:**
  | Discovered | Patched | Verdict |
  |---|---|---|
  | yes | yes, same address | matched |
  | yes | nothing at that address | **not an error** — offer to patch it |
  | no | yes | **must NOT be an error by default** |
  | yes | yes, *different* address | the real, actionable conflict |
  Row 3 is the trap: most conventionals and much LED gear have no RDM at all,
  so "patched but not discovered" is the *normal* case on a mixed rig. Flagging
  it would cry wolf on first use — the same failure mode as the 2850-line
  `sendDmx` flood.

  **Blocking design decision — `Fixture` must persist an RDM UID.** Confirmed
  `engine/src/fixture.h` has no `uid`/`rdm` field today. Without a stored
  UID↔Fixture binding, reconciliation can only match on address, which is
  circular: "this fixture moved address" is indistinguishable from "a different
  fixture is now at this address". Row 4 above is unbuildable until this is
  settled. It is a `.qxw` schema addition on `Fixture` — decide before writing
  any UI. Also: there are **no RDM tests anywhere in the tree**.

  **Cautions.** RDM is in-band with DMX on a wired line, so discovery
  interleaves with output — do not run it from the MasterTimer thread, and
  expect it to be visibly disruptive on a live wired universe (fine in
  Construction phase, dangerous mid-show; probably gate it the same way Blind
  is gated). ArtNet RDM is a different animal from wired RDM and node support
  is uneven, so verify per-node rather than assuming. Discovery is a binary
  search over the UID space and is *slow* — the existing worker is already a
  `QThread` for that reason; keep it cancellable. **Needs the rig** to be
  worth anything: none of this is verifiable offscreen beyond parser unit
  tests, and there are no RDM tests today.

- **Simple Desk sliders have no write-back-to-scene path (PARKED, 2026-08-12)**
  — found while triaging the old `live-edit-4.x` branch: Virtual Console
  sliders/XY-pads already write manual adjustments back into the running
  Scene via `CaptureManager::recordOverride()` (wired from `vcslider.cpp`/
  `vcxypadfixture.cpp`, with the existing capture/undo/diff/store workflow —
  see `LiveCaptureDialog`), but `SimpleDesk` (`ui/src/simpledesk.cpp`) has
  zero `CaptureManager` references — moving a Simple Desk fader doesn't
  persist anywhere. Better path is wiring Simple Desk's
  `slotUniverseSliderValueChanged` into `CaptureManager` rather than
  reviving `live-edit-4.x`'s standalone `LiveEditManager` (undo-less
  direct-write) — confirmed `CaptureManager` isn't just equal plumbing,
  it's strictly *better* than that reference implementation:
  - `LiveEditManager::findMostSignificantScene()` (the "which running Scene
    actually wins when several touch this channel" question) was left as an
    explicit `// TODO: Implement HTP/LTP priority logic` stub —
    `return scenes.first();`, i.e. never solved. `CaptureManager::buildPlan()`
    already solves exactly this: it walks running functions in **start
    order**, so the most-recently-started Scene wins LTP per (fxi, channel) —
    a real, reasoned answer, inherited for free.
  - `CaptureManager::buildPlan()` also already flags **chaser-driven
    channels** (a channel currently under a running Chaser step's Scene) and
    excludes them from capture by default — `LiveEditManager`'s plain
    fader-walk has no equivalent, so a Simple Desk tweak there could point-edit
    a Scene the Chaser immediately overwrites on its next step. Preserve this
    exclusion when wiring Simple Desk in.
  - One piece of `live-edit-4.x` *is* directly reusable regardless: unlike VC
    widgets (pre-bound to a specific function), a Simple Desk slider only
    knows an absolute `(universe, address)` — `LiveEditManager::
    findScenesForChannel()`'s address -> `(fixtureId, channel)` resolution
    via `Doc::fixtureForAddress()` is the correct, already-written way to get
    from "which slider moved" to the `(fxi, channel)` pair
    `CaptureManager::recordOverride()` actually wants.
  - Net effect: Simple Desk gains MORE than `live-edit-4.x` ever had (undo,
    conflict/chaser-driven visibility, save-as-new) for less new code than
    reviving `LiveEditManager` would take.
  Needs its own design/implementation pass, not a quick cherry-pick.

- **Cue transition model — hold-on-miss + release-on-transition (4b; DEFERRED,
  needs rig)** — DECIDED framing: a *missed/skipped* cue is a non-event → hold
  last look (no blackout); a cue that *fires* releases what it replaces (outgoing
  look + its effects fade out) → no dangling. Split confirmed: **fade intensity,
  hold position/colour**. Risky part = the intensity-latch core-mixer work; needs
  a live rig to test, so deferred until Branson has rig time. First verify whether
  the current timeline transition already releases the outgoing look or holds it.
- **Pre-positioning / mark cues + ghost visual + dangle detector** — how big
  consoles do move-in-black. See `MOVEINBLACK_DESIGN.md`. Slices **1 + 2 SHIPPED**
  (all rig/visualiser-verify):
  - **1** — `MarkEffect` DMXSource (holds non-intensity, auto-releases on reveal,
    `<Mark>` XML) + Mark/Unmark buttons + dashed-violet monitor ghost.
  - **2a** — `CueOutput`: offline "what would this cue output" (unit-tested).
  - **2b** — `CueLookahead`: next cue + lead time for Chaser & Show.
  - **2c** — `MarkPlanner` + **Auto MIB** toolbar toggle: look-ahead → pre-set
    dark→lit→moving movers, dark-gap gated.
  - **2 persist** — Auto-MIB toggle + dark-gap round-trip (`<MoveInBlack>`) + a
    "s lead" toolbar spinbox. DONE.
  - **3 — dangle detector** — `MarkPlanner::dangleFixtures()` + `dangleFixturesChanged`
    signal, forwarded `ProgrammerController` → App footer chip. DONE (unit-tested,
    engine/test/markplanner). Runs regardless of Auto-MIB being on (manual marks
    dangle too). *Needs rig eyeball to confirm the chip reads right live.*
  Still open: **verify CueLookahead timing on a rig** (the dark-gap depends on it);
  **force-live / force-mark** per-cue overrides + dark-move fade. Also: mark "to a
  chosen look"; a monitor context-menu Mark action. *(from the cue-policy discussion)*

## Build-season freeze list *(LOCKED with Branson — ship before feature freeze)*

- 🔴 **Rig test pass** — run `RIG_TEST_PLAN.md`, fix any ❌ (the gate).
- 🔴 ~~Auto-MIB persist + dark-gap setting~~ **DONE** (b38795658).
- 🔴 **PMJ (OpenDeck) mapping** — the operating surface for the show (free knobs =
  the relative encoders already supported). APC40 map de-scoped. *Best built with
  the PMJ in hand (LED feedback / knob verify).*
- 🟡 ~~Power/circuits → footer bar~~ **DONE** (a9aab77db) — ⚡ status-bar chip.
- 🟡 ~~Native single-shot~~ **DONE — became the effect-lifecycle work** (4c7c9621c,
  `EFFECT_LIFECYCLE_DESIGN.md`): effects declare loop/reactive/**oneshot**;
  one-shots run on `inputs.phase`, duration resolves Look→cue/chase→default,
  hold/release on finish; Wand example + per-look length UI. *Follow-ups (post-
  freeze OK): span on the Show timeline (falls back to naturalDuration today);
  fold RGBScript `Once` into the lifecycle; per-look syncTo/onFinish UI.*
- 🟡 ~~Small polish batch~~ **DONE** — drop-onto-specific-effect nesting (5e-ish),
  End-at-SMPTE (751ce1b5a); MTC-chip glyph was already in place.
- ⚪ ~~dangle detector~~ **DONE** (2026-08-12) — see slice 3 above.
- ⚪ Post-freeze: cue-transition 4b; "Look" as assembly unit;
  unified object editor; more stage objects; rebrand to qlcconsole.
- **Effects respect the look's master Dimmer — SHIPPED** (563c4b3b1): colour
  output scales by the look's Dimmer on dimmerless fixtures; dimmered fixtures
  carry it on the master channel. Move to DONE.md next pass.
- **Move circuits / power usage to the footer bar** — the power/amperage &
  circuit load estimator currently lives in the Programming space; move it to the
  footer bar (like the timecode/load chips) so it's an always-visible status
  readout and reclaims programming canvas. *(Branson request)* See memory: power
  estimation feature.
- **New control surface — integrate (TBD)** — a new hardware control surface to
  bring in (details TBD). Fold into the MIDI-mapping work below once specced.
- **Control-surface engine (PMJ + APC40 mk2 + Xbox) — IN PROGRESS, at the rig.**
  See `CONTROL_SURFACE_DESIGN.md`. Device-agnostic engine: surface model +
  role/page vocabulary + context-aware LED loop; boards are overlays. **P0
  CORE — DONE** (81eaab311, unit-tested `engine/test/controlsurface/`).
  Decisions locked: static core = **GM, Blackout, Blind, Tap, Go, Back**
  (identify more via workflow); **faders follow the page** (optional
  submaster page). All 4 original P1-blocking board facts are resolved
  (encoder CCs 11-14, LED velocity/steady-level scale, output channel 9,
  static-core placement `O`→Blackout/`Set`→Blind). **P1 PMJ overlay + LED is
  underway** (`ui/src/pmjoverlay.{h,cpp}`, slices 1-6 in "Recently shipped"
  above) — LEDs light only for wired controls, Master fader → Grand Master,
  `O`/`Set` → Blackout/Blind, and Enc 1/2 now nudge a focused PanTilt
  palette's XY pad live, direction-confirmed on the real board. **Still
  open in P1**: Select/Load/Transport wiring (needs the app-side
  selection/paging model — not built yet), Favorites/Tap binding (only a
  Programming-tab-local tap-tempo exists today, not the global static-core
  action the design doc means), per-strip fader Level(1-10) semantics
  (submaster vs per-fixture — open design question for Branson), Enc 3/4
  targeting (colour/beam — needs the same selection model as Select/Load),
  and the `Set` button's LED specifically (toggles correctly but doesn't
  light — likely a hardware button/LED address pairing issue, try
  `qlc-midi opendeck identify`/`align`). Then: P2 runtime page → P3 APC40
  mk2 overlay → P4 Xbox roles. Plumbing found: buttons are MIDI notes
  (offset-128; LED note == button note on ch9); faders/enc-push are CC
  (offset<128); LED via `InputOutputMap::sendFeedBack`→`feedbackToMidi`.
  Relative encoders already built.
- **Timecode slice 3 — auto-fill internal latency** — the packet→DMX figure needs
  plugin-side timestamping; once measured it folds into the offset. *(from Timecode
  calibration in DONE.md)*
- **Audio calibration — active loopback self-test** — play a click on the audio
  output, time the round-trip to detection = true audio latency. Needs an audio-emit
  path (AudioRenderer wants a decoder) and is unverifiable offscreen; the
  editable/seeded detection-latency + ±10 ms nudge cover it meanwhile.
- **Show-length polish** *(2026-08-12: end-handle label collision confirmed
  already fixed — commit dcc620241 moved the length chip to the ruler strip
  above the marker lane specifically to stop it colliding with a section
  marker's own label. Genuinely still open:)* "End at SMPTE hh:mm:ss"
  convenience (needs the offset in the host menu); bar/beat snapping for the
  end handle when a BPM is set.
- **GUI headful automation** — `screencapture` + `cliclick` driver so Claude can
  drive AND validate real UI (moving this to the spare machine). First task there:
  a `gui-drive.sh` wrapper, then drive the end-handle drag with eyes on real pixels.
- **Clickable Load chip → per-function breakdown (2026-08-12 idea)** — today
  `MasterTimer` only times the whole tick as one number (`engine/src/
  mastertimer.cpp`, the `computeTimer` around `timerTickFunctions()` +
  `timerTickDMXSources()`); no per-Function/per-DMXSource granularity exists.
  Would need wrapping each `function->write()` call in the tick loop
  individually, accumulating per-function-id compute time, and exposing a
  top-N query — real engine instrumentation, not a UI-only change. The
  existing single-number Load chip itself is already cheap (a single
  `QElapsedTimer` read per tick, always-on regardless of the chip; the footer
  just polls an atomic every 500ms) — no separate background load meter
  needed for that part.

---

## Done — real "Import" for .qxw, distinct from Open *(2026-08-12, BUILT)*

File > Import: merge fixtures/fixture groups/functions from a second .qxw
into the CURRENTLY OPEN document, unlike Open (which replaces everything).
Resolved the open design questions from this item's original design-first
note:
- **Scope**: fixtures, fixture groups, AND functions/shows — not fixtures-only.
  Picking a function auto-expands to its full dependency closure (member
  functions, the fixtures/groups they touch) via new `Doc::functionFunctions()`
  (mirrors the existing `Doc::functionFixtures()`) — no manual dependency
  picking needed.
- **ID collisions**: real remap-on-import, not a blind merge. Three separate
  ID spaces (fixture/function/fixture-group) each keep the source ID when
  it's free in the target, else get a fresh one; every cross-reference
  (`SceneValue::fxi`, `ChaserStep::fid`, `RGBMatrix`'s fixture group,
  `FixtureGroup` head assignments, `Show`/`Track` function refs) gets
  rewritten afterward to match. Also handles a DMX-address-space collision
  (a fixture ID can be free while its address range still overlaps something
  already patched) by relocating within the same universe, reported
  separately from ID remaps.
- **UI**: dedicated `ImportSelectionDialog` (browse + pick), not drag-and-drop
  of a file — matches the existing `FixtureSelection`/`FunctionSelection`
  picker convention rather than inventing a new one.
- New engine module `engine/src/qxwimporter.{h,cpp}` (`QxwImporter::import()`)
  does the actual closure/remap/clone/rewrite work, engine-side and
  UI-independent; loads the source file into a throwaway scratch `Doc`
  (`App::loadScratchDoc()`) rather than ever touching the live one mid-merge.
- Verified end-to-end against real workspace files (not just code review):
  clean imports, heavy-collision imports (96/96 IDs correctly remapped, exact
  fixture/group/function count arithmetic), and an adversarial dense-universe
  case (correctly reports "no free DMX address" per fixture rather than
  corrupting anything) — all with zero dangling fixture references across
  every function in the target doc afterward.
- Caught and avoided reusing `Function::createCopy()`/`copyFrom()` for this —
  `Show::copyFrom()` has a latent same-doc assumption (looks up member
  functions via the *target* doc, not the source) that would have silently
  dropped every child function of an imported Show. Cloning goes through each
  object's own `saveXML()`/`loadXML()` round-trip instead (doc-agnostic,
  same proven code path real file Open/Save already uses).
- Not rewritten: function/fixture IDs referenced as literal numbers inside
  Script text (can't safely parse arbitrary JS for this) — scripts import
  verbatim. Audio/Video functions' referenced media files don't travel with
  the import (same as moving a workspace to another machine today).

---

## Done — rebrand the fork to "qlcconsole" *(all 3 phases shipped)*

The fork is now firmly a **desktop console** (mouse+keyboard, MIDI, multi-window),
well past the tablet/Android QML flavour — rebrand from QLC+ to **qlcconsole**.
Distinct from the *Lighting Studio* rename (that was just the 2D tool; shipped).
Keep upstream attribution/license — this is a fork identity, not a takeover.

Phase 1 (2026-08-10, commit 7bf4f6daa) — display strings: window/About titles,
log filename, `.qlcc` workspace extension (`.qxw` still read/imported).

Phase 2 (2026-08-10) — build/binary names: top-level CMake project name and
`CPACK_PACKAGE_NAME` → `qlcconsole`; executable targets `qlcplus` →
`qlcconsole`, `qlcplus-launcher` → `qlcconsole-launcher`,
`qlcplus-fixtureeditor` → `qlcconsole-fixtureeditor` (main/CMakeLists.txt,
launcher/CMakeLists.txt, fixtureeditor/CMakeLists.txt, launcher.cpp's
hardcoded spawn paths, macOS Info.plist CFBundleExecutable, Linux .desktop
Exec= lines, CLAUDE.md/RIG_TEST_PLAN.md/testing_st.md run commands).

Phase 3 (2026-08-12, discovered already-shipped via commit 22fde51d1 + this
pass) — icon/asset filenames (`qlcconsole.icns`, `qlcconsole-fixtureeditor.icns`
etc.) and the macOS `CFBundleIdentifier` (`com.bransonmatheson.qlcconsole`)
were already renamed; only `resources/doxygen/qlcplus.dox` (a docs-generation
config, not app-facing) was still qlcplus-named — renamed to
`qlcconsole.dox` + updated its `PROJECT_NAME` and the `CMakeLists.txt`
doxygen target reference.

UI polish pass (2026-08-11) — main toolbar + global actions (Blackout/Blind/
Operate) + bottom tab bar: repadded/gloss-rendered the 4 mismatched action
icons to match the classic Crystal/Oxygen set's shading (`resources/icons/png/
blackout,blind,operate,design.png`), unified toolbar+tab-bar icon size to
24x24 (`App::initToolBar()`), added a bundled default chrome QSS
(`resources/qss/default.qss`, cascades under the existing user
`~/.qlcconsole/qlcplusStyle.qss` override via `AppUtil::getStyleSheet`).
Deliberately deferred: per-manager toolbar icon-size unification (Virtual
Console 26px / I-O Manager 32px / Show timeline 20px / Monitor 16px all stay
as-is), no dark/light theme switcher *(superseded — see backstage color
themes below, 2026-08-11)*, no SVG-icon migration, Fixture/Function
Manager's own toolbars and the Programming tab's button styling untouched.

Backstage color themes (2026-08-11) — View menu > Theme: Default/Tan/Blue,
picked from `App::Theme` (`ui/src/app.h`), persisted via `workspace/theme`
(`QSettings`), applied live by `App::applyTheme()` (`ui/src/app.cpp`). Whole-
app-surface theming via a `QPalette` swap (`qApp->setPalette()`) rather than
hardcoded per-theme stylesheets — pays off the earlier chrome QSS work
directly, since `resources/qss/default.qss` already reads colors via
`palette(...)` functions, so it follows any theme with zero further changes.
Extended `default.qss` slightly (QGroupBox/QMenuBar/QMenu, still
palette()-only) so more chrome reaches along. Verified all 3 themes
end-to-end via the offscreen snapshot harness (real screenshots, not just
code review) — Default exactly restores the pre-theme look, Tan/Blue both
tint the full window (toolbar, tabs, tables, panels) convincingly. Known
platform caveat, not a bug: this app doesn't force the Fusion widget style
app-wide (only a few `ConsoleChannel` sub-widgets use it), so a handful of
plain native-style buttons/menus elsewhere may follow the palette less
completely than content areas do — full uniform recoloring would mean
switching the app's global QStyle to Fusion, a much bigger, separate
look-and-feel decision, not done here.

Window couldn't be resized smaller / again (2026-08-11) — regression report
led to finding `QTabWidget`/`QStackedWidget` compute minimum size as the max
over ALL pages, not just the visible one, so any one oversized page pins the
whole window (or sub-widget)'s minimum regardless of what's showing. Three
real instances fixed with the same `QSizePolicy::Ignored` + explicit-floor
pattern: `SimpleDesk::initSliderView()`'s unwrapped 32-slider row
(simpledesk.cpp), `LookEditor`'s internal `QStackedWidget` where the pan/tilt
page's 200x200 XY pad bloated every other page (lookeditor.cpp), and
`ProgrammingManager`'s per-fixture console scroll area (proactive — same
pattern, was hidden by default so not yet visibly triggered). Window minimum
width dropped 1296px -> 942px. Not yet investigated: Simple Desk's "Cue
Stack" tab (938px, now the binding constraint) looks like legitimate
toolbar+cue-list content rather than a bug — stopped there per Branson's call.

Per-manager toolbar text/icon label mode (2026-08-11) — the "Icons only /
Text only / Icon+text" workspace setting (`App::applyTabLabelMode()`) only
ever touched the main tab bar and the main window's own toolbar (Panic/
Blackout/Blind/Operate); every per-manager toolbar (Function Manager, Fixture
Manager, Input/Output Manager, Virtual Console) was hardcoded to Qt's
icon-only default, deaf to the setting. Added a `applyToolbarLabelMode()`
method to each (mirrors the existing `Monitor::applyToolbarLabelMode()`
pattern — reads `workspace/tabLabelMode` directly via QSettings), called from
`App::applyTabLabelMode()`. Simple Desk has no comparable QToolBar (its view
controls are standalone QToolButtons), so it's not part of this.

Release readiness (2026-08-11) — v0.1.0 prep landed: `VERSION` file +
`CHANGELOG.md` (SemVer release tags, independent of the git-derived
`APPVERSION` build string); `README.md`/`CONTRIBUTING.md`/`SUPPORT.md`
rewritten with correct qlcconsole branding, fork-of-QLC+ attribution, and
links back to this repo instead of upstream; `RELEASE.md` + `release.sh`
wiring up the existing local build/sign/notarize scripts
(`platforms/macos/package-local.sh`, `sign-notarize.sh`) to tag + publish a
signed macOS DMG as a GitHub Release. Still open before actually cutting
v0.1.0: write the first real `CHANGELOG.md` entry against what's shipped by
then, and run `release.sh` for real.

---

## Now — unify the object editor's fixture management *(design-first; options pending)*

Follow-on to the embedded object-editor canvas (Lighting Studio). Combine the
fixture-properties "edit" dialog (double-click a fixture) and the "Edit Fixtures in
Studio" layout window into ONE object editor. Wants: a left tree of fixtures BY
FIXTURE GROUP as assigned to this object; right-click to add fixtures/groups;
multi-select → create a fixture group (opens the Fixtures-tab head-layout mapping);
drag-drop tree→face to add; distribute/put-on-face via popup or right-click.
**Design options being drafted — pick a direction before building.**

---

---

## Now — more stage-feature objects *(active; design-first)*

Extend the discrete map-object model (truss / platform / target / power source /
image / studio group) with the common rigging structures below. Shared needs:
placeable, movable, lockable, layerable/groupable, XML round-trip, a fixture-host
role (fixtures mount to them like they do to a truss), and a 2D + derived-3D
representation. Likely a small **StageStructure** base + parametric shapes, reusing
the truss geometry funnel (`fixtureRigPosition`) and the bar-on-truss / studio-group
patterns. Write a short design doc first (à la FIXTURESTUDIO_DESIGN.md).

SHIPPED (2026-07-31, see DONE.md) — the unified **Pipe** + **Stand** + **Tower**
object set, all fixture-hosting, 2D + elevation, editors, XML:
- [x] **Boom** = a vertical **Pipe** (on a stand, hung from a truss, or free);
  fixtures mount up the pipe at height + facing angle.
- [x] **House electric** = a horizontal **Pipe** (same object, orientation flag +
  run angle). Booms/electrics are ONE object now (resolved the bar/boom/pipe
  overlap).
- [x] **Stand** = a distinct placeable base; a pipe stands on it (base derived,
  follows the stand). (Trusses/towers on stands = future.)
- [x] **Trusses with booms** — a pipe parented to a truss (drop-arm), base derived.
- [x] **Tower** (16" sq × 8') with **shelves** at heights; fixtures mount on a
  shelf (towerU/V), derived from the tower.
- Demo: `test-workspaces/stage-structures-demo.qxw` (one of each + a fixture on the tower).

Still open (lower priority — non-fixture-hosting scenery): flats, drapes/legs,
set pieces. And: a Stand should also support a truss/tower (only pipes today);
a Tower editor "Cancel" still applies (live-edit); horizontal-pipe fixtures at
per-fixture offsets along the run.

---

## Backlog — not started

### Show lifecycle: Construction / Test-Validate / Production *(2026-08-15, design doc written)*
See `SHOW_LIFECYCLE_DESIGN.md` — names the three phases a show moves through
(building off-rig → matching Lighting Studio to the real rig → running on the
road) and settles that this is **not** a third `Doc::mode()`; it's the
existing Design/Operate axis crossed with a "connected to real hardware vs.
fully simulated" axis (real, non-Dummy plugin patched **and** not Blind —
Blind turned out to already BE the "disable real output" switch: it silences
every protocol at once and is already Design-only/force-off-in-Operate, see
the doc's 2026-08-15 finding). Concrete follow-ons, none started:
1. Compute + expose the rolled-up connected/simulated state
   (patched-and-not-Blind).
2. Surface it as a footer chip alongside MTC/Load/Power.
3. Decide + build whether Design mode should auto-engage Blind when it
   detects real hardware patched, rather than leaving it opt-in.
4. Turn `RIG_TEST_PLAN.md`'s manual checklist into an actual in-app
   Test/Validate workflow (bigger; own design pass on where it lives).
5. Retroactively tag the rest of this backlog by which phase it serves, to
   sharpen prioritization instead of treating it as one flat list.

### ~~Tiny: mark the MTC-chip section label as show-sourced~~ **DONE** (f91c91992, 2026-07-29)
Already shipped — `App::slotTimecodeStatusChanged` prefixes the section label with
`▸` so it reads as show-content, not as text arriving from the MTC stream. This
entry was just stale; confirmed via `git log` (2026-08-19) and removed.
