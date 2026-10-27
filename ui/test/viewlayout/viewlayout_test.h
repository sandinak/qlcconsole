/*
  Q Light Controller Plus
  viewlayout_test.h

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

#ifndef VIEWLAYOUT_TEST_H
#define VIEWLAYOUT_TEST_H

#include <QTemporaryDir>
#include <QObject>

class App;

/* "Not using the entire screen. Note the sidebar. We need a test for this;
   it's happening often now." The whole window, offscreen, in every view and
   on every tab, at a few window sizes: geometry, not pixels. */
class ViewLayout_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** Every tab of every view (Classic QLC+, qlcconsole, Everything) fills
     *  the window at 1280x800, 1920x1080 and 2560x1440: nothing stops short
     *  of the right or bottom edge, no splitter pane is shown at no width,
     *  and no pane is left empty but for a strip of something. */
    void everyTabFillsTheWindow();

    /** Every tab of the qlcconsole and Everything views, opened in its own
     *  window: the main window still fills as it is resized smaller and
     *  larger (through its platform window and by hand), between tab
     *  switches; the page fills its own window; and closing that window puts
     *  it back, the main window still following. Two out at once, too. From
     *  a real session's saved window and splitter states. */
    void theWindowFollowsResizesAroundADetach();

    /** Classic QLC+: picking a scene, a chaser or an EFX in the Functions
     *  tab opens its editor beside the tree, as in QLC+ -- even from a saved
     *  splitter state that had the editor collapsed. */
    void classicFunctionsOpensWhatIsPicked();
    /** The same from a saved state that collapsed the tree: picking a
     *  function shows both panes at a usable width. */
    void classicFunctionsUncollapsesTheTree();

    /** A scene the Show Tree made opens in its editor too (greyed, the
     *  banner saying why above it), not a banner at the foot of an empty
     *  pane; elsewhere a double-click opens it, the same way. */
    void aShowTreeSceneOpensGreyedUnderItsBanner();

    /** The row being edited reads in every theme: its text against its
     *  highlight (it was the theme's orange on a light blue). */
    void theEditedRowReadsInEveryTheme();

    /** Classic QLC+ lists none of the Show Tree's own functions (its layer
     *  scenes, cue Collections and cue-list chasers) -- "Show the Show
     *  Tree's own functions" lists them all; the other views list what is
     *  visible, as QLC+ does. The user's own scenes are always there. */
    void classicHidesTheShowTreesOwnFunctions();

    /** The scene editor: General (fixtures, channel groups, looks) in the
     *  top pane, the fixture tabs with their sliders below, a splitter
     *  between; many channels scroll sideways. A saved split that hid a
     *  pane, or one that isn't a split, is put right. */
    void sceneEditorHasGeneralOverTheSliders();

    /** A scene of looks on fixture groups (made in Build) lists them in
     *  General, so it doesn't read as an empty scene. */
    void sceneEditorListsAScenesLooks();

    /** Shift + wheel over a fader scrolls the row sideways (and leaves the
     *  fader alone); a drag on the space between faders pans it. */
    void sliderRowsScrollSideways();

    /** Settings ▸ Appearance ▸ Always show scroll bars: the app's style
     *  never hides a scroll bar, saved, and off again restores the style. */
    void alwaysShowScrollBarsSetting();

private:
    App *m_app = nullptr;
    QTemporaryDir m_dir;
};

#endif
