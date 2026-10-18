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
    void functionsOpensFromTheWindowMenuNotATab();
    void helpOpensOnWhatIsInFront();
    void virtualConsoleKeysDontClashWithTheApp();
    void theRigOverviewOpensWithoutTakingYouToTheStudio();
    /* P7: the Run tab -- the full-screen cue runner, ⇧⌘0. */
    void theRunTabIsTheCueRunner();

    /** "fetch and CACHE .. we cannot have remote stuff for a showfile on a
     *  production run": a show's definitions come from this computer. */
    void aShowsDefinitionsComeFromTheShowCache();

    /** "set the h,w,d by the configured measurement type": one Settings
     *  entry, kept for the person, read by the fixture editor too. */
    void settingsChoosesTheUnits();
    /** The grid follows Settings -> Units: an old show in feet seeds an
     *  unset preference once, a show opens in the person's unit, and is
     *  saved with Units from the preference */
    void theGridFollowsTheUnitPreference();

private:
    App *m_app;
};

#endif
