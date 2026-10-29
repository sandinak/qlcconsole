# Live and blind

What reaches the stage, and what the screens show, in each mode. One page;
2026-10-29, from Branson's notes 4 and 5 of that day. It attaches to what is
there: the Show Tree preview (`ShowTreeController::previewCue`), its RC8 hold
(`takeStageForRun`), the Workbench and the app's Blind button. No new tab,
model or playback path.

## Words

- **Live**: what the stage plays. Shown in the theme's own colours.
- **Blind**: an edit or a view that does not reach the stage. **Always
  blue** (`ui/src/blindstyle.h`, one blue `#1565c0`, white words).

## Design: the latest action wins

| You do | The stage shows |
|---|---|
| Select a cue (or a layer in it) in the Show Tree, or click it again | that cue (the preview, over a running list) |
| The docked Workbench follows the tree | the same |
| Click a cue in the Run tab | that cue; it is also on standby, so GO plays it |
| GO, Back, Sub GO (anywhere) | the run's cue; the preview holds |
| Play beside the held line | the selected cue again, over the run |

A refresh of the same selection (a rebuild, the tab coming back) is not an
action and keeps the hold. Leaving the Show Tree or the Run tab lets go of
what it put up. `ShowTreeController::pickCue()` is "an action";
`previewCue()` stays "a refresh".

Blind in Design is the existing mute: the rig is dark, the screens show the
edit. The footer turns blue and says *"BLIND: the rig is muted, the screens
show the edit"*; the Show Tree's line is blue.

## Operate: only the run is live

- Only the Run (GO and its friends, VC cue lists, show control) reaches the
  stage. Selecting and editing a cue in the Show Tree is blind; it plays as
  edited on its next GO. The tree's line and the docked Workbench's say so,
  in blue.
- The Run tab's click only loads (standby), as before.
- **The Blind button is the Blind view.** Off (the default, and on every
  mode change): the rig views show the stage. On: the plot, the studio and
  every fixture picture show the **cue in edit**, the Show Tree's last
  selected cue (`ShowTreeController::editedCue()`), as levels
  (`cueLevels()`); the stage keeps playing the run and nothing on the wire
  changes. The footer turns blue and names the cue.

## Where it is

- **View ▸ Blind**, moved from the Control menu (the menus keep their
  count), and the Blind button on the top bar (or in the footer).
- No setting: Blind starts off, and every mode change turns it off, so a
  muted rig or a blind view never carries over by accident.

## Decided (can change)

- **The Blind view is the existing Blind button**, not a second toggle on
  each rig view: one control, one meaning ("the screens are not the
  stage"), a menu count kept flat.
- **The cue in edit is the Show Tree's**, kept when the tab is left, so the
  Lighting Studio can show it blind.
- **The Blind view draws levels, not effects**: a cue's scenes and static
  looks; an effect shows its base. Animating effects blind would mean a
  second effect engine.
- **A Run click in Design previews.** It was load-only; in Design it now
  also shows the cue (what you click is what you see; GO plays the same).
- **Build's Workbench in Operate is unchanged**: what it loads plays live,
  as before (a busking aid). Only the Show Tree's cues are blind. If "only
  the run is live" should cover Build too, that is one line in
  `Workbench::startOperate()`.
- **The Run tab's mode chip**: DESIGN in the theme's highlight (it was
  blue); LIVE stays red, the on-air tally.

## History ("where did it go")

- `91a1fb1c6` (2026-07-15) made Blind a Design-only mute with the whole
  footer blue; it has been greyed in Operate since, so Operate had no blind
  colour at all.
- `dbf7b48e1` (2026-08-18) removed the Programming tab's blue in-context
  *"BLIND — rig muted, preview only"* banner and the footer's caption with
  it (the blue footer was thought enough).
- `cc4a510fd` (2026-10-18) gave the Run tab a blue DESIGN chip, so blue
  meant two things.
- `eb131f072` (2026-10-21) lit the Blind button a second blue (`#1f5fd1`).

This change brings the footer's words back, gives Blind a job in Operate,
and makes blue mean blind only, in one shade.

## Tests

`showtree_test::theLatestActionWins`, `aCuesLevelsAreWhatItPutsOnStage`;
`showtreeview_test::theLatestActionWinsAndOperateIsBlind`,
`inDesignTheEditShowsOverItsRunningList`;
`cuerunner_test::aClickInDesignShowsTheCue`;
`mainbar_test::operateClearsBlindAndMakesItAView`.
