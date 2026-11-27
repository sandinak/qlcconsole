# The Timeline and the other big lists

What the Timeline (Show Manager, `ui/src/showmanager/`) still needs to
behave like the Show Tree, Fixtures and Functions lists. Written
2026-11-27 alongside the drop and right-click work on `feat/053-timeline-drop`.
Sizes: S under a day, M a few days, L a week or more. For Branson to rank.

## Where it stands

The Timeline is a `QGraphicsView` (`MultiTrackView`), not an item view, so
it cannot take the list helpers that wrap `QAbstractItemView`
(drag-reorder via `InternalMove`, `setSelectionMode`, filter proxies). What
it can share is behaviour, and that is what is wired:

| Helper | Used by | On the Timeline |
|---|---|---|
| `EditKeys` (`ui/src/editkeys.h`): Delete, F2, Cmd+C/V, Cmd+D, Cmd+F, Esc | Fixtures, Show Tree, Build, I/O | wired: Cmd+D duplicate, F2 rename, Esc clears selection. Delete, Cmd+C/V stay on the toolbar actions it already had |
| Right-click menu built from shared actions | Show Tree (`ContextMenu::Edit`), Fixtures | wired through a hook (`MultiTrackView::setContextMenuHook`) that adds the Show Manager's own toolbar `QAction`s; menu count did not grow |
| Empty-state line | Show Tree, Build | wired: "No tracks: drop audio here or right-click > Add Track" |
| Undo | Show Tree (tree steps), Build (`PaletteOps`) | the Timeline's own snapshot stack (`pushUndoSnapshot`), 25 deep; every new edit pushes one step |
| Drag-reorder | Functions list, Fixtures | not shared: tracks reorder by Move Up / Down and by dragging a header (own code in `TrackItem`) |

Not refactored here: the Timeline's snapshot undo and the Show Tree's step
history are separate systems (one whole-show copy per step versus a step
per edit). Merging them is the "lane per cue list" item below.

## What it still needs

### 1. Keyboard-only workflow (M)
- Sees: Tab into the timeline, arrows move between tracks and items,
  Return opens, F2 renames, Delete deletes, no mouse needed to build a
  show from audio.
- Reuse: `EditKeys` for the verbs; the Show Tree's `keyPressEvent` for the
  arrow model. The Timeline has no current-item cursor, only a selection.
- Why first: today only the playhead and the item drag work from the
  keyboard, which is also the findability bar for students.

### 2. Multi-select edits (S)
- Sees: rubber-band or Shift-click several items, then Duplicate, Move to
  Track, Delete, Lock act on all of them, and Cmd+C/V already copy.
- Reuse: `selectedItems()` exists (Delete already uses it with a
  confirmation); the new right-click verbs take one item. Make them take the
  list. Undo is one step for the batch (already how Delete does it).

### 3. Colour per track (S)
- Sees: a swatch on the header, colour picker in the right-click menu, the
  track's items take it.
- Reuse: `Track::color()` and "Track Colour..." exist and are in the menu;
  items use their own `ShowFunction::color`. Missing: the header swatch
  and "colour the track's items too". Fixtures already colours groups.

### 4. Markers and song sections, Q190 cue numbering (M)
- Sees: the marker lane gets a number per section from the numbering
  scheme setting (`{S}.{P}.{II}`), shown on the marker and in the status
  line; dropping audio can name sections from the song.
- Reuse: markers exist (`ShowMarker`, `Show::markers`); the numbering
  scheme lives with the Show Tree's cue numbers (`treecuestack.*`,
  `showtreesections.cpp`). Needs a shared formatter, not a copy.

### 5. Filter and search (M)
- Sees: a "Filter tracks" field above the sidebar (and Cmd+F focuses it)
  that dims tracks and items that do not match a name, a function type or a
  colour.
- Reuse: Show Tree Library filter (`m_filter`, `focusLibraryFilter`),
  Fixtures' `m_fixturesSearch`. Both are `QLineEdit` plus a match function;
  the Timeline needs only the match to hide `TrackItem` rows. Hiding rows
  must keep drop rows (row index to track) honest: dim, do not remove.

### 6. Track groups or folders (L)
- Sees: collapse "Lights" or "Audio" as a folder; moving the folder moves
  its tracks; solo/mute on the folder.
- Reuse: Functions list folders and `FixtureGroup::path` give the model
  idea (a path string per member). The Timeline's row index assumes a flat
  list in `MultiTrackView` (drop row, header position), so folders are a
  rewrite of row layout, and the `.qxw` Track needs a `Group` attribute.

### 7. A lane per Show Tree cue list (L)
- Sees: each Show Tree cue list as a read-only (then editable) lane on
  the Timeline, so the song's cues and the audio sit on one ruler.
- Reuse: `TreeCueStack` is the model (cues are tree steps, not
  `ShowFunction`s), and the Show Tree already shows section marks
  (`showtreenotemarks.cpp`). This crosses the "no new playback path" guardrail
  in STRATEGY.md, so it needs an explicit yes before any build.

## Ranked "next"

1. Multi-select edits (S): the right-click verbs already exist for one item.
2. Colour per track (S): the data is there, only the header swatch is new.
3. Keyboard-only workflow (M): biggest ease-of-use gain, no model change.
4. Markers and sections with Q190 numbering (M): needs the shared formatter.
5. Filter and search (M).
6. Track groups or folders (L).
7. A lane per Show Tree cue list (L, needs a guardrail yes).

## Also open from this branch

- Rename of an item renames the Function, which is not a timeline undo
  step. Mute, Solo and Lock on a track are not either (as with their
  header buttons).
- Dropping files is wired in the Show Manager tab only. The Timeline
  embedded in Build (`ShowTimelineEditor`) does not accept file drops yet;
  its view leaves them off on purpose rather than accepting and doing
  nothing.
- Cmd+D is Add Video anywhere except the focused timeline with a
  selection; the Add Video shortcut could move to free it.
