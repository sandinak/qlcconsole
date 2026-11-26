/*
  Q Light Controller Plus - Unit test
  editlive_test.h

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

#ifndef EDITLIVE_TEST_H
#define EDITLIVE_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QList>

class Doc;
class Chaser;
class EditLive;
class IOPluginStub;
class SimMidiDevice;

/* Edit Live (Q337-Q339, docs/features/edit-live.md): a simulated APC40's
   page controls trim the running cue of a tracking show in Operate, and
   Store writes only what was trimmed into the look that sets it. */
class EditLive_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void offNothingHappens();
    void onlyInOperate();
    void aTrimMovesOnlyThatAttribute();
    void storeWritesOnlyTheTouchedAttributeIntoTheTrackingSource();
    void aFaderPicksUpThenTrims();
    void dropPutsTheCueBack();
    void goDropsTheTrimsAndTheNoticeStoresToTheRightCue();
    void theEmergencyStopClearsIt();
    void designTurnsItOff();

private:
    bool show();
    void go();
    void tick(int n = 1);
    QByteArray frame();
    QList<int> spotPan() const;

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    SimMidiDevice *m_apc = nullptr;
    EditLive *m_live = nullptr;
    QTemporaryDir m_settings;
    QList<quint32> m_selection;
    Chaser *m_chaser = nullptr;
    quint32 m_spot = 0, m_par1 = 0, m_par2 = 0;
    quint32 m_pars = 0, m_spots = 0;
    quint32 m_c1 = 0, m_c2 = 0, m_c3 = 0;
    quint32 m_red = 0, m_blue = 0, m_full = 0, m_front = 0;
};

#endif
