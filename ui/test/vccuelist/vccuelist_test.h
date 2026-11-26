/*
  Q Light Controller
  vccuelist_test.h

  Copyright (C) Heikki Junnila

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

#ifndef VCCUELIST_TEST_H
#define VCCUELIST_TEST_H

#include <QObject>

class Doc;
class VCCueList_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Show only: a cue list's Next on a list running from a preview
     *  makes it the show (its gate opens), restarting nothing */
    void aNextOnAListRunningFromAPreviewIsTheShow();
    void initTestCase();
    void init();
    void cleanup();

    void initial();
    void chaser();
    void functionRemoved();
    void functionChanged();
    void keySequences();
    void copy();
    void modeChange();
    void loadXML();
    void saveXML();

    void nextPrevious();
    void aGoWaitingForItsBeatShowsOnNext();
    void cuesSkipsAndNextAreShown();
    void showTreeTextColoursAreShown();
    void showTreeListGoesThroughTheOneGoPath();
    void showTreeSubGoAndBackGoThroughShowControl();
    void plainChaserStillStepsDirectly();
    void manualActivation();
    void keyboardNextPrevious();
    void submasterWorksAfterSliderModeRoundTrip();

    void input();

    /** Song sections (Q190): the Run tab's section rows, in order, between
     *  the cues; not cues themselves. */
    void songSectionsShowAsRowsInOrder();
    /** Next / Previous / GO pass a section row; a click on one picks no
     *  section; the current cue's section stays in sight. */
    void goStepsPastASectionRow();
    /** An arrow key or a click landing on a section row leaves the cue it
     *  goes on to highlighted, not only current. */
    void keysAndClicksOnASectionRowKeepTheCueSelected();
    /** An older show (no sections): no section rows, no section line. */
    void aListWithNoSectionsLooksAsBefore();
    /** With QLC_SHOT_DIR: a sectioned list at three sizes, light and dark. */
    void songSectionShots();

private:
    Doc* m_doc;
};

#endif
