# Consolidation review: one way to do each thing

2026-10-09. The tool works for the show; now the gaps are not missing features
but the same job done different ways in different places. Each costs a moment
of "wait, how does it work *here*?" mid-programming. This lists them, most
costly to writing a show first, with the one approach to converge on.

Earlier reviews: WORKFLOW-SUBTLETIES.md is fully resolved; WORKFLOW_UX_REVIEW.md
and VISUAL_CONSISTENCY_REVIEW.md items still open are folded in below.

---

## Tier 1 -- you will hit these writing every scene

### 1. Scene timing is split across tabs -- DONE 2026-10-09
- The scene's own fade in/out is only in the stock Scene Editor's speed dial
  window (Function Manager), not in the Programming tab
  (sceneeditor.cpp:1139; programmingmanager.cpp:867-890 hosts every other
  editor but not the scene's).
- A look's "step" fade means *use the scene fade* -- which can't be seen or
  set where looks are built (scenegrouplooks.cpp:62, lookeditor.cpp:271).
- Transitions (new) sit on the canvas; their time replaces the scene fade.

**Done:** the canvas header row: Fade in / out, Transition in / out + time,
Edge; a scene fade is greyed while a transition stands in for it.

**Converge:** the scene canvas header carries the scene's whole timing:
`Fade in [ ] Fade out [ ]  In: [shape][time]  Out: [shape][time]  Edge [ ]`,
the transition time standing in for the fade when a shape is set.

### 2. Four different time controls -- DONE 2026-10-09
| Where | Range | Shown as | "Use default" |
|---|---|---|---|
| Look fade (canvas + look editor) | 0-600 s, 2 dp | "1.5 s" | "step" |
| Transition | 0.1-120 s, 1 dp | "1.5 s" | -- |
| Chaser step | speed dial | "1s500ms" | -- |
| Scene fade | speed dial window | dial | -- |

And for an Effect look the canvas edits its Fade Out (the release) while the
look editor hides the fade row (scenegrouplooks.cpp:816-835 vs
lookeditor.cpp:924).

**Done:** `FadeTimeSpin` (ui/src/fadetimespin.h) on the canvas, the header
and the look editor; `LookFades` is the one rule (an Effect: release only,
default "snap"; a colour feeding an effect: none; else both, default "scene").
Chaser steps keep the speed dial for now.

**Converge:** one `FadeTimeSpin` (seconds, 2 dp, a "default" value where one
applies) everywhere; same Effect rule in both editors. Beats stays an engine
option until a widget offers it.

### 3. Double-click on a fixture opens different things -- DONE 2026-10-09
- Plot: a fixture on a pipe/tower/riser/deck opens the *structure*; on a
  truss, drills in, then the fixture (monitorgraphicsview.cpp:5895-5918).
- Studio tree/canvas: Fixture Properties (monitor.cpp:3742;
  structurestudioview.cpp:7593).
- Fixture Manager: the stock QLC+ edit dialog -- a different dialog
  (fixturemanager.cpp:494, 2026).

**Done:** plot, studio and Fixture Manager all open Fixture Properties
(its "Definition & Mode…" button reaches the stock dialog's part). A plot
group / truss member: click it again to pick it out; "Edit <structure>…" in
the fixture's menu.

**Converge:** double-click a fixture = Fixture Properties, everywhere. A
structure is opened by double-clicking the structure (or "Edit <structure>"
in the fixture's menu).

### 4. One fixture, three property editors
| | Studio inspector | Fixture Properties | Fixture Manager |
|---|---|---|---|
| Colour (gel) | hex button, no reset, all selected | pixmap + Reset, one fixture | -- |
| DMX address | editable + Patch | read-only | stock dialog |
| Position | -- | "Horizontal/Vertical", 3 dp, "ft" | -- |
| Applies | at once | on OK | on OK |

**Converge:** one property model. The inspector is the dialog's quick subset,
same widgets and labels ("Colour", "X / Y", editable address + Patch…),
applying at once with undo; the dialog is "everything", applying live with
Cancel restoring. The Light/Glass rows already work this way (LightControls,
shared) -- the pattern to copy.

### 5. Problems turn up in different places -- DONE 2026-10-09
- The readiness footer (persistent, with fixes) covers fixtures, patch,
  outputs, definitions, cloud storage.
- The load-time "Some errors occurred" dialog (once, then gone) covers
  out-of-range addresses, missing video files, unsupported sequences -- and
  repeats missing definitions the footer already shows.
- Effect scripts that fail to load, and effect looks whose palette is gone,
  only reach the log: the look silently does nothing.

**Done:** the load dialog is gone -- its lines are the footer's "show.load"
(missing definitions left to fixtures.nodefinition); "effects.failed" lists
effect looks in scenes with no effect chosen, a script not on this computer,
or one that would not start (EffectScriptRunner::failures(), with the
script's reason).

**Converge:** everything goes to the footer, with a fix where there is one;
the load dialog becomes "N problems -- see the footer". Add `effects.failed`.

---

## Tier 2 -- you will hit these setting up and changing the rig

### 6. Structure dialogs behave three ways -- DONE 2026-10-09 (step 1 of 4+6)
- Live preview + Cancel restores: stand, scenery, tower, pipe, platform, truss.
- Apply on OK only: hanger, image, stage target.
- Fixture Properties: only orientation previews.
- Feet-and-inches entry only in the platform dialog (FeetInchesSpinBox);
  elsewhere decimal ft (2 dp, " ft"); Fixture Properties 3 dp, "ft".
- Colour pickers: swatch button / hex-text button / pixmap+Reset / bare
  dialog; labels "Color:" and "Colour:".
- Undo: plot Ctrl+Z (moves, paste); the studio's own stack (rig props
  only -- not gel, address, bar position); dialogs none.

**Done:** `RigEditSession` (ui/src/monitor/rigeditsession.h). Every rig
dialog -- truss, pipe, platform, tower, stand, scenery, group, hanger, image,
target, Fixture Properties -- previews live; Cancel restores the whole rig
from a snapshot (MonitorProperties::snapshot()/restoreSnapshot(), which keeps
every unchanged structure the same object and retires, not frees, the rest
while the DMX thread may hold them); OK is one step on the plot's Undo (now
also a toolbar button). Hanger, image and target went from apply-on-OK to
live. `FeetInchesSpinBox` for every length (follows the show's grid units:
feet-and-inches, or metres); `ColourSwatchButton` (swatch, hex, Reset) for
target, cloth, faces, platform, sock and gel; "Colour" throughout the rig.
Left: the network-node dialogs edit the network topology, outside the rig
snapshot (they still apply on OK); a dialog's studio pane keeps its own
Ctrl+Z while it is open; the studio inspector's widgets are step 2.

**Converge:** one `editInDialog(snapshot, applyLive, revert)` helper (every
dialog previews live, Cancel restores, OK pushes one undo step onto the plot's
stack); `FeetInchesSpinBox` for every length; one `ColourSwatchButton` (swatch,
hex, Reset); "Colour" throughout.

### 7. Search boxes match differently
| Box | Placeholder | Parent kept? | On clear |
|---|---|---|---|
| Programming nav + palettes | "Filter…" | no | collapses |
| Fixtures & groups | "Search fixtures & groups…" | yes | collapses |
| Layers | "Search the tree…" | expands matches | stays open |
| Fixture Manager | "Search fixtures..." | all columns | stays |
| Inventory | "Filter" | name/maker/model | -- |
| Effect browser | "Search effects…" | all tabs | -- |
| Add Fixture | (label "Quick search") | maker or model | -- |
| Function Manager | none | -- | -- |

**Converge:** one `TreeFilter` (trimmed, any case; a match keeps what it
contains and opens the way down; clearing restores what was open) and one
placeholder form, "Search <things>…". Add one to the Function Manager.

### 8. "Add fixture" means three things
Fixture Manager: create one from a definition. Plot "Add Fixture": place an
existing one. Studio: a plain unfiltered list, "Add Fixtures to Structure".

**Converge:** create = "Add Fixture…" (Fixture Manager, inventory); place =
"Place Fixture…" with one filterable picker in the plot and the studio.

### 9. Same action, different names
"Properties…" / "Edit Fixture…" / "Edit fixture…" / "Full properties…";
"Re-address…" vs "Patch…" -- with different mode rules (Fixture Manager needs
Design mode; the plot allows unpatched or multi-select in any mode).

**Converge:** one fixture context-menu factory: "Fixture Properties…",
"Patch…", one rule for when Patch is offered.

---

## Tier 3 -- smaller, but they add up

10. **Duplicates are named three ways** -- functions smart-increment
    (nextDuplicateName), palettes "(Copy)" in Function Manager and "(copy)"
    in Programming. Use nextDuplicateName for everything (it already learns
    the show's SS-PP.II naming).
11. **Rename gestures** -- Function Manager: slow click or F2; Layers: double-
    click a layer, Enter elsewhere; Fixtures & groups: context menu. F2/Enter
    renames everywhere; double-click always opens.
12. **Three effect pickers on one page** -- the hidden combo (source of truth,
    fine), the old Browse… dialog, and the browser. Drop Browse….
13. **Timeline** -- only scene clips open on double-click; EFX, collection,
    RGB matrix, sequence, audio and video clips have no edit gesture.
14. **Cloud storage reaches files the startup skip doesn't** -- printed
    patterns, gobo images and head layouts are read when first used; one
    that is online-only would stall that moment. Run them through the same
    check (QLCFile::isOnlineOnly) and the show cache.
15. **Haze is only on the Rig Overview** -- the structure editors' Angled
    views draw beams and glow but have no Haze control (and no Room
    ambience). One view-settings strip, the same in every angled view.
16. Leftovers from VISUAL_CONSISTENCY_REVIEW.md not re-verified: expand/
    collapse vs add/remove icons, three delete icons, one semantic red.

---

## Suggested order

Before writing the show in earnest: **1, 2, 3, 5** (timing in one place, one
time control, one double-click meaning, problems in one place) -- they touch
every scene. Then **4 and 6** together (one property model, one dialog
helper) -- the biggest code change, best done as one piece. Then 7-9 (search,
add/place, menu names), and Tier 3 as they come up.
