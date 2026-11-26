/*
  Q Light Controller Plus
  levels_test.h

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

#ifndef LEVELS_TEST_H
#define LEVELS_TEST_H

#include <QObject>

class App;

/** Workspace levels, phase L1 (docs/design/WORKSPACE_LEVELS.md §6; board
 *  Q249, Q340-Q344): the model, the registry, the menus and the tabs. */
class Levels_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    /** Saved as Level="student" on <Workspace>; nothing at Standard; a
     *  value a newer build wrote is read as Standard and written back. */
    void theLevelRoundTripsAndQlc4IgnoresIt();
    /** A show with no Level opens at Standard, with every menu item. */
    void anOldShowOpensAtStandard();
    /** Turning Teaching on sets Student; going up asks the PIN once, lasts
     *  until the show is closed and is never saved (Q341, Q342). */
    void teachingSetsStudentAndThePinUnlocksForTheSession();
    /** In a Teaching show going down is for the session too: the saved
     *  level stays, the doc is not modified, and going back up to the
     *  saved level asks nothing (round 1 review). */
    void aTeachingShowsSavedLevelSurvivesGoingDown();
    /** Teaching turned off keeps the level on screen and saves it. */
    void turningTeachingOffKeepsTheLevelOnScreen();
    /** A tab the level hides leaves a window of its own (parked). */
    void aLevelHiddenTabLeavesItsWindow();
    /** Make Student Handout writes the level its Level box says. */
    void theHandoutCarriesItsLevel();
    /** The menu bar's top-level count: Student <= 52, Standard <= 68
     *  (WORKSPACE_LEVELS §3, was 70). */
    void theMenuCountFallsOrStaysFlat();
    /** Build, Functions, Simple Desk and the Timeline leave the tab bar at
     *  Student; the Window menu's More Tabs line names them and switches. */
    void aStudentShowSavedOnFunctionsOpensQuietly();
    void aHiddenTabIsInTheWindowMenuSwitchLine();
    /** Stop All, GO, Back, Blackout, Blind, the Grand Master and the
     *  readiness footer stay at every level; bind() refuses them. */
    void safetyControlsAreNeverHidden();
    /** Every item the level folds has its menu's More line; it raises the
     *  notice with Switch, and Switch brings the item back (Q249: never a
     *  dead end). */
    void everyLevelHiddenItemHasASwitchPath();
    /** Every bound key has a row and every row (but the follow-ups) is
     *  bound somewhere in ui/src. */
    void theRegistryIsWhole();
    /** Classic acts as Advanced; a Student show there switches to the
     *  qlcconsole view and offers Back to Classic QLC+ (Q344). */
    void classicActsAsAdvancedAndStudentLeavesIt();
    /** L2, the course path: the Show Tree's, Fixtures' and the Lighting
     *  Studio's rows grey at Student with the level's words and come back at
     *  Standard, whatever re-enables them in between. */
    void theL2RowsGreyAndSayTheirLevel();
    /** A command-line word, a double-click and a shortcut for a hidden item
     *  answer in the notice line with the level's words, never silently. */
    void aHiddenItemReachedByCommandOrShortcutAnswersOnScreen();
    /** The focus report's Teaching line names the level. */
    void theFocusReportsTeachingLineNamesTheLevel();
    /** Every course start and finish file is saved at its lesson's level;
     *  what the lesson's student page names is not hidden at that level;
     *  and what 3.5 and 3.6 need is reachable at Standard. */
    void everyLessonOpensAtItsLevelAndItsStepsAreReachable();
    /** The window at each level, at three sizes (QLC_SHOT_DIR). */
    void levelShots();

private:
    App *m_app = nullptr;
};

#endif
