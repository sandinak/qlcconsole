# UX rules

What Branson expects of qlcconsole's screens, written down so a test, the
screen walker or a reviewer catches it before he does. He put it as "most
things are reasonably obvious"; these are the obvious things, each one
learned from a catch. Every rule comes from his own words: a memory note,
an answered board question (Q-numbers) or a catch on the day it is dated.
None is invented here.

The machine-readable copy is [ux_rules.json](ux_rules.json): the same IDs,
rules and checks, plus `patterns` (a `forbid_regex` over text a person
reads, or a `require` the walker asserts on live widgets) for the rules
that have one. Change both files together.

## How to read a rule

- **ID** `UX-NN`, stable once given; a retired rule keeps its number and is
  marked retired, never reused.
- **The rule**, one sentence, in his terms.
- **Why:** the catch that taught it, with the date (and the board question
  or commit where there is one).
- **Check:** what catches it, first the main check:
  - a test by name (`units_test`, `menuwords_test::everyButtonLabelIsTitleCase`)
    or a script (`help-shots.sh`), already in the gate;
  - `walker`: the upcoming screen walker, which opens every screen offscreen
    and asserts the rule on live widgets;
  - `review`: the AI reviewer reads the diff against the rule;
  - `manual`: a person has to look (shots, his real rig).
- **enforced** means a gate test guards the catch that taught the rule.
  It does not mean every screen is covered: where the test only guards one
  place (the effect picker, the power chip), `walker` is listed to cover the
  rest.
- **Right / Wrong:** one example of each, as the person would see it.

## Summary

| Theme | Rules | Count | Enforced |
|---|---|---|---|
| Units & numbers | UX-01, UX-02, UX-03, UX-04, UX-05, UX-06 | 6 | 5 |
| Words | UX-07, UX-08, UX-09, UX-10, UX-11, UX-12, UX-13, UX-14, UX-15, UX-16, UX-17, UX-18, UX-19 | 13 | 7 |
| Context-awareness | UX-20, UX-21, UX-22, UX-23, UX-24, UX-25 | 6 | 5 |
| Feedback & state | UX-26, UX-27, UX-28, UX-29, UX-30, UX-31, UX-32, UX-33 | 8 | 6 |
| Layout | UX-34, UX-35, UX-36, UX-37, UX-38, UX-39, UX-40 | 7 | 5 |
| Consistency across views | UX-41, UX-42, UX-43, UX-44, UX-45, UX-46, UX-47, UX-48 | 8 | 6 |
| Safety in Operate | UX-49, UX-50, UX-51, UX-52, UX-53, UX-54, UX-55, UX-56 | 8 | 6 |
| Teaching & course voice | UX-57, UX-58, UX-59, UX-60 | 4 | 1 |

Main check: 41 tests, 13 review, 3 walker, 3 manual. 41 of 60 enforced.

## Units & numbers

### UX-01 Every length follows Settings ▸ Units

Every length a person reads or types (field, label, tooltip, menu, report, CSV, web page, voice) is in the unit chosen in Settings ▸ Units, feet and inches by default.

- **Why:** 2026-11-06: Set Mounting Height… asked "Height above the floor (metres):" on a feet show, after the same field was fixed on 2026-10-27 (Q174). About 55 call sites keyed off the grid unit; he: "we keep finding places where it exists".
- **Check:** `units_test::noUnitWrittenIntoAString`, `units_test::textFollowsThePreference`, `units_test::dialogsFollowThePreference`, `menuwords_test::noLengthIsMetresOnly`, `walker` (enforced)
- **Right:** `Height above the floor: 12' 6"`
- **Wrong:** `Height above the floor (metres): 3.81`

### UX-02 A length field is a length field

A length is typed into a LengthSpinBox or MeasurementEdit (LengthInputDialog for a prompt) that takes any unit, never a bare number box.

- **Why:** 2026-11-06: the Mounting Height prompt was a QInputDialog::getDouble labelled in metres; a number box can't say its unit or take 12' 6".
- **Check:** `units_test::noBareNumberDialogForALength` (enforced)
- **Right:** `A Trim field that takes 18' 0", 18ft or 5.5 m and shows 18' 0"`
- **Wrong:** `QInputDialog::getDouble(this, tr("Trim"), tr("Height (m)"), ...)`

### UX-03 The grid's unit is only for the grid

The show grid's unit sets grid spacing only; nothing picks a display unit from it.

- **Why:** 2026-11-06: the root cause of the metres-on-a-feet-show class of bugs was MonitorProperties::gridUnits() used to choose the display unit.
- **Check:** `units_test::gridUnitsOnlyForTheGrid`, `appviews_test::theGridFollowsTheUnitPreference` (enforced)
- **Right:** `MeasurementUnits::formatLength(mm) in a label`
- **Wrong:** `gridUnits() == Meters ? "%1 m" : "%1 ft"`

### UX-04 Feet and inches read like a tape measure

Imperial lengths show feet and inches to the nearest 1/8", the fraction reduced, and the same forms are accepted when typed.

- **Why:** 2026-11-06 (5eb8b86c7): decimals of a foot are not how a crew measures or calls a trim.
- **Check:** `measurementunits_test`, `units_test::feetAndInchesByDefault` (enforced)
- **Right:** `3' 11 1/4"  (typed as 3' 11 1/4", 11-1/4" or 3/8")`
- **Wrong:** `3.9375 ft  or  3' 11.25"`

### UX-05 Values in the user's terms

A setting shows its meaning, not its storage: an option's name, a switch's sense, "per beat" on a speed master, one cycle's seconds for a wave, lengths through Units.

- **Why:** 2026-11-06 (b3784b1b6): an effect in the tree showed raw script parameters; the effect rows now read like the Aim row.
- **Check:** `showtreeview_test::anEffectShowsWhatItRunsOnAndWhereEachValueComesFrom`, `review` (enforced)
- **Right:** `Direction: Reverse · Speed: 2 per beat`
- **Wrong:** `direction: 1 · speed: 0.5`

### UX-06 A typed value stays when you click away

A typed fade, hold or value is committed when focus leaves the field, not only on Return.

- **Why:** 2026-10-23 (Q60, quick win 2/14): typed times were lost on a click into the tree.
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `Type 2.5 in Fade, click the tree: Fade is 2.5`
- **Wrong:** `Type 2.5 in Fade, click the tree: Fade is back to 0`

## Words

### UX-07 Menu items are Title Case

Every menu item is Title Case: every word capitalised except a, an, the, and, or, of, to, in, on, at, by, for, from, with, as, vs, into after the first.

- **Why:** 2026-11-02/03 menu canon (docs/design/CONTEXT_MENUS.md §3.2): the same action was worded three ways in three menus.
- **Check:** `menuwords_test::everyMenuLabelIsTitleCase`, `contextmenus_test::titleCase`, `walker` (enforced)
- **Right:** `Delete Fixture Group…`
- **Wrong:** `Delete fixture group…`

### UX-08 Buttons are Title Case too

Every push or tool button text is Title Case like a menu item (dialog-box answers such as "Not now" excepted).

- **Why:** 2026-11-06: "Show all effects" on the effect picker; train 1106b failed on it.
- **Check:** `menuwords_test::everyButtonLabelIsTitleCase`, `walker` (enforced)
- **Right:** `Show All Effects`
- **Wrong:** `Show all effects`

### UX-09 "…" means a dialog follows

A label ends in the single character "…" exactly when a dialog (including a confirmation) follows; never three dots; an inline rename has none.

- **Why:** 2026-11-02 menu canon §3.2.
- **Check:** `menuwords_test::everyMenuLabelIsTitleCase`, `walker` (enforced)
- **Right:** `Patch…  ·  Rename (inline)`
- **Wrong:** `Patch...  ·  Rename… (inline)`

### UX-10 No shortcut typed into a label

A shortcut is set on the action so Qt shows it in its own column; it is never part of the text.

- **Why:** 2026-11-02 menu canon §3.2.
- **Check:** `menuwords_test::everyMenuLabelIsTitleCase` (enforced)
- **Right:** `Select All     ⌘A`
- **Wrong:** `Select All (Ctrl+A)`

### UX-11 Delete destroys, Remove takes out

"Delete X…" destroys the thing; "Remove from Y" takes it out of a container and leaves it in the show. A bare "Remove" is never a destroy.

- **Why:** 2026-11-02 menu canon §3.2: "Remove" meant both in different menus.
- **Check:** `contextmenus_test::theCanonCatchesDrift`, `review` (not yet enforced)
- **Right:** `Remove from Truss  ·  Delete Fixture Group…`
- **Wrong:** `Remove (an item that deletes the fixture group)`

### UX-12 One word per idea

Each idea has one name everywhere: Hits (the busk section), Plot symbol, Operate, Hold at end, Quick Focus / Venue Focus / Focus This Cue.

- **Why:** 2026-10-23 (Q70, Q77) and 2026-10-25 (Q149): "look", "focus" and "stage look" each meant several things.
- **Check:** `review`, `walker` (not yet enforced)
- **Right:** `Plot symbol · Hits · Focus This Cue`
- **Wrong:** `Stage Look · Looks (the busk section) · Refine`

### UX-13 Release and Untrack

Ending a tracked layer is Release (plays in this cue, then lets go) or Untrack (never reaches this cue), the same words in menus, tooltips, the tracking column, help, course, command line and voice.

- **Why:** 2026-11-04 (Q222): he turned off the USTs' tracking and expected them gone from the cue he was on; the words had to say which.
- **Check:** `showtreeview_test::releaseAndUntrackOnTheGreyRow`, `review` (enforced)
- **Right:** `Release After This Cue  ·  Untrack From This Cue`
- **Wrong:** `Drop From This Cue  ·  Stop Tracking  ·  Block`

### UX-14 "Classic" means stock QLC+

Anything labelled Classic is stock QLC+ 4 only; qlcconsole's own tools never wear the word.

- **Why:** 2026-10-23 (Q73): "Build ▸ Scenes ▸ Classic" was not QLC+'s.
- **Check:** `appviews_test::classicLooksLikeQlcPlus`, `helpbrowser_test::noGuideSendsYouToScenesClassic` (enforced)
- **Right:** `View ▸ Workspace ▸ Classic QLC+ (the stock tabs and toolbars)`
- **Wrong:** `Build ▸ Scenes ▸ Classic`

### UX-15 A jump names where it goes

A link, Go to or Open item names its destination in the user's terms.

- **Why:** 2026-11-06 (b7cee01ad): a grey row's "Go to" didn't say which cue.
- **Check:** `review`, `showtreeview_test` (not yet enforced)
- **Right:** `Go to 1.1.1 Setup`
- **Wrong:** `Go to origin`

### UX-16 Keys and buttons are named as on the device in hand

Shortcuts read ⌘/⌥/⇧ on macOS and Ctrl/Alt/Shift elsewhere; a game pad's buttons are named as printed on that pad (Cross, Circle, Options on PlayStation; A, B, Menu on Xbox), in text and voice.

- **Why:** 2026-10-27 (Q170): about 45 hard-coded Xbox strings told a DualShock user to press A. 2026-10-15 (270e59ecc): help on Windows told people to press ⌘.
- **Check:** `focusmode_test::aPlayStationPadIsCalledByItsOwnNames`, `review`, `helpbrowser_test::keysAreSpelledForTheComputerYouAreOn` (enforced)
- **Right:** `⌘D on a Mac · "Press ✕ to keep" on a DualSense`
- **Wrong:** `Ctrl+D in the Mac help · "Press A to keep" on a DualSense`

### UX-17 Plain sentences, not engine terms

Explanations, warnings and errors say what happened in the operator's words and what to do next; engine terms (HTP, fader priority, function id) stay out.

- **Why:** 2026-10-27 (Q181, Why is this on?) and the teaching shows: the answer has to be readable by a student on headset.
- **Check:** `whyonpanel_test::plainSentencesPerGroup`, `whyonpanel_test::plainWordsInATeachingShow`, `review` (not yet enforced)
- **Right:** `Par 3 is at 80% because cue 2.1 Verse sets it.`
- **Wrong:** `Channel 37: HTP, fader priority Override, function #112`

### UX-18 Cue numbers and names follow his scheme

Cue numbers are {S}.{P}.{II}, period separated, any level past 9; scene names follow SS-PP.II-Description, learned from siblings when duplicating.

- **Why:** 2026-10-29 (Q190 and his stated scheme): song sections are derived from the number, so the format has to hold.
- **Check:** `songsections_test`, `review` (not yet enforced)
- **Right:** `1.10.2 · 02-03.01-Chorus Wash`
- **Wrong:** `1.A · 1.9 renumbered to 2.0 · "Chorus Wash copy"`

### UX-19 QLC+ 4.x is in maintenance

Text about upstream says QLC+ 4.x is in maintenance; never abandoned or dead.

- **Why:** 2026-10-28 strategy (Q186): qlcconsole is a home for 4.x users, not a campaign against upstream.
- **Check:** `review` (not yet enforced)
- **Right:** `QLC+ 4.x is in maintenance upstream.`
- **Wrong:** `QLC+ 4 is abandoned.`

## Context-awareness

### UX-20 Lists fit the fixtures in play

A picker or list offers what fits the fixtures it will act on, with a Title Case "Show All …" to see the rest, and never drops the item already in use.

- **Why:** 2026-11-06: effect search offered WLED effects for groups with no WLED device. 2026-11-06 (e1c558d12): the fitted effect picker had to keep a look's current effect even when it no longer fit.
- **Check:** `effectfit_test::whatFitsWhat`, `effectfit_test::searchHidesWhatDoesNotFit`, `lookeditor_test::wledOnlyEffectsNeedAWledTarget`, `walker`, `effectfit_test::theEffectInUseIsNeverDropped` (enforced)
- **Right:** `Pars selected: colour and dimmer effects, [Show All Effects]`
- **Wrong:** `Pars selected: "WLED: Fireworks" at the top of the list`

### UX-21 Plain words just work

The command line takes commands and plain language in the same box; it decides typo vs language itself and never needs "?" or a prefix.

- **Why:** 2026-11-06: the command line made him type "?" to reach AI Assist.
- **Check:** `showtree_test::commandLineTypoOrLanguage`, `librarybuild_test::commandBarTypoOrLanguage`, `showtreeview_test::oneCommandLineTypoOrLanguage` (enforced)
- **Right:** `"make the towers blue" goes to AI Assist; "dimer 50" offers Did you mean dimmer 50`
- **Wrong:** `Unknown command. Type ? to ask AI.`

### UX-22 Show what it acts on and where each value comes from

A row for an effect, palette or layer shows what it runs on (and, on a mixed target, how much of it and why not the rest) and, for each value, its origin: own, from a palette or preset, script default, tracked from a cue. Like an Aim row.

- **Why:** 2026-11-06: an effect in the Show Tree didn't show its group or its values. 2026-11-06 (b3784b1b6): a position effect on towers of movers and pars silently ran on half.
- **Check:** `showtreeview_test::anEffectShowsWhatItRunsOnAndWhereEachValueComesFrom`, `showtreeview_test::trackedLayersShowGreyWithWhereTheyCameFrom`, `walker`, `effectfit_test::aMixedSetRunsOnSome` (enforced)
- **Right:** `Runs on: Towers (2 of 4 fixtures: Pars have no pan/tilt) · Speed 2 per beat, from Effect “Chase”`
- **Wrong:** `EFX 3 (no target, no values)`

### UX-23 Nothing to say, nothing shown

A chip, button or column with nothing to report stays hidden until it has something: power without sources, MTC without a source, Upgrade without chasers, Productions with one production.

- **Why:** 2026-10-23 (Q75, Q89, Q90, Q98): the footer and headers were full of "MTC: no source" and empty controls.
- **Check:** `mainbar_test::thePowerChipNeedsPowerSetUp`, `walker`, `review` (enforced)
- **Right:** `No MTC chip until a timecode source is set`
- **Wrong:** `MTC: no source (in every show's footer)`

### UX-24 F1 opens help on what's in front

Every dialog and view carries a helpTopic, so F1 opens the guide for it.

- **Why:** 2026-10-10 in-app help: context-aware help was the ask; a new dialog without a topic drops to Getting Started.
- **Check:** `appviews_test::helpOpensOnWhatIsInFront`, `walker` (enforced)
- **Right:** `F1 in Venue Focus opens focus-calls.md`
- **Wrong:** `F1 in a new dialog opens getting-started.md`

### UX-25 A console setting holds on every surface

A choice made on the console (feedback mode, units, view) is what the pad, voice, web page and dialogs use, without asking again.

- **Why:** 2026-10-24 (Q127): "this should default to what's set on the console and follow that thru the entire operation."
- **Check:** `review` (not yet enforced)
- **Right:** `Feedback: Lights on the console; the pad gives lights, no voice`
- **Wrong:** `The pad starts in Lights + Voice whatever the console says`

## Feedback & state

### UX-26 Anything that waits shows it is busy

An action that can take more than a moment shows a busy sign where it was asked, what it is waiting on, how to cancel, and elapsed seconds after 2 s.

- **Why:** 2026-11-06: no sign AI Assist was working.
- **Check:** `aiassist_test::busyIndicatorWhileAsking`, `walker` (enforced)
- **Right:** `⟳ Asking AI Assist (llama3)… 4 s · Esc cancels`
- **Wrong:** `The line just sits there`

### UX-27 A greyed control says why

Every disabled button or menu item says on hover why, and how to make it available when there is a way.

- **Why:** 2026-10-25 (Q146): Focus mode's Start was greyed with no reason; 2026-10-23 (Q81): Undo silently did nothing.
- **Check:** `appviews_test::editUndoSaysWhereItWorks`, `cuerunner_test::noMainListGreysGoAndOffersUpgrade`, `walker` (not yet enforced)
- **Right:** `Start (greyed) — "Needs a moving head and a stage target"`
- **Wrong:** `Start (greyed), no tooltip`

### UX-28 Nothing is refused silently

When the app won't do something, it says what and why where the person is looking; a drop of something compatible does what it obviously means instead of refusing (a group set on a scene adds its member groups).

- **Why:** 2026-11-03 (Q205): Add Fixture past universe 1024 silently added nothing. 2026-10-23 (Q86): a group set dropped on a Build scene was refused with no word.
- **Check:** `addfixture_test::aFixturePastTheUniverseLimitIsRefused`, `showtreeview_test::refusedDropsSayWhy`, `monitor_test::aHeadThatCanLightNothingSaysWhy`, `focusmode_test::aPausedControllerSaysWhy`, `review` (enforced)
- **Right:** `"Universe 1025 is past the limit (1024). Nothing was added."`
- **Wrong:** `Dialog closes, no fixture`

### UX-29 Red is for real problems

Red means something is wrong on a real rig; "not set up yet" or "nothing patched" is neutral grey.

- **Why:** 2026-10-23 (Q64): a new show with no outputs said red Not ready.
- **Check:** `appviews_test::theReadinessChipIsGreyWithNoOutputs` (enforced)
- **Right:** `grey "No outputs: preview only"`
- **Wrong:** `red "Not ready" on an empty show`

### UX-30 Prompts are quiet lines, once

A suggestion goes in the notice bar with a button, once per session; a modal dialog is only for a destructive or blocking choice.

- **Why:** 2026-10-23 (Q92): GO in Design offers Operate in the notice bar, not a dialog.
- **Check:** `appviews_test::goInDesignOffersOperateOnce`, `review` (enforced)
- **Right:** `Notice bar: You're in Design. Switch to Operate? [Switch]`
- **Wrong:** `A modal box on every GO`

### UX-31 One edit, one undo; ⌘Z says where it doesn't work

Every user action is one undo step (however many objects it touches), and where ⌘Z doesn't work Edit ▸ Undo is greyed with "Nothing to undo on this tab".

- **Why:** 2026-10-23 (Q81) and 2026-10-24 (Q117): ⌘Z silently did nothing on some tabs, and one Keep took two undos.
- **Check:** `showtreeview_test::everyActionIsOneUndoStep`, `appviews_test::editUndoSaysWhereItWorks`, `appviews_test::editUndoFollowsTheTabInFront` (enforced)
- **Right:** `Dragging a cue-list clip 6 s later: one Undo puts every cue back`
- **Wrong:** `Undo restores half the cues; ⌘Z on Run does nothing, silently`

### UX-32 Warnings keep their state visible

A warning is open (amber), meant (green, with who, when and note on hover) or not applicable (listed in Show Review under Accepted); answered warnings don't vanish.

- **Why:** 2026-11-03 (Q210): answered cyc warnings disappeared from the cue.
- **Check:** `review` (not yet enforced)
- **Right:** `cyc ✓ (hover: Branson, 11-03, intended wash)`
- **Wrong:** `the warning gone with no trace`

### UX-33 The footer tells the state of the rig

The footer always shows what the stage is showing (LIVE, Blind, Build look, holds), the venue, and one chip listing everything overriding the rig (Park, Highlight, Flash, Solo, Focus, Blind, Hold at end) that clears them with a click.

- **Why:** 2026-10-24 (Q129) venue chip; 2026-11-03 (b7d0d24cb) stage chip: he couldn't tell what was on the output. 2026-10-23 (Q80): overrides were spread across several indicators.
- **Check:** `appviews_test::theFooterSaysWhatLightsTheStage`, `appviews_test::theVenueChipIsInTheFooter`, `mainbar_test::oneChipListsAndClearsOverrides`, `mainbar_test::theOverridesChipLetsGoOfHighlightAndFocusMode` (enforced)
- **Right:** `LIVE 2.1 Verse · Venue: Lincoln HS · Overrides: Park 2, Solo ▸ Clear All`
- **Wrong:** `Blind on with nothing in the footer; a Park light here, a Solo badge there`

## Layout

### UX-34 No overlap or clipping at three sizes

Every screen, editor and dialog lays out without overlapping or clipped widgets at 1024×640, 1280×800 and 1920×1080, and the tab strip fits the window with no scroll arrows.

- **Why:** 2026-11-03 (Q207): the colour picker's "Wheel / ring" row lay under other widgets. 2026-11-04 (3fa7b2d9e): tabs scrolled off a laptop screen.
- **Check:** `layoutsanity_test::everyScreenIsLaidOutCleanly`, `walker`, `layoutsanity_test::theTabStripFitsTheWindow` (enforced)
- **Right:** `every row readable at 1024×640`
- **Wrong:** `a row drawn under the one below it`

### UX-35 Footer items share one line

The footer's right-side items sit on one line, centred, at equal height and padding, and never overlap.

- **Why:** 2026-11-04 (d8a0acac0) and again 2026-11-06: footer chips misaligned.
- **Check:** `layoutsanity_test::theFooterChipsNeverOverlap`, `appviews_test::theFooterRightSideSharesOneLine` (enforced)
- **Right:** `GM · Ready · Venue chips on one baseline`
- **Wrong:** `one chip 3 px lower and taller than its neighbours`

### UX-36 Words people must read are never cut

State text, cue names and anything a lesson tells students to read is shown whole at the smallest window size.

- **Why:** 2026-10-24 (Q115): the Run tab cut "▷ STANDBY" to "▷ STA…" though lesson 1.5 says to read it.
- **Check:** `layoutsanity_test::theShowTreeShowsCueNamesWithFreshSettings`, `walker` (enforced)
- **Right:** `▷ STANDBY`
- **Wrong:** `▷ STA…`

### UX-37 Readable in every theme

Text, state colours and cue colours stay readable in every theme (Default, Blue, Red Shift, QLC+ Original, dark).

- **Why:** 2026-10-15 (270e59ecc) Windows themes and 2026-10-22 cue colours: text vanished on some themes.
- **Check:** `appviews_test::textColoursReadInEveryTheme`, `mainbar_test::blackoutAndBlindLightUpInEveryTheme`, `cuerunner_test::textColoursOnEveryLine` (enforced)
- **Right:** `a yellow cue colour drawn darker on a light theme so it reads`
- **Wrong:** `yellow cue text on a white row`

### UX-38 Panels open inside the main window

Pickers and helper panels dock in the main window or open in front of it; nothing opens behind it.

- **Why:** 2026-10-23 (Q61): pickers opened behind the main window.
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `the colour picker docks beside the Look Editor`
- **Wrong:** `the picker opens behind the main window`

### UX-39 Plot overlays line up

Rulers, readouts, handles and selection rings on the plot sit on the thing they mark at every zoom and view.

- **Why:** 2026-10-27 (Q179): 13 of 27 Lighting Studio overlays were off.
- **Check:** `plotalignment_test` (enforced)
- **Right:** `a target's readout under the target at 4× zoom`
- **Wrong:** `the readout a fixture-width off at 4×`

### UX-40 A UI change shows its before and after

A change to ui/, web pages or the rig view ships with before/after shots at 1024×640, 1280×800 and 1920×1080 that he reviews before merge.

- **Why:** 2026-11-03 (Q207): "do we not do after-patch checking to see that the result matches intended behavior?"
- **Check:** `manual` (not yet enforced)
- **Right:** `shots-041/<agent>/before-1280x800.png, after-1280x800.png ...`
- **Wrong:** `"gate=0" as the only evidence for a layout change`

## Consistency across views

### UX-41 One object, one menu

Right-clicking an object anywhere gives the place's own items on top, one separator, then the same kind block in the fixed order Open, Kind, Selection, Organise, Edit, Remove.

- **Why:** 2026-11-03: "a line on rig view or in tree .. should still have the same paradigm where the right click can edit all the things that I can in the build".
- **Check:** `contextmenus_test::aPaletteHasOneMenuEverywhere`, `contextmenus_test::aFixtureHasOneMenuEverywhere`, `contextmenus_test::sectionsLayOutInOrderWithOneSeparatorBetween` (enforced)
- **Right:** `Set to ▸ · None (off here) | Edit Palette · ... · Delete Palette…`
- **Wrong:** `Delete in the middle; a different palette menu in the Show Tree`

### UX-42 Duplicate means one thing and works on everything

⌘D is Duplicate in every list, no two application shortcuts share a key, and every kind of list item (cue, layer, hit, cue list, production, reusable cue, palette, truss, target) offers Duplicate, Copy and Paste.

- **Why:** 2026-11-04 (Q214): ⌘D meant Duplicate, Dump DMX and Delete Universe in three places. 2026-11-04 (Q215): a festival cut of a cue list meant rebuilding it.
- **Check:** `appviews_test::noTwoApplicationShortcutsShareAKey`, `appviews_test::ctrlDInBuildListsDuplicates`, `appviews_test::ctrlDOnThePlotDuplicates`, `appviews_test::ctrlDOnTheShowTreeDuplicates`, `showtreeview_test::listsProductionsAndReusableCuesDuplicateCopyAndPaste`, `monitor_test::targetsDuplicateAndCopyPaste`, `review` (enforced)
- **Right:** `⌘D duplicates the selected universe's row; ⌫ deletes it`
- **Wrong:** `⌘D deletes a universe on Connections`

### UX-43 One selection everywhere

The plot, Workbench, Fixtures, Flash/Park/Highlight and the pad act on one selection, like an Eos magic sheet.

- **Why:** 2026-11-02 (Q201): each view kept its own selection.
- **Check:** `selectionsync_test` (enforced)
- **Right:** `select SL towers on the plot; the Workbench shows them selected`
- **Wrong:** `the Workbench still shows the previous pick`

### UX-44 Surface area stays flat

A new menu item, tab, panel or mode replaces one or is justified in writing; the menu-item count is flat or falling each release; a new top-level tab or playback path needs his explicit yes.

- **Why:** 2026-10-23 review (102-item menus, 8+ tabs), freeze Q82; 2026-11-02 (Q200): two new entries folded.
- **Check:** `review`, `manual` (not yet enforced)
- **Right:** `Calling Script… folded under More ▾`
- **Wrong:** `a new top-level "Tools" menu`

### UX-45 One GO path

Every GO (keys, Run tab, VC Cue Runner, pad, OSC, web) ends in the same call with its route, so they behave the same.

- **Why:** 2026-10-23 (Q74), built 2026-10-31: GO did slightly different things from different places.
- **Check:** `appviews_test::everyRouteTakesTheOneGoPath`, `padrun_test::xboxAndDualSenseSendTheSameCommands` (enforced)
- **Right:** `pad GO and Space both fire App::runCommand()`
- **Wrong:** `a VC button that steps the chaser directly`

### UX-46 Solo and Play on every cue and layer page

Solo and Play are available on every cue and layer page, docked or not (greyed with a reason in Operate).

- **Why:** 2026-11-01: "the solo in layer is working great .. that's a huge plus"; Solo had silently gone missing from one page.
- **Check:** `showtreeworkbench_test::soloAndPlayStayAvailable`, `showtreeworkbench_test::soloAndPlayGreyInOperate` (enforced)
- **Right:** `Solo on the docked Workbench's layer row`
- **Wrong:** `Solo only on the undocked page`

### UX-47 Positions relative to the stage

Where a light points is expressed against stage targets and lines (Aim, Formation) before raw pan/tilt.

- **Why:** 2026-11-01: "tying the layout to the targets .. that's gonna help a lot"; Q197/Q206 split Aim (where) from Formation (how they spread).
- **Check:** `review` (not yet enforced)
- **Right:** `Aim: DSC · Formation: pairs, crossed`
- **Wrong:** `Pan 127 / Tilt 64 typed per fixture`

### UX-48 QLC+ habits stay first-class

A QLC+ 4 show opens as it was and sends the same DMX; nothing nags or converts on load; upgrading is one menu item when asked.

- **Why:** 2026-10-22 (Q29) and 2026-10-23 (Q63, Q76): never nag, never convert on load.
- **Check:** `appviews_test::aShowWithoutAViewOpensInItsDefault`, `appviews_test::autoOpenFollowsWhoSavedTheShow`, `stockroundtrip_test` (enforced)
- **Right:** `a 4.x show opens in Classic QLC+, shutters as saved`
- **Wrong:** `"Convert this show now?" on open`

## Safety in Operate

### UX-49 Operate refuses edits from every input

In Operate, edits are refused from menus, the command line, AI Assist, the web page and the pad alike, each with a line saying so.

- **Why:** 2026-11-03 (Q202): typed effect fades still worked in Operate.
- **Check:** `aiassist_test::askInOperateSendsNothing`, `mainbar_test::operateLocksProgrammingNotTheGrandMaster`, `mainbar_test::anOperateConsolesProgrammerFaderChangesNothing`, `walker` (enforced)
- **Right:** `"effect fade 2" in Operate: "Edits are off in Operate."`
- **Wrong:** `the fade changes during the show`

### UX-50 Esc is not a stop

Plain Esc never stops playback; Stop All is ⌘⇧Esc.

- **Why:** 2026-10-25 (c8f278d0b): Esc to close a panel stopped the running list.
- **Check:** `appviews_test::plainEscIsNoPlaybackAction` (enforced)
- **Right:** `Esc closes the picker; the cue keeps running`
- **Wrong:** `Esc closes the picker and blacks the stage`

### UX-51 Focusing during a show is quiet

Focusing in Operate shows nothing to the audience: no blink or dim, only the running cue, and the hold lets go on GO.

- **Why:** 2026-10-24 (Q130): Refine in Operate blinked and dimmed lights live.
- **Check:** `refinesession_test::operateIsQuietOnARunningList` (enforced)
- **Right:** `a touch-up on cue 3 changes only that light, smoothly`
- **Wrong:** `every other light dims to 15% mid-song`

### UX-52 A hold never confirms a warning

Holding a pad button may act, but never answers a warning or confirmation; that takes a deliberate press.

- **Why:** 2026-10-24 (Q132): a held A could confirm the shared-palette warning and change every scene.
- **Check:** `review` (not yet enforced)
- **Right:** `hold A (1.0 s) saves; the shared-palette warning still waits for a press`
- **Wrong:** `hold A saves and confirms the warning in one go`

### UX-53 Safety lines always speak

A controller dropping out or coming back is announced, and warnings, refusals, "controller lost" and the venue ask are spoken even when feedback is Lights only, with a distinct rumble/light pattern.

- **Why:** 2026-10-24 (Q131): in Lights only a Start on stage got no answer. 2026-10-24 (88409a2a7, Q131): a pad going to sleep froze the session with no word.
- **Check:** `focuscallsui_test::theVoiceNeverDropsAConfirmation`, `focuscallsui_test::theVoiceHearsTheAnnouncements`, `appviews_test::aControllerComingAndGoingIsSaid` (enforced)
- **Right:** `Lights only: "Controller lost" is still said`
- **Wrong:** `Lights only: silence when the pad drops`

### UX-54 Remote GO is off until the show allows it

A GO from the phone or web page is refused until Show Settings allow remote GO, and the Operate PIN still applies.

- **Why:** 2026-11-03 (Q204).
- **Check:** `appviews_test::aWebGoIsRefusedUntilTheShowAllowsIt`, `appviews_test::aPinKeepsTheShowInOperate` (enforced)
- **Right:** `web GO: "Remote GO is off for this show"`
- **Wrong:** `anyone on the Wi-Fi can GO`

### UX-55 A locked plot moves nothing

With the plot locked, nothing moves or drops, targets included, unless "Targets stay adjustable" is on.

- **Why:** 2026-10-27 (Q183): a locked platform still saved where it was dropped.
- **Check:** `monitor_test::aLockedPlotRefusesADrag`, `monitor_test::aDropOnALockedPlotIsRefused` (enforced)
- **Right:** `drag on a locked plot: nothing moves, the lock flashes`
- **Wrong:** `the truss snaps to where it was dropped on release`

### UX-56 Fixed means checked on his real path

A fix for something he hit on his rig is "fixed" only after it is checked on his show, patch and device layer; otherwise the report says exactly what was and wasn't checked.

- **Why:** 2026-11-04: the follow spot was reported fixed three times on faked input; "DO NOT REPORT FIXED UNTIL YOU HAVE VALIDATED THAT WORKS".
- **Check:** `manual` (not yet enforced)
- **Right:** `"HID plugin on dink, DanceHysteria-0.6, stick moved the spot: PASS"`
- **Wrong:** `"fixed (unit test injects stick values)"`

## Teaching & course voice

### UX-57 Students are crew

Student text uses real theatre vocabulary defined once, normal sentence rhythm, and no kid-speak, cheerleading, reassurance or talk about style.

- **Why:** 2026-10-21: the course's talk about tone and short sentences was "a bit condescending".
- **Check:** `review` (not yet enforced)
- **Right:** `"A cue that tracks keeps its levels until a later cue changes them."`
- **Wrong:** `"Great job! Don't worry, tracking is easy!"`

### UX-58 Why first, one idea per step

Every lesson, app lessons included, opens with why it matters; then one idea per step, splitting a long lesson rather than cutting a fundamental.

- **Why:** 2026-10-29: "they need to know the WHY of what they're doing before the HOW." 2026-10-28: "the course should be simple and concise" (handouts ran ~1,600 words).
- **Check:** `review` (not yet enforced)
- **Right:** `"Tracking saves re-setting every light each cue." then 3. Select the towers. 4. Press Aim.`
- **Wrong:** `"Step 1: click + Cue, select the towers, press Aim, choose DSC and save."`

### UX-59 One student link per lesson, no teacher material

A student gets one link per lesson that holds everything for it and leads nowhere near teacher guides or answer keys.

- **Why:** 2026-10-28 and 2026-10-29 (Q187): the course site showed answer keys to everyone.
- **Check:** `check-doc-links.py`, `manual` (not yet enforced)
- **Right:** `sandinak.github.io/qlcconsole/class/1-1/`
- **Wrong:** `a lesson page linking to TEACHER.md`

### UX-60 Help ships with the feature

A feature lands with its help page (and a course note where it applies); every help link resolves, no bare `<tag>` eats a page, and screenshots are retaken after a UI change (tools/help-shots.sh, Blue theme) with callouts that match the key.

- **Why:** 2026-10-10 in-app help; 2026-10-28 strategy guardrail 2: no "help later". 2026-10-23 (Q72): pictures still showed "Programming" and a clipped Look Editor.
- **Check:** `helpbrowser_test::everyLinkInTheGuidesGoesSomewhere`, `helpbrowser_test::everyGuideIsBundledAndLoads`, `review`, `help-shots.sh`, `manual` (enforced)
- **Right:** `"WLED: &lt;effect&gt;" in the guide`
- **Wrong:** `"WLED: <effect>" (the rest of the page disappears)`
## How to add a rule

Every new catch by the user becomes a rule and a check, in the same change
as the fix. A catch fixed without one comes back: the metres field was
fixed on 2026-10-27 and found again on 2026-11-06 (UX-01).

1. **Write the rule** under its theme with the next free `UX-NN`: one
   sentence in his words, the catch and its date (board Q-number or commit),
   and a right and a wrong example. If it is a design decision rather than a
   catch, cite the answered board question; don't write a preference he
   hasn't stated.
2. **Add it to [ux_rules.json](ux_rules.json)** with the same ID, theme,
   rule and checks. Give it `patterns` when the wrong text can be matched
   (`forbid_regex`, with a `scope`: `ui_text`, `source`, `docs_and_ui_text`,
   `course_student` or `widget`), and make sure the pattern matches the
   wrong example and not the right one.
3. **Give it a check**, in this order of preference:
   - a gate test that fails on the wrong example, named
     `<what is true>` like the others (`aFixturePastTheUniverseLimitIsRefused`).
     A source scan (`units_test`, `menuwords_test`) is the cheapest way to
     stop a whole class; a pre-build script such as
     `platforms/linux/check-win-macros.sh` (the `near`/`far` catch, three
     times in two releases) is the model for anything that only breaks on
     another platform;
   - `walker`, with a `require` the walker can assert on a live widget;
   - `review`, when it takes judgement (words, course voice);
   - `manual` only for what needs his eyes or his rig.
4. **Mark it enforced** only when the gate fails on the wrong example. A
   rule checked by `review` or `walker` today moves to a test when one can
   be written.
5. **A rule that turns out wrong** is changed with his answer cited, not
   deleted quietly.
