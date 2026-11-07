# UX rule proposals (parity check)

**Status: proposals only.** Nothing here is a rule until Branson says yes.
[UX_RULES.md](UX_RULES.md) holds only rules taken from his own words. This
page compares those 60 rules with the operator conventions of deployed
lighting consoles (ETC Eos, grandMA3, ChamSys MagicQ, Hog 4, Avolites Titan,
Obsidian ONYX, QLC+ 5) and with desktop guidelines (Apple HIG, Microsoft
Windows UX guide, WCAG 2.2). Each proposal below is written to drop into
UX_RULES.md once approved: one sentence, a reason, a check. Approved
proposals get the next free `UX-NN` number. The P-numbers here are not kept.

Written 2026-11-06 against `tools/ux-rules` (6ccb14386). "Today" is from
reading the code at that SHA and the 0.4.1 shots in `shots-041/`. I did not
run the app, so where the evidence is a code search, the row says so.

## How it was ranked

**Score = operator value (1–3) × how likely qlcconsole breaks it today (1–3).**
Value 3 means a missed cue, a wrong light in front of an audience, or lost
work. Likelihood 3 means the code search or a shot shows it broken now.
Likelihood 1 means qlcconsole mostly does it already and the rule would
only guard it.

**Fit** is checked against the guardrails in
[STRATEGY.md](../project/STRATEGY.md):

- **fits**: the rule is about how existing UI behaves.
- **fits (small surface)**: it needs a control on an existing screen and no
  new menu item.
- **feature**: it needs a new model, tab, panel or playback path, or it is
  on the NOT-built list. Features are listed separately under
  [Parity gaps that are features](#parity-gaps-that-are-features-not-rules)
  as Roadmap candidates.

**Check** uses the same words as UX_RULES.md: a gate test by name, `walker`,
`review` or `manual`.

## The ten ideas the orchestrator listed: verdicts

| Idea | Verdict | Becomes |
|---|---|---|
| Keyboard-first + focus order | **Keep**: no `setTabOrder` anywhere in ui/src, and no `:focus` style in default.qss | P-09 |
| Live vs Blind unmistakable | **Already does**: the whole footer turns `BlindStyle` blue, the Workbench says "Blind: edits reach the stage on GO", and the plot shows the blind cue. UX-33 covers it. One small refinement only (P-24) | P-24 (low) |
| Tracked/changed/blocked convention | **Refine**: tracked is already grey + ⤳ with its source; up, down and blocked have no mark | P-13 |
| Booth-dark | **Refine**: the Red Shift and VS Code Dark themes exist; the gaps are native dialogs that open white and no UI dimmer | P-11, feature F-07 |
| Errors say what to do next | **Keep**: there are still "Error" / "Output line already assigned" boxes | P-05 |
| Never lose work | **Keep, and it is the top find**: the recovery copy beside a *saved* show is never offered, and Save keeps no earlier versions | P-01, P-02 |
| One format each for times/addresses/numbers | **Keep**: addresses appear in four formats and times in four | P-03, P-04 |
| Empty states say how to start | **Refine**: this conflicts with UX-23 ("nothing to say, nothing shown") unless it is limited to work areas. Mostly done already | P-26 (low) |
| Colour-blind-safe status + contrast | **Refine**: words or glyphs already sit beside most colours, and mainbar_test measures WCAG contrast in the footer. The gaps: one convention for all of it, and contrast measured beyond the footer | P-20, P-19 |
| Destructive undoable-or-confirmed; dangerous looks dangerous | **Keep, sharpened**: some deletes are undoable *and* still ask Yes/No, and Delete sits between More and Undo on the Show Tree toolbar | P-06, P-07 |

## Proposals, ranked

Source keys such as [S1] are listed under [Sources](#sources).

| # | Rule (qlcconsole terms) | Who does it | Why it matters to an operator | Check | Today | Fit | Score |
|---|---|---|---|---|---|---|---|
| **P-01** | Opening a show that has a newer recovery copy beside it (`Show.qlcc.autosave`) offers that copy before anything overwrites it, the same way the untitled one is offered at start. | MagicQ reloads its auto-backup `.sbk` at start [S12]; grandMA3 autosaves on a timer and keeps backups [S9] | A crash mid-tech that loses up to an hour of programming is the worst thing a console can do. The autosave already exists; the work is lost at the last step | test `autosave_test::aNewerRecoveryCopyBesideAShowIsOffered` | **Broken.** `App::checkAutosaveRecovery()` (app.cpp:7611) looks only at `untitled.qxw.autosave`. Opening a saved show never checks `Show.qlcc.autosave`, and the first autosave after reopening replaces it. autosave_test has no case for a named show. The settings label promises "It can be recovered" | fits | **9** |
| **P-02** | Save keeps the last 10 earlier versions of the show beside it, and they can be opened from the existing Open path, with no new menu item. | grandMA3 writes a backup on every save, up to 10 per show [S9]; MagicQ tells users to save under new names "so you have various points you can back-track to" [S12] | One bad save (a wrong Delete, then ⌘S out of habit) otherwise wipes the only good copy. Undo history does not survive a restart | test `autosave_test::saveKeepsTheLastTenVersions` | **Missing.** There is no `.bak` or versioning code in app.cpp; Save overwrites the file | fits (small surface) | **9** |
| **P-03** | A DMX address is shown one way everywhere, `universe.address` with the address padded to three digits (`2.001`). Typing accepts `2.1`, `2/1`, `2:1` or an absolute `513`. | grandMA3 shows `Univ.addr` or an absolute address as a setting [S10]; Eos patch uses universe/address | Patch notes, the plot, the Fixtures list and the paperwork must match the label on the truss. A crew member reading `001 - 012` in one place and `1.12` in another will patch wrong | test: a source scan like `units_test` that forbids raw `%1.%2` and `A %2–%3` address strings and sends all formatting through one helper; `walker` | **Broken.** At least four formats are in use: `"%.3d - %.3d"` (fixturetreewidget.cpp:913), `"clashes at %1.%2"` (:890), `"Universe %1 · A %2–%3"` (monitorfixtureitem.cpp:981, monitor.cpp:2870) and `"universe %1, address %2"` (fixturetreewidget.cpp:895) | fits | **9** |
| **P-04** | A time is shown one way everywhere: `2.5 s` under a minute, `1:30` from a minute, `1:02:03` from an hour. A time field accepts `90`, `1:30` and `1m30`. | Every console uses one time syntax for its whole surface. I found no single primary page that states it, so the rule rests on UX-12 (one word per idea) and on the inconsistency found here | The operator reads fades and waits at speed. `fade 3s`, `in 1:05` and `1 min 5 s` on one screen make a reader stop and translate | test: a source scan forbidding `%1s"` and `min %2 s` in `tr()`, plus one `TimeFormat` helper; `walker` | **Broken.** Formats in use: `"fade %1s"`, `"%1 s"`, `"%1 min %2 s"`, `"in %1:%2"` (cuerunner.cpp:1508) | fits | **6** |
| **P-05** | An error message says what happened, why, and what to do next, in the words of the screen it came from, and is titled by the action that failed, never "Error". | Microsoft: "Effective error messages inform users that a problem occurred, explain why it happened, and provide a solution" [S20] | An error at a tech table is read once, under pressure. "Error / Output line already assigned" leaves the operator guessing which line, assigned to what, and how to fix it | test: a source scan forbidding `QMessageBox::*(…, tr("Error")`; `review` for the three parts | **Partly broken.** inputoutputpatcheditor.cpp:729/749/857 show "Error" with "Output line already assigned" or "An error occurred while trying to open the selected device line"; :332 shows "Unable to save %1". Newer code ("Autosave failed: %1. Save the show (⌘S)…", app.cpp:7498) already does it | fits | **6** |
| **P-06** | An action that one ⌘Z can undo does not ask first. An action that can't be undone asks, says "can't be undone", names the action and the count on its button ("Delete 3 Scenes"), and makes Cancel the default. | Microsoft: "Provide undo" instead of a confirmation, never OK/Cancel, and the safe choice as the default for risky actions [S19]. Apple HIG alerts: a destructive button names the action and has the destructive style [S21]. grandMA3 lets the user choose when to confirm (Oops Confirmation: Always / Main / Never) [S5] | A confirmation on every undoable delete teaches the operator to click Yes without reading. Then the one that really destroys gets the same reflex | test: `contextmenus_test` / a new `confirmations_test` that finds each `QMessageBox::question` on an undoable path; `review` | **Partly broken.** programmingmanager.cpp:2817 asks Yes/No before `deleteFunctionsUndoably()`. There are 21 Yes/No questions with No as the default, and 7 with `DestructiveRole`. Good counter-example: "Delete %1? Undo Patch Change brings it back." (inputoutputmanager.cpp:705) | fits | **6** |
| **P-07** | Show-stopping and destructive controls (Stop All, Blackout, Release All, Delete) are set apart by a separator or space from GO, Undo and harmless buttons, and look dangerous (red text or outline), the same on every toolbar. | Microsoft: "physically separate destructive commands from other commands" [S19]. Every console puts Blackout and Release on their own keys, away from GO | A mis-click on the button next to Undo deletes a cue mid-show. UX-50 already stops Esc from stopping playback; this is the same idea for the mouse | test: `layoutsanity_test::destructiveButtonsStandApart` (each destructive action has a separator or ≥ 8 px on both sides); `walker` | **Broken in one known place.** The Show Tree toolbar runs "More · **Delete** · Undo · Redo" with no separator (shots-041/cli/showtree-typo-didyoumean-1280x800.png) | fits | **6** |
| **P-08** | Counts read as real English: "1 fixture", "4 fixtures", never "fixture(s)". | Qt's `%n` plural forms exist for exactly this [S26]; Microsoft's style guide says not to use "(s)" | "0 fixture(s)" reads as unfinished software at the moment the operator needs to trust the count | test `menuwords_test::noParenthesisSPlurals` (forbid `\(s\)` in `tr()`), plus an English numerus `.ts` so `%n` resolves | **Broken.** 242 `tr()` strings contain "(s)", and the shot shows "Towers (0) — 0 fixture(s)". 220 of them already use `%n` but there is no English `.ts` to resolve it | fits | **6** |
| **P-09** | Every control can be reached with Tab in reading order (left to right, top to bottom), and the focused control shows a visible focus ring in every theme. | WCAG 2.1.1 Keyboard, 2.4.3 Focus Order, 2.4.7 Focus Visible [S23]; Apple HIG keyboard navigation; consoles are keyboard-first by design (Eos command line, MA3 command line) | The booth runs on keys, and a ring the operator can't see means typing into the wrong field during a cue | `walker`: tab through each screen, assert the order and that the focus frame differs from its background by ≥ 3:1 | **Likely broken.** There are 0 `setTabOrder` calls in ui/src (order follows creation) and default.qss has no `:focus` rule. Stylesheet-styled widgets often lose the native ring | fits | **6** |
| **P-10** | An icon-only button has a tooltip *and* an accessible name that say what it does. | Apple HIG Accessibility (VoiceOver labels) [S25]; WCAG 4.1.2 Name, Role, Value [S23] | VoiceOver, scripts and the walker can only find a button by its name. The tooltip is also the only label a new crew member gets | `walker`: every visible `QAbstractButton` with empty `text()` has a non-empty `toolTip()` and `accessibleName()` | **Broken.** ui/src has exactly one `setAccessibleName` call (tooltips: 1246) | fits | **6** |
| **P-11** | In a dark or Red Shift theme nothing opens white: file dialogs, message boxes, the help browser, the startup window and the web page all follow the theme. | grandMA3 ships colour themes plus per-screen brightness faders [S8]; ONYX documents "Setting Display Brightness" [S17]; Apple HIG dark mode [S24] | One white file dialog in a dark house ruins the operator's night vision and lights the front row | test `appviews_test::aDarkThemeOpensNothingWhite` (offscreen grab of each dialog: mean luminance under a threshold); `manual` for native dialogs | **Likely broken.** No `QStyleHints::setColorScheme` / NSAppearance call, so native `QFileDialog`/`QMessageBox` follow macOS appearance, not the app theme. There is also a hard-coded `background: white` at app.cpp:4912 | fits | **6** |
| **P-12** | A fixture or value held by an override (Park, Highlight, Solo, Capture) shows a mark on itself (on the plot, in Fixtures, on its Workbench row), not only in the footer chip. | Eos prints `P` on a parked channel and `C` on a captured one [S2]; grandMA3 marks programmer values in the fixture sheet [S11] | "Why won't this light move?" is answered on the light itself. UX-33's chip says *that* something is parked, not *which* light it is | test `monitor_test::aParkedHeadIsMarkedOnThePlot`, `fixturemanager` equivalent; `walker` | **Missing.** Park appears only in the overrides chip; no park mark in ui/src/monitor or fixturetreewidget.cpp ("Why is this on?" exists, which helps) | fits | **6** |
| **P-13** | In a cue's values, each value says how it relates to the previous cue: up (↑), down (↓), tracked (⤳, as now) or blocked (⊘). Each has its own colour *and* glyph, the same in the Show Tree, the cue page and the Workbench. | Eos: blue = higher than the previous cue, green = lower, magenta = tracked, white = blocked, red = manual and not stored [S1] | This is the core tracking display every Eos-trained operator reads without thinking. With it, "what does this cue actually change?" is answered at a glance | test `showtreeview_test::valuesMarkUpDownTrackedBlocked` | **Partly done.** Tracked is grey with ⤳ and its source (UX-22, show-tree.md). Up, down and blocked have no mark | fits | **6** |
| **P-14** | A change to a cue in a Tracking list says where it stops tracking ("carries into 2.2–2.5") and offers Cue Only before it is kept. | Eos Record / Update with a Track ⇄ Cue Only choice [S4]; grandMA3 tracking and cue-only store [S7] | The worst tracking accident is a colour change that silently runs through the rest of the act | test `showtreeview_test::aTrackingEditSaysHowFarItCarries` | **Unknown.** Track / Block / Cue-only modes exist on cues; I did not find a line that reports how far an edit carries before it is kept | fits | **6** |
| **P-15** | In Operate, nothing opens a modal dialog the operator didn't ask for, and with any dialog open ⌘↩ GO and ⌘⇧Esc Stop All still work. | Console GO keys work whatever is on screen. Microsoft: notifications or status bars, not modals, for problems with background tasks [S20] | A modal box that swallows GO mid-cue is a missed cue in front of an audience | test `appviews_test::goWorksWithADialogOpen` (open a `QMessageBox` on a timer, send ⌘↩, assert the cue advanced) | **Unknown.** There are 204 `QMessageBox` calls; the autosave failure already goes to the chip (good). Needs the test to find out | fits | **6** |
| **P-16** | A command or drop whose target lights nothing says so where it was typed ("Towers has no fixtures: nothing will change") and still does what was asked. | Eos and MA3 echo the selection on the command line, so an empty selection is visible before Enter | An empty group is the commonest "why isn't it working" in a classroom | test `showtree_test::aCommandOnAnEmptyGroupSaysSo` | **Broken in the shot.** `Towers colour Blue` ran on "Towers (0) — 0 fixture(s)" with no word (shots-041/cli/showtree-typo-didyoumean-1280x800.png) | fits | **6** |
| **P-17** | Storing onto a cue, palette or look that already has content asks Merge / Replace / Cancel, unless Settings already says which, and the result line says which it did. | grandMA3 Store pop-up: Overwrite / Merge / Remove / Release / Cancel, with a user preference [S6]; Avolites: "prompts for merge or overwrite if it already exists" [S16]; Eos separates Record from Update [S4] | Replace when Merge was meant wipes the cue's other fixtures, and nobody sees it until the next run | test `workbench_test::keepOntoAFullCueAsksMergeOrReplace` | **Unknown.** Workbench has Keep and "Update “name”"; I did not see a merge/replace choice | fits | **4** |
| **P-18** | Every screen still lays out without clipping when the text is 125% and 150% of normal size. | WCAG 1.4.4 Resize Text [S23]; Apple HIG typography | Operators over 50, and a laptop across a dark booth. A layout that only works at one font size breaks the first time someone asks for bigger text | test `layoutsanity_test::everyScreenAtLargerText` (same walk, app font × 1.25 and × 1.5) | **Unknown.** No text-size setting; layoutsanity runs at one font. The setting itself is feature F-08 | fits | **4** |
| **P-19** | Text and state colours meet WCAG contrast (4.5:1 for text, 3:1 for large text and state marks) in every theme, measured, not looked at. | WCAG 1.4.3 / 1.4.11 [S23] | This sharpens UX-37 into a number, so a theme or cue-colour change can't quietly drop below it | test: extend mainbar_test's WCAG `contrast()` to `walker` over every screen | **Partly done.** mainbar_test measures the footer; UX-37 checks named cases | fits | **4** |
| **P-20** | Any colour that means something has a word, letter or glyph beside it, so the screen reads the same in greyscale. | WCAG 1.4.1 Use of Color: colour is not "the only visual means" [S23]; Eos pairs colours with text letters (P, B, M, C) [S2] | About 1 man in 12 has a red/green deficiency, and stage light washes out screen colour | `walker`: grab each screen, desaturate, assert the state chips and badges still differ by text; `review` | **Mostly done.** Readiness says "Ready"/"Not ready", tracked has ⤳, the clash text says "clashes at". This guards it | fits | **3** |
| **P-21** | Undo and Redo name the step they will undo on every surface: the Edit menu, the Show Tree and Build toolbars, the pad. | Apple HIG Undo and Redo: name the action [S22]; grandMA3's Oops list shows each action [S5] | Pressing Undo blind mid-programming and losing the wrong thing is common; the name is the check | test `appviews_test::undoButtonsNameTheStep` | **Partly done.** The Edit menu says "Undo %1" (app.cpp:3074). The Show Tree toolbar shows a bare "Undo" (shot) | fits | **4** |
| **P-22** | A fixture that is merely *selected* and one that *has values in the edit* look different everywhere (selected = outline, has values = filled mark). | grandMA3: selected fixtures get a yellow name, programmer values get markers [S11]; Eos red = manual, not stored [S1] | "Selected" and "will be recorded" are different promises. Mixing them up records the wrong fixtures | test `selectionsync_test::selectedAndEditedLookDifferent` | **Unknown.** UX-43 makes one selection; I did not find a separate "has unkept values" mark outside Refine's "Next unsaved light" | fits | **4** |
| **P-23** | Anything shown only in a tooltip is also reachable without hovering: on the page, by F1, or in a details pane. | WCAG 1.4.13 Content on Hover or Focus [S23]; a touch remote (iPad, Steam Deck) has no hover | qlcconsole leans on hover for a lot ("Hover for the details", show-tree.md). That leaves keyboard users and the future tablet path without it | `review`; `walker` flags tooltips over 80 characters with no matching text on the page | **Partly done.** The cue page has "Tracked in" in full; other tooltips hold information found nowhere else | fits | **4** |
| **P-24** | While Blind is on, every view that shows blind values has the blind blue on its frame, not only the footer. | grandMA3 turns the frames of preview windows red and the frames of edited cues yellow [S11]; Hog users asked for a visible Blind marker [S15b] | Blind is already unmistakable at the footer. The frame keeps it unmistakable in a view undocked to a second monitor | test `appviews_test::blindFramesEveryBlindView` | **Mostly done** (UX-33, BlindStyle) | fits | **2** |
| **P-25** | Highlight, Flash and Locate change only the output, never a cue, palette or Workbench value, and turning them off returns everything exactly as it was. | Hog 4: Highlight "changes only the output, not the current cue, Programmer, or editor" [S15] | An identify tool that dirties the look is a trap | test `highlight_test::highlightLeavesTheShowUnchanged` (doc save before and after are byte-equal) | **Probably done**; this guards it | fits | **2** |
| **P-26** | A work area with nothing in it (a tree, list, plot or editor) shows one grey line naming the first action and its key. A chip or column with nothing to report stays hidden (UX-23). | QLC+ 5 and every console open on a usable empty state; this adapts the orchestrator's idea to UX-23 | Students and new operators start on an empty show | `walker` on a new show: each main view's empty area has a hint label | **Mostly done** ("No notes yet. Type one below…", "Drop a palette here…") | fits | **2** |

**Count: 26 proposals.** A score of 9 means it is very likely broken now and
the operator stakes are high. Start there.

## Parity gaps that are features, not rules

These need a new control surface, model or panel. They go on the Roadmap
page as candidates, each with an ROI line per STRATEGY.md, and are **not**
rules.

| # | Gap | Who has it | Attaches to | Note |
|---|---|---|---|---|
| F-01 | **Next / Back through the selection while highlighting** (one fixture at a time) | Hog 4 [S15], Eos [S3] | Build's Highlight button | Small. This is the focus-call workflow for one light at a time. |
| F-02 | **Lowlight / Rem Dim**: the unselected lights dim to a set level while one is highlighted | Eos Rem Dim [S3]; Hog 4 lowlight palette [S15] | Highlight | Small. Overlaps Solo; ask whether Solo is enough. |
| F-03 | **Undo history list** (hold Undo, or Edit ▸ Undo ▸, to see and undo back to a step) | grandMA3 Oops menu [S5] | the shared undo stack | Watch the menu-item count (UX-44). |
| F-04 | **Named screen layouts** for one or two monitors, recalled in one key | MagicQ Layouts [S13]; ONYX workspaces [S17] | Workspace views (Classic / qlcconsole / Everything) | Medium. Window positions already persist. |
| F-05 | **Sneak**: let go of manual changes over a time instead of snapping | Eos Sneak [S4] | Release / overrides chip "Clear All" | Small. Teaching value. |
| F-06 | **Capture-style fixture states** with their own letter on the object | Eos `C` captured [S2] | P-12 markers | Only if Park + Highlight aren't enough. |
| F-07 | **Screen dimmer**: one slider that dims the whole UI below the theme | grandMA3 Screens fader [S8]; ONYX display brightness [S17] | Settings ▸ Theme, or the footer | Small, booth value. |
| F-08 | **Text size setting** (100 / 125 / 150%) | WCAG 1.4.4; Apple HIG | Settings | Needs P-18 first. |
| F-09 | **Store options as a preference** (always Merge / always Replace / Ask) | grandMA3 [S6] | P-17 | Only after P-17. |

Already present, so not gaps: Park, Highlight, Flash, Locate, "Why is this
on?" (Eos About), Move in Black with auto mark and Show Review live-move
checks, command history (↑/↓), "Did you mean", Blind with a blue footer and
a blind plot, autosave with a visible "Autosaved hh:mm" footer and a failure
chip, window geometry kept on screen, Red Shift theme, Operate PIN, and
Remote GO off by default.

Out of scope by STRATEGY.md and not proposed: multi-user editing and
sessions (grandMA3 multi-station Oops), pixel mapping, an audio engine, and
QLC+ 5 parity. QLC+ 5's main-window layout of a top bar, a context bar and
side panels [S18] was compared for conventions only. It adds nothing that
the rows above don't already cover.

## Sources

Primary manufacturer and guideline pages unless marked. Pages are versioned
(MA 2.x, Eos online help), so the wording can move between releases.

- **[S1]** ETC Support: *Channels Are Stuck On and Have a Colored Value*, the Eos colour indicators (bright red manual, blue up, green down, magenta tracked, white blocked, grey unowned, yellow submaster). https://support.etcconnect.com/ETC/Consoles/Eos_Family/Software_and_Programming/Channels_Are_Stuck_On_and_Have_a_Colored_Value. Eos online help: *Color Indicators*. https://www.etcconnect.com/webdocs/Controls/EosFamilyOnlineHelp/en-us/Content/03_System_Basics/06_Live_and_Blind_Displays/Indicators_in_the_Live_and_Blind_Displays/ColorIndicators.htm
- **[S2]** ETC Support: *Text Indicators on Channels/Tombstones in Live and Blind* (P parked, C captured, B blocked, M mark, R broken reference…). https://support.etcconnect.com/ETC/Consoles/Eos_Family/Software_and_Programming/Text_Indicators_on_Channels%2F%2FTombstones_in_Live_and_Blind_on_Eos_Family_Consoles
- **[S3]** Eos online help: *Highlight and Lowlight* (Rem Dim). https://www.etcconnect.com/webdocs/Controls/EosFamilyOnlineHelp/en-us/Content/07_Manual_Control/04_Highlight_and_Lowlight/About_Highlight_and_Lowlight.htm
- **[S4]** Eos online help: *About Cues and Cue Lists* (Record / Update, Track and Cue Only). https://www.etcconnect.com/WebDocs/Controls/EosFamilyOnlineHelp/en/Content/12_Cues_and_Cue_Lists/About_Cues_and_Cue_Lists.htm. Sneak: Eos v1.3 manual p.118 (secondary mirror) https://www.manualsdir.com/manuals/559115/etc-eos-v13.html?page=132
- **[S5]** grandMA3 manual: *Oops* keyword and Oops menu (history list, Oops Confirmation Always / Main / Never). https://help.malighting.com/grandMA3/2.5/HTML/keyword_oops.html and https://help.malighting.com/grandMA3/2.3/HTML/ws_oops_overlay.html
- **[S6]** grandMA3 manual: *Store Cue* (pop-up Overwrite / Merge / Remove / Release / Cancel; store preferences). https://help.malighting.com/grandMA3/2.3/HTML/cue_store.html and https://help.malighting.com/grandMA3/2.4/HTML/cue_store_settings_preferences.html
- **[S7]** grandMA3 manual: *Tracking and Cue Only*. https://help.malighting.com/grandMA3/2.5/HTML/cue_tracking_cue-only.html. Clear key levels (selection, active, all): https://help.malighting.com/grandMA3/2.3/HTML/key_clear.html
- **[S8]** grandMA3 manual: *Color Theme* (default / defaultDAYLIGHT) and *Desk Lights* (Screens brightness fader). https://help.malighting.com/grandMA3/2.4/HTML/ws_colors_color_theme.html and https://help2.malighting.com/grandMA3/2.1/HTML/si_desklights.html
- **[S9]** grandMA3 manual: *Save* (AutoSave interval with countdown) and *Backup* (up to 10 backups per show, made on save). https://help.malighting.com/grandMA3/2.3/HTML/sfh_save.html and https://help.malighting.com/grandMA3/2.5/HTML/sfh_backup.html
- **[S10]** grandMA3 manual: *DMX Sheet* (Univ.addr or absolute address). https://help.malighting.com/grandMA3/2.3/HTML/patch_dmx_sheet.html
- **[S11]** grandMA3 manual: *Programmer* (Preview turns window borders red, editing cues and presets turns frames yellow, selected fixtures get a yellow name, value markers). https://help.malighting.com/grandMA3/2.3/HTML/operate_programmer.html
- **[S12]** ChamSys MagicQ manual: *System Management* (auto backup `.sbk`, "AutoS" in the status display, reload at start, save under several names). https://docs.chamsys.co.uk/magicq/manual/system_management.html
- **[S13]** ChamSys MagicQ manual: *Layout* (window layouts across monitors, stored in the show). https://docs.chamsys.co.uk/magicq/manual/console_layout.html. *Blind Visualisation*: https://docs.chamsys.co.uk/magicq/manual/blind_visualisation.html
- **[S15]** Hog 4 manual §16.3: *Highlight and Lowlight* (output only, Next/Back, lowlight palette). https://www.etcconnect.com/webdocs/Controls/HOG/HTML/en/sect-highlight_lowlight.htm. **[S15b]** Hog community request for a visible Blind marker (user forum, not official): https://community.etcconnect.com/control_consoles/hog-lighting-controls/f/hog-4-requests-and-enhancements/46372/feature-request-visual-marker-near-status-bar-for-blind
- **[S16]** Avolites Titan: *Titan Commands Quick Reference* ("records cue n and prompts for merge or overwrite if it already exists"). https://manual.avolites.com/docs/titan-reference
- **[S17]** Obsidian ONYX manual: *Views and Workspaces* (workspaces, "Setting Display Brightness"). https://support.obsidiancontrol.com/Content/Onyx_Manual/Views_and_Workspaces/Manual_Views_and_Workspaces.htm
- **[S18]** QLC+ 5 docs: *Main Window*. https://docs.qlcplus.org/v5/main-window
- **[S19]** Microsoft Windows UX guide: *Confirmations* ("Provide undo", "physically separate destructive commands", "Never use OK and Cancel for confirmations", safe default for risky actions). https://learn.microsoft.com/en-us/windows/win32/uxguide/mess-confirm
- **[S20]** Microsoft Windows UX guide: *Error Messages* ("inform users that a problem occurred, explain why it happened, and provide a solution"; status bars for background-task problems). https://learn.microsoft.com/en-us/windows/win32/uxguide/mess-error
- **[S21]** Apple HIG: *Alerts* (destructive style, a specific button title, Cancel). https://developer.apple.com/design/human-interface-guidelines/alerts. The page is script-rendered and I could not fetch its text, so this is a paraphrase from the HIG to be checked against the live page.
- **[S22]** Apple HIG: *Undo and Redo* (name the action). https://developer.apple.com/design/human-interface-guidelines/undo-and-redo. Same caveat as S21.
- **[S23]** WCAG 2.2 Understanding pages: 1.4.1 Use of Color https://www.w3.org/WAI/WCAG22/Understanding/use-of-color.html · 1.4.3 Contrast (Minimum) https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html · 1.4.4 Resize Text https://www.w3.org/WAI/WCAG22/Understanding/resize-text.html · 1.4.11 Non-text Contrast https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html · 1.4.13 Content on Hover or Focus https://www.w3.org/WAI/WCAG22/Understanding/content-on-hover-or-focus.html · 2.1.1 Keyboard https://www.w3.org/WAI/WCAG22/Understanding/keyboard.html · 2.4.3 Focus Order https://www.w3.org/WAI/WCAG22/Understanding/focus-order.html · 2.4.7 Focus Visible https://www.w3.org/WAI/WCAG22/Understanding/focus-visible.html · 4.1.2 Name, Role, Value https://www.w3.org/WAI/WCAG22/Understanding/name-role-value.html
- **[S24]** Apple HIG: *Dark Mode*. https://developer.apple.com/design/human-interface-guidelines/dark-mode
- **[S25]** Apple HIG: *Accessibility* (VoiceOver labels). https://developer.apple.com/design/human-interface-guidelines/accessibility
- **[S26]** Qt 6: *Handle plural forms* (`tr("%n fixture(s)", "", n)` with a numerus translation). https://doc.qt.io/qt-6/i18n-source-translation.html#handle-plural-forms

## If a proposal is approved

Follow [UX_RULES.md § How to add a rule](UX_RULES.md#how-to-add-a-rule).
Give it the next `UX-NN`, put its **Why** as "2026-11-06 parity check
(UX_RULES_PROPOSALS.md P-NN), approved <date>/<Q-number>", and add it to
`ux_rules.json`. Where this page says **Broken** (P-01 to P-05, P-07,
P-08, P-10, P-12, P-16), the fix ships in the same change as the rule, as the
rule book asks.
