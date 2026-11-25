/*
  Q Light Controller Plus
  goguardui_test.h

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

#ifndef GOGUARDUI_TEST_H
#define GOGUARDUI_TEST_H

#include <QObject>
#include <QList>

class QTemporaryDir;
class App;

/* The double-GO guard in the console (board Q233; the rules at
   ShowControl::run()): a GO from each of the console's real GO paths -- the
   ⌘↩ key, the Run tab's GO, the calling script's GO, the pad, the phone
   Run page, a VC Cue Runner -- after a GO from each, and every surface
   saying the hold in the same words: the Run tab's line, the footer's Next
   chip, the phone page's state and the pad's notice. */
class GoGuardUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    /** The console applies Settings' window (1.0 s by default). */
    void theConsoleTakesTheWindowFromSettings();
    /** Every real GO path after every one: held, the cue stays, and the
     *  four surfaces say the same words; gone from all four after. */
    void everySourceIsHeldAndEverySurfaceSaysIt();
    /** Back from the Run tab right after a GO is held; the phone's and the
     *  pad's emergency stop never are. */
    void backIsHeldTheStopNever();
    /** Settings ▸ General ▸ Show Controls ▸ Double-GO guard: a longer
     *  window, and Off. */
    void theSettingChangesTheWindow();
    /** Q328: a cue marked Fast -- the GO straight after it goes, from
     *  the console's real GO paths; the cue after is guarded again. */
    void aFastCueGoesOnAtOnce();
    /** Q328: the cue page's Fast tick: a main cue's (not a hit's), one
     *  undo step, the chaser step follows, "fast" in the row's flags. */
    void theFastTickIsOnTheCuePage();
    /** Pictures for the visual check (QLC_SHOT_DIR set; else skipped): the
     *  Run tab and the footer just after a second GO, and the Show Tree's
     *  cue page with Fast ticked, at 1024x640, 1440x900 and 1920x1080, and
     *  Settings ▸ General. */
    void pictures();

private:
    /** A production "Macbeth" with a list Act One of four cues. */
    void makeShow();

    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
    quint32 m_production = 0;
    quint32 m_list = 0;
    qint64 m_now = 100000;
};

#endif
