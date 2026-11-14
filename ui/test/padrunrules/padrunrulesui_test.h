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

private:
    App *m_app = nullptr;
    QTemporaryDir *m_pluginDir = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif
