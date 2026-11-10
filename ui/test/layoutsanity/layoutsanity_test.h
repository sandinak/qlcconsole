/*
  Q Light Controller Plus - Test Unit
  layoutsanity_test.h

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

#ifndef LAYOUTSANITY_TEST_H
#define LAYOUTSANITY_TEST_H

#include <QObject>
#include <QStringList>

class App;
class QWidget;

/* Every screen the app builds -- the tabs, the editors, the dialogs, the
   panels -- opened at its minimum, a laptop (1280x800) and a wide
   (1920x1080) size, offscreen, and checked for widgets laid over one
   another, cut off by their window, text cut short and controls squeezed
   to nothing (board Q207). One row per screen in screens(); see
   docs/dev/TEST_MAP.md ("Adding a screen to layoutsanity_test"). */
class LayoutSanity_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** The checker itself: a hand-made overlap, a cut-off button and a
     *  squeezed label are found; a stacked page, a scroll area's content
     *  and a marked overlay are not. */
    void theCheckerFindsWhatItShould();

    /** Fresh settings, 1280x800 and 1024x640: the Show Tree's pane shows a
     *  cue's number AND its name (not "0.0.2 ..."), and the page beside it
     *  keeps room (wfaudit W1-28). */
    void theShowTreeShowsCueNamesWithFreshSettings();

    /** The Lighting Studio editors (Tower, Truss, Platform, Pipe) on a
     *  1024x640 laptop: the dialog fits the screen with OK and Cancel in
     *  view, its sides scroll, nothing in it is cut or laid over another,
     *  and the plot's edge labels are clear of its measurements (UX-34/36). */
    void theStudioEditorsFitALaptop();

    /** A tool button whose menu opens on a click ("+ Effect ▾") is wide
     *  enough for its text and its arrow, and draws the text clear of the
     *  arrow, under the app's style (UX-34). */
    void menuButtonsKeepTheirArrowOffTheText();

    /** Scene group looks at its smallest: a look's name is not cut to a
     *  letter beside the fades (UX-36). */
    void theSceneGroupLooksShowNamesAtTheSmallest();

    /** The footer at 1024 wide and at the window's own minimum: no two
     *  chips overlap, the stage chip and Unsaved stay (wfaudit W1-30). */
    void theFooterChipsNeverOverlap();

    /** The tab strip in Auto mode, in each tool view at 1920 to the window's
     *  minimum: every tab whole (no scroll buttons, no elision), Run in
     *  view, full names before short before icons as the width falls, a
     *  tooltip with the full name on every tab, and the explicit modes
     *  unchanged. TABFIT_SHOTS=dir (+ TABFIT_TAG) saves pictures. */
    void theTabStripFitsTheWindow();

    /** Every screen at every size: no overlaps, nothing clipped. */
    void everyScreenIsLaidOutCleanly_data();
    void everyScreenIsLaidOutCleanly();

private:
    App *m_app = nullptr;
    QStringList m_seenKnown;
};

#endif
