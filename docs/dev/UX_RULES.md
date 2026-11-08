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
  - `review`: the AI reviewer reads the diff and the screenshots against the
    rule ([UX_REVIEW.md](UX_REVIEW.md));
  - `manual`: a person has to look (shots, his real rig).
- **enforced** means a gate test guards the catch that taught the rule.
  It does not mean every screen is covered: where the test only guards one
  place (the effect picker, the power chip), `walker` is listed to cover the
  rest.
- **Right / Wrong:** one example of each, as the person would see it.

## Summary

| Theme | Rules | Count | Enforced |
|---|---|---|---|
| Units & numbers | UX-01, UX-02, UX-03, UX-04, UX-05, UX-06, UX-63 | 7 | 5 |
| Words | UX-07, UX-08, UX-09, UX-10, UX-11, UX-12, UX-13, UX-14, UX-15, UX-16, UX-17, UX-18, UX-19 | 13 | 7 |
| Context-awareness | UX-20, UX-21, UX-22, UX-23, UX-24, UX-25 | 6 | 5 |
| Feedback & state | UX-26, UX-27, UX-28, UX-29, UX-30, UX-31, UX-32, UX-33 | 8 | 6 |
| Layout | UX-34, UX-35, UX-36, UX-37, UX-38, UX-39, UX-40 | 7 | 5 |
| Consistency across views | UX-41, UX-42, UX-43, UX-44, UX-45, UX-46, UX-47, UX-48 | 8 | 6 |
| Safety in Operate | UX-49, UX-50, UX-51, UX-52, UX-53, UX-54, UX-55, UX-56 | 8 | 6 |
| Teaching & course voice | UX-57, UX-58, UX-59, UX-60 | 4 | 1 |
| Ease of use | UX-61, UX-62, UX-64, UX-65, UX-66, UX-76 | 6 | 0 |
| Does what it says | UX-67, UX-68, UX-69, UX-70, UX-71, UX-72, UX-73, UX-74, UX-75 | 9 | 1 |

Main check: 42 tests, 25 review, 6 walker, 3 manual. 42 of 76 enforced.

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

### UX-63 Help gives a length in feet first, metric in brackets

A help page writes a length in feet and inches with the metric after it in brackets, 4 ft (1.2 m); never a metric length on its own.

- **Why:** 2026-11-07: the Wand help said "1.2 m", "2 cm" and "25 mm" on a product whose lengths default to feet; it now reads "4 ft (1.2 m)" (feat/042-wand-spacing). Help can't follow the setting, so it gives both, feet first (UX-01).
- **Check:** `review` (not yet enforced)
- **Right:** `Wand length: the staff, 4 ft (1.2 m) unless set.`
- **Wrong:** `Wand length: the staff, 1.2 m unless set.`

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
## Ease of use

### UX-61 Findable from where you work

A feature is reachable, or pointed to, from the place a person does that work: its control, or one line naming where it is, is on that screen.

- **Why:** 2026-11-07: he looked for song sections on the cue list (right-click a cue, the list's page); they are switched on only by Numbering on the production's page, and nothing on the list pointed there. The hint on the list page is on feat/042-section-hint.
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `The list's page: "Sections follow the production's Numbering (None)." with a link to it`
- **Wrong:** `The list's page with no word of sections; Numbering only on the production's page`

### UX-62 Testable where it's built

Anything you build can be tried from where you build it, in Design, without switching to Operate.

- **Why:** 2026-11-07: he needed to test a hit's key while programming, and keys fired only in Operate; they now fire in Design when the hit is armed (the Run tab's list, or selected in the Show Tree), on feat/042-key-triggers.
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `A hit selected in the Show Tree, Design: F7 plays it`
- **Wrong:** `F7 does nothing until Operate`

### UX-64 Read it from the device

Configuration a device already holds is read from it, not typed in by hand; typing stays as the fallback.

- **Why:** 2026-11-07: a WLED fountain's sprinkle-loop spans could only be typed into a table; Read Segments now imports them from the device and Edit Spans… stays for changes (feat/042-fountain-sprinkles). "Don't make him count what the device knows."
- **Check:** `review` (not yet enforced)
- **Right:** `Spans: [Read Segments] (6 spans from the WLED) · Edit Spans…`
- **Wrong:** `Spans: type First LED and Last LED for each of 6 spans`

### UX-65 Auto is right for the real gear and shows its number

An Auto setting works its value out from the gear as it is set up, and shows the number it chose.

- **Why:** 2026-11-07: a wand's Auto turns assumed LED tape and drew half a turn for a 16 ft fairy string; Auto now works from the string's length and shows "Auto (58.5)" (feat/042-wand-spacing).
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `Turns: Auto (58.5)`
- **Wrong:** `Turns: Auto  (half a turn drawn on a 16 ft string)`

### UX-66 Easy to find, quick to use

A student finds a core task from where they are working without being told, and a professional isn't slowed down doing it.

- **Why:** 2026-11-07, approving the flows: "this tool will be used by students and professionals .. it needs some level of ease of use while being somewhat comprehensive .. if I have to explain everything all the time, even with training, then we've failed in the design." The checks are under [Complexity and ease of use](#complexity-and-ease-of-use); the tasks under [Flows](#flows).
- **Check:** `walker`, `review` (not yet enforced)
- **Right:** `Number a show into songs: 5 clicks, each control on the screen before it`
- **Wrong:** `A core flow whose next control is found only in the help`

## Does what it says

Caught 2026-11-07 on his office WLED test (branches fix/042-wled-*, fix/042-duplicate-own-looks, fix/042-footer-save-flash; not on main yet). The flows are F-11 and F-12.

### UX-67 A device you add is reachable

Adding a network fixture leaves it on an output that reaches it; one that can't be reached says so in the footer, with a one-click fix.

- **Why:** 2026-11-07: he added a WLED (Connection: Network (WLED), IP 172.18.2.100) through Add Fixture. It landed on universe 1, whose output is Art-Net to his node, so nothing reached the WLED; moving it by hand to universe 2 left that universe with no output at all (fix/042-wled-add-patch).
- **Check:** `review`, `walker` (not yet enforced)
- **Right:** `Add Fixture (WLED) → its own universe, WLED output, sent to its IP`
- **Wrong:** `WLED on U01-Office (Art-Net to 172.18.2.10), no warning`

### UX-68 A control shown works on that fixture, or says why not

A control is offered only where it does something for that fixture; where it can't, it is greyed with the reason (UX-27).

- **Why:** 2026-11-07: Runs (Once / N times) and At the end (Hold / Release) were offered for a WLED device effect and did nothing. "When I set it to once .. it does the entire thing once .. and then does it again"; "hold and release don't appear to do much" (fix/042-wled-runs).
- **Check:** `review`, `walker` (not yet enforced)
- **Right:** `Runs: Once on a WLED effect → it plays once, then holds`
- **Wrong:** `Runs: Once offered on a WLED effect; it loops anyway`

### UX-69 A copy is a copy

Duplicate, Copy and Paste never leave a hidden link; anything still shared says where else it is used and offers Own Copy.

- **Why:** 2026-11-07: duplicating a cue shared the layer's own (inline) looks, so "all the edits to the layer go to both cues", and the Look Editor showed no "Used by" for them (fix/042-duplicate-own-looks).
- **Check:** `review`, `walker` (not yet enforced)
- **Right:** `Duplicate a cue, change the copy's colour: the original keeps its colour`
- **Wrong:** `Duplicate a cue, change the copy: both cues change, and the Look Editor says nothing is shared`

### UX-70 A cue shows what it asks for

Nothing carries in from earlier cues except tracked values, and what tracks is visible; a cue's look is what it says.

- **Why:** 2026-11-07, his words: "unless tracked .. new cue should apply all new values to meet the required look". A colour-only cue after a Fireworks Starburst cue left the WLED still running Starburst: its effect setting latched, because non-intensity channels keep their last value (fix/042-wled-runs).
- **Check:** `review`, `walker` (not yet enforced)
- **Right:** `A colour-only cue after Starburst: the WLED goes to solid colour`
- **Wrong:** `A colour-only cue after Starburst: the WLED keeps running Starburst`

### UX-71 Properties fit the kind of device

A fixture's properties show what that kind of device has: a pixel string shows string things (connection, IP, LED count, length, pixel path), not a mounted light's.

- **Why:** 2026-11-07: Fixture Properties showed a WLED fairy string as a mounted light (orientation, pan/tilt, beam). "Not sure that is how that should work for strings connected to it (we may do other things in the future that utilize this idea .. but not yet)" (fix/042-wled-add-patch).
- **Check:** `review` (not yet enforced)
- **Right:** `WLED string: Connection, IP, LED count, String length, Pixel path`
- **Wrong:** `WLED string: Orientation, Pan/Tilt, Beam angle`

### UX-72 Locate works on every patched fixture

Locate is unmistakable on the device (solid, full, white, sharp flashes) over whatever is playing, and everything is back after.

- **Why:** 2026-11-07: "locate doesn't appear to work" on a WLED (fix/042-wled-locate).
- **Check:** `review`, `manual` (not yet enforced)
- **Right:** `Locate on a WLED mid-effect: solid white flashes, then the effect resumes`
- **Wrong:** `Locate on a WLED: nothing visible, the effect keeps running`

### UX-73 Snap means snap on every connection

A network device follows a snap within the network's own time: it is sent at once with no glide; fades use the cue's time.

- **Why:** 2026-11-07, measured on ender: a WLED over its JSON API reacts about 150 to 340 ms after DMX fixtures on a snap cue (a 100 ms send tick, about 50 ms Wi-Fi with a max of 140, and a 100 ms glide). His spell cue snaps the stage to black with the staff (a WLED) still on. Fixed (feat/042-wled-snap): a change goes at once, a step with no glide, and Move in Black presets a dark WLED's next effect so the GO is its brightness alone; on the office WLED the POST now leaves 23 ms after the Art-Net bytes reach the Pi (was 62 to 105) and the WLED has it 61 ms after (was 129, max 264).
- **Check:** `wledshow_test::aSnapCueReachesTheStaffWithTheStage`, `wled_test::aSnapGoesAtOnceNotOnTheNextTick`, `markplanner_test::aDarkWledIsMovedInBlackToItsNextLook`, `manual` (enforced)
- **Right:** `A snap cue: the WLED changes with the DMX fixtures, within the Wi-Fi's own delay`
- **Wrong:** `A snap cue: the WLED glides in 100 ms after the others, so the staff stays on`

### UX-74 Status doesn't flash

A save or a routine update changes a footer chip at most once, never on and off.

- **Why:** 2026-11-07: "when I save sometimes the power and current cue in the bottom flash hard" (fix/042-footer-save-flash).
- **Check:** `review` (not yet enforced)
- **Right:** `Save: the power and cue chips stay steady, or change once`
- **Wrong:** `Save: the power and cue chips blink off and on`

### UX-75 Tested through the path the person uses

An end-to-end test drives the same dialogs and menus the person uses, not the engine call under them. (A rule for reviewing tests.)

- **Why:** 2026-11-07: the WLED end-to-end test called the patch planner directly, so it passed while the real Add Fixture path put the WLED on an Art-Net universe (UX-67). It also bears on UX-56.
- **Check:** `review` (not yet enforced)
- **Right:** `A test that opens Add Fixture, picks the WLED and the connection, and clicks OK`
- **Wrong:** `A test that calls the patch planner and checks its result`

### UX-76 Keep it simple for the operator (KISS)

Where a change can make the operator's job simpler, that counts for it: it raises the change, and a feature is weighed by how much it saves the person running the desk.

- **Why:** 2026-11-07, on WLEDs found on the network and patched with one right-click instead of typing an IP and counting channels: "lets add that to our rules .. KISS ... Keep It Simple Stupid .. where we can make it easier for the operator that should raise the consideration of the change or feature."
- **Check:** `review` (not yet enforced)
- **Right:** `A WLED on the network shows in the Inventory: right-click ▸ Patch, done`
- **Wrong:** `Add Fixture, pick the model, type the IP, count the LEDs, then patch a universe to it`

## Complexity and ease of use

Students and professionals use the same screens. Branson, 2026-11-07:
"it needs some level of ease of use while being somewhat comprehensive..
that's a tight line to walk I know .. but if I have to explain everything
all the time, even with training, then we've failed in the design."

**The bar:** a student finds it from where they're working without being
told; a pro isn't slowed down.

The checks (also in [ux_flows.json](ux_flows.json) `bar`), each one a thing
the walker or the reviewer can say yes or no to:

| | Check | How |
|---|---|---|
| C-1 | Every core flow has a click budget and stays within it. | walker |
| C-2 | No dead ends: each step's control is on the screen the step before left, or something on that screen points to it. A step whose next control is neither there nor pointed to is a dead end. | walker |
| C-3 | Defaults work for the common case: a flow changes only what it is about, and an Auto shows the number it chose (UX-65). | walker, review |
| C-4 | Depth goes behind the thing it belongs to (More ▾, ⋯, a right-click, a row that opens), not into a new menu item or tab. | review |
| C-5 | Controls use the course's and the help's word for the same thing ([GLOSSARY](../course/GLOSSARY.md)). | review |
| C-6 | No core flow needs a help page to find its next step. A control found only through help counts as a dead end. | walker, review |
| C-7 | What a flow builds can be tried in Design from where it was built (UX-62). | walker |
| C-8 | A pro's shortcut (a key, the command line, a typed number) sits beside the clicks, never instead of them. | review |

**Against UX-44 (surface flat) and UX-23 (nothing to say, nothing shown).**
Findable does not mean a second copy of the control. The fix for a dead
end is a pointer where the work happens: one line or a link that names
where the setting is and goes there. A pointer is not a menu item, so the
count UX-44 keeps stays flat. It follows UX-23 too: it shows only while it
has something to say (sections off on a list whose numbers would make them)
and goes once the feature is on. A pointer that is always there, whatever
the state, is clutter, and UX-23 wins.

## Flows

The core end-to-end tasks, from the course and his real use. Each step is
screen, control, what you see. **Clicks** count toward the budget (a choice
from a drop-down is 2, typing a value is 1); a *find* step clicks nothing:
it asks whether the next control can be found from where the person is
(C-2). Budgets are the clicks of each path as designed on 2026-11-07,
manual steps included; a flow that grows past its budget is a finding
(UX-66). The walker drives the automated steps offscreen on the course show
`docs/course/shows/lesson-3-8-start.qlcc` ([UX_WALKER.md](UX_WALKER.md#flows));
**manual** steps say why. The machine copy is [ux_flows.json](ux_flows.json):
change both together (check_rules.py keeps the ids, titles and budgets in step).

### F-01 Number a show into songs and fold them

Budget 6 clicks. From: course level 3 lesson 8; 2026-11-07 catch (UX-61). Rules: UX-61, UX-66.

- **Student:** Find sections from the cue list, set Numbering once, fold a song.
- **Pro:** The Numbering hint's Use it link sets the scheme its numbers already follow (one click).

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Show Tree tab | the Show Tree | 1 |
| 2 | Show Tree | the cue list's row | the list's page | 1 |
| 3 | the list's page | Numbering (where sections come from) *(find)* | Numbering, or a line pointing to it, on the list's page | - |
| 4 | Show Tree | the production's row | the production's page | 1 |
| 5 | the production's page | Numbering ▸ Show.Part.Iteration | song section rows in the tree | 2 |
| 6 | Show Tree | a section's ▸ | its cues folded under it | 1 |

A cue's right-click menu needs no sections item: the list's page points to
Numbering (or its **Use it** sets it), and a menu item would grow the
surface (UX-44).

### F-02 Put a key on a hit and fire it while building

Budget 6 clicks. From: course level 3 lesson 5; 2026-11-07 catch (UX-62). Rules: UX-62, UX-61.

- **Student:** Right-click the hit, Learn Trigger…, press the key; select the hit and press it.
- **Pro:** The same key fires it in Operate; the Control Map lists every trigger.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Show Tree tab | the Show Tree | 1 |
| 2 | Show Tree | right-click a hit ▸ Learn Trigger… | the learn dialog hears a computer key (F7) and names it | 3 |
| 3 | Show Tree | the hit's row (arms it) | the hit selected | 1 |
| 4 | Show Tree, Design | F7 | the hit plays, in Design | 1 |

### F-03 Patch a WLED, make it a wand, see the effect from the tip

Budget 15 clicks. From: 2026-11-07 (UX-64, UX-65); docs/features/lighting-studio.md Wand. Rules: UX-65, UX-61.

- **Student:** Add the WLED, pick Wand as its pixel path, read Auto's turns, start it at the tip.
- **Pro:** Type String length and Staff diameter; Auto follows them.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | Fixtures | Add fixture ▸ WLED ▸ Lantern, Connection: Network (WLED), IP, OK *(manual)* | a WLED patched | 6 |
| 2 | any | Lighting Studio tab | the plot | 1 |
| 3 | Lighting Studio | right-click a WLED Lantern ▸ Fixture Properties… | its properties, with Pixel path | 2 |
| 4 | Fixture Properties | Pixel path ▸ Wand; Turns reads Auto (58.5); First LED at ▸ Top (the tip); OK *(manual)* | a wand wound from the tip, its turns shown | 5 |
| 5 | Lighting Studio rig view | an effect on the wand *(manual)* | it starts at the tip | 1 |

### F-04 Build a look in the Workbench and record it as a cue

Budget 11 clicks. From: docs/features/build.md, show-tree.md Record and duplicate; course level 2. Rules: UX-61, UX-66.

- **Student:** Open a scene in Build, play it, Record it into the cue list.
- **Pro:** Type record lit on the Show Tree's command line.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Build tab | Build | 1 |
| 2 | Build | + Scene, drag a group onto it, drop a colour *(manual)* | a look | 3 |
| 3 | Build ▸ Scenes | a scene in the navigator | it in the Workbench | 1 |
| 4 | the Workbench | Play | it on the rig | 1 |
| 5 | the Workbench | "Record it as a cue in the Show Tree ▸" *(find)* | the grey line under Play; its link opens the Show Tree with Record ready | - |
| 6 | any | Show Tree tab | the Show Tree | 1 |
| 7 | Show Tree | a cue | its page | 1 |
| 8 | Show Tree | Record ▸ Everything lit ▸ Record | a new cue after it | 3 |

### F-05 GO through a show in the Run tab

Budget 4 clicks. From: course level 0 lesson 8, level 1 lesson 1. Rules: UX-30, UX-66.

- **Student:** Run tab, GO; the notice offers Operate; GO again.
- **Pro:** ⌘↩ from any tab; ⌘F12 to Operate.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Run tab | the cue list, NEXT on the first cue | 1 |
| 2 | Run | GO | the cue plays; the notice offers Operate | 1 |
| 3 | Run | Switch to Operate (notice bar) | LIVE | 1 |
| 4 | Run, Operate | GO | the next cue plays | 1 |

### F-06 Focus calls: Quick Focus from the plot

Budget 12 clicks. From: course level 3 lesson 6; docs/features/focus-calls.md. Rules: UX-61, UX-27.

- **Student:** Focus ▸ Quick Focus…, Start, step with PgDn, keep with Return, Esc.
- **Pro:** The pad: View + Menu starts it; the same keys on the pad.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Lighting Studio tab | the plot and its Focus button | 1 |
| 2 | Lighting Studio | Focus ▸ Quick Focus… ▸ Start | the focus panel | 3 |
| 3 | the focus panel | PgDn (next) | the panel still up | 1 |
| 4 | the focus panel | Esc (leave) | the panel gone | 1 |
| 5 | Venue Focus | Edit Focus Calls…, + Call, Start venue focus, Return to keep *(manual)* | a venue's correction kept | 6 |

### F-07 Follow spot with a pad

Budget 9 clicks. From: docs/features/stage-targets.md Follow-spot behavior; 2026-11-04 (UX-56). Rules: UX-56, UX-66.

- **Student:** Patch the pad, put Followspot and an Aim on the spot, GO, walk the stick.
- **Pro:** Fine stick and speed step on the pad (fix/042-followspot-feel).
- **Manual:** Needs a game pad: followspotcue_test drives a virtual pad through the HID plugin, which the walker doesn't load, and the feel is checked only on his real pad (UX-56).

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | Connections | patch the pad as an input *(manual)* | the pad found | 2 |
| 2 | Lighting Studio | Add ▸ Target (a person) *(manual)* | a target on the plot | 2 |
| 3 | Build Workbench | + Effect ▸ Effect Palette… ▸ Followspot ▸ Make layer *(manual)* | the spot follows | 4 |
| 4 | Run | GO, walk the left stick *(manual)* | footer: ◎ Follow … stick moving | 1 |

### F-08 Set Units and see every length follow

Budget 10 clicks. From: 2026-11-06 (UX-01). Rules: UX-01, UX-61.

- **Student:** Settings… ▸ Units, OK; the plot's lengths change.
- **Pro:** ⌘, opens Settings.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Settings… ▸ Units: Millimetres ▸ OK | the setting kept | 4 |
| 2 | any | Lighting Studio tab | the plot | 1 |
| 3 | Lighting Studio | right-click a light | Set Mounting Height… in mm | 1 |
| 4 | any | Settings… ▸ Units: Feet and inches ▸ OK | the setting kept | 4 |

### F-09 Fountain pars and sprinkle loops from Read Segments

Budget 11 clicks. From: 2026-11-07 (UX-64); docs/features/lighting-studio.md Fountains. Rules: UX-64, UX-61.

- **Student:** Add Fountain…, drop the pars and the WLED in, Pixel path Sprinkle loops, Read Segments.
- **Pro:** Edit Spans… to change what was read.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | Lighting Studio | Add ▸ Add Fountain… ▸ OK; drop pars in the base and a WLED in the gap *(manual)* | a fountain with its lights | 5 |
| 2 | any | Lighting Studio tab | the plot | 1 |
| 3 | Lighting Studio | right-click a WLED Lantern ▸ Fixture Properties… | Pixel path offers Sprinkle loops; Read Segments beside it | 2 |
| 4 | Fixture Properties | Pixel path ▸ Sprinkle loops (in a fountain) *(manual)* | Points, Spans, Read Segments | 2 |
| 5 | the fixture's fields | Read Segments *(manual)* | Read 6 segments as spans at 3 points. | 1 |

### F-10 Recover after a crash (autosave)

Budget 2 clicks. From: 0.4.1 Known issue; CHANGELOG; course level 3 lesson 9. Rules: UX-28, UX-61.

- **Student:** Open the show; it offers the newer autosave; Recover.
- **Pro:** The same question at start-up, in the start-up window.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | File ▸ Open Recent | the show (a newer autosave beside it), then Recover | Recover the autosave from Show.qlcc? ▸ Recover | 2 |
### F-11 Add a WLED string and play its effects

Budget 26 clicks. From: 2026-11-07 his office WLED test (UX-67 to UX-72). Rules: UX-67, UX-68, UX-70, UX-72, UX-66.

- **Student:** Add the WLED with its IP, fix it on the device, build a Starburst cue that runs once, then a colour cue, then Locate.
- **Pro:** Add Fixture takes the IP and puts the WLED on its own universe in one dialog; the cue's Runs and At the end are in the Look Editor.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | any | Fixtures tab | Fixture Manager | 1 |
| 2 | Fixtures | Add Fixture ▸ WLED ▸ Effect Mode / Effect (15ch) | the Add Fixture dialog with the WLED model | 3 |
| 3 | Add Fixture | Connection ▸ Network (WLED, JSON over Wi-Fi) | an IP field; no universe or patch step | 2 |
| 4 | Add Fixture | the IP (172.18.2.100), OK | the WLED on its own universe with the WLED output, sent to its IP (UX-67) | 2 |
| 5 | footer | the finding chip ▸ Fix on Device… ▸ OK | the footer finding gone; the device settings fixed | 2 |
| 6 | any | Show Tree tab | the Show Tree | 1 |
| 7 | Show Tree | + Cue List, + Cue | a cue list with one cue | 2 |
| 8 | Show Tree | the layer on the WLED | its Look Editor | 1 |
| 9 | Look Editor | Effect ▸ Fireworks Starburst | the WLED effect chosen | 2 |
| 10 | Look Editor | Runs ▸ Once; At the end ▸ Hold | Runs: Once and At the end: Hold, working on the device (UX-68) | 4 |
| 11 | Show Tree, Design | GO on the armed cue | the effect plays once on the WLED, in Design (UX-62) | 1 |
| 12 | Show Tree | + Cue, a colour only | a second cue with a colour and no effect; the WLED goes to solid colour (UX-70) | 3 |
| 13 | Show Tree | right-click the WLED ▸ Locate | solid white flashes over whatever plays; all back after (UX-72) | 2 |

### F-12 Duplicate a cue and change only the copy

Budget 4 clicks. From: 2026-11-07 catch (UX-69); docs/features/show-tree.md Record and duplicate. Rules: UX-69, UX-66.

- **Student:** Select the cue, Duplicate, change the copy, check the original.
- **Pro:** Duplicate with ⌘D beside the clicks.

| # | Screen | Control | You see | Clicks |
|---|---|---|---|---|
| 1 | Show Tree | a cue | its page | 1 |
| 2 | Show Tree | Duplicate (⌘D) | a copy of the cue after it (UX-42) | 1 |
| 3 | Look Editor | change the copy's effect or colour | the copy changes; the Look Editor says what, if anything, is still shared (UX-69) | 1 |
| 4 | Show Tree | the original cue | unchanged (UX-69) | 1 |

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
