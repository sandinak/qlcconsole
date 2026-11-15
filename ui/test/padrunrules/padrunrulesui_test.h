/*
  Q Light Controller Plus
  padrunrulesui_test.h

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

#ifndef PADRUNRULESUI_TEST_H
#define PADRUNRULESUI_TEST_H

#include <QObject>

class QTemporaryDir;
class IOPluginStub;
class App;

/* Rule 2 on the real surfaces: the Run tab's Pad: button and its frame,
   the footer's Pad chip and the pad's light (what the stub plugin was
   sent), compared at every step of state changes made in awkward orders. */
class PadRunRulesUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void everySurfaceAgreesAtEveryStep();
    /** Rule 6 on the real surfaces: a venue focus cue waiting, ARMED, LT +
     *  Menu -- Pad: FOCUS in its blue on the button, the frame, the chip
     *  and the pad's light; the emergency stop there turns them red; LT +
     *  Menu again: ARMED, and the Run tab says the list waits (Venue focus
     *  done: LT + A for the next cue) with no command sent. */
    void focusOnEverySurface();
    /** QLC_SHOT_DIR set: the Run tab and the footer at 1024x640, 1280x800
     *  and 1920x1080 with a venue focus started from the armed pad (LT +
     *  Menu), then finished the same way (the UX review's shots). */
    void venueFocusShots();

private:
    /** Two spots on the plot, a group, a target, one focus call; a
     *  Pre-show list (Focus, a venue focus cue, then House) the list is on
     *  (a probe stands in for it playing), the cue armed. Its list id;
     *  @p focus and @p house its cues. */
    quint32 makeFocusShow(quint32 *focus, quint32 *house);

    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
    IOPluginStub *m_stub = nullptr;
    quint32 m_running = 0;      // the cue the probe says the focus list is on
    quint32 m_focusList = 0, m_focusCue = 0, m_houseCue = 0;
};

#endif
