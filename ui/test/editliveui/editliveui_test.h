/*
  Q Light Controller Plus - Unit test
  editliveui_test.h

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

#ifndef EDITLIVEUI_TEST_H
#define EDITLIVEUI_TEST_H

#include <QTemporaryDir>
#include <QObject>

class IOPluginStub;
class SimMidiDevice;
class Chaser;
class Doc;

/* Edit Live in the app (docs/features/edit-live.md): the footer chip and
   the Run tab's switch, the show's PIN, the popup's trims, Store and Drop,
   and Store to Cue N after a GO; shots of the chip and its popup. */
class EditLiveUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void thePinIsAsked();
    void theRunTabSwitchTurnsItOn();
    void theChipIsRedAndListsTheTrims();
    void afterGoTheChipOffersStoreToCue();
    /* Train 053o's cross-branch review: fails on 72ae430a3 */
    void aStudentShowNeverReachesIt();
    void theRunTabSwitchAtStudentTakesARealClick();
    void shots();

private:
    bool show();
    void go();
    void tick(int n = 1);
    void trimRed();

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    SimMidiDevice *m_apc = nullptr;
    Chaser *m_chaser = nullptr;
    QTemporaryDir m_settings;
    QList<quint32> m_pars;
    quint32 m_c1 = 0, m_c2 = 0;
};

#endif
