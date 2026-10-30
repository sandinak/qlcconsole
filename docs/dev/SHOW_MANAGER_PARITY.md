# Show Manager parity: qlcconsole vs QLC+ 4.14.4

A review of the **Shows** tab (Show Manager: the multitrack timeline) for a
QLC+ 4 user, against stock `QLC+_4.14.4` (027e6d4fd). Asked for by
[STRATEGY.md](../project/STRATEGY.md) section 3.1 ("Show Manager parity
review") before Classic is called a promise. Reviewed 2026-10-29 from the
code of both (fork `ui/src/showmanager/`, `engine/src/show*.cpp`,
`track.cpp`, `showrunner.cpp`; upstream by `git show QLC+_4.14.4:<path>`).

`SM` = `ui/src/showmanager/showmanager.cpp`, `MTV` =
`ui/src/showmanager/multitrackview.cpp`. Line numbers are as of this review.

The Shows tab is in the **Classic** and **Everything** views; a stock .qxw
with no Show Tree opens in Classic, so a QLC+ 4 user does meet it.

## Summary

- **Every stock feature is still there.** No toolbar action, shortcut,
  item type, track operation or file field is gone. Most actions moved
  one click deeper (into **Show ▾** and **Edit ▾** menu buttons, Snap on a
  second row, Stop under Play's drop-down).
- **The file format is additive**: Track and Show gain attributes
  (Locked, Color, Solo, SoloSafe, HoldLast, Priority, Intensity;
  FollowTimecode, TimecodeOffset, Duration, `<Marker>`). A stock file
  loads with every default as it was; stock QLC+ skips the extra tags.
- **The main way a stock show plays differently** is chasers on the
  timeline (G1 below): the fork drives every chaser block from the show
  clock, which does not honour loop, ping-pong, random, backward, beat
  tempo or infinite holds.
- **The one stock workflow that's gone** is editing on the timeline:
  clicking a sequence no longer opens the scene and chaser editors below
  and beside it (G2), so a new sequence (⌘E) is an empty block you fill
  in the Functions tab.
- Five small bugs were fixed in this review (section "Fixed").

## Feature by feature

| Upstream 4.14.4 | qlcconsole | |
|---|---|---|
| New show (Ctrl+H), shows combo (sorted) | In the **Show ▾** menu, with Rename… and Delete… added | moved, added |
| Add track / existing function (Ctrl+N) | In **Edit ▾**; also offers Collections; "New track" on a header right-click; drop on empty space or below the last track | moved, added |
| New sequence (Ctrl+E), audio (Ctrl+A), video (Ctrl+D) | In **Edit ▾**; same overlap checks and file dialogs. New sequence opens no editor (G2) | moved, **changed** |
| Copy (Ctrl+C), Paste (Ctrl+V), Delete (Del) | In **Edit ▾**; multi-select; Delete undoable, asks only for several items; with nothing selected Delete does nothing (stock deleted the active track, asking) | moved, changed (G7) |
| Colour (Ctrl+L), Lock (Ctrl+K), Timings tool (Ctrl+T) | In **Edit ▾**; same dialogs. The timings tool on a chaser block stretches the last cue only; stock scaled every step (G5) | moved, changed |
| Snap to grid (Ctrl+G) | Second toolbar row | moved |
| Time label, bold `hh:mm:ss.cc` | Same | same |
| Play/Pause (Space), Stop (Ctrl+Space): two buttons | One split button; Stop in its drop-down; both hidden while following MTC | changed |
| Time division (Time, BPM 4/4, 3/4, 2/4), BPM 10-240 | Same values; hidden while following MTC | same |
| Zoom slider (1-15), Ctrl+wheel | Same; the slider's corner stays put while scrolling | same |
| Track solo / mute buttons (S then M) | **M then S**; solo is its own flag (stock wrote mute onto the other tracks) | changed (G8) |
| Track rename (dialog), move up/down, delete (menu or Del) | Dialog or in place; move by menu or drag; delete from the header menu (undoable) | changed |
| Items: sequence, scene, chaser, audio, video, EFX, RGB matrix | Same, plus Collections and bare scene clips | same, added |
| Item drag, align to cursor, lock (item menu) | Same, plus cross-track moves, multi-select, push-aside, edge resize | same, added |
| Click a sequence: scene editor below, chaser editor right; click audio/video/EFX/RGB/chaser: its editor right | **Removed** (SM:2406-2410); the panes stay hidden; edit in Functions | **missing** (G2) |
| Click empty timeline to move the playhead | Restored in this review (was taken over by the marquee) | fixed |
| Names: "New Show <id>", "Track <n>", "New Sequence <id>", " (Copy)" | Same | same |
| Save/load XML | Additive (above) | same |

What qlcconsole adds, briefly: a function tree to drag from; "Add
function here…"; bare scene and Collection clips; marquee and Shift
select; push-aside and edge resize; cue editing inside a chase block
(roll, slip, fades, group move, scale); whole-timeline Undo; show rename
and delete; track lock, colour, intensity fader, solo-safe, hold-last,
priority; section markers with a linked cue list; a show length with an
END handle; MIDI timecode follow; Virtual Console takeover in Operate; a
held last look at the end in Operate; a 60 Hz playhead; the master show
lock.

## Fixed in this review

Each with a test that fails without it.

| | What | Fix | Test |
|---|---|---|---|
| F1 | A plain click on empty track area no longer moved the playhead: the marquee took every press (MTV mouseRelease) | A band under 3 px is a click: move the playhead there | `EndHandle_Test::aClickOnEmptyTrackAreaMovesThePlayhead` |
| F2 | Undo history leaked across shows: New Show or a refilled combo kept the old show's snapshots, and Undo put show A's tracks into show B | The stack belongs to one show id; another show (or a new document) clears it | `ShowManager_Test::undoStartsFreshForAnotherShowOrDocument` |
| F3 | A function deleted in Functions or Build left its ShowItem in `MultiTrackView::m_items` after its ShowFunction was freed; zoom and the next drop on that track (push-aside) read freed memory | `deleteShowItem` takes the item out of the list and deletes it | `ShowManager_Test::aDeletedFunctionLeavesNoItemBehind` |
| F4 | A plain click on a chase block (any click on a one-cue block) could move it by up to a pixel's worth of time, mark the show modified and push neighbours aside: several press paths skipped setting the press position | `SequenceItem::mousePressEvent` records it first | `ShowManager_Test::aClickOnAChaseBlockDoesntMoveIt` |
| F5 | Renaming a track never renamed its intensity attribute ("<name>-<id>"; stock bug too, `getAttributeIndex(newName)` is always -1), so a VC slider showed the old name and removing the track left the attribute behind; the rename dialog didn't mark the show modified | `Show::trackRenamed()`, called from the dialog, the in-place edit and the Show Tree's timeline editor | `ShowManager_Test::renamingATrackRenamesItsAttribute` |

## Gaps for the queue

Effort: trivial (< 30 lines), small (30-100), medium, large.

| | Gap | Effort | Notes |
|---|---|---|---|
| G1 | **Chasers on a show play differently from 4.14.4.** `ShowRunner::startChild` puts every chaser under the show clock (`showrunner.cpp:362-364`); `Chaser::stepIndexAtLocalMs` walks steps forward once, then holds the last. Beat-tempo chasers freeze on step 1; Loop and Ping-pong stop after one pass; Random and Backward play forward; single-shot keeps its last step up to the block's end; infinite holds advance every 3 s. Sequences take the same path. | trivial to stop it (show-clock only Time + SingleShot + Forward chasers; the rest self-advance as in 4.14.4), medium to honour every mode on the show clock | **Decision for the user:** the show clock is what makes scrubbing and cue roll line up. The trivial fix gives back stock playback for those chasers but they no longer follow a scrub. A hot path: needs `make perf` numbers. |
| G2 | **No editors on the timeline**: clicking an item doesn't open the scene/chaser/EFX/RGB/audio/video editor; a new sequence (⌘E) is an empty block. `showRightEditor()` and `slotStepSelectionChanged` are dead code. | small: an **Edit…** item-menu action / double-click (and after ⌘E) that calls the existing `showSceneEditor` + `showRightEditor` | Per-click editors were removed for speed; an explicit action doesn't bring that back. |
| G3 | **Undo of a drag-move or resize does nothing**: the snapshot is taken after the model changed (MTV:1968-2061, SM:2396), so Ctrl+Z restores the current state and uses up the step. A drop below the last track takes no snapshot. | trivial-small: an `itemAboutToChange()` signal before the change, connected to `pushUndoSnapshot()` | |
| G4 | **The rest of the press-without-drag path in a chase block's fade band** switches the chaser's hold and fade modes to per-step on a press (`sequenceitem.cpp:599-600`), without copying a Default-mode step's own fades: a speed dial stops affecting it, and scenes that faded with their own times can snap. | small | Move `ensurePerStep*` to the first real drag; copy fades for Default mode. |
| G5 | Timings tool on a chaser block stretches only the last cue; stock scaled every step (`sequenceitem.cpp:296-320`) | small | Scale all from the timings tool, keep last-cue for a drag. |
| G6 | **Master show lock only partly enforced**: New sequence/audio/video, Paste, Colour, Lock, the timings tool, track rename/reorder, markers and show length still work while locked. | trivial-small (one guard per slot + disable the actions) | |
| G7 | Delete with nothing selected does nothing (stock: delete the active track, after asking); a single item is deleted without asking (undoable) | trivial | A deliberate change; keep unless 4.x users ask. |
| G8 | Solo and mute buttons swapped (M then S); stock had S then M | trivial | A deliberate change; keep unless 4.x users ask. |
| G9 | Two clips of one function back to back: the second is lost (start runs before stop in the same tick; stock has the same bug, but push-aside now makes exact back-to-back clips routine) | small | |
| G10 | Mute, solo, intensity and priority are found by function id (`trackForFunction`, `showrunner.cpp:327-340`), so one function on two tracks takes the first track's settings; stock matched the ShowFunction | medium | |
| G11 | A Sequence dropped or moved onto a track bound to another scene is **deleted on reload** (`Track::postLoad`, `track.cpp:461`), without a warning | small | Bind an unbound track, refuse or make a new track otherwise. |
| G12 | Paste, New sequence/audio/video, Colour, Lock and the track flags take no Undo snapshot | small | |
| G13 | Muting a track doesn't mark the show modified | trivial | |
| G14 | Dead code: `ShowManager::slotChangeSize` (dead upstream too), `MultiTrackView::deleteSelectedItem` | trivial | With G2, `showRightEditor` comes back to life. |

Deliberate differences, not gaps: leaving the Shows tab stops a show
playing in Design (SM:2969); in Operate the last look stays up when a
show ends (**Control ▸ Hold at End**, on by default).

Shortcuts: all stock shortcuts are still set on actions that now sit in
tool-button menus; they should still fire (the menu's actions belong to
the button), not checked at runtime. Ctrl+N and Ctrl+D clash with File ▸
New and Dump DMX, as they do in stock.
