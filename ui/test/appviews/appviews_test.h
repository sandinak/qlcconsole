/*
  Q Light Controller Plus
  appviews_test.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef APPVIEWS_TEST_H
#define APPVIEWS_TEST_H

#include <QObject>

class App;

class AppViews_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void buildHideShots();
    /** Show only (Output Rules): QLC_SHOT_DIR=dir -- the window at three
     *  sizes with two Show-only lasers (the footer chip, the plot badge), in
     *  Operate, and the Output Rules dialog */
    void showOnlyShots();
    /** The footer says why they are held, the plot badges them, Locate is
     *  refused, and the dialog sets the rule */
    void showOnlyIsSaidAndBadged();
    /** A control surface's Blind is the console's Blind button: in
     *  Operate never a mute of the live show */
    void aControlSurfaceBlindFollowsTheConsolesRule();

    /** Picking a view from the View menu switches to it even when an earlier
     *  tab has been detached and every index after it has shifted. */
    void jumpingToADockedViewSurvivesAnEarlierDetach();

    /** Picking a view that is ALREADY detached brings its own window forward
     *  instead of doing nothing, which is what an index-based jump did: there
     *  was no tab left to select. */
    void jumpingToADetachedViewRaisesItsWindow();

    /** "every time I open the rig view .. the studio tab comes up": the Rig
     *  Overview is a window of its own; opening it leaves the tab alone. */
    void theWindowMenuFindsAndPutsBackBrokenOutViews();
    /** "change the tab -> window from double click to shift click and/or
     *  enable/disable that feature via a toggle on window". */
    void shiftClickBreaksATabOutDoubleClickDoesNot();
    /** Board Q32 + Q73: the qlcconsole view has no Functions tab (one
     *  switch). Window → Functions opens the stock Functions tab, switching
     *  to Everything with the usual notice; a show saved with it in front
     *  doesn't switch view on open; opening a function from outside Build
     *  loads it in Build's Workbench. */
    void windowFunctionsOpensTheFunctionsTab();
    /** "tab icon could be a construction hat": Build's tab and its
     *  Window-menu jump wear a hard hat, crisp at 1x and 2x. */
    void theBuildTabWearsAHardHat();
    /** Board Q266: the Show Tree keeps its name and says what is in it --
     *  "cue lists, cues and hits" in the tab's tooltip, in its Window menu
     *  entry and in its help title. No other tab grows an aside. */
    void theShowTreeSaysItHoldsCuesAndHits();
    /** Q68: Window ▸ Effects is Build ▸ Effects; no Effects window. */
    void windowEffectsIsBuildEffects();
    void withBuildHiddenItsJumpsLandInTheShowTree();
    void helpOpensOnWhatIsInFront();
    void virtualConsoleKeysDontClashWithTheApp();
    void theRigOverviewOpensWithoutTakingYouToTheStudio();
    void noTwoApplicationShortcutsShareAKey();
    /* P7: the Run tab -- the full-screen cue runner (⇧⌘9 in qlcconsole). */
    void theRunTabIsTheCueRunner();
    /** Q81: Edit ▸ Undo acts on the tab in front -- Build, the Show Tree,
     *  the Lighting Studio -- and elsewhere says "Nothing to undo on this
     *  tab" (greyed) instead of undoing another tab's step. */
    void editUndoSaysWhereItWorks();
    /** Q78: Classic QLC+ looks like QLC+ -- its main toolbar, the stock
     *  Fixtures and Functions toolbars, and a footer of mode, GM and Ready;
     *  the qlcconsole view puts it all back. */
    void classicLooksLikeQlcPlus();

    /** "fetch and CACHE .. we cannot have remote stuff for a showfile on a
     *  production run": a show's definitions come from this computer. */
    void aShowsDefinitionsComeFromTheShowCache();
    /** Fixtures whose definitions are missing: Save asks first, naming the
     *  models (Cancel saves nothing); the footer's Fetch Definitions gives
     *  each back its definition once it is on this computer -- where the
     *  mode and channel count match -- and says which still have none. */
    void missingDefinitionsAreAskedAboutAndFetchedBack();
    void editUndoOnConnectionsIsUndoPatchChange();

    /** "set the h,w,d by the configured measurement type": one Settings
     *  entry, kept for the person, read by the fixture editor too. */
    void settingsChoosesTheUnits();
    void settingsAdvancedHoldsAutoOpen();
    void reopenLastShowDecision();
    void theUpdateChannelsAreStableAndBeta();
    void reopenLastShowCheckboxRoundTrips();
    /** Q65: theme, toolbar style, the footer's chips and the speed masters'
     *  beat are in Settings ▸ Appearance, not the View menu, and set the
     *  same settings the View menu did. */
    void settingsAppearanceHoldsHowTheWindowLooks();
    /** Findability (board Q65, commit 385edaab9): the footer chips moved
     *  here from the View menu, but nothing tested that a person could
     *  still find them. Walks the real menu path -- File ▸ Settings…, the
     *  Appearance tab, the Footer group -- and opens the dialog through the
     *  menu action, not the AppSettings constructor directly. */
    void footerChipsAreFindableFromSettingsAppearance();
    /** Q65: View is only what you look at. */
    void theViewMenuIsOnlyViews();
    /** Q150: the venue chip in the footer, by view; View ▸ Venue… and
     *  Focus ▸ Venues ▸ open the same menu; the chip follows a switch. */
    void theVenueChipIsInTheFooter();
    /** The grid follows Settings -> Units: an old show in feet seeds an
     *  unset preference once, a show opens in the person's unit, and is
     *  saved with Units from the preference */
    void theGridFollowsTheUnitPreference();

    /** The cue page's drop area says what it is for in every theme the app
     *  ships: its words stand out from the page (they were palette(mid),
     *  all but the page's own colour on the dark themes). */
    void theDropAreaReadsInEveryTheme();
    /** A skipped cue's row in the Show Tree (greyed, struck through, ⦸)
     *  still reads in every theme; QLC_SKIPCUE_THEMES (a folder) keeps a
     *  picture of the tree in each. */
    void aSkippedCueReadsInEveryTheme();
    void textColoursReadInEveryTheme();

    /** The rig network gate: the footer's red "Output held" says where the
     *  show belongs and where this machine is; Settings -> Network edits the
     *  show's rig networks (validated) and this machine's check. */
    void rigNetworkFooterSaysWhyOutputIsHeld();
    void rigNetworkSettingsRoundTrip();

    /** Board Q35/Q36: View ▸ Workspace -- Classic QLC+, qlcconsole,
     *  Everything -- each its own tabs in its own order; hidden tabs live
     *  on; the Settings dialog shows the show's view. */
    void eachWorkspaceViewShowsItsTabs();
    /** A Window-menu jump to a tab the view hides shows Everything, with a
     *  note and a way back; Window → Functions in qlcconsole is Build. */
    void aJumpToAHiddenTabShowsEverything();
    /** Q69: ⇧⌘1… number the tabs the view shows, in its order, and are
     *  renumbered when the view changes; the Window menu lists them so,
     *  then the hidden tabs with no shortcut. */
    void windowShortcutsFollowTheView();
    /** A view in a window of its own stays there through view switches,
     *  and goes back to its place (or is put away) when closed. */
    void aBrokenOutViewSurvivesAViewSwitch();
    /** The view is saved with the show, as an attribute older readers skip;
     *  switching marks the show modified but is no undo step. */
    void theViewIsSavedWithTheShow();
    /** No view saved: an old QLC+ show opens in Classic QLC+, a show with a
     *  Show Tree (or saved by qlcconsole) in qlcconsole; a new show in
     *  qlcconsole. */
    void aShowWithoutAViewOpensInItsDefault();
    void theTabBarDefaultsPerView();
    void aCustomTabSetRoundTripsThroughTheShow();
    void runAndConnectionsCantBeUnchecked();   // and every other box works from a click
    /** Findability (board Q293): the feature shipped with tests for what it
     *  does but none for whether a person can find it. Walks the real menu
     *  path -- View ▸ Workspace ▸ Tabs & Features…, its text, its place in
     *  the Workspace submenu -- and triggers the menu action itself (not
     *  slotChooseTabs() by hand) to check it opens the real dialog. */
    void tabsAndFeaturesIsFindableFromViewWorkspace();
    /** Q293: View ▸ Workspace ▸ Tabs & Features… turns each feature off for
     *  the show -- its footer chip, menu items and pages put away -- and on
     *  again brings them back; the show is marked changed. */
    void eachFeatureSwitchHidesWhatItOwns();
    void everyFeatureSwitchRespondsThroughTheDialog();
    void turningVenuesOffAsksWhileAVenueIsApplied();
    /** Q293: the features turned off are saved with the show; a show saved
     *  with all on (an older show: no attribute) opens with everything on. */
    void featuresAreSavedWithTheShow();
    /** Q293: with every feature off the emergency stop is still there and
     *  still fires ALL OFF; Control ▸ Panic too. */
    void theEmergencyStopSurvivesEverySwitchOff();
    /** Phase F: one ALL OFF path (screen, VC, desk, pad) through ShowControl::allOff(). */
    void everyAllOffTakesTheOnePath();
    /** Phase F: Stop All is never greyed, so held levels can always be stopped. */
    void stopAllIsNeverGreyed();
    void stopAllTakesOutAHeldLevel();
    /** Review: ... but pan and tilt held by the desk or a slider stay put. */
    void stopAllLeavesAHeldHeadWhereItIs();
    /** Phase F: no Ctrl+Shift+Esc on Windows (Task Manager); the Mac keeps ⌘⇧Esc. */
    void theStopAllKeyIsSafeOnEveryPlatform();
    /** Phase F: the desk STOP goes before every feature / mode / learn check. */
    void theDeskStopGoesFirstPastEveryCheck();
    /** Q293 review: the GM below full or a speed master off its tempo is
     *  said in the footer while its chip is put away. */
    void aHeldMasterIsSaidWithItsChipPutAway();
    /** Q293 review: a venue applied shows its chip with Venues off. */
    void anAppliedVenueIsSaidWithVenuesOff();
    /** Q293 review: with Control Surfaces off the Run tab's Pad button stays
     *  while the pad has a state (Stopped after the emergency stop). */
    void thePadButtonStaysWhileThePadHasAState();
    /** Q293 review: Classic opens Tabs & Features (tabs greyed, features
     *  usable); its footer right-click offers only the Grand Master. */
    void classicCanStillTurnFeaturesBackOn();
    /** Q293: no switch for the follow spot -- the Rig Overview's Operator
     *  views appear once a scene has a Followspot look, and go with it. */
    void theFollowSpotAppearsOnceTheShowHasOne();
    /** Q293: a right-click on the footer shows or hides a chip, the same
     *  setting Settings ▸ Appearance shows; a chip whose feature is off for
     *  the show is greyed in both. */
    void rightClickingTheFooterTogglesAChip();
    /** Q293 pictures (QLC_SHOT_DIR): Tabs & Features over the window and the
     *  footer's right-click menu, at three sizes. */
    void tabsAndFeaturesShots();
    void simpleDeskOpensAndClosesOnDemand();
    /** File ▸ Upgrade This Show… on a Classic show: offered (once) the
     *  qlcconsole view, as a quiet line with a button. */
    void upgradingAClassicShowOffersTheConsoleView();
    /** P45: Classic QLC+ with Show Tree content on the Virtual Console (a
     *  Cue Runner, a button on a Show Tree list): a footer line and a button
     *  to the qlcconsole view; none without such content or in another
     *  view; × puts it away until the show is next opened; it still plays,
     *  and Classic's tabs stay stock. */
    void aShowTreeShowSaysSoInClassic();
    /** Q63: auto-open shutters for a show saved without the setting: off
     *  for a QLC+ 4.x show, on for one qlcconsole saved, on for a new show. */
    void autoOpenFollowsWhoSavedTheShow();

    /** Q64: nothing patched to any output -- a grey "No outputs: preview
     *  only", never red; red "Not ready" for a real problem once a universe
     *  has an output. */
    void theReadinessChipIsGreyWithNoOutputs();
    /** Q92: GO in Design offers Operate once a session, in the notice bar. */
    void goInDesignOffersOperateOnce();
    /** Q92: ...and GO in Design still plays: the list runs, its first
     *  cue's level on the output. */
    void goInDesignStillPlays();
    /** RC8 u5: GO (⌘↩) holds the Show Tree preview first -- only the run
     *  cue's values reach the universe -- says so, and Play brings it
     *  back; a busked scene (the VC, Simple Desk) keeps running. */
    void goHoldsThePreviewSoOnlyTheRunPlays();
    /** The footer's stage chip: LIVE / Blind / Build look / nothing / holds. */
    void theFooterSaysWhatLightsTheStage();
    void theFooterRightSideSharesOneLine();
    /** The one GO path (docs/design/RUN_COMMANDS.md): the keys, the Run
     *  tab (button and Space), a VC Cue Runner, the pad's chaser step and
     *  OSC all end in App::runCommand(), each with its route; the footer
     *  only shows the Run tab. Only the console's routes speak. */
    void everyRouteTakesTheOneGoPath();
    /** ...and a GO that finds nothing to play holds no preview. */
    void aGoThatFindsNothingHoldsNoPreview();
    /** The phone Run page's GO (RouteWeb) is refused until Show Settings
     *  allows remote GO; every other route is not asked. */
    void aWebGoIsRefusedUntilTheShowAllowsIt();
    /** Plain Esc is no playback action: in the main window, the Show Tree
     *  and the Workbench it leaves a function and the main list running;
     *  ⌘⇧Esc (Stop all) stops them. */
    void plainEscIsNoPlaybackAction();
    /** Q81 per tab: Build undoes the shared history's step; the Lighting
     *  Studio and Fixtures undo one rig history either way round; Simple
     *  Desk and Run say "Nothing to undo on this tab" and leave a Show
     *  Tree step alone. */
    void editUndoFollowsTheTabInFront();
    /** Q81: renaming a fixture in the Fixtures tree (Return, type, Return)
     *  is a rig undo step: Edit ▸ Undo puts the old name back. */
    void renamingAFixtureInTheTreeIsARigStep();
    /** Settings ▸ Advanced ▸ Output: when "On when not in use" fixtures
     *  come on -- OK sets the show's choice (saved with it), Cancel leaves
     *  it. */
    void settingsAdvancedHoldsOnWhenNotInUse();
    /** The Show Tree and Run tabs wear their own icons (a cue tree, a green
     *  GO button), not the chaser's. */
    void theShowTreeAndRunWearTheirOwnIcons();
    void aControllerComingAndGoingIsSaid();
    /** "resize still has problems": no qlcconsole page holds the window
     *  wide (a page narrower than it is cut off on the right). */
    void pagesSquashWithTheWindow();
    /** M1 (RC6): Connections ▸ Universes lists the open show's universes,
     *  no more: opening shows (File ▸ Open: clearDocument() + loadXML())
     *  added a show's worth of rows each time, and a rename went by row
     *  number, to another universe. */
    void theUniverseListMirrorsEachShowOpened();

    /** 0.3.1 (k), the QLC+ 4 home: Save As offers QLC+ 4's .qxw, starting
     *  on it for a .qxw show, and a .qxw save still works. */
    void saveAsOffersTheQlcPlus4Format();
    /** Full Screen is back: View ▸ Full Screen, Ctrl+F11 (⌃⌘F on macOS),
     *  Classic's toolbar, and the -f flag's slot. */
    void fullScreenIsBack();
    /** The DMX Monitor (QLC+ 4's Monitor) opens from Classic's toolbar and
     *  View, and shows each patched fixture's channels. */
    void theDmxMonitorIsInClassic();
    /** Settings ▸ General ▸ Start new shows in the Classic view. */
    void newShowsCanStartInClassic();
    void qlcPlus4SettingsAreBroughtOverFromSettings();
    /** A .qxw save holding what QLC+ 4 drops says so once, after saving. */
    void aQxwSaveSaysWhatQlcPlus4Leaves();
    /** a0: "Always Start with the Plot Locked" on: a show saved unlocked
     *  opens with its plot locked, not marked changed; off, as saved. */
    void aShowOpensLockedWhenTheUserAsks();
    /** File ▸ New from Template: the classroom stage, untitled and
     *  unchanged, with nothing patched to an output; the open show is
     *  asked about first. */
    void newFromTemplateOpensTheClassroomStageUntitled();
    /** File ▸ Show Settings: a PIN keeps the show in Operate (and Live Edit
     *  off) until it's given; "Open in Operate" opens the saved show there. */
    void aPinKeepsTheShowInOperate();
    /** Operate puts a lock on the Connections and Fixtures tabs; their Design button
     *  goes through the show's PIN. */
    void operateLocksThePatchTabsAndTheirDesignButtonAsksForThePin();
    void openInOperateWithoutAPinLoadsInOperateAndLeavesFreely();
    /** Teaching mode (docs/features/teaching.md): the chip and the title in
     *  every view, the scramble only in a teaching show */
    void teachingModeShowsItselfAndThePractice();
    /** Show Review's footer chip: hidden until a run, counts what's open,
     *  re-checks itself after an edit, a click opens the panel; Fix goes to
     *  the Show Tree cue */
    void reviewChipCountsAndOpensThePanel();
    /** Turning it off, or letting real output through, asks for the show's
     *  PIN; a wrong one leaves it on; no undo step */
    void teachingModeIsGuardedByThePin();
    void theViewNoticePushesThePageDown();
    /** Operate in a teaching show: a quiet notice and a Show Status entry,
     *  no dialog; the dialog only when real output is let through and patched */
    void teachingOperateWarnsQuietlyThenAsksWhenItLooksReal();
    /** Make Student Handout: the copy has Teaching and the PIN, and the
     *  open show is untouched; autosave and the show file keep the flag */
    void aStudentHandoutIsACopy();
    /** Reset to Handout puts a student copy back to the handout, behind the PIN */
    void aHandoutResetsToItself();
    /** A Teaching show asks before its patch changes; others do not */
    void teachingAsksBeforePatchChanges();
    /** Save As ⇧⌘S and Quit ⌘Q on every platform, owned by nothing else;
     *  Undo/Redo show the tabs' keys without binding them (so they never
     *  fire twice); Settings… is called that everywhere. */
    void theMenusShowTheirKeys();

    /** Teaching: a show opened with "Open in Operate" (from a file, and as the
     *  web page loads one) goes to Operate with real output let through and
     *  patched, without the "Run this show for real?" question */
    void aLoadedTeachingShowOpensInOperateWithoutAsking();

    /** The footer's "Unsaved changes": a click lists what changed since the
     *  last save (cue rename, a look colour, a target), grouped by thing,
     *  each with a jump; Save clears it; Undo takes an entry away. */
    void unsavedChangesListsWhatChanged();

    /** Cmd/Ctrl+D on the Lighting Studio's plot, in the whole main window
     *  (where Control > Dump DMX has Ctrl+D too): it duplicates the
     *  selection, and neither stalls on an ambiguous shortcut nor dumps. */
    void ctrlDOnThePlotDuplicates();

    /** W1-27: Cmd/Ctrl+D on a selected Show Tree cue, in the whole main
     *  window: it duplicates the cue (Dump DMX's Ctrl+D made it ambiguous,
     *  and neither fired). Nothing selected: Dump DMX has the key. */
    void ctrlDOnTheShowTreeDuplicates();
    /** WF1-A: Cmd/Ctrl+D in Build's Palettes, Effects and Scenes lists
     *  duplicates the selection; Cmd/Ctrl+C then V in Effects copies the
     *  effect (it copied the outline's scene). */
    void ctrlDInBuildListsDuplicates();
    /** WF1-A: the Fixtures tab: Cmd/Ctrl+D a fixture -> a new one like it,
     *  unpatched in the Inventory; a group -> a copy; C then V the same;
     *  W2-Q2: Esc clears the selection. */
    void editKeysInTheFixturesTab();
    /** W1-10 / WF1-A: on the plot, Backspace takes every selected fixture off
     *  (they stay in the show), one undo step; F2 renames in Plot layers;
     *  Cmd/Ctrl+F to Plot layers' search; Esc lets go of the selection. */
    void editKeysOnThePlot();
    /** WF1-A: Plot layers: Cmd/Ctrl+D duplicates, Backspace takes it off the
     *  plot, Cmd/Ctrl+F to the search, Esc clears the list and the plot. */
    void editKeysInPlotLayers();
    /** W2-Q2: Esc backs out first (the Show Tree's Look Editor), and only
     *  then clears the selection; in Build the same. */
    void escBacksOutThenClearsTheSelection();

    /** Q236: Operate's Workbench is Blind; Play Live said in the footer. */
    void workbenchPlayLiveIsSaidInTheFooter();

    /** CS1: the PMJ overlay hears only its own board, and edits nothing in Operate. */
    void aPmjOverlayHearsOnlyItsOwnBoard();
    void aPmjControlTheDeskHasIsNotAlsoThePmjs();
    void aPmjFaderOrEncoderInOperateEditsNothing();
    void masterMoveKeepsShowLamps();
    /** The PMJ's Go and Back are the Run tab's GO and Back (one run path,
     *  the double-GO guard), and leave a key the show learned to the show. */
    void pmjGoRunsTheRunTabList();
    void pmjBackStepsBack();
    /** Go and Back lit (the profile's 127) when the Run tab's GO / Back
     *  would do something, dim (15) when not, dark with no cue list. */
    void pmjGoLampLitWhenReady();
    void aSecondPmjBoardIsNamedNotIgnoredInSilence();
    void aRefusedSurfacePressSaysSoOnTheFooter();
    /** Q313: the Grand Master follows its fader at once, nothing waits. */
    void theGrandMasterFollowsItsFaderAtOnce();

    /** Quitting with the window still up and the Show Tree in front: the
     *  tab goes before the Doc, so its hide event (which stops its preview)
     *  doesn't land on a freed Doc. Last: it deletes the app. */
    void quittingWithTheShowTreeShowing();

private:
    /** Two lasers on the plot, Show only; their ids */
    QList<quint32> addShowOnlyLasers();
    void removeFixtures(const QList<quint32> &ids);

    App *m_app;
};

#endif
