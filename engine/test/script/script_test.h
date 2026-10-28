/*
  Q Light Controller
  script_test.h

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

#ifndef SCRIPT_TEST_H
#define SCRIPT_TEST_H

#include <QObject>
#include <QTemporaryDir>

class Script_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void initial();

    /* What a script does (coverage campaign): it was run once, unchecked */
    void linesAreTokenizedAndBadOnesNamed();
    void aScriptSavesAndLoadsAsWritten();
    void functionsStartStopAndStopOnExit();
    void setFixtureWritesItsChannel();
    void waitsJumpsAndBlackout();
    void waitingForAFunction();
    void anUnreadableWaitDoesNotHang();
    void programsRunOnlyWhenAllowed();

private:
    class Doc *m_doc = nullptr;
    QTemporaryDir m_settings;
};

#endif
