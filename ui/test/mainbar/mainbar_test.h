/*
  Q Light Controller Plus
  mainbar_test.h

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

#ifndef MAINBAR_TEST_H
#define MAINBAR_TEST_H

#include <QObject>
#include <QTemporaryDir>

class App;

class MainBar_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** Each tab with tools of its own lends them to the one top bar while it
     *  is on show, all of them, and gets them back after */
    void everyTabsToolsAreInTheTopBar();
    /** A tab moved to a window of its own keeps its tools at its own top */
    void aDetachedTabKeepsItsTools();
    /** Stop all / Blackout / Blind / Design|Operate, in the top bar and in
     *  the status bar: the same actions either way */
    void theShowControlsWorkInBothPlacements();
    /** Settings → Show Controls: saved, read back, applied at once */
    void thePlacementRoundTripsAndSwitchesLive();
    /** Blackout lit red, Blind blue, the switch on its mode: readable in
     *  all six themes */
    void blackoutAndBlindLightUpInEveryTheme();
    /** The point of it all: the show-control row's height (about 50 px)
     *  more for the tab's content */
    void theTopChromeIsAtLeast50PxSmaller();
    /** A narrow window folds the tab's tools into », never the show
     *  controls */
    void aNarrowWindowFoldsTheTabsTools();
    /** The Show Tree's drop hint stays in sight at a laptop's width: the
     *  buttons fold into », not the hint (it says where a drag will go) */
    void theShowTreesHintIsNeverFolded();
    /** Operate hides the Virtual Console's tools, in the top bar as at home */
    void operateModeStillHidesTheConsolesTools();
    /** Blind never survives into a show: Operate turns it off and greys it;
     *  Design gives the button back. */
    void operateClearsBlindAndGreysIt();
    /** Stop all's menu, Fade 1 second and stop: the scene stops and its
     *  light fades out over the second rather than snapping off */
    void fadeOneSecondAndStopFadesTheLightOut();
    /** Q80: Show-mode Lock is part of Operate (no menu item, no ⌘L):
     *  Operate makes the Shows timeline read-only and drops programmer
     *  parameter faders, while a console GM fader still works; Design
     *  gives it all back */
    void operateLocksProgrammingNotTheGrandMaster();
    /** Q80: one footer chip lists what holds the rig over the show (Park,
     *  Hold at end, Blind...), hidden when nothing does; a click lets go of
     *  all of it */
    void oneChipListsAndClearsOverrides();
    /** GO / Back (Ctrl+Return, Ctrl+Shift+Return) from anywhere, in the
     *  Control menu: they step the main cue list 1, 2, back to 1 */
    void goAndBackStepTheMainCueList();
    /** Q74: the footer's Next: is status only (no GO menu); it says what GO
     *  plays, a loaded sub-cue included, and a click brings Run forward */
    void theFootersNextIsStatusOnly();
    /** With a Show Tree production current, the footer says whose GO it is */
    void theFooterNamesTheCurrentProduction();
    /** The footer's Grand Master: moving it sets the GM and dims the rig */
    void theFooterGrandMasterDimsTheRig();
    /** Q89: the power estimate only for a show with its power set up */
    void thePowerChipNeedsPowerSetUp();
    /** Auto move-in-black: one setting of the show, three views (Show
     *  Tree MIB, Programming's Move in black menu, Settings → Show Tree),
     *  each following the others, saved and loaded with the show */
    void moveInBlackIsOneSettingInThreePlaces();
    /** Pictures for the record (MAINBAR_SHOTS=dir) */
    void screenshots();
    /** Kiosk: no top bar, no show controls, the tabs' tools at home. Last:
     *  it takes the tabs away. */
    void kioskModeDropsTheBar();

private:
    int topChrome();
    quint32 addDimmerScene(quint32 *fixtureId);
    void removeAddedFunctionsAndFixtures();
    void showTab(const QString &label);

    App *m_app = nullptr;
    QTemporaryDir m_dir;
    QList<quint32> m_addedFunctions;
    QList<quint32> m_addedFixtures;
};

#endif
