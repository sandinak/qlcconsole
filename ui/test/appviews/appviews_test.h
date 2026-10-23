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
    /** Board Q32: the Functions window is hidden (one switch). Window → Functions
     *  lands on Build ▸ Scenes ▸ Classic with the list focused; no
     *  Function Manager is built; a show saved with it in front opens
     *  there; opening a function from outside Build loads it in Build's
     *  Workbench. */
    void functionsOpensBuildScenesClassic();
    /** "tab icon could be a construction hat": Build's tab and its
     *  Window-menu jump wear a hard hat, crisp at 1x and 2x. */
    void theBuildTabWearsAHardHat();
    /** Q68: Window ▸ Effects is Build ▸ Effects; no Effects window. */
    void windowEffectsIsBuildEffects();
    void helpOpensOnWhatIsInFront();
    void virtualConsoleKeysDontClashWithTheApp();
    void theRigOverviewOpensWithoutTakingYouToTheStudio();
    /* P7: the Run tab -- the full-screen cue runner (⇧⌘9 in qlcconsole). */
    void theRunTabIsTheCueRunner();

    /** "fetch and CACHE .. we cannot have remote stuff for a showfile on a
     *  production run": a show's definitions come from this computer. */
    void aShowsDefinitionsComeFromTheShowCache();
    /** Fixtures whose definitions are missing: Save asks first, naming the
     *  models (Cancel saves nothing); the footer's Fetch Definitions gives
     *  each back its definition once it is on this computer -- where the
     *  mode and channel count match -- and says which still have none. */
    void missingDefinitionsAreAskedAboutAndFetchedBack();

    /** "set the h,w,d by the configured measurement type": one Settings
     *  entry, kept for the person, read by the fixture editor too. */
    void settingsChoosesTheUnits();
    void settingsAdvancedHoldsAutoOpen();
    /** Q65: theme, toolbar style, the footer's chips and the speed masters'
     *  beat are in Settings ▸ Appearance, not the View menu, and set the
     *  same settings the View menu did. */
    void settingsAppearanceHoldsHowTheWindowLooks();
    /** Q65: View is only what you look at. */
    void theViewMenuIsOnlyViews();
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
    /** File ▸ Upgrade This Show… on a Classic show: offered (once) the
     *  qlcconsole view, as a quiet line with a button. */
    void upgradingAClassicShowOffersTheConsoleView();
    /** Q63: auto-open shutters for a show saved without the setting: off
     *  for a QLC+ 4.x show, on for one qlcconsole saved, on for a new show. */
    void autoOpenFollowsWhoSavedTheShow();

    /** Q64: nothing patched to any output -- a grey "No outputs: preview
     *  only", never red; red "Not ready" for a real problem once a universe
     *  has an output. */
    void theReadinessChipIsGreyWithNoOutputs();

    /** Quitting with the window still up and the Show Tree in front: the
     *  tab goes before the Doc, so its hide event (which stops its preview)
     *  doesn't land on a freed Doc. Last: it deletes the app. */
    void quittingWithTheShowTreeShowing();

private:
    App *m_app;
};

#endif
