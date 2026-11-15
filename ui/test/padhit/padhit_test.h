/*
  Q Light Controller Plus
  padhit_test.h

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

#ifndef PADHIT_TEST_H
#define PADHIT_TEST_H

#include <QObject>
#include <QList>

class QTemporaryDir;
class IOPluginStub;
class App;

/* The hit named before it fires (0.4.6, rule 2): the Run tab's line, the
   footer's Pad chip and the phone Run page's state say the same hit, from
   PadRun's one source, before and after each pull of RT, as two hits take
   turns, after the cue moves on, disarmed and after the stop. The pad is
   the stub plugin's, the GO the real one. */
class PadHit_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    /** Every surface names the hit the next pull fires, at every step. */
    void everySurfaceNamesTheNextHit();
    /** The phone page's list is the Run tab's (rule 1). */
    void thePhonePlaysTheRunTabsList();
    /** A GO while a hit plays: the hit completes (output still the hit's,
     *  a previous cue's hit too, a pull on the new cue leaves it). */
    void aHitPlayingWhenAGoComesCompletes();
    /** A one-shot hit ends on its own time, not at the GO. */
    void aOneShotHitEndsOnItsOwnTimeNotAtTheGo();
    /** Pictures for the visual check (QLC_SHOT_DIR set; else skipped): the
     *  Run tab and the footer, armed on a cue with two hits, after a pull,
     *  and on a cue with one, at 1024, 1280 and 1920 wide. */
    void pictures();

private:
    /** The show: a list Opening / Ballad / Finale; Staff Slam and Thunder on
     *  Opening, Breathe on Ballad, none on Finale. */
    void makeShow();
    void tap(quint32 channel);
    void value(quint32 channel, unsigned char v);
    void chord(bool on);
    void go();
    int level(int channel);

    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
    IOPluginStub *m_stub = nullptr;
    quint32 m_list = 0;
    quint32 m_list2 = 0;
    QList<quint32> m_cues;
};

#endif
